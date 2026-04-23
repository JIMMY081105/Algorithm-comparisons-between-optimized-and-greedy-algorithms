#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include "algorithms/ComparisonManager.h"
#include "core/WasteSystem.h"
#include "environment/MissionPresentation.h"
#include "visualization/AnimationController.h"
#include "visualization/DashboardActions.h"

// Renders the gameplay dashboard layered over the isometric scene.
// The class owns UI state such as active panel visibility, while the
// Application layer remains responsible for carrying out the requested actions.
class DashboardUI {
public:
    using UIActions = DashboardUIActions;

private:
    int selectedAlgorithm;
    EnvironmentTheme selectedTheme;
    CitySeason selectedSeason;
    bool showComparisonTable;
    bool showEventLog;
    bool showNodeDetails;

    // Road event panel state
    int roadEventFromIdx;
    int roadEventToIdx;
    int roadEventTypeIdx;

    void drawHeaderPanel(const WasteSystem& system,
                         const ThemeDashboardInfo& environmentInfo,
                         AnimationController::PlaybackState playState,
                         bool hasMission);
    void drawControlPanel(WasteSystem& system, AnimationController& animCtrl,
                          const ThemeDashboardInfo& environmentInfo,
                          UIActions& actions);
    void drawMetricsPanel(const RouteResult& currentResult,
                          const MissionPresentation& currentMission,
                          const WasteSystem& system,
                          const ThemeDashboardInfo& environmentInfo,
                          AnimationController::PlaybackState playState,
                          float progress);
    void drawComparisonTable(const ComparisonManager& compMgr,
                            const ThemeDashboardInfo& environmentInfo);
    void drawNodeDetailsPanel(const WasteSystem& system);
    void drawLegendPanel(const ThemeDashboardInfo& environmentInfo,
                         const WasteSystem& system);
    void drawTrafficLegend();
    void drawRouteOrderPanel(const RouteResult& result,
                             const MissionPresentation& currentMission,
                             const WasteSystem& system,
                             AnimationController::PlaybackState playState,
                             float progress);
    void drawExportPanel(UIActions& actions);
    void drawFuelWagePanel(const WasteSystem& system,
                           const RouteResult& currentResult,
                           const ThemeDashboardInfo& environmentInfo);
    void drawTollOverlays(const WasteSystem& system,
                          const RouteResult& currentResult,
                          EnvironmentTheme activeTheme);

public:
    DashboardUI();

    UIActions render(WasteSystem& system, ComparisonManager& compMgr,
                     AnimationController& animCtrl,
                     const RouteResult& currentResult,
                     const MissionPresentation& currentMission,
                     const ThemeDashboardInfo& environmentInfo,
                     const SceneLayerToggles& layerToggles,
                     EnvironmentTheme activeTheme);

    int getSelectedAlgorithm() const;
};

#endif // DASHBOARD_UI_H
