#include "visualization/CityShapeDrawing.h"

#include "visualization/CityBatchGeometry.h"
#include "visualization/CityRenderUtils.h"
#include "visualization/IsometricRenderer.h"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>

namespace CityShapeDrawing {

using CityBatchGeometry::appendBatchGradientEllipse;
using CityBatchGeometry::appendBatchGradientRingEllipse;
using CityBatchGeometry::appendBatchLine;
using CityBatchGeometry::appendBatchLineLoop;
using CityBatchGeometry::appendBatchQuad;
using CityBatchGeometry::appendBatchTriangle;
using CityBatchGeometry::isCapturingStaticBatch;
using CityRenderUtils::biasColor;
using CityRenderUtils::desaturateColor;
using CityRenderUtils::mixColor;
using CityRenderUtils::scaleColor;
using CityRenderUtils::withAlpha;

namespace {
constexpr float kTwoPi = 6.28318530718f;
}

IsoCoord lerpIso(const IsoCoord& a, const IsoCoord& b, float t) {
    return IsoCoord{
        RenderUtils::lerp(a.x, b.x, t),
        RenderUtils::lerp(a.y, b.y, t)
    };
}

void drawImmediateQuad(const std::array<IsoCoord, 4>& pts, const Color& color) {
    if (isCapturingStaticBatch()) {
        appendBatchQuad(pts, color);
        return;
    }

    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    for (const IsoCoord& pt : pts) {
        glVertex2f(pt.x, pt.y);
    }
    glEnd();
}

void drawWorldQuadPatch(float minWorldX, float minWorldY,
                        float maxWorldX, float maxWorldY,
                        const Color& color) {
    drawImmediateQuad({
        RenderUtils::worldToIso(minWorldX, minWorldY),
        RenderUtils::worldToIso(maxWorldX, minWorldY),
        RenderUtils::worldToIso(maxWorldX, maxWorldY),
        RenderUtils::worldToIso(minWorldX, maxWorldY)
    }, color);
}

void drawGradientEllipse(float cx, float cy,
                         float radiusX, float radiusY,
                         const Color& centerColor,
                         const Color& edgeColor,
                         int segments) {
    if (radiusX <= 0.0f || radiusY <= 0.0f) {
        return;
    }

    if (isCapturingStaticBatch()) {
        appendBatchGradientEllipse(cx, cy, radiusX, radiusY,
                                   centerColor, edgeColor, segments);
        return;
    }

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(centerColor.r, centerColor.g, centerColor.b, centerColor.a);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segments; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const float px = cx + std::cos(angle) * radiusX;
        const float py = cy + std::sin(angle) * radiusY;
        glColor4f(edgeColor.r, edgeColor.g, edgeColor.b, edgeColor.a);
        glVertex2f(px, py);
    }
    glEnd();
}

void drawGradientRingEllipse(float cx, float cy,
                             float innerRadiusX, float innerRadiusY,
                             float outerRadiusX, float outerRadiusY,
                             const Color& innerColor,
                             const Color& outerColor,
                             int segments) {
    if (innerRadiusX <= 0.0f || innerRadiusY <= 0.0f ||
        outerRadiusX <= innerRadiusX || outerRadiusY <= innerRadiusY) {
        return;
    }

    if (isCapturingStaticBatch()) {
        appendBatchGradientRingEllipse(cx, cy,
                                       innerRadiusX, innerRadiusY,
                                       outerRadiusX, outerRadiusY,
                                       innerColor, outerColor,
                                       segments);
        return;
    }

    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= segments; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const float cosAngle = std::cos(angle);
        const float sinAngle = std::sin(angle);

        glColor4f(innerColor.r, innerColor.g, innerColor.b, innerColor.a);
        glVertex2f(cx + cosAngle * innerRadiusX, cy + sinAngle * innerRadiusY);

        glColor4f(outerColor.r, outerColor.g, outerColor.b, outerColor.a);
        glVertex2f(cx + cosAngle * outerRadiusX, cy + sinAngle * outerRadiusY);
    }
    glEnd();
}


BlockFaces makeBlockFaces(float cx, float cy, float width, float depth, float height) {
    const float halfW = width * 0.5f;
    const float halfD = depth * 0.5f;
    const float roofY = cy - height;

    return BlockFaces{
        {
            IsoCoord{cx, roofY - halfD},
            IsoCoord{cx + halfW, roofY},
            IsoCoord{cx, roofY + halfD},
            IsoCoord{cx - halfW, roofY}
        },
        {
            IsoCoord{cx, roofY + halfD},
            IsoCoord{cx - halfW, roofY},
            IsoCoord{cx - halfW, cy},
            IsoCoord{cx, cy + halfD}
        },
        {
            IsoCoord{cx, roofY + halfD},
            IsoCoord{cx + halfW, roofY},
            IsoCoord{cx + halfW, cy},
            IsoCoord{cx, cy + halfD}
        }
    };
}

