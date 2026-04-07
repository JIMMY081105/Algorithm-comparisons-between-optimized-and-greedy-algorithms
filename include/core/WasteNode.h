#ifndef WASTE_NODE_H
#define WASTE_NODE_H

#include <string>

// Urgency categories based on how full a bin is.
// These thresholds drive both visual color coding and route eligibility.
enum class UrgencyLevel {
    LOW,      // 0-39%  — not urgent, green on the map
    MEDIUM,   // 40-69% — moderate fill, yellow warning
    HIGH      // 70-100% — needs immediate collection, red alert
};

// Represents a single waste collection point in the simulation.
// Each node has a fixed position on the map but its waste level
// changes randomly each time a "new day" is generated.
class WasteNode {
private:
    int id;
    std::string name;
    float worldX;           // fixed x position on the logical grid
    float worldY;           // fixed y position on the logical grid
    float wasteLevel;       // current fill percentage (0.0 to 100.0)
    float wasteCapacity;    // maximum capacity in kg
    bool collected;         // whether truck has visited this node today
    bool isHQ;              // true only for the depot/headquarters node

public:
    WasteNode();
    WasteNode(int id, const std::string& name, float x, float y,
              float capacity, bool isHQ = false);

    // Getters
    int getId() const;
    std::string getName() const;
    float getWorldX() const;
    float getWorldY() const;
    float getWasteLevel() const;
    float getWasteCapacity() const;
    bool isCollected() const;
    bool getIsHQ() const;

    // Setters
    void setWasteLevel(float level);
    void setCollected(bool status);

    // Derive urgency from current waste level
    UrgencyLevel getUrgency() const;

    // How much actual waste (in kg) needs collecting
    float getWasteAmount() const;

    // Check if this node qualifies for collection given a threshold percentage
    bool isEligible(float thresholdPercent) const;

    // Reset for a new simulation day
    void resetForNewDay();
};

#endif // WASTE_NODE_H
