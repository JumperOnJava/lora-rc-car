#include <SPI.h>
#include <LoRa.h>

#define SS_PIN    10
#define RST_PIN   9
#define DIO0_PIN  2

const char* nodeName = "NodeArduino";

void setup() {
  Serial.begin(9600);
  while (!Serial);


  LoRa.enableCrc();
  LoRa.setCodingRate4(5);

  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("Lora module not connected.");
    while (1);
  }

  Serial.println("LoRa NodeArduino - Ready");
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
    String received = "";
    while (LoRa.available()) {
      received += (char)LoRa.read();
    }
    Serial.print("Receiving: ");
    Serial.println(received);
  }
}
