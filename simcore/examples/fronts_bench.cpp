// fronts_bench: what track F's fronts cost (docs/fronts/README.md, "Efficiency"). Not a test: a table to read.
//
// Usage: fronts_bench [data_dir] --air-front [--days N] [--seeds N] [--sectors N] [--theatre file] [--moves] [--trace] [faction ...]
//        fronts_bench [data_dir] --ground [--days N] [--seeds N] [--campaign-days N] [--campaign-seeds N] [--player-sector id|battery]
//                     [--gate] [--log] [faction ...]
//        fronts_bench [data_dir] --watch [--days N] [--seeds N] [--sectors N] [--theatre file] [faction ...]
//        fronts_bench [data_dir] --feed [--days N] [--seeds N] [--theatre file] [--gate] [faction ...]
//        fronts_bench [data_dir] --ad [--days N] [--seeds N] [--theatre file] [--gate] [--log] [--no-fighters] [faction ...]
//        fronts_bench [data_dir] --history [--days N] [--seeds N] [--theatre file] [--gate] [faction ...]
//        fronts_bench [data_dir] --player [--days N] [--seeds N] [--theatre file] [--set tuning.key=value ...] [--battery-sector id
//                     [--battery-back-km km]] [--gate] [faction ...]
//        fronts_bench [data_dir] --map post[,post...] [--map-json out.json] [--days N] [--seeds N] [--theatre file] [faction ...]
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
//   part (combat, decisions, movement, line, picture: each side's picture's reports, F3.5; reports are the campaign's), the mean and the worst day against
//   20 ms and 60 ms; then --campaign-days (default 10, 0: none) of whole campaigns over --campaign-seeds (default 1) at
//   ground.mode 0 and 1: their CPU, the ground war's own time and its share of the campaign (budget 5%: 1.05x; a campaign
//   under 0.1 s of CPU at mode 0 is under std::clock's resolution for a share, so there the ground war's own ms a
//   campaign day is held to the 20 ms budget instead: F3.7), and the line's depth by sector at each day's end (a day's move
//   of 300 km or more is a runaway). Factions default to all
//   four. --gate exits 2 over any budget. --log (F3.4): the first seed's commanders' decisions as they happen; the axes'
//   line ends with the decisions made. --player-sector (F3.6): the ground war alone run again with the same seeds and U1
//   centred on that sector (an id, or "battery": the player's battery's sector, as the campaign has it): U1's extra cost a
//   campaign day by part (mean and the worst day) against 15 ms and 45 ms, the levels' changes, the regiments' decisions,
//   the route service's asks, searches, cache hits and fall-backs, and the marches' hours on the roads against F3.4's
//   straight line x 1.3; --gate covers it.
//   --feed (F3.8): each faction's campaign at ground.mode 1 and air_front.mode 1 with our side's pipeline pumped after every
//   step, with our units' reports into it and without (the same seeds): the reports each post compiled, how many reached the
//   army's operations section, the SAM regiment's staff and the battery commander and after how long, the army's summaries
//   to the national sector, the orders, the intelligence summaries, the enemy's air strikes on our troops as our units counted
//   them told to the air defence staff's watch and what the watch made of them (against the run without the reports), and the
//   pipeline's CPU a campaign day with the reports against without (budget 20 ms). Defaults 10 days, 3 seeds, all four factions.
//   --ad (F4.1): each faction's campaign at ground.mode 1, air_front.mode 1 and ad_strategy.mode 1 with our side's pipeline
//   pumped after every step, and again at ad_strategy.mode 0 (the same seeds): our air defence commanders' reviews and
//   changes by sector, each sector's priority over time (the first seed's changes as they were ordered; --log: every
//   seed's), how long from the newest fact a change rested on reaching the commander's post to his order reaching the SAM
//   regiment's commander (the army's chief's) or the fighter regiment (the national sector's), every fact a change rested
//   on checked held by his post by the decision's hour (a change on a fact that had not reached it is counted and fails
//   the gate), whether the air war differs with the commanders on and off (from F4.2 it does: the units move on their
//   orders), and the commanders' CPU a campaign day (the pump's with them against without, the order source's pumps
//   inside the steps counted; budget 5 ms, 04 section 10). F4.2: the campaign asks the feed for the orders its units heard
//   (AirCampaign::SetOrderSource); the moves begun by class with their mean march, the orders (withdrawals and cover),
//   heard and ready, the units overrun against those that got away on a move, the hours off the air per unit that moves
//   on orders, the packages and the loss rate a sortie against mode 0 (F4.1's air war), and the campaign's and the
//   pump's CPU together a day. Defaults 10 days, 3 seeds, warsaw_pact and nato. F4.3: the fighter regiment's duty by sector
//   as its post held the national sector's commander's priorities: each change (its order's hour, when the post had it,
//   when the shares changed; the first seed's, --log every seed's), the delay from the order to the shares changing, the
//   cover by sector over time (each sector's mean over the campaign and at its end), the sum over the sectors against the
//   front's (kept), and the abstract air fights, the enemy's aircraft our fighters shot down and ours lost, against mode 0.
//   --no-fighters: the commanders without the data's "fighters" block (F4.2's war: the front's number everywhere).
//   --map (F5.1): each faction's campaign with every front mode on and our pipeline pumped after every step, then the situation
//   maps (command/situation_map.hpp) of the named posts (ids, comma separated) at the end, each mark with its source and age,
//   and each against the truth, which only this bench reads: per axis the reported depth against the true one and the age of
//   the report, the believed line against the true line over the base line's stations (mean and worst difference in km), whether
//   the posts' maps differ, and the CPU a map takes to build. Defaults 10 days, 1 seed, warsaw_pact and nato.
//   --history (F6.1): each faction's war (default all four) with ground.mode 1, air_front.mode 2 (F7.2; --set air_front.mode=1 for
//   F7.1's) and ad_strategy.mode 1 and our pipeline
//   pumped after every step, N days (default 14) over N seeds (default 3), sampled every hour, against the bands of
//   data/air_war/balance_bands.json whose tool is fronts_bench (docs/fronts/06_BALANCE.md section 2): a formation's day in contact,
//   each army's losses a day, the attacker's losses over the defender's, an axis's day of attack and its fastest, no advance under
//   the stall ratio, the security zone's hours, the enemy's axes won, our formations withdrawn, our counterattacks on a
//   penetration, the air's share of our losses and a strike's toll, the air front's latency, pull-back and steps, our radar SAM
//   units lost and back, an S-75 class move and a unit's moves a day, the enemy's loss rate a sortie. A band marked "argued" is
//   printed and never fails; --gate exits 2 when another is out.
//   --player (F6.2): each faction's war (default all four) as --history runs it, N days (default 7) over N seeds (default 3),
//   twice on each seed: the player's battery careless and careful (air_war_balance's two postures: the moves and the
//   emission discipline; its kills, its fate and what the enemy knows of it measured). The difference in the battery's sector
//   (its sector at the war's start): the line of contact at the end, the air front's steps and the hour of its first step;
//   elsewhere the line's difference over the draws' spread (the same posture's seeds against each other); the outcomes'
//   ledger of the air front's delay the battery bought (campaign/outcomes.hpp). Against the bands "player_*" of
//   balance_bands.json (docs/fronts/06_BALANCE.md section 4); --gate exits 2 when one is out. --set tries a tuning value in
//   both wars (a sensitivity run: air_front.reward_reach_m=0 weighs the targets already in reach, so the air front moves).
//   --battery-sector id (F7.3): the battery put in that sector at the war's start (--battery-back-km behind the base line at
//   its middle, default 20), so the air front's first step there, and its delay careful against careless, can be measured.
// Factions default to warsaw_pact and nato (--ground: all four). CPU time by std::clock (docs/air_battle/01_ROUTES.md: wall times move with
// the machine's other load). Exits 2 when a cycle is over budget or the war differs with the mode on and no air front moved.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <ctime>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/campaign/outcomes.hpp"
#include "simcore/command.hpp"
#include "simcore/ground_war.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

// The air front's mode the fronts' runs use (--feed, --ad, --history, --player, --map): 2 since F7.2, a step's worth
// counting the close support of the enemy's own advancing axes (air_front.hpp); --set air_front.mode=1 runs F7.1's.
constexpr double kFrontsAirFrontMode = 2.0;

bool g_ground_log = false;  // --ground --log: the first seed's ground log (the commanders' decisions), as it happens
// --player --battery-sector id (F7.3): the player's battery put at the war's start in that sector, g_battery_back_km behind the
// base line at the sector's middle, so its effect is measured where an axis runs (the default: where the theatre has it).
std::string g_battery_sector;
double g_battery_back_km = 20.0;

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
    std::vector<double> day_ms;  // every seed's days in order (F3.6: U1's extra day by day)
    // F3.6: the levels, the regiments, the route service and the marches.
    std::uint64_t level_changes = 0, regiment_decisions = 0, asks = 0, finds = 0, hits = 0, fallbacks = 0, marches = 0;
    double route_ms = 0.0, march_h = 0.0, straight_h = 0.0;
    std::size_t regiments = 0;
};

