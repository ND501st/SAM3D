// simcore/air_war/air_fight_session.hpp — the air battle in a running session (roadmap track B, B8;
// docs/air_battle/07_AIR_TO_AIR.md section 5): fighters that find, identify, close and fire, at gameplay level.
//
//  - What a fighter knows: its pilot's picture (B3's perception: what he saw, where he believes it is, whether he
//    recognised the type), its own radar (sensors.json's ranges: by hand, or cued by its controller; against the
//    ground below it the look-down figure; inside its scan's sector), and its controller's vectors. Every decision is
//    taken on these; the truth is read only to resolve what the sensors and the shot do (and to match a believed place
//    to the aircraft there for the act).
//  - Rules of engagement (07 section 4.1): a doctrine of `vid` fires only on a type recognised by eye inside the
//    identification range, or, under an E-3A that controls it, on a radar contact with no friendly near it; a
//    `controller` doctrine on what its controller called hostile; our side by its controller.
//  - The controller (B5's posts over B9's channels): a GCI post or an E-3A tracks within its range and vectors its own
//    fighters every gci_vector_s on its post's net. A vector lost to jamming, distance or a full set of channels is not
//    heard; a fighter that hears none for control_lost_s has lost control and does as his doctrine says for his skill
//    (abort: home; pairs: on with his own radar; continue).
//  - Commit and disengage: joker and bingo (seconds in the fight), Winchester (nothing left), and a leader who commits
//    himself (NATO) goes home when the odds he believes fall below the doctrine's.
//  - The shot is B7's chain: the pilot believes the geometry (his judgement), fires when his belief is in the zone; the
//    missile flies its time and is resolved at arrival on the truth at launch, with the target's break if its pilot knew
//    of the shot and a radar missile's lock held by the launcher to impact. A gun fires in its cone and range. Every
//    draw is keyed (em::DrawHash on Stream::AirCombat with this session's keys, 0xF1600000 and up for shooters).
//
// Kills go to the runner's destroy (the regiment's id for ours); claims are what the shooter believed.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "simcore/aircraft.hpp"
#include "simcore/air_war/air_combat.hpp"
#include "simcore/air_war/air_fight_def.hpp"
#include "simcore/air_war/perception.hpp"
#include "simcore/flights.hpp"

namespace simcore {
namespace airwar {

// What the session reads and does in its runner (ScenarioRunner fills it; a test can fill it by hand).
struct AirFightWorld {
    std::function<const Aircraft*(int target)> air;                       // alive in the session, or null
    std::function<const std::vector<Sighting>*(int target)> picture;      // the pilot's picture (null: none)
    std::function<double(const Vec3& p)> altitude;                        // height above sea level
    std::function<double(const Vec3& p)> agl;                             // height above the ground
    std::function<void(int target, const PilotOverride& ov)> steer;
    std::function<void(int target)> release;                              // back to its flight's orders
    std::function<bool(int target, const std::string& by)> destroy;
    std::function<void(const std::string& who, const std::string& what)> event;
    // A controller's vector on its post's net: the call's number (< 0: not sent); and when the fighter hears it (< 0: never).
    std::function<int(const std::string& post, int fighter_target, const std::string& words, double t)> say;
    std::function<double(int call, int fighter_target, double t)> heard;
};

class AirFightSession {
public:
    enum class Mode { Patrol, Vectored, Search, Identify, Engage, Home };
    static const char* ModeName(Mode m) {
        switch (m) {
            case Mode::Patrol: return "patrol";
            case Mode::Vectored: return "vectored";
            case Mode::Search: return "search";
            case Mode::Identify: return "identify";
            case Mode::Engage: return "engage";
            case Mode::Home: return "home";
        }
        return "?";
    }

    struct Kill {
        std::string target;
        std::string by;       // our regiment, or the enemy's type
        std::string shooter;  // the scenario target that fired
        std::string weapon;
        double t_s = 0.0;
    };
    struct Claim {
        std::string shooter;
        std::string target_believed;  // what he believed he shot at ("aircraft" if not recognised)
        bool real = false;            // a true kill behind it
        double t_s = 0.0;
    };
    struct ShotRecord {
        std::string shooter;
        std::string target;
        std::string weapon;
        double t_s = 0.0;
        bool kill = false;
        MissReason reason = MissReason::None;
    };

