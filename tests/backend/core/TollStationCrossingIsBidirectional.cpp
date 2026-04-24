#include "CoreTestUtils.h"

TEST_CASE(TollStationCrossingIsBidirectional) {
    TollStation toll(4, 7, 3.5f, "Gate");
    REQUIRE_TRUE(toll.isCrossedBy(4, 7));
    REQUIRE_TRUE(toll.isCrossedBy(7, 4));
    REQUIRE_FALSE(toll.isCrossedBy(4, 8));
    REQUIRE_NEAR(toll.fee(), 3.5f, 0.001f);
}
