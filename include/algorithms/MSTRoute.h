#ifndef MST_ROUTE_H
#define MST_ROUTE_H

#include "RouteAlgorithm.h"

// MST-based routing using Prim's algorithm + the classic double-tree
// 2-approximation:
//   1. Build a Minimum Spanning Tree over HQ + the eligible nodes.
//   2. Walk an Eulerian circuit on the doubled MST (each edge twice).
//   3. Shortcut to a Hamiltonian circuit by skipping repeats.
//
// This produces a real, MST-derived collection route whose total cost is
// guaranteed to be at most 2x the optimal TSP tour. It is *intentionally*
// suboptimal compared to the exact TSP solver — that gap is the whole point
// of the comparison panel: it shows what you give up by routing off a
// structural minimisation rather than directly minimising travel.
class MSTRouteAlgorithm : public RouteAlgorithm {
public:
    RouteResult computeRoute(const MapGraph& graph,
                             const std::vector<int>& eligibleIds,
                             int hqId) const override;

    std::string algorithmName() const override;
    std::string description() const override;

private:
    // Prim's algorithm on the subgraph of relevant nodes. Returns the MST
    // as an undirected adjacency list indexed by local node position.
    std::vector<std::vector<int>> buildMST(const MapGraph& graph,
                                           const std::vector<int>& nodeIds) const;

    // Euler walk on the doubled MST starting at `start`. The walk records
    // every node visited, including back-tracks, so each MST edge is
    // traversed exactly twice (length = 2 * vertices - 1 for a tree).
    void buildEulerWalk(const std::vector<std::vector<int>>& mstAdj,
                        int start,
                        std::vector<int>& walk) const;
};

#endif // MST_ROUTE_H
