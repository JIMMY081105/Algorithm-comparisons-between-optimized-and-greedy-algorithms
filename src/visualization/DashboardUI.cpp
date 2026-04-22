#include "visualization/DashboardUI.h"

#include "visualization/DashboardStyle.h"
#include "visualization/RenderUtils.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr ImVec4 kSeaPrimary(0.15f, 0.72f, 0.94f, 1.0f);
constexpr ImVec4 kCityPrimary(0.94f, 0.72f, 0.22f, 1.0f);
constexpr ImVec4 kTextSoft(0.45f, 0.50f, 0.58f, 1.0f);
constexpr ImVec4 kTextMuted(0.46f, 0.56f, 0.64f, 1.0f);
constexpr ImVec4 kTextSubtle(0.55f, 0.60f, 0.68f, 1.0f);
constexpr ImVec4 kTextNeutral(0.76f, 0.80f, 0.84f, 1.0f);

ImVec4 brandColor(EnvironmentTheme theme) {
    return theme == EnvironmentTheme::City ? kCityPrimary : kSeaPrimary;
}

const char* themeTooltip(EnvironmentTheme theme) {
    return theme == EnvironmentTheme::City
        ? "Street-constrained routing with weather, congestion, and incidents."
        : "Open-water routing with harbour visuals and direct marine travel costs.";
}

} // namespace

DashboardUI::DashboardUI()
    : selectedAlgorithm(0),
      selectedTheme(EnvironmentTheme::City),
      selectedSeason(CitySeason::Spring),
      showComparisonTable(false),
      showEventLog(true),
      showNodeDetails(false),
      roadEventFromIdx(0),
      roadEventToIdx(1),
      roadEventTypeIdx(1) {}

DashboardUI::UIActions DashboardUI::render(WasteSystem& system,
                                           ComparisonManager& compMgr,
                                           AnimationController& animCtrl,
                                           const RouteResult& currentResult,
                                           const MissionPresentation& currentMission,
                                           const ThemeDashboardInfo& environmentInfo,
                                           const SceneLayerToggles& layerToggles,
                                           EnvironmentTheme activeTheme) {
    const auto playState = animCtrl.getState();
    const bool hasMission = currentResult.isValid();
    const bool missionRunning =
        playState == AnimationController::PlaybackState::PLAYING;
    const float missionProgress = animCtrl.getProgress();

    DashboardStyle::applyTheme(activeTheme, hasMission, missionRunning);

    UIActions actions;
    actions.selectedTheme = activeTheme;
    actions.selectedSeason = environmentInfo.season;
    actions.layerToggles = layerToggles;

    selectedTheme = activeTheme;
    selectedSeason = environmentInfo.season;

    drawHeaderPanel(system, environmentInfo, playState, hasMission);
    drawControlPanel(system, animCtrl, environmentInfo, actions);

    drawMetricsPanel(currentResult, currentMission, system, environmentInfo,
                     playState, missionProgress);
    drawLegendPanel(environmentInfo, system);
    drawRouteOrderPanel(currentResult, currentMission, system, playState,
                        missionProgress);

    if (showComparisonTable) {
        drawComparisonTable(compMgr, environmentInfo);
    }
    if (showNodeDetails) {
        drawNodeDetailsPanel(system);
    }

    drawExportPanel(actions);
    drawFuelWagePanel(system, currentResult, environmentInfo);
    drawTollOverlays(system, currentResult, activeTheme);
    drawRoadEventOverlays(system, activeTheme);

    return actions;
}

void DashboardUI::drawHeaderPanel(const WasteSystem& system,
                                  const ThemeDashboardInfo& environmentInfo,
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
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 122.0f, 10.0f));
    ImGui::TextColored(stateTint, "[%s]",
                       DashboardStyle::playbackLabel(playState, hasMission));
    ImGui::SetCursorPos(ImVec2(14.0f, 10.0f));
    ImGui::SetWindowFontScale(1.05f);
    ImGui::TextColored(brandColor(environmentInfo.theme), "ECOROUTE SOLUTIONS");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(ImVec4(0.64f, 0.78f, 0.84f, 1.0f),
                       "%s Operations", environmentInfo.themeLabel.c_str());
    if (environmentInfo.supportsWeather && environmentInfo.supportsSeasons) {
        ImGui::TextColored(kTextSoft,
                           "Day %d  |  Seed %u  |  %s  |  %s",
                           system.getDayNumber(), system.getCurrentSeed(),
                           environmentInfo.seasonLabel.c_str(),
                           environmentInfo.weatherLabel.c_str());
    } else {
        ImGui::TextColored(kTextSoft,
                           "Day %d  |  Seed %u  |  %s",
                           system.getDayNumber(), system.getCurrentSeed(),
                           environmentInfo.supportsWeather
                               ? environmentInfo.weatherLabel.c_str()
                               : "Standard Conditions");
    }

    ImGui::End();
}

