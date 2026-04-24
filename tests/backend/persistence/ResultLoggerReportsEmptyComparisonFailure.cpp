#include "PersistenceTestUtils.h"

TEST_CASE(ResultLoggerReportsEmptyComparisonFailure) {
    WasteSystem system;
    initializeExportReadySystem(system);
    ComparisonManager manager;
    manager.initializeAlgorithms();
    ResultLogger logger;

    const int before = system.getEventLog().getCount();
    const std::string path = logger.logComparison(manager, system);

    REQUIRE_TRUE(path.empty());
    REQUIRE_EQ(system.getEventLog().getCount(), before + 1);
    const auto recent = system.getEventLog().getRecentEvents(1);
    REQUIRE_TRUE(recent[0]->message.find("Export failed") != std::string::npos);
}
