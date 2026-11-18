// simcore/air_war/air_radio.hpp — the enemy's radio: every call a flight or a post makes is a transmission (roadmap B9,
// docs/air_battle/09_COMMS_AND_INTERCEPT.md).
//
// A flight's act by its leader (B1: flights.hpp) or a controller's call is said on a net: it has a speaker's platform in the air
// or on the ground, a band and a power from the EM library, a length by its kind and the speaker's stress, a turn on the net
// (the net is half duplex: a threat call breaks in after the call on the air, any other waits, and a call older than its use is
// dropped), and the words it says. The call goes into a session-level EM world (em/world.hpp) as a voice message with its call
// in `Message::payload`. A member of the flight acts on it only if he heard it: the link budget from the speaker to him decides,
// by the EM layer's own rule and draw. A flight flying silent (a NATO deep strike, a Pact strike group on its way in) says
// nothing a listener could hear; its leader passes the act by signal to those who see him, later, and the calls that break its
// silence (a threat call, the codeword after the attack) are said.
//
// The EM layer lets our intercept posts (receivers of kind CommsIntercept, em/comms_intercept.hpp) hear each call as it starts
// by their own link budget, through the ground and the horizon: this class collects what they heard (the time, the bearing and
// the length, and which call) for the information pipeline (command/intercept.hpp), where each post's linguists decide what it
// understood. Nothing here reads the truth of a call but the debrief, through the call log.
//
// A call's speaker, listeners and draws are keyed (Stream::AirRadio): the same seed gives the same log.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "simcore/aircraft.hpp"
#include "simcore/air_radio_def.hpp"
#include "simcore/em/comms_system.hpp"
#include "simcore/em/world.hpp"
#include "simcore/flights.hpp"
#include "simcore/geo.hpp"

