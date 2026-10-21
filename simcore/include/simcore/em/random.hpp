// simcore/em/random.hpp — counter-based random numbers for the electromagnetic layer.
//
// Every random draw is a pure function of (scenario seed, stream, keys). A detection draw for
// radar 3 against platform 17 on scan 412 is the same number whatever order the radars are
// evaluated in, whether the work is split across threads, or how the frame time is batched.
// That keeps runs reproducible (replays, tests, lockstep multiplayer) without a shared,
// order-sensitive generator.
//
// The mixer is SplitMix64's finaliser (Steele, Lea & Flood, "Fast splittable pseudorandom number
// generators", OOPSLA 2014). The Python prototype (prototype/samproto/em.py) implements the same
// functions bit for bit, so both give identical draws.
#pragma once

#include <cmath>
#include <cstdint>

#include "simcore/vec3.hpp"

namespace simcore {
namespace em {

// Independent streams, so e.g. the range noise and the azimuth noise of one measurement differ.
enum class Stream : std::uint64_t {
    Detection = 1,
    RangeNoise = 2,
    AzimuthNoise = 3,
    ElevationNoise = 4,
    EsmBearing = 5,
    EsmFrequency = 6,
    EsmPri = 7,
    EsmPulseWidth = 8,
    CommsDelivery = 9,
    Iff = 10,
    FalseAlarm = 11,
    TdoaNoise = 12,
    Doctrine = 13,
    ArmSeeker = 14
};

inline std::uint64_t Mix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

// Hash of a seed, a stream and up to four keys (ids, scan numbers, tick numbers).
inline std::uint64_t DrawHash(std::uint64_t seed, Stream stream, std::uint64_t a, std::uint64_t b = 0,
                              std::uint64_t c = 0, std::uint64_t d = 0) {
    std::uint64_t h = Mix64(seed ^ static_cast<std::uint64_t>(stream) * 0xD1B54A32D192ED03ull);
    h = Mix64(h ^ a);
    h = Mix64(h ^ b);
    h = Mix64(h ^ c);
    h = Mix64(h ^ d);
    return h;
}

// Uniform in [0, 1) from the top 53 bits.
inline double Uniform01(std::uint64_t h) { return static_cast<double>(h >> 11) * (1.0 / 9007199254740992.0); }

// Standard normal (Box–Muller) from one hash.
inline double StandardNormal(std::uint64_t h) {
    double u1 = Uniform01(Mix64(h ^ 0x5851F42D4C957F2Dull));
    const double u2 = Uniform01(Mix64(h ^ 0x14057B7EF767814Full));
    if (u1 < 1e-300) u1 = 1e-300;
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(kTwoPi * u2);
}

// Convenience wrapper holding the scenario seed.
class Random {
public:
    explicit Random(std::uint64_t seed = 1) : seed_(seed) {}
    std::uint64_t Seed() const { return seed_; }

    double Uniform(Stream s, std::uint64_t a, std::uint64_t b = 0, std::uint64_t c = 0, std::uint64_t d = 0) const {
        return Uniform01(DrawHash(seed_, s, a, b, c, d));
    }
    double Normal(Stream s, std::uint64_t a, std::uint64_t b = 0, std::uint64_t c = 0, std::uint64_t d = 0) const {
        return StandardNormal(DrawHash(seed_, s, a, b, c, d));
    }
    // Draws from a key built elsewhere (e.g. one key per look, several streams per key).
    double Uniform01Of(Stream s, std::uint64_t key) const { return Uniform01(DrawHash(seed_, s, key)); }
    double NormalOf(Stream s, std::uint64_t key) const { return StandardNormal(DrawHash(seed_, s, key)); }

    // True with probability p.
    bool Chance(double p, Stream s, std::uint64_t a, std::uint64_t b = 0, std::uint64_t c = 0,
                std::uint64_t d = 0) const {
        return Uniform(s, a, b, c, d) < p;
    }

private:
    std::uint64_t seed_;
};

// Ids are ints in the sim; widen them for hashing without sign surprises.
inline std::uint64_t Key(int v) { return static_cast<std::uint64_t>(static_cast<std::uint32_t>(v)); }
inline std::uint64_t Key(long long v) { return static_cast<std::uint64_t>(v); }

}  // namespace em
}  // namespace simcore
