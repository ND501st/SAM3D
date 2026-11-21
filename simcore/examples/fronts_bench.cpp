// fronts_bench: what track F's fronts cost (docs/fronts/README.md, "Efficiency"). Not a test: a table to read.
//
// Usage: fronts_bench [data_dir] --air-front [--days N] [--seeds N] [--sectors N] [--theatre file] [--moves] [--trace] [faction ...]
//        fronts_bench [data_dir] --ground [--days N] [--seeds N] [--campaign-days N] [--campaign-seeds N] [--gate] [--log] [faction ...]
//        fronts_bench [data_dir] --watch [--days N] [--seeds N] [--sectors N] [--theatre file] [faction ...]
//   --air-front (F2.2-F2.3, the only part built so far): each faction's campaign run with air_front.mode 1 for N days
//   (default 7) over N seeds (default 3) on the theatre (default theatre_pannonia_nation.json), its front cut into
//   --sectors equal sectors (default 8, the most the design gives a front; 0: the theatre's own). At each day's end the
//   air staff's per-cycle work is timed over 2000 repetitions on that day's picture: the measure of every sector's cover
//   alone (SectorCoverBook::Measure) with the bands kept and with every band moved each time (every baseline taken afresh
//   from the pre-war picture, the worst a cycle can be), and the measure with the decision on it (DecideAirFront on a copy
//   of the day's state: the cycle as the campaign runs it, F2.3). Prints the CPU time per cycle (mean and worst day)
//   against the budget, 1 ms per planning cycle for a front; the campaign's whole CPU time with the mode off and on (the
//   same seeds), which must be the same war while the air front never moved (the same packages, and the same journal but
//   for the air front's own entries; once it moves the planner, routes and C2 read it, F2.4, and the war parts); the
//   packages planned off and on, and of those on, the low-level ones flown past their kind's fixed depth (close support
//   40 km, helicopter raids 25, harassment 70: only an air front takes them there); and how the air front moved
//   (advances, pull-backs, the deepest, the sectors moved and the depths at the end, the first seed's). --moves prints each seed's air front entries; --trace each
//   day's end by sector: the depths, the cover and the mobile air defence against their baselines, the next band's worth
//   and the low-level sorties and loss rate in the loss window.
//   --watch (F2.5): each faction's campaign at air_front.mode 1 (the theatre's own sectors unless --sectors) with our side's
//   information pipeline pumped after every step (data/command/chain_pannonia.json, the intercept service of intercept.json):
//   when and where the enemy's air front moved (his journal) against the signs our army's air defence staff had and when it
//   concluded, and when the regiment's commander and battery 21's had its conclusion.
//   --ground (F3.3): the ground war at U0 alone, built as the campaign builds it on the nation (ground.mode 1), stepped
//   hour by hour for N days (default 30) over N seeds (default 5) with each campaign's weather: a campaign day's cost by
//   part (combat, decisions, movement, line; picture and reports are the campaign's), the mean and the worst day against
//   20 ms and 60 ms; then --campaign-days (default 10, 0: none) of whole campaigns over --campaign-seeds (default 1) at
//   ground.mode 0 and 1: their CPU, the ground war's own time and its share of the campaign (budget 5%: 1.05x), and the
//   line's depth by sector at each day's end (a day's move of 300 km or more is a runaway). Factions default to all
//   four. --gate exits 2 over any budget. --log (F3.4): the first seed's commanders' decisions as they happen; the axes'
//   line ends with the decisions made.
// Factions default to warsaw_pact and nato (--ground: all four). CPU time by std::clock (docs/air_battle/01_ROUTES.md: wall times move with
// the machine's other load). Exits 2 when a cycle is over budget or the war differs with the mode on and no air front moved.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/command.hpp"
#include "simcore/ground_war.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

bool g_ground_log = false;  // --ground --log: the first seed's ground log (the commanders' decisions), as it happens

void Mix(std::uint64_t& h, std::uint64_t v) {
    h ^= v;
    h *= 1099511628211ull;
}

// The journal's text, the air front's own entries left out (they are the mode's, and nothing reads them yet).
std::uint64_t JournalHash(const AirCampaign& c) {
    std::uint64_t h = 1469598103934665603ull;
    for (const auto& j : c.Journal()) {
        if (j.kind == "air_front") continue;
        for (char ch : j.text) Mix(h, static_cast<unsigned char>(ch));
        Mix(h, 0x0a);
    }
    return h;
}

