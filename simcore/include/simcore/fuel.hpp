// simcore/fuel.hpp — the vehicle's fuel (roadmap 7.4).
//
// One tank feeds the gas-turbine power unit, the engine's take-off generator and the drive. The console's "fuel" block
// gives the tank and how fast each power source burns it, stationary and moving (ePap: about 300 l; turbine 75 l/h
// stationary and 100 l/h moving; take-off generator 25 l/h). Half the tank is kept for driving: when the level falls to
// the reserve the crew asks for fuel, and the power sources are shut down (their enable conditions include the
// power-cut input) so the rest stays in the tank. The TZM refuels the vehicle at a halt.
#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "simcore/console_data.hpp"

namespace simcore {

class FuelModel {
public:
    struct Events {
        bool requested = false;  // the level has just fallen to the reserve: the crew asks for fuel
        bool refuelled = false;  // the tank has just been filled
    };

    explicit FuelModel(const FuelDef& def, double start_fraction = 1.0)
        : def_(def), level_l_(def.tank_l * std::min(1.0, std::max(0.0, start_fraction))) {}

    // One step. Burns fuel for the power sources that are running, adds what the TZM pumps, and sets the power-cut input.
    Events Step(double dt, ConsoleData& console) {
        Events ev;
        Equipment& eq = console.equipment;
        const bool stopped = eq.GetInput(def_.stopped_input);
        double burn_lph = 0.0;
        for (const FuelSource& s : def_.sources) {
            if (eq.State(s.subsystem) != SubsystemState::Off) burn_lph += stopped ? s.stationary_lph : s.moving_lph;
        }
        level_l_ = std::max(0.0, level_l_ - burn_lph * dt / 3600.0);
        burned_l_ += burn_lph * dt / 3600.0;
        if (def_.refuel_lpm > 0.0 && eq.GetInput(def_.refuel_input) && stopped && level_l_ < def_.tank_l) {
            const double before = level_l_;
            level_l_ = std::min(def_.tank_l, level_l_ + def_.refuel_lpm * dt / 60.0);
            if (before < def_.tank_l && level_l_ >= def_.tank_l - 1e-9) ev.refuelled = true;
        }
        const bool low = level_l_ <= Reserve() + 1e-9;
        if (low && !requested_) {
            requested_ = true;
            ev.requested = true;
        }
        if (!low && level_l_ > Reserve() + def_.refuel_hysteresis_l) requested_ = false;  // asks again after a refuel
        if (!def_.power_cut_input.empty()) eq.SetInput(def_.power_cut_input, low);
        return ev;
    }

    double LevelL() const { return level_l_; }
    double Fraction() const { return def_.tank_l > 0.0 ? level_l_ / def_.tank_l : 0.0; }
    double Reserve() const { return def_.tank_l * def_.reserve_fraction; }
    double BurnedL() const { return burned_l_; }
    bool PowerCut() const { return level_l_ <= Reserve() + 1e-9; }

private:
    FuelDef def_;
    double level_l_ = 0.0;
    double burned_l_ = 0.0;
    bool requested_ = false;
};

}  // namespace simcore
