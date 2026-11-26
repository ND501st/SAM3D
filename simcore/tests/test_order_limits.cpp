// Track O, O4: the order's limits (campaign/battery_task.hpp's OrderLimits; docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md
// sections 3, 6 and 10). The limits live in the order as its holder was given it; the NPCs keep them; a player may break
// them and nothing refuses him, but the breach is in the records the post wrote (the commander's log), and so in the
// regiment's check (campaign/report.hpp's CheckLimits). The covered unit is known only from its reports on its net or by
// sight; the battery follows it on the roads.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/battery_link.hpp"
#include "simcore/campaign/battery_task.hpp"
#include "simcore/campaign/report.hpp"
#include "simcore/command/held_map.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;
namespace command = simcore::command;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

bool HasLimit(const std::vector<campaign::LimitFinding>& f, const std::string& limit, const std::string& who = std::string()) {
    for (const campaign::LimitFinding& x : f) {
        if (x.limit == limit && (who.empty() || x.who == who)) return true;
    }
    return false;
}

int LogCount(const ScenarioRunner& r, const std::string& kind) {
    int n = 0;
    for (const campaign::LogEntry& e : r.CommanderLog()) n += e.kind == kind ? 1 : 0;
    return n;
}

campaign::LogEntry Entry(double t, const std::string& who, const std::string& kind, double off_m = -1.0, double limit_m = -1.0,
                         const std::string& mode = std::string()) {
    campaign::LogEntry e;
    e.time_s = t;
    e.callsign = who;
    e.kind = kind;
    e.text = "512 443";
    e.off_m = off_m;
    e.limit_m = limit_m;
    e.mode = mode;
    return e;
}

std::unique_ptr<ScenarioRunner> LoadCompany() {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenarioFile(kData + "/scenarios/pu12m_cover_company.json", kData, def, console, &error)) {
        std::printf("    %s\n", error.c_str());
        return nullptr;
    }
    return std::unique_ptr<ScenarioRunner>(new ScenarioRunner(std::move(def), std::move(console)));
}

Vec3 LaunchersMiddle(const ScenarioRunner& r) {
    Vec3 c{0.0, 0.0, 0.0};
    int n = 0;
    for (const Battery::Vehicle& v : r.GetBattery()->Vehicles()) {
        if (v.def.kind != BatteryVehicleKind::Launcher) continue;
        c.x += v.position.x;
        c.y += v.position.y;
        ++n;
    }
    return n > 0 ? Vec3{c.x / n, c.y / n, 0.0} : c;
}

Vec3 CompanyAt(const ScenarioRunner& r) {
    const nav::Mover* m = r.Nav()->GetMovers().Find("coy_2");
    const MapPoint p = nav::PointAlong(m->route, m->arc_m);
    return Vec3{p.east_m, p.north_m, 0.0};
}

}  // namespace

