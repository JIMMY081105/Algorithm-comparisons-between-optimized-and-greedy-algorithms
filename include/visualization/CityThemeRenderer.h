#ifndef CITY_THEME_RENDERER_H
#define CITY_THEME_RENDERER_H

#include "visualization/IThemeRenderer.h"

#include <random>
#include <vector>

class CityThemeRenderer : public IThemeRenderer {
public:
    CityThemeRenderer();
    ~CityThemeRenderer() override = default;

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
    struct Intersection {
        int id = -1;
        float x = 0.0f;
        float y = 0.0f;
        bool hasTrafficLight = false;
        float cycleOffset = 0.0f;
    };

    struct RoadSegment {
        int from = -1;
        int to = -1;
        float baseLength = 0.0f;
        float congestion = 0.0f;
        bool incident = false;
        float signalDelay = 0.0f;
    };

    struct BuildingLot {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float depth = 0.0f;
        float height = 0.0f;
        float hueShift = 0.0f;
        bool tower = false;
    };

    struct AmbientCar {
        int roadIndex = -1;
        float offset = 0.0f;
        float speed = 0.0f;
        bool crashed = false;
    };

    struct PairRouteData {
        std::vector<PlaybackPoint> polyline;
        std::vector<float> segmentSpeedFactors;
        RouteInsight insight;
        bool valid = false;
    };

    ThemeDashboardInfo dashboardInfo;
    SceneLayerToggles layerToggles;
    unsigned int layoutSeed;
    CityWeather weather;
    float trafficClock;
    int gridColumns;
    int gridRows;
    const MapGraph* activeGraph;

    std::vector<Intersection> intersections;
    std::vector<RoadSegment> roads;
    std::vector<std::vector<int>> roadAdjacency;
    std::vector<BuildingLot> buildings;
    std::vector<AmbientCar> ambientCars;
    std::vector<int> nodeAnchors;
    std::vector<std::vector<PairRouteData>> pairRoutes;

    void generateGridNetwork(const MapGraph& graph, std::mt19937& rng);
    void assignNodeAnchors(const MapGraph& graph);
    void generateBuildings(const MapGraph& graph, std::mt19937& rng);
    void generateAmbientCars(std::mt19937& rng);
    void refreshPairRoutes(const MapGraph& graph);
    std::vector<int> shortestPath(int startIntersection, int endIntersection) const;

    float roadCost(const RoadSegment& road) const;
    float roadTravelSpeedFactor(const RoadSegment& road) const;
    float weatherDistanceMultiplier() const;
    float weatherOverlayStrength() const;
    bool isTrafficLightGreen(const Intersection& intersection, float animationTime) const;
    int findRoadIndex(int fromIntersection, int toIntersection) const;

    void refreshDashboardInfo();
    void drawRoutePath(IsometricRenderer& renderer,
                       const MissionPresentation& mission,
                       float routeRevealProgress,
                       float animationTime) const;
    void drawBuildingLot(IsometricRenderer& renderer,
                         const BuildingLot& building) const;
    void drawTrafficLight(IsometricRenderer& renderer,
                          const Intersection& intersection,
                          float animationTime) const;
};

#endif // CITY_THEME_RENDERER_H
