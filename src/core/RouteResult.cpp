#include "core/RouteResult.h"
#include <sstream>
#include <iomanip>

RouteResult::RouteResult()
    : algorithmName("None"),
      totalDistance(0.0f), travelTime(0.0f),
      fuelCost(0.0f),
      basePay(0.0f), perKmBonus(0.0f), efficiencyBonus(0.0f), wageCost(0.0f),
      tollCost(0.0f),
      totalCost(0.0f), wasteCollected(0.0f), runtimeMs(0.0) {}

std::string RouteResult::toSummaryString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << algorithmName << ": "
        << totalDistance << " km, "
        << "Fuel RM " << fuelCost << ", "
        << "Wage RM " << wageCost << ", "
        << "Toll RM " << tollCost << ", "
        << "Total RM " << totalCost << ", "
        << wasteCollected << " kg, "
        << runtimeMs << " ms";
    return oss.str();
}

bool RouteResult::isValid() const {
    return !visitOrder.empty();
}
