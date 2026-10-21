// helper_coverage: every mission of the missions menu, played headless from every seat by a player who only does what the
// helper shows (HelpFollower), against the same mission with an NPC in that seat (docs/crew/13_HELPER.md). Where the
// follower does worse than the NPC, the helper doesn't yet cover something that seat's NPC does: the "Gaps" list.
// Also lists every NPC rule the helper passed on (each duty's rule) and how often.
//
//   helper_coverage <data dir> [out.md|-] [max_s=900] [mission id[:seat] ...] [--trace] [--debrief]
//
// --trace prints, as they happen, every move the follower makes, every move refused (and why) and every duty owed: the
// way to see why a seat falls behind its NPC. --debrief prints each run's debrief.
//
// Writes docs/crew/generated/helper_coverage.md when given that path. Re-run after changing the crew's behaviour, the
// consoles or the missions; a new gap is a rule the NPC follows that doesn't reach the helper (ScenarioRunner::NpcDoes).
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/battery_link.hpp"
#include "simcore/helper.hpp"
#include "simcore/missions.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

using namespace simcore;

namespace {

struct Outcome {
    std::string seat;  // "" the NPC crew
    double end_s = 0.0;
    std::string end;
    std::map<std::string, double> tasks;  // first time each task of the crew data held
    int launches = 0;
    int kills = 0;
    int designations = 0;
    int plots = 0;
    int reports = 0;   // reports the seat's player made
    int orders = 0;    // orders given (any)
    int deviations = 0;
    int faults = 0;
    int moves = 0;
    int refusals = 0;
    std::string last_refusal;
    std::map<std::string, int> by_source;
};

const char* const kTasks[] = {"ready_halted", "captured", "launched"};

struct Run {
    std::unique_ptr<ScenarioRunner> single;
    std::unique_ptr<BatteryLink> link;
    ScenarioRunner* own = nullptr;
    double Time() const { return link ? link->Time() : single->Time(); }
    bool Finished() const { return link ? link->Finished() : single->Finished(); }
    void Step(double dt) {
        if (link) {
            link->Step(dt);
        } else {
            single->Step(dt);
        }
    }
};

bool Start(const std::string& data, const MissionDef& m, const MissionVehicle& v, Run& run, std::string& error) {
    if (m.link) {
        run.link = LoadBatteryLinkFile(data + "/" + m.file, data, &error);
        if (!run.link) return false;
        run.own = v.id == "post" ? &run.link->Post() : &run.link->Osa();
        return true;
    }
    ScenarioDef def;
    ConsoleData console;
    if (!LoadScenarioFile(data + "/" + v.scenario_file, data, def, console, &error)) return false;
    run.single = std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
    run.own = run.single.get();
    return true;
}

int Kills(const ScenarioRunner& r) {
    int n = 0;
    for (const ScenarioTarget& t : r.Def().targets) {
        if (!t.friendly && !r.TargetAlive(t.id)) ++n;
    }
    return n;
}

bool g_trace = false;
bool g_debrief = false;

Outcome Play(const std::string& data, const MissionDef& m, const MissionVehicle& v, const std::string& seat, double max_s,
             std::map<std::string, int>& rules, std::map<std::string, int>& unorderable, std::string& error) {
    Outcome o;
    o.seat = seat;
    Run run;
    if (!Start(data, m, v, run, error)) return o;
    ScenarioRunner& r = *run.own;
    std::unique_ptr<HelpFollower> follower;
    if (!seat.empty()) {
        r.TakeSeat(seat, "Player");
        r.SetAssistLevel(AssistLevel::Guided);
        r.SetHelper(true);
        follower = std::make_unique<HelpFollower>(seat);
    }
    std::set<std::string> seen;  // duties noted once each
    const double limit = r.Def().time_limit_s > 0.0 ? std::min(max_s, r.Def().time_limit_s + 1.0) : max_s;
    while (!run.Finished() && run.Time() < limit) {
        run.Step(0.1);
        const int refused = follower ? follower->Refusals() : 0;
        const HelpStep* moved = follower ? follower->Update(r) : nullptr;
        if (g_trace && moved != nullptr) {
            std::printf("  %7.1f move    %-6s %s  (%s)\n", r.Time(), moved->source.c_str(), moved->text.c_str(), moved->rule.c_str());
        }
        if (g_trace && follower && follower->Refusals() > refused) std::printf("  %7.1f refused %s\n", r.Time(), follower->LastRefusal().c_str());
        for (const SeatDuty& d : r.AllOwedDuties()) {
            const std::string key = d.Key() + "@" + std::to_string(static_cast<long long>(d.since_s * 10.0));
            if (seen.insert(key).second) {
                ++rules[std::string(ToString(d.kind)) + " (" + d.seat + "): " + d.rule];
                if (g_trace) {
                    std::printf("  %7.1f owes    %s %s %s (%s)\n", r.Time(), ToString(d.kind), d.id.c_str(), d.target.c_str(), d.rule.c_str());
                }
            }
        }
        if (r.Def().has_crew_data) {
            for (const char* t : kTasks) {
                const CrewTask* task = r.Def().crew_data.FindTask(t);
                if (task != nullptr && o.tasks.count(t) == 0 && r.Console().equipment.AllHold(task->conditions, r.Console().panel)) {
                    o.tasks[t] = r.Time();
                }
            }
        }
    }
    for (const auto& kv : r.ShadowUnorderable()) ++unorderable[kv.first];
    if (g_debrief) std::printf("%s\n", r.Debrief().c_str());
    o.end_s = run.Time();
    o.end = r.Finished() ? r.EndReason() : (run.Finished() ? "ended" : "time");
    o.launches = r.Launches();
    o.kills = Kills(run.link ? run.link->Osa() : r);
    o.designations = r.DesignationsMade();
    o.plots = r.PlotsMade();
    for (const ScenarioRunner::MadeReport& mr : r.ReportsMade()) o.reports += mr.station == seat ? 1 : 0;
    o.orders = static_cast<int>(r.OrdersGiven().size());
    const DebriefSummary sum = r.Log().Summary();
    o.deviations = sum.out_of_order_actions;
    o.faults = sum.equipment_faults;
    if (follower) {
        o.moves = follower->Moves();
        o.refusals = follower->Refusals();
        o.last_refusal = follower->LastRefusal();
        o.by_source = follower->BySource();
    }
    return o;
}

std::string Time(const std::map<std::string, double>& tasks, const char* t) {
    const auto it = tasks.find(t);
    if (it == tasks.end()) return "-";
    char b[16];
    std::snprintf(b, sizeof(b), "%.0f", it->second);
    return b;
}

std::string Sources(const std::map<std::string, int>& s) {
    std::string out;
    for (const auto& kv : s) out += (out.empty() ? "" : ", ") + kv.first + " " + std::to_string(kv.second);
    return out.empty() ? "-" : out;
}

std::string Cell(std::string s) {
    for (char& c : s) {
        if (c == '|' || c == '\n') c = '/';
    }
    return s;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: helper_coverage <data dir> [out.md] [max_s=900] [mission id ...]\n");
        return 2;
    }
    const std::string data = argv[1];
    const std::string out_path = argc > 2 ? argv[2] : std::string();
    const double max_s = argc > 3 ? std::atof(argv[3]) : 900.0;
    std::set<std::string> only, only_seat;
    for (int i = 4; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--trace") {
            g_trace = true;
            continue;
        }
        if (a == "--debrief") {
            g_debrief = true;
            continue;
        }
        const std::size_t colon = a.find(':');
        only.insert(a.substr(0, colon));
        if (colon != std::string::npos) only_seat.insert(a);
    }
    MissionCatalog cat;
    std::string e;
    if (!LoadMissionCatalog(data, cat, &e)) {
        std::printf("missions: %s\n", e.c_str());
        return 1;
    }
    std::ostringstream doc, gaps;
    std::map<std::string, int> rules, unorderable;
    int runs = 0, gap_count = 0;
    doc << "# The helper's coverage (generated)\n\n"
        << "Written by `simcore/build/helper_coverage data docs/crew/generated/helper_coverage.md` (docs/crew/13_HELPER.md). "
        << "Every mission of the missions menu, played headless from each seat by a player who does only what the helper "
        << "shows (`HelpFollower`), against the same mission with the NPC crew (the first row of each table). Times in "
        << "seconds of simulation; `-` never. A row that does worse than the NPC row is listed under Gaps.\n\n";
    for (const MissionGroup& g : cat.groups) {
        for (const MissionDef& m : g.missions) {
            if (!only.empty() && only.count(m.id) == 0) continue;
            for (const MissionVehicle& v : m.vehicles) {
                doc << "## " << m.id << (v.id.empty() ? "" : " (" + v.id + ")") << ": " << m.title << "\n\n"
                    << "| Seat | End | Ready | Captured | Launched | Launches | Kills | Designations | Plots | Reports | Orders "
                       "| Out of order | Faults | Helper moves | Refused |\n"
                    << "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n";
                std::vector<std::string> seats = {""};
                for (const MissionSeat& s : v.seats) seats.push_back(s.id);
                Outcome npc;
                for (const std::string& seat : seats) {
                    if (!only_seat.empty() && !seat.empty() && only_seat.count(m.id + ":" + seat) == 0) continue;
                    if (g_trace) std::printf("%s %s %s\n", m.id.c_str(), v.id.c_str(), seat.empty() ? "(NPC crew)" : seat.c_str());
                    std::string err;
                    const Outcome o = Play(data, m, v, seat, max_s, rules, unorderable, err);
                    ++runs;
                    if (!err.empty()) {
                        doc << "| " << (seat.empty() ? "NPC crew" : seat) << " | could not start: " << Cell(err) << " |\n";
                        continue;
                    }
                    if (seat.empty()) npc = o;
                    char row[512];
                    std::snprintf(row, sizeof(row), "| %s | %.0f %s | %s | %s | %s | %d | %d | %d | %d | %d | %d | %d | %d | %s | %d |\n",
                                  seat.empty() ? "NPC crew" : seat.c_str(), o.end_s, Cell(o.end).c_str(), Time(o.tasks, "ready_halted").c_str(),
                                  Time(o.tasks, "captured").c_str(), Time(o.tasks, "launched").c_str(), o.launches, o.kills,
                                  o.designations, o.plots, o.reports, o.orders, o.deviations, o.faults,
                                  seat.empty() ? "-" : Sources(o.by_source).c_str(), o.refusals);
                    doc << row;
                    if (seat.empty()) continue;
                    std::vector<std::string> worse;
                    for (const char* t : kTasks) {
                        if (npc.tasks.count(t) != 0 && o.tasks.count(t) == 0) worse.push_back(std::string(t) + " never reached");
                        if (npc.tasks.count(t) != 0 && o.tasks.count(t) != 0 && o.tasks.at(t) > npc.tasks.at(t) + 60.0) {
                            worse.push_back(std::string(t) + " a minute or more later");
                        }
                    }
                    if (o.kills < npc.kills) worse.push_back("fewer kills (" + std::to_string(o.kills) + " vs " + std::to_string(npc.kills) + ")");
                    if (o.launches == 0 && npc.launches > 0) worse.push_back("no launch");
                    if (o.designations < npc.designations / 2) worse.push_back("fewer designations");
                    if (o.faults > npc.faults) worse.push_back("equipment faults");
                    if (!worse.empty()) {
                        ++gap_count;
                        gaps << "- **" << m.id << (v.id.empty() ? "" : " (" + v.id + ")") << ", " << seat << "**: ";
                        for (std::size_t i = 0; i < worse.size(); ++i) gaps << (i ? "; " : "") << worse[i];
                        if (!o.last_refusal.empty()) gaps << ". Last refused: " << Cell(o.last_refusal);
                        gaps << "\n";
                    }
                    std::printf("%s %s %s: %d moves%s\n", m.id.c_str(), v.id.c_str(), seat.c_str(), o.moves, worse.empty() ? "" : " GAP");
                    std::fflush(stdout);
                }
                doc << "\n";
            }
        }
    }
    doc << "## Gaps (" << gap_count << ")\n\n" << (gap_count == 0 ? std::string("None.\n") : gaps.str()) << "\n";
    doc << "## The NPC rules the helper passed on\n\n| Duty (seat): rule | Times |\n|---|---|\n";
    for (const auto& kv : rules) doc << "| " << Cell(kv.first) << " | " << kv.second << " |\n";
    doc << "\n## Drills the NPC commander hands out that no order of the crew data starts\n\n"
        << "A player in command can't give these from his menu (the shadow commander would): an order missing in "
        << "`data/crew/<system>_crew.json`.\n\n";
    if (unorderable.empty()) doc << "None.\n";
    for (const auto& kv : unorderable) doc << "- " << kv.first << " (" << kv.second << " runs)\n";
    doc << "\n" << runs << " runs.\n";
    if (!out_path.empty() && out_path != "-") {
        std::ofstream f(out_path, std::ios::binary);
        f << doc.str();
    } else {
        std::printf("%s", doc.str().c_str());
    }
    std::printf("%d runs, %d gaps\n", runs, gap_count);
    return 0;
}
