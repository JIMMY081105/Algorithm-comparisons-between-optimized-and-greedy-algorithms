#include "visualization/SeaThemeRenderer.h"
#include "environment/MissionPresentationUtils.h"
#include "visualization/IsometricRenderer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// =====================================================================
//  Anonymous helper utilities (shared with sea theme only)
// =====================================================================
namespace {

constexpr float kFieldPadding = 3.4f;
constexpr float kWaterSampleSpacing = 1.55f;
constexpr float kDepotClearRadius = 0.68f;
constexpr float kClearInnerRadius = 0.20f;
constexpr float kClearOuterRadius = 0.56f;
constexpr int kAmbientParticleCount = 42;
constexpr int kDecorIsletCount = 16;

const RouteResult& routeFromMission(const MissionPresentation* mission) {
    static const RouteResult kEmptyRoute{};
    return mission ? mission->route : kEmptyRoute;
}

enum class GarbageSizeTier {
    SMALL,
    MEDIUM,
    LARGE
};

struct OceanWaveField {
    float swell;
    float primary;
    float secondary;
    float detail;
    float combined;
    float crest;
    float shimmer;
};

int positiveMod(int value, int base) {
    int mod = value % base;
    return (mod < 0) ? mod + base : mod;
}

float clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

float clampColor(float value) {
    return clamp01(value);
}

Color shiftColor(const Color& color, float delta, float alphaScale = 1.0f) {
    return Color(
        clampColor(color.r + delta),
        clampColor(color.g + delta),
        clampColor(color.b + delta),
        clampColor(color.a * alphaScale));
}

Color mixColor(const Color& from, const Color& to, float t) {
    t = clamp01(t);
    return Color(
        RenderUtils::lerp(from.r, to.r, t),
        RenderUtils::lerp(from.g, to.g, t),
        RenderUtils::lerp(from.b, to.b, t),
        RenderUtils::lerp(from.a, to.a, t));
}

Color withAlpha(const Color& color, float alpha) {
    return Color(color.r, color.g, color.b, clampColor(alpha));
}

float pseudoRandom01(int x, int y, int salt) {
    const int hash = positiveMod(x * 157 + y * 313 + salt * 571, 997);
    return static_cast<float>(hash) / 996.0f;
}

float pseudoRandomSigned(int x, int y, int salt) {
    return pseudoRandom01(x, y, salt) * 2.0f - 1.0f;
}

float clearanceFalloff(float distance, float innerRadius, float outerRadius) {
    if (distance <= innerRadius) return 1.0f;
    if (distance >= outerRadius) return 0.0f;
    const float t = (distance - innerRadius) / (outerRadius - innerRadius);
    return 1.0f - RenderUtils::smoothstep(t);
}

GarbageSizeTier getGarbageSizeTier(float capacity) {
    if (capacity <= 320.0f) return GarbageSizeTier::SMALL;
    if (capacity <= 430.0f) return GarbageSizeTier::MEDIUM;
    return GarbageSizeTier::LARGE;
}

float getGarbageScale(GarbageSizeTier tier) {
    switch (tier) {
        case GarbageSizeTier::SMALL: return 3.5f;
        case GarbageSizeTier::MEDIUM: return 4.2f;
        case GarbageSizeTier::LARGE: return 5.0f;
        default: return 2.0f;
    }
}

Color getGarbageSizeColor(GarbageSizeTier tier) {
    switch (tier) {
        case GarbageSizeTier::SMALL:
            return Color(0.35f, 0.50f, 0.30f, 0.98f);
        case GarbageSizeTier::MEDIUM:
            return Color(0.55f, 0.45f, 0.25f, 0.98f);
        case GarbageSizeTier::LARGE:
            return Color(0.65f, 0.30f, 0.20f, 0.98f);
        default:
            return Color(0.45f, 0.55f, 0.60f, 0.98f);
    }
}

void drawImmediateQuad(const std::array<IsoCoord, 4>& pts, const Color& color) {
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    for (const auto& pt : pts) {
        glVertex2f(pt.x, pt.y);
    }
    glEnd();
}

void drawImmediateTriangle(const std::array<IsoCoord, 3>& pts, const Color& color) {
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_TRIANGLES);
    for (const auto& pt : pts) {
        glVertex2f(pt.x, pt.y);
    }
    glEnd();
}

void drawWorldQuadPatch(float minWorldX, float minWorldY,
                        float maxWorldX, float maxWorldY,
                        const Color& color) {
    drawImmediateQuad({
        RenderUtils::worldToIso(minWorldX, minWorldY),
        RenderUtils::worldToIso(maxWorldX, minWorldY),
        RenderUtils::worldToIso(maxWorldX, maxWorldY),
        RenderUtils::worldToIso(minWorldX, maxWorldY)
    }, color);
}

void drawWorldDiamondPatch(float centerWorldX, float centerWorldY,
                           float radiusWorldX, float radiusWorldY,
                           const Color& color) {
    drawImmediateQuad({
        RenderUtils::worldToIso(centerWorldX, centerWorldY - radiusWorldY),
        RenderUtils::worldToIso(centerWorldX + radiusWorldX, centerWorldY),
        RenderUtils::worldToIso(centerWorldX, centerWorldY + radiusWorldY),
        RenderUtils::worldToIso(centerWorldX - radiusWorldX, centerWorldY)
    }, color);
}

void drawWorldSwellBand(float startWorldX, float startWorldY,
                        float endWorldX, float endWorldY,
                        float halfWidthWorld,
                        const Color& color) {
    float dirX = endWorldX - startWorldX;
    float dirY = endWorldY - startWorldY;
    const float length = std::sqrt(dirX * dirX + dirY * dirY);
    if (length <= 0.0001f) return;

    dirX /= length;
    dirY /= length;
    const float perpX = -dirY * halfWidthWorld;
    const float perpY = dirX * halfWidthWorld;

    drawImmediateQuad({
        RenderUtils::worldToIso(startWorldX - perpX, startWorldY - perpY),
        RenderUtils::worldToIso(endWorldX - perpX, endWorldY - perpY),
        RenderUtils::worldToIso(endWorldX + perpX, endWorldY + perpY),
        RenderUtils::worldToIso(startWorldX + perpX, startWorldY + perpY)
    }, color);
}

void drawOrganicWorldPatch(float centerWorldX, float centerWorldY,
                           float radiusWorldX, float radiusWorldY,
                           float rotation, float irregularity,
                           float time, float seedPhase,
                           const Color& centerColor, const Color& edgeColor) {
    const int segments = 30;
    const float rotC = std::cos(rotation);
    const float rotS = std::sin(rotation);
    const IsoCoord centerIso = RenderUtils::worldToIso(centerWorldX, centerWorldY);

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(centerColor.r, centerColor.g, centerColor.b, centerColor.a);
    glVertex2f(centerIso.x, centerIso.y);

    for (int i = 0; i <= segments; i++) {
        const float angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) /
                            static_cast<float>(segments);
        const float radiusNoiseX =
            1.0f + irregularity *
                (0.26f * std::sin(angle * 2.0f + seedPhase * 1.7f + time * 0.08f) +
                 0.16f * std::sin(angle * 3.0f - seedPhase * 1.2f) +
                 0.08f * std::sin(angle * 5.0f + seedPhase * 0.6f));
        const float radiusNoiseY =
            1.0f + irregularity *
                (0.22f * std::sin(angle * 2.0f - seedPhase * 1.3f - time * 0.05f) +
                 0.18f * std::sin(angle * 4.0f + seedPhase * 0.8f) -
                 0.07f * std::sin(angle * 5.0f - seedPhase * 0.4f));
        const float localX = std::cos(angle) * radiusWorldX * radiusNoiseX;
        const float localY = std::sin(angle) * radiusWorldY * radiusNoiseY;
        const float worldX = centerWorldX + localX * rotC - localY * rotS;
        const float worldY = centerWorldY + localX * rotS + localY * rotC;
        const IsoCoord pt = RenderUtils::worldToIso(worldX, worldY);

        glColor4f(edgeColor.r, edgeColor.g, edgeColor.b, edgeColor.a);
        glVertex2f(pt.x, pt.y);
    }

    glEnd();
}

void drawWorldWaveRibbon(float startWorldX, float startWorldY,
                         float endWorldX, float endWorldY,
                         float halfWidthWorld, float amplitudeWorld,
                         float frequency, float phase,
                         const Color& color) {
    const int segments = 20;
    float dirX = endWorldX - startWorldX;
    float dirY = endWorldY - startWorldY;
    const float length = std::sqrt(dirX * dirX + dirY * dirY);
    if (length <= 0.0001f) return;

    dirX /= length;
    dirY /= length;
    const float perpX = -dirY;
    const float perpY = dirX;

    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; i++) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float baseX = RenderUtils::lerp(startWorldX, endWorldX, t);
        const float baseY = RenderUtils::lerp(startWorldY, endWorldY, t);
        const float waveOffset =
            std::sin(t * frequency * 2.0f * static_cast<float>(M_PI) + phase) * amplitudeWorld +
            std::sin(t * frequency * 4.0f * static_cast<float>(M_PI) - phase * 0.62f) *
                amplitudeWorld * 0.35f;
        const float widthScale = 0.62f + 0.38f * std::sin(t * static_cast<float>(M_PI));
        const float px = baseX + perpX * waveOffset;
        const float py = baseY + perpY * waveOffset;
        const IsoCoord left = RenderUtils::worldToIso(px - perpX * halfWidthWorld * widthScale,
                                                      py - perpY * halfWidthWorld * widthScale);
        const IsoCoord right = RenderUtils::worldToIso(px + perpX * halfWidthWorld * widthScale,
                                                       py + perpY * halfWidthWorld * widthScale);
        glVertex2f(left.x, left.y);
        glVertex2f(right.x, right.y);
    }
    glEnd();
}

OceanWaveField sampleOceanWaveField(float worldX, float worldY, float time) {
    constexpr float kDir1X = 0.90f;
    constexpr float kDir1Y = 0.43f;
    constexpr float kDir2X = 0.34f;
    constexpr float kDir2Y = 0.94f;
    constexpr float kDir3X = -0.76f;
    constexpr float kDir3Y = 0.65f;
    constexpr float kSwellDirX = 0.52f;
    constexpr float kSwellDirY = 0.85f;
    constexpr float kCrossDirX = -0.92f;
    constexpr float kCrossDirY = 0.21f;

    OceanWaveField wave{};
    wave.swell = std::sin((worldX * kSwellDirX + worldY * kSwellDirY) * 0.075f +
                          time * 0.11f);
    const float crossSwell = std::sin((worldX * kCrossDirX + worldY * kCrossDirY) * 0.062f -
                                      time * 0.09f + 1.7f);
    wave.primary = std::sin((worldX * kDir1X + worldY * kDir1Y) * 0.18f +
                            time * 0.20f + wave.swell * 0.32f);
    wave.secondary = std::sin((worldX * kDir2X + worldY * kDir2Y) * 0.31f +
                              time * 0.29f + 0.85f + crossSwell * 0.22f);
    wave.detail = std::sin((worldX * kDir3X + worldY * kDir3Y) * 0.70f +
                           time * 0.44f + 1.70f + wave.secondary * 0.18f);
    wave.combined = wave.swell * 0.48f +
                    crossSwell * 0.18f +
                    wave.primary * 0.20f +
                    wave.secondary * 0.10f +
                    wave.detail * 0.04f;
    wave.crest = clamp01(0.5f + 0.5f *
        (wave.swell * 0.54f + wave.primary * 0.22f +
         wave.secondary * 0.16f + wave.detail * 0.08f));
    wave.shimmer = std::sin((worldX * 0.64f - worldY * 0.78f) * 0.42f +
                            time * 0.36f + wave.primary * 0.22f);
    return wave;
}

float sampleOceanSurfaceOffset(float worldX, float worldY, float time) {
    const OceanWaveField wave = sampleOceanWaveField(worldX, worldY, time);
    return wave.swell * 0.52f +
           wave.primary * 0.23f +
           wave.secondary * 0.11f +
           wave.detail * 0.04f;
}

float distancePointToSegment(float px, float py,
                              float ax, float ay,
                              float bx, float by) {
    const float abx = bx - ax;
    const float aby = by - ay;
    const float abLenSq = abx * abx + aby * aby;

    if (abLenSq <= 0.0001f) {
        const float dx = px - ax;
        const float dy = py - ay;
        return std::sqrt(dx * dx + dy * dy);
    }

    float t = ((px - ax) * abx + (py - ay) * aby) / abLenSq;
    t = std::max(0.0f, std::min(1.0f, t));

    const float nearestX = ax + abx * t;
    const float nearestY = ay + aby * t;
    const float dx = px - nearestX;
    const float dy = py - nearestY;
    return std::sqrt(dx * dx + dy * dy);
}

bool getRouteNodeWorldPos(const MapGraph& graph, int nodeId,
                          float& outX, float& outY) {
    const int idx = graph.findNodeIndex(nodeId);
    if (idx < 0) return false;
    const WasteNode& node = graph.getNode(idx);
    outX = node.getWorldX();
    outY = node.getWorldY();
    return true;
}

void normalizeVector(float& x, float& y, float fallbackX, float fallbackY) {
    const float len = std::sqrt(x * x + y * y);
    if (len <= 0.0001f) {
        x = fallbackX;
        y = fallbackY;
        return;
    }
    x /= len;
    y /= len;
}

void getTruckHeading(const MapGraph& graph, const Truck& truck,
                     const RouteResult& currentRoute,
                     float& outHeadingX, float& outHeadingY) {
    outHeadingX = 1.0f;
    outHeadingY = 0.0f;

    if (!currentRoute.isValid() || currentRoute.visitOrder.size() < 2) return;

    const int lastSegment = static_cast<int>(currentRoute.visitOrder.size()) - 2;
    if (lastSegment < 0) return;

    int seg = std::max(0, std::min(truck.getCurrentSegment(), lastSegment));
    float fromX = 0.0f, fromY = 0.0f, toX = 0.0f, toY = 0.0f;

    if (!getRouteNodeWorldPos(graph, currentRoute.visitOrder[seg], fromX, fromY) ||
        !getRouteNodeWorldPos(graph, currentRoute.visitOrder[seg + 1], toX, toY)) {
        return;
    }

    outHeadingX = toX - fromX;
    outHeadingY = toY - fromY;

    if (std::sqrt(outHeadingX * outHeadingX + outHeadingY * outHeadingY) <= 0.0001f &&
        seg > 0 &&
        getRouteNodeWorldPos(graph, currentRoute.visitOrder[seg - 1], fromX, fromY)) {
        outHeadingX = toX - fromX;
        outHeadingY = toY - fromY;
    }

    normalizeVector(outHeadingX, outHeadingY, 1.0f, 0.0f);
}

float computeBoatCargoFill(const MapGraph& graph, const Truck& truck,
                           const RouteResult& currentRoute) {
    if (!currentRoute.isValid() || currentRoute.visitOrder.size() < 2) return 0.0f;

    float totalWaste = 0.0f;
    float collectedWaste = 0.0f;
    const int visitedWaypoint = std::max(0, truck.getCurrentSegment());

    for (size_t i = 1; i < currentRoute.visitOrder.size(); i++) {
        const int idx = graph.findNodeIndex(currentRoute.visitOrder[i]);
        if (idx < 0) continue;
        const WasteNode& node = graph.getNode(idx);
        if (node.getIsHQ()) continue;
        const float waste = node.getWasteAmount();
        totalWaste += waste;
        if (static_cast<int>(i) <= visitedWaypoint) {
            collectedWaste += waste;
        }
    }

    return (totalWaste <= 0.001f) ? 0.0f : clamp01(collectedWaste / totalWaste);
}

