// simcore/battery.hpp — the battery around the player's vehicle (roadmap 7.5): its other vehicles and how they move.
//
// An Osa-AK battery was the battery commander's PU-12M, two platoons of two 9A33 and a TZM, and a maintenance vehicle
// (flak11). The battery tactics below are the Soviet 1991 textbook's (voenizdat_osa_1991 ch. 6, pp. 231-239; they
// confirm the unnamed manual retold in `video_osa`), with numbers in data/command/battery_tactics.json:
//   * March: one column, 25-50 m between vehicles, at least 200 m between launchers when an air attack is expected;
//     ready at any moment to stop for a short-halt engagement.
//   * Short halt: a target beyond 20 km, each vehicle pulls 30-50 m off the road to the side away from it; within 20 km
//     it stops in the column (p254; mon_2112_80 p66). Without the target's position the vehicles alternate sides.
//   * Combat formation (advance, pursuit): launchers in pairs up to 3 km apart, at least 200 m within a pair, 1.5-3 km
//     behind the covered unit; the reload vehicles within 500 m of the pairs.
//   * Halts of up to 1 h every 3-4 h for checks, fuel and rest, the column kept in order.
// The model moves the NPC vehicles; the player's own vehicle is a member too, and the battery gives it the same goal
// (its slot in the column or the formation) whether the player or the battery drives it.
//
// Positions are ENU on flat ground (z is kept as given); a vehicle moves straight at its speed toward its goal.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/emission_regime.hpp"
#include "simcore/geo.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

struct BatteryTactics {
    double march_spacing_m = 40.0;
    double threat_spacing_m = 400.0;
    double march_speed_mps = 8.3;
    double cross_country_speed_mps = 5.5;
    double short_halt_offset_m = 40.0;
    double short_halt_in_column_m = 0.0;  // a target nearer than this: stop in the column, not off the road (0: off)
    double deploy_s = 240.0;
    double pair_separation_m = 3000.0;
    double pair_internal_m = 200.0;
    double behind_covered_m = 2000.0;
    double reload_behind_m = 500.0;
    double command_post_behind_m = 1000.0;
    double halt_interval_s = 3.5 * 3600.0;
    double halt_interval_max_s = 4.0 * 3600.0;  // a march plan: the longest movement without a halt
    double halt_duration_s = 1.5 * 3600.0;
    double night_halt_s = 3600.0;
    double threat_range_m = 50000.0;
    double warning_s = 0.0;       // the launchers' warm-up: a track that reaches the zone within it raises readiness (0: off)
    double zone_edge_m = 10000.0; // the far edge of the launchers' zone, where that time is counted to
    double water_speed_mps = 2.2;
    double river_standoff_m = 2000.0;
    double ambush_dwell_s = 1200.0;
    double alongside_offset_m = 8.0;
    double tzm_capacity = 12.0;
    double fuel_truck_capacity_l = 3800.0;
    double slot_tolerance_m = 15.0;
    double breakdown_per_100km = 0.0;  // a fault on the march, each vehicle, per 100 km (0: none; G3.5, a fixed chance while G6 is on hold)
    double repair_s = 1800.0;          // a fault on the march repaired where it stands (by the maintenance vehicle when it has one)
};

enum class BatteryVehicleKind { Launcher, Reload, CommandPost, Maintenance, Fuel };

inline const char* BatteryVehicleKindName(BatteryVehicleKind k) {
    switch (k) {
        case BatteryVehicleKind::Launcher: return "launcher";
        case BatteryVehicleKind::Reload: return "reload";
        case BatteryVehicleKind::CommandPost: return "command_post";
        case BatteryVehicleKind::Maintenance: return "maintenance";
        case BatteryVehicleKind::Fuel: return "fuel";
    }
    return "?";
}

inline bool ParseBatteryVehicleKind(const std::string& s, BatteryVehicleKind& out) {
    for (BatteryVehicleKind k : {BatteryVehicleKind::Launcher, BatteryVehicleKind::Reload, BatteryVehicleKind::CommandPost,
                                 BatteryVehicleKind::Maintenance, BatteryVehicleKind::Fuel}) {
        if (s == BatteryVehicleKindName(k)) {
            out = k;
            return true;
        }
    }
    return false;
}

struct BatteryVehicleDef {
    std::string id;
    std::string callsign;
    BatteryVehicleKind kind = BatteryVehicleKind::Launcher;
    bool own = false;  // the player's vehicle (the scenario's own_unit)
    Vec3 start;
    // A launcher's sector of fire (true azimuth, clockwise from `sector_from_deg` to `sector_to_deg`): in free fire it
    // takes targets only there, and the battery commander gives it targets there first (docs/crew/07 §3).
    bool has_sector = false;
    double sector_from_deg = 0.0;
    double sector_to_deg = 0.0;
    double stock = -1.0;  // a TZM's missiles (a fuel truck's litres) at the start; < 0: the tactics' full load
    int missiles = -1;    // a launcher's missiles at the start (a campaign's session resumed: track O, O1); < 0: a full load
};

// A point of a march plan on the column's route (nav/march.hpp): the start point, a halt, a regulation point, a river
// swum, the end. The battery commander notes the head and the tail passing it; the head does not pass it before its time
// (the regulation points keep the column to the plan's times: voenizdat_osa_1991 p239).
struct BatteryMarchPoint {
    std::string kind;  // "start", "halt", "regulation", "swim", "end"
    std::string name;
    double arc_m = 0.0;          // along the route from its first point
    double not_before_s = -1.0;  // the head passes it no earlier (< 0: no time)
    double halt_s = 0.0;         // the column stops there this long (a halt; a river swum, vehicle by vehicle)
};

struct BatteryOrder {
    enum class Kind { March, ShortHalt, CombatFormation, Halt, Resume, Ambush, RiverCrossing, FarBankHeld, Reload, Refuel, Emcon, Relocate, Emission };
    Kind kind = Kind::March;
    double at_s = 0.0;
    std::vector<Vec3> route;  // March: the road, first point to last. Ambush: the prepared points
    std::string to_place;     // March: where to, by a place's id (an anchor of the route service): the route is planned on
                              // the roads when the order is given (roadmap G2.3, nav/session.hpp)
    Vec3 covered;             // CombatFormation: the covered unit's position. RiverCrossing: the crossing point (near bank)
                              // Relocate: where the own vehicle goes (the others keep their places around it)
    double axis_deg = 0.0;    // direction of advance, or across the river (true, clockwise from north)
    bool bounding = false;    // CombatFormation: the pairs move one at a time, the others staying deployed
    bool roving = false;      // Ambush: the pairs move on to the next point every ambush_dwell_s
    double width_m = 100.0;   // RiverCrossing: the river's width
    std::string vehicle;      // Reload, Refuel: the vehicle served ("" : the player's)
    bool radiate = false;     // Emcon: the radars may radiate again (false: radiation off, the battery fights silent)
    EmissionRegime regime;    // Emission: the battery's emission regime (emission_regime.hpp)
    std::string from;         // "regiment": the regiment's order, passed on by the battery commander ("" or "battery": his own)
    // ShortHalt: the target the halt is for, as the battery commander knows it: its range (< 0: not known) and, if
    // known, its true azimuth from the battery.
    double threat_range_m = -1.0;
    bool threat_has_azimuth = false;
    double threat_az_deg = 0.0;
    // March by a march plan (nav/march.hpp): the column's speed from each route point to the next, the plan's points (its
    // halts replace the halt every halt_interval_s), the order of march by vehicle id. Empty: the tactics' march speed,
    // the scheduled halts, the vehicles as listed.
    std::vector<double> route_mps;
    std::vector<BatteryMarchPoint> points;
    std::vector<std::string> order;
};

inline const char* BatteryOrderName(BatteryOrder::Kind k) {
    switch (k) {
        case BatteryOrder::Kind::March: return "march";
        case BatteryOrder::Kind::ShortHalt: return "short_halt";
        case BatteryOrder::Kind::CombatFormation: return "combat_formation";
        case BatteryOrder::Kind::Halt: return "halt";
        case BatteryOrder::Kind::Resume: return "resume";
        case BatteryOrder::Kind::Ambush: return "ambush";
        case BatteryOrder::Kind::RiverCrossing: return "river_crossing";
        case BatteryOrder::Kind::FarBankHeld: return "far_bank_held";
        case BatteryOrder::Kind::Reload: return "reload";
        case BatteryOrder::Kind::Refuel: return "refuel";
        case BatteryOrder::Kind::Emcon: return "emcon";
        case BatteryOrder::Kind::Relocate: return "relocate";
        case BatteryOrder::Kind::Emission: return "emission";
    }
    return "?";
}

