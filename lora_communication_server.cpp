#include "mongoose.h"
#include "lora-rc-car/LoraData.h"
#include "json.hpp"
#include <string>
#include "lora_functions.cpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <pigpio.h>
#include <unistd.h>
#include <map>
#include <queue>

using namespace std;
#define QUEUE_SIZE 100
#define RESPONSE "Hello, World!"
#define MIME_JSON "application/json"
#define MIME_PLAIN "text/plain"

using namespace std;


static void handle_request(struct mg_connection *c, int ev, void *ev_data, void *fn_data);
string handleRequest(string address, json::JSON body, int &status, string &contentType);

void start_server();

string handleRequest(string address, json::JSON body, int &status, string &contentType)
{
  if (address == "/sendControls")
  {
    float forward = body["forward"].ToNumber();
    float leftRight = body["leftRight"].ToNumber();
    printf("f: %.2f; lr: %.2f;\n",forward,leftRight);

    struct StationMadePacket packet;
    packet.type = StationMadePacketType::SEND_CONTROLS;
    packet.size = sizeof(CarControlData);
    struct CarControlData controlData;

    controlData.forward = forward;
    controlData.leftRight = leftRight;
    memcpy(packet.data,&controlData,sizeof(CarControlData));


    sendLoRaPacket(&packet);

    status = 200;
    contentType = MIME_PLAIN;
    return "Ok";
  }
  if (address == "/carAvailable"){
    status = 200;
    contentType = MIME_PLAIN;
    return "Ok";
  }
  if (address == "/loraConnected"){
    status = 200;
    contentType = MIME_PLAIN;
    return "Ok";
  }
  if (address == "/latestData"){
    try{
      body["since"].ToInt();
      status = 200;
      contentType = MIME_PLAIN;
      return "Ok";
    }
    catch(exception e){
      status = 400;
      contentType = MIME_PLAIN;
      return "Error while getting data";
    }
  }
  if (address == "/")
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
    string body(hm->body.buf, hm->body.len);

    int status = 500;
    string contentType = MIME_PLAIN;
    string response;

    try
    {
      json::JSON parsedBody = json::JSON::Load(body);
      response = handleRequest(address, parsedBody, status, contentType);
    }
    catch (const exception &e)
    {
      response = string("Error responding: ") + e.what();
      status = 500;
      contentType = MIME_PLAIN;
    }

    mg_http_reply(c, status, (string("Content-Type: ")+contentType+"\r\n").c_str(),response.c_str());
  }
}

void start_server()
{
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
int main()
{
    
    if(sizeof(CarSensorData) > 200){
        printf("CarSensorData structure is too large, please make sure everything is ok, or carefully increase this limit\n");
        return -1;
    }
    printf("LoraSubServer started\n");
    start_server();
    startLora();
    return EXIT_SUCCESS;
}


