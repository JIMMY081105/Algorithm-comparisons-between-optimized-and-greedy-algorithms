#include "CoreTestUtils.h"

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
