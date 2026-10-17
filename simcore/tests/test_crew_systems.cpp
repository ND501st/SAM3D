// The Osa's systems in the NPC crew's decisions: IFF, СДЦ for low targets, emission control (the battery's EMCON, an
// ambush), the second missile, the radios, the fuel truck. Each system the console has is used by the NPCs where
// doctrine calls for it, not only by scripted triggers.
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string ReadAll(const std::string& path) {
    std::ifstream f(path);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

std::string Replace(std::string s, const std::string& from, const std::string& to) {
    std::size_t at = 0;
    while ((at = s.find(from, at)) != std::string::npos) {
        s.replace(at, from.size(), to);
        at += to.size();
    }
    return s;
}

std::unique_ptr<ScenarioRunner> Load(const std::string& text) {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    if (!LoadScenario(text, kData, def, console, &e)) {
        std::printf("    %s\n", e.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

bool Said(const ScenarioRunner& r, const std::string& id, const std::string& from = std::string()) {
    for (const SpokenCallout& c : r.Callouts()->Log()) {
        if (c.id == id && (from.empty() || c.from == from)) return true;
    }
    return false;
}

// A ready crew at a halt (no EM layer), its NPC commander, two aircraft: `targets` is their JSON.
std::string ReadyCrew(const std::string& targets, unsigned seed = 1) {
    return R"JSON({ "scenario_id": "sys", "name": "Systems", "source": "sam3d", "confidence": "estimate", "seed": )JSON" + std::to_string(seed) +
           R"JSON(, "origin": { "lat_deg": 46.9, "lon_deg": 19.7, "alt_m": 110 }, "terrain": { "type": "flat", "height_m": 110 },
      "crew_commander": "npc",
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", "readiness": "deployed",
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 } },
      "crew": [ { "name": "Commander", "role": "commander", "skill": "class1" }, { "name": "Deputy commander", "role": "deputy_commander", "skill": "class1" },
                { "name": "Senior operator", "role": "senior_operator", "skill": "class1" }, { "name": "Operator", "role": "operator", "skill": "class1" },
                { "name": "Driver", "role": "driver", "skill": "class1" } ],
      "targets": )JSON" + targets + R"JSON(,
      "triggers": [], "end": { "time_limit_s": 300 } })JSON";
}

std::string BatteryFight(const std::string& orders) {
    std::string t = ReadAll(kData + "/scenarios/osa_battery_fight.json");
    if (!orders.empty()) {
        t = Replace(t, "\"share_picture\": true,", "\"share_picture\": true, \"orders\": " + orders + ",");
    }
    return t;
}

}  // namespace

SC_TEST("npc systems: new contacts are interrogated; a friend is never engaged, the hostile is") {
    const std::string targets = R"JSON([
        { "id": "friend", "name": "Own fighter (placeholder)", "class": "fighter", "iff": "friend", "start": { "east_m": -600, "north_m": 9000, "up_m": 1200 },
          "waypoints": [ { "east_m": -600, "north_m": -20000, "up_m": 1200 } ], "speed_mps": 160, "rcs_m2": 3, "appear_at_s": 20 },
        { "id": "hostile", "name": "Fighter (placeholder)", "class": "fighter", "start": { "east_m": 900, "north_m": 11000, "up_m": 1000 },
          "waypoints": [ { "east_m": 900, "north_m": -20000, "up_m": 1000 } ], "speed_mps": 150, "rcs_m2": 3, "appear_at_s": 20 } ])JSON";
    std::unique_ptr<ScenarioRunner> r = Load(ReadyCrew(targets));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    bool friend_tracked_at_launch = false;
    int launches = 0;
    while (!r->Finished() && r->Time() < 300.0) {
        r->Step(0.1);
        if (r->Launches() > launches) {
            launches = r->Launches();
            friend_tracked_at_launch = friend_tracked_at_launch || r->Engagement().TrackedTarget() == "friend";
        }
    }
    SC_EXPECT(Said(*r, "iff_friend", "senior_operator"));    // "Свой!"
    SC_EXPECT(Said(*r, "iff_no_reply", "senior_operator"));  // "Ответа нет!"
    SC_EXPECT(r->Engagement().KnownFriend("friend") && r->Engagement().NoReply("hostile"));
    SC_EXPECT(launches >= 1);
    SC_EXPECT(!friend_tracked_at_launch);
}

SC_TEST("npc systems: a friend under track is held and let go (Пуск запрещаю, Цель сбросить)") {
    const std::string targets = R"JSON([
        { "id": "friend", "name": "Own fighter (placeholder)", "class": "fighter", "iff": "friend", "start": { "east_m": 0, "north_m": 9000, "up_m": 1200 },
          "waypoints": [ { "east_m": 0, "north_m": -20000, "up_m": 1200 } ], "speed_mps": 150, "rcs_m2": 3, "appear_at_s": 5 } ])JSON";
    std::unique_ptr<ScenarioRunner> r = Load(ReadyCrew(targets));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    // A careless player in command takes the friend before anyone has asked who it is.
    bool given = false, tracked = false, dropped = false;
    while (!r->Finished() && r->Time() < 120.0) {
        r->Step(0.1);
        if (!given && r->Time() > 8.0) {
            r->AssignTarget("friend");
            given = r->GiveCrewOrder("order_take_target");
        }
        if (r->Engagement().TrackedTarget() == "friend") tracked = true;
        if (tracked && r->Engagement().TrackedTarget().empty()) dropped = true;
    }
    SC_EXPECT(given && tracked);
    SC_EXPECT(Said(*r, "hold_fire", "commander"));
    SC_EXPECT(Said(*r, "order_drop_target", "commander"));
    SC_EXPECT(dropped);
    SC_EXPECT(r->Launches() == 0);
}

