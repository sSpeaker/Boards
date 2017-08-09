#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <CapacitiveSensor.h>

// Wi-Fi settings
const char* ssid = "sSpeaker";
const char* password = "adAD123321#";
const char* hostname1 = "dimmer";

// MQTT Settings
const char* mqtt_server = "bedroompi.local";
const int   mqtt_port = 1883;
const char* mqtt_brightness_set_topic = "home/bedroom/1/light/top/1/brightness/set";
const char* mqtt_brightness_get_topic = "home/bedroom/1/light/top/1/brightness/get";
long lastMsg = 0;
char msg[50];

int AC_LOAD = 4;    // Output to Opto Triac pin
int ZC = 5; 
int RED_PIN = 14;
int GREEN_PIN = 13;


int level = 110; //Store dimmer level. 0 - ON, 128 - OFF

String webString="";
ESP8266WebServer server(80);

WiFiClient espClient;
PubSubClient mqtt(espClient);

CapacitiveSensor   touchButton = CapacitiveSensor(12,16); 

void zero_crosss_int() {
  delayMicroseconds(75*level);  
  digitalWrite(AC_LOAD, HIGH);
  delayMicroseconds(10); 
  digitalWrite(AC_LOAD, LOW);
}

void setLevel(int desiredLevel){
  level = desiredLevel;
  
  if (mqtt.connected()) {
    mqtt.publish(mqtt_brightness_get_topic, String(level/10).c_str(), true);
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
    int desiredLevel = s_payload.toInt();
    setLevel(desiredLevel);
  } else {
    Serial.print(" [unknown message]");
  }
    
  Serial.println("");
}

void handle_level() {
  if (server.hasArg("set")) {
    setLevel(server.arg(0).toInt());
  }
  webString = "Dimmer:";
  webString += level; 
  server.send(200, "text/plain", webString);
}

void handle_root() {
  server.send(200, "text/plain", "Use /level?set=");
}

void setup() {
  Serial.begin(9600);

  pinMode(AC_LOAD, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
 
  attachInterrupt(ZC, zero_crosss_int, RISING);

  // Connect to WiFi network
  WiFi.hostname(hostname1);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
   }
  Serial.print("\nConnected: ");
  Serial.println(WiFi.localIP());
  
  // Http based server
  server.on("/", handle_root);
  server.on("/level", handle_level);
  server.begin();

  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(callback); 
}

void loop()  {
// MQTT

  //Serial.println(touchButton.capacitiveSensor(30));

  if ( level >= 125 ) {
    digitalWrite(RED_PIN, LOW);  
    digitalWrite(GREEN_PIN, HIGH);  
  } else {
    digitalWrite(RED_PIN, HIGH);  
    digitalWrite(GREEN_PIN, LOW);  
  }
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

  server.handleClient();
  delay(2);
}
