// simcore/scenery/basic_models.hpp — ultra-basic models: every aircraft, vehicle, site and building as a few primitives
// (docs/scenery/README.md; data/scenery/basic_models.json).
//
// Until real models exist, each type gets the right size, a recognisable silhouette and its side's colour, built from
// boxes, cylinders, cones and spheres. The data names an archetype and a few dimensions; this file expands it into parts
// in the model's frame (x forward, y right, z up, metres; the origin on the ground under the middle, aircraft at the
// middle of the fuselage). Presentation data, engine-agnostic: Unreal turns the parts into its basic shapes
// (SimBasicModels.cpp), and the tests check that every aircraft of the air war and every battery vehicle has a model.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace scenery {

enum class Shape { Box, Cylinder, Cone, Sphere };

// One primitive. A box's or sphere's size is its extent along the model's axes; a cylinder's or cone's runs along
// `axis` ('x', 'y' or 'z': its length) with the other two sizes its diameters; a cone's tip points along +axis.
// Then it is tilted: pitch (nose up, about y), roll (about x), yaw (about z), degrees.
struct Part {
    Shape shape = Shape::Box;
    Vec3 size;   // metres
    Vec3 at;     // the part's centre, metres
    char axis = 'z';
    double pitch_deg = 0.0;
    double roll_deg = 0.0;
    double yaw_deg = 0.0;
    std::string colour;  // "#rrggbb"
};

struct ModelDef {
    std::string id;
    std::string name;
    std::string archetype;
    double length_m = 0.0;
    double span_m = 0.0;   // aircraft: wing span, helicopters: the rotor; ground: width
    double height_m = 0.0;
    std::vector<Part> parts;
};

struct ModelLibrary {
    std::vector<ModelDef> models;
    std::map<std::string, std::string> fallbacks;  // aircraft class -> model
    std::map<std::string, std::string> ground;     // battery vehicle kind -> model
    std::map<std::string, std::string> colours;    // name -> #rrggbb

    const ModelDef* Find(const std::string& id) const {
        for (const ModelDef& m : models) {
            if (m.id == id) return &m;
        }
        return nullptr;
    }
    // The model for a flying target: its own model if the library has it, else its class's, else the default.
    std::string ForTarget(const std::string& model, const std::string& target_class, const std::string& type_tags) const {
        if (!model.empty() && Find(model) != nullptr) return model;
        std::string cls = target_class;
        if (type_tags.find("Rotorcraft") != std::string::npos) cls = "helicopter";
        if (type_tags.find("Missile") != std::string::npos) cls = "missile";
        auto it = fallbacks.find(cls);
        if (it == fallbacks.end()) it = fallbacks.find("default");
        return it == fallbacks.end() ? std::string() : it->second;
    }
    std::string ForGround(const std::string& kind) const {
        const auto it = ground.find(kind);
        return it == ground.end() ? std::string() : it->second;
    }
};

