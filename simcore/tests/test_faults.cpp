// Faults that come about by themselves (simcore/faults.hpp, docs/FAULTS.md): the catalogue, the dice, the crew finding
// them by its instruments, the repairs.

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include "simcore/simcore.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

bool OsaConsole(ConsoleData& c) {
    std::string error;
    const bool ok = LoadConsoleFile(kData + "/procedures/osa_ak.json", c, &error);
    if (!ok) std::printf("    console: %s\n", error.c_str());
    return ok;
}

// A cold Osa with its five crew, no targets; `faults` the scenario's faults block, `inputs` more own_unit inputs,
// `triggers` its triggers.
std::string ColdOsa(const std::string& faults, const std::string& inputs, const std::string& triggers, bool npc_commander = false,
                    unsigned seed = 1) {
    return R"({
      "scenario_id": "f", "name": "F", "source": "sam3d", "confidence": "estimate", "seed": )" + std::to_string(seed) + R"(,
      "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 },
      "terrain": { "type": "flat", "height_m": 0 },)" + std::string(npc_commander ? R"( "crew_commander": "npc",)" : "") + R"(
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json",
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true )" + inputs + R"( } },
      "crew": [ { "name": "Commander", "role": "commander" }, { "name": "Deputy", "role": "deputy_commander" },
                { "name": "Senior operator", "role": "senior_operator" }, { "name": "Operator", "role": "operator" },
                { "name": "Driver", "role": "driver" } ],
      )" + (faults.empty() ? std::string() : "\"faults\": " + faults + ",") + R"(
      "targets": [ ],
      "triggers": [ )" + triggers + R"( ],
      "end": { "time_limit_s": 4000, "when_targets_gone": false } })";
}

const char* kWarmUp = R"({ "id": "go", "when": {}, "do": [ { "order": "power_up", "crew": "Driver" },
        { "order": "equipment_on", "crew": "Operator" } ] })";

std::unique_ptr<ScenarioRunner> Load(const std::string& text) {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenario(text, kData, def, console, &error)) {
        std::printf("    load: %s\n", error.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

const ScenarioRunner::FaultRecord* RecordOf(const ScenarioRunner& r, const std::string& input) {
    const ScenarioRunner::FaultRecord* out = nullptr;
    for (const ScenarioRunner::FaultRecord& f : r.FaultRecords()) {
        if (f.input == input) out = &f;
    }
    return out;
}

template <typename F>
double RunUntil(ScenarioRunner& r, F done, double max_s) {
    while (r.Time() < max_s) {
        r.Step(0.1);
        if (done()) return r.Time();
    }
    return -1.0;
}

}  // namespace

SC_TEST("faults: the Osa's catalogue loads against its console, every fault an input with an instrument and a repair") {
    ConsoleData c;
    SC_EXPECT(OsaConsole(c));
    FaultCatalogue cat;
    std::string error;
    SC_EXPECT(LoadFaultCatalogueFile(kData + "/gameplay/osa_ak_faults.json", c, cat, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(cat.faults.size() == 16);
    for (const FaultDef& f : cat.faults) {
        SC_EXPECT(c.equipment.HasInput(f.input) && !f.found_by.empty() && f.mtbf_h > 0.0);
        SC_EXPECT(f.repair.by == FaultRepair::By::Workshop || f.repair.time_s > 0.0);
    }
    SC_EXPECT(cat.Find("k3_fault") != nullptr && cat.Find("k3_fault")->found_by[0].in_check);
    // An instrument that reads the fault itself is refused: it must show it by what it is set to.
    const std::string bad = R"({ "system_id": "osa_ak", "faults": [ { "input": "k3_fault", "name_en": "x", "name_ru": "x",
        "runs_while": [ { "subsystem": "power", "is": "ready" } ], "mtbf_h": 10,
        "found_by": [ { "seat": "deputy_commander", "setup": [ { "input": "k3_fault", "is": true } ], "shows_en": "x", "shows_ru": "x" } ],
        "repair": { "by": "workshop" }, "source": "sam3d", "confidence": "estimate" } ] })";
    FaultCatalogue refused;
    SC_EXPECT(!LoadFaultCatalogue(bad, c, refused, &error) && error.find("own input") != std::string::npos);
}

