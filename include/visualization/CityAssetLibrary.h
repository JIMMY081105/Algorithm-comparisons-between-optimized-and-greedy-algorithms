// ============================================================================
// CityAssetLibrary.h
// Hard-coded prefab catalog for KL-inspired city scene.
// Contains struct definitions and accessors for all manually-authored presets.
// ============================================================================

#ifndef CITY_ASSET_LIBRARY_H
#define CITY_ASSET_LIBRARY_H

#include "visualization/RenderUtils.h"

#include <vector>

namespace CityAssets {

// ============================================================================
//  ENUMERATIONS
// ============================================================================

enum class BuildingCategory {
    Landmark,
    OfficeTower,
    MidRiseOffice,
    Civic,
    LowRise
};

enum class EnvironmentCategory {
    Park,
    Plaza,
    TreeCluster,
    RoadsideTrees
};

enum class VehicleCategory {
    Compact,
    Sedan,
    Van
};

enum class RoadPropCategory {
    TrafficLight,
    Streetlight,
    Divider,
    Crosswalk
};

enum class RoofStyle {
    Flat,
    Stepped,
    Crown,
    Spire
};

enum class ZoneType {
    Core,
    Financial,
    Mixed,
    Residential,
    Campus,
    Park,
    Service
};

// ============================================================================
//  PRESET STRUCTS
// ============================================================================

struct ColorBias {
    float dr, dg, db;
};

struct BuildingPreset {
    const char* name;
    BuildingCategory category;
    float width;             // world fraction of tile
    float depth;
    float height;            // base height in scene units
    Color material;          // base facade color
    Color accent;            // highlight color
    ColorBias leftBias;      // cool-shift for shadow face
    ColorBias rightBias;     // warm-shift for lit face
    RoofStyle roof;
    float podiumRatio;       // how much of height is podium
    float taper;             // tower width relative to base
    float windowDensity;     // window band frequency
    float windowWarmth;      // warm vs cool glass
    bool isTwinTower;        // rendered as twin-tower form
};

struct EnvironmentPreset {
    const char* name;
    EnvironmentCategory category;
    Color primary;
    Color secondary;
    int featureCount;        // sub-feature density (trees, etc)
    bool hasCentralFeature;  // pond, fountain, etc
    bool hasBorder;          // outline / edge treatment
};

struct VehiclePreset {
    const char* name;
    VehicleCategory category;
    float length;
    float width;
    Color body;
    Color outline;
    float speed;             // 0 = parked
};

struct RoadPropPreset {
    const char* name;
    RoadPropCategory category;
    float width;
    float height;
    Color poleColor;
    Color lightColor;        // for lights/signals
    Color signalRed;         // for traffic lights
};

// ============================================================================
//  PLACEMENT STRUCTS — used by tile assignment
// ============================================================================

struct BuildingPlacement {
    const BuildingPreset& preset;
    float offsetX;           // fraction of tile width from center
    float offsetY;
    float heightScale;       // multiplier on preset height
};

struct EnvironmentPlacement {
    const EnvironmentPreset& preset;
    float offsetX;
    float offsetY;
};

struct VehiclePlacement {
    const VehiclePreset& preset;
    float offsetX;
    float offsetY;
};

struct RoadPropPlacement {
    const RoadPropPreset& preset;
    float offsetX;
    float offsetY;
};

struct TileAssignment {
    ZoneType zone;
    std::vector<BuildingPlacement> buildings;
    std::vector<EnvironmentPlacement> environment;
    std::vector<VehiclePlacement> vehicles;
    std::vector<RoadPropPlacement> roadProps;
};

// ============================================================================
//  CATALOG ACCESS
// ============================================================================

int getLandmarkCount();
const BuildingPreset& getLandmark(int index);

int getOfficeTowerCount();
const BuildingPreset& getOfficeTower(int index);

int getMidRiseCount();
const BuildingPreset& getMidRise(int index);

int getCivicCount();
const BuildingPreset& getCivic(int index);

int getLowRiseCount();
const BuildingPreset& getLowRise(int index);

int getEnvironmentCount();
const EnvironmentPreset& getEnvironment(int index);

int getVehicleCount();
const VehiclePreset& getVehicle(int index);

int getRoadPropCount();
const RoadPropPreset& getRoadProp(int index);

// ============================================================================
//  TILE POPULATION
// ============================================================================

TileAssignment assignTile(ZoneType zone, int tileIndex, unsigned int seed);
ZoneType classifyZone(int districtOrdinal);

} // namespace CityAssets

#endif // CITY_ASSET_LIBRARY_H
