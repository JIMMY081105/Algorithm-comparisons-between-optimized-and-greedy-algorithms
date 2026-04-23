#include "visualization/CityRenderUtils.h"

#include <algorithm>
#include <cmath>

namespace CityRenderUtils {
namespace {

float gZoneMinX = -1.0f;
float gZoneMaxX = 1.0f;
float gZoneMinY = -1.0f;
float gZoneMaxY = 1.0f;
float gZoneFalloffX = 4.0f;
float gZoneFalloffY = 4.0f;

struct ZoneOffset {
    float dx = 0.0f;
    float dy = 0.0f;
};

ZoneOffset zoneOffset(float x, float y) {
    return ZoneOffset{
        (x < gZoneMinX) ? (gZoneMinX - x)
            : (x > gZoneMaxX) ? (x - gZoneMaxX)
                              : 0.0f,
        (y < gZoneMinY) ? (gZoneMinY - y)
            : (y > gZoneMaxY) ? (y - gZoneMaxY)
                              : 0.0f
    };
}

} // namespace

float clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

float remap01(float value, float start, float end) {
    if (std::abs(end - start) <= 0.0001f) {
        return (value >= end) ? 1.0f : 0.0f;
    }
    return clamp01((value - start) / (end - start));
}

float smoothRange(float start, float end, float value) {
    return RenderUtils::smoothstep(remap01(value, start, end));
}

float currentZoomFactor() {
    return RenderUtils::getProjection().tileWidth / RenderUtils::BASE_TILE_WIDTH;
}

Color mixColor(const Color& from, const Color& to, float t) {
    t = clamp01(t);
    return Color(
        RenderUtils::lerp(from.r, to.r, t),
        RenderUtils::lerp(from.g, to.g, t),
        RenderUtils::lerp(from.b, to.b, t),
        RenderUtils::lerp(from.a, to.a, t));
}

Color withAlpha(const Color& color, float alpha) {
    return Color(color.r, color.g, color.b, alpha);
}

Color scaleColor(const Color& color, float scale, float alphaScale) {
    return Color(clamp01(color.r * scale),
                 clamp01(color.g * scale),
                 clamp01(color.b * scale),
                 clamp01(color.a * alphaScale));
}

Color biasColor(const Color& color, float redShift, float greenShift, float blueShift) {
    return Color(clamp01(color.r + redShift),
                 clamp01(color.g + greenShift),
                 clamp01(color.b + blueShift),
                 color.a);
}

Color desaturateColor(const Color& color, float amount) {
    const float grey =
        color.r * 0.299f +
        color.g * 0.587f +
        color.b * 0.114f;
    return mixColor(color, Color(grey, grey, grey, color.a), amount);
}

float pointDistance(float ax, float ay, float bx, float by) {
    const float dx = bx - ax;
    const float dy = by - ay;
    return std::sqrt(dx * dx + dy * dy);
}

float ellipseInfluence(float x, float y,
                       float centerX, float centerY,
                       float radiusX, float radiusY) {
    const float safeRadiusX = std::max(radiusX, 0.001f);
    const float safeRadiusY = std::max(radiusY, 0.001f);
    const float dx = (x - centerX) / safeRadiusX;
    const float dy = (y - centerY) / safeRadiusY;
    const float distance = std::sqrt(dx * dx + dy * dy);
    return 1.0f - RenderUtils::smoothstep(clamp01(distance));
}

float ellipseDistance(float x, float y,
                      float centerX, float centerY,
                      float radiusX, float radiusY) {
    const float safeRadiusX = std::max(radiusX, 0.001f);
    const float safeRadiusY = std::max(radiusY, 0.001f);
    const float dx = (x - centerX) / safeRadiusX;
    const float dy = (y - centerY) / safeRadiusY;
    return std::sqrt(dx * dx + dy * dy);
}

void updateZoneBounds(float minX, float maxX, float minY, float maxY) {
    const float spanX = std::max(1.0f, maxX - minX);
    const float spanY = std::max(1.0f, maxY - minY);
    const float padX = std::max(0.35f, spanX * 0.04f);
    const float padY = std::max(0.30f, spanY * 0.04f);

    gZoneMinX = minX - padX;
    gZoneMaxX = maxX + padX;
    gZoneMinY = minY - padY;
    gZoneMaxY = maxY + padY;
    gZoneFalloffX = std::max(2.8f, spanX * 0.40f);
    gZoneFalloffY = std::max(2.4f, spanY * 0.40f);
}

float zoneOutsideDistance(float x, float y,
                          float falloffScale,
                          float minFalloffX,
                          float minFalloffY) {
    const ZoneOffset offset = zoneOffset(x, y);
    const float falloffX = std::max(gZoneFalloffX * falloffScale, minFalloffX);
    const float falloffY = std::max(gZoneFalloffY * falloffScale, minFalloffY);

    return std::sqrt(
        (offset.dx / falloffX) * (offset.dx / falloffX) +
        (offset.dy / falloffY) * (offset.dy / falloffY));
}

ZoneVisibility computeZoneVisibility(float x, float y,
                                     float centerX, float centerY,
                                     float radiusX, float radiusY) {
    (void)centerX;
    (void)centerY;
    (void)radiusX;
    (void)radiusY;

    const ZoneOffset offset = zoneOffset(x, y);
    if (offset.dx <= 0.0001f && offset.dy <= 0.0001f) {
        return ZoneVisibility{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f};
    }

    const float outside = zoneOutsideDistance(x, y, 1.0f, 0.001f, 0.001f);
    const float focus = 1.0f - smoothRange(0.0f, 0.38f, outside);
    const float transition = 1.0f - smoothRange(0.0f, 1.0f, outside);
    const float brightness = clamp01(transition * 0.86f + focus * 0.14f);
    const float detail = clamp01(0.02f + transition * 0.78f + focus * 0.20f);
    const float roadVisibility = clamp01(std::max(0.20f, 0.26f + transition * 0.74f));
    const float routeVisibility = clamp01(std::max(0.62f, 0.66f + transition * 0.34f));

    return ZoneVisibility{
        focus,
        transition,
        brightness,
        detail,
        roadVisibility,
        routeVisibility,
        1.0f - transition
    };
}

Color attenuateToZone(const Color& color,
                      const ZoneVisibility& zone,
                      const Color& voidTint,
                      float minBrightness,
                      float desaturationStrength,
                      float minAlpha) {
    Color toned = desaturateColor(
        color,
        clamp01((1.0f - zone.focus) * desaturationStrength + zone.voidness * 0.22f));
    toned = scaleColor(toned, std::max(minBrightness, zone.brightness));
    toned = mixColor(withAlpha(voidTint, toned.a), toned, zone.transition);
    toned.a = clamp01(std::max(minAlpha, toned.a * (0.06f + 0.94f * zone.transition)));
    return toned;
}

} // namespace CityRenderUtils
