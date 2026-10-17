// simcore/scenario_runner.hpp — plays a loaded scenario step by step.
//
// Owns the World (targets), the unit's console, its crew and the session log.
// Each Step(dt), in order:
//   1. targets appear on time, the World moves them, finished ones are marked gone;
//   2. triggers whose conditions hold fire once (set inputs, give orders, end);
//   3. Equipment::Update; a press of the launch button with the launch circuit
//      armed is recorded as a launch;
//   4. ControlPanel::ReleaseMomentaries; crew act; the log collects;
//      (the player acts between steps, through PlayerSet / PlayerTurn);
//   5. a Tacview frame is written once per simulated second; end conditions are checked.
// Deterministic: the only randomness is the crew reaction times, drawn once from
// the scenario seed with a portable mapping (std::mt19937 output is fully
// specified; std:: distributions are not).
//
// If the scenario names a gameplay tuning file, the EngagementModel (engagement.hpp)
// runs the search and tracking picture, sets the console's capture and zone inputs,
// lets the aiming crew turn their wheels, and resolves each launch as a hit or
// a miss (step 1b, and on launch). Without one, launches are only recorded.
#pragma once

#include <cstddef>
#include <cstdio>
#include <algorithm>
#include <array>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "simcore/acmi.hpp"
#include "simcore/console_data.hpp"
#include "simcore/crew_comms.hpp"
#include "simcore/debrief.hpp"
#include "simcore/em_link.hpp"
#include "simcore/engagement.hpp"
#include "simcore/fuel.hpp"
#include "simcore/launcher.hpp"
#include "simcore/c2_sources.hpp"
#include "simcore/pu12.hpp"
#include "simcore/scenario.hpp"
#include "simcore/terrain.hpp"
#include "simcore/world.hpp"

namespace simcore {

class ScenarioRunner {
public:
    ScenarioRunner(ScenarioDef def, ConsoleData console)
        : def_(std::move(def)),
          console_(std::move(console)),
          ground_(def_.ground_height_m),
          world_(def_.origin, &ground_),
          acmi_(world_.Frame(), "2026-01-01T00:00:00Z", def_.name),
          rng_(def_.seed),
          engagement_on_(!def_.tuning_file.empty()),
          engagement_(def_.tuning, def_.unit_position) {
        for (const ScenarioCrew& c : def_.crew) {
            const double u = Uniform01(rng_);
            const double reaction = c.reaction_time_s + (2.0 * u - 1.0) * c.reaction_jitter_s;
            crew_.emplace_back(c.name, c.role, reaction);
            if (engagement_on_) {
                aim_crew_.azimuth = aim_crew_.azimuth || c.role == def_.tuning.aim_az_role;
                aim_crew_.elevation = aim_crew_.elevation || c.role == def_.tuning.aim_el_role;
                aim_crew_.range = aim_crew_.range || c.role == def_.tuning.aim_range_role;
            }
        }
        for (const auto& in : def_.initial_inputs) console_.equipment.SetInput(in.first, in.second);
        const ReadinessState* ready_state = console_.FindReadiness(def_.readiness);
        targets_.resize(def_.targets.size());
        for (const ScenarioTarget& t : def_.targets) {
            EngagementTarget e;
            e.id = t.id;
            e.target_class = t.target_class;
            e.evade_on_launch = t.evade_on_launch;
            e.evade_turn_rate_rad_s = t.evade_turn_rate_rad_s;
            e.friendly = t.friendly;
            eng_targets_.push_back(e);
        }
        fired_.assign(def_.triggers.size(), false);
        engagement_.SetUnitHeading(def_.unit_heading_deg);
        if (ready_state != nullptr) ApplyReadiness(*ready_state);
        if (console_.has_launcher) launcher_ = std::make_unique<Launcher>(console_.launcher);
        if (console_.has_comms) comms_ = std::make_unique<CrewComms>(console_.comms);
        if (console_.has_fuel) fuel_ = std::make_unique<FuelModel>(console_.fuel, def_.fuel_fraction);
        if (!def_.c2_sources.empty()) {
            c2_ = std::make_unique<C2Network>(def_.c2_policy, def_.seed);
            for (const C2SourceDef& s : def_.c2_sources) c2_->AddSource(s, def_.unit_position);
            engagement_.SetAirPicture(c2_.get());
        }
        if (def_.has_em && engagement_on_) {
            em_link_ = std::make_unique<EmSensorLink>(def_.origin, &world_.GetTerrain(), def_.seed, def_.em, def_.unit_position);
            if (em_link_->Ok()) {
                engagement_.SetEmLink(em_link_.get());
            } else {
                em_link_.reset();  // the loader checked the radars; fall back to the gameplay rules rather than crash
            }
        }
        if (c2_ && em_link_) {  // sources with a place talk to the unit over EM radio links
            c2_link_ = std::make_unique<EmC2LinkModel>(em_link_.get());
            for (const C2SourceDef& s : def_.c2_sources) {
                if (s.has_position) em_link_->AddRadioSource(s.id, s.position, s.radio, s.frequency_hz);
            }
            c2_->SetLinkModel(c2_link_.get());
        }
        log_.Record({0.0, DebriefEntry::Kind::Scenario, std::string(), def_.id, "start: " + def_.name});
    }

