// simcore/air_war/package_injector.hpp — an enemy package entering a mission already running (roadmap 15.3,
// docs/campaign/05_THE_SYSTEMS.md §6-7).
//
// The raid builder (raid_scenario.hpp) makes a package into a scenario of its own: its frame at the battery's campaign
// position, its clock at the package's start. To bring the same package into a running session instead, the injector
// loads that scenario with the scenario loader, so its aircraft carry everything a played raid's do (routes,
// behaviour, warning receivers, anti-radiation missiles), moves them into the session's frame through the geodetic and
// onto its clock (entering at `at_s`), and adds them to the runner (ScenarioRunner::AddTargets) with the raid's EM
// gear (its library and jammers: AddEmGear) and its warning (the air-raid warning's trigger, a C2 source it has put
// out: AddTriggers). The raid's manifest is moved alike, with our vehicle and the battery where the session has them,
// so a MissionBridge on it reads the session as it would the raid's own scenario.
//
// LivePackages runs the air war in step with a session: the campaign advanced to the session's clock, each package it
// hands to the player's battery taken (AirCampaign::TakePlayerMission), injected and followed by its bridge, and its
// outcome handed back when it is over. The campaign goes on meanwhile (roadmap B5): other packages fly, its posts
// decide, and the played package's pilots' sightings go to them as in-flight reports.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/air_war/comms_jamming.hpp"
#include "simcore/air_war/mission_bridge.hpp"
#include "simcore/air_war/package_control.hpp"
#include "simcore/air_war/raid_scenario.hpp"
#include "simcore/geo.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

