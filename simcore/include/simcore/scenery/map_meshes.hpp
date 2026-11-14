// simcore/scenery/map_meshes.hpp — the map's dressing as triangle meshes (roadmap R6, docs/scenery/README.md "The map's
// meshes"): trees by kind, the mass of a wood seen from afar, houses with their roofs, barns, blocks of flats, industrial
// halls and bridges. Procedural, engine-agnostic and deterministic (clients rebuild them, docs/MULTIPLAYER.md): Unreal
// turns them into static meshes (FSimMapAssets, FSimRuntimeMesh) and draws them where the map's layout has its boxes.
// A real asset registered in data/scenery/assets.json (scenery/asset_register.hpp) replaces one of these by its part key;
// these are the fallback every part always has. Presentation only: nothing in the simulation reads them (the sight lines
// read the map's obstacle heights, simcore/terrain_map.hpp).
//
// Frames (metres; TriMesh's winding, scenery/trimesh.hpp):
// - a tree: its base at the origin, z up, unit height (top at z = 1) and a crown one unit across (x and y in -0.5..0.5);
//   an instance scales it by (crown, crown, height);
// - a wood's mass, a house, a barn, flats, an industrial hall: the unit cube centred on the origin (x along the box's
//   length, its heading; y across; z up, -0.5..0.5), as the engine's own cube the map's boxes are drawn with today, so
//   the same transform (box centre, heading, length x width x height) places either. Its top is the box's top;
// - a bridge: the crossing's own numbers (its two ends and the bed below), relative to its first end.
//
// Every mesh is a set of closed convex solids, each face flat-shaded with its own colour: each edge is used by exactly
// two triangles, once each way, and the volume is positive (tests/test_map_meshes.cpp).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "simcore/scenery/trimesh.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace scenery {

// The kinds of tree the map's woods, rows and gardens are planted with (the plain's oak, the poplar of the river banks,
// the planted pine and the black locust of the shelter belts).
enum class TreeKind { Oak, Poplar, Pine, Acacia, Count };

inline const char* TreeKindName(TreeKind k) {
    switch (k) {
    case TreeKind::Oak: return "oak";
    case TreeKind::Poplar: return "poplar";
    case TreeKind::Pine: return "pine";
    case TreeKind::Acacia: return "acacia";
    case TreeKind::Count: break;
    }
    return "?";
}

constexpr int kTreeLods = 4;           // LOD 0-3: 8, 6 and 4 sides, then the crown alone as a 4-sided spindle
constexpr int kBuildingLods = 2;       // LOD 0 the building, LOD 1 its box
constexpr int kCanopyMassVariants = 4;
constexpr int kHouseVariants = 4;
constexpr int kFlatsMaxFloors = 24;
constexpr double kHouseWallShare = 0.68;  // of a house's height, its walls; the roof the rest (0.32)
constexpr double kBridgePierSpacingM = 28.0;  // the longest span between piers (game value: piers every 25-30 m)

