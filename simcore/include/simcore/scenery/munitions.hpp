// simcore/scenery/munitions.hpp — every munition's shape as data, and a triangle mesh built from it (docs/fx/README.md
// section 3.2; data/scenery/munitions.json; docs/scenery/README.md "Munitions").
//
// Bombs, cluster dispensers, retarded and parachute bombs, guided and glide bombs, rockets, air-to-ground missiles, our
// 9M33 and the neighbours' SAMs, the anti-radiation and cruise missiles, and the guns' rounds. Each is a body of
// revolution (a nose, a cylinder whose diameter may step, a tail) with fins, wings and canards as thin plates, ring tails
// as short open cylinders and its paint bands; with it the motor (boost, sustain, smoke, a tracer for an optical sight),
// the retarder (Snakeye blades, a ballute, a chute), a dispenser's opening, and for guns and shells the round's ballistics
// and tracer. Presentation data, engine-agnostic: Unreal turns the meshes into its own (FSimMunitionModels) and the
// effects read the motor, smoke and charge. Nothing in the simulation reads it.
//
// The mesh's frame: x forward along the body, y right, z up, metres; the origin at the body's middle (the nose at
// x = +length/2). Every triangle winds so that (b - a) x (c - a), the cross product of the coordinates taken as numbers,
// points out of the surface (along its vertices' normals). An engine whose front faces wind the other way swaps two
// indices of each triangle. Smooth normals round the body, its own normal for each flat face (fins, discs, steps).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/json.hpp"
#include "simcore/scenery/trimesh.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace scenery {

// A set of fins, wings or canards: `count` thin plates evenly round the body. Positions along the body are fractions of
// its length from the nose.
struct MunitionFins {
    int count = 4;
    double span_m = 0.0;        // twice a tip's distance from the axis (tip to tip for two opposite fins)
    double root_chord_m = 0.0;
    double tip_chord_m = 0.0;   // 0: a delta's point
    double x = 0.0;             // the root's leading edge
    double sweep_deg = 0.0;     // the leading edge's sweep back from square to the body
    double roll_deg = 0.0;      // the first fin's angle from right (+y) toward up (+z); the rest evenly round
    bool folds = false;         // folds for carriage (BuildPart "wings_folded"): adjacent fins fold toward each other
    double fold_at = 0.5;       // the hinge, a fraction of the exposed span from the root (0: the whole fin folds)
    bool blades = false;        // the retarder's blades, drawn closed (BuildPart "blades_open" opens them)
    std::string colour;         // "#rrggbb"; empty: the body's
};

// A short open cylinder round the body (the FAB's ring tails, the M-54's nose ring), seen from outside and inside.
struct MunitionRing {
    double x = 0.0;             // its front, a fraction of the length from the nose
    double w = 0.0;             // its length, a fraction of the munition's
    double diameter_m = 0.0;
    std::string colour;         // empty: the body's
};

// A painted ring on the body (US: yellow for high explosive, brown for a rocket motor; MIL-STD-709C).
struct MunitionBand {
    double x = 0.0;
    double w = 0.0;
    std::string colour;
};

// From `x` aft the body has this diameter (a booster wider than its sustainer).
struct MunitionSection {
    double x = 0.0;
    double diameter_m = 0.0;
};

struct MunitionRetarder {
    std::string type;           // "blades" (Snakeye), "ballute" (AIR), "chute"
    int count = 4;              // blades, or the lines to a ballute or chute
    double length_m = 0.0;      // a blade's length; how far behind the tail a ballute or chute flies (its lines)
    double width_m = 0.0;       // a blade's width
    double open_deg = 90.0;     // a blade's angle from the body's forward axis once open (90: straight out)
    double diameter_m = 0.0;    // a ballute's; a chute's open canopy as drawn (else from its area)
    double area_m2 = 0.0;       // a chute's nominal area: its canopy's cloth, as the sources give it (FAB-500ShN: 6 m2)
    double open_s = 0.0;        // from release to fully open
    double descent_mps = 0.0;   // a ballute's or chute's steady descent under it (0: not given), for the effects'
                                // fx::BombPathParams::chute_descent_mps
    std::string colour;         // empty: a ballute's khaki, a chute's off-white
};

// An open canopy spreads to about two-thirds of its nominal diameter D0 = sqrt(4 S0 / pi): the inflated (projected)
// diameter Dp / D0 is 0.67-0.70 for flat circular, conical and ribbon canopies (T. W. Knacke, Parachute Recovery Systems
// Design Manual, NWC TP 6575, 1991, chapter 2's tables of canopy types). The FAB-500ShN's 6 m2 chute (D0 2.76 m) is
// drawn 1.85 m across. That 6 m2 is the cloth, not the open disc: the sources' 47 m/s with 513 kg needs C_D S =
// m g / (rho v^2 / 2) = 3.72 m2, a drag coefficient of 0.62 on 6 m2, within the range canopies have on their nominal
// area in the same tables; on 6 m2 of open disc it would be far below what an open canopy gives on its disc.
constexpr double kChuteInflatedShare = 0.67;

// A dispenser's opening and its submunitions' pattern on the ground.
struct MunitionCluster {
    double opens_agl_m = 0.0;   // the height it opens at (0: at a time, or dispensing from the aircraft: the JP233)
    double opens_s = 0.0;       // or this long after release (0: by height)
    int submunitions = 0;
    double pattern_w_m = 0.0;   // the pattern across the track
    double pattern_l_m = 0.0;   // and along it
};

struct MunitionMotor {
    double boost_s = 0.0;
    double sustain_s = 0.0;
    std::string smoke = "none";     // the trail while it burns: dense, reduced, minimal, none
    std::string sustain_smoke;      // the sustainer's when not the boost's (a liquid or ramjet sustainer); empty: smoke's
    std::string plume_colour;       // "#rrggbb"
    bool tracer = false;            // a tracer or lamp in the tail for an optical sight (the 9M33's for the TV, an ATGM's)
    double Burn() const { return boost_s + sustain_s; }
    const std::string& SustainSmoke() const { return sustain_smoke.empty() ? smoke : sustain_smoke; }
};

struct MunitionSpec {
    std::string id;
    std::string name;
    std::string kind;           // bomb, retarded, ballute, parachute, cluster, guided, glide, rocket, missile, sam, arm,
                                // cruise, shell (a gun's round: a mesh and ballistics), gun (an aircraft's: ballistics only)
    double length_m = 0.0;
    double diameter_m = 0.0;    // the body's from the nose (`sections` change it further aft)
    double mass_kg = 0.0;
    double charge_kg = 0.0;     // the warhead's TNT equivalent; a cluster's: one submunition's
    std::string nose = "ogive";     // ogive, conical, hemisphere, blunt (a flat face of half the radius), flat
    double nose_l = 0.0;            // its length, a fraction of the munition's
    std::string tail = "flat";      // boattail (tapers to tail_r), cone (to a point), flat
    double tail_l = 0.0;            // its length, a fraction of the munition's
    double tail_r = 0.55;           // a boattail's end radius, a fraction of the body's
    std::vector<MunitionSection> sections;
    std::vector<MunitionFins> fins;
    std::vector<MunitionRing> rings;
    std::vector<MunitionBand> bands;
    std::string colour;         // "#rrggbb", resolved from the file's named colours
    bool has_retarder = false;
    MunitionRetarder retarder;
    bool has_cluster = false;
    MunitionCluster cluster;
    bool has_motor = false;
    MunitionMotor motor;
    double smoke_s = 0.0;       // how long its trail lingers (game value)
    // A gun's or shell's round.
    double calibre_mm = 0.0;
    double muzzle_mps = 0.0;
    double rate_rpm = 0.0;      // rounds a minute from the whole mount (all its barrels)
    int barrels = 1;            // separate streams of rounds (a rotary gun fires one)
    double tracer_s = -1.0;     // how long its tracer burns (0: no tracer; -1: not given)
    double sd_lo_s = 0.0;       // its self-destruct window after firing (both 0: none)
    double sd_hi_s = 0.0;
    std::string tracer_colour;  // "#rrggbb"
    std::string burst = "contact";  // contact or time (a time fuze: the KS-30's)
    std::string source;
    std::string confidence;
    std::string notes;

