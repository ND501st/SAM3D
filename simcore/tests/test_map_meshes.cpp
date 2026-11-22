// The map's dressing as meshes (simcore/scenery/map_meshes.hpp, roadmap R6): every tree, wood mass, building and bridge is
// a set of closed convex solids wound outward, fits its frame, and the buildings' tops are the boxes' tops.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "simcore/scenery.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::scenery;

namespace {

// The volume a closed mesh holds, from its triangles alone: the sum of a . (b x c) / 6 (positive when wound outward).
double SignedVolume(const TriMesh& m) {
    double v = 0.0;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const Vec3& a = m.positions[m.indices[t]];
        const Vec3& b = m.positions[m.indices[t + 1]];
        const Vec3& c = m.positions[m.indices[t + 2]];
        v += Dot(a, Cross(b, c)) / 6.0;
    }
    return v;
}

using Key = std::tuple<long long, long long, long long>;

Key Quantised(const Vec3& p) {
    return Key{std::llround(p.x * 1e6), std::llround(p.y * 1e6), std::llround(p.z * 1e6)};
}

// Edges that are not used by exactly two triangles, once each way (by position: each face has its own vertices).
int OpenEdges(const TriMesh& m) {
    std::map<std::pair<Key, Key>, int> directed;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        for (int e = 0; e < 3; ++e) {
            const Key a = Quantised(m.positions[m.indices[t + static_cast<std::size_t>(e)]]);
            const Key b = Quantised(m.positions[m.indices[t + static_cast<std::size_t>((e + 1) % 3)]]);
            ++directed[{a, b}];
        }
    }
    int bad = 0;
    for (const auto& kv : directed) {
        const auto back = directed.find({kv.first.second, kv.first.first});
        bad += (kv.second == 1 && back != directed.end() && back->second == 1) ? 0 : 1;
    }
    return bad;
}

// Unit normals, one normal and colour per vertex, indices in range, colours in 0..1, each triangle's face along its
// vertices' normals.
int MeshFaults(const TriMesh& m) {
    if (m.normals.size() != m.positions.size() || m.colours.size() != m.positions.size() || m.indices.size() % 3 != 0) return 1;
    int bad = 0;
    for (const Vec3& n : m.normals) bad += std::fabs(n.Length() - 1.0) > 1e-6 ? 1 : 0;
    for (const std::uint32_t i : m.indices) bad += i >= m.positions.size() ? 1 : 0;
    if (bad > 0) return bad;
    for (const auto& c : m.colours) {
        for (const float v : c) bad += (v >= 0.0f && v <= 1.0f) ? 0 : 1;
    }
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const std::uint32_t a = m.indices[t], b = m.indices[t + 1], c = m.indices[t + 2];
        const Vec3 f = Cross(m.positions[b] - m.positions[a], m.positions[c] - m.positions[a]);
        bad += Dot(f, m.normals[a] + m.normals[b] + m.normals[c]) <= 0.0 ? 1 : 0;
    }
    return bad;
}

bool Sound(const TriMesh& m) { return !m.Empty() && OpenEdges(m) == 0 && MeshFaults(m) == 0 && SignedVolume(m) > 0.0; }

}  // namespace

SC_TEST("map meshes: every part, variant and LOD is closed, wound outward and has volume") {
    int meshes = 0;
    for (const std::string& part : MapMeshParts()) {
        for (int variant = 0; variant < 6; ++variant) {
            for (int lod = 0; lod < kTreeLods; ++lod) {
                const TriMesh m = MapPartMesh(part, part == "flats" ? variant + 1 : variant, lod);
                if (m.Empty()) continue;
                ++meshes;
                SC_EXPECT(OpenEdges(m) == 0);
                SC_EXPECT(MeshFaults(m) == 0);
                SC_EXPECT(SignedVolume(m) > 0.0);
            }
        }
        SC_EXPECT(!MapPartMesh(part, 0, 0).Empty());
        SC_EXPECT(MapPartMesh(part, 0, kTreeLods).Empty());
    }
    SC_EXPECT(meshes > 60);
    SC_EXPECT(MapPartMesh("windmill", 0, 0).Empty());
    // Deterministic: the same mesh each time (clients rebuild them).
    const TriMesh a = MapPartMesh("house", 2, 0), b = MapPartMesh("house", 2, 0);
    SC_EXPECT(a.positions.size() == b.positions.size() && a.indices == b.indices);
    SC_EXPECT(a.positions.back().x == b.positions.back().x && a.positions.back().z == b.positions.back().z);
}

