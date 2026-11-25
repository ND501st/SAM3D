// simcore/campaign/mission_session.hpp — the battery's mission as one session with the enemy's air war in step
// (roadmap 15.2, first form; docs/campaign/05_THE_SYSTEMS.md §6).
//
// A battery mission (a scenario with the regiment's "task", phase 14) plays in one ScenarioRunner from the order to
// its end, and the enemy's air campaign runs beside it on the same clock: the campaign's hour at the session's start is
// the scenario's clock on the war's day, the campaign is run to there first (the packages before it resolved without
// the player), and from then on the packages it hands our battery enter the session live
// (air_war/package_injector.hpp's LivePackages). Our battery's move is the campaign's once it has arrived: until then
// the enemy's picture, and so its aim, has it where it stood.
//
// The regiment tasks the battery in the same session (roadmap 14.2, campaign/regiment.hpp): every minute it reads what
// reached it, our intelligence's estimates in the campaign's journal, and gives the battery its next task on the
// regiment's net (ScenarioRunner::GiveTask), which the battery commander plans from where his battery stands.
//
// The player's two outcomes (roadmap 14.6, campaign/outcomes.hpp) are kept beside it from the truth: the task's covered
// object while the battery stands ready over it, and each package back from the session; the debrief ends with them.
//
// Our radars and warning posts follow the war through the session (roadmap 17.3): the regiment's radars in the theatre
// join the post's feed beside the scenario's own, and each of them, with every warning source of the vehicle's standing
// at a theatre object, is off the air while the war has it struck and on it again once repaired. The regiment tells the
// battery on its command net: at the start what is already out, later each change.
//
// Track F, F4.4 (docs/fronts/04_AIR_DEFENCE_STRATEGY.md section 4): where the ground war and our air defence commanders run
// (ground.mode and ad_strategy.mode 1, MissionSessionDef::fronts), the session keeps the campaign's pipeline (an InfoNet
// on our chain and the CampaignFeed, pumped as the campaign advances and asked for the units' orders: F4.2-F4.3), and the
// regiment's tasking reads what reached its posts (campaign/regiment.hpp's RegimentWatch): the covered division's
// withdrawal, its sector's priority and the sky's sharing there, read again only when a fact reaches the regiment. Its
// tasks go to the battery on the regiment's net as every task (GiveTask), the sharing as a word on it
// (ScenarioRunner::RegimentTellsSharing). At the modes 0 none of it is made: the session is as before.
//
// Track F, F5.2 (built as F8.3; ground_war/session_units.hpp): with the ground war on and the session's roads, the war's
// regiments near the battery come down to U2, their battalions and companies the session's movers at their places and on
// the roads, following the war; the session draws those within the visual range, the enemy's only then. The front's
// artillery heard from afar and its fires glowing at night go into the session's fx feed for show. Without the ground war
// or the roads none of it is made.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/air_war/data.hpp"
#include "simcore/air_war/package_injector.hpp"
#include "simcore/air_war/save.hpp"
#include "simcore/command/air_war_feed.hpp"
#include "simcore/command/info_net.hpp"
#include "simcore/command/intercept.hpp"
#include "simcore/command/own_air.hpp"
#include "simcore/command/held_situation.hpp"
#include "simcore/command/situation_map.hpp"
#include "simcore/campaign/battery_plan.hpp"
#include "simcore/campaign/battery_state.hpp"
#include "simcore/campaign/outcomes.hpp"
#include "simcore/campaign/regiment.hpp"
#include "simcore/geo.hpp"
#include "simcore/ground_war/session_units.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

namespace simcore {
namespace campaign {

struct MissionSessionDef {
    std::string scenario;                               // the battery mission's file
    std::string faction;                                // the enemy ("nato", "warsaw_pact", ...); "" no air war
    std::string theatre = "air_war/theatre_pannonia.json";
    unsigned seed = 1;                                  // the war's
    int day = 1;                                        // the war's day the mission's clock falls on
    std::string tasking;                                // the regiment's tasking rules ("campaign/tasking.json"); "" none
    double hours = 0.0;                                 // the session's length from its start (0: the scenario's own end)
    // The war's light mode (air_war.json's light.mode, roadmap S8). 0 by default: the tuned theatre undated, its parts
    // of the day the buckets, because the bridges mission (19:00, a package on the march) and its tests were built on
    // the buckets' dusk (18-21); by the sun that dusk is 16:13-17:25 and its package comes before the session starts.
    // -1: the war data's own (1, the theatre dated). docs/LIGHT.md section 7.
    int light_mode = 0;
    // Track F (F4.4): the fronts on, ground.mode and ad_strategy.mode at 1 and air_front.mode at 2 (F7.3: the air front
    // follows its own ground offensive, as the benches run it) (a theatre with a country map and fronts with lines:
    // theatre_pannonia_nation.json); false: the war data's own modes.
    bool fronts = false;
    // F5.2: with the fronts, the war's units this near the battery come down to U2 as the session's movers (sam3d: 20 km,
    // past the visual range by day; a test or the demo may widen it where the battery stands far from the line).
    double u2_radius_m = 20000.0;
    // Track O, O1 (campaign_save.hpp): a campaign resumed. The air war's save text (air_war/save.hpp), replayed instead of
    // started: its data files, seed and modes are the save's (`theatre`, `seed`, `day`, `light_mode` and `fronts` are not
    // read), the session's clock its hour. And the battery as it was (battery_state.hpp; null: the mission's own).
    std::string air_war_save;
    const BatteryState* battery = nullptr;
    // The game's campaign: the session takes a seam at the regiment's evening briefing (TakeSeam).
    bool seams = false;
    // Resumed at a briefing's seam: the briefing is held at the first decision (it was due when the save was taken, which
    // may have been after its hour, once the sky was clear).
    bool briefing_due = false;
};

class MissionSession {
public:
    MissionSession() = default;
    MissionSession(const MissionSession&) = delete;
    MissionSession& operator=(const MissionSession&) = delete;
    ~MissionSession() {
        if (campaign_ && feed_) campaign_->SetOrderSource(nullptr);  // the feed goes first
    }