namespace map_mesh_detail {

using Colour = std::array<float, 3>;

// Linear-RGB colours (the sRGB values they were picked as in the comments). Game values: placeholders until real assets.
constexpr Colour kBark{0.051f, 0.033f, 0.019f};         // #3f3226
constexpr Colour kOakCrown{0.045f, 0.093f, 0.024f};     // #3c5629
constexpr Colour kPoplarCrown{0.060f, 0.120f, 0.030f};  // #456130
constexpr Colour kPineCrown{0.024f, 0.059f, 0.025f};    // #2b452c
constexpr Colour kAcaciaCrown{0.070f, 0.115f, 0.035f};  // #4a5f35
constexpr Colour kMassSide{0.036f, 0.070f, 0.022f};     // #354b28
constexpr Colour kMassTop{0.050f, 0.095f, 0.028f};      // #3e562e
constexpr Colour kConcrete{0.300f, 0.300f, 0.280f};     // #959592
constexpr Colour kConcreteDark{0.150f, 0.150f, 0.145f}; // #6c6c6a
constexpr Colour kWindowBand{0.060f, 0.065f, 0.070f};   // #464849
constexpr Colour kBarnWall{0.120f, 0.060f, 0.035f};     // #613f2a
constexpr Colour kBarnRoof{0.080f, 0.080f, 0.080f};     // #4f4f4f
constexpr Colour kHallRoof{0.110f, 0.115f, 0.120f};     // #5c5e60
constexpr Colour kGlazing{0.090f, 0.110f, 0.120f};      // #545c60
constexpr Colour kHouseWalls[kHouseVariants] = {
    {0.700f, 0.680f, 0.600f},  // #dad5cb whitewash
    {0.580f, 0.450f, 0.250f},  // #c8b388 ochre
    {0.450f, 0.430f, 0.400f},  // #b4b1aa grey render
    {0.640f, 0.560f, 0.420f},  // #d1c4ac sand
};
constexpr Colour kHouseRoofs[kHouseVariants] = {
    {0.300f, 0.075f, 0.040f},  // #953e2a tile
    {0.200f, 0.060f, 0.035f},  // #7c3627 old tile
    {0.110f, 0.110f, 0.115f},  // #5c5c5e asbestos slate
    {0.260f, 0.090f, 0.050f},  // #8c4430 tile
};

// Adds closed convex solids, one face at a time. Each face is given as its corners in order (either way round); it is
// turned to face away from the solid's inside point and fanned into triangles, with its own vertices and flat normal.
class SolidBuilder {
public:
    explicit SolidBuilder(TriMesh& mesh) : mesh_(mesh) {}

    void Begin(const Vec3& inside) { inside_ = inside; }

    void Face(std::vector<Vec3> corners, const Colour& colour) {
        if (corners.size() < 3) return;
        // Newell's normal: robust for any planar (or nearly planar) polygon.
        Vec3 n{};
        Vec3 c{};
        for (std::size_t i = 0; i < corners.size(); ++i) {
            const Vec3& a = corners[i];
            const Vec3& b = corners[(i + 1) % corners.size()];
            n.x += (a.y - b.y) * (a.z + b.z);
            n.y += (a.z - b.z) * (a.x + b.x);
            n.z += (a.x - b.x) * (a.y + b.y);
            c = c + a;
        }
        c = c * (1.0 / static_cast<double>(corners.size()));
        if (n.Length() < 1e-12) return;
        if (Dot(n, c - inside_) < 0.0) {
            std::reverse(corners.begin(), corners.end());
            n = n * -1.0;
        }
        n = n.Normalized();
        const std::uint32_t first = static_cast<std::uint32_t>(mesh_.positions.size());
        for (const Vec3& p : corners) {
            mesh_.positions.push_back(p);
            mesh_.normals.push_back(n);
            mesh_.colours.push_back(colour);
        }
        for (std::uint32_t i = 1; i + 1 < static_cast<std::uint32_t>(corners.size()); ++i) {
            mesh_.indices.push_back(first);
            mesh_.indices.push_back(first + i);
            mesh_.indices.push_back(first + i + 1);
        }
    }