namespace simcore {
namespace airwar {

// ------------------------------------------------------------------------------------------------ the calls

// What a call is (docs/air_battle/09 section 2.2).
enum class CallKind { CheckIn, ThreatCall, Report, Request, Divert, Vector, Codeword, Admin };

inline const char* CallKindId(CallKind k) {
    switch (k) {
        case CallKind::CheckIn: return "check_in";
        case CallKind::ThreatCall: return "threat_call";
        case CallKind::Report: return "report";
        case CallKind::Request: return "request";
        case CallKind::Divert: return "divert";
        case CallKind::Vector: return "vector";
        case CallKind::Codeword: return "codeword";
        case CallKind::Admin: return "admin";
    }
    return "?";
}
inline bool ParseCallKind(const std::string& s, CallKind& out) {
    for (CallKind k : {CallKind::CheckIn, CallKind::ThreatCall, CallKind::Report, CallKind::Request, CallKind::Divert, CallKind::Vector,
                       CallKind::Codeword, CallKind::Admin}) {
        if (s == CallKindId(k)) {
            out = k;
            return true;
        }
    }
    return false;
}
// What a listener who understands it learns of a time and a place from a call of this kind (the fast road takes these).
inline bool CallNamesTarget(CallKind k) {
    return k == CallKind::Divert || k == CallKind::Request || k == CallKind::CheckIn || k == CallKind::Report;
}

// A map square as it is said: two letters (the 10 km block east, north) and two digits each of the hundreds of metres in it,
// "KA 4417". A square repeats every 260 km, so it is read against where the listener has reason to think it is.
inline std::string GridOf(const Vec3& p) {
    const auto axis = [](double v, char& letter, int& digits) {
        const double blocks = std::floor(v / 10000.0);
        int b = static_cast<int>(std::fmod(blocks, 26.0));
        if (b < 0) b += 26;
        letter = static_cast<char>('A' + b);
        digits = std::clamp(static_cast<int>((v - blocks * 10000.0) / 100.0), 0, 99);
    };
    char le = 'A', ln = 'A';
    int de = 0, dn = 0;
    axis(p.x, le, de);
    axis(p.y, ln, dn);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%c%c %02d%02d", le, ln, de, dn);
    return buf;
}

// The first square in a text, read against `near` (the nearest of its repeats). False if there is none.
inline bool FindGrid(const std::string& text, const Vec3& near_to, Vec3& out) {
    for (std::size_t i = 0; i + 7 <= text.size(); ++i) {
        const char a = text[i], b = text[i + 1];
        if (a < 'A' || a > 'Z' || b < 'A' || b > 'Z' || text[i + 2] != ' ') continue;
        bool digits = true;
        for (std::size_t k = 3; k < 7; ++k) digits = digits && text[i + k] >= '0' && text[i + k] <= '9';
        if (!digits) continue;
        const auto num = [&](std::size_t at) { return (text[at] - '0') * 10 + (text[at + 1] - '0'); };
        const auto axis = [](char letter, int d, double ref) {
            const double base = (letter - 'A') * 10000.0 + d * 100.0 + 50.0;
            return base + 260000.0 * std::round((ref - base) / 260000.0);
        };
        out = Vec3{axis(a, num(i + 3), near_to.x), axis(b, num(i + 5), near_to.y), 0.0};
        return true;
    }
    return false;
}

// "1432" for a time of day in seconds.
inline std::string ClockOf(double s) {
    double d = std::fmod(s, 86400.0);
    if (d < 0.0) d += 86400.0;
    const int m = static_cast<int>(d / 60.0);
    char buf[32];  // room for any int in each field (GCC's -Wformat-truncation)
    std::snprintf(buf, sizeof(buf), "%02d%02d", m / 60, m % 60);
    return buf;
}

// One call in the log (the truth of it is for the debrief; a listener has only what its post wrote).
struct Call {
    int id = 0;
    double said_s = 0.0;      // when the speaker began to speak (asked for the net)
    double start_s = 0.0;     // on the air
    double end_s = 0.0;
    CallKind kind = CallKind::Admin;
    std::string speaker;      // the speaker's platform name (a flight's id and number, or a post's id)
    std::string flight;       // the flight it concerns ("" a post's call to nobody in particular)
    std::string unit;         // its squadron (call sign continuity)
    std::string callsign;     // as said, with the rotation in force
    std::string language;
    std::string words;        // what is said
    bool has_where = false;   // it names a place (as the speaker's words give it)
    Vec3 where;
    bool has_when = false;
    double when_s = 0.0;
    int net = 0;              // the EM net
    long long message = 0;    // the EM message
    int speaker_node = 0;
    bool have_quick = false;
    bool secure = false;
    bool dropped = false;     // never said: the net was too busy for too long
    bool by_signal = false;   // not said on the air
    bool ours = false;        // our side's call (our GCI's vector, our pilots: roadmap B10): our posts do not log it
    std::string truth;        // what really happened (debrief only)
};

// What one of our posts heard of a call, physically: when, from where, how loud. What it understood is the desk's.
struct Heard {
    int call = -1;
    std::string post;
    std::string chain_post;
    double start_s = 0.0;
    double end_s = 0.0;
    double bearing_rad = 0.0;  // true, with the receiver's error
    double rx_dbw = -300.0;
    double margin_db = 0.0;
    bool line_of_sight = false;
};

// ------------------------------------------------------------------------------------------------ discipline

// A flight's radio discipline: the faction's, moved by the people in it (the officer's quirks, the pilots' nerve) and by
// loose lips on his nets, held to 0..1 (docs/air_battle/09 section 3.2). `quirk_chatter` > 0 is a chatterbox, < 0 a silent
// runner; `loose_lips` the comsec_leak quirk's sum.
inline double RadioDiscipline(double faction, double adjust, double quirk_chatter, double loose_lips, const AirRadioDef& d) {
    const double v = faction + adjust - d.Tune("radio.discipline_chatter") * quirk_chatter -
                     d.Tune("radio.discipline_loose_lips") * (loose_lips > 0.0 ? 1.0 : 0.0);
    return std::clamp(v, 0.0, 1.0);
}

// The staff's operational security after its planner's habits (loose lips lowers it): the hours warning's input.
inline double StaffOpsec(double faction_opsec, double loose_lips, const AirRadioDef& d) {
    return std::clamp(faction_opsec - d.Tune("radio.opsec_loose_lips") * (loose_lips > 0.0 ? 1.0 : 0.0), 0.0, 1.0);
}

// Whether a committed package shows in the enemy's administrative traffic (tasking, tanker and airspace coordination), and the
// hours of warning it gives (docs/air_battle/09 section 5.3). The two ends are PURPLE DRAGON's: a careless staff alerted
// 34 % of its strikes by 8.5 h, a careful one 5 % with under 30 min (nsa_purple_dragon_1993 pp. 22-26); opsec 0..1 maps
// between them in a straight line (a game value). `key` identifies the package; the draw is keyed on it and the seed.
struct OpsecAlert {
    bool alerted = false;
    double lead_h = 0.0;     // how long before the raid the warning could be had
    bool wrong = false;      // the window it names is not the raid's
    double wrong_shift_h = 0.0;
};
inline OpsecAlert DrawOpsecAlert(double opsec, std::uint64_t seed, std::uint64_t key, const AirRadioDef& d) {
    OpsecAlert a;
    const double o = std::clamp(opsec, 0.0, 1.0);
    const double chance = d.Tune("radio.opsec_alert_at0") + (d.Tune("radio.opsec_alert_at1") - d.Tune("radio.opsec_alert_at0")) * o;
    const em::Random rng(seed ^ 0xB90B5ECull);
    if (rng.Uniform(em::Stream::AirRadio, key, 1) >= chance) return a;
    a.alerted = true;
    const double mean = d.Tune("radio.opsec_lead_at0_h") + (d.Tune("radio.opsec_lead_at1_h") - d.Tune("radio.opsec_lead_at0_h")) * o;
    const double spread = d.Tune("radio.opsec_lead_spread");
    a.lead_h = std::max(0.05, mean * (1.0 + spread * (2.0 * rng.Uniform(em::Stream::AirRadio, key, 2) - 1.0)));
    a.wrong = rng.Uniform(em::Stream::AirRadio, key, 3) < d.Tune("radio.opsec_wrong_share");
    if (a.wrong) a.wrong_shift_h = (rng.Uniform(em::Stream::AirRadio, key, 4) < 0.5 ? -1.0 : 1.0) * (1.0 + 2.0 * rng.Uniform(em::Stream::AirRadio, key, 5));
    return a;
}

// The call sign in force for a unit at hour h: the base with the rotation's number ("Hawk 21" -> "Hawk 21" then "Hawk 38").
// The rotation interval is the staff's (a careless planner doubles it).
inline std::string CallsignAt(const std::string& base, double hour, double rotation_h, std::uint64_t seed) {
    if (rotation_h <= 0.0) return base;
    const long long epoch = static_cast<long long>(std::floor(hour / rotation_h));
    if (epoch == 0) return base;
    const em::Random rng(seed ^ 0xCA11517Bull);
    const int n = 10 + static_cast<int>(rng.Uniform(em::Stream::AirRadio, std::hash<std::string>()(base), static_cast<std::uint64_t>(epoch)) * 89.0);
    char buf[16];
    std::snprintf(buf, sizeof(buf), " %d", n);
    return base + buf;
}

// ------------------------------------------------------------------------------------------------ the world of the calls

class AirRadio final : public FlightRadio {
public:
    // False, with why, if the library lacks a set or a receiver the definition names.
    bool Init(const AirRadioDef& def, const GeoPoint& origin, const Terrain* terrain, std::uint64_t seed, std::string* error = nullptr) {
        const auto fail = [error](const std::string& w) {
            if (error != nullptr) *error = w;
            return false;
        };
        def_ = def;
        seed_ = seed;
        rng_ = em::Random(seed ^ 0xA1B2C3D4ull);
        world_ = std::make_unique<em::EmWorld>(origin, terrain, seed ^ 0x9E3779B9ull);
        std::string e;
        if (!world_->SetLibrary(def_.library, &e)) return fail(e);
        const em::EmLibrary& lib = world_->Scene().lib;
        if (def_.air_radio.empty() || lib.Radio(def_.air_radio) < 0) return fail("air_radio: no radio '" + def_.air_radio + "' in the library");
        if (!def_.ground_radio.empty() && lib.Radio(def_.ground_radio) < 0) return fail("air_radio: no radio '" + def_.ground_radio + "' in the library");
        if (!def_.our_radio.empty() && lib.Radio(def_.our_radio) < 0) return fail("air_radio: no radio '" + def_.our_radio + "' in the library");
        // The calls are the only thing this world does: no sweeps, no links on the clock, no ageing.
        em::EmRates& r = world_->Rates();
        r.esm_period_s = r.knowledge_period_s = r.link_period_s = r.age_period_s = 1e9;
        for (const AirRadioPostDef& p : def_.posts) {
            if (!AddPost(p, &e)) return fail(e);
        }
        return true;
    }
    bool On() const { return world_ != nullptr; }
    const AirRadioDef& Def() const { return def_; }
    void SetFlights(const Flights* f) { flights_ = f; }
    void SetClockStart(double s) { clock_start_s_ = s; }

