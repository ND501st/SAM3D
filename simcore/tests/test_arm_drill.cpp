// Roadmap E6, the ARM drill (voenizdat_osa_1991 pp262-264) played through the scenario runner: carriers fire
// anti-radiation missiles on what their warning receivers hear; the senior operator watches his scope for the signs of a
// launch; the commander has every transmitter off once the missile in the air has burst; the least emission under the
// threat; a dummy position's decoy; the battery's change of position after the attack. The raids are the scenarios'
// placeholder numbers (data/scenarios/osa_arm_drill*.json); the missiles are the EM library's classes (estimates).
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string Replace(std::string s, const std::string& from, const std::string& to) {
    const std::size_t at = s.find(from);
    if (at == std::string::npos) {
        std::printf("    test text not found: %s\n", from.c_str());
        return s;
    }
    return s.replace(at, from.size(), to);
}

std::unique_ptr<ScenarioRunner> Runner(const std::string& text, unsigned seed, std::string* error = nullptr) {
    ScenarioDef def;
    ConsoleData console;
    std::string err;
    if (!LoadScenario(text, kData, def, console, &err)) {
        if (error != nullptr) *error = err;
        else std::printf("    load: %s\n", err.c_str());
        return nullptr;
    }
    def.seed = seed;
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

bool Has(const std::string& debrief, const std::string& text) { return debrief.find(text) != std::string::npos; }

const char* kVehicleHit = "the vehicle was hit by an anti-radiation missile";

std::string Harm() { return ReadFile(kData + "/scenarios/osa_arm_drill.json"); }
std::string Shrike() { return ReadFile(kData + "/scenarios/osa_arm_drill_shrike.json"); }
// The seats that watch for the signs of an ARM launch (the search scope, the tracking radar's indicators, the TV) of a
// "basic" tier: they don't know the signs, so nobody reacts to them.
std::string Untrained(const std::string& text) {
    std::string s = Replace(text, "{ \"name\": \"Senior operator\", \"role\": \"senior_operator\", \"skill\": \"class1\" }",
                            "{ \"name\": \"Senior operator\", \"role\": \"senior_operator\", \"skill\": \"class3\" }");
    s = Replace(s, "{ \"name\": \"Deputy commander\", \"role\": \"deputy_commander\", \"skill\": \"class1\" }",
                "{ \"name\": \"Deputy commander\", \"role\": \"deputy_commander\", \"skill\": \"class3\" }");
    return Replace(s, "{ \"name\": \"Operator\", \"role\": \"operator\", \"skill\": \"class2\" }",
                   "{ \"name\": \"Operator\", \"role\": \"operator\", \"skill\": \"class3\" }");
}
// The fight only: the scenario ends when the raid is gone.
std::string FightOnly(const std::string& text) {
    return Replace(text, "\"when_targets_gone\": false", "\"when_targets_gone\": true");
}

struct Outcome {
    bool hit = false;
    bool sign_called = false;
    bool silenced = false;
    double first_miss_m = -1.0;
    int fired = 0;
    std::string first_hit_what;
};

Outcome Play(const std::string& text, unsigned seed, double max_s = 1200.0) {
    Outcome o;
    auto r = Runner(text, seed);
    if (r == nullptr) return o;
    r->RunToEnd(0.1, max_s);
    const std::string d = r->Debrief();
    o.hit = r->EndReason() == kVehicleHit;
    o.sign_called = Has(d, "Пуск ПРР!") || Has(d, "возможен пуск ПРР!");
    o.silenced = Has(d, "radiation off against the ARM");
    o.fired = r->ArmsFired();
    if (!r->ArmResults().empty()) {
        o.first_miss_m = r->ArmResults().front().miss_m;
        o.first_hit_what = r->ArmResults().front().hit_what;
    }
    return o;
}

}  // namespace

