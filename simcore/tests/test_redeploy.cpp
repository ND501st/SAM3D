// Track O, O7: the officers' redeployments (docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md section 8): each situation
// a rule of data/campaign/tasking.json read by simcore/campaign/regiment.hpp on what reached the regiment, weighted by
// the adversary and the regiment commander's traits; launchers detached on the battery net and driven on the roads
// (ScenarioRunner's detachments); the reserve battery's relief (campaign/mission_session.hpp).
#include <cmath>
#include <cstdio>
#include <map>
#include <string>

#include "simcore/campaign/campaign_save.hpp"
#include "simcore/campaign/mission_session.hpp"
#include "simcore/campaign/regiment.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

campaign::TaskingRules Rules() {
    campaign::TaskingRules rules;
    std::string e;
    if (!campaign::LoadTaskingRules(kData, "campaign/tasking.json", rules, &e)) std::printf("    %s\n", e.c_str());
    return rules;
}

const campaign::TaskingRule* RuleOf(const campaign::TaskingRules& rules, const std::string& id) {
    for (const campaign::TaskingRule& r : rules.rules) {
        if (r.id == id) return &r;
    }
    return nullptr;
}

// The rules with only the one wanted (the others would answer first on some pictures).
campaign::TaskingRules Only(const std::string& id) {
    campaign::TaskingRules all = Rules();
    campaign::TaskingRules one = all;
    one.rules.clear();
    for (const campaign::TaskingRule& r : all.rules) {
        if (r.id == id || r.id == "loan_over") one.rules.push_back(r);
    }
    return one;
}

campaign::BatteryTask Bridges() {
    campaign::BatteryTask t;
    std::string e;
    campaign::LoadBatteryTask(kData, "campaign/tasks/bty21_cover_bridges.json", t, &e);
    return t;
}

// The regiment's places by hand (session metres): the bridges at the middle, the rest round it.
std::map<std::string, Vec3> HandPlaces() {
    return {{"bty21_bridges", {0, 0, 0}},          {"bty21_bridges_reserve", {1500, 0, 0}}, {"bty21_dombas", {30000, 5000, 0}},
            {"bty21_south", {0, -30000, 0}},       {"bty21_dam", {-30000, 0, 0}},           {"kesmark_station", {3000, 2000, 0}},
            {"kesmark_sugar", {-3000, 2000, 0}},   {"div_hq", {0, -12000, 0}},              {"tech_battery", {5000, -8000, 0}},
            {"vrbice_crossroads", {-6000, -2000, 0}}, {"lazne_woods", {-8000, 3000, 0}},    {"rava_marshes", {-10000, -5000, 0}},
            {"vrbice", {-5000, -1000, 0}},         {"horka", {4000, 4000, 0}},              {"dubnik", {6000, -3000, 0}},
            {"polany", {-2000, 6000, 0}},          {"rgt_rear", {0, -20000, 0}},            {"bty21_cp", {500, 500, 0}}};
}

campaign::RegimentTasking Regiment(const campaign::TaskingRules& rules) {
    campaign::RegimentTasking rgt(rules, kData);
    const std::map<std::string, Vec3> places = HandPlaces();
    rgt.SetPlaces(
        [places](const std::string& id, Vec3& out) {
            const auto it = places.find(id);
            if (it == places.end()) return false;
            out = it->second;
            return true;
        },
        [](const std::string& id) { return "the place " + id; });
    return rgt;
}

campaign::RegimentPicture At(double now_h, const std::string& adversary) {
    campaign::RegimentPicture p;
    p.now_h = now_h;
    p.adversary = adversary;
    p.has_battery_at = true;
    p.battery_at = Vec3{0, 0, 0};
    return p;
}

campaign::RegimentPicture::Raid Raid(double reached_h, const Vec3& at, const std::string& object = "", bool helicopters = false,
                                     const std::string& entry = "west", bool strategic = false) {
    campaign::RegimentPicture::Raid r;
    r.h = reached_h - 0.2;
    r.reached_h = reached_h;
    r.at = at;
    r.object = object;
    r.target = object.empty() ? std::string("a target") : "the " + object;
    r.helicopters = helicopters;
    r.entry = entry;
    r.strategic = strategic;
    return r;
}

}  // namespace

