// simcore/flights.hpp — flights and packages kept in the session, and the command layer between the pilot and the
// aircraft (roadmap B1, docs/air_battle/02_FLIGHTS_AND_PILOTS.md §1-2).
//
// The layers, top down:
//   RoutePlan      the briefed route: legs with a speed, a height and a tag (the phase); never overwritten
//   FlightOrder    what the flight is doing now, and who ordered it
//   slot keeping   a wingman's station on his leader, by the formation of the phase
//   PilotOverride  a pilot's own manoeuvre (break, dive, jink, turn away), which ends by its own rule
//   AircraftDemand what the aircraft is told to fly (aircraft.hpp): a point or a heading, a speed, a turn rate
// A flight of one flying its route has no demand at all: its aircraft follows its own waypoints, as every hand-made
// scenario's target always has. Nothing here overwrites Aircraft::waypoints; an order that follows the plan moves only
// the place on it (Aircraft::next_waypoint: FollowRoute from a leg, ReturnHome to the last).
//
// One decision-maker per flight (§1.4): the leader. A flight act (an order) reaches the leader at once and each member
// after the call's time on the flight's net (call_s; roadmap B9 makes the call an emission); a member who does not hear
// it keeps the last order he heard. A pilot's own reflex (an override) does not wait. A package turns back once the
// losses its commander knows of reach his nerve, the abstract resolver's rule (air_war/resolve.hpp, PackageNerve).
// When the leader is lost the element lead (No. 3) takes the flight (§1.5).
//
// Who may order a flight (C2Authority::May, docs/air_battle/05_C2_AND_RETASKING.md §4.2) is roadmap B5: Flights::May
// accepts every order until then.
//
// All numbers here are game values (sam3d). No randomness: a session replays the same.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/aircraft.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

enum class OrderKind { FollowRoute, Hold, Orbit, Attack, Rejoin, ReturnHome, Divert, Evade };

inline const char* ToString(OrderKind k) {
    switch (k) {
        case OrderKind::FollowRoute: return "follow the route";
        case OrderKind::Hold: return "hold";
        case OrderKind::Orbit: return "orbit";
        case OrderKind::Attack: return "attack";
        case OrderKind::Rejoin: return "rejoin";
        case OrderKind::ReturnHome: return "return home";
        case OrderKind::Divert: return "divert";
        case OrderKind::Evade: return "evade";
    }
    return "?";
}
inline const char* OrderKindId(OrderKind k) {
    switch (k) {
        case OrderKind::FollowRoute: return "follow_route";
        case OrderKind::Hold: return "hold";
        case OrderKind::Orbit: return "orbit";
        case OrderKind::Attack: return "attack";
        case OrderKind::Rejoin: return "rejoin";
        case OrderKind::ReturnHome: return "return_home";
        case OrderKind::Divert: return "divert";
        case OrderKind::Evade: return "evade";
    }
    return "?";
}

// One leg of a route: flown to `to`, at its speed (0: the target's own), its height above the ground (< 0: the
// behaviour's) or above sea level (has_alt), and its tag: the phase (transit, ingress, attack, egress, defensive,
// orbit) or the planner's point tag it is read from (PhaseOfTag).
struct RouteLeg {
    Vec3 to;
    double speed_mps = 0.0;
    double agl_m = -1.0;
    bool has_alt = false;
    double alt_m = 0.0;
    std::string tag;
};
struct RoutePlan {
    std::vector<RouteLeg> legs;
};

// The phase a route tag is flown in: the phases themselves, or the air war planner's point tags (planner.hpp
// MainRoute): to the entry transit; to the IP or the hide ingress; the attack's points and the release attack; out
// egress; orbits and loiters orbit.
inline std::string PhaseOfTag(const std::string& tag) {
    static const char* const kPhases[] = {"transit", "ingress", "attack", "egress", "defensive", "orbit"};
    for (const char* p : kPhases) {
        if (tag == p) return tag;
    }
    if (tag == "ip" || tag == "hide") return "ingress";
    if (tag == "pull_up" || tag == "apex" || tag == "roll_in" || tag == "target" || tag == "dive" || tag == "launch") return "attack";
    if (tag == "egress" || tag == "exit") return "egress";
    if (tag == "loiter") return "orbit";
    return "transit";
}

struct FlightOrder {
    OrderKind kind = OrderKind::FollowRoute;
    int from_leg = -1;               // FollowRoute: the leg to resume (-1: where it is)
    std::vector<RouteLeg> legs;      // Divert: a new route from here
    bool has_heading = false;        // Divert without legs: a heading to fly (a vector), until told otherwise
    double heading_rad = 0.0;
    bool has_aim = false;            // Attack: the aim; Orbit, Hold: the centre (Hold without: where it is); Evade: the threat
    Vec3 aim;
    double radius_m = 0.0;           // Orbit, Hold: the circle's radius (0: kOrbitRadiusM)
    double speed_mps = 0.0;          // > 0: the speed ordered (else the plan's)
    double until_s = -1.0;           // Hold, Orbit, Evade: until then (< 0: until told otherwise), then the route again
    std::string by;                  // "leader", "package", "pilot", "instructor", a ground node's id
    double at_s = 0.0;               // when it was given
};

