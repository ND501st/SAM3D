// The crew out of their seats (docs/crew/13_OUT_OF_THE_VEHICLE.md, simcore/crew_mobility.hpp): the vehicles' hatches and
// hulls, the pace over the ground, the voice's carry, a crewman's way out through a hatch and back, and the runner
// sending a crewman out for a job done outside, keeping the hatches shut in combat work and the voices in earshot.
#include <cmath>
#include <string>

#include "simcore/cabin.hpp"
#include "simcore/console_data.hpp"
#include "simcore/crew_mobility.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

struct Loaded {
    ConsoleData console;
    VehicleDef vehicle;
    WalkingDef walking;
    bool ok = false;
};

Loaded LoadVehicleOf(const std::string& system) {
    Loaded l;
    std::string e;
    l.ok = LoadConsoleFile(kData + "/procedures/" + system + ".json", l.console, &e) &&
           LoadVehicleFile(kData + "/vehicles/" + system + ".json", l.console, l.vehicle, &e) &&
           LoadWalkingFile(kData + "/crew/walking.json", l.walking, &e);
    if (!l.ok) std::printf("    load: %s\n", e.c_str());
    return l;
}

// Walks a mover until he arrives (or max_s), returning the seconds it took; checks on the way that his feet on the ground
// never pass under the hull and that he is never in a keep-out up top.
double WalkUntilArrived(CrewMover& m, const VehicleDef& v, const GaitDef& g, const VehiclePose& pose, const Terrain* ground,
                        std::map<std::string, bool>& hatches, double max_s, bool* clear = nullptr) {
    double t = 0.0;
    if (clear != nullptr) *clear = true;
    while (m.Moving() && t < max_s) {
        m.Update(0.05, pose, ground, nullptr, g, hatches);
        t += 0.05;
        const MoverView view = m.View(pose);
        if (clear != nullptr && view.place == BodyPlace::Ground && !view.leg_active) {
            if (v.footprint.Contains(view.feet_m.x, view.feet_m.y)) *clear = false;
        }
        if (clear != nullptr && view.place == BodyPlace::Hull && !v.KeptOutBy(view.feet_m.x, view.feet_m.y).empty()) *clear = false;
    }
    return t;
}

bool HasLeg(const CrewMover& m, LegKind k) {
    for (const MoveLeg& l : m.Legs()) {
        if (l.kind == k) return true;
    }
    return false;
}

std::unique_ptr<ScenarioRunner> RunnerFor(const std::string& scenario) {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenarioFile(kData + "/scenarios/" + scenario, kData, def, console, &error)) {
        std::printf("    %s\n", error.c_str());
        return nullptr;
    }
    return std::unique_ptr<ScenarioRunner>(new ScenarioRunner(std::move(def), std::move(console)));
}

}  // namespace

SC_TEST("crew mobility: the vehicles' hatches, hulls and places load; every seat has a way out") {
    for (const char* system : {"osa_ak", "pu12m"}) {
        const Loaded l = LoadVehicleOf(system);
        SC_EXPECT(l.ok);
        for (const ConsoleStation& st : l.console.stations) {
            SC_EXPECT(l.vehicle.Seat(st.id) != nullptr);
            Vec3 at;
            SC_EXPECT(l.vehicle.LineupPoint(st.id, at));
        }
        for (const VehicleExit& e : l.vehicle.exits) {
            // The opening is in the hull's roof (or a window below it); he climbs from the floor under it.
            SC_EXPECT(e.opening_m.z <= l.vehicle.hull_top_m + 1e-9 && e.under_m.z < e.opening_m.z);
        }
    }
    // The drill regulation's pace (110-120 steps a minute of 70-80 cm; running 165-180 of 85-90 cm).
    const Loaded l = LoadVehicleOf("pu12m");
    SC_EXPECT_NEAR(l.walking.gait.walk_mps, 115.0 * 0.75 / 60.0, 0.01);
    SC_EXPECT_NEAR(l.walking.gait.run_mps, 172.5 * 0.875 / 60.0, 0.03);
    // A job's control is its seat's: the AB-1 is the driver's, the PAB-2 the squad leader's.
    SC_EXPECT(l.vehicle.JobPlace("pwr.ab1_set") == "ab1" && l.vehicle.JobPlace("nav.pab2") == "pab2");
    SC_EXPECT(l.vehicle.JobPlace("mast.raise").empty());
}

