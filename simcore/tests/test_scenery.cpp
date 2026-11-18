// Ultra-basic models of every asset (simcore/scenery/, data/scenery/, docs/scenery/).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/command.hpp"
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
        if (m.archetype == "jet" || m.archetype == "wheeled" || m.archetype == "tracked" || m.archetype == "building" || m.archetype == "truck" ||
            m.archetype == "gun" || m.archetype == "launcher" || m.archetype == "radar") {
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

SC_TEST("scenery: every equipment type of our order of battle has a basic model; ours are never the enemy's model of the type") {
    const ModelLibrary lib = Library();
    command::OrderOfBattle oob;
    std::string e;
    SC_EXPECT(command::LoadOrderOfBattle(kData, "command/oob_pannonia.json", oob, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    std::vector<std::string> types;
    for (const command::OobUnit& u : oob.units) {
        for (const std::pair<std::string, int>& eq : u.equipment) {
            if (std::find(types.begin(), types.end(), eq.first) == types.end()) types.push_back(eq.first);
        }
    }
    // Any adversary's aircraft: a battery that sees one of these models knows it for the enemy's.
    std::vector<std::string> enemy;
    for (const char* f : {"rebels", "neutral", "warsaw_pact", "nato"}) {
        airwar::FactionDef fd;
        SC_EXPECT(airwar::LoadFactionFile(kData + "/air_war/faction_" + f + ".json", fd, &e));
        for (const airwar::AircraftType& a : fd.aircraft) enemy.push_back(a.id);
    }
    int aircraft = 0;
    for (const std::string& t : types) {
        const ModelDef* m = lib.Find(t);
        if (m == nullptr) std::printf("    no model for %s\n", t.c_str());
        SC_EXPECT(m != nullptr);
        if (m == nullptr) continue;
        const bool flies = m->archetype == "jet" || m->archetype == "prop" || m->archetype == "helicopter";
        if (!flies) continue;
        ++aircraft;
        const bool theirs = std::find(enemy.begin(), enemy.end(), t) != enemy.end();
        if (theirs) std::printf("    our %s is drawn as the enemy's model\n", t.c_str());
        SC_EXPECT(!theirs);
    }
    std::printf("    %zu equipment types, %d of them aircraft\n", types.size(), aircraft);
    SC_EXPECT(types.size() >= 45 && aircraft >= 8);
    // The ones that needed something new of their archetype: a Shilka's four barrels, the transloaders' missiles lying
    // on their beds, the An-26's two engines on its wings, the P-14's great array.
    const auto count = [&](const std::string& id, Shape s) {
        int n = 0;
        const ModelDef* m = lib.Find(id);
        if (m != nullptr) {
            for (const Part& p : m->parts) n += p.shape == s ? 1 : 0;
        }
        return n;
    };
    SC_EXPECT(count("zsu_23_4", Shape::Cylinder) >= 6);   // a dish and its post, four barrels
    SC_EXPECT(count("s125_transloader", Shape::Cylinder) == 2 + 6);  // two missiles and six wheels
    SC_EXPECT(count("an26", Shape::Cylinder) == 4);       // two nacelles, two propellers
    const ModelDef* p14 = lib.Find("p14");
    SC_EXPECT(p14 != nullptr && p14->span_m == 32.0 && p14->parts.size() == 3);
}

SC_TEST("scenery: every air defence system has a site to draw: its vehicles round its centre") {
    const ModelLibrary lib = Library();
    for (const airwar::AirDefenceSystem& s : airwar::AirDefenceSystems()) {
        const std::vector<std::pair<std::string, Vec3>> site = lib.SiteLayout(s.id);
        if (site.empty()) std::printf("    no site for %s\n", s.id);
        SC_EXPECT(!site.empty());
        for (const auto& v : site) SC_EXPECT(lib.Find(v.first) != nullptr);
    }
    // An S-75M2 fire unit: its guidance radar at the centre, six launchers and the P-18 in a ring 80 m out.
    const std::vector<std::pair<std::string, Vec3>> s75 = lib.SiteLayout("s75m2");
    SC_EXPECT(s75.size() == 8 && s75.front().first == "s75_radar" && s75.front().second.HorizontalLength() < 1e-9);
    int launchers = 0;
    for (std::size_t i = 1; i < s75.size(); ++i) {
        SC_EXPECT_NEAR(s75[i].second.HorizontalLength(), 80.0, 1e-6);
        launchers += s75[i].first == "s75_launcher" ? 1 : 0;
    }
    SC_EXPECT(launchers == 6);
    // An Osa battery: its four vehicles, all in the ring, 300 m out.
    const std::vector<std::pair<std::string, Vec3>> osa = lib.SiteLayout("osa");
    SC_EXPECT(osa.size() == 4);
    for (const auto& v : osa) SC_EXPECT(v.first == "osa_9a33" && std::fabs(v.second.HorizontalLength() - 300.0) < 1e-6);
    SC_EXPECT(lib.SiteLayout("no_such_system").empty());
}

SC_TEST("scenery: every object of the nation's theatre is drawn on the map as its system's or its kind's site (cities as towns)") {
    const ModelLibrary lib = Library();
    airwar::TheatreDef th;
    std::string e;
    SC_EXPECT(airwar::LoadTheatreFile(kData + "/air_war/theatre_pannonia_nation.json", th, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    int drawn = 0, models = 0;
    for (const airwar::TheatreObject& o : th.objects) {
        const std::vector<std::pair<std::string, Vec3>> site = lib.ObjectLayout(airwar::ObjectKindName(o.kind), o.defence.system);
        if (o.kind == airwar::ObjectKind::City) {
            SC_EXPECT(site.empty());  // the country map's towns
            continue;
        }
        if (site.empty()) std::printf("    nothing to draw for %s (%s)\n", o.id.c_str(), airwar::ObjectKindName(o.kind));
        SC_EXPECT(!site.empty());
        for (const auto& v : site) SC_EXPECT(lib.Find(v.first) != nullptr);
        drawn += site.empty() ? 0 : 1;
        models += static_cast<int>(site.size());
    }
    std::printf("    %d objects drawn with %d models\n", drawn, models);
    // An air defence by its system, whatever its kind: the S-125M1 at Kecel is a SAM site of the S-125M1's layout.
    SC_EXPECT(lib.ObjectLayout("sam_site", "s125m1").size() == lib.SiteLayout("s125m1").size());
    // A refinery: its hall at the centre, the tanks and chimneys round it.
    const std::vector<std::pair<std::string, Vec3>> ref = lib.ObjectLayout("refinery", "");
    SC_EXPECT(ref.size() == 9 && ref.front().first == "hall" && ref.front().second.HorizontalLength() < 1e-9);
    SC_EXPECT(lib.ObjectLayout("city", "").empty() && lib.ObjectLayout("no_such_kind", "").empty());
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

SC_TEST("scenery: the loader refuses a kind's site with an unknown model") {
    ModelLibrary bad;
    std::string e;
    SC_EXPECT(!LoadModelLibraryJson(R"({"models_id": "x", "models": [{"id": "a", "name": "A", "archetype": "tower", "height_m": 5, "diameter_m": 2, "colour": "#ffffff"}],
        "kinds": {"refinery": {"models": [["a", 1], ["no_such_model", 3]], "spread_m": 50}}})",
                                    bad, &e));
    SC_EXPECT(e.find("kinds.refinery.models[1]") != std::string::npos);
}

SC_TEST("scenery: a search radar's antenna turns at its turns a minute, the rest of it stands still") {
    const ModelLibrary lib = Library();
    for (const char* id : {"p14", "p15", "p18", "p19", "p40"}) {
        const ModelDef* m = lib.Find(id);
        SC_EXPECT(m != nullptr);
        if (m == nullptr) continue;
        int turning = 0;
        for (const Part& p : m->parts) turning += p.turns ? 1 : 0;
        SC_EXPECT(m->antenna_rpm > 0.0 && turning == 1 && m->parts.size() > 1);  // the antenna only
    }
    // ru_wiki_p19, rusarmy_p15_p19: 6 s a turn; ru_wiki_p18: 10 s.
    SC_EXPECT_NEAR(lib.Find("p19")->antenna_rpm, 10.0, 1e-9);
    SC_EXPECT_NEAR(lib.Find("p18")->antenna_rpm, 6.0, 1e-9);
    const ModelDef* tank = lib.Find("t55");
    SC_EXPECT(tank != nullptr && tank->antenna_rpm == 0.0);
    // Turns a minute on a model with no antenna are refused.
    ModelLibrary bad;
    std::string e;
    SC_EXPECT(!LoadModelLibraryJson(R"({"models_id": "x", "models": [{"id": "a", "name": "A", "archetype": "tower", "height_m": 5, "diameter_m": 2, "antenna_rpm": 6, "colour": "#ffffff"}]})",
                                    bad, &e));
    SC_EXPECT(e.find("antenna_rpm") != std::string::npos);
}

// The register of third-party art (simcore/scenery/asset_register.hpp, data/scenery/assets.json, docs/scenery/ASSETS.md).
namespace {

// A register around `assets` (the array's items) and `parts` (the object's members).
std::string Register(const std::string& assets, const std::string& parts) {
    return "{\"assets\": [" + assets + "], \"parts\": {" + parts + "}}";
}

const char* kCc0Oak =
    R"({"id": "oak_a", "unreal_path": "/Game/SAM3D/Map/Trees/SM_OakA.SM_OakA", "origin": "own work", "author": "", "licence": "CC0-1.0",
        "redistributable": true, "fetched": "2026-11-02", "sha256": "", "scale": 1.0, "yaw_deg": 90, "z_offset_m": -0.2})";

}  // namespace

SC_TEST("scenery: the asset register starts empty, every part of the map procedural") {
    AssetRegister reg;
    std::string e;
    SC_EXPECT(reg.Load(kData + "/scenery/assets.json", &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(reg.assets.empty());
    for (const std::string& part : MapMeshParts()) {
        SC_EXPECT(reg.parts.count(part) == 1);  // every part listed, so the file says what it may hold
        SC_EXPECT(reg.AssetFor(part, 0) == nullptr);
    }
}

SC_TEST("scenery: the asset register keeps its rules") {
    std::string e;
    {
        // A redistributable CC0 asset anywhere under /Game/, one variant of the houses and every tree of a kind.
        AssetRegister reg;
        SC_EXPECT(reg.Load(Register(kCc0Oak, R"("tree_oak": {"asset": "oak_a"}, "house": {"asset": null, "variants": [null, "oak_a"]})"), &e));
        SC_EXPECT(reg.AssetFor("tree_oak", 3) != nullptr && reg.AssetFor("tree_oak", 3)->yaw_deg == 90.0);
        SC_EXPECT(reg.AssetFor("house", 0) == nullptr && reg.AssetFor("house", 1) != nullptr && reg.AssetFor("house", 2) == nullptr);
        SC_EXPECT(reg.AssetFor("barn", 0) == nullptr);
        SC_EXPECT_NEAR(reg.Find("oak_a")->z_offset_m, -0.2, 1e-12);
        // A failed load leaves the register as it was.
        SC_EXPECT(!reg.Load(Register("", R"("windmill": {"asset": null})"), &e));
        SC_EXPECT(e.find("windmill") != std::string::npos);
        SC_EXPECT(reg.assets.size() == 1 && reg.AssetFor("tree_oak", 0) != nullptr);
    }
    auto refused = [&](const std::string& text, const char* why) {
        AssetRegister reg;
        std::string err;
        const bool ok = reg.Load(text, &err);
        if (ok || err.find(why) == std::string::npos) std::printf("    expected a refusal for '%s', got '%s'\n", why, err.c_str());
        return !ok && err.find(why) != std::string::npos;
    };
    // Fab's Standard licence: the files may not be redistributed, so under /Game/ThirdParty/ only (git ignores it).
    const std::string fab_outside =
        R"({"id": "barn_fab", "unreal_path": "/Game/SAM3D/Map/SM_Barn.SM_Barn", "origin": "https://www.fab.com/", "licence": "Fab-Standard", "redistributable": false, "fetched": "2026-11-02"})";
    SC_EXPECT(refused(Register(fab_outside, ""), "/Game/ThirdParty/"));
    const std::string fab_inside =
        R"({"id": "barn_fab", "unreal_path": "/Game/ThirdParty/Farm/SM_Barn.SM_Barn", "origin": "https://www.fab.com/", "licence": "Fab-Standard", "redistributable": false, "fetched": "2026-11-02"})";
    {
        AssetRegister reg;
        SC_EXPECT(reg.Load(Register(fab_inside, R"("barn": {"asset": "barn_fab"})"), &e));
    }
    // ... and never marked redistributable.
    std::string fab_free = fab_inside;
    fab_free.replace(fab_free.find("\"redistributable\": false"), 24, "\"redistributable\": true");
    SC_EXPECT(refused(Register(fab_free, ""), "does not allow"));
    // CC-BY names its author.
    const std::string ccby =
        R"({"id": "pine_by", "unreal_path": "/Game/SAM3D/Map/SM_Pine.SM_Pine", "origin": "https://example.org/pine", "author": "", "licence": "CC-BY-4.0", "redistributable": true, "fetched": "2026-11-02"})";
    SC_EXPECT(refused(Register(ccby, ""), "author"));
    // A licence of its own, a missing flag, a path outside /Game/, a missing date, an id twice.
    SC_EXPECT(refused(Register(R"({"id": "x", "unreal_path": "/Game/ThirdParty/X.X", "origin": "o", "licence": "royalty free", "redistributable": false, "fetched": "2026-11-02"})", ""), "licence"));
    SC_EXPECT(refused(Register(R"({"id": "x", "unreal_path": "/Game/ThirdParty/X.X", "origin": "o", "licence": "Own", "fetched": "2026-11-02"})", ""), "redistributable"));
    SC_EXPECT(refused(Register(R"({"id": "x", "unreal_path": "C:/art/X.fbx", "origin": "o", "licence": "Own", "redistributable": true, "fetched": "2026-11-02"})", ""), "/Game/"));
    SC_EXPECT(refused(Register(R"({"id": "x", "unreal_path": "/Game/X.X", "origin": "o", "licence": "Own", "redistributable": true})", ""), "fetched"));
    SC_EXPECT(refused(Register(std::string(kCc0Oak) + ", " + kCc0Oak, ""), "twice"));
    // A part names a registered asset, or null.
    SC_EXPECT(refused(Register("", R"("house": {"asset": "nobody"})"), "not registered"));
    SC_EXPECT(refused(Register("", R"("house": {})"), "null for procedural"));
    SC_EXPECT(refused("{\"parts\": {}}", "assets"));
}