// A pilot's own manoeuvre: he flies for `point` at his hard turn rate until its end, then back to his flight's slot or
// his route (Rejoin), or out of the session at the point (Home: turning away with his weapons gone; a leader's flight
// goes home behind him). End::Point ends it where the point is passed as a waypoint is (AircraftDemand::capture).
struct PilotOverride {
    enum class Kind { Break, Dive, Jink, TurnAway };
    enum class End { Time, Point };  // at until_s; or at the point (until_s the latest)
    enum class After { Rejoin, Home };
    Kind kind = Kind::Break;
    End end = End::Time;
    After after = After::Rejoin;
    Vec3 point;
    double turn_rate_rad_s = 0.0;
    double until_s = 0.0;
    // A jink: hard turns left and right of base_dir, 60 degrees, flipped every period_s.
    Vec3 base_dir;
    double sign = 1.0;
    double period_s = 4.0;
    double flip_s = 0.0;
};
inline const char* ToString(PilotOverride::Kind k) {
    switch (k) {
        case PilotOverride::Kind::Break: return "break";
        case PilotOverride::Kind::Dive: return "dive";
        case PilotOverride::Kind::Jink: return "jink";
        case PilotOverride::Kind::TurnAway: return "turn away";
    }
    return "?";
}

namespace flights {
constexpr double kCallS = 3.0;              // a call on the flight's net, said and acted on by a member
constexpr double kLeadS = 4.0;              // a wingman flies for a point this far ahead of his station
constexpr double kLeadMinM = 600.0;
constexpr double kCloseGainPerS = 0.05;     // m/s more (or less) than the leader per m he is behind (ahead of) his station
constexpr double kCloseShare = 0.3;         // at most this share of the leader's speed faster or slower
constexpr double kContactLostM = 10000.0;   // farther than this from his station he has lost the leader: his own plan
constexpr double kContactRegainM = 6000.0;  // and closer than this he has him again
constexpr double kOrbitRadiusM = 4000.0;
constexpr double kOrbitLeadRad = 0.5;       // flies for a point this far round the circle ahead
constexpr double kAttackCaptureM = 1000.0;
constexpr double kAwayPointM = 20000.0;     // a heading flown as a point this far along it
constexpr double kVectorM = 50000.0;
constexpr double kJinkTurnRad = DegToRad(60.0);
constexpr double kJinkLegM = 20000.0;
}  // namespace flights

// A formation's slot for a place in a phase: [right, back, up] of the leader (data/air_war/faction_*.json "flights").
// Without the phase, the transit's; a place past the table's repeats it further back. Without any formation, an echelon
// right, kEchelonM apart (a game value).
namespace flights {
constexpr double kEchelonM = 800.0;
constexpr double kRepeatBackM = 1500.0;  // a place past the table's: the table again, this far behind its deepest
}  // namespace flights
inline Vec3 FormationSlot(const std::map<std::string, std::vector<Vec3>>& forms, const std::string& phase, int place) {
    const std::size_t p = static_cast<std::size_t>(std::max(0, place));
    auto it = forms.find(phase);
    if (it == forms.end() || it->second.empty()) it = forms.find("transit");
    if (it == forms.end() || it->second.empty()) {
        it = forms.begin();
        while (it != forms.end() && it->second.empty()) ++it;
    }
    if (it == forms.end()) return Vec3{flights::kEchelonM * static_cast<double>(p), 0.0, 0.0};
    const std::vector<Vec3>& table = it->second;
    const std::size_t n = table.size();
    if (p < n) return table[p];
    double depth = 0.0;
    for (const Vec3& v : table) depth = std::max(depth, v.y);
    const Vec3 base = table[p % n];
    return Vec3{base.x, base.y + static_cast<double>(p / n) * (depth + flights::kRepeatBackM), base.z};
}

// A record of an order given to a flight (or to one aircraft of it, target >= 0), for the debrief and the bridge.
struct OrderRecord {
    double t_s = 0.0;
    OrderKind kind = OrderKind::FollowRoute;
    std::string by;
    bool has_aim = false;
    Vec3 aim;
    int target = -1;
};

struct FlightSlot {
    int target = -1;            // the runner's target index
    int number = 1;             // 1 the leader, 2 his wingman, 3 the element lead, 4 his wingman
    Vec3 offset_m;              // right, back and up of the leader where they started (the formation without data)
    bool alive = true;          // not shot down
    bool gone = false;          // left the session
    bool hears = true;          // the flight's net reaches it (B9: jammed, out of range)
    bool detached = false;      // flies on its own: going home damaged, held by the instructor
    int place = 0;              // its place in the formation's slots (0: the leader's)
    FlightOrder order;          // the last order it heard (the leader's: the flight's)
    bool order_fresh = false;   // not yet put to its aircraft
    std::size_t order_leg = 0;  // Divert: the leg of the order's it is on
    std::vector<std::pair<double, FlightOrder>> calls;  // orders said on the net, reaching it at those times
    bool overriding = false;
    PilotOverride ov;
    bool in_contact = true;     // the leader in sight (else it flies its own plan until it closes again)
    bool keeping = false;       // flew its station on the leader this step
    std::vector<RouteLeg> legs;  // its own plan's legs (speed and tag per waypoint; empty: the target's speed throughout)
    std::size_t speed_leg = static_cast<std::size_t>(-1);  // the leg its speed was last set for
};

