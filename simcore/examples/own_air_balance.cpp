// own_air_balance: our fighters and our own fire, the exercise sweep (roadmap S1, docs/air_battle/08_OUR_FIGHTERS.md
// section 8, "Calibration").
//
// Usage: own_air_balance [--trace] [data_dir] [seeds] [aircraft]
//   --trace: each session's own-aviation and IFF lines to stderr; data_dir (default data); seeds (default 20); aircraft a
//   session (default 5).
//
// Each seed is one exercise session: an Osa-AK deployed at a halt with its NPC crew, and aircraft crossing its zone from
// all sides, each of ours by a chance (a game value: the exercise's mix is not given by the source), the rest the
// "enemy". The same sessions are flown under four settings:
//   1960s    - notices only, sent 10-15 min after our flights began to cross, our transponders off (no code to read);
//   notices  - our transponders off, each notice sent an hour ahead (the chain's way from the fighter regiment to the
//              regiment's post is about 45 min, chain_pannonia.json), so in the crew's hands before our flight crosses;
//   late     - the day's codes working (our pilots on the table), the notices late;
//   working  - the codes working and the notices an hour ahead.
// With neither codes nor timely notices the crew cannot tell ours, so the share of ours among the targets fired on is
// about the share of ours in the air: the 25% anchor is met by the mix (kOursShare), not found independently. What the
// sweep shows is the mechanism: how much of that share the notices and the codes each take away.
// For each it prints the targets fired on (a launch with the target tracked), ours among them, and the share; the share
// under 1960s settings is checked against mt_turchin_coop's "up to 25%" (a Baltic exercise, pp. 11-12), the working
// share against a low band, a game value (data/air_war/balance_bands.json, tool "own_air_balance").
// Exits 1 if the data fail to load, 2 if a share is out of its band.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "simcore/air_war/balance_bands.hpp"
#include "simcore/command/own_air.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

using namespace simcore;