    // ---- the flights (called as each is registered in the session)

    // A flight's net and nodes, one per aircraft; `starts` are where its aircraft begin (the leader's first).
    // Our flights (roadmap B10) have a net too when the scenario gives our side's set (`our_radio`): our GCI's vectors reach
    // them by the same link budget, and can be lost the same way.
    void AddFlight(int fi, const FlightState& f, const FlightRadioInfo& info, const std::vector<Vec3>& starts) {
        if (!On() || f.weapons) return;
        const bool ours = f.side == "ours";
        if (!ours && f.side != "enemy") return;
        if (ours && def_.our_radio.empty()) return;
        const em::Side side = ours ? em::Side::Blue : em::Side::Red;
        FlightEntry e;
        e.ours = ours;
        e.fi = fi;
        e.id = f.id;
        e.callsign = f.callsign.empty() ? f.id : f.callsign;
        e.info = info;
        e.unit = info.unit.empty() ? f.id : info.unit;
        e.discipline = std::clamp(def_.discipline + info.discipline_adjust, 0.0, 1.0);
        const bool strike_like = info.role == "strike" || info.role == "dead" || info.role == "recce";
        e.comm_out = info.comm_out ||
                     (strike_like && rng_.Uniform(em::Stream::AirRadio, std::hash<std::string>()(f.id), 11) < e.discipline && def_.discipline >= 0.5);
        const int index = static_cast<int>(entries_.size());
        std::vector<int> nodes;
        for (std::size_t k = 0; k < f.slots.size(); ++k) {
            Member m;
            m.target = f.slots[k].target;
            m.number = f.slots[k].number;
            em::Platform p;
            p.name = f.id + " " + std::to_string(f.slots[k].number);
            p.side = side;
            p.kind = em::PlatformKind::FixedWing;
            p.position = k < starts.size() ? starts[k] : (starts.empty() ? Vec3() : starts.front());
            m.platform = world_->AddPlatform(p);
            m.node = world_->AddNode(p.name, m.platform, side);
            nodes.push_back(m.node);
            e.members.push_back(m);
        }
        const double freq = (ours ? def_.our_freq_hz : def_.freq_hz) + def_.channel_hz * static_cast<double>(index % 200);
        e.net = world_->AddNet("flight " + f.id, ours ? def_.our_radio : def_.air_radio, freq, nodes);
        e.unit_key = std::hash<std::string>()(e.unit);
        entries_.push_back(std::move(e));
    }

