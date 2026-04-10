#ifndef ITHEME_RENDERER_H
#define ITHEME_RENDERER_H

#include "core/MapGraph.h"
#include "core/Truck.h"
#include "core/RouteResult.h"

class IsometricRenderer;

// Abstract interface for theme-specific rendering.
// Each theme (sea, city, etc.) implements its own visual style
// for ground, waste nodes, HQ, truck, and ambient effects.
class IThemeRenderer {
public:
    virtual ~IThemeRenderer() = default;

    // Called once at startup or when switching themes
    virtual bool init() = 0;

    // Draw the ground plane (ocean, asphalt, etc.)
    virtual void drawGroundPlane(IsometricRenderer& renderer,
                                 const MapGraph& graph,
                                 const Truck& truck,
                                 const RouteResult& currentRoute,
                                 float animationTime) = 0;

    // Draw a single waste collection node
    virtual void drawWasteNode(IsometricRenderer& renderer,
                               const WasteNode& node,
                               float animationTime) = 0;

    // Draw the headquarters / depot node
    virtual void drawHQNode(IsometricRenderer& renderer,
                            const WasteNode& node,
                            float animationTime) = 0;

    // Draw the truck / vehicle
    virtual void drawTruck(IsometricRenderer& renderer,
                           const MapGraph& graph,
                           const Truck& truck,
                           const RouteResult& currentRoute,
                           float animationTime) = 0;

    // Draw decorative background elements (islands, buildings, etc.)
    virtual void drawDecorativeElements(IsometricRenderer& renderer,
                                        const MapGraph& graph,
                                        float animationTime) = 0;

    // Draw atmospheric effects (birds, clouds, traffic, etc.)
    virtual void drawAtmosphericEffects(IsometricRenderer& renderer,
                                        const MapGraph& graph,
                                        float animationTime) = 0;

    // Release GPU resources
    virtual void cleanup() = 0;
};

#endif // ITHEME_RENDERER_H
