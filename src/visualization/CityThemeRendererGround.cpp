#include "CityThemeRendererInternal.h"

void CityThemeRenderer::drawGroundPlane(IsometricRenderer& renderer,
                                        const MapGraph&,
                                        const Truck&,
                                        const MissionPresentation*,
                                        float animationTime) {
    (void)renderer;
    (void)animationTime;

    ensureStaticBatchProjectionCurrent();

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const float left = static_cast<float>(viewport[0]);
    const float top = static_cast<float>(viewport[1]);
    const float right = left + static_cast<float>(viewport[2]);
    const float bottom = top + static_cast<float>(viewport[3]);
    const bool snowfall = hasSnowfall();
    const bool winterStorm = hasWinterStormActive();

    Color skyTop(0.22f, 0.30f, 0.40f, 1.0f);
    Color skyBottom(0.11f, 0.14f, 0.18f, 1.0f);
    Color groundBase(0.17f, 0.18f, 0.16f, 0.98f);
    Color districtLift(0.42f, 0.46f, 0.50f, 0.13f);
    Color wetTint(0.14f, 0.20f, 0.26f, 0.10f);

    switch (weather) {
        case CityWeather::Sunny:
            skyTop = Color(0.26f, 0.34f, 0.46f, 1.0f);
            skyBottom = Color(0.13f, 0.16f, 0.22f, 1.0f);
            groundBase = Color(0.18f, 0.18f, 0.15f, 0.98f);
            districtLift = Color(0.98f, 0.78f, 0.40f, 0.16f);
            wetTint = Color(0.14f, 0.18f, 0.22f, 0.05f);
            break;
        case CityWeather::Cloudy:
            skyTop = Color(0.20f, 0.24f, 0.30f, 1.0f);
            skyBottom = Color(0.10f, 0.12f, 0.17f, 1.0f);
            groundBase = Color(0.16f, 0.17f, 0.16f, 0.98f);
            districtLift = Color(0.54f, 0.58f, 0.64f, 0.12f);
            wetTint = Color(0.14f, 0.18f, 0.24f, 0.10f);
            break;
        case CityWeather::Rainy:
            skyTop = Color(0.14f, 0.18f, 0.24f, 1.0f);
            skyBottom = Color(0.07f, 0.10f, 0.14f, 1.0f);
            groundBase = Color(0.12f, 0.13f, 0.14f, 0.99f);
            districtLift = Color(0.42f, 0.48f, 0.56f, 0.12f);
            wetTint = Color(0.16f, 0.24f, 0.34f, 0.14f);
            break;
        case CityWeather::Stormy:
            skyTop = Color(0.09f, 0.12f, 0.18f, 1.0f);
            skyBottom = Color(0.05f, 0.07f, 0.10f, 1.0f);
            groundBase = Color(0.09f, 0.10f, 0.12f, 1.0f);
            districtLift = Color(0.34f, 0.40f, 0.48f, 0.11f);
            wetTint = Color(0.18f, 0.26f, 0.36f, 0.16f);
            break;
    }

    if (season == CitySeason::Winter) {
        const float floorSnowMix = winterStorm ? 0.92f : (snowfall ? 0.66f : 0.12f);
        const float liftMix      = winterStorm ? 0.70f : (snowfall ? 0.44f : 0.10f);
        const float wetMix       = winterStorm ? 0.80f : (snowfall ? 0.56f : 0.12f);
        skyTop = mixColor(skyTop, Color(0.54f, 0.60f, 0.70f, 1.0f), snowfall ? 0.34f : 0.16f);
        skyBottom = mixColor(skyBottom, Color(0.20f, 0.24f, 0.30f, 1.0f), snowfall ? 0.26f : 0.10f);
        groundBase = mixColor(groundBase, Color(0.94f, 0.96f, 0.99f, groundBase.a),
                              floorSnowMix);
        districtLift = mixColor(districtLift, Color(0.92f, 0.95f, 0.99f, districtLift.a),
                                liftMix);
        wetTint = mixColor(wetTint, Color(0.92f, 0.95f, 0.99f, wetTint.a),
                           wetMix);
        if (winterStorm) {
            skyTop = mixColor(skyTop, Color(0.02f, 0.03f, 0.06f, 1.0f), 0.24f);
            skyBottom = mixColor(skyBottom, Color(0.00f, 0.00f, 0.02f, 1.0f), 0.34f);
        }
    }

    skyTop = mixColor(skyTop, Color(0.02f, 0.02f, 0.03f, 1.0f), 0.88f);
    skyBottom = mixColor(skyBottom, Color(0.00f, 0.00f, 0.00f, 1.0f), 0.96f);
    const float groundDarken = snowfall ? (winterStorm ? 0.04f : 0.18f) : 0.72f;
    groundBase = mixColor(groundBase, Color(0.01f, 0.01f, 0.01f, groundBase.a), groundDarken);

    glBegin(GL_QUADS);
    glColor4f(skyTop.r, skyTop.g, skyTop.b, skyTop.a);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glColor4f(skyBottom.r, skyBottom.g, skyBottom.b, skyBottom.a);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();

    auto buildGroundGeometry = [&]() {
    const float peripheralDarken = snowfall ? (winterStorm ? 0.02f : 0.12f) : 0.44f;
    groundBase = mixColor(groundBase, Color(0.03f, 0.04f, 0.07f, groundBase.a),
                          peripheralDarken);
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

    drawGradientEllipse(opIso.x, opIso.y + glowRadiusY * 0.20f,
                        glowRadiusX * 1.78f, glowRadiusY * 1.46f,
                        withAlpha(scaleColor(districtLift, 1.06f), 0.03f),
                        withAlpha(districtLift, 0.0f));

    auto blockHasMeaningfulContent = [&](const BlockZone& block) {
        if (block.district == DistrictType::Park ||
            block.district == DistrictType::Campus ||
            block.district == DistrictType::Landmark ||
            block.landmarkCluster ||
            block.civicAnchor ||
            block.parkAnchor) {
            return true;
        }

        const float marginX = (block.maxX - block.minX) * 0.05f;
        const float marginY = (block.maxY - block.minY) * 0.05f;
        const auto contains = [&](float worldX, float worldY) {
            return worldX >= block.minX - marginX &&
                   worldX <= block.maxX + marginX &&
                   worldY >= block.minY - marginY &&
                   worldY <= block.maxY + marginY;
        };

        for (const auto& placed : presetBuildings) {
            if (contains(placed.worldX, placed.worldY)) {
                return true;
            }
        }
        for (const auto& env : presetEnvironments) {
            if (contains(env.worldX, env.worldY)) {
                return true;
            }
        }
        for (const auto& vehicle : presetVehicles) {
            if (contains(vehicle.worldX, vehicle.worldY)) {
                return true;
            }
        }
        for (const auto& prop : presetRoadProps) {
            if (contains(prop.worldX, prop.worldY)) {
                return true;
            }
        }
        return false;
    };

    for (const BlockZone& block : blocks) {
        const float spanX = block.maxX - block.minX;
        const float spanY = block.maxY - block.minY;
        const float insetX = spanX * 0.05f;
        const float insetY = spanY * 0.06f;
        const ZoneVisibility zone = computeZoneVisibility(
            block.centerX, block.centerY,
            operationalCenterX, operationalCenterY,
            operationalRadiusX, operationalRadiusY);
        const bool meaningful = blockHasMeaningfulContent(block);

        if (zone.transition <= 0.03f && !meaningful) {
            continue;
        }

        Color patch(0.18f, 0.18f, 0.17f, 0.96f);
        Color curb(0.11f, 0.11f, 0.12f, 0.92f);
        switch (block.district) {
            case DistrictType::Landmark:
                patch = Color(0.30f, 0.31f, 0.34f, 0.96f);
                curb = Color(0.16f, 0.17f, 0.20f, 0.96f);
                break;
            case DistrictType::Commercial:
                patch = Color(0.24f, 0.24f, 0.25f, 0.96f);
                curb = Color(0.12f, 0.12f, 0.13f, 0.94f);
                break;
            case DistrictType::Mixed:
                patch = Color(0.22f, 0.22f, 0.21f, 0.96f);
                curb = Color(0.11f, 0.11f, 0.12f, 0.94f);
                break;
            case DistrictType::Residential:
                patch = Color(0.18f, 0.20f, 0.17f, 0.96f);
                curb = Color(0.11f, 0.12f, 0.11f, 0.92f);
                break;
            case DistrictType::Campus:
                patch = Color(0.20f, 0.22f, 0.18f, 0.96f);
                curb = Color(0.12f, 0.13f, 0.12f, 0.92f);
                break;
            case DistrictType::Park:
                patch = Color(0.18f, 0.26f, 0.16f, 0.94f);
                curb = Color(0.10f, 0.13f, 0.10f, 0.90f);
                break;
            case DistrictType::Service:
                patch = Color(0.18f, 0.18f, 0.18f, 0.96f);
                curb = Color(0.10f, 0.10f, 0.10f, 0.92f);
                break;
        }

        patch = mixColor(patch, wetTint,
                         (weather == CityWeather::Rainy || weather == CityWeather::Stormy)
                             ? 0.26f : 0.0f);

        if (snowfall) {
            const Color snowTint(0.94f, 0.96f, 0.99f, patch.a);
            const float patchSnowMix = winterStorm ? 0.88f : 0.60f;
            const float curbSnowMix  = winterStorm ? 0.70f : 0.45f;
            patch = mixColor(patch, snowTint, patchSnowMix);
            curb = mixColor(curb,
                            Color(0.82f, 0.86f, 0.92f, curb.a),
                            curbSnowMix);
        }

        if (!meaningful &&
            block.district != DistrictType::Park &&
            block.district != DistrictType::Campus) {
            patch = mixColor(patch, Color(0.03f, 0.05f, 0.08f, patch.a), 0.84f);
            curb = mixColor(curb, Color(0.01f, 0.02f, 0.04f, curb.a), 0.82f);
        }

        patch = attenuateToZone(
            patch, zone, Color(0.01f, 0.02f, 0.04f, patch.a),
            meaningful ? 0.08f : 0.01f,
            meaningful ? 0.28f : 0.52f);
        curb = attenuateToZone(
            curb, zone, Color(0.00f, 0.01f, 0.03f, curb.a),
            meaningful ? 0.06f : 0.00f,
            0.42f);

        drawWorldQuadPatch(block.minX + insetX * 0.36f, block.minY + insetY * 0.36f,
                           block.maxX - insetX * 0.36f, block.maxY - insetY * 0.36f,
                           curb);
        drawWorldQuadPatch(block.minX + insetX, block.minY + insetY,
                           block.maxX - insetX, block.maxY - insetY,
                           patch);

        if (meaningful &&
            (block.visualTier == VisualTier::Focus || block.district == DistrictType::Landmark)) {
            drawWorldQuadPatch(block.centerX - spanX * 0.16f,
                               block.centerY - spanY * 0.16f,
                               block.centerX + spanX * 0.16f,
                               block.centerY + spanY * 0.16f,
                               attenuateToZone(
                                   Color(0.32f, 0.36f, 0.42f, 0.12f),
                                   zone,
                                   Color(0.01f, 0.02f, 0.04f, 0.12f),
                                   0.06f,
                                   0.18f));
        }

        if (block.district == DistrictType::Park || block.district == DistrictType::Campus) {
            drawWorldQuadPatch(block.centerX - spanX * 0.10f,
                               block.centerY - spanY * 0.10f,
                               block.centerX + spanX * 0.10f,
                               block.centerY + spanY * 0.10f,
                               attenuateToZone(
                                   Color(0.22f, 0.34f, 0.18f, 0.12f),
                                   zone,
                                   Color(0.01f, 0.02f, 0.04f, 0.12f),
                                   0.04f,
                                   0.24f));
        }
    }

    };

    if (groundBatch.dirty) {
        std::vector<float> verts;
        verts.reserve(4096);
        {
            ScopedStaticBatchCapture capture(verts);
            buildGroundGeometry();
        }
        groundBatch.upload(verts);
    }
    groundBatch.draw();
}