SC_TEST("arm drill: a target's arms load, and need the EM layer, a warning receiver and a library ARM") {
    std::string err;
    auto r = Runner(Harm(), 1, &err);
    SC_EXPECT(r != nullptr);
    if (r != nullptr) {
        const ScenarioTarget& w1 = r->Def().targets.front();
        SC_EXPECT(w1.arms.weapon == "agm88a_class_harm" && w1.arms.count == 1);
        SC_EXPECT_NEAR(w1.arms.launch_range_m, 22000.0, 1e-6);
        SC_EXPECT_NEAR(w1.arms.min_range_m, 3000.0, 1e-6);  // the weapon's own
        SC_EXPECT(r->Def().doctrine.arm_threat && r->Def().doctrine.relocate_after_arm);
        SC_EXPECT(r->Def().tuning.has_arm_table);
        SC_EXPECT_NEAR(r->Def().tuning.arm_track_on_fast_far_m, 24000.0, 1e-6);  // Table 7.3
        SC_EXPECT_NEAR(r->Def().tuning.arm_track_on_slow_far_m, 20000.0, 1e-6);
    }
    SC_EXPECT(Runner(Replace(Harm(), "\"weapon\": \"agm88a_class_harm\", \"count\": 1, \"launch_range_km\": 22, \"notes\"",
                             "\"weapon\": \"no_such_arm\", \"count\": 1, \"launch_range_km\": 22, \"notes\""),
                     1, &err) == nullptr);
    SC_EXPECT(Has(err, "not an anti-radiation missile"));
    // Without its warning receiver it would not know what to fire at.
    std::string no_rwr = Harm();
    no_rwr = Replace(no_rwr, "\"size_m\": 5.0, \"rwr\": \"nato_alr46_class_rwr\",", "\"size_m\": 5.0,");
    SC_EXPECT(Runner(no_rwr, 1, &err) == nullptr);
    SC_EXPECT(Has(err, "rwr"));
    // The change of position needs the battery to drive the vehicle.
    SC_EXPECT(Runner(Replace(Harm(), "\"drive_own\": true,", "\"drive_own\": false,"), 1, &err) == nullptr);
    SC_EXPECT(Has(err, "relocate_after_arm"));
}

SC_TEST("arm drill: a carrier fires only on what its warning receiver hears, then turns away") {
    // The battery silent from the start (its EMCON): nothing on the air, nothing to fire at.
    std::string silent = Replace(Harm(), "\"own\": { \"id\": \"21\", \"callsign\": \"21\" },",
                                 "\"own\": { \"id\": \"21\", \"callsign\": \"21\" }, \"orders\": [ { \"at_s\": 0, \"order\": \"emcon\" } ],");
    auto r = Runner(FightOnly(silent), 1);
    SC_EXPECT(r != nullptr);
    if (r != nullptr) {
        r->RunToEnd(0.1, 400.0);
        SC_EXPECT(r->ArmsFired() == 0);
        SC_EXPECT(r->EndReason() != kVehicleHit);
    }
    auto h = Runner(FightOnly(Untrained(Harm())), 1);
    SC_EXPECT(h != nullptr);
    if (h == nullptr) return;
    bool seen_in_flight = false;
    while (!h->Finished() && h->Time() < 600.0) {
        h->Step(0.1);
        for (const EmSensorLink::ArmView& v : h->ArmsInFlight()) seen_in_flight = seen_in_flight || (v.carrier == "w1" && v.position.z > 0.0);
    }
    const std::string d = h->Debrief();
    SC_EXPECT(Has(d, "w1  fires an anti-radiation missile") && Has(d, "SA-8 search"));  // its RWR's name for the search radar
    SC_EXPECT(Has(d, "w1  turns away from the radar"));
    SC_EXPECT(seen_in_flight);
    SC_EXPECT(Has(h->Acmi().Text(), "AGM-88A HARM class"));  // drawn in Tacview
}

