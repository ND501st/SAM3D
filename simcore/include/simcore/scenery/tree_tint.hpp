// simcore/scenery/tree_tint.hpp — the map's trees in their season (roadmap T5.1, docs/scenery/T5_T8_PLAN.md): the colour
// each tree takes on a day of the year, as a multiplier on its crown's own colour, from rules in
// data/scenery/tree_seasons.json (LoadTreeSeasons). A tree turns on its own day (the kind's onset plus or minus a spread
// drawn from its position) and its brightness differs a little from its neighbour's, so a wood of one kind is not one
// colour. The woods' masses take the weighted mean of the deciduous kinds, so a tree and the mass behind it meet at one
// colour (roadmap rule 9).
//
// Presentation only: nothing in the simulation reads it. A function of the position, the kind and the day alone, so
// every client draws the same trees (docs/MULTIPLAYER.md).
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

#include "simcore/json.hpp"
#include "simcore/scenery/map_meshes.hpp"

namespace simcore {
namespace scenery {

using TreeTintRgb = std::array<float, 3>;  // a multiplier on the crown's linear colour

struct TreeSeason {
    int onset_day = 275, full_day = 310, spread_days = 0;
    TreeTintRgb autumn{1.0f, 1.0f, 1.0f};
    double brightness = 0.0;  // each tree's brightness varies by this share either way
};

struct TreeSeasons {
    std::array<TreeSeason, static_cast<std::size_t>(TreeKind::Count)> kinds{};
    std::array<double, static_cast<std::size_t>(TreeKind::Count)> mass_mix{};  // weights, summing to 1
};

namespace tree_tint_detail {

inline bool Fail(std::string* error, const std::string& why) {
    if (error != nullptr) *error = why;
    return false;
}

// Two draws in 0..1 from a position (to the centimetre): one for the tree's own turn day, one for its brightness.
inline double Draw(double east_m, double north_m, std::uint32_t salt) {
    std::uint64_t x = static_cast<std::uint64_t>(static_cast<std::int64_t>(std::llround(east_m * 100.0))) * 0x9E3779B97F4A7C15ull;
    x ^= static_cast<std::uint64_t>(static_cast<std::int64_t>(std::llround(north_m * 100.0))) * 0xC2B2AE3D27D4EB4Full + salt;
    x ^= x >> 33;
    x *= 0xFF51AFD7ED558CCDull;
    x ^= x >> 33;
    x *= 0xC4CEB9FE1A85EC53ull;
    x ^= x >> 33;
    return static_cast<double>(x >> 11) / 9007199254740992.0;  // 53 bits
}

inline double Smooth(double t) {
    t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
    return t * t * (3.0 - 2.0 * t);
}

}  // namespace tree_tint_detail

// The share of its autumn colour a kind's tree has on the day (0 summer, 1 full).
inline double AutumnShare(const TreeSeason& s, double east_m, double north_m, int day_of_year) {
    const double shift = s.spread_days > 0 ? (tree_tint_detail::Draw(east_m, north_m, 1u) - 0.5) * 2.0 * s.spread_days : 0.0;
    const double span = static_cast<double>(s.full_day - s.onset_day);
    return tree_tint_detail::Smooth((static_cast<double>(day_of_year) - (s.onset_day + shift)) / span);
}

// The tree's colour multiplier on the day: its autumn share of the kind's autumn colour, times its own brightness.
inline TreeTintRgb TreeTintOn(const TreeSeasons& rules, int kind, double east_m, double north_m, int day_of_year) {
    const TreeSeason& s = rules.kinds[static_cast<std::size_t>(kind) % rules.kinds.size()];
    const double a = AutumnShare(s, east_m, north_m, day_of_year);
    const double bright = 1.0 + (tree_tint_detail::Draw(east_m, north_m, 2u) - 0.5) * 2.0 * s.brightness;
    TreeTintRgb out{};
    for (std::size_t i = 0; i < 3; ++i) out[i] = static_cast<float>((1.0 + (static_cast<double>(s.autumn[i]) - 1.0) * a) * bright);
    return out;
}

// The woods' masses on the day: the mix's weighted mean of the kinds' autumn multipliers at the average turn day.
inline TreeTintRgb MassTintOn(const TreeSeasons& rules, int day_of_year) {
    TreeTintRgb out{0.0f, 0.0f, 0.0f};
    for (std::size_t k = 0; k < rules.kinds.size(); ++k) {
        const TreeSeason& s = rules.kinds[k];
        const double a = tree_tint_detail::Smooth((static_cast<double>(day_of_year) - s.onset_day) / static_cast<double>(s.full_day - s.onset_day));
        for (std::size_t i = 0; i < 3; ++i) out[i] += static_cast<float>(rules.mass_mix[k] * (1.0 + (static_cast<double>(s.autumn[i]) - 1.0) * a));
    }
    return out;
}

// Loads the rules from a file path or a JSON text. Every kind needs an entry; the mix must name kinds and sum to 1.
inline bool LoadTreeSeasons(const std::string& path_or_json, TreeSeasons& out, std::string* error = nullptr) {
    using tree_tint_detail::Fail;
    std::string text = path_or_json;
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos || text[first] != '{') {
        std::ifstream f(path_or_json, std::ios::binary);
        if (!f) return Fail(error, "cannot open " + path_or_json);
        std::ostringstream s;
        s << f.rdbuf();
        text = s.str();
    }
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text, root, &parse_error)) return Fail(error, parse_error);
    const json::Value* trees = root.Find("trees");
    if (trees == nullptr || !trees->Is(json::Type::Object)) return Fail(error, "no \"trees\" object");
    TreeSeasons rules;
    for (std::size_t k = 0; k < rules.kinds.size(); ++k) {
        const std::string name = TreeKindName(static_cast<TreeKind>(k));
        const json::Value* t = trees->Find(name);
        if (t == nullptr || !t->Is(json::Type::Object)) return Fail(error, "trees: no entry for \"" + name + "\"");
        const json::Value* onset = t->Find("onset_day");
        const json::Value* full = t->Find("full_day");
        const json::Value* spread = t->Find("spread_days");
        const json::Value* autumn = t->Find("autumn");
        const json::Value* bright = t->Find("brightness");
        if (onset == nullptr || full == nullptr || !onset->Is(json::Type::Number) || !full->Is(json::Type::Number)) return Fail(error, name + ": onset_day and full_day are numbers");
        if (onset->number < 1 || full->number > 366 || full->number <= onset->number) return Fail(error, name + ": onset_day < full_day within 1..366");
        if (spread == nullptr || !spread->Is(json::Type::Number) || spread->number < 0 || spread->number > 30) return Fail(error, name + ": spread_days is not in 0..30");
        if (autumn == nullptr || !autumn->Is(json::Type::Array) || autumn->items.size() != 3) return Fail(error, name + ": autumn is three numbers");
        if (bright == nullptr || !bright->Is(json::Type::Number) || bright->number < 0 || bright->number > 0.5) return Fail(error, name + ": brightness is not in 0..0.5");
        TreeSeason& s = rules.kinds[k];
        s.onset_day = static_cast<int>(onset->number);
        s.full_day = static_cast<int>(full->number);
        s.spread_days = static_cast<int>(spread->number);
        s.brightness = bright->number;
        for (std::size_t i = 0; i < 3; ++i) {
            if (!autumn->items[i].Is(json::Type::Number) || autumn->items[i].number <= 0.0 || autumn->items[i].number > 8.0) return Fail(error, name + ": autumn values are in 0..8");
            s.autumn[i] = static_cast<float>(autumn->items[i].number);
        }
    }
    const json::Value* mix = root.Find("mass_mix");
    if (mix == nullptr || !mix->Is(json::Type::Object)) return Fail(error, "no \"mass_mix\" object");
    double sum = 0.0;
    for (std::size_t k = 0; k < rules.kinds.size(); ++k) {
        if (const json::Value* w = mix->Find(TreeKindName(static_cast<TreeKind>(k)))) {
            if (!w->Is(json::Type::Number) || w->number < 0.0) return Fail(error, "mass_mix: weights are not negative numbers");
            rules.mass_mix[k] = w->number;
            sum += w->number;
        }
    }
    if (std::fabs(sum - 1.0) > 1e-6) return Fail(error, "mass_mix: the weights sum to 1");
    out = rules;
    return true;
}

}  // namespace scenery
}  // namespace simcore
