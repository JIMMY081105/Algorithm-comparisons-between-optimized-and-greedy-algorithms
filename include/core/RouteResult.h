#ifndef ROUTE_RESULT_H
#define ROUTE_RESULT_H

#include <string>
#include <vector>

// Stores the complete output of running one routing algorithm.
// Wage breakdown:
//   wageCost = basePay + perKmBonus + efficiencyBonus
// Full cost:
//   totalCost = fuelCost + wageCost + tollCost
struct RouteResult {
    std::string algorithmName;
    std::vector<int> visitOrder;    // sequence of node IDs visited
    float totalDistance;            // in kilometers
    float travelTime;               // in hours

    // Fuel
    float fuelCost;                 // litres/km * daily price * distance

    // Wage breakdown
    float basePay;                  // flat shift pay (RM)
    float perKmBonus;               // distance incentive (RM)
    float efficiencyBonus;          // reward for short routes (RM)
    float wageCost;                 // basePay + perKmBonus + efficiencyBonus

    // Tolls
    float tollCost;                 // sum of all toll fees crossed
    std::vector<std::string> tollsCrossed;  // names of tolls hit

    float totalCost;                // fuelCost + wageCost + tollCost
    float wasteCollected;           // in kg
    double runtimeMs;               // algorithm compute time

    RouteResult();

    std::string toSummaryString() const;
    bool isValid() const;
};

#endif // ROUTE_RESULT_H
