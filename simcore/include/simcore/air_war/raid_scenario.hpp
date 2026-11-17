// simcore/air_war/raid_scenario.hpp — a raid of the enemy air command as a scenario the existing runner plays
// (docs/air_war/07_INTEGRATION.md).
//
// The package's flights that come within `scenario.clip_m` of the player's battery become scenario targets: one per
// aircraft, spread in their formation, entering where their route crosses into the circle, with their route's heights
// (terrain following for low flights, the pop-up behaviour for helicopters with a hide), their speed, size, warning
// receiver (an EM RWR when the scenario has the EM layer) and how they evade (on the lock if they carry a warning
// receiver, on the launch if they only see the missile). Stand-off and self-protection jammers become EM jammers on
// their aircraft. The campaign's state becomes the rest of the scenario: the battery (the launchers it still has, its
// TZM, fuel truck and PU-12), the prepared position and dummies as the player laid them out, the regiment's radars
// that still stand (the PU-12's picture), the warning nets that still work (outages for the destroyed ones), the
// warning the player's side gets (an air_threat trigger), the crew's skill tier, the missiles left in the vehicle,
// night and day and the weather. NPC commanders run the crew and the battery (crew_commander/battery_commander "npc"),
// so the scenario needs no scripted orders; the player takes any seat.
//
// The scenario is JSON text for LoadScenario (validated by the same strict loader as every hand-written scenario),
// and a manifest the mission bridge (mission_bridge.hpp) uses to map the scenario's targets back to the flights.
//
// On a map (ScenarioFormat::map), the battery is set down on the nearest open ground to where the campaign has it, and
// each dummy too: the campaign moves the battery kilometres at a time without seeing woods, villages or the river, and
// a launcher inside a wood is hidden and blind. What the enemy aimed at them moves with them.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/flights.hpp"
#include "simcore/geo.hpp"
#include "simcore/json.hpp"
#include "simcore/session_deliveries.hpp"
#include "simcore/terrain_map.hpp"

