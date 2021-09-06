#include <Arduino.h>
#include <RadioLib.h>

CC1101 radio = new Module(10, 3, RADIOLIB_NC, 2);
volatile bool receivedFlag = false;
volatile bool enableInterrupt = true;
volatile bool transmitting = false;

bool machineReadbale = true;

void setFlag(void)
{
    if (!enableInterrupt || transmitting) {
        return;
    }
    receivedFlag = true;
}

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

    radio.setGdo0Action(setFlag);

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

    auto len = radio.getPacketLength();
    uint8_t str[255];
    int state = radio.readData(str, 255);

    if (state == ERR_NONE || state == ERR_CRC_MISMATCH) {
        if (!machineReadbale) {
            Serial.println(F("[CC1101] Received packet!"));
            if (state == ERR_CRC_MISMATCH) {
                Serial.print(F("[CC1101] BAD CRC!"));
            }
            Serial.print(F("[CC1101] Data:\t\t"));
            for (auto i = 0; i < len; ++i) {
                PrintHex8(&str[i], 1, (machineReadbale ? nullptr : " "));
                if (!machineReadbale && (i % 16) == 15) {
                    Serial.println();
                }
            }
            Serial.println();

            Serial.print(F("[CC1101] RSSI:\t\t"));
            Serial.print(radio.getRSSI());
            Serial.println(F(" dBm"));

            Serial.print(F("[CC1101] LQI:\t\t"));
            Serial.println(radio.getLQI());
        } else {
            Serial.print(F("*"));
            Serial.print(millis());
            Serial.print(F(","));
            Serial.print(radio.getRSSI());
            Serial.print(F(","));
            Serial.print(radio.getLQI());
            Serial.print(F(","));
            for (auto i = 0; i < len; ++i) {
                PrintHex8(&str[i], 1, nullptr);
            }

            if (state == ERR_CRC_MISMATCH) {
                Serial.print(",BADCRC");
            }
            Serial.println();
        }
    } else {
        if (!machineReadbale) {
            Serial.print(F("failed, code "));
            Serial.println(state);
        } else {
            Serial.print("!ERR:");
            Serial.println(state);
        }
    }

    radio.startReceive();
    enableInterrupt = true;
}

void loop()
{
    if (receivedFlag) {
        handleReceived();
    }
}


