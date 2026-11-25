// The orders as people give them (track O, O3; docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md section 4): the
// regiment's evening briefing with the battery commander driven there and back, the papers handed there, the battery
// commander's combat order in person or by radio, the vehicle commander's briefing of his crew, the morning news, and
// the Osa beside the post in the session (simcore/campaign/briefings.hpp, mission_session.hpp, the runner's O3 part).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/campaign/briefings.hpp"
#include "simcore/campaign/mission_session.hpp"
#include "simcore/command/own_air.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

bool LoadRunner(const std::string& rel, std::unique_ptr<ScenarioRunner>& out, bool briefings) {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    if (!LoadScenarioFile(kData + "/" + rel, kData, def, console, &e)) {
        std::printf("    %s\n", e.c_str());
        return false;
    }
    if (briefings && !campaign::LoadBriefings(kData, "campaign/briefings.json", def.briefings, &e)) {
        std::printf("    %s\n", e.c_str());
        return false;
    }
    out = std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
    return true;
}

bool HasPaper(const ScenarioRunner& r, const std::string& station, const std::string& title_start) {
    for (const HeldPaper& p : r.MapCase(station)) {
        if (p.title.compare(0, title_start.size(), title_start) == 0) return true;
    }
    return false;
}

int Lines(const ScenarioRunner& r, const std::string& scene) {
    int n = 0;
    for (const campaign::SceneLine& l : r.SceneLines()) n += l.scene == scene ? 1 : 0;
    return n;
}

}  // namespace

SC_TEST("briefings (O3): the data loads with its items in order and its papers; a line's fields fill") {
    campaign::BriefingsData b;
    std::string e;
    SC_EXPECT(campaign::LoadBriefings(kData, "campaign/briefings.json", b, &e));
    SC_EXPECT(b.present && b.regiment.items.size() == 7 && b.regiment.items.front().id == "summing_up");
    SC_EXPECT(b.regiment.papers.size() == 3 && b.regiment.papers.front().id == "code_table");
    SC_EXPECT(!b.battery.items.empty() && !b.crew.items.empty() && !b.news.at_local_h.empty());
    SC_EXPECT(campaign::FillLine("Ready by {ready}, {x}.", {{"ready", "21:00"}}) == "Ready by 21:00, {x}.");
    SC_EXPECT(campaign::InPerson(campaign::OrderWay::Ground) && !campaign::InPerson(campaign::OrderWay::Radio));
}

SC_TEST("code table (O3): past its end, without the next, the crew stays on its last code and the table has run out") {
    const command::IffSchedule s = command::IffSchedule::Fixed(3);
    command::IffHolding h;
    h.valid_until_h = 24.0;  // PLACEHOLDER hour
    SC_EXPECT(!h.RunOut(23.0) && h.RunOut(24.0));
    SC_EXPECT(h.FilterAt(s, 23.0) == 3 && h.FilterAt(s, 30.0) == 3);
}

SC_TEST("combat order (O3): in person the vehicle commanders are called, the order said item by item, the cards handed") {
    std::unique_ptr<ScenarioRunner> r;
    if (!LoadRunner("scenarios/pu12m_mission_cover.json", r, true)) {
        SC_EXPECT(false);
        return;
    }
    while (r->CombatOrderWay() == campaign::OrderWay::Default && r->Time() < 3.0 * 3600.0 && !r->Finished()) r->Step(0.5);
    SC_EXPECT(campaign::InPerson(r->CombatOrderWay()));
    const ScenarioRunner::Gathering g = r->CurrentGathering();
    SC_EXPECT(g.id == 1 && g.end_s > g.start_s);
    SC_EXPECT(r->SectorCardOf("21") == nullptr);  // not before its end
    while (r->Time() < g.end_s + 2.0 && !r->Finished()) r->Step(0.5);
    SC_EXPECT(Lines(*r, "battery_order") == static_cast<int>(r->Briefings().battery.items.size()));
    const command::SectorCard* c = r->SectorCardOf("21");
    SC_EXPECT(c != nullptr && !c->written);
    SC_EXPECT(HasPaper(*r, "21:commander", "Sector card"));
    SC_EXPECT(r->TakeRadioOrders() == 0);
    const command::HeldMap m = r->HeldMapOf("21:commander");
    bool in_person = false;
    for (const command::HeldMark& k : m.marks) in_person = in_person || k.means == "in_person";
    SC_EXPECT(in_person);
}

