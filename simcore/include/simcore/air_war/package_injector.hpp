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
// hands to the player's battery injected and followed by its bridge, and its outcome handed back when it is over.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/air_war/mission_bridge.hpp"
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
        if (t.arms.has_aim) t.arms.aim = MoveFrame(from, to, t.arms.aim);
        t.appear_at_s += at_s;
        ts.push_back(std::move(t));
    }
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
// hands to the player's battery is built as a raid and enters the session on its own time (at once if that is past),
// its bridge follows it, and once it is over (MissionBridge::Over) the campaign takes its outcome and goes on. One
// package at a time, as the campaign keeps one player mission pending: another that comes due meanwhile waits for it.
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
    };

    // `h_at_zero`: the campaign's hour at the session's t = 0.
    LivePackages(AirCampaign& c, std::string data_dir, double h_at_zero, ScenarioFormat fmt = ScenarioFormat(), double every_s = 30.0)
        : c_(&c), data_dir_(std::move(data_dir)), h0_(h_at_zero), fmt_(std::move(fmt)), every_s_(every_s) {}

    // After every runner.Step().
    void Step(ScenarioRunner& r) {
        if (bridge_) {
            bridge_->Step(r);
            if (!bridge_->Over(r)) return;
            const PackageOutcome o = bridge_->Finish(r);
            c_->CompletePlayerMission(o, HourOf(r.Time()));
            played_.back().out_s = r.Time();
            played_.back().lost = o.Lost();
            bridge_.reset();
            next_s_ = r.Time();
            return;
        }
        if (r.Time() < next_s_ - 1e-9) return;
        next_s_ = r.Time() + every_s_;
        while (c_->Advance(HourOf(r.Time()))) {
            const Package& p = *c_->PlayerMission();
            const RaidScenario rs = RaidScenarioBuilder().Build(*c_, p, fmt_);
            if (!rs.ok) {
                notes_.push_back("package " + std::to_string(p.id) + " resolved off the scene: " + rs.error);
                c_->SkipPlayerMission();
                continue;
            }
            const double at_s = std::max(r.Time(), (rs.manifest.start_h - h0_) * 3600.0);
            InjectedPackage inj = InjectPackage(r, rs, data_dir_, at_s);
            if (!inj.ok) {
                notes_.push_back("package " + std::to_string(p.id) + " resolved off the scene: " + inj.error);
                c_->SkipPlayerMission();
                continue;
            }
            for (const std::string& n : inj.notes) notes_.push_back("package " + std::to_string(p.id) + ": " + n);
            bridge_ = std::make_unique<MissionBridge>(inj.manifest, c_->Data(), r.Def().seed ^ static_cast<unsigned>(p.id));
            Played pl;
            pl.package_id = p.id;
            pl.name = p.name;
            pl.in_s = at_s;
            pl.aircraft = inj.aircraft;
            pl.jammers = inj.jammers;
            played_.push_back(pl);
            return;
        }
    }

    bool Active() const { return bridge_ != nullptr; }
    const MissionBridge* Bridge() const { return bridge_.get(); }
    const std::vector<Played>& PlayedPackages() const { return played_; }
    const std::vector<std::string>& Notes() const { return notes_; }  // packages that could not come in, gear left out
    double HourOf(double t_s) const { return h0_ + t_s / 3600.0; }

private:
    AirCampaign* c_;
    std::string data_dir_;
    double h0_ = 0.0;
    ScenarioFormat fmt_;
    double every_s_ = 30.0;
    double next_s_ = 0.0;
    std::unique_ptr<MissionBridge> bridge_;
    std::vector<Played> played_;
    std::vector<std::string> notes_;
};

}  // namespace airwar
}  // namespace simcore
