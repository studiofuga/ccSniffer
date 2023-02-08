//
// Created by happycactus on 08/09/21.
//

#ifndef CCSNIFFER_PACKETQUEUE_H
#define CCSNIFFER_PACKETQUEUE_H

#include <Arduino.h>
#include <stdint.h>


enum PacketStatus : uint8_t {
    PacketOK = 0x00, CRCError=0x01
};

#define DEFAULT_PACKET_SIZE 64
#define DEFAULT_QUEUE_LENGTH 3

template <int PKTSIZE = DEFAULT_PACKET_SIZE>
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

template <int PKTQUEUELEN = DEFAULT_QUEUE_LENGTH, int PKTSIZE = DEFAULT_PACKET_SIZE>
class RawPacketsQueue {
public:
    struct RawPacket {
        uint8_t length = 0;
        uint8_t buffer[PKTSIZE];
    };

private:
    RawPacket queue[PKTQUEUELEN];
    uint8_t head = 0;
    uint8_t tail = 0;
public:
    static constexpr size_t MAX_PACKET_SIZE = PKTSIZE;

    RawPacketsQueue()= default;

    bool empty() const {
        return head == tail;
    }

    bool full() const {
        return (head + 1) % PKTQUEUELEN == tail;
    }

    uint8_t push(const uint8_t *data, uint8_t len) {
        if (full()) {
            return 0;
        }
        uint8_t pos = head;
        head = (head + 1) % PKTQUEUELEN;
        queue[pos].length = len;
        memcpy(queue[pos].buffer, data, len);
        return len;
    }

    uint8_t pop(uint8_t *data, uint8_t maxlen) {
        if (empty()) {
            return 0;
        }
        uint8_t pos = tail;
        tail = (tail + 1) % PKTQUEUELEN;
        uint8_t len = queue[pos].length;
        if (len > maxlen)
            len = maxlen;
        memcpy(data, queue[pos].buffer, len);
        return len;
    }
};

template <int PKTQUEUELEN = DEFAULT_QUEUE_LENGTH, int PKTSIZE = DEFAULT_PACKET_SIZE>
class PacketsQueue {
public:
    using PacketType = Packet<PKTSIZE>;
private:
    PacketType queue[PKTQUEUELEN];
    uint8_t head = 0;
    uint8_t tail = 0;

public:
    PacketsQueue() = default;

    bool empty() const {
        return head == tail;
    }

    bool full() const {
        return (head + 1) % PKTQUEUELEN == tail;
    }

    uint8_t push(const PacketType &pkt) {
        if (full()) {
            return 0;
        }
        uint8_t pos = head;
        head = (head + 1) % PKTQUEUELEN;
        queue[pos] = pkt;
        return 1;
    }

    uint8_t pop(PacketType &pkt) {
        if (empty()) {
            return 0;
        }
        uint8_t pos = tail;
        tail = (tail + 1) % PKTQUEUELEN;
        pkt = queue[pos];
        return 1;
    }
};


#endif //CCSNIFFER_PACKETQUEUE_H