struct FlightState {
    std::string id;
    std::string callsign;
    std::string package;
    std::string side = "enemy";  // "enemy" or "ours"
    std::vector<FlightSlot> slots;
    int leader_slot = 0;         // -1: none left
    FlightOrder order;
    RoutePlan plan;              // the briefed route: its first leader's legs
    std::string phase = "transit";
    bool keep_slots = true;      // false: each flies its own plan (a stream of weapons)
    bool weapons = false;        // no crew (cruise missiles, decoy drones): never turns back, never a loss
    double call_s = flights::kCallS;
    // The formation by phase: [right, back, up] of the leader for each place (data/air_war/faction_*.json "flights").
    std::map<std::string, std::vector<Vec3>> formations;
    std::vector<OrderRecord> orders;  // every order given to it, with who gave it

    const FlightSlot* Leader() const {
        return leader_slot >= 0 && leader_slot < static_cast<int>(slots.size()) ? &slots[static_cast<std::size_t>(leader_slot)] : nullptr;
    }
    int Alive() const {
        int n = 0;
        for (const FlightSlot& s : slots) n += s.alive ? 1 : 0;
        return n;
    }
};

struct PackageState {
    std::string id;
    int commander_flight = -1;     // index into the flights
    std::vector<int> flights;
    int size = 0;                  // aircraft in it (with those outside the session)
    int lost_before = 0;           // lost before it came into the session (known)
    int losses_known = 0;          // what its commander knows of
    double turn_back_share = 0.4;  // his nerve (resolve.hpp PackageNerve)
    bool turned_back = false;
    std::vector<double> reports;   // when each loss reaches him
};

// The flights' calls on the air (roadmap B9, air_war/air_radio.hpp implements it): a leader's act is a transmission with its
// turn on the flight's net, and a member acts on it only if he heard it (or was signalled, in a flight flying silent).
// Without one the call is B1's fixed delay (`FlightState::call_s`) to every member the net reaches (`FlightSlot::hears`).
struct FlightRadio {
    static constexpr int kNoModel = -2;  // Say: no radio model for this order (the fixed delay)
    static constexpr int kSignalled = -1;  // Say: not said on the air (silent): passed by signal to those who see the leader
    static constexpr int kDropped = -3;  // Say: said too late to be of use: nobody heard it
    virtual ~FlightRadio() = default;
    // The order `o` given to flight `fi` at t: its call's number (>= 0), or one of the codes above.
    virtual int Say(int fi, const FlightState& f, const FlightOrder& o, double t) = 0;
    // When member `slot` acts on the call (t_s; `react_s` after he heard it), or < 0 if he did not hear it.
    virtual double Reaches(int call, int fi, int slot, double t, double react_s) = 0;
};

class Flights {
public:
    void SetRadio(FlightRadio* radio) { radio_ = radio; }
    FlightRadio* Radio() const { return radio_; }

    // The aircraft of a target in the sky now (spawned, not gone, alive), else null.
    using Lookup = std::function<Aircraft*(int target)>;
    struct Note {
        std::string who;
        std::string what;
    };

    // Who may give a flight an order (roadmap B5, C2Authority::May): every order is accepted until then.
    static bool May(const FlightState&, const FlightOrder&) { return true; }

    int Add(FlightState f) {
        std::stable_sort(f.slots.begin(), f.slots.end(), [](const FlightSlot& a, const FlightSlot& b) { return a.number < b.number; });
        for (std::size_t k = 0; k < f.slots.size(); ++k) {
            f.slots[k].place = static_cast<int>(k);
            f.slots[k].order = f.order;
        }
        f.leader_slot = f.slots.empty() ? -1 : 0;
        if (f.plan.legs.empty() && !f.slots.empty()) f.plan.legs = f.slots[0].legs;
        const int fi = static_cast<int>(flights_.size());
        for (std::size_t k = 0; k < f.slots.size(); ++k) {
            const int t = f.slots[k].target;
            if (t < 0) continue;
            if (static_cast<std::size_t>(t) >= of_target_.size()) of_target_.resize(static_cast<std::size_t>(t) + 1, {-1, -1});
            of_target_[static_cast<std::size_t>(t)] = {fi, static_cast<int>(k)};
        }
        flights_.push_back(std::move(f));
        return fi;
    }
    int AddPackage(PackageState p) {
        if (p.size <= 0) {
            for (int fi : p.flights) {
                if (fi < 0 || fi >= static_cast<int>(flights_.size()) || flights_[static_cast<std::size_t>(fi)].weapons) continue;
                p.size += static_cast<int>(flights_[static_cast<std::size_t>(fi)].slots.size());
            }
            p.size += p.lost_before;
        }
        p.losses_known = p.lost_before;
        packages_.push_back(std::move(p));
        return static_cast<int>(packages_.size()) - 1;
    }