void DashboardUI::drawControlPanel(WasteSystem& system,
                                   AnimationController& animCtrl,
                                   const ThemeDashboardInfo& environmentInfo,
                                   UIActions& actions) {
    const auto playState = animCtrl.getState();
    const bool hasMission = animCtrl.getCurrentRoute().isValid();
    const float missionProgress = animCtrl.getProgress();
    const DashboardStyle::SidebarLayout layout = DashboardStyle::buildSidebarLayout();

    ImGui::SetNextWindowPos(layout.controlsPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.controlsSize, ImGuiCond_Always);
    ImGui::Begin("Operations", nullptr, DashboardStyle::pinnedSidebarWindowFlags());

    ImGui::SeparatorText("Environment");
    if (ImGui::RadioButton("Sea", selectedTheme == EnvironmentTheme::Sea)) {
        selectedTheme = EnvironmentTheme::Sea;
        actions.changeTheme = true;
        actions.selectedTheme = selectedTheme;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", themeTooltip(EnvironmentTheme::Sea));
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("City", selectedTheme == EnvironmentTheme::City)) {
        selectedTheme = EnvironmentTheme::City;
        actions.changeTheme = true;
        actions.selectedTheme = selectedTheme;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", themeTooltip(EnvironmentTheme::City));
    }

    if (environmentInfo.supportsWeather) {
        if (environmentInfo.supportsSeasons && selectedTheme == EnvironmentTheme::City) {
            ImGui::TextDisabled("Season");
            static const char* seasonLabels[] = {"Spring", "Summer", "Autumn", "Winter"};
            int seasonIndex = static_cast<int>(selectedSeason);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::Combo("##season", &seasonIndex, seasonLabels,
                             IM_ARRAYSIZE(seasonLabels))) {
                selectedSeason = static_cast<CitySeason>(seasonIndex);
                actions.changeSeason = true;
                actions.selectedSeason = selectedSeason;
            }
        }

        ImGui::TextDisabled("Weather");
        ImGui::TextColored(brandColor(environmentInfo.theme), "%s",
                           environmentInfo.weatherLabel.c_str());
        if (ImGui::Button("Refresh Weather", ImVec2(-1, 22))) {
            actions.randomizeWeather = true;
        }
    }

    if (selectedTheme == EnvironmentTheme::City) {
        ImGui::Spacing();
        ImGui::TextDisabled("Visual layers");
        bool changed = false;
        changed |= ImGui::Checkbox("Traffic", &actions.layerToggles.showTraffic);
        ImGui::SameLine();
        changed |= ImGui::Checkbox("Congestion", &actions.layerToggles.showCongestion);
        changed |= ImGui::Checkbox("Incidents", &actions.layerToggles.showIncidents);
        ImGui::SameLine();
        changed |= ImGui::Checkbox("Traffic lights", &actions.layerToggles.showTrafficLights);
        changed |= ImGui::Checkbox("Route graph", &actions.layerToggles.showRouteGraph);
        actions.layerTogglesChanged = changed;
    }

    ImGui::SeparatorText("Simulation");
    if (ImGui::Button("Generate New Day", ImVec2(-1, 24))) {
        actions.generateNewDay = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Randomize waste levels, city weather, and traffic conditions while keeping the environment layout fixed.");
    }

    float threshold = system.getCollectionThreshold();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##threshold", &threshold, 5.0f, 80.0f,
                           "Threshold: %.0f%%")) {
        system.setCollectionThreshold(threshold);
    }

    ImGui::SeparatorText("Algorithm");
    // Row 1: the four classic routing strategies
    struct AlgoEntry { const char* label; const char* tooltip; };
    static const AlgoEntry kAlgoEntries[] = {
        {"Regular",      "Baseline: visits nodes in default ID order, no optimisation."},
        {"Greedy",       "Nearest-neighbour: always picks the closest unvisited node next."},
        {"MST",          "Prim's MST + double-tree shortcut (2-approximation of TSP)."},
        {"TSP",          "Optimal bitmask DP (exact for n<=12); NN + 2-opt otherwise."},
        {"Dijkstra",     "Dijkstra shortest paths from HQ; visits nodes in depot-distance order."},
        {"B-Ford",       "Bellman-Ford shortest paths + cheapest-insertion tour construction."},
        {"Floyd-W",      "Floyd-Warshall all-pairs paths + farthest-insertion tour construction."},
    };
    constexpr int kAlgoCount = 7;
    for (int index = 0; index < kAlgoCount; ++index) {
        ImGui::RadioButton(kAlgoEntries[index].label, &selectedAlgorithm, index);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", kAlgoEntries[index].tooltip);
        }
        // 4 per row on first row, 3 on second
        const bool endOfRow = (index == 3) || (index == kAlgoCount - 1);
        if (!endOfRow) {
            ImGui::SameLine();
        }
    }

    if (ImGui::Button("Run Selected", ImVec2(-1, 23))) {
        actions.runSelectedAlgorithm = true;
        actions.algorithmToRun = selectedAlgorithm;
    }
    if (ImGui::Button("Compare All", ImVec2(-1, 23))) {
        actions.compareAll = true;
        showComparisonTable = true;
    }

    ImGui::SeparatorText("Playback");
    const float halfButtonWidth =
        (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (playState == AnimationController::PlaybackState::IDLE ||
        playState == AnimationController::PlaybackState::FINISHED) {
        if (ImGui::Button("Play", ImVec2(halfButtonWidth, 22))) {
            actions.playPause = true;
        }
    } else if (playState == AnimationController::PlaybackState::PLAYING) {
        if (ImGui::Button("Pause", ImVec2(halfButtonWidth, 22))) {
            animCtrl.pause();
        }
    } else {
        if (ImGui::Button("Resume", ImVec2(halfButtonWidth, 22))) {
            animCtrl.resume();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Replay", ImVec2(halfButtonWidth, 22))) {
        actions.replay = true;
    }

    float speed = animCtrl.getSpeed();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##speed", &speed, 0.1f, 5.0f, "Speed: %.1fx")) {
        animCtrl.setSpeed(speed);
    }

    ImGui::TextDisabled("Playback Progress");
    ImGui::ProgressBar(missionProgress, ImVec2(-1, 12));
    ImGui::TextColored(DashboardStyle::playbackTint(playState, hasMission),
                       "%s", DashboardStyle::playbackLabel(playState, hasMission));

    // Road Events (city only)
    if (selectedTheme == EnvironmentTheme::City) {
        ImGui::SeparatorText("Road Events");

        const MapGraph& g = system.getGraph();
        const auto& nodes = g.getNodes();
        const int nodeCount = g.getNodeCount();

        if (roadEventFromIdx >= nodeCount) roadEventFromIdx = 0;
        if (roadEventToIdx   >= nodeCount) roadEventToIdx   = std::min(1, nodeCount - 1);

        std::vector<std::string> nodeNames;
        nodeNames.reserve(nodeCount);
        for (const auto& n : nodes) nodeNames.push_back(n.getName());

        auto nameGetter = [](void* d, int i, const char** out) -> bool {
            const auto& v = *static_cast<std::vector<std::string>*>(d);
            if (i < 0 || i >= static_cast<int>(v.size())) return false;
            *out = v[i].c_str();
            return true;
        };

        ImGui::TextDisabled("From");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::Combo("##reFrom", &roadEventFromIdx, nameGetter, &nodeNames, nodeCount);
        ImGui::TextDisabled("To");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::Combo("##reTo",   &roadEventToIdx,   nameGetter, &nodeNames, nodeCount);

        static const char* kEventTypes[] = {
            "None (clear)", "Flood", "Festival"
        };
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::Combo("##reType", &roadEventTypeIdx, kEventTypes, 3);

        const bool sameNode = (roadEventFromIdx == roadEventToIdx);
        if (sameNode) ImGui::BeginDisabled();
        if (ImGui::Button("Apply & Re-run##re", ImVec2(-1.0f, 22.0f))) {
            const int fromId = nodes[roadEventFromIdx].getId();
            const int toId   = nodes[roadEventToIdx].getId();
            const RoadEvent ev = static_cast<RoadEvent>(roadEventTypeIdx);
            system.getGraph().setEdgeEvent(fromId, toId, ev);

            char logMsg[100];
            if (ev == RoadEvent::NONE) {
                std::snprintf(logMsg, sizeof(logMsg), "Cleared: %s <-> %s",
                              nodeNames[roadEventFromIdx].c_str(),
                              nodeNames[roadEventToIdx].c_str());
            } else {
                std::snprintf(logMsg, sizeof(logMsg), "%s: %s <-> %s",
                              roadEventFullName(ev),
                              nodeNames[roadEventFromIdx].c_str(),
                              nodeNames[roadEventToIdx].c_str());
            }
            system.getEventLog().addEvent(logMsg);
            actions.runSelectedAlgorithm = true;
            actions.algorithmToRun = selectedAlgorithm;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Sets the road condition and immediately re-runs\nthe selected algorithm to update the route.");
        }
        if (sameNode) {
            ImGui::EndDisabled();
            ImGui::TextColored(ImVec4(0.85f, 0.50f, 0.20f, 1.0f), "Select different nodes");
        }

        // Active event list
        const auto activeEvents = system.getGraph().getActiveEdgeEvents();
        if (!activeEvents.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Active:");
            for (const auto& ev : activeEvents) {
                const int ia = g.findNodeIndex(ev.fromId);
                const int ib = g.findNodeIndex(ev.toId);
                const char* na = (ia >= 0) ? nodes[ia].getName().c_str() : "?";
                const char* nb = (ib >= 0) ? nodes[ib].getName().c_str() : "?";

                const ImVec4 col =
                    (ev.type == RoadEvent::FLOOD)
                        ? ImVec4(0.22f, 0.48f, 0.95f, 1.0f)   // dark blue
                        : ImVec4(0.72f, 0.48f, 0.20f, 1.0f);  // brown

                ImGui::TextColored(col, "[%s]", roadEventLabel(ev.type));
                ImGui::SameLine();
                ImGui::TextColored(kTextSoft, "%s-%s", na, nb);
                ImGui::SameLine();
                char btnId[32];
                std::snprintf(btnId, sizeof(btnId), "X##clr%d_%d", ev.fromId, ev.toId);
                if (ImGui::SmallButton(btnId)) {
                    system.getGraph().setEdgeEvent(ev.fromId, ev.toId, RoadEvent::NONE);
                    char logMsg[80];
                    std::snprintf(logMsg, sizeof(logMsg), "Cleared: %s <-> %s", na, nb);
                    system.getEventLog().addEvent(logMsg);
                }
            }
            if (ImGui::SmallButton("Clear All Events##reAll")) {
                system.getGraph().clearAllEvents();
                system.getEventLog().addEvent("All road events cleared");
            }
        }
    }

    ImGui::SeparatorText("Panels");
    ImGui::Checkbox("Comparison", &showComparisonTable);
    ImGui::SameLine();
    ImGui::Checkbox("Log", &showEventLog);
    ImGui::Checkbox("Node Details", &showNodeDetails);

    ImGui::End();
}

