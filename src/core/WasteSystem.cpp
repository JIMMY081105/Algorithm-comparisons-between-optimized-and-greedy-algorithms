#include "core/WasteSystem.h"
#include <chrono>
#include <sstream>
#include <iomanip>

WasteSystem::WasteSystem()
    : currentSeed(0), dayNumber(0), collectionThreshold(30.0f) {
    // Seed the RNG with system clock for genuine randomness on first use
    auto timeSeed = static_cast<unsigned int>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    rng.seed(timeSeed);
    currentSeed = timeSeed;
}

void WasteSystem::initializeMap() {
    // Build the predefined map of a fictitious Malaysian city.
    // Node positions are on a logical grid — the renderer will
    // project them into isometric space for display.
    //
    // Layout designed to create an interesting spread of locations
    // around a central HQ, simulating a typical urban district.

    //                          ID  Name                    X     Y    Capacity(kg)  isHQ
    graph.addNode(WasteNode(    0, "HQ Depot",            9.75f,  9.75f,  0.0f,      true));
    graph.addNode(WasteNode(    1, "Factory Zone",        3.90f,  1.95f,  500.0f,    false));
    graph.addNode(WasteNode(    2, "Residential Block A", 15.60f, 3.90f,  300.0f,    false));
    graph.addNode(WasteNode(    3, "Residential Block B", 17.55f, 11.70f, 300.0f,    false));
    graph.addNode(WasteNode(    4, "Food Street",         5.85f,  15.60f, 400.0f,    false));
    graph.addNode(WasteNode(    5, "Market Area",         1.95f,  9.75f,  450.0f,    false));
    graph.addNode(WasteNode(    6, "School Compound",     13.65f, 17.55f, 250.0f,    false));
    graph.addNode(WasteNode(    7, "Office Park",         11.70f, 1.95f,  350.0f,    false));
    graph.addNode(WasteNode(    8, "Apartment Cluster",   19.50f, 7.80f,  380.0f,    false));
    graph.addNode(WasteNode(    9, "Industrial Yard",     1.95f,  17.55f, 550.0f,    false));

    // Build the complete distance matrix so all routing algorithms
    // can query distances between any pair of locations
    graph.setDistanceScale(1.5f);  // 1 grid unit ~ 1.5 km
    graph.buildFullyConnectedGraph();

    eventLog.addEvent("Map initialized with " +
                      std::to_string(graph.getNodeCount()) + " locations");
}

void WasteSystem::generateNewDay() {
    auto timeSeed = static_cast<unsigned int>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    generateNewDayWithSeed(timeSeed);
}

void WasteSystem::generateNewDayWithSeed(unsigned int seed) {
    currentSeed = seed;
    rng.seed(seed);
    dayNumber++;

    // Assign random waste levels to every non-HQ node
    std::uniform_real_distribution<float> wasteDist(5.0f, 100.0f);

    for (int i = 0; i < graph.getNodeCount(); i++) {
        WasteNode& node = graph.getNodeMutable(i);
        node.resetForNewDay();
        if (!node.getIsHQ()) {
            node.setWasteLevel(wasteDist(rng));
        }
    }

    std::ostringstream oss;
    oss << "Day " << dayNumber << " generated (seed: " << seed << ")";
    eventLog.addEvent(oss.str());
}

MapGraph& WasteSystem::getGraph() { return graph; }
const MapGraph& WasteSystem::getGraph() const { return graph; }
CostModel& WasteSystem::getCostModel() { return costModel; }
const CostModel& WasteSystem::getCostModel() const { return costModel; }
EventLog& WasteSystem::getEventLog() { return eventLog; }
const EventLog& WasteSystem::getEventLog() const { return eventLog; }

float WasteSystem::getCollectionThreshold() const { return collectionThreshold; }
void WasteSystem::setCollectionThreshold(float threshold) {
    collectionThreshold = threshold;
}

std::vector<int> WasteSystem::getEligibleNodes(float thresholdOverride) const {
    float threshold = (thresholdOverride >= 0.0f)
                      ? thresholdOverride
                      : collectionThreshold;
    std::vector<int> eligible;
    for (int i = 0; i < graph.getNodeCount(); i++) {
        const WasteNode& node = graph.getNode(i);
        if (node.isEligible(threshold)) {
            eligible.push_back(node.getId());
        }
    }
    return eligible;
}

void WasteSystem::markNodeCollected(int nodeId) {
    int idx = graph.findNodeIndex(nodeId);
    if (idx >= 0) {
        graph.getNodeMutable(idx).setCollected(true);
    }
}

void WasteSystem::resetCollectionStatus() {
    for (int i = 0; i < graph.getNodeCount(); i++) {
        graph.getNodeMutable(i).setCollected(false);
    }
}

float WasteSystem::computeWasteCollected(const std::vector<int>& route) const {
    float total = 0.0f;
    for (int nodeId : route) {
        int idx = graph.findNodeIndex(nodeId);
        if (idx >= 0) {
            const WasteNode& node = graph.getNode(idx);
            if (!node.getIsHQ()) {
                total += node.getWasteAmount();
            }
        }
    }
    return total;
}

void WasteSystem::populateCosts(RouteResult& result) const {
    result.totalDistance = graph.calculateRouteDistance(result.visitOrder);
    result.travelTime = costModel.calculateTravelTime(result.totalDistance);
    result.fuelCost = costModel.calculateFuelCost(result.totalDistance);
    result.wageCost = costModel.calculateWageCost(result.totalDistance);
    result.totalCost = costModel.calculateTotalCost(result.totalDistance);
    result.wasteCollected = computeWasteCollected(result.visitOrder);
}

int WasteSystem::getDayNumber() const { return dayNumber; }
unsigned int WasteSystem::getCurrentSeed() const { return currentSeed; }
