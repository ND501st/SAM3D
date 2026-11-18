// Roadmap B12: the levels of detail of a session's aircraft (simcore/air_levels.hpp; the runner's A1;
// docs/air_battle/02_FLIGHTS_AND_PILOTS.md §5). Every number here is a generic placeholder, not data of a real aircraft.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
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

std::unique_ptr<ScenarioRunner> Runner(const std::string& text, int mode, int seed = -1) {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenario(text, kData, def, console, &error)) {
        std::printf("    load: %s\n", error.c_str());
        return nullptr;
    }
    def.air_levels.mode = mode;
    if (seed >= 0) def.seed = static_cast<unsigned>(seed);
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

// Does a pilot's picture hold a receiver's contact heard on his leader's receiver, seen since `since_s`?
bool HoldsBorrowed(const ScenarioRunner& r, const std::string& id, double since_s) {
    for (const airwar::Sighting& sg : r.PilotPicture(id)) {
        if (sg.borrowed && sg.time_s >= since_s) return true;
    }
    return false;
}

// A debrief split in two: its warning receivers' lines (who and what, without the time, sorted) and every other line.
// The tolerance of a hand-over from A1: a wingman whose own receiver is off at A1 may hear a lock or a missile's guidance
// at another moment than his own receiver would have (his leader's, 1-2 km away, heard it, or did not yet); everything
// else, places, launches and every other line, is the same.
struct DebriefParts {
    std::vector<std::string> warnings;
    std::string rest;
};
DebriefParts SplitWarnings(const std::string& debrief) {
    DebriefParts p;
    std::istringstream in(debrief);
    std::string l;
    while (std::getline(in, l)) {
        const std::size_t w = l.find("warning receiver: ");
        const std::size_t s = l.find(" s ");
        if (w == std::string::npos || s == std::string::npos) {
            p.rest += l + "\n";
            continue;
        }
        p.warnings.push_back(l.substr(s + 3));
    }
    std::sort(p.warnings.begin(), p.warnings.end());
    return p;
}
// Lines of the warning receivers that differ in time between two debriefs are a wingman's (not slot 1's: "_1 ").
bool WarningsMovedOnlyForWingmen(const std::string& a, const std::string& b) {
    std::vector<std::string> x, y;
    {
        std::istringstream in(a);
        std::string l;
        while (std::getline(in, l))
            if (l.find("warning receiver: ") != std::string::npos) x.push_back(l);
    }
    {
        std::istringstream in(b);
        std::string l;
        while (std::getline(in, l))
            if (l.find("warning receiver: ") != std::string::npos) y.push_back(l);
    }
    std::sort(x.begin(), x.end());
    std::sort(y.begin(), y.end());
    std::vector<std::string> d;
    std::set_symmetric_difference(x.begin(), x.end(), y.begin(), y.end(), std::back_inserter(d));
    for (const std::string& l : d) {
        const std::size_t w = l.find("  warning receiver: ");
        if (w == std::string::npos || w < 2 || l.compare(w - 2, 2, "_1") == 0) return false;
    }
    return true;
}

// The levels off and on side by side, `flights` flights of four: what the run with the levels on did at A1 and at the
// hand-over, and where the two runs parted.
struct SideBySide {
    bool ok = false;
    bool went_a1 = false, came_back = false;  // a flight at A1, then back at A2 and within 55 km of the unit
    int lent_steps = 0;                       // wingman-steps hearing on the leader's receiver
    bool borrowed = false;                    // a wingman's picture held a search heard on his leader's receiver
    bool own_off = true, lead_on = true;      // at A1 the wingmen's receivers off, the leaders' on
    bool borrowed_after = false;              // back at A2 (after the warm-up), a contact still heard on the leader's
    double worst_m = 0.0;                     // the farthest any aircraft was from itself in the other run
    double first_apart_s = -1.0;              // when the two first differed by more than a millimetre
    std::string debrief_off, debrief_on;
    int launches_off = 0, launches_on = 0;
};