float getNodeShelfInfluence(const WasteNode& node, float worldX, float worldY) {
    const float dx = worldX - node.getWorldX();
    const float dy = worldY - node.getWorldY();
    const float distance = std::sqrt(dx * dx + dy * dy);
    const float capacityNorm = clamp01(node.getWasteCapacity() / 550.0f);
    const float radius = node.getIsHQ()
        ? 6.6f
        : 3.2f + capacityNorm * 1.4f;
    const float innerRadius = node.getIsHQ()
        ? radius * 0.38f
        : radius * 0.22f;

    float influence = clearanceFalloff(distance, innerRadius, radius);
    if (node.getIsHQ()) {
        influence = clamp01(influence * 1.18f);
    }
    return influence;
}

float getClearingAmount(const MapGraph& graph, const Truck& truck,
                        const RouteResult& currentRoute,
                        float worldX, float worldY) {
    const WasteNode& hq = graph.getHQNode();
    float minDistance = std::sqrt((worldX - hq.getWorldX()) * (worldX - hq.getWorldX()) +
                                  (worldY - hq.getWorldY()) * (worldY - hq.getWorldY()));

    if (!currentRoute.isValid() || currentRoute.visitOrder.empty()) {
        return clearanceFalloff(minDistance, 0.0f, kDepotClearRadius);
    }

    minDistance = std::min(
        minDistance,
        distancePointToSegment(worldX, worldY,
                               truck.getPosX(), truck.getPosY(),
                               truck.getPosX(), truck.getPosY()));

    const int completedSegments = std::max(0, truck.getCurrentSegment());
    for (int i = 0; i < completedSegments &&
                    i + 1 < static_cast<int>(currentRoute.visitOrder.size()); i++) {
        float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
        if (!getRouteNodeWorldPos(graph, currentRoute.visitOrder[i], ax, ay) ||
            !getRouteNodeWorldPos(graph, currentRoute.visitOrder[i + 1], bx, by)) {
            continue;
        }
        minDistance = std::min(minDistance,
                               distancePointToSegment(worldX, worldY, ax, ay, bx, by));
    }

    if (truck.getCurrentSegment() >= 0 &&
        truck.getCurrentSegment() < static_cast<int>(currentRoute.visitOrder.size())) {
        float startX = 0.0f, startY = 0.0f;
        if (getRouteNodeWorldPos(graph,
                                 currentRoute.visitOrder[truck.getCurrentSegment()],
                                 startX, startY) &&
            truck.isMoving()) {
            minDistance = std::min(minDistance,
                                   distancePointToSegment(worldX, worldY, startX, startY,
                                                          truck.getPosX(), truck.getPosY()));
        }
    }

    return clearanceFalloff(minDistance, kClearInnerRadius, kClearOuterRadius);
}

}  // namespace

// =====================================================================
//  SeaThemeRenderer implementation
// =====================================================================

SeaThemeRenderer::SeaThemeRenderer()
    : dashboardInfo{EnvironmentTheme::Sea, "Sea", "Harbour command",
                    "N/A", "Open-water transit and port logistics", 0.0f, 0, false},
      sceneSeed(0),
      currentGarbageSinkProgress(0.0f),
      boatCargoFillRatio(0.0f),
      boatWakeStrength(0.0f),
      boatPitchWave(0.0f),
      boatRollWave(0.0f),
      boatIsMoving(false),
      sceneBounds{0.0f, 0.0f, 0.0f, 0.0f},
      activeGraph(nullptr) {}

EnvironmentTheme SeaThemeRenderer::getTheme() const {
    return EnvironmentTheme::Sea;
}

bool SeaThemeRenderer::init() {
    return oceanShader.init();
}

void SeaThemeRenderer::rebuildScene(const MapGraph& graph, unsigned int seed) {
    sceneSeed = seed;
    activeGraph = &graph;
    updateSceneBounds(graph);
    dashboardInfo.subtitle =
        (seed % 2 == 0) ? "Harbour command" : "Offshore response deck";
    dashboardInfo.atmosphereLabel =
        (seed % 3 == 0) ? "Calm swells and clean marine lanes"
                        : "Rolling tides across the cleanup corridor";
}

void SeaThemeRenderer::update(float deltaTime) {
    currentGarbageSinkProgress = clamp01(currentGarbageSinkProgress + deltaTime * 0.2f);
}

void SeaThemeRenderer::applyRouteWeights(MapGraph& graph) const {
    graph.buildFullyConnectedGraph();
}

MissionPresentation SeaThemeRenderer::buildMissionPresentation(
    const RouteResult& route,
    const MapGraph& graph) const {
    MissionPresentation presentation;
    presentation.route = route;
    if (!route.isValid()) {
        return presentation;
    }

    presentation.playbackPath = buildDirectPlaybackPath(route, graph);
    presentation.narrative =
        "Sea mode uses direct marine lanes between cleanup sectors for clear route comparison.";

    for (std::size_t i = 0; i + 1 < route.visitOrder.size(); ++i) {
        const int fromId = route.visitOrder[i];
        const int toId = route.visitOrder[i + 1];
        presentation.legInsights.push_back(RouteInsight{
            fromId,
            toId,
            graph.getDistance(fromId, toId),
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            graph.getDistance(fromId, toId)
        });
    }

    return presentation;
}

ThemeDashboardInfo SeaThemeRenderer::getDashboardInfo() const {
    return dashboardInfo;
}

void SeaThemeRenderer::setLayerToggles(const SceneLayerToggles& toggles) {
    layerToggles = toggles;
}

bool SeaThemeRenderer::supportsWeather() const {
    return false;
}

CityWeather SeaThemeRenderer::getWeather() const {
    return CityWeather::Sunny;
}

void SeaThemeRenderer::setWeather(CityWeather) {}

void SeaThemeRenderer::randomizeWeather(unsigned int) {}

void SeaThemeRenderer::cleanup() {
    oceanShader.cleanup();
}

void SeaThemeRenderer::updateSceneBounds(const MapGraph& graph) {
    const auto& nodes = graph.getNodes();
    if (nodes.empty()) {
        sceneBounds = {0.0f, 0.0f, 0.0f, 0.0f};
        return;
    }

    float minWorldX = nodes.front().getWorldX();
    float maxWorldX = nodes.front().getWorldX();
    float minWorldY = nodes.front().getWorldY();
    float maxWorldY = nodes.front().getWorldY();

    for (const auto& node : nodes) {
        minWorldX = std::min(minWorldX, node.getWorldX());
        maxWorldX = std::max(maxWorldX, node.getWorldX());
        minWorldY = std::min(minWorldY, node.getWorldY());
        maxWorldY = std::max(maxWorldY, node.getWorldY());
    }

    sceneBounds = {minWorldX, maxWorldX, minWorldY, maxWorldY};
}

void SeaThemeRenderer::updateGarbageSinkState(const MapGraph& graph, float deltaTime) {
    std::unordered_map<int, float> nextState;
    nextState.reserve(graph.getNodeCount());

    for (const auto& node : graph.getNodes()) {
        float progress = 0.0f;
        const auto it = garbageSinkProgress.find(node.getId());
        if (it != garbageSinkProgress.end()) {
            progress = it->second;
        }

        if (node.isCollected()) {
            progress = clamp01(progress + deltaTime * 1.75f);
        } else {
            progress = clamp01(progress - deltaTime * 3.50f);
        }

        nextState[node.getId()] = progress;
    }

    garbageSinkProgress.swap(nextState);
}

float SeaThemeRenderer::getGarbageSinkState(int nodeId) {
    const auto it = garbageSinkProgress.find(nodeId);
    return (it != garbageSinkProgress.end()) ? it->second : 0.0f;
}

PlaybackPath SeaThemeRenderer::buildDirectPlaybackPath(const RouteResult& route,
                                                       const MapGraph& graph) const {
    std::vector<PlaybackPoint> points;
    std::vector<int> stopNodeIds;
    std::vector<std::size_t> stopPointIndices;

    points.reserve(route.visitOrder.size());
    stopNodeIds.reserve(route.visitOrder.size());
    stopPointIndices.reserve(route.visitOrder.size());

    for (int nodeId : route.visitOrder) {
        const int nodeIndex = graph.findNodeIndex(nodeId);
        if (nodeIndex < 0) {
            continue;
        }

        const WasteNode& node = graph.getNode(nodeIndex);
        points.push_back(PlaybackPoint{node.getWorldX(), node.getWorldY()});
        stopNodeIds.push_back(nodeId);
        stopPointIndices.push_back(points.size() - 1);
    }

    return MissionPresentationUtils::buildPlaybackPath(
        points, stopNodeIds, stopPointIndices);
}

void SeaThemeRenderer::drawTransitNetwork(
    IsometricRenderer& renderer,
    const MapGraph& graph,
    const MissionPresentation* mission,
    AnimationController::PlaybackState playbackState,
    float routeRevealProgress,
    float animationTime) {
    constexpr float kMaxVisibleDist = 48.75f;

    for (int i = 0; i < graph.getNodeCount(); ++i) {
        for (int j = i + 1; j < graph.getNodeCount(); ++j) {
            const float dist = graph.getAdjacencyMatrix()[i][j];
            if (dist <= 0.0f || dist >= kMaxVisibleDist) {
                continue;
            }

            const WasteNode& from = graph.getNode(i);
            const WasteNode& to = graph.getNode(j);
            const IsoCoord isoFrom =
                RenderUtils::worldToIso(from.getWorldX(), from.getWorldY());
            const IsoCoord isoTo =
                RenderUtils::worldToIso(to.getWorldX(), to.getWorldY());
            const float alpha = 1.0f - (dist / kMaxVisibleDist);
            renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                              Color(0.18f, 0.35f, 0.48f, 0.36f * alpha), 1.4f);
        }
    }

    if (!mission || !mission->isValid()) {
        return;
    }

    const PlaybackPath& path = mission->playbackPath;
    const float revealDistance = path.totalLength * std::clamp(routeRevealProgress, 0.0f, 1.0f);
    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 2.4f);

    for (std::size_t i = 1; i < path.points.size(); ++i) {
        const float startDistance = path.cumulativeDistances[i - 1];
        const float endDistance = path.cumulativeDistances[i];
        if (revealDistance <= startDistance) {
            break;
        }

        float worldX = path.points[i].x;
        float worldY = path.points[i].y;
        if (revealDistance < endDistance && endDistance > startDistance) {
            const float localT = (revealDistance - startDistance) / (endDistance - startDistance);
            worldX = RenderUtils::lerp(path.points[i - 1].x, path.points[i].x, localT);
            worldY = RenderUtils::lerp(path.points[i - 1].y, path.points[i].y, localT);
        }

        const IsoCoord isoFrom =
            RenderUtils::worldToIso(path.points[i - 1].x, path.points[i - 1].y);
        const IsoCoord isoTo = RenderUtils::worldToIso(worldX, worldY);
        renderer.drawLine(isoFrom.x, isoFrom.y + 1.0f, isoTo.x, isoTo.y + 1.0f,
                          Color(0.03f, 0.16f, 0.22f, 0.10f + pulse * 0.04f), 6.4f);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          Color(0.24f, 0.78f, 0.80f, 0.18f + pulse * 0.10f), 2.2f);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          Color(0.92f, 0.98f, 0.96f, 0.10f + pulse * 0.08f), 0.9f);
    }

    if (playbackState != AnimationController::PlaybackState::IDLE &&
        path.totalLength > 0.0f) {
        const float markerDistance =
            std::fmod(animationTime * 3.5f, std::max(path.totalLength, 0.001f));
        for (std::size_t i = 1; i < path.points.size(); ++i) {
            if (markerDistance > path.cumulativeDistances[i]) {
                continue;
            }

            const float startDistance = path.cumulativeDistances[i - 1];
            const float endDistance = path.cumulativeDistances[i];
            const float localT = (endDistance > startDistance)
                ? (markerDistance - startDistance) / (endDistance - startDistance)
                : 0.0f;
            const float worldX =
                RenderUtils::lerp(path.points[i - 1].x, path.points[i].x, localT);
            const float worldY =
                RenderUtils::lerp(path.points[i - 1].y, path.points[i].y, localT);
            const IsoCoord iso = RenderUtils::worldToIso(worldX, worldY);
            renderer.drawDiamond(iso.x, iso.y, 4.0f, 1.8f,
                                 Color(0.66f, 1.0f, 0.96f, 0.16f));
            renderer.drawFilledCircle(iso.x, iso.y, 1.1f,
                                      Color(1.0f, 1.0f, 1.0f, 0.78f));
            break;
        }
    }
}

// =====================================================================
//  Ground plane — ocean water
// =====================================================================

