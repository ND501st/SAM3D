// simcore/scenery/trimesh.hpp — a triangle mesh as plain arrays, the form every procedural model in simcore::scenery is
// built in (the munitions, scenery/munitions.hpp; the map's trees, buildings and bridges, scenery/map_meshes.hpp) and
// the form Unreal turns into its own static meshes (FSimRuntimeMesh).
//
// The convention: metres; every triangle winds so that (b - a) x (c - a), the cross product of the coordinates taken as
// numbers, points out of the surface (along its vertices' normals). An engine whose front faces wind the other way swaps
// two indices of each triangle. Colours are linear RGB in 0..1.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "simcore/vec3.hpp"

namespace simcore {
namespace scenery {

// A triangle mesh: positions and normals per vertex, a linear-RGB colour per vertex, three indices a triangle.
struct TriMesh {
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<std::array<float, 3>> colours;
    std::vector<std::uint32_t> indices;

    std::size_t Triangles() const { return indices.size() / 3; }
    bool Empty() const { return indices.empty(); }
    // The box round every vertex (both zero when empty).
    void Bounds(Vec3& lo, Vec3& hi) const {
        lo = Vec3{};
        hi = Vec3{};
        for (std::size_t i = 0; i < positions.size(); ++i) {
            const Vec3& p = positions[i];
            if (i == 0) lo = hi = p;
            lo = Vec3{std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
            hi = Vec3{std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
        }
    }
};

}  // namespace scenery
}  // namespace simcore
