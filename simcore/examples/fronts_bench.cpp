// fronts_bench: what track F's fronts cost (docs/fronts/README.md, "Efficiency"). Not a test: a table to read.
//
// Usage: fronts_bench [data_dir] --air-front [--days N] [--seeds N] [--sectors N] [--theatre file] [faction ...]
//   --air-front (F2.2, the only part built so far): each faction's campaign run with air_front.mode 1 for N days (default
//   7) over N seeds (default 3) on the theatre (default theatre_pannonia_nation.json), its front cut into --sectors equal
//   sectors (default 8, the most the design gives a front; 0: the theatre's own); at each day's end the measure of every
//   sector's cover (the air staff's per-cycle read, SectorCoverBook::Measure) is timed over 2000 repetitions on that day's
//   picture, once with the bands kept (the cached baselines) and once with every band moved each time (every sector's
//   baseline taken afresh from the pre-war picture, the worst a cycle can be once F2.3 moves the air front). Prints the CPU
//   time per measure (mean and worst day) against the budget, 1 ms per planning cycle for a front, and the campaign's
//   whole CPU time with the mode off and on (the same seeds), which must be the same war.
// Factions default to warsaw_pact and nato. CPU time by std::clock (docs/air_battle/01_ROUTES.md: wall times move with
// the machine's other load). Exits 2 when the measure is over budget or the war differs with the mode on.
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

std::uint64_t JournalHash(const AirCampaign& c) {
    std::uint64_t h = 1469598103934665603ull;
    for (const auto& j : c.Journal()) {
        for (char ch : j.text) {
            h ^= static_cast<unsigned char>(ch);
            h *= 1099511628211ull;
        }
        h ^= 0x0a;
        h *= 1099511628211ull;
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
    bool air_front = false;
    std::vector<std::string> factions;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--air-front") {
            air_front = true;
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
        std::printf("fronts_bench: only --air-front is built (F2.2); the ground war's parts come with F3.3\n");
        return 1;
    }
    if (factions.empty()) factions = {"warsaw_pact", "nato"};
    const double budget_ms = 1.0;
    int fails = 0;
    std::printf("The air front's measure (F2.2) on %s, %s sectors, %d days, %d seeds; budget %.1f ms a planning cycle\n\n", theatre.c_str(),
                sectors > 0 ? (std::to_string(sectors) + " equal").c_str() : "the theatre's", days, seeds, budget_ms);
    std::printf("%-12s %8s %8s %10s %10s %10s %10s %12s %12s %8s\n", "faction", "sectors", "entries", "mean_ms", "worst_ms", "moved_ms",
                "prewar", "cpu_off_s", "cpu_on_s", "same");
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
        double sum_ms = 0.0, worst_ms = 0.0, worst_moved_ms = 0.0, entries = 0.0, prewar = 0.0, cpu_off = 0.0, cpu_on = 0.0;
        std::size_t sector_count = 0;
        int samples = 0;
        bool same = true;
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
                worst_moved_ms = moved_ms > worst_moved_ms ? moved_ms : worst_moved_ms;
                measured += CpuSeconds() - run_until;
                sum_ms += ms;
                worst_ms = ms > worst_ms ? ms : worst_ms;
                entries += static_cast<double>(c.Intel().entries.size());
                ++samples;
            }
            cpu_on += CpuSeconds() - t0 - measured;
            same = same && JournalHash(off) == JournalHash(c) && off.Packages().size() == c.Packages().size();
        }
        const double mean_ms = samples > 0 ? sum_ms / samples : 0.0;
        std::printf("%-12s %8zu %8.0f %10.4f %10.4f %10.4f %10.1f %12.2f %12.2f %8s\n", faction.c_str(), sector_count,
                    samples > 0 ? entries / samples : 0.0, mean_ms, worst_ms, worst_moved_ms, prewar / seeds, cpu_off, cpu_on, same ? "yes" : "NO");
        if (worst_ms > budget_ms || worst_moved_ms > budget_ms || !same) ++fails;
    }
    std::printf("\nentries: the enemy's picture at a day's end (mean); mean_ms, worst_ms: a measure with the bands kept (mean, worst day);\n"
                "moved_ms: one with every sector's band moved (worst day); prewar: the entries the sectors' baselines keep; cpu_*: the\n"
                "campaigns' CPU seconds, air_front.mode 0 and 1, the timing repetitions left out; same: the same journal and packages.\n");
    return fails > 0 ? 2 : 0;
}
