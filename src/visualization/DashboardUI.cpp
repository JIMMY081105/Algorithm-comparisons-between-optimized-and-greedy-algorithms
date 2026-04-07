#include "visualization/DashboardUI.h"
#include "visualization/RenderUtils.h"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace {
constexpr float kPanelMargin = 10.0f;
constexpr float kSidebarWidth = 320.0f;
constexpr float kHeaderHeight = 70.0f;
constexpr float kExportHeight = 120.0f;
constexpr float kLegendHeight = 120.0f;
constexpr float kEventLogWidth = 300.0f;
constexpr float kEventLogHeight = 200.0f;
constexpr float kRouteOrderWidth = 300.0f;
constexpr float kRouteOrderHeight = 250.0f;
constexpr float kComparisonHeight = 240.0f;
constexpr float kComparisonMaxWidth = 620.0f;
constexpr ImGuiWindowFlags kPinnedSidebarFlags =
    ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove;
constexpr ImGuiWindowFlags kPinnedFloatingFlags =
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove;

struct SidebarLayout {
    ImVec2 headerPos;
    ImVec2 headerSize;
    ImVec2 controlsPos;
    ImVec2 controlsSize;
    ImVec2 exportPos;
    ImVec2 exportSize;
    ImVec2 metricsPos;
    ImVec2 metricsSize;
    ImVec2 legendPos;
    ImVec2 legendSize;
};

struct BottomOverlayLayout {
    ImVec2 comparisonPos;
    ImVec2 comparisonSize;
    ImVec2 routeOrderPos;
    ImVec2 routeOrderSize;
    ImVec2 eventLogPos;
    ImVec2 eventLogSize;
};

SidebarLayout buildSidebarLayout() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float top = viewport->WorkPos.y + kPanelMargin;
    const float rightX = viewport->WorkPos.x + viewport->WorkSize.x -
                         kSidebarWidth - kPanelMargin;

    const float fixedHeights = kHeaderHeight + kExportHeight + kLegendHeight +
                               (kPanelMargin * 4.0f);
    const float flexibleHeight = std::max(
        0.0f, viewport->WorkSize.y - (kPanelMargin * 2.0f) - fixedHeights);

    float controlsHeight = flexibleHeight * 0.62f;
    float metricsHeight = flexibleHeight - controlsHeight;

    if (metricsHeight < 130.0f) {
        metricsHeight = std::min(130.0f, flexibleHeight);
        controlsHeight = std::max(0.0f, flexibleHeight - metricsHeight);
    }

    SidebarLayout layout{};
    layout.headerPos = ImVec2(rightX, top);
    layout.headerSize = ImVec2(kSidebarWidth, kHeaderHeight);

    layout.controlsPos = ImVec2(rightX, top + kHeaderHeight + kPanelMargin);
    layout.controlsSize = ImVec2(kSidebarWidth, controlsHeight);

    layout.exportPos = ImVec2(
        rightX,
        layout.controlsPos.y + layout.controlsSize.y + kPanelMargin);
    layout.exportSize = ImVec2(kSidebarWidth, kExportHeight);

    layout.metricsPos = ImVec2(
        rightX,
        layout.exportPos.y + layout.exportSize.y + kPanelMargin);
    layout.metricsSize = ImVec2(kSidebarWidth, metricsHeight);

    layout.legendPos = ImVec2(
        rightX,
        layout.metricsPos.y + layout.metricsSize.y + kPanelMargin);
    layout.legendSize = ImVec2(kSidebarWidth, kLegendHeight);

    return layout;
}

ImVec2 buildEventLogPos() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float sidebarLeft = viewport->WorkPos.x + viewport->WorkSize.x -
                              kSidebarWidth - kPanelMargin;
    return ImVec2(
        sidebarLeft - kPanelMargin - kEventLogWidth,
        viewport->WorkPos.y + viewport->WorkSize.y -
        kEventLogHeight - kPanelMargin);
}

