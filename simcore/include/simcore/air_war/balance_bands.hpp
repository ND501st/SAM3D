// simcore/air_war/balance_bands.hpp — the balance pass's bands (roadmap B14, docs/air_war/09_BALANCE.md section 17).
//
// A band is a measure's acceptable range, with the anchor it was read against (a registered source, or "game value"),
// in data/air_war/balance_bands.json. The benches read them and fail when a measure falls outside: `air_war_bench` the
// measures against history (the strikers' answers, the missiles' rates, the exchanges, the latencies, friendly fire),
// `air_war_balance` the campaign's (loss rates, the battery's fate, the air battle, the on-call latencies) for a
// theatre, a faction and a posture. Nothing in the simulation reads them.
#pragma once

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/json.hpp"

namespace simcore {
namespace airwar {

struct BalanceBand {
    std::string id;        // the measure
    std::string tool;      // "air_war_bench", "air_war_balance" or "own_air_balance" (S1)
    std::string theatre;   // air_war_balance: the theatre file ("" any)
    std::string faction;   // air_war_balance: the faction id ("" any)
    std::string posture;   // air_war_balance: "careless", "careful" or "" (both)
    double lo = 0.0;
    double hi = 0.0;
    std::string source;
    std::string confidence;
    std::string status;  // "" a band the tool gates on; "argued": out of it for a reason 09_BALANCE.md gives (F6.1), printed, never failed
};

struct BalanceBands {
    std::vector<BalanceBand> bands;

    // The band for a measure in a context (the first that matches), or null.
    const BalanceBand* Find(const std::string& tool, const std::string& id, const std::string& theatre = std::string(),
                            const std::string& faction = std::string(), const std::string& posture = std::string()) const {
        for (const BalanceBand& b : bands) {
            if (b.tool != tool || b.id != id) continue;
            if (!b.theatre.empty() && b.theatre != theatre) continue;
            if (!b.faction.empty() && b.faction != faction) continue;
            if (!b.posture.empty() && b.posture != posture) continue;
            return &b;
        }
        return nullptr;
    }
};

inline bool LoadBalanceBands(const std::string& path, BalanceBands& out, std::string* error = nullptr) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    json::Value root;
    std::string e;
    if (!json::Parse(ss.str(), root, &e)) {
        if (error != nullptr) *error = path + ": " + e;
        return false;
    }
    const json::Value* list = root.Find("bands");
    if (list == nullptr || !list->Is(json::Type::Array)) {
        if (error != nullptr) *error = path + ": no \"bands\" list";
        return false;
    }
    BalanceBands b;
    for (std::size_t i = 0; i < list->items.size(); ++i) {
        const json::Value& v = list->items[i];
        BalanceBand x;
        const auto str = [&v](const char* k) {
            const json::Value* s = v.Find(k);
            return s != nullptr && s->Is(json::Type::String) ? s->string : std::string();
        };
        const json::Value* lo = v.Find("lo");
        const json::Value* hi = v.Find("hi");
        x.id = str("id");
        x.tool = str("tool");
        x.theatre = str("theatre");
        x.faction = str("faction");
        x.posture = str("posture");
        x.source = str("source");
        x.confidence = str("confidence");
        x.status = str("status");
        if (x.id.empty() || x.tool.empty() || lo == nullptr || hi == nullptr || !lo->Is(json::Type::Number) || !hi->Is(json::Type::Number) ||
            x.source.empty() || x.confidence.empty()) {
            if (error != nullptr) *error = path + ": bands[" + std::to_string(i) + "] needs id, tool, lo, hi, source and confidence";
            return false;
        }
        x.lo = lo->number;
        x.hi = hi->number;
        if (x.lo > x.hi) {
            if (error != nullptr) *error = path + ": bands[" + std::to_string(i) + "] (" + x.id + ") lo above hi";
            return false;
        }
        b.bands.push_back(x);
    }
    out = b;
    return true;
}

// One measure against its band: prints a row "| measure | value | band | anchor | PASS/FAIL |" and returns false when
// out of band. `scale` turns the value and the band into the printed unit (100 for a per cent); no band prints "-".
inline bool BandRow(const BalanceBand* b, const std::string& label, double value, double scale = 1.0, const char* unit = "") {
    if (b == nullptr) {
        std::printf("| %s | %.2f%s | - | - | - |\n", label.c_str(), value * scale, unit);
        return true;
    }
    const bool in = value >= b->lo - 1e-12 && value <= b->hi + 1e-12;
    std::printf("| %s | %.2f%s | %.2f-%.2f%s | %s | %s |\n", label.c_str(), value * scale, unit, b->lo * scale, b->hi * scale, unit, b->source.c_str(),
                in ? "PASS" : "FAIL");
    return in;
}

}  // namespace airwar
}  // namespace simcore
