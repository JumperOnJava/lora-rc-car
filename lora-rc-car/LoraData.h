#pragma once
#include <stdint.h>

struct CarSensorData
{
  float temperature;
  float humidity;

  float co;
  float co2;
  float nh3;
  float nox;
  float gasoline;
  float alcohol;

  float smoke;
  float dust;

  float gps_lat;
  float gps_lng;
  float gps_speed;
  float gps_time;
};

struct CarControlData{
    //from -100 to 100
    int8_t forward;
    //from 0 to 180
    uint8_t leftRight;
};


enum StationMadePacketType{
    PING_TO_CAR = 1,
    SEND_CONTROLS = 2, //sending controls to car
    CAMERA_KEEPALIVE = 3,
};
enum CarMadePacketType{
    PING_REPLY_TO_STATION = 1, 
    SEND_DATA = 2
};

struct CarMadePacket
{
    enum CarMadePacketType type:8;
    uint8_t size;
    union {
        uint8_t raw[256];
        struct CarSensorData CarSensorData;
    } data;
};

struct StationMadePacket{
    enum StationMadePacketType type:8;
    uint8_t size;
    union {
        uint8_t raw[256];
        struct CarControlData CarControlData;
    } data;
};