BottomOverlayLayout buildBottomOverlayLayout() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 eventLogPos = buildEventLogPos();
    const ImVec2 routeOrderPos(
        eventLogPos.x,
        eventLogPos.y - kPanelMargin - kRouteOrderHeight);

    const float desiredComparisonLeft =
        routeOrderPos.x - kPanelMargin - kComparisonMaxWidth;
    const float comparisonLeft =
        std::max(viewport->WorkPos.x + kPanelMargin, desiredComparisonLeft);
    const float comparisonWidth =
        routeOrderPos.x - comparisonLeft - kPanelMargin;

    BottomOverlayLayout layout{};
    layout.eventLogPos = eventLogPos;
    layout.eventLogSize = ImVec2(kEventLogWidth, kEventLogHeight);
    layout.routeOrderPos = routeOrderPos;
    layout.routeOrderSize = ImVec2(kRouteOrderWidth, kRouteOrderHeight);
    layout.comparisonPos = ImVec2(
        comparisonLeft,
        viewport->WorkPos.y + viewport->WorkSize.y -
        kComparisonHeight - kPanelMargin);
    layout.comparisonSize = ImVec2(comparisonWidth, kComparisonHeight);

    return layout;
}
}  // namespace

DashboardUI::DashboardUI()
    : selectedAlgorithm(0), showComparisonTable(false),
      showEventLog(true), showNodeDetails(false) {}