namespace simcore {
namespace airwar {

// ------------------------------------------------------------------ a small JSON builder and writer

namespace jsonb {

inline json::Value Obj() {
    json::Value v;
    v.type = json::Type::Object;
    return v;
}
inline json::Value Arr() {
    json::Value v;
    v.type = json::Type::Array;
    return v;
}
inline json::Value Num(double x) {
    json::Value v;
    v.type = json::Type::Number;
    v.number = x;
    return v;
}
inline json::Value Str(const std::string& s) {
    json::Value v;
    v.type = json::Type::String;
    v.string = s;
    return v;
}
inline json::Value Bool(bool b) {
    json::Value v;
    v.type = json::Type::Bool;
    v.boolean = b;
    return v;
}
inline void Put(json::Value& obj, const std::string& key, json::Value v) {
    obj.keys.push_back(key);
    obj.items.push_back(std::move(v));
}
inline void Push(json::Value& arr, json::Value v) { arr.items.push_back(std::move(v)); }

inline void Escape(std::ostringstream& s, const std::string& t) {
    s << '"';
    for (const char c : t) {
        switch (c) {
            case '"': s << "\\\""; break;
            case '\\': s << "\\\\"; break;
            case '\n': s << "\\n"; break;
            case '\t': s << "\\t"; break;
            case '\r': s << "\\r"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char b[8];
                    std::snprintf(b, sizeof(b), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                    s << b;
                } else {
                    s << c;
                }
        }
    }
    s << '"';
}

// precision: significant digits of non-integral numbers (6 for scenarios a person reads; 17 for saves, exact).
inline void Write(std::ostringstream& s, const json::Value& v, int indent, int depth, int precision = 6) {
    const std::string pad(static_cast<std::size_t>(indent * (depth + 1)), ' ');
    const std::string pad0(static_cast<std::size_t>(indent * depth), ' ');
    switch (v.type) {
        case json::Type::Null: s << "null"; break;
        case json::Type::Bool: s << (v.boolean ? "true" : "false"); break;
        case json::Type::Number: {
            const double tol = precision >= 17 ? 0.0 : 1e-9;  // a save keeps every bit
            if (std::fabs(v.number - std::round(v.number)) <= tol && std::fabs(v.number) < 1e15) {
                s << static_cast<long long>(std::llround(v.number));
            } else {
                char b[40];
                std::snprintf(b, sizeof(b), "%.*g", precision, v.number);
                s << b;
            }
            break;
        }
        case json::Type::String: Escape(s, v.string); break;
        case json::Type::Array: {
            if (v.items.empty()) {
                s << "[]";
                break;
            }
            bool flat = true;
            for (const json::Value& x : v.items) flat = flat && x.type != json::Type::Object && x.type != json::Type::Array;
            s << '[';
            for (std::size_t i = 0; i < v.items.size(); ++i) {
                if (i > 0) s << ',';
                if (!flat && indent > 0) s << '\n' << pad;
                else if (i > 0) s << ' ';
                Write(s, v.items[i], indent, depth + 1, precision);
            }
            if (!flat && indent > 0) s << '\n' << pad0;
            s << ']';
            break;
        }
        case json::Type::Object: {
            if (v.keys.empty()) {
                s << "{}";
                break;
            }
            s << '{';
            for (std::size_t i = 0; i < v.keys.size(); ++i) {
                if (i > 0) s << ',';
                if (indent > 0) s << '\n' << pad;
                Escape(s, v.keys[i]);
                s << (indent > 0 ? ": " : ":");
                Write(s, v.items[i], indent, depth + 1, precision);
            }
            if (indent > 0) s << '\n' << pad0;
            s << '}';
            break;
        }
    }
}

inline std::string Dump(const json::Value& v, int indent = 2, int precision = 6) {
    std::ostringstream s;
    Write(s, v, indent, 0, precision);
    s << '\n';
    return s.str();
}

inline json::Value Pos(const Vec3& enu) {
    json::Value p = Obj();
    Put(p, "east_m", Num(std::round(enu.x)));
    Put(p, "north_m", Num(std::round(enu.y)));
    Put(p, "up_m", Num(std::round(enu.z)));
    return p;
}

}  // namespace jsonb

// ------------------------------------------------------------------ the scenario

// The EM library holding the anti-radiation missile classes (em/arm.hpp), added to a raid with EM ARMs.
constexpr const char* kArmLibrary = "em/nato_1985.json";

// The scenario keys this build's loader accepts beyond the original format (roadmap E5 on feature/em-e1-e6: the
// "environment" block and the targets' "rwr").
struct ScenarioFormat {
    bool environment = true;
    bool target_rwr = true;
    bool target_decoy = true;  // decoy drones marked "decoy" (the TV can tell them; roadmap E6)
    bool doctrine = true;      // the crew's "doctrine": the TV check against decoys, search-radar bursts (roadmap E6)
    bool em = true;            // the Osa's radars from the EM layer (own_unit.em)
    bool model = true;         // each aircraft's basic model (data/scenery/basic_models.json: the aircraft's id)
    bool weather = true;       // the day's "weather" (data/weather/presets.json by the air war's clear/cloudy/poor)
    bool target_arms = true;   // SEAD and hunter aircraft fire EM anti-radiation missiles (the targets' "arms", roadmap A11)
    std::string map_file;      // the raid on a map's ground ("terrain": {"type": "map"}, e.g. "maps/kesmark_range.json"); "" flat
    // With map_file, the map itself (LoadMapCached): the battery and its dummies stand on open ground (no wood, building
    // or river under a vehicle) near where the campaign has them. Null: exactly where the campaign has them.
    std::shared_ptr<const MapData> map;
    // Our side's information pipeline seen from the player's vehicle (command::SessionSource; null: none): the raid's
    // warning comes when it reached the vehicle through the chain, and the messages the vehicle's people get during the
    // raid are played in it (roadmap P1). Without it, the warning comes own_intel.warning_s ahead of the first aircraft.
    const DeliverySource* deliveries = nullptr;
};

// One scenario target, and the flight and aircraft it is.
struct RaidAircraft {
    std::string target_id;
    int flight = -1;
    int index = 0;
    FlightRole role = FlightRole::Strike;
    std::string aircraft;
    bool helicopter = false;
    bool drone = false;
    bool has_release = false;
    Vec3 release_enu;  // where it releases its weapons (scenario ENU)
    Vec3 aim_enu;      // where they are aimed
    int arms = 0;
    double arm_range_m = 0.0;
    bool arm_memory = false;
    bool em_arms = false;  // its ARMs are EM missiles the scenario fires (the target's "arms"); else the bridge's model
    double strike_power = 0.0;
    bool officer = false;
    double appear_s = 0.0;
    Profile profile = Profile::Low;  // its flight's (the defences' cover: low, medium)
    double evasion = 0.0;            // the type's (AircraftType::evasion)
    bool missile = false;            // a cruise missile: it strikes where it arrives, and is never a crew lost
    double speed_mps = 0.0;          // the type's cruise speed (a defence can't take what is too fast for it)
    double hit_chance = 0.0;         // a missile's own accuracy (0: strike.hit_chance)
    // Its flight's weapons (Flight::weapon, weapons, delivery: the planner's weaponeering, docs/air_war/10_WEAPONS.md):
    // a weapons.json id ("" none: the type's strike_power), how many on it, how it puts them on the target. The bridge
    // times their fall by them (weapon_flight.hpp) and the presentation feed shows them (fx/events.hpp).
    std::string weapon;
    int weapons = 0;
    Delivery delivery = Delivery::Count;
    std::string arm_weapon;          // the EM library's ARM it carries (AircraftType::ArmEm): the feed's name for it
};

// One of our other air defences whose reach overlaps the scenario. As the raid's aircraft pass through its zone it takes
// its kill chances at them by the abstract resolver's rule (mission_bridge.hpp): a played raid meets the neighbours
// an abstract one would (17.1's first form; they are not drawn yet).
struct RaidNeighbour {
    std::string id;     // theatre object
    std::string name;
    Vec3 enu;           // scenario ENU
    DefenceDef defence;
    double function = 1.0;
    int missiles = -1;  // ready rounds (-1: guns)
};

struct RaidManifest {
    int package_id = 0;
    std::string scenario_id;
    double start_h = 0.0;   // campaign hour at scenario t = 0
    double end_s = 0.0;     // the scenario's time limit
    GeoPoint origin;
    std::string own_object;               // the theatre object the player's vehicle stands for
    Vec3 own_enu;
    Vec3 own_moved;  // how far the battery was set off its campaign position to stand on open ground (east, north, m)
    std::vector<std::pair<std::string, Vec3>> battery;  // the battery's other vehicles (scenario ids) where they stand
    std::vector<std::pair<std::string, Vec3>> dummies;  // theatre dummy ids
    std::vector<std::pair<std::string, Vec3>> objects;  // other theatre objects inside the scenario
    std::vector<RaidAircraft> aircraft;
    std::vector<std::string> flight_ids;  // per flight: its id in the session (flights.hpp), "" if not in it
    std::vector<double> flight_exit_h;   // per flight: when it leaves the scenario's circle (-1e18: never in it)
    std::vector<bool> flight_in;         // per flight: some of it is in the scenario
    std::vector<RaidNeighbour> neighbours;
    // The package against the neighbours' radars and optics (resolve.hpp's rule).
    double sead_supp = 0.0;    // PackageSuppression
    double jam = 0.0;          // attrition.jamming with a jammer, else 0
    double airmanship = 0.5;   // PackageAirmanship
    bool night = false;
    bool poor_weather = false;
};

struct RaidScenario {
    bool ok = false;
    std::string error;
    std::string json_text;
    RaidManifest manifest;
};

class RaidScenarioBuilder {
public:
    RaidScenario Build(const AirCampaign& c, const Package& p, const ScenarioFormat& fmt = ScenarioFormat()) {
        using namespace jsonb;
        RaidScenario out;
        const AirWarData& d = c.Data();
        const TuningTable& tune = d.tuning;
        const FactionDef& f = d.faction;
        const TheatreDef& th = d.theatre;
        const int pi = c.PlayerIndex();
        const TheatreObject& bdef = th.objects[static_cast<std::size_t>(pi)];
        const ObjectState& bst = c.Truth()[static_cast<std::size_t>(pi)];
        const BatteryPosture& posture = c.Posture();
        LocalFrame theatre(th.origin);
        const std::vector<BatterySpot> layout = BatteryLayout(bst.function);
        // On a map: the battery and each dummy on the nearest open ground; what is aimed at them moves with them.
        std::vector<std::pair<std::string, Vec3>> moved;  // theatre object id, how far it was set off (east, north, m)
        Vec3 own_shift;
        if (fmt.map) {
            const auto ground_at = [&](const Vec3& pos) {
                GeoPoint g = theatre.EnuToGeodetic(Vec3{pos.x, pos.y, 0.0});
                g.alt_m = th.ground_height_m;
                return MapTerrain(fmt.map, g);
            };
            std::vector<Vec3> spots{Vec3{}};
            for (const BatterySpot& v : layout) spots.push_back(Vec3{v.e, v.n, 0.0});
            own_shift = OpenGroundShift(ground_at(bst.pos), spots);
            moved.push_back({bdef.id, own_shift});
            for (int di : c.Dummies()) {
                const ObjectState& ds = c.Truth()[static_cast<std::size_t>(di)];
                moved.push_back({th.objects[static_cast<std::size_t>(di)].id, OpenGroundShift(ground_at(ds.pos), {Vec3{}})});
            }
        }
        const auto moved_by = [&](const std::string& id) {
            for (const auto& mv : moved) {
                if (mv.first == id) return mv.second;
            }
            return Vec3{};
        };
        const GeoPoint g0 = theatre.EnuToGeodetic(Vec3{bst.pos.x + own_shift.x, bst.pos.y + own_shift.y, 0.0});
        GeoPoint origin = g0;
        origin.alt_m = th.ground_height_m;
        LocalFrame scen(origin);
        auto to_scen = [&](const Vec3& t) {
            const GeoPoint g = theatre.EnuToGeodetic(Vec3{t.x, t.y, 0.0});
            Vec3 e = scen.GeodeticToEnu(GeoPoint{g.lat_rad, g.lon_rad, th.ground_height_m});
            e.z = 0.0;
            return e;
        };
        const double clip = tune.Get("scenario.clip_m");
        RaidManifest& m = out.manifest;
        m.package_id = p.id;
        m.origin = origin;
        m.own_object = bdef.id;
        m.own_enu = Vec3{0.0, 0.0, 3.0};
        m.own_moved = own_shift;
        m.flight_exit_h.assign(p.flights.size(), -1e18);
        m.flight_in.assign(p.flights.size(), false);
        m.scenario_id = "air_war_" + f.id + "_" + std::to_string(p.id);

        // Each flight's part inside the circle.
        struct Clipped {
            int flight = -1;
            std::vector<Vec3> pts;  // scenario ENU
            std::vector<double> agl;
            std::vector<double> t_h;
            std::vector<std::string> tags;
            std::vector<std::string> leg_tags;  // the tag of the leg flown to each point (an edge's: the leg it cuts)
            bool release = false;
            Vec3 release_pt;
            int release_i = -1;
        };
        std::vector<Clipped> clipped;
        double first_h = 1e18, last_h = -1e18;
        for (std::size_t fi = 0; fi < p.flights.size(); ++fi) {
            const Flight& fl = p.flights[fi];
            if (fl.route.size() < 2) continue;
            Clipped cl;
            cl.flight = static_cast<int>(fi);
            std::vector<Vec3> pts;
            const Vec3 aim_moved = moved_by(fl.aim_key);  // a run at the battery or a dummy follows it to where it stands
            for (const RoutePoint& r : fl.route) pts.push_back(to_scen(r.pos + aim_moved));
            bool inside_prev = Vec3{pts[0].x, pts[0].y, 0.0}.HorizontalLength() < clip;
            if (inside_prev) {
                cl.pts.push_back(pts[0]);
                cl.agl.push_back(fl.route[0].agl_m);
                cl.t_h.push_back(fl.route[0].t_h);
                cl.tags.push_back(fl.route[0].tag);
                cl.leg_tags.push_back(fl.route[0].tag);
            }
            for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
                const Vec3 a = pts[i], b = pts[i + 1];
                const bool inside_b = b.HorizontalLength() < clip;
                // Crossing points of the segment with the circle.
                std::vector<double> ts = CircleCrossings(a, b, clip);
                for (double tc : ts) {
                    const Vec3 x = a + (b - a) * tc;
                    cl.pts.push_back(x);
                    cl.agl.push_back(Lerp(fl.route[i].agl_m, fl.route[i + 1].agl_m, tc));
                    cl.t_h.push_back(Lerp(fl.route[i].t_h, fl.route[i + 1].t_h, tc));
                    cl.tags.push_back("edge");
                    cl.leg_tags.push_back(fl.route[i + 1].tag);
                }
                if (inside_b) {
                    cl.pts.push_back(b);
                    cl.agl.push_back(fl.route[i + 1].agl_m);
                    cl.t_h.push_back(fl.route[i + 1].t_h);
                    cl.tags.push_back(fl.route[i + 1].tag);
                    cl.leg_tags.push_back(fl.route[i + 1].tag);
                    if (static_cast<int>(i + 1) == fl.release_index) {
                        cl.release = true;
                        cl.release_pt = b;
                        cl.release_i = static_cast<int>(cl.pts.size()) - 1;
                    }
                }
                inside_prev = inside_b;
            }
            (void)inside_prev;
            if (cl.pts.size() < 2) continue;
            // A route that leaves and comes back keeps one continuous track from its first entry to its last exit.
            m.flight_in[fi] = true;
            m.flight_exit_h[fi] = cl.t_h.back();
            first_h = std::min(first_h, cl.t_h.front());
            last_h = std::max(last_h, cl.t_h.back());
            clipped.push_back(cl);
        }
        if (clipped.empty()) {
            out.error = "no flight of package " + std::to_string(p.id) + " comes within the scenario circle";
            return out;
        }
        const double prep_s = tune.Get("scenario.prep_s");
        m.start_h = first_h - prep_s / 3600.0;
        // Order: main, SEAD, decoys, recce, jammers, escorts; cut at the target cap (the rest is resolved abstractly).
        auto rank = [&](const Clipped& cl) {
            switch (p.flights[static_cast<std::size_t>(cl.flight)].role) {
                case FlightRole::Strike:
                case FlightRole::Dead:
                case FlightRole::Helicopter:
                case FlightRole::Missile: return 0;
                case FlightRole::Sead:
                case FlightRole::Hunter: return 1;
                case FlightRole::Decoy: return 2;
                case FlightRole::Recce: return 3;
                case FlightRole::Jammer: return 4;
                default: return 5;
            }
        };
        std::stable_sort(clipped.begin(), clipped.end(), [&](const Clipped& a, const Clipped& b) { return rank(a) < rank(b); });
        const int cap = static_cast<int>(tune.Get("scenario.max_targets"));

