#include "GraphTestUtils.h"

TEST_CASE(MapGraphCalculateRouteDistanceUsesMatrixWeights) {
    MapGraph graph = makeFourNodeGraph();
    REQUIRE_NEAR(graph.calculateRouteDistance({0, 1, 2, 3, 0}),
                 16.0f, 0.001f);
}
