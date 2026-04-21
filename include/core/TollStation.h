#ifndef TOLL_STATION_H
#define TOLL_STATION_H

#include <string>

// A toll booth placed on the road between two nodes.
// When a route travels the edge nodeA<->nodeB, the fee is added to route cost.
struct TollStation {
    int nodeA;
    int nodeB;
    float fee;         // RM charged when crossing this toll
    std::string name;

    TollStation(int a, int b, float f, const std::string& n)
        : nodeA(a), nodeB(b), fee(f), name(n) {}

    // Returns true if the directed edge from->to crosses this toll (either direction)
    bool isCrossedBy(int from, int to) const {
        return (from == nodeA && to == nodeB) || (from == nodeB && to == nodeA);
    }
};

#endif // TOLL_STATION_H