    bool IsRound() const { return kind == "gun" || kind == "shell"; }
    bool HasMesh() const { return kind != "gun" && length_m > 0.0 && diameter_m > 0.0; }
};

struct MunitionLibrary {
    std::vector<MunitionSpec> munitions;
    std::map<std::string, std::string> weapons;   // data/air_war/weapons.json id -> munition
    std::map<std::string, std::string> systems;   // air defence system id (air_war/data.hpp) -> the munition it fires
    std::map<std::string, std::string> models;    // basic model id (basic_models.json) -> munition
    std::map<std::string, std::string> aliases;   // other names: the EM library's ARMs, "9m33"
    std::map<std::string, std::string> colours;   // name -> #rrggbb

    const MunitionSpec* Find(const std::string& id) const {
        for (const MunitionSpec& m : munitions) {
            if (m.id == id) return &m;
        }
        return nullptr;
    }
    // A munition by its own id, else a weapon's, a system's, a basic model's or an alias; nullptr if none.
    const MunitionSpec* Resolve(const std::string& any_id) const {
        if (const MunitionSpec* m = Find(any_id)) return m;
        for (const std::map<std::string, std::string>* table : {&weapons, &systems, &models, &aliases}) {
            const auto it = table->find(any_id);
            if (it != table->end()) return Find(it->second);
        }
        return nullptr;
    }
    std::vector<std::string> Ids() const {
        std::vector<std::string> out;
        for (const MunitionSpec& m : munitions) out.push_back(m.id);
        return out;
    }
    // A file's path, or the JSON text itself (it starts with '{'). False with the reason in `error`; the library is
    // left as it was.
    bool Load(const std::string& path_or_json, std::string* error = nullptr);
    bool LoadJson(const std::string& text, std::string* error = nullptr);
};

// "#rrggbb" (sRGB) as linear RGB in 0..1; mid grey for anything else.
inline std::array<float, 3> LinearColour(const std::string& hex) {
    std::array<float, 3> out = {0.2158605f, 0.2158605f, 0.2158605f};
    if (hex.size() != 7 || hex[0] != '#') return out;
    for (int c = 0; c < 3; ++c) {
        int v = 0;
        for (int k = 1; k <= 2; ++k) {
            const char ch = hex[static_cast<std::size_t>(2 * c + k)];
            const int d = (ch >= '0' && ch <= '9') ? ch - '0' : (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10 : (ch >= 'A' && ch <= 'F') ? ch - 'A' + 10 : -1;
            if (d < 0) return {0.2158605f, 0.2158605f, 0.2158605f};
            v = v * 16 + d;
        }
        const double s = v / 255.0;
        out[static_cast<std::size_t>(c)] = static_cast<float>(s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4));
    }
    return out;
}

// Sides round the body at each level of detail: 24, 12, 6, and 4 (a box, no fins, rings or bands).
inline int MunitionSides(int lod) { return lod <= 0 ? 24 : lod == 1 ? 12 : lod == 2 ? 6 : 4; }

namespace munitions_detail {

constexpr double kTiny = 1e-9;
constexpr double kRootSink = 0.85;  // a fin's root sits inside the body (inside its flat faces even at six sides)

inline double NoseLength(const MunitionSpec& m) { return m.nose == "flat" ? 0.0 : m.nose_l * m.length_m; }
inline double TailLength(const MunitionSpec& m) { return m.tail == "flat" ? 0.0 : m.tail_l * m.length_m; }

// The body's radius between nose and tail, with the steps of its sections.
inline double MidRadius(const MunitionSpec& m, double s) {
    double r = 0.5 * m.diameter_m;
    for (const MunitionSection& sec : m.sections) {
        if (s >= sec.x * m.length_m) r = 0.5 * sec.diameter_m;
    }
    return r;
}

inline double TailEndRadius(const MunitionSpec& m) {
    const double rr = MidRadius(m, m.length_m - TailLength(m));
    if (m.tail == "cone") return 0.0;
    if (m.tail == "boattail" && TailLength(m) > 0.0) return m.tail_r * rr;
    return rr;
}

// The nose's radius r and slope dr/ds `s` metres aft of its tip, for a nose `n` long meeting a body of radius `rr`. An
// ogive is the tangent ogive (a circular arc); one shorter than the radius, and a hemisphere, are a quarter ellipse; a
// blunt nose has a flat face of half the radius and swells to the body along a quarter sine.
inline void NoseProfile(const std::string& kind, double s, double n, double rr, double& r, double& dr) {
    if (kind == "conical") {
        r = rr * s / n;
        dr = rr / n;
    } else if (kind == "blunt") {
        const double rb = 0.5 * rr, k = 0.5 * kPi / n;
        r = rb + (rr - rb) * std::sin(k * s);
        dr = (rr - rb) * k * std::cos(k * s);
    } else if (kind == "ogive" && n >= rr) {
        const double rho = (rr * rr + n * n) / (2.0 * rr), q = n - s;
        const double root = std::sqrt(std::max(rho * rho - q * q, 0.0));
        r = root + rr - rho;
        dr = root > kTiny ? q / root : 1e9;
    } else {  // hemisphere: (s - n)^2 / n^2 + r^2 / rr^2 = 1
        const double q = (n - s) / n, root = std::sqrt(std::max(1.0 - q * q, 0.0));
        r = rr * root;
        dr = root > kTiny ? rr * q / (n * root) : 1e9;
    }
}

}  // namespace munitions_detail

// The body's radius `s_m` metres aft of the nose's tip (0 outside the body), nose and tail profiles included.
inline double BodyRadius(const MunitionSpec& m, double s_m) {
    using namespace munitions_detail;
    const double len = m.length_m, n = NoseLength(m), t = TailLength(m);
    if (s_m < 0.0 || s_m > len) return 0.0;
    if (n > 0.0 && s_m < n) {
        double r = 0.0, dr = 0.0;
        NoseProfile(m.nose, s_m, n, MidRadius(m, n), r, dr);
        return r;
    }
    if (t > 0.0 && s_m > len - t) {
        const double rr = MidRadius(m, len - t), re = TailEndRadius(m);
        return rr + (re - rr) * (s_m - (len - t)) / t;
    }
    return MidRadius(m, s_m);
}

// The farthest any part of the munition reaches from its axis: body, fins and rings (retarder parts aside).
inline double MaxRadius(const MunitionSpec& m) {
    double r = 0.5 * m.diameter_m;
    for (const MunitionSection& s : m.sections) r = std::max(r, 0.5 * s.diameter_m);
    for (const MunitionFins& f : m.fins) r = std::max(r, 0.5 * f.span_m);
    for (const MunitionRing& g : m.rings) r = std::max(r, 0.5 * g.diameter_m);
    return r;
}