    std::size_t Size() const { return flights_.size(); }
    const std::vector<FlightState>& All() const { return flights_; }
    const std::vector<PackageState>& Packages() const { return packages_; }
    int Index(const std::string& id) const {
        for (std::size_t i = 0; i < flights_.size(); ++i) {
            if (flights_[i].id == id) return static_cast<int>(i);
        }
        return -1;
    }
    const FlightState* Find(const std::string& id) const {
        const int i = Index(id);
        return i < 0 ? nullptr : &flights_[static_cast<std::size_t>(i)];
    }
    int FlightOf(int target) const {
        return target >= 0 && static_cast<std::size_t>(target) < of_target_.size() ? of_target_[static_cast<std::size_t>(target)].first : -1;
    }
    const FlightSlot* SlotOf(int target) const {
        const std::pair<int, int> fs = Where(target);
        return fs.first < 0 ? nullptr : &flights_[static_cast<std::size_t>(fs.first)].slots[static_cast<std::size_t>(fs.second)];
    }
    bool IsLeader(int target) const {
        const std::pair<int, int> fs = Where(target);
        return fs.first >= 0 && flights_[static_cast<std::size_t>(fs.first)].leader_slot == fs.second;
    }
    std::vector<std::string> FlightsOf(const std::string& package) const {
        std::vector<std::string> out;
        for (const FlightState& f : flights_) {
            if (!package.empty() && f.package == package) out.push_back(f.id);
        }
        return out;
    }

    // A flight act, by the leader (or given to him from outside: `o.by`): he flies it now; each member who hears the
    // call on the flight's net flies it after call_s; one who does not keeps the last order he heard. False: refused.
    bool Order(int fi, FlightOrder o, double t) {
        if (fi < 0 || fi >= static_cast<int>(flights_.size())) return false;
        FlightState& f = flights_[static_cast<std::size_t>(fi)];
        if (!May(f, o)) return false;
        o.at_s = t;
        Record(f, o, -1);
        f.order = o;
        const int call = radio_ != nullptr ? radio_->Say(fi, f, o, t) : FlightRadio::kNoModel;
        for (std::size_t k = 0; k < f.slots.size(); ++k) {
            FlightSlot& s = f.slots[k];
            if (!s.alive || s.gone || s.detached) continue;
            if (static_cast<int>(k) == f.leader_slot) {
                Hear(s, o);
            } else if (s.hears) {
                const double at = call == FlightRadio::kNoModel ? t + f.call_s : radio_->Reaches(call, fi, static_cast<int>(k), t, f.call_s);
                if (at >= 0.0) s.calls.push_back({at, o});  // a member who did not hear it keeps the last order he heard
            }
        }
        return true;
    }
    // An order to one aircraft of a flight: the instructor's to the one he holds (a flight of one for the trainer), a
    // damaged one's own turn for home. `detach`: it leaves its flight's formation (the leader's place passes on).
    bool OrderMember(int target, FlightOrder o, double t, bool detach, const char* why = "the lead leaves the flight") {
        const std::pair<int, int> fs = Where(target);
        if (fs.first < 0) return false;
        FlightState& f = flights_[static_cast<std::size_t>(fs.first)];
        FlightSlot& s = f.slots[static_cast<std::size_t>(fs.second)];
        if (!May(f, o)) return false;
        o.at_s = t;
        Record(f, o, target);
        Hear(s, o);
        if (detach && !s.detached) {
            s.detached = true;
            if (f.leader_slot == fs.second) Succession(f, nullptr, why);
        }
        return true;
    }
    // Back into its flight's formation, on the flight's order (the instructor let it go).
    void Rejoin(int target) {
        const std::pair<int, int> fs = Where(target);
        if (fs.first < 0) return;
        FlightState& f = flights_[static_cast<std::size_t>(fs.first)];
        FlightSlot& s = f.slots[static_cast<std::size_t>(fs.second)];
        s.detached = false;
        if (f.leader_slot < 0) f.leader_slot = fs.second;
        Replace(f);
        Hear(s, f.order);
    }
    void Override(int target, const PilotOverride& o) {
        const std::pair<int, int> fs = Where(target);
        if (fs.first < 0) return;
        FlightSlot& s = flights_[static_cast<std::size_t>(fs.first)].slots[static_cast<std::size_t>(fs.second)];
        s.overriding = true;
        s.ov = o;
    }
    void EndOverride(int target) {
        const std::pair<int, int> fs = Where(target);
        if (fs.first >= 0) flights_[static_cast<std::size_t>(fs.first)].slots[static_cast<std::size_t>(fs.second)].overriding = false;
    }
    bool Overriding(int target) const {
        const FlightSlot* s = SlotOf(target);
        return s != nullptr && s->overriding;
    }
    void SetHears(int target, bool hears) {
        const std::pair<int, int> fs = Where(target);
        if (fs.first >= 0) flights_[static_cast<std::size_t>(fs.first)].slots[static_cast<std::size_t>(fs.second)].hears = hears;
    }

