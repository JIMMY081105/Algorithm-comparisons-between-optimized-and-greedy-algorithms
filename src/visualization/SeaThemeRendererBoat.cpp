#include "SeaThemeRendererInternal.h"

void SeaThemeRenderer::drawTruck(IsometricRenderer& renderer,
                                  const MapGraph& graph,
                                  const Truck& truck,
                                  const MissionPresentation* mission,
                                  float animationTime) {
    const RouteResult& currentRoute = routeFromMission(mission);
    if (!truck.isMoving() && !currentRoute.isValid()) return;

    IsoCoord iso = RenderUtils::worldToIso(truck.getPosX(), truck.getPosY());
    const OceanWaveField boatWave =
        sampleOceanWaveField(truck.getPosX(), truck.getPosY(), animationTime);
    float headingX = 1.0f;
    float headingY = 0.0f;
    getTruckHeading(graph, truck, currentRoute, headingX, headingY);
    float lateralX = -headingY;
    float lateralY = headingX;
    normalizeVector(lateralX, lateralY, 0.0f, 1.0f);
    const float bowSurface = sampleOceanSurfaceOffset(truck.getPosX() + headingX * 0.88f,
                                                      truck.getPosY() + headingY * 0.88f,
                                                      animationTime);
    const float sternSurface = sampleOceanSurfaceOffset(truck.getPosX() - headingX * 0.88f,
                                                        truck.getPosY() - headingY * 0.88f,
                                                        animationTime);
    const float portSurface = sampleOceanSurfaceOffset(truck.getPosX() + lateralX * 0.54f,
                                                       truck.getPosY() + lateralY * 0.54f,
                                                       animationTime);
    const float starboardSurface = sampleOceanSurfaceOffset(truck.getPosX() - lateralX * 0.54f,
                                                            truck.getPosY() - lateralY * 0.54f,
                                                            animationTime);
    boatCargoFillRatio = computeBoatCargoFill(graph, truck, currentRoute);
    boatPitchWave = bowSurface - sternSurface;
    boatRollWave = portSurface - starboardSurface;
    boatWakeStrength = truck.isMoving()
        ? 0.24f + boatWave.crest * 0.14f + std::abs(boatPitchWave) * 0.10f
        : 0.04f + boatWave.crest * 0.02f;
    boatIsMoving = truck.isMoving();
    const float boatSurfaceOffset =
        ((bowSurface + sternSurface) * 0.5f + boatWave.swell * 0.34f + boatWave.primary * 0.10f) * 4.0f;
    const float bobPhase = animationTime * (truck.isMoving() ? 1.9f : 1.35f) +
                           boatWave.combined * 1.8f;
    drawBoatSprite(renderer, iso.x, iso.y + 0.9f + boatSurfaceOffset, 4.4f, RenderUtils::getTruckColor(),
                   headingX, headingY, bobPhase, animationTime);
}

