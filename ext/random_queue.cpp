#include "random_queue.h"
#include "../run/params.h"

#include <iostream>
#include <limits.h>
#include <algorithm>
#include <chrono>
#include <random>

extern double get_current_time();
extern void add_to_event_queue(Event *ev);
extern DCExpParams params;

/* Fairness Queue */
Random_Queue::Random_Queue(uint32_t id, double rate, uint32_t limit_bytes, int location)
    : Queue(id, rate, limit_bytes, location) {}

void Random_Queue::enque(Packet *packet) {
    registerInsert(packet);
    if (bytes_in_queue > limit_bytes) {
        uint32_t worst_index = (std::rand() % packets.size() + 3*packets.size()) % packets.size();
        bytes_in_queue -= packets[worst_index]->size;
        Packet *worst_packet = packets[worst_index];
        packets.erase(packets.begin() + worst_index);
        pkt_drop++;
        drop(worst_packet);
    }
}


Packet* Random_Queue::extractPacket(Packet *packet) {
    uint32_t best_index = 0;
    for (uint32_t i = 0; i < packets.size(); i++) {
        Packet* curr_pkt = packets[i];
        if (curr_pkt == packet) {
            best_index = i;
            break;
        }
    }
    clearByIndex(best_index);
    return packet;
}

long long Random_Queue::estimate(Packet *packet) {
    return 0;
}

Packet *Random_Queue::peek() {
    if (packets.size() == 0)
    {
        return NULL;
    }
    uint32_t index = (std::rand() % packets.size() + 3*packets.size()) % packets.size();
    return packets[index];
}

void Random_Queue::sort(std::vector<std::pair<Packet *, Queue *>> &packets) {
    static auto rng = std::default_random_engine {23};
    std::shuffle(packets.begin(), packets.end(), rng);
}