    // Shot down: its flight's leader passes on, and its package's commander hears of it (at once in his own flight,
    // after the call from a survivor of its flight in another; a flight lost whole is not heard of).
    void Down(int target, double t, std::vector<Note>* notes = nullptr) {
        const std::pair<int, int> fs = Where(target);
        if (fs.first < 0) return;
        FlightState& f = flights_[static_cast<std::size_t>(fs.first)];
        FlightSlot& s = f.slots[static_cast<std::size_t>(fs.second)];
        if (!s.alive) return;
        s.alive = false;
        s.overriding = false;
        s.calls.clear();
        if (f.leader_slot == fs.second) Succession(f, notes);
        if (f.weapons) return;
        for (PackageState& p : packages_) {
            if (std::find(p.flights.begin(), p.flights.end(), fs.first) == p.flights.end()) continue;
            if (CommanderFlight(p) == fs.first) {
                p.reports.push_back(t);
            } else if (f.Alive() > 0) {
                p.reports.push_back(t + f.call_s);
            }
        }
    }
    // Left the session (past its last waypoint, a weapon at its impact).
    void Gone(int target) {
        const std::pair<int, int> fs = Where(target);
        if (fs.first < 0) return;
        FlightSlot& s = flights_[static_cast<std::size_t>(fs.first)].slots[static_cast<std::size_t>(fs.second)];
        s.gone = true;
        s.overriding = false;
    }

    // Before the world moves: every aircraft of every flight told what to fly (its demand), or nothing (its route).
    void Steer(double t, const Lookup& air) {
        for (FlightState& f : flights_) {
            if (f.leader_slot >= 0 && f.slots[static_cast<std::size_t>(f.leader_slot)].gone) Succession(f, nullptr, "the lead has left");
            Aircraft* L = nullptr;
            const FlightSlot* ls = f.Leader();
            if (ls != nullptr && ls->alive && !ls->gone) L = air(ls->target);
            if (L != nullptr) f.phase = PhaseNow(f, *ls, *L);
            for (std::size_t k = 0; k < f.slots.size(); ++k) {
                FlightSlot& s = f.slots[k];
                s.keeping = false;
                if (!s.alive || s.gone) continue;
                Aircraft* A = air(s.target);
                if (A == nullptr) continue;
                A->demand = AircraftDemand();
                if (s.overriding) {
                    A->demand.active = true;
                    A->demand.point = s.ov.point;
                    A->demand.turn_rate_rad_s = s.ov.turn_rate_rad_s;
                    A->demand.capture = s.ov.end == PilotOverride::End::Point;
                    continue;
                }
                // In the attack each flies his own run (his plan's attack points, on the leader's track, in trail: the
                // flight splits before the target, dubrov_1983) and takes his slot again after it.
                const bool wingman = static_cast<int>(k) != f.leader_slot && !s.detached && f.keep_slots && L != nullptr &&
                                     !f.slots[static_cast<std::size_t>(f.leader_slot)].overriding && f.phase != "attack";
                if (wingman && KeepSlot(f, s, *L, *A)) continue;
                FlyOrder(f, s, static_cast<int>(k) == f.leader_slot, *A, t);
            }
        }
    }

    // After the world moved one aircraft (before its pilot's height layer): its override's turns and end, a wingman's
    // place on the plan kept with his leader's, and the plan's speed for the leg it is on. Returns what happened for the
    // debrief ("" if nothing); `ended`: its override ended (its pilot stops evading).
    std::string AfterMove(int target, Aircraft& a, double t, const Lookup& air, bool& ended) {
        ended = false;
        const std::pair<int, int> fs = Where(target);
        if (fs.first < 0) return std::string();
        FlightState& f = flights_[static_cast<std::size_t>(fs.first)];
        FlightSlot& s = f.slots[static_cast<std::size_t>(fs.second)];
        std::string event;
        if (s.overriding) event = StepOverride(f, s, fs.second, a, t, ended);
        if (s.keeping) {
            const FlightSlot* ls = f.Leader();
            const Aircraft* L = ls != nullptr ? air(ls->target) : nullptr;
            if (L != nullptr && L->waypoints.size() == a.waypoints.size()) a.next_waypoint = L->next_waypoint;
        }
        if (!s.legs.empty() && a.next_waypoint < s.legs.size() && a.next_waypoint != s.speed_leg) {
            s.speed_leg = a.next_waypoint;
            if (s.legs[a.next_waypoint].speed_mps > 0.0) a.speed_mps = s.legs[a.next_waypoint].speed_mps;
        }
        return event;
    }

