#include "CoreTestUtils.h"

TEST_CASE(EfficiencyBonusBoundariesAreExclusiveUpperLimits) {
    CostModel model;
    REQUIRE_NEAR(model.calculateEfficiencyBonus(79.99f), 25.0f, 0.001f);
    REQUIRE_NEAR(model.calculateEfficiencyBonus(80.0f), 15.0f, 0.001f);
    REQUIRE_NEAR(model.calculateEfficiencyBonus(120.0f), 8.0f, 0.001f);
    REQUIRE_NEAR(model.calculateEfficiencyBonus(180.0f), 3.0f, 0.001f);
    REQUIRE_NEAR(model.calculateEfficiencyBonus(250.0f), 0.0f, 0.001f);
}