    ScenarioRunner(const ScenarioRunner&) = delete;
    ScenarioRunner& operator=(const ScenarioRunner&) = delete;

    void Step(double dt) {
        if (finished_) return;
        time_ += dt;
        UpdateTargets(dt);
        DeliverOrders();
        UpdateC2();
        if (engagement_on_) {
            engagement_.Step(time_, dt, world_, console_, eng_targets_, log_, rng_, aim_crew_);
        }
        UpdateLauncher();
        UpdateFuel(dt);
        UpdatePositionPower();
        EvaluateTriggers();
        console_.equipment.Update(dt, console_.panel);
        CheckLaunch();
        CheckEmergencyLaunch();
        console_.panel.ReleaseMomentaries();
        for (CrewMember& c : crew_) c.Update(dt, console_.panel, &console_.equipment);
        player_.Update(dt);
        log_.CollectEquipment(console_.equipment);
        for (CrewMember& c : crew_) log_.CollectCrew(c, time_);
        if (time_ + 1e-9 >= next_acmi_s_) {
            WriteAcmiFrame();
            next_acmi_s_ += 1.0;
        }
        CheckEnd();
    }

    // Steps until the scenario ends. Returns false if `max_s` passed first.
    bool RunToEnd(double dt, double max_s = 1e9) {
        while (!finished_ && time_ < max_s) Step(dt);
        return finished_;
    }

    bool Finished() const { return finished_; }
    const std::string& EndReason() const { return end_reason_; }
    double Time() const { return time_; }
    int Launches() const { return launches_; }

    const ScenarioDef& Def() const { return def_; }
    ConsoleData& Console() { return console_; }
    World& GetWorld() { return world_; }
    const World& GetWorld() const { return world_; }

    // Missiles in flight, for display (empty without the engagement model).
    std::vector<EngagementModel::MissileView> MissilesInFlight() {
        if (!engagement_on_) return {};
        return engagement_.MissilesInFlight(time_, world_, eng_targets_);
    }
    SessionLog& Log() { return log_; }
    const AcmiWriter& Acmi() const { return acmi_; }
    bool EngagementOn() const { return engagement_on_; }
    const EngagementModel& Engagement() const { return engagement_; }

    // The unit moved (e.g. the player drove it). Target ranges, triggers, sensors and the Tacview track follow.
    void SetUnitPosition(const Vec3& enu) {
        def_.unit_position = enu;
        engagement_.SetUnitPosition(enu);
    }
    const Vec3& UnitPosition() const { return def_.unit_position; }
    // The hull turned (e.g. the player drove it): degrees clockwise from north.
    void SetUnitHeading(double heading_deg) {
        def_.unit_heading_deg = heading_deg;
        engagement_.SetUnitHeading(heading_deg);
    }

    // Replace the scenario's flat ground, e.g. with heights sampled from Cesium. Null restores the flat ground.
    // The terrain must outlive the runner or be replaced first.
    void SetTerrain(const Terrain* terrain) {
        world_.SetTerrain(terrain != nullptr ? terrain : &ground_);
        if (em_link_) em_link_->SetTerrain(&world_.GetTerrain());
    }

    // The battery command post's plotting board (null when the scenario has no "command_post").
    const PlottingBoard* Board() const { return c2_ ? c2_->BoardOf("pu12") : nullptr; }

