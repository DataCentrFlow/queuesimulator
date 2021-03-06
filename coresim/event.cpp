#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <chrono>

#include "event.h"
#include "packet.h"
#include "topology.h"
#include "debug.h"


#include "../ext/factory.h"

#include "../run/params.h"

#include "HungarySolver.h"

extern Topology* topology;
extern std::priority_queue<Event*, std::vector<Event*>, EventComparator> event_queue;
extern double current_time;
extern DCExpParams params;
extern std::deque<Event*> flow_arrivals;
extern std::deque<Flow*> flows_to_schedule;

extern uint32_t num_outstanding_packets;
extern uint32_t max_outstanding_packets;

extern uint32_t num_outstanding_packets_at_50;
extern uint32_t num_outstanding_packets_at_100;
extern uint32_t arrival_packets_at_50;
extern uint32_t arrival_packets_at_100;
extern uint32_t arrival_packets_count;
extern uint32_t total_finished_flows;

extern uint32_t backlog3;
extern uint32_t backlog4;
extern uint32_t duplicated_packets_received;
extern uint32_t duplicated_packets;
extern uint32_t injected_packets;
extern uint32_t completed_packets;
extern uint32_t total_completed_packets;
extern uint32_t dead_packets;
extern uint32_t sent_packets;

extern EmpiricalRandomVariable *nv_bytes;

extern double get_current_time();
extern void add_to_event_queue(Event *);
extern int get_event_queue_size();

uint32_t Event::instance_count = 0;

Event::Event(uint32_t type, double time) {
    this->type = type;
    this->time = time;
    this->cancelled = false;
    this->unique_id = Event::instance_count++;
}

Event::~Event() {
}

/* Flow Arrival */
FlowCreationForInitializationEvent::FlowCreationForInitializationEvent(
        double time, 
        Host *src, 
        Host *dst,
        EmpiricalRandomVariable *nv_bytes, 
        RandomVariable *nv_intarr
    ) : Event(FLOW_CREATION_EVENT, time) {
    this->src = src;
    this->dst = dst;
    this->nv_bytes = nv_bytes;
    this->nv_intarr = nv_intarr;
}

FlowCreationForInitializationEvent::~FlowCreationForInitializationEvent() {}

void FlowCreationForInitializationEvent::process_event() {
    uint32_t nvVal, size;
    uint32_t id = flows_to_schedule.size();
    if (params.bytes_mode) {
        nvVal = nv_bytes->value();
        size = (uint32_t) nvVal;
    } else {
        nvVal = (nv_bytes->value() + 0.5); // truncate(val + 0.5) equivalent to round to nearest int
        if (nvVal > 2500000) {
            std::cout << "Giant Flow! event.cpp::FlowCreation:" << 1000000.0 * time << " Generating new flow " << id << " of size " << (nvVal*1460) << " between " << src->id << " " << dst->id << "\n";
            nvVal = 2500000;
        }
        size = (uint32_t) nvVal * 1460;
    }

    if (size != 0) {
        flows_to_schedule.push_back(Factory::get_flow(id, time, size, src, dst, params.flow_type));
        const auto& flow = flows_to_schedule.back();

        std::vector<double> times(nvVal);
        times[0] = time;
        int cl = nvVal / 3 + 1;
        int c = (rand()%(params.flow_split_mode+1)+3*(params.flow_split_mode+1))%(params.flow_split_mode + 1);

        //std::cerr << params.mean_flow_size / params.mss;
        //assert(0);

        double alpha = 0.1*cl*c/params.mean_flow_size*195357;
        if (!params.permutation_tm)
        {
            alpha = alpha/(topology->hosts.size()-1);
        }


        for (int l = 1;  l < nvVal; l++)
        {
            times[l] = times[l-1] + alpha * nv_intarr->value() / (nvVal-1);
            assert(times[l] > times[l-1]-1e-9);
        }

        for (int l = 0; l < nvVal; l++)
        {
            flow->dataArrivalEvents.push_back(new FlowDataArrival(times[l], flow, params.mss));
            flow->max_data_arrival_time = times[l];
            if (c==0)
            {
                flow->dataArrivalEvents.back()->size = nvVal *params.mss;
                break;
            }
        }

        flow->min_data_arrival_time = times[0];
        flow->recalculateMinWorkTime(topology);
        if (flow->max_data_arrival_time - flow->min_data_arrival_time > flow->min_work_time + 1e-10)
        {
            assert(false);
        }
    }

//        std::cout << "event.cpp::FlowCreation:" << 1000000.0 * time << " Generating new flow " << id << " of size "
//         << size << " between " <<src->id << " " << dst->id << " " << (tnext - get_current_time())*1e6 << "\n";

    add_to_event_queue(
            new FlowCreationForInitializationEvent(
                time +  nv_intarr->value()*params.congestion_compress,
                src,
                dst,
                nv_bytes,
                nv_intarr
                )
    );
}


