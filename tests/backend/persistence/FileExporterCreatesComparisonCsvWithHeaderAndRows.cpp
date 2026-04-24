#include "PersistenceTestUtils.h"

TEST_CASE(FileExporterCreatesComparisonCsvWithHeaderAndRows) {
    WasteSystem system;
    initializeExportReadySystem(system);
    RouteResult first = exportReadyResult(system);
    RouteResult second = first;
    second.algorithmName = "Second";
    second.totalCost += 10.0f;
    const auto dir = uniqueTempDirectory("comparison");
    FileExporter exporter(dir.string());

    const std::string path = exporter.exportComparisonCsv({first, second}, system);
    REQUIRE_TRUE(std::filesystem::exists(path));
    const std::string text = readWholeFile(path);
    REQUIRE_TRUE(text.find("Algorithm,Distance (km),Travel Time (h)") !=
                 std::string::npos);
    REQUIRE_TRUE(text.find("Manual Export") != std::string::npos);
    REQUIRE_TRUE(text.find("Second") != std::string::npos);
    REQUIRE_TRUE(text.find("Seed,777") != std::string::npos);
}
