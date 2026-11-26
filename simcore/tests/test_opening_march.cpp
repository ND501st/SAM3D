// The opening march with the division and the wars' districts (track O, O6): data/maps/kesmark_*.json extending the test
// map, the district's tracks joined to the country's roads, the regiment's command post moving with the division, and
// the Pact's, NATO's and the neutral's campaigns started from the menu's data.
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>

#include "simcore/campaign/campaign_save.hpp"
#include "simcore/campaign/mission_session.hpp"
#include "simcore/nav/session.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

bool HasDetail(const MapDef& d, const std::string& id) {
    for (const MapDetailDef& a : d.detail) {
        if (a.id == id) return true;
    }
    return false;
}

}  // namespace

SC_TEST("opening march (O6): a district's map extends the test map: the base's areas, roads and towns, and its own area and tracks") {
    std::string e;
    const std::shared_ptr<const MapData> base = LoadMap(kData, "maps/kesmark_range.json", &e);
    SC_EXPECT(base != nullptr);
    if (!base) return;
    const struct {
        const char* file;
        const char* area;
        const char* track;
    } wars[] = {{"maps/kesmark_rava_dam.json", "rava_dam", "t_dam_west"},
                {"maps/kesmark_dombas.json", "dombas", "t_dombas_hub"},
                {"maps/kesmark_south.json", "south", "t_south_border"}};
    for (const auto& w : wars) {
        const std::shared_ptr<const MapData> m = LoadMap(kData, w.file, &e);
        SC_EXPECT(m != nullptr);
        if (!m) {
            std::printf("    %s: %s\n", w.file, e.c_str());
            continue;
        }
        const MapDef& d = m->Def();
        SC_EXPECT(HasDetail(d, w.area));
        for (const MapDetailDef& a : base->Def().detail) SC_EXPECT(HasDetail(d, a.id));
        SC_EXPECT(d.roads.size() > base->Def().roads.size());
        bool track = false;
        for (const MapLineDef& l : d.roads) track = track || l.id == w.track;
        SC_EXPECT(track);
        SC_EXPECT(d.settlements.size() == base->Def().settlements.size());
        SC_EXPECT(d.far_ground_file == base->Def().far_ground_file);
    }
}

SC_TEST("opening march (O6): a district's track that ends on a country road between its bends joins it; the country's own graph is not split") {
    std::string e;
    const std::shared_ptr<const MapData> m = LoadMap(kData, "maps/kesmark_rava_dam.json", &e);
    SC_EXPECT(m != nullptr);
    if (!m) return;
    nav::SessionNav s;
    SC_EXPECT(s.Init(kData, "maps/pannonia.json", &m->Def(), "command/chain_pannonia.json", &e));
    // t_dam_west leaves the Hradec-Dravec road half a kilometre west of the Dravec bridge, between the road's bends.
    const nav::Anchor* from = s.GetAnchors().Find("kesmark");
    const nav::Anchor* to = s.GetAnchors().Find("bty21_dam");
    SC_EXPECT(from != nullptr && to != nullptr);
    if (from == nullptr || to == nullptr) return;
    BatteryTactics t;
    std::string why;
    const double h = s.MarchHours(from->at, to->at, t, &why);
    SC_EXPECT(h > 2.0 && h < 12.0);
    if (h < 0.0) std::printf("    %s\n", why.c_str());
    // The country's graph alone has none of the district's nodes.
    CountryDef country;
    std::string text;
    SC_EXPECT(simcore::detail::ReadTextFile(kData + "/maps/pannonia.json", text) && LoadCountryDefJson(text, country, &e));
    const nav::RoadGraph alone = nav::RoadGraph::ForCountry(country);
    std::size_t country_edges = 0;
    for (const nav::NavEdge& x : s.Graph().Edges()) {
        bool district = false;
        for (const MapLineDef& l : m->Def().roads) district = district || (x.line_id == l.id && x.line_id.rfind("t_", 0) == 0);
        if (!district) ++country_edges;
    }
    SC_EXPECT(country_edges > alone.Edges().size());  // the joins split two country edges in the session's graph
    SC_EXPECT(country_edges <= alone.Edges().size() + 8);
}

SC_TEST("opening march (O6): the regiment's command post moved: its place and its radars nearby go with it") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_mission_pact.json", kData, def, console, &e));
    if (!def.has_task) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    SC_EXPECT(def.task.regiment_cp == "rgt_cp_dam");
    const Vec3 was = def.post_regiment.position;
    ScenarioRunner r(std::move(def), std::move(console));
    const nav::Anchor* cp = r.Nav() != nullptr ? r.Nav()->GetAnchors().Find("rgt_cp_dam") : nullptr;
    SC_EXPECT(cp != nullptr);
    if (cp == nullptr) return;
    std::vector<Vec3> near_before;
    for (const RegimentRadar& rr : r.Def().post_regiment.radars) {
        if (std::hypot(rr.position.x - was.x, rr.position.y - was.y) < 3000.0) near_before.push_back(rr.position);
    }
    r.MoveRegiment(cp->at.east_m, cp->at.north_m);
    const Vec3 now = r.Def().post_regiment.position;
    SC_EXPECT(std::hypot(now.x - cp->at.east_m, now.y - cp->at.north_m) < 1.0);
    std::size_t k = 0;
    for (const RegimentRadar& rr : r.Def().post_regiment.radars) {
        if (k >= near_before.size()) break;
        if (std::hypot(rr.position.x - cp->at.east_m, rr.position.y - cp->at.north_m) < 3000.0) ++k;
    }
    SC_EXPECT(k == near_before.size());
}