// The body's widest radius (sections included): what a level-3 box is as wide as.
inline double MaxBodyRadius(const MunitionSpec& m) {
    double r = 0.5 * m.diameter_m;
    for (const MunitionSection& s : m.sections) r = std::max(r, 0.5 * s.diameter_m);
    return r;
}

namespace munitions_detail {

// A point of the outline in the body's half-plane: `s` metres aft of the nose, radius `r`, and the outward normal
// (ns aft, nr out), a unit vector.
struct LathePoint {
    double s = 0.0;
    double r = 0.0;
    double ns = 0.0;
    double nr = 1.0;
};
using LatheRun = std::vector<LathePoint>;  // a piece of the outline shading smoothly; pieces meet at creases

inline LathePoint Pt(double s, double r, double ns, double nr) {
    const double l = std::sqrt(ns * ns + nr * nr);
    return l > kTiny ? LathePoint{s, r, ns / l, nr / l} : LathePoint{s, r, 0.0, 1.0};
}

// A straight piece from (s0, r0) to (s1, r1), walking from the nose aft: its normal is square to it, outward.
inline LatheRun Straight(double s0, double r0, double s1, double r1) {
    const double ns = -(r1 - r0), nr = s1 - s0;
    return {Pt(s0, r0, ns, nr), Pt(s1, r1, ns, nr)};
}

inline LathePoint Lerp(const LathePoint& a, const LathePoint& b, double f) {
    return Pt(a.s + (b.s - a.s) * f, a.r + (b.r - a.r) * f, a.ns + (b.ns - a.ns) * f, a.nr + (b.nr - a.nr) * f);
}

// One triangle, wound so its face normal agrees with its vertices' normals; a triangle with no area is left out.
inline void AddTri(TriMesh& m, std::uint32_t i0, std::uint32_t i1, std::uint32_t i2) {
    const Vec3 f = Cross(m.positions[i1] - m.positions[i0], m.positions[i2] - m.positions[i0]);
    if (f.LengthSquared() < 1e-26) return;
    const Vec3 n = m.normals[i0] + m.normals[i1] + m.normals[i2];
    m.indices.push_back(i0);
    if (Dot(f, n) < 0.0) {
        m.indices.push_back(i2);
        m.indices.push_back(i1);
    } else {
        m.indices.push_back(i1);
        m.indices.push_back(i2);
    }
}

inline std::uint32_t AddVertex(TriMesh& m, const Vec3& p, const Vec3& n, const std::array<float, 3>& c) {
    m.positions.push_back(p);
    m.normals.push_back(n);
    m.colours.push_back(c);
    return static_cast<std::uint32_t>(m.positions.size() - 1);
}

// Where the lathe puts its sides: turned by `turn0`, the radius scaled by `scale` (a box's corners).
struct LatheFrame {
    double half = 0.0;   // half the munition's length: x = half - s
    int sides = 24;
    double turn0 = 0.0;
    double scale = 1.0;
};

// A band of the surface of revolution between two outline points, with its own two rings of vertices.
inline void LatheSegment(TriMesh& m, const LathePoint& a, const LathePoint& b, const LatheFrame& fr, const std::array<float, 3>& c) {
    if (std::fabs(a.s - b.s) < kTiny && std::fabs(a.r - b.r) < kTiny) return;
    const std::uint32_t base = static_cast<std::uint32_t>(m.positions.size());
    for (const LathePoint* p : {&a, &b}) {
        for (int k = 0; k < fr.sides; ++k) {
            const double th = fr.turn0 + kTwoPi * k / fr.sides, ct = std::cos(th), st = std::sin(th);
            AddVertex(m, Vec3{fr.half - p->s, p->r * fr.scale * ct, p->r * fr.scale * st}, Vec3{-p->ns, p->nr * ct, p->nr * st}, c);
        }
    }
    const std::uint32_t n = static_cast<std::uint32_t>(fr.sides);
    for (std::uint32_t k = 0; k < n; ++k) {
        const std::uint32_t k1 = (k + 1) % n, a0 = base + k, a1 = base + k1, b0 = base + n + k, b1 = base + n + k1;
        if (a.r < kTiny) {
            AddTri(m, a0, b0, b1);  // an apex
        } else if (b.r < kTiny) {
            AddTri(m, a0, a1, b0);
        } else {
            AddTri(m, a0, b0, b1);
            AddTri(m, a0, b1, a1);
        }
    }
}

// The body's outline from the nose's tip to the tail's: the nose (curved pieces in `steps`), the cylinder with a step at
// each section, the tail, and the discs closing a flat nose or tail.
inline std::vector<LatheRun> Outline(const MunitionSpec& m, int steps) {
    std::vector<LatheRun> runs;
    const double len = m.length_m, n = NoseLength(m), t = TailLength(m), rn = MidRadius(m, n);
    if (n <= 0.0) {
        runs.push_back(Straight(0.0, 0.0, 0.0, rn));
    } else if (m.nose == "conical") {
        runs.push_back(Straight(0.0, 0.0, n, rn));
    } else if (m.nose == "hemisphere" || (m.nose == "ogive" && n < rn)) {
        LatheRun run;
        for (int j = 0; j <= steps; ++j) {
            const double a = 0.5 * kPi * j / steps;
            run.push_back(Pt(n * (1.0 - std::cos(a)), rn * std::sin(a), -std::cos(a) / n, std::sin(a) / rn));
        }
        runs.push_back(run);
    } else {
        if (m.nose == "blunt") runs.push_back(Straight(0.0, 0.0, 0.0, 0.5 * rn));
        LatheRun run;
        for (int j = 0; j <= steps; ++j) {
            const double s = n * j / steps;
            double r = 0.0, dr = 0.0;
            NoseProfile(m.nose, s, n, rn, r, dr);
            run.push_back(Pt(s, r, -dr, 1.0));
        }
        runs.push_back(run);
    }
    double s0 = n, r0 = rn;
    for (const MunitionSection& sec : m.sections) {
        const double sx = sec.x * len, rx = 0.5 * sec.diameter_m;
        if (sx <= s0) continue;
        runs.push_back(Straight(s0, r0, sx, r0));
        runs.push_back(Straight(sx, r0, sx, rx));
        s0 = sx;
        r0 = rx;
    }
    runs.push_back(Straight(s0, r0, len - t, r0));
    const double re = TailEndRadius(m);
    if (t > 0.0) runs.push_back(Straight(len - t, r0, len, re));
    if (re > 0.0) runs.push_back(Straight(len, re, len, 0.0));
    return runs;
}

inline std::array<float, 3> ColourAt(const MunitionSpec& m, double s, bool bands) {
    std::string c = m.colour;
    if (bands) {
        for (const MunitionBand& b : m.bands) {
            if (s >= b.x * m.length_m && s <= (b.x + b.w) * m.length_m) c = b.colour;
        }
    }
    return LinearColour(c);
}

// Turns the outline into the body: each piece split at the bands' edges, each part coloured by its middle.
inline void Lathe(TriMesh& out, const MunitionSpec& m, const std::vector<LatheRun>& runs, const LatheFrame& fr, bool bands) {
    std::vector<double> cuts;
    if (bands) {
        for (const MunitionBand& b : m.bands) {
            cuts.push_back(b.x * m.length_m);
            cuts.push_back((b.x + b.w) * m.length_m);
        }
        std::sort(cuts.begin(), cuts.end());
    }
    for (const LatheRun& run : runs) {
        for (std::size_t i = 0; i + 1 < run.size(); ++i) {
            const LathePoint& a = run[i];
            const LathePoint& b = run[i + 1];
            std::vector<LathePoint> pts = {a};
            if (b.s - a.s > kTiny) {
                for (const double c : cuts) {
                    if (c > a.s + 1e-6 && c < b.s - 1e-6) pts.push_back(Lerp(a, b, (c - a.s) / (b.s - a.s)));
                }
            }
            pts.push_back(b);
            for (std::size_t j = 0; j + 1 < pts.size(); ++j) {
                LatheSegment(out, pts[j], pts[j + 1], fr, ColourAt(m, 0.5 * (pts[j].s + pts[j + 1].s), bands));
            }
        }
    }
}

// A flat plate `t` thick round its mid-plane outline p[0..3] (in order round it), `w` square to it: both faces and the
// edges, each with its own vertices and normal; the root edge p[0]-p[1] is left out when it is inside the body.
inline void Plate(TriMesh& out, const std::array<Vec3, 4>& p, const Vec3& w, double t, const std::array<float, 3>& c, bool root_hidden) {
    const Vec3 h = w * (0.5 * t);
    const Vec3 centre = (p[0] + p[1] + p[2] + p[3]) * 0.25;
    for (const double side : {1.0, -1.0}) {
        const std::uint32_t base = static_cast<std::uint32_t>(out.positions.size());
        for (const Vec3& q : p) AddVertex(out, q + h * side, w * side, c);
        AddTri(out, base, base + 1, base + 2);
        AddTri(out, base, base + 2, base + 3);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        if (i == 0 && root_hidden) continue;
        const Vec3& a = p[i];
        const Vec3& b = p[(i + 1) % 4];
        if ((b - a).Length() < kTiny) continue;
        const Vec3 e = (b - a).Normalized();
        const Vec3 d = (a + b) * 0.5 - centre;
        Vec3 n = d - e * Dot(d, e) - w * Dot(d, w);
        if (n.Length() < kTiny) continue;
        n = n.Normalized();
        const std::uint32_t base = static_cast<std::uint32_t>(out.positions.size());
        AddVertex(out, a + h, n, c);
        AddVertex(out, b + h, n, c);
        AddVertex(out, b - h, n, c);
        AddVertex(out, a - h, n, c);
        AddTri(out, base, base + 1, base + 2);
        AddTri(out, base, base + 2, base + 3);
    }
}

inline Vec3 Radial(double deg) { return Vec3{0.0, std::cos(DegToRad(deg)), std::sin(DegToRad(deg))}; }
inline Vec3 Around(double deg) { return Vec3{0.0, -std::sin(DegToRad(deg)), std::cos(DegToRad(deg))}; }

// One set of fins; `fold`: the folding ones folded, adjacent fins toward each other.
inline void FinSet(TriMesh& out, const MunitionSpec& m, const MunitionFins& f, bool fold) {
    const double half = 0.5 * m.length_m, s0 = f.x * m.length_m, s1 = s0 + f.root_chord_m, tip = 0.5 * f.span_m;
    const double rb = BodyRadius(m, s0);
    const double s2 = s0 + (tip - rb) * std::tan(DegToRad(f.sweep_deg)), s3 = s2 + f.tip_chord_m;
    const double r0 = kRootSink * rb, r1 = kRootSink * BodyRadius(m, s1);
    const double t = 0.02 * f.root_chord_m;
    const std::array<float, 3> c = LinearColour(f.colour.empty() ? m.colour : f.colour);
    for (int k = 0; k < f.count; ++k) {
        const double phi = f.roll_deg + 360.0 * k / f.count;
        const Vec3 u = Radial(phi), w = Around(phi);
        const auto at = [&](double s, double rho) { return Vec3{half - s, 0.0, 0.0} + u * rho; };
        if (!(fold && f.folds)) {
            Plate(out, {at(s0, r0), at(s1, r1), at(s3, tip), at(s2, tip)}, w, t, c, true);
            continue;
        }
        // The hinge runs along the fin `fold_at` of its exposed span out; the part beyond turns 90 degrees about it.
        const double rh = rb + f.fold_at * (tip - rb);
        const double fa = (rh - r0) / (tip - r0), fb = (rh - r1) / (tip - r1);
        const double sa = s0 + (s2 - s0) * fa, sb = s1 + (s3 - s1) * fb;
        const double sigma = k % 2 == 0 ? 1.0 : -1.0;
        const auto folded = [&](double s, double rho) { return Vec3{half - s, 0.0, 0.0} + u * rh + w * (sigma * (rho - rh)); };
        if (rh > r0 + kTiny) Plate(out, {at(s0, r0), at(s1, r1), at(sb, rh), at(sa, rh)}, w, t, c, true);
        Plate(out, {folded(sa, rh), folded(sb, rh), folded(s3, tip), folded(s2, tip)}, u, t, c, rh <= r0 + kTiny);
    }
}

struct BodyOptions {
    bool fold = false;      // folding fins folded
    bool blades = true;     // the retarder's blades drawn closed
};

inline void Body(TriMesh& out, const MunitionSpec& m, int lod, const BodyOptions& o) {
    const int sides = MunitionSides(lod);
    const int steps = lod <= 0 ? 8 : lod == 1 ? 5 : lod == 2 ? 3 : 1;
    LatheFrame fr;
    fr.half = 0.5 * m.length_m;
    fr.sides = sides;
    if (lod >= 3) {
        fr.turn0 = 0.25 * kPi;   // a box: its faces square to y and z, as wide as the body
        fr.scale = std::sqrt(2.0);
    }
    Lathe(out, m, Outline(m, steps), fr, lod < 3);
    if (lod >= 3) return;
    for (const MunitionRing& g : m.rings) {
        const double s0 = g.x * m.length_m, s1 = (g.x + g.w) * m.length_m, r = 0.5 * g.diameter_m;
        const std::array<float, 3> c = LinearColour(g.colour.empty() ? m.colour : g.colour);
        LatheSegment(out, Pt(s0, r, 0.0, 1.0), Pt(s1, r, 0.0, 1.0), fr, c);
        LatheSegment(out, Pt(s0, r, 0.0, -1.0), Pt(s1, r, 0.0, -1.0), fr, c);
    }
    for (const MunitionFins& f : m.fins) {
        if (f.blades && !o.blades) continue;
        FinSet(out, m, f, o.fold);
    }
}

// The Snakeye's blades open: hinged at the tail's end, turned out `open_deg` from the forward axis.
inline void OpenBlades(TriMesh& out, const MunitionSpec& m) {
    const MunitionRetarder& r = m.retarder;
    double roll = 45.0;
    std::string colour = m.colour;
    for (const MunitionFins& f : m.fins) {
        if (!f.blades) continue;
        roll = f.roll_deg;
        if (!f.colour.empty()) colour = f.colour;
    }
    const double half = 0.5 * m.length_m, rh = std::max(TailEndRadius(m), 0.25 * MaxBodyRadius(m));
    const double a = DegToRad(r.open_deg), len = r.length_m, wid = r.width_m;
    const std::array<float, 3> c = LinearColour(colour);
    for (int k = 0; k < r.count; ++k) {
        const double phi = roll + 360.0 * k / r.count;
        const Vec3 u = Radial(phi), w = Around(phi);
        const Vec3 d = Vec3{std::cos(a), 0.0, 0.0} + u * std::sin(a);
        const Vec3 hinge = Vec3{-half, 0.0, 0.0} + u * rh;
        const Vec3 n = Cross(d, w).Normalized();
        Plate(out, {hinge - w * (0.5 * wid), hinge + w * (0.5 * wid), hinge + d * len + w * (0.5 * wid), hinge + d * len - w * (0.5 * wid)}, n,
              std::max(0.004, 0.02 * len), c, false);
    }
}

// A line (a thin strip) from a to b.
inline void Line(TriMesh& out, const Vec3& a, const Vec3& b, const std::array<float, 3>& c) {
    const Vec3 e = (b - a).Normalized();
    Vec3 side = Cross(e, Vec3{0.0, 0.0, 1.0});
    if (side.Length() < 1e-6) side = Cross(e, Vec3{0.0, 1.0, 0.0});
    side = side.Normalized() * 0.006;
    Plate(out, {a - side, a + side, b + side, b - side}, Cross(e, side).Normalized(), 0.003, c, false);
}

// A ballute or chute behind the tail on its lines.
inline void Decelerator(TriMesh& out, const MunitionSpec& m, int lod, bool chute) {
    const MunitionRetarder& r = m.retarder;
    const double len = m.length_m, half = 0.5 * len;
    LatheFrame fr;
    fr.half = half;
    fr.sides = MunitionSides(lod);
    const int steps = lod <= 0 ? 8 : lod == 1 ? 5 : lod == 2 ? 3 : 1;
    std::vector<Vec3> anchors;  // where the lines meet it
    if (chute) {
        const double dc = r.diameter_m > 0.0 ? r.diameter_m : kChuteInflatedShare * std::sqrt(4.0 * r.area_m2 / kPi);
        const double rim = len + (r.length_m > 0.0 ? r.length_m : 1.5 * dc), depth = 0.35 * dc, rc = 0.5 * dc;
        LatheRun outside, inside;
        for (int j = 0; j <= steps; ++j) {
            const double a = 0.5 * kPi * j / steps;
            outside.push_back(Pt(rim + depth * std::sin(a), rc * std::cos(a), std::sin(a) / depth, std::cos(a) / rc));
            inside.push_back(Pt(rim + depth * std::sin(a), rc * std::cos(a), -std::sin(a) / depth, -std::cos(a) / rc));
        }
        const std::array<float, 3> c = LinearColour(r.colour.empty() ? "#d9d4c4" : r.colour);
        for (std::size_t j = 0; j + 1 < outside.size(); ++j) {
            LatheSegment(out, outside[j], outside[j + 1], fr, c);
            LatheSegment(out, inside[j], inside[j + 1], fr, c);
        }
        for (int k = 0; k < r.count; ++k) anchors.push_back(Vec3{half - rim, 0.0, 0.0} + Radial(360.0 * k / r.count) * rc);
    } else {
        const double db = r.diameter_m > 0.0 ? r.diameter_m : 3.0 * MaxBodyRadius(m) * 2.0;
        const double front = len + (r.length_m > 0.0 ? r.length_m : 0.6 * len), rb = 0.5 * db, cone = 0.55 * db, cap = 0.3 * db;
        std::vector<LatheRun> runs = {Straight(front, 0.0, front + cone, rb)};
        LatheRun back;
        for (int j = 0; j <= steps; ++j) {
            const double a = 0.5 * kPi * j / steps;
            back.push_back(Pt(front + cone + cap * std::sin(a), rb * std::cos(a), std::sin(a) / cap, std::cos(a) / rb));
        }
        runs.push_back(back);
        MunitionSpec shape = m;
        shape.bands.clear();
        shape.colour = r.colour.empty() ? "#8f8460" : r.colour;
        Lathe(out, shape, runs, fr, false);
        const double sa = front + 0.2 * cone;
        for (int k = 0; k < r.count; ++k) anchors.push_back(Vec3{half - sa, 0.0, 0.0} + Radial(45.0 + 360.0 * k / r.count) * (0.2 * rb));
    }
    if (lod >= 3) return;
    const double re = TailEndRadius(m);
    const std::array<float, 3> lc = LinearColour("#c8c2ad");
    for (std::size_t k = 0; k < anchors.size(); ++k) {
        const double phi = chute ? 360.0 * static_cast<double>(k) / static_cast<double>(anchors.size()) : 45.0 + 360.0 * static_cast<double>(k) / static_cast<double>(anchors.size());
        Line(out, Vec3{-half, 0.0, 0.0} + Radial(phi) * (0.5 * re), anchors[k], lc);
    }
}

}  // namespace munitions_detail

