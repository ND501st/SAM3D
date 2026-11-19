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
//
// B10 (docs/air_battle/08_OUR_FIGHTERS.md section 4): our controllers vector on our own picture when the runner gives one
// (AirFightWorld::tracks: our GCI's tracks, late and with their errors, identified by its interrogator), never on the truth;
// a fighter is added while the session runs (AddFighter: a pair scrambled by the duty's rule table), and his time in the
// fight and without a vector counts from when he first flies in it.
//
// Roadmap S3 (AirFightDef::mode 1; docs/air_battle/07_AIR_TO_AIR.md section 9):
//  - Radars: a fighter's radar radiates as its EM emitter (AirFightWorld::radar: off, searching its sector, or locked on
//    one aircraft, which the target's warning receiver hears as a lock); a fighter under a controller who brings him in
//    keeps it off until the controller's target is inside its cued range (the Pact's 1984 practice, pennington_1985). He
//    sees by it only while it radiates; looking down at a target against the ground its range is cut by the clutter its
//    look_down_db does not reject (fight.look_down_clutter_db; the APG-63's 40 dB gives dubrov_1983's 50 of 80 km).
//  - Control: a controller vectors only the fighters an authority line of its faction lets it (AirFightDef::MayVector);
//    one it may not vector is not bound to it.
//  - Tactics: the doctrine's attack: a beam attack (NATO, the Bekaa) steers abeam the target's track and turns in on its
//    beam; a slash (the Pact, ours) steers for its tail, fires and extends away for extend_s before coming again; a turn
//    (the neutral, the rebels) is lead pursuit. A fighter warned by his receiver of a lock or a launch, or who sees a
//    trail, drags (turns tail at full speed for drag_s) unless he can shoot first, or always if green (ACEVAL's F-5s,
//    anderegg_2001 p. 160). A missile is a point flying at its mean speed for the flight time its zone gives
//    (fight.missile_ref_target_mps), so a drag can run it out of energy; a semi-active round loses its launcher's lock in
//    the beam against the ground (the notch).
//  - In the fight: a target who knows of the shot answers it with flares (infrared) or chaff (radar) by his reaction
//    chain's habit and load (AirFightWorld::dispense), and what is out when it arrives is the chain's countermeasure;
//    an escort holds its station on its strikers' leader and engages only within escort_tether_m of them.
//  - The far fight (A1, 07 section 5.6): a fighter far from our ground units (AirFightWorld::far_from_ours) thinks at a1_think_s, and
//    a wingman there keeps his slot, takes his leader's contacts and shoots on his leader's geometry. Nothing changes
//    level while a missile of his or at him is in the air.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
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

// A track on a controller's picture (our GCI's: command/own_flights.hpp): where his board has it, and whether it is hostile
// there (no friend's reply, not one of his own). `truth` is for the act only.
struct AirTrack {
    int truth = -1;
    Vec3 pos;
    Vec3 vel;
    bool hostile = true;
};

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
    // Our controllers' picture (B10): a controller of side "ours" with one vectors on it; without one, on the truth in his
    // range (B8's behaviour, for the enemy's E-3A and a range day's GCI).
    std::function<std::vector<AirTrack>(const std::string& controller, double t)> tracks;
    // The presentation feed (fx/events.hpp, roadmap B13; may be empty): told each shot as the fight decides it. Nothing in
    // the fight reads what they do, and they draw nothing from its generators.
    //   fx_launch  a missile leaves the shooter: its def, the shooter's shot number, its resolution time;
    //   fx_end     that shot resolved (`kill`), or its target gone before it arrived (`gone`);
    //   fx_gun     a gun burst of `burst_s` from t, and whether it brought the target down.
    std::function<void(int shooter, int target, const AamDef& aam, int shot, double t, double arrive_s)> fx_launch;
    std::function<void(int shooter, int target, const AamDef& aam, int shot, double t, bool kill, bool gone)> fx_end;
    std::function<void(int shooter, int target, const AirGunDef& gun, double t, double burst_s, bool kill)> fx_gun;
    // Roadmap S3 (each may be empty: then nothing radiates, nobody dispenses, every fighter is near).
    //   radar           the fighter's radar as its EM emitter: state 0 off, 1 searching, 2 locked on `locked` (a target)
    //   dispense        the target's pilot answers a shot he knows of: flares at an infrared round, chaff at a radar one
    //                   (by his reaction chain's habit and load); true if he did
    //   countermeasure  the strength of what he has out at `t` against that kind of seeker (0..1, the chain's link)
    //   leader          his flight's leader (a target), or -1 if he leads or flies alone
    //   far_from_ours   his flight is far from all our ground units: the far fight (A1)
    std::function<void(int target, const std::string& radar_def, int state, int locked)> radar;
    std::function<bool(int target, bool infrared, double t)> dispense;
    std::function<double(int target, bool infrared, double t)> countermeasure;
    std::function<int(int target)> leader;
    std::function<bool(int target)> far_from_ours;
};

