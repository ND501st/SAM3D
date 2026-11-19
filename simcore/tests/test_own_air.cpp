// Our aviation (roadmap track B, B10; docs/air_battle/08_OUR_FIGHTERS.md; simcore/command/own_air.hpp, own_flights.hpp):
// the day's identification codes and who holds them, the plan of cooperation, the notices of our flights down the chain
// and the crews' rule on them, the fighter regiments' duty scrambling pairs on our GCI's picture, the friendly-fire trace
// from the records, and what the enemy learns of our corridors. Cases are picked by property over several seeds.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/campaign/report.hpp"
#include "simcore/command/data.hpp"
#include "simcore/command/own_air.hpp"
#include "simcore/command/own_flights.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::command;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string ReadAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

OwnAirDef Rules() {
    OwnAirDef d;
    std::string e;
    const bool ok = LoadOwnAirFile(kData + "/command/own_air.json", d, &e);
    if (!ok) std::printf("    %s\n", e.c_str());
    SC_EXPECT(ok);
    return d;
}
IffCodesDef Codes() {
    IffCodesDef c;
    std::string e;
    const bool ok = LoadIffCodesFile(kData + "/command/iff_codes.json", c, &e);
    if (!ok) std::printf("    %s\n", e.c_str());
    SC_EXPECT(ok);
    return c;
}

// A ready Osa crew at a halt with its NPC commander (no EM layer), the clock at 10:00, `targets` and an `own_air` block.
std::string Session(const std::string& targets, const std::string& own_air, unsigned seed, const std::string& extra = "", double limit_s = 300.0,
                    const Vec3& unit = Vec3{}) {
    return R"JSON({ "scenario_id": "own_air", "name": "Our aviation", "source": "sam3d", "confidence": "estimate", "seed": )JSON" + std::to_string(seed) +
           R"JSON(, "clock_start": "10:00", "origin": { "lat_deg": 46.9, "lon_deg": 19.7, "alt_m": 110 }, "terrain": { "type": "flat", "height_m": 110 },
      "crew_commander": "npc",
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", "readiness": "deployed",
                    "position": { "east_m": )JSON" + std::to_string(unit.x) + R"JSON(, "north_m": )JSON" + std::to_string(unit.y) + R"JSON(, "up_m": 3 } },
      "crew": [ { "name": "Commander", "role": "commander", "skill": "class1" }, { "name": "Deputy commander", "role": "deputy_commander", "skill": "class1" },
                { "name": "Senior operator", "role": "senior_operator", "skill": "class1" }, { "name": "Operator", "role": "operator", "skill": "class1" },
                { "name": "Driver", "role": "driver", "skill": "class1" } ],
      "targets": )JSON" + targets + "," + extra + R"JSON( "own_air": )JSON" + own_air + R"JSON(,
      "triggers": [], "end": { "time_limit_s": )JSON" + std::to_string(static_cast<int>(limit_s)) + " } }";
}

std::unique_ptr<ScenarioRunner> Load(const std::string& text) {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    if (!LoadScenario(text, kData, def, console, &e)) {
        std::printf("    %s\n", e.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

// One of ours coming over the battery from the north at 1,200 m, and a hostile 3.6 km to the east of its track.
std::string OursAndHostile(const std::string& ours_iff) {
    return R"JSON([
        { "id": "ours", "name": "Our MiG-21 (placeholder)", "class": "fighter", "iff": "friend", )JSON" + ours_iff + R"JSON(
          "start": { "east_m": -600, "north_m": 9000, "up_m": 1200 }, "waypoints": [ { "east_m": -600, "north_m": -20000, "up_m": 1200 } ],
          "speed_mps": 160, "rcs_m2": 3, "appear_at_s": 20 },
        { "id": "hostile", "name": "Fighter (placeholder)", "class": "fighter", "start": { "east_m": 3000, "north_m": 11000, "up_m": 1000 },
          "waypoints": [ { "east_m": 3000, "north_m": -20000, "up_m": 1000 } ], "speed_mps": 150, "rcs_m2": 3, "appear_at_s": 20 } ])JSON";
}

struct Played {
    bool ok = false;
    int launches = 0;
    bool at_ours = false;   // a launch with ours tracked
    bool at_hostile = false;
    std::vector<FriendlyFireFinding> findings;
    std::vector<OwnFlightNotice> notices;
    int battery_code = 0;
    int ours_code = 0;
    bool ours_replied = false;
    std::vector<std::string> own_air_log;  // the log's "own aviation" and "IFF" lines (printed when a case fails)
};

Played Play(const std::string& text, double until_s = 300.0) {
    Played p;
    std::unique_ptr<ScenarioRunner> r = Load(text);
    if (!r) return p;
    p.ok = true;
    while (!r->Finished() && r->Time() < until_s) {
        r->Step(0.1);
        if (r->Launches() > p.launches) {
            p.launches = r->Launches();
            p.at_ours = p.at_ours || r->Engagement().TrackedTarget() == "ours";
            p.at_hostile = p.at_hostile || r->Engagement().TrackedTarget() == "hostile";
        }
        if (r->Time() > 25.0 && r->Time() < 25.15) {
            p.battery_code = r->BatteryIffCode();
            p.ours_code = r->TargetIffCode("ours");
        }
    }
    p.findings = r->FriendlyFire();
    p.notices = r->CrewNotices();
    p.ours_replied = r->Engagement().KnownFriend("ours");
    for (const DebriefEntry& en : r->Log().Entries()) {
        if (en.who == "own aviation" || en.what == "own aviation") p.own_air_log.push_back(en.what + ": " + en.detail);
    }
    return p;
}

}  // namespace

SC_TEST("own air B10: the day's codes change on schedule within the war's filters; a holder without the table keeps yesterday's") {
    const IffCodesDef c = Codes();
    SC_EXPECT(c.filters == 12 && c.war_first == 9 && c.war_last == 12 && c.peace_first == 1 && c.peace_last == 8);
    for (std::uint64_t seed = 1; seed <= 20; ++seed) {
        for (bool war : {true, false}) {
            const IffSchedule s = IffSchedule::Days(c, war, 0, 2, seed);
            SC_EXPECT(!s.Empty());
            const auto& ch = s.Changes();
            for (std::size_t i = 0; i < ch.size(); ++i) {
                SC_EXPECT(ch[i].filter >= (war ? c.war_first : c.peace_first) && ch[i].filter <= (war ? c.war_last : c.peace_last));
                if (i == 0) continue;
                SC_EXPECT(ch[i].filter != ch[i - 1].filter);  // a filter never follows itself
                const double gap = ch[i].from_h - ch[i - 1].from_h;
                const bool midnight = std::fabs(ch[i].from_h - std::round(ch[i].from_h / 24.0) * 24.0) < 1e-9;
                // A change within one to six hours (soviet_iff), or a new day's paper at midnight cutting a period short.
                SC_EXPECT(gap <= c.period_max_h + 1e-9 && (gap >= c.period_min_h - 1e-9 || midnight));
            }
            for (int day = 0; day <= 2; ++day) SC_EXPECT(s.Index(24.0 * day) >= 0 && s.Changes()[static_cast<std::size_t>(s.Index(24.0 * day))].from_h == 24.0 * day);
            // Without the paper: the code it had; with it, from when it came.
            IffHolding none;
            none.has_document = false;
            none.last_filter = 3;
            IffHolding late;
            late.document_h = 30.0;
            late.last_filter = 3;
            SC_EXPECT(none.FilterAt(s, 40.0) == 3);
            SC_EXPECT(late.FilterAt(s, 29.0) == 3 && late.FilterAt(s, 31.0) == s.FilterAt(31.0));
            // Pilots: none missing a change are on the table's code; one who misses every change keeps the day's first.
            for (double h = 0.5; h < 72.0; h += 0.75) {
                SC_EXPECT(PilotFilterAt(s, h, 0.0, seed, 7) == s.FilterAt(h));
                SC_EXPECT(PilotFilterAt(s, h, 1.0, seed, 7) == s.Changes()[0].filter);
            }
        }
    }
    // A reply rides the interrogation on the same system and filter only.
    SC_EXPECT(IffAnswers(IffSystem::Kremniy, 9, IffSystem::Kremniy, 9));
    SC_EXPECT(!IffAnswers(IffSystem::Kremniy, 9, IffSystem::Kremniy, 10));
    SC_EXPECT(!IffAnswers(IffSystem::Kremniy, 9, IffSystem::MarkXII, 9));
    SC_EXPECT(!IffAnswers(IffSystem::None, 9, IffSystem::Kremniy, 9));
    // Pilots who miss changes: at 3 % a missed change is rare but happens over many pilots and changes.
    const IffSchedule s = IffSchedule::Days(c, true, 0, 0, 5);
    int missed = 0, asked = 0;
    for (std::uint64_t pilot = 1; pilot <= 200; ++pilot) {
        for (const IffSchedule::Change& k : s.Changes()) {
            ++asked;
            missed += PilotFilterAt(s, k.from_h + 0.01, Rules().iff_change.pilot_miss, 5, pilot) != k.filter ? 1 : 0;
        }
    }
    std::printf("    %d of %d pilot-changes missed (3 %% each)\n", missed, asked);
    SC_EXPECT(missed > 0 && missed < asked / 10);
}