SC_TEST("redeploy (O7): the eight situations and O6's three moves load as rules, each sourced, the officer's traits with them") {
    const campaign::TaskingRules rules = Rules();
    SC_EXPECT(rules.rules.size() == 20);
    for (const char* id : {"cut_off_withdraw", "contact_lost_roving", "loan_over", "worn_relieved", "help_elsewhere", "detach_for_object",
                           "rebels_singles", "helicopter_ambush", "deep_strike_cover", "second_echelon_to_threat", "cover_first_echelon",
                           "bounds_forward"}) {
        const campaign::TaskingRule* r = RuleOf(rules, id);
        SC_EXPECT(r != nullptr);
        if (r != nullptr) SC_EXPECT(r->ready_in_s > 0.0 && !r->reason.empty());
    }
    SC_EXPECT(rules.officer.post == "rgt_commander");
    SC_EXPECT_NEAR(rules.officer.Willingness(), 0.52, 1e-9);  // 0.5 + 0.2 x (0.6 + 0.5 - 0.4 - 0.6)
    // Each trigger takes its own task, and the tasks their places.
    const auto bad = [](const std::string& rule) {
        json::Value root;
        std::string pe;
        const std::string text = R"({"source": "sam3d", "confidence": "estimate", "rules": [)" + rule + "]}";
        if (!json::Parse(text, root, &pe)) return std::string("parse: ") + pe;
        campaign::TaskingRules r;
        campaign::detail::TaskingLoader l;
        return l.Load(root, r) ? std::string() : l.Message();
    };
    const std::string prov = R"(, "reason": "r", "ready_in_s": 60, "source": "sam3d", "confidence": "estimate"})";
    SC_EXPECT(bad(R"({"id": "a", "when": "heavier_elsewhere", "task": "relocate", "positions": ["x"])" + prov).find("redeploy") != std::string::npos);
    SC_EXPECT(bad(R"({"id": "a", "when": "heavier_elsewhere", "task": "redeploy")" + prov).find("positions") != std::string::npos);
    SC_EXPECT(bad(R"({"id": "a", "when": "special_mission", "task": "ambush", "positions": ["x"])" + prov).find("ambush") != std::string::npos);
    SC_EXPECT(bad(R"({"id": "a", "when": "battery_worn", "task": "relief", "positions": ["x"])" + prov).find("reserve") != std::string::npos);
    SC_EXPECT(bad(R"({"id": "a", "when": "division_advances", "task": "bounds", "positions": ["x"])" + prov).find("forward_m") != std::string::npos);
    SC_EXPECT(bad(R"({"id": "a", "when": "asset_struck", "task": "detach_one", "positions": ["x"], "weights": {"nato": 2})" + prov).find("weight") !=
              std::string::npos);
}

SC_TEST("redeploy (O7): a rule's score is its adversary's weight and the officer's willingness to leave the book") {
    const campaign::TaskingRules rules = Rules();
    const campaign::TaskingRule* singles = RuleOf(rules, "rebels_singles");
    const campaign::TaskingRule* detach = RuleOf(rules, "detach_for_object");
    const campaign::TaskingRule* worn = RuleOf(rules, "worn_relieved");
    SC_EXPECT(singles != nullptr && detach != nullptr && worn != nullptr);
    if (singles == nullptr || detach == nullptr || worn == nullptr) return;
    // The rebels' rule is the rebels' only.
    SC_EXPECT_NEAR(campaign::RuleScore(*singles, rules.officer, "rebels"), 1.0 * (1.0 - 0.6 * (1.0 - 0.52)), 1e-9);
    SC_EXPECT_NEAR(campaign::RuleScore(*singles, rules.officer, "nato"), 0.0, 1e-9);
    // Splitting the battery for an object: against the rebels, not against NATO, with this commander.
    SC_EXPECT(campaign::RuleScore(*detach, rules.officer, "rebels") >= detach->threshold);
    SC_EXPECT(campaign::RuleScore(*detach, rules.officer, "nato") < detach->threshold);
    // A rigid, methodical commander keeps to the book: he does not split it even against the rebels.
    campaign::TaskingOfficer stiff = rules.officer;
    stiff.boldness = stiff.creativity = 0.0;
    stiff.rigidity = stiff.methodical = 1.0;
    SC_EXPECT_NEAR(stiff.Willingness(), 0.1, 1e-9);
    SC_EXPECT(campaign::RuleScore(*singles, stiff, "rebels") < singles->threshold);
    SC_EXPECT(campaign::RuleScore(*detach, stiff, "rebels") < campaign::RuleScore(*detach, rules.officer, "rebels"));
    // A textbook rule with no weights is the same for everyone.
    SC_EXPECT_NEAR(campaign::RuleScore(*worn, stiff, "nato"), 1.0, 1e-9);
}

SC_TEST("redeploy (O7, situation 1): raids heavier elsewhere move the battery to the prepared position nearest them, once") {
    campaign::RegimentTasking rgt = Regiment(Only("help_elsewhere"));
    const campaign::BatteryTask holds = Bridges();
    campaign::RegimentPicture p = At(30.0, "warsaw_pact");
    for (int i = 0; i < 3; ++i) p.raids.push_back(Raid(20.0 + i, Vec3{28000.0 + 500.0 * i, 6000.0, 0.0}));
    p.raids.push_back(Raid(21.0, Vec3{2000.0, 0.0, 0.0}));  // one near the battery
    // Routine: at the briefing.
    SC_EXPECT(!rgt.Decide(p, &holds, 0.0, false).give);
    campaign::TaskingDecision d = rgt.Decide(p, &holds, 0.0, true);
    SC_EXPECT(d.give && d.rule == "help_elsewhere" && d.task.main == "bty21_dombas" && d.task.kind == campaign::TaskKind::CoverObject);
    SC_EXPECT(d.task.id == "bty21_cover_bridges_o7_help_elsewhere" && !d.task.reasons.empty() && d.task.reconnoitre);
    SC_EXPECT(d.fact_h > 22.0 - 1e-9);
    // Held, it does not move it again on the same raids; and too few raids, or as many near it, move nothing.
    SC_EXPECT(!rgt.Decide(p, &d.task, 0.0, true).give);
    campaign::RegimentTasking fresh = Regiment(Only("help_elsewhere"));
    campaign::RegimentPicture even = p;
    for (int i = 0; i < 2; ++i) even.raids.push_back(Raid(22.0, Vec3{1000.0, 1000.0, 0.0}));
    SC_EXPECT(!fresh.Decide(even, &holds, 0.0, true).give);
    // Against the rebels the weight is below the threshold: the regiment does not mass against their raids.
    campaign::RegimentTasking rebels = Regiment(Only("help_elsewhere"));
    p.adversary = "rebels";
    SC_EXPECT(!rebels.Decide(p, &holds, 0.0, true).give);
}

