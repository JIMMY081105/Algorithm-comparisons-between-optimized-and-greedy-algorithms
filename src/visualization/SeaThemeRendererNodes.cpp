#include "SeaThemeRendererInternal.h"

void SeaThemeRenderer::drawWasteNode(IsometricRenderer& renderer,
                                      const WasteNode& node,
                                      float animationTime) {
    IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
    const GarbageSizeTier sizeTier = getGarbageSizeTier(node.getWasteCapacity());
    const float zf = RenderUtils::getProjection().tileWidth / RenderUtils::BASE_TILE_WIDTH;
    const float scale = getGarbageScale(sizeTier) * 1.08f * zf;
    const float islandS = scale * 0.24f;
    const float markerS = scale * 0.42f;
    const float s = islandS;
    const float capacityNorm = clamp01(node.getWasteCapacity() / 550.0f);
    const int seed = node.getId();
    Color bodyColor = getGarbageSizeColor(sizeTier);
    Color accentColor = RenderUtils::getUrgencyColor(node.getUrgency());

    if (node.isCollected()) {
        bodyColor = mixColor(bodyColor, RenderUtils::getCollectedColor(), 0.68f);
        accentColor = shiftColor(RenderUtils::getCollectedColor(), 0.12f, 0.90f);
    } else if (node.getUrgency() == UrgencyLevel::HIGH) {
        const float pulse = RenderUtils::getUrgencyPulse(animationTime);
        accentColor.r = clampColor(accentColor.r * pulse);
        accentColor.g = clampColor(accentColor.g * pulse);
        accentColor.b = clampColor(accentColor.b * pulse);
    }

    const float bob = std::sin(animationTime * 1.0f + node.getWorldX() * 0.45f +
                               node.getWorldY() * 0.28f) * 1.0f;
    currentGarbageSinkProgress = getGarbageSinkState(node.getId());

    const float ix = iso.x - 12.0f * markerS;
    const float iy = iso.y - 36.0f * markerS;

    // ---- Boom Beach Island Palette ----
    const Color islandShadow(0.02f, 0.05f, 0.12f, node.isCollected() ? 0.28f : 0.22f);
    const Color beachTop(0.86f, 0.77f, 0.56f, 1.0f);
    const Color beachLight(0.93f, 0.84f, 0.64f, 1.0f);
    const Color cliffFace(0.70f, 0.56f, 0.38f, 1.0f);
    const Color cliffDark(0.50f, 0.38f, 0.24f, 1.0f);
    const Color canopyA(0.18f, 0.52f, 0.08f, 1.0f);
    const Color canopyB(0.28f, 0.66f, 0.12f, 1.0f);
    const Color canopyC(0.38f, 0.78f, 0.18f, 1.0f);
    const Color canopyD(0.46f, 0.84f, 0.24f, 1.0f);
    const Color trunkColor(0.46f, 0.32f, 0.18f, 1.0f);
    const Color rockTop(0.62f, 0.54f, 0.40f, 1.0f);
    const Color rockSide(0.42f, 0.35f, 0.24f, 1.0f);

    renderer.drawDiamond(ix, iy + 11.0f * s + bob * 0.16f,
                30.0f * s, 13.0f * s, islandShadow);

    {
        const int variant = positiveMod(seed * 7 + 3, 4);
        const float jx = pseudoRandomSigned(seed, 0, 101) * 0.5f;

        renderer.drawIsometricBlock(ix + jx * s, iy + 3.8f * s + bob * 0.10f,
                           22.0f * s, 13.0f * s, 4.0f * s, beachTop, cliffDark);

        if (variant == 0 || variant == 2) {
            renderer.drawIsometricBlock(ix + (jx - 10.0f) * s, iy + 2.0f * s + bob * 0.08f,
                               13.0f * s, 8.5f * s, 3.2f * s,
                               shiftColor(beachTop, -0.02f), shiftColor(cliffDark, -0.02f));
        }
        if (variant == 1 || variant == 3) {
            renderer.drawIsometricBlock(ix + (jx + 10.0f) * s, iy + 1.4f * s + bob * 0.08f,
                               12.0f * s, 7.5f * s, 2.8f * s, beachLight, cliffFace);
        }
        if (variant == 0 || variant == 1) {
            renderer.drawIsometricBlock(ix + (jx + 7.5f) * s, iy + 2.8f * s + bob * 0.07f,
                               9.0f * s, 6.0f * s, 2.2f * s, beachLight, cliffFace);
        } else {
            renderer.drawIsometricBlock(ix + (jx - 7.5f) * s, iy + 2.4f * s + bob * 0.07f,
                               10.0f * s, 6.5f * s, 2.4f * s,
                               shiftColor(beachTop, 0.02f), shiftColor(cliffDark, 0.02f));
        }
        renderer.drawIsometricBlock(ix + jx * 0.3f * s, iy + 0.2f * s + bob * 0.06f,
                           11.0f * s, 6.0f * s, 2.0f * s, beachLight, cliffFace);

        renderer.drawDiamond(ix + jx * 0.2f * s, iy + 5.6f * s + bob * 0.10f,
                    7.5f * s, 3.2f * s, beachLight);
        renderer.drawLine(ix - 8.5f * s, iy + 5.6f * s + bob * 0.10f,
                 ix - 1.5f * s, iy + 7.5f * s + bob * 0.10f,
                 Color(1.0f, 1.0f, 1.0f, 0.84f), 1.6f);
        renderer.drawLine(ix - 1.5f * s, iy + 7.5f * s + bob * 0.10f,
                 ix + 6.5f * s, iy + 5.0f * s + bob * 0.10f,
                 Color(1.0f, 1.0f, 1.0f, 0.78f), 1.4f);

        const int rockCount = 4 + static_cast<int>(capacityNorm * 4.0f);
        for (int r = 0; r < rockCount; ++r) {
            const float ang = static_cast<float>(r) * 6.283f /
                              static_cast<float>(rockCount) +
                              pseudoRandom01(seed, r, 333) * 0.5f;
            const float dist = (10.0f + pseudoRandom01(seed, r, 444) * 3.5f) * s;
            const float rx = ix + std::cos(ang) * dist * 0.80f;
            const float ry = iy + std::sin(ang) * dist * 0.40f + 4.0f * s;
            const float rSize = (1.6f + pseudoRandom01(seed, r, 555) * 1.2f) * s;
            renderer.drawIsometricBlock(rx, ry + bob * 0.06f,
                               rSize, rSize * 0.65f, 0.9f * s,
                               rockTop, rockSide);
        }

        const int backCount = 9 + static_cast<int>(capacityNorm * 5.0f);
        for (int c = 0; c < backCount; ++c) {
            const float t = static_cast<float>(c) / static_cast<float>(backCount);
            const float ang = t * 6.283f + pseudoRandom01(seed, c, 666) * 1.3f;
            const float radX = (4.5f + pseudoRandom01(seed, c, 777) * 6.0f) * s;
            const float radY = (2.2f + pseudoRandom01(seed, c, 888) * 3.5f) * s;
            const float cx = ix + std::cos(ang) * radX * 0.58f + jx * s * 0.15f;
            const float cy = iy + std::sin(ang) * radY * 0.38f - 5.0f * s + bob * 0.08f;
            const float radius = (3.2f + pseudoRandom01(seed, c, 999) * 3.2f) * s;
            const Color& canopy = (c % 4 == 0) ? canopyA :
                                  (c % 4 == 1) ? canopyB :
                                  (c % 4 == 2) ? canopyC : canopyD;
            renderer.drawFilledCircle(cx, cy, radius, canopy);
        }
        const int frontCount = backCount / 2 + 1;
        for (int c = 0; c < frontCount; ++c) {
            const float t = static_cast<float>(c) / static_cast<float>(frontCount);
            const float ang = t * 6.283f + pseudoRandom01(seed, c + 50, 612) * 1.5f;
            const float radX = (3.0f + pseudoRandom01(seed, c + 50, 723) * 4.5f) * s;
            const float radY = (1.4f + pseudoRandom01(seed, c + 50, 834) * 2.2f) * s;
            const float cx = ix + std::cos(ang) * radX * 0.50f + jx * s * 0.10f;
            const float cy = iy + std::sin(ang) * radY * 0.32f - 3.5f * s + bob * 0.08f;
            const float radius = (2.8f + pseudoRandom01(seed, c + 50, 945) * 2.8f) * s;
            const Color& canopy = ((c + 1) % 4 == 0) ? canopyA :
                                  ((c + 1) % 4 == 1) ? canopyB :
                                  ((c + 1) % 4 == 2) ? canopyC : canopyD;
            renderer.drawFilledCircle(cx, cy, radius, shiftColor(canopy, 0.04f));
        }

        const int palmCount = 2 + static_cast<int>(capacityNorm * 3.0f);
        for (int p = 0; p < palmCount; ++p) {
            const float spacing = 15.0f / static_cast<float>(palmCount);
            const float px = (-7.5f + static_cast<float>(p) * spacing +
                              pseudoRandomSigned(seed, p, 111) * 1.5f) * s;
            const float py = (-1.5f + pseudoRandomSigned(seed, p, 222) * 1.5f) * s;
            renderer.drawLine(ix + px, iy + py,
                     ix + px + 0.4f * s, iy + py - 6.5f * s,
                     trunkColor, 1.2f);
            renderer.drawFilledCircle(ix + px - 1.3f * s, iy + py - 6.8f * s,
                             2.2f * s, canopyC);
            renderer.drawFilledCircle(ix + px + 1.3f * s, iy + py - 7.2f * s,
                             2.0f * s, canopyB);
        }

        const float foamPulse = 0.5f + 0.5f *
            std::sin(animationTime * 0.8f + static_cast<float>(seed) * 0.7f);
        renderer.drawDiamondOutline(ix, iy + 9.0f * s,
                           31.0f * s * (0.98f + foamPulse * 0.02f),
                           13.5f * s * (0.98f + foamPulse * 0.02f),
                           Color(0.86f, 0.96f, 1.0f, 0.08f + foamPulse * 0.05f), 1.5f);
    }

    // ---- Garbage patch sprite ----
    const float fillRatio = node.isCollected()
        ? 0.0f
        : clamp01(node.getWasteLevel() / 100.0f);
    const float patchX = iso.x;
    const float patchY = iso.y - 2.0f + bob;
    const float patchScale = scale * 1.14f;
    drawGarbagePatchSprite(renderer, patchX, patchY, patchScale, bodyColor, accentColor,
                           node.isCollected(), fillRatio, animationTime);

    if (node.isCollected()) {
        const float settle = 0.4f + 0.6f * currentGarbageSinkProgress;
        renderer.drawDiamondOutline(iso.x, iso.y + 4.0f * markerS,
                           22.0f * markerS * (0.96f + settle * 0.04f),
                           10.0f * markerS * (0.96f + settle * 0.04f),
                           Color(0.72f, 0.96f, 0.86f, 0.12f + settle * 0.06f), 1.1f);
    }

    if (!node.isCollected()) {
        if (node.getUrgency() != UrgencyLevel::LOW) {
            float ringRadius = 16.0f * markerS;
            float ringHeight = 7.0f * markerS;
            float thickness = 1.2f;
            if (node.getUrgency() == UrgencyLevel::HIGH) {
                ringRadius += 1.5f * markerS * std::sin(animationTime * 2.4f);
                ringHeight += 0.7f * markerS * std::sin(animationTime * 2.4f);
                thickness = 1.6f;
            }
            Color ringColor = accentColor;
            ringColor.a = (node.getUrgency() == UrgencyLevel::HIGH) ? 0.22f : 0.13f;
            renderer.drawDiamondOutline(iso.x, iso.y + 2.0f * markerS + bob * 0.18f,
                               ringRadius, ringHeight, ringColor, thickness);
            const float braceX = ringRadius + 1.5f * markerS;
            const float braceY = ringHeight + 0.8f * markerS;
            renderer.drawLine(iso.x - braceX, iso.y + 2.0f * markerS + bob * 0.18f,
                     iso.x - braceX + 2.5f * markerS,
                     iso.y + 2.0f * markerS - braceY + bob * 0.18f,
                     withAlpha(ringColor, ringColor.a * 0.8f), 1.0f);
            renderer.drawLine(iso.x + braceX, iso.y + 2.0f * markerS + bob * 0.18f,
                     iso.x + braceX - 2.5f * markerS,
                     iso.y + 2.0f * markerS - braceY + bob * 0.18f,
                     withAlpha(ringColor, ringColor.a * 0.8f), 1.0f);
        }

        if (node.getUrgency() != UrgencyLevel::LOW) {
            const float beaconBob = std::sin(animationTime * 2.0f + node.getWorldX()) * 0.6f;
            const float bx = iso.x + 9.0f * markerS;
            const float by = iso.y - 8.0f * markerS + bob + beaconBob;
            renderer.drawLine(bx, by + 3.8f * markerS, bx, by - 0.8f * markerS,
                     Color(0.8f, 0.8f, 0.75f, 0.7f), 1.2f);
            renderer.drawFilledCircle(bx, by - 1.3f * markerS, 1.1f * markerS,
                             Color(accentColor.r, accentColor.g, accentColor.b,
                                   0.6f + 0.3f * std::sin(animationTime * 4.0f)));
        }
    }
}

