// simcore/campaign/battery_state.hpp — the battery between its tasks (roadmap 15.1, first form; track O's O1,
// docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md section 10).
//
// A campaign is one long battery mission with the enemy's air war on its clock (mission_session.hpp), saved at its seams
// and resumed there (campaign_save.hpp). What of the battery carries over a save: where each vehicle stood and whether
// the battery was set up, each launcher's missiles and the TZM's load, the technical battery's stocks, the task it held
// and whether it had reported ready on it, and the hour. A resumed session is the campaign's mission with this applied
// (ApplyBatteryState): a task it was ready on is held (ScenarioDef::task_held: nobody gives it again), a task not yet
// carried out is given again a minute into the session and the battery commander plans it from where the battery stands.
// Not carried (left): a vehicle on a run of its own or broken down (it is back in the battery), a launcher's own regime
// inside the order's ceiling, the crews' needs and records.
#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "simcore/battery.hpp"
#include "simcore/campaign/battery_task.hpp"
#include "simcore/geo.hpp"
#include "simcore/scenario.hpp"

namespace simcore {
namespace campaign {

struct SavedVehicle {
    std::string id;
    Vec3 position;       // the session's frame
    int missiles = -1;   // a launcher's (< 0: not known, a full load)
    double stock = -1.0; // a TZM's missiles, a fuel truck's litres (< 0: not one)
};

struct BatteryState {
    bool present = false;
    double campaign_h = 0.0;   // the air war's hour at the save
    double local_h = 0.0;      // the session's clock then (0-24)
    int day_of_year = 288;     // and its date (the sun's)
    Vec3 own;                  // the player's vehicle
    bool deployed = true;      // every launcher set up where it stood
    std::vector<SavedVehicle> vehicles;
    int siding_tested = -1;    // the technical battery's stocks (< 0: the mission's own)
    int siding_untested = -1;
    bool has_task = false;
    BatteryTask task;
    bool task_ready = false;   // reported ready on it
    std::string at;            // the task's position the campaign has the battery at ("": where it started)
    double arrived_h = -1.0;   // the campaign hour it came there (< 0: not yet; 0: there from the war's start)
};

// A task not carried out at the save is given again this far into the resumed session (as the bridges mission's order).
constexpr double kTaskGivenAgainS = 60.0;

// The campaign's mission with the battery as it was saved.
inline void ApplyBatteryState(ScenarioDef& def, const BatteryState& s) {
    if (!s.present) return;
    def.clock.day_of_year = s.day_of_year;
    SetStartHour(def, s.local_h);
    def.unit_position = s.own;
    def.battery_deployed = s.deployed;
    for (BatteryVehicleDef& v : def.battery_vehicles) {
        if (v.own) {
            v.start = s.own;
            continue;
        }
        for (const SavedVehicle& sv : s.vehicles) {
            if (sv.id != v.id) continue;
            v.start = sv.position;
            if (v.kind == BatteryVehicleKind::Launcher) v.missiles = sv.missiles;
            if ((v.kind == BatteryVehicleKind::Reload || v.kind == BatteryVehicleKind::Fuel) && sv.stock >= 0.0) v.stock = sv.stock;
        }
    }
    if (s.siding_tested >= 0) def.siding_tested = s.siding_tested;
    if (s.siding_untested >= 0) def.siding_untested = s.siding_untested;
    def.has_task = s.has_task;
    def.task_held = s.has_task && s.task_ready;
    if (s.has_task) {
        def.task = s.task;
        def.task_given_s = s.task_ready ? 0.0 : kTaskGivenAgainS;
    }
}

}  // namespace campaign
}  // namespace simcore
