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
constexpr float kFuelPriceMin = 2.00f;
constexpr float kFuelPriceMax = 3.80f;

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

// Fixed toll stations on key edges that algorithms commonly traverse.
// Fees are fixed (real tolls don't change daily); only fuel price varies.
const std::array<TollStation, 7> kFixedTolls{{
    TollStation{ 0, 15, 3.50f, "Central Gate"},
    TollStation{15,  7, 2.00f, "North Checkpoint"},
    TollStation{ 1, 12, 4.00f, "West Toll"},
    TollStation{13,  4, 5.00f, "Southwest Pass"},
    TollStation{18, 14, 3.00f, "East Crossing"},
    TollStation{ 6, 10, 4.50f, "South Gate"},
    TollStation{11, 20, 6.00f, "Far East Toll"},
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
      collectionThreshold(kDefaultCollectionThreshold),
      dailyFuelPricePerLitre(2.50f) {
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

void WasteSystem::setupTollStations() {
    tollStations.clear();
    for (const TollStation& t : kFixedTolls) {
        tollStations.push_back(t);
    }
}

void WasteSystem::randomizeFuelPrice() {
    std::uniform_real_distribution<float> priceDist(kFuelPriceMin, kFuelPriceMax);
    dailyFuelPricePerLitre = priceDist(rng);
    // Round to 2 decimal places for realism
    dailyFuelPricePerLitre = std::round(dailyFuelPricePerLitre * 100.0f) / 100.0f;
    costModel.setDailyFuelPricePerLitre(dailyFuelPricePerLitre);
}

void WasteSystem::initializeMap() {
    graph.clear();
    addDefaultLocations();
    graph.setDistanceScale(kDistanceScaleKmPerGridUnit);
    graph.buildFullyConnectedGraph();
    setupTollStations();
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

    graph.clearAllEvents();
    assignWasteLevelsForCurrentDay();
    randomizeFuelPrice();

    std::ostringstream message;
    message << "Day " << dayNumber
            << " | Fuel: RM " << dailyFuelPricePerLitre << "/L"
            << " (seed: " << seed << ")";
    eventLog.addEvent(message.str());
}

MapGraph& WasteSystem::getGraph()                       { return graph; }
const MapGraph& WasteSystem::getGraph() const           { return graph; }
CostModel& WasteSystem::getCostModel()                  { return costModel; }
const CostModel& WasteSystem::getCostModel() const      { return costModel; }
EventLog& WasteSystem::getEventLog()                    { return eventLog; }
const EventLog& WasteSystem::getEventLog() const        { return eventLog; }
float WasteSystem::getCollectionThreshold() const       { return collectionThreshold; }
void WasteSystem::setCollectionThreshold(float t)       { collectionThreshold = t; }
const std::vector<TollStation>& WasteSystem::getTollStations() const { return tollStations; }
float WasteSystem::getDailyFuelPricePerLitre() const    { return dailyFuelPricePerLitre; }

float WasteSystem::calculateTollCost(const std::vector<int>& route) const {
    float total = 0.0f;
    for (int i = 0; i + 1 < static_cast<int>(route.size()); ++i) {
        for (const TollStation& toll : tollStations) {
            if (toll.isCrossedBy(route[i], route[i + 1])) {
                total += toll.fee;
            }
        }
    }
    return total;
}

std::vector<std::string> WasteSystem::getTollNamesCrossed(
    const std::vector<int>& route) const {
    std::vector<std::string> names;
    for (int i = 0; i + 1 < static_cast<int>(route.size()); ++i) {
        for (const TollStation& toll : tollStations) {
            if (toll.isCrossedBy(route[i], route[i + 1])) {
                names.push_back(toll.name);
            }
        }
    }
    return names;
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
    float total = 0.0f;
    for (int nodeId : route) {
        const int nodeIndex = graph.findNodeIndex(nodeId);
        if (nodeIndex < 0) continue;
        const WasteNode& node = graph.getNode(nodeIndex);
        if (!node.getIsHQ()) {
            total += node.getWasteAmount();
        }
    }
    return total;
}

void WasteSystem::populateCosts(RouteResult& result) const {
    result.totalDistance = graph.calculateRouteDistance(result.visitOrder);

    // Per-segment time and fuel: road events reduce speed and raise fuel consumption.
    // Regular (no-event) segments use the flat model; affected segments are penalized.
    float travelTime = 0.0f;
    float fuelCost   = 0.0f;
    const float normalSpeed  = costModel.getTruckSpeedKmh();
    const float litresPerKm  = costModel.getLitresPerKm();
    const float pricePerLitre = costModel.getDailyFuelPricePerLitre();
    for (std::size_t i = 0; i + 1 < result.visitOrder.size(); ++i) {
        const int fromId = result.visitOrder[i];
        const int toId   = result.visitOrder[i + 1];
        const float segKm    = graph.getDistance(fromId, toId);
        const RoadEvent ev   = graph.getEdgeEvent(fromId, toId);
        const float segSpeed = normalSpeed * roadEventSpeedFraction(ev);
        travelTime += (segSpeed > 0.0f) ? segKm / segSpeed : 0.0f;
        fuelCost   += litresPerKm * roadEventFuelMultiplier(ev) * pricePerLitre * segKm;
    }
    result.travelTime = travelTime;
    result.fuelCost   = fuelCost;

    result.basePay         = costModel.calculateBasePay();
    result.perKmBonus      = costModel.calculatePerKmBonus(result.totalDistance);
    result.efficiencyBonus = costModel.calculateEfficiencyBonus(result.totalDistance);
    result.wageCost        = result.basePay + result.perKmBonus + result.efficiencyBonus;

    result.tollCost        = calculateTollCost(result.visitOrder);
    result.tollsCrossed    = getTollNamesCrossed(result.visitOrder);

    result.totalCost       = result.fuelCost + result.wageCost + result.tollCost;
    result.wasteCollected  = computeWasteCollected(result.visitOrder);
}

int WasteSystem::getDayNumber() const           { return dayNumber; }
unsigned int WasteSystem::getCurrentSeed() const { return currentSeed; }
