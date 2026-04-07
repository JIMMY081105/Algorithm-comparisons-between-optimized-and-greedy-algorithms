#include "visualization/IsometricRenderer.h"

// We need the OpenGL headers for drawing
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

IsometricRenderer::IsometricRenderer()
    : animationTime(0.0f), routeDrawProgress(0), showGrid(true) {}

void IsometricRenderer::render(const MapGraph& graph, const Truck& truck,
                                const RouteResult& currentRoute, float deltaTime) {
    animationTime += deltaTime;

    // Draw layers from back to front to get correct visual overlap
    drawGroundPlane(graph);

    if (showGrid) {
        drawRoadConnections(graph);
    }

    // Draw the route highlight if we have a route loaded
    if (!currentRoute.visitOrder.empty()) {
        drawRouteHighlight(graph, currentRoute, routeDrawProgress);
    }

    // Draw all waste nodes
    for (int i = 0; i < graph.getNodeCount(); i++) {
        const WasteNode& node = graph.getNode(i);
        if (node.getIsHQ()) {
            drawHQNode(node);
        } else {
            drawWasteNode(node, animationTime);
        }
    }

    // Draw the truck on top of everything
    if (truck.isMoving() || currentRoute.isValid()) {
        drawTruck(truck);
    }
}

void IsometricRenderer::drawGroundPlane(const MapGraph& graph) {
    // Draw an isometric ground area as a series of diamond tiles.
    // This creates the visual "floor" that the map sits on.
    Color gridColor = RenderUtils::getGridLineColor();
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

    const int startX = static_cast<int>(std::floor(minWorldX)) - 2;
    const int endX = static_cast<int>(std::ceil(maxWorldX)) + 2;
    const int startY = static_cast<int>(std::floor(minWorldY)) - 2;
    const int endY = static_cast<int>(std::ceil(maxWorldY)) + 2;

    // Draw a large ground diamond covering the map area
    for (int gx = startX; gx <= endX; gx++) {
        for (int gy = startY; gy <= endY; gy++) {
            IsoCoord iso = RenderUtils::worldToIso(
                static_cast<float>(gx), static_cast<float>(gy));

            // Draw subtle grid tile outlines
            drawDiamond(iso.x, iso.y,
                       projection.tileWidth * 0.48f,
                       projection.tileHeight * 0.48f,
                       gridColor);
        }
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

    Color baseColor;
    float nodeRadius = 10.0f;

    if (node.isCollected()) {
        // Collected nodes are dimmed to show they've been serviced
        baseColor = RenderUtils::getCollectedColor();
        nodeRadius = 8.0f;
    } else {
        baseColor = RenderUtils::getUrgencyColor(node.getUrgency());

        // Urgent nodes pulse to draw attention
        if (node.getUrgency() == UrgencyLevel::HIGH) {
            float pulse = RenderUtils::getUrgencyPulse(time);
            baseColor.r *= pulse;
            baseColor.g *= pulse;
            baseColor.b *= pulse;
            nodeRadius = 10.0f + 2.0f * std::sin(time * 3.0f);
        }
    }

    // Draw the node as a pseudo-3D isometric block for visual depth
    Color sideColor(baseColor.r * 0.6f, baseColor.g * 0.6f,
                    baseColor.b * 0.6f, baseColor.a);
    drawIsometricBlock(iso.x, iso.y, nodeRadius * 1.8f, nodeRadius * 1.2f,
                       6.0f, baseColor, sideColor);

    // Draw a fill-level indicator ring around the node
    if (!node.isCollected() && !node.getIsHQ()) {
        float ringRadius = nodeRadius + 4.0f;
        Color ringColor = baseColor;
        ringColor.a = 0.4f;
        drawRing(iso.x, iso.y - 3.0f, ringRadius, ringColor, 1.5f);
    }
}

void IsometricRenderer::drawHQNode(const WasteNode& node) {
    IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
    Color hqColor = RenderUtils::getHQColor();
    Color sideColor(hqColor.r * 0.5f, hqColor.g * 0.5f, hqColor.b * 0.5f);

    // HQ is drawn larger and taller to stand out as the depot
    drawIsometricBlock(iso.x, iso.y, 28.0f, 18.0f, 12.0f, hqColor, sideColor);

    // Small roof accent
    Color accentColor(0.3f, 0.6f, 0.95f);
    drawDiamond(iso.x, iso.y - 12.0f, 10.0f, 6.0f, accentColor);
}

void IsometricRenderer::drawTruck(const Truck& truck) {
    // Draw the truck as a small colored shape at its current animated position
    IsoCoord iso = RenderUtils::worldToIso(truck.getPosX(), truck.getPosY());
    Color truckColor = RenderUtils::getTruckColor();
    Color truckSide(truckColor.r * 0.7f, truckColor.g * 0.7f, truckColor.b * 0.7f);

    drawIsometricBlock(iso.x, iso.y,
                       14.0f, 10.0f, 5.0f, truckColor, truckSide);

    // Add a small highlight on top to make it more visible
    Color highlight(1.0f, 1.0f, 1.0f, 0.5f);
    drawDiamond(iso.x, iso.y - 5.0f, 4.0f, 2.5f, highlight);
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
