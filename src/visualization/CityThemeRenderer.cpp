#include "visualization/CityThemeRenderer.h"

#include "environment/MissionPresentationUtils.h"
#include "visualization/IsometricRenderer.h"

#include <glad/glad.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace {

constexpr float kCityPaddingX = 2.0f;
constexpr float kCityPaddingY = 1.8f;
constexpr float kCongestionPenaltyScale = 0.75f;
constexpr float kIncidentPenaltyScale = 1.75f;
constexpr int kBuildingsPerSquareRows = 5;
constexpr int kBuildingsPerSquareColumns = 10;
constexpr int kBuildingsPerSquare = kBuildingsPerSquareRows * kBuildingsPerSquareColumns;

float clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

float currentZoomFactor() {
    return RenderUtils::getProjection().tileWidth / RenderUtils::BASE_TILE_WIDTH;
}

template <std::size_t N>
void normalizeWeights(std::array<float, N>& weights, float targetSpan) {
    float sum = 0.0f;
    for (const float weight : weights) {
        sum += weight;
    }

    if (sum <= 0.0001f) {
        weights.fill(targetSpan / static_cast<float>(N));
        return;
    }

    const float scale = targetSpan / sum;
    for (float& weight : weights) {
        weight *= scale;
    }
}

float hotspotInfluence(float x, float y,
                       float hotspotX, float hotspotY,
                       float radiusX, float radiusY) {
    const float safeRadiusX = std::max(radiusX, 0.001f);
    const float safeRadiusY = std::max(radiusY, 0.001f);
    const float dx = (x - hotspotX) / safeRadiusX;
    const float dy = (y - hotspotY) / safeRadiusY;
    const float distance = std::sqrt(dx * dx + dy * dy);
    return 1.0f - RenderUtils::smoothstep(clamp01(distance));
}

Color mixColor(const Color& from, const Color& to, float t) {
    t = clamp01(t);
    return Color(
        RenderUtils::lerp(from.r, to.r, t),
        RenderUtils::lerp(from.g, to.g, t),
        RenderUtils::lerp(from.b, to.b, t),
        RenderUtils::lerp(from.a, to.a, t));
}

float pointDistance(float ax, float ay, float bx, float by) {
    const float dx = bx - ax;
    const float dy = by - ay;
    return std::sqrt(dx * dx + dy * dy);
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

} // namespace

