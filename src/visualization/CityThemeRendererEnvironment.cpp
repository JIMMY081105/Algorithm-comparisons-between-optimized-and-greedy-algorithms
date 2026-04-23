#include "visualization/CityThemeRendererInternal.h"

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
    auto makeTreePalette = [&](const Color& canopyBase,
                               const Color& canopyAccent,
                               const Color& trunkBase,
                               float shadowAlpha) {
        TreePalette palette;
        palette.shadow = attenuateToZone(
            Color(0.02f, 0.04f, 0.02f, shadowAlpha),
            zone,
            Color(0.00f, 0.00f, 0.00f, shadowAlpha),
            0.00f,
            0.00f,
            0.02f);
        palette.trunkTop = attenuateToZone(
            scaleColor(mixColor(trunkBase, Color(0.50f, 0.36f, 0.22f, trunkBase.a), 0.14f), 1.06f),
            zone,
            voidTint,
            0.12f,
            0.22f,
            0.04f);
        palette.trunkLeft = attenuateToZone(
            scaleColor(biasColor(trunkBase, -0.05f, -0.02f, 0.00f), 0.84f),
            zone,
            voidTint,
            0.10f,
            0.24f,
            0.04f);
        palette.trunkRight = attenuateToZone(
            scaleColor(biasColor(trunkBase, 0.03f, 0.01f, -0.01f), 0.96f),
            zone,
            voidTint,
            0.10f,
            0.22f,
            0.04f);
        palette.canopyTop = attenuateToZone(
            scaleColor(mixColor(canopyBase, canopyAccent, 0.18f), 1.06f),
            zone,
            voidTint,
            0.14f,
            0.12f,
            0.06f);
        palette.canopyLeft = attenuateToZone(
            scaleColor(biasColor(canopyBase, -0.04f, -0.02f, 0.00f), 0.82f),
            zone,
            voidTint,
            0.12f,
            0.16f,
            0.06f);
        palette.canopyRight = attenuateToZone(
            scaleColor(biasColor(canopyBase, 0.03f, 0.02f, -0.01f), 0.94f),
            zone,
            voidTint,
            0.12f,
            0.14f,
            0.06f);
        palette.canopyHighlight = attenuateToZone(
            withAlpha(scaleColor(mixColor(canopyAccent, Color(0.84f, 0.94f, 0.72f, 1.0f), 0.20f), 1.04f), 0.22f),
            zone,
            voidTint,
            0.18f,
            0.10f,
            0.04f);
        return palette;
    };

    switch (p.category) {
    case CityAssets::EnvironmentCategory::Park: {
        Color parkPrimary = p.primary;
        Color parkSecondary = p.secondary;
        if (season == CitySeason::Winter) {
            parkPrimary = mixColor(
                parkPrimary,
                Color(0.74f, 0.78f, 0.82f, parkPrimary.a),
                hasSnowfall() ? 0.42f : 0.18f);
            parkSecondary = mixColor(
                parkSecondary,
                Color(0.88f, 0.92f, 0.96f, parkSecondary.a),
                hasSnowfall() ? 0.38f : 0.16f);
        }
        // Green ground patch
        drawWorldQuadPatch(placed.worldX - halfW, placed.worldY - halfD,
                           placed.worldX + halfW, placed.worldY + halfD,
                           attenuateToZone(parkPrimary, zone, voidTint, 0.08f, 0.18f));
        // Inner accent
        drawWorldQuadPatch(placed.worldX - halfW * 0.5f, placed.worldY - halfD * 0.5f,
                           placed.worldX + halfW * 0.5f, placed.worldY + halfD * 0.5f,
                           attenuateToZone(parkSecondary, zone, voidTint, 0.10f, 0.18f));
        // Dense tree planting with a packed spiral layout.
        const int treeCount =
            std::max(72, p.featureCount * 18 + (p.hasCentralFeature ? 24 : 14));
        constexpr float kGoldenAngle = 2.39996323f;
        const float minRadius = p.hasCentralFeature ? 0.18f : 0.04f;
        const float maxRadius = p.hasCentralFeature ? 0.96f : 0.98f;
        for (int t = 0; t < treeCount; ++t) {
            const float u = (static_cast<float>(t) + 0.5f) / static_cast<float>(treeCount);
            float radial = minRadius + (maxRadius - minRadius) * std::pow(u, 0.78f);
            radial = clamp01(radial + (static_cast<float>(t % 5) - 2.0f) * 0.018f);
            const float angle = kGoldenAngle * static_cast<float>(t) +
                                (static_cast<float>(t % 7) - 3.0f) * 0.07f;
            const float tx = placed.worldX + std::cos(angle) * halfW * radial;
            const float ty = placed.worldY + std::sin(angle) * halfD * (radial * 0.92f + 0.04f);
            const float pulse = 0.94f + 0.06f * std::sin(animationTime * 1.6f + static_cast<float>(t));
            const float sway = std::sin(animationTime * 1.25f + static_cast<float>(t) * 0.72f) * 0.55f * zf;
            Color canopyBase = scaleColor(
                mixColor(parkPrimary, Color(0.18f, 0.34f, 0.16f, parkPrimary.a), 0.55f),
                pulse);
            Color canopyAccent = scaleColor(
                mixColor(parkSecondary, Color(0.34f, 0.52f, 0.24f, parkSecondary.a), 0.50f),
                pulse * 1.04f);
            applySeasonalTreeCanopy(season, hasSnowfall(), canopyBase, canopyAccent);
            drawStylizedTree(
                renderer,
                tx, ty,
                zf,
                0.96f + (1.0f - u) * 0.18f + static_cast<float>(t % 2) * 0.10f,
                0.88f + static_cast<float>((t + 1) % 3) * 0.10f,
                sway,
                makeTreePalette(canopyBase, canopyAccent, Color(0.34f, 0.24f, 0.14f, 0.78f), 0.18f));
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
        const int treeCount = std::max(28, p.featureCount * 7 + 8);
        constexpr float kGoldenAngle = 2.39996323f;
        for (int t = 0; t < treeCount; ++t) {
            const float u = (static_cast<float>(t) + 0.5f) / static_cast<float>(treeCount);
            const float angle = kGoldenAngle * static_cast<float>(t);
            const float r = 0.10f + 0.84f * std::pow(u, 0.82f);
            const float tx = placed.worldX + std::cos(angle) * halfW * r;
            const float ty = placed.worldY + std::sin(angle) * halfD * (r * 0.92f + 0.04f);
            const float pulse = 0.94f + 0.06f * std::sin(animationTime * 1.4f + static_cast<float>(t) * 0.8f);
            const float sway = std::sin(animationTime * 1.18f + static_cast<float>(t) * 0.84f) * 0.48f * zf;
            Color canopyBase = scaleColor(p.primary, pulse);
            Color canopyAccent = scaleColor(
                mixColor(p.primary, p.secondary, 0.42f),
                pulse * 1.02f);
            applySeasonalTreeCanopy(season, hasSnowfall(), canopyBase, canopyAccent);
            drawStylizedTree(
                renderer,
                tx, ty,
                zf,
                1.06f + static_cast<float>(t % 3) * 0.18f,
                0.90f + static_cast<float>((t + 2) % 3) * 0.08f,
                sway,
                makeTreePalette(canopyBase, canopyAccent, Color(0.32f, 0.23f, 0.14f, 0.74f), 0.16f));
        }
        break;
    }

    case CityAssets::EnvironmentCategory::RoadsideTrees: {
        const int treeCount = std::max(10, p.featureCount * 3);
        for (int t = 0; t < treeCount; ++t) {
            const float frac = (static_cast<float>(t) + 0.5f) / static_cast<float>(treeCount);
            const float tx = placed.worldX - halfW + frac * halfW * 2.0f;
            const float ty = placed.worldY;
            const float pulse = 0.94f + 0.06f * std::sin(animationTime * 1.5f + static_cast<float>(t));
            const float sway = std::sin(animationTime * 1.22f + static_cast<float>(t) * 0.76f) * 0.36f * zf;
            Color canopyBase = scaleColor(p.primary, pulse);
            Color canopyAccent = scaleColor(
                mixColor(p.primary, Color(0.38f, 0.58f, 0.28f, p.primary.a), 0.28f),
                pulse * 1.01f);
            applySeasonalTreeCanopy(season, hasSnowfall(), canopyBase, canopyAccent);
            drawStylizedTree(
                renderer,
                tx, ty,
                zf,
                0.90f + static_cast<float>(t % 2) * 0.12f,
                0.84f + static_cast<float>((t + 1) % 3) * 0.08f,
                sway,
                makeTreePalette(canopyBase, canopyAccent, Color(0.32f, 0.24f, 0.15f, 0.74f), 0.14f));
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
    drawOrAppendDiamond(renderer, iso.x, iso.y, v.length, v.width,
                        attenuateToZone(
                            v.body, zone, Color(0.01f, 0.02f, 0.04f, v.body.a),
                            0.24f, 0.16f, 0.08f));
    drawOrAppendDiamondOutline(renderer, iso.x, iso.y, v.length, v.width,
                               attenuateToZone(
                                   v.outline, zone, Color(0.00f, 0.00f, 0.00f, v.outline.a),
                                   0.18f, 0.08f, 0.04f), 1.0f);

    // Windshield highlight for sedans and vans
    if (v.category == CityAssets::VehicleCategory::Sedan ||
        v.category == CityAssets::VehicleCategory::Van) {
        drawOrAppendDiamond(renderer, iso.x, iso.y - v.width * 0.15f,
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

void CityThemeRenderer::drawMountain(const MountainPeak& peak) const {
    const IsoCoord iso = RenderUtils::worldToIso(peak.worldX, peak.worldY);

    // Mountains use extended zone visibility â€” background scenery should remain
    // visible much further out than gameplay elements when zoomed out.
    // Instead of a hard cull that produces a visible black ring, we fade each
    // ring progressively into the void so distant mountains blend with the
    // background rather than popping off at a fixed radius.
    const float outside = zoneOutsideDistance(peak.worldX, peak.worldY, 6.0f, 40.0f, 34.0f);
    // fade: 0 at city edge â†’ 1 at the outer ring (fully void-tinted)
    const float fade = std::min(1.0f, smoothRange(0.0f, 1.0f, outside));
    const float transition = 1.0f - fade;
    // Only cull mountains so far out they'd be indistinguishable from the
    // background even at full void tint â€” no hard visible cutoff.
    if (outside > 1.35f) {
        return;
    }

    const float zf = currentZoomFactor();
    const float halfW = peak.baseWidth * 34.0f * zf;
    const float halfD = peak.baseDepth * 24.0f * zf;
    const float height = peak.heightScale * 80.0f * zf;
    const float cv = peak.colorVar;

    // Rich Clash of Clans palette â€” 4 altitude tiers
    // Tier 0: lush green-brown foothills
    Color tier0(0.26f + cv * 0.03f, 0.36f + cv * 0.04f, 0.17f + cv * 0.02f, 0.96f);
    // Tier 1: earthy brown slopes
    Color tier1(0.35f + cv * 0.03f, 0.28f + cv * 0.03f, 0.19f + cv * 0.02f, 0.96f);
    // Tier 2: grey rock face
    Color tier2(0.40f + cv * 0.03f, 0.38f + cv * 0.03f, 0.36f + cv * 0.02f, 0.96f);
    // Tier 3: light grey summit rock
    Color tier3(0.55f + cv * 0.04f, 0.53f + cv * 0.04f, 0.50f + cv * 0.03f, 0.96f);

    const Color snowCol(0.88f, 0.92f, 0.96f, 0.96f);
    float snowAmt = 0.0f;

    // Seasonal colour shifts
    switch (season) {
        case CitySeason::Summer:
            tier0 = mixColor(tier0, Color(0.30f, 0.44f, 0.18f, 0.96f), 0.20f);
            break;
        case CitySeason::Autumn:
            tier0 = mixColor(tier0, Color(0.42f, 0.32f, 0.15f, 0.96f), 0.30f);
            tier1 = mixColor(tier1, Color(0.40f, 0.28f, 0.13f, 0.96f), 0.20f);
            break;
        case CitySeason::Winter:
            if (hasWinterStormActive())  snowAmt = 0.85f;
            else if (hasSnowfall())      snowAmt = 0.60f;
            else                         snowAmt = 0.30f;
            tier0 = desaturateColor(tier0, 0.30f);
            tier1 = desaturateColor(tier1, 0.20f);
            break;
        default:
            break;
    }

    // Weather lighting
    switch (weather) {
        case CityWeather::Stormy:
            tier0 = scaleColor(tier0, 0.52f);
            tier1 = scaleColor(tier1, 0.52f);
            tier2 = scaleColor(tier2, 0.56f);
            tier3 = scaleColor(tier3, 0.58f);
            break;
        case CityWeather::Rainy:
            tier0 = scaleColor(tier0, 0.70f);
            tier1 = scaleColor(tier1, 0.70f);
            tier2 = scaleColor(tier2, 0.74f);
            tier3 = scaleColor(tier3, 0.76f);
            break;
        case CityWeather::Sunny:
            tier1 = scaleColor(tier1, 1.14f);
            tier2 = scaleColor(tier2, 1.18f);
            tier3 = scaleColor(tier3, 1.22f);
            break;
        default:
            break;
    }

    // Distance fog â€” ring-by-ring darkening toward the void.
    // fogBlend ramps from 0 at the city edge up to 1.0 at the outer ring so
    // each successive ring of mountains reads darker than the one in front of
    // it and the furthest peaks fade cleanly into the black background
    // (no hard circular cutoff).
    const Color voidTint(0.02f, 0.03f, 0.04f, 0.96f);
    const float fogBlend = std::pow(fade, 0.75f);
    tier0 = mixColor(tier0, voidTint, fogBlend);
    tier1 = mixColor(tier1, voidTint, fogBlend);
    tier2 = mixColor(tier2, voidTint, fogBlend);
    tier3 = mixColor(tier3, voidTint, fogBlend);

    // Soft foothill glow blends mountain base into the terrain
    drawGradientEllipse(iso.x, iso.y + halfD * 0.3f,
                        halfW * 1.9f, halfD * 1.5f,
                        withAlpha(tier0, 0.28f * transition),
                        withAlpha(tier0, 0.0f));

    drawMountainShape(iso.x, iso.y, halfW, halfD, height,
                      tier0, tier1, tier2, tier3,
                      snowAmt, snowCol);
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

