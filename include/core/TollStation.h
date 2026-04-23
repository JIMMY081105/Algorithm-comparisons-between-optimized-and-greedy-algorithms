#ifndef TOLL_STATION_H
#define TOLL_STATION_H

#include <stdexcept>
#include <string>

// A toll booth placed on the road between two nodes.
// When a route travels the edge nodeA<->nodeB, the fee is added to route cost.
class TollStation {
private:
    int nodeA_;
    int nodeB_;
    float fee_;         // RM charged when crossing this toll
    std::string name_;

public:
    TollStation(int a, int b, float f, const std::string& n)
        : nodeA_(a), nodeB_(b), fee_(f), name_(n) {
        if (nodeA_ == nodeB_) {
            throw std::invalid_argument("TollStation endpoints must differ");
        }
        if (fee_ < 0.0f) {
            throw std::invalid_argument("TollStation fee cannot be negative");
        }
        if (name_.empty()) {
            throw std::invalid_argument("TollStation name cannot be empty");
        }
    }

    int fromNodeId() const { return nodeA_; }
    int toNodeId() const { return nodeB_; }
    float fee() const { return fee_; }
    const std::string& name() const { return name_; }

    // Returns true if the directed edge from->to crosses this toll (either direction)
    bool isCrossedBy(int from, int to) const {
        return (from == nodeA_ && to == nodeB_) || (from == nodeB_ && to == nodeA_);
    }
};

#endif // TOLL_STATION_H
