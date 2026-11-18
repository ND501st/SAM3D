// simcore/air_war/opportunity_session.hpp — targets of opportunity in a played raid (roadmap B6,
// docs/air_battle/06_OPPORTUNITY.md §8's level A2).
//
// The mission bridge (mission_bridge.hpp) owns one OpportunityRun. Each of the raid's flights in the session runs its
// leader's chain (opportunity.hpp) every opportunity.tick_s, staggered by a keyed draw:
//   * its contacts are its pilots' ground sightings (ScenarioRunner::PilotPicture, perception.hpp: what their eyes gave
//     them, never the truth), merged across the flight within opportunity.merge_m; a launch's dust or a trail's origin
//     seen at a contact means it fired on them; guns' and missiles' signs seen near it are its threat;
//   * the flight's own target (its aim) is no opportunity;
//   * a report goes on the flight's net and to its posts (the faction's report_in_flight) or waits for the debrief; an
//     attack or a gun run is flown as passes (attack_pass.hpp) by the leader and each wingman with something to fire,
//     in trail; after them the leader may come round again (Reattack).
// It also flies the planned gun and rocket attacks of fixed-wing aircraft as passes (BeginPlanned) in place of the
// release at a waypoint. What a pass did is the bridge's to decide (it holds the truth): Step hands back each pass that
// opened fire, and the bridge answers with what was really hit (NoteResult); the pilots' claims are drawn from that.
// Every draw is keyed (opportunity.hpp's OppDraw: the bridge's seed, the flight, the contact, the step).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "simcore/air_war/attack_pass.hpp"
#include "simcore/air_war/opportunity.hpp"
#include "simcore/air_war/perception.hpp"
#include "simcore/air_war/raid_scenario.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/scenario_runner.hpp"

namespace simcore {
namespace airwar {

class OpportunityRun {
public:
    // A pass that opened fire this step: the bridge decides what it did (NoteResult).
    struct Fire {
        std::size_t aircraft = 0;  // the manifest's
        int contact = -1;          // -1: a planned pass at its briefed aim
        Vec3 aim;                  // where he aimed (believed; the scenario's ENU, on the ground)
        std::string weapon;
        GunPassDef pass;
        bool under_fire = false;
        bool moving = false;       // he believed it moving
        bool first_planned = false;  // the first planned pass of this aircraft: its load's release
    };
    // An in-flight report for the injector (the posts, NoteReport) and the debrief.
    struct Report {
        int flight = -1;
        std::string what;  // OppTargetName
        Vec3 where;        // believed (scenario ENU)
        Vec3 from;         // the reporting aircraft
        double sigma_m = 0.0;
        double t_s = 0.0;
    };

    // A controller's mark (roadmap S2): the FAC's designate, heard by the flight's leader on the air.
    struct Designation {
        std::size_t flight = 0;
        Vec3 where;           // session ENU, on the ground
        double radius_m = 0.0;
        std::string by;
        double t_s = 0.0;
    };