SC_TEST("redeploy (O7): an object struck again and again gets one launcher detached for two days, then the task comes back") {
    campaign::RegimentTasking rgt = Regiment(Only("detach_for_object"));
    const campaign::BatteryTask holds = Bridges();
    campaign::RegimentPicture p = At(30.0, "rebels");
    p.raids.push_back(Raid(25.0, Vec3{5100.0, -8000.0, 0.0}, "tech_battery_obj"));
    p.raids.push_back(Raid(27.0, Vec3{5100.0, -8000.0, 0.0}, "tech_battery_obj"));
    campaign::TaskingDecision d = rgt.Decide(p, &holds, 0.0, true);
    SC_EXPECT(d.give && d.rule == "detach_for_object");
    SC_EXPECT(d.task.detached.size() == 1 && d.task.detached.front().vehicle == "24" && d.task.detached.front().place == "tech_battery");
    SC_EXPECT(d.task.main == holds.main && d.task.Places().size() > holds.Places().size());
    SC_EXPECT(campaign::TaskDocument(d.task, 0.0, [](double) { return std::string("12:00"); }, [](const std::string& s) { return s; })
                  .find("Detached") != std::string::npos);
    SC_EXPECT(rgt.TemporaryTask() == d.task.id);
    SC_EXPECT_NEAR(rgt.TemporaryUntilH(), 30.0 + 48.0, 1e-9);
    // Before its hours are up nothing; then the task held before, at the briefing.
    SC_EXPECT(!rgt.Decide(At(60.0, "rebels"), &d.task, 0.0, true).give);
    const campaign::TaskingDecision back = rgt.Decide(At(79.0, "rebels"), &d.task, 0.0, true);
    SC_EXPECT(back.give && back.rule == "loan_over" && back.task.id == holds.id && back.task.detached.empty());
    // NATO's raids on it move nothing with this commander (the rule's weight and his traits).
    campaign::RegimentTasking nato = Regiment(Only("detach_for_object"));
    p.adversary = "nato";
    SC_EXPECT(!nato.Decide(p, &holds, 0.0, true).give);
}

SC_TEST("redeploy (O7, situation 2): a battery long in line or short of missiles is relieved by the reserve, once per report") {
    campaign::RegimentTasking rgt = Regiment(Only("worn_relieved"));
    const campaign::BatteryTask holds = Bridges();
    campaign::RegimentPicture p = At(50.0, "nato");
    p.ready_report_h = 10.0;
    p.missiles_reported = 20;
    SC_EXPECT(!rgt.Decide(p, &holds, 0.0, true).give);  // 40 h in line, missiles enough
    p.missiles_reported = 6;
    campaign::TaskingDecision d = rgt.Decide(p, &holds, 0.0, true);
    SC_EXPECT(d.give && d.relief && d.reserve_from == "kesmark_barracks" && d.reserve_unit.find("25th") != std::string::npos);
    SC_EXPECT(d.task.main == "rgt_rear" && d.task.kind == campaign::TaskKind::Relocate && d.task.readiness == 2);
    SC_EXPECT_NEAR(d.rest_h, 12.0, 1e-9);
    SC_EXPECT(rgt.ReliefPending());
    SC_EXPECT(!rgt.Decide(p, &holds, 0.0, true).give);  // once per report
    rgt.ReliefDone();
    SC_EXPECT(!rgt.ReliefPending());
    campaign::RegimentTasking tired = Regiment(Only("worn_relieved"));
    campaign::RegimentPicture q = At(83.0, "nato");
    q.ready_report_h = 10.0;
    q.missiles_reported = 20;
    SC_EXPECT(tired.Decide(q, &holds, 0.0, true).give);  // three days in line
}

