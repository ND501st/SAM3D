// simcore/farm_fields.hpp — the plain's fields as a closed-form function of position (roadmap T4, docs/scenery/T3_T4_PLAN.md).
//
// The map is cut into estates, squares of `estate_m`, each a collective farm's land with its fields' heading, their width
// and a draw. Across the heading a field is the estate's width; along it each row of fields has its own length and offset.
// A field's edges are straight at every distance, an estate's border cuts the fields it crosses, and the number a point
// gets depends on the point and the grid alone: every client, the land cover raster, the farm tracks the map lays along
// the fields' sides and the ground material's HLSL (which repeats Hash32 and FieldHash, pinned by tests) find the same
// fields.
//
// Standard library only: the map (terrain_map.hpp) and the scenery both read it.
#pragma once

#include <cmath>
#include <cstdint>

namespace simcore {

// One 32-bit integer hash for the fields, written the same in the ground material's HLSL (uint, no 64-bit): lowbias32.
inline std::uint32_t Hash32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// A draw for a seed and two whole numbers (an estate's square, a field's row and column), wrapping as unsigned ints.
inline std::uint32_t FieldHash(std::uint32_t seed, std::int32_t a, std::int32_t b) {
    return Hash32(seed ^ Hash32(static_cast<std::uint32_t>(a) * 0x8CB92BA7u + Hash32(static_cast<std::uint32_t>(b) + 0x9E3779B9u)));
}

// A number in [0, 1) from a hash's top 24 bits (exact in a float).
inline double Unit24(std::uint32_t h) { return static_cast<double>(h >> 8) * (1.0 / 16777216.0); }

inline std::uint32_t Seed32(std::uint64_t seed) { return static_cast<std::uint32_t>(seed ^ (seed >> 32)); }

// The fields' rules: the map's "farmland" block (MapDef::farmland) and its seed.
struct FieldGrid {
    std::uint32_t seed = 0;
    double estate_m = 4000.0;
    double length_min_m = 1200.0, length_max_m = 2000.0;  // along the furrows (with the width, 84-200 ha: a TSZ's)
    double width_min_m = 700.0, width_max_m = 1000.0;     // across them
};

struct FieldEstate {
    double heading_rad = 0.0;  // the furrows' way in the estate, clockwise from north, in [0, pi)
    double width_m = 0.0;
    std::uint32_t draw = 0;
};

inline FieldEstate EstateAt(const FieldGrid& g, std::int32_t ix, std::int32_t iy) {
    constexpr double kHalfTurn = 3.14159265358979323846;
    FieldEstate s;
    s.draw = FieldHash(g.seed, ix, iy);
    s.heading_rad = Unit24(s.draw) * kHalfTurn;
    s.width_m = g.width_min_m + (g.width_max_m - g.width_min_m) * Unit24(Hash32(s.draw ^ 0x1u));
    return s;
}

// The field of farmland (east, north) lies in, in the map's frame.
struct FieldCell {
    std::uint64_t id = 0;       // the field's number (the high word's bits are its shade)
    double heading_rad = 0.0;   // the furrows' way, clockwise from north, in [0, pi)
    double centre_e = 0.0, centre_n = 0.0;
    double length_m = 0.0, width_m = 0.0;
};

inline FieldCell FieldAt(const FieldGrid& g, double e, double n) {
    const FieldEstate est = EstateAt(g, static_cast<std::int32_t>(std::floor(e / g.estate_m)), static_cast<std::int32_t>(std::floor(n / g.estate_m)));
    FieldCell f;
    f.heading_rad = est.heading_rad;
    f.width_m = est.width_m;
    const double sh = std::sin(est.heading_rad), ch = std::cos(est.heading_rad);
    const double u = e * sh + n * ch, v = e * ch - n * sh;
    const std::int32_t row = static_cast<std::int32_t>(std::floor(v / est.width_m));
    const std::uint32_t rs = FieldHash(est.draw, row, 0x51);
    f.length_m = g.length_min_m + (g.length_max_m - g.length_min_m) * Unit24(rs);
    const double offset = Unit24(Hash32(rs ^ 0x2u)) * f.length_m;
    const std::int32_t col = static_cast<std::int32_t>(std::floor((u + offset) / f.length_m));
    const std::uint32_t id32 = FieldHash(rs, col, 0x77);
    f.id = (static_cast<std::uint64_t>(id32) << 32) | Hash32(id32 ^ 0x9E3779B9u);
    const double uc = (col + 0.5) * f.length_m - offset, vc = (row + 0.5) * est.width_m;
    f.centre_e = uc * sh + vc * ch;
    f.centre_n = uc * ch - vc * sh;
    return f;
}

// The way the furrows run at (east, north): radians clockwise from north, in [0, pi).
inline double FieldHeading(const FieldGrid& g, double e, double n) { return FieldAt(g, e, n).heading_rad; }

// The parcel of farmland (east, north) lies in, as a number (FieldAt's id).
inline std::uint64_t FieldParcel(const FieldGrid& g, double e, double n) { return FieldAt(g, e, n).id; }

}  // namespace simcore
