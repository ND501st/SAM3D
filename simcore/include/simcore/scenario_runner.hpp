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
//
// The presentation feed (fx/events.hpp, docs/fx/README.md 3.1): where the models decide (our launch and the shot's end,
// an aircraft destroyed, the neighbours' and the battery's launchers' missiles, an ARM's burst) the runner pushes an
// event for the game to show and sound. Nothing in the simulation reads it; its few draws (where a burst is, whether a
// wreck burns) come from a generator of its own, so the simulation's random sequence is the same with it or without.
#pragma once

#include <cmath>
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
#include "simcore/air_war/test_waves.hpp"
#include "simcore/console_data.hpp"
#include "simcore/crew_comms.hpp"
#include "simcore/debrief.hpp"
#include "simcore/em_link.hpp"
#include "simcore/engagement.hpp"
#include "simcore/fuel.hpp"
#include "simcore/fx/events.hpp"
#include "simcore/instructor.hpp"
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
#include "simcore/radio_journal.hpp"
#include "simcore/radio_setup.hpp"
#include "simcore/command/people.hpp"
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
            e.span_m = t.span_m;
            eng_targets_.push_back(e);
        }
        flights_ = std::make_unique<Flights>();
        RegisterFlights(0);
        UpdateLight();  // the sun at second 0 (roadmap R7): the TV's daylight from here, the battery's night below
        kill_.Configure(def_.tuning.kill, def_.seed * 6007u + 3u);
        fx_rng_.seed(def_.seed + kFxSeed);  // the presentation's own draws (fx/events.hpp): never a model's generator
        rwr_warned_.assign(def_.targets.size(), 0);
        fired_.assign(def_.triggers.size(), false);
        deliveries_.Set(def_.deliveries);
        engagement_.SetUnitHeading(def_.unit_heading_deg);
        if (console_.has_orientation) {  // roadmap 6.5: the gyro's drift at this site, the crew chief's compass work
            const OrientationDef& o = console_.orientation;
            engagement_.SetSiteLatitude(RadToDeg(def_.origin.lat_rad));
            const bool east = ((def_.seed * 2654435761u) >> 11) & 1u;  // the sign of this gyro's own drift, per seed
            engagement_.SetGyroOwnDrift(east ? o.own_drift_deg_h : -o.own_drift_deg_h);
            compass_ = CompassSurvey(o.survey_procedure.empty() ? o.survey_s : 0.0, o.survey_sigma_deg, def_.seed * 40503u + 977u);
        }
        // A radar to cable to the post is a fact of the position, there before any readiness (its cable laid in "deployed").
        if (!console_.post_radar_here.empty()) console_.equipment.SetInput(console_.post_radar_here, def_.has_post_radar);
        if (ready_state != nullptr) ApplyReadiness(*ready_state);
        for (const auto& c : def_.initial_controls) console_.panel.Set(c.first, c.second);
        StartLines();
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
        if (def_.has_post_em) {  // the cabled radar alone in an EM world of its own, standing where it stands
            post_em_ = std::make_unique<EmSensorLink>(def_.origin, &world_.GetTerrain(), def_.seed ^ 0x9e3779b9u, def_.post_em,
                                                      def_.post_radar.position);
            if (!post_em_->Ok()) Event("post radar", "the EM layer failed: the coverage model stands in");
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
            battery_->SetNight(light_.dark);
            if (def_.prepared.present && def_.prepared.emplacement) {
                battery_->SetInEmplacement(def_.prepared.values.leave_emplacement_s);
            } else if (def_.battery_deployed) {
                battery_->SetInEmplacement(0.0);  // set up where it stands, nothing dug in to climb out of
            }
            battery_orders_done_.assign(def_.battery_orders.size(), 0);
            if (def_.battery_by_means) StartOrderNet();
        }
        if (def_.nav) nav_ = std::make_unique<nav::SessionNav>(*def_.nav);  // the session's own movers
        StartSupply();
        HandInBatteryPapers();
        StartRadioTuning();
        RecordWorkPlaces();
        StartCallouts();
        StartMovers();
        StartEmission();
        log_.Record({0.0, DebriefEntry::Kind::Scenario, std::string(), def_.id, "start: " + def_.name});
    }

    ScenarioRunner(const ScenarioRunner&) = delete;
    ScenarioRunner& operator=(const ScenarioRunner&) = delete;

    void Step(double dt) {
        if (finished_) return;
        time_ += dt;
        UpdateLight();
        weather_.SetTime(time_);  // the clouds drift
        UpdateTargets(dt);
        UpdateWaves();
        DeliverOrders();
        PlayDeliveries();
        UpdateVoice();
        UpdateOsaNet();
        UpdateIntercomCheck();
        UpdateBattery(dt);
        StepNav(dt);
        UpdateEmission(dt);
        UpdateNeighbourFire(dt);
        UpdateC2();
        UpdateRadios();
        UpdatePost();
        UpdateRadioTuning(dt);
        UpdateCompass();
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
        UpdateArms();
        UpdateFuel(dt);
        UpdatePositionPower();
        EvaluateTriggers();
        console_.equipment.Update(dt, console_.panel);
        CheckLaunch();
        CheckEmergencyLaunch();
        UpdateTransmit();
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
        WatchScope();
        WatchTrackSigns();
        ArmEmission();
        QueryBurst();
        UpdateGrades();
        UpdateCallouts();
        UpdateTvRangeCallout();
        UpdateCounterJamming();
        UpdateStress();
        UpdateNpcCommander();
        NpcSurvey();  // after his think: the orders and drills of his seat first, then out with the compass
        UpdateOwnReports();
        PlayerArmReport();
        PlayerChestDuty();
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
    // Designations the NPC commander passed on with a digit wrong (roadmap 7.2).
    int DesignationSlips() const { return designation_slips_; }

    const ScenarioDef& Def() const { return def_; }
    // The session's sky (roadmap R7, light.hpp): the scenario's clock and the light of its sun now, which the TV, the
    // battery's night, the visual range, the guns' sights and the waves all read. A world fact, never a crewman's.
    const SkyClock& Clock() const { return def_.clock; }
    const SunLight& LightNow() const { return light_; }
    ConsoleData& Console() { return console_; }
    const ConsoleData& Console() const { return console_; }
    World& GetWorld() { return world_; }
    const World& GetWorld() const { return world_; }

    // Missiles in flight, for display (empty without the engagement model).
    std::vector<EngagementModel::MissileView> MissilesInFlight() {
        if (!engagement_on_) return {};
        return engagement_.MissilesInFlight(time_, world_, eng_targets_);
    }
    // The presentation feed (fx/events.hpp): what happened, for the game to show and sound. Nothing in the simulation
    // reads it. The mission bridge (air_war/mission_bridge.hpp) pushes its own events here too.
    fx::Feed& Fx() { return fx_; }
    const fx::Feed& Fx() const { return fx_; }
    // Switched off, the runner and the bridge push nothing into the feed and draw nothing for it (the presentation's own
    // generator stays where it was): what is left is the simulation alone, which must run exactly as with it on (the
    // tests compare the two). On by default.
    void SetFxOn(bool on) { fx_on_ = on; }
    bool FxOn() const { return fx_on_; }
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

    // The crew chief's compass work (roadmap 6.5, orientation.hpp): the commander finds the vehicle's heading Qc with
    // the artillery compass at a halt, then commands it to the deputy, who orients the gyro with it. With a survey drill
    // in the console's data (the Osa's measure_qc) an NPC crew chief works it, queued after what he is doing: he carries
    // the aiming circle out ahead (data/vehicles/osa_ak.json), the survey runs from when it is set up to the console's
    // reading, and he tells the deputy the Qc once back in earshot (docs/crew/13_OUT_OF_THE_VEHICLE.md). A player crew
    // chief goes out and sets it up himself (the helper shows it). False if the console has no orientation, the vehicle
    // is moving or a survey is already under way.
    bool StartCompassSurvey(const std::string& by = "Commander") {
        if (!console_.has_orientation || !console_.equipment.GetInput("vehicle_stopped") || SurveyUnderWay()) return false;
        const OrientationDef& o = console_.orientation;
        if (!o.survey_procedure.empty()) {
            survey_recalled_ = false;
            if (!SeatHolder(Cmdr()).empty()) {
                log_.Record({time_, DebriefEntry::Kind::Scenario, by, "orientation", "the compass work: out with the aiming circle (ПАБ) ahead of the vehicle"});
                return true;
            }
            CrewMember* m = MemberAt(Cmdr());
            const Procedure* p = console_.FindProcedure(o.survey_procedure);
            if (m == nullptr || p == nullptr) return false;
            Hand(*m, *p, true);
            survey_drill_ = true;
            log_.Record({time_, DebriefEntry::Kind::Scenario, by, "orientation", "takes the aiming circle (ПАБ) out ahead of the vehicle"});
            return true;
        }
        compass_.Start(time_);
        log_.Record({time_, DebriefEntry::Kind::Scenario, by, "orientation", "sets up the artillery compass (ПАБ) in front of the vehicle"});
        return true;
    }
    const CompassSurvey& Compass() const { return compass_; }
    // A compass survey under way at this halt: the crew chief's drill handed out, the compass running, or its Qc found
    // and not yet told to the deputy.
    bool SurveyUnderWay() const { return compass_.Running() || survey_drill_ || qc_untold_; }
    // The crew chief has this halt's Qc and has not yet told it (out at the aiming circle, or on his way back in).
    bool QcUntold() const { return qc_untold_; }
    // Every designation the commander has heard on the radio so far (the latest last).
    const std::vector<C2Network::Event>& DesignationsHeard() const { return designations_heard_; }
    // When the last call tone went out (ТОН. ВЫЗОВ), or a large negative number.
    double LastToneCallS() const { return last_tone_s_; }
    // The R-123M sets' tuning (null when the console has no preset set-up).
    const RadioTuning* RadioPresets() const { return tuning_.get(); }
    // When the vehicle last went on the air (a report, a carrier second, a call tone).
    double LastTransmissionS() const { return last_tx_s_; }
    // Transmissions the vehicle has made (roadmap 7.8): every report, carrier second and call tone that went out.
    int Transmissions() const { return transmissions_; }
    // Can the commander go on the air on his set now? `why` says what stops him.
    bool CommanderCanTransmit(std::string* why = nullptr) const { return CanTransmitNow(why); }
    // The vehicle has been oriented at this halt (the gyro slaved to the crew chief's Qc since it last stopped).
    bool OrientedAtHalt() const { return halt_oriented_; }

    // Replace the scenario's ground, e.g. with heights sampled from Cesium. Null restores the scenario's own (its map's,
    // or the flat ground). The terrain must outlive the runner or be replaced first.
    void SetTerrain(const Terrain* terrain) {
        world_.SetTerrain(terrain != nullptr ? terrain : (map_ground_ ? static_cast<const Terrain*>(map_ground_.get()) : &ground_));
        if (em_link_) em_link_->SetTerrain(&world_.GetTerrain());
        if (battery_fire_) battery_fire_->SetGround(&world_.GetTerrain(), &world_.Frame());
        if (neighbour_fire_) neighbour_fire_->SetGround(&world_.GetTerrain(), &world_.Frame());
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
        RecallFromJournal(number);
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
        RecallFromJournal(number);
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
    // Every message logged since the start, kept or not: RadioLog()[i] is message RadioTotal() - RadioLog().size() + i.
    std::size_t RadioTotal() const { return radio_total_; }

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
                if (v.def.own || v.away) continue;  // away on a run: drawn as its mover on the roads (MoversInView)
                x.battery.push_back({v.def.callsign, BatteryVehicleKindName(v.def.kind), v.position, v.heading_deg, v.deployed,
                                     GroundEnu(v.position.x, v.position.y)});
            }
        }
        for (std::size_t i = 0; i < def_.prepared.dummies.size(); ++i) {
            const Vec3& at = def_.prepared.dummies[i].position;
            x.battery.push_back({"dummy " + std::to_string(i + 1), "dummy", at, 0.0, true, GroundEnu(at.x, at.y)});
        }
        x.works = Works();
        return x;
    }

    // A prepared position's works (roadmap 7.6): an emplacement and a net at each place a vehicle of the battery started
    // in (ours, the battery's, the dummy positions). The emplacements stay; a vehicle's net is taken down once it has
    // left its place (more than 15 m off it). Empty without a prepared position.
    std::vector<em::ViewWork> Works() const {
        std::vector<em::ViewWork> out;
        if (!def_.prepared.present) return out;
        for (const WorkPlace& w : work_places_) {
            em::ViewWork k;
            k.position = w.position;
            k.heading_deg = w.heading_deg;
            k.emplacement = def_.prepared.emplacement;
            bool here = true;
            if (w.vehicle == "own") {
                here = (def_.unit_position - w.position).HorizontalLength() <= 15.0;
            } else if (!w.vehicle.empty() && battery_) {
                for (const Battery::Vehicle& v : battery_->Vehicles()) {
                    if (v.def.callsign == w.vehicle) here = (v.position - w.position).HorizontalLength() <= 15.0;
                }
            }
            k.nets = def_.prepared.nets && here;
            k.ground = GroundEnu(w.position.x, w.position.y);
            out.push_back(k);
        }
        return out;
    }

    // The prepared position the unit stands in (present = false: none).
    const PreparedPositionDef& Prepared() const { return def_.prepared; }

    // The chain of command's messages in the session (roadmap P1, session_deliveries.hpp, docs/command/06_INTEGRATION.md
    // §6): every delivery the scenario holds, the papers a station has been handed ("" every station's), and the people
    // coming over the ground with one, where they are now.
    const std::vector<DeliveryDef>& Deliveries() const { return deliveries_.Defs(); }
    // The papers in a seat's map case: those the chain of command handed in (P1), the battery commander's own (the staff
    // list, the communications diagram: G8) and the march's (the route card, the order of march: G9.3, G9.4), oldest first.
    std::vector<HeldPaper> MapCase(const std::string& station) const {
        std::vector<HeldPaper> out = deliveries_.MapCase(station);
        for (const HeldPaper& p : own_papers_) {
            if (station.empty() || p.station == station) out.push_back(p);
        }
        std::stable_sort(out.begin(), out.end(), [](const HeldPaper& a, const HeldPaper& b) { return a.time_s < b.time_s; });
        return out;
    }
    // The route service in the session (null: the scenario has no "nav"): the roads, the places, the world's movers.
    const nav::SessionNav* Nav() const { return nav_.get(); }
    // The march planned to a place (G2.3), if one was.
    const nav::MarchPlan* MarchPlanned() const { return has_march_plan_ ? &march_plan_ : nullptr; }
    // The world's movers inside the visual range (roadmap G11): each vehicle of another unit's column, a convoy, a carrier,
    // where the session has it, on the ground. Empty without the session's roads.
    struct WorldMover {
        int id = 0;  // fixed for the session (the mover's place among them and the vehicle's in it): an entity id
        std::string mover;
        std::string kind;
        std::string model;
        Vec3 position;
        double heading_deg = 0.0;
        bool moving = false;
    };
    std::vector<WorldMover> MoversInView() const {
        std::vector<WorldMover> out;
        if (!nav_) return out;
        for (const nav::SessionNav::Seen& s : nav_->Visible()) {
            WorldMover v;
            v.id = s.ordinal * 1000 + std::min(s.index, 999);
            v.mover = s.mover;
            v.kind = s.kind;
            v.model = s.model;
            v.position = GroundEnu(s.at.east_m, s.at.north_m);
            v.heading_deg = s.heading_rad * 180.0 / 3.14159265358979323846;
            v.moving = s.moving;
            out.push_back(v);
        }
        return out;
    }
    // What the crew called in the fight, as its commander reports it (roadmap P4, command/session_reports.hpp): each
    // launch and each result the judge called, right or wrong; the crew's word, not the truth.
    struct CrewCall {
        double time_s = 0.0;
        std::string kind;    // "launch", "destroyed", "missed", "unknown"
        std::string target;  // as the crew knows it (the scope's label)
    };
    const std::vector<CrewCall>& CrewCalls() const { return crew_calls_; }
    struct VisitorState {
        int index = 0;       // which of the scenario's visitors (fixed for the session: an entity id)
        std::string id;
        std::string model;   // its basic model ("soldier", "motorcycle", "uaz469")
        std::string from;    // who he is
        Vec3 position;       // ENU, on the ground
        double heading_deg = 0.0;
        bool at_vehicle = false;  // handing it over
    };
    std::vector<VisitorState> Visitors() const {
        std::vector<VisitorState> out;
        const std::vector<Visitor>& all = deliveries_.Visitors();
        for (std::size_t i = 0; i < all.size(); ++i) {
            const Visitor& v = all[i];
            if (!v.Present(time_)) continue;
            const Vec3 off = v.Offset(time_);
            VisitorState s;
            s.index = static_cast<int>(i);
            s.id = v.id;
            s.model = v.model;
            s.from = v.from;
            s.position = GroundEnu(def_.unit_position.x + off.x, def_.unit_position.y + off.y);
            s.heading_deg = v.HeadingDeg(time_);
            s.at_vehicle = time_ >= v.arrive_s && time_ < v.arrive_s + Visitor::kWaitS;
            out.push_back(s);
        }
        return out;
    }

    // The battery around the player's vehicle (null: the scenario has none).
    const Battery* GetBattery() const { return battery_.get(); }
    bool OrderBattery(const BatteryOrder& o) {
        if (!battery_) return false;
        BatteryTakes(o);
        return true;
    }
    // The battery's orders carried by a means (null: the scenario's battery takes them at once): the book of every order
    // and each vehicle's times, the debrief's lines.
    const command::BatteryOrderNet* GetBatteryOrders() const { return order_net_.get(); }
    // The journal the battery commander's radiotelephonist keeps of the orders and the replies (null: no order net).
    const RadioJournal* GetBatteryJournal() const { return order_net_ ? &GetGiverJournal() : nullptr; }
    // The regiment's orders to the battery commander, carried on the same path (null: no order net).
    const command::BatteryOrderNet* GetRegimentOrders() const { return regiment_net_.get(); }
    // The regiment's task (roadmap 14.1, 14.3): his plan, when he had it, when the battery was reported ready (< 0: not).
    bool HasTask() const { return def_.has_task; }
    const campaign::BatteryTask& Task() const { return def_.task; }
    const campaign::BatteryPlan& TaskPlan() const { return task_plan_; }
    double TaskReceivedS() const { return task_received_s_; }
    double TaskReadyS() const { return task_ready_s_; }
    int TaskSectorsSet() const { return task_sectors_set_; }
    std::string TaskClock(double s) const { return DayClock(s); }
    // The regiment's next task, given into the running session now (the battery's days go on in one session, roadmap
    // 15.2; the regiment's tasking gives it, 14.2): on its net to the battery commander ("radio") or in person, and he
    // plans it from where his battery stands. The task before it is over: its account stays in the debrief. False, with
    // why, without a battery to give it to.
    bool GiveTask(campaign::BatteryTask t, const std::string& means, std::string* why = nullptr) {
        const auto fail = [why](const std::string& w) {
            if (why != nullptr) *why = w;
            return false;
        };
        if (!battery_ || !nav_ || def_.plan_sequence.steps.empty()) return fail("the session has no battery on its roads to plan a task for");
        if (means != "radio" && means != "in_person") return fail("a task goes by \"radio\" or \"in_person\"");
        if (means == "radio" && !regiment_net_) return fail("the session has no regiment's net to say the task on");
        for (const std::string& place : t.Places()) {
            if (nav_->GetAnchors().Find(place) == nullptr) return fail(t.id + ": no place called '" + place + "' on the roads");
        }
        for (const campaign::TaskSector& sec : t.sectors) {
            const Battery::Vehicle* v = battery_->Find(sec.vehicle);
            if (v == nullptr || v->def.kind != BatteryVehicleKind::Launcher) return fail(t.id + ": a sector for '" + sec.vehicle + "', not a launcher of the battery");
        }
        if (def_.has_task) tasks_before_.push_back(TaskBlock());
        // The session goes on to the new task's ready-by and a while past it, so that its readiness is seen.
        def_.time_limit_s = std::max(def_.time_limit_s, time_ + t.ready_in_s + kTaskWatchedS);
        def_.task = std::move(t);
        def_.has_task = true;
        def_.task_given_s = time_;
        def_.task_means = means;
        task_order_ = -1;
        task_given_ = false;
        task_received_s_ = -1.0;
        task_plan_ = campaign::BatteryPlan();
        task_steps_done_.clear();
        task_orders_.clear();
        task_orders_done_.clear();
        task_formation_pending_ = false;
        task_moved_ = false;
        task_ready_s_ = -1.0;
        task_sectors_set_ = 0;
        return true;
    }
    int TasksGiven() const { return static_cast<int>(tasks_before_.size()) + (def_.has_task ? 1 : 0); }
    static constexpr double kTaskWatchedS = 1200.0;  // sam3d: as the bridges mission's end, 19 min past its ready-by

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
            a.stress = m.StressLevel();
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
    // The side of the vehicle toward the carriageway where it stands at a road (-1 its left, 1 its right, 0 not at a
    // road): the crew gets on and off on the other (stroevoy_ustav_1993 art. 147, 154), the movers' ways down and up
    // kept off it. At a road: within half the road's width and kRoadsideM of its centre line on the scenario's map.
    int TrafficSide() const { return traffic_side_; }
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
    // A place in words: "the line-up", a line's far end, a place round the vehicle by its label.
    std::string PlaceLabel(const std::string& id) const {
        if (id == "lineup") return "the line-up";
        if (const int n = LinePlace(id)) {
            const LineDef* l = def_.radio_data.Line(n);
            return "the far end of line " + std::to_string(n) + (l != nullptr ? " (" + l->to_name + ")" : std::string());
        }
        const VehiclePlace* p = def_.vehicle.Place(id);
        return p != nullptr ? p->label : id;
    }
    // The way to a place for a body: a place round the vehicle, "lineup", a line's far end ("line:<n>", at the pace of
    // the cable paid out), "" his seat. What the engine asks for to walk the player out to a job.
    MoveGoal PlaceGoal(const std::string& place) const {
        if (place.empty()) return MoveGoal::Seat();
        if (place == "lineup") return MoveGoal::InLineup();
        if (const int n = LinePlace(place)) {
            Vec3 end_at;
            if (def_.has_vehicle && LineFarEnd(n, end_at)) return MoveGoal::OnGroundAt(end_at, place, def_.vehicle.wire.lay_mps);
        }
        return MoveGoal::AtPlace(place);
    }

    // ---- wire (docs/crew/13_OUT_OF_THE_VEHICLE.md §4): the RTK-1's lines laid by the crew. A line the console has
    // ("wire.line<n>" worked at its far end, "line<n>_run", "line<n>_laid") is laid while line<n>_laid is ready; the
    // scenario's laid lines start so. One not laid: «Линии проложить!», and its man pays the P-274M out from the vehicle
    // to the far end at the laying pace (the cable's length the way he walked), connects the far set there and rings the
    // post on it.
    struct WireRun {
        std::string station;       // who paid it out
        std::vector<Vec3> points;  // the cable on the ground, from the vehicle's side towards the far set (ENU)
        double length_m = 0.0;     // paid out so far
        bool reached = false;      // run out to the far end
    };
    const std::map<int, WireRun>& Wires() const { return wires_; }
    // Whether a line is laid now: by its subsystem where the crew lays it, else as the scenario says.
    bool LineLaid(const LineDef& l) const {
        const std::string n = std::to_string(l.line);
        if (console_.panel.Find("wire.line" + n) != nullptr && console_.equipment.Def("line" + n + "_laid") != nullptr) {
            return console_.equipment.IsReady("line" + n + "_laid");
        }
        return l.laid;
    }
    const LineDef* LaidLineTo(const std::string& to) const {
        for (const LineDef& l : def_.radio_data.lines) {
            if (l.to == to && LineLaid(l)) return &l;
        }
        return nullptr;
    }
    // Where line n's far set stands (ENU): the scenario's far_end; else beside the battery's vehicle of that call sign
    // or the cabled radar, the wire's far_set_m short of it on the post's side. False: no far end the crew can walk to
    // (the regiment's line is laid from the regiment).
    bool LineFarEnd(int line, Vec3& out) const {
        const LineDef* l = def_.radio_data.Line(line);
        if (l == nullptr) return false;
        if (l->has_far_end) {
            out = l->far_end;
            return true;
        }
        Vec3 at;
        bool found = false;
        if (battery_) {
            for (const Battery::Vehicle& v : battery_->Vehicles()) {
                if (!v.def.own && (v.def.id == l->to || v.def.callsign == l->to)) {
                    at = v.position;
                    found = true;
                    break;
                }
            }
        }
        if (!found && l->to == "radar" && def_.has_post_radar) {
            at = def_.post_radar.position;
            found = true;
        }
        if (!found) return false;
        const Vec3 post = Pose().origin;
        const Vec3 d{post.x - at.x, post.y - at.y, 0.0};
        const double len = d.Length();
        const double short_m = def_.has_vehicle ? def_.vehicle.wire.far_set_m : 0.0;
        out = len > short_m ? at + d * (short_m / len) : at;
        out.z = world_.GetTerrain().HeightAt(out.x, out.y);
        return true;
    }
    // Why the crew can't lay line n now ("" : it can): a line of the scenario's on the console, not laid nor being laid,
    // its far end ours to reach and within the cable (the run along the ground a tenth over the straight line).
    std::string LineCantBeLaid(int line) const {
        const LineDef* l = def_.radio_data.Line(line);
        const std::string n = std::to_string(line);
        if (l == nullptr) return "no line " + n;
        if (!def_.has_vehicle || !def_.vehicle.wire.present || console_.panel.Find("wire.line" + n) == nullptr) return "not the crew's to lay";
        if (LineLaid(*l)) return "laid";
        if (console_.panel.Get("wire.line" + n) == 1) return "being laid";
        Vec3 end_at;
        if (!LineFarEnd(line, end_at)) return "its far end is not ours to reach";
        const Vec3 post = Pose().origin;
        if (1.1 * std::hypot(end_at.x - post.x, end_at.y - post.y) > def_.vehicle.wire.cable_m) return "its far end is beyond the cable";
        return std::string();
    }
    bool AnyLineToLay() const {
        for (const LineDef& l : def_.radio_data.lines) {
            if (LineCantBeLaid(l.line).empty()) return true;
        }
        return false;
    }
    // A drill an order would start that has nothing to do here: laying a line that is laid, or that the crew can't lay.
    bool ProcedureWanted(const std::string& procedure) const {
        if (procedure.rfind("lay_line", 0) == 0) return LineCantBeLaid(std::atoi(procedure.c_str() + 8)).empty();
        return true;
    }

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
        for (const auto& w : wires_) {  // at the far end of a line he has run out (not one he walked to without the cable)
            Vec3 end_at;
            if (w.second.reached && w.second.station == station && LineFarEnd(w.first, end_at) && std::hypot(end_at.x - enu.x, end_at.y - enu.y) <= 3.0) {
                at = "line:" + std::to_string(w.first);
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
        UpdateTrafficSide();
        it->second.SetKeepOffSide(traffic_side_);
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
            if (m->Moving() || m->At() != place || m->Place() != BodyPlace::Ground) return "That is done outside: " + PlaceLabel(place);
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
        LineUpCrew();
        return true;
    }
    // The work of the two orders without their words (the order menu's orders speak their own: GiveCrewOrder).
    void LineUpCrew() {
        for (CrewMember& c : crew_) {  // they leave what they are doing and get out
            Vec3 at;
            if (!SeatHolder(c.Role()).empty() || !def_.vehicle.LineupPoint(c.Role(), at)) continue;
            c.CancelOrder();
            c.StayAt("lineup");
        }
        Event(Cmdr(), "К машинам: the crew lines up beside the vehicle");
    }
    void MountCrew() {
        for (CrewMember& c : crew_) c.StayAt(std::string());
        Event(Cmdr(), "По местам: the crew mounts");
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
        MountCrew();
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
    // The lengths of the recorded lines the game plays (crew_voice.hpp RecordedLineLengths, from `from`, its folder): a
    // fixed line with a recording takes the recording's time on the bus and on the air, so the next line on the channel
    // doesn't cut it off. Headless, nothing is played and every line takes SpeakingTimeS.
    void SetRecordedLengths(std::map<std::string, double> lengths_s, std::string from) {
        recorded_lengths_s_ = std::move(lengths_s);
        recorded_lengths_from_ = std::move(from);
        if (callouts_) callouts_->SetRecordedLengths(recorded_lengths_s_);
    }
    const std::string& RecordedLengthsFrom() const { return recorded_lengths_from_; }
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
        // The emission regime (docs/crew/16_EMISSION.md): told to the crew with its numbers, or the battery's on its net.
        if (order_id == "order_emission_regime" || order_id == "order_remind_bursts" || order_id == "order_remind_ssc") return TellRegime(order_id);
        if (order_id.rfind("order_battery_emission_", 0) == 0) {
            EmissionKind k = EmissionKind::Free;
            if (!ParseEmissionKind(order_id.substr(23), k)) return false;
            // The regime he owes (the regiment's, with its turns) when it is that kind; else his own with the data's turns.
            const EmissionRegime r = pending_since_s_ >= 0.0 && pending_regime_.kind == k ? pending_regime_ : def_.emission.Make(k, "battery");
            const bool done = OrderBatteryEmission(r, "Battery commander");
            if (done) orders_given_.push_back({time_, order_id});
            return done;
        }
        if (order_id == "order_battery_preliminary" || order_id == "order_battery_combat_order") {  // the task's plan (14.3)
            const bool done = PostGivesOrders() && GiveTaskOrder(order_id, "Battery commander");
            if (done) orders_given_.push_back({time_, order_id});
            return done;
        }
        BatteryOrder::Kind move_kind = BatteryOrder::Kind::March;
        if (PostOrderKind(order_id, move_kind)) {  // the battery's moves on the order net (G10.8)
            const bool done = PostGivesOrders() && PostGivesBatteryOrder(order_id);
            if (done) orders_given_.push_back({time_, order_id});
            return done;
        }
        // The commander's orders on the nets (docs/crew/12_COMMUNICATIONS.md): said on the air, not on the intercom.
        if (order_id == "order_battery_spare" || order_id == "order_battery_duty" || order_id == "order_battery_simplex" ||
            order_id == "order_warn_covered" || order_id == "order_all_clear_covered" || order_id == "order_report_regiment" ||
            order_id == "order_battery_search_own" || order_id == "order_battery_by_designation" || order_id == "order_tzm_run") {
            bool done = false;
            if (order_id == "order_tzm_run") done = OrderTzmRun("Battery commander");
            if (order_id == "order_battery_search_own") done = OrderSearchOwn(true, "Battery commander");
            if (order_id == "order_battery_by_designation") done = OrderSearchOwn(false, "Battery commander");
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
        EmconOrderTells(order_id);
        if (order_id == "order_drop_target") drop_at_s_ = time_ + SpeakDelayS("operator") + 1.5;  // the wheels off the target
        quiet_orders_ = true;  // the order's own words were spoken: its drills don't each speak an order
        for (const OrderStart& s : o->procedures) {
            if (!ProcedureWanted(s.procedure)) continue;
            CrewMember* m = MemberAt(s.station);
            const Procedure* p = console_.FindProcedure(s.procedure);
            if (p != nullptr && halt_oriented_ && OrientsVehicle(*p)) continue;  // this halt's Qc is on the gyro already
            if (!s.queue && s.procedure != "function_check") BreakOffFunctionCheck(s.station);  // the order comes first
            if (routine_order_) routine_drills_.insert(s.station + "|" + s.procedure);
            if (m != nullptr && p != nullptr) GiveOrder(*m, *p, s.queue);
        }
        quiet_orders_ = false;
        for (const std::string& a : o->actions) {
            if (a == "compass_survey" && !halt_oriented_ && !engagement_.QcKnown() && !SurveyUnderWay()) StartCompassSurvey();
            if (a == "dismount" && def_.has_vehicle && def_.vehicle.lineup.present) LineUpCrew();
            if (a == "mount" && def_.has_vehicle) MountCrew();
        }
        return true;
    }
    // The orientation drill (its first step АК — ЗК to ЗК): an order carrying it skips it at a halt already oriented.
    bool OrientsVehicle(const Procedure& p) const {
        return console_.has_orientation && !p.steps.empty() && p.steps.front().control_id == console_.orientation.mode_control &&
               p.steps.front().value == console_.orientation.set_position;
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
        if (order_id == "order_lay_lines" && !AnyLineToLay()) return refuse("линий для прокладки нет", "no line to lay");
        // The orders on the nets: whoever must talk on the net can (his box on its set, the set transmitting).
        const auto net_of = [&]() -> std::string {
            if (order_id == "order_battery_spare" || order_id == "order_battery_duty" || order_id == "order_battery_simplex") return "battery";
            if (order_id == "order_battery_search_own" || order_id == "order_battery_by_designation") return "battery";
            if (order_id == "order_tzm_run") return "battery";
            if (order_id == "order_warn_covered" || order_id == "order_all_clear_covered") return "covered";
            if (order_id == "order_report_regiment") return "regiment";
            if (order_id.rfind("order_battery_emission_", 0) == 0) return "battery";
            BatteryOrder::Kind k = BatteryOrder::Kind::March;
            if (PostOrderKind(order_id, k)) return MarchRadioBan() ? std::string() : std::string("battery");  // on the march: flags
            return std::string();
        };
        BatteryOrder::Kind move_kind = BatteryOrder::Kind::March;
        if (PostOrderKind(order_id, move_kind)) {
            if (!PostGivesOrders()) return refuse("приказы батарее отдаёт командир батареи на ПУ", "the battery's orders are given from its command post");
            if (move_kind != BatteryOrder::Kind::Halt && move_kind != BatteryOrder::Kind::Resume && !PostPlanned(order_id)) {
                return refuse("в боевом приказе этого нет", "not in the combat order: no route or position for it yet");
            }
        }
        if (order_id == "order_battery_search_own" || order_id == "order_battery_by_designation") {
            if (!battery_fire_) return refuse("пусковых на батарейной сети нет", "no launchers on the battery net");
            if ((order_id == "order_battery_search_own") == (search_own_order_ >= 0)) return refuse("уже выполнено", "done already");
        }
        if (order_id == "order_tzm_run") {
            if (tzm_id_.empty()) return refuse("ТЗМ на батарейной сети нет", "no TZM on the battery net to send");
            if (tzm_run_order_ >= 0) return refuse("ТЗМ уже в рейсе", "the TZM is on its run already");
        }
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
        // The reminders: a regime with bursts told to the crew (the tracking transmitter's rule: the least emission).
        if ((order_id == "order_remind_bursts" && !regime_crew_.Bursts()) || (order_id == "order_remind_ssc" && regime_crew_.kind != EmissionKind::Arm)) {
            return refuse("экипажу не доведён режим с пачками излучения", "the crew has no regime of bursts to be reminded of");
        }
        if (!o->task.empty()) {
            const CrewTask* t = def_.crew_data.FindTask(o->task);
            if (t != nullptr && !t->conditions.empty() && console_.equipment.AllHold(t->conditions, console_.panel)) {
                return refuse("уже выполнено", "done already");
            }
        }
        for (const std::string& a : o->actions) {  // К машине and По местам: a vehicle with a line-up; not when it holds
            if (a != "dismount" && a != "mount") continue;
            if (!def_.has_vehicle || (a == "dismount" && !def_.vehicle.lineup.present)) {
                return refuse("построения у машины нет", "no line-up for this vehicle");
            }
            bool holds = true;
            for (const CrewMember& c : crew_) {
                Vec3 at;
                if (!SeatHolder(c.Role()).empty()) continue;
                if (a == "dismount") {
                    holds = holds && (!def_.vehicle.LineupPoint(c.Role(), at) || c.StaysAt() == "lineup");
                } else {
                    const CrewMover* mv = Mover(c.Role());
                    holds = holds && c.StaysAt().empty() && (mv == nullptr || mv->Place() == BodyPlace::Seated);
                }
            }
            if (holds) return refuse("уже выполнено", "done already");
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
            if (p == nullptr || p->steps.empty() || !ProcedureWanted(s.procedure)) continue;
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
    // Why the equipment turns a hand wheel itself now ("" if a hand turns it): a player's drag on it does nothing then.
    std::string WheelDriven(const std::string& wheel) const {
        const char* why = engagement_on_ ? engagement_.WheelDrivenBy(console_, wheel) : nullptr;
        return why != nullptr ? std::string(why) : std::string();
    }
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
        const std::string driven = WheelDriven(wheel_id);
        if (!driven.empty()) {  // the equipment turns it now: a hand on it changes nothing
            if (why != nullptr) *why = "Hands off: " + driven;
            return false;
        }
        const ControlDef* d = console_.panel.Find(wheel_id);
        if (d != nullptr && !d->guard_id.empty() && console_.panel.Get(d->guard_id) < 1) {
            const ControlDef* g = console_.panel.Find(d->guard_id);
            if (why != nullptr) *why = "Locked: release the lock first" + (g != nullptr && !g->label.empty() ? " (" + g->label + ")" : std::string());
            return false;
        }
        // Turned on against its stop (the tracking antenna's ±330°): nothing moves.
        const double before = console_.panel.GetValue(wheel_id);
        if (d != nullptr && !d->wraps && ((value > d->max_value && before >= d->max_value) || (value < d->min_value && before <= d->min_value))) {
            if (why != nullptr) {
                char buf[64];
                std::snprintf(buf, sizeof buf, "At its stop (%g): turn it back", value > d->max_value ? d->max_value : d->min_value);
                *why = buf;
            }
            return false;
        }
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
        if (station != Cmdr() || !def_.has_crew_data) return out;
        if (c2_) {
            for (const char* id : {"report_ready", "readback"}) {
                if (def_.crew_data.FindCallout(id) != nullptr) out.push_back(id);
            }
        }
        // An ARM attack reported to the battery commander (refused until he has heard a sign of one).
        if ((battery_ || LinkedToPost()) && def_.crew_data.FindCallout("report_arm_attack") != nullptr) out.push_back("report_arm_attack");
        // The battery's orders on the net (orders by a means): his read-back and his «к маршу готов».
        if (!OwnOnOrderNet().empty()) {
            for (const char* id : {"order_readback", "report_ready_to_march"}) {
                if (def_.crew_data.FindCallout(id) != nullptr) out.push_back(id);
            }
        }
        // The TZM and the fuel truck asked for on the battery net (G9; refused until the launcher is empty, the tank at
        // its reserve).
        if (battery_) {
            if (launcher_ && def_.crew_data.FindCallout("request_tzm") != nullptr) out.push_back("request_tzm");
            if (fuel_ && def_.crew_data.FindCallout("request_fuel") != nullptr) out.push_back("request_fuel");
        }
        return out;
    }
    // The chest switch a player at this seat keys before saying this line on the air ("" when none is needed: not a
    // line on the air, no chest switch, or keyed already): the helper shows it first (7.8).
    std::string ChestToTalk(const std::string& seat, const std::string& callout) const {
        if (!comms_ || !comms_->HasChest() || seat != Cmdr() || comms_->ChestKeyed(console_) || !OnTheAir(callout)) return std::string();
        return comms_->ChestControl();
    }
    // Targets given this vehicle on the battery net that its commander heard (the PU-12's designations), so far.
    int DesignationsHeardCount() const { return static_cast<int>(designations_heard_.size()); }
    // The crew's results judged from the instruments' signs (kill_assessment.hpp), the wrong ones among them, and the
    // targets the senior operator saw flying on after they were called destroyed.
    int KillCalls() const { return kill_calls_; }
    int KillCallsWrong() const { return kill_calls_wrong_; }
    int StillFlyingCalls() const { return still_flying_; }
    // Kill reports the commander took back on the battery net; whether this runner (a post) holds a target destroyed.
    int KillReportsTakenBack() const { return kill_reports_taken_back_; }
    bool BelievesDestroyed(const std::string& target) const { return reported_kills_.count(target) != 0; }

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
        if (post_ && !console_.post_journal_keeper.empty() && (!journal_.Written().empty() || journal_.Forgotten() > 0)) {
            int by_hand = 0, digits = 0, right = 0, brief = 0, wrong = 0;
            for (const RadioJournalEntry& e : journal_.Written()) {
                by_hand += e.by_hand ? 1 : 0;
                digits += e.digits;
                right += e.digits_right;
                brief += e.brief ? 1 : 0;
                wrong += !e.brief && e.text != e.said ? 1 : 0;
            }
            std::snprintf(line, sizeof(line), "  The radio journal: %d written (%d by hand, %d of %d digits as said; %d short), %d not as said, %d lost unwritten\n",
                          static_cast<int>(journal_.Written().size()), by_hand, right, digits, brief, wrong, journal_.Forgotten());
            out += line;
            std::snprintf(line, sizeof(line), "  Plotted from the journal's page: %d (%d of them as the page had it wrong)\n", journal_reads_, journal_reads_wrong_);
            out += line;
        }
        if (post_ && (!radar_log_.empty() || !commander_log_.empty())) {  // the report from the records, the regiment's check (P9)
            campaign::CombatReport report;
            ReportFromRecords(report);
            out += "  The combat report from the records: " + campaign::ReportSummary(report) + "\n";
            campaign::RecordCheck checks[2];
            CheckRecords(checks[0], checks[1]);
            for (const campaign::RecordCheck& c : checks) {
                if (c.record == "journal" && console_.post_journal_keeper.empty()) continue;
                std::snprintf(line, sizeof(line), "  The %s against the radar company's log: %d of %d reports as said, %d a digit wrong, %d missing, %d ghosts: %s\n",
                              c.record.c_str(), c.as_said, c.said, c.wrong, c.missing, c.ghosts,
                              campaign::GradeWord(campaign::RecordGrade(c, def_.after_action)));
                out += line;
            }
        }
        for (const std::string& before : tasks_before_) out += before;  // the tasks the regiment gave before this one
        if (def_.has_task) out += TaskBlock();
        if (order_net_ && !order_net_->Book().Orders().empty()) {  // each battery order's times for each vehicle (G10.6)
            out += "  Battery orders (the order book, on the mission's clock):\n";
            for (const std::string& s : order_net_->Debrief(0.0)) out += "    " + s + "\n";
            if (regiment_net_ && !regiment_net_->Book().Orders().empty()) {
                out += "  The regiment's orders to the battery commander:\n";
                for (const std::string& s : regiment_net_->Debrief(0.0)) out += "    " + s + "\n";
            }
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
        // Held by the instructor (roadmap 10.3): it flies his heading, height and speed (his orders to it as a flight of
        // one, flights.hpp); its own pilot waits, and its route, never overwritten, is there when he lets it go.
        bool held = false;
        TargetPilot route_pilot;
        AircraftDamage damage;  // what the testbed's defences did to it (damage.hpp, roadmap R1)
        Vec3 last_position;     // where the world had it before this step (a weapon's impact between the two)
    };
    // A target added once the session runs (the instructor's, a package's): its state beside the scenario's own.
    void EnterTarget(ScenarioTarget t) {
        EngagementTarget e;
        e.id = t.id;
        e.target_class = t.target_class;
        e.evade_on_launch = t.evade_on_launch;
        e.evade_turn_rate_rad_s = t.evade_turn_rate_rad_s;
        e.friendly = t.friendly;
        e.size_m = t.size_m;
        e.span_m = t.span_m;
        eng_targets_.push_back(e);
        targets_.emplace_back();
        rwr_warned_.push_back(0);
        if (em_link_ && !t.rwr.empty()) em_link_->SetTargetRwr(t.id, t.rwr);
        def_.targets.push_back(std::move(t));
    }

    // Start warm: set the state's controls and inputs, warm its subsystems up in order (each sees those before it),
    // orient the vehicle if the state says so. The scenario's own inputs win over the state's.
    void ApplyReadiness(const ReadinessState& st) {
        for (const auto& c : st.controls) console_.panel.Set(c.first, c.second);
        for (const auto& in : st.inputs) console_.equipment.SetInput(in.first, in.second);
        for (const auto& in : def_.initial_inputs) console_.equipment.SetInput(in.first, in.second);
        console_.equipment.Update(0.0, console_.panel);
        for (const std::string& id : st.ready) {
            // Left cold for want of a world fact (no radar here to cable) is the position's, not the state's fault.
            bool world_lacks = false;
            if (const SubsystemDef* d = console_.equipment.Def(id)) {
                for (const Condition& c : d->enable) {
                    world_lacks = world_lacks || (c.kind == Condition::Kind::InputIs && !console_.equipment.Holds(c, console_.panel));
                }
            }
            if (!console_.equipment.Warm(id, console_.panel) && !world_lacks) {
                Event(id, "not commanded on in readiness '" + st.id + "': left cold");
            }
            console_.equipment.Update(0.0, console_.panel);
        }
        if (st.oriented) {
            engagement_.SetOriented();
            halt_oriented_ = true;
            // The crew set the site's latitude on the gyro's correction when they oriented (from the map).
            if (console_.has_orientation && !console_.orientation.latitude_control.empty()) {
                console_.panel.SetValue(console_.orientation.latitude_control, RadToDeg(def_.origin.lat_rad));
            }
        }
        Event(def_.id, "starts " + st.id + (st.description.empty() ? std::string() : ": " + st.description));
    }

    // The sun now, at most once a simulated second: when the TV's daylight or the battery's dark changes, everything that
    // reads it is told, and the debrief has a line (a world fact, not the crew's knowledge).
    void UpdateLight() {
        if (light_init_ && time_ < light_next_s_) return;
        light_next_s_ = time_ + 1.0;
        const SunLight l = LightAt(def_.clock, time_, def_.light_values);
        const bool changed = !light_init_ || l.daylight != light_.daylight || l.dark != light_.dark;
        const bool phase = light_init_ && l.phase != light_.phase;
        light_ = l;
        if (!changed && !phase) return;
        if (changed) {
            em::OpticalConditions o = def_.optics;
            o.daylight = light_.daylight;
            engagement_.SetOpticalConditions(o);
            if (battery_) battery_->SetNight(light_.dark);
            if (order_net_) order_net_->SetNight(light_.dark);
        }
        if (light_init_) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "%s: the sun %.1f deg %s the horizon%s", LightPhaseName(light_.phase),
                          std::fabs(light_.sun_elevation_deg), light_.sun_elevation_deg < 0.0 ? "below" : "above",
                          changed ? (light_.dark ? ", dark" : (light_.daylight ? ", daylight" : "")) : "");
            Event("light", buf);
        }
        light_init_ = true;
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
            a.max_load_g = t.max_load_g;  // its type's limits: with any it flies by bank-to-turn (roadmap R3)
            a.sustained_turn_rate_rad_s = t.sustained_turn_rate_rad_s;
            a.roll_rate_rad_s = t.roll_rate_rad_s;
            a.max_speed_mps = t.max_speed_mps;
            a.accel_mps2 = t.accel_mps2;
            a.max_climb_mps = t.max_climb_mps;
            a.rcs_m2 = t.rcs_m2;
            a.waypoints = t.waypoints;
            a.bottom_m = t.BottomM();
            targets_[i].pilot = TargetPilot(t.behaviour, t.evade);
            targets_[i].pilot.SetWeapon(t.weapon);
            targets_[i].pilot.OnSpawn(a);
            targets_[i].world_id = world_.AddAircraft(std::move(a));
            targets_[i].spawned = true;
            eng_targets_[i].world_id = targets_[i].world_id;
            Event(t.id, "appeared: " + t.name);
        }
        for (std::size_t i = 0; i < targets_.size(); ++i) {
            TargetState& s = targets_[i];
            if (!s.spawned || s.gone) continue;
            Aircraft* a = world_.FindAircraft(s.world_id);
            if (a == nullptr) continue;
            s.last_position = a->position;
            // A weapon on to its aim, its last waypoint, strikes there: it passes it close, not half a kilometre short.
            if (def_.targets[i].weapon && a->next_waypoint + 1 == a->waypoints.size()) a->waypoint_capture_radius_m = kWeaponAimCaptureM;
        }
        // The command layer (flights.hpp, roadmap B1): each aircraft told what to fly, or left to its route; then each
        // pilot's height layer.
        const Flights::Lookup air = FlightLookup();
        flights_->Steer(time_, air);
        for (std::size_t i = 0; i < targets_.size(); ++i) {
            TargetState& s = targets_[i];
            if (!s.spawned || s.gone) continue;
            Aircraft* a = world_.FindAircraft(s.world_id);
            if (a != nullptr) s.pilot.BeforeStep(*a);
        }
        world_.Step(dt);
        for (std::size_t i = 0; i < targets_.size(); ++i) {
            TargetState& s = targets_[i];
            if (!s.spawned) continue;
            Aircraft* a = world_.FindAircraft(s.world_id);
            if (a == nullptr) continue;
            if (s.gone) {
                // Past its last waypoint and flying on until it is taken out of the world (a wave's long gone): no more
                // pilot's work, but still held over the ground (roadmap R5).
                if (a->alive && !def_.targets[i].weapon) s.pilot.HoldOverGround(*a, world_.GetTerrain(), world_.Frame(), dt);
            } else {
                if (a->alive) {  // its override's turns and end, its place on the plan, the leg's speed
                    bool ended = false;
                    const std::string ev = flights_->AfterMove(static_cast<int>(i), *a, time_, air, ended);
                    if (ended) s.pilot.OverrideEnded();
                    if (!ev.empty()) Event(def_.targets[i].id, ev);
                }
                const std::string what = s.pilot.AfterStep(*a, world_.GetTerrain(), world_.Frame(), dt, time_);
                if (!what.empty()) Event(def_.targets[i].id, what);
            }
            // The safety net under the pilot's floor (roadmap R5): an aircraft whose belly is under the ground has flown
            // into it. The pilot's floor never lets one; a count above 0 is a bug.
            if (a->alive && !def_.targets[i].weapon &&
                world_.Frame().AltitudeOf(a->position) - a->bottom_m < world_.GetTerrain().HeightAt(a->position.x, a->position.y) - 0.01) {
                ++terrain_strikes_;
                Event(def_.targets[i].id, "flew into the ground");
                FxDown(def_.targets[i].id, "ground", true, false);
                a->alive = false;
            }
        }
        for (const SimEvent& e : world_.DrainEvents()) {
            if (e.type == EventType::AircraftDestroyed) Event(TargetIdOf(e.aircraft_id), "destroyed");
        }
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            TargetState& s = targets_[i];
            if (!s.spawned || s.gone) continue;
            Aircraft* a = world_.FindAircraft(s.world_id);
            if (a == nullptr || !a->alive) {
                s.gone = true;
                if (def_.targets[i].weapon) {
                    flights_->Gone(static_cast<int>(i));
                } else {
                    std::vector<Flights::Note> notes;
                    flights_->Down(static_cast<int>(i), time_, &notes);
                    for (const Flights::Note& n : notes) Event(n.who, n.what);
                }
            } else if (def_.targets[i].weapon && (a->next_waypoint >= a->waypoints.size() || AboveGround(a->position) <= 1.0)) {
                // A weapon ends where it strikes: it no longer flies (its actor goes as an inactive one does), and the
                // presentation feed has its warhead (a cruise missile's: fx/events.hpp). Where it strikes is on the
                // ground (roadmap R5): the crossing between where it was and where it is, or its aim (its last waypoint)
                // when it got there flying over the ground (a low one, at its floor).
                const bool at_aim = a->next_waypoint >= a->waypoints.size() && AboveGround(a->position) > 1.0 && !a->waypoints.empty();
                a->position = at_aim ? GroundEnu(a->waypoints.back().x, a->waypoints.back().y) : ImpactPoint(s.last_position, a->position);
                a->alive = false;
                s.gone = true;
                ++weapon_impacts_;
                Event(def_.targets[i].id, "impact");
                if (fx_on_) {
                    fx::Event e;
                    e.kind = fx::EventKind::CruiseImpact;
                    e.t_s = e.end_t_s = time_;
                    e.position = a->position;
                    e.velocity = a->velocity;
                    e.aim = GroundEnu(a->position.x, a->position.y);
                    e.source = def_.targets[i].id;
                    e.world_id = s.world_id;
                    e.number = 1;
                    e.weapon = def_.targets[i].model;
                    e.fire = true;
                    fx_.Push(e);
                }
                flights_->Gone(static_cast<int>(i));
            } else if (a->next_waypoint >= a->waypoints.size()) {
                s.gone = true;
                Event(def_.targets[i].id, "passed its last waypoint");
                a->demand = AircraftDemand();  // it flies on straight, as it is
                flights_->Gone(static_cast<int>(i));
            }
        }
        for (const Flights::Note& n : flights_->After(time_)) Event(n.who, n.what);
    }

    // The aircraft of a target in the sky now, for the command layer (flights.hpp).
    Flights::Lookup FlightLookup() {
        return [this](int t) -> Aircraft* {
            if (t < 0 || static_cast<std::size_t>(t) >= targets_.size()) return nullptr;
            const TargetState& s = targets_[static_cast<std::size_t>(t)];
            if (!s.spawned || s.gone) return nullptr;
            Aircraft* a = world_.FindAircraft(s.world_id);
            return a != nullptr && a->alive ? a : nullptr;
        };
    }

    // ---- Flights and packages (flights.hpp, roadmap B1): the scenario's own, and those entering it later. A target named
    // in no flight is a flight of one, its id the flight's.
    void RegisterFlights(std::size_t first) {
        std::map<std::string, std::vector<std::size_t>> members;
        std::vector<std::string> order;
        for (std::size_t i = first; i < def_.targets.size(); ++i) {
            const ScenarioTarget& t = def_.targets[i];
            if (t.flight.empty()) continue;
            if (members.find(t.flight) == members.end()) order.push_back(t.flight);
            members[t.flight].push_back(i);
        }
        for (std::size_t i = first; i < def_.targets.size(); ++i) {
            const ScenarioTarget& t = def_.targets[i];
            if (!t.flight.empty()) {
                if (order.empty() || t.flight != order.front()) continue;
                order.erase(order.begin());
                AddFlightOf(t.flight, members[t.flight]);
                continue;
            }
            AddFlightOf(std::string(), {i});
        }
        for (const ScenarioPackage& p : def_.packages) {
            if (packages_added_.count(p.id) != 0) continue;
            PackageState ps;
            ps.id = p.id;
            ps.size = p.size;
            ps.lost_before = p.lost_before;
            ps.turn_back_share = p.turn_back_share;
            for (std::size_t k = 0; k < flights_->Size(); ++k) {
                if (flights_->All()[k].package == p.id) ps.flights.push_back(static_cast<int>(k));
            }
            if (ps.flights.empty()) continue;  // its flights have not come in yet
            ps.commander_flight = flights_->Index(p.commander);
            flights_->AddPackage(std::move(ps));
            packages_added_.insert(p.id);
        }
    }
    void AddFlightOf(const std::string& id, const std::vector<std::size_t>& targets) {
        FlightState f;
        const ScenarioFlight* def = nullptr;
        for (const ScenarioFlight& d : def_.flights) {
            if (d.id == id) def = &d;
        }
        const ScenarioTarget& lead0 = def_.targets[targets.front()];
        f.id = id.empty() ? lead0.id : id;
        if (id.empty() && flights_->Index(f.id) >= 0) f.id = "target " + f.id;
        f.side = lead0.friendly ? "ours" : "enemy";
        if (def != nullptr) {
            f.callsign = def->callsign;
            f.package = def->package;
            f.side = def->side;
            f.keep_slots = def->keep_slots;
            f.call_s = def->call_s;
            f.formations = def->formations;
        }
        // Its slots in number order (unnumbered: in the order given); where each starts from the leader, in the leader's
        // first leg's frame (the formation for a flight without one).
        std::vector<std::size_t> ts = targets;
        std::vector<int> numbers;
        for (std::size_t k = 0; k < ts.size(); ++k) numbers.push_back(def_.targets[ts[k]].slot > 0 ? def_.targets[ts[k]].slot : static_cast<int>(k) + 1);
        std::size_t lead = 0;
        for (std::size_t k = 1; k < ts.size(); ++k) {
            if (numbers[k] < numbers[lead]) lead = k;
        }
        const ScenarioTarget& L = def_.targets[ts[lead]];
        Vec3 fwd = L.waypoints.empty() ? Vec3{0.0, 1.0, 0.0} : L.waypoints.front() - L.start;
        fwd.z = 0.0;
        fwd = fwd.LengthSquared() > 1.0 ? fwd.Normalized() : Vec3{0.0, 1.0, 0.0};
        const Vec3 right{fwd.y, -fwd.x, 0.0};
        bool weapons = true;
        for (std::size_t k = 0; k < ts.size(); ++k) {
            const ScenarioTarget& t = def_.targets[ts[k]];
            weapons = weapons && t.weapon;
            FlightSlot s;
            s.target = static_cast<int>(ts[k]);
            s.number = numbers[k];
            const Vec3 d = t.start - L.start;
            s.offset_m = Vec3{Dot(d, right), -Dot(d, fwd), d.z};
            s.legs = t.legs;
            f.slots.push_back(std::move(s));
        }
        f.weapons = weapons || (def != nullptr && !def->crewed);  // no crew: no nerve, no loss to its package
        if (weapons) f.keep_slots = false;  // a stream of weapons: each flies its own plan
        flights_->Add(std::move(f));
    }

public:
    // ---- The flights' command (flights.hpp, roadmap B1; docs/air_battle/02_FLIGHTS_AND_PILOTS.md §2.3). Who may give an
    // order (C2Authority::May) is roadmap B5: every order is accepted until then. Each call is logged with its `by`.
    bool OrderFlight(const std::string& flight, FlightOrder o, std::string* why = nullptr) {
        const int fi = flights_->Index(flight);
        if (fi < 0) {
            if (why != nullptr) *why = "no flight '" + flight + "'";
            return false;
        }
        if (o.by.empty()) o.by = "the session";
        if (!flights_->Order(fi, o, time_)) {
            if (why != nullptr) *why = o.by + " may not order " + flight;
            return false;
        }
        Event(flight, std::string("ordered: ") + ToString(o.kind) + " (by " + o.by + ")");
        return true;
    }
    // A new route for a flight from where it is (a Divert on its legs), toward `aim`, its new release point.
    bool RerouteFlight(const std::string& flight, std::vector<RouteLeg> legs, const Vec3& aim, const std::string& by, std::string* why = nullptr) {
        if (legs.empty()) {
            if (why != nullptr) *why = "a new route needs one leg at least";
            return false;
        }
        FlightOrder o;
        o.kind = OrderKind::Divert;
        o.legs = std::move(legs);
        o.has_aim = true;
        o.aim = aim;
        o.by = by;
        return OrderFlight(flight, std::move(o), why);
    }
    const FlightState* Flight(const std::string& id) const { return flights_->Find(id); }
    std::vector<std::string> FlightsOf(const std::string& package) const { return flights_->FlightsOf(package); }
    const Flights& AllFlights() const { return *flights_; }
    // Flights and packages about to enter the running session (a package's, before its AddTargets): their targets name
    // them. All or none: false, with why, for an id the session has already.
    bool AddFlights(const std::vector<ScenarioFlight>& fs, const std::vector<ScenarioPackage>& ps, std::string* why = nullptr) {
        const auto fail = [why](const std::string& w) {
            if (why != nullptr) *why = w;
            return false;
        };
        // Already the session's: a flight flying in it, a package whose flights are in it. A definition no aircraft has
        // named yet is replaced.
        std::set<std::string> ids, pids;
        for (const ScenarioFlight& f : fs) {
            if (f.id.empty() || !ids.insert(f.id).second || flights_->Index(f.id) >= 0) return fail("flight '" + f.id + "' is the session's already (or has no id)");
        }
        for (const ScenarioPackage& p : ps) {
            if (p.id.empty() || !pids.insert(p.id).second || packages_added_.count(p.id) != 0) return fail("package '" + p.id + "' is the session's already (or has no id)");
        }
        for (const ScenarioFlight& f : fs) {
            def_.flights.erase(std::remove_if(def_.flights.begin(), def_.flights.end(), [&f](const ScenarioFlight& o) { return o.id == f.id; }), def_.flights.end());
            def_.flights.push_back(f);
        }
        for (const ScenarioPackage& p : ps) {
            def_.packages.erase(std::remove_if(def_.packages.begin(), def_.packages.end(), [&p](const ScenarioPackage& o) { return o.id == p.id; }), def_.packages.end());
            def_.packages.push_back(p);
        }
        return true;
    }

private:

    // ---- The dynamic campaign testbed's waves (scenario "waves", air_war/test_waves.hpp): a wave when one is due, the
    // strike aircraft's loads released, and the waves' aircraft long gone taken out of the world (first marked dead, so
    // the EM layer and the board let them go, then removed).
    std::unique_ptr<Flights> flights_;     // the flights and packages in the session (flights.hpp, roadmap B1)
    std::set<std::string> packages_added_;
    std::unique_ptr<airwar::TestWaves> waves_;
    std::vector<std::size_t> wave_targets_;          // the waves' targets still in the world (indices into def_.targets)
    std::map<std::string, std::size_t> wave_index_;  // a wave aircraft's id -> its index (for its release)
    std::map<std::size_t, double> wave_gone_s_;      // when each went
    std::vector<airwar::WaveRelease> wave_loads_;    // loads released and still falling or flying to the ground
    int weapon_impacts_ = 0;
    int terrain_strikes_ = 0;
    static constexpr double kWeaponAimCaptureM = 25.0;  // a weapon passes its aim this close (a game value)

    // How far a point is above the ground under it (the altitude less the terrain's height there).
    double AboveGround(const Vec3& p) const { return world_.Frame().AltitudeOf(p) - world_.GetTerrain().HeightAt(p.x, p.y); }
    // Where a weapon strikes, on the ground: where its path from `from` to `to` crossed the ground (eight halvings), or
    // the ground under `to` if it never went under (it reached its aim, or is within a metre of the ground).
    Vec3 ImpactPoint(const Vec3& from, const Vec3& to) const {
        if (AboveGround(to) > 0.0 || !(AboveGround(from) > 0.0)) return GroundEnu(to.x, to.y);
        Vec3 lo = from, hi = to;  // above, under
        for (int k = 0; k < 8; ++k) {
            const Vec3 mid = (lo + hi) * 0.5;
            if (AboveGround(mid) > 0.0) lo = mid;
            else hi = mid;
        }
        const Vec3 at = (lo + hi) * 0.5;
        return GroundEnu(at.x, at.y);
    }

    // A load let go: the feed's release (the game flies and bursts it), its landing kept for its time.
    void ReleaseWaveLoad(airwar::WaveRelease r) {
        Event(r.carrier, "releases " + std::to_string(r.count) + " " + r.weapon_name + (r.delivery.empty() ? "" : " (" + r.delivery + ")"));
        if (fx_on_) {
            fx::Event e;
            e.kind = fx::EventKind::WeaponRelease;
            e.t_s = time_;
            e.end_t_s = r.impact_t_s;
            e.position = r.position;
            e.velocity = r.velocity;
            e.aim = r.impact;
            e.source = r.carrier;
            e.world_id = TargetWorldId(r.carrier);
            e.number = r.count;
            e.weapon = r.weapon;
            e.delivery = r.delivery;
            fx_.Push(e);
        }
        wave_loads_.push_back(std::move(r));
    }

    // The testbed's air defence round the capital (its neighbours that the battery fire model does not fight: the Kub,
    // the S-75M2, the S-125M1, the Strelas, the MANPADS and the guns) engages the waves' enemy aircraft. Each takes on an
    // aircraft once as it comes into its reach: whether it is shot down on that pass is the raids' own chance per pass
    // (air_war/test_waves.hpp WavePassKillChance, with the system's figures from the theatre: WavesDef::defences), drawn
    // from the defences' own generator. A missile site launches (its fire channels at once, a few seconds apart) and
    // its missile bursts at the meeting, on the aircraft if it hit, or flies on and destroys itself; it reloads once
    // empty. A gun site fires passes of a few seconds at the nearest aircraft in reach (rounds a minute as the mission
    // bridge's, air_war/mission_bridge.hpp GunRoundsPerMin: the munitions' rate_rpm, wiki_guns), and the pass that
    // engaged a new aircraft brings it down at its end if it hit. Missile speed, reload and the pauses are game values
    // (sam3d).
    // With the waves' damage and gun tables (roadmap R1, R2: WavesDef::has_damage) the guns fire as aaa.hpp has it
    // instead: through a pass, a group of rounds every group_s at where the fire control predicts the aircraft (its errors
    // drawn for the pass), each group judged when it arrives against where the aircraft really is, each time-fuzed
    // shell where it bursts; what they did is drawn by damage.hpp. A missile's kill is still the chance per pass, but it
    // may leave the aircraft heavily damaged instead of destroyed, and a miss may burst near enough to damage it.
    // Damage has its consequences here: smoke and fire in the feed, the load jettisoned, the aircraft turning for home,
    // a crash 10 s to 5 min after heavy damage.
    static constexpr double kNbMissileMps = 700.0;
    static constexpr double kNbMissileMinS = 3.0;
    static constexpr double kNbMissileMaxS = 45.0;
    static constexpr double kNbLaunchGapS = 4.0;
    static constexpr double kNbReloadS = 600.0;
    static constexpr unsigned kDefenceSeed = 0xDEF5u;
    struct NeighbourShot {
        std::size_t neighbour = 0;
        std::size_t target = 0;  // index into def_.targets
        double end_s = 0.0;
        bool hit = false;
        bool missile = false;
        int number = 0;
    };
    struct NeighbourDefenceState {
        int missiles = -1;              // ready (-1: guns)
        double reload_s = 0.0;          // when an empty site has reloaded
        double next_s = 0.0;            // its next launch or gun pass
        int launches = 0;
        std::set<std::size_t> engaged;  // aircraft (target indices) engaged on their pass through its reach
        // A gun site's pass under way (aaa.hpp): at whom, until when, its next group, the rounds a slow gun still owes,
        // the fire control's errors drawn for it.
        bool passing = false;
        std::size_t pass_target = 0;
        double pass_start_s = 0.0;
        double pass_end_s = 0.0;
        double next_group_s = 0.0;
        double rounds_due = 0.0;
        PassErrors errors;
    };
    // A group of a gun's contact rounds, or one time-fuzed shell, on its way.
    struct GunRoundsInFlight {
        std::size_t neighbour = 0;
        std::size_t target = 0;
        double arrive_s = 0.0;  // the group's predicted meeting; the shell's burst
        Vec3 muzzle;
        GunAim aim;
        int rounds = 0;
        bool shell = false;
        Vec3 burst;
    };
    static constexpr double kGunMuzzleUpM = 2.0;  // the guns' muzzles over the ground (as the picture's, SimFxEffects.cpp)
    std::vector<NeighbourDefenceState> nb_defence_;
    std::vector<NeighbourShot> nb_shots_;
    std::vector<GunRoundsInFlight> nb_rounds_;
    std::mt19937 defence_rng_{1u};
    bool defence_seeded_ = false;
    int nb_launches_ = 0;
    int nb_kills_ = 0;
    int nb_rounds_fired_ = 0;
    int nb_round_hits_ = 0;
    int nb_shell_bursts_ = 0;
    std::map<std::string, int> nb_damaged_;  // how often an aircraft's damage rose to each level short of destroyed

    static double GunRoundsPerMin(const std::string& sys) {
        return sys == "zsu234" ? 3400.0 : sys == "s60" ? 120.0 : sys == "ks30" ? 12.0 : (sys == "zu23" || sys == "aaa") ? 2000.0 : 0.0;
    }

    void FireNeighbourDefences() {
        if (!def_.waves) return;
        if (!defence_seeded_) {
            defence_rng_.seed(def_.seed + kDefenceSeed);
            defence_seeded_ = true;
        }
        if (nb_defence_.size() != def_.neighbours.size()) {
            nb_defence_.resize(def_.neighbours.size());
            for (std::size_t j = 0; j < def_.neighbours.size(); ++j) {
                const airwar::DefenceDef* d = def_.waves->Defence(def_.neighbours[j].system);
                nb_defence_[j].missiles = d != nullptr ? d->missiles : -1;
            }
        }
        EndNeighbourShots();
        EndGunRounds();
        CrashDamaged();
        for (std::size_t j = 0; j < def_.neighbours.size(); ++j) {
            const ScenarioNeighbour& nb = def_.neighbours[j];
            const airwar::DefenceDef* d = def_.waves->Defence(nb.system);
            if (nb.fights || d == nullptr) continue;
            NeighbourDefenceState& st = nb_defence_[j];
            const bool guns = d->missiles < 0;
            const double rpm = GunRoundsPerMin(nb.system);
            if (guns && rpm <= 0.0) continue;
            if (!guns && st.missiles == 0) {
                if (time_ < st.reload_s) continue;
                st.missiles = d->missiles;
                Event(nb.id, "reloaded");
            }
            const Vec3 site = GroundEnu(nb.position.x, nb.position.y);
            const double reach_m = nb.radius_m > 0.0 ? nb.radius_m : d->radius_m;
            // Who is in its reach: the nearest, and the nearest it has not yet engaged. One that has left is free to be
            // engaged again on its next pass.
            std::size_t near_i = 0, fresh_i = 0;
            const Aircraft* near_a = nullptr;
            const Aircraft* fresh_a = nullptr;
            double near_d = reach_m, fresh_d = reach_m;
            for (std::size_t i : wave_targets_) {
                const ScenarioTarget& t = def_.targets[i];
                if (t.friendly || t.weapon || !targets_[i].spawned || targets_[i].gone) continue;
                const Aircraft* a = world_.FindAircraft(targets_[i].world_id);
                if (a == nullptr || !a->alive) continue;
                const double dx = a->position.x - site.x, dy = a->position.y - site.y;
                const double dist = std::sqrt(dx * dx + dy * dy);
                if (dist >= reach_m) {
                    st.engaged.erase(i);
                    continue;
                }
                if (dist < near_d) {
                    near_d = dist;
                    near_a = a;
                    near_i = i;
                }
                if (st.engaged.count(i) == 0 && dist < fresh_d && !Targeted(j, i)) {
                    fresh_d = dist;
                    fresh_a = a;
                    fresh_i = i;
                }
            }
            const GunDef* gun = guns && def_.waves->has_damage ? def_.waves->aaa.Gun(nb.system) : nullptr;
            if (gun != nullptr) FireGunPass(j, *gun, site, st);
            if (time_ < st.next_s || st.passing) continue;
            if (gun != nullptr) {
                const Aircraft* a = fresh_a != nullptr ? fresh_a : near_a;
                const std::size_t i = fresh_a != nullptr ? fresh_i : near_i;
                if (a == nullptr) {
                    st.next_s = time_ + 1.0;
                    continue;
                }
                if (fresh_a != nullptr) st.engaged.insert(i);
                StartGunPass(j, *gun, site, i, *a, st);
                continue;
            }
            if (guns) {
                const Aircraft* a = fresh_a != nullptr ? fresh_a : near_a;
                const std::size_t i = fresh_a != nullptr ? fresh_i : near_i;
                if (a == nullptr) {
                    st.next_s = time_ + 1.0;  // nothing in reach: look again in a second
                    continue;
                }
                const double pass_s = 1.5 + 2.5 * Uniform01(defence_rng_);
                if (fresh_a != nullptr) {  // a new aircraft in its reach: this pass decides it
                    st.engaged.insert(i);
                    if (Uniform01(defence_rng_) < NeighbourKillChance(*d, reach_m, site, *a)) {
                        nb_shots_.push_back({j, i, time_ + pass_s, true, false, 0});
                    }
                }
                st.next_s = time_ + pass_s + 2.0 + 4.0 * Uniform01(defence_rng_);
                if (fx_on_) {
                    fx::Event e;
                    e.kind = fx::EventKind::GunPass;
                    e.t_s = time_;
                    e.end_t_s = time_ + pass_s;
                    e.position = site;
                    e.aim = a->position;
                    e.source = nb.id;
                    e.target = def_.targets[i].id;
                    e.world_id = a->id;
                    e.number = std::max(1, static_cast<int>(std::lround(rpm / 60.0 * pass_s)));  // a KS-30 fires a round or two
                    e.weapon = nb.system;
                    fx_.Push(e);
                }
                continue;
            }
            // A missile site: as many aircraft at once as its fire channels.
            if (fresh_a == nullptr) continue;
            int in_flight = 0;
            for (const NeighbourShot& s : nb_shots_) in_flight += s.neighbour == j && s.missile ? 1 : 0;
            if (airwar::EngagedAircraft(d->channels, 1.0, in_flight + 1) <= in_flight) continue;
            st.engaged.insert(fresh_i);
            const double k = NeighbourKillChance(*d, reach_m, site, *fresh_a);
            if (k <= 0.0) continue;  // out of its cover (too fast, too high or too low for it): it lets it pass
            const bool hit = Uniform01(defence_rng_) < k;
            const Vec3 rails = FxRails(nb.position);
            const double flight_s = std::min(kNbMissileMaxS, std::max(kNbMissileMinS, (fresh_a->position - rails).Length() / kNbMissileMps));
            --st.missiles;
            ++st.launches;
            ++nb_launches_;
            st.next_s = time_ + kNbLaunchGapS;
            if (st.missiles == 0) st.reload_s = time_ + kNbReloadS;
            nb_shots_.push_back({j, fresh_i, time_ + flight_s, hit, true, st.launches});
            Event(nb.id, "launches at " + def_.targets[fresh_i].id);
            if (fx_on_) {
                fx::Event e;
                e.kind = fx::EventKind::SamLaunch;
                e.t_s = time_;
                e.position = rails;
                e.velocity = FxLaunchVelocity(rails, fresh_a->position);
                e.aim = fresh_a->position;
                e.end_t_s = time_ + flight_s;
                e.source = nb.id;
                e.target = def_.targets[fresh_i].id;
                e.world_id = fresh_a->id;
                e.number = st.launches;
                e.weapon = nb.system;
                fx_.Push(e);
            }
        }
    }

    // A missile of neighbour j already on its way to aircraft i.
    bool Targeted(std::size_t j, std::size_t i) const {
        for (const NeighbourShot& s : nb_shots_) {
            if (s.neighbour == j && s.target == i) return true;
        }
        return false;
    }

    double NeighbourKillChance(const airwar::DefenceDef& d, double reach_m, const Vec3& site, const Aircraft& a) const {
        const double agl = world_.Frame().AltitudeOf(a.position) - world_.GetTerrain().HeightAt(a.position.x, a.position.y);
        const bool helicopter = a.speed_mps < 90.0;  // the runner does not know its type here: slow means a helicopter
        return airwar::WavePassKillChance(d, reach_m, site, a.position, a.velocity, agl, helicopter, def_.waves->tuning, light_.dark);
    }

    // The shots whose time has come: a hit brings its aircraft down (its wreck in the feed); a missile that missed, or
    // whose aircraft is already down, flies on and destroys itself.
    void EndNeighbourShots() {
        for (std::size_t k = 0; k < nb_shots_.size();) {
            const NeighbourShot s = nb_shots_[k];
            if (s.end_s > time_ + 1e-9) {
                ++k;
                continue;
            }
            nb_shots_.erase(nb_shots_.begin() + static_cast<std::ptrdiff_t>(k));
            const ScenarioNeighbour& nb = def_.neighbours[s.neighbour];
            const std::string id = def_.targets[s.target].id;
            const Aircraft* a = targets_[s.target].spawned ? world_.FindAircraft(targets_[s.target].world_id) : nullptr;
            if (s.missile && def_.waves->has_damage) {
                EndNeighbourMissile(s, a);
                continue;
            }
            const bool down = s.hit && a != nullptr && a->alive;
            if (s.missile && fx_on_ && a != nullptr) {
                fx::Event e;
                e.kind = fx::EventKind::SamEnd;
                e.t_s = e.end_t_s = time_;
                e.position = FxRails(nb.position);
                e.source = nb.id;
                e.target = id;
                e.world_id = a->id;
                e.number = s.number;
                e.weapon = nb.system;
                if (down) {
                    e.end = fx::BurstEnd::Hit;
                    e.aim = FxAround(a->position, 0.0, kFxHitOffsetM);
                } else {  // it flies on past the target and destroys itself
                    e.end = fx::BurstEnd::SelfDestruct;
                    Vec3 dir = a->position - e.position;
                    dir = dir.LengthSquared() > 1.0 ? dir.Normalized() : Vec3{0.0, 0.0, 1.0};
                    const double past = FxBetween(kFxPastMinM, kFxPastMaxM);
                    e.aim = a->position + dir * past;
                    e.velocity = dir * kFxPastMps;
                    e.end_t_s = time_ + past / kFxPastMps;
                }
                fx_.Push(e);
            }
            if (down && DestroyTarget(id, nb.name)) ++nb_kills_;
        }
    }

    // A gun site opens a pass at aircraft i (aaa.hpp): its fire control's errors drawn for the pass, the pass's length
    // and the pause after it game values (sam3d, as before). The picture gets the pass and its errors, so its tracers
    // miss as these rounds do; a time-fuzed gun's shells go to the feed one by one as they are fired (FireGunPass).
    void StartGunPass(std::size_t j, const GunDef& g, const Vec3& site, std::size_t i, const Aircraft& a, NeighbourDefenceState& st) {
        const FireControlDef* fc = def_.waves->aaa.Control(g.fire_control);
        const double pass_s = 1.5 + 2.5 * Uniform01(defence_rng_);
        st.passing = true;
        st.pass_target = i;
        st.pass_start_s = st.next_group_s = time_;
        st.pass_end_s = time_ + pass_s;
        st.rounds_due = 0.0;
        st.errors = fc != nullptr ? DrawPassErrors(*fc, a.velocity, light_.dark, defence_rng_) : PassErrors{};
        st.next_s = st.pass_end_s + 2.0 + 4.0 * Uniform01(defence_rng_);
        if (fx_on_ && !g.time_fuze) {
            const ScenarioNeighbour& nb = def_.neighbours[j];
            fx::Event e;
            e.kind = fx::EventKind::GunPass;
            e.t_s = time_;
            e.end_t_s = st.pass_end_s;
            e.position = site;
            e.aim = a.position;
            e.source = nb.id;
            e.target = def_.targets[i].id;
            e.world_id = a.id;
            e.number = std::max(1, static_cast<int>(std::lround(g.rate_rpm / 60.0 * pass_s)));
            e.weapon = nb.system;
            e.aim_across_rad = st.errors.across_rad;
            e.aim_up_rad = st.errors.up_rad;
            e.range_share = st.errors.range_share;
            e.lead_error = st.errors.velocity_mps;
            e.correction_per_s = fc != nullptr ? fc->correction_per_s : 0.0;
            e.spread_mrad = g.dispersion_mrad;
            fx_.Push(e);
        }
        FireGunPass(j, g, site, st);
    }

    // The pass's groups of rounds due by now, each aimed where the fire control predicts the aircraft from where it was
    // when the group left (its errors as corrected so far), and on its way.
    void FireGunPass(std::size_t j, const GunDef& g, const Vec3& site, NeighbourDefenceState& st) {
        if (!st.passing) return;
        const std::size_t i = st.pass_target;
        const Aircraft* a = targets_[i].spawned && !targets_[i].gone ? world_.FindAircraft(targets_[i].world_id) : nullptr;
        if (a == nullptr || !a->alive || st.next_group_s >= st.pass_end_s) {
            st.passing = false;
            return;
        }
        const FireControlDef* fc = def_.waves->aaa.Control(g.fire_control);
        const Vec3 muzzle = site + Vec3{0.0, 0.0, kGunMuzzleUpM};
        const ScenarioNeighbour& nb = def_.neighbours[j];
        while (st.next_group_s <= time_ + 1e-9 && st.next_group_s < st.pass_end_s) {
            const double fired_s = st.next_group_s;
            st.rounds_due += g.rate_rpm / 60.0 * g.group_s;
            st.next_group_s += g.group_s;
            const int n = static_cast<int>(std::floor(st.rounds_due + 1e-9));
            if (n <= 0) continue;
            st.rounds_due -= n;
            const PassErrors e = fc != nullptr ? Corrected(st.errors, *fc, fired_s - st.pass_start_s) : PassErrors{};
            const GunAim aim = AimGun(g, muzzle, a->position - a->velocity * (time_ - fired_s), a->velocity, e);
            nb_rounds_fired_ += n;
            if (!g.time_fuze) {
                GunRoundsInFlight r;
                r.neighbour = j;
                r.target = i;
                r.arrive_s = fired_s + aim.tof_s;
                r.muzzle = muzzle;
                r.aim = aim;
                r.rounds = n;
                nb_rounds_.push_back(r);
                continue;
            }
            for (int k = 0; k < n; ++k) {
                const ShellBurst b = TimeShellBurst(g, muzzle, aim, defence_rng_);
                GunRoundsInFlight r;
                r.neighbour = j;
                r.target = i;
                r.arrive_s = fired_s + b.after_s;
                r.muzzle = muzzle;
                r.aim = aim;
                r.shell = true;
                r.burst = b.position;
                nb_rounds_.push_back(r);
                if (fx_on_) {
                    fx::Event ev;
                    ev.kind = fx::EventKind::FlakBurst;
                    ev.t_s = fired_s;
                    ev.end_t_s = fired_s + b.after_s;
                    ev.position = muzzle;
                    ev.velocity = b.dir * g.muzzle_mps;
                    ev.aim = b.position;
                    ev.source = nb.id;
                    ev.target = def_.targets[i].id;
                    ev.world_id = a->id;
                    ev.weapon = nb.system;
                    fx_.Push(ev);
                }
            }
        }
    }

    // The groups and shells whose time has come, judged against where their aircraft is then (back along its velocity to
    // the moment): a group's hits, a shell's burst at its distance, and what they did (damage.hpp).
    void EndGunRounds() {
        const DamageTable& dt = def_.waves->damage;
        for (std::size_t k = 0; k < nb_rounds_.size();) {
            const GunRoundsInFlight r = nb_rounds_[k];
            if (r.arrive_s > time_ + 1e-9) {
                ++k;
                continue;
            }
            nb_rounds_.erase(nb_rounds_.begin() + static_cast<std::ptrdiff_t>(k));
            const ScenarioNeighbour& nb = def_.neighbours[r.neighbour];
            const GunDef* g = def_.waves->aaa.Gun(nb.system);
            const Aircraft* a = targets_[r.target].spawned ? world_.FindAircraft(targets_[r.target].world_id) : nullptr;
            if (r.shell) ++nb_shell_bursts_;
            if (g == nullptr || a == nullptr || !a->alive) continue;
            const Vec3 at = a->position - a->velocity * (time_ - r.arrive_s);
            const double area = PresentedAreaM2(dt, def_.targets[r.target].size_m);
            DamageDraw d;
            if (r.shell) {
                d = DrawBurst(dt, g->mass_kg, g->charge_kg, 0.0, (r.burst - at).Length(), area, defence_rng_);
            } else {
                const int hits = GroupHits(*g, r.muzzle, r.aim, at, r.rounds, area, defence_rng_);
                if (hits <= 0) continue;
                nb_round_hits_ += hits;
                d = DrawRoundHits(dt, g->calibre_mm, hits, defence_rng_);
            }
            if (d.Any()) OnDamage(r.target, ApplyDamage(dt, targets_[r.target].damage, d, time_, defence_rng_, nb.name));
        }
    }

    // A neighbour's missile at its meeting (with the damage tables): its kill still the chance per pass, drawn into
    // destroyed or heavy damage; a miss bursting near enough to strike (and seen to burst there), or flying on to destroy
    // itself.
    void EndNeighbourMissile(const NeighbourShot& s, const Aircraft* a) {
        const ScenarioNeighbour& nb = def_.neighbours[s.neighbour];
        const std::string& id = def_.targets[s.target].id;
        if (a == nullptr) return;
        double near_m = -1.0;
        DamageChange c;
        if (a->alive) {
            const scenery::MunitionSpec* m = def_.waves->aaa.munitions.Resolve(nb.system);
            const double charge = m != nullptr && m->charge_kg > 0.0 ? m->charge_kg : 10.0;
            c = ApplyMissile(def_.waves->damage, targets_[s.target].damage, s.hit, charge,
                             PresentedAreaM2(def_.waves->damage, def_.targets[s.target].size_m), time_, defence_rng_, nb.name, &near_m);
        }
        if (fx_on_) {
            fx::Event e;
            e.kind = fx::EventKind::SamEnd;
            e.t_s = e.end_t_s = time_;
            e.position = FxRails(nb.position);
            e.source = nb.id;
            e.target = id;
            e.world_id = a->id;
            e.number = s.number;
            e.weapon = nb.system;
            if (s.hit && a->alive) {
                e.end = fx::BurstEnd::Hit;
                e.aim = FxAround(a->position, 0.0, kFxHitOffsetM);
            } else if (near_m > 0.0) {
                e.end = fx::BurstEnd::NearBurst;
                e.aim = FxAround(a->position, near_m, near_m);
            } else {
                e.end = fx::BurstEnd::SelfDestruct;
                Vec3 dir = a->position - e.position;
                dir = dir.LengthSquared() > 1.0 ? dir.Normalized() : Vec3{0.0, 0.0, 1.0};
                const double past = FxBetween(kFxPastMinM, kFxPastMaxM);
                e.aim = a->position + dir * past;
                e.velocity = dir * kFxPastMps;
                e.end_t_s = time_ + past / kFxPastMps;
            }
            fx_.Push(e);
        }
        OnDamage(s.target, c);
    }

    // What damage did to aircraft i: destroyed now (its wreck as any kill's), or damaged and flying on: said in the
    // debrief, its smoke and fire in the feed, its load thrown off, its turn for home.
    void OnDamage(std::size_t i, const DamageChange& c) {
        if (!c.Raised()) return;
        TargetState& ts = targets_[i];
        const std::string id = def_.targets[i].id;
        if (c.after == DamageLevel::Destroyed) {
            if (DestroyTarget(id, ts.damage.by)) ++nb_kills_;
            return;
        }
        ++nb_damaged_[ToString(c.after)];
        if (c.after < DamageLevel::Moderate) return;
        Aircraft* a = ts.spawned ? world_.FindAircraft(ts.world_id) : nullptr;
        if (a == nullptr || !a->alive) return;
        std::string what = std::string(ToString(c.after)) + " damage from " + ts.damage.by;
        if (c.fire) what += ", on fire";
        if (c.crashes) what += ", going down";
        if (c.jettison) what += ", jettisons its load";
        if (c.abort) what += ", turns for home";
        Event(id, what);
        if (fx_on_) {
            fx::Event e;
            e.kind = fx::EventKind::AircraftDamaged;
            e.t_s = time_;
            e.position = e.aim = a->position;
            e.velocity = a->velocity;
            e.source = ts.damage.by;
            e.target = id;
            e.world_id = a->id;
            e.number = static_cast<int>(c.after);
            e.fire = ts.damage.fire;
            e.falls = ts.damage.smoke;
            fx_.Push(e);
        }
        if (c.jettison && waves_) {
            airwar::WaveScene scene;
            scene.frame = &world_.Frame();
            scene.terrain = &world_.GetTerrain();
            scene.centre = def_.unit_position;
            for (airwar::WaveRelease& r : waves_->Jettison(id, time_, a->position, a->velocity, scene)) ReleaseWaveLoad(std::move(r));
        }
        if (c.abort) {  // the pilot's own: straight for his way out, out of his flight (its lead passes on)
            FlightOrder home;
            home.kind = OrderKind::ReturnHome;
            home.by = "pilot";
            flights_->OrderMember(static_cast<int>(i), home, time_, true, "the lead turns for home, damaged");
        }
    }

    // The heavily damaged whose time has come fall (burning if they were on fire), credited to who did it.
    void CrashDamaged() {
        for (std::size_t i : wave_targets_) {
            TargetState& ts = targets_[i];
            if (ts.damage.crash_at_s < 0.0 || time_ < ts.damage.crash_at_s || !ts.spawned) continue;
            ts.damage.crash_at_s = -1.0;
            const Aircraft* a = world_.FindAircraft(ts.world_id);
            if (a == nullptr || !a->alive) continue;
            const std::string id = def_.targets[i].id;
            FxDown(id, ts.damage.by, ts.damage.fire, true);
            if (DestroyTarget(id, ts.damage.by, false)) ++nb_kills_;
        }
    }

    // The loads whose time has come strike the ground: an impact in the debrief, and the feed's (the game bursts any load
    // it did not fly itself).
    void LandWaveLoads() {
        for (std::size_t k = 0; k < wave_loads_.size();) {
            const airwar::WaveRelease& r = wave_loads_[k];
            if (r.impact_t_s > time_ + 1e-9) {
                ++k;
                continue;
            }
            ++weapon_impacts_;
            Event(r.carrier, "impact: its " + r.weapon_name + " strike the ground");
            if (fx_on_) {
                fx::Event e;
                e.kind = fx::EventKind::WeaponImpact;
                e.t_s = e.end_t_s = r.impact_t_s;
                e.position = e.aim = r.impact;
                e.source = r.carrier;
                e.world_id = TargetWorldId(r.carrier);
                e.number = r.count;
                e.weapon = r.weapon;
                e.delivery = r.delivery;
                e.end = fx::BurstEnd::Ground;
                fx_.Push(e);
            }
            wave_loads_.erase(wave_loads_.begin() + static_cast<std::ptrdiff_t>(k));
        }
    }

    void EnterWaveTarget(ScenarioTarget t) {
        t.appear_at_s = std::max(t.appear_at_s, time_);
        const std::size_t i = def_.targets.size();
        wave_index_[t.id] = i;
        EnterTarget(std::move(t));
        wave_targets_.push_back(i);
    }

    void UpdateWaves() {
        if (!def_.waves) return;
        if (!waves_) waves_ = std::make_unique<airwar::TestWaves>(def_.waves, def_.seed);
        airwar::WaveScene scene;
        scene.frame = &world_.Frame();
        scene.terrain = &world_.GetTerrain();
        scene.centre = def_.unit_position;
        scene.defences.push_back(def_.unit_position);
        for (const ScenarioNeighbour& nb : def_.neighbours) scene.defences.push_back(nb.position);
        if (battery_) {
            for (const Battery::Vehicle& v : battery_->Vehicles()) scene.defences.push_back(v.position);
        }
        for (const ScenarioScenery& s : def_.scenery) scene.objects.push_back(s.position);
        scene.em = em_link_ ? &em_link_->World().Scene().lib : nullptr;
        if (waves_->Due(time_, WavesAirborne())) {
            airwar::Wave w = waves_->Next(time_, scene);
            char who[32];
            std::snprintf(who, sizeof(who), "wave %d", w.number);
            std::string e;
            if (em_link_ && !w.jammers.empty() && !AddEmGear(def_.em.library, w.jammers, who, &e)) Event(who, "its jammers stay out: " + e);
            const std::size_t n = w.aircraft.size();
            const std::size_t first = def_.targets.size();
            if (!AddFlights(w.flight_defs, {}, &e)) Event(who, "its flights fly without their formations: " + e);
            for (ScenarioTarget& t : w.aircraft) EnterWaveTarget(std::move(t));
            RegisterFlights(first);
            std::string flights;
            for (const std::string& f : w.flights) flights += (flights.empty() ? "" : "; ") + f;
            log_.Record({time_, DebriefEntry::Kind::Scenario, who, "enters", std::to_string(n) + " aircraft: " + flights});
        }
        const auto lookup = [this](const std::string& id, Vec3& position, Vec3& velocity) {
            const auto it = wave_index_.find(id);
            if (it == wave_index_.end()) return false;
            const TargetState& s = targets_[it->second];
            const Aircraft* a = s.spawned && !s.gone ? world_.FindAircraft(s.world_id) : nullptr;
            if (a == nullptr || !a->alive) return false;
            position = a->position;
            velocity = a->velocity;
            return true;
        };
        for (airwar::WaveRelease& r : waves_->Releases(time_, scene, lookup)) ReleaseWaveLoad(std::move(r));
        LandWaveLoads();
        FireNeighbourDefences();
        // The long gone leave the world.
        const double after_s = def_.waves->remove_after_s;
        for (std::size_t k = 0; k < wave_targets_.size();) {
            const std::size_t i = wave_targets_[k];
            if (!targets_[i].gone) {
                ++k;
                continue;
            }
            const auto g = wave_gone_s_.emplace(i, time_).first;
            Aircraft* a = world_.FindAircraft(targets_[i].world_id);
            if (a != nullptr && time_ - g->second >= after_s && a->alive) a->alive = false;  // let the sensors drop it
            if (a != nullptr && time_ - g->second < after_s + 5.0) {
                ++k;
                continue;
            }
            if (a != nullptr) world_.RemoveAircraft(targets_[i].world_id);
            wave_index_.erase(def_.targets[i].id);
            wave_gone_s_.erase(g);
            wave_targets_.erase(wave_targets_.begin() + static_cast<std::ptrdiff_t>(k));
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

    // ---- Anti-radiation missiles (roadmap E6, em/arm.hpp). A carrier (a target's "arms") fires on what its warning
    // receiver hears: a hostile radar in its weapon's band, with the place it believes the radar is within its launch
    // range; each ARM is set to the signal heard. After its last one it turns away (the sign the crew watches for:
    // voenizdat_osa_1991 p262). The ARM itself is no scenario target: on the scope it is a ghost mark. Each one's end is
    // logged; one that bursts on the vehicle ends the scenario.
    struct ArmCarrierState {
        int fired = 0;
        double last_s = -1e9;
        bool turned = false;
    };
    std::vector<ArmCarrierState> arm_carriers_;
    std::vector<EmSensorLink::ArmResult> arm_results_;
    int arms_fired_ = 0;
    static constexpr int kArmAcmiBase = 0x7E000;
    static constexpr double kArmBearingTolRad = 0.35;  // sam3d: about 20 degrees, a warning receiver's bearing on the briefed target
    static constexpr double kArmTurnAwayM = 40000.0;   // sam3d: an ARM carrier, its missiles gone, turns away this far and out
    std::vector<int> arm_acmi_shown_;

    void UpdateArms() {
        if (!em_link_ || finished_) return;
        arm_carriers_.resize(def_.targets.size());
        for (const EmSensorLink::ArmResult& r : em_link_->TakeArmResults()) {
            arm_results_.push_back(r);
            if (fx_on_) {
                fx::Event burst;  // the feed: where it burst (the EM ARM itself is drawn as an entity while it flies)
                burst.kind = fx::EventKind::ArmBurst;
                burst.t_s = time_;
                burst.end_t_s = time_;
                burst.position = burst.aim = r.burst_point;
                burst.end = r.hit ? fx::BurstEnd::Hit : fx::BurstEnd::Ground;
                burst.source = r.carrier;
                burst.weapon = r.weapon;
                fx_.Push(burst);
            }
            char m[32];
            std::snprintf(m, sizeof(m), "%.0f", r.miss_m);
            const std::string who = r.carrier.empty() ? std::string("ARM") : r.carrier;
            if (r.hit && r.hit_what == "unit") {
                Event(who, std::string("its anti-radiation missile bursts ") + m + " m from the vehicle: out of action");
                if (!def_.end_on_unit_hit) continue;  // the testbed runs on
                Finish("the vehicle was hit by an anti-radiation missile");
                return;
            }
            if (r.hit) {
                Event(who, std::string("its anti-radiation missile bursts ") + m + " m from the " + r.hit_what + ": out of action");
            } else if (!r.nearest_what.empty()) {
                Event(who, std::string("its anti-radiation missile misses: it bursts ") + m + " m from the " + r.nearest_what);
            } else {
                Event(who, "its anti-radiation missile misses");
            }
        }
        const em::EmLibrary& lib = em_link_->World().Scene().lib;
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            const ScenarioTarget& t = def_.targets[i];
            if (t.arms.count <= 0 || !targets_[i].spawned || targets_[i].gone) continue;
            Aircraft* a = world_.FindAircraft(targets_[i].world_id);
            const int w = lib.Arm(t.arms.weapon);
            if (a == nullptr || !a->alive || w < 0) continue;
            const em::ArmDef& weapon = lib.arms[static_cast<std::size_t>(w)];
            ArmCarrierState& c = arm_carriers_[i];
            const Vec3 aim = t.arms.has_aim ? t.arms.aim : def_.unit_position;
            if (c.fired < t.arms.count) {
                if (time_ - c.last_s < t.arms.interval_s) continue;
                EmSensorLink::RwrSignal sig;
                if (!em_link_->RwrRadarInBand(t.id, weapon.freq_min_hz, weapon.freq_max_hz, AzimuthOf(aim - a->position), kArmBearingTolRad, sig)) continue;
                const double range = (aim - a->position).Length();
                if (range > t.arms.launch_range_m || range < t.arms.min_range_m) continue;
                if (em_link_->LaunchArmFrom(t.id, t.arms.weapon, aim, sig) == 0) continue;
                ++c.fired;
                ++arms_fired_;
                c.last_s = time_;
                char km[32];
                std::snprintf(km, sizeof(km), "%.1f", range / 1000.0);
                Event(t.id, "fires an anti-radiation missile (" + weapon.name + ") at " + (sig.name.empty() ? std::string("a radar") : sig.name) +
                                ", " + km + " km");
            } else if (t.arms.turn_away && !c.turned && time_ - c.last_s >= 1.0) {
                // Its missiles gone, a hard turn away from the radar and out, 40 km off: the pilot's own (an override
                // that ends there; a leader's flight goes home behind him).
                c.turned = true;
                Vec3 away = a->position - aim;
                away.z = 0.0;
                away = away.LengthSquared() > 1.0 ? away.Normalized() : Vec3{0.0, 1.0, 0.0};
                PilotOverride ov;
                ov.kind = PilotOverride::Kind::TurnAway;
                ov.end = PilotOverride::End::Point;
                ov.after = PilotOverride::After::Home;
                ov.point = Vec3{a->position.x + away.x * kArmTurnAwayM, a->position.y + away.y * kArmTurnAwayM, a->position.z};
                ov.turn_rate_rad_s = std::max(a->max_turn_rate_rad_s, t.evade_turn_rate_rad_s);  // a hard turn
                ov.until_s = time_ + 1e6;  // at the point
                if (flights_->Overriding(static_cast<int>(i))) targets_[i].pilot.OverrideEnded();
                flights_->Override(static_cast<int>(i), ov);
                Event(t.id, "turns away from the radar");
            }
        }
    }

public:
    // Anti-radiation missiles in flight (roadmap E6), for display: empty without the EM layer.
    std::vector<EmSensorLink::ArmView> ArmsInFlight() const {
        return em_link_ ? em_link_->ArmsInFlight() : std::vector<EmSensorLink::ArmView>{};
    }
    // Every ARM that has ended, in order (hit or missed, by how far, what was hit); and how many the carriers fired.
    const std::vector<EmSensorLink::ArmResult>& ArmResults() const { return arm_results_; }
    int ArmsFired() const { return arms_fired_; }

private:
    void WriteArmsAcmi() {
        if (!em_link_) return;
        std::vector<int> shown;
        for (const EmSensorLink::ArmView& v : em_link_->ArmsInFlight()) {
            acmi_.WriteObject(kArmAcmiBase + v.id, v.position, v.velocity, "Type=Weapon+Missile,Name=" + v.name + ",Color=Blue");
            shown.push_back(v.id);
        }
        for (int id : arm_acmi_shown_) {
            if (std::find(shown.begin(), shown.end(), id) == shown.end()) acmi_.RemoveObject(kArmAcmiBase + id);
        }
        arm_acmi_shown_ = shown;
    }

    // A missile was launched at a target (`launch`), or the tracking radar holds it: its pilot may evade.
    void CueTarget(const std::string& id, bool launch) {
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            TargetState& s = targets_[i];
            if (def_.targets[i].id != id || !s.spawned || s.gone) continue;
            Aircraft* a = world_.FindAircraft(s.world_id);
            if (a == nullptr) return;
            const Evasion ev = launch ? s.pilot.OnLaunch(*a, def_.unit_position, time_) : s.pilot.OnLocked(*a, def_.unit_position, time_);
            if (!ev.what.empty()) Event(id, ev.what);
            if (ev.steer) flights_->Override(static_cast<int>(i), ev.ov);  // a reflex: it does not wait for the leader
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
                if (battery_) BatteryTakes(a.battery_order);
                return;
            case TriggerAction::Kind::Breakdown:
                if (battery_) battery_->Breakdown(a.vehicle, time_, a.reason);
                return;
            case TriggerAction::Kind::AirThreat:
                if (battery_) {
                    battery_->SetAirThreat(a.air_threat, time_);
                    air_threat_forced_ = a.air_threat;
                }
                raid_warned_s_ = a.air_threat ? time_ : -1.0;  // a raid announced (the warning), not the standing readiness
                return;
            case TriggerAction::Kind::Instructor: {  // the trainer's scripted drill (roadmap 10.3)
                std::string why;
                if (!Instruct(a.instructor, &why)) {
                    log_.Record({time_, DebriefEntry::Kind::Scenario, "Instructor", a.instructor.target,
                                 std::string(ToString(a.instructor.kind)) + " not done: " + why});
                }
                return;
            }
        }
    }

    // A battery order come due (the scenario's, or the battery commander's plan's): his to give from the post when it is
    // one of the menu's moves (G10.8), else the battery takes it.
    void ScheduledOrder(const BatteryOrder& o) {
        if (PostGivesOrders() && !PostOrderId(o.kind).empty() && o.from != "regiment") {  // his to give when it is due (G10.8)
            post_planned_.push_back(o);
            return;
        }
        BatteryTakes(o);
    }

    // The battery around the player's vehicle (roadmap 7.5): scheduled orders, the air-threat spacing from what the crew
    // has been told, the vehicles' moves; the other launchers' positions feed their neighbour sources.
    void UpdateBattery(double dt) {
        if (!battery_) return;
        for (std::size_t i = 0; i < def_.battery_orders.size(); ++i) {
            if (battery_orders_done_[i] || def_.battery_orders[i].at_s > time_ + 1e-9) continue;
            battery_orders_done_[i] = 1;
            const BatteryOrder& o = def_.battery_orders[i];
            // The regime ordered at 0 s is the combat order's: the commander knows it before the battery deploys.
            if (o.at_s <= 0.0 && (o.kind == BatteryOrder::Kind::Emcon || o.kind == BatteryOrder::Kind::Emission)) regime_from_combat_order_ = true;
            ScheduledOrder(o);
        }
        for (std::size_t i = 0; i < task_orders_.size(); ++i) {  // the battery commander's plan for the regiment's task (14.3)
            if (task_orders_done_[i] || task_orders_[i].at_s > time_ + 1e-9) continue;
            task_orders_done_[i] = 1;
            ScheduledOrder(task_orders_[i]);
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
        if (order_net_) {
            LogMarchPoints();
            UpdateTask();
            RegimentStep();
            StepOrderNet();
            UpdateOwnOrder();
            PostOwesOrders();
            PostStowsForMarch();
            UpdateSupply();
        }
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
                Radio(light_.dark ? "lamp signals" : "flag signals", e.who, e.text, false, SignalSeers());
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
    // Who in the own vehicle sees flag and lamp signals: its driver and commander (the post's too).
    std::vector<std::string> SignalSeers() const {
        std::vector<std::string> seen;
        for (const char* st : {"driver", "commander", "battery_commander", "squad_leader"}) {
            if (console_.FindStation(st) != nullptr) seen.push_back(st);
        }
        return seen;
    }

    // ---- The battery's orders carried by a means (roadmap G10.5; command/battery_orders.hpp, "battery.by_means")

    void StartOrderNet() {
        const Battery::Vehicle* giver = battery_->Find(def_.battery_giver);
        // The order books keep the day's hours when the scenario has a clock ("clock_start"), else the mission's own.
        const double hour_at_zero = def_.clock_start_s >= 0.0 ? def_.clock_start_s / 3600.0 : 0.0;
        order_net_ = std::make_unique<command::BatteryOrderNet>(def_.battery_orders_def, def_.battery_giver_post,
                                                               giver != nullptr ? giver->def.callsign : def_.battery_giver, hour_at_zero);
        order_net_->SetNight(light_.dark);
        for (const command::OrderVehicle& c : def_.battery_crews) order_net_->Add(c);
        KeepOrderNetCurrent();
        // The regiment's orders to the battery commander, on the regiment's command net (his R-111 in the PU-12M).
        const RadioNetDef* rn = def_.radio_data.Find("regiment");
        regiment_net_ = std::make_unique<command::BatteryOrderNet>(def_.battery_orders_def, "rgt_cp_duty",
                                                                  rn != nullptr ? rn->main_call_sign : std::string("regiment"), hour_at_zero);
        command::OrderVehicle bc;
        bc.vehicle = def_.battery_giver;
        bc.callsign = BatteryPostCall();
        bc.post = def_.battery_giver_post;
        bc.commander = "battery commander";
        if (giver != nullptr) bc.at = giver->position;
        regiment_net_->Add(bc);
    }
    static bool MovesBattery(BatteryOrder::Kind k) {
        return k == BatteryOrder::Kind::March || k == BatteryOrder::Kind::ShortHalt || k == BatteryOrder::Kind::CombatFormation ||
               k == BatteryOrder::Kind::Halt || k == BatteryOrder::Kind::Resume || k == BatteryOrder::Kind::Ambush ||
               k == BatteryOrder::Kind::RiverCrossing || k == BatteryOrder::Kind::FarBankHeld || k == BatteryOrder::Kind::Relocate;
    }
    // Orders a vehicle reports done by «к бою готов» once it stands deployed.
    static bool DeploysBattery(BatteryOrder::Kind k) {
        return k == BatteryOrder::Kind::ShortHalt || k == BatteryOrder::Kind::CombatFormation || k == BatteryOrder::Kind::Ambush ||
               k == BatteryOrder::Kind::RiverCrossing || k == BatteryOrder::Kind::Relocate;
    }
    // The words of an order or a report: the crew data's (data/crew/<system>_crew.json), else `fallback`.
    std::string CrewWords(const char* callout, const std::string& fallback) const {
        const CalloutDef* c = def_.crew_data.FindCallout(callout);
        return c != nullptr && !c->ru.empty() ? c->ru : fallback;
    }
    std::string BatteryOrderWords(BatteryOrder::Kind k) const {
        switch (k) {
            case BatteryOrder::Kind::March: return CrewWords("order_march", "К маршу!");
            case BatteryOrder::Kind::ShortHalt:
            case BatteryOrder::Kind::Halt: return CrewWords("order_halt", "Стой!");
            case BatteryOrder::Kind::Resume: return CrewWords("order_go", "Вперёд!");
            case BatteryOrder::Kind::CombatFormation: return CrewWords("order_k_boyu", "К бою!");
            case BatteryOrder::Kind::Relocate: return CrewWords("order_relocate", "Смена позиции!");
            default: return BatteryOrderName(k);
        }
    }
    // Every battery order goes through here. With orders by a means a moving order goes out on the battery net (flags or
    // lamps under the march's radio ban), and each of the battery's other vehicles stays where it is until its commander
    // has heard it; the own vehicle's crew hears it seat by seat as before. The emission regime and the service runs the
    // battery takes at once (G10.7 puts the regime on this path).
    void BatteryTakes(BatteryOrder o) {
        if (!o.to_place.empty() && o.route.empty() && o.from != "regiment") PlanToPlace(o);
        if (regiment_net_ != nullptr && o.from == "regiment") {  // to the battery commander first (G10.7)
            RegimentGives(o);
            return;
        }
        if (order_net_ != nullptr && MovesBattery(o.kind)) {
            KeepOrderNetCurrent();
            std::vector<std::string> to;
            for (const command::OrderVehicle& c : order_net_->Vehicles()) {
                const Battery::Vehicle* bv = battery_->Find(c.vehicle);
                if (bv != nullptr && bv->away) continue;  // out on a run of its own (17.4): not in this move
                battery_->SetAbsent(c.vehicle, true);
                to.push_back(c.vehicle);
            }
            const std::string mean = MarchRadioBan() ? (light_.dark ? "lamps" : "flags") : "radio_voice";
            battery_order_ = order_net_->Give(BatteryOrderName(o.kind), BatteryOrderWords(o.kind), mean, time_, to);
            battery_order_kind_ = o.kind;
            order_words_[battery_order_] = KindWords(o.kind);
            GiverJournal().Heard(time_, mean == "radio_voice" ? "battery net" : mean, "Battery commander", BatteryOrderWords(o.kind));
            const std::string own = OwnOnOrderNet();
            if (!own.empty() && mean == "radio_voice" && CmdrHearsBatteryNet()) {
                order_net_->Reach(battery_order_, own, time_ + order_net_->Def().say_s, time_);
            }
        }
        battery_->Order(o, time_);
        if (o.kind == BatteryOrder::Kind::March || o.kind == BatteryOrder::Kind::Relocate) {
            // The battery leaves its position: once it stands ready at the new one its commander reports that up again.
            regiment_reported_ = false;
            moved_since_report_ = true;
        }
    }
    // A march to a place (roadmap G2.3): the battery commander's march plan on the roads from where the column stands,
    // for its slowest vehicle, moving off now; the order carries the plan's route, speeds, points and order of march. The
    // plan is kept: its route card goes into the own vehicle's map cases, its order of march into the battery commander's.
    void PlanToPlace(BatteryOrder& o) {
        if (!nav_ || !battery_) return;
        std::vector<nav::MarchVehicle> column;
        for (const Battery::Vehicle& v : battery_->Vehicles()) {
            nav::MarchVehicle m;
            m.def = v.def;
            m.def.start = v.position;
            column.push_back(m);
        }
        if (column.empty()) return;
        const Vec3 head = battery_->Own() != nullptr ? battery_->Own()->position : column.front().def.start;
        std::string why;
        nav::MarchPlan plan = nav_->PlanTo({head.x, head.y}, o.to_place, column, battery_->Tactics(), time_, &why, DarkAt(time_));
        if (!plan.found) {
            Event("battery", "no march to " + o.to_place + ": " + why);
            return;
        }
        BatteryOrder planned = plan.Order();
        planned.at_s = o.at_s;
        planned.from = o.from;
        planned.to_place = o.to_place;
        o = planned;
        march_plan_ = plan;
        has_march_plan_ = true;
        char text[200];
        std::snprintf(text, sizeof(text), "march planned to %s: %.1f km by the roads, the head there at %s", plan.request.to_name.c_str(),
                      plan.route.length_m / 1000.0, SessionClock(plan.end_s).c_str());
        Event("battery", text);
        HandInMarchPapers();
    }
    // "1:05:30" for a session second.
    static std::string SessionClock(double s) {
        const long long t = std::llround(std::max(0.0, s));
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", t / 3600, (t / 60) % 60, t % 60);
        return buf;
    }
    // The march's papers (G9.3, G9.4): the own vehicle's route card to its commander and its driver, the order of march
    // with every vehicle's times to the battery commander when he is in the own vehicle (the PU-12M).
    void HandInMarchPapers() {
        if (!has_march_plan_ || !battery_ || battery_->Own() == nullptr) return;
        const std::string own = battery_->Own()->def.id;
        for (const nav::RouteCard& c : march_plan_.cards) {
            if (c.vehicle != own) continue;
            std::string text = "Vehicle " + c.callsign + ": place " + std::to_string(c.place) + " in the column of " +
                               std::to_string(march_plan_.cards.size()) + ", to " + march_plan_.request.to_name + ".";
            for (std::size_t i = 0; i < c.at_s.size() && i < march_plan_.points.size(); ++i) {
                const nav::MarchPoint& p = march_plan_.points[i];
                text += "\n" + SessionClock(c.at_s[i]) + "  " + p.kind + (p.name.empty() ? std::string() : ": " + p.name);
                if (p.halt_s > 0.0) text += " (halt " + std::to_string(static_cast<int>(p.halt_s / 60.0)) + " min)";
            }
            for (const std::string& seat : {Cmdr(), std::string("driver")}) {
                HeldPaper paper;
                paper.id = "route_card:" + std::to_string(own_papers_.size());
                paper.station = seat;
                paper.time_s = time_;
                paper.title = "Route card, vehicle " + c.callsign;
                paper.text = text;
                paper.from = "the battery commander";
                own_papers_.push_back(paper);
            }
        }
        if (PostGivesOrders()) {
            std::string text = "To " + march_plan_.request.to_name + ", " + std::to_string(static_cast<int>(march_plan_.route.length_m / 1000.0 + 0.5)) +
                               " km; spacing " + std::to_string(static_cast<int>(march_plan_.spacing_m)) + " m.";
            for (const nav::RouteCard& c : march_plan_.cards) {
                text += "\n" + std::to_string(c.place) + ". " + c.callsign;
                if (!c.at_s.empty()) text += ": at the start point " + SessionClock(c.at_s.front()) + ", at the end " + SessionClock(c.at_s.back());
            }
            HeldPaper paper;
            paper.id = "order_of_march:" + std::to_string(own_papers_.size());
            paper.station = Cmdr();
            paper.time_s = time_;
            paper.title = "Order of march";
            paper.text = text;
            paper.from = "your own planning";
            own_papers_.push_back(paper);
        }
    }
    // The battery commander's papers from the start (G8, G9.4), when he is in the own vehicle: his battery's staff list and
    // his communications diagram, as the chain's data has them at the session's start.
    void HandInBatteryPapers() {
        if (!PostGivesOrders() || !def_.battery_chain) return;
        command::InfoNet net;
        if (!net.Init(*def_.battery_chain, 1u, 0.0)) return;
        const command::Position* me = def_.battery_chain->FindPosition(def_.battery_giver_post);
        if (me == nullptr) return;
        const command::StaffList list = command::IssueStaffList(net, me->unit, me->id, 0.0);
        std::string staff;
        for (const command::StaffLine& l : list.lines) {
            if (!staff.empty()) staff += "\n";
            staff += l.title + ": " + (l.name.empty() ? std::string("vacant") : l.rank + " " + l.name);
        }
        std::string diagram;
        for (const command::DiagramLine& l : command::CommsDiagram(net, me->id)) {
            if (!diagram.empty()) diagram += "\n";
            diagram += l.title + " (" + net.PlaceName(l.place) + "):";
            for (std::size_t i = 0; i < l.means.size(); ++i) {
                const command::SignalMean* m = def_.battery_chain->FindMean(l.means[i]);
                diagram += (i == 0 ? " " : ", ") + (m != nullptr ? m->name : l.means[i]);
            }
        }
        own_papers_.push_back({"staff_list", Cmdr(), 0.0, "Staff list, " + me->unit, staff, "the regiment's staff"});
        own_papers_.push_back({"comms_diagram", Cmdr(), 0.0, "Communications diagram", diagram, "the regiment's chief of communications"});
    }
    // The world's movers to now (roadmap G11): their levels by the own vehicle's eye at the visual range (the light by day or
    // night, the weather's visibility the cap).
    void StepNav(double dt) {
        if (!nav_) return;
        const Vec3 eye = battery_ && battery_->Own() != nullptr ? battery_->Own()->position : def_.unit_position;
        const double range = nav::VisualRangeM(def_.weather.visibility_km * 1000.0, light_.sun_elevation_deg, nav_->Range());
        nav_->Step(time_, dt, {{eye.x, eye.y}}, range);
    }

    // The net's picture of each vehicle commander: where his vehicle is, whether radio is his to use (on the march only a
    // vehicle out of the column calls; forming up, before the column moves off, they still answer on the net, as in
    // nav::ColumnMarch), whether his set is on the battery net and the net not jammed out (strong jamming; weak jamming
    // lets an order through).
    void KeepOrderNetCurrent() {
        if (const Battery::Vehicle* g = battery_->Find(def_.battery_giver)) order_net_->SetGiverAt(g->position);
        const bool jammed = nets_ && nets_->Jam("battery", time_) >= 2;
        for (const command::OrderVehicle& c : def_.battery_crews) {
            const Battery::Vehicle* bv = battery_->Find(c.vehicle);
            command::OrderVehicle* ov = order_net_->Vehicle(c.vehicle);
            if (bv == nullptr || ov == nullptr) continue;
            ov->at = bv->position;
            ov->may_call = NetMayCall(*bv);
            ov->on_net = !nets_ || nets_->MemberOnNet("battery", c.callsign);
            ov->jammed = jammed;
        }
    }
    bool NetMayCall(const Battery::Vehicle& v) const {
        const bool ban = battery_->CurrentMode() == Battery::Mode::Marching && !RepellingAttack();
        return !ban || v.absent || v.away || v.catching || v.broken;
    }
    // Each step: who has heard (his vehicle acts), the repeat-backs, the silences chased; each vehicle done reports, and
    // what reaches the battery commander the own crew hears where its means reach them: the giver's words again and the
    // vehicle commanders' on the battery net, the signals by flags or lamps. The own vehicle's are its seats'.
    void StepOrderNet() {
        KeepOrderNetCurrent();
        const std::string own = OwnOnOrderNet();
        for (const command::OrderEvent& e : order_net_->Step(time_)) {
            log_.Record({e.t, DebriefEntry::Kind::Scenario, e.callsign, "battery order",
                         e.what + (e.mean.empty() ? std::string() : " by " + e.mean) + ": " + e.text});
            const bool mine = !own.empty() && e.vehicle == own;
            const auto regime = regime_orders_.find(e.order);
            if (e.what == "carrying out" && !mine && regime == regime_orders_.end() && notice_orders_.count(e.order) == 0) battery_->Join(e.vehicle, time_);
            if (e.what == "heard" && mine && regime != regime_orders_.end()) CommanderHearsRegime(regime->second, "the battery net");
            if (e.what == "sent again" && e.mean == "radio_voice") {  // to him by his call sign: «23, я 10: К маршу!»
                Radio("battery net", "Battery commander", e.callsign + ", я " + BatteryPostCall() + ": " + e.text);
                if (mine && CmdrHearsBatteryNet()) order_net_->Reach(e.order, e.vehicle, time_ + order_net_->Def().say_s, time_);
                continue;
            }
            const bool to_giver = e.what == "repeated back" || e.what == "reported" || e.what == "cannot";
            if (to_giver) GiverHears(e);
            if ((e.what == "reported" || e.what == "cannot") && supply_orders_.count(e.order) != 0) CommanderHearsTzm(e);
            if (mine || !to_giver) continue;
            if (e.mean == "radio_voice") {
                SayForVehicle(e);
            } else if (e.what != "repeated back" && (e.mean == "flags" || e.mean == "lamps")) {
                Radio(e.mean == "lamps" ? "lamp signals" : "flag signals", e.callsign, e.text, false, SignalSeers());
            }
        }
        GiverJournalStep();
        if (battery_order_ < 0) return;
        GiverOrdersBattle();
        for (const command::OrderVehicle& c : order_net_->Vehicles()) {
            const Battery::Vehicle* bv = battery_->Find(c.vehicle);
            if (c.seats || bv == nullptr || bv->absent || bv->away || !order_net_->Heard(battery_order_, c.vehicle)) continue;
            if (bv->broken) {  // he cannot carry it out: he says so, and why
                order_net_->Cannot(battery_order_, c.vehicle, time_, bv->broken_why, NetWords("report_cannot", OrderFields(c.callsign), c.callsign + ": не могу"));
                continue;
            }
            if (battery_order_kind_ == BatteryOrder::Kind::March && time_ >= bv->hold_until_s) {
                order_net_->Done(battery_order_, c.vehicle, time_, NetWords("report_ready_to_march", OrderFields(c.callsign), c.callsign + ": К маршу готов!"));
            } else if (DeploysBattery(battery_order_kind_) && bv->deployed) {
                order_net_->Done(battery_order_, c.vehicle, time_, c.callsign + " к бою готов!");
            }
        }
    }
    // A vehicle commander's words on the battery net, in the crew data's words, spoken as the battery's (the callout bus
    // takes turns on the net): «10, я 22: к маршу, понял. Приём.», «10, я 22: к маршу готов. Приём.», a launcher's
    // «к бою готов» with its missiles (voenizdat_osa_1991 p246); a vehicle with no fire channels says the short form.
    void SayForVehicle(const command::OrderEvent& e) {
        if (supply_orders_.count(e.order) != 0 && e.what != "repeated back") {  // the TZM's chief back: his load, or why not
            Radio("battery net", e.callsign, e.text);
            return;
        }
        std::map<std::string, std::string> f = OrderFieldsFor(e.callsign, e.order);  // the order it answers (a notice too)
        std::string id = e.what == "repeated back" ? "order_readback" : e.what == "cannot" ? "report_cannot" : "report_ready_to_march";
        if (e.what == "reported" && e.order == battery_order_ && DeploysBattery(battery_order_kind_)) {
            id.clear();
            if (battery_fire_) {
                for (const BatteryFire::Shooter& s : battery_fire_->Shooters()) {
                    if (s.callsign != e.callsign) continue;
                    id = "report_ready";
                    f = {{"callsign", s.callsign}, {"missiles", std::to_string(s.missiles)}};
                }
            }
        }
        if (!id.empty() && callouts_ && def_.crew_data.FindCallout(id) != nullptr) {
            callouts_->Say(time_, id, "battery", f);
        } else {
            Radio("battery net", e.callsign, e.text);
        }
    }
    // The blanks of a battery order's words on the net: the battery commander's call sign, the vehicle's, the order in
    // force as said («к маршу») and in English.
    std::map<std::string, std::string> OrderFields(const std::string& callsign) const { return OrderFieldsFor(callsign, battery_order_); }
    // ...of a given order on the net (a moving order, the emission regime: G10.7).
    std::map<std::string, std::string> OrderFieldsFor(const std::string& callsign, int order) const {
        std::map<std::string, std::string> f{{"post", BatteryPostCall()}, {"callsign", callsign}};
        const auto it = order_words_.find(order);
        const std::pair<std::string, std::string> w = it != order_words_.end() ? it->second : KindWords(battery_order_kind_);
        f["order"] = w.first;
        f["order_en"] = w.second;
        return f;
    }
    // A moving order as said on the net, and in English: «к маршу», "prepare to march".
    std::pair<std::string, std::string> KindWords(BatteryOrder::Kind k) const {
        const std::string words = BatteryOrderWords(k);
        std::string en = BatteryOrderName(k);
        if (const char* id = OrderCallout(k)) {
            if (const CalloutDef* c = def_.crew_data.FindCallout(id)) en = c->en;
        }
        if (!en.empty() && en.back() == '!') en.pop_back();
        if (!en.empty() && en[0] >= 'A' && en[0] <= 'Z') en[0] = static_cast<char>(en[0] - 'A' + 'a');
        return {LowerFirst(words.substr(0, words.size() > 1 && words.back() == '!' ? words.size() - 1 : words.size())), en};
    }
    std::map<int, std::pair<std::string, std::string>> order_words_;  // each order on the net: its words and in English
    std::map<int, EmissionRegime> regime_orders_;                     // the orders on the net that are a regime
    std::string NetWords(const char* callout, const std::map<std::string, std::string>& fields, const std::string& fallback) const {
        const CalloutDef* c = def_.crew_data.FindCallout(callout);
        return c != nullptr ? FillCallout(c->ru, fields) : fallback;
    }
    static const char* OrderCallout(BatteryOrder::Kind k) {
        switch (k) {
            case BatteryOrder::Kind::March: return "order_march";
            case BatteryOrder::Kind::ShortHalt:
            case BatteryOrder::Kind::Halt: return "order_halt";
            case BatteryOrder::Kind::Resume: return "order_go";
            case BatteryOrder::Kind::CombatFormation: return "order_k_boyu";
            case BatteryOrder::Kind::Relocate: return "order_relocate";
            default: return nullptr;
        }
    }
    // The first letter in lower case (a Cyrillic capital in UTF-8, or a Latin one): «К маршу» -> «к маршу».
    static std::string LowerFirst(std::string s) {
        if (s.size() >= 2 && static_cast<unsigned char>(s[0]) == 0xD0) {
            const unsigned char c = static_cast<unsigned char>(s[1]);
            if (c >= 0x90 && c <= 0x9F) s[1] = static_cast<char>(c + 0x20);                       // А-П -> а-п
            if (c >= 0xA0 && c <= 0xAF) s.replace(0, 2, std::string{'\xD1', static_cast<char>(c - 0x20)});  // Р-Я -> р-я
        } else if (!s.empty() && s[0] >= 'A' && s[0] <= 'Z') {
            s[0] = static_cast<char>(s[0] - 'A' + 'a');
        }
        return s;
    }
    std::string BatteryPostCall() const { return OwnCallSignOn("battery"); }

    // ---- The giver's radiotelephonist (G10.6): what reaches the battery commander, a read-back, a report or a «не могу»,
    // he writes in the journal (pvo_bu_x_1984 art. 24), and the order's last step is the line once it is legible. The
    // giver is the battery's NPC PU-12 here; its journal is the post's own when the player's vehicle gives the orders.
    RadioJournal& GiverJournal() { return GiverIsOwn() ? journal_ : giver_journal_; }
    const RadioJournal& GetGiverJournal() const { return GiverIsOwn() ? journal_ : giver_journal_; }
    bool GiverIsOwn() const {
        const Battery::Vehicle* own = battery_ ? battery_->Own() : nullptr;
        return own != nullptr && own->def.id == def_.battery_giver;
    }
    void GiverHears(const command::OrderEvent& e) {
        GiverJournal().Heard(e.t, e.mean == "radio_voice" ? "battery net" : e.mean, e.callsign, e.text);
        giver_to_log_.push_back(e);
    }
    void GiverJournalStep() {
        if (!GiverIsOwn() && !giver_journal_.Busy(time_)) giver_journal_.WriteNext(time_, def_.battery_orders_def.write_s);
        if (giver_to_log_.empty()) return;
        const std::vector<RadioJournalEntry> page = GiverJournal().Legible(time_);
        for (std::size_t i = 0; i < giver_to_log_.size();) {
            const command::OrderEvent& o = giver_to_log_[i];
            bool found = false;
            for (const RadioJournalEntry& e : page) {
                if (e.from != o.callsign || std::fabs(e.heard_s - o.t) > 1e-6 || e.said != o.text) continue;
                order_net_->Logged(o.order, o.vehicle, e.written_s);
                found = true;
                break;
            }
            if (found) {
                giver_to_log_.erase(giver_to_log_.begin() + static_cast<std::ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
    }
    // The column at the end of its route: the NPC battery commander orders «К бою!» down it by flags or lamps (the march's
    // radio ban still holds), the battery deploying in combat formation along the route's last leg (as nav::ColumnMarch);
    // each vehicle stops where it is until its commander has read it.
    void GiverOrdersBattle() {
        if (battery_order_kind_ != BatteryOrder::Kind::March || arrival_for_ == battery_order_ || !battery_->AtRouteEnd()) return;
        const std::vector<Vec3>& route = battery_->Route();
        if (route.size() < 2) return;
        arrival_for_ = battery_order_;
        const Vec3 a = route[route.size() - 2];
        const Vec3 b = route.back();
        double axis = RadToDeg(std::atan2(b.x - a.x, b.y - a.y));
        if (axis < 0.0) axis += 360.0;
        const double ar = DegToRad(axis);
        const double behind = battery_->Tactics().behind_covered_m;
        BatteryOrder o;
        o.kind = BatteryOrder::Kind::CombatFormation;
        o.at_s = time_;
        o.covered = Vec3{b.x + std::sin(ar) * behind, b.y + std::cos(ar) * behind, 0.0};
        o.axis_deg = axis;
        if (task_formation_pending_) {  // the task's formation, as his plan has it (across the attack direction, 14.5)
            task_formation_pending_ = false;
            o = task_plan_.formation;
            o.at_s = time_;
        }
        if (PostGivesOrders() && !PostOrderId(o.kind).empty()) {  // the post's battery commander's to give (G10.8)
            post_planned_.push_back(o);
            return;
        }
        log_.Record({time_, DebriefEntry::Kind::Scenario, "Battery commander", "battery order",
                     "at the end of the route: " + std::string(o.kind == BatteryOrder::Kind::CombatFormation ? "К бою!" : BatteryOrderName(o.kind))});
        BatteryTakes(o);
    }

    // ---- The player's vehicle gives the battery's orders (G10.8): the PU-12M is the battery commander's, and the moves the
    // combat order plans (the march and its route, the «К бою!» at its end) are his to give when they are due, from the
    // crew menu's Battery orders sector, an NPC by NpcDoes; the helper shows them. Each goes out on the order net as any
    // battery order, the replies come back on it and the radiotelephonist writes them in the post's journal. A halt and a
    // move on he may give at any time; the post itself moves off only once its crew has stowed it.
    bool PostGivesOrders() const { return order_net_ && post_ && GiverIsOwn(); }
    static std::string PostOrderId(BatteryOrder::Kind k) {
        switch (k) {
            case BatteryOrder::Kind::March: return "order_battery_march";
            case BatteryOrder::Kind::ShortHalt:
            case BatteryOrder::Kind::Halt: return "order_battery_halt";
            case BatteryOrder::Kind::Resume: return "order_battery_resume";
            case BatteryOrder::Kind::CombatFormation: return "order_battery_k_boyu";
            case BatteryOrder::Kind::Relocate: return "order_battery_relocate";
            default: return std::string();
        }
    }
    static bool PostOrderKind(const std::string& id, BatteryOrder::Kind& k) {
        for (BatteryOrder::Kind c : {BatteryOrder::Kind::March, BatteryOrder::Kind::Halt, BatteryOrder::Kind::Resume,
                                     BatteryOrder::Kind::CombatFormation, BatteryOrder::Kind::Relocate}) {
            if (PostOrderId(c) == id) {
                k = c;
                return true;
            }
        }
        return false;
    }
    bool PostPlanned(const std::string& id) const {
        for (const BatteryOrder& o : post_planned_) {
            if (PostOrderId(o.kind) == id) return true;
        }
        return false;
    }
    // The first move due: owed by the battery commander's seat until it is given.
    void PostOwesOrders() {
        if (!PostGivesOrders() || post_planned_.empty()) return;
        const std::string id = PostOrderId(post_planned_.front().kind);
        SeatDuty d = SeatDuty::Order("battery_commander", id, "the combat order: " + BatteryOrderWords(post_planned_.front().kind) + " now");
        d.keep_s = 1.0;
        if (NpcDoes("battery_commander", d)) GiveCrewOrder(id);
    }
    // He gives it: the planned one of its kind (its route or its place), or a halt or a move on of his own.
    bool PostGivesBatteryOrder(const std::string& id) {
        BatteryOrder o;
        bool found = false;
        for (std::size_t i = 0; i < post_planned_.size() && !found; ++i) {
            if (PostOrderId(post_planned_[i].kind) != id) continue;
            o = post_planned_[i];
            post_planned_.erase(post_planned_.begin() + static_cast<std::ptrdiff_t>(i));
            found = true;
        }
        if (!found) {
            if (!PostOrderKind(id, o.kind) || (o.kind != BatteryOrder::Kind::Halt && o.kind != BatteryOrder::Kind::Resume)) return false;
        }
        o.at_s = time_;
        if (!MarchRadioBan()) Radio("battery net", BatteryPostCall(), "Всем, я " + BatteryPostCall() + ": " + BatteryOrderWords(o.kind));
        BatteryTakes(o);
        if (o.kind == BatteryOrder::Kind::March && battery_->Own() != nullptr && !OwnStowed()) {
            battery_->SetAbsent(battery_->Own()->def.id, true);
            battery_->SetLeader(battery_->Own()->def.id);  // the column waits for its commander's post
            post_stowing_ = true;
            post_stow_ordered_ = false;
        }
        return true;
    }
    // The post on a march: it stays until its crew has stowed it (the squad leader's «Свернуть!»), then joins the column.
    void PostStowsForMarch() {
        if (!PostGivesOrders() || !post_stowing_ || battery_->Own() == nullptr) return;
        const std::string own = battery_->Own()->def.id;
        if (OwnStowed() || !battery_->Absent(own)) {
            battery_->Join(own, time_);
            post_stowing_ = false;
        } else if (!post_stow_ordered_ && NpcDoes(Cmdr(), SeatDuty::Order(Cmdr(), "order_stow", "the battery marches: the post stows for it"))) {
            post_stow_ordered_ = GiveCrewOrder("order_stow");
        }
    }
    std::vector<BatteryOrder> post_planned_;  // the combat order's moves not yet given
    bool post_stowing_ = false;               // a march given: the post stows, then joins its column
    bool post_stow_ordered_ = false;
    // The emission regime as an order on the net (G10.7): to every vehicle, «Всем, я 10: …» twice; each commander
    // reads it back, and one silent is said it again by his call sign, then by the next means; the own crew's commander
    // has it only once he has heard it (CommanderHearsRegime from the net's "heard").
    void RegimeOnOrderNet(const EmissionRegime& r) {
        KeepOrderNetCurrent();
        std::vector<std::string> to;
        for (const command::OrderVehicle& c : order_net_->Vehicles()) to.push_back(c.vehicle);
        const std::string g = EmissionGroup(r);
        const int order = order_net_->Give("emission", g, "radio_voice", time_, to);
        order_words_[order] = {g, RegimeText(r)};
        regime_orders_[order] = r;
        GiverJournal().Heard(time_, "battery net", "Battery commander", g);
        Radio("battery net", BatteryPostCall(), "Всем, я " + BatteryPostCall() + ": " + g + ". " + g + ".");
        if (const TalkEntry* e = def_.talk_tables.ByEffect(EmissionEffect(r.kind))) HeardGroup(e->code, "battery", BatteryNetHearers());
        const std::string own = OwnOnOrderNet();
        if (!own.empty()) {
            if (CmdrHearsBatteryNet()) order_net_->Reach(order, own, time_ + order_net_->Def().say_s, time_);
        } else if (c2_ == nullptr || c2_->Hearing()) {
            CommanderHearsRegime(r, "the battery net");
        }
    }

    // ---- The regiment's orders down the same path (G10.7): the regiment's duty officer says the order on its command net
    // to the battery commander, who reads it back, and after his reaction and the time to pass it on (the emission
    // data's relay_s) gives it to the battery on the order net; once given he reports it done. Its book is the regiment's.
    void RegimentGives(const BatteryOrder& o) {
        const std::string words = o.kind == BatteryOrder::Kind::Emission ? EmissionGroup(o.regime) : BatteryOrderWords(o.kind);
        const int order = regiment_net_->Give(BatteryOrderName(o.kind), words, "radio_voice", time_, {def_.battery_giver});
        RegimentOrder p;
        p.order = o;
        p.order.from.clear();
        if (p.order.kind == BatteryOrder::Kind::Emission) p.order.regime.from = "regiment";
        regiment_orders_[order] = p;
        log_.Record({time_, DebriefEntry::Kind::Scenario, "Regiment", "regiment net", "to the battery commander: " + words});
    }
    void RegimentStep() {
        if (!regiment_net_) return;
        for (const command::OrderEvent& e : regiment_net_->Step(time_)) {
            log_.Record({e.t, DebriefEntry::Kind::Scenario, e.callsign, "regiment order", e.what + (e.mean.empty() ? std::string() : " by " + e.mean) + ": " + e.text});
            auto it = regiment_orders_.find(e.order);
            if (it != regiment_orders_.end() && e.what == "carrying out") it->second.acting_s = time_;
            if (e.order == task_order_ && e.what == "carrying out") TaskReceived("by radio on the regiment's net");
        }
        for (auto& kv : regiment_orders_) {
            RegimentOrder& p = kv.second;
            if (p.acting_s < 0.0 || p.passed || time_ + 1e-9 < p.acting_s + def_.emission.relay_s) continue;
            p.passed = true;
            if (p.order.kind == BatteryOrder::Kind::Emission) {
                BatteryOrderRegime(p.order.regime);
            } else {
                p.order.at_s = time_;
                BatteryTakes(p.order);
            }
            // A move is done when the battery stands ready at its new place and its commander says so on the regiment's
            // net (ReportToRegiment); an NPC post's report up is not simulated, so there it is done once passed on.
            if (!PostGivesOrders() || !MovesBattery(p.order.kind)) regiment_net_->Done(kv.first, def_.battery_giver, time_, "Выполнено");
        }
    }
    // ---- The march's points in the record (G10.6, decision G-6): as the head passes each one (the start point, a halt,
    // a regulation point, a regulator, the end) the battery commander says it in his cabin and his radiotelephonist
    // writes it in the journal, as nav::ColumnMarch does. Radio is forbidden in the column (voenizdat_osa_1991 p240), so
    // nothing goes up on the march: the regiment hears of it when the battery reports ready at its new place.
    int march_points_for_ = 0;
    std::size_t march_points_logged_ = 0;
    void LogMarchPoints() {
        if (battery_->Marches() != march_points_for_) {
            march_points_for_ = battery_->Marches();
            march_points_logged_ = 0;
        }
        const std::vector<Battery::Passing>& passed = battery_->Passings();
        while (march_points_logged_ < passed.size() && passed[march_points_logged_].head_s >= 0.0) {
            const Battery::Passing& ps = passed[march_points_logged_];
            GiverJournal().Heard(ps.head_s, "march", "Battery commander", Battery::PassingText(ps));
            ++march_points_logged_;
        }
    }
    struct RegimentOrder {
        BatteryOrder order;    // as the battery commander passes it on
        double acting_s = -1.0;
        bool passed = false;
    };
    std::unique_ptr<command::BatteryOrderNet> regiment_net_;  // the regiment's orders to the battery commander (G10.7)
    std::map<int, RegimentOrder> regiment_orders_;

    // ---- The regiment's task (roadmap 14.1, 14.3, 14.5; campaign/battery_task.hpp, battery_plan.hpp): the combat order
    // reaches the battery commander by radio in its short form (the regiment's duty officer on its command net; he reads
    // it back) or in person with the paper. He plans it from that moment by the textbook's sequence, short of time each
    // step shortened alike; at the preliminary step's end he gives the preliminary order on the battery net, at the
    // combat order's the oral combat order with the emission regime and each launcher's sector, then «К маршу!» to the
    // main position, and at the march's end the formation the task calls for. His «к бою готов» to the regiment there is
    // the task's readiness, against the order's ready-by. In the post an NPC battery commander gives each of these by
    // NpcDoes; a player owes them from the crew menu, the helper showing them.
    // The time of day at a session second by the scenario's clock ("19:05"); the session's own clock without one.
    bool DarkAt(double s) const { return LightAt(def_.clock, s, def_.light_values).dark; }  // the sun then (R7)
    std::string DayClock(double s) const {
        if (def_.clock_start_s < 0.0) return SessionClock(s);
        const long long m = (std::llround(def_.clock_start_s + std::max(0.0, s)) / 60) % (24 * 60);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02lld:%02lld", m / 60, m % 60);
        return buf;
    }
    std::string PlaceName(const std::string& id) const {
        if (nav_) {
            if (const nav::Anchor* a = nav_->GetAnchors().Find(id)) return a->name;
        }
        return id;
    }
    std::string TaskArea() const { return def_.task.area_ru.empty() ? PlaceName(def_.task.main) : def_.task.area_ru; }
    void UpdateTask() {
        if (!def_.has_task) return;
        if (!task_given_ && time_ + 1e-9 >= def_.task_given_s) {
            task_given_ = true;
            log_.Record({time_, DebriefEntry::Kind::Scenario, "Regiment", "the task", def_.task.task_text});
            if (def_.task_means == "radio" && regiment_net_) {
                const std::string words = campaign::TaskVoiceSummary(def_.task, def_.task_given_s, [this](double t) { return DayClock(t); },
                                                                     [this](const std::string& id) { return PlaceName(id); });
                task_order_ = regiment_net_->Give("task", words, "radio_voice", time_, {def_.battery_giver});
                log_.Record({time_, DebriefEntry::Kind::Scenario, "Regiment", "regiment net", "to the battery commander: " + words});
            } else {
                TaskReceived("in person, with the paper");
            }
        }
        if (task_received_s_ < 0.0 || !task_plan_.planned) return;
        for (const campaign::PlannedStep& st : task_plan_.steps) {
            if (time_ + 1e-9 < st.end_s || task_steps_done_.count(st.id) != 0) continue;
            if (st.order.empty()) {  // a step of his own planning: done when its time is up
                task_steps_done_.insert(st.id);
                log_.Record({time_, DebriefEntry::Kind::Scenario, "Battery commander", "the task", st.en});
                continue;
            }
            if (!PostGivesOrders()) {  // an NPC battery commander in a post we do not ride in
                GiveTaskOrder(st.order, "Battery commander");
                continue;
            }
            SeatDuty d = SeatDuty::Order("battery_commander", st.order, "the plan for the regiment's task: he " + st.en);
            d.keep_s = 1.0;
            if (NpcDoes("battery_commander", d)) GiveCrewOrder(st.order);
        }
    }
    // He has the task: he plans it from now, with the march's time on the roads from where his vehicle stands.
    void TaskReceived(const std::string& how) {
        if (task_received_s_ >= 0.0 || !battery_) return;
        task_received_s_ = time_;
        campaign::PlanInputs in;
        in.given_s = def_.task_given_s;
        in.received_s = time_;
        in.march_s = TaskMarchS();
        in.where = [this](const std::string& id, Vec3& out) {
            const nav::Anchor* a = nav_ ? nav_->GetAnchors().Find(id) : nullptr;
            if (a == nullptr) return false;
            out = Vec3{a->at.east_m, a->at.north_m, 0.0};
            return true;
        };
        task_plan_ = campaign::PlanBatteryTask(def_.task, def_.plan_sequence, battery_->Tactics(), in);
        log_.Record({time_, DebriefEntry::Kind::Scenario, "Battery commander", "the task",
                     "received " + how + ": " + def_.task.task_text + ", ready by " + DayClock(task_plan_.ready_by_s)});
        if (!task_plan_.planned) {
            Event("battery", "the task is not planned: " + task_plan_.why);
            return;
        }
        std::string plan = task_plan_.march_at_s >= 0.0 ? "«К маршу!» at " + DayClock(task_plan_.march_at_s) + ", at the position at " + DayClock(task_plan_.arrive_s)
                                                       : "in the area: the formation at " + DayClock(task_plan_.order_s);
        plan += ", ready at " + DayClock(task_plan_.ready_s) + (task_plan_.InTime() ? " (in time)" : " (late)");
        log_.Record({time_, DebriefEntry::Kind::Scenario, "Battery commander", "his plan", plan});
        if (!PostGivesOrders()) return;
        const auto clock = [this](double t) { return DayClock(t); };
        HeldPaper order;
        order.id = "combat_order:" + std::to_string(own_papers_.size());
        order.station = Cmdr();
        order.time_s = time_;
        order.title = "Combat order, " + def_.task.unit;
        order.text = campaign::TaskDocument(def_.task, def_.task_given_s, clock, [this](const std::string& id) { return PlaceName(id); });
        order.from = def_.task_means == "radio" ? "taken down from the regiment's net" : "the regiment commander";
        own_papers_.push_back(order);
        HeldPaper mine;
        mine.id = "task_plan:" + std::to_string(own_papers_.size());
        mine.station = Cmdr();
        mine.time_s = time_;
        mine.title = "Plan for the task";
        mine.text = campaign::PlanText(task_plan_, clock, [this](const std::string& id) { return PlaceName(id); });
        mine.from = "your own planning";
        own_papers_.push_back(mine);
    }
    // The head's time by the roads to the main position (< 0: the battery stands in the area, or no road there).
    double TaskMarchS() const {
        const Battery::Vehicle* head = battery_->Own() != nullptr ? battery_->Own() : battery_->Find(def_.battery_giver);
        const nav::Anchor* a = nav_ ? nav_->GetAnchors().Find(def_.task.main) : nullptr;
        if (head == nullptr || a == nullptr) return -1.0;
        if (std::hypot(head->position.x - a->at.east_m, head->position.y - a->at.north_m) < kTaskInAreaM) return -1.0;
        std::vector<nav::MarchVehicle> column;
        for (const Battery::Vehicle& v : battery_->Vehicles()) {
            nav::MarchVehicle m;
            m.def = v.def;
            m.def.start = v.position;
            column.push_back(m);
        }
        std::string why;
        const nav::MarchPlan p = nav_->PlanTo({head->position.x, head->position.y}, def_.task.main, column, battery_->Tactics(), time_, &why,
                                              DarkAt(time_));
        return p.found ? std::max(0.0, p.end_s - time_) : -1.0;
    }
    static constexpr double kTaskInAreaM = 1000.0;  // sam3d: the battery stands in its area within this of the main position
    // The task's words on the battery net: the preliminary order, or the oral combat order (the regime and each
    // launcher's sector go with it, then the move). False, with why, when there is nothing of the kind to give.
    bool GiveTaskOrder(const std::string& id, const std::string& who) {
        const campaign::PlannedStep* st = nullptr;
        for (const campaign::PlannedStep& c : task_plan_.steps) {
            if (c.order == id) st = &c;
        }
        if (task_received_s_ < 0.0 || st == nullptr) {
            last_refusal_ = "no task from the regiment to give it for";
            return false;
        }
        if (task_steps_done_.count(st->id) != 0) {
            last_refusal_ = "given already";
            return false;
        }
        if (!order_net_) return false;
        const bool combat = st->id == "combat_order";
        const std::map<std::string, std::string> f{{"time", DayClock(task_plan_.march_at_s >= 0.0 ? task_plan_.march_at_s : task_plan_.order_s)},
                                                   {"task", def_.task.short_ru}, {"task_en", def_.task.task_text},
                                                   {"area", TaskArea()}, {"area_en", PlaceName(def_.task.main)},
                                                   {"ready", DayClock(task_plan_.ready_by_s)}};
        const CalloutDef* c = def_.crew_data.FindCallout(id.c_str());
        const std::string words = c != nullptr ? FillCallout(c->ru, f) : (combat ? "Боевой приказ: " + def_.task.short_ru + "." : std::string("Предварительное распоряжение."));
        NoticeOnOrderNet(st->id, words, combat ? std::make_pair(std::string("боевой приказ"), std::string("the combat order"))
                                               : std::make_pair(std::string("предварительное распоряжение"), std::string("the preliminary order")));
        task_steps_done_.insert(st->id);
        log_.Record({time_, DebriefEntry::Kind::Scenario, who, "battery net", words});
        if (combat) TaskCombatOrder();
        return true;
    }
    void TaskCombatOrder() {
        EmissionKind k = EmissionKind::Free;
        if (!def_.task.emission.empty() && ParseEmissionKind(def_.task.emission, k)) {
            EmissionRegime r = def_.emission.Make(k, "regiment");
            if (def_.task.on_turns > 0.0) r.on_turns = def_.task.on_turns;
            if (def_.task.off_turns > 0.0) r.off_turns = def_.task.off_turns;
            BatteryOrderRegime(r);
        }
        for (const campaign::TaskSector& sec : def_.task.sectors) task_sectors_set_ += SetBatterySector(sec.vehicle, sec.from_deg, sec.to_deg) ? 1 : 0;
        BatteryOrder move;
        if (task_plan_.march_at_s >= 0.0) {
            move.kind = BatteryOrder::Kind::March;
            move.to_place = task_plan_.to_place;
            task_formation_pending_ = task_plan_.has_formation;
        } else if (task_plan_.has_formation) {
            move = task_plan_.formation;
            regiment_reported_ = false;  // he reports ready again once in the formation
        } else {
            return;
        }
        move.at_s = time_;
        move.from = "battery";
        task_orders_.push_back(move);
        task_orders_done_.push_back(0);
        task_moved_ = true;
    }
    // An order of words only on the battery net (the task's preliminary order and the oral combat order): every vehicle
    // commander hears it and reads it back (only the launchers' for a fire order); nobody moves on it.
    int NoticeOnOrderNet(const std::string& kind, const std::string& words, const std::pair<std::string, std::string>& short_words,
                         bool launchers_only = false) {
        KeepOrderNetCurrent();
        std::vector<std::string> to;
        for (const command::OrderVehicle& v : order_net_->Vehicles()) {
            const Battery::Vehicle* bv = battery_ ? battery_->Find(v.vehicle) : nullptr;
            if (launchers_only && (bv == nullptr || bv->def.kind != BatteryVehicleKind::Launcher)) continue;
            to.push_back(v.vehicle);
        }
        const int order = order_net_->Give(kind, words, "radio_voice", time_, to);
        order_words_[order] = short_words;
        notice_orders_.insert(order);
        GiverJournal().Heard(time_, "battery net", "Battery commander", words);
        Radio("battery net", BatteryPostCall(), "Всем, я " + BatteryPostCall() + ": " + words);
        const std::string own = OwnOnOrderNet();
        if (!own.empty() && CmdrHearsBatteryNet()) order_net_->Reach(order, own, time_ + order_net_->Def().say_s, time_);
        return order;
    }
    // His report up after the task's move is the task's readiness.
    void TaskReadyReported(const std::string& text) {
        if (!task_moved_ || task_ready_s_ >= 0.0) return;
        task_ready_s_ = time_;
        if (task_order_ >= 0 && regiment_net_ && !regiment_net_->IsDone(task_order_, def_.battery_giver)) {
            regiment_net_->Done(task_order_, def_.battery_giver, time_, text);
        }
        const double late = time_ - task_plan_.ready_by_s;
        char buf[160];
        std::snprintf(buf, sizeof(buf), "ready at %s, by the order %s: %s %.0f min%s", DayClock(time_).c_str(), DayClock(task_plan_.ready_by_s).c_str(),
                      late <= 0.0 ? "in time," : "late by", std::fabs(late) / 60.0, late <= 0.0 ? " to spare" : "");
        log_.Record({time_, DebriefEntry::Kind::Scenario, "Battery commander", "the task", buf});
    }
    // The debrief's account of the task: when it came, his plan, and when the battery was ready against its ready-by.
    std::string TaskBlock() const {
        std::string out = "  The task (" + def_.task.id + "): " + def_.task.task_text + "\n";
        if (task_received_s_ >= 0.0) {
            out += "    given " + DayClock(def_.task_given_s) + " (" + def_.task_means + "), received " + DayClock(task_received_s_) +
                   ", to be ready by " + DayClock(task_plan_.ready_by_s) + "\n";
        } else {
            out += "    never reached the battery commander\n";
        }
        if (task_plan_.planned && task_plan_.march_at_s >= 0.0) {
            out += "    his plan: «К маршу!» " + DayClock(task_plan_.march_at_s) + ", at the position " + DayClock(task_plan_.arrive_s) +
                   ", ready " + DayClock(task_plan_.ready_s) + "\n";
        }
        if (task_ready_s_ >= 0.0) {
            char line[160];
            const double late = task_ready_s_ - task_plan_.ready_by_s;
            std::snprintf(line, sizeof(line), "    ready (reported up) %s: %s %.0f min\n", DayClock(task_ready_s_).c_str(),
                          late <= 0.0 ? "in time, to spare" : "late by", std::fabs(late) / 60.0);
            out += line;
        } else if (task_received_s_ >= 0.0) {
            out += "    not reported ready\n";
        }
        return out;
    }
    std::vector<std::string> tasks_before_;  // the debrief's accounts of the tasks given before this one
    int task_order_ = -1;               // the task in the regiment's order book (by radio)
    bool task_given_ = false;
    double task_received_s_ = -1.0;     // the battery commander has it
    campaign::BatteryPlan task_plan_;
    std::set<std::string> task_steps_done_;
    std::vector<BatteryOrder> task_orders_;  // his plan's moves, given when due
    std::vector<char> task_orders_done_;
    bool task_formation_pending_ = false;    // the task's formation at the march's end
    bool task_moved_ = false;                // the combat order given: the move under way
    double task_ready_s_ = -1.0;             // his «к бою готов» up after the move
    int task_sectors_set_ = 0;
    std::set<int> notice_orders_;            // orders on the battery net that move nobody
    RadioJournal giver_journal_;                     // the NPC PU-12's radiotelephonist's
    std::vector<command::OrderEvent> giver_to_log_;  // heard by the giver, not yet legible on the page
    int arrival_for_ = -1;                           // the march order whose arrival «К бою!» was given

    // ---- The own vehicle on the order net (G10.5, level C2; docs/campaign/11_BATTERY_LIFE_PROPOSAL.md G10): its
    // commander, at his seat, hears the battery's orders where his headset is on the battery net (or sees the flags);
    // he reads each back on the net, an NPC by NpcDoes and a player from the crew menu, the helper showing it. On a march
    // the vehicle moves off only once the crew is stowed (he orders «К маршу!» on the intercom if it is not), the driver
    // says so and he reports «к маршу готов»; a deploying order moves it at once and his «к бою готов» is the report.
    // The own vehicle's id on the order net ("" : it is not on it).
    std::string OwnOnOrderNet() const {
        if (!order_net_) return std::string();
        const Battery::Vehicle* own = battery_->Own();
        if (own == nullptr) return std::string();
        for (const command::OrderVehicle& c : order_net_->Vehicles()) {
            if (c.seats && c.vehicle == own->def.id) return c.vehicle;
        }
        return std::string();
    }
    bool CmdrHearsBatteryNet() const {
        if (!voice_) return c2_ == nullptr || c2_->Hearing();
        const std::vector<std::string> h = BatteryNetHearers();
        return std::find(h.begin(), h.end(), Cmdr()) != h.end();
    }
    bool OwnStowed() const {
        const CrewTask* stowed = def_.crew_data.FindTask("stowed");
        return stowed == nullptr || console_.equipment.AllHold(stowed->conditions, console_.panel);
    }
    // Waiting for the crew to stow for a march the battery ordered (the commander's think does nothing else meanwhile).
    bool OwnStowing() const {
        const std::string own = OwnOnOrderNet();
        return !own.empty() && battery_order_ >= 0 && battery_order_kind_ == BatteryOrder::Kind::March && battery_->Absent(own) &&
               order_net_->Heard(battery_order_, own);
    }
    // The first order on the net he has heard and not read back yet (-1: none): a moving order or the regime.
    int OwnReadbackOrder() const {
        const std::string own = OwnOnOrderNet();
        if (own.empty()) return -1;
        const int n = static_cast<int>(order_net_->Book().Orders().size());
        for (int i = 0; i < n; ++i) {
            if (order_net_->Heard(i, own) && !order_net_->RepeatedBack(i, own)) return i;
        }
        return -1;
    }
    // The order a line of his is about.
    int OwnOwedOrder(const std::string& line) const { return line == "order_readback" ? OwnReadbackOrder() : battery_order_; }
    // What his seat owes on the net now: "order_readback", "report_ready_to_march", or "".
    std::string OwnOwesOnNet() const {
        const std::string own = OwnOnOrderNet();
        if (own.empty()) return std::string();
        const Battery::Vehicle* bv = battery_->Find(own);
        if (bv == nullptr || !NetMayCall(*bv)) return std::string();
        if (OwnReadbackOrder() >= 0) return "order_readback";
        if (battery_order_ >= 0 && battery_order_kind_ == BatteryOrder::Kind::March && order_net_->IsDone(battery_order_, own) &&
            !order_net_->Reported(battery_order_, own)) {
            return "report_ready_to_march";
        }
        return std::string();
    }
    void UpdateOwnOrder() {
        const std::string own = OwnOnOrderNet();
        if (own.empty()) return;
        const Battery::Vehicle* bv = battery_->Find(own);
        if (bv == nullptr) return;
        // What he owes on the net: said once for its order (a line that could not go out is tried again).
        const std::string owes = OwnOwesOnNet();
        const int about = OwnOwedOrder(owes);
        if (!owes.empty() && OwnSaidFor(owes) != about) {
            SeatDuty d = SeatDuty::Report(Cmdr(), owes, owes == "order_readback" ? "a battery order heard on the net: read it back"
                                                                                  : "the vehicle stowed and on its way: report it on the net");
            d.fields = OrderFieldsFor(OwnCallsign(), about);
            d.keep_s = 1.0;  // owed again each step while it is due: gone once radio is not his to use
            if (NpcDoes(Cmdr(), d) && SayOnRadio(owes, d.fields, false)) own_said_[owes] = about;
        }
        if (battery_order_ < 0 || !order_net_->Acting(battery_order_, own)) return;  // he acts on it after his reaction
        const bool march = battery_order_kind_ == BatteryOrder::Kind::March;
        if (bv->absent) {
            if (!march || OwnStowed()) {
                battery_->Join(own, time_);
            } else if (own_stow_ordered_ != battery_order_ &&
                       NpcDoes(Cmdr(), SeatDuty::Order(Cmdr(), "order_march", "the battery marches: the crew stows for it"))) {
                if (GiveCrewOrder("order_march")) own_stow_ordered_ = battery_order_;
            }
            return;
        }
        if (march && !order_net_->IsDone(battery_order_, own) && OwnStowed() && time_ >= bv->hold_until_s) {
            if (OwnSaidFor("ready_to_march") != battery_order_ &&
                NpcDoes("driver", SeatDuty::Report("driver", "ready_to_march", "the crew is stowed for the battery's march"))) {
                Say("ready_to_march", "driver");
                own_said_["ready_to_march"] = battery_order_;
            }
            order_net_->Done(battery_order_, own, time_, NetWords("report_ready_to_march", OrderFields(OwnCallsign()), "К маршу готов!"));
        }
        if (DeploysBattery(battery_order_kind_) && bv->deployed) {
            order_net_->Done(battery_order_, own, time_, NetWords("report_ready", RadioFields(), OwnCallsign() + " к бою готов!"));
        }
    }
    // His words go out on the battery net, said by `at_s`: the order net has his repeat-back or report (a deploying
    // order's is his «к бою готов»).
    void OwnSaidOnNet(const std::string& callout_id, double at_s) {
        const std::string own = OwnOnOrderNet();
        if (own.empty()) return;
        if (callout_id == "order_readback" && OwnReadbackOrder() >= 0) order_net_->SaidOnNet(OwnReadbackOrder(), own, at_s, true);
        if (battery_order_ < 0) return;
        if (callout_id == "report_ready_to_march") order_net_->SaidOnNet(battery_order_, own, at_s, false);
        if (callout_id == "report_ready" && DeploysBattery(battery_order_kind_) && order_net_->Heard(battery_order_, own)) {
            order_net_->Done(battery_order_, own, time_, NetWords("report_ready", RadioFields(), OwnCallsign() + " к бою готов!"));
            order_net_->SaidOnNet(battery_order_, own, at_s, false);
        }
    }
    // A launcher's «к бою готов» the order net carries (it has heard a deploying order): not said again by the battery's
    // fire model.
    bool OrderNetReports(const std::string& callsign) const {
        if (!order_net_ || battery_order_ < 0 || !DeploysBattery(battery_order_kind_)) return false;
        for (const command::OrderVehicle& c : order_net_->Vehicles()) {
            if (c.callsign == callsign && !c.seats) return order_net_->Heard(battery_order_, c.vehicle);
        }
        return false;
    }
    std::map<std::string, int> own_said_;  // each line of his on the net: the order it was last said for
    int OwnSaidFor(const std::string& line) const {
        const auto it = own_said_.find(line);
        return it != own_said_.end() ? it->second : -1;
    }
    int own_stow_ordered_ = -1;

    // The reload and refuel drills: an empty launcher calls the TZM; a crew asking for fuel calls the fuel truck (or the
    // TZM). What goes across is counted off the service vehicle; it goes back when the vehicle is full or it is empty.
    void ServiceDrills() {
        if (launcher_) {
            const int loaded = launcher_->LoadedCount();
            if (last_loaded_ >= 0 && loaded > last_loaded_) battery_->Consume("", Battery::Service::Reload, loaded - last_loaded_, time_);
            last_loaded_ = loaded;
            const double aboard = battery_->StockAlongside("", Battery::Service::Reload);  // the crane lifts no more
            launcher_->SetReloadStock(aboard < 0.0 ? -1 : static_cast<int>(std::floor(aboard + 1e-6)));
            if (loaded == 0 && launcher_->ShotsFlying().empty() && (CommanderAsks() ? tzm_asked_ : RequestOnAir("the TZM"))) {
                battery_->RequestService("", Battery::Service::Reload, time_);
            }
            if (loaded == launcher_->Capacity()) battery_->Done("", Battery::Service::Reload, time_);
            if (loaded > 0 && !battery_->Alongside("", Battery::Service::Reload)) tzm_asked_ = false;  // served and gone
        }
        if (fuel_) {
            const double level = fuel_->LevelL();
            if (last_fuel_l_ >= 0.0 && level > last_fuel_l_) battery_->Consume("", Battery::Service::Refuel, level - last_fuel_l_, time_);
            last_fuel_l_ = level;
            if (fuel_->Requested() && (CommanderAsks() ? fuel_asked_ : RequestOnAir("the fuel truck"))) {
                battery_->RequestService("", Battery::Service::Refuel, time_);
            }
            if (level >= fuel_->TankL() - 1e-6) {
                battery_->Done("", Battery::Service::Refuel, time_);
                fuel_asked_ = false;
            }
        }
    }
    // The commander asks for the TZM and the fuel truck himself (an NPC who thinks, or a player in his seat); with nobody
    // there to ask, they come when needed.
    bool CommanderAsks() const {
        return def_.has_crew_data && callouts_ && def_.crew_data.FindCallout("request_tzm") != nullptr &&
               (NpcCommanderActive() || !SeatHolder(Cmdr()).empty());
    }

    // Orders that were shouted reach their crew member after the delay.
    // The chain of command's messages that come due: each said on its means' channel to the seats it reaches, its paper
    // into the map case of the seat it was for (the commander's when it names none), an air-raid warning taken by the
    // battery (roadmap P1).
    void PlayDeliveries() {
        for (const DeliveryDef* d : deliveries_.Due(time_)) {
            std::string label;
            std::vector<std::string> heard;
            switch (d->means) {
                case DeliveryMeans::Radio:
                    label = d->net;
                    heard = HeardBy(label);
                    break;
                case DeliveryMeans::Telephone:
                    label = "telephone";
                    if (!d->to.empty()) heard.push_back(d->to);
                    else heard = HeardBy("cabin");
                    break;
                case DeliveryMeans::Intercom:
                    label = "intercom";
                    heard = HeardBy(label);
                    break;
                case DeliveryMeans::WarningNet: label = "warning net"; heard = HeardBy("cabin"); break;  // the R-311's speaker
                case DeliveryMeans::Siren: label = "sirens"; heard = HeardBy("cabin"); break;
                case DeliveryMeans::Broadcast: label = "state radio"; heard = HeardBy("cabin"); break;
                default: label = "at the hatch"; heard = HeardBy("cabin"); break;  // a carrier has come to the vehicle
            }
            Radio(label, d->from, d->words, false, heard);
            deliveries_.Hand(*d, d->to.empty() ? Cmdr() : d->to, time_);
            // A raid's bearing in it: each who heard it has the direction in his picture, as told (roadmap P4); with no
            // voice paths modelled, everyone in the vehicle.
            if (d->has_azimuth) {
                KnownTarget k;
                k.target = "warning:" + d->id;
                k.has_azimuth = true;
                k.true_azimuth = true;
                k.azimuth_deg = d->azimuth_deg;
                k.time_s = time_;
                k.source = "told by " + d->from;
                if (heard.empty()) {
                    for (const ConsoleStation& st : console_.stations) heard.push_back(st.id);
                }
                for (const std::string& st : heard) pictures_[st].Learn(k);
            }
            log_.Record({time_, DebriefEntry::Kind::Scenario, d->from, label,
                         std::string("message (") + DeliveryMeansName(d->means) + "): " + d->words +
                             (d->paper_title.empty() ? std::string() : " [paper: " + d->paper_title + "]")});
            if (d->air_warning && battery_ && !air_threat_forced_) {
                battery_->SetAirThreat(true, time_);
                air_threat_forced_ = true;
            }
        }
    }

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
                    if (!plan.messages[i].destroyed.empty()) HeardDestroyed(plan.messages[i].destroyed);
                    if (!plan.messages[i].road_crossing.empty()) HeardRoad(plan.messages[i]);
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
            // The telecode reaches the ASPD-U by radio (the RK-1U's ПРИЕМ on Р-407) or by a telecode line from the regiment
            // (ПРИЕМ on ПРОВОД; flak11 PU12-0: "per Funk oder Draht"): the line past any jamming and the set's reach, while
            // the regiment sends at all.
            const int rk1_rx = console_.panel.Find("rk1.rx") == nullptr ? 3 : console_.panel.Get("rk1.rx");
            const bool rk1_code = console_.panel.Find("rk1.code") == nullptr || console_.panel.Get("rk1.code") == 1;
            const bool by_radio = link && R407::ToEquipment(in) && rk1_rx == 3 && rk1_code;
            const bool by_wire = def_.post_regiment.telecode_wire && rk1_rx == 4 && rk1_code && def_.post_regiment.telecode && !off_air;
            SetRadioInput(b.Input("telecode"), by_radio || by_wire);
            // The post's own telecode out (a transmitting post, flak11 PU12-0's figure: the RK-1's send side): ПЕРЕДАЧА on
            // Р-407 with the set reaching the regiment's receiver in duplex, or on ПРОВОД down the telecode line.
            if (!console_.post_telecode_out.empty()) {
                const int rk1_tx = console_.panel.Find("rk1.tx") == nullptr ? 3 : console_.panel.Get("rk1.tx");
                const bool out_radio = rk1_tx == 3 && rk1_code && R407::ToEquipment(in) &&
                                       r407_->SendLink(in, plan.telecode_return_hz, distance, off_air || !def_.post_regiment.telecode);
                const bool out_wire = rk1_tx == 4 && rk1_code && def_.post_regiment.telecode_wire;
                SetRadioInput(console_.post_telecode_out, out_radio || out_wire);
            }
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
        std::vector<std::string> queued;             // the far end's calls waiting for the line (the radar crew's)
        bool checked = false;                        // the man who laid it has rung the post on it
    };
    std::map<int, LineState> lines_;
    std::map<int, WireRun> wires_;                   // the cables paid out by the crew, by line
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
        // To a seat with no box, or whose headset is off: he hears only the cabin, so it is said aloud.
        if (!def.to.empty() && (voice_->Unboxed(def.to) || (voice_->BoxOf(def.to) != nullptr && !voice_->HeadsetOn(def.to)))) {
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
    // Whether a seat listens to a channel (CalloutBus::SetListens: an NPC doesn't start a line while another channel talks
    // in his ear): the intercom by his box, the battery net by his box's set, the cabin while he is inside.
    bool Listens(const std::string& station, const std::string& channel) const {
        if (!voice_ || console_.FindStation(station) == nullptr) return false;
        if (channel == "cabin") return !Outside(station);
        if (channel == "intercom") return !Outside(station) && voice_->HearsIntercom(station);
        if (channel == "battery net") {
            for (const std::string& h : BatteryNetHearers()) {
                if (h == station) return true;
            }
        }
        return false;
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
            const bool told = SearchOwnHeard(s.callsign);
            if ((off || told) != s.on_own) {
                Event(s.callsign, off ? "off the battery net: searches and fires on its own"
                                      : (told ? "searches and fires on its own in its sector, by the battery commander's order"
                                              : "back on the battery net, firing on the post's designations"));
            }
            battery_fire_->SetOnOwn(s.callsign, off || told);
        }
        UpdatePictureLost();
    }
    // The battery commander's order that the launchers search on their own, each in its sector, and engage what they find
    // (pvo_bu_ix_1984: search on the senior commander's designation or independently, sudden targets engaged
    // independently and reported afterwards; vr_1985_plss: a gap in the picture means autonomous combat), and its
    // taking back. Words on the battery net: a launcher whose commander heard it acts on it.
    bool OrderSearchOwn(bool on, const std::string& by) {
        if (!post_ || !order_net_ || !battery_fire_) {
            last_refusal_ = "no launchers on the battery net to order";
            return false;
        }
        if (on == (search_own_order_ >= 0)) {
            last_refusal_ = on ? "they search on their own already" : "they fire on the post's designations already";
            return false;
        }
        const char* id = on ? "order_battery_search_own" : "order_battery_by_designation";
        const CalloutDef* c = def_.crew_data.FindCallout(id);
        const std::string words = c != nullptr ? c->ru : (on ? "Батарее — поиск целей самостоятельно, в своих секторах!" : "Батарее — работать по целеуказанию!");
        const int order = NoticeOnOrderNet(on ? "search_own" : "by_designation", words,
                                           on ? std::make_pair(std::string("поиск самостоятельно"), std::string("search on your own"))
                                              : std::make_pair(std::string("по целеуказанию"), std::string("on the designations")),
                                           true);
        search_own_order_ = on ? order : -1;
        log_.Record({time_, DebriefEntry::Kind::Scenario, by, "battery net", words});
        return true;
    }
    // A launcher's commander heard the order to search on his own (and it stands).
    bool SearchOwnHeard(const std::string& callsign) const {
        if (search_own_order_ < 0 || !order_net_) return false;
        for (const command::OrderVehicle& v : order_net_->Vehicles()) {
            if (v.callsign == callsign) return order_net_->Heard(search_own_order_, v.vehicle);
        }
        return false;
    }
    // What the commander sees: a raid announced (the warning, not the standing readiness: an empty board is normal while
    // nothing flies) and nothing new on his board this long means the post has no air picture to designate from (out of
    // the nets' reach, jammed, the radar company gone): the launchers search on their own. A fresh mark on the board is
    // the picture back: they fire on the designations again.
    void UpdatePictureLost() {
        if (!battery_) return;
        double fresh_s = -1e9;
        for (const BoardMark& m : post_->Board().Marks()) fresh_s = std::max(fresh_s, m.time_s);
        const double since = time_ - raid_warned_s_;
        const bool no_picture = raid_warned_s_ >= 0.0 && since >= kNoPictureS && since < kRaidWarningS && time_ - fresh_s >= kNoPictureS;
        const bool picture = time_ - fresh_s < kPictureBackS;
        if (search_own_order_ < 0 && no_picture && !MarchRadioBan()) {
            SeatDuty d = SeatDuty::Order(std::string(), "order_battery_search_own", "a raid announced and nothing on the board: the launchers search on their own");
            d.keep_s = 120.0;
            if (NpcDoes(Cmdr(), d)) OrderSearchOwn(true, "Battery commander");
        } else if (search_own_order_ >= 0 && picture && !MarchRadioBan()) {
            SeatDuty d = SeatDuty::Order(std::string(), "order_battery_by_designation", "the air picture back on the board: the launchers on the designations");
            d.keep_s = 120.0;
            if (NpcDoes(Cmdr(), d)) OrderSearchOwn(false, "Battery commander");
        }
    }
    static constexpr double kNoPictureS = 240.0;  // sam3d: the warning's lead and the radar company's first report, with a margin
    static constexpr double kPictureBackS = 60.0; // sam3d: a mark plotted within the last minute
    static constexpr double kRaidWarningS = 1800.0;  // sam3d: a raid's warning stands for half an hour
    int search_own_order_ = -1;                   // the standing order to search on their own (the order net's), -1 none
    double raid_warned_s_ = -1.0;                 // a raid announced then (a warning trigger: a package's), -1 none
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
    void StartLines() {
        for (const LineDef& l : def_.radio_data.lines) {
            const std::string n = std::to_string(l.line);
            if (!l.laid || console_.panel.Find("wire.line" + n) == nullptr || console_.equipment.Def("line" + n + "_laid") == nullptr) continue;
            console_.panel.Set("wire.line" + n, 1);
            console_.equipment.SetInput("line" + n + "_run", true);
            console_.equipment.Update(0.0, console_.panel);
            console_.equipment.Warm("line" + n + "_laid", console_.panel);
        }
    }
    // The cable paid out behind a man on his way to a line's far end: a point every 5 m, its length as he walks it.
    // There, the run is out (line<n>_run): he connects the far set, his job's control.
    void PayOut(const std::string& station, const CrewMover& m, const VehiclePose& pose) {
        const int n = LinePlace(m.At());
        if (n == 0) return;
        const MoverView v = m.View(pose);
        if (v.place != BodyPlace::Ground) return;
        WireRun& w = wires_[n];
        if (w.reached) return;
        const Vec3 at{v.feet_enu.x, v.feet_enu.y, v.feet_enu.z};
        if (w.station != station || w.points.empty()) {
            w = WireRun();
            w.station = station;
            w.points.push_back(at);
            wire_last_[n] = at;
        }
        const Vec3 last = wire_last_[n];
        w.length_m += std::hypot(at.x - last.x, at.y - last.y);
        wire_last_[n] = at;
        const Vec3& p = w.points.back();
        if (std::hypot(at.x - p.x, at.y - p.y) >= 5.0) w.points.push_back(at);
        Vec3 end_at;
        if (m.Moving() || !LineFarEnd(n, end_at) || std::hypot(end_at.x - at.x, end_at.y - at.y) > 3.0) return;
        if (std::hypot(at.x - w.points.back().x, at.y - w.points.back().y) > 0.01) w.points.push_back(at);
        w.reached = true;
        console_.equipment.SetInput("line" + std::to_string(n) + "_run", true);
        const LineDef* l = def_.radio_data.Line(n);
        Event("line " + std::to_string(n), station + " ran the P-274M out to " + (l != nullptr ? l->to_name : std::string("its far end")) + ": " +
                                              std::to_string(static_cast<int>(std::lround(w.length_m))) + " m");
    }
    void UpdateLines() {
        for (const LineDef& l : def_.radio_data.lines) {
            if (!LineLaid(l)) continue;
            LineState& st = lines_[l.line];
            const std::string lamp = "line" + std::to_string(l.line) + "_call";
            const auto run = wires_.find(l.line);
            if (run != wires_.end() && run->second.reached && !st.checked) {  // the man who laid it rings the post on it
                st.checked = true;
                st.queued.push_back("Линия проложена. Проверка связи.");
            }
            if (!st.ringing && st.next_call < l.calls.size() && l.calls[st.next_call].at_s <= time_ + 1e-9) {
                st.ringing = true;
                st.ring_from_s = time_;
                st.text = l.calls[st.next_call++].text;
                Event("line " + std::to_string(l.line), l.to_name + " rings");
            } else if (!st.ringing && !st.queued.empty()) {
                st.ringing = true;
                st.ring_from_s = time_;
                st.text = st.queued.front();
                st.queued.erase(st.queued.begin());
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
    const LineDef* LineToLauncher(const std::string& callsign) const { return LaidLineTo(callsign); }
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
    bool moved_since_report_ = false;  // the battery has moved since its last report: the next one says it has arrived
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
        char text[256];
        std::snprintf(text, sizeof(text), "%s, я %s: %sк бою готова, пусковых %d, ракет %d. Приём.", rn->main_call_sign.c_str(),
                      rn->own_call_sign.c_str(), moved_since_report_ ? "в назначенный район прибыла, " : "", ready, missiles);
        Radio("regiment net", SpeakerName(talker), text, false, NetHearers("regiment"));
        Transmitted("regiment", SpeakerName(talker), 5.0);
        log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(talker), "regiment", text});
        if (moved_since_report_ || task_plan_.march_at_s < 0.0) TaskReadyReported(text);
        if (moved_since_report_ && regiment_net_) {  // the regiment's order to move is done now (RegimentStep)
            for (const auto& kv : regiment_orders_) {
                if (kv.second.passed && MovesBattery(kv.second.order.kind) && !regiment_net_->IsDone(kv.first, def_.battery_giver)) {
                    regiment_net_->Done(kv.first, def_.battery_giver, time_, text);
                }
            }
        }
        moved_since_report_ = false;
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
    // With a journal it is the keeper, reading from the page: only a report already written there.
    bool PlayerReadOutAirReport(const std::string& seat) {
        if (!post_ || !callouts_ || post_->Held().empty()) return false;
        if (console_.post_journal_keeper.empty()) {
            if (!SeatHearsNet(seat, "air_picture")) return false;
            ReadOut(seat, post_->Held().front());
            return true;
        }
        if (seat != console_.post_journal_keeper) return false;
        for (const CommandPost::AirReport& r : post_->Held()) {
            if (!InJournal(r)) continue;
            ReadOut(seat, r);
            return true;
        }
        return false;
    }
    // The board numbers the plotter can write now: the reports he heard, and those he can read in the journal that are not
    // on the board since (he finds them on the page, not on a screen).
    std::vector<int> PlotterReportNumbers() const {
        std::vector<int> out;
        if (!post_) return out;
        for (const CommandPost::AirReport& h : post_->Heard()) out.push_back(h.number);
        for (const RadioJournalEntry& e : journal_.Legible(time_)) {
            CommandPost::AirReport r;
            if (!JournalReport(e, r) || OnBoardSince(e.said_number, e.heard_s)) continue;
            if (std::find(out.begin(), out.end(), e.number) == out.end()) out.push_back(e.number);
        }
        return out;
    }
    const RadioJournal& Journal() const { return journal_; }

    // ---- the after-action report from the records (roadmap P9, campaign/report.hpp; docs/command/01_PRINCIPLES.md 9)
    // The battery's combat report compiled from the post's records as they stand now: the radiotelephonist's journal as
    // written, the board's marks, the battery commander's log of his launchers' reports. Never the truth. False without a
    // post (the Osa's crew writes no journal; its commander's report is P4's, command/session_reports.hpp). From `from_s`:
    // the records written since (a raid's report: roadmap 18.1).
    bool ReportFromRecords(campaign::CombatReport& out, const std::string& unit = "The battery", double from_s = 0.0) const {
        if (!post_) return false;
        const CommandPost* post = post_.get();
        std::vector<campaign::JournalLine> journal;
        for (const campaign::JournalLine& l : campaign::JournalLines(journal_, time_)) {
            if (l.heard_s >= from_s) journal.push_back(l);
        }
        std::vector<campaign::BoardRecord> board;
        for (const campaign::BoardRecord& m : campaign::BoardRecords(post_->Board().Marks())) {
            if (m.time_s >= from_s) board.push_back(m);
        }
        std::vector<campaign::LogEntry> log;
        for (const campaign::LogEntry& e : commander_log_) {
            if (e.time_s >= from_s) log.push_back(e);
        }
        out = campaign::CompileReport(unit, from_s, time_, journal, board, log, def_.after_action,
                                      [post](const Vec3& p) { return post->PlaceWords(p); });
        return true;
    }
    // The regiment's check of the post's journal and board against the radar company's log of what it said on the
    // air-picture net. False without a post.
    bool CheckRecords(campaign::RecordCheck& journal, campaign::RecordCheck& board) const {
        if (!post_) return false;
        journal = campaign::CheckJournal(campaign::JournalLines(journal_, time_), radar_log_, def_.after_action);
        board = campaign::CheckBoard(campaign::BoardRecords(post_->Board().Marks()), radar_log_, def_.after_action);
        return true;
    }
    // Every report said on the air-picture net, heard at the post or not (the radar company's own log).
    const std::vector<campaign::SaidReport>& RadarLog() const { return radar_log_; }
    // The battery commander's log: each launcher's launch and result as the post heard it on the battery net.
    const std::vector<campaign::LogEntry>& CommanderLog() const { return commander_log_; }
    const campaign::AfterActionValues& AfterAction() const { return def_.after_action; }
    // The player radiotelephonist takes a message down (radio_journal.hpp's dictation): his pencil on the page starts
    // the oldest one he heard and hasn't written (or the line he left half-written); digits fill its boxes; Finish
    // writes it as it stands, `brief` the number and the time only. False if the seat isn't the journal's keeper's held
    // by a player, or (Begin) with nothing to write.
    bool JournalBegin(const std::string& seat) {
        if (!PlayerKeeps(seat)) return false;
        return journal_.BeginDraft(time_);
    }
    bool JournalType(const std::string& seat, char digit) {
        if (!PlayerKeeps(seat) || journal_.Draft() == nullptr) return false;
        journal_.Type(digit);
        return true;
    }
    bool JournalRub(const std::string& seat) {
        if (!PlayerKeeps(seat) || journal_.Draft() == nullptr) return false;
        journal_.Rub();
        return true;
    }
    bool JournalNextBox(const std::string& seat, int step) {
        if (!PlayerKeeps(seat) || journal_.Draft() == nullptr) return false;
        journal_.NextBox(step);
        return true;
    }
    bool JournalFinish(const std::string& seat, bool brief) {
        if (!PlayerKeeps(seat) || !journal_.Finish(time_, brief)) return false;
        const RadioJournalEntry& e = journal_.Written().back();
        char line[160];
        std::snprintf(line, sizeof(line), "wrote in the journal%s: %d of %d digits as said", e.brief ? " (short form)" : "", e.digits_right, e.digits);
        Event(seat, line);
        return true;
    }
    bool PlayerKeeps(const std::string& seat) const {
        return post_ && !console_.post_journal_keeper.empty() && seat == console_.post_journal_keeper && !SeatHolder(seat).empty();
    }

private:
    // A held report read out to the plotter (the callout "air_report", aloud beside him); heard, it goes to his hand.
    // With a journal it is read from the page: as written, right or wrong.
    std::map<int, CommandPost::AirReport> reading_out_;  // the number it was said under -> the report as read out
    void ReadOut(const std::string& seat, const CommandPost::AirReport& r) {
        if (reading_out_.count(r.number) != 0) return;
        CommandPost::AirReport read = r;
        if (!console_.post_journal_keeper.empty()) {
            const RadioJournalEntry* e = journal_.AirReport(r.number, r.heard_s - 2.0, time_);
            if (e == nullptr || !JournalReport(*e, read)) return;
        }
        reading_out_[r.number] = read;
        KnownTarget k;
        k.target = read.contact.target_id;
        k.number = r.number;
        callouts_->Say(time_, "air_report", seat, {{"report", read.text}}, NpcAt(seat) ? SpeakDelayS(seat) : 0.0, &k);
    }
    // A report heard while the plotter was off FuG-1 (held): with a journal, it is read to him from the page, so only once
    // written; an NPC plotter reads the page himself (PlotterReadsJournal).
    void RelayAirPicture() {
        if (!post_ || !voice_ || !callouts_) return;
        const std::string& keeper = console_.post_journal_keeper;
        if (!keeper.empty() && SeatHolder(console_.post_plotter).empty()) return;
        for (const CommandPost::AirReport& r : post_->Held()) {
            if (reading_out_.count(r.number) != 0) continue;
            if (!keeper.empty()) {
                const RadioJournalEntry* e = journal_.AirReport(r.number, r.heard_s - 2.0, time_);
                CommandPost::AirReport readable;
                if (e == nullptr || !JournalReport(*e, readable)) continue;  // not written, or written so it can't be read out
                SeatDuty d = SeatDuty::Report(keeper, "air_report", "a report the plotter did not hear, written in the journal: read it to him");
                d.number = r.number;
                if (NpcDoes(keeper, d)) ReadOut(keeper, r);
                continue;
            }
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

    // ---- the radiotelephonist's journal (radio_journal.hpp; pvo_bu_x_1984 art. 24): every message the post's radios
    // carry that he hears, written by hand, one at a time; the page is the only way back to a message missed.
    RadioJournal journal_;
    std::map<int, CommandPost::AirReport> journal_air_;  // each board number's latest air report heard, for the page's entry
    int journal_press_ = 0;
    int journal_reads_ = 0, journal_reads_wrong_ = 0;    // reports plotted from the page, and those it had wrong
    std::vector<campaign::SaidReport> radar_log_;        // what the radar company said on the air-picture net (P9)
    std::vector<campaign::LogEntry> commander_log_;      // the launchers' reports the post heard (P9)
    // Does the journal's keeper hear this net now (his box on its set; everyone with no voice paths)?
    bool KeeperHears(const std::string& label) const {
        const std::string& keeper = console_.post_journal_keeper;
        if (keeper.empty()) return false;
        if (!voice_) return true;
        return SeatHearsNet(keeper, NetIdOf(label));
    }
    // Is this air report (as said) written legibly in the journal?
    bool InJournal(const CommandPost::AirReport& r) const { return journal_.AirReport(r.number, r.heard_s - 2.0, time_) != nullptr; }
    // A mark of this board number on the board since `since_s`?
    bool OnBoardSince(int number, double since_s) const {
        if (!post_) return false;
        for (const BoardMark& m : post_->Board().Marks()) {
            if (m.number == number && m.time_s + 1e-9 >= since_s) return true;
        }
        return false;
    }
    // The air report as a journal entry has it: what was said, read through what was written (a wrong digit puts it
    // elsewhere, a wrong number makes a second track). False for a message that is no air report, a stale one, or an
    // entry that doesn't read as a report (the short form, an empty box).
    bool JournalReport(const RadioJournalEntry& e, CommandPost::AirReport& out) const {
        if (!post_ || e.said_number <= 0 || time_ - e.heard_s > post_->Def().air_net_period_s) return false;
        const auto it = journal_air_.find(e.said_number);
        if (it == journal_air_.end() || std::fabs(it->second.heard_s - e.heard_s) > 2.0) return false;
        return post_->FromWords(it->second, e.text, out);
    }
    // Writes a report read off the page onto the board (a plotter reading it, or hearing it read to him from it).
    void TakeFromPage(const RadioJournalEntry& e, const CommandPost::AirReport& r) {
        if (!post_->Recall(r, time_, e.said_number)) return;
        ++journal_reads_;
        if (e.text != e.said) ++journal_reads_wrong_;
        Event(console_.post_plotter, "read from the journal: " + e.text + (e.text != e.said ? "  (said: " + e.said + ")" : std::string()));
    }
    // The keeper writes: an NPC the oldest message waiting whenever he is free, now and then a digit wrong (his tier's
    // slip_chance); a player takes it down as dictation, his pencil on the page (journal_write, or the page itself in
    // the cabin: JournalBegin). A message not written in time is lost.
    void UpdateJournal() {
        const std::string& keeper = console_.post_journal_keeper;
        if (!post_ || keeper.empty()) return;
        const AspdDef& a = post_->Def();
        const int lost = journal_.Forgotten();
        journal_.Forget(time_, a.journal_remember_s);
        if (journal_.Forgotten() > lost) Event(keeper, "a message heard and never written in the journal");
        const int press = console_.panel.Get(console_.post_journal_write);
        const bool pressed = press == 1 && journal_press_ != 1;
        journal_press_ = press;
        if (journal_.Waiting().empty() || journal_.Busy(time_)) return;
        const double speed = SeatSpeed(keeper);
        const double write_s = a.journal_write_s / (speed > 0.1 ? speed : 0.1);
        if (NpcDoes(keeper, SeatDuty::Control(keeper, console_.post_journal_write, 1, "a message heard: write it in the journal"))) {
            const CrewMember* m = MemberAt(keeper);
            if (m != nullptr && m->IsBusy()) return;
            const SkillTier* t = TierAt(keeper);
            const bool slip = t != nullptr && static_cast<double>(hear_rng_()) / 4294967296.0 < t->slip_chance;
            journal_.WriteNext(time_, write_s, slip ? static_cast<int>(hear_rng_() % 1000u) : -1);
        } else if (pressed) {
            journal_.BeginDraft(time_);
        }
    }
    // The NPC plotter, with nothing heard waiting, looks over the journal for a report he has not written: one that came
    // while his box was off FuG-1, or one he didn't take in (his tier's miss_chance). He writes it as the page has it.
    void PlotterReadsJournal() {
        if (!post_ || console_.post_journal_keeper.empty() || !SeatHolder(console_.post_plotter).empty() || !post_->Heard().empty()) return;
        const CrewMember* plotter = MemberAt(console_.post_plotter);
        if (plotter == nullptr || plotter->IsBusy()) return;
        for (const RadioJournalEntry& e : journal_.Legible(time_)) {
            CommandPost::AirReport r;
            if (!JournalReport(e, r) || OnBoardSince(e.said_number, e.heard_s) || OnBoardSince(e.number, e.heard_s)) continue;
            bool missed = false;
            for (const CommandPost::AirReport& h : post_->Held()) missed = missed || h.number == e.said_number;
            if (!missed) continue;
            TakeFromPage(e, r);
            return;
        }
    }
    // A report the player plotter writes from the journal's page (its number as written): back among those waiting for
    // his hand, as the page has it.
    void RecallFromJournal(int number) {
        if (!post_) return;
        for (const CommandPost::AirReport& h : post_->Heard()) {
            if (h.number == number) return;
        }
        const std::vector<RadioJournalEntry> page = journal_.Legible(time_);
        for (auto it = page.rbegin(); it != page.rend(); ++it) {
            CommandPost::AirReport r;
            if (it->number != number || OnBoardSince(it->said_number, it->heard_s) || !JournalReport(*it, r)) continue;
            TakeFromPage(*it, r);
            return;
        }
    }
    // The NPC plotter hears each report on his own headset; now and then one doesn't go in (his tier's miss_chance, more
    // under stress): it waits, unwritten, until he finds it on the page.
    void PlotterMayMiss(const CommandPost::AirReport& r) {
        if (console_.post_journal_keeper.empty() || !SeatHolder(console_.post_plotter).empty()) return;
        for (std::size_t i = 0; i < crew_.size(); ++i) {
            if (crew_[i].Role() != console_.post_plotter) continue;
            const SkillTier* t = i < def_.crew.size() ? SkillOf(def_.crew[i]) : nullptr;
            const double miss = t != nullptr ? t->miss_chance * TierStress(*t, crew_[i]) : 0.0;
            if (static_cast<double>(hear_rng_()) / 4294967296.0 < miss && post_->Miss(r.number)) {
                Event(console_.post_plotter, "did not take in the report of " + std::to_string(r.number) + ": he will look in the journal");
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
        // A target's number is read back when it is first given; the post's updates of it need none (sam3d: a raid of
        // sixteen updated every few seconds read back every time kept the commander on the air).
        if (!read_back_.insert(e.number).second) return;
        char n[16];
        std::snprintf(n, sizeof(n), "%04d", e.number);
        callouts_->Say(time_, "readback", Cmdr(), {{"number", n}, {"callsign", OwnCallsign()}, {"post", "10"}}, SpeakDelayS(Cmdr()));
        if (LinkedToPost()) own_readbacks_.push_back(e.number);
    }
    std::set<int> read_back_;  // the target numbers read back
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
        if (comms_ && comms_->HasChest() && !comms_->ChestKeyed(console_)) {  // on the air only with his chest switch at ПРД (7.8)
            last_refusal_ = "the chest switch is at receive (hold it at ПРД to talk)";
            TransmitBlocked(last_refusal_);
            return false;
        }
        const std::map<std::string, std::string> fields = NetReportFields(callout_id);
        if (!callouts_->Say(time_, callout_id, Cmdr(), fields)) return false;
        double said_s = 0.0;
        if (const CalloutDef* def = def_.crew_data.FindCallout(callout_id)) {
            said_s = callouts_->LineTimeS(callout_id, Cmdr(), FillCallout(def->ru, fields));
            OnAir(said_s + 0.5);
            player_words_until_s_ = time_ + said_s + 0.5;
        }
        reports_made_.push_back({time_, station, callout_id});
        if (callout_id == "readback" && LinkedToPost()) own_readbacks_.push_back(designations_heard_.back().number);
        if (order_net_) OwnSaidOnNet(callout_id, time_ + said_s);
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

    // ---- The emission regime (emission_regime.hpp, docs/crew/16_EMISSION.md)
    // The Osa: the regime its commander has heard from the battery, and the one he has told his crew (the senior
    // operator keeps the bursts by this one). The post: the regime its battery commander has ordered (both the same).
    const EmissionRegime& CommanderRegime() const { return regime_cmdr_; }
    const EmissionRegime& CrewRegime() const { return regime_crew_; }
    // The battery commander in a linked post said a regime on the battery net (battery_link.hpp): heard if the Osa's set
    // hears the net; missed, the Osa goes on under the old one.
    void HearBatteryEmissionOrder(const EmissionRegime& r) {
        if (!(c2_ == nullptr || c2_->Hearing())) {
            Event("battery net", std::string("the battery commander's order of the emission regime (") + EmissionKindName(r.kind) + ") was not heard");
            return;
        }
        Radio("battery net", "10", "Всем, я 10: " + EmissionGroup(r) + ". " + EmissionGroup(r) + ".");
        if (const TalkEntry* e = def_.talk_tables.ByEffect(EmissionEffect(r.kind))) HeardGroup(e->code, "battery", BatteryNetHearers());
        CommanderHearsRegime(r, "the battery net");
    }
    // The post's side of the link: the regimes said on the battery net since the last call; a launcher's report of an ARM
    // attack, heard on the net (its callsign and the azimuth as said).
    std::vector<EmissionRegime> TakeBatteryEmissionOrders() {
        std::vector<EmissionRegime> out;
        out.swap(battery_emission_orders_);
        return out;
    }
    void HearArmAttackReport(const std::string& callsign, const std::string& azimuth) {
        Radio("battery net", callsign, callsign + ": пуск ПРР, азимут " + azimuth + "!", false, BatteryNetHearers());
        log_.Record({time_, DebriefEntry::Kind::Scenario, callsign, "battery net", "reports an ARM attack, azimuth " + azimuth});
        if (arm_report_heard_s_ < 0.0) arm_report_heard_s_ = time_;
    }
    // The Osa's side: its commander's reports of an ARM attack on the battery net since the last call (the azimuths).
    std::vector<std::string> TakeArmAttackReports() {
        std::vector<std::string> out;
        out.swap(arm_reports_out_);
        return out;
    }
    // The battery commander's order of a regime (the orders menu at the post): false with LastRefusal() saying why.
    bool CommanderOrderEmission(EmissionKind k) {
        if (def_.has_crew_data) return GiveCrewOrder(BatteryEmissionOrderId(k));
        return OrderBatteryEmission(def_.emission.Make(k, "battery"), "Battery commander");
    }
    // How the crew kept the regime: at the halt with nothing tracked under a burst regime they were told, the seconds
    // there and the seconds the search transmitter radiated; the reminders and the breaches that called for one; how long
    // after the commander heard each regime his crew was told (the last).
    struct EmissionRecord {
        double bursts_s = 0.0;
        double radiating_s = 0.0;
        int reminders = 0;
        int breaches = 0;
        int regimes_heard = 0;
        int regimes_told = 0;
        double told_after_s = -1.0;
    };
    const EmissionRecord& GetEmissionRecord() const { return emission_record_; }

    // ---- Our radars and warning posts through the session (roadmap 17.3; campaign/mission_session.hpp follows the war's
    // damage to them). A radar of the regiment joins the post's feed in play: false, with why, if the post has no
    // regiment's radars, the type is not in its library, or the id is the session's already.
    bool AddRegimentRadar(const std::string& id, const std::string& type, const Vec3& at, std::string* why = nullptr) {
        const auto fail = [why](const std::string& w) {
            if (why != nullptr) *why = w;
            return false;
        };
        if (!post_ || def_.post_regiment.radars.empty()) return fail("the post has no regiment's radars to join");
        for (const RegimentRadar& r : def_.post_regiment.radars) {
            if (r.id == id) return fail("radar '" + id + "' is the session's already");
        }
        const RegimentRadar* t = def_.post_radar_types.Find(type);
        if (t == nullptr) return fail("no radar type '" + type + "' in the post's library");
        RegimentRadar r = *t;
        r.id = id;
        r.type = type;
        r.position = at;
        def_.post_regiment.radars.push_back(r);
        Event("regiment's radar", id + " (" + type + ") feeds the regiment's picture");
        return true;
    }
    // A radar of the regiment's off the air from now (struck) or back on it (repaired): its picture stops at the board, or
    // comes again. False for an unknown radar, or one already so.
    bool SetRegimentRadarDown(const std::string& id, bool down, const std::string& reason) {
        for (RegimentRadar& r : def_.post_regiment.radars) {
            if (r.id != id) continue;
            if (CommandPost::RadarDownAt(r, time_) == down) return false;
            if (down) {
                C2Outage o;
                o.from_s = time_;
                o.reason = reason;
                r.outages.push_back(o);
            } else {
                for (C2Outage& o : r.outages) {
                    if (o.from_s <= time_ && o.to_s > time_) o.to_s = time_;
                }
            }
            Event("regiment's radar", id + (down ? " off the air: " : " on the air again: ") + reason);
            return true;
        }
        return false;
    }
    // A warning source of the vehicle's (c2_sources.hpp) down from now or restored, as a trigger would. False without
    // the source.
    bool SetC2SourceDown(const std::string& id, bool down, const std::string& reason) {
        if (!c2_) return false;
        bool known = false;
        for (const C2SourceDef& s : def_.c2_sources) known = known || s.id == id;
        if (!known) return false;
        c2_->SetFailed(id, down, time_, reason);
        Event("C2 source", id + (down ? " out of action: " : " working again: ") + reason);
        return true;
    }
    // The regiment says something to the battery on its command net `after_s` from now: heard on the post's R-111 when
    // it is on the net, said again until the post receipts it, as the scenario's own messages. False without the R-111
    // worked as a set.
    bool RegimentSays(const std::string& text, double after_s) {
        if (!r111_) return false;
        RegimentMessage m;
        m.at_s = time_ + after_s;
        m.text = text;
        def_.radio_plan.messages.push_back(m);
        MessageState st;
        st.next_s = m.at_s;
        messages_.push_back(st);
        return true;
    }

    // A neighbour's kill (roadmap 17.1): the neighbour reports it up and the regiment tells the battery on its command net
    // `after_s` from now, by the number the post's board has the target under. Heard at the post, the mark is known
    // destroyed: wiped from the board as the commander reads it, given to no launcher again. False without the R-111
    // worked as a set, or with no mark of it on the post's board (nothing for the post to match it to).
    bool RegimentTellsDestroyed(const std::string& target, const std::string& by, double after_s) {
        if (!r111_ || !post_) return false;
        int number = 0;
        double at = -1.0;
        for (const BoardMark& m : post_->Board().Marks()) {
            if (m.target_id != target || m.time_s < at) continue;
            number = m.number;
            at = m.time_s;
        }
        if (number <= 0) return false;
        char text[160];
        std::snprintf(text, sizeof(text), "Regiment to the battery: target %04d destroyed by %s.", number, by.c_str());
        if (!RegimentSays(text, after_s)) return false;
        def_.radio_plan.messages.back().destroyed = target;
        return true;
    }

    // The roads as they are (roadmap 17.4): a crossing of the session's roads down in truth from now, or open again. A
    // mover finds it only when it comes to it; the battery commander knows only what he is told. False without the roads,
    // for a crossing they do not have, or one already so.
    bool SetCrossingDown(const std::string& crossing, bool down, const std::string& entry, const std::string& fact = "") {
        if (!nav_ || !nav_->SetCrossingDown(crossing, down, time_ / 3600.0, entry, fact)) return false;
        Event("roads", "the bridge " + crossing + (down ? " is down" : " is open again"));
        return true;
    }
    // The regiment tells the battery on its command net that a road's bridge is down, or open again, `after_s` from now:
    // heard on the post's R-111, the battery commander's roads have it (`entry` the damage it stands for: heard again, the
    // same). False without the R-111 worked as a set, or for a crossing the session's roads do not have.
    bool RegimentTellsRoad(const std::string& crossing, const std::string& name, bool down, const std::string& entry, double after_s) {
        if (!nav_ || nav_->Graph().Crossing(crossing) == nullptr) return false;
        if (!RegimentSays("Regiment to the battery: the " + name + (down ? " is down." : " is open again."), after_s)) return false;
        RegimentMessage& m = def_.radio_plan.messages.back();
        m.road_crossing = crossing;
        m.road_down = down;
        m.road_entry = entry;
        return true;
    }
    // The battery commander sends the TZM to the regiment's technical battery for missiles (roadmap 17.4): an order on the
    // battery net to its chief, who plans the way on the roads the commander knows. Refused (last_refusal_ why) without a
    // TZM on the net, while a run stands, on the march (the radio ban), with the TZM full by his reckoning or not at the
    // battery, or with no way there on the roads he knows.
    bool OrderTzmRun(const std::string& by) {
        auto refuse = [this](const std::string& why) {
            last_refusal_ = why;
            return false;
        };
        if (tzm_id_.empty() || !order_net_ || !nav_) return refuse("no TZM on the battery net to send");
        if (tzm_run_order_ >= 0) return refuse("the TZM is on its run already");
        if (MarchRadioBan()) return refuse("on the march the radio is for fire control only");
        const Battery::Vehicle* v = battery_->Find(tzm_id_);
        if (v == nullptr || v->away || v->broken) return refuse("the TZM is not at the battery");
        if (tzm_reckoned_ >= static_cast<int>(def_.logistics.tzm_missiles)) return refuse("the TZM is full, by what he was told");
        const nav::Route way = PlanTzmRun();
        if (!way.found) return refuse("no way to the technical battery on the roads he knows: " + way.why);
        KeepOrderNetCurrent();
        const CalloutDef* c = def_.crew_data.FindCallout("order_tzm_run");
        const std::string words = c != nullptr ? c->ru : "ТЗМ — на техническую батарею за ракетами!";
        const int order = order_net_->Give("tzm_run", words, "radio_voice", time_, {tzm_id_});
        order_words_[order] = {"на техническую батарею за ракетами", "to the technical battery for missiles"};
        notice_orders_.insert(order);  // the battery does not move on it: the TZM's run does
        supply_orders_.insert(order);
        tzm_run_order_ = order;
        tzm_told_ = roads_believed_;
        GiverJournal().Heard(time_, "battery net", "Battery commander", words);
        Radio("battery net", BatteryPostCall(), TzmCallsign() + ", я " + BatteryPostCall() + ": " + words);
        log_.Record({time_, DebriefEntry::Kind::Scenario, by, "battery net", words});
        return true;
    }
    // The TZM's way from where it stands to the technical battery now, on the roads the battery commander knows.
    nav::Route PlanTzmRun() const {
        nav::Route r;
        const Battery::Vehicle* v = battery_ && !tzm_id_.empty() ? battery_->Find(tzm_id_) : nullptr;
        const nav::Anchor* s = nav_ ? nav_->GetAnchors().Find(def_.supply_siding) : nullptr;
        const nav::Mobility* cls = nav_ ? nav_->MobilityClasses().Find(kTzmMobility) : nullptr;
        if (v == nullptr || s == nullptr || cls == nullptr) {
            r.why = "no TZM run in this session";
            return r;
        }
        nav::RouteRequest q;
        q.from = {v->position.x, v->position.y};
        q.to = s->at;
        q.mobility = cls;
        q.h = time_ / 3600.0;
        q.night = light_.dark;
        q.swim = false;  // loaded, it keeps out of the water (campaign/stocks.hpp)
        return nav_->Graph().Find(q, roads_believed_);
    }
    const nav::Overlay& RoadsBelieved() const { return roads_believed_; }  // the battery commander's
    const campaign::TzmRun* GetTzmRun() const { return tzm_run_.get(); }    // the last run (null: none)
    int TzmReckoned() const { return tzm_reckoned_; }                       // his reckoning of the missiles on the TZM
    const campaign::TechnicalBattery& Siding() const { return siding_; }

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
    // The enemy's jamming of a net on its own times (a package's jammers, air_war/comms_jamming.hpp): `by` names it in the
    // log. False if the session has no such net.
    bool AddNetJamming(const JammingDef& j, const std::string& by) {
        if (!nets_ || j.nets.empty() || def_.radio_data.Find(j.nets.front()) == nullptr) return false;
        nets_->AddJamming(j);
        char span[64];
        std::snprintf(span, sizeof(span), " from %.0f s to %.0f s", j.from_s, j.to_s);
        Event(j.nets.front() + " net", by + ": " + (j.strong ? "strong " : "weak ") + (j.spot ? "spot" : "barrage") + " jamming" + span);
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
    // The far end has something to say on a laid line: it rings once the line is free.
    void QueueLineCall(int line, const std::string& text) {
        const LineDef* l = def_.radio_data.Line(line);
        if (l == nullptr || !LineLaid(*l)) return;
        lines_[line].queued.push_back(text);
    }
    bool RingLine(int line) {
        const LineDef* l = def_.radio_data.Line(line);
        if (l == nullptr || !LineLaid(*l)) return false;
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
            const bool reach = post_->TelecodeInReach(def_.post_regiment, time_) || (def_.post_regiment.telecode_wire && def_.post_regiment.telecode);
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
        // The cabled radar's picture (flak11 PU12-0's figure; p19_to 4.3.1): the targets it covers while it is on the air,
        // each a friend only if its interrogator works and the target's transponder answers (the "Kennung" line).
        std::vector<BoardContact> radar;
        if (post_em_ && post_em_->Ok()) {
            radar = PostEmPicture(truth);  // its echoes as the EM layer gives them, its jamming strobes beside them
        } else if (def_.has_post_radar && !PostRadarOff()) {
            for (const BoardContact& c : truth) {
                if (!CommandPost::Covers(def_.post_radar, c)) continue;
                BoardContact e = c;
                e.type = def_.post_radar.iff_interrogator && c.transponder ? 1 : 9;
                radar.push_back(e);
            }
        }
        if (!console_.post_radar_here.empty() && console_.equipment.GetInput(console_.post_radar_here) != def_.has_post_radar) {
            console_.equipment.SetInput(console_.post_radar_here, def_.has_post_radar);
        }
        UpdateRadarLine();
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
        if (voice_) {  // through the crew: a seat's box on FuG-1, the set on its net; the plotter's own headset, or read out
            in.air_net = in.air_net && !NetHearers("air_picture").empty();
            in.relay = !SeatHearsNet(console_.post_plotter, "air_picture");
            in.air_net_loss = nets_ && nets_->Jam("air_picture", time_) == 1 ? Tuning().weak_jam_loss : 0.0;
        }
        // The tube is the commander-operator's (rwd_mb3): his hands take the cabled radar's targets off, an NPC's when
        // he is free. A new echo: an NPC takes the nearest off himself; a player owes it with the DT-2 (the marker on it,
        // then СЪЁМ: UpdateKeypad). The held ones the ASPD-U's semi-automatic round fixes again either way.
        const CrewMember* commander = MemberAt(Cmdr());
        in.tube_free = commander != nullptr && (!SeatHolder(Cmdr()).empty() || !commander->IsBusy());
        in.tube_speed = SeatSpeed(Cmdr());
        in.take_off_new = SeatHolder(Cmdr()).empty();
        in.send = !console_.post_sending.empty() && console_.equipment.IsReady(console_.post_sending);
        post_->Update(time_, CommandPost::RegimentSees(def_.post_regiment, truth, time_), radar, in);
        if (in.primary || in.imitation) OweTakeOff();
        if (in.send && post_->LastSentS() == time_) {
            if (!post_sending_) Event("telecode", "the post sends its picture on: " + std::to_string(post_->Sent().size()) + " targets");
            post_sending_ = true;
        } else if (!in.send && post_sending_) {
            post_sending_ = false;
            Event("telecode", "the post stops sending its picture");
        }
        if (!SeatHolder(console_.post_plotter).empty()) {  // the NPC plotter writes each report he hears; a player owes them
            for (const CommandPost::AirReport& h : post_->Heard()) {
                NpcDoes(console_.post_plotter, SeatDuty::Plot(console_.post_plotter, h.number, h.contact.target_id,
                                                              "a report heard: write it on the board"));
            }
        }
        // What the post heard on the air-picture net, in its radio log (the plotter's headset; first person), and in the
        // radiotelephonist's head until he writes it in the journal.
        for (const CommandPost::Spoken& s : post_->TakeSpoken()) {
            radar_log_.push_back({time_, s.report.number, s.report.text});  // the company's own log, heard here or not
            if (!s.heard) continue;
            Radio("air picture", "Regiment", s.report.text);
            if (KeeperHears("air picture")) {
                journal_.Heard(time_, "air picture", "Regiment", s.report.text, s.report.number);
                journal_air_[s.report.number] = s.report;
            }
            if (!voice_ || SeatHearsNet(console_.post_plotter, "air_picture")) PlotterMayMiss(s.report);
        }
        UpdateJournal();
        PlotterReadsJournal();
        RelayAirPicture();
        bool written_held = false;
        for (const CommandPost::AirReport& h : post_->Held()) written_held = written_held || console_.post_journal_keeper.empty() || InJournal(h);
        SetRadioInput(console_.equipment.HasInput("air_report_waiting") ? std::string("air_report_waiting") : std::string(), written_held);
        SetRadioInput(console_.equipment.HasInput("journal_waiting") ? std::string("journal_waiting") : std::string(),
                      !journal_.Waiting().empty() || journal_.Draft() != nullptr);
        bool by_hand = false;
        for (const RadioJournalEntry& e : journal_.Written()) by_hand = by_hand || e.by_hand;
        SetRadioInput(console_.equipment.HasInput("journal_written_by_hand") ? std::string("journal_written_by_hand") : std::string(), by_hand);
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
                if (m.raw) continue;  // an echo has no number to mark: the symbol over it has
                const double d = std::hypot(m.x - marker_x_, m.y - marker_y_);
                if (d < best) {
                    best = d;
                    marked_ = m.number;
                }
            }
            if (marked_ >= 0) marked_arrival_s_ = ArrivalS(marked_);
            // СЪЁМ: the echo under the marker taken off (p19_to: by hand with the electronic marker).
            if (post_ && !c.post_marker_take_off.empty() && c.panel.Get(c.post_marker_take_off) == 1) {
                const int n = post_->TakeOffAt(time_, marker_x_, marker_y_, PostScaleM(), kMarkerCaptureFraction, SeatSpeed(Cmdr()));
                if (n >= 0) {
                    char text[48];
                    std::snprintf(text, sizeof(text), "echo taken off as %02d", n);
                    Event("IT-45", text);
                }
            }
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
    // The cabled radar in the EM layer (post.em): run its world to now with the targets where they are, the radar on the
    // air unless an outage has it off, its interrogator with it; then its picture as the ASPD gets it down the cable
    // (p19_to 4.3.1: echoes and identification marks): where its tracker puts each echo, a friend if a reply came back,
    // its ghosts (chaff, false echoes) as echoes too; and its strobes (p19_to 4: the jammer's azimuth).
    std::vector<BoardContact> PostEmPicture(const std::vector<BoardContact>& truth) {
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            if (!targets_[i].spawned) continue;
            const Aircraft* a = world_.FindAircraft(targets_[i].world_id);
            const ScenarioTarget& t = def_.targets[i];
            if (a != nullptr) post_em_->SyncTarget(t.id, a->alive && !targets_[i].gone, a->position, a->velocity, t.rcs_m2, t.friendly);
        }
        const bool on = !PostRadarOff();
        post_em_->SetSearch(on, false);
        post_em_->SetInterrogating(on && def_.post_radar.iff_interrogator);
        if (time_ > post_em_t_) post_em_->Advance(time_ - post_em_t_);
        post_em_t_ = time_;
        post_strobes_ = post_em_->SearchStrobes();
        std::vector<BoardContact> out;
        for (const EmSensorLink::Contact& c : post_em_->SearchPicture()) {
            BoardContact e;
            e.target_id = c.target_id;
            e.position = c.position;
            e.altitude_m = world_.Frame().AltitudeOf(c.position);
            for (const BoardContact& tc : truth) {
                if (tc.target_id != c.target_id) continue;
                e.altitude_m = tc.altitude_m;  // the height zone is the operator's to give (a 2-coordinate radar)
                e.formation = tc.formation;
                e.transponder = tc.transponder;
            }
            e.type = c.iff ? 1 : 9;
            out.push_back(e);
        }
        return out;
    }
    // The cabled radar's jamming strobes on the IT-45 (true azimuth from north, -180..180; J/N), while its picture is.
    std::vector<em::ViewStrobe> PostStrobes() const {
        std::vector<em::ViewStrobe> out;
        if (!post_ || !console_.equipment.IsReady(console_.post_primary)) return out;
        for (const EmSensorLink::Strobe& s : post_strobes_) out.push_back({s.true_az_deg, s.jn_db});
        return out;
    }
    // The cabled radar off the air now (its outages: the ARM drill, a fault).
    bool PostRadarOff() const {
        for (const C2Outage& o : def_.post_radar.outages) {
            if (time_ + 1e-9 >= o.from_s && time_ < o.to_s) return true;
        }
        return false;
    }
    // The cabled radar's crew on the service line (flak11 PU12-0's figure: the Dienstverbindung in the radar's cable):
    // they ring the post when their radar goes off the air and when it is back. The post hears it only on a line laid to
    // "radar" (the scenario's lines); otherwise its tube just goes empty.
    void UpdateRadarLine() {
        if (!def_.has_post_radar) return;
        const bool off = PostRadarOff();
        if (off == post_radar_off_) return;
        post_radar_off_ = off;
        std::string reason;
        for (const C2Outage& o : def_.post_radar.outages) {
            if (off && time_ + 1e-9 >= o.from_s && time_ < o.to_s) reason = o.reason;
        }
        Event("cabled radar", off ? "off the air" + (reason.empty() ? std::string() : ": " + reason) : std::string("on the air again"));
        const LineDef* l = LaidLineTo("radar");
        if (l != nullptr) QueueLineCall(l->line, off ? "Станция выключена. Работу прекратил." : "Станция включена, работаю.");
    }
    // A new echo on the tube while fewer than 12 are held: a player at the tube owes it with the DT-2 (the marker on the
    // nearest one, then СЪЁМ). An NPC takes it off himself (CommandPost, take_off_new).
    void OweTakeOff() {
        const double scale = PostScaleM();
        const BoardContact* e = post_->NextNewEcho();
        if (e == nullptr || !(scale > 0.0) || console_.post_marker_azimuth.empty()) return;
        const Vec3 rel = post_->Shown(e->position) - post_->Position();
        if (rel.HorizontalLength() > scale) return;
        double az = RadToDeg(std::atan2(rel.x, rel.y));
        if (az < 0.0) az += 360.0;
        const double r = rel.HorizontalLength() / scale;
        const double az_tol = RadToDeg(std::atan2(0.7 * kMarkerCaptureFraction, std::max(r, kMarkerCaptureFraction)));
        const std::string why = "a new echo on the IT-45: take it off with the DT-2";
        if (NpcDoes(Cmdr(), SeatDuty::Wheel(Cmdr(), console_.post_marker_azimuth, az, az_tol, true, why))) return;
        NpcDoes(Cmdr(), SeatDuty::Wheel(Cmdr(), console_.post_marker_range, r, 0.7 * kMarkerCaptureFraction, false, why));
        if (!console_.post_marker_take_off.empty()) NpcDoes(Cmdr(), SeatDuty::Control(Cmdr(), console_.post_marker_take_off, 1, why));
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
            c.transponder = t.friendly;
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
            if (i < 2 && p >= C2Network::kPresets) {  // ПЛАВНЫЙ ПОДДИАПАЗОН: the set tunes off its knobs (radio_setup.hpp)
                radio_panel_[i] = p;
                radio_now_[i] = now[i];
                continue;
            }
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
    // Range from the TV picture (roadmap 6.6): the deputy, watching the TV, tells the operator the range he makes of the
    // target's size against the reticle; again whenever it has changed by a kilometre.
    int tv_range_said_km_ = -1;
    void UpdateTvRangeCallout() {
        if (!engagement_on_ || !callouts_) return;
        if (!engagement_.RangeFromTvSize(console_)) {
            tv_range_said_km_ = -1;
            return;
        }
        const int km = static_cast<int>(std::floor(engagement_.TvRangeEstimateM() / 1000.0 + 0.5));
        if (km == tv_range_said_km_) return;
        const std::map<std::string, std::string> words{{"range_km", std::to_string(km)}};
        if (!NpcDoes("deputy_commander", SeatDuty::Report("deputy_commander", "tv_range", "the target's size against the TV reticle gives its range", words))) return;
        tv_range_said_km_ = km;
        Say("tv_range", "deputy_commander", words);
    }

    // Counter-jamming (roadmap 9.4) is switched on only when automatic tracking is impossible and off when the jamming
    // stops (voenizdat_osa_1991 p259): switching it on with no range pull-off seen in the last minute is a crew mistake.
    double last_pulled_s_ = -1e9;
    bool rgpo_was_on_ = false;
    void UpdateCounterJamming() {
        const TrackSensorDef& tr = console_.track;
        if (!console_.has_sensors || tr.rgpo_control.empty()) return;
        if (!tr.range_pulled_input.empty() && console_.equipment.GetInput(tr.range_pulled_input)) last_pulled_s_ = time_;
        const bool on = console_.panel.Get(tr.rgpo_control) == 1;
        if (on && !rgpo_was_on_ && time_ - last_pulled_s_ > 60.0) {
            log_.Record({time_, DebriefEntry::Kind::Deviation, SpeakerName("operator"), tr.rgpo_control,
                         "counter-jamming switched on with no range pull-off seen (only when automatic tracking is impossible)"});
        }
        rgpo_was_on_ = on;
    }

    // Where a prepared position's works are: each vehicle's place at the start, and the dummy positions ("" vehicle).
    struct WorkPlace {
        Vec3 position;
        double heading_deg = 0.0;
        std::string vehicle;  // "own", a battery vehicle's callsign, or "" for a dummy position
    };
    std::vector<WorkPlace> work_places_;
    void RecordWorkPlaces() {
        if (!def_.prepared.present) return;
        work_places_.push_back({def_.unit_position, def_.unit_heading_deg, "own"});
        if (battery_) {
            for (const Battery::Vehicle& v : battery_->Vehicles()) {
                if (!v.def.own) work_places_.push_back({v.position, v.heading_deg, v.def.callsign});
            }
        }
        for (const DummyPositionDef& d : def_.prepared.dummies) work_places_.push_back({d.position, 0.0, std::string()});
    }

    // The compass survey runs while the vehicle stands; its Qc is commanded to the deputy (engagement: SetCommandedQc)
    // once the crew chief can tell him. Moving off spoils the measurement and the halt's orientation (the gyro keeps its
    // own, drifting, heading). As his drill (survey_procedure) the survey runs from when the instrument is set up to
    // when the console's measurement of it is ready; stowed before then, it gives nothing.
    CompassSurvey compass_;
    bool was_stopped_ = false;
    bool halt_oriented_ = false;
    bool survey_drill_ = false;     // the survey drill handed to the NPC crew chief, not yet over
    bool survey_set_up_ = false;    // its instrument stood set up at the last step
    bool qc_untold_ = false;        // he has this halt's Qc and the deputy has not heard it
    bool survey_recalled_ = false;  // signalled back from the aiming circle by the crew inside
    void UpdateCompass() {
        if (!console_.has_orientation) return;
        const OrientationDef& o = console_.orientation;
        const bool stopped = console_.equipment.GetInput("vehicle_stopped");
        if (!stopped && was_stopped_) {
            engagement_.ClearCommandedQc();
            halt_oriented_ = false;
            qc_untold_ = false;
            survey_recalled_ = false;
        }
        was_stopped_ = stopped;
        if (stopped && engagement_.QcKnown() && console_.equipment.GetInput(o.synced_input)) halt_oriented_ = true;
        const bool was_running = compass_.Running();
        bool read = compass_.Step(time_, stopped, def_.unit_heading_deg);
        if (!o.survey_procedure.empty()) {
            const bool up = console_.panel.Get(o.survey_control) == o.survey_up_position;
            if (up && !survey_set_up_ && stopped && !compass_.Running() && !qc_untold_ && !engagement_.QcKnown()) {
                compass_.Start(time_);
                log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(Cmdr()), "orientation",
                             "sets the aiming circle (ПАБ) up and lays it on the vehicle"});
            }
            if (!up && compass_.Running()) {
                compass_.Abort();
                Event("orientation", "the aiming circle was stowed before its reading: no Qc");
            }
            survey_set_up_ = up;
            if (compass_.Running() && console_.equipment.IsReady(o.survey_subsystem)) read = compass_.Finish(def_.unit_heading_deg);
            if (compass_.Running() && SeatHolder(Cmdr()).empty() && !SeatHolds(Cmdr(), o.survey_procedure)) {
                compass_.Abort();  // the NPC called away from it (К машинам, another drill): nobody reads it
                Event("orientation", "the crew chief left the aiming circle before its reading: no Qc");
            }
            if (survey_drill_ && !compass_.Running() && !SeatHolds(Cmdr(), o.survey_procedure)) survey_drill_ = false;
        }
        if (read) {
            qc_untold_ = true;
            char detail[96];
            std::snprintf(detail, sizeof(detail), "Qc by compass %s (%.1f deg)", MilsText(compass_.ResultDeg()).c_str(),
                          compass_.ResultDeg());
            log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(Cmdr()), "orientation", detail});
        } else if (was_running && !compass_.Running() && !stopped) {
            Event("orientation", "the compass survey was broken off: the vehicle moved");
        }
        if (qc_untold_ && QcHeard()) {
            qc_untold_ = false;
            engagement_.SetCommandedQc(compass_.ResultDeg());
            Say("qc_report", Cmdr(), {{"qc", MilsText(compass_.ResultDeg())}});
        }
    }
    // Can the seat the crew chief tells his Qc to (qc_report's) hear him now: both inside, or in earshot outside.
    bool QcHeard() const {
        const CalloutDef* c = def_.crew_data.FindCallout("qc_report");
        if (c == nullptr || def_.crew_data.FindSeat(c->to) == nullptr) return !Outside(Cmdr());
        return InEarshot(Cmdr(), c->to);
    }
    // Is this drill the seat's now, or queued after it (the player's seat or the NPC's)?
    bool SeatHolds(const std::string& station, const std::string& procedure_id) {
        if (const Seat* st = FindSeat(station)) {
            if (st->tracker.GetProcedure().id == procedure_id && !st->tracker.IsComplete()) return true;
            for (const std::string& q : st->queued) {
                if (q == procedure_id) return true;
            }
            return false;
        }
        const CrewMember* m = MemberAt(station);
        return m != nullptr && m->Holds(procedure_id);
    }
    // A target: the NPC crew chief breaks his compass work off and goes back to his post (the crew fights in the gyro's
    // frame as it is). Out at the instrument with it set up, he stows it first (survey_break_procedure).
    void BreakOffSurvey() {
        const OrientationDef& o = console_.orientation;
        compass_.Abort();
        if (survey_drill_) {
            survey_drill_ = false;
            if (CrewMember* m = MemberAt(Cmdr()); m != nullptr && m->Holds(o.survey_procedure)) {
                m->Withdraw();
                const Procedure* stow = console_.FindProcedure(o.survey_break_procedure);
                if (stow != nullptr && console_.panel.Get(o.survey_control) == o.survey_up_position) m->Order(*stow);
            }
        }
        log_.Record({time_, DebriefEntry::Kind::Scenario, "Commander", "orientation", "breaks off the compass work: a target"});
    }
    // Out of the vehicle the crew chief hears neither the intercom nor the reports, and no voice carries to him over the
    // machinery: the first NPC inside who knows of a target signals him back from his hatch, by flag or lamp
    // (voenizdat_osa_1991 pp. 232-239, the crews' signals; who signals and when: sam3d). He sees it out to kSignalSeenM
    // (sam3d: he stands facing the vehicle, 20-50 m off). The signaller's body is not moved to the hatch.
    static constexpr double kSignalSeenM = 100.0;
    std::string RecallSignaller() {
        const CrewMover* m = Mover(Cmdr());
        if (m == nullptr || m->Inside() || std::hypot(m->View(Pose()).feet_m.x, m->View(Pose()).feet_m.y) > kSignalSeenM) return std::string();
        return CrewKnowsHostile();
    }
    // The first NPC inside, other than the crew chief, who knows of a hostile target lately ("" none).
    std::string CrewKnowsHostile() {
        for (const CrewMember& c : crew_) {
            if (c.Role() == Cmdr() || !SeatHolder(c.Role()).empty() || Outside(c.Role())) continue;
            for (const KnownTarget& k : pictures_[c.Role()].Recent(time_, 30.0)) {
                if (!k.friendly && k.has_range) return c.Role();
            }
        }
        return std::string();
    }

    // ---- Transmitting from the vehicle (roadmap 7.8). The commander talks on the set his headset is on, keyed by his chest
    // switch (r123m_training: its tangent to ПРД transmits; ТОН. ВЫЗОВ sends a call tone while transmitting). Nothing goes
    // out unless the set is on, warm and in СИМПЛЕКС and his A-1 is on Р-123. What does go out radiates on the net of the
    // source on that set (the EM layer: the other side's ESM can hear it and find the vehicle).
    int transmissions_ = 0;
    double last_tx_s_ = -1e9;
    double last_tone_s_ = -1e9;
    bool chest_was_keyed_ = false;
    double next_carrier_s_ = 0.0;
    std::string tx_blocked_why_;
    double tx_blocked_s_ = -1e9;
    bool reported_ready_ = false;
    std::map<std::string, std::string> request_blocked_;

    int CommanderSet() const { return c2_ ? c2_->ListeningSet() : 1; }
    bool CanTransmitNow(std::string* why = nullptr) const {
        if (!comms_) return true;
        return comms_->CanTransmit(CommanderSet(), console_, why);
    }
    void TransmitBlocked(const std::string& why) {
        if (why == tx_blocked_why_ && time_ - tx_blocked_s_ < 30.0) return;
        tx_blocked_why_ = why;
        tx_blocked_s_ = time_;
        log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(Cmdr()), "radio", "cannot transmit: " + why});
    }
    // The vehicle on the air for `seconds` on the net of the source on the commander's set.
    void OnAir(double seconds) {
        ++transmissions_;
        last_tx_s_ = time_;
        if (!em_link_ || !c2_) return;
        const std::vector<C2Network::RadioView> sets = c2_->Radios();
        const int set = CommanderSet();
        if (set < 0 || set >= static_cast<int>(sets.size()) || sets[static_cast<std::size_t>(set)].source.empty()) return;
        em_link_->SendVoiceFromUnit(sets[static_cast<std::size_t>(set)].source, time_, time_ + seconds);
    }
    // The commander says a radio callout: it goes out only if his set can transmit (a player has to hold the chest switch
    // at ПРД). False, with the reason in the log, if nothing went out.
    bool SayOnRadio(const std::string& id, const std::map<std::string, std::string>& fields, bool by_player) {
        if (!callouts_) return false;
        const CalloutDef* def = def_.crew_data.FindCallout(id);
        if (def == nullptr) return false;
        std::string why;
        if (by_player && comms_ && comms_->HasChest() && !comms_->ChestKeyed(console_)) {
            why = "the chest switch is at receive (hold it at ПРД to talk)";
        } else {
            CanTransmitNow(&why);
        }
        if (!why.empty()) {
            TransmitBlocked(why);
            if (by_player) last_refusal_ = why;
            return false;
        }
        const double delay = by_player ? 0.0 : SpeakDelayS(Cmdr());
        if (!callouts_->Say(time_, id, Cmdr(), fields, delay)) return false;
        const double said_s = delay + callouts_->LineTimeS(id, Cmdr(), FillCallout(def->ru, fields));
        OnAir(said_s + 0.5);  // keyed a moment before the words and after
        if (by_player) player_words_until_s_ = time_ + said_s + 0.5;
        if (order_net_) OwnSaidOnNet(id, time_ + said_s);
        return true;
    }
    double player_words_until_s_ = -1.0;  // a player's line on the air is out by then (his chest switch may come back)
    // A player at the commander's seat holds the chest switch at ПРД to talk (7.8): the helper shows it before a line on
    // the air (ChestToTalk) and, once his words are out and nothing more is owed on the air, back to ПРМ: held, the set
    // stays on the air and he hears nothing.
    void PlayerChestDuty() {
        if (!comms_ || !comms_->HasChest() || SeatHolder(Cmdr()).empty() || !comms_->ChestKeyed(console_)) return;
        if (time_ < player_words_until_s_) return;
        for (const SeatDuty& d : owed_) {
            if (d.kind == SeatDuty::Kind::Report && d.seat == Cmdr() && OnTheAir(d.id)) return;
        }
        Owe(SeatDuty::Control(Cmdr(), comms_->ChestControl(), 0, "the words said: the chest switch back to ПРМ, to hear the net"));
    }
    bool OnTheAir(const std::string& callout) const {
        const CalloutDef* c = def_.has_crew_data ? def_.crew_data.FindCallout(callout) : nullptr;
        return c != nullptr && c->kind == CalloutKind::Radio;
    }
    // The fields of the vehicle's radio reports: its callsign, the channels the КАНАЛЫ selector allows, missiles loaded.
    std::map<std::string, std::string> RadioFields() const {
        std::map<std::string, std::string> f;
        f["callsign"] = OwnCallsign();
        std::string ch;
        if (console_.has_launcher) {
            const int pos = console_.panel.Get(console_.launcher.channel_control);
            if (pos >= 0 && pos < static_cast<int>(console_.launcher.channel_positions.size())) {
                for (int c : console_.launcher.channel_positions[static_cast<std::size_t>(pos)]) ch += (ch.empty() ? "" : ", ") + std::string(c == 1 ? "I" : "II");
            }
        }
        f["channels"] = ch.empty() ? "-" : ch;
        f["missiles"] = std::to_string(launcher_ ? launcher_->LoadedCount() : 0);
        return f;
    }
    // A request the crew makes by radio (the TZM, the fuel truck) reaches the battery only if the set can transmit.
    bool RequestOnAir(const std::string& what) {
        std::string why;
        if (CanTransmitNow(&why)) {
            request_blocked_.erase(what);
            return true;
        }
        if (request_blocked_[what] != why) {
            request_blocked_[what] = why;
            log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(Cmdr()), "radio",
                         "the request for " + what + " can't go out: " + why});
        }
        return false;
    }
    // The NPC commander reports the vehicle ready on the battery net once at each halt (voenizdat_osa_1991 p246: "к бою
    // готов", with its channels and missiles).
    void NpcReportReady(bool halted) {
        if (LinkedToPost()) return;  // OwnReport does it for a linked post
        if (!halted) {
            reported_ready_ = false;
            return;
        }
        if (reported_ready_ || !(battery_ || c2_) || !def_.has_crew_data) return;
        const CrewTask* ready = def_.crew_data.FindTask("ready_halted");
        if (ready == nullptr || !console_.equipment.AllHold(ready->conditions, console_.panel)) return;
        reported_ready_ = SayOnRadio("report_ready", RadioFields(), false);
    }
    // The chest switch and ТОН. ВЫЗОВ, as the crew works them.
    void UpdateTransmit() {
        if (!comms_) return;
        const bool keyed = comms_->ChestKeyed(console_);
        std::string why;
        const bool can = CanTransmitNow(&why);
        if (keyed && !chest_was_keyed_) {
            if (can) {
                Event("radio", "the commander keys R-123M " + comms_->SetName(CommanderSet()));
            } else {
                TransmitBlocked(why);
            }
        }
        if (keyed && can && time_ + 1e-9 >= next_carrier_s_) {  // held: on the air, second by second
            OnAir(1.0);
            next_carrier_s_ = time_ + 1.0;
        }
        chest_was_keyed_ = keyed;
        const int tone = comms_->TonePressed(console_);
        if (tone < 0) return;
        const std::string set = "R-123M " + comms_->SetName(tone);
        if (tone != CommanderSet()) {
            why = set + " is not the set the commander's headset is on";
        } else if (!can) {
            // why says it
        } else if (comms_->HasChest() && !keyed) {
            why = "not transmitting (the chest switch is at receive)";
        }
        if (!why.empty()) {
            log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(Cmdr()), "radio", "ТОН. ВЫЗОВ on " + set + ": nothing goes out: " + why});
            return;
        }
        Radio("battery net", OwnCallsign(), "ТОН. ВЫЗОВ (call tone)");
        last_tone_s_ = time_;
        log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(Cmdr()), "radio", "ТОН. ВЫЗОВ: a call tone on " + set});
        OnAir(1.0);
    }

    // ---- Tuning the R-123M sets (roadmap 7.8, radio_setup.hpp). The frequency plan: the net of the source on each fixed
    // frequency (c2.presets) on its frequency, or a placeholder plan where a source has none. A set off its net's
    // frequency hears nothing and can't be heard; the antenna mismatch costs on the link.
    std::unique_ptr<RadioTuning> tuning_;
    static double DefaultPlanMhz(int preset) {
        static const double plan[4] = {30.0, 32.5, 41.0, 45.5};  // PLACEHOLDER frequencies where the scenario has none
        return plan[preset >= 0 && preset < 4 ? preset : 0];
    }
    double PlanMhz(int preset) const {
        if (c2_) {
            const std::string& id = c2_->PresetSource(preset + 1);
            for (const C2SourceDef& s : def_.c2_sources) {
                if (s.id == id && s.frequency_hz > 0.0) return s.frequency_hz / 1e6;
            }
        }
        return DefaultPlanMhz(preset);
    }
    // The fixed frequency set `set` is on as the network knows it (0-3), or its panel position without a network.
    int SetPreset(int set) const {
        if (c2_) return c2_->Radios()[static_cast<std::size_t>(set)].preset - 1;
        const CommsRadioDef& r = console_.comms.radios[static_cast<std::size_t>(set)];
        return r.preset.empty() ? 0 : std::min(console_.panel.Get(r.preset), 3);
    }
    bool presets_planned_ = false;
    std::array<std::string, 4> plan_ids_{};
    void StartRadioTuning() {
        if (!comms_) return;
        bool any = false;
        for (const CommsRadioDef& r : console_.comms.radios) any = any || r.HasSetup();
        if (any) tuning_ = std::make_unique<RadioTuning>(console_.comms);
    }
    // The fixators' frequency plan, once the command network has put its sources on the presets (its first update);
    // later, a preset the network re-tunes onto another source (the crew's retune) holds that net's frequency.
    void PlanPresets() {
        if (!presets_planned_) {
            for (int s = 0; s < 2; ++s) {
                std::array<double, 4> mhz{};
                for (int p = 0; p < 4; ++p) mhz[static_cast<std::size_t>(p)] = def_.radio_presets_set ? PlanMhz(p) : 0.0;
                tuning_->SetPresets(s, mhz);
            }
            tuning_->Settle(console_);
            presets_planned_ = true;
        } else if (c2_) {
            for (int p = 0; p < 4; ++p) {
                const std::string& id = c2_->PresetSource(p + 1);
                if (id == plan_ids_[static_cast<std::size_t>(p)] || plan_ids_[static_cast<std::size_t>(p)].empty()) continue;
                for (int s = 0; s < 2; ++s) tuning_->SetPresetFrequency(s, p, PlanMhz(p));
            }
        }
        if (c2_) {
            for (int p = 0; p < 4; ++p) plan_ids_[static_cast<std::size_t>(p)] = c2_->PresetSource(p + 1);
        }
    }
    void UpdateRadioTuning(double dt) {
        if (!tuning_) return;
        PlanPresets();
        NpcTuneRadios(dt);
        const bool moving = !console_.equipment.GetInput("vehicle_stopped");
        const bool keyed[2] = {comms_->Keyed(0, CommanderSet(), console_), comms_->Keyed(1, CommanderSet(), console_)};
        for (const RadioTuning::Event& e : tuning_->Step(time_, dt, console_, moving, keyed)) {
            log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(console_.comms.radios[static_cast<std::size_t>(e.set)].preset.empty()
                                                                               ? std::string("driver")
                                                                               : console_.panel.Find(console_.comms.radios[static_cast<std::size_t>(e.set)].preset)->station),
                         "R-123M " + comms_->SetName(e.set), e.text});
        }
        for (int s = 0; s < 2; ++s) {
            const double plan = PlanMhz(SetPreset(s));
            const bool on = tuning_->OnFrequency(s, plan);
            comms_->SetTuning(s, on, tuning_->OffFrequencyWhy(s, plan), tuning_->LossDb(s));
            const CommsRadioDef& r = console_.comms.radios[static_cast<std::size_t>(s)];
            if (!r.on_frequency_input.empty()) console_.equipment.SetInput(r.on_frequency_input, on);
        }
    }
    // An NPC doing a set's preset drill turns its knobs (the drill's steps are the switches): with a fixator open, the
    // frequency knob to the plan's frequency for that fixed frequency and the sub-band to its band; with the antenna's
    // lock off and the set keyed, the antenna knob to the meter's peak. Rates: SAM3D estimates.
    void NpcTuneRadios(double dt) {
        for (std::size_t s = 0; s < console_.comms.radios.size() && s < 2; ++s) {
            const CommsRadioDef& r = console_.comms.radios[s];
            if (!r.HasSetup()) continue;
            const std::string& seat = console_.panel.Find(r.preset)->station;
            const CrewMember* m = MemberAt(seat);
            if (m == nullptr || !SeatHolder(seat).empty() || !m->IsBusy()) continue;
            const std::string& drill = m->Tracker().GetProcedure().id;
            if (drill.find("preset_setup") == std::string::npos) continue;
            const int pos = console_.panel.Get(r.preset);
            if (pos < 0 || pos >= 4) continue;
            if (console_.panel.Get(r.fixators[static_cast<std::size_t>(pos)]) == 1) {
                const double want = PlanMhz(pos);
                const double now = console_.panel.GetValue(r.frequency);
                const double step = 2.0 * dt;  // MHz a second
                console_.panel.SetValue(r.frequency, std::fabs(want - now) <= step ? want : now + (want > now ? step : -step));
                console_.panel.Set(r.subband, RadioTuning::BandOf(want));
            }
            if (console_.panel.Get(r.antenna_lock) == 1 && comms_->Keyed(static_cast<int>(s), CommanderSet(), console_)) {
                const double want = RadioTuning::AntennaOptimum(tuning_->FrequencyMhz(static_cast<int>(s)));
                const double now = console_.panel.GetValue(r.antenna_tune);
                const double step = 10.0 * dt;  // divisions a second
                console_.panel.SetValue(r.antenna_tune, std::fabs(want - now) <= step ? want : now + (want > now ? step : -step));
            }
        }
    }

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
            crew_calls_.push_back({time_, "launch", engagement_on_ ? engagement_.TrackedTarget() : std::string()});
            log_.Record({time_, DebriefEntry::Kind::Launch, std::string(), console_.launch_button,
                         "missile " + std::to_string(launches_) + from +
                             (engagement_on_ ? std::string() : std::string(" (no engagement model: not resolved)"))});
            if (engagement_on_) {
                const std::string& jam = console_.has_launcher ? console_.launcher.jam_control : std::string();
                engagement_.OnLaunch(time_, launches_, world_, eng_targets_, log_, !jam.empty() && console_.panel.Get(jam) == 1);
                FxLaunch(launches_);
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
        FxUnguided(kFxThrownOffBase + launcher_->EmergencyThrown(), engagement_.UnitHeadingDeg(), console_.launcher.emergency.flight_s);
        if (launcher_->PairSpent()) console_.equipment.Restart(console_.launcher.pair_preparation);
        launcher_->Step(time_, console_);
        console_.equipment.Update(0.0, console_.panel);
    }

    void WriteAcmiFrame() {
        acmi_.WriteWorld(world_);
        acmi_.WriteObject(kUnitAcmiId, def_.unit_position, Vec3{},
                          "Type=Ground+AntiAircraft,Name=" + console_.display_name + ",Color=Red");
        if (engagement_on_) engagement_.WriteAcmi(acmi_, time_, world_, eng_targets_);
        WriteArmsAcmi();
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
        if (em_link_ && em_link_->ArmsFlying()) return;  // an ARM still in the air decides the vehicle's fate
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
        const EmissionRecord& er = emission_record_;
        if (!post_ && er.regimes_heard + er.regimes_told > 0) {  // the debrief's line on the emission regime
            char line[200];
            std::snprintf(line, sizeof(line), "%d heard, %d told the crew; bursts: %.0f s at the halt, radiating %.0f s; %d long, %d reminders",
                          er.regimes_heard, er.regimes_told, er.bursts_s, er.radiating_s, er.breaches, er.reminders);
            log_.Record({time_, DebriefEntry::Kind::Scenario, std::string(), "emission regime", std::string(line) + " (last: " + RegimeText(regime_crew_) + ")"});
        }
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
    std::unique_ptr<EmSensorLink> post_em_;  // the radar cabled to the post, in the EM layer (scenario post.em)
    double post_em_t_ = 0.0;                 // how far the post's EM world has run
    std::vector<EmSensorLink::Strobe> post_strobes_;  // its jamming strobes now
    std::vector<int> rwr_warned_;            // per target: 1 a lock warning noted, 2 a launch warning noted
    std::unique_ptr<Launcher> launcher_;
    bool power_supplied_ = false;
    std::unique_ptr<FuelModel> fuel_;
    // ---- The crew's bodies out of their seats (crew_mobility.hpp).
    std::map<std::string, CrewMover> movers_;
    std::map<std::string, bool> hatches_;           // exit id -> standing open
    mutable std::map<std::string, double> way_s_;    // the drill master's ways: "station|from|to" -> seconds
    std::map<int, Vec3> wire_last_;                  // where each line's layer was last step
    int traffic_side_ = 0;                           // TrafficSide, where it was last looked up
    bool traffic_looked_ = false;
    Vec3 traffic_at_;
    double traffic_heading_rad_ = 0.0;
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
            if (def_.nav) {  // his long walks by the district's foot ways (roadmap G2.4)
                std::shared_ptr<const nav::SessionNav> roads = def_.nav;
                m.SetFootRoute([roads](const Vec3& a, const Vec3& b) {
                    std::vector<Vec3> way;
                    for (const MapPoint& p : roads->FootWay({a.x, a.y}, {b.x, b.y})) way.push_back(Vec3{p.east_m, p.north_m, 0.0});
                    return way;
                });
            }
            // A slower hand is a slower climber (docs/crew/05_SKILL.md): the tier's reaction against a trained man's 1 s.
            if (c.HasSkill()) m.SetPace(Clamp(c.Skill().reaction_s, 0.7, 1.8));
            movers_.emplace(c.Role(), m);
            std::vector<OutsideJob> jobs;
            for (const VehicleJob& j : def_.vehicle.jobs) {
                if (j.station == c.Role()) jobs.push_back({j.control, j.place, j.stay_until});
            }
            c.SetOutsideJobs(std::move(jobs));
        }
        GiveDrillTheWays();
    }
    // The drill master walks a man out to a job and back as the crew does (8.14): each seat's jobs, each way's time
    // measured on a body of that seat at a trained man's pace, and where each man is now.
    void GiveDrillTheWays() {
        if (!drill_ || def_.vehicle.jobs.empty()) return;
        std::map<std::string, std::vector<OutsideJob>> jobs;
        for (const VehicleJob& j : def_.vehicle.jobs) jobs[j.station].push_back({j.control, j.place, j.stay_until});
        drill_->SetOutside(
            std::move(jobs), [this](const std::string& station, const std::string& from, const std::string& to) { return WaySeconds(station, from, to); },
            [this](const std::string& station) -> std::pair<std::string, double> {
                const auto it = movers_.find(station);
                if (it == movers_.end() || (it->second.Inside() && it->second.Arrived())) return {std::string(), 0.0};
                const std::string& a = it->second.At();
                const std::string at = a == "lineup" || def_.vehicle.Place(a) != nullptr || LinePlace(a) != 0 ? a : std::string();
                return {at, it->second.Arrived() ? 0.0 : std::max(0.1, WaySeconds(station, std::string(), at) * 0.5)};
            });
    }
    // The seconds a man of `station` takes from one place to another ("" his seat; a place of the vehicle's, "lineup"),
    // walked on a body of his at a trained man's pace with the hatches as they stand (each way measured once).
    double WaySeconds(const std::string& station, const std::string& from, const std::string& to) const {
        if (from == to) return 0.0;
        const std::string key = station + "|" + from + "|" + to + "|" + std::to_string(traffic_side_);
        const auto hit = way_s_.find(key);
        if (hit != way_s_.end()) return hit->second;
        const auto goal = [this](const std::string& place) { return PlaceGoal(place); };
        CrewMover m(&def_.vehicle, station);
        m.SetKeepOffSide(traffic_side_);
        m.SetSeatTimes(def_.has_crew_data ? def_.crew_data.stand_s : def_.walking.gait.stand_up_s,
                       def_.has_crew_data ? def_.crew_data.sit_s : def_.walking.gait.sit_down_s);
        std::map<std::string, bool> hatches = hatches_;
        const VehiclePose pose = Pose();
        const Terrain* ground = &world_.GetTerrain();
        const auto walk = [&](const std::string& place) {
            double s = 0.0;
            if (!m.GoTo(goal(place), &console_, pose)) return -1.0;
            while (m.Moving() && s < 900.0) {
                m.Update(0.1, pose, ground, map_ground_.get(), def_.walking.gait, hatches);
                s += 0.1;
            }
            return s;
        };
        double s = 0.0;
        if (!from.empty() && walk(from) < 0.0) s = -1.0;
        if (s >= 0.0) s = walk(to);
        if (s < 0.0) return 0.0;  // no way now (a hatch the drill keeps shut): asked again later
        way_s_[key] = s;
        return s;
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
    // Each NPC goes where his drill wants him (CrewMember::WantedPlace); every body walks on; each crew member learns
    // where his body is.
    // Beyond a road's edge, the shoulder and the verge a vehicle pulls onto (sam3d): within this of the edge it stands at
    // the road.
    static constexpr double kRoadsideM = 5.0;
    // The traffic side where the vehicle stands now (TrafficSide), looked up again once it has moved or turned.
    void UpdateTrafficSide() {
        const VehiclePose pose = Pose();
        if (traffic_looked_ && (pose.origin - traffic_at_).HorizontalLength() < 1.0 &&
            std::fabs(VehiclePose::WrapAngle(pose.heading_rad - traffic_heading_rad_)) < 0.05) {
            return;
        }
        traffic_looked_ = true;
        traffic_at_ = pose.origin;
        traffic_heading_rad_ = pose.heading_rad;
        traffic_side_ = 0;
        Vec3 centre;
        double width_m = 0.0;
        if (!map_ground_ || !map_ground_->NearestRoad(pose.origin.x, pose.origin.y, 100.0, centre, width_m)) return;
        if (std::hypot(centre.x - pose.origin.x, centre.y - pose.origin.y) > 0.5 * width_m + kRoadsideM) return;
        // The centre line to its right: that side; on the centre line, its left (the side the traffic passes it on).
        traffic_side_ = pose.ToBody(centre).x > 0.3 ? 1 : -1;
    }
    void UpdateMovers(double dt) {
        if (!def_.has_vehicle) return;
        const VehiclePose pose = Pose();
        const MapTerrain* map = map_ground_.get();
        UpdateTrafficSide();
        for (CrewMember& c : crew_) {
            auto it = movers_.find(c.Role());
            if (it == movers_.end()) continue;
            CrewMover& m = it->second;
            m.SetKeepOffSide(traffic_side_);
            const bool npc = SeatHolder(c.Role()).empty();
            if (npc && !PlayerBodyTracked(c.Role())) {
                const std::string want = c.WantedPlace();
                const bool there = m.At() == want && (!want.empty() || m.Place() == BodyPlace::Seated || m.Moving());
                const auto refused = move_refused_s_.find(c.Role());
                const bool retry = refused == move_refused_s_.end() || time_ - refused->second >= 2.0;
                if (!there && retry) {
                    MoveGoal g = PlaceGoal(want);
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
            PayOut(c.Role(), m, pose);
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
    std::unique_ptr<command::BatteryOrderNet> order_net_;  // battery orders carried by a means (G10.5)
    std::unique_ptr<nav::SessionNav> nav_;                 // the roads, the places and the world's movers (G2.3, G11)

    // ---- The TZM's runs to the regiment's technical battery (roadmap 17.4; campaign/stocks.hpp): the battery commander's
    // order on the net, its chief's run on the session's roads, his report back, and what the commander reckons and
    // believes of the roads (the regiment's word on its net, and what the chief found).
    static constexpr const char* kTzmMobility = "tzm_9t217";  // its class on the roads (data/vehicles/mobility.json)
    static constexpr double kSupplyThinkS = 10.0;             // sam3d: how often the NPC looks at his reckoning
    std::string tzm_id_;                         // the battery's TZM on the order net ("" : no runs in this session)
    nav::Overlay roads_believed_;                // the battery commander's: the bridges he was told are down
    campaign::TechnicalBattery siding_;
    std::unique_ptr<campaign::TzmRun> tzm_run_;  // the last run
    int tzm_run_order_ = -1;                     // the run's order on the net, until the commander has the chief's report
    int tzm_started_for_ = -1;                   // the order the last run set off on
    nav::Overlay tzm_told_;                      // the roads its chief was told of with the order
    Vec3 tzm_home_;                              // where it left the battery
    int tzm_reckoned_ = -1;                      // the commander's reckoning of the missiles on it (-1: no runs)
    std::set<int> supply_orders_;                // the runs' orders on the net
    bool siding_empty_heard_ = false;            // a chief reported the technical battery out of missiles
    std::map<std::string, double> road_open_h_;  // the hour the regiment last said each crossing open again
    double next_supply_think_s_ = 0.0;
    std::size_t tzm_events_seen_ = 0;

    // The technical battery's stocks at the start; the TZM on the order net; the commander's reckoning of its load (what
    // it started with: he knows it).
    void StartSupply() {
        if (!def_.has_supply || !nav_ || !battery_ || !order_net_) return;
        siding_.Init(def_.logistics, def_.siding_tested, def_.siding_untested);
        for (const command::OrderVehicle& c : order_net_->Vehicles()) {
            const Battery::Vehicle* v = battery_->Find(c.vehicle);
            if (v == nullptr || v->def.kind != BatteryVehicleKind::Reload || !tzm_id_.empty()) continue;
            tzm_id_ = c.vehicle;
            tzm_reckoned_ = static_cast<int>(std::lround(v->stock));
        }
    }
    std::string TzmCallsign() const {
        if (order_net_) {
            for (const command::OrderVehicle& c : order_net_->Vehicles()) {
                if (c.vehicle == tzm_id_) return c.callsign;
            }
        }
        return tzm_id_;
    }
    void UpdateSupply() {
        if (tzm_id_.empty()) return;
        siding_.Step(time_);
        const Battery::Vehicle* v = battery_->Find(tzm_id_);
        if (v == nullptr) return;
        // The battery commander by his reckoning (sam3d): less than a launcher's load on the TZM, he sends it for missiles
        // once the battery is ready on its task, not on the march, with the TZM free and a way there on the roads he knows.
        if (time_ >= next_supply_think_s_) {
            next_supply_think_s_ = time_ + kSupplyThinkS;
            const bool short_of = tzm_reckoned_ >= 0 && tzm_reckoned_ < static_cast<int>(def_.logistics.launcher_missiles);
            const bool may = tzm_run_order_ < 0 && !OnMarch() && (!def_.has_task || task_ready_s_ >= 0.0) && v->serving.empty() && !v->away &&
                             !v->broken && !siding_empty_heard_;
            if (short_of && may && PlanTzmRun().found) {
                SeatDuty d = SeatDuty::Order(std::string(), "order_tzm_run", "the TZM short of a launcher's load: to the technical battery for missiles");
                d.keep_s = 120.0;
                // At the post the commander's seat (an NPC, or the player owing it); the battery commander of an Osa's
                // session is no seat of it.
                if (GiverIsOwn() ? NpcDoes(Cmdr(), d) : !shadow_) OrderTzmRun("Battery commander");
            }
        }
        // Its chief heard the order and acts: once the TZM is free of the launchers, he sets off.
        if (tzm_run_order_ >= 0 && tzm_started_for_ != tzm_run_order_ && order_net_->Acting(tzm_run_order_, tzm_id_) && v->serving.empty()) {
            StartTzmRun(*v);
        }
        if (tzm_run_ && tzm_run_order_ >= 0 && tzm_started_for_ == tzm_run_order_ && !tzm_run_->Finished()) StepTzmRun();
    }
    void StartTzmRun(const Battery::Vehicle& v) {
        tzm_started_for_ = tzm_run_order_;
        tzm_run_.reset();
        const nav::Anchor* s = nav_->GetAnchors().Find(def_.supply_siding);
        const nav::Mobility* cls = nav_->MobilityClasses().Find(kTzmMobility);
        nav::Mover* had = nav_->GetMovers().Find(tzm_id_);
        nav::Mover m = had != nullptr ? *had : nav::Mover();
        auto run = std::make_unique<campaign::TzmRun>();
        std::string why = "no way to the technical battery";
        const Vec3 home = v.position;
        if (s == nullptr || cls == nullptr ||
            !run->Start(nav_->Graph(), *cls, def_.logistics, tzm_id_, {home.x, home.y}, s->at, static_cast<int>(std::lround(v.stock)), tzm_told_,
                        light_.dark, time_, m, &why)) {
            order_net_->Cannot(tzm_run_order_, tzm_id_, time_, why, TzmCallsign() + ": на техническую батарею пройти не могу!");
            return;
        }
        m.level = nav::MoverLevel::M1;  // the levels follow the eye from here (hidden at M0 since its last run)
        if (had != nullptr) {
            *had = m;
        } else {
            nav_->AddMover(m, kTzmMobility);
        }
        tzm_run_ = std::move(run);
        tzm_home_ = home;
        tzm_events_seen_ = 0;
        battery_->SetAway(tzm_id_, true, home);
        LogTzmEvents();
    }
    void StepTzmRun() {
        nav::Mover* m = nav_->GetMovers().Find(tzm_id_);
        if (m == nullptr) return;
        tzm_run_->Step(time_, nav_->Graph(), nav_->Truth(), siding_, *m);
        LogTzmEvents();
        const std::string call = TzmCallsign();
        if (!tzm_run_->Finished()) {
            // With no way back he says so once, and waits out there for one; the order stands until he is back.
            if (tzm_run_->Stuck() && !order_net_->CannotSaid(tzm_run_order_, tzm_id_)) {
                order_net_->Cannot(tzm_run_order_, tzm_id_, time_, "no way back to the battery", call + ": вернуться не могу, пути перерезаны. Жду на месте.");
            }
            const MapPoint at = nav::PointAlong(m->route, m->arc_m);
            battery_->SetAway(tzm_id_, true, GroundEnu(at.east_m, at.north_m));  // the net's reach from where it is
            return;
        }
        // Back: the battery's vehicle again, not a mover; his report, by the first way back he has.
        m->level = nav::MoverLevel::M0;
        battery_->SetAway(tzm_id_, false, tzm_home_);
        battery_->SetStock(tzm_id_, tzm_run_->Aboard());
        if (!tzm_run_->CannotWhy().empty() && !order_net_->CannotSaid(tzm_run_order_, tzm_id_)) {
            order_net_->Cannot(tzm_run_order_, tzm_id_, time_, tzm_run_->CannotWhy(),
                               call + ": на техническую батарею пройти не могу, все пути перерезаны. Вернулся.");
            return;
        }
        std::string text = call + (tzm_run_->AtSidingS() >= 0.0 ? ": с технической батареи прибыл" : ": вернулся") + ", ракет " +
                           std::to_string(tzm_run_->Aboard()) + ".";
        if (!tzm_run_->Found().empty()) text += " Мост разрушен, шёл в объезд.";
        if (tzm_run_->SidingEmpty()) text += " На технической батарее ракет больше нет.";
        order_net_->Done(tzm_run_order_, tzm_id_, time_, text);
    }
    void LogTzmEvents() {
        const std::vector<campaign::StockEvent>& ev = tzm_run_->Events();
        for (; tzm_events_seen_ < ev.size(); ++tzm_events_seen_) {
            log_.Record({ev[tzm_events_seen_].t, DebriefEntry::Kind::Scenario, TzmCallsign(), "TZM run", ev[tzm_events_seen_].text});
        }
    }
    // The TZM's chief's report reaches the battery commander: his roads gain the bridges the chief found down on the way
    // (closed at the hour the regiment since said one open again), and back, his reckoning is the load reported and the
    // run is over; out there with no way back, the run stands until he is back.
    void CommanderHearsTzm(const command::OrderEvent& e) {
        if (e.order != tzm_run_order_) return;
        const bool ours = tzm_run_ && tzm_started_for_ == e.order;
        if (ours) {
            const double h = time_ / 3600.0;
            for (const nav::OverlayEntry& f : tzm_run_->Found()) {
                bool known = false;
                for (const nav::OverlayEntry& b : roads_believed_.entries) known = known || (b.id == f.id && h < b.until_h);
                if (known) continue;
                nav::OverlayEntry g = f;
                const auto open = road_open_h_.find(f.crossing);
                if (open != road_open_h_.end() && open->second > f.from_h) g.until_h = std::min(g.until_h, open->second);
                roads_believed_.entries.push_back(g);
            }
            if (!tzm_run_->Finished()) return;
            tzm_reckoned_ = tzm_run_->Aboard();
            if (tzm_run_->SidingEmpty()) siding_empty_heard_ = true;
        }
        tzm_run_order_ = -1;
    }
    // A launcher reported reloaded: the TZM gave it what it lacked by the post's row.
    void TzmGave(const std::string& callsign, int missiles) {
        if (tzm_reckoned_ < 0) return;
        int was = 0;
        for (const LauncherStatus& s : status_boards_) {
            if (s.callsign == callsign) was = std::max(0, s.missiles);
        }
        tzm_reckoned_ = std::max(0, tzm_reckoned_ - std::max(0, missiles - was));
    }
    // The regiment's word on a road's bridge, heard: the commander's roads have it down from now, or open again.
    void HeardRoad(const RegimentMessage& m) {
        const double h = time_ / 3600.0;
        if (m.road_down) {
            for (const nav::OverlayEntry& e : roads_believed_.entries) {
                if (e.id == m.road_entry && h < e.until_h) return;  // said again
            }
            roads_believed_.Cut(m.road_entry, m.road_crossing, h);
            return;
        }
        road_open_h_[m.road_crossing] = h;
        for (nav::OverlayEntry& e : roads_believed_.entries) {
            if (e.crossing == m.road_crossing && h >= e.from_h && h < e.until_h) e.until_h = h;
        }
    }
    nav::MarchPlan march_plan_;                            // the march planned to a place (G2.3)
    bool has_march_plan_ = false;
    std::vector<HeldPaper> own_papers_;                    // the battery's papers in the map cases (G8, G9.4)
    int battery_order_ = -1;                               // the moving order in force, in the net's book
    BatteryOrder::Kind battery_order_kind_ = BatteryOrder::Kind::March;
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
        // The journal takes what came over the post's radios (the air picture with its report: the post's update).
        if (post_ && !console_.post_journal_keeper.empty() && net != "air picture" && NetIdOf(net) != net) {
            bool heard = !voice_;
            for (const std::string& h : heard_by) heard = heard || h == console_.post_journal_keeper;
            if (heard) journal_.Heard(time_, net, from, text);
        }
        radio_.push_back(std::move(m));
        ++radio_total_;
        if (radio_.size() > kRadioKept) radio_.erase(radio_.begin());
    }
    std::size_t radio_total_ = 0;
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
    std::map<std::string, double> recorded_lengths_s_;  // SetRecordedLengths
    std::string recorded_lengths_from_;
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
    int designation_slips_ = 0;                   // passed on with a digit wrong
    // One digit of a designation said wrong: one of the azimuth's four or the range's, to another digit (the azimuth kept
    // on the dial, 0-5999 mils). What the crew is told is what was said; the commander heard it right.
    void SlipDesignation(char (&az)[16], char (&r)[16], KnownTarget& k) {
        const std::string right = std::string(az) + ", " + r;
        std::string digits = std::string(az, 2) + std::string(az + 3, 2) + r;
        const unsigned pick = relay_rng_();
        const std::size_t at = pick % digits.size();
        digits[at] = static_cast<char>('0' + (digits[at] - '0' + 1 + static_cast<int>((pick / 7u) % 9u)) % 10);
        const int mils = std::atoi(digits.substr(0, 4).c_str()) % 6000;
        const int km = std::atoi(digits.substr(4).c_str());
        std::snprintf(az, sizeof(az), "%02d-%02d", mils / 100, mils % 100);
        std::snprintf(r, sizeof(r), "%d", km);
        k.azimuth_deg = mils * 360.0 / 6000.0;
        k.range_m = km * 1000.0;
        ++designation_slips_;
        Event(Cmdr(), "passed target " + std::to_string(k.number) + " on with a digit wrong: " + std::string(az) + ", " + r + " for " + right);
    }

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
        callouts_->SetRecordedLengths(recorded_lengths_s_);
        callouts_->SetPlayerSeat([this](const std::string& station) { return !SeatHolder(station).empty(); });
        if (voice_) {
            callouts_->SetRouter([this](const CalloutDef& d, const std::string& from) { return RouteCallout(d, from); });
            callouts_->SetListens([this](const std::string& station, const std::string& channel) { return Listens(station, channel); });
        }
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
                return (t->perception_s + t->callout_delay_s) * TierStress(*t, crew_[i]);
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
    // A standing order's routine (the search radar's bursts): given without words; its drills (station|procedure) are
    // neither acknowledged nor reported done.
    bool routine_order_ = false;
    std::set<std::string> routine_drills_;
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
        if (!callouts_ || routine_order_) return;
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
        for (const PendingOrder& o : pending_orders_) {  // an order still being shouted to him: the next would replace it
            const CrewMember* c = Crew(o.crew);
            if (c != nullptr && c->Role() == seat) return false;
        }
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
        if (!shadow_) npc_thought_s_ = time_;
        shadow_ = false;
        shadow_breaks_check_ = false;
    }
    double npc_thought_s_ = -1.0;  // when the NPC commander last thought (NpcSurvey decides with him)
    bool shadow_breaks_check_ = false;  // the shadow's next orders count the deputy's function check as broken off
    // The deputy's NPC in the function check (its drill, РОД РАБОТЫ at ФК).
    bool InFunctionCheck(const std::string& seat) const {
        const CrewMember* m = MemberAtConst(seat);
        return m != nullptr && SeatHolder(seat).empty() && m->IsBusy() && m->Tracker().GetProcedure().id == "function_check";
    }
    void BreakOffFunctionCheck(const std::string& seat) {
        CrewMember* m = MemberAt(seat);
        if (m == nullptr || !InFunctionCheck(seat)) return;
        m->CancelOrder();
        console_.panel.Set("fo.mode", 1);
    }
    void ThinkAsCommander() {
        npc_next_think_s_ = time_ + 1.0;
        // A stop while the battery forms its column or marches is not a halt: nobody deploys for it.
        const bool column = battery_ && def_.battery_drive_own &&
                            (battery_->CurrentMode() == Battery::Mode::Forming || battery_->CurrentMode() == Battery::Mode::Marching);
        const bool halted = console_.equipment.GetInput("vehicle_stopped") && !column;
        const bool stowing = (npc_stow_ordered_ && !npc_resumed_) || (npc_arm_stow_ordered_ && !arm_relocated_) || OwnStowing();
        if (engagement_on_ && engagement_.Method() != npc_track_method_) {
            npc_track_method_ = engagement_.Method();
            npc_track_since_s_ = time_;
        }
        // The ARM drill (voenizdat_osa_1991 p262): a sign of an ARM launch heard and nothing of ours in the air (a missile
        // in flight is guided to its burst first): every transmitter off at once, «Излучение выключить!», and the target
        // let go. Radiation comes back on through the EMCON path when the silence is over (arm_silence_s).
        // Until the silence is over he sees to it: a drill that would radiate (a capture under way) is stopped, and a
        // transmitter found on again is ordered off again.
        // One order where one covers it (docs/crew/15_TALK.md section 3): «Излучение выключить!» is the task "silent",
        // both transmitters off (it starts the operator's ssc_tx_off beside the senior operator's stop_search), so no
        // «Высокое ССЦ выключить!» beside it; the target is let go by «Цель сбросить!» only once the transmitters are off
        // and a track is still held (by the TV), a moment later, not in the same breath.
        if (ArmSilent()) {
            const bool radiating = console_.panel.Get("soc.hv") == 1 || console_.panel.Get("soc.search") == 1 || console_.panel.Get("ssc.tx") == 1;
            if (time_ - npc_arm_off_s_ >= 10.0) {
                bool acted = false;
                for (CrewMember& m : crew_) {
                    const std::string d = m.Tracker().GetProcedure().id;
                    if (!shadow_ && SeatHolder(m.Role()).empty() && m.IsBusy() &&
                        (d == "capture_with_ssc" || d == "ready_capture" || d == "azimuth_onto_target" || d == "hv_on" || d == "acquire_with_soc" ||
                         d == "fire_at_jammer")) {
                        m.CancelOrder();
                        acted = true;
                    }
                }
                if (radiating) {
                    GiveCrewOrder("order_emcon_off");
                    npc_ordered_["order"] = {"order_emcon_off", time_};
                    acted = true;
                }
                if (acted) {
                    npc_arm_off_s_ = time_;
                    Event(Cmdr(), "radiation off against the ARM");
                    // An engagement order of his still waiting its turn on the intercom is overtaken: he doesn't say it
                    // after «Излучение выключить!» (its drills were cancelled above).
                    if (!shadow_ && callouts_ && SeatHolder(Cmdr()).empty()) {
                        callouts_->Withdraw(Cmdr(), {"order_take_target", "order_take_contact", "order_prepare", "order_jammer"});
                    }
                }
            }
            if (!radiating && engagement_on_ && engagement_.Method() != TrackMethod::None && time_ - npc_arm_off_s_ >= 3.0 &&
                time_ - npc_arm_drop_s_ >= 10.0) {
                GiveCrewOrder("order_drop_target");
                npc_arm_drop_s_ = time_;
                Event(Cmdr(), "the transmitters off, the target let go against the ARM");
            }
        }
        // Under the threat of ARMs, the tracking transmitter off after the missile's burst (p262).
        if (npc_ssc_off_due_ && engagement_on_ && !engagement_.ShotsInFlight() && engagement_.Method() == TrackMethod::None) {
            npc_ssc_off_due_ = false;
            if (console_.panel.Get("ssc.tx") == 1) GiveCrewOrder("order_ssc_off");
        }
        // Readiness: the next drill of each idle seat toward the task.
        // Emission control (the battery's EMCON, an ambush not yet sprung): ready with the radars silent, the search
        // transmitter off; when it lifts, radiation on again.
        const bool silent = NpcSilent();
        if (silent && halted && console_.equipment.IsReady("soc_transmitter")) NpcGive("order_emcon_off", 30.0);
        if (!silent && npc_was_silent_) NpcGive("order_emcon_on", 30.0);
        npc_was_silent_ = silent;
        // The emission regime: the crew told the one he heard (before they deploy, or as soon as it changes), and the
        // radar seats reminded of its bursts.
        NpcPassOnRegime();
        NpcRemindRegime(halted);
        // Emission discipline (the limited regime): between bursts the transmitter off, the antenna turning; the
        // readiness drills then aim at the silent state, so they don't switch it back on or stop the antenna.
        const bool burst_off = !silent && NpcBurstOff(halted);
        if (regime_crew_.kind == EmissionKind::Limited && halted && !silent && console_.equipment.IsReady("hv_ready")) {
            // The first burst is ordered aloud; after it the senior operator keeps the rhythm as a standing order, without
            // a word each way (sam3d: seven callouts every 35 s kept the intercom talking without end).
            routine_order_ = npc_bursts_said_;
            bool given = false;
            // Not while an NPC senior operator is at a drill (his ЗАПРОС between bursts, QueryBurst): the order would cut it.
            const CrewMember* so = MemberAt("senior_operator");
            const bool so_busy = so != nullptr && SeatHolder("senior_operator").empty() && so->IsBusy();
            if (burst_off && console_.panel.Get("soc.hv") == 1 && !so_busy) given = NpcBurstOrder("order_hv_off", "the limited regime: ВЫСОКОЕ off between bursts");
            bool hv_on_owed = false;  // the shadow's, from the burst's start until the player gives it
            for (const SeatDuty& d : owed_) hv_on_owed = hv_on_owed || (d.kind == SeatDuty::Kind::Order && d.id == "order_hv_on");
            if (!burst_off && (npc_burst_off_ || (shadow_ && hv_on_owed)) && console_.panel.Get("soc.hv") == 0) {
                given = NpcBurstOrder("order_hv_on", "the limited regime: ВЫСОКОЕ on for a burst") || given;
            }
            routine_order_ = false;
            if (given && !shadow_) npc_bursts_said_ = true;
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
            // While the search runs in bursts its transmitter is the bursts', not the readiness drills': under the least
            // emission the senior operator works it himself (ArmEmission); under the limited regime, between bursts, the
            // orders above switch it off (sam3d: the plan, made the moment ВЫСОКОЕ went off and before the transmitter
            // did, stopped the search). In a burst the plan still puts it on if the order to was missed.
            const bool own_bursts = halted && !silent && console_.panel.Get("soc.search") == 1 &&
                                    (regime_crew_.kind == EmissionKind::Arm || burst_off);
            for (const DrillPlan::Seat& s : plan.seats) {
                if (!SeatFree(s.station) || (own_bursts && s.station == "senior_operator")) continue;
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
        NpcReportReady(halted);
        // A radar to cable to the post at this position: «Кабель к РЛС проложить!», an order of its own (most positions
        // have none, and the deployment's order would wait on a radar that isn't there).
        if (halted && !console_.post_radar_here.empty() && console_.equipment.GetInput(console_.post_radar_here)) {
            const CrewTask* cabled = def_.crew_data.FindTask("radar_cabled");
            if (cabled != nullptr && !console_.equipment.AllHold(cabled->conditions, console_.panel)) NpcGive("order_radar_cable", 120.0);
        }
        // Lines to lay at this position, once the post is deployed (ru_pu12_text: deployed in 18 min, in 25 with four
        // lines laid to 500 m): «Линии проложить!», each line's man paying its cable out to the far end.
        if (halted && AnyLineToLay()) {
            const CrewTask* deployed = def_.crew_data.FindTask("ready_halted");
            if (deployed == nullptr || console_.equipment.AllHold(deployed->conditions, console_.panel)) NpcGive("order_lay_lines", 120.0);
        }
        if (halted && !stowing) NpcRegimentLink();
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
                    if (!shadow_) BatteryTakes(o);
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
        NpcOrient();
        NpcCounterJamming();
        if (NpcBetweenFights()) return;
        // Silent against an ARM: no engagement order until the silence is over (a new target, method Н or a launch would
        // put a transmitter on again); the ARM block above has the transmitters off and then the target let go.
        if (ArmSilent()) return;
        // The engagement, by the crew data's orders (order_take_target, order_prepare, fire_permitted, order_jammer,
        // order_low_target): the seats and drills each order starts are data, not code.
        const EngagementModel& eng = engagement_;
        const TrackMethod m = eng.Method();
        const std::string tracked = eng.TrackedTarget();
        NpcReleaseStaleDrills(m);
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
                    decoys_known_.count(k.target) == 0 && reported_kills_.count(k.target) == 0 && arms_heard_.count(k.target) == 0) {
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
            // The search radar has lost a low target he gave, but НЛЦ is lit under the azimuth line (the deputy's
            // "НЛЦ!"): the TV looks along the line.
            if (known.empty() && !assigned_target_.empty() && console_.equipment.GetInput("target_too_low")) {
                NpcGive("order_low_target", 30.0);
                return;
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
            if (k.range_m > std::min(reach, TrackOnRangeM(k.target)) || !console_.equipment.IsReady("hv_ready")) {
                NpcIdentify();  // meanwhile: who is it?
                return;
            }
            // A target in reach interrupts the function check (the deputy's РОД РАБОТЫ goes back with the next drills); in
            // a player's place the order that does it is his (GiveCrewOrder breaks the check off).
            if (!shadow_) BreakOffFunctionCheck("deputy_commander");
            shadow_breaks_check_ = shadow_;
            NpcAssign(k.target);
            // НЛЦ with the azimuth line on it (the deputy's "НЛЦ!"): the radar can't take it, so the angles by the TV
            // (track_low_target: samsim_osa p25-26), the range then by ПОЛУАВТОМАТ (below).
            if (console_.equipment.GetInput("target_too_low") && NpcGive("order_low_target", 30.0)) return;
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
        // The TV holds it with the tracking radar up: the operator ranges on it with the radar alone (ПОЛУАВТОМАТ,
        // samsim_osa p26: while the deputy tracks the angles with the TV).
        const TrackSensorDef& tr = console_.track;
        if (m == TrackMethod::Tv && !tr.range_only_mode.empty() && !console_.equipment.IsReady(tr.range_only_mode) &&
            console_.equipment.IsReady(tr.transmitter)) {
            NpcGive("order_semi_auto", 30.0);
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
    // Emission discipline (the limited regime, as told to the crew): halted, nothing tracked and no hostile he knows of
    // within the tracking radar's reach, the search radar radiates in bursts, on for its turns and off for its turns.
    bool NpcBurstOff(bool halted) {
        if (regime_crew_.kind != EmissionKind::Limited || !BurstsApply(halted)) return false;  // arm: the senior operator's own (ArmEmission)
        const double turn = SearchTurnS();
        const double on = regime_crew_.OnS(turn);
        return std::fmod(time_, on + regime_crew_.OffS(turn)) >= on;
    }
    // A burst's order. The shadow commander owes it the player at every burst: what the player said a burst ago doesn't
    // count for this one (ShadowOrder's recall, kShadowRecallS, is longer than a cycle).
    bool NpcBurstOrder(const std::string& order_id, const char* why) {
        if (!shadow_) return NpcGive(order_id, 1.0);
        NpcDoes(Cmdr(), SeatDuty::Order(Cmdr(), order_id, why));
        return false;
    }
    std::string npc_tv_cleared_;
    std::string npc_decoy_dropped_;
    double npc_decoy_dropped_s_ = -1e9;
    std::set<std::string> decoys_known_;
    bool npc_burst_off_ = false;
    // A drill bound to a track (the jammer's method Н, the TV's engagement, the launch) left waiting with nothing tracked
    // and nothing in the air: after 10 s he lets the seat go, so the next target can be taken (sam3d: the deputy waited
    // on a lost jammer's missile, or on a target lost in the haze, for the rest of the raid, and no "take the target"
    // got through to a busy seat).
    double npc_stale_since_s_ = -1.0;
    void NpcReleaseStaleDrills(TrackMethod m) {
        if (shadow_) return;
        CrewMember* dep = MemberAt("deputy_commander");
        const std::string id = dep != nullptr && dep->IsBusy() ? dep->Tracker().GetProcedure().id : std::string();
        const bool bound = id == "fire_at_jammer" || id == "emcon_tv_engagement" || id == "track_low_target" || id == "launch" ||
                           id == "prepare_firing";
        if (!bound || m != TrackMethod::None || engagement_.ShotsInFlight() || !SeatHolder("deputy_commander").empty() ||
            dep->Status() != CrewStatus::Waiting) {
            npc_stale_since_s_ = -1.0;
            return;
        }
        if (npc_stale_since_s_ < 0.0) npc_stale_since_s_ = time_;
        if (time_ - npc_stale_since_s_ < 10.0) return;
        dep->CancelOrder();
        npc_stale_since_s_ = -1.0;
        npc_permit_s_ = -1e9;
        Event(Cmdr(), "lets the deputy's " + id + " go: nothing tracked");
    }
    bool npc_bursts_said_ = false;  // the burst rhythm has been ordered aloud once

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
    // ---- The ARM drill (roadmap E6; voenizdat_osa_1991 pp262-264). The senior operator watches his scope for the signs
    // of an anti-radiation missile's launch: a smaller mark coming at the vehicle much faster than any aircraft, at a
    // crossing parameter near zero (the ARM itself); and, warned of ARMs (doctrine arm_threat), a target that came at the
    // battery at zero parameter turning away out at stand-off range (its launch made). A mark's motion is what his scope
    // showed over its last looks, never the truth. His tier may not know the sign (a "basic" tier: the standard drills
    // only) or miss it (its miss chance, more under stress). The commander acts on hearing it (Hear), not on the truth.
    struct ScopeTrail {
        std::vector<std::pair<double, Vec3>> marks;  // when and where (the crew's frame, the vehicle at the origin)
        double last_seen_s = 0.0;
        bool heading_in = false;
        bool called = false;
    };
    std::map<std::string, ScopeTrail> scope_trails_;
    std::set<std::string> arms_known_;  // marks the senior operator has called ARMs: not targets
    std::set<std::string> arms_heard_;  // and the commander has heard called so
    double arm_sign_s_ = -1e9;          // when the commander last heard a sign of an ARM launch
    double arm_seen_s_ = -1e9;          // when the senior operator last called one
    double arm_reported_s_ = -1e9;
    std::string arm_sign_az_;           // the azimuth of the last sign he heard (his report says it)
    bool arm_attacked_ = false;
    bool arm_relocated_ = false;
    bool npc_arm_stow_ordered_ = false;
    double npc_arm_off_s_ = -1e9;
    double npc_arm_drop_s_ = -1e9;
    bool npc_ssc_off_due_ = false;
    TrackMethod npc_track_method_ = TrackMethod::None;
    double npc_track_since_s_ = 0.0;
    static constexpr double kArmMarkSpeedMps = 450.0;  // sam3d: faster than the aircraft the battery meets (ARMs 600-1000 m/s)
    static constexpr double kZeroParameterCos = 0.9;   // the mark's motion within about 25 degrees of straight at the vehicle
    static constexpr double kTurnAwayMinRangeM = 8000.0;  // a turn out at stand-off range, not a target passing overhead
    static constexpr double kTurnWindowS = 10.0;       // the marks a turn is judged by (across a gap between bursts)
    static constexpr double kSameBlipM = 2500.0;       // a new mark this near where a lost one was heading is the same blip
    static constexpr int kArmBeamProgram = 4;          // ЛУЧ АВТ. I–II (data/procedures/osa_ak.json soc.beam)
    static constexpr double kStableTrackS = 5.0;       // a lock held this long is the textbook's "устойчивое сопровождение"

    // ---- The emission regime (emission_regime.hpp, docs/crew/16_EMISSION.md): who knows which, and how it is passed down.
    // The Osa: regime_cmdr_ is what its commander heard (the combat order before the mission, then the battery net),
    // regime_crew_ what he has told his crew. The post: regime_cmdr_ is what its battery commander has ordered.
    EmissionRegime regime_cmdr_;
    EmissionRegime regime_crew_;
    double regime_heard_s_ = -1e9;           // when the commander heard the regime he hasn't told his crew yet
    bool regime_from_combat_order_ = false;  // a battery order of the regime at 0 s: the combat order, known before deploying
    int battery_regime_said_ = 0;            // an Osa's own battery: its regime orders already said on the net
    int battery_regime_tries_ = 0;
    double battery_regime_next_s_ = -1.0;
    std::size_t next_emission_order_ = 0;
    EmissionRegime regiment_regime_;         // the regiment's order: being said on its net (the post), or heard (an Osa's battery)
    double regiment_regime_s_ = -1.0;        // an Osa's battery: heard at, until passed on (< 0: none)
    int regiment_regime_tries_ = 0;          // the post: the regiment says it until the post hears it
    double regiment_regime_next_s_ = -1.0;
    EmissionRegime pending_regime_;          // the post: a regime the battery commander owes the battery (the regiment's, an ARM attack)
    double pending_since_s_ = -1.0;
    double pending_delay_s_ = 0.0;
    std::string pending_why_;
    double arm_report_heard_s_ = -1.0;       // the battery commander heard a launcher's report of an ARM attack (< 0: none)
    std::vector<EmissionRegime> battery_emission_orders_;  // the post: for a linked Osa
    std::vector<std::string> arm_reports_out_;             // the Osa: for a linked post
    double regime_hostile_s_ = -1e9;         // the commander last knew of a hostile (a reminder after a quiet spell)
    double regime_reminded_s_ = -1e9;
    double hv_on_since_s_ = -1.0;            // the search transmitter on since (a burst running long)
    double ssc_rule_due_s_ = -1.0;           // the operator to be told the tracking transmitter's rule (the least emission)
    EmissionRecord emission_record_;
    static constexpr double kBurstGraceS = 3.0;  // sam3d: the drill's own seconds before a long burst counts as one

    static std::string BatteryEmissionOrderId(EmissionKind k) { return std::string("order_battery_emission_") + EmissionKindName(k); }
    // The regime as said on a net: the talk table's group (with the bursts for limited and arm), else its words.
    std::string EmissionGroup(const EmissionRegime& r) const {
        const TalkEntry* e = def_.talk_tables.ByEffect(EmissionEffect(r.kind));
        std::string g = e != nullptr ? e->code : std::string("режим излучения ") + def_.emission.Of(r.kind).ru;
        if (r.Bursts()) {
            const std::map<std::string, std::string> w = EmissionWords(def_.emission, r, 60.0 / 33.0);
            g += ", " + w.at("on") + " через " + w.at("off");
        }
        return g;
    }
    // The regime in English, for the log and the debrief.
    std::string RegimeText(const EmissionRegime& r) {
        std::string t = def_.emission.Of(r.kind).en;
        if (r.Bursts()) t += " (" + EmissionWords(def_.emission, r, SearchTurnS()).at("bursts_en") + ")";
        if (!r.from.empty()) t += r.from == "regiment" ? ", the regiment's order" : ", the battery commander's";
        return t;
    }
    // A commander who tells his crew: an NPC, or a player. Without one (a script stands in for him, or no crew data) the
    // crew goes by what he knows.
    bool CommanderTellsCrew() const { return def_.has_crew_data && (NpcCommanderActive() || !SeatHolder(Cmdr()).empty()); }
    // The regime the mission starts under (the combat order's: the doctrine), and the battery's own commander holding it.
    void StartEmission() {
        regime_cmdr_ = def_.doctrine.emission;
        if (regime_cmdr_.kind != EmissionKind::Free) emission_record_.regimes_heard = 1;
        if (!CommanderTellsCrew() || post_) regime_crew_ = regime_cmdr_;
        if (battery_ && !LinkedToPost()) {
            BatteryOrderRegime(regime_cmdr_);
            battery_regime_said_ = battery_->RegimeOrders();
        }
    }
    void BatteryOrderRegime(const EmissionRegime& r) {
        BatteryOrder o;
        o.kind = BatteryOrder::Kind::Emission;
        o.regime = r;
        o.at_s = time_;
        battery_->Order(o, time_);
    }
    void CommanderHearsRegime(const EmissionRegime& r, const std::string& how) {
        if (r.Same(regime_cmdr_)) return;
        regime_cmdr_ = r;
        regime_heard_s_ = how == "the combat order" ? -1e9 : time_;
        ++emission_record_.regimes_heard;
        Event(Cmdr(), "hears the emission regime by " + how + ": " + RegimeText(r));
        log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(Cmdr()), how, "the emission regime: " + RegimeText(r)});
        if (!CommanderTellsCrew()) regime_crew_ = r;
    }

    void UpdateEmission(double dt) {
        if (post_) {
            UpdatePostEmission();
        } else {
            UpdateOsaBatteryEmission();
            RecordEmission(dt);
        }
    }
    // An Osa whose battery commander is the battery's own NPC (not a linked post): the regiment's orders reach him and he
    // passes them on after relay_s; a launcher's report of an ARM attack puts the battery under the least emission
    // (voenizdat_osa_1991 p243, p262); every regime he orders is said on the battery net, again until the Osa hears it.
    // One in force at the start is the combat order's, known before the vehicles deploy.
    void UpdateOsaBatteryEmission() {
        if (!battery_ || LinkedToPost()) return;
        const bool npc = def_.battery_commander == "npc";
        while (next_emission_order_ < def_.emission_orders.size() && def_.emission_orders[next_emission_order_].at_s <= time_ + 1e-9) {
            const EmissionRegime& r = def_.emission_orders[next_emission_order_++].regime;
            if (r.from == "regiment" && regiment_net_) {  // to the battery commander on the regiment's net first (G10.7)
                BatteryOrder o;
                o.kind = BatteryOrder::Kind::Emission;
                o.regime = r;
                o.from = "regiment";
                o.at_s = time_;
                RegimentGives(o);
            } else if (r.from == "regiment") {
                regiment_regime_ = r;
                regiment_regime_s_ = time_;
                Event("regiment", "orders the battery's emission regime: " + RegimeText(r));
                log_.Record({time_, DebriefEntry::Kind::Scenario, "Regiment", "battery commander", "the emission regime: " + RegimeText(r)});
            } else {
                BatteryOrderRegime(r);
            }
        }
        if (regiment_regime_s_ >= 0.0 && (!npc || time_ - regiment_regime_s_ >= def_.emission.relay_s)) {
            regiment_regime_s_ = -1.0;
            BatteryOrderRegime(regiment_regime_);
        }
        if (arm_report_heard_s_ >= 0.0 && time_ - arm_report_heard_s_ >= def_.emission.arm_on_report_s) {
            arm_report_heard_s_ = -1.0;
            const EmissionKind k = battery_->Regime().kind;
            if (npc && k != EmissionKind::Arm && k != EmissionKind::Silent) {
                Event("Battery commander", "a launcher reported an ARM attack: the battery under the least emission");
                BatteryOrderRegime(def_.emission.Make(EmissionKind::Arm, "battery"));
            }
        }
        if (battery_->RegimeOrders() != battery_regime_said_) {
            battery_regime_said_ = battery_->RegimeOrders();
            battery_regime_tries_ = 0;
            battery_regime_next_s_ = time_;
            if (regime_from_combat_order_) {
                battery_regime_next_s_ = -1.0;
                CommanderHearsRegime(battery_->Regime(), "the combat order");
            }
        }
        regime_from_combat_order_ = false;
        if (battery_regime_next_s_ < 0.0 || time_ + 1e-9 < battery_regime_next_s_) return;
        const EmissionRegime& r = battery_->Regime();
        ++battery_regime_tries_;
        if (order_net_) {  // an order on the net like any other: said again to a silence, then the next means (G10.7)
            battery_regime_next_s_ = -1.0;
            RegimeOnOrderNet(r);
            return;
        }
        if (c2_ == nullptr || c2_->Hearing()) {
            battery_regime_next_s_ = -1.0;
            Radio("battery net", "10", "Всем, я 10: " + EmissionGroup(r) + ". " + EmissionGroup(r) + ".");
            if (const TalkEntry* e = def_.talk_tables.ByEffect(EmissionEffect(r.kind))) HeardGroup(e->code, "battery", BatteryNetHearers());
            CommanderHearsRegime(r, "the battery net");
        } else if (battery_regime_tries_ > def_.emission.repeats) {
            battery_regime_next_s_ = -1.0;
            Event("battery net", "the battery commander's order of the emission regime never got through");
        } else {
            battery_regime_next_s_ = time_ + def_.emission.repeat_s;
            Event("battery net", "the order of the emission regime was not heard: the battery commander says it again later");
        }
    }
    // The post: the regiment's order comes on its command net (heard on the R-111 and decoded, else said again); the
    // battery commander passes it on after relay_s, and puts the battery under the least emission when a launcher reports
    // an ARM attack. An NPC does it; a player decides (the helper shows it as his duty until he orders a regime).
    void UpdatePostEmission() {
        while (next_emission_order_ < def_.emission_orders.size() && def_.emission_orders[next_emission_order_].at_s <= time_ + 1e-9) {
            const EmissionRegime& r = def_.emission_orders[next_emission_order_++].regime;
            if (r.from == "regiment") {
                regiment_regime_ = r;
                regiment_regime_tries_ = 0;
                regiment_regime_next_s_ = time_;
            } else {
                OweBatteryRegime(r, 0.0, "the battery commander's decision");
            }
        }
        if (regiment_regime_next_s_ >= 0.0 && time_ + 1e-9 >= regiment_regime_next_s_) {
            ++regiment_regime_tries_;
            const RadioNetDef* rn = def_.radio_data.Find("regiment");
            const std::vector<std::string> ears = NetHearers("regiment");
            if (rn != nullptr && !ears.empty()) {
                regiment_regime_next_s_ = -1.0;
                const TalkEntry* e = def_.talk_tables.ByEffect(EmissionEffect(regiment_regime_.kind));
                const std::string g = EmissionGroup(regiment_regime_);
                Radio("regiment net", rn->main_call_sign, "Всем, я " + rn->main_call_sign + ": " + g + ". " + g + ".", false, ears);
                if (e != nullptr) HeardGroup(e->code, "regiment", ears);
                DecodeHeard(ears, e);
                log_.Record({time_, DebriefEntry::Kind::Scenario, "Regiment", "regiment net", "the emission regime: " + RegimeText(regiment_regime_)});
                OweBatteryRegime(regiment_regime_, def_.emission.relay_s, "the regiment's order of the emission regime, passed on to the battery");
            } else if (regiment_regime_tries_ > def_.emission.repeats) {
                regiment_regime_next_s_ = -1.0;
                Event("regiment net", "the regiment's order of the emission regime never got through");
            } else {
                regiment_regime_next_s_ = time_ + def_.emission.repeat_s;
                Event("regiment net", "the regiment's order of the emission regime was not heard on the R-111");
            }
        }
        if (arm_report_heard_s_ >= 0.0 && time_ - arm_report_heard_s_ >= def_.emission.arm_on_report_s) {
            arm_report_heard_s_ = -1.0;
            if (regime_cmdr_.kind != EmissionKind::Arm && regime_cmdr_.kind != EmissionKind::Silent) {
                OweBatteryRegime(def_.emission.Make(EmissionKind::Arm, "battery"), 0.0, "a launcher reports an ARM attack: the least emission");
            }
        }
        if (pending_since_s_ < 0.0 || time_ - pending_since_s_ < pending_delay_s_ || MarchRadioBan()) return;
        if (pending_regime_.Same(regime_cmdr_)) {
            pending_since_s_ = -1.0;
            return;
        }
        bool done = false;
        if (!def_.has_crew_data) {
            done = OrderBatteryEmission(pending_regime_, "Battery commander");
        } else if (NpcDoes(Cmdr(), SeatDuty::Order(Cmdr(), BatteryEmissionOrderId(pending_regime_.kind), pending_why_))) {
            done = GiveCrewOrder(BatteryEmissionOrderId(pending_regime_.kind));
        } else {
            return;  // a player's to decide: the helper shows it until he orders a regime
        }
        if (!done) pending_delay_s_ = time_ - pending_since_s_ + 5.0;  // no way on the net yet: he tries again in a while
    }
    void OweBatteryRegime(const EmissionRegime& r, double delay_s, const std::string& why) {
        pending_regime_ = r;
        pending_since_s_ = time_;
        pending_delay_s_ = delay_s;
        pending_why_ = why;
    }
    // The battery commander says a regime on the battery net: a circular command said twice, no receipt (rrs_1986).
    bool OrderBatteryEmission(const EmissionRegime& r, const std::string& by) {
        if (!post_) return false;
        const std::string why = CanTalkOnNet(Cmdr(), "battery");
        if (!why.empty()) {
            last_refusal_ = why;
            return false;
        }
        const std::string g = EmissionGroup(r);
        Radio("battery net", by, "Всем, я " + OwnCallSignOn("battery") + ": " + g + ". " + g + ".", false, BatteryNetHearers());
        Transmitted("battery", by, 4.0);
        regime_cmdr_ = r;
        regime_crew_ = r;
        battery_emission_orders_.push_back(r);
        pending_since_s_ = -1.0;
        if (battery_) BatteryOrderRegime(r);
        log_.Record({time_, DebriefEntry::Kind::Scenario, by, "battery net", "the emission regime: " + RegimeText(r)});
        return true;
    }

    // The Osa's commander tells his crew the regime he heard (order_emission_regime): before they deploy (the combat
    // order's) or pass_on_s after he hears a new one. A silence his «Излучение выключить!» already said needs no more
    // words, nor does radiation lifted back to free by «Излучение включить!» (EmconOrderTells).
    void NpcPassOnRegime() {
        if (post_ || regime_crew_.Same(regime_cmdr_) || time_ - regime_heard_s_ < def_.emission.pass_on_s) return;
        if (NpcDoes(Cmdr(), SeatDuty::Order(Cmdr(), "order_emission_regime", "the battery's emission regime, told to the crew"))) {
            GiveCrewOrder("order_emission_regime");
        }
    }
    // «Излучение выключить!» under radiation forbidden tells the crew the regime, and «Излучение включить!» lifting it
    // back to free does too: no words of the regime besides.
    void EmconOrderTells(const std::string& order_id) {
        const bool off = order_id == "order_emcon_off" && regime_cmdr_.kind == EmissionKind::Silent;
        const bool on = order_id == "order_emcon_on" && regime_cmdr_.kind == EmissionKind::Free && regime_crew_.kind == EmissionKind::Silent;
        if ((!off && !on) || post_ || regime_crew_.Same(regime_cmdr_)) return;
        regime_crew_ = regime_cmdr_;
        ++emission_record_.regimes_told;
        emission_record_.told_after_s = regime_heard_s_ > -1e8 ? time_ - regime_heard_s_ : 0.0;
        Event(Cmdr(), "tells the crew the emission regime by his order: " + RegimeText(regime_cmdr_));
    }
    // The bursts apply now: at the halt with nothing tracked, under a burst regime the crew knows (limited: and no hostile
    // the commander knows of within the tracking radar's reach).
    bool BurstsApply(bool halted) {
        if (!regime_crew_.Bursts() || !halted || engagement_.Method() != TrackMethod::None || CaptureUnderWay()) return false;
        if (regime_crew_.kind == EmissionKind::Arm) return true;
        const double reach = console_.track.max_range_m > 0.0 ? console_.track.max_range_m : 1e9;
        for (const KnownTarget& k : pictures_[Cmdr()].Recent(time_, 30.0)) {
            if (k.has_range && !k.friendly && k.range_m <= reach) return false;
        }
        return true;
    }
    // Reminders (the radar seats told again): under a burst regime his crew knows, when he learns of a hostile after a quiet
    // spell while the bursts still apply (it is far out: the bursts; under the least emission the tracking transmitter's
    // range too), and under the least emission, where the senior operator keeps the rhythm himself, when a burst runs
    // longer than the regime's after he has kept one (under the limited regime the commander orders each burst: his own
    // «Высокое выключить!» is the correction). The reminders wait behind the fight's orders on the intercom.
    void NpcRemindRegime(bool halted) {
        if (post_ || !regime_crew_.Bursts() || !regime_crew_.Same(regime_cmdr_)) return;
        if (ssc_rule_due_s_ >= 0.0 && time_ >= ssc_rule_due_s_ &&
            NpcDoes(Cmdr(), SeatDuty::Order(Cmdr(), "order_remind_ssc", "the least emission: the operator told the tracking transmitter's rule"))) {
            GiveCrewOrder("order_remind_ssc");
        }
        bool hostile = false;
        for (const KnownTarget& k : pictures_[Cmdr()].Recent(time_, 30.0)) hostile = hostile || !k.friendly;
        if (hostile) {
            const bool after_quiet = time_ - regime_hostile_s_ > def_.emission.remind_quiet_s;
            regime_hostile_s_ = time_;
            if (after_quiet && halted && BurstsApply(halted) && time_ - regime_reminded_s_ > 30.0) {
                RemindRegime("a hostile far out after a quiet spell: the radar seats reminded of the regime", true);
                return;
            }
        }
        if (regime_crew_.kind == EmissionKind::Arm && LongBurst(halted) && time_ - regime_reminded_s_ > 30.0) {
            RemindRegime("the search transmitter on longer than the regime's burst", false);
        }
    }
    // A target ordered taken («Цель ... — взять!») and not yet tracked nor let go: the search transmitter stays on until the
    // tracking radar has it, at most kCaptureBurstS (the target designation is the search radar's picture; sam3d).
    bool CaptureUnderWay() const {
        if (engagement_.Method() != TrackMethod::None) return false;
        for (auto it = orders_given_.rbegin(); it != orders_given_.rend() && time_ - it->time_s <= kCaptureBurstS; ++it) {
            if (it->order == "order_drop_target") return false;
            if (it->order == "order_take_target") return true;
        }
        return false;
    }
    static constexpr double kCaptureBurstS = 30.0;
    // A burst running longer than the regime's, by breach_turns and a few seconds' grace, while the bursts apply and
    // after the crew has kept one burst of the regime (the first is the search drill's, which switches the transmitter on
    // among its other steps).
    bool LongBurst(bool halted) {
        const double turn = SearchTurnS();
        return BurstsApply(halted) && hv_on_since_s_ >= 0.0 && burst_kept_ && burst_kept_of_.Same(regime_crew_) &&
               time_ - hv_on_since_s_ > regime_crew_.OnS(turn) + def_.emission.breach_turns * turn + kBurstGraceS;
    }
    void RemindRegime(const std::string& why, bool with_ssc) {
        if (NpcDoes(Cmdr(), SeatDuty::Order(Cmdr(), "order_remind_bursts", why))) GiveCrewOrder("order_remind_bursts");
        if (with_ssc && regime_crew_.kind == EmissionKind::Arm && NpcDoes(Cmdr(), SeatDuty::Order(Cmdr(), "order_remind_ssc", why))) {
            GiveCrewOrder("order_remind_ssc");
        }
        if (!shadow_) regime_reminded_s_ = time_;
    }
    // order_emission_regime, order_remind_bursts, order_remind_ssc: said with the regime's numbers (the turns and the
    // seconds at this antenna's rate; Table 7.3's ranges). Passing the regime on is what the crew acts on from then; the
    // senior operator reads the bursts back, and under the least emission the operator is told the tracking
    // transmitter's rule with it.
    bool TellRegime(const std::string& order_id) {
        const bool pass_on = order_id == "order_emission_regime";
        const EmissionRegime r = pass_on ? regime_cmdr_ : regime_crew_;
        if (!pass_on && (!r.Bursts() || (order_id == "order_remind_ssc" && r.kind != EmissionKind::Arm))) return false;
        std::map<std::string, std::string> f = EmissionWords(def_.emission, r, SearchTurnS());
        if (order_id == "order_remind_ssc") {
            const EngagementTuning& t = def_.tuning;
            const double reach = console_.track.max_range_m > 0.0 ? console_.track.max_range_m : 20000.0;
            f["range_km"] = std::to_string(static_cast<int>(std::lround((t.has_arm_table ? t.arm_track_on_slow_far_m : reach) / 1000.0)));
            f["range_fast_km"] = std::to_string(static_cast<int>(std::lround((t.has_arm_table ? t.arm_track_on_fast_far_m : reach) / 1000.0)));
        }
        orders_given_.push_back({time_, order_id});
        if (callouts_) callouts_->Say(time_, order_id, Cmdr(), f);
        if (!pass_on) {
            if (order_id == "order_remind_ssc") ssc_rule_due_s_ = -1.0;
            ++emission_record_.reminders;
            Event(Cmdr(), order_id == "order_remind_ssc" ? "reminds the operator of the tracking transmitter's rule" : "reminds the senior operator of the bursts");
            return true;
        }
        regime_crew_ = r;
        ++emission_record_.regimes_told;
        emission_record_.told_after_s = regime_heard_s_ > -1e8 ? time_ - regime_heard_s_ : 0.0;
        Event(Cmdr(), "tells the crew the emission regime: " + RegimeText(r));
        log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName(Cmdr()), "intercom", "the emission regime told to the crew: " + RegimeText(r)});
        if (r.Bursts() && (MemberAt("senior_operator") != nullptr || !SeatHolder("senior_operator").empty()) &&
            NpcDoes("senior_operator", SeatDuty::Report("senior_operator", "emission_readback", "the bursts read back", f))) {
            Say("emission_readback", "senior_operator", f);
        }
        // Under the least emission the operator is told the tracking transmitter's rule once the senior operator has read
        // the bursts back (NpcRemindRegime).
        ssc_rule_due_s_ = r.kind == EmissionKind::Arm && (MemberAt("operator") != nullptr || !SeatHolder("operator").empty()) ? time_ + 8.0 : -1.0;
        return true;
    }
    // The record: the search transmitter's seconds at the halt while the bursts apply, and how long each burst runs.
    void RecordEmission(double dt) {
        if (!engagement_on_ || !def_.has_crew_data) return;
        const bool hv = console_.panel.Get("soc.hv") == 1 && console_.panel.Get("soc.search") == 1;  // radiating
        const bool halted = console_.equipment.GetInput("vehicle_stopped");
        if (hv && hv_on_since_s_ < 0.0) hv_on_since_s_ = time_;
        if (!hv) {
            if (hv_on_since_s_ >= 0.0 && regime_crew_.Bursts()) {  // a burst ended under the regime: the crew keeps it
                burst_kept_ = true;
                burst_kept_of_ = regime_crew_;
            }
            hv_on_since_s_ = -1.0;
            burst_breached_ = false;
        }
        if (BurstsApply(halted) && console_.panel.Get("soc.search") == 1) {
            emission_record_.bursts_s += dt;
            if (hv) emission_record_.radiating_s += dt;
        }
        if (!burst_breached_ && LongBurst(halted)) {
            burst_breached_ = true;
            ++emission_record_.breaches;
            log_.Record({time_, DebriefEntry::Kind::Scenario, SpeakerName("senior_operator"), "search radar",
                         "the transmitter on longer than the regime's burst (" + RegimeText(regime_crew_) + ")"});
        }
    }
    bool burst_breached_ = false;
    bool burst_kept_ = false;        // a burst of this regime ended: the crew keeps its bursts
    EmissionRegime burst_kept_of_;

    void WatchScope() {
        if (!engagement_on_ || !def_.has_crew_data || (MemberAt("senior_operator") == nullptr && SeatHolder("senior_operator").empty())) return;
        for (const Contact& c : engagement_.SearchPicture()) {
            const double a = DegToRad(c.azimuth_deg);
            const Vec3 p{c.range_m * std::sin(a), c.range_m * std::cos(a), 0.0};
            if (scope_trails_.count(c.target_id) == 0) InheritTrail(c.target_id, p);
            ScopeTrail& tr = scope_trails_[c.target_id];
            tr.last_seen_s = time_;
            if (!tr.marks.empty() && (tr.marks.back().second - p).Length() < 1.0) continue;  // the same look
            tr.marks.push_back({time_, p});
            while (tr.marks.size() > 2 && time_ - tr.marks.front().first > 10.0) tr.marks.erase(tr.marks.begin());
            if (tr.called || arms_known_.count(c.target_id) != 0) continue;
            const double span = time_ - tr.marks.front().first;
            if (span < 1.5) continue;
            const Vec3 v = (p - tr.marks.front().second) / span;
            const double speed = v.Length();
            const double closing = p.Length() > 1.0 ? -Dot(v, p.Normalized()) : 0.0;
            const bool at_us = speed > 1.0 && closing > kZeroParameterCos * speed;
            if (speed > kArmMarkSpeedMps && at_us) {
                tr.called = true;
                ArmSignSeen(c, "arm_launch");
                continue;
            }
            if (regime_crew_.kind != EmissionKind::Arm || span < 3.0) continue;
            if (at_us && speed > 100.0) tr.heading_in = true;
            // The turn by its last few seconds' marks (the long window lags a turn).
            std::size_t k = 0;
            while (k + 1 < tr.marks.size() && time_ - tr.marks[k + 1].first >= 1.5 && time_ - tr.marks[k].first > kTurnWindowS) ++k;
            const double recent_s = time_ - tr.marks[k].first;
            if (!tr.heading_in || recent_s < 1.5) continue;
            const Vec3 vr = (p - tr.marks[k].second) / recent_s;
            const double closing_now = -Dot(vr, p.Normalized());
            if (closing_now < 0.3 * vr.Length() && c.range_m > kTurnAwayMinRangeM) {
                tr.called = true;
                ArmSignSeen(c, "arm_turn_away");
            }
        }
        for (auto it = scope_trails_.begin(); it != scope_trails_.end();) {
            if (time_ - it->second.last_seen_s > 30.0) {
                it = scope_trails_.erase(it);
            } else {
                ++it;
            }
        }
    }
    // A new mark where one he was watching went out of sight and was heading (the radar's track broke, e.g. between
    // bursts while the target turned): to him it is the same blip, so its trail goes on under the new mark.
    void InheritTrail(const std::string& id, const Vec3& p) {
        std::string best;
        double best_m = kSameBlipM;
        for (const auto& kv : scope_trails_) {
            const ScopeTrail& o = kv.second;
            if (o.marks.size() < 2 || o.last_seen_s >= time_ - 0.05 || time_ - o.last_seen_s > 15.0) continue;
            const auto& f = o.marks.front();
            const auto& l = o.marks.back();
            const double span = l.first - f.first;
            const Vec3 v = span > 0.5 ? (l.second - f.second) / span : Vec3{};
            const double m = (l.second + v * (time_ - l.first) - p).Length();
            if (m < best_m) {
                best_m = m;
                best = kv.first;
            }
        }
        if (best.empty()) return;
        ScopeTrail moved = scope_trails_[best];
        scope_trails_.erase(best);
        scope_trails_[id] = moved;
    }
    // The mark's speed as the senior operator's scope shows it (< 0: not known yet).
    double ScopeSpeedMps(const std::string& target) const {
        const auto it = scope_trails_.find(target);
        if (it == scope_trails_.end() || it->second.marks.size() < 2) return -1.0;
        const auto& m = it->second.marks;
        const double span = m.back().first - m.front().first;
        return span >= 1.5 ? (m.back().second - m.front().second).Length() / span : -1.0;
    }
    // Whether the seat takes a sign of an ARM in: a "basic" tier (recruit, class3) doesn't know the signs; the others may
    // miss one (miss_chance, more under stress). The debrief says which.
    bool ArmSignNoticed(const std::string& seat, const char* unknown, const char* missed) {
        const SkillTier* t = TierAt(seat);
        if (t != nullptr && t->knows == "basic") {
            Event(seat, unknown);
            return false;
        }
        const CrewMember* m = MemberAt(seat);
        const double miss = t != nullptr ? t->miss_chance * (m != nullptr ? TierStress(*t, *m) : 1.0) : 0.0;
        if (static_cast<double>(hear_rng_()) / 4294967296.0 < miss) {
            Event(seat, missed);
            return false;
        }
        return true;
    }
    void ArmSignSeen(const Contact& c, const std::string& callout) {
        const bool launch = callout == "arm_launch";
        if (!ArmSignNoticed("senior_operator", launch ? "does not know the mark of an ARM for what it is" : "takes the target's turn for nothing",
                            launch ? "misses the mark of an ARM on his scope" : "misses the target's turn away")) {
            return;
        }
        KnownTarget k;
        k.target = c.target_id;
        k.has_azimuth = true;
        k.azimuth_deg = c.azimuth_deg;
        k.time_s = time_;
        const std::map<std::string, std::string> words{{"azimuth", MilsText(c.azimuth_deg)}};
        const char* sign = launch ? "a small fast mark coming straight at the vehicle: an ARM" : "a target that came at the battery turns away";
        if (!NpcDoes("senior_operator", SeatDuty::Report("senior_operator", callout, sign, words).About(k))) return;  // a player at the scope owes the call
        if (launch) {
            arms_known_.insert(c.target_id);
            for (auto& kv : pictures_) kv.second.Forget(c.target_id);
        }
        arm_seen_s_ = time_;  // he holds his next burst off himself (ArmEmission)
        Event("senior_operator", launch ? "sees a small fast mark coming straight at the vehicle: an ARM" : "sees a target that came at the battery turn away");
        Say(callout, "senior_operator", words, &k);
    }
    // The other signs of a launch (voenizdat_osa_1991 p262), each from what its instrument shows: on the tracking
    // radar's indicators the tracked target's pulse grows a moment, then a smaller pulse leaves it closing fast (the
    // range wheel's seat, at his range indicator); on the TV the tracked target firing it (the TV's seat).
    std::set<int> arm_signs_track_, arm_signs_tv_;
    static constexpr double kPulseApartM = 100.0;     // sam3d: two pulses of 0.45 us (67 m) told apart at 1.5 lengths
    static constexpr double kIndicatorSpanM = 3000.0;  // sam3d: the range indicator's expanded sweep about the gate
    static constexpr double kTvLaunchM = 1500.0;       // sam3d: the launch seen on the TV within this of the carrier
    void WatchTrackSigns() {
        const std::string tracked = engagement_.TrackedTarget();
        if (!em_link_ || !engagement_on_ || !def_.has_crew_data || tracked.empty()) return;
        const TrackMethod m = engagement_.Method();
        const double target_range = TargetRange(tracked);
        if (m == TrackMethod::None || target_range < 0.0) return;
        Vec3 target_at;
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            const Aircraft* a = def_.targets[i].id == tracked && targets_[i].spawned ? world_.FindAircraft(targets_[i].world_id) : nullptr;
            if (a != nullptr) target_at = a->position;
        }
        const Vec3 unit = def_.unit_position;
        const Vec3 rel_t = target_at - unit;
        const double true_az = RadToDeg(std::atan2(rel_t.x, rel_t.y));
        KnownTarget k;
        k.target = tracked;
        k.has_azimuth = k.has_range = true;
        k.azimuth_deg = std::remainder(true_az - engagement_.UnitHeadingDeg() + engagement_.GyroHeadingDeg(), 360.0);  // the crew's frame
        k.range_m = target_range;
        k.time_s = time_;
        const std::map<std::string, std::string> words{{"azimuth", MilsText(k.azimuth_deg)}};
        const std::string range_seat = def_.tuning.aim_range_role;
        const std::string tv_seat = def_.tuning.aim_el_role;
        const bool radar = m == TrackMethod::Radar || m == TrackMethod::RadarOnJam;
        const bool tv_on = !console_.track.tv_camera.empty() && console_.equipment.IsReady(console_.track.tv_camera);
        const double tv_half_rad = DegToRad(0.5 * TvFieldOfViewDeg(console_));
        for (const EmSensorLink::ArmView& a : em_link_->ArmsInFlight()) {
            const Vec3 rel = a.position - unit;
            const double range = rel.Length();
            if (range < 1.0) continue;
            const double closing = -Dot(a.velocity, rel / range);
            // The indicators: its own echo in the beam, ahead of the target's pulse, closing faster than any aircraft.
            if (radar && arm_signs_track_.count(a.id) == 0 && (MemberAt(range_seat) != nullptr || !SeatHolder(range_seat).empty()) &&
                target_range - range > kPulseApartM && target_range - range < kIndicatorSpanM && closing > kArmMarkSpeedMps &&
                em_link_->ArmEchoVisible(a.id)) {
                arm_signs_track_.insert(a.id);
                if (ArmSignNoticed(range_seat, "does not know the second pulse leaving the target for an ARM", "misses the second pulse leaving the target") &&
                    NpcDoes(range_seat, SeatDuty::Report(range_seat, "arm_launch_track", "a smaller pulse leaves the tracked target, closing fast: an ARM", words).About(k))) {
                    Event(range_seat, "sees a smaller pulse leave the tracked target's pulse on his indicator, closing fast: an ARM");
                    Say("arm_launch_track", range_seat, words, &k);
                }
            }
            // The TV: the launch itself, the missile leaving the target in the field of view.
            if (tv_on && arm_signs_tv_.count(a.id) == 0 && (MemberAt(tv_seat) != nullptr || !SeatHolder(tv_seat).empty()) &&
                (a.position - target_at).Length() < kTvLaunchM && AngleBetween(rel, rel_t) < tv_half_rad &&
                engagement_.TvLook(console_, tracked).seen) {
                arm_signs_tv_.insert(a.id);
                if (ArmSignNoticed(tv_seat, "does not know the launch on the TV for an ARM", "misses the launch on the TV") &&
                    NpcDoes(tv_seat, SeatDuty::Report(tv_seat, "arm_launch_tv", "the tracked target fires a missile on the TV: an ARM", words).About(k))) {
                    Event(tv_seat, "sees the tracked target fire a missile on the TV: an ARM");
                    Say("arm_launch_tv", tv_seat, words, &k);
                }
            }
        }
    }
    // The commander heard a sign of an ARM launch: the missile in the air is guided to its burst, then every transmitter
    // goes off (ArmSilent); the attack is reported up the battery net.
    // The commander hears a sign of an ARM launch: silent once the missile in the air has burst, and the attack reported
    // to the battery commander (his NPC's report, or a player's duty).
    void ArmSignHeard(const SpokenCallout& s) {
        if (!arm_attacked_) Event(Cmdr(), "hears the sign of an ARM launch: radiation off once the missile in the air has burst");
        arm_attacked_ = true;
        arm_sign_s_ = time_;
        arm_sign_az_ = MilsText(s.about.azimuth_deg);
        if (!battery_ || !callouts_ || time_ - arm_reported_s_ <= 60.0) return;
        if (!SeatHolder(Cmdr()).empty()) {  // a player's report: owed when his NPC would say it, after the silence
            if (arm_report_due_s_ < 0.0) arm_report_due_s_ = time_ + kArmReportDelayS;
            return;
        }
        const std::map<std::string, std::string> f{{"callsign", OwnCallsign()}, {"azimuth", arm_sign_az_}};
        if (!NpcDoes(Cmdr(), SeatDuty::Report(Cmdr(), "report_arm_attack", "an ARM launch: reported to the battery commander", f))) return;
        callouts_->Say(time_, "report_arm_attack", Cmdr(), f, kArmReportDelayS);
        ArmAttackReported(arm_sign_az_);
    }
    // The commander reports the ARM attack this long after its sign: «Излучение выключить!» to his crew comes first.
    static constexpr double kArmReportDelayS = 3.0;
    double arm_report_due_s_ = -1.0;  // a player commander's report of the ARM attack comes due then (< 0: none)
    void PlayerArmReport() {
        if (arm_report_due_s_ < 0.0 || time_ + 1e-9 < arm_report_due_s_) return;
        arm_report_due_s_ = -1.0;
        if (time_ - arm_reported_s_ <= 60.0) return;
        const std::map<std::string, std::string> f{{"callsign", OwnCallsign()}, {"azimuth", arm_sign_az_}};
        NpcDoes(Cmdr(), SeatDuty::Report(Cmdr(), "report_arm_attack", "an ARM launch: reported to the battery commander", f));
    }
    // The battery commander hears the report: a linked post's, or the battery's own (UpdateOsaBatteryEmission).
    void ArmAttackReported(const std::string& az) {
        arm_reported_s_ = time_;
        if (LinkedToPost()) {
            arm_reports_out_.push_back(az);
        } else if (arm_report_heard_s_ < 0.0) {
            arm_report_heard_s_ = time_;
        }
    }
    // Silent against an ARM: from the sign (once nothing of ours is in the air) for arm_silence_s.
    bool ArmSilent() const {
        return time_ - arm_sign_s_ < def_.doctrine.arm_silence_s && !engagement_.ShotsInFlight();
    }
    double SearchTurnS() {
        if (em_link_) {
            const em::RadarState* r = em_link_->SearchRadar();
            const em::EmLibrary& lib = em_link_->World().Scene().lib;
            if (r != nullptr && r->def_index >= 0 && static_cast<std::size_t>(r->def_index) < lib.radars.size()) {
                const double p = lib.radars[static_cast<std::size_t>(r->def_index)].emitter.scan_period_s;
                if (p > 0.0) return p;
            }
        }
        return 60.0 / 33.0;  // 33 rpm
    }
    // The range at which the commander has the tracking transmitter put on a target: its reach, and under the least
    // emission (the arm regime he heard) Table 7.3's far-edge range for the target's speed as the scope shows it (unknown:
    // fast).
    double TrackOnRangeM(const std::string& target) const {
        const double reach = console_.track.max_range_m > 0.0 ? console_.track.max_range_m : 1e9;
        const EngagementTuning& t = def_.tuning;
        if (regime_cmdr_.kind != EmissionKind::Arm || !t.has_arm_table) return reach;
        const double speed = ScopeSpeedMps(target);
        const double table = speed >= 0.0 && speed < t.arm_split_speed_mps ? t.arm_track_on_slow_far_m : t.arm_track_on_fast_far_m;
        return std::min(reach, table);
    }
    // The least emission under the threat of ARMs (the arm regime as told to the crew, voenizdat_osa_1991 p262): the
    // search transmitter's high voltage off now? Off while the tracking radar holds a stable track with nothing else
    // about; with nothing tracked, on for the regime's turns (2) after every off turns (3-4), and on through a capture.
    bool ArmSearchOff() {
        const TrackMethod m = engagement_.Method();
        if (CaptureUnderWay()) return false;
        if (m != TrackMethod::None) {
            if (m != TrackMethod::Radar || time_ - npc_track_since_s_ < kStableTrackS) return false;
            const std::string tracked = engagement_.TrackedTarget();
            for (const KnownTarget& k : pictures_[Cmdr()].Recent(time_, 30.0)) {
                if (k.has_range && !k.friendly && k.target != tracked && arms_heard_.count(k.target) == 0) return false;
            }
            return true;
        }
        const double turn = SearchTurnS();
        const double on = regime_crew_.OnS(turn);
        return std::fmod(time_, on + regime_crew_.OffS(turn)) >= on;
    }

    // ЗАПРОС waiting on the search transmitter between the limited regime's bursts (the interrogator's antenna is built
    // into the search antenna; his own query of a new echo seen in the last burst): the senior operator puts ВЫСОКОЕ on
    // for it and interrogates, and the commander's next burst order puts it off (as under the least emission,
    // ArmEmission; sam3d: otherwise the query, and the crew's orders behind it, waited for the next burst or for ever).
    void QueryBurst() {
        const std::string seat = "senior_operator";
        if (regime_crew_.kind != EmissionKind::Limited || !engagement_on_ || console_.panel.Get("soc.hv") != 0) return;
        if (!console_.equipment.GetInput("vehicle_stopped") || console_.panel.Get("soc.search") != 1 || NpcSilent() ||
            !console_.equipment.IsReady("hv_ready")) {
            return;
        }
        CrewMember* so = MemberAt(seat);
        if (so == nullptr || !SeatHolder(seat).empty()) return;  // a player's query waits on his own hand
        if (so->Status() != CrewStatus::Waiting || so->Tracker().GetProcedure().id != "iff_interrogate") return;
        const Procedure* on = console_.FindProcedure("hv_on");
        const Procedure* query = console_.FindProcedure("iff_interrogate");
        if (on == nullptr || query == nullptr) return;
        so->Order(*on);
        so->Queue(*query);
        Event(seat, "ЗАПРОС waits on the search transmitter: ВЫСОКОЕ on for the interrogation");
    }

    // Under the least emission (the arm regime, once his commander has told him) the senior operator keeps it himself, his
    // duty as ЗАПРОС is with a new echo: ВЫСОКОЕ on and off by ArmSearchOff while the search runs at a halt. A player in
    // his seat does it by hand.
    void ArmEmission() {
        const std::string seat = "senior_operator";
        if (regime_crew_.kind != EmissionKind::Arm || !engagement_on_ || (MemberAt(seat) == nullptr && SeatHolder(seat).empty())) return;
        if (!console_.equipment.GetInput("vehicle_stopped") || console_.panel.Get("soc.search") != 1 || NpcSilent()) return;
        if (!console_.equipment.IsReady("hv_ready") || (npc_arm_stow_ordered_ && !arm_relocated_) || (npc_stow_ordered_ && !npc_resumed_)) return;
        CrewMember* so = MemberAt(seat);
        const bool npc = so != nullptr && SeatHolder(seat).empty();
        // ЗАПРОС waits on the search transmitter (the interrogator's antenna is built into the search antenna): between
        // bursts he gives the burst for it, then interrogates (sam3d: otherwise each waited on the other all raid).
        const bool query_waits = npc && so->Status() == CrewStatus::Waiting && so->Tracker().GetProcedure().id == "iff_interrogate";
        if (npc && so->IsBusy() && !query_waits) return;
        if (!query_waits && console_.panel.Get("soc.beam") != kArmBeamProgram) {  // both low beams in each two-turn burst
            if (const Procedure* p = console_.FindProcedure("arm_beams")) {
                if (NpcDoes(seat, SeatDuty::Drill(seat, "arm_beams", "the least emission against ARMs: both low beams in each burst")) && so) so->Order(*p);
                return;
            }
        }
        // Having seen a sign of an ARM launch himself he gives no more bursts until the silence is over.
        const bool silence = time_ - arm_seen_s_ < def_.doctrine.arm_silence_s;
        const bool off = silence || (!query_waits && ArmSearchOff());
        if (console_.panel.Get("soc.hv") != (off ? 1 : 0)) return;
        const char* drill = off ? "hv_off" : "hv_on";
        if (const Procedure* p = console_.FindProcedure(drill)) {
            const char* why = off                ? "the least emission against ARMs: ВЫСОКОЕ off between bursts"
                              : query_waits        ? "ЗАПРОС needs the search transmitter: ВЫСОКОЕ on for the interrogation"
                              : CaptureUnderWay() ? "a target ordered taken: ВЫСОКОЕ on until the tracking radar has it"
                                                   : "the least emission against ARMs: ВЫСОКОЕ on for a two-turn burst";
            if (NpcDoes(seat, SeatDuty::Drill(seat, drill, why)) && so) {
                // The bursts are his standing routine, not an order to answer: no «Готово!» after each (as the limited
                // regime's, routine_order_).
                routine_drills_.insert(seat + "|" + drill);
                so->Order(*p);
                const Procedure* query = query_waits ? console_.FindProcedure("iff_interrogate") : nullptr;
                if (query != nullptr) so->Queue(*query);
            }
        }
    }

    // Radiation forbidden (the silent regime he heard), or an ambush not yet sprung: sprung once a hostile is designated
    // within 15 km (sam3d).
    bool NpcSilent() {
        if (ArmSilent()) return true;
        if (regime_cmdr_.kind == EmissionKind::Silent) return true;
        if (!battery_) return false;
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
        // Attacked by ARMs, with the regiment's standing order to change position after it (doctrine relocate_after_arm;
        // the battery commander changes position only on the regiment commander's order, voenizdat_osa_1991 p238): when
        // the fight is over and the silence too, the crew stows and the battery moves to its reserve position, keeping its
        // order; there it deploys, and the crew makes ready again.
        if (def_.doctrine.relocate_after_arm && battery_ && arm_attacked_ && !arm_relocated_ && npc_fought_ && !hostile &&
            !ArmSilent() && time_ - npc_last_hostile_s_ > 90.0 && time_ - arm_sign_s_ > def_.doctrine.arm_silence_s + 30.0) {
            if (!npc_arm_stow_ordered_) {
                npc_arm_stow_ordered_ = GiveCrewOrder("order_march");
                return true;
            }
            const CrewTask* stowed = def_.crew_data.FindTask("stowed");
            if (stowed != nullptr && console_.equipment.AllHold(stowed->conditions, console_.panel)) {
                arm_relocated_ = true;
                npc_fought_ = false;
                if (NpcDoes("driver", SeatDuty::Report("driver", "ready_to_march", "stowed after the ARM attack: ready to march"))) {
                    Say("ready_to_march", "driver");
                }
                BatteryOrder o;
                o.kind = BatteryOrder::Kind::Relocate;
                o.covered = def_.doctrine.relocate_to;
                BatteryTakes(o);
                Event(def_.id, "attacked by ARMs: the crew is stowed and the battery changes position (the regiment's standing order)");
            }
            return true;
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
                    BatteryTakes(o);
                }
                Event(def_.id, "the crew is stowed and marches on");
            }
            return true;
        }
        return false;
    }
    double npc_halt_at_ = -1.0;

    // Counter-jamming by doctrine (voenizdat_osa_1991 p259, p266): when the operator reports the range gate pulled off,
    // the protection on; once the fight is over and nothing has pulled the gate for a minute, off again.
    void NpcCounterJamming() {
        const TrackSensorDef& tr = console_.track;
        if (tr.rgpo_control.empty() || tr.range_pulled_input.empty()) return;
        const bool on = console_.panel.Get(tr.rgpo_control) == 1;
        if (console_.equipment.GetInput(tr.range_pulled_input)) {
            if (!on) NpcGive("order_counter_pulloff", 20.0);
            return;
        }
        if (!on || engagement_.Method() != TrackMethod::None || engagement_.ShotsInFlight() || time_ - last_pulled_s_ < 60.0) return;
        for (const KnownTarget& k : pictures_["commander"].Recent(time_, 30.0)) {
            if (!k.friendly && k.has_range) return;
        }
        NpcGive("order_protection_off", 120.0);
    }

    // Orientation at a halt (voenizdat_osa_1991 p246, p249-250): with nothing hostile heard of, the commander finds Qc
    // with the artillery compass and, once he has it, orders the deputy to orient the gyro with it. Once per halt; in a
    // fight he stays at his post and the crew works in the gyro's frame as it is.
    void NpcOrient() {
        if (!console_.has_orientation || halt_oriented_ || !engagement_.QcKnown()) return;
        if (!console_.equipment.IsReady(console_.orientation.gyro)) return;
        for (const KnownTarget& k : pictures_["commander"].Recent(time_, 30.0)) {
            if (!k.friendly && k.has_range) return;
        }
        NpcGive("order_orient", 120.0);
    }
    // The commander's own compass work: an NPC in his seat sets up the compass as soon as the vehicle halts (it needs no
    // power, so it runs while the equipment warms up), unless he already has this halt's Qc or something hostile is
    // heard of (then he stays at his post). Worked out ahead of the vehicle, only while he can get out to it (the
    // hatches stay shut in combat work).
    void NpcSurvey() {
        if (!console_.has_orientation) return;
        // A player crew chief's aiming circle still standing after its reading: stow it and bring the Qc in.
        const OrientationDef& so = console_.orientation;
        if (!so.survey_procedure.empty() && !SeatHolder(Cmdr()).empty() && compass_.Known() && console_.panel.Get(so.survey_control) == so.survey_up_position) {
            NpcDoes(Cmdr(), SeatDuty::Control(Cmdr(), so.survey_control, so.survey_stowed_position, "the aiming circle read: stow it and bring the Qc in"));
        }
        if (halt_oriented_ || engagement_.QcKnown()) return;
        if (MemberAt(Cmdr()) == nullptr && SeatHolder(Cmdr()).empty()) return;
        bool hostile = false;
        for (const KnownTarget& k : pictures_["commander"].Recent(time_, 30.0)) hostile = hostile || (!k.friendly && k.has_range);
        const bool working = compass_.Running() || survey_drill_;
        if (working && !hostile && !survey_recalled_) {
            const std::string by = RecallSignaller();
            if (!by.empty()) {
                survey_recalled_ = true;
                Event(by, "signals the crew chief back from the aiming circle: a target");
            }
        }
        if (SurveyUnderWay()) {
            // A target: back to his post; the crew fights in the gyro's frame as it is (a Qc he already has, he tells).
            if ((hostile || survey_recalled_) && SeatHolder(Cmdr()).empty() && working) BreakOffSurvey();
            return;
        }
        // Signalled back, he stays at his post while the crew still has the target.
        if (survey_recalled_ && !Outside(Cmdr()) && CrewKnowsHostile().empty()) survey_recalled_ = false;
        if (hostile || survey_recalled_ || !console_.equipment.GetInput("vehicle_stopped")) return;
        const OrientationDef& o = console_.orientation;
        const std::string place = JobPlace(o.survey_control);
        if (!place.empty() && Mover(Cmdr()) != nullptr && Mover(Cmdr())->Inside() && WaySeconds(Cmdr(), std::string(), place) <= 0.0) return;
        if (SeatHolder(Cmdr()).empty()) {
            // An NPC first does what his seat has (his orders, his set: as he thinks, NpcCommanderActive), and not while
            // the crew is told to stay (К машинам).
            const CrewMember* m = MemberAt(Cmdr());
            if (NpcCommanderActive() && npc_thought_s_ != time_) return;
            if (!SeatFree(Cmdr()) || (m != nullptr && !m->StaysAt().empty())) return;
        }
        const bool column = battery_ && def_.battery_drive_own &&
                            (battery_->CurrentMode() == Battery::Mode::Forming || battery_->CurrentMode() == Battery::Mode::Marching);
        if (column) return;  // a stop in the column is not a halt
        // A player in command: his К бою carries the compass work (the crew data's order_k_boyu actions), so a separate
        // order is his only once К бою has been given and the Qc is still not known; as a drill out ahead, К бою given,
        // the aiming circle is his to set up.
        if (!SeatHolder(Cmdr()).empty() && !PlayerGave("order_k_boyu", 1e9)) return;
        if (!SeatHolder(Cmdr()).empty() && !o.survey_procedure.empty()) {
            NpcDoes(Cmdr(), SeatDuty::Control(Cmdr(), o.survey_control, o.survey_up_position, "a halt without the vehicle's Qc: the aiming circle out ahead"));
            return;
        }
        if (NpcDoes(Cmdr(), SeatDuty::Order(Cmdr(), "order_compass", "a halt without the vehicle's Qc: the compass work"))) StartCompassSurvey();
    }
    bool npc_empty_reported_ = false;
    bool npc_fuel_reported_ = false;
    // The commander's request for the TZM or the fuel truck gone out on the battery net, by the NPC or a player in his
    // seat from the menu (G9): the vehicle comes only once asked for; cleared once it has served.
    bool tzm_asked_ = false;
    bool fuel_asked_ = false;

    // Logistics: the deputy reports the launcher empty and the commander asks the battery for the TZM; the driver reports
    // the fuel at half (the crew asks for fuel at half: samsim_thread, ePap), and at the reserve the commander asks for
    // the fuel truck. The TZM and the fuel truck themselves come through the battery (ServiceDrills).
    void NpcLogistics() {
        if (launcher_) {
            const bool empty = launcher_->LoadedCount() == 0;
            if (empty && !npc_empty_reported_ && !engagement_.ShotsInFlight()) {
                npc_empty_reported_ = true;
                if (NpcDoes("deputy_commander", SeatDuty::Report("deputy_commander", "launcher_empty", "no missile left on the launcher"))) {
                    Say("launcher_empty", "deputy_commander");
                }
            }
            if (!empty) npc_empty_reported_ = false;
            if (empty && !tzm_asked_ && !engagement_.ShotsInFlight() &&
                NpcDoes(Cmdr(), SeatDuty::Report(Cmdr(), "request_tzm", "the launcher empty: ask the battery for the TZM")) &&
                RequestOnAir("the TZM")) {
                tzm_asked_ = SayOnRadio("request_tzm", {{"callsign", battery_ ? OwnCallsign() : std::string("21")}}, false);
            }
        }
        if (fuel_ && battery_ && fuel_->Requested() && !fuel_asked_ &&  // the fuel model asks at its reserve
            NpcDoes(Cmdr(), SeatDuty::Report(Cmdr(), "request_fuel", "the fuel at its reserve: ask the battery for the fuel truck")) &&
            RequestOnAir("the fuel truck")) {
            fuel_asked_ = SayOnRadio("request_fuel", {{"callsign", OwnCallsign()}}, false);
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
    // Back on the regiment's net at a new position (roadmap 14.5): the readiness drills deploy the post with the R-111 on
    // duty reception and its antenna on the whip (the task needs the set only on), so once the post stands deployed, out
    // of radio silence, the battery commander turns the antenna to the mast (his drill) and then has the radiotelephonist
    // tune the set for it: the two drills of «Пункт управления развернуть!» the readiness plan leaves out.
    void NpcRegimentLink() {
        if (!post_ || !r111_ || def_.radio_data.Find("regiment") == nullptr || (nets_ && nets_->Silence())) return;
        if (def_.radio_plan.presets_hz[0] <= 0.0) return;  // no frequency of the regiment's to tune it to
        const CrewTask* deployed = def_.crew_data.FindTask("ready_halted");
        if (deployed == nullptr || !console_.equipment.AllHold(deployed->conditions, console_.panel)) return;
        const std::string antenna = console_.r111.Control("antenna");
        if (!antenna.empty() && console_.panel.Get(antenna) != 1 && console_.equipment.IsReady("mast_up")) {
            if (SeatFree(Cmdr()) && !NpcOrdered(Cmdr(), "deploy_command_radios", 60.0)) NpcOrder(Cmdr(), "deploy_command_radios");
            return;  // tuned on the whip, it would have to be tuned again
        }
        if (!r111_->CanTransmit(ReadR111(console_), time_) && !r111_->Tuning(time_)) NpcGive("order_r111", 120.0);
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
            if (ProcedureWanted(st.procedure) && !SeatFree(st.station) && !(shadow_breaks_check_ && InFunctionCheck(st.station))) return false;
        }
        const bool given = GiveCrewOrder(order_id);
        if (shadow_ && !given) return false;  // the shadow commander: the player hasn't given it yet
        npc_ordered_["order"] = {order_id, time_};
        return true;
    }
    std::vector<C2Network::Event> designations_heard_;  // every designation so far (the latest last)
    std::map<std::string, double> iff_unanswered_;      // when he last reported each target without a reply
    std::vector<KnownTarget> iff_batch_;                // the echoes of the last interrogation without a reply
    // One "Ответа нет!" for an interrogation's unanswered echoes (sam3d: one a target, fifteen in a row, said nothing
    // new): about the target that matters, the one tracked or ordered taken, else the nearest he hasn't reported in the
    // last two minutes; nothing when every one of them was reported.
    void ReportNoReply() {
        if (iff_batch_.empty()) return;
        std::vector<KnownTarget> batch;
        batch.swap(iff_batch_);
        const std::string tracked = engagement_.TrackedTarget();
        const KnownTarget* pick = nullptr;
        for (const KnownTarget& k : batch) {
            if (k.target == tracked || k.target == assigned_target_) pick = &k;
        }
        if (pick == nullptr) {
            double best = 1e18;
            for (const KnownTarget& k : batch) {
                const auto said = iff_unanswered_.find(k.target);
                if (said != iff_unanswered_.end() && time_ - said->second < 120.0) continue;
                const double r = TargetRange(k.target);
                const double key = r >= 0.0 ? r : 1e17;
                if (key < best) {
                    best = key;
                    pick = &k;
                }
            }
        } else {
            const auto said = iff_unanswered_.find(pick->target);
            if (said != iff_unanswered_.end() && time_ - said->second < 120.0) return;
        }
        if (pick == nullptr) return;
        const KnownTarget k = *pick;
        // The words cover every echo of the interrogation without a reply: a listener learns them all with it. Owed
        // once whoever holds the seat.
        std::vector<KnownTarget>& also = iff_companions_[k.target];
        also.clear();
        for (const KnownTarget& o : batch) {
            iff_unanswered_[o.target] = time_;
            if (o.target != k.target) also.push_back(o);
        }
        if (NpcDoes("senior_operator", SeatDuty::Report("senior_operator", "iff_no_reply", "no reply mark beside the echo").About(k))) {
            Say("iff_no_reply", "senior_operator", {}, &k);
        }
    }
    std::map<std::string, std::vector<KnownTarget>> iff_companions_;  // a "Ответа нет!"'s target -> the other echoes it covers
    std::map<std::string, double> drill_started_s_;  // "<seat>|<procedure>" -> when he began it (his "Есть!")
    std::map<std::string, double> passed_at_;  // when the commander last passed a target's designation on
    std::map<std::string, bool> passed_far_;    // it was still far out then (it is passed on again as it comes near)

    // A crew member's reports as words: acknowledgement, done, can't, fault; and the start of a wait (reminders).
    void CrewCallouts(const CrewMember& c, const std::vector<CrewReport>& reports) {
        if (!callouts_ || !SeatHolder(c.Role()).empty()) return;
        for (const CrewReport& r : reports) {
            const std::string key = c.Role() + "|" + r.procedure_id;
            const bool routine = routine_drills_.count(key) != 0;
            if (r.kind == CrewReport::Kind::Completed || r.kind == CrewReport::Kind::Aborted) routine_drills_.erase(key);
            switch (r.kind) {
                // The commanding seat doesn't answer himself; a standing order's routine needs no answer.
                case CrewReport::Kind::Started:
                    drill_started_s_[key] = time_;
                    if (c.Role() != Cmdr() && !routine) Say("ack", c.Role());
                    break;
                // Under heavy stress the talk shrinks to what is needed (docs/crew/15_TALK.md section 4): a drill done
                // within 10 s of its "Есть!" isn't reported done (the commander sees it on his lamps).
                case CrewReport::Kind::Completed: {
                    const auto started = drill_started_s_.find(key);
                    const bool quick = c.StressLevel() >= 0.7 && started != drill_started_s_.end() && time_ - started->second < 10.0;
                    if (c.Role() != Cmdr() && !routine && !quick) Say("done", c.Role());
                    break;
                }
                // A drill the commander replaced or called off himself needs no "can't", nor does an unspoken routine
                // (the burst's transmitter refused with the search stopped: he sees it and calls "Поиск!").
                case CrewReport::Kind::Aborted:
                    if (r.detail != "superseded" && r.detail != "cancelled" && !routine) Say("cannot", c.Role(), {{"reason", r.detail}});
                    break;
                case CrewReport::Kind::EquipmentFault: {
                    const SubsystemDef* d = console_.equipment.Def(r.detail);
                    Say("fault", c.Role(), {{"what", d != nullptr && !d->name.empty() ? d->name : r.detail}});
                    break;
                }
                default: break;
            }
        }
    }

    // ---- The presentation feed (fx/events.hpp, docs/fx/README.md 3.1): pushed where the models decide, never read by the
    // simulation. Its draws (a burst's place, whether a wreck burns) come from fx_rng_ alone; switched off (SetFxOn) it
    // pushes and draws nothing.
    fx::Feed fx_;
    std::mt19937 fx_rng_{1u};
    bool fx_on_ = true;
    static constexpr unsigned kFxSeed = 0xF0E5u;           // sam3d: the presentation generator's seed, after the scenario's
    static constexpr double kFxLaunchUpM = 3.5;            // the missile on its rails above the ground (sam3d)
    static constexpr double kFxLaunchElevationDeg = 27.0;  // voenizdat_osa_1991: the 9M33 leaves at a constant 27 degrees
    static constexpr double kFxLaunchSpeedMps = 30.0;      // its speed off the rails, for the drawing (sam3d)
    static constexpr double kFxSelfDestructMinS = 25.0;    // voenizdat_osa_1991 p22: it destroys itself 25-28 s after launch
    static constexpr double kFxSelfDestructMaxS = 28.0;
    static constexpr double kFxFlyOnMinS = 0.5;            // a missile that missed is shown flying on at least this long (sam3d)
    static constexpr int kFxThrownOffBase = 1000;          // a thrown-off missile's number: this + its count (never a launch's)
    static constexpr double kFxUnguidedMps = 500.0;        // an unguided missile's average speed, when the tuning has none (sam3d)
    static constexpr double kFxHitOffsetM = 5.0;           // a hit's burst within the warhead's reach of the target (sam3d)
    static constexpr double kFxNearMinM = 10.0;            // a near burst beside it (sam3d)
    static constexpr double kFxNearMaxM = 25.0;
    static constexpr double kFxPastMinM = 500.0;           // a neighbour's missile that missed flies on this far past its
    static constexpr double kFxPastMaxM = 2000.0;          // target before it destroys itself (sam3d)
    static constexpr double kFxPastMps = 500.0;
    static constexpr double kFxWreckFireChance = 0.7;      // a wreck we didn't watch burning (sam3d)
    static constexpr double kFxWreckFallsChance = 0.5;     // ... dropping out of the sky at once (sam3d)

    double FxDraw() { return Uniform01(fx_rng_); }
    double FxBetween(double lo, double hi) { return lo + (hi - lo) * FxDraw(); }
    // A point lo..hi m from `at`, in a direction drawn evenly over the sphere.
    Vec3 FxAround(const Vec3& at, double lo, double hi) {
        const double az = FxBetween(0.0, kTwoPi);
        const double el = std::asin(FxBetween(-1.0, 1.0));
        return at + DirectionFromAzEl(az, el) * FxBetween(lo, hi);
    }
    // A system as the munitions library knows it: an air defence system id (air_war/data.hpp AirDefenceSystems), the
    // console's or tuning's "osa_ak" as "osa".
    static std::string FxSystem(const std::string& system_id) {
        return system_id.compare(0, 3, "osa") == 0 ? std::string("osa") : system_id;
    }
    std::string FxOwnWeapon() const { return FxSystem(console_.system_id); }
    // A launcher's rails over the ground where it stands (ours too: the unit's ENU z is not the ground far from the
    // origin, and stays the start's as the vehicle drives).
    Vec3 FxRails(const Vec3& at) const { return GroundEnu(at.x, at.y) + Vec3{0.0, 0.0, kFxLaunchUpM}; }
    // A missile leaving `from` toward `toward` at the 9M33's 27 degrees.
    static Vec3 FxLaunchVelocity(const Vec3& from, const Vec3& toward) {
        return DirectionFromAzEl(AzimuthOf(toward - from), DegToRad(kFxLaunchElevationDeg)) * kFxLaunchSpeedMps;
    }
    // A scenario target's aircraft (appeared, alive or not), and its index (def_.targets.size(): none).
    std::size_t TargetIndexOf(const std::string& id) const {
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            if (def_.targets[i].id == id) return i;
        }
        return def_.targets.size();
    }
    const Aircraft* AircraftOf(const std::string& id) const {
        const std::size_t i = TargetIndexOf(id);
        return i < def_.targets.size() && targets_[i].spawned ? world_.FindAircraft(targets_[i].world_id) : nullptr;
    }

    // Our missile leaves the launcher (EngagementModel::OnLaunch has just taken it).
    void FxLaunch(int number) {
        if (!fx_on_) return;
        const std::string target = engagement_.TrackedTarget();
        const Aircraft* a = target.empty() ? nullptr : AircraftOf(target);
        const double flight_s = engagement_.ShotFlightTime(number);
        if (a == nullptr || flight_s <= 0.0) {  // no target track: it goes off unguided and destroys itself
            FxUnguided(number, engagement_.UnitHeadingDeg(), FxBetween(kFxSelfDestructMinS, kFxSelfDestructMaxS));
            return;
        }
        fx::Event e;
        e.kind = fx::EventKind::MissileLaunch;
        e.t_s = time_;
        e.position = FxRails(def_.unit_position);
        e.velocity = FxLaunchVelocity(e.position, a->position);
        e.aim = a->position;
        e.end_t_s = time_ + flight_s;
        e.source = "own";
        e.target = target;
        e.number = number;
        e.weapon = FxOwnWeapon();
        fx_.Push(e);
    }

    // A missile that leaves with no target (no track at launch, or ИЗДЕЛИЯ thrown off): along its launch line on the hull's
    // `azimuth_deg`, destroying itself after `flight_s`.
    void FxUnguided(int number, double azimuth_deg, double flight_s) {
        if (!fx_on_) return;
        fx::Event e;
        e.kind = fx::EventKind::MissileLaunch;
        e.t_s = time_;
        e.position = FxRails(def_.unit_position);
        e.velocity = DirectionFromAzEl(DegToRad(azimuth_deg), DegToRad(kFxLaunchElevationDeg)) * kFxLaunchSpeedMps;
        e.end_t_s = time_ + flight_s;
        e.source = "own";
        e.number = number;
        e.weapon = FxOwnWeapon();
        fx_.Push(e);
        const double v = def_.tuning.missile_average_speed_mps > 0.0 ? def_.tuning.missile_average_speed_mps : kFxUnguidedMps;
        e.kind = fx::EventKind::MissileEnd;
        e.end = fx::BurstEnd::Wide;
        e.aim = e.position + e.velocity.Normalized() * (v * flight_s);
        fx_.Push(e);
    }

    // Our shot's end (the engagement model's Hit or Miss): MissileEnd, and AircraftDown for a hit. `assessed`: the kill
    // assessment has just opened it, and its draw is the end (the signs the crew sees follow from it); else the
    // presentation draws one by the same chances.
    void FxShotEnd(const EngagementEvent& ev, bool assessed) {
        if (!fx_on_) return;
        const bool hit = ev.kind == EngagementEvent::Kind::Hit;
        const KillAssessmentTuning& kt = def_.tuning.kill;
        KillAssessment::End end = KillAssessment::End::Wide;
        bool fire = false, falls = false;
        if (assessed && kill_.EndOf(ev.shot, end)) {
            fire = kill_.TvFire(ev.shot);
            falls = kill_.Falls(ev.shot);
        } else {
            if (hit) {
                end = KillAssessment::End::Hit;
            } else if (ev.detail == "track lost") {
                end = KillAssessment::End::Early;
            } else if (ev.detail == "out of reach" || ev.detail == "target already down") {
                end = KillAssessment::End::Wide;
            } else {
                const double d = FxDraw();
                end = d < kt.early_burst_chance ? KillAssessment::End::Early
                      : d < kt.early_burst_chance + kt.near_burst_chance ? KillAssessment::End::NearBurst
                                                                          : KillAssessment::End::Wide;
            }
            fire = hit && FxDraw() < kt.tv_fire_chance;
            falls = hit && FxDraw() < kt.elevation_drop_chance;
        }
        fx::Event e;
        e.kind = fx::EventKind::MissileEnd;
        e.t_s = time_;
        e.end_t_s = time_;
        e.source = "own";
        e.target = ev.target;
        e.number = ev.shot;
        e.weapon = FxOwnWeapon();
        double launch_t = time_;
        EngagementModel::MissileView v;
        if (engagement_.ShotView(ev.shot, time_, world_, eng_targets_, v)) {
            e.position = v.position;
            e.velocity = v.velocity;
            launch_t = v.launch_t_s;
        } else {
            e.position = FxRails(def_.unit_position);
        }
        const Aircraft* a = AircraftOf(ev.target);
        const Vec3 at = a != nullptr ? a->position : e.position;
        switch (end) {
            case KillAssessment::End::Hit:
                e.end = fx::BurstEnd::Hit;
                e.aim = FxAround(at, 0.0, kFxHitOffsetM);
                break;
            case KillAssessment::End::NearBurst:
                e.end = fx::BurstEnd::NearBurst;
                e.aim = FxAround(at, kFxNearMinM, kFxNearMaxM);
                break;
            case KillAssessment::End::Wide:
            case KillAssessment::End::Early: {
                // It flies on to destroy itself 25-28 s after its launch, and always some time after now: an
                // "out of reach" miss is decided at the end of its flight time (26 s), and a missile that ended now would
                // look like a burst on an aircraft that flies on.
                e.end = end == KillAssessment::End::Wide ? fx::BurstEnd::Wide : fx::BurstEnd::Early;
                const double sd = fx::SelfDestructTime(launch_t, time_, FxDraw(), kFxSelfDestructMinS, kFxSelfDestructMaxS, kFxFlyOnMinS);
                e.aim = e.position + e.velocity * (sd - time_);
                e.end_t_s = sd;
                break;
            }
        }
        e.fire = hit && fire;
        e.falls = hit && falls;
        fx_.Push(e);
        if (hit) FxDown(ev.target, "own", fire, falls);
    }

    // An aircraft destroyed: its wreck's start.
    void FxDown(const std::string& id, const std::string& by, bool fire, bool falls) {
        if (!fx_on_) return;
        const std::size_t i = TargetIndexOf(id);
        const Aircraft* a = AircraftOf(id);
        if (a == nullptr) return;
        fx::Event e;
        e.kind = fx::EventKind::AircraftDown;
        e.t_s = time_;
        e.position = a->position;
        e.velocity = a->velocity;
        e.aim = a->position;
        e.source = by;
        e.target = id;
        e.world_id = a->id;
        e.fire = fire;
        e.falls = falls;
        e.model = def_.targets[i].model;
        fx_.Push(e);
    }
    // ... with the presentation's own draws (another's kill: nobody's instruments decided how it looked).
    void FxDown(const std::string& id, const std::string& by) {
        if (!fx_on_) return;
        const bool fire = FxDraw() < kFxWreckFireChance;
        const bool falls = FxDraw() < kFxWreckFallsChance;
        FxDown(id, by, fire, falls);
    }

    // A launcher of the battery or of a neighbour fires (BatteryFire's Launch), and its missile's end (Hit, Miss). Both
    // carry the target's World id, so the game follows it.
    void FxSamLaunch(const BatteryFire::Shooter& s, const std::string& target, const std::string& weapon) {
        if (!fx_on_) return;
        const Aircraft* a = AircraftOf(target);
        if (a == nullptr) return;
        fx::Event e;
        e.kind = fx::EventKind::SamLaunch;
        e.t_s = time_;
        e.position = FxRails(s.position);
        e.velocity = FxLaunchVelocity(e.position, a->position);
        e.aim = a->position;
        e.end_t_s = s.phase == BatteryFire::Phase::InFlight ? s.next_s : time_;
        e.source = s.callsign;
        e.target = target;
        e.world_id = a->id;
        e.number = s.launches;
        e.weapon = weapon;
        fx_.Push(e);
    }
    void FxSamEnd(const BatteryFire::Shooter& s, const std::string& target, const std::string& weapon, bool hit) {
        if (!fx_on_) return;
        const Aircraft* a = AircraftOf(target);
        if (a == nullptr) return;
        fx::Event e;
        e.kind = fx::EventKind::SamEnd;
        e.t_s = time_;
        e.end_t_s = time_;
        e.position = FxRails(s.position);
        e.source = s.callsign;
        e.target = target;
        e.world_id = a->id;
        e.number = s.launches;
        e.weapon = weapon;
        if (hit) {
            e.end = fx::BurstEnd::Hit;
            e.aim = FxAround(a->position, 0.0, kFxHitOffsetM);
        } else {  // it flies on past the target and destroys itself
            e.end = fx::BurstEnd::SelfDestruct;
            Vec3 dir = a->position - e.position;
            dir = dir.LengthSquared() > 1.0 ? dir.Normalized() : Vec3{0.0, 0.0, 1.0};
            const double past = FxBetween(kFxPastMinM, kFxPastMaxM);
            e.aim = a->position + dir * past;
            e.velocity = dir * kFxPastMps;
            e.end_t_s = time_ + past / kFxPastMps;
        }
        fx_.Push(e);
    }

    void UpdateCallouts() {
        if (!callouts_) {
            if (engagement_on_) {  // nobody to say them; the feed still shows how each shot ended
                for (const EngagementEvent& e : engagement_.TakeEvents()) {
                    if (e.kind == EngagementEvent::Kind::Hit || e.kind == EngagementEvent::Kind::Miss) FxShotEnd(e, false);
                }
            }
            return;
        }
        // Automatic reports: a seat says it when its condition becomes true. A lamp that flickers (out for a moment
        // only) is not called again and again: he said it a moment ago.
        report_said_s_.resize(def_.crew_data.reports.size(), -1e9);
        report_off_s_.resize(def_.crew_data.reports.size(), -1e9);
        for (std::size_t i = 0; i < def_.crew_data.reports.size(); ++i) {
            const AutoReportDef& r = def_.crew_data.reports[i];
            const bool now = console_.equipment.Holds(r.when, console_.panel);
            // Under a regime of bursts the search transmitter goes on every few turns: "Готов СОЦ!" once is enough.
            const bool bursting = regime_crew_.Bursts() && r.callout == "soc_ready" &&
                                  time_ - report_said_s_[i] < 300.0;
            const bool flicker = bursting || (time_ - report_off_s_[i] < kReportFlickerS && time_ - report_said_s_[i] < kReportAgainS);
            // Owed once whoever holds the seat: a player's seat isn't asked for it again at every flicker or burst.
            if (now && report_state_[i] == 0 && !flicker) {
                if (NpcDoes(r.from, AutoReportDuty(r))) Say(r.callout, r.from);
                report_said_s_[i] = time_;
            }
            if (!now && report_state_[i] != 0) report_off_s_[i] = time_;
            report_state_[i] = now ? 1 : 0;
        }
        // What the instruments showed.
        if (engagement_on_) {
            ReportNoReply();
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
                        // With nothing in the air the deputy's launch drill has no target to wait for: under the NPC
                        // commander (who orders it again with the next lock) he leaves it, so the seat is free to take the
                        // target again (by the radar or the TV), and the permission lapses.
                        if (NpcCommanderActive() && !engagement_.ShotsInFlight() && SeatHolder("deputy_commander").empty()) {
                            if (CrewMember* dep = MemberAt("deputy_commander")) {
                                const std::string id = dep->IsBusy() ? dep->Tracker().GetProcedure().id : std::string();
                                if (id == "launch" || id == "prepare_firing" || id == "fire_at_jammer") dep->CancelOrder();
                            }
                            npc_permit_s_ = -1e9;
                        }
                        break;
                    case EngagementEvent::Kind::IffNoReply: {
                        KnownTarget k;  // the senior operator saw no reply mark beside the echo
                        k.target = e.target;
                        k.interrogated = true;
                        k.time_s = time_;
                        k.source = "IFF";
                        pictures_["senior_operator"].Learn(k);
                        iff_batch_.push_back(k);  // reported once for the interrogation, below
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
                        FxShotEnd(e, e.detail != "target already down");  // the feed's end is the one the signs show
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
        // Designations the commander heard on the radio, passed on to the crew when they matter to it (sam3d: every
        // update of every target of a massed raid kept the intercom talking without end and the orders waiting behind
        // it): a new target near (within half again the tracking radar's reach), or far out when no other was passed on
        // in the last minute (the raid's first); one passed on far out again as it comes near; the one he has ordered
        // taken, every 15 s. A target another launcher has, once.
        if (c2_) {
            const double near_m = 1.5 * (console_.track.max_range_m > 0.0 ? console_.track.max_range_m : 25000.0);
            int lately = 0;
            for (const auto& pa : passed_at_) lately += time_ - pa.second < 60.0 ? 1 : 0;
            for (const C2Network::Event& e : designations_) {
                const auto last = passed_at_.find(e.subject);
                const bool fresh = last == passed_at_.end();
                const bool ours = e.subject == assigned_target_;
                const bool theirs = battery_fire_ && battery_fire_->Engaged(e.subject);
                const bool close_in = e.range_m <= near_m;
                const auto was_far = passed_far_.find(e.subject);
                const bool closing = !fresh && was_far != passed_far_.end() && was_far->second && close_in && !theirs;
                const bool pass = (ours && (fresh || time_ - last->second >= 15.0)) || closing || (fresh && (close_in || lately == 0));
                if (!pass) continue;
                passed_at_[e.subject] = time_;
                passed_far_[e.subject] = !close_in;
                ++lately;
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
                    // A slip of the tongue (his tier's slip_chance; roadmap 7.2): a digit of the azimuth or the range said
                    // wrong, and the crew aims by what it was told until a later pass puts it right.
                    const SkillTier* tier = TierAt(Cmdr());
                    if (tier != nullptr && static_cast<double>(relay_rng_()) / 4294967296.0 < tier->slip_chance) SlipDesignation(az, r, k);
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

    // ---- The regiment's other Osa batteries beside the scenario (neighbours that fight; the nation on the map N4).
    std::unique_ptr<BatteryFire> neighbour_fire_;
    std::mt19937 neighbour_fire_rng_{6151u};
    bool neighbour_fire_built_ = false;
    std::vector<std::size_t> shooter_neighbour_;            // per shooter: its neighbour (an index into def_.neighbours)
    std::map<std::string, std::string> neighbour_kills_;    // target -> the neighbour's id that shot it down
    std::map<std::string, int> neighbour_launches_;         // neighbour id -> missiles launched
    std::map<std::string, double> neighbour_radar_s_;       // neighbour id -> launcher-seconds on a target (radiating)

    // Whether the scenario has the Osa's tuning to fight its neighbours by (its own, or its launchers' at a post).
    bool NeighbourFireTuned() const { return engagement_on_ || def_.has_launcher_tuning; }

    // A fighting neighbour's working launchers in the battery's combat formation about its place: two pairs 3 km apart
    // east and west, 200 m within (battery_tactics.json).
    std::vector<Vec3> NeighbourLauncherPlaces(const ScenarioNeighbour& nb) const {
        std::vector<Vec3> out;
        const int n = std::max(1, std::min(4, nb.launchers));
        for (int k = 0; k < n; ++k) {
            const double east = k < 2 ? -1500.0 : 1500.0;
            const double north = k % 2 == 0 ? -100.0 : 100.0;
            out.push_back(Vec3{nb.position.x + east, nb.position.y + north, def_.unit_position.z});
        }
        return out;
    }

    // Each fighting neighbour's working launchers are shooters, set up in the battery's combat formation (pairs 3 km apart,
    // 200 m within: battery_tactics.json, video_osa) about its place, with its missiles shared among them. Their battery
    // post is not ours: each takes the nearest hostile in its reach itself (autonomous), and its kills are its battery's.
    void UpdateNeighbourFire(double dt) {
        if (!neighbour_fire_) {
            if (neighbour_fire_built_) return;
            neighbour_fire_built_ = true;
            bool any = false;
            for (const ScenarioNeighbour& nb : def_.neighbours) any = any || nb.fights;
            if (!any || !NeighbourFireTuned()) return;
            const EngagementTuning& tuning = def_.has_launcher_tuning ? def_.launcher_tuning : def_.tuning;
            double reach = def_.has_launcher_tuning ? def_.launcher_reach_m : console_.track.max_range_m;
            if (!(reach > 0.0)) reach = 28000.0;
            neighbour_fire_ = std::make_unique<BatteryFire>(tuning, reach);
            neighbour_fire_->SetGround(&world_.GetTerrain(), &world_.Frame());  // they see as our Osa does (roadmap R6)
            neighbour_fire_->SetAutonomous(true);
            neighbour_fire_rng_.seed(def_.seed * 6151u + 11u);
            for (std::size_t j = 0; j < def_.neighbours.size(); ++j) {
                const ScenarioNeighbour& nb = def_.neighbours[j];
                if (!nb.fights) continue;
                const std::vector<Vec3> places = NeighbourLauncherPlaces(nb);
                const int n = static_cast<int>(places.size());
                const int total = nb.missiles < 0 ? 6 * n : std::min(nb.missiles, 6 * n);
                for (int k = 0; k < n; ++k) {
                    const int share = total / n + (k < total % n ? 1 : 0);
                    neighbour_fire_->AddShooter(nb.id + "/" + std::to_string(k + 1), places[static_cast<std::size_t>(k)], share);
                    shooter_neighbour_.push_back(j);
                }
                neighbour_launches_[nb.id] = 0;
                neighbour_radar_s_[nb.id] = 0.0;
            }
        }
        std::vector<BatteryFireTarget> targets;
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
            targets.push_back(t);
        }
        neighbour_fire_->Step(time_, targets, neighbour_fire_rng_, nullptr, [](const std::string&) {});
        const std::vector<BatteryFire::Shooter>& shooters = neighbour_fire_->Shooters();
        for (std::size_t k = 0; k < shooters.size(); ++k) {
            if (shooters[k].phase != BatteryFire::Phase::Idle) neighbour_radar_s_[def_.neighbours[shooter_neighbour_[k]].id] += dt;
        }
        for (const BatteryFire::Event& e : neighbour_fire_->TakeEvents()) {
            std::size_t k = 0;
            while (k < shooters.size() && shooters[k].callsign != e.callsign) ++k;
            if (k == shooters.size()) continue;
            const ScenarioNeighbour& nb = def_.neighbours[shooter_neighbour_[k]];
            if (e.kind == BatteryFire::Event::Kind::Launch) {
                ++neighbour_launches_[nb.id];
                Event(e.target, "missile launched at it by " + nb.name);
                FxSamLaunch(shooters[k], e.target, FxSystem(nb.system));
            } else if (e.kind == BatteryFire::Event::Kind::Hit) {
                FxSamEnd(shooters[k], e.target, FxSystem(nb.system), true);
                if (DestroyTarget(e.target, nb.name)) neighbour_kills_[e.target] = nb.id;
            } else if (e.kind == BatteryFire::Event::Kind::Miss) {
                FxSamEnd(shooters[k], e.target, FxSystem(nb.system), false);
            }
        }
    }
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
            battery_fire_->SetGround(&world_.GetTerrain(), &world_.Frame());  // they see as our Osa does (roadmap R6)
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
                    m.target_id == engagement_.TrackedTarget() || DesignationPending(m.target_id) || (post_ && (ToldNoTargetThere(m) || LauncherHasMark(m.number)))) {
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
            if (sh != nullptr && !sh->external) {  // the feed: its missile, and the wreck of what it hit
                const std::string sys_id = FxSystem(def_.has_launcher_tuning ? def_.launcher_tuning.system_id : def_.tuning.system_id);
                if (e.kind == BatteryFire::Event::Kind::Launch) FxSamLaunch(*sh, e.target, sys_id);
                if (e.kind == BatteryFire::Event::Kind::Hit || e.kind == BatteryFire::Event::Kind::Miss) {
                    FxSamEnd(*sh, e.target, sys_id, e.kind == BatteryFire::Event::Kind::Hit);
                }
                if (e.kind == BatteryFire::Event::Kind::Hit) FxDown(e.target, e.callsign);
            }
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
        bool taken_back = false;  // a Miss taking back the Hit reported before: the target flies on
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
            a->demand = b->demand;  // what the truth's command layer told it (flights.hpp)
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
    // A target destroyed by something outside the engagement model (a neighbour, another launcher, the bridge). The feed
    // gets its wreck (AircraftDown) unless `wreck` is false: a cruise missile gone by its own warhead is no wreck (the
    // bridge shows its CruiseImpact).
    bool DestroyTarget(const std::string& id, const std::string& by, bool wreck = true) {
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            if (def_.targets[i].id != id) continue;
            Aircraft* a = targets_[i].spawned ? world_.FindAircraft(targets_[i].world_id) : nullptr;
            if (a == nullptr || !a->alive) return false;
            a->alive = false;
            Event(id, "destroyed by " + by);
            if (wreck) FxDown(id, by);
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
    // How it moves now (ENU m/s; zero if it has not appeared), and its World id (= Unreal's entity id; -1 if it has not).
    Vec3 TargetVelocity(const std::string& id) const {
        const Aircraft* a = AircraftOf(id);
        return a != nullptr ? a->velocity : Vec3{};
    }
    int TargetWorldId(const std::string& id) const {
        const Aircraft* a = AircraftOf(id);
        return a != nullptr ? a->id : -1;
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
    // Has the target appeared (it may since have gone)?
    bool TargetAppeared(const std::string& id) const {
        for (std::size_t i = 0; i < def_.targets.size(); ++i) {
            if (def_.targets[i].id == id) return targets_[i].spawned;
        }
        return false;
    }

    // ---- The trainer's instructor (roadmap 10.3, instructor.hpp) ----
    // One order. Turning, a height, a speed or a manoeuvre takes the target first (the trainer holds two at a time:
    // flak11, ePap). False, with the reason, if it can't be done: a target not in the sky, a third target, a jammer it
    // doesn't carry, a height or speed out of reach.
    bool Instruct(const InstructorOrder& o, std::string* why = nullptr) {
        const auto fail = [why](const std::string& w) {
            if (why != nullptr) *why = w;
            return false;
        };
        std::size_t i = 0;
        while (i < def_.targets.size() && def_.targets[i].id != o.target) ++i;
        if (i == def_.targets.size()) return fail("no target '" + o.target + "'");
        TargetState& s = targets_[i];
        Aircraft* a = s.spawned && !s.gone ? world_.FindAircraft(s.world_id) : nullptr;
        if (a == nullptr || !a->alive) return fail(o.target + " is not in the sky");
        const std::string& id = o.target;
        if (o.kind == InstructorOrder::Kind::Release) {
            if (!s.held) return fail(id + " is not held");
            ReleaseTarget(s, *a);
            Instructed(id, "lets it go back to its route");
            return true;
        }
        if (o.kind == InstructorOrder::Kind::Jam) {
            if (!em_link_ || !em_link_->SetJammerSwitch(id, o.value != 0.0)) return fail(id + " carries no jammer");
            Instructed(id, o.value != 0.0 ? "switches its jammer on" : "switches its jammer off");
            return true;
        }
        const bool rotor = a->type.find("Rotorcraft") != std::string::npos;
        if (o.kind == InstructorOrder::Kind::Height && (o.value < kHeldMinAglM || o.value > kHeldMaxAglM)) {
            return fail("a height of 5 to 6000 m above the ground");
        }
        if (o.kind == InstructorOrder::Kind::Speed &&
            (o.value < (rotor ? 0.0 : kHeldMinSpeedMps) || o.value > (rotor ? kHeldRotorMaxMps : kHeldMaxSpeedMps))) {
            return fail(rotor ? "a helicopter's speed of 0 to 90 m/s" : "an aircraft's speed of 80 to 700 m/s");
        }
        const bool took = !s.held;
        if (took && !TakeTarget(i, *a, why)) return false;
        char text[160];
        switch (o.kind) {
            case InstructorOrder::Kind::Take:
                Instructed(id, took ? "takes it: it flies on as it is, at his orders" : "holds it already");
                return true;
            case InstructorOrder::Kind::Turn: {
                double deg = std::fmod(o.value, 360.0);
                if (deg < 0.0) deg += 360.0;
                HoldOn(i, DegToRad(deg));
                std::snprintf(text, sizeof(text), "turns it onto %03.0f degrees", deg);
                break;
            }
            case InstructorOrder::Kind::Height:
                s.pilot.SetAgl(o.value);  // the height layer is the pilot's (target_behaviour.hpp)
                std::snprintf(text, sizeof(text), "sends it to %.0f m above the ground", o.value);
                break;
            case InstructorOrder::Kind::Speed:
                a->speed_mps = o.value;  // it keeps his speed when he lets it go, as it always has
                HoldOn(i, HeldHeading(i), o.value);
                std::snprintf(text, sizeof(text), "sets its speed to %.0f m/s", o.value);
                break;
            case InstructorOrder::Kind::Evade: {
                const Evasion ev = s.pilot.EvadeNow(*a, def_.unit_position, time_);
                if (ev.what.empty()) return fail(id + " is manoeuvring already");
                if (ev.steer) flights_->Override(static_cast<int>(i), ev.ov);
                std::snprintf(text, sizeof(text), "makes it manoeuvre: %s", ev.what.c_str());
                break;
            }
            default:
                return fail("unknown order");
        }
        Instructed(id, (took ? std::string("takes it and ") : std::string()) + text);
        return true;
    }

    // Enters a new target into the air picture: it appears now, with the file's keys (its id must be new).
    bool InstructorEnter(ScenarioTarget t, std::string* why = nullptr) {
        const auto fail = [why](const std::string& w) {
            if (why != nullptr) *why = w;
            return false;
        };
        if (t.id.empty()) return fail("a target needs an id");
        for (const ScenarioTarget& o : def_.targets) {
            if (o.id == t.id) return fail("there is a target '" + t.id + "' already");
        }
        if (t.waypoints.empty()) return fail(t.id + " needs a route (one waypoint at least)");
        t.appear_at_s = time_;
        const Vec3 rel = t.start - def_.unit_position;
        double az = RadToDeg(std::atan2(rel.x, rel.y));
        if (az < 0.0) az += 360.0;
        char text[200];
        std::snprintf(text, sizeof(text), "enters %s: %.1f km out at %03.0f degrees, %.0f m up", t.name.c_str(),
                      std::hypot(rel.x, rel.y) / 1000.0, az, t.start.z);
        const std::string id = t.id;
        t.flight.clear();  // a flight of one
        const std::size_t first = def_.targets.size();
        EnterTarget(std::move(t));
        RegisterFlights(first);
        Instructed(id, text);
        return true;
    }

    // Aircraft entering the running session (roadmap 15.3: an enemy package come into the mission live): each appears at
    // its `appear_at_s` (session seconds; one already past appears now) and is then one of the session's targets like
    // the scenario's own. All or none: false, with why, for an id the session has already or twice, or one without a
    // route. `from` names them in the log ("package 12").
    bool AddTargets(std::vector<ScenarioTarget> ts, const std::string& from, std::string* why = nullptr) {
        const auto fail = [why](const std::string& w) {
            if (why != nullptr) *why = w;
            return false;
        };
        std::set<std::string> ids;
        for (const ScenarioTarget& o : def_.targets) ids.insert(o.id);
        for (const ScenarioTarget& t : ts) {
            if (t.id.empty()) return fail("a target needs an id");
            if (!ids.insert(t.id).second) return fail("there is a target '" + t.id + "' already");
            if (t.waypoints.empty()) return fail(t.id + " needs a route (one waypoint at least)");
        }
        if (ts.empty()) return fail("no aircraft");
        double first_s = 1e18;
        const std::size_t first = def_.targets.size();
        for (ScenarioTarget& t : ts) {
            t.appear_at_s = std::max(t.appear_at_s, time_);
            first_s = std::min(first_s, t.appear_at_s);
            EnterTarget(std::move(t));
        }
        RegisterFlights(first);  // their flights (AddFlights first), or each a flight of one
        char text[160];
        std::snprintf(text, sizeof(text), "%zu aircraft come into the mission, the first at %.0f s", ts.size(), first_s);
        log_.Record({time_, DebriefEntry::Kind::Scenario, from, "enters", text});
        return true;
    }

    // ---- The dynamic campaign testbed's waves (scenario "waves", air_war/test_waves.hpp).
    const airwar::TestWaves* Waves() const { return waves_.get(); }
    int WeaponImpacts() const { return weapon_impacts_; }
    // Aircraft that flew into the ground (roadmap R5's safety net under the pilot's floor: always 0 unless a bug).
    int TerrainStrikes() const { return terrain_strikes_; }
    // The testbed's air defence (FireNeighbourDefences, the neighbours that do not fight in the battery fire model): its missiles launched and the aircraft it brought down.
    int DefenceLaunches() const { return nb_launches_; }
    int DefenceKills() const { return nb_kills_; }
    // The guns' rounds and shells fired, the rounds that struck, the shells burst (aaa.hpp), and how often an aircraft's
    // damage rose to each level short of destroyed ("light", "moderate", "heavy": damage.hpp), so many times.
    int DefenceRoundsFired() const { return nb_rounds_fired_; }
    int DefenceRoundHits() const { return nb_round_hits_; }
    int DefenceShellBursts() const { return nb_shell_bursts_; }
    int DefenceDamaged(const std::string& level) const {
        const auto it = nb_damaged_.find(level);
        return it == nb_damaged_.end() ? 0 : it->second;
    }
    // What the testbed's defences did to a target so far (damage.hpp).
    const AircraftDamage* TargetDamage(const std::string& id) const {
        const std::size_t i = TargetIndexOf(id);
        return i < targets_.size() ? &targets_[i].damage : nullptr;
    }
    const std::vector<ScenarioScenery>& Scenery() const { return def_.scenery; }
    // The waves' aircraft and weapons in the sky or still to enter it (weapons not counted).
    int WavesAirborne() const {
        int n = 0;
        for (std::size_t i : wave_targets_) {
            if (!targets_[i].gone && !def_.targets[i].weapon) ++n;
        }
        return n;
    }

    // Triggers entering the running session with a package: its air-raid warning, a C2 source its raid has put out. Only
    // the kinds about the world (the air threat, C2 sources failing or restored) on time or a target of the session: a
    // raid's console orders and conditions were written for another vehicle's crew. All or none: false, with why, for
    // another kind, a condition, an unknown target, or an id the session has already.
    bool AddTriggers(std::vector<ScenarioTrigger> ts, std::string* why = nullptr) {
        const auto fail = [why](const std::string& w) {
            if (why != nullptr) *why = w;
            return false;
        };
        std::set<std::string> ids;
        for (const ScenarioTrigger& o : def_.triggers) ids.insert(o.id);
        for (const ScenarioTrigger& t : ts) {
            if (t.id.empty() || !ids.insert(t.id).second) return fail("trigger '" + t.id + "' is the session's already (or has no id)");
            if (!t.conditions.empty()) return fail(t.id + ": console conditions are another vehicle's");
            bool known = t.target.empty();
            for (const ScenarioTarget& o : def_.targets) known = known || o.id == t.target;
            if (!known) return fail(t.id + ": no target '" + t.target + "' in the session");
            for (const TriggerAction& a : t.actions) {
                const bool world = a.kind == TriggerAction::Kind::AirThreat || a.kind == TriggerAction::Kind::C2Fail ||
                                   a.kind == TriggerAction::Kind::C2Restore;
                if (!world) return fail(t.id + ": only the air threat and C2 sources' failures enter a running session");
            }
        }
        for (ScenarioTrigger& t : ts) {
            def_.triggers.push_back(std::move(t));
            fired_.push_back(false);
        }
        return true;
    }

    // A package's EM gear entering the session: the library entries the session lacks (its jammers, warning receivers,
    // anti-radiation missiles) and its jammers (times on the session's clock, stand-off points in its frame), in the
    // unit's EM world and the post radar's. False, with why, if the session has no EM layer (its aircraft are then the
    // gameplay model's plain targets) or a jammer is not in `lib`. Before AddTargets: a warning receiver is made from
    // the library when its aircraft appears.
    bool AddEmGear(const em::EmLibrary& lib, const std::vector<EmJammerDef>& jammers, const std::string& from, std::string* why = nullptr) {
        const auto fail = [why](const std::string& w) {
            if (why != nullptr) *why = w;
            return false;
        };
        std::vector<EmSensorLink*> links;
        if (em_link_) links.push_back(em_link_.get());
        if (post_em_ && post_em_->Ok()) links.push_back(post_em_.get());
        if (links.empty()) return fail("the session has no EM layer");
        for (const EmJammerDef& j : jammers) {
            if (lib.Jammer(j.jammer_id) < 0) return fail("jammer '" + j.jammer_id + "' is not in the package's library");
        }
        std::string e;
        for (EmSensorLink* l : links) {
            if (!l->MergeLibrary(lib, &e)) return fail(e);
        }
        for (EmSensorLink* l : links) l->AddJammers(jammers);
        if (!jammers.empty()) {
            char text[96];
            std::snprintf(text, sizeof(text), "%zu jammer%s with it", jammers.size(), jammers.size() == 1 ? "" : "s");
            log_.Record({time_, DebriefEntry::Kind::Scenario, from, "enters", text});
        }
        return true;
    }

    // The targets he holds now (at most kInstructorTargets).
    std::vector<std::string> InstructorTargets() const {
        std::vector<std::string> out;
        for (std::size_t i = 0; i < targets_.size(); ++i) {
            if (targets_[i].held && !targets_[i].gone) out.push_back(def_.targets[i].id);
        }
        return out;
    }

    // What the instructor's seat shows: every target in the sky as it truly is (he is not the crew).
    struct InstructorTrack {
        std::string id;
        std::string name;
        Vec3 position;
        double heading_deg = 0.0;
        double speed_mps = 0.0;
        double agl_m = 0.0;
        bool held = false;
        bool has_jammer = false;
        bool jamming = false;
    };
    std::vector<InstructorTrack> InstructorPicture() const {
        std::vector<InstructorTrack> out;
        for (std::size_t i = 0; i < targets_.size(); ++i) {
            const TargetState& s = targets_[i];
            const Aircraft* a = s.spawned && !s.gone ? world_.FindAircraft(s.world_id) : nullptr;
            if (a == nullptr || !a->alive) continue;
            InstructorTrack t;
            t.id = def_.targets[i].id;
            t.name = def_.targets[i].name;
            t.position = a->position;
            t.heading_deg = RadToDeg(std::atan2(a->velocity.x, a->velocity.y));
            if (t.heading_deg < 0.0) t.heading_deg += 360.0;
            t.speed_mps = a->speed_mps;
            t.agl_m = world_.Frame().AltitudeOf(a->position) - world_.GetTerrain().HeightAt(a->position.x, a->position.y);
            t.held = s.held;
            t.has_jammer = em_link_ && em_link_->CarriesJammer(t.id);
            t.jamming = em_link_ && em_link_->TargetJamming(t.id);
            out.push_back(t);
        }
        return out;
    }

private:
    static constexpr double kHeldMinAglM = 5.0;  // game limits on his orders (sam3d)
    static constexpr double kHeldMaxAglM = 6000.0;
    static constexpr double kHeldMinSpeedMps = 80.0;
    static constexpr double kHeldMaxSpeedMps = 700.0;
    static constexpr double kHeldRotorMaxMps = 90.0;

    void Instructed(const std::string& target, const std::string& what) {
        log_.Record({time_, DebriefEntry::Kind::Scenario, "Instructor", target, what});
    }

    // He takes the target: from now on a terrain-following pilot holds its height, it flies his heading (the one it
    // has now) and its speed, and manoeuvres only when he orders it (a jink; its own if it has one).
    bool TakeTarget(std::size_t i, Aircraft& a, std::string* why) {
        int held = 0;
        for (const TargetState& t : targets_) held += t.held && !t.gone ? 1 : 0;
        if (held >= kInstructorTargets) {
            if (why != nullptr) *why = "the trainer holds two targets at a time: let one go first";
            return false;
        }
        TargetState& s = targets_[i];
        const ScenarioTarget& def = def_.targets[i];
        s.route_pilot = s.pilot;
        if (flights_->Overriding(static_cast<int>(i))) {  // its own manoeuvre ends: he flies it now
            flights_->EndOverride(static_cast<int>(i));
            s.route_pilot.OverrideEnded();
        }
        HoldOn(i, std::atan2(a.velocity.x, a.velocity.y));
        TargetBehaviourDef hold;
        hold.kind = TargetBehaviourDef::Kind::TerrainFollowing;
        const double agl = world_.Frame().AltitudeOf(a.position) - world_.GetTerrain().HeightAt(a.position.x, a.position.y);
        hold.agl_m = std::max(kHeldMinAglM, agl);
        hold.max_climb_mps = def.behaviour.kind == TargetBehaviourDef::Kind::None ? 30.0 : std::max(5.0, def.behaviour.max_climb_mps);
        EvadeDef jink;
        if (def.evade.present && def.evade.kind == EvadeDef::Kind::Jink) jink = def.evade;
        jink.kind = EvadeDef::Kind::Jink;
        if (!(def.evade.present && def.evade.kind == EvadeDef::Kind::Jink)) {
            jink.turn_rate_rad_s = DegToRad(10.0);  // a placeholder jink (sam3d)
            jink.jink_period_s = 4.0;
            jink.evade_s = 15.0;
        }
        jink.present = true;
        jink.on = EvadeDef::On::Order;
        s.pilot = TargetPilot(hold, jink);
        s.held = true;
        return true;
    }

    // His heading for a held target: an order to it as a flight of one (a vector, until he says otherwise), out of its
    // flight's formation while he holds it.
    void HoldOn(std::size_t i, double heading_rad, double speed_mps = -1.0) {
        FlightOrder o;
        o.kind = OrderKind::Divert;
        o.has_heading = true;
        o.heading_rad = heading_rad;
        const FlightSlot* s = flights_->SlotOf(static_cast<int>(i));
        o.speed_mps = speed_mps >= 0.0 ? speed_mps : (s != nullptr ? s->order.speed_mps : 0.0);
        o.by = "instructor";
        flights_->OrderMember(static_cast<int>(i), o, time_, true, "the instructor takes the lead");
    }
    double HeldHeading(std::size_t i) const {
        const FlightSlot* s = flights_->SlotOf(static_cast<int>(i));
        return s != nullptr ? s->order.heading_rad : 0.0;
    }

    // He lets it go: its own pilot again, back into its flight, and on along its route from the waypoint it was flying
    // to when he took it (never overwritten: held, it passes none).
    void ReleaseTarget(TargetState& s, Aircraft& a) {
        s.held = false;
        s.pilot = s.route_pilot;
        a.max_turn_rate_rad_s = std::max(a.max_turn_rate_rad_s, 1e-3);
        flights_->Rejoin(static_cast<int>(&s - targets_.data()));
    }

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

    // The battery commander at the post writes each launch and result he hears in his log (P9: the combat report's claims).
    void LogLauncher(const LinkReport& r, const char* kind) {
        if (post_) commander_log_.push_back({time_, r.callsign, kind, r.number});
    }
    // What a launcher's report by number means to the post: the newest mark of that number on its board (the launcher may
    // have found another aircraft where it was told to look); the report's own target where the board has none of it.
    std::string ReportedTarget(const LinkReport& r) const {
        if (!post_ || r.number <= 0) return r.target;
        const BoardMark* newest = nullptr;
        for (const BoardMark& m : post_->Board().Marks()) {
            if (m.number == r.number && (newest == nullptr || m.time_s > newest->time_s)) newest = &m;
        }
        return newest != nullptr ? newest->target_id : r.target;
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
                if (hears && callouts_ && !OrderNetReports(r.callsign)) {
                    callouts_->Say(time_, "report_ready", "battery", {{"callsign", r.callsign}, {"missiles", std::to_string(r.missiles)}});
                }
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
                if (hears) LogLauncher(r, "launch");
                break;
            case BatteryFire::Event::Kind::Hit:
                Event(r.callsign, "destroys " + r.target);
                if (hears) {
                    LogLauncher(r, "destroyed");
                    HeardDestroyed(ReportedTarget(r));
                    if (callouts_) callouts_->Say(time_, "report_result", "battery", {{"callsign", r.callsign}, {"number", number}, {"spent", "1"}});
                    Status(r.callsign, r.missiles > 0 ? LauncherStatus::State::Ready : LauncherStatus::State::Empty, r.missiles, 0);
                }
                break;
            case BatteryFire::Event::Kind::Miss:
                if (r.taken_back) {  // the kill it reported flies on: back on the board
                    Event(r.callsign, "takes back the kill of " + r.target);
                    if (hears) {
                        LogLauncher(r, "not destroyed");
                        reported_kills_.erase(ReportedTarget(r));
                        if (callouts_) callouts_->Say(time_, "report_not_destroyed", "battery", {{"callsign", r.callsign}, {"number", number}});
                    }
                    break;
                }
                Event(r.callsign, "misses " + r.target);
                if (hears) Status(r.callsign, LauncherStatus::State::Engaging, r.missiles, r.number);
                if (hears) LogLauncher(r, "missed");
                break;
            case BatteryFire::Event::Kind::Empty:
                Event(r.callsign, "out of missiles");
                if (hears && callouts_) callouts_->Say(time_, "request_tzm", "battery", {{"callsign", r.callsign}});
                if (hears) Status(r.callsign, LauncherStatus::State::Empty, 0, 0);
                break;
            case BatteryFire::Event::Kind::Reloaded:
                Event(r.callsign, "reloaded");
                if (hears) {
                    TzmGave(r.callsign, r.missiles);  // before the row takes the new load
                    Status(r.callsign, LauncherStatus::State::Ready, r.missiles, 0);
                }
                break;
        }
    }

    // The player's Osa in a battery whose commander sits in a linked PU-12M: its commander reports on the battery net as
    // the abstract launchers do (flak11: readiness, launch, result), and the link carries it to the post's IT boards.
    void OwnReport(BatteryFire::Event::Kind kind, const std::string& target) {
        if (!LinkedToPost()) return;
        std::string why;
        if (!CanTransmitNow(&why)) {  // the report goes by radio: with the set unable to transmit the post never hears it
            TransmitBlocked(why);
            return;
        }
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
        if (kind == BatteryFire::Event::Kind::Hit) results_reported_.insert(target);
        if (!callouts_ || !NpcAt(Cmdr())) {
            OnAir(1.0);  // a player's words go out as he speaks them; the data reaches the post
            return;
        }
        char n[16];
        std::snprintf(n, sizeof(n), "%04d", r.number);
        const std::string number = r.number > 0 ? std::string(n) : target;
        if (kind == BatteryFire::Event::Kind::Ready) {
            std::map<std::string, std::string> f = RadioFields();
            f["missiles"] = std::to_string(r.missiles);
            SayOnRadio("report_ready", f, false);
        } else if (kind == BatteryFire::Event::Kind::Launch) {
            SayOnRadio("report_launch", {{"callsign", r.callsign}, {"number", number}}, false);
        } else if (kind == BatteryFire::Event::Kind::Hit) {
            const auto spent = npc_launched_at_.find(target);
            SayOnRadio("report_result", {{"callsign", r.callsign}, {"number", number},
                                         {"spent", std::to_string(spent != npc_launched_at_.end() ? spent->second : 1)}}, false);
        } else {
            OnAir(1.0);
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
    bool post_radar_off_ = false;  // the cabled radar off the air (its crew rang the post on the service line)
    bool post_sending_ = false;    // the post sending its picture on as telecode
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
    // A launcher's «цели не наблюдаю» heard at the post: the mark (or the group's marks) he gave it, where that stood on
    // his picture and where he told it to look, and when. That mark, or one within its search of either place, no newer
    // than that is the picture's lag (a square reported late, a stale track), not a target: he gives it to nobody until
    // a fresh mark comes. Dropped once older than any mark he still reads.
    struct NoTargetThere {
        std::vector<std::string> marks;
        Vec3 place;  // the mark on his picture, unled
        Vec3 point;  // where he told the launcher to look
        double heard_s = 0.0;
    };
    std::vector<NoTargetThere> no_target_there_;
    bool ToldNoTargetThere(const BoardMark& m) {
        no_target_there_.erase(std::remove_if(no_target_there_.begin(), no_target_there_.end(),
                                              [this](const NoTargetThere& n) { return n.heard_s < time_ - kMarkReadS; }),
                               no_target_there_.end());
        for (const NoTargetThere& n : no_target_there_) {
            if (m.time_s > n.heard_s) continue;
            if (std::find(n.marks.begin(), n.marks.end(), m.target_id) != n.marks.end() ||
                (m.position - n.place).HorizontalLength() < kDesignationSearchM ||
                (m.position - n.point).HorizontalLength() < kDesignationSearchM) {
                return true;
            }
        }
        return false;
    }
    // A launcher's read-back heard at the post: it has that mark, whichever aircraft it found there. He gives the mark to
    // nobody else while the launcher's last word on it (the read-back, or its reports on the IT boards under that number)
    // is fresh, until it reports on something else.
    static constexpr double kReadBackHoldS = 60.0;
    struct ReadBackHeard {
        std::string callsign;
        int number = 0;
        double heard_s = 0.0;
    };
    std::vector<ReadBackHeard> readbacks_;
    bool LauncherHasMark(int number) {
        readbacks_.erase(std::remove_if(readbacks_.begin(), readbacks_.end(),
                                        [this](const ReadBackHeard& rb) {
                                            double last_s = rb.heard_s;
                                            for (const LauncherStatus& s : status_boards_) {
                                                if (s.callsign != rb.callsign || s.reported_s <= rb.heard_s) continue;
                                                if (s.target_number != rb.number) return true;
                                                last_s = s.reported_s;
                                            }
                                            return last_s < time_ - kReadBackHoldS;
                                        }),
                         readbacks_.end());
        for (const ReadBackHeard& rb : readbacks_) {
            if (rb.number == number) return true;
        }
        return false;
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
    static constexpr double kMarkReadS = 120.0;
    std::vector<BoardMark> PostMarks() const {
        std::vector<BoardMark> out;
        if (!post_) return out;
        for (const BoardMark& m : post_->Board().Marks()) {
            if (time_ - m.time_s > kMarkReadS || reported_kills_.count(m.target_id) != 0) continue;
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
                // Heard at the post (as a read-back would be), his marks there are the picture's lag: he holds them back.
                if (d.by_line || (LaunchersCanTransmit() && NetHears() && (!nets_ || nets_->GetsThrough("battery", time_)))) {
                    no_target_there_.push_back({d.group ? d.group_ids : std::vector<std::string>{d.mark.target_id}, d.mark.position, d.point, time_});
                }
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
                readbacks_.push_back({d.callsign, d.mark.number, time_});
            } else if (LaunchersCanTransmit() && NetHears() && (!nets_ || nets_->GetsThrough("battery", time_))) {
                if (callouts_) callouts_->Say(time_, "readback", "battery", {{"post", OwnCallSignOn("battery")}, {"callsign", d.callsign}, {"number", rn}}, 1.0);
                ++readbacks_heard_;
                readbacks_.push_back({d.callsign, d.mark.number, time_});
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
    // Our other air defences near the scenario (drawn where they stand), and the ground's height (ENU up) anywhere on it.
    const std::vector<ScenarioNeighbour>& Neighbours() const { return def_.neighbours; }
    double GroundHeightAt(double east_m, double north_m) const { return world_.GetTerrain().HeightAt(east_m, north_m); }
    // The point on the ground under (east, north), in ENU: the terrain's height is an altitude, and ENU up is not altitude
    // far from the origin (the Earth curves away: 70 m at 30 km). Its x and y are (east, north) exactly; its z is the
    // frame's up to the ground there (roadmap R5: the vertical the game's ground is drawn on, geo.hpp GroundPointInFrame).
    Vec3 GroundEnu(double east_m, double north_m) const {
        return GroundPointInFrame(world_.Frame(), east_m, north_m, GroundHeightAt(east_m, north_m));
    }
    // A neighbour that fights in the battery fire model here (an Osa battery with "fights", and the Osa's tuning to fight
    // by); the mission bridge leaves its chances to the runner.
    bool NeighbourFights(const std::string& id) const {
        if (!NeighbourFireTuned()) return false;
        for (const ScenarioNeighbour& nb : def_.neighbours) {
            if (nb.id == id) return nb.fights;
        }
        return false;
    }
    // The aircraft the fighting neighbours shot down (target -> neighbour id), their missiles launched, the time their
    // launchers had a target (their radars on it).
    const std::map<std::string, std::string>& NeighbourKills() const { return neighbour_kills_; }
    int NeighbourLaunches(const std::string& id) const {
        const auto it = neighbour_launches_.find(id);
        return it == neighbour_launches_.end() ? 0 : it->second;
    }
    double NeighbourRadarSeconds(const std::string& id) const {
        const auto it = neighbour_radar_s_.find(id);
        return it == neighbour_radar_s_.end() ? 0.0 : it->second;
    }
    // Where the fighting neighbours' launchers stand (neighbour id, ENU), to draw them there.
    std::vector<std::pair<std::string, Vec3>> NeighbourLaunchers() const {
        std::vector<std::pair<std::string, Vec3>> out;
        for (const ScenarioNeighbour& nb : def_.neighbours) {
            if (!NeighbourFights(nb.id)) continue;
            for (const Vec3& at : NeighbourLauncherPlaces(nb)) out.push_back({nb.id, at});
        }
        return out;
    }

    PostFeed PostFeedNow() const {
        PostFeed f;
        if (!post_) return f;
        f.post = true;
        f.scale_m = PostScaleM();
        f.position = post_->Position();
        f.scope = PostScope();
        f.strobes = PostStrobes();
        const PlottingBoard& b = post_->Board();
        const std::vector<BoardMark> now = PostMarks();
        for (const BoardMark& m : b.Marks()) {
            bool current = false;
            for (const BoardMark& c : now) current = current || c.target_id == m.target_id;
            if (!current || m.stamp + 4 < b.Stamp()) continue;
            f.glass.push_back({m.number, m.stamp, m.height_hm, m.Code(), m.position});
        }
        f.launchers = status_boards_;
        for (const ScenarioNeighbour& nb : def_.neighbours) f.neighbours.push_back({nb.name, nb.system, nb.position, nb.radius_m});
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
        for (int n : PlotterReportNumbers()) f.heard.push_back(n);  // heard, or there to read in the journal
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
    std::vector<CrewCall> crew_calls_;
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
    int kill_calls_ = 0, kill_calls_wrong_ = 0, still_flying_ = 0, kill_reports_taken_back_ = 0;
    std::set<std::string> results_reported_;  // targets the commander reported destroyed on the battery net
    std::set<std::string> corrections_due_;   // ...that the scope then showed flying on: the report to take back
    // A target reported destroyed on the battery net that flies on: the commander takes the report back on the net, so
    // the post puts it back on its board (voenizdat_osa_1991 p267: a target whose motion is unchanged is not destroyed).
    void CorrectKillReport(const std::string& target) {
        std::map<std::string, std::string> f = RadioFields();
        const int number = BoardNumberOf(target);
        char n[16];
        std::snprintf(n, sizeof(n), "%04d", number);
        f["number"] = number > 0 ? std::string(n) : target;
        SeatDuty d = SeatDuty::Report(Cmdr(), "report_not_destroyed", "a target reported destroyed flies on: take the report back on the battery net", f);
        d.target = target;
        if (NpcDoes(Cmdr(), d) && SayOnRadio("report_not_destroyed", f, false)) KillReportTakenBack(target);
    }
    // The correction went on the air: a linked post hears it.
    void KillReportTakenBack(const std::string& target) {
        results_reported_.erase(target);
        ++kill_reports_taken_back_;
        Event(target, "the kill report taken back on the battery net: it flies on");
        if (!LinkedToPost()) return;
        LinkReport r;
        r.kind = BatteryFire::Event::Kind::Miss;
        r.callsign = OwnCallsign();
        r.target = target;
        r.number = BoardNumberOf(target);
        r.missiles = launcher_ ? launcher_->LoadedCount() : -1;
        r.taken_back = true;
        own_reports_.push_back(r);
    }
    // The target's number on the post's board, from its designation or a group's pick (0: none heard).
    int BoardNumberOf(const std::string& target) const {
        int number = 0;
        for (const C2Network::Event& e : designations_heard_) {
            if (e.subject == target && !e.group_centre) number = e.number;
        }
        for (const GroupOrder& g : group_orders_) {
            if (g.pick == target) number = g.number;
        }
        return number;
    }
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
            if (const SkillTier* t = SkillOf(def_.crew[i])) return t->perception_s * TierStress(*t, crew_[i]);
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
            crew_calls_.push_back({time_, j.call == KillAssessment::Call::Hit ? "destroyed" : j.call == KillAssessment::Call::Miss ? "missed" : "unknown", j.target});
            if (j.call == KillAssessment::Call::Hit) {
                if (regime_cmdr_.kind == EmissionKind::Arm) npc_ssc_off_due_ = true;  // under the least emission the tracking transmitter goes off after the burst
                Say("hit", judge);
                HeardDestroyed(j.target);
                own_kills_[j.target] = time_;
                OwnReport(BatteryFire::Event::Kind::Hit, j.target);
                // Without a linked post the commander reports the result on the battery net himself (7.8).
                if (!LinkedToPost() && (battery_ || c2_)) {
                    std::map<std::string, std::string> f = RadioFields();
                    f["number"] = j.target;
                    for (const C2Network::Event& d : designations_heard_) {
                        char n[16];
                        std::snprintf(n, sizeof(n), "%04d", d.number);
                        if (d.subject == j.target && d.number > 0) f["number"] = n;
                    }
                    f["spent"] = std::to_string(launches_);
                    SeatDuty d = SeatDuty::Report(Cmdr(), "report_result", "a target destroyed: report it on the battery net", f);
                    d.target = j.target;
                    if (NpcDoes(Cmdr(), d) && SayOnRadio("report_result", f, false)) results_reported_.insert(j.target);
                }
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
        for (const std::string& t : corrections_due_) CorrectKillReport(t);
        corrections_due_.clear();
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
    std::mt19937 relay_rng_{211u};  // the commander's slips passing a designation on (a stream of its own: 7.2)
    int say_agains_ = 0;
    double next_contact_update_s_ = 0.0;

    // The seats' own instruments: the senior operator's scope, the operator's range indicator.
    void SeeInstruments() {
        if (!engagement_on_ || !def_.has_crew_data) return;
        for (const Contact& c : engagement_.SearchPicture()) {
            if (arms_known_.count(c.target_id) != 0) continue;  // an ARM's mark he has called: not a target
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
        if (s.id == "air_report" && post_) {  // read out to the plotter: it waits for his hand now, as it was read
            const auto it = reading_out_.find(s.about.number);
            if (it != reading_out_.end()) {
                post_->Recall(it->second, time_, s.about.number);
                reading_out_.erase(it);
            }
            return;
        }
        if (s.channel != "intercom" && s.channel != "cabin") return;
        NoteStressHeard(s.from, s.id);  // he knows what he said himself
        for (std::size_t i = 0; i < crew_.size(); ++i) {
            CrewMember& c = crew_[i];
            if (c.Role() == s.from) continue;
            if (!HeardBy(s, c.Role())) continue;
            NoteStressHeard(c.Role(), s.id);
            const bool addressed = s.to == "crew" || s.to == "any" || s.to == c.Role();
            if (!addressed) continue;
            const SkillTier* t = i < def_.crew.size() ? SkillOf(def_.crew[i]) : nullptr;
            const bool npc = SeatHolder(c.Role()).empty();
            // Only a seat with a stake in it asks again: the one it was said to, the commander, the aiming seats.
            const bool stake = s.to == c.Role() || c.Role() == Cmdr() || c.Role() == def_.tuning.aim_az_role ||
                               c.Role() == def_.tuning.aim_el_role || c.Role() == def_.tuning.aim_range_role;
            if (npc && stake && t != nullptr && s.has_about && s.repeats == 0) {
                // A man at his hand wheels, following a target, takes in less of what he hears (sam3d: half again).
                bool at_wheels = false;
                for (const WheelWatch& w : wheels_) at_wheels = at_wheels || (w.station == c.Role() && time_ - w.moved_s < 0.5);
                const double miss = t->miss_chance * TierStress(*t, c) * (at_wheels ? 1.5 : 1.0);
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
                if (results_reported_.count(s.about.target) != 0) corrections_due_.insert(s.about.target);
                npc_dropped_kill_.clear();
            }
            if (!s.has_about) continue;
            // The signs of an ARM launch (the ARM drill): the ARM's mark is not a target; the commander acts on either (a
            // player in his seat heard it too: the shadow commander thinks on it, and his acts are the player's duties).
            if (s.id == "arm_launch" || s.id == "arm_turn_away") {
                if (s.id == "arm_launch") pictures_[c.Role()].Forget(s.about.target);
                if (c.Role() == Cmdr()) {
                    if (s.id == "arm_launch") arms_heard_.insert(s.about.target);
                    ArmSignHeard(s);
                }
                continue;
            }
            if (s.id == "arm_launch_track" || s.id == "arm_launch_tv") {  // about the tracked target that fired it
                if (c.Role() == Cmdr()) ArmSignHeard(s);
                continue;
            }
            KnownTarget k = s.about;
            k.time_s = s.start_s;
            k.source = "told by " + SpeakerName(s.from);
            pictures_[c.Role()].Learn(k);
            if (s.id == "iff_no_reply") {  // the other echoes the one "Ответа нет!" covered
                const auto also = iff_companions_.find(s.about.target);
                if (also != iff_companions_.end()) {
                    for (KnownTarget o : also->second) {
                        o.time_s = s.start_s;
                        o.source = k.source;
                        pictures_[c.Role()].Learn(o);
                    }
                }
            }
        }
    }

    // Stress (docs/crew/05_SKILL.md, docs/crew/15_TALK.md section 4): a level per crewman, 0 calm to 1, from what he
    // knows: a war (not an exercise), the crew's own radars jammed, a hostile the commander knows of inside 10 km with
    // nothing tracked, our missile in the air, an ARM bursting near the vehicle, and the warnings and reports he heard
    // (kStressHeard). It rises at once to the strongest of them and falls back slowly (kStressFallS); what it does is
    // scaled by his tier's stress factor (CrewMember::StressMultiplier, TierStress). sam3d estimates throughout.
    struct StressSign {
        const char* callout;
        double level;
        double for_s;  // how long hearing it keeps him at that level
    };
    static constexpr double kStressFallS = 40.0;
    static constexpr double kNearBurstM = 500.0;  // an ARM bursting within this of the vehicle: felt by everyone in it
    std::map<std::string, std::map<std::string, double>> stress_heard_;  // seat -> callout -> when he last heard (or said) it
    double stress_updated_s_ = -1.0;
    double near_burst_s_ = -1e9;
    static double TierStress(const SkillTier& t, const CrewMember& m) { return 1.0 + (t.stress_factor - 1.0) * m.StressLevel(); }
    void NoteStressHeard(const std::string& seat, const std::string& callout) { stress_heard_[seat][callout] = time_; }
    void UpdateStress() {
        static const StressSign kStressHeard[] = {
            {"air_alert", 0.5, 120.0},      {"target_near_zone", 0.4, 30.0}, {"target_in_zone", 0.5, 30.0},
            {"jamming", 0.6, 30.0},         {"lock_lost", 0.5, 20.0},        {"launch", 0.6, 20.0},
            {"missile_away", 0.6, 20.0},    {"arm_turn_away", 0.8, 60.0},    {"arm_launch", 0.9, 90.0},
            {"arm_launch_track", 0.9, 90.0}, {"arm_launch_tv", 0.9, 90.0}};
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
        for (const EmSensorLink::ArmResult& r : arm_results_) {
            if ((r.hit_what == "unit" || r.nearest_what == "unit") && r.miss_m >= 0.0 && r.miss_m < kNearBurstM) near_burst_s_ = r.burst_s;
        }
        const double base = def_.setting == "war" ? 0.2 : 0.0;
        const double dt = stress_updated_s_ < 0.0 ? 0.0 : time_ - stress_updated_s_;
        stress_updated_s_ = time_;
        const double fall = std::exp(-dt / kStressFallS);
        const bool in_flight = engagement_on_ && engagement_.ShotsInFlight();
        for (CrewMember& c : crew_) {
            double want = base;
            if (jammed || pressed) want = std::max(want, 0.7);
            if (in_flight) want = std::max(want, 0.6);
            if (time_ - near_burst_s_ < 120.0) want = 1.0;
            const auto heard = stress_heard_.find(c.Role());
            if (heard != stress_heard_.end()) {
                for (const StressSign& sign : kStressHeard) {
                    const auto it = heard->second.find(sign.callout);
                    if (it != heard->second.end() && time_ - it->second < sign.for_s) want = std::max(want, sign.level);
                }
            }
            const double now = c.StressLevel();
            c.SetStressLevel(want >= now ? want : want + (now - want) * fall);
        }
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

    // The player's quick replies (docs/crew/15_TALK.md section 5, the reply keys): «Есть!» and «Выполнено!» as any seat
    // says them; «Повторите!» asks whoever said the last line that reached his seat for him or the crew (in the last 30 s)
    // to say it again, as an NPC asks (Hear). False with nothing to reply to (LastRefusal says why).
    bool PlayerReply(const std::string& station, const std::string& reply) {
        last_refusal_.clear();
        if (!callouts_ || (reply != "ack" && reply != "done" && reply != "say_again")) return false;
        if (reply != "say_again") {
            if (!callouts_->Say(time_, reply, station)) return false;
            reports_made_.push_back({time_, station, reply});
            return true;
        }
        const SpokenCallout* last = nullptr;
        for (const SpokenCallout& s : callouts_->Log()) {
            const bool to_him = s.to == station || s.to == "crew" || s.to == "any";
            if (s.from != station && s.id != "say_again" && to_him && time_ - s.end_s < 30.0 && CalloutHeardAt(s, station)) last = &s;
        }
        if (last == nullptr) {
            last_refusal_ = "nothing said to you in the last 30 s";
            return false;
        }
        const SpokenCallout again = *last;
        if (!callouts_->Say(time_, "say_again", station)) return false;
        ++say_agains_;
        if (SeatHolder(again.from).empty()) callouts_->Repeat(again, time_, 1.0 + SpeakDelayS(again.from));
        return true;
    }

    // A player in a seat speaks a report of the crew data (the UI's report buttons): what it says comes from his own
    // instruments (a contact: the nearest echo on his scope). False if there is no such callout or no crew data.
    // A paper kept in the cabin (a cabin display of kind "document"), line by line as typed, in Russian as issued.
    // "radio_data": the radiotelephonist's extract of the radio data on its form (rrs_1986, vst_1988: each net's
    // frequencies by number, the call signs, the times they change, the mode of communication, where needed the azimuth
    // to a correspondent), with the telecode channel and the RTK-1's lines. "talk_tables": the commander's ПТ-1 and the
    // signal table ПТ-2 (bstudy_tactics' form: a group, its meaning, a signal's visible sign). Times are counted from the
    // exercise's start (Ч+minutes): the scenarios keep no clock.
    std::vector<std::string> DocumentText(const std::string& document) const {
        if (document == "radio_journal") return journal_.Page(time_, 12);
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
            if (!LineLaid(l)) continue;
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
            f["report"] = "(нет донесения / no report waiting)";
            for (const CommandPost::AirReport& r : post_ ? post_->Held() : std::vector<CommandPost::AirReport>()) {
                if (console_.post_journal_keeper.empty() || InJournal(r)) {
                    f["report"] = r.text;
                    break;
                }
            }
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
        if (callout_id == "report_arm_attack" && station == Cmdr()) {  // the last sign he heard (ArmSignHeard)
            f["callsign"] = OwnCallsign();
            if (!arm_sign_az_.empty()) f["azimuth"] = arm_sign_az_;
        }
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
        if (callout_id == "order_readback" || callout_id == "report_ready_to_march") {  // a battery order on the net (G10)
            if (station != Cmdr() || OwnOwesOnNet() != callout_id) {
                last_refusal_ = callout_id == "order_readback" ? "no battery order heard to read back"
                                                               : "nothing to report: the vehicle is not stowed and on its way";
                return false;
            }
            if (!SayOnRadio(callout_id, OrderFieldsFor(OwnCallsign(), OwnOwedOrder(callout_id)), true)) return false;
            SettleOwedReport(station, callout_id, std::string());
            reports_made_.push_back({time_, station, callout_id});
            return true;
        }
        if (station == "commander") {  // a radio callout of his goes on the air, by his chest switch at ПРД (7.8)
            const CalloutDef* cd = def_.crew_data.FindCallout(callout_id);
            if (cd != nullptr && cd->kind == CalloutKind::Radio) {
                std::map<std::string, std::string> f = RadioFields();
                std::string target;  // a report he owes says what his NPC would have (the target's number)
                for (const SeatDuty& d : owed_) {
                    if (d.kind != SeatDuty::Kind::Report || d.seat != station || d.id != callout_id) continue;
                    for (const auto& kv : d.fields) f[kv.first] = kv.second;
                    target = d.target;
                    break;
                }
                if (callout_id == "report_arm_attack") {  // the sign he heard last (ArmSignHeard)
                    if (arm_sign_az_.empty()) {
                        last_refusal_ = "no sign of an ARM launch heard";
                        return false;
                    }
                    if (f.count("azimuth") == 0) f["azimuth"] = arm_sign_az_;
                }
                if (callout_id == "request_tzm" && (!launcher_ || launcher_->LoadedCount() > 0)) {
                    last_refusal_ = "the launcher still has missiles";
                    return false;
                }
                if (callout_id == "request_fuel" && (!fuel_ || !fuel_->Requested())) {
                    last_refusal_ = "the tank is not down to its reserve";
                    return false;
                }
                if (!SayOnRadio(callout_id, f, true)) return false;
                if (callout_id == "request_tzm") tzm_asked_ = true;
                if (callout_id == "request_fuel") fuel_asked_ = true;
                SettleOwedReport(station, callout_id, target);
                if (callout_id == "report_arm_attack") ArmAttackReported(f["azimuth"]);
                if (callout_id == "report_result" && !target.empty()) results_reported_.insert(target);
                if (callout_id == "report_not_destroyed" && !target.empty()) KillReportTakenBack(target);
                return true;
            }
        }
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
    SessionDeliveries deliveries_;
    SessionLog log_;
    double time_ = 0.0;
    SunLight light_;
    bool light_init_ = false;
    double light_next_s_ = 0.0;
    double next_acmi_s_ = 0.0;
    int launches_ = 0;
    bool finished_ = false;
    std::string end_reason_;
};

}  // namespace simcore
