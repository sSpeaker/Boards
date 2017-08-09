#include <SPI.h>
#include <MySensor.h>
#include <MySigningAtsha204Soft.h>
#include <CapacitiveSensor.h>

#define SEND_PIN A1
#define RECEIVE_PIN_A A0
#define RECEIVE_PIN_B A2
#define BUTTON_REPEAT 5
#define TOUCH_BOUNCE 100

#define UP_PIN 7
#define SPEED_PIN 6
#define DOWN_PIN 8

#define MAX_SPINS 300

#define NodeID 31
#define CHILD_ID 0

MyTransportNRF24 radio;
MyHwATMega328 hw;

MySigningAtsha204Soft signer;

MySensor gw(radio, hw, signer);

MyMessage msg(CHILD_ID, V_PERCENTAGE);

CapacitiveSensor UP_BUTTON = CapacitiveSensor(SEND_PIN,RECEIVE_PIN_A);
CapacitiveSensor DOWN_BUTTON = CapacitiveSensor(SEND_PIN,RECEIVE_PIN_B);

String direction = "STOP";
int spin = 0;
int target_spin = 0;

void setup() {
  pinMode(UP_PIN, OUTPUT);
  pinMode(SPEED_PIN, OUTPUT);
  pinMode(DOWN_PIN, OUTPUT);

  digitalWrite(SPEED_PIN, HIGH);
  attachInterrupt(1, calculate_possition, CHANGE);

  gw.begin(incomingMessage, NodeID, false);

  gw.sendSketchInfo("Bedroom Cover", "2.0");
  gw.present(CHILD_ID, S_COVER);
}

void incomingMessage(const MyMessage &message) {
  if (message.type == V_PERCENTAGE) { // This could be M_ACK_VARIABLE or M_SET_VARIABLE
     String val = message.getString();

     rotate(val);
     Serial.print("incomingMessage: Recived ");
     Serial.println(val);

   } else if (message.type == V_UP) {
     Serial.println("incomingMessage: Recived UP");
     target_spin = MAX_SPINS;
     rotate("UP");

   } else if (message.type == V_DOWN) {
     Serial.println("incomingMessage: Recived DOWN");
     target_spin = 0;
     rotate("DOWN");

   } else if (message.type == V_STOP) {
     Serial.println("incomingMessage: Recived STOP");
     rotate("STOP");
   }
}

void calculate_possition () {
  if ( direction == "UP" ) {
    spin++;  
  } else if ( direction == "DOWN") {
    spin--;
  } else {
    Serial.println("IMPOSIBLE");  
  }
}


void motor_speed ( int pwm ){
  analogWrite(SPEED_PIN, pwm);
  Serial.print("Speed changed to ");
  Serial.println(pwm);
}

void rotate( String command ){
  if ( command == "UP" ) {
    digitalWrite(DOWN_PIN, LOW);
    gw.wait(250);
    digitalWrite(UP_PIN, HIGH);
    
    direction = "UP";
    Serial.println("Rotate: UP");

  } else if ( command == "DOWN" ) {
    digitalWrite(UP_PIN, LOW);
    gw.wait(250);
    digitalWrite(DOWN_PIN, HIGH);
    direction = "DOWN";
    Serial.println("Rotate: DOWN");
    
  } else if ( command == "STOP" ) {
    digitalWrite(UP_PIN, LOW);
    digitalWrite(DOWN_PIN, LOW);
    gw.wait(150);
    direction = "STOP";
    Serial.println("Rotate: STOP");
    
    int val = map( spin, 0, MAX_SPINS, 0, 100 );
    gw.send(msg.set(val));
    
    Serial.print("Spin: ");
    Serial.println(spin);

  } else {
    
    //In this case we got percentage position
    int value = command.toInt();
    if ( value <= 100 || value >= 0 ) {      
      target_spin = map(value, 0, 100, 0, MAX_SPINS);
  
      if ( target_spin > spin || target_spin == MAX_SPINS ) {
        Serial.println("Rotate %: Send self UP %");
        rotate("UP");
      } else if ( target_spin < spin || target_spin == 0 ){
        Serial.println("Rotate %: Send self DOWN %");
        rotate("DOWN");
      } else if ( target_spin == spin ) {
        rotate("STOP");
      }
    }
  }
}

int last_position = 0;
int up_button_repeat = 0;
int down_button_repeat = 0;
int double_button_repeat = 0;

void loop() {
  if ( spin == target_spin && direction != "STOP" ) {
      Serial.println("LOOP: Target spin reached. STOP");
      rotate("STOP");
    }

  int up_button = UP_BUTTON.capacitiveSensor(30);
  int down_button = DOWN_BUTTON.capacitiveSensor(30);

  if ( up_button >= TOUCH_BOUNCE && down_button >= TOUCH_BOUNCE) {
    up_button_repeat = 0;
    down_button_repeat = 0;
    
    if ( double_button_repeat == BUTTON_REPEAT ) {
      if (direction != "STOP")
        Serial.println("LOOP: STOP button detected.");
        rotate("STOP");
    } else if ( double_button_repeat == BUTTON_REPEAT*100 ) {
      Serial.println("RESET!!!!!!!!!!!!!!!!!!!!!!"); 
    }
    double_button_repeat++;
    
  } else if (up_button < TOUCH_BOUNCE && down_button >= TOUCH_BOUNCE) {
    double_button_repeat = 0;
    up_button_repeat = 0;
    if ( down_button_repeat == BUTTON_REPEAT ) {
      if (direction != "DOWN") {
        Serial.println("LOOP: DOWN button detected.");
        target_spin = 0;
        rotate("DOWN");
      }
      down_button_repeat = 0;
    } else {
      down_button_repeat++;
    }
  } else if ( up_button >= TOUCH_BOUNCE && down_button < TOUCH_BOUNCE ) {
    double_button_repeat = 0;
    down_button_repeat = 0;
    if ( up_button_repeat == BUTTON_REPEAT ) {
      if (direction != "UP") {
        Serial.println("LOOP: UP button detected.");
        target_spin = MAX_SPINS;
        rotate("UP");
      }
      up_button_repeat = 0;
    } else {
      up_button_repeat++;
    }
  } else {
      up_button_repeat = 0;
      down_button_repeat = 0;
      double_button_repeat = 0;
  }
  gw.process();
}
