#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include "core/WasteSystem.h"
#include "algorithms/ComparisonManager.h"
#include "visualization/AnimationController.h"

// Renders all ImGui panels for the dashboard interface.
// This includes control buttons, algorithm selection, metrics display,
// comparison tables, event log, and export controls.
// Separated from the map renderer — ImGui handles the UI panels while
// OpenGL handles the isometric map view.
class DashboardUI {
private:
    // Track which algorithm the user has selected
    int selectedAlgorithm;
    bool showComparisonTable;
    bool showEventLog;
    bool showNodeDetails;

    // Internal panel drawing methods
    void drawHeaderPanel(WasteSystem& system);
    void drawControlPanel(WasteSystem& system, ComparisonManager& compMgr,
                          AnimationController& animCtrl);
    void drawMetricsPanel(const RouteResult& currentResult, const WasteSystem& system);
    void drawComparisonTable(const ComparisonManager& compMgr);
    void drawNodeDetailsPanel(const WasteSystem& system);
    void drawEventLogPanel(const WasteSystem& system);
    void drawLegendPanel();
    void drawRouteOrderPanel(const RouteResult& result, const WasteSystem& system);
    void drawExportPanel(WasteSystem& system, const ComparisonManager& compMgr,
                         const RouteResult& currentResult);

public:
    DashboardUI();

    // Main render call — draws all active UI panels
    // Returns action flags that the Application layer should handle
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

    UIActions render(WasteSystem& system, ComparisonManager& compMgr,
                     AnimationController& animCtrl, const RouteResult& currentResult);

    int getSelectedAlgorithm() const;
};

#endif // DASHBOARD_UI_H
