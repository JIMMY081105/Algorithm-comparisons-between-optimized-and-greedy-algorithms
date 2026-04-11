#ifndef MISSION_PRESENTATION_UTILS_H
#define MISSION_PRESENTATION_UTILS_H

#include "environment/MissionPresentation.h"

#include <cstddef>
#include <vector>

namespace MissionPresentationUtils {

float distanceBetween(const PlaybackPoint& a, const PlaybackPoint& b);

PlaybackPath buildPlaybackPath(const std::vector<PlaybackPoint>& points,
                               const std::vector<int>& stopNodeIds,
                               const std::vector<std::size_t>& stopPointIndices);

} // namespace MissionPresentationUtils

#endif // MISSION_PRESENTATION_UTILS_H
