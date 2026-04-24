#include "GraphTestUtils.h"

TEST_CASE(ComparisonManagerRegistersAllAlgorithms) {
    ComparisonManager manager;
    manager.initializeAlgorithms();
    REQUIRE_EQ(manager.getAlgorithmCount(), 7);
    REQUIRE_EQ(manager.getAlgorithmName(0), std::string("Regular Route"));
    REQUIRE_TRUE(manager.getAlgorithmDescription(3).find("bitmask") !=
                 std::string::npos);
    REQUIRE_EQ(manager.getAlgorithmName(100), std::string("Unknown"));
}