SC_TEST("own air B10: a notice said and written back is the same notice; the crews' rule on what they hold") {
    const LocalFrame frame(GeoPoint::FromDegrees(46.9, 19.7, 110.0));
    for (int k = 0; k < 40; ++k) {
        OwnFlightNotice n;
        n.number = 400 + k;
        n.amendment = k % 3;
        n.actual = k % 2 == 0;
        n.flight = "Sokol " + std::to_string(k + 1);
        n.count = 2 + k % 3;
        const double a = 0.37 * k, r = 5000.0 + 1700.0 * k;
        n.route = {Vec3{r * std::cos(a), r * std::sin(a), 0.0}, Vec3{-r * std::sin(a), r * std::cos(a), 0.0}};
        n.floor_m = 500.0 * (k % 6);
        n.ceiling_m = n.floor_m + 3000.0;
        n.cross_from_h = 10.0 + 0.13 * k;
        n.cross_to_h = n.cross_from_h + 0.5;
        n.back_h = n.cross_to_h;
        const std::string words = NoticeWords(n, frame);
        OwnFlightNotice back;
        SC_EXPECT(ReadNotice(words, frame, Vec3{}, n.cross_from_h, back));
        SC_EXPECT(back.number == n.number && back.amendment == n.amendment && back.actual == n.actual && back.flight == n.flight && back.count == n.count);
        SC_EXPECT(std::fabs(back.floor_m - n.floor_m) < 1.0 && std::fabs(back.ceiling_m - n.ceiling_m) < 1.0);
        SC_EXPECT(std::fabs(back.cross_from_h - n.cross_from_h) < 1.0 / 60.0 + 1e-9 && std::fabs(back.cross_to_h - n.cross_to_h) < 1.0 / 60.0 + 1e-9);
        SC_EXPECT(back.route.size() == n.route.size());
        // Said as the PVO grid's small squares: the point read back is within half a square's diagonal.
        for (std::size_t i = 0; i < back.route.size() && i < n.route.size(); ++i) SC_EXPECT((back.route[i] - n.route[i]).Length() < 12000.0);
    }
    // The rule (section 5), each case by its definition.
    OwnFlightNotice n;
    n.flight = "Sokol 1";
    n.route = {Vec3{0.0, 20000.0, 0.0}, Vec3{0.0, -20000.0, 0.0}};
    n.half_width_m = 3000.0;
    n.floor_m = 2000.0;
    n.ceiling_m = 5000.0;
    n.cross_from_h = 10.0;
    n.cross_to_h = 11.0;
    CooperationPlan none;
    const auto judge = [&](double e, double alt, double h, bool replied, const std::vector<OwnFlightNotice>& known, const CooperationPlan& plan) {
        TrackSeen t;
        t.pos = Vec3{e, 0.0, 0.0};
        t.alt_m = alt;
        t.clock_h = h;
        t.replied = replied;
        return JudgeTrack(t, known, plan, 0.0, 0.0);
    };
    SC_EXPECT(judge(1000.0, 3000.0, 10.5, true, {n}, none) == OwnAirCall::Friend);
    SC_EXPECT(judge(9000.0, 3000.0, 10.5, true, {n}, none) == OwnAirCall::Violator);   // a friend's reply off every notice
    SC_EXPECT(judge(1000.0, 3000.0, 10.5, false, {n}, none) == OwnAirCall::Hold);      // silent inside the corridor
    SC_EXPECT(judge(1000.0, 1000.0, 10.5, false, {n}, none) == OwnAirCall::Free);      // below its heights
    SC_EXPECT(judge(1000.0, 3000.0, 12.0, false, {n}, none) == OwnAirCall::Free);      // after its hours
    SC_EXPECT(judge(9000.0, 3000.0, 10.5, false, {n}, none) == OwnAirCall::Free);      // beside it
    SC_EXPECT(judge(9000.0, 3000.0, 10.5, true, {}, none) == OwnAirCall::Friend);      // no notices at all: a reply is a friend, as before
    CooperationPlan one;
    one.present = true;
    one.method = CooperationMethod::OneZone;
    one.split_height_m = 5000.0;
    SC_EXPECT(judge(9000.0, 6000.0, 10.5, false, {}, one) == OwnAirCall::Hold);        // one zone: above the split is the fighters'
    SC_EXPECT(judge(9000.0, 4000.0, 10.5, false, {}, one) == OwnAirCall::Free);
}

SC_TEST("own air B10: a notice down the chain: post by post, each hop's coding work, the division to the regiment in minutes") {
    ChainData chain;
    std::string e;
    SC_EXPECT(LoadChainData(kData, "command/chain_pannonia.json", chain, &e));
    const OwnAirDef d = Rules();
    const LocalFrame frame(GeoPoint::FromDegrees(46.9, 19.7, 110.0));
    OwnFlightNotice n;
    n.number = 402;
    n.flight = "Sokol 1";
    n.route = {Vec3{-25000.0, -20000.0, 0.0}, Vec3{0.0, 30000.0, 0.0}};
    n.floor_m = 2000.0;
    n.ceiling_m = 6000.0;
    n.cross_from_h = 10.5;
    n.cross_to_h = 11.5;
    double sum_div_rgt = 0.0;
    int runs = 0;
    for (unsigned seed = 1; seed <= 10; ++seed) {
        const NoticeTrace t = RunNotice(chain, n, NoticeWords(n, frame), "fighter_rgt_duty", "rgt_cp_duty", 0.25, d.notice, seed, 10.0);
        SC_EXPECT(!t.hops.empty());
        if (t.hops.empty()) continue;
        SC_EXPECT(t.hops.back().post == "rgt_cp_duty");
        std::set<std::string> posts;
        double prev = t.sent_h;
        double div_h = -1.0;
        for (const NoticeHop& h : t.hops) {
            SC_EXPECT(h.h >= prev + d.notice.hop_work_s / 3600.0 - 1e-9);  // each hop codes and decodes it
            prev = h.h;
            posts.insert(h.post);
            if (h.post == "div_ad_cp_duty") div_h = h.h;
        }
        SC_EXPECT(posts.count("sector_cp_duty") == 1);
        if (div_h >= 0.0) {
            sum_div_rgt += (t.hops.back().h - div_h) * 60.0;
            ++runs;
        }
        if (seed == 1) {
            std::printf("    the notice's way:");
            for (const NoticeHop& h : t.hops) std::printf(" %s +%.1f min", h.post.c_str(), (h.h - t.sent_h) * 60.0);
            std::printf("\n");
        }
    }
    SC_EXPECT(runs > 0);
    if (runs > 0) {
        // The division's post to the regiment's post, then the regiment's net to the battery (liaison_s): 10-15 minutes
        // from the division to the battery (mt_turchin_coop: a notice at the battery in 10-15 min).
        const double div_bty = sum_div_rgt / runs + d.notice.liaison_s / 60.0;
        std::printf("    division to battery: %.1f min on average\n", div_bty);
        SC_EXPECT(div_bty >= 10.0 && div_bty <= 16.0);
    }
}

