#ifndef RENDER_UTILS_H
#define RENDER_UTILS_H

#include "core/WasteNode.h"
#include <array>

class MapGraph;

// Color represented as RGBA floats (0.0 to 1.0)
struct Color {
    float r, g, b, a;
    Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
};

// Isometric coordinate after projection
struct IsoCoord {
    float x, y;
};

// Central place for visual constants and coordinate transformations.
// Keeps rendering code clean by separating math and theme from draw calls.
namespace RenderUtils {
    struct ProjectionState {
        float tileWidth;
        float tileHeight;
        float offsetX;
        float offsetY;
    };

    constexpr float BASE_TILE_WIDTH = 64.0f;
    constexpr float BASE_TILE_HEIGHT = 32.0f;

    // Convert world grid coordinates to isometric screen coordinates.
    // The classic isometric transform rotates the grid 45 degrees and
    // compresses the vertical axis by half.
    IsoCoord worldToIso(float worldX, float worldY);

    // Recompute the active projection for the current window size.
    void updateProjection(float viewportWidth, float viewportHeight,
                          const MapGraph& graph);
    const ProjectionState& getProjection();

    // Urgency color mapping for waste nodes
    Color getUrgencyColor(UrgencyLevel urgency);
    Color getCollectedColor();
    Color getHQColor();

    // Theme colors for the overall UI
    Color getBackgroundColor();
    Color getRoadColor();
    Color getTruckColor();
    Color getRouteHighlightColor();
    Color getGridLineColor();

    // Pulsing effect for urgent nodes — returns a brightness multiplier
    float getUrgencyPulse(float time);

    // Lerp between two positions (used for truck animation)
    float lerp(float a, float b, float t);

    // Smoothstep for nicer interpolation curves
    float smoothstep(float t);
}

#endif // RENDER_UTILS_H
