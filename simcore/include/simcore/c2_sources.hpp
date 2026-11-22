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
    bool interference = false;  // noise on the net rather than nobody sending (a set's АПЧ off may ride it out: radio_sets.hpp)
};

// A radar of the regiment's control battery that feeds the PU-12M (roadmap 7.1): a target is on the PU-12's board only if
// some radar of its feed covers it (within range, above that radar's horizon). No feed: the board sees every target.
struct RegimentRadar {
    std::string id;       // the instance: "p19_a"
    std::string type;     // the library entry: "p19"
    std::string name;
    Vec3 position;        // ENU (east, north)
    double max_range_m = 0.0;
    double antenna_height_m = 0.0;
    int switch_position = 0;  // the PU-12M's primary-source switch position for this radar's type (rwd_mb3 pu12e; 0: any)
    // Works with a ground interrogator whose replies go to the PU-12M's ASPD with the echoes when cabled to it (p19_to
    // 4.3.1, 4.3.3: the 1L23-6; flak11 PU12-0's figure: "Kennung" on the radar's cable).
    bool iff_interrogator = false;
    // Off the air at these times (the ARM drill, a fault): its picture and replies stop, and its crew ring the post on the
    // service line if one is laid (scenario "post.radar.outages").
    std::vector<C2Outage> outages;
};

struct RegimentRadarLibrary {
    std::string id;
    std::vector<RegimentRadar> types;  // position unset
    const RegimentRadar* Find(const std::string& type_id) const {
        for (const RegimentRadar& r : types) {
            if (r.id == type_id) return &r;
        }
        return nullptr;
    }
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
    double frequency_hz = 0.0;     // the net's frequency on the frequency plan (0: the middle of the radio's band)
    bool has_position = false;
    Vec3 position;                 // ENU (east, north); the site of a radar or post
    std::vector<C2Outage> outages;
    std::vector<RegimentRadar> feed;  // a relayed source (the PU-12): the radars whose picture it holds; empty = all targets
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
    // The vehicle has two R-123M sets, each with four fixed presets (r123_refs), one frequency plan for both. The
    // crew listens to one set; the other is the standby and is monitored. Presets name the sources on them ("" : free).
    std::vector<std::string> presets;    // up to four source ids (empty: the primary, then the fallback list, then the rest)
    double select_s = 3.0;               // turning a set to another preset, or to the other set, and the call-up (an estimate)
    double retune_s = 30.0;              // putting a source that is on no preset onto one (an estimate)
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

// The library of regiment radar types (data/command/regiment_radars.json).
class RegimentRadarLoader : public BoardLoader {
public:
    bool Load(const json::Value& root, RegimentRadarLibrary& lib) {
        if (!Keys(root, "", {"library_id", "display_name", "status", "notes", "radars"}) ||
            !RequireString(root, "", "library_id", lib.id)) {
            return false;
        }
        const json::Value* list = root.Find("radars");
        if (list == nullptr || !list->Is(json::Type::Array)) return Fail("radars", "required, a list");
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const json::Value& e = list->items[i];
            const std::string p = "radars[" + std::to_string(i) + "]";
            RegimentRadar r;
            if (!Keys(e, p, {"id", "display_name", "max_range_m", "antenna_height_m", "switch_position", "iff_interrogator", "notes"}) ||
                !RequireString(e, p, "id", r.id) ||
                !RequireString(e, p, "display_name", r.name)) {
                return false;
            }
            for (const RegimentRadar& other : lib.types) {
                if (other.id == r.id) return Fail(p, "duplicate radar type '" + r.id + "'");
            }
            cite_path_ = p;
            if (!Cited(e, "max_range_m", r.max_range_m) || !Cited(e, "antenna_height_m", r.antenna_height_m)) return false;
            if (r.max_range_m <= 0.0) return Fail(Join(p, "max_range_m"), "must be > 0");
            if (e.Find("switch_position") != nullptr) {
                double pos = 0.0;
                if (!Cited(e, "switch_position", pos)) return false;
                if (pos < 1.0 || pos > 3.0 || pos != std::floor(pos)) return Fail(Join(p, "switch_position"), "1, 2 or 3");
                r.switch_position = static_cast<int>(pos);
            }
            if (e.Find("iff_interrogator") != nullptr) {
                double iff = 0.0;
                if (!Cited(e, "iff_interrogator", iff)) return false;
                if (iff != 0.0 && iff != 1.0) return Fail(Join(p, "iff_interrogator"), "0 or 1");
                r.iff_interrogator = iff == 1.0;
            }
            lib.types.push_back(std::move(r));
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadRegimentRadarsFile(const std::string& path, RegimentRadarLibrary& out, std::string* error = nullptr) {
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
    RegimentRadarLibrary lib;
    detail::RegimentRadarLoader loader;
    if (!loader.Load(root, lib)) {
        if (error != nullptr) *error = path + ": " + loader.Message();
        return false;
    }
    out = std::move(lib);
    return true;
}

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
    // Loss in the vehicle's own receiving chain (the radio's squelch, a shorter antenna) on every source's words.
    virtual void SetReceiveLossDb(double /*db*/) {}
};

class C2Network {
public:
    // Something the debrief should say.
    struct Event {
        double t = 0.0;
        std::string actor;
        std::string subject;
        std::string text;
        // A designation heard (actor "Commander"): its number, azimuth in mils, range and height; number 0 otherwise.
        int number = 0;
        int azimuth_mils = 0;
        double range_m = 0.0;
        int height_hm = 0;
        // A group's centre the post could not resolve, and the selection rule to pick the aircraft by (BoardMark).
        bool group_centre = false;
        int rule = 0;
    };