namespace {

constexpr double kOursShare = 0.3;    // a game value: the chance an aircraft in the exercise is ours (the exercise's mix, unknown)
constexpr double kSessionS = 1200.0;

struct Crossing {
    std::string id;
    bool ours = false;
    double bearing_rad = 0.0;
    double miss_m = 0.0;
    double up_m = 0.0;
    double speed_mps = 0.0;
    double appear_s = 0.0;
};

std::vector<Crossing> Draw(unsigned seed, int n) {
    std::mt19937 rng(seed * 2654435761u + 0xB10Au);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    std::vector<Crossing> out;
    for (int k = 0; k < n; ++k) {
        Crossing c;
        c.ours = u(rng) < kOursShare;
        c.id = (c.ours ? "ours_" : "enemy_") + std::to_string(k + 1);
        c.bearing_rad = u(rng) * 2.0 * 3.14159265358979;
        c.miss_m = (u(rng) * 2.0 - 1.0) * 4000.0;
        c.up_m = 400.0 + u(rng) * 2600.0;
        c.speed_mps = 150.0 + u(rng) * 100.0;
        c.appear_s = 10.0 + u(rng) * 500.0;
        out.push_back(c);
    }
    return out;
}

std::string Num(double v) {
    char b[32];
    std::snprintf(b, sizeof(b), "%.0f", v);
    return b;
}

// The session's JSON for a setting: 0 the 1960s, 1 timely notices without codes, 2 codes but late notices, 3 working.
std::string Session(unsigned seed, const std::vector<Crossing>& cs, int setting) {
    std::string targets = "[";
    std::string notices = "[";
    for (std::size_t i = 0; i < cs.size(); ++i) {
        const Crossing& c = cs[i];
        const double dx = std::sin(c.bearing_rad), dy = std::cos(c.bearing_rad);
        const double px = -dy * c.miss_m, py = dx * c.miss_m;  // the point of closest approach
        // Each end on a PVO grid square's centre, as a notice's route is written (own_air.hpp NoticeWords), so the corridor
        // the crew reads back is the one flown.
        static const LocalFrame frame(GeoPoint::FromDegrees(46.9, 19.7, 110.0));
        const auto snap = [](double e, double n) {
            const Vec3 p{e, n, 0.0};
            Vec3 centre = p;
            command::ReadSquare(frame, command::SquareOf(frame, p), p, centre);
            return centre;
        };
        const Vec3 a = snap(px + dx * 22000.0, py + dy * 22000.0);
        const Vec3 b = snap(px - dx * 22000.0, py - dy * 22000.0);
        const double e0 = a.x, n0 = a.y, e1 = b.x, n1 = b.y;
        if (i > 0) targets += ",";
        targets += "{ \"id\": \"" + c.id + "\", \"name\": \"" + (c.ours ? "Our MiG-21" : "Enemy fighter-bomber") + " (placeholder)\", \"class\": \"fighter\", ";
        if (c.ours) {
            targets += "\"iff\": \"friend\", ";
            targets += setting <= 1 ? "\"iff_code\": -1, " : "\"iff_table\": true, ";
        }
        targets += "\"start\": { \"east_m\": " + Num(e0) + ", \"north_m\": " + Num(n0) + ", \"up_m\": " + Num(c.up_m) + " }, \"waypoints\": [ { \"east_m\": " +
                   Num(e1) + ", \"north_m\": " + Num(n1) + ", \"up_m\": " + Num(c.up_m) + " } ], \"speed_mps\": " + Num(c.speed_mps) +
                   ", \"rcs_m2\": 3, \"appear_at_s\": " + Num(c.appear_s) + " }";
        if (!c.ours) continue;
        const double cross_s = std::hypot(e1 - e0, n1 - n0) / c.speed_mps;
        std::mt19937 late(seed * 7919u + static_cast<unsigned>(i));
        const double sent = setting == 1 || setting == 3 ? c.appear_s - 3600.0 : c.appear_s + 600.0 + std::uniform_real_distribution<double>(0.0, 300.0)(late);
        if (notices.size() > 1) notices += ",";
        notices += "{ \"flight\": \"Sokol " + std::to_string(i + 1) + "\", \"type\": \"MiG-21\", \"count\": 1, \"route_m\": [ [ " + Num(e0) + ", " + Num(n0) +
                   " ], [ " + Num(e1) + ", " + Num(n1) + " ] ], \"half_width_m\": 5000, \"floor_m\": " + Num(std::max(0.0, c.up_m - 500.0)) +
                   ", \"ceiling_m\": " + Num(c.up_m + 500.0) + ", \"cross_from_s\": " + Num(c.appear_s) + ", \"cross_to_s\": " + Num(c.appear_s + cross_s) +
                   ", \"sent_s\": " + Num(sent) + ", \"aircraft\": [ \"" + c.id + "\" ] }";
    }
    targets += "]";
    notices += "]";
    return R"JSON({ "scenario_id": "own_air_balance", "name": "Our aviation: the exercise sweep", "source": "sam3d", "confidence": "estimate", "seed": )JSON" +
           std::to_string(seed) +
           R"JSON(, "clock_start": "10:00", "origin": { "lat_deg": 46.9, "lon_deg": 19.7, "alt_m": 110 }, "terrain": { "type": "flat", "height_m": 110 },
      "crew_commander": "npc",
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", "readiness": "deployed",
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 } },
      "crew": [ { "name": "Commander", "role": "commander", "skill": "class1" }, { "name": "Deputy commander", "role": "deputy_commander", "skill": "class1" },
                { "name": "Senior operator", "role": "senior_operator", "skill": "class1" }, { "name": "Operator", "role": "operator", "skill": "class1" },
                { "name": "Driver", "role": "driver", "skill": "class1" } ],
      "targets": )JSON" + targets + R"JSON(,
      "own_air": { "source": "sam3d", "confidence": "estimate", "notes": "the exercise sweep", "war": true, "notices": )JSON" + notices + R"JSON( },
      "triggers": [], "end": { "time_limit_s": )JSON" + Num(kSessionS) + " } }";
}

struct Tally {
    int fired = 0;       // targets fired on
    int ours_fired = 0;
    int ours = 0;        // ours flown
    int sessions = 0;
    int read = 0;        // notices the crews read
};