    // The commander's order to a crew member. It goes by the R-124 intercom, so it arrives at once; with the commander's
    // box off, or the member's own box on the radio, it is shouted over the cabin noise and arrives after a delay
    // (roadmap 7.8). The same path serves the scenario's triggers and the player's order menu.
    void GiveOrder(CrewMember& crew, const Procedure& procedure, bool queue) {
        const double delay = comms_ ? comms_->OrderDelayS(crew.Role(), console_) : 0.0;
        if (delay <= 0.0) {
            if (queue) {
                crew.Queue(procedure);
            } else {
                crew.Order(procedure);
            }
            return;
        }
        pending_orders_.push_back({time_ + delay, crew.Name(), &procedure, queue});
        char detail[160];
        std::snprintf(detail, sizeof(detail), "'%s' to %s is shouted, it arrives in %.1f s", procedure.id.c_str(), crew.Name().c_str(), delay);
        log_.Record({time_, DebriefEntry::Kind::Scenario, "Commander", "R-124", detail});
    }
    const CrewComms* Comms() const { return comms_.get(); }
    std::size_t PendingOrders() const { return pending_orders_.size(); }

    // The battery's early-warning and command sources, and which one the crew listens to (null: no command network).
    // `RequestSource` is how a player (or the UI) changes it.
    const C2Network* C2() const { return c2_.get(); }
    // The two R-123M sets (0 left, 1 right): turn one to a preset 1-4, or take the headset to one.
    bool TuneC2Radio(int set, int preset, const std::string& by = "Commander") { return c2_ && c2_->TuneSet(set, preset, time_, by); }
    bool ListenToC2Radio(int set, const std::string& by = "Commander") { return c2_ && c2_->ListenToSet(set, time_, by); }
    bool RequestC2Source(const std::string& id, const std::string& by = "Commander", const std::string& why = std::string()) {
        return c2_ && c2_->RequestSource(id, time_, by, why);
    }

    // The vehicle's fuel (null when the console has no "fuel" block).
    const FuelModel* Fuel() const { return fuel_.get(); }

    // The missile load and guidance channels (null when the console has no "launcher" block).
    Launcher* GetLauncher() { return launcher_.get(); }

    // The electromagnetic layer behind the unit's radars (null when the scenario has no "em" block).
    EmSensorLink* Em() { return em_link_.get(); }

    CrewMember* Crew(const std::string& name) {
        for (CrewMember& c : crew_) {
            if (c.Name() == name) return &c;
        }
        return nullptr;
    }

    // ---- The player's hands on the console (the 3D cabin).
    // Moves go through a ProcedureTracker, the same way a crew member's do, so the debrief records early and
    // out-of-order actions against the procedure the player is working through. No procedure = free play:
    // only physical blocks (covers, locks, key steps) apply.

    // Start judging the player against a procedure of the console; "" for free play. False if unknown.
    bool SetPlayerProcedure(const std::string& procedure_id) {
        if (procedure_id.empty()) {
            player_.Reset(Procedure{});
            return true;
        }
        const Procedure* p = console_.FindProcedure(procedure_id);
        if (p == nullptr) return false;
        player_.Reset(*p);
        log_.Record({time_, DebriefEntry::Kind::Started, kPlayerName, procedure_id, std::string()});
        return true;
    }

    // Move a switch, selector or key, or press a button. The control moves unless the result is Blocked,
    // WaitRequired, UnknownControl or InvalidPosition.
    StepResult PlayerSet(const std::string& control_id, int position) {
        const bool was_complete = player_.IsComplete();
        const StepResult r = player_.Attempt(console_.panel, control_id, position, &console_.equipment);
        log_.RecordAttempt(time_, kPlayerName, player_, r);
        if (!was_complete && player_.IsComplete()) {
            log_.Record({time_, DebriefEntry::Kind::Completed, kPlayerName, player_.GetProcedure().id, std::string()});
        }
        return r;
    }

    // Turn a hand wheel to a value. Wheel work isn't a procedure step (data/procedures/README.md).
    bool PlayerTurn(const std::string& wheel_id, double value) { return console_.panel.SetValue(wheel_id, value); }

    const ProcedureTracker& PlayerTracker() const { return player_; }
    static constexpr const char* kPlayerName = "Player";