void drawBlockMass(const BlockFaces& faces,
                   const Color& roofColor,
                   const Color& leftColor,
                   const Color& rightColor) {
    drawImmediateQuad(faces.left, leftColor);
    drawImmediateQuad(faces.right, rightColor);
    drawImmediateQuad(faces.top, roofColor);
}

void drawMountainShape(float cx, float cy,
                       float halfW, float halfD, float peakHeight,
                       const Color& tier0Col, const Color& tier1Col,
                       const Color& tier2Col, const Color& tier3Col,
                       float snowAmt, const Color& snowCol) {
    // Clash of Clans style: 4 stacked tiers creating a layered, terraced mountain
    // Each tier is a diamond pyramid (4 triangular faces) at increasing height
    struct TierDef {
        float wScale;     // fraction of base halfW
        float dScale;     // fraction of base halfD
        float baseFrac;   // height fraction where tier base sits
        float peakFrac;   // height fraction where tier peak sits
    };

    const TierDef tiers[4] = {
        {1.00f, 1.00f, 0.00f, 0.32f},   // foothill base â€” wide, low
        {0.70f, 0.70f, 0.18f, 0.58f},   // lower rocky slope
        {0.44f, 0.44f, 0.40f, 0.82f},   // upper rocky face
        {0.22f, 0.22f, 0.65f, 1.00f},   // summit peak
    };

    const Color tierCols[4] = {tier0Col, tier1Col, tier2Col, tier3Col};

    if (isCapturingStaticBatch()) {
        for (int t = 0; t < 4; ++t) {
            const float tw = halfW * tiers[t].wScale;
            const float td = halfD * tiers[t].dScale;
            const float tierBaseY = cy - peakHeight * tiers[t].baseFrac;
            const float tierPeakY = cy - peakHeight * tiers[t].peakFrac;

            Color faceCol = tierCols[t];

            if (snowAmt > 0.0f) {
                float blend = 0.0f;
                if (t == 3)      blend = snowAmt * 0.88f;
                else if (t == 2) blend = snowAmt * 0.38f;
                else if (t == 1) blend = snowAmt * 0.10f;
                if (blend > 0.0f) faceCol = mixColor(faceCol, snowCol, blend);
            }

            const Color rightFace = scaleColor(faceCol, 1.14f);
            const Color leftFace  = scaleColor(faceCol, 0.76f);
            const Color backLeft  = scaleColor(faceCol, 0.56f);
            const Color backRight = scaleColor(faceCol, 0.64f);
            const Color tipCol    = (t == 3 && snowAmt > 0.3f)
                ? mixColor(scaleColor(faceCol, 1.30f), snowCol, snowAmt * 0.7f)
                : scaleColor(faceCol, 1.28f);

            const float northY = tierBaseY - td;
            const float southY = tierBaseY + td;
            const float westX  = cx - tw;
            const float eastX  = cx + tw;
            const IsoCoord north{cx, northY};
            const IsoCoord west{westX, tierBaseY};
            const IsoCoord east{eastX, tierBaseY};
            const IsoCoord south{cx, southY};
            const IsoCoord peak{cx, tierPeakY};

            appendBatchTriangle(north, backLeft, west, backLeft, peak, tipCol);
            appendBatchTriangle(north, backRight, east, backRight, peak, tipCol);
            appendBatchTriangle(west, leftFace, south, leftFace, peak, tipCol);
            appendBatchTriangle(east, rightFace, south, rightFace, peak, tipCol);

            Color edgeCol = scaleColor(faceCol, 0.48f);
            edgeCol.a = 0.45f;
            appendBatchLine(west.x, west.y, peak.x, peak.y, edgeCol, 1.0f);
            appendBatchLine(peak.x, peak.y, east.x, east.y, edgeCol, 1.0f);
            appendBatchLine(south.x, south.y, peak.x, peak.y, edgeCol, 1.0f);
        }
        return;
    }

    for (int t = 0; t < 4; ++t) {
        const float tw = halfW * tiers[t].wScale;
        const float td = halfD * tiers[t].dScale;
        const float tierBaseY = cy - peakHeight * tiers[t].baseFrac;
        const float tierPeakY = cy - peakHeight * tiers[t].peakFrac;

        Color faceCol = tierCols[t];

        // Snow coverage increases with altitude
        if (snowAmt > 0.0f) {
            float blend = 0.0f;
            if (t == 3)      blend = snowAmt * 0.88f;
            else if (t == 2) blend = snowAmt * 0.38f;
            else if (t == 1) blend = snowAmt * 0.10f;
            if (blend > 0.0f) faceCol = mixColor(faceCol, snowCol, blend);
        }

        // 3D face shading: right = sunlit, left = shadow, back = darkest
        const Color rightFace = scaleColor(faceCol, 1.14f);
        const Color leftFace  = scaleColor(faceCol, 0.76f);
        const Color backLeft  = scaleColor(faceCol, 0.56f);
        const Color backRight = scaleColor(faceCol, 0.64f);
        const Color tipCol    = (t == 3 && snowAmt > 0.3f)
            ? mixColor(scaleColor(faceCol, 1.30f), snowCol, snowAmt * 0.7f)
            : scaleColor(faceCol, 1.28f);

        // Diamond corner points
        const float northY = tierBaseY - td;
        const float southY = tierBaseY + td;
        const float westX  = cx - tw;
        const float eastX  = cx + tw;

        glBegin(GL_TRIANGLES);
        // Back-left face (North -> West -> Peak)
        glColor4f(backLeft.r, backLeft.g, backLeft.b, backLeft.a);
        glVertex2f(cx, northY);
        glColor4f(backLeft.r, backLeft.g, backLeft.b, backLeft.a);
        glVertex2f(westX, tierBaseY);
        glColor4f(tipCol.r, tipCol.g, tipCol.b, tipCol.a);
        glVertex2f(cx, tierPeakY);

        // Back-right face (North -> East -> Peak)
        glColor4f(backRight.r, backRight.g, backRight.b, backRight.a);
        glVertex2f(cx, northY);
        glColor4f(backRight.r, backRight.g, backRight.b, backRight.a);
        glVertex2f(eastX, tierBaseY);
        glColor4f(tipCol.r, tipCol.g, tipCol.b, tipCol.a);
        glVertex2f(cx, tierPeakY);

        // Front-left face (West -> South -> Peak)
        glColor4f(leftFace.r, leftFace.g, leftFace.b, leftFace.a);
        glVertex2f(westX, tierBaseY);
        glColor4f(leftFace.r, leftFace.g, leftFace.b, leftFace.a);
        glVertex2f(cx, southY);
        glColor4f(tipCol.r, tipCol.g, tipCol.b, tipCol.a);
        glVertex2f(cx, tierPeakY);

        // Front-right face (East -> South -> Peak)
        glColor4f(rightFace.r, rightFace.g, rightFace.b, rightFace.a);
        glVertex2f(eastX, tierBaseY);
        glColor4f(rightFace.r, rightFace.g, rightFace.b, rightFace.a);
        glVertex2f(cx, southY);
        glColor4f(tipCol.r, tipCol.g, tipCol.b, tipCol.a);
        glVertex2f(cx, tierPeakY);
        glEnd();

        // Edge outlines for crisp 3D definition (front ridges only)
        Color edgeCol = scaleColor(faceCol, 0.48f);
        edgeCol.a = 0.45f;
        glLineWidth(1.0f);
        glColor4f(edgeCol.r, edgeCol.g, edgeCol.b, edgeCol.a);
        glBegin(GL_LINE_STRIP);
        glVertex2f(westX, tierBaseY);
        glVertex2f(cx, tierPeakY);
        glVertex2f(eastX, tierBaseY);
        glEnd();
        glBegin(GL_LINES);
        glVertex2f(cx, southY);
        glVertex2f(cx, tierPeakY);
        glEnd();
    }
}

