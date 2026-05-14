/**************************************************************************
 * WiFi Internet clock (NTP) & Weather Station with ESP32
 * Sensors: AHT10/20 & BMP280 | Display: ST7735 TFT
 * Includes Task Watchdog Timer (WDT) for stability
 *************************************************************************/

#include <WiFi.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <NTPClient.h>        // Include NTPClient library
#include <TimeLib.h>          // Include Arduino time library
#include <Adafruit_GFX.h>     // Include Adafruit graphics library
#include <Adafruit_ST7735.h>  // Include Adafruit ST7735 TFT library
#include <WiFiManager.h> 
#include <WebServer.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <esp_task_wdt.h>     // Include ESP32 Task Watchdog library
#include "index.h"            // HTML webpage contents

// Watchdog Timeout (in seconds)
#define WDT_TIMEOUT 15 

// ST7735 TFT module connections for ESP32
#define TFT_RST   12  
#define TFT_CS    23  
#define TFT_DC    5   
#define TFT_MOSI  13  
#define TFT_SCLK  14  

Adafruit_AHTX0 aht;
Adafruit_BMP280 bme; 
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

WiFiUDP ntpUDP;
// 'pool.ntp.org' with +8 hour offset (28800 seconds) and 60000ms update interval
NTPClient timeClient(ntpUDP, "pool.ntp.org", 28800, 60000);
unsigned long unix_epoch;

sensors_event_t humidity, temp;

const char dow_matrix[7][10] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
const byte x_pos[7] = {29, 29, 23, 11, 17, 29, 17};
static byte previous_dow = 255; // Force update on first run

String ledState = "OFF";
const int LED = 27;

const char* http_username = "admin";
const char* http_password = "admin";

WebServer server(80); 

// Timing variables for non-blocking loop
unsigned long previousMillis = 0;
const long interval = 1000; // Update sensors and display every 1000ms (1 second)

//===============================================================
// Web Server Handlers
//===============================================================
void handleRoot() {
  if (!server.authenticate(http_username, http_password))
    return server.requestAuthentication();
  server.send(200, "text/html", MAIN_page); 
}

void handleADC() {
  server.send(200, "text/plain", String(temp.temperature, 1)); 
}

void handleHUM() {
  server.send(200, "text/plain", String(humidity.relative_humidity, 0)); 
}

void handleAirPressure() {
  server.send(200, "text/plain", String(bme.readPressure() / 100.0F, 2)); 
}

void handleAP() {
  server.send(200, "text/plain", String(bme.readAltitude(1019.66), 1)); 
}

void handleTime() {
  char timeStr[10];
  snprintf(timeStr, sizeof(timeStr), "%02u:%02u:%02u", hour(unix_epoch), minute(unix_epoch), second(unix_epoch));
  server.send(200, "text/plain", timeStr); 
}

void handleLED() {
  String t_state = server.arg("LEDstate"); 
  if (t_state == "1") {
    digitalWrite(LED, LOW); // LED ON (Active Low)
    ledState = "ON "; 
  } else {
    digitalWrite(LED, HIGH); // LED OFF
    ledState = "OFF";  
  }
  server.send(200, "text/plain", ledState); 
}

void handleLedState() {
  server.send(200, "text/plain", ledState); 
}

