// simcore/radio_nets.hpp — the battery's radio data and its nets: frequencies by number, scheduled and ordered changes,
// jamming, emission control and telephone lines (docs/crew/12_COMMUNICATIONS.md §2.4, 2.7, 2.8).
//
// The radio data (радиоданные, rrs_1986 art. 2) is the scenario's "radio_data": for each net, the own vehicle's set on it,
// its main station (the senior commander's: "self" when it is this vehicle), the call signs, its frequencies by
// conditional number (1 working, 2 spare, 3 reserve) and the times of its scheduled changes. A set is on a net when its
// preset is the net's present number (the presets hold the net's frequencies in that order: SAM3D's arrangement).
//
// What the nets do (rrs_1986, pvo_bu_ix_1984, palii_1989):
//   * Scheduled changes: at the time every station is on the new number, with no word on air.
//   * Unscheduled changes only on the main station's order «Переходите на запасную частоту»; a member who heard it turns
//     its set and waits for the main station's call; one who missed it finds the net after its clock (1-2 min: call the
//     main station; 5 min: both frequencies; 10 min: report the loss): `Member`.
//   * Jamming (the scenario's list): a net or a band, from-to, weak or strong, barrage or spot. A spot jammer sits on the
//     number it found and finds the net's new one after its `follow_s` (never, if negative). Strong jamming loses every
//     message at a receiver, weak each with `weak_jam_loss`.
//   * Emission control: radio silence on the nets it covers, the exempt ones (air data, designation, warning: vst_1988)
//     excepted. Time on air adds up per net in a window; past `df_fix_s` the net is taken to be fixed by direction finding
//     and, where the scenario allows, a spot jammer comes on it `df_reaction_s` later (palii_1989; SAM3D's inference).
//   * Lines: the RTK-1's five, laid or not, and whom they reach. A line has no emission and no jamming.
// It decides nothing about the battle: the runner asks it whether a message gets through.
#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace simcore {

struct RadioNetDef {
    std::string id;                      // "battery", "regiment", "air_picture", "covered"
    std::string name_ru, name_en;
    std::string set;                     // the own vehicle's set on it ("fug4", "right"); "" : none
    std::string main = "self";           // the main station: "self", or who ("regiment", "post", "covered", "radar")
    std::string own_call_sign;           // this vehicle's
    std::string main_call_sign;          // the main station's
    std::vector<double> frequencies_mhz; // by conditional number: [0] = no. 1 (working), [1] = no. 2 (spare), ...
    struct Change {
        double at_s = 0.0;
        int to = 1;                      // conditional number
    };
    std::vector<Change> changes;         // scheduled: no word on air
    std::vector<Change> orders;          // the main station's orders (when it is not "self")
    bool exempt = false;                 // carries air data, designations and warnings: open under radio silence
};

struct JammingDef {
    std::vector<std::string> nets;       // by id; empty: by band
    double band_lo_mhz = 0.0, band_hi_mhz = 0.0;
    double from_s = 0.0, to_s = 1e18;
    bool strong = true;
    bool spot = false;                   // a spot jammer on one frequency (finds a new one after follow_s)
    double follow_s = -1.0;              // < 0: never
};

struct LineDef {
    int line = 1;                        // the RTK-1's lever 1-5
    std::string to;                      // a launcher's call sign, "regiment", "radar"
    std::string to_name;
    bool laid = true;
    struct Call {
        double at_s = 0.0;
        std::string text;                // what the far end says once the line is answered
    };
    std::vector<Call> calls;             // the far end rings at these times
};

struct CommsTuning {
    double headset_s = 3.0;
    double box_select_s = 1.0;
    double answer_line_s = 2.0;
    double recognise_jamming_s = 6.0;
    double order_spare_s = 20.0;
    double weak_jam_loss = 0.4;
    double call_wait_s = 90.0;
    double both_frequencies_s = 300.0;
    double loss_report_s = 600.0;
    double df_window_s = 300.0;
    double df_fix_s = 30.0;
    double df_reaction_s = 120.0;
    double warn_covered_m = 60000.0;     // a hostile this near: the covered unit is warned («Воздух», ПТ-2)
};