CityThemeRenderer::CityThemeRenderer()
    : dashboardInfo{EnvironmentTheme::City, "City", "Adaptive traffic dashboard",
                    "Sunny", "Street-constrained routing", 0.0f, 0, true},
      layoutSeed(0),
      weather(CityWeather::Sunny),
      trafficClock(0.0f),
      gridColumns(0),
      gridRows(0),
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
    std::mt19937 rng(seed ^ 0x43A9D17u);

    generateGridNetwork(graph, rng);
    assignNodeAnchors(graph);
    generateBuildings(graph, rng);
    generateAmbientCars(rng);

    weather = static_cast<CityWeather>(rng() % 4);
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
        if (!pair.valid) {
            continue;
        }

        if (pair.polyline.empty()) {
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
        std::string("City mode follows the street graph under ") +
        toDisplayString(weather) + " conditions, favouring corridors with lower delay and fewer incidents. "
        "Average live congestion load is " + std::to_string(static_cast<int>(avgCongestion)) + "%.";
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

void CityThemeRenderer::randomizeWeather(unsigned int seed) {
    std::mt19937 rng(seed ^ 0xB3E947u);
    setWeather(static_cast<CityWeather>(rng() % 4));
}

void CityThemeRenderer::drawGroundPlane(IsometricRenderer&,
                                        const MapGraph&,
                                        const Truck&,
                                        const MissionPresentation*,
                                        float animationTime) {
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const float left = static_cast<float>(viewport[0]);
    const float top = static_cast<float>(viewport[1]);
    const float right = left + static_cast<float>(viewport[2]);
    const float bottom = top + static_cast<float>(viewport[3]);

    const Color skyTop = (weather == CityWeather::Sunny)
        ? Color(0.80f, 0.89f, 0.96f, 1.0f)
        : (weather == CityWeather::Cloudy)
            ? Color(0.66f, 0.73f, 0.82f, 1.0f)
            : (weather == CityWeather::Rainy)
                ? Color(0.48f, 0.58f, 0.70f, 1.0f)
                : Color(0.30f, 0.38f, 0.48f, 1.0f);
    const Color skyBottom = (weather == CityWeather::Sunny)
        ? Color(0.94f, 0.93f, 0.84f, 1.0f)
        : (weather == CityWeather::Cloudy)
            ? Color(0.76f, 0.78f, 0.82f, 1.0f)
            : (weather == CityWeather::Rainy)
                ? Color(0.38f, 0.44f, 0.52f, 1.0f)
                : Color(0.18f, 0.22f, 0.28f, 1.0f);

    glBegin(GL_QUADS);
    glColor4f(skyTop.r, skyTop.g, skyTop.b, skyTop.a);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glColor4f(skyBottom.r, skyBottom.g, skyBottom.b, skyBottom.a);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();

    const float haze = 0.08f + 0.03f * std::sin(animationTime * 0.4f);
    glBegin(GL_QUADS);
    glColor4f(0.05f, 0.08f, 0.12f, 0.0f);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glColor4f(0.06f, 0.08f, 0.11f, 0.18f + haze);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();
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

        const float congestionPulse =
            0.6f + 0.4f * std::sin(animationTime * 3.0f + static_cast<float>(i));
        Color baseRoad(0.18f, 0.20f, 0.22f, 0.92f);
        if (road.congestion > 0.58f && layerToggles.showCongestion) {
            baseRoad = mixColor(baseRoad, Color(0.82f, 0.38f, 0.18f, 0.96f),
                                road.congestion * (0.70f + congestionPulse * 0.30f));
        }
        if (road.incident && layerToggles.showIncidents) {
            baseRoad = mixColor(baseRoad, Color(0.78f, 0.18f, 0.16f, 1.0f), 0.55f);
        }

        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y, baseRoad, 9.0f);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          Color(0.86f, 0.85f, 0.76f, 0.25f), 1.2f);

        const float midX = (isoFrom.x + isoTo.x) * 0.5f;
        const float midY = (isoFrom.y + isoTo.y) * 0.5f;
        renderer.drawDiamond(midX, midY, 2.4f, 1.0f,
                             Color(0.92f, 0.88f, 0.62f, 0.12f));
    }

    // Draw buildings after the road bed so tall towers can occlude streets
    // from the current camera angle instead of leaving roads painted on top.
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
                                  Color(0.34f, 0.70f, 0.90f, 0.20f), 1.0f);
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
                : Color(0.94f, 0.94f, 0.98f, 0.80f);
            renderer.drawDiamond(iso.x, iso.y, 3.0f, 1.4f, carColor);
            renderer.drawDiamondOutline(iso.x, iso.y, 3.0f, 1.4f,
                                        Color(0.10f, 0.10f, 0.12f, 0.55f), 1.0f);
        }
    }
}

void CityThemeRenderer::drawWasteNode(IsometricRenderer& renderer,
                                      const WasteNode& node,
                                      float animationTime) {
    const IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
    const Color urgency = RenderUtils::getUrgencyColor(node.getUrgency());
    const float pulse = 0.65f + 0.35f * std::sin(animationTime * 3.2f + node.getId());
    const Color padColor = node.isCollected()
        ? Color(0.32f, 0.34f, 0.38f, 0.85f)
        : Color(0.26f, 0.28f, 0.32f, 0.92f);
    const Color blockColor = node.isCollected()
        ? Color(0.42f, 0.46f, 0.50f, 0.88f)
        : mixColor(Color(0.34f, 0.38f, 0.44f, 0.96f), urgency, 0.34f);

    renderer.drawDiamond(iso.x, iso.y + 9.0f, 11.0f, 5.5f, padColor);
    renderer.drawIsometricBlock(iso.x, iso.y + 4.0f, 12.0f, 8.0f, 10.0f,
                                blockColor, Color(0.18f, 0.20f, 0.24f, 0.92f));
    renderer.drawDiamond(iso.x, iso.y - 7.0f, 5.0f, 2.4f,
                         Color(urgency.r, urgency.g, urgency.b,
                               node.isCollected() ? 0.18f : 0.28f + pulse * 0.14f));
    renderer.drawRing(iso.x, iso.y - 12.0f, 3.4f,
                      Color(urgency.r, urgency.g, urgency.b,
                            node.isCollected() ? 0.18f : 0.24f + pulse * 0.10f),
                      1.2f);
}

