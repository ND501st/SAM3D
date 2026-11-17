// Roadmap B1: flights and packages kept in the session, and the command layer between the pilot and the aircraft
// (simcore/flights.hpp; docs/air_battle/02_FLIGHTS_AND_PILOTS.md sections 1-2). Every number here is a generic
// placeholder, not data of a real aircraft.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/air_war/raid_scenario.hpp"
#include "simcore/flights.hpp"
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

std::string ReplaceOnce(std::string s, const std::string& from, const std::string& to) {
    const std::size_t at = s.find(from);
    if (at == std::string::npos) {
        std::printf("    test text not found: %s\n", from.c_str());
        return s;
    }
    return s.replace(at, from.size(), to);
}

// A jet flown the old way (constant speed, a turn rate): 200 m/s, 6 deg/s (placeholders).
Aircraft Jet(const Vec3& at, std::vector<Vec3> wps) {
    Aircraft a;
    a.position = at;
    a.velocity = Vec3{0.0, 200.0, 0.0};
    a.speed_mps = 200.0;
    a.max_turn_rate_rad_s = DegToRad(6.0);
    a.waypoints = std::move(wps);
    return a;
}

// A flight of `n` jets north 20 km, then a right turn of 90 degrees and east: the leader's route, each wingman's the
// same set off by his slot.
struct Formation {
    Flights flights;
    std::vector<Aircraft> air;
    double t = 0.0;
    std::vector<std::string> events;

    explicit Formation(int n, std::vector<Vec3> slots = {{0.0, 0.0, 0.0}, {1000.0, 500.0, 0.0}, {-2000.0, 1500.0, 0.0}, {-1000.0, 2000.0, 0.0}}) {
        FlightState f;
        f.id = "f1";
        f.callsign = "Test 1";
        f.formations["transit"] = slots;
        for (int k = 0; k < n; ++k) {
            const Vec3 o = slots[static_cast<std::size_t>(k)];
            const Vec3 at{o.x, -o.y, 1000.0};  // north first: right is east, back is south
            air.push_back(Jet(at, {Vec3{o.x, 20000.0 - o.y, 1000.0}, Vec3{30000.0, 20000.0 - o.x, 1000.0}, Vec3{90000.0, 20000.0 - o.x, 1000.0}}));
            FlightSlot s;
            s.target = k;
            s.number = k + 1;
            f.slots.push_back(s);
        }
        flights.Add(f);
    }
    Flights::Lookup Air() {
        return [this](int i) -> Aircraft* { return i >= 0 && i < static_cast<int>(air.size()) && air[static_cast<std::size_t>(i)].alive ? &air[static_cast<std::size_t>(i)] : nullptr; };
    }
    void Run(double until_s) {
        const Flights::Lookup lookup = Air();
        while (t < until_s - 1e-9) {
            t += 0.1;
            flights.Steer(t, lookup);
            for (Aircraft& a : air) StepAircraft(a, 0.1);
            for (std::size_t i = 0; i < air.size(); ++i) {
                bool ended = false;
                const std::string e = flights.AfterMove(static_cast<int>(i), air[i], t, lookup, ended);
                if (!e.empty()) events.push_back(std::to_string(i) + " " + e);
            }
            for (const Flights::Note& n : flights.After(t)) events.push_back(n.who + " " + n.what);
        }
    }
    // How far aircraft i is from its station on the leader now (horizontal).
    double OffStation(int i) const {
        const FlightState& f = flights.All()[0];
        const FlightSlot& s = f.slots[static_cast<std::size_t>(i)];
        const Aircraft& L = air[static_cast<std::size_t>(f.slots[static_cast<std::size_t>(f.leader_slot)].target)];
        const Vec3 st = Flights::StationOf(L, Flights::SlotOffset(f, s));
        const Vec3 d = st - air[static_cast<std::size_t>(i)].position;
        return std::hypot(d.x, d.y);
    }
    bool Saw(const std::string& what) const {
        for (const std::string& e : events) {
            if (e.find(what) != std::string::npos) return true;
        }
        return false;
    }
};

}  // namespace

