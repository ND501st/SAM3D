// The crew judges a shot's result from its instruments' signs (simcore/kill_assessment.hpp, voenizdat_osa_1991 p267), and
// a target in a group the battery post can't resolve is designated by the group's centre and a selection rule
// (simcore/group_designation.hpp, voenizdat_osa_1991 p261). Model numbers here are the data's game estimates or
// test values; positions are placeholders.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/battery_link.hpp"
#include "simcore/group_designation.hpp"
#include "simcore/kill_assessment.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

struct Seen {
    KillSign sign;
    KillInstrument where;
    double t;
};

// Runs one assessment from t = 0 with a fixed look at the target, the judge being the elevation wheel's seat (he sees the
// elevation dial, the TV and the launch lamps himself, hears the rest 2 s later). Returns the call; the signs in `seen`.
KillAssessment::Judgement Run(KillAssessment& k, bool hit, const std::string& detail, const MeetingView& v,
                              KillLook (*look)(double), double wrong_chance, std::vector<Seen>& seen) {
    k.Open(0.0, 1, "t1", hit, detail, v);
    std::vector<std::pair<double, KillSign>> heard;  // said by another seat: heard 2 s later
    for (int i = 0; i <= 120; ++i) {
        const double t = i * 0.1;
        k.Observe(t, [&](const std::string&) { return look(t); });
        for (const KillAssessment::Sign& s : k.TakeSigns()) {
            seen.push_back({s.sign, s.where, s.t});
            const bool own = s.where == KillInstrument::ElevationDial || s.where == KillInstrument::Tv || s.where == KillInstrument::LaunchLamps;
            if (own) {
                k.Learn(s.shot, s.sign, t);
            } else {
                heard.push_back({t + 2.0, s.sign});
            }
        }
        for (auto h = heard.begin(); h != heard.end();) {
            if (h->first > t + 1e-9) {
                ++h;
                continue;
            }
            k.LearnHeard(h->second, t);
            h = heard.erase(h);
        }
        const std::vector<KillAssessment::Judgement> j = k.Judge(t, wrong_chance);
        if (!j.empty()) return j.front();
    }
    return KillAssessment::Judgement{};
}

const Seen* Find(const std::vector<Seen>& seen, KillSign s) {
    for (const Seen& x : seen) {
        if (x.sign == s) return &x;
    }
    return nullptr;
}

KillLook Gone(double) { return KillLook{}; }
KillLook FliesOnRadar(double) {
    KillLook l;
    l.on_range = l.tracked = l.on_scope = true;
    l.elevation_deg = 5.0;
    return l;
}
KillLook Dives(double t) {
    KillLook l = FliesOnRadar(t);
    l.elevation_deg = 5.0 - 0.6 * t;  // 0.6 deg/s down: a dive away after the near miss
    return l;
}

KillAssessmentTuning Sure() {  // the wreck's fall and fire always shown, so the draws don't decide the test
    KillAssessmentTuning t;
    t.tv_fire_chance = 1.0;
    t.elevation_drop_chance = 1.0;
    return t;
}

}  // namespace

// ------------------------------------------------------------------------------------------ the signs and the call

SC_TEST("kill assessment: a hit tracked by radar in daylight: the burst, fire on the TV, the mark gone; settled at once") {
    KillAssessment k;
    k.Configure(Sure(), 1u);
    MeetingView v;
    v.radar_range = v.antenna_on = v.on_tv = v.on_scope = true;
    v.elevation_deg = 5.0;
    std::vector<Seen> seen;
    const KillAssessment::Judgement j = Run(k, true, "", v, Gone, 0.0, seen);
    const Seen* burst = Find(seen, KillSign::Burst);
    const Seen* fire = Find(seen, KillSign::TvFire);
    const Seen* gone = Find(seen, KillSign::MarkGone);
    const Seen* fall = Find(seen, KillSign::ElevationDrop);
    SC_EXPECT(burst && burst->where == KillInstrument::RangeIndicator);  // the operator's range indicator
    SC_EXPECT(fire && fire->where == KillInstrument::Tv);
    SC_EXPECT(gone && gone->where == KillInstrument::RangeIndicator);   // read from the instruments: the echo left them
    SC_EXPECT(fall && fall->where == KillInstrument::ElevationDial);
    SC_EXPECT(Find(seen, KillSign::FliesOn) == nullptr && Find(seen, KillSign::TvWhole) == nullptr);
    if (burst && fire && gone) {
        SC_EXPECT_NEAR(burst->t, 0.3, 0.05);
        SC_EXPECT_NEAR(fire->t, 1.0, 0.05);
        SC_EXPECT_NEAR(gone->t, 1.0, 0.05);
    }
    // The TV settles it, but not before the judge's 2 s.
    SC_EXPECT(j.call == KillAssessment::Call::Hit && j.settled && !j.wrong);
    SC_EXPECT_NEAR(j.meet_s + Sure().judge_min_s, 2.0, 1e-9);
    SC_EXPECT(std::find(j.known.begin(), j.known.end(), KillSign::TvFire) != j.known.end());
    SC_EXPECT(k.OpenCount() == 0 && !k.Pending("t1"));
}

