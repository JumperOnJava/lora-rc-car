#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

const char *ssid = "rc-car-station";
const char *password = "12345678";

void setup() {
    Serial.begin(115200);

    WiFi.softAP(ssid, password);
    Serial.println("Access Point Started");

    // Start mDNS responder with hostname "esp8266"
    if (MDNS.begin("esp8266")) {
        Serial.println("mDNS responder started: esp8266.local");
    } else {
        Serial.println("Error setting up mDNS!");
    }
}

void loop() {
    MDNS.update(); // Required to keep mDNS running
}
