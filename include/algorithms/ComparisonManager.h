#ifndef COMPARISON_MANAGER_H
#define COMPARISON_MANAGER_H

#include <vector>
#include <memory>
#include "RouteAlgorithm.h"
#include "core/WasteSystem.h"

// Orchestrates running all routing algorithms and collecting their results.
// Uses polymorphism — it stores RouteAlgorithm pointers and calls each
// one through the virtual interface, demonstrating a key OOP concept.
class ComparisonManager {
private:
    std::vector<std::unique_ptr<RouteAlgorithm>> algorithms;
    std::vector<RouteResult> lastResults;

public:
    ComparisonManager();

    // Register all four algorithms
    void initializeAlgorithms();

    // Run every registered algorithm against the current waste system state
    void runAllAlgorithms(WasteSystem& system);

    // Run a single algorithm by index
    RouteResult runSingleAlgorithm(int index, WasteSystem& system);

    // Access results
    const std::vector<RouteResult>& getResults() const;
    int getAlgorithmCount() const;
    std::string getAlgorithmName(int index) const;
    std::string getAlgorithmDescription(int index) const;

    // Find which algorithm produced the lowest total cost
    int getBestAlgorithmIndex() const;

    // Clear previous results
    void clearResults();
};

#endif // COMPARISON_MANAGER_H
