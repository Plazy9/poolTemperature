#include "config.h"
#include <PubSubClient.h>

#ifdef ESP32
  #include <WiFi.h>
  #include <ESPAsyncWebSrv.h>
#else
  #include <Arduino.h>
  #include <ESP8266WiFi.h>
  #include <Hash.h>
  #include <ESPAsyncTCP.h>
  #include <ESPAsyncWebSrv.h>
#endif

#include <OneWire.h>
#include <DallasTemperature.h>
#include <FS.h> 

// Data wire is connected to GPIO 4
#define ONE_WIRE_BUS 4

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// Variables to store temperature values
String temperatureF = "";
String temperatureC = "";

int numberOfDevices;
DeviceAddress tempDeviceAddress;

// Replace with your network credentials
const char* ssid = wifiSSID;;
const char* password = wifiPassword;

const char* mqtt_server = mqttServer;
const char* mqttServerUser = mqttUser;
const char* mqttServerPWD = mqttPassword;

WiFiClient espClient;
PubSubClient client(espClient);

#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

//led
//const int ledPin =  LED_BUILTIN;// the number of the LED pin
const int ledPin =  14;// the number of the LED pin

// Variables will change:
int ledState = LOW;  

String readDSTemperatureC() {
  float tempC;
  String TempDataContent;
  String tempTopic;
  
  if (ledState == LOW) {
      ledState = HIGH;
  }else {
      ledState = LOW;
  }

  // set the LED with the ledState of the variable:
  digitalWrite(ledPin, ledState);
  
  sensors.requestTemperatures(); // Send the command to get temperatures

  // Loop through each device, print out temperature data
  for(int i=0;i<numberOfDevices; i++){
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i)){
      // Output the device ID
      
      //Serial.print("Temperature for device: ");
      //Serial.println(i,DEC);
      // Print the data
      float tempC = sensors.getTempC(tempDeviceAddress);
      
      //TempDataContent = TempDataContent + " Temperature for device: " + i + " Temp C: " + tempC;
      TempDataContent = TempDataContent + String(tempC) + ";"; 
      snprintf (msg, MSG_BUFFER_SIZE, "%.2f", tempC);
      
      tempTopic = "pl_nodemcu_temp/pl_outTopic/"+String(i);
      client.publish(tempTopic.c_str(), msg);
    }
  }
  //Serial.println(TempDataContent);
  return String(TempDataContent);
  //TempDataContent = "";
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++){
    if (deviceAddress[i] < 16) Serial.print("0");
      Serial.print(deviceAddress[i], HEX);
  }
}
//wifi connect
void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
//mqtt
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(),mqttServerUser, mqttServerPWD)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("pl_nodemcu_temp/pl_outTopic", "hello world");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup(){
  //led 
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  
  // Serial port for debugging purposes
  Serial.begin(115200);
  Serial.println();
  SPIFFS.begin();

  setup_wifi();
  client.setServer(mqtt_server, 1883);

  digitalWrite(ledPin, HIGH);
  
  // Start up the DS18B20 library
  sensors.begin();

// Grab a count of devices on the wire
  numberOfDevices = sensors.getDeviceCount();
  
  // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(numberOfDevices, DEC);
  Serial.println(" devices.");

  // Loop through each device, print out address
  for(int i=0;i<numberOfDevices; i++){
    // Search the wire for address
    if(sensors.getAddress(tempDeviceAddress, i)){
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddress);
      Serial.println();
    } else {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
    }
  }

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false);
  });

 server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", temperatureC.c_str());
  });

  server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/test.txt", String(), false);
  });
  
  /*
  server.on("/temperaturec", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", temperatureC.c_str());
  });
  server.on("/temperaturef", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", temperatureF.c_str());
  });
  */
  // Start server
  server.begin();
}
 
void loop(){
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  temperatureC = readDSTemperatureC(); 
  delay(1000);
}
