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
#include <cctype>
#include <array>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simcore/board_reading.hpp"
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
#include "simcore/crew_mobility.hpp"
#include "simcore/drill_master.hpp"
#include "simcore/em/replication.hpp"
#include "simcore/pu12.hpp"
#include "simcore/scenario.hpp"
#include "simcore/seat_duty.hpp"
#include "simcore/terrain.hpp"
#include "simcore/voice_comms.hpp"
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
        kill_.Configure(def_.tuning.kill, def_.seed * 6007u + 3u);
        rwr_warned_.assign(def_.targets.size(), 0);
        fired_.assign(def_.triggers.size(), false);
        engagement_.SetUnitHeading(def_.unit_heading_deg);
        if (ready_state != nullptr) ApplyReadiness(*ready_state);
        for (const auto& c : def_.initial_controls) console_.panel.Set(c.first, c.second);
        if (console_.has_launcher) {
            launcher_ = std::make_unique<Launcher>(console_.launcher);
            for (const std::string& c : def_.empty_containers) launcher_->Unload(c);
        }
        if (console_.has_comms) comms_ = std::make_unique<CrewComms>(console_.comms);
        StartVoice();
        if (console_.has_fuel) fuel_ = std::make_unique<FuelModel>(console_.fuel, def_.fuel_fraction);
        if (def_.has_post) post_ = std::make_unique<CommandPost>(def_.post_aspd, def_.post_board, def_.unit_position, def_.seed ^ 0x51ed27u);
        StartRadios(ready_state != nullptr);
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
        StartMovers();
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
        UpdateVoice();
        UpdateOsaNet();
        UpdateIntercomCheck();
        UpdateBattery(dt);
        UpdateC2();
        UpdateRadios();
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
        UpdateMovers(dt);
        for (Seat& st : seats_) {
            st.tracker.Update(dt);  // a seat's min_wait_s counts as the single player's does
            PassSeatChecks(st);
        }
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
        UpdateOwedDuties();
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
    const SessionLog& Log() const { return log_; }
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
    // The post's R-111 and R-407 worked as sets (nullptr when the console has none).
    const R111* GetR111() const { return r111_.get(); }
    const R407* GetR407() const { return r407_.get(); }
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
    // The player at the plotter's seat writes on the board a report he heard on the air-picture net (its board number).
    // False if the vehicle is no post, the seat isn't the player's, no report waiting has the number, or he is still
    // writing the last one. The tube is the commander's: the plotter does not copy it.
    bool PlayerPlot(int number) {
        if (!post_ || SeatHolder(console_.post_plotter).empty()) return false;
        if (!post_->PlotHeard(time_, number, PlotterSpeed())) return false;
        ++plots_made_;
        return true;
    }
    // The player at the plotter's seat puts a mark where he clicks on the board (east, north: the board's point under his
    // click, ENU; metres_per_board_m: the board's scale): a report he heard (its board number
    // number). How near he came decides where it goes (the scenario's post.plotting, CommandPost::PlotAt).
    CommandPost::PlotResult PlayerPlotAt(int number, double east_m, double north_m, double metres_per_board_m) {
        CommandPost::PlotResult r;
        if (!post_ || SeatHolder(console_.post_plotter).empty()) return r;
        r = post_->PlotAt(time_, number, Vec3{east_m, north_m, 0.0}, metres_per_board_m, def_.post_plotting, PlotterSpeed());
        if (r.ok) {
            ++plots_made_;
            last_plot_ = r;
            char text[96];
            std::snprintf(text, sizeof(text), "%d plotted by hand: %.0f cm off, the mark %.0f cm from the right place", number,
                          100.0 * r.miss_board_m, 100.0 * r.error_board_m);
            Event("plotter", text);
        }
        return r;
    }
    const CommandPost::PlotResult& LastPlot() const { return last_plot_; }
    // Targets the player at the plotter's seat has written on the board so far.
    int PlotsMade() const { return plots_made_; }

    // The commander's order to a crew member. It goes by the R-124 intercom, so it arrives at once; with the commander's
    // box off, or the member's own box on the radio, it is shouted over the cabin noise and arrives after a delay
    // (roadmap 7.8). The same path serves the scenario's triggers and the player's order menu.
    void GiveOrder(CrewMember& crew, const Procedure& procedure, bool queue) {
        if (shadow_) {  // the shadow commander: a drill he would hand out, given by the player as an order (ShadowDrillOrders)
            ShadowDrill d{crew.Role(), procedure.id, std::string(), -1};
            if (procedure.steps.size() == 1) {  // one switch, no drill of its own (the readiness loop's loose orders)
                d.control = procedure.steps.front().control_id;
                d.position = procedure.steps.front().value;
            }
            shadow_drills_.push_back(d);
            return;
        }
        SayOrder(crew.Role(), procedure.id);
        // A man out of the vehicle is off the intercom: the order is called to him, and waits until he can hear it.
        const double delay = Outside(crew.Role()) ? ShoutS() : OrderDelayS(crew.Role());
        if (delay <= 0.0) {
            Hand(crew, procedure, queue);
            return;
        }
        pending_orders_.push_back({time_ + delay, crew.Name(), procedure.id, queue});
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
        const ControlDef* def = console_.panel.Find(control_id);
        seat_actions_.push_back({time_, def != nullptr ? def->station : std::string(), control_id, position, 0.0, false, Moved(r), r});
        if (!was_complete && player_.IsComplete()) {
            log_.Record({time_, DebriefEntry::Kind::Completed, kPlayerName, player_.GetProcedure().id, std::string()});
        }
        return r;
    }

    // Turn a hand wheel to a value. Wheel work isn't a procedure step (data/procedures/README.md).
    bool PlayerTurn(const std::string& wheel_id, double value) {
        const bool moved = console_.panel.SetValue(wheel_id, value);
        const ControlDef* def = console_.panel.Find(wheel_id);
        seat_actions_.push_back({time_, def != nullptr ? def->station : std::string(), wheel_id, 0, value, true, moved,
                                 StepResult::ProcedureComplete});
        return moved;
    }

    const ProcedureTracker& PlayerTracker() const { return player_; }
    // Radio and intercom traffic the crew heard, oldest first (the last kRadioKept): the command net (designations, source
    // changes), the battery net (the battery commander's orders) and orders shouted in the cabin.
    struct RadioMessage {
        double time_s = 0.0;
        std::string net;
        std::string from;
        std::string text;
        bool callout = false;  // a crew callout (the voice speaks it from the callout bus already)
        // The seats that heard it (docs/crew/12_COMMUNICATIONS.md): a player hears only what reaches his seat. Not routed:
        // no voice paths modelled, everyone.
        bool routed = false;
        std::vector<std::string> heard_by;
        bool HeardAt(const std::string& station) const {
            if (!routed) return true;
            for (const std::string& h : heard_by) {
                if (h == station) return true;
            }
            return false;
        }
    };
    static constexpr std::size_t kRadioKept = 64;
    const std::vector<RadioMessage>& RadioLog() const { return radio_; }

    // What the player's crew has in front of it besides its knowledge (roadmap 7.7): in the command post its own board's
    // marks of the last five plots; in a launcher only the designations its crew has heard on the net (the board is in
    // the post: first person); the last `radio_count` radio messages, and the battery's other vehicles and dummy positions.
    em::ViewExtras ViewExtrasNow(std::size_t radio_count = 8) const {
        em::ViewExtras x;
        x.valid = true;
        if (post_) {
            ToViewExtras(PostFeedNow(), x);  // the post's own board (killed targets wiped off) and instruments
        } else if (c2_ != nullptr || Board() != nullptr) {
            const std::vector<BoardMark> heard = c2_ != nullptr ? c2_->Known(time_) : Board()->Known(time_);
            int newest = 0;
            for (const BoardMark& m : heard) newest = std::max(newest, m.stamp);
            for (const BoardMark& m : heard) {
                if (m.stamp + 4 < newest || reported_kills_.count(m.target_id) != 0) continue;
                x.board.push_back({m.number, m.stamp, m.height_hm, m.Code(), m.position});
            }
        }
        const std::size_t from = radio_.size() > radio_count ? radio_.size() - radio_count : 0;
        for (std::size_t i = from; i < radio_.size(); ++i) {
            x.radio.push_back({radio_[i].time_s, radio_[i].net, radio_[i].from, radio_[i].text, radio_[i].callout, radio_[i].routed, radio_[i].heard_by});
        }
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
            if (const CrewMover* mv = Mover(m.Role())) a.away = mv->Place() != BodyPlace::Seated || mv->Moving();
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

    // ---- The crew out of their seats (docs/crew/13_OUT_OF_THE_VEHICLE.md, crew_mobility.hpp): each crewman's body over the
    // vehicle and the ground, out and in by the vehicle's hatches. Nothing of it without a vehicle file
    // (data/vehicles/<system>.json): the crew then stays in its seats as before.
    bool HasVehicle() const { return def_.has_vehicle; }
    const VehicleDef* Vehicle() const { return def_.has_vehicle ? &def_.vehicle : nullptr; }
    const WalkingDef& Walking() const { return def_.walking; }
    // The vehicle's body frame on the map: its origin on the ground under the unit's position, its heading.
    VehiclePose Pose() const {
        VehiclePose p;
        p.origin = Vec3{def_.unit_position.x, def_.unit_position.y,
                        world_.GetTerrain().HeightAt(def_.unit_position.x, def_.unit_position.y)};
        p.heading_rad = DegToRad(def_.unit_heading_deg);
        return p;
    }
    const CrewMover* Mover(const std::string& station) const {
        const auto it = movers_.find(station);
        return it != movers_.end() ? &it->second : nullptr;
    }
    // Every crewman's body now (seated ones too), in the scenario's crew order.
    std::vector<MoverView> CrewBodies() const {
        std::vector<MoverView> out;
        const VehiclePose pose = Pose();
        for (const CrewMember& c : crew_) {
            if (const CrewMover* m = Mover(c.Role())) out.push_back(m->View(pose));
        }
        return out;
    }
    // Out of the compartment (on the hull, past half way out of a hatch, on the ground): off the intercom, away from his
    // instruments, heard only by those in earshot.
    bool Outside(const std::string& station) const {
        const CrewMover* m = Mover(station);
        return m != nullptr && !m->Inside();
    }
    // The exits standing open.
    const std::map<std::string, bool>& Hatches() const { return hatches_; }
    // The place a control is worked at ("" inside, at its seat).
    std::string JobPlace(const std::string& control) const { return def_.has_vehicle ? def_.vehicle.JobPlace(control) : std::string(); }

    // The player's body (the engine): he walks himself and reports where he is (SetPlayerBody), or asks to be walked
    // along the vehicle's ways to a goal (PlayerGoTo: out through his hatch, down, to a place; or back to his seat). Once
    // the engine reports a seat's body, its jobs need him where they are done (PlayerJobRefusal).
    // `feet` in the vehicle's body frame (m), `facing_rad` clockwise from its forward, whatever the place; on the ground
    // he is at a place round the vehicle when within 1.5 m of where it is worked.
    void SetPlayerBody(const std::string& station, BodyPlace place, const Vec3& feet, double facing_rad) {
        auto it = movers_.find(station);
        if (it == movers_.end()) return;
        tracked_.insert(station);
        if (it->second.Moving()) return;  // being walked: the mover leads until he arrives
        if (place == BodyPlace::Seated) {
            it->second.SitDown();
            return;
        }
        if (place != BodyPlace::Ground) {
            it->second.Hold(place, feet, facing_rad);
            return;
        }
        const VehiclePose pose = Pose();
        Vec3 enu = pose.ToEnu(feet);
        enu.z = world_.GetTerrain().HeightAt(enu.x, enu.y);
        std::string at = "point";
        double best = 1.5;
        for (const VehiclePlace& p : def_.vehicle.places) {
            const double d = (Vec3{feet.x, feet.y, 0.0} - Vec3{p.at_m.x, p.at_m.y, 0.0}).Length();
            if (d <= best) {
                best = d;
                at = p.id;
            }
        }
        it->second.Hold(BodyPlace::Ground, enu, pose.ToMapHeading(facing_rad), at);
    }
    // A machine that mirrors the server steps the player's led walk itself (its runner isn't stepped).
    void StepMover(const std::string& station, double dt) {
        auto it = movers_.find(station);
        if (it == movers_.end()) return;
        const std::map<std::string, int> still_out = StillOut();
        it->second.Update(dt, Pose(), &world_.GetTerrain(), map_ground_.get(), def_.walking.gait, hatches_, &still_out);
    }
    bool PlayerGoTo(const std::string& station, const MoveGoal& goal, std::string* why = nullptr) {
        auto it = movers_.find(station);
        if (it == movers_.end()) {
            if (why != nullptr) *why = "no vehicle data";
            return false;
        }
        tracked_.insert(station);
        MoveGoal g = goal;
        if (g.kind == MoveGoal::Kind::Seat || g.kind == MoveGoal::Kind::Cabin) g.shut_after = true;  // unless a man is still out
        const bool ok = it->second.GoTo(g, &console_, Pose(), why);
        if (ok) Event(station, g.kind == MoveGoal::Kind::Seat ? std::string("back to his seat") : "on his way: " + it->second.At());
        return ok;
    }
    // He sat down at his seat himself (the engine's X).
    void PlayerSatDown(const std::string& station) {
        auto it = movers_.find(station);
        if (it != movers_.end()) it->second.SitDown();
    }
    // The engine stops reporting the seat's body (the player left it): back to the NPC's.
    void ReleasePlayerBody(const std::string& station) { tracked_.erase(station); }
    bool PlayerBodyTracked(const std::string& station) const { return tracked_.count(station) != 0; }
    // Why the player at `station` can't work `control` from where his body is ("" : he can): a job outside needs him at
    // its place; a control inside needs him inside. Only once the engine reports his body (headless play has none).
    std::string PlayerJobRefusal(const std::string& station, const std::string& control) const {
        if (!def_.has_vehicle || !PlayerBodyTracked(station)) return std::string();
        const CrewMover* m = Mover(station);
        if (m == nullptr) return std::string();
        const std::string place = def_.vehicle.JobPlace(control);
        if (!place.empty()) {
            if (m->Moving() || m->At() != place || m->Place() != BodyPlace::Ground) {
                const VehiclePlace* p = def_.vehicle.Place(place);
                return "That is done outside: " + (p != nullptr ? p->label : place);
            }
            return std::string();
        }
        if (!m->Inside()) return "You are outside the vehicle";
        return std::string();
    }

    // "К машинам!" (stroevoy_ustav_1993 art. 154): the NPC crew out to their places in the line-up beside the vehicle.
    // False (and why) without a line-up. "По местам!" (art. 149-150): back to their seats.
    bool OrderDismount(std::string* why = nullptr) {
        if (!def_.has_vehicle || !def_.vehicle.lineup.present) {
            if (why != nullptr) *why = "no line-up for this vehicle";
            return false;
        }
        Say("order_dismount", Cmdr());
        for (CrewMember& c : crew_) {  // they leave what they are doing and get out
            Vec3 at;
            if (!SeatHolder(c.Role()).empty() || !def_.vehicle.LineupPoint(c.Role(), at)) continue;
            c.CancelOrder();
            c.StayAt("lineup");
        }
        Event(Cmdr(), "К машинам: the crew lines up beside the vehicle");
        return true;
    }
    // The commander's order of one drill to a seat (a situational drill no order of the crew data starts: measure_qc).
    bool GiveDrill(const std::string& station, const std::string& procedure_id, bool queue = false) {
        CrewMember* m = MemberAt(station);
        const Procedure* p = console_.FindProcedure(procedure_id);
        if (m == nullptr || p == nullptr) return false;
        GiveOrder(*m, *p, queue);
        return true;
    }
    bool OrderMount() {
        if (!def_.has_vehicle) return false;
        Say("order_mount", Cmdr());
        for (CrewMember& c : crew_) c.StayAt(std::string());
        Event(Cmdr(), "По местам: the crew mounts");
        return true;
    }
    // Can `to` hear `from` speaking aloud (not on the intercom)? Both inside: yes (the compartment). Otherwise by the
    // distance between them, the noise where `to` is (the machinery running near, or the compartment's own) and what
    // the hull takes off (less through an open hatch near the man outside): crew_mobility.hpp, data/crew/walking.json.
    bool InEarshot(const std::string& from, const std::string& to, bool shout = true) const {
        if (!def_.has_vehicle) return true;
        const CrewMover* a = Mover(from);
        const CrewMover* b = Mover(to);
        const bool a_in = a == nullptr || a->Inside();
        const bool b_in = b == nullptr || b->Inside();
        if (a_in && b_in) return true;
        const VehiclePose pose = Pose();
        const Vec3 up{0.0, 0.0, 1.55};
        const Vec3 pa = (a != nullptr ? a->View(pose).feet_enu : pose.origin) + up;
        const Vec3 pb = (b != nullptr ? b->View(pose).feet_enu : pose.origin) + up;
        const EarshotDef& e = def_.walking.earshot;
        double level = shout ? e.shout_db : e.talk_db;
        if (a_in != b_in) {  // through the hull, or an open hatch near the man outside
            const Vec3 out = a_in ? pb : pa;
            double loss = e.hull_db;
            for (const VehicleExit& x : def_.vehicle.exits) {
                const auto h = hatches_.find(x.id);
                if (h == hatches_.end() || !h->second) continue;
                if ((pose.ToEnu(x.opening_m) - out).Length() < 6.0 && e.hatch_db < loss) loss = e.hatch_db;
            }
            level -= loss;
        }
        const double noise = b_in ? def_.vehicle.inside_noise_db : NoiseAt(pb, pose);
        return SpeechLevelAt(level, (pb - pa).Length()) - noise >= e.needed_snr_db;
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
        if (shadow_) return ShadowOrder(order_id);  // the shadow commander only thinks it: the player's to give
        // The commander's orders on the nets (docs/crew/12_COMMUNICATIONS.md): said on the air, not on the intercom.
        if (order_id == "order_battery_spare" || order_id == "order_battery_duty" || order_id == "order_battery_simplex" ||
            order_id == "order_warn_covered" || order_id == "order_all_clear_covered" || order_id == "order_report_regiment") {
            bool done = false;
            if (order_id == "order_battery_spare") done = OrderBatteryNetSpare("Battery commander");
            if (order_id == "order_battery_duty") done = OrderBatteryDuty(true, "Battery commander");
            if (order_id == "order_battery_simplex") done = OrderBatteryDuty(false, "Battery commander");
            if (order_id == "order_warn_covered") done = WarnCovered(Cmdr(), true);
            if (order_id == "order_all_clear_covered") done = WarnCovered(Cmdr(), false);
            if (order_id == "order_report_regiment") done = ReportToRegiment();
            if (done) orders_given_.push_back({time_, order_id});
            return done;
        }
        orders_given_.push_back({time_, order_id});
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
        // The orders on the nets: whoever must talk on the net can (his box on its set, the set transmitting).
        const auto net_of = [&]() -> std::string {
            if (order_id == "order_battery_spare" || order_id == "order_battery_duty" || order_id == "order_battery_simplex") return "battery";
            if (order_id == "order_warn_covered" || order_id == "order_all_clear_covered") return "covered";
            if (order_id == "order_report_regiment") return "regiment";
            return std::string();
        };
        const std::string net = net_of();
        if (!net.empty() && net != "covered" && MarchRadioBan()) {  // the warnings to the covered troops are open talk
            return refuse("на марше радио — только для управления огнём при отражении налёта",
                          "on the march the radio is for fire control only, while repelling an attack");
        }
        if (!net.empty()) {
            std::string why = CanTalkOnNet(Cmdr(), net);
            if (!why.empty() && net != "battery") {  // the squad leader or the radiotelephonist may talk for him
                const std::string other = net == "covered" ? "squad_leader" : "radio_operator";
                if (CanTalkOnNet(other, net).empty() && (MemberAtConst(other) != nullptr && SeatHolder(other).empty())) why.clear();
            }
            if (!why.empty()) return refuse("нет связи: " + why, "no way on the net: " + why);
            return out;
        }
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

    // ---- The helper (docs/crew/14_HELPER.md, simcore/helper.hpp): what the NPC at a seat a player holds would do now.
    // Every NPC act at a seat goes through NpcDoes: an NPC there does it; a player there owes it (a SeatDuty, kept until
    // he does it, it stops being due, or the moment passes). With the helper on, two more things are worked out for the
    // players' seats: where each hand wheel's NPC would bring it (the engagement's aim), and, when a player holds the
    // commanding seat, the NPC commander thinks in his place (the shadow commander: his orders become the player's
    // duties, and nothing he thinks reaches the crew or the world).
    void SetHelper(bool on) {
        helper_ = on;
        UpdateAimCrew();
    }
    bool HelperOn() const { return helper_; }
    // The shadow commander is thinking for the player in the commanding seat: where an NPC would command but for him (in a
    // scripted drill the scenario gives the orders, and he has none to give).
    bool ShadowCommanding() const { return helper_ && def_.has_crew_data && def_.npc_commander && drill_ && !SeatHolder(Cmdr()).empty(); }
    // The duties a seat owes now, oldest first.
    std::vector<SeatDuty> OwedDuties(const std::string& seat) const {
        std::vector<SeatDuty> out;
        for (const SeatDuty& d : owed_) {
            if (d.seat == seat) out.push_back(d);
        }
        return out;
    }
    const std::vector<SeatDuty>& AllOwedDuties() const { return owed_; }
    // Drills the shadow commander would have handed a seat that no order of the crew data starts ("seat: drill" -> when
    // last): the player in command has no way to give them (the coverage map lists them: a missing order in the data).
    const std::map<std::string, double>& ShadowUnorderable() const { return shadow_unorderable_; }
    // Where the seat's NPC would bring a hand wheel now (with the helper on): the engagement's aim at the target the crew
    // is on. False when it would leave it where it is.
    bool WheelWant(const std::string& wheel, double& value) const { return engagement_on_ && engagement_.WheelWanted(wheel, value); }
    // The next step of a drill a seat owes (SeatDuty::Kind::Drill): the first whose switch isn't where the step puts it,
    // or whose button that seat hasn't pressed since `since_s`. Null: all done.
    const ProcedureStep* DrillNextStep(const std::string& procedure, const std::string& seat, double since_s) const {
        const Procedure* p = console_.FindProcedure(procedure);
        if (p == nullptr) return nullptr;
        for (const ProcedureStep& st : p->steps) {
            const ControlDef* c = console_.panel.Find(st.control_id);
            if (c == nullptr || c->kind != ControlKind::Momentary) {
                if (console_.panel.Get(st.control_id) != st.value) return &st;
                continue;
            }
            bool pressed = false;
            for (auto it = seat_actions_.rbegin(); it != seat_actions_.rend() && it->time_s + 1e-9 >= since_s; ++it) {
                pressed = pressed || (it->station == seat && it->control == st.control_id && it->moved && it->position == st.value);
            }
            if (!pressed) return &st;
        }
        return nullptr;
    }
    // How long the player at a seat has done nothing, and the NPC's patience there (the hint level's wait).
    double SeatHesitation(const std::string& station) const { return Hesitation(station); }
    double SeatPatience(const std::string& station) const { return PatienceAt(station); }

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
        const std::string away = PlayerJobRefusal(role, control_id);  // his body isn't where the control is
        if (!away.empty()) {
            if (why != nullptr) *why = away;
            return StepResult::Blocked;
        }
        JudgeMove(role, control_id, position);
        st->last_action_s = time_;
        // A drill with only checks left (its switches already where it puts them) is done: the next one ordered is due.
        for (int guard = 0; guard < 8 && !st->tracker.IsComplete() && !st->queued.empty(); ++guard) {
            st->tracker.PassChecks(console_.panel, &console_.equipment, control_id, position);
            if (!st->tracker.IsComplete()) break;
            log_.Record({time_, DebriefEntry::Kind::Completed, st->player, st->tracker.GetProcedure().id, std::string()});
            const Procedure* next = console_.FindProcedure(st->queued.front());
            st->queued.erase(st->queued.begin());
            if (next != nullptr) st->tracker.Reset(*next);
        }
        const bool was_complete = st->tracker.IsComplete();
        const StepResult r = st->tracker.Attempt(console_.panel, control_id, position, &console_.equipment);
        log_.RecordAttempt(time_, st->player, st->tracker, r);
        seat_actions_.push_back({time_, role, control_id, position, 0.0, false, Moved(r), r});
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
        const bool moved = console_.panel.SetValue(wheel_id, value);
        seat_actions_.push_back({time_, role, wheel_id, 0, value, true, moved, StepResult::ProcedureComplete});
        return moved;
    }

    // Every move players made, at held seats or not, in order (a lesson reads the learner's: simcore/tutorial.hpp).
    struct SeatAction {
        double time_s = 0.0;
        std::string station;
        std::string control;
        int position = 0;       // a switch, selector, key, cover or button
        double value = 0.0;     // a wheel
        bool wheel = false;
        bool moved = false;     // the control moved (not blocked, not refused)
        StepResult result = StepResult::Accepted;
    };
    const std::vector<SeatAction>& SeatActions() const { return seat_actions_; }

    // Every order of the crew data given (GiveCrewOrder: the menu's, a lesson's, the scenario's), in order, spoken or not.
    struct GivenOrder {
        double time_s = 0.0;
        std::string order;
    };
    const std::vector<GivenOrder>& OrdersGiven() const { return orders_given_; }

    // Targets the battery commander has given the launchers so far (DesignateByVoice: the keys, the menu, the NPC).
    int DesignationsMade() const { return designations_made_; }
    // Targets on the post's board now (the plotter's latest marks).
    std::size_t PostMarkCount() const { return PostMarks().size(); }

    // Every report a player made (PlayerSay: the crew menu's), in order, when he made it: the intercom says it when it is
    // free.
    struct MadeReport {
        double time_s = 0.0;
        std::string station;
        std::string callout;
    };
    const std::vector<MadeReport>& ReportsMade() const { return reports_made_; }
    // What a player at this seat says on the air from the crew menu besides orders (listed after them): the Osa
    // commander's «к бою готов» and his read-back of a target the battery commander gave him.
    std::vector<std::string> PlayerRadioReports(const std::string& station) const {
        std::vector<std::string> out;
        if (!c2_ || station != Cmdr() || !def_.has_crew_data) return out;
        for (const char* id : {"report_ready", "readback"}) {
            if (def_.crew_data.FindCallout(id) != nullptr) out.push_back(id);
        }
        return out;
    }
    // Targets given this vehicle on the battery net that its commander heard (the PU-12's designations), so far.
    int DesignationsHeard() const { return static_cast<int>(designations_heard_.size()); }
    // The crew's results judged from the instruments' signs (kill_assessment.hpp), the wrong ones among them, and the
    // targets the senior operator saw flying on after they were called destroyed.
    int KillCalls() const { return kill_calls_; }
    int KillCallsWrong() const { return kill_calls_wrong_; }
    int StillFlyingCalls() const { return still_flying_; }

    // A target held "on_cue" appears now (a lesson brings it in at the learner's pace). False if there is no such
    // target held.
    bool ReleaseTarget(const std::string& target_id) {
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            if (def_.targets[i].id != target_id || targets_[i].spawned || !def_.targets[i].on_cue) continue;
            def_.targets[i].on_cue = false;
            def_.targets[i].appear_at_s = time_;
            return true;
        }
        return false;
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
        if (shadow_) return;  // the shadow commander's thoughts are not events
        log_.Record({time_, DebriefEntry::Kind::Scenario, std::string(), what, detail});
    }

    void UpdateTargets(double dt) {
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            const ScenarioTarget& t = def_.targets[i];
            if (targets_[i].spawned || t.on_cue || time_ + 1e-9 < t.appear_at_s) continue;
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

    // The guidance channels in the EM layer (roadmap E5): each channel's command transmitter is on from ВНИМАНИЕ but
    // works into its equivalent load, for concealment, until its missile is launched (voenizdat_osa_1991 p21, p140,
    // p155: the launch switches АНТЕННА — ЭКВИВАЛЕНТ to the antenna); then it radiates on its letter's frequency, along
    // the tracking antenna's line of sight. Each missile in flight carries its responder.
    void UpdateGuidanceEm() {
        if (!em_link_ || !launcher_ || !engagement_on_) return;
        const double az = DegToRad(em_link_->AntennaAzimuthDeg());
        const double el = DegToRad(em_link_->AntennaElevationDeg());
        const Vec3 toward = engagement_.UnitPosition() + Vec3{std::sin(az) * std::cos(el), std::cos(az) * std::cos(el), std::sin(el)} * 10000.0;
        for (int ch = 1; ch <= 2; ++ch) {
            const int ts = launcher_->TransmitterState(ch);
            em_link_->SetCommandLink(ch, ts, launcher_->LetterChanges(), toward);
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
            // The battery commander reads the board (he is in the post): a track that will reach the edge of the
            // launchers' zone within their warm-up time raises readiness; a track with one cross, its range alone.
            const BoardReading* why = nullptr;
            bool close = false;
            const std::vector<BoardReading> board = CommanderBoard();
            for (const BoardReading& r : board) {
                const bool soon = def_.battery_tactics.warning_s > 0.0 && r.to_zone_s >= 0.0 && r.to_zone_s <= def_.battery_tactics.warning_s;
                const bool in_range = r.range_m <= def_.battery_tactics.threat_range_m;
                if ((soon || in_range) && (why == nullptr || r.range_m < why->range_m)) why = &r;
                close = close || soon || in_range;
            }
            if (close && !battery_->AirThreat() && why != nullptr) {
                char text[160];
                if (why->has_speed && why->to_zone_s >= 0.0) {
                    std::snprintf(text, sizeof(text), "%04d at %.0f km, %.0f m/s: at the zone in %.1f min", why->number, why->range_m / 1000.0,
                                  why->velocity_mps.HorizontalLength(), why->to_zone_s / 60.0);
                } else {
                    std::snprintf(text, sizeof(text), "%04d at %.0f km", why->number, why->range_m / 1000.0);
                }
                Event("readiness", std::string("from the board: ") + text);
                if (warned_s_.count(why->key) == 0) warned_s_[why->key] = time_;
            }
            battery_->SetAirThreat(close, time_);
            // The debrief: how much warning the battery had of each target it was warned of, at the zone's edge.
            for (std::size_t i = 0; i < targets_.size(); ++i) {
                const std::string& id = def_.targets[i].id;
                const auto w = warned_s_.find(id);
                if (w == warned_s_.end() || warning_told_.count(id) != 0 || !targets_[i].spawned || targets_[i].gone) continue;
                const Aircraft* a = world_.FindAircraft(targets_[i].world_id);
                if (a == nullptr || (a->position - def_.unit_position).HorizontalLength() > def_.battery_tactics.zone_edge_m) continue;
                warning_told_.insert(id);
                char text[96];
                std::snprintf(text, sizeof(text), "at the zone's edge %.1f min after the battery was warned from the board",
                              (time_ - w->second) / 60.0);
                Event(id, text);
            }
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
            if (MarchRadioBan() && e.who == "Battery commander") {
                // voenizdat_osa_1991 pp. 232-239: on the march the battery commander, at the column's head in the
                // PU-12M, signals by flags and lamps; the column's drivers and commanders see them.
                std::vector<std::string> seen;
                for (const char* st : {"driver", "commander", "battery_commander", "squad_leader"}) {
                    if (console_.FindStation(st) != nullptr) seen.push_back(st);
                }
                Radio(def_.battery_night ? "lamp signals" : "flag signals", e.who, e.text, false, seen);
                continue;
            }
            Radio("battery net", e.who, e.text);
            // The post speaking on its battery net (FuG-4): a strong transmitter beside the R-111 for the while.
            if (def_.has_post && e.who == "Battery commander") own_tx_until_["fug4"] = time_ + kOwnTransmissionS;
        }
    }

    // On the march the battery keeps radio silence but for fire control while repelling an attack (voenizdat_osa_1991
    // pp. 232-239: the commander signals by flags and lamps from the column's head). Repelling: a hostile known within
    // the range at which the column stops where it is to fight (battery tactics' short_halt_in_column_m, p254), or a
    // target being engaged.
    bool OnMarch() const {
        if (!battery_) return false;
        const Battery::Mode m = battery_->CurrentMode();
        return m == Battery::Mode::Forming || m == Battery::Mode::Marching;
    }
    bool RepellingAttack() const {
        const double reach = battery_ && battery_->Tactics().short_halt_in_column_m > 0.0 ? battery_->Tactics().short_halt_in_column_m : 20000.0;
        if (engagement_on_ && engagement_.Method() != TrackMethod::None) return true;
        if (post_) {
            for (const BoardMark& m : CommanderPicture()) {
                if (m.type != 1 && (m.position - def_.unit_position).HorizontalLength() <= reach) return true;
            }
            return false;
        }
        const auto it = pictures_.find(Cmdr());
        if (it == pictures_.end()) return false;
        for (const KnownTarget& k : it->second.Recent(time_, 30.0)) {
            if (k.has_range && !k.friendly && k.range_m <= reach) return true;
        }
        return false;
    }
    bool MarchRadioBan() const { return OnMarch() && !RepellingAttack(); }

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
            if (const CrewMember* to = Crew(o.crew)) {  // out of the vehicle and out of earshot: it waits until he can hear
                if (Outside(to->Role()) && !InEarshot(Cmdr(), to->Role())) {
                    ++i;
                    continue;
                }
            }
            pending_orders_.erase(pending_orders_.begin() + static_cast<std::ptrdiff_t>(i));
            // By id: the order's procedure may have been a copy (a drill master's plan) that is gone by now.
            const Procedure* pr = console_.FindProcedure(o.procedure);
            if (CrewMember* c = Crew(o.crew)) {
                if (pr != nullptr) Hand(*c, *pr, o.queue);
            }
        }
    }

    // ---- The post's R-111 and R-407 worked as sets (radio_sets.hpp, docs/research/PU12_RADIOS_2026-10.md).
    std::unique_ptr<R111> r111_;
    std::unique_ptr<R407> r407_;
    struct MessageState {
        double next_s = 0.0;
        int said = 0;
        bool done = false;
    };
    std::vector<MessageState> messages_;
    std::map<std::string, double> own_tx_until_;  // the post's own sets on the air: id -> until when
    double r111_call_until_ = -1.0;
    double r111_tx_until_ = -1.0;
    double r407_call_until_ = -1.0;
    bool r407_link_ = false;
    static constexpr double kOwnTransmissionS = 5.0;  // sam3d: one spoken order or receipt on the air

    void SetRadioInput(const std::string& id, bool value) {
        if (!id.empty() && console_.equipment.GetInput(id) != value) console_.equipment.SetInput(id, value);
    }
    // A wheel behind a lock (the R-407's drums behind their СТОПОР): unlock, turn, lock again as it was.
    void SetLockedWheel(const std::string& wheel, const std::string& lock, double value) {
        const int was = console_.panel.Get(lock);
        if (!lock.empty()) console_.panel.Set(lock, 1);
        console_.panel.SetValue(wheel, value);
        if (!lock.empty() && was >= 0) console_.panel.Set(lock, was);
    }
    double RegimentDistanceM() const {
        if (!def_.post_regiment.has_position) return 0.0;
        const Vec3& r = def_.post_regiment.position;
        return (Vec3{r.x, r.y, 0.0} - Vec3{def_.unit_position.x, def_.unit_position.y, 0.0}).Length();
    }
    // The regiment's net now: nobody sending (an outage), or noise on it (an interference outage).
    void RegimentAir(bool& off_air, bool& interference) const {
        off_air = false;
        interference = false;
        for (const C2Outage& o : def_.post_regiment.outages) {
            if (time_ + 1e-9 < o.from_s || time_ >= o.to_s) continue;
            (o.interference ? interference : off_air) = true;
        }
    }

    // The sets as the session starts: the drums and the scale on the plan if they were set in the park; a set that
    // starts warm (a readiness state) is on preset 1 and tuned for the antenna it feeds.
    void StartRadios(bool warm) {
        if (!def_.has_radio_sets) return;
        const RadioPlan& plan = def_.radio_plan;
        if (console_.r111.present) {
            r111_ = std::make_unique<R111>(def_.radio_sets.r111);
            const double first = plan.presets_hz[0];
            if (plan.prepared && first > 0.0) {
                console_.panel.SetValue(console_.r111.Control("dial"), r111_->DialFor(first));
                console_.panel.Set(console_.r111.Control("subband"), first < def_.radio_sets.r111.subband_split_hz ? kR111SubbandI : kR111SubbandII);
            }
            const R111Inputs in = ReadR111(console_);
            if (warm && in.mode >= kR111Duty && plan.prepared && first > 0.0) r111_->Prepare(1, plan, in);
        }
        if (console_.r407.present) {
            r407_ = std::make_unique<R407>(def_.radio_sets.r407);
            if (plan.prepared && plan.telecode_hz > 0.0) {
                const double back = plan.telecode_return_hz > 0.0 ? plan.telecode_return_hz : plan.telecode_hz - def_.radio_sets.r407.duplex_offset_hz;
                SetLockedWheel(console_.r407.Control("rx_freq"), console_.r407.Control("rx_lock"), plan.telecode_hz / 1.0e6);
                SetLockedWheel(console_.r407.Control("tx_freq"), console_.r407.Control("tx_lock"), back / 1.0e6);
            }
        }
        messages_.assign(plan.messages.size(), MessageState{});
        for (std::size_t i = 0; i < plan.messages.size(); ++i) messages_[i].next_s = plan.messages[i].at_s;
        UpdateRadios();
    }

    void UpdateRadios() {
        if (!r111_ && !r407_) return;
        const RadioPlan& plan = def_.radio_plan;
        bool off_air = false, interference = false;
        RegimentAir(off_air, interference);
        const double distance = RegimentDistanceM();
        if (r111_) {
            const RadioSetBinding& b = console_.r111;
            const R111Inputs in = ReadR111(console_);
            std::vector<double> cosite;
            for (const auto& n : plan.own_nets_hz) {
                const auto it = own_tx_until_.find(n.first);
                if (it != own_tx_until_.end() && time_ < it->second) cosite.push_back(n.second);
            }
            const bool was_protected = r111_->Protected();
            r111_->Update(time_, in, plan, cosite);
            if (r111_->Protected() && !was_protected) Event("R-111", "ЗАЩИТА ВХОДА: a set beside it transmitting too near its frequency tripped the receiver's protection");
            if (r111_->DialToSet() >= 0.0) console_.panel.SetValue(b.Control("dial"), r111_->DialToSet());
            const double net = plan.command_net_hz > 0.0 ? plan.command_net_hz : plan.presets_hz[0];
            const bool on_air = !off_air && !interference;
            const bool on_box = !voice_ || !voice_->SetListeners("fug3").empty() || console_.voice.sets.empty();
            const bool talker = !voice_ || CanTalkOnNet("radio_operator", "regiment").empty() || CanTalkOnNet(Cmdr(), "regiment").empty() ||
                                def_.radio_data.Find("regiment") == nullptr;
            const bool hear = on_air && r111_->Hears(in, net, distance, plan.regiment_mast);
            const bool heard_back = on_air && r111_->Reaches(in, time_, net, distance, plan.regiment_mast);
            // The regiment's messages: heard when said, receipted when the post can answer, said again until it does.
            for (std::size_t i = 0; i < messages_.size(); ++i) {
                MessageState& m = messages_[i];
                if (m.done || time_ + 1e-9 < m.next_s) continue;
                const std::string& text = plan.messages[i].text;
                ++m.said;
                if (hear && on_box) {
                    Radio("regiment net", "Regiment", text);
                    r111_call_until_ = time_ + r111_->Def().call_s;
                    if (heard_back && talker) {
                        Transmitted("regiment", "Radiotelephone operator", 3.0);
                        r111_tx_until_ = time_ + kOwnTransmissionS;
                        m.done = true;
                        Event("regiment net", "heard and receipted: " + text);
                        continue;
                    }
                    Event("regiment net", "heard, but the post cannot answer (the R-111 not tuned, too little power, or out of reach): " + text);
                } else {
                    Event("regiment net", "not heard on the R-111: " + text);
                }
                if (m.said > plan.repeats) {
                    m.done = true;
                    Event("regiment net", "the regiment gave up: the message never got through: " + text);
                } else {
                    m.next_s = time_ + plan.repeat_s;
                }
            }
            if (in.call_pressed && r111_->CanTransmit(in, time_)) r111_tx_until_ = time_ + 2.0;
            const bool tx = in.mode >= kR111Power1 && (r111_->Tuning(time_) || time_ < r111_tx_until_);
            SetRadioInput(b.Input("net"), hear && heard_back);
            SetRadioInput(b.Input("hear"), hear);
            SetRadioInput(b.Input("tuning_um"), r111_->TuningUm(time_));
            SetRadioInput(b.Input("tuning_sau"), r111_->TuningSau(time_));
            SetRadioInput(b.Input("tuned"), r111_->UmTuned() && !r111_->Tuning(time_));
            SetRadioInput(b.Input("sau_tuned"), r111_->SauTuned(in.antenna_mast) && !r111_->Tuning(time_));
            SetRadioInput(b.Input("protect"), r111_->Protected());
            SetRadioInput(b.Input("rx"), in.mode >= kR111Duty && in.supply && !tx);
            SetRadioInput(b.Input("tx"), tx);
            SetRadioInput(b.Input("call_radio"), time_ < r111_call_until_);
            const char* windows[] = {"zpch_1", "zpch_2", "zpch_3", "zpch_4"};
            for (int k = 0; k < 4; ++k) SetRadioInput(b.Input(windows[k]), in.mode >= kR111Duty && in.supply && r111_->Preset() == k + 1);
        }
        if (r407_) {
            const RadioSetBinding& b = console_.r407;
            const R407Inputs in = ReadR407(console_);
            const double regiment_tx = plan.telecode_hz > 0.0 ? plan.telecode_hz : in.rx_mhz * 1.0e6;
            // The regiment not sending its telecode at all (peacetime: the ASPD-U left off) is the channel off the air.
            const bool link = r407_->Link(in, regiment_tx, distance, off_air || !def_.post_regiment.telecode, interference);
            if (link && !r407_link_) {
                r407_call_until_ = time_ + r407_->Def().call_s;  // the regiment calls the post up
                Event("R-407", "the regiment's telecode channel comes in");
            } else if (!link && r407_link_) {
                Event("R-407", interference && in.apch ? "the telecode channel lost: the АПЧ chases the interference" : "the telecode channel lost");
            }
            r407_link_ = link;
            SetRadioInput(b.Input("link"), link);
            const bool rk1 = console_.panel.Find("rk1.rx") == nullptr ||
                             (console_.panel.Get("rk1.rx") == 3 && (console_.panel.Find("rk1.code") == nullptr || console_.panel.Get("rk1.code") == 1));
            SetRadioInput(b.Input("telecode"), link && R407::ToEquipment(in) && rk1);
            SetRadioInput(b.Input("call_radio"), time_ < r407_call_until_);
            SetRadioInput(b.Input("call_wire"), false);
        }
    }

    // ---- The crew's voice paths and the battery's nets (docs/crew/12_COMMUNICATIONS.md; voice_comms.hpp, radio_nets.hpp).
    // A seat hears the intercom, a set or a line only through its headset and box (VoiceComms); a set hears its net only
    // on, warm, turned up, on the net's present frequency and not jammed out (RadioNets). Every message that reaches the
    // crew asks who hears it; whatever no one hears is lost.
    std::unique_ptr<VoiceComms> voice_;
    std::unique_ptr<RadioNets> nets_;
    std::map<std::string, double> jam_heard_s_;      // net -> when its jamming was first heard here (-1: recognised)
    std::map<std::string, double> jam_order_due_s_;  // a net this post runs -> when the commander takes it off the jammed frequency
    std::map<std::string, double> own_lost_s_;       // a net whose change the post missed -> when
    std::size_t next_silence_order_ = 0;
    struct LineState {
        std::size_t next_call = 0;
        bool ringing = false;
        double ring_from_s = 0.0;
        std::string text;                            // what the far end will say once answered
    };
    std::map<int, LineState> lines_;
    std::map<std::string, double> box_call_until_;   // an input -> lit until
    bool battery_duty_ = false;                      // the launchers on duty reception (the commander's order)
    double launchers_tx_from_s_ = 0.0;               // when their sets can transmit again (3 min after Д. ПРИЁМ)
    double covered_warned_s_ = -1.0;                 // the covered unit warned of this raid («Воздух»), and when
    double covered_due_s_ = -1.0;                    // a hostile came inside the warning range
    int readbacks_heard_ = 0;
    int readbacks_missed_ = 0;

    void StartVoice() {
        if (console_.has_voice) voice_ = std::make_unique<VoiceComms>(console_.voice);
        if (def_.radio_data.present) nets_ = std::make_unique<RadioNets>(def_.radio_data, def_.comms_tuning, def_.seed * 2654435761u + 11u);
        if (voice_) voice_->Update(console_);
    }
    const CommsTuning& Tuning() const { return def_.comms_tuning; }
    // A word across the cabin without the intercom: quick in a quiet cabin, slow over the turbine.
    double ShoutS() const {
        if (!voice_) return comms_ ? comms_->ShoutS(console_) : 1.5;
        for (const std::string& id : console_.voice.noisy) {
            if (console_.equipment.IsReady(id)) return console_.voice.shout_noisy_s;
        }
        return console_.voice.shout_quiet_s;
    }
    bool AnyOnRadio() const {
        if (!voice_) return false;
        for (const VoiceBoxDef& b : console_.voice.boxes) {
            if (voice_->OnBox(b.station) && !voice_->HearsIntercom(b.station)) return true;
        }
        return false;
    }
    // How a callout goes (CalloutBus::Router): the intercom when the speaker's box is on it (a moment to turn it there
    // if it is on a set); aloud in the cabin to a seat with no box, or from a seat without a headset (shouted); made with
    // the call when urgent or when its listener's box is on the radio (ВЫЗ, ЦИРКУЛЯР).
    CalloutBus::Route RouteCallout(const CalloutDef& def, const std::string& from) {
        CalloutBus::Route r;
        r.channel = def.kind == CalloutKind::Radio ? "battery net" : "intercom";
        if (def.kind == CalloutKind::Radio || !voice_ || console_.FindStation(from) == nullptr) return r;
        if (Outside(from)) {  // out of the vehicle: called out aloud
            r.channel = "cabin";
            r.extra_s = ShoutS();
            return r;
        }
        if (!def.to.empty() && voice_->Unboxed(def.to)) {
            r.channel = "cabin";
            return r;
        }
        if (!voice_->OnBox(from)) {
            r.channel = "cabin";
            r.extra_s = ShoutS();
            return r;
        }
        if (!voice_->TalksOnIntercom(from)) r.extra_s = Tuning().box_select_s;
        const bool to_seat = console_.FindStation(def.to) != nullptr;
        r.call = def.priority == 0 || (to_seat && !voice_->HearsIntercom(def.to)) || ((def.to == "crew" || def.to == "any") && AnyOnRadio());
        return r;
    }
    // Who heard a callout: the intercom's listeners (with the call, every box), everyone for the cabin, the net's for
    // the battery net. Empty: no voice paths modelled (everyone).
    std::vector<std::string> CalloutHearers(const SpokenCallout& s) const {
        std::vector<std::string> out;
        if (!voice_) return out;
        if (s.channel == "battery net") return BatteryNetHearers();
        for (const ConsoleStation& st : console_.stations) {
            if (HeardBy(s, st.id)) out.push_back(st.id);
        }
        return out;
    }
    // Did `station` hear a callout said on the intercom or aloud? The intercom only inside (a man out of the vehicle has
    // his headset off its box); aloud only within earshot (InEarshot: always so inside the compartment).
    bool HeardBy(const SpokenCallout& s, const std::string& station) const {
        if (station == s.from) return true;
        if (!voice_) return true;
        if (s.channel == "cabin") return InEarshot(s.from, station);
        if (Outside(station)) return false;
        return voice_->HearsIntercom(station) || (s.call && voice_->HearsCall(station));
    }

    // ---- the sets
    const RadioNetDef* NetOnSet(const std::string& set) const { return def_.radio_data.OnSet(set); }
    // The set a net is on: the radio data's, else the console's set whose own net it is.
    std::string SetOfNet(const std::string& net) const {
        if (const RadioNetDef* n = def_.radio_data.Find(net)) return n->set;
        for (const VoiceSetDef& s : console_.voice.sets) {
            if (s.net == net) return s.id;
        }
        return std::string();
    }
    const VoiceSetDef* SetDef(const std::string& set) const { return voice_ ? voice_->FindSet(set) : nullptr; }
    // The seat that works a set (turns its preset): the R-123M's preset control's, the R-111's preset buttons'.
    std::string AttendantOf(const VoiceSetDef& s) const {
        const std::string control = s.kind == "r111" ? console_.r111.Control("zpch_1") : s.preset;
        const ControlDef* c = console_.panel.Find(control);
        return c != nullptr ? c->station : std::string();
    }
    // The set is on, warm, turned up and on its net's present frequency (the jamming aside). The R-111 by its own model.
    bool SetTuned(const std::string& set, std::string* why = nullptr) const {
        const VoiceSetDef* s = SetDef(set);
        if (s == nullptr) return false;
        if (s->kind == "r111") {
            const bool hear = r111_ ? console_.equipment.GetInput(console_.r111.Input("hear"))
                                    : !console_.post_regiment_net.empty() && console_.equipment.IsReady(console_.post_regiment_net);
            if (!hear && why != nullptr) *why = "the R-111 is not on the regiment's net";
            return hear;
        }
        std::string reason;
        if (console_.panel.Get(s->power) != 1) {
            reason = s->name + " is switched off";
        } else if (!console_.equipment.IsReady(s->warm)) {
            reason = s->name + " is still warming up";
        } else if (!s->volume.empty() && console_.panel.Get(s->volume) != 1) {
            reason = s->name + " has its volume right down";
        } else if (nets_ != nullptr) {
            const RadioNetDef* n = NetOnSet(set);
            if (n != nullptr && console_.panel.Get(s->preset) + 1 != nets_->Current(n->id)) {
                reason = s->name + " is not on its net's frequency (preset " + std::to_string(console_.panel.Get(s->preset) + 1) +
                         ", the net on no. " + std::to_string(nets_->Current(n->id)) + ")";
            }
        }
        if (why != nullptr) *why = reason;
        return reason.empty();
    }
    // Tuned, and its net not jammed out here.
    bool SetHears(const std::string& set) const {
        if (!SetTuned(set)) return false;
        const RadioNetDef* n = NetOnSet(set);
        return nets_ == nullptr || n == nullptr || nets_->Jam(n->id, time_) < 2;
    }
    // Tuned and able to transmit: СИМПЛЕКС 3 min since Д. ПРИЁМ (the R-111: tuned and on a transmitting power).
    bool SetCanTransmit(const std::string& set, std::string* why = nullptr) const {
        const VoiceSetDef* s = SetDef(set);
        if (s == nullptr) return true;
        if (!SetTuned(set, why)) return false;
        if (s->kind == "r111") {
            const bool can = !r111_ || console_.equipment.GetInput(console_.r111.Input("net"));
            if (!can && why != nullptr) *why = "the R-111 cannot answer: not tuned to transmit, or on duty reception";
            return can;
        }
        if (console_.panel.Get(s->mode) != s->simplex_position) {
            if (why != nullptr) *why = s->name + " is on Д. ПРИЁМ: receive only";
            return false;
        }
        if (!console_.equipment.IsReady(s->transmit)) {
            if (why != nullptr) *why = s->name + "'s transmitter valves are still warming (3 min after Д. ПРИЁМ)";
            return false;
        }
        return true;
    }
    // The seats whose boxes are on a set that hears its net now.
    std::vector<std::string> NetHearers(const std::string& net) const {
        const std::string set = SetOfNet(net);
        if (!voice_ || set.empty() || !SetHears(set)) return {};
        return voice_->SetListeners(set);
    }
    // Who hears the battery net here: the post's seats on its set; in the Osa, the seats on the set the commander's
    // headset switch picks, while the network says he hears (empty with no voice paths: see NetHears).
    std::vector<std::string> BatteryNetHearers() const {
        if (!voice_) return {};
        if (post_) return NetHearers("battery");
        std::vector<std::string> out;
        if (c2_ && c2_->Hearing()) {
            const std::string set = c2_->ListeningSet() == 0 ? "left" : "right";
            out = voice_->SetListeners(set);
        }
        return out;
    }
    // Why a seat cannot talk on a net now ("" : it can). An NPC turns his box to the set himself; a player must have.
    std::string CanTalkOnNet(const std::string& station, const std::string& net) const {
        const std::string set = SetOfNet(net);
        if (!voice_ || set.empty()) return std::string();
        if (!voice_->TalksOnSet(station, set)) {
            std::string control;
            int position = 0;
            const bool npc = MemberAtConst(station) != nullptr && SeatHolder(station).empty();
            if (!(npc && voice_->HeadsetOn(station) && voice_->PositionFor(station, set, control, position))) {
                return voice_->WhyNot(station, set);
            }
        }
        std::string why;
        if (!SetCanTransmit(set, &why)) return why;
        return std::string();
    }
    const CrewMember* MemberAtConst(const std::string& station) const {
        for (const CrewMember& c : crew_) {
            if (c.Role() == station) return &c;
        }
        return nullptr;
    }
    // A transmission made: time on air (direction finding), and under radio silence on a net that is not exempt, a breach.
    void Transmitted(const std::string& net, const std::string& who, double seconds) {
        if (!nets_ || def_.radio_data.Find(net) == nullptr) return;
        if (nets_->Transmitted(net, time_, seconds)) {
            ++silence_breaches_;
            log_.Record({time_, DebriefEntry::Kind::Scenario, who, net, "transmitted under radio silence"});
        }
    }
    int silence_breaches_ = 0;
    // The radio log's word for a net.
    static std::string NetLabel(const std::string& net) {
        if (net == "air_picture") return "air picture";
        if (net == "regiment") return "regiment net";
        if (net == "battery") return "battery net";
        if (net == "covered") return "covered net";
        return net;
    }
    static std::string NetIdOf(const std::string& label) {
        if (label == "air picture") return "air_picture";
        if (label == "regiment net") return "regiment";
        if (label == "battery net") return "battery";
        if (label == "covered net") return "covered";
        return label;
    }
    // Who hears a line in the radio log (null: no voice paths, everyone).
    std::vector<std::string> HeardBy(const std::string& label) const {
        if (!voice_) return {};
        std::vector<std::string> out;
        if (label == "cabin" || label == "cabin (shouted)") {
            for (const ConsoleStation& st : console_.stations) out.push_back(st.id);
            return out;
        }
        if (label == "intercom") {
            for (const ConsoleStation& st : console_.stations) {
                if (voice_->HearsIntercom(st.id)) out.push_back(st.id);
            }
            return out;
        }
        if (label.compare(0, 5, "line ") == 0) {
            const int line = std::atoi(label.c_str() + 5);
            for (const ConsoleStation& st : console_.stations) {
                if (voice_->HearsLine(st.id, line)) out.push_back(st.id);
            }
            return out;
        }
        if (label == "battery net") return BatteryNetHearers();
        if (label == "command net") return post_ ? std::vector<std::string>() : BatteryNetHearers();
        return NetHearers(NetIdOf(label));
    }

    // ---- once a step
    void UpdateVoice() {
        if (voice_) voice_->Update(console_);
        UpdateBoxCalls();
        if (!nets_) return;
        for (const RadioNets::Due& d : nets_->Step(time_)) OnNetMoved(d);
        for (const RadioNets::Event& e : nets_->TakeEvents()) Event(e.net.empty() ? std::string("radio") : e.net, e.text);
        for (const VoiceSetDef& s : console_.voice.sets) {
            if (s.kind == "r123m") SetRadioInput(console_.equipment.HasInput(s.id + "_net") ? s.id + "_net" : std::string(), SetHears(s.id));
        }
        UpdateSilenceOrders();
        UpdateJamming();
        UpdateBatteryMembers();
        UpdateOwnNets();
        UpdateLines();
        UpdateCoveredWarning();
        UpdateRegimentReport();
    }

    // A net run by another main station moved: scheduled (silently: the NPC attendant turns the set at the time, a player
    // must keep the radio data's times himself) or ordered on air (heard only by whoever is on it).
    void OnNetMoved(const RadioNets::Due& d) {
        const RadioNetDef* n = def_.radio_data.Find(d.net);
        if (n == nullptr || n->main == "self") return;
        if (!d.ordered) {
            Event(d.net, "scheduled change to frequency no. " + std::to_string(d.to));
            RetuneOwn(d.net, d.to);
            return;
        }
        // The order is said on the old frequency: whoever hears the net now hears it (twice, as a circular command).
        const TalkEntry* e = def_.talk_tables.ByEffect("spare_frequency");
        const std::string group = e != nullptr ? e->code : std::string("переходите на запасную частоту");
        const std::string words = "Всем, я " + n->main_call_sign + ": " + group + ". " + group + ".";
        const RadioNetDef* nd = n;
        const int was = d.to == 1 ? 2 : 1;
        bool heard = false;
        if (voice_ && SetTunedTo(nd->set, d.net, was)) {
            heard = !voice_->SetListeners(nd->set).empty() && nets_->JamOn(d.net, was, time_) < 2 && nets_->GetsThrough(d.net, time_, true);
        }
        if (heard) {
            Radio(NetLabel(d.net), n->main_call_sign, words, false, voice_->SetListeners(nd->set));
            Event(d.net, "the main station orders frequency no. " + std::to_string(d.to) + ": heard");
            RetuneOwn(d.net, d.to);
        } else {
            own_lost_s_[d.net] = time_;
            Event(d.net, "the main station's order to change frequency was not heard here");
        }
    }
    // The set was on the given number of its net (before a move).
    bool SetTunedTo(const std::string& set, const std::string& net, int number) const {
        const VoiceSetDef* s = SetDef(set);
        if (s == nullptr) return false;
        if (s->kind == "r111") return r111_ && r111_->Preset() == number;
        (void)net;
        return console_.panel.Get(s->power) == 1 && console_.equipment.IsReady(s->warm) && console_.panel.Get(s->preset) + 1 == number;
    }
    // The set's attendant turns it to the net's number: an NPC by his drill. A player who heard it on his own box does it
    // himself; one who did not is told by the commander, who heard it on his (the R-111 on the RTK-1's second receiver).
    void RetuneOwn(const std::string& net, int number) {
        const RadioNetDef* n = def_.radio_data.Find(net);
        const VoiceSetDef* s = n != nullptr ? SetDef(n->set) : nullptr;
        if (s == nullptr || number > 2) return;
        const std::string pid = (s->kind == "r111" ? std::string("r111") : s->id) + (number == 1 ? "_working" : "_spare");
        const std::string attendant = AttendantOf(*s);
        const Procedure* pr = console_.FindProcedure(pid);
        CrewMember* c = MemberAt(attendant);
        if (pr == nullptr || c == nullptr) return;
        if (!NpcAt(attendant)) {
            if (voice_ && voice_->HearsSet(attendant, s->id)) {  // he heard it himself: his drill, unasked (the helper)
                SeatDuty d = SeatDuty::Drill(attendant, pid, "his net ordered to its other frequency: retune the set");
                d.keep_s = 120.0;
                NpcDoes(attendant, d);
            }
            if (attendant == Cmdr() || !NpcAt(Cmdr()) || !voice_ || voice_->HearsSet(attendant, s->id)) return;
            if (!voice_->HearsSet(Cmdr(), s->id)) return;
            GiveOrder(*c, *pr, true);  // «Р-111 на запасную!»: his words, the player's hand
            return;
        }
        const bool quiet = quiet_orders_;
        quiet_orders_ = true;
        GiveOrder(*c, *pr, true);
        quiet_orders_ = quiet;
    }
    // A net the post missed a change on: after the clock (rrs_1986: 5 min, both frequencies) its NPC attendant finds it.
    void UpdateOwnNets() {
        for (auto it = own_lost_s_.begin(); it != own_lost_s_.end();) {
            const RadioNetDef* n = def_.radio_data.Find(it->first);
            if (n == nullptr || SetTuned(n->set)) {
                it = own_lost_s_.erase(it);
                continue;
            }
            if (time_ - it->second >= Tuning().both_frequencies_s) {
                Event(it->first, "no contact for 5 min: the set tried on both frequencies");
                RetuneOwn(it->first, nets_->Current(it->first));
                it = own_lost_s_.erase(it);
                continue;
            }
            ++it;
        }
    }

    // The regiment's orders of radio silence (ПТ-1 on its command net): the regime is the regiment's; the post keeps it
    // when it hears the order (its NPCs put their sets on duty reception).
    void UpdateSilenceOrders() {
        const std::vector<RadioDataDef::SilenceOrder>& so = def_.radio_data.silence_orders;
        while (next_silence_order_ < so.size() && so[next_silence_order_].at_s <= time_ + 1e-9) {
            const bool on = so[next_silence_order_++].on;
            nets_->SetSilence(on, time_, "the regiment");
            const TalkEntry* e = def_.talk_tables.ByEffect(on ? "silence_on" : "silence_off");
            const RadioNetDef* r = def_.radio_data.Find("regiment");
            const std::vector<std::string> ears = NetHearers("regiment");
            if (r == nullptr || ears.empty()) {
                Event("regiment net", std::string("an order of radio silence ") + (on ? "" : "lifted ") + "was not heard");
                continue;
            }
            const std::string group = e != nullptr ? e->code : std::string(on ? "радиомолчание" : "радиомолчание отменить");
            Radio("regiment net", r->main_call_sign, "Всем, я " + r->main_call_sign + ": " + group + ". " + group + ".", false, ears);
            if (e != nullptr) HeardGroup(e->code, "regiment", ears);
            DecodeHeard(ears, e);
            ApplySilence(on);
        }
    }
    void ApplySilence(bool on) {
        for (const char* pid : {"silence_radio_operator", "silence_commander"}) {
            const std::string id = on ? std::string(pid) : std::string("silence_off_") + (std::string(pid).substr(8));
            const Procedure* pr = console_.FindProcedure(id);
            if (pr == nullptr) continue;
            const std::string seat = std::string(pid).substr(8) == "commander" ? Cmdr() : std::string("radio_operator");
            CrewMember* c = MemberAt(seat);
            if (c != nullptr && NpcDoes(seat, SeatDuty::Drill(seat, pr->id, on ? "radio silence ordered" : "radio silence lifted"))) {
                GiveOrder(*c, *pr, true);
            }
        }
    }
    // An NPC who heard a group of a talk table reads it against his table and tells the commander what it means.
    void DecodeHeard(const std::vector<std::string>& ears, const TalkEntry* e) {
        if (e == nullptr || !callouts_) return;
        std::string npc;
        for (const std::string& s : ears) {
            if (NpcAt(s) && s != Cmdr() && npc.empty()) npc = s;
        }
        if (npc.empty()) return;
        const TalkTable* t = nullptr;
        def_.talk_tables.Decode(e->code, &t);
        Say("decoded", npc, {{"table", t != nullptr ? t->name_ru : std::string("ПТ")}, {"code", e->code}, {"meaning", e->ru}, {"meaning_en", e->en}});
    }

    // Jamming: heard as noise by whoever is on the net's set; recognised after a trained listener's moment, said to the
    // commander, logged and reported (rrs_1986, pvo_bu_ix_1984). On a net this post runs, the commander then takes the
    // net to its spare frequency (an NPC after `order_spare_s`; a player by his order).
    void UpdateJamming() {
        for (const RadioNetDef& n : def_.radio_data.nets) {
            const int j = nets_->Jam(n.id, time_);
            auto it = jam_heard_s_.find(n.id);
            if (j == 0 || n.set.empty() || !SetTuned(n.set)) {
                if (it != jam_heard_s_.end() && j == 0) {
                    Event(NetLabel(n.id), "the jamming has stopped");
                    jam_heard_s_.erase(it);
                    jam_order_due_s_.erase(n.id);
                }
                continue;
            }
            const std::vector<std::string> ears = voice_ ? voice_->SetListeners(n.set) : std::vector<std::string>();
            if (it == jam_heard_s_.end()) {
                jam_heard_s_[n.id] = time_;
                if (!ears.empty()) Radio(NetLabel(n.id), "", j >= 2 ? "(помехи: шум забивает связь)" : "(помехи: шум в эфире)", false, ears);
                continue;
            }
            if (it->second < 0.0 || ears.empty() || time_ - it->second < Tuning().recognise_jamming_s) continue;
            it->second = -1.0;
            std::string who;
            for (const std::string& s : ears) {
                if (who.empty() && NpcAt(s)) who = s;
            }
            if (!who.empty() && who != Cmdr()) Say("net_jammed", who, {{"net", n.name_ru}, {"net_en", n.name_en}});
            if (who.empty()) {  // no NPC heard it: a player who did reports it himself
                for (const std::string& s : ears) {
                    if (s == Cmdr()) continue;
                    SeatDuty d = SeatDuty::Report(s, "net_jammed", "jamming recognised on a net", {{"net", n.name_ru}, {"net_en", n.name_en}});
                    d.keep_s = 60.0;
                    NpcDoes(s, d);
                }
            }
            log_.Record({time_, DebriefEntry::Kind::Scenario, who.empty() ? std::string("post") : SpeakerName(who), n.name_en,
                         j >= 2 ? "strong jamming recognised, logged and reported" : "jamming recognised, logged and reported"});
            if (n.main == "self") jam_order_due_s_[n.id] = time_ + Tuning().order_spare_s;
        }
        for (auto it = jam_order_due_s_.begin(); it != jam_order_due_s_.end();) {
            if (time_ < it->second) {
                ++it;
                continue;
            }
            const std::string net = it->first;
            it = jam_order_due_s_.erase(it);
            if (net == "battery" && nets_->Jam(net, time_) > 0 && !MarchRadioBan() && NpcDoes(Cmdr(), BatterySpareDuty())) {
                OrderBatteryNetSpare("Battery commander");
            }
        }
    }

    // ---- the battery net this post runs (its main station)
    std::string OwnCallSignOn(const std::string& net) const {
        const RadioNetDef* n = def_.radio_data.Find(net);
        return n != nullptr && !n->own_call_sign.empty() ? n->own_call_sign : std::string("10");
    }
    // The commander moves the battery net to its spare frequency (or back): a circular command said twice, no receipt
    // (rrs_1986). Each launcher that gets it turns its set and waits for his call; one that misses it is off the net
    // until its own clock finds it. The post's own FuG-4 is turned by the radiotelephonist (at his table).
    bool OrderBatteryNetSpare(const std::string& by) {
        const RadioNetDef* n = def_.radio_data.Find("battery");
        if (!post_ || !nets_ || n == nullptr || n->main != "self") return false;
        const int to = nets_->Current("battery") == 1 ? 2 : 1;
        if (to > nets_->Numbers("battery")) return false;
        const std::string why = CanTalkOnNet(Cmdr(), "battery");
        if (!why.empty()) {
            last_refusal_ = why;
            return false;
        }
        const TalkEntry* e = def_.talk_tables.ByEffect("spare_frequency");
        const std::string group = e != nullptr ? e->code : std::string("переходите на запасную частоту");
        Radio("battery net", by, "Всем, я " + n->own_call_sign + ": " + group + ". " + group + ".", false, BatteryNetHearers());
        Transmitted("battery", by, 4.0);
        if (battery_fire_) {
            for (const BatteryFire::Shooter& s : battery_fire_->Shooters()) {
                RadioNets::Member& m = nets_->MemberOf("battery", s.callsign);
                if (m.on != nets_->Current("battery")) continue;  // already off the net: it doesn't hear it
                if (nets_->GetsThrough("battery", time_, true)) {
                    m.on = to;
                    m.waiting = true;
                    m.lost_s = -1.0;
                } else if (nets_->Jam("battery", time_) > 0) {
                    // The order jammed out, but an Osa has two R-123M (mon_2112_80): a station with two receivers watches
                    // the spare frequency without being told (rrs_1986 art. 33), so it hears the main station there.
                    m.on = to;
                    m.waiting = true;
                    m.lost_s = -1.0;
                    Event(s.callsign, "missed the order in the jamming, but its second set watches the spare: it hears the post there");
                } else {
                    m.lost_s = time_;
                    Event(s.callsign, "did not hear the order to change frequency");
                }
            }
        }
        nets_->MoveTo("battery", to, time_, "the battery commander's order");
        battery_net_orders_.push_back(to);
        log_.Record({time_, DebriefEntry::Kind::Scenario, by, "battery net", "to frequency no. " + std::to_string(to)});
        const Procedure* pr = console_.FindProcedure(to == 1 ? "fug4_working" : "fug4_spare");
        CrewMember* ro = MemberAt("radio_operator");
        if (pr != nullptr && ro != nullptr) {
            GiveOrder(*ro, *pr, true);  // the commander's word to the radiotelephonist; a player there must do it himself
        } else {
            RetuneOwn("battery", to);
        }
        return true;
    }
    std::vector<int> battery_net_orders_;  // for a linked Osa (battery_link.hpp)
    std::string last_refusal_;
    // The launchers on the battery net: each one's clock after a missed order, the main station's call to those waiting
    // on the new frequency, and a launcher off the net (or jammed out) fighting on its own.
    void UpdateBatteryMembers() {
        const RadioNetDef* n = def_.radio_data.Find("battery");
        if (!post_ || !battery_fire_ || n == nullptr || n->main != "self") return;
        for (const BatteryFire::Shooter& s : battery_fire_->Shooters()) {
            if (s.external) continue;
            const std::string did = nets_->StepMember("battery", s.callsign, time_);
            if (!did.empty()) Event(s.callsign, did);
            RadioNets::Member& m = nets_->MemberOf("battery", s.callsign);
            if (m.waiting && !MarchRadioBan() && m.on == nets_->Current("battery") && SetTuned(n->set) && time_ - nets_->MovedS("battery") > 5.0 &&
                nets_->GetsThrough("battery", time_)) {
                m.waiting = false;
                Radio("battery net", n->own_call_sign, s.callsign + ", я " + n->own_call_sign + ", как слышите? Приём.", false, BatteryNetHearers());
                Radio("battery net", s.callsign, n->own_call_sign + ", я " + s.callsign + ", слышу хорошо. Приём.", false, BatteryNetHearers());
                Transmitted("battery", "Battery commander", 2.0);
            }
            const bool off = !nets_->MemberOnNet("battery", s.callsign) || nets_->Jam("battery", time_) >= 2;
            if (off != s.on_own) Event(s.callsign, off ? "off the battery net: searches and fires on its own" : "back on the battery net");
            battery_fire_->SetOnOwn(s.callsign, off);
        }
    }
    // The commander puts the launchers on duty reception (receive only) or back to transmit (3 min before they can).
    bool OrderBatteryDuty(bool duty, const std::string& by) {
        const std::string why = CanTalkOnNet(Cmdr(), "battery");
        if (!post_ || !nets_ || !why.empty()) {
            if (!why.empty()) last_refusal_ = why;
            return false;
        }
        if (duty == battery_duty_) return false;
        const TalkEntry* e = def_.talk_tables.ByEffect(duty ? "silence_on" : "silence_off");
        const std::string group = e != nullptr ? e->code : std::string(duty ? "дежурный приём" : "симплекс");
        Radio("battery net", by, "Всем, я " + OwnCallSignOn("battery") + ": " + group + ". " + group + ".", false, BatteryNetHearers());
        Transmitted("battery", by, 4.0);
        battery_duty_ = duty;
        if (!duty) launchers_tx_from_s_ = time_ + 180.0;  // r123m_training: 3 min for the transmitter valves
        log_.Record({time_, DebriefEntry::Kind::Scenario, by, "battery net", duty ? "the launchers to duty reception" : "the launchers back to simplex"});
        return true;
    }
    // Can a launcher answer on the battery net (read back, report)? Not on duty reception, nor in the 3 min after it.
    bool LaunchersCanTransmit() const { return !battery_duty_ && time_ >= launchers_tx_from_s_; }

    // ---- lines (the RTK-1's five) and the boxes' calls
    void UpdateLines() {
        for (const LineDef& l : def_.radio_data.lines) {
            if (!l.laid) continue;
            LineState& st = lines_[l.line];
            const std::string lamp = "line" + std::to_string(l.line) + "_call";
            if (!st.ringing && st.next_call < l.calls.size() && l.calls[st.next_call].at_s <= time_ + 1e-9) {
                st.ringing = true;
                st.ring_from_s = time_;
                st.text = l.calls[st.next_call++].text;
                Event("line " + std::to_string(l.line), l.to_name + " rings");
            }
            if (st.ringing) {
                // Answered: the commander's lever up with his headset on (an NPC commander raises it himself).
                if (voice_ && (!NpcAt(Cmdr()) || time_ - st.ring_from_s >= Tuning().answer_line_s) && !voice_->LineUp(Cmdr(), l.line) &&
                    NpcDoes(Cmdr(), SeatDuty::Answer(Cmdr(), l.line, "a line rings"))) {
                    const VoiceBoxDef* b = voice_->BoxOf(Cmdr());
                    if (b != nullptr && static_cast<std::size_t>(l.line) <= b->lines.size()) {
                        console_.panel.Set(b->lines[static_cast<std::size_t>(l.line - 1)], 1);
                        voice_->Update(console_);
                    }
                }
                if (voice_ && voice_->HearsLine(Cmdr(), l.line)) {
                    st.ringing = false;
                    Radio("line " + std::to_string(l.line), l.to_name, st.text, false, HeardBy("line " + std::to_string(l.line)));
                    log_.Record({time_, DebriefEntry::Kind::Scenario, "Battery commander", "line " + std::to_string(l.line), "answered: " + st.text});
                } else if (time_ - st.ring_from_s > 90.0) {
                    st.ringing = false;
                    log_.Record({time_, DebriefEntry::Kind::Scenario, l.to_name, "line " + std::to_string(l.line), "rang and was not answered"});
                }
            }
            SetRadioInput(console_.equipment.HasInput(lamp) ? lamp : std::string(), st.ringing);
        }
    }
    // A line laid to a launcher (for a designation without emission, past any jamming).
    const LineDef* LineToLauncher(const std::string& callsign) const { return def_.radio_data.LineTo(callsign); }
    void UpdateBoxCalls() {
        if (!voice_) return;
        struct Call {
            const char* button;
            const char* lamp;
        };
        static const Call calls[] = {{"rtk1.call_pvs1", "pvs1_call"}, {"rtk1.call_pvs2", "pvs2_call"}, {"rtk1.call_pvs3", "pvs3_call"},
                                     {"pvs1.call", "rtk1_call"}, {"pvs2.call", "rtk1_call"}, {"pvs3.call", "rtk1_call"}};
        for (const Call& c : calls) {
            if (console_.panel.Find(c.button) != nullptr && console_.panel.Get(c.button) == 1) box_call_until_[c.lamp] = time_ + 3.0;
        }
        if (console_.panel.Find("rtk1.circular") != nullptr && console_.panel.Get("rtk1.circular") == 1) {
            for (const char* lamp : {"pvs1_call", "pvs2_call", "pvs3_call"}) box_call_until_[lamp] = time_ + 3.0;
        }
        for (const char* lamp : {"pvs1_call", "pvs2_call", "pvs3_call", "rtk1_call"}) {
            const auto it = box_call_until_.find(lamp);
            SetRadioInput(console_.equipment.HasInput(lamp) ? std::string(lamp) : std::string(), it != box_call_until_.end() && time_ < it->second);
        }
    }

    // ---- the covered unit (FuG-2): warned of the air enemy («Воздух», ПТ-2) once a hostile comes inside the warning
    // range, the all clear («Отбой») once none is left (pvo_bu_x_1984: air defence warns the troops it covers). The NPC
    // squad leader gives it on the commander's order (an NPC commander's own, or a player's from the orders menu).
    void UpdateCoveredWarning() {
        if (!post_ || def_.radio_data.Find("covered") == nullptr) return;
        bool inside = false;
        for (const BoardMark& m : CommanderPicture()) {
            if (m.type == 1) continue;
            inside = inside || (m.position - def_.unit_position).HorizontalLength() <= Tuning().warn_covered_m;
        }
        if (inside && covered_due_s_ < 0.0) covered_due_s_ = time_;
        if (inside && covered_warned_s_ < 0.0 && time_ - covered_due_s_ >= 5.0 &&
            NpcDoes(Cmdr(), SeatDuty::Order(Cmdr(), "order_warn_covered", "a raid within reach of the covered troops"))) {
            WarnCovered(Cmdr(), true);
        }
        if (!inside && covered_warned_s_ >= 0.0 && covered_due_s_ >= 0.0 &&
            NpcDoes(Cmdr(), SeatDuty::Order(Cmdr(), "order_all_clear_covered", "the raid has gone: the all clear"))) {
            WarnCovered(Cmdr(), false);
        }
    }
    // The warning (or the all clear) goes out on FuG-2 from whoever can talk on it: the squad leader (his box on РСТ-2),
    // else the commander himself.
    bool WarnCovered(const std::string& by, bool warning) {
        const RadioNetDef* n = def_.radio_data.Find("covered");
        if (n == nullptr) return false;
        std::string talker;
        for (const std::string& s : {std::string("squad_leader"), Cmdr()}) {
            if (talker.empty() && CanTalkOnNet(s, "covered").empty() && (NpcAt(s) || voice_->TalksOnSet(s, n->set))) talker = s;
        }
        if (talker.empty()) {
            last_refusal_ = CanTalkOnNet(by, "covered");
            if (last_refusal_.empty()) last_refusal_ = "nobody can talk on the covered unit's net";
            return false;
        }
        const TalkEntry* e = def_.talk_tables.ByEffect(warning ? "air_warning" : "all_clear");
        const std::string group = e != nullptr ? e->code + " (" + e->ru + ")" : std::string(warning ? "Воздух" : "Отбой");
        Radio("covered net", SpeakerName(talker), n->main_call_sign + ", я " + n->own_call_sign + ": " + group + ". " + group + ".", false,
              NetHearers("covered"));
        Transmitted("covered", SpeakerName(talker), 4.0);
        char text[96];
        std::snprintf(text, sizeof(text), "%s, %.0f s after the first hostile came inside %.0f km", warning ? "warned («Воздух»)" : "all clear («Отбой»)",
                      covered_due_s_ >= 0.0 ? time_ - covered_due_s_ : 0.0, Tuning().warn_covered_m / 1000.0);
        log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(talker), "covered unit", text});
        if (warning) {
            covered_warned_s_ = time_;
        } else {
            covered_warned_s_ = -1.0;
            covered_due_s_ = -1.0;
        }
        return true;
    }

    // The battery ready, reported up to the regiment on its command net (voenizdat_osa_1991 p. 251: the battery commander
    // reports to the regiment's commander): by the commander on РСТ-3, or the radiotelephonist for him.
    bool regiment_reported_ = false;
    bool ReportToRegiment() {
        const RadioNetDef* rn = def_.radio_data.Find("regiment");
        if (!post_ || rn == nullptr) return false;
        std::string talker;
        for (const std::string& st : {Cmdr(), std::string("radio_operator")}) {
            if (talker.empty() && CanTalkOnNet(st, "regiment").empty() && (st == Cmdr() || NpcAt(st))) talker = st;
        }
        if (talker.empty()) {
            last_refusal_ = CanTalkOnNet(Cmdr(), "regiment");
            return false;
        }
        int ready = 0, missiles = 0;
        for (const LauncherStatus& l : status_boards_) {
            if (l.state != LauncherStatus::State::Empty) ++ready;
            missiles += l.missiles > 0 ? l.missiles : 0;
        }
        char text[160];
        std::snprintf(text, sizeof(text), "%s, я %s: к бою готова, пусковых %d, ракет %d. Приём.", rn->main_call_sign.c_str(),
                      rn->own_call_sign.c_str(), ready, missiles);
        Radio("regiment net", SpeakerName(talker), text, false, NetHearers("regiment"));
        Transmitted("regiment", SpeakerName(talker), 5.0);
        log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(talker), "regiment", text});
        regiment_reported_ = true;
        return true;
    }
    // The NPC commander reports once the post's communications are ready and every launcher has reported ready.
    void UpdateRegimentReport() {
        if (regiment_reported_ || !post_ || def_.radio_data.Find("regiment") == nullptr || MarchRadioBan()) return;
        if (!console_.equipment.IsReady("comms_ready") || status_boards_.empty()) return;
        for (const LauncherStatus& l : status_boards_) {
            if (l.state != LauncherStatus::State::Ready) return;
        }
        if (NpcDoes(Cmdr(), SeatDuty::Order(Cmdr(), "order_report_regiment", "communications and every launcher ready"))) ReportToRegiment();
    }