        json::Value targets = Arr();
        json::Value jammers = Arr();
        std::vector<std::string> libraries;
        int n_targets = 0;
        double end_s = 0.0;
        // The flights and the package as the session keeps them (flights.hpp, roadmap B1).
        const std::string pkg_id = "p" + std::to_string(p.id);
        json::Value flights_json = Arr();
        std::string commander, first_main;  // where an officer flies, else the first main flight (02_FLIGHTS_AND_PILOTS.md §1.2)
        m.flight_ids.assign(p.flights.size(), std::string());
        for (const Clipped& cl : clipped) {
            const Flight& fl = p.flights[static_cast<std::size_t>(cl.flight)];
            const AircraftType* a = f.Aircraft(fl.aircraft);
            if (a == nullptr) continue;
            const int alive = fl.count - fl.lost;
            if (n_targets + alive > cap) {
                m.flight_in[static_cast<std::size_t>(cl.flight)] = false;
                m.flight_exit_h[static_cast<std::size_t>(cl.flight)] = -1e18;
                continue;
            }
            const bool low = fl.profile == Profile::Low || a->helicopter;
            const bool hide = a->helicopter && std::find(cl.tags.begin(), cl.tags.end(), std::string("hide")) != cl.tags.end();
            std::string cs = fl.callsign;
            cs.erase(std::remove(cs.begin(), cs.end(), ' '), cs.end());
            // Its formation by phase from the faction's "flights" block for its type class (a stream of cruise missiles
            // keeps none: each flies its own plan, set off by its slot).
            const std::map<std::string, std::vector<Vec3>> forms = f.FormationsFor(FormationClassOf(a->helicopter, fl.role));
            {
                json::Value fj = Obj();
                Put(fj, "id", Str(cs));
                Put(fj, "callsign", Str(fl.callsign));
                Put(fj, "package", Str(pkg_id));
                if (a->missile) Put(fj, "keep_slots", Bool(false));
                if (a->missile || a->drone) Put(fj, "crewed", Bool(false));  // no nerve: a decoy drone flies on
                if (!forms.empty()) {
                    json::Value fo = Obj();
                    for (const auto& ph : forms) {
                        json::Value sl = Arr();
                        for (const Vec3& v : ph.second) {
                            json::Value tri = Arr();
                            Push(tri, Num(v.x));
                            Push(tri, Num(v.y));
                            Push(tri, Num(v.z));
                            Push(sl, tri);
                        }
                        Put(fo, ph.first, sl);
                    }
                    Put(fj, "formations", fo);
                }
                Push(flights_json, fj);
            }
            m.flight_ids[static_cast<std::size_t>(cl.flight)] = cs;
            if (commander.empty() && fl.officer >= 0 && !a->missile && !a->drone) commander = cs;
            if (first_main.empty() && rank(cl) == 0 && !a->missile && !a->drone) first_main = cs;
            // Each leg's speed from the route's own times (01_ROUTES.md §6.6), the whole route's where a leg has none.
            const double route_len = RouteLength(cl.pts);
            const double dt_s = (cl.t_h.back() - cl.t_h.front()) * 3600.0;
            const double top_mps = std::max(600.0, a->cruise_mps * 1.2);
            const double whole_mps = dt_s > 1.0 ? ClampTo(route_len / dt_s, 20.0, top_mps) : a->cruise_mps;
            std::vector<double> leg_mps(cl.pts.size(), whole_mps);
            for (std::size_t i = 1; i < cl.pts.size(); ++i) {
                const double len = (cl.pts[i] - cl.pts[i - 1]).HorizontalLength();
                const double dt = (cl.t_h[i] - cl.t_h[i - 1]) * 3600.0;
                if (dt > 1.0 && len > 1.0) leg_mps[i] = ClampTo(len / dt, 20.0, top_mps);
            }
            for (int k = 0; k < alive; ++k) {
                // Its place in the formation of each leg's phase, in that leg's frame: where its own plan runs. Only the
                // start is set back by the slot's depth: a turn point or a release point is the leader's, passed by a man
                // in trail when he gets there (set back, he would turn inside the leader and release short).
                const auto shift_at = [&](std::size_t i, bool back) {
                    const std::size_t j = std::max<std::size_t>(1, i);
                    Vec3 dir = cl.pts[j] - cl.pts[j - 1];
                    dir.z = 0.0;
                    dir = dir.LengthSquared() > 1.0 ? dir.Normalized() : Vec3{0.0, 1.0, 0.0};
                    const Vec3 o = FormationSlot(forms, PhaseOfTag(cl.leg_tags[j]), k);
                    const double ob = back ? o.y : 0.0;
                    return Vec3{dir.y * o.x - dir.x * ob, -dir.x * o.x - dir.y * ob, o.z};
                };
                const std::string tid = cs + "-" + std::to_string(k + 1);
                json::Value t = Obj();
                Put(t, "id", Str(tid));
                Put(t, "name", Str(fl.callsign + " " + std::to_string(k + 1) + ": " + a->name + " (" + FlightRoleName(fl.role) + ")"));
                Put(t, "type", Str(a->missile ? "Air+Missile" : (a->helicopter ? "Air+Rotorcraft" : "Air+FixedWing")));
                Put(t, "class", Str(a->target_class));
                if (fmt.model) Put(t, "model", Str(a->id));
                Put(t, "flight", Str(cs));
                Put(t, "slot", Num(k + 1));
                const Vec3 s0 = shift_at(0, true);
                Vec3 start = cl.pts[0] + Vec3{s0.x, s0.y, 0.0};
                start.z = (low ? std::max(5.0, cl.agl[0]) : cl.agl[0]) + s0.z;
                if (low) start.z = std::max(5.0, start.z);
                Put(t, "start", Pos(start));
                json::Value wps = Arr();
                json::Value legs = Arr();
                for (std::size_t i = 1; i < cl.pts.size(); ++i) {
                    json::Value w = Obj();
                    const Vec3 sh = shift_at(i, false);
                    const Vec3 q = cl.pts[i] + Vec3{sh.x, sh.y, 0.0};
                    Put(w, "east_m", Num(std::round(q.x)));
                    Put(w, "north_m", Num(std::round(q.y)));
                    if (low) {
                        Put(w, "agl_m", Num(std::round(std::max(5.0, cl.agl[i] + sh.z))));
                    } else {
                        Put(w, "up_m", Num(std::round(cl.agl[i] + sh.z)));
                    }
                    Push(wps, w);
                    json::Value l = Obj();
                    Put(l, "speed_mps", Num(std::round(leg_mps[i])));
                    Put(l, "tag", Str(cl.leg_tags[i]));
                    Push(legs, l);
                }
                Put(t, "waypoints", wps);
                Put(t, "legs", legs);
                Put(t, "speed_mps", Num(std::round(leg_mps[1])));
                Put(t, "max_turn_rate_deg_s", Num(a->max_turn_deg_s));
                // Its type's flight limits (roadmap R3): it flies by bank-to-turn.
                if (a->max_load_g > 0.0) Put(t, "max_load_g", Num(a->max_load_g));
                if (a->sustained_turn_deg_s > 0.0) Put(t, "sustained_turn_deg_s", Num(a->sustained_turn_deg_s));
                if (a->roll_rate_deg_s > 0.0) Put(t, "roll_rate_deg_s", Num(a->roll_rate_deg_s));
                if (a->max_mps > 0.0) Put(t, "max_mps", Num(a->max_mps));
                if (a->accel_mps2 > 0.0) Put(t, "accel_mps2", Num(a->accel_mps2));
                if (a->climb_mps > 0.0) Put(t, "climb_mps", Num(a->climb_mps));
                Put(t, "rcs_m2", Num(a->rcs_m2));
                const double appear_s = std::max(0.0, (cl.t_h.front() - m.start_h) * 3600.0);
                Put(t, "appear_at_s", Num(std::round(appear_s)));
                Put(t, "board_type", Num(fl.role == FlightRole::Jammer ? 0 : 9));
                Put(t, "formation", Num(alive));
                if (hide) {
                    json::Value b = Obj();
                    Put(b, "kind", Str("pop_up"));
                    Put(b, "hide_agl_m", Num(5));
                    Put(b, "popup_agl_m", Num(25));
                    Put(b, "max_climb_mps", Num(5));
                    Put(b, "first_hold_s", Num(30));
                    Put(b, "expose_s", Num(30));
                    Put(b, "hidden_s", Num(45));
                    Put(b, "cycles", Num(1));
                    Put(t, "behaviour", b);
                } else if (low) {
                    json::Value b = Obj();
                    Put(b, "kind", Str("terrain_following"));
                    Put(b, "agl_m", Num(std::round(std::max(5.0, cl.agl[0]))));
                    Put(b, "max_climb_mps", Num(a->helicopter ? 8 : 120));  // a jet's pop-up climbs at 100 m/s and more
                    Put(b, "lookahead_s", Num(6));
                    Put(t, "behaviour", b);
                }
                const std::string& rwr_em = a->RwrEm(d.theatre.year);
                const bool has_rwr = !rwr_em.empty();
                if (a->seats > 1) Put(t, "seats", Num(a->seats));
                if (!a->drone && !a->missile) {
                    json::Value e = Obj();
                    if (a->helicopter) {
                        Put(e, "on", Str("lock"));
                        Put(e, "kind", Str("dive"));
                    } else {
                        Put(e, "on", Str(has_rwr ? "lock" : "launch"));
                        Put(e, "kind", Str(low ? "jink" : "break"));
                        Put(e, "turn_rate_deg_s", Num(std::min(20.0, a->max_turn_deg_s * 1.5)));
                    }
                    Put(t, "evade", e);
                }
                if (fmt.target_decoy && a->drone) Put(t, "decoy", Bool(true));
                const int arms = a->arms * (fl.role == FlightRole::Sead || fl.role == FlightRole::Hunter ? 1 : 0);
                const std::string& arm_em = a->ArmEm(d.theatre.year);
                const bool em_arms = fmt.em && fmt.target_rwr && fmt.target_arms && has_rwr && arms > 0 && !arm_em.empty();
                if (fmt.em && fmt.target_rwr && has_rwr) {
                    Put(t, "rwr", Str(rwr_em));
                    for (const std::string& lib : f.em_libraries) {
                        if (std::find(libraries.begin(), libraries.end(), lib) == libraries.end()) libraries.push_back(lib);
                    }
                }
                // Its anti-radiation missiles as EM missiles (roadmap A11): fired on what its warning receiver hears, at
                // where its intelligence has the battery (the flight's aim), within its launch range.
                if (em_arms) {
                    json::Value ar = Obj();
                    Put(ar, "weapon", Str(arm_em));
                    Put(ar, "count", Num(arms));
                    Put(ar, "launch_range_km", Num(std::round(a->arm_range_m / 100.0) / 10.0));
                    Put(ar, "min_range_km", Num(std::min(3.0, 0.5 * a->arm_range_m / 1000.0)));
                    Put(ar, "aim", Pos(to_scen(fl.aim_pos + moved_by(fl.aim_key))));
                    Put(t, "arms", ar);
                    if (std::find(libraries.begin(), libraries.end(), kArmLibrary) == libraries.end()) libraries.push_back(kArmLibrary);
                }
                Put(t, "notes", Str("Generated by the enemy air command (simcore/air_war): package " + std::to_string(p.id) + " '" + p.name +
                                    "', " + MissionKindName(p.kind) + ", " + FlightRoleName(fl.role) +
                                    ". Aircraft numbers are PLACEHOLDERS (data/air_war/)."));
                Push(targets, t);
                // Jammers on this aircraft.
                const std::string jam = !a->standoff_jammer_em.empty() ? a->standoff_jammer_em : a->self_jammer_em;
                if (fmt.em && !jam.empty() && (fl.role == FlightRole::Jammer || !a->self_jammer_em.empty())) {
                    json::Value j = Obj();
                    Put(j, "jammer", Str(jam));
                    Put(j, "target", Str(tid));
                    Put(j, "on_at_s", Num(std::round(appear_s)));
                    Push(jammers, j);
                    for (const std::string& lib : f.em_libraries) {
                        if (std::find(libraries.begin(), libraries.end(), lib) == libraries.end()) libraries.push_back(lib);
                    }
                }
                RaidAircraft ra;
                ra.target_id = tid;
                ra.flight = cl.flight;
                ra.index = k;
                ra.role = fl.role;
                ra.aircraft = a->id;
                ra.helicopter = a->helicopter;
                ra.drone = a->drone;
                ra.has_release = cl.release && (fl.role == FlightRole::Strike || fl.role == FlightRole::Dead ||
                                                fl.role == FlightRole::Helicopter || fl.role == FlightRole::Recce ||
                                                fl.role == FlightRole::Missile);
                ra.release_enu = cl.release_pt;
                if (cl.release_i >= 0) {
                    const Vec3 sh = shift_at(static_cast<std::size_t>(cl.release_i), false);
                    ra.release_enu = cl.release_pt + Vec3{sh.x, sh.y, 0.0};
                }
                ra.aim_enu = to_scen(fl.aim_pos + moved_by(fl.aim_key));
                ra.arms = arms;
                ra.arm_range_m = a->arm_range_m;
                ra.arm_memory = a->arm_memory;
                ra.em_arms = em_arms;
                ra.strike_power = a->strike_power * p.load_factor;
                ra.officer = fl.officer >= 0 && k == 0;
                ra.appear_s = appear_s;
                ra.profile = fl.profile;
                ra.evasion = a->evasion;
                ra.missile = a->missile;
                ra.speed_mps = a->cruise_mps;
                ra.hit_chance = a->hit_chance;
                ra.weapon = fl.weapon;
                ra.weapons = fl.weapons;
                ra.delivery = fl.delivery;
                ra.arm_weapon = arm_em;
                m.aircraft.push_back(ra);
                ++n_targets;
                end_s = std::max(end_s, (cl.t_h.back() - m.start_h) * 3600.0);
            }
        }
        m.end_s = end_s + tune.Get("scenario.margin_s");

