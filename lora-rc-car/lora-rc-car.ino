#include <SPI.h>
#include <LoRa.h>
#include "LoraData.h"
#include <Servo.h>


#define SS_PIN    15
#define RST_PIN   2
#define DIO0_PIN  4
// #define SS_PIN    10
// #define RST_PIN   9
// #define DIO0_PIN  2

void charArrayToHexString(const char *data, size_t length, char *output) {
    for (size_t i = 0; i < length; i++) {
        sprintf(output + (i * 2), "%02X", (unsigned char)data[i]);
    }
}

Servo steer;

void setup() {
  Serial.begin(9600);

  steer.attach(16);

  LoRa.enableCrc();
  LoRa.setCodingRate4(6);

  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);

  if (!LoRa.begin(433E6)) {
    Serial.println("Lora module not connected.");
    while(1);
  }

  Serial.println("LoRa NodeEsp - Ready");
}
long latestMoveCommandTime = millis();
float latestMoveCommandForward = 0;
float latestMoveCommandLeftRight = 0;
void loop() {

  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    LoRa.beginPacket();
    LoRa.print(input);
    LoRa.endPacket(false);

    Serial.print("Sending: ");
    Serial.println(input);
    Serial.print("sent: ");
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String data="";
    while (LoRa.available()) {
      data += (char)LoRa.read();
    }
    Serial.println("Received: "+data);

    char hexString[4096];
    charArrayToHexString(data.c_str(), data.length(), hexString);
    Serial.println("As hex: "+String(hexString));

    struct StationMadePacket* packet;
    packet = (StationMadePacket*)data.c_str();
    if(packet->type == SEND_CONTROLS){
      struct CarControlData controls = packet->data.CarControlData;
      Serial.println("Forward: "+String(controls.forward)+" LeftRight: "+String(controls.leftRight));
      latestMoveCommandTime = millis();
      latestMoveCommandForward = controls.forward;
      latestMoveCommandLeftRight = controls.leftRight;
    }
    if(packet->type == PING_TO_CAR){
        LoRa.beginPacket();
        CarMadePacket pingResponse;
        pingResponse.type = PING_REPLY_TO_STATION;
        pingResponse.size = 0;  
        LoRa.write((uint8_t*)&pingResponse,2);
        LoRa.endPacket(false);
        Serial.println("Ping response!");
    }
  }
  if(millis() - latestMoveCommandTime < 1000){
    analogWrite(5,constrain(latestMoveCommandForward,0,127));
    analogWrite(0,constrain(-latestMoveCommandForward,0,127));
    steer.write(constrain(latestMoveCommandLeftRight,-1,1)*90 + 90);
  }
  else{
    analogWrite(0,0);
    analogWrite(5,0);
    
  }
}
