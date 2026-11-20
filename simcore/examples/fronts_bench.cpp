// fronts_bench: what track F's fronts cost (docs/fronts/README.md, "Efficiency"). Not a test: a table to read.
//
// Usage: fronts_bench [data_dir] --air-front [--days N] [--seeds N] [--sectors N] [--theatre file] [--moves] [--trace] [faction ...]
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
// Factions default to warsaw_pact and nato. CPU time by std::clock (docs/air_battle/01_ROUTES.md: wall times move with
// the machine's other load). Exits 2 when a cycle is over budget or the war differs with the mode on and no air front moved.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

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

}  // namespace

int main(int argc, char** argv) {
    std::string data = "data", theatre = "theatre_pannonia_nation.json";
    int days = 7, seeds = 3, sectors = 8;
    bool air_front = false, print_moves = false, trace = false;
    std::vector<std::string> factions;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--air-front") {
            air_front = true;
        } else if (a == "--moves") {
            print_moves = true;
        } else if (a == "--trace") {
            trace = true;
        } else if (a == "--days" && i + 1 < argc) {
            days = std::atoi(argv[++i]);
        } else if (a == "--seeds" && i + 1 < argc) {
            seeds = std::atoi(argv[++i]);
        } else if (a == "--sectors" && i + 1 < argc) {
            sectors = std::atoi(argv[++i]);
        } else if (a == "--theatre" && i + 1 < argc) {
            theatre = argv[++i];
        } else if (i == 1 && a.rfind("--", 0) != 0) {
            data = a;
        } else {
            factions.push_back(a);
        }
    }
    if (!air_front) {
        std::printf("fronts_bench: only --air-front is built (F2.2-F2.3); the ground war's parts come with F3.3\n");
        return 1;
    }
    if (factions.empty()) factions = {"warsaw_pact", "nato"};
    const double budget_ms = 1.0;
    int fails = 0;
    std::printf("The air front's measure and decision (F2.2-F2.3) on %s, %s sectors, %d days, %d seeds; budget %.1f ms a planning cycle\n\n",
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
