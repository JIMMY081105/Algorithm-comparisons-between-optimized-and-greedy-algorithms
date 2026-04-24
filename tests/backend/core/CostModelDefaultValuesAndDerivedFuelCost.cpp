#include "CoreTestUtils.h"

TEST_CASE(CostModelDefaultValuesAndDerivedFuelCost) {
    CostModel model;
    REQUIRE_NEAR(model.getLitresPerKm(), 0.40f, 0.001f);
    REQUIRE_NEAR(model.getDailyFuelPricePerLitre(), 2.50f, 0.001f);
    REQUIRE_NEAR(model.getFuelCostPerKm(), 1.0f, 0.001f);
    REQUIRE_NEAR(model.getTruckSpeedKmh(), 60.0f, 0.001f);
}