SC_TEST("map meshes: trees stand on their base, one unit tall, the crown one unit across") {
    for (int k = 0; k < static_cast<int>(TreeKind::Count); ++k) {
        for (int lod = 0; lod < kTreeLods; ++lod) {
            const TriMesh m = TreeMesh(static_cast<TreeKind>(k), lod);
            SC_EXPECT(Sound(m));
            Vec3 lo, hi;
            m.Bounds(lo, hi);
            SC_EXPECT_NEAR(hi.z, 1.0, 1e-9);
            SC_EXPECT(lo.x >= -0.5 - 1e-9 && hi.x <= 0.5 + 1e-9 && lo.y >= -0.5 - 1e-9 && hi.y <= 0.5 + 1e-9);
            SC_EXPECT(hi.x >= 0.35);  // the crown fills its unit width (a 4-gon's corner reach at LOD 2-3)
            if (lod < 3) {
                SC_EXPECT_NEAR(lo.z, 0.0, 1e-9);  // the trunk to the ground
            } else {
                SC_EXPECT(lo.z > 0.05);  // the crown alone from afar
            }
        }
        // Fewer triangles down the LODs.
        SC_EXPECT(TreeMesh(static_cast<TreeKind>(k), 0).Triangles() > TreeMesh(static_cast<TreeKind>(k), 1).Triangles());
        SC_EXPECT(TreeMesh(static_cast<TreeKind>(k), 2).Triangles() > TreeMesh(static_cast<TreeKind>(k), 3).Triangles());
    }
    SC_EXPECT(TreeMesh(TreeKind::Oak, -1).Empty() && TreeMesh(TreeKind::Oak, kTreeLods).Empty());
    SC_EXPECT(std::string(TreeKindName(TreeKind::Poplar)) == "poplar");
}

SC_TEST("map meshes: buildings and wood masses fill the unit cube, the top the box's top") {
    std::vector<TriMesh> on_cube;
    for (int v = 0; v < kHouseVariants; ++v) on_cube.push_back(HouseMesh(v));
    for (int v = 0; v < kCanopyMassVariants; ++v) on_cube.push_back(CanopyMassMesh(v));
    on_cube.push_back(BarnMesh());
    on_cube.push_back(IndustrialMesh());
    for (const int floors : {1, 5, 11}) on_cube.push_back(FlatsMesh(floors));
    on_cube.push_back(HouseMesh(0, 1));
    on_cube.push_back(FlatsMesh(5, 1));
    for (const TriMesh& m : on_cube) {
        SC_EXPECT(Sound(m));
        Vec3 lo, hi;
        m.Bounds(lo, hi);
        SC_EXPECT(std::fabs(hi.z - 0.5) <= 0.01);  // the house's top is the box's top (1 cm on a 1 m cube: a fraction)
        SC_EXPECT_NEAR(lo.z, -0.5, 1e-9);
        SC_EXPECT(lo.x >= -0.5 - 1e-9 && hi.x <= 0.5 + 1e-9 && lo.y >= -0.5 - 1e-9 && hi.y <= 0.5 + 1e-9);
        SC_EXPECT(SignedVolume(m) > 0.5 && SignedVolume(m) <= 1.0 + 1e-9);
    }
    // A house's walls are 0.68 of its height: its eaves at -0.5 + 0.68, the rest roof up to the ridge.
    for (int v = 0; v < kHouseVariants; ++v) {
        const TriMesh m = HouseMesh(v);
        bool eave = false;
        for (const Vec3& p : m.positions) eave = eave || std::fabs(p.z - (kHouseWallShare - 0.5)) < 1e-9;
        SC_EXPECT(eave);
        // A gable house holds the box below its eaves and half the roof's prism: 0.68 + 0.32 / 2.
        if (v == 0 || v == 1) SC_EXPECT_NEAR(SignedVolume(m), 0.68 + 0.16, 1e-9);
    }
    // Variant 1 is variant 0 turned: its ridge across (along y).
    {
        Vec3 lo, hi;
        const TriMesh m = HouseMesh(1);
        double ridge_extent_x = 0.0, ridge_extent_y = 0.0;
        for (const Vec3& p : m.positions) {
            if (std::fabs(p.z - 0.5) < 1e-9) {
                ridge_extent_x = std::max(ridge_extent_x, std::fabs(p.x));
                ridge_extent_y = std::max(ridge_extent_y, std::fabs(p.y));
            }
        }
        m.Bounds(lo, hi);
        SC_EXPECT(ridge_extent_x < 1e-9 && ridge_extent_y > 0.49);
    }
    // Variants wrap round.
    SC_EXPECT(HouseMesh(5).positions.size() == HouseMesh(1).positions.size() && HouseMesh(5).positions[7].x == HouseMesh(1).positions[7].x);
    SC_EXPECT(HouseMesh(0, 2).Empty() && BarnMesh(2).Empty());
}

