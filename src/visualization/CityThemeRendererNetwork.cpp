#include "visualization/CityThemeRendererInternal.h"

void CityThemeRenderer::generateGridNetwork(const MapGraph& graph, std::mt19937& rng) {
    intersections.clear();
    roads.clear();
    roadAdjacency.clear();

    gridColumns = 8;
    gridRows = 6;
    primaryBoulevardColumn = gridColumns / 2;
    primaryBoulevardRow = gridRows / 2;

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const auto& node : graph.getNodes()) {
        minX = std::min(minX, node.getWorldX());
        maxX = std::max(maxX, node.getWorldX());
        minY = std::min(minY, node.getWorldY());
        maxY = std::max(maxY, node.getWorldY());
    }

    const float startX = minX - kCityPaddingX;
    const float endX = maxX + kCityPaddingX;
    const float startY = minY - kCityPaddingY;
    const float endY = maxY + kCityPaddingY;

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> cycleDistribution(0.0f, 4.0f);

    auto buildAxisPositions = [&](int count, float start, float end,
                                  int boulevardIndex, float boulevardBoost) {
        std::vector<float> spans(std::max(0, count - 1), 1.0f);
        for (int i = 0; i < static_cast<int>(spans.size()); ++i) {
            float spanWeight = 0.90f + unit(rng) * 0.45f;
            if (i == boulevardIndex) {
                spanWeight += boulevardBoost;
            } else if (std::abs(i - boulevardIndex) == 1) {
                spanWeight += boulevardBoost * 0.28f;
            } else if (i == 0 || i == static_cast<int>(spans.size()) - 1) {
                spanWeight += 0.18f;
            }
            spans[i] = spanWeight;
        }

        const float totalSpan = std::accumulate(spans.begin(), spans.end(), 0.0f);
        std::vector<float> positions(count, start);
        float cursor = start;
        for (int i = 1; i < count; ++i) {
            cursor += (end - start) * (spans[i - 1] / totalSpan);
            positions[i] = cursor;
        }
        positions.front() = start;
        positions.back() = end;
        return positions;
    };

    const std::vector<float> columnPositions = buildAxisPositions(
        gridColumns, startX, endX, std::max(0, primaryBoulevardColumn - 1), 0.78f);
    const std::vector<float> rowPositions = buildAxisPositions(
        gridRows, startY, endY, std::max(0, primaryBoulevardRow - 1), 0.62f);

    for (int row = 0; row < gridRows; ++row) {
        for (int col = 0; col < gridColumns; ++col) {
            const bool onBoulevard =
                row == primaryBoulevardRow || col == primaryBoulevardColumn;
            const bool innerCollector =
                row == std::max(1, primaryBoulevardRow - 2) ||
                col == std::max(1, primaryBoulevardColumn - 2);
            const bool hasLight =
                row > 0 && row < gridRows - 1 &&
                col > 0 && col < gridColumns - 1 &&
                (onBoulevard || ((row + col + static_cast<int>(layoutSeed % 3)) % 2 == 0));
            intersections.push_back(Intersection{
                row * gridColumns + col,
                columnPositions[col],
                rowPositions[row],
                hasLight || innerCollector,
                cycleDistribution(rng)
            });
        }
    }

    roadAdjacency.assign(intersections.size(), {});

    auto addRoad = [&](int from, int to) {
        const Intersection& a = intersections[from];
        const Intersection& b = intersections[to];
        const bool verticalRoad = a.x == b.x;
        const int lineIndex = verticalRoad ? (from % gridColumns) : (from / gridColumns);
        const bool arterial = verticalRoad
            ? lineIndex == primaryBoulevardColumn
            : lineIndex == primaryBoulevardRow;

        const int roadIndex = static_cast<int>(roads.size());
        roads.push_back(RoadSegment{
            from,
            to,
            pointDistance(a.x, a.y, b.x, b.y),
            0.0f,
            false,
            0.0f,
            0.0f,
            arterial,
            false,
            RoadEvent::NONE,
            VisualTier::Support
        });
        roadAdjacency[from].push_back(roadIndex);
        roadAdjacency[to].push_back(roadIndex);
    };

    for (int row = 0; row < gridRows; ++row) {
        for (int col = 0; col < gridColumns; ++col) {
            const int id = row * gridColumns + col;
            if (col + 1 < gridColumns) {
                addRoad(id, id + 1);
            }
            if (row + 1 < gridRows) {
                addRoad(id, id + gridColumns);
            }
        }
    }

    applyTrafficConditions(rng);
}

