#include "HungarySolver.h"
#include "../run/flow_generator.h"

HungarySolver::HungarySolver(CoreBigSwitch* bigSwitch) {
    n = bigSwitch->innerQueues.size();
    this->bigSwitch = bigSwitch;
    for (int i = 0; i <= n; i++)
    {
        matr.emplace_back(n+1, (long long)1e+12);
        packets.emplace_back(n+1, nullptr);
        ans.push_back(i);
    }

    std::vector<Queue*> outerQueues;
    std::copy(bigSwitch->queues.begin(), bigSwitch->queues.end(), std::back_inserter(outerQueues));

    for (int i = 0; i < bigSwitch->innerQueues.size(); i++)
    {
        for (int j = 0; j < bigSwitch->queues.size(); j++)
        {
            if (params.queue_type == 7)
            {
                static auto rng3 = std::default_random_engine {47};
                std::shuffle(bigSwitch->innerQueues[i]->packets.begin(), bigSwitch->innerQueues[i]->packets.end(), rng3);
            }
            for (Packet* p : bigSwitch->innerQueues[i]->packets)
            {
                if (topology->get_next_hop(p,bigSwitch->innerQueues[i]) == bigSwitch->queues[j])
                {
                    if (packets[i][j] != nullptr && params.queue_type == 7)
                    {
                        continue;
                    }
                    if (packets[i][j] == nullptr || bigSwitch->innerQueues[i]->comparePackets(p, packets[i][j]) < 0)
                    {
                        packets[i][j] = p;
                        matr[i+1][j+1] = bigSwitch->innerQueues[i]->estimate(p);
                    }
                }
            }
        }
    }
}


std::vector<std::pair<Packet *, Queue *>> HungarySolver::generateAns() {
    std::vector<std::pair<Packet *, Queue *>> result;
    for (int i = 0; i < n; i++)
    {
        if (packets[i][ans[i]])
        {
            result.emplace_back(packets[i][ans[i]], bigSwitch->innerQueues[i]);
        }
    }
    return result;
}


void HungarySolver::runSolver() {
    long long INF = (long long) 1e+15;
    std::vector<long long> u(n+1), v(n+1), p (n+1), way (n+1);
    for (int i=1; i<=n; ++i) {
        p[0] = i;
        long long j0 = 0;
        std::vector<long long> minv (n+1, INF);
        std::vector<char> used (n+1, false);
        do {
            used[j0] = true;
            long long i0 = p[j0],  delta = INF,  j1;
            for (int j=1; j<=n; ++j)
            {
                if (!used[j]) {
                    long long cur = matr[i0][j]-u[i0]-v[j];
                    if (cur < minv[j])
                        minv[j] = cur,  way[j] = j0;
                    if (minv[j] < delta)
                        delta = minv[j],  j1 = j;
                }
            }
            for (int j=0; j<=n; ++j)
                if (used[j])
                    u[p[j]] += delta,  v[j] -= delta;
                else
                    minv[j] -= delta;
            j0 = j1;
        } while (p[j0] != 0);
        do {
            long long j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0);
    }
    for (int j=1; j<=n; ++j)
        ans[p[j]-1] = j-1;
}