    // What a display shows for one source.
    struct SourceView {
        std::string id;
        std::string name;
        C2Kind kind = C2Kind::BatteryPost;
        C2State state = C2State::Up;
        bool available = true;  // able to talk at all (not down, in reach)
        bool active = false;
        bool monitored = false;  // on the preset of one of the two sets, so the commander can hear whether it is alive
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

    // A source that moves (a neighbouring vehicle of the battery): its radar and its link now start from `at`.
    void SetSourcePosition(const std::string& id, const Vec3& at) {
        Source* s = Find(id);
        if (s == nullptr) return;
        s->def.position = at;
        s->def.has_position = true;
        s->board->SetSource(at);
    }

    // The source is the real PU-12M of a battery link (battery_link.hpp): its board plots nothing itself, the
    // designations come by Relay, and while it is on the air its routine calls keep the net alive.
    void SetExternal(const std::string& id) {
        if (Source* s = Find(id)) s->board->SetExternal(true);
    }
    bool Relay(const std::string& id, const BoardMark& m, double start_s, double end_s) {
        Source* s = Find(id);
        if (s == nullptr) return false;
        s->board->Relay(m, start_s, end_s);
        return true;
    }

    bool Has(const std::string& id) const { return Find(id) != nullptr; }
    std::size_t Size() const { return sources_.size(); }
    const C2SourceDef& Def(std::size_t i) const { return sources_[i]->def; }
    const PlottingBoard* BoardOf(const std::string& id) const {
        const Source* s = Find(id);
        return s != nullptr ? s->board.get() : nullptr;
    }
    const C2Policy& Policy() const { return policy_; }

    // The source the crew is listening to ("" when off the net) and the one it is changing to.
    const std::string& ActiveId() const { return active_; }
    bool Switching() const { return op_.active; }
    const std::string& PendingId() const { return pending_; }

    static constexpr int kPresets = 4;

    // What a display shows for the two R-123M sets.
    struct RadioView {
        std::string name;    // "left" or "right"
        int preset = 1;      // 1 to 4
        std::string source;  // the source on that preset ("" : none)
        bool listening = false;
    };
    std::vector<RadioView> Radios() const {
        std::vector<RadioView> out;
        for (int i = 0; i < 2; ++i) {
            RadioView v;
            v.name = i == 0 ? "left" : "right";
            v.preset = sets_[i].preset;
            v.source = presets_[static_cast<std::size_t>(sets_[i].preset - 1)];
            v.listening = i == listening_ && !off_net_;
            out.push_back(v);
        }
        return out;
    }
    // The set the headset is on (0 left, 1 right).
    int ListeningSet() const { return listening_; }
    // The source on preset 1..4 ("" : free).
    const std::string& PresetSource(int preset) const {
        static const std::string none;
        return preset >= 1 && preset <= kPresets ? presets_[static_cast<std::size_t>(preset - 1)] : none;
    }
    // Is the source on the preset of one of the two sets, so that the commander hears whether it is alive?
    bool Monitored(const std::string& id) const {
        return !id.empty() && ((sets_[0].receiving && presets_[static_cast<std::size_t>(sets_[0].preset - 1)] == id) ||
                               (sets_[1].receiving && presets_[static_cast<std::size_t>(sets_[1].preset - 1)] == id));
    }