void SeaThemeRenderer::drawGroundPlane(IsometricRenderer& renderer,
                                        const MapGraph& graph,
                                        const Truck& truck,
                                        const MissionPresentation* mission,
                                        float animationTime) {
    const RouteResult& currentRoute = routeFromMission(mission);
    activeGraph = &graph;
    updateSceneBounds(graph);
    updateGarbageSinkState(graph, renderer.getLastDeltaTime());

    if (graph.getNodes().empty()) return;

    const float paddedMinX = sceneBounds.minX - kFieldPadding - 2.5f;
    const float paddedMaxX = sceneBounds.maxX + kFieldPadding + 2.5f;
    const float paddedMinY = sceneBounds.minY - kFieldPadding - 2.5f;
    const float paddedMaxY = sceneBounds.maxY + kFieldPadding + 2.5f;
    const float centerX = (paddedMinX + paddedMaxX) * 0.5f;
    const float centerY = (paddedMinY + paddedMaxY) * 0.5f;
    const float spanX = std::max(1.0f, paddedMaxX - paddedMinX);
    const float spanY = std::max(1.0f, paddedMaxY - paddedMinY);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const float left = static_cast<float>(viewport[0]);
    const float top = static_cast<float>(viewport[1]);
    const float right = left + static_cast<float>(viewport[2]);
    const float bottom = top + static_cast<float>(viewport[3]);

    glBegin(GL_QUADS);
    glColor4f(0.05f, 0.28f, 0.39f, 1.0f);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();

    drawOrganicWorldPatch(centerX - spanX * 0.28f, centerY + spanY * 0.12f,
                          spanX * 0.82f, spanY * 0.66f,
                          -0.38f, 0.30f, animationTime, 0.7f,
                          Color(0.03f, 0.23f, 0.35f, 0.20f),
                          Color(0.02f, 0.10f, 0.18f, 0.07f));
    drawOrganicWorldPatch(centerX - spanX * 0.40f, centerY + spanY * 0.40f,
                          spanX * 0.54f, spanY * 0.42f,
                          -0.82f, 0.34f, animationTime, 2.1f,
                          Color(0.02f, 0.15f, 0.24f, 0.18f),
                          Color(0.01f, 0.07f, 0.12f, 0.06f));
    drawOrganicWorldPatch(centerX + spanX * 0.26f, centerY - spanY * 0.22f,
                          spanX * 0.62f, spanY * 0.48f,
                          0.32f, 0.28f, animationTime, 3.8f,
                          Color(0.12f, 0.54f, 0.63f, 0.12f),
                          Color(0.24f, 0.72f, 0.76f, 0.04f));
    drawOrganicWorldPatch(centerX + spanX * 0.08f, centerY - spanY * 0.06f,
                          spanX * 0.30f, spanY * 0.20f,
                          -0.18f, 0.26f, animationTime, 5.1f,
                          Color(0.07f, 0.28f, 0.40f, 0.10f),
                          Color(0.02f, 0.10f, 0.18f, 0.03f));
    drawOrganicWorldPatch(centerX - spanX * 0.05f, centerY - spanY * 0.28f,
                          spanX * 0.44f, spanY * 0.22f,
                          0.54f, 0.24f, animationTime, 6.7f,
                          Color(0.18f, 0.58f, 0.64f, 0.08f),
                          Color(0.34f, 0.78f, 0.80f, 0.02f));
    drawOrganicWorldPatch(centerX + spanX * 0.34f, centerY + spanY * 0.26f,
                          spanX * 0.40f, spanY * 0.32f,
                          -0.62f, 0.30f, animationTime, 7.9f,
                          Color(0.02f, 0.12f, 0.20f, 0.12f),
                          Color(0.01f, 0.05f, 0.10f, 0.04f));

    for (int swell = 0; swell < 4; ++swell) {
        const float lane = -spanY * 0.36f + static_cast<float>(swell) * spanY * 0.24f;
        const float drift = std::sin(animationTime * (0.12f + swell * 0.03f) +
                                     static_cast<float>(swell) * 1.7f) * spanY * 0.05f;
        const float width = 1.2f + static_cast<float>(swell) * 0.24f;
        drawWorldSwellBand(centerX - spanX * 0.82f,
                           centerY + lane - spanY * 0.10f + drift,
                           centerX + spanX * 0.84f,
                           centerY + lane + spanY * 0.12f + drift * 0.5f,
                           width,
                           Color(0.02f, 0.12f, 0.18f, 0.040f - swell * 0.004f));
        drawWorldSwellBand(centerX - spanX * 0.72f,
                           centerY + lane - spanY * 0.02f + drift * 0.6f,
                           centerX + spanX * 0.74f,
                           centerY + lane + spanY * 0.06f + drift * 0.2f,
                           width * 0.42f,
                           Color(0.42f, 0.72f, 0.78f, 0.018f - swell * 0.002f));
    }

    auto drawRibbonFamily = [&](float dirX, float dirY, float spacing,
                                float halfWidth, float amplitude,
                                float frequency, float speed,
                                const Color& outerColor,
                                const Color& innerColor) {
        normalizeVector(dirX, dirY, 1.0f, 0.0f);
        const float perpX = -dirY;
        const float perpY = dirX;
        const float coverage = spanX * std::abs(perpX) + spanY * std::abs(perpY) + 12.0f;
        const float halfLength = spanX * std::abs(dirX) + spanY * std::abs(dirY) + 12.0f;

        for (float offset = -coverage; offset <= coverage; offset += spacing) {
            const float drift = std::sin(offset * 0.21f + animationTime * speed) * spacing * 0.16f;
            const float sweep = std::sin(offset * 0.12f - animationTime * speed * 0.58f + 0.9f) * 1.4f;
            const float startX = centerX + perpX * (offset + drift) - dirX * halfLength + dirX * sweep;
            const float startY = centerY + perpY * (offset + drift) - dirY * halfLength + dirY * sweep;
            const float endX = centerX + perpX * (offset + drift) + dirX * halfLength + dirX * sweep;
            const float endY = centerY + perpY * (offset + drift) + dirY * halfLength + dirY * sweep;
            const OceanWaveField wave = sampleOceanWaveField(centerX + perpX * offset,
                                                             centerY + perpY * offset,
                                                             animationTime);
            const float waveWidth = halfWidth * (0.90f + wave.crest * 0.24f);
            const float waveAmp = amplitude * (0.72f + wave.crest * 0.32f);
            const float phase = animationTime * speed + offset * 0.11f + wave.shimmer * 0.35f;

            drawWorldWaveRibbon(startX, startY, endX, endY,
                                waveWidth, waveAmp, frequency, phase,
                                withAlpha(outerColor, outerColor.a * (0.80f + wave.crest * 0.25f)));
            drawWorldWaveRibbon(startX, startY, endX, endY,
                                waveWidth * 0.38f, waveAmp * 0.52f, frequency + 0.65f,
                                phase + 0.55f,
                                withAlpha(innerColor, innerColor.a * (0.70f + wave.crest * 0.35f)));
        }
    };

    drawRibbonFamily(0.94f, 0.34f, 2.7f, 0.14f, 0.18f, 1.5f, 0.25f,
                     Color(0.16f, 0.52f, 0.64f, 0.040f),
                     Color(0.48f, 0.78f, 0.86f, 0.020f));
    drawRibbonFamily(-0.42f, 0.91f, 3.8f, 0.18f, 0.22f, 1.3f, 0.18f,
                     Color(0.08f, 0.30f, 0.42f, 0.030f),
                     Color(0.30f, 0.62f, 0.72f, 0.015f));
    drawRibbonFamily(0.72f, -0.62f, 5.2f, 0.08f, 0.12f, 1.8f, 0.31f,
                     Color(0.70f, 0.88f, 0.92f, 0.022f),
                     Color(0.92f, 0.98f, 1.0f, 0.010f));
    drawRibbonFamily(0.98f, 0.12f, 6.4f, 0.05f, 0.08f, 2.4f, 0.40f,
                     Color(0.78f, 0.92f, 0.96f, 0.015f),
                     Color(0.96f, 0.99f, 1.0f, 0.008f));

    if (currentRoute.visitOrder.size() > 1) {
        for (size_t i = 0; i + 1 < currentRoute.visitOrder.size(); ++i) {
            float fromX = 0.0f, fromY = 0.0f, toX = 0.0f, toY = 0.0f;
            if (!getRouteNodeWorldPos(graph, currentRoute.visitOrder[i], fromX, fromY) ||
                !getRouteNodeWorldPos(graph, currentRoute.visitOrder[i + 1], toX, toY)) {
                continue;
            }
            const float pulse = 0.5f + 0.5f *
                std::sin(animationTime * 0.72f + static_cast<float>(i) * 0.7f);
            drawWorldWaveRibbon(fromX, fromY, toX, toY,
                                0.34f, 0.08f, 1.2f,
                                animationTime * 0.60f + static_cast<float>(i) * 0.6f,
                                Color(0.18f, 0.52f, 0.58f, 0.018f + pulse * 0.010f));
        }
    }

    if (truck.isMoving() || currentRoute.isValid()) {
        drawOrganicWorldPatch(truck.getPosX(), truck.getPosY(),
                              1.1f, 0.72f, 0.62f, 0.24f,
                              animationTime, 8.4f,
                              Color(0.24f, 0.60f, 0.68f, truck.isMoving() ? 0.06f : 0.03f),
                              Color(0.70f, 0.90f, 0.96f, truck.isMoving() ? 0.02f : 0.01f));
    }

    for (const auto& node : graph.getNodes()) {
        const float capacityNorm = clamp01(node.getWasteCapacity() / 550.0f);
        const float tide = std::sin(animationTime * (0.52f + capacityNorm * 0.18f) +
                                    node.getWorldX() * 0.22f - node.getWorldY() * 0.15f);
        const float rotation = pseudoRandom01(node.getId(), 3, 881) * 2.0f *
                               static_cast<float>(M_PI);
        const float shelfX = node.getIsHQ() ? 6.2f : 2.2f + capacityNorm * 0.95f;
        const float shelfY = node.getIsHQ() ? 4.6f : 1.6f + capacityNorm * 0.70f;
        const float driftX = tide * (node.getIsHQ() ? 0.26f : 0.12f);
        const float driftY = std::cos(animationTime * 0.34f + node.getId() * 0.8f) *
                             (node.getIsHQ() ? 0.20f : 0.08f);
        const float baseAlpha = node.getIsHQ()
            ? 0.10f
            : 0.045f + capacityNorm * 0.014f;

        drawOrganicWorldPatch(node.getWorldX() + driftX, node.getWorldY() + driftY,
                              shelfX * 1.38f, shelfY * 1.34f,
                              rotation, 0.32f, animationTime, node.getId() * 0.91f,
                              Color(0.08f, 0.34f, 0.42f, baseAlpha),
                              Color(0.14f, 0.48f, 0.54f, baseAlpha * 0.30f));
        drawOrganicWorldPatch(node.getWorldX() - driftX * 0.35f,
                              node.getWorldY() - driftY * 0.28f,
                              shelfX * 0.96f, shelfY * 0.88f,
                              rotation + 0.44f, 0.28f, animationTime, node.getId() * 1.27f,
                              Color(0.16f, 0.54f, 0.60f, baseAlpha * 0.78f),
                              Color(0.30f, 0.70f, 0.72f, baseAlpha * 0.24f));
        drawOrganicWorldPatch(node.getWorldX() + std::cos(rotation) * shelfX * 0.22f +
                                  driftX * 0.12f,
                              node.getWorldY() + std::sin(rotation) * shelfY * 0.18f +
                                  driftY * 0.16f,
                              shelfX * 0.74f, shelfY * 0.42f,
                              rotation - 0.60f, 0.22f, animationTime, node.getId() * 1.91f,
                              Color(0.48f, 0.76f, 0.76f, baseAlpha * 0.34f),
                              Color(0.70f, 0.90f, 0.88f, baseAlpha * 0.08f));

        if (!node.isCollected() || node.getIsHQ()) {
            drawOrganicWorldPatch(node.getWorldX() + driftX * 0.20f,
                                  node.getWorldY() + driftY * 0.14f,
                                  shelfX * 0.50f, shelfY * 0.44f,
                                  rotation - 0.26f, 0.24f, animationTime, node.getId() * 1.63f,
                                  Color(0.82f, 0.94f, 0.88f, baseAlpha * 0.50f),
                                  Color(0.60f, 0.82f, 0.78f, baseAlpha * 0.12f));
        }
    }

    activeGraph = nullptr;
}

// =====================================================================
//  Water tile
// =====================================================================

void SeaThemeRenderer::drawWaterTile(IsometricRenderer& renderer,
                                      float cx, float cy, float w, float h,
                                      int gx, int gy, float animationTime) {
    const float worldX = static_cast<float>(gx) * kWaterSampleSpacing;
    const float worldY = static_cast<float>(gy) * kWaterSampleSpacing;
    const OceanWaveField wave = sampleOceanWaveField(worldX, worldY, animationTime);
    const float surfaceOffset = sampleOceanSurfaceOffset(worldX, worldY, animationTime);

    float shelf = 0.0f;
    if (activeGraph) {
        for (const auto& node : activeGraph->getNodes()) {
            shelf = std::max(shelf, getNodeShelfInfluence(node, worldX, worldY));
        }
    }

    const float spanX = std::max(1.0f, sceneBounds.maxX - sceneBounds.minX);
    const float spanY = std::max(1.0f, sceneBounds.maxY - sceneBounds.minY);
    const float basinCenterX = RenderUtils::lerp(sceneBounds.minX, sceneBounds.maxX, 0.56f);
    const float basinCenterY = RenderUtils::lerp(sceneBounds.minY, sceneBounds.maxY, 0.46f);
    const float basinDx = (worldX - basinCenterX) / (spanX * 0.44f + 0.6f);
    const float basinDy = (worldY - basinCenterY) / (spanY * 0.38f + 0.6f);
    const float basin = clamp01(
        (1.0f - RenderUtils::smoothstep(
            clamp01(std::sqrt(basinDx * basinDx + basinDy * basinDy)))) * 0.82f + 0.28f *
        (1.0f - RenderUtils::smoothstep(clamp01(std::sqrt(
            ((worldX - RenderUtils::lerp(sceneBounds.minX, sceneBounds.maxX, 0.42f)) /
             (spanX * 0.62f + 0.5f)) *
            ((worldX - RenderUtils::lerp(sceneBounds.minX, sceneBounds.maxX, 0.42f)) /
             (spanX * 0.62f + 0.5f)) +
            ((worldY - RenderUtils::lerp(sceneBounds.minY, sceneBounds.maxY, 0.62f)) /
             (spanY * 0.54f + 0.5f)) *
            ((worldY - RenderUtils::lerp(sceneBounds.minY, sceneBounds.maxY, 0.62f)) /
             (spanY * 0.54f + 0.5f)))))));

    const Color reflection = mixColor(Color(0.12f, 0.46f, 0.54f, 0.0f),
                                      Color(0.88f, 0.97f, 1.0f, 0.0f),
                                      0.42f + basin * 0.18f + shelf * 0.12f);
    const float baseAlpha = 0.010f + basin * 0.014f + shelf * 0.012f;

    renderer.drawDiamond(cx + wave.primary * w * 0.012f,
                cy - h * 0.10f + surfaceOffset * h * 0.12f,
                w * (0.82f + basin * 0.10f),
                h * 0.12f,
                withAlpha(reflection, baseAlpha));
    renderer.drawDiamond(cx - wave.secondary * w * 0.010f,
                cy + h * 0.02f + surfaceOffset * h * 0.08f,
                w * (0.56f + shelf * 0.20f),
                h * 0.10f,
                Color(0.78f, 0.93f, 0.94f, baseAlpha * (0.8f + wave.crest * 0.8f)));
}

// =====================================================================
//  Waste node — Boom Beach island + garbage patch
// =====================================================================