SC_TEST("kill assessment: a missile out of reach: its reply still there after the meeting, the target flying on: a miss") {
    KillAssessment k;
    k.Configure(Sure(), 2u);
    MeetingView v;
    v.radar_range = v.antenna_on = v.on_scope = true;
    v.elevation_deg = 5.0;
    std::vector<Seen> seen;
    const KillAssessment::Judgement j = Run(k, false, "out of reach", v, FliesOnRadar, 0.0, seen);
    const Seen* reply = Find(seen, KillSign::ReplyAfter);
    SC_EXPECT(reply && reply->where == KillInstrument::RangeIndicator && std::fabs(reply->t - 0.5) < 0.05);
    SC_EXPECT(Find(seen, KillSign::Burst) == nullptr && Find(seen, KillSign::MarkGone) == nullptr);
    // Heard from the operator at 2.5 s: the reply alone settles it.
    SC_EXPECT(j.call == KillAssessment::Call::Miss && j.settled && !j.wrong);
}

SC_TEST("kill assessment: a near miss at night: the burst and the target flying on settle nothing; the call can go wrong") {
    KillAssessmentTuning t = Sure();
    t.near_burst_chance = 1.0;
    t.early_burst_chance = 0.0;
    MeetingView v;  // no TV: night
    v.radar_range = v.antenna_on = v.on_scope = true;
    v.elevation_deg = 5.0;
    for (double wrong : {0.0, 1.0}) {
        KillAssessment k;
        k.Configure(t, 3u);
        std::vector<Seen> seen;
        const KillAssessment::Judgement j = Run(k, false, "", v, FliesOnRadar, wrong, seen);
        SC_EXPECT(Find(seen, KillSign::Burst) != nullptr);  // the fuse fired beside it, as for a hit
        const Seen* on = Find(seen, KillSign::FliesOn);
        SC_EXPECT(on && on->where == KillInstrument::RangeIndicator && on->t >= t.watch_s - 1e-9);
        SC_EXPECT(!j.settled);
        SC_EXPECT_NEAR(j.score, 1.0 - 2.0, 1e-9);
        SC_EXPECT_NEAR(j.meet_s + t.judge_max_s, 8.0, 1e-9);  // called with what he has when his time is up
        if (wrong == 0.0) {
            SC_EXPECT(j.call == KillAssessment::Call::Miss && !j.wrong);
        } else {
            SC_EXPECT(j.call == KillAssessment::Call::Hit && j.wrong);
        }
    }
}

SC_TEST("kill assessment: a target diving away after a near miss falls on the elevation dial: it looks destroyed") {
    KillAssessmentTuning t = Sure();
    t.near_burst_chance = 1.0;
    t.early_burst_chance = 0.0;
    KillAssessment k;
    k.Configure(t, 4u);
    MeetingView v;
    v.radar_range = v.antenna_on = v.on_scope = true;
    v.elevation_deg = 5.0;
    std::vector<Seen> seen;
    const KillAssessment::Judgement j = Run(k, false, "", v, Dives, 0.0, seen);
    const Seen* fall = Find(seen, KillSign::ElevationDrop);
    SC_EXPECT(fall && fall->where == KillInstrument::ElevationDial);
    SC_EXPECT(Find(seen, KillSign::FliesOn) == nullptr);  // its motion changed
    SC_EXPECT(j.call == KillAssessment::Call::Hit && j.wrong);
}

