// Roadmap 10.3: the trainer's instructor (simcore/instructor.hpp, ScenarioRunner::Instruct). He takes up to two targets
// and turns them, sends them up or down, changes their speed, makes them manoeuvre, switches their jammers, lets them go
// back to their routes, and enters new targets into the picture; a scenario can script him. Target numbers are
// placeholders.
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string ReplaceOnce(std::string s, const std::string& from, const std::string& to) {
    const std::size_t at = s.find(from);
    if (at == std::string::npos) {
        std::printf("    test text not found: %s\n", from.c_str());
        return s;
    }
    return s.replace(at, from.size(), to);
}

std::unique_ptr<ScenarioRunner> Runner(const std::string& text, std::string* error = nullptr) {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    if (!LoadScenario(text, kData, def, console, &e)) {
        if (error != nullptr) *error = e;
        else std::printf("    load: %s\n", e.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

void Run(ScenarioRunner& r, double seconds) {
    const double until = r.Time() + seconds;
    while (r.Time() < until - 1e-9 && !r.Finished()) r.Step(0.1);
}

// A copy of the target's line of the instructor's picture (the picture is a temporary); found false if it isn't there.
struct Seen {
    bool found = false;
    ScenarioRunner::InstructorTrack t;
    const ScenarioRunner::InstructorTrack* operator->() const { return &t; }
    bool operator!=(std::nullptr_t) const { return found; }
    bool operator==(std::nullptr_t) const { return !found; }
};
Seen Track(const std::vector<ScenarioRunner::InstructorTrack>& picture, const std::string& id) {
    Seen out;
    for (const auto& t : picture) {
        if (t.id == id) {
            out.found = true;
            out.t = t;
        }
    }
    return out;
}

InstructorOrder Order(InstructorOrder::Kind k, const std::string& target, double value = 0.0) {
    InstructorOrder o;
    o.kind = k;
    o.target = target;
    o.value = value;
    return o;
}

// The Kapustin Yar range day: one drone, deployed, straight in from the north at 100 m/s, 3 km up, from 60 s.
std::unique_ptr<ScenarioRunner> RangeDay() { return Runner(ReadFile(kData + "/scenarios/osa_range_kapustin_yar.json")); }

ScenarioTarget Drone(const std::string& id, double east_m, double north_m) {
    ScenarioTarget t;
    t.id = id;
    t.name = "Instructor's drone (placeholder)";
    t.type = "Air+FixedWing+Drone";
    t.target_class = "drone";
    t.start = {east_m, north_m, 2000.0};
    t.waypoints = {{east_m, -30000.0, 2000.0}};
    t.speed_mps = 150.0;
    t.rcs_m2 = 1.0;
    return t;
}

}  // namespace

SC_TEST("instructor: he takes a target and turns it; it turns at its own rate; let go, it goes back to its route") {
    auto r = RangeDay();
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    Run(*r, 70.0);
    std::string why;
    SC_EXPECT(r->Instruct(Order(InstructorOrder::Kind::Turn, "k1", 90.0), &why));
    SC_EXPECT(r->InstructorTargets().size() == 1);
    Run(*r, 5.0);  // 6 deg/s: half-way round after 5 s of the 15
    const Seen mid = Track(r->InstructorPicture(), "k1");
    Run(*r, 25.0);
    const Seen done = Track(r->InstructorPicture(), "k1");
    SC_EXPECT(mid != nullptr && done != nullptr);
    if (mid == nullptr || done == nullptr) return;
    std::printf("    heading %.0f deg after 5 s, %.0f deg after 30 s\n", mid->heading_deg, done->heading_deg);
    SC_EXPECT(mid->heading_deg > 100.0 && mid->heading_deg < 170.0);  // from 180, turning left onto 090
    SC_EXPECT(std::fabs(done->heading_deg - 90.0) < 1.0 && done->held);
    SC_EXPECT(std::fabs(done->speed_mps - 100.0) < 1e-6);
    SC_EXPECT(r->Debrief().find("takes it and turns it onto 090 degrees") != std::string::npos);
    // Let go: back toward its route's waypoint (5 km east, 30 km south): heading south again.
    SC_EXPECT(r->Instruct(Order(InstructorOrder::Kind::Release, "k1"), &why));
    SC_EXPECT(r->InstructorTargets().empty());
    Run(*r, 40.0);
    const Seen back = Track(r->InstructorPicture(), "k1");
    SC_EXPECT(back != nullptr && !back->held && back->heading_deg > 150.0 && back->heading_deg < 230.0);
    SC_EXPECT(!r->Instruct(Order(InstructorOrder::Kind::Release, "k1"), &why) && why.find("not held") != std::string::npos);
}

SC_TEST("instructor: sent down it descends smoothly to his height over the ground; slowed, it flies slower") {
    auto r = RangeDay();
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    Run(*r, 70.0);
    std::string why;
    SC_EXPECT(r->Instruct(Order(InstructorOrder::Kind::Height, "k1", 300.0), &why));
    SC_EXPECT(r->Instruct(Order(InstructorOrder::Kind::Speed, "k1", 60.0), &why) == false);  // below an aircraft's 80-700
    SC_EXPECT(why.find("80 to 700") != std::string::npos);
    SC_EXPECT(r->Instruct(Order(InstructorOrder::Kind::Speed, "k1", 180.0), &why));
    Run(*r, 20.0);
    const Seen going = Track(r->InstructorPicture(), "k1");
    Run(*r, 100.0);
    const Seen there = Track(r->InstructorPicture(), "k1");
    SC_EXPECT(going != nullptr && there != nullptr);
    if (going == nullptr || there == nullptr) return;
    std::printf("    %.0f m up after 20 s, %.0f m after 120 s; %.0f m/s\n", going->agl_m, there->agl_m, there->speed_mps);
    SC_EXPECT(going->agl_m > 2000.0 && going->agl_m < 2800.0);  // 30 m/s down: on the way
    SC_EXPECT(std::fabs(there->agl_m - 300.0) < 5.0);
    SC_EXPECT(std::fabs(there->speed_mps - 180.0) < 1e-6);
    SC_EXPECT(!r->Instruct(Order(InstructorOrder::Kind::Height, "k1", 2.0), &why));  // under 5 m
}

SC_TEST("instructor: he enters new targets; the trainer holds two at a time, a third waits until he lets one go") {
    auto r = RangeDay();
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    Run(*r, 70.0);
    std::string why;
    SC_EXPECT(r->InstructorEnter(Drone("i1", -6000.0, 30000.0), &why));
    SC_EXPECT(r->InstructorEnter(Drone("i2", 8000.0, 32000.0), &why));
    SC_EXPECT(!r->InstructorEnter(Drone("i2", 0.0, 30000.0), &why) && why.find("already") != std::string::npos);
    Run(*r, 1.0);
    SC_EXPECT(r->InstructorPicture().size() == 3);
    SC_EXPECT(r->Instruct(Order(InstructorOrder::Kind::Take, "k1"), &why));
    SC_EXPECT(r->Instruct(Order(InstructorOrder::Kind::Turn, "i1", 45.0), &why));
    SC_EXPECT(!r->Instruct(Order(InstructorOrder::Kind::Turn, "i2", 270.0), &why));
    SC_EXPECT(why.find("two targets at a time") != std::string::npos);
    SC_EXPECT(r->Instruct(Order(InstructorOrder::Kind::Release, "k1"), &why));
    SC_EXPECT(r->Instruct(Order(InstructorOrder::Kind::Turn, "i2", 270.0), &why));
    SC_EXPECT(r->InstructorTargets().size() == 2);
    // The entered targets are in the crew's picture like any other: the search radar finds them.
    Run(*r, 120.0);
    const std::string d = r->Debrief();
    SC_EXPECT(d.find("enters Instructor's drone (placeholder): 30.6 km out at 349 degrees, 2000 m up") != std::string::npos);
    SC_EXPECT(d.find("i1  contact") != std::string::npos);
    SC_EXPECT(!r->Instruct(Order(InstructorOrder::Kind::Turn, "nobody", 0.0), &why) && why.find("no target") != std::string::npos);
}

SC_TEST("instructor: an ordered manoeuvre is a jink, and can be ordered again once it is over") {
    auto r = RangeDay();
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    Run(*r, 70.0);
    std::string why;
    SC_EXPECT(r->Instruct(Order(InstructorOrder::Kind::Evade, "k1"), &why));
    SC_EXPECT(!r->Instruct(Order(InstructorOrder::Kind::Evade, "k1"), &why) && why.find("manoeuvring already") != std::string::npos);
    Run(*r, 20.0);  // a 15 s jink, then back on his heading (south, as it was)
    const Seen t = Track(r->InstructorPicture(), "k1");
    SC_EXPECT(t != nullptr && std::fabs(t->heading_deg - 180.0) < 2.0);
    SC_EXPECT(r->Instruct(Order(InstructorOrder::Kind::Evade, "k1"), &why));
    SC_EXPECT(r->Debrief().find("makes it manoeuvre: jinks (hard turns left and right)") != std::string::npos);
}

SC_TEST("instructor: his switch turns a target's noise pod off and on; one without a pod is refused") {
    auto r = Runner(ReadFile(kData + "/scenarios/osa_soj_strike.json"));
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    Run(*r, 310.0);  // the first striker is in the air, its pod on from the start
    Seen s1 = Track(r->InstructorPicture(), "s1");
    SC_EXPECT(s1 != nullptr && s1->has_jammer && s1->jamming);
    std::string why;
    SC_EXPECT(r->Instruct(Order(InstructorOrder::Kind::Jam, "s1", 0.0), &why));
    Run(*r, 1.0);
    s1 = Track(r->InstructorPicture(), "s1");
    SC_EXPECT(s1 != nullptr && !s1->jamming && !s1->held);  // the jammer is his to switch without taking the target
    SC_EXPECT(r->Instruct(Order(InstructorOrder::Kind::Jam, "s1", 1.0), &why));
    Run(*r, 1.0);
    s1 = Track(r->InstructorPicture(), "s1");
    SC_EXPECT(s1 != nullptr && s1->jamming);
    Run(*r, 30.0);  // the second striker (no pod) appears at 340 s
    SC_EXPECT(!r->Instruct(Order(InstructorOrder::Kind::Jam, "s2", 1.0), &why) && why.find("no jammer") != std::string::npos);
}

SC_TEST("instructor: the trainer's scripted drill plays: turned away, back low, the pod on, and the crew fights both") {
    const std::string text = ReadFile(kData + "/scenarios/osa_trainer.json");
    auto r = Runner(text);
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    r->RunToEnd(0.1, 900.0);
    const std::string d = r->Debrief();
    SC_EXPECT(d.find("takes it and turns it onto 270 degrees") != std::string::npos);
    SC_EXPECT(d.find("sends it to 100 m above the ground") != std::string::npos);
    SC_EXPECT(d.find("switches its jammer on") != std::string::npos);
    SC_EXPECT(d.find("radar on the jammer track") != std::string::npos);  // the jamming drill: fought on the strobe
    SC_EXPECT(d.find("guided at t1") != std::string::npos && d.find("guided at t2") != std::string::npos);
    // Bad scripts are refused with their paths.
    std::string error;
    SC_EXPECT(Runner(ReplaceOnce(text, "\"instructor\": \"turn\", \"target\": \"t1\", \"to\": 270", "\"instructor\": \"loop\", \"target\": \"t1\", \"to\": 270"), &error) == nullptr);
    SC_EXPECT(error.find("triggers[0].do[0].instructor") != std::string::npos && error.find("unknown order 'loop'") != std::string::npos);
    SC_EXPECT(Runner(ReplaceOnce(text, "\"instructor\": \"jam\", \"target\": \"t2\", \"to\": 1", "\"instructor\": \"jam\", \"target\": \"t9\", \"to\": 1"), &error) == nullptr);
    SC_EXPECT(error.find("unknown target 't9'") != std::string::npos);
    SC_EXPECT(Runner(ReplaceOnce(text, "\"instructor\": \"turn\", \"target\": \"t1\", \"to\": 270", "\"instructor\": \"turn\", \"target\": \"t1\""), &error) == nullptr);
    SC_EXPECT(error.find("to") != std::string::npos);
}