class AirFightSession {
public:
    enum class Mode { Patrol, Vectored, Search, Identify, Engage, Home, Setup, Extend, Drag, Station };
    static const char* ModeName(Mode m) {
        switch (m) {
            case Mode::Patrol: return "patrol";
            case Mode::Vectored: return "vectored";
            case Mode::Search: return "search";
            case Mode::Identify: return "identify";
            case Mode::Engage: return "engage";
            case Mode::Home: return "home";
            case Mode::Setup: return "setup";
            case Mode::Extend: return "extend";
            case Mode::Drag: return "drag";
            case Mode::Station: return "station";
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
        ids_ = ids;
        fighters_.clear();
        added_.clear();
        for (const AirFightFighterDef& fd : def.fighters) {
            int target = -1;
            for (std::size_t i = 0; i < ids.size(); ++i) {
                if (ids[i] == fd.target) target = static_cast<int>(i);
            }
            if (!Make(fd, target, error)) return false;
        }
        return true;
    }
    // A fighter joins the running battle (B10: a pair scrambled by the duty's rule table): `target` is its index among the
    // scenario's targets. False, with why, before Init, without a doctrine for its faction or with no such target.
    bool AddFighter(const AirFightFighterDef& fd, int target, std::string* error = nullptr) {
        if (def_ == nullptr) {
            if (error != nullptr) *error = "air_fight: not started";
            return false;
        }
        added_.push_back(fd);
        if (!Make(added_.back(), target, error)) {
            added_.pop_back();
            return false;
        }
        return true;
    }
    std::size_t FighterCount() const { return fighters_.size(); }

    void Step(double t, const AirFightWorld& w) {
        if (def_ == nullptr) return;
        const FightParams& fp = def_->combat.fight;
        if (start_s_ < 0.0) start_s_ = t;
        for (Fighter& f : fighters_) {  // his time in the fight counts from when he is first in the air in it
            if (f.start_s >= 0.0 || f.gone) continue;
            if (w.air && w.air(f.target) != nullptr) f.start_s = t;
        }
        Controllers(t, w);
        Deliver(t);
        Missiles(t, w);
        const bool s3 = def_->mode >= 1;
        if (s3) Levels(w);
        for (std::size_t i = 0; i < fighters_.size(); ++i) {
            Fighter& f = fighters_[i];
            if (f.gone || f.start_s < 0.0 || t < f.next_think_s) continue;
            f.next_think_s = t + (f.is_far ? fp.a1_think_s : fp.think_s);
            const Aircraft* me = w.air ? w.air(f.target) : nullptr;
            if (me == nullptr) {
                f.gone = true;
                if (s3) SetRadar(f, 0, -1, w);
                continue;
            }
            ++counts_.thinks;
            counts_.thinks_far += f.is_far ? 1 : 0;
            Think(f, *me, t, w);
            if (s3) Radar(f, *me, t, w);
            f.modes_seen |= 1u << static_cast<unsigned>(f.mode);
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
    // Sent home by his command (S1: our fighters' pursuit limit past the front line, mt_turchin_coop p. 9): he turns for home at
    // his next think. False if no such fighter, or he is already going home.
    bool Recall(const std::string& target, const std::string& why) {
        for (Fighter& f : fighters_) {
            if (f.def->target != target || f.gone || f.mode == Mode::Home) continue;
            f.recall = why;
            return true;
        }
        return false;
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
    // Roadmap S3. His radar's state (0 off, 1 searching, 2 locked); whom he escorts (a target id); whether he fights far.
    int RadarStateOf(const std::string& target) const {
        const Fighter* f = Find(target);
        return f != nullptr ? f->radar_state : 0;
    }
    std::string EscortOf(const std::string& target) const {
        const Fighter* f = Find(target);
        return f != nullptr && f->escorted >= 0 && static_cast<std::size_t>(f->escorted) < ids_.size() ? ids_[static_cast<std::size_t>(f->escorted)]
                                                                                                    : std::string();
    }
    bool FarOf(const std::string& target) const {
        const Fighter* f = Find(target);
        return f != nullptr && f->is_far;
    }
    // What the fight cost and what it did (the bench's measures): thoughts, of them at the far fight's level, looks by own
    // sensors (a far wingman takes his leader's), drags, extensions after a pass, attacks set up, dispenses against missiles,
    // flown missiles out of energy, radar rounds ending in the beam against the ground.
    struct Counts {
        int thinks = 0, thinks_far = 0, senses = 0, drags = 0, extends = 0, setups = 0, dispenses = 0, out_of_energy = 0, notched = 0;
    };
    const Counts& Tally() const { return counts_; }
    // Was a fighter ever in this mode (tests): did he drag, extend, set up an attack, keep station.
    bool EverWas(const std::string& target, Mode m) const {
        const Fighter* f = Find(target);
        return f != nullptr && (f->modes_seen & (1u << static_cast<unsigned>(m))) != 0;
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
        std::string recall;       // sent home by his command (S1)
        std::uint64_t shots = 0;
        double start_s = -1.0;    // first in the air in this battle
        // Roadmap S3.
        std::string tactic = "turn";  // his doctrine's attack: beam, slash, turn
        bool vectorable = true;   // his controller may vector him (an authority line)
        double look_down = 0.5;   // his radar's range factor looking down against the ground
        int radar_state = 0;      // 0 off, 1 searching, 2 locked
        int radar_locked = -1;
        int escorted = -1;        // an escort's striker (a target), -1 none
        bool escort_looked = false;
        double extend_until = -1.0;
        Vec3 extend_dir;
        double drag_until = -1.0;
        Vec3 drag_dir;
        bool is_far = false;      // the far fight (A1)
        int lead = -1;            // at the far fight: his flight's leader's fighter index (a wingman), -1 none
        unsigned modes_seen = 0;
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
        // Roadmap S3: a point flying at its mean speed for its budget; the target's knowledge and answer.
        bool flown = false;
        Vec3 pos;
        double last_s = 0.0;
        double budget_s = 0.0;
        bool knew = false;
        double knew_s = -1.0;
        bool dispensed = false;
    };
    struct Vector {
        std::size_t fighter = 0;
        double heard_s = -1.0;
        Vec3 point;
        Vec3 vel;
        int truth = -1;
        bool done = false;
    };

    bool Make(const AirFightFighterDef& fd, int target, std::string* error) {
        Fighter f;
        f.def = &fd;
        f.target = target;
        f.doc = def_->combat.Doctrine(fd.faction);
        if (f.target < 0 || f.doc == nullptr) {
            if (error != nullptr) *error = "air_fight: fighter '" + fd.target + "': " + (f.target < 0 ? "no such target" : "no doctrine for " + fd.faction);
            return false;
        }
        f.ours = fd.faction == "ours";
        f.skill = fd.skill >= 0.0 ? fd.skill : f.doc->skill;
        const LoadOut lo = def_->combat.LoadOutFor(fd.type, def_->year, f.ours);
        f.aams = lo.aams;
        f.gun = lo.gun;
        f.gun_s = lo.gun != nullptr ? lo.gun->firing_s : 0.0;
        const std::vector<const SensorDef*> radars = def_->sensors.Of(fd.type, SensorKind::Radar, def_->year, f.ours);
        f.radar = radars.empty() ? nullptr : radars.front();
        f.striker = fd.role == "strike";
        f.key = 0xF1600000ull + static_cast<std::uint64_t>(fighters_.size());
        // Roadmap S3: his tactic, whether his controller may vector him, his radar's look-down.
        f.tactic = f.doc->tactic.empty() ? std::string("turn") : f.doc->tactic;
        if (const AirFightControllerDef* ctl = def_->Controller(fd.controller)) f.vectorable = def_->MayVector(fd.faction, ctl->kind, fd.role);
        f.look_down = LookDownFactor(f.radar);
        if (!fd.escorts.empty()) {
            for (std::size_t i = 0; i < ids_.size(); ++i) {
                if (ids_[i] == fd.escorts) f.escorted = static_cast<int>(i);
            }
        }
        fighters_.push_back(f);
        return true;
    }
    // A radar's range looking down against the ground, as a share of its range looking up: its sourced clutter figure, or
    // what the clutter it does not reject leaves of the radar equation (the range goes as the fourth root of the signal to
    // clutter); before S3 (or with no look-down figure) B8's half.
    double LookDownFactor(const SensorDef* r) const {
        if (r == nullptr) return 0.5;
        if (r->clutter_m > 0.0 && r->by_hand_m > 0.0) return std::min(1.0, r->clutter_m / r->by_hand_m);
        if (def_->mode < 1 || r->look_down_db < 0.0) return 0.5;
        const double loss_db = std::max(0.0, def_->combat.fight.look_down_clutter_db - r->look_down_db);
        return std::pow(10.0, -loss_db / 40.0);
    }

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

    // ---- roadmap S3: the far fight's level and the radars as emitters.

    // Far (A1): his flight is far from our ground units; never changed while a missile of his or at him is in the air. A
    // wingman there fights on his leader's picture and geometry, if his leader is a fighter of this battle and far too.
    void Levels(const AirFightWorld& w) {
        const bool on = def_->FarFight() && static_cast<bool>(w.far_from_ours);
        for (std::size_t i = 0; i < fighters_.size(); ++i) {
            Fighter& f = fighters_[i];
            if (f.gone) continue;
            bool busy = false;
            for (const Missile& m : missiles_) busy = busy || (!m.done && (m.shooter == i || m.target == f.target));
            if (!busy) f.is_far = on && w.far_from_ours(f.target);
        }
        for (Fighter& f : fighters_) {
            f.lead = -1;
            if (!f.is_far || !w.leader) continue;
            const int lt = w.leader(f.target);
            if (lt < 0 || lt == f.target) continue;
            for (std::size_t k = 0; k < fighters_.size(); ++k) {
                const Fighter& o = fighters_[k];
                if (o.target == lt && !o.gone && o.is_far && o.ours == f.ours && !o.striker && !f.striker) f.lead = static_cast<int>(k);
            }
        }
    }

    // His radar: off for a striker, at home, and while his controller brings him in from beyond the cued range (radar silence
    // until close, pennington_1985: bound to him, silent until an aircraft he knows of, else his vector's point, is within
    // the cued range); locked on the hostile he engages within its lock range or illuminates a missile at; else searching.
    bool RadarOn(const Fighter& f, const Aircraft& me) const {
        if (f.radar == nullptr || f.striker || f.mode == Mode::Home || f.mode == Mode::Drag) return false;
        const bool bound = f.vectorable && !f.control_lost && !f.free && def_->Controller(f.def->controller) != nullptr &&
                           (f.doc->ControllerBound() || f.doc->roe == "controller");
        if (!bound) return true;
        // Bound: silent until the nearest aircraft he was told of (else his vector's point) is within the cued range.
        double nearest = 1e18;
        for (const Contact& c : f.contacts) {
            if (!c.friendly) nearest = std::min(nearest, (c.believed - me.position).Length());
        }
        if (nearest > 1e17 && f.last_vector_s >= 0.0) nearest = (f.vector_point - me.position).Length();
        const double cued = f.radar->cued_m > 0.0 ? f.radar->cued_m : f.radar->by_hand_m;
        return nearest <= cued;
    }
    void Radar(Fighter& f, const Aircraft& me, double t, const AirFightWorld& w) {
        if (f.radar == nullptr) return;
        int state = RadarOn(f, me) ? 1 : 0, locked = -1;
        if (state == 1) {
            for (const Missile& m : missiles_) {
                if (!m.done && &fighters_[m.shooter] == &f && IsSemiActive(m.aam->guidance) && m.lock_lost_s < 0.0) {
                    state = 2;
                    locked = m.target;
                }
            }
        }
        if (state == 1 && f.mode == Mode::Engage) {
            const double lock_m = f.radar->lock_m > 0.0 ? f.radar->lock_m : 0.8 * f.radar->by_hand_m;
            double best = lock_m;
            for (const Contact& c : f.contacts) {
                if (!c.hostile || c.truth < 0 || t - c.t_s > def_->combat.fight.contact_hold_s) continue;
                const Aircraft* a = w.air(c.truth);
                if (a == nullptr || Angle(me.velocity, a->position - me.position) > def_->combat.fight.radar_az_rad) continue;
                const double d = (a->position - me.position).Length();
                if (d < best) {
                    best = d;
                    state = 2;
                    locked = c.truth;
                }
            }
        }
        SetRadar(f, state, locked, w);
    }
    void SetRadar(Fighter& f, int state, int locked, const AirFightWorld& w) {
        if (f.radar == nullptr || (state == f.radar_state && locked == f.radar_locked)) return;
        f.radar_state = state;
        f.radar_locked = locked;
        if (w.radar) w.radar(f.target, f.radar->radar_def, state, locked);
    }

    // ---- the controllers: each tracks what is in its range and vectors its own fighters.

    void Controllers(double t, const AirFightWorld& w) {
        const FightParams& fp = def_->combat.fight;
        for (std::size_t ci = 0; ci < def_->controllers.size(); ++ci) {
            const AirFightControllerDef& c = def_->controllers[ci];
            // Ours vector on our picture (B10); the others on what is in their range.
            const bool on_picture = c.side == "ours" && static_cast<bool>(w.tracks);
            std::vector<AirTrack> picture;
            bool asked = false;
            for (std::size_t i = 0; i < fighters_.size(); ++i) {
                Fighter& f = fighters_[i];
                if (f.gone || f.start_s < 0.0 || f.mode == Mode::Home || f.def->controller != c.id || f.striker || !f.vectorable) continue;
                if (t < f.next_vector_due) continue;
                f.next_vector_due = t + fp.gci_vector_s;
                const Aircraft* me = w.air ? w.air(f.target) : nullptr;
                if (me == nullptr || (me->position - c.pos).Length() > c.range_m) continue;
                // The nearest of the other side the controller holds.
                int best = -1;
                double best_d = 1e18;
                Vec3 at, vel;
                if (on_picture) {
                    if (!asked) {
                        picture = w.tracks(c.id, t);
                        asked = true;
                    }
                    for (const AirTrack& k : picture) {
                        if (!k.hostile || (k.pos - c.pos).Length() > c.range_m) continue;
                        const Fighter* kf = FighterAt(k.truth);
                        if (kf != nullptr && kf->ours == f.ours) continue;  // his own, by his plot of them
                        const double d = (k.pos - me->position).Length();
                        if (d < best_d) {
                            best_d = d;
                            best = k.truth;
                            at = k.pos;
                            vel = k.vel;
                        }
                    }
                } else {
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
                }
                if (best < 0) continue;
                const std::uint64_t n = ++vector_count_;
                const double err_m = on_picture ? 0.0 : fp.gci_error_m;  // the picture carries its own errors
                const Vec3 err{err_m * N(0xF16C0000ull + ci, n, 1), err_m * N(0xF16C0000ull + ci, n, 2), 0.0};
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
        if (!f.recall.empty()) return GoHome(f, me, t, w, f.recall);
        const bool s3 = def_->mode >= 1;
        if (s3 && f.lead >= 0) {
            f.contacts = fighters_[static_cast<std::size_t>(f.lead)].contacts;  // a far wingman: his leader's picture
        } else {
            Sense(f, me, t, w);
            ++counts_.senses;
        }
        f.contacts.erase(std::remove_if(f.contacts.begin(), f.contacts.end(), [&](const Contact& c) { return t - c.t_s > fp.contact_hold_s; }),
                         f.contacts.end());
        // A striker only defends himself: he fires at a hostile he knows of close in, and flies his own route.
        // Control: a fighter bound to his controller who hears no vector for control_lost_s.
        const AirFightControllerDef* ctl = def_->Controller(f.def->controller);
        const bool bound = ctl != nullptr && !f.striker && f.vectorable && (f.doc->ControllerBound() || f.doc->roe == "controller");
        if (bound && !f.control_lost && !f.free) {
            const double since = f.last_vector_s >= 0.0 ? t - f.last_vector_s : t - f.start_s;
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
        const double in_fight = t - f.start_s;
        if (!f.striker) {
            if (in_fight >= fp.bingo_s) return GoHome(f, me, t, w, "bingo");
            if (!Armed(f)) return GoHome(f, me, t, w, "winchester");
            if (in_fight >= fp.joker_s && f.mode != Mode::Engage) return GoHome(f, me, t, w, "joker");
        }
        // An escort's strikers (S3): the one named, else the nearest of his side's, looked for once.
        const Aircraft* charge = nullptr;
        if (s3 && f.def->role == "escort") {
            if (f.escorted < 0 && !f.escort_looked) {
                f.escort_looked = true;
                double bd = 40000.0;
                for (const Fighter& o : fighters_) {
                    if (o.gone || !o.striker || o.ours != f.ours) continue;
                    const Aircraft* a = w.air(o.target);
                    if (a != nullptr && (a->position - me.position).Length() < bd) {
                        bd = (a->position - me.position).Length();
                        f.escorted = o.target;
                    }
                }
            }
            if (f.escorted >= 0) charge = w.air(f.escorted);
        }
        // The best contact: the nearest hostile, else the nearest not yet identified (an escort's within his tether).
        const Contact* hostile = nullptr;
        const Contact* unknown = nullptr;
        double dh = 1e18, du = 1e18;
        int hostiles = 0;
        for (const Contact& c : f.contacts) {
            if (charge != nullptr && (c.believed - charge->position).Length() > fp.escort_tether_m) continue;
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
        if (s3) return Decide(f, me, hostile, dh, unknown, charge, t, w);
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

    // ---- roadmap S3: what he does with what he knows.
    void Decide(Fighter& f, const Aircraft& me, const Contact* hostile, double dh, const Contact* unknown, const Aircraft* charge, double t,
                const AirFightWorld& w) {
        const FightParams& fp = def_->combat.fight;
        // A far wingman keeps his slot and fires on his leader's geometry.
        if (f.lead >= 0) {
            const Fighter& L = fighters_[static_cast<std::size_t>(f.lead)];
            f.mode = L.mode == Mode::Home ? Mode::Patrol : L.mode;
            if (f.steering && w.release) w.release(f.target);
            f.steering = false;
            const Aircraft* la = w.air(L.target);
            if (hostile != nullptr && la != nullptr) Fire(f, me, *hostile, t, w, la);
            return;
        }
        // The drag: turned tail, at full speed, until it is over.
        if (f.drag_until > t) {
            f.mode = Mode::Drag;
            Steer(f, me, me.position + f.drag_dir * 30000.0, t, w, FullSpeed(me));
            return;
        }
        Vec3 away;
        if (Warned(f, me, t, w, away) && ShouldDrag(f, me, hostile, dh)) {
            f.drag_until = t + fp.drag_s;
            f.drag_dir = away;
            f.extend_until = -1.0;
            f.mode = Mode::Drag;
            ++counts_.drags;
            if (w.event) w.event(f.def->target, "drags");
            Steer(f, me, me.position + away * 30000.0, t, w, FullSpeed(me));
            return;
        }
        // The extension after a slashing pass: straight on and away, at full speed.
        if (f.extend_until > t) {
            f.mode = Mode::Extend;
            Steer(f, me, me.position + f.extend_dir * 30000.0, t, w, FullSpeed(me));
            return;
        }
        if (hostile != nullptr) {
            Attack(f, me, *hostile, dh, t, w);
        } else if (unknown != nullptr) {
            f.mode = Mode::Identify;  // closes to see what it is
            Steer(f, me, Lead(me, *unknown, t), t, w);
        } else if (f.last_vector_s >= 0.0 && t - f.last_vector_s < fp.control_lost_s && !f.free) {
            f.mode = Mode::Vectored;
            Steer(f, me, f.vector_point, t, w);
        } else if (charge != nullptr) {
            Station(f, me, *charge, t, w);
        } else {
            f.mode = f.free ? Mode::Search : Mode::Patrol;
            if (f.steering && w.release) w.release(f.target);
            f.steering = false;
        }
    }

    static double FullSpeed(const Aircraft& me) { return me.max_speed_mps > 0.0 ? me.max_speed_mps : 0.0; }
    static Vec3 Flat(const Vec3& v) {
        Vec3 h{v.x, v.y, 0.0};
        const double l = h.Length();
        return l > 1e-6 ? h * (1.0 / l) : Vec3{0.0, 1.0, 0.0};
    }

    // Warned: a lock or a launch on his receiver, or a trail he saw, within drag_warning_s; `away` points from the threat.
    bool Warned(const Fighter& f, const Aircraft& me, double t, const AirFightWorld& w, Vec3& away) const {
        if (!w.picture) return false;
        const std::vector<Sighting>* pic = w.picture(f.target);
        if (pic == nullptr) return false;
        const double since = t - def_->combat.fight.drag_warning_s;
        for (const Sighting& s : *pic) {
            if (s.time_s < since) continue;
            if (s.by == SightBy::Rwr && (s.rwr == RwrState::Lock || s.rwr == RwrState::Launch)) {
                away = Vec3{-std::sin(s.bearing_rad), -std::cos(s.bearing_rad), 0.0};
                return true;
            }
            if ((s.kind == SightKind::Trail || s.kind == SightKind::LaunchSign) && s.range_est_m < 10000.0) {
                away = Flat(me.position - s.believed_enu);
                return true;
            }
        }
        return false;
    }
    // He drags unless he can shoot first: a green pilot always; a skilled one when no hostile he knows of is inside the
    // reach of what he carries, and he has no missile of his own in the air.
    bool ShouldDrag(const Fighter& f, const Aircraft& me, const Contact* hostile, double dh) const {
        if (f.skill < 0.5) return true;
        for (const Missile& m : missiles_) {
            if (!m.done && &fighters_[m.shooter] == &f) return false;
        }
        if (hostile == nullptr) return true;
        const double alt = me.position.z;
        double reach = f.gun != nullptr && f.gun_s > 0.0 ? f.gun->effective_m : 0.0;
        for (const auto& m : f.aams) {
            if (m.second > 0) reach = std::max(reach, MaxRangeM(*m.first, def_->combat.calibration.chain, chain_detail::kPi, alt));
        }
        return dh > reach;
    }

    // The doctrine's attack on a hostile he knows of.
    void Attack(Fighter& f, const Aircraft& me, const Contact& c, double dh, double t, const AirFightWorld& w) {
        const FightParams& fp = def_->combat.fight;
        const double age = std::max(0.0, t - c.t_s);
        const Vec3 now = c.believed + c.vel * age;
        const bool moving = Vec3{c.vel.x, c.vel.y, 0.0}.Length() > 1.0;
        const Vec3 track = Flat(c.vel);
        const double on_nose = moving ? Angle(Vec3{c.vel.x, c.vel.y, 0.0}, Vec3{me.position.x - now.x, me.position.y - now.y, 0.0}) : 0.0;
        const double aspect = chain_detail::kPi - on_nose;  // off the target's tail: 0 behind it, pi on its nose
        const double closing = std::max(100.0, me.velocity.Length());
        const double tti = std::min(30.0, dh / closing);
        bool setup = false;
        Vec3 point;
        if (moving && f.tactic == "beam" && dh > fp.beam_turn_in_m && std::fabs(aspect - 0.5 * chain_detail::kPi) > chain_detail::kPi / 6.0) {
            // Abeam the track on his own side, ahead by the time to get there.
            const Vec3 right{track.y, -track.x, 0.0};
            const double side = Dot(me.position - now, right) >= 0.0 ? 1.0 : -1.0;
            point = now + c.vel * (0.5 * tti) + right * (side * fp.beam_offset_m);
            setup = true;
        } else if (moving && f.tactic == "slash" && dh > fp.slash_turn_in_m && aspect > chain_detail::kPi / 3.0) {
            point = now + c.vel * (0.5 * tti) - track * fp.slash_offset_m;  // for its tail
            setup = true;
        }
        if (setup) {
            if (f.mode != Mode::Setup) ++counts_.setups;
            f.mode = Mode::Setup;
            point.z = now.z;
            Steer(f, me, point, t, w);
            return;
        }
        f.mode = Mode::Engage;
        Steer(f, me, Lead(me, c, t), t, w);
        const std::uint64_t before = f.shots;
        const double next_before = f.next_shot_s;
        Fire(f, me, c, t, w);
        if (f.tactic != "slash") return;
        // A slash is one pass: after his shot (once no radar round of his needs his lock) or once through, he extends.
        bool illuminating = false;
        for (const Missile& m : missiles_) illuminating = illuminating || (!m.done && &fighters_[m.shooter] == &f && IsSemiActive(m.aam->guidance));
        const bool fired = f.shots != before && (f.next_shot_s != next_before || f.gun_s <= 0.0);
        if (!illuminating && (fired || dh < fp.pass_m)) {
            f.extend_until = t + fp.extend_s;
            f.extend_dir = Flat(me.velocity);
            ++counts_.extends;
            if (w.event) w.event(f.def->target, "extends");
        }
    }

    // An escort on his station: behind, abeam and above his strikers' leader, closing at a speed that keeps it.
    void Station(Fighter& f, const Aircraft& me, const Aircraft& charge, double t, const AirFightWorld& w) {
        const FightParams& fp = def_->combat.fight;
        f.mode = Mode::Station;
        const Vec3 fwd = Flat(charge.velocity);
        const Vec3 right{fwd.y, -fwd.x, 0.0};
        const double side = (f.key & 1u) != 0u ? 1.0 : -1.0;
        const Vec3 station = charge.position - fwd * fp.escort_back_m + right * (side * fp.escort_side_m) + Vec3{0.0, 0.0, fp.escort_up_m};
        const double v = std::max(50.0, charge.velocity.Length());
        const double behind = Dot(station - me.position, fwd);
        const double speed = v + std::clamp(0.05 * behind, -0.3 * v, 0.3 * v);
        Vec3 point = station + charge.velocity * 4.0;
        if ((point - me.position).Length() < 600.0) point = me.position + fwd * 600.0;
        Steer(f, me, point, t, w, speed);
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
        if (f.radar != nullptr && (def_->mode < 1 || RadarOn(f, me))) {
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
                double r = below ? (def_->mode >= 1 ? f.look_down * reach : f.radar->clutter_m > 0.0 ? f.radar->clutter_m : 0.5 * reach) : reach;
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

    // `speed_mps` > 0: the speed he flies it at (S3: a drag or an extension at full speed, an escort keeping station).
    void Steer(Fighter& f, const Aircraft& me, const Vec3& point, double t, const AirFightWorld& w, double speed_mps = 0.0) {
        if (!w.steer) return;
        PilotOverride ov;
        ov.kind = PilotOverride::Kind::Break;
        ov.end = PilotOverride::End::Time;
        ov.after = PilotOverride::After::Rejoin;
        ov.point = point;
        ov.turn_rate_rad_s = me.max_turn_rate_rad_s;
        ov.speed_mps = speed_mps;
        ov.until_s = t + 3.0 * def_->combat.fight.think_s + 1.0;
        w.steer(f.target, ov);
        f.steering = true;
    }

    void GoHome(Fighter& f, const Aircraft& me, double t, const AirFightWorld& w, const std::string& why) {
        f.mode = Mode::Home;
        f.why_home = why;
        if (def_->mode >= 1) SetRadar(f, 0, -1, w);
        if (w.event) w.event(f.def->target, "turns for home (" + why + ")");
        if (!w.steer) return;
        Vec3 away = me.velocity;
        for (const Contact& c : f.contacts) {
            if (c.hostile) away = me.position - c.believed;
        }
        if (f.def->has_home) away = f.def->home - me.position;  // S1: ours with a base fly to it
        away.z = 0.0;
        if (away.Length() < 1.0) away = Vec3{0.0, 1.0, 0.0};
        PilotOverride ov;
        ov.kind = PilotOverride::Kind::TurnAway;
        ov.end = PilotOverride::End::Point;
        ov.after = PilotOverride::After::Home;
        ov.point = f.def->has_home ? Vec3{f.def->home.x, f.def->home.y, me.position.z} : me.position + away * (30000.0 / away.Length());
        ov.turn_rate_rad_s = me.max_turn_rate_rad_s;
        const double speed = std::max(50.0, me.velocity.Length());
        ov.until_s = t + std::max(600.0, 1.5 * away.Length() / speed);
        w.steer(f.target, ov);
    }

    // The shot: his weapons in his doctrine's order, each fired when he believes the target is in its zone.
    // `from` (S3, the far fight): the geometry his shot is judged on, his leader's; his round still leaves from him.
    void Fire(Fighter& f, const Aircraft& self, const Contact& c, double t, const AirFightWorld& w, const Aircraft* from = nullptr) {
        const FightParams& fp = def_->combat.fight;
        const Aircraft& me = from != nullptr ? *from : self;
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
                if (w.fx_gun) w.fx_gun(f.target, c.truth, *f.gun, t, burst, gr.kill);
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
                if (def_->mode >= 1) {
                    // Flown (S3): from him at its mean speed, for as long as its maximum ranges allow against a target
                    // of the reference speed head-on or from astern; arrive_s is then only the feed's forecast.
                    const double v = std::max(100.0, m.first->mean_mps), ref = fp.missile_ref_target_mps;
                    ms.flown = true;
                    ms.pos = self.position;
                    ms.last_s = t;
                    ms.budget_s = std::max(MaxRangeM(*m.first, ch, chain_detail::kPi, g.altitude_m) / (v + ref),
                                           MaxRangeM(*m.first, ch, 0.0, g.altitude_m) / std::max(50.0, v - ref));
                }
                ms.shot = shot;
                ms.believed = c.what;
                missiles_.push_back(ms);
                if (w.fx_launch) w.fx_launch(f.target, c.truth, *m.first, static_cast<int>(shot), t, ms.arrive_s);
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
            if (m.flown) {
                if (!Fly(m, f, t, w)) continue;
            } else if (t < m.arrive_s) {
                continue;
            }
            m.done = true;
            const Aircraft* tg = w.air(m.target);
            if (tg == nullptr) {  // gone already
                if (w.fx_end) w.fx_end(f.target, m.target, *m.aam, static_cast<int>(m.shot), t, false, true);
                continue;
            }
            ShotConditions k;
            if (m.flown && m.arrive_s < 0.0) {  // out of energy short of a target that ran
                ++counts_.out_of_energy;
                if (w.fx_end) w.fx_end(f.target, m.target, *m.aam, static_cast<int>(m.shot), t, false, false);
                Record(f, m.target, m.aam->id, t, false, MissReason::Energy, m.believed, w);
                continue;
            }
            if (m.flown) {
                // At its end: the target's countermeasures against its seeker, and whether it ends in the beam against the ground.
                if (w.countermeasure) k.countermeasures = w.countermeasure(m.target, IsInfrared(m.aam->guidance), t);
                const Aircraft* sh = w.air(f.target);
                const Vec3 from = sh != nullptr ? sh->position : m.pos;
                const double tgt_alt = w.altitude ? w.altitude(tg->position) : tg->position.z;
                const double sh_alt = w.altitude ? w.altitude(from) : from.z;
                const bool down = tgt_alt < sh_alt - 300.0 && (w.agl ? w.agl(tg->position) : tgt_alt) < 3000.0;
                const double aspect = tg->velocity.Length() < 1.0 ? 0.0 : chain_detail::kPi - Angle(tg->velocity, from - tg->position);
                k.end_notch = down && std::fabs(aspect - 0.5 * chain_detail::kPi) < def_->combat.calibration.chain.beam_notch_half_rad ? 1 : 0;
                counts_.notched += k.end_notch == 1 && IsSemiActive(m.aam->guidance) ? 1 : 0;
            }
            // His break, if he knew of the shot: a lock or a launch on his receiver, or the trail or the shooter seen since.
            bool knew = m.flown && m.knew;
            if (w.picture && !knew) {
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
            if (w.fx_end) w.fx_end(f.target, m.target, *m.aam, static_cast<int>(m.shot), t, r.kill, false);
            Record(f, m.target, m.aam->id, t, r.kill, r.reason, m.believed, w);
            if (r.kill) Down(f, m.target, m.aam->id, t, w);
        }
        missiles_.erase(std::remove_if(missiles_.begin(), missiles_.end(), [](const Missile& m) { return m.done; }), missiles_.end());
    }

    // A flown round (S3): lead pursuit at its mean speed. True once it has ended: at the target (arrive_s the time), its
    // target gone, or out of energy (arrive_s < 0). On the way the target learns of it from his picture, and answers it
    // once, a few seconds before it arrives, with flares or chaff by his habit.
    bool Fly(Missile& m, const Fighter& f, double t, const AirFightWorld& w) {
        const double dt = t - m.last_s;
        m.last_s = t;
        const Aircraft* tg = w.air(m.target);
        if (tg == nullptr) return true;
        const double v = std::max(100.0, m.aam->mean_mps);
        const Vec3 to = tg->position - m.pos;
        const double d = to.Length();
        const double tgo = d / v;
        if (!m.knew && w.picture) {
            if (const std::vector<Sighting>* pic = w.picture(m.target)) {
                for (const Sighting& s : *pic) {
                    if (s.time_s < m.fired_s) continue;
                    if (s.kind == SightKind::Trail || s.kind == SightKind::LaunchSign || (s.by == SightBy::Rwr && s.rwr == RwrState::Launch) ||
                        (s.by == SightBy::Rwr && s.rwr == RwrState::Lock && IsSemiActive(m.aam->guidance))) {
                        m.knew = true;
                        m.knew_s = t;
                    }
                }
            }
        }
        if (m.knew && !m.dispensed && tgo <= 4.0 && w.dispense) {
            m.dispensed = true;
            if (w.dispense(m.target, IsInfrared(m.aam->guidance), t)) ++counts_.dispenses;
        }
        const double step = v * std::max(0.0, dt);
        if (d <= step + 30.0) {
            m.pos = tg->position;
            m.arrive_s = t;
            return true;
        }
        if (t - m.fired_s > m.budget_s) {
            m.arrive_s = -1.0;
            return true;
        }
        Vec3 aim = tg->position + tg->velocity * tgo - m.pos;
        const double la = aim.Length();
        if (la > 1e-6) m.pos = m.pos + aim * (step / la);
        (void)f;
        return false;
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
    std::vector<std::string> ids_;  // the scenario's target ids by index (S3: an escort's strikers)
    Counts counts_;
    std::vector<Fighter> fighters_;
    std::deque<AirFightFighterDef> added_;  // fighters added while it runs (their defs stay put)
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
