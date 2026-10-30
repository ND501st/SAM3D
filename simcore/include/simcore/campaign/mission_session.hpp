// simcore/campaign/mission_session.hpp — the battery's mission as one session with the enemy's air war in step
// (roadmap 15.2, first form; docs/campaign/05_THE_SYSTEMS.md §6).
//
// A battery mission (a scenario with the regiment's "task", phase 14) plays in one ScenarioRunner from the order to
// its end, and the enemy's air campaign runs beside it on the same clock: the campaign's hour at the session's start is
// the scenario's clock on the war's day, the campaign is run to there first (the packages before it resolved without
// the player), and from then on the packages it hands our battery enter the session live
// (air_war/package_injector.hpp's LivePackages). Our battery's move is the campaign's once it has arrived: until then
// the enemy's picture, and so its aim, has it where it stood.
#pragma once

#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "simcore/air_war/campaign.hpp"
#include "simcore/air_war/data.hpp"
#include "simcore/air_war/package_injector.hpp"
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

    // One step of the session and of the war beside it.
    void Step(double dt) {
        runner_->Step(dt);
        if (!live_) return;
        live_->Step(*runner_);
        CarryTheMove();
    }

    ScenarioRunner& Runner() { return *runner_; }
    const ScenarioRunner& Runner() const { return *runner_; }
    const airwar::AirCampaign* Campaign() const { return campaign_.get(); }
    const airwar::LivePackages* Packages() const { return live_.get(); }
    double CampaignHourAtStart() const { return h0_; }
    bool BatteryMoved() const { return moved_; }  // the campaign has our battery where the session's arrived

private:
    // The battery at its main position (the task's march over): the campaign's battery moves there, between packages.
    void CarryTheMove() {
        const ScenarioRunner& r = *runner_;
        if (moved_ || live_->Active() || !r.HasTask() || r.TaskPlan().march_at_s < 0.0 || r.Nav() == nullptr) return;
        const Battery* b = r.GetBattery();
        const nav::Anchor* main = r.Nav()->GetAnchors().Find(r.Def().task.main);
        if (b == nullptr || main == nullptr) return;
        const Battery::Vehicle* head = b->Own() != nullptr ? b->Own() : b->Find(r.Def().battery_giver);
        if (head == nullptr || std::hypot(head->position.x - main->at.east_m, head->position.y - main->at.north_m) > kArrivedM) return;
        campaign_->RelocateBattery(ToTheatre(Vec3{main->at.east_m, main->at.north_m, 0.0}));
        moved_ = true;
    }
    static constexpr double kArrivedM = 1000.0;  // sam3d: as the runner's kTaskInAreaM

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
    double h0_ = 0.0;
    bool moved_ = false;
};

}  // namespace campaign
}  // namespace simcore
