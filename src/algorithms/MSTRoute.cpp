#include "algorithms/MSTRoute.h"
#include "AlgorithmUtils.h"

#include <limits>
#include <algorithm>

RouteResult MSTRouteAlgorithm::computeRoute(const MapGraph& graph,
                                            const std::vector<int>& eligibleIds,
                                            int hqId) const {
    const auto startTime = AlgorithmUtils::RouteClock::now();

    RouteResult result = AlgorithmUtils::makeBaseResult(algorithmName());

    if (eligibleIds.empty()) {
        result.runtimeMs = 0.0;
        return result;
    }

    // Build the set of nodes to include: HQ + all eligible nodes.
    // We include HQ so the MST connects the depot to the collection points.
    const std::vector<int> nodeIds =
        AlgorithmUtils::buildWorkingNodeIds(eligibleIds, hqId);

    // Step 1: Build MST over the relevant subgraph using Prim's algorithm
    const std::vector<std::vector<int>> mstAdj = buildMST(graph, nodeIds);

    // Step 2: DFS traversal from HQ to get the visit order
    // The DFS gives us a walk through the tree that visits every node.
    // This is a standard technique for approximating TSP from an MST.
    std::vector<bool> visited(nodeIds.size(), false);
    std::vector<int> localOrder;

    // Find HQ's position in the local node list
    const int hqLocal = std::max(0, AlgorithmUtils::findNodePosition(nodeIds, hqId));

    dfsTraversal(mstAdj, hqLocal, visited, localOrder);

    // Convert local indices back to actual node IDs
    result.visitOrder.clear();
    for (int localIdx : localOrder) {
        result.visitOrder.push_back(nodeIds[localIdx]);
    }

    if (result.visitOrder.empty() || result.visitOrder.front() != hqId) {
        result.visitOrder.insert(result.visitOrder.begin(), hqId);
    }
    if (result.visitOrder.back() != hqId) {
        result.visitOrder.push_back(hqId);
    }

    AlgorithmUtils::finalizeRuntime(result, startTime);

    return result;
}

std::vector<std::vector<int>> MSTRouteAlgorithm::buildMST(
    const MapGraph& graph, const std::vector<int>& nodeIds) const {
    // Prim's algorithm builds the MST by repeatedly adding the cheapest
    // edge that connects a visited node to an unvisited one.
    //
    // We work with local indices (0 to n-1) mapped to actual node IDs.

    const int nodeCount = static_cast<int>(nodeIds.size());
    std::vector<std::vector<int>> adj(nodeCount);

    if (nodeCount <= 1) return adj;

    // Build a local distance matrix for just the nodes we care about
    std::vector<std::vector<float>> localDist(
        nodeCount, std::vector<float>(nodeCount, 0.0f));
    for (int i = 0; i < nodeCount; ++i) {
        for (int j = i + 1; j < nodeCount; ++j) {
            const float d = graph.getDistance(nodeIds[i], nodeIds[j]);
            localDist[i][j] = d;
            localDist[j][i] = d;
        }
    }

    // Standard Prim's: track which nodes are in the MST and the minimum
    // edge weight to reach each node from the current MST
    std::vector<bool> inMST(nodeCount, false);
    std::vector<float> minEdge(nodeCount, std::numeric_limits<float>::max());
    std::vector<int> parent(nodeCount, -1);

    // Start from node 0 (which is HQ)
    minEdge[0] = 0.0f;

    for (int iter = 0; iter < nodeCount; ++iter) {
        // Find the unvisited node with the smallest edge to the MST
        int u = -1;
        float best = std::numeric_limits<float>::max();
        for (int i = 0; i < nodeCount; ++i) {
            if (!inMST[i] && minEdge[i] < best) {
                best = minEdge[i];
                u = i;
            }
        }

        if (u == -1) break;  // graph might be disconnected (shouldn't happen)
        inMST[u] = true;

        // Add the MST edge (parent -> u)
        if (parent[u] != -1) {
            adj[parent[u]].push_back(u);
            adj[u].push_back(parent[u]);
        }

        // Update edge weights for neighbors of u
        for (int v = 0; v < nodeCount; ++v) {
            if (!inMST[v] && localDist[u][v] < minEdge[v]) {
                minEdge[v] = localDist[u][v];
                parent[v] = u;
            }
        }
    }

    return adj;
}

void MSTRouteAlgorithm::dfsTraversal(const std::vector<std::vector<int>>& mstAdj,
                                     int current,
                                     std::vector<bool>& visited,
                                     std::vector<int>& order) const {
    visited[current] = true;
    order.push_back(current);

    for (int neighbor : mstAdj[current]) {
        if (!visited[neighbor]) {
            dfsTraversal(mstAdj, neighbor, visited, order);
        }
    }
}

std::string MSTRouteAlgorithm::algorithmName() const {
    return "MST Route";
}

std::string MSTRouteAlgorithm::description() const {
    return "Minimum Spanning Tree with DFS traversal — minimizes total edge weight";
}