SC_TEST("kill assessment: a jammer in a group with chaff, off the range indicator: its signs on the search scope") {
    KillAssessment k;
    KillAssessmentTuning waits = Sure();
    waits.decisive = 1e9;  // the judge waits out his time, so the chaff's sign (4 s) shows before the call
    k.Configure(waits, 5u);
    MeetingView v;  // tracked by the TV, the radar silent; on the search scope with a strobe, a wingman and chaff beside it
    v.antenna_on = v.on_scope = v.strobe = v.group = v.chaff = true;
    std::vector<Seen> seen;
    Run(k, true, "", v, Gone, 0.0, seen);
    for (KillSign s : {KillSign::JammingStops, KillSign::GroupSplit, KillSign::ChaffStops, KillSign::MarkGone}) {
        const Seen* x = Find(seen, s);
        SC_EXPECT(x && x->where == KillInstrument::SearchScope);
    }
    const Seen* gone = Find(seen, KillSign::MarkGone);
    SC_EXPECT(gone && gone->t >= Sure().scope_gone_s - 1e-9);  // a turn of the antenna
    const Seen* burst = Find(seen, KillSign::Burst);
    SC_EXPECT(burst && burst->where == KillInstrument::LaunchLamps);  // no range channel: the ЗАХВАТ lamp goes out
    // A track lost before the meeting: the reply goes at once, on the launch lamps.
    KillAssessment e;
    e.Configure(Sure(), 6u);
    std::vector<Seen> early;
    const KillAssessment::Judgement j = Run(e, false, "track lost", MeetingView{}, Gone, 0.0, early);
    SC_EXPECT(!early.empty() && early.front().sign == KillSign::ReplyEarly && early.front().where == KillInstrument::LaunchLamps &&
              early.front().t < 1e-9);
    SC_EXPECT(j.call == KillAssessment::Call::Miss && j.settled);
}

SC_TEST("kill assessment: nothing seen: 'not observed'; the data's numbers load and a sign weighing the wrong way is refused") {
    KillAssessment k;
    k.Configure(Sure(), 7u);
    k.Open(0.0, 3, "t9", true, "", MeetingView{});
    std::vector<KillAssessment::Judgement> j;
    for (int i = 0; i <= 100 && j.empty(); ++i) j = k.Judge(i * 0.1, 0.0);  // the judge learns nothing
    SC_EXPECT(j.size() == 1 && j[0].call == KillAssessment::Call::Unknown && !j[0].wrong && j[0].known.empty());
    EngagementTuning tuning;
    std::string error;
    SC_EXPECT(LoadEngagementTuningFile(kData + "/gameplay/osa_ak.json", tuning, &error));
    SC_EXPECT(tuning.kill.judge_role == "deputy_commander" && tuning.kill.Weight(KillSign::TvFire) == 3.0 &&
              tuning.kill.Delay(KillSign::Burst) == 0.3 && tuning.kill.Weight(KillSign::ReplyAfter) == -3.0);
    SC_EXPECT(tuning.group.same_azimuth_deg == 1.3 && tuning.group.post_unresolved_m == 3000.0);
    std::ifstream f(kData + "/gameplay/osa_ak.json", std::ios::binary);
    std::stringstream text;
    text << f.rdbuf();
    std::string bad = text.str();
    const std::size_t at = bad.find("\"tv_whole\": -3");
    SC_EXPECT(at != std::string::npos);
    if (at == std::string::npos) return;
    bad.replace(at, 14, "\"tv_whole\": 3");
    EngagementTuning t2;
    SC_EXPECT(!LoadEngagementTuning(bad, t2, &error));
    SC_EXPECT(error.find("kill_assessment.weight.tv_whole") != std::string::npos);
}

// ------------------------------------------------------------------------------------------ the selection rules

