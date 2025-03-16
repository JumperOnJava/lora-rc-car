
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <MQ135.h>  // Додавання бібліотеки для MQ-135
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

#define LORA

#ifdef LORA
//{
#include <LoRa.h>
#include "LoraData.h"
#define HSPI_MISO 12
#define HSPI_MOSI 13
#define HSPI_SCK 14
#define SS_PIN 15
#define RST_PIN 17
#define DIO0_PIN 18
SPIClass hspi(HSPI);
void charArrayToHexString(const char *data, size_t length, char *output) {
  for (size_t i = 0; i < length; i++) {
    sprintf(output + (i * 2), "%02X", (unsigned char)data[i]);
  }
}
//}
#endif

const char *ssid = "esp-rc";
const char *password = "12345678";

WebServer server(80);
Servo myServo;

#define PIN_CAMERA 35   // Пін для камери
#define PIN_SERVO 2     // Пін для сервоприводу
#define PIN_FORWARD 6   // Пін для руху вперед
#define PIN_BACKWARD 7  // Пін для руху назад

#define SENSOR_PIN 10  // Пін для MQ-135

// Піни для датчика пилу
#define PIN_AOUT 1     // Пін для AOUT датчика пилу
#define PIN_IR_LED 41  // Пін для IR-LED датчика пилу

// Налаштування датчиків
Adafruit_SHT31 sht31 = Adafruit_SHT31();
MQ135 gasSensor(SENSOR_PIN);  // Ініціалізація MQ-135

// Налаштування змінних для датчика пилу
float zeroSensorDustDensity = 0.6;
int sensorADC;
float sensorVoltage;
float sensorDustDensity;

// Налаштування GPS

#define RX_PIN 39  // Підключити TX GPS до 4-го піну ESP32
#define TX_PIN 38  // Підключити RX GPS до 3-го піну ESP32
#define GPS_BAUD 9600

SoftwareSerial gpsSerial(RX_PIN, TX_PIN);  // Ініціалізація серійного з'єднання з GPS
TinyGPSPlus gps;

unsigned long lastSensorRead = 0;

void setup() {
  Serial.begin(115200);

  // Ініціалізація I²C для SHT31
  Wire.begin(37, 36);  // SDA на GPIO 16, SCL на GPIO 15
  if (!sht31.begin(0x44)) {
    Serial.println("Не найден SHT31!");
    while (1)
      delay(10);
  }
  Serial.println("Датчик SHT31 подключен.");

  // Налаштування Wi-Fi точки доступу
  WiFi.softAP(ssid, password);
  Serial.println("WiFi AP запущено");

  // Налаштування сервоприводу
  myServo.attach(PIN_SERVO);
  myServo.write(90);  // Початкова позиція

  pinMode(PIN_CAMERA, OUTPUT);

  // Налаштування моторних пінів
  pinMode(PIN_FORWARD, OUTPUT);
  pinMode(PIN_BACKWARD, OUTPUT);
  digitalWrite(PIN_FORWARD, LOW);
  digitalWrite(PIN_BACKWARD, LOW);

  // Налаштування пінів для датчика пилу
  pinMode(PIN_IR_LED, OUTPUT);
  digitalWrite(PIN_IR_LED, LOW);  // Вимкнути IR LED при запуску

  // Налаштування GPS
  gpsSerial.begin(GPS_BAUD);
  Serial.println("GPS модуль готовий!");

  // Налаштування веб-сервера
  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.on("/sensors", handleSensors);
  server.begin();
  Serial.println("Сервер запущено!");

#ifdef LORA
  //{
  hspi.begin(HSPI_SCK, HSPI_MISO, HSPI_MOSI, SS_PIN);
  LoRa.setSPI(hspi);
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  LoRa.enableCrc();
  LoRa.setCodingRate4(6);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa module not connected.");
    while (1) {
      Serial.println("LoRa module not connected.");
      delay(1000);
    }
  }
  Serial.println("LoRa NodeESP - Ready");
  //}
#endif
}

#ifdef LORA
//{
long cameraCommandTime = millis();
long latestMoveCommandTime = millis();
float latestMoveCommandForward = 0;
float latestMoveCommandLeftRight = 0;
//}
#endif

