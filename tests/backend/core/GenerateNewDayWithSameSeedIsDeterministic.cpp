#include "CoreTestUtils.h"

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
