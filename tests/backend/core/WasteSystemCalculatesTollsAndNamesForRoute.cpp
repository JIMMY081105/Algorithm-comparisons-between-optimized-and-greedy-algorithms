#include "CoreTestUtils.h"

TEST_CASE(WasteSystemCalculatesTollsAndNamesForRoute) {
    WasteSystem system;
    system.initializeMap();
    const std::vector<int> route = {0, 15, 7, 15, 0};
    REQUIRE_NEAR(system.calculateTollCost(route), 11.0f, 0.001f);
    const std::vector<std::string> names = system.getTollNamesCrossed(route);
    REQUIRE_EQ(names.size(), static_cast<std::size_t>(4));
    REQUIRE_EQ(names.front(), std::string("Central Gate"));
}