SC_TEST("crew mobility: the loader rejects a hatch that opens off the hull and a job on an unknown control") {
    const Loaded l = LoadVehicleOf("pu12m");
    std::string text;
    SC_EXPECT(detail::ReadTextFile(kData + "/vehicles/pu12m.json", text));
    VehicleDef v;
    std::string e;
    std::string bad = text;
    bad.replace(bad.find("\"out_m\": [0.0, 1.8, 2.055]"), 26, "\"out_m\": [0.6, 1.4, 2.055]");  // on the mast's foot
    SC_EXPECT(!LoadVehicle(bad, l.console, v, &e) && e.find("out_m") != std::string::npos);
    bad = text;
    bad.replace(bad.find("\"control\": \"nav.pab2\""), 21, "\"control\": \"nav.pab9\"");
    SC_EXPECT(!LoadVehicle(bad, l.console, v, &e) && e.find("unknown control") != std::string::npos);
}

SC_TEST("crew mobility: the body frame on the map turns with the vehicle's heading (hand calculation)") {
    VehiclePose p;
    p.origin = Vec3{100.0, 200.0, 50.0};
    p.heading_rad = DegToRad(90.0);  // facing east: forward is east, right is south
    const Vec3 ahead = p.ToEnu(Vec3{0.0, 10.0, 0.0});
    SC_EXPECT_NEAR(ahead.x, 110.0, 1e-9);
    SC_EXPECT_NEAR(ahead.y, 200.0, 1e-9);
    const Vec3 right = p.ToEnu(Vec3{3.0, 0.0, 2.0});
    SC_EXPECT_NEAR(right.x, 100.0, 1e-9);
    SC_EXPECT_NEAR(right.y, 197.0, 1e-9);
    SC_EXPECT_NEAR(right.z, 52.0, 1e-9);
    const Vec3 back = p.ToBody(right);
    SC_EXPECT_NEAR(back.x, 3.0, 1e-9);
    SC_EXPECT_NEAR(back.y, 0.0, 1e-9);
    SC_EXPECT_NEAR(p.ToMapHeading(DegToRad(-90.0)), 0.0, 1e-12);  // looking left of an east-facing hull: north
}

SC_TEST("crew mobility: the pace over the ground follows Tobler's function and the woods (hand calculation)") {
    GaitDef g;
    SC_EXPECT_NEAR(ToblerFactor(0.0), 1.0, 1e-12);
    SC_EXPECT_NEAR(ToblerFactor(0.1), std::exp(-0.35), 1e-12);    // up a 10 % slope: 70 % of the flat pace
    SC_EXPECT_NEAR(ToblerFactor(-0.05), std::exp(0.175), 1e-12);  // fastest a little downhill
    FlatTerrain flat(100.0);
    SC_EXPECT_NEAR(GroundSpeed(g, &flat, nullptr, 0.0, 0.0, 1.0, 0.0, false), g.walk_mps, 1e-12);
    SC_EXPECT_NEAR(GroundSpeed(g, &flat, nullptr, 0.0, 0.0, 1.0, 0.0, true), g.run_mps, 1e-12);
    // A bank rising 1 m in 10 m: up it at exp(-0.35) of the pace; a wall (2 m in 1 m) stops him.
    GridTerrain bank(0.0, 0.0, 10.0, 2, 1, {0.0, 1.0});
    SC_EXPECT_NEAR(GroundSpeed(g, &bank, nullptr, 2.0, 0.0, 1.0, 0.0, false), g.walk_mps * std::exp(-0.35), 1e-9);
    GridTerrain wall(0.0, 0.0, 1.0, 2, 1, {0.0, 2.0});
    SC_EXPECT_NEAR(GroundSpeed(g, &wall, nullptr, 0.0, 0.0, 1.0, 0.0, false), 0.0, 1e-12);
}

