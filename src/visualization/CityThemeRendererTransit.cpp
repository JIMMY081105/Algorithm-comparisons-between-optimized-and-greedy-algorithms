#include "visualization/CityThemeRendererInternal.h"

void CityThemeRenderer::drawTransitNetwork(
    IsometricRenderer& renderer,
    const MapGraph& graph,
    const MissionPresentation* mission,
    AnimationController::PlaybackState playbackState,
    float routeRevealProgress,
    float animationTime) {
    applyRoadEvents(graph);

    ensureStaticBatchProjectionCurrent();

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

        if (road.snowBlocked) {
            curbColor = mixColor(curbColor, Color(0.82f, 0.86f, 0.90f, curbColor.a), 0.72f);
            roadColor = Color(0.92f, 0.94f, 0.97f, 0.98f);
            centerLine = withAlpha(Color(0.96f, 0.98f, 1.0f, 1.0f), 0.10f);
            glow = Color(0.82f, 0.90f, 0.98f, 0.10f);
        } else if (weather == CityWeather::Rainy || weather == CityWeather::Stormy) {
            roadColor = mixColor(roadColor, Color(0.12f, 0.16f, 0.22f, roadColor.a), 0.24f);
            glow = mixColor(glow, Color(0.60f, 0.74f, 0.94f, glow.a), 0.30f);
        }

        if (!road.snowBlocked && road.congestion > 0.58f && layerToggles.showCongestion) {
            roadColor = mixColor(roadColor, Color(0.84f, 0.36f, 0.16f, 0.98f),
                                 road.congestion * (0.70f + congestionPulse * 0.30f));
        }
        if (!road.snowBlocked && road.incident && layerToggles.showIncidents) {
            roadColor = mixColor(roadColor, Color(0.82f, 0.14f, 0.12f, 1.0f), 0.60f);
        }
        if (road.arterial) {
            roadColor = mixColor(roadColor, Color(0.24f, 0.25f, 0.27f, roadColor.a), 0.24f);
            centerLine = mixColor(centerLine, Color(0.92f, 0.86f, 0.58f, centerLine.a), 0.36f);
        }
        if (road.snowBlocked) {
            centerLine = mixColor(centerLine, Color(0.92f, 0.96f, 1.0f, centerLine.a), 0.72f);
        }

        // Road events override all other visual states â€” flood = dark blue, festival = purple.
        if (road.eventType == RoadEvent::FLOOD) {
            curbColor  = Color(0.02f, 0.05f, 0.25f, 0.96f);
            roadColor  = Color(0.09f, 0.28f, 0.75f, 1.0f);
            centerLine = withAlpha(Color(0.40f, 0.70f, 1.00f, 1.0f), 0.22f);
            glow       = Color(0.14f, 0.34f, 0.88f, 0.20f);
        } else if (road.eventType == RoadEvent::FESTIVAL) {
            curbColor  = Color(0.20f, 0.04f, 0.28f, 0.96f);
            roadColor  = Color(0.50f, 0.10f, 0.65f, 1.0f);
            centerLine = withAlpha(Color(0.80f, 0.50f, 0.95f, 1.0f), 0.22f);
            glow       = Color(0.55f, 0.15f, 0.70f, 0.20f);
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

    drawRoadEventMarkers();

    if (transitBatch.dirty) {
        std::vector<float> verts;
        verts.reserve(presetBuildings.size() * 720);
        {
            ScopedStaticBatchCapture capture(verts);
            for (const auto& placed : presetBuildings) {
                drawPresetBuilding(renderer, placed);
            }
        }
        transitBatch.upload(verts);
    }
    transitBatch.draw();

    if (layerToggles.showIncidents) {
        for (std::size_t i = 0; i < roads.size(); ++i) {
            const RoadSegment& road = roads[i];
            if (!road.incident || road.snowBlocked) {
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

