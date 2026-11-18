// simcore/air_war/resolve.hpp — a package flown against the player's side as it truly is, without the player
// (docs/air_war/05_EXECUTION.md).
//
// The same rules the planner estimates with, applied to the truth: every flight's route is walked through every real
// air defence's zone; a defence that works and has rounds engages (a radar defence radiates while it does, so ELINT
// and ARMs can find it; under strict EMCON it engages late, by optics, and less well); SEAD with the package suppresses
// what radiates and fires ARMs at it (a radar that switches off in time beats an ARM without memory: Black Buck,
// docs/research/ENEMY_AIR_RESEARCH_2026-10.md §3); jamming and night and weather change the odds; losses are drawn per
// aircraft. A package that loses too much before the target turns back (the leader's nerve). What reaches the target
// is resolved against what is really there: the target if it is where they aimed, a dummy if one is nearer, empty
// ground if the target moved or never existed. Reconnaissance photographs what is near its route. Everything is
// recorded in a PackageOutcome; the campaign turns it into damage, losses, intelligence and lessons.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "simcore/air_war/air_fight.hpp"
#include "simcore/air_war/data.hpp"
#include "simcore/air_war/logistics.hpp"
#include "simcore/air_war/opportunity.hpp"
#include "simcore/air_war/planner.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/air_war/types.hpp"

namespace simcore {
namespace airwar {

struct ResolveContext {
    const AirWarData* data = nullptr;
    std::vector<ObjectState>* truth = nullptr;  // parallel to data->theatre.objects
    const BatteryPosture* posture = nullptr;    // the player's battery
    const AirStaff* staff = nullptr;
    Weather weather = Weather::Clear;
    Rng* rng = nullptr;
    // A defence to leave out (the player's battery when its part of the raid was played as a mission).
    std::string skip_defence;
    // A played raid (AirCampaign::CompletePlayerMission): what flew inside this circle between these hours was the
    // scenario's, and the defences there took their chances in it (mission_bridge.hpp); every other part of every route
    // is resolved here, the way in before the scenario as much as the way out after it.
    Vec3 played_centre;
    double played_radius_m = 0.0;
    double played_from_h = 0.0;
    double played_to_h = 0.0;
    double awacs = 0.0;  // the share of the day the enemy's AWACS is on station (docs/air_war/11_SUPPORT.md)
    // The map's ground (route.mode 1, route_planner.hpp): a low flight is seen only where the ground lets a site see it.
    const RouteGround* ground = nullptr;
    // Targets of opportunity at the abstract level (roadmap B6, FindOpportunity): the campaign's seed for their keyed
    // draws (0: none, as before B6) and its phase (the Pact's free hunt).
    std::uint64_t opportunity_seed = 0;
    int phase = 0;
    // Our fighters' corridors as they are (S1, docs/air_battle/08_OUR_FIGHTERS.md section 9; theatre ENU, hours of the day):
    // a package timed into one its staff believes in meets defences near it holding fire for ours at its hours.
    std::vector<command::OwnAirZone> own_corridors;
};

// The leg a package's fighters meet its main flights on: the one from its IP (or a helicopter's hide); 1 without one.
inline int FighterLeg(const std::vector<RoutePoint>& r) {
    const int ip = std::max(RouteIndexOf(r, "ip"), RouteIndexOf(r, "hide"));
    return ip >= 0 ? ip : 1;
}

// How much a package's SEAD suppresses a radar defence it meets: its SEAD and hunter aircraft, its SEAD officer's skill
// and how well the package keeps together. Shared by the resolver and a played raid's neighbours (mission_bridge.hpp).
inline double PackageSuppression(const Package& p, const AirStaff& staff, const TuningTable& tune) {
    const int sead = p.Count(FlightRole::Sead) + p.Count(FlightRole::Hunter);
    const Officer* so = staff.HolderPtr(StaffRole::Sead);
    const double sead_skill = so != nullptr ? so->Effective(so->def.skills.sead) : 0.3;
    return tune.Get("attrition.sead_suppression") * std::min(1.0, sead / 2.0) * (0.6 + 0.4 * sead_skill) * (0.5 + 0.5 * p.coordination);
}

// The package leader's airmanship (0.5 without a leader).
inline double PackageAirmanship(const Package& p, const AirStaff& staff) {
    const Officer* leader = p.leader >= 0 ? &staff.officers[static_cast<std::size_t>(p.leader)] : nullptr;
    return leader != nullptr ? leader->Effective(leader->def.skills.airmanship) : 0.5;
}

// The package leader's nerve: the share of the package he will lose before turning back (0.25 + 0.3 x boldness, the
// planner's and his own abort_threshold quirks). One rule for the abstract resolver and the played raid (flights.hpp's
// package, roadmap B1), so the two turn back alike.
inline double PackageNerve(const Package& p, const AirStaff& staff) {
    const Officer* leader = p.leader >= 0 ? &staff.officers[static_cast<std::size_t>(p.leader)] : nullptr;
    double nerve = 0.25 + 0.3 * (leader != nullptr ? leader->def.traits.boldness : 0.5);
    if (const Officer* pln = p.planner >= 0 ? &staff.officers[static_cast<std::size_t>(p.planner)] : nullptr) {
        nerve += 0.1 * pln->Quirk("abort_threshold");
    }
    if (leader != nullptr) nerve += 0.15 * leader->Quirk("abort_threshold");
    return ClampTo(nerve, 0.05, 0.9);
}

// A share of a route leg, from a to b (fractions 0..1 of it).
struct LegPiece {
    double a = 0.0;
    double b = 1.0;
};

// The parts of the leg r0-r1 the resolver resolves: all of it, less what a played raid's scenario flew (inside its
// circle while it ran).
inline std::vector<LegPiece> ResolvedPieces(const RoutePoint& r0, const RoutePoint& r1, const ResolveContext& c) {
    if (c.played_radius_m <= 0.0) return {LegPiece{}};
    const double dx = r1.pos.x - r0.pos.x, dy = r1.pos.y - r0.pos.y;
    const double fx = r0.pos.x - c.played_centre.x, fy = r0.pos.y - c.played_centre.y;
    const double qa = dx * dx + dy * dy;
    const double qc = fx * fx + fy * fy - c.played_radius_m * c.played_radius_m;
    double t_in = 0.0, t_out = 0.0;
    if (qa <= 0.0) {
        if (qc >= 0.0) return {LegPiece{}};
        t_in = 0.0;
        t_out = 1.0;
    } else {
        const double qb = 2.0 * (fx * dx + fy * dy);
        const double disc = qb * qb - 4.0 * qa * qc;
        if (disc <= 0.0) return {LegPiece{}};
        const double sq = std::sqrt(disc);
        t_in = Clamp01((-qb - sq) / (2.0 * qa));
        t_out = Clamp01((-qb + sq) / (2.0 * qa));
        if (t_out <= t_in) return {LegPiece{}};
    }
    // Played only if the inside part overlaps the scenario's hours.
    const double h_in = r0.t_h + t_in * (r1.t_h - r0.t_h), h_out = r0.t_h + t_out * (r1.t_h - r0.t_h);
    if (std::max(h_in, h_out) < c.played_from_h || std::min(h_in, h_out) > c.played_to_h) return {LegPiece{}};
    std::vector<LegPiece> out;
    if (t_in > 0.0) out.push_back(LegPiece{0.0, t_in});
    if (t_out < 1.0) out.push_back(LegPiece{t_out, 1.0});
    return out;
}

// What a package does in the air that its own side can hear of (roadmap B5, docs/air_battle/05_C2_AND_RETASKING.md
// §4.6): a launch its aircraft saw, a loss, a column found, a release, the package turning back. The abstract command
// posts (c2.hpp) read them as their flights' reports; each is dated when it happened.
struct AirEvent {
    enum class Kind { LaunchSeen, Loss, ColumnFound, Release, TurnedBack };
    Kind kind = Kind::LaunchSeen;
    double t_h = 0.0;
    int flight = -1;        // index into the package's flights
    std::string object_id;  // the defence that fired, the column found, the target
    Vec3 where;             // where it happened (the defence's place, the column's)
    int alive = 0;          // the flight's aircraft still flying then (somebody to report it)
};

// A package in the air (roadmap B5, A0): walked leg by leg in the order of time, so it has a place at every hour and its
// posts can act while it flies. Resolver::Begin sets it up, Resolver::StepTo walks every leg that ends by then.
struct PackageRun {
    std::vector<char> belt_done;  // per flight: its damaged aircraft have met their own belt (S1)
    PackageOutcome out;
    Rng rng{1};
    Rng* ext = nullptr;  // draw from this stream instead (Resolver::Resolve: the caller's)
    std::vector<double> suppressed;  // 0..1 how much each radar was suppressed
    std::vector<bool> engaged;
    bool abort_now = false;
    std::vector<std::size_t> order;     // the flights by their time over the target (decoys and SEAD first)
    std::vector<std::size_t> next_leg;  // per flight: the leg it flies next (its route's size: done)
    std::vector<int> arms_left;
    std::vector<AirEvent> events;       // since the last read (the posts take them)
    // The route planner's routes (route.mode 1, roadmap B2): one engagement a pass through a defence's reach however many
    // legs it takes (each flight's chord carried on to the pass's end, with the nearest it came for the ARMs), a low flight
    // seen only where the ground lets the site see it, and the corps' suppression at the package's gate.
    bool routed = false;
    bool on_ground = false;
    std::vector<double> corridor_supp;
    std::vector<std::vector<double>> pending;       // per flight, per object
    std::vector<std::vector<double>> pending_dist;  // per flight, per object
    double supp_base = 0.0;
    double airmanship = 0.5;
    double nerve = 0.5;
    int package_size = 1;
    bool jam = false;
    bool begun = false;
    // The air battle (roadmap B8, air_fight.hpp): fought once, when the first strike flight reaches its fighter leg; the
    // strike flights it sent home and those that threw their stores off.
    bool fought = false;
    std::vector<bool> fight_home;
    std::vector<bool> fight_jettisoned;