inline bool ParseBatteryOrder(const std::string& s, BatteryOrder::Kind& out) {
    for (BatteryOrder::Kind k : {BatteryOrder::Kind::March, BatteryOrder::Kind::ShortHalt, BatteryOrder::Kind::CombatFormation,
                                 BatteryOrder::Kind::Halt, BatteryOrder::Kind::Resume, BatteryOrder::Kind::Ambush,
                                 BatteryOrder::Kind::RiverCrossing, BatteryOrder::Kind::FarBankHeld, BatteryOrder::Kind::Reload,
                                 BatteryOrder::Kind::Refuel, BatteryOrder::Kind::Emcon, BatteryOrder::Kind::Relocate,
                                 BatteryOrder::Kind::Emission}) {
        if (s == BatteryOrderName(k)) {
            out = k;
            return true;
        }
    }
    return false;
}

namespace detail {

class BatteryTacticsLoader : public ConsoleLoader {
public:
    bool Load(const json::Value& root, BatteryTactics& t) {
        if (!Keys(root, "", {"tactics_id", "display_name", "status", "notes", "tactics"})) return false;
        const json::Value* b = root.Find("tactics");
        if (b == nullptr || !b->Is(json::Type::Object)) return Fail("tactics", "required, an object");
        const std::pair<const char*, double*> fields[] = {
            {"march_spacing_m", &t.march_spacing_m},       {"threat_spacing_m", &t.threat_spacing_m},
            {"march_speed_mps", &t.march_speed_mps},       {"cross_country_speed_mps", &t.cross_country_speed_mps},
            {"short_halt_offset_m", &t.short_halt_offset_m}, {"deploy_s", &t.deploy_s},
            {"pair_separation_m", &t.pair_separation_m},   {"pair_internal_m", &t.pair_internal_m},
            {"behind_covered_m", &t.behind_covered_m},     {"reload_behind_m", &t.reload_behind_m},
            {"command_post_behind_m", &t.command_post_behind_m}, {"halt_interval_s", &t.halt_interval_s},
            {"halt_duration_s", &t.halt_duration_s},       {"night_halt_s", &t.night_halt_s},
            {"threat_range_m", &t.threat_range_m},         {"water_speed_mps", &t.water_speed_mps},
            {"river_standoff_m", &t.river_standoff_m},     {"ambush_dwell_s", &t.ambush_dwell_s},
            {"alongside_offset_m", &t.alongside_offset_m}, {"tzm_capacity", &t.tzm_capacity},
            {"fuel_truck_capacity_l", &t.fuel_truck_capacity_l}};
        // Optional: the board's warning time (board_reading.hpp).
        const std::pair<const char*, double*> optional[] = {
            {"warning_s", &t.warning_s}, {"zone_edge_m", &t.zone_edge_m}, {"short_halt_in_column_m", &t.short_halt_in_column_m},
            {"halt_interval_max_s", &t.halt_interval_max_s}, {"breakdown_per_100km", &t.breakdown_per_100km}, {"repair_s", &t.repair_s}};
        for (const auto& f : optional) {
            const std::string p = Join("tactics", f.first);
            const json::Value* v = b->Find(f.first);
            if (v == nullptr) continue;
            if (!v->Is(json::Type::Object) || !Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) {
                return message_.empty() ? Fail(p, "expected {value, source, confidence}") : false;
            }
            const json::Value* n = v->Find("value");
            if (n == nullptr || !n->Is(json::Type::Number) || !(n->number > 0.0)) return Fail(Join(p, "value"), "a number > 0");
            *f.second = n->number;
        }
        for (const auto& f : fields) {
            const std::string p = Join("tactics", f.first);
            const json::Value* v = b->Find(f.first);
            if (v == nullptr) return Fail(p, "required");
            if (!v->Is(json::Type::Object) || !Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) {
                return message_.empty() ? Fail(p, "expected {value, source, confidence}") : false;
            }
            const json::Value* n = v->Find("value");
            if (n == nullptr || !n->Is(json::Type::Number) || !(n->number > 0.0)) return Fail(Join(p, "value"), "a number > 0");
            *f.second = n->number;
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadBatteryTacticsFile(const std::string& path, BatteryTactics& out, std::string* error = nullptr) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text.str(), root, &parse_error)) {
        if (error != nullptr) *error = path + ": " + parse_error;
        return false;
    }
    BatteryTactics t;
    detail::BatteryTacticsLoader loader;
    if (!loader.Load(root, t)) {
        if (error != nullptr) *error = path + ": " + loader.Message();
        return false;
    }
    out = t;
    return true;
}

class Battery {
public:
    enum class Mode { Waiting, Forming, Marching, ShortHalt, Formation, Halted, Ambush, RiverApproach, RiverCrossing };
    enum class Service { Reload, Refuel };

    struct Vehicle {
        BatteryVehicleDef def;
        Vec3 position;
        Vec3 goal;
        double heading_deg = 0.0;
        double speed_mps = 0.0;
        bool deployed = false;     // stopped and deployed (short halt or formation slot reached and set up)
        double deploy_done_s = -1.0;
        double stock = 0.0;        // a TZM: missiles aboard; a fuel truck: litres aboard
        // A service run (a TZM or fuel truck going alongside another vehicle): the served vehicle, and where to go back to.
        std::string serving;
        Service service = Service::Reload;
        bool alongside = false;
        bool returning = false;
        Vec3 home;
        // Crossing a river: the stages of this vehicle's own crossing.
        int crossing_stage = 0;  // 0 waiting on the near side, 1 to the water's edge, 2 in the water, 3 to the far slot
        bool afloat = false;
        bool waiting = false;    // bounding or crossing: holding until its pair's turn
        // Orders carried by a means (command/battery_orders.hpp): a vehicle whose commander has not heard the battery's
        // order stays where it is; once he has, it leaves its emplacement and goes to its place, on the march catching
        // the column up along the route.
        bool absent = false;
        // Gone from the battery on a run of its own (the TZM to the technical battery: roadmap 17.4): it takes no part in
        // the battery's moves or services until it is back.
        bool away = false;
        bool dug_in = false;         // still in its emplacement
        double hold_until_s = -1.0;  // leaving its emplacement until then
        bool catching = false;
        bool on_route = false;       // reached the route (it first drives to it across country)
        double catch_arc = 0.0;      // where along the route it has got to
        std::vector<double> catch_mps;  // its own speed on each stretch (empty: a little over the column's)
        // Broken down where it stands (a fault on the march: G3.5): it takes no part in the battery's moves.
        bool broken = false;
        std::string broken_why;
        double odo_m = 0.0;          // driven on the march (a fault's chance per kilometre)
        double repair_at_s = -1.0;   // broken down: repaired then (< 0: waiting for the maintenance vehicle)
        std::string helping;         // the maintenance vehicle: the broken-down vehicle it stays with
        // Track O, O4: a launcher's place in the combat formation as the battery commander set it, and the spot its own
        // commander chose within his leeway of it (has_spot: it drives there in place of its place).
        bool has_slot = false;
        Vec3 slot;
        bool has_spot = false;
        Vec3 spot;
        // Track O, O7: detached from the battery to a place of its own (a single launcher sent to cover an asset, the
        // launchers split singly over villages): it takes no part in the battery's moves and formations, and deploys
        // where it stands once its run on the roads (as `away`, a mover of the session) has brought it there.
        bool detached = false;
    };

    struct Event {
        double t = 0.0;
        std::string who;
        std::string text;
    };

    // A march plan's point as the column passed it: when the head and the tail did (< 0: not yet).
    struct Passing {
        std::string kind;
        std::string name;
        double head_s = -1.0;
        double tail_s = -1.0;
    };

    Battery(BatteryTactics tactics, const std::vector<BatteryVehicleDef>& vehicles) : tactics_(tactics) {
        for (const BatteryVehicleDef& d : vehicles) {
            Vehicle v;
            v.def = d;
            v.position = d.start;
            v.goal = d.start;
            if (d.kind == BatteryVehicleKind::Reload) v.stock = d.stock >= 0.0 ? d.stock : tactics_.tzm_capacity;
            if (d.kind == BatteryVehicleKind::Fuel) v.stock = d.stock >= 0.0 ? d.stock : tactics_.fuel_truck_capacity_l;
            rank_.push_back(vehicles_.size());
            vehicles_.push_back(v);
        }
    }

