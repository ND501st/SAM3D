// simcore/air_war/mission_bridge.hpp — a raid played as a scenario, brought back into the campaign
// (docs/air_war/07_INTEGRATION.md).
//
// The runner (scenario_runner.hpp) flies the raid's aircraft, the crew fights them and the engagement model decides
// the shots. What the runner doesn't do, the bridge does, from the runner's public state each step:
//   * weapon release: a strike, DEAD or helicopter aircraft alive at its release point drops its load; it lands where
//     it was aimed (where the enemy believed the target was, scattered) after its weapon's own flight from the
//     aircraft's height over the ground it lands on, its speed and its path angle (weapon_flight.hpp: a laydown in a few
//     seconds, a level release from 4 km in half a minute; until roadmap FX every load took a fixed
//     `scenario.weapon_fall_s`, 15 s, which is still the time of a load whose weapon is unknown), and hurts what is
//     really there: our vehicle, the battery's other vehicles, a dummy, the defended object (game values in
//     data/gameplay/air_war.json). A load that falls (bombs, dispensers, guided bombs) goes where the aircraft's state
//     throws it, so it is released where that throw reaches the aim (Releases); rockets, missiles and guns, and a weapon
//     not known, within `scenario.release_capture_m` of the release point. Loads still falling when the session ends
//     land in Finish();
//   * anti-radiation missiles: with the EM layer a SEAD or hunter aircraft with a warning receiver carries EM ARMs
//     (the target's "arms", em/arm.hpp): the runner fires and flies them, and the bridge reads their ends (the vehicle
//     hit, a dummy taken, a miss) from ScenarioRunner::ArmResults (roadmap A11). Without (no EM layer, no warning
//     receiver) the gameplay model: in range while our search or tracking radar radiates it fires one; it flies at
//     `scenario.arm_speed_mps` and hits if the radar still radiates when it arrives, or radiated within the seeker's
//     memory if it has one;
//   * the neighbours: our other air defences whose zones reach into the scenario (RaidManifest::neighbours) take their
//     kill chances at the aircraft passing through, by the abstract resolver's rule (its pass chance, fire channels,
//     rounds, the package's SEAD and jamming, night and weather for the optical ones); an aircraft they hit goes down in
//     the scenario, half way through their zone (17.1's first form). The regiment's other Osa batteries fight in the
//     runner's battery fire model instead (ScenarioRunner::NeighbourFights): their kills, missiles and radar time are
//     read from it. Each kill is reported up and the regiment tells the battery on its command net
//     (ScenarioRunner::RegimentTellsDestroyed): the post wipes the mark when it hears it;
//   * what the enemy learns: every second our radars radiate (ELINT), every launch an enemy aircraft sees.
// Every event goes into the runner's log, so the debrief shows it. Finish() turns it all into a PackageOutcome for
// AirCampaign::CompletePlayerMission.
// What it decides it also shows, in the runner's presentation feed (fx/events.hpp, docs/fx/README.md 3.1): the loads'
// release and impact, a cruise missile's warhead, its ARMs' launch and burst, the neighbours' missiles and gun passes.
// The feed changes nothing; where to draw a miss is drawn from a generator of its own, never the bridge's; with the
// runner's feed switched off (ScenarioRunner::SetFxOn) it pushes and draws nothing.
//   * targets of opportunity (roadmap B6, opportunity_session.hpp): its flights' leaders run their chains on what their
//     pilots see, report and fly passes; a fixed-wing aircraft's planned gun or rocket attack is flown as passes too, in
//     place of the release at a waypoint. What a pass that opened fire did is decided here, on what is really near its
//     aim (PassFired), by keyed draws of their own (the bridge's draws fall as without them).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "simcore/air_war/opportunity_session.hpp"
#include "simcore/air_war/raid_scenario.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/air_war/weapon_flight.hpp"
#include "simcore/fx/events.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/scenery/munitions.hpp"

namespace simcore {
namespace airwar {

class MissionBridge {
public:
    MissionBridge(const RaidManifest& m, const AirWarData& d, unsigned seed)
        : m_(m), d_(&d), rng_(seed * 2654435761u + 99u), fx_rng_(seed + kFxSeed) {
        state_.assign(m_.aircraft.size(), AircraftState{});
        for (std::size_t i = 0; i < m_.aircraft.size(); ++i) state_[i].arms_left = m_.aircraft[i].arms;
        passes_.assign(m_.aircraft.size() * m_.neighbours.size(), Pass{});
        neighbours_.assign(m_.neighbours.size(), NeighbourState{});
        for (std::size_t j = 0; j < m_.neighbours.size(); ++j) neighbours_[j].missiles = m_.neighbours[j].missiles;
        kill_by_.assign(m_.aircraft.size(), std::string());
        opp_.Init(m_, d, seed);
        opp_seed_ = static_cast<std::uint64_t>(seed) * 0xC2B2AE3D27D4EB4Full + 0xB6F1ull;
    }
    MissionBridge(const MissionBridge&) = delete;  // its opportunity run holds its manifest
    MissionBridge& operator=(const MissionBridge&) = delete;

    // Call after every runner.Step().
    void Step(ScenarioRunner& r) {
        const TuningTable& tune = d_->tuning;
        const double t = r.Time();
        const double dt = t - last_t_;
        last_t_ = t;
        if (unit_destroyed_) return;
        const ConsoleData& con = r.Console();
        const bool search = con.equipment.IsReady(con.search.antenna) && con.equipment.IsReady(con.search.transmitter);
        const bool track = con.equipment.IsReady(con.track.transmitter);
        const bool radiating = search || track;
        if (radiating && dt > 0.0) {
            emitted_s_ += dt;
            last_radiating_t_ = t;
        }
        // Launches the enemy sees: a launch is seen when some pilot's picture holds its dust and flash or its smoke trail
        // (roadmap B3: the eyes, the light, the weather and the line of sight; never a distance rule).
        if (r.PerceivedLaunches() > 0) launch_seen_ = true;
        if (const std::vector<std::pair<std::string, Sighting>>* seen = r.LaunchSightings()) {
            for (std::size_t k = sightings_taken_; k < seen->size(); ++k) {
                const Sighting& sg = (*seen)[k].second;
                PackageOutcome::LaunchSighting ls;
                ls.time_s = sg.time_s;
                ls.east_m = (sg.has_origin ? sg.origin_enu : sg.believed_enu).x;
                ls.north_m = (sg.has_origin ? sg.origin_enu : sg.believed_enu).y;
                ls.sigma_m = std::max(300.0, sg.range_sigma_m);
                ls.confidence = sg.confidence;
                ls.by = (*seen)[k].first;
                sightings_.push_back(ls);
            }
            sightings_taken_ = seen->size();
        }
        for (std::size_t i = 0; i < m_.aircraft.size(); ++i) {
            const RaidAircraft& a = m_.aircraft[i];
            AircraftState& s = state_[i];
            if (!r.TargetAlive(a.target_id)) continue;
            const Vec3 pos = r.TargetPosition(a.target_id);
            s.prev_pos = s.seen ? s.last_pos : pos;
            s.last_pos = pos;
            s.seen = true;
            // A cruise missile at its target: its warhead goes off where it arrives, and it is gone.
            // (Within the release capture: the runner retires a target at its last waypoint before it is quite there.)
            if (a.missile && a.has_release && !s.released && Dist2d(pos, a.release_enu) < tune.Get("scenario.release_capture_m")) {
                s.released = true;
                s.impacted = true;
                Impact imp;
                imp.t = t;
                imp.aircraft = static_cast<int>(i);
                const double hit = a.hit_chance > 0.0 ? a.hit_chance : tune.Get("strike.hit_chance");
                const double scatter = 20.0 + 300.0 * (1.0 - hit) * rng_.Uniform();
                const double ang = rng_.Uniform(0.0, 6.283185307179586);
                imp.where = a.aim_enu + Vec3{std::cos(ang) * scatter, std::sin(ang) * scatter, 0.0};
                impacts_.push_back(imp);
                FxCruiseImpact(r, a, pos, imp.where);
                r.DestroyTarget(a.target_id, "its own warhead", false);  // no wreck: its warhead is the CruiseImpact
                Log(r, a.target_id, "strikes its target");
                continue;
            }
            // A fixed-wing aircraft's planned gun or rocket attack: flown as passes from run_in_m out (roadmap B6); its
            // first burst or salvo is its release (PassFired).
            const bool flown = opp_.PlannedPass(i);
            if (flown && a.has_release && !s.released && !a.missile && !r.StoresJettisoned(a.target_id) && !opp_.PlannedBegun(i) &&
                Dist2d(pos, a.aim_enu) < tune.Get("opportunity.run_in_m")) {
                opp_.BeginPlanned(r, i, a.aim_enu);
            }
            // Weapons at the release point.
            // (A load thrown off under fire is gone: nothing left to release, roadmap B4.)
            if (!flown && a.has_release && !s.released && !a.missile && !r.StoresJettisoned(a.target_id) && Releases(r, a, s, pos)) {
                s.released = true;
                r.NoteReleased(a.target_id);
                if (a.role == FlightRole::Recce) {
                    photographed_ = true;
                    Log(r, a.target_id, "photographs the area");
                } else {
                    Impact imp;
                    imp.aircraft = static_cast<int>(i);
                    const double scatter = 120.0 + 200.0 * rng_.Uniform();
                    const double ang = rng_.Uniform(0.0, 6.283185307179586);
                    imp.where = a.aim_enu + Vec3{std::cos(ang) * scatter, std::sin(ang) * scatter, 0.0};
                    // It lands after its weapon's own flight from here (weapon_flight.hpp): from the aircraft's height over
                    // the ground where it lands, its speed and its own path angle; an unknown weapon after the scenario's
                    // fixed fall.
                    const Vec3 ground = r.GroundEnu(imp.where.x, imp.where.y);
                    const Vec3 vel = r.TargetVelocity(a.target_id);
                    const std::string delivery = DeliveryId(a.delivery);
                    const ReleaseState st = ReleaseStateOf(r.GetWorld().Frame(), pos, vel, delivery);
                    const double agl = r.GetWorld().Frame().AltitudeOf(pos) - r.GroundHeightAt(imp.where.x, imp.where.y);
                    const double flight = a.weapon.empty() ? -1.0
                                                           : WeaponFlightOf(delivery, a.weapon, agl, st.speed_mps, st.path_deg,
                                                                            (ground - pos).Length(), &d_->weapons).time_s;
                    imp.t = t + (flight > 0.0 ? flight : tune.Get("scenario.weapon_fall_s"));
                    impacts_.push_back(imp);
                    FxRelease(r, a, pos, vel, ground, imp.t);
                    Log(r, a.target_id, a.helicopter ? "fires its rockets" : "releases its bombs");
                }
            }
            // Anti-radiation missiles at a radiating radar.
            if (!a.em_arms && s.arms_left > 0 && radiating && t - s.last_arm_t > 10.0) {
                const double range = Dist2d(pos, m_.own_enu);
                if (range < a.arm_range_m) {
                    --s.arms_left;
                    s.last_arm_t = t;
                    Arm arm;
                    arm.t = t + range / tune.Get("scenario.arm_speed_mps");
                    arm.aircraft = static_cast<int>(i);
                    arm.memory = a.arm_memory;
                    arms_.push_back(arm);
                    FxArmLaunch(r, a, pos, arm.t);
                    ++arms_fired_;
                    Log(r, a.target_id, "fires an anti-radiation missile at the radar (" + std::to_string(static_cast<int>(range / 1000.0 + 0.5)) + " km)");
                }
            }
        }
        // Targets of opportunity and the passes flown (roadmap B6).
        if (opp_.On()) {
            std::vector<bool> released(state_.size(), false);
            for (std::size_t i = 0; i < state_.size(); ++i) released[i] = state_[i].released;
            opp_.SetReleased(released);
            for (const OpportunityRun::Fire& f : opp_.Step(r)) {
                PassFired(r, f);
                if (unit_destroyed_) return;
            }
        }
        Neighbours(r);
        FxGunShows(r);  // the time-fuzed guns' shells fired this step (roadmap S4)
        ReadEmArms(r);
        if (unit_destroyed_) return;
        // Weapons arriving.
        LandDue(r, t);
        if (unit_destroyed_) return;
        for (Arm& arm : arms_) {
            if (arm.done || arm.t > t) continue;
            arm.done = true;
            const bool on = radiating || (arm.memory && t - last_radiating_t_ < tune.Get("scenario.arm_memory_s"));
            const RaidAircraft& carrier = m_.aircraft[static_cast<std::size_t>(arm.aircraft)];
            const std::string who = carrier.target_id;
            if (!on) {
                radar_silenced_ = true;
                FxArmBurst(r, carrier, false);  // wide of where the radar was when it went silent
                Log(r, who, "ARM: the radar went silent, the missile lost it");
                continue;
            }
            if (rng_.Chance(tune.Get("scenario.arm_pk"))) {
                radar_killed_ = true;
                unit_destroyed_ = true;
                FxArmBurst(r, carrier, true);
                Log(r, who, "ARM: hit the vehicle's radar - our vehicle is out of action");
            } else {
                FxArmBurst(r, carrier, false);
                Log(r, who, "ARM: missed");
            }
        }
    }

