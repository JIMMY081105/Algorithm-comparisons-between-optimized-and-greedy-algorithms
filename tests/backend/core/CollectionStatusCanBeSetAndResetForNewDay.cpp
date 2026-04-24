#include "CoreTestUtils.h"

TEST_CASE(CollectionStatusCanBeSetAndResetForNewDay) {
    WasteNode node(3, "Collectable", 0.0f, 0.0f, 100.0f);
    node.setWasteLevel(80.0f);
    node.setCollected(true);
    REQUIRE_TRUE(node.isCollected());
    node.resetForNewDay();
    REQUIRE_FALSE(node.isCollected());
    REQUIRE_NEAR(node.getWasteLevel(), 80.0f, 0.001f);
}