SC_TEST("faults: luck is the seed's; the faults already there follow 1 - exp(-hours/mtbf), the maintenance scaling the hours") {
    ConsoleData c;
    SC_EXPECT(OsaConsole(c));
    FaultCatalogue cat;
    cat.system_id = "osa_ak";
    FaultDef f;
    f.input = "k3_fault";
    f.runs_while.push_back(Condition::Subsystem("power", SubsystemState::Ready));
    f.mtbf_h = 200.0;
    cat.faults.push_back(f);
    const auto share = [&](const std::string& maintenance) {
        int there = 0;
        const int n = 4000;
        for (int s = 0; s < n; ++s) {
            ConsoleData copy = c;
            FaultModel m;
            FaultOptions o;
            o.maintenance = maintenance;
            o.since_check_h = 100.0;
            m.Configure(cat, o, static_cast<std::uint64_t>(s));
            there += m.Start(copy).empty() ? 0 : 1;
        }
        return static_cast<double>(there) / n;
    };
    const double kept = share("kept"), overdue = share("overdue");
    std::printf("    100 h on a 200 h mean: %.3f there (kept; 0.393 expected), %.3f (overdue, x4; 0.865)\n", kept, overdue);
    SC_EXPECT_NEAR(kept, 1.0 - std::exp(-0.5), 0.025);
    SC_EXPECT_NEAR(overdue, 1.0 - std::exp(-2.0), 0.02);
    // The same seed, the same luck; the dice off, nothing comes.
    FaultOptions o;
    o.since_check_h = 100.0;
    FaultModel a, b, off;
    a.Configure(cat, o, 7);
    b.Configure(cat, o, 7);
    SC_EXPECT(std::fabs(a.Used("k3_fault") - b.Used("k3_fault")) < 1e-15);
    off.Configure(cat, o, 7);
    off.SetRandom(false);
    ConsoleData copy = c;
    o.since_check_h = 1e9;
    off.Configure(cat, o, 7);
    off.SetRandom(false);
    SC_EXPECT(off.Start(copy).empty() && !copy.equipment.GetInput("k3_fault"));
    // The racks' heat: twice as fast every 10 °C over 40 °C; not known, no effect.
    SC_EXPECT_NEAR(a.HeatFactor(50.0), 2.0, 1e-12);
    SC_EXPECT_NEAR(a.HeatFactor(30.0), 1.0, 1e-12);
    SC_EXPECT_NEAR(a.HeatFactor(std::nan("")), 1.0, 1e-12);
}

SC_TEST("faults: a clock runs only while its circuit does; used up, the fault comes; a repair is a new block with a new life") {
    ConsoleData c;
    SC_EXPECT(OsaConsole(c));
    FaultCatalogue cat;
    cat.system_id = "osa_ak";
    FaultDef f;
    f.input = "k3_fault";
    Condition runs;  // its circuit runs while the vehicle stands (any input will do for the clock)
    runs.kind = Condition::Kind::InputIs;
    runs.id = "vehicle_stopped";
    f.runs_while.push_back(runs);
    f.mtbf_h = 1.0;
    cat.faults.push_back(f);
    FaultModel m;
    FaultOptions o;
    o.since_check_h = 0.0;
    m.Configure(cat, o, 3);
    SC_EXPECT(m.Start(c).empty());
    // Its circuit off: the clock stands.
    c.equipment.SetInput("vehicle_stopped", false);
    for (int i = 0; i < 100; ++i) m.Step(i * 36.0, 36.0, c, 20.0);
    SC_EXPECT(m.WorkedH("k3_fault") == 0.0 && !c.equipment.GetInput("k3_fault"));
    c.equipment.SetInput("vehicle_stopped", true);
    double came = -1.0;
    for (int i = 0; i < 100000 && came < 0.0; ++i) {
        if (!m.Step(i * 36.0, 36.0, c, 20.0).empty()) came = i * 36.0;
    }
    std::printf("    a 1 h mean life: came after %.2f h of power\n", m.WorkedH("k3_fault"));
    SC_EXPECT(came >= 0.0 && c.equipment.GetInput("k3_fault") && m.Used("k3_fault") >= 1.0);
    m.Repaired("k3_fault", c);
    SC_EXPECT(!c.equipment.GetInput("k3_fault") && m.Used("k3_fault") == 0.0 && m.WorkedH("k3_fault") == 0.0);
}