SC_TEST("flights B1: a wingman keeps his slot through a 90 degree turn and is back on station after it") {
    Formation fm(2);
    fm.Run(80.0);
    SC_EXPECT(fm.OffStation(1) < 100.0);  // on the straight before the turn
    fm.Run(200.0);                        // the leader turned at 100 s; 85 s east since
    SC_EXPECT(fm.air[0].velocity.x > 190.0);
    SC_EXPECT(fm.OffStation(1) < 100.0);
    SC_EXPECT(fm.flights.All()[0].slots[1].keeping);
    // His place on the plan is his leader's.
    SC_EXPECT(fm.air[1].next_waypoint == fm.air[0].next_waypoint);
}

SC_TEST("flights B1: the leader lost, the element lead takes the flight and the rest close up behind him") {
    Formation fm(4);
    fm.Run(20.0);
    std::vector<Flights::Note> notes;
    fm.air[0].alive = false;
    fm.flights.Down(0, fm.t, &notes);
    SC_EXPECT(fm.flights.IsLeader(2));
    SC_EXPECT(!notes.empty() && notes[0].what.find("lead lost: No. 3 takes the flight") != std::string::npos);
    SC_EXPECT(fm.flights.SlotOf(2)->place == 0 && fm.flights.SlotOf(1)->place == 1 && fm.flights.SlotOf(3)->place == 2);
    // He leads: the others take their stations on him (before the turn at 100 s: 2-3 km to make up, a time
    // constant of 1 / kCloseGainPerS, 20 s).
    fm.Run(90.0);
    SC_EXPECT(fm.OffStation(1) < 200.0 && fm.OffStation(3) < 200.0);
    // No. 3 lost too: the lowest number left leads.
    fm.air[2].alive = false;
    fm.flights.Down(2, fm.t, &notes);
    SC_EXPECT(fm.flights.IsLeader(1));
}

SC_TEST("flights B1: a pilot's break ends after its time and he rejoins his flight's slot") {
    Formation fm(2);
    fm.Run(30.0);
    PilotOverride ov;
    ov.kind = PilotOverride::Kind::Break;
    ov.point = fm.air[1].position + Vec3{30000.0, 0.0, 0.0};
    ov.turn_rate_rad_s = DegToRad(12.0);
    ov.until_s = fm.t + 10.0;
    fm.flights.Override(1, ov);
    fm.Run(38.0);
    SC_EXPECT(fm.flights.Overriding(1));
    SC_EXPECT(fm.air[1].velocity.x > 100.0);  // breaking east, off his station
    SC_EXPECT(fm.OffStation(1) > 500.0);
    fm.Run(41.0);
    SC_EXPECT(!fm.flights.Overriding(1));
    SC_EXPECT(fm.Saw("1 rejoins its flight"));
    // Back for his station: 4 km out at 55 s, closing at up to 30 % over the leader's speed.
    fm.Run(60.0);
    const double off60 = fm.OffStation(1);
    fm.Run(95.0);
    SC_EXPECT(off60 > 2000.0 && fm.OffStation(1) < 0.25 * off60 && fm.flights.All()[0].slots[1].keeping);
}

SC_TEST("flights B1: an order reaches the leader at once and each member after the call; one who did not hear keeps the last") {
    Formation fm(3);
    fm.Run(10.0);
    fm.flights.SetHears(2, false);
    FlightOrder hold;
    hold.kind = OrderKind::Orbit;
    hold.has_aim = true;
    hold.aim = fm.air[0].position;
    hold.by = "leader";
    SC_EXPECT(fm.flights.Order(0, hold, fm.t));
    const FlightState& f = fm.flights.All()[0];
    SC_EXPECT(f.slots[0].order.kind == OrderKind::Orbit);
    SC_EXPECT(f.slots[1].order.kind == OrderKind::FollowRoute);
    fm.Run(10.0 + flights::kCallS - 0.5);
    SC_EXPECT(f.slots[1].order.kind == OrderKind::FollowRoute);
    fm.Run(10.0 + flights::kCallS + 0.5);
    SC_EXPECT(f.slots[1].order.kind == OrderKind::Orbit);
    SC_EXPECT(f.slots[2].order.kind == OrderKind::FollowRoute);  // he did not hear it
    SC_EXPECT(f.orders.size() == 1 && f.orders[0].by == "leader");
    // The leader orbits where he was told: a minute on he is still within the circle's reach.
    fm.Run(70.0);
    SC_EXPECT((fm.air[0].position - hold.aim).HorizontalLength() < 2.0 * flights::kOrbitRadiusM);
}