    Rng& R() { return ext != nullptr ? *ext : rng; }
    bool FlightDone(const Package& p, std::size_t fi) const { return next_leg[fi] + 1 >= p.flights[fi].route.size(); }
    bool Done(const Package& p) const {
        for (std::size_t fi = 0; fi < p.flights.size() && fi < next_leg.size(); ++fi) {
            if (!FlightDone(p, fi)) return false;
        }
        return true;
    }
    // Where a flight is at `t_h` on its route (its last point past the end).
    static Vec3 PlaceOf(const Flight& fl, double t_h) {
        if (fl.route.empty()) return Vec3{};
        for (std::size_t i = 0; i + 1 < fl.route.size(); ++i) {
            const RoutePoint& a = fl.route[i];
            const RoutePoint& b = fl.route[i + 1];
            if (t_h > b.t_h) continue;
            if (t_h <= a.t_h || b.t_h <= a.t_h) return a.pos;
            return a.pos + (b.pos - a.pos) * ((t_h - a.t_h) / (b.t_h - a.t_h));
        }
        return fl.route.back().pos;
    }
};

// Send a flight in the air somewhere new from where it is now (a post's divert, a hunter's own leave: roadmap B5): its
// route from the leg it flies goes from its place at `now_h` to `place` and then on to the rest of its route (its last
// point, with `release_there`: its weapons are for the new place), the times moved by the detour at `speed_mps`.
inline bool RedirectFlight(Package& p, PackageRun& run, std::size_t fi, const Vec3& place, double now_h, double speed_mps, bool release_there) {
    if (fi >= p.flights.size() || fi >= run.next_leg.size() || run.FlightDone(p, fi) || speed_mps <= 0.0) return false;
    Flight& fl = p.flights[fi];
    const std::size_t si = run.next_leg[fi];
    const RoutePoint& r0 = fl.route[si];
    if (now_h < r0.t_h) now_h = r0.t_h;
    RoutePoint here;
    here.pos = PackageRun::PlaceOf(fl, now_h);
    here.pos.z = r0.pos.z;
    here.agl_m = r0.agl_m;
    here.t_h = now_h;
    here.tag = "divert";
    RoutePoint there = here;
    there.pos = Vec3{place.x, place.y, r0.pos.z};
    there.t_h = now_h + Dist2d(here.pos, there.pos) / speed_mps / 3600.0;
    there.tag = release_there ? "target" : "divert";
    std::vector<RoutePoint> route(fl.route.begin(), fl.route.begin() + static_cast<std::ptrdiff_t>(si + 1));
    route.push_back(here);
    route.push_back(there);
    if (release_there) {
        RoutePoint home = fl.route.back();
        home.t_h = there.t_h + Dist2d(there.pos, home.pos) / speed_mps / 3600.0;
        route.push_back(home);
        fl.release_index = static_cast<int>(si + 2);
        fl.aim_pos = there.pos;
        fl.released = false;
    } else {
        // On to where it was going, late by the detour.
        const std::size_t next = si + 1;
        const double back_h = there.t_h + Dist2d(there.pos, fl.route[next].pos) / speed_mps / 3600.0;
        const double shift = std::max(0.0, back_h - fl.route[next].t_h);
        for (std::size_t k = next; k < fl.route.size(); ++k) {
            RoutePoint q = fl.route[k];
            q.t_h += shift;
            route.push_back(q);
        }
        if (fl.release_index > static_cast<int>(si)) fl.release_index += 2;
    }
    fl.route = route;
    return true;
}

class Resolver {
public:
    // The whole package at once, drawing from `c.rng` (the bench's and the tests' way; the campaign steps it in time).
    // `by_flight`: each flight's whole route before the next's, in the order over the target, as the resolver walked them
    // before roadmap B5 (a campaign replaying a save of version 1, c2.mode 0); else every leg in the order of time.
    PackageOutcome Resolve(Package& p, ResolveContext& c, bool by_flight = false) {
        PackageRun run;
        Begin(p, c, run, Rng(1));
        run.ext = c.rng;
        if (by_flight) {
            for (std::size_t i = 0; i < run.order.size(); ++i) run.order[i] = i;
            std::sort(run.order.begin(), run.order.end(), [&](std::size_t a, std::size_t b) {
                return TargetTime(p.flights[a]) < TargetTime(p.flights[b]);
            });
            c_ = &c;
            run_ = &run;
            for (const std::size_t fi : run.order) {
                while (!run.FlightDone(p, fi)) Leg(p, fi);
            }
        } else {
            StepTo(p, c, run, 1e18);
        }
        run.ext = nullptr;
        p.resolved = true;
        return run.out;
    }