namespace simcore {
namespace airwar {

struct InjectedPackage {
    bool ok = false;
    std::string error;
    RaidManifest manifest;           // in the session's frame and on its clock
    double at_s = 0.0;               // the session second the raid's own t = 0 falls on
    std::size_t aircraft = 0;        // added to the session
    std::size_t jammers = 0;         // EM jammers added with them
    std::size_t triggers = 0;        // its warning and the C2 sources it has put out
    std::vector<std::string> notes;  // what of the raid did not come in, and why
};

// A point of one local frame in another (both near the battery: through the geodetic, so heights stay heights).
inline Vec3 MoveFrame(const LocalFrame& from, const LocalFrame& to, const Vec3& enu) { return to.GeodeticToEnu(from.EnuToGeodetic(enu)); }

// The manifest of a raid built as a scenario, moved into a session: `from` the raid scenario's frame, `to` the
// session's, its clock `at_s` later. Our vehicle and the battery's others stand where the session has them.
inline RaidManifest MoveManifest(const RaidManifest& m, const LocalFrame& from, const LocalFrame& to, double at_s,
                                 const ScenarioRunner& r) {
    RaidManifest out = m;
    const auto move = [&](Vec3& p) { p = MoveFrame(from, to, p); };
    out.origin = to.Origin();
    out.start_h = m.start_h - at_s / 3600.0;
    out.end_s = m.end_s + at_s;
    out.own_enu = r.UnitPosition();
    if (const Battery* b = r.GetBattery()) {
        out.battery.clear();
        const Battery::Vehicle* own = b->Own();
        for (const Battery::Vehicle& v : b->Vehicles()) {
            if (own != nullptr && v.def.id == own->def.id) continue;
            out.battery.push_back({v.def.kind == BatteryVehicleKind::CommandPost ? std::string("PU-12") : v.def.id, v.position});
        }
    } else {
        for (auto& v : out.battery) move(v.second);
    }
    for (auto& d : out.dummies) move(d.second);
    for (auto& o : out.objects) move(o.second);
    for (RaidNeighbour& nb : out.neighbours) move(nb.enu);
    for (RaidAircraft& a : out.aircraft) {
        move(a.release_enu);
        move(a.aim_enu);
        a.appear_s += at_s;
    }
    return out;
}

// Brings the package of `rs` into the session `r`, its t = 0 at the session's `at_s` (now or later). Fails, changing
// nothing, if its scenario does not load or an aircraft's id is the session's already. Its EM gear needs the session's
// EM layer and its warning a free trigger id: without them the aircraft still come, and `notes` says what did not.
inline InjectedPackage InjectPackage(ScenarioRunner& r, const RaidScenario& rs, const std::string& data_dir, double at_s) {
    InjectedPackage out;
    out.at_s = at_s;
    if (!rs.ok) {
        out.error = "the raid was not built: " + rs.error;
        return out;
    }
    ScenarioDef raid;
    ConsoleData console;
    std::string e;
    if (!LoadScenario(rs.json_text, data_dir, raid, console, &e)) {
        out.error = "the raid's scenario does not load: " + e;
        return out;
    }
    for (const ScenarioTarget& t : raid.targets) {
        for (const ScenarioTarget& o : r.Def().targets) {
            if (o.id != t.id) continue;
            out.error = "there is a target '" + t.id + "' in the session already";
            return out;
        }
    }
    const LocalFrame from(raid.origin);
    const LocalFrame to(r.Def().origin);
    char who[48];
    std::snprintf(who, sizeof(who), "package %d", rs.manifest.package_id);
    // Its EM gear first: a warning receiver is made from the library when its aircraft appears.
    if (raid.has_em) {
        std::vector<EmJammerDef> js = raid.em.jammers;
        for (EmJammerDef& j : js) {
            if (j.target_id.empty()) j.position = MoveFrame(from, to, j.position);
            j.on_at_s += at_s;
            if (j.off_at_s >= 0.0) j.off_at_s += at_s;
        }
        if (r.AddEmGear(raid.em.library, js, who, &e)) {
            out.jammers = js.size();
        } else {
            out.notes.push_back("its EM gear (" + std::to_string(js.size()) + " jammers) stays out: " + e);
        }
    }
    std::vector<ScenarioTarget> ts;
    for (ScenarioTarget t : raid.targets) {
        t.start = MoveFrame(from, to, t.start);
        for (Vec3& w : t.waypoints) w = MoveFrame(from, to, w);
        for (RouteLeg& l : t.legs) l.to = MoveFrame(from, to, l.to);
        if (t.arms.has_aim) t.arms.aim = MoveFrame(from, to, t.arms.aim);
        for (ProgrammeZone& z : t.reactions.zones) z.centre = MoveFrame(from, to, z.centre);
        t.appear_at_s += at_s;
        ts.push_back(std::move(t));
    }
    // Its flights and its package as the session keeps them (flights.hpp, roadmap B1), before the aircraft that name them.
    if (!r.AddFlights(raid.flights, raid.packages, &e)) {
        out.error = e;
        return out;
    }
    // Its pilots' reaction tables and R1's damage states, if the session had none (roadmap B4).
    if (raid.reactions.loaded) r.UseReactionTable(raid.reactions);
    if (raid.has_damage) r.UseDamageStates(raid.damage);
    if (!r.AddTargets(std::move(ts), who, &e)) {
        out.error = e;
        return out;
    }
    out.aircraft = raid.targets.size();
    std::vector<ScenarioTrigger> trs;
    for (ScenarioTrigger t : raid.triggers) {
        t.id = std::string(who) + " " + t.id;
        t.after_s += at_s;
        trs.push_back(std::move(t));
    }
    if (!trs.empty()) {
        const std::size_t n = trs.size();
        if (r.AddTriggers(std::move(trs), &e)) {
            out.triggers = n;
        } else {
            out.notes.push_back("its warning stays out: " + e);
        }
    }
    if (!raid.deliveries.empty()) out.notes.push_back(std::to_string(raid.deliveries.size()) + " messages of the pipeline are not played");
    out.manifest = MoveManifest(rs.manifest, from, to, at_s, r);
    out.ok = true;
    return out;
}

// The air war run in step with a session. The campaign is advanced to the session's clock every `every_s`; a package it
// hands to the player's battery is taken from it, built as a raid and enters the session on its own time (at once if that
// is past), its bridge follows it, and once it is over (MissionBridge::Over) and its last load still falling has landed (a
// package over by its clock may still have bombs in the air: they land in the session at their own times) the
// campaign takes its outcome (AirCampaign::CompleteLiveMission). The campaign never stands still for it (roadmap B5):
// another package that comes due meanwhile enters beside it. What the package's pilots see of our launches goes to the
// enemy's posts as their in-flight reports (AirCampaign::NoteReport), said on the flight's net where the session has the
// enemy's radio; and the session's flights take orders from those posts only by the faction's authority table
// (package_control.hpp).
class LivePackages {
public:
    struct Played {
        int package_id = 0;
        std::string name;
        double in_s = 0.0;   // its t = 0 in the session
        double out_s = -1.0; // handed back to the campaign (< 0: still in the session)
        std::size_t aircraft = 0;
        std::size_t jammers = 0;
        int lost = 0;
        int reports = 0;                   // its pilots' sightings sent to the posts
        std::vector<int> release_flights;  // its flights whose release point was in the session (indices into its flights)
        PackageOutcome outcome;            // as handed back to the campaign (once out)
    };

