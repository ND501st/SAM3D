#include <string>
#include <vector>

#include "placeholder_vehicle.hpp"
#include "simcore/equipment.hpp"
#include "simcore/procedures.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

struct Rig {
    ControlPanel panel = placeholder::BuildPanel();
    Equipment eq;
    bool built = placeholder::BuildEquipment(eq, panel);

    // Equipment first, so crew see this step's lamps.
    void Tick(double dt, CrewMember* crew = nullptr) {
        eq.Update(dt, panel);
        if (crew != nullptr) crew->Update(dt, panel, &eq);
    }
    void Run(double seconds, CrewMember* crew = nullptr, double dt = 0.1) {
        for (double t = 0.0; t < seconds - 1e-9; t += dt) Tick(dt, crew);
    }
    void StartGenerator() {
        panel.Set("pwr.key", 1);
        panel.Set("pwr.key", 2);
    }
    void PowerAndRaiseMast() {
        panel.Set("veh.brake", 1);
        StartGenerator();
        Run(10.1);
        panel.Set("pwr.main", 1);
        panel.Set("ant.mast", 1);
    }
};

bool HasReport(const std::vector<CrewReport>& reports, CrewReport::Kind kind, const std::string& detail = "") {
    for (const CrewReport& r : reports) {
        if (r.kind == kind && (detail.empty() || r.detail == detail)) return true;
    }
    return false;
}

}  // namespace

SC_TEST("panel: covers and key switches block physically") {
    ControlPanel panel = placeholder::BuildPanel();
    SC_EXPECT(!panel.AddControl({"x", "guard added later", 2, 0, "not_yet"}));
    SC_EXPECT(!panel.AddControl({"y", "bad initial", 2, 2}));
    SC_EXPECT(panel.IsBlocked("srch.hv", 1));
    SC_EXPECT(!panel.Set("srch.hv", 1));
    SC_EXPECT(panel.Set("srch.hv_cover", 1));
    SC_EXPECT(panel.Set("srch.hv", 1));
    SC_EXPECT(!panel.Set("pwr.key", 2));  // must insert the key before turning it
    SC_EXPECT(panel.Set("pwr.key", 1) && panel.Set("pwr.key", 2));
    SC_EXPECT(!panel.Set("pwr.key", 0));  // and turn it back before pulling it out
    SC_EXPECT(!panel.IsBlocked("pwr.key", 7));  // invalid, not blocked
}

SC_TEST("equipment: rejects bad definitions") {
    ControlPanel panel = placeholder::BuildPanel();
    Equipment eq;
    SC_EXPECT(eq.AddSubsystem({"a", "A", {Condition::Control("pwr.main", 1)}, {}, 1.0, 1.0}, panel));
    SC_EXPECT(!eq.AddSubsystem({"a", "dup", {}, {}, 0.0, 0.0}, panel));
    SC_EXPECT(!eq.AddSubsystem({"", "no id", {}, {}, 0.0, 0.0}, panel));
    SC_EXPECT(!eq.AddSubsystem({"b", "unknown control", {Condition::Control("nope", 1)}, {}, 0.0, 0.0}, panel));
    SC_EXPECT(!eq.AddSubsystem({"b", "bad position", {Condition::Control("pwr.main", 5)}, {}, 0.0, 0.0}, panel));
    SC_EXPECT(!eq.AddSubsystem({"b", "forward ref", {}, {Condition::Ready("later")}, 0.0, 0.0}, panel));
    SC_EXPECT(!eq.AddSubsystem({"b", "negative time", {}, {}, -1.0, 0.0}, panel));
    SC_EXPECT(eq.AddSubsystem({"b", "B", {}, {Condition::Ready("a")}, 0.0, 0.0}, panel));
    Rig rig;
    SC_EXPECT(rig.built);
}

SC_TEST("equipment: warm-up runs Off -> Starting -> Ready on time") {
    Rig rig;
    rig.StartGenerator();
    rig.Tick(0.1);
    SC_EXPECT(rig.eq.State("generator") == SubsystemState::Starting);
    rig.Run(9.9);  // counting starts the tick after the switch
    SC_EXPECT(rig.eq.State("generator") == SubsystemState::Starting);
    SC_EXPECT_NEAR(rig.eq.Progress("generator"), 0.99, 1e-6);
    rig.Tick(0.1);
    SC_EXPECT(rig.eq.IsReady("generator"));
    const std::vector<SubsystemTransition> tr = rig.eq.TakeTransitions();
    SC_EXPECT(tr.size() == 2);
    SC_EXPECT(tr[0].to == SubsystemState::Starting && tr[1].to == SubsystemState::Ready);
    SC_EXPECT(rig.eq.TakeTransitions().empty());
}

