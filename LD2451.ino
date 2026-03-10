// https://forum.arduino.cc/t/ld2451-anyone-have-had-success-in-connecting-it/1330363
// https://enotarial.com/document-inquiry-form/NjAx
#define FIRSTTIME  false    // Define true if setting up this device for first time.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <EEPROM.h>
#include <WebServer.h>
#include "time.h"
#include <HardwareSerial.h>
#include <PicoMQTT.h>
#include <PicoWebsocket.h>
#include "motionDetector.h" 
#include <ESP32Ping.h> 

HardwareSerial radarSerial(2);

#define RADAR_RX_PIN 16 
#define RADAR_TX_PIN 17 

struct tm timeinfo;
#define MY_TZ "EST5EDT,M3.2.0,M11.1.0" //(New York) https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

char* room = "Livingroom";  // Needed for person locator.Each location must run probeReceiver sketch to implement person locator.
int rssiThreshold = -50;    // Adjust according to signal strength by trial & error.

IPAddress deviceIP(192, 168, 0, 2); // Fixed IP address assigned to family member's devices to be checked for their presence at home.
//IPAddress deviceIP = WiFi.localIP();
int device1IP = 2, device2IP = 3, device3IP = 4, device4IP = 5;
uint8_t device1ID[3] = {0xD0, 0xC0, 0x8A};   // First and last 2 bytes of Mac ID of Cell phone #1.
uint8_t device2ID[3] = {0x36, 0x33, 0x33};   // First and last 2 bytes of Mac ID of Cell phone #2.
uint8_t device3ID[3] = {0x36, 0x33, 0x33};   // First and last 2 bytes of Mac ID of Cell phone #3.
uint8_t device4ID[3] = {0x36, 0x33, 0x33};   // First and last 2 bytes of Mac ID of Cell phone #4.

const char* http_username = "admin";  // Web file editor interface Login.
const char* http_password = "admin";  // Web file editor interface password.

String dataFile = "/data.json";       // File to store sensor data.

const char* apSSID = "ESP";
const char* apPassword = ""; 
int apChannel = 7;
int hidden = 0;                 // If hidden is 1 probe request event handling does not work ?


 //==================User configuration not required below this line ================================================
/*
::WiFiServer server(80);
PicoWebsocket::Server<::WiFiServer> websocket_server(server);
//AsyncWebServer webserver(80); 
PicoMQTT::Server mqtt(websocket_server);
*/


WiFiServer tcp_server(1883);
WiFiServer websocketServer(81);
PicoWebsocket::Server<::WiFiServer> websocket_server(websocketServer);
PicoMQTT::Server mqtt(tcp_server, websocket_server);

WebServer server(80);

const char webpage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WebSocket Client</title>
    <style>
        h1 {
            color: green;
        }

        .container {
            margin: 10px;
        }
    </style>
</head>

<body>
    <h1>WebSocket Example</h1>
    <div class="container">
        <label>Send Message to Server:</label> <br><br>
        <input type="text" id="messageInput">
        <button onclick="sendMessage()">Send</button>
        <div id="output"></div>
    </div>

    <script>
        // Create a WebSocket instance
        // and connect to the server
        const socket = new WebSocket('ws://10.1.10.241:81');

        // Event listener for when 
        //the WebSocket connection is opened
        socket.onopen = function (event) {
            // Alert the user that they are 
            // connected to the WebSocket server
            alert('You are Connected to WebSocket Server');
        };

        // Event listener for when a message
        //  is received from the server
        socket.onmessage = function (event) {
            // Get the output div element
            const outputDiv = document
                .getElementById('output');
            // Append a paragraph with the
            //  received message to the output div
            outputDiv
                .innerHTML += `<p>Received <b>"${event.data}"</b> from server.</p>`;
        };

        // Event listener for when the 
        // WebSocket connection is closed
        socket.onclose = function (event) {
            // Log a message when disconnected
            //  from the WebSocket server
            console.log('Disconnected from WebSocket server');
        };

        // Function to send a message
        //  to the WebSocket server
        function sendMessage() {
            // Get the message input element
            const messageInput = document
                .getElementById('messageInput');
            // Get the value of
            // the message input
            const message = messageInput.value;
            // Send the message to 
            // the WebSocket server
            socket.send(message);
            // Clear the message input
            messageInput.value = '';
        }
</script>
</body>
</html>

)=====";

//#define MYFS SPIFFS
//#define FORMAT_SPIFFS_IF_FAILED true

