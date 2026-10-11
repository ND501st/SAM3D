// simcore/c2_sources.hpp — where the battery's air picture comes from, and how the crew changes it (roadmap E4, 7.8).
//
// The PU-12M battery command post is the main source: the regiment's radars feed it, the operators plot a minute-old
// picture and the battery commander reads the targets out to the vehicles by voice. It is not the only way an Osa
// vehicle got its early warning. Armies the system served also used the early-warning radar sites' own voice and telecode
// nets, neighbouring batteries' radars, visual air-observation posts, and (later) digital data links; and a vehicle that
// lost them all fought on its own radar. docs/research/EM_C2_SOURCES_2026-10.md has the evidence.
//
// This file models that as a set of *sources*, each with its own plotting board (pu12.hpp):
//   * what it can see: the regiment's whole picture (relayed), its own radar out to the radar horizon, or what an
//     observer sees by eye;
//   * how it tells the vehicle: cadence, position errors, and the voice message (operator delay, message, pass-on);
//   * how far its link reaches, which voice net it shares (one message at a time on a net, so two sources on one net
//     delay each other), when it is down (scripted outages, a scenario failing it, or an EM link that cannot carry it).
// The crew listens to ONE source at a time (`RequestSource`, a few seconds to change). What it has been told while
// listening is its picture (`Known`); a source it does not listen to still talks, and still occupies its net.
// A commander who loses the active source for 2.5 reporting periods (the EM layer's own "degraded" threshold, 5 for
// "autonomous") may fall back down his preference list and come back to the main source once it has been up for a
// while: that is the NPC commander's behaviour (`auto_fallback`); a player's changes go through `RequestSource`.
//
// It only reads the world's targets; it decides nothing about the battle.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/geo.hpp"
#include "simcore/json.hpp"
#include "simcore/pu12.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

enum class C2Kind { BatteryPost, EwrVoice, EwrTelecode, NeighbourRadio, ObservationPost, DataLink };

inline const char* C2KindName(C2Kind k) {
    switch (k) {
        case C2Kind::BatteryPost: return "battery_post";
        case C2Kind::EwrVoice: return "ewr_voice";
        case C2Kind::EwrTelecode: return "ewr_telecode";
        case C2Kind::NeighbourRadio: return "neighbour_radio";
        case C2Kind::ObservationPost: return "observation_post";
        case C2Kind::DataLink: return "data_link";
    }
    return "?";
}

inline bool ParseC2Kind(const std::string& s, C2Kind& out) {
    for (C2Kind k : {C2Kind::BatteryPost, C2Kind::EwrVoice, C2Kind::EwrTelecode, C2Kind::NeighbourRadio, C2Kind::ObservationPost,
                     C2Kind::DataLink}) {
        if (s == C2KindName(k)) {
            out = k;
            return true;
        }
    }
    return false;
}

// What the source can see.
enum class C2Sense {
    Relayed,  // the regiment's picture, built from radars far away: no horizon of its own (the PU-12, a data link)
    Radar,    // its own radar at its own site: out to its range or the radar horizon, whichever is nearer
    Visual,   // an observer: out to the distance a man can tell an aircraft
};

inline const char* C2SenseName(C2Sense s) {
    switch (s) {
        case C2Sense::Relayed: return "relayed";
        case C2Sense::Radar: return "radar";
        case C2Sense::Visual: return "visual";
    }
    return "?";
}

// A time the source cannot tell the vehicle anything: destroyed, jammed, line cut, off the air.
struct C2Outage {
    double from_s = 0.0;
    double to_s = 1e18;  // open-ended unless set
    std::string reason;
};

struct C2SourceDef {
    std::string id;            // the instance: "pu12", "ewr_north"
    std::string type;          // the library entry it came from ("" for the built-in PU-12)
    std::string display_name;
    std::string short_name;    // the debrief's actor column (14 characters or fewer)
    C2Kind kind = C2Kind::BatteryPost;
    std::string medium = "voice";  // voice, telecode, wire or data (a label; the timing is in the board)
    std::string era;
    C2Sense sense = C2Sense::Relayed;
    PlottingBoardDef board;        // cadence, errors, designation range, voice timing
    double antenna_height_m = 0.0; // radar sites: for the radar horizon
    double link_range_m = 0.0;     // beyond this from the vehicle the link is down (0: no limit)
    std::string net;               // a voice net shared with other sources ("" : its own)
    std::string radio = "r123m";   // the EM library radio its link uses when the scenario has the EM layer (a source with a position)
    bool has_position = false;
    Vec3 position;                 // ENU (east, north); the site of a radar or post
    std::vector<C2Outage> outages;
};

