#include "visualization/AnimationController.h"

#include "visualization/RenderUtils.h"

#include <algorithm>

namespace {
constexpr float kBaseTravelUnitsPerSecond = 6.5f;
constexpr float kDistanceEpsilon = 0.0001f;
}

AnimationController::AnimationController()
    : state(PlaybackState::IDLE),
      playbackSpeed(1.0f),
      travelledDistance(0.0f),
      currentPolylineSegment(0),
      currentLegIndex(0),
      nextStopIndex(0) {}

void AnimationController::resetPlaybackState() {
    travelledDistance = 0.0f;
    currentPolylineSegment = 0;
    currentLegIndex = 0;
    nextStopIndex = currentMission.playbackPath.stops.size() > 1 ? 1 : 0;
    truck.setSpeed(playbackSpeed);
    truck.setMoving(false);
    truck.setCurrentSegment(0);
    truck.setSegmentProgress(0.0f);

    if (!currentMission.playbackPath.points.empty()) {
        truck.setPosition(currentMission.playbackPath.points.front().x,
                          currentMission.playbackPath.points.front().y);
    } else {
        truck.setPosition(0.0f, 0.0f);
    }
}

void AnimationController::loadRoute(const MissionPresentation& mission) {
    currentMission = mission;
    resetPlaybackState();
    state = mission.isValid() ? PlaybackState::IDLE : PlaybackState::IDLE;
}

void AnimationController::play() {
    if (!currentMission.isValid() || currentMission.playbackPath.points.size() < 2) {
        return;
    }

    resetPlaybackState();
    truck.setMoving(true);
    state = PlaybackState::PLAYING;
}

void AnimationController::pause() {
    if (state == PlaybackState::PLAYING) {
        truck.setMoving(false);
        state = PlaybackState::PAUSED;
    }
}

void AnimationController::resume() {
    if (state == PlaybackState::PAUSED) {
        truck.setMoving(true);
        state = PlaybackState::PLAYING;
    }
}

void AnimationController::replay() {
    if (!currentMission.isValid()) {
        return;
    }

    resetPlaybackState();
    play();
}

void AnimationController::stop() {
    truck.setMoving(false);
    state = PlaybackState::IDLE;
    resetPlaybackState();
}

void AnimationController::syncTruckToDistance(float distance) {
    const PlaybackPath& path = currentMission.playbackPath;
    if (path.empty()) {
        return;
    }

    const float clampedDistance = std::clamp(distance, 0.0f, path.totalLength);

    while (currentPolylineSegment + 1 < path.cumulativeDistances.size() &&
           path.cumulativeDistances[currentPolylineSegment + 1] < clampedDistance) {
        ++currentPolylineSegment;
    }

    while (currentPolylineSegment > 0 &&
           path.cumulativeDistances[currentPolylineSegment] > clampedDistance) {
        --currentPolylineSegment;
    }

    const std::size_t nextPolylineIndex = std::min(currentPolylineSegment + 1,
                                                   path.points.size() - 1);
    const float startDistance = path.cumulativeDistances[currentPolylineSegment];
    const float endDistance = path.cumulativeDistances[nextPolylineIndex];
    const float localT = (endDistance > startDistance)
        ? (clampedDistance - startDistance) / (endDistance - startDistance)
        : 0.0f;

    const float x = RenderUtils::lerp(path.points[currentPolylineSegment].x,
                                      path.points[nextPolylineIndex].x,
                                      RenderUtils::smoothstep(localT));
    const float y = RenderUtils::lerp(path.points[currentPolylineSegment].y,
                                      path.points[nextPolylineIndex].y,
                                      RenderUtils::smoothstep(localT));
    truck.setPosition(x, y);

    std::size_t visitedStopIndex = 0;
    while (visitedStopIndex + 1 < path.stops.size() &&
           path.stops[visitedStopIndex + 1].distanceAlongPath <= clampedDistance + kDistanceEpsilon) {
        ++visitedStopIndex;
    }

    currentLegIndex = visitedStopIndex;
    const int maxLegIndex = std::max(
        0, static_cast<int>(currentMission.route.visitOrder.size()) - 2);
    truck.setCurrentSegment(std::min(static_cast<int>(visitedStopIndex), maxLegIndex));

    if (path.stops.size() > 1) {
        const std::size_t fromStop = std::min(visitedStopIndex, path.stops.size() - 2);
        const std::size_t toStop = std::min(fromStop + 1, path.stops.size() - 1);
        const float legStartDistance = path.stops[fromStop].distanceAlongPath;
        const float legEndDistance = path.stops[toStop].distanceAlongPath;
        const float legT = (legEndDistance > legStartDistance)
            ? (clampedDistance - legStartDistance) / (legEndDistance - legStartDistance)
            : (clampedDistance >= legEndDistance ? 1.0f : 0.0f);
        truck.setSegmentProgress(std::clamp(legT, 0.0f, 1.0f));
    }
}

int AnimationController::update(float deltaTime) {
    if (state != PlaybackState::PLAYING || !currentMission.isValid()) {
        return -1;
    }

    const PlaybackPath& path = currentMission.playbackPath;
    if (path.empty() || path.points.size() < 2) {
        return -1;
    }

    const float newDistance = std::min(
        travelledDistance + (kBaseTravelUnitsPerSecond * playbackSpeed * deltaTime),
        path.totalLength);

    int collectedNodeId = -1;
    while (nextStopIndex < path.stops.size() &&
           newDistance + kDistanceEpsilon >= path.stops[nextStopIndex].distanceAlongPath) {
        collectedNodeId = path.stops[nextStopIndex].nodeId;
        ++nextStopIndex;
    }

    travelledDistance = newDistance;
    syncTruckToDistance(travelledDistance);

    if (travelledDistance + kDistanceEpsilon >= path.totalLength) {
        travelledDistance = path.totalLength;
        syncTruckToDistance(travelledDistance);
        truck.setMoving(false);
        state = PlaybackState::FINISHED;
    }

    return collectedNodeId;
}

void AnimationController::setSpeed(float speed) {
    playbackSpeed = speed;
    truck.setSpeed(speed);
}

float AnimationController::getSpeed() const {
    return playbackSpeed;
}

AnimationController::PlaybackState AnimationController::getState() const {
    return state;
}

bool AnimationController::isPlaying() const {
    return state == PlaybackState::PLAYING;
}

bool AnimationController::isFinished() const {
    return state == PlaybackState::FINISHED;
}

const Truck& AnimationController::getTruck() const {
    return truck;
}

const RouteResult& AnimationController::getCurrentRoute() const {
    return currentMission.route;
}

const MissionPresentation& AnimationController::getCurrentMission() const {
    return currentMission;
}

float AnimationController::getProgress() const {
    if (!currentMission.isValid() || currentMission.playbackPath.totalLength <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(
        travelledDistance / currentMission.playbackPath.totalLength, 0.0f, 1.0f);
}

float AnimationController::getTravelledDistance() const {
    return travelledDistance;
}
