#include "visualization/CityThemeRendererInternal.h"

void CityThemeRenderer::drawLandscapeFeature(IsometricRenderer& renderer,
                                             const LandscapeFeature& feature,
                                             float animationTime) const {
    const float minX = feature.x - feature.width * 0.5f;
    const float maxX = feature.x + feature.width * 0.5f;
    const float minY = feature.y - feature.depth * 0.5f;
    const float maxY = feature.y + feature.depth * 0.5f;
    const float zf = currentZoomFactor();
    const float wetBlend =
        (weather == CityWeather::Rainy || weather == CityWeather::Stormy) ? 0.20f : 0.0f;

    switch (feature.type) {
        case LandscapeFeatureType::Lawn: {
            Color lawn = (feature.district == DistrictType::Park)
                ? Color(0.20f, 0.36f, 0.17f, 0.82f)
                : Color(0.24f, 0.34f, 0.18f, 0.78f);
            lawn = mixColor(lawn, Color(0.16f, 0.24f, 0.28f, lawn.a), wetBlend);
            drawWorldQuadPatch(minX, minY, maxX, maxY, lawn);
            drawWorldQuadPatch(feature.x - feature.width * 0.24f,
                               feature.y - feature.depth * 0.24f,
                               feature.x + feature.width * 0.24f,
                               feature.y + feature.depth * 0.24f,
                               withAlpha(scaleColor(lawn, 1.06f), 0.26f));
            break;
        }
        case LandscapeFeatureType::Path: {
            Color path = Color(0.54f, 0.52f, 0.46f, 0.48f);
            path = mixColor(path, Color(0.28f, 0.32f, 0.36f, path.a), wetBlend * 0.85f);
            drawWorldQuadPatch(minX, minY, maxX, maxY, path);
            break;
        }
        case LandscapeFeatureType::Plaza: {
            Color plaza = (feature.district == DistrictType::Landmark)
                ? Color(0.44f, 0.46f, 0.50f, 0.54f)
                : Color(0.34f, 0.34f, 0.32f, 0.48f);
            plaza = mixColor(plaza, Color(0.22f, 0.26f, 0.30f, plaza.a), wetBlend);
            drawWorldQuadPatch(minX, minY, maxX, maxY, plaza);
            break;
        }
        case LandscapeFeatureType::Water: {
            Color water = mixColor(Color(0.08f, 0.34f, 0.48f, 0.58f),
                                   Color(0.12f, 0.48f, 0.62f, 0.62f),
                                   0.5f + 0.5f * std::sin(animationTime * 0.9f + feature.x));
            drawWorldQuadPatch(minX, minY, maxX, maxY, water);
            const IsoCoord iso = RenderUtils::worldToIso(feature.x, feature.y);
            renderer.drawDiamond(iso.x, iso.y, feature.width * 10.0f * zf,
                                 feature.depth * 4.8f * zf,
                                 Color(0.72f, 0.90f, 0.98f, 0.08f));
            break;
        }
        case LandscapeFeatureType::Court: {
            const bool campusCourt = feature.district == DistrictType::Campus;
            const Color court = campusCourt
                ? Color(0.22f, 0.44f, 0.26f, 0.64f)
                : Color(0.32f, 0.22f, 0.16f, 0.56f);
            drawWorldQuadPatch(minX, minY, maxX, maxY, court);
            const IsoCoord p0 = RenderUtils::worldToIso(minX, minY);
            const IsoCoord p1 = RenderUtils::worldToIso(maxX, minY);
            const IsoCoord p2 = RenderUtils::worldToIso(maxX, maxY);
            const IsoCoord p3 = RenderUtils::worldToIso(minX, maxY);
            renderer.drawLine(p0.x, p0.y, p1.x, p1.y, Color(0.92f, 0.96f, 0.90f, 0.20f), 1.0f);
            renderer.drawLine(p1.x, p1.y, p2.x, p2.y, Color(0.92f, 0.96f, 0.90f, 0.20f), 1.0f);
            renderer.drawLine(p2.x, p2.y, p3.x, p3.y, Color(0.92f, 0.96f, 0.90f, 0.20f), 1.0f);
            renderer.drawLine(p3.x, p3.y, p0.x, p0.y, Color(0.92f, 0.96f, 0.90f, 0.20f), 1.0f);
            break;
        }
        case LandscapeFeatureType::Grove: {
            const float pulse = 0.94f + 0.06f * std::sin(animationTime * 1.6f + feature.accent * 8.0f);
            const std::array<PlaybackPoint, 3> offsets{{
                {feature.width * -0.16f, feature.depth * -0.08f},
                {feature.width * 0.18f, feature.depth * 0.04f},
                {feature.width * -0.02f, feature.depth * 0.18f}
            }};
            for (const PlaybackPoint& offset : offsets) {
                const float sway = std::sin(animationTime * 1.3f + offset.x * 7.0f) * 0.45f * zf;
                Color canopyBase(0.22f * pulse, 0.40f * pulse, 0.18f * pulse, 0.78f);
                Color canopyAccent(0.38f * pulse, 0.58f * pulse, 0.24f * pulse, 0.22f);
                applySeasonalTreeCanopy(season, hasSnowfall(), canopyBase, canopyAccent);
                const TreePalette palette{
                    Color(0.02f, 0.04f, 0.02f, 0.16f),
                    Color(0.36f, 0.28f, 0.18f, 0.70f),
                    Color(0.26f, 0.20f, 0.12f, 0.72f),
                    Color(0.34f, 0.24f, 0.14f, 0.72f),
                    canopyBase,
                    scaleColor(biasColor(canopyBase, -0.06f, -0.04f, 0.00f), 0.84f),
                    scaleColor(biasColor(canopyBase, -0.04f, -0.02f, -0.01f), 0.92f),
                    canopyAccent
                };
                drawStylizedTree(renderer,
                                 feature.x + offset.x, feature.y + offset.y,
                                 zf,
                                 1.10f + feature.accent * 0.55f,
                                 0.96f + feature.accent * 0.35f,
                                 sway,
                                 palette);
            }
            break;
        }
    }
}

