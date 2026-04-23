#include "environment/EnvironmentController.h"

#include "core/MapGraph.h"
#include "visualization/CityThemeRenderer.h"
#include "visualization/IThemeRenderer.h"
#include "visualization/SeaThemeRenderer.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace {
constexpr unsigned int kFnvOffsetBasis = 2166136261u;
constexpr unsigned int kFnvPrime = 16777619u;
constexpr float kCoordinateQuantizationScale = 1000.0f;

// XOR masks that give sea and city renderers independent seed streams from the
// same stable graph hash. Values are arbitrary non-zero constants chosen to
// differ in several bit positions so the two streams are unlikely to correlate.
constexpr unsigned int kSeaSceneSeedXor  = 0x52A913u;
constexpr unsigned int kCitySceneSeedXor = 0x7A5C221u;

// Transition flash alpha on theme switch vs minor scene refresh (season/traffic).
constexpr float kThemeChangeAlpha  = 0.35f;
constexpr float kSceneRefreshAlpha = 0.25f;
// Rate at which the flash fades per second.
constexpr float kTransitionFadeRate = 0.85f;

void mixSeed(unsigned int& seed, unsigned int value) {
    seed ^= value;
    seed *= kFnvPrime;
}

// Safe downcast: cityTheme is always constructed as CityThemeRenderer. Placing
// the cast in one helper means any future type change causes a single compile error.
CityThemeRenderer& asCityRenderer(std::unique_ptr<IThemeRenderer>& ptr) {
    assert(ptr != nullptr);
    return static_cast<CityThemeRenderer&>(*ptr);
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
    seaTheme->rebuildScene(graph, seed ^ kSeaSceneSeedXor);
    cityTheme->rebuildScene(graph, seed ^ kCitySceneSeedXor);
}

bool EnvironmentController::setActiveTheme(EnvironmentTheme theme, MapGraph& graph) {
    if (activeTheme == theme) {
        return false;
    }

    activeTheme = theme;
    applyActiveWeights(graph);
    transitionAlpha = kThemeChangeAlpha;
    return true;
}

void EnvironmentController::applyActiveWeights(MapGraph& graph) {
    rendererFor(activeTheme).applyRouteWeights(graph);
}

void EnvironmentController::setCitySeason(CitySeason season, MapGraph& graph) {
    asCityRenderer(cityTheme).setSeason(season);
    if (activeTheme == EnvironmentTheme::City) {
        applyActiveWeights(graph);
        transitionAlpha = kSceneRefreshAlpha;
    }
}

void EnvironmentController::randomizeCityTraffic(unsigned int seed, MapGraph& graph) {
    asCityRenderer(cityTheme).randomizeTrafficConditions(seed);
    if (activeTheme == EnvironmentTheme::City) {
        applyActiveWeights(graph);
        transitionAlpha = kSceneRefreshAlpha;
    }
}

void EnvironmentController::randomizeCityWeather(unsigned int seed, MapGraph& graph) {
    cityTheme->randomizeWeather(seed);
    if (activeTheme == EnvironmentTheme::City) {
        applyActiveWeights(graph);
        transitionAlpha = kSceneRefreshAlpha;
    }
}

void EnvironmentController::update(float deltaTime) {
    seaTheme->update(deltaTime);
    cityTheme->update(deltaTime);
    transitionAlpha = std::max(0.0f, transitionAlpha - deltaTime * kTransitionFadeRate);
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