    // After the step: the calls heard, the losses reported to the package commanders and their nerve.
    std::vector<Note> After(double t) {
        std::vector<Note> notes;
        for (FlightState& f : flights_) {
            for (FlightSlot& s : f.slots) {
                if (s.calls.empty()) continue;
                std::size_t kept = 0;
                for (std::size_t c = 0; c < s.calls.size(); ++c) {
                    if (s.calls[c].first <= t + 1e-9) {
                        if (s.alive && !s.gone && !s.detached) Hear(s, s.calls[c].second);
                    } else {
                        s.calls[kept++] = s.calls[c];
                    }
                }
                s.calls.resize(kept);
            }
        }
        for (std::size_t pi = 0; pi < packages_.size(); ++pi) {
            PackageState& p = packages_[pi];
            int known = p.lost_before;
            for (double r : p.reports) known += r <= t + 1e-9 ? 1 : 0;
            p.losses_known = known;
            if (p.turned_back || p.size <= 0 || known <= 0) continue;
            if (static_cast<double>(known) / p.size < p.turn_back_share) continue;
            const int cf = CommanderFlight(p);
            if (cf < 0 || PastRelease(flights_[static_cast<std::size_t>(cf)])) continue;
            p.turned_back = true;
            notes.push_back({p.id, "the package turns back after losing " + std::to_string(known) + " of " + std::to_string(p.size)});
            for (int fi : p.flights) {
                if (fi < 0 || fi >= static_cast<int>(flights_.size())) continue;
                const FlightState& f = flights_[static_cast<std::size_t>(fi)];
                if (f.weapons || f.Leader() == nullptr || PastRelease(f) || f.order.kind == OrderKind::ReturnHome) continue;
                FlightOrder home;
                home.kind = OrderKind::ReturnHome;
                home.by = "package";
                Order(fi, home, t);
                notes.push_back({f.id, "ordered: return home (by package)"});
            }
        }
        for (Note& n : pending_notes_) notes.push_back(std::move(n));
        pending_notes_.clear();
        return notes;
    }

    // The formation's slot of a place in a phase: [right, back, up] of the leader. Without data for the phase, the
    // transit's; without any, where each started from the leader. A place past the table's repeats it further back.
    static Vec3 SlotOffset(const FlightState& f, const FlightSlot& s) {
        if (f.formations.empty()) {
            const FlightSlot* ls = f.Leader();
            return ls != nullptr ? s.offset_m - ls->offset_m : s.offset_m;
        }
        return FormationSlot(f.formations, f.phase, s.place);
    }

    // Where a wingman's station is: the leader's place and track, and the slot.
    static Vec3 StationOf(const Aircraft& L, const Vec3& off) {
        const Vec3 fwd = TrackOf(L);
        const Vec3 right{fwd.y, -fwd.x, 0.0};
        return L.position + right * off.x - fwd * off.y + Vec3{0.0, 0.0, off.z};
    }
    static Vec3 TrackOf(const Aircraft& a) {
        const double h = a.velocity.HorizontalLength();
        if (h > 1.0) return Vec3{a.velocity.x / h, a.velocity.y / h, 0.0};
        return Vec3{std::sin(a.heading_rad), std::cos(a.heading_rad), 0.0};
    }

private:
    std::pair<int, int> Where(int target) const {
        if (target < 0 || static_cast<std::size_t>(target) >= of_target_.size()) return {-1, -1};
        return of_target_[static_cast<std::size_t>(target)];
    }
    static void Record(FlightState& f, const FlightOrder& o, int target) {
        OrderRecord r;
        r.t_s = o.at_s;
        r.kind = o.kind;
        r.by = o.by;
        r.has_aim = o.has_aim;
        r.aim = o.aim;
        if (o.kind == OrderKind::Divert && !o.legs.empty() && !o.has_aim) {
            r.has_aim = true;
            r.aim = o.legs.back().to;
        }
        r.target = target;
        f.orders.push_back(r);
    }
    static void Hear(FlightSlot& s, const FlightOrder& o) {
        s.order = o;
        s.order_fresh = true;
        s.order_leg = 0;
    }

    // The leader lost (or gone off on his own): the element lead (No. 3) takes the flight, else the lowest number left;
    // the rest close up behind him in their numbers' order (a lone wingman joins the element).
    void Succession(FlightState& f, std::vector<Note>* notes, const char* why = "lead lost") {
        const int old = f.leader_slot;
        int next = -1;
        for (std::size_t k = 0; k < f.slots.size(); ++k) {
            const FlightSlot& s = f.slots[k];
            if (!s.alive || s.gone || s.detached || static_cast<int>(k) == old) continue;
            if (s.number == 3) {
                next = static_cast<int>(k);
                break;
            }
            if (next < 0 || s.number < f.slots[static_cast<std::size_t>(next)].number) next = static_cast<int>(k);
        }
        f.leader_slot = next;
        if (next < 0) return;
        FlightSlot& nl = f.slots[static_cast<std::size_t>(next)];
        f.order = nl.order;  // the new leader goes on with what he heard last
        Replace(f);
        const std::string text = std::string(why) + ": No. " + std::to_string(nl.number) + " takes the flight";
        if (notes != nullptr) {
            notes->push_back({f.id, text});
        } else {
            pending_notes_.push_back({f.id, text});
        }
    }
    static void Replace(FlightState& f) {
        std::vector<int> order;
        for (std::size_t k = 0; k < f.slots.size(); ++k) {
            const FlightSlot& s = f.slots[k];
            if (static_cast<int>(k) != f.leader_slot && s.alive && !s.gone && !s.detached) order.push_back(static_cast<int>(k));
        }
        if (f.leader_slot >= 0) f.slots[static_cast<std::size_t>(f.leader_slot)].place = 0;
        int place = 1;
        for (int k : order) f.slots[static_cast<std::size_t>(k)].place = place++;
    }

