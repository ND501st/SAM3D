// Roadmap 10.1 and 10.2: the training-range shoots (Ashuluk, Ustka, Kapustin Yar) and the historical scenarios (the
// Bekaa, 24 July 1982; Angola, the north-south road; Cuito Cuanavale, 20 February 1988) play as their sources describe
// them. The events come from the sources named in each file; every flight profile in them is a placeholder.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
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

std::string Scenario(const std::string& name) { return ReadFile(kData + "/scenarios/" + name + ".json"); }

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

bool Has(const std::string& debrief, const std::string& text) { return debrief.find(text) != std::string::npos; }

int Count(const std::string& debrief, const std::string& text) {
    int n = 0;
    for (std::size_t at = debrief.find(text); at != std::string::npos; at = debrief.find(text, at + text.size())) ++n;
    return n;
}

// The number after `prefix` on the first line that has it ("guided at k1, 8.3 km" -> 8.3); -1 if none.
double NumberAfter(const std::string& debrief, const std::string& prefix) {
    const std::size_t at = debrief.find(prefix);
    if (at == std::string::npos) return -1.0;
    return std::atof(debrief.c_str() + at + prefix.size());
}

// The search radar's contacts that matched no target: the operator's unknown echoes ("T12").
int UnknownContacts(const std::string& debrief) {
    int n = 0;
    std::istringstream in(debrief);
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t at = line.find("SOC              event      T");
        if (at != std::string::npos && line.find("contact", at) != std::string::npos) ++n;
    }
    return n;
}

}  // namespace

SC_TEST("range: at Ustka the crew fires a two-missile salvo at each P-15 target, the second before the first arrives") {
    auto r = Runner(Scenario("osa_range_ustka"));
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    int launches = 0, salvo_seconds = 0;
    double last_launch_s = -1e9, gap_s = 0.0;
    while (!r->Finished() && r->Time() < 600.0) {
        const std::string tracked = r->Engagement().TrackedTarget();
        const int in_air = tracked.empty() ? 0 : r->Engagement().ShotsInFlightAt(tracked);
        r->Step(0.1);
        if (r->Launches() > launches) {
            launches = r->Launches();
            if (in_air > 0) {  // launched while a missile was already on its way to the same target
                ++salvo_seconds;
                gap_s = r->Time() - last_launch_s;
            }
            last_launch_s = r->Time();
        }
    }
    const std::string d = r->Debrief();
    std::printf("    launches %d, second missiles of a salvo %d (the last %.1f s after the first)\n", launches, salvo_seconds, gap_s);
    SC_EXPECT(salvo_seconds >= 2);                // one at each target
    SC_EXPECT(gap_s >= 3.0 && gap_s <= 8.0);      // as soon as the launcher allows: 3-5 s, and the deputy's reaction
    SC_EXPECT(Has(d, "Вторая — пуск!"));
    SC_EXPECT(Has(d, "guided at p1") && Has(d, "guided at p2"));
    // The kill's last blip is not reported as a new echo.
    SC_EXPECT(UnknownContacts(d) == 0);
}

SC_TEST("range: without the salvo order the crew waits for each missile's result before the next") {
    auto r = Runner(ReplaceOnce(Scenario("osa_range_ustka"), "\"doctrine\": { \"salvo\": 2 },", ""));
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    int launches = 0, while_in_air = 0;
    while (!r->Finished() && r->Time() < 600.0) {
        const bool flying = r->Engagement().ShotsInFlight();
        r->Step(0.1);
        if (r->Launches() > launches) {
            launches = r->Launches();
            while_in_air += flying ? 1 : 0;
        }
    }
    SC_EXPECT(launches >= 2);
    SC_EXPECT(while_in_air == 0);
}

SC_TEST("range: the salvo doctrine loads 1 or 2 and refuses anything else") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(!LoadScenario(ReplaceOnce(Scenario("osa_range_ustka"), "\"salvo\": 2", "\"salvo\": 3"), kData, def, console, &error));
    SC_EXPECT(error.find("doctrine.salvo") != std::string::npos);
    SC_EXPECT(LoadScenario(Scenario("osa_range_ustka"), kData, def, console, &error) && def.doctrine.salvo == 2);
}