struct C2Library {
    std::string id;
    std::vector<C2SourceDef> types;
    const C2SourceDef* Find(const std::string& type_id) const {
        for (const C2SourceDef& t : types) {
            if (t.id == type_id) return &t;
        }
        return nullptr;
    }
};

// How the crew's commander handles his sources (the scenario's "c2" block).
struct C2Policy {
    std::string primary = "pu12";        // the main source: the first choice, and what he returns to
    std::string active;                  // who he listens to at the start ("" : the primary)
    bool auto_fallback = true;           // an NPC commander changes source when the active one goes silent
    std::vector<std::string> fallback_order;  // after the primary, in this order
    double switch_s = 6.0;               // changing source: re-tuning the set, the call-up, until the first word (an estimate)
    double primary_stable_s = 60.0;      // the main source must have been heard this long before he goes back to it
    double heard_life_s = 0.0;           // a designation older than this (from its plot) is forgotten (0: never)
};

namespace detail {

// The library of source types (data/command/c2_sources.json).
class C2LibraryLoader : public BoardLoader {
public:
    bool Load(const json::Value& root, C2Library& lib) {
        if (!Keys(root, "", {"library_id", "display_name", "status", "notes", "sources"}) ||
            !RequireString(root, "", "library_id", lib.id)) {
            return false;
        }
        const json::Value* list = root.Find("sources");
        if (list == nullptr || !list->Is(json::Type::Array)) return Fail("sources", "required, a list");
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const json::Value& e = list->items[i];
            const std::string p = "sources[" + std::to_string(i) + "]";
            C2SourceDef d;
            std::string kind;
            std::string sense;
            if (!Keys(e, p, {"id", "display_name", "short_name", "kind", "medium", "era", "sense", "antenna_height_m", "link_range_m",
                             "board", "notes"}) ||
                !RequireString(e, p, "id", d.id) || !RequireString(e, p, "display_name", d.display_name) ||
                !RequireString(e, p, "kind", kind) || !RequireString(e, p, "sense", sense) ||
                !OptionalString(e, p, "short_name", d.short_name) || !OptionalString(e, p, "medium", d.medium) ||
                !OptionalString(e, p, "era", d.era)) {
                return false;
            }
            if (lib.Find(d.id) != nullptr) return Fail(p, "duplicate source type '" + d.id + "'");
            if (!ParseC2Kind(kind, d.kind)) return Fail(Join(p, "kind"), "unknown kind '" + kind + "'");
            if (sense == "relayed") {
                d.sense = C2Sense::Relayed;
            } else if (sense == "radar") {
                d.sense = C2Sense::Radar;
            } else if (sense == "visual") {
                d.sense = C2Sense::Visual;
            } else {
                return Fail(Join(p, "sense"), "relayed, radar or visual");
            }
            if (d.short_name.empty()) d.short_name = d.id.substr(0, 14);
            if (d.short_name.size() > 14) return Fail(Join(p, "short_name"), "14 characters at most (the debrief's column)");
            cite_path_ = Join(p, "board");
            const json::Value* b = e.Find("board");
            if (b == nullptr || !b->Is(json::Type::Object)) return Fail(cite_path_, "required, an object");
            if (!LoadBoardBody(*b, d.board)) return false;
            cite_path_ = p;
            if (d.sense == C2Sense::Radar && e.Find("antenna_height_m") == nullptr) {
                return Fail(Join(p, "antenna_height_m"), "a radar source needs its antenna height (for the radar horizon)");
            }
            if (e.Find("antenna_height_m") != nullptr && !CitedField(e, "antenna_height_m", d.antenna_height_m)) return false;
            if (e.Find("link_range_m") != nullptr && !CitedField(e, "link_range_m", d.link_range_m)) return false;
            d.board.id = d.id;
            d.board.display_name = d.display_name;
            lib.types.push_back(std::move(d));
        }
        return true;
    }

private:
    bool CitedField(const json::Value& e, const char* key, double& out) { return Cited(e, key, out); }
};

}  // namespace detail

inline bool LoadC2LibraryFile(const std::string& path, C2Library& out, std::string* error = nullptr) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text.str(), root, &parse_error)) {
        if (error != nullptr) *error = path + ": " + parse_error;
        return false;
    }
    C2Library lib;
    detail::C2LibraryLoader loader;
    if (!loader.Load(root, lib)) {
        if (error != nullptr) *error = path + ": " + loader.Message();
        return false;
    }
    out = std::move(lib);
    return true;
}

