#include "algorithms/MSTRoute.h"
#include <chrono>
#include <limits>
#include <algorithm>

RouteResult MSTRouteAlgorithm::computeRoute(const MapGraph& graph,
                                             const std::vector<int>& eligibleIds,
                                             int hqId) {
    auto startTime = std::chrono::high_resolution_clock::now();

    RouteResult result;
    result.algorithmName = algorithmName();

    if (eligibleIds.empty()) {
        result.runtimeMs = 0.0;
        return result;
    }

    // Build the set of nodes to include: HQ + all eligible nodes.
    // We include HQ so the MST connects the depot to the collection points.
    std::vector<int> nodeIds;
    nodeIds.push_back(hqId);
    for (int id : eligibleIds) {
        if (id != hqId) {
            nodeIds.push_back(id);
        }
    }

    // Step 1: Build MST over the relevant subgraph using Prim's algorithm
    std::vector<std::vector<int>> mstAdj = buildMST(graph, nodeIds);

    // Step 2: DFS traversal from HQ to get the visit order
    // The DFS gives us a walk through the tree that visits every node.
    // This is a standard technique for approximating TSP from an MST.
    std::vector<bool> visited(nodeIds.size(), false);
    std::vector<int> localOrder;

    // Find HQ's position in the local node list
    int hqLocal = 0;
    for (int i = 0; i < static_cast<int>(nodeIds.size()); i++) {
        if (nodeIds[i] == hqId) {
            hqLocal = i;
            break;
        }
    }

    dfsTraversal(mstAdj, hqLocal, visited, localOrder);

    // Convert local indices back to actual node IDs
    result.visitOrder.clear();
    for (int localIdx : localOrder) {
        result.visitOrder.push_back(nodeIds[localIdx]);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.runtimeMs = std::chrono::duration<double, std::milli>(
        endTime - startTime).count();

    return result;
}

std::vector<std::vector<int>> MSTRouteAlgorithm::buildMST(
    const MapGraph& graph, const std::vector<int>& nodeIds) {
    // Prim's algorithm builds the MST by repeatedly adding the cheapest
    // edge that connects a visited node to an unvisited one.
    //
    // We work with local indices (0 to n-1) mapped to actual node IDs.

    int n = static_cast<int>(nodeIds.size());
    std::vector<std::vector<int>> adj(n);

    if (n <= 1) return adj;

    // Build a local distance matrix for just the nodes we care about
    std::vector<std::vector<float>> localDist(n, std::vector<float>(n, 0.0f));
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            float d = graph.getDistance(nodeIds[i], nodeIds[j]);
            localDist[i][j] = d;
            localDist[j][i] = d;
        }
    }

    // Standard Prim's: track which nodes are in the MST and the minimum
    // edge weight to reach each node from the current MST
    std::vector<bool> inMST(n, false);
    std::vector<float> minEdge(n, std::numeric_limits<float>::max());
    std::vector<int> parent(n, -1);

    // Start from node 0 (which is HQ)
    minEdge[0] = 0.0f;

    for (int iter = 0; iter < n; iter++) {
        // Find the unvisited node with the smallest edge to the MST
        int u = -1;
        float best = std::numeric_limits<float>::max();
        for (int i = 0; i < n; i++) {
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
        for (int v = 0; v < n; v++) {
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
                                      std::vector<int>& order) {
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
