// simcore/group_designation.hpp — a target inside a group the battery post can't resolve (voenizdat_osa_1991 p261).
//
// When the senior commander's radars can't show each aircraft of a group apart, the battery post designates a target in
// it by the group's centre and the number of a selection rule, and the launcher's crew picks the aircraft on its own
// screen:
//   rule 1 (2, 3, 4): the first (second, third, fourth) from the left of the nearest targets; if they are on one
//   azimuth, the first (second, ...) by range.
// With no rule given, the crew chief chooses one himself.
// SAM3D's reading of "the nearest targets": the group nearest the given centre, as the crew's screen shows it: the echo
// nearest the centre (within search_m) and the echoes linked to it by echoes no further apart than link_m. "From the
// left" as the crew looks at them; "on one azimuth" within the search radar's beam. A rule beyond the group's size takes
// the last (sam3d). The post counts marks closer together than post_unresolved_m as a group it can't resolve.
// Game numbers: data/gameplay/<system>.json "group".
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "simcore/vec3.hpp"

namespace simcore {

struct GroupTuning {
    double post_unresolved_m = 3000.0;  // marks this close together on the post's picture are one group to it
    double search_m = 6000.0;           // the crew looks this far around the designated centre
    double link_m = 2000.0;             // echoes this close together on the crew's screen are one group
    double same_azimuth_deg = 1.3;      // "on one azimuth": within the search radar's beam
};

// An echo on the crew's screen: its true azimuth (degrees from north) and range from the launcher.
struct GroupEcho {
    std::string id;
    double azimuth_deg = 0.0;
    double range_m = 0.0;
};

namespace group_detail {

inline double Wrap180(double deg) {
    double d = std::fmod(deg + 180.0, 360.0);
    if (d < 0.0) d += 360.0;
    return d - 180.0;
}
inline double East(double az_deg, double r) { return r * std::sin(DegToRad(az_deg)); }
inline double North(double az_deg, double r) { return r * std::cos(DegToRad(az_deg)); }
inline double Apart(const GroupEcho& a, double az_deg, double range_m) {
    const double dx = East(a.azimuth_deg, a.range_m) - East(az_deg, range_m);
    const double dy = North(a.azimuth_deg, a.range_m) - North(az_deg, range_m);
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace group_detail

// The group nearest a designated centre, as the crew's screen shows it (empty if no echo is within search_m).
inline std::vector<GroupEcho> GroupNear(const std::vector<GroupEcho>& echoes, double centre_az_deg, double centre_range_m,
                                        const GroupTuning& t) {
    std::vector<GroupEcho> out;
    std::size_t seed = echoes.size();
    double best = t.search_m;
    for (std::size_t i = 0; i < echoes.size(); ++i) {
        const double d = group_detail::Apart(echoes[i], centre_az_deg, centre_range_m);
        if (d <= best) {
            best = d;
            seed = i;
        }
    }
    if (seed == echoes.size()) return out;
    std::vector<bool> in(echoes.size(), false);
    in[seed] = true;
    out.push_back(echoes[seed]);
    for (std::size_t k = 0; k < out.size(); ++k) {  // grow by links
        for (std::size_t i = 0; i < echoes.size(); ++i) {
            if (in[i] || group_detail::Apart(echoes[i], out[k].azimuth_deg, out[k].range_m) > t.link_m) continue;
            in[i] = true;
            out.push_back(echoes[i]);
        }
    }
    return out;
}

// The group in the rules' order: from the left as the crew looks at it (anticlockwise first, about its own middle), those
// on one azimuth nearest first.
inline std::vector<GroupEcho> RuleOrder(std::vector<GroupEcho> group, double same_azimuth_deg) {
    if (group.empty()) return group;
    double sx = 0.0, sy = 0.0;
    for (const GroupEcho& e : group) {
        sx += group_detail::East(e.azimuth_deg, 1.0);
        sy += group_detail::North(e.azimuth_deg, 1.0);
    }
    const double middle = RadToDeg(std::atan2(sx, sy));
    auto rel = [&](const GroupEcho& e) { return group_detail::Wrap180(e.azimuth_deg - middle); };
    std::sort(group.begin(), group.end(), [&](const GroupEcho& a, const GroupEcho& b) { return rel(a) < rel(b); });
    // Echoes on one azimuth (each within the beam of the one before) count by range.
    for (std::size_t i = 0; i < group.size();) {
        std::size_t j = i + 1;
        while (j < group.size() && rel(group[j]) - rel(group[j - 1]) <= same_azimuth_deg) ++j;
        std::stable_sort(group.begin() + static_cast<std::ptrdiff_t>(i), group.begin() + static_cast<std::ptrdiff_t>(j),
                         [](const GroupEcho& a, const GroupEcho& b) { return a.range_m < b.range_m; });
        i = j;
    }
    return group;
}

// The echo a rule picks in the group nearest the centre ("" if there is none). Rule 0 (none given): the crew chief's own
// choice, the first rule.
inline std::string SelectByRule(const std::vector<GroupEcho>& echoes, double centre_az_deg, double centre_range_m, int rule,
                                const GroupTuning& t) {
    const std::vector<GroupEcho> order = RuleOrder(GroupNear(echoes, centre_az_deg, centre_range_m, t), t.same_azimuth_deg);
    if (order.empty()) return std::string();
    const std::size_t n = static_cast<std::size_t>(rule < 1 ? 1 : rule);
    return order[std::min(n, order.size()) - 1].id;
}

// The post's side: the marks (horizontal positions) that make one group with mark `at` on its picture: linked by marks
// closer together than `unresolved_m`. Indices, `at` first.
inline std::vector<std::size_t> UnresolvedGroup(const std::vector<Vec3>& positions, std::size_t at, double unresolved_m) {
    std::vector<std::size_t> out;
    if (at >= positions.size()) return out;
    std::vector<bool> in(positions.size(), false);
    in[at] = true;
    out.push_back(at);
    for (std::size_t k = 0; k < out.size(); ++k) {
        for (std::size_t i = 0; i < positions.size(); ++i) {
            if (in[i] || (positions[i] - positions[out[k]]).HorizontalLength() >= unresolved_m) continue;
            in[i] = true;
            out.push_back(i);
        }
    }
    return out;
}

// The rule's words on the net and the intercom: "правило 2" / "rule 2".
inline std::string RuleWordsRu(int rule) { return "правило " + std::to_string(rule); }
inline std::string RuleWordsEn(int rule) { return "rule " + std::to_string(rule); }

}  // namespace simcore
