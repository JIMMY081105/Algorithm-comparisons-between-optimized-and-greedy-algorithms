#include "core/MapGraph.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {

int checkedNodeIndex(int index, std::size_t nodeCount, const char* functionName) {
    if (index < 0 || index >= static_cast<int>(nodeCount)) {
        throw std::out_of_range(std::string(functionName) + " - index out of bounds");
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
}

void MapGraph::buildFullyConnectedGraph() {
    adjacencyMatrix = buildEuclideanMatrix();
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
    const int fi = findNodeIndex(fromId);
    const int ti = findNodeIndex(toId);
    if (fi < 0 || ti < 0) {
        throw std::invalid_argument("MapGraph::getDistance - invalid node ID");
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