SC_TEST("combat order (O3): away from his post he waits, then gives it by radio when the ready-by is near; the commanders write it down") {
    std::unique_ptr<ScenarioRunner> r;
    if (!LoadRunner("scenarios/pu12m_mission_cover.json", r, true)) {
        SC_EXPECT(false);
        return;
    }
    r->SetCommanderAway("at the regiment's command post");
    while (r->CombatOrderWay() == campaign::OrderWay::Default && r->Time() < 4.0 * 3600.0 && !r->Finished()) r->Step(0.5);
    SC_EXPECT(r->CombatOrderWay() == campaign::OrderWay::Radio);
    SC_EXPECT(r->TaskPlan().ready_by_s - r->Time() < r->Briefings().battery.urgent_s + 1.0);
    for (int i = 0; i < 4; ++i) r->Step(0.5);
    const command::SectorCard* c = r->SectorCardOf("21");
    SC_EXPECT(c != nullptr && c->written && c->landmarks.empty() && c->code_changes.empty() && !c->has_reserve);
    SC_EXPECT(HasPaper(*r, "21:commander", "Notes of the combat order") && !HasPaper(*r, "21:commander", "Sector card"));
    SC_EXPECT(r->TakeRadioOrders() == 1 && r->TakeRadioOrders() == 0);
    SC_EXPECT(Lines(*r, "battery_order") == 0);
}

SC_TEST("crew briefing (O3): the vehicle commander with a card briefs his crew on the intercom; whoever hears it knows it") {
    std::unique_ptr<ScenarioRunner> r;
    if (!LoadRunner("scenarios/osa_mission_cover.json", r, true)) {
        SC_EXPECT(false);
        return;
    }
    SC_EXPECT(!r->BriefCrew("Commander"));  // no card yet
    command::SectorCard c;  // PLACEHOLDER card
    c.vehicle = "21";
    c.point = Vec3{6000.0, -9200.0, 0.0};
    c.point_name = "the test point";
    c.from_deg = 120.0;
    c.to_deg = 200.0;
    c.given_s = r->Time();
    c.given = "19:00";
    r->TakeSectorCard(c, "the test");
    SC_EXPECT(HasPaper(*r, r->Cmdr(), "Sector card"));
    for (int i = 0; i < 200; ++i) r->Step(0.5);
    SC_EXPECT(Lines(*r, "crew_briefing") >= 4);
    SC_EXPECT(r->CrewKnows("operator", "sector") && r->CrewKnows("driver", "position"));
    SC_EXPECT(!r->CrewKnows("operator", "codes"));  // no code times on the card: not said
}