SideBySide RunSideBySide(const std::string& text, int flights, double end_s, int seed = -1) {
    SideBySide out;
    auto off = Runner(text, 0, seed);
    auto on = Runner(text, 1, seed);
    if (!off || !on) return out;
    std::vector<AirLevel> last(static_cast<std::size_t>(flights), AirLevel::A2);
    std::vector<double> back_at(static_cast<std::size_t>(flights), -1.0);
    while (!off->Finished() && !on->Finished() && off->Time() < end_s) {
        off->Step(0.05);
        on->Step(0.05);
        for (int f = 0; f < flights; ++f) {
            const std::size_t fk = static_cast<std::size_t>(f);
            const std::string fid = "f" + std::to_string(f + 1);
            const AirLevel l = on->FlightLevel(fid);
            if (l == AirLevel::A1) out.went_a1 = true;
            if (l == AirLevel::A2 && last[fk] == AirLevel::A1) back_at[fk] = on->Time();
            last[fk] = l;
            for (int k = 1; k <= 4; ++k) {
                const std::string id = fid + "_" + std::to_string(k);
                const Aircraft* a_on = on->TargetAircraft(id);
                const Aircraft* a_off = off->TargetAircraft(id);
                if (a_on == nullptr || a_off == nullptr) continue;
                const double d = (a_on->position - a_off->position).Length();
                out.worst_m = std::max(out.worst_m, d);
                if (d > 1e-3 && out.first_apart_s < 0.0) out.first_apart_s = on->Time();
                if (k == 1) {
                    out.lead_on = out.lead_on && (l != AirLevel::A1 || on->Em()->RwrOn(id));
                    if (back_at[fk] > 0.0 && std::hypot(a_on->position.x, a_on->position.y) < 55000.0) out.came_back = true;
                    continue;
                }
                if (on->TargetLevel(id) == AirLevel::A1) {
                    ++out.lent_steps;
                    out.own_off = out.own_off && !on->Em()->RwrOn(id);
                    out.borrowed = out.borrowed || HoldsBorrowed(*on, id, on->Time() - 0.5);
                } else if (back_at[fk] > 0.0 && on->Time() > back_at[fk] + 2.0) {
                    out.borrowed_after = out.borrowed_after || HoldsBorrowed(*on, id, on->Time() - 0.5);
                }
            }
        }
    }
    out.debrief_off = off->Debrief();
    out.debrief_on = on->Debrief();
    out.launches_off = off->Launches();
    out.launches_on = on->Launches();
    out.ok = true;
    return out;
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
    // Shipped on (mode 1) since the second pass (02 section 5.4); every other value is read.
    SC_EXPECT(v.mode == 1 && v.radar_m > 10000.0 && v.down_factor >= 1.0 && v.down_dwell_s > 0.0);
    AirLevelValues none;
    SC_EXPECT(LoadAirLevelValues(kData, "gameplay/no_such_file.json", none, &e) && none.mode == 0);
}

SC_TEST("levels B12: a flight out and back flies as with the levels off; at A1 its wingmen hear on the leader's receiver") {
    // North from 30 km to 160 km and back to 20 km, 40 km east: out of the near range (45 + 10 km) and back in. The
    // practice drone comes after the end. With the levels on and off side by side: every aircraft in the same place at
    // every step, the wingmen's receivers off at A1 and the searches heard on the leader's, none of them heard on the
    // leader's once back at A2, and the same debrief.
    const std::string text = ReplaceOnce(Session(1, {30.0, 160.0, 20.0}, 1500.0, true, 40000.0), "\"appear_at_s\": 200", "\"appear_at_s\": 5000");
    const SideBySide s = RunSideBySide(text, 1, 1300.0);
    SC_EXPECT(s.ok);
    std::printf("    A1 %d, back near %d, wingman-steps lent %d, borrowed %d, own receivers off %d, the leader's on %d, borrowed after %d; "
                "worst apart %.4f m (first at %.1f s)\n",
                s.went_a1, s.came_back, s.lent_steps, s.borrowed, s.own_off, s.lead_on, s.borrowed_after, s.worst_m, s.first_apart_s);
    SC_EXPECT(s.went_a1 && s.came_back);
    SC_EXPECT(s.lent_steps > 1000);
    SC_EXPECT(s.borrowed);  // the Osa's search radar is heard on the leader's receiver over 100 km away
    SC_EXPECT(s.own_off && s.lead_on && !s.borrowed_after);
    SC_EXPECT(s.worst_m < 1e-6);  // flown, not placed: the same place at every step
    SC_EXPECT(s.debrief_off == s.debrief_on);
    SC_EXPECT(s.launches_off == s.launches_on);
}

