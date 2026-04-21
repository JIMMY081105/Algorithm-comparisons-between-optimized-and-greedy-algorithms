#ifndef DASHBOARD_STYLE_H
#define DASHBOARD_STYLE_H

#include "environment/EnvironmentTypes.h"
#include "visualization/AnimationController.h"
#include <imgui.h>

namespace DashboardStyle {

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
    ImVec2 nodeDetailsPos;
    ImVec2 nodeDetailsSize;
    // Fuel & Wage panel — placed left of the right sidebar in the map area
    ImVec2 fuelWagePos;
    ImVec2 fuelWageSize;
};

struct BottomOverlayLayout {
    ImVec2 comparisonPos;
    ImVec2 comparisonSize;
    ImVec2 routeOrderPos;
    ImVec2 routeOrderSize;
};

SidebarLayout buildSidebarLayout();
BottomOverlayLayout buildBottomOverlayLayout();

ImGuiWindowFlags pinnedSidebarWindowFlags();
ImGuiWindowFlags pinnedFloatingWindowFlags();

const char* playbackLabel(AnimationController::PlaybackState playState,
                          bool hasMission);
ImVec4 playbackTint(AnimationController::PlaybackState playState,
                    bool hasMission);

void applyTheme(bool hasMission, bool missionRunning);
void applyTheme(EnvironmentTheme theme, bool hasMission, bool missionRunning);

} // namespace DashboardStyle

#endif // DASHBOARD_STYLE_H