SC_TEST("redeploy (O7, situation 7): with all the regiment's radars off the air the battery fights alone, roving") {
    // What the regiment knows of its radars: their reports and the reports of them wrecked, never their state.
    campaign::RadarWord word;
    word.Add("p19_north", 29.0);
    word.Add("p40_south", 29.0);
    campaign::RegimentTasking rgt = Regiment(Only("contact_lost_roving"));
    const campaign::BatteryTask holds = Bridges();
    const auto at = [&word](double now_h) {
        campaign::RegimentPicture p = At(now_h, "warsaw_pact");
        word.Fill(p, now_h);
        return p;
    };
    // Both struck at 29.0 (their last word); five minutes on nothing has reached the regiment: it fights on as before.
    campaign::RegimentPicture p = at(29.0 + 5.0 / 60.0);
    SC_EXPECT(p.radars_total == 2 && p.radars_out == 0 && p.radars_out_h < 0.0);
    SC_EXPECT(!rgt.Decide(p, &holds, 0.0, false).give);
    // One wreck reported at 29.1; the other's report still on its way: not all are out.
    word.Reported("p19_north", 29.0, 29.1);
    word.Reported("p40_south", 29.0, 29.4);
    p = at(29.12);
    SC_EXPECT(p.radars_out == 1 && p.radars_out_h < 0.0);
    SC_EXPECT(!rgt.Decide(p, &holds, 0.0, false).give);
    // The second one silent for a quarter of an hour (its report not yet in): now the regiment has lost both, and acts once
    // its staff has worked on it (the rule's five minutes).
    p = at(29.0 + campaign::RadarWord::kSilentH + 0.01);
    SC_EXPECT(p.radars_out == 2 && !rgt.Decide(p, &holds, 0.0, false).give);
    p = at(29.0 + campaign::RadarWord::kSilentH + 0.1);
    SC_EXPECT(p.radars_out == 2);
    SC_EXPECT_NEAR(p.radars_out_h, 29.0 + campaign::RadarWord::kSilentH, 1e-9);
    const campaign::TaskingDecision d = rgt.Decide(p, &holds, 0.0, false);
    SC_EXPECT(d.give && d.task.kind == campaign::TaskKind::Ambush && d.task.roving && d.task.limits.fire == "independent");
    SC_EXPECT(!d.task.reserve.empty());
    SC_EXPECT(!rgt.Decide(p, &d.task, 0.0, false).give);  // once per outage
    // Both wrecks reported fast: the reports, not the silence, tell it (the later report's hour).
    campaign::RadarWord fast;
    fast.Add("a", 40.0);
    fast.Add("b", 40.0);
    fast.Reported("a", 40.0, 40.05);
    fast.Reported("b", 40.0, 40.08);
    campaign::RegimentPicture q = At(40.2, "warsaw_pact");
    fast.Fill(q, 40.2);
    SC_EXPECT(q.radars_out == 2);
    SC_EXPECT_NEAR(q.radars_out_h, 40.08, 1e-9);
    campaign::RegimentTasking other = Regiment(Only("contact_lost_roving"));
    SC_EXPECT(other.Decide(q, &holds, 0.0, false).give);
    // Repaired and heard again: an old report no longer counts.
    fast.Heard("a", 41.0);
    fast.Heard("b", 41.0);
    fast.Fill(q, 41.05);
    SC_EXPECT(q.radars_out == 0 && q.radars_out_h < 0.0);
    // Down in the session's truth but heard a minute ago (the regiment does not know yet): nothing out.
    campaign::RadarWord quiet;
    quiet.Add("a", 50.0);
    campaign::RegimentPicture r = At(50.02, "warsaw_pact");
    quiet.Fill(r, 50.02);
    SC_EXPECT(r.radars_total == 1 && r.radars_out == 0);
}

SC_TEST("redeploy (O7, situation 3): the covered division broken, the battery withdraws alone to the next line, on its own") {
    campaign::RegimentTasking rgt = Regiment(Only("cut_off_withdraw"));
    rgt.SetPlaceDepth([](const std::string& place, double& depth_m, std::string& name) {
        static const std::map<std::string, double> m{{"bty21_bridges", 4000.0}, {"bty21_depth_south", 12000.0}, {"bty21_depth_east", 7000.0}};
        const auto it = m.find(place);
        if (it == m.end()) return false;
        depth_m = it->second;
        name = place;
        return true;
    });
    const campaign::BatteryTask holds = Bridges();
    campaign::RegimentPicture p = At(30.0, "warsaw_pact");
    p.dispersals = 1;
    p.dispersal_h = 29.0;
    p.dispersal = "the 7th Motor Rifle Division is broken and dispersed";
    const campaign::TaskingDecision d = rgt.Decide(p, &holds, 0.0, false);
    SC_EXPECT(d.give && d.task.main == "bty21_depth_south" && d.task.limits.fire == "independent");
    SC_EXPECT(d.task.id.find("_o7_cut_off_withdraw") != std::string::npos && d.task.reasons.front().find("dispersed") != std::string::npos);
    SC_EXPECT(!rgt.Decide(p, &d.task, 0.0, false).give);  // each once
}