// The munition as carried and in normal flight, `lod` 0-3: the body with its bands, rings and every fin (a Snakeye's
// blades closed, a 9M33M2's wings open). Empty for a gun (no model).
inline TriMesh BuildMesh(const MunitionSpec& m, int lod) {
    TriMesh out;
    if (!m.HasMesh()) return out;
    munitions_detail::Body(out, m, std::max(0, std::min(lod, 3)), munitions_detail::BodyOptions{});
    return out;
}

// The parts a munition has for BuildPart.
inline std::vector<std::string> PartsOf(const MunitionSpec& m) {
    std::vector<std::string> out;
    if (!m.HasMesh()) return out;
    if (m.has_retarder && m.retarder.type == "blades") out.push_back("blades_open");
    if (m.has_retarder && m.retarder.type == "ballute") out.push_back("ballute");
    if (m.has_retarder && m.retarder.type == "chute") out.push_back("chute");
    for (const MunitionFins& f : m.fins) {
        if (f.folds) {
            out.push_back("wings_folded");
            break;
        }
    }
    return out;
}

// True for a part drawn in place of BuildMesh (the whole munition in another state); false for one drawn with it.
inline bool PartReplacesBody(const std::string& part) { return part == "blades_open" || part == "wings_folded"; }

// An animated part, in the munition's own frame (so it is drawn with the munition's transform):
//   "blades_open"  the whole munition with the Snakeye's four blades opened into an X (in place of BuildMesh);
//   "wings_folded" the whole munition with its folding wings folded in pairs, as in its container or under the carrier
//                  (in place of BuildMesh until they open);
//   "ballute"      the AIR's ballute on its lines behind the tail (with BuildMesh);
//   "chute"        the canopy, open, and its lines behind the tail (with BuildMesh; scale it about the tail's end,
//                  x = -length/2, while it opens).
// Empty when the munition has no such part.
inline TriMesh BuildPart(const MunitionSpec& m, const std::string& part, int lod) {
    using namespace munitions_detail;
    TriMesh out;
    if (!m.HasMesh()) return out;
    const int l = std::max(0, std::min(lod, 3));
    const std::vector<std::string> parts = PartsOf(m);
    if (std::find(parts.begin(), parts.end(), part) == parts.end()) return out;
    if (part == "blades_open") {
        BodyOptions o;
        o.blades = false;
        Body(out, m, l, o);
        if (l < 3) OpenBlades(out, m);
    } else if (part == "wings_folded") {
        BodyOptions o;
        o.fold = true;
        Body(out, m, l, o);
    } else {
        Decelerator(out, m, l, part == "chute");
    }
    return out;
}

