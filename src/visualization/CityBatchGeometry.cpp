#include "visualization/CityBatchGeometry.h"

#include "visualization/IsometricRenderer.h"

#include <cmath>


void StaticCityBatch::upload(const std::vector<float>& verts) {
    constexpr GLsizei kStrideFloats = 6;
    vertexCount = static_cast<GLsizei>(verts.size() / kStrideFloats);

    if (vao == 0) {
        glGenVertexArrays(1, &vao);
    }
    if (vbo == 0) {
        glGenBuffers(1, &vbo);
    }

    GLint previousVAO = 0;
    GLint previousArrayBuffer = 0;
    const GLboolean previousVertexArray = glIsEnabled(GL_VERTEX_ARRAY);
    const GLboolean previousColorArray = glIsEnabled(GL_COLOR_ARRAY);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.empty() ? nullptr : verts.data(),
                 GL_STATIC_DRAW);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, kStrideFloats * sizeof(float), nullptr);
    glColorPointer(4, GL_FLOAT, kStrideFloats * sizeof(float),
                   reinterpret_cast<void*>(2 * sizeof(float)));

    glBindVertexArray(static_cast<GLuint>(previousVAO));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousArrayBuffer));
    if (previousVertexArray) {
        glEnableClientState(GL_VERTEX_ARRAY);
    } else {
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    if (previousColorArray) {
        glEnableClientState(GL_COLOR_ARRAY);
    } else {
        glDisableClientState(GL_COLOR_ARRAY);
    }
    dirty = false;
}

void StaticCityBatch::draw() const {
    if (vao == 0 || vertexCount <= 0) {
        return;
    }

    GLint previousProgram = 0;
    GLint previousVAO = 0;
    GLint previousArrayBuffer = 0;
    const GLboolean previousVertexArray = glIsEnabled(GL_VERTEX_ARRAY);
    const GLboolean previousColorArray = glIsEnabled(GL_COLOR_ARRAY);
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);

    glUseProgram(0);
    glBindVertexArray(vao);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);

    glBindVertexArray(static_cast<GLuint>(previousVAO));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousArrayBuffer));
    if (previousVertexArray) {
        glEnableClientState(GL_VERTEX_ARRAY);
    } else {
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    if (previousColorArray) {
        glEnableClientState(GL_COLOR_ARRAY);
    } else {
        glDisableClientState(GL_COLOR_ARRAY);
    }
    glUseProgram(static_cast<GLuint>(previousProgram));
}

void StaticCityBatch::destroy() {
    if (vbo != 0) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (vao != 0) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    vertexCount = 0;
    dirty = true;
}

namespace CityBatchGeometry {
namespace {

constexpr float kTwoPi = 6.28318530718f;
std::vector<float>* gStaticBatchVerts = nullptr;

void appendBatchVertex(float x, float y, const Color& color) {
    if (gStaticBatchVerts == nullptr) {
        return;
    }

    gStaticBatchVerts->push_back(x);
    gStaticBatchVerts->push_back(y);
    gStaticBatchVerts->push_back(color.r);
    gStaticBatchVerts->push_back(color.g);
    gStaticBatchVerts->push_back(color.b);
    gStaticBatchVerts->push_back(color.a);
}

} // namespace

bool isCapturingStaticBatch() {
    return gStaticBatchVerts != nullptr;
}

ScopedStaticBatchCapture::ScopedStaticBatchCapture(std::vector<float>& verts)
    : previous(gStaticBatchVerts) {
    gStaticBatchVerts = &verts;
}

ScopedStaticBatchCapture::~ScopedStaticBatchCapture() {
    gStaticBatchVerts = previous;
}

void appendBatchTriangle(const IsoCoord& a, const Color& ca,
                         const IsoCoord& b, const Color& cb,
                         const IsoCoord& c, const Color& cc) {
    appendBatchVertex(a.x, a.y, ca);
    appendBatchVertex(b.x, b.y, cb);
    appendBatchVertex(c.x, c.y, cc);
}

void appendBatchQuad(const std::array<IsoCoord, 4>& pts, const Color& color) {
    appendBatchTriangle(pts[0], color, pts[1], color, pts[2], color);
    appendBatchTriangle(pts[0], color, pts[2], color, pts[3], color);
}

void appendBatchGradientQuad(const std::array<IsoCoord, 4>& pts,
                             const std::array<Color, 4>& colors) {
    appendBatchTriangle(pts[0], colors[0], pts[1], colors[1], pts[2], colors[2]);
    appendBatchTriangle(pts[0], colors[0], pts[2], colors[2], pts[3], colors[3]);
}

void appendBatchDiamond(float cx, float cy, float w, float h, const Color& color) {
    appendBatchQuad({
        IsoCoord{cx, cy - h},
        IsoCoord{cx + w, cy},
        IsoCoord{cx, cy + h},
        IsoCoord{cx - w, cy}
    }, color);
}

void appendBatchLine(float x1, float y1, float x2, float y2,
                     const Color& color, float width) {
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0001f || color.a <= 0.001f || width <= 0.0f) {
        return;
    }