    // A loft through horizontal rings of the same number of corners, ascending; a ring of one point is an apex (first or
    // last only). The solid must be convex (each ring convex, the radii a concave function of height). `colours[k]` is
    // the band between ring k and k+1; the caps take `bottom` and `top`.
    void Loft(const std::vector<std::vector<Vec3>>& rings, const std::vector<Colour>& colours, const Colour& bottom, const Colour& top) {
        if (rings.size() < 2) return;
        Vec3 inside{};
        std::size_t count = 0;
        for (const auto& r : rings) {
            for (const Vec3& p : r) {
                inside = inside + p;
                ++count;
            }
        }
        Begin(inside * (1.0 / static_cast<double>(count)));
        for (std::size_t k = 0; k + 1 < rings.size(); ++k) {
            const auto& lo = rings[k];
            const auto& hi = rings[k + 1];
            const Colour& col = colours[std::min(k, colours.size() - 1)];
            const std::size_t n = std::max(lo.size(), hi.size());
            for (std::size_t i = 0; i < n; ++i) {
                const std::size_t j = (i + 1) % n;
                if (lo.size() == 1) {
                    Face({lo[0], hi[i], hi[j]}, col);
                } else if (hi.size() == 1) {
                    Face({lo[i], lo[j], hi[0]}, col);
                } else {
                    // Two triangles, not one quad: a ring's corners need not keep a band's four corners in a plane.
                    Face({lo[i], lo[j], hi[j]}, col);
                    Face({lo[i], hi[j], hi[i]}, col);
                }
            }
        }
        if (rings.front().size() > 2) Face(rings.front(), bottom);
        if (rings.back().size() > 2) Face(rings.back(), top);
    }

    // An oriented box: centre, unit axes along, across and up, half sizes.
    void Box(const Vec3& centre, const Vec3& along, const Vec3& across, const Vec3& up, double half_l, double half_w, double half_h,
             const Colour& sides, const Colour& top, const Colour& bottom) {
        std::vector<Vec3> lo, hi;
        const double sx[4] = {-1.0, 1.0, 1.0, -1.0};
        const double sy[4] = {-1.0, -1.0, 1.0, 1.0};
        for (int i = 0; i < 4; ++i) {
            const Vec3 p = centre + along * (sx[i] * half_l) + across * (sy[i] * half_w);
            lo.push_back(p - up * half_h);
            hi.push_back(p + up * half_h);
        }
        Loft({lo, hi}, {sides}, bottom, top);
    }

private:
    TriMesh& mesh_;
    Vec3 inside_{};
};

// A ring of `sides` corners at height z, radius r (a point when r is 0), turned by `turn_rad`.
inline std::vector<Vec3> Ring(int sides, double z, double r, double turn_rad = 0.0) {
    if (r <= 0.0) return {Vec3{0.0, 0.0, z}};
    std::vector<Vec3> out;
    for (int i = 0; i < sides; ++i) {
        const double a = turn_rad + 2.0 * kPi * i / sides;
        out.push_back(Vec3{r * std::cos(a), r * std::sin(a), z});
    }
    return out;
}

// A body of revolution through (z, r) pairs, ascending z, as a `sides`-gon loft in one colour.
inline void Lathe(SolidBuilder& b, int sides, const std::vector<std::array<double, 2>>& profile, const Colour& colour, double turn_rad = 0.0) {
    std::vector<std::vector<Vec3>> rings;
    for (const auto& zr : profile) rings.push_back(Ring(sides, zr[0], zr[1], turn_rad));
    b.Loft(rings, {colour}, colour, colour);
}

// A prism along x from x0 to x1 whose cross-section is the convex polygon `yz` (y, z pairs).
inline void PrismX(SolidBuilder& b, double x0, double x1, const std::vector<std::array<double, 2>>& yz, const std::vector<Colour>& side_colours,
                   const Colour& end_colour) {
    Vec3 inside{};
    for (const auto& p : yz) inside = inside + Vec3{0.5 * (x0 + x1), p[0], p[1]};
    b.Begin(inside * (1.0 / static_cast<double>(yz.size())));
    std::vector<Vec3> e0, e1;
    for (const auto& p : yz) {
        e0.push_back(Vec3{x0, p[0], p[1]});
        e1.push_back(Vec3{x1, p[0], p[1]});
    }
    b.Face(e0, end_colour);
    b.Face(e1, end_colour);
    for (std::size_t i = 0; i < yz.size(); ++i) {
        const std::size_t j = (i + 1) % yz.size();
        b.Face({e0[i], e0[j], e1[j], e1[i]}, side_colours[std::min(i, side_colours.size() - 1)]);
    }
}

// Swaps x and y of every vertex and normal (a quarter turn and a mirror) and rewinds each triangle to keep it outward.
inline void SwapXY(TriMesh& m) {
    for (Vec3& p : m.positions) std::swap(p.x, p.y);
    for (Vec3& n : m.normals) std::swap(n.x, n.y);
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) std::swap(m.indices[t + 1], m.indices[t + 2]);
}

