#include "mongoose.h"
#include "lora-rc-car/LoraData.h"
#include "json.hpp"
#include <string>

#include <pigpio.h>
#include "LoRa.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <pigpio.h>
#include <unistd.h>
#include <map>


using namespace std;
#define RESPONSE "Hello, World!"


#define RESPONSE "Hello, World!"
#define MIME_JSON "application/json"
#define MIME_PLAIN "text/plain"

using namespace std;

void sendLoRaPacket(const struct StationMadePacket *packet);
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

void *receiveThread(void *p);

void tx_f(txData *tx)
{
}
void *rx_f(void *p)
{
    rxData *rx = (rxData *)p;
    printf("\neceived: %s \n", rx->buf);
    free(p);
    return NULL;
}
LoRa_ctl modem;
pthread_mutex_t lora_send_mutex;
map<int, struct CarSensorData> data;
char messageBuf[256];
int main()
{
    modem.spiCS = 0;
    modem.tx.callback = tx_f;
    modem.rx.callback = rx_f;
    modem.eth.preambleLen = 8;
    modem.eth.bw = BW125;             // Bandwidth: 125 kHz
    modem.eth.sf = SF7;               // Spreading Factor: SF7
    modem.eth.CRC = 1;                // Optional CRC enable
    modem.eth.ecr = CR6;              // Error coding rate: CR5
    modem.eth.freq = 433000000;       // Frequency: 433 MHz
    modem.eth.resetGpioN = 4;         // Reset GPIO pin
    modem.eth.dio0GpioN = 17;         // DIO0 GPIO pin for RX/TX done interrupt
    modem.eth.outPower = OP20;        // Output power level
    modem.eth.powerOutPin = PA_BOOST; // Use PA_BOOST for power amplification
    modem.eth.AGC = 1;                // Enable Auto Gain Control
    modem.eth.OCP = 240;              // Over-current protection (max current in mA)
    modem.eth.implicitHeader = 0;     // Use explicit header mode
    modem.eth.syncWord = 0x12;        // Set sync word

    if (LoRa_begin(&modem) != 0)
    {
        fprintf(stderr, "LoRa initialization failed\n");
        return -1;
    }
    lora_reset_irq_flags(modem.spid);
    printf("Started LoRa\n");

    pthread_t transmit_thread;
    pthread_create(&transmit_thread, NULL, receiveThread, NULL);

    LoRa_receive(&modem);

    if(sizeof(CarSensorData) > 200){
        printf("CarSensorData structure is too large, please make sure everything is ok, or carefully increase this limit\n");
        return -1;
    }
    printf("LoraSubServer started\n");
    start_server();
    LoRa_end(&modem);
    return EXIT_SUCCESS;
}
void sendLoRaPacket(const struct StationMadePacket *packet)
{
    if (packet == NULL || packet->data == NULL || packet->size == 0)
    {
        printf("Invalid packet.\n");
        return;
    }

    size_t total_size = 2 + packet->size; // type (1 byte) + size (1 byte) + data
    uint8_t *buffer = (uint8_t *)malloc(total_size);
    if (!buffer)
    {
        printf("Memory allocation failed.\n");
        return;
    }

    buffer[0] = (uint8_t)packet->type;
    buffer[1] = packet->size;
    memcpy(&buffer[2], &(packet->data[0]), packet->size);

    printf("Sending: ");
    for (size_t i = 0; i < total_size; i++)
    {
        printf("%02X ", buffer[i]);
    }
    printf("\n");

    modem.tx.data.buf = (char*) buffer;
    modem.tx.data.size = total_size;
    
    LoRa_send(&modem);
    usleep(100E3);
    LoRa_receive(&modem);

    free(buffer);
}


void *receiveThread(void *p)
{
    int prevread = 0;
    while (1)
    {
        int nowread = gpioRead(17);
        if (nowread == 1 && prevread == 0)
        {
            rxDoneISRf(0, 0, 0, &modem);
        }
    }
    return NULL;
}
