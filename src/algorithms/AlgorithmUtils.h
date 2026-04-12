#ifndef ALGORITHM_UTILS_H
#define ALGORITHM_UTILS_H

#include "core/RouteResult.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

namespace AlgorithmUtils {

using RouteClock = std::chrono::steady_clock;

inline RouteResult makeBaseResult(const std::string& algorithmName) {
    RouteResult result;
    result.algorithmName = algorithmName;
    return result;
}

inline void finalizeRuntime(RouteResult& result,
                            const RouteClock::time_point& startTime) {
    result.runtimeMs = std::chrono::duration<double, std::milli>(
        RouteClock::now() - startTime).count();
}

inline std::vector<int> buildWorkingNodeIds(const std::vector<int>& eligibleIds,
                                            int hqId) {
    std::vector<int> nodeIds;
    nodeIds.reserve(eligibleIds.size() + 1);
    nodeIds.push_back(hqId);

    for (const int nodeId : eligibleIds) {
        if (nodeId != hqId) {
            nodeIds.push_back(nodeId);
        }
    }

    return nodeIds;
}

inline int findNodePosition(const std::vector<int>& nodeIds, int targetId) {
    const auto it = std::find(nodeIds.begin(), nodeIds.end(), targetId);
    if (it == nodeIds.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(nodeIds.begin(), it));
}

} // namespace AlgorithmUtils

#endif // ALGORITHM_UTILS_H
