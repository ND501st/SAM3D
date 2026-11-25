// The game's campaign saved at its seams and resumed (track O, O1): simcore/campaign/battery_state.hpp,
// simcore/campaign/campaign_save.hpp and MissionSession's resume path.
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/campaign/campaign_save.hpp"
#include "simcore/campaign/mission_session.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

const Battery::Vehicle* FindVehicle(const ScenarioRunner& r, const std::string& id) {
    const Battery* b = r.GetBattery();
    return b != nullptr ? b->Find(id) : nullptr;
}

}  // namespace

SC_TEST("campaign save (O1): a task written in its file's format reads back to the same order") {
    campaign::BatteryTask t;
    std::string e;
    SC_EXPECT(campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_bridges.json", t, &e));
    t.own_air.zones.push_back(command::OwnAirZone{});
    t.own_air.zones.back().id = "test_zone";
    t.own_air.zones.back().path = {Vec3{1000.0, 2000.0, 0.0}, Vec3{-500.0, 7000.0, 0.0}};
    t.own_air.present = true;
    std::ostringstream s;
    airwar::jsonb::Write(s, campaign::TaskJsonValue(t), 1, 0, 17);
    campaign::BatteryTask back;
    SC_EXPECT(campaign::LoadBatteryTaskJson(s.str(), back, &e));
    const auto clock = [](double t_s) { return std::to_string(static_cast<int>(t_s)); };
    const auto place = [](const std::string& id) { return id; };
    // The paper the battery commander holds is the same paper.
    SC_EXPECT(campaign::TaskDocument(t, 60.0, clock, place) == campaign::TaskDocument(back, 60.0, clock, place));
    SC_EXPECT(back.id == t.id && back.main == t.main && back.sectors.size() == t.sectors.size() && back.ready_in_s == t.ready_in_s);
    SC_EXPECT(back.own_air.zones.size() == t.own_air.zones.size() && back.own_air.zones.back().path.size() == 2);
    SC_EXPECT(back.own_air.Words() == t.own_air.Words());
}

SC_TEST("campaign save (O1): a held task is ready from the start; its sectors are set and nobody gives it again") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_mission_cover.json", kData, def, console, &e));
    if (!def.has_task) return;
    campaign::BatteryState st;
    st.present = true;
    st.local_h = 21.5;
    st.day_of_year = def.clock.day_of_year + 1;
    st.own = def.unit_position + Vec3{500.0, 0.0, 0.0};
    st.has_task = true;
    st.task = def.task;
    st.task_ready = true;
    for (const BatteryVehicleDef& v : def.battery_vehicles) {
        if (v.own) continue;
        campaign::SavedVehicle sv;
        sv.id = v.id;
        sv.position = v.start + Vec3{100.0, 200.0, 0.0};
        if (v.kind == BatteryVehicleKind::Launcher) sv.missiles = 3;
        st.vehicles.push_back(sv);
    }
    campaign::ApplyBatteryState(def, st);
    SC_EXPECT(def.task_held && def.has_task);
    SC_EXPECT_NEAR(def.clock.start_local_h, 21.5, 1e-9);
    SC_EXPECT_NEAR(def.clock_start_s, 21.5 * 3600.0, 1e-6);
    ScenarioRunner r(std::move(def), std::move(console));
    for (int i = 0; i < 300; ++i) r.Step(0.1);  // 30 s: past the first steps, short of the scenario's own task time
    SC_EXPECT(r.TaskReceivedS() >= 0.0 && r.TaskReceivedS() < 1.0);
    SC_EXPECT(r.TaskReadyS() >= 0.0 && r.TaskReadyS() < 1.0);
    SC_EXPECT(r.TaskSectorsSet() == static_cast<int>(r.Task().sectors.size()));
    // Nobody said the task on the regiment's net: its order book has no task in it.
    int tasks = 0;
    if (const command::BatteryOrderNet* reg = r.GetRegimentOrders()) {
        for (const command::BatteryOrderRecord& o : reg->Book().Orders()) tasks += o.kind == "task" ? 1 : 0;
    }
    SC_EXPECT(tasks == 0);
    // Each launcher where it was saved, with the missiles it had.
    for (const campaign::SavedVehicle& sv : st.vehicles) {
        const Battery::Vehicle* v = FindVehicle(r, sv.id);
        SC_EXPECT(v != nullptr);
        if (v == nullptr) continue;
        SC_EXPECT(std::hypot(v->position.x - sv.position.x, v->position.y - sv.position.y) < 1.0);
        if (sv.missiles < 0) continue;
        for (const BatteryFire::Shooter& sh : r.GetBatteryFire()->Shooters()) {
            if (sh.callsign == sv.id) SC_EXPECT(sh.missiles == 3);
        }
    }
}

