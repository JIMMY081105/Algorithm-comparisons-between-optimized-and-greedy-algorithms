#include "core/MapGraph.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>

namespace {

int checkedNodeIndex(int index, std::size_t nodeCount, const char* functionName) {
    if (index < 0 || index >= static_cast<int>(nodeCount)) {
        throw std::out_of_range(std::string(functionName) + " - index out of bounds");
    }
    return index;
}

int requireNodeIdIndex(const MapGraph& graph, int nodeId, const char* functionName) {
    const int index = graph.findNodeIndex(nodeId);
    if (index < 0) {
        throw std::invalid_argument(std::string(functionName) + " - invalid node ID");
    }
    return index;
}

void validateWeightedMatrixShape(const std::vector<std::vector<float>>& matrix,
                                 std::size_t expectedNodeCount) {
    if (matrix.size() != expectedNodeCount) {
        throw std::invalid_argument(
            "MapGraph::installWeightedMatrix - row count does not match nodes");
    }

    for (const auto& row : matrix) {
        if (row.size() != expectedNodeCount) {
            throw std::invalid_argument(
                "MapGraph::installWeightedMatrix - column count does not match nodes");
        }
    }
}

} // namespace

MapGraph::MapGraph() : distanceScale(1.5f) {}

float MapGraph::computeEuclidean(const WasteNode& a, const WasteNode& b) const {
    const float dx = a.getWorldX() - b.getWorldX();
    const float dy = a.getWorldY() - b.getWorldY();
    return std::sqrt(dx * dx + dy * dy);
}

std::vector<std::vector<float>> MapGraph::buildEuclideanMatrix() const {
    const int n = static_cast<int>(nodes.size());
    std::vector<std::vector<float>> matrix(n, std::vector<float>(n, 0.0f));

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            const float dist = computeEuclidean(nodes[i], nodes[j]) * distanceScale;
            matrix[i][j] = dist;
            matrix[j][i] = dist;
        }
    }

    return matrix;
}

void MapGraph::addNode(const WasteNode& node) {
    nodes.push_back(node);
}

void MapGraph::clear() {
    nodes.clear();
    adjacencyMatrix.clear();
    edgeEvents.clear();
}

void MapGraph::buildFullyConnectedGraph() {
    adjacencyMatrix = buildEuclideanMatrix();
    const int n = static_cast<int>(nodes.size());
    edgeEvents.assign(n, std::vector<RoadEvent>(n, RoadEvent::NONE));
}

void MapGraph::installWeightedMatrix(const std::vector<std::vector<float>>& matrix) {
    validateWeightedMatrixShape(matrix, nodes.size());
    adjacencyMatrix = matrix;
}

void MapGraph::setDistanceScale(float scale) {
    distanceScale = scale;
}

int MapGraph::getNodeCount() const {
    return static_cast<int>(nodes.size());
}

const WasteNode& MapGraph::getNode(int index) const {
    return nodes[checkedNodeIndex(index, nodes.size(), "MapGraph::getNode")];
}

WasteNode& MapGraph::getNodeMutable(int index) {
    return nodes[checkedNodeIndex(index, nodes.size(), "MapGraph::getNodeMutable")];
}

float MapGraph::getDistance(int fromId, int toId) const {
    const int fi = requireNodeIdIndex(*this, fromId, "MapGraph::getDistance");
    const int ti = requireNodeIdIndex(*this, toId, "MapGraph::getDistance");
    return adjacencyMatrix[fi][ti];
}

const std::vector<std::vector<float>>& MapGraph::getAdjacencyMatrix() const {
    return adjacencyMatrix;
}

const std::vector<WasteNode>& MapGraph::getNodes() const {
    return nodes;
}

int MapGraph::findNodeIndex(int nodeId) const {
    const auto it = std::find_if(nodes.begin(), nodes.end(),
                                 [nodeId](const WasteNode& node) {
                                     return node.getId() == nodeId;
                                 });
    if (it == nodes.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(nodes.begin(), it));
}

std::vector<std::pair<int, float>> MapGraph::getNeighbors(int nodeId) const {
    std::vector<std::pair<int, float>> neighbors;
    const int idx = findNodeIndex(nodeId);
    if (idx < 0) {
        return neighbors;
    }

    neighbors.reserve(nodes.size() > 0 ? nodes.size() - 1 : 0);
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        if (i != idx && adjacencyMatrix[idx][i] > 0.0f) {
            neighbors.push_back({nodes[i].getId(), adjacencyMatrix[idx][i]});
        }
    }
    return neighbors;
}

