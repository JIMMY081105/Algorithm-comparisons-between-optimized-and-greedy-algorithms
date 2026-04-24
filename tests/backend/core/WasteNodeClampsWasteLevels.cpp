#include "CoreTestUtils.h"

TEST_CASE(WasteNodeClampsWasteLevels) {
    WasteNode node(1, "Test", 0.0f, 0.0f, 200.0f);
    node.setWasteLevel(-25.0f);
    REQUIRE_NEAR(node.getWasteLevel(), 0.0f, 0.001f);
    node.setWasteLevel(135.0f);
    REQUIRE_NEAR(node.getWasteLevel(), 100.0f, 0.001f);
}