void DashboardUI::drawMetricsPanel(const RouteResult& currentResult,
                                   const MissionPresentation& currentMission,
                                   const WasteSystem& system,
                                   const ThemeDashboardInfo& environmentInfo,
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
        ImGui::TextColored(brandColor(environmentInfo.theme), "%s",
                           currentResult.algorithmName.c_str());
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

        ImGui::TextColored(kTextSubtle, "Wage (Base)");
        ImGui::NextColumn();
        ImGui::Text("RM %.2f", currentResult.basePay); ImGui::NextColumn();

        ImGui::TextColored(kTextSubtle, "Wage (+/km)");
        ImGui::NextColumn();
        ImGui::Text("RM %.2f", currentResult.perKmBonus); ImGui::NextColumn();

        ImGui::TextColored(kTextSubtle, "Efficiency +");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.30f, 0.90f, 0.50f, 1.0f),
                           "RM %.2f", currentResult.efficiencyBonus); ImGui::NextColumn();

        ImGui::TextColored(kTextSubtle, "Toll Cost");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.99f, 0.75f, 0.20f, 1.0f),
                           "RM %.2f", currentResult.tollCost); ImGui::NextColumn();

        ImGui::TextColored(kTextSubtle, "Total Cost");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.95f, 0.82f, 0.18f, 1.0f),
                           "RM %.2f", currentResult.totalCost); ImGui::NextColumn();

        ImGui::TextColored(kTextSubtle, "Collected");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.66f, 0.92f, 0.86f, 1.0f),
                           "%.1f kg", currentResult.wasteCollected);
        ImGui::NextColumn();

        ImGui::TextColored(kTextSubtle, "Compute");
        ImGui::NextColumn();
        ImGui::Text("%.3f ms", currentResult.runtimeMs); ImGui::NextColumn();

        ImGui::Columns(1);

        ImGui::SeparatorText("Environment");
        ImGui::TextColored(brandColor(environmentInfo.theme), "%s",
                           environmentInfo.atmosphereLabel.c_str());
        if (environmentInfo.supportsSeasons) {
            ImGui::TextColored(kTextSoft, "Season: %s",
                               environmentInfo.seasonLabel.c_str());
        }
        ImGui::TextColored(kTextSoft, "Weather: %s", environmentInfo.weatherLabel.c_str());
        ImGui::TextColored(kTextSoft, "Congestion: %.0f%%",
                           environmentInfo.congestionLevel * 100.0f);
        ImGui::TextColored(kTextSoft, "Incidents: %d", environmentInfo.incidentCount);

        if (!currentMission.narrative.empty()) {
            ImGui::Spacing();
            ImGui::TextWrapped("%s", currentMission.narrative.c_str());
        }
    }

    ImGui::End();
}

