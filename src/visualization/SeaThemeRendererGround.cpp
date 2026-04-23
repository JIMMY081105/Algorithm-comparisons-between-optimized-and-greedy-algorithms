#include "visualization/SeaThemeRendererInternal.h"

void SeaThemeRenderer::drawGroundPlane(IsometricRenderer& renderer,
                                        const MapGraph& graph,
                                        const Truck& truck,
                                        const MissionPresentation* mission,
                                        float animationTime) {
    const RouteResult& currentRoute = routeFromMission(mission);
    activeGraph = &graph;
    updateSceneBounds(graph);
    updateGarbageSinkState(graph, renderer.getLastDeltaTime());

    if (graph.getNodes().empty()) return;

    const float paddedMinX = sceneBounds.minX - kFieldPadding - 2.5f;
    const float paddedMaxX = sceneBounds.maxX + kFieldPadding + 2.5f;
    const float paddedMinY = sceneBounds.minY - kFieldPadding - 2.5f;
    const float paddedMaxY = sceneBounds.maxY + kFieldPadding + 2.5f;
    const float centerX = (paddedMinX + paddedMaxX) * 0.5f;
    const float centerY = (paddedMinY + paddedMaxY) * 0.5f;
    const float spanX = std::max(1.0f, paddedMaxX - paddedMinX);
    const float spanY = std::max(1.0f, paddedMaxY - paddedMinY);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const float left = static_cast<float>(viewport[0]);
    const float top = static_cast<float>(viewport[1]);
    const float right = left + static_cast<float>(viewport[2]);
    const float bottom = top + static_cast<float>(viewport[3]);

    glBegin(GL_QUADS);
    glColor4f(0.05f, 0.28f, 0.39f, 1.0f);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();

    drawOrganicWorldPatch(centerX - spanX * 0.28f, centerY + spanY * 0.12f,
                          spanX * 0.82f, spanY * 0.66f,
                          -0.38f, 0.30f, animationTime, 0.7f,
                          Color(0.03f, 0.23f, 0.35f, 0.20f),
                          Color(0.02f, 0.10f, 0.18f, 0.07f));
    drawOrganicWorldPatch(centerX - spanX * 0.40f, centerY + spanY * 0.40f,
                          spanX * 0.54f, spanY * 0.42f,
                          -0.82f, 0.34f, animationTime, 2.1f,
                          Color(0.02f, 0.15f, 0.24f, 0.18f),
                          Color(0.01f, 0.07f, 0.12f, 0.06f));
    drawOrganicWorldPatch(centerX + spanX * 0.26f, centerY - spanY * 0.22f,
                          spanX * 0.62f, spanY * 0.48f,
                          0.32f, 0.28f, animationTime, 3.8f,
                          Color(0.12f, 0.54f, 0.63f, 0.12f),
                          Color(0.24f, 0.72f, 0.76f, 0.04f));
    drawOrganicWorldPatch(centerX + spanX * 0.08f, centerY - spanY * 0.06f,
                          spanX * 0.30f, spanY * 0.20f,
                          -0.18f, 0.26f, animationTime, 5.1f,
                          Color(0.07f, 0.28f, 0.40f, 0.10f),
                          Color(0.02f, 0.10f, 0.18f, 0.03f));
    drawOrganicWorldPatch(centerX - spanX * 0.05f, centerY - spanY * 0.28f,
                          spanX * 0.44f, spanY * 0.22f,
                          0.54f, 0.24f, animationTime, 6.7f,
                          Color(0.18f, 0.58f, 0.64f, 0.08f),
                          Color(0.34f, 0.78f, 0.80f, 0.02f));
    drawOrganicWorldPatch(centerX + spanX * 0.34f, centerY + spanY * 0.26f,
                          spanX * 0.40f, spanY * 0.32f,
                          -0.62f, 0.30f, animationTime, 7.9f,
                          Color(0.02f, 0.12f, 0.20f, 0.12f),
                          Color(0.01f, 0.05f, 0.10f, 0.04f));

    for (int swell = 0; swell < 4; ++swell) {
        const float lane = -spanY * 0.36f + static_cast<float>(swell) * spanY * 0.24f;
        const float drift = std::sin(animationTime * (0.12f + swell * 0.03f) +
                                     static_cast<float>(swell) * 1.7f) * spanY * 0.05f;
        const float width = 1.2f + static_cast<float>(swell) * 0.24f;
        drawWorldSwellBand(centerX - spanX * 0.82f,
                           centerY + lane - spanY * 0.10f + drift,
                           centerX + spanX * 0.84f,
                           centerY + lane + spanY * 0.12f + drift * 0.5f,
                           width,
                           Color(0.02f, 0.12f, 0.18f, 0.040f - swell * 0.004f));
        drawWorldSwellBand(centerX - spanX * 0.72f,
                           centerY + lane - spanY * 0.02f + drift * 0.6f,
                           centerX + spanX * 0.74f,
                           centerY + lane + spanY * 0.06f + drift * 0.2f,
                           width * 0.42f,
                           Color(0.42f, 0.72f, 0.78f, 0.018f - swell * 0.002f));
    }

    auto drawRibbonFamily = [&](float dirX, float dirY, float spacing,
                                float halfWidth, float amplitude,
                                float frequency, float speed,
                                const Color& outerColor,
                                const Color& innerColor) {
        normalizeVector(dirX, dirY, 1.0f, 0.0f);
        const float perpX = -dirY;
        const float perpY = dirX;
        const float coverage = spanX * std::abs(perpX) + spanY * std::abs(perpY) + 12.0f;
        const float halfLength = spanX * std::abs(dirX) + spanY * std::abs(dirY) + 12.0f;

        for (float offset = -coverage; offset <= coverage; offset += spacing) {
            const float drift = std::sin(offset * 0.21f + animationTime * speed) * spacing * 0.16f;
            const float sweep = std::sin(offset * 0.12f - animationTime * speed * 0.58f + 0.9f) * 1.4f;
            const float startX = centerX + perpX * (offset + drift) - dirX * halfLength + dirX * sweep;
            const float startY = centerY + perpY * (offset + drift) - dirY * halfLength + dirY * sweep;
            const float endX = centerX + perpX * (offset + drift) + dirX * halfLength + dirX * sweep;
            const float endY = centerY + perpY * (offset + drift) + dirY * halfLength + dirY * sweep;
            const OceanWaveField wave = sampleOceanWaveField(centerX + perpX * offset,
                                                             centerY + perpY * offset,
                                                             animationTime);
            const float waveWidth = halfWidth * (0.90f + wave.crest * 0.24f);
            const float waveAmp = amplitude * (0.72f + wave.crest * 0.32f);
            const float phase = animationTime * speed + offset * 0.11f + wave.shimmer * 0.35f;

            drawWorldWaveRibbon(startX, startY, endX, endY,
                                waveWidth, waveAmp, frequency, phase,
                                withAlpha(outerColor, outerColor.a * (0.80f + wave.crest * 0.25f)));
            drawWorldWaveRibbon(startX, startY, endX, endY,
                                waveWidth * 0.38f, waveAmp * 0.52f, frequency + 0.65f,
                                phase + 0.55f,
                                withAlpha(innerColor, innerColor.a * (0.70f + wave.crest * 0.35f)));
        }
    };

    drawRibbonFamily(0.94f, 0.34f, 2.7f, 0.14f, 0.18f, 1.5f, 0.25f,
                     Color(0.16f, 0.52f, 0.64f, 0.040f),
                     Color(0.48f, 0.78f, 0.86f, 0.020f));
    drawRibbonFamily(-0.42f, 0.91f, 3.8f, 0.18f, 0.22f, 1.3f, 0.18f,
                     Color(0.08f, 0.30f, 0.42f, 0.030f),
                     Color(0.30f, 0.62f, 0.72f, 0.015f));
    drawRibbonFamily(0.72f, -0.62f, 5.2f, 0.08f, 0.12f, 1.8f, 0.31f,
                     Color(0.70f, 0.88f, 0.92f, 0.022f),
                     Color(0.92f, 0.98f, 1.0f, 0.010f));
    drawRibbonFamily(0.98f, 0.12f, 6.4f, 0.05f, 0.08f, 2.4f, 0.40f,
                     Color(0.78f, 0.92f, 0.96f, 0.015f),
                     Color(0.96f, 0.99f, 1.0f, 0.008f));

    if (currentRoute.visitOrder.size() > 1) {
        for (size_t i = 0; i + 1 < currentRoute.visitOrder.size(); ++i) {
            float fromX = 0.0f, fromY = 0.0f, toX = 0.0f, toY = 0.0f;
            if (!getRouteNodeWorldPos(graph, currentRoute.visitOrder[i], fromX, fromY) ||
                !getRouteNodeWorldPos(graph, currentRoute.visitOrder[i + 1], toX, toY)) {
                continue;
            }
            const float pulse = 0.5f + 0.5f *
                std::sin(animationTime * 0.72f + static_cast<float>(i) * 0.7f);
            drawWorldWaveRibbon(fromX, fromY, toX, toY,
                                0.34f, 0.08f, 1.2f,
                                animationTime * 0.60f + static_cast<float>(i) * 0.6f,
                                Color(0.18f, 0.52f, 0.58f, 0.018f + pulse * 0.010f));
        }
    }

    if (truck.isMoving() || currentRoute.isValid()) {
        drawOrganicWorldPatch(truck.getPosX(), truck.getPosY(),
                              1.1f, 0.72f, 0.62f, 0.24f,
                              animationTime, 8.4f,
                              Color(0.24f, 0.60f, 0.68f, truck.isMoving() ? 0.06f : 0.03f),
                              Color(0.70f, 0.90f, 0.96f, truck.isMoving() ? 0.02f : 0.01f));
    }

    for (const auto& node : graph.getNodes()) {
        const float capacityNorm = clamp01(node.getWasteCapacity() / 550.0f);
        const float tide = std::sin(animationTime * (0.52f + capacityNorm * 0.18f) +
                                    node.getWorldX() * 0.22f - node.getWorldY() * 0.15f);
        const float rotation = pseudoRandom01(node.getId(), 3, 881) * 2.0f *
                               kPi;
        const float shelfX = node.getIsHQ() ? 6.2f : 2.2f + capacityNorm * 0.95f;
        const float shelfY = node.getIsHQ() ? 4.6f : 1.6f + capacityNorm * 0.70f;
        const float driftX = tide * (node.getIsHQ() ? 0.26f : 0.12f);
        const float driftY = std::cos(animationTime * 0.34f + node.getId() * 0.8f) *
                             (node.getIsHQ() ? 0.20f : 0.08f);
        const float baseAlpha = node.getIsHQ()
            ? 0.10f
            : 0.045f + capacityNorm * 0.014f;

        drawOrganicWorldPatch(node.getWorldX() + driftX, node.getWorldY() + driftY,
                              shelfX * 1.38f, shelfY * 1.34f,
                              rotation, 0.32f, animationTime, node.getId() * 0.91f,
                              Color(0.08f, 0.34f, 0.42f, baseAlpha),
                              Color(0.14f, 0.48f, 0.54f, baseAlpha * 0.30f));
        drawOrganicWorldPatch(node.getWorldX() - driftX * 0.35f,
                              node.getWorldY() - driftY * 0.28f,
                              shelfX * 0.96f, shelfY * 0.88f,
                              rotation + 0.44f, 0.28f, animationTime, node.getId() * 1.27f,
                              Color(0.16f, 0.54f, 0.60f, baseAlpha * 0.78f),
                              Color(0.30f, 0.70f, 0.72f, baseAlpha * 0.24f));
        drawOrganicWorldPatch(node.getWorldX() + std::cos(rotation) * shelfX * 0.22f +
                                  driftX * 0.12f,
                              node.getWorldY() + std::sin(rotation) * shelfY * 0.18f +
                                  driftY * 0.16f,
                              shelfX * 0.74f, shelfY * 0.42f,
                              rotation - 0.60f, 0.22f, animationTime, node.getId() * 1.91f,
                              Color(0.48f, 0.76f, 0.76f, baseAlpha * 0.34f),
                              Color(0.70f, 0.90f, 0.88f, baseAlpha * 0.08f));

        if (!node.isCollected() || node.getIsHQ()) {
            drawOrganicWorldPatch(node.getWorldX() + driftX * 0.20f,
                                  node.getWorldY() + driftY * 0.14f,
                                  shelfX * 0.50f, shelfY * 0.44f,
                                  rotation - 0.26f, 0.24f, animationTime, node.getId() * 1.63f,
                                  Color(0.82f, 0.94f, 0.88f, baseAlpha * 0.50f),
                                  Color(0.60f, 0.82f, 0.78f, baseAlpha * 0.12f));
        }
    }

    activeGraph = nullptr;
}

