#include "visualization/DashboardStyle.h"

#include <algorithm>

namespace DashboardStyle {
namespace {
constexpr float kPanelMargin = 8.0f;
constexpr float kSidebarWidth = 340.0f;
constexpr float kHeaderWidth = 420.0f;
constexpr float kLegendWidth = 290.0f;
constexpr float kHeaderHeight = 74.0f;
constexpr float kExportHeight = 112.0f;
constexpr float kLegendHeight = 176.0f;
constexpr float kEventLogWidth = 290.0f;
constexpr float kEventLogHeight = 190.0f;
constexpr float kRouteOrderWidth = 280.0f;
constexpr float kRouteOrderHeight = 230.0f;
constexpr float kComparisonHeight = 220.0f;
constexpr float kComparisonMaxWidth = 620.0f;
constexpr float kControlsHeightRatio = 0.48f;
constexpr float kMinMetricsHeight = 92.0f;
constexpr float kNodeDetailsWidth = 300.0f;
constexpr float kFuelWageWidth = 290.0f;
constexpr float kFuelWageHeight = 380.0f;
}

SidebarLayout buildSidebarLayout() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float top = viewport->WorkPos.y + kPanelMargin;
    const float leftX = viewport->WorkPos.x + kPanelMargin;
    const float headerX = viewport->WorkPos.x + viewport->WorkSize.x -
                          kHeaderWidth - kPanelMargin;
    const float rightX = viewport->WorkPos.x + viewport->WorkSize.x -
                         kSidebarWidth - kPanelMargin;

    const float fixedHeights = kHeaderHeight + kExportHeight + (kPanelMargin * 3.0f);
    const float flexibleHeight = std::max(
        0.0f, viewport->WorkSize.y - (kPanelMargin * 2.0f) - fixedHeights);

    float controlsHeight = flexibleHeight * kControlsHeightRatio;
    float metricsHeight = flexibleHeight - controlsHeight;
    if (metricsHeight < kMinMetricsHeight) {
        metricsHeight = std::min(kMinMetricsHeight, flexibleHeight);
        controlsHeight = std::max(0.0f, flexibleHeight - metricsHeight);
    }

    SidebarLayout layout{};
    layout.headerPos = ImVec2(headerX, top);
    layout.headerSize = ImVec2(kHeaderWidth, kHeaderHeight);
    layout.controlsPos = ImVec2(rightX, top + kHeaderHeight + kPanelMargin);
    layout.controlsSize = ImVec2(kSidebarWidth, controlsHeight);
    layout.exportPos = ImVec2(rightX,
                              layout.controlsPos.y + layout.controlsSize.y + kPanelMargin);
    layout.exportSize = ImVec2(kSidebarWidth, kExportHeight);
    layout.metricsPos = ImVec2(rightX,
                               layout.exportPos.y + layout.exportSize.y + kPanelMargin);
    layout.metricsSize = ImVec2(kSidebarWidth, metricsHeight);
    layout.legendPos = ImVec2(leftX, top);
    layout.legendSize = ImVec2(kLegendWidth, kLegendHeight);
    layout.fuelWagePos = ImVec2(leftX, top + kLegendHeight + kPanelMargin);
    layout.fuelWageSize = ImVec2(kFuelWageWidth, kFuelWageHeight);
    layout.nodeDetailsPos = ImVec2(
        layout.controlsPos.x - kPanelMargin - kNodeDetailsWidth,
        layout.controlsPos.y);
    layout.nodeDetailsSize = ImVec2(kNodeDetailsWidth, layout.controlsSize.y);
    return layout;
}

BottomOverlayLayout buildBottomOverlayLayout() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float sidebarLeft = viewport->WorkPos.x + viewport->WorkSize.x -
                              kSidebarWidth - kPanelMargin;
    const ImVec2 routeOrderPos(
        sidebarLeft - kPanelMargin - kRouteOrderWidth,
        viewport->WorkPos.y + viewport->WorkSize.y -
        kRouteOrderHeight - kPanelMargin);

    const float desiredComparisonLeft =
        routeOrderPos.x - kPanelMargin - kComparisonMaxWidth;
    const float comparisonLeft =
        std::max(viewport->WorkPos.x + kPanelMargin, desiredComparisonLeft);
    const float comparisonWidth =
        routeOrderPos.x - comparisonLeft - kPanelMargin;

    BottomOverlayLayout layout{};
    layout.routeOrderPos = routeOrderPos;
    layout.routeOrderSize = ImVec2(kRouteOrderWidth, kRouteOrderHeight);
    layout.comparisonPos = ImVec2(
        comparisonLeft,
        viewport->WorkPos.y + viewport->WorkSize.y -
        kComparisonHeight - kPanelMargin);
    layout.comparisonSize = ImVec2(comparisonWidth, kComparisonHeight);
    return layout;
}

ImGuiWindowFlags pinnedSidebarWindowFlags() {
    return ImGuiWindowFlags_NoCollapse |
           ImGuiWindowFlags_NoResize |
           ImGuiWindowFlags_NoMove;
}

