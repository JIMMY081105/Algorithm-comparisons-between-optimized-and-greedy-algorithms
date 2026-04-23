#ifndef DASHBOARD_ACTIONS_H
#define DASHBOARD_ACTIONS_H

#include "environment/EnvironmentTypes.h"

// Per-frame commands emitted by DashboardUI and handled by Application.
struct DashboardUIActions {
    bool generateNewDay = false;
    bool runSelectedAlgorithm = false;
    bool compareAll = false;
    bool replay = false;
    bool exportResults = false;
    bool exportComparison = false;
    bool roadEventsChanged = false;
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

#endif // DASHBOARD_ACTIONS_H
