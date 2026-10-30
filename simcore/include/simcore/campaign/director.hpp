// simcore/campaign/director.hpp — the director: when a long session wakes the player (roadmap 15.4, first form;
// docs/campaign/05_THE_SYSTEMS.md §8).
//
// Duty passes fast: the game compresses time (docs/PLAYING.md) and the director tells it when to drop back to full
// speed. It wakes the player only on what reaches the player's person at their seat, never on the truth (first person:
// a raid nobody has seen coming wakes nobody until something of it reaches the seat):
//   - heard: a word heard at the seat on a net, a line or the intercom that is not the crew's own callout (an order, the
//     regiment's message, a designation or a report on the battery net);
//   - contact: a target new in the seat's picture (on its instruments, or told), or one it had dropped coming back;
//   - board: a new mark on the post's board, for a seat at the post (the board is in the cabin).
// Each wake has its reason and what reached the seat. The quiet stretches between wakes are measured: the longest, and the
// share of the session spent in stretches long enough to compress.
// Left: the player's own alarm and the scheduled activities (07 §5), a crew report by its kind (a fault, the TZM
// arriving), the session's own coarse quiet step; the seat's own words wake it too (harmless in play: the player spoke them
// awake, but headless runs count the NPC's).
#pragma once

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simcore/crew_knowledge.hpp"
#include "simcore/pu12.hpp"
#include "simcore/scenario_runner.hpp"

namespace simcore {
namespace campaign {

struct Wake {
    double t_s = 0.0;
    std::string why;     // "heard", "contact", "board"
    std::string detail;  // what reached the seat, as the seat has it
};

// The stretches between wakes over [0, end_s]: the longest, and the share of the whole in stretches of at least `min_s`.
struct QuietStretches {
    double longest_s = 0.0;
    double share = 0.0;
};
inline QuietStretches MeasureQuiet(std::vector<double> wakes_s, double end_s, double min_s) {
    QuietStretches q;
    if (end_s <= 0.0) return q;
    std::sort(wakes_s.begin(), wakes_s.end());
    double from = 0.0, quiet = 0.0;
    wakes_s.push_back(end_s);
    for (double t : wakes_s) {
        const double to = std::min(std::max(t, from), end_s);
        const double gap = to - from;
        q.longest_s = std::max(q.longest_s, gap);
        if (gap >= min_s) quiet += gap;
        from = to;
    }
    q.share = quiet / end_s;
    return q;
}

class Director {
public:
    explicit Director(std::string seat) : seat_(std::move(seat)) {}
    const std::string& Seat() const { return seat_; }

    // After every step of the runner: this step's wakes (none: the player may sleep on).
    std::vector<Wake> Step(const ScenarioRunner& r) {
        std::vector<Wake> now;
        // The runner keeps only its last messages: those logged and dropped between two steps are lost to the director too.
        const std::vector<ScenarioRunner::RadioMessage>& radio = r.RadioLog();
        const std::size_t first = r.RadioTotal() - radio.size();
        for (radio_seen_ = std::max(radio_seen_, first); radio_seen_ < r.RadioTotal(); ++radio_seen_) {
            const ScenarioRunner::RadioMessage& m = radio[radio_seen_ - first];
            if (m.callout || !m.HeardAt(seat_)) continue;
            now.push_back({m.time_s, "heard", m.net + ": " + m.from + ": " + m.text});
        }
        if (const CrewPicture* p = r.PictureOf(seat_)) {
            for (const KnownTarget& k : p->All()) {
                if (!known_.insert(k.target).second) continue;
                char d[96];
                if (k.number > 0) {
                    std::snprintf(d, sizeof(d), "target %04d (%s)", k.number, k.source.c_str());
                } else {
                    std::snprintf(d, sizeof(d), "a target (%s)", k.source.c_str());
                }
                now.push_back({r.Time(), "contact", d});
            }
            for (auto it = known_.begin(); it != known_.end();) {  // dropped (destroyed, lost): may come back
                it = p->Find(*it) == nullptr ? known_.erase(it) : std::next(it);
            }
        }
        for (const BoardMark& m : r.PostBoard()) {
            if (m.number <= 0 || !marks_.insert(m.number).second) continue;
            char d[64];
            std::snprintf(d, sizeof(d), "mark %04d on the board", m.number);
            now.push_back({r.Time(), "board", d});
        }
        wakes_.insert(wakes_.end(), now.begin(), now.end());
        return now;
    }

    const std::vector<Wake>& Wakes() const { return wakes_; }
    int Count(const std::string& why = std::string()) const {
        int n = 0;
        for (const Wake& w : wakes_) n += why.empty() || w.why == why ? 1 : 0;
        return n;
    }
    QuietStretches Quiet(double end_s, double min_s) const {
        std::vector<double> t;
        for (const Wake& w : wakes_) t.push_back(w.t_s);
        return MeasureQuiet(t, end_s, min_s);
    }
    // For the debrief: how often the player was woken and how much of the session was quiet.
    std::string Text(double end_s, double min_s = kCompressS) const {
        const QuietStretches q = Quiet(end_s, min_s);
        char b[256];
        std::snprintf(b, sizeof(b),
                      "The director woke the %s %d times (%d heard, %d contacts, %d marks); the longest quiet stretch %.1f h; %.0f %% "
                      "of the session in quiet stretches of %.0f min or more\n",
                      seat_.c_str(), Count(), Count("heard"), Count("contact"), Count("board"), q.longest_s / 3600.0, 100.0 * q.share,
                      min_s / 60.0);
        return b;
    }
    static constexpr double kCompressS = 600.0;  // sam3d: a stretch worth compressing

private:
    std::string seat_;
    std::size_t radio_seen_ = 0;  // of RadioTotal()
    std::set<std::string> known_;  // the targets in the seat's picture
    std::set<int> marks_;          // the board's numbers seen
    std::vector<Wake> wakes_;
};

}  // namespace campaign
}  // namespace simcore
