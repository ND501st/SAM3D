// simcore/command/info_net.hpp — the information pipeline: how a fact travels from the people who first know it to the
// people who need it, by the means each pair of them has (docs/command/04_PIPELINE.md).
//
// A fact (a raid warning, the day's intelligence summary, a prisoner's story, an order, a bridge down) starts with its
// origin (the national sector's duty officer, the division's intelligence officer, the garrison commandant) and the
// witnesses who saw it themselves. It is meant for a tree of the chain (its audience) down to a level (who needs it).
// It moves hop by hop along the chain (superiors and the people each deals with), and each hop takes the best means
// that pair has at that moment:
//   * the same place at the same time: in person;
//   * urgent ('Воздух', 'Гранит'): the fastest of radio, telephone, the early-warning net, a siren, a runner;
//   * priority: the wire if it is whole (covert: utin_2018), else the radio, else a despatch rider;
//   * routine: the next contact (a briefing, a visit, a courier run), or the priority way if none comes in time.
// Voice carries a summary; a paper (the full intelligence summary, an order with its overlay) goes by hand, and follows
// a voice message by the next contact. Carriers on the road can be lost where bombs fall, the telephone is out while a
// post's wire is cut, the radio while jammed, and every radio call is something the enemy's intercept service hears.
// The player's character does not pass anything on by itself: what the player doesn't tell the people below, they
// don't know (docs/command/01_PRINCIPLES.md).
//
// The net is a pure function of its time-stamped inputs (facts, dangers, cut wire, jamming, the battery post's moves):
// each fact is simulated on its own, and every decision at a time t uses only the inputs known by t, so advancing in
// steps or in one go gives the same deliveries (a saved campaign rebuilds its net exactly). Randomness is drawn from a
// hash of the fact, the hop and the attempt.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/command/data.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace command {

enum class FactKind {
    War,
    AirWarning,
    Order,
    Intsum,
    RaidSeen,
    RaidResult,
    OwnDamage,
    Prisoner,
    EnemyOfficer,
    EnemyKnowsBattery,
    BatteryNews,
    Press,
    Rumour,
    Mail
};

inline const char* FactKindName(FactKind k) {
    switch (k) {
        case FactKind::War: return "war";
        case FactKind::AirWarning: return "air_warning";
        case FactKind::Order: return "order";
        case FactKind::Intsum: return "intsum";
        case FactKind::RaidSeen: return "raid_seen";
        case FactKind::RaidResult: return "raid_result";
        case FactKind::OwnDamage: return "own_damage";
        case FactKind::Prisoner: return "prisoner";
        case FactKind::EnemyOfficer: return "enemy_officer";
        case FactKind::EnemyKnowsBattery: return "enemy_knows_battery";
        case FactKind::BatteryNews: return "battery_news";
        case FactKind::Press: return "press";
        case FactKind::Rumour: return "rumour";
        case FactKind::Mail: return "mail";
    }
    return "?";
}

struct Fact {
    std::string key;                     // unique and stable ("journal:12", "raid:3", "wreck:kesmark_road_bridge:31.5")
    FactKind kind = FactKind::Intsum;
    double h = 0.0;                      // when the origin knew it (campaign hours)
    std::string origin;                  // the position that first knew it
    std::vector<std::string> witnesses;  // others who saw it themselves at h
    std::string text;                    // in full
    std::string summary;                 // as told by voice ("" the same as the text)
    std::string subject;                 // what it is about (a raid's index, an object, an officer)
    std::string audience;                // the tree it is for ("" everyone)
    int need_level = 3;                  // down to this level
    int precedence = 0;                  // 0 routine, 1 priority, 2 air raid
    bool document = false;               // a paper: its full text goes by hand (or teleprinter)
    bool broadcast = false;              // the state radio's next bulletin, to everyone it is for
    double expires_h = 1e18;             // nobody passes it on after this

    const std::string& Told() const { return summary.empty() ? text : summary; }
};

// One person learning one fact.
struct Delivery {
    int fact = -1;
    int position = -1;
    double h = 0.0;          // when they learned it
    int from = -1;           // who told them (-1 they saw it themselves; -2 the state radio)
    int mean = -1;           // the means (-1 none: they saw it)
    std::string place;       // where they were
    bool full = false;       // they have the full text (saw it, or got the paper)
    double full_h = -1.0;    // when the paper reached them, if later
};

struct Danger {
    Vec3 center;
    double radius_m = 0.0;
    double t0_h = 0.0;
    double t1_h = 0.0;
    double chance = 0.0;   // a carrier on the road through it then is lost
    double known_h = 0.0;  // from when the pipeline counts it
};

struct WireCut {
    std::string place;
    double from_h = 0.0;
    double until_h = 0.0;
};

struct RadioJam {
    double from_h = 0.0;
    double until_h = 0.0;
};

struct PlaceMove {
    std::string place;
    double h = 0.0;
    Vec3 pos;
};