    void Init(const RaidManifest& m, const AirWarData& d, unsigned seed) {
        designations_.clear();
        m_ = &m;
        d_ = &d;
        seed_ = static_cast<std::uint64_t>(seed) * 0x9E3779B97F4A7C15ull + 0xB6ull;
        on_ = d.tuning.Get("opportunity.mode") >= 1.0 && !m.leaders.empty();
        flights_.assign(m.flight_ids.size(), FlightRun{});
        ac_.assign(m.aircraft.size(), AircraftRun{});
        for (std::size_t i = 0; i < m.aircraft.size(); ++i) {
            const RaidAircraft& a = m.aircraft[i];
            AircraftRun& ar = ac_[i];
            const WeaponDef* w = a.weapon.empty() ? nullptr : d.Weapon(a.weapon);
            if (w != nullptr && w->pass.present && !a.helicopter && !a.missile && a.role != FlightRole::Recce) {
                ar.planned_pass = true;
                ar.weapon = a.weapon;
                ar.pass = w->pass;
                ar.left = std::max(1, a.weapons);
                ar.rockets = w->kind == "rockets";
                ar.gun = w->kind == "gun";
            }
            // A gun in the type's loads is built in (the A-10's GAU-8, the Su-25's GSh-30, a helicopter's door guns):
            // whatever it was loaded with, it has its bursts.
            if (const AircraftType* t = d.faction.Aircraft(a.aircraft)) {
                for (const auto& ld : t->loads) {
                    const WeaponDef* g = d.Weapon(ld.first);
                    if (g == nullptr || g->kind != "gun" || !g->pass.present || ar.gun) continue;
                    ar.gun = true;
                    if (!ar.planned_pass) {
                        ar.weapon = g->id;
                        ar.pass = g->pass;
                        ar.left = std::max(1, ld.second);
                    }
                }
            }
        }
        for (std::size_t fi = 0; fi < flights_.size(); ++fi) {
            flights_[fi].next_s = OppDraw(seed_, fi, 0, 0, 0) * d.tuning.Get("opportunity.tick_s");
        }
    }

    bool On() const { return on_; }
    // A contact of `flight`'s within `radius_m` of `where` is designated for its chain (opportunity.hpp MayAttack: "designated
    // by the controller").
    void Designate(std::size_t flight, const Vec3& where, double radius_m, const std::string& by, double t_s) {
        designations_.push_back({flight, where, radius_m, by, t_s});
    }
    bool Designated(std::size_t flight, const Vec3& at) const {
        for (const Designation& d : designations_) {
            if (d.flight == flight && Dist2d(d.where, at) <= d.radius_m) return true;
        }
        return false;
    }
    const std::vector<Designation>& Designations() const { return designations_; }
    // A fixed-wing aircraft whose planned load is a gun or rockets: its attack is flown as passes.
    bool PlannedPass(std::size_t i) const { return on_ && i < ac_.size() && ac_[i].planned_pass; }
    bool PlannedBegun(std::size_t i) const { return i < ac_.size() && ac_[i].planned_begun; }

    // Its planned attack begins (at opportunity.run_in_m from its aim).
    void BeginPlanned(ScenarioRunner& r, std::size_t i, const Vec3& aim) {
        AircraftRun& ar = ac_[i];
        if (ar.planned_begun || ar.left <= 0) return;
        ar.planned_begun = true;
        StartPass(r, i, -1, aim, 0.0, 0.0);
    }

    // Each step after the bridge's own: the flights' chains on their ticks, then the passes. Returns the passes that
    // opened fire now.
    std::vector<Fire> Step(ScenarioRunner& r) {
        std::vector<Fire> fired;
        if (!on_) return fired;
        const double t = r.Time();
        const TuningTable& tune = d_->tuning;
        for (std::size_t fi = 0; fi < flights_.size(); ++fi) {
            FlightRun& fr = flights_[fi];
            if (t < fr.next_s) continue;
            fr.next_s = t + tune.Get("opportunity.tick_s");
            Chain(r, fi);
        }
        for (std::size_t i = 0; i < ac_.size(); ++i) StepPass(r, i, fired);
        return fired;
    }

