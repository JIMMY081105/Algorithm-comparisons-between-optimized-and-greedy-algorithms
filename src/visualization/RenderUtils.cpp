#include "visualization/RenderUtils.h"
#include "core/MapGraph.h"
#include <algorithm>
#include <cmath>

namespace RenderUtils {

namespace {
constexpr float kSidebarWidth = 320.0f;
constexpr float kPanelMargin = 10.0f;
constexpr float kMapZoom = 1.12f;
constexpr float kMapVerticalBias = 0.40f;

ProjectionState gProjection{
    BASE_TILE_WIDTH * kMapZoom,
    BASE_TILE_HEIGHT * kMapZoom,
    450.0f,
    80.0f
};

struct WorldBounds {
    float minX;
    float maxX;
    float minY;
    float maxY;
};

WorldBounds getWorldBounds(const MapGraph& graph) {
    const auto& nodes = graph.getNodes();
    if (nodes.empty()) {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    WorldBounds bounds{
        nodes.front().getWorldX(), nodes.front().getWorldX(),
        nodes.front().getWorldY(), nodes.front().getWorldY()
    };

    for (const auto& node : nodes) {
        bounds.minX = std::min(bounds.minX, node.getWorldX());
        bounds.maxX = std::max(bounds.maxX, node.getWorldX());
        bounds.minY = std::min(bounds.minY, node.getWorldY());
        bounds.maxY = std::max(bounds.maxY, node.getWorldY());
    }

    return bounds;
}
}

void updateProjection(float viewportWidth, float viewportHeight,
                      const MapGraph& graph) {
    gProjection.tileWidth = BASE_TILE_WIDTH * kMapZoom;
    gProjection.tileHeight = BASE_TILE_HEIGHT * kMapZoom;

    const float halfW = gProjection.tileWidth * 0.5f;
    const float halfH = gProjection.tileHeight * 0.5f;
    const float sidebarLeft = viewportWidth - kSidebarWidth - kPanelMargin;
    const float contentLeft = kPanelMargin;
    const float contentRight = std::max(contentLeft, sidebarLeft - kPanelMargin);
    const float targetCenterX = (contentLeft + contentRight) * 0.5f;
    const float targetCenterY = viewportHeight * kMapVerticalBias;
    const WorldBounds bounds = getWorldBounds(graph);
    const float paddedMinX = bounds.minX - 2.0f;
    const float paddedMaxX = bounds.maxX + 2.0f;
    const float paddedMinY = bounds.minY - 2.0f;
    const float paddedMaxY = bounds.maxY + 2.0f;

    const IsoCoord corners[] = {
        {(paddedMinX - paddedMinY) * halfW, (paddedMinX + paddedMinY) * halfH},
        {(paddedMinX - paddedMaxY) * halfW, (paddedMinX + paddedMaxY) * halfH},
        {(paddedMaxX - paddedMinY) * halfW, (paddedMaxX + paddedMinY) * halfH},
        {(paddedMaxX - paddedMaxY) * halfW, (paddedMaxX + paddedMaxY) * halfH}
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
            return Color(0.18f, 0.78f, 0.96f);   // cyan-blue
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
    return Color(0.25f, 0.63f, 0.35f);  // eco green for the garbage truck body
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
