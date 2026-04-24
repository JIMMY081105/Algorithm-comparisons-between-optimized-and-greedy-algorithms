#include "CoreTestUtils.h"

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
