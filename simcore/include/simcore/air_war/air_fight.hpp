// simcore/air_war/air_fight.hpp — the abstract air battle (roadmap track B, B8; docs/air_battle/07_AIR_TO_AIR.md sections 4,
// 5.5 and 5.6). It replaces the resolver's flat fighter roll (A0): once a package, when its first strike flight reaches the
// leg our fighters meet it on, our side's interceptors may find it and fight, and its sweep, CAP and close escort may meet
// them first. Every shot goes through B7's chain (air_combat.hpp): the pilot fires on what he believes, the shot is resolved
// on the truth with keyed draws.
//
//  - Ours: the front's fighter threat gives the chance a pair or a four is sent; our ground radar finds the package by
//    day, night, weather and height. A package with a jammer may jam our GCI: our green pilots go home and the surprise
//    is lost. The group's types are drawn from our regiments' strength (air_combat.json "ours"); a kill is written with
//    the regiment's id (LossRecord::by, `fighter`).
//  - Theirs: a sweep, then a CAP, then the close escort may meet our group first (an E-3A that controls fighters in the
//    war's year makes them likelier, and warns the strikers); a met group fights them and reaches the strikers only if
//    they are all down or broke off. The strikers answer by their doctrine (jettison, then press on or abort) and armed
//    ones defend themselves when they saw the attack.
//  - Rounds: the side with the first sight fires first; then both. Each shooter tries his weapons in his doctrine's order
//    at the round's range: a radar missile beyond sight only where his rules clear it (the controller's word, or NATO's
//    E-3A), else at the range a pilot identifies a fighter by eye.
//
// The planner's expectation of it (ExpectedMainLoss) is the same model in closed form.
//
// Roadmap S3 (tuning air_fight.mode 1; 0 replays a save made before it): the doctrines' attacks told apart. A beam attack's
// shots are taken from the target's beam; a slash, by the side that fired first, is one pass, and after it only
// a0_slash_return of the slashers come back for another; a turning doctrine on either side keeps the fight going
// a0_turn_rounds more rounds. A fighter shot at with a radar round from beyond sight drags (turns tail and runs it out of
// energy) at a0_drag of his skill. Strikers that see the shot answer it with flares or chaff if their type carries them in
// the war's year (data/air_war/countermeasures.json), as strong as gameplay/reactions.json's flare_decoy and chaff_factor.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/air_combat.hpp"
#include "simcore/air_war/data.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/air_war/types.hpp"

namespace simcore {
namespace airwar {

// Does the faction's airborne warning post control fighters in the war's year? (NATO's E-3A from 1982; the Pact's A-50
// only from 1989, docs/air_battle/07_AIR_TO_AIR.md section 2.7.) Without it an AWACS orbit warns no fighter.
inline bool AwacsControlsFighters(const AirWarData& d) {
    for (const C2PostDef& p : d.faction.c2.posts) {
        if (p.kind == "airborne_warning" && (p.from_year <= 0 || p.from_year <= d.theatre.year)) return true;
    }
    return false;
}

// What the fight knows of the package's day.
struct AbstractFightSetup {
    double friendly_fighters = 0.0;  // the front's fighter threat, 0..1
    double awacs = 0.0;              // the share of the day the enemy's AWACS is up, 0 where it controls no fighters
    bool night = false;
    bool poor_weather = false;
};

struct AbstractFightResult {
    bool attempted = false;  // our fighters were sent
    bool found = false;      // ... and found the package
    bool met_cover = false;  // its fighters met ours first
    bool reached = false;    // ours reached its strikers
    int ours_up = 0;
    int shots = 0;
    struct Loss {
        int flight = -1;
        std::string by;  // our regiment
    };
    std::vector<Loss> enemy_lost;        // one per aircraft
    std::vector<std::string> ours_lost;  // one per our fighter: its regiment
    std::vector<int> jettisoned;         // strike flights that threw their stores off
    std::vector<int> aborted;            // strike flights that turned for home
    int attacked = 0;                    // strike flights shot at or that saw the attack (each drew its answer)
    int jettison_drawn = 0;              // of them, those that drew a jettison, aborted or not (the balance tool's count)
    int dragged = 0;                     // S3: radar rounds from beyond sight a fighter dragged
    int extended = 0;                    // S3: slashers who extended after their pass and did not come back
    int dispensed = 0;                   // S3: shots at strikers answered with flares or chaff
    std::vector<std::string> notes;
};

class AbstractAirFight {
public:
    explicit AbstractAirFight(const AirWarData& d) : d_(&d), ac_(&d.air_combat), f_(&d.air_combat.fight) {
        ours_ = ac_->Doctrine("ours");
        theirs_ = ac_->Doctrine(d.faction.id);
    }

