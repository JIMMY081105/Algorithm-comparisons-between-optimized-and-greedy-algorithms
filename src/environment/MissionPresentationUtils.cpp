#include "environment/MissionPresentationUtils.h"

#include <algorithm>
#include <cmath>

namespace MissionPresentationUtils {

float distanceBetween(const PlaybackPoint& a, const PlaybackPoint& b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

PlaybackPath buildPlaybackPath(const std::vector<PlaybackPoint>& points,
                               const std::vector<int>& stopNodeIds,
                               const std::vector<std::size_t>& stopPointIndices) {
    PlaybackPath path;
    if (points.empty()) {
        return path;
    }

    path.points = points;
    path.cumulativeDistances.resize(points.size(), 0.0f);

    for (std::size_t i = 1; i < points.size(); ++i) {
        path.cumulativeDistances[i] =
            path.cumulativeDistances[i - 1] + distanceBetween(points[i - 1], points[i]);
    }

    path.totalLength = path.cumulativeDistances.back();

    const std::size_t stopCount = std::min(stopNodeIds.size(), stopPointIndices.size());
    path.stops.reserve(stopCount);
    for (std::size_t i = 0; i < stopCount; ++i) {
        const std::size_t clampedPointIndex =
            std::min(stopPointIndices[i], points.size() - 1);
        path.stops.push_back(
            PlaybackStop{stopNodeIds[i], clampedPointIndex,
                         path.cumulativeDistances[clampedPointIndex]});
    }

    return path;
}

} // namespace MissionPresentationUtils