    bool UnitDestroyed() const { return unit_destroyed_; }
    // The raid as it came into the session, and the launches its pilots have seen so far (roadmap B5: each goes to the
    // enemy's posts as an in-flight report, LivePackages).
    const RaidManifest& Manifest() const { return m_; }
    const std::vector<PackageOutcome::LaunchSighting>& Sightings() const { return sightings_; }
    // The package's flight (index into its flights) an aircraft of the session flies in, or -1 (not this raid's).
    int FlightOfTarget(const std::string& target_id) const {
        for (const RaidAircraft& a : m_.aircraft) {
            if (a.target_id == target_id) return a.flight;
        }
        return -1;
    }
    // Is the package done in the session: every aircraft of it appeared and gone (down, or past its last waypoint at the
    // scenario's circle), its bombs and missiles landed; or the session past the raid's end, or our vehicle destroyed?
    // A package that entered a running mission is then finished and handed back (package_injector.hpp, roadmap 15.3).
    bool Over(const ScenarioRunner& r) const {
        if (unit_destroyed_ || r.Time() >= m_.end_s) return true;
        for (const RaidAircraft& a : m_.aircraft) {
            if (!r.TargetAppeared(a.target_id) || r.TargetAlive(a.target_id)) return false;
        }
        for (const Impact& imp : impacts_)
            if (!imp.done) return false;
        for (const Arm& arm : arms_)
            if (!arm.done) return false;
        return r.ArmsInFlight().empty();
    }
    // When the last load still falling lands (< 0: none falling, or our vehicle out of action, after which Finish lands
    // nothing more). A package over by the clock (Over: the session past the raid's end) can still have loads in the
    // air: a session that runs on lands them at their own times (LandFalling) before it hands the package back
    // (LivePackages), where Finish would land them at once, dated ahead of the session.
    double FallingUntil() const {
        double until = -1.0;
        if (unit_destroyed_) return until;
        for (const Impact& imp : impacts_) {
            if (!imp.done) until = std::max(until, imp.t);
        }
        return until;
    }
    // The loads whose time has come land now, as Step lands them; nothing else of the raid moves on (no release, no
    // ARM, no neighbour's pass). Call after every runner.Step() while a package that is over waits for its loads.
    void LandFalling(ScenarioRunner& r) {
        last_t_ = r.Time();
        if (!unit_destroyed_) LandDue(r, r.Time());
    }
    double EmittedSeconds() const { return emitted_s_; }
    // Targets of opportunity (roadmap B6): the flights' in-flight reports not yet taken (the injector sends them to the
    // posts), and what the leaders did so far.
    std::vector<OpportunityRun::Report> TakeOpportunityReports() { return opp_.TakeReports(); }
    const OpportunityRun& Opportunities() const { return opp_; }
    // A controller's mark for one of its flights, heard on the air (roadmap S2, OpportunityRun::Designate).
    void Designate(std::size_t flight, const Vec3& where, double radius_m, const std::string& by, double t_s) {
        opp_.Designate(flight, where, radius_m, by, t_s);
    }
    int ArmsFired() const { return arms_fired_ + em_arms_fired_; }

    // The munitions library (data/scenery/munitions.json; null: none) a gun neighbour's rounds in its GunPass are counted
    // by, as the game draws them: its munition's rate_rpm. Without one the bridge's own table (GunRoundsPerMin).
    void SetMunitions(const scenery::MunitionLibrary* lib) { munitions_ = lib; }

    // A new route for one of the package's flights (ScenarioRunner::RerouteFlight, roadmap B1), its weapons now for
    // `aim`: its aircraft release there, and the neighbours' passes they are in are decided again on their new track.
    // `by_radio`: a post's order said on its net (roadmap S2); false, and nothing changed, if the leader did not hear it.
    bool Reroute(ScenarioRunner& r, const std::string& flight, std::vector<RouteLeg> legs, const Vec3& aim, const std::string& by,
                 std::string* why = nullptr, bool by_radio = false) {
        int fi = -1;
        for (std::size_t k = 0; k < m_.flight_ids.size(); ++k) {
            if (m_.flight_ids[k] == flight) fi = static_cast<int>(k);
        }
        if (fi < 0) {
            if (why != nullptr) *why = "no flight '" + flight + "' in package " + std::to_string(m_.package_id);
            return false;
        }
        if (!r.RerouteFlight(flight, std::move(legs), aim, by, why, by_radio)) return false;
        if (const FlightState* fs = r.Flight(flight)) {
            if (!fs->orders.empty() && !fs->orders.back().heard) {
                if (why != nullptr) *why = "the leader did not hear the call";
                return false;
            }
        }
        for (std::size_t i = 0; i < m_.aircraft.size(); ++i) {
            RaidAircraft& a = m_.aircraft[i];
            if (a.flight != fi) continue;
            if (a.has_release) {
                a.release_enu = aim;
                a.aim_enu = aim;
            }
            for (std::size_t j = 0; j < m_.neighbours.size(); ++j) {
                Pass& ps = passes_[i * m_.neighbours.size() + j];
                if (ps.inside && !ps.done) ps.rechord = true;
            }
        }
        return true;
    }

