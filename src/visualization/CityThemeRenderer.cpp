#include "visualization/CityThemeRenderer.h"
#include "visualization/CityAssetLibrary.h"

#include "environment/MissionPresentationUtils.h"
#include "visualization/IsometricRenderer.h"

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>

namespace {

constexpr float kCityPaddingX = 2.0f;
constexpr float kCityPaddingY = 1.8f;
constexpr float kPeripheralBandX = 5.8f;
constexpr float kPeripheralBandY = 4.8f;
constexpr float kCongestionPenaltyScale = 0.75f;
constexpr float kIncidentPenaltyScale = 1.75f;
constexpr float kTwoPi = 6.28318530718f;
constexpr unsigned int kNorthEdge = 1u;
constexpr unsigned int kEastEdge = 2u;
constexpr unsigned int kSouthEdge = 4u;
constexpr unsigned int kWestEdge = 8u;
float gZoneMinX = -1.0f;
float gZoneMaxX = 1.0f;
float gZoneMinY = -1.0f;
float gZoneMaxY = 1.0f;
float gZoneFalloffX = 4.0f;
float gZoneFalloffY = 4.0f;

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

Color scaleColor(const Color& color, float scale, float alphaScale = 1.0f) {
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

struct ZoneVisibility {
    float focus = 0.0f;
    float transition = 0.0f;
    float brightness = 0.0f;
    float detail = 0.0f;
    float roadVisibility = 0.0f;
    float routeVisibility = 0.0f;
    float voidness = 0.0f;
};

ZoneVisibility computeZoneVisibility(float x, float y,
                                     float centerX, float centerY,
                                     float radiusX, float radiusY) {
    (void)centerX;
    (void)centerY;
    (void)radiusX;
    (void)radiusY;

    const float dx = (x < gZoneMinX) ? (gZoneMinX - x)
                   : (x > gZoneMaxX) ? (x - gZoneMaxX)
                                     : 0.0f;
    const float dy = (y < gZoneMinY) ? (gZoneMinY - y)
                   : (y > gZoneMaxY) ? (y - gZoneMaxY)
                                     : 0.0f;

    if (dx <= 0.0001f && dy <= 0.0001f) {
        return ZoneVisibility{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f};
    }

    const float outside = std::sqrt(
        (dx / std::max(gZoneFalloffX, 0.001f)) * (dx / std::max(gZoneFalloffX, 0.001f)) +
        (dy / std::max(gZoneFalloffY, 0.001f)) * (dy / std::max(gZoneFalloffY, 0.001f)));
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
                      float minAlpha = 0.0f) {
    Color toned = desaturateColor(
        color,
        clamp01((1.0f - zone.focus) * desaturationStrength + zone.voidness * 0.22f));
    toned = scaleColor(toned, std::max(minBrightness, zone.brightness));
    toned = mixColor(withAlpha(voidTint, toned.a), toned, zone.transition);
    toned.a = clamp01(std::max(minAlpha, toned.a * (0.06f + 0.94f * zone.transition)));
    return toned;
}

void appendDistinctPoint(std::vector<PlaybackPoint>& points, const PlaybackPoint& point) {
    if (points.empty()) {
        points.push_back(point);
        return;
    }

    const PlaybackPoint& back = points.back();
    if (pointDistance(back.x, back.y, point.x, point.y) > 0.001f) {
        points.push_back(point);
    }
}

const MissionPresentation* ensureMission(const MissionPresentation* mission) {
    return (mission && mission->isValid()) ? mission : nullptr;
}

IsoCoord lerpIso(const IsoCoord& a, const IsoCoord& b, float t) {
    return IsoCoord{
        RenderUtils::lerp(a.x, b.x, t),
        RenderUtils::lerp(a.y, b.y, t)
    };
}

void drawImmediateQuad(const std::array<IsoCoord, 4>& pts, const Color& color) {
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
                         int segments = 56) {
    if (radiusX <= 0.0f || radiusY <= 0.0f) {
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
                             int segments = 72) {
    if (innerRadiusX <= 0.0f || innerRadiusY <= 0.0f ||
        outerRadiusX <= innerRadiusX || outerRadiusY <= innerRadiusY) {
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

struct BlockFaces {
    std::array<IsoCoord, 4> top;
    std::array<IsoCoord, 4> left;
    std::array<IsoCoord, 4> right;
};

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

void drawFaceBands(const std::array<IsoCoord, 4>& face,
                   int bandCount,
                   float inset,
                   const Color& color,
                   float width) {
    if (bandCount <= 0 || color.a <= 0.001f) {
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
    glLineWidth(width);
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_LINE_LOOP);
    for (const IsoCoord& pt : pts) {
        glVertex2f(pt.x, pt.y);
    }
    glEnd();
    glLineWidth(1.0f);
}

} // namespace

CityThemeRenderer::CityThemeRenderer()
    : dashboardInfo{EnvironmentTheme::City, "City", "Operational urban district",
                    "Stormy", "Storm-driven logistics pressure", 0.0f, 0, true},
      layoutSeed(0),
      weather(CityWeather::Stormy),
      trafficClock(0.0f),
      gridColumns(0),
      gridRows(0),
      primaryBoulevardColumn(0),
      primaryBoulevardRow(0),
      sceneMinX(0.0f),
      sceneMaxX(0.0f),
      sceneMinY(0.0f),
      sceneMaxY(0.0f),
      peripheralMinX(0.0f),
      peripheralMaxX(0.0f),
      peripheralMinY(0.0f),
      peripheralMaxY(0.0f),
      nodeCentroidX(0.0f),
      nodeCentroidY(0.0f),
      operationalCenterX(0.0f),
      operationalCenterY(0.0f),
      operationalRadiusX(1.0f),
      operationalRadiusY(1.0f),
      activeGraph(nullptr) {}

EnvironmentTheme CityThemeRenderer::getTheme() const {
    return EnvironmentTheme::City;
}

bool CityThemeRenderer::init() {
    return true;
}

void CityThemeRenderer::rebuildScene(const MapGraph& graph, unsigned int seed) {
    layoutSeed = seed;
    activeGraph = &graph;

    std::mt19937 networkRng(seed ^ 0x43A9D17u);
    generateGridNetwork(graph, networkRng);
    assignNodeAnchors(graph);
    updateSceneFocus(graph);

    std::mt19937 layoutRng(seed ^ 0xD741B29u);
    generateBlocks(layoutRng);
    buildings.clear();
    landscapeFeatures.clear();
    populateFromAssetLibrary(graph, layoutRng);
    generatePeripheralScene(layoutRng);
    generateAmbientCars(layoutRng);

    refreshPairRoutes(graph);
    refreshDashboardInfo();
}

void CityThemeRenderer::update(float deltaTime) {
    trafficClock += deltaTime;
    for (auto& car : ambientCars) {
        car.offset += car.speed * deltaTime;
        if (car.offset > 1.0f) {
            car.offset -= std::floor(car.offset);
        }
    }
}

void CityThemeRenderer::applyRouteWeights(MapGraph& graph) const {
    const int nodeCount = graph.getNodeCount();
    std::vector<std::vector<float>> matrix(
        nodeCount, std::vector<float>(nodeCount, 0.0f));

    for (int i = 0; i < nodeCount; ++i) {
        for (int j = i + 1; j < nodeCount; ++j) {
            float weight = 0.0f;
            if (i < static_cast<int>(pairRoutes.size()) &&
                j < static_cast<int>(pairRoutes[i].size()) &&
                pairRoutes[i][j].valid) {
                weight = pairRoutes[i][j].insight.totalCost;
            } else {
                const WasteNode& a = graph.getNode(i);
                const WasteNode& b = graph.getNode(j);
                weight = pointDistance(a.getWorldX(), a.getWorldY(),
                                       b.getWorldX(), b.getWorldY()) * 1.8f;
            }

            matrix[i][j] = weight;
            matrix[j][i] = weight;
        }
    }

    graph.installWeightedMatrix(matrix);
}

MissionPresentation CityThemeRenderer::buildMissionPresentation(
    const RouteResult& route,
    const MapGraph& graph) const {
    MissionPresentation presentation;
    presentation.route = route;
    if (!route.isValid() || route.visitOrder.size() < 2) {
        return presentation;
    }

    std::vector<PlaybackPoint> pathPoints;
    std::vector<int> stopNodeIds;
    std::vector<std::size_t> stopPointIndices;

    for (std::size_t leg = 0; leg + 1 < route.visitOrder.size(); ++leg) {
        const int fromIndex = graph.findNodeIndex(route.visitOrder[leg]);
        const int toIndex = graph.findNodeIndex(route.visitOrder[leg + 1]);
        if (fromIndex < 0 || toIndex < 0 ||
            fromIndex >= static_cast<int>(pairRoutes.size()) ||
            toIndex >= static_cast<int>(pairRoutes[fromIndex].size())) {
            continue;
        }

        const PairRouteData& pair = pairRoutes[fromIndex][toIndex];
        if (!pair.valid || pair.polyline.empty()) {
            continue;
        }

        if (pathPoints.empty()) {
            pathPoints.push_back(pair.polyline.front());
        }

        for (std::size_t pointIndex = 1; pointIndex < pair.polyline.size(); ++pointIndex) {
            const PlaybackPoint& nextPoint = pair.polyline[pointIndex];
            if (pointDistance(pathPoints.back().x, pathPoints.back().y,
                              nextPoint.x, nextPoint.y) <= 0.001f) {
                continue;
            }

            pathPoints.push_back(nextPoint);
            if (pointIndex - 1 < pair.segmentSpeedFactors.size()) {
                presentation.playbackPath.segmentSpeedFactors.push_back(
                    pair.segmentSpeedFactors[pointIndex - 1]);
            } else {
                presentation.playbackPath.segmentSpeedFactors.push_back(1.0f);
            }
        }

        stopNodeIds.push_back(route.visitOrder[leg + 1]);
        stopPointIndices.push_back(pathPoints.empty() ? 0 : pathPoints.size() - 1);
        presentation.legInsights.push_back(pair.insight);
    }

    const int startIndex = graph.findNodeIndex(route.visitOrder.front());
    if (startIndex >= 0) {
        const WasteNode& startNode = graph.getNode(startIndex);
        const PlaybackPoint startPoint{startNode.getWorldX(), startNode.getWorldY()};
        if (pathPoints.empty() ||
            pointDistance(pathPoints.front().x, pathPoints.front().y,
                          startPoint.x, startPoint.y) > 0.001f) {
            pathPoints.insert(pathPoints.begin(), startPoint);
            for (std::size_t i = 0; i < stopPointIndices.size(); ++i) {
                ++stopPointIndices[i];
            }
        }
        stopNodeIds.insert(stopNodeIds.begin(), route.visitOrder.front());
        stopPointIndices.insert(stopPointIndices.begin(), 0);
    }

    presentation.playbackPath = MissionPresentationUtils::buildPlaybackPath(
        pathPoints, stopNodeIds, stopPointIndices,
        presentation.playbackPath.segmentSpeedFactors);

    const float avgCongestion = dashboardInfo.congestionLevel * 100.0f;
    presentation.narrative =
        std::string("City mode follows the district street graph through commercial, residential, campus, park, and landmark zones under ") +
        toDisplayString(weather) + " conditions. Average live congestion load is " +
        std::to_string(static_cast<int>(avgCongestion)) + "%.";
    return presentation;
}

ThemeDashboardInfo CityThemeRenderer::getDashboardInfo() const {
    return dashboardInfo;
}

void CityThemeRenderer::setLayerToggles(const SceneLayerToggles& toggles) {
    layerToggles = toggles;
}

bool CityThemeRenderer::supportsWeather() const {
    return true;
}

CityWeather CityThemeRenderer::getWeather() const {
    return weather;
}

void CityThemeRenderer::setWeather(CityWeather newWeather) {
    weather = newWeather;
    if (activeGraph != nullptr) {
        refreshPairRoutes(*activeGraph);
        refreshDashboardInfo();
    }
}

void CityThemeRenderer::randomizeTrafficConditions(unsigned int seed) {
    std::mt19937 trafficRng(seed ^ 0x6D2F41Bu);
    applyTrafficConditions(trafficRng);

    std::mt19937 carRng(seed ^ 0x41C7A93u);
    generateAmbientCars(carRng);

    if (activeGraph != nullptr) {
        refreshPairRoutes(*activeGraph);
        refreshDashboardInfo();
    }
}

void CityThemeRenderer::randomizeWeather(unsigned int seed) {
    std::mt19937 rng(seed ^ 0xB3E947u);
    setWeather(static_cast<CityWeather>(rng() % 4));
}

void CityThemeRenderer::drawGroundPlane(IsometricRenderer& renderer,
                                        const MapGraph&,
                                        const Truck&,
                                        const MissionPresentation*,
                                        float animationTime) {
    (void)renderer;
    (void)animationTime;

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const float left = static_cast<float>(viewport[0]);
    const float top = static_cast<float>(viewport[1]);
    const float right = left + static_cast<float>(viewport[2]);
    const float bottom = top + static_cast<float>(viewport[3]);

    Color skyTop(0.22f, 0.30f, 0.40f, 1.0f);
    Color skyBottom(0.11f, 0.14f, 0.18f, 1.0f);
    Color groundBase(0.17f, 0.18f, 0.16f, 0.98f);
    Color districtLift(0.42f, 0.46f, 0.50f, 0.13f);
    Color wetTint(0.14f, 0.20f, 0.26f, 0.10f);

    switch (weather) {
        case CityWeather::Sunny:
            skyTop = Color(0.26f, 0.34f, 0.46f, 1.0f);
            skyBottom = Color(0.13f, 0.16f, 0.22f, 1.0f);
            groundBase = Color(0.18f, 0.18f, 0.15f, 0.98f);
            districtLift = Color(0.98f, 0.78f, 0.40f, 0.16f);
            wetTint = Color(0.14f, 0.18f, 0.22f, 0.05f);
            break;
        case CityWeather::Cloudy:
            skyTop = Color(0.20f, 0.24f, 0.30f, 1.0f);
            skyBottom = Color(0.10f, 0.12f, 0.17f, 1.0f);
            groundBase = Color(0.16f, 0.17f, 0.16f, 0.98f);
            districtLift = Color(0.54f, 0.58f, 0.64f, 0.12f);
            wetTint = Color(0.14f, 0.18f, 0.24f, 0.10f);
            break;
        case CityWeather::Rainy:
            skyTop = Color(0.14f, 0.18f, 0.24f, 1.0f);
            skyBottom = Color(0.07f, 0.10f, 0.14f, 1.0f);
            groundBase = Color(0.12f, 0.13f, 0.14f, 0.99f);
            districtLift = Color(0.42f, 0.48f, 0.56f, 0.12f);
            wetTint = Color(0.16f, 0.24f, 0.34f, 0.14f);
            break;
        case CityWeather::Stormy:
            skyTop = Color(0.09f, 0.12f, 0.18f, 1.0f);
            skyBottom = Color(0.05f, 0.07f, 0.10f, 1.0f);
            groundBase = Color(0.09f, 0.10f, 0.12f, 1.0f);
            districtLift = Color(0.34f, 0.40f, 0.48f, 0.11f);
            wetTint = Color(0.18f, 0.26f, 0.36f, 0.16f);
            break;
    }

    skyTop = mixColor(skyTop, Color(0.02f, 0.02f, 0.03f, 1.0f), 0.88f);
    skyBottom = mixColor(skyBottom, Color(0.00f, 0.00f, 0.00f, 1.0f), 0.96f);
    groundBase = mixColor(groundBase, Color(0.01f, 0.01f, 0.01f, groundBase.a), 0.72f);

    glBegin(GL_QUADS);
    glColor4f(skyTop.r, skyTop.g, skyTop.b, skyTop.a);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glColor4f(skyBottom.r, skyBottom.g, skyBottom.b, skyBottom.a);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();

    groundBase = mixColor(groundBase, Color(0.03f, 0.04f, 0.07f, groundBase.a), 0.44f);
    drawWorldQuadPatch(peripheralMinX, peripheralMinY,
                       peripheralMaxX, peripheralMaxY,
                       groundBase);

    const IsoCoord opIso = RenderUtils::worldToIso(operationalCenterX, operationalCenterY);
    const float halfW = RenderUtils::getProjection().tileWidth * 0.5f;
    const float halfH = RenderUtils::getProjection().tileHeight * 0.5f;
    const float glowRadiusX =
        (operationalRadiusX + operationalRadiusY) * halfW * 0.92f;
    const float glowRadiusY =
        (operationalRadiusX + operationalRadiusY) * halfH * 0.64f;

    drawGradientEllipse(opIso.x, opIso.y + glowRadiusY * 0.16f,
                        glowRadiusX * 1.35f, glowRadiusY * 1.05f,
                        districtLift,
                        withAlpha(districtLift, 0.0f));

    drawGradientEllipse(opIso.x, opIso.y + glowRadiusY * 0.20f,
                        glowRadiusX * 1.78f, glowRadiusY * 1.46f,
                        withAlpha(scaleColor(districtLift, 1.06f), 0.03f),
                        withAlpha(districtLift, 0.0f));

    auto blockHasMeaningfulContent = [&](const BlockZone& block) {
        if (block.district == DistrictType::Park ||
            block.district == DistrictType::Campus ||
            block.district == DistrictType::Landmark ||
            block.landmarkCluster ||
            block.civicAnchor ||
            block.parkAnchor) {
            return true;
        }

        const float marginX = (block.maxX - block.minX) * 0.05f;
        const float marginY = (block.maxY - block.minY) * 0.05f;
        const auto contains = [&](float worldX, float worldY) {
            return worldX >= block.minX - marginX &&
                   worldX <= block.maxX + marginX &&
                   worldY >= block.minY - marginY &&
                   worldY <= block.maxY + marginY;
        };

        for (const auto& placed : presetBuildings) {
            if (contains(placed.worldX, placed.worldY)) {
                return true;
            }
        }
        for (const auto& env : presetEnvironments) {
            if (contains(env.worldX, env.worldY)) {
                return true;
            }
        }
        for (const auto& vehicle : presetVehicles) {
            if (contains(vehicle.worldX, vehicle.worldY)) {
                return true;
            }
        }
        for (const auto& prop : presetRoadProps) {
            if (contains(prop.worldX, prop.worldY)) {
                return true;
            }
        }
        return false;
    };

    for (const BlockZone& block : blocks) {
        const float spanX = block.maxX - block.minX;
        const float spanY = block.maxY - block.minY;
        const float insetX = spanX * 0.05f;
        const float insetY = spanY * 0.06f;
        const ZoneVisibility zone = computeZoneVisibility(
            block.centerX, block.centerY,
            operationalCenterX, operationalCenterY,
            operationalRadiusX, operationalRadiusY);
        const bool meaningful = blockHasMeaningfulContent(block);

        if (zone.transition <= 0.03f && !meaningful) {
            continue;
        }

        Color patch(0.18f, 0.18f, 0.17f, 0.96f);
        Color curb(0.11f, 0.11f, 0.12f, 0.92f);
        switch (block.district) {
            case DistrictType::Landmark:
                patch = Color(0.30f, 0.31f, 0.34f, 0.96f);
                curb = Color(0.16f, 0.17f, 0.20f, 0.96f);
                break;
            case DistrictType::Commercial:
                patch = Color(0.24f, 0.24f, 0.25f, 0.96f);
                curb = Color(0.12f, 0.12f, 0.13f, 0.94f);
                break;
            case DistrictType::Mixed:
                patch = Color(0.22f, 0.22f, 0.21f, 0.96f);
                curb = Color(0.11f, 0.11f, 0.12f, 0.94f);
                break;
            case DistrictType::Residential:
                patch = Color(0.18f, 0.20f, 0.17f, 0.96f);
                curb = Color(0.11f, 0.12f, 0.11f, 0.92f);
                break;
            case DistrictType::Campus:
                patch = Color(0.20f, 0.22f, 0.18f, 0.96f);
                curb = Color(0.12f, 0.13f, 0.12f, 0.92f);
                break;
            case DistrictType::Park:
                patch = Color(0.18f, 0.26f, 0.16f, 0.94f);
                curb = Color(0.10f, 0.13f, 0.10f, 0.90f);
                break;
            case DistrictType::Service:
                patch = Color(0.18f, 0.18f, 0.18f, 0.96f);
                curb = Color(0.10f, 0.10f, 0.10f, 0.92f);
                break;
        }

        patch = mixColor(patch, wetTint,
                         (weather == CityWeather::Rainy || weather == CityWeather::Stormy)
                             ? 0.26f : 0.0f);

        if (!meaningful &&
            block.district != DistrictType::Park &&
            block.district != DistrictType::Campus) {
            patch = mixColor(patch, Color(0.03f, 0.05f, 0.08f, patch.a), 0.84f);
            curb = mixColor(curb, Color(0.01f, 0.02f, 0.04f, curb.a), 0.82f);
        }

        patch = attenuateToZone(
            patch, zone, Color(0.01f, 0.02f, 0.04f, patch.a),
            meaningful ? 0.08f : 0.01f,
            meaningful ? 0.28f : 0.52f);
        curb = attenuateToZone(
            curb, zone, Color(0.00f, 0.01f, 0.03f, curb.a),
            meaningful ? 0.06f : 0.00f,
            0.42f);

        drawWorldQuadPatch(block.minX + insetX * 0.36f, block.minY + insetY * 0.36f,
                           block.maxX - insetX * 0.36f, block.maxY - insetY * 0.36f,
                           curb);
        drawWorldQuadPatch(block.minX + insetX, block.minY + insetY,
                           block.maxX - insetX, block.maxY - insetY,
                           patch);

        if (meaningful &&
            (block.visualTier == VisualTier::Focus || block.district == DistrictType::Landmark)) {
            drawWorldQuadPatch(block.centerX - spanX * 0.16f,
                               block.centerY - spanY * 0.16f,
                               block.centerX + spanX * 0.16f,
                               block.centerY + spanY * 0.16f,
                               attenuateToZone(
                                   Color(0.32f, 0.36f, 0.42f, 0.12f),
                                   zone,
                                   Color(0.01f, 0.02f, 0.04f, 0.12f),
                                   0.06f,
                                   0.18f));
        }

        if (block.district == DistrictType::Park || block.district == DistrictType::Campus) {
            drawWorldQuadPatch(block.centerX - spanX * 0.10f,
                               block.centerY - spanY * 0.10f,
                               block.centerX + spanX * 0.10f,
                               block.centerY + spanY * 0.10f,
                               attenuateToZone(
                                   Color(0.22f, 0.34f, 0.18f, 0.12f),
                                   zone,
                                   Color(0.01f, 0.02f, 0.04f, 0.12f),
                                   0.04f,
                                   0.24f));
        }
    }
}

void CityThemeRenderer::drawTransitNetwork(
    IsometricRenderer& renderer,
    const MapGraph& graph,
    const MissionPresentation* mission,
    AnimationController::PlaybackState playbackState,
    float routeRevealProgress,
    float animationTime) {
    (void)graph;

    for (std::size_t i = 0; i < roads.size(); ++i) {
        const RoadSegment& road = roads[i];
        const Intersection& from = intersections[road.from];
        const Intersection& to = intersections[road.to];
        const IsoCoord isoFrom = RenderUtils::worldToIso(from.x, from.y);
        const IsoCoord isoTo = RenderUtils::worldToIso(to.x, to.y);
        const float midWorldX = (from.x + to.x) * 0.5f;
        const float midWorldY = (from.y + to.y) * 0.5f;
        const ZoneVisibility zone = computeZoneVisibility(
            midWorldX, midWorldY,
            operationalCenterX, operationalCenterY,
            operationalRadiusX, operationalRadiusY);
        if (zone.transition <= 0.04f) {
            continue;
        }
        const float roadFocus = road.focusWeight;
        const float congestionPulse =
            0.60f + 0.40f * std::sin(animationTime * 3.0f + static_cast<float>(i));

        Color curbColor = mixColor(Color(0.04f, 0.05f, 0.06f, 0.92f),
                                   Color(0.08f, 0.10f, 0.12f, 0.94f),
                                   roadFocus * 0.65f);
        Color roadColor = mixColor(Color(0.09f, 0.10f, 0.11f, 0.94f),
                                   Color(0.18f, 0.20f, 0.23f, 0.96f),
                                   roadFocus * 0.74f);
        Color centerLine = mixColor(Color(0.44f, 0.46f, 0.48f, 0.18f),
                                    Color(0.86f, 0.82f, 0.62f, 0.30f),
                                    roadFocus * 0.82f);
        Color glow = mixColor(Color(0.12f, 0.14f, 0.18f, 0.00f),
                              Color(0.68f, 0.76f, 0.92f, 0.12f),
                              roadFocus * 0.70f);

        if (weather == CityWeather::Rainy || weather == CityWeather::Stormy) {
            roadColor = mixColor(roadColor, Color(0.12f, 0.16f, 0.22f, roadColor.a), 0.24f);
            glow = mixColor(glow, Color(0.60f, 0.74f, 0.94f, glow.a), 0.30f);
        }

        if (road.congestion > 0.58f && layerToggles.showCongestion) {
            roadColor = mixColor(roadColor, Color(0.84f, 0.36f, 0.16f, 0.98f),
                                 road.congestion * (0.70f + congestionPulse * 0.30f));
        }
        if (road.incident && layerToggles.showIncidents) {
            roadColor = mixColor(roadColor, Color(0.82f, 0.14f, 0.12f, 1.0f), 0.60f);
        }
        if (road.arterial) {
            roadColor = mixColor(roadColor, Color(0.24f, 0.25f, 0.27f, roadColor.a), 0.24f);
            centerLine = mixColor(centerLine, Color(0.92f, 0.86f, 0.58f, centerLine.a), 0.36f);
        }

        curbColor = attenuateToZone(
            curbColor, zone, Color(0.00f, 0.01f, 0.03f, curbColor.a),
            0.04f, 0.42f);
        roadColor = attenuateToZone(
            roadColor, zone, Color(0.01f, 0.02f, 0.05f, roadColor.a),
            0.08f, 0.30f);
        centerLine = attenuateToZone(
            centerLine, zone, Color(0.00f, 0.00f, 0.00f, centerLine.a),
            0.14f, 0.34f, 0.01f);
        glow = attenuateToZone(
            glow, zone, Color(0.00f, 0.00f, 0.00f, glow.a),
            0.16f, 0.24f);

        const float curbWidth = 8.8f + roadFocus * 3.4f + (road.arterial ? 2.0f : 0.0f);
        const float asphaltWidth = 6.6f + roadFocus * 2.8f + (road.arterial ? 1.4f : 0.0f);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y, curbColor, curbWidth);
        if (road.visualTier == VisualTier::Focus) {
            renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                              glow, curbWidth + 2.6f);
        }
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          roadColor, asphaltWidth);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          centerLine, 1.0f + roadFocus * 1.1f);

        const float midX = (isoFrom.x + isoTo.x) * 0.5f;
        const float midY = (isoFrom.y + isoTo.y) * 0.5f;
        renderer.drawDiamond(midX, midY, 1.8f + roadFocus * 1.8f,
                             0.8f + roadFocus * 0.7f,
                             attenuateToZone(
                                 withAlpha(Color(0.88f, 0.84f, 0.62f, 1.0f),
                                           0.05f + roadFocus * 0.08f),
                                 zone,
                                 Color(0.00f, 0.00f, 0.00f, 0.08f),
                                 0.12f,
                                 0.24f));
    }

