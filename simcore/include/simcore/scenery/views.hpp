// simcore/scenery/views.hpp — the views the terrain's look is checked from (roadmap T2, docs/scenery/T2_VIEWS_PLAN.md):
// each anchored to the map's own ids and kinds (a settlement, a road, a site, a river, a hill, a parcel of farmland) and
// resolved on the map as it is laid out, never to fixed coordinates, so a changed map needs no edits to the data.
// Data from data/scenery/views.json (LoadViews); ResolveView gives each camera. A view the map cannot give is
// reported missing with its reason, never skipped.
//
// Presentation only: nothing in the simulation reads it. Headings are true bearings (radians clockwise from north).
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/json.hpp"
#include "simcore/scenery/map_dressing.hpp"
#include "simcore/terrain_map.hpp"

namespace simcore {
namespace scenery {

struct ViewAnchor {
    std::string kind;  // parcel_edge, settlement, road, farmstead, farmyard, site, river, hill, high_ground
    std::string id;    // the settlement's, road's, site's, river's or hill's
    bool has_close = false;
    double close_e_m = 0.0, close_n_m = 0.0;  // where to look for the nearest one (the data's close_km)
    double from_m = 0.0;                      // the eye's distance from the anchor along `bearing_rad`
    double from_centre_m = 0.0;               // a settlement: along its street, from the centre (negative: the other way)
    double bearing_rad = 0.0;                 // the eye lies on this bearing from the anchor
    bool has_bearing = false;
    double at = 0.5;                          // a road: the fraction of its length
    double side_m = 0.0;                      // a road: beyond the carriageway's edge; a river: off its centre line
    std::string towards;                      // a road: the settlement its view looks towards, if any
};

struct ViewDef {
    std::string id;
    ViewAnchor anchor;
    double eye_m = 1.7;  // above the ground
    std::string look = "at_anchor";  // across_parcel, along_street, along_road, across_river, at_anchor, panorama
    double pitch_rad = 0.0;
    double fov_rad = 1.0472;
    double step_rad = 1.5708;  // a panorama's turn between its views
    std::vector<std::string> photos;
    std::string must_see;
};

struct PhotoRef {
    std::string id, title, page_url, licence, author;
};

struct ViewSet {
    std::vector<ViewDef> views;
    std::vector<PhotoRef> photos;

