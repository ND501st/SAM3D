// simcore/crew_mobility.hpp — the crew out of their seats: out of the vehicle and back in by its real hatches, on its
// hull, down to the ground and over the map, and what a man outside hears (docs/crew/13_OUT_OF_THE_VEHICLE.md).
//
// A crewman's body is always somewhere: seated, on his feet in the compartment, in a hatch, on the hull's top or on the
// ground. He goes from one to the next only by the ways the vehicle has (data/vehicles/<system>.json): up from his
// seat, along the compartment's floor to a hatch, the lid opened, up and out onto the roof, across the roof to a place
// where a wheel or a step takes him down, and over the ground; back the same way. Every leg takes its time; nothing
// teleports. The player walks himself (the engine sets his pose) or is walked along the same legs; an NPC is walked by
// his CrewMover when his drill needs him at a place outside (a control of the console worked outside, a "job").
//
//   VehicleDef       data/vehicles/<system>.json: the seats' standing points, the exits (hatches, emergency windows),
//                    the hull's top a man can stand on and where he must not, the ways down, the places round the
//                    vehicle, the jobs done there, the line-up for "К машинам" and the noise of the running machinery
//   WalkingDef       data/crew/walking.json: the pace (the drill regulation), the slope and woods, the voice's carry
//   VehiclePose      the body frame (x right, y forward, z up, metres, origin on the ground under the reference point)
//                    on the map (ENU), from the vehicle's place and heading
//   GroundSpeed      a man's pace over the terrain: walking or running, slowed by the slope (Tobler) and by woods
//   Earshot          whether a voice carries from one man to another: the effort, the distance, the noise, the hull
//   CrewMover        one crewman's body over time: plans the legs to a goal and walks them
#pragma once

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/crew_body.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/equipment.hpp"
#include "simcore/json.hpp"
#include "simcore/terrain.hpp"
#include "simcore/terrain_map.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

// ---------------------------------------------------------------------------------------------- the data

// A man's pace and how the ground slows him (data/crew/walking.json "gait").
struct GaitDef {
    double walk_mps = 1.44;     // stroevoy_ustav_1993 art. 31: 110-120 steps a minute of 70-80 cm
    double run_mps = 2.49;      // 165-180 steps a minute of 85-90 cm
    double cabin_mps = 0.9;     // stooping along a compartment's floor
    double hull_mps = 0.8;      // on a hull's top, minding the edges and the fittings
    double woods_factor = 0.6;  // through a wood (undergrowth, roots)
    double woods_canopy_m = 3.0;  // obstacles at least this tall count as woods
    double steepest = 1.0;      // the steepest slope he walks (rise over run); steeper is a wall to him
    double stand_up_s = 1.5;    // up from a seat (the crew file's stand_s when it has one)
    double sit_down_s = 1.5;
};

// How far a voice carries (data/crew/walking.json "earshot"): speech levels at 1 m, the background, what is needed.
struct EarshotDef {
    double talk_db = 66.0;      // a raised voice, A-weighted at 1 m (iso_9921)
    double shout_db = 78.0;     // a loud voice / shout at 1 m
    double quiet_db = 45.0;     // the countryside's background with nothing running near
    double needed_snr_db = 3.0; // the speech this much over the noise to be understood
    double hull_db = 25.0;      // lost through a closed hull
    double hatch_db = 8.0;      // lost through an open hatch (from the hatch)
};

struct WalkingDef {
    bool present = false;
    GaitDef gait;
    EarshotDef earshot;
};

// A rectangle in plan (body frame x and y).
struct PlanRect {
    double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
    bool Contains(double x, double y) const { return x >= x0 - 1e-9 && x <= x1 + 1e-9 && y >= y0 - 1e-9 && y <= y1 + 1e-9; }
};

struct VehicleKeepOut {
    std::string id;
    std::string why;
    PlanRect area;
};

// Where a seat's crewman stands up (feet on the floor, body frame) and the points he passes on his way to his exit.
struct VehicleSeat {
    std::string station;
    Vec3 stand_m;
    std::vector<Vec3> via_m;
};

// A way in and out: a roof hatch, a side hatch, an emergency window.
struct VehicleExit {
    std::string id;
    std::string label;
    std::string kind;                // roof_hatch, side_hatch, window
    bool emergency = false;          // only to get out in an emergency (the Osa's opening windscreen)
    Vec3 opening_m;                  // the opening's centre on the outside
    double width_m = 0.5;            // across (x)
    double length_m = 0.5;           // along (y)
    Vec3 under_m;                    // where he stands inside to climb out (feet)
    Vec3 out_m;                      // where he stands once out (feet): on the hull, or on the ground (to_ground)
    bool to_ground = false;          // out straight onto the ground (a window low on the bow)
    double open_s = 2.0;             // the lid opened (or shut)
    double climb_out_s = 4.0;
    double climb_in_s = 5.0;
    std::vector<std::string> stations;  // the seats that leave by it by drill
    std::vector<Condition> shut_when;   // all hold: the drill keeps it shut (combat work)
    std::string shut_why;               // said when it stays shut
};

// A way between the hull's top and the ground: a wheel, a step, a handhold.
struct VehicleWayDown {
    std::string id;
    std::string label;
    Vec3 top_m;                // on the hull
    Vec3 bottom_m;             // on the ground (z 0)
    std::vector<Vec3> holds_m; // the steps and handholds on the way (drawn)
    double down_s = 3.0;
    double up_s = 4.0;
};

// A place round the vehicle (body frame on the ground; z ignored) where a job is done.
struct VehiclePlace {
    std::string id;
    std::string label;
    Vec3 at_m;
    Vec3 face_m;               // the point he faces working there
};

// A control of the console worked outside, at a place (its owner seat's man goes there to work it).
struct VehicleJob {
    std::string control;
    std::string place;
    std::string station;       // the control's
    std::vector<Condition> stay_until;  // he stays at the place after working it until these hold (the work goes on)
};

// Machinery that drowns voices near it while it runs (its subsystem ready).
struct VehicleNoise {
    std::string subsystem;
    double db_at_1m = 90.0;
    Vec3 at_m;
};

// "К машинам": the crew lined up beside the vehicle, first man at first_m, each next one step_m on.
struct VehicleLineup {
    bool present = false;
    Vec3 first_m;
    Vec3 step_m;
    std::vector<std::string> order;
    Vec3 face_m;               // they face this point (the vehicle, or forward)
};

struct VehicleDef {
    std::string system_id;
    double hull_top_m = 2.0;
    std::vector<PlanRect> hull;            // where a man can stand on the top
    std::vector<VehicleKeepOut> keep_out;  // where he must not
    PlanRect footprint;                    // the hull in plan: men on the ground walk round it
    std::vector<VehicleSeat> seats;
    std::vector<VehicleExit> exits;
    std::vector<VehicleWayDown> ways;
    std::vector<VehiclePlace> places;
    std::vector<VehicleJob> jobs;
    std::vector<VehicleNoise> noise;
    VehicleLineup lineup;
    double inside_noise_db = 70.0;         // the compartment's own noise (fans) for voices inside

    const VehicleSeat* Seat(const std::string& station) const {
        for (const VehicleSeat& s : seats) {
            if (s.station == station) return &s;
        }
        return nullptr;
    }
    const VehicleExit* Exit(const std::string& id) const {
        for (const VehicleExit& e : exits) {
            if (e.id == id) return &e;
        }
        return nullptr;
    }
    const VehicleWayDown* Way(const std::string& id) const {
        for (const VehicleWayDown& w : ways) {
            if (w.id == id) return &w;
        }
        return nullptr;
    }
    const VehiclePlace* Place(const std::string& id) const {
        for (const VehiclePlace& p : places) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }
    // The place a control is worked at ("" : inside, at its seat).
    std::string JobPlace(const std::string& control) const {
        for (const VehicleJob& j : jobs) {
            if (j.control == control) return j.place;
        }
        return std::string();
    }
    // A plan point a man can stand on up top: on the hull and clear of every keep-out.
    bool OnHull(double x, double y) const {
        bool on = false;
        for (const PlanRect& r : hull) on = on || r.Contains(x, y);
        if (!on) return false;
        for (const VehicleKeepOut& k : keep_out) {
            if (k.area.Contains(x, y)) return false;
        }
        return true;
    }
    // The keep-out a plan point is in ("" none).
    std::string KeptOutBy(double x, double y) const {
        for (const VehicleKeepOut& k : keep_out) {
            if (k.area.Contains(x, y)) return k.id;
        }
        return std::string();
    }
    // The station's line-up point (on the ground, body frame); false if it has none.
    bool LineupPoint(const std::string& station, Vec3& out) const {
        if (!lineup.present) return false;
        for (std::size_t i = 0; i < lineup.order.size(); ++i) {
            if (lineup.order[i] == station) {
                out = lineup.first_m + lineup.step_m * static_cast<double>(i);
                out.z = 0.0;
                return true;
            }
        }
        return false;
    }
};