void CityThemeRenderer::assignNodeAnchors(const MapGraph& graph) {
    nodeAnchors.assign(graph.getNodeCount(), 0);
    for (int i = 0; i < graph.getNodeCount(); ++i) {
        const WasteNode& node = graph.getNode(i);
        float bestDistance = std::numeric_limits<float>::max();
        int bestAnchor = 0;
        for (const auto& intersection : intersections) {
            const float d = pointDistance(node.getWorldX(), node.getWorldY(),
                                          intersection.x, intersection.y);
            if (d < bestDistance) {
                bestDistance = d;
                bestAnchor = intersection.id;
            }
        }
        nodeAnchors[i] = bestAnchor;
    }
}

void CityThemeRenderer::updateSceneFocus(const MapGraph& graph) {
    sceneMinX = std::numeric_limits<float>::max();
    sceneMaxX = std::numeric_limits<float>::lowest();
    sceneMinY = std::numeric_limits<float>::max();
    sceneMaxY = std::numeric_limits<float>::lowest();

    for (const auto& intersection : intersections) {
        sceneMinX = std::min(sceneMinX, intersection.x);
        sceneMaxX = std::max(sceneMaxX, intersection.x);
        sceneMinY = std::min(sceneMinY, intersection.y);
        sceneMaxY = std::max(sceneMaxY, intersection.y);
    }

    float centroidSumX = 0.0f;
    float centroidSumY = 0.0f;
    for (const auto& node : graph.getNodes()) {
        sceneMinX = std::min(sceneMinX, node.getWorldX());
        sceneMaxX = std::max(sceneMaxX, node.getWorldX());
        sceneMinY = std::min(sceneMinY, node.getWorldY());
        sceneMaxY = std::max(sceneMaxY, node.getWorldY());
        centroidSumX += node.getWorldX();
        centroidSumY += node.getWorldY();
    }

    const float nodeCount = std::max(1, graph.getNodeCount());
    nodeCentroidX = centroidSumX / static_cast<float>(nodeCount);
    nodeCentroidY = centroidSumY / static_cast<float>(nodeCount);

    const WasteNode& hq = graph.getHQNode();
    operationalCenterX = (hq.getWorldX() + nodeCentroidX) * 0.5f;
    operationalCenterY = (hq.getWorldY() + nodeCentroidY) * 0.5f;

    const float spanX = std::max(1.0f, sceneMaxX - sceneMinX);
    const float spanY = std::max(1.0f, sceneMaxY - sceneMinY);
    operationalRadiusX = std::max(4.0f, spanX * 0.34f);
    operationalRadiusY = std::max(3.6f, spanY * 0.30f);

    peripheralMinX = sceneMinX - kPeripheralBandX;
    peripheralMaxX = sceneMaxX + kPeripheralBandX;
    peripheralMinY = sceneMinY - kPeripheralBandY;
    peripheralMaxY = sceneMaxY + kPeripheralBandY;
    updateZoneBounds(sceneMinX, sceneMaxX, sceneMinY, sceneMaxY);

    for (auto& road : roads) {
        const Intersection& from = intersections[road.from];
        const Intersection& to = intersections[road.to];
        const float midX = (from.x + to.x) * 0.5f;
        const float midY = (from.y + to.y) * 0.5f;
        road.focusWeight = clamp01(
            computeOperationalFocus(midX, midY) + (road.arterial ? 0.16f : 0.0f));
        road.visualTier = visualTierFromFocus(road.focusWeight);
    }
}