    // A package taking off: its draws from `rng` (a fork keyed by the package: interleaving packages in time changes none
    // of one package's draws).
    void Begin(Package& p, ResolveContext& c, PackageRun& run, Rng rng) {
        const AirWarData& d = *c.data;
        run = PackageRun();
        run.rng = rng;
        run.out.package_id = p.id;
        run.out.truth_applied = true;
        run.jam = p.Has(FlightRole::Jammer);
        run.supp_base = PackageSuppression(p, *c.staff, d.tuning);
        run.airmanship = PackageAirmanship(p, *c.staff);
        run.nerve = PackageNerve(p, *c.staff);  // the share of the package he will lose before turning back
        run.package_size = std::max(1, p.Aircraft());
        run.suppressed.assign(d.theatre.objects.size(), 0.0);
        run.engaged.assign(d.theatre.objects.size(), false);
        // Decoys and SEAD go first in time; a tie keeps the plan's order.
        run.order.resize(p.flights.size());
        for (std::size_t i = 0; i < run.order.size(); ++i) run.order[i] = i;
        std::stable_sort(run.order.begin(), run.order.end(), [&](std::size_t a, std::size_t b) {
            return TargetTime(p.flights[a]) < TargetTime(p.flights[b]);
        });
        // The route planner's routes (route.mode 1): a defence engages once a pass however many legs the pass takes, sees a
        // low flight only where the ground lets it, and the corps suppresses the defences at a gate where the land forces
        // face each other when the package asked for it (jsead_1982).
        const TuningTable& tune = d.tuning;
        const FrontDef* front = d.theatre.FrontFor(d.faction.front);
        run.routed = tune.Get("route.mode") >= 1.0;
        run.on_ground = run.routed && c.ground != nullptr && c.ground->HasGround();
        run.corridor_supp.assign(d.theatre.objects.size(), 0.0);
        if (run.routed && p.corridor_suppression && front != nullptr) {
            for (const EntryPoint& g : front->entries) {
                if (g.id != p.entry) continue;
                bool land = false;
                for (std::size_t oj = 0; oj < d.theatre.objects.size(); ++oj) {
                    const TheatreObject& o = d.theatre.objects[oj];
                    if (o.kind == ObjectKind::Troops && (*c.truth)[oj].function > 0.05 &&
                        Dist2d((*c.truth)[oj].pos, g.pos) < tune.Get("route.land_front_m")) {
                        land = true;
                    }
                }
                for (std::size_t oj = 0; land && oj < d.theatre.objects.size(); ++oj) {
                    if (Dist2d((*c.truth)[oj].pos, g.pos) < tune.Get("route.corridor_halfwidth_m")) run.corridor_supp[oj] = tune.Get("route.corridor_suppression");
                }
            }
        }
        const std::size_t per = run.routed ? d.theatre.objects.size() : 0;
        run.pending.assign(p.flights.size(), std::vector<double>(per, 0.0));
        run.pending_dist.assign(p.flights.size(), std::vector<double>(per, 1e18));
        run.next_leg.assign(p.flights.size(), 0);
        run.arms_left.assign(p.flights.size(), 0);
        run.fight_home.assign(p.flights.size(), false);
        run.fight_jettisoned.assign(p.flights.size(), false);
        for (std::size_t fi = 0; fi < p.flights.size(); ++fi) {
            const Flight& fl = p.flights[fi];
            run.arms_left[fi] = fl.arms;
            if (d.faction.Aircraft(fl.aircraft) == nullptr || fl.count <= 0) run.next_leg[fi] = fl.route.empty() ? 0 : fl.route.size() - 1;
        }
        run.begun = true;
    }

    // Walk every leg that ends by `t_h`, in the order of time (a tie: the flights' order over the target).
    void StepTo(Package& p, ResolveContext& c, PackageRun& run, double t_h) {
        c_ = &c;
        run_ = &run;
        for (;;) {
            int best = -1;
            double best_t = 0.0;
            for (std::size_t k = 0; k < run.order.size(); ++k) {
                const std::size_t fi = run.order[k];
                if (run.FlightDone(p, fi)) continue;
                const double te = p.flights[fi].route[run.next_leg[fi] + 1].t_h;
                if (te > t_h) continue;
                if (best < 0 || te < best_t) {
                    best = static_cast<int>(fi);
                    best_t = te;
                }
            }
            if (best < 0) break;
            Leg(p, static_cast<std::size_t>(best));
        }
    }

private:
    ResolveContext* c_ = nullptr;
    PackageRun* run_ = nullptr;

    Rng& R() { return run_->R(); }

    void Event(AirEvent::Kind kind, double t_h, std::size_t fi, const std::string& id, const Vec3& where, int alive) {
        AirEvent e;
        e.kind = kind;
        e.t_h = t_h;
        e.flight = static_cast<int>(fi);
        e.object_id = id;
        e.where = where;
        e.alive = alive;
        run_->events.push_back(e);
    }

    // The flight is done with its route (shot down, turned back, home): reconnaissance without a release point
    // photographs what was along it.
    // A defence near our fighters' corridor, at its hours, holds fire for ours (S1, mt_1973_nvn pp. 13-14): the share it keeps.
    double OwnCorridorHold(const Package& p, const Vec3& at, double when_h) const {
        const AirWarData& d = *c_->data;
        if (!d.has_own_air || d.tuning.Get("own_air.mode") < 1.0) return 1.0;
        double hod = std::fmod(d.theatre.start_hour + when_h, 24.0);
        if (hod < 0.0) hod += 24.0;
        for (const command::OwnAirZone& z : c_->own_corridors) {
            if (z.id != p.own_corridor || hod < z.from_h || hod > z.to_h || z.DistanceM(at) > d.own_air.learning.near_m) continue;
            return 1.0 - d.own_air.learning.hold_fire;
        }
        return 1.0;
    }
    // Its damaged aircraft come home over its own ground air defence (S1, docs/air_battle/08_OUR_FIGHTERS.md section 10): off
    // the minimum risk route and with Mode 4 failed, each by a chance, and then its own belt may shoot one down; written in
    // its records as lost to its own fire. Draws from a fork of the package's stream keyed on kOwnAirKey + 0x900 (none taken).
    void OwnBelt(Package& p, std::size_t fi) {
        const AirWarData& d = *c_->data;
        const OwnAdDef& a = d.faction.own_ad;
        if (!a.present || d.tuning.Get("own_air.mode") < 1.0 || fi >= p.flights.size()) return;
        PackageRun& run = *run_;
        if (run.belt_done.size() < p.flights.size()) run.belt_done.resize(p.flights.size(), 0);
        if (run.belt_done[fi] != 0) return;
        run.belt_done[fi] = 1;
        Flight& fl = p.flights[fi];
        const int damaged = std::min(fl.damaged, fl.count - fl.lost);
        if (damaged <= 0 || fl.route.empty() || fl.role == FlightRole::Missile) return;
        Rng belt = R().Fork(command::kOwnAirKey + 0x900ull + (static_cast<std::uint64_t>(p.id) << 8) + static_cast<std::uint64_t>(fi));
        for (int k = 0; k < damaged; ++k) {
            const bool off = belt.Chance(a.off_mrr_damaged);
            const bool mode4 = belt.Chance(a.mode4_failed_damaged);
            if (!(off && mode4) || !belt.Chance(a.belt_kill)) continue;
            LossRecord lr;
            lr.flight = static_cast<int>(fi);
            lr.by = a.belt;
            lr.where = fl.route.back().pos;
            lr.own_fire = true;
            run.out.losses.push_back(lr);
            ++fl.lost;
            Event(AirEvent::Kind::Loss, fl.route.back().t_h, fi, a.belt, lr.where, fl.count - fl.lost);
            run.out.notes.push_back(fl.callsign + ": a damaged aircraft lost to its own " + a.belt + " coming home off its minimum risk route, Mode 4 failed");
        }
    }

