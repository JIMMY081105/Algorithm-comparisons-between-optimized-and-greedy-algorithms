#include "visualization/DashboardUI.h"
#include "visualization/RenderUtils.h"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace {
constexpr float kPanelMargin = 8.0f;
constexpr float kSidebarWidth = 290.0f;
constexpr float kHeaderHeight = 56.0f;
constexpr float kExportHeight = 90.0f;
constexpr float kLegendHeight = 108.0f;
constexpr float kEventLogWidth = 280.0f;
constexpr float kEventLogHeight = 180.0f;
constexpr float kRouteOrderWidth = 280.0f;
constexpr float kRouteOrderHeight = 230.0f;
constexpr float kComparisonHeight = 220.0f;
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

void applyDashboardTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 7.0f;
    style.GrabRounding = 7.0f;
    style.ScrollbarRounding = 10.0f;
    style.WindowPadding = ImVec2(14.0f, 12.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(9.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(7.0f, 6.0f);
    style.CellPadding = ImVec2(8.0f, 6.0f);
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.ScrollbarSize = 13.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.09f, 0.14f, 0.94f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.10f, 0.15f, 0.96f);
    colors[ImGuiCol_Border] = ImVec4(0.18f, 0.29f, 0.37f, 0.74f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.12f, 0.17f, 0.96f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.15f, 0.20f, 0.98f);
    colors[ImGuiCol_Header] = ImVec4(0.13f, 0.23f, 0.29f, 0.76f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.33f, 0.41f, 0.86f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.16f, 0.30f, 0.38f, 0.92f);
    colors[ImGuiCol_Button] = ImVec4(0.11f, 0.26f, 0.34f, 0.84f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.16f, 0.36f, 0.45f, 0.92f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.14f, 0.31f, 0.39f, 0.96f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.08f, 0.14f, 0.20f, 0.92f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.19f, 0.25f, 0.94f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.11f, 0.21f, 0.28f, 0.96f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.37f, 0.87f, 0.92f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.82f, 0.88f, 0.94f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.48f, 0.91f, 0.96f, 1.0f);
    colors[ImGuiCol_Separator] = ImVec4(0.18f, 0.31f, 0.39f, 0.74f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.08f, 0.14f, 0.20f, 0.96f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.18f, 0.28f, 0.36f, 0.76f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.14f, 0.23f, 0.30f, 0.50f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.08f, 0.12f, 0.17f, 0.30f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.33f, 0.86f, 0.91f, 0.96f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.52f, 0.94f, 0.98f, 1.0f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.18f, 0.46f, 0.54f, 0.36f);
}
}  // namespace

DashboardUI::DashboardUI()
    : selectedAlgorithm(0), showComparisonTable(false),
      showEventLog(true), showNodeDetails(false) {}

