#include "core/WasteSystem.h"

#include <array>
#include <chrono>
#include <sstream>

namespace {
struct MapNodeDefinition {
    int id;
    const char* name;
    float gridX;
    float gridY;
    float capacityKg;
    bool isHeadquarters;
};

constexpr float kDefaultCollectionThreshold = 5.0f;
constexpr float kDistanceScaleKmPerGridUnit = 1.5f;
constexpr float kMinimumDailyWasteLevel = 5.0f;
constexpr float kMaximumDailyWasteLevel = 100.0f;

constexpr std::array<MapNodeDefinition, 21> kIndianOceanLocations{{
    {0, "Command Anchorage",   11.50f, 11.25f,   0.0f, true},
    {1, "Lakshadweep Current",  3.20f,  1.80f, 500.0f, false},
    {2, "Andaman Passage",     20.80f,  3.10f, 300.0f, false},
    {3, "Nicobar Drift",       22.90f, 14.80f, 300.0f, false},
    {4, "Malacca Reach",        6.20f, 18.70f, 400.0f, false},
    {5, "Arabian Shoal",        0.90f, 10.80f, 450.0f, false},
    {6, "Cocos Corridor",      16.40f, 22.10f, 250.0f, false},
    {7, "Bengal Gate",         12.20f,  0.90f, 350.0f, false},
    {8, "Sunda Channel",       26.80f,  8.30f, 380.0f, false},
    {9, "Monsoon Trench",       0.30f, 22.60f, 550.0f, false},
    {10, "Java Gyre",          24.10f, 20.20f, 320.0f, false},
    {11, "Sumatra Shelf",      18.60f, 17.40f, 360.0f, false},
    {12, "Chagos Current",      7.80f,  6.40f, 340.0f, false},
    {13, "Mascarene Wake",      4.40f, 14.90f, 310.0f, false},
    {14, "Timor Stream",       25.10f, 12.60f, 330.0f, false},
    {15, "Equator Sweep",      14.70f,  6.20f, 280.0f, false},
    {16, "Reunion Spiral",      9.10f, 21.30f, 390.0f, false},
    {17, "Arafura Link",       27.20f,  4.90f, 420.0f, false},
    {18, "Bay Breaker",        17.90f, 10.60f, 300.0f, false},
    {19, "Southern Drift",     13.80f, 23.60f, 440.0f, false},
    {20, "Coral Arc",          28.40f, 17.10f, 360.0f, false},
}};

unsigned int makeTimeSeed() {
    return static_cast<unsigned int>(
        std::chrono::steady_clock::now().time_since_epoch().count());
}

std::string buildMapInitializedMessage(int nodeCount) {
    return "Map initialized with " + std::to_string(nodeCount) + " locations";
}
} // namespace

WasteSystem::WasteSystem()
    : currentSeed(0),
      dayNumber(0),
      collectionThreshold(kDefaultCollectionThreshold) {
    currentSeed = makeTimeSeed();
    rng.seed(currentSeed);
}

void WasteSystem::addDefaultLocations() {
    for (const MapNodeDefinition& location : kIndianOceanLocations) {
        graph.addNode(WasteNode(location.id,
                                location.name,
                                location.gridX,
                                location.gridY,
                                location.capacityKg,
                                location.isHeadquarters));
    }
}

void WasteSystem::initializeMap() {
    // The coursework uses one fixed sector so algorithm behaviour is easier to
    // compare and explain during the demo and in the final report.
    graph.clear();
    addDefaultLocations();

    graph.setDistanceScale(kDistanceScaleKmPerGridUnit);
    graph.buildFullyConnectedGraph();

    eventLog.addEvent(buildMapInitializedMessage(graph.getNodeCount()));
}

void WasteSystem::generateNewDay() {
    generateNewDayWithSeed(makeTimeSeed());
}

void WasteSystem::assignWasteLevelsForCurrentDay() {
    std::uniform_real_distribution<float> wasteLevelDistribution(
        kMinimumDailyWasteLevel, kMaximumDailyWasteLevel);

    for (int i = 0; i < graph.getNodeCount(); ++i) {
        WasteNode& node = graph.getNodeMutable(i);
        node.resetForNewDay();

        if (!node.getIsHQ()) {
            node.setWasteLevel(wasteLevelDistribution(rng));
        }
    }
}

void WasteSystem::generateNewDayWithSeed(unsigned int seed) {
    currentSeed = seed;
    rng.seed(seed);
    ++dayNumber;

    assignWasteLevelsForCurrentDay();

    std::ostringstream message;
    message << "Day " << dayNumber << " generated (seed: " << seed << ")";
    eventLog.addEvent(message.str());
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
    const float activeThreshold = thresholdOverride >= 0.0f
                                      ? thresholdOverride
                                      : collectionThreshold;

    std::vector<int> eligibleNodeIds;
    eligibleNodeIds.reserve(graph.getNodeCount());
    for (int i = 0; i < graph.getNodeCount(); ++i) {
        const WasteNode& node = graph.getNode(i);
        if (node.isEligible(activeThreshold)) {
            eligibleNodeIds.push_back(node.getId());
        }
    }
    return eligibleNodeIds;
}

void WasteSystem::markNodeCollected(int nodeId) {
    const int nodeIndex = graph.findNodeIndex(nodeId);
    if (nodeIndex >= 0) {
        graph.getNodeMutable(nodeIndex).setCollected(true);
    }
}

void WasteSystem::resetCollectionStatus() {
    for (int i = 0; i < graph.getNodeCount(); ++i) {
        graph.getNodeMutable(i).setCollected(false);
    }
}

float WasteSystem::computeWasteCollected(const std::vector<int>& route) const {
    float totalWasteCollected = 0.0f;

    for (int nodeId : route) {
        const int nodeIndex = graph.findNodeIndex(nodeId);
        if (nodeIndex < 0) {
            continue;
        }

        const WasteNode& node = graph.getNode(nodeIndex);
        if (!node.getIsHQ()) {
            totalWasteCollected += node.getWasteAmount();
        }
    }

    return totalWasteCollected;
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