    // `h_at_zero`: the campaign's hour at the session's t = 0.
    LivePackages(AirCampaign& c, std::string data_dir, double h_at_zero, ScenarioFormat fmt = ScenarioFormat(), double every_s = 30.0)
        : c_(&c), data_dir_(std::move(data_dir)), h0_(h_at_zero), fmt_(std::move(fmt)), every_s_(every_s) {}

    // After every runner.Step().
    void Step(ScenarioRunner& r) {
        for (std::size_t k = 0; k < live_.size();) {
            if (Follow(r, live_[k])) {
                live_.erase(live_.begin() + static_cast<std::ptrdiff_t>(k));
                next_s_ = r.Time();
            } else {
                ++k;
            }
        }
        if (r.Time() < next_s_ - 1e-9) return;
        next_s_ = r.Time() + every_s_;
        while (c_->Advance(HourOf(r.Time()))) {
            const Package& p = *c_->PlayerMission();
            const int id = p.id;
            const std::string name = p.name;
            const RaidScenario rs = RaidScenarioBuilder().Build(*c_, p, fmt_);
            if (!rs.ok) {
                notes_.push_back("package " + std::to_string(id) + " resolved off the scene: " + rs.error);
                c_->SkipPlayerMission();
                continue;
            }
            const double at_s = std::max(r.Time(), (rs.manifest.start_h - h0_) * 3600.0);
            InjectedPackage inj = InjectPackage(r, rs, data_dir_, at_s);
            if (!inj.ok) {
                notes_.push_back("package " + std::to_string(id) + " resolved off the scene: " + inj.error);
                c_->SkipPlayerMission();
                continue;
            }
            for (const std::string& n : inj.notes) notes_.push_back("package " + std::to_string(id) + ": " + n);
            JamTheBatteryNet(r, *c_->PlayerMission());
            c_->TakePlayerMission();  // flown here; the campaign goes on
            Authority(r);
            Live l;
            l.bridge = std::make_unique<MissionBridge>(inj.manifest, c_->Data(), r.Def().seed ^ static_cast<unsigned>(id));
            l.played = played_.size();
            Played pl;
            pl.package_id = id;
            pl.name = name;
            pl.in_s = at_s;
            pl.aircraft = inj.aircraft;
            pl.jammers = inj.jammers;
            for (const RaidAircraft& a : inj.manifest.aircraft) {
                if (a.has_release && a.flight >= 0 &&
                    std::find(pl.release_flights.begin(), pl.release_flights.end(), a.flight) == pl.release_flights.end()) {
                    pl.release_flights.push_back(a.flight);
                }
            }
            played_.push_back(pl);
            live_.push_back(std::move(l));
        }
    }

    // What our intercept posts logged between two session times, as they wrote it (roadmap B9: the session owns the desk).
    using HeardSource = std::function<std::vector<HeardRecord>(double from_s, double to_s)>;
    void SetHeardSource(HeardSource f) { heard_source_ = std::move(f); }

    bool Active() const { return !live_.empty(); }
    // The bridge of the package in the session longest (null: none).
    const MissionBridge* Bridge() const { return live_.empty() ? nullptr : live_.front().bridge.get(); }
    std::size_t InSession() const { return live_.size(); }

