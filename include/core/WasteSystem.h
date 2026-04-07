#ifndef WASTE_SYSTEM_H
#define WASTE_SYSTEM_H

#include <vector>
#include <random>
#include "MapGraph.h"
#include "CostModel.h"
#include "EventLog.h"
#include "RouteResult.h"

// The main simulation manager that ties together the map, cost model,
// and event log. It handles waste generation, node eligibility checks,
// and provides the data that algorithms and the UI need to operate.
class WasteSystem {
private:
    MapGraph graph;
    CostModel costModel;
    EventLog eventLog;
    std::mt19937 rng;           // Mersenne Twister for quality randomness
    unsigned int currentSeed;
    int dayNumber;

    // Threshold for route eligibility — nodes below this won't be serviced
    float collectionThreshold;

public:
    WasteSystem();

    // Initialize the predefined Malaysian city map with fixed node positions
    void initializeMap();

    // Generate a new day: randomize waste levels across all non-HQ nodes
    void generateNewDay();

    // Generate with a specific seed for reproducible testing
    void generateNewDayWithSeed(unsigned int seed);

    // Access to subsystems
    MapGraph& getGraph();
    const MapGraph& getGraph() const;
    CostModel& getCostModel();
    const CostModel& getCostModel() const;
    EventLog& getEventLog();
    const EventLog& getEventLog() const;

    // Threshold management
    float getCollectionThreshold() const;
    void setCollectionThreshold(float threshold);

    // Get node IDs that are eligible for collection (above threshold)
    std::vector<int> getEligibleNodes(float thresholdOverride = -1.0f) const;

    // Mark a node as collected
    void markNodeCollected(int nodeId);

    // Reset all nodes to uncollected (for running a different algorithm)
    void resetCollectionStatus();

    // Compute total waste collected for a given route
    float computeWasteCollected(const std::vector<int>& route) const;

    // Fill in a RouteResult's cost fields from its distance
    void populateCosts(RouteResult& result) const;

    // Day info
    int getDayNumber() const;
    unsigned int getCurrentSeed() const;
};

#endif // WASTE_SYSTEM_H
