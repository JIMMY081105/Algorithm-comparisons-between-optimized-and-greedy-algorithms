#include "visualization/IsometricRenderer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
constexpr float kFieldPadding = 3.4f;
constexpr float kWaterSampleSpacing = 1.55f;
constexpr float kDepotClearRadius = 0.68f;
constexpr float kClearInnerRadius = 0.20f;
constexpr float kClearOuterRadius = 0.56f;
constexpr int kAmbientParticleCount = 42;
constexpr int kDecorIsletCount = 16;

enum class GarbageSizeTier {
    SMALL,
    MEDIUM,
    LARGE
};

enum class NodeVisualStyle {
    SANDBAR,
    ROCKY,
    RAFT,
    BUOY_FIELD,
    REEF
};

struct SceneBounds {
    float minX;
    float maxX;
    float minY;
    float maxY;
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

const MapGraph* gActiveGraph = nullptr;
SceneBounds gSceneBounds{0.0f, 0.0f, 0.0f, 0.0f};
std::unordered_map<int, float> gGarbageSinkProgress;
float gCurrentGarbageSinkProgress = 0.0f;
float gBoatCargoFillRatio = 0.0f;
float gBoatWakeStrength = 0.0f;
float gBoatPitchWave = 0.0f;
float gBoatRollWave = 0.0f;
bool gBoatIsMoving = false;

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
    if (distance <= innerRadius) {
        return 1.0f;
    }
    if (distance >= outerRadius) {
        return 0.0f;
    }

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

NodeVisualStyle getNodeVisualStyle(const WasteNode& node) {
    const int capacityBucket =
        static_cast<int>(std::round(node.getWasteCapacity() / 70.0f));
    switch (positiveMod(node.getId() * 17 + capacityBucket * 3, 5)) {
        case 0: return NodeVisualStyle::SANDBAR;
        case 1: return NodeVisualStyle::ROCKY;
        case 2: return NodeVisualStyle::RAFT;
        case 3: return NodeVisualStyle::BUOY_FIELD;
        default: return NodeVisualStyle::REEF;
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
    if (length <= 0.0001f) {
        return;
    }

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
    if (length <= 0.0001f) {
        return;
    }

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

void updateSceneBounds(const MapGraph& graph) {
    const auto& nodes = graph.getNodes();
    if (nodes.empty()) {
        gSceneBounds = {0.0f, 0.0f, 0.0f, 0.0f};
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

    gSceneBounds = {minWorldX, maxWorldX, minWorldY, maxWorldY};
}

void updateGarbageSinkState(const MapGraph& graph, float deltaTime) {
    std::unordered_map<int, float> nextState;
    nextState.reserve(graph.getNodeCount());

    for (const auto& node : graph.getNodes()) {
        float progress = 0.0f;
        const auto it = gGarbageSinkProgress.find(node.getId());
        if (it != gGarbageSinkProgress.end()) {
            progress = it->second;
        }

        if (node.isCollected()) {
            progress = clamp01(progress + deltaTime * 1.75f);
        } else {
            progress = clamp01(progress - deltaTime * 3.50f);
        }

        nextState[node.getId()] = progress;
    }

    gGarbageSinkProgress.swap(nextState);
}

float getGarbageSinkState(int nodeId) {
    const auto it = gGarbageSinkProgress.find(nodeId);
    return (it != gGarbageSinkProgress.end()) ? it->second : 0.0f;
}

float getNodeProximity(float worldX, float worldY) {
    if (!gActiveGraph) return 0.0f;

    float minDistSq = 1.0e9f;
    for (const auto& node : gActiveGraph->getNodes()) {
        const float dx = worldX - node.getWorldX();
        const float dy = worldY - node.getWorldY();
        minDistSq = std::min(minDistSq, dx * dx + dy * dy);
    }

    const float minDist = std::sqrt(minDistSq);
    return 1.0f - RenderUtils::smoothstep(clamp01(minDist / 3.5f));
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

float getShallowWaterFactor(float worldX, float worldY) {
    if (!gActiveGraph) return 0.0f;

    float shelf = 0.0f;
    for (const auto& node : gActiveGraph->getNodes()) {
        shelf = std::max(shelf, getNodeShelfInfluence(node, worldX, worldY));
    }
    return shelf;
}

float getDeepBasinFactor(float worldX, float worldY) {
    const float spanX = std::max(1.0f, gSceneBounds.maxX - gSceneBounds.minX);
    const float spanY = std::max(1.0f, gSceneBounds.maxY - gSceneBounds.minY);

    const float basinCenterX = RenderUtils::lerp(gSceneBounds.minX, gSceneBounds.maxX, 0.56f);
    const float basinCenterY = RenderUtils::lerp(gSceneBounds.minY, gSceneBounds.maxY, 0.46f);
    const float basinDx = (worldX - basinCenterX) / (spanX * 0.44f + 0.6f);
    const float basinDy = (worldY - basinCenterY) / (spanY * 0.38f + 0.6f);
    const float primary = 1.0f - RenderUtils::smoothstep(
        clamp01(std::sqrt(basinDx * basinDx + basinDy * basinDy)));

    const float driftCenterX = RenderUtils::lerp(gSceneBounds.minX, gSceneBounds.maxX, 0.42f);
    const float driftCenterY = RenderUtils::lerp(gSceneBounds.minY, gSceneBounds.maxY, 0.62f);
    const float driftDx = (worldX - driftCenterX) / (spanX * 0.62f + 0.5f);
    const float driftDy = (worldY - driftCenterY) / (spanY * 0.54f + 0.5f);
    const float secondary = 1.0f - RenderUtils::smoothstep(
        clamp01(std::sqrt(driftDx * driftDx + driftDy * driftDy)));

    return clamp01(primary * 0.82f + secondary * 0.28f);
}

float getCenterDepthFactor(float worldX, float worldY) {
    const float centerX = (gSceneBounds.minX + gSceneBounds.maxX) * 0.5f;
    const float centerY = (gSceneBounds.minY + gSceneBounds.maxY) * 0.5f;
    const float spanX = std::max(1.0f, gSceneBounds.maxX - gSceneBounds.minX);
    const float spanY = std::max(1.0f, gSceneBounds.maxY - gSceneBounds.minY);
    const float dx = (worldX - centerX) / (spanX * 0.5f);
    const float dy = (worldY - centerY) / (spanY * 0.5f);
    const float dist = std::sqrt(dx * dx + dy * dy);
    return 1.0f - clamp01(dist);
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

    if (!currentRoute.isValid() || currentRoute.visitOrder.size() < 2) {
        return;
    }

    const int lastSegment = static_cast<int>(currentRoute.visitOrder.size()) - 2;
    if (lastSegment < 0) {
        return;
    }

    int seg = std::max(0, std::min(truck.getCurrentSegment(), lastSegment));
    float fromX = 0.0f;
    float fromY = 0.0f;
    float toX = 0.0f;
    float toY = 0.0f;

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
    if (!currentRoute.isValid() || currentRoute.visitOrder.size() < 2) {
        return 0.0f;
    }

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

    if (totalWaste <= 0.001f) {
        return 0.0f;
    }

    return clamp01(collectedWaste / totalWaste);
}

float getClearingAmount(float worldX, float worldY, const MapGraph& graph,
                        const Truck& truck, const RouteResult& currentRoute) {
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
        float ax = 0.0f;
        float ay = 0.0f;
        float bx = 0.0f;
        float by = 0.0f;
        if (!getRouteNodeWorldPos(graph, currentRoute.visitOrder[i], ax, ay) ||
            !getRouteNodeWorldPos(graph, currentRoute.visitOrder[i + 1], bx, by)) {
            continue;
        }

        minDistance = std::min(minDistance,
                               distancePointToSegment(worldX, worldY, ax, ay, bx, by));
    }

    if (truck.getCurrentSegment() >= 0 &&
        truck.getCurrentSegment() < static_cast<int>(currentRoute.visitOrder.size())) {
        float startX = 0.0f;
        float startY = 0.0f;
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
//  Constructor & main render
// =====================================================================

IsometricRenderer::IsometricRenderer()
    : animationTime(0.0f), routeDrawProgress(0), showGrid(true) {}

void IsometricRenderer::render(const MapGraph& graph, const Truck& truck,
                                const RouteResult& currentRoute, float deltaTime) {
    animationTime += deltaTime;
    gActiveGraph = &graph;
    updateSceneBounds(graph);
    updateGarbageSinkState(graph, deltaTime);

    drawGroundPlane(graph, truck, currentRoute);
    drawDecorativeIslets(graph);

    if (!currentRoute.visitOrder.empty()) {
        drawRouteHighlight(graph, currentRoute, routeDrawProgress);
    }

    for (int i = 0; i < graph.getNodeCount(); i++) {
        const WasteNode& node = graph.getNode(i);
        if (node.getIsHQ()) {
            drawHQNode(node);
        } else {
            drawWasteNode(node, animationTime);
        }
    }

    if (truck.isMoving() || currentRoute.isValid()) {
        drawTruck(graph, truck, currentRoute);
    }

    drawAtmosphericEffects(graph);

    gActiveGraph = nullptr;
}

// =====================================================================
//  Ground plane — water tiles + world composition
// =====================================================================

void IsometricRenderer::drawGroundPlane(const MapGraph& graph, const Truck& truck,
                                         const RouteResult& currentRoute) {
    if (graph.getNodes().empty()) return;

    const float paddedMinX = gSceneBounds.minX - kFieldPadding - 2.5f;
    const float paddedMaxX = gSceneBounds.maxX + kFieldPadding + 2.5f;
    const float paddedMinY = gSceneBounds.minY - kFieldPadding - 2.5f;
    const float paddedMaxY = gSceneBounds.maxY + kFieldPadding + 2.5f;
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
    glColor4f(0.05f, 0.28f, 0.39f, 1.0f);
    glVertex2f(right, top);
    glColor4f(0.05f, 0.28f, 0.39f, 1.0f);
    glVertex2f(right, bottom);
    glColor4f(0.05f, 0.28f, 0.39f, 1.0f);
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
            float fromX = 0.0f;
            float fromY = 0.0f;
            float toX = 0.0f;
            float toY = 0.0f;
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
        const float shelfX = node.getIsHQ()
            ? 6.2f
            : 2.2f + capacityNorm * 0.95f;
        const float shelfY = node.getIsHQ()
            ? 4.6f
            : 1.6f + capacityNorm * 0.70f;
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

    // Per-node shallow water shelf halos — lighten the shader ocean near islands
    if (false) for (const auto& node : graph.getNodes()) {
        const float capacityNorm = clamp01(node.getWasteCapacity() / 550.0f);
        const float tide = std::sin(animationTime * (0.55f + capacityNorm * 0.18f) +
                                    node.getWorldX() * 0.22f - node.getWorldY() * 0.16f);
        const float radiusX = node.getIsHQ()
            ? 8.8f
            : 3.1f + capacityNorm * 1.6f;
        const float radiusY = node.getIsHQ()
            ? 6.4f
            : 2.4f + capacityNorm * 1.1f;
        const float driftX = tide * (node.getIsHQ() ? 0.22f : 0.12f);
        const float driftY = std::cos(animationTime * 0.42f + node.getId()) *
                             (node.getIsHQ() ? 0.16f : 0.08f);
        const float shelfAlpha = node.getIsHQ() ? 0.18f : 0.10f + capacityNorm * 0.03f;

        // Outer shelf — subtle lightening
        drawWorldDiamondPatch(node.getWorldX() + driftX, node.getWorldY() + driftY,
                              radiusX * 1.22f, radiusY * 1.16f,
                              Color(0.25f, 0.62f, 0.58f, shelfAlpha));
        // Middle shelf
        drawWorldDiamondPatch(node.getWorldX() - driftX * 0.35f,
                              node.getWorldY() - driftY * 0.45f,
                              radiusX * 0.98f, radiusY * 0.74f,
                              Color(0.40f, 0.72f, 0.68f, shelfAlpha * 0.50f));
        // Inner glow
        drawWorldDiamondPatch(node.getWorldX() - driftX * 0.7f,
                              node.getWorldY() + driftY * 0.5f,
                              radiusX * 0.68f, radiusY * 0.54f,
                              Color(0.65f, 0.90f, 0.85f, shelfAlpha * 0.40f));
        // Bright core highlight
        if (!node.isCollected() || node.getIsHQ()) {
            drawWorldDiamondPatch(node.getWorldX() + driftX * 0.5f,
                                  node.getWorldY() - driftY * 0.4f,
                                  radiusX * 0.44f, radiusY * 0.28f,
                                  Color(0.85f, 0.97f, 0.92f,
                                        (node.getIsHQ() ? 0.08f : 0.04f) +
                                        0.03f * capacityNorm));
        }
    }
}

// =====================================================================
//  Water tile — premium ocean rendering
// =====================================================================

void IsometricRenderer::drawWaterTile(float cx, float cy, float w, float h,
                                       int gx, int gy) {
    const float worldX = static_cast<float>(gx) * kWaterSampleSpacing;
    const float worldY = static_cast<float>(gy) * kWaterSampleSpacing;
    const OceanWaveField wave = sampleOceanWaveField(worldX, worldY, animationTime);
    const float shelf = getShallowWaterFactor(worldX, worldY);
    const float basin = getDeepBasinFactor(worldX, worldY);
    const float surfaceOffset = sampleOceanSurfaceOffset(worldX, worldY, animationTime);
    const Color reflection = mixColor(Color(0.12f, 0.46f, 0.54f, 0.0f),
                                      Color(0.88f, 0.97f, 1.0f, 0.0f),
                                      0.42f + basin * 0.18f + shelf * 0.12f);
    const float baseAlpha = 0.010f + basin * 0.014f + shelf * 0.012f;

    drawDiamond(cx + wave.primary * w * 0.012f,
                cy - h * 0.10f + surfaceOffset * h * 0.12f,
                w * (0.82f + basin * 0.10f),
                h * 0.12f,
                withAlpha(reflection, baseAlpha));
    drawDiamond(cx - wave.secondary * w * 0.010f,
                cy + h * 0.02f + surfaceOffset * h * 0.08f,
                w * (0.56f + shelf * 0.20f),
                h * 0.10f,
                Color(0.78f, 0.93f, 0.94f, baseAlpha * (0.8f + wave.crest * 0.8f)));
}

// =====================================================================
//  Road connections
// =====================================================================

void IsometricRenderer::drawRoadConnections(const MapGraph& graph) {
    Color roadColor = RenderUtils::getRoadColor();
    float maxVisibleDist = 48.75f;

    for (int i = 0; i < graph.getNodeCount(); i++) {
        for (int j = i + 1; j < graph.getNodeCount(); j++) {
            float dist = graph.getAdjacencyMatrix()[i][j];
            if (dist < maxVisibleDist) {
                const WasteNode& a = graph.getNode(i);
                const WasteNode& b = graph.getNode(j);
                IsoCoord isoA = RenderUtils::worldToIso(a.getWorldX(), a.getWorldY());
                IsoCoord isoB = RenderUtils::worldToIso(b.getWorldX(), b.getWorldY());

                float alpha = 1.0f - (dist / maxVisibleDist);
                Color fadedRoad = roadColor;
                fadedRoad.a *= alpha;
                drawLine(isoA.x, isoA.y, isoB.x, isoB.y, fadedRoad, 1.5f);
            }
        }
    }
}

// =====================================================================
//  Route highlight — layered premium path rendering
// =====================================================================

void IsometricRenderer::drawRouteHighlight(const MapGraph& graph,
                                            const RouteResult& route,
                                            int segmentsToShow) {
    if (route.visitOrder.size() < 2) return;

    const int maxSegments = static_cast<int>(route.visitOrder.size()) - 1;
    const int drawCount = (segmentsToShow <= 0) ? maxSegments
                    : std::min(segmentsToShow, maxSegments);
    const int focusSegment = std::max(0, drawCount - 1);

    auto drawWaypointMarker = [&](const WasteNode& node, bool isCurrent, bool isServiced) {
        const IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
        const float baseScale = node.getIsHQ() ? 1.20f : 1.0f;
        const float pulse = 0.5f + 0.5f *
            std::sin(animationTime * (isCurrent ? 2.8f : 1.4f) + node.getId() * 0.6f);
        const Color markerColor = node.getIsHQ()
            ? Color(0.42f, 0.72f, 0.94f, 0.30f + pulse * 0.06f)
            : isServiced
                ? Color(0.48f, 0.86f, 0.72f, 0.20f + pulse * 0.06f)
                : isCurrent
                    ? Color(0.76f, 0.98f, 1.0f, 0.26f + pulse * 0.10f)
                    : Color(0.54f, 0.86f, 0.84f, 0.16f);
        const Color coreColor = node.getIsHQ()
            ? Color(0.82f, 0.94f, 1.0f, 0.44f)
            : isServiced
                ? Color(0.76f, 0.98f, 0.88f, 0.30f)
                : isCurrent
                    ? Color(0.94f, 1.0f, 1.0f, 0.44f)
                    : Color(0.82f, 0.96f, 0.94f, 0.24f);
        const float rx = (node.getIsHQ() ? 5.2f : 4.0f) * baseScale;
        const float ry = (node.getIsHQ() ? 2.6f : 2.1f) * baseScale;

        drawDiamond(iso.x, iso.y + 0.1f, rx + 1.8f, ry + 1.0f,
                    Color(0.04f, 0.18f, 0.22f, 0.12f + pulse * 0.04f));
        drawDiamondOutline(iso.x, iso.y, rx, ry, markerColor, isCurrent ? 1.6f : 1.1f);
        drawDiamond(iso.x, iso.y, rx * 0.34f, ry * 0.34f, coreColor);

        const float bracketReach = rx + 1.6f + pulse * (isCurrent ? 0.9f : 0.3f);
        const float bracketLift = ry + 0.8f;
        drawLine(iso.x - bracketReach, iso.y,
                 iso.x - bracketReach + 2.4f, iso.y - bracketLift,
                 withAlpha(markerColor, markerColor.a * 0.9f), 1.0f);
        drawLine(iso.x + bracketReach, iso.y,
                 iso.x + bracketReach - 2.4f, iso.y - bracketLift,
                 withAlpha(markerColor, markerColor.a * 0.9f), 1.0f);
        drawLine(iso.x - bracketReach, iso.y,
                 iso.x - bracketReach + 2.2f, iso.y + bracketLift * 0.8f,
                 withAlpha(markerColor, markerColor.a * 0.7f), 1.0f);
        drawLine(iso.x + bracketReach, iso.y,
                 iso.x + bracketReach - 2.2f, iso.y + bracketLift * 0.8f,
                 withAlpha(markerColor, markerColor.a * 0.7f), 1.0f);
    };

    // Layer 1: Soft in-water underglow
    for (int i = 0; i < drawCount; i++) {
        const int fromIdx = graph.findNodeIndex(route.visitOrder[i]);
        const int toIdx = graph.findNodeIndex(route.visitOrder[i + 1]);
        if (fromIdx < 0 || toIdx < 0) continue;

        const WasteNode& from = graph.getNode(fromIdx);
        const WasteNode& to = graph.getNode(toIdx);
        const IsoCoord isoFrom = RenderUtils::worldToIso(from.getWorldX(), from.getWorldY());
        const IsoCoord isoTo = RenderUtils::worldToIso(to.getWorldX(), to.getWorldY());
        const float recency =
            1.0f - clamp01(std::abs(static_cast<float>(i - focusSegment)) / 2.6f);

        drawLine(isoFrom.x, isoFrom.y + 1.0f, isoTo.x, isoTo.y + 1.0f,
                 Color(0.03f, 0.16f, 0.22f, 0.08f + recency * 0.05f),
                 7.2f + recency * 1.6f);
        drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                 Color(0.08f, 0.38f, 0.42f, 0.04f + recency * 0.03f),
                 4.4f + recency * 0.8f);
    }

    // Layer 2: Quiet readable route core
    for (int i = 0; i < drawCount; i++) {
        const int fromIdx = graph.findNodeIndex(route.visitOrder[i]);
        const int toIdx = graph.findNodeIndex(route.visitOrder[i + 1]);
        if (fromIdx < 0 || toIdx < 0) continue;

        const WasteNode& from = graph.getNode(fromIdx);
        const WasteNode& to = graph.getNode(toIdx);
        const IsoCoord isoFrom = RenderUtils::worldToIso(from.getWorldX(), from.getWorldY());
        const IsoCoord isoTo = RenderUtils::worldToIso(to.getWorldX(), to.getWorldY());
        const float recency =
            1.0f - clamp01(std::abs(static_cast<float>(i - focusSegment)) / 2.2f);

        drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                 Color(0.34f, 0.82f, 0.82f, 0.14f + recency * 0.10f),
                 2.0f + recency * 0.8f);
        drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                 Color(0.86f, 0.97f, 0.93f, 0.08f + recency * 0.05f),
                 0.9f + recency * 0.4f);
    }

    // Layer 3: Elegant traveling pulse
    if (drawCount > 0) {
        const float totalSegments = static_cast<float>(drawCount);
        const float pulseSpeed = 0.38f;
        const float pulsePos = std::fmod(animationTime * pulseSpeed, totalSegments);

        for (int i = 0; i < drawCount; i++) {
            const int fromIdx = graph.findNodeIndex(route.visitOrder[i]);
            const int toIdx = graph.findNodeIndex(route.visitOrder[i + 1]);
            if (fromIdx < 0 || toIdx < 0) continue;

            const WasteNode& from = graph.getNode(fromIdx);
            const WasteNode& to = graph.getNode(toIdx);
            const IsoCoord isoFrom = RenderUtils::worldToIso(from.getWorldX(), from.getWorldY());
            const IsoCoord isoTo = RenderUtils::worldToIso(to.getWorldX(), to.getWorldY());

            const float segDist = std::abs(pulsePos - static_cast<float>(i));
            const float segWrap = std::min(segDist, totalSegments - segDist);
            const float pulseAlpha = std::exp(-segWrap * segWrap * 2.2f) * 0.12f;

            if (pulseAlpha > 0.01f) {
                drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                         Color(0.76f, 0.98f, 0.95f, pulseAlpha), 2.8f);
            }

            const float segFloat = static_cast<float>(i);
            if (pulsePos >= segFloat && pulsePos < segFloat + 1.0f) {
                const float t = pulsePos - segFloat;
                const float dotX = RenderUtils::lerp(isoFrom.x, isoTo.x, t);
                const float dotY = RenderUtils::lerp(isoFrom.y, isoTo.y, t);
                drawDiamond(dotX, dotY, 3.8f, 1.8f,
                            Color(0.64f, 1.0f, 0.96f, 0.13f));
                drawDiamondOutline(dotX, dotY, 2.5f, 1.2f,
                                   Color(0.92f, 1.0f, 1.0f, 0.26f), 1.0f);
                drawFilledCircle(dotX, dotY, 1.0f,
                                 Color(1.0f, 1.0f, 1.0f, 0.72f));
            }
        }
    }

    // Waypoints stay visible without taking over the scene
    for (int i = 0; i <= drawCount && i < static_cast<int>(route.visitOrder.size()); i++) {
        const int idx = graph.findNodeIndex(route.visitOrder[i]);
        if (idx < 0) continue;
        const WasteNode& node = graph.getNode(idx);
        const bool isCurrent = !node.getIsHQ() && !node.isCollected() && i == drawCount;
        const bool isServiced = !node.getIsHQ() && node.isCollected();
        drawWaypointMarker(node, isCurrent, isServiced);
    }
}

// =====================================================================
//  Waste nodes
// =====================================================================

void IsometricRenderer::drawWasteNode(const WasteNode& node, float time) {
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
        const float pulse = RenderUtils::getUrgencyPulse(time);
        accentColor.r = clampColor(accentColor.r * pulse);
        accentColor.g = clampColor(accentColor.g * pulse);
        accentColor.b = clampColor(accentColor.b * pulse);
    }

    const float bob = std::sin(time * 1.0f + node.getWorldX() * 0.45f +
                               node.getWorldY() * 0.28f) * 1.0f;
    gCurrentGarbageSinkProgress = getGarbageSinkState(node.getId());

    // Keep a clear water gap between the garbage patch and the island body.
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

    // ---- Water shadow beneath island ----
    drawDiamond(ix, iy + 11.0f * s + bob * 0.16f,
                30.0f * s, 13.0f * s, islandShadow);

    {
        const int variant = positiveMod(seed * 7 + 3, 4);
        const float jx = pseudoRandomSigned(seed, 0, 101) * 0.5f;

        // ---- 3D Cliff Landmass (Boom Beach style) ----
        // Main island block — sand top surface with cliff face on sides
        drawIsometricBlock(ix + jx * s, iy + 3.8f * s + bob * 0.10f,
                           22.0f * s, 13.0f * s, 4.0f * s, beachTop, cliffDark);

        // Asymmetric extension blocks give each island a unique shape
        if (variant == 0 || variant == 2) {
            drawIsometricBlock(ix + (jx - 10.0f) * s, iy + 2.0f * s + bob * 0.08f,
                               13.0f * s, 8.5f * s, 3.2f * s,
                               shiftColor(beachTop, -0.02f), shiftColor(cliffDark, -0.02f));
        }
        if (variant == 1 || variant == 3) {
            drawIsometricBlock(ix + (jx + 10.0f) * s, iy + 1.4f * s + bob * 0.08f,
                               12.0f * s, 7.5f * s, 2.8f * s, beachLight, cliffFace);
        }
        // Secondary extension on the opposite side
        if (variant == 0 || variant == 1) {
            drawIsometricBlock(ix + (jx + 7.5f) * s, iy + 2.8f * s + bob * 0.07f,
                               9.0f * s, 6.0f * s, 2.2f * s, beachLight, cliffFace);
        } else {
            drawIsometricBlock(ix + (jx - 7.5f) * s, iy + 2.4f * s + bob * 0.07f,
                               10.0f * s, 6.5f * s, 2.4f * s,
                               shiftColor(beachTop, 0.02f), shiftColor(cliffDark, 0.02f));
        }
        // Elevated center ridge
        drawIsometricBlock(ix + jx * 0.3f * s, iy + 0.2f * s + bob * 0.06f,
                           11.0f * s, 6.0f * s, 2.0f * s, beachLight, cliffFace);

        // ---- Beach sand landing ----
        drawDiamond(ix + jx * 0.2f * s, iy + 5.6f * s + bob * 0.10f,
                    7.5f * s, 3.2f * s, beachLight);
        // White surf lines at beach edge
        drawLine(ix - 8.5f * s, iy + 5.6f * s + bob * 0.10f,
                 ix - 1.5f * s, iy + 7.5f * s + bob * 0.10f,
                 Color(1.0f, 1.0f, 1.0f, 0.84f), 1.6f);
        drawLine(ix - 1.5f * s, iy + 7.5f * s + bob * 0.10f,
                 ix + 6.5f * s, iy + 5.0f * s + bob * 0.10f,
                 Color(1.0f, 1.0f, 1.0f, 0.78f), 1.4f);

        // ---- Shoreline rocks ----
        const int rockCount = 4 + static_cast<int>(capacityNorm * 4.0f);
        for (int r = 0; r < rockCount; ++r) {
            const float ang = static_cast<float>(r) * 6.283f /
                              static_cast<float>(rockCount) +
                              pseudoRandom01(seed, r, 333) * 0.5f;
            const float dist = (10.0f + pseudoRandom01(seed, r, 444) * 3.5f) * s;
            const float rx = ix + std::cos(ang) * dist * 0.80f;
            const float ry = iy + std::sin(ang) * dist * 0.40f + 4.0f * s;
            const float rSize = (1.6f + pseudoRandom01(seed, r, 555) * 1.2f) * s;
            drawIsometricBlock(rx, ry + bob * 0.06f,
                               rSize, rSize * 0.65f, 0.9f * s,
                               rockTop, rockSide);
        }

        // ---- Dense opaque tree canopy — back layer ----
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
            drawFilledCircle(cx, cy, radius, canopy);
        }
        // ---- Canopy front layer — slightly brighter for depth ----
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
            drawFilledCircle(cx, cy, radius, shiftColor(canopy, 0.04f));
        }

