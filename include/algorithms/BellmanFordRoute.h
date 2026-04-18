#ifndef BELLMAN_FORD_ROUTE_H
#define BELLMAN_FORD_ROUTE_H

#include "RouteAlgorithm.h"

// Bellman-Ford shortest paths + cheapest-insertion tour construction.
//
// Theory: Bellman-Ford computes single-source shortest paths by relaxing every
// edge |V|-1 times. Unlike Dijkstra it handles negative edge weights (none here,
// but the algorithm is still correct and demonstrates a different complexity class:
// O(V * E) versus Dijkstra's O((V + E) log V)).
//
// Route construction (cheapest insertion):
//   1. Run Bellman-Ford from HQ to obtain shortest-path distances.
//   2. Start with the trivial tour: HQ → HQ.
//   3. Repeatedly find the unvisited eligible node whose insertion into the
//      current tour causes the smallest cost increase, and insert it at the
//      cheapest gap. Insertion cost for node k between consecutive stops i→j:
//        delta = dist[i][k] + dist[k][j] - dist[i][j]
//
// Cheapest insertion typically produces tighter tours than nearest-neighbour
// (Greedy) because it considers the effect of each new node on the whole
// tour rather than just the next hop.
class BellmanFordRouteAlgorithm : public RouteAlgorithm {
public:
    RouteResult computeRoute(const MapGraph& graph,
                             const std::vector<int>& eligibleIds,
                             int hqId) const override;

    std::string algorithmName() const override;
    std::string description() const override;
};

#endif // BELLMAN_FORD_ROUTE_H