void DashboardUI::drawComparisonTable(const ComparisonManager& compMgr,
                                      const ThemeDashboardInfo& environmentInfo) {
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
        const int bestIdx = compMgr.getBestAlgorithmIndex();
        ImGui::TextDisabled("Operational cost and runtime under %s conditions.",
                            environmentInfo.themeLabel.c_str());
        ImGui::Spacing();

        if (ImGui::BeginTable("CompTable", 11,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingStretchProp |
                                  ImGuiTableFlags_PadOuterX |
                                  ImGuiTableFlags_ScrollX)) {

            ImGui::TableSetupColumn("Algorithm");
            ImGui::TableSetupColumn("Dist (km)");
            ImGui::TableSetupColumn("Time (h)");
            ImGui::TableSetupColumn("Fuel (RM)");
            ImGui::TableSetupColumn("Base Pay");
            ImGui::TableSetupColumn("+/km");
            ImGui::TableSetupColumn("Effic +");
            ImGui::TableSetupColumn("Toll (RM)");
            ImGui::TableSetupColumn("Total (RM)");
            ImGui::TableSetupColumn("Garbage (kg)");
            ImGui::TableSetupColumn("Runtime (ms)");
            ImGui::TableHeadersRow();

            for (int i = 0; i < static_cast<int>(results.size()); ++i) {
                const auto& r = results[i];
                ImGui::TableNextRow();

                if (i == bestIdx) {
                    ImGui::TableSetBgColor(
                        ImGuiTableBgTarget_RowBg1,
                        ImGui::GetColorU32(ImVec4(0.10f, 0.40f, 0.15f, 0.4f)));
                }

                ImGui::TableSetColumnIndex(0);
                if (i == bestIdx) {
                    ImGui::TextColored(ImVec4(0.30f, 1.0f, 0.40f, 1.0f),
                                       "%s *", r.algorithmName.c_str());
                } else {
                    ImGui::Text("%s", r.algorithmName.c_str());
                }

                ImGui::TableSetColumnIndex(1);  ImGui::Text("%.2f", r.totalDistance);
                ImGui::TableSetColumnIndex(2);  ImGui::Text("%.2f", r.travelTime);
                ImGui::TableSetColumnIndex(3);  ImGui::Text("%.2f", r.fuelCost);
                ImGui::TableSetColumnIndex(4);  ImGui::Text("%.2f", r.basePay);
                ImGui::TableSetColumnIndex(5);  ImGui::Text("%.2f", r.perKmBonus);
                ImGui::TableSetColumnIndex(6);
                ImGui::TextColored(ImVec4(0.30f, 0.90f, 0.50f, 1.0f),
                                   "%.2f", r.efficiencyBonus);
                ImGui::TableSetColumnIndex(7);
                ImGui::TextColored(ImVec4(0.99f, 0.75f, 0.20f, 1.0f),
                                   "%.2f", r.tollCost);
                ImGui::TableSetColumnIndex(8);
                ImGui::TextColored(ImVec4(0.95f, 0.82f, 0.18f, 1.0f),
                                   "%.2f", r.totalCost);
                ImGui::TableSetColumnIndex(9);  ImGui::Text("%.1f", r.wasteCollected);
                ImGui::TableSetColumnIndex(10); ImGui::Text("%.3f", r.runtimeMs);
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
}

void DashboardUI::drawNodeDetailsPanel(const WasteSystem& system) {
    const DashboardStyle::SidebarLayout layout = DashboardStyle::buildSidebarLayout();
    ImGui::SetNextWindowPos(layout.nodeDetailsPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.nodeDetailsSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGui::Begin("Node Details", &showNodeDetails,
                 DashboardStyle::pinnedSidebarWindowFlags());

    const MapGraph& graph = system.getGraph();
    if (ImGui::BeginTable("NodeTable", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Debris %", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("kg", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();

        for (int i = 0; i < graph.getNodeCount(); ++i) {
            const WasteNode& node = graph.getNode(i);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", node.getName().c_str());

            ImGui::TableSetColumnIndex(1);
            if (node.getIsHQ()) {
                ImGui::TextColored(ImVec4(0.3f, 0.5f, 0.9f, 1.0f), "HQ");
            } else {
                const Color c = RenderUtils::getUrgencyColor(node.getUrgency());
                ImGui::TextColored(ImVec4(c.r, c.g, c.b, 1.0f),
                                   "%.0f%%", node.getWasteLevel());
            }

            ImGui::TableSetColumnIndex(2);
            if (!node.getIsHQ()) {
                ImGui::Text("%.0f", node.getWasteAmount());
            }

            ImGui::TableSetColumnIndex(3);
            if (node.getIsHQ()) {
                ImGui::TextColored(ImVec4(0.3f, 0.5f, 0.9f, 1.0f), "HQ");
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

    const int eligible = static_cast<int>(system.getEligibleNodes().size());
    ImGui::Text("Eligible nodes: %d / %d", eligible, graph.getNodeCount() - 1);

    ImGui::End();
}

void DashboardUI::drawLegendPanel(const ThemeDashboardInfo& environmentInfo,
                                  const WasteSystem& system) {
    const DashboardStyle::SidebarLayout layout = DashboardStyle::buildSidebarLayout();
    ImGui::SetNextWindowPos(layout.legendPos, ImGuiCond_Always);
    // Cap height to the gap between top-of-legend and top-of-fuel-wage panel.
    // This ensures legend, fuel wage, and chatbot icon never overlap.
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(layout.legendSize.x, 0.0f),
        ImVec2(layout.legendSize.x, std::max(50.0f, layout.legendSize.y)));
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGui::Begin("Legend", nullptr,
                 DashboardStyle::pinnedSidebarWindowFlags() |
                 ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::TextDisabled("Urgency and environment");

    const Color low = RenderUtils::getUrgencyColor(UrgencyLevel::LOW);
    const Color med = RenderUtils::getUrgencyColor(UrgencyLevel::MEDIUM);
    const Color high = RenderUtils::getUrgencyColor(UrgencyLevel::HIGH);
    const Color hq = RenderUtils::getHQColor();
    const Color collected = RenderUtils::getCollectedColor();

    ImGui::ColorButton("##low", ImVec4(low.r, low.g, low.b, 1.0f));
    ImGui::SameLine(); ImGui::Text("Low (0-39%%)");
    ImGui::ColorButton("##med", ImVec4(med.r, med.g, med.b, 1.0f));
    ImGui::SameLine(); ImGui::Text("Medium (40-69%%)");
    ImGui::ColorButton("##high", ImVec4(high.r, high.g, high.b, 1.0f));
    ImGui::SameLine(); ImGui::Text("High (70-100%%)");
    ImGui::ColorButton("##hq", ImVec4(hq.r, hq.g, hq.b, 1.0f));
    ImGui::SameLine(); ImGui::Text("HQ / Control");
    ImGui::ColorButton("##done", ImVec4(collected.r, collected.g, collected.b, 1.0f));
    ImGui::SameLine(); ImGui::Text("Completed");

    drawTrafficLegend();

    if (selectedTheme == EnvironmentTheme::City) {
        ImGui::Separator();
        ImGui::TextDisabled("Road events");
        ImGui::ColorButton("##flood", ImVec4(0.12f, 0.31f, 0.78f, 1.0f),
                           ImGuiColorEditFlags_NoTooltip);
        ImGui::SameLine(); ImGui::Text("Flood (impassable)");
        ImGui::ColorButton("##fest",  ImVec4(0.55f, 0.31f, 0.10f, 1.0f),
                           ImGuiColorEditFlags_NoTooltip);
        ImGui::SameLine(); ImGui::Text("Festival (closed)");
    }

    ImGui::Separator();
    ImGui::TextWrapped("%s", environmentInfo.atmosphereLabel.c_str());

    if (showEventLog) {
        ImGui::SeparatorText("Activity Log");
        const auto events = system.getEventLog().getRecentEvents(12);
        for (const auto* entry : events) {
            ImGui::TextColored(ImVec4(0.38f, 0.44f, 0.52f, 1.0f),
                               "%s", entry->timestamp.c_str());
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.72f, 0.76f, 0.80f, 1.0f),
                               "%s", entry->message.c_str());
        }
    }

    ImGui::End();
}

void DashboardUI::drawTrafficLegend() {
    ImGui::Separator();
    ImGui::TextDisabled("Road conditions");

    const ImU32 slowColor   = IM_COL32(220, 120, 40, 235);
    const ImU32 jamColor    = IM_COL32(210, 40, 35, 240);
    const ImU32 snowColor   = IM_COL32(240, 245, 250, 240);
    const ImU32 borderColor = IM_COL32(40, 50, 60, 200);

    const float barHeight = 10.0f;
    const float barWidth  = ImGui::GetContentRegionAvail().x;
    const ImVec2 origin   = ImGui::GetCursorScreenPos();
    const float segment   = barWidth / 3.0f;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilledMultiColor(
        ImVec2(origin.x, origin.y),
        ImVec2(origin.x + segment, origin.y + barHeight),
        slowColor, slowColor, slowColor, slowColor);
    draw->AddRectFilledMultiColor(
        ImVec2(origin.x + segment, origin.y),
        ImVec2(origin.x + segment * 2.0f, origin.y + barHeight),
        jamColor, jamColor, jamColor, jamColor);
    draw->AddRectFilledMultiColor(
        ImVec2(origin.x + segment * 2.0f, origin.y),
        ImVec2(origin.x + barWidth, origin.y + barHeight),
        snowColor, snowColor, snowColor, snowColor);
    draw->AddRect(
        ImVec2(origin.x, origin.y),
        ImVec2(origin.x + barWidth, origin.y + barHeight),
        borderColor, 0.0f, 0, 1.0f);

    ImGui::Dummy(ImVec2(barWidth, barHeight));

    ImGui::TextColored(ImVec4(0.86f, 0.48f, 0.18f, 1.0f), "Slow");
    ImGui::SameLine(segment + 4.0f);
    ImGui::TextColored(ImVec4(0.90f, 0.24f, 0.20f, 1.0f), "Jam");
    ImGui::SameLine(segment * 2.0f + 4.0f);
    ImGui::TextColored(ImVec4(0.92f, 0.95f, 0.98f, 1.0f), "Snow block");
}

void DashboardUI::drawRouteOrderPanel(const RouteResult& result,
                                      const MissionPresentation& currentMission,
                                      const WasteSystem& system,
                                      AnimationController::PlaybackState playState,
                                      float progress) {
    if (!result.isValid()) {
        return;
    }

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

    const bool isCity = selectedTheme == EnvironmentTheme::City;
    const ImVec4 brandTint = isCity ? kCityPrimary : kSeaPrimary;
    const ImVec4 stepActive = isCity
        ? ImVec4(1.0f, 0.88f, 0.55f, 1.0f)
        : ImVec4(0.72f, 0.96f, 1.0f, 1.0f);
    const ImVec4 stepNormal = isCity
        ? ImVec4(0.82f, 0.64f, 0.30f, 1.0f)
        : ImVec4(0.46f, 0.80f, 0.88f, 1.0f);
    const ImVec4 hqTint = isCity
        ? ImVec4(0.90f, 0.70f, 0.25f, 1.0f)
        : ImVec4(0.3f, 0.5f, 0.9f, 1.0f);
    const ImVec4 collectedTint = isCity
        ? ImVec4(0.60f, 0.75f, 0.50f, 1.0f)
        : ImVec4(0.54f, 0.76f, 0.66f, 1.0f);
    const ImVec4 currentTint = isCity
        ? ImVec4(1.0f, 0.92f, 0.70f, 1.0f)
        : ImVec4(0.86f, 0.98f, 1.0f, 1.0f);

    ImGui::TextColored(brandTint, "%s", result.algorithmName.c_str());
    ImGui::TextColored(stateTint, "%s",
                       DashboardStyle::playbackLabel(playState, true));
    ImGui::ProgressBar(progress, ImVec2(-1, 8.0f));
    ImGui::TextDisabled("Visit order");
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(result.visitOrder.size()); ++i) {
        const int nodeId = result.visitOrder[i];
        const int idx = system.getGraph().findNodeIndex(nodeId);
        if (idx < 0) {
            continue;
        }

        const WasteNode& node = system.getGraph().getNode(idx);
        const bool isCurrent =
            !node.getIsHQ() && !node.isCollected() &&
            (playState == AnimationController::PlaybackState::PLAYING ||
             playState == AnimationController::PlaybackState::PAUSED) &&
            i == activeIndex;
        const ImVec4 stepTint = isCurrent ? stepActive : stepNormal;
        ImGui::TextColored(stepTint, "%02d", i + 1);
        ImGui::SameLine();

        if (node.getIsHQ()) {
            ImGui::TextColored(hqTint, "%s", node.getName().c_str());
        } else if (node.isCollected()) {
            ImGui::TextColored(collectedTint,
                               "%s  [secured]", node.getName().c_str());
        } else if (isCurrent) {
            ImGui::TextColored(currentTint,
                               "%s  [inbound | %.0f%%]",
                               node.getName().c_str(), node.getWasteLevel());
        } else {
            ImGui::TextColored(kTextNeutral,
                               "%s  [%.0f%%]",
                               node.getName().c_str(), node.getWasteLevel());
        }
    }

    if (!currentMission.legInsights.empty()) {
        ImGui::SeparatorText("Current leg");
        const int insightIndex = std::max(0, std::min(
            activeIndex - 1, static_cast<int>(currentMission.legInsights.size()) - 1));
        const RouteInsight& insight = currentMission.legInsights[insightIndex];
        ImGui::Text("Base %.2f km", insight.baseDistance);
        ImGui::Text("Traffic %.2f  |  Incidents %.2f", insight.congestionPenalty,
                    insight.incidentPenalty);
        ImGui::Text("Signals %.2f  |  Weather %.2f", insight.signalPenalty,
                    insight.weatherPenalty);
    }

    ImGui::End();
}

void DashboardUI::drawExportPanel(UIActions& actions) {
    const DashboardStyle::SidebarLayout layout = DashboardStyle::buildSidebarLayout();
    ImGui::SetNextWindowPos(layout.exportPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.exportSize, ImGuiCond_Always);
    ImGui::Begin("Export", nullptr, DashboardStyle::pinnedSidebarWindowFlags());

    if (ImGui::Button("Export Summary (TXT)", ImVec2(-1, 22))) {
        actions.exportResults = true;
    }
    ImGui::Spacing();
    if (ImGui::Button("Export Comparison (CSV)", ImVec2(-1, 22))) {
        actions.exportComparison = true;
    }

    ImGui::End();
}

int DashboardUI::getSelectedAlgorithm() const {
    return selectedAlgorithm;
}

// ---------------------------------------------------------------------------
// Fuel & Wage Panel — shows today's live fuel price and the wage model
// ---------------------------------------------------------------------------
void DashboardUI::drawFuelWagePanel(const WasteSystem& system,
                                    const RouteResult& currentResult,
                                    const ThemeDashboardInfo& environmentInfo) {
    const DashboardStyle::SidebarLayout layout = DashboardStyle::buildSidebarLayout();
    ImGui::SetNextWindowPos(layout.fuelWagePos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.fuelWageSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGui::Begin("Fuel & Wage", nullptr, DashboardStyle::pinnedSidebarWindowFlags());

    // --- Today's Fuel Price ---
    const float price = system.getDailyFuelPricePerLitre();
    const float fuelCostPerKm = system.getCostModel().getFuelCostPerKm();

    ImGui::SeparatorText("Today's Fuel Price");
    const ImVec4 priceColor = (price < 2.60f)
        ? ImVec4(0.30f, 0.90f, 0.40f, 1.0f)    // green = cheap
        : (price < 3.20f)
            ? ImVec4(0.99f, 0.82f, 0.10f, 1.0f) // yellow = moderate
            : ImVec4(0.95f, 0.30f, 0.25f, 1.0f); // red = expensive

    ImGui::SetWindowFontScale(1.10f);
    ImGui::TextColored(priceColor, "RM %.2f / litre", price);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(kTextSoft, "%.2f L/km  ->  RM %.4f / km",
                       system.getCostModel().getLitresPerKm(), fuelCostPerKm);

    // Visual price bar (RM 2.00 = left, RM 3.80 = right)
    const float barW = ImGui::GetContentRegionAvail().x;
    const float barH = 8.0f;
    const ImVec2 bMin = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(bMin, ImVec2(bMin.x + barW, bMin.y + barH),
                        IM_COL32(40, 50, 65, 200), 3.0f);
    const float fill = (price - 2.00f) / 1.80f;
    const ImU32 barFill = (price < 2.60f)
        ? IM_COL32(50, 200, 80, 220)
        : (price < 3.20f)
            ? IM_COL32(230, 190, 30, 220)
            : IM_COL32(210, 60, 50, 220);
    draw->AddRectFilled(bMin,
                        ImVec2(bMin.x + barW * std::clamp(fill, 0.0f, 1.0f),
                               bMin.y + barH),
                        barFill, 3.0f);
    ImGui::Dummy(ImVec2(barW, barH + 2.0f));

    ImGui::TextColored(ImVec4(0.38f, 0.44f, 0.52f, 1.0f),
                       "RM 2.00                      RM 3.80");

    // --- Toll Stations ---
    ImGui::Spacing();
    ImGui::SeparatorText("Toll Stations");
    const auto& tolls = system.getTollStations();
    ImGui::Columns(2, "tollcols", false);
    ImGui::SetColumnWidth(0, 110);
    for (const auto& t : tolls) {
        // Check if this toll was crossed in the current route
        bool crossed = false;
        for (const auto& name : currentResult.tollsCrossed) {
            if (name == t.name) { crossed = true; break; }
        }
        if (crossed) {
            ImGui::TextColored(ImVec4(0.99f, 0.75f, 0.20f, 1.0f),
                               "%s", t.name.c_str());
            ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.99f, 0.75f, 0.20f, 1.0f),
                               "+RM %.2f", t.fee);
        } else {
            ImGui::TextColored(kTextSoft, "%s", t.name.c_str());
            ImGui::NextColumn();
            ImGui::TextColored(kTextSoft, "RM %.2f", t.fee);
        }
        ImGui::NextColumn();
    }
    ImGui::Columns(1);

    if (currentResult.isValid()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.99f, 0.75f, 0.20f, 1.0f),
                           "Crossed: %d  (RM %.2f)",
                           static_cast<int>(currentResult.tollsCrossed.size()),
                           currentResult.tollCost);
    }

    // --- Wage Model Summary ---
    ImGui::Spacing();
    ImGui::SeparatorText("Wage Model");
    const CostModel& cm = system.getCostModel();
    ImGui::TextColored(kTextSoft, "Base shift pay:  RM %.2f", cm.getBaseWagePerShift());
    ImGui::TextColored(kTextSoft, "Per-km bonus:    RM %.2f/km", cm.getWagePerKmBonus());
    ImGui::TextDisabled("Efficiency bonus tiers:");
    ImGui::TextColored(ImVec4(0.30f, 0.90f, 0.50f, 1.0f), " <80 km  -> +RM 25.00");
    ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.40f, 1.0f), " <120 km -> +RM 15.00");
    ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.30f, 1.0f), " <180 km -> +RM  8.00");
    ImGui::TextColored(ImVec4(0.85f, 0.55f, 0.25f, 1.0f), " <250 km -> +RM  3.00");
    ImGui::TextColored(kTextSoft,                          " >=250 km-> +RM  0.00");

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Toll overlay — draws gold booth markers and "+RM" labels on the 3D map
// using ImGui's foreground draw list (screen-space, no OpenGL needed).
// ---------------------------------------------------------------------------
void DashboardUI::drawTollOverlays(const WasteSystem& system,
                                   const RouteResult& currentResult,
                                   EnvironmentTheme activeTheme) {
    if (activeTheme != EnvironmentTheme::City) return;

    const auto& tolls = system.getTollStations();
    const MapGraph& g = system.getGraph();
    ImDrawList* fg    = ImGui::GetForegroundDrawList();

    for (const auto& toll : tolls) {
        const int idxA = g.findNodeIndex(toll.nodeA);
        const int idxB = g.findNodeIndex(toll.nodeB);
        if (idxA < 0 || idxB < 0) continue;

        const WasteNode& nA = g.getNode(idxA);
        const WasteNode& nB = g.getNode(idxB);

        const float mx = (nA.getWorldX() + nB.getWorldX()) * 0.5f;
        const float my = (nA.getWorldY() + nB.getWorldY()) * 0.5f;
        const IsoCoord sc = RenderUtils::worldToIso(mx, my);

        bool crossed = false;
        for (const auto& name : currentResult.tollsCrossed) {
            if (name == toll.name) { crossed = true; break; }
        }

        // Barrier gate: two dark pillars + striped horizontal arm
        constexpr float kPillarW  = 5.0f;
        constexpr float kPillarH  = 14.0f;
        constexpr float kArmLen   = 24.0f;
        constexpr float kArmH     = 5.0f;
        constexpr int   kStripes  = 4;
        const ImU32 pillarCol     = crossed
            ? IM_COL32(60, 65, 78, 255)
            : IM_COL32(50, 54, 65, 200);

        // Left pillar
        fg->AddRectFilled(
            ImVec2(sc.x - kArmLen * 0.5f - kPillarW, sc.y - kPillarH * 0.5f),
            ImVec2(sc.x - kArmLen * 0.5f,             sc.y + kPillarH * 0.5f),
            pillarCol, 1.5f);
        // Right pillar
        fg->AddRectFilled(
            ImVec2(sc.x + kArmLen * 0.5f,             sc.y - kPillarH * 0.5f),
            ImVec2(sc.x + kArmLen * 0.5f + kPillarW,  sc.y + kPillarH * 0.5f),
            pillarCol, 1.5f);

        // Striped boom arm — orange/white when uncrossed, red/white when crossed
        const float stripeW = kArmLen / kStripes;
        for (int i = 0; i < kStripes; ++i) {
            const ImU32 col = (i % 2 == 0)
                ? (crossed ? IM_COL32(230, 60, 50, 255) : IM_COL32(255, 135, 0, 230))
                : IM_COL32(230, 230, 230, 230);
            fg->AddRectFilled(
                ImVec2(sc.x - kArmLen * 0.5f + i * stripeW,
                       sc.y - kArmH * 0.5f),
                ImVec2(sc.x - kArmLen * 0.5f + (i + 1) * stripeW,
                       sc.y + kArmH * 0.5f),
                col);
        }
        fg->AddRect(
            ImVec2(sc.x - kArmLen * 0.5f, sc.y - kArmH * 0.5f),
            ImVec2(sc.x + kArmLen * 0.5f, sc.y + kArmH * 0.5f),
            IM_COL32(70, 70, 80, 200), 0.0f, 0, 1.0f);

        // "+RM X.XX" notification pill — only when crossed
        if (crossed) {
            char label[24];
            std::snprintf(label, sizeof(label), "+RM %.2f", toll.fee);
            const ImVec2 tSize = ImGui::CalcTextSize(label);
            constexpr float kPad = 5.0f;
            const float pillY = sc.y - kPillarH * 0.5f - tSize.y - kPad * 2.0f - 4.0f;
            fg->AddRectFilled(
                ImVec2(sc.x - tSize.x * 0.5f - kPad, pillY),
                ImVec2(sc.x + tSize.x * 0.5f + kPad, pillY + tSize.y + kPad * 1.2f),
                IM_COL32(255, 200, 20, 245), 5.0f);
            fg->AddRect(
                ImVec2(sc.x - tSize.x * 0.5f - kPad, pillY),
                ImVec2(sc.x + tSize.x * 0.5f + kPad, pillY + tSize.y + kPad * 1.2f),
                IM_COL32(210, 155, 0, 200), 5.0f, 0, 1.0f);
            fg->AddText(
                ImVec2(sc.x - tSize.x * 0.5f, pillY + kPad * 0.5f),
                IM_COL32(20, 14, 0, 255), label);
        }
    }
}