void CityThemeRenderer::drawHQNode(IsometricRenderer& renderer,
                                   const WasteNode& node,
                                   float animationTime) {
    const IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 2.2f);
    renderer.drawDiamond(iso.x, iso.y + 14.0f, 17.0f, 8.5f,
                         Color(0.18f, 0.20f, 0.24f, 0.94f));
    renderer.drawIsometricBlock(iso.x, iso.y + 4.0f, 18.0f, 12.0f, 16.0f,
                                Color(0.36f, 0.44f, 0.58f, 0.98f),
                                Color(0.18f, 0.22f, 0.28f, 0.98f));
    renderer.drawDiamond(iso.x, iso.y - 16.0f, 8.0f, 3.6f,
                         Color(0.64f, 0.84f, 0.98f, 0.22f + pulse * 0.10f));
    renderer.drawRing(iso.x, iso.y - 16.0f, 5.2f,
                      Color(0.76f, 0.92f, 1.0f, 0.18f + pulse * 0.06f), 1.4f);
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
                                               float) {
    (void)renderer;
}

void CityThemeRenderer::drawAtmosphericEffects(IsometricRenderer& renderer,
                                               const MapGraph&,
                                               float animationTime) {
    if (weather == CityWeather::Rainy || weather == CityWeather::Stormy) {
        for (int i = 0; i < 24; ++i) {
            const float phase = animationTime * (weather == CityWeather::Stormy ? 7.0f : 5.0f) +
                                static_cast<float>(i) * 1.2f;
            const float worldX = 1.0f + std::fmod(phase * 1.4f + i * 4.0f, 28.0f);
            const float worldY = 1.0f + std::fmod(phase * 0.9f + i * 2.7f, 24.0f);
            const IsoCoord iso = RenderUtils::worldToIso(worldX, worldY);
            renderer.drawLine(iso.x, iso.y - 10.0f, iso.x - 3.0f, iso.y + 6.0f,
                              Color(0.82f, 0.88f, 0.98f,
                                    weather == CityWeather::Stormy ? 0.22f : 0.16f),
                              1.0f);
        }
    }

    if (weather == CityWeather::Cloudy || weather == CityWeather::Stormy) {
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        const float left = static_cast<float>(viewport[0]);
        const float top = static_cast<float>(viewport[1]);
        const float right = left + static_cast<float>(viewport[2]);
        const float bottom = top + static_cast<float>(viewport[3]);

        glBegin(GL_QUADS);
        glColor4f(0.08f, 0.10f, 0.14f, 0.08f + weatherOverlayStrength());
        glVertex2f(left, top);
        glVertex2f(right, top);
        glColor4f(0.08f, 0.10f, 0.14f, 0.14f + weatherOverlayStrength());
        glVertex2f(right, bottom);
        glVertex2f(left, bottom);
        glEnd();
    }
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
    std::uniform_real_distribution<float> congestionDistribution(0.05f, 0.55f);
    std::bernoulli_distribution incidentDistribution(0.10);
    const int hotRow = static_cast<int>(layoutSeed % std::max(1, gridRows - 1));
    const int hotCol = static_cast<int>((layoutSeed / 3) % std::max(1, gridColumns - 1));

    auto addRoad = [&](int from, int to) {
        const Intersection& a = intersections[from];
        const Intersection& b = intersections[to];
        float congestion = congestionDistribution(rng);
        if (from / gridColumns == hotRow || to / gridColumns == hotRow ||
            from % gridColumns == hotCol || to % gridColumns == hotCol) {
            congestion = std::min(0.92f, congestion + 0.24f);
        }

        const bool incident = incidentDistribution(rng) && congestion > 0.35f;
        const float signalDelay =
            (a.hasTrafficLight || b.hasTrafficLight) ? 0.18f + congestion * 0.25f : 0.0f;

        const int roadIndex = static_cast<int>(roads.size());
        roads.push_back(RoadSegment{
            from,
            to,
            pointDistance(a.x, a.y, b.x, b.y),
            congestion,
            incident,
            signalDelay
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

void CityThemeRenderer::generateBuildings(const MapGraph&, std::mt19937& rng) {
    buildings.clear();
    buildings.reserve((gridRows - 1) * (gridColumns - 1) * kBuildingsPerSquare);
    std::uniform_real_distribution<float> columnWeightDistribution(0.72f, 1.50f);
    std::uniform_real_distribution<float> rowWeightDistribution(0.70f, 1.45f);
    std::uniform_real_distribution<float> widthFillDistribution(0.88f, 1.34f);
    std::uniform_real_distribution<float> depthFillDistribution(0.86f, 1.30f);
    std::uniform_real_distribution<float> hueDistribution(-0.03f, 0.05f);
    std::uniform_real_distribution<float> centerBiasDistribution(0.26f, 0.74f);
    std::uniform_real_distribution<float> positionJitter(-0.16f, 0.16f);
    std::uniform_real_distribution<float> blockTallnessDistribution(0.0f, 1.0f);
    std::uniform_real_distribution<float> skylineBiasDistribution(0.0f, 1.0f);
    std::uniform_real_distribution<float> massNoiseDistribution(0.0f, 1.0f);
    std::uniform_real_distribution<float> hotspotDistribution(0.14f, 0.86f);
    std::uniform_real_distribution<float> hotspotRadiusDistribution(0.16f, 0.38f);
    std::uniform_real_distribution<float> heightNoiseDistribution(0.0f, 8.0f);
    std::uniform_real_distribution<float> towerBoostDistribution(14.0f, 30.0f);
    std::uniform_int_distribution<int> signatureLotDistribution(0, kBuildingsPerSquare - 1);
    std::bernoulli_distribution towerDistribution(0.14);

    for (int row = 0; row + 1 < gridRows; ++row) {
        for (int col = 0; col + 1 < gridColumns; ++col) {
            const Intersection& a = intersections[row * gridColumns + col];
            const Intersection& b = intersections[row * gridColumns + col + 1];
            const Intersection& c = intersections[(row + 1) * gridColumns + col];
            const float blockMinX = RenderUtils::lerp(a.x, b.x, 0.08f);
            const float blockMaxX = RenderUtils::lerp(a.x, b.x, 0.92f);
            const float blockMinY = RenderUtils::lerp(a.y, c.y, 0.10f);
            const float blockMaxY = RenderUtils::lerp(a.y, c.y, 0.90f);
            const float blockSpanX = blockMaxX - blockMinX;
            const float blockSpanY = blockMaxY - blockMinY;
            const float blockTallness = blockTallnessDistribution(rng);
            const float skylineBias = skylineBiasDistribution(rng);
            const int signatureLotA = signatureLotDistribution(rng);
            const int signatureLotB = signatureLotDistribution(rng);
            const float primaryHotspotX = hotspotDistribution(rng);
            const float primaryHotspotY = hotspotDistribution(rng);
            const float secondaryHotspotX = hotspotDistribution(rng);
            const float secondaryHotspotY = hotspotDistribution(rng);
            const float primaryRadiusX = hotspotRadiusDistribution(rng);
            const float primaryRadiusY = hotspotRadiusDistribution(rng);
            const float secondaryRadiusX = hotspotRadiusDistribution(rng) * 1.25f;
            const float secondaryRadiusY = hotspotRadiusDistribution(rng) * 1.25f;
            const float blockBulk = 1.08f + skylineBias * 0.22f + blockTallness * 0.16f;
            const float blockStretchX = 0.92f + massNoiseDistribution(rng) * 0.24f;
            const float blockStretchY = 0.92f + massNoiseDistribution(rng) * 0.24f;

            std::array<float, kBuildingsPerSquareColumns> columnWidths{};
            std::array<float, kBuildingsPerSquareRows> rowHeights{};
            for (float& width : columnWidths) {
                width = columnWeightDistribution(rng);
            }
            for (float& height : rowHeights) {
                height = rowWeightDistribution(rng);
            }
            normalizeWeights(columnWidths, blockSpanX);
            normalizeWeights(rowHeights, blockSpanY);

            std::array<float, kBuildingsPerSquareColumns + 1> columnOffsets{};
            std::array<float, kBuildingsPerSquareRows + 1> rowOffsets{};
            columnOffsets[0] = 0.0f;
            rowOffsets[0] = 0.0f;
            for (int index = 0; index < kBuildingsPerSquareColumns; ++index) {
                columnOffsets[index + 1] = columnOffsets[index] + columnWidths[index];
            }
            for (int index = 0; index < kBuildingsPerSquareRows; ++index) {
                rowOffsets[index + 1] = rowOffsets[index] + rowHeights[index];
            }

            for (int blockRow = 0; blockRow < kBuildingsPerSquareRows; ++blockRow) {
                for (int blockCol = 0; blockCol < kBuildingsPerSquareColumns; ++blockCol) {
                    const int slotIndex = blockRow * kBuildingsPerSquareColumns + blockCol;
                    const float cellMinX = blockMinX + columnOffsets[blockCol];
                    const float cellMaxX = blockMinX + columnOffsets[blockCol + 1];
                    const float cellMinY = blockMinY + rowOffsets[blockRow];
                    const float cellMaxY = blockMinY + rowOffsets[blockRow + 1];
                    const float cellSpanX = cellMaxX - cellMinX;
                    const float cellSpanY = cellMaxY - cellMinY;
                    const float localX =
                        (columnOffsets[blockCol] + columnOffsets[blockCol + 1]) * 0.5f / blockSpanX;
                    const float localY =
                        (rowOffsets[blockRow] + rowOffsets[blockRow + 1]) * 0.5f / blockSpanY;
                    const float hotspotA = hotspotInfluence(localX, localY,
                                                            primaryHotspotX, primaryHotspotY,
                                                            primaryRadiusX, primaryRadiusY);
                    const float hotspotB = hotspotInfluence(localX, localY,
                                                            secondaryHotspotX, secondaryHotspotY,
                                                            secondaryRadiusX, secondaryRadiusY);
                    const float urbanCore = clamp01(std::max(hotspotA, hotspotB * 0.82f) +
                                                    blockTallness * 0.28f +
                                                    skylineBias * 0.18f +
                                                    massNoiseDistribution(rng) * 0.20f);
                    const bool tower = slotIndex == signatureLotA ||
                                       slotIndex == signatureLotB ||
                                       (urbanCore > 0.82f &&
                                        (skylineBias > 0.34f || blockTallness > 0.56f) &&
                                        towerDistribution(rng));
                    const float width =
                        cellSpanX * widthFillDistribution(rng) *
                        (0.92f + urbanCore * 0.26f) * blockBulk * blockStretchX;
                    const float depth =
                        cellSpanY * depthFillDistribution(rng) *
                        (0.90f + urbanCore * 0.24f) * blockBulk * blockStretchY;
                    const float heightBias =
                        clamp01(0.18f + blockTallness * 0.36f + urbanCore * 0.46f);
                    float height = 8.0f + heightBias * 16.0f +
                                   skylineBias * 4.0f + heightNoiseDistribution(rng);
                    if (tower) {
                        height += towerBoostDistribution(rng);
                    } else if (urbanCore < 0.14f) {
                        height *= 0.86f;
                    }

                    const float centerX = RenderUtils::lerp(cellMinX, cellMaxX,
                                                            centerBiasDistribution(rng)) +
                                          cellSpanX * positionJitter(rng) * 0.18f;
                    const float centerY = RenderUtils::lerp(cellMinY, cellMaxY,
                                                            centerBiasDistribution(rng)) +
                                          cellSpanY * positionJitter(rng) * 0.18f;

                    buildings.push_back(BuildingLot{
                        centerX,
                        centerY,
                        width,
                        depth,
                        height,
                        hueDistribution(rng),
                        tower
                    });
                }
            }
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

void CityThemeRenderer::generateAmbientCars(std::mt19937& rng) {
    ambientCars.clear();
    if (roads.empty()) {
        return;
    }

    std::uniform_int_distribution<int> roadDistribution(0, static_cast<int>(roads.size()) - 1);
    std::uniform_real_distribution<float> offsetDistribution(0.0f, 1.0f);
    std::uniform_real_distribution<float> speedDistribution(0.05f, 0.18f);

    for (int i = 0; i < 18; ++i) {
        const int roadIndex = roadDistribution(rng);
        ambientCars.push_back(AmbientCar{
            roadIndex,
            offsetDistribution(rng),
            speedDistribution(rng) * (1.0f - roads[roadIndex].congestion * 0.6f),
            roads[roadIndex].incident && i % 5 == 0
        });
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
            return 0.0f;
    }
}

bool CityThemeRenderer::isTrafficLightGreen(const Intersection& intersection,
                                            float animationTime) const {
    const float phase = std::fmod(animationTime + intersection.cycleOffset, 6.0f);
    return phase < 2.8f;
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
    dashboardInfo.subtitle = "Adaptive traffic dashboard";
    dashboardInfo.atmosphereLabel =
        (weather == CityWeather::Sunny) ? "Fast daytime circulation"
        : (weather == CityWeather::Cloudy) ? "Muted urban flow"
        : (weather == CityWeather::Rainy) ? "Wet-surface traffic slowdown"
        : "Storm-driven diversion pressure";
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

        const IsoCoord isoFrom =
            RenderUtils::worldToIso(path.points[i - 1].x, path.points[i - 1].y);
        const IsoCoord isoTo = RenderUtils::worldToIso(endX, endY);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          Color(0.08f, 0.10f, 0.12f, 0.38f + pulse * 0.08f), 9.0f);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          Color(0.24f, 0.84f, 0.96f, 0.30f + pulse * 0.10f), 3.8f);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          Color(0.94f, 0.98f, 1.0f, 0.18f + pulse * 0.08f), 1.2f);
    }

    for (const auto& stop : path.stops) {
        const IsoCoord iso = RenderUtils::worldToIso(path.points[stop.pointIndex].x,
                                                     path.points[stop.pointIndex].y);
        renderer.drawDiamond(iso.x, iso.y, 4.5f, 2.0f, Color(0.70f, 0.94f, 1.0f, 0.18f));
        renderer.drawDiamondOutline(iso.x, iso.y, 3.0f, 1.4f,
                                    Color(0.94f, 0.98f, 1.0f, 0.24f), 1.0f);
    }
}

void CityThemeRenderer::drawBuildingLot(IsometricRenderer& renderer,
                                        const BuildingLot& building) const {
    const IsoCoord iso = RenderUtils::worldToIso(building.x, building.y);
    const float zf = currentZoomFactor();
    const float shapeNoise = clamp01((building.hueShift + 0.03f) / 0.08f);
    const Color podiumRoof = mixColor(Color(0.03f, 0.03f, 0.04f, 0.98f),
                                      Color(0.09f, 0.09f, 0.10f, 0.98f),
                                      0.24f + building.hueShift);
    const Color podiumSide = mixColor(Color(0.0f, 0.0f, 0.0f, 0.96f),
                                      Color(0.05f, 0.05f, 0.06f, 0.96f),
                                      0.18f + building.hueShift);
    const Color roof = mixColor(Color(0.04f, 0.04f, 0.05f, 0.98f),
                                Color(0.12f, 0.12f, 0.13f, 0.98f),
                                0.28f + building.hueShift);
    const Color side = mixColor(Color(0.01f, 0.01f, 0.02f, 0.98f),
                                Color(0.07f, 0.07f, 0.08f, 0.98f),
                                0.22f + building.hueShift);
    const float totalHeight =
        (building.tower ? building.height * 1.78f : building.height * 1.34f) * zf;
    const float podiumRatio = building.tower
        ? 0.16f + shapeNoise * 0.08f
        : 0.10f + shapeNoise * 0.10f;
    const float podiumHeight =
        std::max(2.8f * zf, totalHeight * podiumRatio);
    const float shaftHeight = std::max(4.4f * zf, totalHeight - podiumHeight);
    const float podiumWidth = building.width * (12.0f + shapeNoise * 1.8f) * zf;
    const float podiumDepth = building.depth * (9.6f + shapeNoise * 1.4f) * zf;
    const float shaftTaper = building.tower
        ? 0.60f + shapeNoise * 0.20f
        : 0.78f + shapeNoise * 0.18f;
    const float shaftWidth = podiumWidth * shaftTaper;
    const float shaftDepth = podiumDepth * (shaftTaper + 0.02f);
    const float shadowOffsetY = 8.0f * zf;
    const float podiumBaseYOffset = 1.2f * zf;

    renderer.drawDiamond(iso.x, iso.y + shadowOffsetY, podiumWidth * 1.08f,
                         podiumDepth * 0.56f, Color(0.02f, 0.02f, 0.02f, 0.26f));
    renderer.drawIsometricBlock(iso.x, iso.y + podiumBaseYOffset, podiumWidth,
                                podiumDepth, podiumHeight,
                                podiumRoof, podiumSide);
    renderer.drawIsometricBlock(iso.x, iso.y - podiumHeight, shaftWidth,
                                shaftDepth, shaftHeight,
                                roof, side);

    if (building.tower) {
        renderer.drawIsometricBlock(iso.x, iso.y - podiumHeight - shaftHeight,
                                    shaftWidth * (0.34f + shapeNoise * 0.18f),
                                    shaftDepth * (0.34f + shapeNoise * 0.18f),
                                    std::max(2.0f * zf, shaftHeight * (0.10f + shapeNoise * 0.05f)),
                                    Color(0.14f, 0.14f, 0.15f, 0.98f),
                                    Color(0.04f, 0.04f, 0.05f, 0.96f));
    }

    renderer.drawDiamondOutline(iso.x, iso.y - podiumHeight - shaftHeight,
                                shaftWidth * 0.48f, shaftDepth * 0.24f,
                                Color(0.18f, 0.18f, 0.20f, 0.34f),
                                std::max(0.8f, 0.8f * zf));
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
