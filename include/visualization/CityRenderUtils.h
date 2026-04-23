#ifndef CITY_RENDER_UTILS_H
#define CITY_RENDER_UTILS_H

#include "visualization/RenderUtils.h"

namespace CityRenderUtils {

struct ZoneVisibility {
    float focus = 0.0f;
    float transition = 0.0f;
    float brightness = 0.0f;
    float detail = 0.0f;
    float roadVisibility = 0.0f;
    float routeVisibility = 0.0f;
    float voidness = 0.0f;
};

float clamp01(float value);
float remap01(float value, float start, float end);
float smoothRange(float start, float end, float value);
float currentZoomFactor();

Color mixColor(const Color& from, const Color& to, float t);
Color withAlpha(const Color& color, float alpha);
Color scaleColor(const Color& color, float scale, float alphaScale = 1.0f);
Color biasColor(const Color& color, float redShift, float greenShift, float blueShift);
Color desaturateColor(const Color& color, float amount);

float pointDistance(float ax, float ay, float bx, float by);
float ellipseInfluence(float x, float y,
                       float centerX, float centerY,
                       float radiusX, float radiusY);
float ellipseDistance(float x, float y,
                      float centerX, float centerY,
                      float radiusX, float radiusY);

void updateZoneBounds(float minX, float maxX, float minY, float maxY);
float zoneOutsideDistance(float x, float y,
                          float falloffScale = 1.0f,
                          float minFalloffX = 0.001f,
                          float minFalloffY = 0.001f);
ZoneVisibility computeZoneVisibility(float x, float y,
                                     float centerX, float centerY,
                                     float radiusX, float radiusY);
Color attenuateToZone(const Color& color,
                      const ZoneVisibility& zone,
                      const Color& voidTint,
                      float minBrightness,
                      float desaturationStrength,
                      float minAlpha = 0.0f);

} // namespace CityRenderUtils

#endif // CITY_RENDER_UTILS_H
