// simcore/reaction_chain.hpp — reactions under fire in a session (roadmap B4, docs/air_battle/04_REACTIONS.md): the
// chain from a cue in a pilot's picture to his manoeuvre, his countermeasures, his call and his flight's decision.
//
//   Sighted / Damaged / Lost      a cue: what his picture (air_war/perception.hpp) gave him, his own damage, a wingman
//                                 seen going down. Never the truth: a launch he did not see is no cue.
//   noticed after a delay         reactions.hpp NoticeDelayS (skill, source, workload, a second seat, stress), or not at
//                                 all (NoReactionChance). The Pact's programme: at once, on a zone it was briefed on.
//   the manoeuvre                 the profile's row: a PilotOverride for the command layer (flights.hpp) to fly, ending
//                                 in a rejoin, the route again, or home; the dive's height to the pilot (target_behaviour)
//   countermeasures               chaff, flares by the type's fit in the war's year, on time, early, late or never
//   the call                      a transmission on the flight's net (B9's AirRadio): who hears it gets a cue of source
//                                 radio, with the clock code's error; a package on one strike frequency hears it too
//   the flight's decision         once per flight and situation (SAMs, flak, fighters, a loss), drawn by its leader:
//                                 jettison (tanks only, or all) called on the net; press on; abort (ReturnHome by the
//                                 leader); go round the believed zone (a Divert by the leader, the rest of the route
//                                 after it). A wingman who heard nothing decides for himself after decision_wait_s.
//
// The chain gives the runner acts (a manoeuvre, a dispense, a jettison, an abort of one aircraft, a line for the debrief)
// and gives its orders to the flights itself. Every draw is keyed (reactions.hpp ReactionU): the same seed, the same
// reactions, whatever the order the targets are visited in. Nothing reads the fx feed. Who may order a flight is
// Flights::May (B5's C2Authority hook): today it accepts every order.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "simcore/aircraft.hpp"
#include "simcore/air_war/air_radio.hpp"
#include "simcore/air_war/perception.hpp"
#include "simcore/damage.hpp"
#include "simcore/flights.hpp"
#include "simcore/reactions.hpp"
#include "simcore/target_behaviour.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

// A cue as the pilot holds it (04_REACTIONS.md 3.1).
struct ThreatCue {
    CueSource source = CueSource::Eye;
    ThreatClass threat = ThreatClass::Unknown;
    Vec3 where;               // where he believes it is (ENU)
    double range_m = 0.0;     // his estimate of the range to it
    bool aimed = false;       // a receiver's lock or launch light: at him
    int level = 1;            // 1 a lock or a sighting, 2 a launch
    double noticed_s = 0.0;
    std::uint64_t serial = 0; // per aircraft: keys its draws
    bool programmed = false;  // a zone of his flight's programme covers it
    int zone = -1;
    Dispense dispense = Dispense::OnTime;
};

// The calls on the air for the chain: a threat call or a report by a flight's member, and when a member of a flight (his
// own or another) acts on it (< 0: he did not hear it).
struct ThreatRadio {
    virtual ~ThreatRadio() = default;
    virtual int Say(const std::string& flight, int number, bool report, const std::string& words, double t, bool has_where, const Vec3& where) = 0;
    virtual double Reaches(int call, int fi, int slot, double t) = 0;
};

// The enemy's radio of roadmap B9 as the chain's: a threat call breaks in and is said even by a flight flying silent.
class AirRadioThreats final : public ThreatRadio {
public:
    explicit AirRadioThreats(airwar::AirRadio* r) : r_(r) {}
    int Say(const std::string& flight, int number, bool report, const std::string& words, double t, bool has_where, const Vec3& where) override {
        return r_->SayAs(flight, report ? airwar::CallKind::Report : airwar::CallKind::ThreatCall, words, t, has_where, where, false, 0.0, number);
    }
    double Reaches(int call, int fi, int slot, double t) override { return r_->Reaches(call, fi, slot, t, 0.0); }

private:
    airwar::AirRadio* r_;
};

// What the chain asks of the runner now.
struct ReactionAct {
    enum class Kind { Manoeuvre, Chaff, Flares, JettisonTanks, JettisonStores, Abort, Event };
    Kind kind = Kind::Event;
    int target = -1;
    PilotOverride ov;          // Manoeuvre
    bool dive = false;         // Manoeuvre: he takes the height in his hands, down to dive_agl_m
    double dive_agl_m = 0.0;
    int count = 0;             // Chaff, Flares: cartridges; JettisonTanks: tanks
    std::string what;          // for the debrief
};

// One aircraft's jettison (the ledger's, the bridge's).
struct JettisonRecord {
    int target = -1;
    double t_s = 0.0;
    int tanks = 0;
    bool stores = false;  // its load went (before its release point)
    std::string why;
};

class ReactionChain {
public:
    struct Context {
        Flights::Lookup air;                     // a target's aircraft in the sky (null: none)
        std::function<double(int)> agl_m;        // its height above the ground
        std::function<bool(int)> attacking;      // in its attack run
        ThreatRadio* radio = nullptr;            // null: the flights' fixed call delay
    };
    struct Counts {
        int cues = 0, reactions = 0, no_reaction = 0, programmed = 0, calls = 0, heard = 0;
        int chaff = 0, flares = 0, jettisons = 0, decisions = 0, press_on = 0, go_round = 0, aborts = 0, self_decided = 0;
        int scripted = 0;  // scripted rows' cues acted on (a hand-written "evade")
    };

    void Configure(const ReactionTable* table, std::uint64_t seed, int year) {
        table_ = table;
        seed_ = seed;
        year_ = year;
    }
    bool Configured() const { return table_ != nullptr && table_->loaded; }

