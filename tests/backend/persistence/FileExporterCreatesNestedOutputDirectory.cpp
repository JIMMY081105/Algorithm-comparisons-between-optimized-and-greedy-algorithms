#include "PersistenceTestUtils.h"

TEST_CASE(FileExporterCreatesNestedOutputDirectory) {
    WasteSystem system;
    initializeExportReadySystem(system);
    RouteResult result = exportReadyResult(system);
    const auto dir = uniqueTempDirectory("nested") / "a" / "b" / "c";
    FileExporter exporter(dir.string());

    const std::string path = exporter.exportSummaryTxt(result, system);
    REQUIRE_TRUE(std::filesystem::exists(dir));
    REQUIRE_TRUE(std::filesystem::exists(path));
}
