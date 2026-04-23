#include "CityThemeRendererInternal.h"

void CityThemeRenderer::generateBuildings(const MapGraph& graph, std::mt19937& rng) {
    (void)graph;
    (void)rng;

    buildings.clear();
    landscapeFeatures.clear();
    return;
}

void CityThemeRenderer::populateFromAssetLibrary(const MapGraph& graph,
                                                  std::mt19937& rng) {
    presetBuildings.clear();
    presetEnvironments.clear();
    presetVehicles.clear();
    presetRoadProps.clear();
    buildings.clear();
    landscapeFeatures.clear();

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> authoredJitter(-0.08f, 0.08f);
    std::uniform_real_distribution<float> slotJitter(-0.06f, 0.06f);

    const std::vector<PlaybackPoint> baseSlots{
        {-0.32f, -0.24f}, { 0.00f, -0.26f}, { 0.30f, -0.22f},
        {-0.34f,  0.00f}, {-0.04f,  0.02f}, { 0.28f,  0.00f},
        {-0.30f,  0.24f}, { 0.02f,  0.26f}, { 0.32f,  0.22f}
    };
    const std::vector<PlaybackPoint> denseEdgeSlots{
        {-0.46f, -0.30f}, {-0.16f, -0.40f}, { 0.16f, -0.40f}, { 0.44f, -0.28f},
        {-0.48f, -0.04f}, { 0.46f, -0.02f}, {-0.44f,  0.24f}, {-0.14f,  0.38f},
        { 0.18f,  0.38f}, { 0.46f,  0.24f}, {-0.18f,  0.00f}, { 0.14f,  0.02f}
    };
    const std::vector<PlaybackPoint> landmarkSlots{
        { 0.00f, -0.16f}, {-0.24f,  0.10f}, { 0.24f,  0.10f}, { 0.00f,  0.28f}
    };
    const std::vector<PlaybackPoint> campusSlots{
        {-0.26f, -0.18f}, { 0.24f, -0.18f}, {-0.20f,  0.14f},
        { 0.22f,  0.16f}, { 0.00f, -0.30f}, { 0.00f,  0.00f}
    };
    const std::vector<PlaybackPoint> roadSlots{
        {-0.34f, -0.42f}, { 0.00f, -0.42f}, { 0.34f, -0.40f},
        {-0.42f, -0.08f}, { 0.42f, -0.04f},
        {-0.38f,  0.36f}, { 0.00f,  0.38f}, { 0.38f,  0.36f}
    };
    const std::vector<PlaybackPoint> vehicleSlots{
        {-0.24f, -0.36f}, { 0.12f, -0.34f}, {-0.30f,  0.32f},
        { 0.08f,  0.34f}, {-0.40f,  0.02f}, { 0.40f,  0.04f}
    };

    auto targetTotalRange = [](DistrictType district) {
        switch (district) {
            case DistrictType::Landmark: return std::pair<int, int>{2, 4};
            case DistrictType::Commercial: return std::pair<int, int>{8, 14};
            case DistrictType::Mixed: return std::pair<int, int>{6, 10};
            case DistrictType::Residential: return std::pair<int, int>{5, 8};
            case DistrictType::Service: return std::pair<int, int>{4, 7};
            case DistrictType::Campus: return std::pair<int, int>{2, 4};
            case DistrictType::Park:
            default: return std::pair<int, int>{0, 1};
        }
    };

    auto heightScaleRange = [](DistrictType district) {
        switch (district) {
            case DistrictType::Commercial: return std::pair<float, float>{0.90f, 1.22f};
            case DistrictType::Mixed: return std::pair<float, float>{0.88f, 1.12f};
            case DistrictType::Residential: return std::pair<float, float>{0.82f, 1.00f};
            case DistrictType::Service: return std::pair<float, float>{0.80f, 0.96f};
            case DistrictType::Landmark: return std::pair<float, float>{1.00f, 1.06f};
            case DistrictType::Campus: return std::pair<float, float>{0.84f, 0.96f};
            case DistrictType::Park:
            default: return std::pair<float, float>{0.82f, 0.94f};
        }
    };

    auto slotPoolFor = [&](DistrictType district) {
        std::vector<PlaybackPoint> slots = baseSlots;
        switch (district) {
            case DistrictType::Commercial:
                slots.insert(slots.end(), denseEdgeSlots.begin(), denseEdgeSlots.end());
                break;
            case DistrictType::Mixed:
                slots.insert(slots.end(), denseEdgeSlots.begin(), denseEdgeSlots.begin() + 9);
                break;
            case DistrictType::Residential:
                slots.insert(slots.end(), denseEdgeSlots.begin(), denseEdgeSlots.begin() + 6);
                break;
            case DistrictType::Service:
                slots.insert(slots.end(), denseEdgeSlots.begin(), denseEdgeSlots.begin() + 5);
                break;
            case DistrictType::Campus:
                slots = campusSlots;
                break;
            case DistrictType::Park:
                slots = {{0.00f, 0.00f}};
                break;
            case DistrictType::Landmark:
            default:
                slots = landmarkSlots;
                break;
        }
        return slots;
    };

    auto pushBuildingIfSafe = [&](const CityAssets::BuildingPreset& preset,
                                  float worldX,
                                  float worldY,
                                  float heightScale) {
        const float clearance = 0.38f + std::max(preset.width, preset.depth) * 0.5f;
        if (isReservedVisualArea(worldX, worldY, clearance, graph)) {
            return false;
        }

        presetBuildings.push_back({&preset, worldX, worldY, heightScale});
        return true;
    };

    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        const BlockZone& block = blocks[i];
        const float spanX = block.maxX - block.minX;
        const float spanY = block.maxY - block.minY;

        const int districtOrd = static_cast<int>(block.district);
        const CityAssets::ZoneType zone = CityAssets::classifyZone(districtOrd);
        const CityAssets::TileAssignment tile =
            CityAssets::assignTile(zone, i, layoutSeed);

        int placedBuildingsForBlock = 0;

        for (const auto& bp : tile.buildings) {
            const float wx =
                block.centerX + bp.offsetX * spanX + authoredJitter(rng) * spanX * 0.08f;
            const float wy =
                block.centerY + bp.offsetY * spanY + authoredJitter(rng) * spanY * 0.08f;
            if (pushBuildingIfSafe(bp.preset, wx, wy, bp.heightScale)) {
                ++placedBuildingsForBlock;
            }
        }

        for (const auto& ep : tile.environment) {
            const float wx = block.centerX + ep.offsetX * spanX;
            const float wy = block.centerY + ep.offsetY * spanY;
            const float envSpanX =
                spanX * ((block.district == DistrictType::Park)
                    ? 0.34f
                    : (block.district == DistrictType::Campus ? 0.31f : 0.28f));
            const float envSpanY =
                spanY * ((block.district == DistrictType::Park)
                    ? 0.30f
                    : (block.district == DistrictType::Campus ? 0.27f : 0.24f));
            presetEnvironments.push_back({&ep.preset, wx, wy, envSpanX, envSpanY});
        }

        for (const auto& vp : tile.vehicles) {
            const float wx = block.centerX + vp.offsetX * spanX;
            const float wy = block.centerY + vp.offsetY * spanY;
            presetVehicles.push_back({&vp.preset, wx, wy});
        }

        for (const auto& rp : tile.roadProps) {
            const float wx = block.centerX + rp.offsetX * spanX;
            const float wy = block.centerY + rp.offsetY * spanY;
            presetRoadProps.push_back({&rp.preset, wx, wy});
        }

        if (!tile.buildings.empty()) {
            const auto [minTotal, maxTotal] = targetTotalRange(block.district);
            std::uniform_int_distribution<int> totalDistribution(minTotal, maxTotal);
            const int targetTotal = totalDistribution(rng);
            const int targetExtraBuildings = std::max(0, targetTotal - placedBuildingsForBlock);

            if (targetExtraBuildings > 0) {
                std::vector<PlaybackPoint> slotPool = slotPoolFor(block.district);
                std::shuffle(slotPool.begin(), slotPool.end(), rng);

                const auto [heightMin, heightMax] = heightScaleRange(block.district);
                const std::size_t slotCount = slotPool.size();
                const int maxAttempts =
                    std::max(targetExtraBuildings * 5, static_cast<int>(slotCount) * 3);

                int placedExtras = 0;
                int attempts = 0;
                while (placedExtras < targetExtraBuildings && attempts < maxAttempts) {
                    const PlaybackPoint& slot =
                        slotPool[static_cast<std::size_t>(attempts) % slotCount];
                    const auto& source =
                        tile.buildings[static_cast<std::size_t>(attempts) % tile.buildings.size()];

                    const float worldX =
                        block.centerX + slot.x * spanX + slotJitter(rng) * spanX;
                    const float worldY =
                        block.centerY + slot.y * spanY + slotJitter(rng) * spanY;
                    const float extraHeightScale =
                        heightMin + (heightMax - heightMin) * unit(rng);

                    if (pushBuildingIfSafe(source.preset, worldX, worldY, extraHeightScale)) {
                        ++placedExtras;
                    }
                    ++attempts;
                }
            }
        }

        int extraVehicleCount = 0;
        int extraRoadPropCount = 0;
        switch (block.district) {
            case DistrictType::Commercial:
                extraVehicleCount = 3 + static_cast<int>(rng() % 3u);
                extraRoadPropCount = 3 + static_cast<int>(rng() % 2u);
                break;
            case DistrictType::Mixed:
                extraVehicleCount = 2 + static_cast<int>(rng() % 2u);
                extraRoadPropCount = 2 + static_cast<int>(rng() % 2u);
                break;
            case DistrictType::Service:
                extraVehicleCount = 1 + static_cast<int>(rng() % 2u);
                extraRoadPropCount = 1 + static_cast<int>(rng() % 2u);
                break;
            case DistrictType::Residential:
                extraVehicleCount = 1;
                break;
            default:
                break;
        }

        if (!tile.vehicles.empty() && extraVehicleCount > 0) {
            std::vector<PlaybackPoint> vehicleSlotPool = vehicleSlots;
            std::shuffle(vehicleSlotPool.begin(), vehicleSlotPool.end(), rng);
            for (int extra = 0; extra < extraVehicleCount; ++extra) {
                const PlaybackPoint& slot =
                    vehicleSlotPool[static_cast<std::size_t>(extra) % vehicleSlotPool.size()];
                const auto& source =
                    tile.vehicles[static_cast<std::size_t>(extra) % tile.vehicles.size()];
                const float worldX =
                    block.centerX + slot.x * spanX + authoredJitter(rng) * spanX * 0.04f;
                const float worldY =
                    block.centerY + slot.y * spanY + authoredJitter(rng) * spanY * 0.04f;
                if (!isReservedVisualArea(worldX, worldY, 0.18f, graph)) {
                    presetVehicles.push_back({&source.preset, worldX, worldY});
                }
            }
        }

        if (!tile.roadProps.empty() && extraRoadPropCount > 0) {
            std::vector<PlaybackPoint> roadSlotPool = roadSlots;
            std::shuffle(roadSlotPool.begin(), roadSlotPool.end(), rng);
            for (int extra = 0; extra < extraRoadPropCount; ++extra) {
                const PlaybackPoint& slot =
                    roadSlotPool[static_cast<std::size_t>(extra) % roadSlotPool.size()];
                const auto& source =
                    tile.roadProps[static_cast<std::size_t>(extra) % tile.roadProps.size()];
                const float worldX =
                    block.centerX + slot.x * spanX + authoredJitter(rng) * spanX * 0.03f;
                const float worldY =
                    block.centerY + slot.y * spanY + authoredJitter(rng) * spanY * 0.03f;
                if (!isReservedVisualArea(worldX, worldY, 0.16f, graph)) {
                    presetRoadProps.push_back({&source.preset, worldX, worldY});
                }
            }
        }
    }

    std::sort(presetBuildings.begin(), presetBuildings.end(),
              [](const PlacedPresetBuilding& a, const PlacedPresetBuilding& b) {
                  if (std::abs(a.worldY - b.worldY) > 0.02f) return a.worldY < b.worldY;
                  return a.worldX < b.worldX;
              });
}

