#ifndef RANDOM_QUEUE_H
#define RANDOM_QUEUE_H

#include "../coresim/queue.h"
#include "../coresim/packet.h"

class Random_Queue : public Queue {
    public:
        Random_Queue(uint32_t id, double rate, uint32_t limit_bytes, int location);
        void enque(Packet *packet);
        Packet* extractPacket(Packet *packet);
        Packet* peek() override;
        long long estimate(Packet *packet);
        virtual void sort(std::vector<std::pair<Packet*, Queue*>>& packets);
};

#endif
