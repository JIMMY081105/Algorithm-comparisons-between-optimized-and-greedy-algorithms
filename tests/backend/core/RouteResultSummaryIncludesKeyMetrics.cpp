#include "CoreTestUtils.h"

TEST_CASE(RouteResultSummaryIncludesKeyMetrics) {
    RouteResult result;
    result.algorithmName = "Unit";
    result.visitOrder = {0, 1, 0};
    result.totalDistance = 12.345f;
    result.fuelCost = 10.0f;
    result.wageCost = 20.0f;
    result.tollCost = 3.0f;
    result.totalCost = 33.0f;
    result.wasteCollected = 44.0f;
    result.runtimeMs = 1.25;

    const std::string summary = result.toSummaryString();
    REQUIRE_TRUE(result.isValid());
    REQUIRE_TRUE(summary.find("Unit") != std::string::npos);
    REQUIRE_TRUE(summary.find("Total RM 33.00") != std::string::npos);
}