    // What a pass that opened fire did (the bridge's truth): `kills` things put out of action, `truth` what was there.
    // The pilots' claim is drawn from it (opportunity.hpp Claim), and the leader decides on another pass.
    void NoteResult(const Fire& f, int kills, const std::string& truth) {
        AircraftRun& ar = ac_[f.aircraft];
        const int contact = f.contact;
        const std::uint64_t key = static_cast<std::uint64_t>(contact < 0 ? 0x10000 + f.aircraft : static_cast<std::size_t>(contact));
        const int n = ++ar.fired_n;
        const int claimed = Claim(kills, m_->night, OppDraw(seed_, Flight(f.aircraft), key, 20, static_cast<std::uint64_t>(n)),
                                  OppDraw(seed_, Flight(f.aircraft), key, 21, static_cast<std::uint64_t>(n)), d_->tuning);
        ar.believed_hit = ar.believed_hit || claimed > 0;
        if (contact >= 0 && static_cast<std::size_t>(contact) < contacts_.size()) {
            Contact& c = contacts_[static_cast<std::size_t>(contact)];
            c.believed_hit = c.believed_hit || claimed > 0;
            if (c.record >= 0) {
                PackageOutcome::OpportunityRecord& rec = records_[static_cast<std::size_t>(c.record)];
                rec.claimed += claimed;
                rec.kills += kills;
                if (rec.truth.empty()) rec.truth = truth;
            }
        } else {
            planned_claims_ += claimed;
        }
    }

    const std::vector<PackageOutcome::OpportunityRecord>& Records() const { return records_; }
    // The in-flight reports not yet taken (the injector forwards them to the posts).
    std::vector<Report> TakeReports() {
        std::vector<Report> out;
        out.swap(reports_);
        return out;
    }
    // The planned passes' claims (a planned gun run's target as its pilots saw it).
    int PlannedClaims() const { return planned_claims_; }
    int Passes() const { return passes_; }

private:
    struct FlightRun {
        double next_s = 0.0;
    };
    struct Contact {
        int flight = -1;
        OppTarget target = OppTarget::Unknown;
        Vec3 pos;                 // believed
        double sigma_m = 0.0;
        double confidence = 0.0;
        double first_s = 0.0;
        double last_s = 0.0;
        bool moving = false;
        bool fired_on = false;
        bool done = false;        // decided for good
        bool looked = false;      // a recce pass flown: the next decision is attack or report
        int record = -1;
        int passes = 0;
        bool believed_hit = false;
        int flying = 0;           // passes in the air at it
    };
    struct AircraftRun {
        bool planned_pass = false;
        bool planned_begun = false;
        bool gun = false;
        bool rockets = false;
        std::string weapon;
        GunPassDef pass;
        int left = 0;             // bursts or salvos left
        bool flying = false;
        bool overriding = false;  // its pass is on the override layer
        int contact = -1;
        Vec3 aim;
        AttackPass ap;
        AttackPass::Phase phase = AttackPass::Phase::Done;
        int fired_n = 0;
        bool believed_hit = false;
        bool recce = false;       // a recce pass: no fire
        int passes_planned = 0;
    };

    std::uint64_t Flight(std::size_t aircraft) const { return static_cast<std::uint64_t>(std::max(0, m_->aircraft[aircraft].flight)); }

