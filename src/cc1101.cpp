/*
 * This code is based on RadioLib by Jan Gromeš
 * https://github.com/jgromes/RadioLib
 */

#include "cc1101.h"
#include "cc1101consts.h"

static const uint8_t SPIreadCommand = CC1101_CMD_READ;
static const uint8_t SPIwriteCommand = CC1101_CMD_WRITE;

[[noreturn]]
static void fail(const char *msg)
{
    Serial.print("FATAL: ");
    Serial.print(msg);
    while (true) {}
}

CC1101Tranceiver::CC1101Tranceiver(uint8_t cs, uint8_t gdo0, uint8_t gdo2, uint8_t rst)
        : _cs(cs), _gdo0(gdo0), _gdo2(gdo2), _rst(rst),
          _spiSettings(SPISettings(2000000, MSBFIRST, SPI_MODE0)),
          _spi(SPI)
{

}

int CC1101Tranceiver::initialize()
{
    detachInterrupt(digitalPinToInterrupt(_gdo0));
    detachInterrupt(digitalPinToInterrupt(_gdo2));

    pinMode(_cs, OUTPUT);
    digitalWrite(_cs, HIGH);

    pinMode(_gdo0, INPUT);
    pinMode(_gdo2, INPUT);

    _spi.begin();

    if (!findChip()) {
        return -1;
    }

    standby();

    SPIsetRegValue(CC1101_REG_MCSM0, CC1101_FS_AUTOCAL_IDLE_TO_RXTX, 5, 4);
    SPIsetRegValue(CC1101_REG_PKTCTRL1, CC1101_CRC_AUTOFLUSH_OFF | CC1101_APPEND_STATUS_ON | CC1101_ADR_CHK_NONE, 3, 0);
    SPIsetRegValue(CC1101_REG_PKTCTRL0, CC1101_WHITE_DATA_OFF | CC1101_PKT_FORMAT_NORMAL, 6, 4);
    SPIsetRegValue(CC1101_REG_PKTCTRL0, CC1101_CRC_ON | CC1101_LENGTH_CONFIG_VARIABLE, 2, 0);
    SPIsetRegValue(CC1101_REG_FIFOTHR, 0x0d, 3, 0);

    setFrequency(868.3f);
    setBitrate(38.4);
    setReceiverBW(200.0);
    setDeviation(20.0);
    setOutputPower(-30);    // minimum allowed power
    setModulation(Modulation::FSK2);
    setVariablePacketLength();
    setMaximumPacketLength(63);
    setPreambleLength(PreambleTypes::Bytes2);
    setSyncWord(0x12, 0xAD);

    SPIsendCommand(CC1101_CMD_FLUSH_RX);
    SPIsendCommand(CC1101_CMD_FLUSH_TX);

    return 0;
}

bool CC1101Tranceiver::findChip()
{
    uint8_t version;
    int i = 0;
    while (true) {
        version = getChipVersion();
        if ((version == CC1101_VERSION_CURRENT) || (version == CC1101_VERSION_LEGACY) ||
            (version == CC1101_VERSION_CLONE)) {
            break;
        }
        delay(10);
        i++;
        if (i == 10) {
            return false;
        }
    }

#if 1
    Serial.print("Version: ");
    Serial.print(version, HEX);
    Serial.println();
#endif

    SPIsendCommand(CC1101_CMD_RESET);
    delay(150);

    return true;
}

uint16_t CC1101Tranceiver::getChipVersion()
{
    return (SPIgetRegValue(CC1101_REG_VERSION));
}

uint16_t CC1101Tranceiver::SPIgetRegValue(uint8_t reg, uint8_t msb, uint8_t lsb)
{
    if (reg > CC1101_REG_TEST0) {
        reg |= CC1101_CMD_ACCESS_STATUS_REG;
    }

    if ((msb > 7) || (lsb > 7) || (lsb > msb)) {
        fail("invalid bit range");
    }

    uint8_t rawValue = SPIreadRegister(reg);
    uint8_t maskedValue = rawValue & ((0b11111111 << lsb) & (0b11111111 >> (7 - msb)));
    return (maskedValue);
}