SC_TEST("crew mobility: a voice carries by its effort, the distance and the noise (hand calculation)") {
    SC_EXPECT_NEAR(SpeechLevelAt(78.0, 10.0), 58.0, 1e-9);
    SC_EXPECT_NEAR(SpeechLevelAt(78.0, 0.5), 78.0, 1e-9);  // closer than a metre counts as a metre
    // A shout (78) over the countryside's 45 dB, needing 3 dB: about 32 m.
    SC_EXPECT_NEAR(EarshotRange(78.0, 45.0, 3.0), std::pow(10.0, 1.5), 1e-9);
    // Two equal noises add 3 dB.
    SC_EXPECT_NEAR(AddNoise(60.0, 60.0, 1.0), 60.0 + 10.0 * std::log10(2.0), 1e-9);
    // Beside the Osa's turbine (110 dB at 1 m) at 10 m the noise is 90 dB: a shout carries about half a metre.
    SC_EXPECT(EarshotRange(78.0, AddNoise(45.0, 110.0, 10.0), 3.0) < 1.0);
}

SC_TEST("crew mobility: the PU-12M's driver gets up, out through his hatch, down the side and round to the AB-1 behind") {
    const Loaded l = LoadVehicleOf("pu12m");
    CrewMover m(&l.vehicle, "driver");
    VehiclePose pose;
    pose.origin = Vec3{1000.0, 2000.0, 110.0};
    pose.heading_rad = DegToRad(30.0);
    FlatTerrain ground(110.0);
    std::map<std::string, bool> hatches;
    std::string why;
    SC_EXPECT(m.GoTo(MoveGoal::AtPlace("ab1"), &l.console, pose, &why));
    // Up, along the floor, the lid, out, across the roof, down, and over the ground.
    const LegKind order[] = {LegKind::StandUp, LegKind::CabinWalk, LegKind::OpenHatch, LegKind::ClimbOut, LegKind::HullWalk,
                             LegKind::ClimbDown, LegKind::GroundWalk};
    std::size_t k = 0;
    for (const MoveLeg& leg : m.Legs()) {
        if (k < 7 && leg.kind == order[k]) ++k;
    }
    SC_EXPECT(k == 7);
    SC_EXPECT(m.Inside());
    bool clear = false;
    const double took = WalkUntilArrived(m, l.vehicle, l.walking.gait, pose, &ground, hatches, 300.0, &clear);
    SC_EXPECT(clear);                         // round the hull, never under it
    SC_EXPECT(!m.Moving() && m.Place() == BodyPlace::Ground && m.At() == "ab1");
    SC_EXPECT(!m.Inside());
    SC_EXPECT(hatches["hatch_driver"]);       // he left it open behind him
    const MoverView v = m.View(pose);
    const Vec3 want = pose.ToEnu(l.vehicle.Place("ab1")->at_m);
    SC_EXPECT_NEAR(v.feet_enu.x, want.x, 0.01);
    SC_EXPECT_NEAR(v.feet_enu.y, want.y, 0.01);
    SC_EXPECT_NEAR(v.feet_enu.z, 110.0, 1e-9);
    // He faces the set (1 m further back).
    SC_EXPECT_NEAR(std::cos(v.yaw_m - kPi), 1.0, 1e-6);
    // The timed legs (1.5 + 2 + 4 + 3 s) and the walks: about 25 s.
    std::printf("    out to the AB-1 in %.1f s\n", took);
    SC_EXPECT(took > 15.0 && took < 40.0);
    // And back: in by his hatch, shutting it as the last man in, and down into his seat.
    SC_EXPECT(m.GoTo([] {
        MoveGoal g = MoveGoal::Seat();
        g.shut_after = true;
        return g;
    }(), &l.console, pose, &why));
    SC_EXPECT(HasLeg(m, LegKind::ClimbUp) && HasLeg(m, LegKind::ClimbIn) && HasLeg(m, LegKind::ShutHatch) && HasLeg(m, LegKind::SitDown));
    WalkUntilArrived(m, l.vehicle, l.walking.gait, pose, &ground, hatches, 300.0, &clear);
    SC_EXPECT(clear && m.Place() == BodyPlace::Seated && m.At().empty() && !hatches["hatch_driver"]);
}