    // The leader's chain on his flight's picture.
    void Chain(ScenarioRunner& r, std::size_t fi) {
        const RaidManifest& m = *m_;
        if (fi >= m.flight_ids.size() || m.flight_ids[fi].empty() || fi >= m.leaders.size()) return;
        const FlightState* fs = r.Flight(m.flight_ids[fi]);
        if (fs == nullptr) return;
        const FlightSlot* ls = fs->Leader();
        if (ls == nullptr || !ls->alive || ls->gone || ls->target < 0 || static_cast<std::size_t>(ls->target) >= r.Def().targets.size()) return;
        const std::string leader = r.Def().targets[static_cast<std::size_t>(ls->target)].id;
        if (!r.TargetAlive(leader)) return;
        // The flight's members in the manifest.
        std::vector<std::size_t> members;
        int ref = -1;
        for (std::size_t i = 0; i < m.aircraft.size(); ++i) {
            if (m.aircraft[i].flight != static_cast<int>(fi)) continue;
            members.push_back(i);
            if (m.aircraft[i].target_id == leader) ref = static_cast<int>(i);
        }
        if (members.empty() || ref < 0) return;
        const RaidAircraft& la = m.aircraft[static_cast<std::size_t>(ref)];
        if (la.missile || la.drone || la.role == FlightRole::Missile || la.role == FlightRole::Decoy || la.role == FlightRole::Jammer) return;
        const double t = r.Time();
        const TuningTable& tune = d_->tuning;
        // What his flight's pilots see on the ground, and the threats they have seen.
        std::vector<Vec3> threats;
        for (std::size_t i : members) {
            const std::string& id = m.aircraft[i].target_id;
            if (!r.TargetAlive(id)) continue;
            for (const Sighting& sg : r.PilotPicture(id)) {
                const double age = t - sg.time_s;
                if (sg.kind == SightKind::LaunchSign || sg.kind == SightKind::Trail || sg.kind == SightKind::Burst || sg.kind == SightKind::Tracer) {
                    if (age <= tune.Get("opportunity.threat_age_s")) threats.push_back(sg.has_origin ? sg.origin_enu : sg.believed_enu);
                }
                if (sg.kind == SightKind::GroundObject || sg.kind == SightKind::LaunchSign || (sg.kind == SightKind::Trail && sg.has_origin)) {
                    Merge(static_cast<int>(fi), sg);
                }
            }
        }
        // His own target is no opportunity.
        const double own_aim_m = 1500.0;
        const Vec3 lpos = r.TargetPosition(leader);
        const Vec3 lvel = r.TargetVelocity(leader);
        for (std::size_t k = 0; k < contacts_.size(); ++k) {
            Contact& c = contacts_[k];
            if (c.flight != static_cast<int>(fi) || c.done || c.flying > 0) continue;
            bool own = false;
            for (std::size_t i : members) {
                if (m.aircraft[i].has_release && Dist2d(m.aircraft[i].aim_enu, c.pos) < own_aim_m) own = true;
            }
            if (own) {
                c.done = true;
                continue;
            }
            OppSituation s = Situation(r, fi, members, ref, c, threats, lpos, lvel);
            const OppLeader& ld = m.leaders[fi];
            OppDecision dec = RunChain(d_->faction.opportunity, s, ld, tune, seed_, fi, static_cast<std::uint64_t>(k) + 1u);
            if (c.looked && dec.act == OppAct::ReccePass) dec.act = OppAct::Report;
            if (dec.stopped == OppStep::Seen || dec.stopped == OppStep::Identified) continue;  // keep looking
            if (dec.too_close) continue;  // a later look from further out may let him
            if (dec.misidentified && c.target != OppTarget::Battery) c.target = OppTarget::Battery;  // takes it for what it is not
            Decide(r, fi, members, k, dec, s);
        }
    }

    // A sighting into the flight's contacts.
    void Merge(int fi, const Sighting& sg) {
        const Vec3 at = sg.kind == SightKind::Trail ? sg.origin_enu : sg.believed_enu;
        const bool fired = sg.kind == SightKind::LaunchSign || sg.kind == SightKind::Trail;
        const OppTarget tg = fired ? OppTarget::Battery : OppTargetOf(sg.what);
        const double merge = d_->tuning.Get("opportunity.merge_m");
        for (Contact& c : contacts_) {
            if (c.flight != fi || Dist2d(c.pos, at) > merge) continue;
            c.confidence = std::max(c.confidence, sg.confidence);
            c.last_s = std::max(c.last_s, sg.time_s);
            c.first_s = std::min(c.first_s, sg.first_s);
            c.fired_on = c.fired_on || fired;
            if (!fired) {
                c.pos = at;
                c.sigma_m = sg.range_sigma_m;
                c.moving = sg.what == "column_moving";
                if (c.target == OppTarget::Unknown) c.target = tg;
            }
            return;
        }
        Contact c;
        c.flight = fi;
        c.target = tg;
        c.pos = at;
        c.sigma_m = sg.range_sigma_m;
        c.confidence = sg.confidence;
        c.first_s = sg.first_s;
        c.last_s = sg.time_s;
        c.moving = sg.what == "column_moving";
        c.fired_on = fired;
        contacts_.push_back(c);
    }