SC_TEST("opening march (O6): the regiment's command post kept in the save: a task's and the battery's regiment_cp read back") {
    campaign::BatteryTask t;
    std::string e;
    SC_EXPECT(campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_dombas.json", t, &e));
    SC_EXPECT(t.regiment_cp == "rgt_cp_dombas");
    campaign::CampaignsData cd;
    SC_EXPECT(campaign::LoadCampaignsData(kData, "campaign/campaigns.json", cd, &e));
    campaign::MissionSessionDef md;
    campaign::CampaignSave s;
    SC_EXPECT(campaign::NewCampaign(cd, "nato", kData, "battery_commander", md, s, &e));
    s.version = 1;
    s.air_war = "{\"kind\": \"test\"}";  // the war's own save is not what this reads back
    s.battery.present = true;
    s.battery.regiment_cp = "rgt_cp_dombas";
    s.battery.has_task = true;
    s.battery.task = t;
    campaign::CampaignSave back;
    const bool read = campaign::LoadCampaignSaveJson(campaign::CampaignSaveJson(s), back, &e);
    SC_EXPECT(read);
    if (!read) std::printf("    %s\n", e.c_str());
    SC_EXPECT(back.battery.regiment_cp == "rgt_cp_dombas" && back.battery.task.regiment_cp == "rgt_cp_dombas");
}

SC_TEST("opening march (O6): the Pact's, NATO's and the neutral's campaigns start from the menu: the first task a march out of Kesmark to the war's district, in time") {
    campaign::CampaignsData cd;
    std::string e;
    SC_EXPECT(campaign::LoadCampaignsData(kData, "campaign/campaigns.json", cd, &e));
    for (const char* war : {"warsaw_pact", "nato", "neutral"}) {
        campaign::MissionSessionDef md;
        campaign::CampaignSave header;
        SC_EXPECT(campaign::NewCampaign(cd, war, kData, "battery_commander", md, header, &e));
        campaign::MissionSession s;
        SC_EXPECT(s.Start(md, kData, &e));
        if (s.Campaign() == nullptr) {
            std::printf("    %s: %s\n", war, e.c_str());
            continue;
        }
        const ScenarioRunner& r = s.Runner();
        const nav::SessionNav* nav = r.Nav();
        SC_EXPECT(nav != nullptr && r.HasTask());
        if (nav == nullptr || !r.HasTask()) continue;
        // The battery in the Kesmark barracks' park, its task's main position in the district far off.
        const nav::Anchor* park = nav->GetAnchors().Find("kesmark_barracks");
        const nav::Anchor* main = nav->GetAnchors().Find(r.Task().main);
        SC_EXPECT(park != nullptr && main != nullptr);
        if (park == nullptr || main == nullptr) continue;
        const Vec3 at = r.UnitPosition();
        SC_EXPECT(std::hypot(at.x - park->at.east_m, at.y - park->at.north_m) < 1000.0);
        SC_EXPECT(std::hypot(at.x - main->at.east_m, at.y - main->at.north_m) > 40000.0);
        // The task's paper at the briefing: his plan a march to the main position, ready by the order's time.
        for (int i = 0; i < 6000 && r.TaskPlan().ready_by_s <= 0.0; ++i) s.Step(0.1);
        const campaign::BatteryPlan& p = r.TaskPlan();
        SC_EXPECT(p.planned && p.march_at_s >= 0.0 && p.to_place == r.Task().main);
        SC_EXPECT(p.InTime());
        std::printf("    %s: «К маршу!» %s, there %s, ready %s, by %s\n", war, r.TaskClock(p.march_at_s).c_str(), r.TaskClock(p.arrive_s).c_str(),
                    r.TaskClock(p.ready_s).c_str(), r.TaskClock(p.ready_by_s).c_str());
        // The regiment's command post sets out for the district once the battery commander has left its briefing.
        for (int i = 0; i < 12000 && s.RegimentGoing().empty() && s.RegimentCp() != r.Task().regiment_cp; ++i) s.Step(0.1);
        SC_EXPECT(s.RegimentGoing() == r.Task().regiment_cp);
    }
}

SC_TEST("opening march (O6): a long march's halt is not its end: the battery commander reports ready to the regiment only at the main position") {
    campaign::CampaignsData cd;
    std::string e;
    SC_EXPECT(campaign::LoadCampaignsData(kData, "campaign/campaigns.json", cd, &e));
    campaign::MissionSessionDef md;
    campaign::CampaignSave header;
    SC_EXPECT(campaign::NewCampaign(cd, "warsaw_pact", kData, "battery_commander", md, header, &e));
    campaign::MissionSession s;
    SC_EXPECT(s.Start(md, kData, &e));
    if (s.Campaign() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    const ScenarioRunner& r = s.Runner();
    bool halted = false;
    while (!r.Finished() && r.Time() < 9.0 * 3600.0 && r.TaskReadyS() < 0.0) {
        s.Step(0.2);
        if (r.GetBattery() != nullptr && r.GetBattery()->CurrentMode() == Battery::Mode::Halted) halted = true;
    }
    SC_EXPECT(halted);  // the march to the dam has its halts (the post deployed at the long one)
    SC_EXPECT(r.TaskReadyS() > 0.0);
    const nav::Anchor* main = r.Nav() != nullptr ? r.Nav()->GetAnchors().Find(r.Task().main) : nullptr;
    SC_EXPECT(main != nullptr);
    if (main == nullptr) return;
    const Vec3 at = r.UnitPosition();
    SC_EXPECT(std::hypot(at.x - main->at.east_m, at.y - main->at.north_m) < 2000.0);
    SC_EXPECT(r.TaskReadyS() >= r.TaskPlan().arrive_s - 600.0);
}