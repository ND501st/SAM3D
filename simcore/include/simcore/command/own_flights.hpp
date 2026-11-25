// simcore/command/own_flights.hpp — our flights as actors under our ground control (roadmap track B, B10;
// docs/air_battle/08_OUR_FIGHTERS.md section 4).
//
//  - OwnGciPicture: what our GCI post sees, never the truth. Its radar takes the aircraft within its reach and over its
//    horizon every update_s, each with a keyed error, identified by its interrogator on the filter the post holds; the
//    controller reads the picture as old as the sector's way of passing it (about 10 s automated, vizh_tikshaev_pvo; 3-6 min
//    off a manual board, mt_1973_nvn). His own fighters he knows as his.
//  - OwnPairPlan: a scrambled pair as a session's flight (two targets, a route from the base toward the group the picture
//    showed and home), its notice (Franchuk's fields), its air fight entries; the raid builder, the testbed and the runner
//    turn it into a scenario's targets.
//
// Draws are keyed on Stream::AirCombat from kOwnAirKey + 0x400 (the picture) so none is shared with B8's fight.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#include "simcore/command/info_net.hpp"
#include "simcore/command/own_air.hpp"
#include "simcore/em/random.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace command {

// What the GCI's radar could look at this update: an aircraft, whether it answers the post's interrogator, and whether the
// post controls it (its own fighters, plotted as its own).
struct GciSeen {
    int truth = -1;         // for the act only: no decision reads it
    std::string id;
    Vec3 pos;
    Vec3 vel;
    bool replies = false;
    bool controlled = false;
};

struct GciTrack {
    int truth = -1;
    std::string id;
    Vec3 pos;
    Vec3 vel;
    double seen_s = 0.0;
    bool friend_reply = false;
    bool controlled = false;
    bool Hostile() const { return !friend_reply && !controlled; }
};

class OwnGciPicture {
public:
    void Init(const GciDef& d, const Vec3& radar, std::uint64_t seed, std::uint64_t key) {
        def_ = d;
        radar_ = radar;
        seed_ = seed;
        key_ = key;
        snaps_.clear();
        next_s_ = -1e18;
        n_ = 0;
    }
    const Vec3& Radar() const { return radar_; }
    double UpdateS() const { return def_.update_s; }
    bool Due(double t) const { return t + 1e-9 >= next_s_; }

    // How old the picture is when the controller reads it: the automated sector's seconds, or a manual board's minutes (a
    // keyed draw per update).
    double AgeS(std::uint64_t n) const {
        if (def_.automated) return def_.age_auto_s;
        const double u = em::Uniform01(em::DrawHash(seed_, em::Stream::AirCombat, kOwnAirKey + 0x401ull, key_, n));
        return def_.age_manual_min_s + (def_.age_manual_max_s - def_.age_manual_min_s) * u;
    }

    // A look by the radar now: `visible(p)` says whether the radar's line of sight reaches the point (the horizon, the
    // ground); each one within reach and seen is plotted with the post's error.
    void Observe(double t, const std::vector<GciSeen>& seen, const std::function<bool(const Vec3&)>& visible) {
        if (!Due(t)) return;
        next_s_ = t + def_.update_s;
        Snap s;
        s.t_s = t;
        s.n = ++n_;
        s.ready_s = t + AgeS(s.n);
        for (const GciSeen& a : seen) {
            if ((a.pos - radar_).Length() > def_.radar_range_m) continue;
            if (visible && !visible(a.pos)) continue;
            GciTrack k;
            k.truth = a.truth;
            k.id = a.id;
            const std::uint64_t ak = static_cast<std::uint64_t>(a.truth + 1);
            const double ex = def_.error_m * em::StandardNormal(em::DrawHash(seed_, em::Stream::AirCombat, kOwnAirKey + 0x402ull, key_ ^ ak, s.n));
            const double ey = def_.error_m * em::StandardNormal(em::DrawHash(seed_, em::Stream::AirCombat, kOwnAirKey + 0x403ull, key_ ^ ak, s.n));
            k.pos = a.pos + Vec3{ex, ey, 0.0};
            k.vel = a.vel;
            k.seen_s = t;
            k.friend_reply = a.replies;
            k.controlled = a.controlled;
            s.tracks.push_back(k);
        }
        snaps_.push_back(std::move(s));
        while (snaps_.size() > 2 && snaps_[1].ready_s <= t - 2.0 * def_.update_s) snaps_.pop_front();
        while (snaps_.size() > 64) snaps_.pop_front();
    }