public:
    // ---- what the voice paths expose (the HUD, the orders menu, the tests)
    const VoiceComms* Voice() const { return voice_.get(); }
    const RadioNets* Nets() const { return nets_.get(); }
    const TalkTables& GetTalkTables() const { return def_.talk_tables; }
    const RadioDataDef& RadioData() const { return def_.radio_data; }
    int SilenceBreaches() const { return silence_breaches_; }
    int ReadbacksHeard() const { return readbacks_heard_; }
    int ReadbacksMissed() const { return readbacks_missed_; }
    double CoveredWarnedS() const { return covered_warned_s_; }
    const std::string& LastRefusal() const { return last_refusal_; }
    // Did a seat hear this callout (its headset, its box; everyone for a word in the cabin)? The voice plays only those.
    bool CalloutHeardAt(const SpokenCallout& s, const std::string& station) const {
        if (!voice_) return true;
        for (const std::string& h : CalloutHearers(s)) {
            if (h == station) return true;
        }
        return false;
    }
    // A set on, warm, turned up, on its net's present frequency and not jammed out (the boxes' РСТ lamps).
    bool SetHearsItsNet(const std::string& set) const { return voice_ != nullptr && SetHears(set); }
    // Can this seat hear a net now (its box on the net's set, the set on it)? For the HUD's comms strip.
    bool SeatHearsNet(const std::string& station, const std::string& net) const {
        for (const std::string& s : NetHearers(net)) {
            if (s == station) return true;
        }
        return false;
    }
    // The battery commander's orders on his own net (the orders menu): the spare frequency (or back), duty reception, and
    // the covered unit's warning. False with LastRefusal() saying why.
    bool CommanderOrderSpare() { return OrderBatteryNetSpare("Battery commander"); }
    bool CommanderOrderDuty(bool duty) { return OrderBatteryDuty(duty, "Battery commander"); }
    bool CommanderWarnCovered(bool warning) { return WarnCovered(Cmdr(), warning); }
    // A linked Osa (battery_link.hpp): the battery net's present number, and the orders to change it since the last call.
    int BatteryNetNumber() const { return nets_ ? nets_->Current("battery") : 1; }
    std::vector<int> TakeBatteryNetOrders() {
        std::vector<int> out;
        out.swap(battery_net_orders_);
        return out;
    }
    // The player at the radiotelephonist's seat (or any seat hearing the air picture) reads the oldest held report out to
    // the plotter. False if none is held, or his box isn't on the air-picture net.
    bool PlayerReadOutAirReport(const std::string& seat) {
        if (!post_ || !callouts_ || post_->Held().empty() || !SeatHearsNet(seat, "air_picture")) return false;
        const CommandPost::AirReport r = post_->Held().front();
        ReadOut(seat, r);
        return true;
    }