    const PhotoRef* Photo(const std::string& id) const {
        for (const PhotoRef& p : photos) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }
};

// One camera: the eye and the point looked at, east and north in the map frame, their heights above the ground there.
struct ResolvedView {
    std::string id;  // the view's, with _h<deg> after it for one of a panorama's
    bool ok = false;
    std::string missing_reason;
    double eye_e = 0.0, eye_n = 0.0, eye_up_m = 0.0;
    double target_e = 0.0, target_n = 0.0, target_up_m = 0.0;
    double fov_rad = 1.0472;
    double bearing_rad = 0.0;
};

namespace views_detail {

inline bool Fail(std::string* error, const std::string& what) {
    if (error != nullptr) *error = what;
    return false;
}

inline double Number(const json::Value& v, const char* key, double fallback) {
    const json::Value* f = v.Find(key);
    return f != nullptr && f->Is(json::Type::Number) ? f->number : fallback;
}

inline std::string Text(const json::Value& v, const char* key) {
    const json::Value* f = v.Find(key);
    return f != nullptr && f->Is(json::Type::String) ? f->string : std::string();
}

inline const std::vector<std::string>& LookKinds() {
    static const std::vector<std::string> k = {"across_parcel", "along_street", "along_road", "across_river", "at_anchor", "panorama"};
    return k;
}

inline const std::vector<std::string>& AnchorKinds() {
    static const std::vector<std::string> k = {"parcel_edge", "settlement", "road", "farmstead", "farmyard", "wood_edge", "site", "river", "hill", "high_ground", "brook_row", "wood_inside", "pond", "sunken_lane",
                                               "scrub_thicket", "pasture_bush"};
    return k;
}

inline bool OneOf(const std::vector<std::string>& list, const std::string& s) {
    for (const std::string& x : list) {
        if (x == s) return true;
    }
    return false;
}

}  // namespace views_detail

// Reads one anchor (a view's, or a plot's: scenery/plots.hpp) into `an`. False with the reason in `error`.
inline bool ParseAnchor(const json::Value& a_value, const std::string& id, ViewAnchor& an, std::string* error) {
    using views_detail::Fail;
    const json::Value* a = &a_value;
    an = ViewAnchor();
    an.kind = views_detail::Text(*a, "kind");
    if (!views_detail::OneOf(views_detail::AnchorKinds(), an.kind)) return Fail(error, id + ": anchor kind \"" + an.kind + "\" unknown");
    for (const char* key : {"settlement", "road", "site", "river", "hill"}) {
        const std::string name = views_detail::Text(*a, key);
        if (!name.empty()) an.id = name;
    }
    if (const json::Value* c = a->Find("close_km")) {
        if (!c->Is(json::Type::Array) || c->items.size() != 2 || !c->items[0].Is(json::Type::Number) || !c->items[1].Is(json::Type::Number)) {
            return Fail(error, id + ": close_km is [east, north] in km");
        }
        an.has_close = true;
        an.close_e_m = c->items[0].number * 1000.0;
        an.close_n_m = c->items[1].number * 1000.0;
    }
    an.from_m = views_detail::Number(*a, "from_m", 0.0);
    an.from_centre_m = views_detail::Number(*a, "from_centre_m", 0.0);
    an.at = views_detail::Number(*a, "at", 0.5);
    an.side_m = views_detail::Number(*a, "side_m", 0.0);
    an.towards = views_detail::Text(*a, "towards");
    if (a->Find("bearing_deg") != nullptr) {
        an.has_bearing = true;
        an.bearing_rad = views_detail::Number(*a, "bearing_deg", 0.0) * kPi / 180.0;
    }
    if (an.at < 0.0 || an.at > 1.0) return Fail(error, id + ": at is a fraction 0..1");
    const bool by_id = an.kind == "settlement" || an.kind == "road" || an.kind == "site" || an.kind == "river" || an.kind == "hill";
    if (by_id && an.id.empty()) return Fail(error, id + ": a " + an.kind + " anchor names its " + an.kind);
    if ((an.kind == "parcel_edge" || an.kind == "farmstead" || an.kind == "farmyard" || an.kind == "wood_edge" || an.kind == "high_ground" ||
         an.kind == "brook_row" || an.kind == "wood_inside" || an.kind == "pond" || an.kind == "sunken_lane" || an.kind == "scrub_thicket" ||
         an.kind == "pasture_bush") &&
        !an.has_close) {
        return Fail(error, id + ": a " + an.kind + " anchor needs close_km");
    }
    return true;
}

// Reads the views from a file's path or the JSON text itself (it starts with '{'). False with the reason in `error` (the
// set is left as it was): ids unique, kinds known, every photo named by a view listed with its page, licence and author.
inline bool LoadViews(const std::string& path_or_json, ViewSet& out, std::string* error = nullptr) {
    using views_detail::Fail;
    std::string text = path_or_json;
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || text[first] != '{') {
        std::ifstream f(path_or_json, std::ios::binary);
        if (!f) return Fail(error, "cannot open " + path_or_json);
        std::ostringstream s;
        s << f.rdbuf();
        text = s.str();
    }
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text, root, &parse_error)) return Fail(error, parse_error);
    ViewSet set;
    const json::Value* photos = root.Find("photos");
    if (photos != nullptr && photos->Is(json::Type::Object)) {
        for (std::size_t i = 0; i < photos->keys.size(); ++i) {
            PhotoRef p;
            p.id = photos->keys[i];
            p.title = views_detail::Text(photos->items[i], "title");
            p.page_url = views_detail::Text(photos->items[i], "page_url");
            p.licence = views_detail::Text(photos->items[i], "licence");
            p.author = views_detail::Text(photos->items[i], "author");
            if (p.page_url.compare(0, 8, "https://") != 0) return Fail(error, "photo " + p.id + ": no page_url");
            if (p.licence.empty() || p.author.empty()) return Fail(error, "photo " + p.id + ": no licence or author");
            set.photos.push_back(p);
        }
    }
    const json::Value* views = root.Find("views");
    if (views == nullptr || !views->Is(json::Type::Array) || views->items.empty()) return Fail(error, "no \"views\" list");
    for (const json::Value& item : views->items) {
        ViewDef v;
        v.id = views_detail::Text(item, "id");
        if (v.id.empty()) return Fail(error, "a view without an id");
        for (const ViewDef& other : set.views) {
            if (other.id == v.id) return Fail(error, "the view \"" + v.id + "\" twice");
        }
        const json::Value* a = item.Find("anchor");
        if (a == nullptr || !a->Is(json::Type::Object)) return Fail(error, v.id + ": no anchor");
        if (!ParseAnchor(*a, v.id, v.anchor, error)) return false;
        v.eye_m = views_detail::Number(item, "eye_m", 1.7);
        if (v.eye_m < 0.5 || v.eye_m > 20000.0) return Fail(error, v.id + ": eye_m is not in 0.5..20000");
        v.look = views_detail::Text(item, "look");
        if (v.look.empty()) v.look = "at_anchor";
        if (!views_detail::OneOf(views_detail::LookKinds(), v.look)) return Fail(error, v.id + ": look \"" + v.look + "\" unknown");
        const double fov = views_detail::Number(item, "fov_deg", 60.0);
        if (fov < 20.0 || fov > 120.0) return Fail(error, v.id + ": fov_deg is not in 20..120");
        v.fov_rad = fov * kPi / 180.0;
        v.pitch_rad = views_detail::Number(item, "pitch_deg", 0.0) * kPi / 180.0;
        if (std::fabs(v.pitch_rad) > 1.5) return Fail(error, v.id + ": pitch_deg is beyond 85");
        const double step = views_detail::Number(item, "step_deg", 90.0);
        if (step < 15.0 || step > 180.0) return Fail(error, v.id + ": step_deg is not in 15..180");
        v.step_rad = step * kPi / 180.0;
        v.must_see = views_detail::Text(item, "must_see");
        if (const json::Value* ps = item.Find("photos")) {
            if (!ps->Is(json::Type::Array)) return Fail(error, v.id + ": photos is not a list");
            for (const json::Value& p : ps->items) {
                if (!p.Is(json::Type::String) || set.Photo(p.string) == nullptr) {
                    return Fail(error, v.id + ": a photo that is not listed in \"photos\"");
                }
                v.photos.push_back(p.string);
            }
        }
        set.views.push_back(v);
    }
    out = set;
    return true;
}