float MapGraph::getEffectiveDistance(int fromId, int toId) const {
    const int fi = requireNodeIdIndex(*this, fromId, "MapGraph::getEffectiveDistance");
    const int ti = requireNodeIdIndex(*this, toId, "MapGraph::getEffectiveDistance");
    const float base = adjacencyMatrix[fi][ti];
    if (base <= 0.0f || edgeEvents.empty()) return base;
    return base * RoadEventRules::distanceMultiplier(edgeEvents[fi][ti]);
}

float MapGraph::getShortestPathDistance(int fromId, int toId) const {
    const int n = static_cast<int>(nodes.size());
    const int startIdx = requireNodeIdIndex(*this, fromId, "MapGraph::getShortestPathDistance");
    const int endIdx = requireNodeIdIndex(*this, toId, "MapGraph::getShortestPathDistance");
    if (n == 0) return getEffectiveDistance(fromId, toId);
    if (startIdx == endIdx) return 0.0f;

    const float kInf = std::numeric_limits<float>::max() / 2.0f;
    std::vector<float> dist(n, kInf);
    std::vector<bool> settled(n, false);
    dist[startIdx] = 0.0f;

    using Entry = std::pair<float, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> pq;
    pq.push({0.0f, startIdx});

    while (!pq.empty()) {
        const auto [d, u] = pq.top();
        pq.pop();
        if (settled[u]) continue;
        settled[u] = true;
        if (u == endIdx) break;

        for (int v = 0; v < n; ++v) {
            if (v == u || adjacencyMatrix[u][v] <= 0.0f) continue;
            // Treat blocked edges as completely impassable — skip them.
            if (!edgeEvents.empty() && edgeEvents[u][v] != RoadEvent::NONE) continue;
            const float tentative = dist[u] + adjacencyMatrix[u][v];
            if (tentative < dist[v]) {
                dist[v] = tentative;
                pq.push({tentative, v});
            }
        }
    }

    // No unblocked path exists — fall back so the algorithm still has a cost.
    return (dist[endIdx] < kInf) ? dist[endIdx] : getEffectiveDistance(fromId, toId);
}

void MapGraph::setEdgeEvent(int fromId, int toId, RoadEvent event) {
    const int fi = findNodeIndex(fromId);
    const int ti = findNodeIndex(toId);
    if (fi < 0 || ti < 0 || edgeEvents.empty()) return;
    edgeEvents[fi][ti] = event;
    edgeEvents[ti][fi] = event;
}

RoadEvent MapGraph::getEdgeEvent(int fromId, int toId) const {
    const int fi = findNodeIndex(fromId);
    const int ti = findNodeIndex(toId);
    if (fi < 0 || ti < 0 || edgeEvents.empty()) return RoadEvent::NONE;
    return edgeEvents[fi][ti];
}

void MapGraph::clearAllEvents() {
    for (auto& row : edgeEvents) {
        std::fill(row.begin(), row.end(), RoadEvent::NONE);
    }
}

std::vector<ActiveEdgeEvent> MapGraph::getActiveEdgeEvents() const {
    std::vector<ActiveEdgeEvent> result;
    const int n = static_cast<int>(nodes.size());
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (edgeEvents[i][j] != RoadEvent::NONE) {
                result.push_back({nodes[i].getId(), nodes[j].getId(), edgeEvents[i][j]});
            }
        }
    }
    return result;
}

float MapGraph::calculateRouteDistance(const std::vector<int>& route) const {
    float total = 0.0f;
    for (std::size_t i = 0; i + 1 < route.size(); ++i) {
        total += getDistance(route[i], route[i + 1]);
    }
    return total;
}

const WasteNode& MapGraph::getHQNode() const {
    const auto it = std::find_if(nodes.begin(), nodes.end(),
                                 [](const WasteNode& node) {
                                     return node.getIsHQ();
                                 });
    if (it == nodes.end()) {
        throw std::runtime_error("MapGraph::getHQNode - no HQ node found in graph");
    }
    return *it;
}

int MapGraph::getHQIndex() const {
    const auto it = std::find_if(nodes.begin(), nodes.end(),
                                 [](const WasteNode& node) {
                                     return node.getIsHQ();
                                 });
    if (it == nodes.end()) {
        throw std::runtime_error("MapGraph::getHQIndex - no HQ node found in graph");
    }
    return static_cast<int>(std::distance(nodes.begin(), it));
}
