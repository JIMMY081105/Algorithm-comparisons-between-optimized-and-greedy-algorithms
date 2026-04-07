#include "core/CostModel.h"

// Default values represent typical Malaysian operational costs
// for a small-to-medium waste collection truck.
CostModel::CostModel()
    : fuelCostPerKm(1.20f),        // RM 1.20 per km (diesel truck)
      driverWagePerHour(15.00f),   // RM 15 per hour (local driver wage)
      truckSpeedKmh(30.0f),       // 30 km/h average in urban areas
      minutesPerKm(2.0f) {}       // derived: 60 / 30 = 2 min per km

void CostModel::setFuelCostPerKm(float cost) { fuelCostPerKm = cost; }
void CostModel::setDriverWagePerHour(float wage) { driverWagePerHour = wage; }
void CostModel::setTruckSpeedKmh(float speed) {
    truckSpeedKmh = speed;
    // Keep the minutes-per-km in sync whenever speed changes
    if (speed > 0) minutesPerKm = 60.0f / speed;
}

float CostModel::getFuelCostPerKm() const { return fuelCostPerKm; }
float CostModel::getDriverWagePerHour() const { return driverWagePerHour; }
float CostModel::getTruckSpeedKmh() const { return truckSpeedKmh; }

float CostModel::calculateFuelCost(float distanceKm) const {
    return distanceKm * fuelCostPerKm;
}

float CostModel::calculateTravelTime(float distanceKm) const {
    // Returns travel time in hours
    if (truckSpeedKmh <= 0) return 0.0f;
    return distanceKm / truckSpeedKmh;
}

float CostModel::calculateWageCost(float distanceKm) const {
    float hours = calculateTravelTime(distanceKm);
    return hours * driverWagePerHour;
}

float CostModel::calculateTotalCost(float distanceKm) const {
    return calculateFuelCost(distanceKm) + calculateWageCost(distanceKm);
}
