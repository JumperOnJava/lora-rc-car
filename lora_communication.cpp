#define _Bool bool
#include "LoRa.h"
#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "pigpio.h"



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
    modem.eth.freq = 433E6;       // Frequency: 433 MHz
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
        return EXIT_FAILURE;
    }
    lora_reset_irq_flags(modem.spid);
    printf("Started LoRa\n");

    pthread_t transmit_thread;
    pthread_create(&transmit_thread, NULL, receiveThread, NULL);

    LoRa_receive(&modem);
    int prevread = 0;
    while (1)
    {
        fgets(messageBuf, sizeof(messageBuf), stdin);
	messageBuf[strcspn(messageBuf, "\n")] = 0;

        if(strcmp(messageBuf,"exit")==0){
            break;
        }
        modem.tx.data.buf = messageBuf;
        int len = strlen(messageBuf);
        printf("Sending: %s\n", messageBuf);
        memcpy(modem.tx.data.buf, messageBuf, len);
        modem.tx.data.size = len;
        LoRa_send(&modem);
        usleep(10E3);
        LoRa_receive(&modem);
    }
    LoRa_end(&modem);
    return EXIT_SUCCESS;
}

void *receiveThread(void *p)
{
    int counter = 0;
    int prevread = 0;
    while (1)
    {
        if(counter++ == 1000000){
            lora_get_rssi_cur(&modem);
            printf("connected: %d %d\n",LoRa_check_conn(&modem),modem.eth.curRSSI);
            counter = 0;
        }
        int nowread = gpioRead(17);
        if (nowread == 1 && prevread == 0)
        {
            rxDoneISRf(0, 0, 0, &modem);
        }
    }
    return NULL;
}
