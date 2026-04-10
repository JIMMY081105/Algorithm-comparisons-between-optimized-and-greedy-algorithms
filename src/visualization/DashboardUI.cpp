#include "visualization/DashboardUI.h"
#include "visualization/DashboardStyle.h"
#include "visualization/RenderUtils.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace {
constexpr ImVec4 kBrandPrimary(0.15f, 0.72f, 0.94f, 1.0f);
constexpr ImVec4 kTextSoft(0.45f, 0.50f, 0.58f, 1.0f);
constexpr ImVec4 kTextMuted(0.46f, 0.56f, 0.64f, 1.0f);
constexpr ImVec4 kTextSubtle(0.55f, 0.60f, 0.68f, 1.0f);
constexpr ImVec4 kTextNeutral(0.76f, 0.80f, 0.84f, 1.0f);
} // namespace

DashboardUI::DashboardUI()
    : selectedAlgorithm(0), showComparisonTable(false),
      showEventLog(true), showNodeDetails(false) {}

DashboardUI::UIActions DashboardUI::render(WasteSystem& system,
                                            ComparisonManager& compMgr,
                                            AnimationController& animCtrl,
                                            const RouteResult& currentResult) {
    const auto playState = animCtrl.getState();
    const bool hasMission = currentResult.isValid();
    const bool missionRunning =
        playState == AnimationController::PlaybackState::PLAYING;
    const float missionProgress = animCtrl.getProgress();

    DashboardStyle::applyTheme(hasMission, missionRunning);
    UIActions actions;

    drawHeaderPanel(system, playState, hasMission);
    drawControlPanel(system, animCtrl, actions);

    drawMetricsPanel(currentResult, system, playState, missionProgress);
    drawLegendPanel();
    drawRouteOrderPanel(currentResult, system, playState, missionProgress);

    if (showComparisonTable) {
        drawComparisonTable(compMgr);
    }
    if (showEventLog) {
        drawEventLogPanel(system);
    }
    if (showNodeDetails) {
        drawNodeDetailsPanel(system);
    }

    drawExportPanel(actions);

    return actions;
}

void DashboardUI::drawHeaderPanel(const WasteSystem& system,
                                  AnimationController::PlaybackState playState,
                                  bool hasMission) {
    const DashboardStyle::SidebarLayout layout = DashboardStyle::buildSidebarLayout();
    ImGui::SetNextWindowPos(layout.headerPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.headerSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);

    ImGui::Begin("##Header", nullptr,
                 DashboardStyle::pinnedSidebarWindowFlags() |
                 ImGuiWindowFlags_NoTitleBar);

    const ImVec4 stateTint = DashboardStyle::playbackTint(playState, hasMission);
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 116.0f, 10.0f));
    ImGui::TextColored(stateTint, "[%s]",
                       DashboardStyle::playbackLabel(playState, hasMission));
    ImGui::SetCursorPos(ImVec2(14.0f, 10.0f));
    ImGui::SetWindowFontScale(1.05f);
    ImGui::TextColored(kBrandPrimary, "ECOROUTE SOLUTIONS");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(ImVec4(0.64f, 0.78f, 0.84f, 1.0f),
                       "Marine Cleanup Command");
    ImGui::TextColored(kTextSoft,
                       "Day %d  |  Seed %u", system.getDayNumber(),
                       system.getCurrentSeed());

    ImGui::End();
}

