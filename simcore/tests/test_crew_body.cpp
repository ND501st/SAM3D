// The crew's bodies (roadmap 8.10, docs/crew/10_BODIES_AND_VOICES.md): the body model, the reach check of the cabin
// layout, the animator, standing up for a control out of reach, and the runner's activity feed.
#include <cmath>
#include <set>
#include <string>

#include "simcore/cabin.hpp"
#include "simcore/console_data.hpp"
#include "simcore/crew_body.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/procedures.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

struct OsaBody {
    ConsoleData console;
    CabinLayout cabin;
    BodyData body;
    CrewData crew;
    bool ok = false;
};

OsaBody LoadOsaBody() {
    OsaBody o;
    std::string e;
    o.ok = LoadConsoleFile(kData + "/procedures/osa_ak.json", o.console, &e) &&
           LoadCabinFile(kData + "/cabins/osa_ak.json", o.console, o.cabin, &e) &&
           LoadBodyDataFile(kData + "/crew/body.json", o.body, &e) &&
           LoadCrewDataFile(kData + "/crew/osa_ak_crew.json", o.console, o.crew, &e);
    if (!o.ok) std::printf("    load: %s\n", e.c_str());
    return o;
}

Vec3 Fingertip(const BodyPose& p, int i, const BodyDims& d) {
    return p.wrist[i] + p.hand_dir[i] * (p.hand_shape[i] == HandShape::Grip ? d.GripOffset() : d.PointOffset());
}

}  // namespace

SC_TEST("crew body: the data loads; the segments follow Winter's fractions") {
    BodyData b;
    std::string e;
    SC_EXPECT(LoadBodyDataFile(kData + "/crew/body.json", b, &e));
    const BodyDims& d = b.dims;
    // 1.72 m: upper arm 0.186 H = 0.320 m, forearm 0.146 H = 0.251 m, sitting eye (0.936 - 0.530) H + 0.09 = 0.788 m.
    SC_EXPECT_NEAR(d.UpperArm(), 0.320, 0.001);
    SC_EXPECT_NEAR(d.Forearm(), 0.251, 0.001);
    SC_EXPECT_NEAR(d.SittingEye(), 0.788, 0.001);
    SC_EXPECT_NEAR(d.seat_height_m, 0.43, 1e-9);  // GOST 12.2.032-78 Table 2
    SC_EXPECT_NEAR(RadToDeg(d.head_yaw_max_rad), 60.0, 1e-6);
    SC_EXPECT(b.Seat("driver") != nullptr && b.Seat("driver")->hands_on == "steering");
    // A broken file is refused with its path.
    SC_EXPECT(!LoadBodyData("{\"body_id\": \"x\", \"source\": \"sam3d\", \"confidence\": \"low\"}", b, &e));
    SC_EXPECT(e.find("body") != std::string::npos);
}

SC_TEST("crew body: two-bone IK matches a hand calculation") {
    // Root at the origin, target 0.5 m along x, both segments 0.3 m, bent towards +z: the elbow is at x = 0.25 and
    // z = sqrt(0.3^2 - 0.25^2) = 0.1658.
    IkResult r = TwoBoneIk(Vec3{0, 0, 0}, Vec3{0.5, 0, 0}, 0.3, 0.3, Vec3{0, 0, 1});
    SC_EXPECT(r.reached);
    SC_EXPECT_NEAR(r.mid.x, 0.25, 1e-6);
    SC_EXPECT_NEAR(r.mid.z, 0.16583, 1e-4);
    SC_EXPECT_NEAR((r.end - Vec3{0.5, 0, 0}).Length(), 0.0, 1e-6);
    SC_EXPECT_NEAR((r.mid - Vec3{0, 0, 0}).Length(), 0.3, 1e-6);
    SC_EXPECT_NEAR((r.end - r.mid).Length(), 0.3, 1e-6);
    // Out of reach: the arm straight along the line, the end 0.6 m out.
    r = TwoBoneIk(Vec3{0, 0, 0}, Vec3{1.0, 0, 0}, 0.3, 0.3, Vec3{0, 0, 1});
    SC_EXPECT(!r.reached);
    SC_EXPECT_NEAR(r.end.x, 0.6, 1e-5);
}