void SeaThemeRenderer::drawWasteNode(IsometricRenderer& renderer,
                                      const WasteNode& node,
                                      float animationTime) {
    IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
    const GarbageSizeTier sizeTier = getGarbageSizeTier(node.getWasteCapacity());
    const float zf = RenderUtils::getProjection().tileWidth / RenderUtils::BASE_TILE_WIDTH;
    const float scale = getGarbageScale(sizeTier) * 1.08f * zf;
    const float islandS = scale * 0.24f;
    const float markerS = scale * 0.42f;
    const float s = islandS;
    const float capacityNorm = clamp01(node.getWasteCapacity() / 550.0f);
    const int seed = node.getId();
    Color bodyColor = getGarbageSizeColor(sizeTier);
    Color accentColor = RenderUtils::getUrgencyColor(node.getUrgency());

    if (node.isCollected()) {
        bodyColor = mixColor(bodyColor, RenderUtils::getCollectedColor(), 0.68f);
        accentColor = shiftColor(RenderUtils::getCollectedColor(), 0.12f, 0.90f);
    } else if (node.getUrgency() == UrgencyLevel::HIGH) {
        const float pulse = RenderUtils::getUrgencyPulse(animationTime);
        accentColor.r = clampColor(accentColor.r * pulse);
        accentColor.g = clampColor(accentColor.g * pulse);
        accentColor.b = clampColor(accentColor.b * pulse);
    }

    const float bob = std::sin(animationTime * 1.0f + node.getWorldX() * 0.45f +
                               node.getWorldY() * 0.28f) * 1.0f;
    currentGarbageSinkProgress = getGarbageSinkState(node.getId());

    const float ix = iso.x - 12.0f * markerS;
    const float iy = iso.y - 36.0f * markerS;

    // ---- Boom Beach Island Palette ----
    const Color islandShadow(0.02f, 0.05f, 0.12f, node.isCollected() ? 0.28f : 0.22f);
    const Color beachTop(0.86f, 0.77f, 0.56f, 1.0f);
    const Color beachLight(0.93f, 0.84f, 0.64f, 1.0f);
    const Color cliffFace(0.70f, 0.56f, 0.38f, 1.0f);
    const Color cliffDark(0.50f, 0.38f, 0.24f, 1.0f);
    const Color canopyA(0.18f, 0.52f, 0.08f, 1.0f);
    const Color canopyB(0.28f, 0.66f, 0.12f, 1.0f);
    const Color canopyC(0.38f, 0.78f, 0.18f, 1.0f);
    const Color canopyD(0.46f, 0.84f, 0.24f, 1.0f);
    const Color trunkColor(0.46f, 0.32f, 0.18f, 1.0f);
    const Color rockTop(0.62f, 0.54f, 0.40f, 1.0f);
    const Color rockSide(0.42f, 0.35f, 0.24f, 1.0f);

    renderer.drawDiamond(ix, iy + 11.0f * s + bob * 0.16f,
                30.0f * s, 13.0f * s, islandShadow);

    {
        const int variant = positiveMod(seed * 7 + 3, 4);
        const float jx = pseudoRandomSigned(seed, 0, 101) * 0.5f;

        renderer.drawIsometricBlock(ix + jx * s, iy + 3.8f * s + bob * 0.10f,
                           22.0f * s, 13.0f * s, 4.0f * s, beachTop, cliffDark);

        if (variant == 0 || variant == 2) {
            renderer.drawIsometricBlock(ix + (jx - 10.0f) * s, iy + 2.0f * s + bob * 0.08f,
                               13.0f * s, 8.5f * s, 3.2f * s,
                               shiftColor(beachTop, -0.02f), shiftColor(cliffDark, -0.02f));
        }
        if (variant == 1 || variant == 3) {
            renderer.drawIsometricBlock(ix + (jx + 10.0f) * s, iy + 1.4f * s + bob * 0.08f,
                               12.0f * s, 7.5f * s, 2.8f * s, beachLight, cliffFace);
        }
        if (variant == 0 || variant == 1) {
            renderer.drawIsometricBlock(ix + (jx + 7.5f) * s, iy + 2.8f * s + bob * 0.07f,
                               9.0f * s, 6.0f * s, 2.2f * s, beachLight, cliffFace);
        } else {
            renderer.drawIsometricBlock(ix + (jx - 7.5f) * s, iy + 2.4f * s + bob * 0.07f,
                               10.0f * s, 6.5f * s, 2.4f * s,
                               shiftColor(beachTop, 0.02f), shiftColor(cliffDark, 0.02f));
        }
        renderer.drawIsometricBlock(ix + jx * 0.3f * s, iy + 0.2f * s + bob * 0.06f,
                           11.0f * s, 6.0f * s, 2.0f * s, beachLight, cliffFace);

        renderer.drawDiamond(ix + jx * 0.2f * s, iy + 5.6f * s + bob * 0.10f,
                    7.5f * s, 3.2f * s, beachLight);
        renderer.drawLine(ix - 8.5f * s, iy + 5.6f * s + bob * 0.10f,
                 ix - 1.5f * s, iy + 7.5f * s + bob * 0.10f,
                 Color(1.0f, 1.0f, 1.0f, 0.84f), 1.6f);
        renderer.drawLine(ix - 1.5f * s, iy + 7.5f * s + bob * 0.10f,
                 ix + 6.5f * s, iy + 5.0f * s + bob * 0.10f,
                 Color(1.0f, 1.0f, 1.0f, 0.78f), 1.4f);

        const int rockCount = 4 + static_cast<int>(capacityNorm * 4.0f);
        for (int r = 0; r < rockCount; ++r) {
            const float ang = static_cast<float>(r) * 6.283f /
                              static_cast<float>(rockCount) +
                              pseudoRandom01(seed, r, 333) * 0.5f;
            const float dist = (10.0f + pseudoRandom01(seed, r, 444) * 3.5f) * s;
            const float rx = ix + std::cos(ang) * dist * 0.80f;
            const float ry = iy + std::sin(ang) * dist * 0.40f + 4.0f * s;
            const float rSize = (1.6f + pseudoRandom01(seed, r, 555) * 1.2f) * s;
            renderer.drawIsometricBlock(rx, ry + bob * 0.06f,
                               rSize, rSize * 0.65f, 0.9f * s,
                               rockTop, rockSide);
        }

        const int backCount = 9 + static_cast<int>(capacityNorm * 5.0f);
        for (int c = 0; c < backCount; ++c) {
            const float t = static_cast<float>(c) / static_cast<float>(backCount);
            const float ang = t * 6.283f + pseudoRandom01(seed, c, 666) * 1.3f;
            const float radX = (4.5f + pseudoRandom01(seed, c, 777) * 6.0f) * s;
            const float radY = (2.2f + pseudoRandom01(seed, c, 888) * 3.5f) * s;
            const float cx = ix + std::cos(ang) * radX * 0.58f + jx * s * 0.15f;
            const float cy = iy + std::sin(ang) * radY * 0.38f - 5.0f * s + bob * 0.08f;
            const float radius = (3.2f + pseudoRandom01(seed, c, 999) * 3.2f) * s;
            const Color& canopy = (c % 4 == 0) ? canopyA :
                                  (c % 4 == 1) ? canopyB :
                                  (c % 4 == 2) ? canopyC : canopyD;
            renderer.drawFilledCircle(cx, cy, radius, canopy);
        }
        const int frontCount = backCount / 2 + 1;
        for (int c = 0; c < frontCount; ++c) {
            const float t = static_cast<float>(c) / static_cast<float>(frontCount);
            const float ang = t * 6.283f + pseudoRandom01(seed, c + 50, 612) * 1.5f;
            const float radX = (3.0f + pseudoRandom01(seed, c + 50, 723) * 4.5f) * s;
            const float radY = (1.4f + pseudoRandom01(seed, c + 50, 834) * 2.2f) * s;
            const float cx = ix + std::cos(ang) * radX * 0.50f + jx * s * 0.10f;
            const float cy = iy + std::sin(ang) * radY * 0.32f - 3.5f * s + bob * 0.08f;
            const float radius = (2.8f + pseudoRandom01(seed, c + 50, 945) * 2.8f) * s;
            const Color& canopy = ((c + 1) % 4 == 0) ? canopyA :
                                  ((c + 1) % 4 == 1) ? canopyB :
                                  ((c + 1) % 4 == 2) ? canopyC : canopyD;
            renderer.drawFilledCircle(cx, cy, radius, shiftColor(canopy, 0.04f));
        }

        const int palmCount = 2 + static_cast<int>(capacityNorm * 3.0f);
        for (int p = 0; p < palmCount; ++p) {
            const float spacing = 15.0f / static_cast<float>(palmCount);
            const float px = (-7.5f + static_cast<float>(p) * spacing +
                              pseudoRandomSigned(seed, p, 111) * 1.5f) * s;
            const float py = (-1.5f + pseudoRandomSigned(seed, p, 222) * 1.5f) * s;
            renderer.drawLine(ix + px, iy + py,
                     ix + px + 0.4f * s, iy + py - 6.5f * s,
                     trunkColor, 1.2f);
            renderer.drawFilledCircle(ix + px - 1.3f * s, iy + py - 6.8f * s,
                             2.2f * s, canopyC);
            renderer.drawFilledCircle(ix + px + 1.3f * s, iy + py - 7.2f * s,
                             2.0f * s, canopyB);
        }

        const float foamPulse = 0.5f + 0.5f *
            std::sin(animationTime * 0.8f + static_cast<float>(seed) * 0.7f);
        renderer.drawDiamondOutline(ix, iy + 9.0f * s,
                           31.0f * s * (0.98f + foamPulse * 0.02f),
                           13.5f * s * (0.98f + foamPulse * 0.02f),
                           Color(0.86f, 0.96f, 1.0f, 0.08f + foamPulse * 0.05f), 1.5f);
    }

    // ---- Garbage patch sprite ----
    const float fillRatio = node.isCollected()
        ? 0.0f
        : clamp01(node.getWasteLevel() / 100.0f);
    const float patchX = iso.x;
    const float patchY = iso.y - 2.0f + bob;
    const float patchScale = scale * 1.14f;
    drawGarbagePatchSprite(renderer, patchX, patchY, patchScale, bodyColor, accentColor,
                           node.isCollected(), fillRatio, animationTime);

    if (node.isCollected()) {
        const float settle = 0.4f + 0.6f * currentGarbageSinkProgress;
        renderer.drawDiamondOutline(iso.x, iso.y + 4.0f * markerS,
                           22.0f * markerS * (0.96f + settle * 0.04f),
                           10.0f * markerS * (0.96f + settle * 0.04f),
                           Color(0.72f, 0.96f, 0.86f, 0.12f + settle * 0.06f), 1.1f);
    }

    if (!node.isCollected()) {
        if (node.getUrgency() != UrgencyLevel::LOW) {
            float ringRadius = 16.0f * markerS;
            float ringHeight = 7.0f * markerS;
            float thickness = 1.2f;
            if (node.getUrgency() == UrgencyLevel::HIGH) {
                ringRadius += 1.5f * markerS * std::sin(animationTime * 2.4f);
                ringHeight += 0.7f * markerS * std::sin(animationTime * 2.4f);
                thickness = 1.6f;
            }
            Color ringColor = accentColor;
            ringColor.a = (node.getUrgency() == UrgencyLevel::HIGH) ? 0.22f : 0.13f;
            renderer.drawDiamondOutline(iso.x, iso.y + 2.0f * markerS + bob * 0.18f,
                               ringRadius, ringHeight, ringColor, thickness);
            const float braceX = ringRadius + 1.5f * markerS;
            const float braceY = ringHeight + 0.8f * markerS;
            renderer.drawLine(iso.x - braceX, iso.y + 2.0f * markerS + bob * 0.18f,
                     iso.x - braceX + 2.5f * markerS,
                     iso.y + 2.0f * markerS - braceY + bob * 0.18f,
                     withAlpha(ringColor, ringColor.a * 0.8f), 1.0f);
            renderer.drawLine(iso.x + braceX, iso.y + 2.0f * markerS + bob * 0.18f,
                     iso.x + braceX - 2.5f * markerS,
                     iso.y + 2.0f * markerS - braceY + bob * 0.18f,
                     withAlpha(ringColor, ringColor.a * 0.8f), 1.0f);
        }

        if (node.getUrgency() != UrgencyLevel::LOW) {
            const float beaconBob = std::sin(animationTime * 2.0f + node.getWorldX()) * 0.6f;
            const float bx = iso.x + 9.0f * markerS;
            const float by = iso.y - 8.0f * markerS + bob + beaconBob;
            renderer.drawLine(bx, by + 3.8f * markerS, bx, by - 0.8f * markerS,
                     Color(0.8f, 0.8f, 0.75f, 0.7f), 1.2f);
            renderer.drawFilledCircle(bx, by - 1.3f * markerS, 1.1f * markerS,
                             Color(accentColor.r, accentColor.g, accentColor.b,
                                   0.6f + 0.3f * std::sin(animationTime * 4.0f)));
        }
    }
}

// =====================================================================
//  HQ harbor island
// =====================================================================