namespace detail {

// The archetype's parameters, as read from the data.
struct Params {
    double length = 0.0, span = 0.0, width = 0.0, height = 0.0, diameter = 0.0, rotor = 0.0, missile = 0.0, elevation = 0.0, antenna_h = 0.0;
    int engines = 1, axles = 2, missiles = 0, rails = 1, barrels = 1;
    bool biplane = false, high_wing = false, wings = false, turret = false, mast = false, cab = false, gun = false, tower = false;
    std::string wing = "swept", tail = "conventional", radar = "none", body = "cargo", roof = "pitched", crown = "cone";
    std::string colour, colour2;
};

inline Part P(Shape s, Vec3 size, Vec3 at, const std::string& colour, char axis = 'z', double pitch = 0.0, double roll = 0.0, double yaw = 0.0) {
    Part p;
    p.shape = s;
    p.size = size;
    p.at = at;
    p.colour = colour;
    p.axis = axis;
    p.pitch_deg = pitch;
    p.roll_deg = roll;
    p.yaw_deg = yaw;
    return p;
}

// A pitched roof: a square beam turned 45 degrees about its length, its upper half above the walls (a 45-degree ridge).
inline void Roof(std::vector<Part>& out, double l, double w, double wall_h, const std::string& colour) {
    const double side = w / std::sqrt(2.0);
    out.push_back(P(Shape::Box, Vec3{l * 1.02, side, side}, Vec3{0.0, 0.0, wall_h}, colour, 'z', 0.0, 45.0));
}

inline void Wheels(std::vector<Part>& out, double l, double w, double d, int axles, const std::string& colour) {
    for (int i = 0; i < axles; ++i) {
        const double x = axles == 1 ? 0.0 : (-0.35 + 0.7 * i / (axles - 1)) * l;
        for (double side : {-1.0, 1.0}) out.push_back(P(Shape::Cylinder, Vec3{d, d, 0.35}, Vec3{x, side * (w * 0.5 - 0.15), d * 0.5}, colour, 'y'));
    }
}

inline std::vector<Part> Expand(const std::string& arch, const Params& a, const std::string& dark, bool& ok) {
    std::vector<Part> o;
    const std::string& c = a.colour;
    const std::string& c2 = a.colour2.empty() ? dark : a.colour2;
    const double L = a.length, S = a.span, H = a.height;
    ok = true;
    if (arch == "jet") {
        const double d = std::max(0.9, std::min(2.6, H * 0.3));
        o.push_back(P(Shape::Cylinder, Vec3{d, d, 0.8 * L}, Vec3{-0.06 * L, 0.0, 0.0}, c, 'x'));
        o.push_back(P(Shape::Cone, Vec3{d, d, 0.16 * L}, Vec3{0.42 * L, 0.0, 0.0}, c, 'x'));
        o.push_back(P(Shape::Sphere, Vec3{0.12 * L, d * 0.55, d * 0.55}, Vec3{0.24 * L, 0.0, d * 0.42}, dark));
        const double chord = a.wing == "delta" ? 0.42 * L : (a.wing == "straight" ? 0.16 * L : (a.wing == "variable" ? 0.2 * L : 0.24 * L));
        const double wx = a.wing == "delta" ? -0.16 * L : (a.wing == "straight" ? 0.0 : -0.06 * L);
        const double wspan = a.wing == "variable" ? S * 0.85 : S;  // half swept, as at speed
        o.push_back(P(Shape::Box, Vec3{chord, wspan, 0.18}, Vec3{wx, 0.0, a.wing == "straight" && L < 16.0 ? 0.0 : -d * 0.15}, c, 'z', 0.0, 0.0,
                      0.0));
        if (a.tail != "none") o.push_back(P(Shape::Box, Vec3{0.1 * L, 0.42 * S, 0.12}, Vec3{-0.42 * L, 0.0, 0.0}, c));
        const double fin_h = 0.42 * H;
        if (a.tail == "twin") {
            for (double side : {-1.0, 1.0}) o.push_back(P(Shape::Box, Vec3{0.15 * L, 0.12, fin_h}, Vec3{-0.4 * L, side * d * 0.45, d * 0.5 + fin_h * 0.5}, c));
        } else {
            o.push_back(P(Shape::Box, Vec3{0.16 * L, 0.14, fin_h}, Vec3{-0.4 * L, 0.0, d * 0.5 + fin_h * 0.5}, c));
        }
        for (int e = 0; e < a.engines; ++e) {
            const double y = a.engines == 1 ? 0.0 : (e == 0 ? -0.22 : 0.22) * d;
            o.push_back(P(Shape::Cylinder, Vec3{d * 0.55, d * 0.55, 0.05 * L}, Vec3{-0.47 * L, y, 0.0}, dark, 'x'));
        }
    } else if (arch == "prop") {
        const double w = std::max(0.9, 0.11 * L), h = std::max(1.0, 0.16 * L);
        o.push_back(P(Shape::Box, Vec3{0.85 * L, w, h}, Vec3{0.0, 0.0, 0.0}, c));
        const double chord = 0.15 * L;
        const double wz = a.high_wing ? h * 0.5 : -h * 0.35;
        if (a.engines >= 2) {
            // Twin engines: a nacelle under each wing with its propeller ahead of the leading edge.
            for (double side : {-1.0, 1.0}) {
                o.push_back(P(Shape::Cylinder, Vec3{0.06 * L, 0.06 * L, 0.22 * L}, Vec3{0.14 * L, side * 0.17 * S, wz - 0.03 * L}, c, 'x'));
                o.push_back(P(Shape::Cylinder, Vec3{S * 0.13, S * 0.13, 0.06}, Vec3{0.26 * L, side * 0.17 * S, wz - 0.03 * L}, dark, 'x'));
            }
        } else {
            o.push_back(P(Shape::Cylinder, Vec3{S * 0.16, S * 0.16, 0.06}, Vec3{0.44 * L, 0.0, 0.0}, dark, 'x'));
        }
        o.push_back(P(Shape::Box, Vec3{chord, S, 0.15}, Vec3{0.12 * L, 0.0, a.biplane ? -h * 0.4 : wz}, c));
        if (a.biplane) o.push_back(P(Shape::Box, Vec3{chord, S * 0.85, 0.15}, Vec3{0.14 * L, 0.0, h * 0.75}, c));
        o.push_back(P(Shape::Box, Vec3{0.1 * L, 0.32 * S, 0.1}, Vec3{-0.42 * L, 0.0, 0.0}, c));
        o.push_back(P(Shape::Box, Vec3{0.12 * L, 0.12, 0.3 * H}, Vec3{-0.42 * L, 0.0, h * 0.5 + 0.15 * H}, c));
    } else if (arch == "helicopter") {
        const double w = std::max(1.2, 0.13 * L), h = 0.42 * H;
        o.push_back(P(Shape::Box, Vec3{0.45 * L, w, h}, Vec3{0.12 * L, 0.0, 0.0}, c));
        o.push_back(P(Shape::Sphere, Vec3{0.16 * L, w, h * 0.9}, Vec3{0.36 * L, 0.0, -h * 0.05}, dark));
        o.push_back(P(Shape::Cylinder, Vec3{w * 0.35, w * 0.35, 0.5 * L}, Vec3{-0.32 * L, 0.0, h * 0.2}, c, 'x'));
        o.push_back(P(Shape::Cylinder, Vec3{0.35, 0.35, 0.22 * H}, Vec3{0.12 * L, 0.0, h * 0.5 + 0.11 * H}, dark));
        o.push_back(P(Shape::Cylinder, Vec3{a.rotor, a.rotor, 0.08}, Vec3{0.12 * L, 0.0, h * 0.5 + 0.22 * H}, dark));
        o.push_back(P(Shape::Cylinder, Vec3{a.rotor * 0.18, a.rotor * 0.18, 0.06}, Vec3{-0.56 * L, w * 0.25, h * 0.4}, dark, 'y'));
        o.push_back(P(Shape::Box, Vec3{0.08 * L, 0.12, 0.2 * H}, Vec3{-0.54 * L, 0.0, h * 0.45}, c));
        if (a.wings) o.push_back(P(Shape::Box, Vec3{0.08 * L, a.rotor * 0.38, 0.12}, Vec3{0.05 * L, 0.0, -h * 0.1}, c));
        for (double side : {-1.0, 1.0}) o.push_back(P(Shape::Box, Vec3{0.35 * L, 0.1, 0.1}, Vec3{0.12 * L, side * w * 0.5, -h * 0.5 - 0.35}, dark));
    } else if (arch == "drone") {
        const double d = std::max(0.25, 0.09 * L);
        o.push_back(P(Shape::Cylinder, Vec3{d, d, 0.85 * L}, Vec3{0.0, 0.0, 0.0}, c, 'x'));
        o.push_back(P(Shape::Cone, Vec3{d, d, 0.15 * L}, Vec3{0.5 * L, 0.0, 0.0}, c, 'x'));
        o.push_back(P(Shape::Box, Vec3{0.2 * L, S, 0.05}, Vec3{0.02 * L, 0.0, 0.0}, c));
        o.push_back(P(Shape::Box, Vec3{0.14 * L, 0.05, 0.3 * L}, Vec3{-0.38 * L, 0.0, 0.15 * L}, c));
    } else if (arch == "missile") {
        const double d = a.diameter;
        o.push_back(P(Shape::Cylinder, Vec3{d, d, 0.85 * L}, Vec3{-0.07 * L, 0.0, 0.0}, c, 'x'));
        o.push_back(P(Shape::Cone, Vec3{d, d, 0.15 * L}, Vec3{0.43 * L, 0.0, 0.0}, c, 'x'));
        o.push_back(P(Shape::Box, Vec3{0.12 * L, 3.0 * d, 0.02}, Vec3{-0.43 * L, 0.0, 0.0}, c));
        o.push_back(P(Shape::Box, Vec3{0.12 * L, 0.02, 3.0 * d}, Vec3{-0.43 * L, 0.0, 0.0}, c));
    } else if (arch == "wheeled" || arch == "tracked") {
        const double W = a.width;
        const bool tracked = arch == "tracked";
        const double hull_h = std::min(H, std::max(1.2, (a.radar != "none" || a.missiles > 0 ? 0.45 : 0.6) * H));
        const double clear = tracked ? 0.45 : 0.55;
        o.push_back(P(Shape::Box, Vec3{L, W * (tracked ? 0.8 : 0.92), hull_h - clear}, Vec3{0.0, 0.0, clear + (hull_h - clear) * 0.5}, c));
        if (tracked) {
            for (double side : {-1.0, 1.0}) o.push_back(P(Shape::Box, Vec3{L * 0.95, W * 0.18, 0.9}, Vec3{0.0, side * W * 0.41, 0.45}, dark));
        } else {
            Wheels(o, L, W, std::min(1.3, std::max(0.7, H * 0.28)), a.axles, dark);
        }
        if (a.cab) o.push_back(P(Shape::Box, Vec3{L * 0.55, W * 0.92, H - hull_h}, Vec3{-0.1 * L, 0.0, hull_h + (H - hull_h) * 0.5}, c));
        double top = hull_h;
        if (a.turret) {
            const double th = std::max(0.6, std::min(1.4, 0.3 * H));
            o.push_back(P(Shape::Box, Vec3{0.38 * L, 0.6 * W, th}, Vec3{0.05 * L, 0.0, hull_h + th * 0.5}, c));
            top = hull_h + th;
            if (a.gun && a.barrels <= 1) {
                o.push_back(P(Shape::Cylinder, Vec3{0.18, 0.18, 0.55 * L}, Vec3{0.05 * L + 0.42 * L, 0.0, hull_h + th * 0.5}, dark, 'x'));
            } else if (a.gun) {
                // Several shorter barrels side by side (the Shilka's four).
                for (int b = 0; b < a.barrels; ++b) {
                    const double y = (-0.5 + b / static_cast<double>(a.barrels - 1)) * 0.4 * W;
                    o.push_back(P(Shape::Cylinder, Vec3{0.1, 0.1, 0.35 * L}, Vec3{0.05 * L + 0.19 * L + 0.175 * L, y, hull_h + th * 0.6}, dark, 'x'));
                }
            }
        }
        if (a.missiles > 0) {
            const int per_side = std::max(1, a.missiles / 2);
            for (int i = 0; i < a.missiles; ++i) {
                const double side = a.missiles == 1 ? 0.0 : (i % 2 == 0 ? -1.0 : 1.0);
                const int k = i / 2;
                const double y = side * (0.3 * W + 0.25 * (k % per_side));
                const double ml = std::min(3.2, 0.4 * L);
                o.push_back(P(Shape::Cylinder, Vec3{0.24, 0.24, ml}, Vec3{0.0, y, top + 0.3 + 0.32 * (k % per_side)}, a.colour2.empty() ? "#dedad0" : c2, 'x',
                              25.0));
            }
        }
        if (a.radar == "dish") {
            // diameter_m: the dish's own diameter where the data gives one.
            const double dish = a.diameter > 0.0 ? a.diameter : std::min(3.0, 0.9 * W);
            o.push_back(P(Shape::Cylinder, Vec3{0.25, 0.25, H - top}, Vec3{-0.1 * L, 0.0, top + (H - top) * 0.5}, dark));
            o.push_back(P(Shape::Cylinder, Vec3{dish, dish, 0.25}, Vec3{-0.1 * L, 0.0, H - 0.6}, c, 'x', 15.0));
        } else if (a.radar == "bar") {
            // span_m and antenna_h_m: the array's own width and height where the data gives them.
            const double bw = a.span > 0.0 ? a.span : std::max(2.5, 1.4 * W), bh = a.antenna_h > 0.0 ? a.antenna_h : 1.0;
            o.push_back(P(Shape::Cylinder, Vec3{0.25, 0.25, H - top}, Vec3{0.0, 0.0, top + (H - top) * 0.5}, dark));
            o.push_back(P(Shape::Box, Vec3{0.5, bw, bh}, Vec3{0.0, 0.0, H - bh * 0.5}, c));
        }
        if (a.mast) o.push_back(P(Shape::Cylinder, Vec3{0.18, 0.18, 2.6 * H}, Vec3{-0.35 * L, 0.0, H + 1.3 * H}, dark));
    } else if (arch == "truck") {
        const double W = a.width;
        const double cab_l = std::min(2.4, 0.3 * L);
        const double bed_h = std::min(H, 2.9);
        o.push_back(P(Shape::Box, Vec3{cab_l, W, 1.6}, Vec3{0.5 * L - cab_l * 0.5, 0.0, 1.0 + 0.8}, c));
        if (a.body == "tank") {
            o.push_back(P(Shape::Cylinder, Vec3{W * 0.9, 1.6, L - cab_l - 0.4}, Vec3{-cab_l * 0.5, 0.0, 1.0 + 0.8}, c, 'x'));
        } else if (a.missiles > 0) {
            // A missile transporter: a low bed and the missiles lying on it side by side (diameter_m: a missile's).
            const double bed_l = L - cab_l - 0.4;
            const double d = a.diameter > 0.0 ? a.diameter : 0.5;
            o.push_back(P(Shape::Box, Vec3{bed_l, W, 0.4}, Vec3{-cab_l * 0.5, 0.0, 1.2}, c));
            for (int i = 0; i < a.missiles; ++i) {
                const double y = a.missiles == 1 ? 0.0 : (-0.5 + i / static_cast<double>(a.missiles - 1)) * (W - d);
                o.push_back(P(Shape::Cylinder, Vec3{d, d, a.missile > 0.0 ? a.missile : bed_l}, Vec3{-cab_l * 0.5, y, 1.4 + d * 0.5}, "#dedad0", 'x'));
            }
        } else {
            o.push_back(P(Shape::Box, Vec3{L - cab_l - 0.4, W, bed_h - 1.0}, Vec3{-cab_l * 0.5, 0.0, 1.0 + (bed_h - 1.0) * 0.5}, a.body == "box" ? c : c2));
        }
        Wheels(o, L, W, 1.0, 3, dark);
        if (a.radar == "bar") {
            // span_m and antenna_h_m: the array's own width and height where the data gives them (the P-19's 7 by 1.6 m).
            const double bw = a.span > 0.0 ? a.span : 7.0, bh = a.antenna_h > 0.0 ? a.antenna_h : 1.6;
            o.push_back(P(Shape::Cylinder, Vec3{0.25, 0.25, H - bed_h}, Vec3{-0.15 * L, 0.0, bed_h + (H - bed_h) * 0.5}, dark));
            o.push_back(P(Shape::Box, Vec3{0.6, bw, bh}, Vec3{-0.15 * L, 0.0, H - bh * 0.5}, c));
        }
    } else if (arch == "launcher") {
        const double W = a.width;
        o.push_back(P(Shape::Box, Vec3{0.6 * L, W, 1.0}, Vec3{0.0, 0.0, 0.5}, dark));
        o.push_back(P(Shape::Cylinder, Vec3{2.0, 2.0, 0.8}, Vec3{0.0, 0.0, 1.4}, c));
        for (int r = 0; r < a.rails; ++r) {
            const double y = a.rails == 1 ? 0.0 : (-0.5 + r / static_cast<double>(a.rails - 1)) * 1.6;
            o.push_back(P(Shape::Cylinder, Vec3{0.5, 0.5, a.missile}, Vec3{0.0, y, 1.8 + 0.25 * a.missile * std::sin(a.elevation * 3.141592653589793 / 180.0)},
                          "#dedad0", 'x', a.elevation));
        }
    } else if (arch == "radar") {
        // A radar post: its equipment cabin, the post the antenna turns on, and the antenna (span_m wide, antenna_h_m
        // tall) at the top: the P-14's great array, a height finder's tall narrow one.
        const double W = a.width;
        const double cab_h = std::min(3.0, 0.25 * H);
        const double ah = a.antenna_h > 0.0 ? a.antenna_h : 0.3 * S;
        const double post = std::max(0.5, H - ah - cab_h);
        o.push_back(P(Shape::Box, Vec3{L, W, cab_h}, Vec3{0.0, 0.0, cab_h * 0.5}, c));
        o.push_back(P(Shape::Cylinder, Vec3{0.6, 0.6, post}, Vec3{0.0, 0.0, cab_h + post * 0.5}, dark));
        o.push_back(P(Shape::Box, Vec3{0.6, S, ah}, Vec3{0.0, 0.0, cab_h + post + ah * 0.5}, c2));
    } else if (arch == "gun") {
        o.push_back(P(Shape::Box, Vec3{0.6 * L, a.width, 0.7}, Vec3{0.0, 0.0, 0.35}, c));
        o.push_back(P(Shape::Box, Vec3{0.8, 1.2, 0.9}, Vec3{0.0, 0.0, 1.15}, c));
        for (int b = 0; b < a.barrels; ++b) {
            const double y = a.barrels == 1 ? 0.0 : (b == 0 ? -0.25 : 0.25);
            o.push_back(P(Shape::Cylinder, Vec3{0.1, 0.1, 0.6 * L}, Vec3{0.25 * L, y, 1.5}, dark, 'x', 35.0));
        }
    } else if (arch == "building") {
        const double W = a.width;
        const double wall = a.roof == "pitched" ? H : H;
        o.push_back(P(Shape::Box, Vec3{L, W, wall}, Vec3{0.0, 0.0, wall * 0.5}, c));
        if (a.roof == "pitched") {
            Roof(o, L, W, wall, c2);
        } else {
            o.push_back(P(Shape::Box, Vec3{L * 1.01, W * 1.01, 0.4}, Vec3{0.0, 0.0, wall + 0.2}, c2));
        }
        if (a.tower) {
            o.push_back(P(Shape::Box, Vec3{4.0, 4.0, H * 0.9}, Vec3{0.0, 0.0, H + H * 0.45}, c));
            o.push_back(P(Shape::Cone, Vec3{4.4, 4.4, 4.0}, Vec3{0.0, 0.0, H * 1.9 + 2.0}, c2));
        }
    } else if (arch == "church") {
        const double W = a.width;
        o.push_back(P(Shape::Box, Vec3{L, W, H}, Vec3{-0.1 * L, 0.0, H * 0.5}, c));
        Roof(o, L, W, H, c2);
        const double t = 0.3 * W;
        o.push_back(P(Shape::Box, Vec3{t * 1.3, t * 1.3, 2.4 * H}, Vec3{0.45 * L, 0.0, 1.2 * H}, c));
        o.push_back(P(Shape::Cone, Vec3{t * 1.5, t * 1.5, 1.2 * H}, Vec3{0.45 * L, 0.0, 2.4 * H + 0.6 * H}, c2));
    } else if (arch == "tower") {
        o.push_back(P(Shape::Cylinder, Vec3{a.diameter, a.diameter, H}, Vec3{0.0, 0.0, H * 0.5}, c));
    } else if (arch == "crates") {
        o.push_back(P(Shape::Box, Vec3{L, a.width, H}, Vec3{0.0, 0.0, H * 0.5}, c));
        o.push_back(P(Shape::Box, Vec3{L * 0.6, a.width * 0.9, H * 0.6}, Vec3{-0.1 * L, 0.0, H * 1.3}, c));
    } else if (arch == "tent") {
        o.push_back(P(Shape::Box, Vec3{L, a.width, H * 0.45}, Vec3{0.0, 0.0, H * 0.225}, c));
        Roof(o, L, a.width, H * 0.45, c);
    } else if (arch == "mast") {
        o.push_back(P(Shape::Cylinder, Vec3{0.3, 0.3, H}, Vec3{0.0, 0.0, H * 0.5}, c));
        o.push_back(P(Shape::Box, Vec3{0.2, 3.0, 0.2}, Vec3{0.0, 0.0, H - 0.5}, c));
    } else if (arch == "tree") {
        const double trunk = a.crown == "cone" ? 0.25 * H : 0.4 * H;
        o.push_back(P(Shape::Cylinder, Vec3{0.45, 0.45, trunk + 0.5}, Vec3{0.0, 0.0, (trunk + 0.5) * 0.5}, c2));
        if (a.crown == "cone") {
            o.push_back(P(Shape::Cone, Vec3{a.diameter, a.diameter, H - trunk}, Vec3{0.0, 0.0, trunk + (H - trunk) * 0.5}, c));
        } else {
            o.push_back(P(Shape::Sphere, Vec3{a.diameter, a.diameter, H - trunk}, Vec3{0.0, 0.0, trunk + (H - trunk) * 0.5}, c));
        }
    } else if (arch == "bridge") {
        const double W = a.width;
        o.push_back(P(Shape::Box, Vec3{L, W, 1.4}, Vec3{0.0, 0.0, H}, c));
        for (double side : {-1.0, 1.0}) o.push_back(P(Shape::Box, Vec3{L, 0.3, 1.0}, Vec3{0.0, side * W * 0.5, H + 1.2}, c));
        const int piers = std::max(2, static_cast<int>(L / 60.0));
        for (int i = 0; i <= piers; ++i) {
            const double x = (-0.5 + i / static_cast<double>(piers)) * L;
            o.push_back(P(Shape::Box, Vec3{2.5, W * 0.8, H}, Vec3{x, 0.0, H * 0.5}, c));
        }
    } else {
        ok = false;
    }
    return o;
}

class ModelLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, ModelLibrary& out) {
        if (!Keys(root, "", {"models_id", "display_name", "status", "source", "confidence", "notes", "colours", "fallbacks", "ground", "models"})) {
            return false;
        }
        if (!Map(root, "colours", out.colours) || !Map(root, "fallbacks", out.fallbacks) || !Map(root, "ground", out.ground)) return false;
        for (const auto& kv : out.colours) {
            if (!IsHex(kv.second)) return Fail(Join("colours", kv.first), "expected #rrggbb");
        }
        const json::Value* arr = OptionalArray(root, "", "models");
        if (arr == nullptr || arr->items.empty()) return message_.empty() ? Fail("models", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& v = arr->items[i];
            const std::string p = Index("models", i);
            ModelDef m;
            Params a;
            if (!Keys(v, p, {"id", "name", "archetype", "length_m", "span_m", "width_m", "height_m", "diameter_m", "rotor_m", "missile_m",
                             "antenna_h_m", "elevation_deg", "engines", "axles", "missiles", "rails", "barrels", "biplane", "high_wing", "wings", "turret",
                             "mast", "cab", "gun", "tower", "wing", "tail", "radar", "body", "roof", "crown", "colour", "colour2", "notes"}) ||
                !RequireString(v, p, "id", m.id) || !RequireString(v, p, "name", m.name) || !RequireString(v, p, "archetype", m.archetype) ||
                !OptionalNumber(v, p, "length_m", a.length) || !OptionalNumber(v, p, "span_m", a.span) || !OptionalNumber(v, p, "width_m", a.width) ||
                !OptionalNumber(v, p, "height_m", a.height) || !OptionalNumber(v, p, "diameter_m", a.diameter) ||
                !OptionalNumber(v, p, "rotor_m", a.rotor) || !OptionalNumber(v, p, "missile_m", a.missile) ||
                !OptionalNumber(v, p, "antenna_h_m", a.antenna_h) ||
                !OptionalNumber(v, p, "elevation_deg", a.elevation) || !OptionalInt(v, p, "engines", a.engines) ||
                !OptionalInt(v, p, "axles", a.axles) || !OptionalInt(v, p, "missiles", a.missiles) || !OptionalInt(v, p, "rails", a.rails) ||
                !OptionalInt(v, p, "barrels", a.barrels) || !OptionalBool(v, p, "biplane", a.biplane) ||
                !OptionalBool(v, p, "high_wing", a.high_wing) || !OptionalBool(v, p, "wings", a.wings) || !OptionalBool(v, p, "turret", a.turret) ||
                !OptionalBool(v, p, "mast", a.mast) || !OptionalBool(v, p, "cab", a.cab) || !OptionalBool(v, p, "gun", a.gun) ||
                !OptionalBool(v, p, "tower", a.tower) || !OptionalString(v, p, "wing", a.wing) || !OptionalString(v, p, "tail", a.tail) ||
                !OptionalString(v, p, "radar", a.radar) || !OptionalString(v, p, "body", a.body) || !OptionalString(v, p, "roof", a.roof) ||
                !OptionalString(v, p, "crown", a.crown) || !RequireString(v, p, "colour", a.colour) || !OptionalString(v, p, "colour2", a.colour2)) {
                return false;
            }
            if (!Colour(out, a.colour, Join(p, "colour")) || (!a.colour2.empty() && !Colour(out, a.colour2, Join(p, "colour2")))) return false;
            bool ok = true;
            const std::string dark = Resolve(out, "steel_dark").empty() ? "#3b3d3f" : Resolve(out, "steel_dark");
            m.parts = Expand(m.archetype, a, dark, ok);
            if (!ok) return Fail(Join(p, "archetype"), "unknown archetype '" + m.archetype + "'");
            for (const Part& part : m.parts) {
                if (!(part.size.x > 0.0 && part.size.y > 0.0 && part.size.z > 0.0)) return Fail(p, "a part has no size: check the dimensions");
            }
            m.length_m = a.length;
            m.span_m = a.span > 0.0 ? a.span : (a.rotor > 0.0 ? a.rotor : a.width);
            m.height_m = a.height;
            for (const ModelDef& q : out.models) {
                if (q.id == m.id) return Fail(Join(p, "id"), "duplicate model");
            }
            out.models.push_back(m);
        }
        for (const auto& kv : out.fallbacks) {
            if (out.Find(kv.second) == nullptr) return Fail(Join("fallbacks", kv.first), "no such model: " + kv.second);
        }
        for (const auto& kv : out.ground) {
            if (out.Find(kv.second) == nullptr) return Fail(Join("ground", kv.first), "no such model: " + kv.second);
        }
        return true;
    }