struct RadioDataDef {
    bool present = false;
    std::vector<RadioNetDef> nets;
    std::vector<JammingDef> jamming;
    std::vector<LineDef> lines;
    bool jamming_follows_df = false;     // a net fixed by direction finding draws a spot jammer
    double df_jam_s = 600.0;             // how long that jammer stays on
    bool silence = false;                // radio silence at the start
    struct SilenceOrder {
        double at_s = 0.0;
        bool on = true;
    };
    std::vector<SilenceOrder> silence_orders;  // the regiment's orders on its command net (ПТ-1), heard or not
    const RadioNetDef* Find(const std::string& id) const {
        for (const RadioNetDef& n : nets) {
            if (n.id == id) return &n;
        }
        return nullptr;
    }
    const RadioNetDef* OnSet(const std::string& set) const {
        for (const RadioNetDef& n : nets) {
            if (n.set == set) return &n;
        }
        return nullptr;
    }
    const LineDef* LineTo(const std::string& to) const {
        for (const LineDef& l : lines) {
            if (l.to == to && l.laid) return &l;
        }
        return nullptr;
    }
    const LineDef* Line(int n) const {
        for (const LineDef& l : lines) {
            if (l.line == n) return &l;
        }
        return nullptr;
    }
};

class RadioNets {
public:
    // A station of a net that is not this vehicle (a launcher on the battery net): where it is, and its clock when it
    // lost the main station.
    struct Member {
        int on = 1;                      // the number it is tuned to
        bool waiting = false;            // turned to a new number, waiting for the main station's call
        double lost_s = -1.0;            // when it lost the net (missed an order), < 0: not lost
        bool reported_loss = false;
    };
    struct Event {
        double t = 0.0;
        std::string net;
        std::string text;
    };

    RadioNets() = default;
    RadioNets(const RadioDataDef& def, const CommsTuning& tuning, unsigned seed) : def_(def), tuning_(tuning), rng_(seed) {
        for (const RadioNetDef& n : def_.nets) {
            NetState s;
            current_[n.id] = s;
        }
        jam_on_.resize(def_.jamming.size(), 0);
        jam_retarget_.resize(def_.jamming.size(), -1.0);
        silence_ = def_.silence;
    }

    const RadioDataDef& Def() const { return def_; }
    const CommsTuning& Tuning() const { return tuning_; }
    // Jamming that comes on during the run (a lesson's cue).
    void AddJamming(const JammingDef& j) {
        def_.jamming.push_back(j);
        jam_on_.push_back(0);
        jam_retarget_.push_back(-1.0);
    }

    // ---- frequencies
    int Current(const std::string& net) const {
        const auto it = current_.find(net);
        return it != current_.end() ? it->second.number : 1;
    }
    double CurrentMhz(const std::string& net) const { return Mhz(net, Current(net)); }
    double Mhz(const std::string& net, int number) const {
        const RadioNetDef* d = def_.Find(net);
        if (d == nullptr || number < 1 || static_cast<std::size_t>(number) > d->frequencies_mhz.size()) return 0.0;
        return d->frequencies_mhz[static_cast<std::size_t>(number - 1)];
    }
    int Numbers(const std::string& net) const {
        const RadioNetDef* d = def_.Find(net);
        return d != nullptr ? static_cast<int>(d->frequencies_mhz.size()) : 0;
    }
    // The main station moves its net to `number` now (its order, or a scheduled change). False if it is there already.
    bool MoveTo(const std::string& net, int number, double t, const std::string& why) {
        auto it = current_.find(net);
        if (it == current_.end() || number < 1 || number > Numbers(net) || it->second.number == number) return false;
        it->second.number = number;
        it->second.moved_s = t;
        events_.push_back({t, net, "to frequency no. " + std::to_string(number) + " (" + why + ")"});
        return true;
    }
    double MovedS(const std::string& net) const {
        const auto it = current_.find(net);
        return it != current_.end() ? it->second.moved_s : -1.0;
    }

