#include "visualization/IsometricRenderer.h"
#include "visualization/SeaThemeRenderer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

float clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

float clampColor(float value) {
    return clamp01(value);
}

Color withAlpha(const Color& color, float alpha) {
    return Color(color.r, color.g, color.b, clampColor(alpha));
}

} // anonymous namespace

// =====================================================================
//  Construction / destruction
// =====================================================================

IsometricRenderer::IsometricRenderer()
    : animationTime(0.0f), lastDeltaTime(0.0f),
      routeDrawProgress(0), showGrid(true) {
    themeRenderer = std::make_unique<SeaThemeRenderer>();
}

IsometricRenderer::~IsometricRenderer() {
    if (themeRenderer) {
        themeRenderer->cleanup();
    }
}

// =====================================================================
//  Main render — delegates theme-specific drawing
// =====================================================================

void IsometricRenderer::render(const MapGraph& graph, const Truck& truck,
                                const RouteResult& currentRoute, float deltaTime) {
    animationTime += deltaTime;
    lastDeltaTime = deltaTime;

    if (!themeRenderer) return;

    // Ground / background
    themeRenderer->drawGroundPlane(*this, graph, truck, currentRoute, animationTime);

    // Decorative background elements
    themeRenderer->drawDecorativeElements(*this, graph, animationTime);

    // Road connections (shared)
    drawRoadConnections(graph);

    // Route highlight (shared)
    if (!currentRoute.visitOrder.empty()) {
        drawRouteHighlight(graph, currentRoute, routeDrawProgress);
    }

    // Per-node rendering
    for (int i = 0; i < graph.getNodeCount(); i++) {
        const WasteNode& node = graph.getNode(i);
        if (node.getIsHQ()) {
            themeRenderer->drawHQNode(*this, node, animationTime);
        } else {
            themeRenderer->drawWasteNode(*this, node, animationTime);
        }
    }

    // Truck / vehicle
    if (truck.isMoving() || currentRoute.isValid()) {
        themeRenderer->drawTruck(*this, graph, truck, currentRoute, animationTime);
    }

    // Atmospheric effects
    themeRenderer->drawAtmosphericEffects(*this, graph, animationTime);
}

// =====================================================================
//  Theme switching
// =====================================================================

void IsometricRenderer::setTheme(std::unique_ptr<IThemeRenderer> theme) {
    if (themeRenderer) {
        themeRenderer->cleanup();
    }
    themeRenderer = std::move(theme);
    if (themeRenderer) {
        themeRenderer->init();
    }
}

// =====================================================================
//  Road connections (shared across all themes)
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
//  Route highlight — layered premium path rendering (shared)
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
//  Low-level drawing primitives (shared across all themes)
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
    if (themeRenderer) {
        themeRenderer->cleanup();
    }
}
