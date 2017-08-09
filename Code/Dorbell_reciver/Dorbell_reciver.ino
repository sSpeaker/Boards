#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();


void setup() {
  Serial.begin(9600);
  mySwitch.enableReceive(0); 
}

void loop() {
 if (mySwitch.available()) {
    
    int value = mySwitch.getReceivedValue();
    
    if (value == 7860) {
      Serial.println("beep");
    }
 }
 mySwitch.resetAvailable();

}