    // `ids`: the scenario's target ids by index. False, with why, if a fighter names no target or no doctrine.
    bool Init(const AirFightDef& def, const std::vector<std::string>& ids, std::uint64_t seed, std::string* error = nullptr) {
        def_ = &def;
        seed_ = seed;
        fighters_.clear();
        for (const AirFightFighterDef& fd : def.fighters) {
            Fighter f;
            f.def = &fd;
            f.target = -1;
            for (std::size_t i = 0; i < ids.size(); ++i) {
                if (ids[i] == fd.target) f.target = static_cast<int>(i);
            }
            f.doc = def.combat.Doctrine(fd.faction);
            if (f.target < 0 || f.doc == nullptr) {
                if (error != nullptr) *error = "air_fight: fighter '" + fd.target + "': " + (f.target < 0 ? "no such target" : "no doctrine for " + fd.faction);
                return false;
            }
            f.ours = fd.faction == "ours";
            f.skill = fd.skill >= 0.0 ? fd.skill : f.doc->skill;
            const LoadOut lo = def.combat.LoadOutFor(fd.type, def.year, f.ours);
            f.aams = lo.aams;
            f.gun = lo.gun;
            f.gun_s = lo.gun != nullptr ? lo.gun->firing_s : 0.0;
            const std::vector<const SensorDef*> radars = def.sensors.Of(fd.type, SensorKind::Radar, def.year, f.ours);
            f.radar = radars.empty() ? nullptr : radars.front();
            f.striker = fd.role == "strike";
            f.key = 0xF1600000ull + static_cast<std::uint64_t>(fighters_.size());
            fighters_.push_back(f);
        }
        return true;
    }

    void Step(double t, const AirFightWorld& w) {
        if (def_ == nullptr) return;
        const FightParams& fp = def_->combat.fight;
        if (start_s_ < 0.0) start_s_ = t;
        Controllers(t, w);
        Deliver(t);
        Missiles(t, w);
        for (std::size_t i = 0; i < fighters_.size(); ++i) {
            Fighter& f = fighters_[i];
            if (f.gone || t < f.next_think_s) continue;
            f.next_think_s = t + fp.think_s;
            const Aircraft* me = w.air ? w.air(f.target) : nullptr;
            if (me == nullptr) {
                f.gone = true;
                continue;
            }
            Think(f, *me, t, w);
        }
    }

    const std::vector<Kill>& Kills() const { return kills_; }
    const std::vector<Claim>& Claims() const { return claims_; }
    const std::vector<ShotRecord>& Shots() const { return shots_; }
    int VectorsSent() const { return vectors_sent_; }
    int VectorsHeard() const { return vectors_heard_; }

    // A fighter's state by its target id (tests, the debrief).
    Mode ModeOf(const std::string& target) const {
        const Fighter* f = Find(target);
        return f != nullptr ? f->mode : Mode::Patrol;
    }
    bool ControlLost(const std::string& target) const {
        const Fighter* f = Find(target);
        return f != nullptr && f->control_lost;
    }
    std::string WhyHome(const std::string& target) const {
        const Fighter* f = Find(target);
        return f != nullptr ? f->why_home : std::string();
    }
    // Is this target one of our fighters, and its regiment.
    bool IsOurs(const std::string& target) const {
        const Fighter* f = Find(target);
        return f != nullptr && f->ours;
    }
    std::string RegimentOf(const std::string& target) const {
        const Fighter* f = Find(target);
        return f != nullptr ? f->def->regiment : std::string();
    }
    int ShotsBy(const std::string& target) const {
        int n = 0;
        for (const ShotRecord& s : shots_) n += s.shooter == target ? 1 : 0;
        return n;
    }

private:
    struct Contact {
        int truth = -1;          // the aircraft behind it (for the act only: no decision reads it)
        Vec3 believed;
        Vec3 vel;
        double t_s = 0.0;
        bool hostile = false;    // identified hostile by the doctrine's rules
        bool friendly = false;   // recognised as one of ours
        std::string what = "aircraft";
    };
    struct Fighter {
        const AirFightFighterDef* def = nullptr;
        const AirDoctrineDef* doc = nullptr;
        int target = -1;
        bool ours = false;
        bool striker = false;
        double skill = 0.5;
        std::vector<std::pair<const AamDef*, int>> aams;
        const AirGunDef* gun = nullptr;
        double gun_s = 0.0;
        const SensorDef* radar = nullptr;
        std::uint64_t key = 0;
        Mode mode = Mode::Patrol;
        std::vector<Contact> contacts;
        double next_think_s = 0.0;
        double next_shot_s = 0.0;
        double last_vector_s = -1.0;
        double next_vector_due = 0.0;
        Vec3 vector_point;
        bool control_lost = false;
        bool free = false;        // lost control and goes on in pairs with his own radar
        bool steering = false;
        bool gone = false;
        std::string why_home;
        std::uint64_t shots = 0;
    };
    struct Missile {
        std::size_t shooter = 0;
        int target = -1;
        const AamDef* aam = nullptr;
        ShotGeometry geo;
        double fired_s = 0.0;
        double arrive_s = 0.0;
        std::uint64_t shot = 0;
        double lock_lost_s = -1.0;
        std::string believed;
        bool done = false;
    };
    struct Vector {
        std::size_t fighter = 0;
        double heard_s = -1.0;
        Vec3 point;
        Vec3 vel;
        int truth = -1;
        bool done = false;
    };