// The packages planned: their ids, sizes, times on target and targets.
std::uint64_t PackageHash(const AirCampaign& c) {
    std::uint64_t h = 1469598103934665603ull;
    for (const Package& p : c.Packages()) {
        Mix(h, static_cast<std::uint64_t>(p.id));
        Mix(h, static_cast<std::uint64_t>(p.Aircraft()));
        Mix(h, static_cast<std::uint64_t>(static_cast<std::int64_t>(p.tot_h * 1000.0)));
        for (char ch : p.target_key) Mix(h, static_cast<unsigned char>(ch));
    }
    return h;
}

double CpuSeconds() { return static_cast<double>(std::clock()) / static_cast<double>(CLOCKS_PER_SEC); }

void CutSectors(AirWarData& d, int n) {
    for (FrontDef& f : d.theatre.fronts) {
        if (f.faction != d.faction.front || n < 1) continue;
        double length = 0.0;
        for (std::size_t i = 1; i < f.line.size(); ++i) length += Dist2d(f.line[i - 1], f.line[i]);
        f.sectors.clear();
        for (int k = 0; k < n; ++k) {
            SectorDef s;
            s.id = "s" + std::to_string(k + 1);
            s.name = s.id;
            s.from_m = length * k / n;
            s.to_m = length * (k + 1) / n;
            f.sectors.push_back(s);
        }
    }
}

// F2.5: the enemy's air front's moves against what our side concluded of them, one campaign.
void WatchCampaign(const AirWarData& d, unsigned seed, int days, const command::ChainData& chain, const command::InterceptData& idata) {
    AirCampaign c;
    c.Init(d, seed);
    command::InfoNet net;
    net.Init(chain, seed, d.theatre.start_hour);
    command::GiveTheatreSky(net, d.theatre);
    command::CampaignFeed feed;
    feed.Bind(&net);
    feed.UseIntercept(idata);
    const double t0 = CpuSeconds();
    while (c.Now() < days * 24.0) {
        if (c.Advance(days * 24.0)) c.SkipPlayerMission();
        feed.Pump(c);
    }
    const double cpu = CpuSeconds() - t0;
    const double start = d.theatre.start_hour;
    std::printf("%s, seed %u, %d days (%.1f s CPU with the pipeline): %zu facts in the pipeline\n", d.faction.id.c_str(), seed, days, cpu,
                net.Facts().size());
    std::printf("  the enemy's air front (his journal):\n");
    int moves = 0;
    for (const JournalEntry& j : c.Journal()) {
        if (j.kind != "air_front") continue;
        ++moves;
        std::printf("    %s  %s\n", command::FormatClock(j.h, start).c_str(), j.text.c_str());
    }
    if (moves == 0) std::printf("    no move\n");
    const LineOfContact& line = c.FrontLine();
    std::printf("  the signs sent to our army's air defence staff, by sector (attacks on troops, helicopters, intercepts; reached it):\n");
    for (std::size_t s = 0; s < line.SectorCount(); ++s) {
        int n[4] = {0, 0, 0, 0}, reached = 0;
        for (const command::FrontSignReport& r : feed.Signs()) {
            if (r.sector != static_cast<int>(s)) continue;
            ++n[static_cast<int>(r.sign)];
            reached += r.known_h >= 0.0 ? 1 : 0;
        }
        std::printf("    %-14s %-24s %3d %3d %3d  %3d   the enemy's depth at the end %4.0f km, ours %4.0f km\n", line.Sector(s).id.c_str(),
                    line.Sector(s).name.c_str(), n[0], n[1], n[2], reached, s < c.AirFrontDepths().size() ? c.AirFrontDepths()[s] / 1000.0 : 0.0,
                    feed.Watch().Believed(static_cast<int>(s)) / 1000.0);
    }
    std::printf("  our staff's conclusions:\n");
    const int rgt = net.Data().PositionIndex("rgt_commander"), bty = net.Data().PositionIndex("bty21_commander");
    for (const command::AirFrontConclusion& k : feed.Watch().Conclusions()) {
        const int fi = net.FactIndex(k.key);
        const command::Delivery* r = net.Known(rgt, fi);
        const command::Delivery* b = net.Known(bty, fi);
        std::printf("    %s  %-14s %2.0f -> %2.0f km on %d signs (the last reached the staff %s); the regiment %s, battery 21 %s\n",
                    command::FormatClock(k.h, start).c_str(), line.Sector(static_cast<std::size_t>(k.sector)).id.c_str(), k.from_m / 1000.0,
                    k.depth_m / 1000.0, k.signs, command::FormatClock(k.decided_h, start).c_str(),
                    r != nullptr ? command::FormatClock(r->h, start).c_str() : "not yet", b != nullptr ? command::FormatClock(b->h, start).c_str() : "not yet");
    }
    if (feed.Watch().Conclusions().empty()) std::printf("    none\n");
    std::printf("\n");
}