SC_TEST("group rules: abreast, counted from the left as the crew looks; on one azimuth by range; past the group's size the last") {
    GroupTuning t;
    // Looking north at 20 km: three aircraft abreast, 2 degrees apart (about 700 m), the left one west of north.
    const std::vector<GroupEcho> abreast = {{"mid", 0.0, 20000.0}, {"right", 2.0, 20000.0}, {"left", 358.0, 20000.0}};
    SC_EXPECT(SelectByRule(abreast, 0.0, 20000.0, 1, t) == "left");
    SC_EXPECT(SelectByRule(abreast, 0.0, 20000.0, 2, t) == "mid");
    SC_EXPECT(SelectByRule(abreast, 0.0, 20000.0, 3, t) == "right");
    SC_EXPECT(SelectByRule(abreast, 0.0, 20000.0, 4, t) == "right");
    SC_EXPECT(SelectByRule(abreast, 0.0, 20000.0, 0, t) == "left");  // none given: the crew chief takes the first
    // A column on one azimuth (east): by range.
    const std::vector<GroupEcho> column = {{"c3", 90.0, 21600.0}, {"c1", 90.0, 20000.0}, {"c2", 90.3, 20800.0}};
    SC_EXPECT(SelectByRule(column, 90.0, 20800.0, 1, t) == "c1");
    SC_EXPECT(SelectByRule(column, 90.0, 20800.0, 2, t) == "c2");
    SC_EXPECT(SelectByRule(column, 90.0, 20800.0, 3, t) == "c3");
    // The group nearest the centre: not a pair 10 km off, nor anything beyond search_m.
    std::vector<GroupEcho> two = abreast;
    two.push_back({"far1", 30.0, 20000.0});
    two.push_back({"far2", 31.5, 20000.0});
    SC_EXPECT(GroupNear(two, 0.0, 21000.0, t).size() == 3);
    SC_EXPECT(SelectByRule(two, 30.5, 20000.0, 1, t) == "far1");
    SC_EXPECT(SelectByRule(two, 90.0, 20000.0, 1, t).empty());
}

SC_TEST("group rules: the post's picture: marks linked closer than it resolves are one group") {
    const std::vector<Vec3> marks = {{0.0, 30000.0, 1000.0}, {800.0, 30000.0, 1000.0}, {1600.0, 30200.0, 1000.0}, {9000.0, 30000.0, 1000.0}};
    const std::vector<std::size_t> g = UnresolvedGroup(marks, 0, 3000.0);
    SC_EXPECT(g.size() == 3 && g[0] == 0);
    SC_EXPECT(UnresolvedGroup(marks, 3, 3000.0).size() == 1);
    SC_EXPECT(RuleWordsRu(2) == "правило 2" && RuleWordsEn(3) == "rule 3");
}

// ------------------------------------------------------------------------------------------ the crew at work

SC_TEST("kill assessment: in the practice shoot each sign is called by the seat whose instrument shows it, then the result") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice_em.json", kData, def, console, &e));
    ScenarioRunner r(std::move(def), std::move(console));
    r.RunToEnd(0.1, 700.0);
    // Who may call each sign (the tuning's aim roles: the range wheel's indicator, the elevation wheel's dial, TV and
    // lamps, the azimuth wheel's scope).
    const std::map<std::string, std::set<std::string>> seats = {
        {"burst", {"operator", "deputy_commander"}}, {"reply_early", {"operator", "deputy_commander"}},
        {"reply_after", {"operator", "deputy_commander"}}, {"mark_gone", {"operator", "senior_operator"}},
        {"flies_on", {"operator", "deputy_commander", "senior_operator"}}, {"jamming_stops", {"operator", "senior_operator"}},
        {"elevation_drop", {"deputy_commander"}}, {"tv_fire", {"deputy_commander"}}, {"tv_whole", {"deputy_commander"}},
        {"group_split", {"senior_operator"}}, {"chaff_stops", {"senior_operator"}}};
    int signs = 0, calls = 0;
    double first_sign = -1.0, first_call = -1.0;
    for (const SpokenCallout& s : r.Callouts()->Log()) {
        KillSign k = KillSign::Burst;
        if (KillSignOfCallout(s.id, k)) {
            ++signs;
            if (first_sign < 0.0) first_sign = s.start_s;
            const auto it = seats.find(KillSignId(k));
            SC_EXPECT(it != seats.end() && it->second.count(s.from) == 1);
        }
        if (s.id == "hit" || s.id == "miss" || s.id == "result_unknown") {
            ++calls;
            if (first_call < 0.0) first_call = s.start_s;
            SC_EXPECT(s.from == "deputy_commander");
        }
    }
    std::printf("    %d results judged (%d wrong), %d signs called, %d calls said\n", r.KillCalls(), r.KillCallsWrong(), signs, calls);
    SC_EXPECT(r.KillCalls() >= 1 && calls == r.KillCalls() && signs >= 1);
    SC_EXPECT(first_sign >= 0.0 && first_call > first_sign);
    SC_EXPECT(r.Debrief().find("judges missile 1") != std::string::npos);
}

