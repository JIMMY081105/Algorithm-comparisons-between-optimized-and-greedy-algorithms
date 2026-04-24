#include "TestHarness.h"

#include "algorithms/ComparisonManager.h"
#include "persistence/FileExporter.h"
#include "persistence/ResultLogger.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string readWholeFile(const std::string& path) {
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::filesystem::path uniqueTempDirectory(const std::string& name) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("cw2_" + name);
    std::filesystem::remove_all(dir);
    return dir;
}

void initializeExportReadySystem(WasteSystem& system) {
    system.initializeMap();
    system.generateNewDayWithSeed(777u);
}

RouteResult exportReadyResult(WasteSystem& system) {
    RouteResult result;
    result.algorithmName = "Manual Export";
    result.visitOrder = {0, 15, 7, 15, 0};
    system.populateCosts(result);
    result.runtimeMs = 0.5;
    return result;
}

} // namespace

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

TEST_CASE(FileExporterExposesConfiguredOutputDirectory) {
    const auto dir = uniqueTempDirectory("configured");
    FileExporter exporter(dir.string());
    REQUIRE_EQ(exporter.getOutputDirectory(), dir.string());
}

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

TEST_CASE(FullPipelineThresholdCanProduceNoRoutesWithoutCrashing) {
    WasteSystem system;
    system.initializeMap();
    system.generateNewDayWithSeed(9090u);
    system.setCollectionThreshold(101.0f);

    ComparisonManager manager;
    manager.initializeAlgorithms();
    manager.runAllAlgorithms(system);

    REQUIRE_EQ(manager.getResults().size(), static_cast<std::size_t>(7));
    REQUIRE_EQ(manager.getBestAlgorithmIndex(), -1);
    for (const RouteResult& result : manager.getResults()) {
        REQUIRE_FALSE(result.isValid());
        REQUIRE_TRUE(result.visitOrder.empty());
    }
}