    // Due now: scheduled changes and the external main stations' orders (an order is returned so the runner can say it
    // on air: only those who hear it know).
    struct Due {
        std::string net;
        int to = 1;
        bool ordered = false;            // said on air by the main station (else scheduled: silent)
    };
    std::vector<Due> Step(double t) {
        std::vector<Due> out;
        for (const RadioNetDef& n : def_.nets) {
            NetState& s = current_[n.id];
            while (s.next_change < n.changes.size() && n.changes[s.next_change].at_s <= t + 1e-9) {
                const int to = n.changes[s.next_change++].to;
                if (MoveTo(n.id, to, t, "scheduled")) out.push_back({n.id, to, false});
            }
            while (s.next_order < n.orders.size() && n.orders[s.next_order].at_s <= t + 1e-9) {
                const int to = n.orders[s.next_order++].to;
                if (MoveTo(n.id, to, t, "ordered by " + n.main)) out.push_back({n.id, to, true});
            }
        }
        StepJammers(t);
        StepDf(t);
        return out;
    }

    // ---- jamming: 0 none, 1 weak, 2 strong, on the net's present frequency
    int Jam(const std::string& net, double t) const { return JamOn(net, Current(net), t); }
    int JamOn(const std::string& net, int number, double t) const {
        int worst = 0;
        const double mhz = Mhz(net, number);
        for (std::size_t i = 0; i < def_.jamming.size(); ++i) {
            const JammingDef& j = def_.jamming[i];
            if (t < j.from_s || t >= j.to_s) continue;
            bool covers = false;
            if (!j.nets.empty()) {
                for (const std::string& id : j.nets) covers = covers || id == net;
                if (covers && j.spot) covers = jam_on_[i] == number;
            } else {
                covers = mhz > 0.0 && mhz >= j.band_lo_mhz && mhz <= j.band_hi_mhz;
            }
            if (covers) worst = std::max(worst, j.strong ? 2 : 1);
        }
        for (const DfJammer& d : df_jammers_) {
            if (d.net == net && t >= d.from_s && t < d.to_s && d.number == number) worst = 2;
        }
        return worst;
    }
    // Does one message get through a net's jamming at a receiver? Strong: never; weak: by chance (said twice: either).
    bool GetsThrough(const std::string& net, double t, bool said_twice = false) {
        const int j = Jam(net, t);
        if (j == 0) return true;
        if (j >= 2) return false;
        const double loss = said_twice ? tuning_.weak_jam_loss * tuning_.weak_jam_loss : tuning_.weak_jam_loss;
        return Uniform() >= loss;
    }

    // ---- emission control
    bool Silence() const { return silence_; }
    void SetSilence(bool on, double t, const std::string& by) {
        if (on == silence_) return;
        silence_ = on;
        events_.push_back({t, std::string(), std::string(on ? "radio silence ordered by " : "radio silence lifted by ") + by});
    }
    // May this net carry routine traffic now? (Exempt nets always; else not under silence.)
    bool Open(const std::string& net) const {
        const RadioNetDef* d = def_.Find(net);
        return !silence_ || (d != nullptr && d->exempt);
    }
    // The own vehicle transmitted on a net for `seconds`: time on air for direction finding; under silence on a net that
    // is not exempt, a violation. Returns true when this transmission broke the silence.
    bool Transmitted(const std::string& net, double t, double seconds) {
        NetState& s = current_[net];
        s.on_air.push_back({t, seconds});
        ++s.transmissions;
        s.air_s += seconds;
        if (!Open(net)) {
            ++violations_;
            events_.push_back({t, net, "transmitted under radio silence"});
            return true;
        }
        return false;
    }
    double OnAirS(const std::string& net) const {
        const auto it = current_.find(net);
        return it != current_.end() ? it->second.air_s : 0.0;
    }
    int Transmissions(const std::string& net) const {
        const auto it = current_.find(net);
        return it != current_.end() ? it->second.transmissions : 0;
    }
    int Violations() const { return violations_; }
    bool Fixed(const std::string& net) const {
        const auto it = current_.find(net);
        return it != current_.end() && it->second.fixed_s >= 0.0;
    }