SC_TEST("flights B1: a package turns back on the losses its commander knows of, at its nerve") {
    Flights fl;
    std::vector<Aircraft> air;
    for (int i = 0; i < 4; ++i) air.push_back(Jet(Vec3{1000.0 * i, 0.0, 1000.0}, {Vec3{1000.0 * i, 50000.0, 1000.0}}));
    for (int k = 0; k < 2; ++k) {
        FlightState f;
        f.id = "f" + std::to_string(k + 1);
        f.package = "p1";
        for (int m = 0; m < 2; ++m) {
            FlightSlot s;
            s.target = 2 * k + m;
            s.number = m + 1;
            f.slots.push_back(s);
        }
        fl.Add(f);
    }
    PackageState p;
    p.id = "p1";
    p.commander_flight = 0;
    p.flights = {0, 1};
    p.turn_back_share = 0.4;
    fl.AddPackage(p);
    SC_EXPECT(fl.Packages()[0].size == 4);
    fl.Down(3, 10.0);  // in the other flight: he hears of it after the call
    SC_EXPECT(fl.After(10.5).empty());
    SC_EXPECT(fl.Packages()[0].losses_known == 0);
    fl.Down(1, 11.0);  // his own wingman: at once
    const std::vector<Flights::Note> notes = fl.After(13.5);
    SC_EXPECT(fl.Packages()[0].losses_known == 2 && fl.Packages()[0].turned_back);
    SC_EXPECT(!notes.empty() && notes[0].what.find("turns back after losing 2 of 4") != std::string::npos);
    SC_EXPECT(fl.All()[1].order.kind == OrderKind::ReturnHome && fl.All()[1].order.by == "package");
}

SC_TEST("flights B1: a hand-written scenario's targets are flights of one and fly their own waypoints, as before") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenario(ReadFile(kData + "/scenarios/osa_practice.json"), kData, def, console, &e));
    const std::size_t n = def.targets.size();
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.AllFlights().Size() == n);
    for (const FlightState& f : r.AllFlights().All()) SC_EXPECT(f.slots.size() == 1 && f.id == r.Def().targets[static_cast<std::size_t>(f.slots[0].target)].id);
    bool demanded = false;
    while (r.Time() < 260.0) {
        r.Step(0.1);
        const Aircraft* a = r.GetWorld().FindAircraft(r.TargetWorldId("t1"));
        demanded = demanded || (a != nullptr && a->demand.active);
    }
    SC_EXPECT(r.TargetAlive("t1") && !demanded);
}