    // False, with why, if the scenario or the war does not load, or the scenario has no clock to put the war on.
    bool Start(const MissionSessionDef& d, const std::string& data_dir, std::string* error = nullptr) {
        const auto fail = [error](const std::string& w) {
            if (error != nullptr) *error = w;
            return false;
        };
        ScenarioDef def;
        ConsoleData console;
        std::string e;
        if (!LoadScenarioFile(d.scenario, data_dir, def, console, &e)) return fail(e);
        if (d.battery != nullptr) ApplyBatteryState(def, *d.battery);
        if (!d.tasking.empty()) {
            TaskingRules rules;
            if (!LoadTaskingRules(data_dir, d.tasking, rules, &e)) return fail(e);
            regiment_ = std::make_unique<RegimentTasking>(std::move(rules), data_dir);
            // A battery without a task from the scenario plans the regiment's by the same sequence.
            if (def.has_battery && !def.has_task && !LoadPlanSequence(data_dir, "campaign/battery_plan.json", def.plan_sequence, &e)) return fail(e);
        }
        if (d.hours > 0.0) {  // the task held through the day: the session ends by the clock, not when the sky is empty
            def.time_limit_s = std::max(def.time_limit_s, d.hours * 3600.0);
            def.end_when_targets_gone = false;
        }
        const double clock_start_s = def.clock_start_s;
        runner_ = std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
        if (d.faction.empty()) return true;
        if (clock_start_s < 0.0) return fail(d.scenario + ": no \"clock_start\" to put the air war's day on");
        faction_file_ = "air_war/faction_" + d.faction + ".json";
        theatre_file_ = d.theatre;
        seams_ = d.seams;
        briefing_due_ = d.briefing_due;
        campaign_ = std::make_unique<airwar::AirCampaign>();
        if (!d.air_war_save.empty()) {
            // Resumed (O1): the war replayed from its save to the hour it was saved at, the session's clock that hour.
            // The fronts' pipeline is set up after the replay (the replay puts our units' moves back without it) and
            // rebuilt from the war's history; the war has had the battery where it stood.
            if (!airwar::LoadCampaignJson(d.air_war_save, data_dir, *campaign_, &e, &theatre_file_, &faction_file_)) return fail(e);
            const airwar::AirWarData& war = campaign_->Data();
            if (regiment_ && war.tuning.Get("ground.mode") >= 1.0 && war.tuning.Get("ad_strategy.mode") >= 1.0 &&
                !SetUpFronts(data_dir, war, campaign_->Seed(), &e)) {
                return fail(e);
            }
            h0_ = campaign_->Now();
            if (d.battery != nullptr) {
                at_ = d.battery->at;
                arrived_h_ = d.battery->arrived_h >= 0.0 ? d.battery->arrived_h : kNeverH;
            }
        } else {
            airwar::AirWarData war;
            if (!airwar::LoadAirWarData(data_dir, d.theatre, faction_file_, war, &e)) return fail(e);
            if (d.light_mode >= 0) {
                war.tuning.values["light.mode"] = static_cast<double>(d.light_mode);
                airwar::ApplyLightMode(war);
            }
            if (d.fronts) {
                for (const char* k : {"ground.mode", "ad_strategy.mode"}) war.tuning.values[k] = 1.0;
                war.tuning.values["air_front.mode"] = 2.0;  // F7.2's close support of its own axes (F7.3)
            }
            if (!campaign_->Init(war, d.seed)) return fail("the air war does not start");
            if (regiment_ && war.tuning.Get("ground.mode") >= 1.0 && war.tuning.Get("ad_strategy.mode") >= 1.0 && !SetUpFronts(data_dir, war, d.seed, &e)) {
                return fail(e);
            }
            // The battery stands where the mission has it from the war's first hour, so the enemy's intelligence has
            // looked at the battery the session plays.
            campaign_->RelocateBattery(ToTheatre(BatteryCentre()));
            // The scenario's clock on the war's day, in campaign hours (hour 0 is the theatre's start hour on day 1).
            h0_ = static_cast<double>(d.day - 1) * 24.0 + clock_start_s / 3600.0 - campaign_->Data().theatre.start_hour;
            if (h0_ < 0.0) h0_ += 24.0;
            while (campaign_->Advance(h0_)) {
                campaign_->SkipPlayerMission();
                PumpFronts();
            }
        }
        const airwar::AirWarData& war = campaign_->Data();
        const unsigned seed = campaign_->Seed();
        PumpFronts();
        if (watch_.Bound()) {  // what reached the regiment before the session was acted on before it
            RegimentPicture pic;
            watch_.Fill(pic, campaign_->Now());
            regiment_->Since(pic);
        }
        airwar::ScenarioFormat fmt;
        fmt.air_radio = war.faction.radio.present;  // the enemy's calls as transmissions in the session (roadmap B9)
        live_ = std::make_unique<airwar::LivePackages>(*campaign_, data_dir, h0_, fmt);
        SetUpIntercept(data_dir, war, seed);
        outcomes_.Check(CheckSuppression(*campaign_, h0_));
        JoinOurPosts();
        JoinOurBridges();
        seed_ = seed;
        u2_radius_m_ = d.u2_radius_m;
        StartGroundUnits();
        return true;
    }

    // One step of the session, of the war beside it and of the regiment's tasking.
    void Step(double dt) {
        runner_->Step(dt);
        if (live_) live_->Step(*runner_);
        StepGroundUnits(false);
        PumpFronts();
        CarryTheMove();
        PumpIntercept();
        TellDummyHeard();
        KeepOutcomes(dt);
        if (campaign_) FollowOurPosts(false);
        if (campaign_) FollowOurBridges(false);
        if (regiment_ && runner_->Time() + 1e-9 >= next_decide_s_) {
            next_decide_s_ = runner_->Time() + kDecideEveryS;
            // The evening briefing (O1): held once the sky is clear of the enemy's packages, the campaign's seam taken there
            // before the regiment decides, so that a load comes back to the briefing.
            if (BriefingNow()) briefing_due_ = true;
            const bool briefing = briefing_due_ && !(live_ && live_->Active());
            if (briefing) {
                briefing_due_ = false;
                if (seams_) MakeSeam("briefing");
            }
            RegimentPicture pic = campaign_ ? PictureFromJournal(*campaign_) : RegimentPicture();
            if (watch_.Bound()) watch_.Fill(pic, campaign_->Now());
            RegimentDecides(pic, briefing);
        }
        NoteReceived();
    }

    // The regiment decides on what reached it (in play: our intelligence's estimates in the campaign's journal) and
    // gives the battery its next task on the regiment's net. True if it gave one.
    bool RegimentDecides(const RegimentPicture& pic, bool briefing) {
        if (!regiment_) return false;
        ScenarioRunner& r = *runner_;
        TaskingDecision d = regiment_->Decide(pic, r.HasTask() ? &r.Task() : nullptr, arrived_h_, briefing);
        if (!d.give) {
            if (!d.why.empty()) Note("the regiment's tasking: " + d.why);
            return false;
        }
        const double fact_s = d.fact_h >= 0.0 ? (d.fact_h - h0_) * 3600.0 : -1.0;
        if (d.tell_sharing) {  // F4.4: no task, the sky's sharing told on the net
            const bool one = d.sharing == command::CooperationMethod::OneZone;
            const std::string text = "Regiment to the battery: in sector " + pic.sector + " the sky is shared " +
                                     (one ? "in one zone by height: our fighters above the split, you below it." : "by zones: our fighters stay out of your zone.");
            if (!r.RegimentTellsSharing(d.sharing, split_m_, text, kRegimentTellsS)) {
                Note("not told (no R-111 in the session): " + text);
                return false;
            }
            given_.push_back({r.Time(), d.rule, std::string("sharing: ") + command::CooperationMethodName(d.sharing), fact_s, -1.0});
            return true;
        }
        std::string why;
        const std::string was_main = r.HasTask() ? r.Task().main : std::string();
        // F4.4: a new readiness on the task held while the battery is still carrying it out (not yet reported ready) keeps
        // the ready-by the regiment gave it: the posture does not hurry the move.
        if (r.HasTask() && d.task.main == was_main && r.TaskReceivedS() >= 0.0 && r.TaskReadyS() < r.TaskReceivedS() && r.TaskPlan().ready_by_s > r.Time()) {
            d.task.ready_in_s = std::max(d.task.ready_in_s, r.TaskPlan().ready_by_s - r.Time());
        }
        if (!r.GiveTask(d.task, "radio", &why)) {
            Note("the regiment's task " + d.task.id + " not given: " + why);
            return false;
        }
        given_.push_back({r.Time(), d.rule, d.task.id, fact_s, -1.0});
        if (d.task.main != at_ || d.task.main != was_main) arrived_h_ = kNeverH;  // until the battery is at the new task's position
        return true;
    }