        // The scenario around them.
        json::Value root = Obj();
        Put(root, "scenario_id", Str(m.scenario_id));
        Put(root, "name", Str(f.short_name + ": " + DescribeForPlayer(p) + " (enemy air command)"));
        Put(root, "description", Str(Briefing(c, p)));
        Put(root, "source", Str("sam3d"));
        Put(root, "confidence", Str("estimate"));
        Put(root, "setting", Str("war"));
        Put(root, "notes", Str("Generated by simcore/air_war/raid_scenario.hpp from campaign day " + std::to_string(c.Day()) +
                               ". Fictional; every aircraft number is a PLACEHOLDER (data/air_war/). The NPC commanders run the crew and the battery."));
        Put(root, "seed", Num(static_cast<double>(1 + (p.id * 7919) % 100000)));
        json::Value og = Obj();
        Put(og, "lat_deg", Num(std::round(origin.LatDeg() * 1e6) / 1e6));
        Put(og, "lon_deg", Num(std::round(origin.LonDeg() * 1e6) / 1e6));
        Put(og, "alt_m", Num(th.ground_height_m));
        Put(root, "origin", og);
        // The raid's clock (roadmap R7): on a dated theatre, the local time at its second 0 and the theatre's date, so the
        // scenario's light is the sun's there and then (light.hpp), as the planner's part was by the sun. An undated theatre
        // (the tuned theatre_pannonia.json) planned by the fixed buckets: its raid keeps the legacy light keys
        // (battery.night, environment.daylight) from the planner's part and no clock, so it plays in the light it was planned in.
        const bool dated = th.day_of_year > 0;
        if (dated) {
            double local = th.start_hour + m.start_h;
            local -= 24.0 * std::floor(local / 24.0);
            Put(root, "clock_start", Str(FormatLocalHour(std::floor(local * 60.0) / 60.0)));
            int month = 1, day = 1;
            if (DateOfDay(th.year, TheatreSky(th).DayOfYearAt(m.start_h * 3600.0), month, day)) {
                char date[16];
                std::snprintf(date, sizeof(date), "%04d-%02d-%02d", th.year, month, day);
                Put(root, "date", Str(date));
            }
            Put(root, "utc_offset_h", Num(th.utc_offset_h));
        }
        const bool legacy_night = p.part == DayPart::Night;
        json::Value terrain = Obj();
        if (fmt.map_file.empty()) {
            Put(terrain, "type", Str("flat"));
            Put(terrain, "height_m", Num(th.ground_height_m));
        } else {  // the map's ground (simcore/terrain_map.hpp), whatever the raid's origin
            Put(terrain, "type", Str("map"));
            Put(terrain, "map", Str(fmt.map_file));
        }
        Put(root, "terrain", terrain);
        if (fmt.weather) Put(root, "weather", Str(WeatherName(c.WeatherNow())));  // winds and cloud (weather.hpp)
        Put(root, "command_post", Str("command/pu12.json"));