uint16_t CC1101Tranceiver::SPIsetRegValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb, uint8_t checkInterval)
{
    if (reg > CC1101_REG_TEST0) {
        reg |= CC1101_CMD_ACCESS_STATUS_REG;
    }

    uint8_t currentValue = SPIreadRegister(reg);
    uint8_t mask = ~((0b11111111 << (msb + 1)) | (0b11111111 >> (8 - lsb)));
    uint8_t newValue = (currentValue & ~mask) | (value & mask);
    SPIwriteRegister(reg, newValue);

    return (0);
}

void CC1101Tranceiver::SPIreadRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t *inBytes)
{
    SPItransfer(SPIreadCommand, reg | CC1101_CMD_BURST, nullptr, inBytes, numBytes);
}

uint8_t CC1101Tranceiver::SPIreadRegister(uint8_t reg)
{
    if (reg > CC1101_REG_TEST0) {
        reg |= CC1101_CMD_ACCESS_STATUS_REG;
    }

    uint8_t resp;
    SPItransfer(SPIreadCommand, reg, nullptr, &resp, 1);
    return (resp);
}

void CC1101Tranceiver::SPIwriteRegister(uint8_t reg, uint8_t data)
{
    // status registers require special command
    if (reg > CC1101_REG_TEST0) {
        reg |= CC1101_CMD_ACCESS_STATUS_REG;
    }

    SPItransfer(SPIwriteCommand, reg, &data, nullptr, 1);
}

void CC1101Tranceiver::SPIwriteRegisterBurst(uint8_t reg, uint8_t *data, size_t len)
{
    SPItransfer(SPIwriteCommand, reg | CC1101_CMD_BURST, data, nullptr, len);
}

void CC1101Tranceiver::SPIsendCommand(uint8_t cmd)
{
    digitalWrite(_cs, LOW);

    // start transfer
    _spi.beginTransaction(_spiSettings);

    // send the command byte
    _spi.transfer(cmd);

    // stop transfer
    _spi.endTransaction();
    digitalWrite(_cs, HIGH);
}

void CC1101Tranceiver::SPItransfer(uint8_t cmd, uint8_t reg, uint8_t *dataOut, uint8_t *dataIn, uint8_t numBytes)
{
    _spi.beginTransaction(_spiSettings);

    digitalWrite(_cs, LOW);
    _spi.transfer(reg | cmd);

    if (cmd == SPIwriteCommand) {
        if (dataOut != nullptr) {
            for (size_t n = 0; n < numBytes; n++) {
                _spi.transfer(dataOut[n]);
            }
        }
    } else if (cmd == SPIreadCommand) {
        if (dataIn != nullptr) {
            for (size_t n = 0; n < numBytes; n++) {
                dataIn[n] = _spi.transfer(0x00);
            }
        }
    }

    digitalWrite(_cs, HIGH);
    _spi.endTransaction();
}

uint16_t CC1101Tranceiver::setFrequency(float freq)
{
    // check allowed frequency range
    if (!(((freq > 300.0) && (freq < 348.0)) ||
          ((freq > 387.0) && (freq < 464.0)) ||
          ((freq > 779.0) && (freq < 928.0)))) {
        fail("Invalid Baseband Frequency");
    }

    // set mode to standby
    SPIsendCommand(CC1101_CMD_IDLE);

    //set carrier frequency
    uint32_t base = 1;
    uint32_t FRF = (freq * (base << 16)) / 26.0;
    int16_t state = SPIsetRegValue(CC1101_REG_FREQ2, (FRF & 0xFF0000) >> 16, 7, 0);
    state |= SPIsetRegValue(CC1101_REG_FREQ1, (FRF & 0x00FF00) >> 8, 7, 0);
    state |= SPIsetRegValue(CC1101_REG_FREQ0, FRF & 0x0000FF, 7, 0);

    mFrequencyMhz = freq;
    return (setOutputPower(mPower));
}

namespace {
static void getExpMant(float target, uint16_t mantOffset, uint8_t divExp, uint8_t expMax, uint8_t &exp, uint8_t &mant)
{
    // get table origin point (exp = 0, mant = 0)
    float origin = (mantOffset * CC1101_CRYSTAL_FREQ * 1000000.0) / ((uint32_t) 1 << divExp);

    // iterate over possible exponent values
    for (int8_t e = expMax; e >= 0; e--) {
        // get table column start value (exp = e, mant = 0);
        float intervalStart = ((uint32_t) 1 << e) * origin;

        // check if target value is in this column
        if (target >= intervalStart) {
            // save exponent value
            exp = e;

            // calculate size of step between table rows
            float stepSize = intervalStart / (float) mantOffset;

            // get target point position (exp = e, mant = m)
            mant = ((target - intervalStart) / stepSize);

            // we only need the first match, terminate
            return;
        }
    }
}

}

