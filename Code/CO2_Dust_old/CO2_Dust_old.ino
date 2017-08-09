#include <SoftwareSerial.h>;
#include <SPI.h>
#include <MySensor.h>
#include <MySigningAtsha204Soft.h>
#include <livolo.h>
#include <RCSwitch.h>
 
#define NodeID 50

#define CO2_RX_PIN A0
#define CO2_TX_PIN A1

#define DUST_READ_PIN A2
#define DUST_LED_PIN 4

#define DUST_SAMPLING_TIME 280
#define DUST_DELTA_TIME 40
#define DUST_SLEEP_TIME 9680

#define TRANSMITER_PIN 5

#define LIVOLO_ON 0
#define LIVOLO_OFF 106

#define CHILD_ID_CO2 0
#define CHILD_ID_DUST 1
#define CHILD_ID_HUM 2
#define CHILD_ID_BEDROOM 3
#define CHILD_ID_KITCHEN 4
#define CHILD_ID_BATH 5
#define CHILD_ID_STORE 6
#define CHILD_ID_CORRIDOR 7
#define CHILD_ID_BEDROOM_LEVEL 8

MyTransportNRF24 radio;
MyHwATMega328 hw;

MySigningAtsha204Soft signer;

MySensor gw(radio, hw, signer);

MyMessage msgCO2(CHILD_ID_CO2, V_LEVEL);
MyMessage msgDust(CHILD_ID_DUST, V_LEVEL);

SoftwareSerial mySerial(CO2_TX_PIN, CO2_RX_PIN);

byte cmd[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79}; 
unsigned char response[9];

RCSwitch mySwitch = RCSwitch();

Livolo livolo(TRANSMITER_PIN);

void setup() {
  //mySerial.begin(9600);
  
  mySwitch.enableTransmit(TRANSMITER_PIN);  // Using Pin #10
  mySwitch.setPulseLength(180);
  
  gw.begin(incomingMessage, NodeID, false);
  gw.sendSketchInfo("Air quality sensor","0.1");
  
  // Register all sensors to gateway (they will be created as child devices)
  gw.present(CHILD_ID_CO2, S_AIR_QUALITY);
  gw.present(CHILD_ID_DUST, S_DUST);
  gw.present(CHILD_ID_HUM, S_LIGHT);
  gw.present(CHILD_ID_BEDROOM, S_LIGHT);
  gw.present(CHILD_ID_BEDROOM_LEVEL, S_LIGHT);
  gw.present(CHILD_ID_KITCHEN, S_LIGHT);
  gw.present(CHILD_ID_BATH, S_LIGHT);
  gw.present(CHILD_ID_STORE, S_LIGHT);
  gw.present(CHILD_ID_CORRIDOR, S_LIGHT);

  pinMode(DUST_LED_PIN,OUTPUT);
}

float dust() {
  uint16_t voMeasured = 0;
  float calcVoltage = 0;
  float dustConcentration = 0;

  digitalWrite(DUST_LED_PIN,LOW); // power on the LED
  delayMicroseconds(DUST_SAMPLING_TIME);
  
  voMeasured = analogRead(DUST_READ_PIN); // read the dust value
  
  delayMicroseconds(DUST_DELTA_TIME);
  digitalWrite(DUST_LED_PIN,HIGH); // turn the LED off
  delayMicroseconds(DUST_SLEEP_TIME);
  
  // 0 - 5V mapped to 0 - 1023 integer values
  // recover voltage
  calcVoltage = voMeasured * (5.0 / 1024.0);

 //http://www.pocketmagic.net/sharp-gp2y1010-dust-sensor/
  if (calcVoltage < 0.583)  
    dustConcentration = 0;
  else
    dustConcentration = (6 * calcVoltage / 35 - 0.1) * 1000;
   /* 
  dustDensity = 6 * calcVoltage / 35 - 0.1;
  
  Serial.print("Raw Signal Value (0-1023): ");
  Serial.print(voMeasured);
  
  Serial.print(" - Voltage: ");
  Serial.print(calcVoltage);
  
  Serial.print(" - Dust Density: ");
  Serial.println(dustConcentration); // unit: mg/m3
  */
  
  return(dustConcentration);
}

void incomingMessage(const MyMessage &message) {
  // We only expect one type of message from controller. But we better check anyway.
  if (message.type==V_LIGHT) {
     switch (message.sensor) {
        case CHILD_ID_HUM:
          Serial.print("Humidifire: ");
          if (message.getBool()) {
            mySwitch.send(4210995, 24); //ON
          } else {
            mySwitch.send(4211004, 24); //OFF
          }
          break;
        case CHILD_ID_BEDROOM:
          Serial.print("Bedroom light: ");
          if (message.getBool()) {
            livolo.sendButton(6400, 108);
          } else {
            livolo.sendButton(6400, LIVOLO_OFF);
          }
        break;
        case CHILD_ID_BEDROOM_LEVEL:
          Serial.print("Bedroom light: ");
          if (message.getBool()) {
            livolo.sendButton(6400, 92);
          } else {
            livolo.sendButton(6400, 116);
          }
        break;
        case CHILD_ID_KITCHEN:
          Serial.print("Kitchen light: ");
          if (message.getBool()) {
            livolo.sendButton(6434, LIVOLO_ON);
          } else {
            livolo.sendButton(6434, LIVOLO_OFF);
          }
        break;
        case CHILD_ID_BATH:
          Serial.print("Bath light: ");
          if (message.getBool()) {
            livolo.sendButton(6412, LIVOLO_ON);
          } else {
            livolo.sendButton(6412, LIVOLO_OFF);
          }
        break;
        case CHILD_ID_STORE:
          Serial.print("Store light: ");
          if (message.getBool()) {
            livolo.sendButton(6445, LIVOLO_ON);
          } else {
            livolo.sendButton(6445, LIVOLO_OFF);
          }
        break;
        case CHILD_ID_CORRIDOR:
          Serial.print("Corridor light: ");
          if (message.getBool()) {
            livolo.sendButton(6423, LIVOLO_ON);
          } else {
            livolo.sendButton(6423, LIVOLO_OFF);
          }
        break;
        default: 
          Serial.print("Unknown device id: ");
        break;
     }
     
     if (message.getBool()) {
       Serial.println("ON");
     } else {
       Serial.println("OFF");
     }           
   } 
}

void loop() 
{
  gw.process();
  
  float dust_level = dust();
  Serial.print("Dust: ");
  Serial.println(dust_level);
  gw.send(msgDust.set(dust_level, 1)); 

  mySerial.write(cmd, 9);
  memset(response, 0, 9);
  mySerial.readBytes(response, 9);
  int i;
  byte crc = 0;
  for (i = 1; i < 8; i++) crc+=response[i];
  crc = 255 - crc;
  crc++;

  if ( !(response[0] == 0xFF && response[1] == 0x86 && response[8] == crc) ) {
    Serial.println("CRC error: " + String(crc) + " / "+ String(response[8]));
  } else {
    unsigned int responseHigh = (unsigned int) response[2];
    unsigned int responseLow = (unsigned int) response[3];
    unsigned int ppm = (256*responseHigh) + responseLow;
    Serial.print("CO2: ");
    Serial.println(ppm);
    gw.send(msgCO2.set(ppm, 0));
  }
 
  gw.wait(60000);
}