void DashboardUI::drawControlPanel(WasteSystem& system,
                                   AnimationController& animCtrl,
                                   UIActions& actions) {
    const auto playState = animCtrl.getState();
    const bool hasMission = animCtrl.getCurrentRoute().isValid();
    const float missionProgress = animCtrl.getProgress();
    const DashboardStyle::SidebarLayout layout = DashboardStyle::buildSidebarLayout();

    ImGui::SetNextWindowPos(layout.controlsPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.controlsSize, ImGuiCond_Always);
    ImGui::Begin("Operations", nullptr, DashboardStyle::pinnedSidebarWindowFlags());

    ImGui::SeparatorText("Simulation");
    if (ImGui::Button("Generate New Day", ImVec2(-1, 28))) {
        actions.generateNewDay = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Randomize debris levels for all sea zones");
    }

    float threshold = system.getCollectionThreshold();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##threshold", &threshold, 10.0f, 80.0f,
                           "Threshold: %.0f%%")) {
        system.setCollectionThreshold(threshold);
    }

    ImGui::SeparatorText("Algorithm");
    const char* algorithmLabels[] = {"Regular", "Greedy", "MST", "TSP"};
    for (int index = 0; index < 4; ++index) {
        ImGui::RadioButton(algorithmLabels[index], &selectedAlgorithm, index);
        if (index < 3) {
            ImGui::SameLine();
        }
    }

    if (ImGui::Button("Run Selected", ImVec2(-1, 26))) {
        actions.runSelectedAlgorithm = true;
        actions.algorithmToRun = selectedAlgorithm;
    }
    if (ImGui::Button("Compare All", ImVec2(-1, 26))) {
        actions.compareAll = true;
        showComparisonTable = true;
    }

    ImGui::SeparatorText("Playback");
    const float halfButtonWidth =
        (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (playState == AnimationController::PlaybackState::IDLE ||
        playState == AnimationController::PlaybackState::FINISHED) {
        if (ImGui::Button("Play", ImVec2(halfButtonWidth, 24))) {
            actions.playPause = true;
        }
    } else if (playState == AnimationController::PlaybackState::PLAYING) {
        if (ImGui::Button("Pause", ImVec2(halfButtonWidth, 24))) {
            animCtrl.pause();
        }
    } else {
        if (ImGui::Button("Resume", ImVec2(halfButtonWidth, 24))) {
            animCtrl.resume();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Replay", ImVec2(halfButtonWidth, 24))) {
        actions.replay = true;
    }

    float speed = animCtrl.getSpeed();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##speed", &speed, 0.1f, 5.0f, "Speed: %.1fx")) {
        animCtrl.setSpeed(speed);
    }

    ImGui::TextDisabled("Playback Progress");
    ImGui::ProgressBar(missionProgress, ImVec2(-1, 14));
    ImGui::TextColored(DashboardStyle::playbackTint(playState, hasMission),
                       "%s", DashboardStyle::playbackLabel(playState, hasMission));

    ImGui::SeparatorText("Panels");
    ImGui::Checkbox("Comparison", &showComparisonTable);
    ImGui::SameLine();
    ImGui::Checkbox("Log", &showEventLog);
    ImGui::Checkbox("Node Details", &showNodeDetails);

    ImGui::End();
}

void DashboardUI::drawMetricsPanel(const RouteResult& currentResult,
                                    const WasteSystem& system,
                                    AnimationController::PlaybackState playState,
                                    float progress) {
    const DashboardStyle::SidebarLayout layout = DashboardStyle::buildSidebarLayout();
    ImGui::SetNextWindowPos(layout.metricsPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.metricsSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGui::Begin("Metrics", nullptr, DashboardStyle::pinnedSidebarWindowFlags());

    if (!currentResult.isValid()) {
        ImGui::TextDisabled("Awaiting route plan");
        ImGui::TextColored(kTextSoft, "No route computed yet.");
        ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.45f, 1.0f),
                           "Generate a day and run an algorithm.");
    } else {
        const ImVec4 stateTint = DashboardStyle::playbackTint(playState, true);
        ImGui::BeginChild("MissionStatus", ImVec2(0.0f, 62.0f), true,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::TextDisabled("Mission Status");
        ImGui::TextColored(stateTint, "%s",
                           DashboardStyle::playbackLabel(playState, true));
        ImGui::ProgressBar(progress, ImVec2(-1, 8.0f));
        ImGui::EndChild();
        ImGui::Spacing();

        ImGui::TextDisabled("Active mission");
        ImGui::SetWindowFontScale(1.03f);
        ImGui::TextColored(kBrandPrimary, "%s", currentResult.algorithmName.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(kTextMuted,
                           "%d stops scheduled  |  Threshold %.0f%%",
                           std::max(0, static_cast<int>(currentResult.visitOrder.size()) - 1),
                           system.getCollectionThreshold());
        ImGui::SeparatorText("Route Snapshot");

        ImGui::Columns(2, "metrics", false);
        ImGui::SetColumnWidth(0, 130);

        ImGui::TextColored(kTextSubtle, "Distance");
        ImGui::NextColumn();
        ImGui::Text("%.2f km", currentResult.totalDistance); ImGui::NextColumn();

        ImGui::TextColored(kTextSubtle, "Travel Time");
        ImGui::NextColumn();
        ImGui::Text("%.2f hrs", currentResult.travelTime); ImGui::NextColumn();

        ImGui::TextColored(kTextSubtle, "Fuel Cost");
        ImGui::NextColumn();
        ImGui::Text("RM %.2f", currentResult.fuelCost); ImGui::NextColumn();

        ImGui::TextColored(kTextSubtle, "Driver Wage");
        ImGui::NextColumn();
        ImGui::Text("RM %.2f", currentResult.wageCost); ImGui::NextColumn();

        ImGui::TextColored(kTextSubtle, "Total Cost");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.95f, 0.82f, 0.18f, 1.0f),
                           "RM %.2f", currentResult.totalCost); ImGui::NextColumn();

        ImGui::TextColored(kTextSubtle, "Collected");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.66f, 0.92f, 0.86f, 1.0f),
                           "%.1f kg", currentResult.wasteCollected);
        ImGui::NextColumn();

        ImGui::TextColored(kTextSubtle, "Nodes");
        ImGui::NextColumn();
        ImGui::Text("%d", static_cast<int>(currentResult.visitOrder.size())); ImGui::NextColumn();

        ImGui::TextColored(kTextSubtle, "Compute");
        ImGui::NextColumn();
        ImGui::Text("%.3f ms", currentResult.runtimeMs); ImGui::NextColumn();

        ImGui::Columns(1);
    }

    ImGui::End();
}

