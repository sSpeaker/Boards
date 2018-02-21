#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>         //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>         //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>        //MQTT
#include "OneButton.h"           //https://github.com/mathertel/OneButton


#define SPEED_PIN 14              //Use PWM to controll motor speed
#define OPEN_PIN 5
#define CLOSE_PIN 4
#define POSITION_PIN A0           // Analog input pin that the potentiometer is attached to
#define BUTTON_PIN 0
#define RELAY_PIN 12
#define LED_PIN 13


#define POSITION_OPEN 795         //Adjust after real setup
#define POSITION_CLOSED  465      //the same

//define your default values here, if there are different values in config.json, they are overwritten.
#define mqtt_server       "openhab.local"
#define mqtt_port         "1883"
#define mqtt_user         ""
#define mqtt_pass         ""
#define status_topic      "deathstar/status"
#define cmd_topic         "deathstar/cmd"
#define light_status_topic      "deathstar/light_status"
#define light_cmd_topic         "deathstar/light_cmd"


WiFiClient espClient;
PubSubClient mqtt(espClient);

long lastMsg = 0;
char msg[50];
int value = 0;

typedef enum {
  LIGHT_ON,
  LIGHT_OFF
} 
LightStatus;

LightStatus currentLightStatus = LIGHT_OFF;
LightStatus previousLightStatus = LIGHT_OFF;

typedef enum {
  ACTION_CLOSE,
  ACTION_OPEN,
  ACTION_OFF
} 
MyActions;

MyActions previousAction = ACTION_OFF;
MyActions currentAction = ACTION_OFF; // no action when starting
boolean stopped = true;

OneButton button(BUTTON_PIN, true);

ESP8266WebServer server ( 80 );

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char c_payload[length];
  memcpy(c_payload, payload, length);
  c_payload[length] = '\0';

  String s_topic = String(topic);
  String s_payload = String(c_payload);

  Serial.print("MQTT in: ");
  Serial.print(s_topic);
  Serial.print(" = ");
  Serial.print(s_payload);
  Serial.println();
  
  if (s_topic == cmd_topic) {
    if ( s_payload == "OPEN") {
      Serial.print("MQTT in: Command OPEN");
      previousAction = currentAction;
      currentAction = ACTION_OPEN;
    } else if ( s_payload == "CLOSE" ) {
      Serial.print("MQTT in: Command CLOSE");
      previousAction = currentAction;
      currentAction = ACTION_CLOSE;
    } else if ( s_payload == "STOP" ) {
      Serial.print("MQTT in: Command STOP");
      previousAction = currentAction;
      currentAction = ACTION_OFF;
    } else {
      Serial.print("MQTT in: unknown command");
    }
  } else if (s_topic == light_cmd_topic) {
    if ( s_payload == "ON") {
      Serial.print("MQTT in: Command ON");
      currentLightStatus = LIGHT_ON;
    } else if ( s_payload == "OFF" ) {
      Serial.print("MQTT in: Command OFF");
      currentLightStatus = LIGHT_OFF;
    } else {
      Serial.print("MQTT in: unknown command");
    }
  }  else {

    Serial.print("MQTT in: unknown topic");
  }
  Serial.println();
}


int getPosition() {
  int i = 0;
  int position_temp = 0;
  while(i < 100) {
    position_temp = position_temp + analogRead(POSITION_PIN);
    i++;
  }
  position_temp = position_temp / 100;
  Serial.print("Raw position = ");
  Serial.println(position_temp);
  
  position_temp = map(constrain(position_temp,POSITION_CLOSED,POSITION_OPEN), POSITION_CLOSED, POSITION_OPEN, 0, 100);
  Serial.print("Position % = ");
  Serial.println(position_temp);
  return position_temp;
}

void buttonClick() {
  Serial.println("buttonClick: Click detected");
  if ( currentAction == ACTION_OFF ) {
    if ( previousAction == ACTION_OPEN ) {
      currentAction = ACTION_CLOSE;
      Serial.println("buttonClick: Set action to CLOSE");
    } else {
      currentAction = ACTION_OPEN;
      Serial.println("buttonClick: Set action to OPEN");
    }
  } else {
    previousAction = currentAction; //Record previous action before rewriting it
    currentAction = ACTION_OFF;
    Serial.println("buttonClick: Set action to OFF");
  }
}