    struct GivenTask {
        double at_s = 0.0;
        std::string rule;
        std::string task;
        // F4.4: when the fact it acted on reached the regiment (session seconds, < 0: none or before the session) and when
        // the battery had the order (the battery commander received the task, or the post heard the sharing; < 0 not yet).
        double fact_s = -1.0;
        double heard_s = -1.0;
    };

    ScenarioRunner& Runner() { return *runner_; }
    const ScenarioRunner& Runner() const { return *runner_; }
    const airwar::AirCampaign* Campaign() const { return campaign_.get(); }
    const airwar::LivePackages* Packages() const { return live_.get(); }
    const RegimentTasking* Regiment() const { return regiment_.get(); }
    RegimentTasking* Regiment() { return regiment_.get(); }  // a test's own map of the front (SetPlaceDepth)
    // F4.4: the campaign's pipeline and what reached the regiment of it (null and unbound at the fronts' modes 0).
    const command::InfoNet* Net() const { return net_.get(); }
    const command::CampaignFeed* Feed() const { return feed_.get(); }
    const RegimentWatch& Watch() const { return watch_; }
    RegimentPicture RegimentHolds() const {
        RegimentPicture p = campaign_ ? PictureFromJournal(*campaign_) : RegimentPicture();
        if (watch_.Bound()) watch_.Fill(p, campaign_->Now());
        return p;
    }
    const std::vector<GivenTask>& TasksGiven() const { return given_; }
    const std::vector<std::string>& Notes() const { return notes_; }

    // Track O, O1: the campaign's seams. A seam is taken at the regiment's evening briefing (MissionSessionDef::seams):
    // the air war as its save text and the battery as it stands (BatteryNow), once a day. The game writes it to the
    // campaign's save (campaign_save.hpp) and a load comes back there.
    struct Seam {
        std::string kind;      // "briefing"
        double at_s = 0.0;     // the session second it was taken
        std::string air_war;   // air_war/save.hpp's text
        BatteryState battery;
    };
    // The seam taken since the last call, once (false: none).
    bool TakeSeam(Seam& out) {
        if (!seam_ready_) return false;
        seam_ready_ = false;
        out = seam_;
        return true;
    }
    int SeamsTaken() const { return seams_taken_; }
    const std::string& TheatreFile() const { return theatre_file_; }
    const std::string& FactionFile() const { return faction_file_; }
    // The battery as it stands now, for a save (battery_state.hpp). The hour is the war's (the session's clock at the
    // war's last step, within the live packages' 30 s of now).
    BatteryState BatteryNow() const {
        BatteryState s;
        s.present = true;
        const ScenarioRunner& r = *runner_;
        s.campaign_h = campaign_ ? campaign_->Now() : h0_ + r.Time() / 3600.0;
        const double t = std::max(0.0, (s.campaign_h - h0_) * 3600.0);
        s.local_h = r.Def().clock.LocalHourAt(t);
        s.day_of_year = r.Def().clock.DayOfYearAt(t);
        s.own = r.UnitPosition();
        if (const Battery* b = r.GetBattery()) {
            bool all = true;
            for (const Battery::Vehicle& v : b->Vehicles()) {
                if (v.def.own) continue;
                SavedVehicle sv;
                sv.id = v.def.id;
                sv.position = v.position;
                if (v.def.kind == BatteryVehicleKind::Reload || v.def.kind == BatteryVehicleKind::Fuel) sv.stock = v.stock;
                if (v.def.kind == BatteryVehicleKind::Launcher) {
                    all = all && v.deployed;
                    if (const BatteryFire* f = r.GetBatteryFire()) {
                        for (const BatteryFire::Shooter& sh : f->Shooters()) {
                            if (sh.callsign == v.def.id) sv.missiles = sh.missiles;
                        }
                    }
                }
                s.vehicles.push_back(sv);
            }
            s.deployed = all;
        }
        if (r.Def().has_supply) {
            s.siding_tested = r.Siding().Tested();
            s.siding_untested = r.Siding().Untested();
        }
        s.has_task = r.HasTask();
        if (s.has_task) s.task = r.Task();
        s.task_ready = r.TaskReadyS() >= 0.0;
        s.at = at_;
        s.arrived_h = arrived_h_ >= kNeverH ? -1.0 : arrived_h_;
        return s;
    }
    double CampaignHourAtStart() const { return h0_; }
    int BatteryMoves() const { return moves_; }  // the campaign had our battery move where the session's arrived
    bool BatteryMoved() const { return moves_ > 0; }
    const OutcomeLedger& Outcomes() const { return outcomes_; }
    // F5.2: the ground war's units at U2 in the session (none made without the ground war or the session's roads), and the
    // front's salvos heard and fires seen at the battery so far (the fx feed's DistantFire and FireGlow events pushed).
    const groundwar::SessionUnits& GroundUnits() const { return units_; }
    int FrontSalvosHeard() const { return salvos_; }
    int FrontFiresSeen() const { return glows_; }
    // A radar of the regiment's or a warning source the session follows the war's damage to.
    struct OurPost {
        std::string object;  // the theatre object (the session's id for it too)
        std::string name;
        bool radar = false;  // a radar of the regiment's feed; else a warning source of the vehicle's
        bool stands = true;  // as the session has it now
    };
    const std::vector<OurPost>& OurPosts() const { return ours_; }
    // A road bridge of the theatre on the session's roads (roadmap 17.4): the session follows the war's damage to it,
    // and the regiment tells the battery of those near it on its net.
    struct OurBridge {
        std::string object;    // the theatre object
        std::string name;
        std::string crossing;  // the crossing of the session's roads it stands on
        std::size_t index = 0; // the object's place in the theatre
        std::string entry;     // the roads' entry for its last time down (the feed's wreck key once wrecked)
        bool told = false;     // near the battery: the regiment tells it
        bool open = true;      // as the session has it now
        int downs = 0;
    };
    const std::vector<OurBridge>& OurBridges() const { return bridges_; }
    // Our radio intercept service in the session (roadmap B9): what its posts wrote of the enemy's calls, for the debrief only.
    bool InterceptOn() const { return intercept_on_; }
    const command::InterceptDesk& Desk() const { return desk_; }
    // What reached the battery of it, and when: the fast road's messages and the day summaries (session seconds).
    struct Told {
        double at_s = 0.0;
        std::string key;
        std::string text;
        bool paper = false;
    };
    const std::vector<Told>& InterceptTold() const { return told_; }
    // The runner's debrief, then the two outcomes (from the truth: for the debrief and the reveal only), the enemy's
    // phase checked now.
    std::string Debrief() const {
        std::string d = runner_->Debrief();
        if (!campaign_ && outcomes_.Assets().empty()) return d;
        const SuppressionCheck now = campaign_ ? CheckSuppression(*campaign_, campaign_->Now()) : SuppressionCheck();
        d += "\n" + outcomes_.Text(campaign_ ? &now : nullptr);
        return d + FrontsDebrief();
    }
    // The map `post` holds (track O, O2; command/held_map.hpp): the runner's part (the regiment's task as it reached him,
    // his decision, a launcher commander's card) and, under the fronts, his situation map from the campaign's pipeline,
    // each mark with who told him, by what and when. Hours on the session's clock (second 0 at 0 h).
    command::HeldMap HeldMapFor(const std::string& post) const {
        command::HeldMap m = runner_->HeldMapOf(post);
        if (!net_ || !feed_ || !campaign_) return m;
        const int p = net_->Data().PositionIndex(post);
        if (p < 0) return m;
        m.holder_label = net_->Data().positions[static_cast<std::size_t>(p)].Label();
        if (!feed_->Ground().On()) return m;
        const command::SituationLayout layout = command::SituationLayoutOf(*campaign_, &feed_->Ad(), &own_air_);
        command::SituationMap sm;
        command::BuildSituationMap(*net_, *feed_, layout, p, sm);
        command::SituationFrame f;
        f.to_session = [this](const Vec3& v) { return FromTheatre(v); };
        f.to_map_h = [this](double h) { return h - h0_; };
        const double start_hour = campaign_->Data().theatre.start_hour;
        f.clock = [start_hour](double h) { return command::FormatClock(h, start_hour); };
        command::AddSituationMarks(m, sm, *net_, p, f, &layout);
        return m;
    }
    // The player's post in the campaign's pipeline ("" without it).
    std::string PlayerPost() const {
        if (!net_ || net_->Player() < 0) return std::string();
        return net_->Data().positions[static_cast<std::size_t>(net_->Player())].id;
    }

