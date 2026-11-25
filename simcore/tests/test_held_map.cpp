// Track O, O2: the maps people hold (simcore/command/held_map.hpp, held_situation.hpp; docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md
// sections 3 and 5). A mark is made only from a fact that reached the holder, with who told him, by what and when; the
// regiment's task comes on his map with its order's means and hour; his decision is checked against the order (listed,
// never refused) and the combat order then uses it; each launcher commander gets his sector card; the sheet's contours
// are the ground's.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/command/held_map.hpp"
#include "simcore/command/held_situation.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;
namespace command = simcore::command;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

campaign::BatteryTask BridgesTask() {
    campaign::BatteryTask t;
    std::string error;
    campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_bridges.json", t, &error);
    return t;
}

bool Where(const std::string& id, Vec3& out) {
    if (id == "bty21_bridges") out = Vec3{9600.0, -7000.0, 0.0};
    else if (id == "bty21_bridges_reserve") out = Vec3{8700.0, -8000.0, 0.0};
    else if (id == "kesmark_bridges") out = Vec3{12000.0, -6000.0, 0.0};
    else if (id == "vrbice_crossroads") out = Vec3{4000.0, -7000.0, 0.0};
    else return false;
    return true;
}

bool Has(const std::vector<command::DecisionBreach>& b, const std::string& kind) {
    for (const command::DecisionBreach& x : b) {
        if (x.kind == kind) return true;
    }
    return false;
}

command::Fact WarningAt(const std::string& key, double h) {
    command::Fact f;
    f.key = key;
    f.kind = command::FactKind::AirWarning;
    f.h = h;
    f.origin = "sector_cp_duty";
    f.text = "'Воздух!' Raid from W2.";
    f.need_level = 5;
    f.precedence = 2;
    f.expires_h = h + 1.0;
    return f;
}

}  // namespace

SC_TEST("held map O2: a mark only from a fact the holder has, with its source, means and hour from his delivery") {
    command::ChainData d;
    std::string e;
    SC_EXPECT(command::LoadChainData(kData, "command/chain_pannonia.json", d, &e));
    command::InfoNet net;
    SC_EXPECT(net.Init(d, 1u, 10.0));
    const double h0 = 2.0;
    net.AddFact(WarningAt("held:known", h0));
    net.AddFact(WarningAt("held:later", h0 + 1.0));  // not yet happened when the map is drawn
    net.AdvanceTo(h0 + 0.5);
    const int bty = d.PositionIndex("bty21_commander");
    const command::Delivery* got = net.Known(bty, net.FactIndex("held:known"));
    SC_EXPECT(got != nullptr);
    if (got == nullptr) return;

    // A situation map with three SAM units: one on the fact he has, one on the fact he does not, one from the plan.
    command::SituationMap sm;
    command::MapSam a;
    a.id = "known_unit";
    a.x = 1000.0;
    a.y = 2000.0;
    a.basis = "reported";
    a.mark.key = "held:known";
    a.mark.h = h0;
    command::MapSam b = a;
    b.id = "unknown_unit";
    b.mark.key = "held:later";
    b.mark.h = h0 + 1.0;
    command::MapSam c = a;
    c.id = "plan_unit";
    c.mark = command::SituationMark();
    c.mark.source = "plan";
    sm.sams = {a, b, c};
    command::SituationFrame f;
    f.to_session = [](const Vec3& p) { return Vec3{p.x + 100.0, p.y - 50.0, 0.0}; };
    f.to_map_h = [](double h) { return h - 1.0; };
    f.clock = [](double h) { return command::FormatClock(h, 10.0); };
    command::HeldMap m;
    command::AddSituationMarks(m, sm, net, bty, f);
    const command::HeldMark* k = m.Find("sam_unit", "known_unit");
    SC_EXPECT(k != nullptr && m.Find("sam_unit", "unknown_unit") == nullptr && m.Find("sam_unit", "plan_unit") != nullptr);
    if (k == nullptr) return;
    const std::string from = d.positions[static_cast<std::size_t>(got->from)].Label();
    const std::string means = command::MeanKindName(d.means[static_cast<std::size_t>(got->mean)].kind);
    std::printf("    %s: told by %s, %s, as of %s (%.2f h)\n", k->label.c_str(), k->source.c_str(), k->means.c_str(), k->as_of.c_str(), k->as_of_h);
    SC_EXPECT(k->source == from && k->means == means && k->fact == "held:known");
    SC_EXPECT_NEAR(k->as_of_h, h0 - 1.0, 1e-9);
    SC_EXPECT(k->certainty == command::MarkCertainty::Reported && k->origin == command::MarkOrigin::Copied);
    SC_EXPECT(!k->points.empty() && std::fabs(k->points[0].x - 1100.0) < 1e-9 && std::fabs(k->points[0].y - 1950.0) < 1e-9);
    SC_EXPECT(m.Find("sam_unit", "plan_unit")->source == "the plan");
    // The other holder (a launcher commander) has not heard it: nothing on his map.
    command::HeldMap other;
    command::AddSituationMarks(other, sm, net, d.PositionIndex("veh21_1_commander"), f);
    SC_EXPECT(other.Find("sam_unit", "known_unit") == nullptr);
}

