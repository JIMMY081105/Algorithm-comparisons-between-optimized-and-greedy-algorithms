#ifndef MISSION_PRESENTATION_H
#define MISSION_PRESENTATION_H

#include "core/RouteResult.h"

#include <cstddef>
#include <string>
#include <vector>

struct PlaybackPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct PlaybackStop {
    int nodeId = -1;
    std::size_t pointIndex = 0;
    float distanceAlongPath = 0.0f;
};

struct PlaybackPath {
    std::vector<PlaybackPoint> points;
    std::vector<float> cumulativeDistances;
    std::vector<PlaybackStop> stops;
    float totalLength = 0.0f;

    bool empty() const {
        return points.empty() || cumulativeDistances.size() != points.size();
    }
};

struct RouteInsight {
    int fromNodeId = -1;
    int toNodeId = -1;
    float baseDistance = 0.0f;
    float congestionPenalty = 0.0f;
    float incidentPenalty = 0.0f;
    float signalPenalty = 0.0f;
    float weatherPenalty = 0.0f;
    float totalCost = 0.0f;
};

struct MissionPresentation {
    RouteResult route;
    PlaybackPath playbackPath;
    std::vector<RouteInsight> legInsights;
    std::string narrative;

    bool isValid() const {
        return route.isValid() && !playbackPath.empty() &&
               playbackPath.points.size() > 1;
    }
};

#endif // MISSION_PRESENTATION_H
