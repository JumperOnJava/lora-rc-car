#if defined(__x86_64__) || defined(__i386__)
#define WSL 1
#elif defined(__arm__) || defined(__aarch64__)
#define RPI 1
#endif

#ifdef RPI
#include <pigpio.h>
#include "LoRa.h"
#include <pigpio.h>
#else
#include "FakeLora.h"
#endif

#include <time.h>

#include "lora-rc-car/LoraData.h"
#include <string>
#include <deque>

pthread_t interrupt_thread;
pthread_t lora_thread;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t lora_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t lora_cond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t tx_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tx_cond = PTHREAD_COND_INITIALIZER;

std::deque<StationMadePacket> packetQueue;
bool thread_running = false;
bool tx_finished = false;
int packet_sent = 0;

long startPing = 0;
long lastReceivedPing = 0;
bool loraModuleConnected();
StationMadePacket *emtpyPriorityPacket;
StationMadePacket emtpyPriorityPacket_;

long millis()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

void sleepms(int milliseconds)
{
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void enqueueLoRaPacket(const struct StationMadePacket *packet);
void charArrayToHexString(const char *data, size_t length, char *output)
{
    for (size_t i = 0; i < length; i++)
    {
        sprintf(output + (i * 2), "%02X ", (unsigned char)data[i]);
    }
}

void tx_f(txData *tx)
{
    printf("Sent packet\n");
}
CarSensorData latestData;
void *rx_f(void *p)
{
    rxData *rx = (rxData *)p;
    printf("\nReceived: %s \n", rx->buf);
    CarMadePacket *packet = (CarMadePacket *)rx->buf;

    printf("SIZE %d\n", rx->size);
    if (packet->type == PING_REPLY_TO_STATION)
    {
        lastReceivedPing = millis();
        printf("ping time: %lld\n", lastReceivedPing - startPing);
        startPing = 0;
    }
    else if (packet->type == SEND_SENSOR_DATA)
    {
        printf("Received SEND_SENSOR_DATA\n");
        latestData = packet->data.CarSensorData;
        latestData.local_time = (uint64_t)time(NULL);
        printf("local_time!! %lld\n", ((uint64_t)time(NULL)));
        printf("local_time %lld\n", latestData.local_time);
        printf("temperature %f\n", latestData.temperature);
        printf("humidity %f\n", latestData.humidity);
        printf("co %f\n", latestData.co);
        printf("co2 %f\n", latestData.co2);
        printf("nh3 %f\n", latestData.nh3);
        printf("nox %f\n", latestData.nox);
        printf("gasoline %f\n", latestData.gasoline);
        printf("alcohol %f\n", latestData.alcohol);
        printf("s2 %f\n", latestData.s2);
        printf("dust %f\n", latestData.dust);
        printf("gps_lat %f\n", latestData.gps_lat);
        printf("gps_lng %f\n", latestData.gps_lng);
        printf("gps_speed %f\n", latestData.gps_speed);
        printf("gps_time %f\n", latestData.gps_time);
    }

    free(p);
    return NULL;
}

void enqueueLoRaPacket(const struct StationMadePacket *packet)
{
    size_t total_size = 2 + packet->size;

    char hexString[4096];
    charArrayToHexString((char *)packet, total_size, hexString);

    printf("Enqueued packet: %s\n", hexString);
    pthread_mutex_lock(&queue_mutex);
    packetQueue.push_back(*packet);
    pthread_mutex_unlock(&queue_mutex);
}
bool loraConnected = false;
bool loraModuleConnected()
{
    return loraConnected;
}
uint64_t cameraTime = millis();
void loraThread()
{
    LoRa_ctl modem;
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
        return;
    }
    lora_reset_irq_flags(modem.spid);
    LoRa_receive(&modem);

    gpioSetMode(26, PI_OUTPUT);
    printf("Started lora thread\n");
outer:
    while (1)
    {
        bool receiverShouldBeActive = millis() - cameraTime <= 20000;
        gpioWrite(26, receiverShouldBeActive);
        printf("active %d, millis %lld\n", receiverShouldBeActive, millis() - cameraTime);
        loraConnected = LoRa_check_conn(&modem);
        if (loraConnected)
        {
            loraConnected = true;
            if (packetQueue.empty())
            {
                int prevread = gpioRead(17);
                long start;
                start = millis();
                while (1)
                {
                    int nowread = gpioRead(17);
                    if (nowread == 1 && prevread == 0)
                    {
                        printf("calling receiveDone\n");
                        rxDoneISRf(0, 0, 0, &modem);
                        printf("finished receiveDone\n");
                    }
                    long mms = millis() - start;
                    if (mms >= 200)
                    {
                        goto outer;
                    }
                }
            }

            pthread_mutex_lock(&queue_mutex);
            StationMadePacket item = packetQueue.front();
            packetQueue.pop_front();
            pthread_mutex_unlock(&queue_mutex);

            char buffer[1024];
            charArrayToHexString((char *)&item, item.size + 2, buffer);
            printf("Sending packet: %s\n", buffer);

            modem.tx.data.buf = (char *)&item;
            modem.tx.data.size = item.size + 2;
            modem.tx.callback = tx_f;

            LoRa_send(&modem);
            int prevread = gpioRead(17);
            uint64_t start = millis();
            while (1)
            {
                int nowread = gpioRead(17);
                if (nowread == 1 && prevread == 0)
                {
                    printf("calling transmitDone\n");
                    txDoneISRf(0, 0, 0, &modem);
                    printf("finished transmitDone\n");
                    break;
                }
                long mms = millis() - start;
                if (mms >= 5000)
                {
                    if (!packetQueue.empty())
                    {
                        printf("WAIT TIMEOUT FOR PACKET %s", buffer);
                        break;
                    }
                }
            }
            printf("waiting finished: %s\n", buffer);
            LoRa_receive(&modem);
        }
        else
        {
            return;
        }
    }
    return;
}
int startLora()
{

    // pthread_create(&lora_thread, NULL, loraThread, NULL);
    // pthread_detach(lora_thread);
    loraThread();
    printf("lora died 💀\n");
    loraConnected = false;
    gpioTerminate();
    sleep(2);
    return 0;

    // printf("receive thread: %d\n",receive_thread);
    // printf("transmit thread: %d\n",transmit_thread);
}