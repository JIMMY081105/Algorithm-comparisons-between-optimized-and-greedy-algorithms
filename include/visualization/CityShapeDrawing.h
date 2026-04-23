#ifndef CITY_SHAPE_DRAWING_H
#define CITY_SHAPE_DRAWING_H

#include "environment/EnvironmentTypes.h"
#include "visualization/RenderUtils.h"

#include <array>

class IsometricRenderer;

namespace CityShapeDrawing {

struct BlockFaces {
    std::array<IsoCoord, 4> top;
    std::array<IsoCoord, 4> left;
    std::array<IsoCoord, 4> right;
};

struct TreePalette {
    Color shadow;
    Color trunkTop;
    Color trunkLeft;
    Color trunkRight;
    Color canopyTop;
    Color canopyLeft;
    Color canopyRight;
    Color canopyHighlight;
};

BlockFaces makeBlockFaces(float cx, float cy, float width, float depth, float height);
void drawImmediateQuad(const std::array<IsoCoord, 4>& pts, const Color& color);
void drawWorldQuadPatch(float minWorldX, float minWorldY,
                        float maxWorldX, float maxWorldY,
                        const Color& color);
void drawGradientEllipse(float cx, float cy,
                         float radiusX, float radiusY,
                         const Color& centerColor,
                         const Color& edgeColor,
                         int segments = 56);
void drawGradientRingEllipse(float cx, float cy,
                             float innerRadiusX, float innerRadiusY,
                             float outerRadiusX, float outerRadiusY,
                             const Color& innerColor,
                             const Color& outerColor,
                             int segments = 72);
void drawBlockMass(const BlockFaces& faces,
                   const Color& roofColor,
                   const Color& leftColor,
                   const Color& rightColor);
void drawMountainShape(float cx, float cy,
                       float halfW, float halfD, float peakHeight,
                       const Color& tier0Col, const Color& tier1Col,
                       const Color& tier2Col, const Color& tier3Col,
                       float snowAmt, const Color& snowCol);
void drawFaceBands(const std::array<IsoCoord, 4>& face,
                   int bandCount,
                   float inset,
                   const Color& color,
                   float width);
void drawOutlineLoop(const std::array<IsoCoord, 4>& pts, const Color& color, float width);
void applySeasonalTreeCanopy(CitySeason season,
                             bool snowCovered,
                             Color& canopyBase,
                             Color& canopyAccent);
void drawStylizedTree(IsometricRenderer& renderer,
                      float worldX, float worldY,
                      float zf,
                      float crownScale,
                      float trunkScale,
                      float swayX,
                      const TreePalette& palette);

} // namespace CityShapeDrawing

#endif // CITY_SHAPE_DRAWING_H