struct RadioCall {
    double h = 0.0;
    int position = -1;  // who called
    int fact = -1;
};

// "D2 19:20" for campaign hour h, the local clock starting at start_hour.
inline std::string FormatClock(double h, double start_hour) {
    const double local = start_hour + h;
    const int day = static_cast<int>(std::floor(local / 24.0)) + 1;
    double in_day = local - 24.0 * std::floor(local / 24.0);
    int hh = static_cast<int>(in_day);
    int mm = static_cast<int>(std::lround((in_day - hh) * 60.0));
    if (mm >= 60) {
        mm -= 60;
        hh += 1;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "D%d %02d:%02d", day, hh % 24, mm);
    return buf;
}

class InfoNet {
public:
    bool Init(const ChainData& data, unsigned seed, double start_hour, std::string* error = nullptr) {
        data_ = data;
        seed_ = seed;
        start_hour_ = start_hour;
        facts_.clear();
        states_.clear();
        dangers_.clear();
        cuts_.clear();
        jams_.clear();
        moves_.clear();
        now_h_ = -1e18;
        const std::size_t n = data_.positions.size();
        home_.assign(n, -1);
        for (std::size_t i = 0; i < n; ++i) home_[i] = data_.PlaceIndex(data_.positions[i].place);
        // The chain's graph: superiors (weight 1) and the people each deals with (links).
        edges_.assign(n, {});
        for (std::size_t i = 0; i < n; ++i) {
            const Position& p = data_.positions[i];
            if (!p.superior.empty()) Connect(static_cast<int>(i), data_.PositionIndex(p.superior), false);
            for (const std::string& l : p.links) Connect(static_cast<int>(i), data_.PositionIndex(l), true);
        }
        player_ = data_.PositionIndex(data_.player_default);
        in_person_ = data_.MeanIndex("in_person");
        if (in_person_ < 0) {
            if (error != nullptr) *error = "the means need 'in_person'";
            return false;
        }
        return true;
    }

    // ---------------------------------------------------------------- inputs

    // A fact; one whose key was added before is not added again. Returns its index.
    int AddFact(const Fact& f) {
        const auto it = by_key_.find(f.key);
        if (it != by_key_.end()) return it->second;
        facts_.push_back(f);
        states_.emplace_back();
        const int i = static_cast<int>(facts_.size()) - 1;
        by_key_[f.key] = i;
        return i;
    }
    void AddDanger(const Danger& d) { dangers_.push_back(d); }
    void CutWire(const std::string& place, double from_h, double until_h) { cuts_.push_back({place, from_h, until_h}); }
    void JamRadio(double from_h, double until_h) { jams_.push_back({from_h, until_h}); }
    void MovePlace(const std::string& place, double h, const Vec3& pos) { moves_.push_back({place, h, pos}); }
    // The post the player's character holds: it passes nothing on by itself.
    void SetPlayer(const std::string& position) {
        player_ = data_.PositionIndex(position);
        for (FactState& s : states_) s = FactState();  // who relays changed: recompute everything
    }

    // ---------------------------------------------------------------- running

    // Everyone's knowledge up to campaign hour h. Call it only once every input known by h has been added.
    void AdvanceTo(double h) {
        for (std::size_t i = 0; i < facts_.size(); ++i) Run(static_cast<int>(i), states_[i], h);
        now_h_ = std::max(now_h_, h);
    }

    // ---------------------------------------------------------------- what people know

    double Now() const { return now_h_; }
    double StartHour() const { return start_hour_; }
    const ChainData& Data() const { return data_; }
    const std::vector<Fact>& Facts() const { return facts_; }
    int FactIndex(const std::string& key) const {
        const auto it = by_key_.find(key);
        return it == by_key_.end() ? -1 : it->second;
    }
    int Player() const { return player_; }

    // What a person has learned by now, in the order they learned it.
    std::vector<Delivery> KnownBy(int position) const {
        std::vector<Delivery> out;
        for (const FactState& s : states_) {
            if (position < 0 || static_cast<std::size_t>(position) >= s.delivered.size()) continue;
            const int di = s.delivered[static_cast<std::size_t>(position)];
            if (di < 0) continue;
            const Delivery& d = s.deliveries[static_cast<std::size_t>(di)];
            if (d.h <= now_h_ + 1e-9) out.push_back(d);
        }
        std::sort(out.begin(), out.end(), [](const Delivery& a, const Delivery& b) { return a.h < b.h || (a.h == b.h && a.fact < b.fact); });
        return out;
    }
    std::vector<Delivery> KnownBy(const std::string& position) const { return KnownBy(data_.PositionIndex(position)); }

    // How (and whether) a person knows a fact by now; nullptr if not.
    const Delivery* Known(int position, int fact) const {
        if (fact < 0 || static_cast<std::size_t>(fact) >= states_.size() || position < 0) return nullptr;
        const FactState& s = states_[static_cast<std::size_t>(fact)];
        if (static_cast<std::size_t>(position) >= s.delivered.size()) return nullptr;
        const int di = s.delivered[static_cast<std::size_t>(position)];
        if (di < 0 || s.deliveries[static_cast<std::size_t>(di)].h > now_h_ + 1e-9) return nullptr;
        return &s.deliveries[static_cast<std::size_t>(di)];
    }

