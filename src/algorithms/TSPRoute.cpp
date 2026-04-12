#include "algorithms/TSPRoute.h"
#include "AlgorithmUtils.h"

#include <limits>
#include <algorithm>

RouteResult TSPRouteAlgorithm::computeRoute(const MapGraph& graph,
                                            const std::vector<int>& eligibleIds,
                                            int hqId) const {
    const auto startTime = AlgorithmUtils::RouteClock::now();

    RouteResult result = AlgorithmUtils::makeBaseResult(algorithmName());

    if (eligibleIds.empty()) {
        result.runtimeMs = 0.0;
        return result;
    }

    // Build the working set: HQ must be index 0, then eligible nodes.
    // We need HQ in the set because TSP must start and end there.
    const std::vector<int> nodeIds =
        AlgorithmUtils::buildWorkingNodeIds(eligibleIds, hqId);
    const int nodeCount = static_cast<int>(nodeIds.size());

    // Build a local distance matrix for just our subset of nodes
    std::vector<std::vector<float>> dist(
        nodeCount, std::vector<float>(nodeCount, 0.0f));
    for (int i = 0; i < nodeCount; ++i) {
        for (int j = 0; j < nodeCount; ++j) {
            if (i != j) {
                dist[i][j] = graph.getDistance(nodeIds[i], nodeIds[j]);
            }
        }
    }

    // Bitmask DP for exact TSP solution.
    //
    // State: dp[mask][i] = minimum distance to reach local node i,
    //        having visited exactly the set of nodes indicated by 'mask'.
    //
    // Transition: dp[mask | (1<<j)][j] = min(dp[mask][i] + dist[i][j])
    //             for all i in mask where j is not in mask.
    //
    // Base case: dp[1][0] = 0 (start at HQ with only HQ visited).
    //
    // Answer: min over all i of (dp[fullMask][i] + dist[i][0]),
    //         which accounts for returning to HQ.

    const int fullMask = (1 << nodeCount) - 1;
    const float kInfinity = std::numeric_limits<float>::max() / 2.0f;

    // dp[mask][i] and parent[mask][i] for path reconstruction
    std::vector<std::vector<float>> dp(
        1 << nodeCount, std::vector<float>(nodeCount, kInfinity));
    std::vector<std::vector<int>> parent(
        1 << nodeCount, std::vector<int>(nodeCount, -1));

    // Start at HQ (local index 0)
    dp[1][0] = 0.0f;

    // Fill the DP table
    for (int mask = 1; mask < (1 << nodeCount); ++mask) {
        for (int u = 0; u < nodeCount; ++u) {
            // Skip if u is not in the current visited set
            if (!(mask & (1 << u))) continue;
            if (dp[mask][u] >= kInfinity) continue;

            // Try extending to each unvisited node
            for (int v = 0; v < nodeCount; ++v) {
                if (mask & (1 << v)) continue;  // already visited

                const int newMask = mask | (1 << v);
                const float newDist = dp[mask][u] + dist[u][v];

                if (newDist < dp[newMask][v]) {
                    dp[newMask][v] = newDist;
                    parent[newMask][v] = u;
                }
            }
        }
    }

    // Find the best final node to return to HQ from
    result.visitOrder = reconstructPath(dp, parent, dist, nodeCount, 0);

    // Map local indices back to actual node IDs
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

    int fullMask = (1 << n) - 1;
    const float INF = std::numeric_limits<float>::max() / 2.0f;

    // Find which node to end at (before returning to HQ) for minimum total
    float bestTotal = INF;
    int lastNode = -1;

    for (int i = 0; i < n; i++) {
        float total = dp[fullMask][i] + dist[i][startIdx];
        if (total < bestTotal) {
            bestTotal = total;
            lastNode = i;
        }
    }

    // Reconstruct the path by walking backwards through parent pointers
    std::vector<int> path;
    int mask = fullMask;
    int current = lastNode;

    while (current != -1) {
        path.push_back(current);
        int prev = parent[mask][current];
        mask = mask ^ (1 << current);
        current = prev;
    }

    // The path is backwards — reverse it so HQ is first
    std::reverse(path.begin(), path.end());

    // Add return to HQ at the end
    path.push_back(startIdx);

    return path;
}

std::string TSPRouteAlgorithm::algorithmName() const {
    return "TSP Route";
}

std::string TSPRouteAlgorithm::description() const {
    return "Exact bitmask DP solver — finds optimal round trip from HQ";
}
