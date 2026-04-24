#include "CoreTestUtils.h"

TEST_CASE(TollStationValidatesConstructorArguments) {
    REQUIRE_THROWS_AS(TollStation(1, 1, 1.0f, "Bad"), std::invalid_argument);
    REQUIRE_THROWS_AS(TollStation(1, 2, -0.1f, "Bad"), std::invalid_argument);
    REQUIRE_THROWS_AS(TollStation(1, 2, 1.0f, ""), std::invalid_argument);
}
