#include <SPI.h>
#include <MySensor.h>
#include <MySigningAtsha204Soft.h>

#define MIN_BATTERY_V  3000
#define MAX_BATTERY_V  4200

#define SENSOR_PIN 3

#define INTERRUPT SENSOR_PIN-2

#define NodeID 21
#define MY_DEBUG_VERBOSE

#define CHILD_ID 0

unsigned long SLEEP_TIME = 3600000; // Sleep time between reads (in milliseconds)
unsigned long SLEEP_TIME_ALARM = 5000; // Sleep time between reads (in milliseconds)

MyTransportNRF24 radio;
MyHwATMega328 hw;

MySigningAtsha204Soft signer;

MySensor gw(radio, hw, signer);

MyMessage msg(CHILD_ID, V_TRIPPED);

boolean interrupt = false;

long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  
 
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring
 
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both
 
  long result = (high<<8) | low; 
  
  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}



void setup() {
  gw.begin(NULL, NodeID, false);
  Serial.println("GW Begin");
  gw.sendSketchInfo("Water leak", "1.0");
  gw.present(CHILD_ID, S_WATER_LEAK);
  
  pinMode(SENSOR_PIN, INPUT);
  digitalWrite(SENSOR_PIN, HIGH); //Pull-up
 
}

void loop() {
  Serial.println(millis());
  int tripped;
  if (interrupt) {
    tripped = 1;
  } else {
    if (digitalRead(SENSOR_PIN) == LOW) {
      tripped = 1; 
    } else {
      tripped = 0;
    }
  }
  Serial.print("Water detected: ");
  Serial.println(tripped);
  gw.send(msg.set(tripped));

  if ( tripped == 0 ) {
    float batteryV = readVcc();
    int batteryPcnt = (((batteryV - MIN_BATTERY_V) / (MAX_BATTERY_V - MIN_BATTERY_V)) * 100 );
    if (batteryPcnt > 100) {
      batteryPcnt = 100;
    }
    
    Serial.print("Battery Voltage: ");
    Serial.print(batteryV/1000);
    Serial.print(" V; ");
    Serial.print("percentage: ");
    Serial.print(batteryPcnt);
    Serial.println(" %");
    gw.sendBatteryLevel(batteryPcnt);
  }
  if (digitalRead(SENSOR_PIN) == LOW) {
    interrupt = false;
    Serial.println("Alarm sleep");
    gw.sleep(SLEEP_TIME_ALARM);
  } else {
    Serial.println("Normal sleep");
    interrupt = gw.sleep(INTERRUPT, FALLING, SLEEP_TIME);
  }
}