// The ground war's log as it stands: one line an entry (the order's name, who decided, about whom, where, its value).
void PrintGroundLog(const groundwar::GroundWar& g) {
    static const char* const kNotes[] = {"axis_starts", "river_reached", "river_crossed", "axis_halted", "day_line", "order", "air_attack"};
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
bool GroundAlone(const AirWarData& d, unsigned seed, int days, GroundLine& acc, std::string& axes_end, const std::string& player = "") {
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
    env.intel = &c.Intel();  // the enemy's pre-war picture of our troops (F3.5's air sightings)
    // U1's centre (F3.6): a sector by id, or the player's battery's as the campaign placed it.
    if (player == "battery" && d.theatre.PlayerBattery() != nullptr) {
        const int pb = d.theatre.IndexOf(d.theatre.PlayerBattery()->id);
        if (pb >= 0) env.player_sector = line.SectorOf(c.Truth()[static_cast<std::size_t>(pb)].pos);
    } else if (!player.empty()) {
        for (std::size_t k = 0; k < line.SectorCount(); ++k) {
            if (line.Sector(k).id == player) env.player_sector = static_cast<int>(k);
        }
    }
    if (!player.empty() && env.player_sector < 0) {
        std::printf("%s: no sector %s on its front\n", d.faction.id.c_str(), player.c_str());
        return false;
    }
    g.SetProfiling(true);
    const bool print_log = g_ground_log && axes_end.empty();
    if (print_log) std::printf("  %s, seed %u:\n", d.faction.id.c_str(), seed);
    for (int h = 1; h <= days * 24; ++h) {
        g.Step(static_cast<double>(h), env, line);
        if (print_log) PrintGroundLog(g);
        g.ClearLog();
    }
    const groundwar::GroundProfile& p = g.Profile();
    for (int k = 0; k < days; ++k) {
        const double ms = static_cast<std::size_t>(k) < p.day_ms.size() ? p.day_ms[static_cast<std::size_t>(k)] : 0.0;
        acc.mean_ms += ms;
        if (ms > acc.worst_ms) acc.worst_ms = ms;
        acc.day_ms.push_back(ms);
        ++acc.days;
    }
    for (int k = 0; k < groundwar::GroundProfile::kParts; ++k) acc.part_ms[k] += p.Ms(k);
    acc.level_changes += p.level_changes;
    acc.regiment_decisions += p.regiment_decisions;
    acc.asks += g.RouteStats().asks;
    acc.finds += g.RouteStats().finds;
    acc.hits += g.RouteStats().hits;
    acc.fallbacks += p.route_fallbacks;
    acc.marches += p.marches;
    acc.route_ms += static_cast<double>(p.route_ns) * 1e-6;
    acc.march_h += p.march_h;
    acc.straight_h += p.march_straight_h;
    acc.regiments = g.Regiments().size();
    if (axes_end.empty()) {
        for (const groundwar::Axis& x : g.Axes()) {
            char buf[160];
            std::snprintf(buf, sizeof buf, "%s%s %.0f km %s", axes_end.empty() ? "" : ", ", x.id.c_str(), x.depth_m / 1000.0,
                          x.status == groundwar::AxisStatus::Halted ? groundwar::HaltReasonName(x.halted) : "on");
            axes_end += buf;
        }
        if (axes_end.empty()) axes_end = "no axes: the line holds";
        char buf[96];
        std::snprintf(buf, sizeof buf, "; %llu decisions", static_cast<unsigned long long>(p.decisions));
        axes_end += buf;
    }
    return true;
}

// --ground: the ground war's cost against docs/fronts/README.md's budget lines, and its line in the campaign.
// --player-sector (F3.6): U1's extra cost, the same seeds with U1 centred on the sector against U0 everywhere.
int PlayerSectorLines(const std::vector<AirWarData>& loaded, const std::vector<GroundLine>& at_u0, const std::string& player, int days, int seeds) {
    const double mean_budget_ms = 15.0, worst_budget_ms = 45.0;
    int fails = 0;
    std::printf("\nU1 in the player's sector (F3.6): centred on %s, the same %d days x %d seeds; U1's extra a campaign day against U0 everywhere,\n"
                "budget %.0f ms (mean), %.0f ms (the worst day)\n\n",
                player.c_str(), days, seeds, mean_budget_ms, worst_budget_ms);
    std::printf("%-12s %8s %8s", "faction", "extra_ms", "worst_x");
    for (int k = 0; k < groundwar::GroundProfile::kParts; ++k) std::printf(" %10s", groundwar::GroundProfile::Name(k));
    std::printf(" %5s %6s %6s %6s %6s %6s %6s %8s %8s %8s\n", "regs", "levels", "rdecis", "asks", "finds", "hits", "fallbk", "route_ms", "march_h", "x1.3_h");
    for (std::size_t f = 0; f < loaded.size(); ++f) {
        const AirWarData& d = loaded[f];
        GroundLine u1;
        std::string axes_end;
        for (int s = 0; s < seeds; ++s) {
            if (!GroundAlone(d, static_cast<unsigned>(7 + s), days, u1, axes_end, player)) return -1;
        }
        const GroundLine& u0 = at_u0[f];
        const double n = u1.days > 0 ? static_cast<double>(u1.days) : 1.0;
        const double extra = (u1.mean_ms - u0.mean_ms) / n;
        double worst = 0.0;
        for (std::size_t k = 0; k < u1.day_ms.size() && k < u0.day_ms.size(); ++k) worst = std::max(worst, u1.day_ms[k] - u0.day_ms[k]);
        std::printf("%-12s %8.4f %8.4f", d.faction.id.c_str(), extra, worst);
        for (int k = 0; k < groundwar::GroundProfile::kParts; ++k) std::printf(" %10.5f", (u1.part_ms[k] - u0.part_ms[k]) / n);
        std::printf(" %5zu %6llu %6llu %6llu %6llu %6llu %6llu %8.3f %8.1f %8.1f\n", u1.regiments, static_cast<unsigned long long>(u1.level_changes),
                    static_cast<unsigned long long>(u1.regiment_decisions), static_cast<unsigned long long>(u1.asks),
                    static_cast<unsigned long long>(u1.finds), static_cast<unsigned long long>(u1.hits),
                    static_cast<unsigned long long>(u1.fallbacks), u1.route_ms, u1.march_h, u1.straight_h);
        std::printf("    at U0: %llu marches, %.1f h on the roads against %.1f h at x1.3 (%llu fall-backs), %llu route asks (%llu searched, %llu cached), %.3f ms\n",
                    static_cast<unsigned long long>(u0.marches), u0.march_h, u0.straight_h, static_cast<unsigned long long>(u0.fallbacks),
                    static_cast<unsigned long long>(u0.asks), static_cast<unsigned long long>(u0.finds), static_cast<unsigned long long>(u0.hits),
                    u0.route_ms);
        std::printf("    U1's axes at the end (seed 7): %s\n", axes_end.c_str());
        if (extra > mean_budget_ms || worst >= worst_budget_ms) {
            std::printf("  OVER BUDGET\n");
            ++fails;
        }
    }
    std::printf("  (ms by part: U1's minus U0's; regs: the regiments built; levels: ups and downs; rdecis: the regiments' commanders' decisions;\n"
                "   asks/finds/hits: the route service asked, searched, from the cache; fallbk: marches on F3.4's straight line x 1.3;\n"
                "   march_h: the marches' hours as timed, x1.3_h: the same marches at F3.4's rule; all over every seed)\n");
    return fails;
}

int GroundMain(const std::string& data, const std::string& theatre, int days, int seeds, int campaign_days, int campaign_seeds, bool gate,
               std::vector<std::string> factions, const std::string& player) {
    const double mean_budget_ms = 20.0, worst_budget_ms = 60.0, campaign_budget = 1.05;
    if (factions.empty()) factions = {"warsaw_pact", "nato", "neutral", "rebels"};
    int fails = 0;
    std::printf("The ground war at U0 (F3.3) on %s: alone %d days x %d seeds; budget %.0f ms a campaign day (mean), %.0f ms the worst day\n\n",
                theatre.c_str(), days, seeds, mean_budget_ms, worst_budget_ms);
    std::printf("%-12s %8s %8s", "faction", "mean_ms", "worst_ms");
    for (int k = 0; k < groundwar::GroundProfile::kParts; ++k) std::printf(" %10s", groundwar::GroundProfile::Name(k));
    std::printf("  %s\n", "axes at the end (the first seed)");
    std::vector<AirWarData> loaded;
    std::vector<GroundLine> at_u0;
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
        at_u0.push_back(g);
    }
    std::printf("  (ms a campaign day by part; \"picture\" is each side's picture's reports (F3.5), \"reports\" the journal's, in the campaign only;\n"
                "   \"levels\" U1's (F3.6), none without a player's sector)\n");
    if (!player.empty()) {
        const int f = PlayerSectorLines(loaded, at_u0, player, days, seeds);
        if (f < 0) return 1;
        fails += f;
    }
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
            GroundLine marches;  // F3.6: the levels and the marches in the campaign (the player's battery's sector as it stands)
            GroundAirStats ga;   // F3.7: the air war and the units, summed over the seeds
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
                    marches.level_changes += p.level_changes;
                    marches.regiment_decisions += p.regiment_decisions;
                    marches.marches += p.marches;
                    marches.fallbacks += p.route_fallbacks;
                    marches.march_h += p.march_h;
                    marches.straight_h += p.march_straight_h;
                    marches.asks += b.Ground()->RouteStats().asks;
                    marches.finds += b.Ground()->RouteStats().finds;
                    marches.hits += b.Ground()->RouteStats().hits;
                    const GroundAirStats& g = b.GroundAir();
                    ga.troop_strikes += g.troop_strikes;
                    ga.strength_lost += g.strength_lost;
                    ga.reinforced += g.reinforced;
                    ga.overrun += g.overrun;
                    ga.retaken += g.retaken;
                    ga.escapes += g.escapes;
                    ga.troop_moves += g.troop_moves;
                    ga.supply_reroutes += g.supply_reroutes;
                    ga.cas += g.cas;
                    ga.bai += g.bai;
                    ga.ai += g.ai;
                    ga.bai_fixed += g.bai_fixed;
                    ga.ai_fixed += g.ai_fixed;
                    for (int k = 0; k < GroundAirStats::kParts; ++k) ga.ms[k] += g.ms[k];
                    ground_ms += g.Ms();  // the air war's part of the ground war (F3.7) is the ground war's cost too
                }
            }
            const double ratio = cpu_off > 0.0 ? cpu_on / cpu_off : 0.0;
            const double share = cpu_off > 0.0 ? ground_ms / (1000.0 * cpu_off) : 0.0;
            std::printf("%-12s %9.2f %9.2f %7.3f %9.2f %9.3f %9.4f %6.3f%% %7zu %7zu\n", on.faction.id.c_str(), cpu_off, cpu_on, ratio, ground_ms,
                        reports_ms, worst_ms, 100.0 * share, packages_off, packages_on);
            std::printf("    F3.6: %llu level changes, %llu regiment decisions; %llu marches, %.1f h as timed against %.1f h at x1.3 (%llu on x1.3: no way on the roads),"
                        " %llu route asks (%llu searched, %llu cached)\n",
                        static_cast<unsigned long long>(marches.level_changes), static_cast<unsigned long long>(marches.regiment_decisions),
                        static_cast<unsigned long long>(marches.marches), marches.march_h, marches.straight_h,
                        static_cast<unsigned long long>(marches.fallbacks), static_cast<unsigned long long>(marches.asks),
                        static_cast<unsigned long long>(marches.finds), static_cast<unsigned long long>(marches.hits));
            {
                const double n = static_cast<double>(campaign_seeds), dn = static_cast<double>(campaign_seeds * campaign_days);
                std::printf("    F3.7 a campaign: %.1f strikes on troops (%.3f of establishments), %.1f reinforcements, %.1f overrun, %.1f retaken, %.1f "
                            "escapes, %.1f troop moves, %.1f supply lines routed again; labels cas %.1f bai %.1f ai %.1f (fixed depth bai %.1f ai %.1f)\n",
                            ga.troop_strikes / n, ga.strength_lost / n, ga.reinforced / n, ga.overrun / n, ga.retaken / n, ga.escapes / n,
                            ga.troop_moves / n, ga.supply_reroutes / n, ga.cas / n, ga.bai / n, ga.ai / n, ga.bai_fixed / n, ga.ai_fixed / n);
                std::printf("    F3.7 ms a campaign day:");
                for (int k = 0; k < GroundAirStats::kParts; ++k) std::printf(" %s %.5f", GroundAirStats::PartName(k), ga.ms[k] / dn);
                std::printf(" (in ground_ms)\n");
            }
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
            // A campaign of a few packages (the neutral's: 4 in 10 days) takes a few ms at mode 0, which std::clock cannot
            // measure for a share (0.00-0.01 s); there the ground war's own time a day is held to the absolute budget (F3.7).
            const double day_ms = ground_ms / static_cast<double>(campaign_seeds * campaign_days);
            if (cpu_off < 0.1 * campaign_seeds) {
                if (day_ms > mean_budget_ms) {
                    std::printf("  OVER BUDGET: the ground war takes %.3f ms a campaign day\n", day_ms);
                    ++fails;
                } else {
                    std::printf("    (a campaign under std::clock's resolution: %.4f ms a campaign day against %.0f ms, not the share)\n", day_ms,
                                mean_budget_ms);
                }
            } else if (share > campaign_budget - 1.0) {
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

// ------------------------------------------------------------------ our units' reports in the pipeline (F3.8)

// One campaign at ground.mode 1 and air_front.mode 1 with our side's pipeline pumped after every step. `feed_on` false
// turns the ground feed off (its posts named none): the same war and the same pipeline without our units' reports, for the
// cost of the reports and for what they change in the air defence staff's watch (F3.7's strikes on troops reach it only
// through them).
struct FeedStats {
    double pump_s = 0.0;       // CPU in the feed's Pump, summed
    double campaign_s = 0.0;   // CPU of the campaign's own steps
    int days = 0;
    std::size_t facts = 0;
    std::vector<command::AirFrontConclusion> conclusions;
    std::size_t signs = 0;
    // The reports (feed on): by the post that compiled them, and what reached whom.
    struct Origin {
        int sent = 0, to_army = 0;
        double delay_sum = 0.0;
    };
    std::map<std::string, Origin> origins;
    int reports = 0, to_army = 0, to_regiment = 0, regiment_sent = 0, to_battery = 0;
    double army_delay = 0.0, regiment_delay = 0.0, battery_delay = 0.0;
    int summaries = 0, summaries_to_sector = 0, intsums = 0, orders = 0;
    double summary_delay = 0.0;
    int air_reports = 0, air_signs = 0, strikes = 0;

    void Add(const FeedStats& y) {
        pump_s += y.pump_s;
        campaign_s += y.campaign_s;
        days += y.days;
        facts += y.facts;
        signs += y.signs;
        reports += y.reports;
        to_army += y.to_army;
        to_regiment += y.to_regiment;
        regiment_sent += y.regiment_sent;
        to_battery += y.to_battery;
        army_delay += y.army_delay;
        regiment_delay += y.regiment_delay;
        battery_delay += y.battery_delay;
        summaries += y.summaries;
        summaries_to_sector += y.summaries_to_sector;
        summary_delay += y.summary_delay;
        intsums += y.intsums;
        orders += y.orders;
        air_reports += y.air_reports;
        air_signs += y.air_signs;
        strikes += y.strikes;
    }
};

void FeedCampaign(const AirWarData& d, unsigned seed, int days, const command::ChainData& chain, bool feed_on, FeedStats& out) {
    AirCampaign c;
    c.Init(d, seed);
    command::InfoNet net;
    net.Init(chain, seed, d.theatre.start_hour);
    command::GiveTheatreSky(net, d.theatre);
    command::CampaignFeed feed;
    feed.Bind(&net);
    if (!feed_on) {
        command::GroundFeedRoles off;
        off.army_staff = "off";
        feed.UseGroundRoles(off);
    }
    const double t0 = CpuSeconds();
    double pump = 0.0;
    while (c.Now() < days * 24.0) {
        if (c.Advance(days * 24.0)) c.SkipPlayerMission();
        const double p0 = CpuSeconds();
        feed.Pump(c);
        pump += CpuSeconds() - p0;
    }
    out.pump_s += pump;
    out.campaign_s += CpuSeconds() - t0 - pump;
    out.days += days;
    out.facts += net.Facts().size();
    out.conclusions = feed.Watch().Conclusions();
    out.signs += feed.Signs().size();
    if (!feed_on || !feed.Ground().On()) return;
    const command::GroundFeed& gf = feed.Ground();
    const command::ChainData& cd = net.Data();
    const int army = cd.PositionIndex(gf.Roles().army_staff), reg = cd.PositionIndex(gf.Roles().regiment), bty = cd.PositionIndex(gf.Roles().battery);
    const int sector = cd.PositionIndex(gf.Roles().sector);
    for (const command::SituationReport& r : gf.Reports()) {
        ++out.reports;
        FeedStats::Origin& o = out.origins[r.post];
        ++o.sent;
        const command::Delivery* a = net.Known(army, r.fact);
        if (a != nullptr) {
            ++out.to_army;
            ++o.to_army;
            o.delay_sum += a->h - r.h;
            out.army_delay += a->h - r.h;
        }
        if (!gf.WithRegiment(r.formation)) continue;
        ++out.regiment_sent;
        const command::Delivery* g = net.Known(reg, r.fact);
        if (g != nullptr) {
            ++out.to_regiment;
            out.regiment_delay += g->h - r.h;
        }
        const command::Delivery* b = net.Known(bty, r.fact);
        if (b != nullptr) {
            ++out.to_battery;
            out.battery_delay += b->h - r.h;
        }
    }
    for (const command::ArmySummary& s : gf.Summaries()) {
        ++out.summaries;
        const command::Delivery* a = net.Known(sector, s.fact);
        if (a == nullptr) continue;
        ++out.summaries_to_sector;
        out.summary_delay += a->h - s.h;
    }
    out.orders += static_cast<int>(gf.Orders().size());
    out.intsums += gf.IntelSummaries();
    out.air_reports += gf.AirReports();
    out.air_signs += gf.AirSigns();
    out.strikes += c.Ground() != nullptr ? static_cast<int>(c.Ground()->SitAirs().size()) : 0;
}

// --feed: our units' reports in the pipeline on the nation, with the ground war and the air front on.
int FeedMain(const std::string& data, const std::string& theatre, int days, int seeds, bool gate, const std::vector<std::string>& factions) {
    command::ChainData chain;
    std::string e;
    if (!command::LoadChainData(data, "command/chain_pannonia.json", chain, &e)) {
        std::printf("%s\n", e.c_str());
        return 1;
    }
    const std::vector<std::string> list = factions.empty() ? std::vector<std::string>{"warsaw_pact", "nato", "neutral", "rebels"} : factions;
    const double budget_ms = 20.0;
    int fails = 0;
    std::printf("Our units' reports in the pipeline (F3.8) on %s, ground.mode 1, air_front.mode %g, %d days x %d seeds; budget %.0f ms a campaign day\n\n",
                theatre.c_str(), kFrontsAirFrontMode, days, seeds, budget_ms);
    for (const std::string& faction : list) {
        AirWarData d;
        if (!LoadAirWarData(data, "air_war/" + theatre, "air_war/faction_" + faction + ".json", d, &e)) {
            std::printf("%s: %s\n", faction.c_str(), e.c_str());
            return 1;
        }
        d.tuning.values["ground.mode"] = 1.0;
        d.tuning.values["air_front.mode"] = kFrontsAirFrontMode;
        FeedStats on, off;
        int same = 0;
        for (int s = 0; s < seeds; ++s) {
            const unsigned seed = static_cast<unsigned>(7 + s);
            FeedStats a, b;
            FeedCampaign(d, seed, days, chain, true, a);
            FeedCampaign(d, seed, days, chain, false, b);
            bool eq = a.conclusions.size() == b.conclusions.size();
            for (std::size_t i = 0; eq && i < a.conclusions.size(); ++i) {
                eq = a.conclusions[i].sector == b.conclusions[i].sector && a.conclusions[i].depth_m == b.conclusions[i].depth_m && a.conclusions[i].h == b.conclusions[i].h;
            }
            same += eq ? 1 : 0;
            if (s == 0) {
                for (const auto& kv : a.origins) {
                    std::printf("  %-12s %-20s sent %3d, reached the army %3d, mean delay %.2f h\n", faction.c_str(), kv.first.c_str(), kv.second.sent,
                                kv.second.to_army, kv.second.to_army > 0 ? kv.second.delay_sum / kv.second.to_army : 0.0);
                }
            }
            on.Add(a);
            off.Add(b);
        }
        auto mean = [](double sum, int n) { return n > 0 ? sum / n : 0.0; };
        std::printf("%-12s %d reports, %d to the army (mean %.2f h); the regiment's staff %d of %d of its division's (%.2f h), the battery commander %d (%.2f h); "
                    "%d summaries, %d to the sector (%.2f h); %d orders, %d intelligence summaries\n",
                    faction.c_str(), on.reports, on.to_army, mean(on.army_delay, on.to_army), on.to_regiment, on.regiment_sent,
                    mean(on.regiment_delay, on.to_regiment), on.to_battery, mean(on.battery_delay, on.to_battery), on.summaries, on.summaries_to_sector,
                    mean(on.summary_delay, on.summaries_to_sector), on.orders, on.intsums);
        std::printf("  air attacks on our troops: %d counted, %d told the air defence staff's watch, %d signs from them (signs in all: %zu with the reports, %zu "
                    "without); its conclusions the same in %d of %d seeds\n",
                    on.strikes, on.air_reports, on.air_signs, on.signs, off.signs, same, seeds);
        const double day_on = on.days > 0 ? 1000.0 * on.pump_s / on.days : 0.0, day_off = off.days > 0 ? 1000.0 * off.pump_s / off.days : 0.0;
        const double extra = day_on - day_off;
        const bool over = extra > budget_ms;
        std::printf("  the pipeline's pump a campaign day: %.3f ms with the reports, %.3f ms without: %.3f ms for them (%zu facts against %zu); campaign %.2f s%s\n\n",
                    day_on, day_off, extra, on.facts, off.facts, on.campaign_s, over ? "  OVER BUDGET" : "");
        fails += over ? 1 : 0;
    }
    if (gate && fails > 0) {
        std::printf("%d over budget\n", fails);
        return 2;
    }
    return 0;
}

// ------------------------------------------------------------------ our air defence commanders' sector priorities (F4.1)

struct AdStats {
    double pump_s = 0.0;
    double total_s = 0.0;  // the campaign and the pump together (the order source pumps inside the steps, F4.2)
    int days = 0;
    // F4.2: our units' moves on orders.
    std::map<std::string, int> moves_by_class;  // begun
    std::map<std::string, double> march_h_by_class;
    int ordered = 0, heard = 0, withdrawals = 0, covers = 0, ready = 0, lost_on_move = 0;
    int units = 0, overrun = 0, escaped = 0;
    double offline_h = 0.0;
    double packages = 0.0, sorties = 0.0, lost = 0.0;
    int reviews = 0, changes = 0, unheld = 0, journal_differs = 0;
    std::map<std::string, std::map<std::string, int>> by_sector;  // commander -> "sector to" -> changes
    int to_target = 0;
    double target_delay = 0.0, prompt_delay = 0.0, origin_delay = 0.0;
    // F4.3: our fighters by sector.
    int fighter_changes = 0, fighter_orders = 0;
    double order_to_change = 0.0, heard_to_change = 0.0, decided_to_change = 0.0;
    std::vector<double> cover_h;  // by sector: the cover times the hours it held
    std::vector<double> cover_end;
    double cover_hours = 0.0;
    double worst_sum_error = 0.0;
    int air_fights = 0, fighter_kills = 0, ours_lost = 0;
};

bool g_no_fighters = false;

void AdCampaign(const AirWarData& d, unsigned seed, int days, const command::ChainData& chain, bool log, AdStats& out, std::uint64_t* journal) {
    AirCampaign c;
    c.Init(d, seed);
    command::InfoNet net;
    net.Init(chain, seed, d.theatre.start_hour);
    command::GiveTheatreSky(net, d.theatre);
    command::CampaignFeed feed;
    feed.Bind(&net);
    if (g_no_fighters) {
        campaign::AdStrategyData sd;
        std::string se;
        if (campaign::LoadAdStrategyData(d.data_dir, "campaign/ad_strategy.json", sd, &se)) {
            sd.fighters.present = false;
            feed.UseAdStrategy(sd);
        }
    }
    // F4.3: the abstract air fights and their kills and losses (the truth, for the bench).
    c.ObserveOutcomes([&](const Package&, const PackageOutcome& o) {
        out.air_fights += o.air_fights;
        for (const LossRecord& l : o.losses) out.fighter_kills += l.fighter ? 1 : 0;
        out.ours_lost += static_cast<int>(o.our_fighters_lost.size());
    });
    double pump = 0.0;
    // F4.2: the units hear their orders inside the steps (never asked at mode 0); its pumps are the pipeline's too.
    const UnitOrderSource source = feed.OrderSource();
    c.SetOrderSource([&](const AirCampaign& cc, std::vector<UnitOrder>& heard, double* next_h) {
        const double p0 = CpuSeconds();
        source(cc, heard, next_h);
        pump += CpuSeconds() - p0;
    });
    const double t0 = CpuSeconds();
    while (c.Now() < days * 24.0) {
        if (c.Advance(days * 24.0)) c.SkipPlayerMission();
        const double p0 = CpuSeconds();
        feed.Pump(c);
        pump += CpuSeconds() - p0;
    }
    out.total_s += CpuSeconds() - t0;
    c.SetOrderSource(nullptr);
    c.ObserveOutcomes(nullptr);
    // F4.3: the fighters' duty by sector over time, and the delay from each order to the shares changing.
    {
        const std::size_t n = c.FrontLine().SectorCount();
        const double front = c.FrontFighters();
        if (out.cover_h.size() < n) out.cover_h.resize(n, 0.0);
        if (out.cover_end.size() < n) out.cover_end.resize(n, 0.0);
        std::vector<double> cur(n, front);
        double from_h = 0.0;
        const double end_h = c.Now();
        const std::vector<AirCampaign::FighterCoverChange>& ch = c.FighterCoverChanges();
        for (std::size_t i = 0; i <= ch.size(); ++i) {
            const double t = i < ch.size() ? std::max(0.0, std::min(end_h, ch[i].h)) : end_h;
            for (std::size_t s = 0; s < n; ++s) out.cover_h[s] += cur[s] * std::max(0.0, t - from_h);
            from_h = std::max(from_h, t);
            if (i == ch.size()) break;
            cur = ch[i].cover;
            double sum = 0.0;
            for (double x : cur) sum += x;
            out.worst_sum_error = std::max(out.worst_sum_error, std::fabs(sum - front * static_cast<double>(n)));
        }
        out.cover_hours += end_h;
        for (std::size_t s = 0; s < n; ++s) out.cover_end[s] += cur[s];
        out.fighter_changes += static_cast<int>(ch.size());
        const campaign::AdStrategy& a = feed.Ad();
        for (const AirCampaign::FighterCoverChange& x : ch) {
            const campaign::AdDecision* dec = nullptr;
            for (const campaign::AdDecision& y : a.Decisions()) dec = y.key == x.key ? &y : dec;
            if (log || g_ground_log) {
                std::string cov;
                for (std::size_t s = 0; s < x.cover.size(); ++s) {
                    char buf[96];
                    std::snprintf(buf, sizeof(buf), "%s%s %.2f", s == 0 ? "" : ", ", c.FrontLine().Sector(s).id.c_str(), x.cover[s]);
                    cov += buf;
                }
                std::printf("    fighters %s: %s (the order %s, at the regiment's post %s): %s\n", command::FormatClock(x.h, d.theatre.start_hour).c_str(),
                            x.key.c_str(), dec != nullptr ? command::FormatClock(net.Facts()[static_cast<std::size_t>(dec->fact)].h, d.theatre.start_hour).c_str() : "the pre-war plan",
                            command::FormatClock(x.heard_h, d.theatre.start_hour).c_str(), cov.c_str());
            }
            if (dec == nullptr) continue;
            ++out.fighter_orders;
            out.order_to_change += x.h - net.Facts()[static_cast<std::size_t>(dec->fact)].h;
            out.heard_to_change += x.h - x.heard_h;
            out.decided_to_change += x.h - dec->h;
        }
    }
    out.pump_s += pump;
    out.days += days;
    if (journal != nullptr) *journal = JournalHash(c);
    for (const Package& p : c.Packages()) {
        if (!p.resolved) continue;
        out.packages += 1.0;
        for (const Flight& f : p.flights) {
            const AircraftType* a = d.faction.Aircraft(f.aircraft);
            if (a != nullptr && (a->drone || a->missile)) continue;
            out.sorties += f.count;
            out.lost += f.lost;
        }
    }
    if (c.UnitMovesOn()) {
        for (std::size_t i = 0; i < d.theatre.objects.size(); ++i) out.units += c.MoveClassOf(i) >= 0 ? 1 : 0;
        out.overrun += c.UnitsOverrun();
        for (const UnitMove& m : c.UnitMoves()) {
            const std::string& cls = c.MoveRules().classes[static_cast<std::size_t>(m.cls)].id;
            out.moves_by_class[cls] += 1;
            out.offline_h += m.OfflineHours(c.Now());
            if (m.stage >= 2) out.march_h_by_class[cls] += m.MarchHours();
            out.ready += m.stage == 3 ? 1 : 0;
            out.lost_on_move += m.stage < 0 ? 1 : 0;
            out.escaped += m.escaped ? 1 : 0;
            if (log) {
                std::printf("    move %-40s %-16s %s heard %s, on the road %s, ready %s: %.0f km%s (%.0f km straight), stage %d%s\n", m.key.c_str(),
                            d.theatre.objects[static_cast<std::size_t>(m.object)].id.c_str(), cls.c_str(), command::FormatClock(m.heard_h, d.theatre.start_hour).c_str(),
                            command::FormatClock(m.depart_h, d.theatre.start_hour).c_str(), command::FormatClock(m.ready_h, d.theatre.start_hour).c_str(),
                            m.road_km + m.cross_km, m.on_roads ? " by road" : " straight", Dist2d(m.from, m.to) / 1000.0, m.stage, m.escaped ? ", got away" : "");
            }
        }
        for (const campaign::AdStrategy::MoveOrder& o : feed.Ad().MoveOrders()) {
            ++out.ordered;
            out.heard += o.heard_h >= 0.0 ? 1 : 0;
            out.withdrawals += o.withdraw ? 1 : 0;
            out.covers += o.withdraw ? 0 : 1;
        }
    }
    const campaign::AdStrategy& ad = feed.Ad();
    if (!ad.On()) {
        if (!ad.Error().empty()) std::printf("  %s\n", ad.Error().c_str());
        return;
    }
    const double start = d.theatre.start_hour;
    const command::ChainData& cd = net.Data();
    const LineOfContact& line = c.FrontLine();
    for (std::size_t k = 0; k < ad.Commanders(); ++k) out.reviews += ad.Reviews(static_cast<int>(k));
    if (log) {
        for (std::size_t k = 0; k < ad.Commanders(); ++k) {
            std::string plan;
            for (std::size_t s = 0; s < line.SectorCount(); ++s) {
                plan += std::string(s == 0 ? "" : ", ") + line.Sector(s).id + " " + campaign::SectorPriorityName(ad.PreWar(static_cast<int>(k))[s]);
            }
            std::printf("    %s (every %.1f h): the pre-war plan %s\n", ad.Def(static_cast<int>(k)).id.c_str(), ad.CycleH(static_cast<int>(k)), plan.c_str());
        }
    }
    for (const campaign::AdDecision& dec : ad.Decisions()) {
        ++out.changes;
        const campaign::AdCommanderDef& def = ad.Def(dec.commander);
        const int post = cd.PositionIndex(def.post);
        out.by_sector[def.id][line.Sector(static_cast<std::size_t>(dec.sector)).id + " " + campaign::SectorPriorityName(dec.to)] += 1;
        // Every fact it rested on had reached his post by the decision's hour.
        double newest = -1.0, newest_origin = -1.0;
        for (std::size_t i = 0; i < dec.basis.size(); ++i) {
            const command::Delivery* got = net.Known(post, dec.basis[i]);
            if (got == nullptr || got->h > dec.h + 1e-9 || std::fabs(got->h - dec.basis_h[i]) > 1e-9) ++out.unheld;
            if (dec.basis_h[i] > newest) {
                newest = dec.basis_h[i];
                newest_origin = net.Facts()[static_cast<std::size_t>(dec.basis[i])].h;
            }
        }
        const std::string target = def.role == campaign::AdRole::Troops ? "rgt_commander" : "fighter_rgt_duty";
        const command::Delivery* at = net.Known(cd.PositionIndex(target), dec.fact);
        if (at != nullptr && newest >= 0.0) {
            ++out.to_target;
            out.target_delay += at->h - newest;
            out.prompt_delay += dec.h - newest;
            out.origin_delay += at->h - newest_origin;
        }
        if (log) {
            std::printf("    %s  %-17s %-10s %-8s -> %-8s (on %zu facts, the newest at his post %s; the order at %s %s): %s\n",
                        command::FormatClock(dec.h, start).c_str(), def.id.c_str(), line.Sector(static_cast<std::size_t>(dec.sector)).id.c_str(),
                        campaign::SectorPriorityName(dec.from), campaign::SectorPriorityName(dec.to), dec.basis.size(),
                        newest >= 0.0 ? command::FormatClock(newest, start).c_str() : "-", target.c_str(),
                        at != nullptr ? command::FormatClock(at->h, start).c_str() : "not yet", dec.why.c_str());
        }
    }
    if (log) {
        for (std::size_t k = 0; k < ad.Commanders(); ++k) {
            std::string now;
            for (std::size_t s = 0; s < line.SectorCount(); ++s) {
                now += std::string(s == 0 ? "" : ", ") + line.Sector(s).id + " " + campaign::SectorPriorityName(ad.Priorities(static_cast<int>(k))[s]);
            }
            std::printf("    %s at the end (%d reviews): %s\n", ad.Def(static_cast<int>(k)).id.c_str(), ad.Reviews(static_cast<int>(k)), now.c_str());
            const campaign::AdPicture held = ad.Held(static_cast<int>(k), c.Now());
            for (std::size_t s = 0; s < held.sectors.size(); ++s) {
                const campaign::AdSectorPicture& sp = held.sectors[s];
                std::printf("      %-16s thrust %5.1f km at %4.1f km a day, enemy %4.2f%s, objects in reach %d, fire units %d, air front %.0f km\n",
                            line.Sector(s).id.c_str(), sp.thrust_m / 1000.0, sp.pace_km_day, sp.enemy, sp.counterattack ? ", our counterattack" : "",
                            sp.objects_in_reach, sp.fire_units, sp.air_front_m / 1000.0);
            }
        }
    }
}

// --ad: our air defence commanders on the nation, with the ground war and the air front on.
int AdMain(const std::string& data, const std::string& theatre, int days, int seeds, bool gate, const std::vector<std::string>& factions) {
    command::ChainData chain;
    std::string e;
    if (!command::LoadChainData(data, "command/chain_pannonia.json", chain, &e)) {
        std::printf("%s\n", e.c_str());
        return 1;
    }
    const std::vector<std::string> list = factions.empty() ? std::vector<std::string>{"warsaw_pact", "nato"} : factions;
    const double budget_ms = 5.0;
    int fails = 0;
    std::printf("Our air defence commanders' sector priorities (F4.1) on %s, ground.mode 1, air_front.mode %g, ad_strategy.mode 1, %d days x %d seeds; "
                "budget %.0f ms a campaign day\n\n",
                theatre.c_str(), kFrontsAirFrontMode, days, seeds, budget_ms);
    for (const std::string& faction : list) {
        AirWarData d;
        if (!LoadAirWarData(data, "air_war/" + theatre, "air_war/faction_" + faction + ".json", d, &e)) {
            std::printf("%s: %s\n", faction.c_str(), e.c_str());
            return 1;
        }
        d.tuning.values["ground.mode"] = 1.0;
        d.tuning.values["air_front.mode"] = kFrontsAirFrontMode;
        AirWarData d0 = d;
        d.tuning.values["ad_strategy.mode"] = 1.0;
        d0.tuning.values["ad_strategy.mode"] = 0.0;
        AdStats on, off;
        std::printf("%s\n", faction.c_str());
        for (int s = 0; s < seeds; ++s) {
            const unsigned seed = static_cast<unsigned>(7 + s);
            std::uint64_t j1 = 0, j0 = 0;
            const bool log = g_ground_log || s == 0;
            if (log) std::printf("  seed %u:\n", seed);
            AdCampaign(d, seed, days, chain, log, on, &j1);
            AdCampaign(d0, seed, days, chain, false, off, &j0);
            on.journal_differs += j1 != j0 ? 1 : 0;
        }
        // F4.3: our fighters by sector.
        {
            const double n = seeds > 0 ? static_cast<double>(seeds) : 1.0;
            std::string mean_row, end_row;
            LineOfContact probe;
            if (const FrontDef* pf = d.theatre.FrontFor(d.faction.front)) probe.Build(*pf, 5000.0);
            const LineOfContact* line = &probe;
            for (std::size_t s = 0; s < on.cover_h.size(); ++s) {
                char buf[96];
                const std::string id = line != nullptr && s < line->SectorCount() ? line->Sector(s).id : std::to_string(s);
                std::snprintf(buf, sizeof(buf), "%s%s %.3f", s == 0 ? "" : ", ", id.c_str(), on.cover_hours > 0.0 ? on.cover_h[s] / on.cover_hours : 0.0);
                mean_row += buf;
                std::snprintf(buf, sizeof(buf), "%s%s %.3f", s == 0 ? "" : ", ", id.c_str(), on.cover_end[s] / n);
                end_row += buf;
            }
            const FrontDef* fr = d.theatre.FrontFor(d.faction.front);
            std::printf("  fighters by sector (F4.3, the front's number %.2f): %.1f changes a campaign, %.1f on an order; from the order to the shares %.2f h "
                        "(from his decision %.2f h, from the regiment's post having it %.2f h); the sum over the sectors off the front's by %.2g at worst\n",
                        fr != nullptr ? fr->friendly_fighters : 0.0, on.fighter_changes / n, on.fighter_orders / n,
                        on.fighter_orders > 0 ? on.order_to_change / on.fighter_orders : 0.0, on.fighter_orders > 0 ? on.decided_to_change / on.fighter_orders : 0.0,
                        on.fighter_orders > 0 ? on.heard_to_change / on.fighter_orders : 0.0, on.worst_sum_error);
            std::printf("    the cover over the campaign: %s\n    at its end: %s\n", mean_row.empty() ? "-" : mean_row.c_str(), end_row.empty() ? "-" : end_row.c_str());
            std::printf("  air fights %.1f a campaign against %.1f at mode 0; the enemy's aircraft our fighters shot down %.1f against %.1f; ours lost %.1f against %.1f\n",
                        on.air_fights / n, off.air_fights / n, on.fighter_kills / n, off.fighter_kills / n, on.ours_lost / n, off.ours_lost / n);
        }
        // F4.2: the units' moves on the orders.
        {
            const double n = seeds > 0 ? static_cast<double>(seeds) : 1.0;
            std::string by;
            for (const auto& kv : on.moves_by_class) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%s%s %.1f (march %.1f h)", by.empty() ? "" : ", ", kv.first.c_str(), kv.second / n,
                              kv.second > 0 ? on.march_h_by_class[kv.first] / kv.second : 0.0);
                by += buf;
            }
            std::printf("  units' moves a campaign (F4.2): %s; orders %.1f (%.1f withdrawals, %.1f to cover), heard %.1f, ready %.1f, lost on a move %.1f\n",
                        by.empty() ? "none" : by.c_str(), on.ordered / n, on.withdrawals / n, on.covers / n, on.heard / n, on.ready / n, on.lost_on_move / n);
            std::printf("  units overrun (lost with the ground) %.1f a campaign, got away on a move %.1f; off the air %.1f h per unit that moves on orders (%d units)\n",
                        on.overrun / n, on.escaped / n, on.units > 0 ? on.offline_h / on.units : 0.0, seeds > 0 ? on.units / seeds : 0);
            std::printf("  the air war, moves on against ad_strategy.mode 0 (F4.1's air war): %.1f against %.1f packages a campaign, loss rate %.2f%% against %.2f%% a sortie\n",
                        on.packages / n, off.packages / n, on.sorties > 0 ? 100.0 * on.lost / on.sorties : 0.0, off.sorties > 0 ? 100.0 * off.lost / off.sorties : 0.0);
            std::printf("  a campaign day, the campaign and the pump together: %.3f ms with the moves, %.3f ms at mode 0\n", on.days > 0 ? 1000.0 * on.total_s / on.days : 0.0,
                        off.days > 0 ? 1000.0 * off.total_s / off.days : 0.0);
        }
        auto mean = [](double sum, int n) { return n > 0 ? sum / n : 0.0; };
        std::printf("  %d reviews, %d changes (%.1f a campaign); every fact a change rested on held by the post by its hour: %s (%d not)\n", on.reviews,
                    on.changes, seeds > 0 ? static_cast<double>(on.changes) / seeds : 0.0, on.unheld == 0 ? "yes" : "NO", on.unheld);
        for (const auto& kv : on.by_sector) {
            std::string row;
            for (const auto& x : kv.second) row += "  " + x.first + " x" + std::to_string(x.second);
            std::printf("  %-17s%s\n", kv.first.c_str(), row.c_str());
        }
        std::printf("  from the newest fact at his post to the order at the regiment (or the fighters): %.2f h (of it to his decision hour %.2f h); from that "
                    "fact's own hour %.2f h; %d orders arrived\n",
                    mean(on.target_delay, on.to_target), mean(on.prompt_delay, on.to_target), mean(on.origin_delay, on.to_target), on.to_target);
        std::printf("  the air war with the commanders on and off: %s\n",
                    on.journal_differs == 0 ? "the same journal in every seed" : "differs (the units move on their orders, F4.2)");
        const double day_on = on.days > 0 ? 1000.0 * on.pump_s / on.days : 0.0, day_off = off.days > 0 ? 1000.0 * off.pump_s / off.days : 0.0;
        const double extra = day_on - day_off;
        const bool over = extra > budget_ms;
        std::printf("  the pipeline's pump a campaign day: %.3f ms with the commanders, %.3f ms without: %.3f ms for them%s\n\n", day_on, day_off, extra,
                    over ? "  OVER BUDGET" : "");
        fails += (over ? 1 : 0) + (on.unheld > 0 ? 1 : 0);
    }
    if (gate && fails > 0) {
        std::printf("%d over budget or failed\n", fails);
        return 2;
    }
    return 0;
}