    // A pilot who reacts by the tables: his profile, skill (0..1; < 0 his profile's), a second seat.
    bool Register(int target, const std::string& id, const ReactionPilotDef& d, double skill, bool two_seat) {
        if (!Configured() || target < 0) return false;
        const ReactionProfile* p = table_->Profile(d.profile);
        if (p == nullptr) return false;
        if (static_cast<std::size_t>(target) >= pilots_.size()) pilots_.resize(static_cast<std::size_t>(target) + 1);
        Pilot& s = pilots_[static_cast<std::size_t>(target)];
        s = Pilot();
        s.on = true;
        s.id = id;
        s.key = airwar::ObjectKeyOf(id);
        s.profile = p;
        s.skill = skill >= 0.0 ? std::clamp(skill, 0.0, 1.0) : p->skill;
        s.two_seat = two_seat;
        s.nerve = std::clamp(d.nerve, 0.0, 1.0);
        s.press_on_shift = d.press_on_shift;
        s.jettison_shift = d.jettison_shift;
        const Loadout l = table_->LoadoutOf(d.aircraft, year_);
        s.chaff = d.chaff >= 0 ? d.chaff : l.chaff;
        s.flares = d.flares >= 0 ? d.flares : l.flares;
        s.tanks = d.tanks >= 0 ? d.tanks : l.tanks;
        s.stores = !d.stores.empty();
        s.zones = d.zones;
        return true;
    }
    bool On(int target) const { return P(target) != nullptr; }

    // ---- A hand-written "evade" as a scripted profile (04_REACTIONS.md §12.3, roadmap S4). One row: its cue (a launch,
    // on his receiver or seen by eye; or a lock on his receiver; or the instructor's order alone), no delay, no
    // countermeasure, no jettison, no call and no flight's decision; he acts on it in the step it reaches him. The
    // manoeuvre is the pilot's own (TargetPilot::Evade): its kind, turn rate and evade_s, the beam taken from the cue's
    // bearing (for our own Osa's lock and launch our vehicle's place), and the follow-on today's code gives it. It needs no
    // reaction tables, and a scripted pilot is not On(): his damage and stores are the old path's.
    struct ScriptedCue {
        bool launch = false;  // a launch (else a lock)
        Vec3 from;            // where he believes the threat is
    };
    void RegisterScripted(int target, const std::string& id, const EvadeDef& e) {
        if (target < 0 || !e.present) return;
        if (static_cast<std::size_t>(target) >= scripted_.size()) scripted_.resize(static_cast<std::size_t>(target) + 1);
        ScriptedRow& r = scripted_[static_cast<std::size_t>(target)];
        r = ScriptedRow();
        r.on = true;
        r.id = id;
        r.cue = e.on;
    }
    bool Scripted(int target) const { return SR(target) != nullptr; }
    // A new sighting in his picture: true, and the cue, if it is his row's.
    bool SightedScripted(int target, const airwar::Sighting& sg, ScriptedCue& out) {
        const ScriptedRow* r = SR(target);
        if (r == nullptr) return false;
        const bool rwr = sg.by == airwar::SightBy::Rwr && sg.kind == airwar::SightKind::Emitter;
        const bool eye_launch = sg.by == airwar::SightBy::Eye && (sg.kind == airwar::SightKind::LaunchSign || sg.kind == airwar::SightKind::Trail);
        if (r->cue == EvadeDef::On::Launch && ((rwr && sg.rwr == airwar::RwrState::Launch) || eye_launch)) {
            out.launch = true;
            out.from = eye_launch && sg.has_origin ? sg.origin_enu : sg.believed_enu;
        } else if (r->cue == EvadeDef::On::Lock && rwr && sg.rwr == airwar::RwrState::Lock) {
            out.launch = false;
            out.from = sg.believed_enu;
        } else {
            return false;
        }
        ++counts_.scripted;
        return true;
    }
    // The instructor's order to manoeuvre (cue source `order`): any row takes it.
    bool OrderedScripted(int target) {
        if (SR(target) == nullptr) return false;
        ++counts_.scripted;
        return true;
    }
    double Stress(int target, double now) const {
        const Pilot* s = P(target);
        return s == nullptr ? 0.0 : StressDecay(s->stress, now - s->stress_t, table_->stress_half_life_s);
    }
    // Its load is gone by its own release (the waves, the bridge): nothing left to jettison but its tanks.
    void Released(int target) {
        if (Pilot* s = P(target)) s->stores = false;
    }
    bool StoresGone(int target) const {
        const Pilot* s = P(target);
        return s != nullptr && s->stores_jettisoned;
    }
    int Chaff(int target) const { return P(target) != nullptr ? P(target)->chaff : 0; }
    int Flares(int target) const { return P(target) != nullptr ? P(target)->flares : 0; }
    int Tanks(int target) const { return P(target) != nullptr ? P(target)->tanks : 0; }
    // Flares out within the window before `t` (an IR shot's end), or chaff within the chaff window.
    bool FlaresOut(int target, double t) const {
        const Pilot* s = P(target);
        return s != nullptr && s->flare_s <= t && t - s->flare_s <= table_->flare_window_s;
    }
    bool ChaffOut(int target, double t) const {
        const Pilot* s = P(target);
        return s != nullptr && s->chaff_s <= t && t - s->chaff_s <= table_->chaff_window_s;
    }
    // Roadmap S3: an air-to-air missile he knows of, in the air fight (air_war/air_fight_session.hpp): flares at an infrared
    // round, chaff at a radar one, by his profile's habit against the like SAM on his warning receiver (the same dispenser
    // and the same drill: an IR SAM, a low radar SAM) and from what he still carries. True if he dispensed.
    bool AnswerAam(int target, bool infrared, double now, std::vector<ReactionAct>& acts) {
        Pilot* s = P(target);
        if (s == nullptr || s->profile == nullptr) return false;
        const ThreatClass like = infrared ? ThreatClass::SamIr : ThreatClass::SamLow;
        const Countermeasure cm = CmFor(ChooseRow(*s->profile, CueSource::Rwr, like).cm, like);
        if (cm == Countermeasure::None) return false;
        const int chaff = s->chaff, flares = s->flares;
        DispenseNow(target, *s, cm, now, acts);
        return s->chaff != chaff || s->flares != flares;
    }
    // A keyed draw: did the flares decoy IR shot `shot` at it?
    bool FlareDecoys(int target, std::uint64_t shot) const {
        const Pilot* s = P(target);
        return s != nullptr && ReactionU(seed_, s->key, shot, ReactionDraw::Flare) < table_->flare_decoy;
    }
    const std::vector<JettisonRecord>& Jettisons() const { return jettisons_; }
    bool Pending() const { return !items_.empty(); }
    const Counts& Totals() const { return counts_; }
    // The flight's decision for a situation (-1 none yet; else FlightChoice), for tests and the debrief.
    int Decision(int fi, ThreatClass c) const {
        const auto it = decided_.find({fi, SituationOf(c)});
        return it != decided_.end() ? static_cast<int>(it->second.choice) : -1;
    }