    void Finish(Package& p, std::size_t fi) {
        OwnBelt(p, fi);
        Flight& fl = p.flights[fi];
        run_->next_leg[fi] = fl.route.empty() ? 0 : fl.route.size() - 1;
        const AircraftType* a = c_->data->faction.Aircraft(fl.aircraft);
        if (a != nullptr && fl.role == FlightRole::Recce && !fl.released && fl.release_index < 0 && fl.lost < fl.count) Photograph(fl, *a, run_->out);
    }

    // The air battle over the package at `t_h` (roadmap B8): our fighters against its fighters and its strikers, the losses
    // written to each flight where it is then, with our regiment as the one that shot it down. Its draws come from a fork
    // of the package's stream (taking none from it) and the chain's keyed draws.
    void AirBattle(Package& p, double t_h) {
        ResolveContext& c = *c_;
        PackageRun& run = *run_;
        PackageOutcome& out = run.out;
        const AirWarData& d = *c.data;
        const AbstractAirFight fight(d);
        const AbstractFightSetup setup = AbstractAirFight::SetupFor(d, p, c.awacs, c.weather);
        std::vector<int> alive(p.flights.size(), 0);
        for (std::size_t fi = 0; fi < p.flights.size(); ++fi) alive[fi] = std::max(0, p.flights[fi].count - p.flights[fi].lost);
        Rng fork = R().Fork(0xB8F16B47ull ^ static_cast<std::uint64_t>(p.id));
        const std::uint64_t seed = fork.Next();
        const AbstractFightResult res = fight.Resolve(p, alive, setup, seed);
        if (!res.found) return;
        ++out.air_fights;
        Rng more(seed ^ 0x5EEDB8ull);
        for (const AbstractFightResult::Loss& l : res.enemy_lost) {
            if (l.flight < 0 || static_cast<std::size_t>(l.flight) >= p.flights.size()) continue;
            Flight& fl = p.flights[static_cast<std::size_t>(l.flight)];
            if (fl.lost >= fl.count) continue;
            LossRecord lr;
            lr.flight = l.flight;
            lr.by = l.by;
            lr.fighter = true;
            lr.where = PackageRun::PlaceOf(fl, t_h);
            lr.officer = fl.officer >= 0 && fl.lost == 0;
            lr.captured = more.Chance(0.5);
            out.losses.push_back(lr);
            ++fl.lost;
            Event(AirEvent::Kind::Loss, t_h, static_cast<std::size_t>(l.flight), l.by, lr.where, fl.count - fl.lost);
        }
        for (const std::string& by : res.ours_lost) out.our_fighters_lost.push_back(by);
        out.fight_attacked_flights += res.attacked;
        out.fight_jettison_drawn += res.jettison_drawn;
        out.fight_aborted += static_cast<int>(res.aborted.size());
        for (const int fi : res.aborted) {
            run.fight_home[static_cast<std::size_t>(fi)] = true;
            out.aborted_flights.push_back(fi);
        }
        for (const int fi : res.jettisoned) {
            run.fight_jettisoned[static_cast<std::size_t>(fi)] = true;
            out.jettisoned_flights.push_back(fi);
        }
        std::string note = "our fighters (" + std::to_string(res.ours_up) + ") found it";
        if (res.met_cover) note += ", met its fighters";
        if (res.reached) note += ", reached its strikers";
        note += ": " + std::to_string(res.enemy_lost.size()) + " of its aircraft and " + std::to_string(res.ours_lost.size()) + " of ours shot down";
        out.notes.push_back(note);
    }

