#include "TestHarness.h"

#include "core/CostModel.h"
#include "core/EventLog.h"
#include "core/RoadEvent.h"
#include "core/RouteResult.h"
#include "core/TollStation.h"
#include "core/WasteNode.h"
#include "core/WasteSystem.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void initializeSystem(WasteSystem& system, unsigned int seed = 1234u) {
    system.initializeMap();
    system.generateNewDayWithSeed(seed);
}

void clearNonHqWaste(WasteSystem& system) {
    MapGraph& graph = system.getGraph();
    for (int i = 0; i < graph.getNodeCount(); ++i) {
        WasteNode& node = graph.getNodeMutable(i);
        if (!node.getIsHQ()) {
            node.setWasteLevel(0.0f);
        }
    }
}

} // namespace

TEST_CASE(WasteNodeClampsWasteLevels) {
    WasteNode node(1, "Test", 0.0f, 0.0f, 200.0f);
    node.setWasteLevel(-25.0f);
    REQUIRE_NEAR(node.getWasteLevel(), 0.0f, 0.001f);
    node.setWasteLevel(135.0f);
    REQUIRE_NEAR(node.getWasteLevel(), 100.0f, 0.001f);
}

TEST_CASE(WasteNodeUrgencyThresholdsAreCorrect) {
    WasteNode node(1, "Test", 0.0f, 0.0f, 100.0f);
    node.setWasteLevel(39.99f);
    REQUIRE_TRUE(node.getUrgency() == UrgencyLevel::LOW);
    node.setWasteLevel(40.0f);
    REQUIRE_TRUE(node.getUrgency() == UrgencyLevel::MEDIUM);
    node.setWasteLevel(69.99f);
    REQUIRE_TRUE(node.getUrgency() == UrgencyLevel::MEDIUM);
    node.setWasteLevel(70.0f);
    REQUIRE_TRUE(node.getUrgency() == UrgencyLevel::HIGH);
}

TEST_CASE(HqNodeIsNeverEligibleAndAlwaysLowUrgency) {
    WasteNode hq(0, "HQ", 0.0f, 0.0f, 0.0f, true);
    hq.setWasteLevel(100.0f);
    REQUIRE_FALSE(hq.isEligible(0.0f));
    REQUIRE_TRUE(hq.getUrgency() == UrgencyLevel::LOW);
}

TEST_CASE(WasteAmountUsesCapacityAndPercent) {
    WasteNode node(2, "Capacity", 0.0f, 0.0f, 250.0f);
    node.setWasteLevel(64.0f);
    REQUIRE_NEAR(node.getWasteAmount(), 160.0f, 0.001f);
}

TEST_CASE(CollectionStatusCanBeSetAndResetForNewDay) {
    WasteNode node(3, "Collectable", 0.0f, 0.0f, 100.0f);
    node.setWasteLevel(80.0f);
    node.setCollected(true);
    REQUIRE_TRUE(node.isCollected());
    node.resetForNewDay();
    REQUIRE_FALSE(node.isCollected());
    REQUIRE_NEAR(node.getWasteLevel(), 80.0f, 0.001f);
}

TEST_CASE(WasteSystemInitializesFixedMapAndTolls) {
    WasteSystem system;
    system.initializeMap();
    REQUIRE_EQ(system.getGraph().getNodeCount(), 21);
    REQUIRE_EQ(system.getGraph().getHQNode().getId(), 0);
    REQUIRE_EQ(system.getTollStations().size(), static_cast<std::size_t>(7));
    REQUIRE_TRUE(system.getEventLog().getCount() >= 1);
}

TEST_CASE(GenerateNewDayWithSameSeedIsDeterministic) {
    WasteSystem first;
    initializeSystem(first, 4242u);
    WasteSystem second;
    initializeSystem(second, 4242u);

    REQUIRE_EQ(first.getDayNumber(), 1);
    REQUIRE_EQ(second.getDayNumber(), 1);
    REQUIRE_NEAR(first.getDailyFuelPricePerLitre(),
                 second.getDailyFuelPricePerLitre(), 0.001f);

    for (int i = 0; i < first.getGraph().getNodeCount(); ++i) {
        REQUIRE_NEAR(first.getGraph().getNode(i).getWasteLevel(),
                     second.getGraph().getNode(i).getWasteLevel(), 0.001f);
    }
}

TEST_CASE(GenerateNewDayAdvancesDayAndFuelPriceRange) {
    WasteSystem system;
    system.initializeMap();
    system.generateNewDayWithSeed(1u);
    system.generateNewDayWithSeed(2u);
    REQUIRE_EQ(system.getDayNumber(), 2);
    REQUIRE_TRUE(system.getDailyFuelPricePerLitre() >= 2.0f);
    REQUIRE_TRUE(system.getDailyFuelPricePerLitre() <= 3.8f);
    REQUIRE_NEAR(system.getCostModel().getDailyFuelPricePerLitre(),
                 system.getDailyFuelPricePerLitre(), 0.001f);
}

