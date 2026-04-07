#include "visualization/IsometricRenderer.h"

// We need the OpenGL headers for drawing
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
constexpr float kFieldCellSize = 0.30f;
constexpr float kFieldPadding = 2.0f;
constexpr float kDepotClearRadius = 0.68f;
constexpr float kClearInnerRadius = 0.20f;
constexpr float kClearOuterRadius = 0.56f;

enum class BinSizeTier {
    SMALL,
    MEDIUM,
    LARGE
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

float pseudoRandom01(int x, int y, int salt) {
    const int hash = positiveMod(x * 157 + y * 313 + salt * 571, 997);
    return static_cast<float>(hash) / 996.0f;
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

BinSizeTier getBinSizeTier(float capacity) {
    if (capacity <= 320.0f) return BinSizeTier::SMALL;
    if (capacity <= 430.0f) return BinSizeTier::MEDIUM;
    return BinSizeTier::LARGE;
}

float getBinScale(BinSizeTier tier) {
    switch (tier) {
        case BinSizeTier::SMALL: return 1.76f;
        case BinSizeTier::MEDIUM: return 2.04f;
        case BinSizeTier::LARGE: return 2.32f;
        default: return 1.0f;
    }
}

Color getBinSizeColor(BinSizeTier tier) {
    switch (tier) {
        case BinSizeTier::SMALL:
            return Color(0.23f, 0.57f, 0.90f, 0.98f);
        case BinSizeTier::MEDIUM:
            return Color(0.96f, 0.68f, 0.18f, 0.98f);
        case BinSizeTier::LARGE:
            return Color(0.84f, 0.26f, 0.22f, 0.98f);
        default:
            return Color(0.45f, 0.55f, 0.60f, 0.98f);
    }
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

IsometricRenderer::IsometricRenderer()
    : animationTime(0.0f), routeDrawProgress(0), showGrid(true) {}

void IsometricRenderer::render(const MapGraph& graph, const Truck& truck,
                                const RouteResult& currentRoute, float deltaTime) {
    animationTime += deltaTime;

    // Draw layers from back to front to get correct visual overlap
    drawGroundPlane(graph, truck, currentRoute);

    // Draw the route highlight if we have a route loaded
    if (!currentRoute.visitOrder.empty()) {
        drawRouteHighlight(graph, currentRoute, routeDrawProgress);
    }

    // Draw all waste nodes
    for (int i = 0; i < graph.getNodeCount(); i++) {
        const WasteNode& node = graph.getNode(i);
        if (node.getIsHQ()) {
            if (!currentRoute.isValid()) {
                drawHQNode(node);
            }
        } else {
            drawWasteNode(node, animationTime);
        }
    }

    // Draw the truck on top of everything
    if (truck.isMoving() || currentRoute.isValid()) {
        drawTruck(graph, truck, currentRoute);
    }
}

void IsometricRenderer::drawGroundPlane(const MapGraph& graph, const Truck& truck,
                                         const RouteResult& currentRoute) {
    // Draw the field as animated corn tiles. Tiles the truck has traversed
    // are rendered as exposed soil to create the harvesting effect.
    const auto& projection = RenderUtils::getProjection();
    const auto& nodes = graph.getNodes();

    if (nodes.empty()) return;

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

    const int startX = static_cast<int>(
        std::floor((minWorldX - kFieldPadding) / kFieldCellSize));
    const int endX = static_cast<int>(
        std::ceil((maxWorldX + kFieldPadding) / kFieldCellSize));
    const int startY = static_cast<int>(
        std::floor((minWorldY - kFieldPadding) / kFieldCellSize));
    const int endY = static_cast<int>(
        std::ceil((maxWorldY + kFieldPadding) / kFieldCellSize));

    // Draw from back to front using x+y ordering so denser corn overlaps cleanly.
    for (int diagonal = startX + startY; diagonal <= endX + endY; diagonal++) {
        for (int gx = startX; gx <= endX; gx++) {
            const int gy = diagonal - gx;
            if (gy < startY || gy > endY) continue;

            const float worldX = static_cast<float>(gx) * kFieldCellSize;
            const float worldY = static_cast<float>(gy) * kFieldCellSize;
            IsoCoord iso = RenderUtils::worldToIso(worldX, worldY);
            const float clearAmount = getClearingAmount(worldX, worldY,
                                                        graph, truck, currentRoute);
            drawCornTile(iso.x, iso.y,
                         projection.tileWidth * kFieldCellSize * 0.56f,
                         projection.tileHeight * kFieldCellSize * 0.56f,
                         gx, gy, clearAmount);
        }
    }
}

void IsometricRenderer::drawCornTile(float cx, float cy, float w, float h,
                                      int gx, int gy, float clearAmount) {
    const float worldX = static_cast<float>(gx) * kFieldCellSize;
    const float worldY = static_cast<float>(gy) * kFieldCellSize;
    const float density = 1.0f - clearAmount;
    const float tint = (pseudoRandom01(gx, gy, 0) - 0.5f) * 0.06f;
    const float lushness = 0.55f + 0.45f * pseudoRandom01(gx, gy, 1);

    const Color canopyShadow = shiftColor(Color(0.03f, 0.10f, 0.07f, 0.78f), tint * 0.45f);
    const Color canopyBase = shiftColor(Color(0.05f, 0.22f, 0.13f, 0.96f), tint);
    const Color canopyTop = shiftColor(Color(0.09f, 0.34f, 0.18f, 0.98f), tint * 0.80f);
    const Color canopyHighlight = shiftColor(Color(0.19f, 0.50f, 0.25f, 0.84f), tint * 0.55f);
    const Color trampledBase(0.24f, 0.27f, 0.17f, 0.94f);
    const Color trampledHighlight(0.41f, 0.37f, 0.19f, 0.68f);
    const Color baseColor = mixColor(canopyBase, trampledBase, clearAmount * 0.86f);
    const Color topColor = mixColor(canopyTop, trampledHighlight, clearAmount * 0.60f);
    const Color stemColor = mixColor(Color(0.08f, 0.27f, 0.12f, 0.92f),
                                     Color(0.42f, 0.40f, 0.20f, 0.66f),
                                     clearAmount * 0.35f);
    const Color leafColor = mixColor(canopyHighlight, trampledHighlight, clearAmount * 0.48f);
    const Color stubbleColor(0.48f, 0.43f, 0.21f, 0.42f + clearAmount * 0.22f);

    drawDiamond(cx, cy + h * 0.12f, w * 1.04f, h * 0.98f, canopyShadow);
    drawDiamond(cx, cy + h * 0.02f, w * 0.98f, h * 0.82f, baseColor);
    drawDiamond(cx, cy - h * 0.08f, w * (0.82f + density * 0.12f),
                h * (0.58f + density * 0.08f), topColor);

    if (clearAmount > 0.16f) {
        drawTileStripe(cx, cy + h * 0.04f, w * 0.86f, h * 0.66f,
                       true, -0.24f, 0.36f, shiftColor(baseColor, -0.03f, 0.75f), 0.85f);
        drawTileStripe(cx, cy + h * 0.02f, w * 0.78f, h * 0.58f,
                       false, 0.20f, 0.28f, shiftColor(baseColor, -0.05f, 0.70f), 0.75f);
        if (clearAmount > 0.42f) {
            drawTileStripe(cx, cy - h * 0.02f, w * 0.62f, h * 0.46f,
                           true, 0.0f, 0.22f, stubbleColor, 0.95f);
        }
    }

    if (density <= 0.08f) {
        return;
    }

    const std::array<std::pair<float, float>, 3> bladeOffsets{{
        {-0.38f, 0.10f}, {-0.04f, -0.04f}, {0.32f, 0.12f}
    }};
    const float windBand = 0.55f +
                           0.45f * std::sin(animationTime * 0.85f +
                                            worldX * 0.32f - worldY * 0.21f);
    const float gust = 0.45f +
                       0.55f * std::sin(animationTime * 1.65f +
                                        worldX * 0.74f + worldY * 0.58f);
    const int bladeCount = 2 + positiveMod(gx + gy, 2);

    for (int i = 0; i < bladeCount; i++) {
        const float random = pseudoRandom01(gx + i * 11, gy - i * 7, 2);
        const float baseX = cx + bladeOffsets[i].first * w * (0.78f + lushness * 0.10f);
        const float baseY = cy + bladeOffsets[i].second * h * 0.74f;
        const float height = h * (1.45f + random * 0.78f) *
                             (0.78f + density * 0.34f);
        const float sway = std::sin(animationTime * (1.75f + random * 0.45f) +
                                    worldX * 0.92f + worldY * 0.71f +
                                    static_cast<float>(i) * 0.70f) *
                           w * (0.34f + 0.18f * gust) * density;
        const float shoulder = std::cos(animationTime * 2.15f +
                                        worldX * 0.45f - worldY * 0.33f +
                                        static_cast<float>(i) * 0.65f) *
                               w * 0.12f * density;
        const float topX = baseX + sway;
        const float topY = baseY - height * (0.90f + 0.10f * windBand);

        drawLine(baseX, baseY, topX, topY, stemColor, 1.15f + lushness * 0.20f);
        drawLine(baseX + sway * 0.16f, baseY - height * 0.42f,
                 topX - w * 0.16f + shoulder, baseY - height * 0.22f,
                 leafColor, 0.95f);
        drawLine(baseX + sway * 0.10f, baseY - height * 0.60f,
                 topX + w * 0.12f - shoulder, baseY - height * 0.36f,
                 shiftColor(leafColor, 0.04f, 0.88f), 0.90f);
    }
}

void IsometricRenderer::drawRoadConnections(const MapGraph& graph) {
    // Draw faint lines between all connected nodes to represent roads.
    // In our fully-connected graph this shows the road network.
    // We only draw connections shorter than a threshold to avoid visual clutter.
    Color roadColor = RenderUtils::getRoadColor();
    float maxVisibleDist = 48.75f;  // scaled for the wider explicit coordinate layout

    for (int i = 0; i < graph.getNodeCount(); i++) {
        for (int j = i + 1; j < graph.getNodeCount(); j++) {
            float dist = graph.getAdjacencyMatrix()[i][j];
            if (dist < maxVisibleDist) {
                const WasteNode& a = graph.getNode(i);
                const WasteNode& b = graph.getNode(j);
                IsoCoord isoA = RenderUtils::worldToIso(a.getWorldX(), a.getWorldY());
                IsoCoord isoB = RenderUtils::worldToIso(b.getWorldX(), b.getWorldY());

                // Fade roads by distance — closer roads are more visible
                float alpha = 1.0f - (dist / maxVisibleDist);
                Color fadedRoad = roadColor;
                fadedRoad.a *= alpha;
                drawLine(isoA.x, isoA.y, isoB.x, isoB.y, fadedRoad, 1.5f);
            }
        }
    }
}

void IsometricRenderer::drawRouteHighlight(const MapGraph& graph,
                                            const RouteResult& route,
                                            int segmentsToShow) {
    // Draw the route as a bright highlighted path.
    // Progressive drawing reveals one segment at a time during animation.
    Color routeColor = RenderUtils::getRouteHighlightColor();

    int maxSegments = static_cast<int>(route.visitOrder.size()) - 1;
    int drawCount = (segmentsToShow <= 0) ? maxSegments
                    : std::min(segmentsToShow, maxSegments);

    for (int i = 0; i < drawCount; i++) {
        int fromIdx = graph.findNodeIndex(route.visitOrder[i]);
        int toIdx = graph.findNodeIndex(route.visitOrder[i + 1]);

        if (fromIdx < 0 || toIdx < 0) continue;

        const WasteNode& from = graph.getNode(fromIdx);
        const WasteNode& to = graph.getNode(toIdx);

        IsoCoord isoFrom = RenderUtils::worldToIso(from.getWorldX(), from.getWorldY());
        IsoCoord isoTo = RenderUtils::worldToIso(to.getWorldX(), to.getWorldY());

        drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y, routeColor, 3.0f);
    }
}

void IsometricRenderer::drawWasteNode(const WasteNode& node, float time) {
    IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
    const BinSizeTier sizeTier = getBinSizeTier(node.getWasteCapacity());
    const float scale = getBinScale(sizeTier);
    Color bodyColor = getBinSizeColor(sizeTier);
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

    const float fillRatio = node.isCollected()
        ? 0.0f
        : clamp01(node.getWasteLevel() / 100.0f);
    drawWasteBinSprite(iso.x, iso.y - 1.0f, scale, bodyColor, accentColor,
                       node.isCollected(), fillRatio);

    if (!node.isCollected()) {
        float ringRadius = 8.6f * scale;
        float thickness = 1.2f;
        if (node.getUrgency() == UrgencyLevel::HIGH) {
            ringRadius += 0.9f * scale * std::sin(time * 3.0f);
            thickness = 1.6f;
        }
        Color ringColor = accentColor;
        ringColor.a = 0.44f;
        drawRing(iso.x, iso.y - 9.6f * scale, ringRadius, ringColor, thickness);
    }
}

void IsometricRenderer::drawHQNode(const WasteNode& node) {
    IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
    drawTruckSprite(iso.x, iso.y - 2.0f, 1.86f, RenderUtils::getTruckColor(),
                    1.0f, 0.0f, 0.0f);
}

void IsometricRenderer::drawTruck(const MapGraph& graph, const Truck& truck,
                                   const RouteResult& currentRoute) {
    // Draw the truck as a small colored shape at its current animated position
    IsoCoord iso = RenderUtils::worldToIso(truck.getPosX(), truck.getPosY());
    float headingX = 1.0f;
    float headingY = 0.0f;
    getTruckHeading(graph, truck, currentRoute, headingX, headingY);
    const float wheelSpin = truck.isMoving()
        ? animationTime * 14.0f +
          truck.getPosX() * 0.90f + truck.getPosY() * 0.65f
        : 0.0f;
    drawTruckSprite(iso.x, iso.y, 2.28f, RenderUtils::getTruckColor(),
                    headingX, headingY, wheelSpin);
}

void IsometricRenderer::drawTruckSprite(float cx, float cy, float scale,
                                         const Color& bodyColor,
                                         float headingX, float headingY,
                                         float wheelSpin) {
    const Color shadow(0.05f, 0.04f, 0.03f, 0.26f);
    const Color chassisTop(0.20f, 0.22f, 0.24f, 0.98f);
    const Color chassisLeft(0.10f, 0.11f, 0.13f, 0.98f);
    const Color chassisRight(0.14f, 0.15f, 0.17f, 0.98f);
    const Color chassisRear(0.08f, 0.09f, 0.11f, 0.98f);
    const Color bodyTop = shiftColor(bodyColor, 0.06f);
    const Color bodyLeft(bodyColor.r * 0.58f, bodyColor.g * 0.58f,
                         bodyColor.b * 0.58f, 1.0f);
    const Color bodyRight(bodyColor.r * 0.72f, bodyColor.g * 0.72f,
                          bodyColor.b * 0.72f, 1.0f);
    const Color bodyRear(bodyColor.r * 0.48f, bodyColor.g * 0.48f,
                         bodyColor.b * 0.48f, 1.0f);
    const Color cabTop(0.93f, 0.95f, 0.97f, 1.0f);
    const Color cabLeft(0.72f, 0.75f, 0.79f, 1.0f);
    const Color cabRight(0.82f, 0.85f, 0.88f, 1.0f);
    const Color cabRear(0.64f, 0.68f, 0.72f, 1.0f);
    const Color glass(0.57f, 0.84f, 1.0f, 0.95f);
    const Color glassSide(0.34f, 0.56f, 0.74f, 0.92f);
    const Color glassRear(0.26f, 0.46f, 0.62f, 0.92f);
    const Color trim(0.92f, 0.95f, 0.98f, 0.58f);
    const Color stripe(0.99f, 0.82f, 0.17f, 0.95f);
    const Color hazard(0.99f, 0.56f, 0.16f, 0.95f);
    const Color light(1.0f, 0.90f, 0.56f, 0.96f);
    const Color tailLight(0.96f, 0.24f, 0.20f, 0.96f);
    const Color wheel(0.11f, 0.12f, 0.14f, 0.98f);
    const Color rim(0.74f, 0.77f, 0.81f, 0.95f);
    const auto& projection = RenderUtils::getProjection();

    float forwardX = (headingX - headingY) * (projection.tileWidth * 0.5f);
    float forwardY = (headingX + headingY) * (projection.tileHeight * 0.5f);
    normalizeVector(forwardX, forwardY, 0.92f, 0.38f);

    const float rightX = -forwardY;
    const float rightY = forwardX;

    auto point = [&](float lateral, float longitudinal, float height = 0.0f) {
        return IsoCoord{
            cx + rightX * lateral * scale + forwardX * longitudinal * scale,
            cy + rightY * lateral * scale + forwardY * longitudinal * scale -
                height * scale
        };
    };

    auto drawQuadPts = [&](const std::array<IsoCoord, 4>& pts, const Color& color) {
        glColor4f(color.r, color.g, color.b, color.a);
        glBegin(GL_QUADS);
        for (const auto& pt : pts) {
            glVertex2f(pt.x, pt.y);
        }
        glEnd();
    };

    auto drawTriPts = [&](const std::array<IsoCoord, 3>& pts, const Color& color) {
        glColor4f(color.r, color.g, color.b, color.a);
        glBegin(GL_TRIANGLES);
        for (const auto& pt : pts) {
            glVertex2f(pt.x, pt.y);
        }
        glEnd();
    };

    auto drawPrism = [&](float left, float right,
                         float rear, float front,
                         float baseHeight, float topHeight,
                         const Color& topColor,
                         const Color& leftColor,
                         const Color& rightColor,
                         const Color& rearColor) {
        const IsoCoord bRearLeft = point(left, rear, baseHeight);
        const IsoCoord bRearRight = point(right, rear, baseHeight);
        const IsoCoord bFrontRight = point(right, front, baseHeight);
        const IsoCoord bFrontLeft = point(left, front, baseHeight);
        const IsoCoord tRearLeft = point(left, rear, topHeight);
        const IsoCoord tRearRight = point(right, rear, topHeight);
        const IsoCoord tFrontRight = point(right, front, topHeight);
        const IsoCoord tFrontLeft = point(left, front, topHeight);

        drawQuadPts({bRearLeft, bRearRight, tRearRight, tRearLeft}, rearColor);
        drawQuadPts({bRearRight, bFrontRight, tFrontRight, tRearRight}, rightColor);
        drawQuadPts({bFrontLeft, bRearLeft, tRearLeft, tFrontLeft}, leftColor);
        drawQuadPts({tRearLeft, tRearRight, tFrontRight, tFrontLeft}, topColor);
    };

    auto drawWheelAt = [&](float lateral, float longitudinal, float radius,
                           float spinPhase) {
        const IsoCoord center = point(lateral, longitudinal, 0.9f);
        drawFilledCircle(center.x, center.y, radius * scale, wheel);
        drawFilledCircle(center.x, center.y, radius * scale * 0.46f, rim);

        for (int i = 0; i < 4; i++) {
            const float angle = spinPhase + static_cast<float>(i) *
                                (2.0f * static_cast<float>(M_PI) / 4.0f);
            const float dx = std::cos(angle) * radius * scale * 0.68f;
            const float dy = std::sin(angle) * radius * scale * 0.68f;
            drawLine(center.x - dx, center.y - dy,
                     center.x + dx, center.y + dy,
                     shiftColor(rim, -0.24f), 1.2f);
        }
    };

    drawQuadPts({point(-9.2f, -8.4f), point(9.2f, -8.4f),
                 point(9.6f, 9.8f), point(-8.8f, 9.8f)}, shadow);

    drawPrism(-7.4f, 7.4f, -7.0f, 8.4f, 0.0f, 2.0f,
              chassisTop, chassisLeft, chassisRight, chassisRear);
    drawPrism(-6.6f, 6.6f, -5.8f, 2.8f, 2.0f, 10.8f,
              bodyTop, bodyLeft, bodyRight, bodyRear);
    drawPrism(-7.4f, -3.6f, -5.8f, 3.1f, 2.0f, 9.6f,
              shiftColor(bodyTop, -0.04f), shiftColor(bodyLeft, -0.08f),
              shiftColor(bodyRight, -0.12f), shiftColor(bodyRear, -0.08f));
    drawPrism(-4.8f, 4.8f, 2.4f, 8.4f, 2.0f, 8.7f,
              cabTop, cabLeft, cabRight, cabRear);
    drawPrism(-3.4f, 3.4f, 5.1f, 8.7f, 6.0f, 8.9f,
              glass, glassSide, shiftColor(glass, -0.10f), glassRear);
    drawPrism(-5.0f, 5.0f, -0.6f, 2.3f, 10.8f, 11.8f,
              trim, shiftColor(trim, -0.14f), shiftColor(trim, -0.06f),
              shiftColor(trim, -0.20f));
    drawPrism(-6.2f, 6.2f, -3.2f, -2.0f, 6.1f, 6.7f,
              stripe, shiftColor(stripe, -0.18f), shiftColor(stripe, -0.10f),
              shiftColor(stripe, -0.24f));
    drawPrism(-6.6f, 6.6f, 2.6f, 3.5f, 5.4f, 6.0f,
              stripe, shiftColor(stripe, -0.18f), shiftColor(stripe, -0.10f),
              shiftColor(stripe, -0.24f));
    drawPrism(-4.8f, 4.8f, 8.4f, 9.3f, 1.2f, 2.8f,
              shiftColor(chassisTop, 0.04f),
              shiftColor(chassisLeft, -0.02f),
              shiftColor(chassisRight, -0.01f),
              shiftColor(chassisRear, -0.04f));

    drawQuadPts({point(-6.7f, -4.8f, 4.1f), point(6.7f, -4.8f, 4.1f),
                 point(6.7f, -4.0f, 4.1f), point(-6.7f, -4.0f, 4.1f)}, stripe);
    drawQuadPts({point(-6.9f, -1.4f, 4.3f), point(6.9f, -1.4f, 4.3f),
                 point(6.9f, -0.6f, 4.3f), point(-6.9f, -0.6f, 4.3f)}, trim);

    const IsoCoord lightLeft = point(-2.2f, 9.4f, 3.8f);
    const IsoCoord lightRight = point(2.2f, 9.4f, 3.8f);
    const IsoCoord tailLeft = point(-4.4f, -6.2f, 3.9f);
    const IsoCoord tailRight = point(4.4f, -6.2f, 3.9f);
    const IsoCoord beaconRear = point(-0.8f, 1.0f, 12.4f);
    const IsoCoord beaconFront = point(1.2f, 2.0f, 12.2f);
    drawFilledCircle(lightLeft.x, lightLeft.y, 0.72f * scale, light);
    drawFilledCircle(lightRight.x, lightRight.y, 0.72f * scale, light);
    drawFilledCircle(tailLeft.x, tailLeft.y, 0.62f * scale, tailLight);
    drawFilledCircle(tailRight.x, tailRight.y, 0.62f * scale, tailLight);
    drawFilledCircle(beaconRear.x, beaconRear.y, 0.54f * scale, hazard);
    drawFilledCircle(beaconFront.x, beaconFront.y, 0.54f * scale, hazard);

    drawWheelAt(-7.1f, -4.8f, 1.46f, wheelSpin);
    drawWheelAt(7.1f, -4.8f, 1.46f, wheelSpin);
    drawWheelAt(-7.1f, 0.7f, 1.46f, wheelSpin + 0.32f);
    drawWheelAt(7.1f, 0.7f, 1.46f, wheelSpin + 0.32f);
    drawWheelAt(-7.1f, 6.2f, 1.46f, wheelSpin + 0.64f);
    drawWheelAt(7.1f, 6.2f, 1.46f, wheelSpin + 0.64f);
}

void IsometricRenderer::drawWasteBinSprite(float cx, float cy, float scale,
                                            const Color& bodyColor,
                                            const Color& accentColor,
                                            bool collected,
                                            float fillRatio) {
    const Color shadow(0.05f, 0.04f, 0.03f, 0.24f);
    const Color bodySide(bodyColor.r * 0.64f, bodyColor.g * 0.64f,
                         bodyColor.b * 0.64f, 1.0f);
    const Color rim = shiftColor(bodyColor, 0.10f);
    const Color cavity(0.08f, 0.09f, 0.10f, 0.98f);
    const Color fillTop = mixColor(Color(0.26f, 0.24f, 0.21f, 0.96f),
                                   accentColor, 0.60f);
    const Color fillSide(fillTop.r * 0.66f, fillTop.g * 0.66f,
                         fillTop.b * 0.66f, 0.98f);
    const Color metal(0.17f, 0.18f, 0.19f, 0.96f);
    const Color wheel(0.11f, 0.12f, 0.14f, 0.98f);
    const float clampedFill = collected ? 0.0f : clamp01(fillRatio);

    drawDiamond(cx, cy + 5.2f * scale, 8.8f * scale, 4.2f * scale, shadow);
    drawIsometricBlock(cx, cy + 0.8f * scale,
                       12.6f * scale, 10.2f * scale, 10.8f * scale,
                       bodyColor, bodySide);
    drawDiamond(cx, cy - 10.8f * scale,
                5.0f * scale, 2.5f * scale, cavity);
    drawDiamondOutline(cx, cy - 10.8f * scale,
                       5.3f * scale, 2.7f * scale,
                       shiftColor(rim, -0.05f, 0.96f), 1.2f);

    if (clampedFill > 0.02f) {
        const float topLift = 4.2f + clampedFill * 7.2f;
        const float fillScale = 0.60f + clampedFill * 0.32f;
        drawIsometricBlock(cx, cy + 0.9f * scale,
                           9.2f * scale * fillScale,
                           7.6f * scale * fillScale,
                           topLift * scale,
                           fillTop, fillSide);
    }

    drawDiamond(cx, cy - 12.6f * scale,
                1.9f * scale, 0.95f * scale, shiftColor(rim, 0.06f, 0.92f));
    drawLine(cx - 3.6f * scale, cy - 14.0f * scale,
             cx - 3.6f * scale, cy - 9.8f * scale, metal, 1.0f);
    drawLine(cx + 3.6f * scale, cy - 14.0f * scale,
             cx + 3.6f * scale, cy - 9.8f * scale, metal, 1.0f);
    drawLine(cx - 4.0f * scale, cy - 2.0f * scale,
             cx + 4.0f * scale, cy - 2.0f * scale,
             shiftColor(bodySide, -0.08f), 1.0f);
    drawFilledCircle(cx - 3.6f * scale, cy + 4.9f * scale,
                     1.10f * scale, wheel);
    drawFilledCircle(cx + 3.6f * scale, cy + 4.9f * scale,
                     1.10f * scale, wheel);
}

// ---- Low-level OpenGL shape drawing ----

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
    // A diamond is the isometric representation of a square tile
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    glVertex2f(cx, cy - h);      // top
    glVertex2f(cx + w, cy);      // right
    glVertex2f(cx, cy + h);      // bottom
    glVertex2f(cx - w, cy);      // left
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
    // Pseudo-3D block: a diamond on top with two side faces below it.
    // This gives objects a lifted, city-builder-style appearance.

    // Top face (diamond)
    drawDiamond(cx, cy - depth, w * 0.5f, h * 0.5f, topColor);

    // Right side face
    Color rightSide(sideColor.r * 0.85f, sideColor.g * 0.85f,
                    sideColor.b * 0.85f, sideColor.a);
    glColor4f(rightSide.r, rightSide.g, rightSide.b, rightSide.a);
    glBegin(GL_QUADS);
    glVertex2f(cx, cy - depth + h * 0.5f);     // top-right of diamond
    glVertex2f(cx + w * 0.5f, cy - depth);      // right corner
    glVertex2f(cx + w * 0.5f, cy);               // right corner + depth
    glVertex2f(cx, cy + h * 0.5f);              // bottom
    glEnd();

    // Left side face
    glColor4f(sideColor.r, sideColor.g, sideColor.b, sideColor.a);
    glBegin(GL_QUADS);
    glVertex2f(cx, cy - depth + h * 0.5f);     // bottom of diamond
    glVertex2f(cx - w * 0.5f, cy - depth);      // left corner
    glVertex2f(cx - w * 0.5f, cy);               // left corner + depth
    glVertex2f(cx, cy + h * 0.5f);              // bottom
    glEnd();
}

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