SC_TEST("arm drill: the crew sees the signs, goes silent and the vehicle mostly survives a HARM pair; untrained, it is hit") {
    int survived = 0, called = 0;
    for (unsigned seed = 1; seed <= 6; ++seed) {
        const Outcome o = Play(FightOnly(Harm()), seed);
        if (!o.hit) ++survived;
        if (o.sign_called && o.silenced) ++called;
        std::printf("    HARM drill seed %u: %s, first ARM %.0f m off%s\n", seed, o.hit ? "HIT" : "survived", o.first_miss_m,
                    o.sign_called ? "" : " (no sign called)");
    }
    SC_EXPECT(called == 6);
    SC_EXPECT(survived >= 4);
    int untrained_hit = 0;
    for (unsigned seed = 1; seed <= 4; ++seed) {
        const Outcome o = Play(FightOnly(Untrained(Harm())), seed);
        if (o.hit) ++untrained_hit;
        SC_EXPECT(!o.silenced);
    }
    std::printf("    untrained senior operator: hit %d of 4\n", untrained_hit);
    SC_EXPECT(untrained_hit >= 3);
}

SC_TEST("arm drill: going silent beats a Shrike-class missile by kilometres; a HARM-class one it misses by less") {
    double shrike_sum = 0.0, harm_sum = 0.0;
    int shrike_hits = 0, n = 0;
    for (unsigned seed = 1; seed <= 6; ++seed) {
        const Outcome s = Play(FightOnly(Shrike()), seed);
        const Outcome h = Play(FightOnly(Harm()), seed);
        if (s.hit) ++shrike_hits;
        if (s.first_miss_m < 0.0 || h.first_miss_m < 0.0) continue;
        shrike_sum += s.first_miss_m;
        harm_sum += h.first_miss_m;
        ++n;
    }
    SC_EXPECT(shrike_hits == 0);
    SC_EXPECT(n >= 5);
    if (n == 0) return;
    std::printf("    first ARM's miss, mean of %d seeds: Shrike class %.0f m, HARM class %.0f m\n", n, shrike_sum / n, harm_sum / n);
    SC_EXPECT(shrike_sum / n > 1500.0);  // lost lock and fell ballistic
    SC_EXPECT(shrike_sum / n > harm_sum / n);
}

SC_TEST("arm drill: a dummy position's decoy, radiating while the radar is silent, takes the HARM") {
    const std::string dummy = Replace(Harm(), "\"readiness\": \"deployed\",",
                                      "\"readiness\": \"deployed\",\n    \"prepared_position\": { \"dummies\": [ { \"position\": { \"east_m\": 600, "
                                      "\"north_m\": 500, \"up_m\": 0 }, \"decoy\": \"when_radar_off\" } ] },");
    int on_dummy = 0, survived = 0;
    for (unsigned seed = 1; seed <= 4; ++seed) {
        const Outcome o = Play(FightOnly(dummy), seed);
        if (o.first_hit_what == "dummy") ++on_dummy;
        if (!o.hit) ++survived;
    }
    std::printf("    dummy decoy: first ARM on the dummy %d of 4, vehicle survived %d of 4\n", on_dummy, survived);
    SC_EXPECT(on_dummy >= 3);
    SC_EXPECT(survived == 4);
}

