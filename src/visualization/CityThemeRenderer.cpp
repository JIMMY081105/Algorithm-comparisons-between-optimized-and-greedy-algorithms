#include "visualization/CityThemeRenderer.h"

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
constexpr int kLotGrid = 5;
constexpr int kLotCells = kLotGrid * kLotGrid;
constexpr float kTwoPi = 6.28318530718f;

float clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
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
    generateBuildings(graph, layoutRng);
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
        std::string("City mode follows the street graph through the illuminated operational district under ") +
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

    Color skyTop(0.22f, 0.30f, 0.44f, 1.0f);
    Color skyBottom(0.10f, 0.13f, 0.19f, 1.0f);
    Color groundBase(0.12f, 0.14f, 0.17f, 0.98f);
    Color districtLift(0.34f, 0.42f, 0.54f, 0.14f);
    Color wetTint(0.14f, 0.20f, 0.28f, 0.12f);

    switch (weather) {
        case CityWeather::Sunny:
            skyTop = Color(0.24f, 0.32f, 0.46f, 1.0f);
            skyBottom = Color(0.12f, 0.15f, 0.22f, 1.0f);
            groundBase = Color(0.14f, 0.15f, 0.18f, 0.98f);
            districtLift = Color(0.96f, 0.74f, 0.38f, 0.16f);
            wetTint = Color(0.14f, 0.19f, 0.24f, 0.08f);
            break;
        case CityWeather::Cloudy:
            skyTop = Color(0.18f, 0.23f, 0.32f, 1.0f);
            skyBottom = Color(0.10f, 0.13f, 0.19f, 1.0f);
            groundBase = Color(0.12f, 0.14f, 0.17f, 0.98f);
            districtLift = Color(0.52f, 0.60f, 0.72f, 0.14f);
            wetTint = Color(0.14f, 0.18f, 0.24f, 0.10f);
            break;
        case CityWeather::Rainy:
            skyTop = Color(0.13f, 0.18f, 0.26f, 1.0f);
            skyBottom = Color(0.07f, 0.10f, 0.15f, 1.0f);
            groundBase = Color(0.10f, 0.12f, 0.15f, 0.99f);
            districtLift = Color(0.42f, 0.50f, 0.62f, 0.12f);
            wetTint = Color(0.16f, 0.24f, 0.34f, 0.14f);
            break;
        case CityWeather::Stormy:
            skyTop = Color(0.10f, 0.13f, 0.19f, 1.0f);
            skyBottom = Color(0.05f, 0.07f, 0.11f, 1.0f);
            groundBase = Color(0.08f, 0.10f, 0.13f, 1.0f);
            districtLift = Color(0.36f, 0.42f, 0.52f, 0.11f);
            wetTint = Color(0.18f, 0.26f, 0.36f, 0.16f);
            break;
    }

    glBegin(GL_QUADS);
    glColor4f(skyTop.r, skyTop.g, skyTop.b, skyTop.a);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glColor4f(skyBottom.r, skyBottom.g, skyBottom.b, skyBottom.a);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();

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

    for (const BlockZone& block : blocks) {
        const float spanX = block.maxX - block.minX;
        const float spanY = block.maxY - block.minY;
        const float insetX = spanX * 0.07f;
        const float insetY = spanY * 0.08f;

        Color basePave(0.16f, 0.17f, 0.20f, 0.94f);
        Color focusPave(0.30f, 0.32f, 0.38f, 0.96f);
        Color patch = mixColor(basePave, focusPave, block.focusWeight * 0.88f);
        patch = mixColor(patch, wetTint,
                         (weather == CityWeather::Rainy || weather == CityWeather::Stormy)
                             ? 0.32f : 0.0f);

        if (block.openPocket) {
            patch = block.serviceCourt
                ? Color(0.12f, 0.12f, 0.13f, 0.88f)
                : Color(0.14f, 0.13f, 0.12f, 0.82f);
        }

        drawWorldQuadPatch(block.minX + insetX, block.minY + insetY,
                           block.maxX - insetX, block.maxY - insetY,
                           patch);

        if (block.visualTier == VisualTier::Focus) {
            drawWorldQuadPatch(block.centerX - spanX * 0.16f,
                               block.centerY - spanY * 0.16f,
                               block.centerX + spanX * 0.16f,
                               block.centerY + spanY * 0.16f,
                               Color(0.24f, 0.29f, 0.36f, 0.16f));
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

        const float curbWidth = 8.8f + roadFocus * 3.4f;
        const float asphaltWidth = 6.6f + roadFocus * 2.8f;
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
                             withAlpha(Color(0.88f, 0.84f, 0.62f, 1.0f),
                                       0.05f + roadFocus * 0.08f));
    }

    for (const auto& building : buildings) {
        drawBuildingLot(renderer, building);
    }

    if (layerToggles.showIncidents) {
        for (std::size_t i = 0; i < roads.size(); ++i) {
            const RoadSegment& road = roads[i];
            if (!road.incident) {
                continue;
            }

            const Intersection& from = intersections[road.from];
            const Intersection& to = intersections[road.to];
            const IsoCoord isoFrom = RenderUtils::worldToIso(from.x, from.y);
            const IsoCoord isoTo = RenderUtils::worldToIso(to.x, to.y);
            const float midX = (isoFrom.x + isoTo.x) * 0.5f;
            const float midY = (isoFrom.y + isoTo.y) * 0.5f;
            renderer.drawLine(midX - 4.0f, midY - 3.0f, midX + 4.0f, midY + 3.0f,
                              Color(1.0f, 0.24f, 0.18f, 0.78f), 1.6f);
            renderer.drawLine(midX - 4.0f, midY + 3.0f, midX + 4.0f, midY - 3.0f,
                              Color(1.0f, 0.24f, 0.18f, 0.78f), 1.6f);
        }
    }

    if (layerToggles.showTrafficLights) {
        for (const auto& intersection : intersections) {
            if (intersection.hasTrafficLight) {
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
                const IsoCoord nodeIso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
                const IsoCoord anchorIso = RenderUtils::worldToIso(anchor.x, anchor.y);
                renderer.drawLine(nodeIso.x, nodeIso.y, anchorIso.x, anchorIso.y,
                                  Color(0.34f, 0.70f, 0.90f, 0.18f), 1.0f);
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
            const IsoCoord iso = RenderUtils::worldToIso(worldX, worldY);
            const Color carColor = car.crashed
                ? Color(0.88f, 0.22f, 0.18f, 0.85f)
                : mixColor(Color(0.80f, 0.84f, 0.88f, 0.78f),
                           Color(0.96f, 0.96f, 0.98f, 0.86f),
                           road.focusWeight * 0.60f);
            renderer.drawDiamond(iso.x, iso.y, 3.0f, 1.4f, carColor);
            renderer.drawDiamondOutline(iso.x, iso.y, 3.0f, 1.4f,
                                        Color(0.08f, 0.09f, 0.10f, 0.55f), 1.0f);
        }
    }
}

