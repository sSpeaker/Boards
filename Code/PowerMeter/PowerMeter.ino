#include <SPI.h>
#include <MySensor.h>
#include <MySigningAtsha204Soft.h>
#include <SoftwareSerial.h>
#include <PZEM004T.h>

#define LOCK_PIN 3
#define LOCK_LED_PIN 4
#define STATUS_LED 5


#define NodeID 10

#define CHILD_ID_VOLTAGE 0
#define CHILD_ID_CURRENT 1
#define CHILD_ID_POWER 2
#define CHILD_ID_KWH 3 
#define CHILD_ID_LOCK 4

unsigned long SLEEP_TIME = 60000; // Sleep time between reads (in milliseconds)

MyTransportNRF24 radio;
MyHwATMega328 hw;

MySigningAtsha204Soft signer;

MySensor gw(radio, hw, signer);

MyMessage msgVoltage(CHILD_ID_VOLTAGE, V_VOLTAGE);
MyMessage msgCurrent(CHILD_ID_CURRENT, V_CURRENT);
MyMessage msgPower(CHILD_ID_POWER, V_WATT);
MyMessage msgKWH(CHILD_ID_KWH, V_KWH);
MyMessage msgLock(CHILD_ID_LOCK, V_TRIPPED);

PZEM004T pzem(8,7);  // RX,TX
IPAddress ip(192,168,1,1);

bool check_lock() {
  digitalWrite(LOCK_LED_PIN, HIGH);
  gw.wait(15);
  bool value = digitalRead(LOCK_PIN);
  digitalWrite(LOCK_LED_PIN, LOW);
  if (value) {
    digitalWrite(STATUS_LED, HIGH);
  } else {
    digitalWrite(STATUS_LED, LOW);
  }
  return value;
}

void setup() {
  gw.begin(NULL, NodeID, false);
  gw.sendSketchInfo("Power Meter + Lock", "1.0");

  gw.present(CHILD_ID_VOLTAGE, S_MULTIMETER);
  gw.present(CHILD_ID_CURRENT, S_MULTIMETER);
  gw.present(CHILD_ID_POWER, S_POWER);
  gw.present(CHILD_ID_KWH, S_POWER);
  gw.present(CHILD_ID_LOCK, S_MOTION);
  
  pzem.setAddress(ip);

  pinMode(LOCK_PIN, INPUT);
  digitalWrite(LOCK_PIN, LOW);
  pinMode(LOCK_LED_PIN, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);

  //We need more ground. 
  pinMode(A0, OUTPUT);
  digitalWrite(A0, LOW);
  pinMode(A1, OUTPUT);
  digitalWrite(A1, LOW);

#define LOCK_PIN 3
#define LOCK_LED_PIN 4
#define STATUS_LED 5
}

unsigned long last_send=millis();
unsigned long last_lock_send=millis();
bool lock_prev_status = 0;

void loop() {
  if ((millis() - last_send) >= SLEEP_TIME) {
    float v = pzem.voltage(ip);
    Serial.print("Voltage: ");
    Serial.print(v);
    Serial.println("V;");
    if (v >= 0.0)
      gw.send(msgVoltage.set(v,2));
    
    float i = pzem.current(ip);
    Serial.print("Current: ");
    Serial.print(i);
    Serial.println("A;"); 
    if(i >= 0.0) 
      gw.send(msgCurrent.set(i,2));
    
    float p = pzem.power(ip);
    Serial.print("Power: ");
    Serial.print(p);
    Serial.println("W;");
    if(p >= 0.0)
      gw.send(msgPower.set(p,2));
    
    float e = pzem.energy(ip);
    Serial.print("Energy: ");
    Serial.print(e);
    Serial.println("Wh;");
    if(e >= 0.0)
      gw.send(msgKWH.set(e,2));
    last_send=millis();
  }

  bool lock_status = check_lock();
  if ((lock_status != lock_prev_status) || ((millis() - last_lock_send) >= SLEEP_TIME)) {
    Serial.print("Lock status: ");
    Serial.println(lock_status ? "LOCKED" : "OPEN");
    gw.send(msgLock.set(lock_status ? 1 : 0));
    lock_prev_status = lock_status;
    last_lock_send = millis();
  }  
  gw.wait(500);
}