    // When a person would learn a fact if nothing new happened before `horizon_h` (-1: not by then). For a raid
    // warning: how much warning the player's character gets.
    double WhenKnown(int fact, int position, double horizon_h) const {
        Delivery d;
        return PreviewDelivery(fact, position, horizon_h, d) ? d.h : -1.0;
    }
    // The same, with who would tell them and how.
    bool PreviewDelivery(int fact, int position, double horizon_h, Delivery& out) const {
        if (fact < 0 || static_cast<std::size_t>(fact) >= states_.size() || position < 0) return false;
        FactState copy = states_[static_cast<std::size_t>(fact)];
        Run(fact, copy, horizon_h);
        if (static_cast<std::size_t>(position) >= copy.delivered.size()) return false;
        const int di = copy.delivered[static_cast<std::size_t>(position)];
        if (di < 0) return false;
        out = copy.deliveries[static_cast<std::size_t>(di)];
        return true;
    }
    // The fact about a subject of a kind, the latest added ("package:12" for a raid warning); -1 if none.
    int FindFact(FactKind kind, const std::string& subject) const {
        for (int i = static_cast<int>(facts_.size()) - 1; i >= 0; --i) {
            if (facts_[static_cast<std::size_t>(i)].kind == kind && facts_[static_cast<std::size_t>(i)].subject == subject) return i;
        }
        return -1;
    }

    // Where a person is at campaign hour h (a briefing or a visit takes them away from their post).
    int PlaceOf(int position, double h) const {
        if (position < 0) return -1;
        const std::string& id = data_.positions[static_cast<std::size_t>(position)].id;
        for (const Routine& r : data_.routines) {
            if (r.kind != RoutineKind::Briefing && r.kind != RoutineKind::Visit) continue;
            const bool in = r.by == id || std::find(r.with.begin(), r.with.end(), id) != r.with.end();
            if (!in) continue;
            for (double s : Occurrences(r, h - r.duration_h, h)) {
                if (h >= s && h < s + r.duration_h) return data_.PlaceIndex(r.place);
            }
        }
        return home_[static_cast<std::size_t>(position)];
    }

    // A place's position at hour h, as known by then (the battery's post moves with the battery).
    Vec3 PlacePos(int place, double h) const {
        if (place < 0) return Vec3{};
        Vec3 pos = data_.places[static_cast<std::size_t>(place)].pos;
        double best = -1e18;
        for (const PlaceMove& m : moves_) {
            if (m.h <= h && m.h >= best && m.place == data_.places[static_cast<std::size_t>(place)].id) {
                best = m.h;
                pos = m.pos;
            }
        }
        return pos;
    }

    // The next contacts a person has: (start hour, routine) from h on, within `horizon_h`, soonest first.
    std::vector<std::pair<double, const Routine*>> NextContacts(int position, double h, double horizon_h) const {
        std::vector<std::pair<double, const Routine*>> out;
        if (position < 0) return out;
        const std::string& id = data_.positions[static_cast<std::size_t>(position)].id;
        const int home = home_[static_cast<std::size_t>(position)];
        for (const Routine& r : data_.routines) {
            bool in = r.by == id || r.carrier == id || std::find(r.with.begin(), r.with.end(), id) != r.with.end();
            if (r.kind == RoutineKind::CourierRun) {
                for (const std::string& s : r.stops) in = in || data_.PlaceIndex(s) == home;
            }
            if (r.kind == RoutineKind::Broadcast) in = true;
            if (!in) continue;
            for (double s : Occurrences(r, h, h + horizon_h)) {
                if (s >= h) out.push_back({s, &r});
            }
        }
        std::sort(out.begin(), out.end(), [](const std::pair<double, const Routine*>& a, const std::pair<double, const Routine*>& b) {
            return a.first < b.first || (a.first == b.first && a.second->id < b.second->id);
        });
        return out;
    }

    // Radio calls made (the enemy's intercept service hears every one), up to now.
    std::vector<RadioCall> RadioCalls() const {
        std::vector<RadioCall> out;
        for (const FactState& s : states_) {
            for (const RadioCall& c : s.radio) {
                if (c.h <= now_h_ + 1e-9) out.push_back(c);
            }
        }
        std::sort(out.begin(), out.end(), [](const RadioCall& a, const RadioCall& b) { return a.h < b.h || (a.h == b.h && a.fact < b.fact); });
        return out;
    }