DashboardUI::UIActions DashboardUI::render(WasteSystem& system,
                                            ComparisonManager& compMgr,
                                            AnimationController& animCtrl,
                                            const RouteResult& currentResult) {
    UIActions actions;

    drawHeaderPanel(system);

    // The control panel sets action flags directly
    // We pass 'actions' into drawControlPanel so button presses are captured
    {
        const SidebarLayout layout = buildSidebarLayout();
        ImGui::SetNextWindowPos(layout.controlsPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(layout.controlsSize, ImGuiCond_Always);
        ImGui::Begin("Controls", nullptr, kPinnedSidebarFlags);

        // ---- Day generation ----
        ImGui::SeparatorText("Simulation");
        if (ImGui::Button("Generate New Day", ImVec2(200, 30))) {
            actions.generateNewDay = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Randomize waste levels for all locations");
        }

        float threshold = system.getCollectionThreshold();
        if (ImGui::SliderFloat("Threshold %%", &threshold, 10.0f, 80.0f, "%.0f%%")) {
            system.setCollectionThreshold(threshold);
        }

        // ---- Algorithm selection ----
        ImGui::SeparatorText("Algorithm");
        const char* algoNames[] = { "Regular", "Greedy", "MST", "TSP" };
        for (int i = 0; i < 4; i++) {
            if (ImGui::RadioButton(algoNames[i], &selectedAlgorithm, i)) {}
            if (i < 3) ImGui::SameLine();
        }

        if (ImGui::Button("Run Selected Algorithm", ImVec2(200, 28))) {
            actions.runSelectedAlgorithm = true;
            actions.algorithmToRun = selectedAlgorithm;
        }

        if (ImGui::Button("Compare All Algorithms", ImVec2(200, 28))) {
            actions.compareAll = true;
            showComparisonTable = true;
        }

        // ---- Playback controls ----
        ImGui::SeparatorText("Animation");
        auto playState = animCtrl.getState();

        if (playState == AnimationController::PlaybackState::IDLE ||
            playState == AnimationController::PlaybackState::FINISHED) {
            if (ImGui::Button("Play", ImVec2(90, 25))) {
                actions.playPause = true;
            }
        } else if (playState == AnimationController::PlaybackState::PLAYING) {
            if (ImGui::Button("Pause", ImVec2(90, 25))) {
                animCtrl.pause();
            }
        } else if (playState == AnimationController::PlaybackState::PAUSED) {
            if (ImGui::Button("Resume", ImVec2(90, 25))) {
                animCtrl.resume();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Replay", ImVec2(90, 25))) {
            actions.replay = true;
        }

        // Speed slider
        float speed = animCtrl.getSpeed();
        if (ImGui::SliderFloat("Speed", &speed, 0.1f, 5.0f, "%.1fx")) {
            animCtrl.setSpeed(speed);
        }

        // Progress bar
        float progress = animCtrl.getProgress();
        ImGui::ProgressBar(progress, ImVec2(-1, 0), "Route Progress");

        // ---- View toggles ----
        ImGui::SeparatorText("Panels");
        ImGui::Checkbox("Comparison Table", &showComparisonTable);
        ImGui::SameLine();
        ImGui::Checkbox("Event Log", &showEventLog);
        ImGui::Checkbox("Node Details", &showNodeDetails);

        ImGui::End();
    }

    drawMetricsPanel(currentResult, system);
    drawLegendPanel();
    drawRouteOrderPanel(currentResult, system);

    if (showComparisonTable) {
        drawComparisonTable(compMgr);
    }
    if (showEventLog) {
        drawEventLogPanel(system);
    }
    if (showNodeDetails) {
        drawNodeDetailsPanel(system);
    }

    // Export panel with action flags
    {
        const SidebarLayout layout = buildSidebarLayout();
        ImGui::SetNextWindowPos(layout.exportPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(layout.exportSize, ImGuiCond_Always);
        ImGui::Begin("Export", nullptr, kPinnedSidebarFlags);

        if (ImGui::Button("Export Summary (TXT)", ImVec2(200, 25))) {
            actions.exportResults = true;
        }
        if (ImGui::Button("Export Comparison (CSV)", ImVec2(200, 25))) {
            actions.exportComparison = true;
        }

        ImGui::End();
    }

    return actions;
}

void DashboardUI::drawHeaderPanel(WasteSystem& system) {
    const SidebarLayout layout = buildSidebarLayout();
    ImGui::SetNextWindowPos(layout.headerPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.headerSize, ImGuiCond_Always);

    ImGui::Begin("Smart Waste Clearance System", nullptr,
                 kPinnedSidebarFlags);

    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f),
                       "EcoRoute Solutions Sdn Bhd");
    ImGui::Text("Day %d | Seed: %u", system.getDayNumber(),
                system.getCurrentSeed());

    ImGui::End();
}

void DashboardUI::drawControlPanel(WasteSystem& system,
                                    ComparisonManager& compMgr,
                                    AnimationController& animCtrl) {
    // Now handled inline in render() for proper action flag passing
}

void DashboardUI::drawMetricsPanel(const RouteResult& currentResult,
                                    const WasteSystem& system) {
    const SidebarLayout layout = buildSidebarLayout();
    ImGui::SetNextWindowPos(layout.metricsPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.metricsSize, ImGuiCond_Always);

    ImGui::Begin("Route Metrics", nullptr, kPinnedSidebarFlags);

    if (!currentResult.isValid()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                           "No route computed yet.\nGenerate a day and run an algorithm.");
    } else {
        ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f),
                           "%s", currentResult.algorithmName.c_str());
        ImGui::Separator();

        ImGui::Columns(2, "metrics", false);
        ImGui::SetColumnWidth(0, 160);

        ImGui::Text("Distance:");     ImGui::NextColumn();
        ImGui::Text("%.2f km", currentResult.totalDistance); ImGui::NextColumn();

        ImGui::Text("Travel Time:");  ImGui::NextColumn();
        ImGui::Text("%.2f hours", currentResult.travelTime); ImGui::NextColumn();

        ImGui::Text("Fuel Cost:");    ImGui::NextColumn();
        ImGui::Text("RM %.2f", currentResult.fuelCost); ImGui::NextColumn();

        ImGui::Text("Driver Wage:");  ImGui::NextColumn();
        ImGui::Text("RM %.2f", currentResult.wageCost); ImGui::NextColumn();

        ImGui::Text("Total Cost:");   ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.2f, 1.0f),
                           "RM %.2f", currentResult.totalCost); ImGui::NextColumn();

        ImGui::Text("Waste Collected:"); ImGui::NextColumn();
        ImGui::Text("%.1f kg", currentResult.wasteCollected); ImGui::NextColumn();

        ImGui::Text("Nodes Visited:"); ImGui::NextColumn();
        ImGui::Text("%d", static_cast<int>(currentResult.visitOrder.size())); ImGui::NextColumn();

        ImGui::Text("Compute Time:"); ImGui::NextColumn();
        ImGui::Text("%.3f ms", currentResult.runtimeMs); ImGui::NextColumn();

        ImGui::Columns(1);
    }

    ImGui::End();
}