void doubleClick() {
  if ( currentLightStatus == LIGHT_ON ) {
    currentLightStatus = LIGHT_OFF;
    Serial.println("doubleClick: Set light to OFF");
  } else {
    currentLightStatus = LIGHT_ON;
    Serial.println("doubleClick: Set light to ON");
  }
}

void slowStart() { //Slow star will keep motor live longer and use lower start current!!!
  Serial.println("slowStart: Hello");
  int speed = 300;
  //TODO this is blocking loop
  while ( speed < 1023 ) {
    analogWrite(SPEED_PIN, speed);
    delay(2);
    speed = speed + 10;
  }
}

void lightControl() {
  if ( currentLightStatus != previousLightStatus ) {
    bool light_status;
    previousLightStatus = currentLightStatus;
    
    if ( currentLightStatus == LIGHT_ON ) {
      light_status = true;
    } else {
      light_status = false;
    }
    digitalWrite(RELAY_PIN, light_status ? HIGH : LOW);
    Serial.print("lightControl: Change status to ");
    Serial.println(light_status ? "ON" : "OFF");
    if (mqtt.connected()) {
      mqtt.publish(light_status_topic, String(light_status ? "ON" : "OFF").c_str(), true);
      Serial.print("MQTT out: ");
      Serial.print(light_status_topic);
      Serial.print(" ");
      Serial.println(light_status ? "ON" : "OFF");
    }
  }
}