    const Fighter* Find(const std::string& target) const {
        for (const Fighter& f : fighters_) {
            if (f.def->target == target) return &f;
        }
        return nullptr;
    }
    bool Opposed(const Fighter& a, int target) const {
        for (const Fighter& f : fighters_) {
            if (f.target == target) return f.ours != a.ours;
        }
        return false;  // not a fighter of this battle: not his to fight
    }
    const Fighter* FighterAt(int target) const {
        for (const Fighter& f : fighters_) {
            if (f.target == target) return &f;
        }
        return nullptr;
    }
    double U(std::uint64_t who, std::uint64_t n, std::uint64_t link) const {
        return em::Uniform01(em::DrawHash(seed_, em::Stream::AirCombat, who, n, link));
    }
    double N(std::uint64_t who, std::uint64_t n, std::uint64_t link) const {
        return em::StandardNormal(em::DrawHash(seed_, em::Stream::AirCombat, who, n, link));
    }
    static double Angle(const Vec3& a, const Vec3& b) {
        const double la = a.Length(), lb = b.Length();
        if (la < 1e-9 || lb < 1e-9) return 0.0;
        return std::acos(std::clamp(Dot(a, b) / (la * lb), -1.0, 1.0));
    }

    // ---- the controllers: each tracks what is in its range and vectors its own fighters.

    void Controllers(double t, const AirFightWorld& w) {
        const FightParams& fp = def_->combat.fight;
        for (std::size_t ci = 0; ci < def_->controllers.size(); ++ci) {
            const AirFightControllerDef& c = def_->controllers[ci];
            for (std::size_t i = 0; i < fighters_.size(); ++i) {
                Fighter& f = fighters_[i];
                if (f.gone || f.mode == Mode::Home || f.def->controller != c.id || f.striker) continue;
                if (t < f.next_vector_due) continue;
                f.next_vector_due = t + fp.gci_vector_s;
                const Aircraft* me = w.air ? w.air(f.target) : nullptr;
                if (me == nullptr || (me->position - c.pos).Length() > c.range_m) continue;
                // The nearest of the other side the controller holds.
                int best = -1;
                double best_d = 1e18;
                Vec3 at, vel;
                for (const Fighter& o : fighters_) {
                    if (o.gone || o.ours == f.ours) continue;
                    const Aircraft* a = w.air(o.target);
                    if (a == nullptr || (a->position - c.pos).Length() > c.range_m) continue;
                    const double d = (a->position - me->position).Length();
                    if (d < best_d) {
                        best_d = d;
                        best = o.target;
                        at = a->position;
                        vel = a->velocity;
                    }
                }
                if (best < 0) continue;
                const std::uint64_t n = ++vector_count_;
                const Vec3 err{fp.gci_error_m * N(0xF16C0000ull + ci, n, 1), fp.gci_error_m * N(0xF16C0000ull + ci, n, 2), 0.0};
                Vector v;
                v.fighter = i;
                v.point = at + err;
                v.vel = vel;
                v.truth = best;
                const Vec3 d = v.point - me->position;
                const double brg = std::fmod(std::atan2(d.x, d.y) * 180.0 / chain_detail::kPi + 360.0, 360.0);
                const std::string words = f.def->target + ", vector " + std::to_string(static_cast<int>(std::lround(brg))) + ", " +
                                          std::to_string(static_cast<int>(std::lround(d.Length() / 1000.0))) + " km, height " +
                                          std::to_string(static_cast<int>(std::lround(at.z / 100.0) * 100));
                ++vectors_sent_;
                if (!c.post.empty() && w.say && w.heard) {
                    const int call = w.say(c.post, f.target, words, t);
                    v.heard_s = call >= 0 ? w.heard(call, f.target, t) : -1.0;
                } else {
                    v.heard_s = t;
                }
                if (w.event) w.event(c.id, words + (v.heard_s < 0.0 ? " (not heard)" : ""));
                if (v.heard_s >= 0.0) vectors_.push_back(v);
            }
        }
    }