/* Flow Arrival */
int flow_arrival_count = 0;

FlowArrivalEvent::FlowArrivalEvent(double time, Flow* flow) : Event(FLOW_ARRIVAL, time) {
    this->flow = flow;
}

FlowArrivalEvent::~FlowArrivalEvent() {
}

void FlowArrivalEvent::process_event() {
    //Flows start at line rate; so schedule a packet to be transmitted
    //First packet scheduled to be queued

    num_outstanding_packets += (this->flow->size / this->flow->mss);
    arrival_packets_count += this->flow->size_in_pkt;
    if (num_outstanding_packets > max_outstanding_packets) {
        max_outstanding_packets = num_outstanding_packets;
    }
    this->flow->start_flow();
    flow_arrival_count++;
    for (const auto& ev : this->flow->dataArrivalEvents)
    {
        add_to_event_queue(ev);
    }
    if (flow_arrivals.size() > 0) {
        add_to_event_queue(flow_arrivals.front());
        flow_arrivals.pop_front();
    }


    if(params.num_flows_to_run > 10 && flow_arrival_count % 100000 == 0){
        double curr_time = get_current_time();
        uint32_t num_unfinished_flows = 0;
        for (uint32_t i = 0; i < flows_to_schedule.size(); i++) {
            Flow *f = flows_to_schedule[i];
            if (f->start_time < curr_time) {
                if (!f->finished) {
                    num_unfinished_flows ++;
                }
            }
        }
        if(flow_arrival_count == (int)(params.num_flows_to_run * 0.5))
        {
            arrival_packets_at_50 = arrival_packets_count;
            num_outstanding_packets_at_50 = num_outstanding_packets;
        }
        if(flow_arrival_count == params.num_flows_to_run)
        {
            arrival_packets_at_100 = arrival_packets_count;
            num_outstanding_packets_at_100 = num_outstanding_packets;
        }
//        std::cout << "## " << current_time << " NumPacketOutstanding " << num_outstanding_packets
//            << " NumUnfinishedFlows " << num_unfinished_flows << " StartedFlows " << flow_arrival_count
//            << " StartedPkts " << arrival_packets_count << "\n";
    }
}


/* Packet Queuing */
PacketQueuingEvent::PacketQueuingEvent(double time, Packet *packet,
        Queue *queue) : Event(PACKET_QUEUING, time) {
    this->packet = packet;
    this->queue = queue;
}

PacketQueuingEvent::~PacketQueuingEvent() {
}