SC_TEST("crew mobility: the PU-12M's bench crew squeeze forward to the front hatch; on the roof they go round the mast") {
    const Loaded l = LoadVehicleOf("pu12m");
    CrewMover m(&l.vehicle, "radio_operator");
    VehiclePose pose;
    FlatTerrain ground(0.0);
    std::map<std::string, bool> hatches;
    SC_EXPECT(m.GoTo(MoveGoal::OnHull(Vec3{1.0, 0.807, 0.0}), &l.console, pose));
    bool through = false;
    for (const MoveLeg& leg : m.Legs()) through = through || leg.exit == "hatch_squad_leader";
    SC_EXPECT(through);
    bool clear = false;
    WalkUntilArrived(m, l.vehicle, l.walking.gait, pose, &ground, hatches, 120.0, &clear);
    SC_EXPECT(clear && m.Place() == BodyPlace::Hull);  // never on the mast's foot on the way
}

SC_TEST("crew mobility: the Osa's hatches stay shut in combat work; the opening windscreen is for emergencies") {
    Loaded l = LoadVehicleOf("osa_ak");
    CrewMover m(&l.vehicle, "commander");
    VehiclePose pose;
    std::string why;
    SC_EXPECT(m.GoTo(MoveGoal::AtPlace("aiming_circle"), &l.console, pose, &why));  // equipment off: out he goes
    CrewMover held(&l.vehicle, "commander");
    l.console.panel.Set("fo.mode", 0);  // БР
    // The equipment powered: the subsystem's state as the console reports it.
    l.console.panel.Set("pwr.equipment", 1);
    for (int i = 0; i < 6000 && !l.console.equipment.IsReady("power"); ++i) {
        // Whatever powers the Osa's equipment in this console: the turbine or an external supply.
        l.console.equipment.SetInput("external_power", true);
        l.console.equipment.SetInput("vehicle_stopped", true);
        l.console.equipment.Update(0.1, l.console.panel);
    }
    if (l.console.equipment.IsReady("power")) {
        SC_EXPECT(!held.GoTo(MoveGoal::AtPlace("aiming_circle"), &l.console, pose, &why));
        SC_EXPECT(why.find("p207") != std::string::npos);
        MoveGoal out = MoveGoal::AtPlace("aiming_circle");
        out.emergency = true;
        SC_EXPECT(held.GoTo(out, &l.console, pose, &why));
        bool window = false;
        for (const MoveLeg& leg : held.Legs()) window = window || leg.exit == "windscreen";
        SC_EXPECT(window);
    } else {
        std::printf("    (the equipment didn't power up headless: the БР check is left to the runner test)\n");
    }
}