private:
    static bool IsHex(const std::string& s) {
        if (s.size() != 7 || s[0] != '#') return false;
        for (std::size_t i = 1; i < 7; ++i) {
            const char ch = s[i];
            if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) return false;
        }
        return true;
    }
    static std::string Resolve(const ModelLibrary& lib, const std::string& name) {
        if (IsHex(name)) return name;
        const auto it = lib.colours.find(name);
        return it == lib.colours.end() ? std::string() : it->second;
    }
    // A colour name or #rrggbb, resolved in place.
    bool Colour(const ModelLibrary& lib, std::string& c, const std::string& path) {
        const std::string r = Resolve(lib, c);
        if (r.empty()) return Fail(path, "no such colour: " + c);
        c = r;
        return true;
    }
    bool Map(const json::Value& root, const char* key, std::map<std::string, std::string>& out) {
        const json::Value* v = root.Find(key);
        if (v == nullptr) return true;
        if (!v->Is(json::Type::Object)) return Fail(key, "expected {name: value}");
        for (std::size_t k = 0; k < v->keys.size(); ++k) {
            if (!v->items[k].Is(json::Type::String)) return Fail(Join(key, v->keys[k]), "expected a string");
            out[v->keys[k]] = v->items[k].string;
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadModelLibraryJson(const std::string& text, ModelLibrary& out, std::string* error = nullptr) {
    json::Value root;
    std::string perr;
    if (!json::Parse(text, root, &perr)) {
        if (error != nullptr) *error = perr;
        return false;
    }
    detail::ModelLoader l;
    ModelLibrary lib;
    if (!l.Load(root, lib)) {
        if (error != nullptr) *error = l.Message();
        return false;
    }
    out = std::move(lib);
    return true;
}

inline bool LoadModelLibrary(const std::string& path, ModelLibrary& out, std::string* error = nullptr) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream s;
    s << f.rdbuf();
    std::string e;
    if (!LoadModelLibraryJson(s.str(), out, &e)) {
        if (error != nullptr) *error = path + ": " + e;
        return false;
    }
    return true;
}

}  // namespace scenery
}  // namespace simcore