TEST_CASE(EligibilityFilteringHonorsThresholdAndExcludesHq) {
    WasteSystem system;
    system.initializeMap();
    clearNonHqWaste(system);
    system.setCollectionThreshold(50.0f);
    system.getGraph().getNodeMutable(1).setWasteLevel(49.99f);
    system.getGraph().getNodeMutable(2).setWasteLevel(50.0f);
    system.getGraph().getNodeMutable(3).setWasteLevel(80.0f);

    const std::vector<int> eligible = system.getEligibleNodes();
    REQUIRE_EQ(eligible.size(), static_cast<std::size_t>(2));
    REQUIRE_TRUE(std::find(eligible.begin(), eligible.end(), 2) != eligible.end());
    REQUIRE_TRUE(std::find(eligible.begin(), eligible.end(), 3) != eligible.end());
    REQUIRE_TRUE(std::find(eligible.begin(), eligible.end(), 0) == eligible.end());
}

TEST_CASE(EligibilityOverrideDoesNotChangeStoredThreshold) {
    WasteSystem system;
    system.initializeMap();
    clearNonHqWaste(system);
    system.setCollectionThreshold(90.0f);
    system.getGraph().getNodeMutable(1).setWasteLevel(60.0f);

    REQUIRE_TRUE(system.getEligibleNodes().empty());
    REQUIRE_EQ(system.getEligibleNodes(50.0f).size(), static_cast<std::size_t>(1));
    REQUIRE_NEAR(system.getCollectionThreshold(), 90.0f, 0.001f);
}

TEST_CASE(MarkNodeCollectedIgnoresInvalidNodeIds) {
    WasteSystem system;
    initializeSystem(system);
    system.markNodeCollected(1);
    REQUIRE_TRUE(system.getGraph().getNode(1).isCollected());
    system.markNodeCollected(9999);
    system.resetCollectionStatus();
    REQUIRE_FALSE(system.getGraph().getNode(1).isCollected());
}

TEST_CASE(ComputeWasteCollectedSkipsHqAndInvalidNodes) {
    WasteSystem system;
    system.initializeMap();
    clearNonHqWaste(system);
    system.getGraph().getNodeMutable(1).setWasteLevel(50.0f);
    system.getGraph().getNodeMutable(2).setWasteLevel(25.0f);

    const float total = system.computeWasteCollected({0, 1, 999, 2, 0});
    REQUIRE_NEAR(total, 325.0f, 0.001f);
}

TEST_CASE(TollStationValidatesConstructorArguments) {
    REQUIRE_THROWS_AS(TollStation(1, 1, 1.0f, "Bad"), std::invalid_argument);
    REQUIRE_THROWS_AS(TollStation(1, 2, -0.1f, "Bad"), std::invalid_argument);
    REQUIRE_THROWS_AS(TollStation(1, 2, 1.0f, ""), std::invalid_argument);
}

TEST_CASE(TollStationCrossingIsBidirectional) {
    TollStation toll(4, 7, 3.5f, "Gate");
    REQUIRE_TRUE(toll.isCrossedBy(4, 7));
    REQUIRE_TRUE(toll.isCrossedBy(7, 4));
    REQUIRE_FALSE(toll.isCrossedBy(4, 8));
    REQUIRE_NEAR(toll.fee(), 3.5f, 0.001f);
}

TEST_CASE(WasteSystemCalculatesTollsAndNamesForRoute) {
    WasteSystem system;
    system.initializeMap();
    const std::vector<int> route = {0, 15, 7, 15, 0};
    REQUIRE_NEAR(system.calculateTollCost(route), 11.0f, 0.001f);
    const std::vector<std::string> names = system.getTollNamesCrossed(route);
    REQUIRE_EQ(names.size(), static_cast<std::size_t>(4));
    REQUIRE_EQ(names.front(), std::string("Central Gate"));
}

TEST_CASE(CostModelDefaultValuesAndDerivedFuelCost) {
    CostModel model;
    REQUIRE_NEAR(model.getLitresPerKm(), 0.40f, 0.001f);
    REQUIRE_NEAR(model.getDailyFuelPricePerLitre(), 2.50f, 0.001f);
    REQUIRE_NEAR(model.getFuelCostPerKm(), 1.0f, 0.001f);
    REQUIRE_NEAR(model.getTruckSpeedKmh(), 60.0f, 0.001f);
}

TEST_CASE(CostModelSettersDriveCostCalculations) {
    CostModel model;
    model.setLitresPerKm(0.5f);
    model.setDailyFuelPricePerLitre(3.0f);
    model.setBaseWagePerShift(30.0f);
    model.setWagePerKmBonus(2.0f);
    model.setTruckSpeedKmh(50.0f);

    REQUIRE_NEAR(model.calculateFuelCost(10.0f), 15.0f, 0.001f);
    REQUIRE_NEAR(model.calculateTravelTime(100.0f), 2.0f, 0.001f);
    REQUIRE_NEAR(model.calculateWageCost(10.0f), 75.0f, 0.001f);
    REQUIRE_NEAR(model.calculateTotalCost(10.0f), 90.0f, 0.001f);
}