// The unit cube centred on the origin in one colour for its sides and one for its top (a building's far LOD).
inline TriMesh UnitBox(const Colour& sides, const Colour& top) {
    TriMesh m;
    SolidBuilder b(m);
    b.Box(Vec3{}, Vec3{1.0, 0.0, 0.0}, Vec3{0.0, 1.0, 0.0}, Vec3{0.0, 0.0, 1.0}, 0.5, 0.5, 0.5, sides, top, sides);
    return m;
}

}  // namespace map_mesh_detail

// A tree of the kind at LOD 0-3 (empty beyond): base at the origin, unit height, a crown one unit across.
inline TriMesh TreeMesh(TreeKind kind, int lod) {
    using namespace map_mesh_detail;
    TriMesh m;
    if (lod < 0 || lod >= kTreeLods) return m;
    SolidBuilder b(m);
    const int sides = lod == 0 ? 8 : lod == 1 ? 6 : 4;
    Colour crown = kOakCrown;
    // The crown's profile (height, radius), its radius a concave function of height; the trunk's top and radius.
    std::vector<std::array<double, 2>> profile;
    double trunk_top = 0.4, trunk_r = 0.04;
    switch (kind) {
    case TreeKind::Poplar:
        crown = kPoplarCrown;
        profile = {{0.12, 0.22}, {0.45, 0.5}, {0.8, 0.38}, {1.0, 0.0}};
        trunk_top = 0.2;
        trunk_r = 0.06;
        break;
    case TreeKind::Pine:
        crown = kPineCrown;
        profile = {{0.5, 0.3}, {0.7, 0.5}, {0.88, 0.4}, {1.0, 0.0}};
        trunk_top = 0.6;
        trunk_r = 0.035;
        break;
    case TreeKind::Acacia:
        crown = kAcaciaCrown;
        profile = {{0.32, 0.36}, {0.58, 0.5}, {0.86, 0.36}, {1.0, 0.0}};
        trunk_top = 0.4;
        trunk_r = 0.045;
        break;
    case TreeKind::Oak:
    case TreeKind::Count:
    default:
        profile = {{0.28, 0.3}, {0.55, 0.5}, {0.82, 0.42}, {1.0, 0.0}};
        trunk_top = 0.38;
        trunk_r = 0.045;
        break;
    }
    if (lod == 3) {
        // From afar the crown alone, a spindle from below its widest ring to the top.
        Lathe(b, 4, {{profile.front()[0], 0.0}, {0.5 * (profile.front()[0] + 1.0), 0.5}, {1.0, 0.0}}, crown, 0.25 * kPi);
        return m;
    }
    if (lod == 2) profile = {profile.front(), profile[1], profile.back()};
    // The trunk runs up into the crown (its top ring inside the crown's lowest one), so no edge of the two coincides.
    Lathe(b, std::max(4, sides / 2 + 1), {{0.0, trunk_r}, {trunk_top, 0.7 * trunk_r}}, kBark);
    Lathe(b, sides, profile, crown, kPi / sides);
    return m;
}

