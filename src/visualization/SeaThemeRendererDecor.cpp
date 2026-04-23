#include "visualization/SeaThemeRendererInternal.h"

void SeaThemeRenderer::drawDecorativeElements(IsometricRenderer& renderer,
                                               const MapGraph& graph,
                                               float animationTime) {
    drawDecorativeIslets(renderer, graph, animationTime);
}

void SeaThemeRenderer::drawDecorativeIslets(IsometricRenderer& renderer,
                                             const MapGraph& graph,
                                             float animationTime) {
    updateSceneBounds(graph);

    auto drawLargeCoastalIsland = [&](float wx, float wy, float scaleBias,
                                      bool lushNorth, float seedPhase) {
        const IsoCoord iso = RenderUtils::worldToIso(wx, wy);
        const float tide = std::sin(animationTime * 0.42f + seedPhase) * 0.6f;
        const float zf = RenderUtils::getProjection().tileWidth / RenderUtils::BASE_TILE_WIDTH;
        const float s = scaleBias * 2.35f * zf;
        const Color shadow(0.02f, 0.05f, 0.12f, 0.20f);
        const Color beachTop(0.87f, 0.77f, 0.56f, 1.0f);
        const Color beachTopLight(0.93f, 0.84f, 0.64f, 1.0f);
        const Color cliffFace(0.70f, 0.56f, 0.38f, 1.0f);
        const Color cliffDark(0.50f, 0.38f, 0.24f, 1.0f);
        const Color canopyA(0.36f, 0.77f, 0.18f, 1.0f);
        const Color canopyB(0.28f, 0.68f, 0.14f, 1.0f);
        const Color canopyC(0.42f, 0.84f, 0.24f, 1.0f);
        const Color trunk(0.46f, 0.32f, 0.18f, 1.0f);
        const Color rockTopC(0.62f, 0.54f, 0.40f, 1.0f);
        const Color rockSideC(0.42f, 0.35f, 0.24f, 1.0f);
        const float bob = std::sin(animationTime * 0.85f + seedPhase) * 0.25f * s;

        renderer.drawDiamond(iso.x - 2.0f * s, iso.y + 12.0f * s + bob, 28.0f * s, 12.0f * s, shadow);
        renderer.drawIsometricBlock(iso.x - 8.0f * s, iso.y + 3.8f * s + bob * 0.2f,
                           22.0f * s, 13.2f * s, 4.0f * s, beachTop, cliffDark);
        renderer.drawIsometricBlock(iso.x - 20.5f * s, iso.y + 1.2f * s + bob * 0.15f,
                           14.4f * s, 9.4f * s, 3.3f * s,
                           shiftColor(beachTop, -0.02f), shiftColor(cliffDark, -0.02f));
        renderer.drawIsometricBlock(iso.x + 10.5f * s, iso.y + 0.8f * s + bob * 0.10f,
                           11.0f * s, 7.2f * s, 2.6f * s, beachTopLight, cliffFace);
        renderer.drawIsometricBlock(iso.x - 2.0f * s, iso.y - 0.2f * s + bob * 0.08f,
                           10.8f * s, 6.0f * s, 1.8f * s, beachTopLight, cliffFace);

        renderer.drawDiamond(iso.x - 2.8f * s, iso.y + 5.4f * s + bob * 0.10f, 7.4f * s, 3.0f * s, beachTopLight);
        renderer.drawLine(iso.x - 8.4f * s, iso.y + 5.4f * s + bob * 0.10f,
                 iso.x - 2.8f * s, iso.y + 7.3f * s + bob * 0.10f,
                 Color(1.0f, 1.0f, 1.0f, 0.86f), 1.6f);
        renderer.drawLine(iso.x - 2.8f * s, iso.y + 7.3f * s + bob * 0.10f,
                 iso.x + 3.2f * s, iso.y + 5.0f * s + bob * 0.10f,
                 Color(1.0f, 1.0f, 1.0f, 0.82f), 1.5f);

        const std::array<IsoCoord, 7> rocks = {{
            {iso.x - 22.0f * s, iso.y + 4.0f * s},
            {iso.x - 16.0f * s, iso.y + 7.0f * s},
            {iso.x - 10.0f * s, iso.y + 8.2f * s},
            {iso.x + 3.0f * s, iso.y + 7.6f * s},
            {iso.x + 11.0f * s, iso.y + 5.8f * s},
            {iso.x + 17.0f * s, iso.y + 2.2f * s},
            {iso.x + 6.0f * s, iso.y + 0.6f * s}
        }};
        for (size_t i = 0; i < rocks.size(); ++i) {
            const float rockSize = (i % 2 == 0 ? 2.2f : 1.7f) * s;
            renderer.drawIsometricBlock(rocks[i].x, rocks[i].y + bob * 0.06f,
                               rockSize, rockSize * 0.68f, 0.9f * s, rockTopC, rockSideC);
        }

        const std::array<IsoCoord, 14> canopyCenters = lushNorth
            ? std::array<IsoCoord, 14>{{
                {iso.x - 20.0f * s, iso.y - 4.5f * s}, {iso.x - 14.5f * s, iso.y - 7.0f * s},
                {iso.x - 8.5f * s, iso.y - 8.8f * s}, {iso.x - 2.0f * s, iso.y - 9.6f * s},
                {iso.x + 4.8f * s, iso.y - 9.2f * s}, {iso.x + 10.5f * s, iso.y - 7.6f * s},
                {iso.x + 15.0f * s, iso.y - 4.8f * s}, {iso.x - 17.0f * s, iso.y - 1.0f * s},
                {iso.x - 10.5f * s, iso.y - 2.4f * s}, {iso.x - 4.0f * s, iso.y - 2.8f * s},
                {iso.x + 2.5f * s, iso.y - 2.2f * s}, {iso.x + 8.5f * s, iso.y - 1.4f * s},
                {iso.x - 7.0f * s, iso.y - 13.0f * s}, {iso.x + 1.0f * s, iso.y - 12.0f * s}
            }}
            : std::array<IsoCoord, 14>{{
                {iso.x - 19.0f * s, iso.y - 5.0f * s}, {iso.x - 13.0f * s, iso.y - 7.4f * s},
                {iso.x - 7.2f * s, iso.y - 9.0f * s}, {iso.x - 1.2f * s, iso.y - 9.5f * s},
                {iso.x + 5.2f * s, iso.y - 8.8f * s}, {iso.x + 11.0f * s, iso.y - 6.6f * s},
                {iso.x + 15.5f * s, iso.y - 3.2f * s}, {iso.x - 15.5f * s, iso.y - 0.8f * s},
                {iso.x - 9.0f * s, iso.y - 2.6f * s}, {iso.x - 2.5f * s, iso.y - 3.0f * s},
                {iso.x + 3.6f * s, iso.y - 2.4f * s}, {iso.x + 9.5f * s, iso.y - 1.6f * s},
                {iso.x - 5.5f * s, iso.y - 12.5f * s}, {iso.x + 2.4f * s, iso.y - 11.5f * s}
            }};
        const std::array<float, 14> canopyRadii = {{
            6.2f, 6.6f, 7.0f, 7.3f, 7.0f, 6.4f, 5.8f,
            5.9f, 6.3f, 6.2f, 5.8f, 5.2f, 4.8f, 4.4f
        }};

        for (size_t i = 0; i < canopyCenters.size(); ++i) {
            const Color canopy = (i % 3 == 0) ? canopyA : ((i % 3 == 1) ? canopyB : canopyC);
            renderer.drawFilledCircle(canopyCenters[i].x, canopyCenters[i].y + bob * 0.08f,
                             canopyRadii[i] * s, canopy);
        }

        for (int palm = 0; palm < 5; ++palm) {
            const float px = (-17.0f + palm * 8.8f) * s;
            const float py = (-1.0f - (palm % 2) * 2.2f) * s;
            renderer.drawLine(iso.x + px, iso.y + py, iso.x + px + 0.5f * s, iso.y + py - 6.2f * s, trunk, 1.1f);
            renderer.drawFilledCircle(iso.x + px - 1.2f * s, iso.y + py - 6.5f * s, 2.2f * s, canopyC);
            renderer.drawFilledCircle(iso.x + px + 1.2f * s, iso.y + py - 6.9f * s, 2.0f * s, canopyB);
        }
    };

    auto drawAmbientIslet = [&](float wx, float wy, int seed, float scaleBias,
                                int variantOverride = -1, float minNodeDistance = 4.8f) {
        float minDist = 1e9f;
        for (const auto& node : graph.getNodes()) {
            const float dx = wx - node.getWorldX();
            const float dy = wy - node.getWorldY();
            minDist = std::min(minDist, std::sqrt(dx * dx + dy * dy));
        }
        if (minDist < minNodeDistance) return;

        const IsoCoord iso = RenderUtils::worldToIso(wx, wy);
        const float zf = RenderUtils::getProjection().tileWidth / RenderUtils::BASE_TILE_WIDTH;
        const float isletScale = (0.90f + pseudoRandom01(seed, 2, 777) * 0.95f) * scaleBias * zf;
        const float tide = std::sin(animationTime * 0.45f + seed * 1.4f) * 0.4f;
        const int variant = (variantOverride >= 0)
            ? variantOverride
            : positiveMod(seed + static_cast<int>(pseudoRandom01(seed, 0, 811) * 9.0f), 5);

        renderer.drawDiamond(iso.x, iso.y + 3.5f * isletScale + tide * 0.15f,
                    9.0f * isletScale, 4.5f * isletScale, Color(0.02f, 0.06f, 0.14f, 0.20f));

        if (variant == 0) {
            const Color sandTop(0.74f, 0.66f, 0.48f, 0.80f);
            const Color sandSide(0.54f, 0.46f, 0.32f, 0.75f);
            renderer.drawIsometricBlock(iso.x, iso.y + 1.5f + tide * 0.1f,
                               7.0f * isletScale, 4.5f * isletScale, 1.8f * isletScale, sandTop, sandSide);
            renderer.drawFilledCircle(iso.x + 0.8f * isletScale, iso.y - 1.5f * isletScale + tide * 0.1f,
                             1.5f * isletScale, Color(0.20f, 0.50f, 0.28f, 0.68f));
        } else if (variant == 1) {
            const Color rockTopV(0.42f, 0.40f, 0.38f, 0.82f);
            const Color rockSideV(0.28f, 0.26f, 0.24f, 0.78f);
            renderer.drawIsometricBlock(iso.x, iso.y + 1.0f + tide * 0.08f,
                               5.0f * isletScale, 3.5f * isletScale, 2.5f * isletScale, rockTopV, rockSideV);
            renderer.drawFilledCircle(iso.x - 0.8f * isletScale, iso.y - 1.0f * isletScale,
                             0.9f * isletScale, Color(0.18f, 0.40f, 0.25f, 0.32f));
        } else if (variant == 2) {
            renderer.drawDiamond(iso.x, iso.y + 1.0f + tide * 0.12f,
                        8.0f * isletScale, 4.0f * isletScale, Color(0.08f, 0.30f, 0.40f, 0.35f));
            renderer.drawDiamond(iso.x, iso.y + 0.5f + tide * 0.1f,
                        5.5f * isletScale, 2.8f * isletScale, Color(0.12f, 0.38f, 0.44f, 0.28f));
        } else if (variant == 3) {
            renderer.drawDiamond(iso.x, iso.y + 0.8f + tide * 0.12f,
                        7.8f * isletScale, 3.8f * isletScale, Color(0.10f, 0.28f, 0.36f, 0.28f));
            renderer.drawDiamondOutline(iso.x, iso.y + 0.8f + tide * 0.10f,
                               8.8f * isletScale, 4.4f * isletScale, Color(0.70f, 0.90f, 0.92f, 0.08f), 0.9f);
            for (int buoy = 0; buoy < 3; buoy++) {
                const float angle = static_cast<float>(buoy) * 2.094f;
                const float bx = iso.x + std::cos(angle) * 4.0f * isletScale;
                const float by = iso.y + std::sin(angle) * 2.0f * isletScale;
                renderer.drawLine(bx, by + 2.0f * isletScale, bx, by - 0.3f * isletScale,
                         Color(0.76f, 0.78f, 0.72f, 0.52f), 0.9f);
                renderer.drawFilledCircle(bx, by - 0.5f * isletScale, 0.45f * isletScale,
                                 Color(0.92f, 0.64f, 0.24f, 0.58f));
            }
        } else {
            renderer.drawDiamond(iso.x + 0.4f * isletScale, iso.y + 1.4f * isletScale + tide * 0.12f,
                        9.4f * isletScale, 4.4f * isletScale, Color(0.08f, 0.26f, 0.34f, 0.22f));
            renderer.drawLine(iso.x - 5.4f * isletScale, iso.y - 1.6f * isletScale,
                     iso.x + 5.8f * isletScale, iso.y + 2.2f * isletScale,
                     Color(0.80f, 0.92f, 0.88f, 0.22f), 1.0f);
            renderer.drawLine(iso.x - 2.2f * isletScale, iso.y + 3.4f * isletScale,
                     iso.x + 3.8f * isletScale, iso.y + 1.0f * isletScale,
                     Color(0.80f, 0.92f, 0.88f, 0.18f), 1.0f);
            renderer.drawLine(iso.x - 4.6f * isletScale, iso.y - 3.0f * isletScale,
                     iso.x - 4.6f * isletScale, iso.y - 7.0f * isletScale,
                     Color(0.78f, 0.78f, 0.72f, 0.62f), 0.9f);
            renderer.drawFilledCircle(iso.x - 4.6f * isletScale, iso.y - 7.4f * isletScale,
                             0.55f * isletScale, Color(0.96f, 0.66f, 0.22f, 0.62f));
        }

        const float foamPulse = 0.5f + 0.5f * std::sin(animationTime * 0.9f + seed * 0.7f);
        renderer.drawDiamondOutline(iso.x, iso.y + 2.2f * isletScale + tide * 0.08f,
                           9.5f * isletScale * (0.97f + foamPulse * 0.03f),
                           4.8f * isletScale * (0.97f + foamPulse * 0.03f),
                           Color(0.86f, 0.96f, 1.0f, 0.06f + foamPulse * 0.05f), 1.0f);
    };

    for (int i = 0; i < kDecorIsletCount; i++) {
        const float rx = pseudoRandom01(i, 0, 777);
        const float ry = pseudoRandom01(i, 1, 777);
        const float wx = RenderUtils::lerp(sceneBounds.minX - 0.8f, sceneBounds.maxX + 0.8f, rx);
        const float wy = RenderUtils::lerp(sceneBounds.minY - 0.8f, sceneBounds.maxY + 0.8f, ry);
        drawAmbientIslet(wx, wy, i, 1.0f);
    }

    auto drawIsletNearNode = [&](int nodeId, float offsetX, float offsetY,
                                 float scaleBias, int seed, int variantOverride, float minNodeDistance) {
        const int idx = graph.findNodeIndex(nodeId);
        if (idx < 0) return;
        const WasteNode& node = graph.getNode(idx);
        drawAmbientIslet(node.getWorldX() + offsetX, node.getWorldY() + offsetY,
                         seed, scaleBias, variantOverride, minNodeDistance);
    };

    auto drawLargeIslandNearNode = [&](int nodeId, float offsetX, float offsetY,
                                       float scaleBias, bool lushNorth, float seedPhase) {
        const int idx = graph.findNodeIndex(nodeId);
        if (idx < 0) return;
        const WasteNode& node = graph.getNode(idx);
        drawLargeCoastalIsland(node.getWorldX() + offsetX, node.getWorldY() + offsetY,
                               scaleBias, lushNorth, seedPhase);
    };

    drawLargeIslandNearNode(1, -7.8f, -2.4f, 1.12f, true, 21.0f);
    drawLargeIslandNearNode(9, -7.2f,  2.8f, 1.24f, false, 24.0f);

    // Suppress unused lambda warnings
    (void)drawIsletNearNode;
}