    for (const auto& placed : presetBuildings) {
        drawPresetBuilding(renderer, placed);
    }

    if (layerToggles.showIncidents) {
        for (std::size_t i = 0; i < roads.size(); ++i) {
            const RoadSegment& road = roads[i];
            if (!road.incident) {
                continue;
            }

            const Intersection& from = intersections[road.from];
            const Intersection& to = intersections[road.to];
            const ZoneVisibility zone = computeZoneVisibility(
                (from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f,
                operationalCenterX, operationalCenterY,
                operationalRadiusX, operationalRadiusY);
            if (zone.transition <= 0.10f) {
                continue;
            }
            const IsoCoord isoFrom = RenderUtils::worldToIso(from.x, from.y);
            const IsoCoord isoTo = RenderUtils::worldToIso(to.x, to.y);
            const float midX = (isoFrom.x + isoTo.x) * 0.5f;
            const float midY = (isoFrom.y + isoTo.y) * 0.5f;
            renderer.drawLine(midX - 4.0f, midY - 3.0f, midX + 4.0f, midY + 3.0f,
                              attenuateToZone(
                                  Color(1.0f, 0.24f, 0.18f, 0.78f),
                                  zone,
                                  Color(0.06f, 0.02f, 0.02f, 0.78f),
                                  0.24f,
                                  0.10f),
                              1.6f);
            renderer.drawLine(midX - 4.0f, midY + 3.0f, midX + 4.0f, midY - 3.0f,
                              attenuateToZone(
                                  Color(1.0f, 0.24f, 0.18f, 0.78f),
                                  zone,
                                  Color(0.06f, 0.02f, 0.02f, 0.78f),
                                  0.24f,
                                  0.10f),
                              1.6f);
        }
    }

    if (layerToggles.showTrafficLights) {
        for (const auto& intersection : intersections) {
            const ZoneVisibility zone = computeZoneVisibility(
                intersection.x, intersection.y,
                operationalCenterX, operationalCenterY,
                operationalRadiusX, operationalRadiusY);
            if (intersection.hasTrafficLight && zone.transition > 0.10f) {
                drawTrafficLight(renderer, intersection, animationTime + trafficClock);
            }
        }
    }

    const MissionPresentation* liveMission = ensureMission(mission);
    if (liveMission != nullptr) {
        drawRoutePath(renderer, *liveMission, routeRevealProgress, animationTime);

        if (layerToggles.showRouteGraph) {
            for (const auto& stop : liveMission->playbackPath.stops) {
                const int nodeIndex = graph.findNodeIndex(stop.nodeId);
                if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodeAnchors.size())) {
                    continue;
                }

                const WasteNode& node = graph.getNode(nodeIndex);
                const Intersection& anchor = intersections[nodeAnchors[nodeIndex]];
                const ZoneVisibility zone = computeZoneVisibility(
                    (node.getWorldX() + anchor.x) * 0.5f,
                    (node.getWorldY() + anchor.y) * 0.5f,
                    operationalCenterX, operationalCenterY,
                    operationalRadiusX, operationalRadiusY);
                const IsoCoord nodeIso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
                const IsoCoord anchorIso = RenderUtils::worldToIso(anchor.x, anchor.y);
                renderer.drawLine(nodeIso.x, nodeIso.y, anchorIso.x, anchorIso.y,
                                  attenuateToZone(
                                      Color(0.34f, 0.70f, 0.90f, 0.18f),
                                      zone,
                                      Color(0.00f, 0.00f, 0.00f, 0.18f),
                                      0.28f,
                                      0.18f,
                                      0.06f),
                                  1.0f);
            }
        }
    }

    if (playbackState == AnimationController::PlaybackState::PLAYING &&
        layerToggles.showTraffic) {
        for (const auto& car : ambientCars) {
            if (car.roadIndex < 0 || car.roadIndex >= static_cast<int>(roads.size())) {
                continue;
            }

            const RoadSegment& road = roads[car.roadIndex];
            const Intersection& from = intersections[road.from];
            const Intersection& to = intersections[road.to];
            const float localOffset = clamp01(car.offset);
            const float worldX = RenderUtils::lerp(from.x, to.x, localOffset);
            const float worldY = RenderUtils::lerp(from.y, to.y, localOffset);
            const ZoneVisibility zone = computeZoneVisibility(
                worldX, worldY,
                operationalCenterX, operationalCenterY,
                operationalRadiusX, operationalRadiusY);
            if (zone.transition <= 0.14f) {
                continue;
            }
            const IsoCoord iso = RenderUtils::worldToIso(worldX, worldY);
            const Color carColor = car.crashed
                ? Color(0.88f, 0.22f, 0.18f, 0.85f)
                : mixColor(Color(0.80f, 0.84f, 0.88f, 0.78f),
                           Color(0.96f, 0.96f, 0.98f, 0.86f),
                           road.focusWeight * 0.60f);
            renderer.drawDiamond(iso.x, iso.y, 3.0f, 1.4f,
                                 attenuateToZone(
                                     carColor,
                                     zone,
                                     Color(0.01f, 0.02f, 0.04f, carColor.a),
                                     0.22f,
                                     0.18f,
                                     0.08f));
            renderer.drawDiamondOutline(iso.x, iso.y, 3.0f, 1.4f,
                                        attenuateToZone(
                                            Color(0.08f, 0.09f, 0.10f, 0.55f),
                                            zone,
                                            Color(0.00f, 0.00f, 0.00f, 0.55f),
                                            0.20f,
                                            0.12f,
                                            0.04f),
                                        1.0f);
        }
    }
}

void CityThemeRenderer::drawWasteNode(IsometricRenderer& renderer,
                                      const WasteNode& node,
                                      float animationTime) {
    const IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
    const float focus = computeOperationalFocus(node.getWorldX(), node.getWorldY());
    const ZoneVisibility zone = computeZoneVisibility(
        node.getWorldX(), node.getWorldY(),
        operationalCenterX, operationalCenterY,
        operationalRadiusX, operationalRadiusY);
    const Color urgency = RenderUtils::getUrgencyColor(node.getUrgency());
    const float pulse = 0.65f + 0.35f * std::sin(animationTime * 3.2f + node.getId());
    const Color padColor = node.isCollected()
        ? Color(0.28f, 0.30f, 0.34f, 0.82f)
        : mixColor(Color(0.18f, 0.20f, 0.23f, 0.92f),
                   Color(0.26f, 0.30f, 0.36f, 0.96f),
                   focus * 0.60f);
    const Color blockColor = node.isCollected()
        ? Color(0.38f, 0.42f, 0.46f, 0.84f)
        : mixColor(Color(0.24f, 0.28f, 0.34f, 0.95f), urgency, 0.28f);
    const Color attenuatedPad = attenuateToZone(
        padColor, zone, Color(0.01f, 0.02f, 0.04f, padColor.a),
        0.22f, 0.16f, 0.08f);
    const Color attenuatedBlock = attenuateToZone(
        blockColor, zone, Color(0.01f, 0.02f, 0.04f, blockColor.a),
        0.26f, 0.12f, 0.10f);
    const Color attenuatedUrgency = attenuateToZone(
        Color(urgency.r, urgency.g, urgency.b,
              node.isCollected() ? 0.18f : 0.22f + pulse * 0.16f),
        zone,
        Color(0.02f, 0.02f, 0.04f, 0.20f),
        0.36f,
        0.08f,
        0.10f);
    const Color attenuatedRing = attenuateToZone(
        Color(urgency.r, urgency.g, urgency.b,
              node.isCollected() ? 0.18f : 0.22f + pulse * 0.12f),
        zone,
        Color(0.02f, 0.02f, 0.04f, 0.18f),
        0.34f,
        0.08f,
        0.08f);

    renderer.drawDiamond(iso.x, iso.y + 9.0f, 11.0f, 5.5f, attenuatedPad);
    renderer.drawIsometricBlock(iso.x, iso.y + 4.0f, 12.0f, 8.0f, 10.0f,
                                attenuatedBlock,
                                attenuateToZone(
                                    Color(0.12f, 0.14f, 0.17f, 0.92f),
                                    zone,
                                    Color(0.01f, 0.02f, 0.04f, 0.92f),
                                    0.22f,
                                    0.16f,
                                    0.08f));
    renderer.drawDiamond(iso.x, iso.y - 7.0f, 5.0f, 2.4f, attenuatedUrgency);
    renderer.drawRing(iso.x, iso.y - 12.0f, 3.4f, attenuatedRing,
                      1.2f);
}

void CityThemeRenderer::drawHQNode(IsometricRenderer& renderer,
                                   const WasteNode& node,
                                   float animationTime) {
    const IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
    const ZoneVisibility zone = computeZoneVisibility(
        node.getWorldX(), node.getWorldY(),
        operationalCenterX, operationalCenterY,
        operationalRadiusX, operationalRadiusY);
    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 2.2f);
    renderer.drawDiamond(iso.x, iso.y + 14.0f, 17.0f, 8.5f,
                         attenuateToZone(
                             Color(0.16f, 0.18f, 0.22f, 0.94f),
                             zone,
                             Color(0.01f, 0.02f, 0.04f, 0.94f),
                             0.30f,
                             0.12f,
                             0.10f));
    renderer.drawIsometricBlock(iso.x, iso.y + 4.0f, 18.0f, 12.0f, 16.0f,
                                attenuateToZone(
                                    Color(0.28f, 0.38f, 0.52f, 0.98f),
                                    zone,
                                    Color(0.02f, 0.03f, 0.05f, 0.98f),
                                    0.34f,
                                    0.08f,
                                    0.12f),
                                attenuateToZone(
                                    Color(0.14f, 0.18f, 0.24f, 0.98f),
                                    zone,
                                    Color(0.01f, 0.02f, 0.04f, 0.98f),
                                    0.28f,
                                    0.10f,
                                    0.12f));
    renderer.drawDiamond(iso.x, iso.y - 16.0f, 8.0f, 3.6f,
                         attenuateToZone(
                             Color(0.74f, 0.86f, 0.96f, 0.20f + pulse * 0.12f),
                             zone,
                             Color(0.02f, 0.03f, 0.05f, 0.24f),
                             0.46f,
                             0.06f,
                             0.10f));
    renderer.drawRing(iso.x, iso.y - 16.0f, 5.2f,
                      attenuateToZone(
                          Color(0.88f, 0.94f, 0.98f, 0.18f + pulse * 0.08f),
                          zone,
                          Color(0.02f, 0.03f, 0.05f, 0.20f),
                          0.42f,
                          0.06f,
                          0.08f), 1.4f);
}

