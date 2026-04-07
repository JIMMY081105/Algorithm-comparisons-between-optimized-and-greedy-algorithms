#include "core/RouteResult.h"
#include <sstream>
#include <iomanip>

RouteResult::RouteResult()
    : algorithmName("None"), totalDistance(0.0f), travelTime(0.0f),
      fuelCost(0.0f), wageCost(0.0f), totalCost(0.0f),
      wasteCollected(0.0f), runtimeMs(0.0) {}

std::string RouteResult::toSummaryString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << algorithmName << ": "
        << totalDistance << " km, "
        << "RM " << totalCost << " total, "
        << wasteCollected << " kg collected, "
        << runtimeMs << " ms runtime";
    return oss.str();
}

bool RouteResult::isValid() const {
    return !visitOrder.empty();
}