// =====================================================================
//  HQ harbor island
// =====================================================================

void SeaThemeRenderer::drawHQNode(IsometricRenderer& renderer,
                                   const WasteNode& node,
                                   float animationTime) {
    IsoCoord iso = RenderUtils::worldToIso(node.getWorldX(), node.getWorldY());
    const float zf = RenderUtils::getProjection().tileWidth / RenderUtils::BASE_TILE_WIDTH;
    const float s = 3.45f * zf;

    const float tide = std::sin(animationTime * 0.9f +
                                node.getWorldX() * 0.18f +
                                node.getWorldY() * 0.12f) * 0.5f;
    const float dockCx = iso.x;
    const float dockCy = iso.y + 2.0f * s + tide * 0.15f;
    const Color dockTop(0.60f, 0.47f, 0.30f, 0.98f);
    const Color dockSide(0.36f, 0.25f, 0.14f, 0.98f);
    const Color dockDark(0.27f, 0.19f, 0.12f, 0.95f);
    const Color dockShadow(0.02f, 0.05f, 0.12f, 0.22f);
    const Color foam(0.83f, 0.95f, 1.0f, 0.18f);
    const Color ropeColor(0.44f, 0.31f, 0.18f, 0.90f);
    const Color reefTop(0.09f, 0.32f, 0.40f, 0.34f);
    const Color reefGlow(0.16f, 0.48f, 0.56f, 0.22f);
    const Color duneTop(0.72f, 0.64f, 0.45f, 0.86f);
    const Color duneSide(0.52f, 0.43f, 0.30f, 0.82f);
    const Color breakwaterTop(0.43f, 0.41f, 0.38f, 0.88f);
    const Color breakwaterSide(0.28f, 0.27f, 0.25f, 0.84f);
    const Color lagoon(0.16f, 0.44f, 0.50f, 0.30f);
    const Color lagoonShine(0.82f, 0.96f, 1.0f, 0.12f);
    const Color steel(0.66f, 0.72f, 0.76f, 0.92f);

    const Color sandBase(0.68f, 0.60f, 0.42f, 0.65f);
    const Color sandSide(0.48f, 0.40f, 0.28f, 0.60f);
    renderer.drawDiamond(dockCx, dockCy + 14.2f * s, 28.0f * s, 12.6f * s, dockShadow);
    renderer.drawDiamond(dockCx + 2.0f * s, dockCy + 9.4f * s, 50.0f * s, 24.0f * s, reefTop);
    renderer.drawDiamond(dockCx + 1.5f * s, dockCy + 8.8f * s, 36.0f * s, 18.0f * s, reefGlow);
    renderer.drawDiamond(dockCx + 16.0f * s, dockCy + 10.0f * s, 22.0f * s, 8.6f * s, lagoon);
    renderer.drawDiamond(dockCx + 16.0f * s, dockCy + 9.2f * s, 15.5f * s, 5.4f * s, lagoonShine);
    renderer.drawIsometricBlock(dockCx, dockCy + 8.8f * s, 54.0f * s, 40.0f * s, 3.2f * s, sandBase, sandSide);
    renderer.drawIsometricBlock(dockCx - 6.5f * s, dockCy + 4.2f * s, 28.0f * s, 19.0f * s, 2.1f * s, duneTop, duneSide);
    renderer.drawIsometricBlock(dockCx - 19.0f * s, dockCy + 6.0f * s, 16.0f * s, 8.8f * s, 4.2f * s, breakwaterTop, breakwaterSide);
    renderer.drawIsometricBlock(dockCx + 20.0f * s, dockCy + 7.2f * s, 18.0f * s, 9.6f * s, 5.0f * s, breakwaterTop, breakwaterSide);
    renderer.drawIsometricBlock(dockCx + 14.2f * s, dockCy - 1.0f * s, 12.0f * s, 7.0f * s, 3.0f * s, breakwaterTop, breakwaterSide);

    const float shoreFoam = 0.5f + 0.5f * std::sin(animationTime * 0.8f);
    renderer.drawDiamondOutline(dockCx, dockCy + 9.6f * s,
                       56.0f * s * (0.98f + shoreFoam * 0.02f),
                       40.0f * s * (0.98f + shoreFoam * 0.02f),
                       Color(0.86f, 0.96f, 1.0f, 0.08f + shoreFoam * 0.05f), 1.6f);
    renderer.drawDiamondOutline(dockCx + 15.5f * s, dockCy + 9.6f * s,
                       27.0f * s, 11.2f * s,
                       Color(0.78f, 0.94f, 1.0f, 0.10f + shoreFoam * 0.04f), 1.3f);

    renderer.drawIsometricBlock(dockCx, dockCy, 38.0f * s, 28.0f * s, 6.0f * s, dockTop, dockSide);
    for (float offset = -0.34f; offset <= 0.34f; offset += 0.12f) {
        renderer.drawTileStripe(dockCx, dockCy - 3.8f * s, 16.0f * s, 11.0f * s,
                       true, offset, 0.42f,
                       Color(0.76f, 0.63f, 0.42f, 0.34f), 1.2f);
    }

    const std::array<IsoCoord, 4> pierTops = {{
        {dockCx - 12.0f * s, dockCy + 2.0f * s},
        {dockCx - 4.0f * s, dockCy + 5.0f * s},
        {dockCx + 5.5f * s, dockCy + 5.2f * s},
        {dockCx + 12.5f * s, dockCy + 2.4f * s}
    }};
    for (size_t i = 0; i < pierTops.size(); i++) {
        const float sway = std::sin(animationTime * 0.8f + static_cast<float>(i)) * 0.5f * s;
        renderer.drawLine(pierTops[i].x, pierTops[i].y - 1.0f * s,
                 pierTops[i].x + sway, pierTops[i].y + (13.5f + i * 1.3f) * s,
                 dockDark, 2.5f);
        renderer.drawFilledCircle(pierTops[i].x, pierTops[i].y - 1.2f * s, 1.8f * s,
                         Color(0.42f, 0.29f, 0.16f, 0.98f));
    }

    auto drawRope = [&](const IsoCoord& start, const IsoCoord& end, float sag) {
        const int segments = 12;
        for (int i = 0; i < segments; i++) {
            const float t0 = static_cast<float>(i) / segments;
            const float t1 = static_cast<float>(i + 1) / segments;
            auto sample = [&](float t) {
                const float x = RenderUtils::lerp(start.x, end.x, t);
                const float y = RenderUtils::lerp(start.y, end.y, t) +
                                sag * 4.0f * t * (1.0f - t);
                return IsoCoord{x, y};
            };
            const IsoCoord p0 = sample(t0);
            const IsoCoord p1 = sample(t1);
            renderer.drawLine(p0.x, p0.y, p1.x, p1.y, ropeColor, 1.5f);
        }
    };

    drawRope({dockCx - 12.0f * s, dockCy - 1.8f * s}, {dockCx - 4.0f * s, dockCy + 1.3f * s}, 4.8f * s);
    drawRope({dockCx + 5.5f * s, dockCy + 1.0f * s}, {dockCx + 12.5f * s, dockCy - 1.6f * s}, 4.2f * s);
    drawRope({dockCx + 11.0f * s, dockCy - 0.8f * s}, {dockCx + 18.0f * s, dockCy - 3.3f * s}, 3.8f * s);

    // Lighthouse
    const float beamAngle = animationTime * 1.35f;
    const float beamSpread = 0.44f;
    const IsoCoord beaconBase{dockCx + 9.5f * s, dockCy - 11.8f * s};
    renderer.drawIsometricBlock(beaconBase.x, beaconBase.y + 4.0f * s, 7.5f * s, 6.0f * s, 10.0f * s,
                       Color(0.88f, 0.89f, 0.84f, 1.0f), Color(0.63f, 0.64f, 0.60f, 1.0f));
    renderer.drawIsometricBlock(beaconBase.x, beaconBase.y - 1.4f * s, 4.0f * s, 3.4f * s, 4.0f * s,
                       Color(0.90f, 0.40f, 0.25f, 1.0f), Color(0.68f, 0.24f, 0.16f, 1.0f));
    const IsoCoord lightCenter{beaconBase.x, beaconBase.y - 6.0f * s};
    renderer.drawFilledCircle(lightCenter.x, lightCenter.y, 2.2f * s,
                     Color(1.0f, 0.92f, 0.68f, 0.95f));
    for (int beam = 0; beam < 2; beam++) {
        const float localAngle = beamAngle + beam * 0.42f;
        const IsoCoord rayA{
            lightCenter.x + std::cos(localAngle - beamSpread) * 38.0f * s,
            lightCenter.y + std::sin(localAngle - beamSpread) * 16.0f * s
        };
        const IsoCoord rayB{
            lightCenter.x + std::cos(localAngle + beamSpread) * 38.0f * s,
            lightCenter.y + std::sin(localAngle + beamSpread) * 16.0f * s
        };
        drawImmediateTriangle({lightCenter, rayA, rayB},
                              Color(1.0f, 0.96f, 0.72f, 0.08f));
    }

    // Storage shed
    const Color shedTop(0.52f, 0.42f, 0.30f, 0.95f);
    const Color shedSide(0.34f, 0.26f, 0.18f, 0.95f);
    renderer.drawIsometricBlock(dockCx - 8.0f * s, dockCy - 6.0f * s, 10.0f * s, 8.0f * s, 5.0f * s, shedTop, shedSide);
    renderer.drawIsometricBlock(dockCx + 4.8f * s, dockCy - 6.5f * s, 7.8f * s, 6.0f * s, 4.0f * s,
                       Color(0.74f, 0.78f, 0.82f, 0.92f), Color(0.48f, 0.52f, 0.56f, 0.92f));
    renderer.drawLine(dockCx + 1.0f * s, dockCy - 4.6f * s, dockCx + 7.8f * s, dockCy - 11.6f * s, steel, 1.7f);
    renderer.drawLine(dockCx + 6.8f * s, dockCy - 11.0f * s, dockCx + 12.2f * s, dockCy - 8.8f * s, steel, 1.4f);
    renderer.drawLine(dockCx + 8.2f * s, dockCy - 10.8f * s, dockCx + 8.2f * s, dockCy - 4.0f * s, steel, 1.3f);
    renderer.drawFilledCircle(dockCx + 12.0f * s, dockCy - 8.6f * s, 1.1f * s, Color(0.95f, 0.62f, 0.20f, 0.80f));

    const IsoCoord radarHub{dockCx + 8.4f * s, dockCy - 10.8f * s};
    const float radarSweep = animationTime * 0.72f;
    renderer.drawFilledCircle(radarHub.x, radarHub.y, 0.9f * s, Color(0.54f, 0.88f, 0.98f, 0.78f));
    renderer.drawDiamondOutline(radarHub.x, radarHub.y, 4.2f * s, 1.9f * s, Color(0.48f, 0.86f, 0.96f, 0.20f), 1.0f);
    const IsoCoord radarA{
        radarHub.x + std::cos(radarSweep - 0.22f) * 22.0f * s,
        radarHub.y + std::sin(radarSweep - 0.22f) * 10.0f * s
    };
    const IsoCoord radarB{
        radarHub.x + std::cos(radarSweep + 0.22f) * 22.0f * s,
        radarHub.y + std::sin(radarSweep + 0.22f) * 10.0f * s
    };
    drawImmediateTriangle({{radarHub, radarA, radarB}}, Color(0.58f, 0.90f, 0.98f, 0.08f));

    for (size_t light = 0; light < pierTops.size(); ++light) {
        renderer.drawFilledCircle(pierTops[light].x, pierTops[light].y - 2.0f * s, 0.75f * s,
                         Color(0.98f, 0.88f, 0.62f, 0.24f + light * 0.03f));
    }

    const float skiffBob = std::sin(animationTime * 1.6f + 0.8f) * 0.5f * s;
    renderer.drawDiamond(dockCx + 22.0f * s, dockCy + 16.2f * s + skiffBob, 7.4f * s, 3.0f * s, Color(0.02f, 0.06f, 0.12f, 0.18f));
    renderer.drawIsometricBlock(dockCx + 21.0f * s, dockCy + 11.8f * s + skiffBob, 8.6f * s, 4.0f * s, 1.2f * s,
                       Color(0.70f, 0.32f, 0.20f, 0.94f), Color(0.46f, 0.18f, 0.10f, 0.94f));
    renderer.drawLine(dockCx + 21.0f * s, dockCy + 8.8f * s + skiffBob, dockCx + 21.0f * s, dockCy + 3.8f * s + skiffBob,
             Color(0.78f, 0.78f, 0.74f, 0.72f), 1.1f);
    drawImmediateTriangle({{
        {dockCx + 21.0f * s, dockCy + 3.8f * s + skiffBob},
        {dockCx + 24.2f * s, dockCy + 6.0f * s + skiffBob},
        {dockCx + 21.0f * s, dockCy + 7.0f * s + skiffBob}
    }}, Color(0.92f, 0.94f, 0.90f, 0.72f));

    renderer.drawFilledCircle(dockCx - 14.0f * s, dockCy - 2.0f * s, 3.5f * s, Color(0.18f, 0.48f, 0.26f, 0.70f));
    renderer.drawFilledCircle(dockCx - 15.5f * s, dockCy + 1.0f * s, 2.8f * s, Color(0.22f, 0.52f, 0.30f, 0.65f));

    for (int lap = 0; lap < 4; lap++) {
        const float phase = animationTime * 1.3f + static_cast<float>(lap) * 0.65f;
        renderer.drawLine(dockCx + (-14.0f + lap * 8.8f) * s, dockCy + (12.0f + std::sin(phase) * 0.8f) * s,
                 dockCx + (-8.0f + lap * 8.8f) * s, dockCy + (13.5f + std::cos(phase) * 0.8f) * s,
                 foam, 1.2f);
    }
    renderer.drawDiamondOutline(dockCx, dockCy + 7.8f * s, 18.0f * s, 8.0f * s, Color(0.74f, 0.94f, 1.0f, 0.12f), 1.2f);
    renderer.drawDiamondOutline(dockCx + 15.4f * s, dockCy + 10.0f * s, 16.0f * s, 6.0f * s, Color(0.74f, 0.94f, 1.0f, 0.10f), 1.0f);

    for (int gull = 0; gull < 3; gull++) {
        const float orbit = animationTime * (0.55f + gull * 0.08f) + gull * 2.1f;
        const float gx = dockCx - 6.0f * s + std::cos(orbit) * (12.0f + gull * 3.0f) * s;
        const float gy = dockCy - 28.0f * s + std::sin(orbit * 1.2f) * (5.0f + gull) * s;
        const float wing = (2.0f + gull * 0.4f) * s;
        renderer.drawLine(gx - wing, gy, gx, gy - 1.2f * s, Color(0.94f, 0.96f, 0.98f, 0.70f), 1.0f);
        renderer.drawLine(gx, gy - 1.2f * s, gx + wing, gy, Color(0.94f, 0.96f, 0.98f, 0.70f), 1.0f);
    }

    renderer.drawRing(dockCx - 1.0f * s, dockCy - 5.6f * s, 5.2f * s, Color(0.18f, 0.55f, 0.88f, 0.70f), 2.0f);
    renderer.drawFilledCircle(dockCx - 1.0f * s, dockCy - 5.6f * s, 1.8f * s, Color(0.18f, 0.55f, 0.88f, 0.78f));
}

// =====================================================================
//  Truck â€” cargo boat
// =====================================================================