    // "D2 19:20, Major Imre Sallai (chief of staff ...), in person at the regiment command post"
    std::string Describe(const Delivery& d) const {
        std::string s = FormatClock(d.h, start_hour_) + ", ";
        const std::string where = d.place.empty() ? std::string() : PlaceName(d.place);
        if (d.from == -1) return s + "seen yourself" + (where.empty() ? std::string() : " at " + where);
        if (d.from == -2) return s + "the state radio's news";
        const Position& p = data_.positions[static_cast<std::size_t>(d.from)];
        const SignalMean* m = d.mean >= 0 ? &data_.means[static_cast<std::size_t>(d.mean)] : nullptr;
        const MeanKind k = m != nullptr ? m->kind : MeanKind::InPerson;
        switch (k) {
            case MeanKind::InPerson: return s + p.Label() + ", in person" + (where.empty() ? std::string() : " at " + where);
            case MeanKind::Messenger: return s + "a runner from " + p.Label();
            case MeanKind::DespatchRider: return s + "a despatch rider with a message from " + p.Label();
            case MeanKind::Courier: return s + "by the courier: " + (d.full ? "a paper" : "a note") + " from " + p.Label();
            case MeanKind::FieldTelephone: return s + p.Label() + ", on the field telephone";
            case MeanKind::CityTelephone: return s + p.Label() + ", on the town telephone";
            case MeanKind::RadioVoice: return s + p.Label() + ", on the radio";
            case MeanKind::WarningReceiver: return s + "the division's early-warning net (" + p.Label() + ")";
            case MeanKind::Teleprinter: return s + "a teleprinter message from " + p.Label();
            case MeanKind::Siren: return s + "the sirens";
            case MeanKind::FieldPost: return s + "the field post, from " + p.Label();
            case MeanKind::Broadcast: return s + "the state radio's news";
        }
        return s + p.Label();
    }

    std::string PlaceName(const std::string& place_id) const {
        const Place* p = data_.FindPlace(place_id);
        return p == nullptr ? place_id : p->name;
    }

private:
    struct Edge {
        int to = -1;
        bool link = false;
    };
    struct Ev {
        double t = 0.0;
        int node = -1;
        int from = -1;     // -1 saw it, -2 the state radio
        int mean = -1;
        bool full = false;
        int attempt = 0;
        bool travel = false;  // a carrier on the road: decide a loss when it arrives
        double depart = 0.0;
        Vec3 a, b;
    };
    struct FactState {
        bool started = false;
        std::vector<std::vector<int>> children;
        std::vector<int> delivered;  // per position: index into deliveries, -1 none
        std::vector<Delivery> deliveries;
        std::vector<Ev> queue;       // a heap, earliest first
        std::vector<RadioCall> radio;
        double done_h = -1e18;
    };

    ChainData data_;
    unsigned seed_ = 1;
    double start_hour_ = 0.0;
    double now_h_ = -1e18;
    int player_ = -1;
    int in_person_ = -1;
    std::vector<int> home_;
    std::vector<std::vector<Edge>> edges_;
    std::vector<Fact> facts_;
    mutable std::vector<FactState> states_;
    std::map<std::string, int> by_key_;
    std::vector<Danger> dangers_;
    std::vector<WireCut> cuts_;
    std::vector<RadioJam> jams_;
    std::vector<PlaceMove> moves_;

    void Connect(int a, int b, bool link) {
        if (a < 0 || b < 0 || a == b) return;
        for (const Edge& e : edges_[static_cast<std::size_t>(a)]) {
            if (e.to == b) return;
        }
        edges_[static_cast<std::size_t>(a)].push_back({b, link});
        edges_[static_cast<std::size_t>(b)].push_back({a, link});
    }

    // ---- randomness: a hash of what is being decided
    static std::uint64_t Mix(std::uint64_t x) {
        x += 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }
    static std::uint64_t HashStr(const std::string& s) {
        std::uint64_t h = 1469598103934665603ull;
        for (char c : s) {
            h ^= static_cast<unsigned char>(c);
            h *= 1099511628211ull;
        }
        return h;
    }
    bool Chance(double p, int fact, int a, int b, int attempt, std::uint64_t salt) const {
        if (p <= 0.0) return false;
        std::uint64_t k = Mix(HashStr(facts_[static_cast<std::size_t>(fact)].key) ^ (static_cast<std::uint64_t>(seed_) << 32));
        k = Mix(k ^ (static_cast<std::uint64_t>(a + 7) * 0x100000001B3ull));
        k = Mix(k ^ (static_cast<std::uint64_t>(b + 11) * 0x2545F4914F6CDD1Dull));
        k = Mix(k ^ (static_cast<std::uint64_t>(attempt) << 8) ^ salt);
        const double u = static_cast<double>(k >> 11) * (1.0 / 9007199254740992.0);
        return u < p;
    }