DashboardUI::UIActions DashboardUI::render(WasteSystem& system,
                                            ComparisonManager& compMgr,
                                            AnimationController& animCtrl,
                                            const RouteResult& currentResult) {
    applyDashboardTheme();
    UIActions actions;

    drawHeaderPanel(system);

    // The control panel sets action flags directly
    // We pass 'actions' into drawControlPanel so button presses are captured
    {
        const SidebarLayout layout = buildSidebarLayout();
        ImGui::SetNextWindowPos(layout.controlsPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(layout.controlsSize, ImGuiCond_Always);
        ImGui::Begin("Operations", nullptr, kPinnedSidebarFlags);

        // ---- Day generation ----
        ImGui::SeparatorText("Simulation");
        if (ImGui::Button("Generate New Day", ImVec2(-1, 28))) {
            actions.generateNewDay = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Randomize debris levels for all sea zones");
        }

        ImGui::Spacing();
        float threshold = system.getCollectionThreshold();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##threshold", &threshold, 10.0f, 80.0f, "Threshold: %.0f%%")) {
            system.setCollectionThreshold(threshold);
        }

        // ---- Algorithm selection ----
        ImGui::SeparatorText("Algorithm");
        const char* algoNames[] = { "Regular", "Greedy", "MST", "TSP" };
        for (int i = 0; i < 4; i++) {
            if (ImGui::RadioButton(algoNames[i], &selectedAlgorithm, i)) {}
            if (i < 3) ImGui::SameLine();
        }

        ImGui::Spacing();
        if (ImGui::Button("Run Selected", ImVec2(-1, 26))) {
            actions.runSelectedAlgorithm = true;
            actions.algorithmToRun = selectedAlgorithm;
        }

        if (ImGui::Button("Compare All", ImVec2(-1, 26))) {
            actions.compareAll = true;
            showComparisonTable = true;
        }

        // ---- Playback controls ----
        ImGui::SeparatorText("Playback");
        auto playState = animCtrl.getState();

        const float halfBtn = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (playState == AnimationController::PlaybackState::IDLE ||
            playState == AnimationController::PlaybackState::FINISHED) {
            if (ImGui::Button("Play", ImVec2(halfBtn, 24))) {
                actions.playPause = true;
            }
        } else if (playState == AnimationController::PlaybackState::PLAYING) {
            if (ImGui::Button("Pause", ImVec2(halfBtn, 24))) {
                animCtrl.pause();
            }
        } else if (playState == AnimationController::PlaybackState::PAUSED) {
            if (ImGui::Button("Resume", ImVec2(halfBtn, 24))) {
                animCtrl.resume();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Replay", ImVec2(halfBtn, 24))) {
            actions.replay = true;
        }

        float speed = animCtrl.getSpeed();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##speed", &speed, 0.1f, 5.0f, "Speed: %.1fx")) {
            animCtrl.setSpeed(speed);
        }

        ImGui::TextDisabled("Playback Progress");
        float progress = animCtrl.getProgress();
        ImGui::ProgressBar(progress, ImVec2(-1, 14));

        // ---- View toggles ----
        ImGui::SeparatorText("Panels");
        ImGui::Checkbox("Comparison", &showComparisonTable);
        ImGui::SameLine();
        ImGui::Checkbox("Log", &showEventLog);
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

        if (ImGui::Button("Export Summary (TXT)", ImVec2(-1, 24))) {
            actions.exportResults = true;
        }
        ImGui::Spacing();
        if (ImGui::Button("Export Comparison (CSV)", ImVec2(-1, 24))) {
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
    ImGui::SetNextWindowBgAlpha(0.97f);

    ImGui::Begin("##Header", nullptr,
                 kPinnedSidebarFlags | ImGuiWindowFlags_NoTitleBar);

    ImGui::SetWindowFontScale(1.05f);
    ImGui::TextColored(ImVec4(0.15f, 0.72f, 0.94f, 1.0f),
                       "ECOROUTE SOLUTIONS");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextColored(ImVec4(0.64f, 0.78f, 0.84f, 1.0f),
                       "Marine Cleanup Command");
    ImGui::TextColored(ImVec4(0.45f, 0.50f, 0.58f, 1.0f),
                       "Day %d  |  Seed %u", system.getDayNumber(),
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
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGui::Begin("Metrics", nullptr, kPinnedSidebarFlags);

    if (!currentResult.isValid()) {
        ImGui::TextDisabled("Awaiting route plan");
        ImGui::TextColored(ImVec4(0.45f, 0.50f, 0.55f, 1.0f),
                           "No route computed yet.");
        ImGui::TextColored(ImVec4(0.35f, 0.40f, 0.45f, 1.0f),
                           "Generate a day and run an algorithm.");
    } else {
        ImGui::TextDisabled("Active mission");
        ImGui::SetWindowFontScale(1.03f);
        ImGui::TextColored(ImVec4(0.15f, 0.72f, 0.94f, 1.0f),
                           "%s", currentResult.algorithmName.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextColored(ImVec4(0.46f, 0.56f, 0.64f, 1.0f),
                           "%d stops scheduled  |  Threshold %.0f%%",
                           static_cast<int>(currentResult.visitOrder.size()),
                           system.getCollectionThreshold());
        ImGui::SeparatorText("Route Snapshot");

        ImGui::Columns(2, "metrics", false);
        ImGui::SetColumnWidth(0, 130);

        ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.0f), "Distance");
        ImGui::NextColumn();
        ImGui::Text("%.2f km", currentResult.totalDistance); ImGui::NextColumn();

        ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.0f), "Travel Time");
        ImGui::NextColumn();
        ImGui::Text("%.2f hrs", currentResult.travelTime); ImGui::NextColumn();

        ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.0f), "Fuel Cost");
        ImGui::NextColumn();
        ImGui::Text("RM %.2f", currentResult.fuelCost); ImGui::NextColumn();

        ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.0f), "Driver Wage");
        ImGui::NextColumn();
        ImGui::Text("RM %.2f", currentResult.wageCost); ImGui::NextColumn();

        ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.0f), "Total Cost");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.95f, 0.82f, 0.18f, 1.0f),
                           "RM %.2f", currentResult.totalCost); ImGui::NextColumn();

        ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.0f), "Collected");
        ImGui::NextColumn();
        ImGui::Text("%.1f kg", currentResult.wasteCollected); ImGui::NextColumn();

        ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.0f), "Nodes");
        ImGui::NextColumn();
        ImGui::Text("%d", static_cast<int>(currentResult.visitOrder.size())); ImGui::NextColumn();

        ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.0f), "Compute");
        ImGui::NextColumn();
        ImGui::Text("%.3f ms", currentResult.runtimeMs); ImGui::NextColumn();

        ImGui::Columns(1);
    }

    ImGui::End();
}

