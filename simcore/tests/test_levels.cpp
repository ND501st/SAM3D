// Roadmap B12: the levels of detail of a session's aircraft (simcore/air_levels.hpp; the runner's A1;
// docs/air_battle/02_FLIGHTS_AND_PILOTS.md §5). Every number here is a generic placeholder, not data of a real aircraft.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/air_levels.hpp"
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

std::string Point(double e, double n, double up) {
    char b[96];
    std::snprintf(b, sizeof(b), "{ \"east_m\": %.0f, \"north_m\": %.0f, \"up_m\": %.0f }", e, n, up);
    return b;
}

// The EM practice shoot with NATO's receivers, its drone, and `flights` flights of four (placeholders) flying `route`
// (north km of each turn point, 3000 m up), the first `east_m` east, each flight 6 km east of the last; `end_s` the time limit.
std::string Session(int flights, const std::vector<double>& route_km, double end_s, bool rwr = true, double east_m = 0.0) {
    std::string s;
    for (char c : ReadFile(kData + "/scenarios/osa_practice_em.json"))
        if (c != '\r') s += c;  // the checkout may have CRLF lines
    s = ReplaceOnce(s, "\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\" }",
                    "\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\", "
                    "\"extra_libraries\": [\"em/nato_1985.json\"] }");
    const double slots[4][3] = {{0.0, 0.0, 0.0}, {1000.0, 500.0, 0.0}, {-2000.0, 1500.0, 0.0}, {-1000.0, 2000.0, 0.0}};
    std::string fl = "\"flights\": [";
    std::string tg;
    for (int f = 0; f < flights; ++f) {
        const std::string fid = "f" + std::to_string(f + 1);
        fl += std::string(f > 0 ? ", " : "") + "{ \"id\": \"" + fid + "\", \"formations\": { \"transit\": [ [0, 0, 0], [1000, 500, 0], [-2000, 1500, 0], [-1000, 2000, 0] ] } }";
        for (int k = 0; k < 4; ++k) {
            const double e = east_m + 6000.0 * f + slots[k][0];
            const double back = slots[k][1];
            const double first_n = route_km[0] * 1000.0;
            const double dir = route_km.size() > 1 && route_km[1] < route_km[0] ? 1.0 : -1.0;  // "back" is behind the first leg's way
            std::string wps;
            for (std::size_t i = 1; i < route_km.size(); ++i) wps += std::string(i > 1 ? ", " : "") + Point(e, route_km[i] * 1000.0, 3000.0);
            tg += ",\n    { \"id\": \"" + fid + "_" + std::to_string(k + 1) + "\", \"name\": \"Jet (placeholder)\", \"type\": \"Air+FixedWing\", \"class\": \"fighter\", " +
                  "\"flight\": \"" + fid + "\", \"slot\": " + std::to_string(k + 1) + ", " + (rwr ? "\"rwr\": \"nato_alr46_class_rwr\", " : "") +
                  "\"start\": " + Point(e, first_n + dir * back, 3000.0) + ", \"waypoints\": [ " + wps + " ], " +
                  "\"speed_mps\": 220, \"max_turn_rate_deg_s\": 5, \"rcs_m2\": 3, \"appear_at_s\": 0 }";
        }
    }
    fl += "],\n  ";
    s = ReplaceOnce(s, "\"targets\": [", fl + "\"targets\": [");
    // After the drone, the flights.
    const std::size_t drone = s.find("\"id\": \"t1\"");
    const std::size_t close = s.find("}\n  ]", drone);
    if (drone != std::string::npos && close != std::string::npos) s.insert(close + 1, tg);
    char end[64];
    std::snprintf(end, sizeof(end), "\"time_limit_s\": %.0f", end_s);
    s = ReplaceOnce(s, "\"time_limit_s\": 700", end);
    return s;
}

std::unique_ptr<ScenarioRunner> Runner(const std::string& text, int mode) {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenario(text, kData, def, console, &error)) {
        std::printf("    load: %s\n", error.c_str());
        return nullptr;
    }
    def.air_levels.mode = mode;
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

}  // namespace

