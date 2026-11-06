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
    static const std::vector<std::string> k = {"parcel_edge", "settlement", "road", "farmstead", "farmyard", "wood_edge", "site", "river", "hill", "high_ground"};
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
    if ((an.kind == "parcel_edge" || an.kind == "farmstead" || an.kind == "farmyard" || an.kind == "wood_edge" || an.kind == "high_ground") &&
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
        // The highest ground within kHighGroundReachM of close_km (sampled every 50 m; roadmap L11's hill country): the eye
        // there, looking along the bearing at the ground a kilometre ahead.
        constexpr double kHighGroundReachM = 1500.0;
        double best = -1e18, be = a.close_e_m, bn = a.close_n_m;
        for (double dn = -kHighGroundReachM; dn <= kHighGroundReachM; dn += 50.0) {
            for (double de = -kHighGroundReachM; de <= kHighGroundReachM; de += 50.0) {
                if (std::hypot(de, dn) > kHighGroundReachM) continue;
                const double h = map.GroundHeight(a.close_e_m + de, a.close_n_m + dn);
                if (h > best) {
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
