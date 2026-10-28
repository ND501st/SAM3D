// simcore/crew_callouts.hpp — the crew's voice: orders, acknowledgements, reports and reminders (roadmap 8.4).
//
// docs/crew/04_COMMANDS_AND_CALLOUTS.md is the specification; the words are the crew data's catalogue
// (data/crew/<system>_crew.json, `callouts`). A CalloutBus carries them:
//   * one voice at a time per channel: the intercom inside the vehicle, the battery net outside, and the cabin (a word
//     said aloud, without the intercom: a seat with no headset or no box); the runner routes each line (its `Router`:
//     docs/crew/12_COMMUNICATIONS.md) and marks a circular call (ВЫЗ, ЦИРКУЛЯР) that reaches boxes on the radio too;
//   * urgent callouts (priority 0) are spoken before everything queued, and break in: a line of less urgency being said
//     on the channel is broken off (not heard) and said again after; otherwise first come, first spoken;
//   * speaking takes time: 0.4 s plus 0.35 s a word (about 2.8 words a second; sam3d estimate), or, for a fixed line
//     the game plays from a recording (art/crew_voices), the recording's length, so the next line doesn't cut it off;
//   * the next speaker on a channel starts a moment after the last stopped (kTurnGapS), and an NPC doesn't start a line
//     that isn't urgent while another channel he listens to is talking in his ear (the runner's `Listens`);
//   * a line left waiting too long is dropped: an acknowledgement or a target's position goes stale (ShelfS);
//   * one caller per fact: the same report in the same words from a second seat, while the first is still fresh, is not
//     said again (Merged());
//   * a callout can be delayed before it is queued (the speaker noticing and thinking: his skill).
// docs/crew/15_TALK.md section 2 gives the rules and where each comes from.
// What is said is filled in from fields: "{azimuth}", "{range_km}", "{seat}", "{what}" and so on.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/crew_data.hpp"
#include "simcore/crew_knowledge.hpp"

namespace simcore {

struct SpokenCallout {
    std::string id;       // the catalogue's id
    std::string from;     // a station, or "battery"
    std::string to;
    std::string channel;  // "intercom", "cabin" or "battery net"
    std::string ru;       // the words, filled in
    std::string en;
    int priority = 1;
    double queued_s = 0.0;
    double start_s = 0.0;
    double end_s = 0.0;
    // What it tells the listeners about a target (a contact's azimuth and range, a designation), if anything: it goes
    // into each listener's picture when heard (crew_knowledge.hpp).
    bool has_about = false;
    KnownTarget about;
    int repeats = 0;  // said again after "Повторите"
    bool call = false;    // made with the call (ВЫЗ / ЦИРКУЛЯР): heard by boxes on the radio too
    std::size_t seq = 0;  // the order it started in (1, 2, ...): the voice plays each line once (roadmap 8.10)
    bool cut = false;     // broken off by an urgent line on its channel (in the log only: nobody heard it whole)
    CalloutKind kind = CalloutKind::Report;
};

// "{key}" replaced by fields[key]; unknown keys are left as they are.
inline std::string FillCallout(std::string text, const std::map<std::string, std::string>& fields) {
    for (const auto& f : fields) {
        const std::string key = "{" + f.first + "}";
        std::size_t at = 0;
        while ((at = text.find(key, at)) != std::string::npos) {
            text.replace(at, key.size(), f.second);
            at += f.second.size();
        }
    }
    return text;
}

// Speaking time of a line: 0.4 s plus 0.35 s a word.
inline double SpeakingTimeS(const std::string& words) {
    int n = 0;
    bool in_word = false;
    for (char c : words) {
        const bool space = c == ' ' || c == '\t' || c == ',' || c == '!' || c == '.' || c == '-';
        if (!space && !in_word) ++n;
        in_word = !space;
    }
    return 0.4 + 0.35 * n;
}

// Azimuth in thousandths on the 6000 circle, as the crew says it: "05-40".
inline std::string MilsText(double azimuth_deg) {
    double a = azimuth_deg;
    while (a < 0.0) a += 360.0;
    while (a >= 360.0) a -= 360.0;
    int mils = static_cast<int>(a * 6000.0 / 360.0 + 0.5) % 6000;
    char out[16];
    std::snprintf(out, sizeof(out), "%02d-%02d", mils / 100, mils % 100);
    return out;
}

class CalloutBus {
public:
    // The pause between one speaker stopping and the next starting on a channel (sam3d, after the 0.2 s gap between
    // turns in conversation: Stivers et al. 2009, PNAS 106(26)); a line breaking in doesn't wait for it.
    static constexpr double kTurnGapS = 0.2;
    // How long a line may wait for the channel before it is no longer worth saying (sam3d estimates): an acknowledgement
    // or "done" after 5 s (the order's giver has moved on), a target's position after 8 s (it has moved on: a fresh
    // report follows), a reminder after 10 s, any other report after 15 s. Orders, warnings, urgent lines and the
    // radio's never: what has to be said is said.
    static double ShelfS(CalloutKind kind, int priority, bool has_about) {
        if (priority == 0 || kind == CalloutKind::Order || kind == CalloutKind::Warning || kind == CalloutKind::Radio) return -1.0;
        if (kind == CalloutKind::Ack) return 5.0;
        if (kind == CalloutKind::Remind) return 10.0;
        return has_about ? 8.0 : 15.0;
    }
    // One caller per fact: a report or warning in the same words from another seat within this long is not said again.
    static constexpr double kSameFactS = 8.0;