    // What happened, for AirCampaign::CompletePlayerMission.
    PackageOutcome Finish(ScenarioRunner& r) {
        PackageOutcome o;
        o.package_id = m_.package_id;
        o.played = true;
        o.truth_applied = false;
        // The loads still falling when the session ended (a raid ends once its aircraft are gone, and a level release
        // falls half a minute) land as Step would have landed them, in their order, until our vehicle is out of action.
        std::vector<Impact*> falling;
        for (Impact& imp : impacts_) {
            if (!imp.done) falling.push_back(&imp);
        }
        std::stable_sort(falling.begin(), falling.end(), [](const Impact* x, const Impact* y) { return x->t < y->t; });
        for (Impact* imp : falling) {
            if (unit_destroyed_) break;
            imp->done = true;
            Explode(r, *imp, imp->t);
        }
        ReadNeighbourKills(r);
        // Our fighters' kills in the air battle (roadmap B8): written with the regiment, counted apart.
        std::vector<bool> by_fighter(m_.aircraft.size(), false);
        if (const AirFightSession* af = r.AirFight()) {
            for (const AirFightSession::Kill& k : af->Kills()) {
                for (std::size_t i = 0; i < m_.aircraft.size(); ++i) {
                    if (m_.aircraft[i].target_id != k.target || !kill_by_[i].empty() || !af->IsOurs(k.shooter)) continue;
                    kill_by_[i] = k.by;
                    by_fighter[i] = true;
                }
                if (af->IsOurs(k.target)) o.our_fighters_lost.push_back(af->RegimentOf(k.target));
            }
            if (!af->Kills().empty() || !af->Shots().empty()) ++o.air_fights;
        }
        // Which aircraft went down: our missiles' hits (the engagement model's HIT), a neighbour's ("destroyed by
        // <neighbour>"), the battery's launchers ("<launcher> destroys <target>", unless it "takes back the kill of" it)
        // and anything else the world destroyed ("destroyed").
        std::vector<bool> down(m_.aircraft.size(), false);
        const std::string kDestroys = "destroys ", kTakesBack = "takes back the kill of ";
        for (const DebriefEntry& e : r.Log().Entries()) {
            // Our hit that left it flying, heavily damaged (damage states, roadmap B4), is not a kill: its crash is.
            const bool hit = e.kind == DebriefEntry::Kind::Hit && e.detail.find("it flies on") == std::string::npos;
            const bool scen = e.kind == DebriefEntry::Kind::Scenario;
            const bool destroyed = scen && e.detail.compare(0, 9, "destroyed") == 0;
            for (std::size_t i = 0; i < m_.aircraft.size(); ++i) {
                const std::string& id = m_.aircraft[i].target_id;
                if ((hit || destroyed) && id == e.what) down[i] = true;
                // A neighbour's damage that brought it down later (a decoy shot down is never anyone's kill).
                if (destroyed && id == e.what && kill_by_[i].empty() && !m_.aircraft[i].drone) {
                    for (const RaidNeighbour& nb : m_.neighbours) {
                        if (e.detail == "destroyed by " + nb.name) kill_by_[i] = nb.id;
                    }
                }
                if (scen && e.detail == kDestroys + id) down[i] = true;
                if (scen && e.detail == kTakesBack + id) down[i] = false;
            }
        }
        const std::size_t nf = m_.flight_in.size();
        std::vector<bool> flight_released(nf, false), flight_has_release(nf, false), flight_alive(nf, false);
        for (std::size_t i = 0; i < m_.aircraft.size(); ++i) {
            const RaidAircraft& a = m_.aircraft[i];
            const std::size_t fi = static_cast<std::size_t>(std::max(0, a.flight));
            if (fi >= flight_released.size()) continue;
            if (a.has_release) flight_has_release[fi] = true;
            if (state_[i].released) flight_released[fi] = true;
            if (!down[i]) flight_alive[fi] = true;
            // Damaged and got away (roadmap S4): its flight and its level, for the campaign's repair.
            if (!down[i] && !a.missile && !a.drone && d_->tuning.Get("damage.mode") >= 1.0) {
                if (const AircraftDamage* dm = r.TargetDamage(a.target_id)) {
                    if (dm->level == DamageLevel::Moderate || dm->level == DamageLevel::Heavy) {
                        PackageOutcome::DamagedRecord d;
                        d.flight = a.flight;
                        d.heavy = dm->level == DamageLevel::Heavy;
                        o.damaged_list.push_back(d);
                        ++o.damaged;
                    }
                }
            }
            if (!down[i] || a.drone) continue;
            if (a.missile) {
                // Shot down before it struck: the defence's (the battery's or a neighbour's), never a crew lost.
                if (!state_[i].impacted) o.missiles_downed.push_back(kill_by_[i].empty() ? m_.own_object : kill_by_[i]);
                continue;
            }
            LossRecord l;
            l.flight = a.flight;
            l.by = kill_by_[i].empty() ? m_.own_object : kill_by_[i];
            l.fighter = by_fighter[i];
            l.where = state_[i].last_pos;
            l.officer = a.officer;
            l.captured = rng_.Chance(0.5);
            o.losses.push_back(l);
        }
        // A flight aborted when it was ordered home before its release (flights.hpp, roadmap B1): by its leader, its
        // package's commander, or each of its pilots on his own (damaged). Orders from outside it are its retasks.
        for (std::size_t fi = 0; fi < flight_released.size(); ++fi) {
            const std::string fid = fi < m_.flight_ids.size() ? m_.flight_ids[fi] : std::string();
            const FlightState* fs = fid.empty() ? nullptr : r.Flight(fid);
            if (fs != nullptr) {
                for (const OrderRecord& rec : fs->orders) {
                    if (rec.by == "leader" || rec.by == "package" || rec.by == "pilot" || !rec.heard) continue;
                    RetaskRecord rt;
                    rt.flight = static_cast<int>(fi);
                    rt.kind = OrderKindId(rec.kind);
                    rt.by = rec.by;
                    rt.t_s = rec.t_s;
                    o.retasks.push_back(rt);
                }
            }
            if (flight_released[fi]) {
                o.released_flights.push_back(static_cast<int>(fi));
            } else if (flight_has_release[fi] && flight_alive[fi]) {
                bool home = fs == nullptr;  // a manifest without its flights (an older save): broke away as before
                if (fs != nullptr) {
                    std::vector<int> member_home;
                    for (const OrderRecord& rec : fs->orders) {
                        if (rec.kind != OrderKind::ReturnHome || !rec.heard) continue;
                        if (rec.target < 0) home = true;
                        else member_home.push_back(rec.target);
                    }
                    bool all = !fs->slots.empty();
                    for (const FlightSlot& sl : fs->slots) {
                        if (sl.alive && std::find(member_home.begin(), member_home.end(), sl.target) == member_home.end()) all = false;
                    }
                    home = home || all;
                }
                if (home) o.aborted_flights.push_back(static_cast<int>(fi));
            }
        }
        Jettisoned(r, flight_released, o);
        o.delivered = delivered_;
        o.hit_nothing = delivered_ && !hit_something_;
        o.damage = damage_;
        o.decoys_hit = decoys_hit_;
        o.destroyed = destroyed_;
        o.missiles_spent = r.Launches();
        ReadEmArms(r);
        o.arms_fired = arms_fired_ + em_arms_fired_;
        if (emitted_s_ > 0.0) o.AddEmission(m_.own_object, emitted_s_);
        if (launch_seen_) o.fired.push_back(m_.own_object);
        o.sightings = sightings_;
        if (radar_killed_) {
            o.radars_killed.push_back(m_.own_object);
        } else if (radar_silenced_) {
            o.radars_suppressed.push_back(m_.own_object);
        }
        if (photographed_) o.recce_seen.push_back(m_.own_object);
        for (std::size_t j = 0; j < m_.neighbours.size(); ++j) {
            const std::string& id = m_.neighbours[j].id;
            const bool fights = r.NeighbourFights(id);
            const NeighbourState& ns = neighbours_[j];
            const int spent = fights ? r.NeighbourLaunches(id) : ns.spent;
            const double emitted = fights ? r.NeighbourRadarSeconds(id) : ns.emitted_s;
            if (fights ? spent == 0 && emitted <= 0.0 : !ns.fired) continue;
            o.fired.push_back(id);
            if (emitted > 0.0) o.AddEmission(id, emitted);
            if (spent > 0) o.missiles_by.push_back({id, spent});
        }
        OpportunityRecords(o);
        o.notes = notes_;
        return o;
    }

    // Under fire (roadmap B4): the flights that threw their stores off short of their release point, and the tanks and
    // loads that fell, from the session's reactions (the truth, for the campaign and the debrief's ledger only).
    void Jettisoned(const ScenarioRunner& r, const std::vector<bool>& flight_released, PackageOutcome& o) const {
        const std::vector<ScenarioTarget>& targets = r.Def().targets;
        for (const JettisonRecord& j : r.Reactions().Jettisons()) {
            if (j.target < 0 || static_cast<std::size_t>(j.target) >= targets.size()) continue;
            const std::string& id = targets[static_cast<std::size_t>(j.target)].id;
            for (std::size_t i = 0; i < m_.aircraft.size(); ++i) {
                const RaidAircraft& a = m_.aircraft[i];
                if (a.target_id != id) continue;
                o.tanks_dropped += j.tanks;
                if (!j.stores) continue;
                ++o.stores_dropped;
                const int fi = a.flight;
                if (fi < 0 || static_cast<std::size_t>(fi) >= flight_released.size() || flight_released[static_cast<std::size_t>(fi)]) continue;
                if (std::find(o.jettisoned_flights.begin(), o.jettisoned_flights.end(), fi) == o.jettisoned_flights.end()) o.jettisoned_flights.push_back(fi);
            }
        }
    }

    // Aircraft the neighbours have shot down so far.
    int NeighbourKills() const {
        int n = 0;
        for (const std::string& k : kill_by_) n += k.empty() ? 0 : 1;
        return n;
    }

private:
    struct AircraftState {
        bool released = false;
        bool seen = false;
        int arms_left = 0;
        double last_arm_t = -1e9;
        Vec3 last_pos;
        Vec3 prev_pos;  // where it was the step before
        bool impacted = false;  // a cruise missile that struck (gone by its own warhead, not shot down)
        double release_d = 1e18;  // its distance to its release point over the ground the step before
        double release_fit = std::numeric_limits<double>::quiet_NaN();  // ln(aim ahead / throw) the step before (Releases)
    };
    // One aircraft's pass through one neighbour's zone: rolled as it enters, the hit (if any) half way through.
    struct Pass {
        bool inside = false;
        bool done = false;
        bool hit = false;
        double flown_m = 0.0;
        double hit_at_m = 0.0;
        bool rechord = false;  // its flight was rerouted inside the zone: the rest of its chord on its new track
        // The presentation (fx): a missile neighbour's round is shown launched so that its flight ends where the pass is
        // decided (half way through the zone), however long the pass.
        bool sam = false;
        bool sam_launched = false;
        bool sam_ended = false;
        double sam_launch_at_m = 0.0;  // flown this far into the zone when it leaves its site
        double sam_end_at_m = 0.0;     // ... and when it bursts
        double sam_flight_s = 0.0;
    };
    struct NeighbourState {
        int missiles = -1;
        int spent = 0;
        bool fired = false;
        double emitted_s = 0.0;
        std::vector<std::pair<int, int>> engaged;  // (flight, its aircraft engaged in their pass)
    };
    struct Impact {
        double t = 0.0;
        int aircraft = -1;
        Vec3 where;
        bool done = false;
        int opp = 0;                 // a dive-bombing pass's (roadmap S4): its pass number, its draws keyed; 0 a release
        OpportunityRun::Fire fire;   // that pass
    };
    struct Arm {
        double t = 0.0;
        int aircraft = -1;
        bool memory = false;
        bool done = false;
    };

