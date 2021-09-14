//
// Created by happycactus on 14/09/21.
//

#include "SerialHandler.h"
#include <Arduino.h>

SerialHandler::SerialHandler()
{

}


void SerialHandler::init()
{
    Serial.begin(38400);

}

bool SerialHandler::lineAvailable()
{
    if (mAvailable)
        return true;

    if (Serial.available() > 0) {
        return readIncoming();
    }

    return false;
}

bool SerialHandler::readIncoming()
{
    while (Serial.available() > 0) {
        mSerialBuf[mSerialLen] = Serial.read();

        // ignore newlines
        if (mSerialLen == 0 && mSerialBuf[mSerialLen] == 0x0a)
            continue;

        if (mSerialBuf[mSerialLen] == 0x0d) {
            mAvailable = true;
            return true;
        }

        if (mSerialLen < MAXSERIAL-1)
            ++mSerialLen;
    }
    return false;
}

int SerialHandler::copyLine(char *buffer, int maxlen)
{
    int l = mSerialLen;
    if (l > maxlen)
        l = maxlen;
    memcpy(buffer, mSerialBuf, l);
    mSerialLen=0;
    mAvailable=false;

    return l;
}