void drawFaceBands(const std::array<IsoCoord, 4>& face,
                   int bandCount,
                   float inset,
                   const Color& color,
                   float width) {
    if (bandCount <= 0 || color.a <= 0.001f) {
        return;
    }

    if (isCapturingStaticBatch()) {
        for (int i = 0; i < bandCount; ++i) {
            const float t = inset + (static_cast<float>(i) + 1.0f) *
                ((1.0f - inset * 2.0f) / static_cast<float>(bandCount + 1));
            const IsoCoord start = lerpIso(face[0], face[3], t);
            const IsoCoord end = lerpIso(face[1], face[2], t);
            appendBatchLine(start.x, start.y, end.x, end.y, color, width);
        }
        return;
    }

    glLineWidth(width);
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_LINES);
    for (int i = 0; i < bandCount; ++i) {
        const float t = inset + (static_cast<float>(i) + 1.0f) *
            ((1.0f - inset * 2.0f) / static_cast<float>(bandCount + 1));
        const IsoCoord start = lerpIso(face[0], face[3], t);
        const IsoCoord end = lerpIso(face[1], face[2], t);
        glVertex2f(start.x, start.y);
        glVertex2f(end.x, end.y);
    }
    glEnd();
    glLineWidth(1.0f);
}

void drawOutlineLoop(const std::array<IsoCoord, 4>& pts, const Color& color, float width) {
    if (isCapturingStaticBatch()) {
        appendBatchLineLoop(pts, color, width);
        return;
    }

    glLineWidth(width);
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_LINE_LOOP);
    for (const IsoCoord& pt : pts) {
        glVertex2f(pt.x, pt.y);
    }
    glEnd();
    glLineWidth(1.0f);
}

