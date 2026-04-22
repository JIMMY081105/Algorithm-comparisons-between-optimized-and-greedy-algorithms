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

    // Step 1 — Build the MST over the eligible subgraph using Prim's.
    // The MST itself is what minimises total edge weight; if you wanted
    // the answer to "which roads to wire up" this would be it.
    const std::vector<std::vector<int>> mstAdj = buildMST(graph, nodeIds);

    // Step 2 — Derive a route from the MST using the textbook
    // "double-tree" 2-approximation:
    //   a) Treat every MST edge as if it were doubled. The doubled
    //      multigraph has every vertex of even degree, so an Eulerian
    //      circuit exists and visits each MST edge exactly twice.
    //   b) Walk that Eulerian circuit from HQ.
    //   c) Shortcut the walk to a Hamiltonian circuit by skipping
    //      vertices we have already visited (triangle inequality holds
    //      on a Euclidean graph, so shortcutting never increases cost).
    //
    // The resulting tour is provably at most 2x the optimal TSP tour and
    // is a much more honest "MST-derived route" than a single DFS preorder
    // (which silently skips the back-edges and pretends it walked the tree).
    const int hqLocal = std::max(0, AlgorithmUtils::findNodePosition(nodeIds, hqId));

    std::vector<int> eulerWalk;
    eulerWalk.reserve(nodeIds.size() * 2);
    buildEulerWalk(mstAdj, hqLocal, eulerWalk);

    // Shortcut the Euler walk: keep first occurrence of every vertex,
    // then close the loop by returning to HQ.
    std::vector<bool> seen(nodeIds.size(), false);
    std::vector<int> shortcutLocal;
    shortcutLocal.reserve(nodeIds.size() + 1);

    for (const int local : eulerWalk) {
        if (!seen[local]) {
            seen[local] = true;
            shortcutLocal.push_back(local);
        }
    }
    if (shortcutLocal.empty() || shortcutLocal.front() != hqLocal) {
        shortcutLocal.insert(shortcutLocal.begin(), hqLocal);
    }
    shortcutLocal.push_back(hqLocal);

    // Translate local indices back to actual node IDs.
    result.visitOrder.clear();
    result.visitOrder.reserve(shortcutLocal.size());
    for (const int localIdx : shortcutLocal) {
        result.visitOrder.push_back(nodeIds[localIdx]);
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

    // Build a local distance matrix for just the nodes we care about.
    std::vector<std::vector<float>> localDist(
        nodeCount, std::vector<float>(nodeCount, 0.0f));
    for (int i = 0; i < nodeCount; ++i) {
        for (int j = i + 1; j < nodeCount; ++j) {
            const float d = graph.getShortestPathDistance(nodeIds[i], nodeIds[j]);
            localDist[i][j] = d;
            localDist[j][i] = d;
        }
    }

    // Standard Prim's: track which nodes are in the MST and the minimum
    // edge weight to reach each node from the current MST.
    std::vector<bool> inMST(nodeCount, false);
    std::vector<float> minEdge(nodeCount, std::numeric_limits<float>::max());
    std::vector<int> parent(nodeCount, -1);

    // Start from node 0 (which is HQ).
    minEdge[0] = 0.0f;

    for (int iter = 0; iter < nodeCount; ++iter) {
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

        if (parent[u] != -1) {
            adj[parent[u]].push_back(u);
            adj[u].push_back(parent[u]);
        }

        for (int v = 0; v < nodeCount; ++v) {
            if (!inMST[v] && localDist[u][v] < minEdge[v]) {
                minEdge[v] = localDist[u][v];
                parent[v] = u;
            }
        }
    }

    // Sort each adjacency list so the Euler walk is deterministic across runs
    // (and so the route looks visually consistent in the dashboard).
    for (auto& neighbors : adj) {
        std::sort(neighbors.begin(), neighbors.end());
    }

    return adj;
}

void MSTRouteAlgorithm::buildEulerWalk(const std::vector<std::vector<int>>& mstAdj,
                                       int start,
                                       std::vector<int>& walk) const {
    // Iterative DFS that records *every* visit (entry and back-track),
    // which is exactly the Euler walk on the MST with each edge doubled.
    // For a tree with k vertices the walk has length 2k - 1.
    const int n = static_cast<int>(mstAdj.size());
    if (n == 0) return;

    std::vector<std::size_t> nextChild(n, 0);
    std::vector<int> stack;
    stack.push_back(start);
    walk.push_back(start);

    while (!stack.empty()) {
        const int node = stack.back();
        if (nextChild[node] < mstAdj[node].size()) {
            const int child = mstAdj[node][nextChild[node]++];
            // Skip the parent edge — adjacency lists are undirected, but
            // the walk should descend into each subtree, then return.
            if (!stack.empty() && stack.size() >= 2 && stack[stack.size() - 2] == child) {
                continue;
            }
            stack.push_back(child);
            walk.push_back(child);
        } else {
            stack.pop_back();
            if (!stack.empty()) {
                walk.push_back(stack.back());  // record the back-track step
            }
        }
    }
}

std::string MSTRouteAlgorithm::algorithmName() const {
    return "MST Route";
}

std::string MSTRouteAlgorithm::description() const {
    return "Prim's MST + double-tree shortcut — provable 2-approximation of TSP";
}