    // ---- per step: aircraft to platforms, the world on, what our posts heard

    void Step(double t, double dt, const Flights::Lookup& air) {
        if (!On()) return;
        em::EmScene& s = world_->Scene();
        for (FlightEntry& e : entries_) {
            for (Member& m : e.members) {
                if (m.gone) continue;
                const Aircraft* a = m.target >= 0 ? air(m.target) : nullptr;
                if (a == nullptr) {
                    if (m.seen) {
                        m.gone = true;
                        world_->DestroyPlatform(m.platform);
                    }
                    continue;
                }
                m.seen = true;
                world_->SetPlatformState(m.platform, a->position, a->velocity);
            }
            Watch(e, t, air);
        }
        s.time_s = t - dt;
        world_->Step(dt);
        s.events.clear();
        for (const em::CommsHeard& h : s.heard) Collect(h);
        s.heard.clear();
        Retire(t);
    }

    // ---- the Flights' hook

    int Say(int fi, const FlightState& f, const FlightOrder& o, double t) override {
        if (!On()) return kNoModel;
        if (o.by == "instructor" || o.by == "the session" || o.by.empty()) return kNoModel;
        FlightEntry* e = EntryOf(fi);
        if (e == nullptr) return kNoModel;
        Speak sp;
        sp.kind = KindOf(o);
        sp.stress = o.kind == OrderKind::Evade ? 1.0 : (f.phase == "attack" ? 0.5 : 0.0);
        const std::string truth = std::string("order ") + ToString(o.kind) + " by " + o.by;
        sp.truth = truth;
        sp.flight = e->id;
        if (o.has_aim) {
            sp.has_where = true;
            sp.where = o.aim;
        }
        if (o.kind == OrderKind::Divert && !o.legs.empty()) {
            sp.has_where = true;
            sp.where = o.legs.back().to;
        }
        const Member* lead = LeaderMember(*e, f);
        if (lead == nullptr) return kNoModel;
        sp.words = WordsFor(*e, o, sp);
        if (o.by == "leader" || o.by == "package") {
            sp.node = lead->node;
            sp.net = e->net;
            sp.speaker = FlightSpeakerName(*e, *lead);
            return Transmit(sp, *e, t);
        }
        // A controller's call (B5 gives these authority): the post of that id speaks, if the scenario has it.
        const Post* post = FindPost(o.by);
        if (post == nullptr || post->node == 0) return kNoModel;
        sp.node = post->node;
        sp.net = post->net;
        sp.speaker = post->def.id;
        if (o.has_heading) sp.kind = CallKind::Vector;
        if (sp.kind == CallKind::Vector && !GciChannelFree(*post, e->id, t)) return kDropped;
        return Transmit(sp, *e, t);
    }

    double Reaches(int call, int fi, int slot, double t, double react_s) override {
        if (!On()) return -1.0;
        FlightEntry* e = EntryOf(fi);
        if (e == nullptr || slot < 0 || static_cast<std::size_t>(slot) >= e->members.size()) return -1.0;
        const Member& listener = e->members[static_cast<std::size_t>(slot)];
        em::EmScene& s = world_->Scene();
        if (call == kDropped) return -1.0;
        if (call == kSignalled) {  // by signal: he must see the leader
            const Member* lead = nullptr;
            for (const Member& m : e->members) {
                if (m.number == 1 || lead == nullptr) lead = &m;
            }
            if (lead == nullptr || lead == &listener) return -1.0;
            const em::Platform* a = s.FindPlatform(lead->platform);
            const em::Platform* b = s.FindPlatform(listener.platform);
            if (a == nullptr || b == nullptr || !a->alive || !b->alive) return -1.0;
            if ((a->position - b->position).Length() > def_.Tune("radio.signal_range_m")) return -1.0;
            return t + react_s * def_.Tune("radio.signal_factor");
        }
        if (call < 0 || static_cast<std::size_t>(call) >= log_.size()) return -1.0;
        const Call& c = log_[static_cast<std::size_t>(call)];
        if (c.dropped) return -1.0;
        const em::NodeState* from = s.FindNode(c.speaker_node);
        const em::NodeState* to = s.FindNode(listener.node);
        const em::NetState* net = s.FindNet(c.net);
        if (from == nullptr || to == nullptr || net == nullptr || !from->alive || !to->alive) return -1.0;
        const em::LinkState l = em::EvaluateLink(s, *net, *from, *to);
        const double p = l.up ? 1.0 / (1.0 + std::exp(-l.margin_db / 2.0)) : 0.0;
        if (s.rng.Uniform(em::Stream::CommsDelivery, em::Key(c.message), em::Key(listener.node)) >= p) return -1.0;
        return std::max(t, c.end_s) + react_s;
    }

    // ---- a call from outside the flights' orders (check-ins, reports, a controller's vectors, B5's messages)

