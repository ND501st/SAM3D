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
#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/air_war/data.hpp"
#include "simcore/air_war/package_injector.hpp"
#include "simcore/campaign/battery_plan.hpp"
#include "simcore/campaign/outcomes.hpp"
#include "simcore/campaign/regiment.hpp"
#include "simcore/geo.hpp"
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
};

class MissionSession {
public:
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
        airwar::AirWarData war;
        if (!airwar::LoadAirWarData(data_dir, d.theatre, "air_war/faction_" + d.faction + ".json", war, &e)) return fail(e);
        campaign_ = std::make_unique<airwar::AirCampaign>();
        if (!campaign_->Init(war, d.seed)) return fail("the air war does not start");
        // The battery stands where the mission has it from the war's first hour, so the enemy's intelligence has
        // looked at the battery the session plays.
        campaign_->RelocateBattery(ToTheatre(BatteryCentre()));
        // The scenario's clock on the war's day, in campaign hours (hour 0 is the theatre's start hour on day 1).
        h0_ = static_cast<double>(d.day - 1) * 24.0 + clock_start_s / 3600.0 - campaign_->Data().theatre.start_hour;
        if (h0_ < 0.0) h0_ += 24.0;
        while (campaign_->Advance(h0_)) campaign_->SkipPlayerMission();
        live_ = std::make_unique<airwar::LivePackages>(*campaign_, data_dir, h0_);
        outcomes_.Check(CheckSuppression(*campaign_, h0_));
        JoinOurPosts();
        JoinOurBridges();
        return true;
    }

    // One step of the session, of the war beside it and of the regiment's tasking.
    void Step(double dt) {
        runner_->Step(dt);
        if (live_) live_->Step(*runner_);
        CarryTheMove();
        KeepOutcomes(dt);
        if (campaign_) FollowOurPosts(false);
        if (campaign_) FollowOurBridges(false);
        if (regiment_ && runner_->Time() + 1e-9 >= next_decide_s_) {
            next_decide_s_ = runner_->Time() + kDecideEveryS;
            RegimentDecides(campaign_ ? PictureFromJournal(*campaign_) : RegimentPicture(), BriefingNow());
        }
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
        std::string why;
        if (!r.GiveTask(d.task, "radio", &why)) {
            Note("the regiment's task " + d.task.id + " not given: " + why);
            return false;
        }
        given_.push_back({r.Time(), d.rule, d.task.id});
        arrived_h_ = kNeverH;  // until the battery is at the new task's position
        return true;
    }

    struct GivenTask {
        double at_s = 0.0;
        std::string rule;
        std::string task;
    };

    ScenarioRunner& Runner() { return *runner_; }
    const ScenarioRunner& Runner() const { return *runner_; }
    const airwar::AirCampaign* Campaign() const { return campaign_.get(); }
    const airwar::LivePackages* Packages() const { return live_.get(); }
    const RegimentTasking* Regiment() const { return regiment_.get(); }
    const std::vector<GivenTask>& TasksGiven() const { return given_; }
    const std::vector<std::string>& Notes() const { return notes_; }
    double CampaignHourAtStart() const { return h0_; }
    int BatteryMoves() const { return moves_; }  // the campaign had our battery move where the session's arrived
    bool BatteryMoved() const { return moves_ > 0; }
    const OutcomeLedger& Outcomes() const { return outcomes_; }
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
    // The runner's debrief, then the two outcomes (from the truth: for the debrief and the reveal only), the enemy's
    // phase checked now.
    std::string Debrief() const {
        const std::string d = runner_->Debrief();
        if (!campaign_ && outcomes_.Assets().empty()) return d;
        const SuppressionCheck now = campaign_ ? CheckSuppression(*campaign_, campaign_->Now()) : SuppressionCheck();
        return d + "\n" + outcomes_.Text(campaign_ ? &now : nullptr);
    }

private:
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
    OutcomeLedger outcomes_;
    std::string covered_place_;     // the task's covered place the ledger has
    std::string covered_object_;    // its theatre object
    std::size_t ledgered_ = 0;      // played packages in the ledger
    double next_seen_s_ = 0.0;
};

}  // namespace campaign
}  // namespace simcore