void loop() {
  server.handleClient();

  digitalWrite(PIN_CAMERA, millis() - cameraCommandTime < 20000);
  if (millis() - lastSensorRead > 2000) {
    lastSensorRead = millis();

    CarSensorData sensorData;
    // Считывание с датчиков SHT31
    float temp = sht31.readTemperature();
    sensorData.temperature = temp;

    float hum = sht31.readHumidity();
    sensorData.humidity = hum;

    if (!isnan(temp) && !isnan(hum)) {
      Serial.print("Температура: ");
      Serial.print(temp);
      Serial.println(" °C");
      Serial.print("Вологість: ");
      Serial.print(hum);
      Serial.println(" %");
    } else {
      Serial.println("Ошибка чтения с датчика!");
    }

    // Считывание с датчика пилу
    sensorADC = 0;  // Обнуляємо перед вимірюванням
    for (int i = 0; i < 10; i++) {
      digitalWrite(PIN_IR_LED, HIGH);  // Включаємо IR LED
      delayMicroseconds(280);          // Час для зчитування
      sensorADC += analogRead(PIN_AOUT);
      digitalWrite(PIN_IR_LED, LOW);  // Вимикаємо IR LED
      delay(10);
    }
    sensorADC = sensorADC / 3;  // Усереднюємо значення

    // Перетворюємо значення з АЦП у напругу
    sensorVoltage = (3.3 / 1024.0) * sensorADC * 11;

    // Перетворюємо напругу в концентрацію пилу
    if (sensorVoltage < zeroSensorDustDensity) {
      sensorDustDensity = 0;
    } else {
      sensorDustDensity = 0.17 * sensorVoltage - 0.1;
    }

    Serial.print("Пил в повітрі: ");
    Serial.print(sensorDustDensity);
    Serial.println(" ug/m3");
    sensorData.dust = sensorDustDensity;

    // Оновлення GPS даних
    while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
    }

    sensorData.gps_lat = gps.location.lat();
    sensorData.gps_lng = gps.location.lng();
    sensorData.gps_speed = gps.speed.kmph();

    //примітка 1
    //я поняття не маю чому це зроблено так, цей код писав не я
    //моє діло його відправити лорою а що ці дані значать я хз
    //  - Сливік
    float ppmCO2 = gasSensor.getPPM();
    Serial.println(String("ppm: ") + gasSensor.getPPM());

    sensorData.co2 = ppmCO2;
    float ppmCO = ppmCO2 * 0.6;
    sensorData.co = ppmCO;
    float ppmBenzene = ppmCO2 * 0.05;
    sensorData.gasoline = ppmBenzene;
    float ppmAlcohol = ppmCO2 * 0.2;
    sensorData.alcohol = ppmAlcohol;
    float ppmAmmonia = ppmCO2 * 0.3;
    sensorData.nh3 = ppmAmmonia;
    float ppmNOx = ppmCO2 * 0.15;
    sensorData.nox = ppmNOx;
    float ppmSulphide = ppmCO2 * 0.1;
    sensorData.s2 = ppmSulphide;

    LoRa.beginPacket();
    CarMadePacket pingResponse;
    pingResponse.type = SEND_SENSOR_DATA;
    pingResponse.size = sizeof(CarSensorData);
    pingResponse.data.CarSensorData = sensorData;
    LoRa.write((uint8_t *)&pingResponse, 2 + sizeof(CarSensorData));
    LoRa.endPacket(false);
  }