SC_TEST("crew body: the seats put the floor where the cabin has it") {
    OsaBody o = LoadOsaBody();
    SC_EXPECT(o.ok);
    // The cabin's floor is 0.62 m up (data/cabins/osa_ak.json notes); the sitting eye and GOST's seat height under each
    // station's eye point should land within a few centimetres of it.
    for (const CabinStation& st : o.cabin.stations) {
        const SeatFrame f = SeatFrameOf(st, o.body.dims);
        SC_EXPECT_NEAR(f.floor_z, 0.62, 0.03);
        SC_EXPECT_NEAR((f.eye - f.hip).z, o.body.dims.SittingEye() - o.body.dims.hip_above_seat_m, 1e-9);
    }
}

SC_TEST("crew body: every control is in reach of its seat, standing only for the crew data's stand_for") {
    OsaBody o = LoadOsaBody();
    SC_EXPECT(o.ok);
    int checked = 0;
    for (const CabinControl& c : o.cabin.controls) {
        const ControlDef* def = o.console.panel.Find(c.control_id);
        SC_EXPECT(def != nullptr);
        if (def == nullptr) continue;
        const CabinStation* st = o.cabin.FindStation(def->station);
        SC_EXPECT(st != nullptr);
        if (st == nullptr) continue;
        ControlTouch t;
        SC_EXPECT(TouchOf(o.cabin, c.control_id, t));
        const ReachPlan p = PlanReach(SeatFrameOf(*st, o.body.dims), o.body.dims, t);
        if (!p.reachable) std::printf("    out of reach: %s (%s)\n", c.control_id.c_str(), def->station.c_str());
        SC_EXPECT(p.reachable);
        const CrewSeatDef* seat = o.crew.FindSeat(def->station);
        bool listed = false;
        for (const std::string& s : seat->stand_for) listed = listed || s == c.control_id;
        if (listed == p.seated) {
            std::printf("    %s: the model says %s, the crew data %s\n", c.control_id.c_str(), p.seated ? "seated" : "standing",
                        listed ? "stand_for" : "seated");
        }
        SC_EXPECT(listed != p.seated);
        ++checked;
    }
    SC_EXPECT(checked >= 60);
}

SC_TEST("crew body: the finger is on the control when he moves it, and the hand comes back") {
    OsaBody o = LoadOsaBody();
    SC_EXPECT(o.ok);
    BodyAnimator anim(&o.cabin, &o.body, "senior_operator");
    SC_EXPECT(anim.Valid());
    const BodyDims& d = o.body.dims;
    const Vec3 rest[2] = {anim.Pose().wrist[0], anim.Pose().wrist[1]};
    ControlTouch t;
    SC_EXPECT(TouchOf(o.cabin, "soc.search", t));
    const ReachPlan& plan = anim.PlanFor("soc.search");
    SC_EXPECT(plan.reachable && plan.seated);
    const int hand = SideIndex(plan.side);
    SeatActivity a;
    a.station = "senior_operator";
    a.control = "soc.search";
    const double dt = 0.02;
    for (double in = 1.0; in > 0.0; in -= dt) {  // the reaction runs down to the action
        a.act_in_s = in;
        anim.Update(dt, a, nullptr);
    }
    a.act_in_s = 0.0;
    anim.Update(dt, a, nullptr);
    const double off = (Fingertip(anim.Pose(), hand, d) - t.point).Length();
    if (off > 0.03) std::printf("    finger %.3f m from the switch\n", off);
    SC_EXPECT(off < 0.03);
    SC_EXPECT(anim.Pose().hand_reached[hand]);
    // He moved it: the hand stays a moment, then goes back to his thigh.
    a.control.clear();
    a.act_in_s = -1.0;
    a.last_control = "soc.search";
    for (double s = 0.0; s < 2.0; s += dt) {
        a.since_last_s = s;
        anim.Update(dt, a, nullptr);
    }
    const Vec3 back = anim.Pose().wrist[hand];
    SC_EXPECT((back - rest[hand]).Length() < 0.05);
    SC_EXPECT(anim.Pose().hand_shape[hand] == HandShape::Rest);
}

