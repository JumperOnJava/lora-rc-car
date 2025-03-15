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

long current_epoch_millis()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
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
void *rx_f(void *p)
{
    rxData *rx = (rxData *)p;
    printf("\nReceived: %s \n", rx->buf);
    CarMadePacket *packet = (CarMadePacket *)rx->buf;

    if (packet->type == PING_REPLY_TO_STATION)
    {
        lastReceivedPing = current_epoch_millis();
        printf("ping time: %lld\n", lastReceivedPing - startPing);
        startPing = 0;
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

// bool loraModuleConnected()
// {
//     return LoRa_check_conn(&modem);
// }

// void *interruptThread(void *p)
// {
//     printf("receive thread: %d/%d\n", gettid(), getpid());
//     int prevread = 0;
//     gpioSetMode(17, PI_INPUT);
//     while (1)
//     {
//         printf("locked mutex interrupt\n");
//         int nowread = gpioRead(17);
//         // printf("pin17: %d\n", nowread);
//         if (nowread == 1 && prevread == 0)
//         {
//             // printf("dio0 Rising edge on mode %d\n", LoRa_get_op_mode(&modem));
//             // if (LoRa_get_op_mode(&modem) == RXCONT_MODE)
//             // {
//             //     printf("calling receiveDone\n");
//             //     rxDoneISRf(0, 0, 0, &modem);
//             //     printf("cafinished receiveDone\n");
//             //     return;
//             // }
//             // else if (LoRa_get_op_mode(&modem) == TX_MODE || LoRa_get_op_mode(&modem) == STDBY_MODE)
//             // {
//             printf("calling transmitDone\n");
//             txDoneISRf(0, 0, 0, &modem);
//             printf("finished transmitDone\n");
//             // }
//             // LoRa_begin(&modem);
//         }
//         if (nowread == 0 && prevread == 1)
//         {
//             printf("dio0 Falling edge\n");
//         }
//         prevread = nowread;
//     }
//     return NULL;
// }

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

    printf("locked mutex begin\n");

    if (LoRa_begin(&modem) != 0)
    {
        fprintf(stderr, "LoRa initialization failed\n");
        return;
    }
    lora_reset_irq_flags(modem.spid);
    LoRa_receive(&modem);
    printf("unlocked mutex begin\n");

    // pthread_create(&interrupt_thread, NULL, interruptThread, NULL);
    // pthread_detach(interrupt_thread);

    printf("Started lora thread\n");
    while (1)
    {
        if (LoRa_check_conn(&modem))
        {

            printf("locked mutex receive swap\n");
            printf("unlocked mutex receive swap\n");

            if (packetQueue.empty())
            {
                LoRa_receive(&modem);
                int prevread = gpioRead(17);
                while (packetQueue.empty())
                {
                    int nowread = gpioRead(17);
                    if (nowread == 1 && prevread == 0)
                    {
                        printf("calling receiveDone\n");
                        rxDoneISRf(0, 0, 0, &modem);
                        printf("finished receiveDone\n");
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

            printf("locked mutex send\n");
            modem.tx.data.buf = (char *)&item;
            modem.tx.data.size = item.size + 2;
            modem.tx.callback = tx_f;

            LoRa_send(&modem);
            int prevread = gpioRead(17);
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
            }
            printf("waiting finished: %s\n", buffer);
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
    while (true)
    {
        loraThread();
        printf("lora died 💀\n");
        gpioTerminate();
    }
    return 0;

    // printf("receive thread: %d\n",receive_thread);
    // printf("transmit thread: %d\n",transmit_thread);
}