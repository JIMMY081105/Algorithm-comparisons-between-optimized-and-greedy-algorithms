#ifndef TSP_ROUTE_H
#define TSP_ROUTE_H

#include "RouteAlgorithm.h"

// TSP solver. Two paths depending on input size:
//
//   * n <= 12: exact bitmask dynamic programming (Held-Karp).
//     Complexity O(n^2 * 2^n) — at most ~50k states for n=12, fine in
//     well under a second. Returns the provably optimal Hamiltonian circuit.
//
//   * n > 12: nearest-neighbour seed + 2-opt local search until no
//     improving 2-edge swap remains. This is a textbook heuristic that
//     reliably improves on plain nearest-neighbour and so produces a
//     route that is *meaningfully different* from the Greedy algorithm
//     (the previous fallback collapsed to identical Greedy output).
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