    // The setup of a package's day in the theatre: the front's fighters, the AWACS (only where it controls fighters).
    static AbstractFightSetup SetupFor(const AirWarData& d, const Package& p, double awacs_share, Weather w) {
        AbstractFightSetup s;
        const FrontDef* front = d.theatre.FrontFor(d.faction.front);
        s.friendly_fighters = front != nullptr ? front->friendly_fighters : 0.0;
        s.awacs = AwacsControlsFighters(d) ? std::clamp(awacs_share, 0.0, 1.0) : 0.0;
        s.night = p.part == DayPart::Night;
        s.poor_weather = w == Weather::Poor;
        return s;
    }

    // The chance our fighters are sent and find the package.
    double FindChance(const Package& p, const AbstractFightSetup& s) const {
        double det = s.night ? f_->a0_detect_night : f_->a0_detect_day;
        if (s.poor_weather) det *= f_->a0_detect_poor;
        if (p.profile == Profile::Low) det *= f_->a0_detect_low;
        return std::clamp(s.friendly_fighters * f_->a0_intercept, 0.0, 1.0) * std::clamp(det, 0.0, 1.0);
    }
    // The chance a fighter element of this role meets our group before the strikers.
    double MeetChance(FlightRole r, const AbstractFightSetup& s) const {
        double m = r == FlightRole::Sweep ? f_->a0_meet_sweep : r == FlightRole::Cap ? f_->a0_meet_cap : r == FlightRole::Escort ? f_->a0_meet_close : 0.0;
        const double meet = d_->tuning.Get("balance.mode") >= 1.0 ? f_->a0_awacs_meet : f_->a0_awacs_meet_mode0;
        m *= 1.0 + (meet - 1.0) * s.awacs;
        return std::clamp(m, 0.0, 1.0);
    }
    // The chance no fighter of the package meets our group.
    double UncoveredChance(const Package& p, const AbstractFightSetup& s) const {
        double u = 1.0;
        for (const Flight& fl : p.flights) {
            if (IsFighterCover(fl.role) && Armed(fl.aircraft, false)) u *= 1.0 - MeetChance(fl.role, s);
        }
        return u;
    }

    // The planner's expectation of the strike aircraft lost to our fighters: the chance they find it and nothing meets
    // them, times what a group of our mean size kills in the rounds with its nominal shot.
    double ExpectedMainLoss(const Package& p, const AbstractFightSetup& s) const {
        const int main = p.Main();
        if (main <= 0) return 0.0;
        const double kills = 3.0 * f_->a0_rounds * NominalKill();
        return FindChance(p, s) * UncoveredChance(p, s) * std::min(static_cast<double>(main), kills);
    }

