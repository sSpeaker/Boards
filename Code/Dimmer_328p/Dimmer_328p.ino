#include <SPI.h>
#include <MySensor.h>
#include <MySigningAtsha204Soft.h>
#include "OneButton.h"

#define TRIAC_PIN 4
#define ZC_PIN 3
#define RED_PIN 6
#define GREEN_PIN 5
#define BUTTON_PIN A1

#define DIMMER_STEP 2
#define DIMMER_SPEED 20 //Delay in milliseconds betwean dimmer lavel change

#define NodeID 41
#define CHILD_ID 0

#define DEBUG

MyTransportNRF24 radio;
MyHwATMega328 hw;

MySigningAtsha204Soft signer;

MySensor gw(radio, hw, signer);

MyMessage msg(CHILD_ID, V_DIMMER);

OneButton button(BUTTON_PIN, false);

int dimmer = 128; //Store dimmer level. 0 - ON, 128 - OFF
boolean dimmer_direction = true; // false - DOWN; true - UP

void zero_cross() {
    if ( dimmer >= 126 ) {
      digitalWrite(TRIAC_PIN, LOW);
    } else if ( dimmer <= 2 ) {
      digitalWrite(TRIAC_PIN, HIGH);
    } else {
      delayMicroseconds(75 * dimmer); 
      digitalWrite(TRIAC_PIN, HIGH);
      delayMicroseconds(10); 
      digitalWrite(TRIAC_PIN, LOW);
    }
}

void setup() {
  Serial.begin(115200);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(TRIAC_PIN, OUTPUT);
  pinMode(ZC_PIN, INPUT);
  digitalWrite(ZC_PIN, HIGH);

  gw.begin(incomingMessage, NodeID, false);
  //gw.sendSketchInfo("Bedroom Light", "1.0");
  gw.present(CHILD_ID, S_DIMMER);
  
  attachInterrupt(1, zero_cross, RISING);

  button.attachClick(click);
  button.attachDoubleClick(doubleclick);
  button.attachLongPressStop(longPressStop);
  button.attachDuringLongPress(longPress);
}

void incomingMessage(const MyMessage &message) {
  if (message.type == V_PERCENTAGE) { // This could be M_ACK_VARIABLE or M_SET_VARIABLE
    Serial.print("V_PERCENTAGE message recived: ");
    Serial.print(atoi( message.data ));
    Serial.println("%");
    dimmer = map(atoi( message.data ), 100, 0, 0, 128);
  } else if (message.type == V_UP) {
    if ( dimmer < 128 ) {
      dimmer += 20; 
      if (dimmer > 128 ) {
        dimmer = 128;
      }
    }
    Serial.println("V_UP message recived");
  } else if (message.type == V_DOWN) {
    if ( dimmer > 0 ) {
      dimmer -= 20; 
      if (dimmer < 0 ) {
        dimmer = 0;
      }
    }
    Serial.println("V_DOWN message recived");
  } else {
    Serial.println("Unknown message recived");
  }
}

void click() {
  if (dimmer < 128 ) {
    dimmer = 128;
    dimmer_direction = true;
  } else {
    dimmer = 0;
    dimmer_direction = false;
  }
  //gw.send( msg.set( map(dimmer, 0, 128, 0, 100) ) );
  Serial.print("Click: ");
  Serial.println(dimmer);
}

void doubleclick() {
  dimmer = 0;
  dimmer_direction = false;
  //gw.send( msg.set( map(dimmer, 0, 128, 0, 100) ) );
  Serial.println("Double click: 0");
}

void longPress() {
  if ( ! dimmer_direction ) {
    if ( dimmer < 128 ) {
      dimmer = dimmer + DIMMER_STEP;
      Serial.print("Long press DOWN: ");
      Serial.println(dimmer);
    }
  } else {
    if ( dimmer > 0 ) {
      dimmer = dimmer - DIMMER_STEP;
      Serial.print("Long press UP: ");
      Serial.println(dimmer);
    }
  }
  gw.wait(DIMMER_SPEED);
}

void longPressStop() {
  dimmer_direction = ! dimmer_direction;
  //gw.send( msg.set( map(dimmer, 0, 128, 0, 100) ) );
  Serial.print("LongPress STOP: ");
  Serial.println(dimmer);
}

void loop() {
  if ( dimmer < 128 ) {
    digitalWrite(GREEN_PIN, LOW);
    digitalWrite(RED_PIN, HIGH);
  } else {
    digitalWrite(RED_PIN, LOW);
    digitalWrite(GREEN_PIN, HIGH);
  }
  gw.process();
  button.tick();
}