void DashboardUI::drawComparisonTable(const ComparisonManager& compMgr) {
    const DashboardStyle::BottomOverlayLayout layout =
        DashboardStyle::buildBottomOverlayLayout();
    ImGui::SetNextWindowPos(layout.comparisonPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.comparisonSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.94f);

    ImGui::Begin("Algorithm Comparison", &showComparisonTable,
                 DashboardStyle::pinnedFloatingWindowFlags());

    const auto& results = compMgr.getResults();
    if (results.empty()) {
        ImGui::TextColored(ImVec4(0.45f, 0.50f, 0.55f, 1.0f),
                           "Run 'Compare All' to populate this table.");
    } else {
        int bestIdx = compMgr.getBestAlgorithmIndex();
        ImGui::TextDisabled("Operational cost and runtime across current sea conditions.");
        ImGui::Spacing();

        if (ImGui::BeginTable("CompTable", 8,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {

            ImGui::TableSetupColumn("Algorithm");
            ImGui::TableSetupColumn("Distance (km)");
            ImGui::TableSetupColumn("Time (h)");
            ImGui::TableSetupColumn("Fuel (RM)");
            ImGui::TableSetupColumn("Wage (RM)");
            ImGui::TableSetupColumn("Total (RM)");
            ImGui::TableSetupColumn("Garbage (kg)");
            ImGui::TableSetupColumn("Runtime (ms)");
            ImGui::TableHeadersRow();

            for (int i = 0; i < static_cast<int>(results.size()); i++) {
                const auto& r = results[i];
                ImGui::TableNextRow();

                if (i == bestIdx) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                        ImGui::GetColorU32(ImVec4(0.1f, 0.4f, 0.15f, 0.4f)));
                }

                ImGui::TableSetColumnIndex(0);
                if (i == bestIdx) {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f),
                                       "%s *", r.algorithmName.c_str());
                } else {
                    ImGui::Text("%s", r.algorithmName.c_str());
                }

                ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f", r.totalDistance);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", r.travelTime);
                ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f", r.fuelCost);
                ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", r.wageCost);
                ImGui::TableSetColumnIndex(5); ImGui::Text("%.2f", r.totalCost);
                ImGui::TableSetColumnIndex(6); ImGui::Text("%.1f", r.wasteCollected);
                ImGui::TableSetColumnIndex(7); ImGui::Text("%.3f", r.runtimeMs);
            }

            ImGui::EndTable();
        }

        if (bestIdx >= 0) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f),
                               "* Recommended: %s (lowest total cost)",
                               results[bestIdx].algorithmName.c_str());
        }
    }

    ImGui::End();
}