namespace detail {

class VehicleLoader : public ConsoleLoader {
public:
    bool Load(const json::Value& root, const ConsoleData& console, VehicleDef& out) {
        if (!Keys(root, "", {"system_id", "status", "notes", "source", "confidence", "hull", "keep_out", "seats", "exits",
                             "ways_down", "places", "jobs", "noise", "lineup", "inside_noise_db"}) ||
            !Provenance(root, "")) {
            return false;
        }
        if (!RequireString(root, "", "system_id", out.system_id)) return false;
        if (out.system_id != console.system_id) {
            return Fail("system_id", "'" + out.system_id + "' doesn't match the console's '" + console.system_id + "'");
        }
        if (!OptionalNumber(root, "", "inside_noise_db", out.inside_noise_db)) return false;
        return LoadHull(root, out) && LoadKeepOut(root, out) && LoadSeats(root, console, out) && LoadExits(root, console, out) &&
               LoadWays(root, out) && LoadPlaces(root, out) && LoadJobs(root, console, out) && LoadNoise(root, console, out) &&
               LoadLineup(root, console, out) && CheckSeats(console, out);
    }

private:
    bool Vec(const json::Value& obj, const std::string& path, const char* key, Vec3& out, bool required = true) {
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return required ? Fail(Join(path, key), "required") : true;
        if (!v->Is(json::Type::Array) || (v->items.size() != 2 && v->items.size() != 3)) {
            return Fail(Join(path, key), "expected [x, y] or [x, y, z]");
        }
        double xyz[3] = {0.0, 0.0, 0.0};
        for (std::size_t i = 0; i < v->items.size(); ++i) {
            if (!v->items[i].Is(json::Type::Number)) return Fail(Index(Join(path, key), i), "expected a number");
            xyz[i] = v->items[i].number;
        }
        out = Vec3{xyz[0], xyz[1], xyz[2]};
        return true;
    }
    bool Rect(const json::Value& obj, const std::string& path, PlanRect& out) {
        Vec3 a, b;
        if (!Vec(obj, path, "from_m", a) || !Vec(obj, path, "to_m", b)) return false;
        out.x0 = a.x < b.x ? a.x : b.x;
        out.x1 = a.x < b.x ? b.x : a.x;
        out.y0 = a.y < b.y ? a.y : b.y;
        out.y1 = a.y < b.y ? b.y : a.y;
        if (out.x1 - out.x0 < 0.05 || out.y1 - out.y0 < 0.05) return Fail(path, "too narrow");
        return true;
    }
    bool Times(const json::Value& obj, const std::string& path, std::initializer_list<std::pair<const char*, double*>> keys) {
        for (const auto& k : keys) {
            if (!OptionalNumber(obj, path, k.first, *k.second)) return false;
            if (*k.second < 0.0 || *k.second > 120.0) return Fail(Join(path, k.first), "must be from 0 to 120 s");
        }
        return true;
    }
    bool KnownStation(const ConsoleData& console, const std::string& s) const {
        return console.stations.empty() || console.FindStation(s) != nullptr;
    }

    bool LoadHull(const json::Value& root, VehicleDef& out) {
        const json::Value* h = root.Find("hull");
        if (h == nullptr) return Fail("hull", "required");
        if (!Keys(*h, "hull", {"top_m", "areas", "footprint", "source", "confidence", "notes"}) || !Provenance(*h, "hull")) return false;
        if (h->Find("top_m") == nullptr) return Fail("hull.top_m", "required");
        if (!OptionalNumber(*h, "hull", "top_m", out.hull_top_m)) return false;
        if (out.hull_top_m < 0.5 || out.hull_top_m > 5.0) return Fail("hull.top_m", "must be from 0.5 to 5 m");
        const json::Value* fp = h->Find("footprint");
        if (fp == nullptr) return Fail("hull.footprint", "required");
        if (!Keys(*fp, "hull.footprint", {"from_m", "to_m", "notes"}) || !Rect(*fp, "hull.footprint", out.footprint)) return false;
        const json::Value* arr = OptionalArray(*h, "hull", "areas");
        if (arr == nullptr || arr->items.empty()) return Fail("hull.areas", "required");
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("hull.areas", i);
            if (!Keys(arr->items[i], p, {"from_m", "to_m", "notes"})) return false;
            PlanRect r;
            if (!Rect(arr->items[i], p, r)) return false;
            out.hull.push_back(r);
        }
        return true;
    }