private:
    // A held report read out to the plotter (the callout "air_report", aloud beside him); heard, it goes to his hand.
    std::set<int> reading_out_;
    void ReadOut(const std::string& seat, const CommandPost::AirReport& r) {
        if (reading_out_.count(r.number) != 0) return;
        reading_out_.insert(r.number);
        KnownTarget k;
        k.target = r.contact.target_id;
        k.number = r.number;
        callouts_->Say(time_, "air_report", seat, {{"report", r.text}}, NpcAt(seat) ? SpeakDelayS(seat) : 0.0, &k);
    }
    void RelayAirPicture() {
        if (!post_ || !voice_ || !callouts_) return;
        for (const CommandPost::AirReport& r : post_->Held()) {
            if (reading_out_.count(r.number) != 0) continue;
            std::string reader;
            for (const std::string& s : NetHearers("air_picture")) {
                if (reader.empty() && NpcAt(s) && s != console_.post_plotter) reader = s;
            }
            if (!reader.empty()) {
                ReadOut(reader, r);
                continue;
            }
            for (const std::string& s : NetHearers("air_picture")) {  // a player who hears the net reads it out himself
                if (s == console_.post_plotter) continue;
                SeatDuty d = SeatDuty::Report(s, "air_report", "an air report heard: read it out to the plotter");
                d.number = r.number;
                NpcDoes(s, d);
            }
        }
    }

    // ---- the plotter's voice (pvo_bu_x_1984 art. 23): aloud in the cabin he reports to the commander the targets
    // appearing on the board (number, bearing and range from the post, height, identity and size), their actions (a
    // turn) and those lost. An NPC says each as it happens; a player's wait, filled in, in his reports menu until he says
    // them or they go stale (plot_stale_s). The commander decides on them (art. 17): the NPC commander takes a new board
    // target into his picture when the plotter calls it, or when he finds it on his next look over the board.
    struct PlotterEvent {
        std::string callout;  // plot_new, plot_turn, plot_lost
        std::string target_id;
        double at_s = 0.0;
        std::map<std::string, std::string> fields;
    };
    std::vector<PlotterEvent> plotter_events_;        // a player plotter's, not yet said
    std::set<std::string> plot_new_seen_, plot_lost_said_;
    std::map<std::string, double> plot_course_deg_;   // each target's course as last reported (or first seen)
    std::map<std::string, double> board_known_s_;     // when the commander was told of each board target
    int plot_reports_due_ = 0, plot_reports_made_ = 0, plot_reports_missed_ = 0;
    // The board's identity digit as the plotter says it (samsim_osa p20, bguir_tsp; 2 the training target by the project
    // owner's decision of 19 Oct 2026; docs/research/PLOTTING_BOARD_2026-10.md).
    static std::pair<std::string, std::string> IdentityWords(int type) {
        switch (type) {
            case 0: return {"постановщик помех", "jammer"};
            case 1: return {"свой", "friendly"};
            case 2: return {"учебная", "training target"};
            case 3: return {"нарушитель границы", "border violator"};
            case 4: return {"контрольный", "control aircraft"};
            case 5: return {"свой по запросу", "friendly on request"};
            case 6: return {"нарушитель режима", "breaking the flying rules"};
            case 8: return {"противник", "hostile"};
            case 9: return {"опознавания нет", "no IFF reply"};
            default: return {"индекс " + std::to_string(type), "index " + std::to_string(type)};
        }
    }
    std::map<std::string, std::string> PlotFields(const BoardMark& m) const {
        char n[8];
        std::snprintf(n, sizeof(n), "%04d", m.number);
        const Vec3 d = m.position - post_->Position();
        double az = RadToDeg(std::atan2(d.x, d.y));
        if (az < 0.0) az += 360.0;
        const std::pair<std::string, std::string> who = IdentityWords(m.type);
        const bool group = m.formation > 1;
        return {{"number", n},
                {"azimuth", std::to_string(static_cast<int>(std::lround(az)) % 360)},
                {"range", std::to_string(static_cast<int>(std::lround(d.HorizontalLength() / 1000.0)))},
                {"height", std::to_string(m.height_hm * 100)},
                {"identity", who.first},
                {"identity_en", who.second},
                {"size", group ? "группа " + std::to_string(m.formation) : std::string("одиночная")},
                {"size_en", group ? "a group of " + std::to_string(m.formation) : std::string("single")}};
    }
    void PlotterSays(const PlotterEvent& e) {
        const std::string& plotter = console_.post_plotter;
        callouts_->Say(time_, e.callout, plotter, e.fields, NpcAt(plotter) ? SpeakDelayS(plotter) : 0.0);
        if (e.callout == "plot_new") board_known_s_[e.target_id] = time_;
        ++plot_reports_made_;
    }
    void UpdatePlotterReports() {
        const std::string& plotter = console_.post_plotter;
        if (!post_ || plotter.empty() || !callouts_ || def_.crew_data.FindCallout("plot_new") == nullptr) return;
        // each target's latest mark on the board
        std::map<std::string, const BoardMark*> latest;
        for (const BoardMark& m : post_->Board().Marks()) {
            const BoardMark*& l = latest[m.target_id];
            if (l == nullptr || m.time_s >= l->time_s) l = &m;
        }
        std::vector<PlotterEvent> events;
        for (const auto& kv : latest) {
            const BoardMark& m = *kv.second;
            if (reported_kills_.count(m.target_id) != 0) continue;
            if (plot_new_seen_.insert(m.target_id).second) {
                events.push_back({"plot_new", m.target_id, time_, PlotFields(m)});
                continue;
            }
            if (time_ - m.time_s >= Tuning().plot_lost_s) {
                if (plot_lost_said_.insert(m.target_id).second) events.push_back({"plot_lost", m.target_id, time_, PlotFields(m)});
                continue;
            }
            // its course: from its latest mark at least 3 km back along its track (clear of the plotting error)
            const BoardMark* back = nullptr;
            for (const BoardMark& o : post_->Board().Marks()) {
                if (o.target_id != m.target_id || o.time_s >= m.time_s || (m.position - o.position).HorizontalLength() < 3000.0) continue;
                if (back == nullptr || o.time_s > back->time_s) back = &o;
            }
            if (back == nullptr) continue;
            const Vec3 d = m.position - back->position;
            double course = RadToDeg(std::atan2(d.x, d.y));
            if (course < 0.0) course += 360.0;
            auto c = plot_course_deg_.find(m.target_id);
            if (c == plot_course_deg_.end()) {
                plot_course_deg_[m.target_id] = course;
            } else if (std::fabs(std::remainder(course - c->second, 360.0)) >= Tuning().plot_turn_deg) {
                c->second = course;
                PlotterEvent e{"plot_turn", m.target_id, time_, PlotFields(m)};
                e.fields["course"] = std::to_string(static_cast<int>(std::lround(course)) % 360);
                events.push_back(e);
            }
        }
        for (const PlotterEvent& e : events) {
            ++plot_reports_due_;
            SeatDuty d = SeatDuty::Report(plotter, e.callout, "a mark new, turned or lost on the board: tell the commander", e.fields);
            d.target = e.target_id;
            if (NpcDoes(plotter, std::move(d))) {
                PlotterSays(e);
            } else {
                plotter_events_.push_back(e);
            }
        }
        for (auto it = plotter_events_.begin(); it != plotter_events_.end();) {
            if (time_ - it->at_s <= Tuning().plot_stale_s) {
                ++it;
                continue;
            }
            ++plot_reports_missed_;
            log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(plotter), it->callout,
                         "not reported to the commander: target " + it->fields["number"]});
            it = plotter_events_.erase(it);
        }
        SetRadioInput(console_.equipment.HasInput("plot_report_waiting") ? std::string("plot_report_waiting") : std::string(),
                      !plotter_events_.empty());
    }
    // The oldest of a player plotter's reports of this kind still to say (null: none).
    const PlotterEvent* PendingPlot(const std::string& callout) const {
        for (const PlotterEvent& e : plotter_events_) {
            if (e.callout == callout) return &e;
        }
        return nullptr;
    }
    // The commander's picture as an NPC commander has it: the tube's tracks (his own eyes), and of the board's targets
    // those the plotter called or he has since found there himself; a player commander reads the board himself.
    std::vector<BoardMark> CommanderPicture() const {
        std::vector<BoardMark> all = PostPicture();
        const bool npc = MemberAtConst(Cmdr()) != nullptr && SeatHolder(Cmdr()).empty();
        if (!post_ || !npc || console_.post_plotter.empty() || def_.crew_data.FindCallout("plot_new") == nullptr) return all;
        std::vector<BoardMark> out;
        for (const BoardMark& m : all) {
            const auto first = post_first_mark_.find(m.target_id);
            const bool on_board = first != post_first_mark_.end();
            if (!on_board || board_known_s_.count(m.target_id) != 0 || time_ - first->second >= Tuning().notice_board_s) {
                out.push_back(m);
                continue;
            }
            for (const AspdTrack& k : post_->Tracks()) {  // not yet his from the board: on the tube, if it is there
                if (k.target_id == m.target_id && !k.imitated) out.push_back(TubeMark(k));
            }
        }
        return out;
    }

    // How long the commander's order takes to reach a seat: at once on the intercom (a moment to turn his box to ВС if it
    // is on a set; the call reaches a box on the radio); shouted over the cabin when either has no headset or no box.
    double OrderDelayS(const std::string& role) const {
        if (!voice_) return comms_ ? comms_->OrderDelayS(role, console_) : 0.0;
        if (role == Cmdr()) return 0.0;
        const bool speaks = voice_->OnBox(Cmdr());
        const bool hears = voice_->HearsIntercom(role) || voice_->HearsCall(role);
        if (speaks && hears) return voice_->TalksOnIntercom(Cmdr()) ? 0.0 : Tuning().box_select_s;
        return ShoutS();
    }

    // ---- the Osa on its battery's net (a linked PU-12M: battery_link.hpp)
    double osa_net_lost_s_ = -1.0;
    std::vector<int> own_readbacks_;
    // The commander reads a designation back on the net (rrs_1986: a command acknowledged at once by its repetition):
    // only on a set that can transmit (not Д. ПРИЁМ). The post hears it through the link.
    void OsaReadBack(const C2Network::Event& e) {
        if (!callouts_ || !NpcDoes(Cmdr(), SeatDuty::Report(Cmdr(), "readback", "a target given on the battery net: read it back"))) return;
        if (comms_ && c2_) {
            const int set = c2_->ListeningSet();
            if (!comms_->Transmitting(set, console_)) {
                Event("battery net", "the designation can't be read back: the set is on Д. ПРИЁМ or its transmitter is warming");
                return;
            }
        }
        char n[16];
        std::snprintf(n, sizeof(n), "%04d", e.number);
        callouts_->Say(time_, "readback", Cmdr(), {{"number", n}, {"callsign", OwnCallsign()}, {"post", "10"}}, SpeakDelayS(Cmdr()));
        if (LinkedToPost()) own_readbacks_.push_back(e.number);
    }
    // A player in command speaks on the battery net himself (the crew menu): «21 к бою готов, …» (voenizdat_osa_1991
    // pp. 246-247) or the read-back of the latest designation, with his A-1 on Р-123, the headset switch on the set that
    // hears the net, that set able to transmit. False with LastRefusal() saying why.
    bool PlayerOnBatteryNet(const std::string& station, const std::string& callout_id) {
        if (!callouts_ || !c2_ || station != Cmdr()) {
            last_refusal_ = "only the commander speaks on the battery net";
            return false;
        }
        if (callout_id == "readback" && designations_heard_.empty()) {
            last_refusal_ = "no target has been given you on the battery net";
            return false;
        }
        if (!c2_->Hearing()) {
            last_refusal_ = "you are not on the battery net: A-1 to Р-123, the headset switch on its set";
            return false;
        }
        if (comms_ && !comms_->Transmitting(c2_->ListeningSet(), console_)) {
            last_refusal_ = "the set can't transmit: on Д. ПРИЁМ, or its transmitter warming";
            return false;
        }
        if (!callouts_->Say(time_, callout_id, Cmdr(), NetReportFields(callout_id))) return false;
        reports_made_.push_back({time_, station, callout_id});
        if (callout_id == "readback" && LinkedToPost()) own_readbacks_.push_back(designations_heard_.back().number);
        return true;
    }
    // The blanks of the commander's words on the battery net: his call sign, the post's, the latest target given him, his
    // channels and missiles. A blank he has nothing for is left out.
    std::map<std::string, std::string> NetReportFields(const std::string& callout_id) const {
        std::map<std::string, std::string> f{{"callsign", OwnCallsign()}};
        if (callout_id == "readback") {
            f["post"] = "10";
            if (!designations_heard_.empty()) {
                char n[16];
                std::snprintf(n, sizeof(n), "%04d", designations_heard_.back().number);
                f["number"] = n;
            }
        }
        if (callout_id == "report_ready") {
            f["channels"] = "2";
            if (launcher_) f["missiles"] = std::to_string(launcher_->LoadedCount());
        }
        return f;
    }
    // «К бою»: the deputy and the operators check the intercom and report «Первый (второй, третий) готов»
    // (voenizdat_osa_1991 pp. 246-247), each as he takes up his drill (the R-124 runs on the vehicle's 27 V), or at once
    // in a vehicle already powered. Checked again after the vehicle has been stood down (its power off).
    std::set<std::string> intercom_checked_;
    bool check_power_was_ready_ = false;
    void UpdateIntercomCheck() {
        if (!voice_ || post_ || !callouts_ || console_.FindReadiness("powered") == nullptr) return;
        const bool power = console_.equipment.IsReady("power");
        if (check_power_was_ready_ && !power) intercom_checked_.clear();
        check_power_was_ready_ = power;
        static const char* const seats[3][2] = {{"deputy_commander", "Первый"}, {"senior_operator", "Второй"}, {"operator", "Третий"}};
        for (const auto& s : seats) {
            const std::string st = s[0];
            if (intercom_checked_.count(st) != 0 || !voice_->HearsIntercom(st)) continue;
            const CrewMember* c = MemberAtConst(st);
            if (c == nullptr || (c->Status() == CrewStatus::Idle && !power)) continue;
            if (!NpcDoes(st, SeatDuty::Report(st, "seat_ready", "К бою: the intercom checked",
                                              {{"seat_number", s[1]}, {"seat_number_en", std::to_string(&s - seats + 1)}}))) {
                continue;
            }
            intercom_checked_.insert(st);
            Say("seat_ready", st, {{"seat_number", s[1]}, {"seat_number_en", std::to_string(&s - seats + 1)}});
        }
    }
    // After a missed order to change the battery net's frequency: the NPC commander finds it after 5 min (rrs_1986).
    void UpdateOsaNet() {
        if (post_ || !c2_ || osa_net_lost_s_ < 0.0) return;
        if (c2_->Hearing()) {
            osa_net_lost_s_ = -1.0;
            return;
        }
        if (NpcAt(Cmdr()) && time_ - osa_net_lost_s_ >= Tuning().both_frequencies_s) {
            const int k = c2_->PresetOfSource(def_.c2_policy.primary);
            if (k > 0) c2_->TuneSet(c2_->ListeningSet(), k, time_, "Commander");
            Event("battery net", "no contact for 5 min: the commander tries the other frequency and finds the net");
            osa_net_lost_s_ = -1.0;
        }
    }