void CityThemeRenderer::drawTruck(IsometricRenderer& renderer,
                                  const MapGraph&,
                                  const Truck& truck,
                                  const MissionPresentation* mission,
                                  float animationTime) {
    const MissionPresentation* liveMission = ensureMission(mission);
    const IsoCoord iso = RenderUtils::worldToIso(truck.getPosX(), truck.getPosY());
    const ZoneVisibility zone = computeZoneVisibility(
        truck.getPosX(), truck.getPosY(),
        operationalCenterX, operationalCenterY,
        operationalRadiusX, operationalRadiusY);

    float dirX = 1.0f;
    float dirY = 0.0f;
    if (liveMission != nullptr && liveMission->playbackPath.points.size() >= 2) {
        float bestDistance = std::numeric_limits<float>::max();
        for (std::size_t i = 1; i < liveMission->playbackPath.points.size(); ++i) {
            const PlaybackPoint& a = liveMission->playbackPath.points[i - 1];
            const PlaybackPoint& b = liveMission->playbackPath.points[i];
            const float centerX = (a.x + b.x) * 0.5f;
            const float centerY = (a.y + b.y) * 0.5f;
            const float d = pointDistance(centerX, centerY, truck.getPosX(), truck.getPosY());
            if (d < bestDistance) {
                bestDistance = d;
                dirX = b.x - a.x;
                dirY = b.y - a.y;
            }
        }
    }

    const float length = std::sqrt(dirX * dirX + dirY * dirY);
    if (length > 0.0001f) {
        dirX /= length;
        dirY /= length;
    }

    const float bob = std::sin(animationTime * 5.0f) * 0.6f;
    renderer.drawRing(iso.x, iso.y + bob, 7.0f,
                      attenuateToZone(
                          Color(0.22f, 0.90f, 1.0f, 0.20f),
                          zone,
                          Color(0.02f, 0.03f, 0.05f, 0.20f),
                          0.48f,
                          0.04f,
                          0.10f), 1.5f);
    renderer.drawDiamond(iso.x, iso.y + bob, 5.0f, 2.4f,
                         attenuateToZone(
                             Color(0.96f, 0.74f, 0.18f, 0.96f),
                             zone,
                             Color(0.04f, 0.03f, 0.02f, 0.96f),
                             0.62f,
                             0.04f,
                             0.16f));
    renderer.drawDiamondOutline(iso.x, iso.y + bob, 5.0f, 2.4f,
                                attenuateToZone(
                                    Color(0.10f, 0.12f, 0.14f, 0.76f),
                                    zone,
                                    Color(0.00f, 0.00f, 0.00f, 0.76f),
                                    0.28f,
                                    0.08f,
                                    0.08f), 1.2f);
    renderer.drawLine(iso.x, iso.y + bob,
                      iso.x + dirX * 10.0f, iso.y - dirY * 5.0f + bob,
                      attenuateToZone(
                          Color(0.98f, 0.92f, 0.64f, 0.52f),
                          zone,
                          Color(0.04f, 0.03f, 0.02f, 0.52f),
                          0.58f,
                          0.04f,
                          0.12f), 1.4f);
}

void CityThemeRenderer::drawDecorativeElements(IsometricRenderer& renderer,
                                               const MapGraph&,
                                               float animationTime) {
    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 1.4f);

    for (const AmbientStreet& street : ambientStreets) {
        const ZoneVisibility zone = computeZoneVisibility(
            (street.startX + street.endX) * 0.5f,
            (street.startY + street.endY) * 0.5f,
            operationalCenterX, operationalCenterY,
            operationalRadiusX, operationalRadiusY);
        if (zone.transition <= 0.08f) {
            continue;
        }
        const IsoCoord isoFrom = RenderUtils::worldToIso(street.startX, street.startY);
        const IsoCoord isoTo = RenderUtils::worldToIso(street.endX, street.endY);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          attenuateToZone(
                              Color(0.04f, 0.05f, 0.06f, 0.40f),
                              zone,
                              Color(0.00f, 0.01f, 0.03f, 0.40f),
                              0.04f,
                              0.40f),
                          street.width + 1.6f);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          attenuateToZone(
                              Color(0.10f, 0.11f, 0.13f, 0.30f),
                              zone,
                              Color(0.00f, 0.01f, 0.03f, 0.30f),
                              0.08f,
                              0.34f),
                          street.width);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          attenuateToZone(
                              Color(0.58f, 0.64f, 0.76f, street.glow * (0.45f + pulse * 0.25f)),
                              zone,
                              Color(0.01f, 0.02f, 0.04f, street.glow),
                              0.18f,
                              0.18f,
                              0.04f),
                          1.2f);
    }

    for (const auto& env : presetEnvironments) {
        drawPresetEnvironment(renderer, env, animationTime);
    }

    for (const auto& prop : presetRoadProps) {
        drawPresetRoadProp(renderer, prop, animationTime);
    }

    for (const auto& vehicle : presetVehicles) {
        drawPresetVehicle(renderer, vehicle);
    }

    for (const auto& building : backgroundBuildings) {
        drawBuildingLot(renderer, building);
    }
}

void CityThemeRenderer::drawAtmosphericEffects(IsometricRenderer& renderer,
                                               const MapGraph&,
                                               float animationTime) {
    (void)renderer;
    const float spanX = std::max(1.0f, peripheralMaxX - peripheralMinX);
    const float spanY = std::max(1.0f, peripheralMaxY - peripheralMinY);

    if (weather == CityWeather::Rainy || weather == CityWeather::Stormy) {
        const int streakCount = (weather == CityWeather::Stormy) ? 34 : 24;
        for (int i = 0; i < streakCount; ++i) {
            const float phase = animationTime * (weather == CityWeather::Stormy ? 7.2f : 5.2f) +
                                static_cast<float>(i) * 1.31f;
            const float worldX = peripheralMinX + std::fmod(phase * 1.4f + i * 3.7f, spanX);
            const float worldY = peripheralMinY + std::fmod(phase * 0.9f + i * 2.4f, spanY);
            const ZoneVisibility zone = computeZoneVisibility(
                worldX, worldY,
                operationalCenterX, operationalCenterY,
                operationalRadiusX, operationalRadiusY);
            if (zone.transition <= 0.18f) {
                continue;
            }
            const IsoCoord iso = RenderUtils::worldToIso(worldX, worldY);
            renderer.drawLine(iso.x, iso.y - 10.0f, iso.x - 3.0f, iso.y + 6.0f,
                              attenuateToZone(
                                  Color(0.82f, 0.88f, 0.98f,
                                        weather == CityWeather::Stormy ? 0.24f : 0.18f),
                                  zone,
                                  Color(0.02f, 0.03f, 0.05f,
                                        weather == CityWeather::Stormy ? 0.24f : 0.18f),
                                  0.18f,
                                  0.12f,
                                  0.04f),
                              1.0f);
        }
    }

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const float left = static_cast<float>(viewport[0]);
    const float top = static_cast<float>(viewport[1]);
    const float right = left + static_cast<float>(viewport[2]);
    const float bottom = top + static_cast<float>(viewport[3]);
    const float midX = (left + right) * 0.5f;
    const float midY = (top + bottom) * 0.5f;
    const float edgeAlpha = 0.01f + weatherOverlayStrength() * 0.04f;

    glBegin(GL_QUADS);
    glColor4f(0.02f, 0.03f, 0.05f, edgeAlpha);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glColor4f(0.02f, 0.03f, 0.05f, 0.0f);
    glVertex2f(right, midY);
    glVertex2f(left, midY);

    glColor4f(0.02f, 0.03f, 0.05f, 0.0f);
    glVertex2f(left, midY);
    glVertex2f(right, midY);
    glColor4f(0.02f, 0.03f, 0.05f, edgeAlpha + 0.04f);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();

    glBegin(GL_QUADS);
    glColor4f(0.02f, 0.03f, 0.05f, edgeAlpha * 0.80f);
    glVertex2f(left, top);
    glColor4f(0.02f, 0.03f, 0.05f, 0.0f);
    glVertex2f(midX, top);
    glVertex2f(midX, bottom);
    glColor4f(0.02f, 0.03f, 0.05f, edgeAlpha + 0.03f);
    glVertex2f(left, bottom);

    glColor4f(0.02f, 0.03f, 0.05f, 0.0f);
    glVertex2f(midX, top);
    glColor4f(0.02f, 0.03f, 0.05f, edgeAlpha * 0.70f);
    glVertex2f(right, top);
    glColor4f(0.02f, 0.03f, 0.05f, edgeAlpha + 0.03f);
    glVertex2f(right, bottom);
    glColor4f(0.02f, 0.03f, 0.05f, 0.0f);
    glVertex2f(midX, bottom);
    glEnd();

    const float zoneCenterX = (gZoneMinX + gZoneMaxX) * 0.5f;
    const float zoneCenterY = (gZoneMinY + gZoneMaxY) * 0.5f;
    const IsoCoord zoneIso = RenderUtils::worldToIso(zoneCenterX, zoneCenterY);
    const std::array<IsoCoord, 4> zoneCorners{{
        RenderUtils::worldToIso(gZoneMinX, gZoneMinY),
        RenderUtils::worldToIso(gZoneMaxX, gZoneMinY),
        RenderUtils::worldToIso(gZoneMaxX, gZoneMaxY),
        RenderUtils::worldToIso(gZoneMinX, gZoneMaxY)
    }};

    float clearRadiusX = 0.0f;
    float clearRadiusY = 0.0f;
    for (const IsoCoord& corner : zoneCorners) {
        clearRadiusX = std::max(clearRadiusX, std::abs(corner.x - zoneIso.x));
        clearRadiusY = std::max(clearRadiusY, std::abs(corner.y - zoneIso.y));
    }
    clearRadiusX += 22.0f;
    clearRadiusY += 14.0f;

    drawGradientRingEllipse(zoneIso.x, zoneIso.y,
                            clearRadiusX, clearRadiusY,
                            clearRadiusX * 1.26f, clearRadiusY * 1.22f,
                            Color(0.00f, 0.00f, 0.00f, 0.0f),
                            Color(0.00f, 0.00f, 0.00f,
                                  0.18f + weatherOverlayStrength() * 0.16f));
    drawGradientRingEllipse(zoneIso.x, zoneIso.y,
                            clearRadiusX * 1.12f, clearRadiusY * 1.10f,
                            clearRadiusX * 1.88f, clearRadiusY * 1.66f,
                            Color(0.00f, 0.00f, 0.00f, 0.0f),
                            Color(0.00f, 0.00f, 0.00f,
                                  0.72f + weatherOverlayStrength() * 0.18f));
}

void CityThemeRenderer::generateGridNetwork(const MapGraph& graph, std::mt19937& rng) {
    intersections.clear();
    roads.clear();
    roadAdjacency.clear();

    gridColumns = 8;
    gridRows = 6;
    primaryBoulevardColumn = gridColumns / 2;
    primaryBoulevardRow = gridRows / 2;

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto& node : graph.getNodes()) {
        minX = std::min(minX, node.getWorldX());
        maxX = std::max(maxX, node.getWorldX());
        minY = std::min(minY, node.getWorldY());
        maxY = std::max(maxY, node.getWorldY());
    }

    const float startX = minX - kCityPaddingX;
    const float endX = maxX + kCityPaddingX;
    const float startY = minY - kCityPaddingY;
    const float endY = maxY + kCityPaddingY;

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> cycleDistribution(0.0f, 4.0f);

    auto buildAxisPositions = [&](int count, float start, float end,
                                  int boulevardIndex, float boulevardBoost) {
        std::vector<float> spans(std::max(0, count - 1), 1.0f);
        for (int i = 0; i < static_cast<int>(spans.size()); ++i) {
            float spanWeight = 0.90f + unit(rng) * 0.45f;
            if (i == boulevardIndex) {
                spanWeight += boulevardBoost;
            } else if (std::abs(i - boulevardIndex) == 1) {
                spanWeight += boulevardBoost * 0.28f;
            } else if (i == 0 || i == static_cast<int>(spans.size()) - 1) {
                spanWeight += 0.18f;
            }
            spans[i] = spanWeight;
        }

        const float totalSpan = std::accumulate(spans.begin(), spans.end(), 0.0f);
        std::vector<float> positions(count, start);
        float cursor = start;
        for (int i = 1; i < count; ++i) {
            cursor += (end - start) * (spans[i - 1] / totalSpan);
            positions[i] = cursor;
        }
        positions.front() = start;
        positions.back() = end;
        return positions;
    };

    const std::vector<float> columnPositions = buildAxisPositions(
        gridColumns, startX, endX, std::max(0, primaryBoulevardColumn - 1), 0.78f);
    const std::vector<float> rowPositions = buildAxisPositions(
        gridRows, startY, endY, std::max(0, primaryBoulevardRow - 1), 0.62f);

    for (int row = 0; row < gridRows; ++row) {
        for (int col = 0; col < gridColumns; ++col) {
            const bool onBoulevard =
                row == primaryBoulevardRow || col == primaryBoulevardColumn;
            const bool innerCollector =
                row == std::max(1, primaryBoulevardRow - 2) ||
                col == std::max(1, primaryBoulevardColumn - 2);
            const bool hasLight =
                row > 0 && row < gridRows - 1 &&
                col > 0 && col < gridColumns - 1 &&
                (onBoulevard || ((row + col + static_cast<int>(layoutSeed % 3)) % 2 == 0));
            intersections.push_back(Intersection{
                row * gridColumns + col,
                columnPositions[col],
                rowPositions[row],
                hasLight || innerCollector,
                cycleDistribution(rng)
            });
        }
    }

    roadAdjacency.assign(intersections.size(), {});

    auto addRoad = [&](int from, int to) {
        const Intersection& a = intersections[from];
        const Intersection& b = intersections[to];
        const bool verticalRoad = a.x == b.x;
        const int lineIndex = verticalRoad ? (from % gridColumns) : (from / gridColumns);
        const bool arterial = verticalRoad
            ? lineIndex == primaryBoulevardColumn
            : lineIndex == primaryBoulevardRow;

        const int roadIndex = static_cast<int>(roads.size());
        roads.push_back(RoadSegment{
            from,
            to,
            pointDistance(a.x, a.y, b.x, b.y),
            0.0f,
            false,
            0.0f,
            0.0f,
            arterial,
            VisualTier::Support
        });
        roadAdjacency[from].push_back(roadIndex);
        roadAdjacency[to].push_back(roadIndex);
    };

    for (int row = 0; row < gridRows; ++row) {
        for (int col = 0; col < gridColumns; ++col) {
            const int id = row * gridColumns + col;
            if (col + 1 < gridColumns) {
                addRoad(id, id + 1);
            }
            if (row + 1 < gridRows) {
                addRoad(id, id + gridColumns);
            }
        }
    }

    applyTrafficConditions(rng);
}

void CityThemeRenderer::assignNodeAnchors(const MapGraph& graph) {
    nodeAnchors.assign(graph.getNodeCount(), 0);
    for (int i = 0; i < graph.getNodeCount(); ++i) {
        const WasteNode& node = graph.getNode(i);
        float bestDistance = std::numeric_limits<float>::max();
        int bestAnchor = 0;
        for (const auto& intersection : intersections) {
            const float d = pointDistance(node.getWorldX(), node.getWorldY(),
                                          intersection.x, intersection.y);
            if (d < bestDistance) {
                bestDistance = d;
                bestAnchor = intersection.id;
            }
        }
        nodeAnchors[i] = bestAnchor;
    }
}

void CityThemeRenderer::updateSceneFocus(const MapGraph& graph) {
    sceneMinX = std::numeric_limits<float>::max();
    sceneMaxX = std::numeric_limits<float>::lowest();
    sceneMinY = std::numeric_limits<float>::max();
    sceneMaxY = std::numeric_limits<float>::lowest();

    for (const auto& intersection : intersections) {
        sceneMinX = std::min(sceneMinX, intersection.x);
        sceneMaxX = std::max(sceneMaxX, intersection.x);
        sceneMinY = std::min(sceneMinY, intersection.y);
        sceneMaxY = std::max(sceneMaxY, intersection.y);
    }

    float centroidSumX = 0.0f;
    float centroidSumY = 0.0f;
    for (const auto& node : graph.getNodes()) {
        sceneMinX = std::min(sceneMinX, node.getWorldX());
        sceneMaxX = std::max(sceneMaxX, node.getWorldX());
        sceneMinY = std::min(sceneMinY, node.getWorldY());
        sceneMaxY = std::max(sceneMaxY, node.getWorldY());
        centroidSumX += node.getWorldX();
        centroidSumY += node.getWorldY();
    }

    const float nodeCount = std::max(1, graph.getNodeCount());
    nodeCentroidX = centroidSumX / static_cast<float>(nodeCount);
    nodeCentroidY = centroidSumY / static_cast<float>(nodeCount);

    const WasteNode& hq = graph.getHQNode();
    operationalCenterX = (hq.getWorldX() + nodeCentroidX) * 0.5f;
    operationalCenterY = (hq.getWorldY() + nodeCentroidY) * 0.5f;

    const float spanX = std::max(1.0f, sceneMaxX - sceneMinX);
    const float spanY = std::max(1.0f, sceneMaxY - sceneMinY);
    operationalRadiusX = std::max(4.0f, spanX * 0.34f);
    operationalRadiusY = std::max(3.6f, spanY * 0.30f);

    peripheralMinX = sceneMinX - kPeripheralBandX;
    peripheralMaxX = sceneMaxX + kPeripheralBandX;
    peripheralMinY = sceneMinY - kPeripheralBandY;
    peripheralMaxY = sceneMaxY + kPeripheralBandY;
    updateZoneBounds(sceneMinX, sceneMaxX, sceneMinY, sceneMaxY);

    for (auto& road : roads) {
        const Intersection& from = intersections[road.from];
        const Intersection& to = intersections[road.to];
        const float midX = (from.x + to.x) * 0.5f;
        const float midY = (from.y + to.y) * 0.5f;
        road.focusWeight = clamp01(
            computeOperationalFocus(midX, midY) + (road.arterial ? 0.16f : 0.0f));
        road.visualTier = visualTierFromFocus(road.focusWeight);
    }
}