    explicit CalloutBus(const CrewData* crew) : crew_(crew) {}

    // Whether `station` listens to `channel` (his headset's box, the net's set, earshot in the cabin), and whether a seat is
    // the player's (his lines are never dropped as stale and he may talk over what he hears). Without them nobody waits
    // on another channel.
    void SetListens(std::function<bool(const std::string& station, const std::string& channel)> f) { listens_ = std::move(f); }
    void SetPlayerSeat(std::function<bool(const std::string& station)> f) { player_seat_ = std::move(f); }

    // How a line goes: its channel, whether it is made with the call, and any extra time before it can be said (turning
    // a box, shouting). Without a router: the intercom, or the battery net for a radio callout.
    struct Route {
        std::string channel;
        bool call = false;
        double extra_s = 0.0;
    };
    using Router = std::function<Route(const CalloutDef&, const std::string& from)>;
    void SetRouter(Router r) { router_ = std::move(r); }

    // The lengths of the recorded lines the game plays (crew_voice.hpp RecordedLineLengths), keyed "<speaker>/<id>", or
    // "<id>" for every speaker. Without them every line takes SpeakingTimeS.
    void SetRecordedLengths(std::map<std::string, double> lengths_s) { recorded_s_ = std::move(lengths_s); }
    // How long a line takes: a fixed line (the catalogue's words, nothing filled in) with a recording takes the
    // recording's length, the speaker's own before the shared one, as SimCrewVoiceComponent::PlayRecorded picks it;
    // any other line the speaking time of its Russian words.
    double LineTimeS(const std::string& id, const std::string& from, const std::string& ru) const {
        const CalloutDef* def = !recorded_s_.empty() && crew_ != nullptr ? crew_->FindCallout(id) : nullptr;
        if (def != nullptr && def->ru == ru) {
            auto it = recorded_s_.find(from + "/" + id);
            if (it == recorded_s_.end()) it = recorded_s_.find(id);
            if (it != recorded_s_.end() && it->second > 0.0) return it->second;
        }
        return SpeakingTimeS(ru);
    }