        // C2: the warning nets of the theatre, and the regiment's radars that still stand.
        json::Value c2 = Obj();
        Put(c2, "library", Str("command/c2_sources.json"));
        Put(c2, "primary", Str("pu12"));
        json::Value sources = Arr();
        json::Value fallback = Arr();
        json::Value radars = Arr();
        bool pu12_blind = true;
        bool any_regiment = false;
        // The warning nets: of each kind, the nearest that still works within 150 km (else the nearest, out of action):
        // the radar company the battery is nearest, the neighbouring battery on the radio. The rest of a net carries on
        // when one post is lost.
        std::map<std::string, std::size_t> warning;
        for (std::size_t i = 0; i < th.objects.size(); ++i) {
            const TheatreObject& o = th.objects[i];
            if (o.c2_source.empty() || o.c2_source == "pu12" || o.player) continue;
            const ObjectState& st = c.Truth()[i];
            const double dist = to_scen(st.pos).HorizontalLength();
            if (dist >= 150000.0) continue;
            const auto it = warning.find(o.c2_source);
            if (it == warning.end()) {
                warning[o.c2_source] = i;
                continue;
            }
            const ObjectState& best = c.Truth()[it->second];
            const bool works = st.function > 0.3, best_works = best.function > 0.3;
            if ((works && !best_works) || (works == best_works && dist < to_scen(best.pos).HorizontalLength())) it->second = i;
        }
        for (std::size_t i = 0; i < th.objects.size(); ++i) {
            const TheatreObject& o = th.objects[i];
            const ObjectState& st = c.Truth()[i];
            const Vec3 e = to_scen(st.pos);
            if (!o.regiment_radar.empty() && e.HorizontalLength() < 250000.0) {
                any_regiment = true;
                if (st.function > 0.3) {
                    json::Value r = Obj();
                    Put(r, "id", Str(o.id));
                    Put(r, "type", Str(o.regiment_radar));
                    Put(r, "position", Pos(Vec3{e.x, e.y, 2.0}));
                    Push(radars, r);
                    pu12_blind = false;
                }
            }
            const auto chosen = warning.find(o.c2_source);
            if (chosen != warning.end() && chosen->second == i) {
                json::Value s = Obj();
                Put(s, "id", Str(o.id));
                Put(s, "type", Str(o.c2_source));
                Put(s, "name", Str(o.name));
                Put(s, "position", Pos(Vec3{e.x, e.y, 0.0}));
                Put(s, "net", Str("battery_net"));
                if (st.function <= 0.3) {
                    json::Value outs = Arr();
                    json::Value ou = Obj();
                    Put(ou, "from_s", Num(0));
                    Put(ou, "reason", Str("destroyed in an earlier raid"));
                    Push(outs, ou);
                    Put(s, "outages", outs);
                }
                Push(sources, s);
                Push(fallback, Str(o.id));
            }
            if (o.kind != ObjectKind::City && !o.player && !o.decoy && e.HorizontalLength() < clip) m.objects.push_back({o.id, e});
            if (o.defence.present && !o.player && !o.decoy && st.function > 0.05 && st.missiles != 0 &&
                e.HorizontalLength() < clip + o.defence.radius_m) {
                RaidNeighbour nb;
                nb.id = o.id;
                nb.name = o.name;
                nb.enu = e;
                nb.defence = o.defence;
                nb.function = st.function;
                nb.missiles = st.missiles;
                m.neighbours.push_back(nb);
            }
        }
        m.sead_supp = PackageSuppression(p, c.Staff(), tune);
        m.jam = p.Has(FlightRole::Jammer) ? tune.Get("attrition.jamming") : 0.0;
        m.airmanship = PackageAirmanship(p, c.Staff());
        m.night = p.part == DayPart::Night;
        m.poor_weather = c.WeatherNow() == Weather::Poor;
        if (!fallback.items.empty()) Put(c2, "fallback_order", fallback);
        if (!sources.items.empty()) Put(c2, "sources", sources);
        if (!radars.items.empty()) {
            json::Value reg = Obj();
            Put(reg, "library", Str("command/regiment_radars.json"));
            Put(reg, "radars", radars);
            Put(c2, "regiment", reg);
        }
        Put(root, "c2", c2);