void DashboardUI::drawNodeDetailsPanel(const WasteSystem& system) {
    ImGui::SetNextWindowPos(ImVec2(970, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(290, 400), ImGuiCond_FirstUseEver);

    ImGui::Begin("Node Details", &showNodeDetails);

    const MapGraph& graph = system.getGraph();

    if (ImGui::BeginTable("NodeTable", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY)) {

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Debris %", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("kg", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();

        for (int i = 0; i < graph.getNodeCount(); i++) {
            const WasteNode& node = graph.getNode(i);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", node.getName().c_str());

            ImGui::TableSetColumnIndex(1);
            if (node.getIsHQ()) {
                ImGui::TextColored(ImVec4(0.3f, 0.5f, 0.9f, 1.0f), "Dock");
            } else {
                Color c = RenderUtils::getUrgencyColor(node.getUrgency());
                ImGui::TextColored(ImVec4(c.r, c.g, c.b, 1.0f),
                                   "%.0f%%", node.getWasteLevel());
            }

            ImGui::TableSetColumnIndex(2);
            if (!node.getIsHQ()) {
                ImGui::Text("%.0f", node.getWasteAmount());
            }

            ImGui::TableSetColumnIndex(3);
            if (node.getIsHQ()) {
                ImGui::TextColored(ImVec4(0.3f, 0.5f, 0.9f, 1.0f), "Dock");
            } else if (node.isCollected()) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Done");
            } else if (node.isEligible(system.getCollectionThreshold())) {
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Eligible");
            } else {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Below");
            }
        }
        ImGui::EndTable();
    }

    int eligible = static_cast<int>(system.getEligibleNodes().size());
    ImGui::Text("Eligible nodes: %d / %d", eligible, graph.getNodeCount() - 1);

    ImGui::End();
}

void DashboardUI::drawEventLogPanel(const WasteSystem& system) {
    const DashboardStyle::BottomOverlayLayout layout =
        DashboardStyle::buildBottomOverlayLayout();
    ImGui::SetNextWindowPos(layout.eventLogPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.eventLogSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.93f);

    ImGui::Begin("Activity Log", &showEventLog,
                 DashboardStyle::pinnedFloatingWindowFlags());
    ImGui::TextDisabled("Latest operations");
    ImGui::Separator();

    auto events = system.getEventLog().getRecentEvents(12);

    for (const auto* entry : events) {
        ImGui::TextColored(ImVec4(0.38f, 0.44f, 0.52f, 1.0f),
                           "%s", entry->timestamp.c_str());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.72f, 0.76f, 0.80f, 1.0f),
                           "%s", entry->message.c_str());
    }

    ImGui::End();
}