    // The battery's report of a package from its records since it came in (roadmap 18.1): what our side's journal says
    // of it. Without a post (the Osa's crew writes no journal) nothing: our side takes the truth.
    static void ReportedFromRecords(const ScenarioRunner& r, double from_s, PackageOutcome& o) {
        campaign::CombatReport rep;
        if (!r.ReportFromRecords(rep, "The battery", from_s)) return;
        o.reported = true;
        o.reported_destroyed = rep.destroyed;
        o.reported_missiles = rep.missiles;
        o.reported_targets = static_cast<int>(rep.targets.size());
    }
    const std::vector<Played>& PlayedPackages() const { return played_; }
    const std::vector<std::string>& Notes() const { return notes_; }  // packages that could not come in, gear left out
    double HourOf(double t_s) const { return h0_ + t_s / 3600.0; }

private:
    struct Live {
        std::unique_ptr<MissionBridge> bridge;
        bool landing = false;      // the package is over; its loads still falling land before it is handed back
        std::size_t played = 0;    // index into played_
        std::size_t sighted = 0;   // its bridge's sightings already sent
    };

    // One package's step in the session; true once it is handed back.
    bool Follow(ScenarioRunner& r, Live& l) {
        if (!l.landing) {
            l.bridge->Step(r);
            Report(r, l);
            if (!l.bridge->Over(r)) return false;
            l.landing = true;
        } else {
            l.bridge->LandFalling(r);
        }
        // Over by its clock with loads still falling: they land in the session at their own times, and the package
        // is handed back once the last has (MissionBridge::FallingUntil).
        if (r.Time() < l.bridge->FallingUntil()) return false;
        Played& pl = played_[l.played];
        PackageOutcome o = l.bridge->Finish(r);
        ReportedFromRecords(r, pl.in_s, o);
        if (heard_source_) o.heard = heard_source_(pl.in_s, r.Time());  // what our intercept posts logged (B9)
        c_->CompleteLiveMission(pl.package_id, o, HourOf(r.Time()));
        pl.out_s = r.Time();
        pl.lost = o.Lost();
        pl.outcome = o;
        return true;
    }

    // A pilot of the package saw a launch (B3's sightings): his flight reports it to the posts (05 §4.7), from where he is,
    // dated when he saw it; said on his flight's net where the session has the enemy's radio (B9).
    void Report(ScenarioRunner& r, Live& l) {
        const std::vector<PackageOutcome::LaunchSighting>& seen = l.bridge->Sightings();
        const LocalFrame session(r.Def().origin), theatre(c_->Data().theatre.origin);
        for (; l.sighted < seen.size(); ++l.sighted) {
            const PackageOutcome::LaunchSighting& s = seen[l.sighted];
            const int flight = l.bridge->FlightOfTarget(s.by);
            if (flight < 0) continue;  // another package's pilot
            Played& pl = played_[l.played];
            const Vec3 at_s{s.east_m, s.north_m, 0.0};
            Vec3 at = MoveFrame(session, theatre, at_s);
            at.z = 0.0;
            const Vec3 from = MoveFrame(session, theatre, r.TargetPosition(s.by));
            c_->NoteReport(pl.package_id, flight, "launch", at, s.sigma_m, HourOf(s.time_s), from);
            ++pl.reports;
            if (AirRadio* radio = r.AirRadio()) {
                const RaidManifest& m = l.bridge->Manifest();
                if (flight < static_cast<int>(m.flight_ids.size()) && !m.flight_ids[static_cast<std::size_t>(flight)].empty()) {
                    radio->SayAs(m.flight_ids[static_cast<std::size_t>(flight)], CallKind::Report, "launch, reporting the site", r.Time(), true, at_s);
                }
            }
        }
        // What its flights found on the ground and reported in flight (roadmap B6, opportunity_session.hpp; said on their
        // nets there already): to the posts, as the launches.
        for (const OpportunityRun::Report& rp : l.bridge->TakeOpportunityReports()) {
            Played& pl = played_[l.played];
            Vec3 at = MoveFrame(session, theatre, rp.where);
            at.z = 0.0;
            c_->NoteReport(pl.package_id, rp.flight, rp.what, at, rp.sigma_m, HourOf(rp.t_s), MoveFrame(session, theatre, rp.from));
            ++pl.reports;
        }
    }