public:
    // The battery commander moved the battery net (a linked post's order, battery_link.hpp): heard on the Osa's set if
    // it hears the net; the NPC commander then turns his set to it. Missed, the Osa is off the net until its clock.
    void HearBatteryNetOrder(int to) {
        if (!c2_) return;
        const bool heard = c2_->Hearing();
        c2_->MoveSource(def_.c2_policy.primary, to, time_);
        if (heard) {
            const TalkEntry* e = def_.talk_tables.ByEffect("spare_frequency");
            const std::string group = e != nullptr ? e->code : std::string("переходите на запасную частоту");
            Radio("battery net", "10", "Всем, я 10: " + group + ". " + group + ".");
            if (e != nullptr) HeardGroup(e->code, "battery", BatteryNetHearers());
            if (NpcAt(Cmdr())) c2_->TuneSet(c2_->ListeningSet(), to, time_, "Commander");
        } else {
            osa_net_lost_s_ = time_;
            Event("battery net", "the order to change frequency was not heard");
        }
    }
    // A lesson's cues (tutorial.hpp): a net's main station orders it to its other frequency now; a strong spot jammer
    // comes on a net; the far end of a line rings.
    bool ExternalNetOrder(const std::string& net) {
        const RadioNetDef* n = def_.radio_data.Find(net);
        if (!nets_ || n == nullptr || n->main == "self" || nets_->Numbers(net) < 2) return false;
        const int to = nets_->Current(net) == 1 ? 2 : 1;
        if (!nets_->MoveTo(net, to, time_, "ordered by " + n->main)) return false;
        OnNetMoved({net, to, true});
        return true;
    }
    bool StartJamming(const std::string& net) {
        if (!nets_ || def_.radio_data.Find(net) == nullptr) return false;
        JammingDef j;
        j.nets = {net};
        j.from_s = time_;
        j.strong = true;
        j.spot = true;
        nets_->AddJamming(j);
        return true;
    }
    bool RingLine(int line) {
        const LineDef* l = def_.radio_data.Line(line);
        if (l == nullptr || !l->laid) return false;
        LineState& st = lines_[line];
        st.ringing = true;
        st.ring_from_s = time_;
        st.text = !l->calls.empty() ? l->calls.front().text : std::string("Проверка связи.");
        Event("line " + std::to_string(line), l->to_name + " rings");
        return true;
    }
    std::vector<int> TakeReadbacks() {
        std::vector<int> out;
        out.swap(own_readbacks_);
        return out;
    }
    // The post hears a launcher's read-back (a linked Osa's) if its box is on the battery net and the net carries it.
    void HearReadback(const std::string& callsign, int number) {
        const bool heard = NetHears() && (!nets_ || nets_->GetsThrough("battery", time_));
        char n[16];
        std::snprintf(n, sizeof(n), "%04d", number);
        if (heard) {
            ++readbacks_heard_;
            Radio("battery net", callsign, OwnCallSignOn("battery") + ", я " + callsign + ": цель " + n + ", понял. Приём.");
        } else {
            ++readbacks_missed_;
            Event(callsign, std::string("read back ") + n + ", not heard at the post");
        }
    }