        // The battery: the launchers it still has, its support vehicles, in combat formation.
        json::Value battery = Obj();
        Put(battery, "share_picture", Bool(true));
        Put(battery, "deployed", Bool(true));
        if (!dated) Put(battery, "night", Bool(legacy_night));
        json::Value own = Obj();
        Put(own, "id", Str("21"));
        Put(own, "callsign", Str("21"));
        Put(battery, "own", own);
        json::Value vehicles = Arr();
        for (const BatterySpot& v : layout) {
            const std::string vid = v.id;
            json::Value veh = Obj();
            Put(veh, "id", Str(vid));
            Put(veh, "kind", Str(v.kind));
            Put(veh, "position", Pos(Vec3{v.e, v.n, 3.0}));
            Push(vehicles, veh);
            m.battery.push_back({vid, Vec3{v.e, v.n, 3.0}});
        }
        Put(battery, "vehicles", vehicles);
        if (posture.fight_silent) {
            json::Value orders = Arr();
            json::Value o = Obj();
            Put(o, "at_s", Num(0));
            Put(o, "order", Str("emcon"));
            Put(o, "radiate", Bool(false));
            Push(orders, o);
            Put(battery, "orders", orders);
        }
        Put(root, "battery", battery);

        // Our vehicle.
        json::Value unit = Obj();
        Put(unit, "console", Str("procedures/osa_ak.json"));
        Put(unit, "tuning", Str("gameplay/osa_ak.json"));
        if (fmt.em) {
            json::Value em = Obj();
            Put(em, "library", Str("em/osa_ak.json"));
            Put(em, "search_radar", Str("osa_ak_soc"));
            Put(em, "track_radar", Str("osa_ak_ssc"));
            if (!libraries.empty()) {
                json::Value libs = Arr();
                for (const std::string& l : libraries) Push(libs, Str(l));
                Put(em, "extra_libraries", libs);
            }
            if (!jammers.items.empty()) Put(em, "jammers", jammers);
            if (c.WeatherNow() == Weather::Poor) Put(em, "rain_mmh", Num(4));
            Put(unit, "em", em);
        }
        Put(unit, "position", Pos(m.own_enu));
        Put(unit, "heading_deg", Num(35));
        json::Value inputs = Obj();
        Put(inputs, "vehicle_stopped", Bool(true));
        Put(unit, "inputs", inputs);
        Put(unit, "readiness", Str("deployed"));
        // Missiles left in our vehicle: the battery's share of its stock.
        if (bdef.defence.missiles > 0) {
            const int per_vehicle = static_cast<int>(std::ceil(6.0 * static_cast<double>(std::max(0, bst.missiles)) / bdef.defence.missiles));
            const char* order[] = {"3-II", "2-II", "1-II", "3-I", "2-I", "1-I"};
            const int empty = 6 - std::min(6, per_vehicle);
            if (empty > 0 && empty < 6) {
                json::Value ec = Arr();
                for (int k = 0; k < empty; ++k) Push(ec, Str(order[k]));
                Put(unit, "empty_containers", ec);
            }
        }
        json::Value prep = Obj();
        Put(prep, "emplacement", Bool(posture.emplacement));
        Put(prep, "nets", Bool(posture.nets));
        Put(prep, "wire", Bool(true));
        json::Value dummies = Arr();
        for (int di : c.Dummies()) {
            const ObjectState& ds = c.Truth()[static_cast<std::size_t>(di)];
            if (ds.function <= 0.0) continue;
            const Vec3 e = to_scen(ds.pos + moved_by(th.objects[static_cast<std::size_t>(di)].id));
            json::Value dm = Obj();
            Put(dm, "position", Pos(e));
            if (!posture.decoy_mode.empty() && fmt.em) Put(dm, "decoy", Str(posture.decoy_mode));
            Push(dummies, dm);
            m.dummies.push_back({th.objects[static_cast<std::size_t>(di)].id, e});
        }
        if (!dummies.items.empty()) Put(prep, "dummies", dummies);
        Put(unit, "prepared_position", prep);
        Put(root, "own_unit", unit);