// ------------------------------------------------------------------ the fronts against history (F6.1)

// A measure's samples summed over a faction's seeds.
struct HistMean {
    double sum = 0.0;
    double n = 0.0;
    void Add(double v, double w = 1.0) {
        sum += v * w;
        n += w;
    }
    double Get() const { return n > 0.0 ? sum / n : 0.0; }
};

// What --history measures, summed over a faction's seeds (the truth: the ground war's tally, the campaign's state).
struct HistStats {
    int runs = 0;
    int days = 0;
    // [0] ours, [1] the enemy's.
    HistMean contact_day[2];          // a formation-day in contact (12 h or more fought): battle losses over the day's first strength
    int contact_over[2] = {0, 0};     // of them, over 6.6 % (the band's top)
    HistMean army_day[2];             // the side's battle losses a day of the offensive, of its strength at the war's start
    double att_lost = 0.0, def_lost = 0.0;  // the axes' fighting: the attacker's and the defender's, T-55 equivalents
    HistMean exchange;                // per axis that fought: the attacker's losses over the defender's
    HistMean attack_day_km;           // the enemy's axis on a day it attacked 12 h or more: its advance that day
    double fastest_day_km = 0.0;
    double below_km = 0.0;            // advanced in hours fought under the stall ratio
    double class_h[groundwar::kAdvanceClassCount] = {};
    double class_km[groundwar::kAdvanceClassCount] = {};
    HistMean zone_h;                  // the security zone: an axis's start to its reaching the main defensive zone
    int axes = 0, axes_won = 0;       // the enemy's (not the rebels' raids) that started; of them at or near their objective
    int ours_engaged = 0, ours_out = 0;  // our formations a day or more in the fight; of them withdrawn or dispersed
    HistMean ours_frac;                   // their strength over establishment at the end
    HistMean counter_ratio;               // a penetrated axis's best truth ratio of our counterattack's candidates over its attackers
    HistMean tip_ratio;                   // F7.1: the same over the enemy at its tip (TipPowerTruth), any hour
    HistMean tip_spent_ratio;             // and in the hours our picture had its momentum spent (none: not counted)
    double spent_h = 0.0;                 // penetrated axes' hours with the momentum spent (our picture), no counterattack on them
    int counters_in = 0;                  // counterattacks that went in (started)
    HistMean counter_won_km;              // a counterattack's line won back: its depth at the start less at its end
    HistMean counter_h;                   // its hours from start to end
    HistMean counter_ordered_ratio;       // the ratio its commander ordered it on (his picture)
    HistMean reserve_share;               // our army reserve's (the deployment's reserve) power over our army's at the war's start
    HistMean reserve_depth_km;            // how far behind the line it stands then
    // F8.1: the national command's reserve (none without ground_defence.json's "front_reserve"): its power over our army's
    // at the start, how far behind the line its home is, the front's counterstrikes ordered, their ratio (his picture), and
    // the best ratio he weighed at a penetration with its momentum spent (his force in his reach over the tip).
    bool front_reserve = false;
    HistMean front_share, front_depth_km, front_ordered_ratio, front_best_ratio;
    int front_orders = 0;
    HistMean low_cover, low_mobile;       // a contested sector's lowest believed cover and highest mobile destroyed share, the war's
    double gate_h[4] = {0.0, 0.0, 0.0, 0.0};  // contested sector-hours: cover gate held; and mobile; and reward; and losses under the line
    HistMean gate_reward;                 // the reward in the sector-hours both gates held
    double ours_lowest = 1.0;             // the lowest
    // F7.3, why none withdraws: each of our formations a day in the fight, its lowest strength over its establishment in the
    // war (hourly, as its own count has it), its losses by every cause over its base establishment, and what came back to it
    // (repairs and columns: the end's fraction less one less its losses).
    HistMean ours_low_frac, ours_gross, ours_given;
    double ours_low_any = 1.0;
    int ours_under_degraded = 0;          // of them, under the degraded line (2/3) at some hour
    int ours_under_hq = 0;                // and under the doctrine's line with command and artillery hit (3/4; no rule reads it)
    double hq_line = 0.0;
    double ask_line = 0.0;                // our doctrine's ineffective line, where a unit asks to withdraw (a flexible man higher)
    double degraded_line = 0.0;
    int withdrawn = 0, dispersed = 0, reliefs = 0, refused = 0, asked = 0;
    int penetrations = 0, counters = 0, wars_penetrated = 0, wars_countered = 0;
    double ours_air = 0.0, ours_battle = 0.0;
    int strikes = 0;
    double strike_share = 0.0;        // summed: each strike's loss as a share of its formation's establishment
    // The air front.
    int advances = 0, pullbacks = 0, crisis_steps = 0;
    HistMean latency_h;               // the gates first seen holding to the step forward
    HistMean pullback_h;              // the loss line first crossed to the step back
    double deepest_km = 0.0;
    int sectors_moved = 0;
    // F7.2: sector-hours with an axis of the enemy's own advancing there in its picture (AirFrontAdvancing); of them
    // contested, the cover gate held, and the mobile too (both risk gates); steps forward made in such a sector; at the
    // war's end, each sector an axis pushed in: the axis's depth (truth) and the air front's beyond the line of contact.
    double axis_h = 0.0, axis_contested_h = 0.0, axis_cover_h = 0.0, axis_mobile_h = 0.0;
    int axis_steps = 0;
    HistMean end_axis_km, end_front_km;
    int end_axis_sectors = 0, end_axis_stepped = 0;
    // Our air defence.
    HistMean ad_lost;                 // radar SAM fire units wrecked or overrun at the end, of all
    int ad_ever = 0, ad_back = 0;     // wrecked at some day's end; of them working at the end
    HistMean s75_h;                   // an S-75 class move: its order heard to ready
    HistMean s75_km;
    int most_moves_a_day = 0;
    double sorties = 0.0, lost = 0.0, packages = 0.0;
};

