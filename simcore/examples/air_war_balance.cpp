// air_war_balance: the enemy air command over many seeds, for the balance pass (roadmap A12, docs/air_war/09_BALANCE.md).
//
// Usage: air_war_balance [--theatre file] [--set tuning.key=value ...] [--debug] [--cpu] [data_dir] [seeds] [days] [faction ...]
//   --theatre: the theatre under data_dir/air_war/ (default theatre_pannonia.json, the one the balance pass tuned;
//   theatre_pannonia_nation.json is the whole nation); --set: try a tuning value (a calibration sweep); data_dir
//   (default data); seeds (default 10); days (default 6); factions (default all four: rebels, neutral, warsaw_pact, nato).
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
// then totals: the mission mix, the staff's turnover, false claims found out, political events and pauses, where the
// packages went (the kinds of target and the objects struck most), how the attacks were flown (the deliveries and
// weapons of the attacking flights, the strike groups' sizes), the support orbits and the force build-up, and our supply
// lines (docs/air_war/10_WEAPONS.md, 11_SUPPORT.md, 12_FORCES.md, 13_PACKAGES.md, 14_LOGISTICS.md).
// Since roadmap B14 also: who shot the enemy's aircraft down (our fighters, the player's battery, the other defences by
// kind and by object) and on which missions, beside the losses the planners expected; the strikers' answers when our
// fighters reached them; the on-call and divert latencies; and each measure that data/air_war/balance_bands.json gives a
// band for this theatre, faction and posture, checked against it (--debug writes each loss to stderr).
// --cpu (track F, F3.3): after each posture, the CPU its campaigns took (std::clock) and, under ground.mode 1 on the
// nation (--set ground.mode=1), the line of contact's depth by sector at each day's end, the mean over the seeds; without
// it the output is as before, byte for byte.
// Exits 1 if the data fail to load, 2 if a measure is out of its band.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