SC_TEST("map meshes: a block of flats has a band of windows a floor, between 1 and 24 floors") {
    for (const int floors : {1, 5, 24, 0, 99}) {
        const TriMesh m = FlatsMesh(floors);
        SC_EXPECT(Sound(m));
        const int n = std::max(1, std::min(kFlatsMaxFloors, floors));
        std::set<long long> levels;
        for (const Vec3& p : m.positions) levels.insert(std::llround(p.z * 1e6));
        SC_EXPECT(static_cast<int>(levels.size()) == 2 * n + 1);
    }
}

SC_TEST("map meshes: a bridge's deck runs end to end on piers down into the bed") {
    // A 100 m crossing, north-east, rising 2 m; the river's bed 8 m below.
    const Vec3 a{1000.0, 2000.0, 105.0};
    const Vec3 b{1060.0, 2080.0, 107.0};
    const TriMesh m = BridgeMesh(a, b, 97.0, 9.0);
    SC_EXPECT(Sound(m));
    Vec3 lo, hi;
    m.Bounds(lo, hi);
    SC_EXPECT_NEAR(lo.z, 97.0 - 105.0 - 1.0, 1e-9);  // the piers a metre into the bed
    SC_EXPECT_NEAR(hi.z, 2.0 + 1.0, 1e-9);            // the parapet at end b
    // The deck reaches both ends: vertices at x, y = 0 and at b - a (give or take half its width across).
    SC_EXPECT(hi.x >= 60.0 && hi.y >= 80.0 && lo.x <= 0.0 && lo.y <= 0.0);
    SC_EXPECT(hi.x <= 60.0 + 4.5 + 1e-9 && lo.x >= -4.5 - 1e-9);
    // Deck and two parapets are 3 boxes of 12 triangles; ceil(100 / 28) = 4 spans, 3 piers.
    SC_EXPECT(m.Triangles() == (3 + 3) * 12);
    // A level crossing over a bed just under its deck (0.8 m deep): no piers.
    const TriMesh low = BridgeMesh(a, Vec3{1060.0, 2080.0, 105.0}, 104.0, 9.0);
    SC_EXPECT(Sound(low));
    SC_EXPECT(low.Triangles() == 3 * 12);
    SC_EXPECT(BridgeMesh(a, a + Vec3{0.5, 0.0, 0.0}, 97.0, 9.0).Empty());
    SC_EXPECT(BridgeMesh(a, b, 97.0, 0.0).Empty());
}