void DashboardUI::drawComparisonTable(const ComparisonManager& compMgr) {
    const BottomOverlayLayout layout = buildBottomOverlayLayout();
    ImGui::SetNextWindowPos(layout.comparisonPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.comparisonSize, ImGuiCond_Always);

    ImGui::Begin("Algorithm Comparison", &showComparisonTable,
                 kPinnedFloatingFlags);

    const auto& results = compMgr.getResults();
    if (results.empty()) {
        ImGui::Text("Run 'Compare All Algorithms' to see results here.");
    } else {
        int bestIdx = compMgr.getBestAlgorithmIndex();

        if (ImGui::BeginTable("CompTable", 8,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable)) {

            ImGui::TableSetupColumn("Algorithm");
            ImGui::TableSetupColumn("Distance (km)");
            ImGui::TableSetupColumn("Time (h)");
            ImGui::TableSetupColumn("Fuel (RM)");
            ImGui::TableSetupColumn("Wage (RM)");
            ImGui::TableSetupColumn("Total (RM)");
            ImGui::TableSetupColumn("Waste (kg)");
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
        ImGui::TableSetupColumn("Waste %", ImGuiTableColumnFlags_WidthFixed, 60);
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
                ImGui::TextColored(ImVec4(0.3f, 0.5f, 0.9f, 1.0f), "HQ");
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
                ImGui::TextColored(ImVec4(0.3f, 0.5f, 0.9f, 1.0f), "Depot");
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
    const BottomOverlayLayout layout = buildBottomOverlayLayout();
    ImGui::SetNextWindowPos(layout.eventLogPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.eventLogSize, ImGuiCond_Always);

    ImGui::Begin("Event Log", &showEventLog, kPinnedFloatingFlags);

    auto events = system.getEventLog().getRecentEvents(15);

    for (const auto* entry : events) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                           "[%s]", entry->timestamp.c_str());
        ImGui::SameLine();
        ImGui::TextWrapped("%s", entry->message.c_str());
    }

    ImGui::End();
}

void DashboardUI::drawLegendPanel() {
    const SidebarLayout layout = buildSidebarLayout();
    ImGui::SetNextWindowPos(layout.legendPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.legendSize, ImGuiCond_Always);

    ImGui::Begin("Legend", nullptr, kPinnedSidebarFlags);

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
    ImGui::SameLine(); ImGui::Text("HQ / Depot");

    ImGui::ColorButton("##done", ImVec4(collected.r, collected.g, collected.b, 1.0f));
    ImGui::SameLine(); ImGui::Text("Collected");

    ImGui::End();
}

void DashboardUI::drawRouteOrderPanel(const RouteResult& result,
                                       const WasteSystem& system) {
    if (!result.isValid()) return;

    const BottomOverlayLayout layout = buildBottomOverlayLayout();
    ImGui::SetNextWindowPos(layout.routeOrderPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.routeOrderSize, ImGuiCond_Always);

    ImGui::Begin("Route Order", nullptr, kPinnedFloatingFlags);

    ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f),
                       "%s", result.algorithmName.c_str());
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(result.visitOrder.size()); i++) {
        int nodeId = result.visitOrder[i];
        int idx = system.getGraph().findNodeIndex(nodeId);
        if (idx < 0) continue;

        const WasteNode& node = system.getGraph().getNode(idx);

        if (node.getIsHQ()) {
            ImGui::TextColored(ImVec4(0.3f, 0.5f, 0.9f, 1.0f),
                               "%d. %s", i + 1, node.getName().c_str());
        } else if (node.isCollected()) {
            ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f),
                               "%d. %s (collected)", i + 1, node.getName().c_str());
        } else {
            ImGui::Text("%d. %s (%.0f%%)", i + 1,
                        node.getName().c_str(), node.getWasteLevel());
        }
    }

    ImGui::End();
}

void DashboardUI::drawExportPanel(WasteSystem& system,
                                   const ComparisonManager& compMgr,
                                   const RouteResult& currentResult) {
    // Now handled inline in render() for proper action flag passing
}

int DashboardUI::getSelectedAlgorithm() const {
    return selectedAlgorithm;
}