void CityThemeRenderer::generateBlocks(std::mt19937& rng) {
    blocks.clear();
    blocks.reserve((gridRows - 1) * (gridColumns - 1));

    std::uniform_real_distribution<float> focusJitter(-0.05f, 0.05f);

    for (int row = 0; row + 1 < gridRows; ++row) {
        for (int col = 0; col + 1 < gridColumns; ++col) {
            const Intersection& a = intersections[row * gridColumns + col];
            const Intersection& b = intersections[row * gridColumns + col + 1];
            const Intersection& c = intersections[(row + 1) * gridColumns + col];
            const float centerX = (a.x + b.x) * 0.5f;
            const float centerY = (a.y + c.y) * 0.5f;
            const float focus = clamp01(computeOperationalFocus(centerX, centerY) + focusJitter(rng));
            unsigned int arterialEdges = 0;
            if (row + 1 == primaryBoulevardRow) arterialEdges |= kNorthEdge;
            if (col == primaryBoulevardColumn) arterialEdges |= kEastEdge;
            if (row == primaryBoulevardRow) arterialEdges |= kSouthEdge;
            if (col + 1 == primaryBoulevardColumn) arterialEdges |= kWestEdge;

            const bool edgeBlock =
                row == 0 || col == 0 ||
                row == gridRows - 2 || col == gridColumns - 2;
            const float arterialBoost = arterialEdges == 0 ? 0.0f : 0.12f;

            DistrictType district = DistrictType::Mixed;
            if (edgeBlock && focus < 0.34f) {
                district = DistrictType::Service;
            } else if (focus >= 0.56f) {
                district = DistrictType::Commercial;
            } else if (focus >= 0.38f) {
                district = DistrictType::Mixed;
            } else if (focus >= 0.22f) {
                district = DistrictType::Residential;
            } else {
                district = DistrictType::Service;
            }

            float occupancyTarget = 0.55f;
            float greenRatio = 0.18f;
            float streetSetback = 0.12f;
            float interiorMargin = 0.10f;
            float heightBias = focus + arterialBoost;
            switch (district) {
                case DistrictType::Commercial:
                    occupancyTarget = 0.84f;
                    greenRatio = 0.10f;
                    streetSetback = 0.08f;
                    interiorMargin = 0.06f;
                    break;
                case DistrictType::Mixed:
                    occupancyTarget = 0.68f;
                    greenRatio = 0.18f;
                    streetSetback = 0.11f;
                    interiorMargin = 0.09f;
                    break;
                case DistrictType::Residential:
                    occupancyTarget = 0.48f;
                    greenRatio = 0.34f;
                    streetSetback = 0.16f;
                    interiorMargin = 0.12f;
                    break;
                case DistrictType::Campus:
                    occupancyTarget = 0.34f;
                    greenRatio = 0.44f;
                    streetSetback = 0.18f;
                    interiorMargin = 0.14f;
                    break;
                case DistrictType::Park:
                    occupancyTarget = 0.08f;
                    greenRatio = 0.84f;
                    streetSetback = 0.20f;
                    interiorMargin = 0.18f;
                    break;
                case DistrictType::Service:
                    occupancyTarget = 0.40f;
                    greenRatio = 0.22f;
                    streetSetback = 0.14f;
                    interiorMargin = 0.10f;
                    break;
                case DistrictType::Landmark:
                    occupancyTarget = 0.78f;
                    greenRatio = 0.12f;
                    streetSetback = 0.08f;
                    interiorMargin = 0.06f;
                    break;
            }

            blocks.push_back(BlockZone{
                row,
                col,
                RenderUtils::lerp(a.x, b.x, 0.07f),
                RenderUtils::lerp(a.x, b.x, 0.93f),
                RenderUtils::lerp(a.y, c.y, 0.08f),
                RenderUtils::lerp(a.y, c.y, 0.92f),
                centerX,
                centerY,
                focus,
                occupancyTarget,
                greenRatio,
                streetSetback,
                interiorMargin,
                heightBias,
                arterialEdges,
                district,
                visualTierFromFocus(clamp01(focus + arterialBoost)),
                false,
                false,
                false
            });
        }
    }

    int landmarkIndex = -1;
    float bestLandmarkScore = std::numeric_limits<float>::max();
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        const BlockZone& block = blocks[i];
        if (block.arterialEdges == 0u || block.district == DistrictType::Service) {
            continue;
        }

        const float centerDistance =
            pointDistance(block.centerX, block.centerY,
                          operationalCenterX, operationalCenterY);
        const float arterialPenalty = (block.arterialEdges & (kNorthEdge | kSouthEdge)) &&
                                      (block.arterialEdges & (kEastEdge | kWestEdge))
            ? -1.0f
            : 0.0f;
        const float score = centerDistance - block.focusWeight * 2.4f + arterialPenalty;
        if (score < bestLandmarkScore) {
            bestLandmarkScore = score;
            landmarkIndex = i;
        }
    }

    if (landmarkIndex >= 0) {
        BlockZone& block = blocks[landmarkIndex];
        block.district = DistrictType::Landmark;
        block.landmarkCluster = true;
        block.occupancyTarget = 0.76f;
        block.greenRatio = 0.10f;
        block.streetSetback = 0.07f;
        block.interiorMargin = 0.05f;
        block.heightBias = clamp01(block.heightBias + 0.35f);
        block.visualTier = VisualTier::Focus;
    }

    int campusIndex = -1;
    float bestCampusScore = std::numeric_limits<float>::max();
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        const BlockZone& block = blocks[i];
        if (i == landmarkIndex || block.arterialEdges == 0u) {
            continue;
        }
        const float focusGap = std::abs(block.focusWeight - 0.48f);
        const float centerDistance =
            pointDistance(block.centerX, block.centerY,
                          operationalCenterX, operationalCenterY);
        const float score = focusGap + centerDistance * 0.08f;
        if (score < bestCampusScore) {
            bestCampusScore = score;
            campusIndex = i;
        }
    }

    if (campusIndex >= 0) {
        BlockZone& block = blocks[campusIndex];
        block.district = DistrictType::Campus;
        block.civicAnchor = true;
        block.occupancyTarget = 0.32f;
        block.greenRatio = 0.46f;
        block.streetSetback = 0.18f;
        block.interiorMargin = 0.14f;
        block.heightBias = clamp01(block.heightBias * 0.78f);
        if (block.visualTier == VisualTier::Peripheral) {
            block.visualTier = VisualTier::Support;
        }
    }

    std::vector<std::pair<float, int>> parkCandidates;
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        const BlockZone& block = blocks[i];
        if (i == landmarkIndex || i == campusIndex) {
            continue;
        }
        if (block.district == DistrictType::Commercial || block.focusWeight > 0.62f) {
            continue;
        }

        const float residentialAffinity =
            (block.district == DistrictType::Residential ? -0.24f : 0.0f) +
            (block.arterialEdges == 0u ? -0.10f : 0.04f);
        const float score =
            std::abs(block.focusWeight - 0.34f) +
            pointDistance(block.centerX, block.centerY, nodeCentroidX, nodeCentroidY) * 0.02f +
            residentialAffinity;
        parkCandidates.push_back({score, i});
    }

    std::sort(parkCandidates.begin(), parkCandidates.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
    const int parkCount = std::min<int>(4, static_cast<int>(parkCandidates.size()));
    for (int i = 0; i < parkCount; ++i) {
        BlockZone& block = blocks[parkCandidates[i].second];
        block.district = DistrictType::Park;
        block.parkAnchor = true;
        block.occupancyTarget = 0.06f;
        block.greenRatio = 0.88f;
        block.streetSetback = 0.20f;
        block.interiorMargin = 0.18f;
        block.heightBias = 0.10f;
        block.visualTier = VisualTier::Support;
    }

    for (BlockZone& block : blocks) {
        if (block.district == DistrictType::Landmark ||
            block.district == DistrictType::Campus ||
            block.district == DistrictType::Park) {
            continue;
        }

        const float distanceToLandmark = landmarkIndex >= 0
            ? pointDistance(block.centerX, block.centerY,
                            blocks[landmarkIndex].centerX, blocks[landmarkIndex].centerY)
            : std::numeric_limits<float>::max();

        if (distanceToLandmark < 8.5f &&
            block.district != DistrictType::Service) {
            block.district = DistrictType::Commercial;
            block.occupancyTarget = std::max(block.occupancyTarget, 0.76f);
            block.greenRatio = std::min(block.greenRatio, 0.14f);
            block.streetSetback = std::min(block.streetSetback, 0.09f);
            block.interiorMargin = std::min(block.interiorMargin, 0.07f);
            const float proximityBoost = 1.0f - clamp01(distanceToLandmark / 8.5f);
            block.heightBias = clamp01(block.heightBias + 0.10f + proximityBoost * 0.22f);
            block.visualTier = VisualTier::Focus;
        } else if (block.district == DistrictType::Service &&
                   block.arterialEdges == 0u && block.focusWeight > 0.24f) {
            block.district = DistrictType::Residential;
            block.occupancyTarget = 0.46f;
            block.greenRatio = 0.36f;
            block.streetSetback = 0.17f;
            block.interiorMargin = 0.12f;
        }
    }
}

void CityThemeRenderer::generateBuildings(const MapGraph& graph, std::mt19937& rng) {
    (void)graph;
    (void)rng;

    buildings.clear();
    landscapeFeatures.clear();
    return;
}

void CityThemeRenderer::populateFromAssetLibrary(const MapGraph& graph,
                                                  std::mt19937& rng) {
    presetBuildings.clear();
    presetEnvironments.clear();
    presetVehicles.clear();
    presetRoadProps.clear();
    buildings.clear();
    landscapeFeatures.clear();

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> authoredJitter(-0.08f, 0.08f);
    std::uniform_real_distribution<float> slotJitter(-0.06f, 0.06f);

    const std::vector<PlaybackPoint> baseSlots{
        {-0.32f, -0.24f}, { 0.00f, -0.26f}, { 0.30f, -0.22f},
        {-0.34f,  0.00f}, {-0.04f,  0.02f}, { 0.28f,  0.00f},
        {-0.30f,  0.24f}, { 0.02f,  0.26f}, { 0.32f,  0.22f}
    };
    const std::vector<PlaybackPoint> denseEdgeSlots{
        {-0.46f, -0.30f}, {-0.16f, -0.40f}, { 0.16f, -0.40f}, { 0.44f, -0.28f},
        {-0.48f, -0.04f}, { 0.46f, -0.02f}, {-0.44f,  0.24f}, {-0.14f,  0.38f},
        { 0.18f,  0.38f}, { 0.46f,  0.24f}, {-0.18f,  0.00f}, { 0.14f,  0.02f}
    };
    const std::vector<PlaybackPoint> landmarkSlots{
        { 0.00f, -0.16f}, {-0.24f,  0.10f}, { 0.24f,  0.10f}, { 0.00f,  0.28f}
    };
    const std::vector<PlaybackPoint> campusSlots{
        {-0.26f, -0.18f}, { 0.24f, -0.18f}, {-0.20f,  0.14f},
        { 0.22f,  0.16f}, { 0.00f, -0.30f}, { 0.00f,  0.00f}
    };
    const std::vector<PlaybackPoint> roadSlots{
        {-0.34f, -0.42f}, { 0.00f, -0.42f}, { 0.34f, -0.40f},
        {-0.42f, -0.08f}, { 0.42f, -0.04f},
        {-0.38f,  0.36f}, { 0.00f,  0.38f}, { 0.38f,  0.36f}
    };
    const std::vector<PlaybackPoint> vehicleSlots{
        {-0.24f, -0.36f}, { 0.12f, -0.34f}, {-0.30f,  0.32f},
        { 0.08f,  0.34f}, {-0.40f,  0.02f}, { 0.40f,  0.04f}
    };

    auto targetTotalRange = [](DistrictType district) {
        switch (district) {
            case DistrictType::Landmark: return std::pair<int, int>{2, 4};
            case DistrictType::Commercial: return std::pair<int, int>{8, 14};
            case DistrictType::Mixed: return std::pair<int, int>{6, 10};
            case DistrictType::Residential: return std::pair<int, int>{5, 8};
            case DistrictType::Service: return std::pair<int, int>{4, 7};
            case DistrictType::Campus: return std::pair<int, int>{2, 4};
            case DistrictType::Park:
            default: return std::pair<int, int>{0, 1};
        }
    };

    auto heightScaleRange = [](DistrictType district) {
        switch (district) {
            case DistrictType::Commercial: return std::pair<float, float>{0.90f, 1.22f};
            case DistrictType::Mixed: return std::pair<float, float>{0.88f, 1.12f};
            case DistrictType::Residential: return std::pair<float, float>{0.82f, 1.00f};
            case DistrictType::Service: return std::pair<float, float>{0.80f, 0.96f};
            case DistrictType::Landmark: return std::pair<float, float>{1.00f, 1.06f};
            case DistrictType::Campus: return std::pair<float, float>{0.84f, 0.96f};
            case DistrictType::Park:
            default: return std::pair<float, float>{0.82f, 0.94f};
        }
    };

    auto slotPoolFor = [&](DistrictType district) {
        std::vector<PlaybackPoint> slots = baseSlots;
        switch (district) {
            case DistrictType::Commercial:
                slots.insert(slots.end(), denseEdgeSlots.begin(), denseEdgeSlots.end());
                break;
            case DistrictType::Mixed:
                slots.insert(slots.end(), denseEdgeSlots.begin(), denseEdgeSlots.begin() + 9);
                break;
            case DistrictType::Residential:
                slots.insert(slots.end(), denseEdgeSlots.begin(), denseEdgeSlots.begin() + 6);
                break;
            case DistrictType::Service:
                slots.insert(slots.end(), denseEdgeSlots.begin(), denseEdgeSlots.begin() + 5);
                break;
            case DistrictType::Campus:
                slots = campusSlots;
                break;
            case DistrictType::Park:
                slots = {{0.00f, 0.00f}};
                break;
            case DistrictType::Landmark:
            default:
                slots = landmarkSlots;
                break;
        }
        return slots;
    };

    auto pushBuildingIfSafe = [&](const CityAssets::BuildingPreset& preset,
                                  float worldX,
                                  float worldY,
                                  float heightScale) {
        const float clearance = 0.38f + std::max(preset.width, preset.depth) * 0.5f;
        if (isReservedVisualArea(worldX, worldY, clearance, graph)) {
            return false;
        }

        presetBuildings.push_back({&preset, worldX, worldY, heightScale});
        return true;
    };

    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        const BlockZone& block = blocks[i];
        const float spanX = block.maxX - block.minX;
        const float spanY = block.maxY - block.minY;

        const int districtOrd = static_cast<int>(block.district);
        const CityAssets::ZoneType zone = CityAssets::classifyZone(districtOrd);
        const CityAssets::TileAssignment tile =
            CityAssets::assignTile(zone, i, layoutSeed);

        int placedBuildingsForBlock = 0;

        for (const auto& bp : tile.buildings) {
            const float wx =
                block.centerX + bp.offsetX * spanX + authoredJitter(rng) * spanX * 0.08f;
            const float wy =
                block.centerY + bp.offsetY * spanY + authoredJitter(rng) * spanY * 0.08f;
            if (pushBuildingIfSafe(bp.preset, wx, wy, bp.heightScale)) {
                ++placedBuildingsForBlock;
            }
        }

        for (const auto& ep : tile.environment) {
            const float wx = block.centerX + ep.offsetX * spanX;
            const float wy = block.centerY + ep.offsetY * spanY;
            const float envSpanX =
                spanX * ((block.district == DistrictType::Park)
                    ? 0.34f
                    : (block.district == DistrictType::Campus ? 0.31f : 0.28f));
            const float envSpanY =
                spanY * ((block.district == DistrictType::Park)
                    ? 0.30f
                    : (block.district == DistrictType::Campus ? 0.27f : 0.24f));
            presetEnvironments.push_back({&ep.preset, wx, wy, envSpanX, envSpanY});
        }

        for (const auto& vp : tile.vehicles) {
            const float wx = block.centerX + vp.offsetX * spanX;
            const float wy = block.centerY + vp.offsetY * spanY;
            presetVehicles.push_back({&vp.preset, wx, wy});
        }

        for (const auto& rp : tile.roadProps) {
            const float wx = block.centerX + rp.offsetX * spanX;
            const float wy = block.centerY + rp.offsetY * spanY;
            presetRoadProps.push_back({&rp.preset, wx, wy});
        }

        if (!tile.buildings.empty()) {
            const auto [minTotal, maxTotal] = targetTotalRange(block.district);
            std::uniform_int_distribution<int> totalDistribution(minTotal, maxTotal);
            const int targetTotal = totalDistribution(rng);
            const int targetExtraBuildings = std::max(0, targetTotal - placedBuildingsForBlock);

            if (targetExtraBuildings > 0) {
                std::vector<PlaybackPoint> slotPool = slotPoolFor(block.district);
                std::shuffle(slotPool.begin(), slotPool.end(), rng);

                const auto [heightMin, heightMax] = heightScaleRange(block.district);
                const std::size_t slotCount = slotPool.size();
                const int maxAttempts =
                    std::max(targetExtraBuildings * 5, static_cast<int>(slotCount) * 3);

                int placedExtras = 0;
                int attempts = 0;
                while (placedExtras < targetExtraBuildings && attempts < maxAttempts) {
                    const PlaybackPoint& slot =
                        slotPool[static_cast<std::size_t>(attempts) % slotCount];
                    const auto& source =
                        tile.buildings[static_cast<std::size_t>(attempts) % tile.buildings.size()];

                    const float worldX =
                        block.centerX + slot.x * spanX + slotJitter(rng) * spanX;
                    const float worldY =
                        block.centerY + slot.y * spanY + slotJitter(rng) * spanY;
                    const float extraHeightScale =
                        heightMin + (heightMax - heightMin) * unit(rng);

                    if (pushBuildingIfSafe(source.preset, worldX, worldY, extraHeightScale)) {
                        ++placedExtras;
                    }
                    ++attempts;
                }
            }
        }

        int extraVehicleCount = 0;
        int extraRoadPropCount = 0;
        switch (block.district) {
            case DistrictType::Commercial:
                extraVehicleCount = 3 + static_cast<int>(rng() % 3u);
                extraRoadPropCount = 3 + static_cast<int>(rng() % 2u);
                break;
            case DistrictType::Mixed:
                extraVehicleCount = 2 + static_cast<int>(rng() % 2u);
                extraRoadPropCount = 2 + static_cast<int>(rng() % 2u);
                break;
            case DistrictType::Service:
                extraVehicleCount = 1 + static_cast<int>(rng() % 2u);
                extraRoadPropCount = 1 + static_cast<int>(rng() % 2u);
                break;
            case DistrictType::Residential:
                extraVehicleCount = 1;
                break;
            default:
                break;
        }

        if (!tile.vehicles.empty() && extraVehicleCount > 0) {
            std::vector<PlaybackPoint> vehicleSlotPool = vehicleSlots;
            std::shuffle(vehicleSlotPool.begin(), vehicleSlotPool.end(), rng);
            for (int extra = 0; extra < extraVehicleCount; ++extra) {
                const PlaybackPoint& slot =
                    vehicleSlotPool[static_cast<std::size_t>(extra) % vehicleSlotPool.size()];
                const auto& source =
                    tile.vehicles[static_cast<std::size_t>(extra) % tile.vehicles.size()];
                const float worldX =
                    block.centerX + slot.x * spanX + authoredJitter(rng) * spanX * 0.04f;
                const float worldY =
                    block.centerY + slot.y * spanY + authoredJitter(rng) * spanY * 0.04f;
                if (!isReservedVisualArea(worldX, worldY, 0.18f, graph)) {
                    presetVehicles.push_back({&source.preset, worldX, worldY});
                }
            }
        }

        if (!tile.roadProps.empty() && extraRoadPropCount > 0) {
            std::vector<PlaybackPoint> roadSlotPool = roadSlots;
            std::shuffle(roadSlotPool.begin(), roadSlotPool.end(), rng);
            for (int extra = 0; extra < extraRoadPropCount; ++extra) {
                const PlaybackPoint& slot =
                    roadSlotPool[static_cast<std::size_t>(extra) % roadSlotPool.size()];
                const auto& source =
                    tile.roadProps[static_cast<std::size_t>(extra) % tile.roadProps.size()];
                const float worldX =
                    block.centerX + slot.x * spanX + authoredJitter(rng) * spanX * 0.03f;
                const float worldY =
                    block.centerY + slot.y * spanY + authoredJitter(rng) * spanY * 0.03f;
                if (!isReservedVisualArea(worldX, worldY, 0.16f, graph)) {
                    presetRoadProps.push_back({&source.preset, worldX, worldY});
                }
            }
        }
    }

    std::sort(presetBuildings.begin(), presetBuildings.end(),
              [](const PlacedPresetBuilding& a, const PlacedPresetBuilding& b) {
                  if (std::abs(a.worldY - b.worldY) > 0.02f) return a.worldY < b.worldY;
                  return a.worldX < b.worldX;
              });
}