    // Distance from the unit to a target, or a negative number if it isn't in the air.
    double TargetRange(const std::string& target_id) {
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            if (def_.targets[i].id != target_id || !targets_[i].spawned) continue;
            const Aircraft* a = world_.FindAircraft(targets_[i].world_id);
            if (a == nullptr || !a->alive) return -1.0;
            return (a->position - def_.unit_position).Length();
        }
        return -1.0;
    }

    std::string Debrief() const {
        char head[512];
        std::snprintf(head, sizeof(head), "Scenario: %s (%s), seed %u, system %s\nEnded at %.1f s: %s\n\n",
                      def_.name.c_str(), def_.id.c_str(), def_.seed, console_.display_name.c_str(), time_,
                      finished_ ? end_reason_.c_str() : "still running");
        return std::string(head) + log_.Format();
    }

private:
    static constexpr int kUnitAcmiId = 0x7FFF0;

    struct TargetState {
        bool spawned = false;
        bool gone = false;
        int world_id = -1;
    };

    // Start warm: set the state's controls and inputs, warm its subsystems up in order (each sees those before it),
    // orient the vehicle if the state says so. The scenario's own inputs win over the state's.
    void ApplyReadiness(const ReadinessState& st) {
        for (const auto& c : st.controls) console_.panel.Set(c.first, c.second);
        for (const auto& in : st.inputs) console_.equipment.SetInput(in.first, in.second);
        for (const auto& in : def_.initial_inputs) console_.equipment.SetInput(in.first, in.second);
        console_.equipment.Update(0.0, console_.panel);
        for (const std::string& id : st.ready) {
            if (!console_.equipment.Warm(id, console_.panel)) {
                Event(id, "not commanded on in readiness '" + st.id + "': left cold");
            }
            console_.equipment.Update(0.0, console_.panel);
        }
        if (st.oriented) engagement_.SetOriented();
        Event(def_.id, "starts " + st.id + (st.description.empty() ? std::string() : ": " + st.description));
    }

    void Event(const std::string& what, const std::string& detail) {
        log_.Record({time_, DebriefEntry::Kind::Scenario, std::string(), what, detail});
    }

