#ifndef WASTE_SYSTEM_H
#define WASTE_SYSTEM_H

#include "CostModel.h"
#include "EventLog.h"
#include "MapGraph.h"
#include "RouteResult.h"

#include <random>
#include <vector>

// Central simulation state for the coursework scenario.
// WasteSystem owns the fixed map, the current day's waste levels, the
// collection threshold, and the event log consumed by the UI.
class WasteSystem {
private:
    MapGraph graph;
    CostModel costModel;
    EventLog eventLog;
    std::mt19937 rng;
    unsigned int currentSeed;
    int dayNumber;

    // Nodes below the threshold are ignored when algorithms build routes.
    float collectionThreshold;

    void addDefaultLocations();
    void assignWasteLevelsForCurrentDay();

public:
    WasteSystem();

    // Initialize the fixed Indian Ocean cleanup sector used across the app.
    void initializeMap();

    // Generate a fresh simulation day using a time-based seed.
    void generateNewDay();

    // Generate the day with a specific seed for reproducible demos and testing.
    void generateNewDayWithSeed(unsigned int seed);

    MapGraph& getGraph();
    const MapGraph& getGraph() const;
    CostModel& getCostModel();
    const CostModel& getCostModel() const;
    EventLog& getEventLog();
    const EventLog& getEventLog() const;

    float getCollectionThreshold() const;
    void setCollectionThreshold(float threshold);

    // Return the node IDs eligible for collection on the current day.
    std::vector<int> getEligibleNodes(float thresholdOverride = -1.0f) const;

    // Mark a node as collected so playback and rendering can show progress.
    void markNodeCollected(int nodeId);

    // Clear the collected state before a fresh comparison or replay.
    void resetCollectionStatus();

    float computeWasteCollected(const std::vector<int>& route) const;
    void populateCosts(RouteResult& result) const;

    int getDayNumber() const;
    unsigned int getCurrentSeed() const;
};

#endif // WASTE_SYSTEM_H