    struct Speak {
        CallKind kind = CallKind::Admin;
        std::string speaker;   // set by the callers below
        std::string flight;
        std::string words;
        int node = 0;
        int net = 0;
        bool has_where = false;
        Vec3 where;
        bool has_when = false;
        double when_s = 0.0;
        double stress = 0.0;
        bool ours = false;
        std::string truth;
    };
    // A flight's leader (or the aircraft numbered `number`) speaks on his flight's net. Returns the call's number, or one of
    // the FlightRadio codes (kSignalled: a flight flying silent keeps routine calls off the air).
    int SayAs(const std::string& flight, CallKind kind, const std::string& words, double t, bool has_where = false,
              const Vec3& where = Vec3(), bool has_when = false, double when_s = 0.0, int number = 1) {
        if (!On()) return kNoModel;
        FlightEntry* e = nullptr;
        for (FlightEntry& x : entries_) {
            if (x.id == flight) e = &x;
        }
        if (e == nullptr) return kNoModel;
        const Member* who = nullptr;
        for (const Member& m : e->members) {
            if (m.gone) continue;
            if (m.number == number || who == nullptr) who = &m;
        }
        if (who == nullptr) return kNoModel;
        Speak sp;
        sp.kind = kind;
        sp.flight = e->id;
        sp.words = words;
        sp.node = who->node;
        sp.net = e->net;
        sp.speaker = FlightSpeakerName(*e, *who);
        sp.has_where = has_where;
        sp.where = where;
        sp.has_when = has_when;
        sp.when_s = when_s;
        sp.stress = kind == CallKind::ThreatCall ? 1.0 : 0.0;
        return Transmit(sp, *e, t);
    }
    // A post speaks (a GCI's vector to a flight, a FAC's confirmation). A GCI keeps to its channels.
    int SayFrom(const std::string& post_id, CallKind kind, const std::string& to_flight, const std::string& words, double t,
                bool has_where = false, const Vec3& where = Vec3()) {
        if (!On()) return kNoModel;
        const Post* post = FindPost(post_id);
        if (post == nullptr || post->node == 0) return kNoModel;
        FlightEntry* e = nullptr;
        for (FlightEntry& x : entries_) {
            if (x.id == to_flight) e = &x;
        }
        if (kind == CallKind::Vector && !GciChannelFree(*post, to_flight, t)) return kDropped;
        Speak sp;
        sp.kind = kind;
        sp.flight = to_flight;
        sp.words = words;
        sp.node = post->node;
        sp.net = post->net;
        sp.speaker = post->def.id;
        sp.has_where = has_where;
        sp.where = where;
        sp.ours = post->def.kind == "our_gci";
        FlightEntry dummy;
        return Transmit(sp, e != nullptr ? *e : dummy, t);
    }

    // ---- the campaign's posts in a session (roadmap S2)

    // A post of the enemy's command the session did not have (the campaign's posts, when a raid enters a battery's
    // session): it speaks from then on. False, with why, if the library lacks its set; true and nothing done if it is here.
    bool AddSpeakerPost(const AirRadioPostDef& p, std::string* error = nullptr) {
        if (!On()) return false;
        if (FindPost(p.id) != nullptr) return true;
        return AddPost(p, error);
    }
    bool HasPost(const std::string& id) const { return FindPost(id) != nullptr; }
    // The frequency of a post's net (0: no such speaking post).
    double PostFreqHz(const std::string& id) const {
        const Post* p = FindPost(id);
        if (p == nullptr || !On()) return 0.0;
        const em::NetState* n = world_->Scene().FindNet(p->net);
        return n != nullptr ? n->freq_hz : 0.0;
    }
    // One of our spot jammers on a post's net (our side's jamming of the enemy's command, data/command/counter_c2.json):
    // `jammer` from the library, standing at `at` (session ENU) on a mast, tuned to the post's net and turned toward the
    // post. Every call on that net is judged against it by the link budget (em::EvaluateLink's J/N). The jammer's id, or 0.
    int JamPostNet(const std::string& post_id, const std::string& jammer, const Vec3& at, double mast_m = 10.0) {
        if (!On()) return 0;
        const Post* p = FindPost(post_id);
        if (p == nullptr || p->node == 0) return 0;
        em::EmScene& s = world_->Scene();
        const em::NetState* net = s.FindNet(p->net);
        if (net == nullptr || s.lib.Jammer(jammer) < 0) return 0;
        const int platform = world_->AddGroundPlatform("our jammer on " + post_id, em::Side::Blue, at.x, at.y, mast_m);
        const int id = world_->AddJammer(platform, jammer, true);
        for (const em::JammerState& j : s.jammers) {
            if (j.id != id) continue;
            if (em::EmitterState* e = s.FindEmitter(j.emitter_id)) {
                e->freq_hz = net->freq_hz;
                e->radiating = true;
                const em::Platform* pp = s.FindPlatform(p->platform);
                if (pp != nullptr) e->pointing_az_rad = em::AzimuthFromTo(Vec3{at.x, at.y, 0.0}, Vec3{pp->position.x, pp->position.y, 0.0});
            }
        }
        return id;
    }

    // ---- what the world holds

