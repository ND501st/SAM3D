// air_war_balance: the enemy air command over many seeds, for the balance pass (roadmap A12, docs/air_war/09_BALANCE.md).
//
// Usage: air_war_balance [--theatre file] [data_dir] [seeds] [days] [faction ...]
//   --theatre: the theatre under data_dir/air_war/ (default theatre_pannonia.json, the one the balance pass tuned;
//   theatre_pannonia_nation.json is the whole nation); data_dir (default data); seeds (default 10); days (default 6);
//   factions (default all four: rebels, neutral, warsaw_pact, nato).
//
// Every campaign runs headless: the raids through the player's battery's sector are resolved abstractly like the rest
// (SkipPlayerMission), with the battery's own lethality, so the numbers are the campaign layer's alone. Each faction is
// run with two battery postures: a careless commander (radars radiating half of every hour, no dummy traffic, never
// moving) and a careful one (the demo's: radars mostly silent, ARM discipline, dummy traffic, a move every 20 hours).
// For each, prints a table by day, averaged over the seeds:
//   packages, sorties (manned aircraft; decoy drones are counted apart), SEAD share (DEAD and hunter packages, and SEAD flights in others), aircraft lost,
//   loss rate per sortie, the player's side's air defences still working (the radar SAM sites the enemy measures, the
//   decoys left out) and what the enemy believes of them, objects wrecked, raids through the battery's sector, the battery's state and how well the enemy
//   knows where it is,
// then totals: the mission mix, the staff's turnover, false claims found out, political events and pauses, and where the
// packages went: the kinds of target and the objects struck most.
// Exits non-zero if the data fail to load.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

struct DayStats {
    double packages = 0.0;
    double sorties = 0.0;
    double sead = 0.0;  // packages with a SEAD purpose
    double lost = 0.0;
    double iads = 0.0;  // working share of the true radar SAM sites the enemy measures, at the day's end
    double believed = 0.0;  // the enemy's belief of the same (IntelPicture::BelievedIadsShare)
    double phase = 0.0;     // the doctrine phase at the day's end (0 the first)
    double wrecked = 0.0;
    double battery_raids = 0.0;
    double battery = 0.0;    // the battery's working share at the day's end
    double knowledge = 0.0;  // EnemyKnowledgeOfBattery at the day's end
};

struct RunStats {
    std::vector<DayStats> days;
    double kinds[static_cast<int>(MissionKind::Count)] = {};
    double staff_changes = 0.0;
    double false_claims = 0.0;
    double events = 0.0;  // political events that fired
    double pauses = 0.0;  // of them, pauses in the bombing
    double battery_wrecked = 0.0;  // runs in which the battery was wrecked at least once
    double arms_left = 0.0;
    double drones = 0.0;  // decoy drones flown
    double missiles = 0.0;  // cruise missiles fired
    double missiles_downed = 0.0;
    std::map<std::string, double> struck;       // packages by target object (ghosts as "(nothing there)")
    std::map<std::string, double> struck_kind;  // packages by the target's kind as the enemy believed it
};