void DashboardUI::drawLegendPanel() {
    const DashboardStyle::SidebarLayout layout = DashboardStyle::buildSidebarLayout();
    ImGui::SetNextWindowPos(layout.legendPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.legendSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGui::Begin("Legend", nullptr, DashboardStyle::pinnedSidebarWindowFlags());
    ImGui::TextDisabled("Urgency and cleanup state");

    Color low = RenderUtils::getUrgencyColor(UrgencyLevel::LOW);
    Color med = RenderUtils::getUrgencyColor(UrgencyLevel::MEDIUM);
    Color high = RenderUtils::getUrgencyColor(UrgencyLevel::HIGH);
    Color hq = RenderUtils::getHQColor();
    Color collected = RenderUtils::getCollectedColor();

    ImGui::ColorButton("##low", ImVec4(low.r, low.g, low.b, 1.0f));
    ImGui::SameLine(); ImGui::Text("Low (0-39%%)");

    ImGui::ColorButton("##med", ImVec4(med.r, med.g, med.b, 1.0f));
    ImGui::SameLine(); ImGui::Text("Medium (40-69%%)");

    ImGui::ColorButton("##high", ImVec4(high.r, high.g, high.b, 1.0f));
    ImGui::SameLine(); ImGui::Text("High (70-100%%) - Pulsing");

    ImGui::ColorButton("##hq", ImVec4(hq.r, hq.g, hq.b, 1.0f));
    ImGui::SameLine(); ImGui::Text("Dock / Harbor");

    ImGui::ColorButton("##done", ImVec4(collected.r, collected.g, collected.b, 1.0f));
    ImGui::SameLine(); ImGui::Text("Cleaned Up");

    ImGui::End();
}

void DashboardUI::drawRouteOrderPanel(const RouteResult& result,
                                       const WasteSystem& system,
                                       AnimationController::PlaybackState playState,
                                       float progress) {
    if (!result.isValid()) return;

    const DashboardStyle::BottomOverlayLayout layout =
        DashboardStyle::buildBottomOverlayLayout();
    ImGui::SetNextWindowPos(layout.routeOrderPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.routeOrderSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.94f);

    ImGui::Begin("Mission Route", nullptr,
                 DashboardStyle::pinnedFloatingWindowFlags());

    const ImVec4 stateTint = DashboardStyle::playbackTint(playState, true);
    const int segmentCount = std::max(1, static_cast<int>(result.visitOrder.size()) - 1);
    int activeIndex = 0;
    if (playState == AnimationController::PlaybackState::PLAYING ||
        playState == AnimationController::PlaybackState::PAUSED) {
        activeIndex = std::min(static_cast<int>(result.visitOrder.size()) - 1,
                               std::max(1, static_cast<int>(
                                   std::floor(std::clamp(progress, 0.0f, 1.0f) *
                                              static_cast<float>(segmentCount))) + 1));
    } else if (playState == AnimationController::PlaybackState::FINISHED) {
        activeIndex = static_cast<int>(result.visitOrder.size()) - 1;
    }

    ImGui::TextColored(kBrandPrimary, "%s", result.algorithmName.c_str());
    ImGui::TextColored(stateTint, "%s",
                       DashboardStyle::playbackLabel(playState, true));
    ImGui::ProgressBar(progress, ImVec2(-1, 8.0f));
    ImGui::TextDisabled("Visit order");
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(result.visitOrder.size()); i++) {
        int nodeId = result.visitOrder[i];
        int idx = system.getGraph().findNodeIndex(nodeId);
        if (idx < 0) continue;

        const WasteNode& node = system.getGraph().getNode(idx);
        const bool isCurrent =
            !node.getIsHQ() && !node.isCollected() &&
            (playState == AnimationController::PlaybackState::PLAYING ||
             playState == AnimationController::PlaybackState::PAUSED) &&
            i == activeIndex;
        const ImVec4 stepTint = isCurrent
            ? ImVec4(0.72f, 0.96f, 1.0f, 1.0f)
            : ImVec4(0.46f, 0.80f, 0.88f, 1.0f);
        ImGui::TextColored(stepTint, "%02d", i + 1);
        ImGui::SameLine();

        if (node.getIsHQ()) {
            ImGui::TextColored(ImVec4(0.3f, 0.5f, 0.9f, 1.0f),
                               "%s", node.getName().c_str());
        } else if (node.isCollected()) {
            ImGui::TextColored(ImVec4(0.54f, 0.76f, 0.66f, 1.0f),
                               "%s  [secured]", node.getName().c_str());
        } else if (isCurrent) {
            ImGui::TextColored(ImVec4(0.86f, 0.98f, 1.0f, 1.0f),
                               "%s  [inbound | %.0f%%]",
                               node.getName().c_str(), node.getWasteLevel());
        } else {
            ImGui::TextColored(kTextNeutral,
                               "%s  [%.0f%%]",
                               node.getName().c_str(), node.getWasteLevel());
        }
    }

    ImGui::End();
}

void DashboardUI::drawExportPanel(UIActions& actions) {
    const DashboardStyle::SidebarLayout layout = DashboardStyle::buildSidebarLayout();
    ImGui::SetNextWindowPos(layout.exportPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.exportSize, ImGuiCond_Always);
    ImGui::Begin("Export", nullptr, DashboardStyle::pinnedSidebarWindowFlags());

    if (ImGui::Button("Export Summary (TXT)", ImVec2(-1, 24))) {
        actions.exportResults = true;
    }
    ImGui::Spacing();
    if (ImGui::Button("Export Comparison (CSV)", ImVec2(-1, 24))) {
        actions.exportComparison = true;
    }

    ImGui::End();
}

int DashboardUI::getSelectedAlgorithm() const {
    return selectedAlgorithm;
}