    const BatteryTactics& Tactics() const { return tactics_; }
    const std::vector<Vehicle>& Vehicles() const { return vehicles_; }
    Mode CurrentMode() const { return mode_; }
    // The battery commander's emission control: the vehicles' radars silent (docs/crew/07 §3). An ambush is silent too
    // until it is sprung; the vehicle commanders decide that by the designations.
    bool Emcon() const { return regime_.kind == EmissionKind::Silent; }
    // The emission regime he has ordered (emission_regime.hpp; the launchers know it only once they hear it on the net),
    // and how many times he has changed it (a new order to say on the net).
    const EmissionRegime& Regime() const { return regime_; }
    int RegimeOrders() const { return regime_orders_; }
    bool AirThreat() const { return air_threat_; }
    double Spacing() const { return air_threat_ ? tactics_.threat_spacing_m : tactics_.march_spacing_m; }
    const Vehicle* Find(const std::string& id) const {
        for (const Vehicle& v : vehicles_)
            if (v.def.id == id) return &v;
        return nullptr;
    }
    const Vehicle* Own() const {
        for (const Vehicle& v : vehicles_)
            if (v.def.own) return &v;
        return nullptr;
    }
    // The march plan's points as the column passed them (a march ordered with points), and where along the route the
    // head and the tail of the column are meant to be.
    const std::vector<Passing>& Passings() const { return passings_; }
    // How many marches with a route the battery has been ordered: a new one starts its passings afresh.
    int Marches() const { return marches_; }
    // A point as the radiotelephonist writes it when the head passes it (the record the march is known by: decision G-6).
    static std::string PassingText(const Passing& ps) {
        return ps.kind == "start"        ? "the head at the start point, " + ps.name
               : ps.kind == "regulation" ? "the head at the regulation point, " + ps.name
               : ps.kind == "halt"       ? "halt, " + ps.name
               : ps.kind == "swim"       ? "swimming " + ps.name
               : ps.kind == "regulator"  ? "the head at " + ps.name
                                         : "arrived: " + ps.name;
    }
    double HeadArc() const { return lead_s_; }
    double TailArc() const { return SlotArc(vehicles_.empty() ? 0 : vehicles_.size() - 1); }
    // A vehicle's place in the column (0 the head).
    std::size_t PlaceOf(std::size_t vehicle) const { return vehicle < rank_.size() ? rank_[vehicle] : vehicle; }

    // The column stops where it stands until it is ordered on (a road found blocked: nav/march.hpp).
    void Block(double t, const std::string& why) {
        mode_ = Mode::Halted;
        halt_until_s_ = 1e18;
        events_.push_back({t, "Battery commander", "the column stops: " + why});
    }

    // The player's vehicle moved (the player drives it): the battery takes its position as it is.
    void SetOwnPosition(const Vec3& p) {
        for (Vehicle& v : vehicles_)
            if (v.def.own) v.position = p;
    }
    // Whether the battery drives the player's vehicle too (a demo, or an NPC driver). Off: it only gives it a goal.
    void SetDriveOwn(bool on) { drive_own_ = on; }
    void SetNight(bool night) { night_ = night; }
    bool Night() const { return night_; }
    // The battery stands in a prepared position: the first move order costs `seconds` to climb out and clear the nets.
    // The battery starts dug in at a prepared position: every vehicle is set up there (deployed) and takes `seconds` to
    // leave its emplacement when it moves.
    void SetInEmplacement(double seconds) {
        leave_s_ = seconds;
        emplacement_s_ = seconds;
        for (Vehicle& v : vehicles_) {
            v.deployed = true;
            v.dug_in = true;
        }
    }

    // Orders carried by a means: until its commander has heard, a vehicle takes no part in the battery's orders and stays
    // where it is.
    void SetAbsent(const std::string& id, bool absent) {
        for (Vehicle& v : vehicles_)
            if (v.def.id == id) v.absent = absent;
    }
    bool Absent(const std::string& id) const {
        const Vehicle* v = Find(id);
        return v != nullptr && v->absent;
    }
    // A vehicle gone on a run of its own (roadmap 17.4), where it is now (its mover's head on the roads), or back at `at`.
    void SetAway(const std::string& id, bool away, const Vec3& at) {
        for (Vehicle& v : vehicles_) {
            if (v.def.id != id) continue;
            v.away = away;
            v.position = at;
            v.goal = at;
            v.speed_mps = 0.0;
        }
    }
    // Track O, O7: a vehicle detached from the battery (`on`), standing at `at` (it deploys there), or back with it. False
    // for no such vehicle.
    bool SetDetached(const std::string& id, bool on, const Vec3& at, double t) {
        for (Vehicle& v : vehicles_) {
            if (v.def.id != id) continue;
            v.detached = on;
            v.away = false;
            v.catching = false;
            v.waiting = false;
            v.position = at;
            v.goal = at;
            v.speed_mps = 0.0;
            v.deployed = false;
            v.deploy_done_s = -1.0;
            v.has_spot = false;
            events_.push_back({t, v.def.callsign, on ? "detached: at its own place, deploying" : "back with the battery"});
            return true;
        }
        return false;
    }
    bool Detached(const std::string& id) const {
        const Vehicle* v = Find(id);
        return v != nullptr && v->detached;
    }
    // Track O, O7: rested and repaired at the rear (a battery relieved by the reserve): every fault mended, the TZM and the
    // fuel truck loaded full again.
    void RestAndRepair(double t) {
        for (Vehicle& v : vehicles_) {
            if (v.broken) {
                v.broken = false;
                v.broken_why.clear();
                v.repair_at_s = -1.0;
            }
            v.helping.clear();
            if (v.def.kind == BatteryVehicleKind::Reload) v.stock = tactics_.tzm_capacity;
            if (v.def.kind == BatteryVehicleKind::Fuel) v.stock = tactics_.fuel_truck_capacity_l;
        }
        events_.push_back({t, "Battery commander", "rested and repaired: the faults mended, the TZM and the fuel truck full"});
    }
    // A vehicle as another model of the same battery has it (track O, O3: a linked Osa's runner follows the post's battery,
    // which marches and deploys it): where it stands, its heading and speed, whether it is set up. False for no such vehicle.
    bool Mirror(const std::string& id, const Vec3& position, double heading_deg, double speed_mps, bool deployed) {
        for (Vehicle& v : vehicles_) {
            if (v.def.id != id) continue;
            v.position = position;
            v.goal = position;
            v.heading_deg = heading_deg;
            v.speed_mps = speed_mps;
            v.deployed = deployed;
            return true;
        }
        return false;
    }
    // What a service vehicle carries now (the TZM back from the technical battery with its load).
    void SetStock(const std::string& id, double stock) {
        for (Vehicle& v : vehicles_)
            if (v.def.id == id) v.stock = std::max(0.0, stock);
    }
    // The vehicle the battery commander rides in: while it is absent (its crew still stowing) the column does not form.
    void SetLeader(const std::string& id) { leader_ = id; }
    // Its commander has heard and acts: the vehicle leaves its emplacement (if it is still dug in) and goes to its place;
    // away from its place in a column already on the march, or in one forming after the battery is out of its
    // emplacements (it heard late), it catches the column up along the route at `catch_mps` (its own speed on each
    // stretch). One in its place that heard a halt late just stops.
    void Join(const std::string& id, double t, const std::vector<double>& catch_mps = {}) {
        for (std::size_t k = 0; k < vehicles_.size(); ++k) {
            Vehicle& v = vehicles_[k];
            if (v.def.id != id || !v.absent) continue;
            v.absent = false;
            const bool away = route_.size() >= 2 && Dist(v.position, PointAt(SlotArc(rank_[k]))) > tactics_.slot_tolerance_m + Spacing();
            const bool late = away && (mode_ == Mode::Marching || mode_ == Mode::Halted ||
                                       (mode_ == Mode::Forming && v.dug_in && t >= hold_until_s_));
            v.hold_until_s = t + (v.dug_in ? emplacement_s_ : 0.0);
            if (v.dug_in) events_.push_back({t, v.def.callsign, "out of the emplacement (" + Fmt(emplacement_s_) + " s)"});
            v.dug_in = false;
            if (route_.size() >= 2 && late) {
                v.deployed = false;
                v.catching = true;
                v.on_route = false;
                v.catch_arc = std::min(NearestArc(v.position), SlotArc(rank_[k]));
                v.catch_mps = catch_mps.size() + 1 == route_.size() ? catch_mps : std::vector<double>();
                events_.push_back({t, v.def.callsign, "after the column, to catch it up"});
            }
        }
    }
    bool Catching(const std::string& id) const {
        const Vehicle* v = Find(id);
        return v != nullptr && v->catching;
    }
    // Track O, O4: a launcher's commander chose his spot (within his leeway of his place in the formation, or not: the order's
    // limits are rules, not walls): in the combat formation the vehicle leaves its place and drives across country there
    // and deploys. False for no such launcher, or none in a formation.
    bool SetSpot(const std::string& id, const Vec3& p, double t) {
        if (mode_ != Mode::Formation) return false;
        for (Vehicle& v : vehicles_) {
            if ((v.def.id != id && v.def.callsign != id) || v.def.kind != BatteryVehicleKind::Launcher || v.broken || v.away) continue;
            v.has_spot = true;
            v.spot = Vec3{p.x, p.y, v.position.z};
            v.goal = v.spot;  // a pair held while another bounds goes there in its turn
            if (Dist(v.position, v.goal) > tactics_.slot_tolerance_m) {
                v.deployed = false;
                v.deploy_done_s = -1.0;
                events_.push_back({t, v.def.callsign, "to its commander's spot, " + Fmt(Dist(v.position, v.goal)) + " m"});
            }
            return true;
        }
        return false;
    }