// How well the commander hears a source (the EM layer's thresholds for a superior's picture: 2.5 and 5 periods).
enum class C2State { Up, Degraded, Silent };

inline const char* C2StateName(C2State s) {
    switch (s) {
        case C2State::Up: return "up";
        case C2State::Degraded: return "degraded";
        case C2State::Silent: return "silent";
    }
    return "?";
}

// Does the link between a source and the vehicle carry its words? A default carries them all; the EM layer's version
// asks the radio link budget (propagation, terrain, jamming), so a jammer or a ridge can take a net down.
class C2LinkModel {
public:
    virtual ~C2LinkModel() = default;
    // The chance one transmission from `source` reaches the vehicle at time t (0 to 1).
    virtual double DeliveryProbability(const C2SourceDef& source, const Vec3& unit, double t) = 0;

    // A link with a real radio carries a source's voice messages itself (the EM layer: they go on the air for the times
    // the network worked out, radiate, and are delivered or lost by the link budget at the end of the transmission).
    virtual bool HasRadio(const C2SourceDef&) { return false; }
    virtual void SendVoice(const C2SourceDef&, long long /*tag*/, double /*start_s*/, double /*end_s*/) {}
    // 1 delivered, 0 lost, -1 not decided yet.
    virtual int VoiceOutcome(const C2SourceDef&, long long /*tag*/) { return -1; }
};

class C2Network {
public:
    // Something the debrief should say.
    struct Event {
        double t = 0.0;
        std::string actor;
        std::string subject;
        std::string text;
    };

    // What a display shows for one source.
    struct SourceView {
        std::string id;
        std::string name;
        C2Kind kind = C2Kind::BatteryPost;
        C2State state = C2State::Up;
        bool available = true;  // able to talk at all (not down, in reach)
        bool active = false;
        double last_contact_s = 0.0;
        int lost_messages = 0;
    };

    C2Network(C2Policy policy, unsigned seed) : policy_(std::move(policy)), seed_(seed) {}
    C2Network(const C2Network&) = delete;
    C2Network& operator=(const C2Network&) = delete;

    // Add a source. The board is made here (its navigation error is drawn once from the seed and the source's index).
    void AddSource(C2SourceDef def, const Vec3& unit) {
        auto s = std::make_unique<Source>();
        s->index = sources_.size();
        s->board = std::make_unique<PlottingBoard>(def.board, unit, seed_ + static_cast<unsigned>(17 * s->index));
        s->board->SetGated(true);
        if (def.has_position) s->board->SetSource(def.position);
        if (!def.net.empty()) {
            s->board->SetChannel(ChannelOf(def.net));
        }
        for (const C2Outage& o : def.outages) s->down.push_back({o.from_s, o.to_s, o.reason});
        s->def = std::move(def);
        sources_.push_back(std::move(s));
    }

    void SetLinkModel(C2LinkModel* model) { link_model_ = model; }

    bool Has(const std::string& id) const { return Find(id) != nullptr; }
    std::size_t Size() const { return sources_.size(); }
    const C2SourceDef& Def(std::size_t i) const { return sources_[i]->def; }
    const PlottingBoard* BoardOf(const std::string& id) const {
        const Source* s = Find(id);
        return s != nullptr ? s->board.get() : nullptr;
    }
    const C2Policy& Policy() const { return policy_; }

    // The source the crew is listening to ("" while it changes or when it has none) and the one it is changing to.
    const std::string& ActiveId() const { return active_; }
    bool Switching() const { return switching_; }
    const std::string& PendingId() const { return pending_; }

    // The commander (or the scenario, or the player) changes source. Returns false for an unknown source or one he
    // already has. "none" puts the vehicle on its own radar alone.
    bool RequestSource(const std::string& id, double t, const std::string& by, const std::string& why = std::string()) {
        const bool autonomous = id == "none";
        if (!autonomous && Find(id) == nullptr) return false;
        if (!switching_ && (autonomous ? active_.empty() : active_ == id)) return false;
        if (switching_ && pending_ == (autonomous ? std::string() : id)) return false;
        switching_ = true;
        pending_ = autonomous ? std::string() : id;
        switch_done_s_ = t + policy_.switch_s;
        active_.clear();  // listening to neither while the set is retuned
        const Source* to = Find(pending_);
        events_.push_back({t, by, autonomous ? "none" : id,
                           autonomous ? std::string("takes the vehicle off the net: its own radar alone") + (why.empty() ? "" : " (" + why + ")")
                                      : "changes to " + to->def.short_name + (why.empty() ? "" : " (" + why + ")")});
        return true;
    }