SC_TEST("flights B1: the loader checks flights, packages, slots and legs") {
    const std::string base = ReadFile(kData + "/scenarios/osa_practice.json");
    const std::string tgt = R"j("id": "t1", "name": "Practice drone (placeholder)",)j";
    auto load = [&](const std::string& text, std::string& e) {
        ScenarioDef d;
        ConsoleData c;
        e.clear();
        return LoadScenario(text, kData, d, c, &e);
    };
    auto bad = [&](const std::string& text, const std::string& expect) {
        std::string e;
        const bool ok = load(text, e);
        if (ok || e.find(expect) == std::string::npos) std::printf("    expected \"%s\", got ok=%d \"%s\"\n", expect.c_str(), ok ? 1 : 0, e.c_str());
        return !ok && e.find(expect) != std::string::npos;
    };
    const std::string flights = R"("flights": [ { "id": "f1", "package": "p1", "formations": { "transit": [ [0, 0, 0], [800, 300, 0] ] } } ],
  "packages": [ { "id": "p1", "commander": "f1", "size": 4, "turn_back_share": 0.5 } ],
  "targets": [)";
    const std::string good = ReplaceOnce(ReplaceOnce(base, R"("targets": [)", flights), tgt, tgt + R"( "flight": "f1", "slot": 1,)");
    std::string e;
    SC_EXPECT(load(good, e));
    SC_EXPECT(bad(ReplaceOnce(good, R"("id": "f1", "package")", R"("id": "f1", "leader": 2, "package")"), "unknown key"));
    SC_EXPECT(bad(ReplaceOnce(good, R"("transit": [ [0, 0, 0])", R"("cruise": [ [0, 0, 0])"), "unknown key"));
    SC_EXPECT(bad(ReplaceOnce(good, R"([800, 300, 0])", R"([800, 300])"), "expected [right_m, back_m, up_m]"));
    SC_EXPECT(bad(ReplaceOnce(good, R"("flight": "f1", "slot": 1,)", R"("flight": "f2", "slot": 1,)"), "no flight 'f2'"));
    SC_EXPECT(bad(ReplaceOnce(good, R"("flight": "f1", "slot": 1,)", R"("slot": 1,)"), "needs a flight"));
    SC_EXPECT(bad(ReplaceOnce(good, R"("commander": "f1")", R"("commander": "f9")"), "is not a flight of this package"));
    SC_EXPECT(bad(ReplaceOnce(good, R"("package": "p1", "formations")", R"("package": "p2", "formations")"), "no package 'p2'"));
    SC_EXPECT(bad(ReplaceOnce(good, R"("turn_back_share": 0.5)", R"("turn_back_share": 1.5)"), "between 0 and 1"));
    SC_EXPECT(bad(ReplaceOnce(good, R"("flight": "f1", "slot": 1,)", R"("flight": "f1", "slot": 1, "legs": [ {}, {} ],)"), "one per waypoint"));
    SC_EXPECT(bad(ReplaceOnce(good, R"("flight": "f1", "slot": 1,)", R"("flight": "f1", "slot": 1, "legs": [ { "speed_mps": 200, "pace": 1 } ],)"), "unknown key"));
    SC_EXPECT(load(ReplaceOnce(good, R"("flight": "f1", "slot": 1,)", R"("flight": "f1", "slot": 1, "legs": [ { "speed_mps": 200, "tag": "ingress" } ],)"), e));
}

SC_TEST("flights B1: a faction's flights block is read strictly: classes, phases, slots and their sources") {
    const std::string text = ReadFile(kData + "/air_war/faction_nato.json");
    airwar::FactionDef f;
    std::string e;
    SC_EXPECT(airwar::LoadFactionJson(text, f, &e));
    SC_EXPECT(f.formations.count("fighter") == 1 && f.formations.count("strike") == 1 && f.formations.count("helicopter") == 1);
    const std::map<std::string, std::vector<Vec3>> strike = f.FormationsFor("strike");
    SC_EXPECT(strike.count("transit") == 1 && strike.at("transit").size() == 4);
    for (const auto& cls : f.formations) {
        for (const auto& ph : cls.second) {
            SC_EXPECT(!ph.second.source.empty() && !ph.second.confidence.empty());
            SC_EXPECT(ph.second.slots.size() >= 2 && ph.second.slots[0].LengthSquared() == 0.0);
        }
    }
    const std::string fighter = R"("fighter": {)";
    SC_EXPECT(!airwar::LoadFactionJson(ReplaceOnce(text, fighter, R"("bomber": {)"), f, &e) && e.find("unknown key") != std::string::npos);
    SC_EXPECT(!airwar::LoadFactionJson(ReplaceOnce(text, R"("transit": { "slots": [ [0, 0, 0])", R"("cruise": { "slots": [ [0, 0, 0])"), f, &e) &&
              e.find("unknown key") != std::string::npos);
    SC_EXPECT(!airwar::LoadFactionJson(ReplaceOnce(text, R"("slots": [ [0, 0, 0])", R"("slots": [ [50, 0, 0])"), f, &e) &&
              e.find("the leader's own place") != std::string::npos);
    SC_EXPECT(!airwar::LoadFactionJson(ReplaceOnce(text, R"("confidence": "medium" })", R"("confidence": "certain" })"), f, &e) &&
              e.find("confidence") != std::string::npos);
    // Every faction has one, with the three classes' transit.
    for (const char* id : {"nato", "warsaw_pact", "neutral", "rebels"}) {
        airwar::FactionDef fd;
        SC_EXPECT(airwar::LoadFactionFile(kData + "/air_war/faction_" + std::string(id) + ".json", fd, &e));
        for (const char* cls : {"fighter", "strike", "helicopter"}) SC_EXPECT(fd.FormationsFor(cls).count("transit") == 1);
    }
}