    OppSituation Situation(const ScenarioRunner& r, std::size_t fi, const std::vector<std::size_t>& members, int ref, const Contact& c,
                           const std::vector<Vec3>& threats, const Vec3& lpos, const Vec3& lvel) const {
        const RaidManifest& m = *m_;
        const TuningTable& tune = d_->tuning;
        const RaidAircraft& la = m.aircraft[static_cast<std::size_t>(ref)];
        const AircraftType* type = d_->faction.Aircraft(la.aircraft);
        OppSituation s;
        s.mission = m.mission;
        s.role = la.role;
        s.helicopter = la.helicopter;
        s.ease = type != nullptr ? type->opportunity_ease : 0.3;
        s.bingo_share = type != nullptr ? type->bingo_share : 0.3;
        s.target = c.target;
        s.confidence = c.confidence;
        s.watched_s = r.Time() - c.first_s;
        s.range_m = Dist2d(lpos, c.pos);
        s.moving = c.moving;
        s.fired_on = c.fired_on;
        s.designated = Designated(fi, c.pos);
        s.depth_km = m.depth_km;
        s.free_hunt_zone = m.free_hunt;
        s.night = m.night;
        bool due = false, gun = false, rockets = false;
        double release_m = 0.0;
        for (std::size_t i : members) {
            const RaidAircraft& a = m.aircraft[i];
            if (!r.TargetAlive(a.target_id)) continue;
            const AircraftRun& ar = ac_[i];
            const bool owed = a.has_release && i < released_.size() && !released_[i];  // its load is for the target still
            due = due || owed;
            if (ar.left > 0 && ar.gun && (!ar.planned_pass || !owed)) gun = true;
            if (ar.left > 0 && ar.rockets && !owed) rockets = true;
            if (ar.left > 0) release_m = std::max(release_m, ar.pass.open_fire_m);
            if (const AircraftDamage* dm = r.TargetDamage(a.target_id)) s.damaged = s.damaged || (i == static_cast<std::size_t>(ref) && dm->level != DamageLevel::None);
        }
        s.primary_due = due;
        s.has_gun = gun;
        s.has_rockets = rockets;
        s.has_bombs = false;  // a first form: bombs only on the planned target (the planned release)
        const double span = std::max(600.0, m.end_s - la.appear_s);
        s.fuel_share = 1.0 - 0.6 * Clamp01((r.Time() - la.appear_s) / span);
        s.slot_slack_s = (due || la.role == FlightRole::Escort) ? tune.Get("opportunity.slot_slack_s") : 1e9;
        s.speed_mps = std::max(50.0, lvel.HorizontalLength());
        s.release_m = release_m > 0.0 ? release_m : tune.Get("opportunity.min_release_m");
        int seen_firing = 0;
        for (const Vec3& th : threats) seen_firing += Dist2d(th, c.pos) <= tune.Get("opportunity.threat_radius_m") ? 1 : 0;
        s.threat = Clamp01(0.5 * seen_firing);
        s.exposure = tune.Get(gun && !rockets ? "delivery.exposure_strafe" : "delivery.exposure_gentle_dive");
        s.passes = c.passes;
        s.believed_hit = c.believed_hit;
        (void)fi;
        return s;
    }

