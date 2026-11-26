// The battery without the air picture (pvo_bu_ix_1984: search on the designation or independently, sudden targets
// engaged independently): the battery commander's order that the launchers search on their own in their sectors,
// given when a raid is announced and his board stays empty, and taken back when the picture returns
// (ScenarioRunner::OrderSearchOwn, UpdatePictureLost).
#include <cstdio>
#include <string>

#include "simcore/campaign/mission_session.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

// The bridges mission run until the battery reports ready at the bridges: some 20 km from the regiment's post, beyond
// the telecode's reach (15 km); the battery commander puts his post at the centre of the battle order to keep the
// air-picture net (aspd_u.json's 20 km: battery_plan.hpp), so these tests take that net away with a barrage jammer.
bool ReadyAtTheBridges(campaign::MissionSession& s) {
    campaign::MissionSessionDef md;
    md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    md.hours = 3.0;
    std::string e;
    if (!s.Start(md, kData, &e)) {
        std::printf("    %s\n", e.c_str());
        return false;
    }
    ScenarioRunner& r = s.Runner();
    while (!r.Finished() && r.TaskReadyS() < 0.0) s.Step(0.1);
    if (r.TaskReadyS() < 0.0) return false;
    JammingDef j;  // strong barrage on the air-picture net from now on: no frequency escapes it
    j.nets = {"air_picture"};
    j.from_s = r.Time();
    j.strong = true;
    j.spot = false;
    return r.AddNetJamming(j, "the enemy's jammer");
}

// A strike aircraft low across launcher 21's sector (its task's 225-270 deg), passing 3 km from it.
ScenarioTarget Sudden(const Vec3& at, double appear_s) {
    ScenarioTarget t;
    t.id = "sudden-1";
    t.name = "a strike aircraft, low";
    t.start = at + Vec3{-14000.0, -9000.0, 150.0};
    t.waypoints = {at + Vec3{-3000.0, -2000.0, 150.0}, at + Vec3{9000.0, 6000.0, 150.0}};
    t.speed_mps = 220.0;
    t.appear_at_s = appear_s;
    return t;
}

ScenarioTrigger Warning(double at_s) {
    ScenarioTrigger w;
    w.id = "warning";
    w.after_s = at_s;
    TriggerAction a;
    a.kind = TriggerAction::Kind::AirThreat;
    a.air_threat = true;
    w.actions.push_back(a);
    return w;
}

}  // namespace

SC_TEST("picture lost: a raid announced and nothing on the board, the commander puts the launchers on their own and one engages") {
    campaign::MissionSession s;
    SC_EXPECT(ReadyAtTheBridges(s));
    ScenarioRunner& r = s.Runner();
    const Battery::Vehicle* l21 = r.GetBattery() != nullptr ? r.GetBattery()->Find("21") : nullptr;
    SC_EXPECT(l21 != nullptr);
    if (l21 == nullptr || r.TaskReadyS() < 0.0) return;
    const double t0 = r.Time();
    std::string why;
    SC_EXPECT(r.AddTriggers({Warning(t0 + 1.0)}, &why));
    SC_EXPECT(r.AddTargets({Sudden(l21->position, t0 + 200.0)}, "a raid", &why));
    // Not before the warning has stood its time with the board empty.
    while (r.Time() < t0 + 200.0) s.Step(0.1);
    SC_EXPECT(r.Debrief().find("Батарее — поиск целей самостоятельно") == std::string::npos);
    while (r.Time() < t0 + 900.0) s.Step(0.1);
    const std::string d = r.Debrief();
    SC_EXPECT(d.find("Батарее — поиск целей самостоятельно") != std::string::npos);
    SC_EXPECT(d.find("searches and fires on its own in its sector, by the battery commander's order") != std::string::npos);
    SC_EXPECT(d.find("launches at sudden-1") != std::string::npos);
    SC_EXPECT(!r.CheckOrder("order_battery_search_own").can && r.CheckOrder("order_battery_by_designation").can);
}

SC_TEST("picture lost: the order goes by the battery net to the launchers that hear it, and back on the designations") {
    campaign::MissionSession s;
    SC_EXPECT(ReadyAtTheBridges(s));
    ScenarioRunner& r = s.Runner();
    if (r.TaskReadyS() < 0.0 || r.GetBatteryFire() == nullptr) return;
    SC_EXPECT(r.CheckOrder("order_battery_search_own").can && !r.CheckOrder("order_battery_by_designation").can);
    SC_EXPECT(r.GiveCrewOrder("order_battery_search_own"));
    SC_EXPECT(!r.GiveCrewOrder("order_battery_search_own"));  // once
    const double t0 = r.Time();
    while (r.Time() < t0 + 30.0) s.Step(0.1);
    int own = 0;
    for (const BatteryFire::Shooter& sh : r.GetBatteryFire()->Shooters()) own += sh.on_own ? 1 : 0;
    SC_EXPECT(own >= 3);  // launcher 23 is the one that hears the net worst in this mission
    SC_EXPECT(r.GiveCrewOrder("order_battery_by_designation"));
    while (r.Time() < t0 + 60.0) s.Step(0.1);
    own = 0;
    for (const BatteryFire::Shooter& sh : r.GetBatteryFire()->Shooters()) own += sh.on_own ? 1 : 0;
    SC_EXPECT(own == 0);
}