int AdvanceClassOf(const groundwar::GroundTables& t, double r) {
    int k = 0;
    for (std::size_t c = 0; c < groundwar::kAdvanceClassCount; ++c) {
        if (r >= t.class_from_ratio[c]) k = static_cast<int>(c);
    }
    return k;
}

bool RadarSam(const TheatreObject& o) { return !o.decoy && o.kind == ObjectKind::SamSite && o.defence.present && o.defence.radar; }

// One campaign with every front mode on and our pipeline pumped after every step, sampled every hour.
void HistoryCampaign(const AirWarData& d, unsigned seed, int days, const command::ChainData& chain, HistStats& out) {
    AirCampaign c;
    c.Init(d, seed);
    command::InfoNet net;
    net.Init(chain, seed, d.theatre.start_hour);
    command::GiveTheatreSky(net, d.theatre);
    command::CampaignFeed feed;
    feed.Bind(&net);
    c.SetOrderSource(feed.OrderSource());
    const groundwar::GroundWar* g = c.Ground();
    const std::size_t nf = g != nullptr ? g->Formations().size() : 0, na = g != nullptr ? g->Axes().size() : 0;
    std::vector<double> day_strength(nf, 0.0), day_battle(nf, 0.0), day_fought(nf, 0.0);
    double start_strength[2] = {0.0, 0.0};
    std::vector<double> axis_depth(na, 0.0), axis_hours(na, 0.0), hour_depth(na, 0.0), best_counter(na, -1.0);
    std::vector<double> best_tip(na, -1.0), best_tip_spent(na, -1.0), counter_from_m(na, -1.0), counter_from_h(na, 0.0);
    std::vector<char> held_ground(na, 0);  // F7.1: an axis halted holding our ground (more than 1 km in), any hour
    std::vector<double> low_frac(nf, 2.0);  // F7.3: our formations' lowest fraction, hourly
    if (g != nullptr) {
        // Our army reserve (F7.1): the deployment's reserve role, its share of our army's power and its depth at the war's start.
        double all = 0.0, reserve = 0.0, front = 0.0;
        for (std::size_t i = 0; i < nf; ++i) {
            const groundwar::Formation& f = g->Formations()[i];
            if (f.unit.side != groundwar::Side::Ours) continue;
            const double p = groundwar::UnitPower(g->Data().tables, f.unit.strength, f.unit.supply, f.unit.cohesion);
            const FrontPlace pl = c.FrontLine().Place(f.pos);
            if (f.front) {  // F8.1: the national command's, not the army's
                front += p;
                out.front_depth_km.Add(pl.side * pl.distance_m / 1000.0);
                continue;
            }
            all += p;
            if (f.role != groundwar::DeployRole::Reserve) continue;
            reserve += p;
            out.reserve_depth_km.Add(pl.side * pl.distance_m / 1000.0);
        }
        if (all > 0.0) out.reserve_share.Add(reserve / all);
        if (g->FrontReserve() >= 0 && all > 0.0) {
            out.front_reserve = true;
            out.front_share.Add(front / all);
        }
    }
    double offensive_h = -1.0;
    for (std::size_t i = 0; i < nf; ++i) {
        const groundwar::Formation& f = g->Formations()[i];
        day_strength[i] = f.unit.strength;
        if (f.front) continue;  // F8.1: the army's day is the army's, not the national command's reserve
        start_strength[f.unit.side == groundwar::Side::Ours ? 0 : 1] += f.unit.strength;
    }
    const std::size_t ns = c.FrontLine().SectorCount();
    std::vector<double> pending(ns, SectorFront::kNever), over_since(ns, -1.0), low_cover(ns, 2.0), high_mobile(ns, -1.0);
    std::vector<int> adv0(ns, 0), back0(ns, 0);
    const std::vector<TheatreObject>& objs = d.theatre.objects;
    std::vector<char> ever(objs.size(), 0);
    const AirFrontDef& afd = d.faction.air_front;
    const double min_sorties = d.tuning.Get("air_front.min_sorties");
    for (int h = 1; h <= days * 24; ++h) {
        while (c.Now() < h) {
            if (c.Advance(static_cast<double>(h))) c.SkipPlayerMission();
            feed.Pump(c);
        }
        // The air front: when the gates were first seen holding and the loss line first crossed, by sector.
        if (c.AirFront().sectors.size() == ns) {
            const int day = SectorLossWindow::DayOf(c.Now());
            for (std::size_t s = 0; s < ns; ++s) {
                const SectorFront& f = c.AirFront().sectors[s];
                // The step's hour as the staff decided it (the cycle's), not the hour sampled after it (F7.2: a cycle runs as
                // the campaign is advanced past its hour, so the sample came an hour late).
                if (f.advances > adv0[s] && pending[s] > SectorFront::kNever) out.latency_h.Add(f.depth_since_h - pending[s]);
                const bool axis_here = s < c.AirFrontAdvancing().size() && c.AirFrontAdvancing()[s] > 0;
                if (axis_here) {
                    out.axis_h += 1.0;
                    if (f.advances > adv0[s]) out.axis_steps += f.advances - adv0[s];
                    if (f.contested && s < c.SectorCovers().size()) {
                        out.axis_contested_h += 1.0;
                        const SectorCover& sc = c.SectorCovers()[s];
                        if (sc.cover <= afd.cover_at_most) {
                            out.axis_cover_h += 1.0;
                            if (sc.mobile_destroyed >= afd.mobile_destroyed_at_least) out.axis_mobile_h += 1.0;
                        }
                    }
                }
                if (f.pullbacks > back0[s] && over_since[s] >= 0.0) out.pullback_h.Add(c.Now() - over_since[s]);
                adv0[s] = f.advances;
                back0[s] = f.pullbacks;
                pending[s] = f.pending_h;
                const int si = static_cast<int>(s);
                const bool over = c.SectorLosses().Sorties(si, day, afd.loss_days) >= min_sorties && c.SectorLosses().Rate(si, day, afd.loss_days) > afd.loss_line;
                if (over && over_since[s] < 0.0) over_since[s] = c.Now();
                if (!over) over_since[s] = -1.0;
                if (f.contested && s < c.SectorCovers().size()) {
                    const SectorCover& sc = c.SectorCovers()[s];
                    low_cover[s] = std::min(low_cover[s], sc.cover);
                    high_mobile[s] = std::max(high_mobile[s], sc.mobile_destroyed);
                    // the faction's gates as written (the chief's traits move them by at most air_front.trait_shift)
                    if (sc.cover <= afd.cover_at_most) {
                        out.gate_h[0] += 1.0;
                        if (sc.mobile_destroyed >= afd.mobile_destroyed_at_least) {
                            out.gate_h[1] += 1.0;
                            if (s < c.AirFrontRewards().size()) out.gate_reward.Add(c.AirFrontRewards()[s]);
                            if (s < c.AirFrontRewards().size() && c.AirFrontRewards()[s] >= afd.min_reward) {
                                out.gate_h[2] += 1.0;
                                if (!over) out.gate_h[3] += 1.0;
                            }
                        }
                    }
                }
            }
        }
        if (g == nullptr) continue;
        const groundwar::GroundTables& t = g->Data().tables;
        for (std::size_t i = 0; i < nf; ++i) {
            const groundwar::Formation& f = g->Formations()[i];
            if (f.unit.side != groundwar::Side::Ours || f.spent || !(f.unit.establishment > 0.0)) continue;
            low_frac[i] = std::min(low_frac[i], g->StrengthAt(i, c.Now()) / f.unit.establishment);
        }
        // F7.1: a penetration's hours with its momentum spent and no counterattack on it, the best tip ratio then; our
        // counterattacks going in and ending (the line won back).
        for (std::size_t a = 0; a < na; ++a) {
            const groundwar::Axis& x = g->Axes()[a];
            const bool countered = x.status == groundwar::AxisStatus::Counterattacked;
            if (!x.raid && x.status == groundwar::AxisStatus::Halted && x.depth_m > 1000.0) held_ground[a] = 1;
            if (countered && counter_from_m[a] < 0.0) {
                counter_from_m[a] = x.depth_m;
                counter_from_h[a] = c.Now();
                ++out.counters_in;
            } else if (!countered && counter_from_m[a] >= 0.0) {
                out.counter_won_km.Add((counter_from_m[a] - x.depth_m) / 1000.0);
                out.counter_h.Add(c.Now() - counter_from_h[a]);
                counter_from_m[a] = -1.0;
            }
            if (x.raid || !x.through_zone || !x.counter.empty() || x.counter_incoming > 0) continue;
            if (x.status != groundwar::AxisStatus::Attacking && x.status != groundwar::AxisStatus::Halted) continue;
            if (!g->MomentumSpentSeen(a)) continue;
            out.spent_h += 1.0;
            double ours = 0.0;
            for (std::size_t i = 0; i < nf; ++i) {
                const groundwar::Formation& f = g->Formations()[i];
                if (f.unit.side != groundwar::Side::Ours || f.spent || f.task != groundwar::Task::None || f.role == groundwar::DeployRole::Support) continue;
                if (!f.free && std::find(x.depth_defenders.begin(), x.depth_defenders.end(), static_cast<int>(i)) == x.depth_defenders.end()) continue;
                ours += groundwar::UnitPower(g->Data().tables, f.unit.strength, f.unit.supply, f.unit.cohesion);
            }
            const double tip = g->TipPowerTruth(a);
            if (tip > 0.0) best_tip_spent[a] = std::max(best_tip_spent[a], ours / tip);
        }
        // The axes' hour: the ratio's class and the advance in it.
        for (std::size_t a = 0; a < na; ++a) {
            const groundwar::Axis& x = g->Axes()[a];
            const double moved = std::max(0.0, (x.depth_m - hour_depth[a]) / 1000.0);
            hour_depth[a] = x.depth_m;
            if (x.status != groundwar::AxisStatus::Attacking) continue;
            const int k = AdvanceClassOf(t, x.last_ratio);
            out.class_h[k] += 1.0;
            out.class_km[k] += moved;
            if (x.last_ratio < t.class_from_ratio[1]) out.below_km += moved;
            if (x.through_zone && x.counter.empty() && x.counter_incoming == 0) {
                double ours = 0.0, theirs = 0.0;
                for (std::size_t i = 0; i < nf; ++i) {
                    const groundwar::Formation& f = g->Formations()[i];
                    if (f.unit.side != groundwar::Side::Ours || f.spent || f.task != groundwar::Task::None || f.role == groundwar::DeployRole::Support) continue;
                    if (!f.free && std::find(x.depth_defenders.begin(), x.depth_defenders.end(), static_cast<int>(i)) == x.depth_defenders.end()) continue;
                    ours += groundwar::UnitPower(t, f.unit.strength, f.unit.supply, f.unit.cohesion);
                }
                for (int fi : x.first) {
                    const groundwar::Formation& f = g->Formations()[static_cast<std::size_t>(fi)];
                    theirs += groundwar::UnitPower(t, f.unit.strength, f.unit.supply, f.unit.cohesion);
                }
                if (theirs > 0.0) best_counter[a] = std::max(best_counter[a], ours / theirs);
                const double tip = g->TipPowerTruth(a);
                if (tip > 0.0) best_tip[a] = std::max(best_tip[a], ours / tip);
            }
            if (!x.raid) {
                axis_hours[a] += 1.0;
                if (offensive_h < 0.0) offensive_h = c.Now() - 1.0;
            }
        }
        if (h % 24 != 0) continue;
        for (std::size_t a = 0; a < na; ++a) {
            const groundwar::Axis& x = g->Axes()[a];
            if (!x.raid && axis_hours[a] >= 12.0) {
                const double km = (x.depth_m - axis_depth[a]) / 1000.0;
                out.attack_day_km.Add(km);
                out.fastest_day_km = std::max(out.fastest_day_km, km);
            }
            axis_depth[a] = x.depth_m;
            axis_hours[a] = 0.0;
        }
        for (std::size_t i = 0; i < nf; ++i) {
            const groundwar::Formation& f = g->Formations()[i];
            const int side = f.unit.side == groundwar::Side::Ours ? 0 : 1;
            const double battle = f.lost_attack + f.lost_defence + f.lost_air;
            const double fought = f.fought_h - day_fought[i];
            if (fought >= 12.0 && day_strength[i] > 0.0) {
                const double share = (battle - day_battle[i]) / day_strength[i];
                out.contact_day[side].Add(share);
                out.contact_over[side] += share > 0.066 ? 1 : 0;
            }
            day_battle[i] = battle;
            day_fought[i] = f.fought_h;
            day_strength[i] = g->StrengthAt(i, c.Now());
        }
        for (std::size_t i = 0; i < objs.size() && i < c.Truth().size(); ++i) {
            if (RadarSam(objs[i]) && c.Truth()[i].function <= 0.0) ever[i] = 1;
        }
    }
    c.SetOrderSource(nullptr);
    ++out.runs;
    out.days += days;
    const double end_h = c.Now();
    // The air war's loss rate a sortie.
    for (const Package& p : c.Packages()) {
        if (!p.resolved) continue;
        out.packages += 1.0;
        for (const Flight& fl : p.flights) {
            const AircraftType* ac = d.faction.Aircraft(fl.aircraft);
            if (ac != nullptr && (ac->drone || ac->missile)) continue;
            out.sorties += fl.count;
            out.lost += fl.lost;
        }
    }
    // The air front at the end.
    for (const SectorFront& f : c.AirFront().sectors) {
        out.advances += f.advances;
        out.pullbacks += f.pullbacks;
        out.sectors_moved += f.advances > 0 ? 1 : 0;
        out.deepest_km = std::max(out.deepest_km, f.depth_m / 1000.0);
    }
    for (const JournalEntry& j : c.Journal()) {
        if (j.kind == "air_front" && j.text.find("crisis") != std::string::npos) ++out.crisis_steps;
    }
    // F7.2: where the air front stands against the enemy's axes at the end, in each sector an axis pushed in.
    if (const groundwar::GroundWar* gw = c.Ground()) {
        std::vector<double> axis_km(ns, 0.0);
        for (const groundwar::Axis& x : gw->Axes()) {
            if (x.raid || x.sector < 0 || static_cast<std::size_t>(x.sector) >= ns) continue;
            axis_km[static_cast<std::size_t>(x.sector)] = std::max(axis_km[static_cast<std::size_t>(x.sector)], x.depth_m / 1000.0);
        }
        for (std::size_t s = 0; s < ns && s < c.AirFront().sectors.size(); ++s) {
            if (axis_km[s] < 1.0) continue;
            ++out.end_axis_sectors;
            out.end_axis_km.Add(axis_km[s]);
            out.end_front_km.Add(c.AirFront().sectors[s].depth_m / 1000.0);
            out.end_axis_stepped += c.AirFront().sectors[s].advances > 0 ? 1 : 0;
        }
    }
    // Our air defence: radar SAM fire units wrecked or overrun at the end; of those wrecked at some day's end, working again.
    {
        int all = 0, gone = 0;
        for (std::size_t i = 0; i < objs.size() && i < c.Truth().size(); ++i) {
            if (!RadarSam(objs[i])) continue;
            ++all;
            const ObjectState& st = c.Truth()[i];
            gone += st.function <= 0.0 || st.overrun ? 1 : 0;
            if (ever[i] != 0) {
                ++out.ad_ever;
                out.ad_back += st.function >= 0.5 && !st.overrun ? 1 : 0;
            }
        }
        if (all > 0) out.ad_lost.Add(static_cast<double>(gone) / all);
    }
    if (c.UnitMovesOn()) {
        std::map<std::pair<int, int>, int> per_day;  // (object, day) -> moves begun
        for (const UnitMove& m : c.UnitMoves()) {
            const int k = ++per_day[std::make_pair(m.object, static_cast<int>(m.start_h / 24.0))];
            out.most_moves_a_day = std::max(out.most_moves_a_day, k);
            const std::string& cls = c.MoveRules().classes[static_cast<std::size_t>(m.cls)].id;
            // the norm's move: 30-40 km (mt_1973_nvn); one of 50 km or less straight
            if (cls.find("s75") != std::string::npos && m.stage == 3 && Dist2d(m.from, m.to) <= 50000.0) {
                out.s75_h.Add(m.ready_h - m.heard_h);
                out.s75_km.Add(Dist2d(m.from, m.to) / 1000.0);
            }
        }
    }
    for (std::size_t s = 0; s < ns; ++s) {
        if (low_cover[s] <= 1.0) out.low_cover.Add(low_cover[s]);
        if (high_mobile[s] >= 0.0) out.low_mobile.Add(high_mobile[s]);
    }
    if (g == nullptr) return;
    // The ground war's tally.
    double battle[2] = {0.0, 0.0};
    for (std::size_t i = 0; i < nf; ++i) {
        const groundwar::Formation& f = g->Formations()[i];
        const int side = f.unit.side == groundwar::Side::Ours ? 0 : 1;
        if (!f.front) battle[side] += f.lost_attack + f.lost_defence + f.lost_air + f.lost_straggle;  // F8.1: the army's
        if (side != 0) continue;
        out.ours_air += f.lost_air;
        out.ours_battle += f.lost_attack + f.lost_defence + f.lost_air;
        if (f.fought_h >= 24.0) {
            ++out.ours_engaged;
            out.ours_out += f.withdrawals + f.dispersals > 0 ? 1 : 0;
            if (f.base_establishment > 0.0) {
                const double fr = f.unit.strength / f.base_establishment;
                out.ours_frac.Add(fr);
                out.ours_lowest = std::min(out.ours_lowest, fr);
                const double gross = (f.lost_attack + f.lost_defence + f.lost_air + f.lost_straggle + f.lost_rest) / f.base_establishment;
                out.ours_gross.Add(gross);
                out.ours_given.Add(fr - (1.0 - gross));
            }
            if (low_frac[i] <= 1.5) {
                out.ours_low_frac.Add(low_frac[i]);
                out.ours_low_any = std::min(out.ours_low_any, low_frac[i]);
            }
            const groundwar::EffectivenessDef& e = g->Data().ours.doctrine.effectiveness;
            out.ask_line = e.ineffective_below;
            out.degraded_line = e.degraded_below;
            out.ours_under_degraded += low_frac[i] < e.degraded_below ? 1 : 0;
            out.hq_line = e.hq_hit_ineffective_below;
            out.ours_under_hq += low_frac[i] < e.hq_hit_ineffective_below ? 1 : 0;
        }
    }
    const double op_days = offensive_h >= 0.0 ? (end_h - offensive_h) / 24.0 : 0.0;
    for (int side = 0; side < 2; ++side) {
        if (op_days >= 1.0 && start_strength[side] > 0.0) out.army_day[side].Add(battle[side] / start_strength[side] / op_days);
    }
    bool penetrated = false;
    for (const groundwar::Axis& x : g->Axes()) {
        out.att_lost += x.lost_attack;
        out.def_lost += x.lost_defence;
        if (x.lost_attack + x.lost_defence > 1.0 && x.lost_defence > 0.0) out.exchange.Add(x.lost_attack / x.lost_defence);
        if (x.raid || x.status == groundwar::AxisStatus::Waiting) continue;
        ++out.axes;
        const bool won = x.depth_m >= x.objective_m - 1000.0 || (x.status == groundwar::AxisStatus::Attacking && x.depth_m >= 0.5 * x.objective_m);
        out.axes_won += won ? 1 : 0;
        const std::size_t ai = static_cast<std::size_t>(&x - g->Axes().data());
        if (x.zone_h >= 0.0) {
            out.zone_h.Add(x.zone_h - x.start_h);
            ++out.penetrations;
            if (best_counter[ai] >= 0.0) out.counter_ratio.Add(best_counter[ai]);
            if (best_tip[ai] >= 0.0) out.tip_ratio.Add(best_tip[ai]);
            if (best_tip_spent[ai] >= 0.0) out.tip_spent_ratio.Add(best_tip_spent[ai]);
            penetrated = true;
        }
        // F7.1: an incursion halted on our ground (the neutral's in our security zone) is one our army counterattacks too.
        if (g->Data().tables.counter_at_tip && held_ground[ai] != 0) penetrated = true;
    }
    using GO = groundwar::GroundOrder;
    out.withdrawn += static_cast<int>(g->OrderCount(GO::WithdrawOrdered) + g->OrderCount(GO::WithdrawDecided));
    out.dispersed += static_cast<int>(g->OrderCount(GO::Dispersed));
    out.reliefs += static_cast<int>(g->OrderCount(GO::ReliefOrdered));
    out.refused += static_cast<int>(g->OrderCount(GO::WithdrawRefused));
    out.asked += static_cast<int>(g->OrderCount(GO::WithdrawAsked));
    const int counters = static_cast<int>(g->OrderCount(GO::CounterattackOrdered));
    out.counters += counters;
    if (counters > 0) out.counter_ordered_ratio.Add(g->CounterOrderedRatioSum() / counters, counters);
    if (g->FrontReserve() >= 0) {
        out.front_orders += g->FrontOrders();
        if (g->FrontOrders() > 0) out.front_ordered_ratio.Add(g->FrontOrderedRatioSum() / g->FrontOrders(), g->FrontOrders());
        if (g->FrontBestRatio() > 0.0) out.front_best_ratio.Add(g->FrontBestRatio());
    }
    out.wars_penetrated += penetrated ? 1 : 0;
    out.wars_countered += penetrated && counters > 0 ? 1 : 0;
    out.strikes += c.GroundAir().troop_strikes;
    out.strike_share += c.GroundAir().strength_lost;
}

