#include "LoRa.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *rx_f(void *p) {
    rxData *rx = (rxData *)p;
    // Display all the data received
    printf("rx done\n");
    printf("CRC error: %d\n", rx->CRC);
    printf("Data size: %d\n", rx->size);
    printf("String: %s\n", rx->buf);
    printf("RSSI: %d\n", rx->RSSI);
    printf("SNR: %f\n", rx->SNR);
    free(p);
    return NULL;
}

int main() {
    LoRa_ctl modem;
    
    // Set Raspberry Pi SPI chip select pin (0 for CE0, for example)
    modem.spiCS = 0;
    
    // Register our callback function to handle incoming packets
    modem.rx.callback = rx_f;
    
    // Configure LoRa module parameters
    modem.eth.preambleLen = 8;
    modem.eth.bw = BW125;             // Bandwidth: 125 kHz
    modem.eth.sf = SF7;               // Spreading Factor: SF7
    modem.eth.CRC = 1;
    modem.eth.ecr = CR5;              // Error coding rate: CR5
    modem.eth.freq = 434000000;       // Frequency: 433 MHz
    modem.eth.resetGpioN = 4;         // Reset GPIO pin
    modem.eth.dio0GpioN = 17;         // DIO0 GPIO pin for RX/TX done interrupt
    modem.eth.outPower = OP20;        // Output power level
    modem.eth.powerOutPin = PA_BOOST; // Use PA_BOOST for power amplification
    modem.eth.AGC = 1;                // Enable Auto Gain Control
    modem.eth.OCP = 240;              // Over-current protection (max current in mA)
    modem.eth.implicitHeader = 0;     // Use explicit header mode
    modem.eth.syncWord = 0x12;        // Set sync word
    
    // Initialize the LoRa module
    LoRa_begin(&modem);
    // Put the module into receive mode
    printf("started receiving!\n");
    LoRa_receive(&modem);
    
    // Loop indefinitely so the program continuously listens for incoming packets.
    // Each received packet will trigger the callback "rx_f".
    while (1) {
        sleep(1);
    }
    
    // Cleanup code (unreachable in this infinite loop, but shown for completeness)
    LoRa_end(&modem);
    return 0;
}
