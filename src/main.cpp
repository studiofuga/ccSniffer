#include <Arduino.h>
#include <RadioLib.h>
#include "PacketQueue.h"

CC1101 radio = new Module(10, 3, RADIOLIB_NC, 2);
volatile bool receivedFlag = false;
volatile bool enableInterrupt = true;
volatile bool transmitting = false;

bool machineReadbale = false;

PacketQueue<4, 64> queue;

void irqRead(void);

int numIrq = 0;
int numFailedIrq = 0;
int numPacket = 0;
int lastState = ERR_NONE;

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

    radio.setPreambleLength(32);
    radio.setSyncWord(0x2d, 0xc5);
    radio.enableSyncWordFiltering();
    radio.disableAddressFiltering();
    radio.setDataShaping(RADIOLIB_SHAPING_0_5);
    radio.setCrcFiltering(false);
    radio.setEncoding(RADIOLIB_ENCODING_WHITENING);
    radio.SPIsetRegValue(CC1101_REG_PKTCTRL0, CC1101_CRC_ON, 2, 2);
    radio.SPIsetRegValue(CC1101_REG_MDMCFG2, CC1101_SYNC_MODE_30_32, 2, 0);
    radio.SPIsetRegValue(CC1101_REG_FSCTRL1, 0x06, 4, 0);
    radio.SPIsetRegValue(CC1101_REG_FSCTRL0, 0x05, 7, 0);

    Serial.print(F("Setting IRQs"));
    radio.setGdo0Action(irqRead);
    Serial.println(F(" Done"));

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

    if (!machineReadbale) {
        Serial.print(F("[CC1101] Starting to listen ... "));
    }
    state = radio.startReceive();
    if (state == ERR_NONE) {
        if (!machineReadbale) {
            Serial.println(F("success!"));
        }
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true) {}
    }
}

void irqRead(void)
{
    ++numIrq;
    if (!enableInterrupt || transmitting) {
        ++numFailedIrq;
//        radio.startReceive();
        return;
    }

    auto len = radio.getPacketLength(true);
    if (len > 0) {
        uint8_t str[CC1101_MAX_PACKET_LENGTH];
        int state = radio.readData(str, CC1101_MAX_PACKET_LENGTH);
        lastState = state;
//        if (state == ERR_NONE || state == ERR_CRC_MISMATCH) {
            ++numPacket;
            queue.push((char *) str, len);
//        }
    }
    receivedFlag = true;
    radio.startReceive();
}

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


int cacheNumIrq = -1, cacheNumFailed = -1, cachedNumPacket = -1;

void loop()
{
    if (lastState != ERR_NONE) {
        Serial.print("ERR: ");
        Serial.println(lastState);
        lastState = ERR_NONE;
    }

    if (cacheNumIrq != numIrq || cacheNumFailed != numFailedIrq || cachedNumPacket != numPacket) {
        Serial.print(numIrq);
        Serial.print(" ");
        Serial.print(numFailedIrq);
        Serial.print(" ");
        Serial.print(numPacket);
        Serial.println();
        cacheNumIrq = numIrq ;
        cacheNumFailed = numFailedIrq;
        cachedNumPacket = numPacket;
    }

    if (receivedFlag) {
        handleReceived();
    }
}


