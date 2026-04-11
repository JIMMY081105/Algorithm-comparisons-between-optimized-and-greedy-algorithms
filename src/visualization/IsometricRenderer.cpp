#include "visualization/IsometricRenderer.h"

#include <glad/glad.h>

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

IsometricRenderer::IsometricRenderer()
    : animationTime(0.0f), lastDeltaTime(0.0f) {}

void IsometricRenderer::render(IThemeRenderer& themeRenderer,
                               const MapGraph& graph,
                               const Truck& truck,
                               const MissionPresentation* mission,
                               AnimationController::PlaybackState playbackState,
                               float routeRevealProgress,
                               float deltaTime) {
    animationTime += deltaTime;
    lastDeltaTime = deltaTime;

    themeRenderer.drawGroundPlane(*this, graph, truck, mission, animationTime);
    themeRenderer.drawDecorativeElements(*this, graph, animationTime);
    themeRenderer.drawTransitNetwork(*this, graph, mission, playbackState,
                                     routeRevealProgress, animationTime);

    for (int i = 0; i < graph.getNodeCount(); ++i) {
        const WasteNode& node = graph.getNode(i);
        if (node.getIsHQ()) {
            themeRenderer.drawHQNode(*this, node, animationTime);
        } else {
            themeRenderer.drawWasteNode(*this, node, animationTime);
        }
    }

    if (truck.isMoving() || (mission && mission->isValid())) {
        themeRenderer.drawTruck(*this, graph, truck, mission, animationTime);
    }

    themeRenderer.drawAtmosphericEffects(*this, graph, animationTime);
}

void IsometricRenderer::resetAnimation() {
    animationTime = 0.0f;
}

void IsometricRenderer::cleanup() {}

void IsometricRenderer::drawFilledCircle(float cx, float cy, float radius,
                                         const Color& color) {
    constexpr int kSegments = 24;
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= kSegments; ++i) {
        const float angle = 2.0f * static_cast<float>(M_PI) * i / kSegments;
        glVertex2f(cx + radius * std::cos(angle),
                   cy + radius * std::sin(angle));
    }
    glEnd();
}

void IsometricRenderer::drawRing(float cx, float cy, float radius,
                                 const Color& color, float thickness) {
    constexpr int kSegments = 24;
    glLineWidth(thickness);
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < kSegments; ++i) {
        const float angle = 2.0f * static_cast<float>(M_PI) * i / kSegments;
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

    const Color rightSide(sideColor.r * 0.85f, sideColor.g * 0.85f,
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