void CityThemeRenderer::drawBuildingLot(IsometricRenderer& renderer,
                                        const BuildingLot& building) const {
    const IsoCoord iso = RenderUtils::worldToIso(building.x, building.y);
    const ZoneVisibility zone = computeZoneVisibility(
        building.x, building.y,
        operationalCenterX, operationalCenterY,
        operationalRadiusX, operationalRadiusY);
    if (zone.transition <= 0.08f) {
        return;
    }
    const float zf = currentZoomFactor();

    Color material(0.42f, 0.46f, 0.52f, 0.98f);
    Color accent(0.58f, 0.64f, 0.72f, 0.98f);
    switch (building.family) {
        case BuildingFamily::Utility:
            material = Color(0.42f, 0.38f, 0.32f, 0.98f);
            accent = Color(0.62f, 0.56f, 0.44f, 0.98f);
            break;
        case BuildingFamily::Commercial:
            material = Color(0.30f, 0.40f, 0.54f, 0.98f);
            accent = Color(0.56f, 0.72f, 0.88f, 0.98f);
            break;
        case BuildingFamily::Residential:
            material = Color(0.54f, 0.44f, 0.38f, 0.98f);
            accent = Color(0.82f, 0.68f, 0.56f, 0.98f);
            break;
        case BuildingFamily::Civic:
            material = Color(0.50f, 0.52f, 0.48f, 0.98f);
            accent = Color(0.82f, 0.84f, 0.78f, 0.98f);
            break;
        case BuildingFamily::Landmark:
            material = Color(0.58f, 0.64f, 0.72f, 0.98f);
            accent = Color(0.92f, 0.96f, 1.0f, 0.98f);
            break;
    }

    material = biasColor(material,
                         building.hueShift * 0.35f,
                         building.hueShift * 0.14f,
                         -building.hueShift * 0.16f);
    accent = biasColor(accent,
                       building.hueShift * 0.26f,
                       building.hueShift * 0.10f,
                       -building.hueShift * 0.12f);

    const float brightness = building.facadeBrightness *
        (building.visualTier == VisualTier::Focus ? 1.15f : 1.02f);
    const Color facade = mixColor(material, accent, 0.35f + building.saturation * 0.6f);
    const Color roof = scaleColor(mixColor(facade, Color(0.82f, 0.86f, 0.92f, 0.98f), 0.18f),
                                  0.88f + brightness * 0.45f);
    const Color leftFacade = biasColor(facade, -0.04f, -0.02f, 0.05f);
    const Color rightFacade = biasColor(facade, 0.04f, 0.02f, -0.03f);
    const Color leftSide = scaleColor(leftFacade,
                                      0.72f + brightness * 0.30f +
                                      building.edgeHighlight * 0.08f);
    const Color rightSide = scaleColor(rightFacade,
                                       0.92f + brightness * 0.34f +
                                       building.edgeHighlight * 0.12f);
    const Color outline = withAlpha(scaleColor(Color(0.92f, 0.94f, 0.98f, 1.0f),
                                               0.55f + building.edgeHighlight * 0.45f),
                                    0.42f + building.edgeHighlight * 0.35f);
    const Color windowColor = withAlpha(
        mixColor(Color(0.54f, 0.66f, 0.82f, 1.0f),
                 Color(0.96f, 0.72f, 0.34f, 1.0f),
                 building.windowWarmth),
        (0.06f + building.windowDensity * 0.12f) *
            (building.visualTier == VisualTier::Peripheral ? 0.72f : 1.0f));

    const Color voidTint(0.01f, 0.02f, 0.04f, 1.0f);
    const Color attenuatedRoof = attenuateToZone(
        roof, zone, voidTint,
        building.landmark ? 0.22f : 0.08f,
        building.landmark ? 0.12f : 0.32f,
        0.08f);
    const Color attenuatedLeft = attenuateToZone(
        leftSide, zone, voidTint,
        building.landmark ? 0.18f : 0.06f,
        0.38f,
        0.08f);
    const Color attenuatedRight = attenuateToZone(
        rightSide, zone, voidTint,
        building.landmark ? 0.20f : 0.08f,
        0.34f,
        0.08f);
    const Color attenuatedOutline = attenuateToZone(
        outline, zone, Color(0.00f, 0.00f, 0.00f, 1.0f),
        0.20f,
        0.26f,
        0.03f);
    const Color attenuatedWindow = attenuateToZone(
        windowColor, zone, voidTint,
        0.24f,
        0.16f,
        0.01f);
    const Color shadowColor = attenuateToZone(
        Color(0.02f, 0.02f, 0.03f, 0.24f), zone, Color(0.00f, 0.00f, 0.00f, 0.24f),
        0.00f,
        0.00f,
        0.02f);

    const float baseWidth = std::max(4.0f * zf, building.width * 38.0f * zf);
    const float baseDepth = std::max(3.0f * zf, building.depth * 28.0f * zf);
    const float totalHeight = std::max(
        6.0f * zf,
        building.height * (building.form == BuildingForm::TwinTower ? 1.46f : 1.32f) * zf);
    float groundCenterOffset = 1.0f * zf;
    switch (building.form) {
        case BuildingForm::TwinTower:
            groundCenterOffset = 2.2f * zf;
            break;
        case BuildingForm::Terrace:
            groundCenterOffset = 2.0f * zf;
            break;
        case BuildingForm::Tower:
            groundCenterOffset = 1.4f * zf;
            break;
        case BuildingForm::Courtyard:
            groundCenterOffset = 0.8f * zf;
            break;
        case BuildingForm::Pavilion:
        case BuildingForm::Slab:
        default:
            break;
    }
    const float groundCenterY = iso.y + groundCenterOffset;
    const float shadowDepth = baseDepth * 0.52f;
    const float shadowScale = (building.visualTier == VisualTier::Focus) ? 1.12f : 1.04f;
    if (season == CitySeason::Winter) {
        const float snowBlend = hasSnowfall() ? 0.82f : 0.52f;
        const Color snowGround = attenuateToZone(
            mixColor(Color(0.20f, 0.22f, 0.25f, 0.30f),
                     Color(0.92f, 0.94f, 0.98f, 0.84f),
                     snowBlend),
            zone,
            voidTint,
            0.32f,
            0.04f,
            0.10f);
        renderer.drawDiamond(iso.x, groundCenterY + shadowDepth * 0.22f,
                             baseWidth * 1.16f, baseDepth * 0.88f,
                             snowGround);
        renderer.drawDiamond(iso.x, groundCenterY + shadowDepth * 0.12f,
                             baseWidth * 0.86f, baseDepth * 0.64f,
                             withAlpha(scaleColor(snowGround, 1.04f), snowGround.a * 0.55f));
    }
    renderer.drawDiamond(iso.x, groundCenterY + shadowDepth, baseWidth * shadowScale,
                         shadowDepth, shadowColor);

    auto drawMass = [&](float offsetX, float offsetY,
                        float width, float depth, float height,
                        float windowScale) -> BlockFaces {
        const BlockFaces faces = makeBlockFaces(iso.x + offsetX, groundCenterY + offsetY,
                                                width, depth, height);
        drawBlockMass(faces, attenuatedRoof, attenuatedLeft, attenuatedRight);

        const int bands = std::max(1, static_cast<int>(std::round(
            height / (5.4f * zf) * building.windowDensity * windowScale)));
        drawFaceBands(faces.left, bands,
                      0.10f, withAlpha(attenuatedWindow, attenuatedWindow.a * building.occupancy * 0.82f),
                      0.8f * zf + 0.35f);
        drawFaceBands(faces.right, bands + (building.family == BuildingFamily::Commercial ? 1 : 0),
                      0.12f, withAlpha(attenuatedWindow, attenuatedWindow.a * building.occupancy),
                      0.8f * zf + 0.35f);
        drawOutlineLoop(faces.top, attenuatedOutline, std::max(0.8f, 0.85f * zf));
        renderer.drawLine(faces.top[1].x, faces.top[1].y, faces.right[2].x, faces.right[2].y,
                          attenuatedOutline, std::max(0.8f, 0.9f * zf));
        renderer.drawLine(faces.top[3].x, faces.top[3].y, faces.left[2].x, faces.left[2].y,
                          attenuatedOutline, std::max(0.8f, 0.9f * zf));
        return faces;
    };

    const float podiumHeight = std::max(2.4f * zf, totalHeight * building.podiumRatio);
    const float mainHeight = std::max(4.0f * zf, totalHeight - podiumHeight);

    auto drawRoofAccent = [&](const BlockFaces& faces,
                              float offsetY,
                              float width,
                              float depth,
                              float height,
                              float windowScale) {
        switch (building.roofProfile) {
            case RoofProfile::Flat:
                drawMass(baseWidth * 0.04f, offsetY,
                         width * 0.26f, depth * 0.24f,
                         std::max(1.4f * zf, height * 0.10f), windowScale * 0.25f);
                break;
            case RoofProfile::Stepped:
                drawMass(0.0f, offsetY,
                         width * 0.70f, depth * 0.70f,
                         std::max(1.8f * zf, height * 0.18f), windowScale * 0.38f);
                break;
            case RoofProfile::Terrace:
                drawMass(-width * 0.12f, offsetY + 0.8f * zf,
                         width * 0.56f, depth * 0.58f,
                         std::max(1.8f * zf, height * 0.14f), windowScale * 0.32f);
                renderer.drawDiamond(faces.top[0].x,
                                     faces.top[0].y + depth * 0.48f,
                                     width * 0.12f, depth * 0.06f,
                                     attenuateToZone(
                                         Color(0.94f, 0.74f, 0.40f, 0.10f),
                                         zone,
                                         Color(0.02f, 0.02f, 0.04f, 0.10f),
                                         0.30f,
                                         0.08f,
                                         0.04f));
                break;
            case RoofProfile::Crown:
                drawMass(0.0f, offsetY,
                         width * 0.34f, depth * 0.34f,
                         std::max(2.0f * zf, height * 0.12f), windowScale * 0.40f);
                renderer.drawLine(faces.top[0].x, faces.top[0].y,
                                  faces.top[0].x, faces.top[0].y - 7.0f * zf,
                                  attenuateToZone(
                                      Color(0.82f, 0.88f, 0.96f, 0.18f),
                                      zone,
                                      Color(0.02f, 0.03f, 0.05f, 0.18f),
                                      0.34f,
                                      0.08f,
                                      0.05f), 1.0f);
                break;
            case RoofProfile::Spire:
                drawMass(0.0f, offsetY,
                         width * 0.30f, depth * 0.32f,
                         std::max(1.8f * zf, height * 0.10f), windowScale * 0.40f);
                renderer.drawLine(faces.top[0].x, faces.top[0].y - 1.0f * zf,
                                  faces.top[0].x, faces.top[0].y - 12.0f * zf,
                                  attenuateToZone(
                                      Color(0.90f, 0.96f, 1.0f, 0.28f),
                                      zone,
                                      Color(0.02f, 0.03f, 0.05f, 0.28f),
                                      0.38f,
                                      0.06f,
                                      0.06f), 1.2f);
                break;
        }
    };

    switch (building.form) {
        case BuildingForm::Pavilion: {
            const BlockFaces faces = drawMass(0.0f, 1.0f * zf,
                                              baseWidth * 0.88f, baseDepth * 0.82f,
                                              podiumHeight + mainHeight * 0.36f, 0.42f);
            drawRoofAccent(faces,
                           -podiumHeight - mainHeight * 0.36f,
                           baseWidth * 0.88f, baseDepth * 0.82f,
                           podiumHeight, 0.24f);
            break;
        }

        case BuildingForm::Terrace: {
            const int units = 3 + (building.annexRatio > 0.30f ? 1 : 0);
            for (int i = 0; i < units; ++i) {
                const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(units);
                const float offsetX = RenderUtils::lerp(-baseWidth * 0.28f, baseWidth * 0.28f, t);
                const float width = baseWidth * (0.18f + (i % 2 == 0 ? 0.02f : -0.01f));
                const float height = podiumHeight * 0.82f + mainHeight * (0.38f + 0.06f * (i % 2));
                drawMass(offsetX, 2.0f * zf,
                         width, baseDepth * 0.40f,
                         height, 0.30f);
            }
            break;
        }

        case BuildingForm::Slab: {
            const BlockFaces slabFaces = drawMass(0.0f, 1.0f * zf,
                                                  baseWidth * 0.96f, baseDepth * 0.86f,
                                                  podiumHeight * 0.78f + mainHeight * 0.56f,
                                                  0.72f);
            if (building.family == BuildingFamily::Civic) {
                drawMass(-baseWidth * 0.22f, -podiumHeight * 0.10f,
                         baseWidth * 0.36f, baseDepth * 0.34f,
                         mainHeight * 0.24f, 0.22f);
            } else if (building.annexRatio > 0.28f) {
                drawMass(baseWidth * 0.22f, 3.2f * zf,
                         baseWidth * 0.30f, baseDepth * 0.28f,
                         podiumHeight * 0.28f, 0.18f);
            }
            drawRoofAccent(slabFaces,
                           -(podiumHeight * 0.78f + mainHeight * 0.56f),
                           baseWidth * 0.96f, baseDepth * 0.86f,
                           mainHeight, 0.30f);
            break;
        }

        case BuildingForm::Courtyard: {
            const float wingHeight = podiumHeight * 0.70f + mainHeight * 0.46f;
            const float wingW = baseWidth * 0.28f;
            const float wingD = baseDepth * 0.26f;
            drawMass(-baseWidth * 0.22f, 0.8f * zf, wingW, baseDepth * 0.72f, wingHeight, 0.42f);
            drawMass(baseWidth * 0.22f, 0.8f * zf, wingW, baseDepth * 0.72f, wingHeight, 0.42f);
            drawMass(0.0f, -baseDepth * 0.18f, baseWidth * 0.70f, wingD,
                     wingHeight * 0.92f, 0.34f);
            const IsoCoord courtIso = RenderUtils::worldToIso(building.x, building.y + building.depth * 0.10f);
            renderer.drawDiamond(courtIso.x, courtIso.y + groundCenterOffset,
                                 baseWidth * 0.16f, baseDepth * 0.08f,
                                 Color(0.26f, 0.34f, 0.22f, 0.16f));
            break;
        }

        case BuildingForm::TwinTower: {
            const float podiumW = baseWidth * 1.12f;
            const float podiumD = baseDepth * 0.94f;
            drawMass(0.0f, 2.2f * zf, podiumW, podiumD, podiumHeight * 0.92f, 0.34f);

            const float towerOffset = baseWidth * 0.18f;
            const float towerW = baseWidth * 0.34f;
            const float towerD = baseDepth * 0.32f;
            const float towerH = mainHeight * 0.96f;
            const BlockFaces leftTower = drawMass(-towerOffset, -podiumHeight * 0.90f,
                                                  towerW, towerD, towerH, 1.10f);
            const BlockFaces rightTower = drawMass(towerOffset, -podiumHeight * 0.90f,
                                                   towerW, towerD, towerH * 0.98f, 1.10f);

            drawMass(-towerOffset, -podiumHeight - towerH,
                     towerW * 0.76f, towerD * 0.76f,
                     towerH * 0.18f, 0.46f);
            drawMass(towerOffset, -podiumHeight - towerH * 0.98f,
                     towerW * 0.76f, towerD * 0.76f,
                     towerH * 0.18f, 0.46f);

            const float bridgeY = groundCenterY - podiumHeight - towerH * 0.46f;
            glColor4f(0.82f, 0.88f, 0.96f, 0.46f);
            glBegin(GL_QUADS);
            glVertex2f(iso.x - towerOffset * 0.74f, bridgeY - 3.2f * zf);
            glVertex2f(iso.x + towerOffset * 0.74f, bridgeY - 3.2f * zf);
            glVertex2f(iso.x + towerOffset * 0.68f, bridgeY + 2.4f * zf);
            glVertex2f(iso.x - towerOffset * 0.68f, bridgeY + 2.4f * zf);
            glEnd();

            renderer.drawLine(leftTower.top[0].x, leftTower.top[0].y - 1.0f * zf,
                              leftTower.top[0].x, leftTower.top[0].y - 14.0f * zf,
                              attenuateToZone(
                                  Color(0.94f, 0.98f, 1.0f, 0.30f),
                                  zone,
                                  Color(0.02f, 0.03f, 0.05f, 0.30f),
                                  0.40f,
                                  0.06f,
                                  0.08f), 1.2f);
            renderer.drawLine(rightTower.top[0].x, rightTower.top[0].y - 1.0f * zf,
                              rightTower.top[0].x, rightTower.top[0].y - 13.0f * zf,
                              attenuateToZone(
                                  Color(0.94f, 0.98f, 1.0f, 0.30f),
                                  zone,
                                  Color(0.02f, 0.03f, 0.05f, 0.30f),
                                  0.40f,
                                  0.06f,
                                  0.08f), 1.2f);
            renderer.drawDiamond(iso.x, groundCenterY - podiumHeight - towerH * 0.44f,
                                 baseWidth * 0.18f, baseDepth * 0.06f,
                                 attenuateToZone(
                                      Color(0.82f, 0.90f, 0.98f, 0.10f),
                                     zone,
                                     Color(0.02f, 0.03f, 0.05f, 0.10f),
                                     0.28f,
                                     0.08f,
                                     0.04f));
            break;
        }

        case BuildingForm::Tower:
        default: {
            drawMass(0.0f, 1.4f * zf, baseWidth, baseDepth, podiumHeight, 0.38f);

            float towerWidth = baseWidth * building.taper;
            float towerDepth = baseDepth * (building.taper + 0.02f);
            if (building.family == BuildingFamily::Residential) {
                towerWidth = baseWidth * (building.taper - 0.08f);
                towerDepth = baseDepth * (building.taper - 0.06f);
            }
            if (building.family == BuildingFamily::Landmark) {
                towerWidth = baseWidth * (building.taper - 0.02f);
                towerDepth = baseDepth * (building.taper - 0.02f);
            }

            const BlockFaces mainFaces = drawMass(0.0f, -podiumHeight,
                                                  towerWidth, towerDepth, mainHeight, 1.0f);

            if (building.family == BuildingFamily::Residential) {
                drawMass(-baseWidth * 0.20f, 3.2f * zf,
                         baseWidth * 0.28f, baseDepth * 0.34f,
                         std::max(2.2f * zf, podiumHeight * 0.28f), 0.22f);
            } else if (building.family == BuildingFamily::Commercial && building.annexRatio > 0.22f) {
                drawMass(baseWidth * 0.20f, 3.0f * zf,
                         baseWidth * 0.26f, baseDepth * 0.24f,
                         std::max(2.0f * zf, podiumHeight * 0.24f), 0.18f);
            }

            drawRoofAccent(mainFaces,
                           -podiumHeight - mainHeight,
                           towerWidth, towerDepth,
                           mainHeight, 0.40f);
            break;
        }
    }

    if (building.visualTier == VisualTier::Focus) {
        renderer.drawDiamond(iso.x, groundCenterY - totalHeight - 2.0f * zf,
                             baseWidth * 0.18f, baseDepth * 0.08f,
                             attenuateToZone(
                                  Color(0.70f, 0.82f, 0.96f, 0.04f + building.edgeHighlight * 0.05f),
                                 zone,
                                 Color(0.02f, 0.03f, 0.05f, 0.08f),
                                 0.22f,
                                 0.06f,
                                 0.03f));
    }
}

