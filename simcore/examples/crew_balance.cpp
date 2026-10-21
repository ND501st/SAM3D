// crew_balance: play the crew scenarios headless at every skill tier over many seeds and tabulate how the crew does
// (roadmap 8.12, docs/crew/11_PLAYTEST_AND_BALANCE.md): readiness against the drill master's ideal, detection to launch,
// launches, hits, equipment faults, out-of-order actions, reminders, slips and forgotten steps, launches late against the
// network chart. A balance pass reads these against the published norms and the skill table's intent.
//
//   crew_balance <data dir> [seeds=10] [scenario file ...]
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

using namespace simcore;

namespace {

std::string ReadAll(const std::string& path) {
    std::ifstream f(path);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

// Every crew member at one tier, and the seed.
std::string WithTierAndSeed(std::string text, const std::string& tier, unsigned seed) {
    std::size_t at = 0;
    const std::string key = "\"skill\":";
    while ((at = text.find(key, at)) != std::string::npos) {
        const std::size_t q1 = text.find('"', at + key.size());
        const std::size_t q2 = text.find('"', q1 + 1);
        if (q1 == std::string::npos || q2 == std::string::npos) break;
        text.replace(q1 + 1, q2 - q1 - 1, tier);
        at = q1 + tier.size() + 2;
    }
    const std::size_t s = text.find("\"seed\":");
    if (s != std::string::npos) {
        std::size_t e = s + 7;
        while (e < text.size() && (text[e] == ' ' || (text[e] >= '0' && text[e] <= '9'))) ++e;
        text.replace(s, e - s, "\"seed\": " + std::to_string(seed));
    }
    return text;
}

struct Stats {
    std::vector<double> ready, ideal, react, launches, hits, faults, deviations, reminders, slips, omissions, after, slow;
    int runs = 0;
    int failed = 0;
};

double Mean(const std::vector<double>& v) {
    if (v.empty()) return -1.0;
    double s = 0.0;
    for (double x : v) s += x;
    return s / static_cast<double>(v.size());
}

std::string Cell(const std::vector<double>& v, int decimals = 0) {
    if (v.empty()) return "-";
    char b[48];
    if (decimals == 0) {
        std::snprintf(b, sizeof(b), "%.0f", Mean(v));
    } else {
        std::snprintf(b, sizeof(b), "%.*f", decimals, Mean(v));
    }
    return b;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: crew_balance <data dir> [seeds=10] [scenario file ...]\n");
        return 2;
    }
    const std::string data = argv[1];
    const unsigned seeds = argc > 2 ? static_cast<unsigned>(std::atoi(argv[2])) : 10u;
    std::vector<std::string> scenarios;
    for (int i = 3; i < argc; ++i) scenarios.push_back(argv[i]);
    if (scenarios.empty()) scenarios = {"scenarios/osa_crew_practice.json", "scenarios/osa_battery_fight.json", "scenarios/osa_mission_march.json"};
    const char* tiers[] = {"master", "class1", "class2", "class3", "recruit"};
    for (const std::string& file : scenarios) {
        const std::string text = ReadAll(data + "/" + file);
        std::printf("\n%s (%u seeds per tier)\n\n", file.c_str(), seeds);
        std::printf("| Tier | К бою s (ideal) | First echo to launch s | Launch after the chart's ideal s | Slower than the norm | Launches | Hits | Faults | Out of order | Slips | Forgotten | Reminders |\n");
        std::printf("|---|---|---|---|---|---|---|---|---|---|---|---|\n");
        for (const char* tier : tiers) {
            Stats st;
            for (unsigned seed = 1; seed <= seeds; ++seed) {
                ScenarioDef def;
                ConsoleData console;
                std::string e;
                if (!LoadScenario(WithTierAndSeed(text, tier, seed), data, def, console, &e)) {
                    std::printf("  %s: %s\n", tier, e.c_str());
                    ++st.failed;
                    continue;
                }
                const double limit = def.time_limit_s;
                ScenarioRunner r(std::move(def), std::move(console));
                double seen = -1.0, launched = -1.0;
                while (!r.Finished() && r.Time() < limit + 1.0) {
                    r.Step(0.1);
                    if (seen < 0.0 && !r.Engagement().SearchPicture().empty()) seen = r.Time();
                    if (launched < 0.0 && r.Launches() > 0) launched = r.Time();
                }
                ++st.runs;
                if (r.ActualReadyS() > 0.0) st.ready.push_back(r.ActualReadyS());
                if (r.IdealReadyS() > 0.0) st.ideal.push_back(r.IdealReadyS());
                if (seen >= 0.0 && launched >= 0.0) st.react.push_back(launched - seen);
                const DebriefSummary sum = r.Log().Summary();
                st.launches.push_back(r.Launches());
                st.hits.push_back(r.Engagement().Hits());
                st.faults.push_back(sum.equipment_faults);
                st.deviations.push_back(sum.out_of_order_actions);
                double reminders = 0.0, slips = 0.0, omissions = 0.0;
                for (const CrewMember& m : r.CrewMembers()) {
                    reminders += r.RemindersTo(m.Role());
                    slips += m.Slips();
                    omissions += m.Omissions();
                }
                st.reminders.push_back(reminders);
                st.slips.push_back(slips);
                st.omissions.push_back(omissions);
                double slow = 0.0;
                for (const ScenarioRunner::DecisionRecord& d : r.Decisions()) {
                    if (d.kind != "launch" || d.ideal_s < 0.0) continue;
                    st.after.push_back(d.late_s);
                    slow += d.good ? 0.0 : 1.0;
                }
                st.slow.push_back(slow);
            }
            const std::string ready = st.ready.empty() ? "-" : Cell(st.ready) + (st.ideal.empty() ? "" : " (" + Cell(st.ideal) + ")");
            std::printf("| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n", tier, ready.c_str(), Cell(st.react).c_str(),
                        Cell(st.after).c_str(), Cell(st.slow, 1).c_str(), Cell(st.launches, 1).c_str(), Cell(st.hits, 1).c_str(),
                        Cell(st.faults, 1).c_str(), Cell(st.deviations, 1).c_str(), Cell(st.slips, 1).c_str(), Cell(st.omissions, 1).c_str(),
                        Cell(st.reminders, 1).c_str());
            if (st.failed > 0) std::printf("  (%d runs failed to load)\n", st.failed);
        }
    }
    return 0;
}
