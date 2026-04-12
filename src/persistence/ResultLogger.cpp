#include "persistence/ResultLogger.h"

namespace {
std::string logExportEvent(WasteSystem& system,
                           const std::string& filename,
                           const char* successMessage) {
    system.getEventLog().addEvent(std::string(successMessage) + filename);
    return filename;
}
} // namespace

std::string ResultLogger::logCurrentResult(const RouteResult& result,
                                           WasteSystem& system) {
    return logExportEvent(system,
                          exporter.exportSummaryTxt(result, system),
                          "Exported summary to: ");
}

std::string ResultLogger::logComparison(const ComparisonManager& compMgr,
                                        WasteSystem& system) {
    const auto& results = compMgr.getResults();
    if (results.empty()) {
        system.getEventLog().addEvent(
            "Export failed: no comparison results available");
        return "";
    }

    return logExportEvent(system,
                          exporter.exportComparisonCsv(results, system),
                          "Exported comparison CSV to: ");
}

std::string ResultLogger::logRouteDetails(const RouteResult& result,
                                          WasteSystem& system) {
    return logExportEvent(system,
                          exporter.exportRouteDetailsTxt(result, system),
                          "Exported route details to: ");
}

const std::string& ResultLogger::getOutputPath() const {
    return exporter.getOutputDirectory();
}