// ============================================================================
// ASSET LIBRARY DRAW FUNCTIONS
// ============================================================================

void CityThemeRenderer::drawPresetBuilding(IsometricRenderer& renderer,
                                            const PlacedPresetBuilding& placed) const {
    const CityAssets::BuildingPreset& p = *placed.preset;
    const IsoCoord iso = RenderUtils::worldToIso(placed.worldX, placed.worldY);
    const ZoneVisibility zone = computeZoneVisibility(
        placed.worldX, placed.worldY,
        operationalCenterX, operationalCenterY,
        operationalRadiusX, operationalRadiusY);
    if (zone.transition <= 0.10f) {
        return;
    }
    const float zf = currentZoomFactor();
    auto emitDiamond = [&](float cx, float cy, float w, float h, const Color& color) {
        drawOrAppendDiamond(renderer, cx, cy, w, h, color);
    };
    auto emitLine = [&](float x1, float y1, float x2, float y2,
                        const Color& color, float width) {
        drawOrAppendLine(renderer, x1, y1, x2, y2, color, width);
    };

    // Compute authored dual-tone colors from preset
    const Color facade = mixColor(p.material, p.accent, 0.40f);
    const float brightness = (p.category == CityAssets::BuildingCategory::Landmark) ? 1.18f : 1.04f;

    const Color roof = scaleColor(
        mixColor(facade, Color(0.84f, 0.88f, 0.94f, 0.98f), 0.20f),
        0.90f + brightness * 0.40f);
    const Color leftSide = scaleColor(
        biasColor(facade, p.leftBias.dr, p.leftBias.dg, p.leftBias.db),
        0.74f + brightness * 0.28f);
    const Color rightSide = scaleColor(
        biasColor(facade, p.rightBias.dr, p.rightBias.dg, p.rightBias.db),
        0.94f + brightness * 0.32f);
    const Color outline = withAlpha(
        scaleColor(Color(0.92f, 0.94f, 0.98f, 1.0f), 0.60f),
        0.44f);
    const Color windowColor = withAlpha(
        mixColor(Color(0.54f, 0.66f, 0.82f, 1.0f),
                 Color(0.96f, 0.72f, 0.34f, 1.0f),
                 p.windowWarmth),
        0.06f + p.windowDensity * 0.12f);

    const Color voidTint(0.01f, 0.02f, 0.04f, 1.0f);
    const Color attenuatedRoof = attenuateToZone(
        roof, zone, voidTint,
        p.category == CityAssets::BuildingCategory::Landmark ? 0.22f : 0.08f,
        p.category == CityAssets::BuildingCategory::Landmark ? 0.12f : 0.30f,
        0.08f);
    const Color attenuatedLeft = attenuateToZone(
        leftSide, zone, voidTint,
        p.category == CityAssets::BuildingCategory::Landmark ? 0.18f : 0.06f,
        0.36f,
        0.08f);
    const Color attenuatedRight = attenuateToZone(
        rightSide, zone, voidTint,
        p.category == CityAssets::BuildingCategory::Landmark ? 0.20f : 0.08f,
        0.32f,
        0.08f);
    const Color attenuatedOutline = attenuateToZone(
        outline, zone, Color(0.00f, 0.00f, 0.00f, 1.0f),
        0.20f,
        0.24f,
        0.03f);
    const Color attenuatedWindow = attenuateToZone(
        windowColor, zone, voidTint,
        0.24f,
        0.14f,
        0.01f);
    const Color shadowColor = attenuateToZone(
        Color(0.02f, 0.02f, 0.03f, 0.24f), zone, Color(0.00f, 0.00f, 0.00f, 0.24f),
        0.00f,
        0.00f,
        0.02f);

    const float baseWidth = std::max(4.0f * zf, p.width * 38.0f * zf);
    const float baseDepth = std::max(3.0f * zf, p.depth * 28.0f * zf);
    const float totalHeight = std::max(6.0f * zf,
        p.height * placed.heightScale * (p.isTwinTower ? 1.46f : 1.32f) * zf);
    const float groundCenterOffset = p.isTwinTower ? 2.2f * zf : 1.4f * zf;
    const float groundCenterY = iso.y + groundCenterOffset;
    const float shadowDepth = baseDepth * 0.52f;

    if (season == CitySeason::Winter) {
        const float snowBlend = hasSnowfall() ? 0.82f : 0.52f;
        const Color snowGround = attenuateToZone(
            mixColor(Color(0.20f, 0.22f, 0.25f, 0.30f),
                     Color(0.92f, 0.94f, 0.98f, 0.84f),
                     snowBlend),
            zone,
            voidTint,
            0.32f,
            0.04f,
            0.10f);
        emitDiamond(iso.x, groundCenterY + shadowDepth * 0.22f,
                    baseWidth * 1.16f, baseDepth * 0.88f,
                    snowGround);
        emitDiamond(iso.x, groundCenterY + shadowDepth * 0.12f,
                    baseWidth * 0.86f, baseDepth * 0.64f,
                    withAlpha(scaleColor(snowGround, 1.04f), snowGround.a * 0.55f));
    }

    // Shadow
    const float shadowScale = (p.category == CityAssets::BuildingCategory::Landmark) ? 1.14f : 1.06f;
    emitDiamond(iso.x, groundCenterY + shadowDepth, baseWidth * shadowScale,
                shadowDepth, shadowColor);

    // Drawing helper
    auto drawMass = [&](float offsetX, float offsetY,
                        float w, float d, float h,
                        float windowScale) -> BlockFaces {
        const BlockFaces faces = makeBlockFaces(iso.x + offsetX, groundCenterY + offsetY,
                                                w, d, h);
        drawBlockMass(faces, attenuatedRoof, attenuatedLeft, attenuatedRight);

        const int bands = std::max(1, static_cast<int>(std::round(
            h / (5.4f * zf) * p.windowDensity * windowScale)));
        drawFaceBands(faces.left, bands, 0.10f,
                      withAlpha(attenuatedWindow, attenuatedWindow.a * 0.82f),
                      0.8f * zf + 0.35f);
        drawFaceBands(faces.right, bands, 0.12f,
                      attenuatedWindow, 0.8f * zf + 0.35f);
        drawOutlineLoop(faces.top, attenuatedOutline, std::max(0.8f, 0.85f * zf));
        emitLine(faces.top[1].x, faces.top[1].y,
                 faces.right[2].x, faces.right[2].y,
                 attenuatedOutline, std::max(0.8f, 0.9f * zf));
        emitLine(faces.top[3].x, faces.top[3].y,
                 faces.left[2].x, faces.left[2].y,
                 attenuatedOutline, std::max(0.8f, 0.9f * zf));
        return faces;
    };

    const float podiumHeight = std::max(2.4f * zf, totalHeight * p.podiumRatio);
    const float mainHeight = std::max(4.0f * zf, totalHeight - podiumHeight);

    if (p.isTwinTower) {
        // === TWIN TOWER FORM ===
        const float podiumW = baseWidth * 1.14f;
        const float podiumD = baseDepth * 0.96f;
        drawMass(0.0f, 2.2f * zf, podiumW, podiumD, podiumHeight * 0.92f, 0.34f);

        const float towerOffset = baseWidth * 0.20f;
        const float towerW = baseWidth * 0.36f;
        const float towerD = baseDepth * 0.34f;
        const float towerH = mainHeight * 0.96f;
        const BlockFaces leftTower = drawMass(-towerOffset, -podiumHeight * 0.90f,
                                              towerW, towerD, towerH, 1.12f);
        const BlockFaces rightTower = drawMass(towerOffset, -podiumHeight * 0.90f,
                                               towerW, towerD, towerH * 0.98f, 1.12f);

        // Upper tier narrowing
        drawMass(-towerOffset, -podiumHeight - towerH,
                 towerW * 0.74f, towerD * 0.74f,
                 towerH * 0.20f, 0.48f);
        drawMass(towerOffset, -podiumHeight - towerH * 0.98f,
                 towerW * 0.74f, towerD * 0.74f,
                 towerH * 0.20f, 0.48f);

        // Third tier
        drawMass(-towerOffset, -podiumHeight - towerH * 1.20f,
                 towerW * 0.52f, towerD * 0.52f,
                 towerH * 0.10f, 0.30f);
        drawMass(towerOffset, -podiumHeight - towerH * 1.18f,
                 towerW * 0.52f, towerD * 0.52f,
                 towerH * 0.10f, 0.30f);

        // Skybridge
        const float bridgeY = groundCenterY - podiumHeight - towerH * 0.46f;
        drawImmediateQuad({
            IsoCoord{iso.x - towerOffset * 0.76f, bridgeY - 3.4f * zf},
            IsoCoord{iso.x + towerOffset * 0.76f, bridgeY - 3.4f * zf},
            IsoCoord{iso.x + towerOffset * 0.70f, bridgeY + 2.6f * zf},
            IsoCoord{iso.x - towerOffset * 0.70f, bridgeY + 2.6f * zf}
        }, Color(0.84f, 0.90f, 0.98f, 0.50f));

        // Spires
        emitLine(leftTower.top[0].x, leftTower.top[0].y - 1.0f * zf,
                 leftTower.top[0].x, leftTower.top[0].y - 18.0f * zf,
                 attenuateToZone(
                     Color(0.94f, 0.98f, 1.0f, 0.34f),
                     zone,
                     Color(0.02f, 0.03f, 0.05f, 0.34f),
                     0.40f,
                     0.06f,
                     0.08f), 1.4f);
        emitLine(rightTower.top[0].x, rightTower.top[0].y - 1.0f * zf,
                 rightTower.top[0].x, rightTower.top[0].y - 17.0f * zf,
                 attenuateToZone(
                     Color(0.94f, 0.98f, 1.0f, 0.34f),
                     zone,
                     Color(0.02f, 0.03f, 0.05f, 0.34f),
                     0.40f,
                     0.06f,
                     0.08f), 1.4f);

        // Glow halo at top
        emitDiamond(iso.x, groundCenterY - podiumHeight - towerH * 0.44f,
                    baseWidth * 0.20f, baseDepth * 0.08f,
                    attenuateToZone(
                         Color(0.84f, 0.92f, 0.98f, 0.12f),
                        zone,
                        Color(0.02f, 0.03f, 0.05f, 0.12f),
                        0.28f,
                        0.08f,
                        0.04f));
    } else {
        // === STANDARD BUILDING FORM ===
        // Podium base
        drawMass(0.0f, 1.4f * zf, baseWidth, baseDepth, podiumHeight, 0.38f);

        // Main tower
        const float towerWidth = baseWidth * p.taper;
        const float towerDepth = baseDepth * (p.taper + 0.02f);
        const BlockFaces mainFaces = drawMass(0.0f, -podiumHeight,
                                              towerWidth, towerDepth, mainHeight, 1.0f);

        // Roof accent based on preset style
        switch (p.roof) {
            case CityAssets::RoofStyle::Flat:
                drawMass(baseWidth * 0.04f, -podiumHeight - mainHeight,
                         towerWidth * 0.26f, towerDepth * 0.24f,
                         std::max(1.4f * zf, mainHeight * 0.10f), 0.25f);
                break;
            case CityAssets::RoofStyle::Stepped:
                drawMass(0.0f, -podiumHeight - mainHeight,
                         towerWidth * 0.72f, towerDepth * 0.72f,
                         std::max(1.8f * zf, mainHeight * 0.18f), 0.38f);
                break;
            case CityAssets::RoofStyle::Crown:
                drawMass(0.0f, -podiumHeight - mainHeight,
                         towerWidth * 0.36f, towerDepth * 0.36f,
                         std::max(2.0f * zf, mainHeight * 0.12f), 0.40f);
                emitLine(mainFaces.top[0].x, mainFaces.top[0].y,
                         mainFaces.top[0].x, mainFaces.top[0].y - 7.0f * zf,
                         attenuateToZone(
                             Color(0.82f, 0.88f, 0.96f, 0.20f),
                             zone,
                             Color(0.02f, 0.03f, 0.05f, 0.20f),
                             0.34f,
                             0.08f,
                             0.05f), 1.0f);
                break;
            case CityAssets::RoofStyle::Spire:
                drawMass(0.0f, -podiumHeight - mainHeight,
                         towerWidth * 0.30f, towerDepth * 0.32f,
                         std::max(1.8f * zf, mainHeight * 0.10f), 0.40f);
                emitLine(mainFaces.top[0].x, mainFaces.top[0].y - 1.0f * zf,
                         mainFaces.top[0].x, mainFaces.top[0].y - 14.0f * zf,
                         attenuateToZone(
                             Color(0.90f, 0.96f, 1.0f, 0.30f),
                             zone,
                             Color(0.02f, 0.03f, 0.05f, 0.30f),
                             0.38f,
                             0.06f,
                             0.06f), 1.2f);
                break;
        }
    }

    // Focus glow for landmarks
    if (p.category == CityAssets::BuildingCategory::Landmark ||
        p.category == CityAssets::BuildingCategory::OfficeTower) {
        emitDiamond(iso.x, groundCenterY - totalHeight - 2.0f * zf,
                    baseWidth * 0.18f, baseDepth * 0.08f,
                    attenuateToZone(
                         Color(0.70f, 0.82f, 0.96f, 0.04f),
                        zone,
                        Color(0.02f, 0.03f, 0.05f, 0.08f),
                        0.22f,
                        0.06f,
                        0.03f));
    }
}