private:
    // ---- The player's vehicle is the command post (docs/PU12_DEVELOPMENT.md P3, command_post.hpp).
    std::unique_ptr<CommandPost> post_;
    std::map<std::string, double> post_first_mark_;  // first mark of each target on the post's board (the log)

    // ---- The regiment's commands (proposal 5 of docs/research/PLOTTING_BOARD_2026-10.md): by telecode on the R-407 as a
    // two-digit code on the tube, and by voice on the regiment's command net (the R-111). Each is taken by whichever is up
    // when it is due, and waits while neither is; the battery acts on its effect (the code table's).
    std::size_t next_command_ = 0;
    int post_command_ = -1;
    bool fire_banned_ = false;
    void ReceiveRegimentCommands() {
        const bool telecode = console_.equipment.IsReady(console_.post_telecode);
        // By voice: the R-111 worked as a set hears the net (radio_sets.hpp); without the set, the console's net subsystem
        // within the R-111's published reach.
        bool voice = r111_ ? console_.equipment.GetInput(console_.r111.Input("hear"))
                           : !console_.post_regiment_net.empty() && console_.equipment.IsReady(console_.post_regiment_net) &&
                                 post_->RegimentNetInReach(def_.post_regiment, time_);
        if (voice_ && !SetOfNet("regiment").empty()) voice = voice && !NetHearers("regiment").empty();
        if (!telecode && !voice) return;  // nothing comes on either net: it waits
        const std::vector<RegimentCommand>& cmds = def_.post_regiment.commands;
        while (next_command_ < cmds.size()) {
            const RegimentCommand& c = cmds[next_command_];
            if (!c.on_cue && c.at_s > time_ + 1e-9) break;
            if (!c.on_cue && !TakeRegimentCommand(c, telecode, voice)) break;  // a group waits for the voice net
            ++next_command_;
        }
        while (!cued_commands_.empty() && TakeRegimentCommand(cued_commands_.front(), telecode, voice)) {
            cued_commands_.erase(cued_commands_.begin());
        }
    }
    // One command taken off the nets that are up; false while it must wait (a group, said by voice only, with the voice
    // net down). Lost when jammed out.
    std::vector<RegimentCommand> cued_commands_;
    bool TakeRegimentCommand(const RegimentCommand& c, bool telecode, bool voice) {
        const RadioNetDef* rn = def_.radio_data.Find("regiment");
        const bool got = voice && (!nets_ || rn == nullptr || nets_->GetsThrough("regiment", time_, true));
        if (!c.group.empty()) {  // a group of ПТ-1, on the command net only
            if (!voice) return false;
            const TalkEntry* te = def_.talk_tables.Decode(c.group);
            if (!got || te == nullptr || rn == nullptr) {
                Event("regiment net", "group " + c.group + " lost in the jamming");
                return true;
            }
            SayRegimentGroup(*rn, *te);
            ApplyRegimentEffect(te->effect);
            return true;
        }
        const AspdCode* code = def_.post_codes.Find(c.code);
        char text[96];
        if (telecode) {
            post_command_ = c.code;
            std::snprintf(text, sizeof(text), "%02d on the IT-45: %s", c.code, code != nullptr ? code->meaning.c_str() : "(not in the table)");
            Event("regiment", text);
        }
        const TalkEntry* te = def_.talk_tables.ByAspd(c.code);
        if (got && code != nullptr && te != nullptr && rn != nullptr) {  // a group of ПТ-1: decoded by whoever keeps the table
            SayRegimentGroup(*rn, *te);
        } else if (got && code != nullptr) {
            std::string said = code->meaning;
            if (!said.empty()) said[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(said[0])));
            Radio("regiment net", "Regiment", said + "!");
        }
        if ((telecode || got) && code != nullptr) ApplyRegimentEffect(code->effect);  // else jammed out: lost
        return true;
    }
    // The regiment's call with a group («Рябина, я Тополь: 143. Приём.»): heard by whoever's box is on its net, and read
    // against the table by an NPC among them (a player reads the table on his desk).
    void SayRegimentGroup(const RadioNetDef& rn, const TalkEntry& te) {
        const std::vector<std::string> ears = NetHearers("regiment");
        Radio("regiment net", rn.main_call_sign, rn.own_call_sign + ", я " + rn.main_call_sign + ": " + te.code + ". Приём.", false, ears);
        HeardGroup(te.code, "regiment", ears);
        DecodeHeard(ears, &te);
    }
    void ApplyRegimentEffect(const std::string& effect) {
        if (effect == "fire_ban" || effect == "cease_fire") fire_banned_ = true;
        if (effect == "fire_free") fire_banned_ = false;
        if (effect == "readiness" && battery_) {
            battery_->SetAirThreat(true, time_);
            air_threat_forced_ = true;
        }
        if (effect == "report_readiness") {  // asked again: the NPC commander reports (a player answers it himself)
            regiment_reported_ = false;
            if (NpcDoes(Cmdr(), SeatDuty::Order(Cmdr(), "order_report_regiment", "the regiment asks for the readiness again"))) ReportToRegiment();
        }
    }

public:
    // A lesson's cue (tutorial.hpp "regiment:<group or code>"): the regiment's command held on cue goes out now, on the
    // nets that are up (a group waits for the command net). False if the scenario holds none such, or it has gone already.
    bool ReleaseRegimentCommand(const std::string& what) {
        for (std::size_t i = 0; i < def_.post_regiment.commands.size(); ++i) {
            const RegimentCommand& c = def_.post_regiment.commands[i];
            if (!c.on_cue || cued_released_.count(i) != 0 || !RegimentCommandIs(c, what)) continue;
            cued_released_.insert(i);
            cued_commands_.push_back(c);
            return true;
        }
        return false;
    }
    // Is it this command: its group, or its two digits?
    static bool RegimentCommandIs(const RegimentCommand& c, const std::string& what) {
        if (!c.group.empty()) return c.group == what;
        char code[8];
        std::snprintf(code, sizeof(code), "%02d", c.code);
        return what == code || what == std::to_string(c.code);
    }
    // A talk-table group said on a net, and the seats whose boxes were on it (the lessons' "group:<code>").
    struct GroupHeard {
        double time_s = 0.0;
        std::string code;
        std::string net;
        std::vector<std::string> ears;
    };
    const std::vector<GroupHeard>& GroupsHeard() const { return groups_heard_; }

