#include "persistence/ResultLogger.h"

ResultLogger::ResultLogger() {}

std::string ResultLogger::logCurrentResult(const RouteResult& result,
                                            WasteSystem& system) {
    std::string filename = exporter.exportSummaryTxt(result, system);
    system.getEventLog().addEvent("Exported summary to: " + filename);
    return filename;
}

std::string ResultLogger::logComparison(const ComparisonManager& compMgr,
                                         WasteSystem& system) {
    const auto& results = compMgr.getResults();
    if (results.empty()) {
        system.getEventLog().addEvent("Export failed: no comparison results available");
        return "";
    }

    std::string filename = exporter.exportComparisonCsv(results, system);
    system.getEventLog().addEvent("Exported comparison CSV to: " + filename);
    return filename;
}

std::string ResultLogger::logRouteDetails(const RouteResult& result,
                                            WasteSystem& system) {
    std::string filename = exporter.exportRouteDetailsTxt(result, system);
    system.getEventLog().addEvent("Exported route details to: " + filename);
    return filename;
}

std::string ResultLogger::getOutputPath() const {
    return exporter.getOutputDirectory();
}
