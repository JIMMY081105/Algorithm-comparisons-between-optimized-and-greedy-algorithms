#include "algorithms/GreedyRoute.h"
#include <chrono>
#include <algorithm>
#include <limits>

RouteResult GreedyRouteAlgorithm::computeRoute(const MapGraph& graph,
                                                const std::vector<int>& eligibleIds,
                                                int hqId) {
    auto startTime = std::chrono::high_resolution_clock::now();

    RouteResult result;
    result.algorithmName = algorithmName();

    if (eligibleIds.empty()) {
        result.runtimeMs = 0.0;
        return result;
    }

    // Greedy nearest-neighbor: from the current position, always go to
    // the closest unvisited eligible node. This is a well-known heuristic
    // that produces reasonable routes quickly but doesn't guarantee optimality.
    //
    // The approach works like this:
    //   1. Start at HQ
    //   2. Look at all unvisited eligible nodes
    //   3. Pick the one with shortest distance from current position
    //   4. Move there and mark it visited
    //   5. Repeat until all eligible nodes are visited
    //   6. Return to HQ

    // Track which eligible nodes we still need to visit
    std::vector<bool> visited(graph.getNodeCount(), false);
    int hqIdx = graph.findNodeIndex(hqId);
    visited[hqIdx] = true;

    result.visitOrder.push_back(hqId);
    int currentId = hqId;

    // Keep picking the nearest unvisited eligible node
    for (size_t step = 0; step < eligibleIds.size(); step++) {
        float bestDist = std::numeric_limits<float>::max();
        int bestId = -1;

        for (int candidateId : eligibleIds) {
            int candidateIdx = graph.findNodeIndex(candidateId);
            if (candidateIdx < 0 || visited[candidateIdx]) continue;

            float dist = graph.getDistance(currentId, candidateId);
            if (dist < bestDist) {
                bestDist = dist;
                bestId = candidateId;
            }
        }

        if (bestId == -1) break;  // no more reachable unvisited nodes

        result.visitOrder.push_back(bestId);
        visited[graph.findNodeIndex(bestId)] = true;
        currentId = bestId;
    }

    // Return to HQ to complete the circuit
    result.visitOrder.push_back(hqId);

    auto endTime = std::chrono::high_resolution_clock::now();
    result.runtimeMs = std::chrono::duration<double, std::milli>(
        endTime - startTime).count();

    return result;
}

std::string GreedyRouteAlgorithm::algorithmName() const {
    return "Greedy Route";
}

std::string GreedyRouteAlgorithm::description() const {
    return "Nearest-neighbor heuristic — always picks the closest unvisited node";
}
