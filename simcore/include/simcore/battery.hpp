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
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
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
};

struct BatteryOrder {
    enum class Kind { March, ShortHalt, CombatFormation, Halt, Resume, Ambush, RiverCrossing, FarBankHeld, Reload, Refuel, Emcon };
    Kind kind = Kind::March;
    double at_s = 0.0;
    std::vector<Vec3> route;  // March: the road, first point to last. Ambush: the prepared points
    Vec3 covered;             // CombatFormation: the covered unit's position. RiverCrossing: the crossing point (near bank)
    double axis_deg = 0.0;    // direction of advance, or across the river (true, clockwise from north)
    bool bounding = false;    // CombatFormation: the pairs move one at a time, the others staying deployed
    bool roving = false;      // Ambush: the pairs move on to the next point every ambush_dwell_s
    double width_m = 100.0;   // RiverCrossing: the river's width
    std::string vehicle;      // Reload, Refuel: the vehicle served ("" : the player's)
    bool radiate = false;     // Emcon: the radars may radiate again (false: radiation off, the battery fights silent)
    // ShortHalt: the target the halt is for, as the battery commander knows it: its range (< 0: not known) and, if
    // known, its true azimuth from the battery.
    double threat_range_m = -1.0;
    bool threat_has_azimuth = false;
    double threat_az_deg = 0.0;
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
    }
    return "?";
}

