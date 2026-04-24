#include "CoreTestUtils.h"

TEST_CASE(WasteAmountUsesCapacityAndPercent) {
    WasteNode node(2, "Capacity", 0.0f, 0.0f, 250.0f);
    node.setWasteLevel(64.0f);
    REQUIRE_NEAR(node.getWasteAmount(), 160.0f, 0.001f);
}
