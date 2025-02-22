#include "LoRa.h"

void tx_f(txData *tx){
    printf("tx done \n");
}

int main(){

    char txbuf[255];
    LoRa_ctl modem;

    //See for typedefs, enumerations and there values in LoRa.h header file
    modem.spiCS = 0;//Raspberry SPI CE pin number
    modem.tx.callback = tx_f;
    modem.tx.data.buf = txbuf;
    memcpy(modem.tx.data.buf, "LoRa", 5);//copy data we'll sent to buffer
    modem.tx.data.size = 5;//Payload len
    modem.eth.preambleLen = 8;
    modem.eth.bw = BW125;             // Bandwidth: 125 kHz
    modem.eth.sf = SF7;               // Spreading Factor: SF7
    // modem.eth.CRC = 1;
    // modem.eth.ecr = CR5;              // Error coding rate: CR5
    modem.eth.freq = 433000000;       // Frequency: 433 MHz
    modem.eth.resetGpioN = 4;         // Reset GPIO pin
    modem.eth.dio0GpioN = 17;         // DIO0 GPIO pin for RX/TX done interrupt
    modem.eth.outPower = OP20;        // Output power level
    modem.eth.powerOutPin = PA_BOOST; // Use PA_BOOST for power amplification
    modem.eth.AGC = 1;                // Enable Auto Gain Control
    modem.eth.OCP = 240;              // Over-current protection (max current in mA)
    modem.eth.implicitHeader = 0;     // Use explicit header mode
    modem.eth.syncWord = 0x12;        // Set sync word
    //For detail information about SF, Error Coding Rate, Explicit header, Bandwidth, AGC, Over current protection and other features refer to sx127x datasheet https://www.semtech.com/uploads/documents/DS_SX1276-7-8-9_W_APP_V5.pdf

    LoRa_begin(&modem);
    LoRa_send(&modem);

    printf("Tsym: %f\n", modem.tx.data.Tsym);
    printf("Tpkt: %f\n", modem.tx.data.Tpkt);
    printf("payloadSymbNb: %u\n", modem.tx.data.payloadSymbNb);

    printf("sleep %d seconds to transmitt complete\n", (int)modem.tx.data.Tpkt/1000);
    sleep(((int)modem.tx.data.Tpkt/1000)+1);

    printf("end\n");

    LoRa_end(&modem);
}
