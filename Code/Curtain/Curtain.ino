#include <Servo.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

const char* ssid     = "sSpeaker";
const char* password = "adAD123321#";
const char* host = "curtain-bedroom";

const int encoderPinA = 14;
const int encoderPinB = 12;
unsigned char encoder_A;
unsigned char encoder_B;
unsigned char encoder_A_prev=0;
unsigned long currentTime;
unsigned long loopTime;

const int buttonUpPin = 0;
const int buttonDownPin = 16;

const int servoPin = 13;
Servo servo;
int servoStatus = 90; // 90 - STOP; 180 - DOWN; 0 - UP;


int servoPossition = 0;      // variable to store the servo position
int servoMaxPossition = 200; // Top curtain possition

int servoDesirePossition = servoPossition;
int servoMoveUp = 60;
int servoMoveDown = 120 ;

long lastDebounceTime = 0;   // This timer reseted every bounce
long debounceDelay = 100;     // Will detect turn if stable for this time

String webString="";

ESP8266WebServer server(80);

// Function to handle root page of webserver
void handle_root() {
  server.send(200, "text/plain", "Use /up /down /stop /possition /possition?set=");
  delay(100);
}

void handle_down() {
  server.send(200, "text/plain", "Going down");
  delay(100);
  servoDesirePossition = 0;
}

void handle_up() {
  server.send(200, "text/plain", "Going up");
  delay(100);
  servoDesirePossition = servoMaxPossition;
}

void handle_stop() {
  server.send(200, "text/plain", "Stoped");
  delay(100);
  // TODO. We need to stop at 1
  servoDesirePossition = servoPossition;
}

void handle_possition() {
  if (server.hasArg("set")) {
    servoDesirePossition = server.arg(0).toInt();
    server.send(200, "text/plain", "On my way");
  } else {
    webString = servoPossition;
    server.send(200, "text/plain", webString);
  }
}

void changePosstition() {
  if ( servoPossition < servoDesirePossition ) {
    servoStatus = servoMoveUp;
  } else if (servoPossition > servoDesirePossition) {
    servoStatus = servoMoveDown;
  } else {
    servoStatus = 90; // Stop
  }

  if ((( servoPossition == servoMaxPossition ) && ( servoStatus == servoMoveUp )) || (( servoPossition == 0 ) && ( servoStatus == servoMoveDown))) {
    servoStatus = 90;
    Serial.println("Servo limit reached");
  }
  servo.write(servoStatus);
}


void countRotation() {
  currentTime = millis();

  if(currentTime >= (loopTime + 1)){
    encoder_A = digitalRead(encoderPinA);
    encoder_B = digitalRead(encoderPinB);
    if((!encoder_A) && (encoder_A_prev)){
      if(encoder_B) {
         servoPossition++;
      } else {
         servoPossition--;
      }
    Serial.println(servoPossition);
    }
    
    encoder_A_prev = encoder_A;
    loopTime = currentTime; 

  }
}

void setup() {
  servo.attach(servoPin);
  Serial.begin(115200);
  
  pinMode(encoderPinA, INPUT);
  pinMode(encoderPinB, INPUT);
  
  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.print("\n\r \n\rWorking to connect");
 
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("Curtain control device");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handle_root);
  server.on("/down", handle_down);
  server.on("/up", handle_up);
  server.on("/stop", handle_stop);
  server.on("/possition", handle_possition);
  server.begin();
  Serial.println("HTTP server started");

  currentTime = millis();
  loopTime = currentTime;

}

void loop() {
  changePosstition();
  countRotation();
  server.handleClient();
}
