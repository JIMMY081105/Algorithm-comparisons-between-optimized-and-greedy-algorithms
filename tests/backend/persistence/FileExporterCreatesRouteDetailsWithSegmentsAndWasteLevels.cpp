#include "PersistenceTestUtils.h"

TEST_CASE(FileExporterCreatesRouteDetailsWithSegmentsAndWasteLevels) {
    WasteSystem system;
    initializeExportReadySystem(system);
    RouteResult result = exportReadyResult(system);
    const auto dir = uniqueTempDirectory("details");
    FileExporter exporter(dir.string());

    const std::string path = exporter.exportRouteDetailsTxt(result, system);
    REQUIRE_TRUE(std::filesystem::exists(path));
    const std::string text = readWholeFile(path);
    REQUIRE_TRUE(text.find("Step-by-step route") != std::string::npos);
    REQUIRE_TRUE(text.find("Command Anchorage -> Equator Sweep") !=
                 std::string::npos);
    REQUIRE_TRUE(text.find("Waste at each location") != std::string::npos);
}
