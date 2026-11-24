// Track F, F3.1: the ground war's data (simcore/ground_war/ground_data.hpp; docs/fronts/03_GROUND_WAR.md section 11).
// Every file loads strictly; every value is sourced or a labelled game value; every unit, place (F1), sector (F2.1) and
// equipment type the files name resolves; a place moved on the map moves what names it; bad data is refused with a
// message, never an exception.
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

// The world the ground files name: the country map's places (F1) and the nation's theatre (F2.1's sectors).
const GroundWorld& NationWorld() {
    static GroundWorld w;
    static bool loaded = false;
    if (!loaded) {
        std::string e;
        if (!LoadGroundWorld(kData, "maps/pannonia.json", "air_war/theatre_pannonia_nation.json", w, &e)) std::printf("    %s\n", e.c_str());
        loaded = true;
    }
    return w;
}

// A place's point on the map, to the bit.
bool At(const GroundPlace& g, const MapPlaces& places) {
    const MapPoint* p = places.Find(g.at);
    return p != nullptr && g.pos.x == p->east_m && g.pos.y == p->north_m && g.pos.z == 0.0;
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
    const GroundWorld& world = NationWorld();
    SC_EXPECT(world.places.Find("capital") != nullptr && world.FrontOf("warsaw_pact") != nullptr);
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
        // Every place stands on its place of the map, to the bit (F1); every unit with a garrison has its point.
        const std::vector<GroundPlace> named = g.NamedPlaces();
        SC_EXPECT(!named.empty());
        for (const GroundPlace& p : named) {
            if (!At(p, world.places)) std::printf("    %s: place %s not at its point\n", f, p.at.c_str());
            SC_EXPECT(At(p, world.places));
        }
        SC_EXPECT(g.garrisons.size() == g.oob.units.size());
        for (std::size_t i = 0; i < g.oob.units.size(); ++i) SC_EXPECT(g.garrisons[i].at == g.oob.units[i].garrison);
        // Each axis attacks in the sector of its faction's front that its crossing of the border lies in (the route's
        // second place projected on the front's base line, F2.1's LineOfContact).
        const airwar::FrontDef* front = world.FrontOf(f);
        SC_EXPECT(front != nullptr);
        if (front != nullptr) {
            airwar::LineOfContact line;
            line.Build(*front, 5000.0);
            for (const AxisDef& a : g.plan.axes) {
                const int at = line.SectorOf(a.route[1].pos);
                if (at != a.sector_index) std::printf("    %s: axis %s crosses in sector %d, names %s\n", f, a.id.c_str(), at, a.sector.c_str());
                SC_EXPECT(at == a.sector_index && front->sectors[static_cast<std::size_t>(a.sector_index)].id == a.sector);
            }
        }
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

SC_TEST("ground data (F3.1): our defence loads with its order of battle, every combat unit equipped, a deployment per adversary") {
    GroundDefenceDef d;
    std::string e;
    SC_EXPECT(LoadGroundDefence(kData, d, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    // The regiments and the brigade that had no equipment have it in the order of battle itself (F3.1's estimates), and
    // every manoeuvre regiment and brigade of the 3rd Army has a weighted strength.
    const command::OobUnit* mrr = d.oob.Find("mrr_71");
    SC_EXPECT(mrr != nullptr && mrr->EquipmentCount("t55") == 31 && mrr->EquipmentCount("bmp1") == 93);
    for (const char* id : {"mrr_71", "mrr_72", "tr_73", "mrr_91", "mrr_92", "tr_93", "tank_brigade", "sw_brigade"}) {
        const command::OobUnit* u = d.oob.Find(id);
        SC_EXPECT(u != nullptr && StrengthOfUnit(Tables(), *u) > 50.0);
    }
    SC_EXPECT(UnweightedTypes(Tables(), d.oob).size() > 0);  // support and air defence: counted by their people only
    // ČSLA 1985: a division 30-45 km wide in defence; the security zone 20-40 km.
    const FrontageDef* div = d.doctrine.Frontage("division", "defence");
    SC_EXPECT(div != nullptr && div->width_km.lo == 30.0 && div->width_km.hi == 45.0);
    SC_EXPECT(d.doctrine.has_security_zone && d.doctrine.security_zone_depth_km.lo == 20.0 && d.doctrine.security_zone_depth_km.hi == 40.0);
    // Every deployment's run of sectors is its adversary's front's (F2.1), and its places stand on the map (F1).
    const GroundWorld& world = NationWorld();
    for (const char* f : kFactions) {
        const DeploymentDef* dep = d.For(f);
        SC_EXPECT(dep != nullptr);
        if (dep == nullptr) continue;
        const airwar::FrontDef* front = world.FrontOf(f);
        SC_EXPECT(front != nullptr);
        if (front == nullptr) continue;
        const double length_km = LineLengthKm(*front);
        SC_EXPECT(dep->security_zone_km >= d.doctrine.security_zone_depth_km.lo - 10.0);
        double strength = 0.0;
        for (const DeployedUnitDef& u : dep->units) {
            const bool on_line = !u.sectors.empty();
            if (on_line) {
                SC_EXPECT(front->sectors[static_cast<std::size_t>(u.first_sector)].id == u.sectors.front());
                SC_EXPECT(u.last_sector - u.first_sector + 1 == static_cast<int>(u.sectors.size()));
                SC_EXPECT_NEAR(u.from_km * 1000.0, front->sectors[static_cast<std::size_t>(u.first_sector)].from_m, 1e-9);
                SC_EXPECT(u.to_km <= length_km + 1.0);  // a last sector may end just past the line (front_line.hpp)
            }
            SC_EXPECT(u.place.at.empty() || At(u.place, world.places));
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
    const GroundWorld& world = NationWorld();
    // The good files load.
    {
        GroundTables t;
        GroundForceDef g;
        GroundDefenceDef d;
        SC_EXPECT(LoadGroundTablesJson(tables, t) && LoadGroundForceJson(pact, world, g) && LoadGroundDefenceJson(ours, oob, world, d));
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
        {"a route of one place", 1, "\"route\": [\"pact_assembly_rava\", \"pact_rava_gorge\", \"rava_dam\", \"dravec\", \"capital\"]", "\"route\": [\"capital\"]"},
        {"a place the map lacks on a route", 1, "\"pact_rava_gorge\", \"rava_dam\"", "\"bx_road_rava\", \"rava_dam\""},
        {"a garrison the map lacks", 1, "\"garrison\": \"pact_assembly_rava\"", "\"garrison\": \"rava_upper_valley\""},
        {"an objective the map lacks", 1, "\"at\": \"mezovar\"", "\"at\": \"mezovaros\""},
        {"coordinates where a place belongs", 1, "\"at\": \"mezovar\"", "\"east_km\": 90, \"north_km\": 60"},
        {"an axis in a sector the front lacks", 1, "\"sector\": \"tisava\"", "\"sector\": \"tisza\""},
        {"an axis with no sector", 1, ", \"sector\": \"tisava\"", ""},
        {"a force on another place map", 1, "\"place_map\": \"maps/pannonia.json\"", "\"place_map\": \"maps/kesmark_range.json\""},
        {"a force on another theatre", 1, "\"theatre\": \"air_war/theatre_pannonia_nation.json\"", "\"theatre\": \"air_war/theatre_pannonia.json\""},
        {"a force without its place map", 1, "\"place_map\": \"maps/pannonia.json\",", ""},
        {"a broken order of battle", 1, "\"parent\": \"pact_td_9\"", "\"parent\": \"pact_td_99\""},
        {"effectiveness lines the wrong way round", 1, "\"value\": 0.667", "\"value\": 0.4"},
        {"caps the wrong way round", 1, "\"broken\": { \"value\": 70", "\"broken\": { \"value\": 20"},
        {"a commander for an unknown unit", 1, "\"unit\": \"pact_td_9\"", "\"unit\": \"pact_td_99\""},
        {"raids without their block", 2, "\"raids\": {", "\"raidz\": {"},
        {"a deployment of an unknown unit", 3, "{ \"unit\": \"mrd_7\", \"role\": \"first_echelon\"", "{ \"unit\": \"mrd_77\", \"role\": \"first_echelon\""},
        {"a reserve holding a stretch", 3, "{ \"unit\": \"tr_73\", \"role\": \"reserve\", \"at\": \"kesmark\" }",
         "{ \"unit\": \"tr_73\", \"role\": \"reserve\", \"sectors\": [\"hollos\"], \"at\": \"kesmark\" }"},
        {"a reserve with no place", 3, "{ \"unit\": \"tr_73\", \"role\": \"reserve\", \"at\": \"kesmark\" }", "{ \"unit\": \"tr_73\", \"role\": \"reserve\" }"},
        {"a unit on the line with no sector", 3, "\"sectors\": [\"rava_reservoir\"], \"at\": \"dravec\"", "\"at\": \"dravec\""},
        {"sectors out of order", 3, "\"sectors\": [\"rava_reservoir\"], \"at\": \"dravec\"", "\"sectors\": [\"rava_reservoir\", \"northern_forest\"], \"at\": \"dravec\""},
        {"a sector of another front", 3, "\"sectors\": [\"rava_reservoir\"], \"at\": \"dravec\"", "\"sectors\": [\"zlatno\"], \"at\": \"dravec\""},
        {"a deployment place the map lacks", 3, "\"sectors\": [\"rava_reservoir\"], \"at\": \"dravec\"", "\"sectors\": [\"rava_reservoir\"], \"at\": \"dravecz\""},
        {"an adversary with no front", 3, "\"adversary\": \"rebels\"", "\"adversary\": \"martians\""},
        {"the old stretch in km", 3, "\"sectors\": [\"rava_reservoir\"], \"at\": \"dravec\"", "\"from_km\": 75, \"to_km\": 120, \"at\": \"dravec\""},
        {"a unit and its parent both deployed", 3, "{ \"unit\": \"tr_73\", \"role\": \"reserve\"", "{ \"unit\": \"mrd_7\", \"role\": \"reserve\""},
        {"an unknown role", 3, "\"role\": \"flank_screen\"", "\"role\": \"flank_guard\""},
        {"not JSON", 3, "{", "["},
        // F8.1: the national command's reserve stands out of the army's line, under its command, at a place of the map.
        {"a front reserve that is deployed", 3, "\"unit\": \"td_11\"", "\"unit\": \"tank_brigade\""},
        {"a front reserve over a deployed unit", 3, "\"unit\": \"td_11\"", "\"unit\": \"army_3\""},
        {"a front reserve not in the order of battle", 3, "\"unit\": \"td_11\"", "\"unit\": \"td_99\""},
        {"a front reserve not under its command", 3, "\"command\": \"ground_forces\"", "\"command\": \"air_force\""},
        {"a front reserve's home the map lacks", 3, "\"at\": \"cegled_junction\"", "\"at\": \"cegled\""},
        {"a front reserve with no reach", 3, "\"reach_h\": { \"value\": 24,", "\"reach\": { \"value\": 24,"},
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
            ok = LoadGroundDefenceJson(bad, oob, world, d, &e);
        } else {
            GroundForceDef g;
            ok = LoadGroundForceJson(bad, world, g, &e);
        }
        if (ok) std::printf("    not refused: %s\n", c.what);
        SC_EXPECT(!ok && !e.empty());
    }
    // A force file loaded for another faction is refused; a missing file is reported.
    GroundForceDef g;
    std::string e;
    SC_EXPECT(!LoadGroundForce(kData, "martians", g, &e) && e.find("cannot open") != std::string::npos);
    // A place the map lacks is named in the message.
    e.clear();
    GroundForceDef bad;
    SC_EXPECT(!LoadGroundForceJson(With(pact, "\"at\": \"mezovar\"", "\"at\": \"mezovaros\""), world, bad, &e));
    SC_EXPECT(e.find("mezovaros") != std::string::npos && e.find("maps/pannonia.json") != std::string::npos);
}

SC_TEST("ground data (F3.1): a place moved on the map moves what the ground files name there, and nothing else") {
    std::string e;
    MapPlaces places;
    SC_EXPECT(LoadMapPlaces(kData, "maps/pannonia.json", places, &e));
    GroundForceDef before;
    GroundDefenceDef ours_before;
    SC_EXPECT(LoadGroundForce(kData, "warsaw_pact", before, &e) && LoadGroundDefence(kData, ours_before, &e));
    // The Pact's eastern assembly area 2 km east, Mezovar 2 km south.
    SC_EXPECT(places.Move("pact_assembly_east", 2000.0, 0.0) && places.Move("mezovar", 0.0, -2000.0));
    GroundForceDef after;
    GroundDefenceDef ours_after;
    SC_EXPECT(LoadGroundForce(kData, "warsaw_pact", after, &e, &places));
    SC_EXPECT(LoadGroundDefence(kData, ours_after, &e, "campaign/ground_defence.json", &places));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    const std::vector<GroundPlace> a = before.NamedPlaces();
    const std::vector<GroundPlace> b = after.NamedPlaces();
    SC_EXPECT(a.size() == b.size());
    int moved = 0;
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
        const double de = b[i].pos.x - a[i].pos.x;
        const double dn = b[i].pos.y - a[i].pos.y;
        if (a[i].at == "pact_assembly_east") {
            SC_EXPECT(de == 2000.0 && dn == 0.0);
            ++moved;
        } else if (a[i].at == "mezovar") {
            SC_EXPECT(de == 0.0 && dn == -2000.0);
            ++moved;
        } else {
            SC_EXPECT(de == 0.0 && dn == 0.0);
        }
    }
    SC_EXPECT(moved == 2);
    // Our 9th Division's command post against the Pact stands at Mezovar.
    const DeployedUnitDef* d0 = ours_before.For("warsaw_pact")->Find("mrd_9");
    const DeployedUnitDef* d1 = ours_after.For("warsaw_pact")->Find("mrd_9");
    SC_EXPECT(d0 != nullptr && d1 != nullptr && d1->place.pos.y - d0->place.pos.y == -2000.0 && d1->place.pos.x == d0->place.pos.x);
}
