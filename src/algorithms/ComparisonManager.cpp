#include "algorithms/ComparisonManager.h"
#include "algorithms/RegularRoute.h"
#include "algorithms/GreedyRoute.h"
#include "algorithms/MSTRoute.h"
#include "algorithms/TSPRoute.h"
#include <sstream>

ComparisonManager::ComparisonManager() {}

void ComparisonManager::initializeAlgorithms() {
    // Register all four routing strategies.
    // Using unique_ptr ensures proper cleanup and demonstrates
    // modern C++ resource management.
    algorithms.clear();
    algorithms.push_back(std::make_unique<RegularRouteAlgorithm>());
    algorithms.push_back(std::make_unique<GreedyRouteAlgorithm>());
    algorithms.push_back(std::make_unique<MSTRouteAlgorithm>());
    algorithms.push_back(std::make_unique<TSPRouteAlgorithm>());
}

void ComparisonManager::runAllAlgorithms(WasteSystem& system) {
    lastResults.clear();

    std::vector<int> eligible = system.getEligibleNodes();
    int hqId = system.getGraph().getHQNode().getId();

    for (auto& algo : algorithms) {
        // Reset collection status before each algorithm runs,
        // so each one starts from a clean state
        system.resetCollectionStatus();

        RouteResult result = algo->computeRoute(system.getGraph(), eligible, hqId);
        system.populateCosts(result);

        // Log the run in the event log
        std::ostringstream msg;
        msg << algo->algorithmName() << " completed: "
            << result.totalDistance << " km, RM " << result.totalCost;
        system.getEventLog().addEvent(msg.str());

        lastResults.push_back(result);
    }

    system.resetCollectionStatus();

    // Identify and log the recommended algorithm
    int bestIdx = getBestAlgorithmIndex();
    if (bestIdx >= 0) {
        system.getEventLog().addEvent(
            "Recommended: " + lastResults[bestIdx].algorithmName +
            " (lowest total cost)");
    }
}

RouteResult ComparisonManager::runSingleAlgorithm(int index, WasteSystem& system) {
    if (index < 0 || index >= static_cast<int>(algorithms.size())) {
        throw std::out_of_range("ComparisonManager::runSingleAlgorithm — invalid index");
    }

    system.resetCollectionStatus();

    std::vector<int> eligible = system.getEligibleNodes();
    int hqId = system.getGraph().getHQNode().getId();

    RouteResult result = algorithms[index]->computeRoute(
        system.getGraph(), eligible, hqId);
    system.populateCosts(result);

    std::ostringstream msg;
    msg << algorithms[index]->algorithmName() << " completed: "
        << result.totalDistance << " km, RM " << result.totalCost;
    system.getEventLog().addEvent(msg.str());

    return result;
}

const std::vector<RouteResult>& ComparisonManager::getResults() const {
    return lastResults;
}

int ComparisonManager::getAlgorithmCount() const {
    return static_cast<int>(algorithms.size());
}

std::string ComparisonManager::getAlgorithmName(int index) const {
    if (index < 0 || index >= static_cast<int>(algorithms.size())) return "Unknown";
    return algorithms[index]->algorithmName();
}

std::string ComparisonManager::getAlgorithmDescription(int index) const {
    if (index < 0 || index >= static_cast<int>(algorithms.size())) return "";
    return algorithms[index]->description();
}

int ComparisonManager::getBestAlgorithmIndex() const {
    if (lastResults.empty()) return -1;

    int bestIdx = 0;
    float bestCost = lastResults[0].totalCost;

    for (int i = 1; i < static_cast<int>(lastResults.size()); i++) {
        if (lastResults[i].isValid() && lastResults[i].totalCost < bestCost) {
            bestCost = lastResults[i].totalCost;
            bestIdx = i;
        }
    }

    return bestIdx;
}

void ComparisonManager::clearResults() {
    lastResults.clear();
}
