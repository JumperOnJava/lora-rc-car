#include "LoRa.h"
#include "lora-rc-car/LoraData.h"
#include <string>
#include <queue>
#include <pigpio.h>

void sendLoRaPacket(const struct StationMadePacket *packet);

typedef struct
{
    uint8_t *data;
    size_t size;
} LoRaQueueItem;

pthread_t send_thread;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
std::queue<LoRaQueueItem> packet_queue;
bool thread_running = false;

LoRa_ctl modem;


void *send_thread_func(void *arg)
{
  while (1)
  {
      pthread_mutex_lock(&queue_mutex);
      while (packet_queue.empty())
      {
          pthread_cond_wait(&queue_cond, &queue_mutex);
      }

      LoRaQueueItem item = packet_queue.front();
      packet_queue.pop();
      pthread_mutex_unlock(&queue_mutex);

      printf("Sending: ");
      for (size_t i = 0; i < item.size; i++)
      {
          printf("%02X ", item.data[i]);
      }
      printf("\n");

      modem.tx.data.buf = (char*) item.data;
      modem.tx.data.size = item.size;

      printf("sending/");
      LoRa_send(&modem);
      printf("sent/");
      usleep(100E3);
      printf("sentw/");
      LoRa_receive(&modem);
      printf("receiving/");

      free(item.data);
  }
  return NULL;
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


int startLora()
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
    return 0;
}

void sendLoRaPacket(const struct StationMadePacket *packet)
{
    if (packet == NULL || packet->data == NULL || packet->size == 0)
    {
        printf("Invalid packet.\n");
        return;
    }

    size_t total_size = 2 + packet->size;
    uint8_t *buffer = (uint8_t *)malloc(total_size);
    if (!buffer)
    {
        printf("Memory allocation failed.\n");
        return;
    }

    buffer[0] = (uint8_t)packet->type;
    buffer[1] = packet->size;
    memcpy(&buffer[2], &(packet->data[0]), packet->size);

    pthread_mutex_lock(&queue_mutex);
    packet_queue.push({buffer, total_size});
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);

    if (!thread_running)
    {
        thread_running = true;
        pthread_create(&send_thread, NULL, send_thread_func, NULL);
    }
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