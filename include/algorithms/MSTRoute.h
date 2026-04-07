#ifndef MST_ROUTE_H
#define MST_ROUTE_H

#include "RouteAlgorithm.h"

// MST-based routing using Prim's algorithm.
// First constructs a Minimum Spanning Tree over the eligible nodes,
// then performs a DFS traversal to derive the visit order.
// The MST approach minimizes total edge weight (total road used),
// which is different from minimizing the travel path — but it
// provides a useful structural comparison against greedy and TSP.
class MSTRouteAlgorithm : public RouteAlgorithm {
public:
    RouteResult computeRoute(const MapGraph& graph,
                             const std::vector<int>& eligibleIds,
                             int hqId) override;

    std::string algorithmName() const override;
    std::string description() const override;

private:
    // Build MST using Prim's algorithm on the subgraph of relevant nodes.
    // Returns adjacency list of the MST.
    std::vector<std::vector<int>> buildMST(const MapGraph& graph,
                                           const std::vector<int>& nodeIds);

    // DFS traversal of the MST to produce a visit order
    void dfsTraversal(const std::vector<std::vector<int>>& mstAdj,
                      int current,
                      std::vector<bool>& visited,
                      std::vector<int>& order);
};

#endif // MST_ROUTE_H
