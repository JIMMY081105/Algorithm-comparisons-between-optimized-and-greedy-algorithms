#ifndef SEASON_PROFILE_H
#define SEASON_PROFILE_H

#include "environment/EnvironmentTypes.h"

#include <random>

const char* toDisplayString(CitySeason season);
const char* toSeasonalWeatherString(CitySeason season, CityWeather weather);

CityWeather randomWeatherForSeason(CitySeason season, std::mt19937& rng);

bool isSnowWeather(CitySeason season, CityWeather weather);
bool isWinterStorm(CitySeason season, CityWeather weather);

#endif // SEASON_PROFILE_H
