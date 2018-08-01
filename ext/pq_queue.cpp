#include "pq_queue.h"
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
PQ_Queue::PQ_Queue(uint32_t id, double rate, uint32_t limit_bytes, int location)
    : Queue(id, rate, limit_bytes, location) {}

#define FAIRNESS_PRIORITY 10
#define FIFO_PRIOROTY 20
#define SRPT_PRIORITY 30

int compValues(uint32_t a, uint32_t b)
{
    if (a == b)
    {
        return 0;
    }
    return  a < b ? -1 : 1;
}

int PQ_Queue::comparePackets(Packet *p1, Packet* p2) {
    if (p1->type == NORMAL_PACKET && p2->type == ACK_PACKET)
    {
        return 1;
    }

    if (p1->type == ACK_PACKET)
    {
        return p2->type == NORMAL_PACKET ? -1 : 0;
    }

    if (params.pq_mode == FAIRNESS_PRIORITY)
    {
        return compValues(p1->seq_no,  p2->seq_no);
    }

    if (params.pq_mode == SRPT_PRIORITY)
    {
        return compValues(p1->pf_priority, p2->pf_priority);
    }

    return 0;
}


void PQ_Queue::enque(Packet *packet) {
    registerInsert(packet);
    dropLeft();
}


Packet *PQ_Queue::extractPacket(Packet *packet) {
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

long long PQ_Queue::estimate(Packet *packet) {
    if (params.pq_mode == FAIRNESS_PRIORITY)
    {
        return packet->seq_no;
    }
    if (params.pq_mode == FIFO_PRIOROTY)
    {
        for (uint32_t i = 0; i < packets.size(); i++)
        {
            if (packets[i] == packet)
            {
                return i;
            }
        }
    }
    unimplemented();
}