        // ---- Palm trees ----
        const int palmCount = 2 + static_cast<int>(capacityNorm * 3.0f);
        for (int p = 0; p < palmCount; ++p) {
            const float spacing = 15.0f / static_cast<float>(palmCount);
            const float px = (-7.5f + static_cast<float>(p) * spacing +
                              pseudoRandomSigned(seed, p, 111) * 1.5f) * s;
            const float py = (-1.5f + pseudoRandomSigned(seed, p, 222) * 1.5f) * s;
            // Trunk
            drawLine(ix + px, iy + py,
                     ix + px + 0.4f * s, iy + py - 6.5f * s,
                     trunkColor, 1.2f);
            // Palm fronds
            drawFilledCircle(ix + px - 1.3f * s, iy + py - 6.8f * s,
                             2.2f * s, canopyC);
            drawFilledCircle(ix + px + 1.3f * s, iy + py - 7.2f * s,
                             2.0f * s, canopyB);
        }

        // ---- Animated shore foam ring ----
        const float foamPulse = 0.5f + 0.5f *
            std::sin(time * 0.8f + static_cast<float>(seed) * 0.7f);
        drawDiamondOutline(ix, iy + 9.0f * s,
                           31.0f * s * (0.98f + foamPulse * 0.02f),
                           13.5f * s * (0.98f + foamPulse * 0.02f),
                           Color(0.86f, 0.96f, 1.0f, 0.08f + foamPulse * 0.05f), 1.5f);
    }

    // ---- Garbage patch sprite (stays at node center, away from island) ----
    const float fillRatio = node.isCollected()
        ? 0.0f
        : clamp01(node.getWasteLevel() / 100.0f);
    const float patchX = iso.x;
    const float patchY = iso.y - 2.0f + bob;
    const float patchScale = scale * 1.14f;
    drawGarbagePatchSprite(patchX, patchY, patchScale, bodyColor, accentColor,
                           node.isCollected(), fillRatio);

    // ---- Collected glow overlay ----
    if (node.isCollected()) {
        const float settle = 0.4f + 0.6f * gCurrentGarbageSinkProgress;
        drawDiamondOutline(iso.x, iso.y + 4.0f * markerS,
                           22.0f * markerS * (0.96f + settle * 0.04f),
                           10.0f * markerS * (0.96f + settle * 0.04f),
                           Color(0.72f, 0.96f, 0.86f, 0.12f + settle * 0.06f), 1.1f);
    }

    // ---- Urgency indicators (around garbage at node center) ----
    if (!node.isCollected()) {
        if (node.getUrgency() != UrgencyLevel::LOW) {
            float ringRadius = 16.0f * markerS;
            float ringHeight = 7.0f * markerS;
            float thickness = 1.2f;
            if (node.getUrgency() == UrgencyLevel::HIGH) {
                ringRadius += 1.5f * markerS * std::sin(time * 2.4f);
                ringHeight += 0.7f * markerS * std::sin(time * 2.4f);
                thickness = 1.6f;
            }
            Color ringColor = accentColor;
            ringColor.a = (node.getUrgency() == UrgencyLevel::HIGH) ? 0.22f : 0.13f;
            drawDiamondOutline(iso.x, iso.y + 2.0f * markerS + bob * 0.18f,
                               ringRadius, ringHeight, ringColor, thickness);
            const float braceX = ringRadius + 1.5f * markerS;
            const float braceY = ringHeight + 0.8f * markerS;
            drawLine(iso.x - braceX, iso.y + 2.0f * markerS + bob * 0.18f,
                     iso.x - braceX + 2.5f * markerS,
                     iso.y + 2.0f * markerS - braceY + bob * 0.18f,
                     withAlpha(ringColor, ringColor.a * 0.8f), 1.0f);
            drawLine(iso.x + braceX, iso.y + 2.0f * markerS + bob * 0.18f,
                     iso.x + braceX - 2.5f * markerS,
                     iso.y + 2.0f * markerS - braceY + bob * 0.18f,
                     withAlpha(ringColor, ringColor.a * 0.8f), 1.0f);
        }

        if (node.getUrgency() != UrgencyLevel::LOW) {
            const float beaconBob = std::sin(time * 2.0f + node.getWorldX()) * 0.6f;
            const float bx = iso.x + 9.0f * markerS;
            const float by = iso.y - 8.0f * markerS + bob + beaconBob;
            drawLine(bx, by + 3.8f * markerS, bx, by - 0.8f * markerS,
                     Color(0.8f, 0.8f, 0.75f, 0.7f), 1.2f);
            drawFilledCircle(bx, by - 1.3f * markerS, 1.1f * markerS,
                             Color(accentColor.r, accentColor.g, accentColor.b,
                                   0.6f + 0.3f * std::sin(time * 4.0f)));
        }
    }
}

