#include "algorithms/BellmanFordRoute.h"
#include "AlgorithmUtils.h"

#include <limits>
#include <algorithm>

RouteResult BellmanFordRouteAlgorithm::computeRoute(const MapGraph& graph,
                                                     const std::vector<int>& eligibleIds,
                                                     int hqId) const {
    const auto startTime = AlgorithmUtils::RouteClock::now();

    RouteResult result = AlgorithmUtils::makeBaseResult(algorithmName());

    if (eligibleIds.empty()) {
        result.runtimeMs = 0.0;
        return result;
    }

    const std::vector<int> nodeIds =
        AlgorithmUtils::buildWorkingNodeIds(eligibleIds, hqId);
    const int nodeCount = static_cast<int>(nodeIds.size());

    // Build local distance matrix (index 0 = HQ).
    std::vector<std::vector<float>> dist(
        nodeCount, std::vector<float>(nodeCount, 0.0f));
    for (int i = 0; i < nodeCount; ++i) {
        for (int j = 0; j < nodeCount; ++j) {
            if (i != j) {
                dist[i][j] = graph.getDistance(nodeIds[i], nodeIds[j]);
            }
        }
    }

    // Bellman-Ford from HQ (local index 0).
    // Relaxes every edge (i→j) for |V|-1 iterations.
    // Works with any edge weights — correct even if weights were negative
    // (they aren't here, but the algorithm is used faithfully regardless).
    const float kInf = std::numeric_limits<float>::max() / 2.0f;
    std::vector<float> shortestDist(nodeCount, kInf);
    shortestDist[0] = 0.0f;

    for (int iter = 0; iter < nodeCount - 1; ++iter) {
        bool anyRelaxed = false;
        for (int u = 0; u < nodeCount; ++u) {
            if (shortestDist[u] >= kInf) continue;
            for (int v = 0; v < nodeCount; ++v) {
                if (u == v || dist[u][v] <= 0.0f) continue;
                const float tentative = shortestDist[u] + dist[u][v];
                if (tentative < shortestDist[v]) {
                    shortestDist[v] = tentative;
                    anyRelaxed = true;
                }
            }
        }
        if (!anyRelaxed) break;  // converged early
    }

    // Route construction: cheapest insertion.
    //
    // Starts with the trivial tour [HQ, HQ] and iteratively inserts the
    // unvisited eligible node that adds the least extra cost:
    //   delta(i, k, j) = dist[i][k] + dist[k][j] - dist[i][j]
    //
    // This considers how each candidate disrupts the whole tour rather than
    // just the next hop, producing tighter routes than nearest-neighbour.
    std::vector<int> tour = {0, 0};  // local indices, starts as HQ→HQ
    std::vector<bool> inserted(nodeCount, false);
    inserted[0] = true;

    const int eligibleCount = nodeCount - 1;  // everything except HQ
    for (int step = 0; step < eligibleCount; ++step) {
        float bestDelta = std::numeric_limits<float>::max();
        int bestNode = -1;
        int bestPos = -1;

        // Evaluate inserting each unvisited node at each gap in the tour.
        for (int k = 1; k < nodeCount; ++k) {
            if (inserted[k]) continue;

            for (int pos = 0; pos + 1 < static_cast<int>(tour.size()); ++pos) {
                const int i = tour[pos];
                const int j = tour[pos + 1];
                const float delta = dist[i][k] + dist[k][j] - dist[i][j];
                if (delta < bestDelta) {
                    bestDelta = delta;
                    bestNode = k;
                    bestPos = pos + 1;
                }
            }
        }

        if (bestNode < 0) break;
        tour.insert(tour.begin() + bestPos, bestNode);
        inserted[bestNode] = true;
    }

    // Convert local indices to actual node IDs.
    result.visitOrder.reserve(tour.size());
    for (const int localIdx : tour) {
        result.visitOrder.push_back(nodeIds[localIdx]);
    }

    AlgorithmUtils::finalizeRuntime(result, startTime);
    return result;
}

std::string BellmanFordRouteAlgorithm::algorithmName() const {
    return "Bellman-Ford Route";
}

std::string BellmanFordRouteAlgorithm::description() const {
    return "Bellman-Ford shortest paths + cheapest-insertion tour construction";
}
