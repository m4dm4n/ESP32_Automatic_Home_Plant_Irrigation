/*  Changelog Version 11
 *   
 *  Changed Host to new IP address 
 *  Changed uSeconds factor from 32bit to Unsigned long long 64bit value (Will this work for breaking the one hour deep sleep limit barrier? Or will it break the universe)
 *  Sleep time 4 hours
 *  MIN Soil humidity 70%
 *  Soil humidity percentage controlled by Node-Red MQTT component -- NOT WORKING WITH DEEP SLEEP FUNCTION
 *  Automatic FOTA update - You need fota.json file (included example), a compiled binary file, and a webserver to put those files on (source wants it to be in a http://x.x.x.x/fota folder
 *  REMEMBER TO DEFINE SERVER IP ADDRESS OR NAME ON LINE 323 (approx)
 *
 *
 *  NTP time sync
 *  Some gameplay with OLED, not really usefull with 4 hour sleep time, and few seconds awake time (ain't nobody got time for that)
 *
 *  Cleaning some code, removing useless comments, adding some usefull (I hope) ones
 *
 *
 *  TODO : Put WiFi, Server settings, and other sensitive data in sepparate document
 *         Do something so esp32 will be more unique (mac addres + something?)
 *
 *
 *
 *
 */

// Defining libraries
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include "SSD1306Wire.h"
#include <NTPClient.h>
#include <esp32fota.h>


// Defining NTP variables
#define NTP_OFFSET  3600 // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS "0.pool.ntp.org"


// Defining total sleep time microseconds*seconds
uint64_t uS_TO_S_FACTOR = 1000000;  /* Conversion factor for micro seconds to seconds */
uint64_t TIME_TO_SLEEP = 14400;      /* Time ESP32 will go to sleep (in seconds) */

// Define bootcount number after esp32 comes back from sleep
RTC_DATA_ATTR int bootCount = 0;


//Firmware version
const int fwVersion = 11;

//FOTA OBJECT DECLARATION AND VERSION DEFINITION
esp32FOTA esp32FOTA("esp32-fota-http", 11);


// WiFi and server settings
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PSK";
const char* mqtt_server = "YOUR_IP_OR_SERVER_NAME";

// Defining MQTT message buffer settings
long lastMsg = 0;
char msg[50];
int value = 0;
String messageTemp;

// Defining variables for irrigation decisions 
long int lastepochTime = 0;
long int timeDiff;
boolean irrigate1 = false;
boolean irrigate2 = false;

//ESP32 ID
int id = 1;

//ESP32 lokacija
const char* location = "kitchen";

//Base and New humidity values from MQTT broker
int OLDhumidity1 = 70;
int OLDhumidity2 = 70;
int NEWhumidity1, NEWhumidity2;

//MQTT Topic subscriptions
String MQTT_SUB_humidity1 = "esp32/humidity1";
String MQTT_SUB_humidity2 = "esp32/humidity2";
String MQTT_PUB_KitchenPublish = "Kitchen/Senzori";

//Sensor pins
const int sensorPin1 = 36;
const int sensorPin2 = 39;

//Sensor RAW and MAPPED values
int sensorValue1,mappedValue1,sensorValue2, mappedValue2;

// LED Pin
const int ledPin = 15;

//Relay Pin
const int relayPin1 = 12;
const int relayPin2 = 13;

//MQTT Message string, group data
String MQTTMessage;

// Connection retry variables 
int mqttRetryAttempt = 0;
int wifiRetryAttempt = 0;


SSD1306Wire  display(0x3c, 5, 4);
WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);