// =====================================================================
//  HQ harbor island
// =====================================================================

void IsometricRenderer::drawHQNode(const WasteNode& node) {
    IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
    const float zf = RenderUtils::getProjection().tileWidth / RenderUtils::BASE_TILE_WIDTH;
    const float s = 3.45f * zf;  // uniform scale factor for HQ landmark

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

    // Sand base around harbor
    const Color sandBase(0.68f, 0.60f, 0.42f, 0.65f);
    const Color sandSide(0.48f, 0.40f, 0.28f, 0.60f);
    drawDiamond(dockCx, dockCy + 14.2f * s, 28.0f * s, 12.6f * s, dockShadow);
    drawDiamond(dockCx + 2.0f * s, dockCy + 9.4f * s, 50.0f * s, 24.0f * s, reefTop);
    drawDiamond(dockCx + 1.5f * s, dockCy + 8.8f * s, 36.0f * s, 18.0f * s, reefGlow);
    drawDiamond(dockCx + 16.0f * s, dockCy + 10.0f * s, 22.0f * s, 8.6f * s, lagoon);
    drawDiamond(dockCx + 16.0f * s, dockCy + 9.2f * s, 15.5f * s, 5.4f * s, lagoonShine);
    drawIsometricBlock(dockCx, dockCy + 8.8f * s, 54.0f * s, 40.0f * s, 3.2f * s, sandBase, sandSide);
    drawIsometricBlock(dockCx - 6.5f * s, dockCy + 4.2f * s, 28.0f * s, 19.0f * s,
                       2.1f * s, duneTop, duneSide);
    drawIsometricBlock(dockCx - 19.0f * s, dockCy + 6.0f * s, 16.0f * s, 8.8f * s,
                       4.2f * s, breakwaterTop, breakwaterSide);
    drawIsometricBlock(dockCx + 20.0f * s, dockCy + 7.2f * s, 18.0f * s, 9.6f * s,
                       5.0f * s, breakwaterTop, breakwaterSide);
    drawIsometricBlock(dockCx + 14.2f * s, dockCy - 1.0f * s, 12.0f * s, 7.0f * s,
                       3.0f * s, breakwaterTop, breakwaterSide);

    // Shore foam ring
    const float shoreFoam = 0.5f + 0.5f * std::sin(animationTime * 0.8f);
    drawDiamondOutline(dockCx, dockCy + 9.6f * s,
                       56.0f * s * (0.98f + shoreFoam * 0.02f),
                       40.0f * s * (0.98f + shoreFoam * 0.02f),
                       Color(0.86f, 0.96f, 1.0f, 0.08f + shoreFoam * 0.05f), 1.6f);
    drawDiamondOutline(dockCx + 15.5f * s, dockCy + 9.6f * s,
                       27.0f * s, 11.2f * s,
                       Color(0.78f, 0.94f, 1.0f, 0.10f + shoreFoam * 0.04f), 1.3f);

    drawIsometricBlock(dockCx, dockCy, 38.0f * s, 28.0f * s, 6.0f * s, dockTop, dockSide);
    for (float offset = -0.34f; offset <= 0.34f; offset += 0.12f) {
        drawTileStripe(dockCx, dockCy - 3.8f * s, 16.0f * s, 11.0f * s,
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
        drawLine(pierTops[i].x, pierTops[i].y - 1.0f * s,
                 pierTops[i].x + sway, pierTops[i].y + (13.5f + i * 1.3f) * s,
                 dockDark, 2.5f);
        drawFilledCircle(pierTops[i].x, pierTops[i].y - 1.2f * s, 1.8f * s,
                         Color(0.42f, 0.29f, 0.16f, 0.98f));
    }

    auto drawRope = [&](const IsoCoord& start, const IsoCoord& end, float sag) {
        const int segments = 12;
        for (int i = 0; i < segments; i++) {
            const float t0 = static_cast<float>(i) / segments;
            const float t1 = static_cast<float>(i + 1) / segments;
            const auto sample = [&](float t) {
                const float x = RenderUtils::lerp(start.x, end.x, t);
                const float y = RenderUtils::lerp(start.y, end.y, t) +
                                sag * 4.0f * t * (1.0f - t);
                return IsoCoord{x, y};
            };
            const IsoCoord p0 = sample(t0);
            const IsoCoord p1 = sample(t1);
            drawLine(p0.x, p0.y, p1.x, p1.y, ropeColor, 1.5f);
        }
    };

    drawRope({dockCx - 12.0f * s, dockCy - 1.8f * s}, {dockCx - 4.0f * s, dockCy + 1.3f * s}, 4.8f * s);
    drawRope({dockCx + 5.5f * s, dockCy + 1.0f * s}, {dockCx + 12.5f * s, dockCy - 1.6f * s}, 4.2f * s);
    drawRope({dockCx + 11.0f * s, dockCy - 0.8f * s}, {dockCx + 18.0f * s, dockCy - 3.3f * s}, 3.8f * s);

    // Lighthouse / beacon tower
    const float beamAngle = animationTime * 1.35f;
    const float beamSpread = 0.44f;
    const IsoCoord beaconBase{dockCx + 9.5f * s, dockCy - 11.8f * s};
    drawIsometricBlock(beaconBase.x, beaconBase.y + 4.0f * s, 7.5f * s, 6.0f * s, 10.0f * s,
                       Color(0.88f, 0.89f, 0.84f, 1.0f),
                       Color(0.63f, 0.64f, 0.60f, 1.0f));
    drawIsometricBlock(beaconBase.x, beaconBase.y - 1.4f * s, 4.0f * s, 3.4f * s, 4.0f * s,
                       Color(0.90f, 0.40f, 0.25f, 1.0f),
                       Color(0.68f, 0.24f, 0.16f, 1.0f));
    const IsoCoord lightCenter{beaconBase.x, beaconBase.y - 6.0f * s};
    drawFilledCircle(lightCenter.x, lightCenter.y, 2.2f * s,
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
    drawIsometricBlock(dockCx - 8.0f * s, dockCy - 6.0f * s, 10.0f * s, 8.0f * s, 5.0f * s,
                       shedTop, shedSide);
    drawIsometricBlock(dockCx + 4.8f * s, dockCy - 6.5f * s, 7.8f * s, 6.0f * s, 4.0f * s,
                       Color(0.74f, 0.78f, 0.82f, 0.92f),
                       Color(0.48f, 0.52f, 0.56f, 0.92f));
    drawLine(dockCx + 1.0f * s, dockCy - 4.6f * s,
             dockCx + 7.8f * s, dockCy - 11.6f * s,
             steel, 1.7f);
    drawLine(dockCx + 6.8f * s, dockCy - 11.0f * s,
             dockCx + 12.2f * s, dockCy - 8.8f * s,
             steel, 1.4f);
    drawLine(dockCx + 8.2f * s, dockCy - 10.8f * s,
             dockCx + 8.2f * s, dockCy - 4.0f * s,
             steel, 1.3f);
    drawFilledCircle(dockCx + 12.0f * s, dockCy - 8.6f * s, 1.1f * s,
                     Color(0.95f, 0.62f, 0.20f, 0.80f));

    const IsoCoord radarHub{dockCx + 8.4f * s, dockCy - 10.8f * s};
    const float radarSweep = animationTime * 0.72f;
    drawFilledCircle(radarHub.x, radarHub.y, 0.9f * s,
                     Color(0.54f, 0.88f, 0.98f, 0.78f));
    drawDiamondOutline(radarHub.x, radarHub.y,
                       4.2f * s, 1.9f * s,
                       Color(0.48f, 0.86f, 0.96f, 0.20f), 1.0f);
    const IsoCoord radarA{
        radarHub.x + std::cos(radarSweep - 0.22f) * 22.0f * s,
        radarHub.y + std::sin(radarSweep - 0.22f) * 10.0f * s
    };
    const IsoCoord radarB{
        radarHub.x + std::cos(radarSweep + 0.22f) * 22.0f * s,
        radarHub.y + std::sin(radarSweep + 0.22f) * 10.0f * s
    };
    drawImmediateTriangle({{radarHub, radarA, radarB}},
                          Color(0.58f, 0.90f, 0.98f, 0.08f));

    for (size_t light = 0; light < pierTops.size(); ++light) {
        drawFilledCircle(pierTops[light].x,
                         pierTops[light].y - 2.0f * s,
                         0.75f * s,
                         Color(0.98f, 0.88f, 0.62f, 0.24f + light * 0.03f));
    }

    const float skiffBob = std::sin(animationTime * 1.6f + 0.8f) * 0.5f * s;
    drawDiamond(dockCx + 22.0f * s, dockCy + 16.2f * s + skiffBob,
                7.4f * s, 3.0f * s,
                Color(0.02f, 0.06f, 0.12f, 0.18f));
    drawIsometricBlock(dockCx + 21.0f * s, dockCy + 11.8f * s + skiffBob,
                       8.6f * s, 4.0f * s, 1.2f * s,
                       Color(0.70f, 0.32f, 0.20f, 0.94f),
                       Color(0.46f, 0.18f, 0.10f, 0.94f));
    drawLine(dockCx + 21.0f * s, dockCy + 8.8f * s + skiffBob,
             dockCx + 21.0f * s, dockCy + 3.8f * s + skiffBob,
             Color(0.78f, 0.78f, 0.74f, 0.72f), 1.1f);
    drawImmediateTriangle({{
        {dockCx + 21.0f * s, dockCy + 3.8f * s + skiffBob},
        {dockCx + 24.2f * s, dockCy + 6.0f * s + skiffBob},
        {dockCx + 21.0f * s, dockCy + 7.0f * s + skiffBob}
    }}, Color(0.92f, 0.94f, 0.90f, 0.72f));

    // Vegetation on harbor island
    drawFilledCircle(dockCx - 14.0f * s, dockCy - 2.0f * s, 3.5f * s,
                     Color(0.18f, 0.48f, 0.26f, 0.70f));
    drawFilledCircle(dockCx - 15.5f * s, dockCy + 1.0f * s, 2.8f * s,
                     Color(0.22f, 0.52f, 0.30f, 0.65f));

    // Water foam laps
    for (int lap = 0; lap < 4; lap++) {
        const float phase = animationTime * 1.3f + static_cast<float>(lap) * 0.65f;
        drawLine(dockCx + (-14.0f + lap * 8.8f) * s, dockCy + (12.0f + std::sin(phase) * 0.8f) * s,
                 dockCx + (-8.0f + lap * 8.8f) * s, dockCy + (13.5f + std::cos(phase) * 0.8f) * s,
                 foam, 1.2f);
    }
    drawDiamondOutline(dockCx, dockCy + 7.8f * s, 18.0f * s, 8.0f * s,
                       Color(0.74f, 0.94f, 1.0f, 0.12f), 1.2f);
    drawDiamondOutline(dockCx + 15.4f * s, dockCy + 10.0f * s, 16.0f * s, 6.0f * s,
                       Color(0.74f, 0.94f, 1.0f, 0.10f), 1.0f);

    // Seagulls circling
    for (int gull = 0; gull < 3; gull++) {
        const float orbit = animationTime * (0.55f + gull * 0.08f) + gull * 2.1f;
        const float gx = dockCx - 6.0f * s + std::cos(orbit) * (12.0f + gull * 3.0f) * s;
        const float gy = dockCy - 28.0f * s + std::sin(orbit * 1.2f) * (5.0f + gull) * s;
        const float wing = (2.0f + gull * 0.4f) * s;
        drawLine(gx - wing, gy, gx, gy - 1.2f * s, Color(0.94f, 0.96f, 0.98f, 0.70f), 1.0f);
        drawLine(gx, gy - 1.2f * s, gx + wing, gy, Color(0.94f, 0.96f, 0.98f, 0.70f), 1.0f);
    }

    // Anchor emblem
    drawRing(dockCx - 1.0f * s, dockCy - 5.6f * s, 5.2f * s,
             Color(0.18f, 0.55f, 0.88f, 0.70f), 2.0f);
    drawFilledCircle(dockCx - 1.0f * s, dockCy - 5.6f * s, 1.8f * s,
                     Color(0.18f, 0.55f, 0.88f, 0.78f));
}

// =====================================================================
//  Truck / boat
// =====================================================================

void IsometricRenderer::drawTruck(const MapGraph& graph, const Truck& truck,
                                   const RouteResult& currentRoute) {
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
    gBoatCargoFillRatio = computeBoatCargoFill(graph, truck, currentRoute);
    gBoatPitchWave = bowSurface - sternSurface;
    gBoatRollWave = portSurface - starboardSurface;
    gBoatWakeStrength = truck.isMoving()
        ? 0.24f + boatWave.crest * 0.14f + std::abs(gBoatPitchWave) * 0.10f
        : 0.04f + boatWave.crest * 0.02f;
    gBoatIsMoving = truck.isMoving();
    const float boatSurfaceOffset =
        ((bowSurface + sternSurface) * 0.5f + boatWave.swell * 0.34f + boatWave.primary * 0.10f) * 4.0f;
    const float bobPhase = animationTime * (truck.isMoving() ? 1.9f : 1.35f) +
                           boatWave.combined * 1.8f;
    drawBoatSprite(iso.x, iso.y + 0.9f + boatSurfaceOffset, 4.4f, RenderUtils::getTruckColor(),
                   headingX, headingY, bobPhase);
}

void IsometricRenderer::drawBoatSprite(float cx, float cy, float scale,
                                        const Color& bodyColor,
                                        float headingX, float headingY,
                                        float bobPhase) {
    const float bobOffset = (std::sin(bobPhase) * 0.018f + gBoatPitchWave * 0.08f) * scale;
    const float rollOffset = (std::sin(bobPhase * 0.85f + animationTime * 1.6f) * 0.05f +
                              gBoatRollWave * 0.55f) * scale;
    const float pitchOffset = (gBoatIsMoving ? 0.03f : 0.014f) *
                              std::sin(animationTime * (gBoatIsMoving ? 4.8f : 1.2f) +
                                       bobPhase * 0.45f) + gBoatPitchWave * 0.10f;
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
    const Color cabinLeft(bodyColor.r * 0.62f, bodyColor.g * 0.62f,
                          bodyColor.b * 0.62f, 1.0f);
    const Color cabinRight(bodyColor.r * 0.78f, bodyColor.g * 0.78f,
                           bodyColor.b * 0.78f, 1.0f);
    const Color cabinRear(bodyColor.r * 0.54f, bodyColor.g * 0.54f,
                          bodyColor.b * 0.54f, 1.0f);
    const Color sailTop(0.97f, 0.95f, 0.86f, 0.98f);
    const Color sailShade(0.82f, 0.86f, 0.84f, 0.96f);
    const Color glass(0.38f, 0.74f, 0.94f, 0.94f);
    const Color rail(0.88f, 0.84f, 0.74f, 0.88f);
    const Color wakeColor(0.82f, 0.95f, 1.0f, 0.10f + gBoatWakeStrength * 0.12f);
    const Color foamColor(0.92f, 0.98f, 1.0f, 0.10f + gBoatWakeStrength * 0.14f);
    const Color cargoTop(0.18f, 0.18f, 0.16f, 0.94f);
    const Color cargoSide(0.10f, 0.10f, 0.09f, 0.92f);
    const auto& projection = RenderUtils::getProjection();

    float screenForwardX = (headingX - headingY) * (projection.tileWidth * 0.5f);
    float screenForwardY = (headingX + headingY) * (projection.tileHeight * 0.5f);
    normalizeVector(screenForwardX, screenForwardY, 0.92f, 0.38f);
    const float screenRightX = -screenForwardY;
    const float screenRightY = screenForwardX;

    drawDiamond(cx, cy + 6.6f * scale,
                10.6f * scale, 4.2f * scale,
                Color(0.02f, 0.07f, 0.12f, 0.20f));
    drawDiamond(cx + screenForwardX * 1.2f,
                cy + 5.8f * scale + std::sin(animationTime * 1.7f + bobPhase) * 0.4f,
                9.2f * scale, 3.2f * scale,
                Color(0.54f, 0.84f, 0.94f, 0.06f + gBoatWakeStrength * 0.05f));
    drawDiamondOutline(cx, cy + 5.7f * scale,
                       11.2f * scale * (0.98f + gBoatWakeStrength * 0.03f),
                       4.0f * scale * (0.98f + gBoatWakeStrength * 0.04f),
                       Color(0.88f, 0.97f, 1.0f, 0.08f + gBoatWakeStrength * 0.06f), 1.0f);

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

    // Wake and propeller wash
    const IsoCoord sternWash = boatPoint(0.0f, -10.2f, 0.2f);
    const float wakeBase = 0.70f + gBoatWakeStrength * 0.90f;
    drawFilledCircle(sternWash.x, sternWash.y + 1.5f, 1.7f * scale * wakeBase,
                     shiftColor(foamColor, 0.0f, 0.55f));
    drawRing(sternWash.x, sternWash.y + 1.5f, 2.6f * scale * wakeBase,
             shiftColor(foamColor, -0.04f, 0.48f), 0.9f);
    if (gBoatIsMoving) {
        for (int band = 0; band < 4; band++) {
            const float lateral = 1.6f + band * 1.7f;
            const float rear = 12.0f + band * 6.0f;
            const float alphaScale = 0.88f - band * 0.22f;
            drawLine(boatPoint(-1.6f, -8.0f, 0.2f).x, boatPoint(-1.6f, -8.0f, 0.2f).y,
                     boatPoint(-lateral, -rear, 0.0f).x, boatPoint(-lateral, -rear, 0.0f).y,
                     shiftColor(wakeColor, 0.01f * band, alphaScale), 1.2f - band * 0.2f);
            drawLine(boatPoint(1.6f, -8.0f, 0.2f).x, boatPoint(1.6f, -8.0f, 0.2f).y,
                     boatPoint(lateral, -rear, 0.0f).x, boatPoint(lateral, -rear, 0.0f).y,
                     shiftColor(wakeColor, 0.01f * band, alphaScale), 1.2f - band * 0.2f);
        }
        drawLine(sternWash.x, sternWash.y + 0.5f,
                 boatPoint(0.0f, -18.0f, 0.0f).x, boatPoint(0.0f, -18.0f, 0.0f).y,
                 shiftColor(wakeColor, -0.02f, 0.72f), 1.0f);
        drawLine(sternWash.x, sternWash.y + 1.2f,
                 boatPoint(0.0f, -24.0f, 0.0f).x, boatPoint(0.0f, -24.0f, 0.0f).y,
                 shiftColor(foamColor, -0.04f, 0.44f), 0.9f);
    }

    // Hull
    prism(-7.8f, 7.8f, -8.2f, 7.4f, 0.0f, 2.5f,
          hullTop, hullLeft, hullRight, hullRear);
    tri({boatPoint(0.0f, 12.0f, 1.4f), boatPoint(-5.4f, 7.4f, 2.5f),
         boatPoint(5.4f, 7.4f, 2.5f)}, hullTop);
    tri({boatPoint(0.0f, 12.0f, 0.0f), boatPoint(0.0f, 12.0f, 1.4f),
         boatPoint(-5.4f, 7.4f, 2.5f)}, hullLeft);
    tri({boatPoint(0.0f, 12.0f, 0.0f), boatPoint(5.4f, 7.4f, 2.5f),
         boatPoint(0.0f, 12.0f, 1.4f)}, hullRight);

    // Trim and deck
    prism(-7.1f, 7.1f, -7.4f, 6.8f, 2.5f, 3.2f,
          trimTop, trimSide, shiftColor(trimSide, 0.04f), shiftColor(trimSide, -0.08f));
    prism(-6.2f, 6.2f, -6.2f, 5.8f, 3.2f, 4.3f,
          deckTop, deckSide, shiftColor(deckSide, 0.06f), shiftColor(deckSide, -0.08f));
    prism(-5.6f, 5.6f, -5.6f, 0.8f, 4.3f, 6.4f,
          shiftColor(deckTop, -0.10f), shiftColor(deckSide, -0.08f),
          shiftColor(deckSide, -0.02f), shiftColor(deckSide, -0.12f));

    // Cargo hold
    const float cargoHeight = 4.38f + gBoatCargoFillRatio * 1.55f;
    if (gBoatCargoFillRatio > 0.02f) {
        prism(-4.8f, 4.8f, -4.9f, 0.1f, 4.32f, cargoHeight,
              cargoTop, cargoSide, shiftColor(cargoSide, 0.04f), shiftColor(cargoSide, -0.02f));
        for (int piece = 0; piece < 4; piece++) {
            const float px = -2.8f + piece * 1.8f;
            const float py = -4.1f + piece * 0.95f;
            const IsoCoord debris = boatPoint(px, py, cargoHeight + 0.08f + std::sin(animationTime * 1.6f + piece) * 0.08f);
            drawFilledCircle(debris.x, debris.y, (0.34f + piece * 0.05f) * scale,
                             Color(0.12f, 0.11f, 0.09f, 0.92f));
        }
    }

    // Cabin and windows
    prism(-4.1f, 4.1f, 2.7f, 7.0f, 4.3f, 8.6f,
          cabinTop, cabinLeft, cabinRight, cabinRear);
    prism(-2.9f, 2.9f, 4.8f, 7.2f, 6.0f, 8.2f,
          glass, shiftColor(glass, -0.12f), shiftColor(glass, -0.04f), shiftColor(glass, -0.20f));

    // Sail accents
    quad({boatPoint(-4.6f, 3.0f, 8.6f), boatPoint(4.6f, 3.0f, 8.6f),
          boatPoint(2.4f, 0.8f, 9.8f), boatPoint(-2.4f, 0.8f, 9.8f)},
         sailTop);
    quad({boatPoint(-0.4f, 1.2f, 9.7f), boatPoint(2.4f, 1.1f, 9.6f),
          boatPoint(2.6f, -2.8f, 9.0f), boatPoint(-0.2f, -2.7f, 9.1f)},
         sailShade);

    // Railing
    for (float pos = -4.4f; pos <= 4.4f; pos += 2.2f) {
        const IsoCoord leftBottom = boatPoint(-6.4f, pos, 4.3f);
        const IsoCoord leftTop = boatPoint(-6.4f, pos, 6.1f);
        const IsoCoord rightBottom = boatPoint(6.4f, pos, 4.3f);
        const IsoCoord rightTop = boatPoint(6.4f, pos, 6.1f);
        drawLine(leftBottom.x, leftBottom.y, leftTop.x, leftTop.y, rail, 1.0f);
        drawLine(rightBottom.x, rightBottom.y, rightTop.x, rightTop.y, rail, 1.0f);
    }
    drawLine(boatPoint(-6.4f, -4.4f, 6.1f).x, boatPoint(-6.4f, -4.4f, 6.1f).y,
             boatPoint(-6.4f, 4.4f, 6.1f).x, boatPoint(-6.4f, 4.4f, 6.1f).y,
             rail, 1.0f);
    drawLine(boatPoint(6.4f, -4.4f, 6.1f).x, boatPoint(6.4f, -4.4f, 6.1f).y,
             boatPoint(6.4f, 4.4f, 6.1f).x, boatPoint(6.4f, 4.4f, 6.1f).y,
             rail, 1.0f);

    // Mast and flag
    const IsoCoord mastBase = boatPoint(0.4f, 2.2f, 8.8f);
    const IsoCoord mastTop = boatPoint(0.4f, 2.2f, 12.8f);
    drawLine(mastBase.x, mastBase.y, mastTop.x, mastTop.y,
             Color(0.55f, 0.40f, 0.24f, 1.0f), 1.4f);
    const float flagSway = std::sin(animationTime * 3.4f + bobPhase * 0.6f) * 1.2f;
    const IsoCoord flagTip{
        mastTop.x + screenForwardX * (3.0f + flagSway * 0.4f) +
            screenRightX * (1.2f + flagSway * 0.3f),
        mastTop.y + screenForwardY * (3.0f + flagSway * 0.4f) +
            screenRightY * (1.2f + flagSway * 0.3f) + flagSway * 0.6f
    };
    const IsoCoord flagLow{
        mastTop.x + screenForwardX * 2.0f + screenRightX * 0.2f,
        mastTop.y + screenForwardY * 2.0f + screenRightY * 0.2f + 1.3f
    };
    tri({mastTop, flagTip, flagLow}, Color(0.95f, 0.84f, 0.22f, 0.96f));

    // Navigation lights
    const IsoCoord bowLight = boatPoint(0.0f, 11.0f, 2.0f);
    const IsoCoord sternLeft = boatPoint(-3.8f, -7.4f, 3.5f);
    const IsoCoord sternRight = boatPoint(3.8f, -7.4f, 3.5f);
    drawFilledCircle(bowLight.x, bowLight.y, 0.65f * scale,
                     Color(1.0f, 0.94f, 0.60f, 0.96f));
    drawFilledCircle(sternLeft.x, sternLeft.y, 0.50f * scale,
                     Color(0.94f, 0.26f, 0.20f, 0.94f));
    drawFilledCircle(sternRight.x, sternRight.y, 0.50f * scale,
                     Color(0.94f, 0.26f, 0.20f, 0.94f));

    // Waterline interactions
    for (int lap = 0; lap < 5; lap++) {
        const float t = static_cast<float>(lap) / 4.0f;
        const float surge = std::sin(animationTime * 2.0f + t * 3.5f) * 0.26f;
        const IsoCoord leftWaterA = boatPoint(-7.6f, -6.4f + t * 12.0f, 0.8f);
        const IsoCoord leftWaterB = boatPoint(-8.7f, -6.0f + t * 11.0f, 0.2f);
        const IsoCoord rightWaterA = boatPoint(7.6f, -6.4f + t * 12.0f, 0.8f);
        const IsoCoord rightWaterB = boatPoint(8.7f, -6.0f + t * 11.0f, 0.2f);
        drawLine(leftWaterA.x, leftWaterA.y + surge, leftWaterB.x, leftWaterB.y - surge,
                 Color(0.82f, 0.95f, 1.0f, 0.16f), 1.0f);
        drawLine(rightWaterA.x, rightWaterA.y - surge, rightWaterB.x, rightWaterB.y + surge,
                 Color(0.82f, 0.95f, 1.0f, 0.16f), 1.0f);
    }
    drawDiamond(cx + screenForwardX * 1.2f,
                cy + 6.2f * scale,
                9.2f * scale, 2.2f * scale,
                Color(0.88f, 0.97f, 1.0f, 0.05f));

    // Bow spray when moving
    if (gBoatIsMoving) {
        for (int spray = 0; spray < 3; spray++) {
            const float phase = animationTime * 3.4f + spray * 0.8f;
            const IsoCoord particle = boatPoint(-1.4f + std::sin(phase) * 2.8f,
                                                9.8f + spray * 0.6f,
                                                0.45f + std::cos(phase) * 0.18f);
            drawFilledCircle(particle.x, particle.y,
                             (0.14f + spray * 0.03f) * scale,
                             Color(0.96f, 0.99f, 1.0f, 0.14f + 0.04f * (2 - spray)));
        }
    }
}

// =====================================================================
//  Garbage patch sprite
// =====================================================================

void IsometricRenderer::drawGarbagePatchSprite(float cx, float cy, float scale,
                                                const Color& bodyColor,
                                                const Color& accentColor,
                                                bool collected,
                                                float fillRatio) {
    const float sink = collected ? gCurrentGarbageSinkProgress : 0.0f;
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
    const Color debrisDark(bodyColor.r * 0.48f, bodyColor.g * 0.48f,
                           bodyColor.b * 0.48f, 0.88f * alphaScale);
    const Color plasticWhite(0.90f, 0.92f, 0.88f, 0.72f * alphaScale);
    const Color plasticBlue(0.30f, 0.50f, 0.82f, 0.82f * alphaScale);
    const Color bottleGreen(0.26f, 0.56f, 0.42f, 0.72f * alphaScale);
    const float sheenMix = 0.5f + 0.5f *
        std::sin(animationTime * 0.9f + cx * 0.02f - cy * 0.03f);
    const Color sheen = mixColor(Color(0.82f, 0.35f, 0.72f, 0.24f * alphaScale),
                                 Color(0.24f, 0.78f, 0.88f, 0.24f * alphaScale),
                                 sheenMix);

    drawDiamond(cx, cy + 3.0f * displayScale, 10.8f * displayScale, 5.8f * displayScale,
                waterShadow);
    drawDiamond(cx + std::sin(animationTime * 1.1f) * 0.3f, cy + 1.2f * displayScale,
                8.2f * displayScale, 4.4f * displayScale,
                Color(0.08f, 0.20f, 0.36f, 0.18f * alphaScale));

    if (alphaScale <= 0.02f) {
        return;
    }

    const float foamPulse = 0.92f + 0.10f * std::sin(animationTime * 2.0f + cx * 0.04f);
    drawDiamondOutline(cx, cy + 0.9f * displayScale,
                       8.8f * displayScale * foamPulse,
                       4.6f * displayScale * foamPulse,
                       foamColor, 1.2f);
    drawDiamondOutline(cx, cy + 1.0f * displayScale,
                       11.0f * displayScale * foamPulse,
                       5.4f * displayScale * foamPulse,
                       shiftColor(foamColor, -0.04f, 0.70f), 1.0f);

    const float patchW = 8.0f * displayScale * (0.88f + displayFill * 0.40f);
    const float patchH = 6.0f * displayScale * (0.90f + displayFill * 0.32f);
    const float patchDepth = (0.9f + displayFill * 1.7f) * displayScale;
    drawIsometricBlock(cx, cy + 0.8f * displayScale, patchW, patchH, patchDepth,
                       debrisBase, debrisDark);
    drawDiamond(cx + 1.0f * displayScale, cy + 0.5f * displayScale,
                4.8f * displayScale, 2.2f * displayScale, sheen);

    auto rotatedQuad = [&](float ox, float oy, float halfW, float halfH,
                           float angle, const Color& color) {
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        auto pt = [&](float lx, float ly) {
            return IsoCoord{
                cx + ox + lx * c - ly * s,
                cy + oy + lx * s + ly * c
            };
        };
        drawImmediateQuad({pt(-halfW, -halfH), pt(halfW, -halfH),
                           pt(halfW, halfH), pt(-halfW, halfH)}, color);
    };

    for (int piece = 0; piece < 5; piece++) {
        const float driftX = std::sin(animationTime * (0.45f + piece * 0.08f) +
                                      piece * 1.7f) * (1.4f + piece * 0.18f) * displayScale;
        const float driftY = std::cos(animationTime * (0.52f + piece * 0.06f) +
                                      piece * 1.2f) * (0.9f + piece * 0.14f) * displayScale;
        const float angle = animationTime * (0.25f + piece * 0.05f) + piece * 0.7f;
        rotatedQuad(driftX, driftY - patchDepth * 0.35f,
                    (0.65f + piece * 0.08f) * displayScale,
                    (0.42f + piece * 0.05f) * displayScale,
                    angle,
                    (piece % 2 == 0) ? debrisDark : shiftColor(debrisBase, 0.08f));
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
        rotatedQuad(ox, oy,
                    0.30f * displayScale,
                    1.05f * displayScale,
                    tilt,
                    (bottle == 1) ? plasticBlue : bottleGreen);
        drawFilledCircle(cx + ox + std::cos(tilt) * 0.55f * displayScale,
                         cy + oy + std::sin(tilt) * 0.55f * displayScale,
                         0.18f * displayScale,
                         Color(0.94f, 0.98f, 1.0f, 0.76f * alphaScale));
    }

    drawFilledCircle(cx - 1.8f * displayScale, cy - patchDepth * 0.42f,
                     0.85f * displayScale, plasticWhite);
    drawFilledCircle(cx + 2.1f * displayScale, cy - patchDepth * 0.48f,
                     0.72f * displayScale, plasticBlue);

    drawDiamondOutline(cx, cy + 0.8f * displayScale,
                       patchW * 0.72f, patchH * 0.72f,
                       shiftColor(accentColor, 0.0f, 0.54f * alphaScale), 1.2f);
}

// =====================================================================
//  Node label (stub — labels via ImGui overlay)
// =====================================================================

void IsometricRenderer::drawNodeLabel(const WasteNode& /*node*/) {
}

// =====================================================================
//  Decorative islets — world composition
// =====================================================================

void IsometricRenderer::drawDecorativeIslets(const MapGraph& graph) {
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
        const Color rockTop(0.62f, 0.54f, 0.40f, 1.0f);
        const Color rockSide(0.42f, 0.35f, 0.24f, 1.0f);
        const float bob = std::sin(animationTime * 0.85f + seedPhase) * 0.25f * s;

        drawDiamond(iso.x - 2.0f * s, iso.y + 12.0f * s + bob,
                    28.0f * s, 12.0f * s, shadow);

        // Solid landmass with a front beach cove instead of stacked floating slabs.
        drawIsometricBlock(iso.x - 8.0f * s, iso.y + 3.8f * s + bob * 0.2f,
                           22.0f * s, 13.2f * s, 4.0f * s, beachTop, cliffDark);
        drawIsometricBlock(iso.x - 20.5f * s, iso.y + 1.2f * s + bob * 0.15f,
                           14.4f * s, 9.4f * s, 3.3f * s,
                           shiftColor(beachTop, -0.02f), shiftColor(cliffDark, -0.02f));
        drawIsometricBlock(iso.x + 10.5f * s, iso.y + 0.8f * s + bob * 0.10f,
                           11.0f * s, 7.2f * s, 2.6f * s,
                           beachTopLight, cliffFace);
        drawIsometricBlock(iso.x - 2.0f * s, iso.y - 0.2f * s + bob * 0.08f,
                           10.8f * s, 6.0f * s, 1.8f * s,
                           beachTopLight, cliffFace);

        // Beach landing where the node sits in front of the island.
        drawDiamond(iso.x - 2.8f * s, iso.y + 5.4f * s + bob * 0.10f,
                    7.4f * s, 3.0f * s, beachTopLight);
        drawLine(iso.x - 8.4f * s, iso.y + 5.4f * s + bob * 0.10f,
                 iso.x - 2.8f * s, iso.y + 7.3f * s + bob * 0.10f,
                 Color(1.0f, 1.0f, 1.0f, 0.86f), 1.6f);
        drawLine(iso.x - 2.8f * s, iso.y + 7.3f * s + bob * 0.10f,
                 iso.x + 3.2f * s, iso.y + 5.0f * s + bob * 0.10f,
                 Color(1.0f, 1.0f, 1.0f, 0.82f), 1.5f);

        // Cliff stones around the shoreline.
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
            drawIsometricBlock(rocks[i].x, rocks[i].y + bob * 0.06f,
                               rockSize, rockSize * 0.68f, 0.9f * s,
                               rockTop, rockSide);
        }

        // Dense opaque canopy mass like the reference, not translucent circles.
        const std::array<IsoCoord, 14> canopyCenters = lushNorth
            ? std::array<IsoCoord, 14>{{
                {iso.x - 20.0f * s, iso.y - 4.5f * s},
                {iso.x - 14.5f * s, iso.y - 7.0f * s},
                {iso.x - 8.5f * s, iso.y - 8.8f * s},
                {iso.x - 2.0f * s, iso.y - 9.6f * s},
                {iso.x + 4.8f * s, iso.y - 9.2f * s},
                {iso.x + 10.5f * s, iso.y - 7.6f * s},
                {iso.x + 15.0f * s, iso.y - 4.8f * s},
                {iso.x - 17.0f * s, iso.y - 1.0f * s},
                {iso.x - 10.5f * s, iso.y - 2.4f * s},
                {iso.x - 4.0f * s, iso.y - 2.8f * s},
                {iso.x + 2.5f * s, iso.y - 2.2f * s},
                {iso.x + 8.5f * s, iso.y - 1.4f * s},
                {iso.x - 7.0f * s, iso.y - 13.0f * s},
                {iso.x + 1.0f * s, iso.y - 12.0f * s}
            }}
            : std::array<IsoCoord, 14>{{
                {iso.x - 19.0f * s, iso.y - 5.0f * s},
                {iso.x - 13.0f * s, iso.y - 7.4f * s},
                {iso.x - 7.2f * s, iso.y - 9.0f * s},
                {iso.x - 1.2f * s, iso.y - 9.5f * s},
                {iso.x + 5.2f * s, iso.y - 8.8f * s},
                {iso.x + 11.0f * s, iso.y - 6.6f * s},
                {iso.x + 15.5f * s, iso.y - 3.2f * s},
                {iso.x - 15.5f * s, iso.y - 0.8f * s},
                {iso.x - 9.0f * s, iso.y - 2.6f * s},
                {iso.x - 2.5f * s, iso.y - 3.0f * s},
                {iso.x + 3.6f * s, iso.y - 2.4f * s},
                {iso.x + 9.5f * s, iso.y - 1.6f * s},
                {iso.x - 5.5f * s, iso.y - 12.5f * s},
                {iso.x + 2.4f * s, iso.y - 11.5f * s}
            }};
        const std::array<float, 14> canopyRadii = {{
            6.2f, 6.6f, 7.0f, 7.3f, 7.0f, 6.4f, 5.8f,
            5.9f, 6.3f, 6.2f, 5.8f, 5.2f, 4.8f, 4.4f
        }};

        for (size_t i = 0; i < canopyCenters.size(); ++i) {
            const Color canopy = (i % 3 == 0) ? canopyA : ((i % 3 == 1) ? canopyB : canopyC);
            drawFilledCircle(canopyCenters[i].x,
                             canopyCenters[i].y + bob * 0.08f,
                             canopyRadii[i] * s, canopy);
        }

        for (int palm = 0; palm < 5; ++palm) {
            const float px = (-17.0f + palm * 8.8f) * s;
            const float py = (-1.0f - (palm % 2) * 2.2f) * s;
            drawLine(iso.x + px, iso.y + py,
                     iso.x + px + 0.5f * s, iso.y + py - 6.2f * s,
                     trunk, 1.1f);
            drawFilledCircle(iso.x + px - 1.2f * s, iso.y + py - 6.5f * s,
                             2.2f * s, canopyC);
            drawFilledCircle(iso.x + px + 1.2f * s, iso.y + py - 6.9f * s,
                             2.0f * s, canopyB);
        }
    };

    auto drawAmbientIslet = [&](float wx, float wy, int seed, float scaleBias,
                                int variantOverride = -1,
                                float minNodeDistance = 4.8f) {
        float minDist = 1e9f;
        for (const auto& node : graph.getNodes()) {
            const float dx = wx - node.getWorldX();
            const float dy = wy - node.getWorldY();
            minDist = std::min(minDist, std::sqrt(dx * dx + dy * dy));
        }
        if (minDist < minNodeDistance) return;

        const IsoCoord iso = RenderUtils::worldToIso(wx, wy);
        const float zf = RenderUtils::getProjection().tileWidth / RenderUtils::BASE_TILE_WIDTH;
        const float isletScale =
            (0.90f + pseudoRandom01(seed, 2, 777) * 0.95f) * scaleBias * zf;
        const float tide = std::sin(animationTime * 0.45f + seed * 1.4f) * 0.4f;
        const int variant = (variantOverride >= 0)
            ? variantOverride
            : positiveMod(seed + static_cast<int>(
                pseudoRandom01(seed, 0, 811) * 9.0f), 5);

        // Water shadow
        drawDiamond(iso.x, iso.y + 3.5f * isletScale + tide * 0.15f,
                    9.0f * isletScale, 4.5f * isletScale,
                    Color(0.02f, 0.06f, 0.14f, 0.20f));

        if (variant == 0) {
            // Sandy islet with vegetation
            const Color sandTop(0.74f, 0.66f, 0.48f, 0.80f);
            const Color sandSide(0.54f, 0.46f, 0.32f, 0.75f);
            drawIsometricBlock(iso.x, iso.y + 1.5f + tide * 0.1f,
                               7.0f * isletScale, 4.5f * isletScale,
                               1.8f * isletScale, sandTop, sandSide);
            drawFilledCircle(iso.x + 0.8f * isletScale,
                             iso.y - 1.5f * isletScale + tide * 0.1f,
                             1.5f * isletScale,
                             Color(0.20f, 0.50f, 0.28f, 0.68f));
        } else if (variant == 1) {
            const Color rockTop(0.42f, 0.40f, 0.38f, 0.82f);
            const Color rockSide(0.28f, 0.26f, 0.24f, 0.78f);
            drawIsometricBlock(iso.x, iso.y + 1.0f + tide * 0.08f,
                               5.0f * isletScale, 3.5f * isletScale,
                               2.5f * isletScale, rockTop, rockSide);
            drawFilledCircle(iso.x - 0.8f * isletScale, iso.y - 1.0f * isletScale,
                             0.9f * isletScale, Color(0.18f, 0.40f, 0.25f, 0.32f));
        } else if (variant == 2) {
            drawDiamond(iso.x, iso.y + 1.0f + tide * 0.12f,
                        8.0f * isletScale, 4.0f * isletScale,
                        Color(0.08f, 0.30f, 0.40f, 0.35f));
            drawDiamond(iso.x, iso.y + 0.5f + tide * 0.1f,
                        5.5f * isletScale, 2.8f * isletScale,
                        Color(0.12f, 0.38f, 0.44f, 0.28f));
        } else if (variant == 3) {
            drawDiamond(iso.x, iso.y + 0.8f + tide * 0.12f,
                        7.8f * isletScale, 3.8f * isletScale,
                        Color(0.10f, 0.28f, 0.36f, 0.28f));
            drawDiamondOutline(iso.x, iso.y + 0.8f + tide * 0.10f,
                               8.8f * isletScale, 4.4f * isletScale,
                               Color(0.70f, 0.90f, 0.92f, 0.08f), 0.9f);
            for (int buoy = 0; buoy < 3; buoy++) {
                const float angle = static_cast<float>(buoy) * 2.094f;
                const float bx = iso.x + std::cos(angle) * 4.0f * isletScale;
                const float by = iso.y + std::sin(angle) * 2.0f * isletScale;
                drawLine(bx, by + 2.0f * isletScale, bx, by - 0.3f * isletScale,
                         Color(0.76f, 0.78f, 0.72f, 0.52f), 0.9f);
                drawFilledCircle(bx, by - 0.5f * isletScale, 0.45f * isletScale,
                                 Color(0.92f, 0.64f, 0.24f, 0.58f));
            }
        } else {
            drawDiamond(iso.x + 0.4f * isletScale, iso.y + 1.4f * isletScale + tide * 0.12f,
                        9.4f * isletScale, 4.4f * isletScale,
                        Color(0.08f, 0.26f, 0.34f, 0.22f));
            drawLine(iso.x - 5.4f * isletScale, iso.y - 1.6f * isletScale,
                     iso.x + 5.8f * isletScale, iso.y + 2.2f * isletScale,
                     Color(0.80f, 0.92f, 0.88f, 0.22f), 1.0f);
            drawLine(iso.x - 2.2f * isletScale, iso.y + 3.4f * isletScale,
                     iso.x + 3.8f * isletScale, iso.y + 1.0f * isletScale,
                     Color(0.80f, 0.92f, 0.88f, 0.18f), 1.0f);
            drawLine(iso.x - 4.6f * isletScale, iso.y - 3.0f * isletScale,
                     iso.x - 4.6f * isletScale, iso.y - 7.0f * isletScale,
                     Color(0.78f, 0.78f, 0.72f, 0.62f), 0.9f);
            drawFilledCircle(iso.x - 4.6f * isletScale, iso.y - 7.4f * isletScale,
                             0.55f * isletScale,
                             Color(0.96f, 0.66f, 0.22f, 0.62f));
        }

        // Shore foam ring
        const float foamPulse = 0.5f + 0.5f *
            std::sin(animationTime * 0.9f + seed * 0.7f);
        drawDiamondOutline(iso.x, iso.y + 2.2f * isletScale + tide * 0.08f,
                           9.5f * isletScale * (0.97f + foamPulse * 0.03f),
                           4.8f * isletScale * (0.97f + foamPulse * 0.03f),
                           Color(0.86f, 0.96f, 1.0f, 0.06f + foamPulse * 0.05f), 1.0f);
    };

    for (int i = 0; i < kDecorIsletCount; i++) {
        const float rx = pseudoRandom01(i, 0, 777);
        const float ry = pseudoRandom01(i, 1, 777);
        const float wx = RenderUtils::lerp(gSceneBounds.minX - 0.8f,
                                            gSceneBounds.maxX + 0.8f, rx);
        const float wy = RenderUtils::lerp(gSceneBounds.minY - 0.8f,
                                            gSceneBounds.maxY + 0.8f, ry);
        drawAmbientIslet(wx, wy, i, 1.0f);
    }

    auto drawIsletNearNode = [&](int nodeId, float offsetX, float offsetY,
                                 float scaleBias, int seed,
                                 int variantOverride,
                                 float minNodeDistance) {
        const int idx = graph.findNodeIndex(nodeId);
        if (idx < 0) return;
        const WasteNode& node = graph.getNode(idx);
        drawAmbientIslet(node.getWorldX() + offsetX,
                         node.getWorldY() + offsetY,
                         seed, scaleBias, variantOverride, minNodeDistance);
    };

    auto drawLargeIslandNearNode = [&](int nodeId, float offsetX, float offsetY,
                                       float scaleBias, bool lushNorth,
                                       float seedPhase) {
        const int idx = graph.findNodeIndex(nodeId);
        if (idx < 0) return;
        const WasteNode& node = graph.getNode(idx);
        drawLargeCoastalIsland(node.getWorldX() + offsetX,
                               node.getWorldY() + offsetY,
                               scaleBias, lushNorth, seedPhase);
    };

    // Keep the decorative landmasses well away from the playable cleanup nodes.
    drawLargeIslandNearNode(1, -7.8f, -2.4f, 1.12f, true, 21.0f);
    drawLargeIslandNearNode(9, -7.2f,  2.8f, 1.24f, false, 24.0f);
}

