//
// Created by happycactus on 08/09/21.
//

#ifndef CCSNIFFER_PACKETQUEUE_H
#define CCSNIFFER_PACKETQUEUE_H

#include <Arduino.h>
#include <stdint.h>

class InterruptProtector {
public:
    InterruptProtector() {
        noInterrupts();
    }
    ~InterruptProtector() {
        interrupts();
    }
};

template <int QUEUESIZE = 4, int PKTSIZE = 64>
class PacketQueue {
    using Packet = char[PKTSIZE+1];
    Packet queue[QUEUESIZE];
    uint8_t head = 0, tail = 0;

    bool emptyNoBlock() const {
        return head == tail;
    }

   bool fullNoBlock() const {
        return ((head+1)%QUEUESIZE) == tail;
    }
public:
    PacketQueue() {

    }

    bool empty() const {
        InterruptProtector ip;
        return emptyNoBlock();
    }

    bool full() const {
        InterruptProtector ip;
        return fullNoBlock();
    }

    int pop(char *buffer) {
        InterruptProtector ip;
        if (!emptyNoBlock()) {
            uint8_t len = queue[tail][0];
            memcpy(buffer, &queue[tail][1], len);
            tail = (tail+1)%QUEUESIZE;
            return len;
        }
        return -1;
    }

    int push(char *buffer, uint8_t len) {
        InterruptProtector ip;
        if (!fullNoBlock()) {
            queue[head][0] = len;
            memcpy(&queue[head][1], buffer, len);
            head = (head+1)%QUEUESIZE;
            return len;
        }
        return -1;
    }
};


#endif //CCSNIFFER_PACKETQUEUE_H
