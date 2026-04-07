#ifndef REGULAR_ROUTE_H
#define REGULAR_ROUTE_H

#include "RouteAlgorithm.h"

// Baseline algorithm that visits eligible nodes in their default order.
// This represents the simplest possible approach — no optimization at all.
// The truck departs from HQ, visits each eligible node in ID order, and returns.
// Useful as a comparison baseline to show how much the other algorithms improve.
class RegularRouteAlgorithm : public RouteAlgorithm {
public:
    RouteResult computeRoute(const MapGraph& graph,
                             const std::vector<int>& eligibleIds,
                             int hqId) override;

    std::string algorithmName() const override;
    std::string description() const override;
};

#endif // REGULAR_ROUTE_H
