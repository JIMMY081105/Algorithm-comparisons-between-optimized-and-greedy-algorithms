#ifndef DIJKSTRA_ROUTE_H
#define DIJKSTRA_ROUTE_H

#include "RouteAlgorithm.h"

// Dijkstra's single-source shortest path algorithm adapted for route planning.
//
// Theory: Dijkstra computes minimum cumulative distance from a source node to
// every other node using a greedy priority-queue expansion.
//
// Route construction: after computing shortest-path distances from HQ to every
// eligible node, we visit them in increasing distance order — closest-to-depot
// first. On a complete Euclidean graph (triangle inequality holds) the shortest
// path to each node is always the direct edge, so this produces a
// "radial from depot" ordering that is provably distinct from Greedy:
//
//   - Greedy: at each step picks nearest to the *current truck position*.
//   - Dijkstra: visits in fixed order of distance from *HQ*, regardless of
//               where the truck is after previous stops.
//
// The difference becomes visible once the truck has moved away from HQ —
// Dijkstra may double back to visit a nearby-HQ node, while Greedy would
// continue outward.
class DijkstraRouteAlgorithm : public RouteAlgorithm {
public:
    RouteResult computeRoute(const MapGraph& graph,
                             const std::vector<int>& eligibleIds,
                             int hqId) const override;

    std::string algorithmName() const override;
    std::string description() const override;
};

#endif // DIJKSTRA_ROUTE_H