SC_TEST("campaign save (O1): the rebels' campaign saved in the evening comes back to the same war, battery and hour") {
    campaign::CampaignsData cd;
    std::string e;
    SC_EXPECT(campaign::LoadCampaignsData(kData, "campaign/campaigns.json", cd, &e));
    campaign::MissionSessionDef md;
    campaign::CampaignSave header;
    SC_EXPECT(campaign::NewCampaign(cd, "rebels", kData, "battery_commander", md, header, &e));
    SC_EXPECT(!campaign::NewCampaign(cd, "nato", kData, "battery_commander", md, header, &e) && e.find("O6") != std::string::npos);
    SC_EXPECT(campaign::NewCampaign(cd, "rebels", kData, "battery_commander", md, header, &e));
    campaign::MissionSession a;
    SC_EXPECT(a.Start(md, kData, &e));
    if (a.Campaign() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    // The war's first evening: the session starts at the regiment's 19:00 briefing, and the first seam is taken there.
    campaign::MissionSession::Seam seam;
    for (int i = 0; i < 1200 && a.SeamsTaken() == 0; ++i) a.Step(0.1);
    SC_EXPECT(a.SeamsTaken() == 1 && a.TakeSeam(seam) && seam.kind == "briefing");
    SC_EXPECT(!a.TakeSeam(seam));  // once
    // On to 21:30: the battery at its new position by the bridges and ready there; saved then (as a seam is).
    while (a.Runner().Time() < 2.5 * 3600.0) a.Step(0.1);
    SC_EXPECT(a.Runner().TaskReadyS() >= 0.0);
    seam.air_war = airwar::SaveCampaignJson(*a.Campaign(), a.TheatreFile(), a.FactionFile());
    seam.battery = a.BatteryNow();
    const campaign::CampaignSave saved = campaign::SaveOfSeam(header, a, seam);
    SC_EXPECT(saved.label.find("Day 1, 21:") == 0);
    campaign::CampaignSave loaded;
    SC_EXPECT(campaign::LoadCampaignSaveJson(campaign::CampaignSaveJson(saved), loaded, &e));
    SC_EXPECT(loaded.battery.task_ready && loaded.battery.has_task && loaded.battery.task.id == a.Runner().Task().id);
    // Resumed: the war replayed to the hour it was saved at, its actions the same; the session's clock that hour.
    campaign::MissionSession b;
    SC_EXPECT(b.Start(campaign::ResumeCampaign(loaded, kData), kData, &e));
    if (b.Campaign() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    SC_EXPECT_NEAR(b.Campaign()->Now(), a.Campaign()->Now(), 1e-9);
    SC_EXPECT(b.Campaign()->Actions().size() == a.Campaign()->Actions().size());
    SC_EXPECT(b.Campaign()->Packages().size() == a.Campaign()->Packages().size());
    SC_EXPECT_NEAR(b.CampaignHourAtStart(), a.Campaign()->Now(), 1e-9);
    SC_EXPECT_NEAR(b.Runner().Def().clock.start_local_h, loaded.battery.local_h, 1e-9);
    // The battery where it stood, holding its task, ready; the campaign has it there.
    for (int i = 0; i < 20; ++i) b.Step(0.1);
    SC_EXPECT(b.Runner().TaskReadyS() >= 0.0 && b.Runner().TaskReadyS() < 1.0);
    for (const campaign::SavedVehicle& sv : loaded.battery.vehicles) {
        const Battery::Vehicle* v = FindVehicle(b.Runner(), sv.id);
        SC_EXPECT(v != nullptr && std::hypot(v->position.x - sv.position.x, v->position.y - sv.position.y) < 1.0);
    }
    SC_EXPECT(b.BatteryNow().at == a.BatteryNow().at);
    // And the war goes on from there, its next hour advanced in the session.
    while (b.Runner().Time() < 3600.0) b.Step(0.1);
    SC_EXPECT(b.Campaign()->Now() > a.Campaign()->Now() + 0.9);
}