uint16_t CC1101Tranceiver::setBitrate(float br)
{
    // TODO check range
//    RADIOLIB_CHECK_RANGE(br, 0.025, 600.0, ERR_INVALID_BIT_RATE);

    // set mode to standby
    SPIsendCommand(CC1101_CMD_IDLE);

    // calculate exponent and mantissa values
    uint8_t e = 0;
    uint8_t m = 0;
    getExpMant(br * 1000.0, 256, 28, 14, e, m);

    // set bit rate value
    int16_t state = SPIsetRegValue(CC1101_REG_MDMCFG4, e, 3, 0);
    state |= SPIsetRegValue(CC1101_REG_MDMCFG3, m);
    mBitrate = br;

    return (state);
}

uint16_t CC1101Tranceiver::setReceiverBW(float rxBw)
{
    // TODO check range
//    RADIOLIB_CHECK_RANGE(rxBw, 58.0, 812.0, ERR_INVALID_RX_BANDWIDTH);

    // set mode to standby
    SPIsendCommand(CC1101_CMD_IDLE);

    // calculate exponent and mantissa values
    for (int8_t e = 3; e >= 0; e--) {
        for (int8_t m = 3; m >= 0; m--) {
            float point = (CC1101_CRYSTAL_FREQ * 1000000.0) / (8 * (m + 4) * ((uint32_t) 1 << e));
            if ((fabs(rxBw * 1000.0) - point) <= 1000) {
                // set Rx channel filter bandwidth
                return (SPIsetRegValue(CC1101_REG_MDMCFG4, (e << 6) | (m << 4), 7, 4));
            }
        }
    }

    fail("Invalid RxBW");
}

uint16_t CC1101Tranceiver::setDeviation(float freqDev)
{
// set frequency deviation to lowest available setting (required for digimodes)
    float newFreqDev = freqDev;
    if (freqDev < 0.0) {
        newFreqDev = 1.587;
    }

    // TODO Check Range
//    RADIOLIB_CHECK_RANGE(newFreqDev, 1.587, 380.8, ERR_INVALID_FREQUENCY_DEVIATION);

    // set mode to standby
    SPIsendCommand(CC1101_CMD_IDLE);

    // calculate exponent and mantissa values
    uint8_t e = 0;
    uint8_t m = 0;
    getExpMant(newFreqDev * 1000.0, 8, 17, 7, e, m);

    // set frequency deviation value
    int16_t state = SPIsetRegValue(CC1101_REG_DEVIATN, (e << 4), 6, 4);
    state |= SPIsetRegValue(CC1101_REG_DEVIATN, m, 2, 0);
    return (state);
}

uint16_t CC1101Tranceiver::setOutputPower(int8_t power)
{
    // round to the known frequency settings
    uint8_t f;
    if (mFrequencyMhz < 374.0) {
        // 315 MHz
        f = 0;
    } else if (mFrequencyMhz < 650.5) {
        // 434 MHz
        f = 1;
    } else if (mFrequencyMhz < 891.5) {
        // 868 MHz
        f = 2;
    } else {
        // 915 MHz
        f = 3;
    }

    // get raw power setting
    static uint8_t paTable[8][4] = {{0x12, 0x12, 0x03, 0x03},
                             {0x0D, 0x0E, 0x0F, 0x0E},
                             {0x1C, 0x1D, 0x1E, 0x1E},
                             {0x34, 0x34, 0x27, 0x27},
                             {0x51, 0x60, 0x50, 0x8E},
                             {0x85, 0x84, 0x81, 0xCD},
                             {0xCB, 0xC8, 0xCB, 0xC7},
                             {0xC2, 0xC0, 0xC2, 0xC0}};

    uint8_t powerRaw;
    switch (power) {
        case -30:
            powerRaw = paTable[0][f];
            break;
        case -20:
            powerRaw = paTable[1][f];
            break;
        case -15:
            powerRaw = paTable[2][f];
            break;
        case -10:
            powerRaw = paTable[3][f];
            break;
        case 0:
            powerRaw = paTable[4][f];
            break;
        case 5:
            powerRaw = paTable[5][f];
            break;
        case 7:
            powerRaw = paTable[6][f];
            break;
        case 10:
            powerRaw = paTable[7][f];
            break;
        default:
            fail("Invalid Power");
    }

    // store the value
    mPower = power;

    if (mModulation == Modulation::ASK_OOK) {
        // Amplitude modulation:
        // PA_TABLE[0] is the power to be used when transmitting a 0  (no power)
        // PA_TABLE[1] is the power to be used when transmitting a 1  (full power)

        uint8_t paValues[2] = {0x00, powerRaw};
        SPIwriteRegisterBurst(CC1101_REG_PATABLE, paValues, 2);
        return 0;
    } else {
        // Freq modulation:
        // PA_TABLE[0] is the power to be used when transmitting.
        return (SPIsetRegValue(CC1101_REG_PATABLE, powerRaw));
    }
}

