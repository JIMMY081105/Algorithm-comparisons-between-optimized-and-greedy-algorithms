#ifndef CITY_BATCH_GEOMETRY_H
#define CITY_BATCH_GEOMETRY_H

#include "visualization/RenderUtils.h"

#include <glad/glad.h>

#include <array>
#include <vector>

class IsometricRenderer;

struct StaticCityBatch {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLsizei vertexCount = 0;
    bool dirty = true;

    void upload(const std::vector<float>& verts);
    void draw() const;
    void destroy();
};

namespace CityBatchGeometry {

class ScopedStaticBatchCapture {
public:
    explicit ScopedStaticBatchCapture(std::vector<float>& verts);
    ~ScopedStaticBatchCapture();

    ScopedStaticBatchCapture(const ScopedStaticBatchCapture&) = delete;
    ScopedStaticBatchCapture& operator=(const ScopedStaticBatchCapture&) = delete;

private:
    std::vector<float>* previous;
};

bool isCapturingStaticBatch();

void appendBatchTriangle(const IsoCoord& a, const Color& ca,
                         const IsoCoord& b, const Color& cb,
                         const IsoCoord& c, const Color& cc);
void appendBatchQuad(const std::array<IsoCoord, 4>& pts, const Color& color);
void appendBatchGradientQuad(const std::array<IsoCoord, 4>& pts,
                             const std::array<Color, 4>& colors);
void appendBatchDiamond(float cx, float cy, float w, float h, const Color& color);
void appendBatchLine(float x1, float y1, float x2, float y2,
                     const Color& color, float width);
void appendBatchLineLoop(const std::array<IsoCoord, 4>& pts,
                         const Color& color,
                         float width);
void appendBatchGradientEllipse(float cx, float cy,
                                float radiusX, float radiusY,
                                const Color& centerColor,
                                const Color& edgeColor,
                                int segments);
void appendBatchGradientRingEllipse(float cx, float cy,
                                    float innerRadiusX, float innerRadiusY,
                                    float outerRadiusX, float outerRadiusY,
                                    const Color& innerColor,
                                    const Color& outerColor,
                                    int segments);

void drawOrAppendLine(IsometricRenderer& renderer,
                      float x1, float y1, float x2, float y2,
                      const Color& color, float width);
void drawOrAppendDiamond(IsometricRenderer& renderer,
                         float cx, float cy, float w, float h,
                         const Color& color);
void drawOrAppendDiamondOutline(IsometricRenderer& renderer,
                                float cx, float cy, float w, float h,
                                const Color& color, float width);

} // namespace CityBatchGeometry

#endif // CITY_BATCH_GEOMETRY_H
