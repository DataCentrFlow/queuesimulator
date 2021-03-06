#ifndef DCTCP_QUEUE_H
#define DCTCP_QUEUE_H

#include "../coresim/queue.h"
#include "../coresim/packet.h"

class DctcpQueue : public Queue {
    public:
        DctcpQueue(uint32_t id, double rate, uint32_t limit_bytes, int location);
        void enque(Packet *packet);
};

#endif
