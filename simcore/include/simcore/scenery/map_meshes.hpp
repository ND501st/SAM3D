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
// the planted pine and the black locust of the shelter belts), then the oak-hornbeam and beech woods' hornbeam and beech,
// the black alder of the wet ground, the field maple of the edges, and the two shrubs of a wood's fringe (hazel, elder):
// docs/scenery/VEGETATION.md. The first four keep their numbers (a kind is an index into the instance lists).
enum class TreeKind { Oak, Poplar, Pine, Acacia, Hornbeam, Beech, Alder, Maple, Hazel, Elder, Count };

// A shrub of a wood's fringe: lower than a tree (data/scenery/map_dressing.json "shrub_height_m"), several stems from the ground.
inline bool IsShrub(TreeKind k) { return k == TreeKind::Hazel || k == TreeKind::Elder; }

inline const char* TreeKindName(TreeKind k) {
    switch (k) {
    case TreeKind::Oak: return "oak";
    case TreeKind::Poplar: return "poplar";
    case TreeKind::Pine: return "pine";
    case TreeKind::Acacia: return "acacia";
    case TreeKind::Hornbeam: return "hornbeam";
    case TreeKind::Beech: return "beech";
    case TreeKind::Alder: return "alder";
    case TreeKind::Maple: return "maple";
    case TreeKind::Hazel: return "hazel";
    case TreeKind::Elder: return "elder";
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
constexpr Colour kGrassBase{0.040f, 0.046f, 0.018f};    // #2e3220 dry grass, the foot of a tuft
constexpr Colour kGrassTip{0.110f, 0.095f, 0.035f};     // #5e5434 its straw-coloured tips
constexpr Colour kStrawFoot{0.100f, 0.075f, 0.028f};    // #4f4128 stubble and a maize stump
constexpr Colour kStrawCut{0.170f, 0.125f, 0.045f};     // #6c5a3a the cut end
// A clod is the furrows' own soil, its top a little drier: brighter ones glowed orange under a low sun (plot 1 a7, 16:00),
// darker ones were grey stones at noon (a8).
constexpr Colour kClodFoot{0.048f, 0.033f, 0.020f};     // #3d3227 a clod's damp underside
constexpr Colour kClodTop{0.110f, 0.076f, 0.044f};      // #5d4d3b its drier top
constexpr Colour kStoverSide{0.300f, 0.240f, 0.150f};   // #95886b a lying maize stalk's weathered side
constexpr Colour kStoverTop{0.420f, 0.350f, 0.220f};    // #ada081 its bleached top
constexpr Colour kReedFoot{0.095f, 0.072f, 0.028f};     // #4d3f28 a reed or a dry stalk
constexpr Colour kReedHead{0.055f, 0.038f, 0.022f};     // #3e3022 its seed head
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
    case TreeKind::Alder:
        profile = {{0.25, 0.3}, {0.55, 0.5}, {0.85, 0.34}, {1.0, 0.0}};
        trunk_top = 0.25;
        trunk_r = 0.04;
        break;
    case TreeKind::Hazel:
    case TreeKind::Elder:
        // A shrub: the crown from near the ground, as wide as it is high.
        profile = {{0.08, 0.36}, {0.5, 0.5}, {0.85, 0.36}, {1.0, 0.0}};
        trunk_top = 0.1;
        trunk_r = 0.05;
        break;
    case TreeKind::Hornbeam:
    case TreeKind::Beech:
    case TreeKind::Maple:
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

// The ground clutter of the camera's ring (roadmap T6.1, scenery/clutter.hpp): a part's mesh at LOD 0-1 (empty beyond),
// base at the origin, unit height, a unit across. "clutter_grass" a tuft of dry grass, "clutter_straw" a stubble clump or a
// maize stump (a short stalk cut flat), "clutter_reed" a reed or a dry stalk with its seed head, "clutter_clod" a clod of
// the plough's (a solid, as a clod is: no card), "clutter_stalk" a maize stalk lying on the stubble. Placeholders: spiky
// solids that read as ground cover from a metre or more.
inline TriMesh ClutterMesh(const std::string& part, int lod) {
    using namespace map_mesh_detail;
    TriMesh m;
    if (lod < 0 || lod > 1) return m;
    SolidBuilder b(m);
    if (part == "clutter_grass") {
        const int sides = lod == 0 ? 5 : 3;
        b.Loft({Ring(sides, 0.0, 0.5), Ring(sides, 0.45, 0.32, 0.3), Ring(sides, 1.0, 0.0)}, {kGrassBase, kGrassTip}, kGrassBase, kGrassTip);
    } else if (part == "clutter_straw") {
        const int sides = lod == 0 ? 6 : 4;
        b.Loft({Ring(sides, 0.0, 0.5), Ring(sides, 1.0, 0.42)}, {kStrawFoot}, kStrawFoot, kStrawCut);
    } else if (part == "clutter_clod") {
        // A clod turned up by the plough: a flat, lumpy solid, its rings turned against each other so no face lines up.
        // Rounded, its widest low down, as if half sunk in the loose soil.
        const int sides = lod == 0 ? 7 : 5;
        b.Loft({Ring(sides, 0.0, 0.47), Ring(sides, 0.35, 0.50, 0.4), Ring(sides, 0.70, 0.40, 0.9), Ring(sides, 0.92, 0.22, 1.3), Ring(sides, 1.0, 0.0, 1.6)},
               {kClodFoot, kClodTop, kClodTop, kClodTop}, kClodFoot, kClodTop);
    } else if (part == "clutter_reed") {
        const int sides = lod == 0 ? 4 : 3;
        b.Loft({Ring(sides, 0.0, 0.5), Ring(sides, 0.7, 0.35), Ring(sides, 0.9, 0.2), Ring(sides, 1.0, 0.0)}, {kReedFoot, kReedFoot, kReedHead}, kReedFoot,
               kReedHead);
    } else if (part == "clutter_stalk") {
        // A maize stalk the harvester cut and threw down, lying along local x (a unit long, 0.028 across, a unit thick, so
        // the instance's width is its length and its height its thickness), broken once so it is not a ruler.
        const Vec3 up{0.0, 0.0, 1.0};
        if (lod == 0) {
            const double turn = 0.26;  // the break, about 15 degrees
            const Vec3 bent{std::cos(turn), std::sin(turn), 0.0}, bent_across{-std::sin(turn), std::cos(turn), 0.0};
            b.Box({-0.23, 0.0, 0.5}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, up, 0.27, 0.014, 0.5, kStoverSide, kStoverTop, kStoverSide);
            b.Box({0.02 + 0.24 * bent.x, 0.24 * bent.y, 0.45}, bent, bent_across, up, 0.24, 0.012, 0.45, kStoverSide, kStoverTop, kStoverSide);
        } else {
            b.Box({0.0, 0.0, 0.5}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, up, 0.5, 0.014, 0.5, kStoverSide, kStoverTop, kStoverSide);
        }
    }
    return m;
}

// The mass of a wood as one solid on the unit cube (variant 0-3): a crown of lumps (a height field of two scales of
// value noise, so the top reads as rounded crowns, not a lid), a ragged outline pulled in at the corners and sides, and
// the foliage's colour in blotches, lighter on the lumps' tops and darker in the hollows and down the sides. Drawn where
// single trees are not (the far ground, a wood's inside). The top's highest point is the box's top.
inline TriMesh CanopyMassMesh(int variant) {
    using namespace map_mesh_detail;
    const int v = ((variant % kCanopyMassVariants) + kCanopyMassVariants) % kCanopyMassVariants;
    constexpr int kN = 14;       // grid cells a side
    constexpr int kSideRows = 3;  // rings down the side
    const std::uint32_t seed = 0x9e3779b1u * static_cast<std::uint32_t>(v + 1);
    auto hash = [&](int x, int y) {
        std::uint32_t h = seed ^ (static_cast<std::uint32_t>(x) * 0x85ebca6bu) ^ (static_cast<std::uint32_t>(y) * 0xc2b2ae35u);
        h ^= h >> 15;
        h *= 0x2c1b3c6du;
        h ^= h >> 12;
        h *= 0x297a2d39u;
        h ^= h >> 15;
        return static_cast<double>(h & 0xffffffu) / 16777215.0;
    };
    auto smooth = [](double t) { return t * t * (3.0 - 2.0 * t); };
    auto noise = [&](double x, double y) {  // value noise 0..1
        const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
        const double fx = smooth(x - ix), fy = smooth(y - iy);
        const double a = hash(ix, iy), b = hash(ix + 1, iy), c = hash(ix, iy + 1), d = hash(ix + 1, iy + 1);
        return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
    };
    auto lump = [&](double x, double y) { return 0.65 * noise(x * 3.0 + 7.0, y * 3.0 + 3.0) + 0.35 * noise(x * 8.0 + 1.0, y * 8.0 + 5.0); };

    TriMesh m;
    const int stride = kN + 1;
    std::vector<double> topz(static_cast<std::size_t>(stride * stride));
    std::vector<double> lumpv(topz.size());
    double zmax = -1e9;
    for (int j = 0; j <= kN; ++j) {
        for (int i = 0; i <= kN; ++i) {
            const double x = -0.5 + static_cast<double>(i) / kN, y = -0.5 + static_cast<double>(j) / kN;
            const double d = std::min(0.5 - std::fabs(x), 0.5 - std::fabs(y));
            const double l = lump(x, y);
            const double rim = smooth(std::min(1.0, d / 0.14));  // the crown falls away to the side
            const double z = 0.12 + 0.30 * l * (0.35 + 0.65 * rim) + 0.08 * rim - 0.16 * (1.0 - rim);
            topz[static_cast<std::size_t>(j * stride + i)] = z;
            lumpv[static_cast<std::size_t>(j * stride + i)] = l;
            zmax = std::max(zmax, z);
        }
    }
    const double lift = 0.5 - zmax;  // the highest crown is the box's top
    // The ragged outline: border vertices pulled in by noise, never out of the box.
    auto xy_at = [&](int i, int j, double& x, double& y) {
        x = -0.5 + static_cast<double>(i) / kN;
        y = -0.5 + static_cast<double>(j) / kN;
        if (i == 0 || i == kN || j == 0 || j == kN) {
            const double pull = 0.05 * noise(x * 6.0 + 11.0, y * 6.0 + 2.0);
            if (i == 0) x += pull;
            if (i == kN) x -= pull;
            if (j == 0) y += pull;
            if (j == kN) y -= pull;
        }
    };
    auto tint = [&](double l, double shade) {  // blotches of the crown's green, lighter on the lumps' tops
        const double t = std::min(1.0, std::max(0.0, (l - 0.25) / 0.5));
        const double f = shade * (0.75 + 0.55 * t);
        return std::array<float, 3>{static_cast<float>(kMassTop[0] * f), static_cast<float>(kMassTop[1] * f), static_cast<float>(kMassTop[2] * f)};
    };
    auto push = [&](double x, double y, double z, const std::array<float, 3>& c) {
        m.positions.push_back(Vec3{x, y, z});
        m.normals.push_back(Vec3{0, 0, 1});
        m.colours.push_back(c);
        return static_cast<std::uint32_t>(m.positions.size() - 1);
    };
    std::vector<std::uint32_t> top(topz.size());
    for (int j = 0; j <= kN; ++j) {
        for (int i = 0; i <= kN; ++i) {
            double x, y;
            xy_at(i, j, x, y);
            const std::size_t k = static_cast<std::size_t>(j * stride + i);
            top[k] = push(x, y, topz[k] + lift, tint(lumpv[k], 1.0));
        }
    }
    for (int j = 0; j < kN; ++j) {
        for (int i = 0; i < kN; ++i) {
            const std::uint32_t a = top[static_cast<std::size_t>(j * stride + i)], b = top[static_cast<std::size_t>(j * stride + i + 1)];
            const std::uint32_t c = top[static_cast<std::size_t>((j + 1) * stride + i + 1)], d = top[static_cast<std::size_t>((j + 1) * stride + i)];
            for (const std::uint32_t t : {a, b, c, a, c, d}) m.indices.push_back(t);
        }
    }
    // The perimeter, counter-clockwise seen from above, and the side's rings down from it.
    std::vector<std::pair<int, int>> loop;
    for (int i = 0; i < kN; ++i) loop.push_back({i, 0});
    for (int j = 0; j < kN; ++j) loop.push_back({kN, j});
    for (int i = kN; i > 0; --i) loop.push_back({i, kN});
    for (int j = kN; j > 0; --j) loop.push_back({0, j});
    const std::size_t n = loop.size();
    std::vector<std::uint32_t> prev(n);
    for (std::size_t q = 0; q < n; ++q) {  // the wall's own copy of the rim: a crease between the crown and the side
        const std::uint32_t t = top[static_cast<std::size_t>(loop[q].second * stride + loop[q].first)];
        prev[q] = push(m.positions[t].x, m.positions[t].y, m.positions[t].z, tint(0.3, 0.8));
    }
    for (int r = 1; r <= kSideRows; ++r) {
        std::vector<std::uint32_t> ring(n);
        for (std::size_t q = 0; q < n; ++q) {
            double x, y;
            xy_at(loop[q].first, loop[q].second, x, y);
            const double top_z = m.positions[prev[q]].z;
            const double base = m.positions[top[static_cast<std::size_t>(loop[q].second * stride + loop[q].first)]].z;
            const double z = (r == kSideRows) ? -0.5 : base + (-0.5 - base) * (static_cast<double>(r) / kSideRows);
            (void)top_z;
            ring[q] = push(x, y, z, tint(0.3, 0.55 + 0.1 * (kSideRows - r)));
        }
        for (std::size_t q = 0; q < n; ++q) {
            const std::size_t w = (q + 1) % n;
            for (const std::uint32_t t : {prev[q], ring[q], ring[w], prev[q], ring[w], prev[w]}) m.indices.push_back(t);
        }
        prev = ring;
    }
    {  // the base, a fan on the last ring, facing down
        const std::uint32_t centre = push(0.0, 0.0, -0.5, tint(0.3, 0.4));
        std::vector<std::uint32_t> base(n);  // its own vertices, so its normals stay flat
        for (std::size_t q = 0; q < n; ++q) base[q] = push(m.positions[prev[q]].x, m.positions[prev[q]].y, -0.5, tint(0.3, 0.4));
        for (std::size_t q = 0; q < n; ++q) {
            const std::size_t w = (q + 1) % n;
            for (const std::uint32_t t : {centre, base[w], base[q]}) m.indices.push_back(t);
        }
    }
    // Smooth normals from the faces.
    std::vector<Vec3> acc(m.positions.size(), Vec3{0, 0, 0});
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const Vec3& p0 = m.positions[m.indices[t]];
        const Vec3& p1 = m.positions[m.indices[t + 1]];
        const Vec3& p2 = m.positions[m.indices[t + 2]];
        const Vec3 e1{p1.x - p0.x, p1.y - p0.y, p1.z - p0.z}, e2{p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
        const Vec3 nrm{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
        for (int k = 0; k < 3; ++k) {
            Vec3& a = acc[m.indices[t + static_cast<std::size_t>(k)]];
            a = Vec3{a.x + nrm.x, a.y + nrm.y, a.z + nrm.z};
        }
    }
    for (std::size_t i = 0; i < acc.size(); ++i) {
        const double len = std::sqrt(acc[i].x * acc[i].x + acc[i].y * acc[i].y + acc[i].z * acc[i].z);
        m.normals[i] = len > 1e-12 ? Vec3{acc[i].x / len, acc[i].y / len, acc[i].z / len} : Vec3{0, 0, 1};
    }
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
    if (part.compare(0, 8, "clutter_") == 0) return ClutterMesh(part, lod);
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
    for (const char* p : {"canopy_mass", "house", "barn", "flats", "industrial", "clutter_grass", "clutter_straw", "clutter_reed", "clutter_clod", "clutter_stalk"}) out.push_back(p);
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
