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

#include "simcore/air_war/data.hpp"
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
    // Only the part of the routes from this hour on (the rest of a raid after the player's mission).
    double from_h = -1e18;
};

class Resolver {
public:
    PackageOutcome Resolve(Package& p, ResolveContext& c) {
        c_ = &c;
        PackageOutcome out;
        out.package_id = p.id;
        out.truth_applied = true;
        const AirWarData& d = *c.data;
        const TuningTable& tune = d.tuning;
        const FactionDef& f = d.faction;
        const FrontDef* front = d.theatre.FrontFor(f.front);
        const int sead = p.Count(FlightRole::Sead) + p.Count(FlightRole::Hunter);
        const bool jam = p.Has(FlightRole::Jammer);
        const Officer* so = c.staff->HolderPtr(StaffRole::Sead);
        const double sead_skill = so != nullptr ? so->Effective(so->def.skills.sead) : 0.3;
        const double supp_base = tune.Get("attrition.sead_suppression") * std::min(1.0, sead / 2.0) * (0.6 + 0.4 * sead_skill) *
                                 (0.5 + 0.5 * p.coordination);
        const Officer* leader = p.leader >= 0 ? &c.staff->officers[static_cast<std::size_t>(p.leader)] : nullptr;
        const double airmanship = leader != nullptr ? leader->Effective(leader->def.skills.airmanship) : 0.5;
        // The leader's nerve: the share of the package he will lose before turning back.
        double nerve = 0.25 + 0.3 * (leader != nullptr ? leader->def.traits.boldness : 0.5);
        if (const Officer* pln = p.planner >= 0 ? &c.staff->officers[static_cast<std::size_t>(p.planner)] : nullptr) {
            nerve += 0.1 * pln->Quirk("abort_threshold");
        }
        if (leader != nullptr) nerve += 0.15 * leader->Quirk("abort_threshold");
        nerve = ClampTo(nerve, 0.05, 0.9);
        const int package_size = std::max(1, p.Aircraft());
        std::vector<double> suppressed(d.theatre.objects.size(), 0.0);  // 0..1 how much each radar was suppressed
        std::vector<bool> engaged(d.theatre.objects.size(), false);
        bool abort_now = false;

        // Decoys and SEAD go first in time; walk flights in order of their time over the target.
        std::vector<std::size_t> order(p.flights.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return TargetTime(p.flights[a]) < TargetTime(p.flights[b]);
        });
        for (std::size_t oi : order) {
            Flight& fl = p.flights[oi];
            const AircraftType* a = f.Aircraft(fl.aircraft);
            if (a == nullptr || fl.count <= 0) continue;
            int alive = fl.count - fl.lost;
            const bool main = fl.role == FlightRole::Strike || fl.role == FlightRole::Dead || fl.role == FlightRole::Helicopter;
            int arms_left = fl.arms;
            for (std::size_t si = 0; si + 1 < fl.route.size() && alive > 0; ++si) {
                const RoutePoint& r0 = fl.route[si];
                const RoutePoint& r1 = fl.route[si + 1];
                if (r1.t_h < c.from_h) continue;
                const bool before_release = fl.release_index < 0 || static_cast<int>(si) < fl.release_index;
                if (main && abort_now && before_release) {
                    fl.aborted = true;
                    break;
                }
                for (std::size_t oj = 0; oj < d.theatre.objects.size(); ++oj) {
                    const TheatreObject& def = d.theatre.objects[oj];
                    ObjectState& st = (*c.truth)[oj];
                    if (!def.defence.present || st.function <= 0.05 || def.decoy) continue;
                    if (!c.skip_defence.empty() && def.id == c.skip_defence) continue;
                    if (st.missiles == 0) continue;
                    const double chord = ChordInCircle2d(r0.pos, r1.pos, st.pos, def.defence.radius_m);
                    if (chord <= 0.0) continue;
                    const bool radar = def.defence.radar;
                    const bool emcon = def.player ? c.posture->fight_silent : false;
                    // A radar defence radiates while it engages (not under EMCON): ELINT hears it; SEAD fires at it.
                    if (radar && !emcon) {
                        out.AddEmission(def.id, chord / std::max(50.0, a->cruise_mps) + 30.0);
                        if (arms_left > 0 && (fl.role == FlightRole::Sead || fl.role == FlightRole::Hunter)) {
                            const double dist = SegmentDistance2d(st.pos, r0.pos, r1.pos);
                            if (dist <= a->arm_range_m) {
                                FireArms(fl, *a, def, st, arms_left, out, suppressed[oj]);
                            }
                        }
                    }
                    if (a->drone && fl.role == FlightRole::Decoy) {
                        // A decoy draws fire: the defence wastes rounds and shows itself.
                        if (!engaged[oj]) {
                            engaged[oj] = true;
                            Spend(st, 1 + c.rng->Index(2), out);
                            out.fired.push_back(def.id);
                            if (c.rng->Chance(0.6)) ++fl.lost;  // drones are meant to be lost; not counted as losses
                        }
                        continue;
                    }
                    double env = 1.0;
                    if (!radar && p.part == DayPart::Night) env *= tune.Get("attrition.night");
                    if (!radar && c.weather == Weather::Poor) env *= tune.Get("attrition.poor_weather");
                    if (emcon) env *= tune.Get("attrition.emcon_lethality");
                    double supp = radar && !emcon ? std::max(supp_base, suppressed[oj]) : 0.0;
                    if (st.function < 1.0) env *= st.function;
                    const double k = PassKillChance(def.defence.lethality, chord, def.defence.radius_m, fl.profile, a->helicopter,
                                                    def.defence.low_cover, def.defence.medium_cover, radar, supp,
                                                    jam && radar ? tune.Get("attrition.jamming") : 0.0, env,
                                                    Clamp01(a->evasion + 0.3 * (airmanship - 0.5)), tune);
                    if (k <= 0.0) continue;
                    engaged[oj] = true;
                    int shots = 0;
                    for (int n = 0; n < alive; ++n) {
                        ++shots;
                        if (c.rng->Chance(k)) {
                            LossRecord lr;
                            lr.flight = static_cast<int>(oi);
                            lr.by = def.id;
                            double along = 0.0;
                            SegmentDistance2d(st.pos, r0.pos, r1.pos, &along);
                            lr.where = r0.pos + (r1.pos - r0.pos) * along;
                            lr.officer = fl.officer >= 0 && fl.lost == 0;
                            lr.captured = c.rng->Chance(0.5);
                            out.losses.push_back(lr);
                            ++fl.lost;
                        } else if (c.rng->Chance(0.5 * k)) {
                            ++fl.damaged;
                            ++out.damaged;
                        }
                    }
                    alive = fl.count - fl.lost;
                    if (shots > 0) {
                        Spend(st, std::min(shots, 2), out);
                        out.fired.push_back(def.id);
                    }
                }
                // The defender's fighters, where the package has no escort.
                if (front != nullptr && main && si == 1 && alive > 0) {
                    const double pk = front->friendly_fighters * tune.Get("attrition.escort_fighter_kill") *
                                      (p.Has(FlightRole::Escort) ? 0.3 : 1.0) * (p.part == DayPart::Night ? 0.5 : 1.0);
                    for (int n = 0; n < alive; ++n) {
                        if (c.rng->Chance(pk)) {
                            LossRecord lr;
                            lr.flight = static_cast<int>(oi);
                            lr.where = r1.pos;
                            lr.officer = fl.officer >= 0 && fl.lost == 0;
                            lr.captured = c.rng->Chance(0.5);
                            out.losses.push_back(lr);
                            ++fl.lost;
                        }
                    }
                    alive = fl.count - fl.lost;
                }
                // The leader's nerve.
                if (main && before_release && static_cast<double>(out.Lost()) / package_size >= nerve && !abort_now) {
                    abort_now = true;
                    out.aborted = true;
                    out.notes.push_back("the package turned back after losing " + std::to_string(out.Lost()) + " of " +
                                        std::to_string(package_size));
                }
                // Weapons at the release point.
                if (static_cast<int>(si + 1) == fl.release_index && alive > 0 && !fl.aborted && !fl.released) {
                    if (main) {
                        Deliver(p, fl, oi, *a, alive, airmanship, out);
                        alive = fl.count - fl.lost;
                    }
                    if (fl.role == FlightRole::Recce) Photograph(fl, *a, out);
                    fl.released = true;
                }
            }
            if (fl.role == FlightRole::Recce && !fl.released && fl.release_index < 0 && fl.lost < fl.count) Photograph(fl, *a, out);
        }
        p.resolved = true;
        return out;
    }

