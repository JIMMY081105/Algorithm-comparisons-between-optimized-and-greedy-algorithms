#include "algorithms/RegularRoute.h"
#include <chrono>

RouteResult RegularRouteAlgorithm::computeRoute(const MapGraph& graph,
                                                 const std::vector<int>& eligibleIds,
                                                 int hqId) {
    auto startTime = std::chrono::high_resolution_clock::now();

    RouteResult result;
    result.algorithmName = algorithmName();

    if (eligibleIds.empty()) {
        result.runtimeMs = 0.0;
        return result;
    }

    // The regular route simply visits nodes in their default order (by ID).
    // This is intentionally non-optimized to serve as a baseline.
    // The truck starts at HQ, visits each eligible node sequentially,
    // and returns to HQ at the end.

    result.visitOrder.push_back(hqId);

    for (int nodeId : eligibleIds) {
        if (nodeId != hqId) {
            result.visitOrder.push_back(nodeId);
        }
    }

    // Return to HQ at the end
    result.visitOrder.push_back(hqId);

    auto endTime = std::chrono::high_resolution_clock::now();
    result.runtimeMs = std::chrono::duration<double, std::milli>(
        endTime - startTime).count();

    return result;
}

std::string RegularRouteAlgorithm::algorithmName() const {
    return "Regular Route";
}

std::string RegularRouteAlgorithm::description() const {
    return "Non-optimized baseline that visits nodes in default ID order";
}
