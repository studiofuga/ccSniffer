#include <Arduino.h>
#include "cc1101.h"
#include "PacketQueue.h"
#include "SerialHandler.h"

#if defined (BOARD_HUZZAH32)
CC1101Tranceiver radio(25, 39, 34);
#elif defined (BOARD_NANO)
CC1101Tranceiver radio(10, 3, 2);
#endif

volatile bool receivedFlag = false;
volatile bool enableInterrupt = true;
volatile bool transmitting = false;

using PacketQueueT = PacketQueue<4, 64>;
PacketQueueT queue;

SerialHandler serial;

void irqRead(void);
void irqSent(void);

volatile int numSent = 0;
volatile int numTimeout = 0;

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
    auto status = radio.read(str, 128);

    if (status.len > 0 && status.errc != ReadErrCode::NoData) {
        PacketQueueT ::PacketT packet;
        if (status.errc == ReadErrCode::CrcError )
            packet.setStatus(CRCError);
        packet.setRssi(status.rssi);
        packet.setLqi(status.lqi);
        packet.rawCopyFrom(str, status.len);
        queue.push(packet);
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
        PacketQueueT ::PacketT packet;
        uint8_t len = queue.pop(packet);

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
    enableInterrupt = true;
}


int cacheNumSent = -1, cachedNumTo = -1;

void loop()
{
    if (cachedNumTo != numTimeout) {
        Serial.println();
        Serial.println("+CC1101 Timeout");
        cachedNumTo = numTimeout;
    }
    if (cacheNumSent != numSent) {
        Serial.println();
        Serial.print("+CC1101 ");
        Serial.print(numSent);
        Serial.println(" Preambles Recv/Sent");
        cacheNumSent = numSent;
    }

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
        auto sn = radio.transmit(pkt, pktlen);

//        Serial.print("Sent ");
//        Serial.print(sn);
//        Serial.print(": ");
//        PrintHex8(pkt, pktlen, " ");
    }

    if (receivedFlag) {
        handleReceived();
    }
}


