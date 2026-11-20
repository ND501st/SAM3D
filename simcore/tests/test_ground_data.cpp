// Track F, F3.1: the ground war's data (simcore/ground_war/ground_data.hpp; docs/fronts/03_GROUND_WAR.md section 11).
// Every file loads strictly; every value is sourced or a labelled game value; every unit, place and equipment type the
// files name resolves; bad data is refused with a message, never an exception.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/command.hpp"
#include "simcore/country_map.hpp"
#include "simcore/ground_war.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::groundwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;
const char* const kFactions[] = {"warsaw_pact", "nato", "rebels", "neutral"};

std::string ReadAll(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

const GroundTables& Tables() {
    static GroundTables t;
    static bool loaded = false;
    if (!loaded) {
        std::string e;
        if (!LoadGroundTables(kData, t, &e)) std::printf("    %s\n", e.c_str());
        loaded = true;
    }
    return t;
}

std::shared_ptr<const CountryMap> Country() {
    static std::shared_ptr<const CountryMap> map;
    if (!map) {
        std::string e;
        map = LoadCountryMap(kData, "maps/pannonia.json", &e);
        if (!map) std::printf("    %s\n", e.c_str());
    }
    return map;
}

// The country map's places: settlements, landmarks, river crossings, and F1's sites once the map has them.
std::set<std::string> MapPlaces() {
    std::set<std::string> out;
    const std::shared_ptr<const CountryMap> m = Country();
    if (!m) return out;
    for (const CountrySettlementDef& s : m->Def().settlements) out.insert(s.id);
    for (const CountryLandmarkDef& l : m->Def().landmarks) out.insert(l.id);
    for (const CountryCrossing& c : m->Crossings()) out.insert(c.id);
    json::Value root;
    if (json::Parse(ReadAll(kData + "/maps/pannonia.json"), root)) {
        if (const json::Value* sites = root.Find("sites")) {
            for (const json::Value& s : sites->items) {
                if (const json::Value* id = s.Find("id")) out.insert(id->string);
            }
        }
    }
    return out;
}

// docs/SOURCES.md's registered ids and their tiers (as tools/check_citations.py reads them).
std::vector<std::pair<std::string, std::string>> Registered() {
    std::vector<std::pair<std::string, std::string>> out;
    const std::string text = ReadAll(kData + "/../docs/SOURCES.md");
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        if (line.compare(0, 3, "| `") != 0) continue;
        const std::size_t close = line.find('`', 3);
        if (close == std::string::npos) continue;
        const std::size_t bar = line.find('|', close);
        if (bar == std::string::npos) continue;
        out.push_back({line.substr(3, close - 3), line.substr(bar + 1, 3)});
    }
    return out;
}

// Every "source" string in a JSON tree.
void Sources(const json::Value& v, const std::string& path, std::vector<std::pair<std::string, std::string>>& out) {
    if (v.Is(json::Type::Object)) {
        for (std::size_t i = 0; i < v.keys.size(); ++i) {
            if (v.keys[i] == "source" && v.items[i].Is(json::Type::String)) {
                out.push_back({path, v.items[i].string});
            } else {
                Sources(v.items[i], path + "." + v.keys[i], out);
            }
        }
    } else if (v.Is(json::Type::Array)) {
        for (std::size_t i = 0; i < v.items.size(); ++i) Sources(v.items[i], path + "[" + std::to_string(i) + "]", out);
    }
}

bool IsWordChar(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; }

// A source names a registered id (not tier 5) as a word, or says it is SAM3D's own (check_citations.py's rule).
bool SourceOk(const std::string& s, const std::vector<std::pair<std::string, std::string>>& reg, std::string* why) {
    std::string lower = s;
    for (char& c : lower) c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    bool named = false;
    for (const auto& r : reg) {
        for (std::size_t at = lower.find(r.first); at != std::string::npos; at = lower.find(r.first, at + 1)) {
            const bool left = at == 0 || !IsWordChar(lower[at - 1]);
            const bool right = at + r.first.size() >= lower.size() || !IsWordChar(lower[at + r.first.size()]);
            if (left && right) {
                if (r.second.find('5') == 0 || r.second.find(" 5") == 0) {
                    *why = "cites tier-5 " + r.first;
                    return false;
                }
                named = true;
            }
        }
    }
    if (named) return true;
    static const char* const own[] = {"sam3d", "estimate", "assumption", "placeholder", "docs/", "derived", "game value"};
    for (const char* o : own) {
        if (lower.find(o) != std::string::npos) return true;
    }
    *why = "names no registered source";
    return false;
}