SC_TEST("equipment: energising before a prerequisite faults; switching off clears it") {
    Rig rig;
    rig.panel.Set("pwr.main", 1);  // generator not running
    rig.Tick(0.1);
    SC_EXPECT(rig.eq.State("main_bus") == SubsystemState::Fault);
    rig.StartGenerator();
    rig.Run(15.0);
    SC_EXPECT(rig.eq.State("main_bus") == SubsystemState::Fault);  // latched until switched off
    rig.panel.Set("pwr.main", 0);
    rig.Tick(0.1);
    SC_EXPECT(rig.eq.State("main_bus") == SubsystemState::Off);
    rig.panel.Set("pwr.main", 1);
    rig.Tick(0.1);
    SC_EXPECT(rig.eq.IsReady("main_bus"));  // zero start time: ready in the same step
}

SC_TEST("equipment: losing a prerequisite while running faults (brake released, mast up)") {
    Rig rig;
    rig.PowerAndRaiseMast();
    rig.Run(21.0);
    SC_EXPECT(rig.eq.IsReady("mast"));
    SC_EXPECT(!placeholder::CanDrive(rig.eq));
    rig.panel.Set("veh.brake", 0);
    rig.Tick(0.1);
    SC_EXPECT(rig.eq.State("mast") == SubsystemState::Fault);
}

SC_TEST("equipment: reversing half-way only undoes what was done") {
    Rig rig;
    rig.PowerAndRaiseMast();
    rig.Tick(0.1);  // mast starts
    rig.Run(10.0);  // half raised (20 s start time)
    SC_EXPECT_NEAR(rig.eq.Progress("mast"), 0.5, 1e-6);
    rig.panel.Set("ant.mast", 0);
    rig.Tick(0.1);
    SC_EXPECT(rig.eq.State("mast") == SubsystemState::Stopping);
    rig.Run(9.8);
    SC_EXPECT(rig.eq.State("mast") == SubsystemState::Stopping);
    rig.Run(0.2);
    SC_EXPECT(rig.eq.State("mast") == SubsystemState::Off);
    SC_EXPECT(placeholder::CanDrive(rig.eq));
}

SC_TEST("tracker: early and out-of-order actions are logged; early steps still advance; blocks aren't judged") {
    Rig rig;
    ProcedureTracker tr(placeholder::PowerUpAndDeploy());
    SC_EXPECT(tr.Attempt(rig.panel, "veh.brake", 1, &rig.eq) == StepResult::Accepted);
    SC_EXPECT(tr.Attempt(rig.panel, "pwr.key", 2, &rig.eq) == StepResult::Blocked);  // key not inserted
    SC_EXPECT(tr.Deviations() == 0);
    SC_EXPECT(tr.Attempt(rig.panel, "pwr.key", 1, &rig.eq) == StepResult::Accepted);
    SC_EXPECT(tr.Attempt(rig.panel, "pwr.key", 2, &rig.eq) == StepResult::Accepted);
    rig.Tick(0.1);
    tr.Update(0.1);
    // Main bus before the generator is ready: allowed, judged early, and the bus faults.
    SC_EXPECT(tr.Attempt(rig.panel, "pwr.main", 1, &rig.eq) == StepResult::AcceptedEarly);
    rig.Tick(0.1);
    SC_EXPECT(rig.eq.State("main_bus") == SubsystemState::Fault);
    SC_EXPECT(tr.NextIndex() == 4);
    tr.Update(0.1);
    SC_EXPECT(tr.Attempt(rig.panel, "srch.fil", 1, &rig.eq) == StepResult::NotNextStep);
    SC_EXPECT(tr.Deviations() == 2);
    const std::vector<Deviation>& log = tr.DeviationLog();
    SC_EXPECT(log[0].kind == DeviationKind::Early && log[0].control_id == "pwr.main" && log[0].expected_step == 3);
    SC_EXPECT(log[1].kind == DeviationKind::OutOfOrder && log[1].control_id == "srch.fil" && log[1].expected_step == 4);
    SC_EXPECT_NEAR(log[1].time_s, 0.2, 1e-9);
}

