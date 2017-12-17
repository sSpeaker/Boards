#include <ESP8266WiFi.h>

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include "DHT.h"
#include <PubSubClient.h>
#include "OneButton.h"

#define REDPIN 14
#define BUTTON 4
#define DHTPIN 5
#define DHTTYPE DHT22

//Dimmer settings
#define DIMMSPEED 2
#define DIMMSTEP 1

// Wi-Fi settings
const char* ssid     = "132312312";
const char* password = "123312";
const char* hostname1 = "bathroom";

// MQTT Settings
const char* mqtt_server = "bedroompi.local";
const int   mqtt_port = 1883;
const char* mqtt_brightness_set_topic = "home/bathroom/1/light/mirror/1/brightness/set";
const char* mqtt_brightness_get_topic = "home/bathroom/1/light/mirror/1/brightness/get";
const char* mqtt_humidity_topic       = "home/bathroom/1/humidity/1";
const char* mqtt_temperature_topic    = "home/bathroom/1/temperature/1";
const char* mqtt_heatindex_topic      = "home/bathroom/1/heatindex/1";
long lastMsg = 0;
char msg[50];


int level = 0; //Store dimmer level
int level_before_off = level;
boolean direction = true; //Direction of dimming. true = UP

float temperature = 0;
float humidity = 0;
float heatindex = 0;
int  sensorReadInterval = 60000;

unsigned long previousMillis = 0;

String webString="";

ESP8266WebServer server(80);

WiFiClient espClient;
PubSubClient mqtt(espClient);

OneButton button(BUTTON, true);

DHT dht(DHTPIN, DHTTYPE);

void setLevel(int desiredLevel){
  // Serial.print("setLevel: desiredLevel = ");
  // Serial.println(desiredLevel);
  
  if (level < desiredLevel ){
    while (level < desiredLevel){
      level += DIMMSTEP;
      analogWrite(REDPIN, level);
      delay(DIMMSPEED);
    }
  } else {
    while (level > desiredLevel){
      level -= DIMMSTEP;
      analogWrite(REDPIN, level);
      delay(DIMMSPEED);
    }
  }

  if (mqtt.connected()) {
    int level_rounded = level/10.23;
    mqtt.publish(mqtt_brightness_get_topic, String(level_rounded).c_str(), true);
    Serial.print("MQTT out: ");
    Serial.print(mqtt_brightness_get_topic);
    Serial.print(" ");
    Serial.println(level);
  }

  Serial.print("setLevel: Done. level = ");
  Serial.println(level);
}

void callback(char* topic, byte* payload, unsigned int length) {
  char c_payload[length];
  memcpy(c_payload, payload, length);
  c_payload[length] = '\0';

  String s_topic = String(topic);
  String s_payload = String(c_payload);

  Serial.print("MQTT in: ");
  Serial.print(s_topic);
  Serial.print(" = ");
  Serial.print(s_payload);

  if (s_topic == mqtt_brightness_set_topic) {
    if ( s_payload == "ON" ) {
      s_payload = "100";
    } else if ( s_payload == "OFF" ) {
      s_payload = "0";
    }
    int desiredLevel = s_payload.toInt()*10.23;
    if (desiredLevel > PWMRANGE) {
      desiredLevel = PWMRANGE;
    } else if (desiredLevel < 0 ) {
      desiredLevel = 0;
    }
    setLevel(desiredLevel);
  } else {
    Serial.print(" [unknown message]");
  }
    
  Serial.println("");
}

void getDht(){
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  

  if (mqtt.connected()) {
    mqtt.publish(mqtt_humidity_topic, String(humidity).c_str(), true);
    Serial.print("MQTT out: ");
    Serial.print(mqtt_humidity_topic);
    Serial.print(" ");
    Serial.println(humidity);
    mqtt.publish(mqtt_temperature_topic, String(temperature).c_str(), true);
    Serial.print("MQTT out: ");
    Serial.print(mqtt_temperature_topic);
    Serial.print(" ");
    Serial.println(temperature);
  }
  
  Serial.print("GetDht: temperature = ");
  Serial.print(temperature);
  Serial.print("; humidity = ");
  Serial.print(humidity);
  Serial.println(";");
}

