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
    void randomizeTrafficConditions(unsigned int seed);
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
    enum class DistrictType {
        Core,
        Mixed,
        Residential,
        Utility
    };

    enum class BuildingFamily {
        Utility,
        Commercial,
        Residential,
        Landmark
    };

    enum class RoofProfile {
        Flat,
        Stepped,
        Terrace,
        Crown
    };

    enum class VisualTier {
        Peripheral,
        Support,
        Focus
    };

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
        float focusWeight = 0.0f;
        VisualTier visualTier = VisualTier::Support;
    };

    struct BlockZone {
        int row = 0;
        int col = 0;
        float minX = 0.0f;
        float maxX = 0.0f;
        float minY = 0.0f;
        float maxY = 0.0f;
        float centerX = 0.0f;
        float centerY = 0.0f;
        float focusWeight = 0.0f;
        float occupancyTarget = 0.0f;
        DistrictType district = DistrictType::Mixed;
        VisualTier visualTier = VisualTier::Support;
        bool landmarkCluster = false;
        bool openPocket = false;
        bool serviceCourt = false;
    };

    struct BuildingLot {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float depth = 0.0f;
        float height = 0.0f;
        float hueShift = 0.0f;
        float saturation = 0.0f;
        float facadeBrightness = 0.0f;
        float edgeHighlight = 0.0f;
        float windowWarmth = 0.0f;
        float windowDensity = 0.0f;
        float occupancy = 0.0f;
        float podiumRatio = 0.0f;
        float taper = 0.0f;
        DistrictType district = DistrictType::Mixed;
        BuildingFamily family = BuildingFamily::Commercial;
        RoofProfile roofProfile = RoofProfile::Flat;
        VisualTier visualTier = VisualTier::Support;
        bool landmark = false;
    };

    struct AmbientStreet {
        float startX = 0.0f;
        float startY = 0.0f;
        float endX = 0.0f;
        float endY = 0.0f;
        float width = 0.0f;
        float glow = 0.0f;
        VisualTier visualTier = VisualTier::Peripheral;
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
    float sceneMinX;
    float sceneMaxX;
    float sceneMinY;
    float sceneMaxY;
    float peripheralMinX;
    float peripheralMaxX;
    float peripheralMinY;
    float peripheralMaxY;
    float nodeCentroidX;
    float nodeCentroidY;
    float operationalCenterX;
    float operationalCenterY;
    float operationalRadiusX;
    float operationalRadiusY;
    const MapGraph* activeGraph;

    std::vector<Intersection> intersections;
    std::vector<RoadSegment> roads;
    std::vector<std::vector<int>> roadAdjacency;
    std::vector<BlockZone> blocks;
    std::vector<BuildingLot> buildings;
    std::vector<BuildingLot> backgroundBuildings;
    std::vector<AmbientStreet> ambientStreets;
    std::vector<AmbientCar> ambientCars;
    std::vector<int> nodeAnchors;
    std::vector<std::vector<PairRouteData>> pairRoutes;

    void generateGridNetwork(const MapGraph& graph, std::mt19937& rng);
    void assignNodeAnchors(const MapGraph& graph);
    void updateSceneFocus(const MapGraph& graph);
    void generateBlocks(std::mt19937& rng);
    void generateBuildings(const MapGraph& graph, std::mt19937& rng);
    void generatePeripheralScene(std::mt19937& rng);
    void generateAmbientCars(std::mt19937& rng);
    void applyTrafficConditions(std::mt19937& rng);
    void refreshPairRoutes(const MapGraph& graph);
    std::vector<int> shortestPath(int startIntersection, int endIntersection) const;

    float roadCost(const RoadSegment& road) const;
    float roadTravelSpeedFactor(const RoadSegment& road) const;
    float weatherDistanceMultiplier() const;
    float weatherOverlayStrength() const;
    float computeOperationalFocus(float x, float y) const;
    DistrictType districtFromFocus(float focusWeight) const;
    VisualTier visualTierFromFocus(float focusWeight) const;
    bool isTrafficLightGreen(const Intersection& intersection, float animationTime) const;
    bool isReservedVisualArea(float x, float y, float clearance, const MapGraph& graph) const;
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