    bool LoadKeepOut(const json::Value& root, VehicleDef& out) {
        const json::Value* arr = OptionalArray(root, "", "keep_out");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("keep_out", i);
            const json::Value& k = arr->items[i];
            if (!Keys(k, p, {"id", "why", "from_m", "to_m", "source", "confidence", "notes"}) || !Provenance(k, p)) return false;
            VehicleKeepOut ko;
            if (!RequireString(k, p, "id", ko.id) || !RequireString(k, p, "why", ko.why) || !Rect(k, p, ko.area)) return false;
            out.keep_out.push_back(ko);
        }
        return true;
    }

    bool LoadSeats(const json::Value& root, const ConsoleData& console, VehicleDef& out) {
        const json::Value* arr = OptionalArray(root, "", "seats");
        if (arr == nullptr) return message_.empty() ? Fail("seats", "required") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("seats", i);
            const json::Value& s = arr->items[i];
            if (!Keys(s, p, {"station", "stand_m", "via_m", "notes"})) return false;
            VehicleSeat seat;
            if (!RequireString(s, p, "station", seat.station) || !Vec(s, p, "stand_m", seat.stand_m)) return false;
            if (!KnownStation(console, seat.station)) return Fail(Join(p, "station"), "unknown station '" + seat.station + "'");
            if (out.Seat(seat.station) != nullptr) return Fail(Join(p, "station"), "duplicate seat '" + seat.station + "'");
            if (const json::Value* via = OptionalArray(s, p, "via_m")) {
                for (std::size_t j = 0; j < via->items.size(); ++j) {
                    const json::Value& pt = via->items[j];
                    if (!pt.Is(json::Type::Array) || pt.items.size() != 2 || !pt.items[0].Is(json::Type::Number) ||
                        !pt.items[1].Is(json::Type::Number)) {
                        return Fail(Index(Join(p, "via_m"), j), "expected [x, y]");
                    }
                    seat.via_m.push_back(Vec3{pt.items[0].number, pt.items[1].number, seat.stand_m.z});
                }
            } else if (!message_.empty()) {
                return false;
            }
            out.seats.push_back(seat);
        }
        return true;
    }

    bool LoadExits(const json::Value& root, const ConsoleData& console, VehicleDef& out) {
        const json::Value* arr = OptionalArray(root, "", "exits");
        if (arr == nullptr || arr->items.empty()) return message_.empty() ? Fail("exits", "required") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("exits", i);
            const json::Value& e = arr->items[i];
            if (!Keys(e, p, {"id", "label", "kind", "emergency", "opening_m", "size_m", "under_m", "out_m", "to_ground", "open_s",
                             "climb_out_s", "climb_in_s", "stations", "shut_when", "shut_why", "source", "confidence", "notes"}) ||
                !Provenance(e, p)) {
                return false;
            }
            VehicleExit x;
            if (!RequireString(e, p, "id", x.id) || !RequireString(e, p, "label", x.label) || !RequireString(e, p, "kind", x.kind) ||
                !OptionalBool(e, p, "emergency", x.emergency) || !OptionalBool(e, p, "to_ground", x.to_ground) ||
                !Vec(e, p, "opening_m", x.opening_m) || !Vec(e, p, "under_m", x.under_m) || !Vec(e, p, "out_m", x.out_m) ||
                !OptionalString(e, p, "shut_why", x.shut_why) ||
                !Times(e, p, {{"open_s", &x.open_s}, {"climb_out_s", &x.climb_out_s}, {"climb_in_s", &x.climb_in_s}})) {
                return false;
            }
            if (x.kind != "roof_hatch" && x.kind != "side_hatch" && x.kind != "window") {
                return Fail(Join(p, "kind"), "must be roof_hatch, side_hatch or window");
            }
            Vec3 size{0.5, 0.5, 0.0};
            if (!Vec(e, p, "size_m", size, false)) return false;
            x.width_m = size.x;
            x.length_m = size.y;
            if (out.Exit(x.id) != nullptr) return Fail(Join(p, "id"), "duplicate exit '" + x.id + "'");
            if (!StringList(e, p, "stations", x.stations, [&](const std::string& s) {
                    return KnownStation(console, s) ? std::string() : "unknown station '" + s + "'";
                })) {
                return false;
            }
            if (!LoadConditions(e, p, "shut_when", console.panel, console.equipment, x.shut_when)) return false;
            if (!x.shut_when.empty() && x.shut_why.empty()) return Fail(Join(p, "shut_why"), "required with shut_when");
            if (x.to_ground && std::fabs(x.out_m.z) > 1e-6) return Fail(Join(p, "out_m"), "on the ground (z 0) for to_ground");
            if (!x.to_ground && !out.OnHull(x.out_m.x, x.out_m.y)) {
                return Fail(Join(p, "out_m"), "not on the hull's top, or in a keep-out");
            }
            if (x.under_m.z >= x.opening_m.z) return Fail(Join(p, "under_m"), "must be below the opening");
            out.exits.push_back(x);
        }
        return true;
    }

    bool LoadWays(const json::Value& root, VehicleDef& out) {
        const json::Value* arr = OptionalArray(root, "", "ways_down");
        if (arr == nullptr || arr->items.empty()) return message_.empty() ? Fail("ways_down", "required") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("ways_down", i);
            const json::Value& w = arr->items[i];
            if (!Keys(w, p, {"id", "label", "top_m", "bottom_m", "holds_m", "down_s", "up_s", "source", "confidence", "notes"}) ||
                !Provenance(w, p)) {
                return false;
            }
            VehicleWayDown d;
            if (!RequireString(w, p, "id", d.id) || !RequireString(w, p, "label", d.label) || !Vec(w, p, "top_m", d.top_m) ||
                !Vec(w, p, "bottom_m", d.bottom_m) || !Times(w, p, {{"down_s", &d.down_s}, {"up_s", &d.up_s}})) {
                return false;
            }
            if (!out.OnHull(d.top_m.x, d.top_m.y)) return Fail(Join(p, "top_m"), "not on the hull's top, or in a keep-out");
            if (std::fabs(d.bottom_m.z) > 1e-6) return Fail(Join(p, "bottom_m"), "on the ground (z 0)");
            if (out.footprint.Contains(d.bottom_m.x, d.bottom_m.y)) return Fail(Join(p, "bottom_m"), "under the hull");
            if (const json::Value* holds = OptionalArray(w, p, "holds_m")) {
                for (std::size_t j = 0; j < holds->items.size(); ++j) {
                    const json::Value& pt = holds->items[j];
                    if (!pt.Is(json::Type::Array) || pt.items.size() != 3 || !pt.items[0].Is(json::Type::Number) ||
                        !pt.items[1].Is(json::Type::Number) || !pt.items[2].Is(json::Type::Number)) {
                        return Fail(Index(Join(p, "holds_m"), j), "expected [x, y, z]");
                    }
                    d.holds_m.push_back(Vec3{pt.items[0].number, pt.items[1].number, pt.items[2].number});
                }
            } else if (!message_.empty()) {
                return false;
            }
            if (out.Way(d.id) != nullptr) return Fail(Join(p, "id"), "duplicate way '" + d.id + "'");
            out.ways.push_back(d);
        }
        return true;
    }

    bool LoadPlaces(const json::Value& root, VehicleDef& out) {
        const json::Value* arr = OptionalArray(root, "", "places");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("places", i);
            const json::Value& pl = arr->items[i];
            if (!Keys(pl, p, {"id", "label", "at_m", "face_m", "source", "confidence", "notes"}) || !Provenance(pl, p)) return false;
            VehiclePlace v;
            if (!RequireString(pl, p, "id", v.id) || !RequireString(pl, p, "label", v.label) || !Vec(pl, p, "at_m", v.at_m) ||
                !Vec(pl, p, "face_m", v.face_m)) {
                return false;
            }
            v.at_m.z = 0.0;
            if (out.footprint.Contains(v.at_m.x, v.at_m.y)) return Fail(Join(p, "at_m"), "under the hull");
            if (out.Place(v.id) != nullptr) return Fail(Join(p, "id"), "duplicate place '" + v.id + "'");
            out.places.push_back(v);
        }
        return true;
    }

    bool LoadJobs(const json::Value& root, const ConsoleData& console, VehicleDef& out) {
        const json::Value* arr = OptionalArray(root, "", "jobs");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("jobs", i);
            const json::Value& j = arr->items[i];
            if (!Keys(j, p, {"control", "place", "stay_until", "source", "confidence", "notes"}) || !Provenance(j, p)) return false;
            VehicleJob job;
            if (!RequireString(j, p, "control", job.control) || !RequireString(j, p, "place", job.place)) return false;
            const ControlDef* c = console.panel.Find(job.control);
            if (c == nullptr) return Fail(Join(p, "control"), "unknown control '" + job.control + "'");
            if (c->station.empty()) return Fail(Join(p, "control"), "'" + job.control + "' has no owner seat");
            if (out.Place(job.place) == nullptr) return Fail(Join(p, "place"), "unknown place '" + job.place + "'");
            if (!out.JobPlace(job.control).empty()) return Fail(Join(p, "control"), "duplicate job '" + job.control + "'");
            job.station = c->station;
            if (!LoadConditions(j, p, "stay_until", console.panel, console.equipment, job.stay_until)) return false;
            out.jobs.push_back(job);
        }
        return true;
    }

    bool LoadNoise(const json::Value& root, const ConsoleData& console, VehicleDef& out) {
        const json::Value* arr = OptionalArray(root, "", "noise");
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("noise", i);
            const json::Value& n = arr->items[i];
            if (!Keys(n, p, {"subsystem", "db_at_1m", "at_m", "source", "confidence", "notes"}) || !Provenance(n, p)) return false;
            VehicleNoise v;
            if (!RequireString(n, p, "subsystem", v.subsystem) || !Vec(n, p, "at_m", v.at_m)) return false;
            if (n.Find("db_at_1m") == nullptr) return Fail(Join(p, "db_at_1m"), "required");
            if (!OptionalNumber(n, p, "db_at_1m", v.db_at_1m)) return false;
            if (!console.equipment.Has(v.subsystem)) return Fail(Join(p, "subsystem"), "unknown subsystem '" + v.subsystem + "'");
            out.noise.push_back(v);
        }
        return true;
    }

    bool LoadLineup(const json::Value& root, const ConsoleData& console, VehicleDef& out) {
        const json::Value* l = root.Find("lineup");
        if (l == nullptr) return true;
        if (!Keys(*l, "lineup", {"first_m", "step_m", "face_m", "order", "source", "confidence", "notes"}) || !Provenance(*l, "lineup")) {
            return false;
        }
        out.lineup.present = true;
        if (!Vec(*l, "lineup", "first_m", out.lineup.first_m) || !Vec(*l, "lineup", "step_m", out.lineup.step_m) ||
            !Vec(*l, "lineup", "face_m", out.lineup.face_m)) {
            return false;
        }
        if (!StringList(*l, "lineup", "order", out.lineup.order, [&](const std::string& s) {
                return KnownStation(console, s) ? std::string() : "unknown station '" + s + "'";
            })) {
            return false;
        }
        for (std::size_t i = 0; i < out.lineup.order.size(); ++i) {
            const Vec3 at = out.lineup.first_m + out.lineup.step_m * static_cast<double>(i);
            if (out.footprint.Contains(at.x, at.y)) return Fail(Index("lineup.order", i), "his place is under the hull");
        }
        return true;
    }

    // Every seat of the console has a standing point and a usual exit.
    bool CheckSeats(const ConsoleData& console, const VehicleDef& out) {
        for (const ConsoleStation& st : console.stations) {
            if (out.Seat(st.id) == nullptr) return Fail("seats", "no standing point for the " + st.id);
            bool has_exit = false;
            for (const VehicleExit& e : out.exits) {
                for (const std::string& s : e.stations) has_exit = has_exit || (s == st.id && !e.emergency);
            }
            if (!has_exit) return Fail("exits", "no exit for the " + st.id);
        }
        return true;
    }
};