void SeaThemeRenderer::drawBoatSprite(IsometricRenderer& renderer,
                                       float cx, float cy, float scale,
                                       const Color& bodyColor,
                                       float headingX, float headingY,
                                       float bobPhase, float animationTime) {
    const float bobOffset = (std::sin(bobPhase) * 0.018f + boatPitchWave * 0.08f) * scale;
    const float rollOffset = (std::sin(bobPhase * 0.85f + animationTime * 1.6f) * 0.05f +
                              boatRollWave * 0.55f) * scale;
    const float pitchOffset = (boatIsMoving ? 0.03f : 0.014f) *
                              std::sin(animationTime * (boatIsMoving ? 4.8f : 1.2f) +
                                       bobPhase * 0.45f) + boatPitchWave * 0.10f;
    cy += bobOffset;

    const Color hullTop(0.78f, 0.28f, 0.16f, 0.99f);
    const Color hullLeft(0.53f, 0.16f, 0.09f, 0.99f);
    const Color hullRight(0.66f, 0.21f, 0.12f, 0.99f);
    const Color hullRear(0.46f, 0.12f, 0.07f, 0.99f);
    const Color trimTop(0.97f, 0.92f, 0.74f, 0.98f);
    const Color trimSide(0.82f, 0.74f, 0.56f, 0.98f);
    const Color deckTop(0.72f, 0.55f, 0.34f, 0.98f);
    const Color deckSide(0.46f, 0.30f, 0.16f, 0.98f);
    const Color cabinTop = shiftColor(bodyColor, 0.22f);
    const Color cabinLeft(bodyColor.r * 0.62f, bodyColor.g * 0.62f, bodyColor.b * 0.62f, 1.0f);
    const Color cabinRight(bodyColor.r * 0.78f, bodyColor.g * 0.78f, bodyColor.b * 0.78f, 1.0f);
    const Color cabinRear(bodyColor.r * 0.54f, bodyColor.g * 0.54f, bodyColor.b * 0.54f, 1.0f);
    const Color sailTop(0.97f, 0.95f, 0.86f, 0.98f);
    const Color sailShade(0.82f, 0.86f, 0.84f, 0.96f);
    const Color glass(0.38f, 0.74f, 0.94f, 0.94f);
    const Color rail(0.88f, 0.84f, 0.74f, 0.88f);
    const Color wakeColor(0.82f, 0.95f, 1.0f, 0.10f + boatWakeStrength * 0.12f);
    const Color foamColor(0.92f, 0.98f, 1.0f, 0.10f + boatWakeStrength * 0.14f);
    const Color cargoTop(0.18f, 0.18f, 0.16f, 0.94f);
    const Color cargoSide(0.10f, 0.10f, 0.09f, 0.92f);
    const auto& projection = RenderUtils::getProjection();

    float screenForwardX = (headingX - headingY) * (projection.tileWidth * 0.5f);
    float screenForwardY = (headingX + headingY) * (projection.tileHeight * 0.5f);
    normalizeVector(screenForwardX, screenForwardY, 0.92f, 0.38f);
    const float screenRightX = -screenForwardY;
    const float screenRightY = screenForwardX;

    renderer.drawDiamond(cx, cy + 6.6f * scale, 10.6f * scale, 4.2f * scale, Color(0.02f, 0.07f, 0.12f, 0.20f));
    renderer.drawDiamond(cx + screenForwardX * 1.2f,
                cy + 5.8f * scale + std::sin(animationTime * 1.7f + bobPhase) * 0.4f,
                9.2f * scale, 3.2f * scale,
                Color(0.54f, 0.84f, 0.94f, 0.06f + boatWakeStrength * 0.05f));
    renderer.drawDiamondOutline(cx, cy + 5.7f * scale,
                       11.2f * scale * (0.98f + boatWakeStrength * 0.03f),
                       4.0f * scale * (0.98f + boatWakeStrength * 0.04f),
                       Color(0.88f, 0.97f, 1.0f, 0.08f + boatWakeStrength * 0.06f), 1.0f);

    auto boatPoint = [&](float lateral, float longitudinal, float height = 0.0f) {
        const float rollLift = (lateral / 8.6f) * rollOffset;
        const float pitchLift = longitudinal * pitchOffset * scale * 0.11f;
        return IsoCoord{
            cx + screenRightX * lateral * scale + screenForwardX * longitudinal * scale,
            cy + screenRightY * lateral * scale + screenForwardY * longitudinal * scale -
                height * scale - rollLift - pitchLift
        };
    };

    auto quad = [&](const std::array<IsoCoord, 4>& pts, const Color& color) {
        drawImmediateQuad(pts, color);
    };
    auto tri = [&](const std::array<IsoCoord, 3>& pts, const Color& color) {
        drawImmediateTriangle(pts, color);
    };

    auto prism = [&](float left, float right,
                     float rear, float front,
                     float baseHeight, float topHeight,
                     const Color& topColor,
                     const Color& leftColor,
                     const Color& rightColor,
                     const Color& rearColor) {
        const IsoCoord bRearLeft = boatPoint(left, rear, baseHeight);
        const IsoCoord bRearRight = boatPoint(right, rear, baseHeight);
        const IsoCoord bFrontRight = boatPoint(right, front, baseHeight);
        const IsoCoord bFrontLeft = boatPoint(left, front, baseHeight);
        const IsoCoord tRearLeft = boatPoint(left, rear, topHeight);
        const IsoCoord tRearRight = boatPoint(right, rear, topHeight);
        const IsoCoord tFrontRight = boatPoint(right, front, topHeight);
        const IsoCoord tFrontLeft = boatPoint(left, front, topHeight);
        quad({bRearLeft, bRearRight, tRearRight, tRearLeft}, rearColor);
        quad({bRearRight, bFrontRight, tFrontRight, tRearRight}, rightColor);
        quad({bFrontLeft, bRearLeft, tRearLeft, tFrontLeft}, leftColor);
        quad({tRearLeft, tRearRight, tFrontRight, tFrontLeft}, topColor);
    };

    // Wake
    const IsoCoord sternWash = boatPoint(0.0f, -10.2f, 0.2f);
    const float wakeBase = 0.70f + boatWakeStrength * 0.90f;
    renderer.drawFilledCircle(sternWash.x, sternWash.y + 1.5f, 1.7f * scale * wakeBase, shiftColor(foamColor, 0.0f, 0.55f));
    renderer.drawRing(sternWash.x, sternWash.y + 1.5f, 2.6f * scale * wakeBase, shiftColor(foamColor, -0.04f, 0.48f), 0.9f);
    if (boatIsMoving) {
        for (int band = 0; band < 4; band++) {
            const float lateral = 1.6f + band * 1.7f;
            const float rear = 12.0f + band * 6.0f;
            const float alphaScale = 0.88f - band * 0.22f;
            renderer.drawLine(boatPoint(-1.6f, -8.0f, 0.2f).x, boatPoint(-1.6f, -8.0f, 0.2f).y,
                     boatPoint(-lateral, -rear, 0.0f).x, boatPoint(-lateral, -rear, 0.0f).y,
                     shiftColor(wakeColor, 0.01f * band, alphaScale), 1.2f - band * 0.2f);
            renderer.drawLine(boatPoint(1.6f, -8.0f, 0.2f).x, boatPoint(1.6f, -8.0f, 0.2f).y,
                     boatPoint(lateral, -rear, 0.0f).x, boatPoint(lateral, -rear, 0.0f).y,
                     shiftColor(wakeColor, 0.01f * band, alphaScale), 1.2f - band * 0.2f);
        }
        renderer.drawLine(sternWash.x, sternWash.y + 0.5f,
                 boatPoint(0.0f, -18.0f, 0.0f).x, boatPoint(0.0f, -18.0f, 0.0f).y,
                 shiftColor(wakeColor, -0.02f, 0.72f), 1.0f);
        renderer.drawLine(sternWash.x, sternWash.y + 1.2f,
                 boatPoint(0.0f, -24.0f, 0.0f).x, boatPoint(0.0f, -24.0f, 0.0f).y,
                 shiftColor(foamColor, -0.04f, 0.44f), 0.9f);
    }

    // Hull
    prism(-7.8f, 7.8f, -8.2f, 7.4f, 0.0f, 2.5f, hullTop, hullLeft, hullRight, hullRear);
    tri({boatPoint(0.0f, 12.0f, 1.4f), boatPoint(-5.4f, 7.4f, 2.5f), boatPoint(5.4f, 7.4f, 2.5f)}, hullTop);
    tri({boatPoint(0.0f, 12.0f, 0.0f), boatPoint(0.0f, 12.0f, 1.4f), boatPoint(-5.4f, 7.4f, 2.5f)}, hullLeft);
    tri({boatPoint(0.0f, 12.0f, 0.0f), boatPoint(5.4f, 7.4f, 2.5f), boatPoint(0.0f, 12.0f, 1.4f)}, hullRight);

    // Trim and deck
    prism(-7.1f, 7.1f, -7.4f, 6.8f, 2.5f, 3.2f, trimTop, trimSide, shiftColor(trimSide, 0.04f), shiftColor(trimSide, -0.08f));
    prism(-6.2f, 6.2f, -6.2f, 5.8f, 3.2f, 4.3f, deckTop, deckSide, shiftColor(deckSide, 0.06f), shiftColor(deckSide, -0.08f));
    prism(-5.6f, 5.6f, -5.6f, 0.8f, 4.3f, 6.4f,
          shiftColor(deckTop, -0.10f), shiftColor(deckSide, -0.08f),
          shiftColor(deckSide, -0.02f), shiftColor(deckSide, -0.12f));

    // Cargo
    const float cargoHeight = 4.38f + boatCargoFillRatio * 1.55f;
    if (boatCargoFillRatio > 0.02f) {
        prism(-4.8f, 4.8f, -4.9f, 0.1f, 4.32f, cargoHeight,
              cargoTop, cargoSide, shiftColor(cargoSide, 0.04f), shiftColor(cargoSide, -0.02f));
        for (int piece = 0; piece < 4; piece++) {
            const float px = -2.8f + piece * 1.8f;
            const float py = -4.1f + piece * 0.95f;
            const IsoCoord debris = boatPoint(px, py, cargoHeight + 0.08f + std::sin(animationTime * 1.6f + piece) * 0.08f);
            renderer.drawFilledCircle(debris.x, debris.y, (0.34f + piece * 0.05f) * scale, Color(0.12f, 0.11f, 0.09f, 0.92f));
        }
    }

    // Cabin
    prism(-4.1f, 4.1f, 2.7f, 7.0f, 4.3f, 8.6f, cabinTop, cabinLeft, cabinRight, cabinRear);
    prism(-2.9f, 2.9f, 4.8f, 7.2f, 6.0f, 8.2f, glass, shiftColor(glass, -0.12f), shiftColor(glass, -0.04f), shiftColor(glass, -0.20f));

    // Sails
    quad({boatPoint(-4.6f, 3.0f, 8.6f), boatPoint(4.6f, 3.0f, 8.6f),
          boatPoint(2.4f, 0.8f, 9.8f), boatPoint(-2.4f, 0.8f, 9.8f)}, sailTop);
    quad({boatPoint(-0.4f, 1.2f, 9.7f), boatPoint(2.4f, 1.1f, 9.6f),
          boatPoint(2.6f, -2.8f, 9.0f), boatPoint(-0.2f, -2.7f, 9.1f)}, sailShade);

    // Railing
    for (float pos = -4.4f; pos <= 4.4f; pos += 2.2f) {
        const IsoCoord leftBottom = boatPoint(-6.4f, pos, 4.3f);
        const IsoCoord leftTop = boatPoint(-6.4f, pos, 6.1f);
        const IsoCoord rightBottom = boatPoint(6.4f, pos, 4.3f);
        const IsoCoord rightTop = boatPoint(6.4f, pos, 6.1f);
        renderer.drawLine(leftBottom.x, leftBottom.y, leftTop.x, leftTop.y, rail, 1.0f);
        renderer.drawLine(rightBottom.x, rightBottom.y, rightTop.x, rightTop.y, rail, 1.0f);
    }
    renderer.drawLine(boatPoint(-6.4f, -4.4f, 6.1f).x, boatPoint(-6.4f, -4.4f, 6.1f).y,
             boatPoint(-6.4f, 4.4f, 6.1f).x, boatPoint(-6.4f, 4.4f, 6.1f).y, rail, 1.0f);
    renderer.drawLine(boatPoint(6.4f, -4.4f, 6.1f).x, boatPoint(6.4f, -4.4f, 6.1f).y,
             boatPoint(6.4f, 4.4f, 6.1f).x, boatPoint(6.4f, 4.4f, 6.1f).y, rail, 1.0f);

    // Mast and flag
    const IsoCoord mastBase = boatPoint(0.4f, 2.2f, 8.8f);
    const IsoCoord mastTop = boatPoint(0.4f, 2.2f, 12.8f);
    renderer.drawLine(mastBase.x, mastBase.y, mastTop.x, mastTop.y, Color(0.55f, 0.40f, 0.24f, 1.0f), 1.4f);
    const float flagSway = std::sin(animationTime * 3.4f + bobPhase * 0.6f) * 1.2f;
    const IsoCoord flagTip{
        mastTop.x + screenForwardX * (3.0f + flagSway * 0.4f) + screenRightX * (1.2f + flagSway * 0.3f),
        mastTop.y + screenForwardY * (3.0f + flagSway * 0.4f) + screenRightY * (1.2f + flagSway * 0.3f) + flagSway * 0.6f
    };
    const IsoCoord flagLow{
        mastTop.x + screenForwardX * 2.0f + screenRightX * 0.2f,
        mastTop.y + screenForwardY * 2.0f + screenRightY * 0.2f + 1.3f
    };
    tri({mastTop, flagTip, flagLow}, Color(0.95f, 0.84f, 0.22f, 0.96f));

    // Nav lights
    const IsoCoord bowLight = boatPoint(0.0f, 11.0f, 2.0f);
    const IsoCoord sternLeft = boatPoint(-3.8f, -7.4f, 3.5f);
    const IsoCoord sternRight = boatPoint(3.8f, -7.4f, 3.5f);
    renderer.drawFilledCircle(bowLight.x, bowLight.y, 0.65f * scale, Color(1.0f, 0.94f, 0.60f, 0.96f));
    renderer.drawFilledCircle(sternLeft.x, sternLeft.y, 0.50f * scale, Color(0.94f, 0.26f, 0.20f, 0.94f));
    renderer.drawFilledCircle(sternRight.x, sternRight.y, 0.50f * scale, Color(0.94f, 0.26f, 0.20f, 0.94f));

    // Waterline
    for (int lap = 0; lap < 5; lap++) {
        const float t = static_cast<float>(lap) / 4.0f;
        const float surge = std::sin(animationTime * 2.0f + t * 3.5f) * 0.26f;
        const IsoCoord leftWaterA = boatPoint(-7.6f, -6.4f + t * 12.0f, 0.8f);
        const IsoCoord leftWaterB = boatPoint(-8.7f, -6.0f + t * 11.0f, 0.2f);
        const IsoCoord rightWaterA = boatPoint(7.6f, -6.4f + t * 12.0f, 0.8f);
        const IsoCoord rightWaterB = boatPoint(8.7f, -6.0f + t * 11.0f, 0.2f);
        renderer.drawLine(leftWaterA.x, leftWaterA.y + surge, leftWaterB.x, leftWaterB.y - surge,
                 Color(0.82f, 0.95f, 1.0f, 0.16f), 1.0f);
        renderer.drawLine(rightWaterA.x, rightWaterA.y - surge, rightWaterB.x, rightWaterB.y + surge,
                 Color(0.82f, 0.95f, 1.0f, 0.16f), 1.0f);
    }
    renderer.drawDiamond(cx + screenForwardX * 1.2f, cy + 6.2f * scale, 9.2f * scale, 2.2f * scale,
                Color(0.88f, 0.97f, 1.0f, 0.05f));

    // Bow spray
    if (boatIsMoving) {
        for (int spray = 0; spray < 3; spray++) {
            const float phase = animationTime * 3.4f + spray * 0.8f;
            const IsoCoord particle = boatPoint(-1.4f + std::sin(phase) * 2.8f,
                                                9.8f + spray * 0.6f,
                                                0.45f + std::cos(phase) * 0.18f);
            renderer.drawFilledCircle(particle.x, particle.y, (0.14f + spray * 0.03f) * scale,
                             Color(0.96f, 0.99f, 1.0f, 0.14f + 0.04f * (2 - spray)));
        }
    }
}

