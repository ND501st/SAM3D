// The air battle's air-to-air weapons and sensors as data, and the shot as a chain of chances (roadmap track B, B7;
// docs/air_battle/07_AIR_TO_AIR.md; simcore/air_war/air_combat_types.hpp, air_combat_data.hpp, air_combat.hpp;
// data/air_war/air_combat.json and sensors.json; data/em/fighter_radars_1985.json). The chain's links are checked against
// hand calculations with generic numbers; the data's chain is held to the period's rates (checo_1967_a2a p. 44).
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/em.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;
constexpr double kDeg = 3.14159265358979323846 / 180.0;

AirCombatDef LoadReal() {
    AirCombatDef d;
    std::string e;
    if (!LoadAirCombatFile(kData + "/air_war/air_combat.json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

SensorsDef LoadRealSensors() {
    SensorsDef d;
    std::string e;
    if (!LoadSensorsFile(kData + "/air_war/sensors.json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

// Generic placeholder numbers, for the hand calculations (never real system data).
ChainParams GenericChain() {
    ChainParams c;
    c.alt_ref_m = 10000.0;
    c.rear_aspect_limit_rad = 90.0 * kDeg;
    c.judge_range_sigma = 0.3;
    c.judge_aspect_sigma_rad = 20.0 * kDeg;
    c.judge_cone_sigma_rad = 10.0 * kDeg;
    c.judge_g_sigma = 0.3;
    c.judge_skill_gain = 0.8;
    c.judge_stress_gain = 0.5;
    c.judge_sarh_scale = 2.0;
    c.switch_error_base = 0.1;
    c.switch_idle_share = 0.2;
    c.switch_skill_gain = 0.5;
    c.switch_stress_gain = 1.0;
    c.guides_ir_rear = 0.8;
    c.guides_ir_all_aspect = 0.9;
    c.guides_sarh = 0.7;
    c.guides_sarh_inertial = 0.7;
    c.aspect_ir_rear_edge = 0.5;
    c.aspect_ir_all_head = 0.8;
    c.sun_soft_factor = 1.5;
    c.sun_soft_floor = 0.2;
    c.beam_notch = 0.5;
    c.beam_notch_half_rad = 20.0 * kDeg;
    c.break_max = 0.6;
    c.break_miss_m = 6.0;
    c.nominal_miss_m = 1.5;
    return c;
}

AamDef GenericIr() {  // a rear-aspect round: generic numbers
    AamDef a;
    a.id = "test_ir";
    a.guidance = Guidance::IrRear;
    a.max_rear_m = 7000.0;
    a.low_factor = 0.4;
    a.min_m = 300.0;
    a.launch_g = 6.0;
    a.seeker_cone_rad = 12.0 * kDeg;
    a.target_g = 8.0;
    a.sun_exclusion_rad = 30.0 * kDeg;
    a.clutter_below = 0.5;
    a.flare_rejection = 0.25;
    a.reliability = 0.8;
    a.design_pk = 0.5;
    a.lethal_m = 2.5;
    return a;
}

AamDef GenericSarh() {
    AamDef a;
    a.id = "test_sarh";
    a.guidance = Guidance::Sarh;
    a.max_head_m = 30000.0;
    a.max_rear_m = 12000.0;
    a.low_factor = 0.5;
    a.min_m = 1000.0;
    a.launch_g = 3.0;
    a.seeker_cone_rad = 60.0 * kDeg;
    a.target_g = 6.0;
    a.clutter_below = 0.6;
    a.reliability = 0.9;
    a.design_pk = 0.7;
    a.lethal_m = 8.0;
    return a;
}

ShotGeometry RearShot(double range_m) {
    ShotGeometry g;
    g.range_m = range_m;
    g.aspect_rad = 0.0;
    g.altitude_m = 10000.0;
    g.launch_g = 1.0;
    return g;
}

// A minimal data file the loaders accept; the strictness tests change one thing in it.
std::string Mini() {
    return R"({
 "air_combat_id": "t", "status": "s", "source": "game value", "confidence": "low",
 "aams": [ { "id": "m1", "name": "M1", "guidance": "ir_rear", "in_service_year": 1970, "max_rear_m": 5000, "min_m": 300, "reliability": 0.8,
             "game_values": ["max_rear_m"], "source": "game value", "confidence": "low" },
           { "id": "m2", "name": "M2", "guidance": "sarh_inertial", "in_service_year": 1980, "allies_year": 1985, "max_head_m": 20000, "max_rear_m": 9000,
             "min_m": 500, "autonomous_s": 10, "source": "game value", "confidence": "low" } ],
 "guns": [ { "id": "g1", "name": "G1", "firing_s": 4, "effective_m": 900, "kill_per_s": 0.1, "source": "game value", "confidence": "low" } ],
 "fits": [ { "id": "t1", "aliases": ["t1a"], "iff": "none", "loads_by_year": [ { "from": 1975, "aams": [["m1", 2]], "gun": "g1" },
                                                                             { "from": 1984, "aams": [["m1", 2], ["m2", 2]], "gun": "g1" } ],
             "source": "game value", "confidence": "low" } ],
 "doctrine": [ { "faction": "nato", "roe": "vid", "commit_by": "leader", "initiative_at_contact": "flight_leader", "formation": "pair",
                 "sort": "nearest", "shot_order": ["ir", "gun"], "tactic": "turn", "strikers_answer": { "jettison": 0.5, "press_on": 0.3, "abort": 0.2,
                 "source": "game value", "confidence": "low" }, "source": "game value", "confidence": "low" } ],
 "calibration": { "source": "game value", "confidence": "low",
   "chain": { "source": "game value", "confidence": "low", "guides_sarh": 0.6 },
   "mix": { "source": "game value", "confidence": "low", "skill": 0.4 },
   "targets": { "x": { "value": 0.1, "tolerance": 0.01, "source": "game value", "confidence": "low" } } }
})";
}

bool Replace(std::string& s, const std::string& from, const std::string& to) {
    const std::size_t i = s.find(from);
    if (i == std::string::npos) return false;
    s.replace(i, from.size(), to);
    return true;
}

bool Rejects(const std::string& from, const std::string& to, std::string* why = nullptr) {
    std::string t = Mini();
    if (!Replace(t, from, to)) {
        std::printf("    test text does not contain '%s'\n", from.c_str());
        return false;
    }
    AirCombatDef d;
    std::string e;
    const bool ok = LoadAirCombatJson(t, d, &e);
    if (why != nullptr) *why = e;
    return !ok;
}

std::string ReadText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

// {registered source id: tier} from docs/SOURCES.md (tools/check_citations.py's reading).
std::map<std::string, std::string> RegisteredSources() {
    std::map<std::string, std::string> out;
    std::istringstream in(ReadText(kData + "/../docs/SOURCES.md"));
    std::string line;
    while (std::getline(in, line)) {
        if (line.size() < 6 || line.compare(0, 3, "| `") != 0) continue;
        const std::size_t e = line.find("` |", 3);
        if (e == std::string::npos) continue;
        const std::string id = line.substr(3, e - 3);
        const std::size_t t0 = e + 3;
        const std::size_t t1 = line.find('|', t0);
        if (t1 == std::string::npos) continue;
        std::string tier = line.substr(t0, t1 - t0);
        while (!tier.empty() && tier.front() == ' ') tier.erase(tier.begin());
        out[id] = tier;
    }
    return out;
}

// Every "source" string in a data file.
void CollectSources(const json::Value& v, std::vector<std::string>& out) {
    if (v.Is(json::Type::Object)) {
        for (std::size_t i = 0; i < v.keys.size(); ++i) {
            if (v.keys[i] == "source" && v.items[i].Is(json::Type::String)) {
                out.push_back(v.items[i].string);
            } else {
                CollectSources(v.items[i], out);
            }
        }
    } else if (v.Is(json::Type::Array)) {
        for (const json::Value& i : v.items) CollectSources(i, out);
    }
}

// How many of a file's source strings name no registered source (and no estimate, placeholder or game value), and how
// many cite tier 5: the rule of tools/check_citations.py.
void CheckSourcesOf(const std::string& rel, const std::map<std::string, std::string>& registered, int& unnamed, int& tier5) {
    json::Value root;
    std::string err;
    if (!json::Parse(ReadText(kData + "/" + rel), root, &err)) {
        std::printf("    %s: %s\n", rel.c_str(), err.c_str());
        ++unnamed;
        return;
    }
    std::vector<std::string> sources;
    CollectSources(root, sources);
    static const char* kOwn[] = {"sam3d", "estimate", "assumption", "placeholder", "docs/", "derived", "game value"};
    for (const std::string& s : sources) {
        std::string lower = s;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        bool named = false;
        bool five = false;
        std::string word;
        auto flush = [&]() {
            if (word.empty()) return;
            const auto it = registered.find(word);
            if (it != registered.end()) {
                named = true;
                five = five || (!it->second.empty() && it->second[0] == '5');
            }
            word.clear();
        };
        for (char c : lower) {
            const bool inside = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9' && !word.empty()) || (c == '_' && !word.empty());
            if (inside) {
                word += c;
            } else {
                flush();
            }
        }
        flush();
        bool own = false;
        for (const char* o : kOwn) own = own || lower.find(o) != std::string::npos;
        if (five) ++tier5;
        if (!named && !own) {
            std::printf("    %s: names no registered source: %s\n", rel.c_str(), s.substr(0, 80).c_str());
            ++unnamed;
        }
    }
}

}  // namespace

// ------------------------------------------------------------------------------------------------ the data

SC_TEST("air combat data: the files load, every missile, gun and fit is sourced and every game value named") {
    const AirCombatDef d = LoadReal();
    SC_EXPECT(d.aams.size() >= 15 && d.guns.size() >= 4 && d.fits.size() >= 14 && d.doctrines.size() == 5);
    static const char* kNumeric[] = {"max_head_m", "max_rear_m", "low_factor", "min_m", "launch_g", "seeker_cone_deg", "target_g",
                                     "sun_exclusion_deg", "clutter_below", "autonomous_s", "flare_rejection", "reliability",
                                     "design_pk", "lethal_m", "mean_mps"};
    for (const AamDef& a : d.aams) {
        SC_EXPECT(!a.source.empty() && !a.confidence.empty() && !a.name.empty());
        SC_EXPECT(a.max_rear_m > a.min_m && a.max_rear_m > 0.0);
        SC_EXPECT(a.in_service_year >= 1950 && a.in_service_year <= 1990);
        // The loader checked each named game value is a key of the entry; every value is then the source's or named.
        for (const std::string& g : a.game_values) {
            bool key = false;
            for (const char* k : kNumeric) key = key || g == k;
            SC_EXPECT(key);
        }
        // Values the source gives for the missile and the game values are disjoint, and a game value is never a year.
        SC_EXPECT(std::find(a.game_values.begin(), a.game_values.end(), std::string("in_service_year")) == a.game_values.end());
    }
    for (const AirGunDef& g : d.guns) SC_EXPECT(!g.source.empty() && !g.confidence.empty() && g.firing_s > 0.0 && g.kill_per_s > 0.0);
    for (const FitDef& f : d.fits) SC_EXPECT(!f.source.empty() && !f.confidence.empty());
    for (const AirDoctrineDef& dd : d.doctrines) {
        SC_EXPECT(!dd.source.empty() && !dd.shot_order.empty());
        SC_EXPECT(dd.strikers_jettison >= 0.0 && dd.strikers_jettison <= 1.0 && dd.strikers_abort <= 1.0);
    }
    // The sourced figures, from the research's table: the R-60 and the R-24R.
    const AamDef* r60 = d.Aam("r60");
    SC_EXPECT(r60 != nullptr);
    if (r60 != nullptr) {
        SC_EXPECT(r60->guidance == Guidance::IrRear && r60->in_service_year == 1973);
        SC_EXPECT_NEAR(r60->max_rear_m, 7200.0, 1e-9);
        SC_EXPECT_NEAR(r60->low_factor, 0.333, 1e-9);
        SC_EXPECT_NEAR(r60->min_m, 250.0, 1e-9);
        SC_EXPECT_NEAR(r60->launch_g, 7.0, 1e-9);
        SC_EXPECT_NEAR(r60->seeker_cone_rad, 12.0 * kDeg, 1e-9);
        SC_EXPECT_NEAR(r60->lethal_m, 2.5, 1e-9);
        SC_EXPECT_NEAR(r60->design_pk, 0.5, 1e-9);
    }
    const AamDef* r24r = d.Aam("r24r");
    SC_EXPECT(r24r != nullptr);
    if (r24r != nullptr) {
        SC_EXPECT(r24r->guidance == Guidance::SarhInertial);
        SC_EXPECT_NEAR(r24r->autonomous_s, 10.0, 1e-9);
        SC_EXPECT_NEAR(r24r->lethal_m, 10.0, 1e-9);
        SC_EXPECT_NEAR(r24r->min_m, 500.0, 1e-9);
        SC_EXPECT_NEAR(r24r->max_head_m, 32000.0, 1e-9);
        SC_EXPECT_NEAR(r24r->max_rear_m, 20000.0, 1e-9);
    }
    const AamDef* k13 = d.Aam("k13");
    SC_EXPECT(k13 != nullptr && k13->launch_g < 2.0 + 1e-9 && k13->sun_exclusion_rad > 29.9 * kDeg && k13->sun_exclusion_rad < 30.1 * kDeg);
    // The sensors.
    const SensorsDef s = LoadRealSensors();
    SC_EXPECT(s.sensors.size() >= 15 && !s.slots.empty());
    for (const SensorDef& x : s.sensors) {
        SC_EXPECT(x.in_service_year > 0 && !x.source.empty() && !x.carriers.empty());
        if (x.kind == SensorKind::Radar) SC_EXPECT(!x.radar_def.empty() && !x.library.empty());
    }
    // An entry whose year is unsourced is a slot: never a sensor of any war.
    for (const SensorSlotDef& sl : s.slots) SC_EXPECT(s.Find(sl.id) == nullptr && !sl.needs.empty());
}

SC_TEST("air combat data: the loaders are strict and read what they are given") {
    AirCombatDef d;
    std::string e;
    SC_EXPECT(LoadAirCombatJson(Mini(), d, &e));
    if (e.size() > 0) std::printf("    %s\n", e.c_str());
    // A round trip of the values.
    SC_EXPECT(d.aams.size() == 2 && d.Aam("m1") != nullptr && d.Aam("m2") != nullptr);
    SC_EXPECT_NEAR(d.Aam("m1")->max_rear_m, 5000.0, 1e-9);
    SC_EXPECT(d.Aam("m2")->guidance == Guidance::SarhInertial && d.Aam("m2")->allies_year == 1985);
    SC_EXPECT_NEAR(d.calibration.chain.guides_sarh, 0.6, 1e-12);
    SC_EXPECT_NEAR(d.calibration.chain.guides_ir_rear, ChainParams().guides_ir_rear, 1e-12);  // not named: the default
    SC_EXPECT_NEAR(d.calibration.mix.skill, 0.4, 1e-12);
    SC_EXPECT(d.Fit("t1a") == d.Fit("t1") && d.Fit("t1") != nullptr);  // an alias finds the fit
    SC_EXPECT(d.calibration.Target("x") != nullptr && d.calibration.Target("y") == nullptr);
    SC_EXPECT(d.Doctrine("nato") != nullptr && d.Doctrine("neutral") == nullptr);
    // Strictness: each of these is refused, with a reason.
    std::string why;
    SC_EXPECT(Rejects("\"in_service_year\": 1970,", "\"in_service_year\": 1970, \"range_km\": 9,", &why));  // unknown key
    SC_EXPECT(why.find("range_km") != std::string::npos);
    SC_EXPECT(Rejects("\"guidance\": \"ir_rear\"", "\"guidance\": \"active_radar\""));  // not a class of the data
    SC_EXPECT(Rejects("\"reliability\": 0.8", "\"reliability\": 1.5"));                  // out of range
    SC_EXPECT(Rejects("\"game_values\": [\"max_rear_m\"]", "\"game_values\": [\"colour\"]"));  // names a key the entry has not
    SC_EXPECT(Rejects("\"id\": \"m2\"", "\"id\": \"m1\""));                              // duplicate id
    SC_EXPECT(Rejects("\"max_head_m\": 20000,", ""));                                    // a radar round needs a head-on range
    SC_EXPECT(Rejects("\"autonomous_s\": 10,", ""));                                     // sarh_inertial without its inertial flight
    SC_EXPECT(Rejects("\"allies_year\": 1985", "\"allies_year\": 1975"));                // the allies cannot be first
    SC_EXPECT(Rejects("\"in_service_year\": 1970, \"max_rear_m\": 5000", "\"in_service_year\": 1970, \"max_head_m\": 900, \"max_rear_m\": 5000"));  // rear-aspect, head-on zone
    SC_EXPECT(Rejects("[[\"m1\", 2]], \"gun\": \"g1\" },", "[[\"m9\", 2]], \"gun\": \"g1\" },"));  // a fit's unknown missile
    SC_EXPECT(Rejects("\"gun\": \"g1\" },\n", "\"gun\": \"nope\" },\n"));                // ... and gun
    SC_EXPECT(Rejects("\"from\": 1984", "\"from\": 1970"));                              // eras in rising order
    SC_EXPECT(Rejects("\"iff\": \"none\"", "\"iff\": \"mk15\""));                        // an IFF that is not 1985's
    SC_EXPECT(Rejects("\"roe\": \"vid\"", "\"roe\": \"free\""));                         // a rule the layer does not know
    SC_EXPECT(Rejects("\"shot_order\": [\"ir\", \"gun\"]", "\"shot_order\": [\"ir\", \"laser\"]"));
    SC_EXPECT(Rejects("\"game_values\": [\"max_rear_m\"], \"source\": \"game value\", \"confidence\": \"low\" }", "\"game_values\": [\"max_rear_m\"] }"));  // a missile with no source
    SC_EXPECT(Rejects("\"guides_sarh\": 0.6", "\"guides_radar\": 0.6"));                 // a chain number the code does not read
    SC_EXPECT(Rejects("\"skill\": 0.4", "\"skill\": 4"));
}

SC_TEST("air combat data: a sensor with no year is refused, a radar names its EM radar, a slot is not a sensor") {
    const char* ok = R"({ "sensors_id": "t", "status": "s", "source": "game value", "confidence": "low",
      "sensors": [ { "id": "r1", "name": "R1", "kind": "radar", "carriers": ["a"], "in_service_year": 1975, "radar_def": "x", "library": "em/x.json",
                     "by_hand_m": 40000, "source": "game value", "confidence": "low" },
                   { "id": "i1", "name": "I1", "kind": "irst", "carriers": ["a"], "in_service_year": 1980, "allies_year": 1984, "rear_m": 20000,
                     "source": "game value", "confidence": "low" } ],
      "slots": [ { "id": "s1", "name": "S1", "kind": "rwr", "carriers": ["a"], "needs": "a year" } ] })";
    SensorsDef s;
    std::string e;
    SC_EXPECT(LoadSensorsJson(ok, s, &e));
    SC_EXPECT(s.sensors.size() == 2 && s.slots.size() == 1);
    SC_EXPECT(s.Of("a", SensorKind::Irst, 1982, false).size() == 1 && s.Of("a", SensorKind::Irst, 1982, true).empty());  // the allies' year
    SC_EXPECT(s.Of("a", SensorKind::Irst, 1984, true).size() == 1 && s.Of("a", SensorKind::Irst, 1979, false).empty());
    SC_EXPECT(s.Of("b", SensorKind::Radar, 1985, false).empty());
    const auto bad = [&](const std::string& from, const std::string& to) {
        std::string t = ok;
        if (!Replace(t, from, to)) return false;
        SensorsDef x;
        return !LoadSensorsJson(t, x, nullptr);
    };
    SC_EXPECT(bad("\"in_service_year\": 1975", "\"in_service_year\": 0"));                  // no year: a slot, not a sensor
    SC_EXPECT(bad("\"radar_def\": \"x\", ", ""));                                           // a radar with no EM radar
    SC_EXPECT(bad("\"rear_m\": 20000,", "\"rear_m\": 20000, \"radar_def\": \"x\", \"library\": \"y\","));  // only a radar has one
    SC_EXPECT(bad("\"id\": \"s1\"", "\"id\": \"i1\""));                                      // a slot that is also a sensor
    SC_EXPECT(bad("\"kind\": \"irst\"", "\"kind\": \"magic\""));
    SC_EXPECT(bad("\"allies_year\": 1984", "\"allies_year\": 1970"));
}

SC_TEST("air combat data: AirWarData loads both files for every faction, and every fighter has a fit") {
    static const char* kFactions[] = {"rebels", "neutral", "warsaw_pact", "nato"};
    for (const char* f : kFactions) {
        AirWarData d;
        std::string e;
        const bool ok = LoadAirWarData(kData, "air_war/theatre_pannonia.json", std::string("air_war/faction_") + f + ".json", d, &e);
        if (!ok) std::printf("    %s: %s\n", f, e.c_str());
        SC_EXPECT(ok);
        if (!ok) continue;
        SC_EXPECT(!d.air_combat.aams.empty() && !d.sensors.sensors.empty());
        SC_EXPECT(d.air_combat.Doctrine(f) != nullptr);
        // Every aircraft type has a fit (roadmap B8 retired `air_to_air`: a fighter's worth in the air is its fit), and a type
        // that can escort carries a gun or a missile in the war's year.
        for (const AircraftType& a : d.faction.aircraft) {
            if (a.missile || a.drone) continue;
            const bool has = d.air_combat.Fit(a.id) != nullptr;
            if (!has) std::printf("    %s: %s has no fit\n", f, a.id.c_str());
            SC_EXPECT(has);
            if (a.Has(FlightRole::Escort) && a.InService(d.theatre.year)) {
                const LoadOut lo = d.air_combat.LoadOutFor(a.id, d.theatre.year, false);
                if (lo.Empty()) std::printf("    %s: escort %s carries nothing in %d\n", f, a.id.c_str(), d.theatre.year);
                SC_EXPECT(!lo.Empty());
            }
        }
    }
    // Our side's types by the order of battle's and the task's names.
    const AirCombatDef d = LoadReal();
    SC_EXPECT(d.Fit("mig21mf") != nullptr && d.Fit("mig21bis") != nullptr && d.Fit("mig23ml") != nullptr && d.Fit("mig21") == d.Fit("mig21mf"));
}

// ------------------------------------------------------------------------------------------------ the period gate

SC_TEST("air combat gate: a 1985 war offers no weapon, gun or sensor from a later one") {
    const AirCombatDef d = LoadReal();
    // The later war's missiles are in the data with their real years and carried by no 1985 fit.
    const AamDef* r27r = d.Aam("r27r");
    const AamDef* r73 = d.Aam("r73");
    SC_EXPECT(r27r != nullptr && r73 != nullptr && d.Aam("r27t") != nullptr);
    if (r27r == nullptr || r73 == nullptr) return;
    SC_EXPECT(!r27r->InService(1985, false) && !r27r->InService(1985, true) && !d.Aam("r27t")->InService(1985, false));
    SC_EXPECT(r73->InService(1985, false));   // the Soviet round, adopted in 1984 ...
    SC_EXPECT(!r73->InService(1985, true));   // ... reaches the Pact's allies and our side in 1988
    SC_EXPECT(!r73->InService(1987, true) && r73->InService(1988, true));
    // The AMRAAM has no guidance class in the data and is absent under every id it goes by.
    SC_EXPECT(d.Aam("aim120") == nullptr && d.Aam("aim120a") == nullptr && d.Aam("amraam") == nullptr);
    // Every missile and gun of every fit, in a war of any year from 1970, was in service that year for the shooter's side.
    for (int year = 1970; year <= 1992; ++year) {
        for (const FitDef& f : d.fits) {
            for (bool allied : {false, true}) {
                const LoadOut out = d.LoadOutFor(f.id, year, allied);
                for (const auto& a : out.aams) SC_EXPECT(a.first->InService(year, allied));
                if (out.gun != nullptr) SC_EXPECT(out.gun->InService(year));
                if (year <= 1985) {
                    for (const auto& a : out.aams) {
                        SC_EXPECT(a.first->id != "r27r" && a.first->id != "r27t");
                        SC_EXPECT(!(allied && a.first->id == "r73"));
                    }
                }
            }
        }
    }
    // The Pact's allies get a missile only from allies_year: our MiG-21's R-60M from 1984, the Soviet's earlier.
    SC_EXPECT(d.Aam("r60m")->allies_year == 1984 && d.Aam("r60m")->in_service_year < 1984);
    SC_EXPECT(d.LoadOutFor("mig21mf", 1983, true).Carries("r60") && !d.LoadOutFor("mig21mf", 1983, true).Carries("r60m"));
    SC_EXPECT(d.LoadOutFor("mig21mf", 1985, true).Carries("r60m") && !d.LoadOutFor("mig21mf", 1985, true).Carries("r60"));
    // The R-24 is the MiG-23ML's from the decree of April 1981; before it the type carries nothing.
    SC_EXPECT(d.LoadOutFor("mig23ml", 1980, false).Empty() && d.LoadOutFor("mig23ml", 1985, false).Carries("r24r"));
    SC_EXPECT(d.LoadOutFor("mig23ml", 1985, true).Carries("r24r"));
    // The MiG-23MLD is a 1984 aircraft; its load follows it.
    SC_EXPECT(d.LoadOutFor("mig23mld", 1983, false).Empty() && d.LoadOutFor("mig23mld", 1985, false).Carries("r24r"));
    // The F-16A carries no radar missile, in any year the data goes to.
    for (int year = 1975; year <= 1992; ++year) SC_EXPECT(d.LoadOutFor("f16a", year, false).SemiActive() == 0);
    SC_EXPECT(d.LoadOutFor("f16a", 1985, false).Carries("aim9m") && d.LoadOutFor("f16a", 1985, false).gun != nullptr);
    // The F-15C: the AIM-9M only from its 1983 deliveries.
    SC_EXPECT(d.LoadOutFor("f15c", 1982, false).Carries("aim9l") && !d.LoadOutFor("f15c", 1982, false).Carries("aim9m"));
    SC_EXPECT(d.LoadOutFor("f15c", 1985, false).Carries("aim7m") && d.LoadOutFor("f15c", 1985, false).Carries("aim9m"));
    // The neutral's Draken is a 1988 aircraft with guns only; the Saab 105 is its 1985 jet.
    SC_EXPECT(d.LoadOutFor("draken_gun", 1985, false).Empty() && !d.LoadOutFor("draken_gun", 1989, false).Empty());
    SC_EXPECT(d.LoadOutFor("saab105", 1985, false).aams.empty() && d.LoadOutFor("saab105", 1985, false).gun != nullptr);
    SC_EXPECT(d.LoadOutFor("galeb", 1985, false).aams.empty());
    // An unknown type carries nothing.
    SC_EXPECT(d.LoadOutFor("no_such_type", 1985, false).Empty());
    // Sensors: the MLD's radar and flares are 1984's.
    const SensorsDef s = LoadRealSensors();
    SC_EXPECT(s.Of("mig23mld", SensorKind::Radar, 1983, false).empty() && s.Of("mig23mld", SensorKind::Radar, 1985, false).size() == 1);
    SC_EXPECT(s.Of("mig23mld", SensorKind::Countermeasure, 1985, false).size() == 1 && s.Of("mig23ml", SensorKind::Countermeasure, 1985, false).empty());
    SC_EXPECT(s.Of("mig23ml", SensorKind::Radar, 1985, true).size() == 1 && s.Of("mig23ml", SensorKind::Irst, 1985, true).size() == 1);
    for (const SensorDef& x : s.sensors) SC_EXPECT(x.InService(1985, true) && x.InService(1985, false));  // nothing in the data is later than the war
    // The unsourced are slots, in no war.
    SC_EXPECT(s.Find("spo15l") == nullptr && s.Find("pave_tack") == nullptr);
}

// ------------------------------------------------------------------------------------------------ the chain

SC_TEST("air combat chain: the envelope, from the missile's data") {
    const ChainParams c = GenericChain();
    const AamDef ir = GenericIr();
    // Maximum range: aspect blends the rear maximum (7,000 m) to none head-on; height blends the low factor to 1 at 10,000 m.
    SC_EXPECT_NEAR(MaxRangeM(ir, c, 0.0, 10000.0), 7000.0, 1e-9);
    SC_EXPECT_NEAR(MaxRangeM(ir, c, 0.0, 0.0), 7000.0 * 0.4, 1e-9);
    SC_EXPECT_NEAR(MaxRangeM(ir, c, 0.0, 5000.0), 7000.0 * (0.4 + 0.6 * 0.5), 1e-9);
    SC_EXPECT_NEAR(MaxRangeM(ir, c, 90.0 * kDeg, 10000.0), 3500.0, 1e-6);  // half way round
    SC_EXPECT_NEAR(MaxRangeM(ir, c, 0.0, 99999.0), 7000.0, 1e-9);        // no more than the maximum above the reference height
    const AamDef sarh = GenericSarh();
    SC_EXPECT_NEAR(MaxRangeM(sarh, c, 180.0 * kDeg, 10000.0), 30000.0, 1e-9);
    SC_EXPECT_NEAR(MaxRangeM(sarh, c, 90.0 * kDeg, 10000.0), 21000.0, 1e-9);
    // The checks and their reasons.
    ShotGeometry g = RearShot(3000.0);
    SC_EXPECT(CheckEnvelope(ir, c, g).ok);
    g.range_m = 200.0;
    SC_EXPECT(CheckEnvelope(ir, c, g).reason == MissReason::TooClose);
    g.range_m = 8000.0;
    SC_EXPECT(CheckEnvelope(ir, c, g).reason == MissReason::TooFar);
    g = RearShot(3000.0);
    g.launch_g = 6.5;  // the R-60's seven g: more than the missile allows
    SC_EXPECT(CheckEnvelope(ir, c, g).reason == MissReason::LaunchG);
    g = RearShot(3000.0);
    g.off_boresight_rad = 15.0 * kDeg;
    SC_EXPECT(CheckEnvelope(ir, c, g).reason == MissReason::OffBoresight);
    // A rear-aspect round refuses a head-on shot; a radar round takes it.
    g = RearShot(3000.0);
    g.aspect_rad = 180.0 * kDeg;
    SC_EXPECT(CheckEnvelope(ir, c, g).reason == MissReason::Aspect);
    SC_EXPECT(CheckEnvelope(sarh, c, g).ok);
    ShotLinks refused = ShotOdds(ir, c, g, ShotConditions(), Pilot());
    SC_EXPECT(refused.in_envelope == 0.0 && refused.Product() == 0.0);
    ShotResult r = ResolveShot(ir, c, g, ShotConditions(), Pilot(), 5, 1, 1);
    SC_EXPECT(!r.kill && r.reason == MissReason::Aspect);
}

SC_TEST("air combat chain: the links equal a hand calculation") {
    const ChainParams c = GenericChain();
    const AamDef ir = GenericIr();
    Pilot p;
    p.skill = 0.5;
    p.stress = 0.0;
    p.mode_change = false;
    ShotGeometry g = RearShot(3000.0);
    g.aspect_rad = 20.0 * kDeg;
    ShotConditions k;
    const ShotLinks l = ShotOdds(ir, c, g, k, p);
    // Works: reliability 0.8 x (1 - switch error); the error is 0.1 x 0.2 (no change of mode) x (1 - 0.5 x 0.5) x 1 = 0.015.
    SC_EXPECT_NEAR(l.switch_error, 0.015, 1e-12);
    SC_EXPECT_NEAR(l.works, 0.8 * (1.0 - 0.015), 1e-12);
    // Guides: base 0.8 x aspect (1 - 0.5 x 20/90) with no sun, ground, countermeasure, break or lock effect.
    SC_EXPECT_NEAR(l.guides, 0.8 * (1.0 - 0.5 * (20.0 / 90.0)), 1e-12);
    // Lethal: the design figure 0.5 with no break.
    SC_EXPECT_NEAR(l.lethal, 0.5, 1e-12);
    SC_EXPECT_NEAR(l.Product(), 0.8 * (1.0 - 0.015) * 0.8 * (1.0 - 0.5 * (20.0 / 90.0)) * 0.5, 1e-12);
    // A change of mode and a stressed pilot: 0.1 x 1 x (1 - 0.5 x 0.5) x (1 + 1 x 0.6) = 0.12.
    p.mode_change = true;
    p.stress = 0.6;
    SC_EXPECT_NEAR(SwitchErrorChance(c, p), 0.12, 1e-12);
    p = Pilot();
    // The ground behind the target halves a radar round in the beam and takes 0.6 of it elsewhere.
    const AamDef sarh = GenericSarh();
    ShotGeometry gs = RearShot(8000.0);
    gs.aspect_rad = 180.0 * kDeg;
    gs.look_down = true;
    SC_EXPECT_NEAR(ShotOdds(sarh, c, gs, k, p).guides, 0.7 * 0.6, 1e-12);
    gs.aspect_rad = 90.0 * kDeg;
    SC_EXPECT_NEAR(ShotOdds(sarh, c, gs, k, p).guides, 0.7 * 0.6 * 0.5, 1e-12);
    // A countermeasure takes its share less what the seeker sees through (0.25): 0.8 x (1 - 0.6 x 0.75) at 0.6.
    ShotConditions flares;
    flares.countermeasures = 0.6;
    SC_EXPECT_NEAR(ShotOdds(ir, c, RearShot(3000.0), flares, Pilot()).guides, 0.8 * (1.0 - 0.6 * 0.75), 1e-12);
    // A break: effectiveness = timing x min(1, load / the missile's limit) = 0.5 x 0.75, guides x (1 - 0.6 x that).
    ShotConditions brk;
    brk.break_timing = 0.5;
    brk.target_g = 6.0;
    const double eff = 0.5 * (6.0 / 8.0);
    const ShotLinks lb = ShotOdds(ir, c, RearShot(3000.0), brk, Pilot());
    SC_EXPECT_NEAR(lb.guides, 0.8 * (1.0 - 0.6 * eff), 1e-12);
    // ... and the miss it adds against the lethal radius: sigma 1.5 m nominal, sqrt(1.5^2 + (6 x eff)^2) with the break.
    const double sn = 1.5;
    const double sb = std::sqrt(sn * sn + (6.0 * eff) * (6.0 * eff));
    const double hn = 1.0 - std::exp(-2.5 * 2.5 / (2.0 * sn * sn));
    const double hb = 1.0 - std::exp(-2.5 * 2.5 / (2.0 * sb * sb));
    SC_EXPECT_NEAR(lb.lethal, 0.5 * hb / hn, 1e-12);
    SC_EXPECT(lb.lethal < 0.5);
    // A big warhead loses less to the same break than a small one.
    AamDef big = ir;
    big.lethal_m = 10.0;
    SC_EXPECT(ShotOdds(big, c, RearShot(3000.0), brk, Pilot()).lethal / 0.5 > lb.lethal / 0.5);
    // A full break at the missile's limit, perfectly timed: the R-60's 2.5 m cost against the nominal 1.5 m miss.
    ShotConditions full;
    full.break_timing = 1.0;
    full.target_g = 8.0;
    const double sf = std::sqrt(1.5 * 1.5 + 36.0);
    SC_EXPECT_NEAR(ShotOdds(ir, c, RearShot(3000.0), full, Pilot()).lethal, 0.5 * (1.0 - std::exp(-6.25 / (2.0 * sf * sf))) / hn, 1e-12);
    // The ceiling holds: no launch beats the design figure.
    SC_EXPECT(l.lethal <= ir.design_pk + 1e-12);
}

SC_TEST("air combat chain: the sun, the lock and the seeker's class") {
    const ChainParams c = GenericChain();
    // A K-13-like round: no guidance within 30 degrees of the sun, a degraded seeker to 45.
    AamDef k13 = GenericIr();
    k13.sun_exclusion_rad = 30.0 * kDeg;
    ShotGeometry g = RearShot(3000.0);
    g.sun_angle_rad = 20.0 * kDeg;
    const ShotLinks in = ShotOdds(k13, c, g, ShotConditions(), Pilot());
    SC_EXPECT(in.guides == 0.0 && in.Product() == 0.0);
    int kills = 0;
    MissReason why = MissReason::None;
    for (std::uint64_t i = 0; i < 400; ++i) {
        const ShotResult r = ResolveShot(k13, c, g, ShotConditions(), Pilot(), 3, i, 0);
        kills += r.kill ? 1 : 0;
        if (!r.kill && r.reason != MissReason::Dud && r.reason != MissReason::SwitchError) why = r.reason;
    }
    SC_EXPECT(kills == 0 && why == MissReason::Sun);
    g.sun_angle_rad = 40.0 * kDeg;  // between 30 and 45: the soft edge, linear from the floor 0.2 to 1
    const double f = 0.2 + 0.8 * (40.0 - 30.0) / (45.0 - 30.0);
    SC_EXPECT_NEAR(ShotOdds(k13, c, g, ShotConditions(), Pilot()).guides, 0.8 * (1.0 - 0.5 * 0.0) * f, 1e-12);
    g.sun_angle_rad = 60.0 * kDeg;
    SC_EXPECT_NEAR(ShotOdds(k13, c, g, ShotConditions(), Pilot()).guides, 0.8, 1e-12);
    // A round with no sun limit (the exclusion 0) ignores the sun.
    AamDef sunless = GenericIr();
    sunless.sun_exclusion_rad = 0.0;
    g.sun_angle_rad = 5.0 * kDeg;
    SC_EXPECT_NEAR(ShotOdds(sunless, c, g, ShotConditions(), Pilot()).guides, 0.8, 1e-12);
    // A semi-active round fails when the shooter breaks lock before impact; one with inertial flight survives a short loss.
    AamDef sparrow = GenericSarh();
    AamDef r24 = GenericSarh();
    r24.guidance = Guidance::SarhInertial;
    r24.autonomous_s = 10.0;
    ShotGeometry gs = RearShot(8000.0);
    gs.aspect_rad = 180.0 * kDeg;
    ShotConditions held;
    ShotConditions lost;
    lost.lock_lost_before_impact_s = 8.0;
    SC_EXPECT(ShotOdds(sparrow, c, gs, held, Pilot()).guides > 0.0);
    SC_EXPECT(ShotOdds(sparrow, c, gs, lost, Pilot()).guides == 0.0);          // the Sparrow needs the lock to impact
    SC_EXPECT(ShotOdds(r24, c, gs, lost, Pilot()).guides > 0.0);               // the R-24R flew 10 s on its own
    SC_EXPECT_NEAR(ShotOdds(r24, c, gs, lost, Pilot()).guides, ShotOdds(r24, c, gs, held, Pilot()).guides, 1e-12);
    lost.lock_lost_before_impact_s = 12.0;
    SC_EXPECT(ShotOdds(r24, c, gs, lost, Pilot()).guides == 0.0);              // ... but not 12 s
    MissReason lock_why = MissReason::None;
    for (std::uint64_t i = 0; i < 300; ++i) {
        const ShotResult r = ResolveShot(sparrow, c, gs, lost, Pilot(), 9, i, 0);
        SC_EXPECT(!r.kill);
        if (r.reason != MissReason::Dud && r.reason != MissReason::SwitchError) lock_why = r.reason;
    }
    SC_EXPECT(lock_why == MissReason::LockLost);
    // An infrared round does not care about the launcher's lock.
    SC_EXPECT(ShotOdds(k13, c, RearShot(3000.0), lost, Pilot()).guides > 0.0);
    // The all-aspect seeker sees head-on at 0.85 of its rear value (the chain's 0.8 here).
    AamDef all = GenericIr();
    all.guidance = Guidance::IrAllAspect;
    all.max_head_m = 3000.0;
    ShotGeometry gh = RearShot(2000.0);
    gh.aspect_rad = 180.0 * kDeg;
    SC_EXPECT_NEAR(ShotOdds(all, c, gh, ShotConditions(), Pilot()).guides, 0.9 * 0.8, 1e-12);
    // ... and a rear-aspect one cannot be fired from there at all.
    SC_EXPECT(!CheckEnvelope(GenericIr(), c, gh).ok);
}

SC_TEST("air combat chain: a shot is keyed, order-independent, and its expectation is the product of its links") {
    const ChainParams c = GenericChain();
    const AamDef ir = GenericIr();
    ShotGeometry g = RearShot(3000.0);
    g.aspect_rad = 30.0 * kDeg;
    ShotConditions k;
    k.break_timing = 0.7;
    k.target_g = 5.0;
    Pilot p;
    p.skill = 0.6;
    p.mode_change = true;
    // The same keys, the same answer; any order.
    std::vector<ShotResult> forward, backward;
    for (std::uint64_t i = 0; i < 50; ++i) forward.push_back(ResolveShot(ir, c, g, k, p, 11, 4, i));
    for (std::uint64_t i = 50; i-- > 0;) backward.push_back(ResolveShot(ir, c, g, k, p, 11, 4, i));
    for (std::size_t i = 0; i < 50; ++i) {
        SC_EXPECT(forward[i].kill == backward[49 - i].kill && forward[i].reason == backward[49 - i].reason);
    }
    // A different seed or shooter changes some of them.
    int differ = 0;
    for (std::uint64_t i = 0; i < 50; ++i) differ += ResolveShot(ir, c, g, k, p, 12, 4, i).reason != forward[i].reason ? 1 : 0;
    SC_EXPECT(differ > 0);
    // The expectation over many keyed draws is the product of the links.
    const double expect = ShotOdds(ir, c, g, k, p).Product();
    const int n = 40000;
    int kills = 0;
    for (int i = 0; i < n; ++i) kills += ResolveShot(ir, c, g, k, p, 21, 77, static_cast<std::uint64_t>(i)).kill ? 1 : 0;
    const double sigma = std::sqrt(expect * (1.0 - expect) / n);
    SC_EXPECT_NEAR(static_cast<double>(kills) / n, expect, 4.0 * sigma);
    // The reasons of the misses add up to the misses.
    int counted = 0;
    int missed = 0;
    for (int i = 0; i < 2000; ++i) {
        const ShotResult r = ResolveShot(ir, c, g, k, p, 31, 5, static_cast<std::uint64_t>(i));
        missed += r.kill ? 0 : 1;
        counted += (!r.kill && r.reason != MissReason::None) ? 1 : 0;
        SC_EXPECT(r.kill == (r.reason == MissReason::None));
    }
    SC_EXPECT(counted == missed);
}

SC_TEST("air combat chain: skill and stress move the judgement and the switch") {
    const ChainParams c = GenericChain();
    const AamDef ir = GenericIr();
    Pilot green;
    green.skill = 0.0;
    green.stress = 0.8;
    Pilot ace;
    ace.skill = 1.0;
    ace.stress = 0.0;
    SC_EXPECT(JudgementScale(c, green, Guidance::IrRear) > 4.0 * JudgementScale(c, ace, Guidance::IrRear));
    SC_EXPECT_NEAR(JudgementScale(c, ace, Guidance::IrRear), 0.2, 1e-12);                        // 1 - 0.8 x 1, no stress
    SC_EXPECT_NEAR(JudgementScale(c, ace, Guidance::Sarh), 0.4, 1e-12);                          // a radar round asks twice as much
    SC_EXPECT(SwitchErrorChance(c, green) > SwitchErrorChance(c, ace));
    // A belief centred on the zone's edge: the believing pilot is outside it far more often when green.
    ShotGeometry truth = RearShot(1000.0);
    ShotGeometry edge = RearShot(300.0);  // the minimum range
    int green_out = 0, ace_out = 0;
    const int n = 4000;
    for (int i = 0; i < n; ++i) {
        const ShotGeometry bg = Believe(edge, c, green, ir.guidance, 5, 1, static_cast<std::uint64_t>(i));
        const ShotGeometry ba = Believe(edge, c, ace, ir.guidance, 5, 2, static_cast<std::uint64_t>(i));
        // He fires if he believes he is inside; the truth is on the edge: half of those who believe so are not.
        green_out += PilotFires(ir, c, bg) ? 1 : 0;
        ace_out += PilotFires(ir, c, ba) ? 1 : 0;
    }
    SC_EXPECT(green_out > 0 && ace_out > 0);
    // The believed range is unbiased in the log: its median is the truth.
    std::vector<double> believed;
    for (int i = 0; i < 2001; ++i) believed.push_back(Believe(truth, c, Pilot(), ir.guidance, 8, 3, static_cast<std::uint64_t>(i)).range_m);
    std::sort(believed.begin(), believed.end());
    SC_EXPECT_NEAR(believed[1000], 1000.0, 60.0);
    // The expectation of being inside: nearly 1 well inside the zone, about a half on the edge, falling with the spread.
    ShotGeometry in = RearShot(3000.0);
    SC_EXPECT(EnvelopeProbability(ir, c, in, ace) > 0.97);
    const double on_edge = EnvelopeProbability(ir, c, edge, ace);
    SC_EXPECT(on_edge > 0.3 && on_edge < 0.7);
    in.range_m = 6800.0;  // inside the reference-height maximum 7,000 m, near it: the green pilot is more often wrong
    SC_EXPECT(EnvelopeProbability(ir, c, in, ace) > EnvelopeProbability(ir, c, in, green));
    // The reference mix's pilots of 1967 misjudge more than a pilot of skill one.
    AirCombatDef d = LoadReal();
    const AamDef* aim9b = d.Aam("aim9b");
    SC_EXPECT(aim9b != nullptr);
    if (aim9b == nullptr) return;
    Pilot veteran = Pilot();
    veteran.skill = 1.0;
    veteran.stress = 0.0;
    Pilot of67;
    of67.skill = d.calibration.mix.skill;
    of67.stress = d.calibration.mix.stress;
    const LaunchStats a = SimulateLaunches(*aim9b, d.calibration, of67, 6000, 3);
    const LaunchStats b = SimulateLaunches(*aim9b, d.calibration, veteran, 6000, 3);
    std::printf("    AIM-9B out of the envelope: skill %.1f %.1f %%, skill 1 %.1f %%; hits %.1f %% and %.1f %%\n", d.calibration.mix.skill,
                100.0 * a.OutOfEnvelopeShare(), 100.0 * b.OutOfEnvelopeShare(), 100.0 * a.HitRate(), 100.0 * b.HitRate());
    SC_EXPECT(b.OutOfEnvelopeShare() < a.OutOfEnvelopeShare() && b.HitRate() > a.HitRate());
}

SC_TEST("air combat chain: a gun pass is a firing window, not rounds") {
    ChainParams c = GenericChain();
    c.gun_skill_floor = 0.4;
    c.gun_range_edge = 0.5;
    c.gun_g_loss = 0.4;
    c.gun_g_full = 8.0;
    AirGunDef gun;
    gun.id = "test_gun";
    gun.firing_s = 4.0;
    gun.effective_m = 1000.0;
    gun.kill_per_s = 0.2;
    Pilot p;
    p.skill = 0.5;
    GunPass pass;
    pass.range_m = 500.0;
    pass.window_s = 3.0;
    pass.target_g = 1.0;
    // 1 - exp(-0.2 x 3 s x skill (0.4 + 0.6 x 0.5 = 0.7) x range (1 - 0.5 x 0.5 = 0.75) x 1).
    SC_EXPECT_NEAR(GunKillChance(gun, c, pass, p), 1.0 - std::exp(-0.2 * 3.0 * 0.7 * 0.75), 1e-12);
    // The magazine caps the window: 10 s in range is 4 s of fire.
    pass.window_s = 10.0;
    SC_EXPECT_NEAR(GunKillChance(gun, c, pass, p), 1.0 - std::exp(-0.2 * 4.0 * 0.7 * 0.75), 1e-12);
    // Beyond the effective range, or with no window, nothing.
    pass.range_m = 1200.0;
    SC_EXPECT(GunKillChance(gun, c, pass, p) == 0.0);
    GunResult far = ResolveGunPass(gun, c, pass, p, 1, 1, 1);
    SC_EXPECT(!far.kill && far.reason == MissReason::TooFar);
    pass.range_m = 500.0;
    pass.window_s = 0.0;
    SC_EXPECT(ResolveGunPass(gun, c, pass, p, 1, 1, 1).reason == MissReason::TooClose);
    // A manoeuvring target costs aim: 8 g takes 0.4 off; 4.5 g takes 0.4 x 3.5/7 = 0.2.
    pass.window_s = 3.0;
    pass.target_g = 8.0;
    SC_EXPECT_NEAR(GunKillChance(gun, c, pass, p), 1.0 - std::exp(-0.2 * 3.0 * 0.7 * 0.75 * 0.6), 1e-12);
    pass.target_g = 4.5;
    SC_EXPECT_NEAR(GunKillChance(gun, c, pass, p), 1.0 - std::exp(-0.2 * 3.0 * 0.7 * 0.75 * 0.8), 1e-12);
    // Skill, range and window all raise it.
    pass.target_g = 1.0;
    Pilot ace;
    ace.skill = 1.0;
    GunPass near_pass = pass;
    near_pass.range_m = 200.0;
    SC_EXPECT(GunKillChance(gun, c, pass, ace) > GunKillChance(gun, c, pass, p) && GunKillChance(gun, c, near_pass, p) > GunKillChance(gun, c, pass, p));
    // Keyed: the expectation over draws is the chance.
    const double chance = GunKillChance(gun, c, pass, p);
    int kills = 0;
    const int n = 20000;
    for (int i = 0; i < n; ++i) kills += ResolveGunPass(gun, c, pass, p, 17, 2, static_cast<std::uint64_t>(i)).kill ? 1 : 0;
    SC_EXPECT_NEAR(static_cast<double>(kills) / n, chance, 4.0 * std::sqrt(chance * (1.0 - chance) / n));
}

// ------------------------------------------------------------------------------------------------ calibration

SC_TEST("air combat calibration: the chain reproduces the 1967 rates (checo_1967_a2a p. 44)") {
    const AirCombatDef d = LoadReal();
    const AamDef* aim7 = d.Aam("aim7e");
    const AamDef* aim9 = d.Aam("aim9b");
    SC_EXPECT(aim7 != nullptr && aim9 != nullptr);
    if (aim7 == nullptr || aim9 == nullptr) return;
    const CalibrationTarget* hit7 = d.calibration.Target("aim7_hit_per_launch");
    const CalibrationTarget* hit9 = d.calibration.Target("aim9_hit_per_launch");
    const CalibrationTarget* out7 = d.calibration.Target("aim7_out_of_params");
    const CalibrationTarget* out9 = d.calibration.Target("aim9_out_of_params");
    const CalibrationTarget* rel = d.calibration.Target("reliability_1972");
    SC_EXPECT(hit7 != nullptr && hit9 != nullptr && out7 != nullptr && out9 != nullptr && rel != nullptr);
    if (hit7 == nullptr || hit9 == nullptr || out7 == nullptr || out9 == nullptr || rel == nullptr) return;
    SC_EXPECT_NEAR(hit7->value, 0.111, 1e-12);
    SC_EXPECT_NEAR(hit9->value, 0.186, 1e-12);
    SC_EXPECT_NEAR(out7->value, 0.35, 1e-12);
    SC_EXPECT_NEAR(out9->value, 0.26, 1e-12);
    Pilot of67;
    of67.skill = d.calibration.mix.skill;
    of67.stress = d.calibration.mix.stress;
    const int n = 60000;
    const LaunchStats s7 = SimulateLaunches(*aim7, d.calibration, of67, n, 7);
    const LaunchStats s9 = SimulateLaunches(*aim9, d.calibration, of67, n, 7);
    std::printf("    1967 crews, per launch: AIM-7 hits %.1f %% (target 11.1), out of parameters %.1f %% (35); AIM-9 hits %.1f %% (18.6), out %.1f %% (26)\n",
                100.0 * s7.HitRate(), 100.0 * s7.OutOfEnvelopeShare(), 100.0 * s9.HitRate(), 100.0 * s9.OutOfEnvelopeShare());
    SC_EXPECT_NEAR(s7.HitRate(), hit7->value, hit7->tolerance);
    SC_EXPECT_NEAR(s9.HitRate(), hit9->value, hit9->tolerance);
    SC_EXPECT_NEAR(s7.OutOfEnvelopeShare(), out7->value, out7->tolerance);
    SC_EXPECT_NEAR(s9.OutOfEnvelopeShare(), out9->value, out9->tolerance);
    // The reliability of 1967 rounds is the 1972 figure (the middle of 0.70-0.75), a SAM3D assumption.
    SC_EXPECT_NEAR(aim7->reliability, rel->value, rel->tolerance);
    SC_EXPECT_NEAR(aim9->reliability, rel->value, rel->tolerance);
    // The reasons of the misses: out of the envelope, dud, seeker, ground and break all appear; and they add up.
    int total = 0;
    for (int i = 0; i < kMissReasonCount; ++i) total += s9.reasons[i];
    SC_EXPECT(total == n);
    SC_EXPECT(s9.reasons[static_cast<int>(MissReason::TooFar)] > 0 && s9.reasons[static_cast<int>(MissReason::Dud)] > 0 &&
              s9.reasons[static_cast<int>(MissReason::Break)] > 0 && s9.reasons[static_cast<int>(MissReason::SeekerFailure)] > 0);
    SC_EXPECT(s7.reasons[static_cast<int>(MissReason::Clutter)] > 0 && s7.reasons[static_cast<int>(MissReason::LaunchG)] > 0);
    // The run is keyed: the same seed again is the same run.
    const LaunchStats again = SimulateLaunches(*aim9, d.calibration, of67, 2000, 7);
    const LaunchStats first = SimulateLaunches(*aim9, d.calibration, of67, 2000, 7);
    SC_EXPECT(again.kills == first.kills && again.OutOfEnvelope() == first.OutOfEnvelope());
    // The period's other rates stay in the data for the benches: the infrared share, the gun shares, the exchange ratios.
    SC_EXPECT(d.calibration.Target("ir_kill_share_1973") != nullptr && d.calibration.Target("ir_kill_share_1982") != nullptr);
    const CalibrationTarget* gv = d.calibration.Target("gun_share_vietnam");
    const CalibrationTarget* gb = d.calibration.Target("gun_share_bekaa");
    SC_EXPECT(gv != nullptr && gb != nullptr && gb->value < gv->value);
    SC_EXPECT(d.calibration.Target("exchange_linebacker_aug_oct_1972") != nullptr && d.calibration.Target("exchange_1965_68") != nullptr);
    for (const auto& t : d.calibration.targets) SC_EXPECT(!t.second.source.empty() && !t.second.confidence.empty() && t.second.tolerance >= 0.0);
    // A later round in the same hands does better (cooled seeker, fresh round); a flare takes some of it back.
    const AamDef* aim9m = d.Aam("aim9m");
    const AamDef* r60 = d.Aam("r60");
    SC_EXPECT(aim9m != nullptr && r60 != nullptr);
    if (aim9m == nullptr || r60 == nullptr) return;
    const LaunchStats m = SimulateLaunches(*aim9m, d.calibration, of67, 20000, 7);
    SC_EXPECT(m.HitRate() > s9.HitRate());
    CalibrationDef flares = d.calibration;
    flares.mix.countermeasure_share = 1.0;
    const LaunchStats mf = SimulateLaunches(*aim9m, flares, of67, 20000, 7);
    const LaunchStats bf = SimulateLaunches(*aim9, flares, of67, 20000, 7);
    SC_EXPECT(mf.HitRate() < m.HitRate());
    // The cooled, flare-resistant seeker loses a smaller share to the flares than the old one does.
    const LaunchStats b0 = SimulateLaunches(*aim9, d.calibration, of67, 20000, 7);
    SC_EXPECT(1.0 - mf.HitRate() / m.HitRate() < 1.0 - bf.HitRate() / b0.HitRate());
    std::printf("    flares: AIM-9M %.1f -> %.1f %%, AIM-9B %.1f -> %.1f %%\n", 100.0 * m.HitRate(), 100.0 * mf.HitRate(), 100.0 * b0.HitRate(), 100.0 * bf.HitRate());
}

// ------------------------------------------------------------------------------------------------ the radars

SC_TEST("air combat radars: the fighter radars load as EM radars and each reproduces its published range") {
    em::EmLibrary lib;
    const em::EmLoadResult r = em::LoadEmLibraryFile(kData + "/em/fighter_radars_1985.json", lib);
    if (!r.ok) std::printf("    %s\n", r.error.c_str());
    SC_EXPECT(r.ok && r.provenance_mode == "per_value");
    SC_EXPECT(lib.radars.size() == 7);
    const em::Environment env;
    for (em::RadarDef d : lib.radars) {
        SC_EXPECT(d.airborne && d.role == em::RadarRole::Search && d.emitter.scan == em::ScanKind::Sector);
        SC_EXPECT(em::Calibrate(d, env) && d.loop_gain > 0.0);
        // An airborne anchor has no ground reflection: the look's multipath factor is exactly 1.
        const em::RadarLookPhysics look = em::AnchorLook(d, env, d.calibration.range_m, d.calibration.target_altitude_m, d.calibration.rcs_m2);
        SC_EXPECT_NEAR(look.multipath_factor, 1.0, 0.0);
        // The radar equation with the fitted gain gives its anchor back: first detection at Pd 0.5 on the anchor's range.
        const double got = em::DetectionRangeAt(d, env, d.calibration.target_altitude_m, d.calibration.rcs_m2, d.calibration.pd, 25.0);
        std::printf("    %-20s anchor %6.1f km, radar equation %6.1f km, look-down %2.0f dB, max %6.1f km\n", d.id.c_str(), d.calibration.range_m / 1000.0,
                    got / 1000.0, d.look_down_db, d.max_range_m / 1000.0);
        SC_EXPECT_NEAR(got, d.calibration.range_m, 0.03 * d.calibration.range_m);
        // A smaller target is seen less far (R^4: a quarter of the cross-section, 0.707 of the range).
        const double small = em::DetectionRangeAt(d, env, d.calibration.target_altitude_m, d.calibration.rcs_m2 / 4.0, d.calibration.pd, 25.0);
        SC_EXPECT_NEAR(small / got, 0.7071, 0.03);
    }
    const auto find = [&](const char* id) -> const em::RadarDef& { return lib.radars[static_cast<std::size_t>(lib.Radar(id))]; };
    // The published figures, from the research: Sapfir-23ML 50-55 km by hand; Sapfir-23D 55 km; the APG-63 class over 80 km looking up.
    {
        em::RadarDef ml = find("sapfir_23ml");
        em::Calibrate(ml, env);
        const double by_hand = em::DetectionRangeAt(ml, env, 5000.0, 5.0, 0.5, 25.0);
        SC_EXPECT(by_hand >= 50000.0 && by_hand <= 55000.0);
        SC_EXPECT_NEAR(ml.max_range_m, 85000.0, 1e-9);  // 85 km on paper
        em::RadarDef d23 = find("sapfir_23d");
        em::Calibrate(d23, env);
        SC_EXPECT_NEAR(em::DetectionRangeAt(d23, env, 5000.0, 5.0, 0.5, 25.0), 55000.0, 1700.0);
        em::RadarDef f15 = find("apg63_class");
        em::Calibrate(f15, env);
        SC_EXPECT(em::DetectionRangeAt(f15, env, 5000.0, 5.0, 0.5, 25.0) >= 79000.0);
    }
    // Look-down: none for the APQ-120 (airwar_ru), most for the APG-63 class.
    SC_EXPECT(find("apq120_class").look_down_db == 0.0 && find("apg63_class").look_down_db > find("sapfir_23ml").look_down_db);
    SC_EXPECT(find("sapfir_23ml").look_down_db > find("sapfir_23d").look_down_db && find("sapfir_23d").look_down_db > 0.0);
    // The same radar on the ground (not airborne) is fitted differently: the ground-reflection model changes the loop gain.
    em::RadarDef ground = find("sapfir_23ml");
    em::RadarDef air = ground;
    ground.airborne = false;
    ground.antenna_height_m = 10.0;
    em::Calibrate(ground, env);
    em::Calibrate(air, env);
    SC_EXPECT(std::fabs(ground.loop_gain - air.loop_gain) > 1e-3 * air.loop_gain);
}

SC_TEST("air combat radars: sensors.json names real radars and its ranges are the radars' anchors") {
    em::EmLibrary lib;
    SC_EXPECT(em::LoadEmLibraryFile(kData + "/em/fighter_radars_1985.json", lib).ok);
    const SensorsDef s = LoadRealSensors();
    const AirCombatDef d = LoadReal();
    int radars = 0;
    for (const SensorDef& x : s.sensors) {
        if (x.kind != SensorKind::Radar) continue;
        ++radars;
        const int idx = lib.Radar(x.radar_def);
        SC_EXPECT(idx >= 0);
        if (idx < 0) continue;
        const em::RadarDef& rd = lib.radars[static_cast<std::size_t>(idx)];
        SC_EXPECT_NEAR(x.by_hand_m, rd.calibration.range_m, 1.0);       // one range, said twice
        SC_EXPECT_NEAR(x.rcs_m2, rd.calibration.rcs_m2, 1e-9);
        SC_EXPECT(x.design_m <= 0.0 || x.design_m >= x.by_hand_m);      // on paper is never less than in service
        SC_EXPECT(x.cued_m <= 0.0 || (x.cued_m > x.by_hand_m && x.cued_m <= x.design_m + 1e-9));  // ground guidance reaches further, not past the paper
        SC_EXPECT(x.lock_m <= 0.0 || x.lock_m <= rd.max_range_m + 1e-9);
        SC_EXPECT(x.placeholder == (x.id == "apg66" || x.id == "rp21" || x.id == "rp22"));
        // Every carrier of a radar that has a fit is a type of the data (or the MiG-23M, a type with none).
        for (const std::string& c : x.carriers) SC_EXPECT(d.Fit(c) != nullptr || c == "mig23m");
    }
    SC_EXPECT(radars == 8);
    // Sapfir-23ML under ground control: 80-85 km, narrower sector and band, no new radar code.
    const SensorDef* ml = s.Find("sapfir_23ml");
    SC_EXPECT(ml != nullptr && ml->by_hand_m >= 50000.0 && ml->by_hand_m <= 55000.0 && ml->cued_m >= 80000.0 && ml->cued_m <= 85000.0 &&
              ml->lock_m >= 30000.0 && ml->lock_m <= 50000.0);
    const SensorDef* tp = s.Find("tp23m");
    SC_EXPECT(tp != nullptr && tp->kind == SensorKind::Irst && tp->rear_m == 35000.0);
}

// ------------------------------------------------------------------------------------------------ sources

SC_TEST("air combat data: every source id is registered in docs/SOURCES.md and none is tier 5") {
    const std::map<std::string, std::string> registered = RegisteredSources();
    SC_EXPECT(registered.size() > 100);
    int unnamed = 0, tier5 = 0;
    for (const char* rel : {"air_war/air_combat.json", "air_war/sensors.json", "em/fighter_radars_1985.json"}) CheckSourcesOf(rel, registered, unnamed, tier5);
    SC_EXPECT(unnamed == 0 && tier5 == 0);
    // The ids the research names for this task are all there.
    for (const char* id : {"checo_1967_a2a", "checo_1969_a2a", "checo_1978_linebacker", "aces_1976", "airwar_ru", "mis", "ru_wiki_aam", "parsch_dusrm",
                           "navy_factfiles", "cbo_1986_amraam", "afit_1982_vid", "dubrov_1983", "lambeth_1984", "nato_radar_refs", "usn_falklands_1983"}) {
        SC_EXPECT(registered.count(id) == 1);
    }
}
