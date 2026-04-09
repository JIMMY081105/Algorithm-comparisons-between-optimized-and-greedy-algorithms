#include "visualization/OceanShader.h"
#include <iostream>
#include <string>

// =====================================================================
//  Embedded GLSL sources
// =====================================================================

namespace {

const char* kVertexSource = R"(
#version 330
layout(location = 0) in vec2 aPos;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* kFragmentSource = R"(
#version 330

uniform float u_time;
uniform vec2  u_resolution;

layout(location = 0) out vec4 fragColor;

// ---- noise utilities ----

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i),              hash(i + vec2(1.0, 0.0)), f.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x),
               f.y);
}

float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * vnoise(p);
        p = p * 2.03 + vec2(1.3, 1.7);
        a *= 0.49;
    }
    return v;
}

// ---- main ----

void main() {
    vec2  uv     = gl_FragCoord.xy / u_resolution;
    float t      = u_time;
    float aspect = u_resolution.x / u_resolution.y;
    vec2  co     = vec2(uv.x * aspect, uv.y);

    // ==== Color palette — bright tropical teal (Boom Beach style) ====
    vec3 deep    = vec3(0.01, 0.25, 0.30);   // deep trenches
    vec3 mid     = vec3(0.0,  0.45, 0.46);   // open ocean
    vec3 bright  = vec3(0.0,  0.63, 0.63);   // rgb(0,160,160) target
    vec3 surface = vec3(0.0,  0.78, 0.71);   // rgb(0,200,180) target

    // Nearly uniform bright base with very subtle gradient
    vec3 base = mix(bright, surface, 0.28);
    base = mix(base, mid,  (1.0 - uv.y) * 0.22);   // bottom slightly deeper
    base = mix(base, surface, uv.y * 0.10);          // top slightly brighter

    // ==== Large ocean swells ====
    float sw1 = sin(co.x * 2.8 + co.y * 1.4 + t * 0.13) * 0.5 + 0.5;
    float sw2 = sin(co.x * 1.6 - co.y * 2.6 + t * 0.10 + 1.8) * 0.5 + 0.5;
    base = mix(base, base * 1.14, (sw1 * 0.55 + sw2 * 0.45) * 0.15);

    // ==== Scrolling wave ripples — three directions ====
    float w1 = sin(dot(co, vec2( 0.82,  0.38)) * 15.0 + t * 0.55);
    float w2 = sin(dot(co, vec2(-0.34,  0.90)) * 12.0 + t * 0.40 + 1.5);
    float w3 = sin(dot(co, vec2( 0.62, -0.56)) * 20.0 + t * 0.70 + 3.0);
    w1 = pow(w1 * 0.5 + 0.5, 3.0);
    w2 = pow(w2 * 0.5 + 0.5, 3.2);
    w3 = pow(w3 * 0.5 + 0.5, 3.8);
    float waves = w1 * 0.40 + w2 * 0.35 + w3 * 0.25;
    base += vec3(0.04, 0.10, 0.07) * waves;

    // ==== Organic depth variation via noise ====
    vec2 drift = vec2(t * 0.014, t * 0.009);
    float dn = fbm((co + drift) * 2.8);
    base = mix(base, deep * 1.2, (1.0 - dn) * 0.12);
    base = mix(base, surface,     dn * dn * 0.08);

    // ==== Caustic light patterns ====
    vec2  cu = (co + drift * 2.0) * 6.5;
    float c1 = vnoise(cu + vec2(t * 0.19, t * 0.14));
    float c2 = vnoise(cu * 1.4 + vec2(-t * 0.15, t * 0.20) + 4.0);
    float caustic = pow(c1 * c2, 1.6) * 2.5;
    base += vec3(0.10, 0.15, 0.11) * caustic;

    // ==== Specular sparkle (sun glints) ====
    float sp = vnoise(co * 38.0 + vec2(t * 0.36, t * 0.28));
    sp = pow(sp, 9.0) * 3.5;
    base += vec3(1.0, 0.97, 0.88) * sp * 0.22;

    // ==== Foam / white-caps on wave crests ====
    float fn   = vnoise((co + drift * 0.4) * 10.0);
    float foam = smoothstep(0.50, 0.64, fn * waves * 2.0);
    base = mix(base, vec3(0.82, 0.94, 0.90), foam * 0.14);

    // ==== Subtle vignette ====
    vec2  vc = (uv - 0.5) * 1.3;
    base *= max(1.0 - dot(vc, vc) * 0.28, 0.75);

    // ==== God rays from top-right ====
    vec2  ro  = vec2(0.82, 0.88);           // top-right in UV space (y=1 is top)
    vec2  rd  = uv - ro;
    float rl  = length(rd);
    float ra  = atan(rd.y, rd.x);
    float rays = (sin(ra * 7.0 + t * 0.09) * 0.5 + 0.5)
               * exp(-rl * 1.8)
               * smoothstep(0.0, 0.25, rl);
    base += vec3(0.10, 0.14, 0.08) * rays * 0.20;

    fragColor = vec4(base, 1.0);
}
)";

} // namespace

