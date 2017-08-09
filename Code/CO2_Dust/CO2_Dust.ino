#include <SPI.h>
#include <MySensor.h>
#include <MySigningAtsha204Soft.h>
//#include <Wire.h>
#include <SoftwareSerial.h>
 
#define NodeID 50

#define CO2_RX_PIN A0
#define CO2_TX_PIN A1

#define DUST_RX_PIN A2
#define DUST_TX_PIN A3

#define VOC_PIN 4

#define FAN_PIN 5

#define CHILD_ID_CO2 0
#define CHILD_ID_DUST01 1
#define CHILD_ID_DUST25 9
#define CHILD_ID_DUST10 10
#define CHILD_ID_FORM 11
#define CHILD_ID_TUL 12

MyTransportNRF24 radio;
MyHwATMega328 hw;

MySigningAtsha204Soft signer;

MySensor gw(radio, hw, signer);

MyMessage msgCO2(CHILD_ID_CO2, V_LEVEL);
MyMessage msgDust01(CHILD_ID_DUST01, V_LEVEL);
MyMessage msgDust25(CHILD_ID_DUST25, V_LEVEL);
MyMessage msgDust10(CHILD_ID_DUST10, V_LEVEL);
MyMessage msgForm(CHILD_ID_FORM, V_LEVEL);
MyMessage msgTul(CHILD_ID_TUL, V_LEVEL);

SoftwareSerial dust_serial(DUST_TX_PIN, DUST_RX_PIN); //TX, RX
SoftwareSerial co2_serial(CO2_TX_PIN, CO2_RX_PIN); //TX, RX

#define LENG 31   //0x42 + 31 bytes equal to 32 bytes

void setup() {
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW);
  analogWrite(FAN_PIN, 1023);
  co2_serial.begin(9600);
  dust_serial.begin(9600);
  
  gw.begin(NULL, NodeID, false);
  gw.sendSketchInfo("Air quality sensor","0.1");
  
  // Register all sensors to gateway (they will be created as child devices)
  gw.present(CHILD_ID_CO2, S_AIR_QUALITY);
  gw.present(CHILD_ID_DUST01, S_DUST);
  gw.present(CHILD_ID_DUST25, S_DUST);
  gw.present(CHILD_ID_FORM, S_AIR_QUALITY);
  gw.present(CHILD_ID_TUL, S_AIR_QUALITY);
  analogWrite(FAN_PIN, 6);
}

char checkValue(unsigned char *thebuf, char leng)
{  
  char receiveflag=0;
  int receiveSum=0;
 
  for(int i=0; i<(leng-2); i++){
    receiveSum=receiveSum+thebuf[i];
  }
  receiveSum=receiveSum + 0x42;
  
  if(receiveSum == ((thebuf[leng-2]<<8)+thebuf[leng-1])) {
    receiveSum = 0;
    receiveflag = 1;
  }
  
  return receiveflag;
}

void loop() 
{
  gw.process();

  byte cmd[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79}; 
  unsigned char response[9];
  
  co2_serial.listen();
  
  co2_serial.write(cmd, 9);
  memset(response, 0, 9);
  co2_serial.readBytes(response, 9);
  byte i;
  byte crc = 0;
  for (i = 1; i < 8; i++) crc+=response[i];
  crc = 255 - crc;
  crc++;
  short co2ppm = 0;
  if ( !(response[0] == 0xFF && response[1] == 0x86 && response[8] == crc) ) {
    Serial.println("CRC error: " + String(crc) + " / "+ String(response[8]));
  } else {
    unsigned int responseHigh = (unsigned int) response[2];
    unsigned int responseLow = (unsigned int) response[3];
    co2ppm = (256*responseHigh) + responseLow;
  }
  if ( co2ppm >= 300 ) {
    Serial.println("CO2: " + String(co2ppm) + "ppm");
    gw.send(msgCO2.set(co2ppm, 0));
  }

  dust_serial.listen();
  
  unsigned char buf[LENG];
  static short PM01Value = 0;
  static short PM2_5Value = 0;
  static short PM10Value = 0;

  if(dust_serial.find(0x42)){    
    dust_serial.readBytes(buf,LENG);
   
    if(buf[0] == 0x4d){
      if(checkValue(buf,LENG)){
        PM01Value=((buf[3]<<8) + buf[4]);
        PM2_5Value=((buf[5]<<8) + buf[6]);
        PM10Value=((buf[7]<<8) + buf[8]);
        
        Serial.println("Dust 0.1 " + String(PM01Value) + " ug/m3" );
        Serial.println("Dust 2.5 " + String(PM2_5Value) + " ug/m3" );
        Serial.println("Dust 10  " + String(PM10Value) + " ug/m3" );
        gw.send(msgDust01.set(PM01Value, 0));
        gw.send(msgDust25.set(PM2_5Value, 0));
        gw.send(msgDust10.set(PM10Value, 0));
      }
    }
  }

  float val = analogRead(VOC_PIN) * (3.3 / 1023.0);
  float f_ppm = pow(10,(-0.125*val*val + 1.528*val-2.631));
  float t_ppm = pow(10,(-0.210*val*val + 2.852*val-7.071));
  
  Serial.println("Formaldehyde " + String(f_ppm) + " ppm");
  Serial.println("Toluene " + String(t_ppm) + " ppm");
  Serial.println();
  
  gw.send(msgForm.set(f_ppm, 2));
  gw.send(msgTul.set(t_ppm, 2));
  
  gw.wait(60000);
}
