// simcore/noise.hpp — repeatable 2D value noise for generated ground and cloud fields.
//
// Hash-based lattice noise (no tables, no state): the same (seed, x, y) always gives the same value on every machine,
// so the sim and the picture agree on where a wood or a cloud is. The lattice hash is SplitMix64 (G. Steele, D. Lea,
// C. Flood, OOPSLA 2014); between lattice points the values blend with a smoothstep.
#pragma once

#include <cmath>
#include <cstdint>

namespace simcore {
namespace noise {

inline std::uint64_t Mix64(std::uint64_t z) {
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// A number in [0, 1) for the lattice point (ix, iy) of field `seed`.
inline double Lattice(std::uint64_t seed, std::int64_t ix, std::int64_t iy) {
    const std::uint64_t h = Mix64(seed ^ Mix64(static_cast<std::uint64_t>(ix) * 0x8CB92BA72F3D8DD7ull ^
                                               Mix64(static_cast<std::uint64_t>(iy))));
    return static_cast<double>(h >> 11) * (1.0 / 9007199254740992.0);
}

inline double Smooth(double t) { return t * t * (3.0 - 2.0 * t); }

// Value noise in [0, 1) with features about `cell` units across.
inline double Value(std::uint64_t seed, double x, double y, double cell) {
    const double fx = x / cell, fy = y / cell;
    const double x0 = std::floor(fx), y0 = std::floor(fy);
    const auto ix = static_cast<std::int64_t>(x0), iy = static_cast<std::int64_t>(y0);
    const double tx = Smooth(fx - x0), ty = Smooth(fy - y0);
    const double a = Lattice(seed, ix, iy), b = Lattice(seed, ix + 1, iy);
    const double c = Lattice(seed, ix, iy + 1), d = Lattice(seed, ix + 1, iy + 1);
    return (a * (1.0 - tx) + b * tx) * (1.0 - ty) + (c * (1.0 - tx) + d * tx) * ty;
}

// Fractal sum of `octaves` value noises, each half the size and half the weight of the last; in [0, 1).
inline double Fbm(std::uint64_t seed, double x, double y, double cell, int octaves) {
    double sum = 0.0, weight = 1.0, total = 0.0, size = cell;
    for (int i = 0; i < octaves; ++i) {
        sum += weight * Value(seed + static_cast<std::uint64_t>(i) * 0x51ED27ull, x, y, size);
        total += weight;
        weight *= 0.5;
        size *= 0.5;
    }
    return total > 0.0 ? sum / total : 0.0;
}

// A repeatable random stream (SplitMix64) for generators that place things one after another.
class Stream {
public:
    explicit Stream(std::uint64_t seed) : state_(seed) {}
    std::uint64_t Next() {
        state_ += 0x9E3779B97F4A7C15ull;
        std::uint64_t z = state_;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    double Uniform() { return static_cast<double>(Next() >> 11) * (1.0 / 9007199254740992.0); }
    double Range(double lo, double hi) { return lo + (hi - lo) * Uniform(); }
    bool Chance(double p) { return Uniform() < p; }

private:
    std::uint64_t state_;
};

}  // namespace noise
}  // namespace simcore