    // The fight over one package. `alive[fi]`: each flight's aircraft still flying (the resolver's count at the fight).
    AbstractFightResult Resolve(const Package& p, const std::vector<int>& alive, const AbstractFightSetup& s, std::uint64_t seed) const {
        AbstractFightResult out;
        Rng r(seed ^ 0xB8F16A0000000001ull);
        if (ours_ == nullptr || ac_->ours.regiments.empty()) return out;
        // Sent, and found.
        double det = s.night ? f_->a0_detect_night : f_->a0_detect_day;
        if (s.poor_weather) det *= f_->a0_detect_poor;
        if (p.profile == Profile::Low) det *= f_->a0_detect_low;
        out.attempted = r.Chance(std::clamp(s.friendly_fighters * f_->a0_intercept, 0.0, 1.0));
        if (!out.attempted) return out;
        out.found = r.Chance(std::clamp(det, 0.0, 1.0));
        if (!out.found) {
            out.notes.push_back("our fighters were sent and did not find it");
            return out;
        }
        // Our group: one regiment's pair or four.
        const int n = r.Chance(0.5) ? 4 : 2;
        std::vector<double> rw;
        for (const OurRegimentDef& rg : ac_->ours.regiments) {
            double t = 0.0;
            for (const auto& ty : rg.types) t += ty.second;
            rw.push_back(t);
        }
        const OurRegimentDef& rg = ac_->ours.regiments[static_cast<std::size_t>(r.Weighted(rw))];
        std::vector<double> tw;
        for (const auto& ty : rg.types) tw.push_back(ty.second);
        std::vector<Fighter> ours;
        for (int i = 0; i < n; ++i) {
            Fighter x;
            x.type = rg.types[static_cast<std::size_t>(r.Weighted(tw))].first;
            x.regiment = rg.id;
            Arm(x, true);
            ours.push_back(x);
        }
        // Our GCI jammed: a green pilot who loses his controller does as his doctrine says (goes home), and the surprise
        // is gone.
        const bool jammed = p.Has(FlightRole::Jammer) && r.Chance(f_->a0_gci_jammed);
        if (jammed) {
            for (Fighter& x : ours) {
                if (ours_->OnControlLost(ours_->skill) == "abort" && r.Chance(f_->a0_green_home)) x.alive = false;
            }
            out.notes.push_back("our GCI was jammed");
        }
        out.ours_up = Count(ours);
        if (out.ours_up == 0) {
            out.notes.push_back("our fighters lost their controller and went home");
            return out;
        }
        bool surprise = r.Chance(jammed ? f_->a0_surprise_jammed : f_->a0_surprise_gci);
        const double seen = s.awacs > 0.0 ? f_->a0_seen_eye + (f_->a0_seen_awacs - f_->a0_seen_eye) * s.awacs : f_->a0_seen_eye;
        // An E-3A that controls the package's fighters sees our GCI's pass coming and warns them: the surprise is gone as
        // often as its warning is heard (balance.mode 1, roadmap B14; Teaball's warning turned 0.47:1 in May 1972 into
        // 3.8:1 by October, checo_1978_linebacker). Before B14 our first pass from the rear came as often under an E-3A.
        if (surprise && s.awacs > 0.0 && d_->tuning.Get("balance.mode") >= 1.0 && r.Chance(s.awacs * f_->a0_seen_awacs)) surprise = false;
        // Teaball's relay (roadmap S6, docs/air_battle/09 section 6): where the faction has one, its listeners overhear our
        // GCI's vectors and warn the package about a minute later, unless the relay fails; the surprise is gone. Off in the
        // data (no Teaball-type service in Europe by 1985 was found); its own draw, so the fight's stream is unchanged.
        if (surprise && !jammed && TeaballWarns(seed)) {
            surprise = false;
            out.notes.push_back("the package was warned of our fighters by its listeners' relay");
        }
        const bool ours_cleared = ours_->roe == "cleared_bvr" || (ours_->roe == "controller" && !jammed);
        std::uint64_t shooter_key = 0;
        // Their fighters: a sweep first, then a CAP, then the close escort.
        for (FlightRole role : {FlightRole::Sweep, FlightRole::Cap, FlightRole::Escort}) {
            if (Count(ours) == 0) break;
            for (std::size_t fi = 0; fi < p.flights.size(); ++fi) {
                const Flight& fl = p.flights[fi];
                if (fl.role != role || fi >= alive.size() || alive[fi] <= 0 || !Armed(fl.aircraft, false)) continue;
                if (!r.Chance(MeetChance(role, s))) continue;
                out.met_cover = true;
                std::vector<Fighter> them;
                for (int k = 0; k < alive[fi]; ++k) {
                    Fighter x;
                    x.type = fl.aircraft;
                    x.flight = static_cast<int>(fi);
                    Arm(x, false);
                    them.push_back(x);
                }
                // A leader who commits himself needs the odds (NATO 0.5); a controller commits his.
                if (theirs_ != nullptr && theirs_->commit_odds > 0.0 && static_cast<double>(them.size()) / std::max(1, Count(ours)) < theirs_->commit_odds) {
                    out.notes.push_back(std::string(FlightRoleName(role)) + " " + fl.callsign + " did not commit");
                    continue;
                }
                const bool them_first = r.Chance(seen) && !surprise;
                const bool them_cleared = theirs_ != nullptr &&
                                          (theirs_->roe == "cleared_bvr" || (theirs_->roe == "vid" && r.Chance(s.awacs)) ||
                                           (theirs_->roe == "controller" && r.Chance(f_->a0_surprise_gci)));
                Fight(ours, them, ours_cleared, them_cleared, surprise ? 1 : (them_first ? -1 : 0), false, seed, shooter_key, r, out);
                for (const Fighter& x : them) {
                    if (!x.alive && x.down) out.enemy_lost.push_back({x.flight, x.killed_by});
                }
                for (const Fighter& x : ours) {
                    if (!x.alive && x.down && !x.counted) out.ours_lost.push_back(x.regiment);
                }
                for (Fighter& x : ours) x.counted = x.counted || x.down;
                out.notes.push_back(std::string("our fighters met its ") + FlightRoleName(role) + " " + fl.callsign);
                if (Count(them) > 0) {
                    // Tied up: the strikers go on unmolested.
                    return out;
                }
            }
        }
        if (Count(ours) == 0 || !AnyWeapon(ours)) return out;
        // The strikers.
        std::vector<Fighter> strikers;
        for (std::size_t fi = 0; fi < p.flights.size(); ++fi) {
            const Flight& fl = p.flights[fi];
            if (!IsMain(fl.role) || fi >= alive.size()) continue;
            for (int k = 0; k < alive[fi]; ++k) {
                Fighter x;
                x.type = fl.aircraft;
                x.flight = static_cast<int>(fi);
                x.striker = true;
                Arm(x, false);
                const AircraftType* a = d_->faction.Aircraft(fl.aircraft);
                x.low = fl.profile == Profile::Low || (a != nullptr && a->helicopter);
                strikers.push_back(x);
            }
        }
        if (strikers.empty()) return out;
        out.reached = true;
        const bool aware = r.Chance(seen);
        Fight(ours, strikers, ours_cleared, false, surprise || !aware ? 1 : 0, true, seed, shooter_key, r, out);
        for (const Fighter& x : strikers) {
            if (!x.alive && x.down) out.enemy_lost.push_back({x.flight, x.killed_by});
        }
        for (const Fighter& x : ours) {
            if (!x.alive && x.down && !x.counted) out.ours_lost.push_back(x.regiment);
        }
        // The strikers' answer, flight by flight (04_REACTIONS.md section 8): those shot at or that saw the attack.
        if (theirs_ != nullptr) {
            for (std::size_t fi = 0; fi < p.flights.size(); ++fi) {
                if (!IsMain(p.flights[fi].role) || fi >= alive.size() || alive[fi] <= 0) continue;
                bool attacked = aware;
                int left = 0;
                for (const Fighter& x : strikers) {
                    if (x.flight != static_cast<int>(fi)) continue;
                    attacked = attacked || x.shot_at;
                    left += x.alive ? 1 : 0;
                }
                if (!attacked || left == 0) continue;
                ++out.attacked;
                const bool jett = r.Chance(theirs_->strikers_jettison);
                out.jettison_drawn += jett ? 1 : 0;
                // The doctrine's abort is a share of the attacked flights (04_REACTIONS.md section 8, as the session's
                // DecideFlight reads it); the rest press on or go round, which in an abstract fight is to go on. Before
                // B14 (balance.mode 0) it was the abort's share of abort and press on alone, which turned NATO home 36%
                // of the time against the F-105s' 3 of 16 (red_baron_vol2).
                const double ab = theirs_->strikers_abort + theirs_->strikers_press_on;
                const bool abort = d_->tuning.Get("balance.mode") >= 1.0 ? r.Chance(theirs_->strikers_abort)
                                                                          : ab > 0.0 && r.Chance(theirs_->strikers_abort / ab);
                if (abort) {
                    out.aborted.push_back(static_cast<int>(fi));
                } else if (jett) {
                    out.jettisoned.push_back(static_cast<int>(fi));
                }
            }
        }
        return out;
    }