SC_TEST("mission session (O3): the briefing at the regiment's, the papers, the drive back on the roads and the order to the Osa in person") {
    campaign::MissionSessionDef d;
    d.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    d.briefings = "campaign/briefings.json";
    d.osa_scenario = kData + "/scenarios/osa_mission_cover.json";
    d.osa_callsign = "21";
    d.hours = 5.0;  // past the mission's own end: the session's clock runs on
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(d, kData, &e));
    if (s.Osa() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    using Trip = campaign::MissionSession::Trip;
    ScenarioRunner& post = s.Runner();
    ScenarioRunner& osa = *s.Osa();
    SC_EXPECT(s.BriefingTrip() == Trip::AtCp && !post.CommanderAway().empty());
    while (post.Time() < 200.0 && !post.Finished()) s.Step(0.5);
    SC_EXPECT(s.BriefingsHeld() == 1 && Lines(post, "regiment_briefing") == 7);
    SC_EXPECT(HasPaper(post, post.Cmdr(), "Identification code table") && HasPaper(post, post.Cmdr(), "Plan of cooperation"));
    SC_EXPECT(s.BriefingTrip() == Trip::Returning);
    const nav::Mover* car = post.Nav() != nullptr ? post.Nav()->GetMovers().Find(campaign::MissionSession::kCarId) : nullptr;
    SC_EXPECT(car != nullptr && car->route.found && car->route.length_m > 1000.0);
    // The task came in person at the briefing: his map's task marks say so.
    bool traced = false;
    for (const command::HeldMark& k : post.HeldMapOf(post.BatteryCommanderPost()).marks) traced = traced || (k.source == "the regiment" && k.means == "in_person");
    SC_EXPECT(traced);
    // Back at his post by the roads, then his combat order: not given while he was away.
    while (s.BriefingTrip() != Trip::AtPost && post.Time() < 3.0 * 3600.0 && !post.Finished()) s.Step(0.5);
    SC_EXPECT(s.BriefingTrip() == Trip::AtPost && post.CommanderAway().empty());
    SC_EXPECT(car->arrived_s > 0.0);
    while (post.CombatOrderWay() == campaign::OrderWay::Default && post.Time() < 4.0 * 3600.0 && !post.Finished()) s.Step(0.5);
    const campaign::OrderWay way = post.CombatOrderWay();
    SC_EXPECT(way != campaign::OrderWay::Default);
    const double until = post.CurrentGathering().end_s;
    const double stop_s = std::max(until, post.Time()) + 300.0;
    while (post.Time() < stop_s && !post.Finished()) s.Step(0.5);
    const command::SectorCard* c = osa.SectorCardOf("21");
    if (way == campaign::OrderWay::Radio) {
        SC_EXPECT(c != nullptr && c->written);
    } else {
        // In person: the Osa's commander walked over (no teleport) and took his card, or missed it and the record says so.
        bool missed = false;
        for (const std::string& n : s.Notes()) missed = missed || n.find("was not at the battery commander's") != std::string::npos;
        SC_EXPECT((c != nullptr && !c->written && HasPaper(osa, osa.Cmdr(), "Sector card")) || missed);
        if (c != nullptr) std::printf("    the Osa's commander took his card in person (%s)\n", campaign::OrderWayName(way));
    }
    // The Osa's map is its commander's own.
    const command::HeldMap m = s.HeldMapFor("21:commander");
    SC_EXPECT(m.holder == "21:commander");
}

SC_TEST("mission session (O3): a battery commander who does not go misses the briefing: no papers, and the record says so") {
    campaign::MissionSessionDef d;
    d.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    d.briefings = "campaign/briefings.json";
    d.start_hour = 18.75;  // PLACEHOLDER: a quarter of an hour before it
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(d, kData, &e));
    ScenarioRunner& post = s.Runner();
    SC_EXPECT(s.BriefingTrip() == campaign::MissionSession::Trip::AtPost);
    SC_EXPECT(post.TakeSeat("battery_commander", "player"));  // the player in his seat, who never leaves
    while (post.Time() < 3600.0 && !post.Finished()) s.Step(1.0);
    SC_EXPECT(s.BriefingsMissed() == 1 && s.BriefingsHeld() == 0);
    SC_EXPECT(!HasPaper(post, post.Cmdr(), "Identification code table"));
    bool noted = false;
    for (const std::string& n : s.Notes()) noted = noted || n.find("missed the regiment's briefing") != std::string::npos;
    SC_EXPECT(noted && post.Debrief().find("missed the regiment's briefing") != std::string::npos);
}

SC_TEST("news (O3): the state radio's seven o'clock bulletin reaches whoever hears a radio") {
    campaign::MissionSessionDef d;
    d.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    d.briefings = "campaign/briefings.json";
    d.start_hour = 6.95;  // PLACEHOLDER: three minutes before it
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(d, kData, &e));
    ScenarioRunner& post = s.Runner();
    while (post.Time() < 400.0 && !post.Finished()) s.Step(1.0);
    bool heard = false;
    for (const DeliveryDef& dd : post.Deliveries()) heard = heard || (dd.means == DeliveryMeans::Broadcast && dd.words.find("state radio") != std::string::npos);
    SC_EXPECT(heard);
}