// =====================================================================
//  Atmospheric effects — ambient life
// =====================================================================

void IsometricRenderer::drawAtmosphericEffects(const MapGraph& graph) {
    const float centerX = (gSceneBounds.minX + gSceneBounds.maxX) * 0.5f;
    const float centerY = (gSceneBounds.minY + gSceneBounds.maxY) * 0.5f;
    const IsoCoord centerIso = RenderUtils::worldToIso(centerX, centerY);
    const float paddedMinX = gSceneBounds.minX - kFieldPadding;
    const float paddedMaxX = gSceneBounds.maxX + kFieldPadding;
    const float paddedMinY = gSceneBounds.minY - kFieldPadding;
    const float paddedMaxY = gSceneBounds.maxY + kFieldPadding;

    // Distant birds — simple V shapes
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

        drawLine(bx - wingSpan, by + wingY, bx, by - 0.6f,
                 Color(0.15f, 0.15f, 0.18f, alpha), 1.0f);
        drawLine(bx, by - 0.6f, bx + wingSpan, by + wingY,
                 Color(0.15f, 0.15f, 0.18f, alpha), 1.0f);
    }

    // Gentle shoreline foam pulses around all nodes
    for (const auto& node : graph.getNodes()) {
        if (node.isCollected() && !node.getIsHQ()) continue;

        const IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
        const float nodeScale = node.getIsHQ() ? 1.8f : 1.0f;
        const float foamPhase = animationTime * 0.6f + node.getWorldX() * 0.3f;
        const float foamAlpha = 0.025f + 0.02f * std::sin(foamPhase * 1.3f);

        for (int foam = 0; foam < 3; foam++) {
            const float localPhase = foamPhase + foam * 2.1f;
            const float ox = std::cos(localPhase) * (12.0f + foam * 3.0f) * nodeScale;
            const float oy = std::sin(localPhase * 1.3f) * (4.0f + foam * 1.1f) * nodeScale;
            drawLine(iso.x + ox - 3.2f * nodeScale,
                     iso.y + oy + 5.4f * nodeScale,
                     iso.x + ox + 3.6f * nodeScale,
                     iso.y + oy + 6.6f * nodeScale,
                     Color(0.80f, 0.94f, 1.0f, foamAlpha * (1.0f - foam * 0.18f)), 1.0f);
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
        if (minDist < 3.8f) continue;

        const IsoCoord markerIso = RenderUtils::worldToIso(mx, my);
        const float bob = std::sin(animationTime * (0.8f + marker * 0.1f) + marker) * 0.5f;
        drawLine(markerIso.x, markerIso.y - 6.0f + bob,
                 markerIso.x, markerIso.y - 0.8f + bob,
                 Color(0.70f, 0.74f, 0.72f, 0.48f), 0.9f);
        drawFilledCircle(markerIso.x, markerIso.y - 1.2f + bob, 1.0f,
                         Color(0.94f, 0.70f, 0.24f, 0.32f));
        drawDiamond(markerIso.x, markerIso.y + 2.2f + bob,
                    3.6f, 1.4f, Color(0.62f, 0.88f, 0.94f, 0.05f));
    }

    for (int edge = 0; edge < 3; edge++) {
        const float wy = paddedMaxY + 2.5f + edge * 0.4f;
        const float waveShift = std::sin(animationTime * (0.15f + edge * 0.04f) + edge) * 0.5f;
        const IsoCoord a = RenderUtils::worldToIso(paddedMinX - 1.0f, wy + waveShift);
        const IsoCoord b = RenderUtils::worldToIso(paddedMaxX + 1.0f, wy + waveShift);
        drawLine(a.x, a.y, b.x, b.y,
                 Color(0.50f, 0.80f, 0.92f, 0.03f + edge * 0.01f), 1.0f);
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
                              Color(0.98f, 0.93f, 0.78f, 0.045f),
                              Color(0.80f, 0.84f, 0.76f, 0.012f));
        drawOrganicWorldPatch(hq.getWorldX() + 1.4f, hq.getWorldY() - 0.8f,
                              7.4f, 4.8f, -0.22f, 0.20f, animationTime, 14.1f,
                              Color(0.34f, 0.74f, 0.84f, 0.026f),
                              Color(0.18f, 0.42f, 0.52f, 0.008f));
        drawOrganicWorldPatch(centerX, centerY,
                              (paddedMaxX - paddedMinX) * 0.20f,
                              (paddedMaxY - paddedMinY) * 0.14f,
                              -0.16f, 0.18f, animationTime, 12.6f,
                              Color(0.56f, 0.78f, 0.84f, 0.024f),
                              Color(0.34f, 0.58f, 0.68f, 0.010f));
    }

    glBegin(GL_QUADS);
    glColor4f(0.01f, 0.03f, 0.08f, 0.12f);
    glVertex2f(left, top);
    glColor4f(0.01f, 0.03f, 0.08f, 0.00f);
    glVertex2f(right, top);
    glColor4f(0.01f, 0.03f, 0.08f, 0.00f);
    glVertex2f(right, top + 140.0f);
    glColor4f(0.01f, 0.03f, 0.08f, 0.08f);
    glVertex2f(left, top + 140.0f);
    glEnd();

    glBegin(GL_QUADS);
    glColor4f(0.01f, 0.03f, 0.08f, 0.00f);
    glVertex2f(left, bottom - 120.0f);
    glColor4f(0.01f, 0.03f, 0.08f, 0.00f);
    glVertex2f(right, bottom - 120.0f);
    glColor4f(0.01f, 0.03f, 0.08f, 0.10f);
    glVertex2f(right, bottom);
    glColor4f(0.01f, 0.03f, 0.08f, 0.12f);
    glVertex2f(left, bottom);
    glEnd();

    drawWorldQuadPatch(paddedMinX - 4.5f, paddedMinY - 4.5f,
                       paddedMinX + 0.4f, paddedMaxY + 4.5f,
                       Color(0.01f, 0.03f, 0.08f, 0.07f));
    drawWorldQuadPatch(paddedMaxX - 0.4f, paddedMinY - 4.5f,
                       paddedMaxX + 4.5f, paddedMaxY + 4.5f,
                       Color(0.01f, 0.03f, 0.08f, 0.09f));
}