class WalkingLoader : public ConsoleLoader {
public:
    bool Load(const json::Value& root, WalkingDef& out) {
        if (!Keys(root, "", {"status", "notes", "source", "confidence", "gait", "earshot"}) || !Provenance(root, "")) return false;
        GaitDef& g = out.gait;
        EarshotDef& e = out.earshot;
        struct Field {
            const char* key;
            double* value;
            double low;
            double high;
        };
        const std::pair<const char*, std::vector<Field>> blocks[] = {
            {"gait",
             {{"walk_mps", &g.walk_mps, 0.3, 3.0},
              {"run_mps", &g.run_mps, 1.0, 6.0},
              {"cabin_mps", &g.cabin_mps, 0.1, 2.0},
              {"hull_mps", &g.hull_mps, 0.1, 2.0},
              {"woods_factor", &g.woods_factor, 0.1, 1.0},
              {"woods_canopy_m", &g.woods_canopy_m, 0.5, 30.0},
              {"steepest", &g.steepest, 0.1, 3.0},
              {"stand_up_s", &g.stand_up_s, 0.0, 10.0},
              {"sit_down_s", &g.sit_down_s, 0.0, 10.0}}},
            {"earshot",
             {{"talk_db", &e.talk_db, 40.0, 100.0},
              {"shout_db", &e.shout_db, 40.0, 110.0},
              {"quiet_db", &e.quiet_db, 0.0, 90.0},
              {"needed_snr_db", &e.needed_snr_db, -20.0, 30.0},
              {"hull_db", &e.hull_db, 0.0, 80.0},
              {"hatch_db", &e.hatch_db, 0.0, 40.0}}},
        };
        for (const auto& b : blocks) {
            const json::Value* v = root.Find(b.first);
            if (v == nullptr) return Fail(b.first, "required");
            if (!v->Is(json::Type::Object)) return Fail(b.first, "expected an object");
            for (const std::string& k : v->keys) {
                bool ok = k == "source" || k == "confidence" || k == "notes";
                for (const Field& f : b.second) ok = ok || k == f.key;
                if (!ok) return Fail(Join(b.first, k), "unknown key");
            }
            if (!Provenance(*v, b.first)) return false;
            for (const Field& f : b.second) {
                if (v->Find(f.key) == nullptr) return Fail(Join(b.first, f.key), "required");
                if (!OptionalNumber(*v, b.first, f.key, *f.value)) return false;
                if (*f.value < f.low || *f.value > f.high) {
                    return Fail(Join(b.first, f.key), "must be from " + std::to_string(f.low) + " to " + std::to_string(f.high));
                }
            }
        }
        if (g.run_mps <= g.walk_mps) return Fail("gait.run_mps", "must be faster than walking");
        if (e.shout_db < e.talk_db) return Fail("earshot.shout_db", "must be louder than talk_db");
        out.present = true;
        return true;
    }
};

}  // namespace detail

inline bool LoadVehicle(const std::string& json_text, const ConsoleData& console, VehicleDef& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(json_text, root, &parse_error)) {
        if (error != nullptr) *error = parse_error;
        return false;
    }
    VehicleDef v;
    detail::VehicleLoader loader;
    if (!loader.Load(root, console, v)) {
        if (error != nullptr) *error = loader.Message();
        return false;
    }
    out = std::move(v);
    return true;
}

inline bool LoadVehicleFile(const std::string& path, const ConsoleData& console, VehicleDef& out, std::string* error = nullptr) {
    std::string text;
    if (!detail::ReadTextFile(path, text)) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    if (!LoadVehicle(text, console, out, error)) {
        if (error != nullptr) *error = path + ": " + *error;
        return false;
    }
    return true;
}

inline bool LoadWalking(const std::string& json_text, WalkingDef& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(json_text, root, &parse_error)) {
        if (error != nullptr) *error = parse_error;
        return false;
    }
    WalkingDef w;
    detail::WalkingLoader loader;
    if (!loader.Load(root, w)) {
        if (error != nullptr) *error = loader.Message();
        return false;
    }
    out = w;
    return true;
}