void CityThemeRenderer::drawWasteNode(IsometricRenderer& renderer,
                                      const WasteNode& node,
                                      float animationTime) {
    const IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
    const float focus = computeOperationalFocus(node.getWorldX(), node.getWorldY());
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

    renderer.drawDiamond(iso.x, iso.y + 9.0f, 11.0f, 5.5f, padColor);
    renderer.drawIsometricBlock(iso.x, iso.y + 4.0f, 12.0f, 8.0f, 10.0f,
                                blockColor, Color(0.12f, 0.14f, 0.17f, 0.92f));
    renderer.drawDiamond(iso.x, iso.y - 7.0f, 5.0f, 2.4f,
                         Color(urgency.r, urgency.g, urgency.b,
                               node.isCollected() ? 0.18f : 0.22f + pulse * 0.16f));
    renderer.drawRing(iso.x, iso.y - 12.0f, 3.4f,
                      Color(urgency.r, urgency.g, urgency.b,
                            node.isCollected() ? 0.18f : 0.22f + pulse * 0.12f),
                      1.2f);
}

void CityThemeRenderer::drawHQNode(IsometricRenderer& renderer,
                                   const WasteNode& node,
                                   float animationTime) {
    const IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 2.2f);
    renderer.drawDiamond(iso.x, iso.y + 14.0f, 17.0f, 8.5f,
                         Color(0.16f, 0.18f, 0.22f, 0.94f));
    renderer.drawIsometricBlock(iso.x, iso.y + 4.0f, 18.0f, 12.0f, 16.0f,
                                Color(0.28f, 0.38f, 0.52f, 0.98f),
                                Color(0.14f, 0.18f, 0.24f, 0.98f));
    renderer.drawDiamond(iso.x, iso.y - 16.0f, 8.0f, 3.6f,
                         Color(0.74f, 0.86f, 0.96f, 0.20f + pulse * 0.12f));
    renderer.drawRing(iso.x, iso.y - 16.0f, 5.2f,
                      Color(0.88f, 0.94f, 0.98f, 0.18f + pulse * 0.08f), 1.4f);
}

void CityThemeRenderer::drawTruck(IsometricRenderer& renderer,
                                  const MapGraph&,
                                  const Truck& truck,
                                  const MissionPresentation* mission,
                                  float animationTime) {
    const MissionPresentation* liveMission = ensureMission(mission);
    const IsoCoord iso = RenderUtils::worldToIso(truck.getPosX(), truck.getPosY());

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
                      Color(0.22f, 0.90f, 1.0f, 0.20f), 1.5f);
    renderer.drawDiamond(iso.x, iso.y + bob, 5.0f, 2.4f,
                         Color(0.96f, 0.74f, 0.18f, 0.96f));
    renderer.drawDiamondOutline(iso.x, iso.y + bob, 5.0f, 2.4f,
                                Color(0.10f, 0.12f, 0.14f, 0.76f), 1.2f);
    renderer.drawLine(iso.x, iso.y + bob,
                      iso.x + dirX * 10.0f, iso.y - dirY * 5.0f + bob,
                      Color(0.98f, 0.92f, 0.64f, 0.52f), 1.4f);
}

