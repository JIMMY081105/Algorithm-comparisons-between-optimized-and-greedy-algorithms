#include "CoreTestUtils.h"

TEST_CASE(CostModelZeroSpeedReturnsZeroTravelTime) {
    CostModel model;
    model.setTruckSpeedKmh(0.0f);
    REQUIRE_NEAR(model.calculateTravelTime(100.0f), 0.0f, 0.001f);
}