SC_TEST("crew body: the deputy stands up at the gyro panel to orient the vehicle") {
    OsaBody o = LoadOsaBody();
    SC_EXPECT(o.ok);
    BodyAnimator anim(&o.cabin, &o.body, "deputy_commander");
    const ReachPlan& plan = anim.PlanFor("orient.qc");
    SC_EXPECT(plan.reachable && !plan.seated);
    SeatActivity a;
    a.station = "deputy_commander";
    a.control = "orient.qc";
    a.standing = true;
    a.stand_control = "orient.qc";
    const double dt = 0.02;
    for (double s = 0.0; s < 1.6; s += dt) {  // getting up (CrewMember's 1.5 s) while the reaction runs
        a.act_in_s = 1.6 - s;
        anim.Update(dt, a, nullptr);
    }
    a.act_in_s = 0.0;
    anim.Update(dt, a, nullptr);
    SC_EXPECT_NEAR(anim.Pose().standing, 1.0, 1e-6);
    SC_EXPECT((anim.Pose().hip - plan.stand_hip).Length() < 0.35);  // up at the panel (crouched a little, maybe)
    ControlTouch t;
    SC_EXPECT(TouchOf(o.cabin, "orient.qc", t));
    const int hand = SideIndex(plan.side);
    SC_EXPECT((Fingertip(anim.Pose(), hand, o.body.dims) - t.point).Length() < 0.04);
    // Standing, his feet are on the floor.
    SC_EXPECT_NEAR(anim.Pose().ankle[0].z, anim.Seat().floor_z + o.body.dims.AnkleHeight(), 0.03);
}

SC_TEST("crew body: a hand wheel's grip rides round with the wheel") {
    OsaBody o = LoadOsaBody();
    SC_EXPECT(o.ok);
    BodyAnimator anim(&o.cabin, &o.body, "operator");
    ConsoleData console = o.console;
    SeatActivity a;
    a.station = "operator";
    a.wheels = {"ssc.range_wheel"};
    const ReachPlan& plan = anim.PlanFor("ssc.range_wheel");
    SC_EXPECT(plan.reachable);
    const int hand = SideIndex(plan.side);
    for (int i = 0; i < 50; ++i) anim.Update(0.02, a, &console.panel);
    const Vec3 before = anim.Pose().wrist[hand];
    console.panel.SetValue("ssc.range_wheel", console.panel.GetValue("ssc.range_wheel") + 500.0);  // a tenth of a turn
    for (int i = 0; i < 5; ++i) anim.Update(0.02, a, &console.panel);
    const double moved = (anim.Pose().wrist[hand] - before).Length();
    SC_EXPECT(moved > 0.01);  // the rim (0.075 m radius) turned 36 degrees under his hand
    SC_EXPECT(moved < 0.1);
    SC_EXPECT(anim.Pose().hand_shape[hand] == HandShape::Grip);
}

SC_TEST("crew body: parts keep the segment lengths") {
    OsaBody o = LoadOsaBody();
    SC_EXPECT(o.ok);
    BodyAnimator anim(&o.cabin, &o.body, "driver");
    SeatActivity a;
    a.station = "driver";
    for (int i = 0; i < 60; ++i) anim.Update(0.02, a, nullptr);
    const std::vector<BodyPart> parts = anim.Parts();
    SC_EXPECT(parts.size() == 16u);
    for (const BodyPart& p : parts) {
        SC_EXPECT_NEAR(p.bone.Length(), 1.0, 1e-6);
        SC_EXPECT_NEAR(p.up.Length(), 1.0, 1e-6);
        SC_EXPECT_NEAR(Dot(p.bone, p.up), 0.0, 1e-6);
        if (p.name == "upper_arm_l" || p.name == "upper_arm_r") SC_EXPECT_NEAR(p.length_m, o.body.dims.UpperArm(), 1e-6);
        if (p.name == "forearm_r") SC_EXPECT_NEAR(p.length_m, o.body.dims.Forearm(), 1e-6);
        if (p.name == "thigh_l") SC_EXPECT_NEAR(p.length_m, o.body.dims.Thigh(), 1e-6);
    }
    // The driver holds the steering wheel when he has nothing else to do.
    SC_EXPECT(anim.Pose().hand_shape[0] == HandShape::Grip && anim.Pose().hand_shape[1] == HandShape::Grip);
}