void CityThemeRenderer::drawDecorativeElements(IsometricRenderer& renderer,
                                               const MapGraph&,
                                               float animationTime) {
    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 1.4f);

    for (const AmbientStreet& street : ambientStreets) {
        const IsoCoord isoFrom = RenderUtils::worldToIso(street.startX, street.startY);
        const IsoCoord isoTo = RenderUtils::worldToIso(street.endX, street.endY);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          Color(0.04f, 0.05f, 0.06f, 0.40f), street.width + 1.6f);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          Color(0.10f, 0.11f, 0.13f, 0.30f), street.width);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          Color(0.58f, 0.64f, 0.76f, street.glow * (0.45f + pulse * 0.25f)),
                          1.2f);
    }

    for (const BlockZone& block : blocks) {
        if (!block.openPocket) {
            continue;
        }

        const float spanX = block.maxX - block.minX;
        const float spanY = block.maxY - block.minY;
        const float insetX = spanX * 0.18f;
        const float insetY = spanY * 0.18f;
        const float minX = block.minX + insetX;
        const float maxX = block.maxX - insetX;
        const float minY = block.minY + insetY;
        const float maxY = block.maxY - insetY;

        const Color courtColor = block.serviceCourt
            ? Color(0.10f, 0.11f, 0.12f, 0.58f)
            : Color(0.14f, 0.13f, 0.12f, 0.46f);
        drawWorldQuadPatch(minX, minY, maxX, maxY, courtColor);

        if (!block.serviceCourt) {
            const IsoCoord centerIso = RenderUtils::worldToIso(block.centerX, block.centerY);
            renderer.drawDiamond(centerIso.x, centerIso.y, 6.2f, 2.8f,
                                 Color(0.74f, 0.62f, 0.38f, 0.08f + pulse * 0.03f));
        }
    }

    for (const auto& building : backgroundBuildings) {
        drawBuildingLot(renderer, building);
    }
}

