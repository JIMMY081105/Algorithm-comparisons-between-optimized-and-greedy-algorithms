#include "algorithms/ComparisonManager.h"

#include "algorithms/GreedyRoute.h"
#include "algorithms/MSTRoute.h"
#include "algorithms/RegularRoute.h"
#include "algorithms/TSPRoute.h"

#include <sstream>
#include <stdexcept>

namespace {
struct RouteRequestContext {
    std::vector<int> eligibleNodeIds;
    int depotNodeId = -1;
};

RouteRequestContext buildRouteRequestContext(const WasteSystem& system) {
    return RouteRequestContext{
        system.getEligibleNodes(),
        system.getGraph().getHQNode().getId()
    };
}

RouteResult executeAlgorithm(RouteAlgorithm& algorithm,
                             WasteSystem& system,
                             const RouteRequestContext& request) {
    // Each algorithm should evaluate the same simulated day from the same
    // collection state, otherwise the comparison would be misleading.
    system.resetCollectionStatus();

    RouteResult result = algorithm.computeRoute(
        system.getGraph(), request.eligibleNodeIds, request.depotNodeId);
    system.populateCosts(result);
    return result;
}

void logAlgorithmCompletion(WasteSystem& system,
                            const RouteAlgorithm& algorithm,
                            const RouteResult& result) {
    std::ostringstream message;
    message << algorithm.algorithmName() << " completed: "
            << result.totalDistance << " km, RM " << result.totalCost;
    system.getEventLog().addEvent(message.str());
}
} // namespace

ComparisonManager::ComparisonManager() {}

void ComparisonManager::initializeAlgorithms() {
    algorithms.clear();

    // Keep registration centralized so the dashboard, comparison table, and
    // execution order all rely on the same algorithm list.
    algorithms.push_back(std::make_unique<RegularRouteAlgorithm>());
    algorithms.push_back(std::make_unique<GreedyRouteAlgorithm>());
    algorithms.push_back(std::make_unique<MSTRouteAlgorithm>());
    algorithms.push_back(std::make_unique<TSPRouteAlgorithm>());
}

void ComparisonManager::runAllAlgorithms(WasteSystem& system) {
    lastResults.clear();

    const RouteRequestContext request = buildRouteRequestContext(system);
    for (auto& algorithm : algorithms) {
        RouteResult result = executeAlgorithm(*algorithm, system, request);
        logAlgorithmCompletion(system, *algorithm, result);
        lastResults.push_back(result);
    }

    system.resetCollectionStatus();

    const int bestIndex = getBestAlgorithmIndex();
    if (bestIndex >= 0) {
        system.getEventLog().addEvent(
            "Recommended: " + lastResults[bestIndex].algorithmName +
            " (lowest total cost)");
    }
}

RouteResult ComparisonManager::runSingleAlgorithm(int index, WasteSystem& system) {
    if (index < 0 || index >= static_cast<int>(algorithms.size())) {
        throw std::out_of_range(
            "ComparisonManager::runSingleAlgorithm - invalid index");
    }

    const RouteRequestContext request = buildRouteRequestContext(system);
    RouteResult result = executeAlgorithm(*algorithms[index], system, request);
    logAlgorithmCompletion(system, *algorithms[index], result);
    return result;
}

const std::vector<RouteResult>& ComparisonManager::getResults() const {
    return lastResults;
}

int ComparisonManager::getAlgorithmCount() const {
    return static_cast<int>(algorithms.size());
}

std::string ComparisonManager::getAlgorithmName(int index) const {
    if (index < 0 || index >= static_cast<int>(algorithms.size())) {
        return "Unknown";
    }
    return algorithms[index]->algorithmName();
}

std::string ComparisonManager::getAlgorithmDescription(int index) const {
    if (index < 0 || index >= static_cast<int>(algorithms.size())) {
        return "";
    }
    return algorithms[index]->description();
}

int ComparisonManager::getBestAlgorithmIndex() const {
    int bestIndex = -1;
    float lowestCost = 0.0f;

    for (int i = 0; i < static_cast<int>(lastResults.size()); ++i) {
        const RouteResult& result = lastResults[i];
        if (!result.isValid()) {
            continue;
        }

        if (bestIndex < 0 || result.totalCost < lowestCost) {
            bestIndex = i;
            lowestCost = result.totalCost;
        }
    }

    return bestIndex;
}

void ComparisonManager::clearResults() {
    lastResults.clear();
}