    // One leg of one flight: the defences it passes, the defender's fighters, the leader's nerve, the release.
    void Leg(Package& p, std::size_t oi) {
        ResolveContext& c = *c_;
        PackageRun& run = *run_;
        PackageOutcome& out = run.out;
        const AirWarData& d = *c.data;
        const TuningTable& tune = d.tuning;
        const FrontDef* front = d.theatre.FrontFor(d.faction.front);
        Flight& fl = p.flights[oi];
        const AircraftType* a = d.faction.Aircraft(fl.aircraft);
        const std::size_t si = run.next_leg[oi];
        int alive = fl.count - fl.lost;
        if (a == nullptr || alive <= 0) {
            Finish(p, oi);
            return;
        }
        const bool main = fl.role == FlightRole::Strike || fl.role == FlightRole::Dead || fl.role == FlightRole::Helicopter;
        int& arms_left = run.arms_left[oi];
        const RoutePoint& r0 = fl.route[si];
        const RoutePoint& r1 = fl.route[si + 1];
        const bool before_release = fl.release_index < 0 || static_cast<int>(si) < fl.release_index;
        const bool sent_home = oi < run.fight_home.size() && run.fight_home[oi];
        if (main && (run.abort_now || sent_home) && before_release) {
            fl.aborted = true;
            Finish(p, oi);
            return;
        }
        const int fighter_leg = FighterLeg(fl.route);
        const bool masked = run.on_ground && c.ground != nullptr && (fl.profile == Profile::Low || a->helicopter);
        const std::vector<LegPiece> pieces = ResolvedPieces(r0, r1, c);
        for (std::size_t pi = 0; pi < pieces.size(); ++pi) {
            const LegPiece& piece = pieces[pi];
            const Vec3 p0 = piece.a <= 0.0 ? r0.pos : r0.pos + (r1.pos - r0.pos) * piece.a;
            const Vec3 p1 = piece.b >= 1.0 ? r1.pos : r0.pos + (r1.pos - r0.pos) * piece.b;
            for (std::size_t oj = 0; oj < d.theatre.objects.size(); ++oj) {
                const TheatreObject& def = d.theatre.objects[oj];
                ObjectState& st = (*c.truth)[oj];
                if (!def.defence.present || st.function <= 0.05 || def.decoy) continue;
                if (!c.skip_defence.empty() && def.id == c.skip_defence) continue;
                if (st.missiles == 0) continue;
                if (def.defence.max_target_mps > 0.0 && a->cruise_mps > def.defence.max_target_mps) continue;  // too fast for it
                double chord = ChordInCircle2d(p0, p1, st.pos, def.defence.radius_m);
                double arm_dist = -1.0;
                if (run.routed) {
                    double& pending = run.pending[oi][oj];
                    double& pending_dist = run.pending_dist[oi][oj];
                    if (chord > 0.0 && masked) {
                        chord = c.ground->Seen(def.id, st.pos, def.defence.radius_m, p0, p1, r0.agl_m + (r1.agl_m - r0.agl_m) * piece.a,
                                               r0.agl_m + (r1.agl_m - r0.agl_m) * piece.b, true);
                    }
                    if (chord > 0.0) {
                        const double dist = SegmentDistance2d(st.pos, p0, p1);
                        // The pass goes on into the next leg: one engagement for the whole pass, where it ends.
                        const bool last_piece = pi + 1 == pieces.size();
                        const bool at_release = static_cast<int>(si + 1) == fl.release_index;
                        if (last_piece && !at_release && si + 2 < fl.route.size()) {
                            const RoutePoint& r2 = fl.route[si + 2];
                            double next = ChordInCircle2d(r1.pos, r2.pos, st.pos, def.defence.radius_m);
                            if (next > 0.0 && masked) next = c.ground->Seen(def.id, st.pos, def.defence.radius_m, r1.pos, r2.pos, r1.agl_m, r2.agl_m, true);
                            if (next > 0.0) {
                                pending += chord;
                                pending_dist = std::min(pending_dist, dist);
                                continue;
                            }
                        }
                        chord += pending;
                        arm_dist = std::min(pending_dist, dist);
                    }
                    pending = 0.0;
                    pending_dist = 1e18;
                }
                if (chord <= 0.0) continue;
                double along = 0.0;
                SegmentDistance2d(st.pos, p0, p1, &along);
                const double when_h = r0.t_h + (piece.a + along * (piece.b - piece.a)) * (r1.t_h - r0.t_h);
                const bool radar = def.defence.radar;
                const bool emcon = def.player ? c.posture->fight_silent : false;
                // A radar defence radiates while it engages (not under EMCON): ELINT hears it; SEAD fires at it.
                if (radar && !emcon) {
                    out.AddEmission(def.id, chord / std::max(50.0, a->cruise_mps) + 30.0);
                    if (arms_left > 0 && (fl.role == FlightRole::Sead || fl.role == FlightRole::Hunter)) {
                        const double dist = arm_dist >= 0.0 ? arm_dist : SegmentDistance2d(st.pos, p0, p1);
                        if (dist <= a->arm_range_m) {
                            FireArms(fl, *a, def, st, arms_left, out, run.suppressed[oj]);
                        }
                    }
                }
                if (a->drone && fl.role == FlightRole::Decoy) {
                    // A decoy draws fire: the defence wastes rounds and shows itself, and the SEAD behind the
                    // decoys knows where to aim (the Bekaa, 1982).
                    if (!run.engaged[oj]) {
                        run.engaged[oj] = true;
                        Spend(st, 1 + R().Index(2), out);
                        if (radar && run.supp_base > 0.0) run.suppressed[oj] = std::max(run.suppressed[oj], tune.Get("attrition.decoy_exposure"));
                        out.fired.push_back(def.id);
                        if (R().Chance(0.6)) ++fl.lost;  // drones are meant to be lost; not counted as losses
                    }
                    continue;
                }
                double env = 1.0;
                if (!radar && p.part == DayPart::Night) env *= tune.Get("attrition.night");
                if (!radar && c.weather == Weather::Poor) env *= tune.Get("attrition.poor_weather");
                if (emcon) env *= tune.Get("attrition.emcon_lethality");
                double supp = radar && !emcon ? std::max(run.supp_base, run.suppressed[oj]) : 0.0;
                if (radar && !emcon && run.routed) supp = std::max(supp, run.corridor_supp[oj]);
                if (st.function < 1.0) env *= st.function;
                if (!p.own_corridor.empty() && p.own_corridor_found) env *= OwnCorridorHold(p, st.pos, when_h);
                const double k = PassKillChance(def.defence.lethality, chord, def.defence.radius_m, fl.profile, a->helicopter,
                                                def.defence.low_cover, def.defence.medium_cover, radar, supp,
                                                run.jam && radar ? tune.Get("attrition.jamming") : 0.0, env,
                                                Clamp01(a->evasion + 0.3 * (run.airmanship - 0.5)), tune, masked);
                if (k <= 0.0) continue;
                run.engaged[oj] = true;
                int shots = 0;
                for (int n = 0; n < EngagedAircraft(def.defence.channels, st.function, alive); ++n) {
                    ++shots;
                    if (a->missile) {
                        // A cruise missile shot down: the defence's, never a crew lost.
                        if (R().Chance(k)) {
                            ++fl.lost;
                            out.missiles_downed.push_back(def.id);
                        }
                        continue;
                    }
                    if (R().Chance(k)) {
                        LossRecord lr;
                        lr.flight = static_cast<int>(oi);
                        lr.by = def.id;
                        lr.where = p0 + (p1 - p0) * along;
                        lr.officer = fl.officer >= 0 && fl.lost == 0;
                        lr.captured = R().Chance(0.5);
                        out.losses.push_back(lr);
                        ++fl.lost;
                        Event(AirEvent::Kind::Loss, when_h, oi, def.id, lr.where, fl.count - fl.lost);
                    } else if (R().Chance(0.5 * k)) {
                        ++fl.damaged;
                        ++out.damaged;
                    }
                }
                alive = fl.count - fl.lost;
                if (shots > 0) {
                    Spend(st, std::min(shots, 2), out);
                    out.fired.push_back(def.id);
                    if (!a->missile) Event(AirEvent::Kind::LaunchSeen, when_h, oi, def.id, st.pos, alive);
                }
            }
        }
        // The defender's fighters (roadmap B8, air_fight.hpp): the air battle over the whole package, once, when its first
        // strike flight reaches the leg they meet it on.
        if (front != nullptr && main && static_cast<int>(si) == fighter_leg && alive > 0 && !run.fought) {
            run.fought = true;
            AirBattle(p, r1.t_h);
            alive = fl.count - fl.lost;
        }
        // The leader's nerve: on the losses so far in time.
        if (main && before_release && static_cast<double>(out.Lost()) / run.package_size >= run.nerve && !run.abort_now) {
            run.abort_now = true;
            out.aborted = true;
            out.notes.push_back("the package turned back after losing " + std::to_string(out.Lost()) + " of " + std::to_string(run.package_size));
            Event(AirEvent::Kind::TurnedBack, r1.t_h, oi, p.target_object, r1.pos, alive);
        }
        // Weapons at the release point.
        const bool jettisoned = oi < run.fight_jettisoned.size() && run.fight_jettisoned[oi];
        if (static_cast<int>(si + 1) == fl.release_index && alive > 0 && !fl.aborted && !fl.released && !jettisoned) {
            if (main || fl.role == FlightRole::Missile) {
                Deliver(p, fl, oi, *a, alive, run.airmanship, out, r1.t_h);
                alive = fl.count - fl.lost;
            }
            if (fl.role == FlightRole::Recce) Photograph(fl, *a, out);
            fl.released = true;
        }
        if (alive > 0 && !fl.aborted) FindOpportunity(p, fl, oi, *a, r0, r1, out);
        run.next_leg[oi] = si + 1;
        if (alive <= 0 || run.FlightDone(p, oi)) Finish(p, oi);
    }