SC_TEST("flights B1: a scenario's flight of two keeps its formation through a turn, its wingman on the leader's place on the plan") {
    const std::string base = ReadFile(kData + "/scenarios/osa_practice.json");
    const std::string pair = R"j("flights": [ { "id": "f1", "formations": { "transit": [ [0, 0, 0], [1500, 600, 0] ] } } ],
  "targets": [
    { "id": "a1", "name": "Lead (placeholder)", "type": "Air+FixedWing", "class": "fighter", "flight": "f1", "slot": 1,
      "start": { "east_m": -20000, "north_m": 40000, "up_m": 2000 },
      "waypoints": [ { "east_m": -20000, "north_m": 20000, "up_m": 2000 }, { "east_m": 40000, "north_m": 20000, "up_m": 2000 } ],
      "speed_mps": 220, "max_turn_rate_deg_s": 5, "rcs_m2": 3, "appear_at_s": 0 },
    { "id": "a2", "name": "Wingman (placeholder)", "type": "Air+FixedWing", "class": "fighter", "flight": "f1", "slot": 2,
      "start": { "east_m": -21500, "north_m": 40600, "up_m": 2000 },
      "waypoints": [ { "east_m": -21500, "north_m": 20600, "up_m": 2000 }, { "east_m": 40000, "north_m": 18500, "up_m": 2000 } ],
      "speed_mps": 220, "max_turn_rate_deg_s": 5, "rcs_m2": 3, "appear_at_s": 0 }
  ],
  "triggers": [],
  "end": { "time_limit_s": 700 }
})j";
    std::string text = base.substr(0, base.find(R"("targets": [)")) + pair;
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenario(text, kData, def, console, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.AllFlights().Size() == 1 && r.Flight("f1") != nullptr && r.Flight("f1")->slots.size() == 2);
    double worst_after_turn = 0.0;
    while (r.Time() < 260.0) {
        r.Step(0.1);
        if (r.Time() < 200.0) continue;  // the turn at about 90 s; settled since
        const Vec3 L = r.TargetPosition("a1"), W = r.TargetPosition("a2");
        const Vec3 v = r.TargetVelocity("a1");
        const Vec3 fwd = Vec3{v.x, v.y, 0.0}.Normalized();
        const Vec3 right{fwd.y, -fwd.x, 0.0};
        const Vec3 station = L + right * 1500.0 - fwd * 600.0;
        worst_after_turn = std::max(worst_after_turn, std::hypot(station.x - W.x, station.y - W.y));
    }
    SC_EXPECT(r.TargetVelocity("a1").x > 200.0);  // east, after the turn
    SC_EXPECT(worst_after_turn < 200.0);
}

