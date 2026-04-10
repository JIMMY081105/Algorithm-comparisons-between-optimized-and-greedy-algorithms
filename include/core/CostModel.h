#ifndef COST_MODEL_H
#define COST_MODEL_H

// Encapsulates the operating assumptions used to turn route distance into
// travel time and cost. Keeping these values in one place makes it easier to
// explain the simulation model and adjust it without touching route logic.
class CostModel {
private:
    float fuelCostPerKm;
    float driverWagePerHour;
    float truckSpeedKmh;

public:
    CostModel();

    void setFuelCostPerKm(float cost);
    void setDriverWagePerHour(float wage);
    void setTruckSpeedKmh(float speed);

    float getFuelCostPerKm() const;
    float getDriverWagePerHour() const;
    float getTruckSpeedKmh() const;

    float calculateFuelCost(float distanceKm) const;
    float calculateTravelTime(float distanceKm) const;
    float calculateWageCost(float distanceKm) const;
    float calculateTotalCost(float distanceKm) const;
};

#endif // COST_MODEL_H