    // The R-123M sets and the commander's headset (roadmap 7.8). A set that is off, still warming up or turned right down
    // hears nothing (`why` says so in the log); a commander whose intercom box is not on the radio hears no set at all.
    // What the crew would have been told while it could not hear is lost, as when the set is changed.
    void SetSetReceiving(int set, bool receiving, const std::string& why = std::string()) {
        if (set < 0 || set > 1) return;
        sets_[set].receiving = receiving;
        sets_[set].why = why;
    }
    bool SetReceiving(int set) const { return set >= 0 && set <= 1 && sets_[set].receiving; }
    void SetCommanderOnRadio(bool on) { commander_on_radio_ = on; }
    bool CommanderOnRadio() const { return commander_on_radio_; }
    // Does the crew hear the net now?
    bool Hearing() const { return !off_net_ && commander_on_radio_ && sets_[listening_].receiving; }
    // Extra time to pass a designation on (no intercom), for the messages queued from now on.
    void SetPassOnExtraS(double seconds) {
        for (auto& s : sources_) s->board->SetPassOnExtra(seconds);
    }

    // Turn a set (0 left, 1 right) to a preset (1..4). Takes select_s. The set's source is heard only if it is the
    // one listened to. False for a bad set or preset, or a set already there.
    bool TuneSet(int set, int preset, double t, const std::string& by) {
        if (set < 0 || set > 1 || preset < 1 || preset > kPresets || op_.active) return false;
        if (sets_[set].preset == preset) return false;
        StartOp(set, preset, std::string(), false, t + policy_.select_s);
        events_.push_back({t, by, "R-123M " + std::string(set == 0 ? "left" : "right"),
                           "turns to preset " + std::to_string(preset)});
        return true;
    }
    // Take the headset to a set (0 left, 1 right). Takes select_s.
    bool ListenToSet(int set, double t, const std::string& by) {
        if (set < 0 || set > 1 || op_.active || (set == listening_ && !off_net_)) return false;
        StartOp(set, sets_[set].preset, std::string(), true, t + policy_.select_s);
        events_.push_back({t, by, "R-123M " + std::string(set == 0 ? "left" : "right"), "takes the headset to this set"});
        return true;
    }

    // The commander (or the scenario, or the player) changes source. A source on a preset takes select_s (the standby
    // set is turned to it, or he moves to the set already on it); one on no preset needs the standby set re-tuned onto
    // it (retune_s). The old source is heard until the change is made. Returns false for an unknown source or one he
    // already has. "none" turns the vehicle off the net.
    bool RequestSource(const std::string& id, double t, const std::string& by, const std::string& why = std::string()) {
        const std::string tail = why.empty() ? std::string() : " (" + why + ")";
        if (id == "none") {
            if (off_net_ && !op_.active) return false;
            off_net_ = true;
            op_ = Op();
            pending_.clear();
            active_.clear();
            events_.push_back({t, by, "none", "takes the vehicle off the net: its own radar alone" + tail});
            return true;
        }
        const Source* to = Find(id);
        if (to == nullptr) return false;
        const int k = PresetOf(id);
        if (k > 0) {
            int set = -1;
            for (int i = 0; i < 2; ++i) {
                if (sets_[i].preset == k && (set < 0 || i == 1 - listening_)) set = i;
            }
            if (set < 0) set = 1 - listening_;
            if (!op_.active && set == listening_ && !off_net_ && active_ == id) return false;
            if (op_.active && op_.listen && op_.set == set && op_.preset == k) return false;
            StartOp(set, k, std::string(), true, t + policy_.select_s);
        } else {
            const int set = 1 - listening_;
            if (op_.active && op_.listen && op_.assign == id) return false;
            StartOp(set, sets_[set].preset, id, true, t + policy_.retune_s);
        }
        pending_ = id;
        events_.push_back({t, by, id, "changes to " + to->def.short_name + tail});
        return true;
    }

