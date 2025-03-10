#include "LoRa.h"
#include "lora-rc-car/LoraData.h"
#include <string>
#include <deque>
#include <pigpio.h>

void sendLoRaPacket(const struct StationMadePacket *packet);
void charArrayToHexString(const char *data, size_t length, char *output)
{
    for (size_t i = 0; i < length; i++)
    {
        sprintf(output + (i * 2), "%02X ", (unsigned char)data[i]);
    }
}

pthread_t send_thread;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
std::deque<StationMadePacket> packetQueue;
bool thread_running = false;

LoRa_ctl modem;
bool loraModuleConnected();
StationMadePacket *emtpyPriorityPacket;
StationMadePacket emtpyPriorityPacket_;
void setEmptyPriorityPacket(StationMadePacket packet)
{
    if (packetQueue.empty())
    {
        packetQueue.push_back(packet);
        pthread_cond_signal(&queue_cond);
        emtpyPriorityPacket = nullptr;
    }
    else
    {
        emtpyPriorityPacket_ = packet;
        emtpyPriorityPacket = &emtpyPriorityPacket_;
    }
}

void *send_thread_func(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&queue_mutex);
        while (packetQueue.empty())
        {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }

        StationMadePacket item = packetQueue.front();

        if (LoRa_check_conn(&modem))
        {
            pthread_mutex_unlock(&queue_mutex);

            char buffer[1024];
            charArrayToHexString((char *)&item, item.size + 2, buffer);
            printf("Sending packet: %s\n", buffer);

            modem.tx.data.buf = (char *)&item;
            modem.tx.data.size = item.size + 2;

            LoRa_send(&modem);
            printf("Sent packet: %s\n", buffer);
            usleep(100e3);
            LoRa_receive(&modem);
        }
        packetQueue.pop_front();
    }
    return NULL;
}

void *receiveThread(void *p);

void tx_f(txData *tx)
{
}

#include <time.h>

long startPing = 0;
long lastReceivedPing = 0;

long current_epoch_millis()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

void *rx_f(void *p)
{
    rxData *rx = (rxData *)p;
    printf("\neceived: %s \n", rx->buf);
    CarMadePacket *packet = (CarMadePacket *)rx->buf;

    if (packet->type == PING_REPLY_TO_STATION)
    {
        lastReceivedPing = current_epoch_millis();
        printf("ping time: %d", lastReceivedPing - startPing);
        startPing = 0;
    }

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
    size_t total_size = 2 + packet->size;

    char hexString[4096];
    charArrayToHexString((char *)packet, total_size, hexString);
    if(!loraModuleConnected())
    {
        printf("%s | Packet discarded because lora module is disconnected\n",hexString);
        return;
    }
    
    printf("Enqueued packet: %s\n", hexString);
    pthread_mutex_lock(&queue_mutex);
    packetQueue.push_back(*packet);
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);

    if (!thread_running)
    {
        thread_running = true;
        pthread_create(&send_thread, NULL, send_thread_func, NULL);
    }
}

bool loraModuleConnected()
{
    return LoRa_check_conn(&modem);
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