namespace views_detail {

struct Spot {
    bool ok = false;
    std::string why;
    double e = 0.0, n = 0.0;       // the anchor point (a settlement's centre, a road's point, ...)
    double eye_e = 0.0, eye_n = 0.0;
    double bearing_rad = 0.0;      // the way the view looks
};

inline Spot Missing(const std::string& why) {
    Spot s;
    s.why = why;
    return s;
}

inline double BearingTo(double from_e, double from_n, double to_e, double to_n) {
    return std::atan2(to_e - from_e, to_n - from_n);
}

inline const MapSettlementDef* FindSettlement(const MapDef& d, const std::string& id) {
    for (const MapSettlementDef& s : d.settlements) {
        if (s.id == id) return &s;
    }
    for (const MapSettlementDef& s : d.detail_settlements) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

template <typename T>
inline const T* FindById(const std::vector<T>& list, const std::string& id) {
    for (const T& x : list) {
        if (x.id == id) return &x;
    }
    return nullptr;
}

// The point of a polyline nearest (e, n): its position, the segment's heading, the distance, and the side (+1 left of the
// way the line runs, -1 right).
struct OnLine {
    double e = 0.0, n = 0.0, heading = 0.0, dist = std::numeric_limits<double>::max();
    int side = 1;
};

inline OnLine NearestOnLine(const std::vector<MapPoint>& pts, double pe, double pn) {
    OnLine best;
    for (std::size_t i = 1; i < pts.size(); ++i) {
        const double ae = pts[i - 1].east_m, an = pts[i - 1].north_m, be = pts[i].east_m, bn = pts[i].north_m;
        const double de = be - ae, dn = bn - an;
        const double len2 = de * de + dn * dn;
        if (len2 <= 0.0) continue;
        const double t = std::fmax(0.0, std::fmin(1.0, ((pe - ae) * de + (pn - an) * dn) / len2));
        const double qe = ae + t * de, qn = an + t * dn;
        const double d = std::hypot(pe - qe, pn - qn);
        if (d < best.dist) {
            best.e = qe;
            best.n = qn;
            best.dist = d;
            best.heading = std::atan2(de, dn);
            best.side = (de * (pn - qn) - dn * (pe - qe)) >= 0.0 ? 1 : -1;
        }
    }
    return best;
}

// The point a fraction of a polyline's length along it, and the segment's heading there.
inline OnLine AlongLine(const std::vector<MapPoint>& pts, double fraction) {
    OnLine at;
    double total = 0.0;
    for (std::size_t i = 1; i < pts.size(); ++i) total += std::hypot(pts[i].east_m - pts[i - 1].east_m, pts[i].north_m - pts[i - 1].north_m);
    double want = total * fraction, run = 0.0;
    for (std::size_t i = 1; i < pts.size(); ++i) {
        const double de = pts[i].east_m - pts[i - 1].east_m, dn = pts[i].north_m - pts[i - 1].north_m;
        const double len = std::hypot(de, dn);
        if (len <= 0.0) continue;
        if (run + len >= want || i + 1 == pts.size()) {
            const double t = std::fmax(0.0, std::fmin(1.0, (want - run) / len));
            at.e = pts[i - 1].east_m + t * de;
            at.n = pts[i - 1].north_m + t * dn;
            at.heading = std::atan2(de, dn);
            at.dist = 0.0;
            return at;
        }
        run += len;
    }
    return at;
}

}  // namespace views_detail

// Where an anchor lies on the map: its point, the eye the anchor's own kind places, and the way it looks (also read by
// the plots, scenery/plots.hpp). Not ok, with the reason, when the map cannot give it.
inline views_detail::Spot ResolveSpot(const MapData& map, const CoverRaster& cover, const ViewAnchor& a) {
    using namespace views_detail;
    const MapDef& def = map.Def();
    Spot s;
    if (a.kind == "settlement") {
        const MapSettlementDef* st = FindSettlement(def, a.id);
        if (st == nullptr) {
            s = Missing("the map has no settlement \"" + a.id + "\"");
        } else {
            s.ok = true;
            s.e = st->centre.east_m;
            s.n = st->centre.north_m;
            if (a.has_bearing) {
                s.eye_e = s.e + a.from_m * std::sin(a.bearing_rad);
                s.eye_n = s.n + a.from_m * std::cos(a.bearing_rad);
            } else {  // along its street (the settlement's heading)
                s.eye_e = s.e + a.from_centre_m * std::sin(st->heading_rad);
                s.eye_n = s.n + a.from_centre_m * std::cos(st->heading_rad);
            }
            s.bearing_rad = BearingTo(s.eye_e, s.eye_n, s.e, s.n);
            if (a.from_m == 0.0 && a.from_centre_m == 0.0) s.bearing_rad = st->heading_rad;
        }
    } else if (a.kind == "road") {
        const MapLineDef* r = FindById(def.roads, a.id);
        if (r == nullptr || r->points.size() < 2) {
            s = Missing("the map has no road \"" + a.id + "\"");
        } else {
            const OnLine p = AlongLine(r->points, a.at);
            double heading = p.heading;
            if (!a.towards.empty()) {
                const MapSettlementDef* st = FindSettlement(def, a.towards);
                if (st != nullptr) {
                    const double to = BearingTo(p.e, p.n, st->centre.east_m, st->centre.north_m);
                    if (std::cos(to - heading) < 0.0) heading += kPi;
                }
            }
            const double off = 0.5 * r->width_m + a.side_m;
            // Beyond the carriageway's right edge, looking along the road.
            s.ok = true;
            s.e = p.e;
            s.n = p.n;
            s.eye_e = p.e + off * std::cos(heading);
            s.eye_n = p.n - off * std::sin(heading);
            s.bearing_rad = heading;
        }
    } else if (a.kind == "parcel_edge") {
        // The nearest field cell to close_km, then back across its parcel along the short axis to the edge.
        const CoverGrid* g = cover.GridAt(a.close_e_m, a.close_n_m);
        if (g == nullptr) {
            s = Missing("close_km is outside the land cover");
        } else {
            double fe = 0.0, fn = 0.0, best = std::numeric_limits<double>::max();
            for (double de = -3000.0; de <= 3000.0; de += 25.0) {
                for (double dn = -3000.0; dn <= 3000.0; dn += 25.0) {
                    const double d = de * de + dn * dn;
                    if (d >= best) continue;
                    const double pe = a.close_e_m + de, pn = a.close_n_m + dn;
                    if (cover.At(pe, pn) != Cover::Field) continue;
                    // Well inside the field: its neighbours a few cells round are fields too.
                    bool inside = true;
                    for (int k = 0; k < 8 && inside; ++k) {
                        const double ang = k * kPi / 4.0;
                        inside = cover.At(pe + 75.0 * std::sin(ang), pn + 75.0 * std::cos(ang)) == Cover::Field;
                    }
                    if (!inside) continue;
                    best = d;
                    fe = pe;
                    fn = pn;
                }
            }
            if (best == std::numeric_limits<double>::max()) {
                s = Missing("no farmland within 3 km of close_km");
            } else {
                const double heading = FieldHeading(g->fields, fe, fn);
                const double across = heading + kPi / 2.0;  // the parcel's short axis
                const double ue = std::sin(across), un = std::cos(across);
                const std::uint64_t here = FieldParcel(g->fields, fe, fn);
                double back = 0.0;
                while (back < 2000.0 && FieldParcel(g->fields, fe - back * ue, fn - back * un) == here) back += 5.0;
                // The edge to a few centimetres: it lies between back - 5 (in the parcel) and back (out of it).
                double in = std::max(0.0, back - 5.0), out = back;
                for (int k = 0; k < 8; ++k) {
                    const double mid = 0.5 * (in + out);
                    (FieldParcel(g->fields, fe - mid * ue, fn - mid * un) == here ? in : out) = mid;
                }
                back = out;
                // 2 m beyond the edge (into the neighbouring parcel), looking back across this one.
                s.ok = true;
                s.e = fe;
                s.n = fn;
                s.eye_e = fe - (back + 2.0) * ue;
                s.eye_n = fn - (back + 2.0) * un;
                s.bearing_rad = across;
            }
        }
    } else if (a.kind == "farmstead") {
        const MapBox* bestbox = nullptr;
        double best = std::numeric_limits<double>::max();
        for (const MapBox& b : map.Layout().boxes) {
            if (b.part != MapPart::Farm) continue;
            const double d = std::hypot(b.east_m - a.close_e_m, b.north_m - a.close_n_m);
            if (d < best) {
                best = d;
                bestbox = &b;
            }
        }
        if (bestbox == nullptr) {
            s = Missing("the map has no farmsteads");
        } else {
            const double bearing = a.has_bearing ? a.bearing_rad : kPi;
            s.ok = true;
            s.e = bestbox->east_m;
            s.n = bestbox->north_m;
            s.eye_e = s.e + a.from_m * std::sin(bearing);
            s.eye_n = s.n + a.from_m * std::cos(bearing);
            s.bearing_rad = BearingTo(s.eye_e, s.eye_n, s.e, s.n);
        }
    } else if (a.kind == "farmyard") {
        const MapSiteDef* st = nullptr;
        double best = std::numeric_limits<double>::max();
        for (const MapSiteDef& site : def.sites) {
            if (site.kind != "farmyard") continue;
            const double d = std::hypot(site.centre.east_m - a.close_e_m, site.centre.north_m - a.close_n_m);
            if (d < best) {
                best = d;
                st = &site;
            }
        }
        if (st == nullptr) {
            s = Missing("the map has no farmyard site kind yet (roadmap T7)");
        } else {
            const double bearing = a.has_bearing ? a.bearing_rad : kPi;
            s.ok = true;
            s.e = st->centre.east_m;
            s.n = st->centre.north_m;
            s.eye_e = s.e + a.from_m * std::sin(bearing);
            s.eye_n = s.n + a.from_m * std::cos(bearing);
            s.bearing_rad = BearingTo(s.eye_e, s.eye_n, s.e, s.n);
        }
    } else if (a.kind == "site") {
        const MapSiteDef* st = FindById(def.sites, a.id);
        if (st == nullptr) {
            s = Missing("the map has no site \"" + a.id + "\"");
        } else {
            s.ok = true;
            s.e = st->centre.east_m;
            s.n = st->centre.north_m;
            // On the site, along its heading; or `from_m` off it on `bearing_deg`, facing it (clear of what stands there).
            s.eye_e = s.e;
            s.eye_n = s.n;
            s.bearing_rad = st->heading_rad;
            if (a.has_bearing && a.from_m > 0.0) {
                s.eye_e = s.e + a.from_m * std::sin(a.bearing_rad);
                s.eye_n = s.n + a.from_m * std::cos(a.bearing_rad);
                s.bearing_rad = BearingTo(s.eye_e, s.eye_n, s.e, s.n);
            }
        }
    } else if (a.kind == "river") {
        const MapLineDef* r = FindById(def.rivers, a.id);
        if (r == nullptr || r->points.size() < 2) {
            s = Missing("the map has no river \"" + a.id + "\"");
        } else {
            const OnLine p = NearestOnLine(r->points, a.close_e_m, a.close_n_m);
            // Off the centre line on its left or right bank, looking across to the water's middle.
            const double off = a.side_m;
            const double normal = p.heading + (p.side > 0 ? -kPi / 2.0 : kPi / 2.0);  // pointing to the eye's bank
            s.ok = true;
            s.e = p.e;
            s.n = p.n;
            s.eye_e = p.e + off * std::sin(normal);
            s.eye_n = p.n + off * std::cos(normal);
            s.bearing_rad = BearingTo(s.eye_e, s.eye_n, p.e, p.n);
        }
    } else if (a.kind == "wood_inside") {
        // The nearest wood block of the map proper to close_km with wood on its four sides (no edge rows: its inside
        // trees and understorey throughout, A.3.19, A.3.21), the cameras looking at its middle along the bearing.
        const MapBox* wood = nullptr;
        double best = std::numeric_limits<double>::max();
        const auto canopy_at = [&](double e, double n, double size) {
            for (const MapBox& o : map.Layout().boxes) {
                if (o.part == MapPart::Canopy && o.length_m == size && std::fabs(o.east_m - e) < 1.0 && std::fabs(o.north_m - n) < 1.0) return true;
            }
            return false;
        };
        for (const MapBox& b : map.Layout().boxes) {
            if (b.part != MapPart::Canopy || !def.InCore(b.east_m, b.north_m) || map.InDetail(b.east_m, b.north_m)) continue;
            const double d = std::hypot(b.east_m - a.close_e_m, b.north_m - a.close_n_m);
            if (d >= best) continue;
            const double z = b.length_m;
            if (!canopy_at(b.east_m + z, b.north_m, z) || !canopy_at(b.east_m - z, b.north_m, z) || !canopy_at(b.east_m, b.north_m + z, z) ||
                !canopy_at(b.east_m, b.north_m - z, z)) {
                continue;
            }
            best = d;
            wood = &b;
        }
        if (wood == nullptr) {
            s = Missing("no wood of the map proper with wood on its four sides");
        } else {
            s.ok = true;
            s.e = s.eye_e = wood->east_m;
            s.n = s.eye_n = wood->north_m;
            s.bearing_rad = a.has_bearing ? a.bearing_rad : 0.0;
        }
    } else if (a.kind == "sunken_lane") {
        // The nearest sunken lane to close_km (terrain roadmap A.3.34), looked up from its floor: the eye on its centre line
        // a third of the way up from its foot (or from_m up when given), looking up it to its head.
        double best = std::numeric_limits<double>::max();
        std::size_t found = map.LaneCount();
        for (std::size_t i = 0; i < map.LaneCount(); ++i) {
            MapPoint foot, head;
            double depth = 0.0;
            map.LaneEnds(i, foot, head, depth);
            const double d = std::hypot(0.5 * (foot.east_m + head.east_m) - a.close_e_m, 0.5 * (foot.north_m + head.north_m) - a.close_n_m);
            if (d < best) {
                best = d;
                found = i;
            }
        }
        if (found == map.LaneCount()) {
            s = Missing("the map has no sunken lanes");
        } else {
            MapPoint foot, head;
            double depth = 0.0;
            map.LaneEnds(found, foot, head, depth);
            const double len = std::hypot(head.east_m - foot.east_m, head.north_m - foot.north_m);
            const double up = a.from_m > 0.0 ? std::min(a.from_m, len) : len / 3.0;
            s.ok = true;
            s.e = head.east_m;
            s.n = head.north_m;
            s.bearing_rad = BearingTo(foot.east_m, foot.north_m, head.east_m, head.north_m);
            s.eye_e = foot.east_m + up * std::sin(s.bearing_rad);
            s.eye_n = foot.north_m + up * std::cos(s.bearing_rad);
        }
    } else if (a.kind == "pond") {
        // The nearest village pond to close_km (terrain roadmap A.3.34), looked at across it: the eye from_m beyond its edge on
        // the side the data's bearing looks from; when none, from the far side of its village, looking over the water at the
        // village's middle (A.3.36, the look target's K-f).
        double best = std::numeric_limits<double>::max();
        std::size_t found = map.PondCount();
        for (std::size_t i = 0; i < map.PondCount(); ++i) {
            MapPoint c;
            double rx = 0.0, ry = 0.0, heading = 0.0, level = 0.0;
            map.PondAt(i, c, rx, ry, heading, level);
            const double d = std::hypot(c.east_m - a.close_e_m, c.north_m - a.close_n_m);
            if (d < best) {
                best = d;
                found = i;
            }
        }
        if (found == map.PondCount()) {
            s = Missing("the map has no ponds");
        } else {
            MapPoint c;
            double rx = 0.0, ry = 0.0, heading = 0.0, level = 0.0;
            map.PondAt(found, c, rx, ry, heading, level);
            double bearing = heading + 0.5 * kPi;
            double nearest = std::numeric_limits<double>::max();
            for (const MapSettlementDef& v : map.Def().settlements) {
                const double dv = std::hypot(v.centre.east_m - c.east_m, v.centre.north_m - c.north_m);
                if (v.kind == "village" && dv < nearest && dv > 1.0) {
                    nearest = dv;
                    bearing = BearingTo(c.east_m, c.north_m, v.centre.east_m, v.centre.north_m);
                }
            }
            if (a.has_bearing) bearing = a.bearing_rad;
            // The pond's edge along the bearing, in its own frame (x along its length, y across).
            const double x = std::cos(bearing - heading), y = std::sin(bearing - heading);
            const double back = rx * ry / std::hypot(ry * x, rx * y) + a.from_m;
            s.ok = true;
            s.e = c.east_m;
            s.n = c.north_m;
            s.eye_e = c.east_m - back * std::sin(bearing);
            s.eye_n = c.north_m - back * std::cos(bearing);
            s.bearing_rad = bearing;
        }
    } else if (a.kind == "scrub_thicket" || a.kind == "pasture_bush") {
        // The nearest of the hills' scrub to close_km (terrain roadmap A.3.41): a thicket (longer than a clump's longest),
        // or a lone bush on a grazed slope (plot 6's). Looked at up the slope from from_m below its edge (30 m for a
        // thicket, 15 for a bush, when none), along the data's bearing when given; the eye stepped back to open ground.
        const bool thicket = a.kind == "scrub_thicket";
        const MapLandUseDef& u = map.Def().land_use;
        const MapBox* box = nullptr;
        double best = std::numeric_limits<double>::max();
        for (const MapBox& b : map.Layout().boxes) {
            if (!b.scrub) continue;
            const bool fits = thicket ? b.length_m > 1.3 * u.scrub_clump_m
                                      : b.length_m <= 1.3 * u.scrub_bush_m && map.LandUseGrass(b.east_m, b.north_m) == MapData::GrassUse::Pasture;
            const double d = std::hypot(b.east_m - a.close_e_m, b.north_m - a.close_n_m);
            if (fits && d < best) {
                best = d;
                box = &b;
            }
        }
        if (box == nullptr) {
            s = Missing(thicket ? "the map has no thickets" : "the map has no lone bushes on its pastures");
        } else {
            const double ge = map.GroundHeight(box->east_m + 10.0, box->north_m) - map.GroundHeight(box->east_m - 10.0, box->north_m);
            const double gn = map.GroundHeight(box->east_m, box->north_m + 10.0) - map.GroundHeight(box->east_m, box->north_m - 10.0);
            const double bearing = a.has_bearing ? a.bearing_rad : (std::hypot(ge, gn) > 1e-3 ? std::atan2(ge, gn) : box->heading_rad + 0.5 * kPi);
            const double from = a.from_m > 0.0 ? a.from_m : (thicket ? 30.0 : 15.0);
            const double edge = 0.5 * std::fabs(box->length_m * std::cos(bearing - box->heading_rad)) + 0.5 * std::fabs(box->width_m * std::sin(bearing - box->heading_rad));
            double back = edge + from;
            while (back < edge + from + 100.0 && map.ObstacleHeight(box->east_m - back * std::sin(bearing), box->north_m - back * std::cos(bearing)) > 1.0) back += 5.0;
            s.ok = true;
            s.e = box->east_m;
            s.n = box->north_m;
            s.bearing_rad = bearing;
            s.eye_e = box->east_m - back * std::sin(bearing);
            s.eye_n = box->north_m - back * std::cos(bearing);
        }
    } else if (a.kind == "brook_row") {
        // The nearest row of trees along a brook to close_km (terrain roadmap A.3.20's alders), looked at across the brook:
        // the bearing from the brook's nearest point to the row's middle, the eye from_m back from the row on that line.
        const MapBox* row = nullptr;
        double best = std::numeric_limits<double>::max();
        for (const MapBox& b : map.Layout().boxes) {
            if (b.part != MapPart::TreeRow || !b.by_water) continue;
            const double d = std::hypot(b.east_m - a.close_e_m, b.north_m - a.close_n_m);
            if (d < best) {
                best = d;
                row = &b;
            }
        }
        double we = 0.0, wn = 0.0, wbest = std::numeric_limits<double>::max();
        if (row != nullptr) {
            for (const MapStrip& st : map.Layout().strips) {
                if (st.surface != MapSurface::Water || st.id.compare(0, 6, "brook_") != 0) continue;
                for (const auto& q : st.points) {
                    const double d = std::hypot(q.x - row->east_m, q.y - row->north_m);
                    if (d < wbest) {
                        wbest = d;
                        we = q.x;
                        wn = q.y;
                    }
                }
            }
        }
        if (row == nullptr || wbest > 100.0 || wbest < 0.5) {
            s = Missing(row == nullptr ? "the map has no rows along its brooks" : "no brook beside the nearest brook row");
        } else {
            s.ok = true;
            s.e = row->east_m;
            s.n = row->north_m;
            s.bearing_rad = BearingTo(we, wn, row->east_m, row->north_m);
            s.eye_e = row->east_m - a.from_m * std::sin(s.bearing_rad);
            s.eye_n = row->north_m - a.from_m * std::cos(s.bearing_rad);
        }
    } else if (a.kind == "wood_edge") {
        // The nearest wood to close_km; the eye stands from_m outside its edge on the side the look comes from (the data's
        // bearing, north when none): the first open ground walking back from the wood's box against the look.
        const MapBox* wood = nullptr;
        double best = std::numeric_limits<double>::max();
        for (const MapBox& b : map.Layout().boxes) {
            if (b.part != MapPart::Canopy) continue;
            const double d = std::hypot(b.east_m - a.close_e_m, b.north_m - a.close_n_m);
            if (d < best) {
                best = d;
                wood = &b;
            }
        }
        if (wood == nullptr) {
            s = Missing("the map has no woods");
        } else {
            const double bearing = a.has_bearing ? a.bearing_rad : 0.0;
            // Woods are tiled from many boxes: from the box's centre walk back against the look until the ground is open.
            bool found = false;
            double edge_e = 0.0, edge_n = 0.0;
            for (double t = 0.0; t <= 1500.0; t += 5.0) {
                const double pe = wood->east_m - t * std::sin(bearing), pn = wood->north_m - t * std::cos(bearing);
                if (t > 0.0 && map.ObstacleHeight(pe, pn) <= 3.0) {
                    found = true;
                    edge_e = pe;
                    edge_n = pn;
                    break;
                }
            }
            if (!found) {
                s = Missing("no edge of the nearest wood found");
            } else {
                s.ok = true;
                s.e = edge_e;
                s.n = edge_n;
                s.eye_e = edge_e - a.from_m * std::sin(bearing);
                s.eye_n = edge_n - a.from_m * std::cos(bearing);
                s.bearing_rad = bearing;
            }
        }
    } else if (a.kind == "high_ground") {
        // The highest open ground within kHighGroundReachM of close_km (sampled every 50 m; roadmap L11's hill country; open:
        // nothing standing within 40 m, since the hills' crests are often wooded): the eye there, looking along the
        // bearing at the ground a kilometre ahead.
        constexpr double kHighGroundReachM = 1500.0, kOpenM = 40.0;
        const auto open = [&map](double e, double n) {
            const double k = kOpenM;
            const double ring[5][2] = {{0.0, 0.0}, {k, 0.0}, {-k, 0.0}, {0.0, k}, {0.0, -k}};
            for (const auto& o : ring) {
                if (map.ObstacleTop(e + o[0], n + o[1]) > map.GroundHeight(e + o[0], n + o[1]) + 1.0) return false;
            }
            return true;
        };
        double best = -1e18, be = a.close_e_m, bn = a.close_n_m;
        for (double dn = -kHighGroundReachM; dn <= kHighGroundReachM; dn += 50.0) {
            for (double de = -kHighGroundReachM; de <= kHighGroundReachM; de += 50.0) {
                if (std::hypot(de, dn) > kHighGroundReachM) continue;
                const double h = map.GroundHeight(a.close_e_m + de, a.close_n_m + dn);
                if (h > best && open(a.close_e_m + de, a.close_n_m + dn)) {
                    best = h;
                    be = a.close_e_m + de;
                    bn = a.close_n_m + dn;
                }
            }
        }
        s.ok = true;
        s.eye_e = be;
        s.eye_n = bn;
        s.bearing_rad = a.has_bearing ? a.bearing_rad : 0.0;
        s.e = be + 1000.0 * std::sin(s.bearing_rad);
        s.n = bn + 1000.0 * std::cos(s.bearing_rad);
    } else if (a.kind == "hill") {
        const MapHillDef* h = FindById(def.hills, a.id);
        if (h == nullptr) {
            s = Missing("the map has no hill \"" + a.id + "\"");
        } else {
            s.ok = true;
            s.e = h->centre.east_m;
            s.n = h->centre.north_m;
            s.eye_e = s.e + a.from_m * std::sin(a.bearing_rad);
            s.eye_n = s.n + a.from_m * std::cos(a.bearing_rad);
            s.bearing_rad = BearingTo(s.eye_e, s.eye_n, s.e, s.n);
        }
    }
    return s;
}

// The cameras of one view on the map: one, or a panorama's several (named <id>_h<degrees>). A view the map cannot give
// has ok false and its reason. `cover` is the land cover (BakeCover), for the parcels of farmland.
inline std::vector<ResolvedView> ResolveView(const MapData& map, const DressingRules& /*rules*/, const CoverRaster& cover, const ViewDef& v) {
    using namespace views_detail;
    const ViewAnchor& a = v.anchor;
    const Spot s = ResolveSpot(map, cover, a);
    std::vector<ResolvedView> out;
    if (!s.ok) {
        ResolvedView r;
        r.id = v.id;
        r.missing_reason = s.why.empty() ? "the anchor \"" + a.kind + "\" is unknown" : s.why;
        r.fov_rad = v.fov_rad;
        out.push_back(r);
        return out;
    }
    std::vector<double> bearings;
    if (v.look == "panorama") {
        for (double b = 0.0; b < 2.0 * kPi - 1e-6; b += v.step_rad) bearings.push_back(b);
    } else {
        bearings.push_back(s.bearing_rad);
    }
    for (double bearing : bearings) {
        ResolvedView r;
        r.id = v.id;
        if (v.look == "panorama") {
            char tail[16];
            std::snprintf(tail, sizeof tail, "_h%03d", static_cast<int>(std::lround(bearing * 180.0 / kPi)) % 360);
            r.id += tail;
        }
        r.fov_rad = v.fov_rad;
        r.bearing_rad = bearing;
        double ee = s.eye_e, en = s.eye_n;
        const double ground = map.GroundHeight(ee, en);
        double eye_z = ground + v.eye_m;
        // An eye inside a building or a tree is stepped back along its view line, up to 50 m, else the view is missing.
        double back = 0.0;
        while (back <= 50.0) {
            ee = s.eye_e - back * std::sin(bearing);
            en = s.eye_n - back * std::cos(bearing);
            eye_z = map.GroundHeight(ee, en) + v.eye_m;
            if (map.ObstacleTop(ee, en) < eye_z - 0.3) break;
            back += 5.0;
        }
        if (back > 50.0) {
            r.missing_reason = "something stands at the eye (a building or a tree)";
            out.push_back(r);
            continue;
        }
        // The point looked at: 200 m along the bearing, tilted by the pitch (a look at the anchor itself is its own point).
        const double reach = 200.0;
        double te = ee + reach * std::sin(bearing), tn = en + reach * std::cos(bearing);
        double tz = eye_z + reach * std::tan(v.pitch_rad);
        if (v.look == "at_anchor" && v.pitch_rad == 0.0) {
            te = s.e;
            tn = s.n;
            tz = map.GroundHeight(te, tn);
        }
        r.ok = true;
        r.eye_e = ee;
        r.eye_n = en;
        r.eye_up_m = v.eye_m;
        r.target_e = te;
        r.target_n = tn;
        r.target_up_m = tz - map.GroundHeight(te, tn);
        out.push_back(r);
    }
    return out;
}

}  // namespace scenery
}  // namespace simcore