void PacketQueuingEvent::process_event() {
    if (params.big_switch && packet->type != NORMAL_PACKET && !params.permutation_tm)
    {
        PacketArrivalEvent* ev = new PacketArrivalEvent(time + 2*queue->propagation_delay+2*queue->get_transmission_delay(packet->flow->hdr_size), packet);
        add_to_event_queue(ev);
        return;
    }

    if (params.big_switch && queue->src->type == HOST && !params.permutation_tm)
    {
        queue->enque(packet);
        return;
    }

    if (params.big_switch && queue->src->type != HOST && !params.permutation_tm)
    {
        double td = queue->get_transmission_delay(packet->size);
        double pd = queue->propagation_delay;
        Event* arrival_evt = new PacketArrivalEvent(time + td + pd, packet);
        add_to_event_queue(arrival_evt);
        return;
    }

    if (!queue->busy) {
        queue->queue_proc_event = new QueueProcessingEvent(get_current_time(), queue);
        add_to_event_queue(queue->queue_proc_event);
        queue->busy = true;
        queue->packet_transmitting = packet;
    }
    else if( params.preemptive_queue && this->packet->pf_priority < queue->packet_transmitting->pf_priority) {
        double remaining_percentage = (queue->queue_proc_event->time - get_current_time()) / queue->get_transmission_delay(queue->packet_transmitting->size);

        if(remaining_percentage > 0.01){
            queue->preempt_current_transmission();

            queue->queue_proc_event = new QueueProcessingEvent(get_current_time(), queue);
            add_to_event_queue(queue->queue_proc_event);
            queue->busy = true;
            queue->packet_transmitting = packet;
        }
    }

    queue->enque(packet);
}

/* Packet Arrival */
PacketArrivalEvent::PacketArrivalEvent(double time, Packet *packet)
    : Event(PACKET_ARRIVAL, time) {
        this->packet = packet;
    }

PacketArrivalEvent::~PacketArrivalEvent() {
}

void PacketArrivalEvent::process_event() {
    if (packet->type == NORMAL_PACKET) {
        completed_packets++;
    }

    packet->flow->receive(packet);
}


/* Queue Processing */
QueueProcessingEvent::QueueProcessingEvent(double time, Queue *queue)
    : Event(QUEUE_PROCESSING, time) {
        this->queue = queue;
}

QueueProcessingEvent::~QueueProcessingEvent() {
    if (queue->queue_proc_event == this) {
        queue->queue_proc_event = NULL;
        queue->busy = false; //TODO is this ok??
    }
}

void QueueProcessingEvent::process_event() {
//    std::cerr << "wel\n";
//    while (true) {};
    Packet *packet = queue->deque();
    if (packet) {
        queue->busy = true;
        queue->busy_events.clear();
        queue->packet_transmitting = packet;
        Queue *next_hop = topology->get_next_hop(packet, queue);
        double td = queue->get_transmission_delay(packet->size);
        double pd = queue->propagation_delay;
        //double additional_delay = 1e-10;
        queue->queue_proc_event = new QueueProcessingEvent(time + td, queue);
        add_to_event_queue(queue->queue_proc_event);
        queue->busy_events.push_back(queue->queue_proc_event);
        if (next_hop == NULL) {
            Event* arrival_evt = new PacketArrivalEvent(time + td + pd, packet);
            add_to_event_queue(arrival_evt);
            queue->busy_events.push_back(arrival_evt);
        } else {
            Event* queuing_evt = NULL;
            if (params.cut_through == 1) {
                double cut_through_delay =
                    queue->get_transmission_delay(packet->flow->hdr_size);
                queuing_evt = new PacketQueuingEvent(time + cut_through_delay + pd, packet, next_hop);
            } else {
                queuing_evt = new PacketQueuingEvent(time + td + pd, packet, next_hop);
            }

            add_to_event_queue(queuing_evt);
            queue->busy_events.push_back(queuing_evt);
        }
    } else {
        queue->busy = false;
        queue->busy_events.clear();
        queue->packet_transmitting = NULL;
        queue->queue_proc_event = NULL;
    }
}


LoggingEvent::LoggingEvent(double time) : Event(LOGGING, time){
    this->ttl = 1e10;
}

LoggingEvent::LoggingEvent(double time, double ttl) : Event(LOGGING, time){
    this->ttl = ttl;
}

LoggingEvent::~LoggingEvent() {
}

void LoggingEvent::process_event() {
    double current_time = get_current_time();
    // can log simulator statistics here.
}