    // The kill chance of our nominal shot (the planner's): the mean over our regiments' types of their best weapon at a
    // rear-hemisphere pass at the infrared range, by a pilot of our skill; a gun where no missile is carried.
    double NominalKill() const {
        if (ours_ == nullptr) return 0.0;
        double sum = 0.0, w = 0.0;
        for (const OurRegimentDef& rg : ac_->ours.regiments) {
            for (const auto& ty : rg.types) {
                const LoadOut lo = ac_->LoadOutFor(ty.first, d_->theatre.year, true);
                double best = 0.0;
                Pilot pl;
                pl.skill = ours_->skill;
                pl.stress = 0.3;
                ShotGeometry g;
                g.range_m = f_->a0_ir_m;
                g.aspect_rad = 0.25 * chain_detail::kPi;
                g.off_boresight_rad = 0.1;
                g.altitude_m = 3000.0;
                g.launch_g = 2.0;
                for (const auto& m : lo.aams) {
                    const double k = ShotOdds(*m.first, ac_->calibration.chain, g, ShotConditions(), pl).Product() *
                                     EnvelopeProbability(*m.first, ac_->calibration.chain, g, pl);
                    best = std::max(best, k);
                }
                if (lo.gun != nullptr) {
                    GunPass gp;
                    gp.range_m = f_->a0_gun_m;
                    gp.window_s = 2.0;
                    best = std::max(best, GunKillChance(*lo.gun, ac_->calibration.chain, gp, pl));
                }
                sum += best * ty.second;
                w += ty.second;
            }
        }
        return w > 0.0 ? sum / w : 0.0;
    }

