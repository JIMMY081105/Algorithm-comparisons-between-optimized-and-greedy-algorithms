#ifndef GREEDY_ROUTE_H
#define GREEDY_ROUTE_H

#include "RouteAlgorithm.h"

// Greedy nearest-neighbor heuristic.
// Starting from HQ, always picks the closest unvisited eligible node next.
// This is a classic heuristic that produces decent (but not optimal) routes.
// It tends to work well locally but can create inefficient long jumps
// when few nodes remain — which is exactly why comparing with TSP is valuable.
class GreedyRouteAlgorithm : public RouteAlgorithm {
public:
    RouteResult computeRoute(const MapGraph& graph,
                             const std::vector<int>& eligibleIds,
                             int hqId) override;

    std::string algorithmName() const override;
    std::string description() const override;
};

#endif // GREEDY_ROUTE_H