    void Deliver(double t) {
        for (Vector& v : vectors_) {
            if (v.done || t < v.heard_s) continue;
            v.done = true;
            Fighter& f = fighters_[v.fighter];
            ++vectors_heard_;
            f.last_vector_s = t;
            f.vector_point = v.point;
            Contact c;
            c.truth = v.truth;
            c.believed = v.point;
            c.vel = v.vel;
            c.t_s = t;
            c.hostile = f.doc->roe == "controller" || f.ours;  // the controller's word identifies it
            Merge(f, c);
        }
        vectors_.erase(std::remove_if(vectors_.begin(), vectors_.end(), [](const Vector& v) { return v.done; }), vectors_.end());
    }

    static void Merge(Fighter& f, const Contact& c) {
        for (Contact& o : f.contacts) {
            if ((o.truth == c.truth && c.truth >= 0) || (o.believed - c.believed).Length() < 2000.0) {
                const bool h = o.hostile || c.hostile;
                const bool fr = o.friendly || c.friendly;
                const std::string what = c.what != "aircraft" ? c.what : o.what;
                o = c;
                o.hostile = h && !fr;
                o.friendly = fr;
                o.what = what;
                return;
            }
        }
        f.contacts.push_back(c);
    }

    // ---- one fighter's thought: what he knows, what he decides, where he flies, whether he fires.

