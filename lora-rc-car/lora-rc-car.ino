#include <SPI.h>
#include <LoRa.h>
#include "LoraData.h"

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


void setup() {
  Serial.begin(9600);

  LoRa.enableCrc();
  LoRa.setCodingRate4(6);

  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);

  if (!LoRa.begin(433E6)) {
    Serial.println("Lora module not connected.");
    while(1);
  }

  Serial.println("LoRa NodeEsp - Ready");
}

void loop() {

  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');

    LoRa.beginPacket();
    LoRa.print(input);
    LoRa.endPacket(false);

    Serial.print("Sending: ");
    Serial.println(input);
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String data="";
    while (LoRa.available()) {
      data += (char)LoRa.read();
    }
    Serial.println("Received: "+data);

    char hexString[data.length()*2+1];
    charArrayToHexString(data.c_str(), data.length(), hexString);
    Serial.println("As hex: "+String(hexString));

    struct StationMadePacket* packet;
    packet = (StationMadePacket*)data.c_str();
    if(packet->type == SEND_CONTROLS){
      struct CarControlData controls = *(CarControlData*)(packet->data);
      Serial.println("Forward: "+String(controls.forward)+" LeftRight: "+String(controls.leftRight));
    }
  }
}
