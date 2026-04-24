#include "PersistenceTestUtils.h"

TEST_CASE(FullPipelineGeneratesScenarioComparesAndExportsCsv) {
    WasteSystem system;
    system.initializeMap();
    system.generateNewDayWithSeed(20260424u);

    ComparisonManager manager;
    manager.initializeAlgorithms();
    manager.runAllAlgorithms(system);

    const auto dir = uniqueTempDirectory("pipeline");
    FileExporter exporter(dir.string());
    const std::string path = exporter.exportComparisonCsv(manager.getResults(), system);
    const std::string text = readWholeFile(path);

    REQUIRE_EQ(manager.getResults().size(), static_cast<std::size_t>(7));
    REQUIRE_TRUE(manager.getBestAlgorithmIndex() >= 0);
    REQUIRE_TRUE(std::filesystem::exists(path));
    REQUIRE_TRUE(text.find("Regular Route") != std::string::npos);
    REQUIRE_TRUE(text.find("Floyd-Warshall Route") != std::string::npos);
}
