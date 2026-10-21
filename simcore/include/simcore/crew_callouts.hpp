// simcore/crew_callouts.hpp — the crew's voice: orders, acknowledgements, reports and reminders (roadmap 8.4).
//
// docs/crew/04_COMMANDS_AND_CALLOUTS.md is the specification; the words are the crew data's catalogue
// (data/crew/<system>_crew.json, `callouts`). A CalloutBus carries them:
//   * one voice at a time per channel: the intercom (R-124) inside the vehicle, the battery net (R-123M) outside;
//   * urgent callouts (priority 0) are spoken before everything queued; otherwise first come, first spoken;
//   * speaking takes time: 0.4 s plus 0.35 s a word (about 2.8 words a second; sam3d estimate);
//   * a callout can be delayed before it is queued (the speaker noticing and thinking: his skill).
// What is said is filled in from fields: "{azimuth}", "{range_km}", "{seat}", "{what}" and so on.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdio>
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
    std::string channel;  // "intercom" or "battery net"
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
    std::size_t seq = 0;  // the order it started in (1, 2, ...): the voice plays each line once (roadmap 8.10)
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
    explicit CalloutBus(const CrewData* crew) : crew_(crew) {}

    // Queue the catalogue's callout `id` from `from` (a station), after `delay_s`. Returns false for an unknown id.
    bool Say(double now_s, const std::string& id, const std::string& from, const std::map<std::string, std::string>& fields = {},
             double delay_s = 0.0, const KnownTarget* about = nullptr) {
        const CalloutDef* def = crew_ != nullptr ? crew_->FindCallout(id) : nullptr;
        if (def == nullptr) return false;
        SpokenCallout s;
        s.id = id;
        s.from = from;
        s.to = def->to;
        s.channel = def->kind == CalloutKind::Radio ? "battery net" : "intercom";
        s.ru = FillCallout(def->ru, fields);
        std::map<std::string, std::string> en = fields;  // "<key>_en" gives the English for a field
        for (const auto& f : fields) {
            const auto it = fields.find(f.first + "_en");
            if (it != fields.end()) en[f.first] = it->second;
        }
        s.en = FillCallout(def->en, en);
        s.priority = def->priority;
        s.queued_s = now_s + (delay_s > 0.0 ? delay_s : 0.0);
        if (about != nullptr) {
            s.has_about = true;
            s.about = *about;
        }
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
                heard_.push_back(s);
                log_.push_back(s);
            } else {
                still.push_back(std::move(s));
            }
        }
        speaking_.swap(still);
        // Start: per channel, the most urgent of those due, oldest first.
        for (;;) {
            int best = -1;
            for (std::size_t i = 0; i < pending_.size(); ++i) {
                const SpokenCallout& s = pending_[i];
                if (s.queued_s > now_s + 1e-9 || Busy(s.channel)) continue;
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
            s.end_s = now_s + SpeakingTimeS(s.ru);
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

private:
    bool Busy(const std::string& channel) const {
        for (const SpokenCallout& s : speaking_) {
            if (s.channel == channel) return true;
        }
        return false;
    }

    const CrewData* crew_;
    std::vector<SpokenCallout> pending_;
    std::vector<SpokenCallout> speaking_;
    std::vector<SpokenCallout> heard_;
    std::vector<SpokenCallout> log_;
    std::size_t started_ = 0;
};

}  // namespace simcore