    // The scenario fails a source (destroyed, line cut, net jammed) from t, or restores it.
    void SetFailed(const std::string& id, bool failed, double t, const std::string& reason = std::string()) {
        Source* s = Find(id);
        if (s == nullptr) return;
        if (failed) {
            if (!s->down.empty() && s->down.back().to_s >= 1e17 && s->down.back().from_s <= t) return;
            s->down.push_back({t, 1e18, reason});
        } else {
            for (auto& d : s->down) {
                if (d.from_s <= t && d.to_s > t) d.to_s = t;
            }
        }
    }

    // Run to time t: sources plot and tell, links carry or lose their words, the crew hears the active one, and the
    // commander decides about changing. `truth` are the targets as they are, `unit` the vehicle's position.
    void Update(double t, const std::vector<BoardContact>& truth, const Vec3& unit) {
        unit_ = unit;
        if (!started_) {
            started_ = true;
            if (policy_.active.empty()) policy_.active = policy_.primary;
            if (Find(policy_.active) != nullptr) {
                active_ = policy_.active;
                Source* a = Find(active_);
                a->board->DiscardHeard(t);
            }
        }
        for (auto& sp : sources_) Run(*sp, t, truth);
        if (switching_ && t + 1e-9 >= switch_done_s_) {
            switching_ = false;
            active_ = pending_;
            pending_.clear();
            if (Source* a = Find(active_)) {
                a->board->DiscardHeard(t);  // what it said while the crew listened to the other is gone
                events_.push_back({t, "Commander", active_, "has " + a->def.short_name + " on the net"});
            }
        }
        if (!active_.empty()) Hear(*Find(active_), t);
        if (policy_.auto_fallback && !switching_) Decide(t);
        Expire(t);
    }

    // What the crew has been told and still remembers (the latest designation of each target, oldest first).
    std::vector<BoardMark> Known(double t) const {
        std::vector<BoardMark> out;
        for (const Heard& h : heard_) {
            if (policy_.heard_life_s > 0.0 && t - h.mark.time_s > policy_.heard_life_s) continue;
            out.push_back(h.mark);
        }
        return out;
    }
    // Who told it (for the display).
    std::string HeardFrom(const std::string& target_id) const {
        for (const Heard& h : heard_) {
            if (h.mark.target_id == target_id) return h.source;
        }
        return std::string();
    }
    static PlottingBoard::Designation DesignationFor(const BoardMark& m, const Vec3& from) { return PlottingBoard::Designate(m, from); }

    std::vector<SourceView> Views() const {
        std::vector<SourceView> out;
        for (const auto& sp : sources_) {
            SourceView v;
            v.id = sp->def.id;
            v.name = sp->def.display_name;
            v.kind = sp->def.kind;
            v.state = sp->state;
            v.available = sp->avail;
            v.active = sp->def.id == active_;
            v.last_contact_s = sp->last_contact_s;
            v.lost_messages = sp->board->LostCount();
            out.push_back(v);
        }
        return out;
    }

    std::vector<Event> TakeEvents() {
        std::vector<Event> out;
        out.swap(events_);
        return out;
    }

    // The radar horizon over a 4/3 Earth: 4.12 (sqrt(h1) + sqrt(h2)) km, heights in metres (vr_1989_tr1).
    static double RadarHorizonM(double antenna_m, double target_m) {
        return 4120.0 * (std::sqrt(antenna_m > 0.0 ? antenna_m : 0.0) + std::sqrt(target_m > 0.0 ? target_m : 0.0));
    }

private:
    struct Source {
        std::size_t index = 0;
        C2SourceDef def;
        std::unique_ptr<PlottingBoard> board;
        struct Down {
            double from_s;
            double to_s;
            std::string reason;
        };
        std::vector<Down> down;  // [from, to) when it cannot talk
        bool avail = true;
        bool init = false;
        double last_contact_s = 0.0;
        C2State state = C2State::Up;
        long long draws = 0;
    };

    struct Heard {
        BoardMark mark;
        std::string source;
        double heard_s = 0.0;
    };