    // Targets of opportunity at the abstract level (roadmap B6, docs/air_battle/06_OPPORTUNITY.md section 8's A0): a flight
    // that is not on an armed reconnaissance (FindColumn) may find a column on the move its leg passes within
    // logistics.search_m, at the column find chance times opportunity.a0_find_scale times its type's ease; its leader then
    // runs the chain (opportunity.hpp) on it: identified, his faction's rules, his stores (a built-in gun, or rockets no
    // longer owed to his target), worth and his tendency. An attack takes opportunity.a0_column_hit of the column's
    // working share per aircraft and loses one to its guns at opportunity.a0_column_guns; a report goes to the posts
    // (its faction's report_in_flight) or waits for the debrief. One find a leg. Every draw keyed (em::Stream::Opportunity:
    // the campaign's seed, the package, the flight, the column): the resolver's own stream falls as without it.
    void FindOpportunity(Package& p, Flight& fl, std::size_t oi, const AircraftType& a, const RoutePoint& r0, const RoutePoint& r1,
                         PackageOutcome& out) {
        const ResolveContext& c = *c_;
        const AirWarData& d = *c.data;
        const TuningTable& tune = d.tuning;
        if (c.opportunity_seed == 0 || tune.Get("opportunity.mode") < 1.0) return;
        if (p.kind == MissionKind::ArmedRecce || a.missile || a.drone || a.opportunity_ease <= 0.0) return;
        if (fl.role == FlightRole::Missile || fl.role == FlightRole::Decoy || fl.role == FlightRole::Jammer || fl.role == FlightRole::Awacs ||
            fl.role == FlightRole::Tanker || fl.role == FlightRole::Elint)
            return;
        const int alive = fl.count - fl.lost;
        if (alive <= 0) return;
        // A leg through a played raid's circle at its time was the session's (opportunity_session.hpp).
        if (c.played_radius_m > 0.0 && SegmentDistance2d(c.played_centre, r0.pos, r1.pos) < c.played_radius_m && r1.t_h >= c.played_from_h &&
            r0.t_h <= c.played_to_h)
            return;
        // His stores: a gun built in, or rockets no longer owed to his target.
        const bool due = !fl.released && fl.release_index >= 0;
        const WeaponDef* gun = nullptr;
        for (const auto& ld : a.loads) {
            const WeaponDef* w = d.Weapon(ld.first);
            if (w != nullptr && w->kind == "gun" && w->pass.present) gun = w;
        }
        const WeaponDef* planned = fl.weapon.empty() ? nullptr : d.Weapon(fl.weapon);
        const bool rockets = planned != nullptr && planned->kind == "rockets" && planned->pass.present && fl.released;
        const double reach = tune.Get("logistics.search_m");
        const double find = ColumnFindChance(p.part, c.weather, a.night, tune) * tune.Get("opportunity.a0_find_scale") * a.opportunity_ease;
        const FrontDef* front = d.theatre.FrontFor(d.faction.front);
        const std::uint64_t pkey = static_cast<std::uint64_t>(p.id) * 64u + oi;
        for (std::size_t i = 0; i < d.theatre.objects.size(); ++i) {
            const TheatreObject& o = d.theatre.objects[i];
            ObjectState& st = (*c.truth)[i];
            if (!IsColumn(o.kind) || st.function <= 0.05 || (st.leg != 1 && st.leg != 3) || o.id == p.target_object) continue;
            if (SegmentDistance2d(st.pos, r0.pos, r1.pos) > reach) continue;
            if (OppDraw(c.opportunity_seed, pkey, i + 1u, 50, 0) >= find) continue;
            OppSituation s;
            s.mission = p.kind;
            s.role = fl.role;
            s.helicopter = a.helicopter;
            s.ease = a.opportunity_ease;
            s.bingo_share = a.bingo_share;
            s.target = OppTarget::Column;
            s.confidence = 0.8;
            s.watched_s = 60.0;
            s.range_m = 1e5;  // the abstract level: room for a pass
            s.moving = true;
            s.depth_km = front != nullptr ? DistanceToLine(front->line, st.pos) / 1000.0 : 0.0;
            const C2FreeHuntDef& fh = d.faction.c2.free_hunt;
            s.free_hunt_zone = fh.from_phase >= 0 && c.phase >= fh.from_phase && s.depth_km <= fh.depth_km;
            s.night = p.part == DayPart::Night;
            s.primary_due = due;
            s.has_gun = gun != nullptr;
            s.has_rockets = rockets;
            s.fuel_share = 0.7;
            s.slot_slack_s = due ? tune.Get("opportunity.slot_slack_s") : 1e9;
            s.speed_mps = a.cruise_mps;
            s.release_m = gun != nullptr ? gun->pass.open_fire_m : (rockets ? planned->pass.open_fire_m : 0.0);
            s.exposure = tune.Get(gun != nullptr ? "delivery.exposure_strafe" : "delivery.exposure_gentle_dive");
            const OppLeader ld = LeaderOf(p, fl);
            const OppDecision dec = RunChain(d.faction.opportunity, s, ld, tune, c.opportunity_seed, pkey, i + 1u);
            PackageOutcome::OpportunityRecord rec;
            rec.flight = static_cast<int>(oi);
            rec.t_h = 0.5 * (r0.t_h + r1.t_h);
            rec.believed = OppTargetName(s.target);
            rec.east_m = st.pos.x;
            rec.north_m = st.pos.y;
            rec.act = OppActName(dec.act);
            rec.stopped = OppStepName(dec.stopped);
            rec.why = dec.why;
            rec.object = o.id;
            rec.truth = o.id;
            rec.misidentified = dec.misidentified;
            if (dec.act == OppAct::Attack || dec.act == OppAct::GunRun) {
                const double before = st.function;
                int lost = 0;
                for (int n = 0; n < alive; ++n) {
                    if (OppDraw(c.opportunity_seed, pkey, i + 1u, 51, static_cast<std::uint64_t>(n)) < tune.Get("opportunity.a0_column_guns")) ++lost;
                }
                st.function = std::max(0.0, st.function - tune.Get("opportunity.a0_column_hit") * alive);
                out.AddDamage(o.id, before - st.function);
                if (st.function <= 0.05 && before > 0.05) {
                    st.function = 0.0;
                    out.destroyed.push_back(o.id);
                }
                for (int n = 0; n < lost; ++n) {
                    LossRecord lr;
                    lr.flight = static_cast<int>(oi);
                    lr.by = o.id;
                    lr.where = st.pos;
                    lr.officer = fl.officer >= 0 && fl.lost == 0;
                    lr.captured = OppDraw(c.opportunity_seed, pkey, i + 1u, 52, static_cast<std::uint64_t>(n)) < 0.5;
                    out.losses.push_back(lr);
                    ++fl.lost;
                }
                rec.passes = 1;
                rec.kills = before - st.function > 0.0 ? 1 : 0;
                rec.claimed = Claim(rec.kills, s.night, OppDraw(c.opportunity_seed, pkey, i + 1u, 53, 0), OppDraw(c.opportunity_seed, pkey, i + 1u, 54, 0), tune);
                out.notes.push_back(fl.callsign + " found " + o.name + " on its way and attacked it");
            } else if (dec.act == OppAct::Report || dec.act == OppAct::ReccePass) {
                rec.reported_in_flight = d.faction.opportunity.report_in_flight;
                if (rec.reported_in_flight) Event(AirEvent::Kind::ColumnFound, rec.t_h, oi, o.id, st.pos, fl.count - fl.lost);
            }
            out.opportunities.push_back(rec);
            return;  // one find a leg
        }
    }