double LineLengthKm(const airwar::FrontDef& f) {
    double l = 0.0;
    for (std::size_t i = 1; i < f.line.size(); ++i) l += std::hypot(f.line[i].x - f.line[i - 1].x, f.line[i].y - f.line[i - 1].y);
    return l / 1000.0;
}

// The text of a good file with one substring replaced (the refusal tests).
std::string With(const std::string& text, const std::string& from, const std::string& to) {
    std::string out = text;
    const std::size_t at = out.find(from);
    if (at != std::string::npos) out.replace(at, from.size(), to);
    return out;
}

}  // namespace

SC_TEST("ground data (F3.1): the tables load strictly and hold the cited norms") {
    const GroundTables& t = Tables();
    SC_EXPECT(!t.weights.empty());
    SC_EXPECT_NEAR(t.WeightOf("t55"), 1.0, 1e-12);
    bool known = true;
    SC_EXPECT_NEAR(t.WeightOf("ural_truck", &known), 0.0, 1e-12);
    SC_EXPECT(!known);
    // Dupuy's posture factors (ciano_1988_qjm fig. 24).
    SC_EXPECT_NEAR(t.posture[static_cast<std::size_t>(Posture::Attack)], 1.0, 1e-12);
    SC_EXPECT_NEAR(t.posture[static_cast<std::size_t>(Posture::HastyDefence)], 1.3, 1e-12);
    SC_EXPECT_NEAR(t.posture[static_cast<std::size_t>(Posture::PreparedDefence)], 1.5, 1e-12);
    SC_EXPECT_NEAR(t.posture[static_cast<std::size_t>(Posture::FortifiedDefence)], 1.6, 1e-12);
    SC_EXPECT_NEAR(t.posture[static_cast<std::size_t>(Posture::Delay)], 1.2, 1e-12);
    SC_EXPECT_NEAR(t.posture[static_cast<std::size_t>(Posture::Withdrawal)], 1.15, 1e-12);
    // FM 101-10-1 table 5-8, as fractions: attack of a position 4.1% then 2.2%, defence 2.2% then 1.3%, delay and inactive 1.0%.
    const LossRow& ap = t.losses[static_cast<std::size_t>(LossAction::AttackPosition)];
    const LossRow& dp = t.losses[static_cast<std::size_t>(LossAction::DefencePosition)];
    SC_EXPECT_NEAR(ap.battle_first + ap.nonbattle, 0.041, 1e-12);
    SC_EXPECT_NEAR(ap.battle_later + ap.nonbattle, 0.022, 1e-12);
    SC_EXPECT_NEAR(dp.battle_first + dp.nonbattle, 0.022, 1e-12);
    SC_EXPECT_NEAR(dp.battle_later + dp.nonbattle, 0.013, 1e-12);
    SC_EXPECT_NEAR(t.losses[static_cast<std::size_t>(LossAction::Delay)].battle_first + t.losses[static_cast<std::size_t>(LossAction::Delay)].nonbattle, 0.010, 1e-12);
    SC_EXPECT_NEAR(t.losses[static_cast<std::size_t>(LossAction::Inactive)].battle_later + t.losses[static_cast<std::size_t>(LossAction::Inactive)].nonbattle, 0.010, 1e-12);
    SC_EXPECT_NEAR(t.losses[static_cast<std::size_t>(LossAction::AttackFortified)].battle_first + 0.003, 0.066, 1e-12);
    // HERO's averages (hero_1972_oralfore p. IV-2) and FM 101-10-1's tanks (table 7-7).
    SC_EXPECT_NEAR(t.class_km_per_day[1], 2.80, 1e-12);
    SC_EXPECT_NEAR(t.class_km_per_day[2], 10.10, 1e-12);
    SC_EXPECT_NEAR(t.class_km_per_day[3], 23.34, 1e-12);
    SC_EXPECT_NEAR(t.class_km_per_day[4], 54.79, 1e-12);
    SC_EXPECT_NEAR(t.unopposed_km_per_day, 69.78, 1e-12);
    SC_EXPECT_NEAR(t.tank_losses[static_cast<std::size_t>(TankCommitment::DelayHeavy)][0], 0.73, 1e-12);
    SC_EXPECT_NEAR(t.tank_losses[static_cast<std::size_t>(TankCommitment::DefendHeavy)][0], 0.54, 1e-12);
    // FM 100-2-1's decision times (p. 5-14).
    SC_EXPECT_NEAR(t.reaction_h[static_cast<std::size_t>(Echelon::Division)].lo, 2.0, 1e-12);
    SC_EXPECT_NEAR(t.reaction_h[static_cast<std::size_t>(Echelon::Division)].hi, 4.0, 1e-12);
    SC_EXPECT_NEAR(t.reaction_h[static_cast<std::size_t>(Echelon::Battalion)].lo * 60.0, 25.0, 0.01);
    SC_EXPECT_NEAR(t.road_day_kmh.lo, 20.0, 1e-12);
    SC_EXPECT(t.division_reports_per_day == 2);
}