SC_TEST("levels B12: flights that go far and come back to the battery play as with the levels off (cases by property)") {
    // Routes out of the near range and back over the unit, at several distances east and seeds. Every case in which a
    // flight went to A1, came back within 55 km of the unit and its wingmen heard searches on the leader's receiver is
    // compared with the levels off: every aircraft in the same place at every step, the same launches, and the same
    // debrief within the tolerance of SplitWarnings (a wingman's warning-receiver line may move in time, not appear or go).
    struct Case {
        std::vector<double> route_km;
        double east_m;
        int seed;
    };
    const Case cases[] = {{{30.0, 160.0, -20.0}, 20000.0, 1}, {{25.0, 140.0, 0.0}, 0.0, 2}, {{40.0, 180.0, 10.0}, -30000.0, 3}, {{-30.0, -150.0, 5.0}, 10000.0, 4}};
    int compared = 0, same = 0;
    for (const Case& c : cases) {
        const SideBySide s = RunSideBySide(Session(2, c.route_km, 1500.0, true, c.east_m), 2, 1400.0, c.seed);
        SC_EXPECT(s.ok);
        const bool qualifies = s.went_a1 && s.came_back && s.borrowed;
        const bool identical = s.debrief_off == s.debrief_on && s.launches_off == s.launches_on;
        std::printf("    case %.0f km east, seed %d: A1 %d, back %d, borrowed %d; worst apart %.4f m; launches %d and %d; debrief %s\n", c.east_m / 1000.0,
                    c.seed, s.went_a1, s.came_back, s.borrowed, s.worst_m, s.launches_off, s.launches_on, identical ? "the same" : "differs");
        if (!identical) {
            // Where the two parted, for the record.
            std::istringstream x(s.debrief_off), y(s.debrief_on);
            std::string lx, ly;
            int line = 0;
            while (std::getline(x, lx) && std::getline(y, ly) && lx == ly) ++line;
            std::printf("      line %d:\n        %s\n        %s\n", line, lx.c_str(), ly.c_str());
        }
        if (!qualifies) continue;
        ++compared;
        if (identical) ++same;
        SC_EXPECT(s.own_off && s.lead_on && !s.borrowed_after);
        SC_EXPECT(s.worst_m < 1e-6);
        SC_EXPECT(s.launches_off == s.launches_on);
        const DebriefParts a = SplitWarnings(s.debrief_off), b = SplitWarnings(s.debrief_on);
        SC_EXPECT(a.rest == b.rest);
        SC_EXPECT(a.warnings == b.warnings);
        SC_EXPECT(WarningsMovedOnlyForWingmen(s.debrief_off, s.debrief_on));
    }
    std::printf("    %d of %d cases identical, the rest within the tolerance\n", same, compared);
    SC_EXPECT(compared >= 3);
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

SC_TEST("levels B12: 100 aircraft far away at A1 hear on a quarter of the receivers, and the levels cost little (time, loose)") {
    // 25 flights of four, 90 to 200 km north: far from the unit throughout. Every aircraft flies and looks at both levels;
    // at A1 three of each four receivers are off (the wingmen hear on the leader's), so the EM world runs a quarter of them.
    const std::string text = Session(25, {90.0, 200.0, 90.0}, 400.0);
    int on[2] = {0, 0};
    std::uint64_t ticks[2] = {0, 0};
    StepProfile prof[2];
    for (int mode = 0; mode < 2; ++mode) {
        auto r = Runner(text, mode);
        SC_EXPECT(r != nullptr && r->Em() != nullptr);
        if (!r || r->Em() == nullptr) return;
        while (r->Time() < 60.0) r->Step(0.05);
        r->SetProfile(true);
        const std::uint64_t t0 = r->PerceptionTicks();
        while (r->Time() < 180.0) r->Step(0.05);
        ticks[mode] = r->PerceptionTicks() - t0;
        prof[mode] = r->Profile();
        for (int f = 1; f <= 25; ++f)
            for (int k = 1; k <= 4; ++k)
                if (r->Em()->RwrOn("f" + std::to_string(f) + "_" + std::to_string(k))) ++on[mode];
        if (mode == 1) SC_EXPECT(r->FlightLevel("f1") == AirLevel::A1 && r->FlightLevel("f25") == AirLevel::A1);
    }
    std::printf("    receivers on: %d at A2, %d at A1; perception ticks in 120 s %llu and %llu; step %.3f ms and %.3f ms mean, the levels %.4f ms\n",
                on[0], on[1], static_cast<unsigned long long>(ticks[0]), static_cast<unsigned long long>(ticks[1]), prof[0].MeanStepMs(),
                prof[1].MeanStepMs(), prof[1].MeanMs(StepProfile::Levels));
    SC_EXPECT(on[0] == 100);
    SC_EXPECT(on[1] == 25);               // the leaders' receivers alone
    SC_EXPECT(ticks[1] == ticks[0]);      // every pilot looks at A1 as at A2
    SC_EXPECT(prof[1].MeanMs(StepProfile::Levels) < 0.5);  // ten times what it takes on a quiet machine: a loose guard
}
