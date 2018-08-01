#include "pfabricqueue.h"
#include "../run/params.h"

#include <iostream>
#include <limits.h>
#include <assert.h>
#include <algorithm>
#include <random>
#include <chrono>

extern double get_current_time();
extern void add_to_event_queue(Event *ev);
extern DCExpParams params;
/* PFabric Queue */
PFabricQueue::PFabricQueue(uint32_t id, double rate, uint32_t limit_bytes, int location)
    : Queue(id, rate, limit_bytes, location) {}


void PFabricQueue::updatePacketPriorities(Packet *packet)
{
    if (packet->type != NORMAL_PACKET)
    {
        return;
    }

    for (int i = 0; i < packets.size(); i++)
    {
        if (packets[i]->flow->id != packet->flow->id || packets[i]->type != NORMAL_PACKET)
        {
            continue;
        }

        if (packets[i]->seq_no == packet->seq_no)
        {
            packets[i]->wa_srpt = std::max(packets[i]->wa_srpt, packet->wa_srpt);
            packet->wa_srpt = std::max(packet->wa_srpt, packets[i]->wa_srpt);
        }

        if (packet->pf_hidden_priority == packet->pf_priority)
        {
            packets[i]->pf_priority = packets[i]->pf_hidden_priority;
        }

        if (packets[i]->pf_hidden_priority == packets[i]->pf_priority)
        {
            packet->pf_priority = packet -> pf_hidden_priority;
        }
    }
}

int compValues(int a, int b) {
    if (a == b)
    {
        return 0;
    }
    return a < b ? -1 : 1;
}

int PFabricQueue::comparePackets(Packet *p1, Packet *p2) {

    if (params.srpt_mode == 20)
    {
        return compValues(p1->wa_srpt, p2->wa_srpt);
    }
    if (p1->pf_priority != p2->pf_priority) {
        return compValues(p1->pf_priority, p2->pf_priority);
    }
    if (params.srpt_wit_fair == 1) {
        return compValues(p1->seq_no, p2->seq_no);
    }
    if (params.srpt_wit_fair == 2) {
        return compValues(p1->wa_srpt, p2->wa_srpt);
    }
    return 0;
}


long long PFabricQueue::estimate(Packet *packet) {
    return params.srpt_mode == 20 ? packet->wa_srpt : packet->pf_priority;
}


void PFabricQueue::enque(Packet *packet) {
    updatePacketPriorities(packet);
    if (packet->type == NORMAL_PACKET)
    {
        for (int i =0; i < packets.size(); i++)
        {
            if ((packets[i]->flow->id == packet->flow->id) && (packets[i]->seq_no == packet->seq_no) && packets[i]->type == NORMAL_PACKET)
            {
                return;
            }
        }
    }
    registerInsert(packet);
    dropLeft();
}

Packet *PFabricQueue::extractPacket(Packet *packet) {
    int best_index = -1;
    for (uint32_t i = 0; i < packets.size(); i++) {
        Packet* curr_pkt = packets[i];
        if (curr_pkt->flow->id  == packet->flow->id) {
            best_index = i;
            break;
        }
    }
    Packet *p = packets[best_index];
    clearByIndex(best_index);
    return p;
}