SC_TEST("own air B10 in a session: the Osa's IFF asks on the code its crew holds; one of ours on another code is fired on, and the trace says why") {
    int fired_on_ours_mismatch = 0, replied_match = 0, fired_on_ours_match = 0;
    for (unsigned seed = 1; seed <= 3; ++seed) {
        const std::string block = R"JSON({ "source": "sam3d", "confidence": "estimate", "notes": "a test", "fixed_code": 10 })JSON";
        const Played mismatch = Play(Session(OursAndHostile(R"JSON("iff_code": 9,)JSON"), block, seed));
        const Played match = Play(Session(OursAndHostile(R"JSON("iff_code": 10,)JSON"), block, seed));
        SC_EXPECT(mismatch.ok && match.ok);
        SC_EXPECT(mismatch.battery_code == 10 && mismatch.ours_code == 9 && match.ours_code == 10);
        SC_EXPECT(!mismatch.ours_replied);
        replied_match += match.ours_replied ? 1 : 0;
        fired_on_ours_match += match.at_ours ? 1 : 0;
        if (mismatch.at_ours) {
            ++fired_on_ours_mismatch;
            SC_EXPECT(!mismatch.findings.empty());
            if (!mismatch.findings.empty()) {
                const FriendlyFireFinding& f = mismatch.findings.front();
                std::printf("    seed %u: %s\n", seed, f.why.c_str());
                SC_EXPECT(f.aircraft == "ours" && f.code_mismatch && f.no_notice && f.battery_filter == 10 && f.aircraft_filter == 9);
            }
        }
    }
    SC_EXPECT(replied_match == 3 && fired_on_ours_match == 0);
    SC_EXPECT(fired_on_ours_mismatch >= 1);
}

SC_TEST("own air B10 in a session: a notice that reached the crew holds its fire on a silent one of ours in the corridor; a lost one does not") {
    // The notice's route as the PVO grid's small squares: its two ends are squares' centres, so the corridor the crew reads
    // back is the one sent. The battery stands on it; ours flies along it at 1,200 m, silent (its transponder off); a hostile
    // comes at the battery square to the corridor and turns away 5.5 km out, outside the corridor's 5 km.
    const LocalFrame frame(GeoPoint::FromDegrees(46.9, 19.7, 110.0));
    Vec3 c_n, c_s;
    SC_EXPECT(ReadSquare(frame, SquareOf(frame, Vec3{0.0, 30000.0, 0.0}), Vec3{0.0, 30000.0, 0.0}, c_n));
    SC_EXPECT(ReadSquare(frame, SquareOf(frame, Vec3{0.0, -30000.0, 0.0}), Vec3{0.0, -30000.0, 0.0}, c_s));
    const Vec3 dir = (c_s - c_n) * (1.0 / (c_s - c_n).Length());
    const Vec3 perp{-dir.y, dir.x, 0.0};
    const Vec3 unit = c_n + dir * Dot(Vec3{} - c_n, dir);
    const auto at = [](const Vec3& p, double up) {
        return "{ \"east_m\": " + std::to_string(p.x) + ", \"north_m\": " + std::to_string(p.y) + ", \"up_m\": " + std::to_string(up) + " }";
    };
    const std::string targets = "[ { \"id\": \"ours\", \"name\": \"Our MiG-21 (placeholder)\", \"class\": \"fighter\", \"iff\": \"friend\", \"iff_code\": -1, "
                                "\"start\": " + at(unit - dir * 9000.0, 1200.0) + ", \"waypoints\": [ " + at(unit + dir * 25000.0, 1200.0) + " ], "
                                "\"speed_mps\": 160, \"rcs_m2\": 3, \"appear_at_s\": 20 }, "
                                "{ \"id\": \"hostile\", \"name\": \"Fighter (placeholder)\", \"class\": \"fighter\", "
                                "\"start\": " + at(unit + perp * 14000.0 - dir * 6000.0, 1000.0) + ", \"waypoints\": [ " + at(unit + perp * 5500.0, 1000.0) + ", " +
                                at(unit + perp * 14000.0 + dir * 6000.0, 1000.0) + " ], \"speed_mps\": 150, \"rcs_m2\": 3, \"appear_at_s\": 20 } ]";
    const auto block = [&](bool lost) {
        return std::string(R"JSON({ "source": "sam3d", "confidence": "estimate", "notes": "a test", "fixed_code": 10,
            "notices": [ { "flight": "Sokol 1", "type": "MiG-21", "count": 1, "route_m": [ [ )JSON") + std::to_string(c_n.x) + ", " + std::to_string(c_n.y) +
               " ], [ " + std::to_string(c_s.x) + ", " + std::to_string(c_s.y) + R"JSON( ] ], "half_width_m": 5000,
                           "floor_m": 500, "ceiling_m": 2500, "cross_from_s": 0, "cross_to_s": 1200, "sent_s": -3600, "lost": )JSON" +
               (lost ? "true" : "false") + R"JSON(, "aircraft": [ "ours" ] } ] })JSON";
    };
    int held = 0, fired_lost = 0, hostile_engaged = 0;
    for (unsigned seed = 1; seed <= 3; ++seed) {
        const Played told = Play(Session(targets, block(false), seed, "", 300.0, unit));
        const Played lost = Play(Session(targets, block(true), seed, "", 300.0, unit));
        SC_EXPECT(told.ok && lost.ok);
        SC_EXPECT(told.notices.size() == 1 && lost.notices.empty());  // the crew has what reached it, read from the words
        if (told.notices.size() != 1) {
            for (const std::string& l : told.own_air_log) std::printf("    %s\n", l.c_str());
        }
        if (!told.notices.empty()) SC_EXPECT(told.notices.front().flight == "Sokol 1");
        held += told.at_ours ? 0 : 1;
        hostile_engaged += told.at_hostile ? 1 : 0;
        if (lost.at_ours) {
            ++fired_lost;
            SC_EXPECT(!lost.findings.empty() && lost.findings.front().no_notice && lost.findings.front().transponder_off);
        }
    }
    std::printf("    held %d of 3, the hostile engaged %d of 3, ours fired on without the notice %d of 3\n", held, hostile_engaged, fired_lost);
    SC_EXPECT(held == 3);
    SC_EXPECT(hostile_engaged >= 2);  // the hostile outside the corridor is not held
    SC_EXPECT(fired_lost >= 1);
}

SC_TEST("own air B10 in a session: the duty scrambles a pair on our GCI's picture; its controller vectors it; its codes are the table's") {
    const std::string targets = R"JSON([
        { "id": "raid1", "name": "Strike jet (placeholder)", "class": "fighter", "start": { "east_m": 2000, "north_m": 70000, "up_m": 3000 },
          "waypoints": [ { "east_m": 2000, "north_m": -40000, "up_m": 3000 } ], "speed_mps": 200, "rcs_m2": 6, "appear_at_s": 0 } ])JSON";
    const std::string air_fight = R"JSON( "air_fight": { "year": 1985, "source": "sam3d", "confidence": "estimate", "notes": "our GCI only",
        "controllers": [ { "id": "our_gci", "side": "ours", "kind": "radar_control", "east_m": 0, "north_m": -30000, "range_m": 220000 } ] },)JSON";
    const std::string block = R"JSON({ "source": "sam3d", "confidence": "estimate", "notes": "a test", "raid_warning_s": 0,
        "regiments": [ { "id": "fighter_rgt_1", "base": "kecel_airbase", "base_east_m": -25000, "base_north_m": -20000, "area_east_m": 0, "area_north_m": 0,
                         "gci": "our_gci" } ],
        "gci": { "east_m": 0, "north_m": -30000 } })JSON";
    int scrambled = 0, vectored = 0;
    for (unsigned seed = 1; seed <= 3; ++seed) {
        std::unique_ptr<ScenarioRunner> r = Load(Session(targets, block, seed, air_fight, 900.0));
        SC_EXPECT(r != nullptr);
        if (!r) return;
        while (!r->Finished() && r->Time() < 700.0) r->Step(0.1);
        const std::vector<Scramble> s = r->OwnScrambles();
        if (s.empty()) continue;
        ++scrambled;
        const Scramble& first = s.front();
        SC_EXPECT(first.regiment == "fighter_rgt_1" && first.readiness == 1);
        // Readiness 1: 3-5 min from the order to airborne (fm_100_2_1_1984).
        SC_EXPECT(first.airborne_s - first.ordered_s >= 180.0 - 1e-6 && first.airborne_s - first.ordered_s <= 300.0 + 1e-6);
        const std::string id = "fighter_rgt_1-own1-1";
        const int code = r->TargetIffCode(id);
        const int table = r->IffCodes() != nullptr ? r->IffCodes()->FilterAt((36000.0 + r->Time()) / 3600.0) : 0;
        std::printf("    seed %u: ordered %.0f s, airborne %.0f s; pair's code %d, the table's %d; notices traced %zu\n", seed, first.ordered_s,
                    first.airborne_s, code, table, r->NoticeTraces().size());
        SC_EXPECT(code >= 9 && code <= 12);
        const airwar::AirFightSession* af = r->AirFight();
        SC_EXPECT(af != nullptr);
        if (af != nullptr && af->VectorsSent() > 0) ++vectored;
        SC_EXPECT(r->NoticeTraces().size() == 1);  // the pair's notice went down the chain
    }
    SC_EXPECT(scrambled == 3 && vectored == 3);
}