    // The situation maps of the battery commander and the regiment's staff at the end (F5.1): the front as their facts gave
    // it, each mark with its source and age; empty without the fronts' pipeline. What the posts did not hold is not here:
    // the truth's line is the outcomes' and the developer's.
    std::string FrontsDebrief() const {
        if (!feed_ || !net_ || !campaign_) return std::string();
        const command::SituationLayout layout = command::SituationLayoutOf(*campaign_, &feed_->Ad(), &own_air_);
        std::string out;
        const std::string posts[2] = {feed_->Ground().Roles().battery, feed_->Ground().Roles().regiment};
        for (const std::string& id : posts) {
            const int post = net_->Data().PositionIndex(id);
            if (post < 0) continue;
            command::SituationMap map;
            command::BuildSituationMap(*net_, *feed_, layout, post, map);
            out += "\n" + command::SituationMapText(map, campaign_->Data().theatre.start_hour);
        }
        return out;
    }

private:
    // The enemy's radio, and our intercept service listening to it (roadmap B9): the posts of data/command/intercept.json at
    // their places, hearing by link budget; what they write goes down the pipeline to the battery's journal.
    void SetUpIntercept(const std::string& data_dir, const airwar::AirWarData& war, unsigned seed) {
        const airwar::FactionRadio& fr = war.faction.radio;
        if (!fr.present) return;
        std::string e;
        command::ChainData chain;
        if (!command::LoadInterceptData(data_dir, "command/intercept.json", idata_, &e) ||
            !command::LoadChainData(data_dir, "command/chain_pannonia.json", chain, &e)) {
            Note("no intercept service: " + e);
            return;
        }
        AirRadioDef def;
        def.on = true;
        def.faction = war.faction.id;
        def.library_file = "em/air_radios_1985.json";
        const em::EmLoadResult res = em::LoadEmLibraryFile(data_dir + "/" + def.library_file, def.library);
        if (!res.ok) {
            Note("no intercept service: " + res.error);
            return;
        }
        def.air_radio = fr.air_radio;
        def.ground_radio = fr.ground_radio;
        def.freq_hz = fr.band_mhz * 1e6;
        def.language = fr.language;
        def.discipline = fr.discipline;
        def.callsign_rotation_h = fr.callsign_rotation_h;
        def.have_quick_share = fr.have_quick_share;
        def.secure_voice_share = fr.secure_voice_share;
        def.opsec = fr.opsec;
        for (const auto& kv : AirRadioTuningDefaults()) def.tune[kv.first] = war.tuning.Get(kv.first);
        for (const command::InterceptPostData& p : idata_.posts) {
            const command::Position* pos = chain.FindPosition(p.position);
            const command::Place* pl = pos != nullptr ? chain.FindPlace(pos->place) : nullptr;
            if (pl == nullptr) continue;
            const Vec3 at = FromTheatre(pl->pos);
            for (int hf = 0; hf < 2; ++hf) {  // a VHF and UHF receiver and an HF one at each post
                AirRadioPostDef d;
                d.id = p.id + (hf != 0 ? "_hf" : "");
                d.name = pl->name;
                d.kind = "intercept";
                d.at = at;
                d.mast_m = kInterceptMastM;
                d.esm = hf != 0 ? "air_intercept_hf" : "air_intercept_vhf_uhf";
                d.chain_post = p.id;
                def.posts.push_back(d);
            }
        }
        PlaceDummyTraffic(war, def);
        if (!runner_->EnableAirRadio(def, &e)) {
            Note("no intercept service: " + e);
            return;
        }
        const double clock = runner_->Def().clock_start_s;
        desk_.Init(&idata_, seed, clock);
        if (!ibridge_.Init(data_dir, "command/chain_pannonia.json", seed, clock, &e)) {
            Note("the intercept service has no pipeline: " + e);
            return;
        }
        intercept_on_ = true;
        live_->SetHeardSource([this](double from_s, double to_s) {
            std::vector<airwar::HeardRecord> out;
            for (const command::Logged& l : desk_.Log()) {
                if (l.heard_s < from_s || l.heard_s > to_s) continue;
                airwar::HeardRecord h;
                h.post = l.post;
                h.t_s = l.heard_s;
                h.kind = l.understood ? airwar::CallKindId(l.kind) : "traffic";
                h.text = l.text;
                h.understood = l.understood;
                if (l.fast) h.fact = "intercept:" + std::to_string(l.call) + ":" + l.post;
                // Roadmap S6: what the campaign's feed needs to make the same facts on a load (command/air_war_feed.hpp).
                h.chain_post = l.chain_post;
                h.h = live_->HourOf(l.heard_s);
                h.written_h = live_->HourOf(l.written_s);
                h.bearing_deg = l.bearing_deg;
                h.precedence = l.precedence;
                h.names_battery = l.names_battery;
                out.push_back(h);
            }
            return out;
        });
    }
    // What our posts heard since the last step is written up; the fast road's messages are said on the battery's net and
    // reach its radiotelephonist's journal; the day's summary comes by the pipeline's courier to the commander.
    void PumpIntercept() {
        const airwar::AirRadio* ar = runner_->AirRadio();
        if (!intercept_on_ || ar == nullptr) return;
        const double now = runner_->Time();
        if (now + 1e-9 >= next_watch_s_) {  // where the battery stands now is the square it would be named by
            next_watch_s_ = now + kDecideEveryS;
            desk_.ClearWatch();
            desk_.Watch(BatteryCentre());
        }
        const std::vector<command::Logged> fresh = desk_.Process(*ar);
        for (const command::InterceptBridge::Arrival& a : ibridge_.Feed(fresh, idata_, *ar)) {
            const std::string call_key = a.key.substr(0, a.key.rfind(':'));  // one call heard by several posts is told once
            if (!told_calls_.insert(call_key).second) continue;
            const double after = std::max(0.0, a.at_s - now);
            runner_->SayOnBatteryNet("Regiment", a.text, after);
            told_.push_back({now + after, a.key, a.text, false});
        }
        // The air section's summary of the day, at its hour: a paper to the battery commander by the means the pipeline has.
        if (runner_->Def().clock_start_s >= 0.0) {
            const double local_h = std::fmod((runner_->Def().clock_start_s + now) / 3600.0, 24.0);
            const bool due = now + 1e-9 >= next_summary_s_ && local_h >= idata_.daily_summary_hour && local_h < idata_.daily_summary_hour + 0.5;
            if (due) {
                next_summary_s_ = now + 20.0 * 3600.0;
                const std::string text = desk_.DailySummary(summary_from_s_, now, ar->Def().Tune("radio.continuity_name_at"), *ar);
                summary_from_s_ = now;
                const command::Fact f = command::SummaryFact(text, ++summaries_, now);
                command::InterceptBridge::Arrival a;
                if (ibridge_.Add(f, a)) {
                    DeliveryDef d;
                    d.id = f.key;
                    d.after_s = std::max(now, a.at_s);
                    d.from = "Army intelligence, air section";
                    d.words = "The air section's radio intercept summary.";
                    d.paper_title = "Radio intercept summary";
                    d.paper_text = text;
                    DeliveryMeans m = DeliveryMeans::Courier;
                    if (!ParseDeliveryMeans(a.mean, m) || !Carried(m)) m = DeliveryMeans::Courier;
                    d.means = m;
                    runner_->AddDeliveries({d});
                    told_.push_back({d.after_s, f.key, text, true});
                }
            }
        }
    }
    static constexpr double kInterceptMastM = 30.0;  // sam3d: the posts' antennas (docs/air_battle/09 section 4.2)
    static constexpr const char* kOurAirRadio = "pact_air_r832m";  // our side's air set and first net, as raid_scenario.hpp's
    static constexpr double kOurAirFreqHz = 124e6;

