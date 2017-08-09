#include <SPI.h>
#include <MySensor.h>
#include <MySigningAtsha204Soft.h>
#include <CapacitiveSensor.h>
#include <Servo.h>

#define POSITION_PIN A1
#define POSITION_BOUNCE 10

#define END_PIN_A A2
#define END_PIN_B A3
#define SERVO_PIN 6
#define END_A_LIMIT 560 //580
#define END_B_LIMIT 420 //400

#define SEND_PIN 3
#define RECEIVE_PIN_A 4
#define RECEIVE_PIN_B 5
#define BUTTON_REPEAT 5
#define TOUCH_BOUNCE 100

#define SERVO_UP 0
#define SERVO_STOP 90
#define SERVO_DOWN 180
#define MAX_SPINS 38

#define NodeID 30
#define CHILD_ID 0

#define SEND_TIME 30000
#define MINIMUM_SEND_TIME 10000

MyTransportNRF24 radio;
MyHwATMega328 hw;

MySigningAtsha204Soft signer;

MySensor gw(radio, hw, signer);

MyMessage msg(CHILD_ID, V_PERCENTAGE);

Servo servo;

CapacitiveSensor UP_BUTTON = CapacitiveSensor(SEND_PIN,RECEIVE_PIN_A);
CapacitiveSensor DOWN_BUTTON = CapacitiveSensor(SEND_PIN,RECEIVE_PIN_B);

void setup() {
  servo.attach(SERVO_PIN);
  servo.write(SERVO_STOP);

  gw.begin(incomingMessage, NodeID, false);

  gw.sendSketchInfo("Bedroom Cover", "1.0");
  gw.present(CHILD_ID, S_COVER);
}

String direction = "STOP";
int spin = 0;
int target_spin = 0;
boolean rotate_to_end = false;

void rotate( String command ){
  if ( command == "UP" ) {
    if ( !servo.attached() )
      servo.attach(SERVO_PIN);

    servo.write(SERVO_UP);
    direction = "UP";
    Serial.println("Rotate: UP");

  } else if ( command == "DOWN" ) {
    
    if ( !servo.attached() )
      servo.attach(SERVO_PIN);

    servo.write(SERVO_DOWN);
    direction = "DOWN";
    Serial.println("Rotate: DOWN");
    
  } else if ( command == "STOP" ) {
    
    servo.write(SERVO_STOP);
    if ( servo.attached() )
      servo.detach();

    rotate_to_end = false;
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
  
      if ( target_spin == 0 ) {
        rotate_to_end = true;
        rotate("DOWN");
      } else if (target_spin == MAX_SPINS ) {
        rotate_to_end = true;
        rotate("UP");
      } else if ( target_spin > spin ) {
        rotate_to_end = false;
        Serial.println("Rotate %: Send self UP %");
        rotate("UP");
      } else if ( target_spin < spin ){
        rotate_to_end = false;
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

int top_repeat = 0;
int zero_repeat = 0;
int down_repeat =0;

void loop() {
  int end_a = analogRead(END_PIN_A);
  int end_b = analogRead(END_PIN_B);

  if (end_a >= END_A_LIMIT && direction == "DOWN") {
    spin = 0;
    Serial.println("LOOP: Top down detected");
    rotate("STOP");
  } else if (end_b <= END_B_LIMIT && direction == "UP") {
    spin = MAX_SPINS;
    Serial.println("LOOP: Top up detected");
    rotate("STOP");
  }

  if ( direction != "STOP" ) {
    int sensorValue = analogRead(POSITION_PIN);
    if (sensorValue > 550) {
      zero_repeat = 0;
      down_repeat = 0;
      if ( last_position != 1  ) {
        if ( top_repeat == POSITION_BOUNCE ){
          direction == "UP" ? spin++ : spin--;  
          last_position = 1;
          top_repeat = 0;
          Serial.print("Top: ");
          Serial.println(spin);
        } else {
          top_repeat++;
        }
      }
    } else if (sensorValue < 490 ) {
      if (last_position != -1 ) {
        zero_repeat = 0;
        top_repeat = 0;
        if ( down_repeat == POSITION_BOUNCE ){
          direction == "UP" ? spin++ : spin--;
          last_position = -1;
          down_repeat = 0;
          Serial.print("Down: ");
          Serial.println(spin);
        } else {
          down_repeat++;
        }
      }
    } else {
      if (last_position != 0 ) {
        top_repeat = 0;
        down_repeat = 0;
        if ( zero_repeat == POSITION_BOUNCE ){
          Serial.println("Zero");
          last_position = 0;
          zero_repeat = 0;
        } else {
          zero_repeat++;
        }
      }
    }
    
    if ( spin == target_spin && !rotate_to_end ) {
      Serial.println("LOOP: Target spin reached. STOP");
      rotate("STOP");
    }
  }

  int up_button = UP_BUTTON.capacitiveSensor(30);
  int down_button = DOWN_BUTTON.capacitiveSensor(30);

  if (up_button >= TOUCH_BOUNCE && down_button >= TOUCH_BOUNCE) {
    up_button_repeat = 0;
    down_button_repeat = 0;
    if ( double_button_repeat == BUTTON_REPEAT ) {
      if (direction != "STOP")
        Serial.println("LOOP: STOP button detected.");
        rotate("STOP");
      double_button_repeat = 0;
    } else {
      double_button_repeat++;
    }
  } else if (up_button < TOUCH_BOUNCE && down_button >= TOUCH_BOUNCE) {
    double_button_repeat = 0;
    up_button_repeat = 0;
    if ( down_button_repeat == BUTTON_REPEAT ) {
      if (direction != "DOWN") {
        rotate_to_end = true;
        Serial.println("LOOP: DOWN button detected.");
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
        rotate_to_end = true;
        Serial.println("LOOP: UP button detected.");
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

void incomingMessage(const MyMessage &message) {
  if (message.type == V_PERCENTAGE) { // This could be M_ACK_VARIABLE or M_SET_VARIABLE
     String val = message.getString();
     rotate_to_end = false;
     rotate(val);
     Serial.print("incomingMessage: Recived ");
     Serial.println(val);

   } else if (message.type == V_UP) {
     Serial.println("incomingMessage: Recived UP");
     rotate_to_end = true;
     rotate("UP");

   } else if (message.type == V_DOWN) {
     Serial.println("incomingMessage: Recived DOWN");
     rotate_to_end = true;
     rotate("DOWN");

   } else if (message.type == V_STOP) {
     Serial.println("incomingMessage: Recived STOP");
     rotate("STOP");
   }
}