SC_TEST("own air B10: the enemy learns a corridor it hears used often enough, and only a creative planner times a raid into it") {
    const OwnAirDef d = Rules();
    OwnAirZone z;
    z.id = "corridor_north";
    z.path = {Vec3{0.0, 0.0, 0.0}, Vec3{0.0, 40000.0, 0.0}};
    z.floor_m = 2000.0;
    z.ceiling_m = 5000.0;
    z.from_h = 34.0;
    z.to_h = 35.0;
    int learnt = 0;
    double off_m = 0.0;
    for (std::uint64_t seed = 1; seed <= 30; ++seed) {
        EnemyCorridorIntel intel;
        intel.Init(d.learning, seed);
        SC_EXPECT(intel.Believed(0.0).empty());
        int heard = 0;
        for (int use = 0; use < 40; ++use) heard += intel.Use(z, 24.0 + use) ? 1 : 0;
        const double h = 24.0 + 39.0;
        const std::vector<BelievedCorridor> b = intel.Believed(h);
        SC_EXPECT(b.empty() == (heard < d.learning.learn_after));
        if (b.empty()) continue;
        ++learnt;
        off_m += (b.front().path.front() - z.path.front()).Length();
        SC_EXPECT(b.front().from_h == std::fmod(z.from_h, 24.0));
        SC_EXPECT(intel.Exploits(d.learning.use_creativity, h) && !intel.Exploits(d.learning.use_creativity - 0.1, h));
        SC_EXPECT(intel.Believed(h + d.learning.use_age_h + 1.0).empty());  // a stale belief is not used
    }
    std::printf("    %d of 30 learnt the corridor; placed %.1f km off on average\n", learnt, learnt > 0 ? off_m / learnt / 1000.0 : 0.0);
    SC_EXPECT(learnt >= 25);
    SC_EXPECT(learnt == 0 || off_m / learnt < 3.0 * d.learning.sigma_m);
}

SC_TEST("own air B10: the post, the regiment's picture and the neighbours identify by the code they hold, never by the truth") {
    const std::string src = ReadAll(kData + "/../simcore/include/simcore/scenario_runner.hpp");
    SC_EXPECT(!src.empty());
    const auto count = [&src](const std::string& what) {
        int n = 0;
        for (std::size_t at = src.find(what); at != std::string::npos; at = src.find(what, at + 1)) ++n;
        return n;
    };
    SC_EXPECT(count("(t.friendly ? 1 : 9)") == 0);                  // the board's type
    SC_EXPECT(count("c.transponder = t.friendly") == 0);           // the post's interrogator
    SC_EXPECT(count("  t.friendly = def_.targets[i].friendly") == 0);  // the battery fire model's and the neighbours'
    SC_EXPECT(count("RegimentHoldsFire(i, a->position)") == 3);
}

// ------------------------------------------------------------------------------------------------ S1: our fighters in full