// The mass of a wood as one solid on the unit cube (variant 0-3): steep sides with chamfered corners to a shoulder, then
// rounding in to a flat crown. Drawn where single trees are not (the far ground, a wood's inside).
inline TriMesh CanopyMassMesh(int variant) {
    using namespace map_mesh_detail;
    const int v = ((variant % kCanopyMassVariants) + kCanopyMassVariants) % kCanopyMassVariants;
    const double shoulder[kCanopyMassVariants] = {0.15, 0.22, 0.08, 0.18};  // z of the shoulder in -0.5..0.5
    const double inset[kCanopyMassVariants] = {0.12, 0.16, 0.10, 0.14};     // the crown's inset at the top
    const double chamfer = 0.1;
    auto ring = [&](double z, double in) {
        const double h = 0.5 - in;
        const double c = chamfer * h / 0.5;  // the corners cut in proportion, never to nothing
        return std::vector<Vec3>{{-h + c, -h, z}, {h - c, -h, z}, {h, -h + c, z}, {h, h - c, z},
                                 {h - c, h, z},  {-h + c, h, z}, {-h, h - c, z}, {-h, -h + c, z}};
    };
    TriMesh m;
    SolidBuilder b(m);
    const double mid_z = 0.5 * (shoulder[v] + 0.5);
    b.Loft({ring(-0.5, 0.0), ring(shoulder[v], 0.0), ring(mid_z, 0.35 * inset[v]), ring(0.5, inset[v])},
           {kMassSide, kMassSide, kMassTop}, kMassSide, kMassTop);
    return m;
}

// A house on the unit cube (variant 0-3, LOD 0-1): walls to 0.68 of its height, the roof the rest, its ridge at the box's
// top. 0: a gable roof with the ridge along the length; 1: the ridge across (a gable to the street); 2: a hipped roof;
// 3: a hipped roof across. Each variant its own walls and roof.
inline TriMesh HouseMesh(int variant, int lod = 0) {
    using namespace map_mesh_detail;
    const int v = ((variant % kHouseVariants) + kHouseVariants) % kHouseVariants;
    const Colour& wall = kHouseWalls[v];
    const Colour& roof = kHouseRoofs[v];
    if (lod == 1) return UnitBox(wall, roof);
    TriMesh m;
    if (lod != 0) return m;
    SolidBuilder b(m);
    const double eave = -0.5 + kHouseWallShare;
    if (v == 0 || v == 1) {
        // The cross-section: floor, wall, eave, ridge, eave, wall.
        PrismX(b, -0.5, 0.5, {{-0.5, -0.5}, {0.5, -0.5}, {0.5, eave}, {0.0, 0.5}, {-0.5, eave}}, {wall, wall, roof, roof, wall}, wall);
    } else {
        const double hip = 0.25;  // the ridge stops this far from each end
        b.Begin(Vec3{0.0, 0.0, 0.0});
        const Vec3 f0{-0.5, -0.5, -0.5}, f1{0.5, -0.5, -0.5}, f2{0.5, 0.5, -0.5}, f3{-0.5, 0.5, -0.5};
        const Vec3 e0{-0.5, -0.5, eave}, e1{0.5, -0.5, eave}, e2{0.5, 0.5, eave}, e3{-0.5, 0.5, eave};
        const Vec3 r0{-0.5 + hip, 0.0, 0.5}, r1{0.5 - hip, 0.0, 0.5};
        b.Face({f0, f1, f2, f3}, wall);
        b.Face({f0, f1, e1, e0}, wall);
        b.Face({f1, f2, e2, e1}, wall);
        b.Face({f2, f3, e3, e2}, wall);
        b.Face({f3, f0, e0, e3}, wall);
        b.Face({e0, e1, r1, r0}, roof);
        b.Face({e2, e3, r0, r1}, roof);
        b.Face({e1, e2, r1}, roof);
        b.Face({e3, e0, r0}, roof);
    }
    if (v == 1 || v == 3) SwapXY(m);
    return m;
}

// A barn or farm building on the unit cube (LOD 0-1): low timber walls (0.55 of its height) under a steep gable roof
// along its length.
inline TriMesh BarnMesh(int lod = 0) {
    using namespace map_mesh_detail;
    if (lod == 1) return UnitBox(kBarnWall, kBarnRoof);
    TriMesh m;
    if (lod != 0) return m;
    SolidBuilder b(m);
    const double eave = 0.05;
    PrismX(b, -0.5, 0.5, {{-0.5, -0.5}, {0.5, -0.5}, {0.5, eave}, {0.0, 0.5}, {-0.5, eave}},
           {kBarnWall, kBarnWall, kBarnRoof, kBarnRoof, kBarnWall}, kBarnWall);
    return m;
}

