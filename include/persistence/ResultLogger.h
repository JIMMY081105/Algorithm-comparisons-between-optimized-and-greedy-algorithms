#ifndef RESULT_LOGGER_H
#define RESULT_LOGGER_H

#include "FileExporter.h"
#include "algorithms/ComparisonManager.h"
#include "core/WasteSystem.h"

#include <string>

// Application-facing export service that wraps FileExporter with higher-level
// actions such as "export the current result" or "export the comparison set".
class ResultLogger {
private:
    FileExporter exporter;

public:
    ResultLogger();

    std::string logCurrentResult(const RouteResult& result, WasteSystem& system);
    std::string logComparison(const ComparisonManager& compMgr, WasteSystem& system);
    std::string logRouteDetails(const RouteResult& result, WasteSystem& system);
    std::string getOutputPath() const;
};

#endif // RESULT_LOGGER_H