void SeaThemeRenderer::drawHQNode(IsometricRenderer& renderer,
                                   const WasteNode& node,
                                   float animationTime) {
    IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
    const float zf = RenderUtils::getProjection().tileWidth / RenderUtils::BASE_TILE_WIDTH;
    const float s = 3.45f * zf;

    const float tide = std::sin(animationTime * 0.9f +
                                node.getWorldX() * 0.18f +
                                node.getWorldY() * 0.12f) * 0.5f;
    const float dockCx = iso.x;
    const float dockCy = iso.y + 2.0f * s + tide * 0.15f;
    const Color dockTop(0.60f, 0.47f, 0.30f, 0.98f);
    const Color dockSide(0.36f, 0.25f, 0.14f, 0.98f);
    const Color dockDark(0.27f, 0.19f, 0.12f, 0.95f);
    const Color dockShadow(0.02f, 0.05f, 0.12f, 0.22f);
    const Color foam(0.83f, 0.95f, 1.0f, 0.18f);
    const Color ropeColor(0.44f, 0.31f, 0.18f, 0.90f);
    const Color reefTop(0.09f, 0.32f, 0.40f, 0.34f);
    const Color reefGlow(0.16f, 0.48f, 0.56f, 0.22f);
    const Color duneTop(0.72f, 0.64f, 0.45f, 0.86f);
    const Color duneSide(0.52f, 0.43f, 0.30f, 0.82f);
    const Color breakwaterTop(0.43f, 0.41f, 0.38f, 0.88f);
    const Color breakwaterSide(0.28f, 0.27f, 0.25f, 0.84f);
    const Color lagoon(0.16f, 0.44f, 0.50f, 0.30f);
    const Color lagoonShine(0.82f, 0.96f, 1.0f, 0.12f);
    const Color steel(0.66f, 0.72f, 0.76f, 0.92f);

    const Color sandBase(0.68f, 0.60f, 0.42f, 0.65f);
    const Color sandSide(0.48f, 0.40f, 0.28f, 0.60f);
    renderer.drawDiamond(dockCx, dockCy + 14.2f * s, 28.0f * s, 12.6f * s, dockShadow);
    renderer.drawDiamond(dockCx + 2.0f * s, dockCy + 9.4f * s, 50.0f * s, 24.0f * s, reefTop);
    renderer.drawDiamond(dockCx + 1.5f * s, dockCy + 8.8f * s, 36.0f * s, 18.0f * s, reefGlow);
    renderer.drawDiamond(dockCx + 16.0f * s, dockCy + 10.0f * s, 22.0f * s, 8.6f * s, lagoon);
    renderer.drawDiamond(dockCx + 16.0f * s, dockCy + 9.2f * s, 15.5f * s, 5.4f * s, lagoonShine);
    renderer.drawIsometricBlock(dockCx, dockCy + 8.8f * s, 54.0f * s, 40.0f * s, 3.2f * s, sandBase, sandSide);
    renderer.drawIsometricBlock(dockCx - 6.5f * s, dockCy + 4.2f * s, 28.0f * s, 19.0f * s, 2.1f * s, duneTop, duneSide);
    renderer.drawIsometricBlock(dockCx - 19.0f * s, dockCy + 6.0f * s, 16.0f * s, 8.8f * s, 4.2f * s, breakwaterTop, breakwaterSide);
    renderer.drawIsometricBlock(dockCx + 20.0f * s, dockCy + 7.2f * s, 18.0f * s, 9.6f * s, 5.0f * s, breakwaterTop, breakwaterSide);
    renderer.drawIsometricBlock(dockCx + 14.2f * s, dockCy - 1.0f * s, 12.0f * s, 7.0f * s, 3.0f * s, breakwaterTop, breakwaterSide);

    const float shoreFoam = 0.5f + 0.5f * std::sin(animationTime * 0.8f);
    renderer.drawDiamondOutline(dockCx, dockCy + 9.6f * s,
                       56.0f * s * (0.98f + shoreFoam * 0.02f),
                       40.0f * s * (0.98f + shoreFoam * 0.02f),
                       Color(0.86f, 0.96f, 1.0f, 0.08f + shoreFoam * 0.05f), 1.6f);
    renderer.drawDiamondOutline(dockCx + 15.5f * s, dockCy + 9.6f * s,
                       27.0f * s, 11.2f * s,
                       Color(0.78f, 0.94f, 1.0f, 0.10f + shoreFoam * 0.04f), 1.3f);

    renderer.drawIsometricBlock(dockCx, dockCy, 38.0f * s, 28.0f * s, 6.0f * s, dockTop, dockSide);
    for (float offset = -0.34f; offset <= 0.34f; offset += 0.12f) {
        renderer.drawTileStripe(dockCx, dockCy - 3.8f * s, 16.0f * s, 11.0f * s,
                       true, offset, 0.42f,
                       Color(0.76f, 0.63f, 0.42f, 0.34f), 1.2f);
    }

    const std::array<IsoCoord, 4> pierTops = {{
        {dockCx - 12.0f * s, dockCy + 2.0f * s},
        {dockCx - 4.0f * s, dockCy + 5.0f * s},
        {dockCx + 5.5f * s, dockCy + 5.2f * s},
        {dockCx + 12.5f * s, dockCy + 2.4f * s}
    }};
    for (size_t i = 0; i < pierTops.size(); i++) {
        const float sway = std::sin(animationTime * 0.8f + static_cast<float>(i)) * 0.5f * s;
        renderer.drawLine(pierTops[i].x, pierTops[i].y - 1.0f * s,
                 pierTops[i].x + sway, pierTops[i].y + (13.5f + i * 1.3f) * s,
                 dockDark, 2.5f);
        renderer.drawFilledCircle(pierTops[i].x, pierTops[i].y - 1.2f * s, 1.8f * s,
                         Color(0.42f, 0.29f, 0.16f, 0.98f));
    }

    auto drawRope = [&](const IsoCoord& start, const IsoCoord& end, float sag) {
        const int segments = 12;
        for (int i = 0; i < segments; i++) {
            const float t0 = static_cast<float>(i) / segments;
            const float t1 = static_cast<float>(i + 1) / segments;
            auto sample = [&](float t) {
                const float x = RenderUtils::lerp(start.x, end.x, t);
                const float y = RenderUtils::lerp(start.y, end.y, t) +
                                sag * 4.0f * t * (1.0f - t);
                return IsoCoord{x, y};
            };
            const IsoCoord p0 = sample(t0);
            const IsoCoord p1 = sample(t1);
            renderer.drawLine(p0.x, p0.y, p1.x, p1.y, ropeColor, 1.5f);
        }
    };

    drawRope({dockCx - 12.0f * s, dockCy - 1.8f * s}, {dockCx - 4.0f * s, dockCy + 1.3f * s}, 4.8f * s);
    drawRope({dockCx + 5.5f * s, dockCy + 1.0f * s}, {dockCx + 12.5f * s, dockCy - 1.6f * s}, 4.2f * s);
    drawRope({dockCx + 11.0f * s, dockCy - 0.8f * s}, {dockCx + 18.0f * s, dockCy - 3.3f * s}, 3.8f * s);

    // Lighthouse
    const float beamAngle = animationTime * 1.35f;
    const float beamSpread = 0.44f;
    const IsoCoord beaconBase{dockCx + 9.5f * s, dockCy - 11.8f * s};
    renderer.drawIsometricBlock(beaconBase.x, beaconBase.y + 4.0f * s, 7.5f * s, 6.0f * s, 10.0f * s,
                       Color(0.88f, 0.89f, 0.84f, 1.0f), Color(0.63f, 0.64f, 0.60f, 1.0f));
    renderer.drawIsometricBlock(beaconBase.x, beaconBase.y - 1.4f * s, 4.0f * s, 3.4f * s, 4.0f * s,
                       Color(0.90f, 0.40f, 0.25f, 1.0f), Color(0.68f, 0.24f, 0.16f, 1.0f));
    const IsoCoord lightCenter{beaconBase.x, beaconBase.y - 6.0f * s};
    renderer.drawFilledCircle(lightCenter.x, lightCenter.y, 2.2f * s,
                     Color(1.0f, 0.92f, 0.68f, 0.95f));
    for (int beam = 0; beam < 2; beam++) {
        const float localAngle = beamAngle + beam * 0.42f;
        const IsoCoord rayA{
            lightCenter.x + std::cos(localAngle - beamSpread) * 38.0f * s,
            lightCenter.y + std::sin(localAngle - beamSpread) * 16.0f * s
        };
        const IsoCoord rayB{
            lightCenter.x + std::cos(localAngle + beamSpread) * 38.0f * s,
            lightCenter.y + std::sin(localAngle + beamSpread) * 16.0f * s
        };
        drawImmediateTriangle({lightCenter, rayA, rayB},
                              Color(1.0f, 0.96f, 0.72f, 0.08f));
    }

    // Storage shed
    const Color shedTop(0.52f, 0.42f, 0.30f, 0.95f);
    const Color shedSide(0.34f, 0.26f, 0.18f, 0.95f);
    renderer.drawIsometricBlock(dockCx - 8.0f * s, dockCy - 6.0f * s, 10.0f * s, 8.0f * s, 5.0f * s, shedTop, shedSide);
    renderer.drawIsometricBlock(dockCx + 4.8f * s, dockCy - 6.5f * s, 7.8f * s, 6.0f * s, 4.0f * s,
                       Color(0.74f, 0.78f, 0.82f, 0.92f), Color(0.48f, 0.52f, 0.56f, 0.92f));
    renderer.drawLine(dockCx + 1.0f * s, dockCy - 4.6f * s, dockCx + 7.8f * s, dockCy - 11.6f * s, steel, 1.7f);
    renderer.drawLine(dockCx + 6.8f * s, dockCy - 11.0f * s, dockCx + 12.2f * s, dockCy - 8.8f * s, steel, 1.4f);
    renderer.drawLine(dockCx + 8.2f * s, dockCy - 10.8f * s, dockCx + 8.2f * s, dockCy - 4.0f * s, steel, 1.3f);
    renderer.drawFilledCircle(dockCx + 12.0f * s, dockCy - 8.6f * s, 1.1f * s, Color(0.95f, 0.62f, 0.20f, 0.80f));

    const IsoCoord radarHub{dockCx + 8.4f * s, dockCy - 10.8f * s};
    const float radarSweep = animationTime * 0.72f;
    renderer.drawFilledCircle(radarHub.x, radarHub.y, 0.9f * s, Color(0.54f, 0.88f, 0.98f, 0.78f));
    renderer.drawDiamondOutline(radarHub.x, radarHub.y, 4.2f * s, 1.9f * s, Color(0.48f, 0.86f, 0.96f, 0.20f), 1.0f);
    const IsoCoord radarA{
        radarHub.x + std::cos(radarSweep - 0.22f) * 22.0f * s,
        radarHub.y + std::sin(radarSweep - 0.22f) * 10.0f * s
    };
    const IsoCoord radarB{
        radarHub.x + std::cos(radarSweep + 0.22f) * 22.0f * s,
        radarHub.y + std::sin(radarSweep + 0.22f) * 10.0f * s
    };
    drawImmediateTriangle({{radarHub, radarA, radarB}}, Color(0.58f, 0.90f, 0.98f, 0.08f));

    for (size_t light = 0; light < pierTops.size(); ++light) {
        renderer.drawFilledCircle(pierTops[light].x, pierTops[light].y - 2.0f * s, 0.75f * s,
                         Color(0.98f, 0.88f, 0.62f, 0.24f + light * 0.03f));
    }

    const float skiffBob = std::sin(animationTime * 1.6f + 0.8f) * 0.5f * s;
    renderer.drawDiamond(dockCx + 22.0f * s, dockCy + 16.2f * s + skiffBob, 7.4f * s, 3.0f * s, Color(0.02f, 0.06f, 0.12f, 0.18f));
    renderer.drawIsometricBlock(dockCx + 21.0f * s, dockCy + 11.8f * s + skiffBob, 8.6f * s, 4.0f * s, 1.2f * s,
                       Color(0.70f, 0.32f, 0.20f, 0.94f), Color(0.46f, 0.18f, 0.10f, 0.94f));
    renderer.drawLine(dockCx + 21.0f * s, dockCy + 8.8f * s + skiffBob, dockCx + 21.0f * s, dockCy + 3.8f * s + skiffBob,
             Color(0.78f, 0.78f, 0.74f, 0.72f), 1.1f);
    drawImmediateTriangle({{
        {dockCx + 21.0f * s, dockCy + 3.8f * s + skiffBob},
        {dockCx + 24.2f * s, dockCy + 6.0f * s + skiffBob},
        {dockCx + 21.0f * s, dockCy + 7.0f * s + skiffBob}
    }}, Color(0.92f, 0.94f, 0.90f, 0.72f));

    renderer.drawFilledCircle(dockCx - 14.0f * s, dockCy - 2.0f * s, 3.5f * s, Color(0.18f, 0.48f, 0.26f, 0.70f));
    renderer.drawFilledCircle(dockCx - 15.5f * s, dockCy + 1.0f * s, 2.8f * s, Color(0.22f, 0.52f, 0.30f, 0.65f));

    for (int lap = 0; lap < 4; lap++) {
        const float phase = animationTime * 1.3f + static_cast<float>(lap) * 0.65f;
        renderer.drawLine(dockCx + (-14.0f + lap * 8.8f) * s, dockCy + (12.0f + std::sin(phase) * 0.8f) * s,
                 dockCx + (-8.0f + lap * 8.8f) * s, dockCy + (13.5f + std::cos(phase) * 0.8f) * s,
                 foam, 1.2f);
    }
    renderer.drawDiamondOutline(dockCx, dockCy + 7.8f * s, 18.0f * s, 8.0f * s, Color(0.74f, 0.94f, 1.0f, 0.12f), 1.2f);
    renderer.drawDiamondOutline(dockCx + 15.4f * s, dockCy + 10.0f * s, 16.0f * s, 6.0f * s, Color(0.74f, 0.94f, 1.0f, 0.10f), 1.0f);

    for (int gull = 0; gull < 3; gull++) {
        const float orbit = animationTime * (0.55f + gull * 0.08f) + gull * 2.1f;
        const float gx = dockCx - 6.0f * s + std::cos(orbit) * (12.0f + gull * 3.0f) * s;
        const float gy = dockCy - 28.0f * s + std::sin(orbit * 1.2f) * (5.0f + gull) * s;
        const float wing = (2.0f + gull * 0.4f) * s;
        renderer.drawLine(gx - wing, gy, gx, gy - 1.2f * s, Color(0.94f, 0.96f, 0.98f, 0.70f), 1.0f);
        renderer.drawLine(gx, gy - 1.2f * s, gx + wing, gy, Color(0.94f, 0.96f, 0.98f, 0.70f), 1.0f);
    }

    renderer.drawRing(dockCx - 1.0f * s, dockCy - 5.6f * s, 5.2f * s, Color(0.18f, 0.55f, 0.88f, 0.70f), 2.0f);
    renderer.drawFilledCircle(dockCx - 1.0f * s, dockCy - 5.6f * s, 1.8f * s, Color(0.18f, 0.55f, 0.88f, 0.78f));
}

// =====================================================================
//  Truck — cargo boat
// =====================================================================

void SeaThemeRenderer::drawTruck(IsometricRenderer& renderer,
                                  const MapGraph& graph,
                                  const Truck& truck,
                                  const MissionPresentation* mission,
                                  float animationTime) {
    const RouteResult& currentRoute = routeFromMission(mission);
    if (!truck.isMoving() && !currentRoute.isValid()) return;

    IsoCoord iso = RenderUtils::worldToIso(truck.getPosX(), truck.getPosY());
    const OceanWaveField boatWave =
        sampleOceanWaveField(truck.getPosX(), truck.getPosY(), animationTime);
    float headingX = 1.0f;
    float headingY = 0.0f;
    getTruckHeading(graph, truck, currentRoute, headingX, headingY);
    float lateralX = -headingY;
    float lateralY = headingX;
    normalizeVector(lateralX, lateralY, 0.0f, 1.0f);
    const float bowSurface = sampleOceanSurfaceOffset(truck.getPosX() + headingX * 0.88f,
                                                      truck.getPosY() + headingY * 0.88f,
                                                      animationTime);
    const float sternSurface = sampleOceanSurfaceOffset(truck.getPosX() - headingX * 0.88f,
                                                        truck.getPosY() - headingY * 0.88f,
                                                        animationTime);
    const float portSurface = sampleOceanSurfaceOffset(truck.getPosX() + lateralX * 0.54f,
                                                       truck.getPosY() + lateralY * 0.54f,
                                                       animationTime);
    const float starboardSurface = sampleOceanSurfaceOffset(truck.getPosX() - lateralX * 0.54f,
                                                            truck.getPosY() - lateralY * 0.54f,
                                                            animationTime);
    boatCargoFillRatio = computeBoatCargoFill(graph, truck, currentRoute);
    boatPitchWave = bowSurface - sternSurface;
    boatRollWave = portSurface - starboardSurface;
    boatWakeStrength = truck.isMoving()
        ? 0.24f + boatWave.crest * 0.14f + std::abs(boatPitchWave) * 0.10f
        : 0.04f + boatWave.crest * 0.02f;
    boatIsMoving = truck.isMoving();
    const float boatSurfaceOffset =
        ((bowSurface + sternSurface) * 0.5f + boatWave.swell * 0.34f + boatWave.primary * 0.10f) * 4.0f;
    const float bobPhase = animationTime * (truck.isMoving() ? 1.9f : 1.35f) +
                           boatWave.combined * 1.8f;
    drawBoatSprite(renderer, iso.x, iso.y + 0.9f + boatSurfaceOffset, 4.4f, RenderUtils::getTruckColor(),
                   headingX, headingY, bobPhase, animationTime);
}

