#ifndef DASHBOARD_UI_INTERNAL_H
#define DASHBOARD_UI_INTERNAL_H

#include "visualization/DashboardUI.h"

#include "visualization/DashboardStyle.h"
#include "visualization/RenderUtils.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace DashboardUIInternal {

constexpr ImVec4 kSeaPrimary(0.15f, 0.72f, 0.94f, 1.0f);
constexpr ImVec4 kCityPrimary(0.94f, 0.72f, 0.22f, 1.0f);
constexpr ImVec4 kTextSoft(0.45f, 0.50f, 0.58f, 1.0f);
constexpr ImVec4 kTextMuted(0.46f, 0.56f, 0.64f, 1.0f);
constexpr ImVec4 kTextSubtle(0.55f, 0.60f, 0.68f, 1.0f);
constexpr ImVec4 kTextNeutral(0.76f, 0.80f, 0.84f, 1.0f);

inline ImVec4 brandColor(EnvironmentTheme theme) {
    return theme == EnvironmentTheme::City ? kCityPrimary : kSeaPrimary;
}

inline const char* themeTooltip(EnvironmentTheme theme) {
    return theme == EnvironmentTheme::City
        ? "Street-constrained routing with weather, congestion, and incidents."
        : "Open-water routing with harbour visuals and direct marine travel costs.";
}

} // namespace DashboardUIInternal

using DashboardUIInternal::brandColor;
using DashboardUIInternal::kCityPrimary;
using DashboardUIInternal::kSeaPrimary;
using DashboardUIInternal::kTextMuted;
using DashboardUIInternal::kTextNeutral;
using DashboardUIInternal::kTextSoft;
using DashboardUIInternal::kTextSubtle;
using DashboardUIInternal::themeTooltip;

#endif // DASHBOARD_UI_INTERNAL_H
