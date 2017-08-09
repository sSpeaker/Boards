
#include <SPI.h>
#include <MySensor.h>
#include <MySigningAtsha204Soft.h>

#define BUTTON_PIN 3
#define LIGHT_PIN 4
#define ECHO_PIN 6
#define TRIG_PIN 5

#define DISTANCE 90
#define DISTANCE_ON_TIME 500 //seconds
#define DISTANCE_OFF_TIME 600000 //seconds

#define NodeID 40
#define CHILD_ID 0

MyTransportNRF24 radio;
MyHwATMega328 hw;

MySigningAtsha204Soft signer;

MySensor gw(radio, hw, signer);

MyMessage msg(CHILD_ID, V_STATUS);

long debouncing_time = 100; //Debouncing Time in Milliseconds

boolean light_status = true; //OFF by default
boolean button_state;
boolean button_prev;

void setup() {
 gw.begin(incomingMessage, NodeID, false);

 gw.sendSketchInfo("sink light", "1.0");
 gw.present(CHILD_ID, S_LIGHT);
 
 pinMode(LIGHT_PIN, OUTPUT);
 pinMode(TRIG_PIN ,OUTPUT);
 pinMode(ECHO_PIN ,INPUT);
 pinMode(BUTTON_PIN, INPUT);
 digitalWrite(BUTTON_PIN, HIGH); // Pull up
 
 button_state = digitalRead(BUTTON_PIN);
 button_prev = button_state;
}

unsigned long button_millis = millis();
unsigned long short_millis  = millis();
unsigned long long_millis  = millis();
boolean distance_on = false;

void loop() {
  digitalWrite(4, light_status);
  
  button_state = digitalRead(BUTTON_PIN);

  if ( button_state != button_prev ) {
    if((millis() - button_millis) >= debouncing_time) {
      Serial.println("Button");
      switch_light();
      button_prev = button_state;
      button_millis = millis();  
    }
  } else {
    button_millis = millis();  
  }

  long duration, distance;
  digitalWrite(TRIG_PIN, LOW);  // Added this line
  delayMicroseconds(2); // Added this line
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10); // Added this line
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH);
  distance = (duration/2) / 29.1;

  if ( distance <= DISTANCE ){
    if ( millis() - short_millis >= DISTANCE_ON_TIME && light_status) {
      light_status = false; //ON
      Serial.println("DISTANCE: ON");
      short_millis = millis();
      gw.send(msg.set(int(! light_status)));
    } else if ( ! light_status) {
      short_millis = millis();
    }
    Serial.println(distance);
    long_millis = millis();
  } else {
    if ( millis() - long_millis >= DISTANCE_OFF_TIME && ! light_status ) {
      light_status = true; //OFF
      Serial.println("DISTANCE: OFF");
      long_millis = millis();
      gw.send(msg.set(int(! light_status)));
    } else if ( light_status ) {
      long_millis = millis();
    }

    short_millis = millis();
  }
  //Serial.println(distance);
  gw.wait(50);
}

void incomingMessage(const MyMessage &message) {
  if (message.type == V_STATUS) { // This could be M_ACK_VARIABLE or M_SET_VARIABLE
    if ( message.getBool() ) {
      light_status = false; //ON
      Serial.println("incomingMessage: Recived ON");
    } else {
      light_status = true; //OFF
      Serial.println("incomingMessage: Recived OFF");
    }
  } else {
    Serial.println("Unknown message");
  }
}

void switch_light() {
  Serial.println("Light status changed");
  light_status = ! light_status;
  gw.send(msg.set(int(! light_status)));
}