    // Who may order the session's enemy flights: their own command, and the campaign's posts by the faction's table, a
    // post's own area judged where the flight's leader is (in the theatre's frame).
    void Authority(ScenarioRunner& r) {
        if (authority_set_ || !c_->Data().faction.c2.present) return;
        authority_set_ = true;
        AirCampaign* c = c_;
        ScenarioRunner* rp = &r;
        const PackageControl::MissionOf mission_of = [c](const FlightState& f) {
            const Package* p = c->PackageById(SessionPackageId(f.package));
            return p != nullptr ? C2MissionOf(*p, c->Data()) : std::string("any");
        };
        const PackageControl::PostKind kind_of = [c](const std::string& by) {
            const C2PostState* p = c->C2().Post(by);
            return p != nullptr ? p->kind : std::string();
        };
        const PackageControl::WhereOf where_of = [c, rp](const std::string& by, const FlightState& f, const FlightOrder& o) {
            C2Where w;
            const C2PostState* p = c->C2().Post(by);
            if (p == nullptr) return w;
            Vec3 at;
            if (o.has_aim) {
                at = o.aim;
            } else if (const FlightSlot* s = f.Leader()) {
                if (s->target >= 0 && static_cast<std::size_t>(s->target) < rp->Def().targets.size()) at = rp->TargetPosition(rp->Def().targets[static_cast<std::size_t>(s->target)].id);
            }
            const Vec3 t = MoveFrame(LocalFrame(rp->Def().origin), LocalFrame(c->Data().theatre.origin), at);
            return c->C2().WhereFor(*p, t, c->Phase());
        };
        r.SetFlightAuthority(PackageControl::Rule(c_->Data(), kind_of, mission_of, where_of));
    }

    // The faction's jammers of our voice nets with the package (comms_jamming.hpp), against the battery where the session
    // has it: each that reaches the battery net is on it on the package's times; the rest are noted with why not.
    void JamTheBatteryNet(ScenarioRunner& r, const Package& p) {
        const RadioNetDef* net = r.Def().radio_data.Find("battery");
        const Battery* b = r.GetBattery();
        if (net == nullptr || b == nullptr || c_->Data().faction.comms_jamming.empty()) return;
        std::vector<Vec3> launchers;
        Vec3 centre;
        for (const Battery::Vehicle& v : b->Vehicles()) {
            if (v.def.kind != BatteryVehicleKind::Launcher) continue;
            launchers.push_back(v.position);
            centre += v.position;
        }
        if (launchers.empty()) return;
        centre = centre / static_cast<double>(launchers.size());
        JammedBattery jb;
        const LocalFrame session(r.Def().origin), theatre(c_->Data().theatre.origin);
        jb.centre = MoveFrame(session, theatre, centre);
        jb.centre.z = 0.0;
        jb.post = r.UnitPosition() - centre;
        for (const Vec3& l : launchers) jb.receivers.push_back(l - centre);
        jb.net_mhz = net->frequencies_mhz;
        const std::vector<VoiceJammer> jammers = VoiceJammersFor(c_->Data(), p, jb);
        const std::string tag = "package " + std::to_string(p.id) + ": ";
        for (const VoiceJammer& v : jammers) notes_.push_back(tag + v.name + " on the battery net: " + v.why);
        for (const VoiceJammer& v : jammers) {
            for (const JammingDef& j : VoiceJammingDefs(c_->Data(), p, {v}, h0_)) r.AddNetJamming(j, tag + v.name);
        }
    }

    HeardSource heard_source_;
    AirCampaign* c_;
    std::string data_dir_;
    double h0_ = 0.0;
    ScenarioFormat fmt_;
    double every_s_ = 30.0;
    double next_s_ = 0.0;
    std::vector<Live> live_;  // the packages in the session now
    bool authority_set_ = false;
    std::vector<Played> played_;
    std::vector<std::string> notes_;
};

}  // namespace airwar
}  // namespace simcore