SC_TEST("kill assessment: a wrong call of a hit: the track let go, then the senior operator sees it flying on") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice_em.json", kData, def, console, &e));
    for (auto& h : def.tuning.hit_chance) h.second = 0.0;  // every missile misses
    def.tuning.kill.near_burst_chance = 1.0;                // beside the target: the burst, as for a hit
    def.tuning.kill.early_burst_chance = 0.0;
    def.tuning.kill.decisive = 100.0;                       // nothing settles it
    def.tuning.kill.ambiguous_wrong_chance = 1.0;           // and he reads it the wrong way
    ScenarioRunner r(std::move(def), std::move(console));
    bool hit_said = false, dropped = false, flying = false;
    for (int i = 0; i < 7000 && !flying; ++i) {
        r.Step(0.1);
        for (const SpokenCallout& s : r.Callouts()->Log()) {
            hit_said = hit_said || s.id == "hit";
            dropped = dropped || (hit_said && s.id == "order_drop_target");
            flying = flying || s.id == "target_still_flying";
        }
    }
    std::printf("    %d results judged, %d wrong; 'still flying' %d\n", r.KillCalls(), r.KillCallsWrong(), r.StillFlyingCalls());
    SC_EXPECT(hit_said && r.KillCallsWrong() >= 1);
    SC_EXPECT(flying && r.StillFlyingCalls() >= 1);
    SC_EXPECT(r.Debrief().find("WRONG: it was missed") != std::string::npos);
    (void)dropped;
}

SC_TEST("kill assessment: a wrong kill reported on the battery net is taken back there once the target is seen flying on") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_battery_practice.json", kData, def, console, &e));
    for (auto& h : def.tuning.hit_chance) h.second = 0.0;  // every missile misses
    def.tuning.kill.near_burst_chance = 1.0;
    def.tuning.kill.early_burst_chance = 0.0;
    def.tuning.kill.decisive = 100.0;
    def.tuning.kill.ambiguous_wrong_chance = 1.0;  // and the deputy reads it the wrong way
    ScenarioRunner r(std::move(def), std::move(console));
    double reported_s = -1.0, flying_s = -1.0, taken_back_s = -1.0;
    for (int i = 0; i < 9000 && taken_back_s < 0.0; ++i) {
        r.Step(0.1);
        for (const SpokenCallout& s : r.Callouts()->Log()) {
            if (s.id == "report_result" && s.from == "commander" && reported_s < 0.0) reported_s = s.start_s;
            if (s.id == "target_still_flying" && flying_s < 0.0) flying_s = s.start_s;
            if (s.id == "report_not_destroyed" && taken_back_s < 0.0) {
                taken_back_s = s.start_s;
                SC_EXPECT(s.from == "commander" && s.channel != "intercom");
                SC_EXPECT(s.ru.find("не поражена") != std::string::npos);
            }
        }
    }
    std::printf("    reported %.0f s, seen flying %.0f s, taken back %.0f s (%d)\n", reported_s, flying_s, taken_back_s,
                r.KillReportsTakenBack());
    SC_EXPECT(reported_s > 0.0 && flying_s > reported_s && taken_back_s >= flying_s);
    SC_EXPECT(r.KillReportsTakenBack() == 1);
    SC_EXPECT(r.Debrief().find("kill report taken back") != std::string::npos);
}

SC_TEST("kill assessment: the post hears a launcher take its kill back: the target is back on its board") {
    std::string error;
    std::unique_ptr<BatteryLink> link = LoadBatteryLinkFile(kData + "/scenarios/battery_link_practice.json", kData, &error);
    SC_EXPECT(link != nullptr);
    if (!link) return;
    for (int i = 0; i < 50; ++i) link->Step(0.1);
    ScenarioRunner::LinkReport r;
    r.kind = BatteryFire::Event::Kind::Hit;
    r.callsign = link->Callsign();
    r.target = "t1";
    r.missiles = 3;
    link->Post().HearBatteryReport(r);
    SC_EXPECT(link->Post().BelievesDestroyed("t1"));
    r.kind = BatteryFire::Event::Kind::Miss;
    r.taken_back = true;
    link->Post().HearBatteryReport(r);
    SC_EXPECT(!link->Post().BelievesDestroyed("t1"));
    SC_EXPECT(link->Post().Debrief().find("takes back the kill of t1") != std::string::npos);
}