SC_TEST("ground data (F3.1): every enemy faction's ground forces load, by their own faction, with a plan") {
    const std::set<std::string> map_places = MapPlaces();
    SC_EXPECT(map_places.count("capital") == 1 && map_places.count("bx_road_ne") == 1);
    for (const char* f : kFactions) {
        GroundForceDef g;
        std::string e;
        SC_EXPECT(LoadGroundForce(kData, f, g, &e));
        if (!e.empty()) std::printf("    %s: %s\n", f, e.c_str());
        SC_EXPECT(g.faction == f && !g.oob.units.empty());
        // Every equipment type is weighted (a misspelt id would count nothing).
        const std::vector<std::string> unweighted = UnweightedTypes(Tables(), g.oob);
        for (const std::string& u : unweighted) std::printf("    %s: unweighted type %s\n", f, u.c_str());
        SC_EXPECT(unweighted.empty());
        // Every place resolves: on the map, or in the file's places_todo (and then not on the map yet: F1 adds it).
        for (const std::string& p : g.NamedPlaces()) {
            const bool on_map = map_places.count(p) == 1;
            const bool todo = g.PlaceTodo(p) != nullptr;
            if (on_map == todo) std::printf("    %s: place %s %s\n", f, p.c_str(), on_map ? "on the map and in places_todo" : "unknown");
            SC_EXPECT(on_map != todo);
        }
        // Every place waiting for F1 is used.
        const std::vector<std::string> named = g.NamedPlaces();
        for (const PlaceTodoDef& p : g.places_todo) SC_EXPECT(std::find(named.begin(), named.end(), p.id) != named.end());
        // Each force's top unit is a commanded formation; the plan's every unit is in its order of battle (the loader's check).
        SC_EXPECT(!g.commanders.authored.empty());
        SC_EXPECT(g.commanders.pool.ranks.count("division") == 1 || g.commanders.pool.ranks.count("brigade") == 1);
        const double strength = StrengthOfTree(Tables(), g.oob, g.oob.units.front().id);
        std::printf("    %s: %zu units, strength %.0f T-55 equivalents, plan %s from h %.0f, %zu axes\n", f, g.oob.units.size(), strength,
                    PlanKindName(g.plan.kind), g.plan.start_after_h, g.plan.axes.size());
        SC_EXPECT(strength > 0.0);
    }
}