void handle_debug () {
  uint32_t realSize = ESP.getFlashChipRealSize();
  uint32_t ideSize = ESP.getFlashChipSize();
  unsigned long uptime = millis()/1000;
  int level_rounded = level/10.23;
  
  if (uptime < 60) {
    webString = "Uptime: ";
    webString += uptime;
    webString += " seconds\n";
  } else if ( uptime >= 60 & uptime < 3600 ) {
    int minutes = uptime / 60;
    int seconds = uptime % 60;
    
    webString = "Uptime: ";
    webString += minutes;
    webString += " minutes ";
    webString += seconds;
    webString += " seconds\n";
    
  } else if ( uptime >= 3600 & uptime < 86400) {
    int hours = uptime / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = (uptime % 3600) % 60;
    
    webString = "Uptime: ";
    webString += hours;
    webString += " hours ";
    webString += minutes;
    webString += " minutes ";
    webString += seconds;
    webString += " seconds\n";
    
  } else if (uptime >= 86400) {
    int days = uptime / 86400;
    int hours = (uptime % 86400) / 3600;
    int minutes = ((uptime % 86400) % 3600) / 60;
    int seconds = ((uptime % 86400) % 3600) % 60;

    webString = "Uptime: ";
    webString += days;
    webString += " days ";
    webString += hours;
    webString += " hours ";
    webString += minutes;
    webString += " minutes ";
    webString += seconds;
    webString += " seconds\n";
  }

//webString +=  printf("\nFlash real id:   %08X\n", ESP.getFlashChipId());
  webString += "\nFlash real size: ";
  webString += realSize;
  webString += "\nFlash ide  size: ";
  webString += ideSize;
  
  webString += "\n\nDimmer level: ";
  webString += level_rounded;
  webString += " %\n\nHumidity: ";
  webString +=  humidity;
  webString += " % \nTemperature: ";
  webString += temperature;
  webString += " °C";
  
  server.send(200, "text/plain", webString);
  delay(100);
}

void handle_sensors () {
  webString = "hostname:";
  webString += hostname1;
  webString += ";dhtt1:";
  webString += temperature;
  webString += ";dhth1:";
  webString += humidity;
  webString += ";";
  server.send(200, "text/plain", webString);
  delay(100);
}

void handle_level() {
  if (server.hasArg("set")) {
    int desiredLevel = server.arg(0).toInt()*10.23;
    if (desiredLevel > PWMRANGE) {
      desiredLevel = PWMRANGE;
    } else if (desiredLevel < 0 ) {
      desiredLevel = 0;
    }
    setLevel(desiredLevel);
  }
  int level_rounded = level/10.23;
  webString = "Dimmer:";
  webString += level_rounded; 
  server.send(200, "text/plain", webString);
  delay(100);
}

void handle_root() {
  server.send(200, "text/plain", "Use /sensors /level?set=");
}

void click() {
  Serial.println("Click: Begin ");
  
  if (level > 0) {
    level_before_off = level;
    setLevel(0);
    direction = true;
  } else {
    if (level_before_off > 0) {
      setLevel(level_before_off);
    } else {
      setLevel(PWMRANGE);
    }
    direction = false;
  }
  Serial.print("Click: level = ");
  Serial.print(level);
  Serial.print(" direction = ");
  Serial.println(direction);
}

void doubleclick() {
  setLevel(PWMRANGE);
  direction = false;
  Serial.print("DoubleClick: level = ");
  Serial.println(level);
}

void longPress() {
 if ( direction == true ) {
   if ( level < PWMRANGE ) {
      level += DIMMSTEP;
      analogWrite(REDPIN, level);
      delay(DIMMSPEED);
   } 
 } else {
   if ( level > 0 ) {
      level -= DIMMSTEP;
      analogWrite(REDPIN, level);
      delay(DIMMSPEED);
   }
 }
 
 Serial.print("LongPress: level = ");
 Serial.println(level);
}

void longPressStop() {
  if ( direction ) {
    direction = false;
  } else {
    direction = true;
  }

  if (mqtt.connected()) {
    int level_rounded = level/10.23;
    mqtt.publish(mqtt_brightness_get_topic, String(level_rounded).c_str(), true);
    Serial.print("MQTT out: ");
    Serial.print(mqtt_brightness_get_topic);
    Serial.print(" ");
    Serial.println(level);
  }

  Serial.print("LongPressStop: Done. direction = ");
  Serial.println(direction);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Version 0.2");
 
  // Connect to WiFi network
  WiFi.hostname(hostname1);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());  
  }

  // OTA Update
  ArduinoOTA.setHostname(hostname1);
  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  
  dht.begin();

 // Http based server
  server.on("/", handle_root);
  server.on("/sensors", handle_sensors);
  server.on("/level", handle_level);
  server.on("/debug", handle_debug);
  server.begin();

  pinMode(REDPIN, OUTPUT);
  button.attachClick(click);
  button.attachDoubleClick(doubleclick);
  button.attachLongPressStop(longPressStop);
  button.attachDuringLongPress(longPress);

  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback);

  analogWriteFreq(200);

// DHT
  getDht();
  previousMillis = millis();
}

void loop() {
  // MQTT
  if (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(hostname1)) {
      Serial.println("connected");
      mqtt.subscribe(mqtt_brightness_set_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      // TODO
      delay(5000);
    }
  }
  mqtt.loop();

// DHT
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= sensorReadInterval) {
    getDht();
    previousMillis = currentMillis;
  }

  button.tick();
  server.handleClient();
  ArduinoOTA.handle();
}