    // The preset a source is on (1-4; 0: none).
    int PresetOfSource(const std::string& id) const { return PresetOf(id); }
    // A source's net moved to another frequency (its main station's order, radio_nets.hpp): it is heard now only on
    // `preset`, until the crew turns a set to it; what it says meanwhile on the old preset is not there to hear.
    bool MoveSource(const std::string& id, int preset, double t) {
        if (Find(id) == nullptr || preset < 1 || preset > kPresets) return false;
        if (presets_[static_cast<std::size_t>(preset - 1)] == id) return false;
        for (std::string& slot : presets_) {
            if (slot == id) slot.clear();
        }
        presets_[static_cast<std::size_t>(preset - 1)] = id;
        const std::string now = off_net_ ? std::string() : presets_[static_cast<std::size_t>(sets_[listening_].preset - 1)];
        if (now != active_) active_ = now;
        events_.push_back({t, "net", id, "now on preset " + std::to_string(preset)});
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
            Start(t);
        }
        for (auto& sp : sources_) Run(*sp, t, truth);
        if (op_.active && t + 1e-9 >= op_.done_s) Complete(t);
        if (!active_.empty() && !off_net_) {
            if (Hearing()) {
                Hear(*Find(active_), t);
            } else {
                Find(active_)->board->DiscardHeard(t);  // spoken to a crew that could not hear it
            }
        }
        NoteHearing(t);
        if (policy_.auto_fallback && !op_.active) Decide(t);
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
            v.monitored = Monitored(sp->def.id);
            v.last_contact_s = sp->last_contact_s;
            v.lost_messages = sp->board->LostCount();
            out.push_back(v);
        }
        return out;
    }

    const std::vector<Event>& PeekEvents() const { return events_; }
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

    // A source on a preset can only be heard by a set that is on and tuned to it; one on no preset is not heard at all but
    // its words still count as contact, as before (the commander only listens to what is on his two sets).
    bool Receivable(const Source& s) const {
        const int k = PresetOf(s.def.id);
        if (k == 0) return true;
        for (int i = 0; i < 2; ++i) {
            if (sets_[i].preset == k && sets_[i].receiving) return true;
        }
        return false;
    }

    // Say once when the crew stops or starts hearing the net, and why.
    void NoteHearing(double t) {
        const bool now = Hearing() || off_net_;  // off the net is the commander's own choice, not a fault
        if (now == was_hearing_) return;
        was_hearing_ = now;
        if (now) {
            events_.push_back({t, "Commander", "net", "hears the net again"});
            return;
        }
        const std::string why = !commander_on_radio_ ? std::string("his intercom box is not on the radio (A-1)")
                                : sets_[listening_].why.empty() ? std::string("the set he listens on hears nothing")
                                                                : sets_[listening_].why;
        events_.push_back({t, "Commander", "net", "cannot hear the net: " + why});
    }

    // Did the words it said reach the vehicle (it was up when they ended, in reach, and the link carried them)?
    bool Carried(Source& s, double ended_s, double t) {
        if (DownAt(s, ended_s) || !InReach(s)) return false;
        const double p = LinkP(s, t);
        return p >= 1.0 || Draw(s) < p;
    }

    // The regiment's radars hold a target: within some radar's range and above its horizon.
    static bool Covered(const std::vector<RegimentRadar>& feed, const BoardContact& c) {
        const double h = c.height_agl_m >= 0.0 ? c.height_agl_m : c.altitude_m;
        for (const RegimentRadar& r : feed) {
            const double d = (c.position - r.position).HorizontalLength();
            if (d <= r.max_range_m && d <= RadarHorizonM(r.antenna_height_m, h)) return true;
        }
        return false;
    }

