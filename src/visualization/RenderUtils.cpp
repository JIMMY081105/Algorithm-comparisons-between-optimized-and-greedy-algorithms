#include "visualization/RenderUtils.h"
#include "core/MapGraph.h"
#include <algorithm>
#include <cmath>

namespace RenderUtils {

namespace {
constexpr float kSidebarWidth = 290.0f;
constexpr float kPanelMargin = 10.0f;
constexpr float kWorldPadding = 3.2f;
constexpr float kTallStructureAllowanceTop = 112.0f;
constexpr float kTallStructureAllowanceBottom = 30.0f;

ProjectionState gProjection{
    BASE_TILE_WIDTH,
    BASE_TILE_HEIGHT,
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
    if (graph.getNodes().empty()) {
        gProjection.tileWidth = BASE_TILE_WIDTH;
        gProjection.tileHeight = BASE_TILE_HEIGHT;
        gProjection.offsetX = viewportWidth * 0.5f;
        gProjection.offsetY = viewportHeight * 0.5f;
        return;
    }

    const float sidebarLeft = viewportWidth - kSidebarWidth - kPanelMargin;
    const float contentLeft = kPanelMargin;
    const float contentRight = std::max(contentLeft, sidebarLeft - kPanelMargin);
    const float viewWidth = std::max(160.0f, contentRight - contentLeft);
    const float viewHeight = std::max(160.0f, viewportHeight);
    const WorldBounds bounds = getWorldBounds(graph);
    const float paddedMinX = bounds.minX - kWorldPadding;
    const float paddedMaxX = bounds.maxX + kWorldPadding;
    const float paddedMinY = bounds.minY - kWorldPadding;
    const float paddedMaxY = bounds.maxY + kWorldPadding;

    const float baseHalfW = BASE_TILE_WIDTH * 0.5f;
    const float baseHalfH = BASE_TILE_HEIGHT * 0.5f;

    const IsoCoord corners[] = {
        {(paddedMinX - paddedMinY) * baseHalfW, (paddedMinX + paddedMinY) * baseHalfH},
        {(paddedMinX - paddedMaxY) * baseHalfW, (paddedMinX + paddedMaxY) * baseHalfH},
        {(paddedMaxX - paddedMinY) * baseHalfW, (paddedMaxX + paddedMinY) * baseHalfH},
        {(paddedMaxX - paddedMaxY) * baseHalfW, (paddedMaxX + paddedMaxY) * baseHalfH}
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

    minX -= baseHalfW;
    maxX += baseHalfW;
    minY -= baseHalfH + kTallStructureAllowanceTop;
    maxY += baseHalfH + kTallStructureAllowanceBottom;

    const float worldWidth = std::max(1.0f, maxX - minX);
    const float worldHeight = std::max(1.0f, maxY - minY);
    const float mapZoom = std::min(viewWidth / worldWidth,
                                   viewHeight / worldHeight) * 0.9f;

    gProjection.tileWidth = BASE_TILE_WIDTH * mapZoom;
    gProjection.tileHeight = BASE_TILE_HEIGHT * mapZoom;

    const float halfW = gProjection.tileWidth * 0.5f;
    const float halfH = gProjection.tileHeight * 0.5f;
    const auto scaledIso = [&](float worldX, float worldY) {
        return IsoCoord{
            (worldX - worldY) * halfW,
            (worldX + worldY) * halfH
        };
    };

    const IsoCoord scaledCorners[] = {
        scaledIso(paddedMinX, paddedMinY),
        scaledIso(paddedMinX, paddedMaxY),
        scaledIso(paddedMaxX, paddedMinY),
        scaledIso(paddedMaxX, paddedMaxY)
    };

    minX = scaledCorners[0].x;
    maxX = scaledCorners[0].x;
    minY = scaledCorners[0].y;
    maxY = scaledCorners[0].y;
    for (const IsoCoord& corner : scaledCorners) {
        minX = std::min(minX, corner.x);
        maxX = std::max(maxX, corner.x);
        minY = std::min(minY, corner.y);
        maxY = std::max(maxY, corner.y);
    }

    const float targetCenterX = contentLeft + viewWidth * 0.5f;
    const float targetCenterY = viewportHeight * 0.5f;
    gProjection.offsetX = targetCenterX - (minX + maxX) * 0.5f;
    gProjection.offsetY = targetCenterY - (minY + maxY) * 0.5f;
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
    return Color(0.55f, 0.42f, 0.28f);  // dock/harbor brown
}

Color getBackgroundColor() {
    return Color(0.03f, 0.06f, 0.14f);  // deep ocean dark
}

Color getRoadColor() {
    return Color(0.20f, 0.35f, 0.50f, 0.45f);  // ocean lane markers
}

Color getTruckColor() {
    return Color(0.20f, 0.55f, 0.75f);  // boat hull blue
}

Color getRouteHighlightColor() {
    return Color(0.0f, 0.90f, 0.80f, 0.75f);  // aqua/teal glow
}

Color getGridLineColor() {
    return Color(0.10f, 0.18f, 0.28f, 0.35f);
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
