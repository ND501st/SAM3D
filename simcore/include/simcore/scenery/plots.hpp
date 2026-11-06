// simcore/scenery/plots.hpp — the test plots the terrain's look is perfected on (roadmap L1, docs/scenery/TERRAIN_ROADMAP.md
// part A): a plot is a small piece of one land type (a ploughed field, a wood's edge, a road with its verge), found on the
// map by an anchor as the views are (scenery/views.hpp), and seen from three cameras looking at its middle: the eye
// (1.7 m), a low oblique (25 m, about 30 degrees down) and a high oblique (150 m, about 30 degrees down). The look is
// made by rules that apply to every matching part of the map, so a plot found anywhere shows the same rules.
// Data from data/scenery/plots.json (LoadPlots); ResolvePlot gives the cameras.
//
// Presentation only: nothing in the simulation reads it. Headings are true bearings (radians clockwise from north).
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/json.hpp"
#include "simcore/scenery/ground_layers.hpp"
#include "simcore/scenery/map_dressing.hpp"
#include "simcore/scenery/views.hpp"
#include "simcore/terrain_map.hpp"

namespace simcore {
namespace scenery {

// One of a plot's three cameras: its height over the ground and its distance back from the plot's middle.
struct PlotCamera {
    const char* id;
    double up_m;
    double back_m;
};

// Eye: 30 m back at 1.7 m. Low and high: about 30 degrees down onto the middle (25 / 43, 150 / 260).
inline const std::vector<PlotCamera>& PlotCameras() {
    static const std::vector<PlotCamera> c = {{"eye", 1.7, 30.0}, {"low", 25.0, 43.0}, {"high", 150.0, 260.0}};
    return c;
}

struct PlotDef {
    std::string id;
    std::string name;
    ViewAnchor anchor;         // a view's anchor kinds, and field_layer (a field in one ground layer on the day)
    std::string layer;         // field_layer: the ground layer's id (data/scenery/ground_layers.json)
    std::string crop;          // field_layer: only a field of this crop (the dressing's id); empty: any
    double turn_rad = 0.0;     // the cameras' bearing from the anchor's own (field_layer: from the furrows' heading)
    bool edge = false;         // field_layer: look at where the field ends toward the cameras, from outside it (a standing crop's face)
    double core_m = 50.0;      // the plot's side: a field_layer plot's whole square is the one layer of one parcel
    double fov_rad = 1.0472;
    std::vector<std::string> photos;
    std::string must_see;
};

struct PlotSet {
    std::vector<PlotDef> plots;
};

namespace plots_detail {

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

}  // namespace plots_detail

// Reads the plots from a file's path or the JSON text itself (it starts with '{'). An anchor is read as a view's is,
// plus the kind field_layer ({"kind": "field_layer", "layer": "<id>", "close_km": [e, n]}). `photos`, if given, is the
// views' set whose photo register the plots' photos must be in. False with the reason in `error` (the set is unchanged).
inline bool LoadPlots(const std::string& path_or_json, PlotSet& out, std::string* error = nullptr, const ViewSet* photos = nullptr) {
    using plots_detail::Fail;
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
    const json::Value* list = root.Find("plots");
    if (list == nullptr || !list->Is(json::Type::Array) || list->items.empty()) return Fail(error, "no \"plots\" list");
    PlotSet set;
    for (const json::Value& item : list->items) {
        PlotDef p;
        p.id = plots_detail::Text(item, "id");
        if (p.id.empty()) return Fail(error, "a plot without an id");
        for (const PlotDef& other : set.plots) {
            if (other.id == p.id) return Fail(error, "the plot \"" + p.id + "\" twice");
        }
        p.name = plots_detail::Text(item, "name");
        const json::Value* a = item.Find("anchor");
        if (a == nullptr || !a->Is(json::Type::Object)) return Fail(error, p.id + ": no anchor");
        const std::string kind = plots_detail::Text(*a, "kind");
        if (kind == "field_layer") {
            p.anchor.kind = kind;
            p.layer = plots_detail::Text(*a, "layer");
            if (p.layer.empty()) return Fail(error, p.id + ": a field_layer anchor names its layer");
            if (const json::Value* crop = a->Find("crop")) {
                if (!crop->Is(json::Type::String) || crop->string.empty()) return Fail(error, p.id + ": crop is a crop's id");
                p.crop = crop->string;
            }
            const json::Value* c = a->Find("close_km");
            if (c == nullptr || !c->Is(json::Type::Array) || c->items.size() != 2 || !c->items[0].Is(json::Type::Number) ||
                !c->items[1].Is(json::Type::Number)) {
                return Fail(error, p.id + ": close_km is [east, north] in km");
            }
            p.anchor.has_close = true;
            p.anchor.close_e_m = c->items[0].number * 1000.0;
            p.anchor.close_n_m = c->items[1].number * 1000.0;
            if (const json::Value* edge = a->Find("edge")) {
                if (!edge->Is(json::Type::Bool)) return Fail(error, p.id + ": edge is true or false");
                p.edge = edge->boolean;
            }
        } else {
            if (!ParseAnchor(*a, p.id, p.anchor, error)) return false;  // any other kind is a view's anchor
        }
        p.turn_rad = plots_detail::Number(item, "turn_deg", 0.0) * kPi / 180.0;
        p.core_m = plots_detail::Number(item, "core_m", 50.0);
        if (p.core_m < 10.0 || p.core_m > 400.0) return Fail(error, p.id + ": core_m is not in 10..400");
        const double fov = plots_detail::Number(item, "fov_deg", 60.0);
        if (fov < 20.0 || fov > 120.0) return Fail(error, p.id + ": fov_deg is not in 20..120");
        p.fov_rad = fov * kPi / 180.0;
        p.must_see = plots_detail::Text(item, "must_see");
        if (const json::Value* ps = item.Find("photos")) {
            if (!ps->Is(json::Type::Array)) return Fail(error, p.id + ": photos is not a list");
            for (const json::Value& ph : ps->items) {
                if (!ph.Is(json::Type::String)) return Fail(error, p.id + ": a photo is not a name");
                if (photos != nullptr && photos->Photo(ph.string) == nullptr) {
                    return Fail(error, p.id + ": the photo " + ph.string + " is not in the views' register");
                }
                p.photos.push_back(ph.string);
            }
        }
        set.plots.push_back(p);
    }
    out = set;
    return true;
}

namespace plots_detail {

// The nearest point to close_km, on a 25 m lattice within 3 km, whose whole square of `core_m` (its corners, its sides'
// middles and its middle, 10 m wider each way) lies in one parcel of farmland in the layer `layer` on the day.
inline views_detail::Spot FieldLayerSpot(const CoverRaster& cover, const DressingRules& dressing, const GroundLayerRules& layers,
                                         int day_of_year, const PlotDef& p) {
    const int want = layers.Index(p.layer);
    if (want < 0) return views_detail::Missing("no ground layer \"" + p.layer + "\"");
    int crop = -1;
    if (!p.crop.empty()) {
        for (std::size_t i = 0; i < dressing.crops.size(); ++i) {
            if (dressing.crops[i].id == p.crop) crop = static_cast<int>(i);
        }
        if (crop < 0) return views_detail::Missing("no crop \"" + p.crop + "\"");
    }
    const double half = 0.5 * p.core_m + 10.0;
    double best = std::numeric_limits<double>::max();
    views_detail::Spot s;
    for (double de = -3000.0; de <= 3000.0; de += 25.0) {
        for (double dn = -3000.0; dn <= 3000.0; dn += 25.0) {
            const double d = de * de + dn * dn;
            if (d >= best) continue;
            const double pe = p.anchor.close_e_m + de, pn = p.anchor.close_n_m + dn;
            const CoverGrid* g = cover.GridAt(pe, pn);
            if (g == nullptr || cover.At(pe, pn) != Cover::Field) continue;
            if (PointLayer(cover, dressing, layers, pe, pn, day_of_year) != want) continue;
            if (crop >= 0 && CropOf(dressing, FieldAt(g->fields, pe, pn).id) != crop) continue;
            const std::uint64_t parcel = FieldParcel(g->fields, pe, pn);
            bool whole = true;
            for (int i = -1; i <= 1 && whole; ++i) {
                for (int j = -1; j <= 1 && whole; ++j) {
                    const double qe = pe + i * half, qn = pn + j * half;
                    whole = cover.GridAt(qe, qn) == g && cover.At(qe, qn) == Cover::Field && FieldParcel(g->fields, qe, qn) == parcel &&
                            PointLayer(cover, dressing, layers, qe, qn, day_of_year) == want;
                }
            }
            if (!whole) continue;
            best = d;
            s.ok = true;
            s.e = s.eye_e = pe;
            s.n = s.eye_n = pn;
            s.bearing_rad = FieldHeading(g->fields, pe, pn);
        }
    }
    if (!s.ok) {
        return views_detail::Missing("no field" + (p.crop.empty() ? std::string() : " of " + p.crop) + " in \"" + p.layer + "\" with a whole square within 3 km of close_km");
    }
    return s;
}

}  // namespace plots_detail

// The plot's three cameras (named <plot>_eye, <plot>_low, <plot>_high), each looking at the plot's middle on the ground
// along the anchor's bearing turned by turn_deg. A camera that would stand in a building or a tree steps back along its
// line, up to 50 m, else it is missing with the reason; a plot the map cannot give is missing whole.
inline std::vector<ResolvedView> ResolvePlot(const MapData& map, const DressingRules& dressing, const CoverRaster& cover,
                                             const GroundLayerRules& layers, int day_of_year, const PlotDef& p) {
    views_detail::Spot s = p.anchor.kind == "field_layer" ? plots_detail::FieldLayerSpot(cover, dressing, layers, day_of_year, p)
                                                           : ResolveSpot(map, cover, p.anchor);
    if (s.ok && p.edge) {
        // Back toward the cameras, metre by metre, to the field's last point in the layer: the cameras look at its face.
        // The four sides in turn from turn_deg's: the first whose cameras' ground (to the high one's) is open farmland in
        // another layer (not a road, a wood or the same crop).
        const int want = layers.Index(p.layer);
        const auto in_layer = [&](double e, double n) {
            return cover.At(e, n) == Cover::Field && PointLayer(cover, dressing, layers, e, n, day_of_year) == want;
        };
        for (int side = 0; side < 4; ++side) {
            const double bearing = s.bearing_rad + p.turn_rad + side * 0.5 * kPi, ue = std::sin(bearing), un = std::cos(bearing);
            double back = 0.0;
            while (back < 2000.0 && in_layer(s.e - (back + 1.0) * ue, s.n - (back + 1.0) * un)) back += 1.0;
            const double ee = s.e - back * ue, en = s.n - back * un;
            bool open = true;
            for (double d = 5.0; d <= 60.0 && open; d += 5.0) {
                const double qe = ee - d * ue, qn = en - d * un;
                open = cover.At(qe, qn) == Cover::Field && !in_layer(qe, qn);
            }
            if (!open && side < 3) continue;
            s.e = s.eye_e = ee;
            s.n = s.eye_n = en;
            s.bearing_rad += side * 0.5 * kPi;
            break;
        }
    }
    std::vector<ResolvedView> out;
    for (const PlotCamera& c : PlotCameras()) {
        ResolvedView r;
        r.id = p.id + "_" + c.id;
        r.fov_rad = p.fov_rad;
        if (!s.ok) {
            r.missing_reason = s.why.empty() ? "the anchor \"" + p.anchor.kind + "\" is unknown" : s.why;
            out.push_back(r);
            continue;
        }
        const double bearing = s.bearing_rad + p.turn_rad;
        const double ue = std::sin(bearing), un = std::cos(bearing);
        double back = c.back_m;
        double ee = 0.0, en = 0.0;
        bool clear = false;
        for (; back <= c.back_m + 50.0; back += 5.0) {
            ee = s.e - back * ue;
            en = s.n - back * un;
            if (map.ObstacleTop(ee, en) < map.GroundHeight(ee, en) + c.up_m - 0.3) {
                clear = true;
                break;
            }
        }
        if (!clear) {
            r.missing_reason = "something stands at the camera (a building or a tree)";
            out.push_back(r);
            continue;
        }
        r.ok = true;
        r.bearing_rad = bearing;
        r.eye_e = ee;
        r.eye_n = en;
        r.eye_up_m = c.up_m;
        r.target_e = s.e;
        r.target_n = s.n;
        r.target_up_m = 0.0;
        out.push_back(r);
    }
    return out;
}

}  // namespace scenery
}  // namespace simcore