    RaidManifest m_;
    const AirWarData* d_;
    Rng rng_;
    std::mt19937 fx_rng_;  // the presentation feed's own draws (never rng_: the bridge's chances fall as without it)
    std::vector<AircraftState> state_;
    std::vector<Impact> impacts_;
    std::vector<Arm> arms_;
    double last_t_ = 0.0;
    double emitted_s_ = 0.0;
    double last_radiating_t_ = -1e9;
    bool launch_seen_ = false;
    std::vector<PackageOutcome::LaunchSighting> sightings_;
    std::size_t sightings_taken_ = 0;
    bool unit_destroyed_ = false;
    bool radar_killed_ = false;
    bool radar_silenced_ = false;
    bool photographed_ = false;
    bool delivered_ = false;
    bool hit_something_ = false;
    int arms_fired_ = 0;
    int em_arms_fired_ = 0;
    std::size_t em_arms_read_ = 0;
    std::vector<std::pair<std::string, double>> damage_;
    std::vector<std::string> decoys_hit_;
    std::vector<std::string> destroyed_;
    std::vector<std::string> notes_;
    std::vector<Pass> passes_;  // aircraft x neighbours
    std::vector<NeighbourState> neighbours_;
    std::vector<std::string> kill_by_;  // per aircraft: the neighbour that shot it down
    const scenery::MunitionLibrary* munitions_ = nullptr;
    OpportunityRun opp_;          // targets of opportunity and the passes flown (roadmap B6)
    std::uint64_t opp_seed_ = 0;  // the passes' effect draws (keyed: em::Stream::Opportunity)
    int pass_fires_ = 0;

    // When an aircraft releases its load (docs/air_war/05_EXECUTION.md "release"). Rockets, missiles, guns, a weapon not
    // known and a recce's camera: within `scenario.release_capture_m` of its release point. A load that falls goes where
    // the aircraft's state throws it (weapon_flight.hpp), so it is released where the throw from here best meets the aim
    // ahead along its track (the aim within the capture of its line: a flight's aircraft fly their formation's slots
    // apart and aim at one point): at the step where the throw comes to reach the aim (the
    // aim's distance ahead over the throw crosses 1), or, where it never quite does, at the step after the two came
    // closest. The window for it is the attack run: where the plan puts the release point over the target (level,
    // laydown: a throw short of it is where the bombs go), its farthest throw (in a vacuum) and kReleaseMarginM more;
    // for a toss or a dive, from the point the attack starts (the pull-up, the roll-in: data/gameplay/air_war.json's
    // delivery.*); for a pop-up, from its apex (planner.hpp MainRoute: half way between the pull-up and the release
    // point), and only once it is past the top of its climb and going down (its path angle at or below the horizontal):
    // a pop-up attack rolls in and releases in its dive, never in the climb (that would be a toss); and never less than
    // the capture. At the latest it is released at its release point: within the runner's waypoint capture, or turning
    // away from it inside the capture (a route that turns away there).
    // (The routes' tosses level off before their release point and their dives point short of the target, so those come
    // nearest early in the attack; see docs/fx/README.md 3.1.)
    static constexpr double kReleaseMarginM = 500.0;     // sam3d
    static constexpr double kWaypointCaptureM = 500.0;   // the runner's (aircraft.hpp waypoint_capture_radius_m)
    double AttackWindow(const RaidAircraft& a, double h, const ReleaseState& st, double capture) const {
        const TuningTable& tune = d_->tuning;
        const double short_m = Dist2d(a.release_enu, a.aim_enu);  // the release point's distance short of the aim
        double start_m = -1.0;  // the attack's start, short of the aim
        if (a.delivery == Delivery::Loft) start_m = tune.Get("delivery.loft_pull_m");
        if (a.delivery == Delivery::PopUp) start_m = 0.5 * (tune.Get("delivery.popup_pull_m") + short_m);  // its apex
        if (a.delivery == Delivery::Dive) start_m = tune.Get("delivery.dive_roll_in_m");
        if (start_m >= 0.0) return std::max(capture, start_m - short_m);
        return std::max(capture, VacuumThrow(h, st.speed_mps, st.path_deg) + kReleaseMarginM);
    }
    bool Releases(const ScenarioRunner& r, const RaidAircraft& a, AircraftState& s, const Vec3& pos) {
        const double capture = d_->tuning.Get("scenario.release_capture_m");
        const double d = Dist2d(pos, a.release_enu);
        const double prev_d = s.release_d;
        s.release_d = d;
        const double prev_fit = s.release_fit;
        s.release_fit = std::numeric_limits<double>::quiet_NaN();
        const bool falls = a.role != FlightRole::Recce && !a.weapon.empty() && Falls(WeaponPathOf(a.weapon, &d_->weapons));
        if (!falls) return d < capture;
        const LocalFrame& frame = r.GetWorld().Frame();
        const std::string delivery = DeliveryId(a.delivery);
        const ReleaseState st = ReleaseStateOf(frame, pos, r.TargetVelocity(a.target_id), delivery);
        if (st.ground_mps < 1.0) return d < capture;
        const double h = frame.AltitudeOf(pos) - r.GroundHeightAt(a.aim_enu.x, a.aim_enu.y);
        if (d >= AttackWindow(a, h, st, capture)) return false;
        const bool latest = d <= kWaypointCaptureM || (d < capture && d > prev_d);
        if (a.delivery == Delivery::PopUp && st.path_deg > 0.0) return latest;  // still climbing to its apex
        const Vec3 to_aim = r.GroundEnu(a.aim_enu.x, a.aim_enu.y) - pos;
        const double along = Dot(to_aim, st.along);
        const double up = Dot(to_aim, st.up);
        const double cross = std::sqrt(std::max(0.0, to_aim.LengthSquared() - along * along - up * up));
        if (cross > capture) return latest;
        const WeaponFlight wf = WeaponFlightOf(delivery, a.weapon, h, st.speed_mps, st.path_deg, to_aim.Length(), &d_->weapons);
        if (wf.throw_m < 1.0) return latest;
        const double fit = std::log(std::max(1.0, along) / wf.throw_m);  // 0: the throw lands it on the aim
        s.release_fit = fit;
        if (std::isnan(prev_fit)) return latest;  // the first look: nothing to compare with yet
        if ((fit <= 0.0) != (prev_fit <= 0.0)) return true;  // the throw has come to reach the aim
        // A pop-up in its dive with the aim still beyond his throw holds on, down to his release point: since the climb is
        // flown as a path angle (roadmap S4) he tops out early and dives toward his release height, his throw shrinking
        // faster than the aim comes nearer, until it reaches the aim.
        if (a.delivery == Delivery::PopUp && fit > 0.0) return latest;
        if (std::fabs(fit) > std::fabs(prev_fit)) return true;  // they came closest a step ago: no nearer from here
        return latest;
    }

    // The neighbours' fire (see the header): each aircraft's pass through each zone is rolled once, as it enters.
    static constexpr double kRegimentTellsS = 90.0;  // sam3d: the neighbour reports up, the regiment passes it on
    // The fighting neighbours' kills, as the runner's fire model made them.
    void ReadNeighbourKills(ScenarioRunner& r) {
        for (const auto& k : r.NeighbourKills()) {
            for (std::size_t i = 0; i < m_.aircraft.size(); ++i) {
                if (m_.aircraft[i].target_id != k.first || !kill_by_[i].empty()) continue;
                kill_by_[i] = k.second;
                for (const RaidNeighbour& nb : m_.neighbours) {
                    if (nb.id != k.second) continue;
                    notes_.push_back(k.first + " shot down by " + nb.name);
                    r.RegimentTellsDestroyed(k.first, nb.name, kRegimentTellsS);
                }
            }
        }
    }

    void Neighbours(ScenarioRunner& r) {
        ReadNeighbourKills(r);
        for (std::size_t i = 0; i < m_.aircraft.size(); ++i) {
            const RaidAircraft& a = m_.aircraft[i];
            const AircraftState& s = state_[i];
            if (!s.seen) continue;
            if (!r.TargetAlive(a.target_id)) {
                // Gone (shot down by another, or past its last waypoint) with a neighbour's round on its way: the feed
                // shows that round flying on past where it was and destroying itself.
                for (std::size_t j = 0; j < m_.neighbours.size(); ++j) {
                    Pass& ps = passes_[i * m_.neighbours.size() + j];
                    if (!ps.sam_launched || ps.sam_ended) continue;
                    ps.sam_ended = true;
                    FxSamEnd(r, i, j, false);
                }
                continue;
            }
            const Vec3 step = s.last_pos - s.prev_pos;
            const double step_m = std::hypot(step.x, step.y);
            for (std::size_t j = 0; j < m_.neighbours.size() && r.TargetAlive(a.target_id); ++j) {
                const RaidNeighbour& nb = m_.neighbours[j];
                Pass& ps = passes_[i * m_.neighbours.size() + j];
                if (ps.done || r.NeighbourFights(nb.id)) continue;
                const bool inside = Dist2d(s.last_pos, nb.enu) < nb.defence.radius_m;
                if (inside && !ps.inside) {
                    ps.inside = true;
                    Engage(r, i, j, step, step_m);
                } else if (ps.inside) {
                    ps.flown_m += step_m;
                    if (ps.rechord && inside && step_m > 1.0) {  // rerouted: the hit half way through what it has left
                        ps.rechord = false;
                        const Vec3 d = nb.enu - s.last_pos;
                        const double along = (d.x * step.x + d.y * step.y) / step_m;
                        const double cross2 = std::max(0.0, d.x * d.x + d.y * d.y - along * along);
                        const double left = std::max(0.0, along + std::sqrt(std::max(0.0, nb.defence.radius_m * nb.defence.radius_m - cross2)));
                        if (ps.hit) ps.hit_at_m = ps.flown_m + 0.5 * left;
                        if (ps.sam) {
                            ps.sam_end_at_m = ps.hit_at_m > 0.0 ? ps.hit_at_m : ps.flown_m + 0.5 * left;
                            ps.sam_launch_at_m = std::max(ps.flown_m, ps.sam_launch_at_m);
                        }
                    }
                }
                if (!ps.inside || ps.done) continue;
                FxPass(r, i, j, inside);  // the feed: its missile leaving the site, and bursting where the pass is decided
                // The hit, half way through the zone, or as it leaves if it turned away sooner.
                if (ps.hit && (ps.flown_m >= ps.hit_at_m || !inside)) {
                    ps.done = true;
                    // An IR missile (no radar, rounds: a Strela's or a MANPADS') taken by its flares, out in time (B4).
                    const std::uint64_t shot = (static_cast<std::uint64_t>(j) << 32) ^ (0x5EED0000ull + static_cast<std::uint64_t>(i));
                    if (!nb.defence.radar && nb.defence.missiles >= 0 && r.FlaresDecoy(a.target_id, shot)) {
                        notes_.push_back(a.target_id + "'s flares decoy the missile from " + nb.name);
                        continue;
                    }
                    // With damage states (roadmap R1, B4) its kill is drawn into destroyed or heavily damaged: a damaged
                    // one flies on and may crash later (its credit read from the log in Finish).
                    const bool down = r.DamageStates() ? r.MissileDamage(a.target_id, true, 0.0, nb.name) : r.DestroyTarget(a.target_id, nb.name);
                    if (!down && r.DamageStates() && r.TargetAlive(a.target_id)) notes_.push_back(a.target_id + " damaged by " + nb.name);
                    if (down) {
                        kill_by_[i] = nb.id;
                        notes_.push_back(a.target_id + " shot down by " + nb.name);
                        r.RegimentTellsDestroyed(a.target_id, nb.name, kRegimentTellsS);
                    }
                    continue;
                }
                if (!inside) ps.done = true;
            }
        }
    }

