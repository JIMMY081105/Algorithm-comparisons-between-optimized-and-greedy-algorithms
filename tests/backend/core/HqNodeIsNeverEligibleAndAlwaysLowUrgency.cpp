#include "CoreTestUtils.h"

TEST_CASE(HqNodeIsNeverEligibleAndAlwaysLowUrgency) {
    WasteNode hq(0, "HQ", 0.0f, 0.0f, 0.0f, true);
    hq.setWasteLevel(100.0f);
    REQUIRE_FALSE(hq.isEligible(0.0f));
    REQUIRE_TRUE(hq.getUrgency() == UrgencyLevel::LOW);
}
