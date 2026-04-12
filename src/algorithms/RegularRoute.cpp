#include "algorithms/RegularRoute.h"
#include "AlgorithmUtils.h"

RouteResult RegularRouteAlgorithm::computeRoute(const MapGraph& graph,
                                                const std::vector<int>& eligibleIds,
                                                int hqId) const {
    const auto startTime = AlgorithmUtils::RouteClock::now();
    (void)graph;

    RouteResult result = AlgorithmUtils::makeBaseResult(algorithmName());

    if (eligibleIds.empty()) {
        result.runtimeMs = 0.0;
        return result;
    }

    // The regular route simply visits nodes in their default order (by ID).
    // This is intentionally non-optimized to serve as a baseline.
    // The truck starts at HQ, visits each eligible node sequentially,
    // and returns to HQ at the end.

    result.visitOrder = AlgorithmUtils::buildWorkingNodeIds(eligibleIds, hqId);
    result.visitOrder.push_back(hqId);
    AlgorithmUtils::finalizeRuntime(result, startTime);

    return result;
}

std::string RegularRouteAlgorithm::algorithmName() const {
    return "Regular Route";
}

std::string RegularRouteAlgorithm::description() const {
    return "Non-optimized baseline that visits nodes in default ID order";
}