namespace {

std::vector<std::string> Said(const ScenarioRunner& r) {
    std::vector<std::string> out;
    if (r.Callouts() == nullptr) return out;
    for (const SpokenCallout& s : r.Callouts()->Log()) out.push_back(s.id);
    return out;
}
bool Has(const std::vector<std::string>& v, const std::string& id) { return std::find(v.begin(), v.end(), id) != v.end(); }

// The session's text with the Osa's interrogator code switch set at the start (a position, the code less one).
std::string WithKnob(std::string text, int position) {
    const std::string at = "\"readiness\": \"deployed\",";
    const std::size_t p = text.find(at);
    if (p != std::string::npos) text.insert(p + at.size(), " \"controls\": { \"iff.code\": " + std::to_string(position) + " },");
    return text;
}

airwar::AirWarData War(const std::string& faction, double own_air_mode) {
    airwar::AirWarData d;
    std::string e;
    if (!airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_" + faction + ".json", d, &e)) std::printf("    %s\n", e.c_str());
    d.tuning.values["own_air.mode"] = own_air_mode;
    return d;
}
void Run(airwar::AirCampaign& c, double days) {
    while (c.Now() < days * 24.0) {
        if (c.Advance(days * 24.0)) c.SkipPlayerMission();
    }
}

}  // namespace

SC_TEST("own air S1: a fighter regiment's loss or claim said on the net and read back from the journal is the same report") {
    const LocalFrame frame(GeoPoint::FromDegrees(46.9, 19.7, 110.0));
    std::mt19937 rng(11);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    const char* const causes[] = {"in air combat", "by fire from the ground", "cause unknown"};
    for (int k = 0; k < 40; ++k) {
        OwnAirReport r;
        r.kind = k % 2 == 0 ? OwnAirReportKind::Loss : OwnAirReportKind::Claim;
        r.regiment = k % 3 == 0 ? "fighter_rgt_1" : "kecel_fighters";
        r.flight = "Sokol " + std::to_string(k + 1);
        r.type = k % 2 == 0 ? "MiG-21" : "F-16A";
        r.count = 1 + k % 3;
        r.clock_h = 6.0 + 14.0 * u(rng);
        r.where = Vec3{(u(rng) - 0.5) * 120000.0, (u(rng) - 0.5) * 120000.0, 0.0};
        r.cause = r.kind == OwnAirReportKind::Loss ? causes[k % 3] : "";
        const std::string words = OwnAirReportWords(r, frame);
        OwnAirReport back;
        SC_EXPECT(ReadOwnAirReport("radio: " + words, frame, Vec3{}, r.clock_h + 0.2, back));
        SC_EXPECT(back.kind == r.kind && back.regiment == r.regiment && back.flight == r.flight && back.type == r.type && back.count == r.count);
        SC_EXPECT(std::fabs(back.clock_h - r.clock_h) <= 1.0 / 60.0 + 1e-9);   // to the minute
        SC_EXPECT((back.where - r.where).HorizontalLength() <= 10000.0);        // within the grid's small square
        SC_EXPECT(back.cause == r.cause);
    }
    OwnAirReport none;
    SC_EXPECT(!ReadOwnAirReport("402. Notice 101. Own aircraft: 2 MiG-21", frame, Vec3{}, 10.0, none));
}

SC_TEST("own air S1: a loss goes down the chain as our damage and a claim as a raid's result, each hop with its coding work") {
    ChainData chain;
    std::string e;
    SC_EXPECT(LoadChainData(kData, "command/chain_pannonia.json", chain, &e));
    const OwnAirDef d = Rules();
    const LocalFrame frame(GeoPoint::FromDegrees(46.9, 19.7, 110.0));
    for (unsigned seed = 1; seed <= 6; ++seed) {
        for (OwnAirReportKind kind : {OwnAirReportKind::Loss, OwnAirReportKind::Claim}) {
            OwnAirReport r;
            r.kind = kind;
            r.regiment = "fighter_rgt_1";
            r.flight = "Sokol 1";
            r.clock_h = 10.25;
            const NoticeTrace t = RunOwnAirReport(chain, r, static_cast<int>(seed), OwnAirReportWords(r, frame), "fighter_rgt_duty", "rgt_cp_duty", 0.25,
                                                  d.reports, seed, 10.0);
            SC_EXPECT(!t.hops.empty() && t.flight == "Sokol 1");
            if (t.hops.empty()) continue;
            SC_EXPECT(t.hops.back().post == "rgt_cp_duty");
            double prev = t.sent_h;
            for (const NoticeHop& h : t.hops) {
                SC_EXPECT(h.h >= prev + d.reports.hop_work_s / 3600.0 - 1e-9);
                prev = h.h;
            }
        }
    }
}

SC_TEST("own air S1: the regiment's check meets a launch with a loss report; a notice in the journal before the launch makes it a debit") {
    const LocalFrame frame(GeoPoint::FromDegrees(46.9, 19.7, 110.0));
    const OwnAirDef d = Rules();
    const double clock0 = 10.0;
    OwnFlightNotice n;
    n.number = 101;
    n.flight = "Sokol 1";
    n.route = {Vec3{0.0, 30000.0, 0.0}, Vec3{0.0, -30000.0, 0.0}};
    n.floor_m = 500.0;
    n.ceiling_m = 3000.0;
    n.cross_from_h = 10.0;
    n.cross_to_h = 11.0;
    OwnAirReport loss;
    loss.regiment = "fighter_rgt_1";
    loss.flight = "Sokol 1";
    loss.clock_h = clock0 + 600.0 / 3600.0;
    loss.where = Vec3{500.0, 2000.0, 0.0};
    loss.cause = "cause unknown";
    std::vector<campaign::BoardRecord> board = {{7, 590.0, Vec3{400.0, 2100.0, 0.0}, 15, 9, 1}, {8, 900.0, Vec3{40000.0, 40000.0, 0.0}, 15, 9, 1}};
    std::vector<campaign::LogEntry> log = {{600.0, "21", "launch", 7}, {905.0, "21", "launch", 8}, {1500.0, "21", "launch", 9}};
    const std::vector<campaign::JournalLine> with_notice = {{100.0, "regiment", "rgt", NoticeWords(n, frame)},
                                                           {800.0, "regiment", "rgt", OwnAirReportWords(loss, frame)}};
    const std::vector<campaign::JournalLine> without = {{800.0, "regiment", "rgt", OwnAirReportWords(loss, frame)}};
    const campaign::OwnAirCheck a = campaign::CheckOwnAir(with_notice, board, log, frame, Vec3{}, clock0, d.reports);
    const campaign::OwnAirCheck b = campaign::CheckOwnAir(without, board, log, frame, Vec3{}, clock0, d.reports);
    SC_EXPECT(a.notices == 1 && a.losses == 1 && b.notices == 0 && b.losses == 1);
    SC_EXPECT(a.launches.size() == 3 && b.launches.size() == 3);
    // Number 7 at the loss's place and minute: met; 8 far away and 9 without a mark: not.
    SC_EXPECT(a.Met() == 1 && b.Met() == 1 && a.launches[0].loss_met && !a.launches[1].loss_met && !a.launches[2].placed);
    SC_EXPECT(a.AgainstRecord() == 1 && b.AgainstRecord() == 0);
    std::printf("    %s\n", campaign::OwnAirCheckText(a, [](double s) { return std::to_string(static_cast<int>(s)) + " s"; }).c_str());
    campaign::ReportTrust ta, tb;
    ta.Checked("bty21", a);
    tb.Checked("bty21", b);
    SC_EXPECT(ta.Of("bty21") < tb.Of("bty21"));  // fired against its own journal: believed less
    // A loss report written before the notice reached the battery is no debit: the notice came after the launch.
    const std::vector<campaign::JournalLine> late = {{700.0, "regiment", "rgt", NoticeWords(n, frame)}, {800.0, "regiment", "rgt", OwnAirReportWords(loss, frame)}};
    SC_EXPECT(campaign::CheckOwnAir(late, board, log, frame, Vec3{}, clock0, d.reports).AgainstRecord() == 0);
}

SC_TEST("own air S1: the pursuit limit is 25-30 km past the front by regiment; airborne-alert zones lie behind the front in their band") {
    const OwnAirDef d = Rules();
    const Vec3 front{10000.0, 20000.0, 0.0};
    const Vec3 to_enemy{0.6, 0.8, 0.0};
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        for (int k = 0; k < 10; ++k) {
            const std::uint64_t key = FighterDuty::Key("rgt_" + std::to_string(k));
            const double lim = PursuitLimitM(d, seed, key);
            SC_EXPECT(lim >= 1000.0 * d.pursuit_min_km - 1e-6 && lim <= 1000.0 * d.pursuit_max_km + 1e-6);
            SC_EXPECT(lim == PursuitLimitM(d, seed, key));  // keyed: the same regiment, the same line
            for (bool low : {true, false}) {
                const AlertZone z = AlertZoneOf(d, front, to_enemy, low, seed, key);
                const double behind = -Dot(z.centre - front, to_enemy);
                SC_EXPECT(behind >= 1000.0 * d.alert_inside_min_km - 1e-6 && behind <= 1000.0 * d.alert_inside_max_km + 1e-6);
                if (low) SC_EXPECT(z.alt_m >= d.alert_low_min_m - 1e-9 && z.alt_m <= d.alert_low_max_m + 1e-9);
                else SC_EXPECT(z.alt_m >= d.alert_high_min_m - 1e-9 && z.alt_m <= d.alert_high_max_m + 1e-9);
            }
        }
    }
}

