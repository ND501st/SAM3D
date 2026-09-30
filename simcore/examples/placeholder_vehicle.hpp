// PLACEHOLDER content: a generic mobile SAM vehicle used to exercise the panel,
// equipment, procedure and crew code.
//
// The control TYPES match what SAM Simulator's Osa panels use (see docs/re/osa.md):
// a key switch (out / in / turned), a covered switch, multi-position selectors.
// The control names, step order and times are invented round numbers and are NOT
// data for the Osa or any other real system. Real content will come from SAM
// Simulator's behaviour and public operator manuals, loaded from data files
// (docs/DEVELOPMENT_ROADMAP.md, tasks 1.1 and 1.3).
#pragma once

#include "simcore/equipment.hpp"
#include "simcore/panel.hpp"
#include "simcore/procedures.hpp"

namespace placeholder {

using simcore::Condition;

inline simcore::ControlPanel BuildPanel() {
    simcore::ControlPanel panel;
    panel.AddControl({"veh.brake", "PARKING BRAKE", 2, 0});
    panel.AddControl({"pwr.key", "GENERATOR KEY", 3, 0, "", true});  // 0 out, 1 inserted, 2 turned
    panel.AddControl({"pwr.main", "MAIN BUS", 2, 0});
    panel.AddControl({"ant.mast", "MAST", 2, 0});
    panel.AddControl({"srch.fil", "SEARCH FILAMENT", 2, 0});
    panel.AddControl({"srch.hv_cover", "SEARCH HV COVER", 2, 0});
    panel.AddControl({"srch.hv", "SEARCH HV", 2, 0, "srch.hv_cover"});
    panel.AddControl({"srch.mode", "SEARCH MODE", 3, 0});  // 0 standby, 1 sector, 2 circular
    return panel;
}

// Returns false if the definitions don't match the panel (a bug in this file).
inline bool BuildEquipment(simcore::Equipment& eq, const simcore::ControlPanel& panel) {
    bool ok = true;
    ok = ok && eq.AddSubsystem({"generator", "Generator", {Condition::Control("pwr.key", 2)}, {}, 10.0, 5.0}, panel);
    ok = ok && eq.AddSubsystem(
                   {"main_bus", "Main bus", {Condition::Control("pwr.main", 1)}, {Condition::Ready("generator")}, 0.0, 0.0},
                   panel);
    ok = ok && eq.AddSubsystem({"mast", "Antenna mast", {Condition::Control("ant.mast", 1)},
                                {Condition::Control("veh.brake", 1), Condition::Ready("main_bus")}, 20.0, 20.0},
                               panel);
    ok = ok && eq.AddSubsystem({"search_fil", "Search filament", {Condition::Control("srch.fil", 1)},
                                {Condition::Ready("main_bus")}, 60.0, 0.0},
                               panel);
    ok = ok && eq.AddSubsystem({"search_tx", "Search transmitter", {Condition::Control("srch.hv", 1)},
                                {Condition::Ready("search_fil"), Condition::Ready("mast")}, 5.0, 0.0},
                               panel);
    return ok;
}

inline simcore::Procedure PowerUpAndDeploy() {
    return {"power_up_deploy",
            "Power up and deploy (placeholder)",
            {{"veh.brake", 1, 0.0, "Set parking brake", {}},
             {"pwr.key", 1, 0.0, "Insert generator key", {}},
             {"pwr.key", 2, 0.0, "Turn generator key", {}},
             {"pwr.main", 1, 0.0, "Main bus ON", {Condition::Ready("generator")}},
             {"ant.mast", 1, 0.0, "Raise mast", {Condition::Ready("main_bus")}},
             {"srch.fil", 1, 0.0, "Search filament ON", {Condition::Ready("main_bus")}},
             {"srch.hv_cover", 1, 0.0, "Open search HV cover", {}},
             {"srch.hv", 1, 0.0, "Search HV ON", {Condition::Ready("search_fil"), Condition::Ready("mast")}},
             {"srch.mode", 2, 0.0, "Search mode CIRCULAR", {Condition::Ready("search_tx")}}}};
}

inline simcore::Procedure Stow() {
    return {"stow",
            "Stow for travel (placeholder)",
            {{"srch.mode", 0, 0.0, "Search mode STANDBY", {}},
             {"srch.hv", 0, 0.0, "Search HV OFF", {}},
             {"srch.hv_cover", 0, 0.0, "Close search HV cover", {}},
             {"srch.fil", 0, 0.0, "Search filament OFF", {}},
             {"ant.mast", 0, 0.0, "Lower mast", {}},
             {"pwr.main", 0, 0.0, "Main bus OFF", {Condition::Stopped("mast")}},
             {"pwr.key", 1, 0.0, "Turn generator key back", {}},
             {"pwr.key", 0, 0.0, "Remove generator key", {}},
             {"veh.brake", 0, 0.0, "Release parking brake", {Condition::Stopped("mast")}}}};
}

// The vehicle may only drive with the mast fully down.
inline bool CanDrive(const simcore::Equipment& eq) { return eq.State("mast") == simcore::SubsystemState::Off; }

}  // namespace placeholder
