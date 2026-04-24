#include "PersistenceTestUtils.h"

TEST_CASE(FileExporterCreatesSummaryTxtWithRouteMetrics) {
    WasteSystem system;
    initializeExportReadySystem(system);
    RouteResult result = exportReadyResult(system);
    const auto dir = uniqueTempDirectory("summary");
    FileExporter exporter(dir.string());

    const std::string path = exporter.exportSummaryTxt(result, system);
    REQUIRE_TRUE(std::filesystem::exists(path));
    const std::string text = readWholeFile(path);
    REQUIRE_TRUE(text.find("Ocean Cleanup System - Route Summary") !=
                 std::string::npos);
    REQUIRE_TRUE(text.find("Manual Export") != std::string::npos);
    REQUIRE_TRUE(text.find("Total Cost") != std::string::npos);
}