// =====================================================================
//  Atmospheric effects
// =====================================================================

void SeaThemeRenderer::drawAtmosphericEffects(IsometricRenderer& renderer,
                                               const MapGraph& graph,
                                               float animationTime) {
    updateSceneBounds(graph);
    const float centerX = (sceneBounds.minX + sceneBounds.maxX) * 0.5f;
    const float centerY = (sceneBounds.minY + sceneBounds.maxY) * 0.5f;
    const IsoCoord centerIso = RenderUtils::worldToIso(centerX, centerY);
    const float paddedMinX = sceneBounds.minX - kFieldPadding;
    const float paddedMaxX = sceneBounds.maxX + kFieldPadding;
    const float paddedMinY = sceneBounds.minY - kFieldPadding;
    const float paddedMaxY = sceneBounds.maxY + kFieldPadding;

    for (int bird = 0; bird < 5; bird++) {
        const float orbitSpeed = 0.18f + bird * 0.04f;
        const float orbitRadius = 80.0f + bird * 35.0f;
        const float heightOffset = -60.0f - bird * 20.0f;
        const float angle = animationTime * orbitSpeed + bird * 1.26f;
        const float bx = centerIso.x + std::cos(angle) * orbitRadius;
        const float by = centerIso.y + std::sin(angle * 0.7f) * (orbitRadius * 0.4f) + heightOffset;
        const float wingFlap = std::sin(animationTime * 5.0f + bird * 2.0f);
        const float wingSpan = 2.5f + bird * 0.3f;
        const float wingY = wingFlap * 0.8f;
        const float alpha = 0.35f + 0.15f * std::sin(animationTime * 0.3f + bird);
        renderer.drawLine(bx - wingSpan, by + wingY, bx, by - 0.6f, Color(0.15f, 0.15f, 0.18f, alpha), 1.0f);
        renderer.drawLine(bx, by - 0.6f, bx + wingSpan, by + wingY, Color(0.15f, 0.15f, 0.18f, alpha), 1.0f);
    }

    for (const auto& node : graph.getNodes()) {
        if (node.isCollected() && !node.getIsHQ()) continue;
        const IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
        const float nodeScale = node.getIsHQ() ? 1.8f : 1.0f;
        const float foamPhase = animationTime * 0.6f + node.getWorldX() * 0.3f;
        const float foamAlpha = 0.025f + 0.02f * std::sin(foamPhase * 1.3f);
        for (int fm = 0; fm < 3; fm++) {
            const float localPhase = foamPhase + fm * 2.1f;
            const float ox = std::cos(localPhase) * (12.0f + fm * 3.0f) * nodeScale;
            const float oy = std::sin(localPhase * 1.3f) * (4.0f + fm * 1.1f) * nodeScale;
            renderer.drawLine(iso.x + ox - 3.2f * nodeScale, iso.y + oy + 5.4f * nodeScale,
                     iso.x + ox + 3.6f * nodeScale, iso.y + oy + 6.6f * nodeScale,
                     Color(0.80f, 0.94f, 1.0f, foamAlpha * (1.0f - fm * 0.18f)), 1.0f);
        }
    }

    for (int marker = 0; marker < 4; ++marker) {
        const float mx = RenderUtils::lerp(paddedMinX + 1.2f, paddedMaxX - 1.4f,
                                           pseudoRandom01(marker, 4, 933));
        const float my = RenderUtils::lerp(paddedMinY + 1.0f, paddedMaxY - 1.2f,
                                           pseudoRandom01(marker, 5, 933));
        float minDist = 1.0e9f;
        for (const auto& node : graph.getNodes()) {
            const float dx = mx - node.getWorldX();
            const float dy = my - node.getWorldY();
            minDist = std::min(minDist, std::sqrt(dx * dx + dy * dy));
        }
        if (minDist < 3.2f) continue;
        const IsoCoord mIso = RenderUtils::worldToIso(mx, my);
        const float mBob = std::sin(animationTime * 1.4f + marker * 2.3f) * 0.6f;
        renderer.drawDiamond(mIso.x, mIso.y + mBob + 2.0f, 3.6f, 1.4f, Color(0.62f, 0.88f, 0.94f, 0.05f));
    }

    for (int edge = 0; edge < 3; edge++) {
        const float wy = paddedMaxY + 2.5f + edge * 0.4f;
        const float waveShift = std::sin(animationTime * (0.15f + edge * 0.04f) + edge) * 0.5f;
        const IsoCoord a = RenderUtils::worldToIso(paddedMinX - 1.0f, wy + waveShift);
        const IsoCoord b = RenderUtils::worldToIso(paddedMaxX + 1.0f, wy + waveShift);
        renderer.drawLine(a.x, a.y, b.x, b.y, Color(0.50f, 0.80f, 0.92f, 0.03f + edge * 0.01f), 1.0f);
    }

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const float left = static_cast<float>(viewport[0]);
    const float top = static_cast<float>(viewport[1]);
    const float right = left + static_cast<float>(viewport[2]);
    const float bottom = top + static_cast<float>(viewport[3]);

    if (!graph.getNodes().empty()) {
        const WasteNode& hq = graph.getHQNode();
        drawOrganicWorldPatch(hq.getWorldX() + 0.6f, hq.getWorldY() + 0.8f,
                              5.0f, 3.8f, 0.28f, 0.24f, animationTime, 10.8f,
                              Color(0.98f, 0.93f, 0.78f, 0.045f), Color(0.80f, 0.84f, 0.76f, 0.012f));
        drawOrganicWorldPatch(hq.getWorldX() + 1.4f, hq.getWorldY() - 0.8f,
                              7.4f, 4.8f, -0.22f, 0.20f, animationTime, 14.1f,
                              Color(0.34f, 0.74f, 0.84f, 0.026f), Color(0.18f, 0.42f, 0.52f, 0.008f));
        drawOrganicWorldPatch(centerX, centerY,
                              (paddedMaxX - paddedMinX) * 0.20f, (paddedMaxY - paddedMinY) * 0.14f,
                              -0.16f, 0.18f, animationTime, 12.6f,
                              Color(0.56f, 0.78f, 0.84f, 0.024f), Color(0.34f, 0.58f, 0.68f, 0.010f));
    }

    glBegin(GL_QUADS);
    glColor4f(0.01f, 0.03f, 0.08f, 0.12f); glVertex2f(left, top);
    glColor4f(0.01f, 0.03f, 0.08f, 0.00f); glVertex2f(right, top);
    glColor4f(0.01f, 0.03f, 0.08f, 0.00f); glVertex2f(right, top + 140.0f);
    glColor4f(0.01f, 0.03f, 0.08f, 0.08f); glVertex2f(left, top + 140.0f);
    glEnd();

    glBegin(GL_QUADS);
    glColor4f(0.01f, 0.03f, 0.08f, 0.00f); glVertex2f(left, bottom - 120.0f);
    glColor4f(0.01f, 0.03f, 0.08f, 0.00f); glVertex2f(right, bottom - 120.0f);
    glColor4f(0.01f, 0.03f, 0.08f, 0.10f); glVertex2f(right, bottom);
    glColor4f(0.01f, 0.03f, 0.08f, 0.12f); glVertex2f(left, bottom);
    glEnd();

    drawWorldQuadPatch(paddedMinX - 4.5f, paddedMinY - 4.5f,
                       paddedMinX + 0.4f, paddedMaxY + 4.5f,
                       Color(0.01f, 0.03f, 0.08f, 0.07f));
    drawWorldQuadPatch(paddedMaxX - 0.4f, paddedMinY - 4.5f,
                       paddedMaxX + 4.5f, paddedMaxY + 4.5f,
                       Color(0.01f, 0.03f, 0.08f, 0.09f));
}