    void Engage(ScenarioRunner& r, std::size_t i, std::size_t j, const Vec3& step, double step_m) {
        const TuningTable& tune = d_->tuning;
        const RaidAircraft& a = m_.aircraft[i];
        const RaidNeighbour& nb = m_.neighbours[j];
        NeighbourState& ns = neighbours_[j];
        Pass& ps = passes_[i * m_.neighbours.size() + j];
        if (nb.function <= 0.05 || ns.missiles == 0) return;
        if (nb.defence.max_target_mps > 0.0 && a.speed_mps > nb.defence.max_target_mps) return;  // too fast for it
        // The chord it will fly through the zone on its present heading.
        const double radius = nb.defence.radius_m;
        double chord = radius;
        if (step_m > 1.0) {
            const Vec3 d = state_[i].last_pos - nb.enu;
            const double along = (d.x * step.x + d.y * step.y) / step_m;
            const double cross2 = std::max(0.0, d.x * d.x + d.y * d.y - along * along);
            chord = 2.0 * std::sqrt(std::max(0.0, radius * radius - cross2));
        }
        // The route planner's routes turn inside a zone (route.mode 1): the pass is what is left of its planned route
        // inside the zone from here, to where the route leaves it.
        if (tune.Get("route.mode") >= 1.0) {
            const int wid = r.TargetWorldId(a.target_id);
            const Aircraft* ac = wid >= 0 ? r.GetWorld().FindAircraft(wid) : nullptr;
            if (ac != nullptr && ac->next_waypoint < ac->waypoints.size()) {
                double planned = 0.0;
                Vec3 from = state_[i].last_pos;
                for (std::size_t k = ac->next_waypoint; k < ac->waypoints.size(); ++k) {
                    const Vec3& to = ac->waypoints[k];
                    planned += ChordInCircle2d(from, to, nb.enu, radius);
                    if (planned > 0.0 && Dist2d(to, nb.enu) >= radius) break;
                    from = to;
                }
                if (planned > 0.0) chord = std::min(planned, 2.0 * radius);
            }
        }

        const bool radar = nb.defence.radar;
        if (radar) ns.emitted_s += chord / 250.0 + 30.0;
        ns.fired = true;
        if (a.drone) {
            // A decoy draws fire: the defence wastes rounds on it and shows itself.
            const int before = ns.spent;
            Spend(ns, 1 + rng_.Index(2));
            const bool shot = rng_.Chance(0.6);
            FxDecoyFire(r, i, j, ns.spent - before, chord, shot);
            if (shot) r.DestroyTarget(a.target_id, nb.name);  // drones are meant to be lost: never a loss
            ps.done = true;
            return;
        }
        // Its fire channels: how many of this flight it can take on in their pass.
        std::size_t e = 0;
        while (e < ns.engaged.size() && ns.engaged[e].first != a.flight) ++e;
        if (e == ns.engaged.size()) ns.engaged.push_back({a.flight, 0});
        int& engaged = ns.engaged[e].second;
        if (EngagedAircraft(nb.defence.channels, nb.function, engaged + 1) <= engaged) return;
        ++engaged;
        double env = nb.function;
        if (!radar && m_.night) env *= tune.Get("attrition.night");
        if (!radar && m_.poor_weather) env *= tune.Get("attrition.poor_weather");
        const double k = PassKillChance(nb.defence.lethality, chord, radius, a.profile, a.helicopter, nb.defence.low_cover,
                                        nb.defence.medium_cover, radar, radar ? m_.sead_supp : 0.0, radar ? m_.jam : 0.0, env,
                                        Clamp01(a.evasion + 0.3 * (m_.airmanship - 0.5)), tune);
        if (k <= 0.0) return;
        Spend(ns, 1);
        if (rng_.Chance(k)) {
            ps.hit = true;
            ps.hit_at_m = 0.5 * chord;
        }
        // The feed: guns fire through the pass; a missile is shown leaving its site in time to burst half way through.
        const double speed = FxSpeed(r, a);
        if (nb.defence.missiles < 0) {
            FxGunPass(r, i, j, chord / speed);
        } else {
            ps.sam = true;
            ps.sam_end_at_m = 0.5 * chord;
            ps.sam_flight_s = std::min(ps.sam_end_at_m / speed, kFxSamFlightS);
            ps.sam_launch_at_m = std::max(0.0, ps.sam_end_at_m - ps.sam_flight_s * speed);
        }
    }

    static void Spend(NeighbourState& ns, int n) {
        if (ns.missiles < 0) return;
        const int used = std::min(ns.missiles, n);
        ns.missiles -= used;
        ns.spent += used;
    }

    // The EM ARMs' ends (the runner logs each itself): on our vehicle, the radar is killed and the vehicle out of
    // action; on a dummy, that dummy is destroyed; a miss means the crew beat it (silent in time, or a decoy).
    void ReadEmArms(ScenarioRunner& r) {
        em_arms_fired_ = r.ArmsFired();
        const std::vector<EmSensorLink::ArmResult>& res = r.ArmResults();
        for (; em_arms_read_ < res.size(); ++em_arms_read_) {
            const EmSensorLink::ArmResult& a = res[em_arms_read_];
            if (a.hit && a.hit_what == "unit") {
                radar_killed_ = true;
                unit_destroyed_ = true;
                notes_.push_back(a.carrier + " ARM: hit the vehicle's radar - our vehicle is out of action");
                continue;
            }
            if (a.hit && a.hit_what == "dummy") {
                const std::string* best = nullptr;
                double best_m = 1e18;
                for (const auto& dm : m_.dummies) {
                    const double d = Dist2d(dm.second, a.burst_point);
                    if (d < best_m) {
                        best_m = d;
                        best = &dm.first;
                    }
                }
                if (best != nullptr && std::find(decoys_hit_.begin(), decoys_hit_.end(), *best) == decoys_hit_.end()) {
                    decoys_hit_.push_back(*best);
                    destroyed_.push_back(*best);
                }
                notes_.push_back(a.carrier + " ARM: a dummy position is hit");
                continue;
            }
            radar_silenced_ = true;
            notes_.push_back(a.carrier + " ARM: missed");
        }
    }

    // `at_t`: when it happened (< 0: now).
    void Log(ScenarioRunner& r, const std::string& who, const std::string& what, double at_t = -1.0) {
        r.Log().Record({at_t >= 0.0 ? at_t : r.Time(), DebriefEntry::Kind::Scenario, "enemy air", who, what});
        notes_.push_back(who + " " + what);
    }

    void AddDamage(const std::string& id, double x) {
        for (auto& e : damage_) {
            if (e.first == id) {
                e.second += x;
                return;
            }
        }
        damage_.push_back({id, x});
    }

    // The loads whose time has come by `t` land at `t`, in their order, until our vehicle is out of action.
    void LandDue(ScenarioRunner& r, double t) {
        for (Impact& imp : impacts_) {
            if (imp.done || imp.t > t) continue;
            imp.done = true;
            Explode(r, imp, t);
            if (unit_destroyed_) return;
        }
    }

    // The load arrives at `at_t` (now in Step; its own time in Finish): what it hurts, and (a cruise missile's warhead
    // shown already) its WeaponImpact in the feed, set alight when it hit something.
    void Explode(ScenarioRunner& r, const Impact& imp, double at_t) {
        const bool burnt = Blast(r, imp, at_t);
        const RaidAircraft& a = m_.aircraft[static_cast<std::size_t>(imp.aircraft)];
        if (!a.missile) FxImpact(r, a, imp.where, burnt, at_t);
        if (imp.opp > 0) opp_.NoteResult(imp.fire, blast_kills_, blast_what_);  // the pilots' claims from what it did
    }
    int blast_kills_ = 0;
    std::string blast_what_;

    // A dive-bombing pass released its load at what it found (roadmap S4): it falls from here (weapon_flight.hpp) and
    // lands about the aim by the dive's circular error (delivery.cep_dive_m, by eye at night times its factor), drawn by
    // keys; what it hurts is the bridge's Blast when it lands.
    void BombsFired(ScenarioRunner& r, const OpportunityRun::Fire& f) {
        const TuningTable& tune = d_->tuning;
        const RaidAircraft& a = m_.aircraft[f.aircraft];
        AircraftState& s = state_[f.aircraft];
        s.released = true;  // its load is gone (owed to its target or not)
        r.NoteReleased(a.target_id);
        const int n = ++pass_fires_;
        const AircraftType* type = d_->faction.Aircraft(a.aircraft);
        const bool night_eye = m_.night && (type == nullptr || !type->night);
        const double cep = tune.Get("delivery.cep_dive_m") * (night_eye ? tune.Get("delivery.night_visual_factor") : 1.0);
        const double u1 = OppDraw(opp_seed_, f.aircraft, static_cast<std::uint64_t>(n), 42, 0);
        const double u2 = OppDraw(opp_seed_, f.aircraft, static_cast<std::uint64_t>(n), 43, 0);
        const double miss = cep * std::sqrt(-std::log(std::max(1e-12, 1.0 - u1)) / std::log(2.0));  // half within the CEP
        Impact imp;
        imp.aircraft = static_cast<int>(f.aircraft);
        imp.opp = n;
        imp.fire = f;
        imp.where = Vec3{f.aim.x + std::cos(6.283185307179586 * u2) * miss, f.aim.y + std::sin(6.283185307179586 * u2) * miss, 0.0};
        const Vec3 pos = r.TargetPosition(a.target_id);
        const Vec3 vel = r.TargetVelocity(a.target_id);
        const Vec3 ground = r.GroundEnu(imp.where.x, imp.where.y);
        const std::string delivery = DeliveryId(Delivery::Dive);
        const ReleaseState st = ReleaseStateOf(r.GetWorld().Frame(), pos, vel, delivery);
        const double agl = r.GetWorld().Frame().AltitudeOf(pos) - r.GroundHeightAt(imp.where.x, imp.where.y);
        const double fall = WeaponFlightOf(delivery, f.weapon, agl, st.speed_mps, st.path_deg, (ground - pos).Length(), &d_->weapons).time_s;
        imp.t = r.Time() + (fall > 0.0 ? fall : tune.Get("scenario.weapon_fall_s"));
        impacts_.push_back(imp);
        FxRelease(r, a, pos, vel, ground, imp.t);
        Log(r, a.target_id, "dive-bombs what it found");
    }