void CC1101Tranceiver::setSyncType(CC1101Tranceiver::SyncType type)
{
    uint8_t value = static_cast<uint8_t>(type);
    SPIsetRegValue(CC1101_REG_MDMCFG2, value, 2, 0);
}

void CC1101Tranceiver::setPreambleLength(CC1101Tranceiver::PreambleTypes type)
{
    uint8_t value = static_cast<uint8_t>(type) << 4;
    SPIsetRegValue(CC1101_REG_MDMCFG1, value, 6, 4);
}

void CC1101Tranceiver::setModulation(CC1101Tranceiver::Modulation modulation)
{
    switch (modulation) {
        case Modulation::FSK2:
            SPIsetRegValue(CC1101_REG_MDMCFG2, CC1101_MOD_FORMAT_2_FSK, 6, 4);
            SPIsetRegValue(CC1101_REG_FREND0, 0, 2, 0);
            break;
        case Modulation::GFSK:
            SPIsetRegValue(CC1101_REG_MDMCFG2, CC1101_MOD_FORMAT_GFSK, 6, 4);
            SPIsetRegValue(CC1101_REG_FREND0, 0, 2, 0);
            break;
        case Modulation::ASK_OOK:
            SPIsetRegValue(CC1101_REG_MDMCFG2, CC1101_MOD_FORMAT_ASK_OOK, 6, 4);
            SPIsetRegValue(CC1101_REG_FREND0, 1, 2, 0);
            break;
        case Modulation::FSK4:
            SPIsetRegValue(CC1101_REG_MDMCFG2, CC1101_MOD_FORMAT_4_FSK, 6, 4);
            SPIsetRegValue(CC1101_REG_FREND0, 1, 2, 0);
            break;
        case Modulation::MFSK:
            SPIsetRegValue(CC1101_REG_MDMCFG2, CC1101_MOD_FORMAT_MFSK, 6, 4);
            SPIsetRegValue(CC1101_REG_FREND0, 1, 2, 0);
            break;
    }
    mModulation = modulation;
    setOutputPower(mPower);
}

void CC1101Tranceiver::setVariablePacketLength()
{
    SPIsetRegValue(CC1101_REG_PKTCTRL0, CC1101_LENGTH_CONFIG_VARIABLE, 1, 0);
    mVariableLength = true;
}

void CC1101Tranceiver::setSyncWord(uint8_t w1, uint8_t w2)
{
    SPIsetRegValue(CC1101_REG_SYNC1, w1);
    SPIsetRegValue(CC1101_REG_SYNC0, w2);
}

void CC1101Tranceiver::enableCRC()
{
//    if (crcOn == true) {
        SPIsetRegValue(CC1101_REG_PKTCTRL0, CC1101_CRC_ON, 2, 2);
//    } else {
//        return(SPIsetRegValue(CC1101_REG_PKTCTRL0, CC1101_CRC_OFF, 2, 2));
//    }
}

void CC1101Tranceiver::enableWhitening()
{
    SPIsetRegValue(CC1101_REG_PKTCTRL0, CC1101_WHITE_DATA_ON, 6, 6);
}

void CC1101Tranceiver::setMaximumPacketLength(uint8_t max)
{
    SPIsetRegValue(CC1101_REG_PKTLEN, max);
}

