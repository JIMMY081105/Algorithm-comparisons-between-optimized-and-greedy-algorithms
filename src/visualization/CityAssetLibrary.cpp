// ============================================================================
// CityAssetLibrary.cpp
// Hard-coded prefab library for KL-inspired city scene population.
// Every preset is manually authored with specific proportions, roof shapes,
// facade color setups, and silhouette compositions.
// ============================================================================

#include "visualization/CityAssetLibrary.h"

#include <algorithm>
#include <cmath>

namespace CityAssets {

// ============================================================================
//  COLOR HELPERS
// ============================================================================

static Color mix(const Color& a, const Color& b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return {a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t};
}

static Color scale(const Color& c, float s) {
    return {std::min(1.0f, c.r * s),
            std::min(1.0f, c.g * s),
            std::min(1.0f, c.b * s),
            c.a};
}

static Color bias(const Color& c, float dr, float dg, float db) {
    return {std::max(0.0f, std::min(1.0f, c.r + dr)),
            std::max(0.0f, std::min(1.0f, c.g + dg)),
            std::max(0.0f, std::min(1.0f, c.b + db)),
            c.a};
}

// ============================================================================
//  1. LANDMARK BUILDING PRESETS
// ============================================================================

static const BuildingPreset kLandmarkPresets[] = {

    // [0] KL Twin Tower A — tall steel-glass pinnacle
    {"KL Tower A",
     BuildingCategory::Landmark,
     0.34f, 0.28f,       // width, depth (world-fraction of block)
     72.0f,              // height
     {0.60f, 0.66f, 0.74f, 0.98f},  // material: steel grey-blue
     {0.92f, 0.96f, 1.0f, 0.98f},   // accent: bright white-blue
     {-0.05f, -0.02f, 0.06f},       // leftBias: cool blue shadow
     {0.05f, 0.03f, -0.04f},        // rightBias: warm sun
     RoofStyle::Spire,
     0.22f,  // podiumRatio
     0.48f,  // taper
     0.78f,  // windowDensity
     0.30f,  // windowWarmth — cool glass
     true},  // isTwinTower

    // [1] KL Twin Tower B — mirror of A, slightly shorter
    {"KL Tower B",
     BuildingCategory::Landmark,
     0.34f, 0.28f,
     70.0f,
     {0.60f, 0.66f, 0.74f, 0.98f},
     {0.92f, 0.96f, 1.0f, 0.98f},
     {-0.05f, -0.02f, 0.06f},
     {0.05f, 0.03f, -0.04f},
     RoofStyle::Spire,
     0.22f, 0.48f, 0.78f, 0.30f,
     true},

    // [2] Menara KL-style observation tower — tapered needle
    {"Observation Tower",
     BuildingCategory::Landmark,
     0.18f, 0.16f,
     58.0f,
     {0.54f, 0.58f, 0.64f, 0.98f},
     {0.86f, 0.90f, 0.96f, 0.98f},
     {-0.03f, -0.01f, 0.04f},
     {0.03f, 0.02f, -0.02f},
     RoofStyle::Spire,
     0.30f, 0.38f, 0.64f, 0.26f,
     false},

    // [3] Convention center — wide, low, dramatic crown
    {"Convention Center",
     BuildingCategory::Landmark,
     0.52f, 0.36f,
     18.0f,
     {0.44f, 0.46f, 0.50f, 0.98f},
     {0.78f, 0.82f, 0.88f, 0.98f},
     {-0.02f, -0.01f, 0.03f},
     {0.02f, 0.01f, -0.01f},
     RoofStyle::Crown,
     0.40f, 0.82f, 0.52f, 0.36f,
     false},
};

// ============================================================================
//  2. OFFICE / COMMERCIAL TOWER PRESETS
// ============================================================================

static const BuildingPreset kOfficeTowerPresets[] = {

    // [0] Glass curtain-wall skyscraper — tall, blue-steel
    {"Glass Tower",
     BuildingCategory::OfficeTower,
     0.22f, 0.18f,
     38.0f,
     {0.28f, 0.38f, 0.54f, 0.98f},
     {0.54f, 0.70f, 0.88f, 0.98f},
     {-0.04f, -0.02f, 0.06f},
     {0.04f, 0.02f, -0.03f},
     RoofStyle::Spire,
     0.18f, 0.52f, 0.72f, 0.24f,
     false},

    // [1] Rectangular bank tower — granite grey, stepped crown
    {"Bank Tower",
     BuildingCategory::OfficeTower,
     0.26f, 0.20f,
     34.0f,
     {0.38f, 0.38f, 0.40f, 0.98f},
     {0.68f, 0.70f, 0.74f, 0.98f},
     {-0.03f, -0.01f, 0.04f},
     {0.03f, 0.01f, -0.02f},
     RoofStyle::Stepped,
     0.20f, 0.58f, 0.60f, 0.32f,
     false},

    // [2] Corporate HQ — dark tinted glass, medium-tall
    {"Corporate HQ",
     BuildingCategory::OfficeTower,
     0.24f, 0.22f,
     30.0f,
     {0.22f, 0.26f, 0.34f, 0.98f},
     {0.48f, 0.54f, 0.66f, 0.98f},
     {-0.04f, -0.02f, 0.05f},
     {0.03f, 0.02f, -0.03f},
     RoofStyle::Crown,
     0.24f, 0.56f, 0.68f, 0.28f,
     false},

    // [3] Slim finance tower — narrow, very tall, steel frame
    {"Finance Needle",
     BuildingCategory::OfficeTower,
     0.16f, 0.14f,
     42.0f,
     {0.34f, 0.42f, 0.52f, 0.98f},
     {0.62f, 0.72f, 0.84f, 0.98f},
     {-0.03f, -0.01f, 0.04f},
     {0.04f, 0.02f, -0.03f},
     RoofStyle::Spire,
     0.16f, 0.44f, 0.74f, 0.22f,
     false},

    // [4] Insurance building — wide, solid, mid-height
    {"Insurance Block",
     BuildingCategory::OfficeTower,
     0.30f, 0.24f,
     24.0f,
     {0.40f, 0.40f, 0.42f, 0.98f},
     {0.72f, 0.74f, 0.78f, 0.98f},
     {-0.02f, -0.01f, 0.03f},
     {0.02f, 0.01f, -0.02f},
     RoofStyle::Flat,
     0.26f, 0.62f, 0.56f, 0.34f,
     false},

    // [5] Podium+tower combo — broad base, slender tower
    {"Podium Tower",
     BuildingCategory::OfficeTower,
     0.28f, 0.22f,
     32.0f,
     {0.32f, 0.36f, 0.44f, 0.98f},
     {0.58f, 0.64f, 0.76f, 0.98f},
     {-0.03f, -0.01f, 0.04f},
     {0.03f, 0.01f, -0.02f},
     RoofStyle::Stepped,
     0.32f, 0.50f, 0.66f, 0.28f,
     false},

    // [6] Green-glass eco tower
    {"Eco Tower",
     BuildingCategory::OfficeTower,
     0.20f, 0.18f,
     28.0f,
     {0.26f, 0.38f, 0.34f, 0.98f},
     {0.52f, 0.72f, 0.64f, 0.98f},
     {-0.02f, 0.02f, 0.04f},
     {0.02f, 0.01f, -0.03f},
     RoofStyle::Crown,
     0.20f, 0.54f, 0.62f, 0.30f,
     false},

    // [7] Art-deco inspired heritage tower
    {"Heritage Tower",
     BuildingCategory::OfficeTower,
     0.22f, 0.20f,
     26.0f,
     {0.46f, 0.42f, 0.36f, 0.98f},
     {0.76f, 0.70f, 0.60f, 0.98f},
     {-0.02f, -0.02f, 0.03f},
     {0.04f, 0.02f, -0.02f},
     RoofStyle::Crown,
     0.28f, 0.60f, 0.54f, 0.38f,
     false},
};

// ============================================================================
//  3. MID-RISE OFFICE BLOCKS
// ============================================================================

static const BuildingPreset kMidRisePresets[] = {

    // [0] Standard office slab — wide, modest height
    {"Office Slab",
     BuildingCategory::MidRiseOffice,
     0.36f, 0.20f,
     14.0f,
     {0.36f, 0.38f, 0.42f, 0.98f},
     {0.64f, 0.68f, 0.74f, 0.98f},
     {-0.02f, -0.01f, 0.03f},
     {0.02f, 0.01f, -0.02f},
     RoofStyle::Flat,
     0.22f, 0.72f, 0.50f, 0.34f,
     false},

    // [1] Boutique office — compact, warm tones
    {"Boutique Office",
     BuildingCategory::MidRiseOffice,
     0.20f, 0.18f,
     16.0f,
     {0.42f, 0.38f, 0.34f, 0.98f},
     {0.72f, 0.66f, 0.58f, 0.98f},
     {-0.02f, -0.01f, 0.02f},
     {0.03f, 0.02f, -0.02f},
     RoofStyle::Stepped,
     0.20f, 0.60f, 0.48f, 0.40f,
     false},

    // [2] Medical/tech building — clean white, mid-height
    {"Tech Center",
     BuildingCategory::MidRiseOffice,
     0.28f, 0.22f,
     18.0f,
     {0.48f, 0.50f, 0.54f, 0.98f},
     {0.82f, 0.84f, 0.88f, 0.98f},
     {-0.01f, -0.01f, 0.02f},
     {0.02f, 0.01f, -0.01f},
     RoofStyle::Flat,
     0.24f, 0.68f, 0.58f, 0.30f,
     false},
};

// ============================================================================
//  4. PUBLIC / CIVIC / EDUCATION BUILDINGS
// ============================================================================

static const BuildingPreset kCivicPresets[] = {

    // [0] School block — wide, low, institutional
    {"School Block",
     BuildingCategory::Civic,
     0.42f, 0.26f,
     8.0f,
     {0.48f, 0.46f, 0.40f, 0.98f},
     {0.78f, 0.76f, 0.68f, 0.98f},
     {-0.01f, -0.01f, 0.02f},
     {0.02f, 0.01f, -0.01f},
     RoofStyle::Flat,
     0.14f, 0.82f, 0.36f, 0.42f,
     false},

    // [1] Campus building — L-shaped feel, courtyard implied
    {"Campus Hall",
     BuildingCategory::Civic,
     0.38f, 0.30f,
     12.0f,
     {0.44f, 0.44f, 0.42f, 0.98f},
     {0.74f, 0.76f, 0.72f, 0.98f},
     {-0.02f, -0.01f, 0.02f},
     {0.02f, 0.01f, -0.01f},
     RoofStyle::Stepped,
     0.18f, 0.76f, 0.42f, 0.36f,
     false},

    // [2] Small civic / community center
    {"Community Center",
     BuildingCategory::Civic,
     0.26f, 0.22f,
     7.0f,
     {0.50f, 0.50f, 0.46f, 0.98f},
     {0.80f, 0.82f, 0.76f, 0.98f},
     {-0.01f, 0.00f, 0.02f},
     {0.01f, 0.01f, -0.01f},
     RoofStyle::Crown,
     0.16f, 0.84f, 0.32f, 0.44f,
     false},

    // [3] Utility / service depot — boxy, functional
    {"Service Depot",
     BuildingCategory::Civic,
     0.32f, 0.24f,
     6.0f,
     {0.40f, 0.36f, 0.30f, 0.98f},
     {0.60f, 0.56f, 0.48f, 0.98f},
     {-0.01f, -0.01f, 0.01f},
     {0.01f, 0.01f, -0.01f},
     RoofStyle::Flat,
     0.12f, 0.86f, 0.28f, 0.20f,
     false},

    // [4] Library / museum — elegant, stepped silhouette
    {"Library",
     BuildingCategory::Civic,
     0.30f, 0.24f,
     10.0f,
     {0.46f, 0.48f, 0.50f, 0.98f},
     {0.78f, 0.80f, 0.84f, 0.98f},
     {-0.02f, -0.01f, 0.03f},
     {0.02f, 0.01f, -0.02f},
     RoofStyle::Crown,
     0.20f, 0.78f, 0.44f, 0.36f,
     false},
};

// ============================================================================
//  5. LOW-RISE CITY FILLERS
// ============================================================================

static const BuildingPreset kLowRisePresets[] = {

    // [0] Terrace / shoplot row — narrow, repeating
    {"Shoplot Row",
     BuildingCategory::LowRise,
     0.40f, 0.16f,
     5.5f,
     {0.52f, 0.44f, 0.38f, 0.98f},
     {0.78f, 0.68f, 0.58f, 0.98f},
     {-0.01f, -0.01f, 0.02f},
     {0.02f, 0.01f, -0.01f},
     RoofStyle::Flat,
     0.10f, 0.86f, 0.30f, 0.52f,
     false},

    // [1] Residential apartment block — warm tones
    {"Apartment Block",
     BuildingCategory::LowRise,
     0.28f, 0.22f,
     9.0f,
     {0.54f, 0.44f, 0.38f, 0.98f},
     {0.82f, 0.68f, 0.56f, 0.98f},
     {-0.02f, -0.01f, 0.02f},
     {0.03f, 0.02f, -0.02f},
     RoofStyle::Stepped,
     0.16f, 0.70f, 0.40f, 0.56f,
     false},

    // [2] Warehouse / industrial — boxy grey
    {"Warehouse",
     BuildingCategory::LowRise,
     0.36f, 0.28f,
     5.0f,
     {0.38f, 0.36f, 0.32f, 0.98f},
     {0.56f, 0.54f, 0.48f, 0.98f},
     {-0.01f, -0.01f, 0.01f},
     {0.01f, 0.01f, -0.01f},
     RoofStyle::Flat,
     0.08f, 0.88f, 0.24f, 0.18f,
     false},

    // [3] Mixed-use small block — shop below, apartment above
    {"Mixed-Use Block",
     BuildingCategory::LowRise,
     0.24f, 0.20f,
     7.5f,
     {0.48f, 0.42f, 0.36f, 0.98f},
     {0.76f, 0.68f, 0.58f, 0.98f},
     {-0.01f, -0.01f, 0.02f},
     {0.02f, 0.01f, -0.01f},
     RoofStyle::Stepped,
     0.18f, 0.74f, 0.38f, 0.48f,
     false},

    // [4] Row house terrace — narrow, colourful
    {"Row House",
     BuildingCategory::LowRise,
     0.18f, 0.14f,
     6.0f,
     {0.56f, 0.48f, 0.42f, 0.98f},
     {0.84f, 0.74f, 0.62f, 0.98f},
     {-0.02f, 0.00f, 0.02f},
     {0.03f, 0.01f, -0.02f},
     RoofStyle::Crown,
     0.12f, 0.76f, 0.34f, 0.58f,
     false},

    // [5] Corner commercial — slightly taller mixed use
    {"Corner Shop",
     BuildingCategory::LowRise,
     0.22f, 0.20f,
     8.0f,
     {0.44f, 0.40f, 0.36f, 0.98f},
     {0.72f, 0.66f, 0.58f, 0.98f},
     {-0.01f, -0.01f, 0.02f},
     {0.02f, 0.01f, -0.01f},
     RoofStyle::Flat,
     0.14f, 0.78f, 0.36f, 0.46f,
     false},
};

// ============================================================================
//  6. GREEN / PUBLIC ENVIRONMENT PRESETS
// ============================================================================

static const EnvironmentPreset kEnvironmentPresets[] = {

    // [0] Park tile — open grass, a few tree clusters
    {"City Park",
     EnvironmentCategory::Park,
     {0.18f, 0.34f, 0.16f, 0.84f},   // primary: rich grass green
     {0.22f, 0.42f, 0.18f, 0.40f},   // secondary: lighter green accent
     3,                                // featureCount: 3 tree clusters
     true,                             // hasCentralFeature (pond/fountain)
     false},                           // hasBorder

    // [1] Plaza tile — paved public square
    {"Public Plaza",
     EnvironmentCategory::Plaza,
     {0.42f, 0.42f, 0.44f, 0.56f},
     {0.56f, 0.58f, 0.62f, 0.30f},
     0, true, true},

    // [2] Tree cluster — dense canopy group
    {"Tree Cluster",
     EnvironmentCategory::TreeCluster,
     {0.16f, 0.32f, 0.14f, 0.78f},
     {0.22f, 0.40f, 0.18f, 0.68f},
     5, false, false},

    // [3] Roadside tree row — linear planting
    {"Roadside Trees",
     EnvironmentCategory::RoadsideTrees,
     {0.18f, 0.34f, 0.16f, 0.72f},
     {0.14f, 0.28f, 0.12f, 0.64f},
     4, false, false},

    // [4] Open grass square — simple lawn
    {"Grass Square",
     EnvironmentCategory::Park,
     {0.20f, 0.36f, 0.18f, 0.80f},
     {0.24f, 0.38f, 0.20f, 0.36f},
     1, false, false},

    // [5] Fountain plaza — landmark-adjacent open space
    {"Fountain Plaza",
     EnvironmentCategory::Plaza,
     {0.44f, 0.46f, 0.50f, 0.58f},
     {0.14f, 0.38f, 0.50f, 0.48f},
     0, true, true},

    // [6] Garden — smaller ornamental green space
    {"Garden",
     EnvironmentCategory::Park,
     {0.22f, 0.38f, 0.20f, 0.82f},
     {0.30f, 0.46f, 0.24f, 0.44f},
     2, true, false},
};

// ============================================================================
//  7. VEHICLE PRESETS
// ============================================================================

static const VehiclePreset kVehiclePresets[] = {

    // [0] Compact hatchback — small diamond
    {"Compact",
     VehicleCategory::Compact,
     2.6f, 1.2f,
     {0.72f, 0.74f, 0.78f, 0.82f},
     {0.08f, 0.08f, 0.10f, 0.58f},
     0.14f},

    // [1] Standard sedan — medium
    {"Sedan",
     VehicleCategory::Sedan,
     3.2f, 1.4f,
     {0.80f, 0.82f, 0.86f, 0.84f},
     {0.08f, 0.08f, 0.10f, 0.56f},
     0.12f},

    // [2] Dark sedan — executive
    {"Executive",
     VehicleCategory::Sedan,
     3.4f, 1.5f,
     {0.18f, 0.20f, 0.24f, 0.88f},
     {0.06f, 0.06f, 0.08f, 0.60f},
     0.10f},

    // [3] Van / service vehicle — larger
    {"Service Van",
     VehicleCategory::Van,
     4.0f, 1.8f,
     {0.86f, 0.86f, 0.82f, 0.86f},
     {0.08f, 0.08f, 0.10f, 0.54f},
     0.08f},

    // [4] Yellow taxi
    {"Taxi",
     VehicleCategory::Sedan,
     3.0f, 1.4f,
     {0.92f, 0.78f, 0.18f, 0.88f},
     {0.08f, 0.08f, 0.10f, 0.56f},
     0.14f},

    // [5] Red compact
    {"Red Compact",
     VehicleCategory::Compact,
     2.8f, 1.2f,
     {0.78f, 0.22f, 0.18f, 0.84f},
     {0.08f, 0.06f, 0.06f, 0.56f},
     0.14f},

    // [6] Blue SUV
    {"Blue SUV",
     VehicleCategory::Van,
     3.6f, 1.6f,
     {0.24f, 0.42f, 0.68f, 0.86f},
     {0.06f, 0.08f, 0.10f, 0.56f},
     0.10f},

    // [7] Delivery truck — box shape
    {"Delivery Truck",
     VehicleCategory::Van,
     4.4f, 2.0f,
     {0.82f, 0.82f, 0.80f, 0.88f},
     {0.10f, 0.10f, 0.10f, 0.52f},
     0.06f},

    // [8] Green city bus
    {"City Bus",
     VehicleCategory::Van,
     5.2f, 1.8f,
     {0.22f, 0.52f, 0.30f, 0.88f},
     {0.06f, 0.08f, 0.06f, 0.54f},
     0.05f},

    // [9] Parked silver — stationary, slightly faded
    {"Parked Silver",
     VehicleCategory::Compact,
     2.8f, 1.3f,
     {0.66f, 0.68f, 0.72f, 0.74f},
     {0.08f, 0.08f, 0.10f, 0.48f},
     0.0f},

    // [10] Parked dark — stationary
    {"Parked Dark",
     VehicleCategory::Sedan,
     3.2f, 1.4f,
     {0.22f, 0.24f, 0.28f, 0.76f},
     {0.06f, 0.06f, 0.08f, 0.48f},
     0.0f},
};

// ============================================================================
//  8. ROAD PROP PRESETS
// ============================================================================

static const RoadPropPreset kRoadPropPresets[] = {

    // [0] Traffic light — standard 3-signal
    {"Traffic Light",
     RoadPropCategory::TrafficLight,
     1.4f, 14.0f,
     {0.18f, 0.18f, 0.20f, 0.86f},
     {0.28f, 0.96f, 0.44f, 0.92f},
     {0.92f, 0.20f, 0.16f, 0.88f}},

    // [1] Streetlight — tall lamp post
    {"Streetlight",
     RoadPropCategory::Streetlight,
     0.8f, 16.0f,
     {0.24f, 0.24f, 0.26f, 0.82f},
     {0.96f, 0.90f, 0.64f, 0.42f},
     {0.0f, 0.0f, 0.0f, 0.0f}},

    // [2] Double streetlight — wide lamp
    {"Double Streetlight",
     RoadPropCategory::Streetlight,
     1.2f, 18.0f,
     {0.22f, 0.22f, 0.24f, 0.84f},
     {0.94f, 0.88f, 0.62f, 0.46f},
     {0.0f, 0.0f, 0.0f, 0.0f}},

    // [3] Bollard / road divider
    {"Bollard",
     RoadPropCategory::Divider,
     0.6f, 3.0f,
     {0.56f, 0.56f, 0.52f, 0.78f},
     {0.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 0.0f}},

    // [4] Jersey barrier / road divider
    {"Road Barrier",
     RoadPropCategory::Divider,
     1.8f, 2.4f,
     {0.62f, 0.60f, 0.56f, 0.82f},
     {0.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 0.0f}},

    // [5] Crosswalk stripes
    {"Crosswalk",
     RoadPropCategory::Crosswalk,
     6.0f, 0.4f,
     {0.88f, 0.88f, 0.84f, 0.52f},
     {0.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 0.0f}},

    // [6] Bus stop shelter
    {"Bus Stop",
     RoadPropCategory::Streetlight,
     2.2f, 8.0f,
     {0.34f, 0.36f, 0.40f, 0.82f},
     {0.68f, 0.78f, 0.90f, 0.26f},
     {0.0f, 0.0f, 0.0f, 0.0f}},
};

// ============================================================================
//  CATALOG ACCESS FUNCTIONS
// ============================================================================

int getLandmarkCount() {
    return static_cast<int>(sizeof(kLandmarkPresets) / sizeof(kLandmarkPresets[0]));
}

const BuildingPreset& getLandmark(int index) {
    return kLandmarkPresets[index % getLandmarkCount()];
}

int getOfficeTowerCount() {
    return static_cast<int>(sizeof(kOfficeTowerPresets) / sizeof(kOfficeTowerPresets[0]));
}

const BuildingPreset& getOfficeTower(int index) {
    return kOfficeTowerPresets[index % getOfficeTowerCount()];
}

int getMidRiseCount() {
    return static_cast<int>(sizeof(kMidRisePresets) / sizeof(kMidRisePresets[0]));
}

const BuildingPreset& getMidRise(int index) {
    return kMidRisePresets[index % getMidRiseCount()];
}

int getCivicCount() {
    return static_cast<int>(sizeof(kCivicPresets) / sizeof(kCivicPresets[0]));
}

const BuildingPreset& getCivic(int index) {
    return kCivicPresets[index % getCivicCount()];
}

int getLowRiseCount() {
    return static_cast<int>(sizeof(kLowRisePresets) / sizeof(kLowRisePresets[0]));
}

const BuildingPreset& getLowRise(int index) {
    return kLowRisePresets[index % getLowRiseCount()];
}

int getEnvironmentCount() {
    return static_cast<int>(sizeof(kEnvironmentPresets) / sizeof(kEnvironmentPresets[0]));
}

const EnvironmentPreset& getEnvironment(int index) {
    return kEnvironmentPresets[index % getEnvironmentCount()];
}

int getVehicleCount() {
    return static_cast<int>(sizeof(kVehiclePresets) / sizeof(kVehiclePresets[0]));
}

const VehiclePreset& getVehicle(int index) {
    return kVehiclePresets[index % getVehicleCount()];
}

int getRoadPropCount() {
    return static_cast<int>(sizeof(kRoadPropPresets) / sizeof(kRoadPropPresets[0]));
}

const RoadPropPreset& getRoadProp(int index) {
    return kRoadPropPresets[index % getRoadPropCount()];
}

// ============================================================================
//  TILE POPULATION — assigns presets to map tiles by zone
// ============================================================================

TileAssignment assignTile(ZoneType zone, int tileIndex, unsigned int seed) {
    TileAssignment tile;
    tile.zone = zone;

    // Deterministic per-tile variation using seed+index hash
    const unsigned int h = seed ^ (static_cast<unsigned int>(tileIndex) * 2654435761u);
    auto pick = [&](int range) -> int {
        return static_cast<int>((h >> 3) % static_cast<unsigned int>(range));
    };
    auto pickAlt = [&](int range) -> int {
        return static_cast<int>(((h >> 7) ^ (h * 31u)) % static_cast<unsigned int>(range));
    };
    const float jitter = static_cast<float>(h & 0xFFu) / 255.0f;  // 0..1

    switch (zone) {

    case ZoneType::Core: {
        // Twin towers + flanking landmarks
        tile.buildings.push_back({getLandmark(0), 0.0f, -0.04f, 1.0f + jitter * 0.08f});
        tile.buildings.push_back({getLandmark(1), 0.0f, -0.04f, 1.0f + jitter * 0.06f});
        tile.buildings.push_back({getOfficeTower(pick(getOfficeTowerCount())),
                                  -0.34f, -0.12f, 0.70f + jitter * 0.10f});
        tile.buildings.push_back({getOfficeTower(pickAlt(getOfficeTowerCount())),
                                  0.34f, -0.08f, 0.68f + jitter * 0.10f});
        tile.environment.push_back({getEnvironment(5), 0.0f, 0.24f});  // Fountain plaza
        tile.environment.push_back({getEnvironment(3), -0.28f, 0.30f}); // Roadside trees
        tile.environment.push_back({getEnvironment(3), 0.28f, 0.30f});
        break;
    }

    case ZoneType::Financial: {
        // Dense commercial towers — pick 4-5 distinct ones
        const int primary = pick(getOfficeTowerCount());
        const int secondary = (primary + 1 + pickAlt(getOfficeTowerCount() - 1)) % getOfficeTowerCount();
        const int tertiary = (primary + 3) % getOfficeTowerCount();
        tile.buildings.push_back({getOfficeTower(primary),
                                  -0.20f, -0.08f, 0.85f + jitter * 0.15f});
        tile.buildings.push_back({getOfficeTower(secondary),
                                  0.18f, -0.14f, 0.80f + jitter * 0.12f});
        tile.buildings.push_back({getOfficeTower(tertiary),
                                  0.02f, 0.06f, 0.72f + jitter * 0.10f});
        tile.buildings.push_back({getMidRise(pick(getMidRiseCount())),
                                  0.0f, 0.28f, 0.68f + jitter * 0.08f});
        if (jitter > 0.35f) {
            tile.buildings.push_back({getOfficeTower((primary + 5) % getOfficeTowerCount()),
                                      -0.30f, 0.18f, 0.60f + jitter * 0.10f});
        }
        tile.environment.push_back({getEnvironment(1), 0.0f, 0.20f}); // Plaza
        break;
    }

    case ZoneType::Mixed: {
        // Mix of mid-rise offices, low-rise, and some green
        tile.buildings.push_back({getMidRise(pick(getMidRiseCount())),
                                  -0.18f, -0.06f, 0.80f + jitter * 0.12f});
        tile.buildings.push_back({getLowRise(pickAlt(getLowRiseCount())),
                                  0.18f, -0.02f, 0.76f + jitter * 0.10f});
        tile.buildings.push_back({getLowRise((pick(getLowRiseCount()) + 2) % getLowRiseCount()),
                                  0.0f, 0.24f, 0.72f + jitter * 0.08f});
        tile.environment.push_back({getEnvironment(4), 0.10f, 0.16f}); // Grass
        if (jitter > 0.6f) {
            tile.environment.push_back({getEnvironment(3), -0.24f, 0.28f}); // Trees
        }
        break;
    }

    case ZoneType::Residential: {
        // Low-rise, warm-toned, with green spaces
        tile.buildings.push_back({getLowRise(0),
                                  0.0f, -0.10f, 0.82f + jitter * 0.10f});
        tile.buildings.push_back({getLowRise(1),
                                  -0.18f, 0.12f, 0.78f + jitter * 0.10f});
        tile.buildings.push_back({getLowRise(pick(getLowRiseCount())),
                                  0.20f, 0.08f, 0.74f + jitter * 0.08f});
        tile.environment.push_back({getEnvironment(0), 0.0f, 0.26f}); // Park
        tile.environment.push_back({getEnvironment(2), -0.22f, 0.18f}); // Trees
        break;
    }

    case ZoneType::Campus: {
        // Civic/education buildings with lots of green
        tile.buildings.push_back({getCivic(0), 0.0f, -0.12f, 0.84f + jitter * 0.08f});
        tile.buildings.push_back({getCivic(pick(getCivicCount())),
                                  -0.24f, 0.14f, 0.78f + jitter * 0.08f});
        tile.buildings.push_back({getCivic(pickAlt(getCivicCount())),
                                  0.26f, 0.10f, 0.74f + jitter * 0.06f});
        tile.environment.push_back({getEnvironment(6), 0.0f, 0.22f}); // Garden
        tile.environment.push_back({getEnvironment(2), 0.20f, 0.28f}); // Trees
        tile.environment.push_back({getEnvironment(4), -0.18f, 0.24f}); // Grass
        break;
    }

    case ZoneType::Park: {
        // Green space dominant — minimal buildings
        tile.environment.push_back({getEnvironment(0), 0.0f, 0.0f}); // Park
        tile.environment.push_back({getEnvironment(2), -0.22f, -0.14f}); // Trees
        tile.environment.push_back({getEnvironment(2), 0.20f, 0.16f});
        tile.environment.push_back({getEnvironment(6), 0.0f, 0.20f}); // Garden
        if (jitter > 0.5f) {
            tile.buildings.push_back({getCivic(2), 0.28f, -0.24f, 0.60f}); // Small pavilion
        }
        break;
    }

    case ZoneType::Service: {
        // Utility, warehouses, service depot
        tile.buildings.push_back({getCivic(3), -0.10f, -0.04f, 0.76f + jitter * 0.08f});
        tile.buildings.push_back({getLowRise(2), 0.24f, -0.20f, 0.72f + jitter * 0.06f});
        if (jitter > 0.4f) {
            tile.buildings.push_back({getLowRise(5), 0.0f, 0.18f, 0.68f + jitter * 0.06f});
        }
        break;
    }

    }

    // Road props — add crosswalks and streetlights contextually
    if (zone == ZoneType::Core || zone == ZoneType::Financial) {
        tile.roadProps.push_back({getRoadProp(1), -0.38f, 0.30f}); // Streetlight
        tile.roadProps.push_back({getRoadProp(1), 0.38f, 0.30f});
        if (zone == ZoneType::Core) {
            tile.roadProps.push_back({getRoadProp(5), 0.0f, 0.38f}); // Crosswalk
        }
    }
    if (zone == ZoneType::Mixed || zone == ZoneType::Residential) {
        tile.roadProps.push_back({getRoadProp(pickAlt(2)), -0.36f, 0.28f});
    }

    // Parked vehicles — scatter a few
    if (zone != ZoneType::Park) {
        const int carCount = (zone == ZoneType::Financial || zone == ZoneType::Core) ? 2 : 1;
        for (int c = 0; c < carCount; ++c) {
            const int vi = (pick(getVehicleCount()) + c * 3) % getVehicleCount();
            tile.vehicles.push_back({getVehicle(vi),
                                     -0.32f + c * 0.24f + jitter * 0.06f,
                                     0.34f + c * 0.04f});
        }
    }

    return tile;
}

// ============================================================================
//  ZONE CLASSIFICATION — maps CityThemeRenderer district to asset zone
// ============================================================================

ZoneType classifyZone(int districtOrdinal) {
    switch (districtOrdinal) {
        case 0: return ZoneType::Core;       // Landmark
        case 1: return ZoneType::Financial;  // Commercial
        case 2: return ZoneType::Mixed;      // Mixed
        case 3: return ZoneType::Residential; // Residential
        case 4: return ZoneType::Campus;     // Campus
        case 5: return ZoneType::Park;       // Park
        case 6: return ZoneType::Service;    // Service
        default: return ZoneType::Service;
    }
}

} // namespace CityAssets