namespace munitions_detail {

inline bool IsHex(const std::string& s) {
    if (s.size() != 7 || s[0] != '#') return false;
    for (std::size_t i = 1; i < 7; ++i) {
        const char ch = s[i];
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))) return false;
    }
    return true;
}

inline bool OneOf(const std::string& v, std::initializer_list<const char*> allowed) {
    for (const char* a : allowed) {
        if (v == a) return true;
    }
    return false;
}

class MunitionLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, MunitionLibrary& out) {
        if (!Keys(root, "", {"munitions_id", "display_name", "status", "source", "confidence", "notes", "colours", "munitions", "weapons", "systems",
                             "models", "aliases"})) {
            return false;
        }
        if (!Map(root, "colours", out.colours)) return false;
        for (const auto& kv : out.colours) {
            if (!IsHex(kv.second)) return Fail(Join("colours", kv.first), "expected #rrggbb");
        }
        const json::Value* arr = OptionalArray(root, "", "munitions");
        if (arr == nullptr || arr->items.empty()) return message_.empty() ? Fail("munitions", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index("munitions", i);
            MunitionSpec m;
            if (!One(arr->items[i], p, out, m)) return false;
            if (out.Find(m.id) != nullptr) return Fail(Join(p, "id"), "duplicate munition " + m.id);
            out.munitions.push_back(std::move(m));
        }
        return Targets(root, "weapons", out.weapons, out) && Targets(root, "systems", out.systems, out) && Targets(root, "models", out.models, out) &&
               Targets(root, "aliases", out.aliases, out);
    }

