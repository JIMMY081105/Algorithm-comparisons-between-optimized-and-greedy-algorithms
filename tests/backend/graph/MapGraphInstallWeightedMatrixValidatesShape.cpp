#include "GraphTestUtils.h"

TEST_CASE(MapGraphInstallWeightedMatrixValidatesShape) {
    MapGraph graph = makeFourNodeGraph();
    REQUIRE_THROWS_AS(graph.installWeightedMatrix({{0.0f, 1.0f}}),
                      std::invalid_argument);
}