bool g_debug = false;  // --debug: each loss to stderr
bool g_cpu = false;    // --cpu: the CPU a posture's campaigns took, and the ground war's line
// The bands (roadmap B14, data/air_war/balance_bands.json) and what Print checks them for.
BalanceBands g_bands;
std::string g_theatre, g_faction;
int g_fails = 0;

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
    std::map<std::string, std::vector<double>> ground_km;  // --cpu: the line's depth by sector at each day's end, summed
    GroundAirStats ground_air;  // --cpu with the ground war: the air war and its units (F3.7), summed over the runs
    double overrun_end = 0.0;   // our objects overrun at the campaign's end, summed
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
    std::map<std::string, double> deliveries;   // attacking flights by their delivery
    std::map<std::string, double> weapons;      // attacking flights by their weapon
    std::map<std::string, std::pair<double, double>> group_by_kind;  // target kind -> (strike packages, their attackers)
    double strike_packages = 0.0;  // strike, DEAD and CAS packages
    double strike_aircraft = 0.0;  // their attacking aircraft
    double big_packages = 0.0;     // of them, with 8 or more attackers
    double refuelled = 0.0;        // aircraft refuelled to reach their targets
    double awacs = 0.0;            // AWACS on station, the last plan's
    double support_sorties = 0.0;
    double aircraft_day1 = 0.0;    // manned aircraft in theatre on day 1
    double aircraft_end = 0.0;     // and at the end (after losses, replacements and the build-up)
    // Our supply lines (logistics.hpp): what they brought and lost, how long they were cut, the troops' stocks at the end.
    double lines = 0.0, delivered = 0.0, strength = 0.0, cargo_lost = 0.0, vehicles_lost = 0.0, columns_destroyed = 0.0;
    double hours_cut = 0.0, reroutes = 0.0, stocks_end = 0.0, armed_recce = 0.0, columns_found = 0.0;
    // Command in the air (roadmap B5, c2.hpp): reports, diverts, alert pools, the calls our listeners heard.
    double c2_reports = 0.0, c2_out_of_cover = 0.0, c2_diverts = 0.0, c2_warnings = 0.0, c2_scrambles = 0.0, c2_on_call = 0.0;
    double c2_on_call_sorties = 0.0, c2_pool_aircraft = 0.0, c2_pools = 0.0, c2_stood_down = 0.0, c2_heard = 0.0, c2_refusals = 0.0;
    double c2_briefs = 0.0, c2_requests = 0.0, c2_to_intel = 0.0, c2_on_call_sorties_d1 = 0.0, c2_sorties_d1 = 0.0;
    // The command live (roadmap S2): recalls, posts struck and set up again, traffic to an alternate or lost with its post,
    // hops our jamming took, close support held and sent home for want of its FAC's clearance.
    double c2_recalls = 0.0, c2_posts_struck = 0.0, c2_posts_back = 0.0, c2_to_alternate = 0.0, c2_lost_with_post = 0.0;
    double c2_jammed_by_us = 0.0, c2_holds = 0.0, c2_clearance_aborts = 0.0;
    // The air battle (roadmap B8, air_fight.hpp): fights, the enemy's aircraft our fighters shot down, ours lost.
    double air_fights = 0.0, air_enemy_lost = 0.0, air_ours_lost = 0.0;
    // The latencies (roadmap B14): minutes from a pilot's sighting to an on-call package over the place, and to a diverted
    // flight having its new task.
    double on_call_min = 0.0, on_call_n = 0.0, divert_min = 0.0, divert_n = 0.0;
    // Who shot the enemy's manned aircraft down and on what missions (roadmap B14, the campaign's outcome observer).
    std::map<std::string, double> lost_by;         // the defence's kind ("our fighters", "the player's battery", an object kind)
    std::map<std::string, double> lost_by_object;  // the defence's object id
    std::map<std::string, double> lost_mission;    // the package's mission kind
    std::map<std::string, std::pair<double, double>> expect_mission;  // mission kind -> (packages, their planned losses)
    // The strikers' answer when our fighters reached them (roadmap B14 against red_baron_vol2 and aces_1976).
    double fight_attacked = 0.0, fight_jettison = 0.0, fight_abort = 0.0;
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
    acc.aircraft_day1 += c.AircraftInTheatre();
    {
        const std::vector<TheatreObject>& objs = data.theatre.objects;
        const std::string player = objs[static_cast<std::size_t>(c.PlayerIndex())].id;
        c.ObserveOutcomes([&acc, &objs, &data, player](const Package& p, const PackageOutcome& o) {
            for (const LossRecord& l : o.losses) {
                if (l.flight < 0 || static_cast<std::size_t>(l.flight) >= p.flights.size()) continue;
                const AircraftType* a = data.faction.Aircraft(p.flights[static_cast<std::size_t>(l.flight)].aircraft);
                if (a != nullptr && (a->drone || a->missile)) continue;
                std::string kind = "(unknown)";
                if (l.fighter) {
                    kind = "our fighters";
                } else if (l.own_fire) {
                    kind = "their own air defence";  // S1: its own belt (NATO's, the Pact's), not one of our objects
                } else if (l.by == player) {
                    kind = "the player's battery";
                } else {
                    for (const TheatreObject& ob : objs) {
                        if (ob.id == l.by) kind = ObjectKindName(ob.kind);
                    }
                }
                acc.lost_by[kind] += 1.0;
                acc.lost_by_object[l.by.empty() ? std::string("(unknown)") : l.by] += 1.0;
                acc.lost_mission[MissionKindName(p.kind)] += 1.0;
                if (g_debug) {
                    const Flight& lf = p.flights[static_cast<std::size_t>(l.flight)];
                    std::fprintf(stderr, "loss %s target %s (%s) by %s role %s profile %s exp %.3f\n", MissionKindName(p.kind), p.target_object.c_str(),
                                 p.target_key.c_str(), l.by.c_str(), FlightRoleName(lf.role), ProfileName(lf.profile), p.expected_losses);
                }
            }
            std::pair<double, double>& em = acc.expect_mission[MissionKindName(p.kind)];
            em.first += 1.0;
            em.second += p.expected_losses;
            acc.fight_attacked += o.fight_attacked_flights;
            acc.fight_jettison += o.fight_jettison_drawn;
            acc.fight_abort += o.fight_aborted;
        });
    }
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
        if (d == 0) acc.c2_on_call_sorties_d1 += c.C2().Stats().on_call_sorties;
        s.battery_raids += raids;
        s.iads += IadsShare(c);
        s.believed += c.Intel().BelievedIadsShare();
        s.phase += c.Phase();
        s.wrecked += Wrecked(c);
        s.battery += c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].function;
        s.knowledge += c.EnemyKnowledgeOfBattery();
        if (g_cpu && c.Ground() != nullptr) {
            for (std::size_t k = 0; k < c.FrontLine().SectorCount(); ++k) {
                std::vector<double>& v = acc.ground_km[c.FrontLine().Sector(k).id];
                if (v.size() < static_cast<std::size_t>(days)) v.resize(static_cast<std::size_t>(days), 0.0);
                v[static_cast<std::size_t>(d)] += groundwar::GroundWar::SectorDepth(c.FrontLine(), k) / 1000.0;
            }
        }
    }
    if (g_cpu && c.Ground() != nullptr) {
        const GroundAirStats& g = c.GroundAir();
        GroundAirStats& a = acc.ground_air;
        a.troop_strikes += g.troop_strikes;
        a.strength_lost += g.strength_lost;
        a.reinforced += g.reinforced;
        a.strength_brought += g.strength_brought;
        a.overrun += g.overrun;
        a.retaken += g.retaken;
        a.escapes += g.escapes;
        a.troop_moves += g.troop_moves;
        a.troops_moved_km += g.troops_moved_km;
        a.supply_reroutes += g.supply_reroutes;
        a.cas += g.cas;
        a.bai += g.bai;
        a.ai += g.ai;
        a.bai_fixed += g.bai_fixed;
        a.ai_fixed += g.ai_fixed;
        for (const ObjectState& st : c.Truth()) acc.overrun_end += st.overrun ? 1.0 : 0.0;
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
        acc.refuelled += p.refuelled;
        const IntelEntry* e = c.Intel().Find(p.target_key);
        if (p.kind == MissionKind::Strike || p.kind == MissionKind::Dead || p.kind == MissionKind::Cas) {
            int attackers = 0;
            for (const Flight& f : p.flights) {
                if (f.role != FlightRole::Strike && f.role != FlightRole::Dead) continue;
                attackers += f.count;
                acc.deliveries[f.delivery == Delivery::Count ? std::string("(none)") : DeliveryName(f.delivery)] += 1.0;
                acc.weapons[f.weapon.empty() ? std::string("(strike power)") : f.weapon] += 1.0;
            }
            acc.strike_packages += 1.0;
            acc.strike_aircraft += attackers;
            acc.big_packages += attackers >= 8 ? 1.0 : 0.0;
            std::pair<double, double>& g = acc.group_by_kind[e != nullptr ? ObjectKindName(e->kind) : "?"];
            g.first += 1.0;
            g.second += attackers;
        }
        acc.struck[p.target_object.empty() ? std::string("(nothing there)") : p.target_object] += 1.0;
        acc.struck_kind[e != nullptr ? ObjectKindName(e->kind) : "?"] += 1.0;
    }
    for (const JournalEntry& j : c.Journal()) {
        if (j.kind == "career" && (Contains(j.text, " was killed") || Contains(j.text, " was captured") ||
                                   Contains(j.text, "has been relieved") || Contains(j.text, "promoted out"))) {
            acc.staff_changes += 1.0;
        }
        if (j.kind == "decision" && Contains(j.text, "claimed destroyed is working again")) acc.false_claims += 1.0;
        // The resolver's note on an air battle: "our fighters (N) found it ...: A of its aircraft and B of ours shot down".
        const std::size_t af = j.text.find("our fighters (");
        if (j.kind == "result" && af != std::string::npos) {
            const std::size_t colon = j.text.find(": ", af);
            const std::size_t mid = colon == std::string::npos ? std::string::npos : j.text.find(" of its aircraft and ", colon);
            if (mid != std::string::npos) {
                acc.air_fights += 1.0;
                acc.air_enemy_lost += static_cast<double>(std::strtol(j.text.c_str() + colon + 2, nullptr, 10));
                acc.air_ours_lost += static_cast<double>(std::strtol(j.text.c_str() + mid + 21, nullptr, 10));
            }
        }
    }
    for (const PoliticalEventDef& e : data.faction.politics.events) {
        if (c.Politics().fired_events.count(e.id) == 0) continue;
        acc.events += 1.0;
        if (e.pause_h > 0.0) acc.pauses += 1.0;
    }
    acc.battery_wrecked += battery_wrecked ? 1.0 : 0.0;
    acc.arms_left += c.ArmStock();
    acc.awacs += c.Support().awacs;
    acc.support_sorties += c.SupportSorties();
    acc.aircraft_end += c.AircraftInTheatre();
    const SupplyLedger& led = c.Supply().Ledger();
    acc.lines = static_cast<double>(c.Supply().Lines());
    acc.delivered += led.delivered;
    acc.strength += led.strength;
    acc.cargo_lost += led.cargo_lost;
    acc.vehicles_lost += led.vehicles_lost;
    acc.columns_destroyed += led.columns_destroyed;
    acc.hours_cut += led.line_hours_cut;
    acc.reroutes += led.reroutes;
    double stocks = 0.0;
    for (int j : c.Supply().Served()) stocks += c.Truth()[static_cast<std::size_t>(j)].supply;
    acc.stocks_end += c.Supply().Served().empty() ? 1.0 : stocks / static_cast<double>(c.Supply().Served().size());
    {
        const C2Stats& cs = c.C2().Stats();
        acc.c2_reports += cs.reports;
        acc.c2_out_of_cover += cs.out_of_cover;
        acc.c2_diverts += cs.diverts;
        acc.c2_warnings += cs.warnings;
        acc.c2_scrambles += cs.scrambles;
        acc.c2_on_call += cs.on_call_packages;
        acc.c2_on_call_sorties += cs.on_call_sorties;
        acc.c2_pool_aircraft += cs.pool_aircraft;
        acc.c2_pools += cs.pools;
        acc.c2_stood_down += cs.pools_stood_down;
        acc.c2_heard += cs.heard;
        acc.c2_refusals += cs.refusals;
        acc.c2_briefs += cs.briefs;
        acc.c2_requests += cs.requests;
        acc.c2_to_intel += cs.to_intel;
        acc.c2_recalls += cs.recalls;
        acc.c2_posts_struck += cs.posts_struck;
        acc.c2_posts_back += cs.posts_back;
        acc.c2_to_alternate += cs.to_alternate;
        acc.c2_lost_with_post += cs.lost_with_post;
        acc.c2_jammed_by_us += cs.jammed_by_us;
        acc.c2_holds += cs.clearance_holds;
        acc.c2_clearance_aborts += cs.clearance_aborts;
        for (double m : cs.on_call_min) acc.on_call_min += m;
        acc.on_call_n += static_cast<double>(cs.on_call_min.size());
        for (double m : cs.divert_min) acc.divert_min += m;
        acc.divert_n += static_cast<double>(cs.divert_min.size());
    }
    for (const Package& p : c.Packages()) {
        if (p.kind != MissionKind::ArmedRecce || !p.resolved) continue;
        acc.armed_recce += 1.0;
        acc.columns_found += p.found.empty() ? 0.0 : 1.0;
    }
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
    if (r.strike_packages > 0.0) {
        std::printf("Attacks (flights a campaign):");
        top(r.deliveries, 8);
        std::printf("; weapons:");
        top(r.weapons, 8);
        std::printf(". Strike groups: %.1f attackers a package, %.0f%% of eight or more; by target:", r.strike_aircraft / r.strike_packages,
                    100.0 * r.big_packages / r.strike_packages);
        for (const auto& g : r.group_by_kind) {
            if (g.second.first >= 0.5 * n) std::printf(" %s %.1f", g.first.c_str(), g.second.second / g.second.first);
        }
        std::printf(".\n");
    }
    if (!r.lost_by.empty()) {
        std::printf("Lost (manned aircraft a campaign) by:");
        top(r.lost_by, 8);
        std::printf("; most by:");
        top(r.lost_by_object, 6);
        std::printf("; on:");
        top(r.lost_mission, 8);
        std::printf("; the planners expected:");
        for (const auto& kv : r.expect_mission) std::printf(" %s %.1f (%.1f packages)", kv.first.c_str(), kv.second.second / n, kv.second.first / n);
        std::printf(".\n");
    }
    if (r.fight_attacked > 0.0) {
        std::printf("Strikers our fighters reached: %.1f flights a campaign; %.0f%% threw their stores off, %.0f%% turned for home (red_baron_vol2: 8 and 3 of 16 F-105 flights).\n",
                    r.fight_attacked / n, 100.0 * r.fight_jettison / r.fight_attacked, 100.0 * r.fight_abort / r.fight_attacked);
    }
    if (r.air_fights > 0.0) {
        std::printf("Air battle: %.1f fights a campaign, %.1f of the enemy's aircraft shot down by our fighters, %.1f of ours lost (%.1f:1).\n",
                    r.air_fights / n, r.air_enemy_lost / n, r.air_ours_lost / n, r.air_ours_lost > 0.0 ? r.air_enemy_lost / r.air_ours_lost : 0.0);
    }
    std::printf("Support: AWACS on station %.0f%%, %.1f aircraft refuelled and %.1f support sorties a campaign. Force: %.0f aircraft on day 1, %.0f at the end.\n",
                100.0 * r.awacs / n, r.refuelled / n, r.support_sorties / n, r.aircraft_day1 / n, r.aircraft_end / n);
    if (r.c2_pools > 0.0 || r.c2_reports > 0.0) {
        double sorties = 0.0;
        for (const DayStats& s : r.days) sorties += s.sorties;
        const double d1 = r.days.empty() ? 0.0 : r.days[0].sorties;
        std::printf("Command in the air: %.1f in-flight reports (%.1f out of cover), %.1f requests, %.1f warnings to hunters, %.1f diverts, %.1f scrambles, %.1f refused, %.1f to intelligence, %.1f FAC briefs; our listeners heard %.1f calls a campaign. Alert: %.1f pools of %.1f aircraft, %.1f stood down unused; %.1f on-call packages, %.1f sorties (%.1f%% of the sorties; day 1 %.1f%%).\n",
                    r.c2_reports / n, r.c2_out_of_cover / n, r.c2_requests / n, r.c2_warnings / n, r.c2_diverts / n, r.c2_scrambles / n, r.c2_refusals / n,
                    r.c2_to_intel / n, r.c2_briefs / n, r.c2_heard / n, r.c2_pools / n, r.c2_pool_aircraft / n, r.c2_stood_down / n, r.c2_on_call / n,
                    r.c2_on_call_sorties / n, sorties > 0.0 ? 100.0 * r.c2_on_call_sorties / sorties : 0.0,
                    d1 > 0.0 ? 100.0 * r.c2_on_call_sorties_d1 / d1 : 0.0);
        std::printf("Command live (S2): %.1f recalls, %.1f hops jammed by us, %.1f posts struck by our fires (%.1f set up again), %.1f messages to an alternate, %.1f lost with their post, %.1f close support briefs held, %.1f sent home a campaign.\n",
                    r.c2_recalls / n, r.c2_jammed_by_us / n, r.c2_posts_struck / n, r.c2_posts_back / n, r.c2_to_alternate / n, r.c2_lost_with_post / n,
                    r.c2_holds / n, r.c2_clearance_aborts / n);
    }
    // The bands (roadmap B14): each measure this theatre, faction and posture has one for.
    {
        double lost_all = 0.0, sorties_all = 0.0;
        for (const DayStats& s : r.days) {
            lost_all += s.lost;
            sorties_all += s.sorties;
        }
        struct M {
            const char* id;
            const char* label;
            double value;
            bool have;
            double scale;
            const char* unit;
        };
        const M ms[] = {
            {"loss_share", "aircraft lost a sortie", sorties_all > 0.0 ? lost_all / sorties_all : 0.0, sorties_all > 0.0, 100.0, "%"},
            {"battery_wrecked", "runs with the battery wrecked", r.battery_wrecked / n, true, 100.0, "%"},
            {"exchange", "the air battle: theirs lost for each of ours", r.air_ours_lost > 0.0 ? r.air_enemy_lost / r.air_ours_lost : 0.0,
             r.air_ours_lost + r.air_enemy_lost >= 10.0, 1.0, ""},
            {"on_call_min", "on call: the sighting to over the place (min)", r.on_call_n > 0.0 ? r.on_call_min / r.on_call_n : 0.0, r.on_call_n >= 5.0, 1.0, ""},
            {"divert_min", "a divert: the sighting to the new task (min)", r.divert_n > 0.0 ? r.divert_min / r.divert_n : 0.0, r.divert_n >= 3.0, 1.0, ""},
        };
        bool header = false;
        for (const M& m : ms) {
            const BalanceBand* b = g_bands.Find("air_war_balance", m.id, g_theatre, g_faction, posture);
            if (b == nullptr || !m.have) continue;
            if (!header) {
                std::printf("\n| Measure | The model | Band | Anchor | |\n|---|---|---|---|---|\n");
                header = true;
            }
            if (!BandRow(b, m.label, m.value, m.scale, m.unit)) ++g_fails;
        }
        if (header) std::printf("\n");
        if (r.on_call_n > 0.0 || r.divert_n > 0.0) {
            std::printf("Latencies: %.0f on-call packages over the place %.1f min after the sighting; %.0f diverts with the new task %.1f min after it.\n",
                        r.on_call_n, r.on_call_n > 0.0 ? r.on_call_min / r.on_call_n : 0.0, r.divert_n, r.divert_n > 0.0 ? r.divert_min / r.divert_n : 0.0);
        }
    }
    if (r.lines > 0.0) {
        std::printf("Our supply lines (%.0f): %.2f stocks and %.2f strength delivered, %.2f stocks lost on the way, %.1f columns destroyed (%.0f vehicles), %.0f line-hours cut, %.1f reroutes a campaign; the troops' stocks at the end %.0f%%. Armed reconnaissance: %.1f searches a campaign, a column found in %.0f%%.\n",
                    r.lines, r.delivered / n, r.strength / n, r.cargo_lost / n, r.columns_destroyed / n, r.vehicles_lost / n, r.hours_cut / n,
                    r.reroutes / n, 100.0 * r.stocks_end / n, r.armed_recce / n, r.armed_recce > 0.0 ? 100.0 * r.columns_found / r.armed_recce : 0.0);
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string theatre = "theatre_pannonia.json";
    std::vector<std::string> args;
    std::vector<std::pair<std::string, double>> sets;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--debug") {
            g_debug = true;
        } else if (a == "--cpu") {
            g_cpu = true;
        } else if (a == "--theatre" && i + 1 < argc) {
            theatre = argv[++i];
        } else if (a == "--set" && i + 1 < argc) {
            const std::string kv = argv[++i];
            const std::size_t eq = kv.find('=');
            if (eq == std::string::npos || TuningTable::Defaults().count(kv.substr(0, eq)) == 0) {
                std::fprintf(stderr, "--set needs a tuning key=value: %s\n", kv.c_str());
                return 1;
            }
            sets.push_back({kv.substr(0, eq), std::atof(kv.c_str() + eq + 1)});
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
    g_theatre = theatre;
    {
        std::string be;
        if (!LoadBalanceBands(data_dir + "/air_war/balance_bands.json", g_bands, &be)) {
            std::fprintf(stderr, "%s\n", be.c_str());
            return 1;
        }
    }

    for (const std::string& faction : factions) {
        AirWarData data;
        std::string error;
        if (!LoadAirWarData(data_dir, "air_war/" + theatre, "air_war/faction_" + faction + ".json", data, &error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        for (const auto& kv : sets) data.tuning.values[kv.first] = kv.second;
        ApplyLightMode(data);  // --set light.mode=0: the tuned theatre undated, as before roadmap S8
        for (int careful = 0; careful < 2; ++careful) {
            RunStats r;
            const std::clock_t c0 = std::clock();
            for (int s = 1; s <= seeds; ++s) {
                if (!Run(data, static_cast<unsigned>(s), days, careful != 0, r)) return 1;
            }
            const double cpu_s = static_cast<double>(std::clock() - c0) / CLOCKS_PER_SEC;
            g_faction = faction;
            Print(data.faction.name, careful != 0 ? "careful" : "careless", r, seeds);
            if (g_cpu) {
                std::printf("CPU: %.2f s for %d campaigns of %d days (%.1f ms a campaign day), ground.mode %.0f\n", cpu_s, seeds, days,
                            1000.0 * cpu_s / (seeds * std::max(1, days)), data.tuning.Get("ground.mode"));
                if (data.tuning.Get("ground.mode") >= 1.0 && !r.ground_km.empty()) {
                    const GroundAirStats& g = r.ground_air;
                    const double n = static_cast<double>(seeds);
                    std::printf("  F3.7 a campaign: %.1f strikes on troops (%.3f of their formations' establishments), %.1f march columns' "
                                "reinforcements (%.3f); %.1f overrun (%.1f at the end), %.1f retaken, %.1f escapes; %.1f troop moves (%.0f km), %.1f "
                                "supply lines routed again\n",
                                g.troop_strikes / n, g.strength_lost / n, g.reinforced / n, g.strength_brought / n, g.overrun / n, r.overrun_end / n,
                                g.retaken / n, g.escapes / n, g.troop_moves / n, g.troops_moved_km / n, g.supply_reroutes / n);
                    std::printf("  F3.7 labels a campaign: cas %.1f, bai %.1f, ai %.1f (by c2.bai_depth_m bai %.1f, ai %.1f)\n", g.cas / n, g.bai / n,
                                g.ai / n, g.bai_fixed / n, g.ai_fixed / n);
                }
                for (const auto& kv : r.ground_km) {
                    bool moved = false;
                    for (double v : kv.second) moved = moved || v > 0.0;
                    if (!moved) continue;
                    std::printf("  line in %-22s km by day:", kv.first.c_str());
                    for (double v : kv.second) std::printf(" %5.1f", v / seeds);
                    std::printf("\n");
                }
            }
        }
    }
    if (g_fails > 0) std::printf("\n%d measure(s) out of band.\n", g_fails);
    return g_fails > 0 ? 2 : 0;
}