SC_TEST("crew body: a crewman gets up for a control out of seated reach and sits back down") {
    ControlPanel panel;
    panel.AddControl({"a", "A", 2, 0});
    panel.AddControl({"far", "FAR", 2, 0});
    panel.AddControl({"c", "C", 2, 0});
    CrewMember m("Sgt. Test", "deputy", 1.0);
    m.SetStanding({"far"}, 1.5, 1.0);
    m.Order(Procedure{"p", "test", {{"a", 1, 0.0, "A on"}, {"far", 1, 0.0, "FAR on"}, {"c", 1, 0.0, "C on"}}});
    double t = 0.0, t_a = -1.0, t_far = -1.0, t_c = -1.0;
    bool stood = false;
    while (t < 10.0) {
        t += 0.05;
        m.Update(0.05, panel);
        if (t_a < 0.0 && panel.Get("a") == 1) t_a = t;
        if (t_far < 0.0 && panel.Get("far") == 1) t_far = t;
        if (t_c < 0.0 && panel.Get("c") == 1) t_c = t;
        stood = stood || m.Standing();
        if (t_a > 0.0 && t_far < 0.0) {  // on his way to it
            const CrewMember::Intent in = m.CurrentIntent();
            SC_EXPECT(in.control == "far" && in.standing && in.in_s >= 0.0);
        }
    }
    SC_EXPECT(stood);
    SC_EXPECT_NEAR(t_a, 1.0, 0.06);
    SC_EXPECT_NEAR(t_far - t_a, 1.0 + 1.5, 0.06);  // reaction and getting up
    SC_EXPECT_NEAR(t_c - t_far, 1.0 + 1.0, 0.06);  // reaction and sitting back down
    SC_EXPECT(!m.Standing());
    SC_EXPECT(m.CurrentIntent().last_control == "c");
}

SC_TEST("crew body: the runner tells the bodies what each seat is doing") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_crew_practice.json", kData, def, console, &e));
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.GiveCrewOrder("order_orient"));  // the deputy orients the vehicle at the gyro panel: he stands for it
    std::set<std::string> reached, spoke, turned;
    bool deputy_stood = false;
    for (int i = 0; i < 6000 && !r.Finished(); ++i) {  // 600 s: К бою from cold, the orientation, the target
        r.Step(0.1);
        for (const SeatActivity& a : r.CrewActivity()) {
            if (!a.control.empty() && a.act_in_s >= 0.0) reached.insert(a.station);
            if (a.speaking) spoke.insert(a.station);
            if (!a.wheels.empty()) turned.insert(a.station);
            if (a.station == "deputy_commander" && a.standing) {
                deputy_stood = true;
                SC_EXPECT(a.stand_control.rfind("orient.", 0) == 0);
            }
        }
    }
    SC_EXPECT(r.CrewActivity().size() == 5u);
    SC_EXPECT(reached.size() >= 4u);  // every working seat had something to reach for (the commander gives orders)
    SC_EXPECT(spoke.count("commander") == 1u && spoke.count("senior_operator") == 1u);
    SC_EXPECT(turned.count("senior_operator") == 1u);  // the azimuth wheel onto the target
    SC_EXPECT(deputy_stood);
}

// ---- The PU-12M (docs/PU12_DEVELOPMENT.md P6): the same body model in the command post's cabin.