SC_TEST("held map O2: the decision's leeway and drawing values load as labelled game values") {
    command::HeldMapValues v;
    std::string e;
    SC_EXPECT(command::LoadHeldMapValues(kData, "gameplay/held_map.json", v, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT_NEAR(v.decision_leeway_m, 1000.0, 1e-9);
    SC_EXPECT(v.coverage_step_deg > 0.0 && v.sector_drawn_m > 0.0);
    SC_EXPECT(command::GridSquare(Vec3{9600.0, -7000.0, 0.0}, v) == "5943");
}

SC_TEST("held map O2: CheckDecision lists a position outside the leeway and a battery sector left uncovered, and refuses nothing") {
    const campaign::BatteryTask t = BridgesTask();
    command::HeldMapValues v;
    SC_EXPECT(t.sectors.size() == 4);
    // Within the leeway: nothing listed.
    command::BatteryDecision ok;
    ok.has_main = true;
    ok.main = Vec3{9600.0 + 600.0, -7000.0 - 300.0, 0.0};
    ok.reserve = {Vec3{8700.0, -8500.0, 0.0}};
    for (const campaign::TaskSector& s : t.sectors) ok.sectors.push_back({s.vehicle, s.from_deg, s.to_deg, s.duty});
    SC_EXPECT(command::CheckDecision(t, ok, Where, v).empty());
    // 1.5 km off: listed with how far.
    command::BatteryDecision off = ok;
    off.main = Vec3{9600.0 + 1500.0, -7000.0, 0.0};
    off.dummies = {Vec3{4000.0, -4000.0, 0.0}};  // 3 km from the order's dummy
    const std::vector<command::DecisionBreach> b = command::CheckDecision(t, off, Where, v);
    SC_EXPECT(Has(b, "main_off") && Has(b, "dummy_off") && !Has(b, "reserve_off"));
    for (const command::DecisionBreach& x : b) {
        std::printf("    %s\n", x.text.c_str());
        if (x.kind == "main_off") SC_EXPECT_NEAR(x.off_m, 1500.0, 1e-6);
    }
    // The sectors: 22 and 23 split 225-300 leaving 251-259 open, 24 forgotten, a sector for a vehicle the order does not name.
    command::BatteryDecision split;
    split.sectors = {{"21", 225.0, 250.0, true}, {"22", 225.0, 250.0, false}, {"23", 260.0, 300.0, false}, {"29", 0.0, 10.0, false}};
    const std::vector<command::DecisionBreach> s = command::CheckDecision(t, split, Where, v);
    SC_EXPECT(Has(s, "sector_missing") && Has(s, "sector_unknown") && Has(s, "sector_gap"));
    int gaps = 0;
    for (const command::DecisionBreach& x : s) {
        std::printf("    %s\n", x.text.c_str());
        if (x.kind != "sector_gap") continue;
        ++gaps;
        // 24 gone: 180-224 open as well as 251-259.
        SC_EXPECT((std::fabs(x.from_deg - 180.0) < 1e-9 && std::fabs(x.to_deg - 224.0) < 1e-9) ||
                  (std::fabs(x.from_deg - 251.0) < 1e-9 && std::fabs(x.to_deg - 259.0) < 1e-9));
    }
    SC_EXPECT(gaps == 2);
    // A battery sector across north: covered by two launchers either side of 000, then one short.
    campaign::BatteryTask north = t;
    north.sectors = {{"21", 340.0, 20.0, true}, {"22", 340.0, 20.0, false}};
    command::BatteryDecision across;
    across.sectors = {{"21", 340.0, 0.0, true}, {"22", 0.0, 20.0, false}};
    SC_EXPECT(command::CheckDecision(north, across, Where, v).empty());
    across.sectors[1].to_deg = 15.0;
    const std::vector<command::DecisionBreach> n = command::CheckDecision(north, across, Where, v);
    SC_EXPECT(n.size() == 1 && n[0].kind == "sector_gap" && std::fabs(n[0].from_deg - 16.0) < 1e-9 && std::fabs(n[0].to_deg - 20.0) < 1e-9);
}

SC_TEST("held map O2: contours from a height grid lie at their heights; every fifth is an index contour") {
    struct Grid {
        double west_m = 0.0, south_m = 0.0, spacing_m = 50.0;
        int cols = 41, rows = 41;
        std::vector<double> values;
        double At(int c, int r) const { return values[static_cast<std::size_t>(r * cols + c)]; }
    } g;
    // A cone 100 m high, 1 km in radius, on flat ground.
    for (int r = 0; r < g.rows; ++r) {
        for (int c = 0; c < g.cols; ++c) g.values.push_back(std::max(0.0, 100.0 - std::hypot(c * 50.0 - 1000.0, r * 50.0 - 1000.0) / 10.0));
    }
    const std::vector<command::ContourSegment> s = command::ContourSegments(g, 10.0, 0.0, 0.0, 2000.0, 2000.0);
    SC_EXPECT(!s.empty());
    double worst = 0.0;
    int at50 = 0, index50 = 0, index40 = 0;
    for (const command::ContourSegment& x : s) {
        if (x.level_m < 5.0) continue;  // the cone's foot: where it meets the flat, cut by the grid
        for (const Vec3& p : {x.a, x.b}) worst = std::max(worst, std::fabs(std::hypot(p.x - 1000.0, p.y - 1000.0) - (100.0 - x.level_m) * 10.0));
        if (std::fabs(x.level_m - 50.0) < 1e-9) {
            ++at50;
            index50 += x.index ? 1 : 0;
        }
        if (std::fabs(x.level_m - 40.0) < 1e-9) index40 += x.index ? 1 : 0;
    }
    std::printf("    %zu segments; the worst point %.1f m off its circle; %d at 50 m\n", s.size(), worst, at50);
    SC_EXPECT(worst < 15.0);
    // The 50 m circle (500 m radius) crosses about 2 pi r / spacing cells' edges.
    SC_EXPECT(at50 > 40 && at50 < 120 && index50 == at50 && index40 == 0);
    // A coarser sheet (every 4th node) still has the 50 m contour, with fewer segments.
    const std::vector<command::ContourSegment> coarse = command::ContourSegments(g, 10.0, 0.0, 0.0, 2000.0, 2000.0, 4);
    SC_EXPECT(!coarse.empty() && coarse.size() < s.size());
}

SC_TEST("held map O2: the regiment's task on the battery commander's map with its order's means and hour; nothing before it reached him") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_mission_cover.json", kData, def, console, &error));
    if (!def.has_task) return;
    ScenarioRunner r(std::move(def), std::move(console));
    const std::string post = r.BatteryCommanderPost();
    SC_EXPECT(r.HeldMapOf(post).marks.empty());
    while (r.TaskReceivedS() < 0.0 && r.Time() < 300.0) r.Step(0.05);
    SC_EXPECT(r.TaskReceivedS() > 0.0);
    const command::HeldMap m = r.HeldMapOf(post);
    const command::HeldMark* main = m.Find("main");
    SC_EXPECT(main != nullptr && m.Find("covered") != nullptr && m.Count("reserve") == 1 && m.Count("dummy") == 1);
    SC_EXPECT(m.Count("sector") == 4 && m.Find("corridor") != nullptr && m.Find("duty_zone") != nullptr);
    if (main == nullptr) return;
    std::printf("    %s: %s, %s, as of %s\n", main->label.c_str(), main->source.c_str(), main->means.c_str(), main->as_of.c_str());
    SC_EXPECT(main->source == "the regiment" && main->means == "radio_voice" && main->origin == command::MarkOrigin::Copied);
    SC_EXPECT_NEAR(main->as_of_h, r.TaskReceivedS() / 3600.0, 1e-9);
    const command::HeldMark* attack = m.Find("attack");
    SC_EXPECT(attack != nullptr && attack->side == command::MarkSide::Enemy && attack->has_arc && attack->certainty == command::MarkCertainty::Estimated);
    // A launcher commander holds nothing until his card comes with the combat order.
    SC_EXPECT(r.HeldMapOf("22:commander").marks.empty());
}

SC_TEST("held map O2: the combat order uses the decided place and sectors; each launcher commander gets his sector card") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_mission_cover.json", kData, def, console, &error));
    if (!def.has_task) return;
    ScenarioRunner r(std::move(def), std::move(console));
    while (r.TaskReceivedS() < 0.0 && r.Time() < 300.0) r.Step(0.05);
    Vec3 order_main;
    SC_EXPECT(r.TaskPlace(r.Task().main, order_main));
    command::BatteryDecision d;
    d.has_main = true;
    d.main = Vec3{order_main.x - 700.0, order_main.y + 400.0, 0.0};
    d.sectors = {{"21", 230.0, 260.0, true}, {"22", 250.0, 280.0, false}, {"23", 270.0, 300.0, false}, {"24", 180.0, 235.0, true}};
    const std::vector<command::DecisionBreach> b = r.SetBatteryDecision(d);
    SC_EXPECT(b.empty());  // within 1 km, and 180-300 covered
    for (const command::DecisionBreach& x : b) std::printf("    %s\n", x.text.c_str());
    SC_EXPECT(r.BatteryDecisionHeld() != nullptr);
    // The NPC battery commander gives his orders when his plan has them: run to the march.
    while (r.MarchPlanned() == nullptr && r.Time() < 3600.0) r.Step(0.05);
    const nav::MarchPlan* march = r.MarchPlanned();
    SC_EXPECT(march != nullptr && march->found);
    if (march != nullptr) {
        std::printf("    march to %s at (%.0f, %.0f), the order's main (%.0f, %.0f)\n", march->request.to_name.c_str(), march->request.to.east_m,
                    march->request.to.north_m, order_main.x, order_main.y);
        SC_EXPECT(std::fabs(march->request.to.east_m - d.main.x) < 1.0 && std::fabs(march->request.to.north_m - d.main.y) < 1.0);
    }
    // The decided sectors, not the task's.
    const BatteryFire* fire = r.GetBatteryFire();
    SC_EXPECT(fire != nullptr);
    if (fire != nullptr) {
        int set = 0;
        for (const BatteryFire::Shooter& s : fire->Shooters()) {
            const command::DecidedSector* want = d.SectorOf(s.callsign);
            if (want == nullptr) continue;
            ++set;
            SC_EXPECT(s.has_sector && std::fabs(s.sector_from_deg - want->from_deg) < 1e-9 && std::fabs(s.sector_to_deg - want->to_deg) < 1e-9);
        }
        SC_EXPECT(set == 4);
    }
    // Each launcher commander has his card in his map case, and his map from it.
    SC_EXPECT(r.SectorCards().size() == 4);
    for (const char* v : {"21", "22", "23", "24"}) {
        int cards = 0;
        for (const HeldPaper& p : r.MapCase(std::string(v) + ":commander")) {
            if (p.title.find("Sector card") == std::string::npos) continue;
            ++cards;
            if (std::string(v) == "22") std::printf("    [%s]\n%s", p.title.c_str(), p.text.c_str());
        }
        SC_EXPECT(cards == 1);
    }
    for (const HeldPaper& p : r.MapCase("22:commander")) {
        if (p.title.find("Sector card") != std::string::npos) SC_EXPECT(p.text.find("Your sector: 250 to 280") != std::string::npos && p.text.find("Neighbours") != std::string::npos);
    }
    const command::HeldMap m22 = r.HeldMapOf("22:commander");
    const command::HeldMark* own = m22.Find("sector", "22");
    SC_EXPECT(own != nullptr && m22.Count("sector") == 4 && m22.Find("main", "point") != nullptr);
    if (own != nullptr) SC_EXPECT(std::fabs(own->from_deg - 250.0) < 1e-9 && own->source == "the battery commander");
    // The battery commander's map shows his decision beside the order's points.
    const command::HeldMap mc = r.HeldMapOf(r.BatteryCommanderPost());
    SC_EXPECT(mc.FindDecision("main") != nullptr && mc.Find("main") != nullptr && mc.FindDecision("sector", "23") != nullptr);
    SC_EXPECT(mc.Count("sector") == 4);  // the decided ones replace the order's
}