    int CommanderFlight(const PackageState& p) const {
        if (p.commander_flight >= 0 && p.commander_flight < static_cast<int>(flights_.size()) &&
            flights_[static_cast<std::size_t>(p.commander_flight)].Leader() != nullptr) {
            return p.commander_flight;
        }
        for (int fi : p.flights) {
            if (fi >= 0 && fi < static_cast<int>(flights_.size()) && !flights_[static_cast<std::size_t>(fi)].weapons &&
                flights_[static_cast<std::size_t>(fi)].Leader() != nullptr) {
                return fi;
            }
        }
        return -1;
    }
    static bool PastRelease(const FlightState& f) { return f.phase == "egress"; }

    static std::string PhaseNow(const FlightState& f, const FlightSlot& ls, const Aircraft& L) {
        if (ls.overriding) return "defensive";
        switch (ls.order.kind) {
            case OrderKind::Hold:
            case OrderKind::Orbit: return "orbit";
            case OrderKind::Evade: return "defensive";
            case OrderKind::ReturnHome: return "egress";
            default: break;
        }
        if (ls.order.kind == OrderKind::Divert && !ls.order.legs.empty() && ls.order_leg < ls.order.legs.size()) {
            return PhaseOfTag(ls.order.legs[ls.order_leg].tag);
        }
        if (L.next_waypoint < ls.legs.size()) return PhaseOfTag(ls.legs[L.next_waypoint].tag);
        if (!ls.legs.empty() && L.next_waypoint >= ls.legs.size()) return PhaseOfTag(ls.legs.back().tag);
        (void)f;
        return "transit";
    }

    // A wingman on his station: he flies for a point ahead of it, faster when behind it and slower when ahead, within
    // his own limits (the flight model's). Lost from sight (too far), he flies his own plan until he closes again.
    bool KeepSlot(const FlightState& f, FlightSlot& s, const Aircraft& L, Aircraft& A) {
        const Vec3 off = SlotOffset(f, s);
        const Vec3 S = StationOf(L, off);
        const Vec3 d = S - A.position;
        const double dist = std::hypot(d.x, d.y);
        if (s.in_contact && dist > flights::kContactLostM) s.in_contact = false;
        if (!s.in_contact && dist < flights::kContactRegainM) s.in_contact = true;
        if (!s.in_contact) return false;
        const Vec3 fwd = TrackOf(L);
        const double along = d.x * fwd.x + d.y * fwd.y;
        const double vl = FliesByBank(L) ? AirspeedOf(L) : L.speed_mps;
        const double close = std::max(-flights::kCloseShare * vl, std::min(flights::kCloseShare * vl, flights::kCloseGainPerS * along));
        const double lead = std::max({flights::kLeadMinM, vl * flights::kLeadS, flights::kLeadMinM - along});
        A.demand.active = true;
        A.demand.point = S + fwd * lead;
        A.demand.speed_mps = std::max(1.0, vl + close);
        s.keeping = true;
        return true;
    }