SC_TEST("crew mobility: in the PU-12M deploying, the driver walks out to set the AB-1 and the squad leader to the PAB-2") {
    auto r = RunnerFor("pu12m_deploy.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->HasVehicle());
    double driver_out_s = -1.0, driver_back_s = -1.0, sl_out_s = -1.0, sl_at_pab2_s = -1.0;
    double ab1_set_s = -1.0, pab2_set_s = -1.0;
    bool heard_outside = false;
    while (!r->Finished() && r->Time() < 1500.0) {
        r->Step(0.1);
        if (driver_out_s < 0.0 && r->Outside("driver")) driver_out_s = r->Time();
        if (ab1_set_s < 0.0 && r->Console().panel.Get("pwr.ab1_set") == 1) {
            ab1_set_s = r->Time();
            // He set it where the set stands.
            const MoverView v = r->Mover("driver")->View(r->Pose());
            const Vec3 want = r->Pose().ToEnu(r->Vehicle()->Place("ab1")->at_m);
            SC_EXPECT((Vec3{v.feet_enu.x, v.feet_enu.y, 0.0} - Vec3{want.x, want.y, 0.0}).Length() < 0.05);
            SC_EXPECT(r->Outside("driver"));
        }
        if (driver_out_s > 0.0 && driver_back_s < 0.0 && r->Mover("driver")->Place() == BodyPlace::Seated) driver_back_s = r->Time();
        if (sl_out_s < 0.0 && r->Outside("squad_leader")) sl_out_s = r->Time();
        if (sl_at_pab2_s < 0.0 && r->Mover("squad_leader")->At() == "pab2" && !r->Mover("squad_leader")->Moving()) {
            sl_at_pab2_s = r->Time();
        }
        if (pab2_set_s < 0.0 && r->Console().panel.Get("nav.pab2") == 1) pab2_set_s = r->Time();
        // A callout on the intercom while the driver is out: he doesn't hear it.
        if (r->Outside("driver") && r->Callouts() != nullptr) {
            for (const SpokenCallout& c : r->Callouts()->Log()) {
                if (c.channel == "intercom" && c.start_s > driver_out_s + 1.0 && r->CalloutHeardAt(c, "driver") && c.from != "driver") heard_outside = true;
            }
        }
    }
    std::printf("    driver out %.0f s, AB-1 set %.0f s, back in his seat %.0f s; squad leader out %.0f s, at the PAB-2 %.0f s, set %.0f s\n",
                driver_out_s, ab1_set_s, driver_back_s, sl_out_s, sl_at_pab2_s, pab2_set_s);
    SC_EXPECT(driver_out_s > 0.0 && ab1_set_s > driver_out_s && driver_back_s > ab1_set_s);
    SC_EXPECT(sl_out_s > 0.0 && sl_at_pab2_s > sl_out_s && pab2_set_s >= sl_at_pab2_s);
    // 35 m out at a walk: at least 20 s from the hatch to the aiming circle.
    SC_EXPECT(sl_at_pab2_s - sl_out_s > 20.0);
    SC_EXPECT(!heard_outside);
    // Everyone back inside at the end, the hatches shut, the post deployed.
    for (const MoverView& v : r->CrewBodies()) SC_EXPECT(v.place == BodyPlace::Seated);
    for (const auto& h : r->Hatches()) SC_EXPECT(!h.second);
    SC_EXPECT(r->Console().panel.Get("pwr.input") == 1 && r->Console().equipment.IsReady("oriented"));
}

SC_TEST("crew mobility: К машинам lines the crew up beside the vehicle, По местам puts them back in their seats") {
    auto r = RunnerFor("school_deputy_commander.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    r->Step(0.1);
    SC_EXPECT(r->OrderDismount());
    for (int i = 0; i < 1200; ++i) r->Step(0.1);
    const VehiclePose pose = r->Pose();
    int lined_up = 0;
    for (const MoverView& v : r->CrewBodies()) {
        Vec3 at;
        if (!r->Vehicle()->LineupPoint(v.station, at)) continue;
        const Vec3 want = pose.ToEnu(at);
        // Some may be out at a job still; the others stand in their places.
        if (v.place == BodyPlace::Ground && v.at == "lineup") {
            SC_EXPECT((Vec3{v.feet_enu.x - want.x, v.feet_enu.y - want.y, 0.0}).Length() < 0.05);
            ++lined_up;
        }
    }
    std::printf("    %d in the line-up after 2 min\n", lined_up);
    SC_EXPECT(lined_up == 5);
    // Lined up side by side they hear each other.
    SC_EXPECT(r->InEarshot("commander", "driver"));
    SC_EXPECT(r->OrderMount());
    for (int i = 0; i < 1500; ++i) r->Step(0.1);
    for (const MoverView& v : r->CrewBodies()) SC_EXPECT(v.place == BodyPlace::Seated);
    for (const auto& h : r->Hatches()) SC_EXPECT(!h.second);
}

SC_TEST("crew mobility: К машине and По местам are orders of the menu, greyed while they hold, each said once") {
    for (const char* scenario : {"school_deputy_commander.json", "school_pu12m_operator.json"}) {
        auto r = RunnerFor(scenario);
        SC_EXPECT(r != nullptr);
        if (!r) continue;
        r->Step(0.1);
        // Everyone in his seat: По местам holds already; К машине can be given.
        SC_EXPECT(!r->CheckOrder("order_mount").can);
        SC_EXPECT(r->CheckOrder("order_mount").reason_en == "done already");
        SC_EXPECT(r->CheckOrder("order_dismount").can);
        SC_EXPECT(r->GiveCrewOrder("order_dismount"));
        SC_EXPECT(!r->CheckOrder("order_dismount").can);  // they are on their way to the line-up
        for (int i = 0; i < 1200; ++i) r->Step(0.1);
        int on_ground = 0;
        for (const MoverView& v : r->CrewBodies()) on_ground += v.place == BodyPlace::Ground && v.at == "lineup" ? 1 : 0;
        std::printf("    %s: %d in the line-up\n", scenario, on_ground);
        SC_EXPECT(on_ground >= 4);
        SC_EXPECT(r->CheckOrder("order_mount").can);
        SC_EXPECT(r->GiveCrewOrder("order_mount"));
        for (int i = 0; i < 1500; ++i) r->Step(0.1);
        for (const MoverView& v : r->CrewBodies()) SC_EXPECT(v.place == BodyPlace::Seated);
        SC_EXPECT(!r->CheckOrder("order_mount").can);
        int said_dismount = 0, said_mount = 0;
        if (r->Callouts() != nullptr) {
            for (const SpokenCallout& c : r->Callouts()->Log()) {
                said_dismount += c.id == "order_dismount" ? 1 : 0;
                said_mount += c.id == "order_mount" ? 1 : 0;
            }
        }
        SC_EXPECT(said_dismount == 1 && said_mount == 1);
    }
}

SC_TEST("crew mobility: the Osa's crew chief carries the aiming circle out ahead to find Qc, and comes back") {
    auto r = RunnerFor("school_deputy_commander.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    r->Console().equipment.SetInput("vehicle_stopped", true);
    SC_EXPECT(r->HasVehicle());
    SC_EXPECT(r->GiveDrill("commander", "measure_qc"));
    double out_s = -1.0, set_s = -1.0, ready_s = -1.0, back_s = -1.0;
    double farthest = 0.0;
    while (r->Time() < 600.0 && back_s < 0.0) {
        r->Step(0.1);
        const MoverView v = r->Mover("commander")->View(r->Pose());
        farthest = std::fmax(farthest, v.feet_m.y);
        if (out_s < 0.0 && r->Outside("commander")) out_s = r->Time();
        if (set_s < 0.0 && r->Console().panel.Get("nav.aiming_circle") == 1) set_s = r->Time();
        if (ready_s < 0.0 && r->Console().equipment.IsReady("qc_measured")) ready_s = r->Time();
        if (ready_s > 0.0 && back_s < 0.0 && v.place == BodyPlace::Seated) back_s = r->Time();
    }
    std::printf("    out %.0f s, set up %.0f s, Qc %.0f s, back %.0f s, %.1f m ahead at the farthest\n", out_s, set_s, ready_s, back_s, farthest);
    SC_EXPECT(out_s > 0.0 && set_s > out_s && ready_s > set_s && back_s > ready_s);
    SC_EXPECT(farthest > 30.0 && farthest < 40.0);
}

SC_TEST("crew mobility: once the engine reports the player's body, a job outside needs him out there") {
    auto r = RunnerFor("pu12m_deploy.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("squad_leader", "Player"));
    SC_EXPECT(r->PlayerJobRefusal("squad_leader", "nav.pab2").empty());  // headless: no body reported
    r->SetPlayerBody("squad_leader", BodyPlace::Seated, Vec3{}, 0.0);
    SC_EXPECT(r->PlayerJobRefusal("squad_leader", "nav.pab2").find("outside") != std::string::npos);
    SC_EXPECT(r->PlayerJobRefusal("squad_leader", "gak.power").empty());
    // Walked out to it along the same ways, he can.
    std::string why;
    SC_EXPECT(r->PlayerGoTo("squad_leader", MoveGoal::AtPlace("pab2"), &why));
    for (int i = 0; i < 1200 && r->Mover("squad_leader")->Moving(); ++i) r->Step(0.1);
    SC_EXPECT(r->PlayerJobRefusal("squad_leader", "nav.pab2").empty());
    SC_EXPECT(r->PlayerJobRefusal("squad_leader", "gak.power").find("outside") != std::string::npos);
}

SC_TEST("crew mobility: a crewman away from his seat is drawn on his feet where his body is, stepping as he walks") {
    BodyData body;
    std::string e;
    SC_EXPECT(LoadBodyDataFile(kData + "/crew/body.json", body, &e));
    MoverView v;
    v.station = "driver";
    v.place = BodyPlace::Ground;
    v.inside = false;
    v.feet_m = Vec3{-1.85, 2.2, 0.0};
    v.yaw_m = DegToRad(180.0);  // walking aft
    v.moving = true;
    v.leg_active = true;
    v.leg = LegKind::GroundWalk;
    for (double phase : {0.0, 0.25, 0.5}) {
        const BodyPose p = WalkingPose(v, body.dims, phase);
        // Feet on the ground (a swinging foot lifted at most 10 cm), the head a stature up, facing aft.
        for (int i = 0; i < 2; ++i) SC_EXPECT(p.ankle[i].z >= 0.0 && p.ankle[i].z < body.dims.AnkleHeight() + 0.1);
        SC_EXPECT(p.head.z > 1.4 && p.head.z < body.dims.stature_m);
        SC_EXPECT(p.pelvis_forward.y < -0.99);
        // Parts keep their lengths.
        SC_EXPECT_NEAR((p.knee[0] - p.hip_joint[0]).Length(), body.dims.Thigh(), 1e-6);
        SC_EXPECT_NEAR((p.elbow[1] - p.shoulder[1]).Length(), body.dims.UpperArm(), 1e-6);
    }
    // A quarter turn on, the legs are apart: one foot ahead of the other.
    const BodyPose q = WalkingPose(v, body.dims, 0.25);
    SC_EXPECT(std::fabs(q.ankle[0].y - q.ankle[1].y) > 0.3);
    // Climbing, the hands are up over the shoulders.
    v.leg = LegKind::ClimbUp;
    const BodyPose c = WalkingPose(v, body.dims, 0.0);
    SC_EXPECT(c.wrist[0].z > c.shoulder[0].z && c.wrist[1].z > c.shoulder[1].z);
}

SC_TEST("crew mobility: a player inside can't set out the AB-1 from his seat; out at the set he can (the seat's move)") {
    auto r = RunnerFor("pu12m_deploy.json");
    SC_EXPECT(r != nullptr);
    if (!r) return;
    SC_EXPECT(r->TakeSeat("driver", "Player"));
    r->SetPlayerBody("driver", BodyPlace::Seated, Vec3{}, 0.0);
    std::string why;
    SC_EXPECT(r->SeatSet("driver", "pwr.ab1_set", 1, &why) == StepResult::Blocked && why.find("outside") != std::string::npos);
    SC_EXPECT(r->Console().panel.Get("pwr.ab1_set") == 0);
    // Walking there himself (the engine reporting his feet at the set's place) he can.
    r->SetPlayerBody("driver", BodyPlace::Ground, r->Vehicle()->Place("ab1")->at_m, 0.0);
    SC_EXPECT(r->SeatSet("driver", "pwr.ab1_set", 1, &why) != StepResult::Blocked);
    SC_EXPECT(r->Console().panel.Get("pwr.ab1_set") == 1);
    // And from out there his own console inside is out of reach.
    SC_EXPECT(r->SeatSet("driver", "pwr.input", 1, &why) == StepResult::Blocked);
}
