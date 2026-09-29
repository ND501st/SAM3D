#include "simcore/procedures.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {
// Illustrative panel/procedure only — real content comes from SAM Sim + manuals.
ControlPanel ExamplePanel() {
    ControlPanel panel;
    panel.AddControl({"pwr.main", "MAIN POWER", 2, 0});
    panel.AddControl({"srch.filament", "SEARCH FILAMENT", 2, 0});
    panel.AddControl({"srch.hv", "SEARCH HV", 2, 0});
    panel.AddControl({"srch.range_scale", "RANGE SCALE", 3, 0});
    return panel;
}

Procedure ExamplePowerUp() {
    return Procedure{"search_power_up", "Search radar power-up",
                     {{"pwr.main", 1, 0.0, "Main power ON"},
                      {"srch.filament", 1, 0.0, "Filament ON"},
                      {"srch.hv", 1, 30.0, "High voltage ON after 30 s warm-up"},
                      {"srch.range_scale", 2, 0.0, "Range scale to 2"}}};
}
}  // namespace

SC_TEST("panel: add, get, set with validation") {
    ControlPanel panel = ExamplePanel();
    SC_EXPECT(!panel.AddControl({"pwr.main", "dup", 2, 0}));
    SC_EXPECT(panel.Get("pwr.main") == 0);
    SC_EXPECT(panel.Get("nope") == -1);
    SC_EXPECT(panel.Set("srch.range_scale", 2));
    SC_EXPECT(!panel.Set("srch.range_scale", 3));
    SC_EXPECT(panel.Get("srch.range_scale") == 2);
}

SC_TEST("procedure: correct order is accepted; warm-up enforced; deviations counted") {
    ControlPanel panel = ExamplePanel();
    ProcedureTracker tr(ExamplePowerUp());
    SC_EXPECT(tr.Attempt(panel, "srch.hv", 1) == StepResult::NotNextStep);  // out of order
    SC_EXPECT(tr.Deviations() == 1);
    panel.Set("srch.hv", 0);
    SC_EXPECT(tr.Attempt(panel, "pwr.main", 1) == StepResult::Accepted);
    SC_EXPECT(tr.Attempt(panel, "srch.filament", 1) == StepResult::Accepted);
    tr.Update(10.0);
    SC_EXPECT(tr.Attempt(panel, "srch.hv", 1) == StepResult::WaitRequired);
    SC_EXPECT(panel.Get("srch.hv") == 0);
    tr.Update(25.0);
    SC_EXPECT(tr.Attempt(panel, "srch.hv", 1) == StepResult::Accepted);
    SC_EXPECT(tr.Attempt(panel, "srch.range_scale", 2) == StepResult::Accepted);
    SC_EXPECT(tr.IsComplete());
    SC_EXPECT(tr.Attempt(panel, "bogus", 1) == StepResult::UnknownControl);
    SC_EXPECT(tr.Attempt(panel, "pwr.main", 7) == StepResult::InvalidPosition);
}

SC_TEST("crew: NPC completes an ordered procedure with reaction time and warm-up") {
    ControlPanel panel = ExamplePanel();
    CrewMember op("Sgt. Petrov", "search_operator", 1.5);
    op.Order(ExamplePowerUp());
    double t = 0.0;
    const double dt = 0.1;
    while (op.IsBusy() && t < 120.0) {
        op.Update(dt, panel);
        t += dt;
    }
    SC_EXPECT(!op.IsBusy());
    SC_EXPECT(op.Tracker().IsComplete());
    SC_EXPECT(op.Tracker().Deviations() == 0);
    SC_EXPECT(panel.Get("srch.hv") == 1 && panel.Get("srch.range_scale") == 2);
    // 1.5 + 1.5 + 30 (warm-up dominates reaction) + 1.5 = ~34.5 s
    SC_EXPECT(t > 33.0 && t < 36.5);
}

SC_TEST("crew: player and NPC share the same panel state") {
    ControlPanel panel = ExamplePanel();
    panel.Set("pwr.main", 1);  // the player already did step 1 by hand
    CrewMember op("Cpl. Ivanov", "search_operator", 1.0);
    Procedure p = ExamplePowerUp();
    p.steps.erase(p.steps.begin());  // order only the remaining steps
    op.Order(p);
    for (int i = 0; i < 500 && op.IsBusy(); ++i) op.Update(0.1, panel);
    SC_EXPECT(panel.Get("pwr.main") == 1 && panel.Get("srch.hv") == 1);
}
