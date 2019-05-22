/*  Changelog Version 9
 *   
 *  Changed Host to new IP address 
 *  Changed uSeconds factor from 32bit to Unsigned long long 64bit value (Will this work for breaking the one hour deep sleep limit barrier? Or will it break the universe)
 *  Sleep time 4 hours
 *  
 *  
 *  
 *
 *
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include "SSD1306Wire.h"
#include <NTPClient.h>
#include <esp32fota.h>
#include <esp32-hal-bt.c>


#define NTP_OFFSET  3600 // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS "0.pool.ntp.org"

uint64_t uS_TO_S_FACTOR = 1000000;  /* Conversion factor for micro seconds to seconds */
uint64_t TIME_TO_SLEEP = 14400;      /* Time ESP32 will go to sleep (in seconds) */


RTC_DATA_ATTR int bootCount = 0;


//Firmware version
const int fwVersion = 9;
//FOTA OBJECT DECLARATION AND VERSION DEFINITION
//esp32FOTA esp32FOTA("esp32-fota-http", 9);

// Network configuration
const char* ssid = "xxxx";
const char* password = "xxxxx";
const char* mqtt_server = "xxxxx";

// MQTT Protocol variables
long lastMsg = 0;
char msg[50];
int value = 0;

// Irrigation variables
long int lastepochTime = 0;
long int timeDiff;
boolean zaliti2 = false;
boolean zaliti1 = false;

//ESP32 ID
int id = 1;

//ESP32 lokacija
const char* location = "kuhinja";

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

//MQTT Message string
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
  String messageTemp;
 
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
  // Changes the output state according to the message
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
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      
      // Subscribe
      client.subscribe("esp32/ledonoff");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
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

  Serial.println("WiFi retry attempt: " + String(wifiRetryAttempt));
  Serial.println("MQTT retry attempt: " + String(mqttRetryAttempt));
  
  //WiFi Client setup
  WiFi.mode( WIFI_STA );
  
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
  Serial.print("WiFi Status: ");
  Serial.println(WiFi.status());
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

  /*esp32FOTA.checkURL = "http://192.168.0.26/fota/fota.json";
  bool updatedNeeded = esp32FOTA.execHTTPcheck();
  if (updatedNeeded)
  {
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

*/
  
  //
  //
  //        MAIN CODE START
  //
  // 
  if (!client.connected())
  {
    reconnect();
    delay(1000);
  }
  client.loop();
  delay(1000);
  

  display.clear();
  display.display();
 
  ArduinoOTA.handle();
 
  timeClient.update();
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
    zaliti1 = true;
    zaliti2 = true;   
  }


  MQTTMessage = String(bootCount) + ';' + formattedTime + ';' + String(location) + ';' + String(id) + ';' + String(fwVersion) + ';' + String(mappedValue1) + ';' + String(mappedValue2); 
  Serial.println(MQTTMessage);
 
  /*display.setColor(BLACK);
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
  */

  delay(2000);
  
  client.publish("Kuhinja/Senzori",String(MQTTMessage).c_str(), true);

  delay(2000);
  
  //display.drawString(0, 40, "Sensor2 : " + String(sensorValue2));
  //client.publish("Kuhinja/Senzor2",String(mappedValue2).c_str(), true);
  
  /*display.drawString(0, 50, "Zaliti1: " + String(zaliti1));
  display.drawString(64, 50, "Zaliti2: " + String(zaliti2));
  display.display();
  delay(1000);
  */

  if( mappedValue1 < 70 && zaliti1 == true ){
    zaliti1 = false;
    digitalWrite(relayPin1, LOW);
    delay(1000);
    digitalWrite(relayPin1, HIGH);   
  }

   if( mappedValue2 < 70 && zaliti2 == true ){
    zaliti2 = false;
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
 /* display.setColor(BLACK);
  display.fillRect(0, 10, 128, 54);
  display.display();
  display.setColor(WHITE);
 */ 

  WiFi.disconnect(true);
  delay(100);


  Serial.print("WiFi Status: ");
  Serial.println(WiFi.status());
  
  btStop();
  WiFi.mode( WIFI_OFF );
  
  Serial.flush();
  delay(100);
  esp_deep_sleep_start();

}



void loop()
{
 
}
