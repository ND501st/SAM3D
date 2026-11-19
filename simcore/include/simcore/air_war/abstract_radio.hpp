// simcore/air_war/abstract_radio.hpp — the calls of the packages no session flies, and our posts' hearing of them in closed
// form (roadmap S6, docs/air_battle/09_COMMS_AND_INTERCEPT.md section 2.4, level A0).
//
// A session's flights speak on its EM world (air_radio.hpp); a package the campaign resolves abstractly has no node. Its
// calls are made here from what it did (its route's points, its losses, whether it released, aborted or was comm-out) and
// from the messages its command's posts sent about it (c2.hpp's log), each with a place and an antenna height. Our posts
// hear a call by the 4/3-earth horizon (an HF request net by a flat reach), with a keyed chance, and take a bearing with
// the VHF DF error; the desk (command/intercept.hpp) then writes it up as for a session's calls, and the day's summary
// (the slow road, section 5.2) is all that comes of it. Every draw is keyed on the seed, the package and the call, with
// stable string hashes, so a campaign loaded from its save hears the same.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/air_war/air_radio.hpp"
#include "simcore/air_war/c2.hpp"
#include "simcore/air_war/data.hpp"
#include "simcore/air_war/officers.hpp"
#include "simcore/air_war/state.hpp"

namespace simcore {
namespace airwar {

// A string's key for the draws, the same on every compiler (FNV-1a, then mixed).
inline std::uint64_t AbstractKeyOf(const std::string& s) {
    std::uint64_t h = 1469598103934665603ull;
    for (char ch : s) h = (h ^ static_cast<unsigned char>(ch)) * 1099511628211ull;
    return em::Mix64(h);
}

// One call of an abstract package: the call as air_radio.hpp keeps it (times in campaign seconds, hour x 3600), where it
// was said from (theatre ENU) and the speaker's antenna above the ground.
struct AbstractCall {
    Call call;
    Vec3 from;
    double height_m = 0.0;
    bool hf = false;         // on an HF net (the request nets): heard by an HF receiver, without a horizon
    int package_id = -1;
};

// One of our listening receivers in the campaign (a post's VHF/UHF or its HF set).
struct ListeningPost {
    std::string id;          // as the desk logs it ("intercept_post_1", "intercept_post_1_hf")
    std::string chain_post;  // the post in data/command/intercept.json
    Vec3 at;                 // theatre ENU
    double mast_m = 30.0;
    bool hf = false;
};

namespace detail_abstract {

inline double CallLengthS(CallKind k, const AirRadioDef& d) { return d.Tune(std::string("radio.call_") + CallKindId(k) + "_s"); }

inline const char* const kCodewords[] = {"SUPERFUSE", "LONGBOW", "REDWING", "SLEDGE", "TIDEWATER"};

inline Vec3 RouteAt(const Flight& f, double h, double* agl_m) {
    if (f.route.empty()) return Vec3();
    if (h <= f.route.front().t_h) {
        *agl_m = f.route.front().agl_m;
        return f.route.front().pos;
    }
    for (std::size_t i = 1; i < f.route.size(); ++i) {
        const RoutePoint& a = f.route[i - 1];
        const RoutePoint& b = f.route[i];
        if (h > b.t_h) continue;
        const double u = b.t_h > a.t_h ? (h - a.t_h) / (b.t_h - a.t_h) : 1.0;
        *agl_m = a.agl_m + (b.agl_m - a.agl_m) * u;
        return Vec3{a.pos.x + (b.pos.x - a.pos.x) * u, a.pos.y + (b.pos.y - a.pos.y) * u, 0.0};
    }
    *agl_m = f.route.back().agl_m;
    return f.route.back().pos;
}

}  // namespace detail_abstract

// Whether a flight of a deception package (or a decoy flight) talks as a strike would, naming a target (section 8,
// nazarenko_1984's radio demonstration): the faction's share, more with a feint artist or a decoy man planning or
// commanding it. Keyed on the seed, the package and the flight.
inline bool Demonstrates(const Package& p, std::size_t fi, const FactionRadio& fr, const AirStaff& staff, const AirRadioDef& td,
                         std::uint64_t seed) {
    const Flight& fl = p.flights[fi];
    if (p.kind != MissionKind::Deception && fl.role != FlightRole::Decoy) return false;
    double share = fr.demonstration;
    for (int who : {p.planner, p.commander}) {
        if (who < 0 || static_cast<std::size_t>(who) >= staff.officers.size()) continue;
        const Officer& o = staff.officers[static_cast<std::size_t>(who)];
        if (o.HasQuirk("feint_artist") || o.HasQuirk("decoy_lover")) {
            share += td.Tune("radio.demonstration_quirk");
            break;
        }
    }
    if (share <= 0.0) return false;
    const em::Random rng(seed ^ 0xDE30057Aull);
    return rng.Uniform(em::Stream::AirRadio, static_cast<std::uint64_t>(p.id) + 1, fi + 1, 1) < std::min(1.0, share);
}

// The calls an abstract package's flights made on the way (section 2.4), at most `radio.abstract_calls` a flight, by the same
// rules a session's flights keep (section 3.2): a comm-out strike says nothing routine before its attack and its act at the
// IP goes by signal; a threat call is always said; past the attack a strike's routine calls are kept off the air with its
// discipline's chance and a comm-out flight says its codeword once. Ids are the package's id x 256 plus the call's index.
inline std::vector<AbstractCall> AbstractPackageCalls(const Package& p, const FactionRadio& fr, const AirStaff& staff, const AirRadioDef& td,
                                                      std::uint64_t seed) {
    std::vector<AbstractCall> out;
    if (!fr.present) return out;
    const em::Random rng(seed ^ 0xAB57AC70ull);
    double rotation = fr.callsign_rotation_h;
    if (p.planner >= 0 && static_cast<std::size_t>(p.planner) < staff.officers.size() &&
        staff.officers[static_cast<std::size_t>(p.planner)].Quirk("punctuality") < 0.0) {
        rotation *= 2.0;  // a careless planner doubles the call signs' interval (as the raid builder does)
    }
    const int per_flight = std::max(0, static_cast<int>(td.Tune("radio.abstract_calls") + 0.5));
    int index = 0;
    for (std::size_t fi = 0; fi < p.flights.size(); ++fi) {
        const Flight& fl = p.flights[fi];
        if (fl.role == FlightRole::Missile || fl.route.empty()) continue;
        double chatter = 0.0, leak = 0.0;
        if (fl.officer >= 0 && static_cast<std::size_t>(fl.officer) < staff.officers.size()) {
            const Officer& o = staff.officers[static_cast<std::size_t>(fl.officer)];
            chatter = o.Quirk("radio_chatter");
            leak = o.Quirk("comsec_leak");
        }
        const double discipline = RadioDiscipline(fr.discipline, 0.0, chatter, leak, td);
        const std::uint64_t fkey = static_cast<std::uint64_t>(p.id) * 64u + fi + 1;
        const bool demo = Demonstrates(p, fi, fr, staff, td, seed);
        const bool strike_like = fl.role == FlightRole::Strike || fl.role == FlightRole::Dead || fl.role == FlightRole::Recce || demo;
        const bool comm_out = !demo && strike_like && fr.discipline >= 0.5 && rng.Uniform(em::Stream::AirRadio, fkey, 11) < discipline;
        const std::uint64_t sq = AbstractKeyOf(fl.squadron);
        const bool have_quick = rng.Uniform(em::Stream::AirRadio, sq, 31) < fr.have_quick_share;
        const bool secure = rng.Uniform(em::Stream::AirRadio, sq, 32) < fr.secure_voice_share;
        const std::string codeword = detail_abstract::kCodewords[sq % 5];
        const int alive = std::max(0, fl.count - fl.lost);
        const bool no_aim = fl.aim_pos.x == 0.0 && fl.aim_pos.y == 0.0;
        const Vec3 aim = no_aim ? p.target_pos : fl.aim_pos;
        int made = 0;
        bool attacked = false, checked_in = false, off_target = false;
        const auto say = [&](const RoutePoint& at, CallKind kind, const std::string& words, bool has_where, const Vec3& where, bool by_signal) {
            if (made >= per_flight || index >= 255) return;
            AbstractCall a;
            Call& c = a.call;
            c.id = p.id * 256 + index;
            c.said_s = c.start_s = at.t_h * 3600.0;
            c.end_s = c.start_s + detail_abstract::CallLengthS(kind, td);
            c.kind = kind;
            c.speaker = fl.callsign + " 1";
            c.flight = fl.callsign;
            c.unit = fl.squadron.empty() ? fl.callsign : fl.squadron;
            c.callsign = CallsignAt(fl.callsign, at.t_h, rotation, seed);
            c.language = fr.language;
            c.words = words;
            c.has_where = has_where;
            c.where = where;
            c.have_quick = have_quick;
            c.secure = secure;
            c.by_signal = by_signal;
            c.truth = std::string(FlightRoleName(fl.role)) + (demo ? " (a demonstration)" : "") + " of package " + std::to_string(p.id);
            a.from = at.pos;
            a.height_m = std::max(0.0, at.agl_m);
            a.package_id = p.id;
            ++index;
            ++made;
            if (!by_signal) out.push_back(a);
        };
        // Routine calls: before the attack a comm-out flight says none; after it a strike keeps them off the air by its discipline.
        const auto routine_ok = [&](std::uint64_t salt) {
            if (!attacked) return !comm_out;
            if (!strike_like) return true;
            return rng.Uniform(em::Stream::AirRadio, fkey, salt, 21) >= discipline;
        };
        for (std::size_t k = 0; k < fl.route.size(); ++k) {
            const RoutePoint& rp = fl.route[k];
            const std::string& cs = fl.callsign;
            const bool release = static_cast<int>(k) == fl.release_index || (fl.release_index < 0 && rp.tag == "target");
            if (rp.tag == "entry" && routine_ok(100 + k)) {
                say(rp, CallKind::Admin, cs + ", " + std::to_string(fl.count) + " aircraft, airborne", false, Vec3(), false);
            } else if ((rp.tag == "gate" || rp.tag == "checkpoint") && !checked_in && !attacked) {
                checked_in = true;
                if (fl.aborted) {
                    say(rp, CallKind::Admin, cs + ", aborting, RTB", false, Vec3(), false);
                    break;
                }
                if (routine_ok(100 + k)) say(rp, CallKind::CheckIn, cs + " checking in, " + std::to_string(fl.count) + " " + fl.aircraft, false, Vec3(), false);
            } else if (rp.tag == "ip" && strike_like && !attacked) {
                // The act: a strike tells its package it is in, naming the target's square; a comm-out flight by signal.
                say(rp, CallKind::CheckIn, cs + ", IP inbound, target " + GridOf(aim), true, aim, comm_out);
            }
            if (release && !attacked) {
                attacked = true;
                if (fl.lost > 0 || fl.damaged > 0) say(rp, CallKind::ThreatCall, cs + ", " + cs + " hit, SAM", false, Vec3(), false);
                if (alive == 0) break;  // nobody left to speak
                continue;
            }
            if (attacked && !off_target && (rp.tag == "egress" || rp.tag == "climb" || rp.tag == "turn")) {
                off_target = true;
                if (comm_out && fl.released) {
                    say(rp, CallKind::Codeword, codeword, false, Vec3(), false);
                } else if (routine_ok(200 + k)) {
                    say(rp, CallKind::Admin, cs + ", off target", false, Vec3(), false);
                }
            }
            if (rp.tag == "exit" && alive > 0 && routine_ok(300 + k)) {
                say(rp, CallKind::Admin, cs + ", RTB, " + std::to_string(alive) + " aircraft", false, Vec3(), false);
            }
        }
    }
    return out;
}

// A message of the enemy's command (c2.hpp) about a package as a call on the air, when it is one (C2CallKind): said by a
// post at its place (an E-3A or A-50 at its orbit's height, a ground post at its mast) or by a flight of the package where
// its route had it then; requests and their answers on the HF request net. False if nobody would say it on the air.
inline bool AbstractC2Call(const C2Message& m, const Package* p, const AirC2& c2, const FactionRadio& fr, const AirRadioDef& td,
                           AbstractCall& out) {
    const std::string kind = C2CallKind(m.kind);
    CallKind ck = CallKind::Admin;
    if (kind.empty() || m.lost || !fr.present || !ParseCallKind(kind, ck)) return false;
    AbstractCall a;
    bool found = false;
    for (const C2PostState& ps : c2.Posts()) {
        if (ps.id != m.from) continue;
        a.from = ps.at;
        a.height_m = ps.kind == "airborne_warning" ? td.Tune("radio.awacs_height_m") : td.Tune("radio.abstract_mast_m");
        found = true;
    }
    if (!found && p != nullptr) {
        for (const Flight& f : p->flights) {
            if (f.callsign != m.from) continue;
            double agl = 0.0;
            a.from = detail_abstract::RouteAt(f, m.sent_h, &agl);
            a.height_m = agl;
            a.call.unit = f.squadron;
            found = true;
        }
    }
    if (!found) return false;  // "army", "silence": a land line, or nothing said
    a.hf = m.kind == C2Kind::Request || m.kind == C2Kind::Approval || m.kind == C2Kind::Refusal;
    Call& c = a.call;
    c.id = 100000000 + m.id;
    c.said_s = c.start_s = m.sent_h * 3600.0;
    c.end_s = c.start_s + detail_abstract::CallLengthS(ck, td);
    c.kind = ck;
    c.speaker = m.from;
    c.flight = m.to;
    if (c.unit.empty()) c.unit = m.from;
    c.callsign = m.from;
    c.language = fr.language;
    const bool place = (m.where.x != 0.0 || m.where.y != 0.0) &&
                       (ck == CallKind::Divert || ck == CallKind::Report || ck == CallKind::ThreatCall || ck == CallKind::Request);
    c.words = m.to + ", this is " + m.from + ", " + (m.text.empty() ? std::string(kind) : m.text) + (place ? ", square " + GridOf(m.where) : std::string());
    c.has_where = place;
    c.where = m.where;
    c.truth = std::string("c2 message ") + std::to_string(m.id) + (m.package_id >= 0 ? " of package " + std::to_string(m.package_id) : std::string());
    a.package_id = m.package_id;
    out = a;
    return true;
}

// Whether one of our receivers hears an abstract call, in closed form (section 2.4): an HF call within the HF reach, any
// other within the radio horizon between the two antennas (and never beyond `radio.abstract_range_m`), with the chance
// `radio.abstract_hear`; the bearing with the DF's error. Keyed on the seed, the call and the receiver.
inline bool HearAbstract(const AbstractCall& a, const ListeningPost& post, const AirRadioDef& td, std::uint64_t seed, Heard& out) {
    if (a.call.by_signal || a.hf != post.hf) return false;
    const double dx = a.from.x - post.at.x, dy = a.from.y - post.at.y;
    const double dist = std::hypot(dx, dy);
    const double reach =
        a.hf ? td.Tune("radio.abstract_hf_range_m") : std::min(RadioHorizonM(a.height_m, post.mast_m), td.Tune("radio.abstract_range_m"));
    if (dist > reach) return false;
    const em::Random rng(seed ^ 0x4EA2AB57ull);
    const std::uint64_t ck = static_cast<std::uint64_t>(a.call.id) + 1, pk = AbstractKeyOf(post.id);
    if (rng.Uniform(em::Stream::AirRadio, ck, pk, 1) >= td.Tune("radio.abstract_hear")) return false;
    Heard h;
    h.call = a.call.id;
    h.post = post.id;
    h.chain_post = post.chain_post;
    h.start_s = a.call.start_s;
    h.end_s = a.call.end_s;
    h.bearing_rad = std::atan2(dx, dy) + td.Tune("radio.df_sigma_deg") * (3.14159265358979323846 / 180.0) * rng.Normal(em::Stream::AirRadio, ck, pk, 2);
    h.line_of_sight = !a.hf;
    out = h;
    return true;
}

}  // namespace airwar
}  // namespace simcore