namespace {

struct Pu12mBody {
    ConsoleData console;
    CabinLayout cabin;
    BodyData body;
    CrewData crew;
    bool ok = false;
};

Pu12mBody LoadPu12mBody() {
    Pu12mBody o;
    std::string e;
    o.ok = LoadConsoleFile(kData + "/procedures/pu12m.json", o.console, &e) &&
           LoadCabinFile(kData + "/cabins/pu12m.json", o.console, o.cabin, &e) &&
           LoadBodyDataFile(kData + "/crew/body.json", o.body, &e) &&
           LoadCrewDataFile(kData + "/crew/pu12m_crew.json", o.console, o.crew, &e);
    if (!o.ok) std::printf("    load: %s\n", e.c_str());
    return o;
}

}  // namespace

SC_TEST("crew body: the PU-12M's seats put the floor where its cabin has it") {
    Pu12mBody o = LoadPu12mBody();
    SC_EXPECT(o.ok);
    for (const CabinStation& st : o.cabin.stations) {
        SC_EXPECT_NEAR(SeatFrameOf(st, o.body.dims).floor_z, 0.62, 0.03);
    }
}

SC_TEST("crew body: the PU-12M's plotter rests his eyes on his map, the commander on the IT-45 (look alternatives)") {
    Pu12mBody o = LoadPu12mBody();
    SC_EXPECT(o.ok);
    for (const auto& seat : {std::make_pair("operator", "map"), std::make_pair("battery_commander", "it45")}) {
        BodyAnimator anim(&o.cabin, &o.body, seat.first);
        SC_EXPECT(anim.Valid());
        SeatActivity idle;
        for (int i = 0; i < 40; ++i) anim.Update(0.05, idle, &o.console.panel);
        const CabinDisplay* disp = nullptr;
        for (const CabinDisplay& d : o.cabin.displays) {
            if (d.id == seat.second) disp = &d;
        }
        SC_EXPECT(disp != nullptr);
        if (disp == nullptr) continue;
        const Vec3 target = o.cabin.FindPanel(disp->panel_id)->PointAt(disp->u_m, disp->v_m);
        const BodyPose& p = anim.Pose();
        const Vec3 to = (target - p.head).Normalized();
        SC_EXPECT(Dot(p.head_forward, to) > 0.97);   // within about 14 degrees
    }
}

SC_TEST("crew body: in the PU-12M, every control is in reach of its seat, standing only for the crew data's stand_for") {
    Pu12mBody o = LoadPu12mBody();
    SC_EXPECT(o.ok);
    int seated = 0, standing = 0, away = 0;
    for (const CabinControl& c : o.cabin.controls) {
        const ControlDef* def = o.console.panel.Find(c.control_id);
        const CabinStation* st = def != nullptr ? o.cabin.FindStation(def->station) : nullptr;
        SC_EXPECT(st != nullptr);
        if (st == nullptr) continue;
        ControlTouch t;
        SC_EXPECT(TouchOf(o.cabin, c.control_id, t));
        const ReachPlan p = PlanReach(SeatFrameOf(*st, o.body.dims), o.body.dims, t);
        if (!p.reachable) std::printf("    out of reach: %s (%s)\n", c.control_id.c_str(), def->station.c_str());
        SC_EXPECT(p.reachable);
        // He stands for those the crew data says (the DTP low at the squad leader's side, the RTK-1 over the tube, the
        // mast's winch at the front of the compartment: he walks to it), and only those.
        const CrewSeatDef* seat = o.crew.FindSeat(def->station);
        bool listed = false;
        for (const std::string& s : seat->stand_for) listed = listed || s == c.control_id;
        if (listed == p.seated) {
            std::printf("    %s: the model says %s, the crew data %s\n", c.control_id.c_str(), p.seated ? "seated" : "standing",
                        listed ? "stand_for" : "seated");
        }
        SC_EXPECT(listed != p.seated);
        if (!p.reachable) ++away; else if (p.seated) ++seated; else ++standing;
    }
    SC_EXPECT(seated + standing == static_cast<int>(o.console.panel.Controls().size()) && away == 0);
}