// =====================================================================
//  Low-level OpenGL shape drawing
// =====================================================================

void IsometricRenderer::drawFilledCircle(float cx, float cy, float radius,
                                          const Color& color) {
    const int segments = 24;
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * static_cast<float>(M_PI) * i / segments;
        glVertex2f(cx + radius * std::cos(angle),
                   cy + radius * std::sin(angle));
    }
    glEnd();
}

void IsometricRenderer::drawRing(float cx, float cy, float radius,
                                  const Color& color, float thickness) {
    const int segments = 24;
    glColor4f(color.r, color.g, color.b, color.a);
    glLineWidth(thickness);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; i++) {
        float angle = 2.0f * static_cast<float>(M_PI) * i / segments;
        glVertex2f(cx + radius * std::cos(angle),
                   cy + radius * std::sin(angle));
    }
    glEnd();
    glLineWidth(1.0f);
}

void IsometricRenderer::drawLine(float x1, float y1, float x2, float y2,
                                  const Color& color, float width) {
    glLineWidth(width);
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
    glLineWidth(1.0f);
}

void IsometricRenderer::drawDiamond(float cx, float cy, float w, float h,
                                     const Color& color) {
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    glVertex2f(cx, cy - h);
    glVertex2f(cx + w, cy);
    glVertex2f(cx, cy + h);
    glVertex2f(cx - w, cy);
    glEnd();
}