    const std::vector<Call>& Calls() const { return log_; }
    const std::vector<Heard>& HeardLog() const { return heard_; }
    const em::EmWorld& World() const { return *world_; }
    em::EmWorld& World() { return *world_; }
    bool CommOut(const std::string& flight) const {
        for (const FlightEntry& e : entries_) {
            if (e.id == flight) return e.comm_out;
        }
        return false;
    }
    std::size_t FlightCount() const { return entries_.size(); }
    int GciChannelsInUse(const std::string& post_id, double t) const {
        const Post* p = FindPost(post_id);
        if (p == nullptr) return 0;
        int n = 0;
        for (const auto& kv : p->streams) n += t - kv.second <= def_.Tune("radio.vector_hold_s") ? 1 : 0;
        return n;
    }
    // The same call's net and members, for tests.
    int NetOf(const std::string& flight) const {
        for (const FlightEntry& e : entries_) {
            if (e.id == flight) return e.net;
        }
        return 0;
    }
    int NodeOf(const std::string& flight, int number) const {
        for (const FlightEntry& e : entries_) {
            if (e.id != flight) continue;
            for (const Member& m : e.members) {
                if (m.number == number) return m.node;
            }
        }
        return 0;
    }
    int PlatformOf(const std::string& flight, int number) const {
        for (const FlightEntry& e : entries_) {
            if (e.id != flight) continue;
            for (const Member& m : e.members) {
                if (m.number == number) return m.platform;
            }
        }
        return 0;
    }
    // Where a call's time went: the length a call of this kind and stress takes.
    double LengthOf(CallKind k, double stress) const {
        return def_.Tune(std::string("radio.call_") + CallKindId(k) + "_s") * (1.0 + def_.Tune("radio.stress_repeat") * std::clamp(stress, 0.0, 1.0));
    }
    // The call sign in force at session time t for a base (the rotation interval is the faction's).
    std::string CallsignFor(const std::string& base, double t_s) const {
        return CallsignAt(base, (clock_start_s_ + t_s) / 3600.0, def_.callsign_rotation_h, seed_);
    }

private:
    struct Member {
        int target = -1;
        int number = 1;
        int platform = 0;
        int node = 0;
        bool seen = false;
        bool gone = false;
    };
    struct FlightEntry {
        int fi = -1;
        std::string id;
        std::string callsign;
        std::string unit;
        std::size_t unit_key = 0;
        FlightRadioInfo info;
        double discipline = 0.5;
        bool comm_out = false;
        int net = 0;
        std::vector<Member> members;
        std::string phase;
        bool checked_in = false;
        bool codeword_said = false;
        bool attacked = false;
        bool ours = false;
    };
    struct Post {
        AirRadioPostDef def;
        int platform = 0;
        int node = 0;
        int net = 0;
        int esm = 0;
        std::map<std::string, double> streams;  // flight -> the time of its last vector (a GCI's channels)
    };

    bool AddPost(const AirRadioPostDef& p, std::string* error) {
        em::EmScene& s = world_->Scene();
        Post po;
        po.def = p;
        const bool listens = p.kind == "intercept";
        const bool ours = p.kind == "our_gci";  // our GCI post speaking to our flights (roadmap B10)
        const em::Side side = listens || ours ? em::Side::Blue : em::Side::Red;
        po.platform = world_->AddGroundPlatform(p.name.empty() ? p.id : p.name, side, p.at.x, p.at.y, p.mast_m);
        if (listens) {
            po.esm = world_->AddEsm(po.platform, p.esm);
            if (po.esm == 0) {
                if (error != nullptr) *error = "air_radio post " + p.id + ": no receiver '" + p.esm + "' in the library";
                return false;
            }
        } else {
            po.node = world_->AddNode(p.name.empty() ? p.id : p.name, po.platform, side);
            const std::string radio = p.radio.empty() ? (ours ? def_.our_radio : def_.ground_radio) : p.radio;
            if (radio.empty() || s.lib.Radio(radio) < 0) {
                if (error != nullptr) *error = "air_radio post " + p.id + ": no radio '" + radio + "' in the library";
                return false;
            }
            po.net = world_->AddNet("post " + p.id, radio, p.freq_hz > 0.0 ? p.freq_hz : (ours ? def_.our_freq_hz : def_.freq_hz), {po.node});
        }
        posts_.push_back(std::move(po));
        return true;
    }
    const Post* FindPost(const std::string& id) const {
        for (const Post& p : posts_) {
            if (p.def.id == id) return &p;
        }
        return nullptr;
    }
    Post* FindPostMutable(const std::string& id) {
        for (Post& p : posts_) {
            if (p.def.id == id) return &p;
        }
        return nullptr;
    }
    FlightEntry* EntryOf(int fi) {
        for (FlightEntry& e : entries_) {
            if (e.fi == fi) return &e;
        }
        return nullptr;
    }
    const Member* LeaderMember(const FlightEntry& e, const FlightState& f) const {
        const FlightSlot* ls = f.Leader();
        if (ls == nullptr) return nullptr;
        for (const Member& m : e.members) {
            if (m.target == ls->target && !m.gone) return &m;
        }
        return nullptr;
    }
    std::string FlightSpeakerName(const FlightEntry& e, const Member& m) const { return e.id + " " + std::to_string(m.number); }