    Source* Find(const std::string& id) {
        for (auto& s : sources_) {
            if (s->def.id == id) return s.get();
        }
        return nullptr;
    }
    const Source* Find(const std::string& id) const {
        for (const auto& s : sources_) {
            if (s->def.id == id) return s.get();
        }
        return nullptr;
    }

    VoiceChannel* ChannelOf(const std::string& net) {
        for (auto& n : nets_) {
            if (n.first == net) return n.second.get();
        }
        nets_.emplace_back(net, std::make_unique<VoiceChannel>());
        return nets_.back().second.get();
    }

    // SplitMix64 of (seed, source, draw number): a repeatable number in [0, 1) for each message.
    double Draw(Source& s) const {
        std::uint64_t z = (static_cast<std::uint64_t>(seed_) << 32) ^ (static_cast<std::uint64_t>(s.index) << 48) ^
                          static_cast<std::uint64_t>(++s.draws) * 0x9e3779b97f4a7c15ull;
        z += 0x9e3779b97f4a7c15ull;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        z ^= z >> 31;
        return static_cast<double>(z >> 11) / 9007199254740992.0;
    }

    static bool DownAt(const Source& s, double t) {
        return DownReason(s, t) != nullptr;
    }
    static const std::string* DownReason(const Source& s, double t) {
        for (const auto& d : s.down) {
            if (t + 1e-9 >= d.from_s && t < d.to_s) return &d.reason;
        }
        return nullptr;
    }

    bool InReach(const Source& s) const {
        if (s.def.link_range_m <= 0.0 || (!s.def.has_position && s.def.sense == C2Sense::Relayed)) return true;
        const Vec3 from = s.def.has_position ? s.def.position : unit_;
        return (from - unit_).HorizontalLength() <= s.def.link_range_m;
    }

    double LinkP(const Source& s, double t) {
        return link_model_ != nullptr ? link_model_->DeliveryProbability(s.def, unit_, t) : 1.0;
    }

    // Did the words it said reach the vehicle (it was up when they ended, in reach, and the link carried them)?
    bool Carried(Source& s, double ended_s, double t) {
        if (DownAt(s, ended_s) || !InReach(s)) return false;
        const double p = LinkP(s, t);
        return p >= 1.0 || Draw(s) < p;
    }

    std::vector<BoardContact> Sees(const Source& s, const std::vector<BoardContact>& truth) const {
        if (s.def.sense == C2Sense::Relayed) return truth;
        std::vector<BoardContact> out;
        for (const BoardContact& c : truth) {
            const double d = (c.position - s.def.position).HorizontalLength();
            if (d > s.def.board.max_range_m) continue;
            if (s.def.sense == C2Sense::Radar) {
                const double h = c.height_agl_m >= 0.0 ? c.height_agl_m : c.altitude_m;
                if (d > RadarHorizonM(s.def.antenna_height_m, h)) continue;
            }
            out.push_back(c);
        }
        return out;
    }

    void Run(Source& s, double t, const std::vector<BoardContact>& truth) {
        s.board->SetCentre(unit_);
        const bool up = !DownAt(s, t) && InReach(s);
        if (!s.init) {
            s.init = true;
            s.avail = up;
            s.last_contact_s = t;  // the net is up when the scenario starts
        }
        if (up != s.avail) {
            s.avail = up;
            const std::string* why = DownReason(s, t);
            events_.push_back({t, s.def.short_name, "link",
                               up ? std::string("back on the air")
                                  : (why != nullptr ? "off the air" + (why->empty() ? std::string() : " (" + *why + ")")
                                                    : std::string("out of reach of the vehicle"))});
            if (!up) s.board->CancelQueued(t);
        }
        if (s.avail && s.board->Due(t)) {
            const std::size_t before = s.board->Marks().size();
            if (s.board->Update(t, Sees(s, truth))) {
                if (Carried(s, t, t)) s.last_contact_s = t;  // the routine call that goes with each plot
                for (std::size_t k = before; k < s.board->Marks().size(); ++k) {
                    const BoardMark& m = s.board->Marks()[k];
                    bool first = true;
                    for (std::size_t j = 0; j < before && first; ++j) first = s.board->Marks()[j].target_id != m.target_id;
                    if (first) events_.push_back({t, s.def.short_name, m.target_id, "on the board: " + s.board->Describe(m)});
                }
            }
        }
        const bool on_radio = link_model_ != nullptr && link_model_->HasRadio(s.def);
        if (on_radio) {
            for (const PlottingBoard::Sent& m : s.board->TakeUnsent()) link_model_->SendVoice(s.def, static_cast<long long>(m.index), m.start_s, m.end_s);
        }
        s.board->ResolveMessages(t, [&](const BoardMark&, double ended, std::size_t index) {
            int r = 1;
            if (DownAt(s, ended) || !InReach(s)) {
                r = 0;
            } else if (on_radio) {
                r = link_model_->VoiceOutcome(s.def, static_cast<long long>(index));
            } else {
                r = Carried(s, ended, t) ? 1 : 0;
            }
            if (r > 0) s.last_contact_s = t > s.last_contact_s ? t : s.last_contact_s;
            return r;
        });
        const double period = s.def.board.update_period_s;
        const double since = t - s.last_contact_s;
        const C2State st = since <= 2.5 * period ? C2State::Up : since <= 5.0 * period ? C2State::Degraded : C2State::Silent;
        if (st != s.state) {
            events_.push_back({t, s.def.short_name, "link", std::string("net contact ") + C2StateName(s.state) + " -> " + C2StateName(st)});
            s.state = st;
        }
    }

