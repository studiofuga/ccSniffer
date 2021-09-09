#include <Arduino.h>
#include "cc1101.h"
//#include <RadioLib.h>
#include "PacketQueue.h"

CC1101Tranceiver radio(10, 3, 2);
volatile bool receivedFlag = false;
volatile bool enableInterrupt = true;
volatile bool transmitting = false;

bool machineReadbale = false;

PacketQueue<4, 64> queue;

void irqDetect(void);
void irqRead(void);
void irqSent(void);

volatile int numTo = 0;
volatile int numIrq = 0;
volatile int numSent = 0;
volatile int numFailedIrq = 0;
volatile int numPacket = 0;
//int lastState = ERR_NONE;

void PrintHex8(uint8_t *data, uint8_t length, char const *separator) // prints 8-bit data in hex with leading zeroes
{
    for (int i = 0; i < length; i++) {
        if (data[i] < 0x10) { Serial.print("0"); }
        Serial.print(data[i], HEX);

        if (separator != nullptr) {
            Serial.print(" ");
        }
    }
}

void setup()
{
    Serial.begin(38400);

    // initialize CC1101 with default settings
    if (!machineReadbale) {
        Serial.print(F("[CC1101] Initializing ... "));
    }
/*
    int state = radio.begin(868.3, 38.383, 20.63, 101.56, 10, 32);
    if (state == ERR_NONE) {
        if (!machineReadbale) {
            Serial.println(F("success!"));
        }
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true) {}
    }
*/
    auto state = radio.initialize();
    if (state == 0) {
        Serial.println(F("success!"));
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true) {}
    }

    auto v = radio.getChipVersion();
    Serial.print("Chip version: ");
    Serial.println(v);

    radio.setFrequency(868.3);
    radio.setBitrate(38.383);
    radio.setDeviation(20.63);
    radio.setReceiverBW(101.56);
    radio.setOutputPower(10);

    radio.setModulation(CC1101Tranceiver::Modulation::GFSK);
    radio.setSyncType(CC1101Tranceiver::SyncType::Sync30_32);
    radio.setPreambleLength(CC1101Tranceiver::PreambleTypes::Bytes4);
    radio.setSyncWord(0x2d, 0xc5);
    radio.enableCRC();
    radio.enableWhitening();
    radio.SPIsetRegValue(CC1101_REG_FSCTRL1, 0x06, 4, 0);
    radio.SPIsetRegValue(CC1101_REG_FSCTRL0, 0x05, 7, 0);

    radio.setReceiveHandler(irqRead, CC1101Tranceiver::SignalDirection::Rising);
    radio.setTransmitHandler(irqSent, CC1101Tranceiver::SignalDirection::Rising);

    delay(500);
    radio.receive();

    if (!machineReadbale) {
        Serial.println(F("[CC1101] Registers dump:"));
        for (int i = 0; i < 0x30; ++i) {
            uint8_t value = radio.SPIreadRegister(i);
            PrintHex8(&value, 1, " ");
            if (i % 8 == 7) {
                Serial.println();
            }
        }
    }
}

void irqSent(void)
{
    ++numSent;
}

void irqDetect(void)
{
    ++numIrq;
    receivedFlag=true;
}

void irqRead(void)
{
    ++numIrq;
    if (!enableInterrupt || transmitting) {
        ++numFailedIrq;
//        radio.receive();
        return;
    }

    size_t retries = 0;
    while(true) {
        if (++retries > 100) {
            // timeout
            ++numTo;
//            radio.receive();
            return;
        }
        int fifo = radio.SPIgetRegValue(CC1101_REG_RXBYTES, 6, 0);
        if (fifo > 0) break;
    };

    uint8_t str[128];
    int len = radio.read(str, 128);

    if (len > 0) {
//        if (state == ERR_NONE || state == ERR_CRC_MISMATCH) {
            ++numPacket;
            queue.push((char *) str, len);
//        }
    }
    receivedFlag = true;
//    radio.receive();
}