void CityThemeRenderer::drawAtmosphericEffects(IsometricRenderer& renderer,
                                               const MapGraph&,
                                               float animationTime) {
    const float spanX = std::max(1.0f, peripheralMaxX - peripheralMinX);
    const float spanY = std::max(1.0f, peripheralMaxY - peripheralMinY);

    if (weather == CityWeather::Rainy || weather == CityWeather::Stormy) {
        const int streakCount = (weather == CityWeather::Stormy) ? 34 : 24;
        for (int i = 0; i < streakCount; ++i) {
            const float phase = animationTime * (weather == CityWeather::Stormy ? 7.2f : 5.2f) +
                                static_cast<float>(i) * 1.31f;
            const float worldX = peripheralMinX + std::fmod(phase * 1.4f + i * 3.7f, spanX);
            const float worldY = peripheralMinY + std::fmod(phase * 0.9f + i * 2.4f, spanY);
            const IsoCoord iso = RenderUtils::worldToIso(worldX, worldY);
            renderer.drawLine(iso.x, iso.y - 10.0f, iso.x - 3.0f, iso.y + 6.0f,
                              Color(0.82f, 0.88f, 0.98f,
                                    weather == CityWeather::Stormy ? 0.24f : 0.18f),
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
    const float edgeAlpha = 0.10f + weatherOverlayStrength();

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

    const IsoCoord opIso = RenderUtils::worldToIso(operationalCenterX, operationalCenterY);
    const float halfW = RenderUtils::getProjection().tileWidth * 0.5f;
    const float halfH = RenderUtils::getProjection().tileHeight * 0.5f;
    const float vignetteRadiusX =
        (operationalRadiusX + operationalRadiusY) * halfW * 1.60f;
    const float vignetteRadiusY =
        (operationalRadiusX + operationalRadiusY) * halfH * 1.12f;
    drawGradientEllipse(opIso.x, opIso.y + vignetteRadiusY * 0.10f,
                        vignetteRadiusX, vignetteRadiusY,
                        Color(0.02f, 0.03f, 0.05f, 0.0f),
                        Color(0.02f, 0.03f, 0.05f, 0.14f + weatherOverlayStrength()));
}

void CityThemeRenderer::generateGridNetwork(const MapGraph& graph, std::mt19937& rng) {
    intersections.clear();
    roads.clear();
    roadAdjacency.clear();

    gridColumns = 6;
    gridRows = 5;

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
    const float stepX = (endX - startX) / static_cast<float>(gridColumns - 1);
    const float stepY = (endY - startY) / static_cast<float>(gridRows - 1);

    std::uniform_real_distribution<float> cycleDistribution(0.0f, 4.0f);
    for (int row = 0; row < gridRows; ++row) {
        for (int col = 0; col < gridColumns; ++col) {
            const bool hasLight = row > 0 && row < gridRows - 1 &&
                                  col > 0 && col < gridColumns - 1 &&
                                  ((row + col + static_cast<int>(layoutSeed % 3)) % 2 == 0);
            intersections.push_back(Intersection{
                row * gridColumns + col,
                startX + col * stepX,
                startY + row * stepY,
                hasLight,
                cycleDistribution(rng)
            });
        }
    }

    roadAdjacency.assign(intersections.size(), {});

    auto addRoad = [&](int from, int to) {
        const Intersection& a = intersections[from];
        const Intersection& b = intersections[to];

        const int roadIndex = static_cast<int>(roads.size());
        roads.push_back(RoadSegment{
            from,
            to,
            pointDistance(a.x, a.y, b.x, b.y),
            0.0f,
            false,
            0.0f,
            0.0f,
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

    for (auto& road : roads) {
        const Intersection& from = intersections[road.from];
        const Intersection& to = intersections[road.to];
        const float midX = (from.x + to.x) * 0.5f;
        const float midY = (from.y + to.y) * 0.5f;
        road.focusWeight = computeOperationalFocus(midX, midY);
        road.visualTier = visualTierFromFocus(road.focusWeight);
    }
}

void CityThemeRenderer::generateBlocks(std::mt19937& rng) {
    blocks.clear();
    blocks.reserve((gridRows - 1) * (gridColumns - 1));

    std::uniform_real_distribution<float> focusJitter(-0.06f, 0.06f);

    for (int row = 0; row + 1 < gridRows; ++row) {
        for (int col = 0; col + 1 < gridColumns; ++col) {
            const Intersection& a = intersections[row * gridColumns + col];
            const Intersection& b = intersections[row * gridColumns + col + 1];
            const Intersection& c = intersections[(row + 1) * gridColumns + col];
            const float centerX = (a.x + b.x) * 0.5f;
            const float centerY = (a.y + c.y) * 0.5f;
            const float focus = clamp01(computeOperationalFocus(centerX, centerY) + focusJitter(rng));
            const DistrictType district = districtFromFocus(focus);

            float occupancyTarget = 0.90f;
            switch (district) {
                case DistrictType::Core: occupancyTarget = 0.98f; break;
                case DistrictType::Mixed: occupancyTarget = 0.92f; break;
                case DistrictType::Residential: occupancyTarget = 0.86f; break;
                case DistrictType::Utility: occupancyTarget = 0.78f; break;
            }

            blocks.push_back(BlockZone{
                row,
                col,
                RenderUtils::lerp(a.x, b.x, 0.10f),
                RenderUtils::lerp(a.x, b.x, 0.90f),
                RenderUtils::lerp(a.y, c.y, 0.12f),
                RenderUtils::lerp(a.y, c.y, 0.88f),
                centerX,
                centerY,
                focus,
                occupancyTarget,
                district,
                visualTierFromFocus(focus),
                false,
                false,
                false
            });
        }
    }

    std::vector<int> coreBlocks;
    std::vector<int> nonCoreBlocks;
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        if (blocks[i].district == DistrictType::Core) {
            coreBlocks.push_back(i);
        } else {
            nonCoreBlocks.push_back(i);
        }
    }

    std::shuffle(coreBlocks.begin(), coreBlocks.end(), rng);
    std::shuffle(nonCoreBlocks.begin(), nonCoreBlocks.end(), rng);

    const int landmarkCount = std::min<int>(static_cast<int>(coreBlocks.size()),
                                            1 + static_cast<int>((layoutSeed >> 1) & 1u));
    for (int i = 0; i < landmarkCount; ++i) {
        blocks[coreBlocks[i]].landmarkCluster = true;
        blocks[coreBlocks[i]].occupancyTarget = 0.92f;
        blocks[coreBlocks[i]].focusWeight = clamp01(blocks[coreBlocks[i]].focusWeight + 0.08f);
        blocks[coreBlocks[i]].visualTier = VisualTier::Focus;
    }

    const int openPocketCount = std::min<int>(static_cast<int>(nonCoreBlocks.size()),
                                              2 + static_cast<int>(layoutSeed & 1u));
    for (int i = 0; i < openPocketCount; ++i) {
        BlockZone& block = blocks[nonCoreBlocks[i]];
        block.openPocket = true;
        block.serviceCourt = (i % 2 == 0);
        block.occupancyTarget = block.serviceCourt ? 0.70f : 0.64f;
        block.focusWeight = clamp01(block.focusWeight - 0.05f);
    }
}

void CityThemeRenderer::generateBuildings(const MapGraph& graph, std::mt19937& rng) {
    buildings.clear();
    buildings.reserve(blocks.size() * 6);

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> signedJitter(-0.12f, 0.12f);
    std::uniform_real_distribution<float> hueDistribution(-0.04f, 0.04f);

    auto chooseFamily = [&](const BlockZone& block) {
        const float roll = unit(rng);
        if (block.landmarkCluster && roll < 0.24f) {
            return BuildingFamily::Landmark;
        }
        if (block.openPocket && block.serviceCourt) {
            return (roll < 0.72f) ? BuildingFamily::Utility : BuildingFamily::Commercial;
        }

        switch (block.district) {
            case DistrictType::Core:
                if (roll < 0.50f) return BuildingFamily::Commercial;
                if (roll < 0.78f) return BuildingFamily::Residential;
                return BuildingFamily::Utility;
            case DistrictType::Mixed:
                if (roll < 0.42f) return BuildingFamily::Commercial;
                if (roll < 0.76f) return BuildingFamily::Residential;
                return BuildingFamily::Utility;
            case DistrictType::Residential:
                if (roll < 0.64f) return BuildingFamily::Residential;
                if (roll < 0.84f) return BuildingFamily::Commercial;
                return BuildingFamily::Utility;
            case DistrictType::Utility:
            default:
                if (roll < 0.70f) return BuildingFamily::Utility;
                if (roll < 0.88f) return BuildingFamily::Commercial;
                return BuildingFamily::Residential;
        }
    };

    auto chooseRoof = [&](BuildingFamily family, const BlockZone& block) {
        const float roll = unit(rng);
        if (family == BuildingFamily::Landmark) {
            return (roll < 0.60f) ? RoofProfile::Crown : RoofProfile::Stepped;
        }
        if (family == BuildingFamily::Residential) {
            return (roll < 0.46f) ? RoofProfile::Terrace : RoofProfile::Crown;
        }
        if (family == BuildingFamily::Utility || block.serviceCourt) {
            return RoofProfile::Flat;
        }
        return (roll < 0.50f) ? RoofProfile::Stepped : RoofProfile::Terrace;
    };

    for (const BlockZone& block : blocks) {
        bool occupied[kLotGrid][kLotGrid] = {};
        const float buildMinX = block.minX + (block.maxX - block.minX) * 0.02f;
        const float buildMaxX = block.maxX - (block.maxX - block.minX) * 0.02f;
        const float buildMinY = block.minY + (block.maxY - block.minY) * 0.02f;
        const float buildMaxY = block.maxY - (block.maxY - block.minY) * 0.02f;
        const float cellSpanX = (buildMaxX - buildMinX) / static_cast<float>(kLotGrid);
        const float cellSpanY = (buildMaxY - buildMinY) / static_cast<float>(kLotGrid);

        int targetCells = static_cast<int>(std::round(block.occupancyTarget * static_cast<float>(kLotCells)));
        targetCells = std::clamp(targetCells, 2, kLotCells);

        std::vector<int> cellOrder(kLotCells);
        std::iota(cellOrder.begin(), cellOrder.end(), 0);
        std::shuffle(cellOrder.begin(), cellOrder.end(), rng);

        int usedCells = 0;
        for (int cellIndex : cellOrder) {
            if (usedCells >= targetCells) {
                break;
            }

            const int cellRow = cellIndex / kLotGrid;
            const int cellCol = cellIndex % kLotGrid;
            if (occupied[cellRow][cellCol]) {
                continue;
            }

            BuildingFamily family = chooseFamily(block);
            int spanCols = 1;
            int spanRows = 1;

            const bool canWide = cellCol + 1 < kLotGrid && !occupied[cellRow][cellCol + 1];
            const bool canTall = cellRow + 1 < kLotGrid && !occupied[cellRow + 1][cellCol];
            const bool canSquare = canWide && canTall && !occupied[cellRow + 1][cellCol + 1];

            const float mergeRoll = unit(rng);
            if (family == BuildingFamily::Landmark && canSquare && mergeRoll < 0.72f) {
                spanCols = 2;
                spanRows = 2;
            } else if (family == BuildingFamily::Utility && canWide && mergeRoll < 0.54f) {
                spanCols = 2;
            } else if (family == BuildingFamily::Commercial && canWide && mergeRoll < 0.42f) {
                spanCols = 2;
            } else if (family == BuildingFamily::Commercial && canTall && mergeRoll < 0.62f) {
                spanRows = 2;
            } else if (family == BuildingFamily::Residential && canTall && mergeRoll < 0.40f) {
                spanRows = 2;
            } else if (block.serviceCourt && canWide && mergeRoll < 0.35f) {
                spanCols = 2;
            }

            const int spanCells = spanCols * spanRows;
            if (usedCells + spanCells > targetCells + 1) {
                spanCols = 1;
                spanRows = 1;
            }

            const float lotMinX = buildMinX + static_cast<float>(cellCol) * cellSpanX;
            const float lotMaxX = buildMinX + static_cast<float>(cellCol + spanCols) * cellSpanX;
            const float lotMinY = buildMinY + static_cast<float>(cellRow) * cellSpanY;
            const float lotMaxY = buildMinY + static_cast<float>(cellRow + spanRows) * cellSpanY;
            const float lotSpanX = lotMaxX - lotMinX;
            const float lotSpanY = lotMaxY - lotMinY;
            const float centerX = (lotMinX + lotMaxX) * 0.5f + lotSpanX * signedJitter(rng) * 0.08f;
            const float centerY = (lotMinY + lotMaxY) * 0.5f + lotSpanY * signedJitter(rng) * 0.08f;

            if (isReservedVisualArea(centerX, centerY,
                                     0.55f + static_cast<float>(std::max(spanCols, spanRows)) * 0.10f,
                                     graph)) {
                continue;
            }

            for (int r = 0; r < spanRows; ++r) {
                for (int c = 0; c < spanCols; ++c) {
                    occupied[cellRow + r][cellCol + c] = true;
                }
            }
            usedCells += spanCells;

            const float shapeRoll = unit(rng);
            float widthFill = 0.94f;
            float depthFill = 0.92f;
            switch (family) {
                case BuildingFamily::Utility:
                    widthFill = 0.95f + unit(rng) * 0.04f;
                    depthFill = 0.90f + unit(rng) * 0.08f;
                    break;
                case BuildingFamily::Commercial:
                    widthFill = 0.92f + unit(rng) * 0.06f;
                    depthFill = 0.90f + unit(rng) * 0.08f;
                    break;
                case BuildingFamily::Residential:
                    widthFill = 0.88f + unit(rng) * 0.10f;
                    depthFill = 0.86f + unit(rng) * 0.10f;
                    break;
                case BuildingFamily::Landmark:
                    widthFill = 0.94f + unit(rng) * 0.04f;
                    depthFill = 0.92f + unit(rng) * 0.06f;
                    break;
            }
            if (shapeRoll < 0.18f && family != BuildingFamily::Landmark) {
                widthFill *= 0.72f;
            } else if (shapeRoll > 0.86f && family != BuildingFamily::Landmark) {
                depthFill *= 0.72f;
            }

            const float focusCurve = block.focusWeight * block.focusWeight;
            const float centerBoost = 0.28f + focusCurve * 2.4f;
            float height = 6.0f + focusCurve * 14.0f + unit(rng) * 4.0f;
            switch (family) {
                case BuildingFamily::Utility:
                    height = 3.8f + unit(rng) * 3.6f + spanCols * 0.8f;
                    break;
                case BuildingFamily::Commercial:
                    height = 5.5f + focusCurve * 26.0f + unit(rng) * 5.0f +
                             static_cast<float>(spanCols + spanRows - 1) * 1.6f;
                    break;
                case BuildingFamily::Residential:
                    height = 5.0f + focusCurve * 14.0f + unit(rng) * 6.0f +
                             static_cast<float>(spanRows) * 1.8f;
                    break;
                case BuildingFamily::Landmark:
                    height = 28.0f + focusCurve * 32.0f + unit(rng) * 10.0f;
                    break;
            }

            if (family != BuildingFamily::Landmark && family != BuildingFamily::Utility) {
                height *= 0.55f + centerBoost * 0.45f;
            }
            if (block.visualTier == VisualTier::Peripheral) {
                height *= 0.62f;
            } else if (block.visualTier == VisualTier::Support) {
                height *= 0.82f;
            }
            if (block.openPocket && family != BuildingFamily::Landmark) {
                height *= block.serviceCourt ? 0.78f : 0.88f;
            }

            const float baseHue = hueDistribution(rng);
            const float saturation = (family == BuildingFamily::Commercial)
                ? 0.18f + block.focusWeight * 0.10f
                : (family == BuildingFamily::Residential)
                    ? 0.14f + block.focusWeight * 0.08f
                    : 0.08f + block.focusWeight * 0.05f;
            const float facadeBrightness = 0.55f + block.focusWeight * 0.30f + unit(rng) * 0.12f;
            const float edgeHighlight = 0.38f + block.focusWeight * 0.40f + unit(rng) * 0.14f;
            const float windowWarmth = (family == BuildingFamily::Residential)
                ? 0.52f + unit(rng) * 0.34f
                : (family == BuildingFamily::Commercial)
                    ? 0.24f + unit(rng) * 0.24f
                    : 0.12f + unit(rng) * 0.18f;
            const float windowDensity = (family == BuildingFamily::Utility)
                ? 0.18f + unit(rng) * 0.20f
                : (family == BuildingFamily::Landmark)
                    ? 0.48f + unit(rng) * 0.22f
                    : 0.34f + unit(rng) * 0.30f;
            const float occupancy = (block.visualTier == VisualTier::Focus)
                ? 0.55f + unit(rng) * 0.35f
                : 0.26f + unit(rng) * 0.42f;
            const float podiumRatio = (family == BuildingFamily::Utility)
                ? 0.22f + unit(rng) * 0.10f
                : (family == BuildingFamily::Residential)
                    ? 0.16f + unit(rng) * 0.08f
                    : 0.20f + unit(rng) * 0.16f;
            const float taper = (family == BuildingFamily::Residential)
                ? 0.58f + unit(rng) * 0.14f
                : (family == BuildingFamily::Landmark)
                    ? 0.50f + unit(rng) * 0.12f
                    : 0.74f + unit(rng) * 0.12f;

            buildings.push_back(BuildingLot{
                centerX,
                centerY,
                lotSpanX * widthFill,
                lotSpanY * depthFill,
                height,
                baseHue,
                saturation,
                facadeBrightness,
                edgeHighlight,
                windowWarmth,
                windowDensity,
                occupancy,
                podiumRatio,
                taper,
                block.district,
                family,
                chooseRoof(family, block),
                block.visualTier,
                family == BuildingFamily::Landmark || block.landmarkCluster
            });
        }
    }

    std::sort(buildings.begin(), buildings.end(),
              [](const BuildingLot& lhs, const BuildingLot& rhs) {
                  if (std::abs(lhs.y - rhs.y) > 0.02f) {
                      return lhs.y < rhs.y;
                  }
                  return lhs.x < rhs.x;
              });
}

void CityThemeRenderer::generatePeripheralScene(std::mt19937& rng) {
    backgroundBuildings.clear();
    ambientStreets.clear();

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> hueDistribution(-0.03f, 0.02f);

    auto addBackgroundLot = [&](float x, float y, float width, float depth, float height,
                                BuildingFamily family, float brightnessBias) {
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
            DistrictType::Utility,
            family,
            RoofProfile::Flat,
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
                         0.01f);
    }

    std::sort(backgroundBuildings.begin(), backgroundBuildings.end(),
              [](const BuildingLot& lhs, const BuildingLot& rhs) {
                  if (std::abs(lhs.y - rhs.y) > 0.02f) {
                      return lhs.y < rhs.y;
                  }
                  return lhs.x < rhs.x;
              });

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
    if (focusWeight >= 0.72f) return DistrictType::Core;
    if (focusWeight >= 0.48f) return DistrictType::Mixed;
    if (focusWeight >= 0.28f) return DistrictType::Residential;
    return DistrictType::Utility;
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
    dashboardInfo.subtitle = "Operational urban district";
    dashboardInfo.atmosphereLabel =
        (weather == CityWeather::Sunny) ? "Clear-night arterial flow"
        : (weather == CityWeather::Cloudy) ? "Muted district circulation"
        : (weather == CityWeather::Rainy) ? "Wet-surface logistics slowdown"
        : "Storm-driven logistics pressure";
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
        const float focus = computeOperationalFocus((startPoint.x + endX) * 0.5f,
                                                    (startPoint.y + endY) * 0.5f);
        const IsoCoord isoFrom = RenderUtils::worldToIso(startPoint.x, startPoint.y);
        const IsoCoord isoTo = RenderUtils::worldToIso(endX, endY);
        if (focus > 0.26f) {
            renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                              Color(0.90f, 0.78f, 0.44f, 0.06f + focus * 0.08f),
                              10.0f + focus * 3.0f);
        }
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          Color(0.06f, 0.08f, 0.10f, 0.40f + pulse * 0.08f), 9.2f);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          Color(0.24f, 0.84f, 0.96f, 0.32f + pulse * 0.10f), 3.8f);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          Color(0.94f, 0.98f, 1.0f, 0.18f + pulse * 0.08f), 1.2f);
    }

    for (const auto& stop : path.stops) {
        const float focus = computeOperationalFocus(path.points[stop.pointIndex].x,
                                                    path.points[stop.pointIndex].y);
        const IsoCoord iso = RenderUtils::worldToIso(path.points[stop.pointIndex].x,
                                                     path.points[stop.pointIndex].y);
        if (focus > 0.28f) {
            renderer.drawDiamond(iso.x, iso.y, 6.0f, 2.6f,
                                 Color(0.92f, 0.80f, 0.46f, 0.08f + focus * 0.08f));
        }
        renderer.drawDiamond(iso.x, iso.y, 4.5f, 2.0f, Color(0.70f, 0.94f, 1.0f, 0.18f));
        renderer.drawDiamondOutline(iso.x, iso.y, 3.0f, 1.4f,
                                    Color(0.94f, 0.98f, 1.0f, 0.24f), 1.0f);
    }
}

