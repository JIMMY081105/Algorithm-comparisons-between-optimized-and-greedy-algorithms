#ifndef PERSISTENCE_TEST_UTILS_H
#define PERSISTENCE_TEST_UTILS_H

#include "TestHarness.h"

#include "algorithms/ComparisonManager.h"
#include "persistence/FileExporter.h"
#include "persistence/ResultLogger.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

inline std::string readWholeFile(const std::string& path) {
    std::ifstream input(path);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

inline std::filesystem::path uniqueTempDirectory(const std::string& name) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("cw2_" + name);
    std::filesystem::remove_all(dir);
    return dir;
}

inline void initializeExportReadySystem(WasteSystem& system) {
    system.initializeMap();
    system.generateNewDayWithSeed(777u);
}

inline RouteResult exportReadyResult(WasteSystem& system) {
    RouteResult result;
    result.algorithmName = "Manual Export";
    result.visitOrder = {0, 15, 7, 15, 0};
    system.populateCosts(result);
    result.runtimeMs = 0.5;
    return result;
}

#endif // PERSISTENCE_TEST_UTILS_H