//Functions for MQTT
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  
 
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Checking MQTT topics and its messages

  if (String(topic) == MQTT_SUB_humidity1) {
    NEWhumidity1 = messageTemp.toInt();
    if( NEWhumidity1 > 0 && NEWhumidity1 < 100){
      if( NEWhumidity1 != OLDhumidity1){
       OLDhumidity1 = NEWhumidity1;
       Serial.print("New humidity1 is now: ");
       Serial.println(String(OLDhumidity1));
       }
      }
     }
       else {
       Serial.println("Humidity1 hasn't changed");  
       }
      
    
    

   if (String(topic) == MQTT_SUB_humidity2) {
    NEWhumidity2 = messageTemp.toInt();
    if( NEWhumidity2 > 0 && NEWhumidity2 < 100){
      if( NEWhumidity2 != OLDhumidity2){
       OLDhumidity2 = NEWhumidity2;
       Serial.print("New humidity2 is now: ");
       Serial.println(String(OLDhumidity2));
       }
      }
     }
       else {
       Serial.println("Humidity2 hasn't changed");  
       }
  
  
  
  
  // This is only a test subscription, it is commented for future reference
  /*  
  if (String(topic) == "esp32/ledonoff") {
    Serial.print("Changing output to ");
    if(messageTemp == "true"){
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "false"){
      Serial.println("off");
      digitalWrite(ledPin, LOW);
     }
     }
    */

}





void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      display.drawString(0, 0, "MQTT Connected");
      display.display();
      delay(1000);
      // Subscribe
      //client.subscribe("esp32/ledonoff");
      client.subscribe(MQTT_SUB_humidity1.c_str());
      client.subscribe(MQTT_SUB_humidity2.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 3 seconds");
      display.drawString(0, 0, "MQTT Error");
      display.display();
      // Wait 3 seconds before retrying
      delay(3000);
      mqttRetryAttempt++;
      if (mqttRetryAttempt > 5) {
      Serial.println("MQTT retry limit reached, going back to sleep, better luck next time!");
      esp_deep_sleep_start();
      // OR YOU CAN JUST TRY TO RESTART THE DAMN THING
      /*
      Serial.println("Restarting!");
      ESP.restart();
      */
      }
    }
  }
}