    // A vehicle breaks down where it stands: it stops, and the column goes on without it.
    void Breakdown(const std::string& id, double t, const std::string& why) {
        for (Vehicle& v : vehicles_) {
            if (v.def.id != id || v.broken) continue;
            v.broken = true;
            v.broken_why = why;
            v.catching = false;
            v.speed_mps = 0.0;
            v.goal = v.position;
            events_.push_back({t, v.def.callsign, "broken down: " + why});
        }
    }
    bool Broken(const std::string& id) const {
        const Vehicle* v = Find(id);
        return v != nullptr && v->broken;
    }
    // Faults on the march (G3.5): each vehicle at the tactics' chance per kilometre driven, the rolls repeatable from the seed.
    // The column goes on without it; the maintenance vehicle at the tail stops when it comes up to it and repairs it in
    // `repair_s` (its crew alone, if there is none or it is busy); then both catch the column up.
    void SetBreakdownSeed(std::uint64_t seed) { breakdown_seed_ = seed; }

    // The march's route, and whether the head of the column has come to its end.
    const std::vector<Vec3>& Route() const { return route_; }
    bool AtRouteEnd() const { return arrived_logged_; }

    void SetAirThreat(bool threat, double t) {
        if (threat == air_threat_) return;
        air_threat_ = threat;
        events_.push_back({t, "Battery commander",
                           threat ? "air attack expected: open the column to " + Fmt(tactics_.threat_spacing_m) + " m"
                                  : "close the column to " + Fmt(tactics_.march_spacing_m) + " m"});
    }

    void Order(const BatteryOrder& o, double t) {
        const bool moves = o.kind == BatteryOrder::Kind::March || o.kind == BatteryOrder::Kind::CombatFormation ||
                           o.kind == BatteryOrder::Kind::Relocate;
        if (moves && leave_s_ > 0.0) {
            hold_until_s_ = t + leave_s_;
            events_.push_back({t, "Battery commander", "out of the position: clear the nets and climb out (" + Fmt(leave_s_) + " s)"});
            leave_s_ = 0.0;
        }
        switch (o.kind) {
            case BatteryOrder::Kind::Emcon:  // silent, or back to the regime before the silence
                if (o.radiate == (regime_.kind != EmissionKind::Silent)) return;
                if (o.radiate) {
                    regime_ = before_silence_;
                } else {
                    before_silence_ = regime_;
                    regime_ = EmissionRegime{};
                    regime_.kind = EmissionKind::Silent;
                    regime_.from = "battery";
                }
                ++regime_orders_;
                events_.push_back({t, "Battery commander", o.radiate ? "radiation on" : "radiation off: the battery's radars silent"});
                return;
            case BatteryOrder::Kind::Emission:
                if (o.regime.Same(regime_)) return;
                if (o.regime.kind == EmissionKind::Silent && regime_.kind != EmissionKind::Silent) before_silence_ = regime_;
                regime_ = o.regime;
                ++regime_orders_;
                events_.push_back({t, "Battery commander", std::string("emission regime: ") + EmissionKindName(regime_.kind) +
                                                               (regime_.from == "regiment" ? " (the regiment's order)" : "")});
                return;
            case BatteryOrder::Kind::March:
                if (o.route.size() < 2) return;
                route_ = o.route;
                route_mps_ = o.route_mps.size() + 1 == o.route.size() ? o.route_mps : std::vector<double>();
                points_ = o.points;
                ++marches_;
                passings_.clear();
                for (const BatteryMarchPoint& p : points_) passings_.push_back({p.kind, p.name, -1.0, -1.0});
                next_point_ = 0;
                SetOrderOfMarch(o.order);
                BuildArcs();
                lead_s_ = Spacing() * static_cast<double>(vehicles_.size() - 1);
                mode_ = Mode::Forming;
                marched_s_ = 0.0;
                for (Vehicle& v : vehicles_) {
                    if (v.detached) continue;  // O7: at its own place
                    v.deployed = false;
                    v.catching = false;
                    if (!v.absent) v.dug_in = false;
                }
                events_.push_back({t, "Battery commander", "march: form the column (" + std::to_string(vehicles_.size()) + " vehicles)"});
                break;
            case BatteryOrder::Kind::ShortHalt: {
                if (route_.size() < 2) return;
                mode_ = Mode::ShortHalt;
                // A target within 20 km: stop in the column; beyond, off the road away from it (voenizdat_osa_1991 p254).
                const bool in_column = o.threat_range_m >= 0.0 && tactics_.short_halt_in_column_m > 0.0 &&
                                       o.threat_range_m < tactics_.short_halt_in_column_m;
                const Vec3 toward{std::sin(DegToRad(o.threat_az_deg)), std::cos(DegToRad(o.threat_az_deg)), 0.0};
                for (std::size_t k = 0; k < vehicles_.size(); ++k) {
                    Vehicle& v = vehicles_[k];
                    if (v.detached) continue;  // O7: at its own place
                    const double s = SlotArc(rank_[k]);
                    const Vec3 dir = DirectionAt(s);
                    const Vec3 left{-dir.y, dir.x, 0.0};
                    double side = rank_[k] % 2 == 0 ? 1.0 : -1.0;
                    if (o.threat_has_azimuth) side = Dot(left, toward) > 0.0 ? -1.0 : 1.0;
                    v.goal = PointAt(s) + left * (in_column ? 0.0 : side * tactics_.short_halt_offset_m);
                    v.deployed = false;
                    v.deploy_done_s = -1.0;
                }
                events_.push_back({t, "Battery commander", in_column ? "short halt: the target within " +
                                                                          Fmt(tactics_.short_halt_in_column_m / 1000.0) +
                                                                          " km, stop in the column, deploy"
                                                     : o.threat_has_azimuth ? "short halt: off the road away from the target, deploy"
                                                                            : "short halt: off the road, deploy"});
                break;
            }
            case BatteryOrder::Kind::CombatFormation:
                mode_ = Mode::Formation;
                for (Vehicle& v : vehicles_) v.catching = false;  // a vehicle still catching up goes straight to its slot
                FormationSlots(o.covered, o.axis_deg);
                bounding_ = o.bounding;
                if (bounding_) HoldPairsAfterFirst();
                events_.push_back({t, "Battery commander", std::string(o.bounding ? "bound forward in pairs" : "combat formation") + ": pairs " +
                                                               Fmt(tactics_.pair_separation_m) + " m apart, " +
                                                               Fmt(tactics_.behind_covered_m) + " m behind the covered unit"});
                break;
            case BatteryOrder::Kind::Relocate: {
                // A change of position (after an ARM attack, on the regiment's order: voenizdat_osa_1991 p238): the
                // whole battery moves across country by the own vehicle's offset to the new position, keeping its order,
                // and deploys there.
                const Vehicle* own = Own();
                if (own == nullptr) return;
                const Vec3 shift{o.covered.x - own->position.x, o.covered.y - own->position.y, 0.0};
                mode_ = Mode::Formation;
                bounding_ = false;
                for (Vehicle& v : vehicles_) {
                    if (v.detached) continue;  // O7: at its own place
                    v.waiting = false;
                    v.goal = v.position + shift;
                    v.slot = v.goal;  // O4: the new place is where it is taken; a chosen spot stays behind
                    v.has_spot = false;
                    v.deployed = false;
                    v.deploy_done_s = -1.0;
                }
                events_.push_back({t, "Battery commander", "change of position: to the reserve position, " + Fmt(shift.HorizontalLength()) +
                                                               " m, in the same order"});
                break;
            }
            case BatteryOrder::Kind::Ambush:
                if (o.route.empty()) return;
                mode_ = Mode::Ambush;
                ambush_points_ = o.route;
                ambush_roving_ = o.roving;
                ambush_shift_ = 0;
                ambush_next_move_s_ = t + tactics_.ambush_dwell_s;
                AmbushSlots();
                events_.push_back({t, "Battery commander", "ambush: pairs to " + std::to_string(o.route.size()) + " prepared points" +
                                                               (o.roving ? ", roving between them" : "")});
                break;
            case BatteryOrder::Kind::RiverCrossing:
                mode_ = Mode::RiverApproach;
                river_point_ = o.covered;
                river_axis_deg_ = o.axis_deg;
                river_width_m_ = o.width_m;
                RiverApproachSlots();
                events_.push_back({t, "Battery commander", "river crossing: positions " + Fmt(tactics_.river_standoff_m) +
                                                               " m from the river; cross in pairs when the far bank is held"});
                break;
            case BatteryOrder::Kind::FarBankHeld:
                if (mode_ != Mode::RiverApproach) return;
                mode_ = Mode::RiverCrossing;
                crossing_pair_ = 0;
                StartCrossingPair(t);
                events_.push_back({t, "Battery commander", "far bank held: cross in pairs"});
                break;
            case BatteryOrder::Kind::Reload:
                RequestService(o.vehicle, Service::Reload, t);
                break;
            case BatteryOrder::Kind::Refuel:
                RequestService(o.vehicle, Service::Refuel, t);
                break;
            case BatteryOrder::Kind::Halt:
                StartHalt(t, night_ ? tactics_.night_halt_s : tactics_.halt_duration_s, "halt ordered");
                break;
            case BatteryOrder::Kind::Resume:
                if (route_.size() >= 2) {
                    mode_ = Mode::Forming;
                    for (Vehicle& v : vehicles_) {
                        if (!v.detached) v.deployed = false;
                    }
                    events_.push_back({t, "Battery commander", "back on the road: form the column"});
                }
                break;
        }
    }