// One band's row: the measure against the band of `id` for the faction (fronts_bench's), PASS, FAIL or ARGUED.
// A band's anchor for the table: its source up to the first parenthesis (the ids and pages), the rest in the data.
std::string HistAnchor(const std::string& s) {
    const std::size_t p = s.find(" (");
    std::string a = p == std::string::npos ? s : s.substr(0, p);
    if (a.size() > 72) a = a.substr(0, 69) + "...";
    return a;
}

int HistRow(const BalanceBands& bands, const std::string& theatre, const std::string& faction, const char* id, const char* label, double v,
            bool measured, double scale = 1.0, const char* unit = "") {
    const BalanceBand* b = bands.Find("fronts_bench", id, theatre, faction);
    if (!measured) {
        if (b != nullptr) std::printf("| %-44s | %10s | %.2f-%.2f%s | - | %s |\n", label, "none", b->lo * scale, b->hi * scale, unit, HistAnchor(b->source).c_str());
        else std::printf("| %-44s | %10s | - | - | - |\n", label, "none");
        return 0;
    }
    if (b == nullptr) {
        std::printf("| %-44s | %9.2f%s | - | - |\n", label, v * scale, unit);
        return 0;
    }
    const bool in = v >= b->lo - 1e-12 && v <= b->hi + 1e-12;
    const bool argued = b->status == "argued";
    std::printf("| %-44s | %9.2f%s | %.2f-%.2f%s | %s | %s |\n", label, v * scale, unit, b->lo * scale, b->hi * scale, unit, in ? "PASS" : argued ? "ARGUED" : "FAIL",
                HistAnchor(b->source).c_str());
    return in || argued ? 0 : 1;
}