void setup()
{

  //OLED setup
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  
  //Serial Setup
  Serial.begin(115200);
  delay(1000);

  Serial.println("Firmware version is: " + String(fwVersion));
  
  //WiFi Client setup
  WiFi.begin(ssid,password);
  while (WiFi.status() != WL_CONNECTED)
  {
  delay(500);
  Serial.print(".");
  wifiRetryAttempt++;
    if (wifiRetryAttempt > 5) {
    Serial.println("WiFi retry limit reached, going back to sleep, better luck next time!");
    esp_deep_sleep_start();
    // OR YOU CAN JUST TRY TO RESTART THE DAMN THING
    /*
    Serial.println("Restarting!");
    ESP.restart();
    */
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  String ipaddress = WiFi.localIP().toString();
/*
  display.drawString(0, 0, "IP address: " + ipaddress);
  display.setColor(BLACK);
  display.fillRect(0, 10, 128, 54);
  display.display();
*/
  //NTP Client setup
  timeClient.begin();

  //MQTT Client setup
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  //Pins Setup
  pinMode(ledPin, OUTPUT);
  pinMode(relayPin1, OUTPUT);
  digitalWrite(relayPin1, HIGH);
  pinMode(relayPin2, OUTPUT);
  digitalWrite(relayPin2, HIGH);

  //Arduino OTA Setup
   ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
   ArduinoOTA.begin();


   ++bootCount;
   esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  //
  //
  //  Check FIRMWARE AND UPDATE 
  //
  //

  esp32FOTA.checkURL = "http://YOUR_SERVER_IP_ADDRES_OR NAME/fota/fota.json";
  bool updatedNeeded = esp32FOTA.execHTTPcheck();
  if (updatedNeeded)
  {
    Serial.println("Update Needed!");
    Serial.println("Begin update");
    display.setColor(BLACK);
    display.fillRect(0, 10, 128, 54);
    display.display();
    display.setColor(WHITE);
    display.drawString(0, 10, "FIRMWARE UPDATE NEEDED");
    display.drawString(20, 400, "Updating firmware...");
    display.display();
    delay(1000);
    esp32FOTA.execOTA();
  }
  else
  {
  Serial.println("Firmware version OK");  
  display.setColor(BLACK);
  display.fillRect(0, 10, 128, 54);
  display.display();
  display.setColor(WHITE);
  display.drawString(0, 10, "FIRMWARE VERSION OK");
  display.display();
  delay(2000);
  display.setColor(BLACK);
  display.fillRect(0, 10, 128, 54);
  display.display();
  display.setColor(WHITE);
  }


  
  //
  //
  //        MAIN CODE START
  //
  // 
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  display.clear();
  display.display();
 
  ArduinoOTA.handle();
 
  timeClient.update();

  Serial.print("Humidity1 limit is: ");
  Serial.println(String(NEWhumidity1));
  
  Serial.print("Humidity2 limit is: ");
  Serial.println(String(NEWhumidity2));
  
  String formattedTime = timeClient.getFormattedTime();
  long int epochTime = timeClient.getEpochTime();
  
  sensorValue1 = 0;
  sensorValue2 = 0;
  
  for (int m=0; m < 16; m++) 
  {  
  sensorValue1 = sensorValue1 + analogRead(sensorPin1);
  sensorValue2 = sensorValue2 + analogRead(sensorPin2);
  delay(50);  
  }
  
  sensorValue1 = sensorValue1/16;
  sensorValue2 = sensorValue2/16;
  
  mappedValue1 = map(sensorValue1, 1400, 3350, 100, 0);
  mappedValue2 = map(sensorValue2, 1400, 3350, 100, 0);
  
  timeDiff = epochTime - lastepochTime;
   
  if ( timeDiff > 14400){
    lastepochTime = epochTime;
    irrigate1 = true;
    irrigate2 = true;   
  }


  MQTTMessage = String(location) + ';' + String(id) + ';' + String(fwVersion) + ';' + String(mappedValue1) + ';' + String(mappedValue2); 
  Serial.println(MQTTMessage);
 
  display.setColor(BLACK);
  display.fillRect(0, 10, 128, 54);
  display.display();
  display.setColor(WHITE);
  display.drawString(0, 10, "Time: " + String(formattedTime));
  //display.drawString(0, 20, "Linux Time : " + String(epochTime));
  display.drawString(90, 10, "ver " + String(fwVersion));
  display.drawString(0, 20, "Boot Count: " + String(bootCount));
  display.drawString(0, 30, "Sensor1 : " + String(mappedValue1) + " %");
  display.drawString(0, 40, "Sensor2 : " + String(mappedValue2) + " %");
  //display.drawString(0, 30, "Sensor1 : " + String(sensorValue1));
  client.publish(MQTT_PUB_KitchenPublish.c_str(), String(MQTTMessage).c_str(), true);
  //display.drawString(0, 40, "Sensor2 : " + String(sensorValue2));
  //client.publish("Kitchen/Senzor2",String(mappedValue2).c_str(), true);
  display.drawString(0, 50, "Irrigate1: " + String(irrigate1));
  display.drawString(64, 50, "Irrigate2: " + String(irrigate2));
  display.display();
  delay(1000);

  if( mappedValue1 < 70 && irrigate1 == true ){
    irrigate1 = false;
    digitalWrite(relayPin1, LOW);
    delay(1000);
    digitalWrite(relayPin1, HIGH);   
  }

   if( mappedValue2 < 70 && irrigate2 == true ){
    irrigate2 = false;
    digitalWrite(relayPin2, LOW);
    delay(1000);
    digitalWrite(relayPin2, HIGH);
  }

  
  //
  //
  //          MAIN CODE END
  //
  //

  //  TURN OFF DISPLAY
  display.setColor(BLACK);
  display.fillRect(0, 10, 128, 54);
  display.display();
  display.setColor(WHITE);
  
  esp_deep_sleep_start();

}



void loop()
{
 
}
