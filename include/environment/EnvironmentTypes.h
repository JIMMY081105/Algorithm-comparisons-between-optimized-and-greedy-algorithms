#ifndef ENVIRONMENT_TYPES_H
#define ENVIRONMENT_TYPES_H

#include <string>

enum class EnvironmentTheme {
    Sea = 0,
    City = 1
};

enum class CityWeather {
    Sunny = 0,
    Cloudy = 1,
    Rainy = 2,
    Stormy = 3
};

enum class CitySeason {
    Spring = 0,
    Summer = 1,
    Autumn = 2,
    Winter = 3
};

struct SceneLayerToggles {
    bool showTraffic = true;
    bool showCongestion = true;
    bool showIncidents = true;
    bool showRouteGraph = false;
    bool showTrafficLights = true;
};

struct ThemeDashboardInfo {
    EnvironmentTheme theme = EnvironmentTheme::Sea;
    std::string themeLabel = "Sea";
    std::string subtitle = "Harbour operations";
    CitySeason season = CitySeason::Spring;
    std::string seasonLabel = "N/A";
    std::string weatherLabel = "N/A";
    std::string atmosphereLabel = "Steady marine conditions";
    float congestionLevel = 0.0f;
    int incidentCount = 0;
    bool supportsWeather = false;
    bool supportsSeasons = false;
};

inline const char* toDisplayString(EnvironmentTheme theme) {
    switch (theme) {
        case EnvironmentTheme::City: return "City";
        case EnvironmentTheme::Sea:
        default: return "Sea";
    }
}

inline const char* toDisplayString(CityWeather weather) {
    switch (weather) {
        case CityWeather::Sunny: return "Sunny";
        case CityWeather::Cloudy: return "Cloudy";
        case CityWeather::Rainy: return "Rainy";
        case CityWeather::Stormy: return "Stormy";
        default: return "Unknown";
    }
}

#endif // ENVIRONMENT_TYPES_H
