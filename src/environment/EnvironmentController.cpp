#include "environment/EnvironmentController.h"

#include "core/MapGraph.h"
#include "visualization/CityThemeRenderer.h"
#include "visualization/IThemeRenderer.h"
#include "visualization/SeaThemeRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace {
constexpr unsigned int kFnvOffsetBasis = 2166136261u;
constexpr unsigned int kFnvPrime = 16777619u;
constexpr float kCoordinateQuantizationScale = 1000.0f;

void mixSeed(unsigned int& seed, unsigned int value) {
    seed ^= value;
    seed *= kFnvPrime;
}

unsigned int stableVisualSeed(const MapGraph& graph) {
    unsigned int seed = kFnvOffsetBasis;
    mixSeed(seed, static_cast<unsigned int>(graph.getNodeCount()));

    for (const WasteNode& node : graph.getNodes()) {
        mixSeed(seed, static_cast<unsigned int>(node.getId()));
        mixSeed(seed, static_cast<unsigned int>(
            std::lround(node.getWorldX() * kCoordinateQuantizationScale)));
        mixSeed(seed, static_cast<unsigned int>(
            std::lround(node.getWorldY() * kCoordinateQuantizationScale)));
        mixSeed(seed, node.getIsHQ() ? 0x9E3779B9u : 0x7F4A7C15u);
    }

    return seed;
}
} // namespace

EnvironmentController::EnvironmentController()
    : seaTheme(std::make_unique<SeaThemeRenderer>()),
      cityTheme(std::make_unique<CityThemeRenderer>()),
      activeTheme(EnvironmentTheme::City),
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

void EnvironmentController::rebuildScenes(const MapGraph& graph) {
    const unsigned int seed = stableVisualSeed(graph);
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

void EnvironmentController::setCitySeason(CitySeason season, MapGraph& graph) {
    static_cast<CityThemeRenderer&>(*cityTheme).setSeason(season);
    if (activeTheme == EnvironmentTheme::City) {
        applyActiveWeights(graph);
        transitionAlpha = 0.25f;
    }
}

void EnvironmentController::randomizeCityTraffic(unsigned int seed, MapGraph& graph) {
    static_cast<CityThemeRenderer&>(*cityTheme).randomizeTrafficConditions(seed);
    if (activeTheme == EnvironmentTheme::City) {
        applyActiveWeights(graph);
        transitionAlpha = 0.25f;
    }
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
