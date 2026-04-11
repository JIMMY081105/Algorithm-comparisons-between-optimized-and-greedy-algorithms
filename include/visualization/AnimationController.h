#ifndef ANIMATION_CONTROLLER_H
#define ANIMATION_CONTROLLER_H

#include "core/Truck.h"
#include "environment/MissionPresentation.h"

// Manages playback along a prepared mission path. The environment builds the
// polyline and stop metadata; the animation controller only advances along it.
class AnimationController {
public:
    enum class PlaybackState {
        IDLE,
        PLAYING,
        PAUSED,
        FINISHED
    };

private:
    Truck truck;
    MissionPresentation currentMission;
    PlaybackState state;
    float playbackSpeed;
    float travelledDistance;
    std::size_t currentPolylineSegment;
    std::size_t currentLegIndex;
    std::size_t nextStopIndex;

    void syncTruckToDistance(float distance);
    void resetPlaybackState();

public:
    AnimationController();

    void loadRoute(const MissionPresentation& mission);

    void play();
    void pause();
    void resume();
    void replay();
    void stop();

    int update(float deltaTime);

    void setSpeed(float speed);
    float getSpeed() const;

    PlaybackState getState() const;
    bool isPlaying() const;
    bool isFinished() const;
    const Truck& getTruck() const;
    const RouteResult& getCurrentRoute() const;
    const MissionPresentation& getCurrentMission() const;

    float getProgress() const;
    float getTravelledDistance() const;
};

#endif // ANIMATION_CONTROLLER_H