void CityThemeRenderer::generatePeripheralScene(std::mt19937& rng) {
    backgroundBuildings.clear();
    ambientStreets.clear();

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> hueDistribution(-0.03f, 0.02f);

    auto addBackgroundLot = [&](float x, float y, float width, float depth, float height,
                                BuildingFamily family, BuildingForm form,
                                float brightnessBias) {
        backgroundBuildings.push_back(BuildingLot{
            x,
            y,
            width,
            depth,
            height,
            hueDistribution(rng),
            0.06f + unit(rng) * 0.08f,
            0.12f + brightnessBias + unit(rng) * 0.08f,
            0.10f + unit(rng) * 0.12f,
            0.10f + unit(rng) * 0.12f,
            0.10f + unit(rng) * 0.12f,
            0.12f + unit(rng) * 0.16f,
            0.18f + unit(rng) * 0.08f,
            0.82f,
            0.0f,
            0.10f + unit(rng) * 0.08f,
            0.16f + unit(rng) * 0.18f,
            DistrictType::Service,
            family,
            RoofProfile::Flat,
            form,
            VisualTier::Peripheral,
            false
        });
    };

    const int northCount = 10;
    for (int i = 0; i < northCount; ++i) {
        const float x = peripheralMinX + (static_cast<float>(i) + 0.5f) *
            ((peripheralMaxX - peripheralMinX) / static_cast<float>(northCount));
        const float y = sceneMinY - 1.2f - unit(rng) * 3.4f;
        addBackgroundLot(x, y,
                         1.0f + unit(rng) * 1.8f,
                         0.8f + unit(rng) * 1.4f,
                         9.0f + unit(rng) * 12.0f,
                         (i % 4 == 0) ? BuildingFamily::Residential : BuildingFamily::Commercial,
                         (i % 4 == 0) ? BuildingForm::Slab : BuildingForm::Tower,
                         0.02f);
    }

    const int southCount = 8;
    for (int i = 0; i < southCount; ++i) {
        const float x = peripheralMinX + (static_cast<float>(i) + 0.5f) *
            ((peripheralMaxX - peripheralMinX) / static_cast<float>(southCount));
        const float y = sceneMaxY + 1.4f + unit(rng) * 3.8f;
        addBackgroundLot(x, y,
                         0.9f + unit(rng) * 1.6f,
                         0.8f + unit(rng) * 1.2f,
                         8.0f + unit(rng) * 10.0f,
                         (i % 3 == 0) ? BuildingFamily::Utility : BuildingFamily::Residential,
                         (i % 3 == 0) ? BuildingForm::Pavilion : BuildingForm::Terrace,
                         0.00f);
    }

    const int westCount = 6;
    for (int i = 0; i < westCount; ++i) {
        const float x = sceneMinX - 1.2f - unit(rng) * 3.0f;
        const float y = peripheralMinY + (static_cast<float>(i) + 0.7f) *
            ((peripheralMaxY - peripheralMinY) / static_cast<float>(westCount));
        addBackgroundLot(x, y,
                         1.2f + unit(rng) * 1.4f,
                         1.0f + unit(rng) * 1.2f,
                         7.0f + unit(rng) * 9.0f,
                         BuildingFamily::Utility,
                         BuildingForm::Slab,
                         -0.01f);
    }

    const int eastCount = 6;
    for (int i = 0; i < eastCount; ++i) {
        const float x = sceneMaxX + 1.4f + unit(rng) * 3.4f;
        const float y = peripheralMinY + (static_cast<float>(i) + 0.6f) *
            ((peripheralMaxY - peripheralMinY) / static_cast<float>(eastCount));
        addBackgroundLot(x, y,
                         1.0f + unit(rng) * 1.8f,
                         0.9f + unit(rng) * 1.3f,
                         8.0f + unit(rng) * 11.0f,
                         (i % 2 == 0) ? BuildingFamily::Commercial : BuildingFamily::Residential,
                         (i % 2 == 0) ? BuildingForm::Tower : BuildingForm::Slab,
                         0.01f);
    }

    std::sort(backgroundBuildings.begin(), backgroundBuildings.end(),
              [](const BuildingLot& lhs, const BuildingLot& rhs) {
                  if (std::abs(lhs.y - rhs.y) > 0.02f) {
                      return lhs.y < rhs.y;
                  }
                  return lhs.x < rhs.x;
              });
    const int streetCount = 7;
    for (int i = 0; i < streetCount; ++i) {
        const float y = peripheralMinY +
            (static_cast<float>(i) + 0.5f) * ((peripheralMaxY - peripheralMinY) / streetCount);
        ambientStreets.push_back(AmbientStreet{
            peripheralMinX,
            y + unit(rng) * 0.6f - 0.3f,
            peripheralMaxX,
            y + unit(rng) * 0.6f - 0.3f,
            3.8f + unit(rng) * 2.2f,
            0.10f + unit(rng) * 0.10f,
            VisualTier::Peripheral
        });
    }
}

void CityThemeRenderer::generateAmbientCars(std::mt19937& rng) {
    ambientCars.clear();
    if (roads.empty()) {
        return;
    }

    std::vector<double> weights;
    weights.reserve(roads.size());
    for (const auto& road : roads) {
        weights.push_back(0.35 + road.focusWeight * 0.95 + (road.incident ? 0.10 : 0.0));
    }

    std::discrete_distribution<int> roadDistribution(weights.begin(), weights.end());
    std::uniform_real_distribution<float> offsetDistribution(0.0f, 1.0f);
    std::uniform_real_distribution<float> speedDistribution(0.05f, 0.18f);

    for (int i = 0; i < 22; ++i) {
        const int roadIndex = roadDistribution(rng);
        ambientCars.push_back(AmbientCar{
            roadIndex,
            offsetDistribution(rng),
            speedDistribution(rng) *
                (0.90f + roads[roadIndex].focusWeight * 0.14f) *
                (1.0f - roads[roadIndex].congestion * 0.6f),
            roads[roadIndex].incident && i % 5 == 0
        });
    }
}

void CityThemeRenderer::applyTrafficConditions(std::mt19937& rng) {
    if (roads.empty()) {
        return;
    }

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> congestionDistribution(0.05f, 0.55f);
    std::bernoulli_distribution incidentDistribution(0.10);
    std::uniform_int_distribution<int> hotRowDistribution(0, std::max(0, gridRows - 1));
    std::uniform_int_distribution<int> hotColDistribution(0, std::max(0, gridColumns - 1));

    const int hotRow = hotRowDistribution(rng);
    const int hotCol = hotColDistribution(rng);

    for (RoadSegment& road : roads) {
        const Intersection& from = intersections[road.from];
        const Intersection& to = intersections[road.to];

        float congestion = congestionDistribution(rng);
        if (road.arterial) {
            congestion += 0.10f + unit(rng) * 0.14f;
        }
        if (road.from / gridColumns == hotRow || road.to / gridColumns == hotRow ||
            road.from % gridColumns == hotCol || road.to % gridColumns == hotCol) {
            congestion = std::min(0.92f, congestion + 0.24f);
        }

        road.congestion = congestion;
        road.incident = incidentDistribution(rng) && congestion > 0.35f;
        road.signalDelay =
            (from.hasTrafficLight || to.hasTrafficLight) ? 0.18f + congestion * 0.25f : 0.0f;
    }
}

void CityThemeRenderer::refreshPairRoutes(const MapGraph& graph) {
    const int nodeCount = graph.getNodeCount();
    pairRoutes.assign(nodeCount, std::vector<PairRouteData>(nodeCount));

    for (int fromIndex = 0; fromIndex < nodeCount; ++fromIndex) {
        for (int toIndex = fromIndex + 1; toIndex < nodeCount; ++toIndex) {
            const std::vector<int> intersectionPath =
                shortestPath(nodeAnchors[fromIndex], nodeAnchors[toIndex]);
            if (intersectionPath.empty()) {
                continue;
            }

            PairRouteData routeData;
            const WasteNode& fromNode = graph.getNode(fromIndex);
            const WasteNode& toNode = graph.getNode(toIndex);
            appendDistinctPoint(routeData.polyline,
                                PlaybackPoint{fromNode.getWorldX(), fromNode.getWorldY()});
            appendDistinctPoint(routeData.polyline,
                                PlaybackPoint{intersections[nodeAnchors[fromIndex]].x,
                                              intersections[nodeAnchors[fromIndex]].y});
            routeData.segmentSpeedFactors.push_back(1.0f);

            float roadBaseDistance = 0.0f;
            float congestionPenalty = 0.0f;
            float incidentPenalty = 0.0f;
            float signalPenalty = 0.0f;

            for (std::size_t i = 0; i < intersectionPath.size(); ++i) {
                const Intersection& intersection = intersections[intersectionPath[i]];
                appendDistinctPoint(routeData.polyline,
                                    PlaybackPoint{intersection.x, intersection.y});
                if (i == 0) {
                    continue;
                }

                const int roadIndex = findRoadIndex(intersectionPath[i - 1], intersectionPath[i]);
                if (roadIndex < 0) {
                    continue;
                }

                const RoadSegment& road = roads[roadIndex];
                roadBaseDistance += road.baseLength;
                congestionPenalty += road.baseLength * road.congestion * kCongestionPenaltyScale;
                incidentPenalty += road.incident
                    ? road.baseLength * kIncidentPenaltyScale
                    : 0.0f;
                signalPenalty += road.signalDelay;
                routeData.segmentSpeedFactors.push_back(roadTravelSpeedFactor(road));
            }

            appendDistinctPoint(routeData.polyline,
                                PlaybackPoint{intersections[nodeAnchors[toIndex]].x,
                                              intersections[nodeAnchors[toIndex]].y});
            appendDistinctPoint(routeData.polyline,
                                PlaybackPoint{toNode.getWorldX(), toNode.getWorldY()});
            routeData.segmentSpeedFactors.push_back(1.0f);

            const float spurDistance =
                pointDistance(fromNode.getWorldX(), fromNode.getWorldY(),
                              intersections[nodeAnchors[fromIndex]].x,
                              intersections[nodeAnchors[fromIndex]].y) +
                pointDistance(toNode.getWorldX(), toNode.getWorldY(),
                              intersections[nodeAnchors[toIndex]].x,
                              intersections[nodeAnchors[toIndex]].y);
            const float weatherPenalty =
                roadBaseDistance * std::max(0.0f, weatherDistanceMultiplier() - 1.0f);

            routeData.insight = RouteInsight{
                graph.getNode(fromIndex).getId(),
                graph.getNode(toIndex).getId(),
                roadBaseDistance + spurDistance,
                congestionPenalty,
                incidentPenalty,
                signalPenalty,
                weatherPenalty,
                roadBaseDistance + spurDistance + congestionPenalty +
                    incidentPenalty + signalPenalty + weatherPenalty
            };
            routeData.valid = true;
            pairRoutes[fromIndex][toIndex] = routeData;

            PairRouteData reverseRoute = routeData;
            std::reverse(reverseRoute.polyline.begin(), reverseRoute.polyline.end());
            std::reverse(reverseRoute.segmentSpeedFactors.begin(),
                         reverseRoute.segmentSpeedFactors.end());
            reverseRoute.insight.fromNodeId = routeData.insight.toNodeId;
            reverseRoute.insight.toNodeId = routeData.insight.fromNodeId;
            pairRoutes[toIndex][fromIndex] = reverseRoute;
        }
    }
}

std::vector<int> CityThemeRenderer::shortestPath(int startIntersection,
                                                 int endIntersection) const {
    if (startIntersection == endIntersection) {
        return {startIntersection};
    }

    const float inf = std::numeric_limits<float>::max();
    std::vector<float> distance(intersections.size(), inf);
    std::vector<int> parent(intersections.size(), -1);

    using QueueItem = std::pair<float, int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> pq;
    distance[startIntersection] = 0.0f;
    pq.push({0.0f, startIntersection});

    while (!pq.empty()) {
        const auto [currentDistance, current] = pq.top();
        pq.pop();

        if (currentDistance > distance[current]) {
            continue;
        }
        if (current == endIntersection) {
            break;
        }

        for (const int roadIndex : roadAdjacency[current]) {
            const RoadSegment& road = roads[roadIndex];
            const int next = (road.from == current) ? road.to : road.from;
            const float nextDistance = currentDistance + roadCost(road);
            if (nextDistance < distance[next]) {
                distance[next] = nextDistance;
                parent[next] = current;
                pq.push({nextDistance, next});
            }
        }
    }

    if (parent[endIntersection] == -1) {
        return {};
    }

    std::vector<int> path;
    for (int current = endIntersection; current != -1; current = parent[current]) {
        path.push_back(current);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

float CityThemeRenderer::roadCost(const RoadSegment& road) const {
    return road.baseLength * weatherDistanceMultiplier() +
           road.baseLength * road.congestion * kCongestionPenaltyScale +
           (road.incident ? road.baseLength * kIncidentPenaltyScale : 0.0f) +
           road.signalDelay;
}

float CityThemeRenderer::roadTravelSpeedFactor(const RoadSegment& road) const {
    if (road.incident || road.congestion >= 0.82f) {
        return 0.25f;
    }
    if (road.congestion >= 0.58f) {
        return 0.50f;
    }
    return 1.0f;
}

float CityThemeRenderer::weatherDistanceMultiplier() const {
    switch (weather) {
        case CityWeather::Sunny: return 1.00f;
        case CityWeather::Cloudy: return 1.05f;
        case CityWeather::Rainy: return 1.14f;
        case CityWeather::Stormy: return 1.25f;
        default: return 1.0f;
    }
}

float CityThemeRenderer::weatherOverlayStrength() const {
    switch (weather) {
        case CityWeather::Cloudy: return 0.08f;
        case CityWeather::Rainy: return 0.14f;
        case CityWeather::Stormy: return 0.22f;
        case CityWeather::Sunny:
        default:
            return 0.03f;
    }
}

float CityThemeRenderer::computeOperationalFocus(float x, float y) const {
    const float ellipse = ellipseInfluence(x, y,
                                           operationalCenterX, operationalCenterY,
                                           operationalRadiusX, operationalRadiusY);
    const float citySpanX = std::max(1.0f, sceneMaxX - sceneMinX);
    const float citySpanY = std::max(1.0f, sceneMaxY - sceneMinY);
    const float centroidPull = 1.0f - clamp01(pointDistance(x, y, nodeCentroidX, nodeCentroidY) /
                                              std::sqrt(citySpanX * citySpanX + citySpanY * citySpanY));
    return clamp01(ellipse * 0.82f + centroidPull * 0.18f);
}

CityThemeRenderer::DistrictType CityThemeRenderer::districtFromFocus(float focusWeight) const {
    if (focusWeight >= 0.68f) return DistrictType::Commercial;
    if (focusWeight >= 0.48f) return DistrictType::Mixed;
    if (focusWeight >= 0.28f) return DistrictType::Residential;
    return DistrictType::Service;
}

CityThemeRenderer::VisualTier CityThemeRenderer::visualTierFromFocus(float focusWeight) const {
    if (focusWeight >= 0.56f) return VisualTier::Focus;
    if (focusWeight >= 0.24f) return VisualTier::Support;
    return VisualTier::Peripheral;
}

bool CityThemeRenderer::isTrafficLightGreen(const Intersection& intersection,
                                            float animationTime) const {
    const float phase = std::fmod(animationTime + intersection.cycleOffset, 6.0f);
    return phase < 2.8f;
}

bool CityThemeRenderer::isReservedVisualArea(float x, float y, float clearance,
                                             const MapGraph& graph) const {
    for (const auto& node : graph.getNodes()) {
        const float nodeClearance = node.getIsHQ() ? clearance + 0.35f : clearance + 0.15f;
        if (pointDistance(x, y, node.getWorldX(), node.getWorldY()) < nodeClearance) {
            return true;
        }
    }

    return false;
}

int CityThemeRenderer::findRoadIndex(int fromIntersection, int toIntersection) const {
    for (const int roadIndex : roadAdjacency[fromIntersection]) {
        const RoadSegment& road = roads[roadIndex];
        if ((road.from == fromIntersection && road.to == toIntersection) ||
            (road.from == toIntersection && road.to == fromIntersection)) {
            return roadIndex;
        }
    }
    return -1;
}

void CityThemeRenderer::refreshDashboardInfo() {
    float congestionSum = 0.0f;
    int incidentCount = 0;
    for (const auto& road : roads) {
        congestionSum += road.congestion;
        if (road.incident) {
            ++incidentCount;
        }
    }

    dashboardInfo.theme = EnvironmentTheme::City;
    dashboardInfo.themeLabel = "City";
    dashboardInfo.weatherLabel = toDisplayString(weather);
    dashboardInfo.subtitle = "District-planned urban map";
    dashboardInfo.atmosphereLabel =
        (weather == CityWeather::Sunny) ? "Satellite-lit mixed districts and boulevard flow"
        : (weather == CityWeather::Cloudy) ? "Soft-density urban fabric under muted circulation"
        : (weather == CityWeather::Rainy) ? "Wet-surface district logistics through parks and campus blocks"
        : "Storm-loaded boulevard routing across dense landmark districts";
    dashboardInfo.congestionLevel = roads.empty()
        ? 0.0f
        : congestionSum / static_cast<float>(roads.size());
    dashboardInfo.incidentCount = incidentCount;
    dashboardInfo.supportsWeather = true;
}

void CityThemeRenderer::drawRoutePath(IsometricRenderer& renderer,
                                      const MissionPresentation& mission,
                                      float routeRevealProgress,
                                      float animationTime) const {
    const PlaybackPath& path = mission.playbackPath;
    if (path.empty()) {
        return;
    }

    const float revealDistance = path.totalLength * std::clamp(routeRevealProgress, 0.0f, 1.0f);
    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 2.8f);

    for (std::size_t i = 1; i < path.points.size(); ++i) {
        const float startDistance = path.cumulativeDistances[i - 1];
        const float endDistance = path.cumulativeDistances[i];
        if (revealDistance <= startDistance) {
            break;
        }

        float endX = path.points[i].x;
        float endY = path.points[i].y;
        if (revealDistance < endDistance && endDistance > startDistance) {
            const float localT = (revealDistance - startDistance) / (endDistance - startDistance);
            endX = RenderUtils::lerp(path.points[i - 1].x, path.points[i].x, localT);
            endY = RenderUtils::lerp(path.points[i - 1].y, path.points[i].y, localT);
        }

        const PlaybackPoint& startPoint = path.points[i - 1];
        const ZoneVisibility zone = computeZoneVisibility(
            (startPoint.x + endX) * 0.5f,
            (startPoint.y + endY) * 0.5f,
            operationalCenterX, operationalCenterY,
            operationalRadiusX, operationalRadiusY);
        const float focus = computeOperationalFocus((startPoint.x + endX) * 0.5f,
                                                    (startPoint.y + endY) * 0.5f);
        const IsoCoord isoFrom = RenderUtils::worldToIso(startPoint.x, startPoint.y);
        const IsoCoord isoTo = RenderUtils::worldToIso(endX, endY);
        if (focus > 0.20f) {
            renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                              attenuateToZone(
                                  Color(0.90f, 0.78f, 0.44f,
                                        0.10f + zone.routeVisibility * 0.10f),
                                  zone,
                                  Color(0.08f, 0.05f, 0.02f, 0.14f),
                                  0.44f,
                                  0.04f,
                                  0.10f),
                              10.0f + focus * 3.0f);
        }
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          attenuateToZone(
                              Color(0.06f, 0.08f, 0.10f, 0.22f + zone.routeVisibility * 0.18f),
                              zone,
                              Color(0.01f, 0.02f, 0.04f, 0.28f),
                              0.26f,
                              0.10f,
                              0.10f), 9.2f);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          attenuateToZone(
                              Color(0.24f, 0.84f, 0.96f,
                                    0.26f + zone.routeVisibility * 0.18f + pulse * 0.06f),
                              zone,
                              Color(0.02f, 0.03f, 0.05f, 0.30f),
                              0.58f,
                              0.04f,
                              0.20f), 3.8f);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          attenuateToZone(
                              Color(0.94f, 0.98f, 1.0f,
                                    0.12f + zone.routeVisibility * 0.12f + pulse * 0.04f),
                              zone,
                              Color(0.02f, 0.03f, 0.05f, 0.16f),
                              0.66f,
                              0.02f,
                              0.12f), 1.2f);
    }

    for (const auto& stop : path.stops) {
        const ZoneVisibility zone = computeZoneVisibility(
            path.points[stop.pointIndex].x, path.points[stop.pointIndex].y,
            operationalCenterX, operationalCenterY,
            operationalRadiusX, operationalRadiusY);
        const float focus = computeOperationalFocus(path.points[stop.pointIndex].x,
                                                    path.points[stop.pointIndex].y);
        const IsoCoord iso = RenderUtils::worldToIso(path.points[stop.pointIndex].x,
                                                     path.points[stop.pointIndex].y);
        if (focus > 0.28f) {
            renderer.drawDiamond(iso.x, iso.y, 6.0f, 2.6f,
                                 attenuateToZone(
                                     Color(0.92f, 0.80f, 0.46f, 0.08f + focus * 0.08f),
                                     zone,
                                     Color(0.08f, 0.05f, 0.02f, 0.12f),
                                     0.42f,
                                     0.04f,
                                     0.08f));
        }
        renderer.drawDiamond(iso.x, iso.y, 4.5f, 2.0f,
                             attenuateToZone(
                                 Color(0.70f, 0.94f, 1.0f, 0.18f),
                                 zone,
                                 Color(0.02f, 0.03f, 0.05f, 0.18f),
                                 0.54f,
                                 0.04f,
                                 0.10f));
        renderer.drawDiamondOutline(iso.x, iso.y, 3.0f, 1.4f,
                                    attenuateToZone(
                                        Color(0.94f, 0.98f, 1.0f, 0.24f),
                                        zone,
                                        Color(0.02f, 0.03f, 0.05f, 0.24f),
                                        0.64f,
                                        0.02f,
                                        0.12f), 1.0f);
    }
}