SC_TEST("range: at Kapustin Yar a ready crew gets its missile away near the zone's far edge") {
    const std::string d = Play(Scenario("osa_range_kapustin_yar"), 600.0);
    const double km = NumberAfter(d, "guided at k1, ");
    std::printf("    the first missile at %.1f km (the zone's edge 10.3 km)\n", km);
    // The Osa-AK starts the preparation when the target is in the zone (flak11): 15 s at 80 m/s of closing speed.
    SC_EXPECT(km >= 8.0 && km <= 10.3);
}

SC_TEST("range: at Ashuluk the crew comes to readiness from cold and engages both drones, the high one too") {
    const std::string d = Play(Scenario("osa_range_ashuluk"), 1000.0);
    if (!Has(d, "guided at la1")) std::printf("%s\n", d.c_str());
    SC_EXPECT(Has(d, "Ready for action at a halt"));  // timed against the network chart
    SC_EXPECT(Has(d, "guided at la1"));               // 4 km up: in the search radar's tall beam III
    SC_EXPECT(Has(d, "guided at la2"));
    SC_EXPECT(UnknownContacts(d) == 0);               // the high drone stays itself, not an unknown echo
}

SC_TEST("history: over the Bekaa the mountains hide the F-4Es until they top the crest; the crew engages them") {
    const std::string text = Scenario("osa_bekaa_1982");
    const std::string d = Play(text, 600.0);
    const double seen_km = NumberAfter(d, "  contact ");
    // The same raid over a flat valley floor, the mountains taken away.
    std::string flat = text;
    const std::size_t from = flat.find("\"ridges\": [");
    SC_EXPECT(from != std::string::npos);
    if (from != std::string::npos) {
        std::size_t to = flat.find('[', from);
        for (int depth = 0; to < flat.size(); ++to) {
            depth += flat[to] == '[' ? 1 : (flat[to] == ']' ? -1 : 0);
            if (depth == 0) break;
        }
        flat.replace(from, to + 1 - from, "\"ridges\": []");
    }
    const double flat_km = NumberAfter(Play(flat, 300.0), "  contact ");
    std::printf("    first seen at %.1f km behind the mountains, %.1f km over flat ground\n", seen_km, flat_km);
    SC_EXPECT(seen_km > 0.0 && flat_km > seen_km + 5.0);
    SC_EXPECT(Has(d, "guided at f1") || Has(d, "guided at f2"));
    SC_EXPECT(Has(d, "warning receiver: missile guidance"));  // the command link on the air from the launch
}

SC_TEST("history: dug in at night in Angola the crew fights by the radars; the Puma shows only when it climbs out of the canyon") {
    const std::string d = Play(Scenario("osa_angola_night"), 900.0);
    SC_EXPECT(Has(d, "guided at b1") || Has(d, "guided at b2"));
    SC_EXPECT(!Has(d, "made out on the TV") && !Has(d, "TV track"));  // night: the TV sees nothing
    // Its first echo is west of the canyon's mouth (azimuth -37 degrees): not while it flew below the walls.
    const double az = NumberAfter(d, "h1  contact ") >= 0.0 ? NumberAfter(d.substr(d.find("h1  contact ")), "azimuth ") : 0.0;
    std::printf("    the Puma first seen at azimuth %.0f deg\n", az);
    SC_EXPECT(Has(d, "h1  contact"));
    SC_EXPECT(az < -37.0);
    SC_EXPECT(Has(d, "guided at h1"));  // and fought as it heads for the road (the memoir's claim)
    SC_EXPECT(Count(d, "НЛЦ!") <= 6);  // a flickering lamp is called once, not again and again
}

SC_TEST("history: at Cuito Cuanavale the crew gets missiles away at the Mirages between the pull-up and the turn away") {
    const std::string d = Play(Scenario("osa_cuito_1988"), 600.0);
    const int at_mirages = Count(d, "guided at m");
    std::printf("    missiles at the Mirages: %d\n", at_mirages);
    SC_EXPECT(at_mirages >= 1);
}