SC_TEST("ground data (F3.1): the owner's open decisions as proposed: who starts, the rebels' raids, the neutral's 40 km, our stop at the border") {
    GroundForceDef pact;
    GroundForceDef nato;
    GroundForceDef rebels;
    GroundForceDef neutral;
    SC_EXPECT(LoadGroundForce(kData, "warsaw_pact", pact) && LoadGroundForce(kData, "nato", nato) && LoadGroundForce(kData, "rebels", rebels) &&
              LoadGroundForce(kData, "neutral", neutral));
    // The Pact with its air operation, NATO after a few days of rollback.
    SC_EXPECT(pact.plan.kind == PlanKind::Offensive && pact.plan.start_after_h < 24.0);
    SC_EXPECT(nato.plan.kind == PlanKind::Offensive && nato.plan.start_after_h >= 48.0);
    // The Pact's army: two to four motor rifle and one or two tank divisions (fm_100_2_1_1984 pp. 4-4 to 4-7).
    int divisions = 0;
    for (const command::OobUnit& u : pact.oob.units) divisions += u.kind == "division" ? 1 : 0;
    SC_EXPECT(divisions >= 3 && divisions <= 6);
    SC_EXPECT(pact.doctrine.withdraw_needs_order && pact.doctrine.second_echelon_to_success);
    SC_EXPECT(pact.doctrine.effectiveness.kind == EffectivenessKind::Lines);
    SC_EXPECT_NEAR(pact.doctrine.effectiveness.ineffective_below, 0.5, 1e-12);
    SC_EXPECT_NEAR(pact.doctrine.caps.breakthrough_km_per_day, 30.0, 1e-12);
    // NATO: a corps of three divisions and a cavalry regiment; breakpoints drawn from ranges; 6:1 at the point of decision.
    SC_EXPECT(nato.doctrine.effectiveness.kind == EffectivenessKind::Breakpoints);
    SC_EXPECT_NEAR(nato.doctrine.effectiveness.attack_stop_losses.lo, 0.15, 1e-12);
    SC_EXPECT_NEAR(nato.doctrine.effectiveness.defence_break_losses.hi, 0.5, 1e-12);
    SC_EXPECT_NEAR(nato.doctrine.breakthrough_ratio.lo, 6.0, 1e-12);
    SC_EXPECT(!nato.doctrine.withdraw_needs_order);
    // The rebels hold a ragged line and raid by night; they disperse rather than retreat.
    SC_EXPECT(rebels.plan.kind == PlanKind::Raids && rebels.plan.has_raids && rebels.plan.raids.night_only);
    SC_EXPECT(rebels.doctrine.disperse_rather_than_retreat && rebels.plan.axes.empty());
    // The neutral goes to its political depth and holds.
    SC_EXPECT(neutral.plan.kind == PlanKind::LimitedIncursion);
    SC_EXPECT_NEAR(neutral.plan.max_depth_km, 40.0, 1e-12);
    // Our counterattack restores the border and stops there.
    GroundDefenceDef ours;
    SC_EXPECT(LoadGroundDefence(kData, ours));
    SC_EXPECT(ours.doctrine.counterattack_stops_at_border && ours.doctrine.withdraw_needs_order && ours.doctrine.counterattack_by_senior);
}