// ------------------------------------------------------------------ the ground war (F3.3)

struct GroundLine {
    double mean_ms = 0.0;   // a campaign day, over every day of every seed
    double worst_ms = 0.0;  // the worst day
    double part_ms[groundwar::GroundProfile::kParts] = {};  // by part, a day's mean
    int days = 0;
};

// The ground war's log as it stands: one line an entry (the order's name, who decided, about whom, where, its value).
void PrintGroundLog(const groundwar::GroundWar& g) {
    static const char* const kNotes[] = {"axis_starts", "river_reached", "river_crossed", "axis_halted", "day_line", "order"};
    for (const groundwar::GroundLogEntry& l : g.Log()) {
        if (l.what == groundwar::GroundNote::DayLine) continue;
        const char* what = l.what == groundwar::GroundNote::Order ? groundwar::GroundOrderName(l.order) : kNotes[static_cast<int>(l.what)];
        const std::string who = l.cmdr >= 0 ? g.Commanders()[static_cast<std::size_t>(l.cmdr)].Title() : std::string("-");
        const std::string unit = l.formation >= 0 ? g.Formations()[static_cast<std::size_t>(l.formation)].unit.id : std::string("-");
        const std::string axis = l.axis >= 0 ? g.Axes()[static_cast<std::size_t>(l.axis)].id : std::string("-");
        std::printf("    d%d %02dh %-22s %-28s %-16s %-22s %8.2f %s\n", static_cast<int>(l.h / 24.0) + 1, static_cast<int>(l.h) % 24, what, who.c_str(),
                    unit.c_str(), axis.c_str(), l.value, l.what == groundwar::GroundNote::AxisHalted ? groundwar::HaltReasonName(l.reason) : "");
    }
}

// The ground war alone, as the campaign builds it, stepped hour by hour for `days` days with the campaign's weather and
// its objects at the start (bridges standing): the per-day cost by part, steady_clock in the war's own profile.
bool GroundAlone(const AirWarData& d, unsigned seed, int days, GroundLine& acc, std::string& axes_end) {
    auto gd = std::make_shared<groundwar::GroundWarData>();
    std::string e;
    if (!groundwar::LoadGroundWarData(d.data_dir, d.faction.id, *gd, &e)) {
        std::printf("%s\n", e.c_str());
        return false;
    }
    const FrontDef* front = d.theatre.FrontFor(d.faction.front);
    if (front == nullptr || d.country == nullptr) return false;
    AirCampaign c;  // for its weather and its objects
    if (!c.Init(d, seed, &e)) return false;
    std::vector<airwar::Weather> weather;
    for (int day = 1; day <= days + 1; ++day) weather.push_back(c.WeatherOn(day));
    LineOfContact line;
    line.Build(*front, d.tuning.Get("air_front.station_m"));
    groundwar::GroundWar g;
    if (!g.Build(gd, line, d.theatre, d.country.get(), seed, &e)) {
        std::printf("%s\n", e.c_str());
        return false;
    }
    groundwar::GroundEnv env;
    env.theatre = &d.theatre;
    env.weather = &weather;
    env.truth = &c.Truth();
    g.SetProfiling(true);
    const bool print_log = g_ground_log && axes_end.empty();
    if (print_log) std::printf("  %s, seed %u:\n", d.faction.id.c_str(), seed);
    for (int h = 1; h <= days * 24; ++h) {
        g.Step(static_cast<double>(h), env, line);
        if (print_log) PrintGroundLog(g);
        g.ClearLog();
    }
    const groundwar::GroundProfile& p = g.Profile();
    for (std::size_t k = 0; k < p.day_ms.size() && static_cast<int>(k) < days; ++k) {
        acc.mean_ms += p.day_ms[k];
        if (p.day_ms[k] > acc.worst_ms) acc.worst_ms = p.day_ms[k];
        ++acc.days;
    }
    for (int k = 0; k < groundwar::GroundProfile::kParts; ++k) acc.part_ms[k] += p.Ms(k);
    if (axes_end.empty()) {
        for (const groundwar::Axis& x : g.Axes()) {
            char buf[160];
            std::snprintf(buf, sizeof buf, "%s%s %.0f km %s", axes_end.empty() ? "" : ", ", x.id.c_str(), x.depth_m / 1000.0,
                          x.status == groundwar::AxisStatus::Halted ? groundwar::HaltReasonName(x.halted) : "on");
            axes_end += buf;
        }
        if (axes_end.empty()) axes_end = "no axes: the line holds";
        char buf[64];
        std::snprintf(buf, sizeof buf, "; %llu decisions", static_cast<unsigned long long>(p.decisions));
        axes_end += buf;
    }
    return true;
}