    void UpdateTargets(double dt) {
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            const ScenarioTarget& t = def_.targets[i];
            if (targets_[i].spawned || time_ + 1e-9 < t.appear_at_s) continue;
            Aircraft a;
            a.name = t.name;
            a.type = t.type;
            a.position = t.start;
            a.speed_mps = t.speed_mps;
            a.max_turn_rate_rad_s = t.max_turn_rate_rad_s;
            a.rcs_m2 = t.rcs_m2;
            a.waypoints = t.waypoints;
            targets_[i].world_id = world_.AddAircraft(std::move(a));
            targets_[i].spawned = true;
            eng_targets_[i].world_id = targets_[i].world_id;
            Event(t.id, "appeared: " + t.name);
        }
        world_.Step(dt);
        for (const SimEvent& e : world_.DrainEvents()) {
            if (e.type == EventType::AircraftDestroyed) Event(TargetIdOf(e.aircraft_id), "destroyed");
        }
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            TargetState& s = targets_[i];
            if (!s.spawned || s.gone) continue;
            const Aircraft* a = world_.FindAircraft(s.world_id);
            if (a == nullptr || !a->alive) {
                s.gone = true;
            } else if (a->next_waypoint >= a->waypoints.size()) {
                s.gone = true;
                Event(def_.targets[i].id, "passed its last waypoint");
            }
        }
    }

    std::string TargetIdOf(int world_id) const {
        for (std::size_t i = 0; i < targets_.size(); ++i) {
            if (targets_[i].world_id == world_id) return def_.targets[i].id;
        }
        return "?";
    }

    bool TriggerReady(const ScenarioTrigger& t) {
        if (time_ + 1e-9 < t.after_s) return false;
        if (launches_ < t.launches_at_least) return false;
        if (!t.target.empty()) {
            const double r = TargetRange(t.target);
            if (r < 0.0 || r > t.target_within_m) return false;
        }
        return console_.equipment.AllHold(t.conditions, console_.panel);
    }

    void EvaluateTriggers() {
        for (std::size_t i = 0; i < def_.triggers.size(); ++i) {
            if (fired_[i] || finished_) continue;
            const ScenarioTrigger& t = def_.triggers[i];
            if (!TriggerReady(t)) continue;
            fired_[i] = true;
            Event(t.id, "trigger");
            for (const TriggerAction& a : t.actions) Apply(a);
        }
    }

    void Apply(const TriggerAction& a) {
        switch (a.kind) {
            case TriggerAction::Kind::SetInput:
                console_.equipment.SetInput(a.input, a.value);
                return;
            case TriggerAction::Kind::Order:
            case TriggerAction::Kind::Queue: {
                CrewMember* c = Crew(a.crew);
                const Procedure* p = console_.FindProcedure(a.procedure);
                if (c == nullptr || p == nullptr) return;  // checked when the scenario loaded
                GiveOrder(*c, *p, a.kind == TriggerAction::Kind::Queue);
                return;
            }
            case TriggerAction::Kind::End:
                Finish(a.reason);
                return;
            case TriggerAction::Kind::C2Source:
                if (c2_) c2_->RequestSource(a.c2_id, time_, "Scenario");
                return;
            case TriggerAction::Kind::C2Fail:
                if (c2_) c2_->SetFailed(a.c2_id, true, time_, a.reason);
                return;
            case TriggerAction::Kind::C2Restore:
                if (c2_) c2_->SetFailed(a.c2_id, false, time_);
                return;
        }
    }

    // Orders that were shouted reach their crew member after the delay.
    void DeliverOrders() {
        for (std::size_t i = 0; i < pending_orders_.size();) {
            if (pending_orders_[i].at_s > time_ + 1e-9) {
                ++i;
                continue;
            }
            const PendingOrder o = pending_orders_[i];
            pending_orders_.erase(pending_orders_.begin() + static_cast<std::ptrdiff_t>(i));
            if (CrewMember* c = Crew(o.crew)) {
                if (o.queue) {
                    c->Queue(*o.procedure);
                } else {
                    c->Order(*o.procedure);
                }
            }
        }
    }

    // The regiment's picture onto the board, once a minute; the first mark of each target goes in the log.
    void UpdateC2() {
        if (!c2_) return;
        std::vector<BoardContact> contacts;
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            if (!targets_[i].spawned || targets_[i].gone) continue;
            const Aircraft* a = world_.FindAircraft(targets_[i].world_id);
            if (a == nullptr || !a->alive) continue;
            const ScenarioTarget& t = def_.targets[i];
            BoardContact c;
            c.target_id = t.id;
            c.position = a->position;
            c.altitude_m = world_.Frame().AltitudeOf(a->position);
            c.height_agl_m = c.altitude_m - world_.GetTerrain().HeightAt(a->position.x, a->position.y);
            c.type = t.board_type >= 0 ? t.board_type : (t.friendly ? 1 : 9);
            c.formation = t.formation;
            contacts.push_back(c);
        }
        if (comms_) {  // the R-123M sets and the R-124 box as the console has them now
            for (const CrewComms::Event& e : comms_->Step(console_, c2_.get(), c2_link_.get())) {
                log_.Record({time_, DebriefEntry::Kind::Scenario, e.actor, e.subject, e.text});
            }
        }
        c2_->Update(time_, contacts, def_.unit_position);
        SyncRadios();
        for (const C2Network::Event& e : c2_->TakeEvents()) {
            log_.Record({e.t, DebriefEntry::Kind::Scenario, e.actor, e.subject, e.text});
        }
    }

    // The R-123M controls and the command network (the console's radio.left.preset, radio.right.preset, radio.headset): a
    // control the crew moves asks the network to tune a set or move the headset (it follows after the select time); a
    // change the network makes itself (the commander falling back) moves the control. Absent controls are skipped.
    void SyncRadios() {
        static const char* const ids[3] = {"radio.left.preset", "radio.right.preset", "radio.headset"};
        ControlPanel& pan = console_.panel;
        const auto radios = c2_->Radios();
        const int now[3] = {radios[0].preset - 1, radios[1].preset - 1, c2_->ListeningSet()};
        for (int i = 0; i < 3; ++i) {
            if (pan.Find(ids[i]) == nullptr) continue;
            const int p = pan.Get(ids[i]);
            if (!radio_synced_) {
                pan.Set(ids[i], now[i]);
            } else if (p != radio_panel_[i]) {
                radio_want_[i] = p;  // the crew moved it
            } else if (now[i] != radio_now_[i] && radio_want_[i] < 0) {
                pan.Set(ids[i], now[i]);  // the network changed it
            }
            if (radio_want_[i] >= 0) {
                if (now[i] == radio_want_[i]) {
                    radio_want_[i] = -1;
                } else if (i < 2) {
                    c2_->TuneSet(i, radio_want_[i] + 1, time_, "Crew");
                } else {
                    c2_->ListenToSet(radio_want_[i], time_, "Crew");
                }
            }
            radio_panel_[i] = pan.Get(ids[i]);
            radio_now_[i] = now[i];
        }
        radio_synced_ = true;
    }

    // Fuel: burned by the running power sources, pumped by the TZM; at the reserve the crew asks for fuel and the power
    // sources are cut so the rest stays for driving (roadmap 7.4).
    // A prepared position's supply (roadmap 7.6): the grid, then after a cut the diesel set once it has started.
    void UpdatePositionPower() {
        const PositionPowerDef& p = def_.position_power;
        if (!p.present) return;
        const bool cut = p.grid_cut_s >= 0.0 && time_ >= p.grid_cut_s;
        const bool grid = p.grid && !cut;
        const double diesel_from = p.grid_cut_s + p.diesel_start_s;
        const bool diesel = cut && p.diesel_hours > 0.0 && time_ >= diesel_from && time_ < diesel_from + p.diesel_hours * 3600.0;
        const bool supplied = grid || diesel;
        console_.equipment.SetInput("external_power", supplied);
        if (supplied != power_supplied_) {
            power_supplied_ = supplied;
            const char* what = grid ? "grid power on" : diesel ? "the grid is cut: the diesel set carries the position"
                                                               : cut ? "the grid is cut and no diesel set runs: no external power"
                                                                    : "no external power";
            log_.Record({time_, DebriefEntry::Kind::Scenario, "Driver", "position power", what});
        }
    }

    void UpdateFuel(double dt) {
        if (!fuel_) return;
        const FuelModel::Events ev = fuel_->Step(dt, console_);
        char detail[128];
        if (ev.requested) {
            std::snprintf(detail, sizeof(detail), "asks for fuel: %.0f l left, the rest is kept for driving (power cut)", fuel_->LevelL());
            log_.Record({time_, DebriefEntry::Kind::Scenario, "Commander", "fuel", detail});
        }
        if (ev.refuelled) log_.Record({time_, DebriefEntry::Kind::Scenario, "Driver", "fuel", "tank full"});
    }

    // The missile load and channels: shots the outcome model has finished free their channel; then selection at
    // ВНИМАНИЕ and every lamp.
    void UpdateLauncher() {
        if (!launcher_) return;
        if (engagement_on_) {
            for (int n : launcher_->ShotsFlying()) {
                if (engagement_.ShotDone(n)) launcher_->ShotDone(n);
            }
        }
        launcher_->Step(time_, console_);
    }

    void CheckLaunch() {
        if (console_.launch_button.empty() || console_.panel.Get(console_.launch_button) != 1) return;
        if (console_.equipment.IsReady(console_.launch_armed)) {
            std::string from;
            if (launcher_) {
                const std::string why = launcher_->WhyNot(time_, console_);
                if (!why.empty()) {
                    Event(console_.launch_button, "pressed, but " + why + ": no launch");
                    return;
                }
                const int ch = launcher_->NextChannel(time_, console_);
                from = " from " + launcher_->Selected(ch) + ", channel " + Launcher::ChannelName(ch);
            }
            ++launches_;
            log_.Record({time_, DebriefEntry::Kind::Launch, std::string(), console_.launch_button,
                         "missile " + std::to_string(launches_) + from +
                             (engagement_on_ ? std::string() : std::string(" (no engagement model: not resolved)"))});
            if (engagement_on_) {
                const std::string& jam = console_.has_launcher ? console_.launcher.jam_control : std::string();
                engagement_.OnLaunch(time_, launches_, world_, eng_targets_, log_, !jam.empty() && console_.panel.Get(jam) == 1);
            }
            if (launcher_) {
                const double flight_s = engagement_on_ ? engagement_.ShotFlightTime(launches_) : 0.0;
                launcher_->Launch(time_, console_, launches_, flight_s,
                                  engagement_on_ ? def_.tuning.missile_average_speed_mps : 0.0);
                // The next pair has to be selected and prepared before the circuit is armed again; a pair's second
                // missile only waits for the interval between launches.
                if (launcher_->PairSpent()) console_.equipment.Restart(console_.launcher.pair_preparation);
                launcher_->Step(time_, console_);  // the lamps follow at once (СХОД; the hold puts ПУСК ГОТОВ out)
                console_.equipment.Update(0.0, console_.panel);
            } else if (!console_.launch_restarts.empty()) {
                // The next missile has to be prepared before the circuit is armed again (ПУСК ГОТОВ goes out).
                console_.equipment.Restart(console_.launch_restarts);
            }
        } else {
            Event(console_.launch_button, "pressed, but " + console_.launch_armed + " is not ready: no launch");
        }
    }

    // АВАРИЙНЫЙ ПУСК: the missile ИЗДЕЛИЯ is set to goes off unguided and self-destructs; no channel, no outcome.
    void CheckEmergencyLaunch() {
        if (!launcher_ || !console_.launcher.has_emergency) return;
        const std::string& button = console_.launcher.emergency.fire_control;
        if (console_.panel.Get(button) != 1) return;
        const std::string container = launcher_->EmergencyLaunch(time_, console_);
        if (container.empty()) {
            Event(button, "pressed, but ИЗДЕЛИЯ is not on a missile: nothing thrown off");
            return;
        }
        Event(button, "missile " + container + " thrown off unguided; it climbs " +
                          std::to_string(static_cast<int>(console_.launcher.emergency.flight_s + 0.5)) + " s, then self-destructs");
        if (launcher_->PairSpent()) console_.equipment.Restart(console_.launcher.pair_preparation);
        launcher_->Step(time_, console_);
        console_.equipment.Update(0.0, console_.panel);
    }

    void WriteAcmiFrame() {
        acmi_.WriteWorld(world_);
        acmi_.WriteObject(kUnitAcmiId, def_.unit_position, Vec3{},
                          "Type=Ground+AntiAircraft,Name=" + console_.display_name + ",Color=Red");
        if (engagement_on_) engagement_.WriteAcmi(acmi_, time_, world_, eng_targets_);
    }

    void CheckEnd() {
        if (finished_) return;
        if (time_ + 1e-9 >= def_.time_limit_s) {
            Finish("time limit");
            return;
        }
        if (!def_.end_when_targets_gone || def_.targets.empty()) return;
        if (engagement_on_ && engagement_.ShotsInFlight()) return;
        for (const TargetState& s : targets_) {
            if (!s.spawned || !s.gone) return;
        }
        Finish("all targets gone");
    }

    void Finish(const std::string& reason) {
        if (finished_) return;
        finished_ = true;
        end_reason_ = reason;
        Event(def_.id, "end: " + reason);
        WriteAcmiFrame();
    }

    ScenarioDef def_;
    ConsoleData console_;
    FlatTerrain ground_;
    World world_;
    AcmiWriter acmi_;
    std::mt19937 rng_;
    bool engagement_on_;
    std::unique_ptr<EmSensorLink> em_link_;  // before engagement_: the model keeps a pointer to it
    std::unique_ptr<Launcher> launcher_;
    bool power_supplied_ = false;
    std::unique_ptr<FuelModel> fuel_;
    struct PendingOrder {
        double at_s = 0.0;
        std::string crew;
        const Procedure* procedure = nullptr;
        bool queue = false;
    };
    std::vector<PendingOrder> pending_orders_;
    std::unique_ptr<CrewComms> comms_;
    bool radio_synced_ = false;
    int radio_panel_[3] = {-1, -1, -1};
    int radio_now_[3] = {-1, -1, -1};
    int radio_want_[3] = {-1, -1, -1};
    std::unique_ptr<EmC2LinkModel> c2_link_;
    std::unique_ptr<C2Network> c2_;  // before engagement_ is fine: the model keeps a pointer, set in the constructor
    EngagementModel engagement_;
    AimCrew aim_crew_;
    std::vector<EngagementTarget> eng_targets_;
    std::vector<CrewMember> crew_;
    ProcedureTracker player_;  // empty procedure = free play
    std::vector<TargetState> targets_;
    std::vector<bool> fired_;
    SessionLog log_;
    double time_ = 0.0;
    double next_acmi_s_ = 0.0;
    int launches_ = 0;
    bool finished_ = false;
    std::string end_reason_;
};

}  // namespace simcore