namespace {

// Four hostiles abreast, 800 m apart, from the north: the strike aircraft of the scenario copied four times.
void Formation(std::vector<ScenarioTarget>& targets, const std::string& like, double east_m, int n) {
    ScenarioTarget base;
    for (const ScenarioTarget& t : targets) {
        if (t.id == like) base = t;
    }
    std::vector<ScenarioTarget> out;
    for (const ScenarioTarget& t : targets) {
        if (t.friendly) out.push_back(t);  // the friend stays
    }
    for (int i = 0; i < n; ++i) {
        ScenarioTarget t = base;
        t.id = "g" + std::to_string(i + 1);
        t.formation = n;
        const double dx = east_m + (i - (n - 1) / 2.0) * 800.0 - base.start.x;
        t.start.x += dx;
        for (Vec3& w : t.waypoints) w.x += dx;
        out.push_back(t);
    }
    targets = out;
}

}  // namespace

SC_TEST("group designation: the NPC battery commander gives a group he can't resolve by its centre and rules 1, 2, 3, 4") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_battery.json", kData, def, console, &e));
    Formation(def.targets, "t1", 3000.0, 4);
    ScenarioRunner r(std::move(def), std::move(console));
    while (r.Time() < 400.0) r.Step(0.1);
    // The first four orders: the group's centre, each with its own rule (later ones may name a lone survivor).
    std::set<std::string> rules;
    int groups = 0, orders = 0;
    for (const SpokenCallout& s : r.Callouts()->Log()) {
        if (s.id != "order_assign" || ++orders > 4) continue;
        const std::string rule = "правило ";  // 15 bytes in UTF-8
        const std::size_t at = s.ru.find(rule);
        if (s.ru.find("группа") == std::string::npos || s.ru.find("центр") == std::string::npos || at == std::string::npos) continue;
        ++groups;
        rules.insert(s.ru.substr(at, s.ru.find_first_of(" ,.;", at + rule.size()) - at));
        SC_EXPECT(s.en.find("group ") != std::string::npos && s.en.find(", rule ") != std::string::npos);
    }
    const std::string d = r.Debrief();
    std::printf("    %d group designations, %d rules\n", groups, static_cast<int>(rules.size()));
    SC_EXPECT(groups == 4 && rules.size() == 4);  // a launcher per aircraft, each with its own rule
    SC_EXPECT(d.find("in the group by rule 1") != std::string::npos && d.find("its centre, rule 4") != std::string::npos);
}

SC_TEST("group designation: the player's Osa hears a group's centre and a rule; its senior operator counts it, the commander takes it") {
    ScenarioDef osa, post;
    ConsoleData osa_console, post_console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/link_osa_21.json", kData, osa, osa_console, &e));
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/link_pu12m.json", kData, post, post_console, &e));
    Formation(osa.targets, "t1", -2500.0, 3);
    BatteryLink link(std::move(osa), std::move(osa_console), std::move(post), std::move(post_console), "21");
    std::string picked, taken;
    bool passed_on = false;
    for (int i = 0; i < 4000 && taken.empty(); ++i) {
        link.Step(0.1);
        for (const SpokenCallout& s : link.Osa().Callouts()->Log()) {
            passed_on = passed_on || (s.id == "designation_group" && s.from == "commander");
            if (s.id == "group_pick" && s.from == "senior_operator" && s.has_about) picked = s.about.target;
        }
        if (!picked.empty() && link.Osa().BatteryTarget() == picked) taken = picked;
    }
    std::printf("    the senior operator picked %s; the crew took %s\n", picked.c_str(), taken.c_str());
    SC_EXPECT(passed_on);
    SC_EXPECT(!picked.empty() && picked[0] == 'g');
    SC_EXPECT(taken == picked);
    SC_EXPECT(link.Osa().Debrief().find("counts the group") != std::string::npos);
}