/*
void transmitHex(char *text)
{
    transmitting = true;
    radio.clearGdo0Action();

    Serial.println(F("Sending..."));
    uint8_t data[] = {0x00, 0x20, 0x00, 0x01, 0x41, 0x42, 0x43, 0x44};
    size_t datalen = 8;
    radio.transmit(data, datalen);

    radio.startReceive();
    transmitting = false;
}

*/

void handleReceived()
{
    enableInterrupt = false;
    receivedFlag = false;

    if (!queue.empty()) {
        uint8_t buf[128];
        uint8_t len = queue.pop((char *) buf);

        Serial.print(F("*"));
        Serial.print(millis());
        Serial.print(F(","));
        for (auto i = 0; i < len; ++i) {
            PrintHex8(&buf[i], 1, nullptr);
        }

/*
        if (state == ERR_CRC_MISMATCH) {
            Serial.print(",BADCRC");
        }
*/
        Serial.println();
    }
    enableInterrupt = true;
}


int cacheNumIrq = -1, cacheNumFailed = -1, cachedNumPacket = -1, cacheNumSent = -1, cachedNumTo = -1;

unsigned long oldTime  = 0;

void loop()
{
    unsigned long time = millis();
    if (time - oldTime >= 1000) {
        Serial.print("Tick: ");
        Serial.println(time/1000);
        oldTime= time;
    }

    if (cacheNumIrq != numIrq ||
    cacheNumFailed != numFailedIrq ||
    cachedNumPacket != numPacket ||
    cacheNumSent != numSent ||
    cachedNumTo != numTo) {
        Serial.print(numIrq);
        Serial.print(" ");
        Serial.print(numFailedIrq);
        Serial.print(" ");
        Serial.print(numPacket);
        Serial.print(" ");
        Serial.print(numSent);
        Serial.print(" ");
        Serial.print(numTo);
        Serial.println();
        cacheNumIrq = numIrq ;
        cacheNumFailed = numFailedIrq;
        cachedNumPacket = numPacket;
        cacheNumSent= numSent;
        cachedNumTo = numTo;
    }

#if 1
    if (receivedFlag) {
        Serial.println("*");
        handleReceived();
    }
#else
    // check if the flag is set
    if(receivedFlag) {
        // disable the interrupt service routine while
        // processing the data
        enableInterrupt = false;

        // reset flag
        receivedFlag = false;

        auto now = millis();
        bool fifoReady = false;
        do {
            if ((millis() - now) > 20) {
                // timeout
                ++numTo;
                Serial.println("TIMEOUT");
                radio.receive();
                return;
            }
            int fifo = radio.SPIgetRegValue(CC1101_REG_RXBYTES, 6, 0);
            fifoReady = (fifo > 0);
        } while (fifoReady);

        uint8_t str[128];
        int len = radio.read(str, 128);

        if (!machineReadbale)
            Serial.println(F("[CC1101] Received packet!"));

        // print data of the packet
        if (!machineReadbale)
            Serial.print(F("[CC1101] Data:\t\t"));
        Serial.println(len);
        for (auto i = 0; i < len; ++i){
            PrintHex8(&str[i],1, (machineReadbale ? nullptr : " "));
            if (!machineReadbale && (i % 16 )== 15) {
                Serial.println();
            }
        }
        Serial.println();

        // print RSSI (Received Signal Strength Indicator)
        // of the last received packet
        if (!machineReadbale) {
            Serial.print(F("[CC1101] RSSI:\t\t"));
//            Serial.print(radio.getRSSI());
            Serial.println(F(" dBm"));

            // print LQI (Link Quality Indicator)
            // of the last received packet, lower is better
            Serial.println(F("[CC1101] LQI:\t\t"));
//            Serial.println(radio.getLQI());

        }

        // put module back to listen mode
        enableInterrupt = true;
        radio.receive();

        // we're ready to receive more packets,
        // enable interrupt service routine
    }
#endif

}


