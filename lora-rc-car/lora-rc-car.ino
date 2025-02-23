#include <SPI.h>
#include <LoRa.h>
#include "LoraData.h"

#define SS_PIN    10
#define RST_PIN   9
#define DIO0_PIN  2

const char* nodeName = "NodeArduino";

void setup() {
  Serial.begin(9600);
  while (!Serial);


  LoRa.enableCrc();
  LoRa.setCodingRate4(6);

  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("Lora module not connected.");
    while (1);
  }

  Serial.println("LoRa NodeArduino - Ready");
}

void loop() {
  // if (Serial.available()) {
  //   String input = Serial.readStringUntil('\n');

  //   LoRa.beginPacket();
  //   LoRa.print(input);
  //   LoRa.endPacket(false);

  //   Serial.print("Sending: ");
  //   Serial.println(input);
  // }

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String data="";
    while (LoRa.available()) {
      data += (char)LoRa.read();
    }
    struct StationMadePacket* packet;
    packet = (StationMadePacket*)data.c_str();
    if(packet->type == SEND_CONTROLS){
      struct CarControlData* controls = (CarControlData*)packet->data;
      Serial.println("Forward: "+String(controls->forward)+" LeftRight: "+String(controls->leftRight));
    }
  }
}
