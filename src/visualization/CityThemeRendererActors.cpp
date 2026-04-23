#include "visualization/CityThemeRendererInternal.h"

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
    ensureStaticBatchProjectionCurrent();

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

    if (decorativeBatch.dirty) {
        std::vector<float> verts;
        verts.reserve(presetVehicles.size() * 36);
        {
            ScopedStaticBatchCapture capture(verts);
            for (const auto& vehicle : presetVehicles) {
                drawPresetVehicle(renderer, vehicle);
            }
        }
        decorativeBatch.upload(verts);
    }
    decorativeBatch.draw();
}

void CityThemeRenderer::drawAtmosphericEffects(IsometricRenderer& renderer,
                                               const MapGraph&,
                                               float animationTime) {
    (void)renderer;
    ensureStaticBatchProjectionCurrent();

    // Mountains render here (after ground, decorations, transit network,
    // nodes and truck) so tall peaks visually occlude road edges and block
    // borders â€” matching real parallax where a taller object closer to the
    // camera covers the flat terrain behind it. Routing is graph-based and
    // unaffected by draw order.
    if (mountainBatch.dirty) {
        std::vector<float> verts;
        verts.reserve(mountains.size() * 180);
        {
            ScopedStaticBatchCapture capture(verts);
            for (const auto& peak : mountains) {
                drawMountain(peak);
            }
        }
        mountainBatch.upload(verts);
    }
    mountainBatch.draw();

    const float spanX = std::max(1.0f, peripheralMaxX - peripheralMinX);
    const float spanY = std::max(1.0f, peripheralMaxY - peripheralMinY);

    if (hasSnowfall()) {
        const bool winterStorm = hasWinterStormActive();
        const int flakeCount = winterStorm ? 220 : 140;
        for (int i = 0; i < flakeCount; ++i) {
            const float phase =
                animationTime * (winterStorm ? 3.1f : 2.2f) +
                static_cast<float>(i) * 0.97f;
            const float worldX =
                peripheralMinX + std::fmod(phase * 1.1f + i * 4.1f, spanX);
            const float worldY =
                peripheralMinY + std::fmod(phase * 0.9f + i * 2.8f, spanY);
            const float drift =
                std::sin(animationTime * (winterStorm ? 3.7f : 2.4f) +
                         static_cast<float>(i) * 0.61f) *
                (winterStorm ? 4.8f : 2.8f);
            const ZoneVisibility zone = computeZoneVisibility(
                worldX, worldY,
                operationalCenterX, operationalCenterY,
                operationalRadiusX, operationalRadiusY);
            if (zone.transition <= 0.18f) {
                continue;
            }

            const IsoCoord iso = RenderUtils::worldToIso(worldX, worldY);
            const float flakeRadius = 1.5f + static_cast<float>(i % 4) * 0.55f;
            const Color flake = attenuateToZone(
                Color(0.94f, 0.97f, 1.0f,
                      winterStorm ? 0.36f : 0.28f),
                zone,
                Color(0.02f, 0.03f, 0.05f,
                      winterStorm ? 0.36f : 0.28f),
                0.28f,
                0.04f,
                0.06f);
            renderer.drawFilledCircle(iso.x + drift, iso.y - 1.0f,
                                      flakeRadius, flake);
            renderer.drawLine(iso.x + drift - flakeRadius * (winterStorm ? 1.3f : 0.7f),
                              iso.y - flakeRadius * (winterStorm ? 4.6f : 2.8f),
                              iso.x + drift + flakeRadius * 0.2f,
                              iso.y + flakeRadius * 0.7f,
                              withAlpha(flake, flake.a * (winterStorm ? 0.76f : 0.42f)),
                              winterStorm ? 1.2f : 0.9f);
        }
    } else if (weather == CityWeather::Rainy || weather == CityWeather::Stormy) {
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
    const float edgeAlpha =
        0.01f + weatherOverlayStrength() * 0.04f +
        (hasWinterStormActive() ? 0.03f : 0.0f);

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

    if (hasWinterStormActive()) {
        glBegin(GL_QUADS);
        glColor4f(0.01f, 0.02f, 0.04f, 0.08f);
        glVertex2f(left, top);
        glVertex2f(right, top);
        glColor4f(0.01f, 0.02f, 0.04f, 0.14f);
        glVertex2f(right, bottom);
        glVertex2f(left, bottom);
        glEnd();
    }

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

}