    // Our dummy traffic in the session (roadmap S6's follow-ups, radio.mode 2): each false corridor the campaign's rule gives
    // for the session's days (AirCampaign::FalseCorridorsOn) gets a dummy transmitter at its middle, calling on our fighters'
    // net through the corridor's hours; the enemy's listeners of data/command/intercept.json stand in the session's EM world
    // where the data puts them, and hear it, our GCI and our flights by link margin (em/comms_intercept.hpp).
    void PlaceDummyTraffic(const airwar::AirWarData& war, AirRadioDef& def) {
        if (!campaign_ || war.tuning.Get("radio.mode") < 2.0) return;
        const auto it = idata_.listeners.find(war.faction.id);
        if (it == idata_.listeners.end() || it->second.empty()) return;  // nobody of theirs to hear it
        for (const command::ListenerData& l : it->second) {
            AirRadioPostDef d;
            d.id = l.id;
            d.name = l.id;
            d.kind = "enemy_intercept";
            d.at = FromTheatre(Vec3{l.east_km * 1000.0, l.north_km * 1000.0, 0.0});
            d.mast_m = l.height_m;  // an airborne listener: its orbit's height
            d.esm = "air_intercept_vhf_uhf";
            def.posts.push_back(d);
        }
        const double start_hour = campaign_->Data().theatre.start_hour;
        const int first = static_cast<int>(std::floor(h0_ / 24.0));
        for (int day = std::max(0, first - 1); day <= first + 2; ++day) {  // the days a session of up to two days meets
            for (const command::OwnAirZone& z : campaign_->FalseCorridorsOn(day)) {
                if (z.path.size() < 2) continue;
                const double from_s = (day * 24.0 + z.from_h - start_hour - h0_) * 3600.0;
                const double until_s = (day * 24.0 + z.to_h - start_hour - h0_) * 3600.0;
                if (until_s <= 0.0) continue;
                AirRadioPostDef d;
                d.id = "our_dummy_" + std::to_string(day + 1) + "_" + std::to_string(dummies_.size() + 1);
                d.name = "dummy transmitter, " + z.id;
                d.kind = "our_dummy";
                d.at = FromTheatre((z.path.front() + z.path.back()) * 0.5);
                d.mast_m = war.tuning.Get("radio.our_dummy_mast_m");
                d.radio = kOurAirRadio;
                d.freq_hz = kOurAirFreqHz;
                d.corridor = z.id;
                d.from_s = std::max(0.0, from_s);
                d.until_s = until_s;
                def.posts.push_back(d);
                dummies_.push_back(Dummy{d.id, z.id, day, until_s, false});
            }
        }
    }
    // The enemy's word on each false corridor's use to the campaign's corridor learning: heard as soon as a listener heard a
    // dummy call, not heard once the corridor's hours are over without one.
    void TellDummyHeard() {
        const airwar::AirRadio* ar = runner_->AirRadio();
        if (!campaign_ || ar == nullptr) return;
        for (Dummy& d : dummies_) {
            if (d.told) continue;
            const bool heard = ar->EnemyHeardDummy(d.post);
            if (!heard && runner_->Time() < d.until_s) continue;
            campaign_->SessionHeardCorridor(d.day, d.corridor, heard);
            d.told = true;
            Note(std::string("our dummy traffic in ") + d.corridor + (heard ? ": heard by the enemy" : ": not heard by the enemy"));
        }
    }

