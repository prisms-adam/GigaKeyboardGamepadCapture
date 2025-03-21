// v0.2 - Added hostname service from ArduinoMDNS found in the Library Manager
// v0.3 - Added realtime sensor data example read from A0, displays free SRAM, and removed unnecessary commented code
// v0.4 - Added a terminal log

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoMDNS.h>

char ssid[] = "PRIS_Student";
char pass[] = "wearethebest1";
char hostname[] = "example"; //Your hostname in lowercase

WiFiUDP udp;
MDNS mdns(udp);

WiFiServer server(80);
int status = WL_IDLE_STATUS; //Added to allow the Giga to connect to PRIS_Student

String terminalText = "Example text";

const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
  <html>
    <head>
        <title>WiFi Bridge Controller - Giga</title>
        <script>
        let lastKeys = new Set();
        let lastGamepadState = {};
    
        function updateTerminal() {
          fetch('/terminal')
          .then(response => response.text())
          .then(data => {
              document.getElementById("terminalData").innerHTML += data;
              document.getElementById("terminalData").scrollTop = document.getElementById("terminalData").scrollHeight; //Auto-scroll to the bottom of the textbox
          });
        }
        setInterval(updateTerminal, 1000); // Update every second

        function clearTerminal() {
            document.getElementById("terminalData").value = ""; // Clears the text area
        }
        
        function updateData() {
          fetch('/data')
          .then(response => response.text())
          .then(data => {
              document.getElementById("sensorValue").innerText = data;
          });
        }
        setInterval(updateData, 1000); // Update every second

        function sendData(endpoint, data) {
          let query = Object.keys(data).map(key => key + "=" + encodeURIComponent(data[key])).join("&");
          let url = endpoint + "?" + query;
    
          // Use sendBeacon for better performance
          let blob = new Blob([query], {type: 'application/x-www-form-urlencoded'});
          if (!navigator.sendBeacon(url, blob)) {
            fetch(url, { method: "GET", cache: "no-store" }).catch(err => console.error("Fetch failed:", err));
          }
        }
    
        document.addEventListener("keydown", function(event) {
          sendData("/keypress", {char: event.key});
        });
    
        document.addEventListener("keyup", function(event) {
          lastKeys.delete(event.key);
        });
    
        window.addEventListener("gamepadconnected", function(event) {
          console.log("Gamepad connected:", event.gamepad.id);
          setInterval(checkGamepad, 200);
        });
    
        function checkGamepad() {
          let gamepads = navigator.getGamepads();
          if (!gamepads) return;
    
          for (let i = 0; i < gamepads.length; i++) {
            let gp = gamepads[i];
            if (!gp) continue;
    
            let newState = {};
    
            gp.buttons.forEach((button, index) => {
              if (button.pressed && !lastGamepadState[index]) {
                sendData("/gamepad", {button: index});
              }
              newState[index] = button.pressed;
            });
    
            gp.axes.forEach((value, index) => {
              let roundedValue = value.toFixed(2);
              if (lastGamepadState[`axis${index}`] !== roundedValue) {
                sendData("/gamepad", {axis: index, value: roundedValue});
              }
              newState[`axis${index}`] = roundedValue;
            });
    
            lastGamepadState = newState;
          }
        }
      </script>
    </head>
  <style>
  .content {
    max-width: 500px;
    margin: auto;
    text-align: center;
    font-family: courier;
  }
  </style>
  <body>
    <div class="content">
      <h2>PRISMS Engineering</h2>
      <h3>WiFi Bridge Controller - Giga v0.3</h3>
      <p>Keep this window active and start typing or using a gamepad.</p>
      <p>Check the Serial Monitor for received input.</p>
      <p>Example sensor data: <span id="sensorValue">0</span></p>
      <textarea id="terminalData" rows=25 cols=60 autofocus input disabled></textarea>
      <button onclick="clearTextbox()">Clear</button>
    </div>
  </body>
  </html>
)rawliteral";

void sendResponse(WiFiClient& client, const String& contentType, const String& content) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: " + contentType);
    client.println("Content-Length: " + String(content.length()));
    client.println("Connection: close");
    client.println();
    client.print(content);
}

void handleRequest(WiFiClient& client, const String& request) {
  if (request.startsWith("GET / ")) {
    sendResponse(client, "text/html", htmlPage);
    return;
  }

  // Keyboard Input Handling
  int keyPos = request.indexOf("char=");
  if (keyPos >= 0) {
    String key = request.substring(keyPos + 5, request.indexOf(" ", keyPos));
    Serial.print("Keyboard Key: ");
    Serial.println(key);
  }

  // Gamepad Button Input Handling
  int buttonPos = request.indexOf("button=");
  if (buttonPos >= 0) {
    String button = request.substring(buttonPos + 7, request.indexOf(" ", buttonPos));
    Serial.print("Gamepad Button: ");
    Serial.println(button);
  }

  // Gamepad Axis Input Handling
  int axisPos = request.indexOf("axis=");
  int valuePos = request.indexOf("value=");
  if (axisPos >= 0 && valuePos >= 0) {
    String axis = request.substring(axisPos + 5, request.indexOf(" ", axisPos));
    String value = request.substring(valuePos + 6, request.indexOf(" ", valuePos));
    Serial.print("Gamepad Axis ");
    Serial.print(axis);
    Serial.print(": ");
    Serial.println(value);
  }

  if(request.indexOf("GET /data") >= 0){
    int sensorValue = analogRead(A0);
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.println(String(sensorValue));
  }
  else if(request.indexOf("GET /terminal") >= 0){
    unsigned long now = millis();
    unsigned long seconds = now/1000;
    unsigned long minutes = seconds/60;
    unsigned long hours = minutes/60;
    seconds %= 60;
    minutes %= 60;
    hours %= 24;
    
    String terminalTextTimestamp = "[" + String(hours) + ":" + String(minutes) + ":" + String(seconds) + "] " + terminalText;
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.println(terminalTextTimestamp);
  }
  else{
    client.println("HTTP/1.1 204 No Content");
    client.println();
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 3 seconds for connection:
    delay(3000);
  }
  Serial.println("Connected to wifi");
  printWifiStatus();

  mdns.begin(WiFi.localIP(), hostname);
  mdns.addServiceRecord("WiFi Bridge Controller - Giga._http",80,MDNSServiceTCP);

  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  mdns.run();
  WiFiClient client = server.available();
  if(client){
    String request = client.readStringUntil('\r');
    handleRequest(client, request);
    client.stop();
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print your board's hostname:
  Serial.print("Hostname: ");
  Serial.println(hostname);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