    // What the load hurts; true if it hit something (a vehicle, an object; not a near miss or a dummy).
    bool Blast(ScenarioRunner& r, const Impact& imp, double at_t) {
        const TuningTable& tune = d_->tuning;
        const RaidAircraft& a = m_.aircraft[static_cast<std::size_t>(imp.aircraft)];
        // A dive-bombing pass's load (roadmap S4) draws by keys of its own (em::Stream::Opportunity), so the bridge's own
        // stream falls as without it; a release draws on the bridge's stream as before.
        int draw = 0;
        const auto chance = [&](double p) {
            if (imp.opp <= 0) return rng_.Chance(p);
            return OppDraw(opp_seed_, static_cast<std::size_t>(imp.aircraft), static_cast<std::uint64_t>(imp.opp), 44, static_cast<std::uint64_t>(draw++)) < p;
        };
        blast_kills_ = 0;
        blast_what_.clear();
        const double lethal = tune.Get("scenario.bomb_lethal_m") * (a.helicopter ? 0.6 : 1.0);
        const double pk = (a.helicopter ? tune.Get("scenario.rocket_pk_vehicle") : tune.Get("scenario.bomb_pk_vehicle")) *
                          std::min(1.5, std::max(0.2, a.strike_power * 2.0));
        if (imp.opp <= 0 || imp.fire.contact < 0) delivered_ = true;
        bool hit = false;
        bool burnt = false;
        // Our vehicle.
        if (Dist2d(imp.where, m_.own_enu) < lethal) {
            hit = true;
            if (chance(pk)) {
                ++blast_kills_;
                blast_what_ = m_.own_object;
                unit_destroyed_ = true;
                AddDamage(m_.own_object, 0.25);
                Log(r, a.target_id, "bombs: our vehicle is hit and out of action", at_t);
                hit_something_ = true;
                return true;
            }
            Log(r, a.target_id, "bombs: near miss on our vehicle", at_t);
        }
        // The battery's other vehicles: each one lost costs the battery a quarter of its strength.
        for (const auto& v : m_.battery) {
            if (Dist2d(imp.where, v.second) < lethal && chance(pk)) {
                hit = true;
                ++blast_kills_;
                if (blast_what_.empty()) blast_what_ = v.first;
                hit_something_ = true;
                burnt = true;
                AddDamage(m_.own_object, v.first == "PU-12" ? 0.15 : 0.2);
                Log(r, a.target_id, "bombs: vehicle " + v.first + " of the battery is hit", at_t);
            }
        }
        // Dummies.
        for (const auto& dm : m_.dummies) {
            if (Dist2d(imp.where, dm.second) < tune.Get("strike.decoy_area_m") * 0.4) {
                hit = true;
                if (std::find(decoys_hit_.begin(), decoys_hit_.end(), dm.first) == decoys_hit_.end()) {
                    decoys_hit_.push_back(dm.first);
                    destroyed_.push_back(dm.first);
                    Log(r, a.target_id, "bombs: a dummy position is destroyed", at_t);
                }
            }
        }
        // Other objects (a bridge, a depot) inside the scenario.
        for (const auto& ob : m_.objects) {
            if (Dist2d(imp.where, ob.second) > tune.Get("strike.stale_miss_m") * 0.5) continue;
            const TheatreObject* def = d_->theatre.Find(ob.first);
            const double hard = def != nullptr ? def->hardness : 0.5;
            if (chance(tune.Get("strike.hit_chance"))) {
                ++blast_kills_;
                if (blast_what_.empty()) blast_what_ = ob.first;
                const double x = a.strike_power * (1.0 - 0.7 * hard);
                AddDamage(ob.first, x);
                hit = true;
                hit_something_ = true;
                burnt = true;
                Log(r, a.target_id, "bombs: " + (def != nullptr ? def->name : ob.first) + " hit", at_t);
            }
        }
        if (!hit) Log(r, a.target_id, "bombs fall on empty ground", at_t);
        return burnt;
    }

    // ---- Passes (roadmap B6). A pass that opened fire: on his run the pilot lines up on what is really there within
    // opportunity.find_m of where he believed it (the nearest of our vehicle, the battery's vehicles, a dummy, an object,
    // a mover on the roads); nothing there, it hits nothing. One keyed draw against the weapon's per_pass for its class
    // (attack_pass.hpp GunPassEffect: moving, under fire, by eye at night). Its first planned pass is its load's release.
    void PassFired(ScenarioRunner& r, const OpportunityRun::Fire& f) {
        if (f.bombs) {
            BombsFired(r, f);
            return;
        }
        const TuningTable& tune = d_->tuning;
        const RaidAircraft& a = m_.aircraft[f.aircraft];
        AircraftState& s = state_[f.aircraft];
        if (f.first_planned && !s.released) {
            s.released = true;
            r.NoteReleased(a.target_id);
        }
        if (f.contact < 0) delivered_ = true;
        const int n = ++pass_fires_;
        // What is really near the aim.
        std::string what;
        int kind = -1;  // 0 our vehicle, 1 a battery vehicle, 2 a dummy, 3 an object, 4 a mover
        TargetClass cls = TargetClass::Light;
        bool moving = false;
        double best = tune.Get("opportunity.find_m");
        const auto consider = [&](const Vec3& at, const std::string& id, int k, TargetClass c, bool mv) {
            const double dist = Dist2d(at, f.aim);
            if (dist >= best) return;
            best = dist;
            what = id;
            kind = k;
            cls = c;
            moving = mv;
        };
        consider(m_.own_enu, m_.own_object, 0, TargetClass::Light, false);
        for (const auto& v : m_.battery) consider(v.second, v.first, 1, TargetClass::Light, false);
        for (const auto& dm : m_.dummies) consider(dm.second, dm.first, 2, TargetClass::Light, false);
        for (const auto& ob : m_.objects) {
            const TheatreObject* def = d_->theatre.Find(ob.first);
            consider(ob.second, ob.first, 3, def != nullptr && def->hardness > 0.6 ? TargetClass::Hard : TargetClass::Light, false);
        }
        if (const nav::SessionNav* nav = r.Nav()) {
            for (const nav::Mover& mv : nav->GetMovers().All()) {
                if (mv.state == nav::MoverState::Waiting || mv.state == nav::MoverState::Arrived || mv.state == nav::MoverState::BrokenDown ||
                    mv.route.path.empty())
                    continue;
                const MapPoint p = nav::PointAlong(mv.route, mv.arc_m);
                consider(Vec3{p.east_m, p.north_m, 0.0}, "mover:" + mv.id, 4, TargetClass::Soft, mv.state == nav::MoverState::Moving);
            }
        }
        const AircraftType* type = d_->faction.Aircraft(a.aircraft);
        const bool night_eye = m_.night && (type == nullptr || !type->night);
        const PassEffect eff = GunPassEffect(f.pass, cls, moving, f.under_fire, night_eye, tune);
        const double u = OppDraw(opp_seed_, f.aircraft, static_cast<std::uint64_t>(n), 40, 0);
        const bool hit = kind >= 0 && u < eff.kill;
        const bool burnt = hit && OppDraw(opp_seed_, f.aircraft, static_cast<std::uint64_t>(n), 41, 0) < eff.catastrophic;
        const std::string how = IsRocketPass(f) ? "rocket pass" : "gun run";
        int kills = 0;
        if (kind < 0) {
            Log(r, a.target_id, how + ": nothing there");
        } else if (!hit) {
            Log(r, a.target_id, how + ": misses " + (kind == 0 ? std::string("our vehicle") : what));
        } else {
            kills = 1;
            hit_something_ = true;
            switch (kind) {
                case 0:
                    unit_destroyed_ = true;
                    AddDamage(m_.own_object, 0.25);
                    Log(r, a.target_id, how + ": our vehicle is hit and out of action");
                    break;
                case 1:
                    AddDamage(m_.own_object, what == "PU-12" ? 0.15 : 0.2);
                    Log(r, a.target_id, how + ": vehicle " + what + " of the battery is hit");
                    break;
                case 2:
                    if (std::find(decoys_hit_.begin(), decoys_hit_.end(), what) == decoys_hit_.end()) {
                        decoys_hit_.push_back(what);
                        destroyed_.push_back(what);
                    }
                    Log(r, a.target_id, how + ": a dummy position is wrecked");
                    break;
                case 3: {
                    const TheatreObject* def = d_->theatre.Find(what);
                    AddDamage(what, 0.3 * a.strike_power * (1.0 - 0.7 * (def != nullptr ? def->hardness : 0.5)));
                    Log(r, a.target_id, how + ": " + (def != nullptr ? def->name : what) + " hit");
                    break;
                }
                default:
                    r.StrikeMover(what.substr(6), a.target_id);
                    Log(r, a.target_id, how + ": " + what.substr(6) + " put out of action on the road");
                    break;
            }
        }
        opp_.NoteResult(f, kills, what);
        FxPass(r, a, f, burnt || (hit && kind != 2));
    }
    // Rockets (a salvo of a few) or a gun (a burst of many rounds).
    static bool IsRocketPass(const OpportunityRun::Fire& f) { return f.pass.rounds <= 20 && f.weapon != "door_guns"; }

