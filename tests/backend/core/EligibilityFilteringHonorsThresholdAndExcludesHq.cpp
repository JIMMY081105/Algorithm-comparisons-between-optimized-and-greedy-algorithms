#include "CoreTestUtils.h"

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