// =====================================================================
//  Construction / destruction
// =====================================================================

OceanShader::OceanShader()
    : program(0), vao(0), vbo(0),
      timeLoc(-1), resolutionLoc(-1),
      initialized(false) {}

OceanShader::~OceanShader() {
    cleanup();
}

// =====================================================================
//  Shader compilation helpers
// =====================================================================

GLuint OceanShader::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        std::cerr << "Shader compile error ("
                  << (type == GL_VERTEX_SHADER ? "vertex" : "fragment")
                  << "): " << info << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint OceanShader::linkProgram(GLuint vert, GLuint frag) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint success = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(prog, sizeof(info), nullptr, info);
        std::cerr << "Shader link error: " << info << std::endl;
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// =====================================================================
//  Initialization — compile shaders, set up full-screen quad
// =====================================================================

bool OceanShader::init() {
    if (initialized) return true;

    GLuint vert = compileShader(GL_VERTEX_SHADER, kVertexSource);
    if (!vert) return false;

    GLuint frag = compileShader(GL_FRAGMENT_SHADER, kFragmentSource);
    if (!frag) { glDeleteShader(vert); return false; }

    program = linkProgram(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);
    if (!program) return false;

    timeLoc       = glGetUniformLocation(program, "u_time");
    resolutionLoc = glGetUniformLocation(program, "u_resolution");

    float quadVerts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };

    // Save current VAO so we don't corrupt it
    GLint prevVAO = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    // Unbind our VAO BEFORE unbinding the buffer, restore previous VAO
    glBindVertexArray(prevVAO);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    initialized = true;
    std::cout << "[OceanShader] Initialized (program=" << program
              << ", vao=" << vao << ", vbo=" << vbo << ")" << std::endl;
    return true;
}

// =====================================================================
//  Draw — render the ocean as a full-screen background
// =====================================================================

void OceanShader::draw(float time, float width, float height) {
    if (!initialized) {
        if (!init()) return;
    }

    // ---- Save ALL OpenGL state that our shader will touch ----
    GLint prevProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

    GLint prevVAO = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);

    GLint prevArrayBuffer = 0;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);

    // Save whether vertex attrib 0 was enabled (fixed-function uses it for glVertex)
    GLint attrib0Enabled = 0;
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &attrib0Enabled);

    // Save matrix state
    GLint prevMatrixMode = 0;
    glGetIntegerv(GL_MATRIX_MODE, &prevMatrixMode);

    // Push both matrices so the shader's NDC quad doesn't corrupt the ortho projection
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // ---- Draw the ocean quad ----
    glUseProgram(program);
    glUniform1f(timeLoc, time);
    glUniform2f(resolutionLoc, width, height);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glUseProgram(0);

    // ---- Restore ALL state ----

    // CRITICAL: Disable vertex attrib 0 on the default VAO if it wasn't enabled before.
    // In compatibility profile, VAO 0 is the default, and glVertex2f uses the
    // fixed-function vertex submission path which conflicts with generic attrib 0.
    if (!attrib0Enabled) {
        glDisableVertexAttribArray(0);
    }

    // Restore previous bindings
    glBindBuffer(GL_ARRAY_BUFFER, prevArrayBuffer);
    glBindVertexArray(prevVAO);
    glUseProgram(prevProgram);

    // Restore matrices
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    // Restore matrix mode
    glMatrixMode(prevMatrixMode);
}

// =====================================================================
//  Cleanup — release GPU resources
// =====================================================================

void OceanShader::cleanup() {
    if (!initialized) return;

    if (program) { glDeleteProgram(program); program = 0; }
    if (vao)     { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo)     { glDeleteBuffers(1, &vbo); vbo = 0; }

    initialized = false;
}