/* Flow Finished */
FlowFinishedEvent::FlowFinishedEvent(double time, Flow *flow)
    : Event(FLOW_FINISHED, time) {
        this->flow = flow;
    }

FlowFinishedEvent::~FlowFinishedEvent() {}


void FlowFinishedEvent::process_event() {
    this->flow->finished = true;
    this->flow->finish_time = get_current_time();
    this->flow->flow_completion_time = this->flow->finish_time - this->flow->start_time;
    total_finished_flows++;
    auto slowdown = 1000000 * flow->flow_completion_time / topology->get_oracle_fct(flow);
    if (slowdown < 1.0 && slowdown > 0.9999) {
        slowdown = 1.0;
    }
    if (slowdown < 1.0) {
        std::cout << "bad slowdown " << 1e6 * flow->flow_completion_time << " " << topology->get_oracle_fct(flow) << " " << slowdown << "\n";
    }
    assert(slowdown >= 1.0);

    if (false) {
        std::cout << std::setprecision(4) << std::fixed ;
        std::cout
            << flow->id << " "
            << flow->size << " "
            << flow->src->id << " "
            << flow->dst->id << " "
            << 1000000 * flow->start_time << " "
            << 1000000 * flow->finish_time << " "
            << 1000000.0 * flow->flow_completion_time << " "
            << topology->get_oracle_fct(flow) << " "
            << slowdown << " "
            << flow->total_pkt_sent << "/" << (flow->size/flow->mss) << "//" << flow->received_count << " "
            << flow->data_pkt_drop << "/" << flow->ack_pkt_drop << "/" << flow->pkt_drop << " "
            << 1000000 * (flow->first_byte_send_time - flow->start_time) << " "
            << std::endl;
        std::cout << std::setprecision(9) << std::fixed;
    }
}


/* Flow Processing */
FlowProcessingEvent::FlowProcessingEvent(double time, Flow *flow)
    : Event(FLOW_PROCESSING, time) {
        this->flow = flow;
    assert(false);
    }

FlowProcessingEvent::~FlowProcessingEvent() {
    if (flow->flow_proc_event == this) {
        flow->flow_proc_event = NULL;
    }
    assert(false);
}

void FlowProcessingEvent::process_event() {
    this->flow->send_pending_data();
    assert(false);
}


/* Retx Timeout */
RetxTimeoutEvent::RetxTimeoutEvent(double time, Flow *flow)
    : Event(RETX_TIMEOUT, time) {
        this->flow = flow;
    }

RetxTimeoutEvent::~RetxTimeoutEvent() {
    if (flow->retx_event == this) {
        flow->retx_event = NULL;
    }
}

void RetxTimeoutEvent::process_event() {
    flow->handle_timeout();
}

FlowDataArrival::FlowDataArrival(double time, Flow *flow, uint32_t size)
    : Event(FLOW_DATA_ARRIVAL, time)
{
    this->flow = flow;
    this->size = size;
}

FlowDataArrival::~FlowDataArrival() {
}

void FlowDataArrival::process_event() {
    this->flow->additional_size_coming(size);
}

BigSwitchProcessing::BigSwitchProcessing(double time, CoreBigSwitch *bigSwitch): Event(BIG_SWITCH_PROCESSING, time) {
    this->bigSwitch = bigSwitch;
}



std::vector<std::pair<Packet *, Queue *>> BigSwitchProcessing::selectBestCandidatesToSend() {
    std::vector<std::pair<Packet*, Queue*>> canBeApplied;
    for (Queue* queue : bigSwitch->innerQueues)
    {
        Packet* packet = queue->peek();
        if (packet != nullptr)
        {
            canBeApplied.emplace_back(packet, queue);
        }
    }
    bigSwitch->innerQueues[0]->sort(canBeApplied);
    return canBeApplied;
}


std::vector<std::pair<Packet *, Queue *>> BigSwitchProcessing::selectAllCandidatesToSend() {
    std::vector<std::pair<Packet*, Queue*>> canBeApplied;
    for (Queue* queue : bigSwitch->innerQueues)
    {
        for (Packet* packet : queue->packets) {
            canBeApplied.emplace_back(packet, queue);
        }
    }
    bigSwitch->innerQueues[0]->sort(canBeApplied);
    return canBeApplied;

}