void SeaThemeRenderer::drawBoatSprite(IsometricRenderer& renderer,
                                       float cx, float cy, float scale,
                                       const Color& bodyColor,
                                       float headingX, float headingY,
                                       float bobPhase, float animationTime) {
    const float bobOffset = (std::sin(bobPhase) * 0.018f + boatPitchWave * 0.08f) * scale;
    const float rollOffset = (std::sin(bobPhase * 0.85f + animationTime * 1.6f) * 0.05f +
                              boatRollWave * 0.55f) * scale;
    const float pitchOffset = (boatIsMoving ? 0.03f : 0.014f) *
                              std::sin(animationTime * (boatIsMoving ? 4.8f : 1.2f) +
                                       bobPhase * 0.45f) + boatPitchWave * 0.10f;
    cy += bobOffset;

    const Color hullTop(0.78f, 0.28f, 0.16f, 0.99f);
    const Color hullLeft(0.53f, 0.16f, 0.09f, 0.99f);
    const Color hullRight(0.66f, 0.21f, 0.12f, 0.99f);
    const Color hullRear(0.46f, 0.12f, 0.07f, 0.99f);
    const Color trimTop(0.97f, 0.92f, 0.74f, 0.98f);
    const Color trimSide(0.82f, 0.74f, 0.56f, 0.98f);
    const Color deckTop(0.72f, 0.55f, 0.34f, 0.98f);
    const Color deckSide(0.46f, 0.30f, 0.16f, 0.98f);
    const Color cabinTop = shiftColor(bodyColor, 0.22f);
    const Color cabinLeft(bodyColor.r * 0.62f, bodyColor.g * 0.62f, bodyColor.b * 0.62f, 1.0f);
    const Color cabinRight(bodyColor.r * 0.78f, bodyColor.g * 0.78f, bodyColor.b * 0.78f, 1.0f);
    const Color cabinRear(bodyColor.r * 0.54f, bodyColor.g * 0.54f, bodyColor.b * 0.54f, 1.0f);
    const Color sailTop(0.97f, 0.95f, 0.86f, 0.98f);
    const Color sailShade(0.82f, 0.86f, 0.84f, 0.96f);
    const Color glass(0.38f, 0.74f, 0.94f, 0.94f);
    const Color rail(0.88f, 0.84f, 0.74f, 0.88f);
    const Color wakeColor(0.82f, 0.95f, 1.0f, 0.10f + boatWakeStrength * 0.12f);
    const Color foamColor(0.92f, 0.98f, 1.0f, 0.10f + boatWakeStrength * 0.14f);
    const Color cargoTop(0.18f, 0.18f, 0.16f, 0.94f);
    const Color cargoSide(0.10f, 0.10f, 0.09f, 0.92f);
    const auto& projection = RenderUtils::getProjection();

    float screenForwardX = (headingX - headingY) * (projection.tileWidth * 0.5f);
    float screenForwardY = (headingX + headingY) * (projection.tileHeight * 0.5f);
    normalizeVector(screenForwardX, screenForwardY, 0.92f, 0.38f);
    const float screenRightX = -screenForwardY;
    const float screenRightY = screenForwardX;

    renderer.drawDiamond(cx, cy + 6.6f * scale, 10.6f * scale, 4.2f * scale, Color(0.02f, 0.07f, 0.12f, 0.20f));
    renderer.drawDiamond(cx + screenForwardX * 1.2f,
                cy + 5.8f * scale + std::sin(animationTime * 1.7f + bobPhase) * 0.4f,
                9.2f * scale, 3.2f * scale,
                Color(0.54f, 0.84f, 0.94f, 0.06f + boatWakeStrength * 0.05f));
    renderer.drawDiamondOutline(cx, cy + 5.7f * scale,
                       11.2f * scale * (0.98f + boatWakeStrength * 0.03f),
                       4.0f * scale * (0.98f + boatWakeStrength * 0.04f),
                       Color(0.88f, 0.97f, 1.0f, 0.08f + boatWakeStrength * 0.06f), 1.0f);

    auto boatPoint = [&](float lateral, float longitudinal, float height = 0.0f) {
        const float rollLift = (lateral / 8.6f) * rollOffset;
        const float pitchLift = longitudinal * pitchOffset * scale * 0.11f;
        return IsoCoord{
            cx + screenRightX * lateral * scale + screenForwardX * longitudinal * scale,
            cy + screenRightY * lateral * scale + screenForwardY * longitudinal * scale -
                height * scale - rollLift - pitchLift
        };
    };

    auto quad = [&](const std::array<IsoCoord, 4>& pts, const Color& color) {
        drawImmediateQuad(pts, color);
    };
    auto tri = [&](const std::array<IsoCoord, 3>& pts, const Color& color) {
        drawImmediateTriangle(pts, color);
    };

    auto prism = [&](float left, float right,
                     float rear, float front,
                     float baseHeight, float topHeight,
                     const Color& topColor,
                     const Color& leftColor,
                     const Color& rightColor,
                     const Color& rearColor) {
        const IsoCoord bRearLeft = boatPoint(left, rear, baseHeight);
        const IsoCoord bRearRight = boatPoint(right, rear, baseHeight);
        const IsoCoord bFrontRight = boatPoint(right, front, baseHeight);
        const IsoCoord bFrontLeft = boatPoint(left, front, baseHeight);
        const IsoCoord tRearLeft = boatPoint(left, rear, topHeight);
        const IsoCoord tRearRight = boatPoint(right, rear, topHeight);
        const IsoCoord tFrontRight = boatPoint(right, front, topHeight);
        const IsoCoord tFrontLeft = boatPoint(left, front, topHeight);
        quad({bRearLeft, bRearRight, tRearRight, tRearLeft}, rearColor);
        quad({bRearRight, bFrontRight, tFrontRight, tRearRight}, rightColor);
        quad({bFrontLeft, bRearLeft, tRearLeft, tFrontLeft}, leftColor);
        quad({tRearLeft, tRearRight, tFrontRight, tFrontLeft}, topColor);
    };

    // Wake
    const IsoCoord sternWash = boatPoint(0.0f, -10.2f, 0.2f);
    const float wakeBase = 0.70f + boatWakeStrength * 0.90f;
    renderer.drawFilledCircle(sternWash.x, sternWash.y + 1.5f, 1.7f * scale * wakeBase, shiftColor(foamColor, 0.0f, 0.55f));
    renderer.drawRing(sternWash.x, sternWash.y + 1.5f, 2.6f * scale * wakeBase, shiftColor(foamColor, -0.04f, 0.48f), 0.9f);
    if (boatIsMoving) {
        for (int band = 0; band < 4; band++) {
            const float lateral = 1.6f + band * 1.7f;
            const float rear = 12.0f + band * 6.0f;
            const float alphaScale = 0.88f - band * 0.22f;
            renderer.drawLine(boatPoint(-1.6f, -8.0f, 0.2f).x, boatPoint(-1.6f, -8.0f, 0.2f).y,
                     boatPoint(-lateral, -rear, 0.0f).x, boatPoint(-lateral, -rear, 0.0f).y,
                     shiftColor(wakeColor, 0.01f * band, alphaScale), 1.2f - band * 0.2f);
            renderer.drawLine(boatPoint(1.6f, -8.0f, 0.2f).x, boatPoint(1.6f, -8.0f, 0.2f).y,
                     boatPoint(lateral, -rear, 0.0f).x, boatPoint(lateral, -rear, 0.0f).y,
                     shiftColor(wakeColor, 0.01f * band, alphaScale), 1.2f - band * 0.2f);
        }
        renderer.drawLine(sternWash.x, sternWash.y + 0.5f,
                 boatPoint(0.0f, -18.0f, 0.0f).x, boatPoint(0.0f, -18.0f, 0.0f).y,
                 shiftColor(wakeColor, -0.02f, 0.72f), 1.0f);
        renderer.drawLine(sternWash.x, sternWash.y + 1.2f,
                 boatPoint(0.0f, -24.0f, 0.0f).x, boatPoint(0.0f, -24.0f, 0.0f).y,
                 shiftColor(foamColor, -0.04f, 0.44f), 0.9f);
    }

    // Hull
    prism(-7.8f, 7.8f, -8.2f, 7.4f, 0.0f, 2.5f, hullTop, hullLeft, hullRight, hullRear);
    tri({boatPoint(0.0f, 12.0f, 1.4f), boatPoint(-5.4f, 7.4f, 2.5f), boatPoint(5.4f, 7.4f, 2.5f)}, hullTop);
    tri({boatPoint(0.0f, 12.0f, 0.0f), boatPoint(0.0f, 12.0f, 1.4f), boatPoint(-5.4f, 7.4f, 2.5f)}, hullLeft);
    tri({boatPoint(0.0f, 12.0f, 0.0f), boatPoint(5.4f, 7.4f, 2.5f), boatPoint(0.0f, 12.0f, 1.4f)}, hullRight);

    // Trim and deck
    prism(-7.1f, 7.1f, -7.4f, 6.8f, 2.5f, 3.2f, trimTop, trimSide, shiftColor(trimSide, 0.04f), shiftColor(trimSide, -0.08f));
    prism(-6.2f, 6.2f, -6.2f, 5.8f, 3.2f, 4.3f, deckTop, deckSide, shiftColor(deckSide, 0.06f), shiftColor(deckSide, -0.08f));
    prism(-5.6f, 5.6f, -5.6f, 0.8f, 4.3f, 6.4f,
          shiftColor(deckTop, -0.10f), shiftColor(deckSide, -0.08f),
          shiftColor(deckSide, -0.02f), shiftColor(deckSide, -0.12f));

    // Cargo
    const float cargoHeight = 4.38f + boatCargoFillRatio * 1.55f;
    if (boatCargoFillRatio > 0.02f) {
        prism(-4.8f, 4.8f, -4.9f, 0.1f, 4.32f, cargoHeight,
              cargoTop, cargoSide, shiftColor(cargoSide, 0.04f), shiftColor(cargoSide, -0.02f));
        for (int piece = 0; piece < 4; piece++) {
            const float px = -2.8f + piece * 1.8f;
            const float py = -4.1f + piece * 0.95f;
            const IsoCoord debris = boatPoint(px, py, cargoHeight + 0.08f + std::sin(animationTime * 1.6f + piece) * 0.08f);
            renderer.drawFilledCircle(debris.x, debris.y, (0.34f + piece * 0.05f) * scale, Color(0.12f, 0.11f, 0.09f, 0.92f));
        }
    }

    // Cabin
    prism(-4.1f, 4.1f, 2.7f, 7.0f, 4.3f, 8.6f, cabinTop, cabinLeft, cabinRight, cabinRear);
    prism(-2.9f, 2.9f, 4.8f, 7.2f, 6.0f, 8.2f, glass, shiftColor(glass, -0.12f), shiftColor(glass, -0.04f), shiftColor(glass, -0.20f));

    // Sails
    quad({boatPoint(-4.6f, 3.0f, 8.6f), boatPoint(4.6f, 3.0f, 8.6f),
          boatPoint(2.4f, 0.8f, 9.8f), boatPoint(-2.4f, 0.8f, 9.8f)}, sailTop);
    quad({boatPoint(-0.4f, 1.2f, 9.7f), boatPoint(2.4f, 1.1f, 9.6f),
          boatPoint(2.6f, -2.8f, 9.0f), boatPoint(-0.2f, -2.7f, 9.1f)}, sailShade);

    // Railing
    for (float pos = -4.4f; pos <= 4.4f; pos += 2.2f) {
        const IsoCoord leftBottom = boatPoint(-6.4f, pos, 4.3f);
        const IsoCoord leftTop = boatPoint(-6.4f, pos, 6.1f);
        const IsoCoord rightBottom = boatPoint(6.4f, pos, 4.3f);
        const IsoCoord rightTop = boatPoint(6.4f, pos, 6.1f);
        renderer.drawLine(leftBottom.x, leftBottom.y, leftTop.x, leftTop.y, rail, 1.0f);
        renderer.drawLine(rightBottom.x, rightBottom.y, rightTop.x, rightTop.y, rail, 1.0f);
    }
    renderer.drawLine(boatPoint(-6.4f, -4.4f, 6.1f).x, boatPoint(-6.4f, -4.4f, 6.1f).y,
             boatPoint(-6.4f, 4.4f, 6.1f).x, boatPoint(-6.4f, 4.4f, 6.1f).y, rail, 1.0f);
    renderer.drawLine(boatPoint(6.4f, -4.4f, 6.1f).x, boatPoint(6.4f, -4.4f, 6.1f).y,
             boatPoint(6.4f, 4.4f, 6.1f).x, boatPoint(6.4f, 4.4f, 6.1f).y, rail, 1.0f);

    // Mast and flag
    const IsoCoord mastBase = boatPoint(0.4f, 2.2f, 8.8f);
    const IsoCoord mastTop = boatPoint(0.4f, 2.2f, 12.8f);
    renderer.drawLine(mastBase.x, mastBase.y, mastTop.x, mastTop.y, Color(0.55f, 0.40f, 0.24f, 1.0f), 1.4f);
    const float flagSway = std::sin(animationTime * 3.4f + bobPhase * 0.6f) * 1.2f;
    const IsoCoord flagTip{
        mastTop.x + screenForwardX * (3.0f + flagSway * 0.4f) + screenRightX * (1.2f + flagSway * 0.3f),
        mastTop.y + screenForwardY * (3.0f + flagSway * 0.4f) + screenRightY * (1.2f + flagSway * 0.3f) + flagSway * 0.6f
    };
    const IsoCoord flagLow{
        mastTop.x + screenForwardX * 2.0f + screenRightX * 0.2f,
        mastTop.y + screenForwardY * 2.0f + screenRightY * 0.2f + 1.3f
    };
    tri({mastTop, flagTip, flagLow}, Color(0.95f, 0.84f, 0.22f, 0.96f));

    // Nav lights
    const IsoCoord bowLight = boatPoint(0.0f, 11.0f, 2.0f);
    const IsoCoord sternLeft = boatPoint(-3.8f, -7.4f, 3.5f);
    const IsoCoord sternRight = boatPoint(3.8f, -7.4f, 3.5f);
    renderer.drawFilledCircle(bowLight.x, bowLight.y, 0.65f * scale, Color(1.0f, 0.94f, 0.60f, 0.96f));
    renderer.drawFilledCircle(sternLeft.x, sternLeft.y, 0.50f * scale, Color(0.94f, 0.26f, 0.20f, 0.94f));
    renderer.drawFilledCircle(sternRight.x, sternRight.y, 0.50f * scale, Color(0.94f, 0.26f, 0.20f, 0.94f));

    // Waterline
    for (int lap = 0; lap < 5; lap++) {
        const float t = static_cast<float>(lap) / 4.0f;
        const float surge = std::sin(animationTime * 2.0f + t * 3.5f) * 0.26f;
        const IsoCoord leftWaterA = boatPoint(-7.6f, -6.4f + t * 12.0f, 0.8f);
        const IsoCoord leftWaterB = boatPoint(-8.7f, -6.0f + t * 11.0f, 0.2f);
        const IsoCoord rightWaterA = boatPoint(7.6f, -6.4f + t * 12.0f, 0.8f);
        const IsoCoord rightWaterB = boatPoint(8.7f, -6.0f + t * 11.0f, 0.2f);
        renderer.drawLine(leftWaterA.x, leftWaterA.y + surge, leftWaterB.x, leftWaterB.y - surge,
                 Color(0.82f, 0.95f, 1.0f, 0.16f), 1.0f);
        renderer.drawLine(rightWaterA.x, rightWaterA.y - surge, rightWaterB.x, rightWaterB.y + surge,
                 Color(0.82f, 0.95f, 1.0f, 0.16f), 1.0f);
    }
    renderer.drawDiamond(cx + screenForwardX * 1.2f, cy + 6.2f * scale, 9.2f * scale, 2.2f * scale,
                Color(0.88f, 0.97f, 1.0f, 0.05f));

    // Bow spray
    if (boatIsMoving) {
        for (int spray = 0; spray < 3; spray++) {
            const float phase = animationTime * 3.4f + spray * 0.8f;
            const IsoCoord particle = boatPoint(-1.4f + std::sin(phase) * 2.8f,
                                                9.8f + spray * 0.6f,
                                                0.45f + std::cos(phase) * 0.18f);
            renderer.drawFilledCircle(particle.x, particle.y, (0.14f + spray * 0.03f) * scale,
                             Color(0.96f, 0.99f, 1.0f, 0.14f + 0.04f * (2 - spray)));
        }
    }
}