    // A new sighting in his picture (perception.hpp): a cue if it is a threat he can name.
    void Sighted(int target, const airwar::Sighting& sg, double now, const Context& ctx) {
        Pilot* s = P(target);
        if (s == nullptr) return;
        ThreatCue c;
        if (!Classify(sg, *s, now, c)) return;
        c.noticed_s = now;
        Notice(target, *s, c, now, ctx);
    }
    // Damage he took (damage.hpp): his stress, and his call to his flight ("I'm hit"). What it makes him do (jettison,
    // abort) is the damage table's draw, read as his own damage row (the runner's OnDamage).
    void Damaged(int target, DamageLevel after, double now, const Flights& fl, const Context& ctx) {
        Pilot* s = P(target);
        if (s == nullptr || after < DamageLevel::Light) return;
        AddStress(*s, table_->stress_damage, now);
        const Aircraft* a = ctx.air ? ctx.air(target) : nullptr;
        if (a == nullptr || after < DamageLevel::Moderate) return;
        ThreatCue c;
        c.source = CueSource::Radio;
        c.threat = ThreatClass::Loss;
        c.where = a->position;
        c.noticed_s = now;
        Call(target, *s, fl, c, now, ctx, *a, "I'm hit");
    }
    // A wingman shot down at `where`: the others of his flight in sight of it see it.
    void Lost(int target, const Vec3& where, double now, const Flights& fl, const Context& ctx) {
        const int fi = fl.FlightOf(target);
        if (fi < 0) return;
        const FlightState& f = fl.All()[static_cast<std::size_t>(fi)];
        for (const FlightSlot& sl : f.slots) {
            if (sl.target == target || !sl.alive || sl.gone) continue;
            Pilot* s = P(sl.target);
            const Aircraft* a = ctx.air ? ctx.air(sl.target) : nullptr;
            if (s == nullptr || a == nullptr) continue;
            if ((a->position - where).Length() > table_->loss_seen_m) continue;
            ThreatCue c;
            c.source = CueSource::Eye;
            c.threat = ThreatClass::Loss;
            c.where = where;
            c.range_m = (a->position - where).Length();
            c.noticed_s = now;
            Notice(sl.target, *s, c, now, ctx);
        }
    }

    // What is due now: manoeuvres, dispenses, calls heard, decisions.
    std::vector<ReactionAct> Step(double now, Flights& fl, const Context& ctx) {
        std::vector<ReactionAct> acts;
        // Due items in the order of their time, then of their target and their serial (never the visiting order).
        std::vector<Item> due;
        for (std::size_t k = 0; k < items_.size();) {
            if (items_[k].due_s <= now + 1e-9) {
                due.push_back(items_[k]);
                items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(k));
            } else {
                ++k;
            }
        }
        std::stable_sort(due.begin(), due.end(), [](const Item& x, const Item& y) {
            if (x.due_s != y.due_s) return x.due_s < y.due_s;
            if (x.target != y.target) return x.target < y.target;
            return x.cue.serial < y.cue.serial;
        });
        for (const Item& it : due) Run(it, now, fl, ctx, acts);
        return acts;
    }

    // A jettison the runner did for another reason (his damage): in the record, his load gone.
    void RecordJettison(int target, double now, const std::string& why) {
        Pilot* s = P(target);
        if (s == nullptr) return;
        JettisonRecord r;
        r.target = target;
        r.t_s = now;
        r.tanks = s->tanks;
        r.stores = s->stores;
        r.why = why;
        s->tanks = 0;
        if (s->stores) s->stores_jettisoned = true;
        s->stores = false;
        if (r.tanks > 0 || r.stores) jettisons_.push_back(r);
    }