    // The outcome's opportunity records, with the theatre object each report or claim is taken to be about (the nearest of
    // our vehicle, a dummy or an object within kClaimMatchM of where he believed it: the enemy cannot tell a dummy from a
    // battery), and a note of each act for the journal.
    static constexpr double kClaimMatchM = 1500.0;  // sam3d
    void OpportunityRecords(PackageOutcome& o) {
        for (PackageOutcome::OpportunityRecord rec : opp_.Records()) {
            const Vec3 at{rec.east_m, rec.north_m, 0.0};
            double best = kClaimMatchM;
            const auto consider = [&](const Vec3& p, const std::string& id) {
                const double dist = Dist2d(p, at);
                if (dist < best) {
                    best = dist;
                    rec.object = id;
                }
            };
            if (rec.believed == "battery" || rec.believed == "vehicles") {
                consider(m_.own_enu, m_.own_object);
                for (const auto& dm : m_.dummies) consider(dm.second, dm.first);
            }
            for (const auto& ob : m_.objects) consider(ob.second, ob.first);
            const std::string callsign = rec.flight >= 0 && static_cast<std::size_t>(rec.flight) < m_.flight_ids.size() &&
                                                 !m_.flight_ids[static_cast<std::size_t>(rec.flight)].empty()
                                             ? m_.flight_ids[static_cast<std::size_t>(rec.flight)]
                                             : std::string("a flight");
            if (rec.passes > 0) {
                notes_.push_back(callsign + " attacked a " + rec.believed + " it found (" + std::to_string(rec.passes) + " pass" +
                                 (rec.passes > 1 ? "es" : "") + "), claims " + std::to_string(rec.claimed));
            } else if (rec.act == "report") {
                notes_.push_back(callsign + " reported a " + rec.believed + (rec.reported_in_flight ? " in flight" : " at the debrief"));
            }
            o.opportunities.push_back(rec);
        }
    }

    // A pass in the feed: its burst or salvo leaving the aircraft, and landing at the aim.
    void FxPass(ScenarioRunner& r, const RaidAircraft& a, const OpportunityRun::Fire& f, bool burnt) {
        if (!r.FxOn()) return;
        const Vec3 pos = r.TargetPosition(a.target_id);
        const Vec3 vel = r.TargetVelocity(a.target_id);
        const bool rockets = IsRocketPass(f);
        const double arrive = r.Time() + (f.aim - pos).Length() / (rockets ? kFxRocketMps : kFxRoundMps);
        fx::Event e = FxEvent(fx::EventKind::WeaponRelease, r.Time());
        e.position = pos;
        e.velocity = vel;
        e.aim = f.aim;
        e.end_t_s = arrive;
        e.source = a.target_id;
        e.world_id = r.TargetWorldId(a.target_id);
        e.number = f.pass.rounds;
        e.weapon = f.weapon;
        e.delivery = rockets ? "gentle_dive" : "strafe";
        r.Fx().Push(e);
        fx::Event i = FxEvent(fx::EventKind::WeaponImpact, arrive);
        i.position = i.aim = f.aim;
        i.source = a.target_id;
        i.world_id = e.world_id;
        i.number = f.pass.rounds;
        i.weapon = f.weapon;
        i.delivery = e.delivery;
        i.end = fx::BurstEnd::Ground;
        i.fire = burnt;
        r.Fx().Push(i);
    }
    static constexpr double kFxRoundMps = 1000.0;   // sam3d: a cannon round's mean speed to the aim
    static constexpr double kFxRocketMps = 600.0;   // sam3d: a rocket's

    // ---- The presentation feed (fx/events.hpp): pushed into the runner's feed, never read back. Draws from fx_rng_.
    static constexpr unsigned kFxSeed = 0xB41D6Eu;     // sam3d: the presentation generator's seed, after the bridge's
    static constexpr double kFxSamFlightS = 20.0;      // a neighbour's missile shown flying at most this long (sam3d)
    static constexpr double kFxRailsM = 3.5;           // its launcher's rails above the ground (sam3d)
    static constexpr double kFxSamLaunchSpeedMps = 30.0;  // off the rails, for the drawing (sam3d)
    static constexpr double kFxHitOffsetM = 5.0;       // a hit's burst within the warhead's reach (sam3d)
    static constexpr double kFxPastMinM = 500.0;       // a missile that missed flies on this far past its target (sam3d)
    static constexpr double kFxPastMaxM = 2000.0;
    static constexpr double kFxPastMps = 500.0;
    static constexpr double kFxArmWideMinM = 30.0;     // an ARM that missed or lost the radar bursts this far wide (sam3d)
    static constexpr double kFxArmWideMaxM = 80.0;
    static constexpr double kFxRadarUpM = 4.0;         // our radar's antenna above the ground (sam3d)

    double FxDraw() { return static_cast<double>(fx_rng_()) / 4294967296.0; }
    double FxBetween(double lo, double hi) { return lo + (hi - lo) * FxDraw(); }
    static fx::Event FxEvent(fx::EventKind kind, double t) {
        fx::Event e;
        e.kind = kind;
        e.t_s = t;
        e.end_t_s = t;
        return e;
    }
    // weapons.json's name for a delivery ("" for none).
    static std::string DeliveryId(Delivery d) { return d == Delivery::Count ? std::string() : std::string(DeliveryName(d)); }
    // The aircraft's speed over the ground now (its type's cruise speed if it has none), never under 1 m/s.
    static double FxSpeed(const ScenarioRunner& r, const RaidAircraft& a) {
        const double v = r.TargetVelocity(a.target_id).HorizontalLength();
        return std::max(1.0, v > 1.0 ? v : a.speed_mps);
    }
    // A neighbour's site on the ground, and its missile's rails.
    static Vec3 FxSite(const ScenarioRunner& r, const RaidNeighbour& nb) { return r.GroundEnu(nb.enu.x, nb.enu.y); }
    // A point lo..hi m from `at` over the ground, on the ground.
    Vec3 FxWide(const ScenarioRunner& r, const Vec3& at, double lo, double hi) {
        const double ang = FxBetween(0.0, kTwoPi);
        const double d = FxBetween(lo, hi);
        return r.GroundEnu(at.x + std::cos(ang) * d, at.y + std::sin(ang) * d);
    }

    void FxCruiseImpact(ScenarioRunner& r, const RaidAircraft& a, const Vec3& pos, const Vec3& where) {
        if (!r.FxOn()) return;
        fx::Event e = FxEvent(fx::EventKind::CruiseImpact, r.Time());
        e.position = pos;
        e.velocity = r.TargetVelocity(a.target_id);
        e.aim = r.GroundEnu(where.x, where.y);
        e.source = a.target_id;
        e.target = m_.own_object;
        e.world_id = r.TargetWorldId(a.target_id);
        e.number = 1;
        e.weapon = a.aircraft;
        e.fire = true;
        r.Fx().Push(e);
    }

    void FxRelease(ScenarioRunner& r, const RaidAircraft& a, const Vec3& pos, const Vec3& vel, const Vec3& ground, double impact_t) {
        if (!r.FxOn()) return;
        fx::Event e = FxEvent(fx::EventKind::WeaponRelease, r.Time());
        e.position = pos;
        e.velocity = vel;
        e.aim = ground;
        e.end_t_s = impact_t;
        e.source = a.target_id;
        e.world_id = r.TargetWorldId(a.target_id);
        e.number = a.weapons;
        e.weapon = a.weapon;
        e.delivery = DeliveryId(a.delivery);
        r.Fx().Push(e);
    }

    // At `at_t`: now in Step, the load's own time when it lands after the session (Finish).
    void FxImpact(ScenarioRunner& r, const RaidAircraft& a, const Vec3& where, bool burnt, double at_t) {
        if (!r.FxOn()) return;
        fx::Event e = FxEvent(fx::EventKind::WeaponImpact, at_t);
        e.position = e.aim = r.GroundEnu(where.x, where.y);
        e.source = a.target_id;
        e.world_id = r.TargetWorldId(a.target_id);
        e.number = a.weapons;
        e.weapon = a.weapon;
        e.delivery = DeliveryId(a.delivery);
        e.end = fx::BurstEnd::Ground;
        e.fire = burnt;
        r.Fx().Push(e);
    }

    // Our radar's antenna, over the ground where the vehicle stands (ENU z is not up far from the origin).
    static Vec3 FxRadar(const ScenarioRunner& r) {
        const Vec3 unit = r.UnitPosition();
        return r.GroundEnu(unit.x, unit.y) + Vec3{0.0, 0.0, kFxRadarUpM};
    }
    // The bridge's ARM model: its launch at our radar, and its burst (on the radar, or wide of it).
    void FxArmLaunch(ScenarioRunner& r, const RaidAircraft& a, const Vec3& pos, double arrive_t) {
        if (!r.FxOn()) return;
        fx::Event e = FxEvent(fx::EventKind::ArmLaunch, r.Time());
        e.position = pos;
        e.velocity = r.TargetVelocity(a.target_id);
        e.aim = FxRadar(r);
        e.end_t_s = arrive_t;
        e.source = a.target_id;
        e.target = m_.own_object;
        e.world_id = r.TargetWorldId(a.target_id);
        e.number = 1;
        e.weapon = a.arm_weapon;
        r.Fx().Push(e);
    }
    void FxArmBurst(ScenarioRunner& r, const RaidAircraft& a, bool hit) {
        if (!r.FxOn()) return;
        fx::Event e = FxEvent(fx::EventKind::ArmBurst, r.Time());
        e.position = e.aim = hit ? FxRadar(r) : FxWide(r, r.UnitPosition(), kFxArmWideMinM, kFxArmWideMaxM);
        e.end = hit ? fx::BurstEnd::Hit : fx::BurstEnd::Ground;
        e.source = a.target_id;
        e.target = m_.own_object;
        e.number = 1;
        e.weapon = a.arm_weapon;
        r.Fx().Push(e);
    }

