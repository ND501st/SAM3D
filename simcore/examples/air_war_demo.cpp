// air_war_demo: the enemy air command's war, end to end (docs/air_war/).
//
// Usage: air_war_demo [--theatre file] [faction] [days] [data_dir] [out_dir] [seed] [map]
//   --theatre: the theatre under data_dir/air_war/ (default theatre_pannonia.json; theatre_pannonia_nation.json is the
//   whole nation); faction: rebels, neutral, warsaw_pact or nato (default nato); days (default 3); data_dir (default
//   data); out_dir (default out/air_war); seed (default 1); map: the raids on a map's ground (e.g.
//   maps/kesmark_range.json), the battery and its dummies set down on open ground near where the campaign has them
//   (default: flat ground).
//
// Runs the campaign hour by hour. The enemy's staff plans its air tasking orders; raids are flown abstractly, except
// those through the player's battery's sector, which become scenarios (raid_scenario.hpp) loaded by the same loader
// as every hand-written scenario and played by ScenarioRunner with the NPC crew and battery commanders; the mission
// bridge brings the outcome back and the campaign carries on. Prints the journal (the enemy's mind and what our side
// learned), the last intelligence summary and the whole truth about the enemy's staff. Writes each generated scenario
// to out_dir (open one in the missions menu by copying it to data/scenarios, or play it with scenario_demo) and the
// journal as JSON (air_war_<faction>.json).
// Exits non-zero if the data fail to load or a generated scenario fails to load.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

std::string Escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        if (c == '\n') {
            o += "\\n";
            continue;
        }
        o += c;
    }
    return o;
}

}  // namespace

