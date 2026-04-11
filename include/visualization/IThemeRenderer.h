#ifndef ITHEME_RENDERER_H
#define ITHEME_RENDERER_H

#include "core/MapGraph.h"
#include "core/RouteResult.h"
#include "core/Truck.h"
#include "environment/EnvironmentTypes.h"
#include "environment/MissionPresentation.h"
#include "visualization/AnimationController.h"

class IsometricRenderer;

// Theme renderers own environment-specific scene generation, weighted travel
// costs, route expansion, and drawing. The shared renderer only supplies low-
// level primitives and timing.
class IThemeRenderer {
public:
    virtual ~IThemeRenderer() = default;

    virtual EnvironmentTheme getTheme() const = 0;
    virtual bool init() = 0;
    virtual void rebuildScene(const MapGraph& graph, unsigned int seed) = 0;
    virtual void update(float deltaTime) = 0;
    virtual void applyRouteWeights(MapGraph& graph) const = 0;

    virtual MissionPresentation buildMissionPresentation(
        const RouteResult& route,
        const MapGraph& graph) const = 0;

    virtual ThemeDashboardInfo getDashboardInfo() const = 0;
    virtual void setLayerToggles(const SceneLayerToggles& toggles) = 0;

    virtual bool supportsWeather() const = 0;
    virtual CityWeather getWeather() const = 0;
    virtual void setWeather(CityWeather weather) = 0;
    virtual void randomizeWeather(unsigned int seed) = 0;

    virtual void drawGroundPlane(IsometricRenderer& renderer,
                                 const MapGraph& graph,
                                 const Truck& truck,
                                 const MissionPresentation* mission,
                                 float animationTime) = 0;

    virtual void drawTransitNetwork(IsometricRenderer& renderer,
                                    const MapGraph& graph,
                                    const MissionPresentation* mission,
                                    AnimationController::PlaybackState playbackState,
                                    float routeRevealProgress,
                                    float animationTime) = 0;

    virtual void drawWasteNode(IsometricRenderer& renderer,
                               const WasteNode& node,
                               float animationTime) = 0;

    virtual void drawHQNode(IsometricRenderer& renderer,
                            const WasteNode& node,
                            float animationTime) = 0;

    virtual void drawTruck(IsometricRenderer& renderer,
                           const MapGraph& graph,
                           const Truck& truck,
                           const MissionPresentation* mission,
                           float animationTime) = 0;

    virtual void drawDecorativeElements(IsometricRenderer& renderer,
                                        const MapGraph& graph,
                                        float animationTime) = 0;

    virtual void drawAtmosphericEffects(IsometricRenderer& renderer,
                                        const MapGraph& graph,
                                        float animationTime) = 0;

    virtual void cleanup() = 0;
};

#endif // ITHEME_RENDERER_H