const byte HEADER[4] = {0xF4, 0xF3, 0xF2, 0xF1};
const byte FOOTER[4] = {0xF8, 0xF7, 0xF6, 0xF5};

int RSSIlevel, motionLevel = -1; // initial value = -1, any values < 0 are errors, see motionDetector.h , ERROR LEVELS sections for details on how to intepret any errors.
String ssid = "HTM",password, graphData;
uint8_t receivedCommand[6],showConfig[20];
const char* ntpServer = "pool.ntp.org";
unsigned long time_now, epoch, lastDetected;      // Epoch time at which last motion level detected above trigger threshold.
unsigned long getTime() {time_t now;if (!getLocalTime(&timeinfo)) {Serial.println("Failed to obtain time");return(0);}Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");time(&now);return now;}


void handleNotFound() {String message = "File Not Found\n\n";message += "URI: ";message += server.uri();message += "\nMethod: ";message += (server.method() == HTTP_GET) ? "GET" : "POST";    message += "\nArguments: ";message += server.args();  message += "\n";
    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
}



void setup() {
    
  Serial.begin(115200);
  radarSerial.begin(115200, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  radarSerial.setTimeout(10);
  Serial.println("HLK-LD2451 Radar initiating...");

  WiFi.mode(WIFI_AP_STA);
  
  WiFi.softAP(apSSID, apPassword, apChannel, hidden);
  Serial.print("AP started with SSID: ");Serial.println(apSSID);
    
  WiFi.begin(ssid.c_str(), password.c_str());
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Wifi connection failed");
    Serial.print("Connect to Access Point '");Serial.print(apSSID);Serial.println("' and point your browser to 192.168.4.1 to set SSID and password");
    WiFi.disconnect(false);
   
    delay(1000);
    
    WiFi.begin(ssid.c_str(), password.c_str());
    
 }
    Serial.print("This device IP: ");Serial.println(WiFi.localIP());
    Serial.print("Connect at IP: ");Serial.print(WiFi.localIP()); Serial.print(" or 192.168.4.1"); Serial.print(" if Connected to Access Point - ");Serial.print(apSSID);
   
    EEPROM.begin(512);  
  /*  SPIFFS.begin();

#if FIRSTTIME       
    firstTimeSetup();    // Setup device numbers and wifi Channel for remote devices in EEPROM permanantly.
#endif
  
    EEPROM.readBytes(0, showConfig,20);for(int i=0;i<20;i++){Serial.printf("%d ", showConfig[i]);}
    Serial.println();

    startAsyncWebServer();
  */
    motionDetector_init();  // initializes the storage arrays in internal RAM
    motionDetector_config(64, 16, 3, 3, false); 
    Serial.setTimeout(1000);
    
    configTime(0, 0, ntpServer); setenv("TZ", MY_TZ, 1); tzset(); // Set environment variable with your time zone
      
    mqtt.subscribe("#", [](const char * payload) {
        if (payload && strlen(payload)) {
           Serial.printf("Received message in topic '#': %s\n", payload);
        }
    });
   
   server.on("/", []() {
        server.send(200, "text/html", webpage);
    });
    server.onNotFound(handleNotFound);

    server.begin();
    server.onNotFound(handleNotFound);

    websocket_server.begin();
    mqtt.begin(); 
   // WiFi.onEvent(probeRequest, ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED); Serial.print("Waiting for probe requests ... ");

}
/*
void startAsyncWebServer()
{
  
 // ws.onEvent(onEvent);
 // webserver.addHandler(&ws);
      
  webserver.on("/post", HTTP_POST, [](AsyncWebServerRequest * request){
  
  int params = request->params();
  
  for(int i=0;i<params;i++){
  AsyncWebParameter* p = request->getParam(i);
    
    String input0 =request->getParam(0)->value();receivedCommand[0] =(atoi(input0.c_str()));
    String input1 =request->getParam(1)->value();receivedCommand[1] =(atoi(input1.c_str()));  
    String input2 =request->getParam(2)->value();receivedCommand[2] =(atoi(input2.c_str()));
    String input3 =request->getParam(3)->value();receivedCommand[3] =(atoi(input3.c_str())); 
    ssid = request->getParam(4)->value().c_str();                  
    password =request->getParam(5)->value().c_str();

} 
  request -> send(200, "text/plain", "Command received by server successfully, please click browser's back button to get back to main page.");
  Serial.print("Command received from Browser: ");Serial.print(receivedCommand[0]);Serial.print(receivedCommand[1]);Serial.print(receivedCommand[2]);Serial.print(receivedCommand[3]);Serial.print(receivedCommand[4]);Serial.println(receivedCommand[5]);

  if (ssid.length() > 0 || password.length() > 0) 
  {  
    EEPROM.writeString(21,ssid);EEPROM.writeString(51, password);
    EEPROM.commit();Serial.println();Serial.print("Wifi Configuration saved to EEPROM: SSID="); Serial.print(ssid);Serial.print(" & Password="); Serial.println(password);Serial.println("Restarting Gateway now...");delay(1000);
    ESP.restart();
  }
    for (int i = 0; i < 4; i++) 
      {
       uint8_t motionSettings[4]; // Enable/diable motion sensor, Scan interval, Level threshold & minimum RSSI.
       motionSettings[i] = receivedCommand[i];
       EEPROM.writeBytes(0, motionSettings,4);
      }
      EEPROM.commit();
      EEPROM.readBytes(0, showConfig,10);for(int i=0;i<10;i++){Serial.printf("%d ", showConfig[i]);}Serial.println();
}); 

  webserver.serveStatic("/", MYFS, "/").setDefaultFile("index.html");
   
  webserver.addHandler(new SPIFFSEditor(MYFS, http_username,http_password));

  webserver.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
    if(!index)
      Serial.printf("UploadStart: %s\n", filename.c_str());
      Serial.printf("%s", (const char*)data);
      if(final)
      Serial.printf("UploadEnd: %s (%u)\n", filename.c_str(), index+len);
  });
  
   webserver.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
      Serial.printf("BodyStart: %u\n", total);
      Serial.printf("%s", (const char*)data);
      if(index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });
  
   webserver.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if(!index)
      Serial.printf("BodyStart: %u\n", total);
      Serial.printf("%s", (const char*)data);
      if(index + len == total)
      Serial.printf("BodyEnd: %u\n", total);
  });

    //Following line must be added before server.begin() to allow local lan request see : https://github.com/me-no-dev/ESPAsyncWebServer/issues/726
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    webserver.begin();
}
*/
void collectRadardata() {
    
    static byte buffer[128];
    static int index = 0;

    while (radarSerial.available()) {
        byte data = radarSerial.read();

        // 1. Adım: Başlık Tespiti (F4 F3 F2 F1)
        if (index < 4) {
            if (data == HEADER[index]) {
                buffer[index++] = data;
                if (index == 4) Serial.println("\nBaşlık Algılandı!");
            } else {
                index = 0;
            }
        } 
        // 2. Adım: Veri ve Footer Okuma
        else {
            buffer[index++] = data;

            // Veri Uzunluğu Okuma (6. ve 7. Byte)
            if (index == 6) {
                int dataLength = buffer[4] | (buffer[5] << 8);
                Serial.printf("Beklenen Veri Uzunluğu: %d\n", dataLength);
                if (dataLength < 0 || dataLength > 100) index = 0; // Geçersiz uzunluk
            }

            // Paket Tamamlama Kontrolü
            if (index >= 10) { 
                int dataLength = buffer[4] | (buffer[5] << 8);
                int totalLength = 4 + 2 + dataLength + 4;

                // Buffer Taşması Kontrolü
                if (totalLength > sizeof(buffer)) {
                    Serial.println("HATA: Buffer taşması!");
                    index = 0;
                    return;
                }

                // Kalan Veriyi Oku
                while (index < totalLength && radarSerial.available()) {
                    buffer[index++] = radarSerial.read();
                }

                // Footer Kontrolü (Son 4 Byte)
                bool footerOK = true;
                for (int i=0; i<4; i++) {
                    if (buffer[totalLength-4+i] != FOOTER[i]) {
                        footerOK = false;
                        Serial.printf("HATA: Footer[%d] = %02X (Beklenen: %02X)\n", i, buffer[totalLength-4+i], FOOTER[i]);
                        break;
                    }
                }

                if (!footerOK) {
                    Serial.println("HATA: Footer geçersiz!");
                    index = 0;
                    return;
                }

                // Ham Veriyi Yazdır
                Serial.print("Alınan Paket: ");
                for (int i=0; i<totalLength; i++) Serial.printf("%02X ", buffer[i]);
                Serial.println();
           
                // Veriyi İşle
                processData(buffer + 6, dataLength); // Veri bloğu 6. bytedan başlar
                index = 0;
            }
        }
    }
}

void processData(byte* data, int length) {
    if (length <= 0) return;

    byte alarmStatus = data[0];
    Serial.printf("Alarm status: %s\n", (alarmStatus == 0x01) ? "Target detected" : "No");

    byte targetCount = data[1];
    Serial.printf("Target count: %d\n", targetCount);

    if (targetCount > 0) {
        mqtt.publish("radarStatus", "Target detected");
        mqtt.subscribe("#", [](const char * payload) {
        if (payload && strlen(payload)) {
           Serial.printf("Received message in topic '#': %s\n", payload);
        }
    });
  }
          
    for (int i=0; i<targetCount; i++) {
        int offset = 2 + i*5; 
        if (offset + 4 >= length) {
            Serial.println("Error: data size is insufficient!");
            break;
        }

        int8_t angle = data[offset] - 0x80; // Örn: 0x8A → 10°
        uint8_t distance_m = data[offset + 1]; // Örn: 0x28 → 40m
        byte direction = data[offset + 2]; // 0x01: Yaklaşan, 0x00: Uzaklaşan
        uint8_t speed_kmh = data[offset + 3]; // Direkt km/h
        uint8_t snr = data[offset + 4]; // 0-255

        auto websocket = websocket_server.accept();
            if (!websocket) {return;}

            while (websocket.connected()) {
            yield();

            if (websocket.available()) {
            uint8_t buffer[19];
            for (int i=0; i<offset+4; i++) {
            buffer[i] = data[i];
            }
            websocket.write(buffer, offset+4);

            yield();
            }
        }
    }
}
/*
void presenceDetection() {

        if(millis() >= time_now + (EEPROM.readByte(1) * 10))
  {      // Implementation of non blocking delay function.
   Serial.print("Motion sensor scan interval: ");Serial.println(EEPROM.readByte(1) * 10);
   time_now += (EEPROM.readByte(1) * 10);
         
      if (EEPROM.readByte(0) == 1) // Only process following code if motion sensor Enabled.
       {
         Serial.print("Motion sensor Status: "); Serial.println("Enabled");
         Serial.print("Motion sensor minimum RSSI value set to: ");Serial.println(EEPROM.readByte(3) * -1); //motionDetector_set_minimum_RSSI = (EEPROM.readByte(5) * -1); // Minimum RSSI value to be considered reliable. Default value is 80 * -1 = -80. 
    
      //   notifyClients(String(motionLevel));
              
     //Serial.print("Motion sensor Threshold set to: ");Serial.println(EEPROM.readByte(4));
     if (motionLevel > EEPROM.readByte(2)) // If motion is detected.
      {
       lastDetected = getTime();
       // If motion detected, check any family member is at home.
       
       Serial.println("Checking if anybody at Home.... ");
        
       int pingTime, fatherPing, motherPing, sonPing, daughterPing;
         
         deviceIP[3] = device1IP;
         Serial.println("Pinging IP address 2... ");
       //  if(Ping.ping(deviceIP)) {Serial.println("Father is at Home");notifyClients(String(2020));} else { Serial.println("Father is Away");notifyClients(String(2000));}
         fatherPing = Ping.averageTime();Serial.print("Ping time in milliseconds: ");Serial.println(fatherPing);
         deviceIP[3] = device2IP;
         Serial.println("Pinging IP address 3... ");
       //  if(Ping.ping(deviceIP)) {Serial.println("Mother is at Home");notifyClients(String(3030));} else { Serial.println("Mother is Away");notifyClients(String(3000));}
         motherPing = Ping.averageTime();Serial.print("Ping time in milliseconds: ");Serial.println(motherPing);
         deviceIP[3] = device3IP;
         Serial.println("Pinging IP address 4... ");
         //if(Ping.ping(deviceIP)) {Serial.println("Son is at Home");notifyClients(String(4040));} else { Serial.println("Son is Away");notifyClients(String(4000));}
         sonPing = Ping.averageTime();Serial.print("Ping time in milliseconds: ");Serial.println(sonPing);
         deviceIP[3] = device4IP;
         Serial.println("Pinging IP address 5... ");
         //if(Ping.ping(deviceIP)) {Serial.println("Daughter is at Home");notifyClients(String(5050));} else { Serial.println("Daughter is Away");notifyClients(String(5000));}
         daughterPing = Ping.averageTime();Serial.print("Ping time in milliseconds: ");Serial.println(daughterPing);

         RSSIlevel = WiFi.RSSI();
         
         graphData = ",";graphData += lastDetected;graphData += ",";graphData += motionLevel;graphData += ",";graphData += RSSIlevel;graphData += ",";graphData += fatherPing;graphData += ",";graphData += motherPing;graphData += ",";graphData += sonPing;graphData += ",";graphData += daughterPing;graphData += "]";
     
         File f = SPIFFS.open(dataFile, "r+"); Serial.print("File size: "); Serial.println(f.size());  // See https://github.com/lorol/LITTLEFS/issues/33
         f.seek((f.size()-1), SeekSet);Serial.print("Position: "); Serial.println(f.position());
         f.print(graphData);Serial.println();Serial.print("Appended to file: "); Serial.println(graphData);Serial.print("File size: "); Serial.println(f.size());
         f.close(); 
      }
       //notifyClients(String(lastDetected));
      // ws.cleanupClients();
 
    } else { Serial.print("Motion sensor Status: "); Serial.println("Disabled");}
       motionLevel = 0;  // Reset motionLevel to 0 to resume motion tracking.
       motionLevel = motionDetector_esp();  // if the connection fails, the radar will automatically try to switch to different operating modes by using ESP32 specific calls. 
       Serial.print("Motion Level: ");
       Serial.println(motionLevel);
  }  
       if (WiFi.waitForConnectResult() != WL_CONNECTED) {ssid = EEPROM.readString(270); password = EEPROM.readString(301);Serial.println("Wifi connection failed");Serial.print("Connect to Access Point ");Serial.print(apSSID);Serial.println(" and point your browser to 192.168.4.1 to set SSID and password" );WiFi.disconnect(false);delay(1000);WiFi.begin(ssid.c_str(), password.c_str());}
}

/*
void probeRequest(WiFiEvent_t event, WiFiEventInfo_t info) 
{ 
  Serial.println();
  Serial.print("Probe Received :  ");for (int i = 0; i < 6; i++) {Serial.printf("%02X", info.wifi_ap_probereqrecved.mac[i]);if (i < 5)Serial.print(":");}Serial.println();
  Serial.print("Connect at IP: ");Serial.print(WiFi.localIP()); Serial.print(" or 192.168.4.1 with connection to ESP AP");Serial.println(" to monitor and control whole network");

  if (info.wifi_ap_probereqrecved.mac[0] == device1ID[0] && info.wifi_ap_probereqrecved.mac[4] == device1ID[1] && info.wifi_ap_probereqrecved.mac[5] == device1ID[2]) 
  { // write code to match MAC ID of cell phone to predefined variable and store presence/absense in new variable.
    
    Serial.println("################ Person 1 arrived ###################### ");
    
    //myClient.publish("Sensordata/Person1/", "Home");
    Serial.print("Signal Strength of device: ");
    Serial.println(info.wifi_ap_probereqrecved.rssi);
    //myClient.publish("Sensordata/Signal/", (String)info.wifi_ap_probereqrecved.rssi);
    if (info.wifi_ap_probereqrecved.rssi > rssiThreshold) // Adjust according to signal strength by trial & error.
     { // write code to match MAC ID of cell phone to predefined variable and store presence/absense in new variable.
  //     myClient.publish("Sensordata/Person1/in/", room);
     }
              
    }
 } // End of Proberequest function.

void firstTimeSetup() {
 
  EEPROM.writeByte(0, 0);          // Enable/disable motion sensor.
  EEPROM.writeByte(1, 100);        // Scan interval for motion sensor.
  EEPROM.writeByte(2, 100);        // Level threshold where motion is considered valid.
  EEPROM.writeByte(3, 80);         // Minimum RSSI level to be considered for reliable motion detection.
  EEPROM.commit();
}
*/  
void loop() {
    
       //collectRadardata();
      // presenceDetection();  
      
       mqtt.loop();
       server.handleClient();
       
       mqtt.subscribe("command", [](const char * payload) {
        if (payload && strlen(payload)) {
            Serial.printf("Received message in topic 'command': %s\n", payload);
        }
    });

            auto websocket = websocket_server.accept();
            if (!websocket) {return;}

            while (websocket.connected()) {
            yield();

            if (websocket.available()) {
            uint8_t buffer[128];
            const auto bytes_read = websocket.read(buffer, 128);
            websocket.write(buffer, bytes_read);

            yield();
        }
    }
}