SC_TEST("faults: the deputy finds a K3 fault in the function check by its dark lamp, mends it from ЗИП-1А, and checks again") {
    // The NPC commander brings the cold vehicle to readiness and has the check run (as from cold, between fights).
    auto r = Load(ColdOsa(R"({ "random": false })", R"(, "k3_fault": true)", "", true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(!r->RandomFaults());
    const auto* rec = [&] { r->Step(0.1); return RecordOf(*r, "k3_fault"); }();
    SC_EXPECT(rec != nullptr && rec->scripted && rec->found_s < 0.0);  // there, but nobody knows
    const double found = RunUntil(*r, [&] { const auto* f = RecordOf(*r, "k3_fault"); return f != nullptr && f->found_s >= 0.0; }, 1500);
    rec = RecordOf(*r, "k3_fault");
    SC_EXPECT(found > 0.0 && rec != nullptr && rec->found_by == "deputy_commander");
    std::printf("    found at %.1f min by the %s\n", found / 60.0, rec != nullptr ? rec->found_by.c_str() : "?");
    SC_EXPECT(r->Console().equipment.IsReady("fk_anode"));  // found with the check set's anode on, КЗ pressed
    SC_EXPECT(r->KnownFaults().size() == 1);
    const double repaired = RunUntil(*r, [&] { const auto* f = RecordOf(*r, "k3_fault"); return f != nullptr && f->repaired_s >= 0.0; }, 3000);
    rec = RecordOf(*r, "k3_fault");
    SC_EXPECT(repaired > found + 890.0 && rec != nullptr && rec->repaired_by == "deputy_commander");
    std::printf("    the repair ordered at %.1f min, done at %.1f min\n", rec != nullptr ? rec->repair_from_s / 60.0 : -1.0, repaired / 60.0);
    SC_EXPECT(!r->Console().equipment.GetInput("k3_fault") && r->KnownFaults().empty());
    // The commander has the check run again once the man reports it mended.
    const double again = RunUntil(*r, [&] { return r->Console().panel.Get("fo.mode") == 3; }, repaired + 120.0);
    SC_EXPECT(again > repaired);
    const std::string d = r->Debrief();
    SC_EXPECT(d.find("found by the deputy_commander") != std::string::npos && d.find("repaired by the deputy_commander") != std::string::npos);
}

SC_TEST("faults: the operator's noise check finds a degraded receiver; a fault only the workshop mends stays known, unmended") {
    auto r = Load(ColdOsa(R"({ "random": false })", R"(, "ssc_receiver_degraded": true, "missile_error_fault": true)", "", true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    // The operator's noise check runs while the check does; the deputy reads the meters once it has run its time.
    RunUntil(*r, [&] {
        const auto* f = RecordOf(*r, "ssc_receiver_degraded");
        const auto* g = RecordOf(*r, "missile_error_fault");
        return f != nullptr && f->found_s >= 0.0 && g != nullptr && g->found_s >= 0.0;
    }, 1500);
    const auto* rx = RecordOf(*r, "ssc_receiver_degraded");
    const auto* srp = RecordOf(*r, "missile_error_fault");
    SC_EXPECT(rx != nullptr && rx->found_by == "operator");
    SC_EXPECT(srp != nullptr && srp->found_by == "deputy_commander");  // the КАНАЛ Р meters at nought in the check
    RunUntil(*r, [] { return false; }, 3000);
    rx = RecordOf(*r, "ssc_receiver_degraded");
    srp = RecordOf(*r, "missile_error_fault");
    SC_EXPECT(rx != nullptr && rx->repaired_s > 0.0 && rx->repaired_by == "operator");
    SC_EXPECT(srp != nullptr && srp->repaired_s < 0.0 && srp->repair_from_s < 0.0);  // ЗИП-1Б: not the crew's
    SC_EXPECT(r->KnownFaults().size() == 1 && !r->CheckOrder("order_repair").can);
}

// A failed block is not a tripped one: switching the equipment off and on again mends nothing, so the NPC crew must not
// cycle it (sam3d: with a failed СРП the drill master's recovery did so without end, and the search never came up).
SC_TEST("faults: with a block failed the crew does not cycle the equipment switch; the search radar still comes up") {
    auto r = Load(ColdOsa(R"({ "random": false })", R"(, "srp_fault": true)", "", true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    const double soc = RunUntil(*r, [&] { return r->Console().equipment.IsReady("soc_transmitter"); }, 900);
    RunUntil(*r, [] { return false; }, 900);
    const std::string d = r->Debrief();
    std::size_t cycles = 0;
    for (std::size_t at = d.find("started    order: pwr.equipment"); at != std::string::npos; at = d.find("started    order: pwr.equipment", at + 1)) ++cycles;
    std::printf("    the search radar up at %.0f s; the equipment switch ordered %zu times\n", soc, cycles);
    SC_EXPECT(soc > 0.0);
    SC_EXPECT(cycles <= 1);
    SC_EXPECT(r->Console().equipment.State("fire_control_computer") == SubsystemState::Fault);  // still failed, and known
    SC_EXPECT(r->KnownFaults().size() == 1);
}

// A blown fuse (voenizdat_osa_1991 p166: lamps in 0096-9M1 and 0096-10M; p54: dark in the function check): the
// search radar cannot radiate; the operator sees the lamp, fits a fuse from ЗИП-1А, and the radar comes up after it.
SC_TEST("faults: a blown fuse in the search radar's supply lights its lamp; the operator fits a new one and the radar comes up") {
    auto r = Load(ColdOsa(R"({ "random": false })", R"(, "fuse_soc": true)", "", true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    const double found = RunUntil(*r, [&] { const auto* f = RecordOf(*r, "fuse_soc"); return f != nullptr && f->found_s >= 0.0; }, 1500);
    const auto* rec = RecordOf(*r, "fuse_soc");
    SC_EXPECT(found > 0.0 && rec != nullptr && rec->found_by == "operator");
    SC_EXPECT(!r->Console().equipment.IsReady("soc_transmitter"));
    const double repaired = RunUntil(*r, [&] { const auto* f = RecordOf(*r, "fuse_soc"); return f != nullptr && f->repaired_s >= 0.0; }, 3000);
    const double soc = RunUntil(*r, [&] { return r->Console().equipment.IsReady("soc_transmitter"); }, 3000);
    std::printf("    the fuse found at %.1f min, a new one in at %.1f min, the search radar up at %.1f min\n", found / 60.0, repaired / 60.0,
                soc / 60.0);
    SC_EXPECT(repaired > found + 110.0);
    SC_EXPECT(soc > repaired);
    SC_EXPECT(r->KnownFaults().empty());
}

// A stabilisation drive's fault puts it on its hard stop (p47-48): ПРЕД. УГОЛ, the search off. Stowing puts it out only if
// nothing is broken (p48); this drive is, so the табло stays lit and the workshop must mend it. The crew must not stow and
// raise the antenna for ever (sam3d: the drill master's way out of the trip did so every two seconds).
SC_TEST("faults: a drive on its stop lights ПРЕД. УГОЛ; the commander knows it for the workshop's; no endless stowing") {
    auto r = Load(ColdOsa(R"({ "random": false })", R"(, "stab_limit": true)", "", true));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    const double found = RunUntil(*r, [&] { const auto* f = RecordOf(*r, "stab_limit"); return f != nullptr && f->found_s >= 0.0; }, 1500);
    const auto* rec = RecordOf(*r, "stab_limit");
    SC_EXPECT(found > 0.0 && rec != nullptr && rec->found_by == "commander");
    int last = r->Console().panel.Get("apu.stow"), moves = 0;
    RunUntil(*r, [&] {
        const int now = r->Console().panel.Get("apu.stow");
        moves += now != last ? 1 : 0;
        last = now;
        return false;
    }, 2400);
    std::printf("    ПРЕД. УГОЛ known at %.1f min; ПРОХОД ПОД МОСТОМ worked %d times after\n", found / 60.0, moves);
    SC_EXPECT(r->Console().equipment.IsReady("stab_limit_tripped"));  // still lit: the drive is still broken
    SC_EXPECT(!r->Console().equipment.IsReady("soc_antenna"));
    SC_EXPECT(r->KnownFaults().size() == 1 && !r->CheckOrder("order_repair").can);  // ЗИП-1Б: the workshop's
    SC_EXPECT(moves <= 2);
}

SC_TEST("faults: a fault no instrument shows stays unknown; the dice off leave a sound vehicle; on, an overdue one is full of them") {
    // The receiver degraded with no check run: nobody knows.
    auto quiet = Load(ColdOsa(R"({ "random": false })", R"(, "soc_receiver_degraded": true)", kWarmUp));
    SC_EXPECT(quiet != nullptr);
    if (!quiet) return;
    RunUntil(*quiet, [] { return false; }, 600);
    SC_EXPECT(quiet->KnownFaults().empty() && quiet->Console().equipment.GetInput("soc_receiver_degraded"));
    // A vehicle a thousand hours overdue: every fault there at the start, unless the menu's Faults is off.
    const std::string worn = R"({ "maintenance": "overdue", "since_check_h": 100000 })";
    auto off = Load(ColdOsa(worn, "", kWarmUp));
    auto on = Load(ColdOsa(worn, "", kWarmUp));
    SC_EXPECT(off != nullptr && on != nullptr);
    if (!off || !on) return;
    off->SetRandomFaults(false);
    off->Step(0.1);
    on->Step(0.1);
    off->SetRandomFaults(true);  // too late: the session has started
    SC_EXPECT(off->FaultRecords().empty() && !off->RandomFaults());
    SC_EXPECT(on->FaultRecords().size() == 16 && on->RandomFaults());
    for (const auto& f : on->FaultRecords()) SC_EXPECT(!f.scripted && f.came_s >= 0.0);
}

SC_TEST("faults: the instructor puts a fault on and takes it off; the crew learns of it only by its lamp") {
    auto r = Load(ColdOsa(R"({ "random": false })", "", kWarmUp));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    RunUntil(*r, [&] { return r->Console().equipment.IsReady("power"); }, 300);
    InstructorOrder o;
    o.kind = InstructorOrder::Kind::Fault;
    o.target = "scanner_phase_break";
    o.value = 1.0;
    std::string why;
    SC_EXPECT(r->Instruct(o, &why));
    SC_EXPECT(!r->Instruct(o, &why) && why.find("already") != std::string::npos);
    RunUntil(*r, [] { return false; }, r->Time() + 5.0);
    const auto* rec = RecordOf(*r, "scanner_phase_break");
    SC_EXPECT(rec != nullptr && rec->scripted && rec->found_by == "operator");  // ОБРЫВ ФАЗЫ ГОН lit
    o.value = 0.0;
    SC_EXPECT(r->Instruct(o, &why));
    r->Step(0.1);
    rec = RecordOf(*r, "scanner_phase_break");
    SC_EXPECT(rec != nullptr && (rec->gone_s >= 0.0 || rec->repaired_s >= 0.0) && r->KnownFaults().empty());
    o.target = "no_such_fault";
    SC_EXPECT(!r->Instruct(o, &why));
}
