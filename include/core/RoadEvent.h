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

// Both FLOOD and FESTIVAL are fully impassable — algorithms route around them.
inline float roadEventDistanceMultiplier(RoadEvent event) {
    switch (event) {
        case RoadEvent::FLOOD:    return 9999.0f;
        case RoadEvent::FESTIVAL: return 9999.0f;
        default:                  return 1.0f;
    }
}

// Speed fraction for cost reporting (impassable segments won't appear in routes).
inline float roadEventSpeedFraction(RoadEvent event) {
    switch (event) {
        case RoadEvent::FLOOD:    return 0.0f;
        case RoadEvent::FESTIVAL: return 0.0f;
        default:                  return 1.0f;
    }
}

// Fuel multiplier (won't be reached in practice since both events block routing).
inline float roadEventFuelMultiplier(RoadEvent event) {
    (void)event;
    return 1.0f;
}

inline const char* roadEventLabel(RoadEvent event) {
    switch (event) {
        case RoadEvent::FLOOD:    return "FLOOD";
        case RoadEvent::FESTIVAL: return "FEST";
        default:                  return "";
    }
}

inline const char* roadEventFullName(RoadEvent event) {
    switch (event) {
        case RoadEvent::FLOOD:    return "Flood";
        case RoadEvent::FESTIVAL: return "Festival";
        default:                  return "None";
    }
}

#endif // ROAD_EVENT_H
