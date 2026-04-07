#include "visualization/AnimationController.h"
#include "visualization/RenderUtils.h"

AnimationController::AnimationController()
    : state(PlaybackState::IDLE), playbackSpeed(1.0f), routeDrawSegment(0) {}

void AnimationController::loadRoute(const RouteResult& route, const MapGraph& graph) {
    currentRoute = route;
    buildWaypoints(graph);

    // Position the truck at the first waypoint (HQ)
    if (!waypoints.empty()) {
        truck.setPosition(waypoints[0].x, waypoints[0].y);
    }

    truck.setCurrentSegment(0);
    truck.setSegmentProgress(0.0f);
    truck.setSpeed(playbackSpeed);
    truck.setMoving(false);
    routeDrawSegment = 0;
    state = PlaybackState::IDLE;
}

void AnimationController::buildWaypoints(const MapGraph& graph) {
    waypoints.clear();

    // Keep route waypoints in world space so the renderer can recenter
    // and zoom the map without desynchronizing the truck animation.
    for (int nodeId : currentRoute.visitOrder) {
        int idx = graph.findNodeIndex(nodeId);
        if (idx < 0) continue;

        const WasteNode& node = graph.getNode(idx);

        Waypoint wp;
        wp.x = node.getWorldX();
        wp.y = node.getWorldY();
        wp.nodeId = nodeId;
        waypoints.push_back(wp);
    }
}

void AnimationController::play() {
    if (waypoints.size() < 2) return;

    truck.setCurrentSegment(0);
    truck.setSegmentProgress(0.0f);
    truck.setMoving(true);
    truck.setPosition(waypoints[0].x, waypoints[0].y);
    routeDrawSegment = 1;  // show the first route segment
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

void AnimationController::replay(const MapGraph& graph) {
    loadRoute(currentRoute, graph);
    play();
}

void AnimationController::stop() {
    truck.setMoving(false);
    state = PlaybackState::IDLE;
    routeDrawSegment = 0;
}

int AnimationController::update(float deltaTime) {
    if (state != PlaybackState::PLAYING) return -1;
    if (waypoints.size() < 2) return -1;

    int seg = truck.getCurrentSegment();
    if (seg >= static_cast<int>(waypoints.size()) - 1) {
        // Animation complete — truck has reached the final waypoint
        truck.setMoving(false);
        state = PlaybackState::FINISHED;
        routeDrawSegment = static_cast<int>(waypoints.size());
        return -1;
    }

    // Interpolate truck position between current and next waypoint
    bool segmentDone = truck.advanceProgress(deltaTime);

    float t = RenderUtils::smoothstep(truck.getSegmentProgress());
    float newX = RenderUtils::lerp(waypoints[seg].x, waypoints[seg + 1].x, t);
    float newY = RenderUtils::lerp(waypoints[seg].y, waypoints[seg + 1].y, t);
    truck.setPosition(newX, newY);

    if (segmentDone) {
        // Move to the next segment
        int collectedNodeId = waypoints[seg + 1].nodeId;
        truck.setCurrentSegment(seg + 1);
        truck.setSegmentProgress(0.0f);
        routeDrawSegment = seg + 2;  // reveal the next route line

        // Snap truck to the waypoint exactly
        truck.setPosition(waypoints[seg + 1].x, waypoints[seg + 1].y);

        return collectedNodeId;  // signal that this node was just reached
    }

    return -1;
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
    return currentRoute;
}

int AnimationController::getRouteDrawSegment() const {
    return routeDrawSegment;
}

float AnimationController::getProgress() const {
    if (waypoints.size() < 2) return 0.0f;
    int seg = truck.getCurrentSegment();
    float segFrac = truck.getSegmentProgress();
    float totalSegs = static_cast<float>(waypoints.size() - 1);
    return (seg + segFrac) / totalSegs;
}
