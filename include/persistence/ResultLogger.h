#ifndef RESULT_LOGGER_H
#define RESULT_LOGGER_H

#include "FileExporter.h"
#include "core/WasteSystem.h"
#include "algorithms/ComparisonManager.h"

// Higher-level persistence manager that coordinates exports.
// Wraps FileExporter with convenience methods that the Application
// layer calls when the user presses export buttons.
class ResultLogger {
private:
    FileExporter exporter;

public:
    ResultLogger();

    // Export the current single-algorithm result
    std::string logCurrentResult(const RouteResult& result,
                                  WasteSystem& system);

    // Export the full comparison of all algorithms
    std::string logComparison(const ComparisonManager& compMgr,
                               WasteSystem& system);

    // Export detailed route for one algorithm
    std::string logRouteDetails(const RouteResult& result,
                                 WasteSystem& system);

    // Get the output folder path for display in the UI
    std::string getOutputPath() const;
};

#endif // RESULT_LOGGER_H
