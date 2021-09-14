//
// Created by happycactus on 14/09/21.
//

#ifndef CCSNIFFER_SERIALHANDLER_H
#define CCSNIFFER_SERIALHANDLER_H

#include <stdint.h>

#define MAXSERIAL 128

class SerialHandler {
    char mSerialBuf[MAXSERIAL];
    uint8_t mSerialLen = 0;
    bool mAvailable = false;

    bool readIncoming();
public:
    SerialHandler();

    void init();

    bool lineAvailable() ;

    int copyLine(char *buffer, int maxlen);
};


#endif //CCSNIFFER_SERIALHANDLER_H