// --history: each faction's war on the nation with every front mode on, against the bands (docs/fronts/06_BALANCE.md section 2).
int HistoryMain(const std::string& data, const std::string& theatre, int days, int seeds, bool gate, const std::vector<std::string>& factions,
                const std::vector<std::pair<std::string, double>>& sets) {
    command::ChainData chain;
    BalanceBands bands;
    std::string e;
    if (!command::LoadChainData(data, "command/chain_pannonia.json", chain, &e) || !LoadBalanceBands(data + "/air_war/balance_bands.json", bands, &e)) {
        std::printf("%s\n", e.c_str());
        return 1;
    }
    const std::vector<std::string> list = factions.empty() ? std::vector<std::string>{"warsaw_pact", "nato", "neutral", "rebels"} : factions;
    int fails = 0;
    std::printf("The fronts against history (F6.1) on %s: ground.mode 1, air_front.mode %g, ad_strategy.mode 1, %d days x %d seeds\n", theatre.c_str(), kFrontsAirFrontMode,
                days, seeds);
    for (const auto& kv : sets) std::printf("  --set %s=%g\n", kv.first.c_str(), kv.second);
    std::printf("\n");
    for (const std::string& faction : list) {
        AirWarData d;
        if (!LoadAirWarData(data, "air_war/" + theatre, "air_war/faction_" + faction + ".json", d, &e)) {
            std::printf("%s: %s\n", faction.c_str(), e.c_str());
            return 1;
        }
        d.tuning.values["ground.mode"] = 1.0;
        d.tuning.values["air_front.mode"] = kFrontsAirFrontMode;
        d.tuning.values["ad_strategy.mode"] = 1.0;
        for (const auto& kv : sets) d.tuning.values[kv.first] = kv.second;
        HistStats s;
        const double t0 = CpuSeconds();
        for (int k = 0; k < seeds; ++k) HistoryCampaign(d, static_cast<unsigned>(7 + k), days, chain, s);
        const double n = s.runs > 0 ? static_cast<double>(s.runs) : 1.0;
        std::printf("## %s (%.1f s CPU)\n\n", faction.c_str(), CpuSeconds() - t0);
        std::printf("| Measure | Value | Band | | Anchor |\n|---|---|---|---|---|\n");
        int f = 0;
        f += HistRow(bands, theatre, faction, "div_loss_contact_ours", "our formation's day in contact: losses", s.contact_day[0].Get(), s.contact_day[0].n > 0, 100.0, " %");
        f += HistRow(bands, theatre, faction, "div_loss_contact_enemy", "the enemy's formation's day in contact: losses", s.contact_day[1].Get(), s.contact_day[1].n > 0, 100.0, " %");
        f += HistRow(bands, theatre, faction, "army_loss_day_ours", "our army's battle losses a day of the offensive", s.army_day[0].Get(), s.army_day[0].n > 0, 100.0, " %");
        f += HistRow(bands, theatre, faction, "army_loss_day_enemy", "the enemy's army's a day of its offensive", s.army_day[1].Get(), s.army_day[1].n > 0, 100.0, " %");
        f += HistRow(bands, theatre, faction, "exchange", "attacker's losses over the defender's (axes)", s.def_lost > 0.0 ? s.att_lost / s.def_lost : 0.0, s.def_lost > 0.0);
        f += HistRow(bands, theatre, faction, "attack_day_km", "an axis's day of attack: km", s.attack_day_km.Get(), s.attack_day_km.n > 0, 1.0, " km");
        f += HistRow(bands, theatre, faction, "fastest_day_km", "the fastest day of any axis: km", s.fastest_day_km, s.attack_day_km.n > 0, 1.0, " km");
        f += HistRow(bands, theatre, faction, "below_threshold_km", "advance under the stall ratio: km", s.below_km, s.class_h[0] + s.class_h[1] > 0, 1.0, " km");
        f += HistRow(bands, theatre, faction, "security_zone_h", "the security zone: start to main zone, h", s.zone_h.Get(), s.zone_h.n > 0, 1.0, " h");
        f += HistRow(bands, theatre, faction, "attack_won", "the enemy's axes at or near their objective", s.axes > 0 ? static_cast<double>(s.axes_won) / s.axes : 0.0, s.axes > 0);
        f += HistRow(bands, theatre, faction, "ours_out", "our formations in the fight a day: withdrawn", s.ours_engaged > 0 ? static_cast<double>(s.ours_out) / s.ours_engaged : 0.0, s.ours_engaged > 0);
        f += HistRow(bands, theatre, faction, "counterattacked", "wars penetrated or held: we counterattack", s.wars_penetrated > 0 ? static_cast<double>(s.wars_countered) / s.wars_penetrated : 0.0, s.wars_penetrated > 0);
        f += HistRow(bands, theatre, faction, "air_share", "the air's share of our battle losses", s.ours_battle > 0.0 ? s.ours_air / s.ours_battle : 0.0, s.ours_battle > 0.0, 100.0, " %");
        f += HistRow(bands, theatre, faction, "strike_toll", "a strike on troops: of its division", s.strikes > 0 ? s.strike_share / s.strikes : 0.0, s.strikes > 0, 100.0, " %");
        f += HistRow(bands, theatre, faction, "air_front_latency_h", "the air front: gates holding to the step, h", s.latency_h.Get(), s.latency_h.n > 0, 1.0, " h");
        f += HistRow(bands, theatre, faction, "air_front_pullback_h", "the air front: loss line to the step back, h", s.pullback_h.Get(), s.pullback_h.n > 0, 1.0, " h");
        f += HistRow(bands, theatre, faction, "air_front_steps", "the air front's steps forward a war", s.advances / n, true);
        f += HistRow(bands, theatre, faction, "ad_lost", "our radar SAM fire units lost at the end", s.ad_lost.Get(), s.ad_lost.n > 0, 100.0, " %");
        f += HistRow(bands, theatre, faction, "ad_back", "of those wrecked, working again at the end", s.ad_ever > 0 ? static_cast<double>(s.ad_back) / s.ad_ever : 0.0, s.ad_ever > 0, 100.0, " %");
        f += HistRow(bands, theatre, faction, "s75_move_h", "an S-75 class move, heard to ready: h", s.s75_h.Get(), s.s75_h.n > 0, 1.0, " h");
        f += HistRow(bands, theatre, faction, "moves_a_day", "a unit's moves begun in a day, at most", static_cast<double>(s.most_moves_a_day), s.most_moves_a_day > 0);
        f += HistRow(bands, theatre, faction, "loss_share", "the enemy's aircraft lost a sortie", s.sorties > 0.0 ? s.lost / s.sorties : 0.0, s.sorties > 0.0, 100.0, " %");
        {
            // The air war's own band for the loss rate (air_war_balance's, the nation), read beside it.
            const BalanceBand* b = bands.Find("air_war_balance", "loss_share", theatre, faction);
            const double v = s.sorties > 0.0 ? s.lost / s.sorties : 0.0;
            if (b != nullptr) std::printf("|   (air_war_balance's band) | | %.2f-%.2f %% | %s | %s |\n", 100.0 * b->lo, 100.0 * b->hi, v >= b->lo && v <= b->hi ? "PASS" : "out", b->source.c_str());
        }
        std::printf("\n  formation-days in contact: ours %.0f (%d over 6.6 %%), the enemy's %.0f (%d over); axes %d (%d at or near the objective), penetrations of the main zone %d\n",
                    s.contact_day[0].n, s.contact_over[0], s.contact_day[1].n, s.contact_over[1], s.axes, s.axes_won, s.penetrations);
        std::printf("  orders a war: withdrawals %.1f, dispersals %.1f, asked %.1f, refused %.1f, reliefs %.1f, counterattacks %.1f (wars with a penetration %d, countered %d); "
                    "our formations a day in the fight %d, withdrawn %d, at the end %.0f %% of establishment (lowest %.0f %%)\n",
                    s.withdrawn / n, s.dispersed / n, s.asked / n, s.refused / n, s.reliefs / n, s.counters / n, s.wars_penetrated, s.wars_countered, s.ours_engaged, s.ours_out,
                    100.0 * s.ours_frac.Get(), 100.0 * (s.ours_frac.n > 0.0 ? s.ours_lowest : 0.0));
        std::printf("  why none withdraws (F7.3): our formations a day in the fight at their lowest in the war %.0f %% of establishment on average (the lowest %.0f %%; "
                    "%d of %d under the degraded line %.0f %%, the ask line %.0f %%; %d under the line with command and artillery hit, %.0f %%, which no rule "
                    "reads); their losses by every cause %.0f %% of establishment, given back by repairs and columns %.0f %%\n",
                    100.0 * s.ours_low_frac.Get(), 100.0 * (s.ours_low_frac.n > 0.0 ? s.ours_low_any : 0.0), s.ours_under_degraded, s.ours_engaged,
                    100.0 * s.degraded_line, 100.0 * s.ask_line, s.ours_under_hq, 100.0 * s.hq_line, 100.0 * s.ours_gross.Get(), 100.0 * s.ours_given.Get());
        std::printf("  a penetrated axis's best counterattack ratio (truth, its candidates over its attackers): %.2f (%.0f axes); over the enemy at its tip (F7.1): %.2f, "
                    "in the hours its momentum was spent %.2f (%.0f axes, %.0f h a war)\n",
                    s.counter_ratio.Get(), s.counter_ratio.n, s.tip_ratio.Get(), s.tip_spent_ratio.Get(), s.tip_spent_ratio.n, s.spent_h / n);
        std::printf("  our army reserve (F7.1): %.1f %% of our army's power at the start, %.0f km behind the line; counterattacks went in %d (%.1f a war; "
                    "the army's and the front's), ordered on %.2f, the line won back %.1f km in %.0f h (%.0f ended)\n",
                    100.0 * s.reserve_share.Get(), s.reserve_depth_km.Get(), s.counters_in, s.counters_in / n, s.counter_ordered_ratio.Get(), s.counter_won_km.Get(),
                    s.counter_h.Get(), s.counter_won_km.n);
        if (s.front_reserve) {
            std::printf("  the front reserve (F8.1): %.1f %% of our army's power at the start, its home %.0f km behind the line; the front's best ratio at a spent "
                        "penetration, his force in reach over the tip (his picture): %.2f (%.0f wars); counterstrikes ordered %d (%.1f a war) on %.2f\n",
                        100.0 * s.front_share.Get(), s.front_depth_km.Get(), s.front_best_ratio.Get(), s.front_best_ratio.n, s.front_orders, s.front_orders / n,
                        s.front_ordered_ratio.Get());
        }
        std::printf("  losses on the axes, T-55 equivalents a war: attacker %.1f, defender %.1f (per axis mean ratio %.2f); our air losses %.1f of %.1f; strikes on troops %.1f a war\n",
                    s.att_lost / n, s.def_lost / n, s.exchange.Get(), s.ours_air / n, s.ours_battle / n, s.strikes / n);
        std::printf("  the air front: %.1f steps forward and %.1f back a war (%d sectors moved, deepest %.0f km, %d crisis entries); latency %.1f h (%.0f), pull-back %.1f h (%.0f)\n",
                    s.advances / n, s.pullbacks / n, s.sectors_moved, s.deepest_km, s.crisis_steps, s.latency_h.Get(), s.latency_h.n, s.pullback_h.Get(), s.pullback_h.n);
        std::printf("  our air defence: %.1f %% of radar SAM fire units lost at the end, %d wrecked at a day's end and %d of them back; S-75 class moves %.0f, %.1f h for %.1f km\n",
                    100.0 * s.ad_lost.Get(), s.ad_ever, s.ad_back, s.s75_h.n, s.s75_h.Get(), s.s75_km.Get());
        std::printf("  the air front's gates: a contested sector's lowest cover %.2f, highest mobile destroyed %.2f (%.0f sectors); sector-hours a war the cover gate held %.0f, and the mobile %.0f, and the reward %.0f (mean %.3f), and the losses %.0f\n",
                    s.low_cover.Get(), s.low_mobile.Get(), s.low_cover.n, s.gate_h[0] / n, s.gate_h[1] / n, s.gate_h[2] / n, s.gate_reward.Get(), s.gate_h[3] / n);
        std::printf("  the air front behind its own axes (F7.2): sector-hours a war with an axis advancing in its picture %.0f, contested %.0f, the cover gate held %.0f, "
                    "and the mobile %.0f; steps forward there %.1f a war; at the end %d sectors pushed in (%.1f km on average), the air front there %.1f km beyond the line "
                    "on average, %d of them stepped\n",
                    s.axis_h / n, s.axis_contested_h / n, s.axis_cover_h / n, s.axis_mobile_h / n, s.axis_steps / n, s.end_axis_sectors, s.end_axis_km.Get(),
                    s.end_front_km.Get(), s.end_axis_stepped);
        std::printf("  the air war: %.1f packages, %.0f sorties, %.1f lost a war\n", s.packages / n, s.sorties / n, s.lost / n);
        std::printf("  advance by the ratio's class (hours attacking, km a day):");
        for (std::size_t k = 0; k < groundwar::kAdvanceClassCount; ++k) {
            std::printf(" %s %.0f h %.1f", k == 0 ? "stalled" : k == 1 ? "intense" : k == 2 ? "moderate" : k == 3 ? "slight" : "negligible", s.class_h[k],
                        s.class_h[k] > 0.0 ? 24.0 * s.class_km[k] / s.class_h[k] : 0.0);
        }
        std::printf("\n\n");
        fails += f;
    }
    if (fails > 0) std::printf("%d band(s) out\n", fails);
    return gate && fails > 0 ? 2 : 0;
}

}  // namespace