    // A gun neighbour's pass at an aircraft: from now for `pass_s`, the rounds its guns fire in it. Rounds a minute, the
    // site's guns as one stream, as the game draws them: the munitions library's rate_rpm for the system's round when the
    // bridge has the library (SetMunitions), else the same figures as data/scenery/munitions.json's rounds (wiki_guns,
    // the mount's cyclic rate; the game's drawing breaks it into bursts): the ZSU-23-4 3400 (its four barrels), the
    // ZU-23-2 2000 (two), the S-60 120, the KS-30 12; other AAA as the ZU-23, which the theatres' "aaa" sites are
    // (theatre_pannonia.json: "ZU-23 guns at the Kesmark bridges").
    double GunRoundsPerMin(const std::string& system) const {
        if (munitions_ != nullptr) {
            const scenery::MunitionSpec* m = munitions_->Resolve(system);
            if (m != nullptr && m->IsRound() && m->rate_rpm > 0.0) return m->rate_rpm;
        }
        if (system == "zsu234") return 3400.0;
        if (system == "s60") return 120.0;
        if (system == "ks30") return 12.0;
        return 2000.0;  // zu23, aaa
    }
    // With the guns' tables (AirWarData::aaa, data/gameplay/aaa.json; roadmap S4) the pass is shown as the testbed's
    // guns fire (aaa.hpp): its fire control's errors drawn for the pass, a contact gun's GunPass carrying them, a
    // time-fuzed gun's shells fired one by one at the aircraft as it flies on and bursting where the fuze puts them
    // (FlakBurst, exact: no offset of the picture's own). What the pass does stays its chance per pass (Engage); these
    // are the presentation's own draws (fx_rng_).
    void FxGunPass(ScenarioRunner& r, std::size_t i, std::size_t j, double pass_s) {
        if (!r.FxOn()) return;
        const RaidAircraft& a = m_.aircraft[i];
        const RaidNeighbour& nb = m_.neighbours[j];
        const GunDef* g = d_->aaa.Gun(nb.defence.system);
        const FireControlDef* fc = g != nullptr ? d_->aaa.Control(g->fire_control) : nullptr;
        GunShow show;
        if (g != nullptr) {
            show.aircraft = i;
            show.neighbour = j;
            show.start_s = show.next_group_s = r.Time();
            show.end_s = r.Time() + std::max(0.0, pass_s);
            show.errors = fc != nullptr ? DrawPassErrors(*fc, r.TargetVelocity(a.target_id), m_.night, fx_rng_) : PassErrors{};
            if (g->time_fuze) {
                gun_shows_.push_back(show);
                FxGunShows(r);
                return;
            }
        }
        fx::Event e = FxEvent(fx::EventKind::GunPass, r.Time());
        if (g != nullptr) {
            e.aim_across_rad = show.errors.across_rad;
            e.aim_up_rad = show.errors.up_rad;
            e.range_share = show.errors.range_share;
            e.lead_error = show.errors.velocity_mps;
            e.correction_per_s = fc != nullptr ? fc->correction_per_s : 0.0;
            e.spread_mrad = g->dispersion_mrad;
        }
        e.position = FxSite(r, nb);
        e.aim = r.TargetPosition(a.target_id);
        e.end_t_s = r.Time() + std::max(0.0, pass_s);
        e.source = nb.id;
        e.target = a.target_id;
        e.world_id = r.TargetWorldId(a.target_id);
        e.number = static_cast<int>(std::lround(GunRoundsPerMin(nb.defence.system) / 60.0 * std::max(0.0, pass_s)));
        e.weapon = nb.defence.system;
        r.Fx().Push(e);
    }

    // A time-fuzed gun's pass shown shell by shell (roadmap S4): each group due by now aimed where the fire control
    // predicts the aircraft (its errors as corrected so far), each shell's burst where its fuze puts it (aaa.hpp).
    struct GunShow {
        std::size_t aircraft = 0;
        std::size_t neighbour = 0;
        double start_s = 0.0;
        double end_s = 0.0;
        double next_group_s = 0.0;
        double rounds_due = 0.0;
        PassErrors errors;
    };
    std::vector<GunShow> gun_shows_;
    static constexpr double kGunMuzzleUpM = 2.0;  // the guns' muzzles over the ground (as the runner's and the picture's)
    void FxGunShows(ScenarioRunner& r) {
        for (std::size_t k = 0; k < gun_shows_.size();) {
            GunShow& s = gun_shows_[k];
            const RaidAircraft& a = m_.aircraft[s.aircraft];
            const RaidNeighbour& nb = m_.neighbours[s.neighbour];
            const GunDef* g = d_->aaa.Gun(nb.defence.system);
            const int wid = r.TargetWorldId(a.target_id);
            if (g == nullptr || !r.TargetAlive(a.target_id) || s.next_group_s >= s.end_s || !r.FxOn()) {
                gun_shows_.erase(gun_shows_.begin() + static_cast<std::ptrdiff_t>(k));
                continue;
            }
            const FireControlDef* fc = d_->aaa.Control(g->fire_control);
            const Vec3 pos = r.TargetPosition(a.target_id);
            const Vec3 vel = r.TargetVelocity(a.target_id);
            const Vec3 muzzle = FxSite(r, nb) + Vec3{0.0, 0.0, kGunMuzzleUpM};
            while (s.next_group_s <= r.Time() + 1e-9 && s.next_group_s < s.end_s) {
                const double fired_s = s.next_group_s;
                s.rounds_due += g->rate_rpm / 60.0 * g->group_s;
                s.next_group_s += g->group_s;
                const int n = static_cast<int>(std::floor(s.rounds_due + 1e-9));
                if (n <= 0) continue;
                s.rounds_due -= n;
                const PassErrors e = fc != nullptr ? Corrected(s.errors, *fc, fired_s - s.start_s) : PassErrors{};
                const GunAim aim = AimGun(*g, muzzle, pos - vel * (r.Time() - fired_s), vel, e);
                for (int q = 0; q < n; ++q) {
                    const ShellBurst b = TimeShellBurst(*g, muzzle, aim, fx_rng_);
                    fx::Event ev = FxEvent(fx::EventKind::FlakBurst, fired_s);
                    ev.end_t_s = fired_s + b.after_s;
                    ev.position = muzzle;
                    ev.velocity = b.dir * g->muzzle_mps;
                    ev.aim = b.position;
                    ev.source = nb.id;
                    ev.target = a.target_id;
                    ev.world_id = wid;
                    ev.weapon = nb.defence.system;
                    r.Fx().Push(ev);
                }
            }
            ++k;
        }
    }

    // A missile neighbour's round: SamLaunch from its site toward the aircraft, SamEnd where it bursts. Both carry the
    // aircraft's World id, so the game follows it (fx/events.hpp).
    void FxSamLaunch(ScenarioRunner& r, std::size_t i, std::size_t j, double flight_s) {
        if (!r.FxOn()) return;
        const RaidAircraft& a = m_.aircraft[i];
        const RaidNeighbour& nb = m_.neighbours[j];
        fx::Event e = FxEvent(fx::EventKind::SamLaunch, r.Time());
        e.position = FxSite(r, nb) + Vec3{0.0, 0.0, kFxRailsM};
        e.aim = state_[i].last_pos;
        const Vec3 to = e.aim - e.position;
        const double el = Clamp(ElevationOf(to), DegToRad(20.0), DegToRad(60.0));  // a launcher's elevation (sam3d)
        e.velocity = DirectionFromAzEl(AzimuthOf(to), el) * kFxSamLaunchSpeedMps;
        e.end_t_s = r.Time() + std::max(0.0, flight_s);
        e.source = nb.id;
        e.target = a.target_id;
        e.world_id = r.TargetWorldId(a.target_id);
        e.number = 1;
        e.weapon = nb.defence.system;
        r.Fx().Push(e);
    }
    // ... at the aircraft where the bridge last saw it (this step's place while it flies, its last once it is gone).
    void FxSamEnd(ScenarioRunner& r, std::size_t i, std::size_t j, bool hit) {
        if (!r.FxOn()) return;
        const RaidAircraft& a = m_.aircraft[i];
        const RaidNeighbour& nb = m_.neighbours[j];
        fx::Event e = FxEvent(fx::EventKind::SamEnd, r.Time());
        e.position = FxSite(r, nb) + Vec3{0.0, 0.0, kFxRailsM};
        const Vec3 at = state_[i].last_pos;
        e.source = nb.id;
        e.target = a.target_id;
        e.world_id = r.TargetWorldId(a.target_id);
        e.number = 1;
        e.weapon = nb.defence.system;
        if (hit) {
            e.end = fx::BurstEnd::Hit;
            const double az = FxBetween(0.0, kTwoPi);
            const double elev = std::asin(FxBetween(-1.0, 1.0));
            e.aim = at + DirectionFromAzEl(az, elev) * FxBetween(0.0, kFxHitOffsetM);
        } else {  // it flies on past the aircraft and destroys itself
            e.end = fx::BurstEnd::SelfDestruct;
            Vec3 dir = at - e.position;
            dir = dir.LengthSquared() > 1.0 ? dir.Normalized() : Vec3{0.0, 0.0, 1.0};
            const double past = FxBetween(kFxPastMinM, kFxPastMaxM);
            e.aim = at + dir * past;
            e.velocity = dir * kFxPastMps;
            e.end_t_s = r.Time() + past / kFxPastMps;
        }
        r.Fx().Push(e);
    }
    // As the pass goes on: the round leaves its site when the aircraft is far enough in, and bursts at the hit (the
    // hit itself is the bridge's: Neighbours), or where a hit would have been for a miss, or as the aircraft leaves.
    void FxPass(ScenarioRunner& r, std::size_t i, std::size_t j, bool inside) {
        if (!r.FxOn()) return;
        Pass& ps = passes_[i * m_.neighbours.size() + j];
        if (!ps.sam || ps.sam_ended) return;
        if (!ps.sam_launched && (ps.flown_m >= ps.sam_launch_at_m - 1e-9 || !inside)) {
            ps.sam_launched = true;
            FxSamLaunch(r, i, j, inside ? ps.sam_flight_s : 0.0);
        }
        if (!ps.sam_launched) return;
        const double end_m = ps.hit ? ps.hit_at_m : ps.sam_end_at_m;
        if (ps.flown_m >= end_m - 1e-9 || !inside) {
            ps.sam_ended = true;
            FxSamEnd(r, i, j, ps.hit);
        }
    }
    // A decoy draws fire: the guns' pass, or `missiles` rounds launched and bursting at once (the first on it if it goes down).
    void FxDecoyFire(ScenarioRunner& r, std::size_t i, std::size_t j, int missiles, double chord, bool down) {
        if (!r.FxOn()) return;
        const RaidNeighbour& nb = m_.neighbours[j];
        if (nb.defence.missiles < 0) {
            FxGunPass(r, i, j, chord / FxSpeed(r, m_.aircraft[i]));
            return;
        }
        for (int k = 0; k < missiles; ++k) {
            FxSamLaunch(r, i, j, 0.0);
            FxSamEnd(r, i, j, down && k == 0);
        }
    }
};

}  // namespace airwar
}  // namespace simcore
