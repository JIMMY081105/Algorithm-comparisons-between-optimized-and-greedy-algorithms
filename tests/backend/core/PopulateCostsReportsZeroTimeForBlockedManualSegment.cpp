#include "CoreTestUtils.h"

TEST_CASE(PopulateCostsReportsZeroTimeForBlockedManualSegment) {
    WasteSystem system;
    system.initializeMap();
    system.getGraph().setEdgeEvent(0, 15, RoadEvent::FLOOD);

    RouteResult result;
    result.visitOrder = {0, 15};
    system.populateCosts(result);

    REQUIRE_NEAR(result.travelTime, 0.0f, 0.001f);
    REQUIRE_TRUE(result.fuelCost > 0.0f);
}