void CityThemeRenderer::drawLandscapeFeature(IsometricRenderer& renderer,
                                             const LandscapeFeature& feature,
                                             float animationTime) const {
    const float minX = feature.x - feature.width * 0.5f;
    const float maxX = feature.x + feature.width * 0.5f;
    const float minY = feature.y - feature.depth * 0.5f;
    const float maxY = feature.y + feature.depth * 0.5f;
    const float zf = currentZoomFactor();
    const float wetBlend =
        (weather == CityWeather::Rainy || weather == CityWeather::Stormy) ? 0.20f : 0.0f;

    switch (feature.type) {
        case LandscapeFeatureType::Lawn: {
            Color lawn = (feature.district == DistrictType::Park)
                ? Color(0.20f, 0.36f, 0.17f, 0.82f)
                : Color(0.24f, 0.34f, 0.18f, 0.78f);
            lawn = mixColor(lawn, Color(0.16f, 0.24f, 0.28f, lawn.a), wetBlend);
            drawWorldQuadPatch(minX, minY, maxX, maxY, lawn);
            drawWorldQuadPatch(feature.x - feature.width * 0.24f,
                               feature.y - feature.depth * 0.24f,
                               feature.x + feature.width * 0.24f,
                               feature.y + feature.depth * 0.24f,
                               withAlpha(scaleColor(lawn, 1.06f), 0.26f));
            break;
        }
        case LandscapeFeatureType::Path: {
            Color path = Color(0.54f, 0.52f, 0.46f, 0.48f);
            path = mixColor(path, Color(0.28f, 0.32f, 0.36f, path.a), wetBlend * 0.85f);
            drawWorldQuadPatch(minX, minY, maxX, maxY, path);
            break;
        }
        case LandscapeFeatureType::Plaza: {
            Color plaza = (feature.district == DistrictType::Landmark)
                ? Color(0.44f, 0.46f, 0.50f, 0.54f)
                : Color(0.34f, 0.34f, 0.32f, 0.48f);
            plaza = mixColor(plaza, Color(0.22f, 0.26f, 0.30f, plaza.a), wetBlend);
            drawWorldQuadPatch(minX, minY, maxX, maxY, plaza);
            break;
        }
        case LandscapeFeatureType::Water: {
            Color water = mixColor(Color(0.08f, 0.34f, 0.48f, 0.58f),
                                   Color(0.12f, 0.48f, 0.62f, 0.62f),
                                   0.5f + 0.5f * std::sin(animationTime * 0.9f + feature.x));
            drawWorldQuadPatch(minX, minY, maxX, maxY, water);
            const IsoCoord iso = RenderUtils::worldToIso(feature.x, feature.y);
            renderer.drawDiamond(iso.x, iso.y, feature.width * 10.0f * zf,
                                 feature.depth * 4.8f * zf,
                                 Color(0.72f, 0.90f, 0.98f, 0.08f));
            break;
        }
        case LandscapeFeatureType::Court: {
            const bool campusCourt = feature.district == DistrictType::Campus;
            const Color court = campusCourt
                ? Color(0.22f, 0.44f, 0.26f, 0.64f)
                : Color(0.32f, 0.22f, 0.16f, 0.56f);
            drawWorldQuadPatch(minX, minY, maxX, maxY, court);
            const IsoCoord p0 = RenderUtils::worldToIso(minX, minY);
            const IsoCoord p1 = RenderUtils::worldToIso(maxX, minY);
            const IsoCoord p2 = RenderUtils::worldToIso(maxX, maxY);
            const IsoCoord p3 = RenderUtils::worldToIso(minX, maxY);
            renderer.drawLine(p0.x, p0.y, p1.x, p1.y, Color(0.92f, 0.96f, 0.90f, 0.20f), 1.0f);
            renderer.drawLine(p1.x, p1.y, p2.x, p2.y, Color(0.92f, 0.96f, 0.90f, 0.20f), 1.0f);
            renderer.drawLine(p2.x, p2.y, p3.x, p3.y, Color(0.92f, 0.96f, 0.90f, 0.20f), 1.0f);
            renderer.drawLine(p3.x, p3.y, p0.x, p0.y, Color(0.92f, 0.96f, 0.90f, 0.20f), 1.0f);
            break;
        }
        case LandscapeFeatureType::Grove: {
            const float pulse = 0.94f + 0.06f * std::sin(animationTime * 1.6f + feature.accent * 8.0f);
            const std::array<PlaybackPoint, 3> offsets{{
                {feature.width * -0.16f, feature.depth * -0.08f},
                {feature.width * 0.18f, feature.depth * 0.04f},
                {feature.width * -0.02f, feature.depth * 0.18f}
            }};
            for (const PlaybackPoint& offset : offsets) {
                const IsoCoord iso = RenderUtils::worldToIso(feature.x + offset.x,
                                                             feature.y + offset.y);
                renderer.drawFilledCircle(iso.x, iso.y + 5.0f * zf,
                                          (4.2f + feature.accent * 2.4f) * zf,
                                          Color(0.20f * pulse, 0.36f * pulse,
                                                0.18f * pulse, 0.74f));
                renderer.drawLine(iso.x, iso.y + 5.0f * zf,
                                  iso.x, iso.y - 4.0f * zf,
                                  Color(0.30f, 0.22f, 0.14f, 0.46f), 1.0f);
            }
            break;
        }
    }
}

