#ifndef ENVIRONMENT_CONTROLLER_H
#define ENVIRONMENT_CONTROLLER_H

#include "environment/EnvironmentTypes.h"
#include "environment/MissionPresentation.h"

#include <memory>

class MapGraph;
class IThemeRenderer;

class EnvironmentController {
private:
    std::unique_ptr<IThemeRenderer> seaTheme;
    std::unique_ptr<IThemeRenderer> cityTheme;
    EnvironmentTheme activeTheme;
    SceneLayerToggles layerToggles;
    float transitionAlpha;

    IThemeRenderer& rendererFor(EnvironmentTheme theme);
    const IThemeRenderer& rendererFor(EnvironmentTheme theme) const;

public:
    EnvironmentController();
    ~EnvironmentController();

    bool init();
    void rebuildScenes(const MapGraph& graph, unsigned int seed);
    bool setActiveTheme(EnvironmentTheme theme, MapGraph& graph);
    void applyActiveWeights(MapGraph& graph) const;
    void randomizeCityWeather(unsigned int seed, MapGraph& graph);
    void update(float deltaTime);
    void setLayerToggles(const SceneLayerToggles& toggles);

    EnvironmentTheme getActiveTheme() const;
    const SceneLayerToggles& getLayerToggles() const;
    float getTransitionAlpha() const;
    ThemeDashboardInfo getDashboardInfo() const;
    MissionPresentation buildMissionPresentation(const RouteResult& route,
                                                 const MapGraph& graph) const;

    IThemeRenderer& activeRenderer();
    const IThemeRenderer& activeRenderer() const;

    void cleanup();
};

#endif // ENVIRONMENT_CONTROLLER_H
