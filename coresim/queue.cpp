#include <climits>
#include <iostream>
#include <stdlib.h>
#include <algorithm>
#include "assert.h"

#include "queue.h"
#include "packet.h"
#include "event.h"
#include "debug.h"

#include "../run/params.h"

extern double get_current_time(); // TODOm
extern void add_to_event_queue(Event* ev);
extern uint32_t dead_packets;
extern DCExpParams params;

uint32_t Queue::instance_count = 0;

/* Queues */
Queue::Queue(uint32_t id, double rate, uint32_t limit_bytes, int location) {
    this->id = id;
    this->unique_id = Queue::instance_count++;
    this->rate = rate; // in bps
    this->limit_bytes = limit_bytes;
    this->bytes_in_queue = 0;
    this->busy = false;
    this->queue_proc_event = NULL;
    //this->packet_propagation_event = NULL;
    this->location = location;

    if (params.ddc != 0) {
        if (location == 0) {
            this->propagation_delay = 10e-9;
        }
        else if (location == 1 || location == 2) {
            this->propagation_delay = 400e-9;
        }
        else if (location == 3) {
            this->propagation_delay = 210e-9;
        }
        else {
            assert(false);
        }
    }
    else {
        this->propagation_delay = params.propagation_delay;
    }
    this->p_arrivals = 0; this->p_departures = 0;
    this->b_arrivals = 0; this->b_departures = 0;

    this->pkt_drop = 0;
    this->spray_counter=std::rand();
    this->packet_transmitting = NULL;
}

void Queue::set_src_dst(Node *src, Node *dst) {
    this->src = src;
    this->dst = dst;
}

void Queue::enque(Packet *packet) {
    p_arrivals += 1;
    b_arrivals += packet->size;
    if (bytes_in_queue + packet->size <= limit_bytes) {
        packets.push_back(packet);
        bytes_in_queue += packet->size;
    } else {
        pkt_drop++;
        drop(packet);
    }
}

Packet *Queue::deque() {
    return bytes_in_queue == 0 ? NULL : extractPacket(peek());
}

void Queue::drop(Packet *packet) {
    if (location == 0)
    {
        packet->flow->pkt_drop_init++;
    }
    packet->flow->pkt_drop++;

    if(packet->seq_no < packet->flow->size){
        packet->flow->data_pkt_drop++;
    }
    if(packet->type == ACK_PACKET)
        packet->flow->ack_pkt_drop++;

    if (location != 0 && packet->type == NORMAL_PACKET) {
        dead_packets += 1;
    }

    if(debug_flow(packet->flow->id))
        std::cout << get_current_time() << " pkt drop. flow:" << packet->flow->id
            << " type:" << packet->type << " seq:" << packet->seq_no
            << " at queue id:" << this->id << " loc:" << this->location << "\n";

    delete packet;
}

double Queue::get_transmission_delay(uint32_t size) {
    return size * 8.0 / rate;
}

void Queue::preempt_current_transmission() {
    if(params.preemptive_queue && busy){
        this->queue_proc_event->cancelled = true;
        assert(this->packet_transmitting);

        uint delete_index;
        bool found = false;
        for (delete_index = 0; delete_index < packets.size(); delete_index++) {
            if (packets[delete_index] == this->packet_transmitting) {
                found = true;
                break;
            }
        }
        if(found){
            bytes_in_queue -= packet_transmitting->size;
            packets.erase(packets.begin() + delete_index);
        }

        for(uint i = 0; i < busy_events.size(); i++){
            busy_events[i]->cancelled = true;
        }
        busy_events.clear();
        //drop(packet_transmitting);//TODO: should be put back to queue
        enque(packet_transmitting);
        packet_transmitting = NULL;
        queue_proc_event = NULL;
        busy = false;
    }
}

Packet* Queue::peek() {
    if (bytes_in_queue > 0) {
        uint32_t best_index = 0;
        for (uint32_t i = 0; i < packets.size(); i++) {
            Packet* curr_pkt = packets[i];
            if (comparePackets(curr_pkt, packets[best_index]) < 0)
            {
                best_index = i;
            }
        }
        return packets[best_index];
    }
    return nullptr;
}

void Queue::sort(std::vector<std::pair<Packet*, Queue*>> &packetOrder) {
    std::sort(packetOrder.begin(), packetOrder.end(), [this](const std::pair<Packet *, Queue *>& p1, const std::pair<Packet *, Queue *>& p2)
    {
        if (comparePackets(p1.first, p2.first) == 0)
        {
            return p1.first->last_enque_time < p2.first->last_enque_time;
        }
        else
        {
            return comparePackets(p1.first, p2.first) == -1;
        }
    });
}

void Queue::clearByIndex(int index) {
    Packet* p = packets[index];
    bytes_in_queue -= p->size;
    packets.erase(packets.begin() + index);

    p_departures += 1;
    b_departures += p->size;

    p->total_queuing_delay += get_current_time() - p->last_enque_time;

    if(p->type ==  NORMAL_PACKET){
        if(p->flow->first_byte_send_time < 0)
            p->flow->first_byte_send_time = get_current_time();
        if(this->location == 0)
            p->flow->first_hop_departure++;
        if(this->location == 3)
            p->flow->last_hop_departure++;
    }
}

void Queue::registerInsert(Packet *packet) {
    p_arrivals += 1;
    b_arrivals += packet->size;
    packets.push_back(packet);
    bytes_in_queue += packet->size;
    packet->last_enque_time = get_current_time();
}

void Queue::dropLeft() {
    if (bytes_in_queue > limit_bytes) {
        uint32_t worst_index = 0;
        for (uint32_t i = 0; i < packets.size(); i++) {
            if (comparePackets(packets[i], packets[worst_index]) >=0)
            {
                worst_index = i;
            }
        }
        bytes_in_queue -= packets[worst_index]->size;
        Packet *worst_packet = packets[worst_index];

        packets.erase(packets.begin() + worst_index);
        pkt_drop++;
        drop(worst_packet);
    }
}

void Queue::unimplemented() {
    while (true)
    {
        std::cerr << "Unimplemented";
    }
}

int Queue::comparePackets(Packet *p1, Packet *p2) {
    unimplemented();
}

long long Queue::estimate(Packet *packet) {
    unimplemented();
}


Packet *Queue::extractPacket(Packet *packet) {
    unimplemented();
}