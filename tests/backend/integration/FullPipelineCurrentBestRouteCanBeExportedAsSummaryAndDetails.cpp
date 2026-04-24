#include "PersistenceTestUtils.h"

TEST_CASE(FullPipelineCurrentBestRouteCanBeExportedAsSummaryAndDetails) {
    WasteSystem system;
    system.initializeMap();
    system.generateNewDayWithSeed(314159u);

    ComparisonManager manager;
    manager.initializeAlgorithms();
    manager.runAllAlgorithms(system);
    const RouteResult& best = manager.getResults()[manager.getBestAlgorithmIndex()];

    const auto dir = uniqueTempDirectory("best_route");
    FileExporter exporter(dir.string());
    const std::string summary = exporter.exportSummaryTxt(best, system);
    const std::string details = exporter.exportRouteDetailsTxt(best, system);

    REQUIRE_TRUE(std::filesystem::exists(summary));
    REQUIRE_TRUE(std::filesystem::exists(details));
    REQUIRE_TRUE(readWholeFile(summary).find(best.algorithmName) !=
                 std::string::npos);
    REQUIRE_TRUE(readWholeFile(details).find("Total:") != std::string::npos);
}