    // An aircraft flying an order itself: the leader, or one on its own.
    void FlyOrder(FlightState& f, FlightSlot& s, bool leader, Aircraft& A, double t) {
        FlightOrder& o = s.order;
        if (s.order_fresh) {
            s.order_fresh = false;
            s.order_leg = 0;
            if (o.kind == OrderKind::FollowRoute && o.from_leg >= 0) {
                A.next_waypoint = std::min(static_cast<std::size_t>(o.from_leg), A.waypoints.size());
            } else if (o.kind == OrderKind::ReturnHome && !A.waypoints.empty()) {
                A.next_waypoint = std::max(A.next_waypoint, A.waypoints.size() - 1);  // straight for its way out
            } else if (o.kind == OrderKind::Hold && !o.has_aim) {
                o.has_aim = true;
                o.aim = A.position;
            }
        }
        const auto done = [&]() {
            FlightOrder next;
            next.by = o.by;
            next.at_s = t;
            o = next;
            if (leader) f.order = o;
        };
        switch (o.kind) {
            case OrderKind::FollowRoute:
            case OrderKind::Rejoin:
            case OrderKind::ReturnHome: return;
            case OrderKind::Divert: {
                if (o.has_heading && o.legs.empty()) {
                    const Vec3 dir{std::sin(o.heading_rad), std::cos(o.heading_rad), 0.0};
                    Demand(A, A.position + dir * flights::kVectorM, o.speed_mps);
                    return;
                }
                while (s.order_leg < o.legs.size()) {
                    const Vec3 p = o.legs[s.order_leg].to;
                    const Vec3 nose{std::sin(A.heading_rad), std::cos(A.heading_rad), 0.0};
                    if ((p - A.position).Length() > A.waypoint_capture_radius_m && !WaypointInsideTurn(A, nose, p)) break;
                    ++s.order_leg;
                }
                if (s.order_leg >= o.legs.size()) {  // the new route flown: its route is done
                    A.next_waypoint = A.waypoints.size();
                    done();
                    return;
                }
                const RouteLeg& leg = o.legs[s.order_leg];
                Demand(A, leg.to, leg.speed_mps > 0.0 ? leg.speed_mps : o.speed_mps);
                return;
            }
            case OrderKind::Hold:
            case OrderKind::Orbit: {
                if (o.until_s >= 0.0 && t >= o.until_s) {
                    done();
                    return;
                }
                const Vec3 c = o.has_aim ? o.aim : A.position;
                const double r = o.radius_m > 0.0 ? o.radius_m : flights::kOrbitRadiusM;
                const double ang = std::atan2(A.position.x - c.x, A.position.y - c.y) + flights::kOrbitLeadRad;  // round clockwise
                Demand(A, Vec3{c.x + r * std::sin(ang), c.y + r * std::cos(ang), A.position.z}, o.speed_mps);
                return;
            }
            case OrderKind::Attack: {
                if (!o.has_aim || std::hypot(A.position.x - o.aim.x, A.position.y - o.aim.y) < flights::kAttackCaptureM) {
                    done();
                    return;
                }
                Demand(A, Vec3{o.aim.x, o.aim.y, A.position.z}, o.speed_mps);
                return;
            }
            case OrderKind::Evade: {
                if (o.until_s >= 0.0 && t >= o.until_s) {
                    done();
                    return;
                }
                Vec3 away = o.has_aim ? A.position - o.aim : TrackOf(A);
                away.z = 0.0;
                away = away.LengthSquared() > 1.0 ? away.Normalized() : TrackOf(A);
                Demand(A, A.position + away * flights::kAwayPointM, o.speed_mps);
                return;
            }
        }
    }
    static void Demand(Aircraft& A, const Vec3& point, double speed_mps) {
        A.demand.active = true;
        A.demand.point = point;
        A.demand.speed_mps = speed_mps;
    }

    // A pilot's override, after the step: a jink's turns, and its end.
    std::string StepOverride(FlightState& f, FlightSlot& s, int k, Aircraft& a, double t, bool& ended) {
        PilotOverride& o = s.ov;
        bool end = t >= o.until_s;
        if (o.end == PilotOverride::End::Point && a.demand.reached) end = true;  // passed as a waypoint is (aircraft.hpp)
        if (!end) {
            if (o.kind == PilotOverride::Kind::Jink && t >= o.flip_s) {
                o.sign = -o.sign;
                o.flip_s = t + o.period_s;
                o.point = JinkPoint(o, a.position);
            }
            return std::string();
        }
        s.overriding = false;
        ended = true;
        if (o.after == PilotOverride::After::Home) {
            // It leaves the session at the point, its way out (as its last waypoint); a leader's flight goes home behind
            // him, each by its own way out.
            a.next_waypoint = a.waypoints.size();
            s.detached = true;
            if (f.leader_slot != k) return std::string();
            int others = 0;
            for (std::size_t m = 0; m < f.slots.size(); ++m) {
                const FlightSlot& o2 = f.slots[m];
                others += static_cast<int>(m) != k && o2.alive && !o2.gone && !o2.detached ? 1 : 0;
            }
            if (others == 0) return std::string();
            Succession(f, nullptr, "the lead turns for home");
            FlightOrder home;
            home.kind = OrderKind::ReturnHome;
            home.by = "leader";
            Order(static_cast<int>(&f - flights_.data()), home, t);
            return "turns for home: the flight follows";
        }
        const bool wingman = k != f.leader_slot && !s.detached && f.keep_slots;
        return wingman ? "rejoins its flight" : "back on its route";
    }

public:
    // A jink's point: 60 degrees left or right of its course, a long way out.
    static Vec3 JinkPoint(const PilotOverride& o, const Vec3& from) {
        const double ang = o.sign * flights::kJinkTurnRad;
        const Vec3 d{o.base_dir.x * std::cos(ang) + o.base_dir.y * std::sin(ang), -o.base_dir.x * std::sin(ang) + o.base_dir.y * std::cos(ang), 0.0};
        return from + d * flights::kJinkLegM;
    }

private:
    FlightRadio* radio_ = nullptr;  // the calls on the air (B9); null: the fixed delay
    std::vector<FlightState> flights_;
    std::vector<PackageState> packages_;
    std::vector<std::pair<int, int>> of_target_;  // target -> (flight, slot)
    std::vector<Note> pending_notes_;
};

}  // namespace simcore
