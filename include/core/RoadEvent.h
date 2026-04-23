#ifndef ROAD_EVENT_H
#define ROAD_EVENT_H

#include <array>

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

struct Profile {
    RoadEvent event;
    float distanceMultiplier;
    float speedFraction;
    const char* label;
    const char* fullName;
};

inline constexpr std::array<Profile, 3> kProfiles{{
    {RoadEvent::NONE, kNormalDistanceMultiplier, kNormalSpeedFraction, "", "None"},
    {RoadEvent::FLOOD, kImpassablePenalty, kBlockedSpeedFraction, "FLOOD", "Flood"},
    {RoadEvent::FESTIVAL, kImpassablePenalty, kBlockedSpeedFraction, "FEST", "Festival"},
}};

inline constexpr const Profile& profile(RoadEvent event) {
    for (const Profile& candidate : kProfiles) {
        if (candidate.event == event) {
            return candidate;
        }
    }
    return kProfiles[0];
}

inline bool isBlocking(RoadEvent event) {
    return profile(event).speedFraction == kBlockedSpeedFraction;
}

// Both FLOOD and FESTIVAL are fully impassable: algorithms route around them.
inline float distanceMultiplier(RoadEvent event) {
    return profile(event).distanceMultiplier;
}

// Speed fraction for cost reporting. Blocked segments should not appear in routes.
inline float speedFraction(RoadEvent event) {
    return profile(event).speedFraction;
}

// Current road events block routing, so all reachable segments use this default.
inline float fuelMultiplier() {
    return kDefaultFuelMultiplier;
}

inline const char* label(RoadEvent event) {
    return profile(event).label;
}

inline const char* fullName(RoadEvent event) {
    return profile(event).fullName;
}

} // namespace RoadEventRules

#endif // ROAD_EVENT_H