    void Hear(Source& s, double t) {
        for (const BoardMark& m : s.board->PollHeard(t)) {
            bool replaced = false;
            for (Heard& h : heard_) {
                if (h.mark.target_id == m.target_id) {
                    h.mark = m;
                    h.source = s.def.id;
                    h.heard_s = t;
                    replaced = true;
                }
            }
            if (!replaced) heard_.push_back({m, s.def.id, t});
            const PlottingBoard::Designation d = PlottingBoard::Designate(m, unit_);
            char detail[128];
            std::snprintf(detail, sizeof(detail), "designation %04d: azimuth %02d-%02d, range %.0f km, height %d hm", m.number,
                          d.azimuth_mils / 100, d.azimuth_mils % 100, d.range_m / 1000.0, m.height_hm);
            std::string text = detail;
            if (s.def.id != policy_.primary) text += " [" + s.def.short_name + "]";
            events_.push_back({t, "Commander", m.target_id, text});
        }
    }

    void Expire(double t) {
        if (policy_.heard_life_s <= 0.0) return;
        heard_.erase(std::remove_if(heard_.begin(), heard_.end(),
                                    [&](const Heard& h) { return t - h.mark.time_s > policy_.heard_life_s; }),
                     heard_.end());
    }

    // The commander's choice: the main source first, then his list.
    std::vector<std::string> Preference() const {
        std::vector<std::string> out;
        if (Find(policy_.primary) != nullptr) out.push_back(policy_.primary);
        for (const std::string& id : policy_.fallback_order) {
            if (Find(id) != nullptr && std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
        }
        return out;
    }

    void Decide(double t) {
        const Source* a = Find(active_);
        const std::vector<std::string> order = Preference();
        if (a != nullptr && a->state != C2State::Silent) {
            // On a standby: go back to the main source once it has been heard steadily.
            const Source* p = Find(policy_.primary);
            if (p != nullptr && a != p && p->state == C2State::Up) {
                if (primary_up_since_ < 0.0) primary_up_since_ = t;
                if (t - primary_up_since_ >= policy_.primary_stable_s) {
                    primary_up_since_ = -1.0;
                    RequestSource(policy_.primary, t, "Commander", "the main source is back");
                }
            } else {
                primary_up_since_ = -1.0;
            }
            return;
        }
        if (a == nullptr) return;  // he took the vehicle off the net on purpose, or is changing
        primary_up_since_ = -1.0;
        for (const std::string& id : order) {
            const Source* c = Find(id);
            if (c == nullptr || id == active_ || c->state == C2State::Silent) continue;
            RequestSource(id, t, "Commander", a->def.short_name + " silent");
            return;
        }
        if (!told_none_) {
            told_none_ = true;
            events_.push_back({t, "Commander", active_, "no source answers: the vehicle fights on its own radar"});
        }
    }

    C2Policy policy_;
    unsigned seed_ = 1;
    bool started_ = false;
    Vec3 unit_;
    std::vector<std::unique_ptr<Source>> sources_;
    std::vector<std::pair<std::string, std::unique_ptr<VoiceChannel>>> nets_;
    std::string active_;
    std::string pending_;
    bool switching_ = false;
    double switch_done_s_ = 0.0;
    double primary_up_since_ = -1.0;
    bool told_none_ = false;
    std::vector<Heard> heard_;
    std::vector<Event> events_;
    C2LinkModel* link_model_ = nullptr;
};

}  // namespace simcore