#ifdef LORA
  //{
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String data = "";
    while (LoRa.available()) {
      data += (char)LoRa.read();
    }
    Serial.println("Received: " + data);

    char hexString[4096];
    charArrayToHexString(data.c_str(), data.length(), hexString);
    Serial.println("As hex: " + String(hexString));

    struct StationMadePacket *packet;
    packet = (StationMadePacket *)data.c_str();
    if (packet->type == SEND_CONTROLS) {
      struct CarControlData controls = packet->data.CarControlData;
      Serial.println("Forward: " + String(controls.forward) + " LeftRight: " + String(controls.leftRight));
      latestMoveCommandTime = millis();
      latestMoveCommandForward = controls.forward;
      latestMoveCommandLeftRight = controls.leftRight;
    }
    if (packet->type == PING_TO_CAR) {
      delay(20);
      LoRa.beginPacket();
      CarMadePacket pingResponse;
      pingResponse.type = PING_REPLY_TO_STATION;
      pingResponse.size = 0;
      LoRa.write((uint8_t *)&pingResponse, 2);
      LoRa.endPacket(false);
      Serial.println("Ping response!");
    }
    if (packet->type == CAMERA_KEEPALIVE) {
      cameraCommandTime = millis();
    }
  }
  if (millis() - latestMoveCommandTime < 500) {
    myServo.write(latestMoveCommandLeftRight);
    if (latestMoveCommandForward > 0) {
      digitalWrite(PIN_FORWARD, HIGH);
      digitalWrite(PIN_BACKWARD, LOW);
    } else if (latestMoveCommandForward < 0) {
      digitalWrite(PIN_FORWARD, LOW);
      digitalWrite(PIN_BACKWARD, HIGH);
    } else if (latestMoveCommandForward == 0) {
      digitalWrite(PIN_FORWARD, LOW);
      digitalWrite(PIN_BACKWARD, LOW);
    }
  } else {
    digitalWrite(PIN_FORWARD, LOW);
    digitalWrite(PIN_BACKWARD, LOW);
  }
//}
#endif LORA
}

// Обробник для веб-запиту керування (сервопривід і мотор)
void handleControl() {
  // Керування сервоприводом через слайдер
  if (server.hasArg("servo")) {
    int angle = server.arg("servo").toInt();
    if (angle >= 0 && angle <= 180) {
      //myServo.write(angle);
      latestMoveCommandTime = millis();
      latestMoveCommandLeftRight = angle;
    }
  }
  // Керування мотором через кнопки
  if (server.hasArg("motor")) {
    String command = server.arg("motor");
    command.trim();  // Видаляємо зайві пробіли
    Serial.println("Received motor command: " + command);
    latestMoveCommandTime = millis();
    if (command.equals("forward")) {
      latestMoveCommandForward = 1;
    } else if (command.equals("backward")) {
      latestMoveCommandForward = -1;
    } else if (command.equals("stop")) {
      latestMoveCommandForward = 0;
    }
  }
  server.send(200, "text/plain", "OK");
}

// Обробник для веб-запиту зчитування датчиків SHT31 та MQ-135
void handleSensors() {
  float temp = sht31.readTemperature();
  float hum = sht31.readHumidity();

  // Читання значень з MQ-135
  // ctrl + f > примітка 1
  float ppmCO2 = gasSensor.getPPM();
  float ppmCO = ppmCO2 * 0.6;
  float ppmBenzene = ppmCO2 * 0.05;
  float ppmAlcohol = ppmCO2 * 0.2;
  float ppmAmmonia = ppmCO2 * 0.3;
  float ppmNOx = ppmCO2 * 0.15;
  float ppmSulphide = ppmCO2 * 0.1;

  // Читання GPS координат
  String gpsData = "{\"latitude\":" + String(gps.location.lat(), 6) + ",\"longitude\":" + String(gps.location.lng(), 6) + ",\"speed\":" + String(gps.speed.kmph()) + "}";

  if (isnan(temp) || isnan(hum)) {
    server.send(500, "application/json", "{\"error\":\"Ошибка чтения с датчика\"}");
  } else {
    String json = "{\"temperature\":" + String(temp, 2) + ",\"humidity\":" + String(hum, 2) + ",\"CO2\":" + String(ppmCO2, 2) + ",\"CO\":" + String(ppmCO, 2) + ",\"Benzene\":" + String(ppmBenzene, 2) + ",\"Alcohol\":" + String(ppmAlcohol, 2) + ",\"Ammonia\":" + String(ppmAmmonia, 2) + ",\"NOx\":" + String(ppmNOx, 2) + ",\"Sulphide\":" + String(ppmSulphide, 2) + ",\"dust\":" + String(sensorDustDensity, 2) + ",\"gps\":" + gpsData + "}";
    server.send(200, "application/json", json);
  }
}

// Головна веб-сторінка з HTML-кодом

void handleRoot() {
  String page =
#include "page.html"
    ;

  server.send(200, "text/html", page);
}