    void Step(double t, double dt) {
        StepRepairs(t);
        if (mode_ == Mode::Halted && t >= halt_until_s_) {
            mode_ = Mode::Forming;
            marched_s_ = 0.0;
            events_.push_back({t, "Battery commander", "halt over: form the column"});
        }
        if (mode_ == Mode::Forming || mode_ == Mode::Marching) {
            bool formed = true;
            for (std::size_t k = 0; k < vehicles_.size(); ++k) {
                Vehicle& v = vehicles_[k];
                if (v.absent && v.def.id == leader_) formed = false;  // the column does not move off without its commander
                if (v.absent || v.away || v.catching || v.broken || v.detached) continue;  // not in the column (yet): the column goes without it
                v.goal = PointAt(SlotArc(rank_[k]));
                if (Dist(v.position, v.goal) > tactics_.slot_tolerance_m + Spacing() * 0.5) formed = false;
            }
            if (mode_ == Mode::Forming && formed && t >= hold_until_s_) {
                mode_ = Mode::Marching;
                events_.push_back({t, "Battery commander", "column formed: march"});
            }
            if (mode_ == Mode::Marching) {
                // The leader moves on as long as nobody behind has fallen too far back.
                bool keeping_up = true;
                for (const Vehicle& v : vehicles_)
                    if (!v.absent && !v.away && !v.catching && !v.broken && !v.detached && Dist(v.position, v.goal) > 4.0 * Spacing() + 50.0) keeping_up = false;
                if (keeping_up && lead_s_ < total_ && t >= hold_until_s_) {
                    double next = Advance(lead_s_, dt);
                    // The head stops at a plan's point until its time, and at a halt.
                    if (next_point_ < points_.size() && (points_[next_point_].not_before_s > t || points_[next_point_].halt_s > 0.0) &&
                        next > points_[next_point_].arc_m) {
                        next = std::max(lead_s_, points_[next_point_].arc_m);
                    }
                    lead_s_ = next;
                    marched_s_ += dt;
                }
                NotePassings(t);
                // At the end of the route once the plan's last point is passed (at its time: an early head waits there).
                if (lead_s_ >= total_ - 1e-6 && next_point_ >= points_.size() && !arrived_logged_) {
                    arrived_logged_ = true;
                    events_.push_back({t, "Battery commander", "the head of the column is at the end of the route"});
                }
                if (points_.empty() && marched_s_ >= tactics_.halt_interval_s) {
                    StartHalt(t, night_ ? tactics_.night_halt_s : tactics_.halt_duration_s, "scheduled halt: checks, fuel, rest");
                }
            }
        }
        if (mode_ == Mode::Ambush && ambush_roving_ && t >= ambush_next_move_s_) {
            ++ambush_shift_;
            ambush_next_move_s_ = t + tactics_.ambush_dwell_s;
            AmbushSlots();
            events_.push_back({t, "Battery commander", "ambush: pairs move on to the next point"});
        }
        if (mode_ == Mode::RiverCrossing) StepCrossing(t);
        if (mode_ == Mode::Formation && bounding_) StepBounding(t);
        const bool on_road = mode_ == Mode::Marching || mode_ == Mode::Forming;
        const double speed = on_road && mode_ == Mode::Marching ? tactics_.march_speed_mps * 1.3  // a little faster to keep up
                                                                : (on_road ? tactics_.march_speed_mps : tactics_.cross_country_speed_mps);
        for (Vehicle& v : vehicles_) {
            if (v.def.own && !drive_own_) continue;
            if (v.absent || v.away || v.broken || v.detached || t < v.hold_until_s) {
                v.speed_mps = 0.0;
                continue;
            }
            if (!v.serving.empty()) {  // a service run goes on whatever the battery does
                StepService(v, t, dt);
                continue;
            }
            if (v.catching) {  // through the column's halts too
                StepCatchUp(v, t, dt);
                continue;
            }
            if (mode_ == Mode::Halted || mode_ == Mode::Waiting || t < hold_until_s_ || v.waiting) {
                v.speed_mps = 0.0;
                continue;
            }
            const Vec3 was = v.position;
            Move(v, v.afloat ? tactics_.water_speed_mps : speed, dt, mode_ == Mode::Marching);
            if (mode_ == Mode::Marching) MarchWear(v, t, Dist(was, v.position));
        }
        // O7: a detached vehicle at its own place deploys there, whatever the battery does.
        for (Vehicle& v : vehicles_) {
            if (!v.detached || v.deployed || v.broken || v.away) continue;
            if (v.deploy_done_s < 0.0) v.deploy_done_s = t + tactics_.deploy_s;
            if (t >= v.deploy_done_s) {
                v.deployed = true;
                events_.push_back({t, v.def.callsign, "detached, in position, deployed"});
            }
        }
        // Stopped at a short-halt point, a formation slot, an ambush point or a river position: deploy.
        if (mode_ == Mode::ShortHalt || mode_ == Mode::Formation || mode_ == Mode::Ambush || mode_ == Mode::RiverApproach ||
            mode_ == Mode::RiverCrossing) {
            for (Vehicle& v : vehicles_) {
                if (v.deployed || !v.serving.empty() || v.waiting || v.absent || v.away || v.broken || v.detached) continue;
                if (mode_ == Mode::RiverCrossing && v.crossing_stage != 0 && v.crossing_stage != 4) continue;
                if (Dist(v.position, v.goal) > tactics_.slot_tolerance_m) continue;
                if (v.deploy_done_s < 0.0) v.deploy_done_s = t + tactics_.deploy_s;
                if (t >= v.deploy_done_s) {
                    v.deployed = true;
                    events_.push_back({t, v.def.callsign, "in position, deployed"});
                }
            }
        }
    }