    void Think(Fighter& f, const Aircraft& me, double t, const AirFightWorld& w) {
        const FightParams& fp = def_->combat.fight;
        if (f.mode == Mode::Home) return;
        Sense(f, me, t, w);
        f.contacts.erase(std::remove_if(f.contacts.begin(), f.contacts.end(), [&](const Contact& c) { return t - c.t_s > fp.contact_hold_s; }),
                         f.contacts.end());
        // A striker only defends himself: he fires at a hostile he knows of close in, and flies his own route.
        // Control: a fighter bound to his controller who hears no vector for control_lost_s.
        const AirFightControllerDef* ctl = def_->Controller(f.def->controller);
        const bool bound = ctl != nullptr && !f.striker && (f.doc->ControllerBound() || f.doc->roe == "controller");
        if (bound && !f.control_lost && !f.free) {
            const double since = f.last_vector_s >= 0.0 ? t - f.last_vector_s : t - start_s_;
            if (since > fp.control_lost_s) {
                f.control_lost = true;
                const std::string& what = f.doc->OnControlLost(f.skill);
                if (w.event) w.event(f.def->target, "has lost his controller: " + what);
                if (what == "abort") {
                    GoHome(f, me, t, w, "lost control");
                    return;
                }
                f.free = what == "pairs";
            }
        }
        // Joker, bingo, Winchester.
        const double in_fight = t - start_s_;
        if (!f.striker) {
            if (in_fight >= fp.bingo_s) return GoHome(f, me, t, w, "bingo");
            if (!Armed(f)) return GoHome(f, me, t, w, "winchester");
            if (in_fight >= fp.joker_s && f.mode != Mode::Engage) return GoHome(f, me, t, w, "joker");
        }
        // The best contact: the nearest hostile, else the nearest not yet identified.
        const Contact* hostile = nullptr;
        const Contact* unknown = nullptr;
        double dh = 1e18, du = 1e18;
        int hostiles = 0;
        for (const Contact& c : f.contacts) {
            const double d = (c.believed - me.position).Length();
            if (c.hostile) {
                ++hostiles;
                if (d < dh) {
                    dh = d;
                    hostile = &c;
                }
            } else if (!c.friendly && d < du) {
                du = d;
                unknown = &c;
            }
        }
        // A leader who commits himself goes home when the odds he believes are against him.
        if (!f.striker && f.doc->commit_odds > 0.0 && !f.doc->ControllerBound() && hostiles > 0) {
            int own = 0;
            for (const Fighter& o : fighters_) own += !o.gone && o.mode != Mode::Home && o.ours == f.ours && !o.striker ? 1 : 0;
            if (static_cast<double>(own) / hostiles < f.doc->commit_odds) return GoHome(f, me, t, w, "the odds");
        }
        if (f.striker) {
            if (hostile != nullptr && dh < fp.vid_m) Fire(f, me, *hostile, t, w);
            return;
        }
        if (hostile != nullptr) {
            f.mode = Mode::Engage;
            Steer(f, me, Lead(me, *hostile, t), t, w);
            Fire(f, me, *hostile, t, w);
        } else if (unknown != nullptr) {
            f.mode = Mode::Identify;  // closes to see what it is
            Steer(f, me, Lead(me, *unknown, t), t, w);
        } else if (f.last_vector_s >= 0.0 && t - f.last_vector_s < fp.control_lost_s && !f.free) {
            f.mode = Mode::Vectored;
            Steer(f, me, f.vector_point, t, w);
        } else {
            f.mode = f.free ? Mode::Search : Mode::Patrol;
            if (f.steering && w.release) w.release(f.target);
            f.steering = false;
        }
    }

    // What he sees and what his radar holds.
    void Sense(Fighter& f, const Aircraft& me, double t, const AirFightWorld& w) {
        const FightParams& fp = def_->combat.fight;
        // His eyes: B3's picture.
        if (w.picture) {
            if (const std::vector<Sighting>* pic = w.picture(f.target)) {
                for (const Sighting& s : *pic) {
                    if (s.kind != SightKind::Aircraft || t - s.time_s > fp.contact_hold_s) continue;
                    Contact c;
                    c.believed = s.believed_enu;
                    c.t_s = s.time_s;
                    c.what = s.what;
                    c.truth = Nearest(f, s.believed_enu, w, 3000.0, true);
                    if (c.truth < 0) continue;  // nothing of the battle's there: not his business
                    const bool opposed = Opposed(f, c.truth);
                    // Recognised inside the identification range: friend or foe by the type he recognised.
                    if (s.recognised && s.range_est_m <= fp.vid_m) {
                        c.hostile = opposed;
                        c.friendly = !opposed;
                    }
                    if (const Aircraft* a = w.air(c.truth)) c.vel = a->velocity;
                    Merge(f, c);
                }
            }
        }
        // His radar: by hand, or cued after a vector; looking down, the clutter figure; inside the scan's sector.
        if (f.radar != nullptr) {
            const bool cued = f.last_vector_s >= 0.0 && t - f.last_vector_s < fp.control_lost_s;
            const double reach = cued && f.radar->cued_m > 0.0 ? f.radar->cued_m : f.radar->by_hand_m;
            const double my_alt = w.altitude ? w.altitude(me.position) : me.position.z;
            const AirFightControllerDef* ctl = def_->Controller(f.def->controller);
            const bool e3a = ctl != nullptr && ctl->kind == "airborne_warning" && cued;
            for (const Fighter& o : fighters_) {
                if (o.gone || o.ours == f.ours) continue;
                const Aircraft* a = w.air(o.target);
                if (a == nullptr) continue;
                const Vec3 d = a->position - me.position;
                if (Angle(me.velocity, d) > fp.radar_az_rad) continue;
                const double tgt_alt = w.altitude ? w.altitude(a->position) : a->position.z;
                const bool below = tgt_alt < my_alt - 300.0 && (w.agl ? w.agl(a->position) : tgt_alt) < 3000.0;
                double r = below ? (f.radar->clutter_m > 0.0 ? f.radar->clutter_m : 0.5 * reach) : reach;
                if (f.radar->rcs_m2 > 0.0) r *= std::pow(std::max(0.01, a->rcs_m2) / f.radar->rcs_m2, 0.25);
                if (d.Length() > r) continue;
                Contact c;
                c.truth = o.target;
                const std::uint64_t n = ++sense_count_;
                c.believed = a->position + Vec3{0.01 * d.Length() * N(f.key, n, 31), 0.01 * d.Length() * N(f.key, n, 32), 0.0};
                c.vel = a->velocity;
                c.t_s = t;
                // The controller's word for a contact near the one he called; an E-3A clears one with no friendly near it.
                for (const Contact& k : f.contacts) {
                    if (k.hostile && (k.believed - c.believed).Length() < 3000.0) c.hostile = true;
                }
                if (e3a && f.doc->NeedsVid() && !FriendlyNear(f, c.believed, fp.friendly_clear_m, w)) c.hostile = true;
                Merge(f, c);
            }
        }
    }