    static bool IsMain(FlightRole r) { return r == FlightRole::Strike || r == FlightRole::Dead || r == FlightRole::Helicopter; }
    // Roadmap S3's fight (tuning air_fight.mode 1).
    bool S3() const { return d_->tuning.Get("air_fight.mode") >= 1.0; }
    static bool IsFighterCover(FlightRole r) { return r == FlightRole::Escort || r == FlightRole::Sweep || r == FlightRole::Cap; }

    // Whether the faction's Teaball-type relay warns a package of our fighters (radio.mode 1 and the faction's relay on):
    // keyed on the fight's seed, its own stream.
    bool TeaballWarns(std::uint64_t seed) const {
        const FactionRadio::Teaball& tb = d_->faction.radio.teaball;
        if (!tb.on || d_->tuning.Get("radio.mode") < 1.0) return false;
        Rng tr(seed ^ 0x7EAB0A1100000001ull);
        return tr.Chance(1.0 - tb.fail);
    }

private:
    struct Fighter {
        std::string type;
        std::string regiment;  // ours
        int flight = -1;       // theirs: the package's flight
        bool alive = true;
        bool down = false;     // shot down (not gone home)
        bool counted = false;
        bool striker = false;
        bool low = false;
        bool shot_at = false;
        std::string killed_by;
        std::vector<std::pair<const AamDef*, int>> aams;
        const AirGunDef* gun = nullptr;
    };

    bool Armed(const std::string& type, bool allied) const { return !ac_->LoadOutFor(type, d_->theatre.year, allied).Empty(); }
    void Arm(Fighter& x, bool allied) const {
        const LoadOut lo = ac_->LoadOutFor(x.type, d_->theatre.year, allied);
        x.aams = lo.aams;
        x.gun = lo.gun;
    }
    static int Count(const std::vector<Fighter>& v) {
        int n = 0;
        for (const Fighter& x : v) n += x.alive ? 1 : 0;
        return n;
    }
    static bool AnyWeapon(const std::vector<Fighter>& v) {
        for (const Fighter& x : v) {
            if (!x.alive) continue;
            if (x.gun != nullptr) return true;
            for (const auto& m : x.aams) {
                if (m.second > 0) return true;
            }
        }
        return false;
    }

