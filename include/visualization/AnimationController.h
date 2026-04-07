#ifndef ANIMATION_CONTROLLER_H
#define ANIMATION_CONTROLLER_H

#include "core/Truck.h"
#include "core/MapGraph.h"
#include "core/RouteResult.h"

// Manages the playback of a route animation.
// Controls the truck's movement along a computed route, handling
// segment transitions, speed adjustments, and playback state.
// Separated from the renderer so animation logic is testable independently.
class AnimationController {
public:
    enum class PlaybackState {
        IDLE,       // no animation loaded
        PLAYING,    // truck is moving
        PAUSED,     // truck is stopped mid-route
        FINISHED    // route animation complete
    };

private:
    Truck truck;
    RouteResult currentRoute;
    PlaybackState state;
    float playbackSpeed;
    int routeDrawSegment;  // how many route segments have been revealed

    // Route waypoints in world coordinates. The renderer projects them each frame,
    // which keeps the truck aligned if the viewport layout changes.
    struct Waypoint {
        float x, y;
        int nodeId;
    };
    std::vector<Waypoint> waypoints;

    // Build waypoints from the current route and graph
    void buildWaypoints(const MapGraph& graph);

public:
    AnimationController();

    // Load a route for animation
    void loadRoute(const RouteResult& route, const MapGraph& graph);

    // Playback controls
    void play();
    void pause();
    void resume();
    void replay(const MapGraph& graph);
    void stop();

    // Update animation state each frame
    // Returns the ID of a newly collected node, or -1 if no collection this frame
    int update(float deltaTime);

    // Speed control (0.1x to 5.0x)
    void setSpeed(float speed);
    float getSpeed() const;

    // State queries
    PlaybackState getState() const;
    bool isPlaying() const;
    bool isFinished() const;
    const Truck& getTruck() const;
    const RouteResult& getCurrentRoute() const;
    int getRouteDrawSegment() const;

    // Get progress as percentage (0.0 to 1.0)
    float getProgress() const;
};

#endif // ANIMATION_CONTROLLER_H
