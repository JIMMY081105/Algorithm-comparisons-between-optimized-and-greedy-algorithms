#ifndef ISOMETRIC_RENDERER_H
#define ISOMETRIC_RENDERER_H

#include "core/MapGraph.h"
#include "core/Truck.h"
#include "core/RouteResult.h"
#include "visualization/RenderUtils.h"
#include "visualization/IThemeRenderer.h"
#include <vector>
#include <memory>

// Handles all OpenGL drawing for the isometric map view.
// Theme-specific visuals (sea, city, etc.) are delegated to an
// IThemeRenderer implementation, while shared logic like road
// connections, route highlights, and low-level primitives live here.
class IsometricRenderer {
private:
    float animationTime;    // accumulated time for pulsing effects
    float lastDeltaTime;    // most recent delta for theme updates
    int routeDrawProgress;  // how many segments of the route to show
    bool showGrid;
    std::unique_ptr<IThemeRenderer> themeRenderer;

    // Shared drawing helpers (not theme-specific)
    void drawRoadConnections(const MapGraph& graph);
    void drawRouteHighlight(const MapGraph& graph, const RouteResult& route,
                            int segmentsToShow);

public:
    IsometricRenderer();
    ~IsometricRenderer();

    // Main render call — draws the full map scene
    void render(const MapGraph& graph, const Truck& truck,
                const RouteResult& currentRoute, float deltaTime);

    // Switch the active visual theme
    void setTheme(std::unique_ptr<IThemeRenderer> theme);

    // Control progressive route drawing
    void setRouteDrawProgress(int segments);
    int getRouteDrawProgress() const;

    // Toggle ground grid visibility
    void setShowGrid(bool show);

    // Reset visual state for a new route animation
    void resetAnimation();

    // Release GPU resources (call before GL context destruction)
    void cleanup();

    // Get accumulated animation time (for theme renderers)
    float getAnimationTime() const { return animationTime; }
    float getLastDeltaTime() const { return lastDeltaTime; }

    // Low-level shape drawing — public so theme renderers can use them
    void drawFilledCircle(float cx, float cy, float radius, const Color& color);
    void drawRing(float cx, float cy, float radius, const Color& color, float thickness);
    void drawLine(float x1, float y1, float x2, float y2,
                  const Color& color, float width);
    void drawDiamond(float cx, float cy, float w, float h, const Color& color);
    void drawDiamondOutline(float cx, float cy, float w, float h,
                            const Color& color, float width);
    void drawTileStripe(float cx, float cy, float w, float h, bool alongX,
                        float offset, float extent, const Color& color,
                        float width);
    void drawIsometricBlock(float cx, float cy, float w, float h,
                            float depth, const Color& topColor, const Color& sideColor);
};

#endif // ISOMETRIC_RENDERER_H
