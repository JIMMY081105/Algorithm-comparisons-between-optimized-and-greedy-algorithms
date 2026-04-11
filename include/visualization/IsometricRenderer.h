#ifndef ISOMETRIC_RENDERER_H
#define ISOMETRIC_RENDERER_H

#include "core/MapGraph.h"
#include "core/Truck.h"
#include "environment/MissionPresentation.h"
#include "visualization/IThemeRenderer.h"
#include "visualization/RenderUtils.h"

// Shared OpenGL draw host. Theme renderers own scene composition; this class
// only tracks timing and exposes low-level primitives.
class IsometricRenderer {
private:
    float animationTime;
    float lastDeltaTime;

public:
    IsometricRenderer();

    void render(IThemeRenderer& themeRenderer,
                const MapGraph& graph,
                const Truck& truck,
                const MissionPresentation* mission,
                AnimationController::PlaybackState playbackState,
                float routeRevealProgress,
                float deltaTime);

    float getAnimationTime() const { return animationTime; }
    float getLastDeltaTime() const { return lastDeltaTime; }

    void resetAnimation();
    void cleanup();

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