SC_TEST("npc systems: a low target designated by the PU-12 makes the commander order СДЦ, and the search radar finds it") {
    std::string t = BatteryFight("");
    t = Replace(t, "\"up_m\": 1000", "\"up_m\": 100");  // the first raid comes in low
    std::unique_ptr<ScenarioRunner> r = Load(t);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    bool mti_on = false, seen_low = false;
    while (!r->Finished() && r->Time() < 900.0 && !(mti_on && seen_low && Said(*r, "order_mti", "commander"))) {
        r->Step(0.1);
        mti_on = mti_on || r->Console().panel.Get("soc.mti") == 1;
        for (const Contact& c : r->Engagement().SearchPicture()) seen_low = seen_low || (mti_on && c.target_id == "t1");
    }
    std::printf("    СДЦ %s, the low target %s\n", mti_on ? "on" : "off", seen_low ? "seen" : "not seen");
    SC_EXPECT(Said(*r, "order_mti", "commander"));
    SC_EXPECT(mti_on);
    SC_EXPECT(seen_low);
}

SC_TEST("npc systems: under the battery's EMCON the crew stays silent and fights a designated target by the TV") {
    std::unique_ptr<ScenarioRunner> r = Load(BatteryFight(R"JSON([ { "at_s": 0, "order": "emcon", "radiate": false } ])JSON"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    bool radiated = false, tv = false;
    while (!r->Finished() && r->Time() < 900.0) {
        r->Step(0.1);
        radiated = radiated || r->Console().equipment.IsReady("soc_transmitter");
        tv = tv || r->Engagement().Method() == TrackMethod::Tv;
        if (tv && r->Launches() > 0) break;
    }
    std::printf("    TV track %s, launches %d at %.0f s\n", tv ? "yes" : "no", r->Launches(), r->Time());
    SC_EXPECT(!radiated);
    SC_EXPECT(Said(*r, "order_emcon_tv", "commander"));
    SC_EXPECT(tv);
}

SC_TEST("npc systems: in an ambush the radars stay silent until a hostile is close, then the ambush is sprung") {
    std::unique_ptr<ScenarioRunner> r = Load(BatteryFight(
        R"JSON([ { "at_s": 0, "order": "ambush", "points": [ { "east_m": 0, "north_m": 0, "up_m": 3 }, { "east_m": 3000, "north_m": 0, "up_m": 3 } ] } ])JSON"));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    double first_radiated = -1.0;
    while (!r->Finished() && r->Time() < 900.0 && first_radiated < 0.0) {
        r->Step(0.1);
        if (r->Console().equipment.IsReady("soc_transmitter")) first_radiated = r->Time();
    }
    std::printf("    first radiated at %.0f s\n", first_radiated);
    SC_EXPECT(r->Debrief().find("the ambush is sprung") != std::string::npos);
    SC_EXPECT(first_radiated > 300.0);  // the raid appears at 300 s, 45 km out: silent until it is within 15 km
}

SC_TEST("npc systems: a miss with the target still in the zone brings \"Вторая — пуск!\"") {
    // The practice drone over several seeds until one needs a second missile.
    bool found = false;
    for (unsigned seed = 1; seed <= 20 && !found; ++seed) {
        std::string t = ReadAll(kData + "/scenarios/osa_crew_practice.json");
        t = Replace(t, "\"seed\": 1", "\"seed\": " + std::to_string(seed));
        std::unique_ptr<ScenarioRunner> r = Load(t);
        if (!r) break;
        r->RunToEnd(0.1, 900.0);
        if (r->Launches() >= 2 && r->Engagement().Misses() >= 1) {
            found = true;
            SC_EXPECT(Said(*r, "order_second_missile", "commander"));
        }
    }
    SC_EXPECT(found);
}

SC_TEST("npc systems: the commander has the radios switched on when his set is off") {
    std::unique_ptr<ScenarioRunner> r = Load(BatteryFight(""));
    SC_EXPECT(r != nullptr);
    if (!r) return;
    r->Step(0.1);
    r->PlayerSet("radio.right.power", 0);  // someone switched the commander's set off
    while (!r->Finished() && r->Time() < 180.0 && r->Console().panel.Get("radio.right.power") != 1) r->Step(0.1);
    SC_EXPECT(Said(*r, "order_radio_on", "commander"));
    SC_EXPECT(r->Console().panel.Get("radio.right.power") == 1);
}

SC_TEST("npc systems: at the fuel reserve the commander asks for the fuel truck, and it comes alongside") {
    std::string t = BatteryFight("");
    t = Replace(t, "\"heading_deg\": 35,", "\"heading_deg\": 35, \"fuel_fraction\": 0.05,");
    std::unique_ptr<ScenarioRunner> r = Load(t);
    SC_EXPECT(r != nullptr);
    if (!r) return;
    bool alongside = false;
    while (!r->Finished() && r->Time() < 900.0 && !alongside) {
        r->Step(0.1);
        alongside = r->Console().equipment.GetInput("fuel_truck_alongside");
    }
    SC_EXPECT(Said(*r, "request_fuel", "commander"));
    SC_EXPECT(alongside);
}