// ---------------------------------------------------------------------------
// Road event overlays — thick colored bands painted over blocked road segments.
// FLOOD = dark blue, FESTIVAL = brown. Both are fully impassable.
// City-only.
// ---------------------------------------------------------------------------
void DashboardUI::drawRoadEventOverlays(const WasteSystem& system,
                                        EnvironmentTheme activeTheme) {
    if (activeTheme != EnvironmentTheme::City) return;

    const auto activeEvents = system.getGraph().getActiveEdgeEvents();
    if (activeEvents.empty()) return;

    const MapGraph& g = system.getGraph();
    ImDrawList*    fg = ImGui::GetForegroundDrawList();

    for (const auto& ev : activeEvents) {
        if (ev.type == RoadEvent::NONE) continue;

        const int idxA = g.findNodeIndex(ev.fromId);
        const int idxB = g.findNodeIndex(ev.toId);
        if (idxA < 0 || idxB < 0) continue;

        const WasteNode& nA = g.getNode(idxA);
        const WasteNode& nB = g.getNode(idxB);

        const IsoCoord scA = RenderUtils::worldToIso(nA.getWorldX(), nA.getWorldY());
        const IsoCoord scB = RenderUtils::worldToIso(nB.getWorldX(), nB.getWorldY());

        // Road segment colour is applied in CityThemeRenderer::drawTransitNetwork.
        // Here we only draw the floating pill label so the event name is visible.
        const float dx = scB.x - scA.x;
        const float dy = scB.y - scA.y;

        // Midpoint pill label
        const float mx = scA.x + dx * 0.5f;
        const float my = scA.y + dy * 0.5f;

        const ImU32 pillBg = (ev.type == RoadEvent::FLOOD)
                             ? IM_COL32(20, 60, 180, 245)
                             : IM_COL32(120, 70, 20, 245);

        const char* label = roadEventLabel(ev.type);
        const ImVec2 tSize = ImGui::CalcTextSize(label);
        constexpr float kPad = 4.5f;
        fg->AddRectFilled(
            ImVec2(mx - tSize.x * 0.5f - kPad, my - tSize.y * 0.5f - kPad * 0.5f),
            ImVec2(mx + tSize.x * 0.5f + kPad, my + tSize.y * 0.5f + kPad * 0.5f),
            pillBg, 4.0f);
        fg->AddText(ImVec2(mx - tSize.x * 0.5f, my - tSize.y * 0.5f),
                    IM_COL32(255, 255, 255, 255), label);
    }
}
