#include <HardwareSerial.h>
#include <TinyGPS++.h>

#define RX_PIN 39  // GPS TX -> ESP32 RX
#define TX_PIN 38  // GPS RX -> ESP32 TX
#define GPS_BAUD 9600

HardwareSerial gpsSerial(1);  // Use UART1 on ESP32
TinyGPSPlus gps;

void setup() {
    Serial.begin(115200);
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RX_PIN, TX_PIN); // Initialize GPS Serial

    Serial.println("GPS Module Test");
}

void loop() {
    while (gpsSerial.available()) {
        char c = gpsSerial.read();
        //Serial.print(c);
        gps.encode(c);
    }

    if (gps.location.isUpdated()) {
        Serial.print("Latitude: ");
        Serial.print(gps.location.lat(), 6);
        Serial.print(" Longitude: ");
        Serial.println(gps.location.lng(), 6);
    }

    if (gps.date.isUpdated()) {
        Serial.print("Date: ");
        Serial.print(gps.date.day());
        Serial.print("/");
        Serial.print(gps.date.month());
        Serial.print("/");
        Serial.println(gps.date.year());
    }

    if (gps.time.isUpdated()) {
        Serial.print("Time: ");
        Serial.print(gps.time.hour());
        Serial.print(":");
        Serial.print(gps.time.minute());
        Serial.print(":");
        Serial.println(gps.time.second());
    }
}
