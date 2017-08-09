
#include <SPI.h>
#include <MySensor.h>
#include <MySigningAtsha204Soft.h>  
#include <BH1750.h>
#include <Wire.h>
#include <dht.h>

#define NodeID 1

#define DHT22_VCC 7
#define DHT22_PIN 6
#define LIGHT_VCC 8
#define MOTION_PIN 3
#define LED_PIN 4

// Declare interrupt pin
#define INTERRUPT MOTION_PIN-2

#define CHILD_ID_LIGHT 0
#define CHILD_ID_HUM 1
#define CHILD_ID_TEMP 2
#define CHILD_ID_MOTION 3 

unsigned long SLEEP_TIME = 60000; // Sleep time between reads (in milliseconds)

BH1750 lightSensor;
dht DHT;

MyTransportNRF24 radio;
MyHwATMega328 hw;

//uint8_t soft_serial[SHA204_SERIAL_SZ] = {0x027,0x03,0xa1,0xaa,0x06,0x11,0x25,0xa3,0xa1};

MySigningAtsha204Soft signer; //(true, 0, NULL, soft_serial);

MySensor gw(radio, hw, signer);

MyMessage msgLight(CHILD_ID_LIGHT, V_LIGHT_LEVEL);
MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
MyMessage msgMot(CHILD_ID_MOTION, V_TRIPPED);

// MyMessage msg(CHILD_ID_LIGHT, V_LEVEL);  

boolean interrupt = false;

void blink()
{
  digitalWrite(LED_PIN, HIGH);
  gw.wait(1000);
  digitalWrite(LED_PIN, LOW);
}

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


void setup()  
{ 
  pinMode(DHT22_VCC, OUTPUT);
  pinMode(MOTION_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  analogReference(INTERNAL);

  gw.begin(NULL, NodeID, false);
  gw.wait(50);
  gw.sendSketchInfo("Light Lux Sensor", "2.1");
  gw.wait(50);
  // Register all sensors to gateway (they will be created as child devices)
  gw.present(CHILD_ID_LIGHT, S_LIGHT_LEVEL);
  gw.wait(50);
  gw.present(CHILD_ID_HUM, S_HUM);
  gw.wait(50);
  gw.present(CHILD_ID_TEMP, S_TEMP);
  gw.wait(50);
  gw.present(CHILD_ID_MOTION, S_MOTION);

  lightSensor.begin(BH1750_ONE_TIME_LOW_RES_MODE);
}

void loop()
{ 
  digitalWrite(DHT22_VCC, HIGH); // Turn on DHT22 here to save time.
  unsigned long dht_time = millis();
  
  float batteryV  = readVcc();
  int batteryPcnt = ( ( batteryV - 3000 ) / 1200 ) * 100;
  Serial.print("Battery Voltage: ");
  Serial.print(batteryV);
  Serial.print(" mV; ");
  Serial.print("percentage: ");
  Serial.print(batteryPcnt);
  Serial.println(" %");
  gw.sendBatteryLevel(batteryPcnt);

  // Motion
  int tripped;
  if (interrupt) {
    //blink();
    tripped = 1;
  } else {
    if (digitalRead(MOTION_PIN) == HIGH) {
      //blink();
      tripped = 1; 
    } else {
      tripped = 0;
    }
  }
  Serial.print("Motion detected: ");
  Serial.println(tripped);
  
  gw.send(msgMot.set(tripped));  // Send tripped value to gw 

  //Light
  lightSensor.configure(BH1750_ONE_TIME_LOW_RES_MODE);
  uint16_t lux = lightSensor.readLightLevel();// Get Lux value
  
  Serial.print("Light: ");
  Serial.print(lux);
  Serial.println(" lx");

  gw.send(msgLight.set(lux));
  
  // Temp & Hum
  if ((millis() - dht_time) < 1000 ) {
    Serial.print("Wait fro DHT22: ");
    Serial.println(1000 - (millis() - dht_time));
    gw.wait(1000 - (millis() - dht_time));
  }
  Serial.print("DHT22, \t");
  int chk = DHT.read22(DHT22_PIN);

  digitalWrite(DHT22_VCC, LOW);
  switch (chk)
  {
    case DHTLIB_OK:
    Serial.print("OK,\t"); 
    break;
    case DHTLIB_ERROR_CHECKSUM: 
    Serial.println("Checksum error,\t"); 
    break;
    case DHTLIB_ERROR_TIMEOUT: 
    Serial.println("Time out error,\t"); 
    break;
    default: 
    Serial.println("Unknown error,\t"); 
    break;
  }
  // DISPLAY DATA
  if (chk == DHTLIB_OK ) {
    float temperature = DHT.temperature;
    float humidity = DHT.humidity;
    
    Serial.print(DHT.humidity, 1);
    Serial.print(",\t");
    Serial.println(DHT.temperature, 1);

    gw.send(msgTemp.set(temperature, 1)); 
    gw.send(msgHum.set(humidity, 1));
  }
  //We don't need to send motion trigger every minute.
  if (interrupt) {
    interrupt = false;
    gw.sleep(SLEEP_TIME);
  } else {
    interrupt = gw.sleep(INTERRUPT,RISING, SLEEP_TIME);
  }
}