    // The picture as the controller has it at t: the latest look that has reached his board.
    std::vector<GciTrack> At(double t) const {
        const Snap* best = nullptr;
        for (const Snap& s : snaps_) {
            if (s.ready_s <= t + 1e-9 && (best == nullptr || s.t_s > best->t_s)) best = &s;
        }
        return best != nullptr ? best->tracks : std::vector<GciTrack>();
    }

private:
    struct Snap {
        double t_s = 0.0;
        double ready_s = 0.0;
        std::uint64_t n = 0;
        std::vector<GciTrack> tracks;
    };
    GciDef def_;
    Vec3 radar_;
    std::uint64_t seed_ = 0;
    std::uint64_t key_ = 0;
    std::deque<Snap> snaps_;
    double next_s_ = -1e18;
    std::uint64_t n_ = 0;
};

// ------------------------------------------------------------------------------------------------ a scrambled pair

struct OwnPairPlan {
    std::string flight;            // the flight's id in the session
    std::string callsign;
    std::string regiment;
    std::string type;              // air_combat.json's fit id ("mig21mf")
    std::string name;              // "MiG-21"
    std::vector<std::string> ids;  // the two targets
    Vec3 base;
    std::vector<Vec3> route;       // from the base toward the group, then home
    double alt_m = 4000.0;         // its transit height above sea level
    double speed_mps = 250.0;
    double appear_s = 0.0;         // airborne
    double skill = 0.45;
    std::string controller;        // the GCI's id in the air fight
};

// A pair from a scramble: up from the base, toward where the group was on the picture, and home the same way.
inline OwnPairPlan PlanPair(const Scramble& s, const FighterRegimentDef& r, int number, double alt_m, double speed_mps) {
    OwnPairPlan p;
    p.regiment = r.id;
    p.type = r.type;
    p.name = r.name;
    p.callsign = "Sokol " + std::to_string(number);
    p.flight = "own" + std::to_string(number);
    p.ids = {p.flight + "-1", p.flight + "-2"};
    p.base = r.base_pos;
    p.alt_m = alt_m;
    p.speed_mps = speed_mps;
    p.appear_s = s.airborne_s;
    p.skill = r.skill;
    p.controller = r.gci;
    Vec3 to = Vec3{s.toward.x, s.toward.y, 0.0};
    Vec3 from = Vec3{r.base_pos.x, r.base_pos.y, 0.0};
    p.route = {from, to, from};
    return p;
}

// The notice of a pair's flight (mt_franchuk_front p. 6): its route as planned, its heights, the clock hours it may be over
// the SAM zones (from airborne to its return), its return; actual (a scramble, not a plan).
inline OwnFlightNotice NoticeOf(const OwnPairPlan& p, int number, double clock_start_s, const NoticeDef& d) {
    OwnFlightNotice n;
    n.number = number;
    n.actual = true;
    n.flight = p.callsign;
    n.type = p.name;
    n.count = static_cast<int>(p.ids.size());
    n.route = p.route;
    n.half_width_m = d.corridor_half_width_m;
    n.floor_m = std::max(0.0, std::floor((p.alt_m - 2000.0) / 500.0) * 500.0);
    n.ceiling_m = std::ceil((p.alt_m + 2000.0) / 500.0) * 500.0;
    double len = 0.0;
    for (std::size_t i = 0; i + 1 < p.route.size(); ++i) len += (p.route[i + 1] - p.route[i]).Length();
    n.cross_from_h = (clock_start_s + p.appear_s) / 3600.0;
    n.cross_to_h = (clock_start_s + p.appear_s + len / std::max(50.0, p.speed_mps) + 900.0) / 3600.0;
    n.back_h = n.cross_to_h;
    return n;
}

// ------------------------------------------------------------------------------------------------ the notice's way