SC_TEST("own air S1 in a session: the table's code changed, the commander orders it and the senior operator turns the interrogator's switch") {
    const std::string block = R"JSON({ "source": "sam3d", "confidence": "estimate", "notes": "a test", "fixed_code": 10 })JSON";
    const OwnAirDef d = Rules();
    int set = 0;
    for (unsigned seed = 1; seed <= 3; ++seed) {
        // Both come in at 120 s, after the switch is turned: ours then answers on the battery's code.
        std::string text = WithKnob(Session(OursAndHostile(R"JSON("iff_code": 10,)JSON"), block, seed), 2);
        for (std::size_t at = text.find("\"appear_at_s\": 20"); at != std::string::npos; at = text.find("\"appear_at_s\": 20", at + 1)) {
            text.replace(at, 17, "\"appear_at_s\": 120");
        }
        std::unique_ptr<ScenarioRunner> r = Load(text);
        SC_EXPECT(r != nullptr);
        if (!r) return;
        double first_s = -1.0, set_s = -1.0;
        while (!r->Finished() && r->Time() < 300.0) {
            r->Step(0.1);
            if (first_s < 0.0 && r->BatteryIffCode() != 0) first_s = r->Time();
            if (first_s >= 0.0 && r->Time() - first_s < 1.0) SC_EXPECT(r->BatteryIffCode() == 3);  // the switch as the crew found it
            if (set_s < 0.0 && r->BatteryIffCode() == 10) set_s = r->Time();
        }
        const std::vector<std::string> said = Said(*r);
        std::printf("    seed %u: the switch at 10 after %.0f s; ordered %d, reported %d\n", seed, set_s, Has(said, "order_iff_code") ? 1 : 0,
                    Has(said, "iff_code_set") ? 1 : 0);
        if (set_s < 0.0) continue;
        ++set;
        SC_EXPECT(Has(said, "order_iff_code") && Has(said, "iff_code_set"));
        SC_EXPECT(set_s - first_s >= d.iff_change.job_s / 2.0);   // the turn takes its time, even for a quick hand
        SC_EXPECT(r->Engagement().KnownFriend("ours") || !r->TargetAircraft("ours"));  // then ours answers on it
    }
    SC_EXPECT(set == 3);
}

SC_TEST("own air S1 in a session: the crew says the notice it read; a friend's reply off every notice is reported as a violator") {
    // A notice for a flight far to the north-east; ours answers the battery's code 10 over the battery: a violator.
    const std::string block = R"JSON({ "source": "sam3d", "confidence": "estimate", "notes": "a test", "fixed_code": 10,
        "notices": [ { "flight": "Sokol 7", "type": "MiG-21", "count": 2, "route_m": [ [ 60000, 60000 ], [ 90000, 90000 ] ], "half_width_m": 5000,
                       "floor_m": 3000, "ceiling_m": 6000, "cross_from_s": 0, "cross_to_s": 1200, "sent_s": -3600 } ] })JSON";
    int told = 0, violators = 0;
    for (unsigned seed = 1; seed <= 3; ++seed) {
        std::unique_ptr<ScenarioRunner> r = Load(Session(OursAndHostile(R"JSON("iff_code": 10,)JSON"), block, seed));
        SC_EXPECT(r != nullptr);
        if (!r) return;
        while (!r->Finished() && r->Time() < 300.0) r->Step(0.1);
        const std::vector<std::string> said = Said(*r);
        told += Has(said, "own_air_told") ? 1 : 0;
        violators += Has(said, "report_violator") ? 1 : 0;
        SC_EXPECT(!Has(said, "fire_prohibited_own"));  // nothing silent in a corridor here
    }
    std::printf("    told %d of 3, the violator reported %d of 3\n", told, violators);
    SC_EXPECT(told == 3);
    SC_EXPECT(violators >= 2);
}

SC_TEST("own air S1 in a session: one of ours shot down, its wingman saw it: the regiment's loss report reaches the battery's net") {
    const std::string targets = R"JSON([
        { "id": "ours", "name": "Our MiG-21 (placeholder)", "class": "fighter", "iff": "friend", "iff_code": 9,
          "start": { "east_m": -600, "north_m": 9000, "up_m": 1200 }, "waypoints": [ { "east_m": -600, "north_m": -20000, "up_m": 1200 } ],
          "speed_mps": 160, "rcs_m2": 3, "appear_at_s": 20 },
        { "id": "wing", "name": "Our MiG-21 (placeholder)", "class": "fighter", "iff": "friend", "iff_code": 9,
          "start": { "east_m": -1600, "north_m": 10500, "up_m": 1300 }, "waypoints": [ { "east_m": -1600, "north_m": -18500, "up_m": 1300 } ],
          "speed_mps": 160, "rcs_m2": 3, "appear_at_s": 20 } ])JSON";
    const std::string block = R"JSON({ "source": "sam3d", "confidence": "estimate", "notes": "a test", "fixed_code": 10 })JSON";
    const OwnAirDef d = Rules();
    int down = 0, reported = 0;
    for (unsigned seed = 1; seed <= 4; ++seed) {
        // The session runs on after both are down (it would end once every target is gone): the reports come later.
        std::string text = Session(targets, block, seed, "", 700.0);
        const std::string limit = R"JSON("time_limit_s": 700)JSON";
        text.replace(text.find(limit), limit.size(), limit + R"JSON(, "when_targets_gone": false)JSON");
        std::unique_ptr<ScenarioRunner> r = Load(text);
        SC_EXPECT(r != nullptr);
        if (!r) return;
        std::map<std::string, double> down_s;
        while (!r->Finished() && r->Time() < 600.0) {
            r->Step(0.1);
            for (const char* id : {"ours", "wing"}) {
                const Aircraft* a = r->TargetAircraft(id);
                if (down_s.count(id) == 0 && a != nullptr && !a->alive) down_s[id] = r->Time();
            }
        }
        const std::vector<OwnAirReport> reps = r->OwnAirReports();
        for (const OwnAirReport& rep : reps) {
            SC_EXPECT(rep.kind == OwnAirReportKind::Loss && down_s.count(rep.flight) == 1);  // only what went down, never a claim here
            std::printf("    seed %u: %s\n", seed, OwnAirReportWords(rep, LocalFrame(GeoPoint::FromDegrees(46.9, 19.7, 110.0))).c_str());
        }
        for (const auto& kv : down_s) {
            ++down;
            if (kv.second + d.reports.seen_s > 590.0) continue;  // not due within the session
            // Seen by the other while it flew within 15 km: by fire from the ground, said after seen_s; nobody near: missing.
            for (const OwnAirReport& rep : reps) {
                if (rep.flight != kv.first) continue;
                ++reported;
                SC_EXPECT(rep.cause == "by fire from the ground" || rep.cause == "cause unknown");
            }
        }
    }
    std::printf("    %d of ours down, %d loss reports heard\n", down, reported);
    SC_EXPECT(down >= 1);
    SC_EXPECT(reported >= 1);
}