// A block of flats on the unit cube (LOD 0-1) with `floors` storeys (1-24): each a band of concrete panel under a band
// of windows, and a flat roof.
inline TriMesh FlatsMesh(int floors, int lod = 0) {
    using namespace map_mesh_detail;
    if (lod == 1) return UnitBox(kConcrete, kConcreteDark);
    TriMesh m;
    if (lod != 0) return m;
    const int n = std::max(1, std::min(kFlatsMaxFloors, floors));
    std::vector<std::vector<Vec3>> rings;
    std::vector<Colour> bands;
    auto ring = [](double z) {
        return std::vector<Vec3>{{-0.5, -0.5, z}, {0.5, -0.5, z}, {0.5, 0.5, z}, {-0.5, 0.5, z}};
    };
    const double storey = 1.0 / n;
    rings.push_back(ring(-0.5));
    for (int f = 0; f < n; ++f) {
        const double z0 = -0.5 + f * storey;
        rings.push_back(ring(z0 + 0.55 * storey));
        bands.push_back(kConcrete);
        rings.push_back(ring(f + 1 == n ? 0.5 : z0 + storey));
        bands.push_back(kWindowBand);
    }
    SolidBuilder b(m);
    b.Loft(rings, bands, kConcrete, kConcreteDark);
    return m;
}

// An industrial hall on the unit cube (LOD 0-1): three bays under a sawtooth roof (the north lights' glazing upright),
// the teeth's tops at the box's top. The bays overlap a little so that no two share an edge.
inline TriMesh IndustrialMesh(int lod = 0) {
    using namespace map_mesh_detail;
    if (lod == 1) return UnitBox(kConcrete, kHallRoof);
    TriMesh m;
    if (lod != 0) return m;
    SolidBuilder b(m);
    const int bays = 3;
    const double low = 0.15;     // z of each tooth's low edge
    const double overlap = 0.004;
    for (int k = 0; k < bays; ++k) {
        const double x0 = std::max(-0.5, -0.5 + k / static_cast<double>(bays) - overlap);
        const double x1 = std::min(0.5, -0.5 + (k + 1) / static_cast<double>(bays) + overlap);
        // The bay's cross-section in (x, z), extruded across y: floor, the glazed upright at x1, the roof's slope.
        b.Begin(Vec3{0.5 * (x0 + x1), 0.0, 0.0});
        const Vec3 a0{x0, -0.5, -0.5}, a1{x1, -0.5, -0.5}, a2{x1, -0.5, 0.5}, a3{x0, -0.5, low};
        const Vec3 c0{x0, 0.5, -0.5}, c1{x1, 0.5, -0.5}, c2{x1, 0.5, 0.5}, c3{x0, 0.5, low};
        b.Face({a0, a1, a2, a3}, kConcrete);
        b.Face({c0, c1, c2, c3}, kConcrete);
        b.Face({a0, a1, c1, c0}, kConcreteDark);
        b.Face({a1, a2, c2, c1}, kGlazing);
        b.Face({a2, a3, c3, c2}, kHallRoof);
        b.Face({a3, a0, c0, c3}, kConcrete);
    }
    return m;
}

// The procedural mesh of a part by its asset register key (scenery/asset_register.hpp), its variant and LOD; empty for
// an unknown key or past the last LOD. For "flats" the variant is the number of floors; trees ignore it.
inline TriMesh MapPartMesh(const std::string& part, int variant, int lod) {
    for (int k = 0; k < static_cast<int>(TreeKind::Count); ++k) {
        if (part == std::string("tree_") + TreeKindName(static_cast<TreeKind>(k))) return TreeMesh(static_cast<TreeKind>(k), lod);
    }
    if (part == "canopy_mass") return lod == 0 ? CanopyMassMesh(variant) : TriMesh{};
    if (part == "house") return HouseMesh(variant, lod);
    if (part == "barn") return BarnMesh(lod);
    if (part == "flats") return FlatsMesh(variant, lod);
    if (part == "industrial") return IndustrialMesh(lod);
    return TriMesh{};
}