int CC1101Tranceiver::read(uint8_t *buffer, int buffersize)
{
    uint8_t _rawRSSI;
    uint8_t _rawLQI;

    uint8_t len;
    // this code is for Variable Length and no filtering.
    len = SPIreadRegister(CC1101_REG_FIFO);
    SPIreadRegister(CC1101_REG_FIFO);

    uint8_t bytesInFIFO = SPIgetRegValue(CC1101_REG_RXBYTES, 6, 0);
    size_t readBytes = 0;
    uint32_t lastPop = millis();

    // keep reading from FIFO until we get all the packet.
    while (readBytes < len) {
        if (bytesInFIFO == 0) {
            if (millis() - lastPop > 5) {
                // TODO: handle this timeout.
                break;
            } else {
                /*
                 * Does this work for all rates? If 1 ms is longer than the 1ms delay
                 * then the entire FIFO will be transmitted during that delay.
                 *
                 * TODO: drop this delay(1) or come up with a better solution:
                */
                delay(1);
                bytesInFIFO = SPIgetRegValue(CC1101_REG_RXBYTES, 6, 0);
                continue;
            }
        }

        // read the minimum between "remaining length" and bytesInFifo
        uint8_t bytesToRead = min((uint8_t)(len - readBytes), bytesInFIFO);
        SPIreadRegisterBurst(CC1101_REG_FIFO, bytesToRead, &(buffer[readBytes]));
        readBytes += bytesToRead;
        lastPop = millis();

        // Get how many bytes are left in FIFO.
        bytesInFIFO = SPIgetRegValue(CC1101_REG_RXBYTES, 6, 0);
    }

    // check if status bytes are enabled (default: CC1101_APPEND_STATUS_ON)
    bool isAppendStatus = SPIgetRegValue(CC1101_REG_PKTCTRL1, 2, 2) == CC1101_APPEND_STATUS_ON;

    // If status byte is enabled at least 2 bytes (2 status bytes + any following packet) will remain in FIFO.
    if (bytesInFIFO >= 2 && isAppendStatus) {
        // read RSSI byte
        _rawRSSI = SPIgetRegValue(CC1101_REG_FIFO);

        // read LQI and CRC byte
        uint8_t val = SPIgetRegValue(CC1101_REG_FIFO);
        _rawLQI = val & 0x7F;

        // TODO: check CRC here if needed.
//        if (_crcOn && (val & CC1101_CRC_OK) == CC1101_CRC_ERROR) {
//            return (ERR_CRC_MISMATCH);
//        }
    }

    // Flush then standby according to RXOFF_MODE (default: CC1101_RXOFF_IDLE)
    if (SPIgetRegValue(CC1101_REG_MCSM1, 3, 2) == CC1101_RXOFF_IDLE) {
        SPIsendCommand(CC1101_CMD_FLUSH_RX);
        standby();
    }

}


void CC1101Tranceiver::standby()
{
    SPIsendCommand(CC1101_CMD_IDLE);
//    _mod->setRfSwitchState(LOW, LOW);
}

bool CC1101Tranceiver::receive()
{
    standby();

    SPIsendCommand(CC1101_CMD_FLUSH_RX);
    // set RF switch (if present)
    // _mod->setRfSwitchState(HIGH, LOW);

    SPIsendCommand(CC1101_CMD_RX);
}

void CC1101Tranceiver::setReceiveHandler(void (*func)(void), CC1101Tranceiver::SignalDirection direction)
{
    standby();
    SPIsendCommand(CC1101_CMD_FLUSH_RX);
    // Here should we allow the user to decide what interrupt to handle?
    SPIsetRegValue(CC1101_REG_IOCFG0,CC1101_GDOX_SYNC_WORD_SENT_OR_RECEIVED/*CC1101_GDOX_PKT_RECEIVED_CRC_OK*/);

    attachInterrupt(digitalPinToInterrupt(_gdo0), func, static_cast<int >(direction));
}

void CC1101Tranceiver::setTransmitHandler(void (*func)(void), CC1101Tranceiver::SignalDirection direction)
{
    SPIsetRegValue(CC1101_REG_IOCFG2, CC1101_GDOX_TX_FIFO_UNDERFLOW);
    attachInterrupt(digitalPinToInterrupt(_gdo2), func, static_cast<int >(direction));
}