    // A call's kind from the order it carries.
    static CallKind KindOf(const FlightOrder& o) {
        switch (o.kind) {
            case OrderKind::Evade: return CallKind::ThreatCall;
            case OrderKind::Divert:
            case OrderKind::Attack:
            case OrderKind::ReturnHome: return CallKind::Divert;
            case OrderKind::Hold:
            case OrderKind::Orbit:
            case OrderKind::Rejoin:
            case OrderKind::FollowRoute: return o.has_heading ? CallKind::Vector : CallKind::Admin;
        }
        return CallKind::Admin;
    }
    std::string WordsFor(const FlightEntry& e, const FlightOrder& o, const Speak& sp) const {
        const std::string cs = e.callsign;
        switch (o.kind) {
            case OrderKind::Evade: return cs + ", break" + (sp.has_where ? " " + GridOf(sp.where) : std::string());
            case OrderKind::Divert: return cs + ", new route" + (sp.has_where ? ", target " + GridOf(sp.where) : std::string());
            case OrderKind::Attack: return cs + ", attack " + (sp.has_where ? GridOf(sp.where) : std::string("target"));
            case OrderKind::ReturnHome: return cs + ", return to base";
            case OrderKind::Hold: return cs + ", hold" + (sp.has_where ? " at " + GridOf(sp.where) : std::string());
            case OrderKind::Orbit: return cs + ", orbit" + (sp.has_where ? " at " + GridOf(sp.where) : std::string());
            case OrderKind::Rejoin: return cs + ", rejoin";
            case OrderKind::FollowRoute: return cs + ", resume";
        }
        return cs;
    }

    // Is a GCI channel free for this group? A group keeps its channel while its vectors continue (section 2.6).
    bool GciChannelFree(const Post& post, const std::string& flight, double t) {
        Post* p = FindPostMutable(post.def.id);
        if (p == nullptr) return true;
        const double hold = def_.Tune("radio.vector_hold_s");
        int used = 0;
        for (const auto& kv : p->streams) {
            if (kv.first != flight && t - kv.second <= hold) ++used;
        }
        if (used >= std::max(1, p->def.channels)) return false;
        p->streams[flight] = t;
        return true;
    }

    // Put a call on its net: its turn, its length, its words in the log and the voice message in the EM world.
    int Transmit(Speak sp, const FlightEntry& e, double t) {
        em::EmScene& s = world_->Scene();
        em::NetState* net = s.FindNet(sp.net);
        const em::NodeState* from = s.FindNode(sp.node);
        if (net == nullptr || from == nullptr || !from->alive) return kNoModel;
        const double hash = static_cast<double>(std::hash<std::string>()(sp.speaker) % 1000003);
        const std::uint64_t key = static_cast<std::uint64_t>(hash);
        const bool threat = sp.kind == CallKind::ThreatCall;
        // A flight flying silent keeps its routine calls off the air; what breaks its silence is said.
        const bool routine = sp.kind == CallKind::CheckIn || sp.kind == CallKind::Report || sp.kind == CallKind::Admin || sp.kind == CallKind::Request;
        const bool speaker_is_flight = !e.id.empty() && sp.net == e.net;
        if (speaker_is_flight && routine && Silent(e, t, key)) return kSignalled;
        Call c;
        c.id = static_cast<int>(log_.size());
        c.said_s = t;
        c.kind = sp.kind;
        c.speaker = sp.speaker;
        c.flight = sp.flight;
        c.unit = e.unit.empty() ? sp.speaker : e.unit;
        c.callsign = CallsignFor(e.callsign.empty() ? sp.speaker : e.callsign, t);
        c.ours = sp.ours || e.ours;
        c.language = c.ours ? std::string("pannonian") : def_.language;
        c.words = sp.words;
        if (sp.has_when && c.words.find(" time ") == std::string::npos) c.words += ", time " + ClockOf(clock_start_s_ + sp.when_s);
        if (!e.callsign.empty() && c.callsign != e.callsign) {  // the rotation changed the sign: say it as in force
            const std::size_t at = c.words.find(e.callsign);
            if (at != std::string::npos) c.words.replace(at, e.callsign.size(), c.callsign);
        }
        c.has_where = sp.has_where;
        c.where = sp.where;
        c.has_when = sp.has_when;
        c.when_s = sp.when_s;
        c.net = sp.net;
        c.speaker_node = sp.node;
        c.have_quick = e.info.have_quick;
        c.secure = e.info.secure_voice;
        c.truth = sp.truth + (sp.flight.empty() ? std::string() : " (" + sp.flight + ")");
        // Its turn on the net: a call waits for the one on the air and those queued before it; a threat call goes in after the
        // one on the air only; one older than its use is dropped.
        const double len = LengthOf(sp.kind, sp.stress);
        double on_air_end = t;
        for (const em::Message& m : s.messages) {
            if (m.net == sp.net && !m.done && m.start_s <= t && m.end_s > on_air_end) on_air_end = m.end_s;
        }
        const double start = threat ? on_air_end : std::max(t, net->busy_until_s);
        double stale = def_.Tune("radio.stale_other_s");
        if (threat) stale = def_.Tune("radio.stale_threat_call_s");
        else if (sp.kind == CallKind::Vector) stale = def_.Tune("radio.stale_vector_s");
        if (start - t > stale) {
            c.dropped = true;
            c.start_s = c.end_s = start;
            log_.push_back(c);
            return kDropped;
        }
        c.start_s = start;
        c.end_s = start + len;
        c.message = em::SendVoice(s, *net, *from, 0, c.start_s, c.end_s, c.id);
        if (threat && c.end_s > net->busy_until_s) net->busy_until_s = c.end_s;
        log_.push_back(c);
        return c.id;
    }

