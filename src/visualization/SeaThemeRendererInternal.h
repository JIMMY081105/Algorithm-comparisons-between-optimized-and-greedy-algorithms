#ifndef SEA_THEME_RENDERER_INTERNAL_H
#define SEA_THEME_RENDERER_INTERNAL_H

#include "visualization/SeaThemeRenderer.h"

#include "environment/MissionPresentationUtils.h"
#include "visualization/IsometricRenderer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

namespace SeaRenderHelpers {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kFieldPadding = 3.4f;
constexpr float kWaterSampleSpacing = 1.55f;
constexpr float kDepotClearRadius = 0.68f;
constexpr float kClearInnerRadius = 0.20f;
constexpr float kClearOuterRadius = 0.56f;
constexpr int kAmbientParticleCount = 42;
constexpr int kDecorIsletCount = 16;

enum class GarbageSizeTier {
    SMALL,
    MEDIUM,
    LARGE
};

struct OceanWaveField {
    float swell;
    float primary;
    float secondary;
    float detail;
    float combined;
    float crest;
    float shimmer;
};

const RouteResult& routeFromMission(const MissionPresentation* mission);
int positiveMod(int value, int base);
float clamp01(float value);
float clampColor(float value);
Color shiftColor(const Color& color, float delta, float alphaScale = 1.0f);
Color mixColor(const Color& from, const Color& to, float t);
Color withAlpha(const Color& color, float alpha);
float pseudoRandom01(int x, int y, int salt);
float pseudoRandomSigned(int x, int y, int salt);
float clearanceFalloff(float distance, float innerRadius, float outerRadius);
GarbageSizeTier getGarbageSizeTier(float capacity);
float getGarbageScale(GarbageSizeTier tier);
Color getGarbageSizeColor(GarbageSizeTier tier);
void drawImmediateQuad(const std::array<IsoCoord, 4>& pts, const Color& color);
void drawImmediateTriangle(const std::array<IsoCoord, 3>& pts, const Color& color);
void drawWorldQuadPatch(float minWorldX, float minWorldY,
                        float maxWorldX, float maxWorldY,
                        const Color& color);
void drawWorldDiamondPatch(float centerWorldX, float centerWorldY,
                           float radiusWorldX, float radiusWorldY,
                           const Color& color);
void drawWorldSwellBand(float startWorldX, float startWorldY,
                        float endWorldX, float endWorldY,
                        float halfWidthWorld,
                        const Color& color);
void drawOrganicWorldPatch(float centerWorldX, float centerWorldY,
                           float radiusX, float radiusY,
                           float skewX, float skewY,
                           float animationTime,
                           float salt,
                           const Color& centerColor,
                           const Color& edgeColor);
void drawWorldWaveRibbon(float startWorldX, float startWorldY,
                         float endWorldX, float endWorldY,
                         float halfWidth,
                         float animationTime,
                         float frequency,
                         float amplitudeWorld,
                         const Color& color);
OceanWaveField sampleOceanWaveField(float worldX, float worldY, float time);
float sampleOceanSurfaceOffset(float worldX, float worldY, float time);
float distancePointToSegment(float px, float py,
                             float ax, float ay,
                             float bx, float by);
bool getRouteNodeWorldPos(const MapGraph& graph, int nodeId,
                          float& outX, float& outY);
void normalizeVector(float& x, float& y, float fallbackX, float fallbackY);
void getTruckHeading(const MapGraph& graph, const Truck& truck,
                     const RouteResult& currentRoute,
                     float& outHeadingX, float& outHeadingY);
float computeBoatCargoFill(const MapGraph& graph, const Truck& truck,
                           const RouteResult& currentRoute);
float getNodeShelfInfluence(const WasteNode& node, float worldX, float worldY);
float getClearingAmount(const MapGraph& graph, const Truck& truck,
                        const RouteResult& currentRoute,
                        float worldX, float worldY);

} // namespace SeaRenderHelpers

using namespace SeaRenderHelpers;

#endif // SEA_THEME_RENDERER_INTERNAL_H
