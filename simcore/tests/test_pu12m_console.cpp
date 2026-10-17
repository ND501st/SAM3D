// The PU-12M battery command post's console (data/procedures/pu12m.json, docs/PU12_DEVELOPMENT.md P1): five stations,
// the equipment's switching and the deployment drills, run by a crew of five as the Osa's console is.
#include <string>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/procedures.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

std::string Pu12mPath() { return std::string(SIMCORE_DATA_DIR) + "/procedures/pu12m.json"; }

// One crew member per station; one sim step: equipment, buttons released, crew.
class Crew {
public:
    explicit Crew(ConsoleData& c) : c_(c) {
        for (const ConsoleStation& st : c.stations) crew_.emplace_back(st.label, st.id, 1.0);
    }
    CrewMember& At(const std::string& station) {
        for (CrewMember& m : crew_) {
            if (m.Role() == station) return m;
        }
        return crew_.front();
    }
    void Order(const char* id) {
        const Procedure& p = *c_.FindProcedure(id);
        At(p.station).Order(p);
    }
    void Tick(double dt = 0.1) {
        c_.equipment.Update(dt, c_.panel);
        c_.panel.ReleaseMomentaries();
        for (CrewMember& m : crew_) m.Update(dt, c_.panel, &c_.equipment);
    }
    bool Busy() const {
        for (const CrewMember& m : crew_) {
            if (m.IsBusy()) return true;
        }
        return false;
    }
    int Deviations() const {
        int n = 0;
        for (const CrewMember& m : crew_) n += m.Tracker().Deviations();
        return n;
    }

private:
    ConsoleData& c_;
    std::vector<CrewMember> crew_;
};

bool Faulted(ConsoleData& c) {
    bool fault = false;
    for (const SubsystemTransition& tr : c.equipment.TakeTransitions()) {
        if (tr.to == SubsystemState::Fault) {
            std::printf("    fault: %s at %.1f s\n", tr.id.c_str(), tr.time_s);
            fault = true;
        }
    }
    return fault;
}

bool Deployed(const ConsoleData& c) {
    for (const char* id : {"equipment_power", "mast_up", "fug1", "fug2", "fug3", "fug4", "fug5", "rtk1", "oriented", "kp4", "aspd_working", "battery_net"}) {
        if (!c.equipment.IsReady(id)) return false;
    }
    return true;
}

void OrderDeployment(Crew& crew) {
    crew.Order("deploy_power");
    crew.Order("deploy_mast");
    crew.Order("deploy_orient");
    crew.Order("deploy_aspd");
    crew.Order("deploy_command_radios");
}

}  // namespace

SC_TEST("pu12m data: the PU-12M console loads: five stations, every control at one of them") {
    ConsoleData c;
    std::string error;
    SC_EXPECT(LoadConsoleFile(Pu12mPath(), c, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(c.system_id == "pu12m");
    SC_EXPECT(c.stations.size() == 5);
    std::vector<std::string> ids;
    for (const ConsoleStation& st : c.stations) ids.push_back(st.id);
    SC_EXPECT(ids == (std::vector<std::string>{"battery_commander", "squad_leader", "operator", "radio_operator", "driver"}));
    // One drill per seat for the deployment; each seat's drill touches only its own controls (the loader checks).
    for (const char* p : {"deploy_power", "deploy_mast", "deploy_orient", "deploy_aspd", "deploy_command_radios"}) {
        SC_EXPECT(c.FindProcedure(p) != nullptr);
    }
    SC_EXPECT(c.FindProcedure("deploy_power")->station == "driver");
    SC_EXPECT(c.FindProcedure("deploy_mast")->station == "radio_operator");
    SC_EXPECT(c.FindProcedure("deploy_orient")->station == "squad_leader");
    SC_EXPECT(c.FindProcedure("deploy_aspd")->station == "operator");
    SC_EXPECT(c.FindProcedure("deploy_command_radios")->station == "battery_commander");
    SC_EXPECT(c.panel.Find("fr.scale")->position_labels[2] == "М100");
    // Cold in the park: only the 27 V network.
    SC_EXPECT(c.equipment.IsReady("net_27v") && !c.equipment.IsReady("power_220") && !c.equipment.IsReady("fug4"));
}

SC_TEST("pu12m data: a crew of five deploys the post from cold within the 18 min norm") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(Pu12mPath(), c));
    Crew crew(c);
    c.equipment.SetInput("vehicle_stopped", true);
    OrderDeployment(crew);
    double t = 0.0;
    for (; t < 1800.0 && !(Deployed(c) && !crew.Busy()); t += 0.1) crew.Tick();
    std::printf("    deployed in %.0f s (norm: at most 1080 s by day)\n", t);
    SC_EXPECT(Deployed(c) && !crew.Busy());
    SC_EXPECT(t <= 1080.0);
    SC_EXPECT(t > 600.0);  // the radios' 10 min warm-up at least
    SC_EXPECT(crew.Deviations() == 0);
    SC_EXPECT(!Faulted(c));
    // Up on the AB-1 set; the aiming circle stowed again after the orientation, which holds.
    SC_EXPECT(c.equipment.IsReady("ab1_on_line") && c.panel.Get("nav.pab2") == 0 && c.equipment.IsReady("oriented"));
}

