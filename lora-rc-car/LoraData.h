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

enum SensorDataType{
    GPS = 0,
    HUM_TEMP_CO_CO2 = 1,
};

struct CarMadePacket
{
    enum CarMadePacketType type:8;
    uint8_t size;
    uint8_t* data;
};

struct StationMadePacket{
    enum StationMadePacketType type:8;
    uint8_t size;
    uint8_t* data;
};

struct CarMadeSensorData
{
    enum SensorDataType sensorType:8;
    uint8_t data[64];
};


struct Gps_Data
{
    float lat;
    float lng;
};

struct CarControlData{
    int8_t forward;
    int8_t leftRight;
};