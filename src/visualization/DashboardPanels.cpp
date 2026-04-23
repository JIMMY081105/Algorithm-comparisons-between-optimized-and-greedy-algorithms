#include "DashboardUIInternal.h"

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
        if (ImGui::Button("Apply##re", ImVec2(-1.0f, 22.0f))) {
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
                              RoadEventRules::fullName(ev),
                              nodeNames[roadEventFromIdx].c_str(),
                              nodeNames[roadEventToIdx].c_str());
            }
            system.getEventLog().addEvent(logMsg);
            actions.roadEventsChanged = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Applies the road condition. Use Run Selected to calculate a route under the new conditions.");
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

                ImGui::TextColored(col, "[%s]", RoadEventRules::label(ev.type));
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
                    actions.roadEventsChanged = true;
                }
            }
            if (ImGui::SmallButton("Clear All Events##reAll")) {
                system.getGraph().clearAllEvents();
                system.getEventLog().addEvent("All road events cleared");
                actions.roadEventsChanged = true;
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

