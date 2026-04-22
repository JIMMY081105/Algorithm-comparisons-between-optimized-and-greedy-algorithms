#include "algorithms/DijkstraRoute.h"
#include "AlgorithmUtils.h"

#include <limits>
#include <algorithm>
#include <queue>

RouteResult DijkstraRouteAlgorithm::computeRoute(const MapGraph& graph,
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

    // Build a local distance matrix (index 0 = HQ).
    std::vector<std::vector<float>> dist(
        nodeCount, std::vector<float>(nodeCount, 0.0f));
    for (int i = 0; i < nodeCount; ++i) {
        for (int j = 0; j < nodeCount; ++j) {
            if (i != j) {
                dist[i][j] = graph.getShortestPathDistance(nodeIds[i], nodeIds[j]);
            }
        }
    }

    // Dijkstra's algorithm from local index 0 (HQ).
    // shortestDist[i] = minimum cumulative distance from HQ to node i.
    const float kInf = std::numeric_limits<float>::max();
    std::vector<float> shortestDist(nodeCount, kInf);
    std::vector<bool> settled(nodeCount, false);
    shortestDist[0] = 0.0f;

    // Min-heap: (distance, local_index)
    using Entry = std::pair<float, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;
    pq.push({0.0f, 0});

    while (!pq.empty()) {
        const auto [d, u] = pq.top();
        pq.pop();

        if (settled[u]) continue;
        settled[u] = true;

        for (int v = 0; v < nodeCount; ++v) {
            if (v == u || dist[u][v] <= 0.0f) continue;
            const float tentative = shortestDist[u] + dist[u][v];
            if (tentative < shortestDist[v]) {
                shortestDist[v] = tentative;
                pq.push({tentative, v});
            }
        }
    }

    // Route construction: visit eligible nodes (indices 1..n-1) in order of
    // their Dijkstra distance from HQ — "closest-to-depot first".
    // This is the defining difference from Greedy: Greedy chooses by distance
    // from the truck's current stop; Dijkstra orders by distance from the depot.
    std::vector<int> eligibleLocal;
    eligibleLocal.reserve(nodeCount - 1);
    for (int i = 1; i < nodeCount; ++i) {
        eligibleLocal.push_back(i);
    }

    std::sort(eligibleLocal.begin(), eligibleLocal.end(),
              [&](int a, int b) {
                  return shortestDist[a] < shortestDist[b];
              });

    result.visitOrder.push_back(hqId);
    for (const int localIdx : eligibleLocal) {
        result.visitOrder.push_back(nodeIds[localIdx]);
    }
    result.visitOrder.push_back(hqId);

    AlgorithmUtils::finalizeRuntime(result, startTime);
    return result;
}

std::string DijkstraRouteAlgorithm::algorithmName() const {
    return "Dijkstra Route";
}

std::string DijkstraRouteAlgorithm::description() const {
    return "Dijkstra shortest paths from HQ; visits eligible nodes in depot-distance order";
}