TEST_CASE(CostModelZeroSpeedReturnsZeroTravelTime) {
    CostModel model;
    model.setTruckSpeedKmh(0.0f);
    REQUIRE_NEAR(model.calculateTravelTime(100.0f), 0.0f, 0.001f);
}

TEST_CASE(EfficiencyBonusBoundariesAreExclusiveUpperLimits) {
    CostModel model;
    REQUIRE_NEAR(model.calculateEfficiencyBonus(79.99f), 25.0f, 0.001f);
    REQUIRE_NEAR(model.calculateEfficiencyBonus(80.0f), 15.0f, 0.001f);
    REQUIRE_NEAR(model.calculateEfficiencyBonus(120.0f), 8.0f, 0.001f);
    REQUIRE_NEAR(model.calculateEfficiencyBonus(180.0f), 3.0f, 0.001f);
    REQUIRE_NEAR(model.calculateEfficiencyBonus(250.0f), 0.0f, 0.001f);
}

TEST_CASE(PopulateCostsFillsEveryCostField) {
    WasteSystem system;
    system.initializeMap();
    clearNonHqWaste(system);
    system.getCostModel().setDailyFuelPricePerLitre(2.5f);
    system.getGraph().getNodeMutable(15).setWasteLevel(50.0f);

    RouteResult result;
    result.algorithmName = "Manual";
    result.visitOrder = {0, 15, 0};
    system.populateCosts(result);

    const float distance = system.getGraph().calculateRouteDistance(result.visitOrder);
    REQUIRE_NEAR(result.totalDistance, distance, 0.001f);
    REQUIRE_NEAR(result.travelTime, distance / 60.0f, 0.001f);
    REQUIRE_NEAR(result.fuelCost, distance * 1.0f, 0.001f);
    REQUIRE_NEAR(result.tollCost, 7.0f, 0.001f);
    REQUIRE_NEAR(result.totalCost,
                 result.fuelCost + result.wageCost + result.tollCost, 0.001f);
    REQUIRE_TRUE(result.wasteCollected > 0.0f);
}

TEST_CASE(PopulateCostsReportsZeroTimeForBlockedManualSegment) {
    WasteSystem system;
    system.initializeMap();
    system.getGraph().setEdgeEvent(0, 15, RoadEvent::FLOOD);

    RouteResult result;
    result.visitOrder = {0, 15};
    system.populateCosts(result);

    REQUIRE_NEAR(result.travelTime, 0.0f, 0.001f);
    REQUIRE_TRUE(result.fuelCost > 0.0f);
}

TEST_CASE(RouteResultDefaultsAreInvalidAndZeroed) {
    RouteResult result;
    REQUIRE_EQ(result.algorithmName, std::string("None"));
    REQUIRE_FALSE(result.isValid());
    REQUIRE_NEAR(result.totalCost, 0.0f, 0.001f);
}

TEST_CASE(RouteResultSummaryIncludesKeyMetrics) {
    RouteResult result;
    result.algorithmName = "Unit";
    result.visitOrder = {0, 1, 0};
    result.totalDistance = 12.345f;
    result.fuelCost = 10.0f;
    result.wageCost = 20.0f;
    result.tollCost = 3.0f;
    result.totalCost = 33.0f;
    result.wasteCollected = 44.0f;
    result.runtimeMs = 1.25;

    const std::string summary = result.toSummaryString();
    REQUIRE_TRUE(result.isValid());
    REQUIRE_TRUE(summary.find("Unit") != std::string::npos);
    REQUIRE_TRUE(summary.find("Total RM 33.00") != std::string::npos);
}

TEST_CASE(EventLogAppendsAndReturnsRecentNewestFirst) {
    EventLog log;
    log.addEventWithTime("10:00:00", "first");
    log.addEventWithTime("10:01:00", "second");
    log.addEventWithTime("10:02:00", "third");

    const auto recent = log.getRecentEvents(2);
    REQUIRE_EQ(log.getCount(), 3);
    REQUIRE_EQ(recent.size(), static_cast<std::size_t>(2));
    REQUIRE_EQ(recent[0]->message, std::string("third"));
    REQUIRE_EQ(recent[1]->message, std::string("second"));
}

TEST_CASE(EventLogClearResetsLinkedList) {
    EventLog log;
    log.addEventWithTime("10:00:00", "first");
    log.clear();
    REQUIRE_EQ(log.getCount(), 0);
    REQUIRE_TRUE(log.getHead() == nullptr);
    REQUIRE_TRUE(log.getRecentEvents(5).empty());
}