//===============================================================
// Setup
//===============================================================
void setup(void) {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH); // Default OFF

  // Initialize TFT
  tft.initR(INITR_GREENTAB);     
  tft.setRotation(2);
  tft.setTextWrap(false);        // Prevent text from wrapping to the next line
  tft.fillScreen(ST7735_BLACK);  
  tft.drawFastHLine(0, 34, tft.width(), ST7735_BLUE);    
  tft.setTextColor(ST7735_WHITE, ST7735_BLACK);     
  tft.setTextSize(1);                 
  tft.setCursor(1, 0);               
  tft.print("Wi-Fi Clock");

  tft.setCursor(0, 54);              
  tft.print("Connecting..");

  // WiFi Manager
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setConnectTimeout(60);
  wifiManager.autoConnect("AutoConnectAP");

  while (WiFi.status() != WL_CONNECTED) {
    ESP.restart();
  }

  Serial.println("Connected");
  tft.fillRect(0, 54, tft.width(), 8, ST7735_BLACK); // Clear "Connecting" text
  
  tft.setTextColor(ST7735_ORANGE, ST7735_BLACK);     
  tft.setCursor(2, 15);              
  tft.print("IP: ");
  tft.print(WiFi.localIP().toString());

  tft.drawFastHLine(0, 108, tft.width(), ST7735_BLUE);  

  timeClient.begin();

  // Initialize Sensors
  if (!aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
  } else {
    Serial.println("AHT10/20 found");
  }

  if (!bme.begin(0x77)) { 
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
  } else {
    Serial.println("BMP280 found");
  }

  // Web Server Routing
  server.on("/", handleRoot);      
  server.on("/readTime", handleTime);
  server.on("/setLED", handleLED);
  server.on("/readADC", handleADC);
  server.on("/readHUM", handleHUM);
  server.on("/readPa", handleAirPressure);
  server.on("/readAp", handleAP);
  server.on("/readLedState", handleLedState);
  
  server.begin();                  
  Serial.println("HTTP server started");

  // Initialize Watchdog Timer (placed at the end of setup to avoid reboot loops during WiFi connection)
  esp_task_wdt_init(WDT_TIMEOUT, true); // Set timeout, true means trigger a system panic (reboot)
  esp_task_wdt_add(NULL);               // Add the current thread (the main loop) to the watchdog
}

//===============================================================
// Display Update Routine
//===============================================================
void RTC_display() {
  // Print Day of the Week
  tft.setTextSize(2);
  if (previous_dow != weekday(unix_epoch)) {
    previous_dow = weekday(unix_epoch);
    tft.fillRect(11, 45, 108, 14, ST7735_BLACK);     
    tft.setCursor(x_pos[previous_dow - 1], 45);
    
    if ((previous_dow == 1) || (previous_dow == 7)) { // Sunday or Saturday
      tft.setTextColor(ST7735_RED, ST7735_BLACK);     
    } else {
      tft.setTextColor(ST7735_CYAN, ST7735_BLACK);     
    }
    tft.print(dow_matrix[previous_dow - 1]);
  }

  // Print Date
  tft.setCursor(4, 65);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);     
  tft.printf("%04u-%02u-%02u", year(unix_epoch), month(unix_epoch), day(unix_epoch));
  
  // Print Time
  tft.setCursor(16, 85);
  tft.setTextColor(ST7735_GREEN, ST7735_BLACK);     
  tft.printf("%02u:%02u:%02u", hour(unix_epoch), minute(unix_epoch), second(unix_epoch));

  // Print Temperature
  tft.setCursor(0, 116);
  tft.setTextColor(ST7735_MAGENTA, ST7735_BLACK);     
  tft.printf("%4.1f", temp.temperature); 

  tft.setTextSize(1);
  tft.setCursor(48, 116);
  tft.print("o");

  tft.setTextSize(2);
  tft.setCursor(54, 116);
  tft.print("C");

  // Print Humidity - Shifted left to X=80 to prevent line wrapping
  tft.setCursor(80, 116);
  tft.printf("%3.0f%%", humidity.relative_humidity); 

  // Print Pressure & Altitude
  tft.setTextSize(1);
  tft.setCursor(0, 142);
  tft.printf("%6.2fhPa", bme.readPressure()/100.0F); 

  tft.setCursor(60, 142);
  tft.printf("%4.0fm", bme.readAltitude(1019.66)); 

  // Print LED State
  tft.setCursor(95, 142);
  tft.setTextColor(ST7735_RED, ST7735_BLACK);     
  tft.print(ledState);

  // Print WiFi Signal Strength
  tft.setCursor(85, 0);
  tft.setTextColor(ST7735_YELLOW, ST7735_BLACK);     
  tft.printf("%4ddBm", WiFi.RSSI());
}

//===============================================================
// Main Loop
//===============================================================
void loop() {
  // Feed the Watchdog so it doesn't trigger a reboot
  esp_task_wdt_reset();

  server.handleClient();          
  timeClient.update();            

  unsigned long currentMillis = millis();
  
  // Non-blocking timer: Run sensor reads and display updates every 1 second
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    unix_epoch = timeClient.getEpochTime();   
    aht.getEvent(&humidity, &temp);

    // Update Display
    RTC_display();
  }
}