    // The battle's aircraft nearest a believed place (within `within_m`), for the act; `any`: either side.
    int Nearest(const Fighter& f, const Vec3& p, const AirFightWorld& w, double within_m, bool any) const {
        int best = -1;
        double bd = within_m;
        for (const Fighter& o : fighters_) {
            if (o.gone || &o == &f || (!any && o.ours == f.ours)) continue;
            const Aircraft* a = w.air(o.target);
            if (a == nullptr) continue;
            const double d = (a->position - p).Length();
            if (d < bd) {
                bd = d;
                best = o.target;
            }
        }
        return best;
    }
    bool FriendlyNear(const Fighter& f, const Vec3& p, double r, const AirFightWorld& w) const {
        for (const Fighter& o : fighters_) {
            if (o.gone || o.ours != f.ours) continue;
            const Aircraft* a = w.air(o.target);
            if (a != nullptr && &o != &f && (a->position - p).Length() < r) return true;
        }
        return false;
    }
    static bool Armed(const Fighter& f) {
        if (f.gun != nullptr && f.gun_s > 0.0) return true;
        for (const auto& m : f.aams) {
            if (m.second > 0) return true;
        }
        return false;
    }
    static Vec3 Lead(const Aircraft& me, const Contact& c, double t) {
        const double age = std::max(0.0, t - c.t_s);
        const Vec3 now = c.believed + c.vel * age;
        const double closing = std::max(100.0, me.velocity.Length());
        const double tti = std::min(30.0, (now - me.position).Length() / closing);
        return now + c.vel * (0.5 * tti);
    }

    void Steer(Fighter& f, const Aircraft& me, const Vec3& point, double t, const AirFightWorld& w) {
        if (!w.steer) return;
        PilotOverride ov;
        ov.kind = PilotOverride::Kind::Break;
        ov.end = PilotOverride::End::Time;
        ov.after = PilotOverride::After::Rejoin;
        ov.point = point;
        ov.turn_rate_rad_s = me.max_turn_rate_rad_s;
        ov.until_s = t + 3.0 * def_->combat.fight.think_s + 1.0;
        w.steer(f.target, ov);
        f.steering = true;
    }

    void GoHome(Fighter& f, const Aircraft& me, double t, const AirFightWorld& w, const std::string& why) {
        f.mode = Mode::Home;
        f.why_home = why;
        if (w.event) w.event(f.def->target, "turns for home (" + why + ")");
        if (!w.steer) return;
        Vec3 away = me.velocity;
        for (const Contact& c : f.contacts) {
            if (c.hostile) away = me.position - c.believed;
        }
        away.z = 0.0;
        if (away.Length() < 1.0) away = Vec3{0.0, 1.0, 0.0};
        PilotOverride ov;
        ov.kind = PilotOverride::Kind::TurnAway;
        ov.end = PilotOverride::End::Point;
        ov.after = PilotOverride::After::Home;
        ov.point = me.position + away * (30000.0 / away.Length());
        ov.turn_rate_rad_s = me.max_turn_rate_rad_s;
        ov.until_s = t + 600.0;
        w.steer(f.target, ov);
    }

