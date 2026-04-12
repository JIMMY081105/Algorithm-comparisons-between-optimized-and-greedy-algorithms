#include "algorithms/GreedyRoute.h"
#include "AlgorithmUtils.h"

#include <algorithm>
#include <limits>

RouteResult GreedyRouteAlgorithm::computeRoute(const MapGraph& graph,
                                               const std::vector<int>& eligibleIds,
                                               int hqId) const {
    const auto startTime = AlgorithmUtils::RouteClock::now();

    RouteResult result = AlgorithmUtils::makeBaseResult(algorithmName());

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

    const std::vector<int> nodeIds =
        AlgorithmUtils::buildWorkingNodeIds(eligibleIds, hqId);
    std::vector<bool> visited(nodeIds.size(), false);
    visited.front() = true;

    result.visitOrder.push_back(hqId);
    int currentId = hqId;

    // Keep picking the nearest unvisited eligible node
    for (std::size_t step = 1; step < nodeIds.size(); ++step) {
        float bestDistance = std::numeric_limits<float>::max();
        int bestNodeIndex = -1;

        for (std::size_t candidateIndex = 1; candidateIndex < nodeIds.size(); ++candidateIndex) {
            if (visited[candidateIndex]) {
                continue;
            }

            const float dist = graph.getDistance(currentId, nodeIds[candidateIndex]);
            if (dist < bestDistance) {
                bestDistance = dist;
                bestNodeIndex = static_cast<int>(candidateIndex);
            }
        }

        if (bestNodeIndex < 0) {
            break;
        }

        const int nextNodeId = nodeIds[bestNodeIndex];
        result.visitOrder.push_back(nextNodeId);
        visited[bestNodeIndex] = true;
        currentId = nextNodeId;
    }

    // Return to HQ to complete the circuit
    result.visitOrder.push_back(hqId);
    AlgorithmUtils::finalizeRuntime(result, startTime);

    return result;
}

std::string GreedyRouteAlgorithm::algorithmName() const {
    return "Greedy Route";
}

std::string GreedyRouteAlgorithm::description() const {
    return "Nearest-neighbor heuristic — always picks the closest unvisited node";
}
