#include "lora_functions.cpp"
#include "mongoose.h"
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include <map>
#include <string>
#include "json.hpp"
#include <fstream>
#include <math.h>
#include <vector>

#define QUEUE_SIZE 100
#define RESPONSE "Hello, World!"
#define MIME_JSON "application/json"
#define MIME_HTML "text/html"
#define MIME_PLAIN "text/plain"

using namespace std;

string ERR_JSON_TEXT = "~~err";
auto ERR_JSON = json::JSON::Load("[\""+ERR_JSON_TEXT+"\"]");

static void handle_request(struct mg_connection *c, int ev, void *ev_data, void *fn_data);
string handleRequest(string address, json::JSON body, int &status, string &contentType);

void start_server();

string WEB_PREFIX = string("/web/");

string handleRequest(string address, json::JSON body, int &status, string &contentType)
{
  if (address == "/api/carAvailable")
  {
    status = 200;
    contentType = MIME_PLAIN;
    return "Ok";
  }
  if (address == "/api/loraConnected")
  {
    status = 200;
    contentType = MIME_PLAIN;
    bool connected = loraModuleConnected();
    printf( connected ? "lora true\n" : "lora false\n");
    return connected ? "true" : "false";
  }
  if(address == "/api/ping"){
    struct StationMadePacket packet;
    packet.type = PING_TO_CAR;
    packet.size = 0;
    startPing = millis();
    enqueueLoRaPacket(&packet);
    
    status = 200;
    contentType = MIME_PLAIN;
    return "pinged, check result in console";
  }
  if(address == "/api/cameraKeepAlive"){
    struct StationMadePacket packet;
    packet.type = CAMERA_KEEPALIVE;
    packet.size = 0;
    enqueueLoRaPacket(&packet);
    printf("cameraKeepAlivePacket\n");

    cameraTime = millis();

    status = 200;
    contentType = MIME_PLAIN;
    return "done";
  }
  if(address == "/api/queue"){
    json::JSON queueDataJson = json::Object();
    queueDataJson["length"] = packetQueue.size();
    vector<json::JSON> json;
    #define queueCopy packetQueue
    
    queueDataJson["data"] = json::Array();
    
    status = 200;
    contentType = MIME_JSON;
    return queueDataJson.dump();
  }


  if(address == "/api/sensorData"){
    json::JSON responseJson = json::Object();
    CarSensorData sensorData = latestData;

    responseJson["local_time"] = sensorData.local_time;
    responseJson["temperature"] = sensorData.temperature;
    responseJson["humidity"] = sensorData.humidity;
    responseJson["co"] = sensorData.co;
    responseJson["co2"] = sensorData.co2;
    responseJson["nh3"] = sensorData.nh3;
    responseJson["nox"] = sensorData.nox;
    responseJson["gasoline"] = sensorData.gasoline;
    responseJson["alcohol"] = sensorData.alcohol;
    responseJson["s2"] = sensorData.s2;
    responseJson["dust"] = sensorData.dust;
    responseJson["gps_lat"] = sensorData.gps_lat;
    responseJson["gps_lng"] = sensorData.gps_lng;
    responseJson["gps_speed"] = sensorData.gps_speed;
    responseJson["gps_time"] = sensorData.gps_time;

    status = 200;
    contentType = MIME_JSON;
    return responseJson.dump();
  }

  #define DEFINE_JSON if(false){status=400;contentType=MIME_HTML;return "json body empty";}json::JSON json = body;

  if (address == "/api/sendControls")
  {
    DEFINE_JSON
    int forward = json["forward"].ToInt();
    int leftRight = json["leftRight"].ToInt();
    printf("f: %d; lr: %d;\n", forward, leftRight);

    forward = max(-100,min(100,forward));
    leftRight = max(0,min(180,leftRight));

    struct StationMadePacket packet;
    packet.type = SEND_CONTROLS;
    packet.size = sizeof(CarControlData);
    packet.data.CarControlData.forward = forward;
    packet.data.CarControlData.leftRight = leftRight;

    if(packetQueue.empty()){
      enqueueLoRaPacket(&packet);
    }


    status = 200;
    contentType = MIME_JSON;
    return json.dump();
  }
  if (address == "/api/latestData")
  {
    try
    {
      DEFINE_JSON
      status = 200;
      contentType = MIME_PLAIN;
      return "Ok";
    }
    catch (exception e)
    {
      status = 400;
      contentType = MIME_PLAIN;
      return "Error while getting data";
    }
  }

  #undef DEFINE_JSON

  if (address == "/")
  {
  }
  status = 404;
  contentType = MIME_PLAIN;
  return "Wrong endpoint";
}

static void handle_request(struct mg_connection *c, int ev, void *ev_data)
{
  if (ev == MG_EV_HTTP_MSG)
  {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;

    string address(hm->uri.buf, hm->uri.len);
    string body(hm->body.buf);

    int status = 500;
    string contentType = MIME_PLAIN;
    string response;

    try
    {
      json::JSON parsedBody;
      if(body.length()>0)
      {
        parsedBody = json::JSON::Load(hm->body.buf);
      }
      else
      {
        parsedBody = ERR_JSON;
      }
      response = handleRequest(address, parsedBody, status, contentType);
    }
    catch (const exception &e)
    {
      response = string("Error responding: ") + e.what();
      status = 500;
      contentType = MIME_PLAIN;
    }

    mg_http_reply(c, status, (string("Content-Type: ") + contentType + "\r\n").c_str(), response.c_str());
  }
}

void* start_server(void *p)
{
  printf("http thread: %d/%d\n",gettid(),getpid());
  struct mg_mgr mgr;
  mg_mgr_init(&mgr);
  mg_http_listen(&mgr, "http://0.0.0.0:8008", handle_request, NULL);
  while (true)
  {
    mg_mgr_poll(&mgr, 1000);
  }
  mg_mgr_free(&mgr);
}

map<int, struct CarSensorData> data;
char messageBuf[256];

pthread_t http_thread;
int main()
{
  printf("LoraSubServer started\n");
  pthread_create(&http_thread, NULL, start_server, NULL);
  pthread_detach(http_thread);
  startLora();
  return EXIT_SUCCESS;
}