    // ---- schedules
    std::vector<double> Occurrences(const Routine& r, double from_h, double to_h) const {
        std::vector<double> out;
        for (double local : r.at_local_h) {
            double first = local - start_hour_;
            first -= 24.0 * std::floor(first / 24.0);  // the first time that local hour comes, from h = 0
            if (to_h < first) continue;
            double k = std::max(0.0, std::floor((from_h - first) / r.every_h));
            for (double s = first + k * r.every_h; s <= to_h + 1e-9; s += r.every_h) {
                if (s >= from_h - 1e-9) out.push_back(s);
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    double NextBroadcast(double h) const {
        double best = -1.0;
        for (const Routine& r : data_.routines) {
            if (r.kind != RoutineKind::Broadcast) continue;
            for (double s : Occurrences(r, h, h + 48.0)) {
                if (s >= h && (best < 0.0 || s < best)) best = s;
            }
        }
        return best;
    }

    bool WireCutAt(int place, double h) const {
        if (place < 0) return false;
        const std::string& id = data_.places[static_cast<std::size_t>(place)].id;
        for (const WireCut& c : cuts_) {
            if (c.place == id && c.from_h <= h && h < c.until_h) return true;
        }
        return false;
    }
    bool Jammed(double h) const {
        for (const RadioJam& j : jams_) {
            if (j.from_h <= h && h < j.until_h) return true;
        }
        return false;
    }
    bool Night(double h) const {
        double local = start_hour_ + h;
        local -= 24.0 * std::floor(local / 24.0);
        return local >= 20.0 || local < 6.0;
    }

    double RelayDelayH(int a, int precedence) const {
        const RelayRules& r = data_.relay;
        const double base = precedence >= 2 ? r.urgent_s : (precedence == 1 ? r.priority_s : r.routine_s);
        return base * r.SkillFactor(data_.positions[static_cast<std::size_t>(a)].holder.skill) / 3600.0;
    }
    double HandlingH(const SignalMean& m, int precedence) const {
        const bool voice = m.kind == MeanKind::InPerson || m.kind == MeanKind::RadioVoice || m.kind == MeanKind::FieldTelephone ||
                           m.kind == MeanKind::CityTelephone || m.kind == MeanKind::Messenger;
        const double f = precedence >= 2 && voice ? data_.relay.urgent_handling_factor : 1.0;
        return m.handling_s * f / 3600.0;
    }
    double TravelH(const SignalMean& m, const Vec3& a, const Vec3& b, double depart) const {
        const double speed = m.speed_mps * (Night(depart) ? data_.relay.night_speed_factor : 1.0);
        if (speed <= 0.0) return 1e9;
        const double d = std::hypot(b.x - a.x, b.y - a.y) * data_.relay.road_factor;
        return d / speed / 3600.0;
    }

    // The next time a and b are at the same place, from h, within the horizon (-1 if none).
    double NextMeeting(int a, int b, double h, double horizon_h) const {
        std::vector<double> cand{h};
        for (const Routine& r : data_.routines) {
            if (r.kind != RoutineKind::Briefing && r.kind != RoutineKind::Visit) continue;
            for (double s : Occurrences(r, h - r.duration_h, h + horizon_h)) {
                if (s >= h) cand.push_back(s);
                if (s + r.duration_h >= h) cand.push_back(s + r.duration_h + 1e-6);
            }
        }
        std::sort(cand.begin(), cand.end());
        for (double c : cand) {
            if (c > h + horizon_h) break;
            if (PlaceOf(a, c) == PlaceOf(b, c) && PlaceOf(a, c) >= 0) return c;
        }
        return -1.0;
    }

    // The next courier run that picks up at a's place and later stops at b's (arrival hour, -1 if none).
    double NextCourier(int a, int b, double h, double horizon_h, int& mean_out) const {
        double best = -1.0;
        for (const Routine& r : data_.routines) {
            if (r.kind != RoutineKind::CourierRun) continue;
            const int mi = data_.MeanIndex(r.mean);
            if (mi < 0) continue;
            const SignalMean& m = data_.means[static_cast<std::size_t>(mi)];
            for (double s : Occurrences(r, h - 12.0, h + horizon_h)) {
                // The run's stop times (the stops' positions as known when it sets out).
                std::vector<double> at(r.stops.size(), s);
                for (std::size_t i = 1; i < r.stops.size(); ++i) {
                    const Vec3 p0 = PlacePos(data_.PlaceIndex(r.stops[i - 1]), s);
                    const Vec3 p1 = PlacePos(data_.PlaceIndex(r.stops[i]), s);
                    at[i] = at[i - 1] + m.handling_s / 3600.0 + TravelH(m, p0, p1, at[i - 1]);
                }
                for (std::size_t i = 0; i < r.stops.size(); ++i) {
                    if (at[i] < h || data_.PlaceIndex(r.stops[i]) != PlaceOf(a, at[i])) continue;
                    for (std::size_t j = i + 1; j < r.stops.size(); ++j) {
                        if (data_.PlaceIndex(r.stops[j]) != PlaceOf(b, at[j])) continue;
                        if (best < 0.0 || at[j] < best) {
                            best = at[j];
                            mean_out = mi;
                        }
                        break;
                    }
                }
            }
        }
        return best > h + horizon_h ? -1.0 : best;
    }

    // ---- one fact
    void Route(int fi, FactState& s) const {
        const Fact& f = facts_[static_cast<std::size_t>(fi)];
        const std::size_t n = data_.positions.size();
        s.delivered.assign(n, -1);
        s.children.assign(n, {});
        // Who it is for.
        std::vector<char> want(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            const Position& p = data_.positions[i];
            if (p.level > f.need_level) continue;
            if (!f.audience.empty() && !data_.InSubtree(p.id, f.audience)) continue;
            want[i] = 1;
        }
        // The shortest way along the chain (links cost more for routine facts: those go through the staff).
        const int origin = data_.PositionIndex(f.origin);
        std::vector<double> dist(n, 1e18);
        std::vector<int> parent(n, -1);
        std::vector<char> done(n, 0);
        std::vector<int> sources;
        if (origin >= 0) sources.push_back(origin);
        for (const std::string& w : f.witnesses) {
            const int wi = data_.PositionIndex(w);
            if (wi >= 0) sources.push_back(wi);
        }
        for (int src : sources) dist[static_cast<std::size_t>(src)] = 0.0;
        for (std::size_t iter = 0; iter < n; ++iter) {
            int u = -1;
            for (std::size_t i = 0; i < n; ++i) {
                if (!done[i] && dist[i] < 1e17 && (u < 0 || dist[i] < dist[static_cast<std::size_t>(u)])) u = static_cast<int>(i);
            }
            if (u < 0) break;
            done[static_cast<std::size_t>(u)] = 1;
            // Nothing passes through the player's character on its own (a source is the exception: they saw it).
            const bool is_source = std::find(sources.begin(), sources.end(), u) != sources.end();
            if (u == player_ && !is_source) continue;
            for (const Edge& e : edges_[static_cast<std::size_t>(u)]) {
                const double w = e.link && f.precedence == 0 ? 3.0 : 1.0;
                const std::size_t v = static_cast<std::size_t>(e.to);
                if (dist[static_cast<std::size_t>(u)] + w < dist[v] - 1e-12) {
                    dist[v] = dist[static_cast<std::size_t>(u)] + w;
                    parent[v] = u;
                }
            }
        }
        // Keep the branches that lead to someone it is for.
        std::vector<char> needed(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            if (!want[i] || dist[i] > 1e17) continue;
            int at = static_cast<int>(i);
            while (at >= 0 && !needed[static_cast<std::size_t>(at)]) {
                needed[static_cast<std::size_t>(at)] = 1;
                at = parent[static_cast<std::size_t>(at)];
            }
        }
        for (std::size_t i = 0; i < n; ++i) {
            if (needed[i] && parent[i] >= 0) s.children[static_cast<std::size_t>(parent[i])].push_back(static_cast<int>(i));
        }
        for (std::vector<int>& c : s.children) std::sort(c.begin(), c.end());
        // The start: the origin and the witnesses know it at h.
        for (int src : sources) {
            Ev e;
            e.t = f.h;
            e.node = src;
            e.from = -1;
            e.full = true;
            Push(s, e);
        }
        if (f.broadcast) {
            const double b = NextBroadcast(f.h);
            if (b >= 0.0) {
                for (std::size_t i = 0; i < n; ++i) {
                    if (!want[i]) continue;
                    Ev e;
                    e.t = b;
                    e.node = static_cast<int>(i);
                    e.from = -2;
                    e.mean = data_.MeanIndex("broadcast");
                    e.full = false;
                    Push(s, e);
                }
            }
        }
        s.started = true;
    }

    static bool Later(const Ev& a, const Ev& b) {
        if (a.t != b.t) return a.t > b.t;
        if (a.node != b.node) return a.node > b.node;
        if (a.from != b.from) return a.from > b.from;
        return a.attempt > b.attempt;
    }
    static void Push(FactState& s, const Ev& e) {
        s.queue.push_back(e);
        std::push_heap(s.queue.begin(), s.queue.end(), Later);
    }

    void Run(int fi, FactState& s, double until_h) const {
        const Fact& f = facts_[static_cast<std::size_t>(fi)];
        if (f.h > until_h) return;
        if (!s.started) Route(fi, s);
        while (!s.queue.empty() && s.queue.front().t <= until_h) {
            std::pop_heap(s.queue.begin(), s.queue.end(), Later);
            const Ev e = s.queue.back();
            s.queue.pop_back();
            // A carrier on the road: did it get through?
            if (e.travel && Lost(fi, e)) {
                if (e.attempt < 3) Send(fi, s, e.from, e.node, e.t + data_.relay.retry_h, e.full, e.attempt + 1);
                continue;
            }
            Deliver(fi, s, e);
        }
        s.done_h = std::max(s.done_h, until_h);
    }

    bool Lost(int fi, const Ev& e) const {
        for (const Danger& d : dangers_) {
            if (d.known_h > e.t || d.t1_h < e.depart || d.t0_h > e.t) continue;
            const double dx = e.b.x - e.a.x, dy = e.b.y - e.a.y;
            const double len2 = dx * dx + dy * dy;
            double u = len2 > 0.0 ? ((d.center.x - e.a.x) * dx + (d.center.y - e.a.y) * dy) / len2 : 0.0;
            u = std::max(0.0, std::min(1.0, u));
            const double px = e.a.x + u * dx - d.center.x, py = e.a.y + u * dy - d.center.y;
            if (px * px + py * py > d.radius_m * d.radius_m) continue;
            const std::uint64_t salt = static_cast<std::uint64_t>(std::llround(d.t0_h * 100.0)) * 31ull;
            if (Chance(d.chance, fi, e.from, e.node, e.attempt, salt)) return true;
        }
        return false;
    }

    void Deliver(int fi, FactState& s, const Ev& e) const {
        const Fact& f = facts_[static_cast<std::size_t>(fi)];
        const std::size_t node = static_cast<std::size_t>(e.node);
        const int place = PlaceOf(e.node, e.t);
        int& di = s.delivered[node];
        if (di >= 0) {
            Delivery& d = s.deliveries[static_cast<std::size_t>(di)];
            if (e.full && !d.full) {
                d.full = true;
                d.full_h = e.t;
            }
            return;  // they knew it already: nothing more to pass on
        }
        Delivery d;
        d.fact = fi;
        d.position = e.node;
        d.h = e.t;
        d.from = e.from;
        d.mean = e.mean;
        d.place = place >= 0 ? data_.places[static_cast<std::size_t>(place)].id : std::string();
        d.full = e.full;
        if (e.full) d.full_h = e.t;
        s.deliveries.push_back(d);
        di = static_cast<int>(s.deliveries.size()) - 1;
        // The player's character tells nobody on their own (unless they saw it themselves: then it is theirs to report
        // like any witness's; the game passes it on through them when they do).
        const bool witness = e.from == -1;
        if (e.node == player_ && !witness) return;
        if (e.from == -2) return;  // heard on the news: not passed on as an order
        if (e.t > f.expires_h) return;
        for (int c : s.children[node]) {
            if (s.delivered[static_cast<std::size_t>(c)] >= 0) continue;
            Send(fi, s, e.node, c, e.t, e.full, 0);
        }
    }

    struct Option {
        double t = 1e18;
        int mean = -1;
        bool full = false;
        bool travel = false;
        double depart = 0.0;
    };

    // Pass a fact from a to b, deciding at hour t (after the sender's own delay) how it goes.
    void Send(int fi, FactState& s, int a, int b, double t, bool full, int attempt) const {
        const Fact& f = facts_[static_cast<std::size_t>(fi)];
        const Position& pa = data_.positions[static_cast<std::size_t>(a)];
        const Position& pb = data_.positions[static_cast<std::size_t>(b)];
        const double t0 = t + RelayDelayH(a, f.precedence);
        if (t0 > f.expires_h) return;
        const int la = PlaceOf(a, t0), lb = PlaceOf(b, t0);
        const Vec3 xa = PlacePos(la, t0), xb = PlacePos(lb, t0);
        const double dist = std::hypot(xb.x - xa.x, xb.y - xa.y);
        auto mean_of = [&](const char* id) { return data_.MeanIndex(id); };
        auto both = [&](const char* id) { return pa.Has(id) && pb.Has(id) && mean_of(id) >= 0; };
        auto M = [&](int mi) -> const SignalMean& { return data_.means[static_cast<std::size_t>(mi)]; };

        // Together now: tell them.
        if (la == lb && la >= 0) {
            Ev e;
            e.t = t0 + HandlingH(M(in_person_), f.precedence);
            e.node = b;
            e.from = a;
            e.mean = in_person_;
            e.full = full;
            e.attempt = attempt;
            Push(s, e);
            return;
        }

        // The electrical and runner options, for anything that can't wait for a contact.
        std::vector<Option> fast, wire;
        auto add = [&](std::vector<Option>& v, int mi, double at, bool paper, bool travel) {
            if (mi < 0) return;
            const SignalMean& m = M(mi);
            if (m.precedence < f.precedence) return;
            Option o;
            o.mean = mi;
            o.full = paper && full;
            o.travel = travel;
            o.depart = t0;
            o.t = at;
            // A garbled message is repeated (voice) or sent again.
            if (!travel && Chance(m.loss, fi, a, b, attempt, 0x51ull + static_cast<std::uint64_t>(mi))) o.t += 0.1;
            v.push_back(o);
        };
        if (both("radio_voice")) {
            const int mi = mean_of("radio_voice");
            if ((M(mi).range_m <= 0.0 || dist <= M(mi).range_m) && !Jammed(t0)) add(fast, mi, t0 + HandlingH(M(mi), f.precedence), false, false);
        }
        if (both("field_telephone") && !WireCutAt(la, t0) && !WireCutAt(lb, t0)) {
            const int mi = mean_of("field_telephone");
            const double connect = (f.precedence >= 2 ? M(mi).connect_s : M(mi).connect_routine_s) / 3600.0;
            add(wire, mi, t0 + connect + HandlingH(M(mi), f.precedence), false, false);
        }
        if (both("city_telephone") && !WireCutAt(la, t0) && !WireCutAt(lb, t0)) {
            const int mi = mean_of("city_telephone");
            const double connect = (f.precedence >= 2 ? M(mi).connect_s : M(mi).connect_routine_s) / 3600.0;
            add(wire, mi, t0 + connect + HandlingH(M(mi), f.precedence), false, false);
        }
        if (both("teleprinter") && f.precedence >= 1 && !WireCutAt(la, t0) && !WireCutAt(lb, t0)) {
            const int mi = mean_of("teleprinter");
            add(wire, mi, t0 + HandlingH(M(mi), f.precedence), true, false);
        }
        if (f.precedence >= 2 && both("warning_receiver") && pa.level < pb.level) {
            const int mi = mean_of("warning_receiver");
            if (M(mi).range_m <= 0.0 || dist <= M(mi).range_m) add(fast, mi, t0 + HandlingH(M(mi), f.precedence), false, false);
        }
        if (f.kind == FactKind::AirWarning && pa.Has("siren") && mean_of("siren") >= 0) {
            const int mi = mean_of("siren");
            if (M(mi).range_m <= 0.0 || dist <= M(mi).range_m) add(fast, mi, t0 + HandlingH(M(mi), f.precedence), false, false);
        }
        if (pa.Has("messenger") && mean_of("messenger") >= 0) {
            const int mi = mean_of("messenger");
            if (M(mi).range_m <= 0.0 || dist <= M(mi).range_m) add(fast, mi, t0 + HandlingH(M(mi), 0) + TravelH(M(mi), xa, xb, t0), true, true);
        }
        if (pa.Has("despatch_rider") && mean_of("despatch_rider") >= 0) {
            const int mi = mean_of("despatch_rider");
            add(fast, mi, t0 + HandlingH(M(mi), 0) + TravelH(M(mi), xa, xb, t0), true, true);
        }
        auto best_of = [](const std::vector<Option>& v) {
            Option pick;
            for (const Option& o : v) {
                if (o.t < pick.t || (o.t == pick.t && o.mean < pick.mean)) pick = o;
            }
            return pick;
        };

        Option chosen;
        if (f.precedence >= 2) {
            std::vector<Option> all = fast;
            all.insert(all.end(), wire.begin(), wire.end());
            chosen = best_of(all);
        } else if (f.precedence == 1) {
            chosen = !wire.empty() ? best_of(wire) : best_of(fast);
        } else {
            // Routine: the next contact, if one comes in time.
            const double horizon = data_.relay.max_wait_h;
            Option contact;
            const double meet = NextMeeting(a, b, t0, horizon);
            if (meet >= 0.0) {
                contact.t = meet + HandlingH(M(in_person_), 0);
                contact.mean = in_person_;
                contact.full = full;
            }
            int cm = -1;
            const double courier = NextCourier(a, b, t0, horizon, cm);
            if (courier >= 0.0 && courier + M(cm).handling_s / 3600.0 < contact.t) {
                contact.t = courier + M(cm).handling_s / 3600.0;
                contact.mean = cm;
                contact.full = full;
            }
            if (contact.mean >= 0) {
                chosen = contact;
            } else {
                chosen = !wire.empty() ? best_of(wire) : best_of(fast);
            }
        }
        if (chosen.mean < 0) return;  // no way at all
        if (M(chosen.mean).kind == MeanKind::RadioVoice) s.radio.push_back({t0, a, fi});
        Ev e;
        e.t = chosen.t;
        e.node = b;
        e.from = a;
        e.mean = chosen.mean;
        e.full = chosen.full;
        e.attempt = attempt;
        e.travel = chosen.travel;
        e.depart = chosen.depart;
        e.a = xa;
        e.b = xb;
        Push(s, e);
        // A paper sent ahead by voice follows by the next contact (or a rider if none comes in time).
        if (f.document && full && !chosen.full) {
            const double horizon = data_.relay.max_wait_h;
            double at = -1.0;
            int mi = -1;
            const double meet = NextMeeting(a, b, chosen.t, horizon);
            if (meet >= 0.0) {
                at = meet + HandlingH(M(in_person_), 0);
                mi = in_person_;
            }
            int cm = -1;
            const double courier = NextCourier(a, b, chosen.t, horizon, cm);
            if (courier >= 0.0 && (at < 0.0 || courier < at)) {
                at = courier + M(cm).handling_s / 3600.0;
                mi = cm;
            }
            if (mi >= 0) {
                Ev p;
                p.t = at;
                p.node = b;
                p.from = a;
                p.mean = mi;
                p.full = true;
                p.attempt = attempt + 10;
                Push(s, p);
            }
        }
    }
};

}  // namespace command
}  // namespace simcore