private:
    bool One(const json::Value& v, const std::string& p, const MunitionLibrary& lib, MunitionSpec& m) {
        if (!Keys(v, p, {"id", "name", "kind", "length_m", "diameter_m", "mass_kg", "charge_kg", "nose", "nose_l", "tail", "tail_l", "tail_r",
                         "sections", "fins", "rings", "colour", "bands", "retarder", "cluster", "motor", "smoke_s", "calibre_mm", "muzzle_mps",
                         "rate_rpm", "barrels", "tracer_s", "sd_s", "tracer_colour", "burst", "source", "confidence", "notes"}) ||
            !RequireString(v, p, "id", m.id) || !RequireString(v, p, "name", m.name) || !RequireString(v, p, "kind", m.kind) ||
            !Provenance(v, p) || !RequireString(v, p, "source", m.source) || !RequireString(v, p, "confidence", m.confidence) ||
            !OptionalString(v, p, "notes", m.notes) || !OptionalNumber(v, p, "mass_kg", m.mass_kg) ||
            !OptionalNumber(v, p, "charge_kg", m.charge_kg) || !OptionalNumber(v, p, "smoke_s", m.smoke_s)) {
            return false;
        }
        if (!OneOf(m.kind, {"bomb", "retarded", "ballute", "parachute", "cluster", "guided", "glide", "rocket", "missile", "sam", "arm", "cruise",
                            "shell", "gun"})) {
            return Fail(Join(p, "kind"), "unknown kind '" + m.kind + "'");
        }
        if (m.mass_kg < 0.0 || m.charge_kg < 0.0 || m.smoke_s < 0.0) return Fail(p, "mass_kg, charge_kg and smoke_s must not be negative");
        if (m.IsRound() && !Round(v, p, lib, m)) return false;
        if (m.kind != "gun" && !Shape(v, p, lib, m)) return false;
        if (!Motor(v, p, lib, m) || !Retarder(v, p, lib, m) || !Cluster(v, p, m)) return false;
        if (m.kind == "cluster" && !m.has_cluster) return Fail(Join(p, "cluster"), "required for a cluster");
        return true;
    }

    bool RequireNumber(const json::Value& obj, const std::string& p, const char* key, double& out) {
        if (obj.Find(key) == nullptr) return Fail(Join(p, key), "required");
        return OptionalNumber(obj, p, key, out);
    }

    // An object, absent or null (`out` nullptr), else a failure.
    bool OptionalObject(const json::Value& obj, const std::string& p, const char* key, const json::Value*& out) {
        out = obj.Find(key);
        if (out != nullptr && out->Is(json::Type::Null)) out = nullptr;
        if (out != nullptr && !out->Is(json::Type::Object)) return Fail(Join(p, key), "expected an object or null");
        return true;
    }

    // A list of objects, absent or null (`out` nullptr), else a failure.
    bool OptionalList(const json::Value& obj, const std::string& p, const char* key, const json::Value*& out) {
        out = obj.Find(key);
        if (out != nullptr && out->Is(json::Type::Null)) out = nullptr;
        if (out != nullptr && !out->Is(json::Type::Array)) return Fail(Join(p, key), "expected a list");
        return true;
    }

    // A colour name or #rrggbb, resolved in place.
    bool Colour(const MunitionLibrary& lib, std::string& c, const std::string& p) {
        if (IsHex(c)) return true;
        const auto it = lib.colours.find(c);
        if (it == lib.colours.end()) return Fail(p, "no such colour: " + c);
        c = it->second;
        return true;
    }

    bool Round(const json::Value& v, const std::string& p, const MunitionLibrary& lib, MunitionSpec& m) {
        if (!RequireNumber(v, p, "calibre_mm", m.calibre_mm) || !RequireNumber(v, p, "muzzle_mps", m.muzzle_mps) ||
            !RequireNumber(v, p, "rate_rpm", m.rate_rpm) || !RequireNumber(v, p, "tracer_s", m.tracer_s) || !OptionalInt(v, p, "barrels", m.barrels) ||
            !OptionalString(v, p, "tracer_colour", m.tracer_colour) || !OptionalString(v, p, "burst", m.burst)) {
            return false;
        }
        if (!(m.calibre_mm > 0.0 && m.muzzle_mps > 0.0 && m.rate_rpm > 0.0) || m.tracer_s < 0.0 || m.barrels < 1) {
            return Fail(p, "a round needs calibre_mm, muzzle_mps and rate_rpm over 0, tracer_s 0 or more, barrels 1 or more");
        }
        if (!OneOf(m.burst, {"contact", "time"})) return Fail(Join(p, "burst"), "contact or time");
        if (m.tracer_s > 0.0 && m.tracer_colour.empty()) return Fail(Join(p, "tracer_colour"), "required with a tracer");
        if (!m.tracer_colour.empty() && !Colour(lib, m.tracer_colour, Join(p, "tracer_colour"))) return false;
        const json::Value* sd = v.Find("sd_s");
        if (sd != nullptr) {
            if (!sd->Is(json::Type::Array) || sd->items.size() != 2 || !sd->items[0].Is(json::Type::Number) || !sd->items[1].Is(json::Type::Number) ||
                sd->items[0].number < 0.0 || sd->items[1].number < sd->items[0].number) {
                return Fail(Join(p, "sd_s"), "expected [earliest, latest] seconds");
            }
            m.sd_lo_s = sd->items[0].number;
            m.sd_hi_s = sd->items[1].number;
        }
        return true;
    }

    bool Shape(const json::Value& v, const std::string& p, const MunitionLibrary& lib, MunitionSpec& m) {
        if (!RequireNumber(v, p, "length_m", m.length_m) || !RequireNumber(v, p, "diameter_m", m.diameter_m) ||
            !RequireString(v, p, "colour", m.colour) || !OptionalString(v, p, "nose", m.nose) || !OptionalNumber(v, p, "nose_l", m.nose_l) ||
            !OptionalString(v, p, "tail", m.tail) || !OptionalNumber(v, p, "tail_l", m.tail_l) || !OptionalNumber(v, p, "tail_r", m.tail_r)) {
            return false;
        }
        if (!(m.length_m > 0.0 && m.diameter_m > 0.0)) return Fail(p, "length_m and diameter_m must be over 0");
        if (!Colour(lib, m.colour, Join(p, "colour"))) return false;
        if (!OneOf(m.nose, {"ogive", "conical", "hemisphere", "blunt", "flat"})) return Fail(Join(p, "nose"), "ogive, conical, hemisphere, blunt or flat");
        if (!OneOf(m.tail, {"boattail", "cone", "flat"})) return Fail(Join(p, "tail"), "boattail, cone or flat");
        const double nf = m.nose == "flat" ? 0.0 : m.nose_l, tf = m.tail == "flat" ? 0.0 : m.tail_l;
        if (m.nose_l < 0.0 || m.tail_l < 0.0 || nf + tf > 1.0) return Fail(p, "nose_l and tail_l are fractions of the length, together at most 1");
        if (m.nose != "flat" && m.nose_l <= 0.0) return Fail(Join(p, "nose_l"), "a " + m.nose + " nose needs a length");
        if (m.tail != "flat" && m.tail_l <= 0.0) return Fail(Join(p, "tail_l"), "a " + m.tail + " tail needs a length");
        if (m.tail_r < 0.0 || m.tail_r > 1.0) return Fail(Join(p, "tail_r"), "a fraction of the body's radius");
        const json::Value* arr = nullptr;
        if (!OptionalList(v, p, "sections", arr)) return false;
        for (std::size_t i = 0; arr != nullptr && i < arr->items.size(); ++i) {
            const std::string q = Index(Join(p, "sections"), i);
            MunitionSection s;
            if (!Keys(arr->items[i], q, {"x", "diameter_m", "notes"}) || !RequireNumber(arr->items[i], q, "x", s.x) ||
                !RequireNumber(arr->items[i], q, "diameter_m", s.diameter_m)) {
                return false;
            }
            const double before = m.sections.empty() ? nf : m.sections.back().x;
            if (s.diameter_m <= 0.0 || s.x <= before || s.x >= 1.0 - tf) return Fail(q, "a section starts after the nose and the one before, before the tail, with a diameter");
            m.sections.push_back(s);
        }
        if (!OptionalList(v, p, "bands", arr)) return false;
        for (std::size_t i = 0; arr != nullptr && i < arr->items.size(); ++i) {
            const std::string q = Index(Join(p, "bands"), i);
            MunitionBand b;
            if (!Keys(arr->items[i], q, {"x", "w", "colour", "notes"}) || !RequireNumber(arr->items[i], q, "x", b.x) ||
                !RequireNumber(arr->items[i], q, "w", b.w) || !RequireString(arr->items[i], q, "colour", b.colour) || !Colour(lib, b.colour, Join(q, "colour"))) {
                return false;
            }
            if (b.x < 0.0 || b.w <= 0.0 || b.x + b.w > 1.0) return Fail(q, "x and w are fractions of the length, inside it");
            m.bands.push_back(b);
        }
        if (!OptionalList(v, p, "rings", arr)) return false;
        for (std::size_t i = 0; arr != nullptr && i < arr->items.size(); ++i) {
            const std::string q = Index(Join(p, "rings"), i);
            MunitionRing g;
            if (!Keys(arr->items[i], q, {"x", "w", "diameter_m", "colour", "notes"}) || !RequireNumber(arr->items[i], q, "x", g.x) ||
                !RequireNumber(arr->items[i], q, "w", g.w) || !RequireNumber(arr->items[i], q, "diameter_m", g.diameter_m) ||
                !OptionalString(arr->items[i], q, "colour", g.colour) || (!g.colour.empty() && !Colour(lib, g.colour, Join(q, "colour")))) {
                return false;
            }
            if (g.x < 0.0 || g.w <= 0.0 || g.x + g.w > 1.0 || g.diameter_m <= 0.0) return Fail(q, "a ring inside the length, with a diameter");
            m.rings.push_back(g);
        }
        if (!OptionalList(v, p, "fins", arr)) return false;
        for (std::size_t i = 0; arr != nullptr && i < arr->items.size(); ++i) {
            const std::string q = Index(Join(p, "fins"), i);
            MunitionFins f;
            const json::Value& fv = arr->items[i];
            if (!Keys(fv, q, {"count", "span_m", "root_chord_m", "tip_chord_m", "x", "sweep_deg", "roll_deg", "folds", "fold_at", "blades", "colour", "notes"}) ||
                !OptionalInt(fv, q, "count", f.count) || !RequireNumber(fv, q, "span_m", f.span_m) || !RequireNumber(fv, q, "root_chord_m", f.root_chord_m) ||
                !RequireNumber(fv, q, "tip_chord_m", f.tip_chord_m) || !RequireNumber(fv, q, "x", f.x) || !OptionalNumber(fv, q, "sweep_deg", f.sweep_deg) ||
                !OptionalNumber(fv, q, "roll_deg", f.roll_deg) || !OptionalBool(fv, q, "folds", f.folds) || !OptionalNumber(fv, q, "fold_at", f.fold_at) ||
                !OptionalBool(fv, q, "blades", f.blades) || !OptionalString(fv, q, "colour", f.colour) ||
                (!f.colour.empty() && !Colour(lib, f.colour, Join(q, "colour")))) {
                return false;
            }
            if (f.count < 1 || f.span_m <= 0.0 || f.root_chord_m <= 0.0 || f.tip_chord_m < 0.0 || f.x < 0.0 || f.x > 1.0 || std::fabs(f.sweep_deg) >= 85.0 ||
                f.fold_at < 0.0 || f.fold_at >= 1.0) {
                return Fail(q, "count 1 or more, span and root chord over 0, x a fraction, sweep under 85 degrees, fold_at in 0..1");
            }
            m.fins.push_back(f);
            const double len = m.length_m, s0 = f.x * len, s1 = s0 + f.root_chord_m, tip = 0.5 * f.span_m;
            const double s2 = s0 + (tip - BodyRadius(m, s0)) * std::tan(DegToRad(f.sweep_deg)), s3 = s2 + f.tip_chord_m;
            if (tip <= BodyRadius(m, s0) || tip <= BodyRadius(m, s1)) return Fail(q, "the fins' tips are inside the body");
            if (s1 > len * (1.0 + 1e-6) || s3 > len * (1.0 + 1e-6) || s2 < 0.0) {
                return Fail(q, "the fins reach past the tail or ahead of the nose (x is the root's leading edge)");
            }
        }
        return true;
    }

    bool Motor(const json::Value& v, const std::string& p, const MunitionLibrary& lib, MunitionSpec& m) {
        const json::Value* o = nullptr;
        if (!OptionalObject(v, p, "motor", o)) return false;
        if (o == nullptr) return true;
        const std::string q = Join(p, "motor");
        MunitionMotor& mo = m.motor;
        if (!Keys(*o, q, {"boost_s", "sustain_s", "smoke", "sustain_smoke", "plume_colour", "tracer", "notes"}) || !OptionalNumber(*o, q, "boost_s", mo.boost_s) ||
            !OptionalNumber(*o, q, "sustain_s", mo.sustain_s) || !OptionalString(*o, q, "smoke", mo.smoke) ||
            !OptionalString(*o, q, "sustain_smoke", mo.sustain_smoke) || !OptionalString(*o, q, "plume_colour", mo.plume_colour) ||
            !OptionalBool(*o, q, "tracer", mo.tracer)) {
            return false;
        }
        if (mo.boost_s < 0.0 || mo.sustain_s < 0.0) return Fail(q, "boost_s and sustain_s must not be negative");
        if (!OneOf(mo.smoke, {"dense", "reduced", "minimal", "none"})) return Fail(Join(q, "smoke"), "dense, reduced, minimal or none");
        if (!mo.sustain_smoke.empty() && !OneOf(mo.sustain_smoke, {"dense", "reduced", "minimal", "none"})) {
            return Fail(Join(q, "sustain_smoke"), "dense, reduced, minimal or none");
        }
        if (!mo.plume_colour.empty() && !Colour(lib, mo.plume_colour, Join(q, "plume_colour"))) return false;
        m.has_motor = true;
        return true;
    }

    bool Retarder(const json::Value& v, const std::string& p, const MunitionLibrary& lib, MunitionSpec& m) {
        const json::Value* o = nullptr;
        if (!OptionalObject(v, p, "retarder", o)) return false;
        const char* wanted = m.kind == "retarded" ? "blades" : m.kind == "ballute" ? "ballute" : m.kind == "parachute" ? "chute" : nullptr;
        bool any_blades = false;
        for (const MunitionFins& f : m.fins) any_blades = any_blades || f.blades;
        if (o == nullptr) {
            if (wanted != nullptr) return Fail(Join(p, "retarder"), std::string("a ") + m.kind + " munition needs its retarder");
            if (any_blades) return Fail(Join(p, "fins"), "blades without a retarder");
            return true;
        }
        const std::string q = Join(p, "retarder");
        MunitionRetarder& r = m.retarder;
        if (!Keys(*o, q, {"type", "count", "length_m", "width_m", "open_deg", "diameter_m", "area_m2", "open_s", "descent_mps", "colour", "notes"}) ||
            !RequireString(*o, q, "type", r.type) || !OptionalInt(*o, q, "count", r.count) || !OptionalNumber(*o, q, "length_m", r.length_m) ||
            !OptionalNumber(*o, q, "width_m", r.width_m) || !OptionalNumber(*o, q, "open_deg", r.open_deg) ||
            !OptionalNumber(*o, q, "diameter_m", r.diameter_m) || !OptionalNumber(*o, q, "area_m2", r.area_m2) ||
            !OptionalNumber(*o, q, "open_s", r.open_s) || !OptionalNumber(*o, q, "descent_mps", r.descent_mps) ||
            !OptionalString(*o, q, "colour", r.colour) || (!r.colour.empty() && !Colour(lib, r.colour, Join(q, "colour")))) {
            return false;
        }
        if (wanted == nullptr || r.type != wanted) {
            return Fail(Join(q, "type"), "blades for a retarded munition, ballute for a ballute, chute for a parachute (and none for the rest)");
        }
        if (r.count < 1 || r.length_m < 0.0 || r.width_m < 0.0 || r.diameter_m < 0.0 || r.area_m2 < 0.0 || r.open_s < 0.0 || r.descent_mps < 0.0) {
            return Fail(q, "count 1 or more, no negative sizes, times or speeds");
        }
        if (r.type == "blades" && r.descent_mps > 0.0) return Fail(Join(q, "descent_mps"), "a ballute's or chute's (blades only slow the bomb)");
        if (r.type == "blades" && (r.length_m <= 0.0 || r.width_m <= 0.0 || !any_blades)) {
            return Fail(q, "blades need length_m and width_m, and a fin set marked \"blades\" (them, closed)");
        }
        if (r.type != "blades" && any_blades) return Fail(Join(p, "fins"), "blades only with a blades retarder");
        if (r.type == "chute" && r.area_m2 <= 0.0 && r.diameter_m <= 0.0) return Fail(q, "a chute needs area_m2 or diameter_m");
        m.has_retarder = true;
        return true;
    }

    bool Cluster(const json::Value& v, const std::string& p, MunitionSpec& m) {
        const json::Value* o = nullptr;
        if (!OptionalObject(v, p, "cluster", o)) return false;
        if (o == nullptr) return true;
        const std::string q = Join(p, "cluster");
        MunitionCluster& c = m.cluster;
        if (!Keys(*o, q, {"opens_agl_m", "opens_s", "submunitions", "pattern_m", "notes"}) || !OptionalNumber(*o, q, "opens_agl_m", c.opens_agl_m) ||
            !OptionalNumber(*o, q, "opens_s", c.opens_s) || !OptionalInt(*o, q, "submunitions", c.submunitions)) {
            return false;
        }
        if (m.kind != "cluster") return Fail(q, "only for a cluster");
        if (c.submunitions < 1 || c.opens_agl_m < 0.0 || c.opens_s < 0.0) return Fail(q, "submunitions 1 or more, no negative height or time");
        const json::Value* pat = o->Find("pattern_m");
        if (pat != nullptr) {
            if (!pat->Is(json::Type::Array) || pat->items.size() != 2 || !pat->items[0].Is(json::Type::Number) ||
                !pat->items[1].Is(json::Type::Number) || pat->items[0].number <= 0.0 || pat->items[1].number <= 0.0) {
                return Fail(Join(q, "pattern_m"), "expected [across, along] metres");
            }
            c.pattern_w_m = pat->items[0].number;
            c.pattern_l_m = pat->items[1].number;
        }
        m.has_cluster = true;
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

    // {id: munition id}: every munition named must exist.
    bool Targets(const json::Value& root, const char* key, std::map<std::string, std::string>& out, const MunitionLibrary& lib) {
        if (!Map(root, key, out)) return false;
        for (const auto& kv : out) {
            if (lib.Find(kv.second) == nullptr) return Fail(Join(key, kv.first), "no such munition: " + kv.second);
        }
        return true;
    }
};

}  // namespace munitions_detail

inline bool MunitionLibrary::LoadJson(const std::string& text, std::string* error) {
    json::Value root;
    std::string perr;
    if (!json::Parse(text, root, &perr)) {
        if (error != nullptr) *error = perr;
        return false;
    }
    munitions_detail::MunitionLoader l;
    MunitionLibrary lib;
    if (!l.Load(root, lib)) {
        if (error != nullptr) *error = l.Message();
        return false;
    }
    *this = std::move(lib);
    return true;
}

inline bool MunitionLibrary::Load(const std::string& path_or_json, std::string* error) {
    const std::size_t first = path_or_json.find_first_not_of(" \t\r\n");
    if (first != std::string::npos && path_or_json[first] == '{') return LoadJson(path_or_json, error);
    std::ifstream f(path_or_json, std::ios::binary);
    if (!f) {
        if (error != nullptr) *error = "cannot open " + path_or_json;
        return false;
    }
    std::ostringstream s;
    s << f.rdbuf();
    std::string e;
    if (!LoadJson(s.str(), &e)) {
        if (error != nullptr) *error = path_or_json + ": " + e;
        return false;
    }
    return true;
}

}  // namespace scenery
}  // namespace simcore