        // The crew, at the tier the campaign has brought it to.
        json::Value crew = Arr();
        const char* seats[][2] = {{"Commander", "commander"}, {"Deputy commander", "deputy_commander"},
                                  {"Senior operator", "senior_operator"}, {"Operator", "operator"}, {"Driver", "driver"}};
        for (const auto& s : seats) {
            json::Value cm = Obj();
            Put(cm, "name", Str(s[0]));
            Put(cm, "role", Str(s[1]));
            Put(cm, "skill", Str(c.CrewTier()));
            Push(crew, cm);
        }
        Put(root, "crew", crew);
        Put(root, "crew_commander", Str("npc"));
        Put(root, "battery_commander", Str("npc"));
        Put(root, "targets", targets);
        // The flights and their package (flights.hpp, roadmap B1): the package turns back on the losses its commander
        // knows of, at the abstract resolver's nerve (PackageNerve), counting those it lost before it came in.
        if (!flights_json.items.empty()) {
            Put(root, "flights", flights_json);
            int lost_before = 0;
            for (const Flight& fl : p.flights) lost_before += fl.role == FlightRole::Missile || fl.role == FlightRole::Decoy ? 0 : fl.lost;
            json::Value pk = Obj();
            Put(pk, "id", Str(pkg_id));
            const std::string cmd = !commander.empty() ? commander : first_main;
            if (!cmd.empty()) Put(pk, "commander", Str(cmd));
            Put(pk, "size", Num(std::max(1, p.Aircraft())));
            Put(pk, "lost_before", Num(lost_before));
            Put(pk, "turn_back_share", Num(std::round(PackageNerve(p, c.Staff()) * 1000.0) / 1000.0));
            json::Value pks = Arr();
            Push(pks, pk);
            Put(root, "packages", pks);
        }
        // Our other air defences round the battery: drawn where they stand, plotted on the board. Their fire is the
        // bridge's, except the regiment's other Osa batteries, which fight in the runner's battery fire model with the
        // launchers that work and the missiles they have.
        if (!m.neighbours.empty()) {
            json::Value nbs = Arr();
            for (const RaidNeighbour& nb : m.neighbours) {
                json::Value v = Obj();
                Put(v, "id", Str(nb.id));
                Put(v, "name", Str(nb.name));
                Put(v, "system", Str(nb.defence.system));
                Put(v, "position", Pos(Vec3{nb.enu.x, nb.enu.y, 0.0}));
                Put(v, "radius_km", Num(nb.defence.radius_m / 1000.0));
                const int launchers = static_cast<int>(std::lround(4.0 * nb.function));
                if (nb.defence.system == "osa" && launchers >= 1 && nb.missiles != 0) {
                    Put(v, "fights", Bool(true));
                    Put(v, "launchers", Num(std::min(4, launchers)));
                    if (nb.missiles > 0) Put(v, "missiles", Num(std::min(24, nb.missiles)));
                }
                Push(nbs, v);
            }
            Put(root, "neighbours", nbs);
        }

        // The warning our side gets, and the PU-12 without its radars.
        json::Value triggers = Arr();
        double warn_s = tune.Get("own_intel.warning_s");
        if (const Officer* pln = p.planner >= 0 ? &c.Staff().officers[static_cast<std::size_t>(p.planner)] : nullptr) {
            if (pln->Quirk("comsec_leak") > 0.0) warn_s = std::max(warn_s, tune.Get("own_intel.leak_warning_s"));
        }
        const double first_s = std::max(0.0, (first_h - m.start_h) * 3600.0);
        double warn_after_s = std::max(0.0, first_s - warn_s);
        std::vector<DeliveryDef> messages;
        if (fmt.deliveries != nullptr) {
            // From the pipeline: played as a message when it comes inside the raid; known from the start when it came
            // before; no warning at all when it never reached the vehicle.
            const double end_h = m.start_h + m.end_s / 3600.0;
            messages = fmt.deliveries->Between(p.id, m.start_h, end_h);
            bool said = false;
            for (const DeliveryDef& msg : messages) said = said || msg.air_warning;
            const double warned_h = fmt.deliveries->AirWarnedAt(p.id, end_h);
            warn_after_s = said || warned_h < -1e17 || warned_h > end_h ? -1.0 : std::max(0.0, (warned_h - m.start_h) * 3600.0);
        }
        if (warn_after_s >= 0.0) {
            json::Value warn = Obj();
            Put(warn, "id", Str("air_raid_warning"));
            json::Value when = Obj();
            Put(when, "after_s", Num(std::round(warn_after_s)));
            Put(warn, "when", when);
            json::Value doo = Arr();
            json::Value act = Obj();
            Put(act, "air_threat", Bool(true));
            Push(doo, act);
            Put(warn, "do", doo);
            Push(triggers, warn);
        }
        if (!messages.empty()) {
            json::Value arr = Arr();
            for (const DeliveryDef& msg : messages) {
                json::Value v = Obj();
                Put(v, "id", Str(msg.id));
                Put(v, "after_s", Num(std::round(msg.after_s * 10.0) / 10.0));
                if (!msg.to.empty()) Put(v, "to", Str(msg.to));
                Put(v, "means", Str(DeliveryMeansName(msg.means)));
                if (!msg.net.empty()) Put(v, "net", Str(msg.net));
                Put(v, "from", Str(msg.from));
                Put(v, "words", Str(msg.words));
                if (!msg.paper_title.empty()) {
                    json::Value paper = Obj();
                    Put(paper, "title", Str(msg.paper_title));
                    Put(paper, "text", Str(msg.paper_text));
                    Put(v, "paper", paper);
                }
                if (msg.air_warning) {
                    Put(v, "air_warning", Bool(true));
                    // The raid's direction from the battery, as the warning gives it (roadmap P4: into the hearers' picture).
                    const Vec3 from = clipped.front().pts[0] - m.own_enu;
                    double az = RadToDeg(std::atan2(from.x, from.y));
                    if (az < 0.0) az += 360.0;
                    Put(v, "azimuth_deg", Num(std::fmod(std::round(az), 360.0)));
                }
                if (Carried(msg.means)) Put(v, "from_bearing_deg", Num(std::round(msg.from_bearing_deg)));
                Push(arr, v);
            }
            Put(root, "deliveries", arr);
        }
        if (any_regiment && pu12_blind) {
            json::Value tr = Obj();
            Put(tr, "id", Str("pu12_blind"));
            json::Value w2 = Obj();
            Put(w2, "after_s", Num(0));
            Put(tr, "when", w2);
            json::Value d2 = Arr();
            json::Value a2 = Obj();
            Put(a2, "c2_fail", Str("pu12"));
            Put(a2, "reason", Str("the regiment's radars are destroyed"));
            Push(d2, a2);
            Put(tr, "do", d2);
            Push(triggers, tr);
        }
        Put(root, "triggers", triggers);
        // The crew's orders from above, from what our side knows and the battery's posture: warned of decoys once our
        // intelligence has seen the enemy use them; the ARM drill once our radars have come under ARM attack; search in
        // bursts when the battery keeps emission discipline. (The regiment's standing order to relocate after an ARM
        // attack needs a battery that drives the vehicle, which a raid's battery, deployed in place, doesn't.)
        if (fmt.doctrine && (c.DecoysExpected() || c.ArmsExpected() || posture.emcon_discipline >= 0.6 || posture.emission < 0.2)) {
            json::Value doc = Obj();
            if (c.DecoysExpected()) Put(doc, "tv_check", Bool(true));
            // The emission regime the combat order gives (docs/crew/16_EMISSION.md): the regiment's least emission once
            // ARMs are expected, else the battery commander's bursts (2 turns on; off 10-40 s by the posture, in turns at
            // the Osa's 33 rpm).
            if (c.ArmsExpected() || posture.emcon_discipline >= 0.6 || posture.emission < 0.2) {
                json::Value em = Obj();
                if (c.ArmsExpected()) {
                    Put(em, "regime", Str("arm"));
                    Put(em, "from", Str("regiment"));
                } else {
                    Put(em, "regime", Str("limited"));
                    Put(em, "on_turns", Num(2));
                    Put(em, "off_turns", Num(std::max(2.0, std::round((10.0 + 30.0 * posture.emcon_discipline) * 33.0 / 60.0))));
                }
                Put(doc, "emission", em);
            }
            Put(doc, "notes", Str("From the campaign: our intelligence's warning of decoys, the ARM drill once our radars have been "
                                  "attacked by ARMs, and the battery's emission discipline."));
            Put(root, "doctrine", doc);
        }
        if (fmt.environment) {
            json::Value env = Obj();
            const Weather w = c.WeatherNow();
            Put(env, "visibility_km", Num(w == Weather::Clear ? 40 : (w == Weather::Cloudy ? 20 : 6)));
            if (!dated) Put(env, "daylight", Bool(!legacy_night));
            Put(root, "environment", env);
        }
        json::Value end = Obj();
        Put(end, "time_limit_s", Num(std::round(m.end_s)));
        Put(end, "when_targets_gone", Bool(true));
        Put(root, "end", end);
        out.json_text = Dump(root);
        out.ok = true;
        return out;
    }