private:
    struct Pilot {
        bool on = false;
        std::string id;
        std::uint64_t key = 0;
        const ReactionProfile* profile = nullptr;
        double skill = 0.6;
        bool two_seat = false;
        double nerve = 0.5;
        double press_on_shift = 0.0;  // his officer's (roadmap B11)
        double jettison_shift = 0.0;
        double stress = 0.0;
        double stress_t = 0.0;
        int chaff = 0, flares = 0, tanks = 0;
        bool stores = false;
        bool stores_jettisoned = false;
        std::uint64_t serial = 0;
        std::map<int, std::pair<double, int>> last;  // situation -> (when he last reacted, the cue's level)
        std::set<int> heard_decision;                // situations his leader's decision reached him for
        std::set<int> self_decided;
        std::vector<ProgrammeZone> zones;
        double flare_s = -1e9;
        double chaff_s = -1e9;
    };
    struct Item {
        enum class Kind { Act, Pull, Dispense, Hear, Decision, SelfDecide };
        Kind kind = Kind::Act;
        int target = -1;
        double due_s = 0.0;
        ThreatCue cue;
        Countermeasure cm = Countermeasure::None;
        FlightDecision decision;  // Decision: what was decided (a jettison he carries out)
    };
    struct Decided {
        FlightChoice choice = FlightChoice::PressOn;
        bool jettison = false;
    };

    Pilot* P(int target) {
        if (target < 0 || static_cast<std::size_t>(target) >= pilots_.size()) return nullptr;
        Pilot& s = pilots_[static_cast<std::size_t>(target)];
        return s.on ? &s : nullptr;
    }
    const Pilot* P(int target) const {
        if (target < 0 || static_cast<std::size_t>(target) >= pilots_.size()) return nullptr;
        const Pilot& s = pilots_[static_cast<std::size_t>(target)];
        return s.on ? &s : nullptr;
    }

    void AddStress(Pilot& s, double add, double now) {
        s.stress = StressAdd(StressDecay(s.stress, now - s.stress_t, table_->stress_half_life_s), add);
        s.stress_t = now;
    }

    // What a sighting means to him: false if it is no threat he can name.
    bool Classify(const airwar::Sighting& sg, Pilot& s, double now, ThreatCue& c) {
        c.where = sg.has_origin ? sg.origin_enu : sg.believed_enu;
        c.range_m = sg.range_est_m;
        if (sg.by == airwar::SightBy::Rwr && sg.kind == airwar::SightKind::Emitter) {
            c.source = CueSource::Rwr;
            c.threat = table_->RwrClass(sg.what);
            if (sg.rwr == airwar::RwrState::Search) {
                AddStress(s, table_->stress_search, now);
                return false;  // a search paint: his stress, no reaction
            }
            if (sg.rwr != airwar::RwrState::Lock && sg.rwr != airwar::RwrState::Launch) return false;
            c.aimed = true;
            c.level = sg.rwr == airwar::RwrState::Launch ? 2 : 1;
            return true;
        }
        if (sg.by != airwar::SightBy::Eye) return false;
        c.source = CueSource::Eye;
        switch (sg.kind) {
            case airwar::SightKind::LaunchSign:
            case airwar::SightKind::Trail:
                c.threat = EyeSamClass(sg.what);
                c.level = 2;
                return true;
            case airwar::SightKind::Burst:
                c.threat = ThreatClass::FlakHeavy;
                return true;
            case airwar::SightKind::Tracer:
                c.threat = ThreatClass::FlakLight;
                return true;
            case airwar::SightKind::Aircraft:
                if (!sg.recognised || !table_->IsFighterModel(sg.what)) return false;
                c.threat = ThreatClass::Fighter;
                return true;
            default:
                return false;
        }
    }
    // The eye names a SAM by its size and smoke (perception_feed.hpp Launch's class): "large SAM", "shoulder SAM",
    // else a SAM of the Osa's size.
    static ThreatClass EyeSamClass(const std::string& what) {
        if (what.compare(0, 9, "large SAM") == 0) return ThreatClass::SamHigh;
        if (what.compare(0, 12, "shoulder SAM") == 0) return ThreatClass::SamIr;
        return ThreatClass::SamLow;
    }
    double StressOf(const ThreatCue& c) const {
        if (c.source == CueSource::Radio) return table_->stress_call;
        switch (c.threat) {
            case ThreatClass::FlakLight:
            case ThreatClass::FlakHeavy: return table_->stress_flak;
            case ThreatClass::Fighter: return table_->stress_fighter;
            case ThreatClass::Loss: return table_->stress_loss;
            default: return c.level >= 2 ? table_->stress_launch : table_->stress_lock;
        }
    }

    // He has the cue: his stress, then whether and when he reacts.
    void Notice(int target, Pilot& s, ThreatCue c, double now, const Context& ctx) {
        ++counts_.cues;
        c.serial = ++s.serial;
        AddStress(s, StressOf(c), now);
        const int sit = SituationOf(c.threat);
        const auto last = s.last.find(sit);
        if (last != s.last.end() && now - last->second.first < table_->refractory_s && c.level <= last->second.second) return;
        const Aircraft* a = ctx.air ? ctx.air(target) : nullptr;
        if (a == nullptr || !a->alive) return;
        if (s.profile->programme && IsSam(c.threat) && c.threat != ThreatClass::SamIr) {
            c.zone = ProgrammeZoneOf(s, *a, c);
            c.programmed = c.zone >= 0;
        }
        NoticeContext nc;
        nc.source = c.source;
        nc.two_seat = s.two_seat;
        nc.stress = Stress(target, now);
        nc.programmed = c.programmed;
        const double agl = ctx.agl_m ? ctx.agl_m(target) : 1000.0;
        nc.workload = (ctx.attacking && ctx.attacking(target)) || (agl < table_->workload_agl_m && a->speed_mps > table_->workload_mps);
        if (ReactionU(seed_, s.key, c.serial, ReactionDraw::NoReaction) < NoReactionChance(*table_, s.skill, nc)) {
            ++counts_.no_reaction;
            return;
        }
        s.last[sit] = {now, c.level};
        const double delay = NoticeDelayS(*table_, s.skill, nc, ReactionU(seed_, s.key, c.serial, ReactionDraw::Delay));
        c.dispense = DrawDispense(*table_, s.skill, ReactionU(seed_, s.key, c.serial, ReactionDraw::Dispense));
        Item it;
        it.kind = Item::Kind::Act;
        it.target = target;
        it.due_s = now + delay;
        it.cue = c;
        items_.push_back(it);
        if (c.dispense == Dispense::Early) DispenseEarly(target, s, c, now);  // at the cue, before the manoeuvre
    }

    // The zone of his programme his cue falls in: its believed place inside the zone (with the margin), or he inside it.
    int ProgrammeZoneOf(const Pilot& s, const Aircraft& a, const ThreatCue& c) const {
        for (std::size_t z = 0; z < s.zones.size(); ++z) {
            const ProgrammeZone& zn = s.zones[z];
            const double r = zn.radius_m + table_->programme_margin_m;
            const double dc = std::hypot(c.where.x - zn.centre.x, c.where.y - zn.centre.y);
            if (dc <= std::max(table_->programme_margin_m * 2.0, 0.25 * zn.radius_m)) return static_cast<int>(z);  // the site he was briefed on
            const double da = std::hypot(a.position.x - zn.centre.x, a.position.y - zn.centre.y);
            if (c.source == CueSource::Rwr && da <= r) {
                // A receiver's bearing toward the briefed site: within 30 degrees.
                const double b_cue = std::atan2(c.where.x - a.position.x, c.where.y - a.position.y);
                const double b_zone = std::atan2(zn.centre.x - a.position.x, zn.centre.y - a.position.y);
                double d = std::fabs(std::remainder(b_cue - b_zone, 2.0 * kPi));
                if (d < DegToRad(30.0)) return static_cast<int>(z);
            }
        }
        return -1;
    }

    void Run(const Item& it, double now, Flights& fl, const Context& ctx, std::vector<ReactionAct>& acts) {
        Pilot* s = P(it.target);
        if (s == nullptr) return;
        Aircraft* a = ctx.air ? ctx.air(it.target) : nullptr;
        if (a == nullptr || !a->alive) return;
        switch (it.kind) {
            case Item::Kind::Act: Act(it, *s, *a, now, fl, ctx, acts); return;
            case Item::Kind::Pull: Manoeuvre_(it.target, *s, *a, it.cue, Manoeuvre::Break, now, ctx, acts, "pulls hard now"); return;
            case Item::Kind::Dispense: DispenseNow(it.target, *s, it.cm, now, acts); return;
            case Item::Kind::Hear: {
                ++counts_.heard;
                ThreatCue c = it.cue;
                // The call's clock code: the bearing to the place called, with half an hour of the clock's error.
                const double b = std::atan2(c.where.x - a->position.x, c.where.y - a->position.y) +
                                 DegToRad(table_->call_bearing_sigma_deg) * ReactionN(seed_, s->key, s->serial + 1000003u, ReactionDraw::Bearing);
                const double r = std::max(1000.0, std::hypot(c.where.x - a->position.x, c.where.y - a->position.y));
                c.where = Vec3{a->position.x + std::sin(b) * r, a->position.y + std::cos(b) * r, c.where.z};
                c.range_m = r;
                c.aimed = false;
                c.noticed_s = now;
                Notice(it.target, *s, c, now, ctx);
                return;
            }
            case Item::Kind::Decision: {
                const int sit = SituationOf(it.cue.threat);
                s->heard_decision.insert(sit);
                if (it.decision.jettison) Jettison(it.target, *s, it.decision.tanks_only, now, "on his leader's call", acts);
                return;
            }
            case Item::Kind::SelfDecide: SelfDecide(it, *s, now, fl, acts); return;
        }
    }

    // His reaction: the manoeuvre (a late pull waits), the dispense, the call, and the flight's decision.
    void Act(const Item& it, Pilot& s, Aircraft& a, double now, Flights& fl, const Context& ctx, std::vector<ReactionAct>& acts) {
        ++counts_.reactions;
        const ThreatCue& c = it.cue;
        const ReactionRow& row = ChooseRow(*s.profile, c.source, c.threat);
        Manoeuvre m = row.manoeuvre;
        if (m == Manoeuvre::Programme && !c.programmed) m = Manoeuvre::Break;  // a site he was not briefed on: unplanned
        if (c.programmed) ++counts_.programmed;
        if (m == Manoeuvre::BreakLate && c.level >= 2) {
            const double wait = LateBreakWaitS(*table_, c.range_m);
            if (wait > 0.5) {
                Item pull;
                pull.kind = Item::Kind::Pull;
                pull.target = it.target;
                pull.due_s = now + wait;
                pull.cue = c;
                items_.push_back(pull);
                Event(acts, it.target, "watches the missile: he will pull late");
                m = Manoeuvre::None;
            } else {
                m = Manoeuvre::Break;
            }
        } else if (m == Manoeuvre::BreakLate) {
            m = Manoeuvre::Break;  // a lock, no missile yet: he breaks away from it
        }
        if (m != Manoeuvre::None) Manoeuvre_(it.target, s, a, c, m, now, ctx, acts, nullptr);
        const Countermeasure cm = CmFor(row.cm, c.threat);
        if (cm != Countermeasure::None && c.dispense != Dispense::Never && c.dispense != Dispense::Early) {
            if (c.dispense == Dispense::Late) {
                Item d;
                d.kind = Item::Kind::Dispense;
                d.target = it.target;
                d.due_s = now + table_->dispense_late_s;
                d.cm = cm;
                d.cue = c;
                items_.push_back(d);
            } else {
                DispenseNow(it.target, s, cm, now, acts);
            }
        }
        if (c.source != CueSource::Radio) Call(it.target, s, fl, c, now, ctx, a, nullptr);
        Decide(it.target, s, a, c, row, now, fl, ctx, acts);
    }

    // Chaff against a radar SAM, flares against an IR one; both to a row that says both.
    static Countermeasure CmFor(Countermeasure row, ThreatClass c) {
        if (row == Countermeasure::None) return row;
        if (c == ThreatClass::SamIr) return row == Countermeasure::Chaff ? Countermeasure::None : Countermeasure::Flares;
        if (IsSam(c)) return row == Countermeasure::Flares ? Countermeasure::None : Countermeasure::Chaff;
        return Countermeasure::None;
    }
    void DispenseEarly(int target, Pilot& s, const ThreatCue& c, double now) {
        const ReactionRow& row = ChooseRow(*s.profile, c.source, c.threat);
        const Countermeasure cm = CmFor(row.cm, c.threat);
        if (cm == Countermeasure::None) return;
        Item d;
        d.kind = Item::Kind::Dispense;
        d.target = target;
        d.due_s = now;
        d.cm = cm;
        d.cue = c;
        items_.push_back(d);
    }
    void DispenseNow(int target, Pilot& s, Countermeasure cm, double now, std::vector<ReactionAct>& acts) {
        if (cm == Countermeasure::Chaff || cm == Countermeasure::Both) {
            const int n = std::min(s.chaff, static_cast<int>(table_->chaff_salvo));
            if (n > 0) {
                s.chaff -= n;
                s.chaff_s = now;
                ++counts_.chaff;
                ReactionAct act;
                act.kind = ReactionAct::Kind::Chaff;
                act.target = target;
                act.count = n;
                act.what = "dispenses chaff";
                acts.push_back(act);
            }
        }
        if (cm == Countermeasure::Flares || cm == Countermeasure::Both) {
            const int n = std::min(s.flares, static_cast<int>(table_->flare_salvo));
            if (n > 0) {
                s.flares -= n;
                s.flare_s = now;
                ++counts_.flares;
                ReactionAct act;
                act.kind = ReactionAct::Kind::Flares;
                act.target = target;
                act.count = n;
                act.what = "fires flares";
                acts.push_back(act);
            }
        }
    }

    // The manoeuvre as the command layer's override (04_REACTIONS.md 6.3): each ends in a rejoin or the route again,
    // none in a beam point it flies on to.
    void Manoeuvre_(int target, Pilot& s, Aircraft& a, const ThreatCue& c, Manoeuvre m, double now, const Context& ctx,
                    std::vector<ReactionAct>& acts, const char* what) {
        ReactionAct act;
        act.kind = ReactionAct::Kind::Manoeuvre;
        act.target = target;
        PilotOverride& o = act.ov;
        o.after = PilotOverride::After::Rejoin;
        o.end = PilotOverride::End::Time;
        o.turn_rate_rad_s = std::max(DegToRad(table_->break_turn_deg_s), a.max_turn_rate_rad_s);
        Vec3 to{c.where.x - a.position.x, c.where.y - a.position.y, 0.0};
        to = to.LengthSquared() > 1.0 ? to.Normalized() : Vec3{0.0, 1.0, 0.0};
        Vec3 beam{-to.y, to.x, 0.0};
        if (Dot(beam, a.velocity) < 0.0) beam = -beam;
        const double agl = ctx.agl_m ? ctx.agl_m(target) : 1000.0;
        if (IsRotorcraft(a)) m = m == Manoeuvre::None ? m : Manoeuvre::Dive;  // a helicopter drops to the ground
        switch (m) {
            case Manoeuvre::None: return;
            case Manoeuvre::Break:
            case Manoeuvre::BreakLate:
                o.kind = PilotOverride::Kind::Break;
                o.point = a.position + beam * table_->extend_m;
                o.until_s = now + table_->break_s;
                act.what = what != nullptr ? what : "breaks away from the threat";
                break;
            case Manoeuvre::Dive:
                o.kind = PilotOverride::Kind::Dive;
                o.point = a.position + beam * table_->extend_m;
                o.until_s = now + table_->dive_s;
                act.dive = agl > table_->dive_agl_m;
                act.dive_agl_m = IsRotorcraft(a) ? std::min(table_->dive_agl_m, std::max(5.0, agl)) * 0.25 : table_->dive_agl_m;
                act.what = "breaks away and dives for the ground";
                break;
            case Manoeuvre::Jink: {
                o.kind = PilotOverride::Kind::Jink;
                Vec3 dir{a.velocity.x, a.velocity.y, 0.0};
                o.base_dir = dir.LengthSquared() > 0.0 ? dir.Normalized() : Vec3{0.0, 1.0, 0.0};
                o.sign = ReactionU(seed_, s.key, c.serial, ReactionDraw::Row) < 0.5 ? 1.0 : -1.0;
                o.period_s = JinkPeriodS(*table_, agl);
                o.flip_s = now + o.period_s;
                o.point = Flights::JinkPoint(o, a.position);
                o.until_s = now + table_->jink_s;
                act.what = "jinks";
                break;
            }
            case Manoeuvre::BreakInto:
                o.kind = PilotOverride::Kind::Break;
                o.point = Vec3{c.where.x, c.where.y, a.position.z};
                o.until_s = now + table_->break_into_s;
                act.what = "breaks into the fighter";
                break;
            case Manoeuvre::Extend:
                o.kind = PilotOverride::Kind::TurnAway;
                o.point = a.position - to * table_->extend_m;
                o.until_s = now + table_->extend_s;
                o.speed_mps = a.max_speed_mps;
                act.what = "runs from it at full speed";
                break;
            case Manoeuvre::Programme: {
                const ProgrammeZone& z = s.zones[static_cast<std::size_t>(std::max(0, c.zone))];
                Vec3 out{a.position.x - z.centre.x, a.position.y - z.centre.y, 0.0};
                out = out.LengthSquared() > 1.0 ? out.Normalized() : -to;
                o.kind = PilotOverride::Kind::TurnAway;
                o.end = PilotOverride::End::Point;
                o.point = Vec3{z.centre.x, z.centre.y, a.position.z} + out * (z.radius_m + table_->programme_margin_m);
                o.until_s = now + table_->programme_max_s;
                o.speed_mps = a.max_speed_mps;
                act.dive = agl > table_->dive_agl_m;
                act.dive_agl_m = table_->dive_agl_m;
                act.what = "flies his counter-SAM programme: full speed, down, out by the zone's nearest edge";
                break;
            }
        }
        acts.push_back(act);
    }

    // His call on the flight's net (5): his flight, and a package on one strike frequency, hear it, or not.
    void Call(int target, Pilot& s, const Flights& fl, const ThreatCue& c, double now, const Context& ctx, const Aircraft& a, const char* words_in) {
        if (!s.profile->calls) return;
        const int fi = fl.FlightOf(target);
        if (fi < 0) return;
        const FlightState& f = fl.All()[static_cast<std::size_t>(fi)];
        const FlightSlot* me = fl.SlotOf(target);
        const int number = me != nullptr ? me->number : 1;
        std::string words;
        if (words_in != nullptr) {
            words = std::string(f.callsign.empty() ? f.id : f.callsign) + " " + std::to_string(number) + ", " + words_in;
        } else {
            const double b = std::atan2(c.where.x - a.position.x, c.where.y - a.position.y);
            const char* what = IsSam(c.threat) ? "SAM" : IsFlak(c.threat) ? "flak" : c.threat == ThreatClass::Fighter ? "MIGs" : "lost one";
            words = std::string(what) + ", " + std::to_string(ClockOf(b, a.heading_rad)) + " o'clock";
        }
        ++counts_.calls;
        const int call = ctx.radio != nullptr ? ctx.radio->Say(f.id, number, false, words, now, true, c.where) : FlightRadio::kNoModel;
        ThreatCue heard = c;
        heard.source = CueSource::Radio;
        heard.programmed = false;
        heard.zone = -1;
        heard.level = 1;
        const auto hear = [&](int fj, std::size_t k, const FlightSlot& sl, double call_s) {
            if (sl.target == target || !sl.alive || sl.gone || P(sl.target) == nullptr) return;
            double at = -1.0;
            if (call == FlightRadio::kNoModel) {
                at = sl.hears ? now + call_s : -1.0;
            } else if (call >= 0 && ctx.radio != nullptr) {
                at = ctx.radio->Reaches(call, fj, static_cast<int>(k), now);
            }
            if (at < 0.0) return;
            Item h;
            h.kind = Item::Kind::Hear;
            h.target = sl.target;
            h.due_s = at;
            h.cue = heard;
            items_.push_back(h);
        };
        for (std::size_t k = 0; k < f.slots.size(); ++k) hear(fi, k, f.slots[k], f.call_s);
        if (!s.profile->package_net || f.package.empty() || !(IsSam(c.threat) || c.threat == ThreatClass::Fighter)) return;
        const std::vector<FlightState>& all = fl.All();
        for (std::size_t fj = 0; fj < all.size(); ++fj) {
            if (static_cast<int>(fj) == fi || all[fj].package != f.package || all[fj].weapons) continue;
            for (std::size_t k = 0; k < all[fj].slots.size(); ++k) {
                const FlightSlot& sl = all[fj].slots[k];
                const Aircraft* b = ctx.air ? ctx.air(sl.target) : nullptr;
                if (b == nullptr || (b->position - a.position).Length() > table_->package_call_range_m) continue;
                hear(static_cast<int>(fj), k, sl, all[fj].call_s);
            }
        }
    }

    // The flight's decision, once per situation, by its leader; a wingman waits for it.
    void Decide(int target, Pilot& s, Aircraft& a, const ThreatCue& c, const ReactionRow& row, double now, Flights& fl, const Context& ctx,
                std::vector<ReactionAct>& acts) {
        const int fi = fl.FlightOf(target);
        const int sit = SituationOf(c.threat);
        const bool leader = fi < 0 || fl.IsLeader(target);
        if (!leader) {
            if (s.heard_decision.count(sit) == 0) {
                Item w;
                w.kind = Item::Kind::SelfDecide;
                w.target = target;
                w.due_s = now + table_->decision_wait_s;
                w.cue = c;
                items_.push_back(w);
            }
            return;
        }
        if (fi >= 0 && fl.All()[static_cast<std::size_t>(fi)].weapons) return;
        const int key = fi >= 0 ? fi : -1 - target;  // a lone aircraft decides for himself
        if (decided_.count({key, sit}) != 0) return;
        const FlightDecision d = DecideFlight(*table_, ShiftedRow(row, s.press_on_shift, s.jettison_shift), Stress(target, now), s.nerve,
                                              ReactionU(seed_, s.key, c.serial, ReactionDraw::Jettison),
                                              ReactionU(seed_, s.key, c.serial, ReactionDraw::TanksOnly),
                                              ReactionU(seed_, s.key, c.serial, ReactionDraw::Choice));
        Decided rec;
        rec.choice = d.choice;
        rec.jettison = d.jettison;
        decided_[{key, sit}] = rec;
        s.heard_decision.insert(sit);
        ++counts_.decisions;
        if (fi < 0) {  // a lone aircraft outside any flight: his own decision
            if (d.jettison) Jettison(target, s, d.tanks_only, now, "his own decision", acts);
            if (d.choice == FlightChoice::Abort) Abort(target, now, acts, "turns for home");
            return;
        }
        const FlightState& f = fl.All()[static_cast<std::size_t>(fi)];
        const std::string name = f.callsign.empty() ? f.id : f.callsign;
        if (d.jettison) {
            Jettison(target, s, d.tanks_only, now, "his own call", acts);
            const std::string words = name + ", jettison " + (d.tanks_only ? "tanks" : "stores");
            ++counts_.calls;
            const int call = ctx.radio != nullptr ? ctx.radio->Say(f.id, f.slots[static_cast<std::size_t>(f.leader_slot)].number, false, words, now, false, Vec3())
                                                  : FlightRadio::kNoModel;
            for (std::size_t k = 0; k < f.slots.size(); ++k) {
                const FlightSlot& sl = f.slots[k];
                if (sl.target == target || !sl.alive || sl.gone || sl.detached) continue;
                double at = -1.0;
                if (call == FlightRadio::kNoModel) at = sl.hears ? now + f.call_s : -1.0;
                else if (call >= 0) at = ctx.radio->Reaches(call, fi, static_cast<int>(k), now);
                if (at < 0.0) continue;
                Item h;
                h.kind = Item::Kind::Decision;
                h.target = sl.target;
                h.due_s = at;
                h.cue = c;
                h.decision = d;
                items_.push_back(h);
            }
            Event(acts, target, words);
        }
        if (d.choice == FlightChoice::PressOn) {
            ++counts_.press_on;
            Event(acts, target, "the leader presses on");
            NoteHeard(fl, fi, target, sit, now, d, false);
            return;
        }
        if (d.choice == FlightChoice::Abort) {
            ++counts_.aborts;
            FlightOrder home;
            home.kind = OrderKind::ReturnHome;
            home.by = "leader";
            if (fl.Order(fi, home, now)) {
                Event(acts, target, "the leader aborts: the flight turns for home");
                NoteHeard(fl, fi, target, sit, now, d, true);
            }
            return;
        }
        // Go round the zone he believes in, then the rest of the route; inside it (his target there) he presses on.
        std::vector<RouteLeg> legs;
        if (!GoRoundLegs(fl, fi, target, a, c, legs)) {
            ++counts_.press_on;
            decided_[{key, sit}].choice = FlightChoice::PressOn;
            Event(acts, target, "the leader presses on (no way round)");
            NoteHeard(fl, fi, target, sit, now, d, false);
            return;
        }
        ++counts_.go_round;
        FlightOrder round;
        round.kind = OrderKind::Divert;
        round.legs = std::move(legs);
        round.by = "leader";
        if (fl.Order(fi, round, now)) {
            Event(acts, target, "the leader takes the flight round the zone");
            NoteHeard(fl, fi, target, sit, now, d, true);
        }
    }

    // Who heard the leader's decision: with an order, each member it will reach (the flights' own calls); without one
    // (pressing on), all who hear the net.
    void NoteHeard(const Flights& fl, int fi, int leader, int sit, double now, const FlightDecision& d, bool ordered) {
        const FlightState& f = fl.All()[static_cast<std::size_t>(fi)];
        for (const FlightSlot& sl : f.slots) {
            if (sl.target == leader || !sl.alive || sl.gone || sl.detached || P(sl.target) == nullptr) continue;
            double at = -1.0;
            if (ordered) {
                if (!sl.calls.empty()) at = sl.calls.back().first;
            } else if (sl.hears) {
                at = now + f.call_s;
            }
            if (at < 0.0) continue;
            Item h;
            h.kind = Item::Kind::Decision;
            h.target = sl.target;
            h.due_s = at;
            h.cue.threat = sit == 0 ? ThreatClass::SamLow : sit == 1 ? ThreatClass::FlakLight : sit == 2 ? ThreatClass::Fighter : ThreatClass::Loss;
            h.decision = d;
            h.decision.jettison = false;  // the jettison came with its own call
            items_.push_back(h);
        }
    }

    // A wingman who heard nothing from his leader decides on his own row: his jettison, his own turn for home.
    void SelfDecide(const Item& it, Pilot& s, double now, const Flights& fl, std::vector<ReactionAct>& acts) {
        const int sit = SituationOf(it.cue.threat);
        if (s.heard_decision.count(sit) != 0 || s.self_decided.count(sit) != 0) return;
        const FlightSlot* sl = fl.SlotOf(it.target);
        if (sl != nullptr && sl->detached) return;
        s.self_decided.insert(sit);
        ++counts_.self_decided;
        const ReactionRow& row = ChooseRow(*s.profile, it.cue.source, it.cue.threat);
        const std::uint64_t k = it.cue.serial + 7919u;
        const FlightDecision d = DecideFlight(*table_, ShiftedRow(row, s.press_on_shift, s.jettison_shift), Stress(it.target, now), s.nerve,
                                              ReactionU(seed_, s.key, k, ReactionDraw::Jettison),
                                              ReactionU(seed_, s.key, k, ReactionDraw::TanksOnly), ReactionU(seed_, s.key, k, ReactionDraw::Choice));
        if (d.jettison) Jettison(it.target, s, d.tanks_only, now, "his own decision, nothing heard from the lead", acts);
        if (d.choice == FlightChoice::Abort) Abort(it.target, now, acts, "nothing heard from the lead: turns for home alone");
    }

    void Jettison(int target, Pilot& s, bool tanks_only, double now, const char* why, std::vector<ReactionAct>& acts) {
        const bool stores = !tanks_only && s.stores;
        if (s.tanks <= 0 && !stores) return;
        JettisonRecord r;
        r.target = target;
        r.t_s = now;
        r.tanks = s.tanks;
        r.stores = stores;
        r.why = why;
        jettisons_.push_back(r);
        ++counts_.jettisons;
        if (s.tanks > 0) {
            ReactionAct t;
            t.kind = ReactionAct::Kind::JettisonTanks;
            t.target = target;
            t.count = s.tanks;
            t.what = std::string("jettisons his tanks (") + why + ")";
            acts.push_back(t);
            s.tanks = 0;
        }
        if (stores) {
            ReactionAct t;
            t.kind = ReactionAct::Kind::JettisonStores;
            t.target = target;
            t.what = std::string("jettisons his stores (") + why + ")";
            acts.push_back(t);
            s.stores = false;
            s.stores_jettisoned = true;
        }
    }
    static void Abort(int target, double now, std::vector<ReactionAct>& acts, const char* what) {
        (void)now;
        ReactionAct a;
        a.kind = ReactionAct::Kind::Abort;
        a.target = target;
        a.what = what;
        acts.push_back(a);
    }
    static void Event(std::vector<ReactionAct>& acts, int target, const std::string& what) {
        ReactionAct a;
        a.kind = ReactionAct::Kind::Event;
        a.target = target;
        a.what = what;
        acts.push_back(a);
    }

    // Round the believed zone: a point abeam its centre on his side, its radius and a margin out, then the rest of the
    // leader's route. False when there is no route left or its next point lies inside the zone (his target there).
    bool GoRoundLegs(const Flights& fl, int fi, int target, const Aircraft& a, const ThreatCue& c, std::vector<RouteLeg>& legs) const {
        const double radius = table_->GoRoundRadiusM(c.threat);
        if (radius <= 0.0 || a.next_waypoint >= a.waypoints.size()) return false;
        const double keep = radius + table_->go_round_margin_m;
        const Vec3 centre{c.where.x, c.where.y, 0.0};
        const Vec3& next = a.waypoints[a.next_waypoint];
        if (std::hypot(next.x - centre.x, next.y - centre.y) < keep) return false;
        Vec3 track{next.x - a.position.x, next.y - a.position.y, 0.0};
        track = track.LengthSquared() > 1.0 ? track.Normalized() : Vec3{0.0, 1.0, 0.0};
        Vec3 side{-track.y, track.x, 0.0};
        if (Dot(side, Vec3{a.position.x - centre.x, a.position.y - centre.y, 0.0}) < 0.0) side = -side;
        RouteLeg round;
        round.to = Vec3{centre.x + side.x * keep, centre.y + side.y * keep, a.position.z};
        round.tag = "defensive";
        legs.push_back(round);
        const FlightSlot* sl = fl.SlotOf(target);
        (void)fi;
        for (std::size_t k = a.next_waypoint; k < a.waypoints.size(); ++k) {
            RouteLeg l;
            if (sl != nullptr && k < sl->legs.size()) l = sl->legs[k];
            l.to = a.waypoints[k];
            legs.push_back(l);
        }
        return true;
    }

    const ReactionTable* table_ = nullptr;
    std::uint64_t seed_ = 0;
    int year_ = 1985;
    std::vector<Pilot> pilots_;
    struct ScriptedRow {
        bool on = false;
        std::string id;
        EvadeDef::On cue = EvadeDef::On::Launch;
    };
    const ScriptedRow* SR(int target) const {
        if (target < 0 || static_cast<std::size_t>(target) >= scripted_.size()) return nullptr;
        const ScriptedRow& r = scripted_[static_cast<std::size_t>(target)];
        return r.on ? &r : nullptr;
    }
    std::vector<ScriptedRow> scripted_;
    std::vector<Item> items_;
    std::map<std::pair<int, int>, Decided> decided_;  // (flight, situation) -> the leader's decision
    std::vector<JettisonRecord> jettisons_;
    Counts counts_;
};

}  // namespace simcore