    // ---- the members of nets this vehicle runs as main station
    Member& MemberOf(const std::string& net, const std::string& who) { return members_[net + "/" + who]; }
    const Member* FindMember(const std::string& net, const std::string& who) const {
        const auto it = members_.find(net + "/" + who);
        return it != members_.end() ? &it->second : nullptr;
    }
    // A member is on the net when tuned to its present number and not still waiting to be called.
    bool MemberOnNet(const std::string& net, const std::string& who) const {
        const Member* m = FindMember(net, who);
        return m == nullptr || (m->on == Current(net) && !m->waiting);
    }
    // A member's clock after missing an order (rrs_1986): returns what it did this step ("" nothing).
    std::string StepMember(const std::string& net, const std::string& who, double t) {
        Member& m = MemberOf(net, who);
        if (m.on == Current(net) || m.lost_s < 0.0) return std::string();
        if (t - m.lost_s >= tuning_.both_frequencies_s) {
            m.on = Current(net);
            m.waiting = false;
            m.lost_s = -1.0;
            m.reported_loss = false;
            return "finds the net on frequency no. " + std::to_string(m.on) + " (working the old and the new)";
        }
        if (t - m.lost_s >= tuning_.loss_report_s && !m.reported_loss) {
            m.reported_loss = true;
            return "reports the loss of communication";
        }
        return std::string();
    }

    std::vector<Event> TakeEvents() {
        std::vector<Event> out;
        out.swap(events_);
        return out;
    }

    double Uniform() { return static_cast<double>(rng_()) / 4294967296.0; }

private:
    struct NetState {
        int number = 1;
        double moved_s = -1.0;
        std::size_t next_change = 0;
        std::size_t next_order = 0;
        std::vector<std::pair<double, double>> on_air;  // (start, seconds)
        double air_s = 0.0;
        int transmissions = 0;
        double fixed_s = -1.0;
    };
    struct DfJammer {
        std::string net;
        int number = 1;
        double from_s = 0.0, to_s = 0.0;
    };

    void StepJammers(double t) {
        for (std::size_t i = 0; i < def_.jamming.size(); ++i) {
            const JammingDef& j = def_.jamming[i];
            if (!j.spot || j.nets.empty()) continue;
            const std::string& net = j.nets.front();
            if (t < j.from_s) continue;
            if (jam_on_[i] == 0) jam_on_[i] = Current(net);  // it finds the net where it is when it starts
            if (jam_on_[i] != Current(net)) {
                if (j.follow_s < 0.0) continue;
                if (jam_retarget_[i] < 0.0) jam_retarget_[i] = t + j.follow_s;
                if (t >= jam_retarget_[i]) {
                    jam_on_[i] = Current(net);
                    jam_retarget_[i] = -1.0;
                    events_.push_back({t, net, "the spot jammer has found frequency no. " + std::to_string(jam_on_[i])});
                }
            }
        }
        for (DfJammer& d : df_jammers_) {
            if (t >= d.from_s && d.number != Current(d.net) && t >= d.from_s + 60.0) d.number = Current(d.net);  // it follows within a minute
        }
    }

    void StepDf(double t) {
        for (auto& kv : current_) {
            NetState& s = kv.second;
            double sum = 0.0;
            std::vector<std::pair<double, double>> kept;
            for (const auto& a : s.on_air) {
                if (t - a.first <= tuning_.df_window_s) {
                    kept.push_back(a);
                    sum += a.second;
                }
            }
            s.on_air.swap(kept);
            if (s.fixed_s < 0.0 && sum >= tuning_.df_fix_s) {
                s.fixed_s = t;
                events_.push_back({t, kv.first, "the net's emissions are long enough to be fixed by direction finding"});
                if (def_.jamming_follows_df) {
                    df_jammers_.push_back({kv.first, s.number, t + tuning_.df_reaction_s, t + tuning_.df_reaction_s + def_.df_jam_s});
                }
            }
        }
    }

    RadioDataDef def_;
    CommsTuning tuning_;
    std::map<std::string, NetState> current_;
    std::map<std::string, Member> members_;
    std::vector<int> jam_on_;
    std::vector<double> jam_retarget_;
    std::vector<DfJammer> df_jammers_;
    std::vector<Event> events_;
    bool silence_ = false;
    int violations_ = 0;
    std::mt19937 rng_{1u};
};

}  // namespace simcore
