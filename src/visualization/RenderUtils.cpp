#include "visualization/RenderUtils.h"
#include <algorithm>
#include <cmath>

namespace RenderUtils {

namespace {
constexpr float kSidebarWidth = 320.0f;
constexpr float kPanelMargin = 10.0f;
constexpr float kMapZoom = 1.12f;
constexpr float kMapVerticalBias = 0.30f;

ProjectionState gProjection{
    BASE_TILE_WIDTH * kMapZoom,
    BASE_TILE_HEIGHT * kMapZoom,
    450.0f,
    80.0f
};
}

void updateProjection(float viewportWidth, float viewportHeight) {
    gProjection.tileWidth = BASE_TILE_WIDTH * kMapZoom;
    gProjection.tileHeight = BASE_TILE_HEIGHT * kMapZoom;

    const float halfW = gProjection.tileWidth * 0.5f;
    const float halfH = gProjection.tileHeight * 0.5f;
    const float sidebarLeft = viewportWidth - kSidebarWidth - kPanelMargin;
    const float contentLeft = kPanelMargin;
    const float contentRight = std::max(contentLeft, sidebarLeft - kPanelMargin);
    const float targetCenterX = (contentLeft + contentRight) * 0.5f;
    const float targetCenterY = viewportHeight * kMapVerticalBias;

    const IsoCoord corners[] = {
        {(-1.0f - -1.0f) * halfW, (-1.0f + -1.0f) * halfH},
        {(-1.0f - 12.0f) * halfW, (-1.0f + 12.0f) * halfH},
        {(12.0f - -1.0f) * halfW, (12.0f + -1.0f) * halfH},
        {(12.0f - 12.0f) * halfW, (12.0f + 12.0f) * halfH}
    };

    float minX = corners[0].x;
    float maxX = corners[0].x;
    float minY = corners[0].y;
    float maxY = corners[0].y;

    for (const IsoCoord& corner : corners) {
        minX = std::min(minX, corner.x);
        maxX = std::max(maxX, corner.x);
        minY = std::min(minY, corner.y);
        maxY = std::max(maxY, corner.y);
    }

    // Expand the bounds to account for tile extents and the tallest blocks.
    minX -= halfW;
    maxX += halfW;
    minY -= halfH + 24.0f;
    maxY += halfH + 12.0f;

    const float rawCenterX = (minX + maxX) * 0.5f;
    const float rawCenterY = (minY + maxY) * 0.5f;

    gProjection.offsetX = targetCenterX - rawCenterX;
    gProjection.offsetY = targetCenterY - rawCenterY;
}

const ProjectionState& getProjection() {
    return gProjection;
}

IsoCoord worldToIso(float worldX, float worldY) {
    // Standard isometric projection:
    // Rotate the 2D grid 45 degrees and compress vertically by half.
    // This creates the classic "diamond" tile look used in city builders.
    IsoCoord iso;
    iso.x = (worldX - worldY) * (gProjection.tileWidth * 0.5f) + gProjection.offsetX;
    iso.y = (worldX + worldY) * (gProjection.tileHeight * 0.5f) + gProjection.offsetY;
    return iso;
}

Color getUrgencyColor(UrgencyLevel urgency) {
    switch (urgency) {
        case UrgencyLevel::LOW:
            return Color(0.30f, 0.78f, 0.35f);   // green
        case UrgencyLevel::MEDIUM:
            return Color(0.95f, 0.75f, 0.15f);   // yellow-amber
        case UrgencyLevel::HIGH:
            return Color(0.90f, 0.22f, 0.20f);   // red
        default:
            return Color(0.5f, 0.5f, 0.5f);
    }
}

Color getCollectedColor() {
    return Color(0.45f, 0.45f, 0.50f, 0.6f);  // dimmed grey
}

Color getHQColor() {
    return Color(0.20f, 0.50f, 0.85f);  // strong blue
}

Color getBackgroundColor() {
    return Color(0.12f, 0.14f, 0.18f);  // dark charcoal
}

Color getRoadColor() {
    return Color(0.35f, 0.35f, 0.40f, 0.5f);  // subtle grey
}

Color getTruckColor() {
    return Color(1.0f, 0.85f, 0.20f);  // bright yellow
}

Color getRouteHighlightColor() {
    return Color(0.0f, 0.85f, 1.0f, 0.8f);  // cyan glow
}

Color getGridLineColor() {
    return Color(0.22f, 0.24f, 0.28f, 0.4f);
}

float getUrgencyPulse(float time) {
    // Smooth sinusoidal pulse between 0.7 and 1.0 brightness
    // Used to make high-urgency nodes visually pulse on the map
    return 0.85f + 0.15f * std::sin(time * 4.0f);
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float smoothstep(float t) {
    // Standard Hermite smoothstep for nicer easing
    t = std::max(0.0f, std::min(1.0f, t));
    return t * t * (3.0f - 2.0f * t);
}

} // namespace RenderUtils