void CityThemeRenderer::generatePeripheralScene(std::mt19937& rng) {
    backgroundBuildings.clear();
    ambientStreets.clear();
    mountains.clear();

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> colorDist(-1.0f, 1.0f);

    // Keep mountain bases just outside the city envelope â€” tight to the
    // roadside boundary but never crossing it. Only the mountain's own
    // footprint counts as the buffer, so peaks press right up to the edge.
    auto addMountain = [&](float wx, float wy, float bw, float bd,
                           float hs, float cv) {
        const float footprintPadX = bw * 0.45f;
        const float footprintPadY = bd * 0.45f;
        if (wx > sceneMinX - footprintPadX && wx < sceneMaxX + footprintPadX &&
            wy > sceneMinY - footprintPadY && wy < sceneMaxY + footprintPadY) {
            return;
        }
        mountains.push_back(MountainPeak{wx, wy, bw, bd, hs, cv});
    };

    // Extended placement area â€” mountains spread far beyond the city
    const float mtnMinX = peripheralMinX - 40.0f;
    const float mtnMaxX = peripheralMaxX + 40.0f;
    const float mtnSpanX = mtnMaxX - mtnMinX;
    const float mtnMinY = peripheralMinY - 36.0f;
    const float mtnMaxY = peripheralMaxY + 36.0f;
    const float mtnSpanY = mtnMaxY - mtnMinY;

    // Innermost ring (row 0 on every side) gets a tall-peak boost so silhouettes
    // project up in screen space and visually occlude the adjacent road rows â€”
    // routing graph is independent, so no impact on pathfinding.
    const float frontPeakBoost = 0.85f;
    const float frontPeakExtra = 0.35f;
    const float frontWidthBoost = 0.35f;

    // === NORTH RIDGE â€” 8 rows, tallest backdrop range ===
    for (int row = 0; row < 8; ++row) {
        const float rowF = static_cast<float>(row);
        const float baseY = sceneMinY - 0.6f - rowF * 3.4f;
        const int count = 28 + row * 4;
        const float hBoost = 0.12f * rowF;
        const float rowFrontBoost = (row == 0) ? frontPeakBoost : 0.0f;
        const float rowWidthBoost = (row == 0) ? frontWidthBoost : 0.0f;
        for (int i = 0; i < count; ++i) {
            const float t = (static_cast<float>(i) + 0.5f) /
                            static_cast<float>(count);
            float x = mtnMinX + t * mtnSpanX;
            x += (unit(rng) - 0.5f) * 1.8f;
            const float y = baseY - unit(rng) * 3.4f;
            float hs = 0.55f + unit(rng) * 0.55f + hBoost + rowFrontBoost;
            const float cb = 1.0f - std::abs(t - 0.5f) * 1.4f;
            hs += std::max(0.0f, cb) * 0.5f;
            if (row == 0) hs += unit(rng) * frontPeakExtra;
            const float bw = 1.6f + unit(rng) * 1.5f + rowF * 0.25f + rowWidthBoost;
            const float bd = 1.1f + unit(rng) * 1.0f + rowF * 0.18f;
            addMountain(x, y, bw, bd, hs, colorDist(rng));
        }
    }

    // === SOUTH HILLS â€” 7 rows, lower foreground ridges ===
    for (int row = 0; row < 7; ++row) {
        const float rowF = static_cast<float>(row);
        const float baseY = sceneMaxY + 0.6f + rowF * 3.2f;
        const int count = 24 + row * 4;
        const float rowFrontBoost = (row == 0) ? frontPeakBoost : 0.0f;
        const float rowWidthBoost = (row == 0) ? frontWidthBoost : 0.0f;
        for (int i = 0; i < count; ++i) {
            const float t = (static_cast<float>(i) + 0.5f) /
                            static_cast<float>(count);
            float x = mtnMinX + t * mtnSpanX;
            x += (unit(rng) - 0.5f) * 1.5f;
            const float y = baseY + unit(rng) * 3.0f;
            float hs = 0.35f + unit(rng) * 0.48f + rowF * 0.10f + rowFrontBoost;
            if (row == 0) hs += unit(rng) * frontPeakExtra;
            const float bw = 1.4f + unit(rng) * 1.3f + rowF * 0.25f + rowWidthBoost;
            const float bd = 0.9f + unit(rng) * 0.9f + rowF * 0.18f;
            addMountain(x, y, bw, bd, hs, colorDist(rng));
        }
    }

    // === WEST RIDGE â€” 7 rows ===
    for (int row = 0; row < 7; ++row) {
        const float rowF = static_cast<float>(row);
        const int count = 22 + row * 4;
        const float rowFrontBoost = (row == 0) ? frontPeakBoost : 0.0f;
        const float rowWidthBoost = (row == 0) ? frontWidthBoost : 0.0f;
        for (int i = 0; i < count; ++i) {
            const float t = (static_cast<float>(i) + 0.5f) /
                            static_cast<float>(count);
            float y = mtnMinY + t * mtnSpanY;
            y += (unit(rng) - 0.5f) * 1.2f;
            const float x = sceneMinX - 0.8f - unit(rng) * 3.0f - rowF * 3.0f;
            float hs = 0.45f + unit(rng) * 0.52f + rowF * 0.09f + rowFrontBoost;
            if (row == 0) hs += unit(rng) * frontPeakExtra;
            const float bw = 1.4f + unit(rng) * 1.2f + rowF * 0.25f + rowWidthBoost;
            const float bd = 1.0f + unit(rng) * 0.9f + rowF * 0.18f;
            addMountain(x, y, bw, bd, hs, colorDist(rng));
        }
    }

    // === EAST RIDGE â€” 7 rows ===
    for (int row = 0; row < 7; ++row) {
        const float rowF = static_cast<float>(row);
        const int count = 22 + row * 4;
        const float rowFrontBoost = (row == 0) ? frontPeakBoost : 0.0f;
        const float rowWidthBoost = (row == 0) ? frontWidthBoost : 0.0f;
        for (int i = 0; i < count; ++i) {
            const float t = (static_cast<float>(i) + 0.5f) /
                            static_cast<float>(count);
            float y = mtnMinY + t * mtnSpanY;
            y += (unit(rng) - 0.5f) * 1.2f;
            const float x = sceneMaxX + 0.8f + unit(rng) * 3.0f + rowF * 3.0f;
            float hs = 0.45f + unit(rng) * 0.52f + rowF * 0.09f + rowFrontBoost;
            if (row == 0) hs += unit(rng) * frontPeakExtra;
            const float bw = 1.4f + unit(rng) * 1.2f + rowF * 0.25f + rowWidthBoost;
            const float bd = 1.0f + unit(rng) * 0.9f + rowF * 0.18f;
            addMountain(x, y, bw, bd, hs, colorDist(rng));
        }
    }

    // === CORNER FILLS â€” NW, NE, SW, SE clusters (denser, larger radius) ===
    const float cornerOffsets[4][2] = {
        {sceneMinX - 2.5f, sceneMinY - 2.5f},
        {sceneMaxX + 2.5f, sceneMinY - 2.5f},
        {sceneMinX - 2.5f, sceneMaxY + 2.5f},
        {sceneMaxX + 2.5f, sceneMaxY + 2.5f},
    };
    for (int c = 0; c < 4; ++c) {
        const int count = 32;
        for (int i = 0; i < count; ++i) {
            const float angle = kTwoPi * static_cast<float>(i) /
                                static_cast<float>(count)
                              + unit(rng) * 0.6f;
            const float r = 2.0f + unit(rng) * 18.0f;
            const float x = cornerOffsets[c][0] + std::cos(angle) * r;
            const float y = cornerOffsets[c][1] + std::sin(angle) * r * 0.7f;
            const float hs = 0.40f + unit(rng) * 0.55f;
            const float bw = 1.4f + unit(rng) * 1.3f;
            const float bd = 1.0f + unit(rng) * 1.0f;
            addMountain(x, y, bw, bd, hs, colorDist(rng));
        }
    }

    // === OUTER HALO SCATTER â€” blanket the entire peripheral zone outside
    // the scene box so any remaining black space is filled with mountains ===
    {
        const float haloMinX = mtnMinX - 6.0f;
        const float haloMaxX = mtnMaxX + 6.0f;
        const float haloMinY = mtnMinY - 6.0f;
        const float haloMaxY = mtnMaxY + 6.0f;
        const float stepX = 2.2f;
        const float stepY = 2.2f;
        for (float gy = haloMinY; gy <= haloMaxY; gy += stepY) {
            for (float gx = haloMinX; gx <= haloMaxX; gx += stepX) {
                // Coarse skip â€” the addMountain exclusion does the precise
                // boundary check; this just avoids wasted work inside the city.
                if (gx > sceneMinX && gx < sceneMaxX &&
                    gy > sceneMinY && gy < sceneMaxY) {
                    continue;
                }
                const float x = gx + (unit(rng) - 0.5f) * stepX * 0.9f;
                const float y = gy + (unit(rng) - 0.5f) * stepY * 0.9f;
                const float hs = 0.38f + unit(rng) * 0.70f;
                const float bw = 1.3f + unit(rng) * 1.6f;
                const float bd = 0.9f + unit(rng) * 1.2f;
                addMountain(x, y, bw, bd, hs, colorDist(rng));
            }
        }
    }

    // === FAR FIELD â€” extra distant peaks beyond the halo for depth ===
    {
        const int farCount = 260;
        const float farPad = 10.0f;
        for (int i = 0; i < farCount; ++i) {
            const int side = static_cast<int>(unit(rng) * 4.0f) % 4;
            float x = 0.0f;
            float y = 0.0f;
            if (side == 0) { // north band
                x = mtnMinX - farPad + unit(rng) * (mtnSpanX + 2.0f * farPad);
                y = mtnMinY - farPad - unit(rng) * 10.0f;
            } else if (side == 1) { // south band
                x = mtnMinX - farPad + unit(rng) * (mtnSpanX + 2.0f * farPad);
                y = mtnMaxY + farPad + unit(rng) * 10.0f;
            } else if (side == 2) { // west band
                x = mtnMinX - farPad - unit(rng) * 10.0f;
                y = mtnMinY - farPad + unit(rng) * (mtnSpanY + 2.0f * farPad);
            } else { // east band
                x = mtnMaxX + farPad + unit(rng) * 10.0f;
                y = mtnMinY - farPad + unit(rng) * (mtnSpanY + 2.0f * farPad);
            }
            const float hs = 0.55f + unit(rng) * 0.80f;
            const float bw = 1.8f + unit(rng) * 2.0f;
            const float bd = 1.2f + unit(rng) * 1.4f;
            addMountain(x, y, bw, bd, hs, colorDist(rng));
        }
    }

    // Sort back-to-front (ascending worldY for painter's algorithm)
    std::sort(mountains.begin(), mountains.end(),
              [](const MountainPeak& a, const MountainPeak& b) {
                  if (std::abs(a.worldY - b.worldY) > 0.02f)
                      return a.worldY < b.worldY;
                  return a.worldX < b.worldX;
              });
}

