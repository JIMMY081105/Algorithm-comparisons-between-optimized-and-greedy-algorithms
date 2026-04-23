#include "CityThemeRendererInternal.h"

void CityThemeRenderer::generateAmbientCars(std::mt19937& rng) {
    ambientCars.clear();
    if (roads.empty()) {
        return;
    }

    std::vector<int> availableRoads;
    availableRoads.reserve(roads.size());
    std::vector<double> weights;
    weights.reserve(roads.size());
    for (int i = 0; i < static_cast<int>(roads.size()); ++i) {
        const RoadSegment& road = roads[static_cast<std::size_t>(i)];
        if (road.snowBlocked) {
            continue;
        }
        availableRoads.push_back(i);
        weights.push_back(0.35 + road.focusWeight * 0.95 + (road.incident ? 0.10 : 0.0));
    }

    if (availableRoads.empty()) {
        return;
    }

    std::discrete_distribution<int> roadDistribution(weights.begin(), weights.end());
    std::uniform_real_distribution<float> offsetDistribution(0.0f, 1.0f);
    std::uniform_real_distribution<float> speedDistribution(0.05f, 0.18f);

    for (int i = 0; i < 22; ++i) {
        const int roadIndex =
            availableRoads[static_cast<std::size_t>(roadDistribution(rng))];
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

void CityThemeRenderer::refreshSeasonalRoadState() {
    for (RoadSegment& road : roads) {
        road.snowBlocked = false;
    }

    if (!hasSnowfall() || roads.empty() || intersections.empty()) {
        return;
    }

    std::vector<int> candidates;
    candidates.reserve(roads.size());
    for (int i = 0; i < static_cast<int>(roads.size()); ++i) {
        const RoadSegment& road = roads[static_cast<std::size_t>(i)];
        if (road.arterial || road.visualTier == VisualTier::Focus) {
            continue;
        }
        candidates.push_back(i);
    }

    if (candidates.empty()) {
        return;
    }

    std::mt19937 rng(layoutSeed ^ 0x8F4C39u ^
                     static_cast<unsigned int>(season) * 173u ^
                     static_cast<unsigned int>(weather) * 313u);
    std::shuffle(candidates.begin(), candidates.end(), rng);

    const int targetBlockedRoads =
        std::max(2, std::min(6, static_cast<int>(roads.size() / 14)));
    int blockedRoads = 0;
    for (const int roadIndex : candidates) {
        roads[static_cast<std::size_t>(roadIndex)].snowBlocked = true;
        if (!isRoadNetworkConnected()) {
            roads[static_cast<std::size_t>(roadIndex)].snowBlocked = false;
            continue;
        }

        ++blockedRoads;
        if (blockedRoads >= targetBlockedRoads) {
            break;
        }
    }
}

void CityThemeRenderer::refreshPairRoutes(const MapGraph& graph) {
    applyRoadEvents(graph);

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
                                                 int endIntersection,
                                                 bool avoidRoadEvents) const {
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
            if (road.snowBlocked ||
                (avoidRoadEvents && road.eventType != RoadEvent::NONE)) {
                continue;
            }
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

bool CityThemeRenderer::isRoadNetworkConnected() const {
    if (intersections.empty()) {
        return true;
    }

    std::vector<bool> visited(intersections.size(), false);
    std::queue<int> pending;
    pending.push(0);
    visited[0] = true;

    while (!pending.empty()) {
        const int current = pending.front();
        pending.pop();

        for (const int roadIndex : roadAdjacency[current]) {
            const RoadSegment& road = roads[static_cast<std::size_t>(roadIndex)];
            if (road.snowBlocked) {
                continue;
            }

            const int next = (road.from == current) ? road.to : road.from;
            if (!visited[static_cast<std::size_t>(next)]) {
                visited[static_cast<std::size_t>(next)] = true;
                pending.push(next);
            }
        }
    }

    return std::all_of(visited.begin(), visited.end(),
                       [](bool reached) { return reached; });
}

bool CityThemeRenderer::hasSnowfall() const {
    return isSnowWeather(season, weather);
}

bool CityThemeRenderer::hasWinterStormActive() const {
    return isWinterStorm(season, weather);
}

float CityThemeRenderer::roadCost(const RoadSegment& road) const {
    if (road.snowBlocked) {
        return 1000000.0f;
    }
    return road.baseLength * weatherDistanceMultiplier() +
           road.baseLength * road.congestion * kCongestionPenaltyScale +
           (road.incident ? road.baseLength * kIncidentPenaltyScale : 0.0f) +
           road.signalDelay;
}

float CityThemeRenderer::roadTravelSpeedFactor(const RoadSegment& road) const {
    if (road.snowBlocked) {
        return 0.15f;
    }
    if (road.incident || road.congestion >= 0.82f) {
        return 0.25f;
    }
    if (road.congestion >= 0.58f) {
        return 0.50f;
    }
    return 1.0f;
}

float CityThemeRenderer::weatherDistanceMultiplier() const {
    if (season == CitySeason::Winter) {
        switch (weather) {
            case CityWeather::Sunny: return 1.04f;
            case CityWeather::Cloudy: return 1.10f;
            case CityWeather::Rainy: return 1.22f;
            case CityWeather::Stormy: return 1.36f;
            default: return 1.0f;
        }
    }

    switch (weather) {
        case CityWeather::Sunny: return 1.00f;
        case CityWeather::Cloudy: return 1.05f;
        case CityWeather::Rainy: return 1.14f;
        case CityWeather::Stormy: return 1.25f;
        default: return 1.0f;
    }
}

float CityThemeRenderer::weatherOverlayStrength() const {
    if (hasSnowfall()) {
        return hasWinterStormActive() ? 0.24f : 0.18f;
    }

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

int CityThemeRenderer::selectEventRoadIndex(const std::vector<int>& intersectionPath) const {
    if (intersectionPath.size() < 2) {
        return -1;
    }

    float totalLength = 0.0f;
    std::vector<std::pair<int, float>> pathRoads;
    pathRoads.reserve(intersectionPath.size() - 1);
    for (std::size_t i = 1; i < intersectionPath.size(); ++i) {
        const int roadIndex = findRoadIndex(intersectionPath[i - 1], intersectionPath[i]);
        if (roadIndex < 0 || roadIndex >= static_cast<int>(roads.size())) {
            continue;
        }

        const float length = roads[static_cast<std::size_t>(roadIndex)].baseLength;
        pathRoads.push_back({roadIndex, length});
        totalLength += length;
    }

    if (pathRoads.empty()) {
        return -1;
    }

    const float target = totalLength * 0.5f;
    float travelled = 0.0f;
    for (const auto& [roadIndex, length] : pathRoads) {
        travelled += length;
        if (travelled >= target) {
            return roadIndex;
        }
    }

    return pathRoads.back().first;
}

void CityThemeRenderer::applyRoadEvents(const MapGraph& graph) {
    for (auto& road : roads) {
        road.eventType = RoadEvent::NONE;
    }

    if (nodeAnchors.empty() || intersections.empty()) return;

    for (const auto& ev : graph.getActiveEdgeEvents()) {
        if (ev.type == RoadEvent::NONE) continue;

        const int idxA = graph.findNodeIndex(ev.fromId);
        const int idxB = graph.findNodeIndex(ev.toId);
        if (idxA < 0 || idxB < 0) continue;
        if (idxA >= static_cast<int>(nodeAnchors.size()) ||
            idxB >= static_cast<int>(nodeAnchors.size())) continue;

        const int anchorA = nodeAnchors[idxA];
        const int anchorB = nodeAnchors[idxB];
        if (anchorA < 0 || anchorA >= static_cast<int>(intersections.size())) continue;
        if (anchorB < 0 || anchorB >= static_cast<int>(intersections.size())) continue;
        if (anchorA == anchorB) continue;

        const std::vector<int> path = shortestPath(anchorA, anchorB, false);
        const int roadIdx = selectEventRoadIndex(path);
        if (roadIdx >= 0 && roadIdx < static_cast<int>(roads.size())) {
            roads[static_cast<std::size_t>(roadIdx)].eventType = ev.type;
        }
    }
}

