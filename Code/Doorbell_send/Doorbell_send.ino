#include <CapacitiveSensor.h>

#include <SPI.h>
#include <MySensor.h>
#include <MySigningAtsha204Soft.h>

#define LED_PIN 4
#define TOUCH_VALUE 120
#define SEND_ON_FREQ 4000 //How often send value. Milliseconds
#define SEND_OFF_FREQ 60000
#define TOUCH_DEBOUNCE 30  //

#define NodeID 30

#define CHILD_ID 0

MyTransportNRF24 radio;
MyHwATMega328 hw;

//uint8_t soft_serial[SHA204_SERIAL_SZ] = {0x027,0x03,0xa1,0xaa,0x06,0x11,0x25,0xc3,0xe1};
MySigningAtsha204Soft signer; //(true, 0, NULL, soft_serial);

MySensor gw(radio, hw, signer);

MyMessage msg(CHILD_ID, V_STATUS);

CapacitiveSensor   touch = CapacitiveSensor(6,7);        // 10M resistor between pins 4 & 2, pin 2 is sensor pin, add a wire and or foil if desired


void blink(int DELAY, int REPEAT)
{
  for(int i=0; i<REPEAT; i++) {
    digitalWrite(LED_PIN,HIGH);
    gw.wait(DELAY);
    digitalWrite(LED_PIN,LOW);
    gw.wait(DELAY);
  }
}

void setup()                    
{
  
  gw.begin(NULL, NodeID, true);

  gw.sendSketchInfo("Doorbell button", "1.0");
  gw.present(CHILD_ID, S_LIGHT);
  
  pinMode(LED_PIN,OUTPUT);
  blink(200, 2);

  Serial.println("Start");
}

unsigned long last_send = millis();
int debounce = 0;

void loop()
{  
  long total1 =  touch.capacitiveSensor(30);
  //Serial.println(total1);
  
  if ( total1 > TOUCH_VALUE && (millis() - last_send) > SEND_ON_FREQ ) {
    if ( debounce == TOUCH_DEBOUNCE ) {
      Serial.println("Click");
      last_send = millis();
      debounce = 0;
      gw.send(msg.set(1));
      blink(300,3);
    } else {
      debounce++;
    }
  } else {
    debounce = 0;
  }
  
  if ((millis() - last_send) > SEND_OFF_FREQ ){
    Serial.println("Heartbeat");
    gw.send(msg.set(0));
    last_send = millis();
    blink(100,1);
  }
  //gw.wait(50);
}
