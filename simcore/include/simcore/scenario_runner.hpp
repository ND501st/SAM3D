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
#include <cstdlib>
#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <random>
#include <set>
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
#include "simcore/battery_fire.hpp"
#include "simcore/c2_sources.hpp"
#include "simcore/command_post.hpp"
#include "simcore/post_feed.hpp"
#include "simcore/console_sync.hpp"
#include "simcore/crew_body.hpp"
#include "simcore/crew_callouts.hpp"
#include "simcore/crew_decisions.hpp"
#include "simcore/drill_master.hpp"
#include "simcore/em/replication.hpp"
#include "simcore/pu12.hpp"
#include "simcore/scenario.hpp"
#include "simcore/terrain.hpp"
#include "simcore/world.hpp"

namespace simcore {

class ScenarioRunner {
public:
    // The ДХ keypad: the two digits typed (the target's ASPD-U number), ЦУ pressed and waiting for the launcher's place
    // on the IT boards (1-...), the last designation it gave ("05>22"). The target it is on: typed, or the marker's.
    struct PostKeypad {
        std::string entry;
        bool designating = false;
        int held = -1;
        std::string last;
    };
    ScenarioRunner(ScenarioDef def, ConsoleData console)
        : def_(std::move(def)),
          console_(std::move(console)),
          ground_(def_.ground_height_m, def_.ridges),
          world_(def_.origin, &ground_),
          acmi_(world_.Frame(), "2026-01-01T00:00:00Z", def_.name),
          rng_(def_.seed),
          engagement_on_(!def_.tuning_file.empty()),
          engagement_(def_.tuning, def_.unit_position) {
        assist_level_ = def_.assistance;
        if (def_.map) {  // a generated map's ground under the whole scenario (terrain_map.hpp)
            map_ground_ = std::make_unique<MapTerrain>(def_.map, def_.origin);
            world_.SetTerrain(map_ground_.get());
        }
        if (def_.has_weather) {  // the winds and the cloud over the whole airspace (weather.hpp)
            weather_ = Weather(def_.weather, def_.ground_height_m, noise::Mix64(def_.seed + 0x3EA7u));
            if (def_.map) weather_.SetRoughnessM(def_.map->Def().roughness_m);
            world_.SetWeather(&weather_);
        }
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
            if (def_.has_crew_data) {  // controls out of seated reach: he gets up for them (crew_body.hpp)
                const CrewSeatDef* seat = def_.crew_data.FindSeat(c.role);
                if (seat != nullptr && !seat->stand_for.empty()) {
                    crew_.back().SetStanding(seat->stand_for, def_.crew_data.stand_s, def_.crew_data.sit_s);
                }
            }
        }
        for (const ControlDef& c : console_.panel.Controls()) {
            if (c.kind == ControlKind::Continuous) wheels_.push_back({c.id, c.station, console_.panel.GetValue(c.id), -1e9});
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
            e.size_m = t.size_m;
            eng_targets_.push_back(e);
        }
        engagement_.SetOpticalConditions(def_.optics);
        rwr_warned_.assign(def_.targets.size(), 0);
        fired_.assign(def_.triggers.size(), false);
        engagement_.SetUnitHeading(def_.unit_heading_deg);
        if (ready_state != nullptr) ApplyReadiness(*ready_state);
        if (console_.has_launcher) {
            launcher_ = std::make_unique<Launcher>(console_.launcher);
            for (const std::string& c : def_.empty_containers) launcher_->Unload(c);
        }
        if (console_.has_comms) comms_ = std::make_unique<CrewComms>(console_.comms);
        if (console_.has_fuel) fuel_ = std::make_unique<FuelModel>(console_.fuel, def_.fuel_fraction);
        if (def_.has_post) post_ = std::make_unique<CommandPost>(def_.post_aspd, def_.post_board, def_.unit_position, def_.seed ^ 0x51ed27u);
        if (!def_.c2_sources.empty()) {
            c2_ = std::make_unique<C2Network>(def_.c2_policy, def_.seed);
            for (const C2SourceDef& s : def_.c2_sources) c2_->AddSource(s, def_.unit_position);
            if (LinkedToPost()) c2_->SetExternal(def_.c2_policy.primary);  // the real PU-12M reads its designations out
            engagement_.SetAirPicture(c2_.get());
        }
        if (def_.has_em && engagement_on_) {
            em_link_ = std::make_unique<EmSensorLink>(def_.origin, &world_.GetTerrain(), def_.seed, def_.em, def_.unit_position);
            if (em_link_->Ok()) {
                engagement_.SetEmLink(em_link_.get());
                for (const ScenarioTarget& t : def_.targets) {
                    if (!t.rwr.empty()) em_link_->SetTargetRwr(t.id, t.rwr);  // made with the target's platform
                }
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
            if (def_.prepared.present && def_.prepared.emplacement) {
                battery_->SetInEmplacement(def_.prepared.values.leave_emplacement_s);
            } else if (def_.battery_deployed) {
                battery_->SetInEmplacement(0.0);  // set up where it stands, nothing dug in to climb out of
            }
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
        weather_.SetTime(time_);  // the clouds drift
        UpdateTargets(dt);
        DeliverOrders();
        UpdateBattery(dt);
        UpdateC2();
        UpdatePost();
        if (engagement_on_) {
            aim_crew_.assigned = assigned_target_;
            aim_crew_.ignore = decoys_known_;
            engagement_.Step(time_, dt, world_, console_, eng_targets_, log_, rng_, aim_crew_);
            // A radar lock is heard by the target's warning receiver; a TV track is not. With the EM layer, a target with
            // a warning receiver of its own goes by what it hears (CueFromWarnings).
            const TrackMethod m = engagement_.Method();
            if ((m == TrackMethod::Radar || m == TrackMethod::RadarOnJam) && !HasRwr(engagement_.TrackedTarget())) {
                CueTarget(engagement_.TrackedTarget(), false);
            }
        }
        UpdateLauncher();
        UpdateGuidanceEm();
        CueFromWarnings();
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
        for (WheelWatch& w : wheels_) {  // who is turning a hand wheel (the bodies, roadmap 8.10)
            const double v = console_.panel.GetValue(w.id);
            if (std::fabs(v - w.value) > 1e-9) w.moved_s = time_;
            w.value = v;
        }
        log_.CollectEquipment(console_.equipment);
        for (CrewMember& c : crew_) {
            std::vector<CrewReport> reports = c.TakeReports();
            CrewCallouts(c, reports);
            log_.CollectCrewReports(c.Name(), std::move(reports), time_);
        }
        SeeInstruments();
        UpdateGrades();
        UpdateCallouts();
        UpdateStress();
        UpdateNpcCommander();
        UpdateOwnReports();
        if (drop_at_s_ >= 0.0 && time_ >= drop_at_s_ && engagement_on_) {
            drop_at_s_ = -1.0;
            if (engagement_.DropTrack(time_, log_, "Цель сбросить")) assigned_target_.clear();
        }
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
    // The crew members, in the scenario's order (their slips, omissions, skill: the balance tool, the debrief).
    const std::vector<CrewMember>& CrewMembers() const { return crew_; }
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

    // Replace the scenario's ground, e.g. with heights sampled from Cesium. Null restores the scenario's own (its map's,
    // or the flat ground). The terrain must outlive the runner or be replaced first.
    void SetTerrain(const Terrain* terrain) {
        world_.SetTerrain(terrain != nullptr ? terrain : (map_ground_ ? static_cast<const Terrain*>(map_ground_.get()) : &ground_));
        if (em_link_) em_link_->SetTerrain(&world_.GetTerrain());
    }

    // The scenario's weather (its winds and clouds, drifted to now) and its map's ground (null on flat ground).
    const Weather& GetWeather() const { return weather_; }
    const MapTerrain* MapGround() const { return map_ground_.get(); }

    // The battery command post's plotting board: the player's own when the vehicle is the post, else the scenario's
    // "command_post" (null when there is none).
    const PlottingBoard* Board() const {
        if (post_) return &post_->Board();
        return c2_ ? c2_->BoardOf("pu12") : nullptr;
    }
    // The command post's information chain when the player's vehicle is the post (null otherwise): the IT-45's tracks,
    // the navigation error (command_post.hpp).
    const CommandPost* Post() const { return post_.get(); }
    // The IT-45 as it shows now: the tracks on the scale the console's scale switch is at (its position picks the
    // ASPD-U's scale), while the tube has a picture. Empty otherwise.
    std::vector<CommandPost::ScopeMark> PostScope() const {
        if (!post_) return {};
        const bool picture = console_.equipment.IsReady(console_.post_telecode) || console_.equipment.IsReady(console_.post_primary);
        if (!picture) return {};
        return post_->Scope(PostScaleM());
    }
    double PostScaleM() const {
        if (!post_ || post_->Def().scales_m.empty()) return 0.0;
        const std::vector<double>& s = post_->Def().scales_m;
        const int pos = console_.post_scale.empty() ? 0 : console_.panel.Get(console_.post_scale);
        return s[static_cast<std::size_t>(std::min<int>(std::max(pos, 0), static_cast<int>(s.size()) - 1))];
    }
    // The player at the plotter's seat writes one target from the IT-45 on the board (its ASPD-U number). False if the
    // vehicle is no post, the seat isn't the player's, the number isn't on the tube, or he is still writing the last one.
    bool PlayerPlot(int aspd_number) {
        if (!post_ || SeatHolder(console_.post_plotter).empty()) return false;
        return post_->PlotTrack(time_, aspd_number, PlotterSpeed());
    }

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
        if (post_) {
            ToViewExtras(PostFeedNow(), x);  // the post's own board (killed targets wiped off) and instruments
        } else if (const PlottingBoard* b = Board()) {
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

    // The vehicle is oriented: its gyro half-compass agrees with the true heading (or the console models no
    // orientation). Designations' azimuths agree with the crew's only then.
    bool Oriented() const {
        if (!console_.has_orientation || !engagement_on_) return true;
        double err = std::fmod(engagement_.GyroHeadingDeg() - engagement_.UnitHeadingDeg(), 360.0);
        if (err > 180.0) err -= 360.0;
        if (err < -180.0) err += 360.0;
        return std::fabs(err) < 1.0;
    }

    // What each seat's crewmate is doing, for the bodies (crew_body.hpp, roadmap 8.10): his next action and when, his
    // last one, whether he is up at a panel, the hand wheels he is turning, whether he is speaking. One per crew
    // member, in the scenario's order.
    std::vector<SeatActivity> CrewActivity() const {
        std::vector<SeatActivity> out;
        for (const CrewMember& m : crew_) {
            SeatActivity a;
            a.station = m.Role();
            a.npc = SeatHolder(m.Role()).empty();
            const CrewMember::Intent in = m.CurrentIntent();
            if (a.npc) {
                a.control = in.control;
                a.act_in_s = in.in_s;
                a.waiting = in.waiting;
                a.standing = in.standing;
                a.stand_control = in.stand_control;
            }
            a.last_control = in.last_control;
            a.since_last_s = in.since_last_s;
            for (const WheelWatch& w : wheels_) {
                if (w.station == m.Role() && time_ - w.moved_s < 0.5) a.wheels.push_back(w.id);
            }
            a.speaking = callouts_ != nullptr && callouts_->SpeakingFrom(m.Role());
            a.stressed = m.Stressed();
            out.push_back(std::move(a));
        }
        return out;
    }

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
        if (order_id == "order_drop_target") drop_at_s_ = time_ + SpeakDelayS("operator") + 1.5;  // the wheels off the target
        quiet_orders_ = true;  // the order's own words were spoken: its drills don't each speak an order
        for (const OrderStart& s : o->procedures) {
            CrewMember* m = MemberAt(s.station);
            const Procedure* p = console_.FindProcedure(s.procedure);
            if (m != nullptr && p != nullptr) GiveOrder(*m, *p, s.queue);
        }
        quiet_orders_ = false;
        return true;
    }
    // The seat that commands the crew (the crew data's commander_seat: the Osa's commander, the PU-12M's squad leader).
    const std::string& Cmdr() const { return def_.crew_data.commander; }
    bool NpcCommanderActive() const { return def_.npc_commander && SeatHolder(Cmdr()).empty(); }

    // Whether an order can be carried out now, and if not why: the order menu's honest greying (roadmap 8.8,
    // docs/crew/07_COMMANDER_MODE.md §1.3). Greyed: an order whose task already holds; one whose drills wait on equipment
    // that has failed; one whose first step waits for a world fact no seat can make true and that isn't so (no target in
    // the zone, the target not flying low: the crew data's facts say which, and what to say); one that needs what the
    // commander doesn't know or have (a reported contact, a track, a jammer, a missile: the order's "needs"). Waits the
    // crew works through by itself (power, warm-up, the driver halting) don't grey an order.
    struct OrderCheck {
        bool can = true;
        std::string reason_ru;
        std::string reason_en;
    };
    OrderCheck CheckOrder(const std::string& order_id) const {
        OrderCheck out;
        auto refuse = [&out](const std::string& ru, const std::string& en) {
            out.can = false;
            out.reason_ru = ru;
            out.reason_en = en;
            return out;
        };
        const OrderDef* o = def_.has_crew_data ? def_.crew_data.FindOrder(order_id) : nullptr;
        if (o == nullptr) return refuse("нет такого приказа", "no such order");
        if (!o->task.empty()) {
            const CrewTask* t = def_.crew_data.FindTask(o->task);
            if (t != nullptr && !t->conditions.empty() && console_.equipment.AllHold(t->conditions, console_.panel)) {
                return refuse("уже выполнено", "done already");
            }
        }
        for (const std::string& need : o->needs) {
            if (need == "contact") {
                bool known = false;
                const auto it = pictures_.find(Cmdr());
                if (it != pictures_.end()) {
                    for (const KnownTarget& k : it->second.Recent(time_, 30.0)) known = known || (k.has_range && !k.friendly);
                }
                if (!known) return refuse("цели нет", "no target reported");
            } else if (need == "tracked") {
                if (!engagement_on_ || engagement_.Method() == TrackMethod::None) return refuse("сопровождения нет", "nothing tracked");
            } else if (need == "jamming") {
                if (!engagement_on_ || engagement_.Method() != TrackMethod::RadarOnJam) return refuse("помехи нет", "no jammer tracked");
            } else if (need == "missiles") {
                if (launcher_ && launcher_->LoadedCount() == 0) return refuse("ракет нет", "no missiles left");
            }
        }
        for (const OrderStart& s : o->procedures) {
            const Procedure* p = console_.FindProcedure(s.procedure);
            if (p == nullptr || p->steps.empty()) continue;
            bool someone = !SeatHolder(s.station).empty();
            for (const CrewMember& m : crew_) someone = someone || m.Role() == s.station;
            if (!someone) return refuse("нет номера расчёта", "nobody at the " + s.station + "'s seat");
            for (const ProcedureStep& st : p->steps) {
                for (const Condition& c : st.wait_for) {
                    if (c.kind == Condition::Kind::SubsystemIs && c.state != SubsystemState::Fault &&
                        console_.equipment.State(c.id) == SubsystemState::Fault) {
                        return refuse("отказ: " + c.id, "fault: " + c.id);
                    }
                }
            }
            for (const Condition& c : p->steps.front().wait_for) {
                if (c.kind != Condition::Kind::InputIs || console_.equipment.Holds(c, console_.panel)) continue;
                const CrewFact* f = def_.crew_data.FindFact(c.id);
                if (f != nullptr && f->by_action && c.input_value) continue;  // a seat makes it true (the driver halts)
                if (f != nullptr && !f->unmet_ru.empty()) return refuse(f->unmet_ru, f->unmet_en.empty() ? f->unmet_ru : f->unmet_en);
                return refuse("нет: " + c.id, "waiting for " + c.id);
            }
        }
        return out;
    }

    // ---- Training (roadmap 8.7, docs/crew/08_TRAINING_AND_ASSISTANCE.md): the overlay's level, what it shows a seat,
    // and the verdict on each move a player makes at a held seat.
    void SetAssistLevel(AssistLevel l) { assist_level_ = l; }
    AssistLevel GetAssistLevel() const { return assist_level_; }

    // The task the overlay aims at now: К бою until the vehicle is ready, then the capture, then the launch.
    std::string TrainingTask() const {
        if (!def_.has_crew_data) return std::string();
        const CrewTask* ready = def_.crew_data.FindTask("ready_halted");
        if (ready != nullptr && !console_.equipment.AllHold(ready->conditions, console_.panel)) return "ready_halted";
        if (!engagement_on_ || engagement_.Method() == TrackMethod::None) return "captured";
        return "launched";
    }

    // What the overlay shows a seat at the level set: the drill master's best next move (guided; hint once the player
    // has hesitated longer than an NPC's patience at that seat), or the move the seat's NPC is about to make
    // (demonstrate), or nothing (unassisted, exam).
    AssistView Assistance(const std::string& station) const {
        AssistView v;
        if (!drill_ || !def_.has_crew_data) return v;
        v.task = TrainingTask();
        const CrewTask* t = def_.crew_data.FindTask(v.task);
        if (t == nullptr) return v;
        const DrillGoal goal = drill_->GoalFor(*t);
        switch (assist_level_) {
            case AssistLevel::Unassisted:
            case AssistLevel::Exam:
                return v;
            case AssistLevel::Demonstrate:
                for (const CrewMember& m : crew_) {
                    if (m.Role() != station) continue;
                    const CrewMember::Intent in = m.CurrentIntent();
                    if (in.control.empty()) return v;
                    v.show = true;
                    v.demonstrating = true;
                    v.control = in.control;
                    v.position = in.value;
                    v.due_in_s = in.in_s > 0.0 ? in.in_s : 0.0;
                    if (m.IsBusy() && m.Tracker().NextStep() != nullptr) v.description = m.Tracker().NextStep()->description;
                    const DrillPlan plan = drill_->Plan(goal, console_.panel, console_.equipment);
                    const auto w = plan.why.find(in.control + "=" + std::to_string(in.value));
                    if (w != plan.why.end()) {
                        v.why = w->second;
                    } else if (m.IsBusy()) {
                        v.why = {"a step of the drill '" + m.Tracker().GetProcedure().id + "'"};
                    }
                    return v;
                }
                return v;
            case AssistLevel::Hint:
                if (Hesitation(station) < PatienceAt(station)) return v;
                break;
            case AssistLevel::Guided:
                break;
        }
        const DrillAdvice a = drill_->NextFor(station, goal, console_);
        if (a.control.empty()) return v;
        v.show = true;
        v.control = a.control;
        v.position = a.position;
        v.description = a.description;
        v.why = a.why;
        v.due_in_s = a.due_in_s;
        return v;
    }

    // Every move players made at held seats, judged against the drill master's best (the overlay's "that cost" line
    // and the debrief's).
    const std::vector<MoveVerdict>& MoveVerdicts() const { return verdicts_; }
    const MoveVerdict* LastVerdict(const std::string& station) const {
        for (auto it = verdicts_.rbegin(); it != verdicts_.rend(); ++it) {
            if (it->station == station) return &*it;
        }
        return nullptr;
    }

    // The player's order from the menu: carried out if CheckOrder allows it; otherwise the seat it is for answers
    // "Не могу: <reason>" and nothing starts. False if refused.
    bool TryCrewOrder(const std::string& order_id, OrderCheck* why = nullptr) {
        const OrderCheck c = CheckOrder(order_id);
        if (why != nullptr) *why = c;
        if (!c.can) {
            const OrderDef* o = def_.has_crew_data ? def_.crew_data.FindOrder(order_id) : nullptr;
            const std::string seat = o != nullptr && !o->procedures.empty() ? o->procedures.front().station : std::string("deputy_commander");
            if (o != nullptr) SpeakOrder(order_id);
            if (callouts_ && NpcAt(seat)) {
                Say("cannot", seat, {{"reason", c.reason_ru}, {"reason_en", c.reason_en}});
            }
            return false;
        }
        return GiveCrewOrder(order_id);
    }

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
        st.taken_s = time_;
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
        JudgeMove(role, control_id, position);
        st->last_action_s = time_;
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
        Seat* st = FindSeat(role);
        if (st == nullptr || !AtStation(role, wheel_id, why)) return false;
        st->last_action_s = time_;
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
        return std::string(head) + log_.Format() + CrewGrades();
    }

    // The crew's grades (docs/crew/08_TRAINING_AND_ASSISTANCE.md §3): "К бою" against the drill master's ideal time from
    // the session's start, detection to launch against the published reaction time, and each seat's reminders, slips
    // and forgotten steps. Empty without crew data.
    std::string CrewGrades() const {
        if (!def_.has_crew_data) return std::string();
        std::string out = "\nCrew:\n";
        char line[256];
        if (ideal_ready_s_ > 0.0) {
            // The readiness task by its name ("Ready for action at a halt (к бою готов)", "Post deployed ...").
            const CrewTask* task = def_.crew_data.FindTask("ready_halted");
            const std::string what = task != nullptr && !task->name.empty() ? task->name : std::string("Ready, halted");
            if (actual_ready_s_ > 0.0) {
                const double pct = 100.0 * actual_ready_s_ / ideal_ready_s_;
                std::snprintf(line, sizeof(line), "  %s: %.0f s, ideal %.0f s (%.0f %%): %s\n", what.c_str(), actual_ready_s_,
                              ideal_ready_s_, pct, GradeWord(pct));
            } else {
                std::snprintf(line, sizeof(line), "  %s: not reached; ideal %.0f s\n", what.c_str(), ideal_ready_s_);
            }
            out += line;
        }
        if (first_seen_s_ >= 0.0 && first_launch_s_ >= 0.0) {
            // The published figure counts from a detection in reach: so from the later of the first echo and the moment
            // the crew knew the target was within the tracking radar's reach.
            const bool popped_up = in_reach_s_ >= 0.0 && in_reach_s_ - first_seen_s_ < 1.0;
            const double dt = first_launch_s_ - first_seen_s_;
            if (popped_up) {  // first seen already in reach: the published reaction time applies
                std::snprintf(line, sizeof(line), "  Detection to launch: %.0f s (published 26-40 s: nvo_2015, korovin_tiv_2010)%s\n", dt,
                              dt <= 40.0 ? ": within the norm" : ": slower than the norm");
            } else {
                std::snprintf(line, sizeof(line), "  First echo to launch: %.0f s; in reach to launch %.0f s (seen beyond reach: no norm)\n", dt,
                              in_reach_s_ >= 0.0 ? first_launch_s_ - in_reach_s_ : -1.0);
            }
            out += line;
        }
        if (!decisions_.empty()) out += "  Decisions (the drill master's evaluation, on the truth):\n";
        for (const DecisionRecord& d : decisions_) {
            if (d.kind == "target") {
                if (d.best.empty() || d.best == d.target) {
                    std::snprintf(line, sizeof(line), "    %.0f s: took %s: the best choice (score %.2f)\n", d.time_s, d.target.c_str(), d.score);
                } else {
                    std::snprintf(line, sizeof(line), "    %.0f s: took %s (score %.2f); the evaluation's first was %s (%.2f)%s\n", d.time_s,
                                  d.target.c_str(), d.score, d.best.c_str(), d.best_score, d.good ? ": as good" : "");
                }
            } else if (d.ideal_s >= 0.0) {
                std::snprintf(line, sizeof(line), "    %.0f s: launch at %s, the ideal moment %.0f s (network chart from the detection, the zone's entry): %.0f s after it%s\n",
                              d.time_s, d.target.c_str(), d.ideal_s, d.late_s, d.good ? ", within the norm" : ", slower than the norm");
            } else {
                std::snprintf(line, sizeof(line), "    %.0f s: launch at %s\n", d.time_s, d.target.c_str());
            }
            out += line;
        }
        for (std::size_t i = 0; i < crew_.size(); ++i) {
            const CrewMember& c = crew_[i];
            const auto it = reminders_to_.find(c.Role());
            const int reminders = it != reminders_to_.end() ? it->second : 0;
            const std::string tier = c.HasSkill() ? c.Skill().tier : std::string("-");
            const std::string who = SeatHolder(c.Role()).empty() ? c.Name() : SeatHolder(c.Role()) + " (player)";
            std::snprintf(line, sizeof(line), "  %-18s %-16s tier %-8s reminders %d, slips %d, forgotten steps %d\n", c.Role().c_str(),
                          who.c_str(), tier.c_str(), reminders, c.Slips(), c.Omissions());
            out += line;
        }
        return out;
    }
    static const char* GradeWord(double pct) {
        if (pct <= 110.0) return "excellent (отлично)";
        if (pct <= 130.0) return "good (хорошо)";
        if (pct <= 160.0) return "satisfactory (удовлетворительно)";
        return "unsatisfactory (неудовлетворительно)";
    }
    double IdealReadyS() const { return ideal_ready_s_; }
    double ActualReadyS() const { return actual_ready_s_; }

    // The commander's decisions judged afterwards (docs/crew/06_REFERENCE_PERFORMER.md §3.6): each target he ordered
    // taken against the drill master's evaluation of the targets his crew had reported, and each launch against the
    // ideal moment (the network chart from the detection, laid on when the target entered the zone). Judged on the
    // truth: the debrief may compare with it.
    struct DecisionRecord {
        double time_s = 0.0;
        std::string kind;          // "target" or "launch"
        std::string target;        // chosen / launched at
        std::string best;          // "target": the evaluation's first choice
        double score = 0.0;        // "target": the chosen one's score, and the best's
        double best_score = 0.0;
        double ideal_s = -1.0;     // "launch": the ideal moment
        double late_s = 0.0;       // "launch": after the ideal moment
        bool good = true;
    };
    const std::vector<DecisionRecord>& Decisions() const { return decisions_; }

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

    // The guidance channels in the EM layer (roadmap E5): each channel's command transmitter radiates from ВНИМАНИЕ
    // (into its equivalent load until a missile flies, in a peacetime check), on its letter's frequency, along the
    // tracking antenna's line of sight; each missile in flight carries its responder.
    void UpdateGuidanceEm() {
        if (!em_link_ || !launcher_ || !engagement_on_) return;
        const double az = DegToRad(em_link_->AntennaAzimuthDeg());
        const double el = DegToRad(em_link_->AntennaElevationDeg());
        const Vec3 toward = engagement_.UnitPosition() + Vec3{std::sin(az) * std::cos(el), std::cos(az) * std::cos(el), std::sin(el)} * 10000.0;
        for (int ch = 1; ch <= 2; ++ch) {
            const int ts = launcher_->TransmitterState(ch);
            const int state = ts == 2 ? 2 : (ts == 1 ? (def_.em.command_link_in_load ? 1 : 2) : 0);
            em_link_->SetCommandLink(ch, state, launcher_->LetterChanges(), toward);
        }
        std::vector<EmSensorLink::GuidedMissile> missiles;
        for (const EngagementModel::MissileView& m : engagement_.MissilesInFlight(time_, world_, eng_targets_)) {
            missiles.push_back({m.number, m.position, m.velocity});
        }
        em_link_->SyncMissiles(missiles);
    }

    // The target carries a radar warning receiver in the EM layer.
    bool HasRwr(const std::string& target_id) const { return em_link_ && em_link_->HasRwr(target_id); }

    // Targets with a warning receiver: the pilot reacts to what it warns of (a lock, the command link of a launch), and
    // the debrief notes the first warning of each kind.
    void CueFromWarnings() {
        if (!em_link_) return;
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            const ScenarioTarget& t = def_.targets[i];
            if (t.rwr.empty() || !targets_[i].spawned || targets_[i].gone) continue;
            const EmSensorLink::RwrWarning w = em_link_->WarningOf(t.id);
            if (w.lock && (rwr_warned_[i] & 1) == 0) {
                rwr_warned_[i] |= 1;
                Event(t.id, "warning receiver: locked by " + w.lock_by);
            }
            if (w.launch && (rwr_warned_[i] & 2) == 0) {
                rwr_warned_[i] |= 2;
                Event(t.id, "warning receiver: missile guidance, " + w.launch_by);
            }
            if (w.launch) CueTarget(t.id, true);
            if (w.lock) CueTarget(t.id, false);
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
        if ((c2_ || post_) && !air_threat_forced_) {
            bool close = false;
            for (const BoardMark& m : post_ ? PostMarks() : c2_->Known(time_)) {
                close = close || (m.position - def_.unit_position).HorizontalLength() <= def_.battery_tactics.threat_range_m;
            }
            battery_->SetAirThreat(close, time_);
        }
        if (!def_.battery_drive_own) battery_->SetOwnPosition(def_.unit_position);
        ServiceDrills();
        battery_->Step(time_, dt);
        UpdateBatteryFire();
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

    // ---- The player's vehicle is the command post (docs/PU12_DEVELOPMENT.md P3, command_post.hpp).
    std::unique_ptr<CommandPost> post_;
    std::map<std::string, double> post_first_mark_;  // first mark of each target on the post's board (the log)

    // The regiment's telecode is within reach (the R-407 net, its outages); the console shows it once its ASPD-U works;
    // the post is oriented when its subsystem says so; the plotter works when he is free (an NPC not at a drill).
    void UpdatePost() {
        if (!post_) return;
        post_->MoveTo(def_.unit_position);
        const bool reach = post_->TelecodeInReach(def_.post_regiment, time_);
        if (reach != console_.equipment.GetInput(console_.post_telecode_input)) console_.equipment.SetInput(console_.post_telecode_input, reach);
        const bool oriented = console_.equipment.IsReady(console_.post_oriented);
        if (oriented != post_->IsOriented()) {
            if (oriented) {
                post_->Oriented(OrientQuality());
            } else {
                post_->Unoriented();
            }
            char detail[128];
            std::snprintf(detail, sizeof(detail), "%s: azimuth error %.1f deg, position %.0f m", oriented ? "oriented" : "not oriented",
                          RadToDeg(post_->AzErrorRad()), post_->OffsetM().HorizontalLength());
            Event("navigation", detail);
        }
        const std::vector<BoardContact> truth = TruthContacts();
        std::vector<BoardContact> radar;
        if (def_.has_post_radar) {
            for (const BoardContact& c : truth) {
                if (CommandPost::Covers(def_.post_radar, c)) radar.push_back(c);
            }
        }
        CommandPost::Inputs in;
        in.telecode = console_.equipment.IsReady(console_.post_telecode);
        in.primary = console_.equipment.IsReady(console_.post_primary);
        const CrewMember* plotter = MemberAt(console_.post_plotter);
        in.operator_free = plotter != nullptr && SeatHolder(console_.post_plotter).empty() && !plotter->IsBusy();
        in.operator_speed = PlotterSpeed();
        post_->Update(time_, CommandPost::RegimentSees(def_.post_regiment, truth), radar, in);
        for (const BoardMark& m : post_->Board().Marks()) {
            if (post_first_mark_.count(m.target_id) != 0) continue;
            post_first_mark_[m.target_id] = m.time_s;
            Event(m.target_id, "on the post's board: " + post_->Board().Describe(m));
        }
        UpdateKeypad();
    }
    // The DT-2 marker and the ДХ keypad (rwd_mb3: the Pult DCh enters a target's number, height zone, characteristics and
    // the designation signal with an address; the DT-2 gives the coordinates; the IT-45 brightens the marked target and
    // shows its time of arrival). Read before the step's buttons are released, so each press counts once.
    static constexpr double kMarkerCaptureFraction = 0.05;  // sam3d: the marker takes a target within 5 % of the scale
    void UpdateKeypad() {
        const ConsoleData& c = console_;
        marked_ = -1;
        marked_arrival_s_ = -1.0;
        if (!c.post_marker_azimuth.empty()) {
            const double az = DegToRad(c.panel.GetValue(c.post_marker_azimuth));
            const double r = c.panel.GetValue(c.post_marker_range);
            marker_x_ = r * std::sin(az);
            marker_y_ = r * std::cos(az);
            double best = kMarkerCaptureFraction;
            for (const CommandPost::ScopeMark& m : PostScope()) {
                const double d = std::hypot(m.x - marker_x_, m.y - marker_y_);
                if (d < best) {
                    best = d;
                    marked_ = m.number;
                }
            }
            if (marked_ >= 0) marked_arrival_s_ = ArrivalS(marked_);
        }
        if (c.post_keypad_digits.empty()) return;
        PostKeypad& k = keypad_;
        if (c.panel.Get(c.post_keypad_reset) == 1) {
            k.entry.clear();
            k.designating = false;
        }
        if (c.panel.Get(c.post_keypad_designate) == 1) {
            const int target = KeypadTarget();
            k.designating = target >= 0;
            if (k.designating) k.held = target;
        }
        for (std::size_t d = 0; d < c.post_keypad_digits.size(); ++d) {
            if (c.panel.Get(c.post_keypad_digits[d]) != 1) continue;
            if (k.designating) {
                // the address: the launcher's place on the IT boards (1 the first)
                k.designating = false;
                if (d >= 1 && d <= status_boards_.size()) {
                    const std::string& callsign = status_boards_[d - 1].callsign;
                    char text[32];
                    std::snprintf(text, sizeof(text), "%02d>%s", k.held, callsign.c_str());
                    k.last = DesignateFromTube(callsign, k.held) ? std::string(text) : std::string("--");
                }
            } else {
                k.entry = (k.entry + static_cast<char>('0' + static_cast<int>(d)));
                if (k.entry.size() > 2) k.entry.erase(0, k.entry.size() - 2);
            }
        }
    }
    // When a target on the IT-45 reaches the post at its closing speed (s), or -1 if it isn't closing.
    double ArrivalS(int aspd_number) const {
        if (!post_) return -1.0;
        for (const AspdTrack& t : post_->Tracks()) {
            if (t.number != aspd_number) continue;
            const Vec3 rel = t.position - post_->Position();
            const double range = rel.HorizontalLength();
            const double closing = range > 1.0 ? -(rel.x * t.velocity.x + rel.y * t.velocity.y) / range : 0.0;
            return closing > 1.0 ? range / closing : -1.0;
        }
        return -1.0;
    }
    // The plotter's handwork speed (his tier's), and how well the squad leader orients (his tier's azimuth error
    // against the reference tier's: 1 for a trained one).
    double PlotterSpeed() const {
        for (std::size_t i = 0; i < crew_.size(); ++i) {
            if (crew_[i].Role() != console_.post_plotter) continue;
            if (const SkillTier* t = SkillOf(def_.crew[i])) return t->wheel_rate_factor;
        }
        return 1.0;
    }
    double OrientQuality() const {
        const SkillTier* ref = def_.has_skills ? def_.skills.Reference() : nullptr;
        if (ref == nullptr || !(ref->azimuth_error_deg > 0.0)) return 1.0;
        const std::string orienter = OrientingSeat();
        for (std::size_t i = 0; i < crew_.size(); ++i) {
            if (crew_[i].Role() != orienter) continue;
            if (const SkillTier* t = SkillOf(def_.crew[i])) return t->azimuth_error_deg / ref->azimuth_error_deg;
        }
        return 1.0;
    }
    // The seat whose drill makes the post oriented (the one that owns the controls the subsystem waits for).
    std::string OrientingSeat() const {
        const SubsystemDef* d = console_.equipment.Def(console_.post_oriented);
        if (d == nullptr) return std::string();
        for (const Condition& c : d->enable) {
            if (c.kind != Condition::Kind::ControlEquals && c.kind != Condition::Kind::ControlAtLeast) continue;
            if (const ControlDef* cd = console_.panel.Find(c.id)) return cd->station;
        }
        return std::string();
    }

    // The targets as they are, for the boards (alive and spawned).
    std::vector<BoardContact> TruthContacts() {
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
        return contacts;
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
            // The commander heard it on the radio, and passes it on; not a target he heard destroyed (the PU-12's plot
            // a few seconds behind the kill report).
            if (e.actor == "Commander" && e.number > 0 && reported_kills_.count(e.subject) == 0) {
                designations_.push_back(e);
                designations_heard_.push_back(e);
                KnownTarget k;
                k.target = e.subject;
                k.number = e.number;
                k.has_azimuth = k.has_range = k.true_azimuth = true;
                k.azimuth_deg = e.azimuth_mils * 360.0 / 6000.0;
                k.range_m = e.range_m;
                k.has_height = e.height_hm > 0;
                k.height_m = e.height_hm * 100.0;
                k.time_s = time_;
                k.source = "radio";
                pictures_[Cmdr()].Learn(k);
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
            if (engagement_on_ && !engagement_.TrackedTarget().empty()) ++npc_launched_at_[engagement_.TrackedTarget()];
            if (engagement_on_) {  // a salvo (doctrine salvo): this launch opens one if nothing is in the air, else adds to it
                if (!engagement_.ShotsInFlight() || salvo_target_ != engagement_.TrackedTarget()) {
                    salvo_target_ = engagement_.TrackedTarget();
                    salvo_count_ = 0;
                }
                ++salvo_count_;
            }
            if (engagement_on_) OwnReport(BatteryFire::Event::Kind::Launch, engagement_.TrackedTarget());
            log_.Record({time_, DebriefEntry::Kind::Launch, std::string(), console_.launch_button,
                         "missile " + std::to_string(launches_) + from +
                             (engagement_on_ ? std::string() : std::string(" (no engagement model: not resolved)"))});
            if (engagement_on_) {
                const std::string& jam = console_.has_launcher ? console_.launcher.jam_control : std::string();
                engagement_.OnLaunch(time_, launches_, world_, eng_targets_, log_, !jam.empty() && console_.panel.Get(jam) == 1);
                if (!HasRwr(engagement_.TrackedTarget())) CueTarget(engagement_.TrackedTarget(), true);
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
    std::unique_ptr<MapTerrain> map_ground_;  // or its map's ground (terrain.type "map")
    Weather weather_;      // the scenario's "weather" (still, clear air without one)
    World world_;
    AcmiWriter acmi_;
    std::mt19937 rng_;
    bool engagement_on_;
    std::unique_ptr<EmSensorLink> em_link_;  // before engagement_: the model keeps a pointer to it
    std::vector<int> rwr_warned_;            // per target: 1 a lock warning noted, 2 a launch warning noted
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
    struct WheelWatch {
        std::string id;
        std::string station;
        double value = 0.0;
        double moved_s = -1e9;  // when it last turned
    };
    std::vector<WheelWatch> wheels_;

    struct Seat {
        std::string role;
        std::string player;
        ProcedureTracker tracker;
        std::vector<std::string> queued;  // procedures ordered while one was under way
        double taken_s = 0.0;             // when the player took it
        double last_action_s = -1e9;      // his last move there
    };
    AssistLevel assist_level_ = AssistLevel::Guided;
    std::vector<MoveVerdict> verdicts_;
    // How long the player at a seat has done nothing (since he took it, or since his last move).
    double Hesitation(const std::string& station) const {
        for (const Seat& s : seats_) {
            if (s.role == station) return time_ - std::max(s.taken_s, s.last_action_s);
        }
        return 0.0;
    }
    // An NPC's patience at a seat (its tier's), the hint level's wait.
    double PatienceAt(const std::string& station) const {
        for (const CrewMember& m : crew_) {
            if (m.Role() == station && m.HasSkill()) return m.Skill().patience_s;
        }
        return 5.0;
    }
    // A player's move at a held seat against the drill master's best for the training task (not while demonstrating).
    void JudgeMove(const std::string& station, const std::string& control, int position) {
        if (!drill_ || !def_.has_crew_data || assist_level_ == AssistLevel::Demonstrate) return;
        if (console_.panel.Get(control) == position) return;  // no change
        MoveVerdict v;
        v.time_s = time_;
        v.station = station;
        v.control = control;
        v.position = position;
        v.task = TrainingTask();
        const CrewTask* t = def_.crew_data.FindTask(v.task);
        if (t == nullptr) return;
        const DrillGoal goal = drill_->GoalFor(*t);
        const DrillAdvice best = drill_->NextFor(station, goal, console_);
        v.best_control = best.control;
        v.best_position = best.position;
        v.best_description = best.description;
        v.best = best.control == control && best.position == position;
        if (!v.best) {
            v.cost_s = drill_->CostOf(goal, console_, control, position);
            v.best = v.cost_s <= 0.0;
        }
        if (!v.best) {
            char cost[32];
            std::snprintf(cost, sizeof(cost), "%.1f", v.cost_s);
            Event(def_.id, station + ": " + control + " to " + std::to_string(position) + " off the drill; the best move was " +
                               (best.control.empty() ? std::string("none") : best.description.empty() ? best.control : best.description) +
                               " (" + cost + " s later)");
        }
        verdicts_.push_back(std::move(v));
    }
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
    std::vector<double> report_said_s_;  // when each was last said
    std::vector<double> report_off_s_;   // when its condition last went false
    static constexpr double kReportAgainS = 10.0;
    static constexpr double kReportFlickerS = 3.0;
    struct WaitWatch {
        double since_s = -1.0;
        int reminded = 0;
    };
    std::vector<WaitWatch> waits_;  // per crew member
    std::map<std::string, int> reminders_to_;
    std::vector<C2Network::Event> designations_;  // heard this step, to pass on

    // The marks the grades are measured from.
    double ideal_ready_s_ = -1.0;   // the drill master's К бою from the session's start (when the vehicle starts halted, not ready)
    double actual_ready_s_ = -1.0;
    std::vector<DecisionRecord> decisions_;
    std::string judged_choice_;
    int judged_launches_ = 0;
    std::map<std::string, double> seen_at_;  // each target's first echo on the search scope
    std::string zone_target_;
    bool was_flying_ = false;
    double free_since_s_ = -1.0;            // the last missile's result came in (the crew free for the next target)
    double zone_since_s_ = -1.0;            // ЦЕЛЬ В ЗОНЕ lit for the tracked target since
    // The truth about a live target from the unit, for the evaluation (null if it isn't in the air).
    bool TrackOf(const std::string& id, DecisionTrack& out) {
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            if (def_.targets[i].id != id || !targets_[i].spawned) continue;
            const Aircraft* a = world_.FindAircraft(targets_[i].world_id);
            if (a == nullptr || !a->alive) return false;
            out.target = id;
            out.target_class = def_.targets[i].target_class;
            out.friendly = def_.targets[i].friendly;
            out.position = a->position - def_.unit_position;
            out.velocity = a->velocity;
            out.agl_m = world_.Frame().AltitudeOf(a->position) - world_.GetTerrain().HeightAt(a->position.x, a->position.y);
            return true;
        }
        return false;
    }
    void JudgeDecisions() {
        if (!engagement_on_) return;
        for (const Contact& c : engagement_.SearchPicture()) {
            if (seen_at_.count(c.target_id) == 0) seen_at_[c.target_id] = time_;
        }
        const std::string tracked = engagement_.TrackedTarget();
        const bool in_zone = console_.equipment.HasInput("target_in_zone") && console_.equipment.GetInput("target_in_zone");
        if (!in_zone || tracked != zone_target_) {
            zone_target_ = in_zone ? tracked : std::string();
            zone_since_s_ = in_zone ? time_ : -1.0;
        }
        const NetworkChart chart;
        const double cycle_s = chart.result_s + chart.launch_s;
        // The crew is free for the next target once a missile's result is in (one tracking channel).
        const bool flying = engagement_.ShotsInFlight();
        if (was_flying_ && !flying) free_since_s_ = time_;
        was_flying_ = flying;
        // A target ordered taken: against the evaluation of every hostile his crew had reported.
        if (!assigned_target_.empty() && assigned_target_ != judged_choice_) {
            judged_choice_ = assigned_target_;
            std::vector<DecisionTrack> tracks;
            DecisionTrack d;
            for (const auto& kv : seen_at_) {
                if (TrackOf(kv.first, d)) tracks.push_back(d);
            }
            if (TrackOf(assigned_target_, d) && seen_at_.count(assigned_target_) == 0) tracks.push_back(d);
            const std::vector<TargetScore> scores = ScoreTargets(def_.tuning, tracks, cycle_s);
            DecisionRecord r;
            r.time_s = time_;
            r.kind = "target";
            r.target = assigned_target_;
            for (const TargetScore& s : scores) {
                if (s.target == assigned_target_) r.score = s.score;
                if (r.best.empty() && !s.friendly) {
                    r.best = s.target;
                    r.best_score = s.score;
                }
            }
            r.good = r.best.empty() || r.best == r.target || r.score >= 0.9 * r.best_score;
            decisions_.push_back(r);
        }
        // A launch: against the network chart from the detection and the zone's entry (the far edge).
        if (launches_ > judged_launches_) {
            judged_launches_ = launches_;
            DecisionRecord r;
            r.time_s = time_;
            r.kind = "launch";
            r.target = tracked;
            const auto seen = seen_at_.find(tracked);
            const double ready = std::max(chart.radar_on_s + chart.capture_s + chart.computer_s, chart.prep_s) + chart.launch_s;
            // From the detection, or from when the crew was done with the target before (the chart starts with a free crew).
            double ideal = seen != seen_at_.end() ? std::max(seen->second, free_since_s_) + ready : -1.0;
            if (zone_since_s_ >= 0.0 && zone_target_ == tracked) ideal = std::max(ideal, zone_since_s_);
            r.ideal_s = ideal;
            r.late_s = ideal >= 0.0 ? std::max(0.0, time_ - ideal) : 0.0;
            // The chart is the ideal crew's; a trained crew is within the published norm (nvo_2015, korovin_tiv_2010: up
            // to 40 s from detection), so late is beyond 40 s less the chart's own time.
            r.good = r.late_s <= 40.0 - ready;
            decisions_.push_back(r);
        }
    }
    double first_seen_s_ = -1.0;    // the first echo on the search scope
    double first_launch_s_ = -1.0;
    double in_reach_s_ = -1.0;      // the first echo within the tracking radar's reach
    void UpdateGrades() {
        if (!def_.has_crew_data) return;
        if (actual_ready_s_ < 0.0 && ideal_ready_s_ > 0.0) {
            const CrewTask* t = def_.crew_data.FindTask("ready_halted");
            if (t != nullptr && console_.equipment.AllHold(t->conditions, console_.panel)) actual_ready_s_ = time_;
        }
        if (first_seen_s_ < 0.0 && engagement_on_ && !engagement_.SearchPicture().empty()) first_seen_s_ = time_;
        if (in_reach_s_ < 0.0 && engagement_on_) {
            const double reach = console_.track.max_range_m > 0.0 ? console_.track.max_range_m : 1e9;
            for (const Contact& c : engagement_.SearchPicture()) {
                if (c.range_m <= reach) in_reach_s_ = time_;
            }
        }
        if (first_launch_s_ < 0.0 && launches_ > 0) first_launch_s_ = time_;
        JudgeDecisions();
    }

    void StartCallouts() {
        if (!def_.has_crew_data) return;
        callouts_ = std::make_unique<CalloutBus>(&def_.crew_data);
        drill_ = std::make_unique<DrillMaster>(console_, &def_.crew_data, def_.has_skills && def_.skills.Reference() != nullptr
                                                                             ? def_.skills.Reference()->reaction_s : 0.55);
        for (const AutoReportDef& r : def_.crew_data.reports) {
            report_state_.push_back(console_.equipment.Holds(r.when, console_.panel) ? 1 : 0);
        }
        started_cold_ = !console_.equipment.IsReady("power");
        if (const CrewTask* t = def_.crew_data.FindTask("ready_halted")) {  // the ideal "К бою" from here, if there is one to do
            if (console_.equipment.GetInput("vehicle_stopped") && !console_.equipment.AllHold(t->conditions, console_.panel)) {
                const IdealTimeline tl = drill_->Ideal(drill_->GoalFor(*t), console_);
                if (tl.reached) ideal_ready_s_ = tl.time_s;
            }
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
    bool quiet_orders_ = false;
    void SayOrder(const std::string& station, const std::string& procedure) {
        if (!callouts_ || quiet_orders_ || station == Cmdr()) return;  // his own drill needs no words
        const OrderDef* best = nullptr;
        for (const OrderDef& o : def_.crew_data.orders) {
            for (const OrderStart& s : o.procedures) {
                if (s.station == station && s.procedure == procedure && (best == nullptr || o.procedures.size() < best->procedures.size())) best = &o;
            }
        }
        if (best != nullptr) SpeakOrder(best->callout);
    }
    // The words for drills given together: the one order that starts them all when there is one ("К бою!", "Пункт
    // управления развернуть!"), else each drill's own.
    void SayOrders(const std::vector<std::pair<std::string, std::string>>& given) {
        std::vector<std::pair<std::string, std::string>> others;
        for (const auto& g : given) {
            if (g.first != Cmdr()) others.push_back(g);
        }
        if (others.size() > 1) {
            const OrderDef* best = nullptr;
            for (const OrderDef& o : def_.crew_data.orders) {
                bool all = true;
                for (const auto& g : others) {
                    bool in = false;
                    for (const OrderStart& s : o.procedures) in = in || (s.station == g.first && s.procedure == g.second);
                    all = all && in;
                }
                if (all && (best == nullptr || o.procedures.size() < best->procedures.size())) best = &o;
            }
            if (best != nullptr) {
                SpeakOrder(best->callout);
                return;
            }
        }
        for (const auto& g : others) SayOrder(g.first, g.second);
    }
    // An order's words, filled in from what the crew knows (the latest designation's number); not spoken twice within
    // 3 s (one order starts several drills) nor with a blank left in it.
    void SpeakOrder(const std::string& callout_id) {
        if (!callouts_) return;
        if (callout_id == last_order_said_ && time_ - last_order_s_ < 3.0) return;
        const CalloutDef* def = def_.crew_data.FindCallout(callout_id);
        std::map<std::string, std::string> fields;
        // The number is the ordered target's, from its designation; a target he has no designation of has none (the
        // order then goes by its azimuth). With no target ordered yet: the latest designation's.
        int number = 0;
        for (const C2Network::Event& e : designations_heard_) {
            if (assigned_target_.empty() || e.subject == assigned_target_) number = e.number;
        }
        if (number > 0) {
            char n[16];
            std::snprintf(n, sizeof(n), "%04d", number);
            fields["number"] = n;
        }
        if (def == nullptr || FillCallout(def->ru, fields).find('{') != std::string::npos) return;
        const std::string from = console_.FindStation(def->from) != nullptr ? def->from : Cmdr();
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
        // A stop while the battery forms its column or marches is not a halt: nobody deploys for it.
        const bool column = battery_ && def_.battery_drive_own &&
                            (battery_->CurrentMode() == Battery::Mode::Forming || battery_->CurrentMode() == Battery::Mode::Marching);
        const bool halted = console_.equipment.GetInput("vehicle_stopped") && !column;
        const bool stowing = npc_stow_ordered_ && !npc_resumed_;
        // Readiness: the next drill of each idle seat toward the task.
        // Emission control (the battery's EMCON, an ambush not yet sprung): ready with the radars silent, the search
        // transmitter off; when it lifts, radiation on again.
        const bool silent = NpcSilent();
        if (silent && halted && console_.equipment.IsReady("soc_transmitter")) NpcGive("order_emcon_off", 30.0);
        if (!silent && npc_was_silent_) NpcGive("order_emcon_on", 30.0);
        npc_was_silent_ = silent;
        // Emission discipline (doctrine search_bursts): between bursts the transmitter off, the antenna turning; the
        // readiness drills then aim at the silent state, so they don't switch it back on or stop the antenna.
        const bool burst_off = !silent && NpcBurstOff(halted);
        if (def_.doctrine.search_on_s > 0.0 && halted && !silent && console_.equipment.IsReady("hv_ready")) {
            if (burst_off && console_.panel.Get("soc.hv") == 1) NpcGive("order_hv_off", 1.0);
            if (!burst_off && npc_burst_off_ && console_.panel.Get("soc.hv") == 0) NpcGive("order_hv_on", 1.0);
        }
        npc_burst_off_ = burst_off;
        // The radios: the commander's set off with a command net to hear (a scenario that starts them off).
        if (c2_ && console_.panel.Get("radio.right.power") == 0) NpcGive("order_radio_on", 120.0);
        const CrewTask* task = def_.crew_data.FindTask(halted ? (silent || burst_off ? "ready_silent" : "ready_halted") : "searching_on_march");
        if (task != nullptr && !stowing) {
            const DrillPlan plan = drill_->Plan(drill_->GoalFor(*task), console_.panel, console_.equipment);
            const bool was_quiet = quiet_orders_;
            quiet_orders_ = true;  // the words once for all the drills given now
            std::vector<std::pair<std::string, std::string>> given;
            for (const DrillPlan::Seat& s : plan.seats) {
                if (!SeatFree(s.station)) continue;
                for (const std::string& drill : s.procedures) {
                    if (NpcOrdered(s.station, drill, 60.0)) continue;
                    if (NpcOrder(s.station, drill)) given.emplace_back(s.station, drill);
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
            quiet_orders_ = was_quiet;
            if (!was_quiet) SayOrders(given);
        }
        NpcLogistics();
        // A low target designated (under the search radar's clutter height, with a margin) and СДЦ off: "СДЦ включить!"
        // (a low target is invisible to the search radar in the ground clutter without it).
        if (engagement_on_ && !console_.search.mti_control.empty() && console_.panel.Get(console_.search.mti_control) == 0 && !NpcSilent()) {
            for (const KnownTarget& k : pictures_[Cmdr()].Recent(time_, 60.0)) {
                if (k.has_height && !k.friendly && k.height_m <= def_.tuning.clutter_altitude_m + 100.0) {
                    NpcGive("order_mti", 120.0);
                    break;
                }
            }
        }
        if (!engagement_on_) return;
        if (!halted) {  // on the march: a hostile he knows of inside 20 km stops the vehicle at once (mon_2112_80 p63-68)
            for (const KnownTarget& k : pictures_[Cmdr()].Recent(time_, 30.0)) {
                if (!k.has_range || k.friendly || k.range_m > 20000.0 || npc_halt_ordered_) continue;
                npc_halt_ordered_ = true;
                SpeakOrder("order_halt");
                if (battery_ && def_.battery_drive_own) {  // the battery drives us: its short halt (off the road, deploy)
                    BatteryOrder o;
                    o.kind = BatteryOrder::Kind::ShortHalt;
                    battery_->Order(o, time_);
                } else if (NpcAt("driver")) {  // our driver stops the vehicle
                    npc_halt_at_ = time_ + SpeakDelayS("driver") + 2.0;
                }
                break;
            }
            if (npc_halt_at_ > 0.0 && time_ >= npc_halt_at_) {
                console_.equipment.SetInput("vehicle_stopped", true);
                npc_halt_at_ = -1.0;
            }
            return;
        }
        npc_halt_ordered_ = false;
        if (NpcBetweenFights()) return;
        // The engagement, by the crew data's orders (order_take_target, order_prepare, fire_permitted, order_jammer,
        // order_low_target): the seats and drills each order starts are data, not code.
        const EngagementModel& eng = engagement_;
        const TrackMethod m = eng.Method();
        const std::string tracked = eng.TrackedTarget();
        // A missile in the air: wait for the result; under a two-missile salvo (doctrine salvo) the second goes first.
        if (eng.ShotsInFlight() && !NpcSalvoDue(tracked)) return;
        if (m == TrackMethod::None) {
            npc_target_.clear();
            // Take the nearest contact once it is within the tracking radar's reach (the drill switches the tracking
            // transmitter on only when the azimuth line is on the target: voenizdat_osa_1991 p250); a designation
            // from the PU-12 counts when it is that close too.
            // He knows what his crew told him and what the radio said (his picture): the nearest hostile he has heard of
            // in the last 30 s, unless his judgement slips (his tier's priority error: he takes the next one).
            const double reach = console_.track.max_range_m > 0.0 ? console_.track.max_range_m : 1e9;
            std::vector<KnownTarget> known;
            const std::vector<KnownTarget> recent = pictures_[Cmdr()].Recent(time_, 30.0);
            // A target the battery commander gave another launcher (he heard it on the battery net) is theirs, and so is
            // an echo beside it on his scope (a second track of the same aircraft, which the tracking radar would lock).
            auto theirs = [&](const KnownTarget& k) { return (battery_fire_ && battery_fire_->Engaged(k.target)) || GivenElsewhere(k.target); };
            auto beside_theirs = [&](const KnownTarget& k) {
                if (!k.has_range) return false;
                for (const KnownTarget& o : recent) {
                    if (o.target == k.target || !o.has_range || o.true_azimuth != k.true_azimuth || !theirs(o)) continue;
                    const double a = DegToRad(k.azimuth_deg), b = DegToRad(o.azimuth_deg);
                    const double dx = k.range_m * std::sin(a) - o.range_m * std::sin(b);
                    const double dy = k.range_m * std::cos(a) - o.range_m * std::cos(b);
                    if (dx * dx + dy * dy < kBesideTheirsM * kBesideTheirsM) return true;
                }
                return false;
            };
            for (const KnownTarget& k : recent) {
                if (k.has_range && !k.friendly && !eng.KnownFriend(k.target) && !theirs(k) && !beside_theirs(k) &&
                    decoys_known_.count(k.target) == 0) {
                    known.push_back(k);
                }
            }
            std::sort(known.begin(), known.end(), [](const KnownTarget& a, const KnownTarget& b) { return a.range_m < b.range_m; });
            // With the battery commander in a real post, what he gave this vehicle comes first: the latest designation
            // to it of a target still in the picture (the post reads out only this vehicle's on its net).
            if (LinkedToPost()) {
                for (auto d = designations_heard_.rbegin(); d != designations_heard_.rend(); ++d) {
                    const auto at = std::find_if(known.begin(), known.end(), [&](const KnownTarget& k) { return k.target == d->subject; });
                    if (at == known.end()) continue;
                    std::rotate(known.begin(), at, at + 1);
                    break;
                }
            }
            if (known.empty()) return;
            std::size_t pick = 0;
            if (known.size() > 1 && npc_choice_for_ != known.front().target) {
                npc_choice_for_ = known.front().target;
                const SkillTier* t = CommanderTier();
                npc_choice_slip_ = t != nullptr && static_cast<double>(hear_rng_()) / 4294967296.0 < t->priority_error_chance;
            }
            if (npc_choice_slip_ && known.size() > 1) pick = 1;
            const KnownTarget& k = known[pick];
            // Silent: a designated hostile within the TV's reach is engaged by the TV, the range from the board (the
            // EMCON engagement: emcon_tv_azimuth, emcon_tv_engagement).
            if (NpcSilent()) {
                if (k.number > 0 && k.range_m <= def_.tuning.tv_max_range_m) {
                    assigned_target_ = k.target;
                    NpcGive("order_emcon_tv", 90.0);
                }
                return;
            }
            if (k.range_m > reach || !console_.equipment.IsReady("hv_ready")) {
                NpcIdentify();  // meanwhile: who is it?
                return;
            }
            // A target in reach interrupts the function check (the deputy's РОД РАБОТЫ goes back with the next drills).
            if (CrewMember* dep = MemberAt("deputy_commander")) {
                if (dep->IsBusy() && dep->Tracker().GetProcedure().id == "function_check" && SeatHolder("deputy_commander").empty()) {
                    dep->CancelOrder();
                    console_.panel.Set("fo.mode", 1);
                }
            }
            assigned_target_ = k.target;
            if (NpcGive("order_take_target", 60.0) && k.number == 0 && callouts_) {  // no number from the PU-12: by its azimuth
                callouts_->Say(time_, "order_take_contact", Cmdr(), {{"azimuth", MilsText(k.azimuth_deg)}});
            }
            return;
        }
        // Never at a friend: "Пуск запрещаю!" and the track let go ("Цель сбросить!").
        const KnownTarget* told = pictures_[Cmdr()].Find(tracked);
        if (eng.KnownFriend(tracked) || (told != nullptr && told->friendly)) {
            if (npc_held_ != tracked) {
                npc_held_ = tracked;
                SpeakOrder("hold_fire");
                GiveCrewOrder("order_drop_target");
            }
            return;
        }
        // In a battery whose commander is in a linked post: the crew has locked a target he gave another launcher (they
        // took an echo beside it): "Цель сбросить!", and look again.
        if (LinkedToPost() && GivenElsewhere(tracked)) {
            if (npc_released_ != tracked) {
                npc_released_ = tracked;
                GiveCrewOrder("order_drop_target");
            }
            return;
        }
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
        // One permission stands until a missile goes (the launch is counted a step after ПУСК); after a miss with the
        // target still in the zone it is given again (re-engage: voenizdat_osa_1991 p257).
        if (npc_permit_launches_ == launches_ && time_ - npc_permit_s_ < 60.0) return;
        if (!NpcTvCleared(tracked, m)) return;
        // Identified first: interrogated with no reply (the senior operator's report), or designated by the PU-12 (the
        // battery commander's identification). Under EMCON the interrogator is silent too: the designation must do.
        const bool identified = told != nullptr && (told->interrogated || told->number > 0);
        if (!identified && !NpcSilent()) {
            NpcGive("order_iff", 10.0);
            return;
        }
        // A missile already at this target (a miss, still in the zone): "Вторая — пуск!".
        const char* permit = npc_launched_at_.count(tracked) != 0 ? "order_second_missile" : "fire_permitted";
        if (NpcGive(permit, 0.0)) {
            npc_permit_launches_ = launches_;
            npc_permit_s_ = time_;
        }
    }
    // Under a two-missile salvo (doctrine salvo) the second missile at the target goes as soon as the launcher allows
    // (3-5 s after the first: korovin_tiv_2010), while the first is still in the air; after that the results are waited
    // for, and a miss with the target still in the zone is engaged again with a new salvo.
    bool NpcSalvoDue(const std::string& tracked) const {
        return def_.doctrine.salvo > 1 && !tracked.empty() && tracked == salvo_target_ && salvo_count_ < def_.doctrine.salvo &&
               engagement_.ShotsInFlightAt(tracked) == salvo_count_ && engagement_.Method() != TrackMethod::None;
    }
    // Warned of decoys (doctrine tv_check), the commander launches only at what the TV has shown him (roadmap E6): the
    // deputy looks at what the radar holds through the narrow lens (tv_watch); once he can make its type out
    // (em/optical.hpp), a decoy (a small drone where the echo says aircraft) is reported, the launch forbidden and the
    // track dropped, and it is not taken again. If the TV can't see it (night, haze, too far) by the time it is in the
    // zone, he goes by the radar. The deputy may miss it (his tier's identification error). False: wait.
    bool NpcTvCleared(const std::string& tracked, TrackMethod m) {
        if (decoys_known_.count(tracked) != 0) {  // a decoy taken again: let it go at once
            if (npc_decoy_dropped_ != tracked || time_ - npc_decoy_dropped_s_ > 10.0) {
                npc_decoy_dropped_ = tracked;
                npc_decoy_dropped_s_ = time_;
                GiveCrewOrder("order_drop_target");
            }
            return false;
        }
        if (!def_.doctrine.tv_check || m != TrackMethod::Radar || npc_tv_cleared_ == tracked) return true;
        const TrackSensorDef& d = console_.track;
        const bool narrow = d.fov_control.empty() || console_.panel.Get(d.fov_control) == 1;
        if (!console_.equipment.IsReady(d.tv_camera) || !narrow) {
            NpcGive("order_tv_watch", 20.0);
            return false;
        }
        const em::OpticalLook look = engagement_.TvLook(console_, tracked);
        if (look.recognised) {
            if (!IsDecoy(tracked)) {
                npc_tv_cleared_ = tracked;
                return true;
            }
            const SkillTier* dep = TierAt("deputy_commander");
            const bool missed = dep != nullptr && static_cast<double>(hear_rng_()) / 4294967296.0 < dep->priority_error_chance;
            if (missed) {
                npc_tv_cleared_ = tracked;
                Event(tracked, "the deputy commander takes the decoy on the TV for an aircraft");
                return true;
            }
            decoys_known_.insert(tracked);
            if (assigned_target_ == tracked) assigned_target_.clear();
            npc_decoy_dropped_ = tracked;
            npc_decoy_dropped_s_ = time_;
            if (NpcAt("deputy_commander")) Say("false_target", "deputy_commander", {});
            Event(tracked, "made out on the TV: a decoy drone");
            SpeakOrder("hold_fire");
            GiveCrewOrder("order_drop_target");
            return false;
        }
        if (!look.seen && console_.equipment.GetInput("target_in_zone")) {
            npc_tv_cleared_ = tracked;  // nothing to see on the TV and the target in the zone: by the radar
            return true;
        }
        return false;
    }
    bool IsDecoy(const std::string& target_id) const {
        for (const ScenarioTarget& t : def_.targets) {
            if (t.id == target_id) return t.decoy;
        }
        return false;
    }
    const SkillTier* TierAt(const std::string& seat) const {
        for (std::size_t i = 0; i < crew_.size() && i < def_.crew.size(); ++i) {
            if (crew_[i].Role() == seat) return SkillOf(def_.crew[i]);
        }
        return nullptr;
    }
    // Emission discipline (doctrine search_bursts): halted, nothing tracked and no hostile he knows of within the
    // tracking radar's reach, the search radar radiates in bursts, on for on_s and off for off_s.
    bool NpcBurstOff(bool halted) {
        if (def_.doctrine.search_on_s <= 0.0 || !halted || engagement_.Method() != TrackMethod::None) return false;
        const double reach = console_.track.max_range_m > 0.0 ? console_.track.max_range_m : 1e9;
        for (const KnownTarget& k : pictures_[Cmdr()].Recent(time_, 30.0)) {
            if (k.has_range && !k.friendly && k.range_m <= reach) return false;
        }
        return std::fmod(time_, def_.doctrine.search_on_s + def_.doctrine.search_off_s) >= def_.doctrine.search_on_s;
    }
    std::string npc_tv_cleared_;
    std::string npc_decoy_dropped_;
    double npc_decoy_dropped_s_ = -1e9;
    std::set<std::string> decoys_known_;
    bool npc_burst_off_ = false;

    // Every hostile he has heard of within the interrogator's reach that isn't identified yet: "Запрос!" (the senior
    // operator's ЗАПРОС). Not one the PU-12 designated (identified by the battery commander), not under EMCON.
    void NpcIdentify() {
        const SearchSensorDef& d = console_.search;
        if (d.iff_query.empty() || NpcSilent() || !console_.equipment.IsReady(d.iff_ready)) return;
        for (const KnownTarget& k : pictures_[Cmdr()].Recent(time_, 30.0)) {
            if (k.friendly || k.interrogated || k.number > 0 || !k.has_range || k.range_m > d.iff_max_range_m) continue;
            NpcGive("order_iff", 15.0);
            return;
        }
    }
    // The battery's EMCON, or an ambush not yet sprung: sprung once a hostile is designated within 15 km (sam3d).
    bool NpcSilent() {
        if (!battery_) return false;
        if (battery_->Emcon()) return true;
        if (battery_->CurrentMode() != Battery::Mode::Ambush) {
            npc_ambush_sprung_ = false;
            return false;
        }
        if (!npc_ambush_sprung_) {
            for (const KnownTarget& k : pictures_[Cmdr()].Recent(time_, 30.0)) {
                if (k.has_range && !k.friendly && k.range_m <= 15000.0) npc_ambush_sprung_ = true;
            }
            if (npc_ambush_sprung_) Event(def_.id, "the ambush is sprung: radiation on");
        }
        return !npc_ambush_sprung_;
    }
    bool npc_was_silent_ = false;
    bool npc_ambush_sprung_ = false;
    std::string npc_held_;
    std::map<std::string, int> npc_launched_at_;  // launches at each target (the second missile's words)
    std::string salvo_target_;  // the salvo in the air: its target and how many missiles it has (doctrine salvo)
    int salvo_count_ = 0;
    double drop_at_s_ = -1.0;
    int npc_permit_launches_ = -1;
    double npc_permit_s_ = -1e9;
    std::string npc_target_;
    bool npc_prepared_ = false;
    std::string npc_choice_for_;
    bool npc_choice_slip_ = false;
    std::string npc_branch_for_;
    double npc_branch_at_ = 0.0;
    bool npc_halt_ordered_ = false;
    bool npc_checked_ = false;
    bool started_cold_ = false;  // the vehicle had no power at the session's start
    bool npc_fought_ = false;
    double npc_last_hostile_s_ = -1.0;
    bool npc_stow_ordered_ = false;
    bool npc_resumed_ = false;

    // Between fights at a halt (docs/crew/03_ENVIRONMENTS_AND_PATHS.md §2.1, §2.6): with nothing hostile known and the
    // vehicle ready, the function check once; when the fight is over (something was fought, nothing hostile heard of for
    // 90 s, nothing in the air), stow for the march, and once stowed the battery marches on. True while it is busy so.
    bool NpcBetweenFights() {
        bool hostile = false;
        for (const KnownTarget& k : pictures_[Cmdr()].Recent(time_, 30.0)) hostile = hostile || (!k.friendly && k.has_range);
        if (hostile) {
            npc_last_hostile_s_ = time_;
            npc_fought_ = true;
        }
        if (engagement_.Method() != TrackMethod::None || engagement_.ShotsInFlight()) return false;
        const CrewTask* ready = def_.crew_data.FindTask(NpcSilent() ? "ready_silent" : "ready_halted");
        const bool is_ready = ready != nullptr && console_.equipment.AllHold(ready->conditions, console_.panel);
        // Orientation (voenizdat_osa_1991 p246: the deputy orients the vehicle as part of К бою, the gyro half-compass
        // set to the course angle so that designations' azimuths agree): once ready at a halt with nothing hostile known,
        // if it isn't oriented yet. It needs the deputy at the gyro panel, so not while a fight is on.
        if (!hostile && is_ready && !Oriented() && NpcGive("order_orient", 120.0)) return true;
        // The function check belongs to the preliminary preparation from cold (flak11: 3.8 min switch-on, then 3.0 min check).
        if (!hostile && is_ready && !npc_checked_ && !npc_fought_ && started_cold_) {
            npc_checked_ = NpcGive("order_function_check", 600.0);
            return npc_checked_;
        }
        // A reload under way (the launcher not full and a TZM in the battery): the crew waits for it before marching on.
        const bool reloading = launcher_ && battery_ && launcher_->LoadedCount() < launcher_->Capacity() && BatteryHasTzm();
        // Stowed and on again only on a march (the battery drives this vehicle on its route) and once no air attack is
        // expected: a battery deployed in its position stays ready.
        const bool marching_on = battery_ && def_.battery_drive_own && !battery_->AirThreat();
        if (marching_on && npc_fought_ && !hostile && !reloading && time_ - npc_last_hostile_s_ > 90.0) {
            if (!npc_stow_ordered_) {
                npc_stow_ordered_ = GiveCrewOrder("order_march");
                return true;
            }
            const CrewTask* stowed = def_.crew_data.FindTask("stowed");
            if (!npc_resumed_ && stowed != nullptr && console_.equipment.AllHold(stowed->conditions, console_.panel)) {
                npc_resumed_ = true;
                if (NpcAt("driver")) Say("ready_to_march", "driver");
                if (battery_ && def_.battery_drive_own) {
                    BatteryOrder o;
                    o.kind = BatteryOrder::Kind::Resume;
                    battery_->Order(o, time_);
                }
                Event(def_.id, "the crew is stowed and marches on");
            }
            return true;
        }
        return false;
    }
    double npc_halt_at_ = -1.0;
    bool npc_empty_reported_ = false;
    bool npc_fuel_reported_ = false;
    bool npc_fuel_requested_ = false;

    // Logistics: the deputy reports the launcher empty and the commander asks the battery for the TZM; the driver reports
    // the fuel at half (the crew asks for fuel at half: samsim_thread, ePap). The TZM and the fuel truck themselves come
    // through the battery (ServiceDrills).
    void NpcLogistics() {
        if (launcher_) {
            const bool empty = launcher_->LoadedCount() == 0;
            if (empty && !npc_empty_reported_ && !engagement_.ShotsInFlight()) {
                npc_empty_reported_ = true;
                if (NpcAt("deputy_commander")) Say("launcher_empty", "deputy_commander");
                const std::string callsign = battery_ ? OwnCallsign() : std::string("21");
                if (callouts_) callouts_->Say(time_, "request_tzm", Cmdr(), {{"callsign", callsign}}, 3.0);
            }
            if (!empty) npc_empty_reported_ = false;
        }
        if (fuel_ && battery_) {  // the fuel model asks at its reserve; the commander says so on the battery net
            if (fuel_->Requested() && !npc_fuel_requested_ && NpcAt(Cmdr()) && callouts_) {
                callouts_->Say(time_, "request_fuel", Cmdr(), {{"callsign", OwnCallsign()}}, 3.0);
            }
            npc_fuel_requested_ = fuel_->Requested();
        }
        if (fuel_) {
            const bool half = fuel_->LevelL() <= 0.5 * fuel_->TankL();
            if (half && !npc_fuel_reported_ && NpcAt("driver")) {
                npc_fuel_reported_ = true;
                Say("fuel_half", "driver");
            }
            if (!half) npc_fuel_reported_ = false;
        }
    }
    std::string OwnCallsign() const {
        if (battery_) {
            for (const Battery::Vehicle& v : battery_->Vehicles()) {
                if (v.def.own) return v.def.callsign;
            }
        }
        return "21";
    }
    const SkillTier* CommanderTier() const {
        for (std::size_t i = 0; i < crew_.size() && i < def_.crew.size(); ++i) {
            if (crew_[i].Role() == Cmdr()) return SkillOf(def_.crew[i]);
        }
        return nullptr;
    }

    // Give an order of the crew data unless it was given within `again_s`, and only when every seat it goes to is free.
    // The battery has a transport-loader.
    bool BatteryHasTzm() const {
        if (!battery_) return false;
        for (const Battery::Vehicle& v : battery_->Vehicles()) {
            if (v.def.kind == BatteryVehicleKind::Reload) return true;
        }
        return false;
    }
    bool NpcGive(const std::string& order_id, double again_s) {
        const OrderDef* o = def_.crew_data.FindOrder(order_id);
        if (o == nullptr) return false;
        // He doesn't give an order that needs what isn't there (a missile, a track): CheckOrder's "needs" (world waits
        // he leaves to the crew: a launch permission given before the target is in the zone waits for it).
        if (!o->needs.empty()) {
            const OrderCheck c = CheckOrder(order_id);
            if (!c.can && c.reason_en != "done already") {
                bool need = false;
                for (const std::string& n : o->needs) {
                    need = need || (n == "missiles" && c.reason_en == "no missiles left") || (n == "tracked" && c.reason_en == "nothing tracked") ||
                           (n == "jamming" && c.reason_en == "no jammer tracked") || (n == "contact" && c.reason_en == "no target reported");
                }
                if (need) return false;
            }
        }
        if (NpcOrdered("order", order_id, again_s)) return false;
        for (const OrderStart& st : o->procedures) {
            if (!SeatFree(st.station)) return false;
        }
        GiveCrewOrder(order_id);
        npc_ordered_["order"] = {order_id, time_};
        return true;
    }
    std::vector<C2Network::Event> designations_heard_;  // every designation so far (the latest last)
    std::map<std::string, double> passed_at_;  // when the commander last passed a target's designation on

    // A crew member's reports as words: acknowledgement, done, can't, fault; and the start of a wait (reminders).
    void CrewCallouts(const CrewMember& c, const std::vector<CrewReport>& reports) {
        if (!callouts_ || !SeatHolder(c.Role()).empty()) return;
        for (const CrewReport& r : reports) {
            switch (r.kind) {
                // The commanding seat doesn't answer himself.
                case CrewReport::Kind::Started: if (c.Role() != Cmdr()) Say("ack", c.Role()); break;
                case CrewReport::Kind::Completed: if (c.Role() != Cmdr()) Say("done", c.Role()); break;
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
        // Automatic reports: a seat says it when its condition becomes true. A lamp that flickers (out for a moment
        // only) is not called again and again: he said it a moment ago.
        report_said_s_.resize(def_.crew_data.reports.size(), -1e9);
        report_off_s_.resize(def_.crew_data.reports.size(), -1e9);
        for (std::size_t i = 0; i < def_.crew_data.reports.size(); ++i) {
            const AutoReportDef& r = def_.crew_data.reports[i];
            const bool now = console_.equipment.Holds(r.when, console_.panel);
            const bool flicker = time_ - report_off_s_[i] < kReportFlickerS && time_ - report_said_s_[i] < kReportAgainS;
            if (now && report_state_[i] == 0 && NpcAt(r.from) && !flicker) {
                Say(r.callout, r.from);
                report_said_s_[i] = time_;
            }
            if (!now && report_state_[i] != 0) report_off_s_[i] = time_;
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
                            // His own duty with a new echo in the interrogator's reach: ЗАПРОС, unasked (the commander's
                            // "Запрос!" covers what he misses).
                            CrewMember* so = MemberAt("senior_operator");
                            const Procedure* iff = console_.FindProcedure("iff_interrogate");
                            if (so != nullptr && iff != nullptr && !so->IsBusy() && !console_.search.iff_query.empty() &&
                                e.range_m <= console_.search.iff_max_range_m) {
                                so->Queue(*iff);
                            }
                        }
                        break;
                    case EngagementEvent::Kind::TrackLost:
                        // not a target just shot down, nor one the crew let go
                        if (NpcAt("operator") && TargetRange(e.target) >= 0.0 && e.detail != "dropped") Say("lock_lost", "operator");
                        break;
                    case EngagementEvent::Kind::IffNoReply: {
                        KnownTarget k;  // the senior operator saw no reply mark beside the echo
                        k.target = e.target;
                        k.interrogated = true;
                        k.time_s = time_;
                        k.source = "IFF";
                        pictures_["senior_operator"].Learn(k);
                        if (NpcAt("senior_operator")) Say("iff_no_reply", "senior_operator", {}, &k);
                        break;
                    }
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
                    case EngagementEvent::Kind::Hit:
                        if (NpcAt("deputy_commander")) Say("hit", "deputy_commander");
                        HeardDestroyed(e.target);
                        OwnReport(BatteryFire::Event::Kind::Hit, e.target);
                        break;
                    case EngagementEvent::Kind::Miss:
                        if (NpcAt("deputy_commander")) Say("miss", "deputy_commander");
                        OwnReport(BatteryFire::Event::Kind::Miss, e.target);
                        break;
                    case EngagementEvent::Kind::Track: break;  // the capture and TV-track reports are automatic (inputs)
                }
            }
        }
        // Until something is tracked the senior operator keeps the crew up to date: the nearest echo on his scope,
        // every 15 s (the first report of each contact is spoken when it appears).
        if (engagement_on_ && NpcAt("senior_operator") && engagement_.Method() == TrackMethod::None && time_ >= next_contact_update_s_) {
            const KnownTarget* nearest = nullptr;
            for (const KnownTarget& k : pictures_["senior_operator"].All()) {
                if (time_ - k.time_s > 2.0 || !k.has_range || k.friendly) continue;
                if (nearest == nullptr || k.range_m < nearest->range_m) nearest = &k;
            }
            if (nearest != nullptr) {
                KnownTarget k = *nearest;
                char r[16];
                std::snprintf(r, sizeof(r), "%.0f", k.range_m / 1000.0);
                Say("contact", "senior_operator", {{"azimuth", MilsText(k.azimuth_deg)}, {"range_km", r}}, &k);
                next_contact_update_s_ = time_ + 15.0;
            }
        }
        // Designations the commander heard on the radio, passed on to the crew: a new target, the one he has ordered
        // taken, or one not passed on for 30 s (sam3d: every update of every target, four targets every 12 s, would
        // keep the intercom talking without end and the orders waiting behind it). A target another launcher has, once.
        if (c2_) {
            for (const C2Network::Event& e : designations_) {
                const auto last = passed_at_.find(e.subject);
                const bool fresh = last == passed_at_.end();
                const bool ours = e.subject == assigned_target_;
                const bool theirs = battery_fire_ && battery_fire_->Engaged(e.subject);
                const bool stale = !fresh && time_ - last->second >= 30.0;
                if (!(fresh || ours || (stale && !theirs))) continue;
                passed_at_[e.subject] = time_;
                if (NpcAt(Cmdr())) {
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
                    Say("designation", Cmdr(), {{"number", n}, {"azimuth", az}, {"range_km", r}, {"height_hm", h}}, &k);
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
                Say("remind_order", Cmdr(), {{"seat", SeatWord(seat, false)}, {"seat_en", SeatWord(seat, true)}});
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

    // ---- The battery's other launchers and the battery commander's fire distribution (roadmap 8.9, battery_fire.hpp).
    std::unique_ptr<BatteryFire> battery_fire_;
    std::mt19937 battery_fire_rng_{4099u};
    double next_distribution_s_ = 0.0;

    void UpdateBatteryFire() {
        if (LinkedToPost()) return;  // the battery's other launchers are the linked post's
        if (!engagement_on_ && !post_) return;
        if (!battery_fire_) {
            // The launchers' own tuning and reach when the player's vehicle is the post (battery.launchers), else ours.
            const EngagementTuning& tuning = def_.has_launcher_tuning ? def_.launcher_tuning : def_.tuning;
            double reach = def_.has_launcher_tuning ? def_.launcher_reach_m : console_.track.max_range_m;
            if (!(reach > 0.0)) reach = 28000.0;
            battery_fire_ = std::make_unique<BatteryFire>(tuning, reach);
            battery_fire_->SetAutonomous(def_.battery_commander == "none" || (!c2_ && !post_));
            battery_fire_rng_.seed(def_.seed * 4099u + 7u);
        }
        battery_fire_->Sync(*battery_);
        for (const std::string& c : external_launchers_) battery_fire_->SetExternal(c);
        ReportReadiness();
        // What the other vehicles see (geometric sensors), with each target's number on the board: the post's own when
        // the player's vehicle is the post, else what the commander has been told.
        std::vector<BatteryFireTarget> targets;
        std::vector<BoardMark> marks = post_ ? PostMarks() : c2_ ? c2_->Known(time_) : std::vector<BoardMark>();
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            if (!targets_[i].spawned || targets_[i].gone) continue;
            const Aircraft* a = world_.FindAircraft(targets_[i].world_id);
            if (a == nullptr || !a->alive) continue;
            BatteryFireTarget t;
            t.id = def_.targets[i].id;
            t.position = a->position;
            t.velocity = a->velocity;
            t.agl_m = world_.Frame().AltitudeOf(a->position) - world_.GetTerrain().HeightAt(a->position.x, a->position.y);
            t.target_class = def_.targets[i].target_class;
            t.friendly = def_.targets[i].friendly;
            for (const BoardMark& m : marks) {
                if (m.target_id == t.id) t.number = m.number;
            }
            targets.push_back(t);
        }
        // The NPC battery commander: each hostile on the board that nobody has goes to the nearest ready launcher that
        // can reach it soon (our own vehicle's target is ours).
        // In the post, he does it from his board by voice on the battery net, and only while he is an NPC and the net is up.
        const bool distribute = post_ ? NpcAt(Cmdr()) && BatteryNetUp() : c2_ != nullptr;
        if (def_.battery_commander == "npc" && distribute && time_ >= next_distribution_s_) {
            next_distribution_s_ = time_ + 5.0;
            for (const BoardMark& m : marks) {
                if (m.type == 1 || battery_fire_->Engaged(m.target_id) || m.target_id == assigned_target_ ||
                    m.target_id == engagement_.TrackedTarget() || DesignationPending(m.target_id)) {
                    continue;
                }
                bool alive = false;
                if (post_) {
                    alive = reported_kills_.count(m.target_id) == 0;  // he knows only what he was told
                } else {
                    for (const BatteryFireTarget& t : targets) alive = alive || t.id == m.target_id;
                }
                if (!alive) continue;
                // The nearest ready launcher whose sector of fire covers it; any ready launcher if none does.
                const BatteryFire::Shooter* best = nullptr;
                double best_r = battery_fire_->ReachM() + 10000.0;
                for (int pass = 0; pass < 2 && best == nullptr; ++pass) {
                    for (const BatteryFire::Shooter& s : battery_fire_->Shooters()) {
                        if (!s.deployed || s.missiles <= 0 || s.phase != BatteryFire::Phase::Idle || DesignationPendingFor(s.callsign)) continue;
                        if (pass == 0 && !BatteryFire::InSector(s, m.position)) continue;
                        const double r = (m.position - s.position).HorizontalLength();
                        if (r < best_r) {
                            best_r = r;
                            best = &s;
                        }
                    }
                }
                // Our own vehicle is a launcher of the battery too: when it is the nearest, the target is left to its crew
                // (the PU-12's designation by voice).
                const bool own_nearer = launcher_ != nullptr && launcher_->LoadedCount() > 0 &&
                                        (m.position - def_.unit_position).HorizontalLength() < best_r;
                if (best != nullptr && !own_nearer) {
                    if (post_) {
                        DesignateByVoice(best->callsign, m);
                    } else {
                        AssignBatteryTarget(best->callsign, m.target_id);
                    }
                }
            }
        }
        DeliverDesignations(targets);
        battery_fire_->Step(time_, targets, battery_fire_rng_, battery_.get(), [this](const std::string& id) {
            for (std::size_t i = 0; i < def_.targets.size(); ++i) {
                if (def_.targets[i].id != id) continue;
                if (Aircraft* a = world_.FindAircraft(targets_[i].world_id)) a->alive = false;
            }
        });
        // The launchers' reports go out on the battery net: the post hears them (and writes them on its IT boards) only
        // while its set on that net is up.
        for (const BatteryFire::Event& e : battery_fire_->TakeEvents()) {
            const BatteryFire::Shooter* sh = battery_fire_->Find(e.callsign);
            const int missiles = sh != nullptr ? sh->missiles : -1;
            LauncherReport({e.kind, e.callsign, e.target, e.number, missiles});
            if (sh != nullptr && !sh->external) net_reports_.push_back({e.kind, e.callsign, e.target, e.number, missiles});
        }
    }

public:
    // ---- The battery link (battery_link.hpp, docs/PU12_DEVELOPMENT.md P8): the player's Osa and the PU-12M in one
    // battery, each in its own runner. The Osa's runner keeps the targets' truth and the post's follows it; the post's
    // designations to the Osa reach its commander as the PU-12's; each side hears the other's reports on the battery net.
    struct LinkedDesignation {
        std::string callsign;
        BoardMark mark;      // where he told it to look (the IT-45's fix led on)
        double start_s = 0.0;
        double end_s = 0.0;  // the message ended
    };
    struct LinkReport {
        BatteryFire::Event::Kind kind = BatteryFire::Event::Kind::Ready;
        std::string callsign;
        std::string target;
        int number = 0;     // its number on the board (0: not known)
        int missiles = -1;  // missiles left (-1: not said)
    };
    // The battery commander sits in a linked PU-12M (scenario "battery_commander": "post").
    bool LinkedToPost() const { return def_.battery_commander == "post"; }

    // The post's side: a launcher of its battery is a linked Osa. Designations to it are handed to the link, its state
    // comes from its crew, and its reports from its commander.
    void SetExternalLauncher(const std::string& callsign) { external_launchers_.insert(callsign); }
    bool ExternalLauncher(const std::string& callsign) const { return external_launchers_.count(callsign) != 0; }
    bool SetExternalLauncherState(const std::string& callsign, int missiles, const std::string& target, bool tracking) {
        if (!battery_fire_) return false;
        const BatteryFire::Phase phase = target.empty() ? BatteryFire::Phase::Idle
                                         : tracking     ? BatteryFire::Phase::Tracking
                                                        : BatteryFire::Phase::Acquiring;
        return battery_fire_->SetExternalState(callsign, missiles, target, phase);
    }
    std::vector<LinkedDesignation> TakeLinkedDesignations() {
        std::vector<LinkedDesignation> out;
        out.swap(linked_out_);
        return out;
    }
    // Reports of the post's own (abstract) launchers on the battery net, for the linked Osa's crew to hear.
    std::vector<LinkReport> TakeNetReports() {
        std::vector<LinkReport> out;
        out.swap(net_reports_);
        return out;
    }
    bool PostBatteryNetUp() const { return BatteryNetUp(); }
    // The targets as another runner has them (the linked Osa's: the truth): positions, courses, who is destroyed.
    void MirrorTargets(const ScenarioRunner& truth) {
        for (std::size_t i = 0; i < targets_.size() && i < truth.targets_.size(); ++i) {
            TargetState& s = targets_[i];
            const TargetState& o = truth.targets_[i];
            if (!s.spawned || !o.spawned) continue;
            Aircraft* a = world_.FindAircraft(s.world_id);
            const Aircraft* b = truth.world_.FindAircraft(o.world_id);
            if (a == nullptr || b == nullptr) continue;
            a->position = b->position;
            a->velocity = b->velocity;
            a->speed_mps = b->speed_mps;
            a->next_waypoint = b->next_waypoint;
            if (!b->alive) a->alive = false;
            if (o.gone) s.gone = true;
        }
    }

    // The Osa's side: a designation the linked post read out to it (as the PU-12's, on the command net), whether the
    // post's net is on the air, the battery's reports it hears, its own commander's reports, a target another launcher
    // destroyed, and the target its crew is on (tracked, else ordered).
    bool RelayDesignation(const BoardMark& m, double start_s, double end_s) {
        return c2_ && c2_->Relay(def_.c2_policy.primary, m, start_s, end_s);
    }
    void SetPostNetUp(bool up) {
        if (c2_) c2_->SetFailed(def_.c2_policy.primary, !up, time_, "the post's battery net is off the air");
    }
    void HearBatteryReport(const LinkReport& r) { LauncherReport(r); }
    std::vector<LinkReport> TakeOwnReports() {
        std::vector<LinkReport> out;
        out.swap(own_reports_);
        return out;
    }
    bool DestroyTarget(const std::string& id, const std::string& by) {
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            if (def_.targets[i].id != id) continue;
            Aircraft* a = targets_[i].spawned ? world_.FindAircraft(targets_[i].world_id) : nullptr;
            if (a == nullptr || !a->alive) return false;
            a->alive = false;
            Event(id, "destroyed by " + by);
            return true;
        }
        return false;
    }
    std::string BatteryTarget() const {
        if (engagement_on_ && !engagement_.TrackedTarget().empty()) return engagement_.TrackedTarget();
        return assigned_target_;
    }
    bool Tracking() const { return engagement_on_ && !engagement_.TrackedTarget().empty(); }
    // Where a target is now (ENU; zero if it has not appeared).
    Vec3 TargetPosition(const std::string& id) const {
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            if (def_.targets[i].id != id || !targets_[i].spawned) continue;
            const Aircraft* a = world_.FindAircraft(targets_[i].world_id);
            return a != nullptr ? a->position : Vec3{};
        }
        return Vec3{};
    }
    // Is the target in the sky now (appeared, not destroyed, not past its last waypoint)?
    bool TargetAlive(const std::string& id) const {
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            if (def_.targets[i].id != id || !targets_[i].spawned || targets_[i].gone) continue;
            const Aircraft* a = world_.FindAircraft(targets_[i].world_id);
            return a != nullptr && a->alive;
        }
        return false;
    }

private:
    std::set<std::string> external_launchers_;
    std::vector<LinkedDesignation> linked_out_;
    std::vector<LinkReport> net_reports_;
    std::vector<LinkReport> own_reports_;
    bool own_ready_reported_ = false;
    bool own_empty_reported_ = false;
    std::map<std::string, std::string> given_elsewhere_;  // target -> the launcher the post gave it (heard on the net)
    std::string npc_released_;                            // the last such target his crew let go
    static constexpr double kBesideTheirsM = 2500.0;     // an echo this close to another launcher's target is it (sam3d)
    bool GivenElsewhere(const std::string& target) const {
        return given_elsewhere_.count(target) != 0 && reported_kills_.count(target) == 0;
    }

    // A launcher's report on the battery net (its own abstract crew's, a linked Osa's, or, in that Osa, the post's other
    // launchers'): the log, the words if the vehicle hears the net, the IT boards in the post.
    void LauncherReport(const LinkReport& r) {
        const bool hears = NetHears();
        char n[16];
        std::snprintf(n, sizeof(n), "%04d", r.number);
        const std::string number = r.number > 0 ? std::string(n) : r.target;
        switch (r.kind) {
            case BatteryFire::Event::Kind::Assigned:
                // Spoken by whoever assigned it; the linked Osa hears the post give another launcher a target.
                if (LinkedToPost() && r.callsign != OwnCallsign()) {
                    given_elsewhere_[r.target] = r.callsign;
                    if (hears && callouts_) callouts_->Say(time_, "order_assign", "battery", {{"callsign", r.callsign}, {"number", number}});
                }
                break;
            case BatteryFire::Event::Kind::Ready:
                Event(r.callsign, "ready for action");
                if (hears && callouts_) callouts_->Say(time_, "report_ready", "battery", {{"callsign", r.callsign}, {"missiles", std::to_string(r.missiles)}});
                if (hears) {
                    reported_deployed_[r.callsign] = true;
                    Status(r.callsign, r.missiles != 0 ? LauncherStatus::State::Ready : LauncherStatus::State::Empty, r.missiles, 0);
                }
                break;
            case BatteryFire::Event::Kind::Captured:
                Event(r.callsign, "captures " + r.target);
                if (hears) Status(r.callsign, LauncherStatus::State::Engaging, r.missiles, r.number);
                break;
            case BatteryFire::Event::Kind::Launch:
                Event(r.callsign, "launches at " + r.target);
                if (hears && callouts_) callouts_->Say(time_, "report_launch", "battery", {{"callsign", r.callsign}, {"number", number}});
                if (hears) Status(r.callsign, LauncherStatus::State::Launched, r.missiles, r.number);
                break;
            case BatteryFire::Event::Kind::Hit:
                Event(r.callsign, "destroys " + r.target);
                if (hears) {
                    HeardDestroyed(r.target);
                    if (callouts_) callouts_->Say(time_, "report_result", "battery", {{"callsign", r.callsign}, {"number", number}, {"spent", "1"}});
                    Status(r.callsign, r.missiles > 0 ? LauncherStatus::State::Ready : LauncherStatus::State::Empty, r.missiles, 0);
                }
                break;
            case BatteryFire::Event::Kind::Miss:
                Event(r.callsign, "misses " + r.target);
                if (hears) Status(r.callsign, LauncherStatus::State::Engaging, r.missiles, r.number);
                break;
            case BatteryFire::Event::Kind::Empty:
                Event(r.callsign, "out of missiles");
                if (hears && callouts_) callouts_->Say(time_, "request_tzm", "battery", {{"callsign", r.callsign}});
                if (hears) Status(r.callsign, LauncherStatus::State::Empty, 0, 0);
                break;
            case BatteryFire::Event::Kind::Reloaded:
                Event(r.callsign, "reloaded");
                if (hears) Status(r.callsign, LauncherStatus::State::Ready, r.missiles, 0);
                break;
        }
    }

    // The player's Osa in a battery whose commander sits in a linked PU-12M: its commander reports on the battery net as
    // the abstract launchers do (flak11: readiness, launch, result), and the link carries it to the post's IT boards.
    void OwnReport(BatteryFire::Event::Kind kind, const std::string& target) {
        if (!LinkedToPost()) return;
        LinkReport r;
        r.kind = kind;
        r.callsign = OwnCallsign();
        r.target = target;
        for (const C2Network::Event& e : designations_heard_) {
            if (e.subject == target) r.number = e.number;
        }
        r.missiles = launcher_ ? launcher_->LoadedCount() : -1;
        own_reports_.push_back(r);
        if (!callouts_ || !NpcAt(Cmdr())) return;
        char n[16];
        std::snprintf(n, sizeof(n), "%04d", r.number);
        const std::string number = r.number > 0 ? std::string(n) : target;
        if (kind == BatteryFire::Event::Kind::Ready) {
            callouts_->Say(time_, "report_ready", Cmdr(), {{"callsign", r.callsign}, {"channels", "2"}, {"missiles", std::to_string(r.missiles)}}, 3.0);
        } else if (kind == BatteryFire::Event::Kind::Launch) {
            callouts_->Say(time_, "report_launch", Cmdr(), {{"callsign", r.callsign}, {"number", number}}, 3.0);
        } else if (kind == BatteryFire::Event::Kind::Hit) {
            const auto spent = npc_launched_at_.find(target);
            callouts_->Say(time_, "report_result", Cmdr(),
                           {{"callsign", r.callsign}, {"number", number},
                            {"spent", std::to_string(spent != npc_launched_at_.end() ? spent->second : 1)}}, 3.0);
        }
    }
    // К бою готов once the vehicle is ready (the crew data's ready_halted) with missiles aboard, and again after a reload;
    // out of missiles when the last has gone and none is in flight.
    void UpdateOwnReports() {
        if (!LinkedToPost()) return;
        const int loaded = launcher_ ? launcher_->LoadedCount() : 1;
        bool ready = loaded > 0;
        if (def_.has_crew_data) {
            const CrewTask* t = def_.crew_data.FindTask("ready_halted");
            ready = ready && (t == nullptr || console_.equipment.AllHold(t->conditions, console_.panel));
        }
        if (ready && !own_ready_reported_) {
            own_ready_reported_ = true;
            OwnReport(BatteryFire::Event::Kind::Ready, std::string());
        }
        const bool empty = launcher_ != nullptr && loaded == 0 && (!engagement_on_ || !engagement_.ShotsInFlight());
        if (empty && !own_empty_reported_) {
            own_empty_reported_ = true;
            own_ready_reported_ = false;  // reloaded and ready, it reports again
            OwnReport(BatteryFire::Event::Kind::Empty, std::string());
        }
        if (!empty) own_empty_reported_ = false;
    }

private:

    // ---- The battery commander in the post (docs/PU12_DEVELOPMENT.md P4): designations by voice on the battery net,
    // the launchers' reports onto the IT-1 and IT-2 boards.
    struct VoiceDesignation {
        std::string callsign;
        BoardMark mark;
        Vec3 point;            // where he told it to look (the IT-45's fix led along its speed vector)
        double heard_s = 0.0;  // the launcher's commander has it (the message ended)
    };
    std::vector<VoiceDesignation> designations_out_;
    double battery_net_free_s_ = 0.0;
    std::vector<LauncherStatus> status_boards_;
    PostKeypad keypad_;
    double marker_x_ = 0.0;
    double marker_y_ = 0.0;
    int marked_ = -1;
    double marked_arrival_s_ = -1.0;
    std::map<std::string, bool> reported_deployed_;

    bool BatteryNetUp() const {
        return post_ && (console_.post_battery_net.empty() || console_.equipment.IsReady(console_.post_battery_net));
    }
    // Does the player's vehicle hear the battery net's reports? (Always, unless it is the post with its set down.)
    bool NetHears() const { return !post_ || BatteryNetUp(); }
    bool DesignationPending(const std::string& target) const {
        for (const VoiceDesignation& d : designations_out_) {
            if (d.mark.target_id == target) return true;
        }
        return false;
    }
    bool DesignationPendingFor(const std::string& callsign) const {
        for (const VoiceDesignation& d : designations_out_) {
            if (d.callsign == callsign) return true;
        }
        return false;
    }
    // The latest mark of each target on the post's board in the last two minutes, not one he heard destroyed.
    std::vector<BoardMark> PostMarks() const {
        std::vector<BoardMark> out;
        if (!post_) return out;
        for (const BoardMark& m : post_->Board().Marks()) {
            if (time_ - m.time_s > 120.0 || reported_kills_.count(m.target_id) != 0) continue;
            bool replaced = false;
            for (BoardMark& o : out) {
                if (o.target_id == m.target_id) {
                    if (m.time_s >= o.time_s) o = m;
                    replaced = true;
                }
            }
            if (!replaced) out.push_back(m);
        }
        return out;
    }
    // One designation read out on the battery net: number, azimuth and range from the launcher (flak11: by voice,
    // azimuth and range), one message at a time (the board's operator delay and message time).
    bool DesignateByVoice(const std::string& callsign, const BoardMark& m) {
        if (!battery_fire_ || !BatteryNetUp()) return false;
        const BatteryFire::Shooter* sh = battery_fire_->Find(callsign);
        if (sh == nullptr || sh->missiles <= 0) return false;
        const PlottingBoardDef& bd = post_->Board().Def();
        const double start = std::max(time_, battery_net_free_s_) + bd.operator_delay_s;
        battery_net_free_s_ = start + bd.message_s;
        // He reads the number off the board and the place off the IT-45: its latest fix, led along its speed vector to
        // when the launcher will have it (the tube shows the vector, ru_pu12_text). Not on the tube: the board's mark.
        BoardMark at = m;
        for (const AspdTrack& k : post_->Tracks()) {
            if (k.target_id == m.target_id) at.position = k.position + k.velocity * (battery_net_free_s_ - k.time_s);
        }
        designations_out_.push_back({callsign, m, at.position, battery_net_free_s_});
        if (ExternalLauncher(callsign)) linked_out_.push_back({callsign, at, start, battery_net_free_s_});
        const PlottingBoard::Designation d = PlottingBoard::Designate(at, sh->position);
        char n[16], r[16];
        std::snprintf(n, sizeof(n), "%04d", m.number);
        std::snprintf(r, sizeof(r), "%.0f", d.range_m / 1000.0);
        if (callouts_) {
            callouts_->Say(time_, "order_assign", Cmdr(), {{"callsign", callsign}, {"number", n}, {"azimuth", MilsText(d.azimuth_deg)}, {"range_km", r}});
        }
        log_.Record({time_, DebriefEntry::Kind::Scenario, "Battery commander", callsign,
                     std::string("target ") + n + " by voice: azimuth " + MilsText(d.azimuth_deg) + ", range " + r + " km"});
        return true;
    }
    // A designation heard: the launcher's crew looks where it was told and takes the nearest target there within
    // kDesignationSearchM (sam3d: the board's error is up to 5 degrees and 3 km, flak11). Off by much, it may be another
    // target, or none.
    static constexpr double kDesignationSearchM = 6000.0;
    void DeliverDesignations(const std::vector<BatteryFireTarget>& targets) {
        for (std::size_t i = 0; i < designations_out_.size();) {
            const VoiceDesignation d = designations_out_[i];
            if (d.heard_s > time_ + 1e-9) {
                ++i;
                continue;
            }
            designations_out_.erase(designations_out_.begin() + static_cast<std::ptrdiff_t>(i));
            if (ExternalLauncher(d.callsign)) {  // a linked Osa: its own crew looks for it (the link sets its state)
                battery_fire_->SetExternalState(d.callsign, -1, d.mark.target_id, BatteryFire::Phase::Acquiring);
                continue;
            }
            const BatteryFireTarget* best = nullptr;
            double best_d = kDesignationSearchM;
            for (const BatteryFireTarget& t : targets) {
                if (!t.alive || t.friendly) continue;
                const double dist = (Vec3{t.position.x, t.position.y, 0.0} - Vec3{d.point.x, d.point.y, 0.0}).Length();
                if (dist < best_d) {
                    best_d = dist;
                    best = &t;
                }
            }
            if (best == nullptr) {
                Event(d.callsign, "no target where it was designated");
                if (callouts_) callouts_->Say(time_, "report_no_target", "battery", {{"callsign", d.callsign}});
                continue;
            }
            if (best->id != d.mark.target_id) Event(d.callsign, "takes " + best->id + ", not the designated " + d.mark.target_id);
            battery_fire_->Assign(d.callsign, best->id, d.mark.number, time_);
        }
    }
    LauncherStatus& Status(const std::string& callsign, LauncherStatus::State state, int missiles, int target_number) {
        LauncherStatus* s = nullptr;
        for (LauncherStatus& e : status_boards_) {
            if (e.callsign == callsign) s = &e;
        }
        if (s == nullptr) {  // a new row, in the battery's order (21, 22, ...): the ДХ's launcher places count down it
            LauncherStatus row;
            row.callsign = callsign;
            const auto at = std::lower_bound(status_boards_.begin(), status_boards_.end(), row,
                                             [](const LauncherStatus& a, const LauncherStatus& b) { return a.callsign < b.callsign; });
            s = &*status_boards_.insert(at, row);
        }
        s->state = state;
        if (missiles >= 0) s->missiles = missiles;
        s->target_number = target_number;
        s->reported_s = time_;
        return *s;
    }
    // The launchers report on the battery net when they come into action (к бою готов, with their missiles) and when they
    // march off. The post hears it only while its set is up; a launcher reports again once the post can hear it.
    void ReportReadiness() {
        if (!post_ || !battery_fire_) return;
        const bool hears = NetHears();
        for (const BatteryFire::Shooter& s : battery_fire_->Shooters()) {
            if (s.external) continue;  // a linked Osa reports for itself
            bool& reported = reported_deployed_[s.callsign];
            if (s.deployed && !reported && hears) {
                reported = true;
                if (callouts_) callouts_->Say(time_, "report_ready", "battery", {{"callsign", s.callsign}, {"missiles", std::to_string(s.missiles)}});
                Status(s.callsign, s.missiles > 0 ? LauncherStatus::State::Ready : LauncherStatus::State::Empty, s.missiles, 0);
            } else if (!s.deployed && reported) {
                reported = false;
                if (hears) Status(s.callsign, LauncherStatus::State::March, s.missiles, 0);
            }
            if (!hears) reported = false;  // what it says while the post can't hear is lost: it reports again
        }
    }

public:
    // The battery commander (the player, or the NPC) gives one of the battery's other launchers a target on the battery
    // net. False without a battery, for an unknown launcher, or one with no missiles.
    // The battery commander sets a launcher's sector of fire (true azimuths, clockwise; from == to clears it).
    bool SetBatterySector(const std::string& callsign, double from_deg, double to_deg) {
        return battery_fire_ && battery_fire_->SetSector(callsign, from_deg, to_deg);
    }
    bool AssignBatteryTarget(const std::string& callsign, const std::string& target) {
        if (!battery_fire_) return false;
        int number = 0;
        if (c2_) {
            for (const BoardMark& m : c2_->Known(time_)) {
                if (m.target_id == target) number = m.number;
            }
        }
        if (!battery_fire_->Assign(callsign, target, number, time_)) return false;
        char n[16];
        std::snprintf(n, sizeof(n), "%04d", number);
        if (callouts_) callouts_->Say(time_, "order_assign", "battery", {{"callsign", callsign}, {"number", number > 0 ? std::string(n) : target}});
        log_.Record({time_, DebriefEntry::Kind::Scenario, "Battery commander", callsign, "target " + target});
        return true;
    }
    const BatteryFire* GetBatteryFire() const { return battery_fire_.get(); }
    // The battery commander in the post gives a launcher the target with this number on the post's board, by voice on the
    // battery net. False if the vehicle is no post, the net is down, the launcher is unknown or empty, or no target on
    // the board has the number.
    bool DesignateBatteryTarget(const std::string& callsign, int number) {
        if (!post_) return false;
        for (const BoardMark& m : PostMarks()) {
            if (m.number == number) return DesignateByVoice(callsign, m);
        }
        return false;
    }
    // The IT-1 and IT-2 boards: each launcher's state as the post last heard it.
    const std::vector<LauncherStatus>& StatusBoards() const { return status_boards_; }
    // The post's instruments in one block (post_feed.hpp): the IT-45 now, the board as the commander reads it (the
    // targets of PostBoard, each with its marks of the last five minutes: the same numbers he can give out), the IT
    // boards. Empty (post false) when the vehicle is no post.
    PostFeed PostFeedNow() const {
        PostFeed f;
        if (!post_) return f;
        f.post = true;
        f.scale_m = PostScaleM();
        f.position = post_->Position();
        f.scope = PostScope();
        const PlottingBoard& b = post_->Board();
        const std::vector<BoardMark> now = PostMarks();
        for (const BoardMark& m : b.Marks()) {
            bool current = false;
            for (const BoardMark& c : now) current = current || c.target_id == m.target_id;
            if (!current || m.stamp + 4 < b.Stamp()) continue;
            f.glass.push_back({m.number, m.stamp, m.height_hm, m.Code(), m.position});
        }
        f.launchers = status_boards_;
        f.marker_x = marker_x_;
        f.marker_y = marker_y_;
        f.marked = marked_;
        f.marked_arrival_s = marked_arrival_s_;
        const int target = KeypadTarget();
        if (target >= 0) {
            char n[8];
            std::snprintf(n, sizeof(n), "%02d", target % 100);
            f.keypad = n;
        }
        f.keypad_designating = keypad_.designating;
        f.keypad_last = keypad_.last;
        return f;
    }
    const PostKeypad& Keypad() const { return keypad_; }
    int KeypadTarget() const {
        if (!keypad_.entry.empty()) return std::atoi(keypad_.entry.c_str());
        return marked_;
    }
    // The DT-2 marker on the IT-45 (fractions of the scale, x east, y north), the target it is on (-1 none) and that
    // target's time of arrival at the post (s; -1 not closing).
    double MarkerX() const { return marker_x_; }
    double MarkerY() const { return marker_y_; }
    int MarkedTarget() const { return marked_; }
    double MarkedArrivalS() const { return marked_arrival_s_; }
    // The battery commander gives a launcher the target with this number on the IT-45 (the ASPD-U's): its board number
    // if the plotter has written it, else the tube's; by voice on the battery net as DesignateBatteryTarget.
    bool DesignateFromTube(const std::string& callsign, int aspd_number) {
        if (!post_) return false;
        const AspdTrack* track = nullptr;
        for (const AspdTrack& t : post_->Tracks()) {
            if (t.number == aspd_number) track = &t;
        }
        if (track == nullptr || reported_kills_.count(track->target_id) != 0) return false;
        for (const BoardMark& m : PostMarks()) {
            if (m.target_id == track->target_id) return DesignateByVoice(callsign, m);
        }
        BoardMark m;
        m.target_id = track->target_id;
        m.number = aspd_number;
        m.time_s = track->time_s;
        m.position = track->position;
        m.height_hm = static_cast<int>(std::lround(track->height_m / 100.0));
        m.type = track->type;
        return DesignateByVoice(callsign, m);
    }
    // A player's designation through his seat (multiplayer): only the commanding seat gives the launchers targets.
    bool SeatDesignate(const std::string& seat, const std::string& callsign, int number, std::string* why = nullptr) {
        if (seat != Cmdr()) {
            if (why) *why = "designations are the " + Cmdr() + "'s";
            return false;
        }
        const bool ok = DesignateBatteryTarget(callsign, number);
        if (!ok && why) *why = "the net is down, the launcher is unknown or empty, or the number is not on the board";
        return ok;
    }
    // A player's plot through his seat: only the plotter's.
    bool SeatPlot(const std::string& seat, int aspd_number, std::string* why = nullptr) {
        if (!post_ || seat != console_.post_plotter) {
            if (why) *why = "the board is the " + console_.post_plotter + "'s";
            return false;
        }
        const bool ok = PlayerPlot(aspd_number);
        if (!ok && why) *why = "not on the tube, or the last mark is still being written";
        return ok;
    }
    // The post's board as the commander reads it now: each target's latest mark of the last two minutes, the ones
    // reported destroyed gone. Empty when the vehicle is no post.
    std::vector<BoardMark> PostBoard() const { return PostMarks(); }

private:
    // ---- Knowledge per seat (roadmap 8.5, crew_knowledge.hpp): each seat's picture, from its own instruments and
    // from what it heard. The NPCs act on these, never on the truth.
    std::map<std::string, CrewPicture> pictures_;
    // Targets the crew knows are destroyed (their own "Цель поражена!", the battery net's "цель … поражена"): out of
    // every seat's picture, and a late designation of one is not passed on.
    std::set<std::string> reported_kills_;
    void HeardDestroyed(const std::string& target) {
        reported_kills_.insert(target);
        for (auto& kv : pictures_) kv.second.Forget(target);
        if (assigned_target_ == target) assigned_target_.clear();
    }
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
            const bool stake = s.to == c.Role() || c.Role() == Cmdr() || c.Role() == def_.tuning.aim_az_role ||
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
            for (const KnownTarget& k : pictures_[Cmdr()].Recent(time_, 30.0)) {
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
            const Contact* nearest = nullptr;
            for (const Contact& c : engagement_.SearchPicture()) {
                if (nearest == nullptr || c.range_m < nearest->range_m) nearest = &c;
            }
            if (nearest == nullptr) return false;  // nothing on his scope to report
            KnownTarget k;
            k.target = nearest->target_id;
            k.has_azimuth = k.has_range = true;
            k.azimuth_deg = nearest->azimuth_deg;
            k.range_m = nearest->range_m;
            char r[16];
            std::snprintf(r, sizeof(r), "%.0f", nearest->range_m / 1000.0);
            callouts_->Say(time_, callout_id, station, {{"azimuth", MilsText(nearest->azimuth_deg)}, {"range_km", r}}, 0.0, &k);
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
            if (crew_[i].Role() != Cmdr()) continue;
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
                Say("remind", Cmdr(), {{"seat", SeatWord(st.role, false)}, {"seat_en", SeatWord(st.role, true)}, {"what", what}});
            } else {
                Say("remind_order", Cmdr(), {{"seat", SeatWord(st.role, false)}, {"seat_en", SeatWord(st.role, true)}});
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
