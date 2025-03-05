#pragma once
#include <stdint.h>

enum StationMadePacketType{
    CAR_PLS_RESPOND = 0, //sent once a second when car didn't sent any packets for some time
    EMPTY = 1, //sent when 
    SEND_CONTROLS = 2 //sending controls to car
};
enum CarMadePacketType{
    STATION_PLS_RESPOND = 0, //sent once a second when car did't received any packets for some time
    CAR_FOUND_RESPONSE = 1,
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
    int8_t forward;
    int8_t leftRight;
};