    // Is this flight flying silent now? Comm-out until its attack (a NATO deep strike; a Pact strike group on its way in), and
    // afterwards a strike's routine calls are kept off the air with the faction's discipline.
    bool Silent(const FlightEntry& e, double t, std::uint64_t key) const {
        (void)t;
        if (e.comm_out && !e.attacked) return true;
        if (e.info.role == "strike" || e.info.role == "dead" || e.info.role == "recce") {
            return rng_.Uniform(em::Stream::AirRadio, key, static_cast<std::uint64_t>(log_.size()), 21) < e.discipline;
        }
        return false;
    }

    // The flights' own habits: the check-in at the front, and the codeword after the attack of a flight that flew silent.
    void Watch(FlightEntry& e, double t, const Flights::Lookup& air) {
        (void)air;
        if (flights_ == nullptr) return;
        const FlightState* f = flights_->Find(e.id);
        if (f == nullptr) return;
        const std::string phase = f->phase;
        if (phase != e.phase) {
            const std::string was = e.phase;
            e.phase = phase;
            if (phase == "attack") e.attacked = true;
            if (was == "attack" && (phase == "egress" || phase == "transit") && e.comm_out && !e.codeword_said && !e.info.codeword.empty()) {
                e.codeword_said = true;
                SayAs(e.id, CallKind::Codeword, e.info.codeword, t);
            }
            if (phase == "ingress" && !e.checked_in) {
                e.checked_in = true;
                const FlightSlot* ls = f->Leader();
                (void)ls;
                SayAs(e.id, CallKind::CheckIn, e.callsign + ", " + std::to_string(f->Alive()) + " aircraft, checking in", t);
            }
        }
    }

    void Collect(const em::CommsHeard& h) {
        if (h.payload < 0 || static_cast<std::size_t>(h.payload) >= log_.size()) return;
        if (log_[static_cast<std::size_t>(h.payload)].ours) return;  // our own GCI and pilots: not the intercept's business
        for (const Post& p : posts_) {
            if (p.esm != h.esm) continue;
            Heard r;
            r.call = h.payload;
            r.post = p.def.id;
            r.chain_post = p.def.chain_post;
            r.start_s = h.start_s;
            r.end_s = h.end_s;
            r.bearing_rad = h.bearing_rad;
            r.rx_dbw = h.rx_dbw;
            r.margin_db = h.margin_db;
            r.line_of_sight = h.line_of_sight;
            heard_.push_back(r);
        }
    }

    // A flight whose aircraft are all gone, and whose calls are all done, leaves the world: the target lists do not grow
    // without end (map/scale.md 1.4).
    void Retire(double t) {
        em::EmScene& s = world_->Scene();
        for (std::size_t i = 0; i < entries_.size();) {
            const FlightEntry& e = entries_[i];
            bool all_gone = !e.members.empty();
            for (const Member& m : e.members) all_gone = all_gone && m.gone;
            const em::NetState* net = s.FindNet(e.net);
            if (!all_gone || (net != nullptr && net->busy_until_s > t) || t - last_call_s(e) < 30.0) {
                ++i;
                continue;
            }
            for (const Member& m : e.members) {
                s.nodes.erase(std::remove_if(s.nodes.begin(), s.nodes.end(), [&](const em::NodeState& n) { return n.id == m.node; }), s.nodes.end());
                s.emitters.erase(std::remove_if(s.emitters.begin(), s.emitters.end(), [&](const em::EmitterState& x) { return x.platform_id == m.platform; }),
                                 s.emitters.end());
                s.platforms.erase(std::remove_if(s.platforms.begin(), s.platforms.end(), [&](const em::Platform& p) { return p.id == m.platform; }),
                                  s.platforms.end());
            }
            s.nets.erase(std::remove_if(s.nets.begin(), s.nets.end(), [&](const em::NetState& n) { return n.id == e.net; }), s.nets.end());
            retired_.push_back(e.id);
            entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
    double last_call_s(const FlightEntry& e) const {
        double last = -1e18;
        for (auto it = log_.rbegin(); it != log_.rend(); ++it) {
            if (it->net == e.net) {
                last = it->end_s;
                break;
            }
        }
        return last;
    }

    AirRadioDef def_;
    std::uint64_t seed_ = 1;
    em::Random rng_{1};
    std::unique_ptr<em::EmWorld> world_;
    const Flights* flights_ = nullptr;
    double clock_start_s_ = 0.0;
    std::vector<FlightEntry> entries_;
    std::vector<Post> posts_;
    std::vector<Call> log_;
    std::vector<Heard> heard_;
    std::vector<std::string> retired_;
};

}  // namespace airwar
}  // namespace simcore
