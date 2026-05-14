const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Dashboard</title>
    <style>
        body { 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
            background-color: #121212; 
            color: #e0e0e0; 
            text-align: center; 
            margin: 0; 
            padding: 20px; 
        }
        .container { 
            max-width: 600px; 
            margin: 0 auto; 
            background: #1e1e1e; 
            padding: 20px; 
            border-radius: 10px; 
            box-shadow: 0 4px 8px rgba(0,0,0,0.5); 
        }
        h1 { 
            font-size: 1.5rem; 
            border-bottom: 1px solid #333; 
            padding-bottom: 15px; 
            margin-bottom: 20px; 
        }
        .grid { 
            display: grid; 
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); 
            gap: 15px; 
            text-align: left; 
            margin-bottom: 25px; 
        }
        .card { 
            background: #2c2c2c; 
            padding: 15px; 
            border-radius: 8px; 
            border-left: 4px solid #007acc; 
        }
        .label { 
            font-size: 0.9rem; 
            color: #aaa; 
            text-transform: uppercase; 
        }
        .value { 
            font-size: 1.5rem; 
            font-weight: bold; 
            margin-top: 5px; 
            color: #fff; 
        }
        .buttons { 
            display: flex; 
            gap: 15px; 
            justify-content: center; 
        }
        button { 
            background: #007acc; 
            color: white; 
            border: none; 
            padding: 12px 24px; 
            font-size: 1rem; 
            border-radius: 5px; 
            cursor: pointer; 
            transition: background 0.3s;
            font-weight: bold;
        }
        button:hover { background: #005f9e; }
        button.off { background: #d32f2f; }
        button.off:hover { background: #9a0007; }
    </style>
</head>
<body>

    <div class="container">
        <h1>Environmental Telemetry</h1>
        
        <div class="grid">
            <div class="card"><div class="label">System Time</div><div class="value" id="TimeValue">--:--:--</div></div>
            <div class="card"><div class="label">Relay State</div><div class="value" id="LEDState">--</div></div>
            <div class="card"><div class="label">Temperature</div><div class="value"><span id="ADCValue">--</span> &deg;C</div></div>
            <div class="card"><div class="label">Humidity</div><div class="value"><span id="HUMValue">--</span> %</div></div>
            <div class="card"><div class="label">Air Pressure</div><div class="value"><span id="PaValue">--</span> hPa</div></div>
            <div class="card"><div class="label">Altitude</div><div class="value"><span id="ApValue">--</span> m</div></div>
        </div>

        <div class="buttons">
            <button class="off" onclick="sendData(0)">RELAY OFF</button>
            <button onclick="sendData(1)">RELAY ON</button>
        </div>
    </div>

    <script>
        // 1. Reusable fetch function (Replaces 6 redundant XMLHttpRequest blocks)
        async function fetchAndUpdate(endpoint, elementId) {
            try {
                const response = await fetch('/' + endpoint);
                if (response.ok) {
                    document.getElementById(elementId).innerText = await response.text();
                }
            } catch (error) {
                console.error('Fetch error for ' + endpoint, error);
            }
        }

        // 2. Group all updates together
        function updateAllSensors() {
            fetchAndUpdate('readTime', 'TimeValue');
            fetchAndUpdate('readLedState', 'LEDState');
            fetchAndUpdate('readADC', 'ADCValue');
            fetchAndUpdate('readHUM', 'HUMValue');
            fetchAndUpdate('readPa', 'PaValue');
            fetchAndUpdate('readAp', 'ApValue');
        }

        // 3. Modernized Relay Control
        async function sendData(state) {
            try {
                const response = await fetch('/setLED?LEDstate=' + state);
                if (response.ok) {
                    document.getElementById('LEDState').innerText = await response.text();
                }
            } catch (error) {
                console.error('Relay toggle error', error);
            }
        }

        // Run immediately on load, then loop every second
        updateAllSensors();
        setInterval(updateAllSensors, 1000);
    </script>

</body>
</html>
)=====";