SC_TEST("crew: powers up, deploys and stows the placeholder vehicle with no deviations") {
    Rig rig;
    SC_EXPECT(placeholder::CanDrive(rig.eq));
    CrewMember crew("Sgt. Placeholder", "commander", 1.0);
    crew.Order(placeholder::PowerUpAndDeploy());
    double t = 0.0;
    while (crew.IsBusy() && t < 300.0) {
        rig.Tick(0.1, &crew);
        t += 0.1;
    }
    SC_EXPECT(crew.Tracker().IsComplete() && crew.Tracker().Deviations() == 0);
    SC_EXPECT(rig.eq.IsReady("search_tx") && rig.panel.Get("srch.mode") == 2);
    SC_EXPECT(!placeholder::CanDrive(rig.eq));
    // Filament starts ~16 s in and takes 60 s; then HV, 5 s transmitter warm-up, and mode.
    SC_EXPECT(t > 79.0 && t < 87.0);
    for (const SubsystemTransition& tr : rig.eq.TakeTransitions()) SC_EXPECT(tr.to != SubsystemState::Fault);

    crew.Queue(placeholder::Stow());
    while (crew.IsBusy() && t < 600.0) {
        rig.Tick(0.1, &crew);
        t += 0.1;
    }
    SC_EXPECT(crew.Tracker().IsComplete() && crew.Tracker().Deviations() == 0);
    SC_EXPECT(placeholder::CanDrive(rig.eq) && rig.panel.Get("veh.brake") == 0 && rig.panel.Get("pwr.key") == 0);
    SC_EXPECT(crew.Status() == CrewStatus::Idle);

    const std::vector<CrewReport> reports = crew.TakeReports();
    SC_EXPECT(reports.front().kind == CrewReport::Kind::Started && reports.front().procedure_id == "power_up_deploy");
    SC_EXPECT(reports.back().kind == CrewReport::Kind::Completed && reports.back().procedure_id == "stow");
    SC_EXPECT(HasReport(reports, CrewReport::Kind::Waiting, "search_fil"));
    SC_EXPECT(!HasReport(reports, CrewReport::Kind::EquipmentFault));
}

SC_TEST("crew: queued orders run in order after the current one") {
    Rig rig;
    CrewMember crew("Cpl. Placeholder", "operator", 0.5);
    crew.Order(placeholder::PowerUpAndDeploy());
    crew.Queue(placeholder::Stow());
    SC_EXPECT(crew.QueuedCount() == 1);
    for (int i = 0; i < 6000 && crew.IsBusy(); ++i) rig.Tick(0.1, &crew);
    SC_EXPECT(!crew.IsBusy() && crew.QueuedCount() == 0);
    SC_EXPECT(crew.Tracker().GetProcedure().id == "stow" && crew.Tracker().IsComplete());
    SC_EXPECT(placeholder::CanDrive(rig.eq));
}

SC_TEST("crew: stops and reports when equipment it waits on faults") {
    Rig rig;
    CrewMember crew("Sgt. Placeholder", "commander", 1.0);
    crew.Order(placeholder::PowerUpAndDeploy());
    crew.Queue(placeholder::Stow());
    rig.Run(30.0, &crew);  // mast raising, filament warming
    SC_EXPECT(rig.eq.State("mast") == SubsystemState::Starting);
    rig.panel.Set("veh.brake", 0);  // someone releases the brake
    rig.Run(60.0, &crew);
    SC_EXPECT(rig.eq.State("mast") == SubsystemState::Fault);
    SC_EXPECT(!crew.IsBusy() && crew.QueuedCount() == 0);
    SC_EXPECT(HasReport(crew.TakeReports(), CrewReport::Kind::EquipmentFault, "mast"));
}

SC_TEST("crew: a procedure that forgets to open a cover is aborted, not looped") {
    Rig rig;
    Procedure p{"bad", "HV without cover", {{"srch.hv", 1, 0.0, "Search HV ON", {}}}};
    CrewMember crew("Cpl. Placeholder", "operator", 0.5);
    crew.Order(p);
    rig.Run(5.0, &crew);
    SC_EXPECT(!crew.IsBusy() && rig.panel.Get("srch.hv") == 0);
    SC_EXPECT(HasReport(crew.TakeReports(), CrewReport::Kind::Aborted, "srch.hv"));
}

SC_TEST("crew: a new order supersedes; cancel clears the queue") {
    Rig rig;
    CrewMember crew("Cpl. Placeholder", "operator", 1.0);
    crew.Order(placeholder::PowerUpAndDeploy());
    rig.Run(5.0, &crew);
    crew.Order(placeholder::Stow());
    crew.Queue(placeholder::PowerUpAndDeploy());
    crew.CancelOrder();
    SC_EXPECT(!crew.IsBusy() && crew.QueuedCount() == 0 && crew.Status() == CrewStatus::Idle);
    const std::vector<CrewReport> reports = crew.TakeReports();
    SC_EXPECT(HasReport(reports, CrewReport::Kind::Aborted, "superseded"));
    SC_EXPECT(HasReport(reports, CrewReport::Kind::Aborted, "cancelled"));
}
