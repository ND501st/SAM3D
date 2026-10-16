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
#include <map>
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
#include "simcore/battery.hpp"
#include "simcore/c2_sources.hpp"
#include "simcore/console_sync.hpp"
#include "simcore/crew_callouts.hpp"
#include "simcore/drill_master.hpp"
#include "simcore/em/replication.hpp"
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
          ground_(def_.ground_height_m, def_.ridges),
          world_(def_.origin, &ground_),
          acmi_(world_.Frame(), "2026-01-01T00:00:00Z", def_.name),
          rng_(def_.seed),
          engagement_on_(!def_.tuning_file.empty()),
          engagement_(def_.tuning, def_.unit_position) {
        for (std::size_t i = 0; i < def_.crew.size(); ++i) {
            const ScenarioCrew& c = def_.crew[i];
            const double u = Uniform01(rng_);
            const double reaction = c.reaction_time_s + (2.0 * u - 1.0) * c.reaction_jitter_s;
            crew_.emplace_back(c.name, c.role, reaction);
            if (const SkillTier* tier = SkillOf(c)) {  // docs/crew/05_SKILL.md
                CrewSkill sk = ToCrewSkill(*tier);
                if (def_.has_crew_data) sk.decision_controls = def_.crew_data.decision_controls;
                crew_.back().SetSkill(sk, def_.seed * 7919u + static_cast<unsigned>(i) * 104729u + 17u);
            }
        }
        UpdateAimCrew();
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
        if (def_.has_battery) {
            battery_ = std::make_unique<Battery>(def_.battery_tactics, def_.battery_vehicles);
            battery_->SetDriveOwn(def_.battery_drive_own);
            battery_->SetNight(def_.battery_night);
            if (def_.prepared.present && def_.prepared.emplacement) battery_->SetInEmplacement(def_.prepared.values.leave_emplacement_s);
            battery_orders_done_.assign(def_.battery_orders.size(), 0);
        }
        StartCallouts();
        log_.Record({0.0, DebriefEntry::Kind::Scenario, std::string(), def_.id, "start: " + def_.name});
    }

    ScenarioRunner(const ScenarioRunner&) = delete;
    ScenarioRunner& operator=(const ScenarioRunner&) = delete;

    void Step(double dt) {
        if (finished_) return;
        time_ += dt;
        UpdateTargets(dt);
        DeliverOrders();
        UpdateBattery(dt);
        UpdateC2();
        if (engagement_on_) {
            aim_crew_.assigned = assigned_target_;
            engagement_.Step(time_, dt, world_, console_, eng_targets_, log_, rng_, aim_crew_);
            // A radar lock is heard by the target's warning receiver; a TV track is not.
            const TrackMethod m = engagement_.Method();
            if (m == TrackMethod::Radar || m == TrackMethod::RadarOnJam) CueTarget(engagement_.TrackedTarget(), false);
        }
        UpdateLauncher();
        UpdateFuel(dt);
        UpdatePositionPower();
        EvaluateTriggers();
        console_.equipment.Update(dt, console_.panel);
        CheckLaunch();
        CheckEmergencyLaunch();
        console_.panel.ReleaseMomentaries();
        for (CrewMember& c : crew_) {
            if (!SeatHolder(c.Role()).empty()) continue;  // a player sits there
            c.Update(dt, console_.panel, &console_.equipment);
        }
        player_.Update(dt);
        log_.CollectEquipment(console_.equipment);
        for (CrewMember& c : crew_) {
            std::vector<CrewReport> reports = c.TakeReports();
            CrewCallouts(c, reports);
            log_.CollectCrewReports(c.Name(), std::move(reports), time_);
        }
        SeeInstruments();
        UpdateCallouts();
        UpdateStress();
        UpdateNpcCommander();
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
    const ConsoleData& Console() const { return console_; }
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
        SayOrder(crew.Role(), procedure.id);
        const double delay = comms_ ? comms_->OrderDelayS(crew.Role(), console_) : 0.0;
        if (delay <= 0.0) {
            Hand(crew, procedure, queue);
            return;
        }
        pending_orders_.push_back({time_ + delay, crew.Name(), &procedure, queue});
        Radio("cabin (shouted)", "Commander", crew.Name() + ": " + procedure.id);
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
    // Radio and intercom traffic the crew heard, oldest first (the last kRadioKept): the command net (designations, source
    // changes), the battery net (the battery commander's orders) and orders shouted in the cabin.
    struct RadioMessage {
        double time_s = 0.0;
        std::string net;
        std::string from;
        std::string text;
    };
    static constexpr std::size_t kRadioKept = 64;
    const std::vector<RadioMessage>& RadioLog() const { return radio_; }

    // What the player's crew has in front of it besides its knowledge (roadmap 7.7): the PU-12 board's marks of the last
    // five plots, the last `radio_count` radio messages, and the battery's other vehicles and dummy positions.
    em::ViewExtras ViewExtrasNow(std::size_t radio_count = 8) const {
        em::ViewExtras x;
        x.valid = true;
        if (const PlottingBoard* b = Board()) {
            for (const BoardMark& m : b->Marks()) {
                if (m.stamp + 4 < b->Stamp()) continue;
                x.board.push_back({m.number, m.stamp, m.height_hm, m.Code(), m.position});
            }
        }
        const std::size_t from = radio_.size() > radio_count ? radio_.size() - radio_count : 0;
        for (std::size_t i = from; i < radio_.size(); ++i) x.radio.push_back({radio_[i].time_s, radio_[i].net, radio_[i].from, radio_[i].text});
        if (battery_) {
            for (const Battery::Vehicle& v : battery_->Vehicles()) {
                if (v.def.own) continue;
                x.battery.push_back({v.def.callsign, BatteryVehicleKindName(v.def.kind), v.position, v.heading_deg, v.deployed});
            }
        }
        for (std::size_t i = 0; i < def_.prepared.dummies.size(); ++i) {
            x.battery.push_back({"dummy " + std::to_string(i + 1), "dummy", def_.prepared.dummies[i].position, 0.0, true});
        }
        return x;
    }

    // The prepared position the unit stands in (present = false: none).
    const PreparedPositionDef& Prepared() const { return def_.prepared; }

    // The battery around the player's vehicle (null: the scenario has none).
    const Battery* GetBattery() const { return battery_.get(); }
    bool OrderBattery(const BatteryOrder& o) {
        if (!battery_) return false;
        battery_->Order(o, time_);
        return true;
    }

    // Which wheels the NPC crew aims (none at a seat a player holds).
    AimCrew AimFlags() const { return aim_crew_; }

    // ---- The crew's voice (roadmap 8.4, docs/crew/04_COMMANDS_AND_CALLOUTS.md): orders, acknowledgements, reports
    // and reminders as they were spoken (null without crew data). They also go to the radio log and the debrief.
    const CalloutBus* Callouts() const { return callouts_.get(); }
    // The drill master on this console (null without crew data): the best move of any seat toward a task.
    const DrillMaster* Drill() const { return drill_.get(); }
    // The order menu's backend (roadmap 8.8): give a whole order of the crew data (an order callout id, e.g.
    // "order_k_boyu") as the commander: its drills go to their seats (a player's seat gets them as orders for the player).
    // False if there is no such order. `fields` fill the spoken words ({number}, ...).
    bool GiveCrewOrder(const std::string& order_id) {
        if (!def_.has_crew_data) return false;
        const OrderDef* o = def_.crew_data.FindOrder(order_id);
        if (o == nullptr) return false;
        SpeakOrder(order_id);
        for (const OrderStart& s : o->procedures) {
            CrewMember* m = MemberAt(s.station);
            const Procedure* p = console_.FindProcedure(s.procedure);
            if (m != nullptr && p != nullptr) GiveOrder(*m, *p, s.queue);
        }
        return true;
    }
    bool NpcCommanderActive() const { return def_.npc_commander && SeatHolder("commander").empty(); }

    // Reminders each seat has been given (the seat that was waited for), for the debrief and grading.
    int RemindersTo(const std::string& station) const {
        const auto it = reminders_to_.find(station);
        return it != reminders_to_.end() ? it->second : 0;
    }

    // ---- Multiplayer crew: players in the crew's seats (one per station role).
    // A player in a seat replaces the NPC there: the NPC stops acting (and stops aiming its wheel), and orders for that
    // seat become the player's procedure, judged in the debrief under the player's name. A player may only work the
    // controls of their own station (a control with no station is anyone's). The server calls these on behalf of the
    // players; each player's machine mirrors the console (console_sync.hpp).

    // Take a seat. False for a role the console doesn't have, or a seat another player holds.
    bool TakeSeat(const std::string& role, const std::string& player) {
        if (player.empty() || (!console_.stations.empty() && console_.FindStation(role) == nullptr)) return false;
        for (Seat& st : seats_) {
            if (st.role != role) continue;
            if (st.player != player) return false;
            return true;
        }
        Seat st;
        st.role = role;
        st.player = player;
        if (CrewMember* c = CrewAt(role)) {
            // the NPC's current work passes to the player
            if (c->IsBusy()) st.tracker = c->Tracker();  // carry on where the NPC was
            c->CancelOrder();
        }
        seats_.push_back(st);
        UpdateAimCrew();
        log_.Record({time_, DebriefEntry::Kind::Scenario, player, role, "takes the seat"});
        return true;
    }
    void LeaveSeat(const std::string& role) {
        for (std::size_t i = 0; i < seats_.size(); ++i) {
            if (seats_[i].role != role) continue;
            log_.Record({time_, DebriefEntry::Kind::Scenario, seats_[i].player, role, "leaves the seat (the NPC takes over)"});
            seats_.erase(seats_.begin() + static_cast<std::ptrdiff_t>(i));
            UpdateAimCrew();
            return;
        }
    }
    // Who sits there ("" for the NPC).
    std::string SeatHolder(const std::string& role) const {
        for (const Seat& st : seats_)
            if (st.role == role) return st.player;
        return std::string();
    }
    // The procedure the player in a seat is being judged against (null: no player there).
    const ProcedureTracker* SeatTracker(const std::string& role) const {
        for (const Seat& st : seats_)
            if (st.role == role) return &st.tracker;
        return nullptr;
    }
    bool SetSeatProcedure(const std::string& role, const std::string& procedure_id) {
        Seat* st = FindSeat(role);
        if (st == nullptr) return false;
        if (procedure_id.empty()) {
            st->tracker.Reset(Procedure{});
            return true;
        }
        const Procedure* p = console_.FindProcedure(procedure_id);
        if (p == nullptr) return false;
        st->tracker.Reset(*p);
        log_.Record({time_, DebriefEntry::Kind::Started, st->player, procedure_id, std::string()});
        return true;
    }
    // A seated player moves a control. Blocked (with `why`) if no player sits there or it is another station's control.
    StepResult SeatSet(const std::string& role, const std::string& control_id, int position, std::string* why = nullptr) {
        Seat* st = FindSeat(role);
        if (st == nullptr) {
            if (why != nullptr) *why = "nobody sits at '" + role + "'";
            return StepResult::Blocked;
        }
        if (!AtStation(role, control_id, why)) return StepResult::Blocked;
        const bool was_complete = st->tracker.IsComplete();
        const StepResult r = st->tracker.Attempt(console_.panel, control_id, position, &console_.equipment);
        log_.RecordAttempt(time_, st->player, st->tracker, r);
        if (!was_complete && st->tracker.IsComplete() && !st->tracker.GetProcedure().id.empty()) {
            log_.Record({time_, DebriefEntry::Kind::Completed, st->player, st->tracker.GetProcedure().id, std::string()});
            if (!st->queued.empty()) {
                const Procedure* next = console_.FindProcedure(st->queued.front());
                st->queued.erase(st->queued.begin());
                if (next != nullptr) st->tracker.Reset(*next);
            }
        }
        return r;
    }
    bool SeatTurn(const std::string& role, const std::string& wheel_id, double value, std::string* why = nullptr) {
        if (FindSeat(role) == nullptr || !AtStation(role, wheel_id, why)) return false;
        return console_.panel.SetValue(wheel_id, value);
    }
    // The console's whole state for the players' machines (console_sync.hpp).
    std::vector<std::uint8_t> ConsoleSnapshot(std::uint32_t seq) const { return EncodeConsoleState(console_, seq, time_); }
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
        TargetPilot pilot;  // terrain following, pop-up, evasion (target_behaviour.hpp)
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
            targets_[i].pilot = TargetPilot(t.behaviour, t.evade);
            targets_[i].pilot.OnSpawn(a);
            targets_[i].world_id = world_.AddAircraft(std::move(a));
            targets_[i].spawned = true;
            eng_targets_[i].world_id = targets_[i].world_id;
            Event(t.id, "appeared: " + t.name);
        }
        for (TargetState& s : targets_) {
            if (!s.spawned || s.gone) continue;
            if (Aircraft* a = world_.FindAircraft(s.world_id)) s.pilot.BeforeStep(*a);
        }
        world_.Step(dt);
        for (std::size_t i = 0; i < targets_.size(); ++i) {
            TargetState& s = targets_[i];
            if (!s.spawned || s.gone) continue;
            Aircraft* a = world_.FindAircraft(s.world_id);
            if (a == nullptr) continue;
            const std::string what = s.pilot.AfterStep(*a, world_.GetTerrain(), world_.Frame(), dt, time_);
            if (!what.empty()) Event(def_.targets[i].id, what);
        }
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

    // A missile was launched at a target (`launch`), or the tracking radar holds it: its pilot may evade.
    void CueTarget(const std::string& id, bool launch) {
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            TargetState& s = targets_[i];
            if (def_.targets[i].id != id || !s.spawned || s.gone) continue;
            Aircraft* a = world_.FindAircraft(s.world_id);
            if (a == nullptr) return;
            const std::string what = launch ? s.pilot.OnLaunch(*a, def_.unit_position, time_)
                                            : s.pilot.OnLocked(*a, def_.unit_position, time_);
            if (!what.empty()) Event(id, what);
            return;
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
            case TriggerAction::Kind::BatteryOrder:
                if (battery_) battery_->Order(a.battery_order, time_);
                return;
            case TriggerAction::Kind::AirThreat:
                if (battery_) {
                    battery_->SetAirThreat(a.air_threat, time_);
                    air_threat_forced_ = a.air_threat;
                }
                return;
        }
    }

    // The battery around the player's vehicle (roadmap 7.5): scheduled orders, the air-threat spacing from what the crew
    // has been told, the vehicles' moves; the other launchers' positions feed their neighbour sources.
    void UpdateBattery(double dt) {
        if (!battery_) return;
        for (std::size_t i = 0; i < def_.battery_orders.size(); ++i) {
            if (battery_orders_done_[i] || def_.battery_orders[i].at_s > time_ + 1e-9) continue;
            battery_orders_done_[i] = 1;
            battery_->Order(def_.battery_orders[i], time_);
        }
        if (c2_ && !air_threat_forced_) {
            bool close = false;
            for (const BoardMark& m : c2_->Known(time_)) {
                close = close || (m.position - def_.unit_position).HorizontalLength() <= def_.battery_tactics.threat_range_m;
            }
            battery_->SetAirThreat(close, time_);
        }
        if (!def_.battery_drive_own) battery_->SetOwnPosition(def_.unit_position);
        ServiceDrills();
        battery_->Step(time_, dt);
        // The TZM or fuel truck alongside: the console's inputs (the launcher's crane and the fuel model act on them).
        bool has_tzm = false, has_fuel = false;
        for (const Battery::Vehicle& v : battery_->Vehicles()) {
            has_tzm = has_tzm || v.def.kind == BatteryVehicleKind::Reload;
            has_fuel = has_fuel || v.def.kind == BatteryVehicleKind::Fuel;
        }
        if (has_tzm) {
            console_.equipment.SetInput("tzm_alongside", battery_->Alongside("", Battery::Service::Reload) ||
                                                             battery_->Alongside("", Battery::Service::Refuel));
        }
        if (has_fuel && console_.equipment.HasInput("fuel_truck_alongside")) {
            console_.equipment.SetInput("fuel_truck_alongside", battery_->Alongside("", Battery::Service::Refuel));
        }
        if (def_.battery_drive_own) {
            if (const Battery::Vehicle* own = battery_->Own()) {
                SetUnitPosition(Vec3{own->position.x, own->position.y, def_.unit_position.z});
                if (own->speed_mps > 0.1) SetUnitHeading(own->heading_deg);
                console_.equipment.SetInput("vehicle_stopped", own->speed_mps <= 0.1);
            }
        }
        if (c2_) {
            for (const Battery::Vehicle& v : battery_->Vehicles()) {
                if (!v.def.own) c2_->SetSourcePosition(v.def.id, v.position);
            }
        }
        for (const Battery::Event& e : battery_->TakeEvents()) {
            log_.Record({e.t, DebriefEntry::Kind::Scenario, e.who, "battery", e.text});
            Radio("battery net", e.who, e.text);
        }
    }

    // The reload and refuel drills: an empty launcher calls the TZM; a crew asking for fuel calls the fuel truck (or the
    // TZM). What goes across is counted off the service vehicle; it goes back when the vehicle is full or it is empty.
    void ServiceDrills() {
        if (launcher_) {
            const int loaded = launcher_->LoadedCount();
            if (last_loaded_ >= 0 && loaded > last_loaded_) battery_->Consume("", Battery::Service::Reload, loaded - last_loaded_, time_);
            last_loaded_ = loaded;
            if (loaded == 0 && launcher_->ShotsFlying().empty()) battery_->RequestService("", Battery::Service::Reload, time_);
            if (loaded == launcher_->Capacity()) battery_->Done("", Battery::Service::Reload, time_);
        }
        if (fuel_) {
            const double level = fuel_->LevelL();
            if (last_fuel_l_ >= 0.0 && level > last_fuel_l_) battery_->Consume("", Battery::Service::Refuel, level - last_fuel_l_, time_);
            last_fuel_l_ = level;
            if (fuel_->Requested()) battery_->RequestService("", Battery::Service::Refuel, time_);
            if (level >= fuel_->TankL() - 1e-6) battery_->Done("", Battery::Service::Refuel, time_);
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
            if (CrewMember* c = Crew(o.crew)) Hand(*c, *o.procedure, o.queue);
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
        for (const C2Network::Event& e : c2_->PeekEvents()) {
            Radio("command net", e.actor, e.subject + ": " + e.text);
            if (e.actor == "Commander" && e.number > 0) {  // the commander heard it on the radio, and passes it on
                designations_.push_back(e);
                designations_heard_.push_back(e);
                KnownTarget k;
                k.target = e.subject;
                k.number = e.number;
                k.has_azimuth = k.has_range = k.true_azimuth = true;
                k.azimuth_deg = e.azimuth_mils * 360.0 / 6000.0;
                k.range_m = e.range_m;
                k.time_s = time_;
                k.source = "radio";
                pictures_["commander"].Learn(k);
            }
        }
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
                CueTarget(engagement_.TrackedTarget(), true);
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
        for (std::size_t i = 0; i < def_.prepared.dummies.size(); ++i) {
            acmi_.WriteObject(kUnitAcmiId + 100 + static_cast<int>(i), def_.prepared.dummies[i].position, Vec3{},
                              "Type=Ground+Static,Name=Dummy position,Color=Red");
        }
        if (battery_) {
            int k = 0;
            for (const Battery::Vehicle& v : battery_->Vehicles()) {
                ++k;
                if (v.def.own) continue;
                acmi_.WriteObject(kUnitAcmiId + k, Vec3{v.position.x, v.position.y, def_.unit_position.z}, Vec3{},
                                  std::string("Type=Ground+") + (v.def.kind == BatteryVehicleKind::Launcher ? "AntiAircraft" : "Vehicle") +
                                      ",Name=" + v.def.callsign + ",Color=Red");
            }
        }
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
        if (callouts_) {  // what the crew was about to say is still said (the last result, a last report)
            for (double t = time_; t < time_ + 15.0 && (callouts_->Pending() > 0 || callouts_->Speaking("intercom") ||
                                                          callouts_->Speaking("battery net"));
                 t += 0.1) {
                callouts_->Step(t);
                for (const SpokenCallout& sc : callouts_->TakeHeard()) {
                    const std::string who = sc.from == "battery" ? std::string("Battery") : SpeakerName(sc.from);
                    Radio(sc.channel, who, sc.ru + " (" + sc.en + ")");
                    log_.Record({sc.start_s, DebriefEntry::Kind::Scenario, who, "says", sc.ru + " (" + sc.en + ")"});
                }
            }
        }
    }

    ScenarioDef def_;
    ConsoleData console_;
    RidgeTerrain ground_;  // the scenario's flat ground and its ridges
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

    struct Seat {
        std::string role;
        std::string player;
        ProcedureTracker tracker;
        std::vector<std::string> queued;  // procedures ordered while one was under way
    };
    std::vector<Seat> seats_;
    std::unique_ptr<Battery> battery_;
    std::vector<RadioMessage> radio_;
    void Radio(const std::string& net, const std::string& from, const std::string& text) {
        radio_.push_back({time_, net, from, text});
        if (radio_.size() > kRadioKept) radio_.erase(radio_.begin());
    }
    std::vector<char> battery_orders_done_;
    bool air_threat_forced_ = false;
    int last_loaded_ = -1;
    double last_fuel_l_ = -1.0;

    Seat* FindSeat(const std::string& role) {
        for (Seat& st : seats_)
            if (st.role == role) return &st;
        return nullptr;
    }
    CrewMember* CrewAt(const std::string& role) {
        for (CrewMember& c : crew_)
            if (c.Role() == role) return &c;
        return nullptr;
    }
    bool AtStation(const std::string& role, const std::string& control_id, std::string* why) const {
        const ControlDef* d = console_.panel.Find(control_id);
        if (d == nullptr) {
            if (why != nullptr) *why = "no control '" + control_id + "'";
            return false;
        }
        if (!d->station.empty() && d->station != role) {
            if (why != nullptr) *why = "'" + control_id + "' is at the " + d->station + "'s station";
            return false;
        }
        return true;
    }
    // An order reaches a crew member, or the player in that seat.
    void Hand(CrewMember& crew, const Procedure& procedure, bool queue) {
        if (Seat* st = FindSeat(crew.Role())) {
            const bool busy = !st->tracker.IsComplete() && !st->tracker.GetProcedure().id.empty();
            if (queue && busy) {
                st->queued.push_back(procedure.id);
            } else {
                st->tracker.Reset(procedure);
                st->queued.clear();
            }
            log_.Record({time_, DebriefEntry::Kind::Scenario, st->player, procedure.id,
                         std::string(queue && busy ? "order queued for the player" : "order for the player")});
            return;
        }
        if (queue) {
            crew.Queue(procedure);
        } else {
            crew.Order(procedure);
        }
    }
    // NPCs aim the wheels of the roles they sit in, unless a player sits there.
    void UpdateAimCrew() {
        aim_crew_ = AimCrew{};
        if (!engagement_on_) return;
        for (std::size_t i = 0; i < crew_.size(); ++i) {
            const CrewMember& c = crew_[i];
            if (!SeatHolder(c.Role()).empty()) continue;
            const SkillTier* tier = i < def_.crew.size() ? SkillOf(def_.crew[i]) : nullptr;
            if (c.Role() == def_.tuning.aim_az_role) {
                aim_crew_.azimuth = true;
                if (tier != nullptr) {
                    aim_crew_.az_rate_factor = tier->wheel_rate_factor;
                    aim_crew_.az_lag_s = tier->tv_lag_s;
                    aim_crew_.az_noise_deg = tier->aim_noise_deg;
                }
            }
            if (c.Role() == def_.tuning.aim_el_role) {
                aim_crew_.elevation = true;
                if (tier != nullptr) {
                    aim_crew_.el_rate_factor = tier->wheel_rate_factor;
                    aim_crew_.el_lag_s = tier->tv_lag_s;
                    aim_crew_.el_noise_deg = tier->aim_noise_deg;
                }
            }
            if (c.Role() == def_.tuning.aim_range_role) {
                aim_crew_.range = true;
                if (def_.has_crew_data) aim_crew_.range_knows = &pictures_[c.Role()];  // range only from what he sees or hears
                if (tier != nullptr) {
                    aim_crew_.range_rate_factor = tier->wheel_rate_factor;
                    aim_crew_.range_error_m = tier->range_error_m;
                }
            }
        }
    }
    const SkillTier* SkillOf(const ScenarioCrew& c) const {
        return def_.has_skills && !c.skill.empty() ? def_.skills.Find(c.skill) : nullptr;
    }

    // ---- Callouts (roadmap 8.4).
    std::unique_ptr<CalloutBus> callouts_;
    std::unique_ptr<DrillMaster> drill_;
    std::vector<char> report_state_;  // each automatic report's condition last step
    struct WaitWatch {
        double since_s = -1.0;
        int reminded = 0;
    };
    std::vector<WaitWatch> waits_;  // per crew member
    std::map<std::string, int> reminders_to_;
    std::vector<C2Network::Event> designations_;  // heard this step, to pass on

    void StartCallouts() {
        if (!def_.has_crew_data) return;
        callouts_ = std::make_unique<CalloutBus>(&def_.crew_data);
        drill_ = std::make_unique<DrillMaster>(console_, &def_.crew_data, def_.has_skills && def_.skills.Reference() != nullptr
                                                                             ? def_.skills.Reference()->reaction_s : 0.55);
        for (const AutoReportDef& r : def_.crew_data.reports) {
            report_state_.push_back(console_.equipment.Holds(r.when, console_.panel) ? 1 : 0);
        }
        waits_.assign(crew_.size(), WaitWatch{});
    }

    CrewMember* MemberAt(const std::string& station) {
        for (CrewMember& c : crew_) {
            if (c.Role() == station) return &c;
        }
        return nullptr;
    }
    // A seat speaks for itself only when an NPC holds it (a player speaks with his own voice).
    bool NpcAt(const std::string& station) { return MemberAt(station) != nullptr && SeatHolder(station).empty(); }
    // How long the crewmate at a seat takes to notice and speak (skill), stressed or not.
    double SpeakDelayS(const std::string& station) {
        for (std::size_t i = 0; i < crew_.size() && i < def_.crew.size(); ++i) {
            if (crew_[i].Role() != station) continue;
            if (const SkillTier* t = SkillOf(def_.crew[i])) {
                return (t->perception_s + t->callout_delay_s) * (crew_[i].Stressed() ? t->stress_factor : 1.0);
            }
        }
        return 0.6;
    }
    std::string SpeakerName(const std::string& station) {
        if (const CrewMember* c = MemberAt(station)) return c->Name();
        if (const CrewSeatDef* s = def_.crew_data.FindSeat(station)) return s->title_en;
        return station;
    }
    // The seat's word in a reminder: the start of its Russian title ("Оператор", "Старший оператор").
    std::string SeatWord(const std::string& station, bool english) const {
        const CrewSeatDef* s = def_.crew_data.FindSeat(station);
        if (s == nullptr) return station;
        std::string t = english ? s->title_en : s->title_ru;
        const std::size_t cut = t.find(" (");
        return cut == std::string::npos ? t : t.substr(0, cut);
    }

    void Say(const std::string& id, const std::string& station, const std::map<std::string, std::string>& fields = {},
             const KnownTarget* about = nullptr) {
        if (callouts_) callouts_->Say(time_, id, station, fields, NpcAt(station) ? SpeakDelayS(station) : 0.0, about);
    }

    // The commander's order for a procedure, if the crew data has one: the most specific order that starts it.
    void SayOrder(const std::string& station, const std::string& procedure) {
        if (!callouts_) return;
        const OrderDef* best = nullptr;
        for (const OrderDef& o : def_.crew_data.orders) {
            for (const OrderStart& s : o.procedures) {
                if (s.station == station && s.procedure == procedure && (best == nullptr || o.procedures.size() < best->procedures.size())) best = &o;
            }
        }
        if (best != nullptr) SpeakOrder(best->callout);
    }
    // An order's words, filled in from what the crew knows (the latest designation's number); not spoken twice within
    // 3 s (one order starts several drills) nor with a blank left in it.
    void SpeakOrder(const std::string& callout_id) {
        if (!callouts_) return;
        if (callout_id == last_order_said_ && time_ - last_order_s_ < 3.0) return;
        const CalloutDef* def = def_.crew_data.FindCallout(callout_id);
        std::map<std::string, std::string> fields;
        if (!designations_heard_.empty()) {
            char n[16];
            std::snprintf(n, sizeof(n), "%04d", designations_heard_.back().number);
            fields["number"] = n;
        }
        if (def == nullptr || FillCallout(def->ru, fields).find('{') != std::string::npos) return;
        const std::string from = console_.FindStation(def->from) != nullptr ? def->from : std::string("commander");
        callouts_->Say(time_, callout_id, from, fields);
        last_order_said_ = callout_id;
        last_order_s_ = time_;
    }
    std::string last_order_said_;
    double last_order_s_ = -1e9;

    // ---- The NPC vehicle commander (roadmap 8.6): the crew's orders by doctrine, from what the crew has reported.
    // Readiness first: the drill master's plan for "ready, halted" (or the search on the march while moving) says which
    // seat's drill is next; each idle seat gets its drill. Then the engagement: a contact the crew has (the search
    // scope or a designation) is taken (ready_capture, capture_with_ssc); a held, hostile target is fired at (prepare,
    // launch; the jammer drill when the radar has only the jammer's angles; the low-target drill on НЛЦ); after a
    // miss with the target still in the zone, again. The commander thinks at his own pace (every 1 s).
    double npc_next_think_s_ = 0.0;
    std::map<std::string, std::pair<std::string, double>> npc_ordered_;  // seat -> (drill, when)

    bool NpcOrdered(const std::string& seat, const std::string& drill, double within_s) const {
        const auto it = npc_ordered_.find(seat);
        return it != npc_ordered_.end() && it->second.first == drill && time_ - it->second.second < within_s;
    }
    bool NpcOrder(const std::string& seat, const std::string& drill, bool queue = false) {
        CrewMember* m = MemberAt(seat);
        const Procedure* p = console_.FindProcedure(drill);
        if (m == nullptr || p == nullptr) return false;
        GiveOrder(*m, *p, queue);
        npc_ordered_[seat] = {drill, time_};
        return true;
    }
    // A seat is free for a new order when an NPC there has nothing to do, or the player there has finished his.
    bool SeatFree(const std::string& seat) {
        if (const ProcedureTracker* t = SeatTracker(seat)) return t->GetProcedure().id.empty() || t->IsComplete();
        const CrewMember* m = MemberAt(seat);
        return m != nullptr && !m->IsBusy();
    }

    void UpdateNpcCommander() {
        if (!NpcCommanderActive() || !drill_ || time_ + 1e-9 < npc_next_think_s_) return;
        npc_next_think_s_ = time_ + 1.0;
        const bool halted = console_.equipment.GetInput("vehicle_stopped");
        // Readiness: the next drill of each idle seat toward the task.
        const CrewTask* task = def_.crew_data.FindTask(halted ? "ready_halted" : "searching");
        if (task != nullptr) {
            const DrillPlan plan = drill_->Plan(drill_->GoalFor(*task), console_.panel, console_.equipment);
            for (const DrillPlan::Seat& s : plan.seats) {
                if (!SeatFree(s.station)) continue;
                for (const std::string& drill : s.procedures) {
                    if (NpcOrdered(s.station, drill, 60.0)) continue;
                    NpcOrder(s.station, drill);
                    break;
                }
                if (!s.loose.empty() && SeatFree(s.station) && SeatHolder(s.station).empty()) {  // one switch, no drill: an order of its own
                    Procedure p;
                    p.id = "order: " + s.loose.front().control;
                    p.station = s.station;
                    ProcedureStep st;
                    st.control_id = s.loose.front().control;
                    st.value = s.loose.front().position;
                    st.description = s.loose.front().description;
                    st.wait_for = s.loose_waits.front();
                    p.steps.push_back(st);
                    if (!NpcOrdered(s.station, p.id, 30.0)) {
                        if (CrewMember* m = MemberAt(s.station)) {
                            GiveOrder(*m, p, false);
                            npc_ordered_[s.station] = {p.id, time_};
                        }
                    }
                }
            }
        }
        if (!engagement_on_ || !halted) return;
        // The engagement, by the crew data's orders (order_take_target, order_prepare, fire_permitted, order_jammer,
        // order_low_target): the seats and drills each order starts are data, not code.
        const EngagementModel& eng = engagement_;
        const TrackMethod m = eng.Method();
        const std::string tracked = eng.TrackedTarget();
        if (eng.ShotsInFlight()) return;  // a missile in the air: wait for the result
        if (m == TrackMethod::None) {
            npc_target_.clear();
            // Take the nearest contact once it is within the tracking radar's reach (the drill switches the tracking
            // transmitter on only when the azimuth line is on the target: voenizdat_osa_1991 p250); a designation
            // from the PU-12 counts when it is that close too.
            // He knows what his crew told him and what the radio said (his picture): the nearest hostile he has heard of
            // in the last 30 s, unless his judgement slips (his tier's priority error: he takes the next one).
            const double reach = console_.track.max_range_m > 0.0 ? console_.track.max_range_m : 1e9;
            std::vector<KnownTarget> known;
            for (const KnownTarget& k : pictures_["commander"].Recent(time_, 30.0)) {
                if (k.has_range && !k.friendly && !eng.KnownFriend(k.target)) known.push_back(k);
            }
            std::sort(known.begin(), known.end(), [](const KnownTarget& a, const KnownTarget& b) { return a.range_m < b.range_m; });
            if (known.empty()) return;
            std::size_t pick = 0;
            if (known.size() > 1 && npc_choice_for_ != known.front().target) {
                npc_choice_for_ = known.front().target;
                const SkillTier* t = CommanderTier();
                npc_choice_slip_ = t != nullptr && static_cast<double>(hear_rng_()) / 4294967296.0 < t->priority_error_chance;
            }
            if (npc_choice_slip_ && known.size() > 1) pick = 1;
            const KnownTarget& k = known[pick];
            if (k.range_m > reach || !console_.equipment.IsReady("hv_ready")) return;
            assigned_target_ = k.target;
            if (NpcGive("order_take_target", 60.0) && k.number == 0 && callouts_) {  // no number from the PU-12: by its azimuth
                callouts_->Say(time_, "order_take_contact", "commander", {{"azimuth", MilsText(k.azimuth_deg)}});
            }
            return;
        }
        if (eng.KnownFriend(tracked)) return;  // never at a friend: hold
        if (tracked != npc_target_) {
            npc_target_ = tracked;
            npc_prepared_ = false;
        }
        // A branch (the low-target drill, the jammer drill): his judgement may make him hesitate (his tier's branch
        // error: 10 s before he sees it).
        const bool branch = (m == TrackMethod::Radar && console_.equipment.GetInput("target_too_low")) || m == TrackMethod::RadarOnJam;
        if (branch && npc_branch_for_ != tracked) {
            npc_branch_for_ = tracked;
            const SkillTier* t = CommanderTier();
            const bool slow = t != nullptr && static_cast<double>(hear_rng_()) / 4294967296.0 < t->branch_error_chance;
            npc_branch_at_ = time_ + (slow ? 10.0 : 0.0);
        }
        if (branch && time_ < npc_branch_at_) return;
        if (m == TrackMethod::Radar && console_.equipment.GetInput("target_too_low")) {
            NpcGive("order_low_target", 30.0);
            return;
        }
        if (m == TrackMethod::RadarOnJam) {
            NpcGive("order_jammer", 40.0);
            return;
        }
        if (!npc_prepared_) {
            npc_prepared_ = NpcGive("order_prepare", 20.0);
            return;
        }
        NpcGive("fire_permitted", 8.0);
    }
    std::string npc_target_;
    bool npc_prepared_ = false;
    std::string npc_choice_for_;
    bool npc_choice_slip_ = false;
    std::string npc_branch_for_;
    double npc_branch_at_ = 0.0;
    const SkillTier* CommanderTier() const {
        for (std::size_t i = 0; i < crew_.size() && i < def_.crew.size(); ++i) {
            if (crew_[i].Role() == "commander") return SkillOf(def_.crew[i]);
        }
        return nullptr;
    }

    // Give an order of the crew data unless it was given within `again_s`, and only when every seat it goes to is free.
    bool NpcGive(const std::string& order_id, double again_s) {
        const OrderDef* o = def_.crew_data.FindOrder(order_id);
        if (o == nullptr) return false;
        if (NpcOrdered("order", order_id, again_s)) return false;
        for (const OrderStart& st : o->procedures) {
            if (!SeatFree(st.station)) return false;
        }
        GiveCrewOrder(order_id);
        npc_ordered_["order"] = {order_id, time_};
        return true;
    }
    std::vector<C2Network::Event> designations_heard_;  // every designation so far (the latest last)

    // A crew member's reports as words: acknowledgement, done, can't, fault; and the start of a wait (reminders).
    void CrewCallouts(const CrewMember& c, const std::vector<CrewReport>& reports) {
        if (!callouts_ || !SeatHolder(c.Role()).empty()) return;
        for (const CrewReport& r : reports) {
            switch (r.kind) {
                case CrewReport::Kind::Started: Say("ack", c.Role()); break;
                case CrewReport::Kind::Completed: Say("done", c.Role()); break;
                case CrewReport::Kind::Aborted: Say("cannot", c.Role(), {{"reason", r.detail}}); break;
                case CrewReport::Kind::EquipmentFault: {
                    const SubsystemDef* d = console_.equipment.Def(r.detail);
                    Say("fault", c.Role(), {{"what", d != nullptr && !d->name.empty() ? d->name : r.detail}});
                    break;
                }
                default: break;
            }
        }
    }

    void UpdateCallouts() {
        if (!callouts_) {
            if (engagement_on_) engagement_.TakeEvents();  // nobody to say them
            return;
        }
        // Automatic reports: a seat says it when its condition becomes true.
        for (std::size_t i = 0; i < def_.crew_data.reports.size(); ++i) {
            const AutoReportDef& r = def_.crew_data.reports[i];
            const bool now = console_.equipment.Holds(r.when, console_.panel);
            if (now && report_state_[i] == 0 && NpcAt(r.from)) Say(r.callout, r.from);
            report_state_[i] = now ? 1 : 0;
        }
        // What the instruments showed.
        if (engagement_on_) {
            for (const EngagementEvent& e : engagement_.TakeEvents()) {
                char range[16];
                std::snprintf(range, sizeof(range), "%.0f", e.range_m / 1000.0);
                switch (e.kind) {
                    case EngagementEvent::Kind::Contact:
                        if (NpcAt("senior_operator")) {
                            KnownTarget k;
                            k.target = e.target;
                            k.has_azimuth = k.has_range = true;
                            k.azimuth_deg = e.az_deg;
                            k.range_m = e.range_m;
                            Say("contact", "senior_operator", {{"azimuth", MilsText(e.az_deg)}, {"range_km", range}}, &k);
                            next_contact_update_s_ = time_ + 15.0;
                        }
                        break;
                    case EngagementEvent::Kind::TrackLost:
                        if (NpcAt("operator") && TargetRange(e.target) >= 0.0) Say("lock_lost", "operator");  // not a target just shot down
                        break;
                    case EngagementEvent::Kind::OnJammer: if (NpcAt("operator")) Say("on_jammer", "operator"); break;
                    case EngagementEvent::Kind::BurnThrough: if (NpcAt("operator")) Say("captured", "operator"); break;
                    case EngagementEvent::Kind::IffFriend:
                        if (NpcAt("senior_operator")) {
                            KnownTarget k;
                            k.target = e.target;
                            k.friendly = true;
                            Say("iff_friend", "senior_operator", {}, &k);
                        }
                        break;
                    case EngagementEvent::Kind::Hit: if (NpcAt("deputy_commander")) Say("hit", "deputy_commander"); break;
                    case EngagementEvent::Kind::Miss: if (NpcAt("deputy_commander")) Say("miss", "deputy_commander"); break;
                    case EngagementEvent::Kind::Track: break;  // the capture and TV-track reports are automatic (inputs)
                }
            }
        }
        // Until something is tracked the senior operator keeps the crew up to date: the nearest echo on his scope,
        // every 15 s (the first report of each contact is spoken when it appears).
        if (engagement_on_ && NpcAt("senior_operator") && engagement_.Method() == TrackMethod::None && time_ >= next_contact_update_s_) {
            const KnownTarget* near = nullptr;
            for (const KnownTarget& k : pictures_["senior_operator"].All()) {
                if (time_ - k.time_s > 2.0 || !k.has_range || k.friendly) continue;
                if (near == nullptr || k.range_m < near->range_m) near = &k;
            }
            if (near != nullptr) {
                KnownTarget k = *near;
                char r[16];
                std::snprintf(r, sizeof(r), "%.0f", k.range_m / 1000.0);
                Say("contact", "senior_operator", {{"azimuth", MilsText(k.azimuth_deg)}, {"range_km", r}}, &k);
                next_contact_update_s_ = time_ + 15.0;
            }
        }
        // Designations the commander heard on the radio, passed on to the crew.
        if (c2_) {
            for (const C2Network::Event& e : designations_) {
                if (NpcAt("commander")) {
                    char az[16], n[16], r[16], h[16];
                    std::snprintf(az, sizeof(az), "%02d-%02d", e.azimuth_mils / 100, e.azimuth_mils % 100);
                    std::snprintf(n, sizeof(n), "%04d", e.number);
                    std::snprintf(r, sizeof(r), "%.0f", e.range_m / 1000.0);
                    std::snprintf(h, sizeof(h), "%d", e.height_hm);
                    KnownTarget k;
                    k.target = e.subject;
                    k.number = e.number;
                    k.has_azimuth = k.has_range = k.true_azimuth = true;
                    k.azimuth_deg = e.azimuth_mils * 360.0 / 6000.0;
                    k.range_m = e.range_m;
                    Say("designation", "commander", {{"number", n}, {"azimuth", az}, {"range_km", r}, {"height_hm", h}}, &k);
                }
            }
        }
        designations_.clear();
        // Reminders: a crewmate waiting on another seat's work calls that seat after his patience, then the commander.
        for (std::size_t i = 0; i < crew_.size(); ++i) {
            CrewMember& c = crew_[i];
            WaitWatch& w = waits_[i];
            const ProcedureStep* step = c.IsBusy() ? c.Tracker().NextStep() : nullptr;
            if (c.Status() != CrewStatus::Waiting || step == nullptr || !SeatHolder(c.Role()).empty()) {
                w = WaitWatch{};
                continue;
            }
            if (w.since_s < 0.0) w.since_s = time_;
            const double patience = c.HasSkill() ? c.Skill().patience_s : 6.0;
            if (w.reminded >= 2 || time_ - w.since_s < patience * (w.reminded + 1)) continue;
            // The seat it waits for, and its next action: only a seat that can do it now (not one waiting itself for a
            // warm-up or for power) and isn't already at it is reminded.
            std::string seat, what, drill;
            for (const Condition& cond : step->wait_for) {
                if (console_.equipment.Holds(cond, console_.panel)) continue;
                const DrillPlan plan = drill_->Plan(std::vector<Condition>{cond}, console_.panel, console_.equipment);
                for (const DrillPlan::Seat& s : plan.seats) {
                    if (s.station == c.Role()) continue;
                    const CrewMember* other = MemberAt(s.station);
                    if (other != nullptr && SeatHolder(s.station).empty() && other->Status() == CrewStatus::Working) continue;
                    if (!s.loose.empty() && console_.equipment.AllHold(s.loose_waits.front(), console_.panel)) {
                        what = ShortLabel(s.loose.front().control, s.loose.front().position);
                    } else if (!s.procedures.empty()) {
                        const ProcedureStep* next = FirstUndoneStep(s.procedures.front());
                        if (next == nullptr || !console_.equipment.AllHold(next->wait_for, console_.panel)) continue;
                        what = ShortLabel(next->control_id, next->value);
                        drill = s.procedures.front();
                    } else {
                        continue;
                    }
                    seat = s.station;
                    break;
                }
                if (!seat.empty()) break;
            }
            if (seat.empty()) {  // waiting for a warm-up, the world, or a seat already at it: nobody to remind yet
                w.since_s = time_;
                continue;
            }
            ++w.reminded;
            ++reminders_to_[seat];
            if (w.reminded == 1) {
                Say("remind", c.Role(), {{"seat", SeatWord(seat, false)}, {"seat_en", SeatWord(seat, true)}, {"what", what}});
            } else {
                Say("remind_order", "commander", {{"seat", SeatWord(seat, false)}, {"seat_en", SeatWord(seat, true)}});
                // An NPC who was never ordered to do it is ordered now: the crew sorts it out among themselves.
                CrewMember* other = MemberAt(seat);
                const Procedure* p = drill.empty() ? nullptr : console_.FindProcedure(drill);
                if (other != nullptr && SeatHolder(seat).empty() && !other->IsBusy() && p != nullptr) GiveOrder(*other, *p, true);
            }
            log_.Record({time_, DebriefEntry::Kind::Scenario, c.Name(), "waits for " + SeatWord(seat, true), what});
        }
        SuperviseOrders();
        // What was said.
        callouts_->Step(time_);
        for (const SpokenCallout& s : callouts_->TakeHeard()) {
            const std::string who = s.from == "battery" ? std::string("Battery") : SpeakerName(s.from);
            Radio(s.channel, who, s.ru + " (" + s.en + ")");
            log_.Record({s.start_s, DebriefEntry::Kind::Scenario, who, "says", s.ru + " (" + s.en + ")"});
            Hear(s);
        }
    }

    // ---- Knowledge per seat (roadmap 8.5, crew_knowledge.hpp): each seat's picture, from its own instruments and
    // from what it heard. The NPCs act on these, never on the truth.
    std::map<std::string, CrewPicture> pictures_;
    std::string assigned_target_;  // the target the commander ordered taken ("" = none yet)
    std::mt19937 hear_rng_{97u};    // who misses a callout (the outcome draws stay where they were)
    int say_agains_ = 0;
    double next_contact_update_s_ = 0.0;

    // The seats' own instruments: the senior operator's scope, the operator's range indicator.
    void SeeInstruments() {
        if (!engagement_on_ || !def_.has_crew_data) return;
        for (const Contact& c : engagement_.SearchPicture()) {
            KnownTarget k;
            k.target = c.target_id;
            k.has_azimuth = k.has_range = true;
            k.azimuth_deg = c.azimuth_deg;
            k.range_m = c.range_m;
            k.time_s = time_;
            k.source = "scope";
            pictures_["senior_operator"].Learn(k);
        }
        for (const Contact& c : engagement_.TrackReturns()) {
            KnownTarget k;
            k.target = c.target_id;
            k.has_range = true;
            k.range_m = c.range_m;
            k.time_s = time_;
            k.source = "range indicator";
            pictures_["operator"].Learn(k);
        }
    }

    // A callout was heard: what it says about a target goes into each listener's picture. An NPC listener may not take
    // it in (his tier's miss chance, more under stress): he says "Повторите!" and the speaker says it again (once).
    void Hear(const SpokenCallout& s) {
        if (s.channel != "intercom") return;
        for (std::size_t i = 0; i < crew_.size(); ++i) {
            CrewMember& c = crew_[i];
            if (c.Role() == s.from) continue;
            const bool addressed = s.to == "crew" || s.to == "any" || s.to == c.Role();
            if (!addressed) continue;
            const SkillTier* t = i < def_.crew.size() ? SkillOf(def_.crew[i]) : nullptr;
            const bool npc = SeatHolder(c.Role()).empty();
            // Only a seat with a stake in it asks again: the one it was said to, the commander, the aiming seats.
            const bool stake = s.to == c.Role() || c.Role() == "commander" || c.Role() == def_.tuning.aim_az_role ||
                               c.Role() == def_.tuning.aim_el_role || c.Role() == def_.tuning.aim_range_role;
            if (npc && stake && t != nullptr && s.has_about && s.repeats == 0) {
                const double miss = t->miss_chance * (c.Stressed() ? t->stress_factor : 1.0);
                if (static_cast<double>(hear_rng_()) / 4294967296.0 < miss) {
                    ++say_agains_;
                    Say("say_again", c.Role());
                    callouts_->Repeat(s, time_, 1.0 + SpeakDelayS(s.from));
                    continue;
                }
            }
            if (!s.has_about) continue;
            KnownTarget k = s.about;
            k.time_s = s.start_s;
            k.source = "told by " + SpeakerName(s.from);
            pictures_[c.Role()].Learn(k);
        }
    }

    // Stress (docs/crew/05_SKILL.md): the crew's own radars jammed, or a hostile they know of inside 10 km and nothing
    // tracked. It multiplies every crewmate's delays and chances by his tier's stress factor.
    void UpdateStress() {
        bool jammed = false;
        if (em_link_) {
            const em::RadarState* sr = em_link_->SearchRadar();
            const em::RadarState* tr = em_link_->TrackRadar();
            jammed = (sr != nullptr && sr->on && !sr->strobe_tracks.empty()) || (tr != nullptr && tr->on && tr->on_jam);
        }
        bool pressed = false;
        if (engagement_on_ && engagement_.Method() == TrackMethod::None) {
            for (const KnownTarget& k : pictures_["commander"].Recent(time_, 30.0)) {
                pressed = pressed || (k.has_range && k.range_m < 10000.0 && !k.friendly);
            }
        }
        for (CrewMember& c : crew_) c.SetStressed(jammed || pressed);
    }

public:
    // What a seat knows (its picture), for the HUD of a player in that seat and for tests.
    const CrewPicture* PictureOf(const std::string& station) const {
        const auto it = pictures_.find(station);
        return it != pictures_.end() ? &it->second : nullptr;
    }
    // The target the commander has ordered taken ("" = none).
    const std::string& AssignedTarget() const { return assigned_target_; }
    // The player in command picks the target to take (a contact his crew reported, or a designation).
    void AssignTarget(const std::string& target) { assigned_target_ = target; }
    // "Повторите" so far (a crewmate missed a report).
    int SayAgains() const { return say_agains_; }

    // A player in a seat speaks a report of the crew data (the UI's report buttons): what it says comes from his own
    // instruments (a contact: the nearest echo on his scope). False if there is no such callout or no crew data.
    bool PlayerSay(const std::string& station, const std::string& callout_id) {
        if (!callouts_ || def_.crew_data.FindCallout(callout_id) == nullptr) return false;
        if (callout_id == "contact" && engagement_on_) {
            const Contact* near = nullptr;
            for (const Contact& c : engagement_.SearchPicture()) {
                if (near == nullptr || c.range_m < near->range_m) near = &c;
            }
            if (near == nullptr) return false;  // nothing on his scope to report
            KnownTarget k;
            k.target = near->target_id;
            k.has_azimuth = k.has_range = true;
            k.azimuth_deg = near->azimuth_deg;
            k.range_m = near->range_m;
            char r[16];
            std::snprintf(r, sizeof(r), "%.0f", near->range_m / 1000.0);
            callouts_->Say(time_, callout_id, station, {{"azimuth", MilsText(near->azimuth_deg)}, {"range_km", r}}, 0.0, &k);
            return true;
        }
        return callouts_->Say(time_, callout_id, station);
    }

private:

    // The NPC commander watches the orders he gave to players' seats: when the next step could be done (its lamps are
    // lit) and nothing has moved for his patience, he names it ("Оператор, ПЕРЕДАТЧИК ВКЛ.!"), then orders it again.
    struct OrderWatch {
        std::string procedure;
        std::size_t index = 0;
        double since_s = -1.0;
        int reminded = 0;
    };
    std::map<std::string, OrderWatch> order_watch_;
    void SuperviseOrders() {
        if (!NpcCommanderActive()) return;
        double patience = 8.0;
        for (std::size_t i = 0; i < crew_.size() && i < def_.crew.size(); ++i) {
            if (crew_[i].Role() != "commander") continue;
            if (const SkillTier* t = SkillOf(def_.crew[i])) patience = t->patience_s;
        }
        for (const Seat& st : seats_) {
            const ProcedureTracker& tr = st.tracker;
            const ProcedureStep* next = tr.GetProcedure().id.empty() ? nullptr : tr.NextStep();
            OrderWatch& w = order_watch_[st.role];
            if (next == nullptr) {
                w = OrderWatch{};
                continue;
            }
            if (w.procedure != tr.GetProcedure().id || w.index != tr.NextIndex()) {  // progress (or a new order)
                w = OrderWatch{tr.GetProcedure().id, tr.NextIndex(), -1.0, 0};
            }
            if (!console_.equipment.AllHold(next->wait_for, console_.panel)) {  // it can't be done yet: not his fault
                w.since_s = -1.0;
                continue;
            }
            if (w.since_s < 0.0) w.since_s = time_;
            if (w.reminded >= 2 || time_ - w.since_s < patience * (w.reminded + 1)) continue;
            ++w.reminded;
            ++reminders_to_[st.role];
            const std::string what = ShortLabel(next->control_id, next->value);
            if (w.reminded == 1) {
                Say("remind", "commander", {{"seat", SeatWord(st.role, false)}, {"seat_en", SeatWord(st.role, true)}, {"what", what}});
            } else {
                Say("remind_order", "commander", {{"seat", SeatWord(st.role, false)}, {"seat_en", SeatWord(st.role, true)}});
            }
            log_.Record({time_, DebriefEntry::Kind::Scenario, "Commander", "waits for " + SeatWord(st.role, true), what});
        }
    }

    std::string LabelOfControl(const std::string& id) const {
        const ControlDef* d = console_.panel.Find(id);
        return d != nullptr && !d->label.empty() ? d->label : id;
    }
    // The first step of a drill whose control isn't where the step puts it (what the seat still has to do).
    const ProcedureStep* FirstUndoneStep(const std::string& procedure) const {
        const Procedure* p = console_.FindProcedure(procedure);
        if (p == nullptr) return nullptr;
        for (const ProcedureStep& s : p->steps) {
            if (console_.panel.Get(s.control_id) != s.value) return &s;
        }
        return nullptr;
    }
    // A control as the crew names it in a reminder: its label up to " — " (the labels give both positions), then the
    // position's label if the label doesn't already say it ("АППАРАТУРА ВКЛ.", "ПЕРЕДАТЧИК ВКЛ.").
    std::string ShortLabel(const std::string& id, int position) const {
        const ControlDef* d = console_.panel.Find(id);
        if (d == nullptr) return id;
        std::string l = d->label.empty() ? id : d->label;
        const std::size_t dash = l.find(" — ");
        if (dash != std::string::npos) l = l.substr(0, dash);
        if (position >= 0 && static_cast<std::size_t>(position) < d->position_labels.size()) {
            const std::string& pos = d->position_labels[static_cast<std::size_t>(position)];
            if (l.find(pos) == std::string::npos) l += " " + pos;
        }
        return l;
    }
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