    // F4.4: the campaign's pipeline in the session (see the header). The net on our chain, the feed bound to it and asked
    // for the units' orders, the regiment's watch on its posts, how the regiment places a position against the front's line
    // (its map's base line: the line as reported is not a fact it holds) and the sky's table from own_air.json.
    bool SetUpFronts(const std::string& data_dir, const airwar::AirWarData& war, unsigned seed, std::string* error) {
        command::ChainData chain;
        command::OwnAirDef own_air;
        std::string e;
        if (!command::LoadChainData(data_dir, "command/chain_pannonia.json", chain, &e) ||
            !command::LoadOwnAirFile(data_dir + "/command/own_air.json", own_air, &e)) {
            if (error != nullptr) *error = "the fronts' pipeline: " + e;
            return false;
        }
        net_ = std::make_unique<command::InfoNet>();
        if (!net_->Init(chain, seed, war.theatre.start_hour, &e)) {
            if (error != nullptr) *error = "the fronts' pipeline: " + e;
            return false;
        }
        command::GiveTheatreSky(*net_, war.theatre);
        feed_ = std::make_unique<command::CampaignFeed>();
        feed_->Bind(net_.get());
        campaign_->SetOrderSource(feed_->OrderSource());
        if (!watch_.Bind(net_.get(), feed_.get(), own_air, &e)) {
            if (error != nullptr) *error = "the fronts' pipeline: " + e;
            return false;
        }
        split_m_ = own_air.coop.split_height_m;
        own_air_ = own_air;
        regiment_->SetSplitHeight(split_m_);
        regiment_->SetPlaceDepth([this](const std::string& place, double& depth_m, std::string& name) {
            const nav::SessionNav* nav = runner_ ? runner_->Nav() : nullptr;
            const nav::Anchor* a = nav != nullptr ? nav->GetAnchors().Find(place) : nullptr;
            if (a == nullptr || !campaign_ || campaign_->FrontLine().Empty()) return false;
            const airwar::FrontPlace pl = campaign_->FrontLine().Place(ToTheatre(Vec3{a->at.east_m, a->at.north_m, 0.0}));
            depth_m = pl.side * pl.distance_m;
            name = a->name;
            return true;
        });
        return true;
    }
    // The feed brought up to the campaign's hour every kFrontsEveryS of the session (each delivery keeps its own hour; the
    // regiment's staff works on a fact from then), the regiment's watch reading what is new.
    void PumpFronts() {
        if (!feed_ || !campaign_ || campaign_->Now() <= pumped_h_ + kFrontsEveryS / 3600.0 - 1e-9) return;
        pumped_h_ = campaign_->Now();
        feed_->Pump(*campaign_);
        watch_.Update(*campaign_);
    }
    // When the battery had the regiment's last order (F4.4's delays): the battery commander received the task, the post
    // heard the sharing.
    void NoteReceived() {
        if (given_.empty() || given_.back().heard_s >= 0.0) return;
        GivenTask& g = given_.back();
        const ScenarioRunner& r = *runner_;
        const double heard = g.task.compare(0, 8, "sharing:") == 0 ? r.SharingHeardS() : r.TaskReceivedS();
        if (heard >= g.at_s - 1e-9) g.heard_s = heard;
    }

    // The battery at the position its task names (the task's march over, or in its area from the start): the
    // campaign's battery is there from then (between packages), and the regiment's estimates count from then.
    void CarryTheMove() {
        const ScenarioRunner& r = *runner_;
        if (!r.HasTask() || r.TaskReceivedS() < 0.0 || !r.TaskPlan().planned || at_ == r.Def().task.main || r.Nav() == nullptr) return;
        if (live_ && live_->Active()) return;
        const Battery* b = r.GetBattery();
        const nav::Anchor* main = r.Nav()->GetAnchors().Find(r.Def().task.main);
        if (b == nullptr || main == nullptr) return;
        const bool marched = r.TaskPlan().march_at_s >= 0.0;
        if (marched) {  // the column's head at the end of its route, there
            const Battery::Vehicle* head = b->Own() != nullptr ? b->Own() : b->Find(r.Def().battery_giver);
            if (head == nullptr || r.Time() < r.TaskPlan().march_at_s || !b->AtRouteEnd() ||
                std::hypot(head->position.x - main->at.east_m, head->position.y - main->at.north_m) > kArrivedM) {
                return;
            }
        }
        at_ = r.Def().task.main;
        arrived_h_ = marched ? h0_ + r.Time() / 3600.0 : 0.0;  // in its area: the campaign has had it there from the war's start
        if (!campaign_ || !marched) return;
        campaign_->RelocateBattery(ToTheatre(Vec3{main->at.east_m, main->at.north_m, 0.0}));
        ++moves_;
    }
    static constexpr double kArrivedM = 1000.0;  // sam3d: as the runner's kTaskInAreaM

    // The two outcomes' ledgers: the task's covered object (its place's theatre object, looked up once per task) while
    // the battery stands ready for action on the task, every object it covered as it stands now, and each package
    // once it is back from the session.
    void KeepOutcomes(double dt) {
        const ScenarioRunner& r = *runner_;
        if (r.HasTask() && !r.Def().task.covered.empty()) {
            if (r.Def().task.covered != covered_place_) {
                covered_place_ = r.Def().task.covered;
                const nav::Anchor* a = r.Nav() != nullptr ? r.Nav()->GetAnchors().Find(covered_place_) : nullptr;
                covered_object_ = a != nullptr ? a->theatre_object : std::string();
                outcomes_.Cover(covered_place_, covered_object_, a != nullptr ? a->name : covered_place_, Function(covered_object_));
            }
            if (r.TaskReadyS() >= 0.0) outcomes_.Covered(covered_place_, dt, Function(covered_object_));
        }
        if (!campaign_) return;
        if (r.Time() + 1e-9 >= next_seen_s_) {  // the raids resolved off the scene count too
            next_seen_s_ = r.Time() + kDecideEveryS;
            for (const CoveredAsset& a : outcomes_.Assets()) outcomes_.Seen(a.place, Function(a.object));
            // The enemy's air front in the battery's sector (F6.2): the hours the battery's entry held it, a check a minute.
            outcomes_.AirFront(CheckAirFront(*campaign_, campaign_->Now()), kDecideEveryS / 3600.0);
        }
        const std::vector<airwar::LivePackages::Played>& played = live_->PlayedPackages();
        for (; ledgered_ < played.size() && played[ledgered_].out_s >= 0.0; ++ledgered_) {
            for (const airwar::Package& p : campaign_->Packages()) {
                if (p.id == played[ledgered_].package_id) {
                    outcomes_.Played(p, played[ledgered_].outcome, played[ledgered_].release_flights, OwnObject(*campaign_));
                }
            }
        }
    }
    double Function(const std::string& object) const { return campaign_ && !object.empty() ? ObjectFunction(*campaign_, object) : 1.0; }
    static constexpr double kDecideEveryS = 60.0;
    static constexpr double kFrontsEveryS = 300.0;  // sam3d: the pipeline read every 5 min of the session (F4.4)
    static constexpr double kNeverH = 1e18;

