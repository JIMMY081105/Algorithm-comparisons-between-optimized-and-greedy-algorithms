#include "CoreTestUtils.h"

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