    std::vector<BoardContact> Sees(const Source& s, const std::vector<BoardContact>& truth) const {
        if (s.def.sense == C2Sense::Relayed) {
            if (s.def.feed.empty()) return truth;
            std::vector<BoardContact> covered;
            for (const BoardContact& c : truth) {
                if (Covered(s.def.feed, c)) covered.push_back(c);
            }
            return covered;
        }
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
        if (s.avail && s.board->External() && Receivable(s)) s.last_contact_s = t;
        if (s.avail && s.board->Due(t)) {
            const std::size_t before = s.board->Marks().size();
            if (s.board->Update(t, Sees(s, truth))) {
                if (Carried(s, t, t) && Receivable(s)) s.last_contact_s = t;  // the routine call that goes with each plot
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
            if (r > 0 && Receivable(s)) s.last_contact_s = t > s.last_contact_s ? t : s.last_contact_s;
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
            if (m.group_centre) text += ", a group's centre: rule " + std::to_string(m.rule);
            if (s.def.id != policy_.primary) text += " [" + s.def.short_name + "]";
            events_.push_back({t, "Commander", m.target_id, text, m.number, d.azimuth_mils, d.range_m, m.height_hm, m.group_centre, m.rule});
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

    struct Op {
        bool active = false;
        int set = 0;
        int preset = 1;
        std::string assign;  // a source to write onto the preset first (a re-tune)
        bool listen = false; // and then listen to that set
        double done_s = 0.0;
    };

    int PresetOf(const std::string& id) const {
        for (int i = 0; i < kPresets; ++i) {
            if (presets_[static_cast<std::size_t>(i)] == id) return i + 1;
        }
        return 0;
    }

    void StartOp(int set, int preset, const std::string& assign, bool listen, double done_s) {
        op_.active = true;
        op_.set = set;
        op_.preset = preset;
        op_.assign = assign;
        op_.listen = listen;
        op_.done_s = done_s;
        const std::string& src = assign.empty() ? presets_[static_cast<std::size_t>(preset - 1)] : assign;
        pending_ = listen ? src : std::string();
    }

    void Start(double t) {
        // the frequency plan: the commander's choice first, then the rest
        std::vector<std::string> plan = policy_.presets;
        if (plan.empty()) {
            plan = Preference();
            for (const auto& sp : sources_) {
                if (std::find(plan.begin(), plan.end(), sp->def.id) == plan.end()) plan.push_back(sp->def.id);
            }
        }
        for (const std::string& id : plan) {
            if (Find(id) != nullptr && PresetOf(id) == 0) {
                for (std::string& slot : presets_) {
                    if (slot.empty()) {
                        slot = id;
                        break;
                    }
                }
            }
        }
        if (policy_.active.empty()) policy_.active = policy_.primary;
        if (Find(policy_.active) != nullptr && PresetOf(policy_.active) == 0) presets_[0] = policy_.active;
        const int k = Find(policy_.active) != nullptr ? PresetOf(policy_.active) : 0;
        listening_ = 0;
        sets_[0].preset = k > 0 ? k : 1;
        int other = 0;
        for (int i = 1; i <= kPresets && other == 0; ++i) {
            if (i != sets_[0].preset && !presets_[static_cast<std::size_t>(i - 1)].empty()) other = i;
        }
        sets_[1].preset = other > 0 ? other : sets_[0].preset;
        if (k > 0) {
            active_ = policy_.active;
            Find(active_)->board->DiscardHeard(t);
        }
    }

    void Complete(double t) {
        const Op op = op_;
        op_ = Op();
        pending_.clear();
        if (!op.assign.empty()) presets_[static_cast<std::size_t>(op.preset - 1)] = op.assign;
        sets_[op.set].preset = op.preset;
        if (op.listen) {
            listening_ = op.set;
            off_net_ = false;
        }
        const std::string now = off_net_ ? std::string() : presets_[static_cast<std::size_t>(sets_[listening_].preset - 1)];
        if (now != active_) {
            active_ = now;
            if (Source* a = Find(active_)) {
                a->board->DiscardHeard(t);  // what it said while the crew listened to the other is gone
                events_.push_back({t, "Commander", active_, "has " + a->def.short_name + " on the net"});
            }
        }
    }

    void Decide(double t) {
        if (off_net_) return;  // he took the vehicle off the net on purpose
        const Source* a = Find(active_);
        const std::vector<std::string> order = Preference();
        if (a == nullptr) return;
        if (a->state != C2State::Silent) {
            // On a standby: go back to the main source once it has been heard steadily.
            const Source* p = Find(policy_.primary);
            if (p != nullptr && a != p && Monitored(p->def.id) && p->state == C2State::Up) {
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
        primary_up_since_ = -1.0;
        // one he can hear on the other set
        for (const std::string& id : order) {
            const Source* c = Find(id);
            if (c == nullptr || id == active_ || !Monitored(id) || c->state == C2State::Silent) continue;
            RequestSource(id, t, "Commander", a->def.short_name + " silent");
            return;
        }
        // none on the sets: try the next on his list by turning the standby set to it
        if (t >= probe_after_s_) {
            for (const std::string& id : order) {
                if (id == active_ || Monitored(id)) continue;
                const int k = PresetOf(id);
                const int set = 1 - listening_;
                StartOp(set, k > 0 ? k : sets_[set].preset, k > 0 ? std::string() : id, false,
                        t + (k > 0 ? policy_.select_s : policy_.retune_s));
                probe_after_s_ = op_.done_s + 10.0;
                events_.push_back({t, "Commander", id, "listens for " + Find(id)->def.short_name + " on the other set"});
                return;
            }
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
    Op op_;
    struct Set {
        int preset = 1;
        bool receiving = true;
        std::string why;
    };
    Set sets_[2];
    int listening_ = 0;
    bool off_net_ = false;
    bool commander_on_radio_ = true;
    bool was_hearing_ = true;
    std::vector<std::string> presets_ = std::vector<std::string>(4);
    double probe_after_s_ = 0.0;
    double primary_up_since_ = -1.0;
    bool told_none_ = false;
    std::vector<Heard> heard_;
    std::vector<Event> events_;
    C2LinkModel* link_model_ = nullptr;
};

}  // namespace simcore
