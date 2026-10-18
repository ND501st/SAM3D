// Ultra-basic models of every asset (simcore/scenery/, data/scenery/, docs/scenery/).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenery.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::scenery;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

ModelLibrary Library() {
    ModelLibrary lib;
    std::string e;
    if (!LoadModelLibrary(kData + "/scenery/basic_models.json", lib, &e)) std::printf("    %s\n", e.c_str());
    return lib;
}

// The parts' overall extent along x, y and z (a box around every part, unrotated: a fair upper bound).
Vec3 Extent(const ModelDef& m, Vec3& lo, Vec3& hi) {
    lo = Vec3{1e9, 1e9, 1e9};
    hi = Vec3{-1e9, -1e9, -1e9};
    for (const Part& p : m.parts) {
        Vec3 s = p.size;
        if (p.shape == Shape::Cylinder || p.shape == Shape::Cone) {
            if (p.axis == 'x') s = Vec3{p.size.z, p.size.x, p.size.y};
            if (p.axis == 'y') s = Vec3{p.size.x, p.size.z, p.size.y};
        }
        // The box turned by its tilt: each axis's extent is the sum of the box's sides on it (signs don't matter).
        const double k = 3.141592653589793 / 180.0;
        const double cp = std::cos(p.pitch_deg * k), sp = std::sin(p.pitch_deg * k);
        const double cr = std::cos(p.roll_deg * k), sr = std::sin(p.roll_deg * k);
        const double cy = std::cos(p.yaw_deg * k), sy = std::sin(p.yaw_deg * k);
        const double R[3][3] = {{cy * cp, cy * sp * sr - sy * cr, cy * sp * cr + sy * sr},
                                {sy * cp, sy * sp * sr + cy * cr, sy * sp * cr - cy * sr},
                                {-sp, cp * sr, cp * cr}};
        const double side[3] = {s.x, s.y, s.z};
        double e[3] = {0.0, 0.0, 0.0};
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) e[i] += std::fabs(R[i][j]) * side[j];
        }
        const Vec3 h{e[0] * 0.5, e[1] * 0.5, e[2] * 0.5};
        lo = Vec3{std::min(lo.x, p.at.x - h.x), std::min(lo.y, p.at.y - h.y), std::min(lo.z, p.at.z - h.z)};
        hi = Vec3{std::max(hi.x, p.at.x + h.x), std::max(hi.y, p.at.y + h.y), std::max(hi.z, p.at.z + h.z)};
    }
    return hi - lo;
}

}  // namespace