ImGuiWindowFlags pinnedFloatingWindowFlags() {
    return ImGuiWindowFlags_NoResize |
           ImGuiWindowFlags_NoMove;
}

const char* playbackLabel(AnimationController::PlaybackState playState,
                          bool hasMission) {
    switch (playState) {
        case AnimationController::PlaybackState::PLAYING: return "Mission Live";
        case AnimationController::PlaybackState::PAUSED: return "Mission Paused";
        case AnimationController::PlaybackState::FINISHED: return "Mission Complete";
        case AnimationController::PlaybackState::IDLE:
        default:
            return hasMission ? "Mission Ready" : "Awaiting Route";
    }
}

ImVec4 playbackTint(AnimationController::PlaybackState playState,
                    bool hasMission) {
    switch (playState) {
        case AnimationController::PlaybackState::PLAYING:
            return ImVec4(0.32f, 0.92f, 0.96f, 1.0f);
        case AnimationController::PlaybackState::PAUSED:
            return ImVec4(0.90f, 0.78f, 0.28f, 1.0f);
        case AnimationController::PlaybackState::FINISHED:
            return ImVec4(0.44f, 0.88f, 0.58f, 1.0f);
        case AnimationController::PlaybackState::IDLE:
        default:
            return hasMission
                ? ImVec4(0.50f, 0.84f, 0.92f, 1.0f)
                : ImVec4(0.46f, 0.52f, 0.60f, 1.0f);
    }
}

void applyTheme(bool hasMission, bool missionRunning) {
    applyTheme(EnvironmentTheme::City, hasMission, missionRunning);
}

void applyTheme(EnvironmentTheme theme, bool hasMission, bool missionRunning) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 7.0f;
    style.GrabRounding = 7.0f;
    style.ScrollbarRounding = 10.0f;
    style.WindowPadding = ImVec2(11.0f, 9.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(7.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
    style.CellPadding = ImVec2(6.0f, 5.0f);
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.ScrollbarSize = 11.0f;

    const float missionGlow = missionRunning ? 1.0f : (hasMission ? 0.45f : 0.0f);
    ImVec4* colors = style.Colors;
    const ImVec4 accent = (theme == EnvironmentTheme::City)
        ? ImVec4(0.88f, 0.64f, 0.20f, 1.0f)
        : ImVec4(0.24f, 0.82f, 0.90f, 1.0f);

    colors[ImGuiCol_WindowBg] = (theme == EnvironmentTheme::City)
        ? ImVec4(0.08f, 0.09f, 0.11f, 0.95f)
        : ImVec4(0.06f, 0.09f, 0.14f, 0.94f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.10f, 0.15f, 0.96f);
    colors[ImGuiCol_Border] = ImVec4(0.16f + accent.x * 0.10f + missionGlow * 0.03f,
                                     0.18f + accent.y * 0.12f + missionGlow * 0.09f,
                                     0.20f + accent.z * 0.12f + missionGlow * 0.08f, 0.74f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.12f, 0.17f, 0.96f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.15f, 0.20f, 0.98f);
    colors[ImGuiCol_Header] = ImVec4(0.13f, 0.23f + missionGlow * 0.04f,
                                     0.29f + missionGlow * 0.05f, 0.76f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.33f + missionGlow * 0.04f,
                                            0.41f + missionGlow * 0.05f, 0.86f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.16f, 0.30f + missionGlow * 0.04f,
                                           0.38f + missionGlow * 0.05f, 0.92f);
    colors[ImGuiCol_Button] = ImVec4(0.11f, 0.26f + missionGlow * 0.04f,
                                     0.34f + missionGlow * 0.05f, 0.84f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.16f, 0.36f + missionGlow * 0.04f,
                                            0.45f + missionGlow * 0.05f, 0.92f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.14f, 0.31f + missionGlow * 0.05f,
                                           0.39f + missionGlow * 0.05f, 0.96f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.08f, 0.14f + missionGlow * 0.03f,
                                      0.20f + missionGlow * 0.04f, 0.92f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10f, 0.19f + missionGlow * 0.03f,
                                             0.25f + missionGlow * 0.04f, 0.94f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.11f, 0.21f + missionGlow * 0.03f,
                                            0.28f + missionGlow * 0.04f, 0.96f);
    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = ImVec4(accent.x * 0.92f, accent.y * 0.92f,
                                         accent.z * 0.92f, 0.94f);
    colors[ImGuiCol_SliderGrabActive] = accent;
    colors[ImGuiCol_Separator] = ImVec4(0.18f, 0.31f, 0.39f, 0.74f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.08f, 0.14f, 0.20f, 0.96f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.18f, 0.28f, 0.36f, 0.76f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.14f, 0.23f, 0.30f, 0.50f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.08f, 0.12f, 0.17f, 0.30f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(accent.x, accent.y, accent.z, 0.96f);
    colors[ImGuiCol_PlotHistogramHovered] = accent;
    colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x * 0.5f, accent.y * 0.5f,
                                             accent.z * 0.5f, 0.36f);
}

} // namespace DashboardStyle
