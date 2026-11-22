// simcore/air_war/comms_jamming.hpp — the enemy's jamming of our VHF voice nets around a package (roadmap 16.2, first
// form; docs/research/VOICE_JAMMING_2026-10.md).
//
// The period's voice jamming came from the ground near the front and from helicopters orbiting behind it (a NATO corps'
// jammers and its divisions' Quick Fix; the Pact army's jamming battalion), not from the strike packages. Each of the
// faction's jammers in service in the war's year stands behind its front line where the line is nearest our battery, and
// is on the air with a package of its kinds from the package's take-off to its last aircraft out (the Bekaa, 1982: from
// the strike's take-off), off a few minutes before the time on target when the package fires anti-radiation missiles
// (paprcka_1984).
//
// How hard it jams one of our receivers is FM 34-40-7's rule (fm_34_40_7): the jamming-to-signal ratio
// J/S = (Pj / Pt) (Dt / Dj)^n, Pj and Pt the jammer's and our transmitter's power, Dj and Dt their distances to the
// receiver, n the ground's exponent (2 flat or water, 3 rolling farmland, 4 rough or wooded, 5 mountains). At J/S >= K (2
// for FM) the jammer captures the receiver: strong, the net useless for traffic. From weak_js (a SAM3D threshold) words
// are lost and repeats asked for: weak. A barrage jammer spreads its power over its band, one 25 kHz channel's share on
// ours. Beyond the range its side gives, nothing. The net is as jammed as its worst receiver (the one nearest the jammer
// and farthest from its transmitter is jammed first).
// Left: each receiver jammed on its own, the ground's exponent from the map, the regiment's and the warning nets.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/radio_nets.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace airwar {

// FM 34-40-7's ratio at one receiver.
inline double JammingToSignal(double jammer_w, double own_w, double jammer_m, double own_m, double n) {
    if (jammer_w <= 0.0 || own_w <= 0.0 || jammer_m <= 0.0 || own_m <= 0.0) return 0.0;
    return jammer_w / own_w * std::pow(own_m / jammer_m, n);
}
// The farthest a jammer can stand from a receiver and still reach J/S = `js` there (its worked examples: 1.5 kW against a
// 5 W set 9 km away, n = 4, captures from 31.5 km).
inline double JammerReach(double jammer_w, double own_w, double own_m, double n, double js) {
    if (jammer_w <= 0.0 || own_w <= 0.0 || js <= 0.0) return 0.0;
    return own_m * std::pow(jammer_w / (js * own_w), 1.0 / n);
}

// The nearest point of a front line (a polyline) to `at`.
inline Vec3 NearestOnLine(const std::vector<Vec3>& line, const Vec3& at) {
    Vec3 best = line.empty() ? at : line.front();
    double best_d = 1e300;
    for (std::size_t i = 0; i + 1 < line.size(); ++i) {
        const Vec3 a{line[i].x, line[i].y, 0.0}, b{line[i + 1].x, line[i + 1].y, 0.0};
        const Vec3 ab = b - a;
        const double len2 = Dot(ab, ab);
        const double u = len2 > 0.0 ? std::max(0.0, std::min(1.0, Dot(Vec3{at.x, at.y, 0.0} - a, ab) / len2)) : 0.0;
        const Vec3 p = a + ab * u;
        const double dd = (p - Vec3{at.x, at.y, 0.0}).HorizontalLength();
        if (dd < best_d) {
            best_d = dd;
            best = p;
        }
    }
    return best;
}

// One jammer against the battery net, as a package brings it.
struct VoiceJammer {
    std::string id;
    std::string name;
    Vec3 at;              // theatre ENU
    int strength = 0;     // 0 none, 1 weak, 2 strong
    double distance_m = 0.0;  // to the worst receiver
    double js = 0.0;          // there (0: its power is not known)
    std::string why;          // why it reaches or not
};

// The battery as the jammers meet it: where it stands in the theatre's frame, its main station (the post) and its
// receivers (the launchers) relative to that, our sets' power, the battery net's frequencies.
struct JammedBattery {
    Vec3 centre;                   // theatre ENU
    Vec3 post;                     // relative to the centre
    std::vector<Vec3> receivers;   // relative to the centre
    std::vector<double> net_mhz;   // the battery net's frequencies
};

