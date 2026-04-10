#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include "algorithms/ComparisonManager.h"
#include "core/WasteSystem.h"
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
        int algorithmToRun = -1;
        bool playPause = false;
    };

private:
    int selectedAlgorithm;
    bool showComparisonTable;
    bool showEventLog;
    bool showNodeDetails;

    void drawHeaderPanel(const WasteSystem& system,
                         AnimationController::PlaybackState playState,
                         bool hasMission);
    void drawControlPanel(WasteSystem& system, AnimationController& animCtrl,
                          UIActions& actions);
    void drawMetricsPanel(const RouteResult& currentResult,
                          const WasteSystem& system,
                          AnimationController::PlaybackState playState,
                          float progress);
    void drawComparisonTable(const ComparisonManager& compMgr);
    void drawNodeDetailsPanel(const WasteSystem& system);
    void drawEventLogPanel(const WasteSystem& system);
    void drawLegendPanel();
    void drawRouteOrderPanel(const RouteResult& result,
                             const WasteSystem& system,
                             AnimationController::PlaybackState playState,
                             float progress);
    void drawExportPanel(UIActions& actions);

public:
    DashboardUI();

    UIActions render(WasteSystem& system, ComparisonManager& compMgr,
                     AnimationController& animCtrl,
                     const RouteResult& currentResult);

    int getSelectedAlgorithm() const;
};

#endif // DASHBOARD_UI_H
