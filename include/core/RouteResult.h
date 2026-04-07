#ifndef ROUTE_RESULT_H
#define ROUTE_RESULT_H

#include <string>
#include <vector>

// Stores the complete output of running one routing algorithm.
// This struct bundles the route order with all computed metrics
// so we can display, compare, and export results easily.
struct RouteResult {
    std::string algorithmName;
    std::vector<int> visitOrder;    // sequence of node IDs visited
    float totalDistance;            // in kilometers
    float travelTime;              // in hours
    float fuelCost;                // in RM
    float wageCost;                // in RM
    float totalCost;               // fuel + wage combined
    float wasteCollected;          // in kg
    double runtimeMs;              // how long the algorithm took to compute

    RouteResult();

    // Quick summary string for logging
    std::string toSummaryString() const;

    // Check if this result is valid (has at least one node visited)
    bool isValid() const;
};

#endif // ROUTE_RESULT_H
