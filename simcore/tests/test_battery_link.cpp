// The player's Osa and the PU-12M in one battery (simcore/battery_link.hpp, docs/PU12_DEVELOPMENT.md P8): one sky for
// both runners, the post's designations reaching the Osa's commander as the PU-12's, the Osa's reports on the post's IT
// boards, kills crossing both ways. The scenario is data/scenarios/battery_link_practice.json (placeholder raid).
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <string>

#include "simcore/battery_link.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::unique_ptr<BatteryLink> LoadLink() {
    std::string error;
    std::unique_ptr<BatteryLink> link = LoadBatteryLinkFile(kData + "/scenarios/battery_link_practice.json", kData, &error);
    if (!link) std::printf("    %s\n", error.c_str());
    return link;
}

int Count(const std::string& text, const std::string& what) {
    int n = 0;
    for (std::size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + what.size())) ++n;
    return n;
}

}  // namespace

SC_TEST("battery link: one sky; the post's targets follow the Osa's") {
    std::unique_ptr<BatteryLink> link = LoadLink();
    SC_EXPECT(link != nullptr);
    if (!link) return;
    SC_EXPECT(link->Post().Def().targets.size() == link->Osa().Def().targets.size());
    double worst = 0.0;
    int compared = 0;
    for (int i = 0; i < 2400; ++i) {  // 240 s
        link->Step(0.1);
        if (i % 50 != 0) continue;
        for (const ScenarioTarget& t : link->Osa().Def().targets) {
            const bool a = link->Osa().TargetAlive(t.id);
            SC_EXPECT(a == link->Post().TargetAlive(t.id));
            if (!a) continue;
            worst = std::fmax(worst, (link->Osa().TargetPosition(t.id) - link->Post().TargetPosition(t.id)).Length());
            ++compared;
        }
    }
    std::printf("    %d comparisons, worst position difference %.3f m\n", compared, worst);
    SC_EXPECT(compared > 10 && worst < 1.0);
}

SC_TEST("battery link: the post gives 21 targets by voice; its commander hears them as the PU-12's and the crew fights") {
    std::unique_ptr<BatteryLink> link = LoadLink();
    SC_EXPECT(link != nullptr);
    if (!link) return;
    link->RunToEnd(0.1, 900.0);
    const std::string post = link->Post().Debrief();
    const std::string osa = link->Osa().Debrief();
    // The post designated something to 21, and 21's commander heard a designation from the PU-12 on the command net.
    const int to_21 = Count(post, "21  target ");
    std::printf("    designations to 21: %d; the Osa heard: %d; its launches: %d\n", to_21, Count(osa, "designation 6"), link->Osa().Launches());
    SC_EXPECT(to_21 >= 1);
    SC_EXPECT(Count(osa, "designation 6") >= 1);
    SC_EXPECT(link->Osa().Launches() >= 1);
    // 21 reported on the battery net, and the post wrote it on its IT boards; it never reported as an abstract shooter.
    SC_EXPECT(post.find("21  ready for action") != std::string::npos);
    bool on_board = false;
    for (const LauncherStatus& s : link->Post().StatusBoards()) on_board = on_board || (s.callsign == "21" && s.reported_s > 0.0);
    SC_EXPECT(on_board);
    SC_EXPECT(post.find("21  captures") == std::string::npos);  // an abstract crew's words; 21's own crew fights
}

SC_TEST("battery link: a target is destroyed once, by one launcher; both runners agree; the friend is never fired at") {
    std::unique_ptr<BatteryLink> link = LoadLink();
    SC_EXPECT(link != nullptr);
    if (!link) return;
    link->RunToEnd(0.1, 900.0);
    const std::string post = link->Post().Debrief();
    const std::string osa = link->Osa().Debrief();
    std::map<std::string, int> kills;
    for (const char* t : {"t1", "t2", "t3", "t4"}) {
        const std::string id = t;
        const int by_post = Count(post, "destroys " + id);
        const int by_osa = Count(post, "21  destroys " + id);  // the Osa's own report, on the post's net
        kills[id] = by_post;
        SC_EXPECT(by_post <= 1);
        SC_EXPECT(!link->Osa().TargetAlive(id) && !link->Post().TargetAlive(id));
        (void)by_osa;
    }
    for (const auto& k : kills) std::printf("    %s: destroyed %d time(s)\n", k.first.c_str(), k.second);
    // Every launch of the Osa was at a target the post gave it (the board's number of each target from the post's log).
    std::map<std::string, std::string> number_of;  // target -> its board number
    const std::string marked = "on the post's board: ";
    for (std::size_t at = post.find(marked); at != std::string::npos; at = post.find(marked, at + 1)) {
        const std::size_t line = post.rfind('\n', at);
        const std::string head = post.substr(line + 1, at - line - 1);
        const std::size_t t = head.find(" t");
        if (t == std::string::npos) continue;
        number_of[head.substr(t + 1, head.find(' ', t + 1) - t - 1)] = post.substr(at + marked.size(), 4);
    }
    int own_launches = 0;
    const std::string launched = "21  launches at ";
    for (std::size_t at = post.find(launched); at != std::string::npos; at = post.find(launched, at + 1)) {
        const std::string target = post.substr(at + launched.size(), post.find('\n', at) - at - launched.size());
        ++own_launches;
        const bool given = number_of.count(target) != 0 && post.find("21  target " + number_of[target] + " by voice") != std::string::npos;
        if (!given) std::printf("    21 launched at %s, which the post never gave it\n", target.c_str());
        SC_EXPECT(given);
    }
    SC_EXPECT(own_launches >= 1);
    SC_EXPECT(osa.find("launches at t5") == std::string::npos && post.find("launches at t5") == std::string::npos);
    // What the post's launchers destroyed, the Osa heard on the battery net.
    SC_EXPECT(Count(osa, "destroys t") >= 1);
}

SC_TEST("battery link: with the post's battery net off the air the Osa hears no designation from it") {
    std::unique_ptr<BatteryLink> link = LoadLink();
    SC_EXPECT(link != nullptr);
    if (!link) return;
    ConsoleData& c = link->Post().Console();
    SC_EXPECT(!c.post_battery_net.empty());
    // The post's battery-net set off: every control of the subsystem's power chain is the post's own; turn the set off.
    const std::string set = "radio.fug4.power";
    SC_EXPECT(c.panel.Find(set) != nullptr);
    c.panel.Set(set, 0);
    link->RunToEnd(0.1, 400.0);
    SC_EXPECT(!link->Post().PostBatteryNetUp());
    SC_EXPECT(Count(link->Osa().Debrief(), "designation 6") == 0);
    SC_EXPECT(Count(link->Post().Debrief(), "21  target ") == 0);
}

SC_TEST("battery link: the files must fit together") {
    std::string error;
    SC_EXPECT(LoadBatteryLinkFile(kData + "/scenarios/no_such_link.json", kData, &error) == nullptr && !error.empty());
    // An Osa scenario whose battery commander is not in a post is refused.
    std::unique_ptr<BatteryLink> link = LoadLink();
    SC_EXPECT(link != nullptr);
    SC_EXPECT(link && link->Osa().LinkedToPost() && !link->Post().LinkedToPost() && link->Post().ExternalLauncher("21"));
}