void Posture(BatteryPosture& p, bool careful) {
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

bool Contains(const std::string& s, const char* what) { return s.find(what) != std::string::npos; }

// The truth of what the enemy measures (IntelPicture::BelievedIadsShare): the radar SAM sites.
double IadsShare(const AirCampaign& c) {
    double sum = 0.0;
    int n = 0;
    const std::vector<TheatreObject>& objs = c.Data().theatre.objects;
    for (std::size_t i = 0; i < objs.size() && i < c.Truth().size(); ++i) {
        if (objs[i].decoy || objs[i].kind != ObjectKind::SamSite || !objs[i].defence.present || !objs[i].defence.radar) continue;
        sum += c.Truth()[i].function;
        ++n;
    }
    return n > 0 ? sum / n : 1.0;
}

int Wrecked(const AirCampaign& c) {
    int n = 0;
    const std::vector<TheatreObject>& objs = c.Data().theatre.objects;
    for (std::size_t i = 0; i < objs.size() && i < c.Truth().size(); ++i) {
        if (!objs[i].decoy && c.Truth()[i].function <= 0.0) ++n;
    }
    return n;
}

bool SeadPurpose(const Package& p) {
    return p.kind == MissionKind::Dead || p.kind == MissionKind::Hunter || p.Has(FlightRole::Sead);
}

// One campaign; adds its numbers into `acc` (sums, divided by the seeds at the end).
bool Run(const AirWarData& data, unsigned seed, int days, bool careful, RunStats& acc) {
    AirCampaign c;
    std::string error;
    if (!c.Init(data, seed, &error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return false;
    }
    Posture(c.Posture(), careful);
    if (acc.days.size() < static_cast<std::size_t>(days)) acc.days.resize(static_cast<std::size_t>(days));
    bool battery_wrecked = false;
    for (int d = 0; d < days; ++d) {
        const double end = 24.0 * (d + 1);
        int raids = 0;
        while (c.Now() < end - 1e-9) {
            if (c.Advance(end)) {
                ++raids;
                c.SkipPlayerMission();
            }
            if (c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].function <= 0.0) battery_wrecked = true;
        }
        DayStats& s = acc.days[static_cast<std::size_t>(d)];
        s.battery_raids += raids;
        s.iads += IadsShare(c);
        s.believed += c.Intel().BelievedIadsShare();
        s.phase += c.Phase();
        s.wrecked += Wrecked(c);
        s.battery += c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].function;
        s.knowledge += c.EnemyKnowledgeOfBattery();
    }
    for (const Package& p : c.Packages()) {
        if (!p.resolved && !p.played) continue;  // planned beyond the last day
        const int d = static_cast<int>(p.tot_h / 24.0);
        if (d < 0 || d >= days) continue;
        DayStats& s = acc.days[static_cast<std::size_t>(d)];
        s.packages += 1.0;
        s.sead += SeadPurpose(p) ? 1.0 : 0.0;
        // Manned aircraft only: decoy drones are meant to be shot down (the campaign doesn't count them either).
        for (const Flight& f : p.flights) {
            const AircraftType* a = data.faction.Aircraft(f.aircraft);
            if (a != nullptr && a->drone) {
                acc.drones += f.count;
                continue;
            }
            if (a != nullptr && a->missile) {
                acc.missiles += f.count;
                acc.missiles_downed += f.lost;
                continue;
            }
            s.sorties += f.count;
            s.lost += f.lost;
        }
        acc.kinds[static_cast<int>(p.kind)] += 1.0;
        acc.struck[p.target_object.empty() ? std::string("(nothing there)") : p.target_object] += 1.0;
        const IntelEntry* e = c.Intel().Find(p.target_key);
        acc.struck_kind[e != nullptr ? ObjectKindName(e->kind) : "?"] += 1.0;
    }
    for (const JournalEntry& j : c.Journal()) {
        if (j.kind == "career" && (Contains(j.text, " was killed") || Contains(j.text, " was captured") ||
                                   Contains(j.text, "has been relieved") || Contains(j.text, "promoted out"))) {
            acc.staff_changes += 1.0;
        }
        if (j.kind == "decision" && Contains(j.text, "claimed destroyed is working again")) acc.false_claims += 1.0;
    }
    for (const PoliticalEventDef& e : data.faction.politics.events) {
        if (c.Politics().fired_events.count(e.id) == 0) continue;
        acc.events += 1.0;
        if (e.pause_h > 0.0) acc.pauses += 1.0;
    }
    acc.battery_wrecked += battery_wrecked ? 1.0 : 0.0;
    acc.arms_left += c.ArmStock();
    return true;
}

