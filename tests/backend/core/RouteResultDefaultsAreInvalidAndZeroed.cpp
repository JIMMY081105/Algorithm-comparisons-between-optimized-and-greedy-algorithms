#include "CoreTestUtils.h"

TEST_CASE(RouteResultDefaultsAreInvalidAndZeroed) {
    RouteResult result;
    REQUIRE_EQ(result.algorithmName, std::string("None"));
    REQUIRE_FALSE(result.isValid());
    REQUIRE_NEAR(result.totalCost, 0.0f, 0.001f);
}