void applySeasonalTreeCanopy(CitySeason season,
                             bool snowCovered,
                             Color& canopyBase,
                             Color& canopyAccent) {
    switch (season) {
        case CitySeason::Summer:
            canopyBase = mixColor(canopyBase, Color(0.84f, 0.58f, 0.70f, canopyBase.a), 0.76f);
            canopyAccent = mixColor(canopyAccent, Color(0.98f, 0.78f, 0.86f, canopyAccent.a), 0.82f);
            break;
        case CitySeason::Autumn:
            canopyBase = mixColor(canopyBase, Color(0.60f, 0.34f, 0.12f, canopyBase.a), 0.74f);
            canopyAccent = mixColor(canopyAccent, Color(0.82f, 0.52f, 0.20f, canopyAccent.a), 0.78f);
            break;
        case CitySeason::Winter:
            canopyBase = mixColor(desaturateColor(canopyBase, 0.48f),
                                  Color(0.70f, 0.74f, 0.78f, canopyBase.a),
                                  snowCovered ? 0.30f : 0.14f);
            canopyAccent = mixColor(desaturateColor(canopyAccent, 0.38f),
                                    Color(0.86f, 0.90f, 0.94f, canopyAccent.a),
                                    snowCovered ? 0.36f : 0.18f);
            break;
        case CitySeason::Spring:
        default:
            break;
    }
}

void drawStylizedTree(IsometricRenderer& renderer,
                      float worldX, float worldY,
                      float zf,
                      float crownScale,
                      float trunkScale,
                      float swayX,
                      const TreePalette& palette) {
    const IsoCoord iso = RenderUtils::worldToIso(worldX, worldY);
    const float groundY = iso.y + 0.9f * zf;
    const float trunkWidth = std::max(0.9f * zf, (1.0f + crownScale * 0.28f) * zf);
    const float trunkDepth = std::max(0.7f * zf, (0.78f + crownScale * 0.18f) * zf);
    const float trunkHeight = (4.4f + trunkScale * 2.7f) * zf;
    const float canopyWidth = (4.6f + crownScale * 2.6f) * zf;
    const float canopyDepth = (2.6f + crownScale * 1.3f) * zf;
    const float canopyHeight = (4.2f + crownScale * 1.5f) * zf;
    const float shadowDepth = canopyDepth * 0.36f;

    renderer.drawDiamond(iso.x + swayX * 0.25f, groundY + shadowDepth,
                         canopyWidth * 0.78f, shadowDepth, palette.shadow);

    const BlockFaces trunk = makeBlockFaces(
        iso.x + swayX * 0.18f, groundY, trunkWidth, trunkDepth, trunkHeight);
    drawBlockMass(trunk, palette.trunkTop, palette.trunkLeft, palette.trunkRight);

    const float canopyBaseY = groundY - trunkHeight + 1.8f * zf;
    auto drawCanopyMass = [&](float offsetX, float offsetY,
                              float widthScale, float depthScale, float heightScale,
                              float highlightMix) {
        const float width = canopyWidth * widthScale;
        const float depth = canopyDepth * depthScale;
        const float height = canopyHeight * heightScale;
        const float cx = iso.x + swayX + offsetX;
        const float cy = canopyBaseY + offsetY;
        const BlockFaces canopy = makeBlockFaces(cx, cy, width, depth, height);
        drawBlockMass(
            canopy,
            mixColor(palette.canopyTop, palette.canopyHighlight, highlightMix),
            mixColor(palette.canopyLeft, palette.canopyHighlight, highlightMix * 0.35f),
            mixColor(palette.canopyRight, palette.canopyHighlight, highlightMix * 0.25f));

        const float roofY = cy - height;
        renderer.drawDiamond(
            cx + swayX * 0.08f, roofY,
            width * 0.14f, depth * 0.10f,
            withAlpha(palette.canopyHighlight, palette.canopyHighlight.a * (0.26f + highlightMix * 0.3f)));
    };

    drawCanopyMass(0.0f, 0.8f * zf, 1.00f, 1.00f, 1.00f, 0.08f);
    drawCanopyMass(-canopyWidth * 0.24f, 0.2f * zf, 0.72f, 0.80f, 0.90f, 0.04f);
    drawCanopyMass(canopyWidth * 0.24f, 0.1f * zf, 0.70f, 0.78f, 0.86f, 0.12f);
    drawCanopyMass(0.0f, -canopyHeight * 0.54f, 0.56f, 0.62f, 0.74f, 0.18f);
}

} // namespace CityShapeDrawing
