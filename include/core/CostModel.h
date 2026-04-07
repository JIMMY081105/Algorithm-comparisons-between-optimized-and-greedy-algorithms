#ifndef COST_MODEL_H
#define COST_MODEL_H

#include <vector>

// Centralizes all cost and operational parameters for the simulation.
// Instead of scattering magic numbers throughout the codebase, every
// cost calculation references this single model. This makes it easy
// to adjust assumptions (e.g., fuel price changes) in one place.
class CostModel {
private:
    float fuelCostPerKm;    // RM per kilometer
    float driverWagePerHour; // RM per hour
    float truckSpeedKmh;    // average speed in km/h
    float minutesPerKm;     // derived from truck speed

public:
    CostModel();

    // Allow overriding defaults if needed
    void setFuelCostPerKm(float cost);
    void setDriverWagePerHour(float wage);
    void setTruckSpeedKmh(float speed);

    float getFuelCostPerKm() const;
    float getDriverWagePerHour() const;
    float getTruckSpeedKmh() const;

    // Core calculations — these take a total route distance and produce metrics
    float calculateFuelCost(float distanceKm) const;
    float calculateTravelTime(float distanceKm) const;  // returns hours
    float calculateWageCost(float distanceKm) const;
    float calculateTotalCost(float distanceKm) const;
};

#endif // COST_MODEL_H