const char* const kNames[] = {"1960s", "notices", "late", "working"};

}  // namespace

int main(int argc, char** argv) {
    const bool trace = argc > 1 && std::string(argv[1]) == "--trace";
    const int a0 = trace ? 2 : 1;
    const std::string data_dir = argc > a0 ? argv[a0] : "data";
    const int seeds = argc > a0 + 1 ? std::atoi(argv[a0 + 1]) : 20;
    const int per = argc > a0 + 2 ? std::atoi(argv[a0 + 2]) : 5;
    airwar::BalanceBands bands;
    std::string be;
    if (!airwar::LoadBalanceBands(data_dir + "/air_war/balance_bands.json", bands, &be)) {
        std::fprintf(stderr, "%s\n", be.c_str());
        return 1;
    }
    Tally tally[4];
    for (int s = 1; s <= seeds; ++s) {
        const unsigned seed = static_cast<unsigned>(s);
        const std::vector<Crossing> cs = Draw(seed, per);
        for (int setting = 0; setting < 4; ++setting) {
            ScenarioDef def;
            ConsoleData console;
            std::string e;
            if (!LoadScenario(Session(seed, cs, setting), data_dir, def, console, &e)) {
                std::fprintf(stderr, "seed %d, %s: %s\n", s, kNames[setting], e.c_str());
                return 1;
            }
            ScenarioRunner r(std::move(def), std::move(console));
            std::set<std::string> fired;
            int launches = 0;
            while (!r.Finished() && r.Time() < kSessionS) {
                r.Step(0.1);
                if (r.Launches() > launches) {
                    launches = r.Launches();
                    const std::string t = r.Engagement().TrackedTarget();
                    if (!t.empty()) fired.insert(t);
                }
            }
            Tally& ta = tally[setting];
            ++ta.sessions;
            for (const Crossing& c : cs) ta.ours += c.ours ? 1 : 0;
            ta.read += static_cast<int>(r.CrewNotices().size());
            if (trace) {  // the session's own-aviation and IFF lines, to stderr
                for (const DebriefEntry& en : r.Log().Entries()) {
                    if (en.who == "own aviation" || en.what == "own aviation" || en.who == "IFF" || en.what == "IFF") {
                        std::fprintf(stderr, "  [%s %d] %.0f s %s: %s %s\n", kNames[setting], s, en.time_s, en.who.c_str(), en.what.c_str(), en.detail.c_str());
                    }
                }
            }
            for (const std::string& t : fired) {
                ++ta.fired;
                ta.ours_fired += t.rfind("ours_", 0) == 0 ? 1 : 0;
            }
        }
        std::fprintf(stderr, "seed %d done\n", s);
    }
    std::printf("# Our own fire: the exercise sweep (%d sessions of %d aircraft, ours by a chance of %.2f)\n\n", seeds, per, kOursShare);
    std::printf("| Settings | Ours flown | Notices read | Targets fired on | Ours among them | Share | Band | Anchor | |\n|---|---|---|---|---|---|---|---|---|\n");
    int out = 0;
    const char* const ids[] = {"own_fired_share_1960s", "own_fired_share_notices", "own_fired_share_late", "own_fired_share_working"};
    for (int k = 0; k < 4; ++k) {
        const Tally& ta = tally[k];
        const double share = ta.fired > 0 ? static_cast<double>(ta.ours_fired) / ta.fired : 0.0;
        const airwar::BalanceBand* b = bands.Find("own_air_balance", ids[k]);
        std::string band = "-", anchor = "-", mark;
        if (b != nullptr) {
            char x[48];
            std::snprintf(x, sizeof(x), "%.2f-%.2f", b->lo, b->hi);
            band = x;
            anchor = b->source;
            const bool in = share >= b->lo && share <= b->hi;
            mark = in ? "in" : "OUT";
            if (!in) ++out;
        }
        std::printf("| %s | %d | %d | %d | %d | %.2f | %s | %s | %s |\n", kNames[k], ta.ours, ta.read, ta.fired, ta.ours_fired, share, band.c_str(), anchor.c_str(), mark.c_str());
    }
    return out > 0 ? 2 : 0;
}