    const float halfWidth = width * 0.5f;
    const float nx = -dy / length * halfWidth;
    const float ny = dx / length * halfWidth;

    appendBatchQuad({
        IsoCoord{x1 + nx, y1 + ny},
        IsoCoord{x2 + nx, y2 + ny},
        IsoCoord{x2 - nx, y2 - ny},
        IsoCoord{x1 - nx, y1 - ny}
    }, color);
}

void appendBatchLineLoop(const std::array<IsoCoord, 4>& pts,
                         const Color& color,
                         float width) {
    for (int i = 0; i < 4; ++i) {
        const IsoCoord& start = pts[static_cast<std::size_t>(i)];
        const IsoCoord& end = pts[static_cast<std::size_t>((i + 1) % 4)];
        appendBatchLine(start.x, start.y, end.x, end.y, color, width);
    }
}

void appendBatchGradientEllipse(float cx, float cy,
                                float radiusX, float radiusY,
                                const Color& centerColor,
                                const Color& edgeColor,
                                int segments) {
    if (radiusX <= 0.0f || radiusY <= 0.0f || segments <= 0) {
        return;
    }

    const IsoCoord center{cx, cy};
    IsoCoord previous{
        cx + radiusX,
        cy
    };
    for (int i = 1; i <= segments; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const IsoCoord current{
            cx + std::cos(angle) * radiusX,
            cy + std::sin(angle) * radiusY
        };
        appendBatchTriangle(center, centerColor, previous, edgeColor, current, edgeColor);
        previous = current;
    }
}

void appendBatchGradientRingEllipse(float cx, float cy,
                                    float innerRadiusX, float innerRadiusY,
                                    float outerRadiusX, float outerRadiusY,
                                    const Color& innerColor,
                                    const Color& outerColor,
                                    int segments) {
    if (innerRadiusX <= 0.0f || innerRadiusY <= 0.0f ||
        outerRadiusX <= innerRadiusX || outerRadiusY <= innerRadiusY ||
        segments <= 0) {
        return;
    }

    for (int i = 0; i < segments; ++i) {
        const float a0 = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const float a1 = kTwoPi * static_cast<float>(i + 1) / static_cast<float>(segments);
        const float c0 = std::cos(a0);
        const float s0 = std::sin(a0);
        const float c1 = std::cos(a1);
        const float s1 = std::sin(a1);

        const IsoCoord i0{cx + c0 * innerRadiusX, cy + s0 * innerRadiusY};
        const IsoCoord o0{cx + c0 * outerRadiusX, cy + s0 * outerRadiusY};
        const IsoCoord i1{cx + c1 * innerRadiusX, cy + s1 * innerRadiusY};
        const IsoCoord o1{cx + c1 * outerRadiusX, cy + s1 * outerRadiusY};

        appendBatchTriangle(i0, innerColor, o0, outerColor, o1, outerColor);
        appendBatchTriangle(i0, innerColor, o1, outerColor, i1, innerColor);
    }
}

void drawOrAppendLine(IsometricRenderer& renderer,
                      float x1, float y1, float x2, float y2,
                      const Color& color, float width) {
    if (isCapturingStaticBatch()) {
        appendBatchLine(x1, y1, x2, y2, color, width);
        return;
    }
    renderer.drawLine(x1, y1, x2, y2, color, width);
}

void drawOrAppendDiamond(IsometricRenderer& renderer,
                         float cx, float cy, float w, float h,
                         const Color& color) {
    if (isCapturingStaticBatch()) {
        appendBatchDiamond(cx, cy, w, h, color);
        return;
    }
    renderer.drawDiamond(cx, cy, w, h, color);
}

void drawOrAppendDiamondOutline(IsometricRenderer& renderer,
                                float cx, float cy, float w, float h,
                                const Color& color, float width) {
    if (isCapturingStaticBatch()) {
        appendBatchLineLoop({
            IsoCoord{cx, cy - h},
            IsoCoord{cx + w, cy},
            IsoCoord{cx, cy + h},
            IsoCoord{cx - w, cy}
        }, color, width);
        return;
    }
    renderer.drawDiamondOutline(cx, cy, w, h, color, width);
}

} // namespace CityBatchGeometry

