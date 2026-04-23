#ifndef CITY_THEME_RENDERER_INTERNAL_H
#define CITY_THEME_RENDERER_INTERNAL_H

#include "visualization/CityThemeRenderer.h"

#include "environment/MissionPresentationUtils.h"
#include "environment/SeasonProfile.h"
#include "visualization/CityBatchGeometry.h"
#include "visualization/CityRenderUtils.h"
#include "visualization/CityShapeDrawing.h"
#include "visualization/IsometricRenderer.h"

#include <glad/glad.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <vector>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

namespace CityRendererConstants {
constexpr float kCityPaddingX = 2.0f;
constexpr float kCityPaddingY = 1.8f;
constexpr float kPeripheralBandX = 9.0f;
constexpr float kPeripheralBandY = 7.5f;
constexpr float kCongestionPenaltyScale = 0.75f;
constexpr float kIncidentPenaltyScale = 1.75f;
constexpr float kTwoPi = 6.28318530718f;
constexpr unsigned int kNorthEdge = 1u;
constexpr unsigned int kEastEdge = 2u;
constexpr unsigned int kSouthEdge = 4u;
constexpr unsigned int kWestEdge = 8u;
} // namespace CityRendererConstants

using CityBatchGeometry::ScopedStaticBatchCapture;
using CityBatchGeometry::drawOrAppendDiamond;
using CityBatchGeometry::drawOrAppendDiamondOutline;
using CityBatchGeometry::drawOrAppendLine;

using CityRenderUtils::ZoneVisibility;
using CityRenderUtils::attenuateToZone;
using CityRenderUtils::biasColor;
using CityRenderUtils::clamp01;
using CityRenderUtils::computeZoneVisibility;
using CityRenderUtils::currentZoomFactor;
using CityRenderUtils::desaturateColor;
using CityRenderUtils::ellipseInfluence;
using CityRenderUtils::mixColor;
using CityRenderUtils::pointDistance;
using CityRenderUtils::scaleColor;
using CityRenderUtils::smoothRange;
using CityRenderUtils::updateZoneBounds;
using CityRenderUtils::withAlpha;
using CityRenderUtils::zoneOutsideDistance;

using CityShapeDrawing::BlockFaces;
using CityShapeDrawing::TreePalette;
using CityShapeDrawing::applySeasonalTreeCanopy;
using CityShapeDrawing::drawBlockMass;
using CityShapeDrawing::drawFaceBands;
using CityShapeDrawing::drawGradientEllipse;
using CityShapeDrawing::drawImmediateQuad;
using CityShapeDrawing::drawMountainShape;
using CityShapeDrawing::drawOutlineLoop;
using CityShapeDrawing::drawStylizedTree;
using CityShapeDrawing::drawWorldQuadPatch;
using CityShapeDrawing::makeBlockFaces;

using CityRendererConstants::kCityPaddingX;
using CityRendererConstants::kCityPaddingY;
using CityRendererConstants::kCongestionPenaltyScale;
using CityRendererConstants::kEastEdge;
using CityRendererConstants::kIncidentPenaltyScale;
using CityRendererConstants::kNorthEdge;
using CityRendererConstants::kPeripheralBandX;
using CityRendererConstants::kPeripheralBandY;
using CityRendererConstants::kSouthEdge;
using CityRendererConstants::kTwoPi;
using CityRendererConstants::kWestEdge;

inline const MissionPresentation* ensureMission(const MissionPresentation* mission) {
    return (mission && mission->isValid()) ? mission : nullptr;
}

inline void appendDistinctPoint(std::vector<PlaybackPoint>& points,
                                const PlaybackPoint& point) {
    if (points.empty()) {
        points.push_back(point);
        return;
    }

    const PlaybackPoint& back = points.back();
    if (pointDistance(back.x, back.y, point.x, point.y) > 0.001f) {
        points.push_back(point);
    }
}

#endif // CITY_THEME_RENDERER_INTERNAL_H
