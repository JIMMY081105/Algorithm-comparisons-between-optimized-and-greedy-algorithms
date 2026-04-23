#include "CityThemeRendererInternal.h"

void CityThemeRenderer::drawRoadEventMarkers() const {
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    if (fg == nullptr) {
        return;
    }

    for (const RoadSegment& road : roads) {
        if (road.eventType == RoadEvent::NONE) {
            continue;
        }

        const Intersection& from = intersections[road.from];
        const Intersection& to = intersections[road.to];
        const float midWorldX = (from.x + to.x) * 0.5f;
        const float midWorldY = (from.y + to.y) * 0.5f;
        const ZoneVisibility zone = computeZoneVisibility(
            midWorldX, midWorldY,
            operationalCenterX, operationalCenterY,
            operationalRadiusX, operationalRadiusY);
        if (zone.transition <= 0.08f) {
            continue;
        }

        const IsoCoord iso = RenderUtils::worldToIso(midWorldX, midWorldY);
        const char* label = RoadEventRules::label(road.eventType);
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        constexpr float kPadX = 5.5f;
        constexpr float kPadY = 3.5f;
        const ImVec2 min(iso.x - textSize.x * 0.5f - kPadX,
                         iso.y - textSize.y * 0.5f - kPadY);
        const ImVec2 max(iso.x + textSize.x * 0.5f + kPadX,
                         iso.y + textSize.y * 0.5f + kPadY);
        const ImU32 bg = (road.eventType == RoadEvent::FLOOD)
            ? IM_COL32(20, 60, 180, 245)
            : IM_COL32(100, 25, 130, 245);
        const ImU32 border = (road.eventType == RoadEvent::FLOOD)
            ? IM_COL32(95, 155, 255, 230)
            : IM_COL32(210, 120, 240, 230);

        fg->AddRectFilled(min, max, bg, 4.0f);
        fg->AddRect(min, max, border, 4.0f, 0, 1.0f);
        fg->AddText(ImVec2(iso.x - textSize.x * 0.5f,
                           iso.y - textSize.y * 0.5f),
                    IM_COL32(255, 255, 255, 255), label);
    }
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
    dashboardInfo.season = season;
    dashboardInfo.seasonLabel = toDisplayString(season);
    dashboardInfo.weatherLabel = toSeasonalWeatherString(season, weather);
    dashboardInfo.subtitle =
        std::string(toDisplayString(season)) + " district-planned urban map";

    if (season == CitySeason::Summer) {
        dashboardInfo.atmosphereLabel =
            "Sakura-tinted park districts under warm boulevard circulation";
    } else if (season == CitySeason::Autumn) {
        dashboardInfo.atmosphereLabel =
            "Amber park canopies and cooler cross-district routing pressure";
    } else if (season == CitySeason::Winter && hasSnowfall()) {
        dashboardInfo.atmosphereLabel =
            hasWinterStormActive()
                ? "Darkened winter grid with snow-blocked lanes and rerouted traffic"
                : "Snowfall across the district grid with partial lane closures";
    } else {
        dashboardInfo.atmosphereLabel =
            (weather == CityWeather::Sunny) ? "Satellite-lit mixed districts and boulevard flow"
            : (weather == CityWeather::Cloudy) ? "Soft-density urban fabric under muted circulation"
            : (weather == CityWeather::Rainy) ? "Wet-surface district logistics through parks and campus blocks"
            : "Storm-loaded boulevard routing across dense landmark districts";
    }
    dashboardInfo.congestionLevel = roads.empty()
        ? 0.0f
        : congestionSum / static_cast<float>(roads.size());
    dashboardInfo.incidentCount = incidentCount;
    dashboardInfo.supportsWeather = true;
    dashboardInfo.supportsSeasons = true;
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

