#include "core/MapGraph.h"
#include <cmath>
#include <stdexcept>
#include <algorithm>

MapGraph::MapGraph() : distanceScale(1.5f) {}

float MapGraph::computeEuclidean(const WasteNode& a, const WasteNode& b) const {
    float dx = a.getWorldX() - b.getWorldX();
    float dy = a.getWorldY() - b.getWorldY();
    return std::sqrt(dx * dx + dy * dy);
}

void MapGraph::addNode(const WasteNode& node) {
    nodes.push_back(node);
}

void MapGraph::buildFullyConnectedGraph() {
    // Every location can reach every other location — we're simulating
    // a road network where all areas are accessible.
    int n = static_cast<int>(nodes.size());
    adjacencyMatrix.assign(n, std::vector<float>(n, 0.0f));

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            float dist = computeEuclidean(nodes[i], nodes[j]) * distanceScale;
            adjacencyMatrix[i][j] = dist;
            adjacencyMatrix[j][i] = dist;
        }
    }
}

void MapGraph::setDistanceScale(float scale) {
    distanceScale = scale;
}

int MapGraph::getNodeCount() const {
    return static_cast<int>(nodes.size());
}

const WasteNode& MapGraph::getNode(int index) const {
    if (index < 0 || index >= static_cast<int>(nodes.size())) {
        throw std::out_of_range("MapGraph::getNode — index out of bounds");
    }
    return nodes[index];
}

WasteNode& MapGraph::getNodeMutable(int index) {
    if (index < 0 || index >= static_cast<int>(nodes.size())) {
        throw std::out_of_range("MapGraph::getNodeMutable — index out of bounds");
    }
    return nodes[index];
}

float MapGraph::getDistance(int fromId, int toId) const {
    int fi = findNodeIndex(fromId);
    int ti = findNodeIndex(toId);
    if (fi < 0 || ti < 0) {
        throw std::invalid_argument("MapGraph::getDistance — invalid node ID");
    }
    return adjacencyMatrix[fi][ti];
}

const std::vector<std::vector<float>>& MapGraph::getAdjacencyMatrix() const {
    return adjacencyMatrix;
}

const std::vector<WasteNode>& MapGraph::getNodes() const {
    return nodes;
}

int MapGraph::findNodeIndex(int nodeId) const {
    for (int i = 0; i < static_cast<int>(nodes.size()); i++) {
        if (nodes[i].getId() == nodeId) return i;
    }
    return -1;
}

std::vector<std::pair<int, float>> MapGraph::getNeighbors(int nodeId) const {
    std::vector<std::pair<int, float>> neighbors;
    int idx = findNodeIndex(nodeId);
    if (idx < 0) return neighbors;

    for (int i = 0; i < static_cast<int>(nodes.size()); i++) {
        if (i != idx && adjacencyMatrix[idx][i] > 0) {
            neighbors.push_back({nodes[i].getId(), adjacencyMatrix[idx][i]});
        }
    }
    return neighbors;
}

float MapGraph::calculateRouteDistance(const std::vector<int>& route) const {
    float total = 0.0f;
    for (size_t i = 0; i + 1 < route.size(); i++) {
        total += getDistance(route[i], route[i + 1]);
    }
    return total;
}

const WasteNode& MapGraph::getHQNode() const {
    for (const auto& node : nodes) {
        if (node.getIsHQ()) return node;
    }
    throw std::runtime_error("MapGraph::getHQNode — no HQ node found in graph");
}

int MapGraph::getHQIndex() const {
    for (int i = 0; i < static_cast<int>(nodes.size()); i++) {
        if (nodes[i].getIsHQ()) return i;
    }
    throw std::runtime_error("MapGraph::getHQIndex — no HQ node found in graph");
}