SC_TEST("ground data (F3.1): our defence loads with its order of battle, the equipment filled and a deployment per adversary") {
    GroundDefenceDef d;
    std::string e;
    SC_EXPECT(LoadGroundDefence(kData, d, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    // The fill is applied to the loaded order of battle, not to the file.
    const command::OobUnit* mrr = d.oob.Find("mrr_71");
    SC_EXPECT(mrr != nullptr && mrr->EquipmentCount("t55") == 31);
    command::OrderOfBattle file_oob;
    SC_EXPECT(command::LoadOrderOfBattle(kData, d.oob_file, file_oob));
    SC_EXPECT(file_oob.Find("mrr_71") != nullptr && file_oob.Find("mrr_71")->equipment.empty());
    // ČSLA 1985: a division 30-45 km wide in defence; the security zone 20-40 km.
    const FrontageDef* div = d.doctrine.Frontage("division", "defence");
    SC_EXPECT(div != nullptr && div->width_km.lo == 30.0 && div->width_km.hi == 45.0);
    SC_EXPECT(d.doctrine.has_security_zone && d.doctrine.security_zone_depth_km.lo == 20.0 && d.doctrine.security_zone_depth_km.hi == 40.0);
    // Every deployment's stretch lies on its adversary's front line in the nation's theatre, and its places resolve.
    airwar::TheatreDef nation;
    SC_EXPECT(airwar::LoadTheatreFile(kData + "/air_war/theatre_pannonia_nation.json", nation, &e));
    const std::set<std::string> map_places = MapPlaces();
    for (const char* f : kFactions) {
        const DeploymentDef* dep = d.For(f);
        SC_EXPECT(dep != nullptr);
        if (dep == nullptr) continue;
        const airwar::FrontDef* front = nullptr;
        for (const airwar::FrontDef& fr : nation.fronts) front = fr.faction == f ? &fr : front;
        SC_EXPECT(front != nullptr);
        const double length_km = front != nullptr ? LineLengthKm(*front) : 0.0;
        SC_EXPECT(dep->security_zone_km >= d.doctrine.security_zone_depth_km.lo - 10.0);
        double strength = 0.0;
        for (const DeployedUnitDef& u : dep->units) {
            SC_EXPECT(u.to_km <= length_km + 1e-9);
            SC_EXPECT(u.place.empty() || map_places.count(u.place) == 1);
            strength += StrengthOfTree(Tables(), d.oob, u.unit);
        }
        std::printf("    against %s: %zu formations, strength %.0f, line %.0f km\n", f, dep->units.size(), strength, length_km);
        SC_EXPECT(strength > 0.0);
    }
    // Our authored commander is the chain's: Maj Gen Kral of the 7th Division.
    SC_EXPECT(!d.commanders.authored.empty() && d.commanders.authored.front().post == "div_commander");
}

SC_TEST("ground data (F3.1): every source in the new files is registered in docs/SOURCES.md or a labelled game value") {
    const std::vector<std::pair<std::string, std::string>> reg = Registered();
    SC_EXPECT(reg.size() > 100);
    const char* const files[] = {"gameplay/ground_war.json", "air_war/ground_warsaw_pact.json", "air_war/ground_nato.json",
                                 "air_war/ground_rebels.json", "air_war/ground_neutral.json", "campaign/ground_defence.json"};
    int checked = 0;
    for (const char* f : files) {
        json::Value root;
        SC_EXPECT(json::Parse(ReadAll(kData + "/" + f), root));
        std::vector<std::pair<std::string, std::string>> sources;
        Sources(root, "", sources);
        SC_EXPECT(!sources.empty());
        for (const auto& s : sources) {
            std::string why;
            const bool ok = SourceOk(s.second, reg, &why);
            if (!ok) std::printf("    %s %s: %s\n", f, s.first.c_str(), why.c_str());
            SC_EXPECT(ok);
            ++checked;
        }
    }
    std::printf("    %d sources checked\n", checked);
}

SC_TEST("ground data (F3.1): bad data is refused with a message") {
    const std::string tables = ReadAll(kData + "/gameplay/ground_war.json");
    const std::string pact = ReadAll(kData + "/air_war/ground_warsaw_pact.json");
    const std::string rebels = ReadAll(kData + "/air_war/ground_rebels.json");
    const std::string ours = ReadAll(kData + "/campaign/ground_defence.json");
    command::OrderOfBattle oob;
    SC_EXPECT(command::LoadOrderOfBattle(kData, "command/oob_pannonia.json", oob));
    // The good files load.
    {
        GroundTables t;
        GroundForceDef g;
        GroundDefenceDef d;
        SC_EXPECT(LoadGroundTablesJson(tables, t) && LoadGroundForceJson(pact, g) && LoadGroundDefenceJson(ours, oob, d));
    }
    struct Case {
        const char* what;
        int file;  // 0 tables, 1 the Pact, 2 the rebels, 3 ours
        const char* from;
        const char* to;
    };
    const Case cases[] = {
        {"an unknown key", 0, "\"trait_shift\":", "\"trait_shif\":"},
        {"a posture without a source", 0, "\"value\": 1.5, \"source\": \"ciano_1988_qjm fig. 24\", ", "\"value\": 1.5, "},
        {"a posture out of range", 0, "\"value\": 1.6, \"source\": \"ciano_1988_qjm fig. 24\"", "\"value\": 9.0, \"source\": \"ciano_1988_qjm fig. 24\""},
        {"advance classes out of order", 0, "\"from_ratio\": 4.0", "\"from_ratio\": 2.0"},
        {"a loss row twice", 0, "\"action\": \"pursuit\"", "\"action\": \"delay\""},
        {"a confidence not in the list", 0, "\"confidence\": \"low\"", "\"confidence\": \"probably\""},
        {"an unknown unit on an axis", 1, "\"first_echelon\": [\"pact_mrd_57\"]", "\"first_echelon\": [\"pact_mrd_99\"]"},
        {"a unit on two axes", 1, "\"first_echelon\": [\"pact_mrd_57\"]", "\"first_echelon\": [\"pact_mrd_35\"]"},
        {"a regiment beside its division", 1, "\"second_echelon\": [\"pact_td_9\"]", "\"second_echelon\": [\"pact_mrr_35_1\"]"},
        {"an unknown objective", 1, "\"objective\": \"dravec\"", "\"objective\": \"berlin\""},
        {"a route of one place", 1, "\"route\": [\"pact_assembly_rava\", \"bx_road_rava\", \"rava_dam\", \"dravec\", \"capital\"]", "\"route\": [\"capital\"]"},
        {"a place to add without anchor_todo", 1, "\"north_km\": 128, \"anchor_todo\": true", "\"north_km\": 128"},
        {"a broken order of battle", 1, "\"parent\": \"pact_td_9\"", "\"parent\": \"pact_td_99\""},
        {"effectiveness lines the wrong way round", 1, "\"value\": 0.667", "\"value\": 0.4"},
        {"caps the wrong way round", 1, "\"broken\": { \"value\": 70", "\"broken\": { \"value\": 20"},
        {"a commander for an unknown unit", 1, "\"unit\": \"pact_td_9\"", "\"unit\": \"pact_td_99\""},
        {"raids without their block", 2, "\"raids\": {", "\"raidz\": {"},
        {"a fill for a unit that has equipment", 3, "{ \"unit\": \"sw_brigade\"", "{ \"unit\": \"tr_73\""},
        {"a deployment of an unknown unit", 3, "{ \"unit\": \"mrd_7\", \"role\": \"first_echelon\"", "{ \"unit\": \"mrd_77\", \"role\": \"first_echelon\""},
        {"a reserve holding a stretch", 3, "{ \"unit\": \"tr_73\", \"role\": \"reserve\", \"place\": \"kesmark\" }",
         "{ \"unit\": \"tr_73\", \"role\": \"reserve\", \"from_km\": 0, \"to_km\": 10, \"place\": \"kesmark\" }"},
        {"a stretch the wrong way round", 3, "\"from_km\": 75, \"to_km\": 120, \"place\": \"dravec\"", "\"from_km\": 120, \"to_km\": 75, \"place\": \"dravec\""},
        {"a unit and its parent both deployed", 3, "{ \"unit\": \"tr_73\", \"role\": \"reserve\"", "{ \"unit\": \"mrd_7\", \"role\": \"reserve\""},
        {"an unknown role", 3, "\"role\": \"flank_screen\"", "\"role\": \"flank_guard\""},
        {"not JSON", 3, "{", "["},
    };
    for (const Case& c : cases) {
        const std::string& base = c.file == 0 ? tables : c.file == 1 ? pact : c.file == 2 ? rebels : ours;
        const std::string bad = With(base, c.from, c.to);
        SC_EXPECT(bad != base);  // the case's text is found in the file
        std::string e;
        bool ok = true;
        if (c.file == 0) {
            GroundTables t;
            ok = LoadGroundTablesJson(bad, t, &e);
        } else if (c.file == 3) {
            GroundDefenceDef d;
            ok = LoadGroundDefenceJson(bad, oob, d, &e);
        } else {
            GroundForceDef g;
            ok = LoadGroundForceJson(bad, g, &e);
        }
        if (ok) std::printf("    not refused: %s\n", c.what);
        SC_EXPECT(!ok && !e.empty());
    }
    // A force file loaded for another faction is refused; a missing file is reported.
    GroundForceDef g;
    std::string e;
    SC_EXPECT(!LoadGroundForce(kData, "martians", g, &e) && e.find("cannot open") != std::string::npos);
}