SC_TEST("scenery: every aircraft of the air war and every battery vehicle has a basic model of its size") {
    const ModelLibrary lib = Library();
    SC_EXPECT(lib.models.size() >= 60);
    // Every aircraft any adversary flies.
    int aircraft = 0;
    for (const char* f : {"rebels", "neutral", "warsaw_pact", "nato"}) {
        airwar::FactionDef fd;
        std::string e;
        SC_EXPECT(airwar::LoadFactionFile(kData + "/air_war/faction_" + f + ".json", fd, &e));
        for (const airwar::AircraftType& a : fd.aircraft) {
            ++aircraft;
            const ModelDef* m = lib.Find(a.id);
            if (m == nullptr) std::printf("    no model for %s (%s)\n", a.id.c_str(), f);
            SC_EXPECT(m != nullptr);
            if (m != nullptr && a.helicopter) SC_EXPECT(m->archetype == "helicopter");
        }
    }
    // Every kind of battery vehicle the scenarios have.
    for (const char* k : {"launcher", "reload", "fuel", "command_post", "maintenance", "dummy"}) SC_EXPECT(!lib.ForGround(k).empty());
    // A target's own model, else its class's, else a helicopter's by its type tags.
    SC_EXPECT(lib.ForTarget("su24", "bomber", "Air+FixedWing") == "su24");
    SC_EXPECT(lib.ForTarget("", "bomber", "Air+FixedWing") == "generic_bomber");
    SC_EXPECT(lib.ForTarget("", "default", "Air+Rotorcraft") == "generic_helicopter");
    SC_EXPECT(lib.ForTarget("no_such_type", "drone", "Air+FixedWing") == "generic_drone");
    // Every model is built, and is about its stated size (parts within 1.6 times its length, span and height).
    int checked = 0;
    for (const ModelDef& m : lib.models) {
        SC_EXPECT(!m.parts.empty());
        for (const Part& p : m.parts) SC_EXPECT(p.colour.size() == 7 && p.colour[0] == '#');
        Vec3 lo, hi;
        const Vec3 ext = Extent(m, lo, hi);
        const double big = std::max(m.length_m, std::max(m.span_m, m.height_m));
        if (m.archetype == "jet" || m.archetype == "wheeled" || m.archetype == "tracked" || m.archetype == "building") {
            // Masts and antennas may stand well above a vehicle's stated height; an aircraft's height is its height.
            const double tall = m.archetype == "jet" ? 1.6 * big + 2.0 : 4.0 * std::max(m.height_m, 1.0) + 6.0;
            const bool fits = ext.x <= 1.6 * std::max(m.length_m, 1.0) + 2.0 && ext.y <= 1.6 * std::max(m.span_m, 1.0) + 2.0 && ext.z <= tall;
            if (!fits) std::printf("    %s: extent %.1f x %.1f x %.1f against %.1f x %.1f x %.1f\n", m.id.c_str(), ext.x, ext.y, ext.z, m.length_m, m.span_m, m.height_m);
            SC_EXPECT(fits);
            ++checked;
        }
    }
    // A jet is longer than it is wide only if its data says so; a fighter's nose is forward.
    const ModelDef* f16 = lib.Find("f16a");
    SC_EXPECT(f16 != nullptr);
    if (f16 != nullptr) {
        double nose = -1e9;
        for (const Part& p : f16->parts) {
            if (p.shape == Shape::Cone) nose = p.at.x;
        }
        SC_EXPECT(nose > 0.3 * f16->length_m);
    }
    std::printf("    %zu models; %d aircraft types covered; %d checked against their size\n", lib.models.size(), aircraft, checked);

    // The loader refuses an unknown archetype or colour.
    ModelLibrary bad;
    std::string e;
    SC_EXPECT(!LoadModelLibraryJson(R"({"models_id": "x", "models": [{"id": "a", "name": "A", "archetype": "zeppelin", "colour": "#ffffff"}]})", bad, &e) &&
              e.find("archetype") != std::string::npos);
    SC_EXPECT(!LoadModelLibraryJson(R"({"models_id": "x", "models": [{"id": "a", "name": "A", "archetype": "tower", "height_m": 5, "diameter_m": 2, "colour": "puce"}]})",
                                    bad, &e) &&
              e.find("colour") != std::string::npos);
}

SC_TEST("scenery: a raid's aircraft carry their model into the scenario, and the loader keeps it") {
    airwar::AirWarData d;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", d, &e));
    airwar::AirCampaign c;
    SC_EXPECT(c.Init(d, 2u));
    bool raid = false;
    for (int day = 0; day < 6 && !raid; ++day) raid = c.Advance(c.Now() + 24.0);
    SC_EXPECT(raid);
    if (!raid) return;
    airwar::RaidScenarioBuilder b;
    const airwar::RaidScenario rs = b.Build(c, *c.PlayerMission());
    SC_EXPECT(rs.ok);
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenario(rs.json_text, kData, def, console, &e));
    const ModelLibrary lib = Library();
    int with_model = 0;
    for (const ScenarioTarget& t : def.targets) {
        with_model += lib.Find(t.model) != nullptr ? 1 : 0;
        SC_EXPECT(!lib.ForTarget(t.model, t.target_class, t.type).empty());
    }
    std::printf("    %zu aircraft in the raid, %d with their own model\n", def.targets.size(), with_model);
    SC_EXPECT(!def.targets.empty() && with_model == static_cast<int>(def.targets.size()));
}