private:
    std::set<std::size_t> cued_released_;
    std::vector<GroupHeard> groups_heard_;
    void HeardGroup(const std::string& code, const std::string& net, const std::vector<std::string>& ears) {
        groups_heard_.push_back({time_, code, net, ears});
    }
    std::string PostCommandText() const {
        if (post_command_ < 0) return std::string();
        const AspdCode* code = def_.post_codes.Find(post_command_);
        char text[96];
        std::snprintf(text, sizeof(text), "%02d %s", post_command_, code != nullptr ? code->meaning.c_str() : "");
        return text;
    }

    // The regiment's telecode is within reach (the R-407 net, its outages); the console shows it once its ASPD-U works;
    // the post is oriented when its subsystem says so; the plotter works when he is free (an NPC not at a drill).
    void UpdatePost() {
        if (!post_) return;
        post_->MoveTo(def_.unit_position);
        if (!r407_) {  // without the set worked as a set: the R-407 net's published reach (aspd_u.json)
            const bool reach = post_->TelecodeInReach(def_.post_regiment, time_);
            if (reach != console_.equipment.GetInput(console_.post_telecode_input)) console_.equipment.SetInput(console_.post_telecode_input, reach);
        }
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
        // The primary radar's source switch (rwd_mb3 pu12e): the cabled radar's picture comes through only with the
        // switch at its type's position.
        if (!console_.post_radar_input.empty()) {
            const int want = def_.post_radar.switch_position;
            const bool through = def_.has_post_radar && (want == 0 || console_.post_radar_switch.empty() ||
                                                         console_.panel.Get(console_.post_radar_switch) + 1 == want);
            if (through != console_.equipment.GetInput(console_.post_radar_input)) console_.equipment.SetInput(console_.post_radar_input, through);
        }
        post_->SetRegimentDesignationRange(def_.post_regiment.designate_within_m);
        post_->SetFrame(world_.Frame());
        ReceiveRegimentCommands();
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
        // The air-picture net: the regiment's radar battery reads from its radar (the regiment's first, else its post).
        if (!def_.post_regiment.radars.empty()) {
            post_->SetAirNetSource(def_.post_regiment.radars.front().position);
        } else if (def_.post_regiment.has_position) {
            post_->SetAirNetSource(def_.post_regiment.position);
        }
        in.air_net = !console_.post_air_net.empty() && console_.equipment.IsReady(console_.post_air_net) &&
                     post_->AirNetInReach(def_.post_regiment, time_);
        in.imitation = !console_.post_imitation.empty() && console_.equipment.IsReady(console_.post_imitation);
        in.board_open = console_.post_board_open.empty() || console_.equipment.IsReady(console_.post_board_open);
        const CrewMember* plotter = MemberAt(console_.post_plotter);
        in.operator_free = plotter != nullptr && SeatHolder(console_.post_plotter).empty() && !plotter->IsBusy();
        in.operator_speed = PlotterSpeed();
        if (voice_) {  // through the crew: a seat's box on FuG-1, the set on its net; read out to the plotter
            in.air_net = in.air_net && !NetHearers("air_picture").empty();
            in.relay = true;
            in.air_net_loss = nets_ && nets_->Jam("air_picture", time_) == 1 ? Tuning().weak_jam_loss : 0.0;
        }
        // The tube is the commander-operator's (rwd_mb3): his hands take the cabled radar's targets off, an NPC's when
        // he is free, a player's with the ASPD-U's semi-automatic take-off.
        const CrewMember* commander = MemberAt(Cmdr());
        in.tube_free = commander != nullptr && (!SeatHolder(Cmdr()).empty() || !commander->IsBusy());
        in.tube_speed = SeatSpeed(Cmdr());
        post_->Update(time_, CommandPost::RegimentSees(def_.post_regiment, truth), radar, in);
        if (!SeatHolder(console_.post_plotter).empty()) {  // the NPC plotter writes each report he hears; a player owes them
            for (const CommandPost::AirReport& h : post_->Heard()) {
                NpcDoes(console_.post_plotter, SeatDuty::Plot(console_.post_plotter, h.number, h.contact.target_id,
                                                              "a report heard: write it on the board"));
            }
        }
        // What the post heard on the air-picture net, in its radio log (the plotter's headset; first person).
        for (const CommandPost::Spoken& s : post_->TakeSpoken()) {
            if (s.heard) Radio("air picture", "Regiment", s.report.text);
        }
        RelayAirPicture();
        SetRadioInput(console_.equipment.HasInput("air_report_waiting") ? std::string("air_report_waiting") : std::string(),
                      !post_->Held().empty());
        for (const BoardMark& m : post_->Board().Marks()) {
            if (post_first_mark_.count(m.target_id) != 0) continue;
            post_first_mark_[m.target_id] = m.time_s;
            Event(m.target_id, "on the post's board: " + post_->Board().Describe(m));
        }
        UpdatePlotterReports();
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
        // The characteristics keys (СВ, Ч, ГР, Н1-Н3; rwd_mb3 pu12a): set on the target in the window or under the marker.
        CommandPost::Attributes attr;
        auto pressed = [&c](const std::string& id) { return !id.empty() && c.panel.Get(id) == 1; };
        if (pressed(c.post_keypad_friend)) attr.friendly = 1;
        if (pressed(c.post_keypad_foe)) attr.friendly = 0;
        if (pressed(c.post_keypad_group)) attr.group = 1;
        for (int z = 0; z < 3; ++z) {
            if (pressed(c.post_keypad_zone[z])) attr.zone = z + 1;
        }
        if (attr.friendly >= 0 || attr.group >= 0 || attr.zone > 0) {
            const int target = KeypadTarget();
            if (target >= 0 && post_ && post_->SetAttributes(target, attr)) {
                char text[64];
                std::snprintf(text, sizeof(text), "%02d:%s%s%s", target, attr.friendly == 1 ? " СВ" : attr.friendly == 0 ? " Ч" : "",
                              attr.group == 1 ? " ГР" : "", attr.zone > 0 ? (attr.zone == 1 ? " Н1" : attr.zone == 2 ? " Н2" : " Н3") : "");
                k.last = text;
            }
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
    double PlotterSpeed() const { return SeatSpeed(console_.post_plotter); }
    double SeatSpeed(const std::string& station) const {
        for (std::size_t i = 0; i < crew_.size(); ++i) {
            if (crew_[i].Role() != station) continue;
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
        if (voice_ && !voice_->HeadsetOn(Cmdr())) c2_->SetCommanderOnRadio(false);  // no headset: no net
        c2_->Update(time_, contacts, def_.unit_position);
        for (const C2Network::Event& e : c2_->PeekEvents()) {
            Radio("command net", e.actor, e.subject + ": " + e.text);
            // The commander heard it on the radio, and passes it on; not a target he heard destroyed (the PU-12's plot
            // a few seconds behind the kill report).
            if (e.actor == "Commander" && e.number > 0 && reported_kills_.count(e.subject) == 0) {
                designations_.push_back(e);
                designations_heard_.push_back(e);
                OsaReadBack(e);
                if (e.group_centre) {  // a group's centre is no aircraft: the crew counts the group on its scope by the rule
                    GroupOrder g;
                    g.number = e.number;
                    g.rule = e.rule > 0 ? e.rule : 1;  // none given: the crew chief chooses (sam3d: the first)
                    g.azimuth_deg = e.azimuth_mils * 360.0 / 6000.0;
                    g.range_m = e.range_m;
                    g.heard_s = time_;
                    group_orders_.push_back(g);
                    continue;
                }
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
        if (engagement_on_ && (engagement_.ShotsInFlight() || kill_.OpenCount() > 0)) return;
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

    static bool Moved(StepResult r) {
        return r == StepResult::Accepted || r == StepResult::AcceptedEarly || r == StepResult::NotNextStep ||
               r == StepResult::ProcedureComplete || r == StepResult::Toward;
    }
    std::vector<SeatAction> seat_actions_;
    std::vector<GivenOrder> orders_given_;
    std::vector<MadeReport> reports_made_;
    int designations_made_ = 0;
    int plots_made_ = 0;
    CommandPost::PlotResult last_plot_;
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
    // ---- The crew's bodies out of their seats (crew_mobility.hpp).
    std::map<std::string, CrewMover> movers_;
    std::map<std::string, bool> hatches_;           // exit id -> standing open
    std::set<std::string> tracked_;                 // seats whose body the engine reports (the players')
    std::map<std::string, double> move_refused_s_;  // a seat's last refused way (tried again every 2 s)

    void StartMovers() {
        if (!def_.has_vehicle) return;
        const double stand_s = def_.has_crew_data ? def_.crew_data.stand_s : def_.walking.gait.stand_up_s;
        const double sit_s = def_.has_crew_data ? def_.crew_data.sit_s : def_.walking.gait.sit_down_s;
        for (CrewMember& c : crew_) {
            if (def_.vehicle.Seat(c.Role()) == nullptr) continue;
            CrewMover m(&def_.vehicle, c.Role());
            m.SetSeatTimes(stand_s, sit_s);
            // A slower hand is a slower climber (docs/crew/05_SKILL.md): the tier's reaction against a trained man's 1 s.
            if (c.HasSkill()) m.SetPace(Clamp(c.Skill().reaction_s, 0.7, 1.8));
            movers_.emplace(c.Role(), m);
            std::vector<OutsideJob> jobs;
            for (const VehicleJob& j : def_.vehicle.jobs) {
                if (j.station == c.Role()) jobs.push_back({j.control, j.place, j.stay_until});
            }
            c.SetOutsideJobs(std::move(jobs));
        }
    }
    // The men still out of the vehicle, by the exit each comes back in by (his seat's): a hatch is left open for them.
    std::map<std::string, int> StillOut() const {
        std::map<std::string, int> out;
        for (const auto& m : movers_) {
            if (m.second.Inside()) continue;
            for (const VehicleExit& e : def_.vehicle.exits) {
                bool mine = false;
                for (const std::string& s : e.stations) mine = mine || (s == m.first && !e.emergency);
                if (mine) {
                    ++out[e.id];
                    break;
                }
            }
        }
        return out;
    }
    bool OthersOutside(const std::string& station) const {
        for (const auto& m : movers_) {
            if (m.first != station && m.second.Place() != BodyPlace::Seated && m.second.Place() != BodyPlace::Cabin) return true;
        }
        return false;
    }
    // The noise at a point outside: the countryside's, and the vehicle's machinery running.
    double NoiseAt(const Vec3& enu, const VehiclePose& pose) const {
        double n = def_.walking.earshot.quiet_db;
        for (const VehicleNoise& v : def_.vehicle.noise) {
            if (console_.equipment.IsReady(v.subsystem)) n = AddNoise(n, v.db_at_1m, (pose.ToEnu(v.at_m) - enu).Length());
        }
        return n;
    }
    static MoveGoal GoalFor(const std::string& want) {
        if (want.empty()) return MoveGoal::Seat();
        if (want == "lineup") return MoveGoal::InLineup();
        return MoveGoal::AtPlace(want);
    }
    std::string PlaceLabel(const std::string& id) const {
        if (id == "lineup") return "the line-up";
        const VehiclePlace* p = def_.vehicle.Place(id);
        return p != nullptr ? p->label : id;
    }
    // Each NPC goes where his drill wants him (CrewMember::WantedPlace); every body walks on; each crew member learns
    // where his body is.
    void UpdateMovers(double dt) {
        if (!def_.has_vehicle) return;
        const VehiclePose pose = Pose();
        const MapTerrain* map = map_ground_.get();
        for (CrewMember& c : crew_) {
            auto it = movers_.find(c.Role());
            if (it == movers_.end()) continue;
            CrewMover& m = it->second;
            const bool npc = SeatHolder(c.Role()).empty();
            if (npc && !PlayerBodyTracked(c.Role())) {
                const std::string want = c.WantedPlace();
                const bool there = m.At() == want && (!want.empty() || m.Place() == BodyPlace::Seated || m.Moving());
                const auto refused = move_refused_s_.find(c.Role());
                const bool retry = refused == move_refused_s_.end() || time_ - refused->second >= 2.0;
                if (!there && retry) {
                    MoveGoal g = GoalFor(want);
                    g.shut_after = want.empty();  // the last man in by a hatch shuts it
                    std::string why;
                    if (m.GoTo(g, &console_, pose, &why)) {
                        move_refused_s_.erase(c.Role());
                        Event(c.Role(), want.empty() ? std::string("back to his seat") : "out to " + PlaceLabel(want));
                    } else {
                        if (refused == move_refused_s_.end()) Event(c.Role(), "can't go out: " + why);
                        move_refused_s_[c.Role()] = time_;
                    }
                }
            }
            const std::map<std::string, int> still_out = StillOut();
            m.Update(dt, pose, &world_.GetTerrain(), map, def_.walking.gait, hatches_, &still_out);
            c.SetWhere(m.Moving() ? std::string("moving") : m.At(), m.Moving());
        }
    }

    struct PendingOrder {
        double at_s = 0.0;
        std::string crew;
        std::string procedure;
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
    // The steps of a player's drill whose switches already sit where they put them are checks, passed once their wait
    // is over, as an NPC's are (a light already on from the drill before); a drill so finished is done and the next one
    // ordered is due. Without it the drill would wait for a move the player has nothing to make.
    void PassSeatChecks(Seat& st) {
        for (int guard = 0; guard < 8; ++guard) {
            if (st.tracker.GetProcedure().id.empty() || st.tracker.IsComplete()) return;
            st.tracker.PassChecks(console_.panel, &console_.equipment);
            if (!st.tracker.IsComplete()) return;
            log_.Record({time_, DebriefEntry::Kind::Completed, st.player, st.tracker.GetProcedure().id, std::string()});
            if (st.queued.empty()) return;
            const Procedure* next = console_.FindProcedure(st.queued.front());
            st.queued.erase(st.queued.begin());
            if (next == nullptr) return;
            st.tracker.Reset(*next);
        }
    }
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
    // ---- The helper's duties (docs/crew/14_HELPER.md).
    bool helper_ = false;
    bool shadow_ = false;        // the NPC commander thinks in the player's place (ShadowCommanding): he acts on nothing
    std::string shadow_target_;  // the target he would have the crew take
    // A drill he would hand a seat in this think; a single switch (control, position) when it is no drill of the data.
    struct ShadowDrill {
        std::string seat;
        std::string drill;
        std::string control;
        int position = -1;
    };
    std::vector<ShadowDrill> shadow_drills_;
    std::map<std::string, double> shadow_unorderable_;  // "seat: drill" he would order that no order of the crew data starts
    std::vector<SeatDuty> owed_;
    // An order the player gave this lately counts, for the shadow commander, as the one he would give (he doesn't ask
    // for it again).
    static constexpr double kShadowRecallS = 60.0;

    // The one way an NPC acts for a seat. True when the seat's NPC is there to do it. When a player holds the seat, or
    // the commander thinks in the player's place, it is false and the act is the player's duty instead (the helper shows
    // it). New NPC behaviour goes through here, not through NpcAt, so the helper knows of it without more code.
    bool NpcDoes(const std::string& seat, SeatDuty d) {
        if (!shadow_ && NpcAt(seat)) return true;
        if (SeatHolder(seat).empty()) return false;
        d.seat = seat;
        Owe(std::move(d));
        return false;
    }
    void Owe(SeatDuty d) {
        const std::string key = d.Key();
        for (SeatDuty& o : owed_) {
            if (o.Key() != key) continue;
            o.last_s = time_;
            o.value = d.value;
            o.fields = std::move(d.fields);
            o.has_about = d.has_about;
            o.about = d.about;
            return;
        }
        d.since_s = time_;
        d.last_s = time_;
        owed_.push_back(std::move(d));
    }
    bool PlayerGave(const std::string& order, double within_s) const {
        for (auto it = orders_given_.rbegin(); it != orders_given_.rend() && time_ - it->time_s <= within_s; ++it) {
            if (it->order == order) return true;
        }
        return false;
    }
    // The shadow commander's order: one the player gave lately counts as given; else the player owes it.
    bool ShadowOrder(const std::string& order_id) {
        if (PlayerGave(order_id, kShadowRecallS)) return true;
        if (def_.crew_data.FindOrder(order_id) != nullptr) {
            SeatDuty d = SeatDuty::Order(Cmdr(), order_id, "the commander's next order", shadow_target_);
            // The target's number from its designation, as his words would have it (SpeakOrder).
            for (const C2Network::Event& e : designations_heard_) {
                if (!shadow_target_.empty() && e.subject == shadow_target_ && e.number > 0) {
                    char n[16];
                    std::snprintf(n, sizeof(n), "%04d", e.number);
                    d.fields["number"] = n;
                }
            }
            NpcDoes(Cmdr(), d);
        }
        return false;
    }
    // Whether an order of the crew data starts this drill at its seat. A single switch the NPC commander orders on its
    // own (the readiness loop's "order: <control>") is no drill of any order: it is left unorderable, not matched to an
    // order whose drill happens to set it too (the TV camera's switch is in the EMCON engagement's drill: a different
    // order altogether).
    bool OrderStarts(const OrderDef& o, const ShadowDrill& d) const {
        for (const OrderStart& st : o.procedures) {
            if (st.station == d.seat && st.procedure == d.drill) return true;
        }
        return false;
    }
    // The drills the shadow commander would hand out in one think, as the orders of the crew data that start them: the
    // order that starts the most of them first (К бою rather than each seat's drill), else the one with the fewest steps.
    // A drill no order starts is left (ShadowUnorderable): a commander can't give it from his menu.
    void ShadowDrillOrders() {
        std::vector<ShadowDrill> left;
        left.swap(shadow_drills_);
        while (!left.empty()) {
            const OrderDef* best = nullptr;
            std::size_t best_n = 0;
            std::size_t best_steps = 0;
            for (const OrderDef& o : def_.crew_data.orders) {
                std::size_t n = 0;
                for (const ShadowDrill& sd : left) n += OrderStarts(o, sd) ? 1 : 0;
                std::size_t steps = 0;
                for (const OrderStart& st : o.procedures) {
                    const Procedure* p = console_.FindProcedure(st.procedure);
                    steps += p != nullptr ? p->steps.size() : 0;
                }
                if (n > best_n || (n > 0 && n == best_n && steps < best_steps)) {
                    best = &o;
                    best_n = n;
                    best_steps = steps;
                }
            }
            if (best == nullptr) {
                for (const ShadowDrill& sd : left) {
                    const ControlDef* c = sd.control.empty() ? nullptr : console_.panel.Find(sd.control);
                    shadow_unorderable_[sd.seat + ": " + (c != nullptr ? ShortLabel(sd.control, sd.position) + " (" + sd.control + ")" : sd.drill)] = time_;
                }
                return;
            }
            if (!PlayerGave(best->callout, kShadowRecallS)) {
                NpcDoes(Cmdr(), SeatDuty::Order(Cmdr(), best->callout, "the drills the crew needs next"));
            }
            for (std::size_t i = 0; i < left.size();) {
                const bool covered = OrderStarts(*best, left[i]);
                if (covered) {
                    left.erase(left.begin() + static_cast<std::ptrdiff_t>(i));
                } else {
                    ++i;
                }
            }
        }
    }
    // Each step: a duty is gone once the player has done it, it is no longer due (its lamp out, the order done already),
    // the moment has passed (keep_s since the NPC last would have done it), or the player has left the seat.
    void UpdateOwedDuties() {
        for (std::size_t i = 0; i < owed_.size();) {
            const SeatDuty& d = owed_[i];
            const bool gone = SeatHolder(d.seat).empty() || time_ - d.last_s > d.keep_s ||
                              (!d.holds_while.empty() && !console_.equipment.AllHold(d.holds_while, console_.panel)) || DutyDone(d);
            if (gone) {
                owed_.erase(owed_.begin() + static_cast<std::ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
    }
    // A report the player made settles the duty it answers: the one about the target it named, else the oldest.
    void SettleOwedReport(const std::string& station, const std::string& callout, const std::string& target) {
        auto match = [&](const SeatDuty& d, bool any) {
            return d.kind == SeatDuty::Kind::Report && d.seat == station && d.id == callout && (any || d.target == target);
        };
        for (int pass = 0; pass < 2; ++pass) {
            for (std::size_t i = 0; i < owed_.size(); ++i) {
                if (!match(owed_[i], pass == 1)) continue;
                owed_.erase(owed_.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
    }
    bool DutyDone(const SeatDuty& d) const {
        switch (d.kind) {
            case SeatDuty::Kind::Report:
                if (!d.target.empty()) return false;  // about one target: settled by the report that names it (SettleOwedReport)
                for (auto it = reports_made_.rbegin(); it != reports_made_.rend() && it->time_s + 1e-9 >= d.since_s; ++it) {
                    if (it->station == d.seat && it->callout == d.id) return true;
                }
                return false;
            case SeatDuty::Kind::Order: {
                for (auto it = orders_given_.rbegin(); it != orders_given_.rend() && it->time_s + 1e-9 >= d.since_s; ++it) {
                    if (it->order == d.id) return true;
                }
                const OrderCheck c = CheckOrder(d.id);
                return !c.can && c.reason_en == "done already";
            }
            case SeatDuty::Kind::Drill: return DrillNextStep(d.id, d.seat, d.since_s) == nullptr;
            case SeatDuty::Kind::Control: return console_.panel.Get(d.id) == d.number;
            case SeatDuty::Kind::Wheel: {
                double diff = console_.panel.GetValue(d.id) - d.value;
                if (d.wraps) diff = std::remainder(diff, 360.0);
                return std::fabs(diff) <= d.tolerance;
            }
            case SeatDuty::Kind::Designate:
                return DesignationPending(d.target) || (battery_fire_ && battery_fire_->Engaged(d.target)) || reported_kills_.count(d.target) != 0;
            case SeatDuty::Kind::Plot:
                if (!post_) return true;
                for (const CommandPost::AirReport& h : post_->Heard()) {
                    if (h.number == d.number) return false;
                }
                return true;
            case SeatDuty::Kind::Answer: return !voice_ || voice_->LineUp(d.seat, d.number);
            case SeatDuty::Kind::Halt: return console_.equipment.GetInput("vehicle_stopped");
        }
        return false;
    }
    std::vector<Seat> seats_;
    std::unique_ptr<Battery> battery_;
    std::vector<RadioMessage> radio_;
    void Radio(const std::string& net, const std::string& from, const std::string& text, bool callout = false) {
        Radio(net, from, text, callout, HeardBy(net));
    }
    void Radio(const std::string& net, const std::string& from, const std::string& text, bool callout, const std::vector<std::string>& heard_by) {
        RadioMessage m;
        m.time_s = time_;
        m.net = net;
        m.from = from;
        m.text = text;
        m.callout = callout;
        m.routed = voice_ != nullptr;
        m.heard_by = heard_by;
        radio_.push_back(std::move(m));
        if (radio_.size() > kRadioKept) radio_.erase(radio_.begin());
    }
    std::vector<char> battery_orders_done_;
    bool air_threat_forced_ = false;
    std::map<std::string, double> warned_s_;  // targets the battery was warned of from the board, and when
    std::set<std::string> warning_told_;      // their warning time is in the debrief

    // The board as the battery commander has it (he is in the post): the post's own board, else the PU-12's, else
    // what the vehicle heard; its crosses of the last three minutes, the targets reported destroyed wiped off.
    std::vector<BoardReading> CommanderBoard() const {
        std::vector<BoardFix> fixes;
        const PlottingBoard* b = post_ ? &post_->Board() : Board();
        const std::vector<BoardMark> heard = (b == nullptr && c2_) ? c2_->Known(time_) : std::vector<BoardMark>();
        for (const BoardMark& m : b != nullptr ? b->Marks() : heard) {
            if (time_ - m.time_s > 180.0 || reported_kills_.count(m.target_id) != 0) continue;
            fixes.push_back({m.target_id, m.number, m.position, m.time_s});
        }
        return ReadBoard(fixes, def_.unit_position, def_.battery_tactics.zone_edge_m);
    }
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
        // The helper: where the wheels' NPCs would bring them is worked out for players' wheels too; a player at the range
        // wheel is advised from what his own seat knows, as its NPC would be.
        aim_crew_.advise = helper_;
        if (helper_ && !aim_crew_.range && def_.has_crew_data && !def_.tuning.aim_range_role.empty() &&
            !SeatHolder(def_.tuning.aim_range_role).empty()) {
            aim_crew_.range_knows = &pictures_[def_.tuning.aim_range_role];
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
        if (voice_) callouts_->SetRouter([this](const CalloutDef& d, const std::string& from) { return RouteCallout(d, from); });
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
        if (shadow_) return;  // nobody speaks in the shadow commander's head
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
        if (shadow_) {  // words only: an order of the crew data is the player's to give, other words he keeps
            ShadowOrder(callout_id);
            return;
        }
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
        if (shadow_) {
            if (console_.FindProcedure(drill) != nullptr) shadow_drills_.push_back({seat, drill, std::string(), -1});
            return false;
        }
        CrewMember* m = MemberAt(seat);
        const Procedure* p = console_.FindProcedure(drill);
        if (m == nullptr || p == nullptr) return false;
        GiveOrder(*m, *p, queue);
        npc_ordered_[seat] = {drill, time_};
        return true;
    }
    static SeatDuty BatterySpareDuty() {
        SeatDuty d = SeatDuty::Order(std::string(), "order_battery_spare", "the battery net jammed: to its spare frequency");
        d.keep_s = 120.0;
        return d;
    }
    // The target the commander has the crew take (the shadow commander only keeps it for his advice).
    void NpcAssign(const std::string& target) { (shadow_ ? shadow_target_ : assigned_target_) = target; }
    // An automatic report of the crew data as the player's duty: due while its lamp or input stays on.
    static SeatDuty AutoReportDuty(const AutoReportDef& r) {
        SeatDuty d = SeatDuty::Report(r.from, r.callout, "a report the crew makes when its lamp or input comes on");
        d.holds_while = {r.when};
        d.keep_s = 600.0;
        return d;
    }
    // A seat is free for a new order when an NPC there has nothing to do, or the player there has finished his.
    bool SeatFree(const std::string& seat) {
        if (const ProcedureTracker* t = SeatTracker(seat)) return t->GetProcedure().id.empty() || t->IsComplete();
        const CrewMember* m = MemberAt(seat);
        return m != nullptr && !m->IsBusy();
    }

    // With the helper on and a player in the commanding seat, the same think runs as the shadow commander: whatever he
    // would order becomes the player's duty, and nothing else of it reaches the crew or the world (the root acts check
    // shadow_: GiveCrewOrder, GiveOrder, NpcOrder, SpeakOrder, Say, Event; the few direct acts below do too).
    void UpdateNpcCommander() {
        const bool shadow = !NpcCommanderActive() && ShadowCommanding();
        if ((!NpcCommanderActive() && !shadow) || !drill_ || time_ + 1e-9 < npc_next_think_s_) return;
        shadow_ = shadow;
        shadow_target_.clear();
        ThinkAsCommander();
        if (shadow_) ShadowDrillOrders();
        shadow_ = false;
    }
    void ThinkAsCommander() {
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
                if (!shadow_ && !s.loose.empty() && SeatFree(s.station) && !SeatHolder(s.station).empty() &&
                    console_.equipment.AllHold(s.loose_waits.front(), console_.panel)) {  // a player there: the switch is his
                    NpcDoes(s.station, SeatDuty::Control(s.station, s.loose.front().control, s.loose.front().position,
                                                         "the commander's order: " + s.loose.front().description));
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
        if (!halted) {  // on the march: a hostile he knows of inside 20 km stops the vehicle at once, in the column
                        // (mon_2112_80 p66; voenizdat_osa_1991 p254)
            for (const KnownTarget& k : pictures_[Cmdr()].Recent(time_, 30.0)) {
                if (!k.has_range || k.friendly || k.range_m > 20000.0 || npc_halt_ordered_) continue;
                if (!shadow_) npc_halt_ordered_ = true;  // the shadow asks for it until the player gives it
                SpeakOrder("order_halt");
                if (battery_ && def_.battery_drive_own) {  // the battery drives us: its short halt (inside 20 km: in the column)
                    BatteryOrder o;
                    o.kind = BatteryOrder::Kind::ShortHalt;
                    o.threat_range_m = k.range_m;
                    if (k.has_azimuth && k.true_azimuth) {
                        o.threat_has_azimuth = true;
                        o.threat_az_deg = k.azimuth_deg;
                    }
                    if (!shadow_) battery_->Order(o, time_);
                } else if (NpcDoes("driver", SeatDuty::Halt("driver", "the commander's halt: a hostile inside 20 km"))) {  // our driver stops the vehicle
                    npc_halt_at_ = time_ + SpeakDelayS("driver") + 2.0;
                }
                break;
            }
            if (!shadow_ && npc_halt_at_ > 0.0 && time_ >= npc_halt_at_) {
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
        // A missile's result being judged: the call decides whether to engage again (voenizdat_osa_1991 p267).
        if (!tracked.empty() && kill_.Pending(tracked)) return;
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
                    decoys_known_.count(k.target) == 0 && reported_kills_.count(k.target) == 0) {
                    known.push_back(k);
                }
            }
            std::sort(known.begin(), known.end(), [](const KnownTarget& a, const KnownTarget& b) { return a.range_m < b.range_m; });
            // With the battery commander in a real post, what he gave this vehicle comes first: the latest designation
            // to it of a target still in the picture (the post reads out only this vehicle's on its net).
            if (LinkedToPost()) {
                for (auto d = designations_heard_.rbegin(); d != designations_heard_.rend(); ++d) {
                    std::string want = d->subject;
                    if (d->group_centre) {  // a group's centre: the aircraft the rule picks
                        GroupOrder* g = FindGroupOrder(d->number);
                        if (g == nullptr) continue;
                        const std::string& so = def_.tuning.aim_az_role;
                        if (g->pick.empty() && (MemberAt(so) != nullptr || !SeatHolder(so).empty())) {  // the senior operator counts: wait for his report
                            const bool counting = g->told_s >= 0.0 ? (g->reported ? time_ - g->reported_s < kGroupPickWaitS : time_ - g->told_s < kGroupLookS)
                                                                   : time_ - g->heard_s < kGroupPickWaitS;
                            if (counting) return;
                        }
                        if (g->pick.empty()) g->pick = SelectByRule(EchoesOf(known, eng), g->azimuth_deg, g->range_m, g->rule, def_.tuning.group);
                        want = g->pick;
                    }
                    const auto at = std::find_if(known.begin(), known.end(), [&](const KnownTarget& k) { return k.target == want; });
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
                    NpcAssign(k.target);
                    NpcGive("order_emcon_tv", 90.0);
                }
                return;
            }
            if (k.range_m > reach || !console_.equipment.IsReady("hv_ready")) {
                NpcIdentify();  // meanwhile: who is it?
                return;
            }
            // A target in reach interrupts the function check (the deputy's РОД РАБОТЫ goes back with the next drills).
            if (CrewMember* dep = shadow_ ? nullptr : MemberAt("deputy_commander")) {
                if (dep->IsBusy() && dep->Tracker().GetProcedure().id == "function_check" && SeatHolder("deputy_commander").empty()) {
                    dep->CancelOrder();
                    console_.panel.Set("fo.mode", 1);
                }
            }
            NpcAssign(k.target);
            if (NpcGive("order_take_target", 60.0) && k.number == 0 && callouts_ && !shadow_) {  // no number from the PU-12: by its azimuth
                callouts_->Say(time_, "order_take_contact", Cmdr(), {{"azimuth", MilsText(k.azimuth_deg)}});
            }
            return;
        }
        // A target his crew has called destroyed and still held (a wrong call, or the track not yet broken): "Цель
        // сбросить!". If it flies on, the senior operator says so (AssessKills) and it may be taken again.
        if (reported_kills_.count(tracked) != 0) {
            if (npc_dropped_kill_ != tracked) {
                npc_dropped_kill_ = tracked;
                GiveCrewOrder("order_drop_target");
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
            if (!shadow_) {
                decoys_known_.insert(tracked);
                if (assigned_target_ == tracked) assigned_target_.clear();
            }
            npc_decoy_dropped_ = tracked;
            npc_decoy_dropped_s_ = time_;
            if (NpcDoes("deputy_commander", SeatDuty::Report("deputy_commander", "false_target", "the TV shows a decoy"))) {
                Say("false_target", "deputy_commander", {});
            }
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
        if (shadow_) return nullptr;  // the shadow commander's crew makes no slips (and draws no chances)
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
                if (NpcDoes("driver", SeatDuty::Report("driver", "ready_to_march", "the crew is stowed"))) Say("ready_to_march", "driver");
                if (battery_ && def_.battery_drive_own && !shadow_) {
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
                if (NpcDoes("deputy_commander", SeatDuty::Report("deputy_commander", "launcher_empty", "no missile left on the launcher"))) {
                    Say("launcher_empty", "deputy_commander");
                }
                const std::string callsign = battery_ ? OwnCallsign() : std::string("21");
                if (callouts_ && !shadow_) callouts_->Say(time_, "request_tzm", Cmdr(), {{"callsign", callsign}}, 3.0);
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
            if (half && !npc_fuel_reported_ && NpcDoes("driver", SeatDuty::Report("driver", "fuel_half", "the tank at half"))) {
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
        if (shadow_) return nullptr;
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
        const bool given = GiveCrewOrder(order_id);
        if (shadow_ && !given) return false;  // the shadow commander: the player hasn't given it yet
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
            if (now && report_state_[i] == 0 && !flicker && NpcDoes(r.from, AutoReportDuty(r))) {
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
                    case EngagementEvent::Kind::Contact: {
                        KnownTarget k;
                        k.target = e.target;
                        k.has_azimuth = k.has_range = true;
                        k.azimuth_deg = e.az_deg;
                        k.range_m = e.range_m;
                        if (NpcDoes("senior_operator", SeatDuty::Report("senior_operator", "contact", "a new echo on his scope",
                                                                        {{"azimuth", MilsText(e.az_deg)}, {"range_km", range}}).About(k))) {
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
                        } else if (console_.FindProcedure("iff_interrogate") != nullptr && !console_.search.iff_query.empty() &&
                                   e.range_m <= console_.search.iff_max_range_m) {
                            SeatDuty d = SeatDuty::Drill("senior_operator", "iff_interrogate", "a new echo in the interrogator's reach: ЗАПРОС");
                            d.target = e.target;
                            NpcDoes("senior_operator", d);
                        }
                        break;
                    }
                    case EngagementEvent::Kind::TrackLost:
                        // not a target just shot down, nor one the crew let go
                        if (TargetRange(e.target) >= 0.0 && e.detail != "dropped" &&
                            NpcDoes("operator", SeatDuty::Report("operator", "lock_lost", "the track was lost"))) {
                            Say("lock_lost", "operator");
                        }
                        break;
                    case EngagementEvent::Kind::IffNoReply: {
                        KnownTarget k;  // the senior operator saw no reply mark beside the echo
                        k.target = e.target;
                        k.interrogated = true;
                        k.time_s = time_;
                        k.source = "IFF";
                        pictures_["senior_operator"].Learn(k);
                        if (NpcDoes("senior_operator", SeatDuty::Report("senior_operator", "iff_no_reply", "no reply mark beside the echo").About(k))) {
                            Say("iff_no_reply", "senior_operator", {}, &k);
                        }
                        break;
                    }
                    case EngagementEvent::Kind::OnJammer:
                        if (NpcDoes("operator", SeatDuty::Report("operator", "on_jammer", "the radar holds only the jammer's angles"))) {
                            Say("on_jammer", "operator");
                        }
                        break;
                    case EngagementEvent::Kind::BurnThrough:
                        if (NpcDoes("operator", SeatDuty::Report("operator", "captured", "the echo shows through the jamming"))) {
                            Say("captured", "operator");
                        }
                        break;
                    case EngagementEvent::Kind::IffFriend: {
                        KnownTarget k;
                        k.target = e.target;
                        k.friendly = true;
                        if (NpcDoes("senior_operator", SeatDuty::Report("senior_operator", "iff_friend", "a reply mark beside the echo").About(k))) {
                            Say("iff_friend", "senior_operator", {}, &k);
                        }
                        break;
                    }
                    case EngagementEvent::Kind::Hit:
                    case EngagementEvent::Kind::Miss:
                        // Nobody aboard knows the outcome: the crew judges it from the signs on its instruments (AssessKills,
                        // where a seat's calls go through NpcDoes). A missile whose target was already down has nothing to show.
                        if (e.detail != "target already down") {
                            kill_.Open(time_, e.shot, e.target, e.kind == EngagementEvent::Kind::Hit, e.detail, e.view);
                        }
                        break;
                    case EngagementEvent::Kind::Track: break;  // the capture and TV-track reports are automatic (inputs)
                }
            }
            AssessKills();
            UpdateGroupPicks();
        }
        // Until something is tracked the senior operator keeps the crew up to date: the nearest echo on his scope,
        // every 15 s (the first report of each contact is spoken when it appears).
        if (engagement_on_ && engagement_.Method() == TrackMethod::None && time_ >= next_contact_update_s_ &&
            (NpcAt("senior_operator") || !SeatHolder("senior_operator").empty())) {
            const KnownTarget* nearest = nullptr;
            for (const KnownTarget& k : pictures_["senior_operator"].All()) {
                if (time_ - k.time_s > 2.0 || !k.has_range || k.friendly) continue;
                if (nearest == nullptr || k.range_m < nearest->range_m) nearest = &k;
            }
            if (nearest != nullptr) {
                KnownTarget k = *nearest;
                char r[16];
                std::snprintf(r, sizeof(r), "%.0f", k.range_m / 1000.0);
                if (NpcDoes("senior_operator", SeatDuty::Report("senior_operator", "contact", "the nearest echo, every 15 s until a track",
                                                                {{"azimuth", MilsText(k.azimuth_deg)}, {"range_km", r}}).About(k))) {
                    Say("contact", "senior_operator", {{"azimuth", MilsText(k.azimuth_deg)}, {"range_km", r}}, &k);
                    next_contact_update_s_ = time_ + 15.0;
                }
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
                    if (e.group_centre) {
                        const GroupOrder* g = FindGroupOrder(e.number);
                        Say("designation_group", Cmdr(), {{"number", n}, {"azimuth", az}, {"range_km", r}, {"height_hm", h},
                                                          {"rule", std::to_string(g != nullptr ? g->rule : 1)}});
                        continue;
                    }
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
            Radio(s.channel, who, s.ru + " (" + s.en + ")", true, CalloutHearers(s));
            log_.Record({s.start_s, DebriefEntry::Kind::Scenario, who, "says", s.ru + " (" + s.en + ")"});
            Hear(s);
        }
    }

    // ---- The battery's other launchers and the battery commander's fire distribution (roadmap 8.9, battery_fire.hpp).
    std::unique_ptr<BatteryFire> battery_fire_;
    std::mt19937 battery_fire_rng_{4099u};
    double next_distribution_s_ = 0.0;
    // A board mark with the IT-45's newer and finer place, when the target is on the tube.
    BoardMark OnTheTube(const BoardMark& m) const {
        if (!post_) return m;
        for (const AspdTrack& k : post_->Tracks()) {
            if (k.target_id != m.target_id || k.imitated || k.time_s + 1e-9 < m.time_s) continue;
            BoardMark out = m;
            out.position = k.position;
            out.time_s = k.time_s;
            return out;
        }
        return m;
    }
    bool RegimentDesignated(const std::string& target_id) const {
        if (!post_) return false;
        for (const AspdTrack& k : post_->Tracks()) {
            if (k.target_id == target_id && k.designated) return true;
        }
        return false;
    }

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
        std::vector<BoardMark> marks = post_ ? CommanderPicture() : c2_ ? c2_->Known(time_) : std::vector<BoardMark>();
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
        // The NPC battery commander: each hostile on the board or the tube that nobody has goes to the nearest ready launcher that
        // can reach it soon (our own vehicle's target is ours).
        // In the post, he does it from his board by voice on the battery net, and only while he is an NPC and the net is up.
        // A player in the commander's seat: the same reckoning, as the targets he owes the launchers (the helper).
        const bool player_commands = post_ && !SeatHolder(Cmdr()).empty();
        const bool distribute = post_ ? (NpcAt(Cmdr()) || player_commands) && BatteryNetUp() : c2_ != nullptr;
        if ((def_.battery_commander == "npc" || player_commands) && distribute && !fire_banned_ && time_ >= next_distribution_s_) {
            next_distribution_s_ = time_ + 5.0;
            // The regiment's designations to the battery first (they came with its address), then the rest.
            if (post_) {
                std::stable_sort(marks.begin(), marks.end(), [this](const BoardMark& a, const BoardMark& b) {
                    return RegimentDesignated(a.target_id) && !RegimentDesignated(b.target_id);
                });
            }
            for (const BoardMark& board_mark : marks) {
                // He reads the target off the most precise picture he has: the IT-45 (the marker's azimuth and range)
                // when it is on the tube, else the board (a mark from the air-picture net is only as good as its square).
                const BoardMark m = OnTheTube(board_mark);
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
                // A group he can't resolve takes a launcher per aircraft, one rule each, four at most (voenizdat_osa_1991 p261).
                if (post_) {
                    const std::vector<BoardMark> group = PostGroupOf(m);
                    if (group.size() > 1 && LaunchersOnGroup(group) >= std::min<int>(4, static_cast<int>(group.size()))) continue;
                }
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
                        if (NpcDoes(Cmdr(), SeatDuty::Designate(Cmdr(), best->callsign, m.number, m.target_id,
                                                                "the nearest ready launcher whose sector covers it"))) {
                            DesignateByVoice(best->callsign, m);
                        }
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
    std::string npc_dropped_kill_;                        // the last target called destroyed whose track he had dropped
    static constexpr double kBesideTheirsM = 2500.0;     // an echo this close to another launcher's target is it (sam3d)
    bool GivenElsewhere(const std::string& target) const {
        return given_elsewhere_.count(target) != 0 && reported_kills_.count(target) == 0;
    }

    // A launcher's report on the battery net (its own abstract crew's, a linked Osa's, or, in that Osa, the post's other
    // launchers'): the log, the words if the vehicle hears the net, the IT boards in the post.
    void LauncherReport(const LinkReport& r) {
        // The post hears a launcher on its net through any jamming, while the launchers can transmit (not Д. ПРИЁМ).
        const bool hears = NetHears() && (!post_ || !nets_ || ((nets_->GetsThrough("battery", time_)) && (LaunchersCanTransmit() ||
                                                                                                       r.kind == BatteryFire::Event::Kind::Assigned)));
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
            if (e.subject == target && !e.group_centre) r.number = e.number;
        }
        for (const GroupOrder& g : group_orders_) {
            if (g.pick == target) r.number = g.number;
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
        bool by_line = false;  // on a telephone line: no emission, no jamming
        int repeats = 0;       // said again for want of a read-back
        // Into a group the post can't resolve (voenizdat_osa_1991 p261): `point` is its centre, the launcher picks by the
        // rule; the group's targets as the post has them.
        bool group = false;
        int rule = 0;
        std::vector<std::string> group_ids;
    };
    // The selection rules' numbers (group_designation.hpp): the launchers' tuning in the post, else the vehicle's own.
    const GroupTuning& GroupRules() const { return def_.has_launcher_tuning ? def_.launcher_tuning.group : def_.tuning.group; }
    // The group a mark is in on the post's picture: the hostile marks linked to it by marks closer together than the post
    // can resolve, each at its newest place. The mark alone when it is single.
    std::vector<BoardMark> PostGroupOf(const BoardMark& m) const {
        std::vector<BoardMark> marks{OnTheTube(m)};
        for (const BoardMark& o : PostPicture()) {
            if (o.target_id != m.target_id && o.type != 1) marks.push_back(OnTheTube(o));
        }
        std::vector<Vec3> at;
        for (const BoardMark& k : marks) at.push_back(k.position);
        std::vector<BoardMark> out;
        for (std::size_t i : UnresolvedGroup(at, 0, GroupRules().post_unresolved_m)) out.push_back(marks[i]);
        return out;
    }
    // How many launchers the post has on a group: its targets taken, and designations into it still on their way.
    int LaunchersOnGroup(const std::vector<BoardMark>& group) const {
        int n = 0;
        for (const BoardMark& g : group) n += battery_fire_ && battery_fire_->Engaged(g.target_id) ? 1 : 0;
        for (const VoiceDesignation& d : designations_out_) {
            bool in = false;
            for (const BoardMark& g : group) in = in || std::find(d.group_ids.begin(), d.group_ids.end(), g.target_id) != d.group_ids.end();
            n += d.group && in ? 1 : 0;
        }
        return n;
    }
    int designations_lost_ = 0;
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
        if (post_ && voice_ && !SetOfNet("battery").empty()) return CanTalkOnNet(Cmdr(), "battery").empty();
        return post_ && (console_.post_battery_net.empty() || console_.equipment.IsReady(console_.post_battery_net));
    }
    // Does the player's vehicle hear the battery net's reports? (Always, unless it is the post with its set down.)
    bool NetHears() const {
        if (!voice_) return !post_ || BatteryNetUp();
        return !BatteryNetHearers().empty();
    }
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
    // A track on the IT-45 as the commander reads it: its ASPD-U number, its place and height off the tube.
    static BoardMark TubeMark(const AspdTrack& k) {
        BoardMark m;
        m.target_id = k.target_id;
        m.number = k.number;
        m.time_s = k.time_s;
        m.position = k.position;
        m.height_hm = static_cast<int>(std::lround(k.height_m / 100.0));
        m.type = k.type;
        return m;
    }
    // The commander's picture: the board (what the plotter wrote from the air-picture net), and the tube's tracks the
    // board does not carry (he sits at the IT-45, rwd_mb3). Not the imitated echo, not the ones he heard destroyed.
    std::vector<BoardMark> PostPicture() const {
        std::vector<BoardMark> out = PostMarks();
        if (!post_) return out;
        for (const AspdTrack& k : post_->Tracks()) {
            if (k.imitated || reported_kills_.count(k.target_id) != 0) continue;
            bool on_board = false;
            for (const BoardMark& m : out) on_board = on_board || m.target_id == k.target_id;
            if (!on_board) out.push_back(TubeMark(k));
        }
        return out;
    }
    // One designation read out on the battery net: number, azimuth and range from the launcher (flak11: by voice,
    // azimuth and range), one message at a time (the board's operator delay and message time).
    // A mark in a group his picture can't resolve goes as the group's centre and a selection rule (voenizdat_osa_1991
    // p261): `rule` 1-4, 0 none (the crew chief chooses), -1 the next one no launcher has on that group yet.
    bool DesignateByVoice(const std::string& callsign, const BoardMark& m, int repeats = 0, int rule = -1) {
        if (!battery_fire_) return false;
        const BatteryFire::Shooter* sh = battery_fire_->Find(callsign);
        if (sh == nullptr || sh->missiles <= 0) return false;
        // On a line laid to the launcher with its lever up: no emission, no jamming (an NPC commander raises it when the
        // net is jammed out or the launcher is off it). Else on the battery net: his box on FuG-4, the set transmitting.
        const LineDef* line = voice_ ? LineToLauncher(callsign) : nullptr;
        if (line != nullptr && NpcAt(Cmdr()) && nets_ && (nets_->Jam("battery", time_) >= 2 || !nets_->MemberOnNet("battery", callsign))) {
            const VoiceBoxDef* b = voice_->BoxOf(Cmdr());
            if (b != nullptr && static_cast<std::size_t>(line->line) <= b->lines.size()) {
                for (std::size_t k = 0; k < b->lines.size(); ++k) console_.panel.Set(b->lines[k], static_cast<int>(k) + 1 == line->line ? 1 : 0);
                voice_->Update(console_);
            }
        }
        const bool by_line = line != nullptr && voice_->HearsLine(Cmdr(), line->line);
        if (!by_line && !BatteryNetUp()) {
            if (voice_) last_refusal_ = CanTalkOnNet(Cmdr(), "battery");
            return false;
        }
        const PlottingBoardDef& bd = post_->Board().Def();
        const double start = std::max(time_, battery_net_free_s_) + bd.operator_delay_s;
        // Under jamming a command is said twice (rrs_1986).
        const bool twice = !by_line && nets_ && nets_->Jam("battery", time_) > 0;
        battery_net_free_s_ = start + bd.message_s * (twice ? 2.0 : 1.0);
        // He reads the number off the board and the place off the IT-45: its latest fix, led along its speed vector to
        // when the launcher will have it (the tube shows the vector, ru_pu12_text). Not on the tube: the board's mark.
        // A group: the centre of its marks, each led on the same way.
        auto led = [&](const BoardMark& b) {
            for (const AspdTrack& k : post_->Tracks()) {
                if (k.target_id == b.target_id) return k.position + k.velocity * (battery_net_free_s_ - k.time_s);
            }
            return b.position;
        };
        const std::vector<BoardMark> group = PostGroupOf(m);
        const bool grouped = group.size() > 1;
        if (grouped && rule < 0) rule = std::min(4, LaunchersOnGroup(group) + 1);
        if (!grouped) rule = 0;
        BoardMark at = m;
        at.position = Vec3{};
        for (const BoardMark& g : group) at.position = at.position + led(g) / static_cast<double>(group.size());
        at.group_centre = grouped;
        at.rule = rule;
        VoiceDesignation out{callsign, m, at.position, battery_net_free_s_, by_line, repeats};
        out.group = grouped;
        out.rule = rule;
        for (const BoardMark& g : group) out.group_ids.push_back(g.target_id);
        designations_out_.push_back(out);
        if (ExternalLauncher(callsign)) linked_out_.push_back({callsign, at, start, battery_net_free_s_});
        const PlottingBoard::Designation d = PlottingBoard::Designate(at, sh->position);
        char n[16], r[16], az[16];
        std::snprintf(n, sizeof(n), "%04d", m.number);
        std::snprintf(r, sizeof(r), "%.0f", d.range_m / 1000.0);
        std::snprintf(az, sizeof(az), "%03.0f", std::fmod(d.azimuth_deg + 360.0, 360.0));  // degrees on the battery net (bguir_tsp)
        // The words: "цель 6201", or "группа 6201, центр" with ", правило 2" after the range.
        const std::string what = grouped ? std::string("группа ") + n + ", центр" : std::string("цель ") + n;
        const std::string what_en = grouped ? std::string("group ") + n + ", centre" : std::string("target ") + n;
        const std::string rule_ru = grouped && rule > 0 ? ", " + RuleWordsRu(rule) : std::string();
        const std::string rule_en = grouped && rule > 0 ? ", " + RuleWordsEn(rule) : std::string();
        if (by_line) {
            const std::string label = "line " + std::to_string(line->line);
            Radio(label, "Battery commander", callsign + ", я " + OwnCallSignOn("battery") + ": " + what + ", азимут " + az + ", дальность " + r +
                                                  rule_ru + " — уничтожить!");
        } else {
            if (callouts_) {
                callouts_->Say(time_, "order_assign", Cmdr(), {{"callsign", callsign}, {"own", OwnCallSignOn("battery")}, {"number", n},
                                                                {"target", what}, {"target_en", what_en}, {"rule", rule_ru},
                                                                {"rule_en", rule_en}, {"azimuth", az}, {"range_km", r}});
            }
            Transmitted("battery", "Battery commander", bd.message_s * (twice ? 2.0 : 1.0));
        }
        log_.Record({time_, DebriefEntry::Kind::Scenario, "Battery commander", callsign,
                     (grouped ? std::string("group ") + n + " (" + std::to_string(group.size()) + " marks; its centre, rule " +
                                    std::to_string(rule) + ")"
                              : std::string("target ") + n) +
                         (by_line ? " by line" : twice ? " by voice, said twice" : " by voice") + ": azimuth " + az + ", range " + r + " km"});
        ++designations_made_;
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
            // The launcher hears it only on the battery net's present frequency and through any jamming (a line always).
            if (!d.by_line && nets_ && def_.radio_data.Find("battery") != nullptr &&
                (!nets_->MemberOnNet("battery", d.callsign) || !nets_->GetsThrough("battery", time_, nets_->Jam("battery", time_) > 0))) {
                ++designations_lost_;
                Event(d.callsign, "the designation did not get through (jammed, or off the battery net)");
                // No read-back: the NPC commander says it again (twice more at most).
                if (d.repeats < 2 && reported_kills_.count(d.mark.target_id) == 0 &&
                    NpcDoes(Cmdr(), SeatDuty::Designate(Cmdr(), d.callsign, d.mark.number, d.mark.target_id, "no read-back: say it again"))) {
                    DesignateByVoice(d.callsign, d.mark, d.repeats + 1, d.group ? d.rule : -1);
                }
                continue;
            }
            const BatteryFireTarget* best = nullptr;
            if (d.group) {
                // A group's centre: its crew counts the group on its own screen by the rule (group_designation.hpp).
                const BatteryFire::Shooter* sh = battery_fire_->Find(d.callsign);
                const Vec3 from = sh != nullptr ? sh->position : def_.unit_position;
                auto echo = [&](const std::string& id, const Vec3& p) {
                    const Vec3 rel = p - from;
                    return GroupEcho{id, RadToDeg(std::atan2(rel.x, rel.y)), rel.HorizontalLength()};
                };
                std::vector<GroupEcho> echoes;
                for (const BatteryFireTarget& t : targets) {
                    if (t.alive && !t.friendly) echoes.push_back(echo(t.id, t.position));
                }
                const GroupEcho c = echo(std::string(), d.point);
                const std::string pick = SelectByRule(echoes, c.azimuth_deg, c.range_m, d.rule, GroupRules());
                for (const BatteryFireTarget& t : targets) best = t.id == pick ? &t : best;
                if (best != nullptr) Event(d.callsign, "takes " + best->id + " in the group by rule " + std::to_string(d.rule));
            } else {
                double best_d = kDesignationSearchM;
                for (const BatteryFireTarget& t : targets) {
                    if (!t.alive || t.friendly) continue;
                    const double dist = (Vec3{t.position.x, t.position.y, 0.0} - Vec3{d.point.x, d.point.y, 0.0}).Length();
                    if (dist < best_d) {
                        best_d = dist;
                        best = &t;
                    }
                }
            }
            if (best == nullptr) {
                Event(d.callsign, "no target where it was designated");
                if (callouts_) callouts_->Say(time_, "report_no_target", "battery", {{"callsign", d.callsign}});
                continue;
            }
            if (best->id != d.mark.target_id && !d.group) Event(d.callsign, "takes " + best->id + ", not the designated " + d.mark.target_id);
            battery_fire_->Assign(d.callsign, best->id, d.mark.number, time_);
            // Its read-back (rrs_1986): heard at the post if the launcher can transmit and the post hears the net.
            char rn[16];
            std::snprintf(rn, sizeof(rn), "%04d", d.mark.number);
            if (d.by_line) {
                Radio("line " + std::to_string(LineToLauncher(d.callsign)->line), d.callsign,
                      OwnCallSignOn("battery") + ", я " + d.callsign + ": цель " + rn + ", понял.");
                ++readbacks_heard_;
            } else if (LaunchersCanTransmit() && NetHears() && (!nets_ || nets_->GetsThrough("battery", time_))) {
                if (callouts_) callouts_->Say(time_, "readback", "battery", {{"post", OwnCallSignOn("battery")}, {"callsign", d.callsign}, {"number", rn}}, 1.0);
                ++readbacks_heard_;
            } else {
                ++readbacks_missed_;
                Event(d.callsign, std::string("read back ") + rn + ": not heard at the post");
            }
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
    // A target in a group the post can't resolve goes as the group's centre and a selection rule (`rule` 1-4, 0 none: the
    // crew chief chooses; -1: the next rule no launcher has on that group yet).
    bool DesignateBatteryTarget(const std::string& callsign, int number, int rule = -1) {
        if (!post_) return false;
        for (const BoardMark& m : PostMarks()) {
            if (m.number == number) return DesignateByVoice(callsign, m, 0, rule);
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
        for (const CommandPost::AirReport& r : post_->Heard()) f.heard.push_back(r.number);
        f.command = post_command_;
        f.command_text = PostCommandText();
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
    bool DesignateFromTube(const std::string& callsign, int aspd_number, int rule = -1) {
        if (!post_) return false;
        const AspdTrack* track = nullptr;
        for (const AspdTrack& t : post_->Tracks()) {
            if (t.number == aspd_number) track = &t;
        }
        if (track == nullptr || reported_kills_.count(track->target_id) != 0) return false;
        for (const BoardMark& m : PostMarks()) {
            if (m.target_id == track->target_id) return DesignateByVoice(callsign, m, 0, rule);
        }
        return DesignateByVoice(callsign, TubeMark(*track), 0, rule);
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
    // A player's plot at a point of the board through his seat: only the plotter's.
    bool SeatPlotAt(const std::string& seat, int number, double east_m, double north_m, double metres_per_board_m, std::string* why = nullptr) {
        if (!post_ || seat != console_.post_plotter) {
            if (why) *why = "the board is the " + console_.post_plotter + "'s";
            return false;
        }
        const bool ok = PlayerPlotAt(number, east_m, north_m, metres_per_board_m).ok;
        if (!ok && why) *why = "the board folded, no report heard with that number, or the last mark still being written";
        return ok;
    }
    // A player's plot through his seat: only the plotter's.
    bool SeatPlot(const std::string& seat, int board_number, std::string* why = nullptr) {
        if (!post_ || seat != console_.post_plotter) {
            if (why) *why = "the board is the " + console_.post_plotter + "'s";
            return false;
        }
        const bool ok = PlayerPlot(board_number);
        if (!ok && why) *why = "no report heard with that number, or the last mark is still being written";
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

    // ---- The kill assessment (kill_assessment.hpp, voenizdat_osa_1991 p267): each seat calls the signs its instruments
    // show of a missile's target; the judge calls the result from what he has seen and heard. What the crew knows of a
    // kill, its report to the battery and its next move follow his call, right or wrong.
    KillAssessment kill_;
    std::map<std::string, double> own_kills_;  // targets our judge called destroyed -> when (looked for again on the scope)
    int kill_calls_ = 0, kill_calls_wrong_ = 0, still_flying_ = 0;
    // The seat whose instrument shows a sign: the range indicator the range wheel's, the search scope the azimuth
    // wheel's, the elevation dial, the TV and the launch console's lamps the elevation wheel's (flak11, voenizdat_osa_1991 p17).
    std::string KillSeat(KillInstrument where) const {
        switch (where) {
            case KillInstrument::RangeIndicator: return def_.tuning.aim_range_role;
            case KillInstrument::SearchScope: return def_.tuning.aim_az_role;
            case KillInstrument::LaunchLamps:
            case KillInstrument::ElevationDial:
            case KillInstrument::Tv: return def_.tuning.aim_el_role;
        }
        return def_.tuning.aim_el_role;
    }
    // Who calls the result: the tuning's judge (the deputy commander) when an NPC sits there, else the commander; nobody
    // when players hold both (they judge for themselves).
    std::string KillJudge() {
        if (NpcAt(def_.tuning.kill.judge_role)) return def_.tuning.kill.judge_role;
        return NpcAt(Cmdr()) ? Cmdr() : std::string();
    }
    // What the instruments show of a target now.
    KillLook KillLookOf(const std::string& id) {
        KillLook l;
        l.tracked = engagement_.Method() != TrackMethod::None && engagement_.TrackedTarget() == id;
        l.on_range = l.tracked && engagement_.Method() == TrackMethod::Radar;
        for (const Contact& c : engagement_.TrackReturns()) l.on_range = l.on_range || c.target_id == id;
        l.elevation_deg = engagement_.AntennaElevationDeg();
        for (const Contact& c : engagement_.SearchPicture()) l.on_scope = l.on_scope || c.target_id == id;
        const TrackSensorDef& d = console_.track;
        l.on_tv = l.tracked && !d.tv_camera.empty() && console_.equipment.IsReady(d.tv_camera) && engagement_.TvLook(console_, id).seen;
        return l;
    }
    double PerceptionS(const std::string& station) {
        for (std::size_t i = 0; i < crew_.size() && i < def_.crew.size(); ++i) {
            if (crew_[i].Role() != station) continue;
            if (const SkillTier* t = SkillOf(def_.crew[i])) return t->perception_s * (crew_[i].Stressed() ? t->stress_factor : 1.0);
        }
        return 0.5;
    }
    void AssessKills() {
        kill_.Observe(time_, [this](const std::string& id) { return KillLookOf(id); });
        const std::string judge = KillJudge();
        for (const KillAssessment::Sign& s : kill_.TakeSigns()) {
            const std::string seat = KillSeat(s.where);
            log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(seat), s.target,
                         std::string("sees ") + KillSignText(s.sign) + " (" + ToString(s.where) + ", missile " + std::to_string(s.shot) + ")"});
            if (seat == judge) kill_.Learn(s.shot, s.sign, time_ + PerceptionS(judge));  // his own instrument
            SeatDuty d = SeatDuty::Report(seat, KillSignCallout(s.sign), std::string("a sign of the shot on his instrument: ") + KillSignText(s.sign));
            d.target = s.target;
            if (NpcDoes(seat, std::move(d))) Say(KillSignCallout(s.sign), seat);
        }
        double wrong = def_.tuning.kill.ambiguous_wrong_chance;
        if (const SkillTier* t = judge.empty() ? nullptr : TierAt(judge)) wrong += t->branch_error_chance;
        for (const KillAssessment::Judgement& j : kill_.Judge(time_, wrong < 1.0 ? wrong : 1.0)) {
            ++kill_calls_;
            if (j.wrong) ++kill_calls_wrong_;
            std::string by;
            for (KillSign k : j.known) by += (by.empty() ? "" : ", ") + std::string(KillSignId(k));
            std::string text = std::string("judges missile ") + std::to_string(j.shot) + ": " + KillAssessment::ToString(j.call) + " (" +
                               (by.empty() ? std::string("no sign") : by) + (j.settled ? "" : "; the signs settle nothing") + ")";
            if (j.wrong) text += j.hit ? " - WRONG: it was hit" : " - WRONG: it was missed";
            if (judge.empty()) text += "; the players judge for themselves";
            log_.Record({time_, DebriefEntry::Kind::Scenario, judge.empty() ? std::string("Crew") : SpeakerName(judge), j.target, text});
            if (judge.empty()) continue;
            if (j.call == KillAssessment::Call::Hit) {
                Say("hit", judge);
                HeardDestroyed(j.target);
                own_kills_[j.target] = time_;
                OwnReport(BatteryFire::Event::Kind::Hit, j.target);
            } else if (j.call == KillAssessment::Call::Miss) {
                Say("miss", judge);
                OwnReport(BatteryFire::Event::Kind::Miss, j.target);
            } else {
                Say("result_unknown", judge);
            }
        }
        // A target called destroyed still on the search scope a while later: it flies on (the call was wrong).
        const std::string scope = def_.tuning.aim_az_role;
        for (auto it = own_kills_.begin(); it != own_kills_.end();) {
            if (time_ - it->second < def_.tuning.kill.recheck_s) {
                ++it;
                continue;
            }
            for (const Contact& c : engagement_.SearchPicture()) {
                if (c.target_id != it->first) continue;
                KnownTarget k;
                k.target = c.target_id;
                k.has_azimuth = k.has_range = true;
                k.azimuth_deg = c.azimuth_deg;
                k.range_m = c.range_m;
                char r[16];
                std::snprintf(r, sizeof(r), "%.0f", c.range_m / 1000.0);
                const std::map<std::string, std::string> words{{"azimuth", MilsText(c.azimuth_deg)}, {"range_km", r}};
                if (!NpcDoes(scope, SeatDuty::Report(scope, "target_still_flying", "a target called destroyed still on his scope", words).About(k))) continue;
                Say("target_still_flying", scope, words, &k);
                ++still_flying_;
            }
            it = own_kills_.erase(it);
        }
    }
    // ---- A group's centre and a selection rule from the battery post (voenizdat_osa_1991 p261, group_designation.hpp):
    // the senior operator counts the group on his scope and reports the aircraft the rule picks; the commander takes it.
    struct GroupOrder {
        int number = 0;
        int rule = 1;
        double azimuth_deg = 0.0;  // the centre, true azimuth from the vehicle
        double range_m = 0.0;
        double heard_s = 0.0;      // the commander heard it on the radio
        double told_s = -1.0;      // the senior operator heard it passed on (-1: not yet)
        double next_look_s = 0.0;  // he looks again (the group not on his scope yet)
        bool reported = false;
        double reported_s = 0.0;
        std::string pick;          // the aircraft the commander knows was picked (the report, or his own count)
    };
    std::vector<GroupOrder> group_orders_;
    static constexpr double kGroupPickWaitS = 15.0;  // the commander waits this long for a count not coming, then counts himself (sam3d)
    static constexpr double kGroupLookS = 120.0;     // the senior operator gives up looking for the group (sam3d)
    GroupOrder* FindGroupOrder(int number) {
        for (auto g = group_orders_.rbegin(); g != group_orders_.rend(); ++g) {
            if (g->number == number) return &*g;
        }
        return nullptr;
    }
    static std::vector<GroupEcho> EchoesOf(const std::vector<KnownTarget>& known, const EngagementModel& eng) {
        std::vector<GroupEcho> out;
        for (const KnownTarget& k : known) {
            if (!k.has_azimuth || !k.has_range) continue;
            out.push_back({k.target, k.true_azimuth ? k.azimuth_deg : eng.IndicatedToTrueDeg(k.azimuth_deg), k.range_m});
        }
        return out;
    }
    void UpdateGroupPicks() {
        const std::string so = def_.tuning.aim_az_role;
        for (GroupOrder& g : group_orders_) {
            if (g.reported || g.told_s < 0.0 || (MemberAt(so) == nullptr && SeatHolder(so).empty()) || time_ < g.next_look_s ||
                time_ - g.told_s > kGroupLookS) {
                continue;
            }
            std::vector<GroupEcho> echoes;
            for (const Contact& c : engagement_.SearchPicture()) {
                if (!engagement_.KnownFriend(c.target_id)) echoes.push_back({c.target_id, engagement_.IndicatedToTrueDeg(c.azimuth_deg), c.range_m});
            }
            const std::string id = SelectByRule(echoes, g.azimuth_deg, g.range_m, g.rule, def_.tuning.group);
            if (id.empty()) {
                g.next_look_s = time_ + 5.0;  // not on his scope yet
                continue;
            }
            g.reported = true;
            g.reported_s = time_;
            for (const Contact& c : engagement_.SearchPicture()) {
                if (c.target_id != id) continue;
                KnownTarget k;
                k.target = id;
                k.number = g.number;
                k.has_azimuth = k.has_range = true;
                k.azimuth_deg = c.azimuth_deg;
                k.range_m = c.range_m;
                char r[16];
                std::snprintf(r, sizeof(r), "%.0f", c.range_m / 1000.0);
                const std::map<std::string, std::string> words{{"rule", std::to_string(g.rule)}, {"azimuth", MilsText(c.azimuth_deg)}, {"range_km", r}};
                // A player at the scope owes the count; the commander counts himself if it doesn't come (kGroupPickWaitS).
                if (!NpcDoes(so, SeatDuty::Report(so, "group_pick", "a group's centre and rule passed on: count it on the scope", words).About(k))) continue;
                Say("group_pick", so, words, &k);
                log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(so), id,
                             "counts the group " + std::to_string(g.number) + " on his scope: rule " + std::to_string(g.rule) + " picks this one"});
            }
        }
    }
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
        if (s.id == "air_report" && post_) {  // read out to the plotter: it waits for his hand now
            post_->ReleaseHeld(s.about.number, time_);
            reading_out_.erase(s.about.number);
            return;
        }
        if (s.channel != "intercom" && s.channel != "cabin") return;
        for (std::size_t i = 0; i < crew_.size(); ++i) {
            CrewMember& c = crew_[i];
            if (c.Role() == s.from) continue;
            if (!HeardBy(s, c.Role())) continue;
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
            KillSign sign = KillSign::Burst;
            if (KillSignOfCallout(s.id, sign) && c.Role() == KillJudge()) kill_.LearnHeard(sign, time_);
            if (s.id == "designation_group" && c.Role() == def_.tuning.aim_az_role) {
                for (GroupOrder& g : group_orders_) {
                    if (g.told_s < 0.0) g.told_s = time_;  // passed on in the order they came
                }
            }
            if (s.id == "group_pick" && s.has_about && c.Role() == Cmdr()) {
                for (GroupOrder& g : group_orders_) {
                    if (g.number == s.about.number && g.pick.empty()) g.pick = s.about.target;
                }
            }
            if (s.id == "target_still_flying" && s.has_about && c.Role() == Cmdr() && reported_kills_.erase(s.about.target) != 0) {
                Event(s.about.target, "not destroyed after all: back in the commander's picture");
                npc_dropped_kill_.clear();
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
    // A paper kept in the cabin (a cabin display of kind "document"), line by line as typed, in Russian as issued.
    // "radio_data": the radiotelephonist's extract of the radio data on its form (rrs_1986, vst_1988: each net's
    // frequencies by number, the call signs, the times they change, the mode of communication, where needed the azimuth
    // to a correspondent), with the telecode channel and the RTK-1's lines. "talk_tables": the commander's ПТ-1 and the
    // signal table ПТ-2 (bstudy_tactics' form: a group, its meaning, a signal's visible sign). Times are counted from the
    // exercise's start (Ч+minutes): the scenarios keep no clock.
    std::vector<std::string> DocumentText(const std::string& document) const {
        std::vector<std::string> out;
        auto number = [](double v, int decimals) {
            char b[32];
            std::snprintf(b, sizeof(b), "%.*f", decimals, v);
            std::string s(b);
            for (char& c : s) {
                if (c == '.') c = ',';  // the decimal comma
            }
            return s;
        };
        if (document == "talk_tables") {
            for (const TalkTable& t : def_.talk_tables.tables) {
                bool signals = false;
                for (const TalkEntry& e : t.entries) signals = signals || !e.visual.empty();
                out.push_back(std::string(signals ? "ТАБЛИЦА СИГНАЛОВ " : "ПЕРЕГОВОРНАЯ ТАБЛИЦА ") + t.name_ru);
                for (const TalkEntry& e : t.entries) {
                    std::string line = "  " + e.code + " — " + (signals ? "«" + e.ru + "»" : e.ru);
                    if (!e.visual.empty()) line += " — " + e.visual;
                    out.push_back(line);
                }
                out.push_back(std::string());
            }
            if (!out.empty()) out.pop_back();
            return out;
        }
        if (document != "radio_data") return out;
        out.push_back("РАДИОДАННЫЕ");
        for (const RadioNetDef& n : def_.radio_data.nets) {
            out.push_back(UpperFirst(n.name_ru));
            std::string f = "  частоты: ";
            for (std::size_t i = 0; i < n.frequencies_mhz.size(); ++i) {
                f += (i > 0 ? "; № " : "№ ") + std::to_string(i + 1) + " — " + number(n.frequencies_mhz[i], 3);
            }
            out.push_back(f + " МГц");
            if (n.main == "self") {
                std::string members;
                if (battery_fire_) {
                    for (const BatteryFire::Shooter& s : battery_fire_->Shooters()) members += (members.empty() ? "" : ", ") + s.callsign;
                }
                out.push_back("  позывные: свой — " + n.own_call_sign + " (главная)" + (members.empty() ? std::string() : "; " + members));
            } else {
                out.push_back("  позывные: свой — " + n.own_call_sign + "; главная — " + n.main_call_sign);
            }
            for (const RadioNetDef::Change& c : n.changes) {
                out.push_back("  смена: Ч+" + std::to_string(static_cast<int>(c.at_s / 60.0)) + " — на № " + std::to_string(c.to));
            }
            std::string mode = "  вид связи: телефония";
            if (n.main == "regiment" && def_.post_regiment.has_position) {
                const Vec3 d = def_.post_regiment.position - def_.unit_position;
                double az = RadToDeg(std::atan2(d.x, d.y));
                if (az < 0.0) az += 360.0;
                mode += "; азимут на КП полка " + std::to_string(static_cast<int>(std::lround(az)) % 360) + "°";
            }
            out.push_back(mode);
        }
        if (def_.radio_plan.telecode_hz > 0.0) {
            out.push_back("Телекодовый канал (Р-407): приём " + number(def_.radio_plan.telecode_hz / 1.0e6, 1) + ", передача " +
                          number(def_.radio_plan.telecode_return_hz / 1.0e6, 1) + " МГц");
        }
        std::string lines;
        for (const LineDef& l : def_.radio_data.lines) {
            if (!l.laid) continue;
            const std::string to = l.to == "regiment" ? std::string("КП полка") : l.to == "radar" ? std::string("РЛР") : l.to;
            lines += (lines.empty() ? "" : "; ") + std::to_string(l.line) + " — " + to;
        }
        if (!lines.empty()) out.push_back("Линии (РТК-1): " + lines);
        return out;
    }
    // A Russian name with its first letter capitalised (the Cyrillic lower case а-я, as UTF-8).
    static std::string UpperFirst(std::string s) {
        if (s.size() >= 2) {
            unsigned char a = static_cast<unsigned char>(s[0]), b = static_cast<unsigned char>(s[1]);
            if (a == 0xD0 && b >= 0xB0 && b <= 0xBF) {
                s[1] = static_cast<char>(b - 0x20);  // а-п
            } else if (a == 0xD1 && b >= 0x80 && b <= 0x8F) {
                s[0] = static_cast<char>(0xD0);      // р-я
                s[1] = static_cast<char>(b + 0x20);
            }
        }
        return s;
    }
    // A report's words as the player's menu shows them (Russian, English): the blanks his seat can fill filled (the
    // report he would read out to the plotter, his seat's number), any other blank «…».
    std::pair<std::string, std::string> ReportWords(const std::string& station, const std::string& callout_id) const {
        const CalloutDef* c = def_.crew_data.FindCallout(callout_id);
        if (c == nullptr) return {std::string(), std::string()};
        std::map<std::string, std::string> f;
        if (callout_id == "air_report") {
            f["report"] = post_ && !post_->Held().empty() ? post_->Held().front().text : std::string("(нет донесения / no report waiting)");
        }
        if (const PlotterEvent* e = PendingPlot(callout_id)) f = e->fields;
        if (callout_id == "seat_ready") {
            static const char* const words[3][3] = {{"deputy_commander", "Первый", "1"}, {"senior_operator", "Второй", "2"}, {"operator", "Третий", "3"}};
            for (const auto& w : words) {
                if (station != w[0]) continue;
                f["seat_number"] = w[1];
                f["seat_number_en"] = w[2];
            }
        }
        if ((callout_id == "readback" || callout_id == "report_ready") && c2_ && station == Cmdr()) f = NetReportFields(callout_id);
        auto blanks = [](std::string t) {
            for (std::size_t a = t.find('{'); a != std::string::npos; a = t.find('{', a)) {
                const std::size_t b = t.find('}', a);
                if (b == std::string::npos) break;
                t.replace(a, b - a + 1, "…");
            }
            return t;
        };
        return {blanks(FillCallout(c->ru, f)), blanks(FillCallout(c->en, f))};
    }
    // The plotter's reports to the commander (art. 23): due, made, and a player's left too late.
    int PlotReportsDue() const { return plot_reports_due_; }
    int PlotReportsMade() const { return plot_reports_made_; }
    int PlotReportsMissed() const { return plot_reports_missed_; }
    // When the commander learned of a board target from the plotter's report (-1: not from him).
    double BoardTargetToldS(const std::string& target_id) const {
        const auto it = board_known_s_.find(target_id);
        return it != board_known_s_.end() ? it->second : -1.0;
    }
    // The NPC commander's picture of the air (the post's board as the plotter has called it, and the tube).
    std::vector<BoardMark> CommanderPictureNow() const { return CommanderPicture(); }
    bool PlayerSay(const std::string& station, const std::string& callout_id) {
        if (!callouts_ || def_.crew_data.FindCallout(callout_id) == nullptr) return false;
        if (callout_id == "seat_ready") {  // «Первый (второй, третий) готов» (voenizdat_osa_1991 p. 247)
            static const char* const words[3][3] = {{"deputy_commander", "Первый", "1"}, {"senior_operator", "Второй", "2"}, {"operator", "Третий", "3"}};
            for (const auto& w : words) {
                if (station != w[0]) continue;
                if (!voice_ || !voice_->HearsIntercom(station)) {
                    last_refusal_ = "your headset is not on the intercom";
                    return false;
                }
                if (!callouts_->Say(time_, callout_id, station, {{"seat_number", w[1]}, {"seat_number_en", w[2]}})) return false;
                intercom_checked_.insert(station);
                reports_made_.push_back({time_, station, callout_id});
                return true;
            }
            return false;
        }
        if (callout_id == "plot_new" || callout_id == "plot_turn" || callout_id == "plot_lost") {  // the plotter's art. 23 reports
            if (station != console_.post_plotter) return false;
            for (auto it = plotter_events_.begin(); it != plotter_events_.end(); ++it) {
                if (it->callout != callout_id) continue;
                const PlotterEvent e = *it;
                plotter_events_.erase(it);
                PlotterSays(e);
                reports_made_.push_back({time_, station, callout_id});
                return true;
            }
            last_refusal_ = "nothing of that kind to report";
            return false;
        }
        if (callout_id == "air_report") {  // the radiotelephonist reads the oldest report he heard to the plotter
            if (!PlayerReadOutAirReport(station)) return false;
            reports_made_.push_back({time_, station, callout_id});
            return true;
        }
        if ((callout_id == "readback" || callout_id == "report_ready") && c2_) return PlayerOnBatteryNet(station, callout_id);
        const Contact* nearest = nullptr;
        if (callout_id == "contact" && engagement_on_) {
            for (const Contact& c : engagement_.SearchPicture()) {
                if (nearest == nullptr || c.range_m < nearest->range_m) nearest = &c;
            }
            bool owes = false;  // the echo between two sweeps: what he would have said of it a moment ago stands
            for (const SeatDuty& d : owed_) owes = owes || (d.kind == SeatDuty::Kind::Report && d.seat == station && d.id == callout_id);
            if (nearest == nullptr && !owes) return false;  // nothing on his scope to report
        }
        if (nearest != nullptr) {
            KnownTarget k;
            k.target = nearest->target_id;
            k.has_azimuth = k.has_range = true;
            k.azimuth_deg = nearest->azimuth_deg;
            k.range_m = nearest->range_m;
            char r[16];
            std::snprintf(r, sizeof(r), "%.0f", nearest->range_m / 1000.0);
            callouts_->Say(time_, callout_id, station, {{"azimuth", MilsText(nearest->azimuth_deg)}, {"range_km", r}}, 0.0, &k);
            reports_made_.push_back({time_, station, callout_id});
            if (station == "senior_operator") next_contact_update_s_ = time_ + 15.0;  // the crew's last update: just now
            SettleOwedReport(station, callout_id, k.target);
            return true;
        }
        // A report the seat owes says what its NPC would have said, and tells the crew what his would have (the helper's
        // duty: the target, its IFF reply). The oldest owed first.
        const SeatDuty* owed = nullptr;
        for (const SeatDuty& d : owed_) {
            if (owed == nullptr && d.kind == SeatDuty::Kind::Report && d.seat == station && d.id == callout_id) owed = &d;
        }
        std::string about_target;
        if (owed != nullptr) {
            const std::map<std::string, std::string> fields = owed->fields;
            const KnownTarget about = owed->about;
            const bool has_about = owed->has_about;
            about_target = owed->target;
            if (!callouts_->Say(time_, callout_id, station, fields, 0.0, has_about ? &about : nullptr)) return false;
        } else if (!callouts_->Say(time_, callout_id, station)) {
            return false;
        }
        reports_made_.push_back({time_, station, callout_id});
        if (callout_id == "contact" && station == "senior_operator") next_contact_update_s_ = time_ + 15.0;
        SettleOwedReport(station, callout_id, about_target);
        return true;
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