// =====================================================================
//  Garbage patch sprite
// =====================================================================

void SeaThemeRenderer::drawGarbagePatchSprite(IsometricRenderer& renderer,
                                               float cx, float cy, float scale,
                                               const Color& bodyColor,
                                               const Color& accentColor,
                                               bool collected,
                                               float fillRatio,
                                               float animationTime) {
    const float sink = collected ? currentGarbageSinkProgress : 0.0f;
    const float alphaScale = clamp01(1.0f - sink * 0.88f);
    const float displayScale = scale * (1.0f - sink * 0.72f);
    const float displayFill = collected
        ? 0.42f * (1.0f - sink)
        : clamp01(fillRatio);
    const float bob = std::sin(animationTime * 1.35f + cx * 0.012f + cy * 0.017f) *
                      0.55f * displayScale;
    cy += bob + sink * 4.6f * scale;

    const Color waterShadow(0.02f, 0.06f, 0.14f, 0.28f * alphaScale);
    const Color foamColor(0.86f, 0.95f, 1.0f, 0.18f * alphaScale);
    const Color debrisBase = shiftColor(bodyColor, -0.04f, alphaScale);
    const Color debrisDark(bodyColor.r * 0.48f, bodyColor.g * 0.48f, bodyColor.b * 0.48f, 0.88f * alphaScale);
    const Color plasticWhite(0.90f, 0.92f, 0.88f, 0.72f * alphaScale);
    const Color plasticBlue(0.30f, 0.50f, 0.82f, 0.82f * alphaScale);
    const Color bottleGreen(0.26f, 0.56f, 0.42f, 0.72f * alphaScale);
    const float sheenMix = 0.5f + 0.5f * std::sin(animationTime * 0.9f + cx * 0.02f - cy * 0.03f);
    const Color sheen = mixColor(Color(0.82f, 0.35f, 0.72f, 0.24f * alphaScale),
                                 Color(0.24f, 0.78f, 0.88f, 0.24f * alphaScale), sheenMix);

    renderer.drawDiamond(cx, cy + 3.0f * displayScale, 10.8f * displayScale, 5.8f * displayScale, waterShadow);
    renderer.drawDiamond(cx + std::sin(animationTime * 1.1f) * 0.3f, cy + 1.2f * displayScale,
                8.2f * displayScale, 4.4f * displayScale,
                Color(0.08f, 0.20f, 0.36f, 0.18f * alphaScale));

    if (alphaScale <= 0.02f) return;

    const float foamPulse = 0.92f + 0.10f * std::sin(animationTime * 2.0f + cx * 0.04f);
    renderer.drawDiamondOutline(cx, cy + 0.9f * displayScale,
                       8.8f * displayScale * foamPulse, 4.6f * displayScale * foamPulse, foamColor, 1.2f);
    renderer.drawDiamondOutline(cx, cy + 1.0f * displayScale,
                       11.0f * displayScale * foamPulse, 5.4f * displayScale * foamPulse,
                       shiftColor(foamColor, -0.04f, 0.70f), 1.0f);

    const float patchW = 8.0f * displayScale * (0.88f + displayFill * 0.40f);
    const float patchH = 6.0f * displayScale * (0.90f + displayFill * 0.32f);
    const float patchDepth = (0.9f + displayFill * 1.7f) * displayScale;
    renderer.drawIsometricBlock(cx, cy + 0.8f * displayScale, patchW, patchH, patchDepth, debrisBase, debrisDark);
    renderer.drawDiamond(cx + 1.0f * displayScale, cy + 0.5f * displayScale,
                4.8f * displayScale, 2.2f * displayScale, sheen);

    auto rotatedQuad = [&](float ox, float oy, float halfW, float halfH,
                           float angle, const Color& color) {
        const float c = std::cos(angle);
        const float sn = std::sin(angle);
        auto pt = [&](float lx, float ly) {
            return IsoCoord{cx + ox + lx * c - ly * sn, cy + oy + lx * sn + ly * c};
        };
        drawImmediateQuad({pt(-halfW, -halfH), pt(halfW, -halfH),
                           pt(halfW, halfH), pt(-halfW, halfH)}, color);
    };

    for (int piece = 0; piece < 5; piece++) {
        const float driftX = std::sin(animationTime * (0.45f + piece * 0.08f) + piece * 1.7f) * (1.4f + piece * 0.18f) * displayScale;
        const float driftY = std::cos(animationTime * (0.52f + piece * 0.06f) + piece * 1.2f) * (0.9f + piece * 0.14f) * displayScale;
        const float angle = animationTime * (0.25f + piece * 0.05f) + piece * 0.7f;
        rotatedQuad(driftX, driftY - patchDepth * 0.35f,
                    (0.65f + piece * 0.08f) * displayScale,
                    (0.42f + piece * 0.05f) * displayScale,
                    angle, (piece % 2 == 0) ? debrisDark : shiftColor(debrisBase, 0.08f));
    }

    for (int bag = 0; bag < 2; bag++) {
        const float flap = std::sin(animationTime * (1.8f + bag * 0.3f) + bag * 2.0f);
        rotatedQuad((-2.2f + bag * 4.2f) * displayScale,
                    (-0.8f - bag * 0.9f) * displayScale - patchDepth * 0.35f,
                    (1.35f + bag * 0.15f) * displayScale,
                    (0.42f + flap * 0.10f) * displayScale,
                    animationTime * 0.8f + bag * 1.1f,
                    Color(0.92f, 0.94f, 0.98f, (0.34f + bag * 0.08f) * alphaScale));
    }

    for (int bottle = 0; bottle < 3; bottle++) {
        const float drift = std::sin(animationTime * (0.9f + bottle * 0.15f) + bottle) * 0.5f;
        const float tilt = 0.55f + std::sin(animationTime * 0.7f + bottle * 1.8f) * 0.35f;
        const float ox = (-2.8f + bottle * 2.7f) * displayScale;
        const float oy = (0.2f + drift + bottle * 0.4f) * displayScale - patchDepth * 0.28f;
        rotatedQuad(ox, oy, 0.30f * displayScale, 1.05f * displayScale, tilt,
                    (bottle == 1) ? plasticBlue : bottleGreen);
        renderer.drawFilledCircle(cx + ox + std::cos(tilt) * 0.55f * displayScale,
                         cy + oy + std::sin(tilt) * 0.55f * displayScale,
                         0.18f * displayScale, Color(0.94f, 0.98f, 1.0f, 0.76f * alphaScale));
    }

    renderer.drawFilledCircle(cx - 1.8f * displayScale, cy - patchDepth * 0.42f, 0.85f * displayScale, plasticWhite);
    renderer.drawFilledCircle(cx + 2.1f * displayScale, cy - patchDepth * 0.48f, 0.72f * displayScale, plasticBlue);
    renderer.drawDiamondOutline(cx, cy + 0.8f * displayScale,
                       patchW * 0.72f, patchH * 0.72f,
                       shiftColor(accentColor, 0.0f, 0.54f * alphaScale), 1.2f);
}

// =====================================================================
//  Decorative islets
// =====================================================================

