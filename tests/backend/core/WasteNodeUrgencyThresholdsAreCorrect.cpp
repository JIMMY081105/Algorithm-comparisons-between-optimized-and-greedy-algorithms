#include "CoreTestUtils.h"

TEST_CASE(WasteNodeUrgencyThresholdsAreCorrect) {
    WasteNode node(1, "Test", 0.0f, 0.0f, 100.0f);
    node.setWasteLevel(39.99f);
    REQUIRE_TRUE(node.getUrgency() == UrgencyLevel::LOW);
    node.setWasteLevel(40.0f);
    REQUIRE_TRUE(node.getUrgency() == UrgencyLevel::MEDIUM);
    node.setWasteLevel(69.99f);
    REQUIRE_TRUE(node.getUrgency() == UrgencyLevel::MEDIUM);
    node.setWasteLevel(70.0f);
    REQUIRE_TRUE(node.getUrgency() == UrgencyLevel::HIGH);
}