    // Queue the catalogue's callout `id` from `from` (a station), after `delay_s`. Returns false for an unknown id.
    bool Say(double now_s, const std::string& id, const std::string& from, const std::map<std::string, std::string>& fields = {},
             double delay_s = 0.0, const KnownTarget* about = nullptr) {
        const CalloutDef* def = crew_ != nullptr ? crew_->FindCallout(id) : nullptr;
        if (def == nullptr) return false;
        // A seat's acknowledgement still waiting its turn when he reports the drill done: "Выполнено!" covers both; and
        // one "Есть!" waiting is enough (sam3d: a busy intercom otherwise spoke strings of late "Есть! Выполнено!").
        if (id == "done") {
            pending_.erase(std::remove_if(pending_.begin(), pending_.end(),
                                          [&](const SpokenCallout& p) { return p.id == "ack" && p.from == from && p.repeats == 0; }),
                           pending_.end());
        }
        if (id == "ack") {
            for (const SpokenCallout& p : pending_) {
                if (p.id == "ack" && p.from == from) return true;
            }
        }
        SpokenCallout s;
        s.id = id;
        s.from = from;
        s.to = def->to;
        s.kind = def->kind;
        s.ru = FillCallout(def->ru, fields);
        if (about != nullptr) {
            s.has_about = true;
            s.about = *about;
        }
        if ((def->kind == CalloutKind::Report || def->kind == CalloutKind::Warning) && SameFactSaid(now_s, s)) {
            ++merged_;
            return true;
        }
        s.channel = def->kind == CalloutKind::Radio ? "battery net" : "intercom";
        double extra = 0.0;
        if (router_) {
            const Route r = router_(*def, from);
            s.channel = r.channel;
            s.call = r.call;
            extra = r.extra_s;
        }
        std::map<std::string, std::string> en = fields;  // "<key>_en" gives the English for a field
        for (const auto& f : fields) {
            const auto it = fields.find(f.first + "_en");
            if (it != fields.end()) en[f.first] = it->second;
        }
        s.en = FillCallout(def->en, en);
        s.priority = def->priority;
        s.queued_s = now_s + (delay_s > 0.0 ? delay_s : 0.0) + extra;
        pending_.push_back(std::move(s));
        return true;
    }

    // Say a callout again (a listener asked "Повторите"), after `delay_s`.
    void Repeat(const SpokenCallout& said, double now_s, double delay_s) {
        SpokenCallout s = said;
        s.queued_s = now_s + delay_s;
        s.start_s = s.end_s = 0.0;
        ++s.repeats;
        pending_.push_back(std::move(s));
    }

    // Start what can be spoken now, finish what has been said. Finished callouts go to TakeHeard().
    void Step(double now_s) {
        // Finish.
        std::vector<SpokenCallout> still;
        for (SpokenCallout& s : speaking_) {
            if (now_s + 1e-9 >= s.end_s) {
                free_at_[s.channel] = s.end_s + kTurnGapS;
                heard_.push_back(s);
                log_.push_back(s);
            } else {
                still.push_back(std::move(s));
            }
        }
        speaking_.swap(still);
        // Stale: a line that waited past its shelf life isn't said (never the player's: he chose to say it).
        pending_.erase(std::remove_if(pending_.begin(), pending_.end(),
                                      [&](const SpokenCallout& p) {
                                          const double shelf = ShelfS(p.kind, p.priority, p.has_about);
                                          if (shelf < 0.0 || now_s - p.queued_s <= shelf || (player_seat_ && player_seat_(p.from))) return false;
                                          ++dropped_;
                                          return true;
                                      }),
                       pending_.end());
        // Break in: an urgent line due on a channel where a less urgent one is being said cuts it off; the cut line is
        // said again after, whole (unless it goes stale first).
        std::vector<SpokenCallout> again;
        for (const SpokenCallout& p : pending_) {
            if (p.priority != 0 || p.queued_s > now_s + 1e-9) continue;
            for (auto it = speaking_.begin(); it != speaking_.end();) {
                if (it->channel != p.channel || it->priority == 0) {
                    ++it;
                    continue;
                }
                SpokenCallout cut = *it;
                cut.cut = true;
                cut.end_s = now_s;
                cut_log_.push_back(std::move(cut));
                SpokenCallout redo = *it;
                redo.start_s = redo.end_s = 0.0;
                redo.seq = 0;
                again.push_back(std::move(redo));
                it = speaking_.erase(it);
                free_at_[p.channel] = now_s;
            }
        }
        for (SpokenCallout& r : again) pending_.push_back(std::move(r));
        // Start: per channel, the most urgent of those due, oldest first.
        for (;;) {
            int best = -1;
            for (std::size_t i = 0; i < pending_.size(); ++i) {
                const SpokenCallout& s = pending_[i];
                if (s.queued_s > now_s + 1e-9 || Busy(s.channel) || SpeakingFrom(s.from)) continue;
                if (s.priority != 0 && (now_s + 1e-9 < FreeAt(s.channel) || InHisEar(s))) continue;
                if (best < 0 || s.priority < pending_[static_cast<std::size_t>(best)].priority ||
                    (s.priority == pending_[static_cast<std::size_t>(best)].priority && s.queued_s < pending_[static_cast<std::size_t>(best)].queued_s)) {
                    best = static_cast<int>(i);
                }
            }
            if (best < 0) return;
            SpokenCallout s = pending_[static_cast<std::size_t>(best)];
            pending_.erase(pending_.begin() + best);
            s.seq = ++started_;
            s.start_s = now_s;
            s.end_s = now_s + LineTimeS(s.id, s.from, s.ru);
            speaking_.push_back(std::move(s));
        }
    }