void DashboardUI::drawComparisonTable(const ComparisonManager& compMgr) {
    const BottomOverlayLayout layout = buildBottomOverlayLayout();
    ImGui::SetNextWindowPos(layout.comparisonPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.comparisonSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.94f);

    ImGui::Begin("Algorithm Comparison", &showComparisonTable,
                 kPinnedFloatingFlags);

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
    const BottomOverlayLayout layout = buildBottomOverlayLayout();
    ImGui::SetNextWindowPos(layout.eventLogPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.eventLogSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.93f);

    ImGui::Begin("Activity Log", &showEventLog, kPinnedFloatingFlags);
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
    const SidebarLayout layout = buildSidebarLayout();
    ImGui::SetNextWindowPos(layout.legendPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.legendSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGui::Begin("Legend", nullptr, kPinnedSidebarFlags);
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
                                       const WasteSystem& system) {
    if (!result.isValid()) return;

    const BottomOverlayLayout layout = buildBottomOverlayLayout();
    ImGui::SetNextWindowPos(layout.routeOrderPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(layout.routeOrderSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.94f);

    ImGui::Begin("Mission Route", nullptr, kPinnedFloatingFlags);

    ImGui::TextColored(ImVec4(0.15f, 0.72f, 0.94f, 1.0f),
                       "%s", result.algorithmName.c_str());
    ImGui::TextDisabled("Visit order");
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(result.visitOrder.size()); i++) {
        int nodeId = result.visitOrder[i];
        int idx = system.getGraph().findNodeIndex(nodeId);
        if (idx < 0) continue;

        const WasteNode& node = system.getGraph().getNode(idx);
        ImGui::TextColored(ImVec4(0.46f, 0.80f, 0.88f, 1.0f), "%02d", i + 1);
        ImGui::SameLine();

        if (node.getIsHQ()) {
            ImGui::TextColored(ImVec4(0.3f, 0.5f, 0.9f, 1.0f),
                               "%s", node.getName().c_str());
        } else if (node.isCollected()) {
            ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f),
                               "%s (cleaned)", node.getName().c_str());
        } else {
            ImGui::Text("%s (%.0f%%)",
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
