#ifndef COMPARISON_MANAGER_H
#define COMPARISON_MANAGER_H

#include "RouteAlgorithm.h"
#include "core/WasteSystem.h"

#include <memory>
#include <string>
#include <vector>

// Owns the available routing strategies and provides a consistent way to run
// either one algorithm or the full comparison set against the current day.
class ComparisonManager {
private:
    std::vector<std::unique_ptr<RouteAlgorithm>> algorithms;
    std::vector<RouteResult> lastResults;

public:
    ComparisonManager() = default;

    void initializeAlgorithms();
    void runAllAlgorithms(WasteSystem& system);
    RouteResult runSingleAlgorithm(int index, WasteSystem& system);

    const std::vector<RouteResult>& getResults() const;
    int getAlgorithmCount() const;
    std::string getAlgorithmName(int index) const;
    std::string getAlgorithmDescription(int index) const;
    int getBestAlgorithmIndex() const;
    void clearResults();
};

#endif // COMPARISON_MANAGER_H