void motorControl(){
  if ( currentAction == ACTION_OPEN ) {
    if ( getPosition() == 100 ) { // If lamp is OPEN -> save state for the button control and stop motor.
       previousAction = currentAction;
       currentAction = ACTION_OFF;
    } else {
      if ( stopped ) { // Run this code only once in every state change.
        Serial.println("motorControl: Let's OPEN");
        stopped = false;
        digitalWrite(OPEN_PIN, HIGH);
        slowStart();
      }
    }
  } else if ( currentAction == ACTION_CLOSE) {
    if ( getPosition() == 0 ) {
      previousAction = currentAction;
      currentAction = ACTION_OFF;
    } else {
      if ( stopped ) {
        Serial.println("motorControl: Let's CLOSE");
        stopped = false;
        digitalWrite(CLOSE_PIN, HIGH);
        slowStart();
      }
    }
  } else { //currentAction == ACTION_OFF
    if ( ! stopped ) {
      Serial.println("motorControl: Let's STOP");
      digitalWrite(OPEN_PIN, LOW);
      digitalWrite(CLOSE_PIN, LOW);
      analogWrite(SPEED_PIN, 0);
      stopped = true;
      if (mqtt.connected()) {
        mqtt.publish(status_topic, String(getPosition()).c_str(), true);
        Serial.print("MQTT out: ");
        Serial.print(status_topic);
        Serial.print(" ");
        Serial.println(getPosition());
      }
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(SPEED_PIN, OUTPUT);
  pinMode(OPEN_PIN, OUTPUT);
  pinMode(CLOSE_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  
  analogWrite(SPEED_PIN, 0);
  digitalWrite(OPEN_PIN, LOW);
  digitalWrite(CLOSE_PIN, LOW);
  Serial.println("Waiting one second for reset button");
  delay(1000);
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Formating FS");
    SPIFFS.format();
  }
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_pass, json["mqtt_pass"]);
          strcpy(status_topic, json["status_topic"]);
          strcpy(cmd_topic, json["cmd_topic"]);
          strcpy(light_status_topic, json["light_status_topic"]);
          strcpy(light_cmd_topic, json["light_cmd_topic"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  WiFiManagerParameter custom_mqtt_server("server", "Mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "Mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "Mqtt user", mqtt_user, 20);
  WiFiManagerParameter custom_mqtt_pass("pass", "Mqtt pass", mqtt_pass, 20);
  WiFiManagerParameter custom_status_topic("status", "Status topic", status_topic, 50);
  WiFiManagerParameter custom_cmd_topic("cmd", "Cmd topic", cmd_topic, 50);
  WiFiManagerParameter custom_light_status_topic("light_status", "Light status topic", light_status_topic, 50);
  WiFiManagerParameter custom_light_cmd_topic("light_cmd", "Light cmd topic", light_cmd_topic, 50);

  WiFiManager wifiManager;
  
  if (digitalRead(BUTTON_PIN) == LOW) {
    //Reset Wifi settings for testing  
    Serial.println("Reseting saved settings");
    wifiManager.resetSettings();
  }
  
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  wifiManager.addParameter(&custom_status_topic);
  wifiManager.addParameter(&custom_cmd_topic);
  wifiManager.addParameter(&custom_light_status_topic);
  wifiManager.addParameter(&custom_light_cmd_topic);

  //We still wants to control lamp if there is no wifi
  //Config portal will be killed in 2 minutes
  wifiManager.setTimeout(120);
  
  wifiManager.autoConnect("Death_Star","123321aa1");
  Serial.println("WiFI connected.");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());
  strcpy(status_topic, custom_status_topic.getValue());
  strcpy(cmd_topic, custom_cmd_topic.getValue());
  strcpy(light_status_topic, custom_light_status_topic.getValue());
  strcpy(light_cmd_topic, custom_light_cmd_topic.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;
    json["status_topic"] = status_topic;
    json["cmd_topic"] = cmd_topic;
    json["light_status_topic"] = light_status_topic;
    json["light_cmd_topic"] = light_cmd_topic;
    
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    Serial.println();
    json.printTo(configFile);
    configFile.close();
    //end save
  }
  
  //TODO port from settings is ignorred!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  const uint16_t mqtt_port_x = 1883; 
  mqtt.setServer(mqtt_server, mqtt_port_x);
  mqtt.setCallback(mqttCallback);

  button.attachClick(buttonClick);
  button.attachDoubleClick(doubleClick);

  server.on ( "/", []() {
    server.send ( 200, "text/plain", "DeathStar Lamp.\nUse /status, /open, /close, /stop" );
    Serial.println("webServer: /open");
  } );

  server.on ( "/open", []() {
    server.send ( 200, "text/plain", "OK" );
    currentAction = ACTION_OPEN;
    Serial.println("webServer: /open");
  } );
  server.on ( "/close", []() {
    server.send ( 200, "text/plain", "OK" );
    currentAction = ACTION_CLOSE;
    Serial.println("webServer: /close");
  } );
  server.on ( "/stop", []() {
    server.send ( 200, "text/plain", "OK" );
    currentAction = ACTION_OFF;
    Serial.println("webServer: /stop");
  } );
  server.on ( "/status", []() {
    server.send ( 200, "text/plain", String(getPosition()));
    Serial.println("webServer: /status");
  } );
  server.on ( "/light_on", []() {
    server.send ( 200, "text/plain", "OK");
    Serial.println("webServer: /light_on");
  } );
  server.on ( "/light_off", []() {
    server.send ( 200, "text/plain", "OK");
    Serial.println("webServer: /light_off");
  } );
  server.begin();
}


unsigned long mqttMillis = 0;

void loop() {
  // MQTT
  if (!mqtt.connected()) {
    if ( ( millis() - mqttMillis ) >= 30000 ) {
      Serial.print("Attempting MQTT connection...");
      if (mqtt.connect("DeathStar", mqtt_user, mqtt_pass)) {
        Serial.println("connected");
        mqtt.subscribe(cmd_topic);
        mqtt.subscribe(light_cmd_topic);
        mqtt.publish(status_topic, String(getPosition()).c_str(), true); //Report start possition
        mqtt.publish(light_status_topic, String("OFF").c_str(), true); //Report start light status
      } else {
        Serial.print("failed, rc=");
        Serial.print(mqtt.state());
        Serial.println(" try again in 30 seconds");
        mqttMillis = millis();
      }
    }
  }
  
  mqtt.loop();
  button.tick(); // Monitor button state
  server.handleClient();
  lightControl();
  motorControl(); // Control lamp state
  delay(10);
  //getPosition(); //Debug
}
