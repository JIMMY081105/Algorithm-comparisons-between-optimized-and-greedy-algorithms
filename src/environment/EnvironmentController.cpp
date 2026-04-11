#include "environment/EnvironmentController.h"

#include "core/MapGraph.h"
#include "visualization/CityThemeRenderer.h"
#include "visualization/IThemeRenderer.h"
#include "visualization/SeaThemeRenderer.h"

#include <algorithm>
#include <stdexcept>

EnvironmentController::EnvironmentController()
    : seaTheme(std::make_unique<SeaThemeRenderer>()),
      cityTheme(std::make_unique<CityThemeRenderer>()),
      activeTheme(EnvironmentTheme::Sea),
      transitionAlpha(0.0f) {}

EnvironmentController::~EnvironmentController() = default;

IThemeRenderer& EnvironmentController::rendererFor(EnvironmentTheme theme) {
    return (theme == EnvironmentTheme::City) ? *cityTheme : *seaTheme;
}

const IThemeRenderer& EnvironmentController::rendererFor(EnvironmentTheme theme) const {
    return (theme == EnvironmentTheme::City) ? *cityTheme : *seaTheme;
}

bool EnvironmentController::init() {
    return seaTheme->init() && cityTheme->init();
}

void EnvironmentController::rebuildScenes(const MapGraph& graph, unsigned int seed) {
    seaTheme->setLayerToggles(layerToggles);
    cityTheme->setLayerToggles(layerToggles);
    seaTheme->rebuildScene(graph, seed ^ 0x52A913u);
    cityTheme->rebuildScene(graph, seed ^ 0x7A5C221u);
}

bool EnvironmentController::setActiveTheme(EnvironmentTheme theme, MapGraph& graph) {
    if (activeTheme == theme) {
        return false;
    }

    activeTheme = theme;
    applyActiveWeights(graph);
    transitionAlpha = 0.35f;
    return true;
}

void EnvironmentController::applyActiveWeights(MapGraph& graph) const {
    rendererFor(activeTheme).applyRouteWeights(graph);
}

void EnvironmentController::randomizeCityWeather(unsigned int seed, MapGraph& graph) {
    cityTheme->randomizeWeather(seed);
    if (activeTheme == EnvironmentTheme::City) {
        applyActiveWeights(graph);
        transitionAlpha = 0.25f;
    }
}

void EnvironmentController::update(float deltaTime) {
    seaTheme->update(deltaTime);
    cityTheme->update(deltaTime);
    transitionAlpha = std::max(0.0f, transitionAlpha - deltaTime * 0.85f);
}

void EnvironmentController::setLayerToggles(const SceneLayerToggles& toggles) {
    layerToggles = toggles;
    seaTheme->setLayerToggles(layerToggles);
    cityTheme->setLayerToggles(layerToggles);
}

EnvironmentTheme EnvironmentController::getActiveTheme() const {
    return activeTheme;
}

const SceneLayerToggles& EnvironmentController::getLayerToggles() const {
    return layerToggles;
}

float EnvironmentController::getTransitionAlpha() const {
    return transitionAlpha;
}

ThemeDashboardInfo EnvironmentController::getDashboardInfo() const {
    return rendererFor(activeTheme).getDashboardInfo();
}

MissionPresentation EnvironmentController::buildMissionPresentation(
    const RouteResult& route,
    const MapGraph& graph) const {
    return rendererFor(activeTheme).buildMissionPresentation(route, graph);
}

IThemeRenderer& EnvironmentController::activeRenderer() {
    return rendererFor(activeTheme);
}

const IThemeRenderer& EnvironmentController::activeRenderer() const {
    return rendererFor(activeTheme);
}

void EnvironmentController::cleanup() {
    seaTheme->cleanup();
    cityTheme->cleanup();
}