void CityThemeRenderer::drawBuildingLot(IsometricRenderer& renderer,
                                        const BuildingLot& building) const {
    const IsoCoord iso = RenderUtils::worldToIso(building.x, building.y);
    const ZoneVisibility zone = computeZoneVisibility(
        building.x, building.y,
        operationalCenterX, operationalCenterY,
        operationalRadiusX, operationalRadiusY);
    if (zone.transition <= 0.08f) {
        return;
    }
    const float zf = currentZoomFactor();

    Color material(0.42f, 0.46f, 0.52f, 0.98f);
    Color accent(0.58f, 0.64f, 0.72f, 0.98f);
    switch (building.family) {
        case BuildingFamily::Utility:
            material = Color(0.42f, 0.38f, 0.32f, 0.98f);
            accent = Color(0.62f, 0.56f, 0.44f, 0.98f);
            break;
        case BuildingFamily::Commercial:
            material = Color(0.30f, 0.40f, 0.54f, 0.98f);
            accent = Color(0.56f, 0.72f, 0.88f, 0.98f);
            break;
        case BuildingFamily::Residential:
            material = Color(0.54f, 0.44f, 0.38f, 0.98f);
            accent = Color(0.82f, 0.68f, 0.56f, 0.98f);
            break;
        case BuildingFamily::Civic:
            material = Color(0.50f, 0.52f, 0.48f, 0.98f);
            accent = Color(0.82f, 0.84f, 0.78f, 0.98f);
            break;
        case BuildingFamily::Landmark:
            material = Color(0.58f, 0.64f, 0.72f, 0.98f);
            accent = Color(0.92f, 0.96f, 1.0f, 0.98f);
            break;
    }

    material = biasColor(material,
                         building.hueShift * 0.35f,
                         building.hueShift * 0.14f,
                         -building.hueShift * 0.16f);
    accent = biasColor(accent,
                       building.hueShift * 0.26f,
                       building.hueShift * 0.10f,
                       -building.hueShift * 0.12f);

    const float brightness = building.facadeBrightness *
        (building.visualTier == VisualTier::Focus ? 1.15f : 1.02f);
    const Color facade = mixColor(material, accent, 0.35f + building.saturation * 0.6f);
    const Color roof = scaleColor(mixColor(facade, Color(0.82f, 0.86f, 0.92f, 0.98f), 0.18f),
                                  0.88f + brightness * 0.45f);
    const Color leftFacade = biasColor(facade, -0.04f, -0.02f, 0.05f);
    const Color rightFacade = biasColor(facade, 0.04f, 0.02f, -0.03f);
    const Color leftSide = scaleColor(leftFacade,
                                      0.72f + brightness * 0.30f +
                                      building.edgeHighlight * 0.08f);
    const Color rightSide = scaleColor(rightFacade,
                                       0.92f + brightness * 0.34f +
                                       building.edgeHighlight * 0.12f);
    const Color outline = withAlpha(scaleColor(Color(0.92f, 0.94f, 0.98f, 1.0f),
                                               0.55f + building.edgeHighlight * 0.45f),
                                    0.42f + building.edgeHighlight * 0.35f);
    const Color windowColor = withAlpha(
        mixColor(Color(0.54f, 0.66f, 0.82f, 1.0f),
                 Color(0.96f, 0.72f, 0.34f, 1.0f),
                 building.windowWarmth),
        (0.06f + building.windowDensity * 0.12f) *
            (building.visualTier == VisualTier::Peripheral ? 0.72f : 1.0f));

    const Color voidTint(0.01f, 0.02f, 0.04f, 1.0f);
    const Color attenuatedRoof = attenuateToZone(
        roof, zone, voidTint,
        building.landmark ? 0.22f : 0.08f,
        building.landmark ? 0.12f : 0.32f,
        0.08f);
    const Color attenuatedLeft = attenuateToZone(
        leftSide, zone, voidTint,
        building.landmark ? 0.18f : 0.06f,
        0.38f,
        0.08f);
    const Color attenuatedRight = attenuateToZone(
        rightSide, zone, voidTint,
        building.landmark ? 0.20f : 0.08f,
        0.34f,
        0.08f);
    const Color attenuatedOutline = attenuateToZone(
        outline, zone, Color(0.00f, 0.00f, 0.00f, 1.0f),
        0.20f,
        0.26f,
        0.03f);
    const Color attenuatedWindow = attenuateToZone(
        windowColor, zone, voidTint,
        0.24f,
        0.16f,
        0.01f);
    const Color shadowColor = attenuateToZone(
        Color(0.02f, 0.02f, 0.03f, 0.24f), zone, Color(0.00f, 0.00f, 0.00f, 0.24f),
        0.00f,
        0.00f,
        0.02f);

    const float baseWidth = std::max(4.0f * zf, building.width * 38.0f * zf);
    const float baseDepth = std::max(3.0f * zf, building.depth * 28.0f * zf);
    const float totalHeight = std::max(
        6.0f * zf,
        building.height * (building.form == BuildingForm::TwinTower ? 1.46f : 1.32f) * zf);
    const float shadowScale = (building.visualTier == VisualTier::Focus) ? 1.12f : 1.04f;
    renderer.drawDiamond(iso.x, iso.y + 8.5f * zf, baseWidth * shadowScale,
                         baseDepth * 0.54f, shadowColor);

    auto drawMass = [&](float offsetX, float offsetY,
                        float width, float depth, float height,
                        float windowScale) -> BlockFaces {
        const BlockFaces faces = makeBlockFaces(iso.x + offsetX, iso.y + offsetY,
                                                width, depth, height);
        drawBlockMass(faces, attenuatedRoof, attenuatedLeft, attenuatedRight);

        const int bands = std::max(1, static_cast<int>(std::round(
            height / (5.4f * zf) * building.windowDensity * windowScale)));
        drawFaceBands(faces.left, bands,
                      0.10f, withAlpha(attenuatedWindow, attenuatedWindow.a * building.occupancy * 0.82f),
                      0.8f * zf + 0.35f);
        drawFaceBands(faces.right, bands + (building.family == BuildingFamily::Commercial ? 1 : 0),
                      0.12f, withAlpha(attenuatedWindow, attenuatedWindow.a * building.occupancy),
                      0.8f * zf + 0.35f);
        drawOutlineLoop(faces.top, attenuatedOutline, std::max(0.8f, 0.85f * zf));
        renderer.drawLine(faces.top[1].x, faces.top[1].y, faces.right[2].x, faces.right[2].y,
                          attenuatedOutline, std::max(0.8f, 0.9f * zf));
        renderer.drawLine(faces.top[3].x, faces.top[3].y, faces.left[2].x, faces.left[2].y,
                          attenuatedOutline, std::max(0.8f, 0.9f * zf));
        return faces;
    };

    const float podiumHeight = std::max(2.4f * zf, totalHeight * building.podiumRatio);
    const float mainHeight = std::max(4.0f * zf, totalHeight - podiumHeight);

    auto drawRoofAccent = [&](const BlockFaces& faces,
                              float offsetY,
                              float width,
                              float depth,
                              float height,
                              float windowScale) {
        switch (building.roofProfile) {
            case RoofProfile::Flat:
                drawMass(baseWidth * 0.04f, offsetY,
                         width * 0.26f, depth * 0.24f,
                         std::max(1.4f * zf, height * 0.10f), windowScale * 0.25f);
                break;
            case RoofProfile::Stepped:
                drawMass(0.0f, offsetY,
                         width * 0.70f, depth * 0.70f,
                         std::max(1.8f * zf, height * 0.18f), windowScale * 0.38f);
                break;
            case RoofProfile::Terrace:
                drawMass(-width * 0.12f, offsetY + 0.8f * zf,
                         width * 0.56f, depth * 0.58f,
                         std::max(1.8f * zf, height * 0.14f), windowScale * 0.32f);
                renderer.drawDiamond(faces.top[0].x,
                                     faces.top[0].y + depth * 0.48f,
                                     width * 0.12f, depth * 0.06f,
                                     attenuateToZone(
                                         Color(0.94f, 0.74f, 0.40f, 0.10f),
                                         zone,
                                         Color(0.02f, 0.02f, 0.04f, 0.10f),
                                         0.30f,
                                         0.08f,
                                         0.04f));
                break;
            case RoofProfile::Crown:
                drawMass(0.0f, offsetY,
                         width * 0.34f, depth * 0.34f,
                         std::max(2.0f * zf, height * 0.12f), windowScale * 0.40f);
                renderer.drawLine(faces.top[0].x, faces.top[0].y,
                                  faces.top[0].x, faces.top[0].y - 7.0f * zf,
                                  attenuateToZone(
                                      Color(0.82f, 0.88f, 0.96f, 0.18f),
                                      zone,
                                      Color(0.02f, 0.03f, 0.05f, 0.18f),
                                      0.34f,
                                      0.08f,
                                      0.05f), 1.0f);
                break;
            case RoofProfile::Spire:
                drawMass(0.0f, offsetY,
                         width * 0.30f, depth * 0.32f,
                         std::max(1.8f * zf, height * 0.10f), windowScale * 0.40f);
                renderer.drawLine(faces.top[0].x, faces.top[0].y - 1.0f * zf,
                                  faces.top[0].x, faces.top[0].y - 12.0f * zf,
                                  attenuateToZone(
                                      Color(0.90f, 0.96f, 1.0f, 0.28f),
                                      zone,
                                      Color(0.02f, 0.03f, 0.05f, 0.28f),
                                      0.38f,
                                      0.06f,
                                      0.06f), 1.2f);
                break;
        }
    };

    switch (building.form) {
        case BuildingForm::Pavilion: {
            const BlockFaces faces = drawMass(0.0f, 1.0f * zf,
                                              baseWidth * 0.88f, baseDepth * 0.82f,
                                              podiumHeight + mainHeight * 0.36f, 0.42f);
            drawRoofAccent(faces,
                           -podiumHeight - mainHeight * 0.36f,
                           baseWidth * 0.88f, baseDepth * 0.82f,
                           podiumHeight, 0.24f);
            break;
        }

        case BuildingForm::Terrace: {
            const int units = 3 + (building.annexRatio > 0.30f ? 1 : 0);
            for (int i = 0; i < units; ++i) {
                const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(units);
                const float offsetX = RenderUtils::lerp(-baseWidth * 0.28f, baseWidth * 0.28f, t);
                const float width = baseWidth * (0.18f + (i % 2 == 0 ? 0.02f : -0.01f));
                const float height = podiumHeight * 0.82f + mainHeight * (0.38f + 0.06f * (i % 2));
                drawMass(offsetX, 2.0f * zf,
                         width, baseDepth * 0.40f,
                         height, 0.30f);
            }
            break;
        }

        case BuildingForm::Slab: {
            const BlockFaces slabFaces = drawMass(0.0f, 1.0f * zf,
                                                  baseWidth * 0.96f, baseDepth * 0.86f,
                                                  podiumHeight * 0.78f + mainHeight * 0.56f,
                                                  0.72f);
            if (building.family == BuildingFamily::Civic) {
                drawMass(-baseWidth * 0.22f, -podiumHeight * 0.10f,
                         baseWidth * 0.36f, baseDepth * 0.34f,
                         mainHeight * 0.24f, 0.22f);
            } else if (building.annexRatio > 0.28f) {
                drawMass(baseWidth * 0.22f, 3.2f * zf,
                         baseWidth * 0.30f, baseDepth * 0.28f,
                         podiumHeight * 0.28f, 0.18f);
            }
            drawRoofAccent(slabFaces,
                           -(podiumHeight * 0.78f + mainHeight * 0.56f),
                           baseWidth * 0.96f, baseDepth * 0.86f,
                           mainHeight, 0.30f);
            break;
        }

        case BuildingForm::Courtyard: {
            const float wingHeight = podiumHeight * 0.70f + mainHeight * 0.46f;
            const float wingW = baseWidth * 0.28f;
            const float wingD = baseDepth * 0.26f;
            drawMass(-baseWidth * 0.22f, 0.8f * zf, wingW, baseDepth * 0.72f, wingHeight, 0.42f);
            drawMass(baseWidth * 0.22f, 0.8f * zf, wingW, baseDepth * 0.72f, wingHeight, 0.42f);
            drawMass(0.0f, -baseDepth * 0.18f, baseWidth * 0.70f, wingD,
                     wingHeight * 0.92f, 0.34f);
            const IsoCoord courtIso = RenderUtils::worldToIso(building.x, building.y + building.depth * 0.10f);
            renderer.drawDiamond(courtIso.x, courtIso.y, baseWidth * 0.16f, baseDepth * 0.08f,
                                 Color(0.26f, 0.34f, 0.22f, 0.16f));
            break;
        }

        case BuildingForm::TwinTower: {
            const float podiumW = baseWidth * 1.12f;
            const float podiumD = baseDepth * 0.94f;
            drawMass(0.0f, 2.2f * zf, podiumW, podiumD, podiumHeight * 0.92f, 0.34f);

            const float towerOffset = baseWidth * 0.18f;
            const float towerW = baseWidth * 0.34f;
            const float towerD = baseDepth * 0.32f;
            const float towerH = mainHeight * 0.96f;
            const BlockFaces leftTower = drawMass(-towerOffset, -podiumHeight * 0.90f,
                                                  towerW, towerD, towerH, 1.10f);
            const BlockFaces rightTower = drawMass(towerOffset, -podiumHeight * 0.90f,
                                                   towerW, towerD, towerH * 0.98f, 1.10f);

            drawMass(-towerOffset, -podiumHeight - towerH,
                     towerW * 0.76f, towerD * 0.76f,
                     towerH * 0.18f, 0.46f);
            drawMass(towerOffset, -podiumHeight - towerH * 0.98f,
                     towerW * 0.76f, towerD * 0.76f,
                     towerH * 0.18f, 0.46f);

            const float bridgeY = iso.y - podiumHeight - towerH * 0.46f;
            glColor4f(0.82f, 0.88f, 0.96f, 0.46f);
            glBegin(GL_QUADS);
            glVertex2f(iso.x - towerOffset * 0.74f, bridgeY - 3.2f * zf);
            glVertex2f(iso.x + towerOffset * 0.74f, bridgeY - 3.2f * zf);
            glVertex2f(iso.x + towerOffset * 0.68f, bridgeY + 2.4f * zf);
            glVertex2f(iso.x - towerOffset * 0.68f, bridgeY + 2.4f * zf);
            glEnd();

            renderer.drawLine(leftTower.top[0].x, leftTower.top[0].y - 1.0f * zf,
                              leftTower.top[0].x, leftTower.top[0].y - 14.0f * zf,
                              attenuateToZone(
                                  Color(0.94f, 0.98f, 1.0f, 0.30f),
                                  zone,
                                  Color(0.02f, 0.03f, 0.05f, 0.30f),
                                  0.40f,
                                  0.06f,
                                  0.08f), 1.2f);
            renderer.drawLine(rightTower.top[0].x, rightTower.top[0].y - 1.0f * zf,
                              rightTower.top[0].x, rightTower.top[0].y - 13.0f * zf,
                              attenuateToZone(
                                  Color(0.94f, 0.98f, 1.0f, 0.30f),
                                  zone,
                                  Color(0.02f, 0.03f, 0.05f, 0.30f),
                                  0.40f,
                                  0.06f,
                                  0.08f), 1.2f);
            renderer.drawDiamond(iso.x, iso.y - podiumHeight - towerH * 0.44f,
                                 baseWidth * 0.18f, baseDepth * 0.06f,
                                 attenuateToZone(
                                     Color(0.82f, 0.90f, 0.98f, 0.10f),
                                     zone,
                                     Color(0.02f, 0.03f, 0.05f, 0.10f),
                                     0.28f,
                                     0.08f,
                                     0.04f));
            break;
        }

        case BuildingForm::Tower:
        default: {
            drawMass(0.0f, 1.4f * zf, baseWidth, baseDepth, podiumHeight, 0.38f);

            float towerWidth = baseWidth * building.taper;
            float towerDepth = baseDepth * (building.taper + 0.02f);
            if (building.family == BuildingFamily::Residential) {
                towerWidth = baseWidth * (building.taper - 0.08f);
                towerDepth = baseDepth * (building.taper - 0.06f);
            }
            if (building.family == BuildingFamily::Landmark) {
                towerWidth = baseWidth * (building.taper - 0.02f);
                towerDepth = baseDepth * (building.taper - 0.02f);
            }

            const BlockFaces mainFaces = drawMass(0.0f, -podiumHeight,
                                                  towerWidth, towerDepth, mainHeight, 1.0f);

            if (building.family == BuildingFamily::Residential) {
                drawMass(-baseWidth * 0.20f, 3.2f * zf,
                         baseWidth * 0.28f, baseDepth * 0.34f,
                         std::max(2.2f * zf, podiumHeight * 0.28f), 0.22f);
            } else if (building.family == BuildingFamily::Commercial && building.annexRatio > 0.22f) {
                drawMass(baseWidth * 0.20f, 3.0f * zf,
                         baseWidth * 0.26f, baseDepth * 0.24f,
                         std::max(2.0f * zf, podiumHeight * 0.24f), 0.18f);
            }

            drawRoofAccent(mainFaces,
                           -podiumHeight - mainHeight,
                           towerWidth, towerDepth,
                           mainHeight, 0.40f);
            break;
        }
    }

    if (building.visualTier == VisualTier::Focus) {
        renderer.drawDiamond(iso.x, iso.y - totalHeight - 2.0f * zf,
                             baseWidth * 0.18f, baseDepth * 0.08f,
                             attenuateToZone(
                                 Color(0.70f, 0.82f, 0.96f, 0.04f + building.edgeHighlight * 0.05f),
                                 zone,
                                 Color(0.02f, 0.03f, 0.05f, 0.08f),
                                 0.22f,
                                 0.06f,
                                 0.03f));
    }
}

// ============================================================================
// ASSET LIBRARY DRAW FUNCTIONS
// ============================================================================

void CityThemeRenderer::drawPresetBuilding(IsometricRenderer& renderer,
                                            const PlacedPresetBuilding& placed) const {
    const CityAssets::BuildingPreset& p = *placed.preset;
    const IsoCoord iso = RenderUtils::worldToIso(placed.worldX, placed.worldY);
    const ZoneVisibility zone = computeZoneVisibility(
        placed.worldX, placed.worldY,
        operationalCenterX, operationalCenterY,
        operationalRadiusX, operationalRadiusY);
    if (zone.transition <= 0.10f) {
        return;
    }
    const float zf = currentZoomFactor();

    // Compute authored dual-tone colors from preset
    const Color facade = mixColor(p.material, p.accent, 0.40f);
    const float brightness = (p.category == CityAssets::BuildingCategory::Landmark) ? 1.18f : 1.04f;

    const Color roof = scaleColor(
        mixColor(facade, Color(0.84f, 0.88f, 0.94f, 0.98f), 0.20f),
        0.90f + brightness * 0.40f);
    const Color leftSide = scaleColor(
        biasColor(facade, p.leftBias.dr, p.leftBias.dg, p.leftBias.db),
        0.74f + brightness * 0.28f);
    const Color rightSide = scaleColor(
        biasColor(facade, p.rightBias.dr, p.rightBias.dg, p.rightBias.db),
        0.94f + brightness * 0.32f);
    const Color outline = withAlpha(
        scaleColor(Color(0.92f, 0.94f, 0.98f, 1.0f), 0.60f),
        0.44f);
    const Color windowColor = withAlpha(
        mixColor(Color(0.54f, 0.66f, 0.82f, 1.0f),
                 Color(0.96f, 0.72f, 0.34f, 1.0f),
                 p.windowWarmth),
        0.06f + p.windowDensity * 0.12f);

    const Color voidTint(0.01f, 0.02f, 0.04f, 1.0f);
    const Color attenuatedRoof = attenuateToZone(
        roof, zone, voidTint,
        p.category == CityAssets::BuildingCategory::Landmark ? 0.22f : 0.08f,
        p.category == CityAssets::BuildingCategory::Landmark ? 0.12f : 0.30f,
        0.08f);
    const Color attenuatedLeft = attenuateToZone(
        leftSide, zone, voidTint,
        p.category == CityAssets::BuildingCategory::Landmark ? 0.18f : 0.06f,
        0.36f,
        0.08f);
    const Color attenuatedRight = attenuateToZone(
        rightSide, zone, voidTint,
        p.category == CityAssets::BuildingCategory::Landmark ? 0.20f : 0.08f,
        0.32f,
        0.08f);
    const Color attenuatedOutline = attenuateToZone(
        outline, zone, Color(0.00f, 0.00f, 0.00f, 1.0f),
        0.20f,
        0.24f,
        0.03f);
    const Color attenuatedWindow = attenuateToZone(
        windowColor, zone, voidTint,
        0.24f,
        0.14f,
        0.01f);
    const Color shadowColor = attenuateToZone(
        Color(0.02f, 0.02f, 0.03f, 0.24f), zone, Color(0.00f, 0.00f, 0.00f, 0.24f),
        0.00f,
        0.00f,
        0.02f);

    const float baseWidth = std::max(4.0f * zf, p.width * 38.0f * zf);
    const float baseDepth = std::max(3.0f * zf, p.depth * 28.0f * zf);
    const float totalHeight = std::max(6.0f * zf,
        p.height * placed.heightScale * (p.isTwinTower ? 1.46f : 1.32f) * zf);

    // Shadow
    const float shadowScale = (p.category == CityAssets::BuildingCategory::Landmark) ? 1.14f : 1.06f;
    renderer.drawDiamond(iso.x, iso.y + 8.5f * zf, baseWidth * shadowScale,
                         baseDepth * 0.54f, shadowColor);

    // Drawing helper
    auto drawMass = [&](float offsetX, float offsetY,
                        float w, float d, float h,
                        float windowScale) -> BlockFaces {
        const BlockFaces faces = makeBlockFaces(iso.x + offsetX, iso.y + offsetY,
                                                w, d, h);
        drawBlockMass(faces, attenuatedRoof, attenuatedLeft, attenuatedRight);

        const int bands = std::max(1, static_cast<int>(std::round(
            h / (5.4f * zf) * p.windowDensity * windowScale)));
        drawFaceBands(faces.left, bands, 0.10f,
                      withAlpha(attenuatedWindow, attenuatedWindow.a * 0.82f),
                      0.8f * zf + 0.35f);
        drawFaceBands(faces.right, bands, 0.12f,
                      attenuatedWindow, 0.8f * zf + 0.35f);
        drawOutlineLoop(faces.top, attenuatedOutline, std::max(0.8f, 0.85f * zf));
        renderer.drawLine(faces.top[1].x, faces.top[1].y,
                          faces.right[2].x, faces.right[2].y,
                          attenuatedOutline, std::max(0.8f, 0.9f * zf));
        renderer.drawLine(faces.top[3].x, faces.top[3].y,
                          faces.left[2].x, faces.left[2].y,
                          attenuatedOutline, std::max(0.8f, 0.9f * zf));
        return faces;
    };

    const float podiumHeight = std::max(2.4f * zf, totalHeight * p.podiumRatio);
    const float mainHeight = std::max(4.0f * zf, totalHeight - podiumHeight);

    if (p.isTwinTower) {
        // === TWIN TOWER FORM ===
        const float podiumW = baseWidth * 1.14f;
        const float podiumD = baseDepth * 0.96f;
        drawMass(0.0f, 2.2f * zf, podiumW, podiumD, podiumHeight * 0.92f, 0.34f);

        const float towerOffset = baseWidth * 0.20f;
        const float towerW = baseWidth * 0.36f;
        const float towerD = baseDepth * 0.34f;
        const float towerH = mainHeight * 0.96f;
        const BlockFaces leftTower = drawMass(-towerOffset, -podiumHeight * 0.90f,
                                              towerW, towerD, towerH, 1.12f);
        const BlockFaces rightTower = drawMass(towerOffset, -podiumHeight * 0.90f,
                                               towerW, towerD, towerH * 0.98f, 1.12f);

        // Upper tier narrowing
        drawMass(-towerOffset, -podiumHeight - towerH,
                 towerW * 0.74f, towerD * 0.74f,
                 towerH * 0.20f, 0.48f);
        drawMass(towerOffset, -podiumHeight - towerH * 0.98f,
                 towerW * 0.74f, towerD * 0.74f,
                 towerH * 0.20f, 0.48f);

        // Third tier
        drawMass(-towerOffset, -podiumHeight - towerH * 1.20f,
                 towerW * 0.52f, towerD * 0.52f,
                 towerH * 0.10f, 0.30f);
        drawMass(towerOffset, -podiumHeight - towerH * 1.18f,
                 towerW * 0.52f, towerD * 0.52f,
                 towerH * 0.10f, 0.30f);

        // Skybridge
        const float bridgeY = iso.y - podiumHeight - towerH * 0.46f;
        glColor4f(0.84f, 0.90f, 0.98f, 0.50f);
        glBegin(GL_QUADS);
        glVertex2f(iso.x - towerOffset * 0.76f, bridgeY - 3.4f * zf);
        glVertex2f(iso.x + towerOffset * 0.76f, bridgeY - 3.4f * zf);
        glVertex2f(iso.x + towerOffset * 0.70f, bridgeY + 2.6f * zf);
        glVertex2f(iso.x - towerOffset * 0.70f, bridgeY + 2.6f * zf);
        glEnd();

        // Spires
        renderer.drawLine(leftTower.top[0].x, leftTower.top[0].y - 1.0f * zf,
                          leftTower.top[0].x, leftTower.top[0].y - 18.0f * zf,
                          attenuateToZone(
                              Color(0.94f, 0.98f, 1.0f, 0.34f),
                              zone,
                              Color(0.02f, 0.03f, 0.05f, 0.34f),
                              0.40f,
                              0.06f,
                              0.08f), 1.4f);
        renderer.drawLine(rightTower.top[0].x, rightTower.top[0].y - 1.0f * zf,
                          rightTower.top[0].x, rightTower.top[0].y - 17.0f * zf,
                          attenuateToZone(
                              Color(0.94f, 0.98f, 1.0f, 0.34f),
                              zone,
                              Color(0.02f, 0.03f, 0.05f, 0.34f),
                              0.40f,
                              0.06f,
                              0.08f), 1.4f);

        // Glow halo at top
        renderer.drawDiamond(iso.x, iso.y - podiumHeight - towerH * 0.44f,
                             baseWidth * 0.20f, baseDepth * 0.08f,
                             attenuateToZone(
                                 Color(0.84f, 0.92f, 0.98f, 0.12f),
                                 zone,
                                 Color(0.02f, 0.03f, 0.05f, 0.12f),
                                 0.28f,
                                 0.08f,
                                 0.04f));
    } else {
        // === STANDARD BUILDING FORM ===
        // Podium base
        drawMass(0.0f, 1.4f * zf, baseWidth, baseDepth, podiumHeight, 0.38f);

        // Main tower
        const float towerWidth = baseWidth * p.taper;
        const float towerDepth = baseDepth * (p.taper + 0.02f);
        const BlockFaces mainFaces = drawMass(0.0f, -podiumHeight,
                                              towerWidth, towerDepth, mainHeight, 1.0f);

        // Roof accent based on preset style
        switch (p.roof) {
            case CityAssets::RoofStyle::Flat:
                drawMass(baseWidth * 0.04f, -podiumHeight - mainHeight,
                         towerWidth * 0.26f, towerDepth * 0.24f,
                         std::max(1.4f * zf, mainHeight * 0.10f), 0.25f);
                break;
            case CityAssets::RoofStyle::Stepped:
                drawMass(0.0f, -podiumHeight - mainHeight,
                         towerWidth * 0.72f, towerDepth * 0.72f,
                         std::max(1.8f * zf, mainHeight * 0.18f), 0.38f);
                break;
            case CityAssets::RoofStyle::Crown:
                drawMass(0.0f, -podiumHeight - mainHeight,
                         towerWidth * 0.36f, towerDepth * 0.36f,
                         std::max(2.0f * zf, mainHeight * 0.12f), 0.40f);
                renderer.drawLine(mainFaces.top[0].x, mainFaces.top[0].y,
                                  mainFaces.top[0].x, mainFaces.top[0].y - 7.0f * zf,
                                  attenuateToZone(
                                      Color(0.82f, 0.88f, 0.96f, 0.20f),
                                      zone,
                                      Color(0.02f, 0.03f, 0.05f, 0.20f),
                                      0.34f,
                                      0.08f,
                                      0.05f), 1.0f);
                break;
            case CityAssets::RoofStyle::Spire:
                drawMass(0.0f, -podiumHeight - mainHeight,
                         towerWidth * 0.30f, towerDepth * 0.32f,
                         std::max(1.8f * zf, mainHeight * 0.10f), 0.40f);
                renderer.drawLine(mainFaces.top[0].x, mainFaces.top[0].y - 1.0f * zf,
                                  mainFaces.top[0].x, mainFaces.top[0].y - 14.0f * zf,
                                  attenuateToZone(
                                      Color(0.90f, 0.96f, 1.0f, 0.30f),
                                      zone,
                                      Color(0.02f, 0.03f, 0.05f, 0.30f),
                                      0.38f,
                                      0.06f,
                                      0.06f), 1.2f);
                break;
        }
    }

    // Focus glow for landmarks
    if (p.category == CityAssets::BuildingCategory::Landmark ||
        p.category == CityAssets::BuildingCategory::OfficeTower) {
        renderer.drawDiamond(iso.x, iso.y - totalHeight - 2.0f * zf,
                             baseWidth * 0.18f, baseDepth * 0.08f,
                             attenuateToZone(
                                 Color(0.70f, 0.82f, 0.96f, 0.04f),
                                 zone,
                                 Color(0.02f, 0.03f, 0.05f, 0.08f),
                                 0.22f,
                                 0.06f,
                                 0.03f));
    }
}