void IsometricRenderer::drawDiamondOutline(float cx, float cy, float w, float h,
                                            const Color& color, float width) {
    glLineWidth(width);
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_LINE_LOOP);
    glVertex2f(cx, cy - h);
    glVertex2f(cx + w, cy);
    glVertex2f(cx, cy + h);
    glVertex2f(cx - w, cy);
    glEnd();
    glLineWidth(1.0f);
}

void IsometricRenderer::drawTileStripe(float cx, float cy, float w, float h,
                                        bool alongX, float offset, float extent,
                                        const Color& color, float width) {
    const float dirX = alongX ? w * 0.55f : -w * 0.55f;
    const float dirY = h * 0.55f;
    const float perpX = w * 0.17f * offset;
    const float perpY = (alongX ? -h : h) * 0.17f * offset;

    drawLine(cx + perpX - dirX * extent,
             cy + perpY - dirY * extent,
             cx + perpX + dirX * extent,
             cy + perpY + dirY * extent,
             color, width);
}

void IsometricRenderer::drawIsometricBlock(float cx, float cy,
                                            float w, float h, float depth,
                                            const Color& topColor,
                                            const Color& sideColor) {
    drawDiamond(cx, cy - depth, w * 0.5f, h * 0.5f, topColor);

    Color rightSide(sideColor.r * 0.85f, sideColor.g * 0.85f,
                    sideColor.b * 0.85f, sideColor.a);
    glColor4f(rightSide.r, rightSide.g, rightSide.b, rightSide.a);
    glBegin(GL_QUADS);
    glVertex2f(cx, cy - depth + h * 0.5f);
    glVertex2f(cx + w * 0.5f, cy - depth);
    glVertex2f(cx + w * 0.5f, cy);
    glVertex2f(cx, cy + h * 0.5f);
    glEnd();

    glColor4f(sideColor.r, sideColor.g, sideColor.b, sideColor.a);
    glBegin(GL_QUADS);
    glVertex2f(cx, cy - depth + h * 0.5f);
    glVertex2f(cx - w * 0.5f, cy - depth);
    glVertex2f(cx - w * 0.5f, cy);
    glVertex2f(cx, cy + h * 0.5f);
    glEnd();
}

// =====================================================================
//  Accessors
// =====================================================================

void IsometricRenderer::setRouteDrawProgress(int segments) {
    routeDrawProgress = segments;
}

int IsometricRenderer::getRouteDrawProgress() const {
    return routeDrawProgress;
}

void IsometricRenderer::setShowGrid(bool show) {
    showGrid = show;
}

void IsometricRenderer::resetAnimation() {
    routeDrawProgress = 0;
    animationTime = 0.0f;
}

void IsometricRenderer::cleanup() {
    oceanShader.cleanup();
}