int main(int argc, char** argv) {
    std::string theatre = "theatre_pannonia.json";
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--theatre" && i + 1 < argc) {
            theatre = argv[++i];
        } else {
            args.push_back(a);
        }
    }
    const std::string faction = args.size() > 0 ? args[0] : "nato";
    const double days = args.size() > 1 ? std::atof(args[1].c_str()) : 3.0;
    const std::string data_dir = args.size() > 2 ? args[2] : "data";
    const std::string out_dir = args.size() > 3 ? args[3] : "out/air_war";
    const unsigned seed = args.size() > 4 ? static_cast<unsigned>(std::atoi(args[4].c_str())) : 1u;
    const std::string map_file = args.size() > 5 ? args[5] : "";

    AirWarData data;
    std::string error;
    if (!LoadAirWarData(data_dir, "air_war/" + theatre, "air_war/faction_" + faction + ".json", data, &error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    AirCampaign campaign;
    if (!campaign.Init(data, seed, &error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    ScenarioFormat format;
    if (!map_file.empty()) {
        format.map_file = map_file;
        format.map = LoadMapCached(data_dir, map_file, &error);
        if (!format.map) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
    }
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    // The battery fights the way a careful commander would: radars mostly silent, a move every 20 hours, dummy radio
    // traffic, decoy emitters at its dummies while its own radars are off. The enemy has to find it again and again.
    BatteryPosture& posture = campaign.Posture();
    posture.emission = 0.1;
    posture.emcon_discipline = 0.7;
    posture.radio_discipline = 0.7;
    posture.dummy_traffic = true;
    posture.relocate_every_h = 20.0;

    int played = 0, failed = 0;
    while (campaign.Now() < days * 24.0) {
        if (!campaign.Advance(days * 24.0)) continue;
        const Package* p = campaign.PlayerMission();
        RaidScenarioBuilder builder;
        const RaidScenario rs = builder.Build(campaign, *p, format);
        if (!rs.ok) {
            std::printf("  (raid %d not playable: %s)\n", p->id, rs.error.c_str());
            campaign.SkipPlayerMission();
            continue;
        }
        if (rs.manifest.own_moved.HorizontalLength() > 0.0) {
            std::printf("  (raid %d: the battery set %.0f m east and %.0f m north of its campaign position, onto open ground)\n", p->id,
                        rs.manifest.own_moved.x, rs.manifest.own_moved.y);
        }
        const std::string path = out_dir + "/" + rs.manifest.scenario_id + ".json";
        std::ofstream(path, std::ios::binary) << rs.json_text;
        ScenarioDef def;
        ConsoleData console;
        if (!LoadScenario(rs.json_text, data_dir, def, console, &error)) {
            std::fprintf(stderr, "generated scenario %s does not load: %s\n", path.c_str(), error.c_str());
            ++failed;
            campaign.SkipPlayerMission();
            continue;
        }
        ScenarioRunner runner(std::move(def), std::move(console));
        MissionBridge bridge(rs.manifest, campaign.Data(), seed * 977u + static_cast<unsigned>(p->id));
        while (!runner.Finished() && !bridge.UnitDestroyed()) {
            runner.Step(0.1);
            bridge.Step(runner);
        }
        const PackageOutcome outcome = bridge.Finish(runner);
        int drones = 0;
        for (const DebriefEntry& e : runner.Log().Entries()) {
            const bool hit = e.kind == DebriefEntry::Kind::Hit;
            if (!hit && (e.kind != DebriefEntry::Kind::Scenario || e.detail.compare(0, 9, "destroyed") != 0)) continue;
            for (const RaidAircraft& a : rs.manifest.aircraft) drones += a.drone && a.target_id == e.what ? 1 : 0;
        }
        std::printf("  >> %s (%s '%s' on %s): %d aircraft, %d launches, %d shot down (%d by the %d neighbours in reach) and %d decoys, "
                    "radars on %.0f s, %d ARMs%s\n",
                    rs.manifest.scenario_id.c_str(), MissionKindName(p->kind), p->name.c_str(), p->target_label.c_str(),
                    static_cast<int>(rs.manifest.aircraft.size()), runner.Launches(), outcome.Lost(), bridge.NeighbourKills(),
                    static_cast<int>(rs.manifest.neighbours.size()), drones, bridge.EmittedSeconds(), bridge.ArmsFired(),
                    bridge.UnitDestroyed() ? ", OUR VEHICLE HIT" : "");
        campaign.CompletePlayerMission(outcome, rs.manifest.start_h + runner.Time() / 3600.0);
        ++played;
    }

    const double start = data.theatre.start_hour;
    std::printf("\n=== Journal: %s, %.0f days ===\n", data.faction.name.c_str(), days);
    for (const JournalEntry& j : campaign.Journal()) {
        std::printf("%s %-14s %s%s: %s\n", FormatHour(j.h, start).c_str(), j.kind.c_str(), j.visible ? "[ours] " : "", j.who.c_str(), j.text.c_str());
    }
    std::printf("\n=== What our side knows ===\n%s\n=== The enemy's staff (the debrief's reveal) ===\n%s\n", campaign.Intsum(24.0).c_str(),
                campaign.EnemyMind().c_str());
    std::printf("Missions played: %d (failed to load: %d). Enemy aircraft lost: %d. ARMs left: %.0f. Enemy's knowledge of the battery: %.2f. Crew: %s.\n",
                played, failed, campaign.AircraftLost(), campaign.ArmStock(), campaign.EnemyKnowledgeOfBattery(), campaign.CrewTier().c_str());

    // The journal as JSON, for tools and the UI.
    const std::string jpath = out_dir + "/air_war_" + faction + ".json";
    std::ofstream js(jpath, std::ios::binary);
    js << "{\n  \"faction\": \"" << Escape(data.faction.name) << "\",\n  \"days\": " << days << ",\n  \"journal\": [\n";
    for (std::size_t i = 0; i < campaign.Journal().size(); ++i) {
        const JournalEntry& j = campaign.Journal()[i];
        js << "    {\"h\": " << j.h << ", \"clock\": \"" << FormatHour(j.h, start) << "\", \"kind\": \"" << j.kind << "\", \"who\": \""
           << Escape(j.who) << "\", \"visible\": " << (j.visible ? "true" : "false") << ", \"text\": \"" << Escape(j.text) << "\"}"
           << (i + 1 < campaign.Journal().size() ? "," : "") << "\n";
    }
    js << "  ],\n  \"enemy_mind\": \"" << Escape(campaign.EnemyMind()) << "\",\n  \"intsum\": \"" << Escape(campaign.Intsum(24.0)) << "\"\n}\n";
    std::printf("Wrote %s\n", jpath.c_str());
    return failed == 0 ? 0 : 1;
}