inline bool ParseBatteryOrder(const std::string& s, BatteryOrder::Kind& out) {
    for (BatteryOrder::Kind k : {BatteryOrder::Kind::March, BatteryOrder::Kind::ShortHalt, BatteryOrder::Kind::CombatFormation,
                                 BatteryOrder::Kind::Halt, BatteryOrder::Kind::Resume, BatteryOrder::Kind::Ambush,
                                 BatteryOrder::Kind::RiverCrossing, BatteryOrder::Kind::FarBankHeld, BatteryOrder::Kind::Reload,
                                 BatteryOrder::Kind::Refuel, BatteryOrder::Kind::Emcon}) {
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
            {"warning_s", &t.warning_s}, {"zone_edge_m", &t.zone_edge_m}, {"short_halt_in_column_m", &t.short_halt_in_column_m}};
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
    };

    struct Event {
        double t = 0.0;
        std::string who;
        std::string text;
    };

    Battery(BatteryTactics tactics, const std::vector<BatteryVehicleDef>& vehicles) : tactics_(tactics) {
        for (const BatteryVehicleDef& d : vehicles) {
            Vehicle v;
            v.def = d;
            v.position = d.start;
            v.goal = d.start;
            if (d.kind == BatteryVehicleKind::Reload) v.stock = tactics_.tzm_capacity;
            if (d.kind == BatteryVehicleKind::Fuel) v.stock = tactics_.fuel_truck_capacity_l;
            vehicles_.push_back(v);
        }
    }

    const BatteryTactics& Tactics() const { return tactics_; }
    const std::vector<Vehicle>& Vehicles() const { return vehicles_; }
    Mode CurrentMode() const { return mode_; }
    // The battery commander's emission control: the vehicles' radars silent (docs/crew/07 §3). An ambush is silent too
    // until it is sprung; the vehicle commanders decide that by the designations.
    bool Emcon() const { return emcon_; }
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

    // The player's vehicle moved (the player drives it): the battery takes its position as it is.
    void SetOwnPosition(const Vec3& p) {
        for (Vehicle& v : vehicles_)
            if (v.def.own) v.position = p;
    }
    // Whether the battery drives the player's vehicle too (a demo, or an NPC driver). Off: it only gives it a goal.
    void SetDriveOwn(bool on) { drive_own_ = on; }
    void SetNight(bool night) { night_ = night; }
    // The battery stands in a prepared position: the first move order costs `seconds` to climb out and clear the nets.
    // The battery starts dug in at a prepared position: every vehicle is set up there (deployed) and takes `seconds` to
    // leave its emplacement when it moves.
    void SetInEmplacement(double seconds) {
        leave_s_ = seconds;
        for (Vehicle& v : vehicles_) v.deployed = true;
    }

    void SetAirThreat(bool threat, double t) {
        if (threat == air_threat_) return;
        air_threat_ = threat;
        events_.push_back({t, "Battery commander",
                           threat ? "air attack expected: open the column to " + Fmt(tactics_.threat_spacing_m) + " m"
                                  : "close the column to " + Fmt(tactics_.march_spacing_m) + " m"});
    }

    void Order(const BatteryOrder& o, double t) {
        const bool moves = o.kind == BatteryOrder::Kind::March || o.kind == BatteryOrder::Kind::CombatFormation;
        if (moves && leave_s_ > 0.0) {
            hold_until_s_ = t + leave_s_;
            events_.push_back({t, "Battery commander", "out of the position: clear the nets and climb out (" + Fmt(leave_s_) + " s)"});
            leave_s_ = 0.0;
        }
        switch (o.kind) {
            case BatteryOrder::Kind::Emcon:
                emcon_ = !o.radiate;
                events_.push_back({t, "Battery commander", emcon_ ? "radiation off: the battery's radars silent" : "radiation on"});
                return;
            case BatteryOrder::Kind::March:
                if (o.route.size() < 2) return;
                route_ = o.route;
                BuildArcs();
                lead_s_ = Spacing() * static_cast<double>(vehicles_.size() - 1);
                mode_ = Mode::Forming;
                marched_s_ = 0.0;
                for (Vehicle& v : vehicles_) v.deployed = false;
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
                    const double s = SlotArc(k);
                    const Vec3 dir = DirectionAt(s);
                    const Vec3 left{-dir.y, dir.x, 0.0};
                    double side = k % 2 == 0 ? 1.0 : -1.0;
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
                FormationSlots(o.covered, o.axis_deg);
                bounding_ = o.bounding;
                if (bounding_) HoldPairsAfterFirst();
                events_.push_back({t, "Battery commander", std::string(o.bounding ? "bound forward in pairs" : "combat formation") + ": pairs " +
                                                               Fmt(tactics_.pair_separation_m) + " m apart, " +
                                                               Fmt(tactics_.behind_covered_m) + " m behind the covered unit"});
                break;
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
                    for (Vehicle& v : vehicles_) v.deployed = false;
                    events_.push_back({t, "Battery commander", "back on the road: form the column"});
                }
                break;
        }
    }

    void Step(double t, double dt) {
        if (mode_ == Mode::Halted && t >= halt_until_s_) {
            mode_ = Mode::Forming;
            marched_s_ = 0.0;
            events_.push_back({t, "Battery commander", "halt over: form the column"});
        }
        if (mode_ == Mode::Forming || mode_ == Mode::Marching) {
            bool formed = true;
            for (std::size_t k = 0; k < vehicles_.size(); ++k) {
                vehicles_[k].goal = PointAt(SlotArc(k));
                if (Dist(vehicles_[k].position, vehicles_[k].goal) > tactics_.slot_tolerance_m + Spacing() * 0.5) formed = false;
            }
            if (mode_ == Mode::Forming && formed && t >= hold_until_s_) {
                mode_ = Mode::Marching;
                events_.push_back({t, "Battery commander", "column formed: march"});
            }
            if (mode_ == Mode::Marching) {
                // The leader moves on as long as nobody behind has fallen too far back.
                bool keeping_up = true;
                for (std::size_t k = 0; k < vehicles_.size(); ++k)
                    if (Dist(vehicles_[k].position, vehicles_[k].goal) > 4.0 * Spacing() + 50.0) keeping_up = false;
                if (keeping_up && lead_s_ < total_ && t >= hold_until_s_) {
                    lead_s_ = std::min(total_, lead_s_ + tactics_.march_speed_mps * dt);
                    marched_s_ += dt;
                }
                if (lead_s_ >= total_ - 1e-6 && !arrived_logged_) {
                    arrived_logged_ = true;
                    events_.push_back({t, "Battery commander", "the head of the column is at the end of the route"});
                }
                if (marched_s_ >= tactics_.halt_interval_s) StartHalt(t, night_ ? tactics_.night_halt_s : tactics_.halt_duration_s,
                                                                      "scheduled halt: checks, fuel, rest");
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
            if (!v.serving.empty()) {  // a service run goes on whatever the battery does
                StepService(v, t, dt);
                continue;
            }
            if (mode_ == Mode::Halted || mode_ == Mode::Waiting || t < hold_until_s_ || v.waiting) {
                v.speed_mps = 0.0;
                continue;
            }
            Move(v, v.afloat ? tactics_.water_speed_mps : speed, dt, mode_ == Mode::Marching);
        }
        // Stopped at a short-halt point, a formation slot, an ambush point or a river position: deploy.
        if (mode_ == Mode::ShortHalt || mode_ == Mode::Formation || mode_ == Mode::Ambush || mode_ == Mode::RiverApproach ||
            mode_ == Mode::RiverCrossing) {
            for (Vehicle& v : vehicles_) {
                if (v.deployed || !v.serving.empty() || v.waiting) continue;
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
            if (!fits || !v.serving.empty() || v.stock <= 0.0) continue;
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
    // Vehicle k's place in the column: behind the leader by k spacings (never before the start of the route).
    double SlotArc(std::size_t k) const { return std::max(0.0, lead_s_ - Spacing() * static_cast<double>(k)); }

    void FormationSlots(const Vec3& covered, double axis_deg) {
        const double a = DegToRad(axis_deg);
        const Vec3 fwd{std::sin(a), std::cos(a), 0.0};
        const Vec3 right{std::cos(a), -std::sin(a), 0.0};
        std::vector<std::size_t> launchers, reloads, others;
        for (std::size_t k = 0; k < vehicles_.size(); ++k) {
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
            if (vehicles_[k].def.kind == BatteryVehicleKind::Launcher) out.push_back(k);
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
    bool emcon_ = false;
    bool night_ = false;
    bool drive_own_ = false;
    std::vector<Vec3> route_;
    std::vector<double> arcs_;
    double total_ = 0.0;
    double lead_s_ = 0.0;
    double marched_s_ = 0.0;
    double halt_until_s_ = 0.0;
    bool arrived_logged_ = false;
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
