#ifndef SEA_THEME_RENDERER_H
#define SEA_THEME_RENDERER_H

#include "visualization/IThemeRenderer.h"
#include "visualization/OceanShader.h"
#include "visualization/RenderUtils.h"
#include <unordered_map>

// Sea / Boom Beach themed renderer.
// Draws ocean water, tropical islands for waste nodes, a harbor HQ,
// a cargo boat for the truck, decorative islets, and seabirds.
class SeaThemeRenderer : public IThemeRenderer {
public:
    SeaThemeRenderer();
    ~SeaThemeRenderer() override = default;

    EnvironmentTheme getTheme() const override;
    bool init() override;
    void rebuildScene(const MapGraph& graph, unsigned int seed) override;
    void update(float deltaTime) override;
    void applyRouteWeights(MapGraph& graph) const override;
    MissionPresentation buildMissionPresentation(const RouteResult& route,
                                                 const MapGraph& graph) const override;
    ThemeDashboardInfo getDashboardInfo() const override;
    void setLayerToggles(const SceneLayerToggles& toggles) override;
    bool supportsWeather() const override;
    CityWeather getWeather() const override;
    void setWeather(CityWeather weather) override;
    void randomizeWeather(unsigned int seed) override;

    void drawGroundPlane(IsometricRenderer& renderer,
                         const MapGraph& graph,
                         const Truck& truck,
                         const MissionPresentation* mission,
                         float animationTime) override;

    void drawTransitNetwork(IsometricRenderer& renderer,
                            const MapGraph& graph,
                            const MissionPresentation* mission,
                            AnimationController::PlaybackState playbackState,
                            float routeRevealProgress,
                            float animationTime) override;

    void drawWasteNode(IsometricRenderer& renderer,
                       const WasteNode& node,
                       float animationTime) override;

    void drawHQNode(IsometricRenderer& renderer,
                    const WasteNode& node,
                    float animationTime) override;

    void drawTruck(IsometricRenderer& renderer,
                   const MapGraph& graph,
                   const Truck& truck,
                   const MissionPresentation* mission,
                   float animationTime) override;

    void drawDecorativeElements(IsometricRenderer& renderer,
                                const MapGraph& graph,
                                float animationTime) override;

    void drawAtmosphericEffects(IsometricRenderer& renderer,
                                const MapGraph& graph,
                                float animationTime) override;

    void cleanup() override;

private:
    OceanShader oceanShader;
    ThemeDashboardInfo dashboardInfo;
    SceneLayerToggles layerToggles;
    unsigned int sceneSeed;

    // Per-node garbage collection sink animation state
    std::unordered_map<int, float> garbageSinkProgress;
    float currentGarbageSinkProgress;

    // Boat physics state
    float boatCargoFillRatio;
    float boatWakeStrength;
    float boatPitchWave;
    float boatRollWave;
    bool boatIsMoving;

    // Cached scene bounds
    struct SceneBounds {
        float minX;
        float maxX;
        float minY;
        float maxY;
    };
    SceneBounds sceneBounds;
    const MapGraph* activeGraph;

    // Internal helpers
    void updateSceneBounds(const MapGraph& graph);
    void updateGarbageSinkState(const MapGraph& graph, float deltaTime);
    float getGarbageSinkState(int nodeId);

    void drawWaterTile(IsometricRenderer& renderer,
                       float cx, float cy, float w, float h,
                       int gx, int gy, float animationTime);

    void drawBoatSprite(IsometricRenderer& renderer,
                        float cx, float cy, float scale,
                        const Color& bodyColor,
                        float headingX, float headingY,
                        float bobPhase, float animationTime);

    void drawGarbagePatchSprite(IsometricRenderer& renderer,
                                float cx, float cy, float scale,
                                const Color& bodyColor,
                                const Color& accentColor,
                                bool collected, float fillRatio,
                                float animationTime);

    void drawDecorativeIslets(IsometricRenderer& renderer,
                              const MapGraph& graph,
                              float animationTime);

    PlaybackPath buildDirectPlaybackPath(const RouteResult& route,
                                         const MapGraph& graph) const;
};

#endif // SEA_THEME_RENDERER_H