SC_TEST("redeploy (O7, situation 6): strikes on the country's power and dams lend the battery to the dam for three days") {
    campaign::RegimentTasking rgt = Regiment(Only("deep_strike_cover"));
    const campaign::BatteryTask holds = Bridges();
    campaign::RegimentPicture p = At(40.0, "nato");
    p.raids.push_back(Raid(30.0, Vec3{-29000.0, 1000.0, 0.0}, "rava_dam", false, "west", true));
    p.raids.push_back(Raid(33.0, Vec3{-29000.0, 1000.0, 0.0}, "rava_dam", false, "west", true));
    p.raids.push_back(Raid(34.0, Vec3{25000.0, 0.0, 0.0}, "airfield"));  // not strategic
    const campaign::TaskingDecision d = rgt.Decide(p, &holds, 0.0, true);
    SC_EXPECT(d.give && d.task.main == "bty21_dam");
    SC_EXPECT_NEAR(rgt.TemporaryUntilH(), 40.0 + 72.0, 1e-9);
    campaign::RegimentTasking rebels = Regiment(Only("deep_strike_cover"));
    p.adversary = "rebels";
    SC_EXPECT(!rebels.Decide(p, &holds, 0.0, true).give);
}

SC_TEST("redeploy (O7, situation 5): helicopters again and again by one way: an ambush on it, radars silent, for a day") {
    campaign::RegimentTasking rgt = Regiment(Only("helicopter_ambush"));
    const campaign::BatteryTask holds = Bridges();
    campaign::RegimentPicture p = At(40.0, "neutral");
    p.raids.push_back(Raid(30.0, Vec3{-6000.0, -2000.0, 0.0}, "", true, "the Rava valley"));
    SC_EXPECT(!rgt.Decide(p, &holds, 0.0, true).give);  // one is not a habit
    p.raids.push_back(Raid(35.0, Vec3{-7000.0, -2500.0, 0.0}, "", true, "the Rava valley"));
    const campaign::TaskingDecision d = rgt.Decide(p, &holds, 0.0, true);
    SC_EXPECT(d.give && d.task.kind == campaign::TaskKind::Ambush && d.task.main == "vrbice_crossroads" && d.task.reserve.size() == 2);
    SC_EXPECT(d.task.emission == "silent" && d.task.task_text.find("the Rava valley") != std::string::npos);
    SC_EXPECT_NEAR(rgt.TemporaryUntilH(), 64.0, 1e-9);
}

SC_TEST("redeploy (O7, situation 4): against the rebels' helicopters the launchers go singly to the villages, on their own") {
    campaign::RegimentTasking rgt = Regiment(Only("rebels_singles"));
    const campaign::BatteryTask holds = Bridges();
    campaign::RegimentPicture p = At(40.0, "rebels");
    for (int i = 0; i < 3; ++i) p.raids.push_back(Raid(30.0 + i, Vec3{-4000.0, 0.0, 0.0}, "", true, "the south bank"));
    p.raids.push_back(Raid(31.0, Vec3{-4000.0, 0.0, 0.0}));
    const campaign::TaskingDecision d = rgt.Decide(p, &holds, 0.0, true);
    SC_EXPECT(d.give && d.task.detached.size() == 4 && d.task.limits.fire == "independent");
    if (d.task.detached.size() == 4) {
        SC_EXPECT(d.task.detached[0].vehicle == "21" && d.task.detached[0].place == "vrbice");
        SC_EXPECT(d.task.detached[3].vehicle == "24" && d.task.detached[3].place == "polany");
    }
    SC_EXPECT(!rgt.Decide(p, &d.task, 0.0, true).give);  // split already
    campaign::RegimentTasking pact = Regiment(Only("rebels_singles"));
    p.adversary = "warsaw_pact";
    SC_EXPECT(!pact.Decide(p, &holds, 0.0, true).give);  // the rebels' rule only
}