    // The leader of a flight as the chain reads him: its own officer, else the package's leader, else its planner.
    OppLeader LeaderOf(const Package& p, const Flight& fl) const {
        OppLeader ld;
        const int oi = fl.officer >= 0 ? fl.officer : (p.leader >= 0 ? p.leader : p.planner);
        if (c_->staff != nullptr && oi >= 0 && static_cast<std::size_t>(oi) < c_->staff->officers.size()) {
            const Officer& o = c_->staff->officers[static_cast<std::size_t>(oi)];
            ld.traits = o.def.traits;
            ld.press_on = o.HasQuirk("press_on");
            ld.by_the_book = o.HasQuirk("by_the_book");
            ld.loss_shy = o.HasQuirk("loss_shy");
            ld.chases_kills = o.HasQuirk("chases_kills");
            ld.stress = o.stress;
            ld.skill = o.def.skills.airmanship;
        }
        return ld;
    }


    static double TargetTime(const Flight& fl) {
        for (const RoutePoint& r : fl.route) {
            if (r.tag == "target" || r.tag == "orbit") return r.t_h;
        }
        return fl.route.empty() ? 0.0 : fl.route.front().t_h;
    }

    // Armed reconnaissance (logistics.hpp): the flight searches its stretch of road or railway and attacks the first
    // column it sees on the move there, nearest first; a column loading or unloading is inside its depot or with its
    // troops. Nothing found, it brings its weapons home.
    bool FindColumn(Package& p, Flight& fl, std::size_t fi, const AircraftType& a, PackageOutcome& out, double t_h) {
        const AirWarData& d = *c_->data;
        const double reach = d.tuning.Get("logistics.search_m");
        const double find = ColumnFindChance(p.part, c_->weather, a.night, d.tuning);
        std::vector<std::pair<double, std::size_t>> on_road;
        for (std::size_t i = 0; i < d.theatre.objects.size(); ++i) {
            const ObjectState& st = (*c_->truth)[i];
            if (!IsColumn(d.theatre.objects[i].kind) || st.function <= 0.0 || (st.leg != 1 && st.leg != 3)) continue;
            const double dist = Dist2d(st.pos, fl.aim_pos);
            if (dist <= reach) on_road.push_back({dist, i});
        }
        std::sort(on_road.begin(), on_road.end());
        for (const auto& c : on_road) {
            if (!R().Chance(find)) continue;
            const TheatreObject& o = d.theatre.objects[c.second];
            fl.aim_pos = (*c_->truth)[c.second].pos;
            p.target_object = o.id;
            p.found = o.id;
            out.recce_seen.push_back(o.id);
            out.notes.push_back(fl.callsign + " found " + o.name + (o.kind == ObjectKind::Train ? " on the railway" : " on the road"));
            Event(AirEvent::Kind::ColumnFound, t_h, fi, o.id, fl.aim_pos, fl.count - fl.lost);
            return true;
        }
        out.hit_nothing = true;
        out.notes.push_back(fl.callsign + " found nothing on its stretch and brought its weapons home");
        return false;
    }

    void Spend(ObjectState& st, int n, PackageOutcome& out) {
        if (st.missiles < 0) return;
        const int used = std::min(st.missiles, n);
        st.missiles -= used;
        out.missiles_spent += used;
    }

    void FireArms(Flight& fl, const AircraftType& a, const TheatreObject& def, ObjectState& st, int& arms_left,
                  PackageOutcome& out, double& suppression) {
        const TuningTable& tune = c_->data->tuning;
        const int shots = std::min(arms_left, 2);
        arms_left -= shots;
        out.arms_fired += shots;
        // The defenders' discipline: the player's battery by his posture, others by an average crew.
        const double discipline = def.player ? c_->posture->emcon_discipline : 0.5;
        for (int s = 0; s < shots; ++s) {
            const bool switched_off = R().Chance(discipline);
            const double pk = switched_off ? tune.Get(a.arm_memory ? "attrition.arm_kill_memory_off" : "attrition.arm_kill_no_memory_off")
                                           : tune.Get("attrition.arm_kill");
            if (R().Chance(pk)) {
                // One of its fire-control radars is gone; with the last, the battery is out of the fight.
                const double before = st.function;
                st.function = std::max(0.0, st.function - 1.0 / std::max(1, def.defence.radars));
                if (st.function <= 0.05) st.function = 0.0;
                out.AddDamage(def.id, before - st.function);
                out.radars_killed.push_back(def.id);
                if (st.function <= 0.0) {
                    out.destroyed.push_back(def.id);
                    out.notes.push_back(fl.callsign + "'s ARM destroyed the radar of " + def.name);
                    suppression = 1.0;
                    return;
                }
                out.notes.push_back(fl.callsign + "'s ARM destroyed one of the radars of " + def.name);
                suppression = std::max(suppression, 0.7);
                continue;
            }
            if (switched_off) {
                suppression = std::max(suppression, 0.7);
                out.radars_suppressed.push_back(def.id);
            }
        }
    }

