#include "core/CostModel.h"

namespace {
constexpr CostModel::EfficiencyBonusTiers kEfficiencyBonusTiers{{
    {80.0f, 25.0f},
    {120.0f, 15.0f},
    {180.0f, 8.0f},
    {250.0f, 3.0f},
}};
} // namespace

CostModel::CostModel()
    : litresPerKm(0.40f),
      dailyFuelPricePerLitre(2.50f),
      baseWagePerShift(40.0f),
      wagePerKmBonus(0.50f),
      truckSpeedKmh(60.0f) {}

const CostModel::EfficiencyBonusTiers& CostModel::getEfficiencyBonusTiers() {
    return kEfficiencyBonusTiers;
}

void CostModel::setLitresPerKm(float rate)               { litresPerKm = rate; }
void CostModel::setDailyFuelPricePerLitre(float p)       { dailyFuelPricePerLitre = p; }
void CostModel::setBaseWagePerShift(float base)          { baseWagePerShift = base; }
void CostModel::setWagePerKmBonus(float bonusPerKm)      { wagePerKmBonus = bonusPerKm; }
void CostModel::setTruckSpeedKmh(float speed)            { truckSpeedKmh = speed; }

float CostModel::getLitresPerKm() const                  { return litresPerKm; }
float CostModel::getDailyFuelPricePerLitre() const       { return dailyFuelPricePerLitre; }
float CostModel::getFuelCostPerKm() const                { return litresPerKm * dailyFuelPricePerLitre; }
float CostModel::getBaseWagePerShift() const             { return baseWagePerShift; }
float CostModel::getWagePerKmBonus() const               { return wagePerKmBonus; }
float CostModel::getTruckSpeedKmh() const                { return truckSpeedKmh; }

float CostModel::calculateFuelCost(float distanceKm) const {
    return litresPerKm * dailyFuelPricePerLitre * distanceKm;
}

float CostModel::calculateTravelTime(float distanceKm) const {
    if (truckSpeedKmh <= 0.0f) return 0.0f;
    return distanceKm / truckSpeedKmh;
}

float CostModel::calculateBasePay() const {
    return baseWagePerShift;
}

float CostModel::calculatePerKmBonus(float distanceKm) const {
    return wagePerKmBonus * distanceKm;
}

// Tiered efficiency bonus — shorter routes earn more.
// This incentivises algorithms that minimise total travel distance.
float CostModel::efficiencyBonusForDistance(float distanceKm) const {
    for (const EfficiencyBonusTier& tier : getEfficiencyBonusTiers()) {
        if (distanceKm < tier.distanceLimitKm) {
            return tier.bonusRm;
        }
    }
    return 0.0f;
}

float CostModel::calculateEfficiencyBonus(float distanceKm) const {
    return efficiencyBonusForDistance(distanceKm);
}

float CostModel::calculateWageCost(float distanceKm) const {
    return calculateBasePay()
         + calculatePerKmBonus(distanceKm)
         + calculateEfficiencyBonus(distanceKm);
}

float CostModel::calculateTotalCost(float distanceKm) const {
    return calculateFuelCost(distanceKm) + calculateWageCost(distanceKm);
}