void CityThemeRenderer::drawPresetEnvironment(IsometricRenderer& renderer,
                                               const PlacedEnvironment& placed,
                                               float animationTime) const {
    const CityAssets::EnvironmentPreset& p = *placed.preset;
    const ZoneVisibility zone = computeZoneVisibility(
        placed.worldX, placed.worldY,
        operationalCenterX, operationalCenterY,
        operationalRadiusX, operationalRadiusY);
    if (zone.transition <= 0.08f) {
        return;
    }
    const float zf = currentZoomFactor();
    const float halfW = placed.spanX * 0.5f;
    const float halfD = placed.spanY * 0.5f;
    const Color voidTint(0.01f, 0.02f, 0.04f, 1.0f);

    switch (p.category) {
    case CityAssets::EnvironmentCategory::Park: {
        // Green ground patch
        drawWorldQuadPatch(placed.worldX - halfW, placed.worldY - halfD,
                           placed.worldX + halfW, placed.worldY + halfD,
                           attenuateToZone(p.primary, zone, voidTint, 0.08f, 0.18f));
        // Inner accent
        drawWorldQuadPatch(placed.worldX - halfW * 0.5f, placed.worldY - halfD * 0.5f,
                           placed.worldX + halfW * 0.5f, placed.worldY + halfD * 0.5f,
                           attenuateToZone(p.secondary, zone, voidTint, 0.10f, 0.18f));
        // Tree clusters
        for (int t = 0; t < p.featureCount; ++t) {
            const float angle = 6.28318f * static_cast<float>(t) / static_cast<float>(std::max(1, p.featureCount));
            const float tx = placed.worldX + std::cos(angle) * halfW * 0.55f;
            const float ty = placed.worldY + std::sin(angle) * halfD * 0.45f;
            const IsoCoord treeIso = RenderUtils::worldToIso(tx, ty);
            const float pulse = 0.94f + 0.06f * std::sin(animationTime * 1.6f + static_cast<float>(t));
            renderer.drawFilledCircle(treeIso.x, treeIso.y + 4.0f * zf,
                                      (4.0f + static_cast<float>(t % 2) * 1.4f) * zf,
                                      attenuateToZone(
                                          Color(0.18f * pulse, 0.34f * pulse,
                                                0.16f * pulse, 0.76f),
                                          zone,
                                          voidTint,
                                          0.14f,
                                          0.14f,
                                          0.06f));
            renderer.drawLine(treeIso.x, treeIso.y + 4.0f * zf,
                              treeIso.x, treeIso.y - 2.0f * zf,
                              attenuateToZone(
                                  Color(0.30f, 0.22f, 0.14f, 0.46f),
                                  zone,
                                  voidTint,
                                  0.12f,
                                  0.20f,
                                  0.04f), 1.0f);
        }
        // Central pond/fountain
        if (p.hasCentralFeature) {
            const IsoCoord cIso = RenderUtils::worldToIso(placed.worldX, placed.worldY);
            const Color water = mixColor(
                Color(0.08f, 0.34f, 0.48f, 0.58f),
                Color(0.12f, 0.48f, 0.62f, 0.62f),
                0.5f + 0.5f * std::sin(animationTime * 0.9f));
            renderer.drawDiamond(cIso.x, cIso.y, halfW * 16.0f * zf, halfD * 8.0f * zf,
                                 attenuateToZone(water, zone, voidTint, 0.18f, 0.08f, 0.06f));
        }
        break;
    }

    case CityAssets::EnvironmentCategory::Plaza: {
        drawWorldQuadPatch(placed.worldX - halfW, placed.worldY - halfD,
                           placed.worldX + halfW, placed.worldY + halfD,
                           attenuateToZone(p.primary, zone, voidTint, 0.08f, 0.24f));
        if (p.hasBorder) {
            const IsoCoord corners[] = {
                RenderUtils::worldToIso(placed.worldX - halfW, placed.worldY - halfD),
                RenderUtils::worldToIso(placed.worldX + halfW, placed.worldY - halfD),
                RenderUtils::worldToIso(placed.worldX + halfW, placed.worldY + halfD),
                RenderUtils::worldToIso(placed.worldX - halfW, placed.worldY + halfD)
            };
            for (int e = 0; e < 4; ++e) {
                const int next = (e + 1) % 4;
                renderer.drawLine(corners[e].x, corners[e].y,
                                  corners[next].x, corners[next].y,
                                  attenuateToZone(
                                      Color(0.72f, 0.74f, 0.78f, 0.22f),
                                      zone,
                                      voidTint,
                                      0.16f,
                                      0.20f,
                                      0.04f), 1.0f);
            }
        }
        if (p.hasCentralFeature) {
            const IsoCoord cIso = RenderUtils::worldToIso(placed.worldX, placed.worldY);
            renderer.drawDiamond(cIso.x, cIso.y, halfW * 10.0f * zf, halfD * 5.0f * zf,
                                 attenuateToZone(p.secondary, zone, voidTint, 0.12f, 0.18f, 0.04f));
        }
        break;
    }

    case CityAssets::EnvironmentCategory::TreeCluster: {
        for (int t = 0; t < p.featureCount; ++t) {
            const float angle = 6.28318f * static_cast<float>(t) / static_cast<float>(std::max(1, p.featureCount));
            const float r = 0.3f + static_cast<float>(t % 3) * 0.2f;
            const float tx = placed.worldX + std::cos(angle) * halfW * r;
            const float ty = placed.worldY + std::sin(angle) * halfD * r;
            const IsoCoord treeIso = RenderUtils::worldToIso(tx, ty);
            const float pulse = 0.94f + 0.06f * std::sin(animationTime * 1.4f + static_cast<float>(t) * 0.8f);
            const float rad = (3.6f + static_cast<float>(t % 3) * 1.8f) * zf;
            renderer.drawFilledCircle(treeIso.x, treeIso.y + 4.0f * zf, rad,
                                      attenuateToZone(
                                          Color(p.primary.r * pulse, p.primary.g * pulse,
                                                p.primary.b * pulse, p.primary.a),
                                          zone,
                                          voidTint,
                                          0.14f,
                                          0.14f,
                                          0.06f));
            renderer.drawLine(treeIso.x, treeIso.y + 4.0f * zf,
                              treeIso.x, treeIso.y - 2.0f * zf,
                              attenuateToZone(
                                  Color(0.30f, 0.22f, 0.14f, 0.40f),
                                  zone,
                                  voidTint,
                                  0.12f,
                                  0.20f,
                                  0.04f), 1.0f);
        }
        break;
    }

    case CityAssets::EnvironmentCategory::RoadsideTrees: {
        for (int t = 0; t < p.featureCount; ++t) {
            const float frac = (static_cast<float>(t) + 0.5f) / static_cast<float>(p.featureCount);
            const float tx = placed.worldX - halfW + frac * halfW * 2.0f;
            const float ty = placed.worldY;
            const IsoCoord treeIso = RenderUtils::worldToIso(tx, ty);
            const float pulse = 0.94f + 0.06f * std::sin(animationTime * 1.5f + static_cast<float>(t));
            renderer.drawFilledCircle(treeIso.x, treeIso.y + 3.5f * zf,
                                      (3.2f + static_cast<float>(t % 2) * 0.8f) * zf,
                                      attenuateToZone(
                                          Color(p.primary.r * pulse, p.primary.g * pulse,
                                                p.primary.b * pulse, p.primary.a),
                                          zone,
                                          voidTint,
                                          0.14f,
                                          0.14f,
                                          0.06f));
            renderer.drawLine(treeIso.x, treeIso.y + 3.5f * zf,
                              treeIso.x, treeIso.y - 2.0f * zf,
                              attenuateToZone(
                                  Color(0.30f, 0.22f, 0.14f, 0.42f),
                                  zone,
                                  voidTint,
                                  0.12f,
                                  0.20f,
                                  0.04f), 1.0f);
        }
        break;
    }
    }
}

void CityThemeRenderer::drawPresetVehicle(IsometricRenderer& renderer,
                                           const PlacedVehicle& placed) const {
    const CityAssets::VehiclePreset& v = *placed.preset;
    const ZoneVisibility zone = computeZoneVisibility(
        placed.worldX, placed.worldY,
        operationalCenterX, operationalCenterY,
        operationalRadiusX, operationalRadiusY);
    if (zone.transition <= 0.12f) {
        return;
    }
    const IsoCoord iso = RenderUtils::worldToIso(placed.worldX, placed.worldY);
    renderer.drawDiamond(iso.x, iso.y, v.length, v.width,
                         attenuateToZone(
                             v.body, zone, Color(0.01f, 0.02f, 0.04f, v.body.a),
                             0.24f, 0.16f, 0.08f));
    renderer.drawDiamondOutline(iso.x, iso.y, v.length, v.width,
                                attenuateToZone(
                                    v.outline, zone, Color(0.00f, 0.00f, 0.00f, v.outline.a),
                                    0.18f, 0.08f, 0.04f), 1.0f);

    // Windshield highlight for sedans and vans
    if (v.category == CityAssets::VehicleCategory::Sedan ||
        v.category == CityAssets::VehicleCategory::Van) {
        renderer.drawDiamond(iso.x, iso.y - v.width * 0.15f,
                             v.length * 0.35f, v.width * 0.45f,
                             attenuateToZone(
                                 Color(0.58f, 0.68f, 0.82f, 0.28f),
                                 zone,
                                 Color(0.02f, 0.03f, 0.05f, 0.28f),
                                 0.24f,
                                 0.08f,
                                 0.06f));
    }
}

void CityThemeRenderer::drawPresetRoadProp(IsometricRenderer& renderer,
                                            const PlacedRoadProp& placed,
                                            float animationTime) const {
    const CityAssets::RoadPropPreset& rp = *placed.preset;
    const ZoneVisibility zone = computeZoneVisibility(
        placed.worldX, placed.worldY,
        operationalCenterX, operationalCenterY,
        operationalRadiusX, operationalRadiusY);
    if (zone.transition <= 0.12f) {
        return;
    }
    const IsoCoord iso = RenderUtils::worldToIso(placed.worldX, placed.worldY);
    const float zf = currentZoomFactor();

    switch (rp.category) {
    case CityAssets::RoadPropCategory::TrafficLight: {
        // Pole
        renderer.drawLine(iso.x, iso.y + 6.0f, iso.x, iso.y - rp.height * 0.6f * zf,
                          attenuateToZone(
                              rp.poleColor, zone, Color(0.01f, 0.02f, 0.04f, rp.poleColor.a),
                              0.16f, 0.18f, 0.05f), 1.2f);
        // Housing
        renderer.drawDiamond(iso.x, iso.y - rp.height * 0.7f * zf,
                             2.8f, 1.6f,
                             attenuateToZone(
                                 Color(0.08f, 0.08f, 0.10f, 0.94f),
                                 zone,
                                 Color(0.00f, 0.00f, 0.00f, 0.94f),
                                 0.12f,
                                 0.12f,
                                 0.06f));
        // Signal lights
        const float phase = std::fmod(animationTime + placed.worldX * 0.7f, 6.0f);
        const bool green = phase < 2.8f;
        renderer.drawFilledCircle(iso.x - 0.9f, iso.y - rp.height * 0.7f * zf, 0.9f,
                                  attenuateToZone(
                                      green ? Color(0.26f, 0.26f, 0.28f, 0.62f) : rp.signalRed,
                                      zone,
                                      Color(0.02f, 0.02f, 0.04f, 0.62f),
                                      0.18f,
                                      0.08f,
                                      0.06f));
        renderer.drawFilledCircle(iso.x + 0.9f, iso.y - rp.height * 0.7f * zf, 0.9f,
                                  attenuateToZone(
                                      green ? rp.lightColor : Color(0.26f, 0.26f, 0.28f, 0.62f),
                                      zone,
                                      Color(0.02f, 0.02f, 0.04f, 0.62f),
                                      0.20f,
                                      0.08f,
                                      0.06f));
        break;
    }

    case CityAssets::RoadPropCategory::Streetlight: {
        renderer.drawLine(iso.x, iso.y + 4.0f, iso.x, iso.y - rp.height * 0.6f * zf,
                          attenuateToZone(
                              rp.poleColor, zone, Color(0.01f, 0.02f, 0.04f, rp.poleColor.a),
                              0.16f, 0.18f, 0.05f), 1.0f);
        // Lamp glow
        const float pulse = 0.7f + 0.3f * std::sin(animationTime * 1.2f + placed.worldX);
        renderer.drawFilledCircle(iso.x, iso.y - rp.height * 0.6f * zf, 2.0f * zf,
                                  attenuateToZone(
                                      withAlpha(rp.lightColor, rp.lightColor.a * pulse),
                                      zone,
                                      Color(0.02f, 0.03f, 0.05f, rp.lightColor.a),
                                      0.26f,
                                      0.08f,
                                      0.06f));
        break;
    }

    case CityAssets::RoadPropCategory::Divider: {
        renderer.drawDiamond(iso.x, iso.y, rp.width * zf, rp.height * 0.3f * zf,
                             attenuateToZone(
                                 rp.poleColor, zone, Color(0.01f, 0.02f, 0.04f, rp.poleColor.a),
                                 0.14f, 0.18f, 0.05f));
        break;
    }

    case CityAssets::RoadPropCategory::Crosswalk: {
        const int stripes = 5;
        for (int s = 0; s < stripes; ++s) {
            const float t = (static_cast<float>(s) + 0.5f) / static_cast<float>(stripes);
            const float offsetY = RenderUtils::lerp(-rp.width * 0.4f, rp.width * 0.4f, t);
            const IsoCoord sIso = RenderUtils::worldToIso(placed.worldX, placed.worldY + offsetY);
            renderer.drawDiamond(sIso.x, sIso.y, 4.0f * zf, 0.8f * zf,
                                 attenuateToZone(
                                     rp.poleColor, zone, Color(0.01f, 0.02f, 0.04f, rp.poleColor.a),
                                     0.16f, 0.18f, 0.05f));
        }
        break;
    }
    }
}

void CityThemeRenderer::drawTrafficLight(IsometricRenderer& renderer,
                                         const Intersection& intersection,
                                         float animationTime) const {
    const IsoCoord iso = RenderUtils::worldToIso(intersection.x, intersection.y);
    const ZoneVisibility zone = computeZoneVisibility(
        intersection.x, intersection.y,
        operationalCenterX, operationalCenterY,
        operationalRadiusX, operationalRadiusY);
    const bool green = isTrafficLightGreen(intersection, animationTime);
    renderer.drawLine(iso.x - 4.0f, iso.y + 6.0f, iso.x - 4.0f, iso.y - 8.0f,
                      attenuateToZone(
                          Color(0.18f, 0.18f, 0.20f, 0.82f),
                          zone,
                          Color(0.00f, 0.00f, 0.00f, 0.82f),
                          0.16f,
                          0.12f,
                          0.06f), 1.2f);
    renderer.drawDiamond(iso.x - 4.0f, iso.y - 10.0f, 2.6f, 1.4f,
                         attenuateToZone(
                             Color(0.08f, 0.08f, 0.10f, 0.94f),
                             zone,
                             Color(0.00f, 0.00f, 0.00f, 0.94f),
                             0.10f,
                             0.10f,
                             0.06f));
    renderer.drawFilledCircle(iso.x - 4.8f, iso.y - 10.0f, 0.9f,
                              attenuateToZone(
                                  green ? Color(0.26f, 0.26f, 0.28f, 0.62f)
                                        : Color(0.92f, 0.20f, 0.16f, 0.88f),
                                  zone,
                                  Color(0.02f, 0.02f, 0.04f, 0.62f),
                                  0.18f,
                                  0.08f,
                                  0.06f));
    renderer.drawFilledCircle(iso.x - 3.2f, iso.y - 10.0f, 0.9f,
                              attenuateToZone(
                                  green ? Color(0.28f, 0.96f, 0.44f, 0.92f)
                                        : Color(0.34f, 0.34f, 0.36f, 0.62f),
                                  zone,
                                  Color(0.02f, 0.02f, 0.04f, 0.62f),
                                  0.20f,
                                  0.08f,
                                  0.06f));
}

void CityThemeRenderer::cleanup() {
    presetBuildings.clear();
    presetEnvironments.clear();
    presetVehicles.clear();
    presetRoadProps.clear();
}
