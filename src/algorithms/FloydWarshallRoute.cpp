#include "algorithms/FloydWarshallRoute.h"
#include "AlgorithmUtils.h"

#include <limits>
#include <algorithm>

RouteResult FloydWarshallRouteAlgorithm::computeRoute(const MapGraph& graph,
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

    // Floyd-Warshall all-pairs shortest paths.
    // Initialize with direct edge weights, then relax through every intermediate k.
    // This is the full O(n^3) computation — every pair of nodes gets the true
    // shortest-path distance, not just the direct edge.
    const float kInf = std::numeric_limits<float>::max() / 2.0f;
    std::vector<std::vector<float>> fw(nodeCount,
                                       std::vector<float>(nodeCount, kInf));
    for (int i = 0; i < nodeCount; ++i) {
        fw[i][i] = 0.0f;
        for (int j = 0; j < nodeCount; ++j) {
            if (i != j) {
                fw[i][j] = graph.getEffectiveDistance(nodeIds[i], nodeIds[j]);
            }
        }
    }

    for (int k = 0; k < nodeCount; ++k) {
        for (int i = 0; i < nodeCount; ++i) {
            if (fw[i][k] >= kInf) continue;
            for (int j = 0; j < nodeCount; ++j) {
                const float through = fw[i][k] + fw[k][j];
                if (through < fw[i][j]) {
                    fw[i][j] = through;
                }
            }
        }
    }

    // Route construction: farthest insertion.
    //
    // Builds the tour "from the extremes inward":
    //   1. Start with HQ → the eligible node farthest from HQ → HQ.
    //   2. Each step: find the unvisited eligible node that is FARTHEST from
    //      any node currently in the tour (max of min-distances to tour members).
    //   3. Insert that node at whichever gap in the current tour is cheapest.
    //
    // Farthest insertion tends to produce a large bounding tour first, then
    // fills in interior nodes — a structurally different shape from the
    // nearest-first approaches (Greedy, Dijkstra) and different from
    // cheapest-insertion (Bellman-Ford).
    std::vector<int> tour;
    std::vector<bool> inserted(nodeCount, false);
    inserted[0] = true;

    // Step 1: seed with HQ and the farthest eligible node from HQ.
    float maxDistFromHQ = -1.0f;
    int farthestLocal = 1;
    for (int i = 1; i < nodeCount; ++i) {
        if (fw[0][i] > maxDistFromHQ) {
            maxDistFromHQ = fw[0][i];
            farthestLocal = i;
        }
    }
    tour = {0, farthestLocal, 0};
    inserted[farthestLocal] = true;

    // Steps 2+: farthest-unvisited insertion.
    const int eligibleCount = nodeCount - 1;
    for (int step = 1; step < eligibleCount; ++step) {
        // Find unvisited node k with the largest min-distance to any tour member.
        float farthestScore = -1.0f;
        int farthestNode = -1;

        for (int k = 1; k < nodeCount; ++k) {
            if (inserted[k]) continue;

            float minDist = std::numeric_limits<float>::max();
            for (const int t : tour) {
                minDist = std::min(minDist, fw[t][k]);
            }
            if (minDist > farthestScore) {
                farthestScore = minDist;
                farthestNode = k;
            }
        }

        if (farthestNode < 0) break;

        // Insert farthestNode at the cheapest gap.
        float bestDelta = std::numeric_limits<float>::max();
        int bestPos = 1;

        for (int pos = 0; pos + 1 < static_cast<int>(tour.size()); ++pos) {
            const int i = tour[pos];
            const int j = tour[pos + 1];
            const float delta = fw[i][farthestNode] + fw[farthestNode][j] - fw[i][j];
            if (delta < bestDelta) {
                bestDelta = delta;
                bestPos = pos + 1;
            }
        }

        tour.insert(tour.begin() + bestPos, farthestNode);
        inserted[farthestNode] = true;
    }

    // Convert local indices to actual node IDs.
    result.visitOrder.reserve(tour.size());
    for (const int localIdx : tour) {
        result.visitOrder.push_back(nodeIds[localIdx]);
    }

    AlgorithmUtils::finalizeRuntime(result, startTime);
    return result;
}

std::string FloydWarshallRouteAlgorithm::algorithmName() const {
    return "Floyd-Warshall Route";
}

std::string FloydWarshallRouteAlgorithm::description() const {
    return "Floyd-Warshall all-pairs shortest paths + farthest-insertion tour construction";
}
