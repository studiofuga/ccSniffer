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

enum PacketStatus : uint8_t {
    PacketOK = 0x00, CRCError=0x01
};

template <int PKTSIZE = 64>
class Packet {
private:
    uint8_t buffer[PKTSIZE];
    uint8_t length = 0;
    uint8_t lqi = 0;
    uint8_t rssi = 0;
    PacketStatus status = PacketStatus::PacketOK;

public:
    static const uint8_t RAWSIZE = PKTSIZE+4;

    Packet() {

    }

    const uint8_t *data() const {
        return &buffer[0];
    }

    uint8_t len() const {
        return length;
    }

    uint8_t getLqi() const
    {
        return lqi;
    }

    void setLqi(uint8_t lqi)
    {
        Packet::lqi = lqi;
    }

    uint8_t getRssi() const
    {
        return rssi;
    }

    void setRssi(uint8_t rssi)
    {
        Packet::rssi = rssi;
    }

    PacketStatus getStatus() const
    {
        return status;
    }

    void setStatus(PacketStatus status)
    {
        Packet::status = status;
    }

    void rawCopyFrom(uint8_t *memory, uint8_t len) {
        length = len;
        if (length > PKTSIZE) {
            length = PKTSIZE;
        }
        memcpy(buffer, memory, length);
    }

    uint8_t rawCopyTo(uint8_t *memory, uint8_t maxlen) const {
        uint8_t len = length;
        if (len > maxlen)
            len = maxlen;
        memcpy(memory, buffer, len);
        return len;
    }
};

template <int QUEUESIZE = 4, int PKTSIZE = 64>
class PacketQueue {
    using RawPacket = uint8_t[Packet<PKTSIZE>::RAWSIZE];
    RawPacket queue[QUEUESIZE];
    uint8_t head = 0, tail = 0;

    bool emptyNoBlock() const {
        return head == tail;
    }

   bool fullNoBlock() const {
        return ((head+1)%QUEUESIZE) == tail;
    }
    
    static const uint8_t IDX_LQI=0;
    static const uint8_t IDX_RSSI=1;
    static const uint8_t IDX_STATUS=2;
    static const uint8_t IDX_LEN=3;
    static const uint8_t IDX_DATA=4;
public:
    using PacketT = Packet<PKTSIZE>;

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

    int pop(Packet<PKTSIZE> &packet) {
        InterruptProtector ip;
        if (!emptyNoBlock()) {
            packet.setLqi(queue[tail][IDX_LQI]);
            packet.setRssi(queue[tail][IDX_RSSI]);
            packet.setStatus(static_cast<PacketStatus>(queue[tail][IDX_STATUS]));
            packet.rawCopyFrom(&queue[tail][IDX_DATA], queue[tail][IDX_LEN]);
            tail = (tail+1)%QUEUESIZE;
            return packet.len();
        }
        return 0;
    }

    int push(Packet<PKTSIZE> const &packet) {
        InterruptProtector ip;
        if (!fullNoBlock()) {
            queue[head][IDX_LEN] = packet.len();
            queue[head][IDX_LQI] = packet.getLqi();
            queue[head][IDX_RSSI] = packet.getRssi();
            queue[head][IDX_STATUS] = packet.getStatus();
            packet.rawCopyTo(&queue[head][IDX_DATA], PKTSIZE);
            head = (head+1)%QUEUESIZE;
            return packet.len();
        }
        return 0;
    }
};


#endif //CCSNIFFER_PACKETQUEUE_H
