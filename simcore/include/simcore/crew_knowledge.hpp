// simcore/crew_knowledge.hpp — what each crewmate knows, and how he came to know it (roadmap 8.5).
//
// docs/crew/01_ROLES_AND_KNOWLEDGE.md §4: knowledge starts at a seat's own instruments (the senior operator's search
// scope, the operator's range indicator, the deputy's TV) and moves between seats only by voice (the intercom) or the
// radio. A crewmate who wasn't told doesn't know; what he was told keeps its time and its speaker, so an NPC who acts on
// a report acts on where the target was then. The simulation's truth never enters a picture.
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace simcore {

// One target as a crewmate knows it. `target` is the blip's label (the scenario id the scope shows, or "T<n>" for a
// ghost); a designation from the PU-12 also has its board number.
struct KnownTarget {
    std::string target;
    int number = 0;
    bool has_azimuth = false;
    bool has_range = false;
    double azimuth_deg = 0.0;  // the crew's frame (the gyro's); a designation's is true azimuth (true_azimuth)
    bool true_azimuth = false;
    double range_m = 0.0;
    double time_s = 0.0;       // when it was seen or said
    std::string source;        // "scope", "range indicator", "TV", "told by Senior operator", "radio"
    bool friendly = false;     // answered the IFF, or reported as a friend
    bool interrogated = false; // asked by the IFF and didn't answer ("Ответа нет!"): not a friend we know of
    bool has_height = false;   // a designation's height
    double height_m = 0.0;
};

class CrewPicture {
public:
    // What was learnt now: the fields it has replace the old ones; the others are kept.
    void Learn(const KnownTarget& k) {
        for (KnownTarget& e : items_) {
            if (e.target != k.target) continue;
            if (k.has_azimuth) {
                e.has_azimuth = true;
                e.azimuth_deg = k.azimuth_deg;
                e.true_azimuth = k.true_azimuth;
            }
            if (k.has_range) {
                e.has_range = true;
                e.range_m = k.range_m;
            }
            if (k.number != 0) e.number = k.number;
            e.friendly = e.friendly || k.friendly;
            e.interrogated = e.interrogated || k.interrogated;
            if (k.has_height) {
                e.has_height = true;
                e.height_m = k.height_m;
            }
            if (k.time_s >= e.time_s) {
                e.time_s = k.time_s;
                e.source = k.source;
            }
            return;
        }
        items_.push_back(k);
    }
    void Forget(const std::string& target) {
        std::vector<KnownTarget> kept;
        for (KnownTarget& e : items_) {
            if (e.target != target) kept.push_back(std::move(e));
        }
        items_.swap(kept);
    }
    const KnownTarget* Find(const std::string& target) const {
        for (const KnownTarget& e : items_) {
            if (e.target == target) return &e;
        }
        return nullptr;
    }
    // What is no older than `max_age_s`.
    std::vector<KnownTarget> Recent(double now_s, double max_age_s) const {
        std::vector<KnownTarget> out;
        for (const KnownTarget& e : items_) {
            if (now_s - e.time_s <= max_age_s) out.push_back(e);
        }
        return out;
    }
    const std::vector<KnownTarget>& All() const { return items_; }
    std::size_t Size() const { return items_.size(); }

private:
    std::vector<KnownTarget> items_;
};

}  // namespace simcore
