#include "PersistenceTestUtils.h"

TEST_CASE(FullPipelineIsDeterministicForSameSeedAtBackendLevel) {
    WasteSystem first;
    first.initializeMap();
    first.generateNewDayWithSeed(8080u);
    ComparisonManager firstManager;
    firstManager.initializeAlgorithms();
    firstManager.runAllAlgorithms(first);

    WasteSystem second;
    second.initializeMap();
    second.generateNewDayWithSeed(8080u);
    ComparisonManager secondManager;
    secondManager.initializeAlgorithms();
    secondManager.runAllAlgorithms(second);

    REQUIRE_EQ(firstManager.getResults().size(), secondManager.getResults().size());
    for (std::size_t i = 0; i < firstManager.getResults().size(); ++i) {
        REQUIRE_EQ(firstManager.getResults()[i].algorithmName,
                   secondManager.getResults()[i].algorithmName);
        REQUIRE_TRUE(firstManager.getResults()[i].visitOrder ==
                     secondManager.getResults()[i].visitOrder);
        REQUIRE_NEAR(firstManager.getResults()[i].totalCost,
                     secondManager.getResults()[i].totalCost, 0.001f);
    }
}