private:
    ResolveContext* c_ = nullptr;

    static double TargetTime(const Flight& fl) {
        for (const RoutePoint& r : fl.route) {
            if (r.tag == "target" || r.tag == "orbit") return r.t_h;
        }
        return fl.route.empty() ? 0.0 : fl.route.front().t_h;
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
            const bool switched_off = c_->rng->Chance(discipline);
            const double pk = switched_off ? (a.arm_memory ? tune.Get("attrition.arm_kill") * 0.5 : tune.Get("attrition.arm_kill_no_memory_off"))
                                           : tune.Get("attrition.arm_kill");
            if (c_->rng->Chance(pk)) {
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

    void Deliver(Package& p, Flight& fl, std::size_t flight_index, const AircraftType& a, int alive, double airmanship, PackageOutcome& out) {
        const AirWarData& d = *c_->data;
        const TuningTable& tune = d.tuning;
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
        if (p.part == DayPart::Night && !a.night) acc *= tune.Get("strike.night_accuracy");
        if (p.part == DayPart::Night) acc *= 0.85;
        if (c_->weather == Weather::Poor) acc *= tune.Get("strike.poor_weather_accuracy");
        if (fl.profile == Profile::Low && !a.helicopter) acc *= 0.9 + 0.1 * tune.Get("strike.toss_accuracy");
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
        // The guns around the target, on the attack run.
        if (!o.decoy && !o.defence.present && st.function > 0.05) {
            double env = 1.0;
            if (p.part == DayPart::Night) env *= tune.Get("attrition.night");
            if (c_->weather == Weather::Poor) env *= tune.Get("attrition.poor_weather");
            const double k = TargetGunsChance(o.kind, fl.profile, a.helicopter, Clamp01(a.evasion + 0.3 * (airmanship - 0.5)),
                                              env * st.function, tune);
            const int attacking = alive;
            for (int n = 0; n < attacking && k > 0.0; ++n) {
                if (!c_->rng->Chance(k)) continue;
                LossRecord lr;
                lr.flight = static_cast<int>(flight_index);
                lr.by = o.id;
                lr.where = st.pos;
                lr.officer = fl.officer >= 0 && fl.lost == 0;
                lr.captured = c_->rng->Chance(0.5);
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
        for (int n = 0; n < alive; ++n) {
            if (c_->rng->Chance(acc)) removed += a.strike_power * (1.0 - 0.7 * o.hardness);
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
            if (c_->rng->Chance(pd)) out.recce_seen.push_back(o.id);
        }
    }
};

}  // namespace airwar
}  // namespace simcore