// ------------------------------------------------------------------ the player's effect (F6.2)

// The battery careful or careless: air_war_balance's two postures (the moves and the emission discipline the headless
// campaign exposes for the player's battery). The engagement record is not set: it follows from them (a battery found,
// struck and wrecked shoots down less), and is measured below as the battery's kills.
void PlayerPosture(BatteryPosture& p, bool careful) {
    if (careful) {
        p.emission = 0.1;
        p.emcon_discipline = 0.7;
        p.radio_discipline = 0.7;
        p.dummy_traffic = true;
        p.relocate_every_h = 20.0;
    } else {
        p.emission = 0.5;
        p.emcon_discipline = 0.2;
        p.radio_discipline = 0.2;
        p.dummy_traffic = false;
        p.relocate_every_h = 0.0;
    }
}

// One war as --player measures it: the line and the air front in every sector at the end, the battery's own sector's
// first step forward, the outcomes' ledger of its air front's delay, and the battery's record.
struct PlayerRun {
    int sector = -1;                 // the battery's sector at the war's start
    std::string sector_id;
    std::vector<double> line_km;     // each sector's line of contact at the end, km beyond the base line
    std::vector<int> af_steps;       // each sector's air front: steps forward and back
    std::vector<int> af_back;
    std::vector<double> af_km;       // and its depth at the end
    double first_step_h = -1.0;      // the battery's sector's first step forward (-1: none)
    double held_h = 0.0;             // the ledger (campaign/outcomes.hpp): hours the battery's entry held its sector's air front
    double watched_h = 0.0;
    int kills = 0;                   // the enemy's manned aircraft and cruise missiles the battery shot down
    bool wrecked = false;            // wrecked at some hour's end
    double knowledge = 0.0;          // EnemyKnowledgeOfBattery at the end
    double sorties = 0.0, lost = 0.0;
    double cpu_s = 0.0;
};

bool PlayerCampaign(const AirWarData& d, unsigned seed, int days, const command::ChainData& chain, bool careful, PlayerRun& out) {
    const double t0 = CpuSeconds();
    AirCampaign c;
    if (!c.Init(d, seed)) return false;
    PlayerPosture(c.Posture(), careful);
    command::InfoNet net;
    net.Init(chain, seed, d.theatre.start_hour);
    command::GiveTheatreSky(net, d.theatre);
    command::CampaignFeed feed;
    feed.Bind(&net);
    c.SetOrderSource(feed.OrderSource());
    const std::string own = campaign::OwnObject(c);
    c.ObserveOutcomes([&out, &d, own](const Package& p, const PackageOutcome& o) {
        for (const LossRecord& l : o.losses) {
            if (l.by != own || l.flight < 0 || static_cast<std::size_t>(l.flight) >= p.flights.size()) continue;
            const AircraftType* a = d.faction.Aircraft(p.flights[static_cast<std::size_t>(l.flight)].aircraft);
            if (a != nullptr && a->drone) continue;  // decoy drones are meant to be shot down
            ++out.kills;
        }
        for (const std::string& by : o.missiles_downed) out.kills += by == own ? 1 : 0;
    });
    const std::size_t player = static_cast<std::size_t>(c.PlayerIndex());
    const LineOfContact& line = c.FrontLine();
    const std::size_t ns = line.SectorCount();
    if (!g_battery_sector.empty() && !line.Empty()) {
        const int k = line.SectorIndex(g_battery_sector);
        if (k < 0) {
            std::printf("--battery-sector: no sector %s\n", g_battery_sector.c_str());
            return false;
        }
        const SectorDef& sd = line.Sector(static_cast<std::size_t>(k));
        const double mid = 0.5 * (sd.from_m + sd.to_m);
        Vec3 to = line.PointAt(mid) + line.InwardNormal(mid) * (g_battery_back_km * 1000.0);
        to.z = c.Truth()[player].pos.z;
        c.RelocateBattery(to);
    }
    out.sector = line.Empty() ? -1 : line.SectorOf(c.Truth()[player].pos);
    if (out.sector >= 0) out.sector_id = line.Sector(static_cast<std::size_t>(out.sector)).id;
    campaign::OutcomeLedger led;
    for (int h = 1; h <= days * 24; ++h) {
        while (c.Now() < h) {
            if (c.Advance(static_cast<double>(h))) c.SkipPlayerMission();
            feed.Pump(c);
        }
        if (c.Truth()[player].function <= 0.0) out.wrecked = true;
        led.AirFront(campaign::CheckAirFront(c, c.Now()), 1.0);
        if (out.first_step_h < 0.0 && out.sector >= 0 && static_cast<std::size_t>(out.sector) < c.AirFront().sectors.size() &&
            c.AirFront().sectors[static_cast<std::size_t>(out.sector)].advances > 0) {
            out.first_step_h = c.Now();
        }
    }
    c.SetOrderSource(nullptr);
    out.line_km.assign(ns, 0.0);
    out.af_steps.assign(ns, 0);
    out.af_back.assign(ns, 0);
    out.af_km.assign(ns, 0.0);
    for (std::size_t k = 0; k < ns; ++k) {
        if (c.Ground() != nullptr) out.line_km[k] = groundwar::GroundWar::SectorDepth(line, k) / 1000.0;
        if (k < c.AirFront().sectors.size()) {
            const SectorFront& f = c.AirFront().sectors[k];
            out.af_steps[k] = f.advances;
            out.af_back[k] = f.pullbacks;
            out.af_km[k] = f.depth_m / 1000.0;
        }
    }
    out.held_h = led.AirFrontHeldH();
    out.watched_h = led.AirFrontWatchedH();
    out.knowledge = c.EnemyKnowledgeOfBattery();
    for (const Package& p : c.Packages()) {
        if (!p.resolved) continue;
        for (const Flight& fl : p.flights) {
            const AircraftType* ac = d.faction.Aircraft(fl.aircraft);
            if (ac != nullptr && (ac->drone || ac->missile)) continue;
            out.sorties += fl.count;
            out.lost += fl.lost;
        }
    }
    out.cpu_s = CpuSeconds() - t0;
    return true;
}

// --player: each faction's war twice on the same seeds, the battery careful and careless, every front mode on and our
// pipeline pumped (docs/fronts/06_BALANCE.md section 4): the difference in the battery's sector (the line at the end, the air
// front's steps and its first step's hour) against the bands, and elsewhere against the draws' spread (the same posture's
// seeds against each other).
int PlayerMain(const std::string& data, const std::string& theatre, int days, int seeds, bool gate, const std::vector<std::string>& factions,
               const std::vector<std::pair<std::string, double>>& sets) {
    command::ChainData chain;
    BalanceBands bands;
    std::string e;
    if (!command::LoadChainData(data, "command/chain_pannonia.json", chain, &e) || !LoadBalanceBands(data + "/air_war/balance_bands.json", bands, &e)) {
        std::printf("%s\n", e.c_str());
        return 1;
    }
    const std::vector<std::string> list = factions.empty() ? std::vector<std::string>{"warsaw_pact", "nato", "neutral", "rebels"} : factions;
    int fails = 0;
    std::printf("The player's effect (F6.2) on %s: ground.mode 1, air_front.mode %g, ad_strategy.mode 1, %d days x %d seeds, the battery careless and careful\n",
                theatre.c_str(), kFrontsAirFrontMode, days, seeds);
    for (const auto& kv : sets) std::printf("  --set %s=%g\n", kv.first.c_str(), kv.second);
    std::printf("\n");
    for (const std::string& faction : list) {
        AirWarData d;
        if (!LoadAirWarData(data, "air_war/" + theatre, "air_war/faction_" + faction + ".json", d, &e)) {
            std::printf("%s: %s\n", faction.c_str(), e.c_str());
            return 1;
        }
        d.tuning.values["ground.mode"] = 1.0;
        d.tuning.values["air_front.mode"] = kFrontsAirFrontMode;
        d.tuning.values["ad_strategy.mode"] = 1.0;
        for (const auto& kv : sets) d.tuning.values[kv.first] = kv.second;
        std::vector<PlayerRun> runs[2];  // [0] careless, [1] careful
        double cpu = 0.0;
        for (int k = 0; k < seeds; ++k) {
            for (int careful = 0; careful < 2; ++careful) {
                PlayerRun r;
                if (!PlayerCampaign(d, static_cast<unsigned>(7 + k), days, chain, careful != 0, r)) {
                    std::printf("%s: the campaign does not start\n", faction.c_str());
                    return 1;
                }
                cpu += r.cpu_s;
                runs[careful].push_back(r);
            }
        }
        std::printf("## %s (%.1f s CPU)\n\n", faction.c_str(), cpu);
        std::printf("| Seed | The battery's sector | Line careless | careful | Air front careless: steps (first) | careful | Held by the battery careless | careful | "
                    "Kills careless | careful | Wrecked | Enemy knows |\n|---|---|---|---|---|---|---|---|---|---|---|---|\n");
        HistMean line_abs, line_signed, other_abs, spread, other_steps, step_spread, delay_h;
        int worst_steps = 0, delay_none = 0;
        for (int k = 0; k < seeds; ++k) {
            const PlayerRun& a = runs[0][static_cast<std::size_t>(k)];
            const PlayerRun& b = runs[1][static_cast<std::size_t>(k)];
            const std::size_t s = static_cast<std::size_t>(std::max(0, a.sector));
            const bool have = a.sector >= 0 && s < a.line_km.size() && s < b.line_km.size();
            char fa[64], fb[64];
            std::snprintf(fa, sizeof(fa), "%d (%s)", have ? a.af_steps[s] : 0, a.first_step_h >= 0.0 ? std::to_string(static_cast<int>(a.first_step_h)).append(" h").c_str() : "-");
            std::snprintf(fb, sizeof(fb), "%d (%s)", have ? b.af_steps[s] : 0, b.first_step_h >= 0.0 ? std::to_string(static_cast<int>(b.first_step_h)).append(" h").c_str() : "-");
            std::printf("| %d | %s | %.1f km | %.1f km | %s | %s | %.0f h of %.0f | %.0f h of %.0f | %d | %d | %s/%s | %.2f/%.2f |\n", 7 + k, a.sector_id.c_str(),
                        have ? a.line_km[s] : 0.0, have ? b.line_km[s] : 0.0, fa, fb, a.held_h, a.watched_h, b.held_h, b.watched_h, a.kills, b.kills,
                        a.wrecked ? "yes" : "no", b.wrecked ? "yes" : "no", a.knowledge, b.knowledge);
            if (!have) continue;
            line_abs.Add(std::fabs(b.line_km[s] - a.line_km[s]));
            line_signed.Add(b.line_km[s] - a.line_km[s]);
            worst_steps = std::max(worst_steps, std::abs(b.af_steps[s] - a.af_steps[s]));
            if (a.first_step_h >= 0.0 && b.first_step_h >= 0.0) {
                delay_h.Add(b.first_step_h - a.first_step_h);
            } else if (a.first_step_h >= 0.0 || b.first_step_h >= 0.0) {
                ++delay_none;  // one stepped, the other not: a step's difference (above), no delay to measure
            }
            for (std::size_t o = 0; o < a.line_km.size() && o < b.line_km.size(); ++o) {
                if (o == s) continue;
                other_abs.Add(std::fabs(b.line_km[o] - a.line_km[o]));
                other_steps.Add(std::abs(b.af_steps[o] - a.af_steps[o]));
                // The draws' spread: the same posture's other seeds in the same sector.
                for (int j = k + 1; j < seeds; ++j) {
                    for (int p = 0; p < 2; ++p) {
                        const PlayerRun& x = runs[p][static_cast<std::size_t>(k)];
                        const PlayerRun& y = runs[p][static_cast<std::size_t>(j)];
                        if (o < y.line_km.size()) {
                            spread.Add(std::fabs(x.line_km[o] - y.line_km[o]));
                            step_spread.Add(std::abs(x.af_steps[o] - y.af_steps[o]));
                        }
                    }
                }
            }
        }
        std::printf("\n| Measure | Value | Band | | Anchor |\n|---|---|---|---|---|\n");
        int f = 0;
        f += HistRow(bands, theatre, faction, "player_line_km", "the battery's sector's line: careful against careless", line_abs.Get(), line_abs.n > 0, 1.0, " km");
        f += HistRow(bands, theatre, faction, "player_air_front_steps", "the battery's sector's air front: steps apart", static_cast<double>(worst_steps), line_abs.n > 0);
        f += HistRow(bands, theatre, faction, "player_air_front_delay_h", "its first step: careful later by, h", delay_h.Get(), delay_h.n > 0, 1.0, " h");
        // Elsewhere: the careful battery's war against the careless one's in the other sectors, over the draws' spread there.
        const double ratio = spread.Get() > 1e-9 ? other_abs.Get() / spread.Get() : (other_abs.Get() > 1e-9 ? 99.0 : 0.0);
        f += HistRow(bands, theatre, faction, "player_elsewhere", "elsewhere: the line apart over the draws' spread", ratio, other_abs.n > 0);
        std::printf("\n  the battery's sector: the line %.2f km apart on average (careful minus careless %+.2f km), the air front at most %d step(s) apart, "
                    "its first step %+.1f h (%.0f seeds both stepped, %d one only)\n",
                    line_abs.Get(), line_signed.Get(), worst_steps, delay_h.Get(), delay_h.n, delay_none);
        std::printf("  elsewhere: the line %.2f km apart against %.2f km between seeds; the air front %.2f steps apart against %.2f between seeds\n",
                    other_abs.Get(), spread.Get(), other_steps.Get(), step_spread.Get());
        double held[2] = {0.0, 0.0}, kills[2] = {0.0, 0.0}, wrecked[2] = {0.0, 0.0}, lost[2] = {0.0, 0.0}, sorties[2] = {0.0, 0.0};
        for (int p = 0; p < 2; ++p) {
            for (const PlayerRun& r : runs[p]) {
                held[p] += r.held_h;
                kills[p] += r.kills;
                wrecked[p] += r.wrecked ? 1.0 : 0.0;
                lost[p] += r.lost;
                sorties[p] += r.sorties;
            }
        }
        const double n = seeds > 0 ? static_cast<double>(seeds) : 1.0;
        std::printf("  the ledger's air front held by the battery: %.1f h careless, %.1f h careful a war; the battery's kills %.1f and %.1f, wrecked in %.0f %% and %.0f %%; "
                    "the enemy's loss a sortie %.2f %% and %.2f %%\n\n",
                    held[0] / n, held[1] / n, kills[0] / n, kills[1] / n, 100.0 * wrecked[0] / n, 100.0 * wrecked[1] / n,
                    sorties[0] > 0.0 ? 100.0 * lost[0] / sorties[0] : 0.0, sorties[1] > 0.0 ? 100.0 * lost[1] / sorties[1] : 0.0);
        fails += f;
    }
    if (fails > 0) std::printf("%d band(s) out\n", fails);
    return gate && fails > 0 ? 2 : 0;
}

