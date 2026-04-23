#ifndef APPLICATION_INTERNAL_H
#define APPLICATION_INTERNAL_H

#include "core/WasteSystem.h"

#include <GLFW/glfw3.h>

namespace ApplicationInternal {

constexpr float kMaxFrameDeltaTime = 0.1f;
constexpr unsigned int kWeatherSeedDayMultiplier = 97u;
constexpr unsigned int kTrafficSeedDayMultiplier = 131u;
constexpr double kMillisecondsPerSecond = 1000.0;

// The scene renderer uses a fixed-step delta so particle/animation rates stay
// consistent regardless of the real frame rate. 60 Hz is the vsync target.
constexpr float kSceneRenderDeltaTime = 1.0f / 60.0f;

// Threshold below which the transition flash quad is skipped entirely.
constexpr float kTransitionAlphaThreshold = 0.001f;

// Flash quad alpha multipliers: bottom edge dims slightly vs top edge to give
// a subtle gradient wipe during theme transitions.
constexpr float kFlashAlphaBottom = 0.08f;
constexpr float kFlashAlphaTop = 0.14f;

inline float clampFrameDelta(float deltaTime) {
    return (deltaTime > kMaxFrameDeltaTime) ? kMaxFrameDeltaTime : deltaTime;
}

inline unsigned int buildWeatherSeed(const WasteSystem& system) {
    return system.getCurrentSeed() +
           static_cast<unsigned int>(system.getDayNumber()) *
               kWeatherSeedDayMultiplier +
           static_cast<unsigned int>(glfwGetTime() * kMillisecondsPerSecond);
}

inline unsigned int buildTrafficSeed(const WasteSystem& system) {
    return system.getCurrentSeed() ^
           (static_cast<unsigned int>(system.getDayNumber()) *
            kTrafficSeedDayMultiplier);
}

} // namespace ApplicationInternal

#endif // APPLICATION_INTERNAL_H
