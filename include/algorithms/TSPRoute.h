#ifndef TSP_ROUTE_H
#define TSP_ROUTE_H

#include "RouteAlgorithm.h"

// Exact TSP solver using bitmask dynamic programming.
//
// This is the most computationally intensive algorithm in the system.
// It finds the truly optimal route that starts at HQ, visits all
// eligible nodes, and returns to HQ with minimum total distance.
//
// Complexity is O(n^2 * 2^n) where n is the number of nodes to visit.
// With our 8-10 eligible nodes this means at most ~102,400 states,
// which runs in well under a second on any modern machine.
//
// The bitmask encodes which nodes have been visited:
//   bit i set = node i has been visited
// dp[mask][i] = minimum distance to reach node i having visited the set 'mask'
class TSPRouteAlgorithm : public RouteAlgorithm {
public:
    RouteResult computeRoute(const MapGraph& graph,
                             const std::vector<int>& eligibleIds,
                             int hqId) const override;

    std::string algorithmName() const override;
    std::string description() const override;

private:
    // Reconstruct the optimal path from the DP table
    std::vector<int> reconstructPath(
        const std::vector<std::vector<float>>& dp,
        const std::vector<std::vector<int>>& parent,
        const std::vector<std::vector<float>>& dist,
        int n, int startIdx) const;
};

#endif // TSP_ROUTE_H