    // The shot: his weapons in his doctrine's order, each fired when he believes the target is in its zone.
    void Fire(Fighter& f, const Aircraft& me, const Contact& c, double t, const AirFightWorld& w) {
        const FightParams& fp = def_->combat.fight;
        if (t < f.next_shot_s || c.truth < 0) return;
        const Aircraft* tg = w.air(c.truth);
        if (tg == nullptr) return;
        for (const Missile& m : missiles_) {
            if (!m.done && fighters_[m.shooter].target == f.target && m.target == c.truth) return;  // one at a time at each
        }
        const ChainParams& ch = def_->combat.calibration.chain;
        const Vec3 los = tg->position - me.position;
        ShotGeometry g;
        g.range_m = los.Length();
        g.aspect_rad = chain_detail::kPi - Angle(tg->velocity, me.position - tg->position);
        if (tg->velocity.Length() < 1.0) g.aspect_rad = chain_detail::kPi / 2.0;
        g.off_boresight_rad = Angle(me.velocity, los);
        g.altitude_m = w.altitude ? w.altitude(me.position) : me.position.z;
        g.launch_g = std::max(1.0, me.load_g);
        const double tgt_alt = w.altitude ? w.altitude(tg->position) : tg->position.z;
        g.look_down = tgt_alt < g.altitude_m - 300.0 && (w.agl ? w.agl(tg->position) : tgt_alt) < 3000.0;
        Pilot pl;
        pl.skill = f.skill;
        pl.stress = 0.4;
        for (std::size_t wi = 0; wi < f.doc->shot_order.size(); ++wi) {
            const std::string& kind = f.doc->shot_order[wi];
            if (kind == "gun") {
                if (f.gun == nullptr || f.gun_s <= 0.0) continue;
                if (g.range_m > f.gun->effective_m || g.off_boresight_rad > fp.gun_cone_rad || g.aspect_rad > fp.gun_aspect_rad) continue;
                const double burst = std::min(f.gun_s, fp.think_s);
                f.gun_s -= burst;
                GunPass gp;
                gp.range_m = g.range_m;
                gp.window_s = burst;
                gp.target_g = std::max(1.0, tg->load_g);
                const GunResult gr = ResolveGunPass(*f.gun, ch, gp, pl, seed_, f.key, ++f.shots);
                Record(f, c.truth, f.gun->id, t, gr.kill, gr.reason, c.what, w);
                if (gr.kill) Down(f, c.truth, f.gun->id, t, w);
                return;
            }
            const bool sarh = kind == "sarh";
            for (auto& m : f.aams) {
                if (m.second <= 0 || (sarh ? !IsSemiActive(m.first->guidance) : !IsInfrared(m.first->guidance))) continue;
                // A radar missile needs his own radar's lock.
                // (A radar with no published lock range locks at 0.8 of its search range: a game value.)
                const double lock_m = f.radar == nullptr ? 0.0 : (f.radar->lock_m > 0.0 ? f.radar->lock_m : 0.8 * f.radar->by_hand_m);
                if (sarh && (f.radar == nullptr || g.range_m > lock_m || g.off_boresight_rad > fp.radar_az_rad)) continue;
                const std::uint64_t shot = ++f.shots;
                const ShotGeometry believed = Believe(g, ch, pl, m.first->guidance, seed_, f.key, shot);
                if (!PilotFires(*m.first, ch, believed)) continue;
                --m.second;
                Missile ms;
                ms.shooter = static_cast<std::size_t>(&f - fighters_.data());
                ms.target = c.truth;
                ms.aam = m.first;
                ms.geo = g;
                ms.fired_s = t;
                ms.arrive_s = t + g.range_m / std::max(100.0, m.first->mean_mps);
                ms.shot = shot;
                ms.believed = c.what;
                missiles_.push_back(ms);
                f.next_shot_s = t + 4.0;
                if (w.event) w.event(f.def->target, "fires " + m.first->name + " at " + c.what + " " + std::to_string(static_cast<int>(g.range_m)) + " m");
                return;
            }
        }
    }