SC_TEST("levels B12: the stagger fires at the same simulation times at 0.05 s and 0.1 s, and spreads the serials") {
    for (std::uint64_t serial : {1ull, 7ull, 12345ull}) {
        int fired05 = 0, fired10 = 0;
        std::vector<double> ticks05, ticks10;
        for (int k = 1; k <= 400; ++k) {
            const double t = 0.05 * k;
            if (StaggerDue(serial, 2.0, t - 0.05, t)) {
                ++fired05;
                ticks05.push_back(std::ceil(t * 2.0 + StaggerPhase(serial) - 1e-9));  // the tick it ran for
            }
        }
        for (int k = 1; k <= 200; ++k) {
            const double t = 0.1 * k;
            if (StaggerDue(serial, 2.0, t - 0.1, t)) {
                ++fired10;
                ticks10.push_back(std::ceil(t * 2.0 + StaggerPhase(serial) - 1e-9));
            }
        }
        SC_EXPECT(fired05 == 40 || fired05 == 39 || fired05 == 41);
        SC_EXPECT(fired05 == fired10);
        SC_EXPECT(ticks05 == ticks10);
    }
    // Phases spread: 100 serials fill the ten tenths of a period.
    int bins[10] = {};
    for (std::uint64_t s = 0; s < 100; ++s) ++bins[static_cast<int>(StaggerPhase(s) * 10.0)];
    for (int b : bins) SC_EXPECT(b >= 3);
}

SC_TEST("levels B12: hysteresis, up at once, down only past 1.2 times the range after 30 s; no flicker at the edge") {
    AirLevelValues v;
    v.mode = 1;
    AirLevels lv;
    lv.Configure(v);
    // Near: A2. Past the range but inside its margin: still A2. Beyond 1.2 for 29 s: A2; at 30 s: A1.
    SC_EXPECT(!lv.Update(0, 0.5, false, 0.0) && lv.LevelOf(0) == AirLevel::A2);
    lv.Update(0, 1.1, false, 1.0);
    SC_EXPECT(lv.LevelOf(0) == AirLevel::A2);
    for (double t = 2.0; t < 31.0; t += 1.0) lv.Update(0, 1.3, false, t);
    SC_EXPECT(lv.LevelOf(0) == AirLevel::A2);
    SC_EXPECT(lv.Update(0, 1.3, false, 32.0) && lv.LevelOf(0) == AirLevel::A1);
    // Back inside the range: A2 at once. Engaged far away: A2 at once.
    SC_EXPECT(lv.Update(0, 0.99, false, 40.0) && lv.LevelOf(0) == AirLevel::A2);
    for (double t = 41.0; t < 80.0; t += 1.0) lv.Update(0, 1.5, false, t);
    SC_EXPECT(lv.LevelOf(0) == AirLevel::A1);
    SC_EXPECT(lv.Update(0, 1.5, true, 80.0) && lv.LevelOf(0) == AirLevel::A2);
    // A route that runs along the edge (0.9 to 1.25, never 30 s beyond 1.2) never changes.
    AirLevels edge;
    edge.Configure(v);
    for (int k = 0; k < 2000; ++k) {
        const double t = 0.5 * k;
        const double ratio = 1.075 + 0.175 * std::sin(t / 7.0);  // beyond 1.2 for under 25 s of each 44 s
        edge.Update(3, ratio, false, t);
    }
    SC_EXPECT(edge.LevelOf(3) == AirLevel::A2 && edge.EntryOf(3)->changes == 0);
    // A flight that enters far away starts at A1; mode 0 keeps every flight at A2.
    SC_EXPECT(lv.Update(5, 3.0, false, 100.0) && lv.LevelOf(5) == AirLevel::A1);
    AirLevels off;
    off.Configure(AirLevelValues());
    off.Update(0, 50.0, false, 0.0);
    for (double t = 0.0; t < 100.0; t += 1.0) off.Update(0, 50.0, false, t);
    SC_EXPECT(off.LevelOf(0) == AirLevel::A2);
    // The near ratio: the nearest point's distance over its reach and margin.
    const std::vector<NearPoint> pts = {{Vec3{0.0, 0.0, 0.0}, 45000.0}, {Vec3{100000.0, 0.0, 0.0}, 20000.0}};
    SC_EXPECT_NEAR(NearRatio(Vec3{0.0, 55000.0, 3000.0}, pts, 10000.0), 1.0, 1e-9);
    SC_EXPECT_NEAR(NearRatio(Vec3{85000.0, 0.0, 3000.0}, pts, 10000.0), 0.5, 1e-9);
}