// =====================================================================
//  Garbage patch sprite
// =====================================================================

void SeaThemeRenderer::drawGarbagePatchSprite(IsometricRenderer& renderer,
                                               float cx, float cy, float scale,
                                               const Color& bodyColor,
                                               const Color& accentColor,
                                               bool collected,
                                               float fillRatio,
                                               float animationTime) {
    const float sink = collected ? currentGarbageSinkProgress : 0.0f;
    const float alphaScale = clamp01(1.0f - sink * 0.88f);
    const float displayScale = scale * (1.0f - sink * 0.72f);
    const float displayFill = collected
        ? 0.42f * (1.0f - sink)
        : clamp01(fillRatio);
    const float bob = std::sin(animationTime * 1.35f + cx * 0.012f + cy * 0.017f) *
                      0.55f * displayScale;
    cy += bob + sink * 4.6f * scale;

    const Color waterShadow(0.02f, 0.06f, 0.14f, 0.28f * alphaScale);
    const Color foamColor(0.86f, 0.95f, 1.0f, 0.18f * alphaScale);
    const Color debrisBase = shiftColor(bodyColor, -0.04f, alphaScale);
    const Color debrisDark(bodyColor.r * 0.48f, bodyColor.g * 0.48f, bodyColor.b * 0.48f, 0.88f * alphaScale);
    const Color plasticWhite(0.90f, 0.92f, 0.88f, 0.72f * alphaScale);
    const Color plasticBlue(0.30f, 0.50f, 0.82f, 0.82f * alphaScale);
    const Color bottleGreen(0.26f, 0.56f, 0.42f, 0.72f * alphaScale);
    const float sheenMix = 0.5f + 0.5f * std::sin(animationTime * 0.9f + cx * 0.02f - cy * 0.03f);
    const Color sheen = mixColor(Color(0.82f, 0.35f, 0.72f, 0.24f * alphaScale),
                                 Color(0.24f, 0.78f, 0.88f, 0.24f * alphaScale), sheenMix);

    renderer.drawDiamond(cx, cy + 3.0f * displayScale, 10.8f * displayScale, 5.8f * displayScale, waterShadow);
    renderer.drawDiamond(cx + std::sin(animationTime * 1.1f) * 0.3f, cy + 1.2f * displayScale,
                8.2f * displayScale, 4.4f * displayScale,
                Color(0.08f, 0.20f, 0.36f, 0.18f * alphaScale));

    if (alphaScale <= 0.02f) return;

    const float foamPulse = 0.92f + 0.10f * std::sin(animationTime * 2.0f + cx * 0.04f);
    renderer.drawDiamondOutline(cx, cy + 0.9f * displayScale,
                       8.8f * displayScale * foamPulse, 4.6f * displayScale * foamPulse, foamColor, 1.2f);
    renderer.drawDiamondOutline(cx, cy + 1.0f * displayScale,
                       11.0f * displayScale * foamPulse, 5.4f * displayScale * foamPulse,
                       shiftColor(foamColor, -0.04f, 0.70f), 1.0f);

    const float patchW = 8.0f * displayScale * (0.88f + displayFill * 0.40f);
    const float patchH = 6.0f * displayScale * (0.90f + displayFill * 0.32f);
    const float patchDepth = (0.9f + displayFill * 1.7f) * displayScale;
    renderer.drawIsometricBlock(cx, cy + 0.8f * displayScale, patchW, patchH, patchDepth, debrisBase, debrisDark);
    renderer.drawDiamond(cx + 1.0f * displayScale, cy + 0.5f * displayScale,
                4.8f * displayScale, 2.2f * displayScale, sheen);

    auto rotatedQuad = [&](float ox, float oy, float halfW, float halfH,
                           float angle, const Color& color) {
        const float c = std::cos(angle);
        const float sn = std::sin(angle);
        auto pt = [&](float lx, float ly) {
            return IsoCoord{cx + ox + lx * c - ly * sn, cy + oy + lx * sn + ly * c};
        };
        drawImmediateQuad({pt(-halfW, -halfH), pt(halfW, -halfH),
                           pt(halfW, halfH), pt(-halfW, halfH)}, color);
    };

    for (int piece = 0; piece < 5; piece++) {
        const float driftX = std::sin(animationTime * (0.45f + piece * 0.08f) + piece * 1.7f) * (1.4f + piece * 0.18f) * displayScale;
        const float driftY = std::cos(animationTime * (0.52f + piece * 0.06f) + piece * 1.2f) * (0.9f + piece * 0.14f) * displayScale;
        const float angle = animationTime * (0.25f + piece * 0.05f) + piece * 0.7f;
        rotatedQuad(driftX, driftY - patchDepth * 0.35f,
                    (0.65f + piece * 0.08f) * displayScale,
                    (0.42f + piece * 0.05f) * displayScale,
                    angle, (piece % 2 == 0) ? debrisDark : shiftColor(debrisBase, 0.08f));
    }

    for (int bag = 0; bag < 2; bag++) {
        const float flap = std::sin(animationTime * (1.8f + bag * 0.3f) + bag * 2.0f);
        rotatedQuad((-2.2f + bag * 4.2f) * displayScale,
                    (-0.8f - bag * 0.9f) * displayScale - patchDepth * 0.35f,
                    (1.35f + bag * 0.15f) * displayScale,
                    (0.42f + flap * 0.10f) * displayScale,
                    animationTime * 0.8f + bag * 1.1f,
                    Color(0.92f, 0.94f, 0.98f, (0.34f + bag * 0.08f) * alphaScale));
    }

    for (int bottle = 0; bottle < 3; bottle++) {
        const float drift = std::sin(animationTime * (0.9f + bottle * 0.15f) + bottle) * 0.5f;
        const float tilt = 0.55f + std::sin(animationTime * 0.7f + bottle * 1.8f) * 0.35f;
        const float ox = (-2.8f + bottle * 2.7f) * displayScale;
        const float oy = (0.2f + drift + bottle * 0.4f) * displayScale - patchDepth * 0.28f;
        rotatedQuad(ox, oy, 0.30f * displayScale, 1.05f * displayScale, tilt,
                    (bottle == 1) ? plasticBlue : bottleGreen);
        renderer.drawFilledCircle(cx + ox + std::cos(tilt) * 0.55f * displayScale,
                         cy + oy + std::sin(tilt) * 0.55f * displayScale,
                         0.18f * displayScale, Color(0.94f, 0.98f, 1.0f, 0.76f * alphaScale));
    }

    renderer.drawFilledCircle(cx - 1.8f * displayScale, cy - patchDepth * 0.42f, 0.85f * displayScale, plasticWhite);
    renderer.drawFilledCircle(cx + 2.1f * displayScale, cy - patchDepth * 0.48f, 0.72f * displayScale, plasticBlue);
    renderer.drawDiamondOutline(cx, cy + 0.8f * displayScale,
                       patchW * 0.72f, patchH * 0.72f,
                       shiftColor(accentColor, 0.0f, 0.54f * alphaScale), 1.2f);
}

// =====================================================================
//  Decorative islets
// =====================================================================

void SeaThemeRenderer::drawDecorativeElements(IsometricRenderer& renderer,
                                               const MapGraph& graph,
                                               float animationTime) {
    drawDecorativeIslets(renderer, graph, animationTime);
}

void SeaThemeRenderer::drawDecorativeIslets(IsometricRenderer& renderer,
                                             const MapGraph& graph,
                                             float animationTime) {
    updateSceneBounds(graph);

    auto drawLargeCoastalIsland = [&](float wx, float wy, float scaleBias,
                                      bool lushNorth, float seedPhase) {
        const IsoCoord iso = RenderUtils::worldToIso(wx, wy);
        const float tide = std::sin(animationTime * 0.42f + seedPhase) * 0.6f;
        const float zf = RenderUtils::getProjection().tileWidth / RenderUtils::BASE_TILE_WIDTH;
        const float s = scaleBias * 2.35f * zf;
        const Color shadow(0.02f, 0.05f, 0.12f, 0.20f);
        const Color beachTop(0.87f, 0.77f, 0.56f, 1.0f);
        const Color beachTopLight(0.93f, 0.84f, 0.64f, 1.0f);
        const Color cliffFace(0.70f, 0.56f, 0.38f, 1.0f);
        const Color cliffDark(0.50f, 0.38f, 0.24f, 1.0f);
        const Color canopyA(0.36f, 0.77f, 0.18f, 1.0f);
        const Color canopyB(0.28f, 0.68f, 0.14f, 1.0f);
        const Color canopyC(0.42f, 0.84f, 0.24f, 1.0f);
        const Color trunk(0.46f, 0.32f, 0.18f, 1.0f);
        const Color rockTopC(0.62f, 0.54f, 0.40f, 1.0f);
        const Color rockSideC(0.42f, 0.35f, 0.24f, 1.0f);
        const float bob = std::sin(animationTime * 0.85f + seedPhase) * 0.25f * s;

        renderer.drawDiamond(iso.x - 2.0f * s, iso.y + 12.0f * s + bob, 28.0f * s, 12.0f * s, shadow);
        renderer.drawIsometricBlock(iso.x - 8.0f * s, iso.y + 3.8f * s + bob * 0.2f,
                           22.0f * s, 13.2f * s, 4.0f * s, beachTop, cliffDark);
        renderer.drawIsometricBlock(iso.x - 20.5f * s, iso.y + 1.2f * s + bob * 0.15f,
                           14.4f * s, 9.4f * s, 3.3f * s,
                           shiftColor(beachTop, -0.02f), shiftColor(cliffDark, -0.02f));
        renderer.drawIsometricBlock(iso.x + 10.5f * s, iso.y + 0.8f * s + bob * 0.10f,
                           11.0f * s, 7.2f * s, 2.6f * s, beachTopLight, cliffFace);
        renderer.drawIsometricBlock(iso.x - 2.0f * s, iso.y - 0.2f * s + bob * 0.08f,
                           10.8f * s, 6.0f * s, 1.8f * s, beachTopLight, cliffFace);

        renderer.drawDiamond(iso.x - 2.8f * s, iso.y + 5.4f * s + bob * 0.10f, 7.4f * s, 3.0f * s, beachTopLight);
        renderer.drawLine(iso.x - 8.4f * s, iso.y + 5.4f * s + bob * 0.10f,
                 iso.x - 2.8f * s, iso.y + 7.3f * s + bob * 0.10f,
                 Color(1.0f, 1.0f, 1.0f, 0.86f), 1.6f);
        renderer.drawLine(iso.x - 2.8f * s, iso.y + 7.3f * s + bob * 0.10f,
                 iso.x + 3.2f * s, iso.y + 5.0f * s + bob * 0.10f,
                 Color(1.0f, 1.0f, 1.0f, 0.82f), 1.5f);

        const std::array<IsoCoord, 7> rocks = {{
            {iso.x - 22.0f * s, iso.y + 4.0f * s},
            {iso.x - 16.0f * s, iso.y + 7.0f * s},
            {iso.x - 10.0f * s, iso.y + 8.2f * s},
            {iso.x + 3.0f * s, iso.y + 7.6f * s},
            {iso.x + 11.0f * s, iso.y + 5.8f * s},
            {iso.x + 17.0f * s, iso.y + 2.2f * s},
            {iso.x + 6.0f * s, iso.y + 0.6f * s}
        }};
        for (size_t i = 0; i < rocks.size(); ++i) {
            const float rockSize = (i % 2 == 0 ? 2.2f : 1.7f) * s;
            renderer.drawIsometricBlock(rocks[i].x, rocks[i].y + bob * 0.06f,
                               rockSize, rockSize * 0.68f, 0.9f * s, rockTopC, rockSideC);
        }

        const std::array<IsoCoord, 14> canopyCenters = lushNorth
            ? std::array<IsoCoord, 14>{{
                {iso.x - 20.0f * s, iso.y - 4.5f * s}, {iso.x - 14.5f * s, iso.y - 7.0f * s},
                {iso.x - 8.5f * s, iso.y - 8.8f * s}, {iso.x - 2.0f * s, iso.y - 9.6f * s},
                {iso.x + 4.8f * s, iso.y - 9.2f * s}, {iso.x + 10.5f * s, iso.y - 7.6f * s},
                {iso.x + 15.0f * s, iso.y - 4.8f * s}, {iso.x - 17.0f * s, iso.y - 1.0f * s},
                {iso.x - 10.5f * s, iso.y - 2.4f * s}, {iso.x - 4.0f * s, iso.y - 2.8f * s},
                {iso.x + 2.5f * s, iso.y - 2.2f * s}, {iso.x + 8.5f * s, iso.y - 1.4f * s},
                {iso.x - 7.0f * s, iso.y - 13.0f * s}, {iso.x + 1.0f * s, iso.y - 12.0f * s}
            }}
            : std::array<IsoCoord, 14>{{
                {iso.x - 19.0f * s, iso.y - 5.0f * s}, {iso.x - 13.0f * s, iso.y - 7.4f * s},
                {iso.x - 7.2f * s, iso.y - 9.0f * s}, {iso.x - 1.2f * s, iso.y - 9.5f * s},
                {iso.x + 5.2f * s, iso.y - 8.8f * s}, {iso.x + 11.0f * s, iso.y - 6.6f * s},
                {iso.x + 15.5f * s, iso.y - 3.2f * s}, {iso.x - 15.5f * s, iso.y - 0.8f * s},
                {iso.x - 9.0f * s, iso.y - 2.6f * s}, {iso.x - 2.5f * s, iso.y - 3.0f * s},
                {iso.x + 3.6f * s, iso.y - 2.4f * s}, {iso.x + 9.5f * s, iso.y - 1.6f * s},
                {iso.x - 5.5f * s, iso.y - 12.5f * s}, {iso.x + 2.4f * s, iso.y - 11.5f * s}
            }};
        const std::array<float, 14> canopyRadii = {{
            6.2f, 6.6f, 7.0f, 7.3f, 7.0f, 6.4f, 5.8f,
            5.9f, 6.3f, 6.2f, 5.8f, 5.2f, 4.8f, 4.4f
        }};

        for (size_t i = 0; i < canopyCenters.size(); ++i) {
            const Color canopy = (i % 3 == 0) ? canopyA : ((i % 3 == 1) ? canopyB : canopyC);
            renderer.drawFilledCircle(canopyCenters[i].x, canopyCenters[i].y + bob * 0.08f,
                             canopyRadii[i] * s, canopy);
        }

        for (int palm = 0; palm < 5; ++palm) {
            const float px = (-17.0f + palm * 8.8f) * s;
            const float py = (-1.0f - (palm % 2) * 2.2f) * s;
            renderer.drawLine(iso.x + px, iso.y + py, iso.x + px + 0.5f * s, iso.y + py - 6.2f * s, trunk, 1.1f);
            renderer.drawFilledCircle(iso.x + px - 1.2f * s, iso.y + py - 6.5f * s, 2.2f * s, canopyC);
            renderer.drawFilledCircle(iso.x + px + 1.2f * s, iso.y + py - 6.9f * s, 2.0f * s, canopyB);
        }
    };

    auto drawAmbientIslet = [&](float wx, float wy, int seed, float scaleBias,
                                int variantOverride = -1, float minNodeDistance = 4.8f) {
        float minDist = 1e9f;
        for (const auto& node : graph.getNodes()) {
            const float dx = wx - node.getWorldX();
            const float dy = wy - node.getWorldY();
            minDist = std::min(minDist, std::sqrt(dx * dx + dy * dy));
        }
        if (minDist < minNodeDistance) return;

        const IsoCoord iso = RenderUtils::worldToIso(wx, wy);
        const float zf = RenderUtils::getProjection().tileWidth / RenderUtils::BASE_TILE_WIDTH;
        const float isletScale = (0.90f + pseudoRandom01(seed, 2, 777) * 0.95f) * scaleBias * zf;
        const float tide = std::sin(animationTime * 0.45f + seed * 1.4f) * 0.4f;
        const int variant = (variantOverride >= 0)
            ? variantOverride
            : positiveMod(seed + static_cast<int>(pseudoRandom01(seed, 0, 811) * 9.0f), 5);

        renderer.drawDiamond(iso.x, iso.y + 3.5f * isletScale + tide * 0.15f,
                    9.0f * isletScale, 4.5f * isletScale, Color(0.02f, 0.06f, 0.14f, 0.20f));

        if (variant == 0) {
            const Color sandTop(0.74f, 0.66f, 0.48f, 0.80f);
            const Color sandSide(0.54f, 0.46f, 0.32f, 0.75f);
            renderer.drawIsometricBlock(iso.x, iso.y + 1.5f + tide * 0.1f,
                               7.0f * isletScale, 4.5f * isletScale, 1.8f * isletScale, sandTop, sandSide);
            renderer.drawFilledCircle(iso.x + 0.8f * isletScale, iso.y - 1.5f * isletScale + tide * 0.1f,
                             1.5f * isletScale, Color(0.20f, 0.50f, 0.28f, 0.68f));
        } else if (variant == 1) {
            const Color rockTopV(0.42f, 0.40f, 0.38f, 0.82f);
            const Color rockSideV(0.28f, 0.26f, 0.24f, 0.78f);
            renderer.drawIsometricBlock(iso.x, iso.y + 1.0f + tide * 0.08f,
                               5.0f * isletScale, 3.5f * isletScale, 2.5f * isletScale, rockTopV, rockSideV);
            renderer.drawFilledCircle(iso.x - 0.8f * isletScale, iso.y - 1.0f * isletScale,
                             0.9f * isletScale, Color(0.18f, 0.40f, 0.25f, 0.32f));
        } else if (variant == 2) {
            renderer.drawDiamond(iso.x, iso.y + 1.0f + tide * 0.12f,
                        8.0f * isletScale, 4.0f * isletScale, Color(0.08f, 0.30f, 0.40f, 0.35f));
            renderer.drawDiamond(iso.x, iso.y + 0.5f + tide * 0.1f,
                        5.5f * isletScale, 2.8f * isletScale, Color(0.12f, 0.38f, 0.44f, 0.28f));
        } else if (variant == 3) {
            renderer.drawDiamond(iso.x, iso.y + 0.8f + tide * 0.12f,
                        7.8f * isletScale, 3.8f * isletScale, Color(0.10f, 0.28f, 0.36f, 0.28f));
            renderer.drawDiamondOutline(iso.x, iso.y + 0.8f + tide * 0.10f,
                               8.8f * isletScale, 4.4f * isletScale, Color(0.70f, 0.90f, 0.92f, 0.08f), 0.9f);
            for (int buoy = 0; buoy < 3; buoy++) {
                const float angle = static_cast<float>(buoy) * 2.094f;
                const float bx = iso.x + std::cos(angle) * 4.0f * isletScale;
                const float by = iso.y + std::sin(angle) * 2.0f * isletScale;
                renderer.drawLine(bx, by + 2.0f * isletScale, bx, by - 0.3f * isletScale,
                         Color(0.76f, 0.78f, 0.72f, 0.52f), 0.9f);
                renderer.drawFilledCircle(bx, by - 0.5f * isletScale, 0.45f * isletScale,
                                 Color(0.92f, 0.64f, 0.24f, 0.58f));
            }
        } else {
            renderer.drawDiamond(iso.x + 0.4f * isletScale, iso.y + 1.4f * isletScale + tide * 0.12f,
                        9.4f * isletScale, 4.4f * isletScale, Color(0.08f, 0.26f, 0.34f, 0.22f));
            renderer.drawLine(iso.x - 5.4f * isletScale, iso.y - 1.6f * isletScale,
                     iso.x + 5.8f * isletScale, iso.y + 2.2f * isletScale,
                     Color(0.80f, 0.92f, 0.88f, 0.22f), 1.0f);
            renderer.drawLine(iso.x - 2.2f * isletScale, iso.y + 3.4f * isletScale,
                     iso.x + 3.8f * isletScale, iso.y + 1.0f * isletScale,
                     Color(0.80f, 0.92f, 0.88f, 0.18f), 1.0f);
            renderer.drawLine(iso.x - 4.6f * isletScale, iso.y - 3.0f * isletScale,
                     iso.x - 4.6f * isletScale, iso.y - 7.0f * isletScale,
                     Color(0.78f, 0.78f, 0.72f, 0.62f), 0.9f);
            renderer.drawFilledCircle(iso.x - 4.6f * isletScale, iso.y - 7.4f * isletScale,
                             0.55f * isletScale, Color(0.96f, 0.66f, 0.22f, 0.62f));
        }

        const float foamPulse = 0.5f + 0.5f * std::sin(animationTime * 0.9f + seed * 0.7f);
        renderer.drawDiamondOutline(iso.x, iso.y + 2.2f * isletScale + tide * 0.08f,
                           9.5f * isletScale * (0.97f + foamPulse * 0.03f),
                           4.8f * isletScale * (0.97f + foamPulse * 0.03f),
                           Color(0.86f, 0.96f, 1.0f, 0.06f + foamPulse * 0.05f), 1.0f);
    };

    for (int i = 0; i < kDecorIsletCount; i++) {
        const float rx = pseudoRandom01(i, 0, 777);
        const float ry = pseudoRandom01(i, 1, 777);
        const float wx = RenderUtils::lerp(sceneBounds.minX - 0.8f, sceneBounds.maxX + 0.8f, rx);
        const float wy = RenderUtils::lerp(sceneBounds.minY - 0.8f, sceneBounds.maxY + 0.8f, ry);
        drawAmbientIslet(wx, wy, i, 1.0f);
    }

    auto drawIsletNearNode = [&](int nodeId, float offsetX, float offsetY,
                                 float scaleBias, int seed, int variantOverride, float minNodeDistance) {
        const int idx = graph.findNodeIndex(nodeId);
        if (idx < 0) return;
        const WasteNode& node = graph.getNode(idx);
        drawAmbientIslet(node.getWorldX() + offsetX, node.getWorldY() + offsetY,
                         seed, scaleBias, variantOverride, minNodeDistance);
    };

    auto drawLargeIslandNearNode = [&](int nodeId, float offsetX, float offsetY,
                                       float scaleBias, bool lushNorth, float seedPhase) {
        const int idx = graph.findNodeIndex(nodeId);
        if (idx < 0) return;
        const WasteNode& node = graph.getNode(idx);
        drawLargeCoastalIsland(node.getWorldX() + offsetX, node.getWorldY() + offsetY,
                               scaleBias, lushNorth, seedPhase);
    };

    drawLargeIslandNearNode(1, -7.8f, -2.4f, 1.12f, true, 21.0f);
    drawLargeIslandNearNode(9, -7.2f,  2.8f, 1.24f, false, 24.0f);

    // Suppress unused lambda warnings
    (void)drawIsletNearNode;
}

