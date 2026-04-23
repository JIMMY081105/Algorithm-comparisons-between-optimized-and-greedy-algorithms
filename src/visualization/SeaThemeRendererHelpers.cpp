#include "visualization/SeaThemeRendererInternal.h"

namespace SeaRenderHelpers {

const RouteResult& routeFromMission(const MissionPresentation* mission) {
    static const RouteResult kEmptyRoute{};
    return mission ? mission->route : kEmptyRoute;
}

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

Color shiftColor(const Color& color, float delta, float alphaScale) {
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
        const float angle = 2.0f * kPi * static_cast<float>(i) /
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
            std::sin(t * frequency * 2.0f * kPi + phase) * amplitudeWorld +
            std::sin(t * frequency * 4.0f * kPi - phase * 0.62f) *
                amplitudeWorld * 0.35f;
        const float widthScale = 0.62f + 0.38f * std::sin(t * kPi);
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

} // namespace SeaRenderHelpers