// A notice of our flight down the information pipeline (section 6): sent by its origin (the fighter regiment's duty
// officer) at sent_h, passed post by post by the chain's means with each hop's work (the talk table's coding and decoding,
// NoticeDef::hop_work_s), to the post that tells the battery (the regiment's command post, which says it on the battery's
// net). Hours are the chain's (session hours from the clock start: start_hour is the clock hour at 0). The trace holds each
// post and the hour it had the notice; at_battery_h and written_h are the runner's to fill.
// Any of our aviation's facts down the chain the same way (S1): a notice (FactKind::OwnAirNotice), a fighter regiment's loss
// (FactKind::OwnDamage) or its claim (FactKind::RaidResult), each with its key, subject, precedence and each hop's work.
inline NoticeTrace RunOwnAirFact(const ChainData& chain, FactKind kind, const std::string& key, const std::string& subject, const std::string& text,
                                 const std::string& origin, const std::string& to_post, double sent_h, int precedence, double hop_work_s,
                                 unsigned seed, double start_hour, double horizon_h = 6.0) {
    NoticeTrace tr;
    tr.flight = subject;
    tr.sent_h = sent_h;
    InfoNet net;
    if (!net.Init(chain, seed, start_hour)) return tr;
    const int to = chain.PositionIndex(to_post);
    const int from = chain.PositionIndex(origin);
    if (to < 0 || from < 0) return tr;
    Fact f;
    f.key = key;
    f.kind = kind;
    f.h = sent_h;
    f.origin = origin;
    f.text = text;
    f.subject = subject;
    f.need_level = chain.positions[static_cast<std::size_t>(to)].level;
    f.precedence = precedence;
    f.hop_work_s = hop_work_s;
    const int fact = net.AddFact(f);
    std::vector<NoticeHop> back;
    Delivery dl;
    int at = to;
    for (int guard = 0; guard < 32 && at >= 0 && at != from; ++guard) {
        if (!net.PreviewDelivery(fact, at, sent_h + horizon_h, dl)) return tr;
        back.push_back({chain.positions[static_cast<std::size_t>(at)].id, dl.h});
        at = dl.from;
    }
    if (at != from) return tr;
    tr.hops.assign(back.rbegin(), back.rend());
    return tr;
}
inline NoticeTrace RunNotice(const ChainData& chain, const OwnFlightNotice& n, const std::string& text, const std::string& origin,
                             const std::string& to_post, double sent_h, const NoticeDef& d, unsigned seed, double start_hour,
                             double horizon_h = 6.0) {
    return RunOwnAirFact(chain, FactKind::OwnAirNotice, "own_air_notice:" + std::to_string(n.number) + ":" + std::to_string(n.amendment), n.flight,
                         text, origin, to_post, sent_h, d.precedence, d.hop_work_s, seed, start_hour, horizon_h);
}
// A regiment's loss or claim report down the chain (S1).
inline NoticeTrace RunOwnAirReport(const ChainData& chain, const OwnAirReport& r, int number, const std::string& text, const std::string& origin,
                                   const std::string& to_post, double sent_h, const ReportsDef& d, unsigned seed, double start_hour,
                                   double horizon_h = 6.0) {
    const bool loss = r.kind == OwnAirReportKind::Loss;
    return RunOwnAirFact(chain, loss ? FactKind::OwnDamage : FactKind::RaidResult, std::string(loss ? "own_air_loss:" : "own_air_claim:") + std::to_string(number),
                         r.flight, text, origin, to_post, sent_h, d.precedence, d.hop_work_s, seed, start_hour, horizon_h);
}
// When the post that tells the battery had it (session hours; -1e18: never within the horizon).
inline double NoticeAtPostH(const NoticeTrace& t) { return t.hops.empty() ? -1e18 : t.hops.back().h; }

// ------------------------------------------------------------------------------------------------ a scenario's block

// A notice the scenario sends: its fields, when its origin sends it (session s), and the aircraft it covers (the debrief's
// trace only: no crew reads them).
struct OwnAirScenarioNotice {
    OwnFlightNotice notice;
    double sent_s = 0.0;
    bool lost = false;                 // never sent on (a missed notice: section 8)
    std::vector<std::string> aircraft;
};

// A scenario's "own_air" block (roadmap B10, docs/air_battle/08_OUR_FIGHTERS.md): the rules and codes files, the war or
// peace codes and the day, when the day's code table reached the battery and the post (-1: never; they keep the code they
// had), the plan of cooperation the battery was given, the notices sent, our fighter regiments and their GCI.
struct OwnAirScenarioDef {
    bool present = false;
    std::string data_file = "command/own_air.json";
    std::string codes_file = "command/iff_codes.json";
    bool war = true;
    int day = 0;
    int fixed_code = 0;                // > 0: one code the whole session (a lesson)
    double battery_table_s = 0.0;      // the code table reached the battery (session s; < 0 never)
    int battery_last_code = 0;         // the code it had before (0: the table's first)
    double post_table_s = 0.0;         // the same for the command post
    int post_last_code = 0;
    std::string chain_file = "command/chain_pannonia.json";
    std::string origin = "fighter_rgt_duty";
    std::string notice_post = "rgt_cp_duty";
    CooperationPlan plan;
    std::vector<OwnAirScenarioNotice> notices;
    std::vector<FighterRegimentDef> regiments;
    Vec3 gci_radar;
    double gci_mast_m = 25.0;
    double raid_warning_s = -1.0;
    // S1: the front line the airborne-alert zones stand behind and the pursuit limit is measured from (a point of it and the
    // direction across it to the enemy), and whether the enemy's allies fly our Kremniy (a war with the Pact: the false friend).
    bool has_front = false;
    Vec3 front;
    Vec3 to_enemy{0.0, 1.0, 0.0};
    bool enemy_allies = false;
    // Loaded.
    OwnAirDef def;
    IffCodesDef codes;
    ChainData chain;
    bool has_chain = false;
    std::string source;
    std::string confidence;
};

}  // namespace command
}  // namespace simcore