// =====================================================================
//  Atmospheric effects
// =====================================================================

void SeaThemeRenderer::drawAtmosphericEffects(IsometricRenderer& renderer,
                                               const MapGraph& graph,
                                               float animationTime) {
    updateSceneBounds(graph);
    const float centerX = (sceneBounds.minX + sceneBounds.maxX) * 0.5f;
    const float centerY = (sceneBounds.minY + sceneBounds.maxY) * 0.5f;
    const IsoCoord centerIso = RenderUtils::worldToIso(centerX, centerY);
    const float paddedMinX = sceneBounds.minX - kFieldPadding;
    const float paddedMaxX = sceneBounds.maxX + kFieldPadding;
    const float paddedMinY = sceneBounds.minY - kFieldPadding;
    const float paddedMaxY = sceneBounds.maxY + kFieldPadding;

    for (int bird = 0; bird < 5; bird++) {
        const float orbitSpeed = 0.18f + bird * 0.04f;
        const float orbitRadius = 80.0f + bird * 35.0f;
        const float heightOffset = -60.0f - bird * 20.0f;
        const float angle = animationTime * orbitSpeed + bird * 1.26f;
        const float bx = centerIso.x + std::cos(angle) * orbitRadius;
        const float by = centerIso.y + std::sin(angle * 0.7f) * (orbitRadius * 0.4f) + heightOffset;
        const float wingFlap = std::sin(animationTime * 5.0f + bird * 2.0f);
        const float wingSpan = 2.5f + bird * 0.3f;
        const float wingY = wingFlap * 0.8f;
        const float alpha = 0.35f + 0.15f * std::sin(animationTime * 0.3f + bird);
        renderer.drawLine(bx - wingSpan, by + wingY, bx, by - 0.6f, Color(0.15f, 0.15f, 0.18f, alpha), 1.0f);
        renderer.drawLine(bx, by - 0.6f, bx + wingSpan, by + wingY, Color(0.15f, 0.15f, 0.18f, alpha), 1.0f);
    }

    for (const auto& node : graph.getNodes()) {
        if (node.isCollected() && !node.getIsHQ()) continue;
        const IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
        const float nodeScale = node.getIsHQ() ? 1.8f : 1.0f;
        const float foamPhase = animationTime * 0.6f + node.getWorldX() * 0.3f;
        const float foamAlpha = 0.025f + 0.02f * std::sin(foamPhase * 1.3f);
        for (int fm = 0; fm < 3; fm++) {
            const float localPhase = foamPhase + fm * 2.1f;
            const float ox = std::cos(localPhase) * (12.0f + fm * 3.0f) * nodeScale;
            const float oy = std::sin(localPhase * 1.3f) * (4.0f + fm * 1.1f) * nodeScale;
            renderer.drawLine(iso.x + ox - 3.2f * nodeScale, iso.y + oy + 5.4f * nodeScale,
                     iso.x + ox + 3.6f * nodeScale, iso.y + oy + 6.6f * nodeScale,
                     Color(0.80f, 0.94f, 1.0f, foamAlpha * (1.0f - fm * 0.18f)), 1.0f);
        }
    }

    for (int marker = 0; marker < 4; ++marker) {
        const float mx = RenderUtils::lerp(paddedMinX + 1.2f, paddedMaxX - 1.4f,
                                           pseudoRandom01(marker, 4, 933));
        const float my = RenderUtils::lerp(paddedMinY + 1.0f, paddedMaxY - 1.2f,
                                           pseudoRandom01(marker, 5, 933));
        float minDist = 1.0e9f;
        for (const auto& node : graph.getNodes()) {
            const float dx = mx - node.getWorldX();
            const float dy = my - node.getWorldY();
            minDist = std::min(minDist, std::sqrt(dx * dx + dy * dy));
        }
        if (minDist < 3.2f) continue;
        const IsoCoord mIso = RenderUtils::worldToIso(mx, my);
        const float mBob = std::sin(animationTime * 1.4f + marker * 2.3f) * 0.6f;
        renderer.drawDiamond(mIso.x, mIso.y + mBob + 2.0f, 3.6f, 1.4f, Color(0.62f, 0.88f, 0.94f, 0.05f));
    }

    for (int edge = 0; edge < 3; edge++) {
        const float wy = paddedMaxY + 2.5f + edge * 0.4f;
        const float waveShift = std::sin(animationTime * (0.15f + edge * 0.04f) + edge) * 0.5f;
        const IsoCoord a = RenderUtils::worldToIso(paddedMinX - 1.0f, wy + waveShift);
        const IsoCoord b = RenderUtils::worldToIso(paddedMaxX + 1.0f, wy + waveShift);
        renderer.drawLine(a.x, a.y, b.x, b.y, Color(0.50f, 0.80f, 0.92f, 0.03f + edge * 0.01f), 1.0f);
    }

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const float left = static_cast<float>(viewport[0]);
    const float top = static_cast<float>(viewport[1]);
    const float right = left + static_cast<float>(viewport[2]);
    const float bottom = top + static_cast<float>(viewport[3]);

    if (!graph.getNodes().empty()) {
        const WasteNode& hq = graph.getHQNode();
        drawOrganicWorldPatch(hq.getWorldX() + 0.6f, hq.getWorldY() + 0.8f,
                              5.0f, 3.8f, 0.28f, 0.24f, animationTime, 10.8f,
                              Color(0.98f, 0.93f, 0.78f, 0.045f), Color(0.80f, 0.84f, 0.76f, 0.012f));
        drawOrganicWorldPatch(hq.getWorldX() + 1.4f, hq.getWorldY() - 0.8f,
                              7.4f, 4.8f, -0.22f, 0.20f, animationTime, 14.1f,
                              Color(0.34f, 0.74f, 0.84f, 0.026f), Color(0.18f, 0.42f, 0.52f, 0.008f));
        drawOrganicWorldPatch(centerX, centerY,
                              (paddedMaxX - paddedMinX) * 0.20f, (paddedMaxY - paddedMinY) * 0.14f,
                              -0.16f, 0.18f, animationTime, 12.6f,
                              Color(0.56f, 0.78f, 0.84f, 0.024f), Color(0.34f, 0.58f, 0.68f, 0.010f));
    }

    glBegin(GL_QUADS);
    glColor4f(0.01f, 0.03f, 0.08f, 0.12f); glVertex2f(left, top);
    glColor4f(0.01f, 0.03f, 0.08f, 0.00f); glVertex2f(right, top);
    glColor4f(0.01f, 0.03f, 0.08f, 0.00f); glVertex2f(right, top + 140.0f);
    glColor4f(0.01f, 0.03f, 0.08f, 0.08f); glVertex2f(left, top + 140.0f);
    glEnd();

    glBegin(GL_QUADS);
    glColor4f(0.01f, 0.03f, 0.08f, 0.00f); glVertex2f(left, bottom - 120.0f);
    glColor4f(0.01f, 0.03f, 0.08f, 0.00f); glVertex2f(right, bottom - 120.0f);
    glColor4f(0.01f, 0.03f, 0.08f, 0.10f); glVertex2f(right, bottom);
    glColor4f(0.01f, 0.03f, 0.08f, 0.12f); glVertex2f(left, bottom);
    glEnd();

    drawWorldQuadPatch(paddedMinX - 4.5f, paddedMinY - 4.5f,
                       paddedMinX + 0.4f, paddedMaxY + 4.5f,
                       Color(0.01f, 0.03f, 0.08f, 0.07f));
    drawWorldQuadPatch(paddedMaxX - 0.4f, paddedMinY - 4.5f,
                       paddedMaxX + 4.5f, paddedMaxY + 4.5f,
                       Color(0.01f, 0.03f, 0.08f, 0.09f));
}