    // ---- Service runs: a TZM (reload; it carries fuel too) or a fuel truck goes alongside a vehicle ("" : the player's).
    // Returns false if no free vehicle of that kind has anything left.
    bool RequestService(const std::string& vehicle, Service kind, double t) {
        const Vehicle* target = vehicle.empty() ? Own() : Find(vehicle);
        if (target == nullptr) return false;
        for (const Vehicle& v : vehicles_)
            if (v.serving == target->def.id && v.service == kind && !v.returning) return true;  // already on its way
        Vehicle* best = nullptr;
        double best_d = 1e18;
        for (Vehicle& v : vehicles_) {
            const bool fits = kind == Service::Reload ? v.def.kind == BatteryVehicleKind::Reload
                                                      : (v.def.kind == BatteryVehicleKind::Fuel || v.def.kind == BatteryVehicleKind::Reload);
            if (!fits || !v.serving.empty() || v.away || v.stock <= 0.0) continue;
            // a fuel truck is preferred for fuel; a TZM only if no fuel truck can come
            const double d = Dist(v.position, target->position) + (kind == Service::Refuel && v.def.kind == BatteryVehicleKind::Reload ? 1e7 : 0.0);
            if (d < best_d) {
                best_d = d;
                best = &v;
            }
        }
        const std::string told = target->def.id + (kind == Service::Reload ? "/reload" : "/refuel");
        if (best == nullptr) {
            if (std::find(told_nothing_.begin(), told_nothing_.end(), told) == told_nothing_.end()) {
                told_nothing_.push_back(told);
                events_.push_back({t, "Battery commander", std::string(kind == Service::Reload ? "reload" : "refuel") + " for " +
                                                               target->def.callsign + ": nothing left to send"});
            }
            return false;
        }
        told_nothing_.erase(std::remove(told_nothing_.begin(), told_nothing_.end(), told), told_nothing_.end());
        best->serving = target->def.id;
        best->service = kind;
        best->alongside = false;
        best->returning = false;
        best->home = best->position;
        events_.push_back({t, best->def.callsign, std::string(kind == Service::Reload ? "to reload " : "to refuel ") + target->def.callsign});
        return true;
    }
    // Is a vehicle of this service alongside `vehicle` now?
    bool Alongside(const std::string& vehicle, Service kind) const {
        const Vehicle* target = vehicle.empty() ? Own() : Find(vehicle);
        if (target == nullptr) return false;
        for (const Vehicle& v : vehicles_)
            if (v.serving == target->def.id && v.service == kind && v.alongside) return true;
        return false;
    }
    // What the server handed over (missiles, litres); the service vehicle goes back when it is empty.
    void Consume(const std::string& vehicle, Service kind, double amount, double t) {
        const Vehicle* target = vehicle.empty() ? Own() : Find(vehicle);
        if (target == nullptr) return;
        for (Vehicle& v : vehicles_) {
            if (v.serving != target->def.id || v.service != kind || !v.alongside) continue;
            v.stock = std::max(0.0, v.stock - amount);
            if (v.stock <= 0.0) Release(v, t, "empty");
            return;
        }
    }
    // What the service vehicle alongside the served one still carries (missiles, litres); < 0: none alongside.
    double StockAlongside(const std::string& vehicle, Service kind) const {
        const Vehicle* target = vehicle.empty() ? Own() : Find(vehicle);
        if (target == nullptr) return -1.0;
        for (const Vehicle& v : vehicles_) {
            if (v.serving == target->def.id && v.service == kind && v.alongside) return v.stock;
        }
        return -1.0;
    }
    // The served vehicle is full: the service vehicle goes back.
    void Done(const std::string& vehicle, Service kind, double t) {
        const Vehicle* target = vehicle.empty() ? Own() : Find(vehicle);
        if (target == nullptr) return;
        for (Vehicle& v : vehicles_)
            if (v.serving == target->def.id && v.service == kind && !v.returning) Release(v, t, kind == Service::Reload ? "reload done" : "refuelled");
    }

    std::vector<Event> TakeEvents() {
        std::vector<Event> out;
        out.swap(events_);
        return out;
    }

private:
    static double Dist(const Vec3& a, const Vec3& b) { return std::hypot(a.x - b.x, a.y - b.y); }
    static std::string Fmt(double v) { return std::to_string(static_cast<long long>(std::llround(v))); }

    void BuildArcs() {
        arcs_.assign(1, 0.0);
        for (std::size_t i = 1; i < route_.size(); ++i) arcs_.push_back(arcs_.back() + Dist(route_[i - 1], route_[i]));
        total_ = arcs_.back();
        arrived_logged_ = false;
    }
    Vec3 PointAt(double s) const {
        if (route_.empty()) return Vec3{};
        if (s <= 0.0) return route_.front();
        for (std::size_t i = 1; i < route_.size(); ++i) {
            if (s <= arcs_[i]) {
                const double seg = arcs_[i] - arcs_[i - 1];
                const double u = seg > 0.0 ? (s - arcs_[i - 1]) / seg : 0.0;
                return route_[i - 1] + (route_[i] - route_[i - 1]) * u;
            }
        }
        return route_.back();
    }
    Vec3 DirectionAt(double s) const {
        for (std::size_t i = 1; i < route_.size(); ++i) {
            if (s <= arcs_[i] || i + 1 == route_.size()) {
                Vec3 d = route_[i] - route_[i - 1];
                d.z = 0.0;
                const double l = d.Length();
                return l > 0.0 ? d / l : Vec3{0.0, 1.0, 0.0};
            }
        }
        return Vec3{0.0, 1.0, 0.0};
    }
    // The k-th place in the column: behind the leader by k spacings (never before the start of the route).
    double SlotArc(std::size_t k) const { return std::max(0.0, lead_s_ - Spacing() * static_cast<double>(k)); }

    // Where the head is `dt` on from `s`: at the tactics' march speed, or stretch by stretch at the march plan's speeds.
    double Advance(double s, double dt) const { return AdvanceBy(s, dt, route_mps_, 1.0); }
    // Along the route from `s` for `dt` at `mps` on each stretch (none: the tactics' march speed), times `factor`.
    double AdvanceBy(double s, double dt, const std::vector<double>& mps, double factor) const {
        if (mps.empty()) return std::min(total_, s + tactics_.march_speed_mps * factor * dt);
        double left = dt;
        for (std::size_t i = 1; i < arcs_.size() && left > 0.0; ++i) {
            if (s >= arcs_[i]) continue;
            const double v = mps[i - 1] * factor;
            const double reach_s = (arcs_[i] - s) / v;
            if (reach_s >= left) return s + v * left;
            s = arcs_[i];
            left -= reach_s;
        }
        return std::min(total_, s);
    }
    // The place along the route nearest a point.
    double NearestArc(const Vec3& p) const {
        double best = 0.0, best_d = 1e300;
        for (std::size_t i = 1; i < route_.size(); ++i) {
            const Vec3 a = route_[i - 1];
            const double dx = route_[i].x - a.x, dy = route_[i].y - a.y;
            const double len2 = dx * dx + dy * dy;
            const double u = len2 > 0.0 ? std::max(0.0, std::min(1.0, ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2)) : 0.0;
            const double e = std::hypot(p.x - (a.x + dx * u), p.y - (a.y + dy * u));
            if (e < best_d - 1e-9) {
                best_d = e;
                best = arcs_[i - 1] + (arcs_[i] - arcs_[i - 1]) * u;
            }
        }
        return best;
    }
    // A vehicle catching the column up: across country to the route, then along it at its own speed (or a little over
    // the column's) to its place in the column, where it falls in.
    void StepCatchUp(Vehicle& v, double t, double dt) {
        const std::size_t k = static_cast<std::size_t>(&v - vehicles_.data());
        const double slot = SlotArc(rank_[k]);
        if (!v.on_route) {
            v.goal = PointAt(v.catch_arc);
            Move(v, tactics_.cross_country_speed_mps, dt, false);
            if (Dist(v.position, v.goal) <= 1.0) v.on_route = true;
            return;
        }
        const double from = v.catch_arc;
        const double ahead = v.catch_mps.empty() ? AdvanceBy(from, dt, route_mps_, 1.3) : AdvanceBy(from, dt, v.catch_mps, 1.0);
        v.catch_arc = std::min(slot, ahead);
        v.goal = PointAt(v.catch_arc);
        Move(v, (v.catch_arc - from) / dt + 0.5, dt, false);
        if (v.catch_arc >= slot - 1e-6 && Dist(v.position, v.goal) <= tactics_.slot_tolerance_m) {
            v.catching = false;
            events_.push_back({t, v.def.callsign, "in its place in the column"});
        }
    }

    // The order of march by vehicle id: each named vehicle in its place, the others after them as listed.
    void SetOrderOfMarch(const std::vector<std::string>& order) {
        std::size_t place = 0;
        std::vector<bool> placed(vehicles_.size(), false);
        for (const std::string& id : order) {
            for (std::size_t k = 0; k < vehicles_.size(); ++k) {
                if (placed[k] || vehicles_[k].def.id != id) continue;
                rank_[k] = place++;
                placed[k] = true;
            }
        }
        for (std::size_t k = 0; k < vehicles_.size(); ++k) {
            if (!placed[k]) rank_[k] = place++;
        }
    }