// =====================================================================
//  Water tile
// =====================================================================

void SeaThemeRenderer::drawWaterTile(IsometricRenderer& renderer,
                                      float cx, float cy, float w, float h,
                                      int gx, int gy, float animationTime) {
    const float worldX = static_cast<float>(gx) * kWaterSampleSpacing;
    const float worldY = static_cast<float>(gy) * kWaterSampleSpacing;
    const OceanWaveField wave = sampleOceanWaveField(worldX, worldY, animationTime);
    const float surfaceOffset = sampleOceanSurfaceOffset(worldX, worldY, animationTime);

    float shelf = 0.0f;
    if (activeGraph) {
        for (const auto& node : activeGraph->getNodes()) {
            shelf = std::max(shelf, getNodeShelfInfluence(node, worldX, worldY));
        }
    }

    const float spanX = std::max(1.0f, sceneBounds.maxX - sceneBounds.minX);
    const float spanY = std::max(1.0f, sceneBounds.maxY - sceneBounds.minY);
    const float basinCenterX = RenderUtils::lerp(sceneBounds.minX, sceneBounds.maxX, 0.56f);
    const float basinCenterY = RenderUtils::lerp(sceneBounds.minY, sceneBounds.maxY, 0.46f);
    const float basinDx = (worldX - basinCenterX) / (spanX * 0.44f + 0.6f);
    const float basinDy = (worldY - basinCenterY) / (spanY * 0.38f + 0.6f);
    const float basin = clamp01(
        (1.0f - RenderUtils::smoothstep(
            clamp01(std::sqrt(basinDx * basinDx + basinDy * basinDy)))) * 0.82f + 0.28f *
        (1.0f - RenderUtils::smoothstep(clamp01(std::sqrt(
            ((worldX - RenderUtils::lerp(sceneBounds.minX, sceneBounds.maxX, 0.42f)) /
             (spanX * 0.62f + 0.5f)) *
            ((worldX - RenderUtils::lerp(sceneBounds.minX, sceneBounds.maxX, 0.42f)) /
             (spanX * 0.62f + 0.5f)) +
            ((worldY - RenderUtils::lerp(sceneBounds.minY, sceneBounds.maxY, 0.62f)) /
             (spanY * 0.54f + 0.5f)) *
            ((worldY - RenderUtils::lerp(sceneBounds.minY, sceneBounds.maxY, 0.62f)) /
             (spanY * 0.54f + 0.5f)))))));

    const Color reflection = mixColor(Color(0.12f, 0.46f, 0.54f, 0.0f),
                                      Color(0.88f, 0.97f, 1.0f, 0.0f),
                                      0.42f + basin * 0.18f + shelf * 0.12f);
    const float baseAlpha = 0.010f + basin * 0.014f + shelf * 0.012f;

    renderer.drawDiamond(cx + wave.primary * w * 0.012f,
                cy - h * 0.10f + surfaceOffset * h * 0.12f,
                w * (0.82f + basin * 0.10f),
                h * 0.12f,
                withAlpha(reflection, baseAlpha));
    renderer.drawDiamond(cx - wave.secondary * w * 0.010f,
                cy + h * 0.02f + surfaceOffset * h * 0.08f,
                w * (0.56f + shelf * 0.20f),
                h * 0.10f,
                Color(0.78f, 0.93f, 0.94f, baseAlpha * (0.8f + wave.crest * 0.8f)));
}

// =====================================================================
//  Waste node â€” Boom Beach island + garbage patch
// =====================================================================