SC_TEST("redeploy (O6 part 3): the second echelon goes to a threatened sector, a battery covers the counterattack, bounds forward") {
    const campaign::BatteryTask holds = Bridges();
    {
        campaign::RegimentTasking rgt = Regiment(Only("second_echelon_to_threat"));
        campaign::RegimentPicture p = At(40.0, "warsaw_pact");
        p.threatened_sector = "dombas_gap";
        p.threatened_h = 39.0;
        p.threatened_why = "The army's chief of air defence raises the Dombas gap to defend.";
        p.has_threatened_at = true;
        p.threatened_at = Vec3{29000.0, 9000.0, 0.0};
        const campaign::TaskingDecision d = rgt.Decide(p, &holds, 0.0, false);
        SC_EXPECT(d.give && d.task.main == "bty21_dombas" && d.task.reasons.front().find("Dombas") != std::string::npos);
        SC_EXPECT(!rgt.Decide(p, &d.task, 0.0, false).give);
    }
    {
        campaign::RegimentTasking rgt = Regiment(Only("cover_first_echelon"));
        campaign::RegimentPicture p = At(40.0, "warsaw_pact");
        p.counterattacks = 1;
        p.counterattack_h = 39.5;
        p.counterattack = "The 7th Motor Rifle Division's first echelon counterattacks at dawn.";
        p.has_counterattack_at = true;
        p.counterattack_at = Vec3{0.0, 8000.0, 0.0};
        const campaign::TaskingDecision d = rgt.Decide(p, &holds, 0.0, false);
        SC_EXPECT(d.give && d.task.kind == campaign::TaskKind::CoverTroops && d.task.main == "bty21_bridges");
        SC_EXPECT_NEAR(d.task.axis_deg, 0.0, 1e-6);  // north, toward the line
        SC_EXPECT_NEAR(d.task.ready_in_s, 3600.0, 1e-9);  // ready an hour before it (vr_1981_fanfule)
    }
    {
        campaign::RegimentTasking rgt = Regiment(Only("bounds_forward"));
        rgt.SetPlaceDepth([](const std::string& place, double& depth_m, std::string& name) {
            static const std::map<std::string, double> m{{"bty21_bridges", 12000.0}, {"bty21_bridges_reserve", 11000.0}, {"bty21_south", 20000.0},
                                                         {"bty21_dombas", 7000.0}, {"bty21_cp", 2000.0}};
            const auto it = m.find(place);
            if (it == m.end()) return false;
            depth_m = it->second;
            name = place;
            return true;
        });
        campaign::RegimentPicture p = At(40.0, "warsaw_pact");
        p.advances = 1;
        p.advance_h = 39.5;
        p.advance = "The division's attack is resumed.";
        const campaign::TaskingDecision d = rgt.Decide(p, &holds, 0.0, false);
        SC_EXPECT(d.give && d.task.main == "bty21_dombas");  // the nearest a bound (4 km) forward, not the furthest
        p.advances = 2;
        const campaign::TaskingDecision next = rgt.Decide(p, &d.task, 0.0, false);
        SC_EXPECT(next.give && next.task.main == "bty21_cp");  // and the next bound
    }
}

// ------------------------------------------------------------------------------------- in a session: by a means, on roads

SC_TEST("redeploy (O7): split singly, each launcher is told on the battery net, drives to its village on the roads, deploys on its own") {
    campaign::MissionSessionDef md;
    md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    md.tasking = "campaign/tasking.json";
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(md, kData, &e));
    if (s.Regiment() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    ScenarioRunner& r = s.Runner();
    while (!r.Finished() && r.TaskReadyS() < 0.0) s.Step(0.2);
    SC_EXPECT(r.TaskReadyS() > 0.0);
    const campaign::TaskingRules rules = Rules();
    const campaign::TaskingRule* rule = RuleOf(rules, "rebels_singles");
    SC_EXPECT(rule != nullptr);
    if (rule == nullptr) return;
    const campaign::BatteryTask singles = campaign::SinglesTask(r.Task(), *rule, [&r](const std::string& id) { return r.PlaceNameOf(id); }, "test");
    std::string why;
    SC_EXPECT(r.GiveTask(singles, "radio", &why));
    const double given = r.Time();
    bool away = false;
    while (!r.Finished() && r.Time() < given + 3.0 * 3600.0) {
        s.Step(0.2);
        for (const ScenarioRunner::DetachRun& d : r.DetachRuns()) {
            const bool now = d.started && !d.there;
            if (now && !away) std::printf("    %s sets off at %.0f s\n", d.vehicle.c_str(), r.Time() - given);
            away = away || now;
        }
        bool all = !r.DetachRuns().empty();
        for (const ScenarioRunner::DetachRun& d : r.DetachRuns()) all = all && d.there;
        if (all) {
            bool deployed = true;
            for (const campaign::TaskDetach& d : singles.detached) deployed = deployed && r.GetBattery()->Find(d.vehicle)->deployed;
            if (deployed) break;
        }
    }
    for (const std::string& n : s.Notes()) std::printf("    %s\n", n.c_str());
    std::printf("    the launchers in their villages %.0f min after the task\n", (r.Time() - given) / 60.0);
    SC_EXPECT(away);  // driven, not put there
    SC_EXPECT(singles.detached.size() == 4);
    SC_EXPECT(r.DetachRuns().size() == singles.detached.size());
    for (const ScenarioRunner::DetachRun& d : r.DetachRuns()) {
        SC_EXPECT(d.there && !d.failed && d.order >= 0);
        const nav::Anchor* a = r.Nav()->GetAnchors().Find(d.place);
        const Battery::Vehicle* v = r.GetBattery()->Find(d.vehicle);
        SC_EXPECT(a != nullptr && v != nullptr);
        if (a == nullptr || v == nullptr) continue;
        SC_EXPECT(v->detached && v->deployed && std::hypot(v->position.x - a->at.east_m, v->position.y - a->at.north_m) < 600.0);
        std::string at;
        SC_EXPECT(r.LauncherDetached(d.vehicle, &at) && at == d.place);
        bool own = false;
        for (const BatteryFire::Shooter& sh : r.GetBatteryFire()->Shooters()) own = own || (sh.callsign == d.vehicle && sh.on_own);
        SC_EXPECT(own);
    }
    // The orders went on the battery net, each to its launcher; the log has them.
    const std::string d = r.Debrief();
    for (std::size_t at = d.find("detach"); at != std::string::npos; at = d.find("detach", at + 1)) {
        const std::size_t from = d.rfind('\n', at) + 1;
        std::printf("    %s\n", d.substr(from, d.find('\n', at) - from).c_str());
        at = d.find('\n', at);
        if (at == std::string::npos) break;
    }
    SC_EXPECT(d.find("выдвинуться в район") != std::string::npos);
}

