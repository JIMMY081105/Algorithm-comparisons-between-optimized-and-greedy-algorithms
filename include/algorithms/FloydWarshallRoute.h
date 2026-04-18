#ifndef FLOYD_WARSHALL_ROUTE_H
#define FLOYD_WARSHALL_ROUTE_H

#include "RouteAlgorithm.h"

// Floyd-Warshall all-pairs shortest paths + farthest-insertion tour construction.
//
// Theory: Floyd-Warshall computes shortest distances between every pair of nodes
// in O(n^3) via dynamic programming:
//   dist[i][j] = min(dist[i][j], dist[i][k] + dist[k][j])  for all k.
// Unlike Dijkstra/Bellman-Ford it answers all-pairs queries in one pass, which
// makes it natural for route problems that need inter-node distances globally.
//
// Route construction (farthest insertion):
//   1. Run Floyd-Warshall to get all-pairs distances.
//   2. Start tour: HQ → the eligible node farthest from HQ → HQ.
//   3. Repeatedly select the unvisited eligible node that is *farthest* from any
//      node already in the tour (max of min-distances to tour members), then
//      insert it at the cheapest gap in the current route.
//
// Farthest insertion tends to build tours from the "extremes inward", which
// produces a different shape than Greedy/Dijkstra (nearest-first) approaches.
// It is well-studied as a heuristic with a known 2× approximation ratio for
// Euclidean instances.
class FloydWarshallRouteAlgorithm : public RouteAlgorithm {
public:
    RouteResult computeRoute(const MapGraph& graph,
                             const std::vector<int>& eligibleIds,
                             int hqId) const override;

    std::string algorithmName() const override;
    std::string description() const override;
};

#endif // FLOYD_WARSHALL_ROUTE_H