    // One shooter's shot this round, or none: his weapons in his doctrine's order at the round's range.
    // `target_skill`: the target's doctrine's (S3: a fighter's drag).
    bool TakeShot(Fighter& s, const Fighter& t, const AirDoctrineDef& doc, int round, bool cleared, bool rear, bool vs_strikers, std::uint64_t seed,
                  std::uint64_t shooter, Rng& r, bool& kill, double target_skill, AbstractFightResult& out) const {
        const bool s3 = S3();
        const ChainParams& c = ac_->calibration.chain;
        Pilot pl;
        pl.skill = doc.skill;
        pl.stress = round == 1 ? 0.3 : 0.5;
        const double alt = vs_strikers && t.low ? 1000.0 : 5000.0;
        for (std::size_t wi = 0; wi < doc.shot_order.size(); ++wi) {
            const std::string& w = doc.shot_order[wi];
            const std::uint64_t shot = static_cast<std::uint64_t>(round) * 16u + wi;
            if (w == "gun") {
                if (s.gun == nullptr || (round == 1 && !s.aams.empty())) continue;
                GunPass gp;
                gp.range_m = f_->a0_gun_m * std::exp(f_->a0_range_sigma * r.Normal());
                gp.window_s = r.Uniform(1.0, 3.0) * (rear ? 1.5 : 1.0);
                gp.target_g = t.striker && !rear ? 2.0 : f_->break_g;
                if (gp.range_m > s.gun->effective_m) continue;  // he judges it too far and closes
                kill = ResolveGunPass(*s.gun, c, gp, pl, seed, shooter, shot).kill;
                return true;
            }
            const bool sarh = w == "sarh";
            for (auto& m : s.aams) {
                if (m.second <= 0 || (sarh ? !IsSemiActive(m.first->guidance) : !IsInfrared(m.first->guidance))) continue;
                ShotGeometry g;
                const bool bvr = sarh && round == 1 && cleared;
                const double base = sarh ? (bvr ? f_->a0_sarh_bvr_m : f_->a0_sarh_vid_m) : f_->a0_ir_m;
                g.range_m = base * std::exp(f_->a0_range_sigma * r.Normal());
                const double pi = chain_detail::kPi;
                if (bvr) {
                    g.aspect_rad = pi - r.Uniform() * pi / 4.0;
                } else if (rear) {
                    g.aspect_rad = r.Chance(0.75) ? r.Uniform() * pi / 2.0 : pi / 2.0 + r.Uniform() * pi / 2.0;
                } else if (s3 && doc.tactic == "beam") {
                    g.aspect_rad = pi / 2.0 + (r.Uniform() - 0.5) * pi / 3.0;  // set up on the target's beam
                } else {
                    g.aspect_rad = r.Uniform() * pi;
                }
                g.off_boresight_rad = std::fabs(r.Normal()) * 0.15;
                g.altitude_m = alt;
                g.launch_g = bvr ? 1.5 : std::min(7.0, 1.0 - std::log(std::max(1e-9, r.Uniform())));
                g.look_down = vs_strikers && t.low;
                ShotConditions k;
                bool saw = false;
                if (!t.striker || r.Chance(0.5)) {  // a fighter sees the shot and breaks; a striker half the time
                    saw = true;
                    k.break_timing = r.Uniform(0.4, 1.0);
                    k.target_g = t.striker ? std::min(f_->break_g, 5.0) : f_->break_g;
                }
                const ShotGeometry believed = Believe(g, c, pl, m.first->guidance, seed, shooter, shot);
                if (!PilotFires(*m.first, c, believed)) continue;
                --m.second;
                // S3: a fighter drags a radar round from beyond sight; a striker that saw the shot dispenses.
                if (s3 && bvr && !t.striker && r.Chance(std::clamp(f_->a0_drag * target_skill, 0.0, 1.0))) {
                    ++out.dragged;
                    kill = false;
                    return true;
                }
                if (s3 && saw && t.striker) {
                    const Loadout lo = d_->reactions.LoadoutOf(t.type, d_->theatre.year);
                    if (IsInfrared(m.first->guidance) && lo.flares > 0) {
                        k.countermeasures = d_->reactions.flare_decoy;
                        ++out.dispensed;
                    } else if (IsSemiActive(m.first->guidance) && lo.chaff > 0) {
                        k.countermeasures = 1.0 - d_->reactions.chaff_factor;
                        ++out.dispensed;
                    }
                }
                kill = ResolveShot(*m.first, c, g, k, pl, seed, shooter, shot).kill;
                return true;
            }
        }
        return false;
    }