void CityThemeRenderer::generateBlocks(std::mt19937& rng) {
    blocks.clear();
    blocks.reserve((gridRows - 1) * (gridColumns - 1));

    std::uniform_real_distribution<float> focusJitter(-0.05f, 0.05f);

    for (int row = 0; row + 1 < gridRows; ++row) {
        for (int col = 0; col + 1 < gridColumns; ++col) {
            const Intersection& a = intersections[row * gridColumns + col];
            const Intersection& b = intersections[row * gridColumns + col + 1];
            const Intersection& c = intersections[(row + 1) * gridColumns + col];
            const float centerX = (a.x + b.x) * 0.5f;
            const float centerY = (a.y + c.y) * 0.5f;
            const float focus = clamp01(computeOperationalFocus(centerX, centerY) + focusJitter(rng));
            unsigned int arterialEdges = 0;
            if (row + 1 == primaryBoulevardRow) arterialEdges |= kNorthEdge;
            if (col == primaryBoulevardColumn) arterialEdges |= kEastEdge;
            if (row == primaryBoulevardRow) arterialEdges |= kSouthEdge;
            if (col + 1 == primaryBoulevardColumn) arterialEdges |= kWestEdge;

            const bool edgeBlock =
                row == 0 || col == 0 ||
                row == gridRows - 2 || col == gridColumns - 2;
            const float arterialBoost = arterialEdges == 0 ? 0.0f : 0.12f;

            DistrictType district = DistrictType::Mixed;
            if (edgeBlock && focus < 0.34f) {
                district = DistrictType::Service;
            } else if (focus >= 0.56f) {
                district = DistrictType::Commercial;
            } else if (focus >= 0.38f) {
                district = DistrictType::Mixed;
            } else if (focus >= 0.22f) {
                district = DistrictType::Residential;
            } else {
                district = DistrictType::Service;
            }

            float occupancyTarget = 0.55f;
            float greenRatio = 0.18f;
            float streetSetback = 0.12f;
            float interiorMargin = 0.10f;
            float heightBias = focus + arterialBoost;
            switch (district) {
                case DistrictType::Commercial:
                    occupancyTarget = 0.84f;
                    greenRatio = 0.10f;
                    streetSetback = 0.08f;
                    interiorMargin = 0.06f;
                    break;
                case DistrictType::Mixed:
                    occupancyTarget = 0.68f;
                    greenRatio = 0.18f;
                    streetSetback = 0.11f;
                    interiorMargin = 0.09f;
                    break;
                case DistrictType::Residential:
                    occupancyTarget = 0.48f;
                    greenRatio = 0.34f;
                    streetSetback = 0.16f;
                    interiorMargin = 0.12f;
                    break;
                case DistrictType::Campus:
                    occupancyTarget = 0.34f;
                    greenRatio = 0.44f;
                    streetSetback = 0.18f;
                    interiorMargin = 0.14f;
                    break;
                case DistrictType::Park:
                    occupancyTarget = 0.08f;
                    greenRatio = 0.84f;
                    streetSetback = 0.20f;
                    interiorMargin = 0.18f;
                    break;
                case DistrictType::Service:
                    occupancyTarget = 0.40f;
                    greenRatio = 0.22f;
                    streetSetback = 0.14f;
                    interiorMargin = 0.10f;
                    break;
                case DistrictType::Landmark:
                    occupancyTarget = 0.78f;
                    greenRatio = 0.12f;
                    streetSetback = 0.08f;
                    interiorMargin = 0.06f;
                    break;
            }

            blocks.push_back(BlockZone{
                row,
                col,
                RenderUtils::lerp(a.x, b.x, 0.07f),
                RenderUtils::lerp(a.x, b.x, 0.93f),
                RenderUtils::lerp(a.y, c.y, 0.08f),
                RenderUtils::lerp(a.y, c.y, 0.92f),
                centerX,
                centerY,
                focus,
                occupancyTarget,
                greenRatio,
                streetSetback,
                interiorMargin,
                heightBias,
                arterialEdges,
                district,
                visualTierFromFocus(clamp01(focus + arterialBoost)),
                false,
                false,
                false
            });
        }
    }

    int landmarkIndex = -1;
    float bestLandmarkScore = std::numeric_limits<float>::max();
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        const BlockZone& block = blocks[i];
        if (block.arterialEdges == 0u || block.district == DistrictType::Service) {
            continue;
        }

        const float centerDistance =
            pointDistance(block.centerX, block.centerY,
                          operationalCenterX, operationalCenterY);
        const float arterialPenalty = (block.arterialEdges & (kNorthEdge | kSouthEdge)) &&
                                      (block.arterialEdges & (kEastEdge | kWestEdge))
            ? -1.0f
            : 0.0f;
        const float score = centerDistance - block.focusWeight * 2.4f + arterialPenalty;
        if (score < bestLandmarkScore) {
            bestLandmarkScore = score;
            landmarkIndex = i;
        }
    }

    if (landmarkIndex >= 0) {
        BlockZone& block = blocks[landmarkIndex];
        block.district = DistrictType::Landmark;
        block.landmarkCluster = true;
        block.occupancyTarget = 0.76f;
        block.greenRatio = 0.10f;
        block.streetSetback = 0.07f;
        block.interiorMargin = 0.05f;
        block.heightBias = clamp01(block.heightBias + 0.35f);
        block.visualTier = VisualTier::Focus;
    }

    int campusIndex = -1;
    float bestCampusScore = std::numeric_limits<float>::max();
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        const BlockZone& block = blocks[i];
        if (i == landmarkIndex || block.arterialEdges == 0u) {
            continue;
        }
        const float focusGap = std::abs(block.focusWeight - 0.48f);
        const float centerDistance =
            pointDistance(block.centerX, block.centerY,
                          operationalCenterX, operationalCenterY);
        const float score = focusGap + centerDistance * 0.08f;
        if (score < bestCampusScore) {
            bestCampusScore = score;
            campusIndex = i;
        }
    }

    if (campusIndex >= 0) {
        BlockZone& block = blocks[campusIndex];
        block.district = DistrictType::Campus;
        block.civicAnchor = true;
        block.occupancyTarget = 0.32f;
        block.greenRatio = 0.46f;
        block.streetSetback = 0.18f;
        block.interiorMargin = 0.14f;
        block.heightBias = clamp01(block.heightBias * 0.78f);
        if (block.visualTier == VisualTier::Peripheral) {
            block.visualTier = VisualTier::Support;
        }
    }

    std::vector<std::pair<float, int>> parkCandidates;
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        const BlockZone& block = blocks[i];
        if (i == landmarkIndex || i == campusIndex) {
            continue;
        }
        if (block.district == DistrictType::Commercial || block.focusWeight > 0.62f) {
            continue;
        }

        const float residentialAffinity =
            (block.district == DistrictType::Residential ? -0.24f : 0.0f) +
            (block.arterialEdges == 0u ? -0.10f : 0.04f);
        const float score =
            std::abs(block.focusWeight - 0.34f) +
            pointDistance(block.centerX, block.centerY, nodeCentroidX, nodeCentroidY) * 0.02f +
            residentialAffinity;
        parkCandidates.push_back({score, i});
    }

    std::sort(parkCandidates.begin(), parkCandidates.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
    const int parkCount = std::min<int>(4, static_cast<int>(parkCandidates.size()));
    for (int i = 0; i < parkCount; ++i) {
        BlockZone& block = blocks[parkCandidates[i].second];
        block.district = DistrictType::Park;
        block.parkAnchor = true;
        block.occupancyTarget = 0.06f;
        block.greenRatio = 0.88f;
        block.streetSetback = 0.20f;
        block.interiorMargin = 0.18f;
        block.heightBias = 0.10f;
        block.visualTier = VisualTier::Support;
    }

    for (BlockZone& block : blocks) {
        if (block.district == DistrictType::Landmark ||
            block.district == DistrictType::Campus ||
            block.district == DistrictType::Park) {
            continue;
        }

        const float distanceToLandmark = landmarkIndex >= 0
            ? pointDistance(block.centerX, block.centerY,
                            blocks[landmarkIndex].centerX, blocks[landmarkIndex].centerY)
            : std::numeric_limits<float>::max();

        if (distanceToLandmark < 8.5f &&
            block.district != DistrictType::Service) {
            block.district = DistrictType::Commercial;
            block.occupancyTarget = std::max(block.occupancyTarget, 0.76f);
            block.greenRatio = std::min(block.greenRatio, 0.14f);
            block.streetSetback = std::min(block.streetSetback, 0.09f);
            block.interiorMargin = std::min(block.interiorMargin, 0.07f);
            const float proximityBoost = 1.0f - clamp01(distanceToLandmark / 8.5f);
            block.heightBias = clamp01(block.heightBias + 0.10f + proximityBoost * 0.22f);
            block.visualTier = VisualTier::Focus;
        } else if (block.district == DistrictType::Service &&
                   block.arterialEdges == 0u && block.focusWeight > 0.24f) {
            block.district = DistrictType::Residential;
            block.occupancyTarget = 0.46f;
            block.greenRatio = 0.36f;
            block.streetSetback = 0.17f;
            block.interiorMargin = 0.12f;
        }
    }
}

