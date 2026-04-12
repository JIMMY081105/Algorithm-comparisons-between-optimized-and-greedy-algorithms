#include "core/WasteNode.h"
#include <algorithm>

namespace {
constexpr float kMinimumWasteLevel = 0.0f;
constexpr float kMaximumWasteLevel = 100.0f;
constexpr float kHighUrgencyThreshold = 70.0f;
constexpr float kMediumUrgencyThreshold = 40.0f;
constexpr float kPercentToFraction = 0.01f;
}

WasteNode::WasteNode()
    : id(0), name("Unknown"), worldX(0.0f), worldY(0.0f),
      wasteLevel(0.0f), wasteCapacity(100.0f),
      collected(false), isHQ(false) {}

WasteNode::WasteNode(int id, const std::string& name, float x, float y,
                     float capacity, bool isHQ)
    : id(id), name(name), worldX(x), worldY(y),
      wasteLevel(0.0f), wasteCapacity(capacity),
      collected(false), isHQ(isHQ) {}

int WasteNode::getId() const { return id; }
const std::string& WasteNode::getName() const { return name; }
float WasteNode::getWorldX() const { return worldX; }
float WasteNode::getWorldY() const { return worldY; }
float WasteNode::getWasteLevel() const { return wasteLevel; }
float WasteNode::getWasteCapacity() const { return wasteCapacity; }
bool WasteNode::isCollected() const { return collected; }
bool WasteNode::getIsHQ() const { return isHQ; }

void WasteNode::setWasteLevel(float level) {
    wasteLevel = std::clamp(level, kMinimumWasteLevel, kMaximumWasteLevel);
}

void WasteNode::setCollected(bool status) {
    collected = status;
}

UrgencyLevel WasteNode::getUrgency() const {
    if (isHQ) return UrgencyLevel::LOW;
    if (wasteLevel >= kHighUrgencyThreshold) return UrgencyLevel::HIGH;
    if (wasteLevel >= kMediumUrgencyThreshold) return UrgencyLevel::MEDIUM;
    return UrgencyLevel::LOW;
}

float WasteNode::getWasteAmount() const {
    return wasteCapacity * (wasteLevel * kPercentToFraction);
}

bool WasteNode::isEligible(float thresholdPercent) const {
    // HQ is never "eligible" for collection — it's the depot
    if (isHQ) return false;
    return wasteLevel >= thresholdPercent;
}

void WasteNode::resetForNewDay() {
    collected = false;
    // wasteLevel will be reassigned by the simulation manager
}
