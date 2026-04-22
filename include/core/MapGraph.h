#ifndef MAP_GRAPH_H
#define MAP_GRAPH_H

#include <vector>
#include <string>
#include "RoadEvent.h"
#include "WasteNode.h"

// Represents the road network as a weighted undirected graph.
// Internally uses an adjacency matrix where each entry stores
// the Euclidean distance between two nodes (scaled to km).
// A parallel event matrix overlays road conditions (flood, traffic, festival)
// that inflate effective distances seen by the routing algorithms.
class MapGraph {
private:
    std::vector<WasteNode> nodes;
    std::vector<std::vector<float>> adjacencyMatrix;
    std::vector<std::vector<RoadEvent>> edgeEvents;
    float distanceScale;  // multiplier to convert grid units to km

    // Compute Euclidean distance between two nodes on the grid
    float computeEuclidean(const WasteNode& a, const WasteNode& b) const;
    std::vector<std::vector<float>> buildEuclideanMatrix() const;

public:
    MapGraph();

    // Setup
    void addNode(const WasteNode& node);
    void clear();
    void buildFullyConnectedGraph();  // connect every pair of nodes
    void installWeightedMatrix(const std::vector<std::vector<float>>& matrix);
    void setDistanceScale(float scale);

    // Queries
    int getNodeCount() const;
    const WasteNode& getNode(int index) const;
    WasteNode& getNodeMutable(int index);
    float getDistance(int fromId, int toId) const;
    // Returns the base distance multiplied by the active road-event penalty.
    // Algorithms should call this instead of getDistance so they route around events.
    float getEffectiveDistance(int fromId, int toId) const;
    const std::vector<std::vector<float>>& getAdjacencyMatrix() const;
    const std::vector<WasteNode>& getNodes() const;

    // Road event management
    void setEdgeEvent(int fromId, int toId, RoadEvent event);
    RoadEvent getEdgeEvent(int fromId, int toId) const;
    void clearAllEvents();
    std::vector<ActiveEdgeEvent> getActiveEdgeEvents() const;

    // Find node index by ID
    int findNodeIndex(int nodeId) const;

    // Get all neighbor distances for a given node
    std::vector<std::pair<int, float>> getNeighbors(int nodeId) const;

    // Calculate total distance for a sequence of node IDs
    float calculateRouteDistance(const std::vector<int>& route) const;

    // Get the HQ node (assumes it was added first or flagged)
    const WasteNode& getHQNode() const;
    int getHQIndex() const;
};

#endif // MAP_GRAPH_H
