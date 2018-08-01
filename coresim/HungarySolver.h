#include <vector>
#include "packet.h"

#ifndef SIMULATOR_HUNGARYSOLVER_H
#define SIMULATOR_HUNGARYSOLVER_H

#endif //SIMULATOR_HUNGARYSOLVER_H

class Packet;
class CoreBigSwitch;

struct HungarySolver
{
    HungarySolver(CoreBigSwitch* bigSwitch);
    std::vector<std::pair<Packet*, Queue*>> generateAns();
    void runSolver();
    std::vector<std::vector<long long>> matr;
    std::vector<std::vector<Packet*>> packets;
    std::vector<int> ans;
    int n;
    CoreBigSwitch* bigSwitch;
};