    void Missiles(double t, const AirFightWorld& w) {
        const FightParams& fp = def_->combat.fight;
        for (Missile& m : missiles_) {
            if (m.done) continue;
            Fighter& f = fighters_[m.shooter];
            // A radar missile: the launcher holds the lock in his radar's sector, alive, until impact.
            if (IsSemiActive(m.aam->guidance) && m.lock_lost_s < 0.0) {
                const Aircraft* me = w.air(f.target);
                const Aircraft* tg = w.air(m.target);
                if (me == nullptr || tg == nullptr || Angle(me->velocity, tg->position - me->position) > fp.radar_az_rad) m.lock_lost_s = t;
            }
            if (t < m.arrive_s) continue;
            m.done = true;
            const Aircraft* tg = w.air(m.target);
            if (tg == nullptr) continue;  // gone already
            ShotConditions k;
            // His break, if he knew of the shot: a lock or a launch on his receiver, or the trail or the shooter seen since.
            bool knew = false;
            if (w.picture) {
                if (const std::vector<Sighting>* pic = w.picture(m.target)) {
                    for (const Sighting& s : *pic) {
                        if (s.time_s < m.fired_s) continue;
                        if (s.kind == SightKind::Trail || s.kind == SightKind::LaunchSign || (s.by == SightBy::Rwr && s.rwr != RwrState::Search) ||
                            (s.kind == SightKind::Aircraft && s.range_est_m < 8000.0)) {
                            knew = true;
                        }
                    }
                }
            }
            if (knew) {
                const Fighter* tf = FighterAt(m.target);
                const double sk = tf != nullptr ? tf->skill : 0.5;
                k.break_timing = 0.4 + 0.5 * sk;
                k.target_g = tg->max_load_g > 0.0 ? std::min(fp.break_g, tg->max_load_g) : fp.break_g;
            }
            if (m.lock_lost_s >= 0.0) k.lock_lost_before_impact_s = std::max(0.0, m.arrive_s - m.lock_lost_s);
            const ShotResult r = ResolveShot(*m.aam, def_->combat.calibration.chain, m.geo, k, Pilot{f.skill, 0.4, false}, seed_, f.key, m.shot);
            Record(f, m.target, m.aam->id, t, r.kill, r.reason, m.believed, w);
            if (r.kill) Down(f, m.target, m.aam->id, t, w);
        }
        missiles_.erase(std::remove_if(missiles_.begin(), missiles_.end(), [](const Missile& m) { return m.done; }), missiles_.end());
    }

    void Record(Fighter& f, int target, const std::string& weapon, double t, bool kill, MissReason why, const std::string& believed,
                const AirFightWorld& w) {
        ShotRecord s;
        s.shooter = f.def->target;
        const Fighter* tf = FighterAt(target);
        s.target = tf != nullptr ? tf->def->target : std::string();
        s.weapon = weapon;
        s.t_s = t;
        s.kill = kill;
        s.reason = why;
        shots_.push_back(s);
        // What he claims: a kill he saw, and some misses he believed were kills.
        if (kill || U(f.key, f.shots, 41) < def_->combat.fight.claim_on_miss) {
            claims_.push_back({f.def->target, believed, kill, t});
            if (!kill && w.event) w.event(f.def->target, "claims a kill");
        }
        (void)w;
    }

    void Down(Fighter& f, int target, const std::string& weapon, double t, const AirFightWorld& w) {
        const Fighter* tf = FighterAt(target);
        const std::string by = f.ours ? (f.def->regiment.empty() ? std::string("our fighters") : f.def->regiment) : f.def->type;
        if (w.destroy && !w.destroy(target, by)) return;
        kills_.push_back({tf != nullptr ? tf->def->target : std::string(), by, f.def->target, weapon, t});
        for (Fighter& o : fighters_) {
            if (o.target == target) o.gone = true;
        }
    }

    const AirFightDef* def_ = nullptr;
    std::uint64_t seed_ = 0;
    double start_s_ = -1.0;
    std::vector<Fighter> fighters_;
    std::vector<Missile> missiles_;
    std::vector<Vector> vectors_;
    std::vector<Kill> kills_;
    std::vector<Claim> claims_;
    std::vector<ShotRecord> shots_;
    std::uint64_t vector_count_ = 0;
    std::uint64_t sense_count_ = 0;
    int vectors_sent_ = 0;
    int vectors_heard_ = 0;
};

}  // namespace airwar
}  // namespace simcore
