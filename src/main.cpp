#include <Arduino.h>
#include "cc1101.h"
#include "PacketQueue.h"
#include "SerialHandler.h"

#if defined (BOARD_HUZZAH32)
CC1101Tranceiver radio(25, 39, 34);
#elif defined (BOARD_NANO)
CC1101Tranceiver radio(10, 3, 2);
#endif

using UnprocessedQueue = RawPacketsQueue<4,64>;
UnprocessedQueue unprocessedQueue;
using Queue = PacketsQueue<4,64>;
Queue queue;

SerialHandler serial;

void irqRead(void);
void irqSent(void);

volatile int numSent = 0;
volatile int numTimeout = 0;
volatile int numRecvIrq = 0;

void PrintHex8(const uint8_t *data, uint8_t length, char const *separator) // prints 8-bit data in hex with leading zeroes
{
    for (int i = 0; i < length; i++) {
        if (data[i] < 0x10) { Serial.print("0"); }
        Serial.print(data[i], HEX);

        if (separator != nullptr) {
            Serial.print(" ");
        }
    }
}

uint8_t nibble(char n) {
    if (n >= '0' && n <= '9')
        return n - '0';
    if (n >= 'A' && n <= 'F')
        return n - 'A' + 10;
    if (n >= 'a' && n <= 'f')
        return n - 'a' + 10;
    return 0;
}

size_t hexToBin(const char *hex, uint8_t *bin, size_t maxbinlen) {
    size_t len = 0;
    while (true) {
        if (*hex == '\0') {
            return len;
        }
        *bin = (nibble(*hex) << 4);
        ++hex;
        if (*hex == '\0') {
            return len;
        }
        *bin = *bin | (nibble(*hex));
        ++hex;
        ++len;
        ++bin;
        if (len == maxbinlen)
            return len;
    }
}

void setup()
{
    serial.init();

    Serial.println(F("+ccSniffer"));
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
    Serial.print("+Chip version: ");
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
        if ((i%8) == 0)
            Serial.print("+");
        uint8_t value = radio.SPIreadRegister(i);
        PrintHex8(&value, 1, " ");
        if (i % 8 == 7) {
            Serial.println();
        }
    }

    Serial.println("+READY");
}

void irqSent(void)
{
    ++numSent;
}

void irqRead(void)
{
    ++numRecvIrq;
    size_t retries = 0;
    while(true) {
        if (++retries > 100) {
            // timeout
            ++numTimeout;
            radio.receive();
            return;
        }
        auto fifo = radio.SPIgetRegValue(CC1101_REG_RXBYTES, 6, 0);
        if (fifo > 0) break;
    };

    uint8_t str[128];
    auto status = radio.read(str, 128);

    if (status.len > 0 && status.errc != ReadErrCode::NoData) {
        unprocessedQueue.push(str, status.len);
    }

    radio.receive();
}

void handleUnprocessed()
{
    int y= 0;
    do {
        noInterrupts();
        if (unprocessedQueue.empty()) {
            interrupts();
/*
            if (y>0) {
                Serial.print(y);
                Serial.print(" ");
                Serial.println(queue.empty());
            }
*/
            return;
        }

        uint8_t raw[UnprocessedQueue::MAX_PACKET_SIZE];
        uint8_t len = unprocessedQueue.pop(raw, UnprocessedQueue::MAX_PACKET_SIZE);
        interrupts();

        Queue::PacketType packet;

        packet.setRssi(raw[len-2]);
        packet.setLqi(raw[len-1] & 0x7f);
        packet.setStatus((raw[len-1] & 0x80) ? PacketOK : CRCError);
        packet.rawCopyFrom(raw+1, len-3);

        queue.push(packet);
        ++y;
    } while (true);
}

void handleReceived()
{
    if (!queue.empty()) {
        Queue::PacketType packet;
        if (queue.pop(packet)) {

            Serial.print(F("*"));
            Serial.print(millis());
            Serial.print(F(","));
            Serial.print(packet.getRssi());
            Serial.print(F(","));
            Serial.print(packet.getLqi());
            Serial.print(F(","));
            PrintHex8(packet.data(), packet.len(), nullptr);

            if (packet.getStatus() == CRCError) {
                Serial.print(",BADCRC");
            }
            Serial.println();
        }
    }
}


int cacheNumSent = -1, cachedNumTo = -1;
int cachedNumIrq = -1;

void loop()
{
    if (cachedNumTo != numTimeout) {
        Serial.println("+CC1101 Timeout");
        cachedNumTo = numTimeout;
    }
/*
    if (cachedNumIrq != numRecvIrq) {
        Serial.print("+CC1101 IRQ:");
        Serial.println(numRecvIrq);
        cachedNumIrq = numRecvIrq;
    }
*/
/*
    if (cacheNumSent != numSent) {
        Serial.println();
        Serial.print("+CC1101 ");
        Serial.print(numSent);
        Serial.println(" Preambles Recv/Sent");
        cacheNumSent = numSent;
    }
*/

    if (serial.lineAvailable()) {
        static char buf[128];
        auto sz = serial.copyLine(buf,128);
        buf[sz] = '\0';
//        Serial.print("Got ");
//        Serial.print(sz);
//        Serial.print(": ");
//        Serial.println(buf);

        static uint8_t pkt[64];
        auto pktlen = hexToBin(buf,pkt,64);
        radio.transmit(pkt, pktlen);

//        Serial.print("Sent ");
//        Serial.print(sn);
//        Serial.print(": ");
//        PrintHex8(pkt, pktlen, " ");
    }

    handleUnprocessed();
    handleReceived();
}