std::vector<std::pair<Packet *, Queue *>> BigSwitchProcessing::selectNotGreedyMarriageOrder() {
    std::vector<std::pair<Packet*, Queue*>> canBeApplied;
    std::vector<std::vector<std::pair<Packet*, Queue*>>> sortedInQueues;
    std::vector<Queue*> innerQueues;
    std::copy(bigSwitch->innerQueues.begin(), bigSwitch->innerQueues.end(), std::back_inserter(innerQueues));
    static auto rng = std::default_random_engine {17};
    std::shuffle(std::begin(innerQueues), std::end(innerQueues), rng);

    for (Queue* queue : innerQueues)
    {
        std::vector<std::pair<Packet*, Queue*>> temp;
        for (Packet* packet : queue->packets)
        {
            temp.emplace_back(packet, queue);
        }
        bigSwitch->innerQueues[0]->sort(temp);
        std::copy(temp.begin(), temp.end(), std::back_inserter(canBeApplied));
    }

    return canBeApplied;
}


std::vector<std::pair<Packet *, Queue *>> BigSwitchProcessing::selectCandidatesToSendHungary() {
    HungarySolver solver(bigSwitch);
    solver.runSolver();
    return solver.generateAns();
}


std::vector<std::pair<Packet *, Queue *>> BigSwitchProcessing::selectCandidatesToSend() {
    if (params.marriage_type == 0)
    {
        return selectBestCandidatesToSend();
    }
    if (params.marriage_type == 1)
    {
        return selectAllCandidatesToSend();
    }
    if (params.marriage_type == 2)
    {
        return selectCandidatesToSendHungary();
    }
    return selectNotGreedyMarriageOrder();
}

std::vector<std::pair<Packet *, Queue *>> BigSwitchProcessing::selectBlockingToSend(const std::vector<std::pair<Packet *, Queue *>> &canBeApplied) {
    std::unordered_set<Queue*> used;
    std::vector<std::pair<Packet*, Queue*>> willBeApplied;
    for (const auto& a : canBeApplied)
    {

        if (params.marriage_type != 0 && (used.count(a.second) || used.count(topology->get_next_hop(a.first, a.second))))
        {
            continue;
        }

        used.insert(a.second);
        used.insert(topology->get_next_hop(a.first, a.second));
        willBeApplied.push_back(a);
    }
    return willBeApplied;
}


void BigSwitchProcessing::process_event() {
    double deltaTd = bigSwitch->innerQueues[0]->get_transmission_delay(1500);
    std::vector<std::pair<Packet*, Queue*>> canBeApplied = selectCandidatesToSend();
    auto  willBeApplied = selectBlockingToSend(canBeApplied);
    for (const auto& cand : willBeApplied)
    {
        Queue* queue = cand.second;
        Packet* packet = queue->extractPacket(cand.first);
        Queue *next_hop = topology->get_next_hop(packet, queue);
        double td = queue->get_transmission_delay(packet->size);
        double pd = queue->propagation_delay;
        assert(next_hop != NULL);
        Event* queuing_evt = NULL;
        if (params.cut_through == 1) {
            double cut_through_delay =
                    queue->get_transmission_delay(packet->flow->hdr_size);
            queuing_evt = new PacketQueuingEvent(time + cut_through_delay + pd, packet, next_hop);
        } else {
            queuing_evt = new PacketQueuingEvent(time + td + pd, packet, next_hop);
        }
        add_to_event_queue(queuing_evt);
    }

    if (total_finished_flows == flows_to_schedule.size())
    {
        return;
    }
    BigSwitchProcessing* ev =  new BigSwitchProcessing(time + deltaTd, bigSwitch);
    add_to_event_queue(ev);
}

BigSwitchProcessing::~BigSwitchProcessing() {

}




