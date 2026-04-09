#ifndef OCEAN_SHADER_H
#define OCEAN_SHADER_H

#include <glad/glad.h>

// GPU-accelerated ocean background renderer using GLSL shaders.
// Draws a full-screen quad with animated tropical water (waves, caustics,
// specular sparkle, depth variation, vignette, god rays).
class OceanShader {
public:
    OceanShader();
    ~OceanShader();

    bool init();
    void draw(float time, float width, float height);
    void cleanup();

private:
    GLuint program;
    GLuint vao;
    GLuint vbo;
    GLint timeLoc;
    GLint resolutionLoc;
    bool initialized;

    GLuint compileShader(GLenum type, const char* source);
    GLuint linkProgram(GLuint vert, GLuint frag);
};

#endif // OCEAN_SHADER_H