    // Callouts finished since the last call, in the order they ended.
    std::vector<SpokenCallout> TakeHeard() {
        std::vector<SpokenCallout> out;
        out.swap(heard_);
        return out;
    }
    // Everything said so far.
    const std::vector<SpokenCallout>& Log() const { return log_; }
    // What is being said now (started, not finished), on every channel.
    const std::vector<SpokenCallout>& SpeakingNow() const { return speaking_; }
    std::size_t Pending() const { return pending_.size(); }
    bool Speaking(const std::string& channel) const { return Busy(channel); }
    // What is being said right now, one per channel (the voice and the speakers' bodies, roadmap 8.10).
    const std::vector<SpokenCallout>& Now() const { return speaking_; }
    bool SpeakingFrom(const std::string& station) const {
        for (const SpokenCallout& s : speaking_) {
            if (s.from == station) return true;
        }
        return false;
    }
    // Lines dropped as stale, and not said because another seat had just said the same.
    int Dropped() const { return dropped_; }
    int Merged() const { return merged_; }
    // Lines broken off by an urgent line (each said again after, whole, unless it went stale): not in Log().
    const std::vector<SpokenCallout>& CutOff() const { return cut_log_; }

private:
    bool Busy(const std::string& channel) const {
        for (const SpokenCallout& s : speaking_) {
            if (s.channel == channel) return true;
        }
        return false;
    }
    double FreeAt(const std::string& channel) const {
        const auto it = free_at_.find(channel);
        return it != free_at_.end() ? it->second : -1e9;
    }
    // Another channel the speaker listens to is talking in his ear: an NPC waits for it to finish.
    bool InHisEar(const SpokenCallout& s) const {
        if (!listens_ || (player_seat_ && player_seat_(s.from))) return false;
        for (const SpokenCallout& o : speaking_) {
            if (o.channel != s.channel && listens_(s.from, o.channel)) return true;
        }
        return false;
    }
    bool SameFactSaid(double now_s, const SpokenCallout& s) const {
        const auto same = [&s](const SpokenCallout& o) {
            return o.id == s.id && o.from != s.from && o.ru == s.ru && o.has_about == s.has_about &&
                   (!s.has_about || o.about.target == s.about.target);
        };
        for (const SpokenCallout& o : pending_) {
            if (same(o)) return true;
        }
        for (const SpokenCallout& o : speaking_) {
            if (same(o)) return true;
        }
        for (auto it = log_.rbegin(); it != log_.rend() && it->end_s >= now_s - kSameFactS; ++it) {
            if (same(*it)) return true;
        }
        return false;
    }

    const CrewData* crew_;
    Router router_;
    std::function<bool(const std::string&, const std::string&)> listens_;
    std::function<bool(const std::string&)> player_seat_;
    std::map<std::string, double> free_at_;  // when each channel is free for the next speaker (kTurnGapS after the last)
    int dropped_ = 0;
    int merged_ = 0;
    std::vector<SpokenCallout> cut_log_;
    std::map<std::string, double> recorded_s_;
    std::vector<SpokenCallout> pending_;
    std::vector<SpokenCallout> speaking_;
    std::vector<SpokenCallout> heard_;
    std::vector<SpokenCallout> log_;
    std::size_t started_ = 0;
};

}  // namespace simcore