SC_TEST("own air S1 in a session: the raid warning sends a pair to its airborne-alert zone; no pair of ours flies past its pursuit limit") {
    const std::string targets = R"JSON([
        { "id": "raid1", "name": "Strike jet (placeholder)", "class": "fighter", "start": { "east_m": 2000, "north_m": 80000, "up_m": 3000 },
          "waypoints": [ { "east_m": 2000, "north_m": 5000, "up_m": 3000 }, { "east_m": 2000, "north_m": 160000, "up_m": 3000 } ],
          "speed_mps": 200, "rcs_m2": 6, "appear_at_s": 0 } ])JSON";
    const std::string air_fight = R"JSON( "air_fight": { "year": 1985, "source": "sam3d", "confidence": "estimate", "notes": "our GCI only",
        "controllers": [ { "id": "our_gci", "side": "ours", "kind": "radar_control", "east_m": 0, "north_m": -30000, "range_m": 220000 } ] },)JSON";
    const std::string block = R"JSON({ "source": "sam3d", "confidence": "estimate", "notes": "a test", "raid_warning_s": 0,
        "front": { "east_m": 0, "north_m": 20000, "enemy_bearing_deg": 0 },
        "regiments": [ { "id": "fighter_rgt_1", "base": "kecel_airbase", "base_east_m": -25000, "base_north_m": -60000, "area_east_m": 0, "area_north_m": 0,
                         "gci": "our_gci" } ],
        "gci": { "east_m": 0, "north_m": -30000 } })JSON";
    const OwnAirDef d = Rules();
    int alerts = 0;
    double worst_past_km = -1e9;
    for (unsigned seed = 1; seed <= 3; ++seed) {
        std::unique_ptr<ScenarioRunner> r = Load(Session(targets, block, seed, air_fight, 1300.0));
        SC_EXPECT(r != nullptr);
        if (!r) return;
        const double limit = PursuitLimitM(d, seed, FighterDuty::Key("fighter_rgt_1"));
        while (!r->Finished() && r->Time() < 1200.0) {
            r->Step(0.1);
            for (const ScenarioTarget& t : r->Def().targets) {
                if (!t.friendly) continue;
                const Aircraft* a = r->TargetAircraft(t.id);
                if (a == nullptr || !a->alive) continue;
                const double past = a->position.y - 20000.0;
                worst_past_km = std::max(worst_past_km, (past - limit) / 1000.0);
                SC_EXPECT(past <= limit + 8000.0);  // recalled at the line; the turn home takes a few km
            }
        }
        for (const Scramble& s : r->OwnScrambles()) {
            if (!s.alert) continue;
            ++alerts;
            SC_EXPECT(s.ordered_s <= 1.0);
            const double behind_km = (20000.0 - s.toward.y) / 1000.0;
            SC_EXPECT(behind_km >= d.alert_inside_min_km - 1e-6 && behind_km <= d.alert_inside_max_km + 1e-6);
        }
    }
    std::printf("    %d alert pairs; our furthest past its pursuit limit %.1f km\n", alerts, worst_past_km);
    SC_EXPECT(alerts == 3 * d.alert_pairs);
}

SC_TEST("own air S1 in a session: in a war with the Pact some of its aircraft are allies' on our Kremniy filters, and answer when the codes meet") {
    std::string targets = "[";
    for (int k = 0; k < 6; ++k) {
        targets += std::string(k ? "," : "") + "{ \"id\": \"pact" + std::to_string(k) + "\", \"name\": \"MiG-23 (placeholder)\", \"class\": \"fighter\", " +
                   "\"start\": { \"east_m\": " + std::to_string(-15000 + 6000 * k) + ", \"north_m\": 30000, \"up_m\": 3000 }, \"waypoints\": [ { \"east_m\": " +
                   std::to_string(-15000 + 6000 * k) + ", \"north_m\": -30000, \"up_m\": 3000 } ], \"speed_mps\": 220, \"rcs_m2\": 4, \"appear_at_s\": 0 }";
    }
    targets += "]";
    const std::string block = R"JSON({ "source": "sam3d", "confidence": "estimate", "notes": "a test", "war": true, "enemy_allies": true })JSON";
    int allied = 0, all = 0, answered = 0;
    for (unsigned seed = 1; seed <= 8; ++seed) {
        std::unique_ptr<ScenarioRunner> r = Load(Session(targets, block, seed, "", 200.0));
        SC_EXPECT(r != nullptr);
        if (!r) return;
        while (!r->Finished() && r->Time() < 120.0) r->Step(0.1);
        for (int k = 0; k < 6; ++k) {
            const std::string id = "pact" + std::to_string(k);
            ++all;
            const int code = r->TargetIffCode(id);
            if (r->IsAlliedEnemy(id)) {
                ++allied;
                SC_EXPECT(code >= 9 && code <= 12);  // the allies' own schedule of the war filters
            } else {
                SC_EXPECT(code <= 0);
            }
        }
        for (const std::string& f : r->FalseFriends()) {
            ++answered;
            SC_EXPECT(r->IsAlliedEnemy(f));  // only an ally's aircraft answers friend, and only on our code
        }
    }
    std::printf("    %d of %d the allies', %d answered friend\n", allied, all, answered);
    SC_EXPECT(allied >= all / 4 && allied <= 3 * all / 4);
}

SC_TEST("own air S1: the enemy hears our corridors used day by day, learns them, and a creative planner times a raid into one") {
    for (const char* faction : {"warsaw_pact", "nato"}) {
        const airwar::AirWarData on = War(faction, 1.0);
        const airwar::AirWarData off = War(faction, 0.0);
        SC_EXPECT(on.has_own_air);
        for (unsigned seed = 1; seed <= 2; ++seed) {
            airwar::AirCampaign c;
            c.Init(on, seed);
            const std::vector<OwnAirZone> zs = c.OwnCorridors();
            int airfields = 0;
            for (const airwar::TheatreObject& o : on.theatre.objects) airfields += o.kind == airwar::ObjectKind::Airfield ? 1 : 0;
            SC_EXPECT(static_cast<int>(zs.size()) == airfields);
            for (const OwnAirZone& z : zs) SC_EXPECT(z.path.size() == 2 && z.floor_m == 2000.0 && z.ceiling_m == 6000.0 && z.from_h < z.to_h);
            Run(c, 8.0);
            int heard_lines = 0, heard = 0, timed = 0;
            for (const airwar::JournalEntry& j : c.Journal()) heard_lines += j.text.find("our fighters' traffic in corridor") != std::string::npos ? 1 : 0;
            for (const BelievedCorridor& b : c.CorridorIntel().Heard()) heard += b.heard;
            SC_EXPECT(heard_lines == heard);  // each hearing in the enemy's journal, never seen by our side
            for (const airwar::Package& p : c.Packages()) {
                if (p.own_corridor.empty()) continue;
                ++timed;
                bool why = false;
                for (const std::string& s : p.rationale) why = why || s.find("timed into our fighters' " + p.own_corridor) != std::string::npos;
                SC_EXPECT(why);
                SC_EXPECT(c.CorridorIntel().Heard().size() > 0);
            }
            std::printf("    %s seed %u: %zu corridors, heard %d times, %zu believed by day 8, %d packages timed into one\n", faction, seed, zs.size(), heard,
                        c.CorridorIntel().Believed(c.Now()).size(), timed);
            // Before S1 (a save's own_air.mode 0): none of it.
            airwar::AirCampaign b;
            b.Init(off, seed);
            SC_EXPECT(b.OwnCorridors().empty());
            Run(b, 3.0);
            for (const airwar::Package& p : b.Packages()) SC_EXPECT(p.own_corridor.empty());
            SC_EXPECT(b.CorridorIntel().Heard().empty());
        }
    }
}