void Print(const std::string& faction, const char* posture, const RunStats& r, int seeds) {
    const double n = seeds;
    std::printf("\n**%s, %s battery** (%d seeds):\n\n", faction.c_str(), posture, seeds);
    std::printf("| Day | Phase | Packages | Sorties | SEAD share | Lost | Loss %% | Air defence working (believed) | Wrecked | Battery raids | Battery | Enemy knows battery |\n");
    std::printf("|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    double pk = 0, so = 0, lo = 0, br = 0;
    for (std::size_t d = 0; d < r.days.size(); ++d) {
        const DayStats& s = r.days[d];
        pk += s.packages;
        so += s.sorties;
        lo += s.lost;
        br += s.battery_raids;
        std::printf("| D%d | %.1f | %.1f | %.1f | %.0f%% | %.1f | %.1f%% | %.0f%% (%.0f%%) | %.1f | %.1f | %.0f%% | %.2f |\n", static_cast<int>(d) + 1,
                    s.phase / n + 1.0, s.packages / n, s.sorties / n, s.packages > 0 ? 100.0 * s.sead / s.packages : 0.0, s.lost / n,
                    s.sorties > 0 ? 100.0 * s.lost / s.sorties : 0.0, 100.0 * s.iads / n, 100.0 * s.believed / n, s.wrecked / n, s.battery_raids / n,
                    100.0 * s.battery / n, s.knowledge / n);
    }
    std::printf("| all | | %.1f | %.1f | | %.1f | %.1f%% | | | %.1f | | |\n\n", pk / n, so / n, lo / n, so > 0 ? 100.0 * lo / so : 0.0, br / n);
    std::printf("Mix:");
    double total = 0.0;
    for (double k : r.kinds) total += k;
    for (int k = 0; k < static_cast<int>(MissionKind::Count); ++k) {
        if (r.kinds[k] > 0.0) std::printf(" %s %.0f%%", MissionKindName(static_cast<MissionKind>(k)), 100.0 * r.kinds[k] / total);
    }
    std::printf(". Staff changes %.1f, false claims found out %.1f, political events %.1f (pauses %.1f) a campaign; battery wrecked in %.0f%% of runs; ARMs left %.0f; decoy drones %.0f.\n",
                r.staff_changes / n, r.false_claims / n, r.events / n, r.pauses / n, 100.0 * r.battery_wrecked / n, r.arms_left / n, r.drones / n);
    if (r.missiles > 0.0) std::printf("Cruise missiles: %.1f fired a campaign, %.1f shot down.\n", r.missiles / n, r.missiles_downed / n);
    const auto top = [&](const std::map<std::string, double>& m, std::size_t count) {
        std::vector<std::pair<double, std::string>> v;
        for (const auto& kv : m) v.push_back({kv.second, kv.first});
        std::sort(v.begin(), v.end(), [](const std::pair<double, std::string>& a, const std::pair<double, std::string>& b) {
            return a.first != b.first ? a.first > b.first : a.second < b.second;
        });
        for (std::size_t i = 0; i < v.size() && i < count; ++i) std::printf(" %s %.1f%s", v[i].second.c_str(), v[i].first / n, i + 1 < count && i + 1 < v.size() ? "," : "");
    };
    std::printf("Targets by kind (packages a campaign):");
    top(r.struck_kind, 12);
    std::printf(".\nMost struck:");
    top(r.struck, 12);
    std::printf(".\n");
}

}  // namespace

int main(int argc, char** argv) {
    std::string theatre = "theatre_pannonia.json";
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--theatre" && i + 1 < argc) {
            theatre = argv[++i];
        } else {
            args.push_back(a);
        }
    }
    const std::string data_dir = args.size() > 0 ? args[0] : "data";
    const int seeds = args.size() > 1 ? std::atoi(args[1].c_str()) : 10;
    const int days = args.size() > 2 ? std::atoi(args[2].c_str()) : 6;
    std::vector<std::string> factions(args.size() > 3 ? args.begin() + 3 : args.end(), args.end());
    if (factions.empty()) factions = {"rebels", "neutral", "warsaw_pact", "nato"};
    std::printf("Theatre: air_war/%s\n", theatre.c_str());

    for (const std::string& faction : factions) {
        AirWarData data;
        std::string error;
        if (!LoadAirWarData(data_dir, "air_war/" + theatre, "air_war/faction_" + faction + ".json", data, &error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        for (int careful = 0; careful < 2; ++careful) {
            RunStats r;
            for (int s = 1; s <= seeds; ++s) {
                if (!Run(data, static_cast<unsigned>(s), days, careful != 0, r)) return 1;
            }
            Print(data.faction.name, careful != 0 ? "careful" : "careless", r, seeds);
        }
    }
    return 0;
}
