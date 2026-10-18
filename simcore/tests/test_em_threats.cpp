// Roadmap E6, threats in the EM layer (without anti-radiation missiles): decoy drones the TV can unmask, a stand-off
// jammer escorting a strike, the search radar in bursts, and the battery's own ground objects kept off the picture.
// Target numbers and jammer placements are placeholders.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/scope_feed.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string ReplaceOnce(std::string s, const std::string& from, const std::string& to) {
    const std::size_t at = s.find(from);
    if (at == std::string::npos) {
        std::printf("    test text not found: %s\n", from.c_str());
        return s;
    }
    return s.replace(at, from.size(), to);
}

std::unique_ptr<ScenarioRunner> Runner(const std::string& text) {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenario(text, kData, def, console, &error)) {
        std::printf("    load: %s\n", error.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

std::string Play(const std::string& text, double max_s) {
    auto r = Runner(text);
    if (r == nullptr) return std::string();
    r->RunToEnd(0.1, max_s);
    return r->Debrief();
}

bool Has(const std::string& debrief, const std::string& text) {
    return debrief.find(text) != std::string::npos;
}

}  // namespace

SC_TEST("em threats: warned of decoys, the crew makes the drones out on the TV, holds fire and keeps its missiles for the striker") {
    const std::string d = Play(ReadFile(kData + "/scenarios/osa_decoys.json"), 900.0);
    const bool unmasked = Has(d, "d1  made out on the TV: a decoy drone") || Has(d, "d2  made out on the TV: a decoy drone");
    if (!unmasked) std::printf("%s\n", d.c_str());
    SC_EXPECT(unmasked);
    SC_EXPECT(Has(d, "Ложная цель"));
    SC_EXPECT(!Has(d, "guided at d1") && !Has(d, "guided at d2"));  // no missile at a decoy
    SC_EXPECT(Has(d, "guided at s1"));
}

SC_TEST("em threats: not warned, the crew fires at the decoys; at night the TV can't help, so it fires by the radar") {
    const std::string text = ReadFile(kData + "/scenarios/osa_decoys.json");
    const std::string unwarned = Play(ReplaceOnce(text, "\"doctrine\": { \"tv_check\": true }", "\"doctrine\": { \"tv_check\": false }"), 900.0);
    SC_EXPECT(Has(unwarned, "guided at d1") || Has(unwarned, "guided at d2"));
    SC_EXPECT(!Has(unwarned, "made out on the TV"));
    const std::string night = Play(ReplaceOnce(text, "\"daylight\": true", "\"daylight\": false"), 900.0);
    SC_EXPECT(Has(night, "guided at d1") || Has(night, "guided at d2"));
    SC_EXPECT(!Has(night, "made out on the TV"));
}

SC_TEST("em threats: under stand-off jamming the strobe lies on the scope, the strikers show late, the pod is fought on its angle") {
    auto r = Runner(ReadFile(kData + "/scenarios/osa_soj_strike.json"));
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    bool strobe = false;
    double strobe_az = 0.0;
    while (!r->Finished() && r->Time() < 900.0) {
        r->Step(0.1);
        const ScopeFeed f = ScopeFeedOf(r->Engagement(), r->GetLauncher(), r->Time());
        if (!strobe && !f.strobes.empty()) {
            strobe = true;
            strobe_az = f.strobes[0].azimuth_deg;
        }
    }
    const std::string d = r->Debrief();
    std::printf("    strobe at %.0f deg; launches %d\n", strobe_az, r->Launches());
    SC_EXPECT(strobe);
    SC_EXPECT(strobe_az > -30.0 && strobe_az < 40.0);  // the racetrack lies north to north-east
    SC_EXPECT(!Has(d, "guided at soj1") && !Has(d, "soj1  contact"));  // the jammer itself stays beyond reach
    SC_EXPECT(Has(d, "radar on the jammer track"));                   // the striker's pod: angles only
    SC_EXPECT(Has(d, "warning receiver: missile guidance") || Has(d, "warning receiver: locked by"));
    SC_EXPECT(r->Launches() >= 1);
}

SC_TEST("em threats: with search bursts the transmitter radiates a fraction of the time while nothing is near, and the target is still fought") {
    std::string text = ReadFile(kData + "/scenarios/osa_crew_practice.json");
    text = ReplaceOnce(text, "\"crew_commander\": \"npc\",", "\"crew_commander\": \"npc\", \"doctrine\": { \"search_bursts\": { \"on_s\": 4, \"off_s\": 16 } },");
    auto r = Runner(text);
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    int on = 0, total = 0, switches = 0;
    bool was = false;
    while (!r->Finished() && r->Time() < 800.0) {
        r->Step(0.1);
        const bool now = r->Console().equipment.IsReady("soc_transmitter");
        if (r->Time() >= 300.0 && r->Time() < 340.0) {  // ready, the drone appeared (280 s) but still beyond 28 km
            ++total;
            on += now ? 1 : 0;
            switches += now != was ? 1 : 0;
        }
        was = now;
    }
    const double fraction = total > 0 ? static_cast<double>(on) / total : 1.0;
    std::printf("    radiating %.0f %% of 300-340 s (%d switches); launches %d\n", 100.0 * fraction, switches, r->Launches());
    SC_EXPECT(fraction > 0.1 && fraction < 0.5);  // 4 s in 20, plus the senior operator's reaction
    SC_EXPECT(switches >= 2);
    SC_EXPECT(r->Launches() >= 1);
    SC_EXPECT(r->Debrief().find("Высокое выключить!") != std::string::npos);
}

SC_TEST("em threats: the battery's own vehicles and dummy positions don't show as contacts, and a deployed battery doesn't stow between raids") {
    // The battery fight (its vehicles and two dummies around the position, an EM search radar), the raid held back
    // past the end: ten minutes with nothing in the air.
    std::string text = ReadFile(kData + "/scenarios/osa_battery_fight.json");
    std::size_t at = 0;
    int moved = 0;
    while ((at = text.find("\"appear_at_s\":", at)) != std::string::npos) {
        const std::size_t end = text.find_first_of(",}", at);
        text.replace(at, end - at, "\"appear_at_s\": 5000");
        at += 10;
        ++moved;
    }
    SC_EXPECT(moved >= 1);
    const std::string d = Play(text, 600.0);
    SC_EXPECT(d.find("SOC              event") == std::string::npos);  // no contact at all: nothing flies
    SC_EXPECT(!Has(d, "the crew is stowed and marches on"));
}
