// simcore/console_sync.hpp — the vehicle's console as the server holds it, mirrored on each player's machine
// (multiplayer crew, roadmap 10.x). The server runs the one simulation; a client keeps the same scenario loaded but never
// steps it, and overwrites its console (every control position and wheel value, every subsystem's state and progress,
// every sim input) from these snapshots, so the cabin, lamps and gauges it draws are the server's. Players move
// controls through the server (ScenarioRunner::SeatSet), never on their own copy.
//
// A snapshot is small (a few hundred bytes for the Osa) and complete, so it is sent unreliably and the newest wins.
// A fingerprint of the console's control, subsystem and input ids guards against two machines with different files.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"

namespace simcore {

// FNV-1a over the ids of every control, subsystem and input, in order.
inline std::uint32_t ConsoleFingerprint(const ConsoleData& c) {
    std::uint32_t h = 2166136261u;
    auto mix = [&h](const std::string& text) {
        for (char ch : text) {
            h ^= static_cast<std::uint8_t>(ch);
            h *= 16777619u;
        }
        h ^= 0xffu;
        h *= 16777619u;
    };
    for (const ControlDef& d : c.panel.Controls()) mix(d.id);
    for (const std::string& id : c.equipment.UnitIds()) mix(id);
    for (const auto& in : c.equipment.Inputs()) mix(in.first);
    return h;
}

namespace console_wire {

constexpr std::uint32_t kMagic = 0x31534343u;  // "CCS1"

inline void U8(std::vector<std::uint8_t>& b, std::uint32_t v) { b.push_back(static_cast<std::uint8_t>(v & 0xffu)); }
inline void U16(std::vector<std::uint8_t>& b, std::uint32_t v) {
    U8(b, v);
    U8(b, v >> 8);
}
inline void U32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    U16(b, v);
    U16(b, v >> 16);
}

struct In {
    const std::vector<std::uint8_t>& b;
    std::size_t pos = 0;
    bool ok = true;
    std::uint32_t U8() {
        if (pos >= b.size()) {
            ok = false;
            return 0;
        }
        return b[pos++];
    }
    std::uint32_t U16() {
        const std::uint32_t lo = U8();
        return lo | (U8() << 8);
    }
    std::uint32_t U32() {
        const std::uint32_t lo = U16();
        return lo | (U16() << 16);
    }
};

}  // namespace console_wire

// The console's whole state: positions (one byte each, positions < 255), wheel values (1e-4 of a unit, 32 bits),
// subsystems (state, progress in 1/65535, latched), inputs (one bit each).
inline std::vector<std::uint8_t> EncodeConsoleState(const ConsoleData& c, std::uint32_t seq, double time_s) {
    using namespace console_wire;
    std::vector<std::uint8_t> b;
    U32(b, kMagic);
    U32(b, ConsoleFingerprint(c));
    U32(b, seq);
    const std::int64_t ms = static_cast<std::int64_t>(std::llround(time_s * 1000.0));
    U32(b, static_cast<std::uint32_t>(static_cast<std::uint64_t>(ms) & 0xffffffffu));
    U32(b, static_cast<std::uint32_t>(static_cast<std::uint64_t>(ms) >> 32));
    const std::vector<int>& pos = c.panel.Positions();
    const std::vector<double>& val = c.panel.Values();
    U16(b, static_cast<std::uint32_t>(pos.size()));
    for (std::size_t i = 0; i < pos.size(); ++i) {
        U8(b, static_cast<std::uint32_t>(pos[i] < 0 ? 255 : (pos[i] > 254 ? 254 : pos[i])));
        if (pos[i] < 0) U32(b, static_cast<std::uint32_t>(static_cast<std::int32_t>(std::lround(val[i] * 10000.0))));
    }
    const auto units = c.equipment.UnitStates();
    U16(b, static_cast<std::uint32_t>(units.size()));
    for (const auto& u : units) {
        U8(b, static_cast<std::uint32_t>(u.state) | (u.latched ? 0x80u : 0u));
        const double pr = u.progress < 0.0 ? 0.0 : (u.progress > 1.0 ? 1.0 : u.progress);
        U16(b, static_cast<std::uint32_t>(std::lround(pr * 65535.0)));
    }
    const auto inputs = c.equipment.Inputs();
    U16(b, static_cast<std::uint32_t>(inputs.size()));
    std::uint32_t bits = 0;
    int nbits = 0;
    for (const auto& in : inputs) {
        bits |= (in.second ? 1u : 0u) << nbits;
        if (++nbits == 8) {
            U8(b, bits);
            bits = 0;
            nbits = 0;
        }
    }
    if (nbits > 0) U8(b, bits);
    return b;
}

// Apply a snapshot to a console loaded from the same file. False (console untouched) for a foreign or damaged packet,
// another console's fingerprint, or a snapshot not newer than `*seq` (which is then updated).
inline bool ApplyConsoleState(const std::vector<std::uint8_t>& bytes, ConsoleData& c, std::uint32_t* seq = nullptr,
                              double* time_s = nullptr) {
    using namespace console_wire;
    In in{bytes};
    if (in.U32() != kMagic || in.U32() != ConsoleFingerprint(c)) return false;
    const std::uint32_t n = in.U32();
    if (seq != nullptr && *seq != 0 && n <= *seq) return false;
    const std::uint64_t lo = in.U32();
    const std::uint64_t hi = in.U32();
    const double t = static_cast<double>(static_cast<std::int64_t>(lo | (hi << 32))) / 1000.0;
    std::vector<int> pos = c.panel.Positions();
    std::vector<double> val = c.panel.Values();
    if (in.U16() != pos.size()) return false;
    for (std::size_t i = 0; i < pos.size() && in.ok; ++i) {
        const std::uint32_t p = in.U8();
        if (p == 255) {
            pos[i] = -1;
            val[i] = static_cast<double>(static_cast<std::int32_t>(in.U32())) / 10000.0;
        } else {
            pos[i] = static_cast<int>(p);
        }
    }
    auto units = c.equipment.UnitStates();
    if (in.U16() != units.size()) return false;
    for (auto& u : units) {
        const std::uint32_t s = in.U8();
        if ((s & 0x7fu) > static_cast<std::uint32_t>(SubsystemState::Fault)) return false;
        u.state = static_cast<SubsystemState>(s & 0x7fu);
        u.latched = (s & 0x80u) != 0;
        u.progress = static_cast<double>(in.U16()) / 65535.0;
    }
    auto inputs = c.equipment.Inputs();
    if (in.U16() != inputs.size()) return false;
    std::uint32_t bits = 0;
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        if (i % 8 == 0) bits = in.U8();
        inputs[i].second = ((bits >> (i % 8)) & 1u) != 0;
    }
    if (!in.ok || in.pos != bytes.size()) return false;
    if (!c.panel.ApplyState(pos, val) || !c.equipment.ApplyState(units, inputs)) return false;
    if (seq != nullptr) *seq = n;
    if (time_s != nullptr) *time_s = t;
    return true;
}

}  // namespace simcore