SC_TEST("own air S6: our dummy traffic in false corridors is heard and believed like the real; a raid timed into one gets no held fire; not at radio.mode 0") {
    const auto dummy_id = [](const std::string& id) { return id.size() > 2 && id.compare(id.size() - 2, 2, " B") == 0; };
    for (const char* faction : {"warsaw_pact", "nato"}) {
        const airwar::AirWarData on = War(faction, 1.0);
        airwar::AirWarData off = War(faction, 1.0);
        off.tuning.values["radio.mode"] = 0.0;
        int dummy_heard = 0, dummy_timed = 0, real_timed = 0;
        for (unsigned seed = 1; seed <= 3; ++seed) {
            airwar::AirCampaign c;
            c.Init(on, seed);
            Run(c, 8.0);
            const std::vector<OwnAirZone> real = c.OwnCorridors();
            for (const BelievedCorridor& b : c.CorridorIntel().Heard()) {
                if (!dummy_id(b.id)) continue;
                dummy_heard += b.heard;
                // The false corridor leaves the real one's airfield on another way.
                const std::string base = b.id.substr(0, b.id.size() - 2);
                for (const OwnAirZone& z : real) {
                    if (z.id == base && !b.path.empty()) SC_EXPECT(z.DistanceM(b.path.back()) > z.half_width_m);
                }
            }
            for (const airwar::Package& p : c.Packages()) {
                if (p.own_corridor.empty()) continue;
                if (dummy_id(p.own_corridor)) {
                    ++dummy_timed;
                    SC_EXPECT(!p.own_corridor_found);  // our defences hold no fire for a corridor nobody flies
                } else {
                    ++real_timed;
                }
            }
            // Before S6: only the real corridors are talked on.
            airwar::AirCampaign b;
            b.Init(off, seed);
            Run(b, 4.0);
            for (const BelievedCorridor& x : b.CorridorIntel().Heard()) SC_EXPECT(!dummy_id(x.id));
        }
        std::printf("    %s: dummy corridors heard %d times over three seeds' eight days; raids timed into a dummy %d, into a real one %d\n", faction, dummy_heard,
                    dummy_timed, real_timed);
        SC_EXPECT(dummy_heard > 0);
    }
}

SC_TEST("own air S1: NATO's and the Pact's damaged aircraft coming home may fall to their own belts; the rebels' and the neutral's never do, and not before S1") {
    // The data: NATO's belt and the Pact's, each in its own words; NATO's words the defaults; no belt for the other two.
    const airwar::AirWarData nato = War("nato", 1.0);
    const airwar::AirWarData pact = War("warsaw_pact", 1.0);
    SC_EXPECT(nato.faction.own_ad.present && pact.faction.own_ad.present);
    SC_EXPECT(nato.faction.own_ad.belt == "Hawk and Nike belt" && pact.faction.own_ad.belt != nato.faction.own_ad.belt);
    SC_EXPECT(nato.faction.own_ad.route == "off its minimum risk route" && nato.faction.own_ad.iff == "Mode 4 failed");
    SC_EXPECT(pact.faction.own_ad.route.find("corridor") != std::string::npos && pact.faction.own_ad.iff.find("Parol") != std::string::npos);
    // Its own fratricide lower than NATO's (section 10: Parol and the Osa-AKM's launch block for its Soviet units).
    const auto per_damaged = [](const airwar::OwnAdDef& a) { return a.off_mrr_damaged * a.mode4_failed_damaged * a.belt_kill; };
    SC_EXPECT(per_damaged(pact.faction.own_ad) < per_damaged(nato.faction.own_ad));
    for (const char* faction : {"rebels", "neutral"}) SC_EXPECT(!War(faction, 1.0).faction.own_ad.present);

    int own_fire[2] = {0, 0};
    for (const char* faction : {"nato", "warsaw_pact", "rebels", "neutral"}) {
        const std::string f = faction;
        const bool belted = f == "nato" || f == "warsaw_pact";
        for (double mode : {1.0, 0.0}) {
            if (!belted && mode < 1.0) continue;  // nothing to switch off
            const airwar::AirWarData d = War(faction, mode);
            for (unsigned seed = 1; seed <= (belted ? 3u : 2u); ++seed) {
                airwar::AirCampaign c;
                c.Init(d, seed);
                const std::string belt = d.faction.own_ad.belt;
                c.ObserveOutcomes([&](const airwar::Package& p, const airwar::PackageOutcome& o) {
                    for (const airwar::LossRecord& l : o.losses) {
                        if (!l.own_fire) continue;
                        SC_EXPECT(belted && mode >= 1.0 && l.by == belt);
                        // Only a damaged aircraft comes home over its belt off its route.
                        if (l.flight >= 0 && static_cast<std::size_t>(l.flight) < p.flights.size()) SC_EXPECT(p.flights[static_cast<std::size_t>(l.flight)].damaged > 0);
                        own_fire[f == "nato" ? 0 : 1] += 1;
                    }
                });
                Run(c, 6.0);
            }
        }
    }
    std::printf("    lost to their own belts over 3 campaigns of 6 days: NATO %d, the Pact %d\n", own_fire[0], own_fire[1]);
    // Every chance at 1 (a test's figures, not the data's): each damaged aircraft that came home is lost to the belt, so a
    // campaign with any damaged aircraft has such losses, each written to the belt by its name, only to damaged flights,
    // and noted in the faction's own words.
    for (const char* faction : {"nato", "warsaw_pact"}) {
        airwar::AirWarData sure = War(faction, 1.0);
        sure.faction.own_ad.off_mrr_damaged = 1.0;
        sure.faction.own_ad.mode4_failed_damaged = 1.0;
        sure.faction.own_ad.belt_kill = 1.0;
        const std::string words = sure.faction.own_ad.route + ", " + sure.faction.own_ad.iff;
        int damaged = 0, belt = 0, noted = 0;
        for (unsigned seed = 1; seed <= 2; ++seed) {
            airwar::AirCampaign c;
            c.Init(sure, seed);
            c.ObserveOutcomes([&](const airwar::Package& p, const airwar::PackageOutcome& o) {
                for (const airwar::Flight& fl : p.flights) damaged += fl.role == airwar::FlightRole::Missile ? 0 : fl.damaged;
                for (const airwar::LossRecord& l : o.losses) {
                    if (!l.own_fire) continue;
                    ++belt;
                    SC_EXPECT(l.by == sure.faction.own_ad.belt);
                    if (l.flight >= 0 && static_cast<std::size_t>(l.flight) < p.flights.size()) SC_EXPECT(p.flights[static_cast<std::size_t>(l.flight)].damaged > 0);
                }
                for (const std::string& n : o.notes) {
                    if (n.find("lost to its own " + sure.faction.own_ad.belt) == std::string::npos) continue;
                    ++noted;
                    SC_EXPECT(n.find(words) != std::string::npos);
                }
            });
            Run(c, 4.0);
        }
        std::printf("    %s, every chance 1: %d damaged, %d lost to the belt, %d noted\n", faction, damaged, belt, noted);
        SC_EXPECT(damaged == 0 || belt > 0);
        SC_EXPECT(noted == belt);
    }
}

SC_TEST("own air S1: a save keeps its own_air mode; one made before S1 loads with it off and replays as played") {
    const airwar::AirWarData d = War("warsaw_pact", 1.0);
    airwar::AirCampaign c;
    c.Init(d, 3);
    Run(c, 2.0);
    const std::string text = airwar::SaveCampaignJson(c, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json");
    SC_EXPECT(text.find("\"own_air_mode\"") != std::string::npos);
    airwar::AirCampaign loaded;
    std::string e;
    SC_EXPECT(airwar::LoadCampaignJson(text, kData, loaded, &e));
    SC_EXPECT(loaded.Data().tuning.Get("own_air.mode") == 1.0 && loaded.Packages().size() == c.Packages().size());
    std::string old = text;
    const std::size_t at = old.find("\"own_air_mode\"");
    if (at != std::string::npos) {
        const std::size_t end = old.find_first_of(",}", at);
        old.erase(at, end - at + (old[end] == ',' ? 1 : 0));
    }
    airwar::AirCampaign before;
    SC_EXPECT(airwar::LoadCampaignJson(old, kData, before, &e));
    SC_EXPECT(before.Data().tuning.Get("own_air.mode") == 0.0);
    SC_EXPECT(before.OwnCorridors().empty());
}