    void Deliver(Package& p, Flight& fl, std::size_t flight_index, const AircraftType& a, int alive, double airmanship, PackageOutcome& out,
                 double t_h) {
        const AirWarData& d = *c_->data;
        const TuningTable& tune = d.tuning;
        if (p.kind == MissionKind::ArmedRecce && !a.missile && !FindColumn(p, fl, flight_index, a, out, t_h)) return;
        out.delivered = true;
        // What is really near the aim point: the target where they aimed, a dummy, or nothing.
        int hit_obj = -1;
        double best = 1e18;
        const double stale = tune.Get("strike.stale_miss_m");
        for (std::size_t i = 0; i < d.theatre.objects.size(); ++i) {
            const TheatreObject& o = d.theatre.objects[i];
            if (o.kind == ObjectKind::City) continue;
            const double dist = Dist2d((*c_->truth)[i].pos, fl.aim_pos);
            const double reach = o.decoy ? tune.Get("strike.decoy_area_m") : stale;
            if (dist < reach && dist < best) {
                best = dist;
                hit_obj = static_cast<int>(i);
            }
        }
        double acc = tune.Get("strike.hit_chance") * p.load_factor;
        // Its weapons (docs/air_war/10_WEAPONS.md): how accurately they fall, as delivered now. A guided weapon that
        // can't be guided after all (cloud over the laser spot, darkness for a TV seeker) falls like a bomb from height.
        const WeaponDef* wd = a.missile ? nullptr : d.Weapon(fl.weapon);
        double cep = 0.0;
        if (wd != nullptr) {
            cep = DeliveryCep(*wd, fl.delivery, a, p.part, c_->weather, airmanship, tune);
            const bool night = p.part == DayPart::Night;
            bool lost = false;
            if (wd->guidance == "laser") {
                lost = c_->weather == Weather::Poor || (night && !a.night) ||
                       (c_->weather == Weather::Cloudy && !R().Chance(tune.Get("delivery.cloudy_laser")));
            }
            if (wd->guidance == "tv") lost = night || c_->weather == Weather::Poor;
            if (lost) {
                cep = tune.Get("delivery.cep_level_m") * tune.Get("delivery.poor_weather_factor");
                out.notes.push_back(fl.callsign + "'s " + wd->name + " lost its guidance");
            }
            acc = 1.0 - std::pow(0.5, (wd->footprint_m / cep) * (wd->footprint_m / cep));
        } else if (a.missile) {
            acc = a.hit_chance > 0.0 ? a.hit_chance : tune.Get("strike.hit_chance");  // its own guidance, day or night
        } else {
            if (p.part == DayPart::Night && !a.night) acc *= tune.Get("strike.night_accuracy");
            if (p.part == DayPart::Night) acc *= 0.85;
            if (c_->weather == Weather::Poor) acc *= tune.Get("strike.poor_weather_accuracy");
            if (fl.profile == Profile::Low && !a.helicopter) acc *= 0.9 + 0.1 * tune.Get("strike.toss_accuracy");
        }
        // Collateral: bombs near a town.
        for (const TheatreObject& o : d.theatre.objects) {
            if (o.kind == ObjectKind::City && Dist2d(o.pos, fl.aim_pos) < o.radius_m + tune.Get("strike.collateral_m")) {
                out.collateral += alive * (1.0 - acc) * 0.5;
            }
        }
        if (hit_obj < 0) {
            out.hit_nothing = true;
            out.notes.push_back(fl.callsign + " bombed empty ground: nothing at the aim point");
            return;
        }
        const TheatreObject& o = d.theatre.objects[static_cast<std::size_t>(hit_obj)];
        ObjectState& st = (*c_->truth)[static_cast<std::size_t>(hit_obj)];
        // The guns around the target, on the attack run (not at a missile).
        if (!o.decoy && !o.defence.present && st.function > 0.05 && !a.missile) {
            double env = 1.0;
            if (p.part == DayPart::Night) env *= tune.Get("attrition.night");
            if (c_->weather == Weather::Poor) env *= tune.Get("attrition.poor_weather");
            const double k = TargetGunsChance(o.kind, FlightExposure(fl, a.helicopter, tune), Clamp01(a.evasion + 0.3 * (airmanship - 0.5)),
                                              env * st.function, tune);
            const int attacking = alive;
            for (int n = 0; n < attacking && k > 0.0; ++n) {
                if (!R().Chance(k)) continue;
                LossRecord lr;
                lr.flight = static_cast<int>(flight_index);
                lr.by = o.id;
                lr.where = st.pos;
                lr.officer = fl.officer >= 0 && fl.lost == 0;
                lr.captured = R().Chance(0.5);
                out.losses.push_back(lr);
                ++fl.lost;
                --alive;
            }
            if (alive <= 0) {
                out.notes.push_back(fl.callsign + " was shot down over " + o.name);
                return;
            }
        }
        if (o.decoy) {
            if (st.function > 0.0) {
                st.function = 0.0;
                out.decoys_hit.push_back(o.id);
                out.notes.push_back(fl.callsign + " destroyed a dummy: " + o.name);
            }
            return;
        }
        if (st.function <= 0.05) {
            out.notes.push_back(fl.callsign + " found " + o.name + " already wrecked");
            return;
        }
        if (o.id != p.target_object && !p.target_object.empty()) out.notes.push_back(fl.callsign + " hit " + o.name + " instead");
        double removed = 0.0;
        if (wd != nullptr) {
            // Each weapon on one of the aim points still working; each it puts out of action is gone. Under the fire of
            // a defence that works and isn't suppressed, the aim is worse.
            const TargetProfileDef tp = d.TargetOf(o);
            if (o.defence.present && st.function > 0.3 && !p.Has(FlightRole::Sead)) cep *= tune.Get("delivery.under_fire_factor");
            const double kill = WeaponKill(*wd, fl.delivery, tp.target_class, cep, tune);
            const int aim = std::max(1, tp.aim_points);
            int working = static_cast<int>(std::ceil(st.function * aim - 1e-9));
            const int per_aircraft = std::max(0, static_cast<int>(std::lround(fl.weapons * p.load_factor)));
            int killed = 0;
            for (int n = 0; n < alive && working > 0; ++n) {
                for (int k = 0; k < per_aircraft && working > 0; ++k) {
                    if (!R().Chance(kill)) continue;
                    --working;
                    ++killed;
                }
            }
            removed = static_cast<double>(killed) / aim;
        } else {
            for (int n = 0; n < alive; ++n) {
                if (R().Chance(acc)) removed += a.strike_power * (1.0 - 0.7 * o.hardness);
            }
        }
        if (removed <= 0.0) {
            out.notes.push_back(fl.callsign + " missed " + o.name);
            return;
        }
        const double before = st.function;
        st.function = std::max(0.0, st.function - removed);
        out.AddDamage(o.id, before - st.function);
        if (st.function <= 0.05 && before > 0.05) {
            st.function = 0.0;
            out.destroyed.push_back(o.id);
            out.notes.push_back(fl.callsign + " wrecked " + o.name);
        } else {
            out.notes.push_back(fl.callsign + " damaged " + o.name + " (" + std::to_string(static_cast<int>((before - st.function) * 100.0 + 0.5)) +
                                "%)");
        }
        if (IsAirDefence(o.kind) && st.missiles > 0 && st.function < 0.3) st.missiles = 0;
    }

    void Photograph(const Flight& fl, const AircraftType& a, PackageOutcome& out) {
        const AirWarData& d = *c_->data;
        const double corridor = d.tuning.Get("intel.recce_corridor_m");
        for (std::size_t i = 0; i < d.theatre.objects.size(); ++i) {
            const TheatreObject& o = d.theatre.objects[i];
            const ObjectState& st = (*c_->truth)[i];
            double best = 1e18;
            for (std::size_t s = 0; s + 1 < fl.route.size(); ++s) best = std::min(best, SegmentDistance2d(st.pos, fl.route[s].pos, fl.route[s + 1].pos));
            if (best > corridor) continue;
            double pd = a.recce_quality * (1.0 - best / corridor * 0.5);
            if (o.player && c_->posture->nets) pd *= 0.6;
            if (c_->weather == Weather::Poor) pd *= 0.4;
            if (R().Chance(pd)) out.recce_seen.push_back(o.id);
        }
    }
};

}  // namespace airwar
}  // namespace simcore