SC_TEST("redeploy (O7): the reserve battery comes on the roads, takes over the position, and the worn battery marches to rest") {
    campaign::MissionSessionDef md;
    md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    md.tasking = "campaign/tasking.json";
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(md, kData, &e));
    if (s.Regiment() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    ScenarioRunner& r = s.Runner();
    while (!r.Finished() && r.LastReportS() < 0.0) s.Step(0.2);
    SC_EXPECT(r.LastReportMissiles() >= 0 && s.InLineH() >= 0.0);  // its ready report: in line from then
    // The regiment's picture three days on: the relief.
    campaign::RegimentPicture p = s.RegimentHolds();
    p.now_h = s.InLineH() + 80.0;
    SC_EXPECT(s.RegimentDecides(p, true));
    SC_EXPECT(std::string(s.ReliefStage()) == "coming");
    const nav::Mover* col = r.Nav()->GetMovers().Find("regiment_reserve_battery");
    SC_EXPECT(col != nullptr && col->vehicles == 6);
    const std::string held = r.Task().id;
    const double from = r.Time();
    while (!r.Finished() && std::string(s.ReliefStage()) == "coming" && r.Time() < from + 4.0 * 3600.0) s.Step(0.5);
    for (const std::string& n : s.Notes()) std::printf("    %s\n", n.c_str());
    SC_EXPECT(std::string(s.ReliefStage()) == "resting");
    SC_EXPECT(r.Task().id != held && r.Task().main == "rgt_rear");
    bool beside = false;
    for (const ScenarioNeighbour& nb : r.Def().neighbours) beside = beside || (nb.id == "regiment_reserve_battery" && nb.fights);
    SC_EXPECT(beside);  // the reserve fights at the position from now
    SC_EXPECT(!s.Regiment()->ReliefPending());
    // Rested and repaired: the runner's part.
    r.RestAndRepair(6);
    for (const BatteryFire::Shooter& sh : r.GetBatteryFire()->Shooters()) {
        if (!sh.external) SC_EXPECT(sh.missiles >= 6);
    }
}

SC_TEST("redeploy (O7): the player's Osa detached: told on the net, it drives on its own, its commander the senior man there") {
    campaign::MissionSessionDef md;
    md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    md.osa_scenario = kData + "/scenarios/osa_mission_cover.json";
    md.osa_callsign = "21";
    md.tasking = "campaign/tasking.json";
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(md, kData, &e));
    if (s.Osa() == nullptr || s.Regiment() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    ScenarioRunner& r = s.Runner();
    while (!r.Finished() && r.TaskReadyS() < 0.0) s.Step(0.2);
    campaign::TaskingRule rule = *RuleOf(Rules(), "detach_for_object");
    rule.vehicle = "21";
    const campaign::BatteryTask t = campaign::DetachOneTask(r.Task(), rule, "kesmark_station", r.PlaceNameOf("kesmark_station"), "test");
    std::string why;
    SC_EXPECT(r.GiveTask(t, "radio", &why));
    const double given = r.Time();
    while (!r.Finished() && r.Time() < given + 2.0 * 3600.0 && !(r.GetBattery()->Find("21")->detached && r.GetBattery()->Find("21")->deployed)) s.Step(0.2);
    const Battery::Vehicle* v = r.GetBattery()->Find("21");
    SC_EXPECT(v != nullptr && v->detached && v->deployed);
    SC_EXPECT(s.Osa()->CommanderFireMode() == "independent");
    bool told = false;
    for (const campaign::SceneLine& l : s.Osa()->SceneLines()) told = told || l.text.find("senior man") != std::string::npos;
    if (!told) {
        for (const std::string& n : s.Notes()) std::printf("    %s\n", n.c_str());
        for (const campaign::SceneLine& l : s.Osa()->SceneLines()) std::printf("    osa line: %s\n", l.text.c_str());
    }
    SC_EXPECT(told);
    // The Osa stands where it went.
    const nav::Anchor* a = r.Nav()->GetAnchors().Find("kesmark_station");
    if (a != nullptr) SC_EXPECT(std::hypot(s.Osa()->UnitPosition().x - a->at.east_m, s.Osa()->UnitPosition().y - a->at.north_m) < 600.0);
}