void CityThemeRenderer::drawBuildingLot(IsometricRenderer& renderer,
                                        const BuildingLot& building) const {
    const IsoCoord iso = RenderUtils::worldToIso(building.x, building.y);
    const float zf = currentZoomFactor();

    Color material(0.42f, 0.46f, 0.52f, 0.98f);
    Color accent(0.58f, 0.64f, 0.72f, 0.98f);
    switch (building.family) {
        case BuildingFamily::Utility:
            material = Color(0.46f, 0.42f, 0.34f, 0.98f);
            accent = Color(0.64f, 0.56f, 0.42f, 0.98f);
            break;
        case BuildingFamily::Commercial:
            material = Color(0.34f, 0.46f, 0.60f, 0.98f);
            accent = Color(0.52f, 0.68f, 0.84f, 0.98f);
            break;
        case BuildingFamily::Residential:
            material = Color(0.56f, 0.38f, 0.32f, 0.98f);
            accent = Color(0.76f, 0.54f, 0.42f, 0.98f);
            break;
        case BuildingFamily::Landmark:
            material = Color(0.58f, 0.48f, 0.28f, 0.98f);
            accent = Color(0.86f, 0.72f, 0.40f, 0.98f);
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
    const Color leftSide = scaleColor(facade,
                                      0.72f + brightness * 0.30f +
                                      building.edgeHighlight * 0.08f);
    const Color rightSide = scaleColor(facade,
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

    const float baseWidth = std::max(4.0f * zf, building.width * 38.0f * zf);
    const float baseDepth = std::max(3.0f * zf, building.depth * 28.0f * zf);
    const float totalHeight = std::max(7.0f * zf, building.height * 1.38f * zf);
    const float shadowScale = (building.visualTier == VisualTier::Focus) ? 1.12f : 1.04f;
    renderer.drawDiamond(iso.x, iso.y + 8.5f * zf, baseWidth * shadowScale,
                         baseDepth * 0.54f, Color(0.02f, 0.02f, 0.03f, 0.24f));

    auto drawMass = [&](float offsetX, float offsetY,
                        float width, float depth, float height,
                        float windowScale) -> BlockFaces {
        const BlockFaces faces = makeBlockFaces(iso.x + offsetX, iso.y + offsetY,
                                                width, depth, height);
        drawBlockMass(faces, roof, leftSide, rightSide);

        const int bands = std::max(1, static_cast<int>(std::round(
            height / (5.4f * zf) * building.windowDensity * windowScale)));
        drawFaceBands(faces.left, bands,
                      0.10f, withAlpha(windowColor, windowColor.a * building.occupancy * 0.82f),
                      0.8f * zf + 0.35f);
        drawFaceBands(faces.right, bands + (building.family == BuildingFamily::Commercial ? 1 : 0),
                      0.12f, withAlpha(windowColor, windowColor.a * building.occupancy),
                      0.8f * zf + 0.35f);
        drawOutlineLoop(faces.top, outline, std::max(0.8f, 0.85f * zf));
        renderer.drawLine(faces.top[1].x, faces.top[1].y, faces.right[2].x, faces.right[2].y,
                          outline, std::max(0.8f, 0.9f * zf));
        renderer.drawLine(faces.top[3].x, faces.top[3].y, faces.left[2].x, faces.left[2].y,
                          outline, std::max(0.8f, 0.9f * zf));
        return faces;
    };

    const float podiumHeight = std::max(2.6f * zf, totalHeight * building.podiumRatio);
    const float mainHeight = std::max(4.4f * zf, totalHeight - podiumHeight);

    if (building.family == BuildingFamily::Utility) {
        drawMass(0.0f, 1.2f * zf, baseWidth * 0.96f, baseDepth * 0.90f,
                 podiumHeight + mainHeight * 0.52f, 0.55f);
        if (building.width > 1.1f) {
            drawMass(baseWidth * 0.14f, 4.2f * zf,
                     baseWidth * 0.42f, baseDepth * 0.46f,
                     std::max(2.4f * zf, mainHeight * 0.24f), 0.28f);
        }
    } else {
        drawMass(0.0f, 1.4f * zf, baseWidth, baseDepth, podiumHeight, 0.38f);

        float towerWidth = baseWidth * building.taper;
        float towerDepth = baseDepth * (building.taper + 0.02f);
        if (building.family == BuildingFamily::Residential) {
            towerWidth = baseWidth * (building.taper - 0.06f);
            towerDepth = baseDepth * (building.taper - 0.04f);
        }
        if (building.family == BuildingFamily::Landmark) {
            towerWidth = baseWidth * (building.taper - 0.02f);
            towerDepth = baseDepth * (building.taper - 0.02f);
        }

        const BlockFaces mainFaces = drawMass(0.0f, -podiumHeight,
                                              towerWidth, towerDepth, mainHeight, 1.0f);

        if (building.family == BuildingFamily::Residential) {
            drawMass(-baseWidth * 0.22f, 3.4f * zf,
                     baseWidth * 0.30f, baseDepth * 0.38f,
                     std::max(2.4f * zf, podiumHeight * 0.28f), 0.24f);
        }

        switch (building.roofProfile) {
            case RoofProfile::Flat:
                drawMass(baseWidth * 0.06f, -podiumHeight - mainHeight,
                         towerWidth * 0.22f, towerDepth * 0.24f,
                         std::max(1.6f * zf, mainHeight * 0.08f), 0.16f);
                break;
            case RoofProfile::Stepped:
                drawMass(0.0f, -podiumHeight - mainHeight,
                         towerWidth * 0.72f, towerDepth * 0.72f,
                         std::max(2.0f * zf, mainHeight * 0.18f), 0.40f);
                break;
            case RoofProfile::Terrace:
                drawMass(-towerWidth * 0.12f, -podiumHeight - mainHeight + 1.0f * zf,
                         towerWidth * 0.56f, towerDepth * 0.58f,
                         std::max(2.1f * zf, mainHeight * 0.14f), 0.36f);
                renderer.drawDiamond(mainFaces.top[0].x,
                                     mainFaces.top[0].y + towerDepth * 0.48f,
                                     towerWidth * 0.12f, towerDepth * 0.06f,
                                     Color(0.94f, 0.74f, 0.40f, 0.10f));
                break;
            case RoofProfile::Crown:
                drawMass(0.0f, -podiumHeight - mainHeight,
                         towerWidth * 0.32f, towerDepth * 0.34f,
                         std::max(2.2f * zf, mainHeight * 0.12f), 0.44f);
                renderer.drawLine(iso.x, iso.y - podiumHeight - mainHeight - 2.0f * zf,
                                  iso.x, iso.y - podiumHeight - mainHeight - 9.0f * zf,
                                  Color(0.78f, 0.84f, 0.92f, 0.18f + building.edgeHighlight * 0.12f),
                                  1.0f);
                break;
        }
    }

    if (building.visualTier == VisualTier::Focus) {
        renderer.drawDiamond(iso.x, iso.y - totalHeight - 2.0f * zf,
                             baseWidth * 0.18f, baseDepth * 0.08f,
                             Color(0.70f, 0.82f, 0.96f, 0.04f + building.edgeHighlight * 0.05f));
    }
}

void CityThemeRenderer::drawTrafficLight(IsometricRenderer& renderer,
                                         const Intersection& intersection,
                                         float animationTime) const {
    const IsoCoord iso = RenderUtils::worldToIso(intersection.x, intersection.y);
    const bool green = isTrafficLightGreen(intersection, animationTime);
    renderer.drawLine(iso.x - 4.0f, iso.y + 6.0f, iso.x - 4.0f, iso.y - 8.0f,
                      Color(0.18f, 0.18f, 0.20f, 0.82f), 1.2f);
    renderer.drawDiamond(iso.x - 4.0f, iso.y - 10.0f, 2.6f, 1.4f,
                         Color(0.08f, 0.08f, 0.10f, 0.94f));
    renderer.drawFilledCircle(iso.x - 4.8f, iso.y - 10.0f, 0.9f,
                              green ? Color(0.26f, 0.26f, 0.28f, 0.62f)
                                    : Color(0.92f, 0.20f, 0.16f, 0.88f));
    renderer.drawFilledCircle(iso.x - 3.2f, iso.y - 10.0f, 0.9f,
                              green ? Color(0.28f, 0.96f, 0.44f, 0.92f)
                                    : Color(0.34f, 0.34f, 0.36f, 0.62f));
}

void CityThemeRenderer::cleanup() {}
