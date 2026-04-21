#ifndef WASTE_SYSTEM_H
#define WASTE_SYSTEM_H

#include "CostModel.h"
#include "EventLog.h"
#include "MapGraph.h"
#include "RouteResult.h"
#include "TollStation.h"

#include <random>
#include <vector>

// Central simulation state for the coursework scenario.
// WasteSystem owns the fixed map, the current day's waste levels, toll
// stations, the randomized daily fuel price, and the event log.
class WasteSystem {
private:
    MapGraph graph;
    CostModel costModel;
    EventLog eventLog;
    std::mt19937 rng;
    unsigned int currentSeed;
    int dayNumber;

    float collectionThreshold;

    // Toll stations — placed once on map init, fees stay fixed
    std::vector<TollStation> tollStations;

    // Fuel price is re-randomized every new day (RM 2.00–3.80 per litre)
    float dailyFuelPricePerLitre;

    void addDefaultLocations();
    void assignWasteLevelsForCurrentDay();
    void setupTollStations();
    void randomizeFuelPrice();

public:
    WasteSystem();

    void initializeMap();

    void generateNewDay();
    void generateNewDayWithSeed(unsigned int seed);

    MapGraph& getGraph();
    const MapGraph& getGraph() const;
    CostModel& getCostModel();
    const CostModel& getCostModel() const;
    EventLog& getEventLog();
    const EventLog& getEventLog() const;

    float getCollectionThreshold() const;
    void setCollectionThreshold(float threshold);

    // --- Toll station access ---
    const std::vector<TollStation>& getTollStations() const;
    float calculateTollCost(const std::vector<int>& route) const;
    std::vector<std::string> getTollNamesCrossed(const std::vector<int>& route) const;

    // --- Daily fuel price ---
    float getDailyFuelPricePerLitre() const;

    std::vector<int> getEligibleNodes(float thresholdOverride = -1.0f) const;
    void markNodeCollected(int nodeId);
    void resetCollectionStatus();
    float computeWasteCollected(const std::vector<int>& route) const;

    // Fills all cost fields of result including tolls and wage breakdown.
    void populateCosts(RouteResult& result) const;

    int getDayNumber() const;
    unsigned int getCurrentSeed() const;
};

#endif // WASTE_SYSTEM_H