    // What the player is told: the warning and what our side knows, never the enemy's reasons.
    static std::string DescribeForPlayer(const Package& p) {
        int fixed = 0, helos = 0;
        for (const Flight& fl : p.flights) {
            if (fl.role == FlightRole::Helicopter) helos += fl.count;
            else fixed += fl.count;
        }
        std::string s = "raid from " + p.entry + " at " + DayPartName(p.part);
        if (fixed > 0) s += ", " + std::to_string(fixed) + " aircraft";
        if (helos > 0) s += ", " + std::to_string(helos) + " helicopters";
        return s;
    }

    static std::string Briefing(const AirCampaign& c, const Package& p) {
        std::string s = "Generated by the enemy air command, campaign day " + std::to_string(c.Day()) + ", " +
                        DayPartName(p.part) + ", weather " + WeatherName(c.WeatherNow()) + ". The warning net reports a raid coming in from " +
                        p.entry + ". The battery is deployed in its prepared position; the NPC commanders run the crew and the battery. " +
                        "Take a seat, or command the vehicle.";
        const double k = c.EnemyKnowledgeOfBattery();
        if (k > 0.6) s += " Intelligence believes the enemy knows the battery's position.";
        return s;
    }

private:
    // The battery in combat formation around the player's vehicle (scenario east and north, m): the launchers it still
    // has, its TZM, fuel truck and PU-12.
    struct BatterySpot {
        const char* id;
        const char* kind;
        double e, n;
    };
    static std::vector<BatterySpot> BatteryLayout(double function) {
        const BatterySpot all[] = {{"22", "launcher", 200, 0},   {"23", "launcher", 3000, 0},   {"24", "launcher", 3200, 0},
                                   {"TZM", "reload", 1600, -500}, {"ATZ", "fuel", 1700, -700}, {"PU-12", "command_post", 1600, -1000}};
        std::vector<BatterySpot> out;
        for (const BatterySpot& v : all) {
            const std::string id = v.id;
            if ((id == "24" && function < 0.75) || (id == "23" && function < 0.5)) continue;
            out.push_back(v);
        }
        return out;
    }

    // Open ground for a vehicle: nothing taller than kOpenObstacleM (a house, a wood; a revetment's earth wall is lower)
    // at the spot or kOpenClearM round it, and no river within kOpenClearM. A position is moved at most kOpenSearchM.
    static constexpr double kOpenObstacleM = 3.0;
    static constexpr double kOpenClearM = 20.0;
    static constexpr double kOpenSearchM = 2500.0;
    static constexpr double kOpenStepM = 100.0;

    // The smallest move (in rings kOpenStepM apart) that puts every spot (east and north in `ground`'s frame, m) on open
    // ground; none within kOpenSearchM: no move.
    static Vec3 OpenGroundShift(const MapTerrain& ground, const std::vector<Vec3>& spots) {
        struct WaterSeg {
            double ae, an, be, bn, half_width_m;
        };
        double reach = 0.0;
        for (const Vec3& s : spots) reach = std::max(reach, s.HorizontalLength());
        reach += kOpenSearchM + kOpenClearM;
        const MapPoint centre = ground.Link().FrameToMap(0.0, 0.0);
        std::vector<WaterSeg> water;
        for (const MapStrip& st : ground.Map().Layout().strips) {
            if (st.surface != MapSurface::Water) continue;
            for (std::size_t i = 1; i < st.points.size(); ++i) {
                const Vec3& a = st.points[i - 1];
                const Vec3& b = st.points[i];
                if (mapgeo::DistToSegment(centre.east_m, centre.north_m, a.x, a.y, b.x, b.y) < reach + st.width_m) {
                    water.push_back({a.x, a.y, b.x, b.y, 0.5 * st.width_m});
                }
            }
        }
        const auto open = [&](double e, double n) {
            const double k = kOpenClearM, d = 0.7071 * kOpenClearM;
            const double ring[9][2] = {{0.0, 0.0}, {k, 0.0}, {-k, 0.0}, {0.0, k}, {0.0, -k}, {d, d}, {d, -d}, {-d, d}, {-d, -d}};
            // Every footprint within kOpenClearM, whole (roadmap R6's review: a house between the ring's points is not
            // missed); the ring for the clutter of the towns the map does not lay out.
            if (ground.ObstacleHeightWithin(e, n, kOpenClearM) > kOpenObstacleM) return false;
            for (const auto& o : ring) {
                if (ground.ObstacleHeightAt(e + o[0], n + o[1]) > kOpenObstacleM) return false;
            }
            const MapPoint p = ground.Link().FrameToMap(e, n);
            for (const WaterSeg& w : water) {
                if (mapgeo::DistToSegment(p.east_m, p.north_m, w.ae, w.an, w.be, w.bn) < w.half_width_m + kOpenClearM) return false;
            }
            return true;
        };
        for (double r = 0.0; r <= kOpenSearchM; r += kOpenStepM) {
            const int count = r <= 0.0 ? 1 : std::max(8, static_cast<int>(std::ceil(2.0 * kPi * r / kOpenStepM)));
            for (int i = 0; i < count; ++i) {
                const double a = 2.0 * kPi * i / count;
                const Vec3 shift{r * std::sin(a), r * std::cos(a), 0.0};
                bool fits = true;
                for (const Vec3& s : spots) {
                    if (!open(s.x + shift.x, s.y + shift.y)) {
                        fits = false;
                        break;
                    }
                }
                if (fits) return shift;
            }
        }
        return Vec3{};
    }

    static std::vector<double> CircleCrossings(const Vec3& a, const Vec3& b, double r) {
        std::vector<double> out;
        const double dx = b.x - a.x, dy = b.y - a.y;
        const double qa = dx * dx + dy * dy;
        if (qa <= 0.0) return out;
        const double qb = 2.0 * (a.x * dx + a.y * dy);
        const double qc = a.x * a.x + a.y * a.y - r * r;
        const double disc = qb * qb - 4.0 * qa * qc;
        if (disc <= 0.0) return out;
        const double s = std::sqrt(disc);
        for (double t : {(-qb - s) / (2.0 * qa), (-qb + s) / (2.0 * qa)}) {
            if (t > 1e-6 && t < 1.0 - 1e-6) out.push_back(t);
        }
        return out;
    }
    static double RouteLength(const std::vector<Vec3>& pts) {
        double s = 0.0;
        for (std::size_t i = 0; i + 1 < pts.size(); ++i) s += (pts[i + 1] - pts[i]).HorizontalLength();
        return s;
    }
};

}  // namespace airwar
}  // namespace simcore
