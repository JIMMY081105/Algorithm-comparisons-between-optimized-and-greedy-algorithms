#include "environment/SeasonProfile.h"

#include <array>

const char* toDisplayString(CitySeason season) {
    switch (season) {
        case CitySeason::Spring: return "Spring";
        case CitySeason::Summer: return "Summer";
        case CitySeason::Autumn: return "Autumn";
        case CitySeason::Winter: return "Winter";
        default: return "Unknown";
    }
}

const char* toSeasonalWeatherString(CitySeason season, CityWeather weather) {
    if (season != CitySeason::Winter) {
        return toDisplayString(weather);
    }

    switch (weather) {
        case CityWeather::Sunny: return "Clear Winter";
        case CityWeather::Cloudy: return "Overcast Winter";
        case CityWeather::Rainy: return "Snowfall";
        case CityWeather::Stormy: return "Snowstorm";
        default: return "Unknown";
    }
}

CityWeather randomWeatherForSeason(CitySeason season, std::mt19937& rng) {
    std::array<double, 4> weights{};
    switch (season) {
        case CitySeason::Spring:
            weights = {0.28, 0.28, 0.27, 0.17};
            break;
        case CitySeason::Summer:
            weights = {0.48, 0.28, 0.16, 0.08};
            break;
        case CitySeason::Autumn:
            weights = {0.16, 0.32, 0.34, 0.18};
            break;
        case CitySeason::Winter:
            weights = {0.10, 0.34, 0.34, 0.22};
            break;
        default:
            weights = {0.25, 0.25, 0.25, 0.25};
            break;
    }

    std::discrete_distribution<int> distribution(weights.begin(), weights.end());
    return static_cast<CityWeather>(distribution(rng));
}

CityWeather nextDistinctWeatherForSeason(CitySeason season,
                                         CityWeather current,
                                         std::mt19937& rng) {
    constexpr int kWeatherCount = 4;
    const int currentIndex = static_cast<int>(current);
    std::uniform_int_distribution<int> offsetDistribution(1, kWeatherCount - 1);
    const int nextIndex = (currentIndex + offsetDistribution(rng)) % kWeatherCount;
    (void)season;
    return static_cast<CityWeather>(nextIndex);
}

bool isSnowWeather(CitySeason season, CityWeather weather) {
    return season == CitySeason::Winter &&
           (weather == CityWeather::Rainy || weather == CityWeather::Stormy);
}

bool isWinterStorm(CitySeason season, CityWeather weather) {
    return season == CitySeason::Winter && weather == CityWeather::Stormy;
}
