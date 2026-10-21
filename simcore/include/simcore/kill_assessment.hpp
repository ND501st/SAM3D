// simcore/kill_assessment.hpp — the crew judges a shot's result from what its instruments show (voenizdat_osa_1991 p267).
//
// The outcome model (engagement.hpp) decides what happened at the meeting; nobody in the crew knows it. What they have
// are the signs on their instruments (p267), each seen by the seat whose instrument shows it:
//   a hit:  the burst's mark at the meeting and the missile's reply gone (the range indicator; without the tracking
//           radar's range, the ЗАХВАТ lamp going out on the launch console); the target's mark vanishing (the range
//           indicator, else the search scope); its elevation dropping fast (the elevation dial); a group's mark
//           splitting, a jammer's strobe or a chaff drop stopping (the search scope); fire or break-up on the TV;
//   a miss: the missile's reply lost before the meeting, or still there after it; the target flying on unchanged (the
//           range indicator, the elevation dial or the search scope); the target whole on the TV.
// The NPC at that seat calls each sign out; the judge (the deputy commander, who has the TV and the elevation dial: the
// book names the TV the fullest source) weighs what he has seen and heard and calls the result: as soon as a sign
// settles it, else with what he has when his time is up. Where the signs say little he may call it wrong.
//
// The World has no wreck: a destroyed aircraft leaves the instruments at once, so its mark vanishing is read from the
// instruments, and what the wreck would show (the burst, its fall on the elevation dial, fire on the TV, the group
// splitting, its jamming and chaff stopping) is drawn here as signs. A miss's signs are read from the live target on
// the instruments, so a target that dives away after a near miss can look destroyed.
// Game numbers: data/gameplay/<system>.json "kill_assessment" (estimates; the signs themselves are the book's).
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace simcore {

enum class KillSign {
    Burst,          // the burst's mark at the meeting, the missile's reply gone
    MarkGone,       // the target's mark vanished
    ElevationDrop,  // its elevation dropping fast
    GroupSplit,     // the group's mark splitting
    JammingStops,   // the jammer's strobe gone
    ChaffStops,     // the chaff drop stopped
    TvFire,         // fire or break-up on the TV
    ReplyEarly,     // the missile's reply lost before the meeting
    ReplyAfter,     // the missile's reply still there after the meeting
    FliesOn,        // the target flying on unchanged
    TvWhole,        // the target whole on the TV
};
constexpr std::size_t kKillSignCount = 11;

inline const char* KillSignId(KillSign s) {
    switch (s) {
        case KillSign::Burst: return "burst";
        case KillSign::MarkGone: return "mark_gone";
        case KillSign::ElevationDrop: return "elevation_drop";
        case KillSign::GroupSplit: return "group_split";
        case KillSign::JammingStops: return "jamming_stops";
        case KillSign::ChaffStops: return "chaff_stops";
        case KillSign::TvFire: return "tv_fire";
        case KillSign::ReplyEarly: return "reply_early";
        case KillSign::ReplyAfter: return "reply_after";
        case KillSign::FliesOn: return "flies_on";
        case KillSign::TvWhole: return "tv_whole";
    }
    return "?";
}
inline bool KillSignFromId(const std::string& id, KillSign& out) {
    for (std::size_t i = 0; i < kKillSignCount; ++i) {
        if (id == KillSignId(static_cast<KillSign>(i))) {
            out = static_cast<KillSign>(i);
            return true;
        }
    }
    return false;
}
// The sign as the debrief says it.
inline const char* KillSignText(KillSign s) {
    switch (s) {
        case KillSign::Burst: return "the burst at the meeting, the missile's reply gone";
        case KillSign::MarkGone: return "the target's mark gone";
        case KillSign::ElevationDrop: return "the target's elevation dropping fast";
        case KillSign::GroupSplit: return "the group's mark splitting";
        case KillSign::JammingStops: return "its jamming gone";
        case KillSign::ChaffStops: return "its chaff stopped";
        case KillSign::TvFire: return "fire and break-up on the TV";
        case KillSign::ReplyEarly: return "the missile's reply lost before the meeting";
        case KillSign::ReplyAfter: return "the missile's reply still there after the meeting";
        case KillSign::FliesOn: return "the target flying on unchanged";
        case KillSign::TvWhole: return "the target whole on the TV";
    }
    return "?";
}
// The crew's callout for a sign (data/crew/<system>_crew.json): "kill_sign_<id>".
inline std::string KillSignCallout(KillSign s) { return std::string("kill_sign_") + KillSignId(s); }
inline bool KillSignOfCallout(const std::string& callout, KillSign& out) {
    static const std::string prefix = "kill_sign_";
    return callout.compare(0, prefix.size(), prefix) == 0 && KillSignFromId(callout.substr(prefix.size()), out);
}

// Where a sign shows. The runner gives each its seat: the range indicator the range wheel's, the elevation dial, the TV
// and the launch console's lamps the elevation wheel's, the search scope the azimuth wheel's (the tuning's aim roles).
enum class KillInstrument { RangeIndicator, LaunchLamps, ElevationDial, Tv, SearchScope };

inline const char* ToString(KillInstrument i) {
    switch (i) {
        case KillInstrument::RangeIndicator: return "range indicator";
        case KillInstrument::LaunchLamps: return "launch console";
        case KillInstrument::ElevationDial: return "elevation dial";
        case KillInstrument::Tv: return "TV";
        case KillInstrument::SearchScope: return "search scope";
    }
    return "?";
}

struct KillAssessmentTuning {
    std::string judge_role = "deputy_commander";  // who calls the result (the commander when he is not an NPC)
    double judge_min_s = 2.0;    // the earliest call after the meeting, however clear the signs
    double judge_max_s = 8.0;    // the latest: the call with what he has
    double watch_s = 3.0;        // the target watched this long after the meeting before "flies on unchanged"
    double mark_gone_s = 1.0;    // its echo this long off the range indicator: gone
    double scope_gone_s = 2.0;   // ... off the search scope (a turn of the antenna and a little)
    double near_burst_chance = 0.5;   // of the outcome model's misses, the fuse fired beside the target
    double early_burst_chance = 0.05; // ... the warhead went before the meeting (the rest pass by: the reply goes on)
    double elevation_drop_chance = 0.7;  // a destroyed aircraft's fall shows on the elevation dial before the track breaks
    double tv_fire_chance = 0.9;         // ... its fire or break-up shows on the TV
    double elevation_drop_deg_s = 0.3;   // a live target's elevation falling this fast reads as a fall
    double strobe_match_deg = 3.0;       // a strobe this close to the target's bearing is its jamming
    double beside_m = 2000.0;            // an echo this close beside it on the search scope: its group, or its chaff
    double ambiguous_wrong_chance = 0.2; // the call goes the wrong way when no sign settles it (plus the judge's skill)
    double recheck_s = 10.0;             // a target called destroyed and still on the search scope this long after: flying
    double decisive = 3.0;               // the signs' weight that settles it
    std::array<double, kKillSignCount> weight{{1.0, 2.0, 1.0, 1.0, 2.0, 1.0, 3.0, -3.0, -3.0, -2.0, -3.0}};
    std::array<double, kKillSignCount> delay_s{{0.3, 0.0, 1.5, 2.0, 1.5, 4.0, 1.0, 0.0, 0.5, 0.0, 2.0}};

    double Weight(KillSign s) const { return weight[static_cast<std::size_t>(s)]; }
    double Delay(KillSign s) const { return delay_s[static_cast<std::size_t>(s)]; }
};

// What the crew's instruments showed of a shot's target as the missile met it (engagement.hpp fills it in).
struct MeetingView {
    bool radar_range = false;  // its echo on the range indicator: the tracking radar on it
    bool antenna_on = false;   // the tracking antenna on it (radar, TV or the jammer's noise): the elevation dial shows it
    bool on_tv = false;        // seen on the TV
    bool on_scope = false;     // its echo on the search scope
    bool strobe = false;       // a jamming strobe on its bearing on the search scope
    bool noise_tracked = false;  // the tracking radar held it by its noise (ПОМЕХА)
    bool chaff = false;        // an echo of no aircraft beside it on the search scope (its chaff)
    bool group = false;        // another aircraft's echo beside it on the search scope
    double elevation_deg = 0.0;
};

// What the instruments show of a shot's target now (the runner reads them each step).
struct KillLook {
    bool on_range = false;  // its echo on the range indicator
    bool tracked = false;   // the tracking antenna on it
    double elevation_deg = 0.0;  // on the elevation dial, while tracked
    bool on_scope = false;  // its echo on the search scope
    bool on_tv = false;     // seen on the TV
};

class KillAssessment {
public:
    enum class Call { Hit, Miss, Unknown };
    static const char* ToString(Call c) { return c == Call::Hit ? "hit" : c == Call::Miss ? "miss" : "not observed"; }

    struct Sign {
        int shot = 0;
        std::string target;
        KillSign sign = KillSign::Burst;
        KillInstrument where = KillInstrument::RangeIndicator;
        double t = 0.0;
    };
    struct Judgement {
        int shot = 0;
        std::string target;
        Call call = Call::Unknown;
        bool hit = false;    // what the outcome model had (for the debrief)
        bool wrong = false;  // the call went against it
        bool settled = false;  // a sign settled it (no guess)
        double score = 0.0;
        double meet_s = 0.0;
        std::vector<KillSign> known;  // what the judge went by
    };

    void Configure(const KillAssessmentTuning& tuning, unsigned seed) {
        tuning_ = tuning;
        rng_.seed(seed);
    }
    const KillAssessmentTuning& Tuning() const { return tuning_; }

    // A shot ended (engagement.hpp's Hit or Miss): `hit` the outcome model's, `detail` its reason for a miss ("track lost",
    // "out of reach", "" for a miss by chance), `view` what the instruments showed at that moment.
    void Open(double t, int shot, const std::string& target, bool hit, const std::string& detail, const MeetingView& view) {
        Assessment a;
        a.shot = shot;
        a.target = target;
        a.meet_s = t;
        a.hit = hit;
        a.view = view;
        if (hit) {
            a.end = End::Hit;
        } else if (detail == "track lost") {
            a.end = End::Early;  // no commands: the missile tracker loses it
        } else if (detail == "out of reach") {
            a.end = End::Wide;  // it flies on to its self-destruction
        } else {
            const double d = Draw();
            a.end = d < tuning_.early_burst_chance ? End::Early : d < tuning_.early_burst_chance + tuning_.near_burst_chance ? End::NearBurst : End::Wide;
        }
        a.tv_fire = hit && Draw() < tuning_.tv_fire_chance;
        a.falls = hit && Draw() < tuning_.elevation_drop_chance;
        a.known_s.fill(-1.0);
        open_.push_back(std::move(a));
    }

    // What the instruments show of each open shot's target now (`look(target)` returns a KillLook); the signs that
    // appear go to TakeSigns.
    template <class Look>
    void Observe(double t, Look&& look) {
        for (Assessment& a : open_) {
            const double since = t - a.meet_s;
            const MeetingView& v = a.view;
            const KillInstrument reply = v.radar_range ? KillInstrument::RangeIndicator : KillInstrument::LaunchLamps;
            // What the shot itself shows: the burst, the missile's reply, and a destroyed aircraft's wreck.
            if (a.end == End::Hit || a.end == End::NearBurst) Due(a, t, KillSign::Burst, reply);
            if (a.end == End::Early) Due(a, t, KillSign::ReplyEarly, reply);
            if (a.end == End::Wide) Due(a, t, KillSign::ReplyAfter, reply);
            if (a.end == End::Hit) {
                if (a.tv_fire && v.on_tv) Due(a, t, KillSign::TvFire, KillInstrument::Tv);
                if (a.falls && v.antenna_on) Due(a, t, KillSign::ElevationDrop, KillInstrument::ElevationDial);
                if (v.group) Due(a, t, KillSign::GroupSplit, KillInstrument::SearchScope);
                if (v.strobe || v.noise_tracked) {
                    Due(a, t, KillSign::JammingStops, v.strobe ? KillInstrument::SearchScope : KillInstrument::RangeIndicator);
                }
                if (v.chaff) Due(a, t, KillSign::ChaffStops, KillInstrument::SearchScope);
            }
            // What the instruments show of the target itself, live or gone (the same reading for a hit and a miss).
            const KillLook l = look(a.target);
            if (v.radar_range) {
                Gone(a, t, !l.on_range, tuning_.mark_gone_s, a.range_gone_s, KillInstrument::RangeIndicator);
            } else if (v.on_scope) {
                Gone(a, t, !l.on_scope, tuning_.scope_gone_s, a.scope_gone_s, KillInstrument::SearchScope);
            }
            if (v.antenna_on && l.tracked && since >= 1.0 && !a.Shown(KillSign::ElevationDrop) &&
                (l.elevation_deg - v.elevation_deg) / since <= -tuning_.elevation_drop_deg_s) {
                Show(a, t, KillSign::ElevationDrop, KillInstrument::ElevationDial);
            }
            if (l.on_tv && since >= tuning_.Delay(KillSign::TvWhole) && !a.Shown(KillSign::TvFire)) {
                Show(a, t, KillSign::TvWhole, KillInstrument::Tv);
            }
            if (since >= tuning_.watch_s && !a.Shown(KillSign::ElevationDrop) && !a.Shown(KillSign::MarkGone)) {
                if (l.on_range) {
                    Show(a, t, KillSign::FliesOn, KillInstrument::RangeIndicator);
                } else if (l.tracked) {
                    Show(a, t, KillSign::FliesOn, KillInstrument::ElevationDial);
                } else if (l.on_scope) {
                    Show(a, t, KillSign::FliesOn, KillInstrument::SearchScope);
                }
            }
        }
    }

    // The signs that appeared since the last call, oldest first.
    std::vector<Sign> TakeSigns() {
        std::vector<Sign> out;
        out.swap(signs_);
        return out;
    }

    // The judge has taken a sign in from his own instrument (at `t`, after he noticed it).
    void Learn(int shot, KillSign s, double t) {
        for (Assessment& a : open_) {
            if (a.shot == shot && a.Shown(s) && a.known_s[static_cast<std::size_t>(s)] < 0.0) a.known_s[static_cast<std::size_t>(s)] = t;
        }
    }
    // ... or heard it called out: the oldest open shot that showed it and he doesn't know of yet. False if none.
    bool LearnHeard(KillSign s, double t) {
        for (Assessment& a : open_) {
            if (a.Shown(s) && a.known_s[static_cast<std::size_t>(s)] < 0.0) {
                a.known_s[static_cast<std::size_t>(s)] = t;
                return true;
            }
        }
        return false;
    }

    // The calls due now. `wrong_chance`: the judge misreading signs that settle nothing (the tuning's chance and his
    // skill's). A judged shot is closed.
    std::vector<Judgement> Judge(double t, double wrong_chance) {
        std::vector<Judgement> out;
        for (std::size_t i = 0; i < open_.size();) {
            Assessment& a = open_[i];
            Judgement j;
            j.shot = a.shot;
            j.target = a.target;
            j.hit = a.hit;
            j.meet_s = a.meet_s;
            for (std::size_t k = 0; k < kKillSignCount; ++k) {
                if (a.known_s[k] < 0.0 || a.known_s[k] > t + 1e-9) continue;
                j.known.push_back(static_cast<KillSign>(k));
                j.score += tuning_.weight[k];
            }
            const double since = t - a.meet_s;
            j.settled = std::fabs(j.score) >= tuning_.decisive - 1e-9;
            if (!((j.settled && since >= tuning_.judge_min_s) || since >= tuning_.judge_max_s)) {
                ++i;
                continue;
            }
            if (j.score > 0.0) {
                j.call = Call::Hit;
            } else if (j.score < 0.0) {
                j.call = Call::Miss;
            }
            if (!j.settled && j.call != Call::Unknown && Draw() < wrong_chance) j.call = j.call == Call::Hit ? Call::Miss : Call::Hit;
            j.wrong = j.call != Call::Unknown && (j.call == Call::Hit) != a.hit;
            out.push_back(std::move(j));
            open_.erase(open_.begin() + static_cast<std::ptrdiff_t>(i));
        }
        return out;
    }

    // A shot at this target is being judged (the crew waits for the call before the next move).
    bool Pending(const std::string& target) const {
        for (const Assessment& a : open_) {
            if (a.target == target) return true;
        }
        return false;
    }
    std::size_t OpenCount() const { return open_.size(); }

private:
    enum class End { Hit, NearBurst, Wide, Early };
    struct Assessment {
        int shot = 0;
        std::string target;
        double meet_s = 0.0;
        bool hit = false;
        End end = End::Wide;
        MeetingView view;
        bool tv_fire = false;
        bool falls = false;
        double range_gone_s = -1.0;  // its echo off the range indicator since
        double scope_gone_s = -1.0;  // ... off the search scope since
        std::array<bool, kKillSignCount> shown{};
        std::array<double, kKillSignCount> known_s{};  // when the judge took it in (-1: not yet)
        bool Shown(KillSign s) const { return shown[static_cast<std::size_t>(s)]; }
    };

    double Draw() { return static_cast<double>(rng_()) / 4294967296.0; }

    void Show(Assessment& a, double t, KillSign s, KillInstrument where) {
        if (a.Shown(s)) return;
        a.shown[static_cast<std::size_t>(s)] = true;
        signs_.push_back({a.shot, a.target, s, where, t});
    }
    // A drawn sign, once its delay after the meeting has passed.
    void Due(Assessment& a, double t, KillSign s, KillInstrument where) {
        if (t - a.meet_s >= tuning_.Delay(s) - 1e-9) Show(a, t, s, where);
    }
    void Gone(Assessment& a, double t, bool off, double need_s, double& since_s, KillInstrument where) {
        if (!off) {
            since_s = -1.0;
            return;
        }
        if (since_s < 0.0) since_s = t;
        if (t - since_s >= need_s - 1e-9 && !a.Shown(KillSign::FliesOn)) Show(a, t, KillSign::MarkGone, where);
    }

    KillAssessmentTuning tuning_;
    std::mt19937 rng_{17u};
    std::vector<Assessment> open_;
    std::vector<Sign> signs_;
};

}  // namespace simcore