SC_TEST("order limits (O4): the order carries its limits; a fire mode that is not one, or a leash with no unit, is refused") {
    campaign::BatteryTask t;
    std::string e;
    SC_EXPECT(campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_company.json", t, &e));
    SC_EXPECT(t.limits.cover_unit == "coy_2" && t.limits.fire == "designation" && t.limits.Any());
    SC_EXPECT(t.limits.leash_m < 0.0 && t.limits.spot_m < 0.0);  // the held map's values
    const std::string doc = campaign::TaskDocument(t, 0.0, [](double) { return std::string("19:00"); }, [](const std::string& id) { return id; });
    std::printf("%s", doc.c_str());
    SC_EXPECT(doc.find("designation only") != std::string::npos && doc.find("Limits:") != std::string::npos);
    std::ifstream f(kData + "/campaign/tasks/bty21_cover_company.json");
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string text = ss.str();
    std::string bad = text;
    bad.replace(bad.find("\"fire\": \"designation\""), 22, "\"fire\": \"whenever\"");
    campaign::BatteryTask b;
    SC_EXPECT(!campaign::LoadBatteryTaskJson(bad, b, &e));
    std::string no_unit = text;
    no_unit.replace(no_unit.find("\"cover_unit\": \"coy_2\""), 21, "\"leash_m\": 2000");
    SC_EXPECT(!campaign::LoadBatteryTaskJson(no_unit, b, &e));
    // The held map's leash and the covered unit's reporting are labelled values.
    command::HeldMapValues v;
    SC_EXPECT(command::LoadHeldMapValues(kData, "gameplay/held_map.json", v, &e));
    SC_EXPECT(v.leash_m == 3000.0 && v.leash_behind_m == 1500.0 && v.covered_report_s > 0.0 && v.leash_grace_s > 0.0);
}

SC_TEST("order limits (O4): a grid reference names its 100 m square and reads back in the cycle nearest the hearer") {
    command::HeldMapValues v;
    const Vec3 p{1234.0, -5678.0, 0.0};
    const std::string g = command::GridRef(p, v);
    SC_EXPECT(g == "512 443");
    Vec3 back;
    SC_EXPECT(command::ReadGridRef(g, Vec3{0.0, 0.0, 0.0}, v, back));
    SC_EXPECT(std::fabs(back.x - 1250.0) < 1e-6 && std::fabs(back.y + 5650.0) < 1e-6);
    // 100 km on, the same figures: the hearer's place picks the cycle.
    const Vec3 next_cycle{p.x + 100000.0, p.y, 0.0};
    SC_EXPECT(command::GridRef(next_cycle, v) == g);
    SC_EXPECT(command::ReadGridRef(g, Vec3{95000.0, 0.0, 0.0}, v, back) && std::fabs(back.x - 101250.0) < 1e-6);
    SC_EXPECT(!command::ReadGridRef("51 2443", Vec3{}, v, back) && !command::ReadGridRef("5a2 443", Vec3{}, v, back));
}

SC_TEST("order limits (O4): the records' check finds each breach the log shows, and nothing it does not") {
    campaign::GivenLimits g;
    g.fire = "designation";
    g.leash_grace_s = 900.0;
    g.emission = {{0.0, "limited"}, {3000.0, "arm"}};
    std::vector<campaign::LogEntry> log;
    log.push_back(Entry(100.0, "21", "spot", 300.0, 1000.0));   // within its leeway
    log.push_back(Entry(110.0, "22", "spot", 2500.0, 1000.0));  // beyond
    log.push_back(Entry(200.0, "battery", "regime", -1.0, -1.0, "limited"));  // as the regiment's
    log.push_back(Entry(3100.0, "battery", "regime", -1.0, -1.0, "limited"));  // above the regiment's arm since 3000 s
    // The covered unit beyond the leash: 900 s and no move ordered is a breach; a follow in time is none.
    log.push_back(Entry(1000.0, "battery", "covered", 4000.0, 3000.0));
    log.push_back(Entry(1600.0, "battery", "covered", 4200.0, 3000.0));
    log.push_back(Entry(1950.0, "battery", "covered", 4300.0, 3000.0));
    log.push_back(Entry(4000.0, "battery", "covered", 3500.0, 3000.0));
    log.push_back(Entry(4300.0, "battery", "follow"));
    log.push_back(Entry(5400.0, "battery", "covered", 3600.0, 3000.0));
    log.push_back(Entry(5500.0, "battery", "covered", 1500.0, 3000.0));
    log.push_back(Entry(6000.0, "battery", "fire_mode", -1.0, -1.0, "independent"));
    campaign::LogEntry ok = Entry(6100.0, "21", "launch", -1.0, -1.0, "designation");
    ok.number = 6201;
    ok.designated = true;
    campaign::LogEntry wrong = ok;
    wrong.time_s = 6200.0;
    wrong.callsign = "22";
    wrong.designated = false;
    log.push_back(ok);
    log.push_back(wrong);
    campaign::LogEntry own = wrong;  // on its own by order: no breach of the launch itself
    own.mode = "independent";
    log.push_back(own);
    const std::vector<campaign::LimitFinding> f = campaign::CheckLimits(log, g);
    for (const campaign::LimitFinding& x : f) std::printf("    %.0f s %s %s: %s\n", x.time_s, x.who.c_str(), x.limit.c_str(), x.text.c_str());
    SC_EXPECT(f.size() == 5);
    SC_EXPECT(HasLimit(f, "spot", "22") && !HasLimit(f, "spot", "21"));
    SC_EXPECT(HasLimit(f, "emission") && HasLimit(f, "leash") && HasLimit(f, "fire", "battery") && HasLimit(f, "fire", "22"));
    int leash = 0;
    for (const campaign::LimitFinding& x : f) leash += x.limit == "leash" ? 1 : 0;
    SC_EXPECT(leash == 1);  // the one after 4000 s was followed within the grace
    const std::string text = campaign::LimitsCheckText(f, [](double t) { return std::to_string(static_cast<int>(t)); });
    SC_EXPECT(text.find("5 breaches") != std::string::npos);
    SC_EXPECT(campaign::LimitsCheckText({}, [](double) { return std::string(); }).find("no breach") != std::string::npos);
    // The combat report keeps the limits' lines apart from its claims.
    campaign::CombatReport rep = campaign::CompileReport("battery 21", 0.0, 7000.0, {}, {}, log, campaign::AfterActionValues());
    SC_EXPECT(rep.limits.size() == log.size() - 3);
}

SC_TEST("order limits (O4): the NPC battery commander keeps behind the covered company, knowing it only from its reports and his sight") {
    std::unique_ptr<ScenarioRunner> rp = LoadCompany();
    SC_EXPECT(rp != nullptr);
    if (!rp) return;
    ScenarioRunner& r = *rp;
    while (r.TaskReadyS() < 0.0 && r.Time() < 6000.0) r.Step(0.1);
    SC_EXPECT(r.TaskReadyS() > 0.0 && r.TaskReadyS() < 5400.0);  // before the company moves on
    std::printf("    ready at %.0f s; the company %s\n", r.TaskReadyS(), r.CoveredUnitKnown().known ? "known" : "not known");
    // Its reports on its net: the post's journal has them as words.
    int said = 0;
    for (const ScenarioRunner::RadioMessage& m : r.RadioLog()) said += m.text.find("нахожусь") != std::string::npos ? 1 : 0;
    SC_EXPECT(said >= 1 && r.CoveredUnitKnown().known);
    while (LogCount(r, "follow") == 0 && r.Time() < 9000.0) r.Step(0.1);
    SC_EXPECT(LogCount(r, "follow") >= 1);
    const double follow_s = r.Time();
    // On to the end: the battery has gone after it on the roads and stands within the leash of it.
    while (!r.Finished() && r.Time() < 10200.0) r.Step(0.1);
    const Vec3 c = LaunchersMiddle(r);
    const Vec3 k = CompanyAt(r);
    const double off = std::hypot(c.x - k.x, c.y - k.y);
    std::printf("    followed at %.0f s; at the end the launchers %.0f m from the company (the leash %.0f)\n", follow_s, off, r.LeashM());
    SC_EXPECT(off <= r.LeashM());
    SC_EXPECT(LogCount(r, "covered") >= 2);
    const std::vector<campaign::LimitFinding> f = r.LimitBreaches();
    for (const campaign::LimitFinding& x : f) std::printf("    breach: %s\n", x.text.c_str());
    SC_EXPECT(f.empty());
    // His map: the company where he last knew it, the leash round it, the fire mode and emission noted.
    const command::HeldMap m = r.HeldMapOf(r.BatteryCommanderPost());
    const command::HeldMark* cu = m.Find("covered_unit");
    SC_EXPECT(cu != nullptr && m.Find("leash") != nullptr);
    if (cu != nullptr) std::printf("    on his map: %s (%s, %s)\n", cu->label.c_str(), cu->source.c_str(), cu->as_of.c_str());
    SC_EXPECT(r.Debrief().find("The order's limits: no breach in the records.") != std::string::npos);
}

SC_TEST("order limits (O4): a player battery commander who stays put is not stopped; the record shows the company beyond the leash") {
    std::unique_ptr<ScenarioRunner> rp = LoadCompany();
    SC_EXPECT(rp != nullptr);
    if (!rp) return;
    ScenarioRunner& r = *rp;
    while (r.TaskReadyS() < 0.0 && r.Time() < 6000.0) r.Step(0.1);
    SC_EXPECT(r.TaskReadyS() > 0.0);
    SC_EXPECT(r.TakeSeat("battery_commander", "Player"));
    bool owed = false;
    while (!r.Finished() && r.Time() < 8400.0) {
        r.Step(0.1);
        for (const SeatDuty& d : r.AllOwedDuties()) owed = owed || d.id == "order_battery_follow";
    }
    SC_EXPECT(owed);  // the helper shows it as his
    SC_EXPECT(LogCount(r, "follow") == 0);
    const std::vector<campaign::LimitFinding> f = r.LimitBreaches();
    for (const campaign::LimitFinding& x : f) std::printf("    breach: %s\n", x.text.c_str());
    SC_EXPECT(HasLimit(f, "leash"));
    SC_EXPECT(r.Debrief().find("The order's limits: ") != std::string::npos && r.Debrief().find("no move ordered after it") != std::string::npos);
    // The order to follow is refused only while nobody knows where the company is; he can still give it now.
    SC_EXPECT(r.GiveCrewOrder("order_battery_follow"));
    SC_EXPECT(LogCount(r, "follow") == 1);
}

SC_TEST("order limits (O4): a launcher's spot as it reported it, against its place; moved to its commander's spot, it drives there") {
    std::unique_ptr<ScenarioRunner> rp = LoadCompany();
    SC_EXPECT(rp != nullptr);
    if (!rp) return;
    ScenarioRunner& r = *rp;
    while (r.TaskReadyS() < 0.0 && r.Time() < 6000.0) r.Step(0.1);
    const Battery::Vehicle* v21 = r.GetBattery()->Find("21");
    const Battery::Vehicle* v22 = r.GetBattery()->Find("22");
    SC_EXPECT(v21 != nullptr && v22 != nullptr && v21->has_slot);
    if (v21 == nullptr || v22 == nullptr) return;
    // The card says his spot's leeway, the fire mode and the emission at most.
    const command::SectorCard* card = r.SectorCardOf("21");
    SC_EXPECT(card != nullptr && card->fire == "designation" && card->emission == "limited" && card->spot_m == r.LauncherSpotM());
    if (card != nullptr) SC_EXPECT(command::SectorCardText(*card, r.Def().held_map).find("Fire: ") != std::string::npos);
    r.HearLauncherSpot("21", command::GridRef(v21->slot, r.Def().held_map));
    r.HearLauncherSpot("22", command::GridRef(Vec3{v22->slot.x + 2500.0, v22->slot.y, 0.0}, r.Def().held_map));
    SC_EXPECT(LogCount(r, "spot") == 2);
    const std::vector<campaign::LimitFinding> f = r.LimitBreaches();
    SC_EXPECT(HasLimit(f, "spot", "22") && !HasLimit(f, "spot", "21"));
    // A spot chosen 300 m off: the vehicle leaves its place and deploys there.
    const Battery::Vehicle* v23 = r.GetBattery()->Find("23");
    const Vec3 spot{v23->slot.x + 300.0, v23->slot.y, 0.0};
    SC_EXPECT(r.MoveLauncherSpot("23", spot));
    for (int i = 0; i < 12000 && !(v23->deployed && std::hypot(v23->position.x - spot.x, v23->position.y - spot.y) < 20.0); ++i) r.Step(0.1);
    std::printf("    23 at its spot: %.0f m off, deployed %d\n", std::hypot(v23->position.x - spot.x, v23->position.y - spot.y), v23->deployed ? 1 : 0);
    SC_EXPECT(v23->deployed && std::hypot(v23->position.x - spot.x, v23->position.y - spot.y) < 20.0);
    // The launcher commander's map: his spot's area round his place, the fire mode noted.
    const command::HeldMap m = r.HeldMapOf("21:commander");
    const command::HeldMark* area = m.Find("spot_area");
    SC_EXPECT(area != nullptr && area->radius_m == r.LauncherSpotM());
}

SC_TEST("order limits (O4): on designation only, the Osa's NPC commander launches at nothing the post did not give him") {
    std::string error;
    std::unique_ptr<BatteryLink> link = LoadBatteryLinkFile(kData + "/scenarios/battery_link_practice.json", kData, &error);
    SC_EXPECT(link != nullptr);
    if (!link) return;
    ScenarioRunner& osa = link->Osa();
    for (int i = 0; i < 9000 && osa.CommanderFireMode().empty(); ++i) {
        link->Step(0.1);
        osa.HearFireModeOrder("designation");
    }
    SC_EXPECT(osa.CommanderFireMode() == "designation");
    link->RunToEnd(0.1, 900.0);
    std::set<std::string> given;
    for (const C2Network::Event& d : osa.DesignationsHeard()) given.insert(d.subject);
    int launches = 0, outside = 0;
    for (const ScenarioRunner::CrewCall& c : osa.CrewCalls()) {
        if (c.kind != "launch" || c.target.empty()) continue;
        ++launches;
        outside += given.count(c.target) == 0 ? 1 : 0;
    }
    std::printf("    launches %d, at targets not designated %d (designations heard %d)\n", launches, outside, static_cast<int>(given.size()));
    SC_EXPECT(launches >= 1 && outside == 0);
}
