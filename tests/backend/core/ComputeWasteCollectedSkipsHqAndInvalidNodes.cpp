#include "CoreTestUtils.h"

TEST_CASE(ComputeWasteCollectedSkipsHqAndInvalidNodes) {
    WasteSystem system;
    system.initializeMap();
    clearNonHqWaste(system);
    system.getGraph().getNodeMutable(1).setWasteLevel(50.0f);
    system.getGraph().getNodeMutable(2).setWasteLevel(25.0f);

    const float total = system.computeWasteCollected({0, 1, 999, 2, 0});
    REQUIRE_NEAR(total, 325.0f, 0.001f);
}