inline bool LoadWalkingFile(const std::string& path, WalkingDef& out, std::string* error = nullptr) {
    std::string text;
    if (!detail::ReadTextFile(path, text)) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    if (!LoadWalking(text, out, error)) {
        if (error != nullptr) *error = path + ": " + *error;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------- the vehicle on the map

// Where the vehicle's body frame is on the map: its origin (on the ground under the reference point, ENU) and its
// heading (radians clockwise from north).
struct VehiclePose {
    Vec3 origin;
    double heading_rad = 0.0;

    Vec3 ToEnu(const Vec3& body) const {
        const double s = std::sin(heading_rad), c = std::cos(heading_rad);
        return Vec3{origin.x + body.y * s + body.x * c, origin.y + body.y * c - body.x * s, origin.z + body.z};
    }
    Vec3 ToBody(const Vec3& enu) const {
        const double s = std::sin(heading_rad), c = std::cos(heading_rad);
        const double de = enu.x - origin.x, dn = enu.y - origin.y;
        return Vec3{de * c - dn * s, de * s + dn * c, enu.z - origin.z};
    }
    // A heading in the body frame (clockwise from forward) on the map, and back.
    double ToMapHeading(double body_yaw_rad) const { return WrapAngle(body_yaw_rad + heading_rad); }
    double ToBodyYaw(double map_heading_rad) const { return WrapAngle(map_heading_rad - heading_rad); }
    static double WrapAngle(double a) { return std::remainder(a, kTwoPi); }
};

// ---------------------------------------------------------------------------------------------- the pace

// Tobler's hiking function as a factor of the pace on the flat: fastest a little downhill (a slope of -0.05), slower up
// and down steeper. tobler_1993: v = 6 exp(-3.5 |s + 0.05|) km/h.
inline double ToblerFactor(double slope) { return std::exp(-3.5 * (std::fabs(slope + 0.05) - 0.05)); }

// A man's pace at (e, n) going towards (dir_e, dir_n) (a unit vector): walking or running, the slope ahead (the ground
// one metre on), and a wood if the map stands one there. Zero where the slope is too steep for him.
inline double GroundSpeed(const GaitDef& g, const Terrain* ground, const MapTerrain* map, double e, double n, double dir_e,
                          double dir_n, bool run) {
    double v = run ? g.run_mps : g.walk_mps;
    if (ground != nullptr) {
        const double slope = ground->HeightAt(e + dir_e, n + dir_n) - ground->HeightAt(e, n);
        if (std::fabs(slope) > g.steepest) return 0.0;
        v *= ToblerFactor(slope);
    }
    if (map != nullptr && map->ObstacleHeightAt(e, n) >= g.woods_canopy_m) v *= g.woods_factor;
    return v;
}

// ---------------------------------------------------------------------------------------------- the voice

// Speech at `distance_m` from a mouth speaking at `db_at_1m` (spherical spreading, 6 dB a doubling).
inline double SpeechLevelAt(double db_at_1m, double distance_m) {
    return db_at_1m - 20.0 * std::log10(distance_m > 1.0 ? distance_m : 1.0);
}
// Noise at `distance_m` from a source of `db_at_1m`, added to a background (energies add).
inline double AddNoise(double background_db, double db_at_1m, double distance_m) {
    const double src = SpeechLevelAt(db_at_1m, distance_m);
    return 10.0 * std::log10(std::pow(10.0, background_db / 10.0) + std::pow(10.0, src / 10.0));
}
// The farthest a voice of `db_at_1m` is understood over `noise_db` (needing `snr_db` over it).
inline double EarshotRange(double db_at_1m, double noise_db, double snr_db) {
    return std::pow(10.0, (db_at_1m - noise_db - snr_db) / 20.0);
}

// ---------------------------------------------------------------------------------------------- the body moving

// Where a crewman's body is.
enum class BodyPlace { Seated, Cabin, Hatch, Hull, Ground };

inline const char* ToString(BodyPlace p) {
    switch (p) {
        case BodyPlace::Seated: return "seated";
        case BodyPlace::Cabin: return "on his feet inside";
        case BodyPlace::Hatch: return "in the hatch";
        case BodyPlace::Hull: return "on the hull";
        case BodyPlace::Ground: return "on the ground";
    }
    return "?";
}
// Inside the compartment (his headset can reach its box; his eyes its instruments).
inline bool IsInside(BodyPlace p) { return p == BodyPlace::Seated || p == BodyPlace::Cabin; }

// One leg of a way: what he does, from where to where (body frame, or the map for a walk to a map point), and how long a
// timed leg takes (walks go at their pace).
enum class LegKind { StandUp, CabinWalk, OpenHatch, ClimbOut, HullWalk, ClimbDown, GroundWalk, ClimbUp, ClimbIn, ShutHatch, SitDown };

inline const char* ToString(LegKind k) {
    switch (k) {
        case LegKind::StandUp: return "stands up";
        case LegKind::CabinWalk: return "walks inside";
        case LegKind::OpenHatch: return "opens the hatch";
        case LegKind::ClimbOut: return "climbs out";
        case LegKind::HullWalk: return "crosses the hull";
        case LegKind::ClimbDown: return "climbs down";
        case LegKind::GroundWalk: return "walks";
        case LegKind::ClimbUp: return "climbs up";
        case LegKind::ClimbIn: return "climbs in";
        case LegKind::ShutHatch: return "shuts the hatch";
        case LegKind::SitDown: return "sits down";
    }
    return "?";
}

struct MoveLeg {
    LegKind kind = LegKind::CabinWalk;
    Vec3 to;                  // body frame, or ENU when to_map
    bool to_map = false;
    double duration_s = 0.0;  // timed legs
    std::string exit;         // the hatch it goes through
    std::string way;          // the way down it uses
    BodyPlace after = BodyPlace::Cabin;  // where he is once it is done
};

// Where to go. Seat: his own seat. Place: a place round the vehicle. Lineup: his place in the line-up. Ground: a point
// on the map (ENU). Hull, Cabin: a point on the hull's top or the compartment's floor (body frame).
struct MoveGoal {
    enum class Kind { Seat, Place, Lineup, Ground, Hull, Cabin };
    Kind kind = Kind::Seat;
    std::string place;
    Vec3 point;
    bool run = false;
    bool emergency = false;   // the emergency exits may be used
    bool shut_after = false;  // coming in: shut the hatch behind him (the last man in)

    static MoveGoal Seat() { return MoveGoal(); }
    static MoveGoal AtPlace(const std::string& id) {
        MoveGoal g;
        g.kind = Kind::Place;
        g.place = id;
        return g;
    }
    static MoveGoal InLineup() {
        MoveGoal g;
        g.kind = Kind::Lineup;
        return g;
    }
    static MoveGoal OnGround(const Vec3& enu) {
        MoveGoal g;
        g.kind = Kind::Ground;
        g.point = enu;
        return g;
    }
    static MoveGoal OnHull(const Vec3& body) {
        MoveGoal g;
        g.kind = Kind::Hull;
        g.point = body;
        return g;
    }
    static MoveGoal InCabin(const Vec3& body) {
        MoveGoal g;
        g.kind = Kind::Cabin;
        g.point = body;
        return g;
    }
};

// A crewman's body as it is now, for renderers, the voice and the tests.
struct MoverView {
    std::string station;
    BodyPlace place = BodyPlace::Seated;
    bool inside = true;
    bool moving = false;
    bool leg_active = false;
    LegKind leg = LegKind::StandUp;
    double leg_progress = 0.0;  // 0..1 through the leg
    std::string exit;           // the hatch he is in or going through
    std::string at;             // the goal he has reached: "" his seat, a place's id, "lineup", "point"
    Vec3 feet_m;                // the vehicle's body frame
    Vec3 feet_enu;
    double yaw_m = 0.0;         // facing, clockwise from the vehicle's forward
    double heading_rad = 0.0;   // facing on the map, clockwise from north
    bool running = false;
};

// One crewman's body over the vehicle and the ground (docs/crew/13_OUT_OF_THE_VEHICLE.md). It knows his seat, where he
// is and the legs left to his goal; Update walks them. Positions on the vehicle are kept in its body frame (they move
// with it), on the ground on the map.
class CrewMover {
public:
    CrewMover() = default;
    CrewMover(const VehicleDef* vehicle, std::string station) : vehicle_(vehicle), station_(std::move(station)) {
        if (vehicle_ != nullptr) {
            if (const VehicleSeat* s = vehicle_->Seat(station_)) pos_ = s->stand_m;
        }
    }

    const std::string& Station() const { return station_; }
    BodyPlace Place() const { return place_; }
    // In the compartment: seated, on his feet inside, or not yet half out of a hatch (or more than half in).
    bool Inside() const {
        if (legs_.empty()) return IsInside(place_);
        const MoveLeg& l = legs_.front();
        const double dur = l.duration_s * pace_;
        const double u = dur > 0.0 ? leg_s_ / dur : 0.0;
        if (l.kind == LegKind::ClimbOut) return u < 0.5;
        if (l.kind == LegKind::ClimbIn) return u >= 0.5;
        return IsInside(place_);
    }
    bool Moving() const { return !legs_.empty(); }
    const std::vector<MoveLeg>& Legs() const { return legs_; }
    // The goal he is going to or at ("" his seat).
    const std::string& At() const { return at_; }
    bool Arrived() const { return legs_.empty(); }
    // The seconds left on his way at his pace (walks at their flat pace).
    double SecondsLeft(const GaitDef& g) const {
        double s = 0.0;
        Vec3 from = pos_;
        for (std::size_t i = 0; i < legs_.size(); ++i) {
            const MoveLeg& l = legs_[i];
            const double done = i == 0 ? leg_s_ : 0.0;
            switch (l.kind) {
                case LegKind::CabinWalk: s += (l.to - from).HorizontalLength() / g.cabin_mps - done; break;
                case LegKind::HullWalk: s += (l.to - from).HorizontalLength() / g.hull_mps - done; break;
                case LegKind::GroundWalk: s += 0.0; break;  // depends on the ground: not counted here
                default: s += l.duration_s * pace_ - done; break;
            }
            if (!l.to_map) from = l.to;
        }
        return s > 0.0 ? s : 0.0;
    }
    // A factor on every timed leg (the skill: 1 for a trained man, more for a recruit).
    void SetPace(double factor) { pace_ = factor > 0.1 ? factor : 0.1; }
    double Pace() const { return pace_; }
    // Getting up from his seat and sitting back down (the crew file's stand_s and sit_s, or the gait's).
    void SetSeatTimes(double stand_up_s, double sit_down_s) {
        stand_up_s_ = stand_up_s;
        sit_down_s_ = sit_down_s;
    }

    // Plan the way to a goal from where he is. False (and why) if there is none: the exits shut by the drill, a place
    // the vehicle doesn't have. A goal given while he is on his way takes over when the current leg ends.
    bool GoTo(const MoveGoal& goal, const ConsoleData* console, const VehiclePose& pose, std::string* why = nullptr) {
        if (vehicle_ == nullptr) return Refuse(why, "no vehicle data");
        std::vector<MoveLeg> legs;
        // Plan from where the current leg ends (he finishes the leg he is on).
        BodyPlace from_place = place_;
        Vec3 from = pos_;
        bool from_map = place_ == BodyPlace::Ground;
        if (!legs_.empty()) {
            const MoveLeg& cur = legs_.front();
            from_place = cur.after;
            from = cur.to;
            from_map = cur.to_map;
            if (!from_map && from_place == BodyPlace::Ground) {  // a way's bottom in the body frame: on the map now
                from = pose.ToEnu(from);
                from_map = true;
            }
            legs.push_back(cur);
        }
        if (from_place == BodyPlace::Ground && !from_map) from = pose.ToEnu(from);
        const std::size_t kept = legs.size();
        if (!Plan(goal, console, pose, from_place, from, legs, why)) return false;
        legs_ = std::move(legs);
        if (kept == 0) {
            leg_s_ = 0.0;
            leg_dist_ = 0.0;
        }
        goal_ = goal;
        at_ = GoalName(goal);
        stuck_ = false;
        return true;
    }

    // Stop where he is (the end of the current leg).
    void Halt() {
        if (legs_.size() > 1) legs_.resize(1);
    }

    // The player walks himself: his place and his feet (body frame inside and on the hull, the map on the ground), his
    // facing. Any way planned is dropped.
    void Hold(BodyPlace place, const Vec3& feet, double yaw_or_heading_rad, const std::string& at = std::string("point")) {
        legs_.clear();
        leg_s_ = 0.0;
        place_ = place;
        pos_ = feet;
        facing_ = yaw_or_heading_rad;
        at_ = place == BodyPlace::Seated ? std::string() : at;
    }
    // Sat down at his seat (the engine's X, a seat taken).
    void SitDown() {
        legs_.clear();
        leg_s_ = 0.0;
        place_ = BodyPlace::Seated;
        if (vehicle_ != nullptr) {
            if (const VehicleSeat* s = vehicle_->Seat(station_)) pos_ = s->stand_m;
        }
        at_.clear();
    }

    // Walk on by dt. `hatches` holds which exits stand open (the mover opens and shuts them); `still_out` (may be null)
    // how many other men are still out who come in by each exit: he leaves a hatch open for them.
    void Update(double dt, const VehiclePose& pose, const Terrain* ground, const MapTerrain* map, const GaitDef& gait,
                std::map<std::string, bool>& hatches, const std::map<std::string, int>* still_out = nullptr) {
        double left = dt;
        int guard = 0;
        while (left > 1e-12 && !legs_.empty() && guard++ < 16) {
            MoveLeg& l = legs_.front();
            if (leg_s_ == 0.0 && leg_dist_ == 0.0) StartLeg(l, pose, hatches, still_out);
            double used = left;
            bool done = false;
            switch (l.kind) {
                case LegKind::CabinWalk:
                case LegKind::HullWalk: {
                    const double v = l.kind == LegKind::CabinWalk ? gait.cabin_mps : gait.hull_mps;
                    const Vec3 d = l.to - pos_;
                    const double len = d.HorizontalLength();
                    const double step = v / pace_ * left;
                    if (len > 1e-9) facing_ = std::atan2(d.x, d.y);
                    if (step >= len) {
                        used = v > 0.0 ? len / (v / pace_) : left;
                        pos_ = l.to;
                        done = true;
                    } else {
                        pos_ = pos_ + d * (step / len);
                    }
                    break;
                }
                case LegKind::GroundWalk: {
                    Vec3 to = l.to_map ? l.to : pose.ToEnu(l.to);
                    const double de = to.x - pos_.x, dn = to.y - pos_.y;
                    const double len = std::sqrt(de * de + dn * dn);
                    if (len < 1e-6) {
                        used = 0.0;
                        done = true;
                        break;
                    }
                    const double ue = de / len, un = dn / len;
                    facing_ = std::atan2(ue, un);
                    const double v = GroundSpeed(gait, ground, map, pos_.x, pos_.y, ue, un, goal_.run);
                    if (v <= 0.0) {  // a wall to him: he stops there
                        legs_.clear();
                        stuck_ = true;
                        return;
                    }
                    const double step = v * left;
                    if (step >= len) {
                        used = len / v;
                        pos_.x = to.x;
                        pos_.y = to.y;
                        done = true;
                    } else {
                        pos_.x += ue * step;
                        pos_.y += un * step;
                    }
                    pos_.z = ground != nullptr ? ground->HeightAt(pos_.x, pos_.y) : pose.origin.z;
                    break;
                }
                default: {
                    const double dur = l.duration_s * pace_;
                    if (leg_s_ + left >= dur) {
                        used = dur - leg_s_;
                        done = true;
                    }
                    leg_s_ += used;
                    break;
                }
            }
            if (l.kind == LegKind::CabinWalk || l.kind == LegKind::HullWalk || l.kind == LegKind::GroundWalk) leg_s_ += used;
            left -= used;
            if (done) EndLeg(l, pose, ground, hatches);
        }
    }

    // How he is now (for renderers and the voice).
    MoverView View(const VehiclePose& pose) const {
        MoverView v;
        v.station = station_;
        v.place = place_;
        v.moving = Moving();
        v.at = at_;
        v.running = goal_.run && Moving();
        if (!legs_.empty()) {
            const MoveLeg& l = legs_.front();
            v.leg_active = true;
            v.leg = l.kind;
            v.exit = l.exit;
            const double dur = l.duration_s * pace_;
            v.leg_progress = dur > 0.0 ? Clamp(leg_s_ / dur, 0.0, 1.0) : 0.0;
            if (l.kind == LegKind::CabinWalk || l.kind == LegKind::HullWalk || l.kind == LegKind::GroundWalk) v.leg_progress = 0.0;
        }
        // Feet: walks are where pos_ is; timed legs between where they started and where they end.
        Vec3 feet_enu = place_ == BodyPlace::Ground ? pos_ : pose.ToEnu(pos_);
        if (v.leg_active && IsTimed(v.leg) && v.leg_progress > 0.0) {
            const MoveLeg& l = legs_.front();
            const Vec3 a = feet_enu;
            const Vec3 b = l.to_map ? l.to : pose.ToEnu(l.to);
            const double u = v.leg_progress * v.leg_progress * (3.0 - 2.0 * v.leg_progress);
            feet_enu = a + (b - a) * u;
        }
        v.feet_enu = feet_enu;
        v.feet_m = pose.ToBody(feet_enu);
        const bool map_facing = place_ == BodyPlace::Ground;
        v.heading_rad = map_facing ? facing_ : pose.ToMapHeading(facing_);
        v.yaw_m = map_facing ? pose.ToBodyYaw(facing_) : facing_;
        v.inside = IsInside(place_) && !(v.leg_active && v.leg == LegKind::ClimbOut && v.leg_progress > 0.5);
        if (v.leg_active && v.leg == LegKind::ClimbIn && v.leg_progress > 0.5) v.inside = true;
        if (v.leg_active && (v.leg == LegKind::ClimbOut || v.leg == LegKind::ClimbIn)) v.place = BodyPlace::Hatch;
        return v;
    }

    // He found the ground too steep on his way and stopped (cleared by the next goal).
    bool Stuck() const { return stuck_; }

private:
    static bool IsTimed(LegKind k) { return k != LegKind::CabinWalk && k != LegKind::HullWalk && k != LegKind::GroundWalk; }

    static bool Refuse(std::string* why, const std::string& text) {
        if (why != nullptr) *why = text;
        return false;
    }
    static std::string GoalName(const MoveGoal& g) {
        switch (g.kind) {
            case MoveGoal::Kind::Seat: return std::string();
            case MoveGoal::Kind::Place: return g.place;
            case MoveGoal::Kind::Lineup: return "lineup";
            default: return "point";
        }
    }

    // The exit he takes: his own usual one (open by the drill), else (an emergency) any emergency exit; `near` picks
    // the nearest of the open ones when he isn't at his seat's way (coming in from the hull).
    const VehicleExit* ChooseExit(const ConsoleData* console, bool emergency, const Vec3* near_m, std::string* why) const {
        const VehicleExit* best = nullptr;
        double best_d = 1e18;
        std::string shut;
        for (int pass = 0; pass < 2 && best == nullptr; ++pass) {
            for (const VehicleExit& e : vehicle_->exits) {
                if (e.emergency != (pass == 1)) continue;
                if (pass == 1 && !emergency) continue;
                bool mine = near_m != nullptr;
                for (const std::string& s : e.stations) mine = mine || s == station_;
                if (!mine && pass == 0) continue;
                if (console != nullptr && !e.shut_when.empty() && ConditionsHold(e.shut_when, console->panel, &console->equipment)) {
                    if (shut.empty()) shut = e.shut_why;
                    continue;
                }
                const double d = near_m != nullptr ? (e.out_m - *near_m).HorizontalLength() : 0.0;
                if (best == nullptr || d < best_d) {
                    best = &e;
                    best_d = d;
                }
            }
        }
        if (best == nullptr && why != nullptr) *why = shut.empty() ? "no way out for the " + station_ : shut;
        return best;
    }

    // The way down nearest to go from the hull point `top` to the ground point `ground_m` (body frame).
    const VehicleWayDown* ChooseWay(const Vec3& top, const Vec3& ground_m) const {
        const VehicleWayDown* best = nullptr;
        double best_d = 1e18;
        for (const VehicleWayDown& w : vehicle_->ways) {
            const double d = (w.top_m - top).HorizontalLength() + (w.bottom_m - ground_m).HorizontalLength();
            if (d < best_d) {
                best_d = d;
                best = &w;
            }
        }
        return best;
    }

    // Round a rectangle (grown by `margin`): its corners between a and b, the shorter way round; empty if the straight
    // line clears it.
    static std::vector<Vec3> Around(const Vec3& a, const Vec3& b, const PlanRect& rect, double margin) {
        const PlanRect r{rect.x0 - margin, rect.y0 - margin, rect.x1 + margin, rect.y1 + margin};
        const PlanRect in{rect.x0 - 0.05, rect.y0 - 0.05, rect.x1 + 0.05, rect.y1 + 0.05};
        if (!SegmentHits(a, b, in)) return {};
        const Vec3 corners[4] = {{r.x0, r.y0, a.z}, {r.x1, r.y0, a.z}, {r.x1, r.y1, a.z}, {r.x0, r.y1, a.z}};
        // The corners in order round the rectangle, from the one nearest a to the one nearest b, either way round.
        int ia = 0, ib = 0;
        double da = 1e18, db = 1e18;
        for (int i = 0; i < 4; ++i) {
            const double x = (corners[i] - a).HorizontalLength(), y = (corners[i] - b).HorizontalLength();
            if (x < da) {
                da = x;
                ia = i;
            }
            if (y < db) {
                db = y;
                ib = i;
            }
        }
        std::vector<Vec3> best;
        double best_len = 1e18;
        for (int dir : {1, -1}) {
            std::vector<Vec3> path;
            int i = ia;
            path.push_back(corners[i]);
            while (i != ib) {
                i = (i + dir + 4) % 4;
                path.push_back(corners[i]);
            }
            // Keep a corner only where the line from the last point kept to the next one would cut the rectangle.
            std::vector<Vec3> lean;
            Vec3 prev = a;
            for (std::size_t k = 0; k < path.size(); ++k) {
                const Vec3 next = k + 1 < path.size() ? path[k + 1] : b;
                if (SegmentHits(prev, next, in)) {
                    lean.push_back(path[k]);
                    prev = path[k];
                }
            }
            if (lean.empty()) lean = path;
            double len = 0.0;
            prev = a;
            for (const Vec3& p : lean) {
                len += (p - prev).HorizontalLength();
                prev = p;
            }
            len += (b - prev).HorizontalLength();
            if (len < best_len) {
                best_len = len;
                best = lean;
            }
        }
        return best;
    }

    // Round the hull on the ground (with room to spare).
    std::vector<Vec3> AroundHull(const Vec3& a, const Vec3& b) const { return Around(a, b, vehicle_->footprint, 0.4); }

    static bool SegmentHits(const Vec3& a, const Vec3& b, const PlanRect& r) {
        // Liang-Barsky against the rectangle.
        double t0 = 0.0, t1 = 1.0;
        const double dx = b.x - a.x, dy = b.y - a.y;
        const double p[4] = {-dx, dx, -dy, dy};
        const double q[4] = {a.x - r.x0, r.x1 - a.x, a.y - r.y0, r.y1 - a.y};
        for (int i = 0; i < 4; ++i) {
            if (std::fabs(p[i]) < 1e-12) {
                if (q[i] < 0.0) return false;
                continue;
            }
            const double t = q[i] / p[i];
            if (p[i] < 0.0) {
                if (t > t1) return false;
                if (t > t0) t0 = t;
            } else {
                if (t < t0) return false;
                if (t < t1) t1 = t;
            }
        }
        return t1 - t0 > 1e-9;
    }

    void AddGroundWalk(std::vector<MoveLeg>& legs, const Vec3& from_m, const Vec3& to, bool to_map, const VehiclePose& pose) const {
        const Vec3 to_m = to_map ? pose.ToBody(to) : to;
        for (const Vec3& c : AroundHull(from_m, to_m)) {
            MoveLeg l;
            l.kind = LegKind::GroundWalk;
            l.to = Vec3{c.x, c.y, 0.0};
            l.after = BodyPlace::Ground;
            legs.push_back(l);
        }
        MoveLeg l;
        l.kind = LegKind::GroundWalk;
        l.to = to;
        l.to_map = to_map;
        l.after = BodyPlace::Ground;
        legs.push_back(l);
    }

    bool Plan(const MoveGoal& goal, const ConsoleData* console, const VehiclePose& pose, BodyPlace place, Vec3 at,
              std::vector<MoveLeg>& legs, std::string* why) const {
        const VehicleSeat* seat = vehicle_->Seat(station_);
        if (seat == nullptr) return Refuse(why, "no seat for the " + station_);
        // The goal's level: 0 his seat, 1 the compartment, 2 the hull, 3 the ground.
        int want = 0;
        Vec3 target;
        bool target_map = false;
        switch (goal.kind) {
            case MoveGoal::Kind::Seat: want = 0; break;
            case MoveGoal::Kind::Cabin: want = 1; target = goal.point; break;
            case MoveGoal::Kind::Hull:
                want = 2;
                target = goal.point;
                if (!vehicle_->OnHull(target.x, target.y)) return Refuse(why, "no footing there on the hull");
                target.z = vehicle_->hull_top_m;
                break;
            case MoveGoal::Kind::Place: {
                const VehiclePlace* p = vehicle_->Place(goal.place);
                if (p == nullptr) return Refuse(why, "no place '" + goal.place + "' round the vehicle");
                want = 3;
                target = p->at_m;
                break;
            }
            case MoveGoal::Kind::Lineup:
                if (!vehicle_->LineupPoint(station_, target)) return Refuse(why, "no place in the line-up for the " + station_);
                want = 3;
                break;
            case MoveGoal::Kind::Ground:
                want = 3;
                target = goal.point;
                target_map = true;
                break;
        }
        int level = place == BodyPlace::Seated ? 0 : place == BodyPlace::Cabin ? 1 : place == BodyPlace::Ground ? 3 : 2;
        // At his seat already, going to it: nothing to do.
        if (level == 0 && want == 0) return true;
        Vec3 here = at;  // body frame unless on the ground (the map)
        if (level == 0) {  // up from the seat
            MoveLeg l;
            l.kind = LegKind::StandUp;
            l.to = seat->stand_m;
            l.duration_s = stand_up_s_;
            l.after = BodyPlace::Cabin;
            legs.push_back(l);
            here = seat->stand_m;
            level = 1;
        }
        if (level == 1 && want >= 2) {  // out through a hatch
            const VehicleExit* e = ChooseExit(console, goal.emergency, nullptr, why);
            if (e == nullptr) return false;
            const bool from_seat = place == BodyPlace::Seated || (here - seat->stand_m).HorizontalLength() < 0.05;
            if (from_seat) {
                for (const Vec3& v : seat->via_m) AddWalk(legs, LegKind::CabinWalk, v, BodyPlace::Cabin);
            }
            AddWalk(legs, LegKind::CabinWalk, e->under_m, BodyPlace::Cabin);
            AddTimed(legs, LegKind::OpenHatch, e->under_m, e->open_s, BodyPlace::Cabin, e->id);
            AddTimed(legs, LegKind::ClimbOut, e->out_m, e->climb_out_s, e->to_ground ? BodyPlace::Ground : BodyPlace::Hull, e->id);
            here = e->out_m;
            level = e->to_ground ? 3 : 2;
            if (level == 3) here = pose.ToEnu(here);
        }
        if (level == 2 && want == 2) {
            AddHullWalk(legs, here, target);
            return true;
        }
        if (level == 2 && want == 3) {  // down to the ground
            const Vec3 target_m = target_map ? pose.ToBody(target) : target;
            const VehicleWayDown* w = ChooseWay(here, target_m);
            if (w == nullptr) return Refuse(why, "no way down");
            AddHullWalk(legs, here, w->top_m);
            MoveLeg d;
            d.kind = LegKind::ClimbDown;
            d.to = w->bottom_m;
            d.duration_s = w->down_s;
            d.way = w->id;
            d.after = BodyPlace::Ground;
            legs.push_back(d);
            here = pose.ToEnu(w->bottom_m);
            level = 3;
        }
        if (level == 3 && want == 3) {
            AddGroundWalk(legs, pose.ToBody(here), target, target_map, pose);
            return true;
        }
        if (level == 3 && want <= 2) {  // up onto the hull
            const Vec3 here_m = pose.ToBody(here);
            // The way nearest to him and to where he climbs in.
            const VehicleExit* e = want <= 1 ? ChooseExit(console, goal.emergency, want == 0 ? nullptr : &here_m, why) : nullptr;
            if (want <= 1 && e == nullptr) return false;
            if (e != nullptr && e->to_ground) {  // in by an emergency window from the ground
                AddGroundWalk(legs, here_m, e->out_m, false, pose);
                AddTimed(legs, LegKind::ClimbIn, e->under_m, e->climb_in_s, BodyPlace::Cabin, e->id);
                here = e->under_m;
                level = 1;
            } else {
                const Vec3 goal_top = e != nullptr ? e->out_m : target;
                const VehicleWayDown* w = ChooseWay(goal_top, here_m);
                if (w == nullptr) return Refuse(why, "no way up");
                AddGroundWalk(legs, here_m, w->bottom_m, false, pose);
                AddTimed(legs, LegKind::ClimbUp, w->top_m, w->up_s, BodyPlace::Hull, std::string(), w->id);
                here = w->top_m;
                level = 2;
                if (want == 2) {
                    AddHullWalk(legs, here, target);
                    return true;
                }
            }
        }
        if (level == 2 && want <= 1) {  // in through a hatch
            const VehicleExit* e = ChooseExit(console, goal.emergency, want == 0 ? nullptr : &here, why);
            if (e == nullptr || e->to_ground) {
                e = ChooseExit(console, goal.emergency, &here, why);
                if (e == nullptr) return false;
            }
            AddHullWalk(legs, here, e->out_m);
            AddTimed(legs, LegKind::OpenHatch, e->out_m, e->open_s, BodyPlace::Hull, e->id);
            AddTimed(legs, LegKind::ClimbIn, e->under_m, e->climb_in_s, BodyPlace::Cabin, e->id);
            if (goal.shut_after) AddTimed(legs, LegKind::ShutHatch, e->under_m, e->open_s, BodyPlace::Cabin, e->id);
            here = e->under_m;
            level = 1;
        }
        if (level == 1 && want == 1) {
            AddWalk(legs, LegKind::CabinWalk, target, BodyPlace::Cabin);
            return true;
        }
        if (level == 1 && want == 0) {  // back to the seat: the way he came, backwards
            for (std::size_t i = seat->via_m.size(); i > 0; --i) AddWalk(legs, LegKind::CabinWalk, seat->via_m[i - 1], BodyPlace::Cabin);
            AddWalk(legs, LegKind::CabinWalk, seat->stand_m, BodyPlace::Cabin);
            AddTimed(legs, LegKind::SitDown, seat->stand_m, sit_down_s_, BodyPlace::Seated);
            return true;
        }
        return Refuse(why, "no way there");
    }

    // Across the hull's top from `from` to `to`, round a keep-out in the way (the mast's foot, the roof box).
    void AddHullWalk(std::vector<MoveLeg>& legs, const Vec3& from, const Vec3& to) const {
        Vec3 at = from;
        for (const VehicleKeepOut& k : vehicle_->keep_out) {
            for (Vec3 c : Around(at, to, k.area, 0.15)) {
                c.z = vehicle_->hull_top_m;
                AddWalk(legs, LegKind::HullWalk, c, BodyPlace::Hull);
                at = c;
            }
        }
        AddWalk(legs, LegKind::HullWalk, to, BodyPlace::Hull);
    }

    static void AddWalk(std::vector<MoveLeg>& legs, LegKind kind, const Vec3& to, BodyPlace after) {
        MoveLeg l;
        l.kind = kind;
        l.to = to;
        l.after = after;
        legs.push_back(l);
    }
    static void AddTimed(std::vector<MoveLeg>& legs, LegKind kind, const Vec3& to, double s, BodyPlace after,
                         const std::string& exit = std::string(), const std::string& way = std::string()) {
        MoveLeg l;
        l.kind = kind;
        l.to = to;
        l.duration_s = s;
        l.exit = exit;
        l.way = way;
        l.after = after;
        legs.push_back(l);
    }

    void StartLeg(MoveLeg& l, const VehiclePose& pose, std::map<std::string, bool>& hatches, const std::map<std::string, int>* still_out) {
        leg_dist_ = 1e-12;  // started
        // Opening an open hatch, or shutting a shut one, takes no time; nor does leaving it open for a man still out.
        if (l.kind == LegKind::OpenHatch && hatches[l.exit]) l.duration_s = 0.0;
        if (l.kind == LegKind::ShutHatch && !hatches[l.exit]) l.duration_s = 0.0;
        if (l.kind == LegKind::ShutHatch && still_out != nullptr) {
            const auto it = still_out->find(l.exit);
            if (it != still_out->end() && it->second > 0) {
                l.duration_s = 0.0;
                l.exit.clear();  // not his to shut yet
            }
        }
        // Climbing faces the hatch or the hull's side.
        if (l.kind == LegKind::ClimbDown || l.kind == LegKind::ClimbUp || l.kind == LegKind::ClimbOut || l.kind == LegKind::ClimbIn) {
            const Vec3 from_m = place_ == BodyPlace::Ground ? pose.ToBody(pos_) : pos_;
            const Vec3 d = l.to - from_m;
            if (d.HorizontalLength() > 0.05) {
                const double yaw = std::atan2(d.x, d.y);
                facing_ = place_ == BodyPlace::Ground ? pose.ToMapHeading(yaw) : yaw;
            }
        }
    }

    void EndLeg(const MoveLeg& l, const VehiclePose& pose, const Terrain* ground, std::map<std::string, bool>& hatches) {
        const BodyPlace before = place_;
        if (l.kind == LegKind::OpenHatch) hatches[l.exit] = true;
        if (l.kind == LegKind::ShutHatch && !l.exit.empty()) hatches[l.exit] = false;
        place_ = l.after;
        if (place_ == BodyPlace::Ground) {
            // A walk on the ground has carried him there already; a climb lands him at the leg's end.
            if (l.kind != LegKind::GroundWalk) pos_ = l.to_map ? l.to : pose.ToEnu(l.to);
            if (ground != nullptr) pos_.z = ground->HeightAt(pos_.x, pos_.y);
            if (before != BodyPlace::Ground) facing_ = pose.ToMapHeading(facing_);
        } else {
            pos_ = l.to;
            if (before == BodyPlace::Ground) facing_ = pose.ToBodyYaw(facing_);
        }
        legs_.erase(legs_.begin());
        leg_s_ = 0.0;
        leg_dist_ = 0.0;
        if (legs_.empty()) Arrive(pose);
    }

    void Arrive(const VehiclePose& pose) {
        stuck_ = false;
        // At a place he faces his work; in the line-up, its front.
        if (vehicle_ == nullptr) return;
        Vec3 face;
        bool has = false;
        if (goal_.kind == MoveGoal::Kind::Place) {
            if (const VehiclePlace* p = vehicle_->Place(goal_.place)) {
                face = p->face_m;
                has = true;
            }
        } else if (goal_.kind == MoveGoal::Kind::Lineup) {
            face = vehicle_->lineup.face_m;
            has = true;
        }
        if (has && place_ == BodyPlace::Ground) {
            const Vec3 me = pose.ToBody(pos_);
            const Vec3 d = face - me;
            if (d.HorizontalLength() > 0.05) facing_ = pose.ToMapHeading(std::atan2(d.x, d.y));
        }
    }

    const VehicleDef* vehicle_ = nullptr;
    std::string station_;
    BodyPlace place_ = BodyPlace::Seated;
    Vec3 pos_;                 // body frame, or ENU on the ground
    double facing_ = 0.0;      // body yaw, or map heading on the ground
    std::vector<MoveLeg> legs_;
    double leg_s_ = 0.0;
    double leg_dist_ = 0.0;
    MoveGoal goal_;
    bool stuck_ = false;
    std::string at_;
    double pace_ = 1.0;
    double stand_up_s_ = 1.5;
    double sit_down_s_ = 1.5;
};


// ---------------------------------------------------------------------------------------------- the body drawn on its way

// A crewman away from his seat drawn on his feet (crew_body.hpp's body): standing where the mover has him, turned his
// way; walking, his legs step and his arms swing (`stride_phase` in turns, from the distance walked); climbing, his hands
// go up to the rim or the handhold and his knees bend; at a job, his hands work in front of him. In the vehicle's body
// frame (the bodies are drawn in it), so a man on the ground far out is drawn there too.
inline BodyPose WalkingPose(const MoverView& v, const BodyDims& d, double stride_phase) {
    const Vec3 z{0.0, 0.0, 1.0};
    SeatFrame f;
    f.station = v.station;
    f.forward = Vec3{std::sin(v.yaw_m), std::cos(v.yaw_m), 0.0};
    f.right = Vec3{std::cos(v.yaw_m), -std::sin(v.yaw_m), 0.0};
    f.floor_z = v.feet_m.z;
    const double hip_h = d.hip_frac * d.stature_m;
    const bool climbing = v.leg_active && (v.leg == LegKind::ClimbOut || v.leg == LegKind::ClimbIn || v.leg == LegKind::ClimbUp ||
                                           v.leg == LegKind::ClimbDown);
    const bool walking = v.leg_active && (v.leg == LegKind::CabinWalk || v.leg == LegKind::HullWalk || v.leg == LegKind::GroundWalk);
    const bool stooping = v.inside && v.place != BodyPlace::Seated;  // under a compartment's roof
    const double crouch = climbing ? 0.25 * hip_h : (stooping ? 0.12 * hip_h : 0.0);
    f.hip = v.feet_m + z * hip_h;
    f.eye = f.hip + z * (d.SittingEye() - d.hip_above_seat_m);
    BodyTargets tg;
    tg.standing = 1.0;
    tg.stand_hip = f.hip;
    tg.stand_forward = f.forward;
    tg.lean.crouch_m = crouch;
    tg.lean.forward_rad = climbing ? DegToRad(25.0) : (stooping ? DegToRad(20.0) : 0.0);
    BodyPose p = SolveBody(f, d, tg);
    const double swing = walking ? std::sin(kTwoPi * stride_phase) : 0.0;
    // The hands: up to a hold climbing, swinging walking, in front of him at work.
    for (int i = 0; i < 2; ++i) {
        const double side = i == 0 ? -1.0 : 1.0;
        WristTarget w;
        if (climbing) {
            w.wrist = p.shoulder[i] + z * 0.45 + f.forward * 0.25 + f.right * (side * 0.05);
            w.dir = (z + f.forward * 0.3).Normalized();
            w.up = f.forward * -1.0;
            w.shape = HandShape::Grip;
        } else if (!v.moving && v.place == BodyPlace::Ground && !v.at.empty() && v.at != "lineup") {
            w.wrist = p.shoulder[i] - z * 0.35 + f.forward * 0.35 + f.right * (side * 0.06);
            w.dir = f.forward;
            w.up = z;
            w.shape = HandShape::Grip;
        } else {
            w.wrist = p.hip_joint[i] - z * 0.22 + f.right * (side * 0.09) + f.forward * (0.18 * swing * -side);
            w.dir = (f.forward * 0.2 - z).Normalized();
            w.up = f.right * side;
            w.shape = HandShape::Rest;
        }
        const Vec3 pole = (-p.torso.up + p.torso.right * (0.7 * side) - p.torso.forward * 0.4).Normalized();
        const IkResult ik = TwoBoneIk(p.shoulder[i], w.wrist, d.UpperArm(), d.Forearm(), pole);
        p.elbow[i] = ik.mid;
        p.wrist[i] = ik.end;
        p.hand_dir[i] = w.dir;
        p.hand_up[i] = w.up;
        p.hand_shape[i] = w.shape;
    }
    // The legs: a step each, the foot lifted as it swings through.
    if (walking || climbing) {
        for (int i = 0; i < 2; ++i) {
            const double s = i == 0 ? swing : -swing;
            const double lift = walking ? 0.08 * std::fmax(0.0, std::cos(kTwoPi * stride_phase + (i == 0 ? 0.0 : kPi))) : 0.0;
            Vec3 ankle = Vec3{p.hip_joint[i].x, p.hip_joint[i].y, f.floor_z + d.AnkleHeight()} + f.forward * (0.32 * s) + z * lift;
            if (climbing) ankle = ankle + z * (i == 0 ? 0.35 : 0.0) + f.forward * 0.15;
            const IkResult ik = TwoBoneIk(p.hip_joint[i], ankle, d.Thigh(), d.Shank(), (f.forward + z * 0.3).Normalized());
            p.knee[i] = ik.mid;
            p.ankle[i] = ik.end;
        }
    }
    return p;
}

}  // namespace simcore
