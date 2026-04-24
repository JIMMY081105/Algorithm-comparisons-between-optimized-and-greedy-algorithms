#include "CoreTestUtils.h"

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