    // The leader's decision: the record, the report, the passes.
    void Decide(ScenarioRunner& r, std::size_t fi, const std::vector<std::size_t>& members, std::size_t k, const OppDecision& dec,
                const OppSituation& s) {
        const RaidManifest& m = *m_;
        Contact& c = contacts_[k];
        if (c.record < 0) {
            PackageOutcome::OpportunityRecord rec;
            rec.flight = static_cast<int>(fi);
            rec.t_h = m.start_h + r.Time() / 3600.0;
            rec.believed = OppTargetName(c.target);
            rec.east_m = c.pos.x;
            rec.north_m = c.pos.y;
            rec.misidentified = dec.misidentified;
            c.record = static_cast<int>(records_.size());
            records_.push_back(rec);
        }
        PackageOutcome::OpportunityRecord& rec = records_[static_cast<std::size_t>(c.record)];
        rec.act = OppActName(dec.act);
        rec.stopped = OppStepName(dec.stopped);
        rec.why = dec.why;
        if (dec.act == OppAct::Report || dec.act == OppAct::Ignore) {
            c.done = true;
            if (dec.act == OppAct::Report && d_->faction.opportunity.report_in_flight) {
                rec.reported_in_flight = true;
                Report rp;
                rp.flight = static_cast<int>(fi);
                rp.what = OppTargetName(c.target);
                rp.where = c.pos;
                rp.sigma_m = std::max(c.sigma_m, d_->tuning.Get("opportunity.report_sigma_m"));
                rp.t_s = r.Time();
                for (std::size_t i : members) {
                    if (r.TargetAlive(m.aircraft[i].target_id)) {
                        rp.from = r.TargetPosition(m.aircraft[i].target_id);
                        break;
                    }
                }
                reports_.push_back(rp);
                if (AirRadio* radio = r.AirRadio()) {
                    radio->SayAs(m.flight_ids[fi], CallKind::Report, std::string(OppTargetName(c.target)) + " on the ground, reporting it", r.Time(), true, c.pos);
                }
            }
            return;
        }
        // A pass at it by every member with something to fire, the leader first, the rest in trail.
        const bool recce = dec.act == OppAct::ReccePass;
        c.looked = c.looked || recce;
        int place = 0;
        for (std::size_t i : members) {
            AircraftRun& ar = ac_[i];
            if (!r.TargetAlive(m.aircraft[i].target_id) || ar.flying) continue;
            if (!recce && (ar.left <= 0 || (!ar.gun && !ar.rockets))) continue;
            if (!recce && ar.planned_pass && !ar.planned_begun && m.aircraft[i].has_release && ar.rockets) continue;  // rockets owed to the target
            const double delay = place * d_->tuning.Get("opportunity.trail_m") / std::max(50.0, s.speed_mps);
            const double off = c.passes > 0 ? DegToRad(d_->tuning.Get("opportunity.reattack_heading_deg")) : 0.0;
            StartPass(r, i, static_cast<int>(k), c.pos, off, delay);
            ar.recce = recce;
            ++place;
            if (recce) break;  // the leader looks; the flight holds on
        }
        if (place == 0) {
            c.done = true;
            rec.act = OppActName(OppAct::Report);
            rec.why = "nobody left to fire";
        }
    }

    void StartPass(ScenarioRunner& r, std::size_t i, int contact, const Vec3& aim, double heading_off, double delay_s) {
        AircraftRun& ar = ac_[i];
        const RaidAircraft& a = m_->aircraft[i];
        const TuningTable& tune = d_->tuning;
        const Vec3 pos = r.TargetPosition(a.target_id);
        const Vec3 vel = r.TargetVelocity(a.target_id);
        const double run_in = heading_off != 0.0 ? tune.Get("opportunity.reattack_separation_m") : tune.Get("opportunity.run_in_m");
        ar.ap.Begin(pos, vel, aim, ar.pass, run_in, tune.Get("opportunity.recover_m"), heading_off, r.Time(), delay_s);
        ar.flying = true;
        ar.overriding = false;
        ar.contact = contact;
        ar.aim = aim;
        ar.recce = false;
        ar.phase = ar.ap.GetPhase();
        if (contact >= 0) ++contacts_[static_cast<std::size_t>(contact)].flying;
    }