SC_TEST("levels B12: the data's values load, and a data folder without them keeps every aircraft at A2") {
    AirLevelValues v;
    std::string e;
    SC_EXPECT(LoadAirLevelValues(kData, "gameplay/air_levels.json", v, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    // Shipped off (mode 0) until a balance decision (02 section 5.4); every other value is read.
    SC_EXPECT(v.mode == 0 && v.radar_m > 10000.0 && v.down_factor >= 1.0 && v.down_dwell_s > 0.0);
    AirLevelValues none;
    SC_EXPECT(LoadAirLevelValues(kData, "gameplay/no_such_file.json", none, &e) && none.mode == 0);
}

SC_TEST("levels B12: a flight out and back keeps its route place, slots, receivers and pictures across A2 to A1 to A2") {
    // North from 30 km to 160 km and back to 20 km, 40 km east: out of the near range (45 + 10 km) and back in. The
    // practice drone comes after the end: with it, the Osa's launch at it is heard on the flight's receivers 124 km away,
    // and a launch on a receiver is a pilot's reason to be at A2 however far (A2 for a minute, then A1 again: four changes).
    // The output below says why each change was made.
    auto r = Runner(ReplaceOnce(Session(1, {30.0, 160.0, 20.0}, 1500.0, true, 40000.0), "\"appear_at_s\": 200", "\"appear_at_s\": 5000"), 1);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    bool was_a1 = false, back_a2 = false;
    std::size_t pic_at_a1[5] = {};
    bool shut[5] = {};
    bool pic_kept = true, rwr_off = true, lead_rwr = true, slot_kept = true, place_kept = true;
    int placed_steps = 0;
    double worst_station_m = 0.0;
    AirLevel last = AirLevel::A2;
    double worst_placed_m = 0.0;
    // His station by the flight's own rule (the formation's slot for the flight's phase, off the leader's track).
    auto station_of = [&](int k) {
        const FlightState* f = r->AllFlights().Find("f1");
        const FlightSlot* me = nullptr;
        for (std::size_t n = 0; f != nullptr && n < f->slots.size(); ++n) {
            if (f->slots[n].number == k) me = &f->slots[n];
        }
        const Aircraft* L = r->GetWorld().FindAircraft(r->TargetWorldId("f1_1"));
        if (f == nullptr || me == nullptr || L == nullptr) return Vec3{1e9, 1e9, 0.0};
        return Flights::StationOf(*L, Flights::SlotOffset(*f, *me));
    };
    while (!r->Finished() && r->Time() < 1300.0) {
        r->Step(0.05);
        const AirLevel l = r->FlightLevel("f1");
        if (l != last) {
            // Why it changed: what the members hold (a manoeuvre, a recent launch, trail, burst or lock in a picture).
            std::string why;
            const FlightState* f = r->AllFlights().Find("f1");
            for (std::size_t n = 0; f != nullptr && n < f->slots.size(); ++n) {
                if (f->slots[n].overriding) why += " slot" + std::to_string(n) + ":override";
            }
            for (int k = 1; k <= 4; ++k) {
                for (const airwar::Sighting& sg : r->PilotPicture("f1_" + std::to_string(k))) {
                    if (sg.time_s < r->Time() - 60.0 || sg.kind == airwar::SightKind::Aircraft) continue;
                    why += " f1_" + std::to_string(k) + ":" + sg.what;
                }
            }
            std::printf("    t %.0f s, the leader %.0f km north: %s to %s;%s\n", r->Time(), r->TargetPosition("f1_1").y / 1000.0,
                        AirLevelName(last), AirLevelName(l), why.c_str());
            last = l;
        }
        if (l == AirLevel::A1) was_a1 = true;
        else if (was_a1) back_a2 = true;
        lead_rwr = lead_rwr && (l != AirLevel::A1 || r->Em()->RwrOn("f1_1"));
        for (int k = 2; k <= 4; ++k) {
            const std::string id = "f1_" + std::to_string(k);
            if (r->TargetLevel(id) == AirLevel::A1) {
                ++placed_steps;
                if (!shut[k]) pic_at_a1[k] = r->PilotPicture(id).size();
                shut[k] = true;
                pic_kept = pic_kept && r->PilotPicture(id).size() == pic_at_a1[k];  // his eyes shut, his picture as it was
                rwr_off = rwr_off && !r->Em()->RwrOn(id);
                // Placed on his station, on the leader's place on the plan.
                const Vec3 W = r->TargetPosition(id), S = station_of(k);
                worst_placed_m = std::max(worst_placed_m, std::hypot(S.x - W.x, S.y - W.y));
                slot_kept = slot_kept && std::hypot(S.x - W.x, S.y - W.y) < 1.0;
                place_kept = place_kept && r->TargetNextWaypoint(id) == r->TargetNextWaypoint("f1_1");
            } else {
                shut[k] = false;
                if (back_a2 && k == 2 && r->TargetPosition("f1_1").y < 40000.0) {
                    // Back at A2 he flies his slot himself: within a few hundred metres of it while the flight flies straight.
                    const Vec3 W = r->TargetPosition(id), S = station_of(k);
                    worst_station_m = std::max(worst_station_m, std::hypot(S.x - W.x, S.y - W.y));
                }
            }
        }
    }
    std::printf("    level changes %d, wingman-steps placed %d; kept: picture %d, receivers off %d, the leader's on %d, slot %d, "
                "route place %d; worst off station placed %.2f m, back at A2 %.0f m\n",
                r->LevelChanges(), placed_steps, pic_kept, rwr_off, lead_rwr, slot_kept, place_kept, worst_placed_m, worst_station_m);
    SC_EXPECT(was_a1 && back_a2);
    SC_EXPECT(placed_steps > 1000);
    SC_EXPECT(pic_kept && rwr_off && lead_rwr && slot_kept && place_kept);
    SC_EXPECT(worst_station_m < 300.0);
    SC_EXPECT(r->Em()->RwrOn("f1_2"));  // his receiver back on at A2
    // Down once, up once: no flicker at the edges.
    SC_EXPECT(r->LevelChanges() == 2);
}

SC_TEST("levels B12: near the battery the levels change nothing: the debrief is the same with the levels on and off") {
    // Three flights that stay within 45 km of the unit (and the drone): every flight at A2 throughout.
    const std::string text = Session(3, {40.0, 5.0, -40.0}, 700.0);
    auto off = Runner(text, 0);
    auto on = Runner(text, 1);
    SC_EXPECT(off != nullptr && on != nullptr);
    if (!off || !on) return;
    off->RunToEnd(0.05, 700.0);
    on->RunToEnd(0.05, 700.0);
    SC_EXPECT(on->LevelChanges() == 0);
    SC_EXPECT(off->Debrief() == on->Debrief());
    SC_EXPECT(off->Launches() == on->Launches());
}

SC_TEST("levels B12: a session with the levels on, run twice with one seed, writes the same debrief") {
    const std::string text = Session(2, {30.0, 160.0, 20.0}, 900.0);
    auto a = Runner(text, 1);
    auto b = Runner(text, 1);
    SC_EXPECT(a != nullptr && b != nullptr);
    if (!a || !b) return;
    a->RunToEnd(0.05, 900.0);
    b->RunToEnd(0.05, 900.0);
    SC_EXPECT(a->LevelChanges() > 0);
    SC_EXPECT(a->Debrief() == b->Debrief());
}

SC_TEST("levels B12: 100 aircraft far away at A1 do a fraction of the work at A2 (counts), and the levels cost little (time, loose)") {
    // 25 flights of four, 90 to 200 km north: far from the unit throughout.
    const std::string text = Session(25, {90.0, 200.0, 90.0}, 400.0);
    std::uint64_t ticks[2] = {0, 0};
    StepProfile prof[2];
    for (int mode = 0; mode < 2; ++mode) {
        auto r = Runner(text, mode);
        SC_EXPECT(r != nullptr);
        if (!r) return;
        while (r->Time() < 60.0) r->Step(0.05);
        r->SetProfile(true);
        const std::uint64_t t0 = r->PerceptionTicks();
        while (r->Time() < 180.0) r->Step(0.05);
        ticks[mode] = r->PerceptionTicks() - t0;
        prof[mode] = r->Profile();
        if (mode == 1) SC_EXPECT(r->FlightLevel("f1") == AirLevel::A1 && r->FlightLevel("f25") == AirLevel::A1);
    }
    std::printf("    perception ticks in 120 s: %llu at A2, %llu at A1; step %.3f ms and %.3f ms mean, the levels %.4f ms\n",
                static_cast<unsigned long long>(ticks[0]), static_cast<unsigned long long>(ticks[1]), prof[0].MeanStepMs(), prof[1].MeanStepMs(),
                prof[1].MeanMs(StepProfile::Levels));
    SC_EXPECT(ticks[1] * 3 < ticks[0]);  // the leaders alone look: a quarter, at their own rates
    SC_EXPECT(prof[1].MeanMs(StepProfile::Levels) < 0.5);  // ten times what it takes on a quiet machine: a loose guard
}
