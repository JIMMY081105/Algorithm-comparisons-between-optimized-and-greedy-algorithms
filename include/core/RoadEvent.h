#ifndef ROAD_EVENT_H
#define ROAD_EVENT_H

enum class RoadEvent {
    NONE,
    FLOOD,
    FESTIVAL
};

struct ActiveEdgeEvent {
    int fromId;
    int toId;
    RoadEvent type;
};

namespace RoadEventRules {

inline constexpr float kNormalDistanceMultiplier = 1.0f;
inline constexpr float kNormalSpeedFraction = 1.0f;
inline constexpr float kBlockedSpeedFraction = 0.0f;
inline constexpr float kDefaultFuelMultiplier = 1.0f;
// Large but finite so blocked edges are avoided without infinities/overflow in
// route summaries, debug output, or cost calculations.
inline constexpr float kImpassablePenalty = 9999.0f;

inline bool isBlocking(RoadEvent event) {
    return event == RoadEvent::FLOOD || event == RoadEvent::FESTIVAL;
}

// Both FLOOD and FESTIVAL are fully impassable: algorithms route around them.
inline float distanceMultiplier(RoadEvent event) {
    switch (event) {
        case RoadEvent::FLOOD:
        case RoadEvent::FESTIVAL:
            return kImpassablePenalty;
        case RoadEvent::NONE:
        default:
            return kNormalDistanceMultiplier;
    }
}

// Speed fraction for cost reporting. Blocked segments should not appear in routes.
inline float speedFraction(RoadEvent event) {
    switch (event) {
        case RoadEvent::FLOOD:
        case RoadEvent::FESTIVAL:
            return kBlockedSpeedFraction;
        case RoadEvent::NONE:
        default:
            return kNormalSpeedFraction;
    }
}

// Current road events block routing, so all reachable segments use this default.
inline float fuelMultiplier() {
    return kDefaultFuelMultiplier;
}

inline const char* label(RoadEvent event) {
    switch (event) {
        case RoadEvent::FLOOD:    return "FLOOD";
        case RoadEvent::FESTIVAL: return "FEST";
        default:                  return "";
    }
}

inline const char* fullName(RoadEvent event) {
    switch (event) {
        case RoadEvent::FLOOD:    return "Flood";
        case RoadEvent::FESTIVAL: return "Festival";
        default:                  return "None";
    }
}

} // namespace RoadEventRules

#endif // ROAD_EVENT_H