    // The head reaching the plan's next point (after its time), and the tail passing the points the head has passed.
    void NotePassings(double t) {
        while (next_point_ < points_.size()) {
            const BatteryMarchPoint& p = points_[next_point_];
            if (lead_s_ < p.arc_m - 1e-6 || p.not_before_s > t) break;
            passings_[next_point_].head_s = t;
            ++next_point_;
            if (p.halt_s > 0.0) {
                StartHalt(t, p.halt_s, (p.kind == "swim" ? "swimming: " : "halt: ") + p.name);
                break;
            }
        }
        const double tail = TailArc();
        for (std::size_t i = 0; i < next_point_; ++i) {
            if (passings_[i].tail_s < 0.0 && (tail >= points_[i].arc_m - 1e-6 || lead_s_ >= total_ - 1e-6)) passings_[i].tail_s = t;
        }
    }

    void FormationSlots(const Vec3& covered, double axis_deg) {
        const double a = DegToRad(axis_deg);
        const Vec3 fwd{std::sin(a), std::cos(a), 0.0};
        const Vec3 right{std::cos(a), -std::sin(a), 0.0};
        std::vector<std::size_t> launchers, reloads, others;
        for (std::size_t k = 0; k < vehicles_.size(); ++k) {
            if (vehicles_[k].detached) continue;  // O7: at its own place
            vehicles_[k].waiting = false;
            switch (vehicles_[k].def.kind) {
                case BatteryVehicleKind::Launcher: launchers.push_back(k); break;
                case BatteryVehicleKind::Reload: reloads.push_back(k); break;
                default: others.push_back(k); break;
            }
        }
        const std::size_t pairs = (launchers.size() + 1) / 2;
        auto pair_centre = [&](std::size_t p) {
            const double lateral = (static_cast<double>(p) - 0.5 * static_cast<double>(pairs - 1)) * tactics_.pair_separation_m;
            return covered + right * lateral - fwd * tactics_.behind_covered_m;
        };
        for (std::size_t j = 0; j < launchers.size(); ++j) {
            const double side = j % 2 == 0 ? -0.5 : 0.5;
            Vehicle& v = vehicles_[launchers[j]];
            v.goal = pair_centre(j / 2) + right * (side * tactics_.pair_internal_m);
            v.slot = v.goal;
            v.has_slot = true;
            v.has_spot = false;
            v.deployed = false;
            v.deploy_done_s = -1.0;
        }
        for (std::size_t j = 0; j < reloads.size(); ++j) {
            Vehicle& v = vehicles_[reloads[j]];
            v.goal = pair_centre(pairs > 0 ? j % pairs : 0) - fwd * tactics_.reload_behind_m;
            v.deployed = false;
            v.deploy_done_s = -1.0;
        }
        for (std::size_t j = 0; j < others.size(); ++j) {
            Vehicle& v = vehicles_[others[j]];
            v.goal = covered - fwd * (tactics_.behind_covered_m + tactics_.command_post_behind_m + 300.0 * static_cast<double>(j));
            v.deployed = false;
            v.deploy_done_s = -1.0;
        }
    }

    std::vector<std::size_t> Launchers() const {
        std::vector<std::size_t> out;
        for (std::size_t k = 0; k < vehicles_.size(); ++k)
            if (vehicles_[k].def.kind == BatteryVehicleKind::Launcher && !vehicles_[k].detached) out.push_back(k);
        return out;
    }

    // Bounding: only the first pair moves now; the others wait deployed where they are.
    void HoldPairsAfterFirst() {
        const std::vector<std::size_t> l = Launchers();
        for (std::size_t j = 0; j < l.size(); ++j) vehicles_[l[j]].waiting = j >= 2;
        bound_pair_ = 0;
    }
    void StepBounding(double t) {
        const std::vector<std::size_t> l = Launchers();
        const std::size_t pairs = (l.size() + 1) / 2;
        // the pair that is moving: once both of its vehicles are deployed, the next pair goes
        bool pair_done = true;
        for (std::size_t j = 2 * bound_pair_; j < std::min(l.size(), 2 * bound_pair_ + 2); ++j) pair_done = pair_done && vehicles_[l[j]].deployed;
        if (!pair_done || bound_pair_ + 1 >= pairs) return;
        ++bound_pair_;
        for (std::size_t j = 2 * bound_pair_; j < std::min(l.size(), 2 * bound_pair_ + 2); ++j) vehicles_[l[j]].waiting = false;
        events_.push_back({t, "Battery commander", "pair " + std::to_string(bound_pair_) + " in position: pair " +
                                                       std::to_string(bound_pair_ + 1) + " bounds forward"});
    }

    // Ambush: pair p at point (p + shift) mod n, the two vehicles 200 m apart; the others behind the first point.
    void AmbushSlots() {
        const std::vector<std::size_t> l = Launchers();
        const std::size_t n = ambush_points_.size();
        for (std::size_t j = 0; j < l.size(); ++j) {
            Vehicle& v = vehicles_[l[j]];
            const Vec3& p = ambush_points_[(j / 2 + ambush_shift_) % n];
            v.goal = p + Vec3{(j % 2 == 0 ? -0.5 : 0.5) * tactics_.pair_internal_m, 0.0, 0.0};
            v.deployed = false;
            v.deploy_done_s = -1.0;
        }
        std::size_t k = 0;
        for (Vehicle& v : vehicles_) {
            if (v.def.kind == BatteryVehicleKind::Launcher || !v.serving.empty()) continue;
            v.goal = ambush_points_.front() + Vec3{0.0, -tactics_.reload_behind_m - 300.0 * static_cast<double>(k++), 0.0};
            v.deployed = false;
            v.deploy_done_s = -1.0;
        }
    }

    // River crossing: the battery takes a formation river_standoff_m short of the crossing, facing across.
    void RiverApproachSlots() {
        const double a = DegToRad(river_axis_deg_);
        const Vec3 across{std::sin(a), std::cos(a), 0.0};
        FormationSlots(river_point_ - across * (tactics_.river_standoff_m - tactics_.behind_covered_m), river_axis_deg_);
        for (Vehicle& v : vehicles_) {
            v.crossing_stage = 0;
            v.afloat = false;
            v.waiting = false;
        }
    }
    void StartCrossingPair(double t) {
        const std::vector<std::size_t> l = Launchers();
        const double a = DegToRad(river_axis_deg_);
        const Vec3 across{std::sin(a), std::cos(a), 0.0};
        const Vec3 right{std::cos(a), -std::sin(a), 0.0};
        std::vector<std::size_t> group;
        if (2 * crossing_pair_ < l.size()) {
            for (std::size_t j = 2 * crossing_pair_; j < std::min(l.size(), 2 * crossing_pair_ + 2); ++j) group.push_back(l[j]);
        } else {
            for (std::size_t k = 0; k < vehicles_.size(); ++k)  // the rest of the battery crosses last
                if (vehicles_[k].def.kind != BatteryVehicleKind::Launcher && vehicles_[k].serving.empty()) group.push_back(k);
        }
        for (std::size_t g = 0; g < group.size(); ++g) {
            Vehicle& v = vehicles_[group[g]];
            v.crossing_stage = 1;
            v.deployed = false;
            v.deploy_done_s = -1.0;
            v.goal = river_point_ + right * ((static_cast<double>(g) - 0.5) * 30.0);  // the water's edge
        }
        if (!group.empty()) {
            events_.push_back({t, vehicles_[group.front()].def.callsign, group.size() > 1 && vehicles_[group.front()].def.kind == BatteryVehicleKind::Launcher
                                                                            ? "pair crossing"
                                                                            : "the rest of the battery crossing"});
        }
        (void)across;
    }
    void StepCrossing(double t) {
        const double a = DegToRad(river_axis_deg_);
        const Vec3 across{std::sin(a), std::cos(a), 0.0};
        bool group_moving = false;
        for (Vehicle& v : vehicles_) {
            if (v.crossing_stage == 1 && Dist(v.position, v.goal) <= tactics_.slot_tolerance_m) {
                v.crossing_stage = 2;
                v.afloat = true;
                v.goal = v.position + across * river_width_m_;
                events_.push_back({t, v.def.callsign, "in the water"});
            } else if (v.crossing_stage == 2 && Dist(v.position, v.goal) <= 1.0) {
                v.crossing_stage = 3;
                v.afloat = false;
                v.goal = v.position + across * tactics_.behind_covered_m;  // a position beyond the far bank
                events_.push_back({t, v.def.callsign, "on the far bank"});
            } else if (v.crossing_stage == 3 && Dist(v.position, v.goal) <= tactics_.slot_tolerance_m) {
                v.crossing_stage = 4;
            }
            if (v.crossing_stage == 1 || v.crossing_stage == 2) group_moving = true;
        }
        if (group_moving) return;
        // the group in the water is across: the next one goes
        const std::vector<std::size_t> l = Launchers();
        const std::size_t groups = (l.size() + 1) / 2 + 1;
        bool any_waiting = false;
        for (const Vehicle& v : vehicles_) any_waiting = any_waiting || (v.crossing_stage == 0 && v.serving.empty());
        if (any_waiting && crossing_pair_ + 1 < groups) {
            ++crossing_pair_;
            StartCrossingPair(t);
        } else if (!any_waiting && !crossing_done_logged_) {
            crossing_done_logged_ = true;
            events_.push_back({t, "Battery commander", "the battery is across"});
        }
    }

