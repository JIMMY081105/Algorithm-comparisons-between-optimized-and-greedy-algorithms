#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include "algorithms/ComparisonManager.h"
#include "core/WasteSystem.h"
#include "environment/EnvironmentTypes.h"
#include "environment/MissionPresentation.h"
#include "visualization/AnimationController.h"

// Renders the gameplay dashboard layered over the isometric scene.
// The class owns UI state such as active panel visibility, while the
// Application layer remains responsible for carrying out the requested actions.
class DashboardUI {
public:
    struct UIActions {
        bool generateNewDay = false;
        bool runSelectedAlgorithm = false;
        bool compareAll = false;
        bool replay = false;
        bool exportResults = false;
        bool exportComparison = false;
        bool changeTheme = false;
        bool changeSeason = false;
        bool randomizeWeather = false;
        bool layerTogglesChanged = false;
        int algorithmToRun = -1;
        bool playPause = false;
        EnvironmentTheme selectedTheme = EnvironmentTheme::Sea;
        CitySeason selectedSeason = CitySeason::Spring;
        SceneLayerToggles layerToggles;
    };

private:
    int selectedAlgorithm;
    EnvironmentTheme selectedTheme;
    CitySeason selectedSeason;
    bool showComparisonTable;
    bool showEventLog;
    bool showNodeDetails;

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
