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
#pragma once

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/air_war/data.hpp"
#include "simcore/air_war/package_injector.hpp"
#include "simcore/campaign/battery_plan.hpp"
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
        return true;
    }

    // One step of the session, of the war beside it and of the regiment's tasking.
    void Step(double dt) {
        runner_->Step(dt);
        if (live_) live_->Step(*runner_);
        CarryTheMove();
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
    std::vector<std::string> notes_;
};

}  // namespace campaign
}  // namespace simcore
