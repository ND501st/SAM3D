// The missions menu's catalogue (simcore/missions.hpp, data/scenarios/missions.json).
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>

#include "simcore/missions.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

bool Refused(const std::string& json_text, const std::string& expect_in_error) {
    MissionCatalog c;
    std::string e;
    const bool ok = LoadMissionCatalogJson(json_text, kData, c, &e);
    if (ok || e.find(expect_in_error) == std::string::npos) std::printf("    error: %s\n", e.c_str());
    return !ok && e.find(expect_in_error) != std::string::npos;
}

}  // namespace

SC_TEST("missions: the catalogue loads and every mission in it starts, with its seats and the one that commands") {
    const auto t0 = std::chrono::steady_clock::now();
    MissionCatalog c;
    std::string e;
    SC_EXPECT(LoadMissionCatalog(kData, c, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::printf("    %zu missions in %zu groups, loaded in %.0f ms\n", c.Count(), c.groups.size(), ms);
    SC_EXPECT(c.Count() == 14 && c.groups.size() == 2);
    for (const MissionGroup& g : c.groups) {
        SC_EXPECT(!g.label.empty() && !g.notes.empty());
        for (const MissionDef& m : g.missions) {
            SC_EXPECT(!m.title.empty() && !m.name.empty() && !m.description.empty());
            SC_EXPECT(!m.vehicles.empty());
            for (const MissionVehicle& v : m.vehicles) {
                SC_EXPECT(v.seats.size() == 5);           // the Osa's and the PU-12M's crews of five
                SC_EXPECT(v.Commander() != nullptr);
            }
        }
    }
}

SC_TEST("missions: the commander's seat commands (the Osa's commander, the PU-12M's battery commander); a link offers both vehicles") {
    MissionCatalog c;
    SC_EXPECT(LoadMissionCatalog(kData, c));
    const MissionDef* crew = c.Find("osa_crew_practice");
    SC_EXPECT(crew != nullptr && !crew->link && crew->vehicles.size() == 1);
    if (crew != nullptr) {
        SC_EXPECT(crew->vehicles[0].system_id == "osa_ak" && crew->vehicles[0].Commander()->id == "commander");
        SC_EXPECT(crew->name == "Osa-AK crew practice: the NPC commander runs the crew (take any seat)");
        SC_EXPECT(crew->group == "missions");
    }
    const MissionDef* post = c.Find("pu12m_deploy");
    SC_EXPECT(post != nullptr && post->vehicles[0].Commander()->id == "battery_commander");
    const MissionDef* link = c.Find("battery_link_practice");
    SC_EXPECT(link != nullptr && link->link && link->vehicles.size() == 2);
    if (link != nullptr) {
        const MissionVehicle* osa = link->Vehicle("osa");
        const MissionVehicle* pu = link->Vehicle("post");
        SC_EXPECT(osa != nullptr && pu != nullptr);
        SC_EXPECT(osa->scenario_file == "scenarios/link_osa_21.json" && osa->system_id == "osa_ak" && osa->Commander()->id == "commander");
        SC_EXPECT(pu->scenario_file == "scenarios/link_pu12m.json" && pu->system_id == "pu12m" && pu->Commander()->id == "battery_commander");
        SC_EXPECT(osa->label == "the Osa 21");
    }
    SC_EXPECT(c.Find("link_osa_21") == nullptr);  // a half of the link, not a mission of its own
}

SC_TEST("missions: every scenario of data/scenarios is in the menu, or is a half of a link in it") {
    MissionCatalog c;
    SC_EXPECT(LoadMissionCatalog(kData, c));
    std::set<std::string> covered;
    for (const MissionGroup& g : c.groups) {
        for (const MissionDef& m : g.missions) {
            covered.insert(m.file);
            for (const MissionVehicle& v : m.vehicles) covered.insert(v.scenario_file);
        }
    }
    int files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(kData + "/scenarios")) {
        const std::string name = entry.path().filename().string();
        if (entry.path().extension() != ".json" || name == "missions.json") continue;
        ++files;
        const bool listed = covered.count("scenarios/" + name) > 0;
        if (!listed) std::printf("    not in the menu: scenarios/%s\n", name.c_str());
        SC_EXPECT(listed);
    }
    SC_EXPECT(files == 16);
}

SC_TEST("missions: a mission that would not start is refused, naming its place in the list") {
    const std::string head = R"({"catalog_id": "t", "groups": [{"id": "g", "label": "G", "missions": [)";
    const std::string tail = "]}]}";
    SC_EXPECT(Refused(head + R"({"file": "scenarios/nowhere.json", "title": "x"})" + tail, "groups[0].missions[0].file: cannot open"));
    SC_EXPECT(Refused(head + R"({"file": "scenarios/osa_practice.json", "title": "x", "vehicles": [{"id": "osa", "label": "x"}]})" + tail,
                      "only a battery link"));
    SC_EXPECT(Refused(head + R"({"file": "scenarios/battery_link_practice.json", "title": "x"})" + tail, "needs the vehicles"));
    SC_EXPECT(Refused(head + R"({"file": "scenarios/battery_link_practice.json", "title": "x", "vehicles": [{"id": "tzm", "label": "x"}]})" + tail,
                      "must be \"osa\" or \"post\""));
    SC_EXPECT(Refused(head + R"({"file": "scenarios/osa_practice.json", "title": "x"}, {"file": "scenarios/osa_practice.json", "title": "y"})" + tail,
                      "listed twice"));
    SC_EXPECT(Refused(R"({"catalog_id": "t", "groups": []})", "groups: required"));
}