    // Rounds between our group and theirs. `initiative`: 1 ours fire first in the first round, -1 theirs, 0 together.
    // `vs_strikers`: theirs are strikers (defend themselves with infrared missiles and guns, not in the first round
    // unless they saw ours first).
    void Fight(std::vector<Fighter>& ours, std::vector<Fighter>& them, bool ours_cleared, bool them_cleared, int initiative, bool vs_strikers,
               std::uint64_t seed, std::uint64_t& shooter_key, Rng& r, AbstractFightResult& out) const {
        const AirDoctrineDef* td = theirs_;
        int rounds = std::max(1, static_cast<int>(std::lround(f_->a0_rounds)));
        const bool s3 = S3();
        // S3: a turning doctrine on either side keeps a fight between fighters going.
        if (s3 && !vs_strikers && td != nullptr && (ours_->tactic == "turn" || td->tactic == "turn")) {
            rounds += std::max(0, static_cast<int>(std::lround(f_->a0_turn_rounds)));
        }
        for (int round = 1; round <= rounds; ++round) {
            if (Count(ours) == 0 || Count(them) == 0) break;
            const auto volley = [&](std::vector<Fighter>& a, std::vector<Fighter>& b, const AirDoctrineDef& doc, bool cleared, bool rear, bool strikers_fire,
                                    bool b_strikers) {
                std::vector<std::pair<std::size_t, std::size_t>> hits;  // (shooter, target)
                for (std::size_t i = 0; i < a.size(); ++i) {
                    if (!a[i].alive || Count(b) == 0) continue;
                    if (strikers_fire && a[i].striker && a[i].aams.empty() && a[i].gun == nullptr) continue;
                    std::vector<std::size_t> live;
                    for (std::size_t j = 0; j < b.size(); ++j) {
                        if (b[j].alive) live.push_back(j);
                    }
                    const std::size_t j = live[static_cast<std::size_t>(r.Index(static_cast<int>(live.size())))];
                    bool kill = false;
                    const std::uint64_t key = 0xB8000000ull + (++shooter_key);
                    const AirDoctrineDef* bdoc = &doc == ours_ ? td : ours_;
                    if (!TakeShot(a[i], b[j], doc, round, cleared, rear, b_strikers, seed, key, r, kill, bdoc != nullptr ? bdoc->skill : 0.5, out)) {
                        continue;
                    }
                    ++out.shots;
                    b[j].shot_at = true;
                    if (kill) hits.push_back({i, j});
                }
                return hits;
            };
            const auto apply = [](std::vector<Fighter>& a, std::vector<Fighter>& b, const std::vector<std::pair<std::size_t, std::size_t>>& hits) {
                for (const auto& h : hits) {
                    if (!b[h.second].alive) continue;
                    b[h.second].alive = false;
                    b[h.second].down = true;
                    b[h.second].killed_by = a[h.first].regiment;
                }
            };
            const bool them_fire = td != nullptr && (!vs_strikers || round > 1 || initiative <= 0);
            const bool ours_rear = initiative > 0 && round == 1;
            const bool them_rear = initiative < 0 && round == 1;
            if (round == 1 && initiative != 0) {
                if (initiative > 0) {
                    apply(ours, them, volley(ours, them, *ours_, ours_cleared, ours_rear, false, vs_strikers));
                    if (them_fire) apply(them, ours, volley(them, ours, *td, them_cleared, them_rear, vs_strikers, false));
                } else {
                    if (them_fire) apply(them, ours, volley(them, ours, *td, them_cleared, them_rear, vs_strikers, false));
                    apply(ours, them, volley(ours, them, *ours_, ours_cleared, ours_rear, false, vs_strikers));
                }
            } else {
                const auto h1 = volley(ours, them, *ours_, ours_cleared && round == 1, false, false, vs_strikers);
                std::vector<std::pair<std::size_t, std::size_t>> h2;
                if (them_fire) h2 = volley(them, ours, *td, them_cleared && round == 1, false, vs_strikers, false);
                apply(ours, them, h1);
                apply(them, ours, h2);
            }
            if (!AnyWeapon(ours)) break;
            // S3: a slash is one pass. After the first round the side that fired first, if its doctrine slashes, extends
            // away; only a0_slash_return of it comes back (the rest leave the fight, not shot down).
            if (s3 && round == 1 && !vs_strikers && initiative != 0 && td != nullptr) {
                std::vector<Fighter>& side = initiative > 0 ? ours : them;
                const AirDoctrineDef& sd = initiative > 0 ? *ours_ : *td;
                if (sd.tactic == "slash") {
                    for (Fighter& x : side) {
                        if (!x.alive || r.Chance(f_->a0_slash_return)) continue;
                        x.alive = false;
                        ++out.extended;
                    }
                }
            }
        }
    }

    const AirWarData* d_;
    const AirCombatDef* ac_;
    const FightParams* f_;
    const AirDoctrineDef* ours_ = nullptr;
    const AirDoctrineDef* theirs_ = nullptr;
};

}  // namespace airwar
}  // namespace simcore
