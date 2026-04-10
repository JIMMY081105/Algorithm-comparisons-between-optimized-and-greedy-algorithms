#include "core/CostModel.h"

// The defaults are intentionally simple and easy to justify in a coursework
// report: one fuel rate, one wage rate, and one average travel speed.
CostModel::CostModel()
    : fuelCostPerKm(1.20f),
      driverWagePerHour(15.00f),
      truckSpeedKmh(30.0f) {}

void CostModel::setFuelCostPerKm(float cost) { fuelCostPerKm = cost; }
void CostModel::setDriverWagePerHour(float wage) { driverWagePerHour = wage; }
void CostModel::setTruckSpeedKmh(float speed) { truckSpeedKmh = speed; }

float CostModel::getFuelCostPerKm() const { return fuelCostPerKm; }
float CostModel::getDriverWagePerHour() const { return driverWagePerHour; }
float CostModel::getTruckSpeedKmh() const { return truckSpeedKmh; }

float CostModel::calculateFuelCost(float distanceKm) const {
    return distanceKm * fuelCostPerKm;
}

float CostModel::calculateTravelTime(float distanceKm) const {
    if (truckSpeedKmh <= 0.0f) {
        return 0.0f;
    }
    return distanceKm / truckSpeedKmh;
}

float CostModel::calculateWageCost(float distanceKm) const {
    return calculateTravelTime(distanceKm) * driverWagePerHour;
}

float CostModel::calculateTotalCost(float distanceKm) const {
    return calculateFuelCost(distanceKm) + calculateWageCost(distanceKm);
}