    void Release(Vehicle& v, double t, const std::string& why) {
        v.returning = true;
        v.alongside = false;
        events_.push_back({t, v.def.callsign, why + ": going back"});
    }
    void StepService(Vehicle& v, double t, double dt) {
        const Vehicle* target = Find(v.serving);
        if (target == nullptr) {
            v.serving.clear();
            return;
        }
        if (v.returning) {
            v.goal = v.home;
            Move(v, tactics_.cross_country_speed_mps, dt, false);
            if (Dist(v.position, v.home) <= 1.0) {
                v.serving.clear();
                v.returning = false;
                events_.push_back({t, v.def.callsign, "back in place"});
            }
            return;
        }
        // alongside: to the target's right, offset_m away, level with it
        const double hd = DegToRad(target->heading_deg);
        const Vec3 right{std::cos(hd), -std::sin(hd), 0.0};
        v.goal = target->position + right * tactics_.alongside_offset_m;
        if (Dist(v.position, v.goal) <= 2.0 && target->speed_mps <= 0.1) {
            if (!v.alongside) events_.push_back({t, v.def.callsign, std::string("alongside ") + target->def.callsign});
            v.alongside = true;
            v.speed_mps = 0.0;
            return;
        }
        v.alongside = false;
        Move(v, tactics_.cross_country_speed_mps, dt, false);
    }

    void StartHalt(double t, double duration, const std::string& why) {
        mode_ = Mode::Halted;
        halt_until_s_ = t + duration;
        events_.push_back({t, "Battery commander", why + " (" + Fmt(duration / 60.0) + " min)"});
    }

    // Toward the goal at `speed`; on the road a vehicle doesn't back up to open a gap: it waits.
    void Move(Vehicle& v, double speed, double dt, bool on_road) {
        Vec3 d = v.goal - v.position;
        d.z = 0.0;
        const double dist = d.Length();
        if (dist < 0.5) {
            v.speed_mps = 0.0;
            return;
        }
        const Vec3 dir = d / dist;
        if (on_road) {
            const double hd = DegToRad(v.heading_deg);
            const Vec3 facing{std::sin(hd), std::cos(hd), 0.0};
            if (v.speed_mps > 0.0 && Dot(facing, dir) < 0.0 && dist < 2.0 * Spacing()) {
                v.speed_mps = 0.0;
                return;
            }
        }
        const double step = std::min(dist, speed * dt);
        v.position = v.position + dir * step;
        v.speed_mps = step / (dt > 0.0 ? dt : 1.0);
        double hdg = RadToDeg(std::atan2(dir.x, dir.y));
        if (hdg < 0.0) hdg += 360.0;
        v.heading_deg = hdg;
    }

    BatteryTactics tactics_;
    std::vector<Vehicle> vehicles_;
    Mode mode_ = Mode::Waiting;
    bool air_threat_ = false;
    EmissionRegime regime_;
    EmissionRegime before_silence_;
    int regime_orders_ = 0;
    bool night_ = false;
    bool drive_own_ = false;
    std::string leader_;
    std::uint64_t breakdown_seed_ = 1;

    // A repeatable number in [0, 1) for vehicle k's kilometre n (SplitMix64's finaliser).
    double BreakdownRoll(std::size_t k, long long n) const {
        std::uint64_t z = breakdown_seed_ * 0x9E3779B97F4A7C15ull + static_cast<std::uint64_t>(k) * 0xBF58476D1CE4E5B9ull +
                          static_cast<std::uint64_t>(n) * 0x94D049BB133111EBull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        return static_cast<double>(z >> 11) * (1.0 / 9007199254740992.0);
    }
    void MarchWear(Vehicle& v, double t, double moved_m) {
        if (tactics_.breakdown_per_100km <= 0.0 || moved_m <= 0.0) return;
        const long long before = static_cast<long long>(std::floor(v.odo_m / 1000.0));
        v.odo_m += moved_m;
        const long long after = static_cast<long long>(std::floor(v.odo_m / 1000.0));
        const std::size_t k = static_cast<std::size_t>(&v - vehicles_.data());
        for (long long n = before + 1; n <= after && !v.broken; ++n) {
            if (BreakdownRoll(k, n) >= tactics_.breakdown_per_100km / 100.0) continue;
            Breakdown(v.def.id, t, "a fault on the march");
            Vehicle* fixer = nullptr;
            for (Vehicle& m : vehicles_) {
                // The maintenance vehicle behind it (at the tail, it comes up to it), free.
                if (m.def.kind == BatteryVehicleKind::Maintenance && &m != &v && !m.absent && !m.broken && m.helping.empty() &&
                    NearestArc(m.position) <= NearestArc(v.position) + Spacing()) {
                    fixer = &m;
                }
            }
            if (fixer != nullptr) {
                fixer->helping = v.def.id;
                events_.push_back({t, fixer->def.callsign, "to " + v.def.callsign + ", to repair it"});
            } else {
                v.repair_at_s = t + tactics_.repair_s;
            }
        }
    }
    void StepRepairs(double t) {
        for (Vehicle& m : vehicles_) {
            if (m.helping.empty() || m.broken) continue;
            Vehicle* v = nullptr;
            for (Vehicle& c : vehicles_) {
                if (c.def.id == m.helping) v = &c;
            }
            if (v == nullptr || !v->broken) {
                m.helping.clear();
                continue;
            }
            if (Dist(m.position, v->position) <= Spacing() + tactics_.slot_tolerance_m) {  // come up to it: stops and repairs
                m.broken = true;
                m.broken_why = "with " + v->def.callsign + ", repairing it";
                m.speed_mps = 0.0;
                m.goal = m.position;
                m.catching = false;
                v->repair_at_s = m.repair_at_s = t + tactics_.repair_s;
                events_.push_back({t, m.def.callsign, "stops with " + v->def.callsign + " and repairs it"});
            }
        }
        for (std::size_t k = 0; k < vehicles_.size(); ++k) {
            Vehicle& v = vehicles_[k];
            if (!v.broken || v.repair_at_s < 0.0 || t < v.repair_at_s) continue;
            v.broken = false;
            v.broken_why.clear();
            v.repair_at_s = -1.0;
            v.helping.clear();
            if (route_.size() >= 2 && (mode_ == Mode::Marching || mode_ == Mode::Halted || mode_ == Mode::Forming)) {
                v.deployed = false;
                v.catching = true;
                v.on_route = false;
                v.catch_arc = std::min(NearestArc(v.position), SlotArc(rank_[k]));
                v.catch_mps.clear();
                events_.push_back({t, v.def.callsign, "repaired: after the column, to catch it up"});
            } else {
                events_.push_back({t, v.def.callsign, "repaired"});
            }
        }
    }
    std::vector<Vec3> route_;
    std::vector<double> route_mps_;
    double emplacement_s_ = 0.0;  // to leave the emplacement (SetInEmplacement), for a vehicle that leaves late
    std::vector<BatteryMarchPoint> points_;
    std::vector<Passing> passings_;
    std::size_t next_point_ = 0;
    std::vector<std::size_t> rank_;  // each vehicle's place in the column
    std::vector<double> arcs_;
    double total_ = 0.0;
    double lead_s_ = 0.0;
    double marched_s_ = 0.0;
    double halt_until_s_ = 0.0;
    bool arrived_logged_ = false;
    int marches_ = 0;
    double leave_s_ = 0.0;
    bool bounding_ = false;
    std::size_t bound_pair_ = 0;
    std::vector<Vec3> ambush_points_;
    bool ambush_roving_ = false;
    std::size_t ambush_shift_ = 0;
    double ambush_next_move_s_ = 0.0;
    Vec3 river_point_;
    double river_axis_deg_ = 0.0;
    double river_width_m_ = 100.0;
    std::size_t crossing_pair_ = 0;
    bool crossing_done_logged_ = false;
    std::vector<std::string> told_nothing_;
    double hold_until_s_ = -1.0;
    std::vector<Event> events_;
};

}  // namespace simcore
