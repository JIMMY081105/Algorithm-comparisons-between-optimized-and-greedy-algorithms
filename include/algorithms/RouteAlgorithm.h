#ifndef ROUTE_ALGORITHM_H
#define ROUTE_ALGORITHM_H

#include <string>
#include "core/RouteResult.h"
#include "core/MapGraph.h"

// Abstract base class for all routing algorithms.
// This demonstrates polymorphism — each derived algorithm implements
// its own strategy for computeRoute(), and the ComparisonManager can
// call any of them through this common interface without knowing
// which specific algorithm it's dealing with.
class RouteAlgorithm {
public:
    virtual ~RouteAlgorithm() = default;

    // Run the routing algorithm and return the result.
    // Parameters:
    //   graph        — the map with all node positions and distances
    //   eligibleIds  — which node IDs are above the waste threshold
    //   hqId         — the ID of the HQ/depot node
    virtual RouteResult computeRoute(const MapGraph& graph,
                                     const std::vector<int>& eligibleIds,
                                     int hqId) = 0;

    // Human-readable name for display in the UI and export files
    virtual std::string algorithmName() const = 0;

    // Short description for tooltips or the comparison panel
    virtual std::string description() const = 0;
};

#endif // ROUTE_ALGORITHM_H