// Every part key MapPartMesh builds: the parts a real asset may replace.
inline std::vector<std::string> MapMeshParts() {
    std::vector<std::string> out;
    for (int k = 0; k < static_cast<int>(TreeKind::Count); ++k) out.push_back(std::string("tree_") + TreeKindName(static_cast<TreeKind>(k)));
    for (const char* p : {"canopy_mass", "house", "barn", "flats", "industrial"}) out.push_back(p);
    return out;
}

// A bridge over a crossing: its deck from end a to end b (east, north and the deck's top, metres, in the map's frame),
// `width_m` wide, over a bed (the river's or the road's bottom) at `bed_m`; the mesh relative to end a (x east, y north,
// z up). A deck 0.8 m deep, parapets 1 m high along both edges, and piers at most 28 m apart down into the bed (none
// when the bed is within 0.5 m of the deck's underside). Empty for a crossing shorter than a metre.
inline TriMesh BridgeMesh(const Vec3& end_a, const Vec3& end_b, double bed_m, double width_m) {
    using namespace map_mesh_detail;
    TriMesh m;
    const Vec3 span = end_b - end_a;
    const double length = std::sqrt(span.x * span.x + span.y * span.y);
    if (length < 1.0 || width_m <= 0.0) return m;
    SolidBuilder b(m);
    const Vec3 flat_along{span.x / length, span.y / length, 0.0};
    const Vec3 across{-flat_along.y, flat_along.x, 0.0};
    const Vec3 up{0.0, 0.0, 1.0};
    const double deck_depth = 0.8, parapet_h = 1.0, parapet_w = 0.3, sink = 0.05;
    // The deck (and the parapets on it) follow the slope from end a to end b: a box sheared along its length.
    auto sloped = [&](double off_across, double half_w, double z_lo, double z_hi, const Colour& sides, const Colour& top) {
        std::vector<Vec3> lo, hi;
        const double sx[4] = {0.0, 1.0, 1.0, 0.0};
        const double sy[4] = {-1.0, -1.0, 1.0, 1.0};
        for (int i = 0; i < 4; ++i) {
            const Vec3 p = span * sx[i] + across * (off_across + sy[i] * half_w);
            lo.push_back(p + up * z_lo);
            hi.push_back(p + up * z_hi);
        }
        b.Loft({lo, hi}, {sides}, sides, top);
    };
    sloped(0.0, 0.5 * width_m, -deck_depth, 0.0, kConcrete, kConcreteDark);
    for (const double side : {-1.0, 1.0}) {
        // 1 cm in from the deck's edge, so the parapet's face and the deck's are not one plane.
        sloped(side * (0.5 * width_m - 0.5 * parapet_w - 0.01), 0.5 * parapet_w, -sink, parapet_h, kConcrete, kConcrete);
    }
    const int spans = static_cast<int>(std::ceil(length / kBridgePierSpacingM));
    for (int k = 1; k < spans; ++k) {
        const double f = static_cast<double>(k) / spans;
        const Vec3 at = span * f;
        const double underside = at.z - deck_depth;
        const double bed = bed_m - end_a.z;
        if (underside - bed < 0.5) continue;  // no room under the deck for a pier
        const double top = underside + sink;  // into the deck's underside
        const double bottom = bed - 1.0;      // a metre into the bed
        b.Box(Vec3{at.x, at.y, 0.5 * (top + bottom)}, flat_along, across, up, 0.6, 0.4 * width_m, 0.5 * (top - bottom), kConcreteDark,
              kConcreteDark, kConcreteDark);
    }
    return m;
}

}  // namespace scenery
}  // namespace simcore