// --ground: the ground war's cost against docs/fronts/README.md's budget lines, and its line in the campaign.
int GroundMain(const std::string& data, const std::string& theatre, int days, int seeds, int campaign_days, int campaign_seeds, bool gate,
               std::vector<std::string> factions) {
    const double mean_budget_ms = 20.0, worst_budget_ms = 60.0, campaign_budget = 1.05;
    if (factions.empty()) factions = {"warsaw_pact", "nato", "neutral", "rebels"};
    int fails = 0;
    std::printf("The ground war at U0 (F3.3) on %s: alone %d days x %d seeds; budget %.0f ms a campaign day (mean), %.0f ms the worst day\n\n",
                theatre.c_str(), days, seeds, mean_budget_ms, worst_budget_ms);
    std::printf("%-12s %8s %8s", "faction", "mean_ms", "worst_ms");
    for (int k = 0; k < groundwar::GroundProfile::kParts; ++k) std::printf(" %10s", groundwar::GroundProfile::Name(k));
    std::printf("  %s\n", "axes at the end (the first seed)");
    std::vector<AirWarData> loaded;
    for (const std::string& faction : factions) {
        AirWarData d;
        std::string e;
        if (!LoadAirWarData(data, "air_war/" + theatre, "air_war/faction_" + faction + ".json", d, &e)) {
            std::printf("%s: %s\n", faction.c_str(), e.c_str());
            return 1;
        }
        if (!groundwar::HasGroundData(data, faction) || d.country == nullptr) {
            std::printf("%-12s no ground war (no ground file or no country map): its front holds\n", faction.c_str());
            continue;
        }
        d.tuning.values["ground.mode"] = 1.0;
        GroundLine g;
        std::string axes_end;
        for (int s = 0; s < seeds; ++s) {
            if (!GroundAlone(d, static_cast<unsigned>(7 + s), days, g, axes_end)) return 1;
        }
        const double mean = g.days > 0 ? g.mean_ms / g.days : 0.0;
        std::printf("%-12s %8.4f %8.4f", faction.c_str(), mean, g.worst_ms);
        for (int k = 0; k < groundwar::GroundProfile::kParts; ++k) std::printf(" %10.5f", g.days > 0 ? g.part_ms[k] / g.days : 0.0);
        std::printf("  %s\n", axes_end.c_str());
        if (mean > mean_budget_ms || g.worst_ms >= worst_budget_ms) {
            std::printf("  OVER BUDGET\n");
            ++fails;
        }
        loaded.push_back(d);
    }
    std::printf("  (ms a campaign day by part; \"picture\" is F3.8's and \"reports\" the journal's, both in the campaign only)\n");
    if (campaign_days > 0) {
        std::printf("\nIn the campaign: %d days x %d seeds, ground.mode 0 against 1 (std::clock CPU); budget %.2fx the campaign at 0\n\n",
                    campaign_days, campaign_seeds, campaign_budget);
        std::printf("%-12s %9s %9s %7s %9s %9s %9s %7s %7s %7s\n", "faction", "cpu_off_s", "cpu_on_s", "ratio", "ground_ms", "reports", "worst_ms",
                    "share", "pk_off", "pk_on");
        for (const AirWarData& on : loaded) {
            AirWarData off = on;
            off.tuning.values["ground.mode"] = 0.0;
            double cpu_off = 0.0, cpu_on = 0.0, ground_ms = 0.0, reports_ms = 0.0, worst_ms = 0.0;
            std::size_t packages_off = 0, packages_on = 0;
            std::vector<std::vector<double>> by_day;  // [sector][day] km, the first seed
            for (int s = 0; s < campaign_seeds; ++s) {
                const unsigned seed = static_cast<unsigned>(7 + s);
                AirCampaign a;
                a.Init(off, seed);
                double t0 = CpuSeconds();
                while (a.Now() < campaign_days * 24.0) {
                    if (a.Advance(campaign_days * 24.0)) a.SkipPlayerMission();
                }
                cpu_off += CpuSeconds() - t0;
                packages_off += a.Packages().size();
                AirCampaign b;
                b.Init(on, seed);
                b.ProfileGround(true);
                if (s == 0) by_day.assign(b.FrontLine().SectorCount(), std::vector<double>(static_cast<std::size_t>(campaign_days), 0.0));
                t0 = CpuSeconds();
                for (int day = 1; day <= campaign_days; ++day) {
                    while (b.Now() < day * 24.0) {
                        if (b.Advance(day * 24.0)) b.SkipPlayerMission();
                    }
                    if (s == 0) {
                        for (std::size_t k = 0; k < b.FrontLine().SectorCount(); ++k) {
                            by_day[k][static_cast<std::size_t>(day - 1)] = groundwar::GroundWar::SectorDepth(b.FrontLine(), k) / 1000.0;
                        }
                    }
                }
                cpu_on += CpuSeconds() - t0;
                packages_on += b.Packages().size();
                if (b.Ground() != nullptr) {
                    const groundwar::GroundProfile& p = b.Ground()->Profile();
                    for (int k = 0; k < groundwar::GroundProfile::kParts; ++k) ground_ms += p.Ms(k);
                    reports_ms += p.Ms(groundwar::GroundProfile::Reports);
                    for (double v : p.day_ms) worst_ms = v > worst_ms ? v : worst_ms;
                }
            }
            const double ratio = cpu_off > 0.0 ? cpu_on / cpu_off : 0.0;
            const double share = cpu_off > 0.0 ? ground_ms / (1000.0 * cpu_off) : 0.0;
            std::printf("%-12s %9.2f %9.2f %7.3f %9.2f %9.3f %9.4f %6.3f%% %7zu %7zu\n", on.faction.id.c_str(), cpu_off, cpu_on, ratio, ground_ms,
                        reports_ms, worst_ms, 100.0 * share, packages_off, packages_on);
            const LineOfContact* line = nullptr;
            AirCampaign names;  // the sectors' ids
            names.Init(on, 7);
            line = &names.FrontLine();
            for (std::size_t k = 0; k < by_day.size(); ++k) {
                bool moved = false;
                for (double v : by_day[k]) moved = moved || v > 0.0;
                if (!moved) continue;
                std::printf("    line in %-20s km by day (seed 7):", line->Sector(k).id.c_str());
                double last = 0.0, fastest = 0.0;
                for (double v : by_day[k]) {
                    std::printf(" %5.1f", v);
                    fastest = v - last > fastest ? v - last : fastest;
                    last = v;
                }
                std::printf("   fastest day %.1f km\n", fastest);
                if (fastest >= 300.0) {
                    std::printf("    RUNAWAY: %.0f km in a day\n", fastest);
                    ++fails;
                }
            }
            // The ground war's own time is the budget's line; the whole ratio also carries the war's divergence once the
            // line has moved (the planner, routes and C2 read it: another war, F2.4), so it is printed beside it.
            if (share > campaign_budget - 1.0) {
                std::printf("  OVER BUDGET: the ground war is %.2f%% of the campaign\n", 100.0 * share);
                ++fails;
            }
        }
    }
    if (gate && fails > 0) {
        std::printf("\n%d over budget\n", fails);
        return 2;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string data = "data", theatre = "theatre_pannonia_nation.json";
    int days = 7, seeds = 3, sectors = 8, campaign_days = 10, campaign_seeds = 1;
    bool air_front = false, print_moves = false, trace = false, watch = false, sectors_given = false, ground = false, gate = false,
         days_given = false, seeds_given = false;
    std::vector<std::string> factions;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--air-front") {
            air_front = true;
        } else if (a == "--ground") {
            ground = true;
        } else if (a == "--gate") {
            gate = true;
        } else if (a == "--log") {
            g_ground_log = true;
        } else if (a == "--campaign-days" && i + 1 < argc) {
            campaign_days = std::atoi(argv[++i]);
        } else if (a == "--campaign-seeds" && i + 1 < argc) {
            campaign_seeds = std::atoi(argv[++i]);
        } else if (a == "--watch") {
            watch = true;
        } else if (a == "--moves") {
            print_moves = true;
        } else if (a == "--trace") {
            trace = true;
        } else if (a == "--days" && i + 1 < argc) {
            days = std::atoi(argv[++i]);
            days_given = true;
        } else if (a == "--seeds" && i + 1 < argc) {
            seeds = std::atoi(argv[++i]);
            seeds_given = true;
        } else if (a == "--sectors" && i + 1 < argc) {
            sectors = std::atoi(argv[++i]);
            sectors_given = true;
        } else if (a == "--theatre" && i + 1 < argc) {
            theatre = argv[++i];
        } else if (i == 1 && a.rfind("--", 0) != 0) {
            data = a;
        } else {
            factions.push_back(a);
        }
    }
    if (ground) return GroundMain(data, theatre, days_given ? days : 30, seeds_given ? seeds : 5, campaign_days, campaign_seeds, gate, factions);
    if (factions.empty()) factions = {"warsaw_pact", "nato"};
    if (watch) {
        command::ChainData chain;
        command::InterceptData idata;
        std::string e;
        if (!command::LoadChainData(data, "command/chain_pannonia.json", chain, &e) || !command::LoadInterceptData(data, "command/intercept.json", idata, &e)) {
            std::printf("%s\n", e.c_str());
            return 1;
        }
        std::printf("Our side learns the enemy's air front (F2.5) on %s, %s sectors, %d days, %d seeds, air_front.mode 1\n\n", theatre.c_str(),
                    sectors_given && sectors > 0 ? (std::to_string(sectors) + " equal").c_str() : "the theatre's", days, seeds);
        for (const std::string& faction : factions) {
            AirWarData d;
            if (!LoadAirWarData(data, "air_war/" + theatre, "air_war/faction_" + faction + ".json", d, &e)) {
                std::printf("%s: %s\n", faction.c_str(), e.c_str());
                return 1;
            }
            if (sectors_given) CutSectors(d, sectors);
            d.tuning.values["air_front.mode"] = 1.0;
            for (int s = 0; s < seeds; ++s) WatchCampaign(d, static_cast<unsigned>(7 + s), days, chain, idata);
        }
        return 0;
    }
    if (!air_front) {
        std::printf("fronts_bench: --air-front (F2.2-F2.4), --watch (F2.5) or --ground (F3.3)\n");
        return 1;
    }
    const double budget_ms = 1.0;
    int fails = 0;
    std::printf("The air front's measure, decision and readers (F2.2-F2.4) on %s, %s sectors, %d days, %d seeds; budget %.1f ms a planning cycle\n\n",
                theatre.c_str(), sectors > 0 ? (std::to_string(sectors) + " equal").c_str() : "the theatre's", days, seeds, budget_ms);
    std::printf("%-12s %7s %7s %9s %9s %9s %9s %9s %7s %9s %9s %5s %6s %6s %6s %5s %5s %5s %5s  %s\n", "faction", "sectors", "entries", "mean_ms",
                "worst_ms", "moved_ms", "cycle_ms", "cyc_worst", "prewar", "cpu_off_s", "cpu_on_s", "same", "pk_off", "pk_on", "deeper", "adv",
                "back", "deep", "moved", "depths at the end, km (first seed)");
    for (const std::string& faction : factions) {
        AirWarData d;
        std::string e;
        if (!LoadAirWarData(data, "air_war/" + theatre, "air_war/faction_" + faction + ".json", d, &e)) {
            std::printf("%s: %s\n", faction.c_str(), e.c_str());
            return 1;
        }
        CutSectors(d, sectors);
        AirWarData on = d;
        on.tuning.values["air_front.mode"] = 1.0;
        double sum_ms = 0.0, worst_ms = 0.0, worst_moved_ms = 0.0, cycle_sum_ms = 0.0, cycle_worst_ms = 0.0, entries = 0.0, prewar = 0.0,
               cpu_off = 0.0, cpu_on = 0.0, deepest_km = 0.0;
        std::size_t sector_count = 0;
        int samples = 0, advances = 0, pullbacks = 0, moved_sectors = 0, packages_off = 0, packages_on = 0, past_fixed = 0;
        bool same = true;
        std::string depths_end;
        for (int s = 0; s < seeds; ++s) {
            const unsigned seed = static_cast<unsigned>(7 + s);
            double t0 = CpuSeconds();
            AirCampaign off;
            off.Init(d, seed);
            while (off.Now() < days * 24.0) {
                if (off.Advance(days * 24.0)) off.SkipPlayerMission();
            }
            cpu_off += CpuSeconds() - t0;
            t0 = CpuSeconds();
            AirCampaign c;
            c.Init(on, seed);
            prewar += static_cast<double>(c.CoverBook().PrewarUnits());
            double measured = 0.0;
            for (int day = 1; day <= days; ++day) {
                while (c.Now() < day * 24.0) {
                    if (c.Advance(day * 24.0)) c.SkipPlayerMission();
                }
                const double run_until = CpuSeconds();
                SectorCoverBook book = c.CoverBook();
                std::vector<SectorCover> out;
                sector_count = c.FrontLine().SectorCount();
                const double step_m = on.tuning.Get("air_front.step_m");
                const std::vector<double> depth(sector_count, 0.0), deeper(sector_count, step_m);
                const int reps = 2000;  // std::clock ticks in ms: 2000 measures of some microseconds each
                book.Measure(c.Intel(), c.FrontLine(), depth, step_m, out);  // the kept vectors sized once, as the campaign's are
                double m0 = CpuSeconds();
                for (int r = 0; r < reps; ++r) book.Measure(c.Intel(), c.FrontLine(), depth, step_m, out);
                const double ms = (CpuSeconds() - m0) * 1000.0 / reps;
                m0 = CpuSeconds();
                for (int r = 0; r < reps; ++r) book.Measure(c.Intel(), c.FrontLine(), (r % 2) == 0 ? deeper : depth, step_m, out);
                const double moved_ms = (CpuSeconds() - m0) * 1000.0 / reps;
                // The cycle as the campaign runs it: the measure at the day's depths, then the decision on a copy of its state.
                AirFrontRules rules;
                AirFrontInputs in;
                AirFrontChief who;
                c.AirFrontDecisionInputs(rules, in, who);
                in.cover = &out;
                AirFrontState st;
                std::vector<AirFrontMove> moves;
                std::vector<double> reward, scratch, now_depth;
                c.AirFront().Depths(now_depth);
                book.Measure(c.Intel(), c.FrontLine(), now_depth, step_m, out);
                m0 = CpuSeconds();
                for (int r = 0; r < reps; ++r) {
                    book.Measure(c.Intel(), c.FrontLine(), now_depth, step_m, out);
                    st = c.AirFront();
                    DecideAirFront(on.faction.air_front, rules, in, who, st, moves, &reward, &scratch);
                }
                const double cycle_ms = (CpuSeconds() - m0) * 1000.0 / reps;
                if (trace) {
                    SectorRewards(c.Intel(), c.FrontLine(), now_depth, rules.reward_reach_m, step_m, on.faction.politics.target_weight, &c.Politics().banned, reward);
                    if (day == 1) {  // what the enemy believes is worth flying for within 60 km of the line
                        for (const IntelEntry& en : c.Intel().entries) {
                            if (!IsAirFrontReward(en.kind)) continue;
                            const FrontPlace pl = c.FrontLine().Place(en.pos);
                            const double dep = pl.side * pl.distance_m - c.FrontLine().Offset(pl.along_m);
                            if (dep < -5000.0 || dep > 60000.0) continue;
                            std::printf("  %s target %-20s %-12s sector %-16s depth %5.1f km value %.2f live %.2f weight %.2f\n", faction.c_str(),
                                        en.key.c_str(), ObjectKindName(en.kind),
                                        c.FrontLine().Sector(static_cast<std::size_t>(c.FrontLine().SectorAt(pl.along_m))).id.c_str(), dep / 1000.0,
                                        en.value, en.Live(), on.faction.politics.target_weight[static_cast<int>(en.kind)]);
                        }
                    }
                    for (std::size_t k = 0; k < out.size(); ++k) {
                        const SectorFront& f = c.AirFront().sectors[k];
                        std::printf("  %s seed %u day %d %-4s depth %2.0f/%2.0f cover %.2f (sam %.1f of %.1f) mobile gone %.2f (%.1f of %.1f) worth %5.2f sorties %3d rate %.3f%s\n",
                                    faction.c_str(), seed, day, c.FrontLine().Sector(k).id.c_str(), f.depth_m / 1000.0, f.helo_depth_m / 1000.0, out[k].cover,
                                    out[k].sam_working, out[k].sam_baseline, out[k].mobile_destroyed, out[k].mobile_working, out[k].mobile_baseline, reward[k],
                                    c.SectorLosses().Sorties(static_cast<int>(k), SectorLossWindow::DayOf(c.Now()), on.faction.air_front.loss_days),
                                    c.SectorLosses().Rate(static_cast<int>(k), SectorLossWindow::DayOf(c.Now()), on.faction.air_front.loss_days),
                                    f.contested ? " contested" : "");
                    }
                }
                cycle_sum_ms += cycle_ms;
                cycle_worst_ms = cycle_ms > cycle_worst_ms ? cycle_ms : cycle_worst_ms;
                worst_moved_ms = moved_ms > worst_moved_ms ? moved_ms : worst_moved_ms;
                measured += CpuSeconds() - run_until;
                sum_ms += ms;
                worst_ms = ms > worst_ms ? ms : worst_ms;
                entries += static_cast<double>(c.Intel().entries.size());
                ++samples;
            }
            cpu_on += CpuSeconds() - t0 - measured;
            same = same && JournalHash(off) == JournalHash(c) && PackageHash(off) == PackageHash(c) && off.Packages().size() == c.Packages().size();
            packages_off += static_cast<int>(off.Packages().size());
            packages_on += static_cast<int>(c.Packages().size());
            for (const Package& p : c.Packages()) {  // low-level work past its fixed depth: only the air front takes it there
                const double fixed_m = p.kind == MissionKind::Cas ? 40000.0 : p.kind == MissionKind::HeloRaid ? 25000.0 : p.kind == MissionKind::Harassment ? 70000.0 : -1.0;
                if (fixed_m > 0.0 && c.FrontLine().AbsDistance(p.target_pos) > fixed_m + 1.0) ++past_fixed;
            }
            for (const SectorFront& f : c.AirFront().sectors) {
                advances += f.advances;
                pullbacks += f.pullbacks;
                moved_sectors += f.advances > 0 ? 1 : 0;
                deepest_km = f.depth_m / 1000.0 > deepest_km ? f.depth_m / 1000.0 : deepest_km;
            }
            if (print_moves) {
                for (const JournalEntry& j : c.Journal()) {
                    if (j.kind == "air_front") {
                        std::printf("  %s seed %u %s %s: %s\n", faction.c_str(), seed, FormatHour(j.h, on.theatre.start_hour).c_str(), j.who.c_str(),
                                    j.text.c_str());
                    }
                }
            }
            if (s == 0) {
                for (const SectorFront& f : c.AirFront().sectors) {
                    char buf[64];
                    std::snprintf(buf, sizeof buf, "%s%.0f/%.0f", depths_end.empty() ? "" : " ", f.depth_m / 1000.0, f.helo_depth_m / 1000.0);
                    depths_end += buf;
                }
            }
        }
        const double mean_ms = samples > 0 ? sum_ms / samples : 0.0;
        const double cycle_mean_ms = samples > 0 ? cycle_sum_ms / samples : 0.0;
        std::printf("%-12s %7zu %7.0f %9.4f %9.4f %9.4f %9.4f %9.4f %7.1f %9.2f %9.2f %5s %6d %6d %6d %5d %5d %5.0f %5d  %s\n", faction.c_str(),
                    sector_count, samples > 0 ? entries / samples : 0.0, mean_ms, worst_ms, worst_moved_ms, cycle_mean_ms, cycle_worst_ms, prewar / seeds,
                    cpu_off, cpu_on, same ? "yes" : "no", packages_off, packages_on, past_fixed, advances, pullbacks, deepest_km, moved_sectors,
                    depths_end.c_str());
        if (worst_ms > budget_ms || worst_moved_ms > budget_ms || cycle_worst_ms > budget_ms || (!same && advances == 0)) ++fails;
    }
    std::printf("\nentries: the enemy's picture at a day's end (mean); mean_ms, worst_ms: a measure with the bands kept (mean, worst day);\n"
                "moved_ms: one with every sector's band moved (worst day); cycle_ms, cyc_worst: the measure and the decision on it, the\n"
                "planning cycle's air front work (mean, worst day); prewar: the entries the sectors' baselines keep; cpu_*: the campaigns'\n"
                "CPU seconds, air_front.mode 0 and 1, the timing repetitions left out; same: the same packages and journal (the air front's\n"
                "entries aside; once it moves the planner, routes and C2 read it, F2.4); pk_off, pk_on: the packages planned, mode 0 and 1;\n"
                "deeper: those of mode 1 flying close support, helicopter raids or harassment past the kind's fixed depth (40, 25, 70 km);\n"
                "adv, back: the air front's steps forward and back over every seed and sector; deep: the deepest at the end;\n"
                "moved: sectors that moved at all (over the seeds); depths: each sector's fixed-wing/helicopter front at the end.\n");
    return fails > 0 ? 2 : 0;
}
