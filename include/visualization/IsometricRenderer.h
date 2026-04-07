#ifndef ISOMETRIC_RENDERER_H
#define ISOMETRIC_RENDERER_H

#include "core/MapGraph.h"
#include "core/Truck.h"
#include "core/RouteResult.h"
#include "visualization/RenderUtils.h"
#include <vector>

// Handles all OpenGL drawing for the isometric map view.
// This renders the ground plane, roads, waste nodes, route lines,
// and the animated truck. It uses immediate-mode OpenGL for simplicity,
// which is appropriate for this scale of project.
class IsometricRenderer {
private:
    float animationTime;    // accumulated time for pulsing effects
    int routeDrawProgress;  // how many segments of the route to show (for progressive drawing)
    bool showGrid;

    // Internal drawing helpers
    void drawGroundPlane(const MapGraph& graph);
    void drawRoadConnections(const MapGraph& graph);
    void drawRouteHighlight(const MapGraph& graph, const RouteResult& route,
                            int segmentsToShow);
    void drawWasteNode(const WasteNode& node, float time);
    void drawHQNode(const WasteNode& node);
    void drawTruck(const Truck& truck);
    void drawNodeLabel(const WasteNode& node);

    // Low-level shape drawing using OpenGL
    void drawFilledCircle(float cx, float cy, float radius, const Color& color);
    void drawRing(float cx, float cy, float radius, const Color& color, float thickness);
    void drawLine(float x1, float y1, float x2, float y2,
                  const Color& color, float width);
    void drawDiamond(float cx, float cy, float w, float h, const Color& color);
    void drawIsometricBlock(float cx, float cy, float w, float h,
                            float depth, const Color& topColor, const Color& sideColor);

public:
    IsometricRenderer();

    // Main render call — draws the full map scene
    void render(const MapGraph& graph, const Truck& truck,
                const RouteResult& currentRoute, float deltaTime);

    // Control progressive route drawing
    void setRouteDrawProgress(int segments);
    int getRouteDrawProgress() const;

    // Toggle ground grid visibility
    void setShowGrid(bool show);

    // Reset visual state for a new route animation
    void resetAnimation();
};

#endif // ISOMETRIC_RENDERER_H