SC_TEST("redeploy (O7): a loan begun before a save ends after the resume, and the battery gets the task it held before") {
    campaign::CampaignsData cd;
    std::string e;
    SC_EXPECT(campaign::LoadCampaignsData(kData, "campaign/campaigns.json", cd, &e));
    campaign::MissionSessionDef md;
    campaign::CampaignSave header;
    SC_EXPECT(campaign::NewCampaign(cd, "rebels", kData, "battery_commander", md, header, &e));
    campaign::MissionSession a;
    SC_EXPECT(a.Start(md, kData, &e));
    if (a.Campaign() == nullptr || a.Regiment() == nullptr || a.Runner().Nav() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    ScenarioRunner& r = a.Runner();
    while (!r.Finished() && r.TaskReceivedS() < 0.0 && r.Time() < 3600.0) a.Step(0.2);
    SC_EXPECT(r.HasTask());
    if (!r.HasTask()) return;
    const std::string held = r.Task().id;
    // An object away from the battery struck twice, the warning net's reports reaching the regiment in the session (the
    // picture taken an hour on, once its staff has worked on them).
    const nav::Anchors& anchors = r.Nav()->GetAnchors();
    const nav::Anchor* main = anchors.Find(r.Task().main);
    const nav::Anchor* object = nullptr;
    for (const char* id : {"kesmark_station", "kesmark_sugar", "div_hq", "tech_battery", "vrbice_crossroads"}) {
        const nav::Anchor* x = anchors.Find(id);
        if (x != nullptr && main != nullptr && std::hypot(x->at.east_m - main->at.east_m, x->at.north_m - main->at.north_m) > 5000.0) object = x;
    }
    SC_EXPECT(object != nullptr);
    if (object == nullptr) return;
    campaign::RegimentPicture p = a.RegimentHolds();
    const double now_h = p.now_h;
    p.now_h = now_h + 1.0;
    for (int i = 0; i < 2; ++i) p.raids.push_back(Raid(now_h + 0.2 + 0.1 * i, Vec3{object->at.east_m, object->at.north_m, 0.0}, "test_object"));
    SC_EXPECT(a.RegimentDecides(p, true));
    const std::string loan = a.Regiment()->TemporaryTask();
    const double until_h = a.Regiment()->TemporaryUntilH();
    SC_EXPECT(!loan.empty() && loan != held);
    for (double t0 = r.Time(); r.Task().id != loan && r.Time() < t0 + 600.0;) a.Step(0.2);
    SC_EXPECT(r.Task().id == loan && !r.Task().detached.empty());
    // Saved, written and read back: the regiment's memory with it.
    campaign::MissionSession::Seam seam;
    seam.kind = "briefing";
    seam.air_war = airwar::SaveCampaignJson(*a.Campaign(), a.TheatreFile(), a.FactionFile());
    seam.battery = a.BatteryNow();
    campaign::CampaignSave loaded;
    SC_EXPECT(campaign::LoadCampaignSaveJson(campaign::CampaignSaveJson(campaign::SaveOfSeam(header, a, seam)), loaded, &e));
    const campaign::RegimentMemory& m = loaded.battery.regiment;
    SC_EXPECT(m.present && m.temp_id == loan && m.has_before && m.before.id == held);
    SC_EXPECT_NEAR(m.temp_until_h, until_h, 1e-6);
    SC_EXPECT(!m.last_h.empty());
    // Resumed: the regiment still has the loan, the battery its task; at the loan's end the task held before comes back.
    campaign::MissionSession b;
    SC_EXPECT(b.Start(campaign::ResumeCampaign(loaded, kData), kData, &e));
    if (b.Campaign() == nullptr || b.Regiment() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    for (int i = 0; i < 20; ++i) b.Step(0.1);
    SC_EXPECT(b.Regiment()->TemporaryTask() == loan);
    SC_EXPECT_NEAR(b.Regiment()->TemporaryUntilH(), until_h, 1e-6);
    SC_EXPECT(b.Runner().HasTask() && b.Runner().Task().id == loan);
    campaign::RegimentPicture q = b.RegimentHolds();
    SC_EXPECT(!b.RegimentDecides(q, true));  // not yet
    q.now_h = until_h + 0.1;
    SC_EXPECT(b.RegimentDecides(q, true));
    for (double t0 = b.Runner().Time(); b.Runner().Task().id != held && b.Runner().Time() < t0 + 600.0;) b.Step(0.2);
    SC_EXPECT(b.Runner().Task().id == held && b.Runner().Task().detached.empty());
    SC_EXPECT(b.Regiment()->TemporaryTask().empty());
    // A relief under way is carried the same way.
    campaign::RegimentMemory relief;
    relief.present = true;
    relief.relief_pending = true;
    relief.in_line_h = 12.0;
    relief.relief_stage = "resting";
    relief.relief_unit = "the 25th Battery";
    relief.relief_from = "kesmark_barracks";
    relief.relief_rest_h = 12.0;
    relief.relief_task = m.before;
    relief.rest_until_h = 40.5;
    const json::Value v = campaign::save_json::RegimentMemoryJson(relief);
    campaign::RegimentMemory back;
    const bool read = campaign::save_json::ReadRegimentMemory(v, back, &e);
    if (!read) std::printf("    %s%c", e.c_str(), 10);
    SC_EXPECT(read);
    SC_EXPECT(back.relief_stage == "resting" && back.relief_pending && back.relief_unit == relief.relief_unit && back.relief_task.id == held);
    SC_EXPECT_NEAR(back.rest_until_h, 40.5, 1e-9);
    SC_EXPECT_NEAR(back.in_line_h, 12.0, 1e-9);
}