// The faction's jammers against the battery, by the package's kind and the war's year: where each stands, and how hard it
// jams the battery net's worst receiver (both ways: the launchers hearing the post, the post hearing the launchers).
inline std::vector<VoiceJammer> VoiceJammersFor(const AirWarData& d, const Package& p, const JammedBattery& b) {
    std::vector<VoiceJammer> out;
    const FrontDef* front = nullptr;
    for (const FrontDef& f : d.theatre.fronts) front = f.faction == d.faction.front ? &f : front;
    if (front == nullptr || front->line.empty()) return out;
    const TuningTable& t = d.tuning;
    const double k = t.Get("comms_jam.capture_k"), weak = t.Get("comms_jam.weak_js"), n = t.Get("comms_jam.terrain_n");
    const double own_w = t.Get("comms_jam.own_power_w");
    const Vec3 on_line = NearestOnLine(front->line, b.centre);
    const Vec3 toward = on_line - Vec3{b.centre.x, b.centre.y, 0.0};
    const double line_m = toward.HorizontalLength();
    const Vec3 across = line_m > 1.0 ? toward * (1.0 / line_m) : Vec3{};
    for (const CommsJammerDef& c : d.faction.comms_jamming) {
        if (c.in_service_year > d.theatre.year) continue;
        if (!c.with.empty() && std::find(c.with.begin(), c.with.end(), p.kind) == c.with.end()) continue;
        VoiceJammer v;
        v.id = c.id;
        v.name = c.name;
        v.at = on_line + across * (c.behind_km * 1000.0);
        bool in_band = false;
        for (double mhz : b.net_mhz) in_band = in_band || (mhz >= c.band_lo_mhz && mhz <= c.band_hi_mhz);
        // Each link both ways: the post's words at a launcher, a launcher's at the post.
        std::vector<std::pair<Vec3, double>> links;  // receiver (theatre), its transmitter's distance
        for (const Vec3& r : b.receivers) {
            const double dt = (r - b.post).HorizontalLength();
            links.push_back({b.centre + r, dt});
            links.push_back({b.centre + b.post, dt});
        }
        const double per_channel = c.barrage ? c.power_w * 0.025 / std::max(0.025, c.band_hi_mhz - c.band_lo_mhz) : c.power_w;
        double nearest = 1e300;
        for (const auto& l : links) {
            const double dj = (v.at - l.first).HorizontalLength();
            nearest = std::min(nearest, dj);
            if (dj > c.range_km * 1000.0) continue;
            const double js = JammingToSignal(per_channel, own_w, dj, std::max(l.second, 1.0), n);
            const int s = c.power_w <= 0.0 ? 1 : js >= k ? 2 : js >= weak ? 1 : 0;
            if (s > v.strength || (s == v.strength && js > v.js)) {
                v.strength = s;
                v.js = js;
                v.distance_m = dj;
            }
        }
        if (v.distance_m <= 0.0) v.distance_m = nearest;
        char buf[160];
        if (!in_band) {
            v.strength = 0;
            std::snprintf(buf, sizeof(buf), "the battery net's frequencies are outside its band (%.0f-%.0f MHz)", c.band_lo_mhz, c.band_hi_mhz);
        } else if (nearest > c.range_km * 1000.0) {
            std::snprintf(buf, sizeof(buf), "%.0f km off, beyond its %.0f km", nearest / 1000.0, c.range_km);
        } else if (c.power_w <= 0.0) {
            std::snprintf(buf, sizeof(buf), "%.0f km off, within its %.0f km; its power unknown: weak", v.distance_m / 1000.0, c.range_km);
        } else {
            std::snprintf(buf, sizeof(buf), "%.0f km off, J/S %.2f at the worst receiver: %s", v.distance_m / 1000.0, v.js,
                          v.strength == 2 ? "strong" : v.strength == 1 ? "weak" : "too weak to matter");
        }
        v.why = buf;
        out.push_back(v);
    }
    return out;
}

// The package's times in the session: from its take-off to its last aircraft out, with a gap before the time on target
// when it fires anti-radiation missiles. `h_at_zero`: the campaign's hour at the session's t = 0.
inline std::vector<std::pair<double, double>> VoiceJammingWindows(const AirWarData& d, const Package& p, double h_at_zero) {
    const double from = (p.FirstH() - h_at_zero) * 3600.0, to = (p.LastH() - h_at_zero) * 3600.0, tot = (p.tot_h - h_at_zero) * 3600.0;
    if (to <= from) return {};
    bool arms = false;
    for (const Flight& f : p.flights) arms = arms || f.arms > 0;
    const double ease = d.tuning.Get("comms_jam.ease_before_tot_s");
    if (!arms || tot - ease <= from || tot >= to) return {{from, to}};
    return {{from, tot - ease}, {tot, to}};
}

// The jamming the battery net gets from the package's jammers, as the radio nets take it.
inline std::vector<JammingDef> VoiceJammingDefs(const AirWarData& d, const Package& p, const std::vector<VoiceJammer>& jammers,
                                                double h_at_zero) {
    std::vector<JammingDef> out;
    for (const VoiceJammer& v : jammers) {
        if (v.strength == 0) continue;
        const CommsJammerDef* c = nullptr;
        for (const CommsJammerDef& e : d.faction.comms_jamming) c = e.id == v.id ? &e : c;
        if (c == nullptr) continue;
        for (const auto& w : VoiceJammingWindows(d, p, h_at_zero)) {
            JammingDef j;
            j.nets = {"battery"};
            j.band_lo_mhz = c->band_lo_mhz;
            j.band_hi_mhz = c->band_hi_mhz;
            j.from_s = w.first;
            j.to_s = w.second;
            j.strong = v.strength == 2;
            j.spot = !c->barrage;
            j.follow_s = c->follow_s;
            out.push_back(j);
        }
    }
    return out;
}

}  // namespace airwar
}  // namespace simcore