SC_TEST("flights B1: a planner's raid flies as flights in formation, in one package with the resolver's nerve") {
    using namespace airwar;
    AirWarData d;
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", d, &e));
    AirCampaign c;
    SC_EXPECT(c.Init(d, 1));
    c.Posture().emission = 0.1;
    c.Posture().relocate_every_h = 20.0;
    // A flight of two or more that keeps its slots: its wingmen on their stations once in the air a while, outside the
    // attack (where each flies his own run). The first raid that has one in the air out of its attack (the campaign's
    // course decides which raids come: roadmap B5's packages fly in time; a raid clipped near the battery may begin in
    // its attack).
    int raids = 0, kept = 0, looked = 0;
    std::string pair_id;
    std::map<std::string, int> phases;
    while (c.Now() < 6.0 * 24.0 && looked == 0) {
        if (!c.Advance(6.0 * 24.0)) continue;
        RaidScenarioBuilder builder;
        const RaidScenario rs = builder.Build(c, *c.PlayerMission());
        c.SkipPlayerMission();
        if (!rs.ok) continue;
        ++raids;
        ScenarioDef def;
        ConsoleData console;
        SC_EXPECT(LoadScenario(rs.json_text, kData, def, console, &e));
        SC_EXPECT(!def.flights.empty() && def.packages.size() == 1);
        SC_EXPECT(def.packages[0].turn_back_share >= 0.05 && def.packages[0].turn_back_share <= 0.9);
        for (const ScenarioTarget& t : def.targets) SC_EXPECT(!t.flight.empty() && t.slot >= 1 && t.legs.size() == t.waypoints.size());
        ScenarioRunner r(std::move(def), std::move(console));
        const FlightState* pair = nullptr;
        for (const FlightState& f : r.AllFlights().All()) {
            if (f.slots.size() >= 2 && f.keep_slots && !f.weapons && pair == nullptr) pair = &f;
        }
        if (pair == nullptr) continue;
        pair_id = pair->id;
        const std::string lead = r.Def().targets[static_cast<std::size_t>(pair->slots[0].target)].id;
        const std::string wing = r.Def().targets[static_cast<std::size_t>(pair->slots[1].target)].id;
        const double appear = r.Def().targets[static_cast<std::size_t>(pair->slots[0].target)].appear_at_s;
        phases.clear();
        while (r.Time() < appear + 240.0 && !r.Finished()) {
            r.Step(0.1);
            if (r.Time() < appear + 60.0 || !r.TargetAlive(lead) || !r.TargetAlive(wing)) continue;
            const FlightState* f = r.Flight(pair_id);
            if (f == nullptr) continue;
            ++phases[f->phase];
            if (f->phase == "attack") continue;
            ++looked;
            kept += f->slots[1].keeping ? 1 : 0;
        }
    }
    SC_EXPECT(raids > 0 && looked > 0 && kept > looked / 2);
    std::printf("    flight %s (raid %d): wingman on his station %d of %d steps out of the attack\n", pair_id.c_str(), raids, kept, looked);
    for (const auto& kv : phases) std::printf("      %s: %d steps\n", kv.first.c_str(), kv.second);
}

SC_TEST("flights B1: the runner's flight orders are logged with who gave them; a reroute flies its new legs") {
    const std::string base = ReadFile(kData + "/scenarios/osa_practice.json");
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenario(base, kData, def, console, &e));
    def.targets[0].appear_at_s = 0.0;
    ScenarioRunner r(std::move(def), std::move(console));
    r.Step(0.1);
    std::string why;
    SC_EXPECT(!r.OrderFlight("nobody", FlightOrder(), &why) && why.find("no flight") != std::string::npos);
    RouteLeg leg;
    leg.to = Vec3{30000.0, 45000.0, 1000.0};
    SC_EXPECT(r.RerouteFlight("t1", {leg}, leg.to, "regiment", &why));
    while (r.Time() < 60.0) r.Step(0.1);
    SC_EXPECT(r.TargetVelocity("t1").x > 100.0);  // east, for its new point
    SC_EXPECT(r.Flight("t1")->orders.size() == 1 && r.Flight("t1")->orders[0].by == "regiment");
    SC_EXPECT(r.Debrief().find("ordered: divert (by regiment)") != std::string::npos);
}

