/*
 * This code is based on RadioLib by Jan Gromeš
 * https://github.com/jgromes/RadioLib
 */

#ifndef CCSNIFFER_CC1101_H
#define CCSNIFFER_CC1101_H

#include <SPI.h>
#include "cc1101consts.h"

enum class ReadErrCode : uint8_t {
    Ok = 0x00,
    CrcError = 0x01,
    NoData = 0xff
};

struct ReadStatus {
    ReadErrCode errc = ReadErrCode::Ok;
    uint8_t lqi= 0;
    uint8_t rssi= 0;
    uint8_t len = 0;
};

class CC1101Tranceiver {
public:
    CC1101Tranceiver(uint8_t cs, uint8_t gdo0, uint8_t gdo2, uint8_t rst = 0xff);

    int initialize();

    enum class SyncType {
        NoSync = 0, Sync15_16, Sync16_16, Sync30_32,
        CarrierSense, CarrierSense15_16, CarrierSense16_16, CarrierSense30_32
    };
    enum class PreambleTypes {
        Bytes2 = 0, Bytes3, Bytes4, Bytes6, Bytes8, bytes12, Bytes16, Bytes28
    };
    enum class Modulation {
        FSK2, GFSK, ASK_OOK, FSK4, MFSK
    };
    enum class SignalDirection {
        Change = 1, Falling = 1, Rising = 2
    };

private:
    uint8_t _cs = 0xff;
    uint8_t _gdo0 = 0xff;
    uint8_t _gdo2 = 0xff;
    uint8_t _rst = 0xff;

    float mFrequencyMhz = 868.3f;
    float mBitrate;
    float mPower;
    Modulation mModulation = Modulation::GFSK;
    bool mVariableLength;
    uint8_t mFixedPacketLength;

    SPISettings _spiSettings;
    SPIClass &_spi;

    bool findChip();

public:
    uint16_t getChipVersion();

    uint16_t setFrequency(float freq);
    uint16_t setBitrate(float br);
    uint16_t setReceiverBW(float rxBw);
    uint16_t setDeviation(float freqDev);
    uint16_t setOutputPower(int8_t power);

    void setModulation(Modulation modulation);
    void setMaximumPacketLength(uint8_t max = 255);
    void setVariablePacketLength();
    void setSyncType(SyncType type);
    void setPreambleLength(PreambleTypes type);
    void setSyncWord(uint8_t w1, uint8_t w2);
    void enableCRC();
    void enableWhitening();

    void standby();

    void setReceiveHandler(void (*func)(void), SignalDirection direction = SignalDirection::Rising);
    void setTransmitHandler(void (*func)(void), SignalDirection direction = SignalDirection::Rising);

    ReadStatus read(uint8_t *buffer, int buffersize);
    void receive();

    uint16_t SPIgetRegValue(uint8_t reg, uint8_t msb = 7, uint8_t lsb = 0);
    uint16_t SPIsetRegValue(uint8_t reg, uint8_t value, uint8_t msb = 7, uint8_t lsb = 0, uint8_t checkInterval = 2);
    void SPIreadRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t *inBytes);
    uint8_t SPIreadRegister(uint8_t reg);
    void SPIwriteRegisterBurst(uint8_t reg, uint8_t *data, size_t len);
    void SPIwriteRegister(uint8_t reg, uint8_t data);
    void SPIsendCommand(uint8_t cmd);

    void SPItransfer(uint8_t cmd, uint8_t reg, uint8_t *dataOut, uint8_t *dataIn, uint8_t numBytes);
};

#endif //CCSNIFFER_CC1101_H
