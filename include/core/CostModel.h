#ifndef COST_MODEL_H
#define COST_MODEL_H

// Encapsulates the operating assumptions used to turn route distance into
// travel time and cost.
//
// Wage model:
//   Total wage = basePay (flat shift pay) + perKmBonus (distance incentive)
//                + efficiencyBonus (reward for short routes)
//
// Fuel model:
//   Fuel cost = litresPerKm * dailyFuelPricePerLitre * distanceKm
//   The daily fuel price is randomized each simulation day (RM 2.00–3.80/L).
class CostModel {
private:
    float litresPerKm;                // fuel consumption rate (L/km)
    float dailyFuelPricePerLitre;     // RM/litre — set fresh each day
    float baseWagePerShift;           // flat RM pay regardless of distance
    float wagePerKmBonus;             // extra RM per km driven
    float truckSpeedKmh;

    // Efficiency bonus tiers (RM) based on total route distance
    float efficiencyBonusForDistance(float distanceKm) const;

public:
    CostModel();

    // --- Setters ---
    void setLitresPerKm(float rate);
    void setDailyFuelPricePerLitre(float pricePerLitre);
    void setBaseWagePerShift(float base);
    void setWagePerKmBonus(float bonusPerKm);
    void setTruckSpeedKmh(float speed);

    // --- Getters ---
    float getLitresPerKm() const;
    float getDailyFuelPricePerLitre() const;
    float getFuelCostPerKm() const;     // derived: litresPerKm * dailyPrice
    float getBaseWagePerShift() const;
    float getWagePerKmBonus() const;
    float getTruckSpeedKmh() const;

    // --- Calculations ---
    float calculateFuelCost(float distanceKm) const;
    float calculateTravelTime(float distanceKm) const;

    // Returns base + per-km + efficiency bonus breakdown
    float calculateBasePay() const;
    float calculatePerKmBonus(float distanceKm) const;
    float calculateEfficiencyBonus(float distanceKm) const;
    float calculateWageCost(float distanceKm) const;       // sum of all wage parts

    // Total operational cost (fuel + wage). Toll cost is added by WasteSystem.
    float calculateTotalCost(float distanceKm) const;
};

#endif // COST_MODEL_H