    // The regiment's evening briefing fell in the last minute of the session's clock (none without a clock).
    bool BriefingNow() {
        const ScenarioRunner& r = *runner_;
        if (r.Def().clock_start_s < 0.0) return false;
        const double now = std::fmod((r.Def().clock_start_s + r.Time()) / 3600.0, 24.0);
        double before = now - kDecideEveryS / 3600.0;
        if (before < 0.0) before += 24.0;
        const double bh = regiment_->Rules().briefing_hour;
        return before <= now ? (before < bh && bh <= now) : (bh > before || bh <= now);
    }
    void Note(const std::string& n) {
        if (notes_.empty() || notes_.back() != n) notes_.push_back(n);
    }
    void MakeSeam(const char* kind) {
        if (!campaign_) return;
        seam_.kind = kind;
        seam_.at_s = runner_->Time();
        seam_.air_war = airwar::SaveCampaignJson(*campaign_, theatre_file_, faction_file_);
        seam_.battery = BatteryNow();
        seam_ready_ = true;
        ++seams_taken_;
    }

    // Where the battery stands in the session: the middle of its launchers (our vehicle without a battery).
    Vec3 BatteryCentre() const {
        const Battery* b = runner_->GetBattery();
        Vec3 sum;
        int n = 0;
        if (b != nullptr) {
            for (const Battery::Vehicle& v : b->Vehicles()) {
                if (v.def.kind != BatteryVehicleKind::Launcher) continue;
                sum += v.position;
                ++n;
            }
        }
        return n > 0 ? sum / static_cast<double>(n) : runner_->UnitPosition();
    }
    // Our radars and warning posts (see the header): joined at the start, then followed each minute.
    void JoinOurPosts() {
        ScenarioRunner& r = *runner_;
        const airwar::TheatreDef& th = campaign_->Data().theatre;
        const Vec3 centre = BatteryCentre();
        for (std::size_t i = 0; i < th.objects.size(); ++i) {
            const airwar::TheatreObject& o = th.objects[i];
            if (o.regiment_radar.empty()) continue;
            const Vec3 at = FromTheatre(campaign_->Truth()[i].pos);
            if (std::hypot(at.x - centre.x, at.y - centre.y) >= kOurRadarsM) continue;
            if (r.AddRegimentRadar(o.id, o.regiment_radar, Vec3{at.x, at.y, kRadarUpM})) ours_.push_back({o.id, o.name, true, true});
        }
        for (const C2SourceDef& s : r.Def().c2_sources) {
            for (const airwar::TheatreObject& o : th.objects) {
                if (o.id == s.id) ours_.push_back({o.id, o.name, false, true});
            }
        }
        FollowOurPosts(true);
    }
    void FollowOurPosts(bool start) {
        if (!start && runner_->Time() + 1e-9 < next_posts_s_) return;
        next_posts_s_ = runner_->Time() + kDecideEveryS;
        std::vector<std::string> out, back;
        for (OurPost& p : ours_) {
            const bool stands = Function(p.object) > kStandsAbove;
            if (stands == p.stands) continue;
            p.stands = stands;
            const std::string why = stands ? "repaired" : start ? "struck before the mission" : "struck";
            const bool changed = p.radar ? runner_->SetRegimentRadarDown(p.object, !stands, why) : runner_->SetC2SourceDown(p.object, !stands, why);
            if (changed && p.radar) (stands ? back : out).push_back(p.name);
        }
        const auto told = [this, start](const std::vector<std::string>& names, const char* one, const char* many) {
            if (names.empty()) return;
            std::string list;
            for (std::size_t i = 0; i < names.size(); ++i) list += (i == 0 ? "" : i + 1 == names.size() ? " and " : ", ") + names[i];
            const std::string text = "Regiment to the battery: " + list + (names.size() == 1 ? one : many);
            if (!runner_->RegimentSays(text, start ? kToldAtStartS : kRegimentTellsS)) Note("not told (no R-111 in the session): " + text);
        };
        told(out, " is out of action; the regiment's picture is thinner.", " are out of action; the regiment's picture is thinner.");
        told(back, " is on the air again.", " are on the air again.");
    }
    // The theatre's road bridges on the session's roads, joined at the start and followed each minute: a bridge is down
    // below the supply lines' rule (logistics.bridge_open, air_war/logistics.hpp), and so in the truth of the session's
    // roads at once; the regiment tells the battery of those near it, a while after.
    void JoinOurBridges() {
        const nav::SessionNav* nav = runner_->Nav();
        if (nav == nullptr) return;
        const airwar::TheatreDef& th = campaign_->Data().theatre;
        const Vec3 centre = BatteryCentre();
        for (std::size_t i = 0; i < th.objects.size(); ++i) {
            const airwar::TheatreObject& o = th.objects[i];
            if (o.kind != airwar::ObjectKind::Bridge) continue;
            const Vec3 at = FromTheatre(campaign_->Truth()[i].pos);
            double d = 0.0;
            const nav::NavCrossing* c = nav->Graph().NearestCrossing({at.x, at.y}, &d);
            if (c == nullptr || d > kBridgeOnCrossingM) continue;
            OurBridge b;
            b.object = o.id;
            b.name = o.name;
            b.crossing = c->id;
            b.index = i;
            b.told = std::hypot(at.x - centre.x, at.y - centre.y) <= kRegimentRoadsM;
            bridges_.push_back(b);
        }
        FollowOurBridges(true);
    }
    void FollowOurBridges(bool start) {
        if (!start && runner_->Time() + 1e-9 < next_bridges_s_) return;
        next_bridges_s_ = runner_->Time() + kDecideEveryS;
        const double open_at = campaign_->Data().tuning.Get("logistics.bridge_open");
        for (OurBridge& b : bridges_) {
            const bool open = Function(b.object) >= open_at;
            if (open == b.open) continue;
            b.open = open;
            if (!open) {
                ++b.downs;
                const double wrecked_h = campaign_->Truth()[b.index].wrecked_h;
                // One entry for each time down: a bridge back in use before its repair is done keeps its wreck hour.
                b.entry = (wrecked_h >= 0.0 ? "wreck:" + b.object + ":" + std::to_string(static_cast<long long>(std::llround(wrecked_h * 100.0))) + ":"
                                            : "cut:" + b.object + ":") +
                          std::to_string(b.downs);
            }
            if (!runner_->SetCrossingDown(b.crossing, !open, b.entry, b.entry) || !b.told) continue;
            if (!runner_->RegimentTellsRoad(b.crossing, b.name, !open, b.entry, start ? kToldAtStartS : kRegimentTellsS)) {
                Note("not told (no R-111 in the session): the " + b.name + (open ? " is open again" : " is down"));
            }
        }
    }
    static constexpr double kBridgeOnCrossingM = 100.0;  // sam3d: a bridge on the road crossing this near (the rail bridges lie
                                                         // kilometres from any but the capital's, 134 m from its road bridge)
    static constexpr double kRegimentRoadsM = 50000.0;   // sam3d: the bridges the regiment tells the battery of, this near it
    static constexpr double kStandsAbove = 0.3;      // as raid_scenario.hpp's: at or below it a post is out of action
    static constexpr double kOurRadarsM = 250000.0;  // as raid_scenario.hpp's: the regiment's radars this near the battery
    static constexpr double kRadarUpM = 2.0;         // as raid_scenario.hpp's radar positions
    static constexpr double kToldAtStartS = 90.0;    // sam3d: after the regiment's first word to the battery
    static constexpr double kRegimentTellsS = 180.0; // sam3d: the radar company reports up, the regiment passes it on

