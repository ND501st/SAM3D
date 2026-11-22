// The director (roadmap 15.4, first form): simcore/campaign/director.hpp.
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/campaign/director.hpp"
#include "simcore/campaign/mission_session.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

}  // namespace

SC_TEST("director (15.4): the quiet stretches between wakes, by hand") {
    // Wakes at 100, 400 and 1000 s of a 4600 s session: stretches of 100, 300, 600 and 3600 s.
    const campaign::QuietStretches q = campaign::MeasureQuiet({400.0, 100.0, 1000.0}, 4600.0, 600.0);
    SC_EXPECT_NEAR(q.longest_s, 3600.0, 1e-9);
    SC_EXPECT_NEAR(q.share, (600.0 + 3600.0) / 4600.0, 1e-12);
    const campaign::QuietStretches none = campaign::MeasureQuiet({}, 3000.0, 600.0);
    SC_EXPECT_NEAR(none.longest_s, 3000.0, 1e-9);
    SC_EXPECT_NEAR(none.share, 1.0, 1e-12);
    SC_EXPECT_NEAR(campaign::MeasureQuiet({10.0}, 0.0, 1.0).share, 0.0, 1e-12);
}

SC_TEST("director (15.4): at the post the battery commander is woken by what reaches his seat: the words he hears, the marks on the board") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_battery.json", kData, def, console, &e));
    ScenarioRunner r(std::move(def), std::move(console));
    campaign::Director d(r.Cmdr());
    while (r.Time() < 600.0 && !r.Finished()) {
        r.Step(0.1);
        d.Step(r);
    }
    for (std::size_t i = 0; i < d.Wakes().size() && i < 6; ++i) {
        std::printf("    %.1f s %s: %s\n", d.Wakes()[i].t_s, d.Wakes()[i].why.c_str(), d.Wakes()[i].detail.c_str());
    }
    std::printf("    %s", d.Text(r.Time()).c_str());
    SC_EXPECT(d.Count("board") > 0);
    SC_EXPECT(d.Count("heard") > 0);
    // Every word that woke him reached his seat and is no crew callout: after the oldest message the runner still keeps,
    // exactly those.
    const double kept_from_s = r.RadioLog().front().time_s;
    int heard = 0, woke = 0;
    for (const ScenarioRunner::RadioMessage& m : r.RadioLog()) heard += m.time_s > kept_from_s && !m.callout && m.HeardAt(r.Cmdr()) ? 1 : 0;
    for (const campaign::Wake& w : d.Wakes()) woke += w.why == "heard" && w.t_s > kept_from_s ? 1 : 0;
    SC_EXPECT(woke == heard);
    for (const campaign::Wake& w : d.Wakes()) {
        if (w.why == "board") SC_EXPECT(w.detail.find("mark ") == 0);
    }
}

SC_TEST("director (15.4): in the Osa the operator is woken by the target when the senior operator tells him of it") {
    ScenarioDef def;
    ConsoleData console;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_crew_practice.json", kData, def, console));
    ScenarioRunner r(std::move(def), std::move(console));
    campaign::Director d("operator");
    double told_s = -1.0;
    while (!r.Finished() && r.Time() < 460.0 && told_s < 0.0) {
        r.Step(0.1);
        d.Step(r);
        const CrewPicture* p = r.PictureOf("operator");
        if (p != nullptr && p->Find("t1") != nullptr) told_s = r.Time();
    }
    const campaign::Wake* contact = nullptr;
    for (const campaign::Wake& w : d.Wakes()) {
        if (w.why == "contact" && contact == nullptr) contact = &w;
    }
    std::printf("    told at %.1f s; woken: %s\n", told_s, contact != nullptr ? contact->detail.c_str() : "never");
    SC_EXPECT(told_s > 0.0);
    SC_EXPECT(contact != nullptr && contact->t_s == told_s && contact->detail.find("told by Senior operator") != std::string::npos);
    SC_EXPECT(d.Count("contact") == 1);
}

SC_TEST("director (15.4): the enemy's package enters the session unseen; the battery commander wakes when its first mark is on his board, not before") {
    campaign::MissionSessionDef md;
    md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    md.faction = "warsaw_pact";
    md.seed = 1;
    md.day = 1;
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(md, kData, &e));
    if (s.Campaign() == nullptr) return;
    campaign::Director d(s.Runner().Cmdr());
    while (!s.Runner().Finished()) {
        s.Step(0.1);
        d.Step(s.Runner());
    }
    const ScenarioRunner& r = s.Runner();
    std::printf("    %s", d.Text(r.Time()).c_str());
    SC_EXPECT(!s.Packages()->PlayedPackages().empty());
    if (s.Packages()->PlayedPackages().empty()) return;
    const double in_s = s.Packages()->PlayedPackages().front().in_s;
    double first_board_s = -1.0, first_contact_s = -1.0;
    for (const campaign::Wake& w : d.Wakes()) {
        if (w.why == "board" && first_board_s < 0.0) first_board_s = w.t_s;
        if (w.why == "contact" && first_contact_s < 0.0) first_contact_s = w.t_s;
    }
    std::printf("    the package in at %.0f s; the first mark woke him at %.0f s, the first contact at %.0f s\n", in_s, first_board_s,
                first_contact_s);
    SC_EXPECT(first_board_s > in_s);  // its aircraft were in the session before anyone at the post saw them
    // Between the order's words and the package the session was quiet enough to compress for a while.
    SC_EXPECT(d.Quiet(r.Time(), campaign::Director::kCompressS).longest_s >= campaign::Director::kCompressS);
    // The runner kept only its last messages: the director still heard the newest of them.
    std::printf("    %zu messages logged, %zu kept\n", r.RadioTotal(), r.RadioLog().size());
    SC_EXPECT(r.RadioTotal() > r.RadioLog().size());
    int kept_heard = 0, woke_since = 0;
    for (const ScenarioRunner::RadioMessage& m : r.RadioLog()) kept_heard += !m.callout && m.HeardAt(r.Cmdr()) ? 1 : 0;
    for (const campaign::Wake& w : d.Wakes()) woke_since += w.why == "heard" && w.t_s >= r.RadioLog().front().time_s ? 1 : 0;
    SC_EXPECT(kept_heard > 0);
    SC_EXPECT(woke_since >= kept_heard);
}