SC_TEST("pu12m data: on the move the mast, the generator set and the aiming circle wait for the halt") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(Pu12mPath(), c));
    Crew crew(c);
    c.equipment.SetInput("vehicle_stopped", false);
    OrderDeployment(crew);
    for (int i = 0; i < 1200; ++i) crew.Tick();  // two minutes on the move
    SC_EXPECT(c.panel.Get("mast.raise") == 0 && c.panel.Get("pwr.ab1_set") == 0 && c.panel.Get("nav.pab2") == 0);
    SC_EXPECT(crew.At("radio_operator").Status() == CrewStatus::Waiting && crew.At("driver").Status() == CrewStatus::Waiting);
    // The radios and the gyro are switched on meanwhile (they run on the 27 V network).
    SC_EXPECT(c.panel.Get("radio.fug2.power") == 1 && c.panel.Get("gak.power") == 1 && c.panel.Get("radio.fug4.power") == 1);
    c.equipment.SetInput("vehicle_stopped", true);
    double t = 0.0;
    for (; t < 1800.0 && !(Deployed(c) && !crew.Busy()); t += 0.1) crew.Tick();
    SC_EXPECT(Deployed(c));
    SC_EXPECT(!Faulted(c));
}

SC_TEST("pu12m data: the telecode picture needs the R-407 warm, the ASPD-U working, the parallax set and the regiment sending") {
    ConsoleData c;
    SC_EXPECT(LoadConsoleFile(Pu12mPath(), c));
    Crew crew(c);
    c.equipment.SetInput("vehicle_stopped", true);
    OrderDeployment(crew);
    for (double t = 0.0; t < 1800.0 && !(Deployed(c) && !crew.Busy()); t += 0.1) crew.Tick();
    crew.Tick();
    SC_EXPECT(!c.equipment.IsReady("telecode_reception"));  // nobody sending yet
    c.equipment.SetInput("regiment_telecode", true);
    crew.Tick();
    SC_EXPECT(c.equipment.IsReady("telecode_reception"));
    c.panel.Set("radio.fug5.power", 0);  // the R-407 off: the picture goes
    crew.Tick();
    SC_EXPECT(!c.equipment.IsReady("telecode_reception"));
    // A radar coupled by cable gives the primary picture without any radio.
    c.equipment.SetInput("radar_coupled", true);
    crew.Tick();
    SC_EXPECT(c.equipment.IsReady("primary_picture"));
}

SC_TEST("pu12m data: the readiness states warm what they list; stowing brings the set and the mast back") {
    for (const char* state : {"march", "deployed"}) {
        ConsoleData c;
        SC_EXPECT(LoadConsoleFile(Pu12mPath(), c));
        const ReadinessState* st = c.FindReadiness(state);
        SC_EXPECT(st != nullptr);
        if (st == nullptr) continue;
        for (const auto& kv : st->controls) c.panel.Set(kv.first, kv.second);
        for (const auto& kv : st->inputs) c.equipment.SetInput(kv.first, kv.second);
        c.equipment.Update(0.0, c.panel);
        bool all = true;
        for (const std::string& id : st->ready) {
            const bool warmed = c.equipment.Warm(id, c.panel);
            if (!warmed) std::printf("    %s: %s not commanded on\n", state, id.c_str());
            all = all && warmed;
            c.equipment.Update(0.0, c.panel);
        }
        SC_EXPECT(all);
        if (std::string(state) != "deployed") continue;
        SC_EXPECT(Deployed(c));
        Crew crew(c);
        crew.Order("stow_aspd");
        crew.Order("stow_mast");
        crew.Order("stow_power");
        double t = 0.0;
        for (; t < 1200.0 && (crew.Busy() || c.equipment.IsReady("mast_up") || c.equipment.State("mast_up") == SubsystemState::Stopping ||
                              c.equipment.State("ab1_placed") != SubsystemState::Off);
             t += 0.1) {
            crew.Tick();
        }
        SC_EXPECT(c.panel.Get("mast.raise") == 0 && c.panel.Get("pwr.ab1_set") == 0 && c.panel.Get("aspd.power") == 0);
        SC_EXPECT(c.equipment.State("mast_up") == SubsystemState::Off && c.equipment.State("ab1_placed") == SubsystemState::Off);
        SC_EXPECT(crew.Deviations() == 0 && !Faulted(c));
    }
}