    // F5.2 (see the header): U2 and the front from afar, read every kUnitsEveryS and the fx values' every_s of the session.
    void StartGroundUnits() {
        if (!campaign_ || campaign_->Ground() == nullptr || runner_->MutableNav() == nullptr) return;
        units_.Start(campaign_->Ground()->Data());
        StepGroundUnits(true);
    }
    void StepGroundUnits(bool start) {
        if (!units_.Started()) return;
        const double t = runner_->Time();
        const groundwar::GroundWar& w = *campaign_->Ground();
        const airwar::LineOfContact& line = campaign_->FrontLine();
        const Vec3 centre = BatteryCentre();
        if (start || t + 1e-9 >= next_units_s_) {
            next_units_s_ = t + kUnitsEveryS;
            std::vector<groundwar::U2Unit> now = groundwar::U2Near(w, line, ToTheatre(centre), u2_radius_m_, units_.OurTypeList());
            for (groundwar::U2Unit& u : now) u.place = FromTheatre(u.place);
            units_.Apply(now, *runner_->MutableNav(), t, runner_->LightNow().dark);
        }
        if (!start && t + 1e-9 < next_fire_s_) return;
        next_fire_s_ = t + front_fx_.every_s;
        std::vector<groundwar::FrontContact> contacts = groundwar::FrontContacts(w, line);
        if (contacts.empty()) return;
        for (groundwar::FrontContact& c : contacts) c.at = FromTheatre(c.at);
        const ScenarioDef& def = runner_->Def();
        groundwar::FrontAir air;
        if (def.has_weather) {
            air.temperature_k = def.weather.surface_temperature_c + 273.15;
            air.rh_pct = fx::RelativeHumidityPct(def.weather.surface_temperature_c, def.weather.surface_dewpoint_c);
            air.pressure_pa = def.weather.qnh_hpa * 100.0;
        }
        for (const fx::Event& e : groundwar::DistantFire(contacts, centre, t, fire_step_, air, front_fx_, seed_)) {
            runner_->Fx().Push(e);
            ++salvos_;
        }
        ++fire_step_;
        if (t + 1e-9 < next_glow_s_) return;
        next_glow_s_ = t + front_fx_.glow_every_s;
        const double visibility_m = def.weather.visibility_km * 1000.0;
        for (const fx::Event& e : groundwar::FireGlows(contacts, centre, t, glow_step_, runner_->LightNow().dark, visibility_m, front_fx_, seed_)) {
            runner_->Fx().Push(e);
            ++glows_;
        }
        ++glow_step_;
    }
    static constexpr double kUnitsEveryS = 60.0;  // sam3d: U2's places read every minute of the session

    // A theatre point in the session's frame (on the ground).
    Vec3 FromTheatre(const Vec3& theatre_enu) const {
        Vec3 to = airwar::MoveFrame(LocalFrame(campaign_->Data().theatre.origin), LocalFrame(runner_->Def().origin), theatre_enu);
        to.z = 0.0;
        return to;
    }
    // A session point in the theatre's frame (on the ground).
    Vec3 ToTheatre(const Vec3& session_enu) const {
        Vec3 to = airwar::MoveFrame(LocalFrame(runner_->Def().origin), LocalFrame(campaign_->Data().theatre.origin), session_enu);
        to.z = 0.0;
        return to;
    }

    std::unique_ptr<airwar::AirCampaign> campaign_;
    std::string theatre_file_;      // the war's data files, for its save (O1)
    std::string faction_file_;
    bool seams_ = false;
    bool briefing_due_ = false;     // the briefing's hour has come; held once the sky is clear
    Seam seam_;
    bool seam_ready_ = false;
    int seams_taken_ = 0;
    std::unique_ptr<command::InfoNet> net_;          // F4.4: the campaign's pipeline (the fronts on)
    std::unique_ptr<command::CampaignFeed> feed_;
    command::OwnAirDef own_air_;                     // F5.1: the sky's sharing, for the situation maps' layout
    RegimentWatch watch_;
    double pumped_h_ = -1.0;
    double split_m_ = 5000.0;
    std::unique_ptr<ScenarioRunner> runner_;
    std::unique_ptr<airwar::LivePackages> live_;
    std::unique_ptr<RegimentTasking> regiment_;
    double h0_ = 0.0;
    std::string at_;                // the task's position the campaign has our battery at ("": where it started)
    int moves_ = 0;
    double arrived_h_ = kNeverH;    // the campaign hour the battery came to the position it holds
    double next_decide_s_ = 0.0;
    std::vector<GivenTask> given_;
    std::vector<OurPost> ours_;
    double next_posts_s_ = 0.0;
    std::vector<OurBridge> bridges_;
    double next_bridges_s_ = 0.0;
    std::vector<std::string> notes_;
    unsigned seed_ = 1;
    groundwar::SessionUnits units_;      // F5.2: U2
    double u2_radius_m_ = 20000.0;       // MissionSessionDef::u2_radius_m
    groundwar::FrontFxValues front_fx_;
    double next_units_s_ = 0.0;
    double next_fire_s_ = 0.0;
    double next_glow_s_ = 0.0;
    std::uint64_t fire_step_ = 0;
    std::uint64_t glow_step_ = 0;
    int salvos_ = 0;
    int glows_ = 0;
    OutcomeLedger outcomes_;
    std::string covered_place_;     // the task's covered place the ledger has
    std::string covered_object_;    // its theatre object
    std::size_t ledgered_ = 0;      // played packages in the ledger
    double next_seen_s_ = 0.0;
    command::InterceptData idata_;       // our radio intercept service (roadmap B9)
    command::InterceptDesk desk_;
    command::InterceptBridge ibridge_;
    bool intercept_on_ = false;
    struct Dummy {
        std::string post;      // its transmitter in the session's air radio
        std::string corridor;  // the false corridor
        int day = 0;           // the campaign day of its use
        double until_s = 0.0;  // the session time its hours end
        bool told = false;     // the campaign has the enemy's word on it
    };
    std::vector<Dummy> dummies_;  // our dummy transmitters (roadmap S6's follow-ups)
    std::vector<Told> told_;
    std::set<std::string> told_calls_;  // the calls the fast road has told already
    double next_watch_s_ = 0.0;
    double next_summary_s_ = 0.0;
    double summary_from_s_ = 0.0;
    int summaries_ = 0;
};

}  // namespace campaign
}  // namespace simcore