    void EndPass(ScenarioRunner& r, std::size_t i) {
        AircraftRun& ar = ac_[i];
        const RaidAircraft& a = m_->aircraft[i];
        if (ar.overriding) r.EndPass(a.target_id);
        ar.flying = false;
        ar.overriding = false;
        if (ar.contact >= 0) {
            Contact& c = contacts_[static_cast<std::size_t>(ar.contact)];
            c.flying = std::max(0, c.flying - 1);
            if (c.flying == 0) AfterPasses(r, static_cast<std::size_t>(ar.contact));
        } else if (ar.left > 0 && r.TargetAlive(a.target_id)) {
            // A planned gun or rocket attack: round again at the briefed aim?
            ++ar.passes_planned;
            OppSituation s;
            s.passes = ar.passes_planned;
            s.has_gun = ar.gun;
            s.has_rockets = ar.rockets;
            s.believed_hit = ar.believed_hit;
            s.fuel_share = 0.6;
            s.bingo_share = 0.3;
            const OppLeader& ld = m_->leaders[static_cast<std::size_t>(std::max(0, std::min(a.flight, static_cast<int>(m_->leaders.size()) - 1)))];
            const double u = OppDraw(seed_, Flight(i), 0x20000 + i, 30, static_cast<std::uint64_t>(ar.passes_planned));
            if (Reattack(d_->faction.opportunity, s, ld, d_->tuning, u)) {
                StartPass(r, i, -1, ar.aim, DegToRad(d_->tuning.Get("opportunity.reattack_heading_deg")), 0.0);
            }
        }
        ar.contact = ar.flying ? ar.contact : -1;
    }

    // The flight's passes at a contact are over: the leader decides on another.
    void AfterPasses(ScenarioRunner& r, std::size_t k) {
        Contact& c = contacts_[k];
        const bool was_recce = c.looked && c.passes == 0 && (c.record < 0 || records_[static_cast<std::size_t>(c.record)].act == "recce_pass");
        if (was_recce) {
            c.confidence = std::max(c.confidence, 0.8);  // he has had a good look: the chain again, attack or report
            return;
        }
        ++c.passes;
        if (c.record >= 0) records_[static_cast<std::size_t>(c.record)].passes = c.passes;
        const RaidManifest& m = *m_;
        const std::size_t fi = static_cast<std::size_t>(c.flight);
        std::vector<std::size_t> members;
        int ref = -1;
        const FlightState* fs = fi < m.flight_ids.size() ? r.Flight(m.flight_ids[fi]) : nullptr;
        const FlightSlot* ls = fs != nullptr ? fs->Leader() : nullptr;
        const std::string leader = ls != nullptr && ls->target >= 0 && static_cast<std::size_t>(ls->target) < r.Def().targets.size()
                                       ? r.Def().targets[static_cast<std::size_t>(ls->target)].id
                                       : std::string();
        for (std::size_t i = 0; i < m.aircraft.size(); ++i) {
            if (m.aircraft[i].flight != static_cast<int>(fi)) continue;
            members.push_back(i);
            if (m.aircraft[i].target_id == leader) ref = static_cast<int>(i);
        }
        if (ref < 0 || !r.TargetAlive(leader) || fi >= m.leaders.size()) {
            c.done = true;
            return;
        }
        const OppSituation s = Situation(r, fi, members, ref, c, std::vector<Vec3>(), r.TargetPosition(leader), r.TargetVelocity(leader));
        const double u = OppDraw(seed_, fi, static_cast<std::uint64_t>(k) + 1u, 10, static_cast<std::uint64_t>(c.passes));
        if (!Reattack(d_->faction.opportunity, s, m.leaders[fi], d_->tuning, u)) {
            c.done = true;
            return;
        }
        if (c.record >= 0) records_[static_cast<std::size_t>(c.record)].act = OppActName(OppAct::Reattack);
        OppDecision dec;
        dec.act = s.has_gun && !s.has_rockets ? OppAct::GunRun : OppAct::Attack;
        dec.stopped = OppStep::Act;
        dec.why = "comes round again";
        Decide(r, fi, members, k, dec, s);
        if (c.record >= 0) records_[static_cast<std::size_t>(c.record)].act = OppActName(OppAct::Reattack);
    }

