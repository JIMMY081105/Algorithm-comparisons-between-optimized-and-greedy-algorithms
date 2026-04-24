#include "PersistenceTestUtils.h"

TEST_CASE(FileExporterExposesConfiguredOutputDirectory) {
    const auto dir = uniqueTempDirectory("configured");
    FileExporter exporter(dir.string());
    REQUIRE_EQ(exporter.getOutputDirectory(), dir.string());
}
