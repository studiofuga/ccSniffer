#include <Arduino.h>
#include "cc1101.h"
//#include <RadioLib.h>
#include "PacketQueue.h"

CC1101Tranceiver radio(10, 3, 2);
volatile bool receivedFlag = false;
volatile bool enableInterrupt = true;
volatile bool transmitting = false;

PacketQueue<4, 64> queue;

void irqRead(void);
void irqSent(void);

volatile int numSent = 0;
volatile int numTimeout = 0;

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
    Serial.print(F("+CC1101 Initializing ... "));

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

    Serial.println(F("+CC1101 Registers dump:"));
    for (int i = 0; i < 0x30; ++i) {
        uint8_t value = radio.SPIreadRegister(i);
        PrintHex8(&value, 1, " ");
        if (i % 8 == 7) {
            Serial.println();
        }
    }
}

void irqSent(void)
{
    ++numSent;
}

void irqRead(void)
{
    size_t retries = 0;
    while(true) {
        if (++retries > 100) {
            // timeout
            ++numTimeout;
            radio.receive();
            return;
        }
        int fifo = radio.SPIgetRegValue(CC1101_REG_RXBYTES, 6, 0);
        if (fifo > 0) break;
    };

    uint8_t str[128];
    int len = radio.read(str, 128);

    if (len > 0) {
            queue.push((char *) str, len);
    }

    receivedFlag = true;
    radio.receive();
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


int cacheNumSent = -1, cachedNumTo = -1;

void loop()
{
    if (cachedNumTo != numTimeout) {
        Serial.println("+CC1101 Timeout");
        cachedNumTo = numTimeout;
    }

    if (receivedFlag) {
        handleReceived();
    }
}