SC_TEST("arm drill: the least emission; the attack reported up; the battery changes position when the fight is over") {
    auto r = Runner(Harm(), 1);
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    int hv_switches = 0, last_hv = -1;
    bool beams_set = false;
    double moved_at = -1.0;
    while (!r->Finished() && r->Time() < 1200.0) {
        r->Step(0.1);
        const int hv = r->Console().panel.Get("soc.hv");
        if (r->Time() < 95.0 && last_hv >= 0 && hv != last_hv) ++hv_switches;  // before the launch: the bursts
        last_hv = hv;
        beams_set = beams_set || r->Console().panel.Get("soc.beam") == 4;
        if (moved_at < 0.0 && (r->UnitPosition() - Vec3{0.0, 0.0, r->UnitPosition().z}).HorizontalLength() > 50.0) moved_at = r->Time();
    }
    const std::string d = r->Debrief();
    SC_EXPECT(r->EndReason() != kVehicleHit);
    std::printf("    search transmitter switched %d times in the first 95 s; the vehicle moved off at %.0f s\n", hv_switches, moved_at);
    SC_EXPECT(hv_switches >= 12);  // on 2 turns in every 5: off 5.5 s, on 3.6 s
    SC_EXPECT(beams_set);          // АВТ. I–II, both low beams in each burst
    SC_EXPECT(Has(d, "Излучение выключить!"));
    SC_EXPECT(Has(d, "21: пуск ПРР"));  // up the battery net
    SC_EXPECT(Has(d, "the battery changes position"));
    SC_EXPECT(moved_at > 0.0);
    SC_EXPECT((r->UnitPosition() - Vec3{700.0, -600.0, r->UnitPosition().z}).HorizontalLength() < 50.0);
    SC_EXPECT(Has(d, "Излучение включить!"));  // radiation back on after the silence
}


// The practice shoot (data/scenarios/osa_practice_em.json) with its target given a warning receiver and a HARM-class
// missile, its own missiles all missing, the deputy's TV on: the crew tracks the shooter when it fires. It fires from
// 8 km: the EM layer's tracking radar sees the HARM's 0.08 m2 (an estimate) only that close, for the few tenths of a
// second before the missile's loft takes it out of the 1 deg beam; from 12 km the indicators show nothing.
SC_TEST("arm drill: the launch from the tracked target seen on the tracking radar's indicators and on the TV") {
    std::string text = ReadFile(kData + "/scenarios/osa_practice_em.json");
    text = Replace(text, "\"track_radar\": \"osa_ak_ssc\" }",
                   "\"track_radar\": \"osa_ak_ssc\", \"extra_libraries\": [\"em/nato_1985.json\"] }");
    text = Replace(text, "\"speed_mps\": 150, \"rcs_m2\": 0.5, \"appear_at_s\": 200,",
                   "\"speed_mps\": 150, \"rcs_m2\": 0.5, \"appear_at_s\": 200, \"rwr\": \"nato_alr46_class_rwr\", "
                   "\"arms\": { \"weapon\": \"agm88a_class_harm\", \"count\": 1, \"launch_range_km\": 8 },");
    ScenarioDef def;
    ConsoleData console;
    std::string err;
    SC_EXPECT(LoadScenario(text, kData, def, console, &err));
    if (!err.empty()) std::printf("    load: %s\n", err.c_str());
    for (auto& h : def.tuning.hit_chance) h.second = 0.0;  // the shooter lives to fire
    ScenarioRunner r(std::move(def), std::move(console));
    r.Console().panel.Set("tov.power", 1);  // the deputy's TV on
    bool track = false, tv = false;
    while (!r.Finished() && r.Time() < 900.0 && !(track && tv)) {
        r.Step(0.1);
        for (const SpokenCallout& s : r.Callouts()->Log()) {
            if (s.id == "arm_launch_track" && !track) {
                track = true;
                SC_EXPECT(s.from == "operator");
            }
            if (s.id == "arm_launch_tv" && !tv) {
                tv = true;
                SC_EXPECT(s.from == "deputy_commander");
            }
        }
    }
    for (int i = 0; i < 50; ++i) r.Step(0.1);  // the calls heard
    const std::string d = r.Debrief();
    const bool heard = Has(d, "hears the sign of an ARM launch");
    std::printf("    ARMs fired %d; on the indicators %s, on the TV %s; the commander %s\n", r.ArmsFired(), track ? "yes" : "no",
                tv ? "yes" : "no", heard ? "acts on it" : "heard nothing");
    SC_EXPECT(r.ArmsFired() == 1);
    SC_EXPECT(track && tv && heard);
    SC_EXPECT(Has(d, "smaller pulse leave the tracked target's pulse") && Has(d, "fire a missile on the TV"));
}