SC_TEST("flights B1: an aircraft's demand steers it for the point at the speed and turn rate given, its waypoints kept") {
    // Mirrored in prototype/tests/test_prototype.py (test_aircraft_demand).
    for (int by_bank = 0; by_bank < 2; ++by_bank) {
        Aircraft a = Jet(Vec3{0.0, 0.0, 1000.0}, {Vec3{0.0, 40000.0, 1000.0}});
        if (by_bank == 1) {
            a.max_load_g = 7.0;
            a.roll_rate_rad_s = DegToRad(90.0);
        }
        a.demand.active = true;
        a.demand.point = Vec3{30000.0, 0.0, 1000.0};
        a.demand.speed_mps = 250.0;
        a.demand.turn_rate_rad_s = DegToRad(10.0);
        for (int i = 0; i < 300; ++i) StepAircraft(a, 0.1);
        SC_EXPECT(a.velocity.x > 240.0 && std::fabs(a.velocity.y) < 30.0);  // for the point, on y = 0
        SC_EXPECT(a.speed_mps == 200.0 && a.max_turn_rate_rad_s == DegToRad(6.0));  // its own again after each step
        SC_EXPECT(a.next_waypoint == 0 && !a.demand.reached);
        // Captured: the point passed as a waypoint is, it flies straight on and says so.
        a.demand.point = a.position + Vec3{400.0, 0.0, 0.0};
        a.demand.capture = true;
        const Vec3 v = a.velocity;
        StepAircraft(a, 0.1);
        SC_EXPECT(a.demand.reached && (a.velocity - v).Length() < 1.0);
        // Inactive: its waypoints as before.
        a.demand = AircraftDemand();
        for (int i = 0; i < 300; ++i) StepAircraft(a, 0.1);
        SC_EXPECT(a.velocity.y > 150.0);
    }
}

SC_TEST("flights B1: the command layer's cost for 100 aircraft in 25 flights of four (budget about 0.5 ms a 0.05 s step)") {
    Flights fl;
    std::vector<Aircraft> air;
    for (int f = 0; f < 25; ++f) {
        FlightState fs;
        fs.id = "f" + std::to_string(f);
        fs.formations["transit"] = {Vec3{}, Vec3{1000.0, 500.0, 0.0}, Vec3{-2000.0, 1500.0, 0.0}, Vec3{-1000.0, 2000.0, 0.0}};
        for (int k = 0; k < 4; ++k) {
            const Vec3 at{5000.0 * f + 1000.0 * k, -500.0 * k, 1000.0};
            air.push_back(Jet(at, {at + Vec3{0.0, 30000.0, 0.0}, at + Vec3{30000.0, 30000.0, 0.0}}));
            FlightSlot s;
            s.target = 4 * f + k;
            s.number = k + 1;
            fs.slots.push_back(s);
        }
        fl.Add(fs);
    }
    const Flights::Lookup lookup = [&air](int i) -> Aircraft* { return &air[static_cast<std::size_t>(i)]; };
    const int steps = 2000;
    double layer_s = 0.0;
    double t = 0.0;
    for (int n = 0; n < steps; ++n) {
        t += 0.05;
        const auto t0 = std::chrono::steady_clock::now();
        fl.Steer(t, lookup);
        const auto t1 = std::chrono::steady_clock::now();
        for (Aircraft& a : air) StepAircraft(a, 0.05);
        const auto t2 = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < air.size(); ++i) {
            bool ended = false;
            fl.AfterMove(static_cast<int>(i), air[i], t, lookup, ended);
        }
        fl.After(t);
        const auto t3 = std::chrono::steady_clock::now();
        layer_s += std::chrono::duration<double>(t1 - t0).count() + std::chrono::duration<double>(t3 - t2).count();
    }
    const double ms = 1000.0 * layer_s / steps;
    std::printf("    the command layer: %.4f ms a step for 100 aircraft\n", ms);
    SC_EXPECT(ms < 5.0);  // ten times the budget: a loose guard, the number is the report's
}
