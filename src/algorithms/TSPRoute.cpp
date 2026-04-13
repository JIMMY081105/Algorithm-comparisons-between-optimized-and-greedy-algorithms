#include "algorithms/TSPRoute.h"
#include "AlgorithmUtils.h"

#include <algorithm>
#include <limits>

namespace {
constexpr int kExactTspNodeLimit = 12;

std::vector<int> buildNearestNeighborRoute(const std::vector<std::vector<float>>& dist) {
    const int nodeCount = static_cast<int>(dist.size());
    std::vector<int> route;
    if (nodeCount <= 0) {
        return route;
    }

    std::vector<bool> visited(nodeCount, false);
    int current = 0;
    visited[current] = true;
    route.push_back(current);

    for (int visitedCount = 1; visitedCount < nodeCount; ++visitedCount) {
        float bestDistance = std::numeric_limits<float>::max();
        int bestNext = -1;

        for (int candidate = 1; candidate < nodeCount; ++candidate) {
            if (visited[candidate]) {
                continue;
            }

            if (dist[current][candidate] < bestDistance) {
                bestDistance = dist[current][candidate];
                bestNext = candidate;
            }
        }

        if (bestNext < 0) {
            break;
        }

        visited[bestNext] = true;
        route.push_back(bestNext);
        current = bestNext;
    }

    route.push_back(0);
    return route;
}
} // namespace

RouteResult TSPRouteAlgorithm::computeRoute(const MapGraph& graph,
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

    std::vector<std::vector<float>> dist(
        nodeCount, std::vector<float>(nodeCount, 0.0f));
    for (int i = 0; i < nodeCount; ++i) {
        for (int j = 0; j < nodeCount; ++j) {
            if (i != j) {
                dist[i][j] = graph.getDistance(nodeIds[i], nodeIds[j]);
            }
        }
    }

    if (nodeCount > kExactTspNodeLimit) {
        result.visitOrder = buildNearestNeighborRoute(dist);

        std::vector<int> actualRoute;
        actualRoute.reserve(result.visitOrder.size());
        for (const int localIdx : result.visitOrder) {
            actualRoute.push_back(nodeIds[localIdx]);
        }
        result.visitOrder = actualRoute;

        AlgorithmUtils::finalizeRuntime(result, startTime);
        return result;
    }

    const int fullMask = (1 << nodeCount) - 1;
    const float kInfinity = std::numeric_limits<float>::max() / 2.0f;

    std::vector<std::vector<float>> dp(
        1 << nodeCount, std::vector<float>(nodeCount, kInfinity));
    std::vector<std::vector<int>> parent(
        1 << nodeCount, std::vector<int>(nodeCount, -1));

    dp[1][0] = 0.0f;

    for (int mask = 1; mask < (1 << nodeCount); ++mask) {
        for (int u = 0; u < nodeCount; ++u) {
            if (!(mask & (1 << u))) continue;
            if (dp[mask][u] >= kInfinity) continue;

            for (int v = 0; v < nodeCount; ++v) {
                if (mask & (1 << v)) continue;

                const int newMask = mask | (1 << v);
                const float newDist = dp[mask][u] + dist[u][v];

                if (newDist < dp[newMask][v]) {
                    dp[newMask][v] = newDist;
                    parent[newMask][v] = u;
                }
            }
        }
    }

    result.visitOrder = reconstructPath(dp, parent, dist, nodeCount, 0);

    std::vector<int> actualRoute;
    actualRoute.reserve(result.visitOrder.size());
    for (const int localIdx : result.visitOrder) {
        actualRoute.push_back(nodeIds[localIdx]);
    }
    result.visitOrder = actualRoute;

    AlgorithmUtils::finalizeRuntime(result, startTime);
    return result;
}

std::vector<int> TSPRouteAlgorithm::reconstructPath(
    const std::vector<std::vector<float>>& dp,
    const std::vector<std::vector<int>>& parent,
    const std::vector<std::vector<float>>& dist,
    int n, int startIdx) const {

    const int fullMask = (1 << n) - 1;
    const float inf = std::numeric_limits<float>::max() / 2.0f;

    float bestTotal = inf;
    int lastNode = -1;

    for (int i = 0; i < n; ++i) {
        const float total = dp[fullMask][i] + dist[i][startIdx];
        if (total < bestTotal) {
            bestTotal = total;
            lastNode = i;
        }
    }

    std::vector<int> path;
    int mask = fullMask;
    int current = lastNode;

    while (current != -1) {
        path.push_back(current);
        const int prev = parent[mask][current];
        mask = mask ^ (1 << current);
        current = prev;
    }

    std::reverse(path.begin(), path.end());
    path.push_back(startIdx);

    return path;
}

std::string TSPRouteAlgorithm::algorithmName() const {
    return "TSP Route";
}

std::string TSPRouteAlgorithm::description() const {
    return "Exact bitmask DP solver for small sets with nearest-neighbor fallback for large ones";
}