// --map: the situation maps of some posts at the end of a campaign with every front mode on, against the truth.
// The truth at the end of a day, for map_view.py --front (this bench only): the line of contact at every station of the base line,
// the air front's depth by sector, the formations where they are.
static json::Value TruthDayValue(const AirCampaign& c, int day) {
    using namespace airwar::jsonb;
    json::Value d = Obj();
    Put(d, "day", Num(day));
    Put(d, "hour", Num(c.Now()));
    const LineOfContact& line = c.FrontLine();
    json::Value offs = Arr(), pts = Arr();
    for (std::size_t i = 0; i < line.StationCount(); ++i) {
        Push(offs, Num(line.StationOffset(i)));
        const Vec3 p = line.ContactPoint(i);
        json::Value xy = Arr();
        Push(xy, Num(p.x));
        Push(xy, Num(p.y));
        Push(pts, xy);
    }
    Put(d, "offset_m", offs);
    Put(d, "line", pts);
    json::Value af = Arr();
    for (double depth : c.AirFrontDepths()) Push(af, Num(depth));
    Put(d, "air_front_m", af);
    json::Value fs = Arr();
    if (const groundwar::GroundWar* g = c.Ground()) {
        for (const groundwar::Formation& f : g->Formations()) {
            json::Value j = Obj();
            Put(j, "name", Str(f.name));
            Put(j, "kind", Str(f.kind));
            Put(j, "ours", Bool(f.unit.side == groundwar::Side::Ours));
            Put(j, "x", Num(f.pos.x));
            Put(j, "y", Num(f.pos.y));
            Put(j, "fraction", Num(f.unit.Fraction()));
            Put(j, "axis", Num(f.axis));
            Put(j, "spent", Bool(f.spent));
            Push(fs, j);
        }
    }
    Put(d, "formations", fs);
    return d;
}

int MapMain(const std::string& data, const std::string& theatre, int days, int seeds, const std::string& posts_arg, const std::vector<std::string>& factions,
            const std::string& json_path = std::string()) {
    command::ChainData chain;
    command::OwnAirDef own_air;
    std::string e;
    if (!command::LoadChainData(data, "command/chain_pannonia.json", chain, &e) || !command::LoadOwnAirFile(data + "/command/own_air.json", own_air, &e)) {
        std::printf("%s\n", e.c_str());
        return 1;
    }
    std::vector<std::string> posts;
    for (std::size_t from = 0; from <= posts_arg.size();) {
        std::size_t to = posts_arg.find(',', from);
        if (to == std::string::npos) to = posts_arg.size();
        if (to > from) posts.push_back(posts_arg.substr(from, to - from));
        from = to + 1;
    }
    for (const std::string& p : posts) {
        if (chain.PositionIndex(p) < 0) {
            std::printf("no such post: %s\n", p.c_str());
            return 1;
        }
    }
    const std::vector<std::string> list = factions.empty() ? std::vector<std::string>{"warsaw_pact", "nato"} : factions;
    json::Value runs = airwar::jsonb::Arr();
    for (const std::string& faction : list) {
        AirWarData d;
        if (!LoadAirWarData(data, "air_war/" + theatre, "air_war/faction_" + faction + ".json", d, &e)) {
            std::printf("%s: %s\n", faction.c_str(), e.c_str());
            return 1;
        }
        d.tuning.values["ground.mode"] = 1.0;
        d.tuning.values["air_front.mode"] = kFrontsAirFrontMode;
        d.tuning.values["ad_strategy.mode"] = 1.0;
        for (int s = 0; s < seeds; ++s) {
            const unsigned seed = static_cast<unsigned>(7 + s);
            AirCampaign c;
            c.Init(d, seed);
            command::InfoNet net;
            net.Init(chain, seed, d.theatre.start_hour);
            command::GiveTheatreSky(net, d.theatre);
            command::CampaignFeed feed;
            feed.Bind(&net);
            c.SetOrderSource(feed.OrderSource());
            json::Value truth_days = airwar::jsonb::Arr();
            if (json_path.empty()) {
                while (c.Now() < days * 24.0) {
                    if (c.Advance(days * 24.0)) c.SkipPlayerMission();
                    feed.Pump(c);
                }
            } else {
                // the same war, stopped at each day's end to write the truth of the day (the dump's, not the player's)
                for (int day = 0; day <= days; ++day) {
                    while (c.Now() < day * 24.0) {
                        if (c.Advance(day * 24.0)) c.SkipPlayerMission();
                        feed.Pump(c);
                    }
                    airwar::jsonb::Push(truth_days, TruthDayValue(c, day));
                }
            }
            c.SetOrderSource(nullptr);
            const command::SituationLayout layout = command::SituationLayoutOf(c, &feed.Ad(), &own_air);
            const groundwar::GroundWar* g = c.Ground();
            json::Value posts_json = airwar::jsonb::Obj();
            std::printf("==== %s, seed %u, %d days (hour %.1f): the ground war %s, %zu axes, %zu reports, %zu orders, %zu summaries\n", faction.c_str(), seed, days, c.Now(),
                        feed.Ground().On() ? "on" : "off", layout.axes.size(), feed.Ground().Reports().size(), feed.Ground().Orders().size(), feed.Ground().Summaries().size());
            std::set<std::string> dumps;
            for (const std::string& post : posts) {
                const int pi = net.Data().PositionIndex(post);
                command::SituationMap m;
                command::BuildSituationMap(net, feed, layout, pi, m);
                std::printf("\n%s", command::SituationMapText(m, d.theatre.start_hour).c_str());
                dumps.insert(command::SituationMapJson(m));
                airwar::jsonb::Put(posts_json, post, command::SituationMapValue(m));
                // against the truth (this bench only)
                if (g != nullptr) {
                    std::printf("  against the truth (axis: reported / true depth, km; age of the report, h):\n");
                    for (std::size_t k = 0; k < g->Axes().size(); ++k) {
                        const double truth_m = g->Axes()[k].depth_m;
                        const command::MapAxis* held = nullptr;
                        for (const command::MapAxis& a : m.axes) held = a.index == static_cast<int>(k) ? &a : held;
                        if (held != nullptr) {
                            std::printf("    %-26s %6.1f / %6.1f  (%+6.1f)  age %5.1f h%s\n", held->name.c_str(), held->depth_m / 1000.0, truth_m / 1000.0,
                                        (held->depth_m - truth_m) / 1000.0, held->mark.age_h, held->mark.stale ? " stale" : "");
                        } else if (truth_m > 0.0) {
                            std::printf("    %-26s  not reported to the post / %6.1f  (the axis is %.1f km in)\n", g->Axes()[k].name.c_str(), truth_m / 1000.0, truth_m / 1000.0);
                        }
                    }
                }
                if (!m.line.empty() && c.FrontLine().StationCount() == m.line.size()) {
                    double sum = 0.0, worst = 0.0, true_sum = 0.0;
                    for (std::size_t i = 0; i < m.line.size(); ++i) {
                        const double truth_m = std::max(0.0, c.FrontLine().StationOffset(i));
                        const double diff = std::fabs(m.line[i].depth_m - truth_m);
                        sum += diff;
                        worst = std::max(worst, diff);
                        true_sum += truth_m;
                    }
                    const double n = static_cast<double>(m.line.size());
                    std::printf("  the believed line against the true one over %zu stations: mean %.2f km, worst %.1f km (the true line is %.2f km in on average); oldest mark %.1f h\n",
                                m.line.size(), sum / n / 1000.0, worst / 1000.0, true_sum / n / 1000.0, m.OldestAgeH());
                }
                const int reps = 200;
                const double t0 = CpuSeconds();
                for (int r = 0; r < reps; ++r) command::BuildSituationMap(net, feed, layout, pi, m);
                std::printf("  built in %.3f ms (%zu marks)\n", (CpuSeconds() - t0) * 1000.0 / reps,
                            m.axes.size() + m.ours.size() + m.enemy.size() + m.priorities.size() + m.air_front.size() + m.sams.size() + m.orders.size());
            }
            std::printf("\n%zu posts, %zu different maps\n\n", posts.size(), dumps.size());
            if (!json_path.empty()) {
                using namespace airwar::jsonb;
                json::Value run = Obj();
                Put(run, "faction", Str(faction));
                Put(run, "seed", Num(seed));
                Put(run, "days", Num(days));
                Put(run, "start_hour", Num(d.theatre.start_hour));
                Put(run, "end_hour", Num(c.Now()));
                json::Value base = Arr(), sectors = Arr(), stations = Arr();
                for (const Vec3& p : c.FrontLine().BaseLine()) {
                    json::Value xy = Arr();
                    Push(xy, Num(p.x));
                    Push(xy, Num(p.y));
                    Push(base, xy);
                }
                for (const SectorDef& sd : c.FrontLine().Sectors()) {
                    json::Value j = Obj();
                    Put(j, "id", Str(sd.id));
                    Put(j, "name", Str(sd.name.empty() ? sd.id : sd.name));
                    Put(j, "from_m", Num(sd.from_m));
                    Put(j, "to_m", Num(sd.to_m));
                    Push(sectors, j);
                }
                for (std::size_t i = 0; i < c.FrontLine().StationCount(); ++i) {
                    const double along = c.FrontLine().StationAlong(i);
                    const Vec3 b = c.FrontLine().PointAt(along), n = c.FrontLine().InwardNormal(along);
                    json::Value j = Obj();
                    Put(j, "along_m", Num(along));
                    Put(j, "x", Num(b.x));
                    Put(j, "y", Num(b.y));
                    Put(j, "nx", Num(n.x));
                    Put(j, "ny", Num(n.y));
                    Put(j, "sector", Num(c.FrontLine().SectorAt(along)));
                    Push(stations, j);
                }
                Put(run, "base_line", base);
                Put(run, "sectors", sectors);
                Put(run, "stations", stations);
                Put(run, "truth", truth_days);
                Put(run, "posts", posts_json);
                Push(runs, run);
            }
        }
    }
    if (!json_path.empty()) {
        json::Value root = airwar::jsonb::Obj();
        airwar::jsonb::Put(root, "kind", airwar::jsonb::Str("fronts_map"));
        airwar::jsonb::Put(root, "runs", runs);
        std::ofstream f(json_path, std::ios::binary);
        if (!f) {
            std::printf("cannot write %s\n", json_path.c_str());
            return 1;
        }
        f << airwar::jsonb::Dump(root, 0, 17);
        f.close();
        std::printf("wrote %s\n", json_path.c_str());
    }
    return 0;
}

int main(int argc, char** argv) {
    std::string data = "data", theatre = "theatre_pannonia_nation.json", player, map_posts, map_json;
    int days = 7, seeds = 3, sectors = 8, campaign_days = 10, campaign_seeds = 1;
    bool air_front = false, print_moves = false, trace = false, watch = false, sectors_given = false, ground = false, gate = false,
         days_given = false, seeds_given = false, feed = false, ad = false, history = false, player_effect = false;
    std::vector<std::string> factions;
    std::vector<std::pair<std::string, double>> sets;  // --player --set
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--set" && i + 1 < argc) {
            const std::string kv = argv[++i];
            const std::size_t eq = kv.find('=');
            if (eq == std::string::npos || TuningTable::Defaults().count(kv.substr(0, eq)) == 0) {
                std::printf("--set needs a tuning key=value: %s\n", kv.c_str());
                return 1;
            }
            sets.push_back({kv.substr(0, eq), std::atof(kv.c_str() + eq + 1)});
        } else if (a == "--air-front") {
            air_front = true;
        } else if (a == "--ground") {
            ground = true;
        } else if (a == "--gate") {
            gate = true;
        } else if (a == "--log") {
            g_ground_log = true;
        } else if (a == "--player-sector" && i + 1 < argc) {
            player = argv[++i];
        } else if (a == "--battery-sector" && i + 1 < argc) {
            g_battery_sector = argv[++i];
        } else if (a == "--battery-back-km" && i + 1 < argc) {
            g_battery_back_km = std::atof(argv[++i]);
        } else if (a == "--campaign-days" && i + 1 < argc) {
            campaign_days = std::atoi(argv[++i]);
        } else if (a == "--campaign-seeds" && i + 1 < argc) {
            campaign_seeds = std::atoi(argv[++i]);
        } else if (a == "--watch") {
            watch = true;
        } else if (a == "--feed") {
            feed = true;
        } else if (a == "--ad") {
            ad = true;
        } else if (a == "--history") {
            history = true;
        } else if (a == "--player") {
            player_effect = true;
        } else if (a == "--map" && i + 1 < argc) {
            map_posts = argv[++i];
        } else if (a == "--map-json" && i + 1 < argc) {
            map_json = argv[++i];
        } else if (a == "--no-fighters") {
            g_no_fighters = true;
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
    if (!map_posts.empty()) return MapMain(data, theatre, days_given ? days : 10, seeds_given ? seeds : 1, map_posts, factions, map_json);
    if (player_effect) return PlayerMain(data, theatre, days_given ? days : 7, seeds_given ? seeds : 3, gate, factions, sets);
    if (history) return HistoryMain(data, theatre, days_given ? days : 14, seeds_given ? seeds : 3, gate, factions, sets);
    if (ad) return AdMain(data, theatre, days_given ? days : 10, seeds_given ? seeds : 3, gate, factions);
    if (feed) return FeedMain(data, theatre, days_given ? days : 10, seeds_given ? seeds : 3, gate, factions);
    if (ground) return GroundMain(data, theatre, days_given ? days : 30, seeds_given ? seeds : 5, campaign_days, campaign_seeds, gate, factions, player);
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
        std::printf("fronts_bench: --air-front (F2.2-F2.4), --watch (F2.5), --ground (F3.3), --feed (F3.8) or --ad (F4.1)\n");
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
