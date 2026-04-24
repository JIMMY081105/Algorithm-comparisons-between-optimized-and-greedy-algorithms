#include "CoreTestUtils.h"

TEST_CASE(WasteSystemInitializesFixedMapAndTolls) {
    WasteSystem system;
    system.initializeMap();
    REQUIRE_EQ(system.getGraph().getNodeCount(), 21);
    REQUIRE_EQ(system.getGraph().getHQNode().getId(), 0);
    REQUIRE_EQ(system.getTollStations().size(), static_cast<std::size_t>(7));
    REQUIRE_TRUE(system.getEventLog().getCount() >= 1);
}