    // One aircraft's pass, after the step.
    void StepPass(ScenarioRunner& r, std::size_t i, std::vector<Fire>& fired) {
        AircraftRun& ar = ac_[i];
        if (!ar.flying) return;
        const RaidAircraft& a = m_->aircraft[i];
        const double t = r.Time();
        if (!r.TargetAlive(a.target_id) || r.StoresJettisoned(a.target_id)) {
            ar.ap.Abandon();
            EndPass(r, i);
            return;
        }
        if (t < ar.ap.StartS()) return;  // a wingman in trail
        if (!ar.overriding) {
            if (!r.FlyPass(a.target_id, ar.ap.Override(t))) {  // breaking or diving under fire: no pass now
                ar.ap.Abandon();
                EndPass(r, i);
                return;
            }
            ar.overriding = true;
            ++passes_;
        }
        const bool reached = r.PassOf(a.target_id) == nullptr;
        const Vec3 pos = r.TargetPosition(a.target_id);
        const Vec3 vel = r.TargetVelocity(a.target_id);
        const Vec3 aim_ground = r.GroundEnu(ar.aim.x, ar.aim.y);
        const bool fire = ar.ap.Step(pos, vel, aim_ground, reached, t) && !ar.recce && ar.left > 0;
        if (fire) {
            --ar.left;
            Fire f;
            f.aircraft = i;
            f.contact = ar.contact;
            f.aim = aim_ground;
            f.weapon = ar.weapon;
            f.pass = ar.pass;
            f.moving = ar.contact >= 0 && contacts_[static_cast<std::size_t>(ar.contact)].moving;
            f.first_planned = ar.contact < 0 && !PlannedFired(i);
            if (f.first_planned) planned_fired_.push_back(i);
            f.under_fire = UnderFire(r, a.target_id, aim_ground);
            fired.push_back(f);
        }
        if (ar.ap.Done()) {
            EndPass(r, i);
            return;
        }
        if (reached || ar.ap.GetPhase() != ar.phase) {
            ar.phase = ar.ap.GetPhase();
            if (!r.FlyPass(a.target_id, ar.ap.Override(t))) {
                ar.ap.Abandon();
                EndPass(r, i);
            }
        }
    }

    bool PlannedFired(std::size_t i) const { return std::find(planned_fired_.begin(), planned_fired_.end(), i) != planned_fired_.end(); }

    // Under fire: a launch, a trail, a burst or tracer seen within the threat radius of the aim lately.
    bool UnderFire(const ScenarioRunner& r, const std::string& id, const Vec3& aim) const {
        const TuningTable& tune = d_->tuning;
        for (const Sighting& sg : r.PilotPicture(id)) {
            if (sg.kind != SightKind::LaunchSign && sg.kind != SightKind::Trail && sg.kind != SightKind::Burst && sg.kind != SightKind::Tracer) continue;
            if (r.Time() - sg.time_s > tune.Get("opportunity.threat_age_s")) continue;
            const Vec3 at = sg.has_origin ? sg.origin_enu : sg.believed_enu;
            if (Dist2d(at, aim) <= tune.Get("opportunity.threat_radius_m")) return true;
        }
        return false;
    }

public:
    // The bridge's word on which aircraft have released their planned loads (for "primary due").
    void SetReleased(const std::vector<bool>& released) { released_ = released; }

private:
    const RaidManifest* m_ = nullptr;
    const AirWarData* d_ = nullptr;
    std::uint64_t seed_ = 0;
    bool on_ = false;
    std::vector<FlightRun> flights_;
    std::vector<AircraftRun> ac_;
    std::vector<Contact> contacts_;
    std::vector<PackageOutcome::OpportunityRecord> records_;
    std::vector<Report> reports_;
    std::vector<bool> released_;
    std::vector<std::size_t> planned_fired_;
    int planned_claims_ = 0;
    int passes_ = 0;
    std::vector<Designation> designations_;
};

}  // namespace airwar
}  // namespace simcore
