#include "visualization/SeaThemeRendererInternal.h"

void SeaThemeRenderer::drawTransitNetwork(
    IsometricRenderer& renderer,
    const MapGraph& graph,
    const MissionPresentation* mission,
    AnimationController::PlaybackState playbackState,
    float routeRevealProgress,
    float animationTime) {
    constexpr float kMaxVisibleDist = 48.75f;

    for (int i = 0; i < graph.getNodeCount(); ++i) {
        for (int j = i + 1; j < graph.getNodeCount(); ++j) {
            const float dist = graph.getAdjacencyMatrix()[i][j];
            if (dist <= 0.0f || dist >= kMaxVisibleDist) {
                continue;
            }

            const WasteNode& from = graph.getNode(i);
            const WasteNode& to = graph.getNode(j);
            const IsoCoord isoFrom =
                RenderUtils::worldToIso(from.getWorldX(), from.getWorldY());
            const IsoCoord isoTo =
                RenderUtils::worldToIso(to.getWorldX(), to.getWorldY());
            const float alpha = 1.0f - (dist / kMaxVisibleDist);
            renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                              Color(0.18f, 0.35f, 0.48f, 0.36f * alpha), 1.4f);
        }
    }

    if (!mission || !mission->isValid()) {
        return;
    }

    const PlaybackPath& path = mission->playbackPath;
    const float revealDistance = path.totalLength * std::clamp(routeRevealProgress, 0.0f, 1.0f);
    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 2.4f);

    for (std::size_t i = 1; i < path.points.size(); ++i) {
        const float startDistance = path.cumulativeDistances[i - 1];
        const float endDistance = path.cumulativeDistances[i];
        if (revealDistance <= startDistance) {
            break;
        }

        float worldX = path.points[i].x;
        float worldY = path.points[i].y;
        if (revealDistance < endDistance && endDistance > startDistance) {
            const float localT = (revealDistance - startDistance) / (endDistance - startDistance);
            worldX = RenderUtils::lerp(path.points[i - 1].x, path.points[i].x, localT);
            worldY = RenderUtils::lerp(path.points[i - 1].y, path.points[i].y, localT);
        }

        const IsoCoord isoFrom =
            RenderUtils::worldToIso(path.points[i - 1].x, path.points[i - 1].y);
        const IsoCoord isoTo = RenderUtils::worldToIso(worldX, worldY);
        renderer.drawLine(isoFrom.x, isoFrom.y + 1.0f, isoTo.x, isoTo.y + 1.0f,
                          Color(0.03f, 0.16f, 0.22f, 0.10f + pulse * 0.04f), 6.4f);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          Color(0.24f, 0.78f, 0.80f, 0.18f + pulse * 0.10f), 2.2f);
        renderer.drawLine(isoFrom.x, isoFrom.y, isoTo.x, isoTo.y,
                          Color(0.92f, 0.98f, 0.96f, 0.10f + pulse * 0.08f), 0.9f);
    }

    if (playbackState != AnimationController::PlaybackState::IDLE &&
        path.totalLength > 0.0f) {
        const float markerDistance =
            std::fmod(animationTime * 3.5f, std::max(path.totalLength, 0.001f));
        for (std::size_t i = 1; i < path.points.size(); ++i) {
            if (markerDistance > path.cumulativeDistances[i]) {
                continue;
            }

            const float startDistance = path.cumulativeDistances[i - 1];
            const float endDistance = path.cumulativeDistances[i];
            const float localT = (endDistance > startDistance)
                ? (markerDistance - startDistance) / (endDistance - startDistance)
                : 0.0f;
            const float worldX =
                RenderUtils::lerp(path.points[i - 1].x, path.points[i].x, localT);
            const float worldY =
                RenderUtils::lerp(path.points[i - 1].y, path.points[i].y, localT);
            const IsoCoord iso = RenderUtils::worldToIso(worldX, worldY);
            renderer.drawDiamond(iso.x, iso.y, 4.0f, 1.8f,
                                 Color(0.66f, 1.0f, 0.96f, 0.16f));
            renderer.drawFilledCircle(iso.x, iso.y, 1.1f,
                                      Color(1.0f, 1.0f, 1.0f, 0.78f));
            break;
        }
    }
}

// =====================================================================
//  Ground plane â€” ocean water
// =====================================================================

