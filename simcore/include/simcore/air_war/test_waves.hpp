// simcore/air_war/test_waves.hpp — the dynamic campaign testbed's waves of aircraft (docs/DYNAMIC_CAMPAIGN_TEST.md).
//
// TestWaves makes random waves from a WavesDef (test_waves_data.hpp) for a running session (ScenarioRunner, which
// owns one when its scenario names "waves"): every so often a wave of flights, each an adversary's aircraft type drawn
// from a deck of all of them (so every type comes round) or one of our own side's, flying its role's profile over the
// scenario's centre. The enemy's come in from their front's entries (the theatre's fronts) at the entry range: strike
// and DEAD flights low or at medium height on what our side has there (the aims: our air defences, the battery, the
// city's objects), SEAD and hunters with their anti-radiation missiles and warning receivers, escorts sweeping, recce
// passing, jammers and the support aircraft (AWACS, tankers, ELINT) on orbits out of reach, decoys, armed helicopters
// low, cruise missiles to their aim. Ours fly combat air patrols over the city, intercepts out toward a front, ground
// attack out to the front, transports and trainers across, helicopters low.
//
// A strike aircraft's load is released when it comes within its weapon's release range of its aim, still flying (the
// runner asks Releases each step): a WaveRelease, which the runner gives the presentation feed as the load's release
// and, after its flight (weapon_flight.hpp), its impact; the game draws and sounds both. A cruise missile is a target
// of the session ("weapon": its impact ends it). All numbers are game values for a testbed.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/planner.hpp"
#include "simcore/air_war/test_waves_data.hpp"
#include "simcore/air_war/weapon_flight.hpp"
#include "simcore/em_link.hpp"
#include "simcore/geo.hpp"
#include "simcore/scenario.hpp"
#include "simcore/terrain.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace airwar {

// What a wave flies over: the session's frame and ground, the city's middle and what the enemy aims at.
struct WaveScene {
    const LocalFrame* frame = nullptr;
    const Terrain* terrain = nullptr;
    Vec3 centre;                    // the defended city's middle (ENU, the ground's height or near it)
    std::vector<Vec3> defences;     // our air defences and the battery's vehicles: DEAD and SEAD aim here
    std::vector<Vec3> objects;      // anything else worth a bomb (the scenery, points of the city)
    const em::EmLibrary* em = nullptr;  // the session's EM library (null: no warning receivers, ARMs or jammers)
};

struct Wave {
    int number = 0;
    double at_s = 0.0;
    std::vector<ScenarioTarget> aircraft;
    std::vector<EmJammerDef> jammers;
    std::vector<std::string> flights;  // "Hammer 12: 4 F-16A, strike (NATO)" for the log
    std::vector<ScenarioFlight> flight_defs;  // the flights as the session keeps them (flights.hpp): its aircraft name them
};

// A load let go: the presentation feed's release and, at impact_t_s, its impact (fx/events.hpp; the runner pushes both).
// The weapons themselves are the effects layer's to draw and fly, not objects of the session.
struct WaveRelease {
    std::string carrier;       // the aircraft's target id
    std::string carrier_name;
    std::string weapon;        // weapons.json id
    std::string weapon_name;
    std::string delivery;      // weapons.json's delivery name
    int count = 1;             // how many of it
    double t_s = 0.0;
    Vec3 position, velocity;   // the aircraft's at the release
    Vec3 impact;               // on the ground
    double impact_t_s = 0.0;
};

// An airborne aircraft as the session has it (for its release): false if it is not in the world or not alive.
using AircraftLookup = std::function<bool(const std::string& target_id, Vec3& position, Vec3& velocity)>;

class TestWaves {
public:
    TestWaves(std::shared_ptr<const WavesDef> def, unsigned seed) : def_(std::move(def)), rng_(seed * 2654435761u + 97u) {
        next_s_ = def_->first_wave_s;
    }

    const WavesDef& Def() const { return *def_; }
    int Waves() const { return waves_; }
    double NextWaveS() const { return next_s_; }

    // Whether a wave is due at `t` with `airborne` of the waves' aircraft in the sky or about to enter it.
    bool Due(double t, int airborne) {
        if (t + 1e-9 < next_s_) return false;
        if (airborne >= def_->max_airborne) {
            next_s_ = t + 10.0;  // the sky is full: look again shortly
            return false;
        }
        return true;
    }

    // The next wave, entering from `t`.
    Wave Next(double t, const WaveScene& scene) {
        Wave w;
        w.number = ++waves_;
        w.at_s = t;
        next_s_ = t + Uniform(def_->interval_min_s, def_->interval_max_s);
        const int flights = UniformInt(def_->flights_min, def_->flights_max);
        for (int i = 0; i < flights; ++i) {
            const double at = t + (i == 0 ? 0.0 : Uniform(0.0, 45.0));
            if (!def_->friendly.empty() && Uniform(0.0, 1.0) < def_->friendly_share) {
                FriendlyFlight(at, scene, w);
            } else {
                EnemyFlight(at, scene, w);
            }
        }
        return w;
    }

    // The loads released by `t`: each aircraft with a load still to drop that has come within its weapon's release range
    // of its aim. One shot down or gone first drops nothing.
    std::vector<WaveRelease> Releases(double t, const WaveScene& scene, const AircraftLookup& lookup) {
        std::vector<WaveRelease> out;
        for (Pending& p : pending_) {
            if (p.done) continue;
            Vec3 pos, vel;
            if (t > p.give_up_s) {
                p.done = true;
                continue;
            }
            if (t + 1e-9 < p.not_before_s) continue;
            if (!lookup(p.carrier, pos, vel)) {
                if (t > p.not_before_s + 5.0) p.done = true;  // spawned and gone: shot down, or out of the world
                continue;
            }
            const double d = std::hypot(pos.x - p.aim.x, pos.y - p.aim.y);
            if (d > p.release_m) continue;
            p.done = true;
            const WeaponDef* wd = def_->weapons.Find(p.weapon);
            const WaveWeaponKind* k = wd != nullptr ? def_->Kind(wd->kind) : nullptr;
            if (wd == nullptr || k == nullptr) continue;
            WaveRelease r;
            r.carrier = p.carrier;
            r.carrier_name = p.carrier_name;
            r.weapon = p.weapon;
            r.weapon_name = wd->name;
            r.delivery = p.delivery;
            r.count = std::max(1, p.count);
            r.t_s = t;
            r.position = pos;
            r.velocity = vel;
            r.impact = OnGround(scene, p.aim.x + Uniform(-k->scatter_m, k->scatter_m), p.aim.y + Uniform(-k->scatter_m, k->scatter_m), 0.0);
            // It lands after its own flight from here (weapon_flight.hpp, as the raids' loads): by its kind, the delivery,
            // the aircraft's height over the ground where it lands, its speed and its path angle.
            const ReleaseState st = ReleaseStateOf(*scene.frame, pos, vel, p.delivery);
            const double agl = scene.frame->AltitudeOf(pos) - scene.frame->AltitudeOf(r.impact);
            const double flight = WeaponFlightOf(p.delivery, p.weapon, agl, st.speed_mps, st.path_deg, (r.impact - pos).Length(),
                                                 &def_->weapons).time_s;
            r.impact_t_s = t + (flight > 0.0 ? flight : 15.0);
            out.push_back(std::move(r));
            ++drops_;
        }
        pending_.erase(std::remove_if(pending_.begin(), pending_.end(), [](const Pending& p) { return p.done; }), pending_.end());
        return out;
    }

    // An aircraft throws off its load (damaged: simcore/damage.hpp): what it has not yet released falls free from where it
    // is, inert (delivery "jettison"), and it releases none of it later. It falls its height above the ground carried on
    // by the aircraft's speed (no drag: a game value).
    std::vector<WaveRelease> Jettison(const std::string& carrier, double t, const Vec3& pos, const Vec3& vel, const WaveScene& scene) {
        std::vector<WaveRelease> out;
        for (Pending& p : pending_) {
            if (p.done || p.carrier != carrier) continue;
            p.done = true;
            const WeaponDef* wd = def_->weapons.Find(p.weapon);
            WaveRelease r;
            r.carrier = p.carrier;
            r.carrier_name = p.carrier_name;
            r.weapon = p.weapon;
            r.weapon_name = wd != nullptr ? wd->name : p.weapon;
            r.delivery = "jettison";
            r.count = std::max(1, p.count);
            r.t_s = t;
            r.position = pos;
            r.velocity = vel;
            const Vec3 below = OnGround(scene, pos.x, pos.y, 0.0);
            const double agl = scene.frame != nullptr ? scene.frame->AltitudeOf(pos) - scene.frame->AltitudeOf(below) : pos.z - below.z;
            const double fall_s = std::sqrt(2.0 * std::max(agl, 1.0) / kStandardGravity);
            r.impact = OnGround(scene, pos.x + vel.x * fall_s, pos.y + vel.y * fall_s, 0.0);
            r.impact_t_s = t + fall_s;
            out.push_back(std::move(r));
        }
        return out;
    }

    int Drops() const { return drops_; }
    // The adversaries' aircraft types flown so far (faction id, type id), and our own (id).
    const std::vector<std::pair<std::string, std::string>>& EnemyTypesFlown() const { return enemy_flown_; }
    const std::vector<std::string>& FriendlyTypesFlown() const { return friendly_flown_; }
    const std::vector<std::string>& WeaponsLoaded() const { return weapons_loaded_; }

private:
    // How a load is put on its aim: a helicopter fires hovering, a guided weapon or missile from its stand-off, a low
    // flight lays it down (or pops up to it), a higher one dives or bombs level; always one its weapon allows.
    static std::string DeliveryFor(const WeaponDef& w, bool low, bool helicopter) {
        std::vector<Delivery> want;
        if (helicopter) want.push_back(Delivery::HoverFire);
        if (w.standoff_m > 0.0) want.push_back(Delivery::Standoff);
        if (low) {
            want.push_back(Delivery::Laydown);
            want.push_back(Delivery::PopUp);
        } else {
            want.push_back(Delivery::Dive);
            want.push_back(Delivery::Level);
        }
        for (Delivery d : want) {
            if (w.Can(d)) return DeliveryName(d);
        }
        return w.deliveries.empty() ? std::string() : std::string(DeliveryName(w.deliveries.front()));
    }

    struct Pending {
        std::string carrier;
        std::string carrier_name;
        Vec3 aim;
        std::string weapon;
        std::string delivery;  // weapons.json's name: laydown, popup, dive, level, standoff, hover_fire
        int count = 1;
        double release_m = 1500.0;
        double not_before_s = 0.0;
        double give_up_s = 0.0;
        bool done = false;
    };
    // A height mode for a route: above the ground (terrain following) or a height above sea level.
    struct Leg {
        Vec3 at;           // horizontal place (z ignored)
        double height_m;   // above the ground if low, else above the ground under the centre
    };

    double Uniform(double lo, double hi) {
        if (!(hi > lo)) return lo;
        return std::uniform_real_distribution<double>(lo, hi)(rng_);
    }
    int UniformInt(int lo, int hi) {
        if (hi <= lo) return lo;
        return std::uniform_int_distribution<int>(lo, hi)(rng_);
    }
    template <typename T>
    const T& Pick(const std::vector<T>& v) {
        return v[static_cast<std::size_t>(UniformInt(0, static_cast<int>(v.size()) - 1))];
    }

    static Vec3 Toward(double bearing_rad) { return Vec3{std::sin(bearing_rad), std::cos(bearing_rad), 0.0}; }
    static double BearingOf(const Vec3& d) { return std::atan2(d.x, d.y); }

    // A point `up_m` above the ground at (east, north) (straight up the frame's z, as the runner's GroundEnu: roadmap R5).
    static Vec3 OnGround(const WaveScene& s, double e, double n, double up_m) {
        const double ground = s.terrain != nullptr ? s.terrain->HeightAt(e, n) : 0.0;
        if (s.frame == nullptr) return Vec3{e, n, ground + up_m};
        return GroundPointInFrame(*s.frame, e, n, ground + up_m);
    }
    // A point at a height above sea level: the centre's ground plus `above_m`.
    static Vec3 AtHeight(const WaveScene& s, double e, double n, double above_m) {
        const double base = s.terrain != nullptr ? s.terrain->HeightAt(s.centre.x, s.centre.y) : 0.0;
        if (s.frame == nullptr) return Vec3{e, n, base + above_m};
        return GroundPointInFrame(*s.frame, e, n, std::max(base + above_m, (s.terrain != nullptr ? s.terrain->HeightAt(e, n) : 0.0) + 150.0));
    }

    // The direction from the centre to one of the faction's front entries (through the geodetic: the theatre's frame
    // is not the session's), spread a little.
    double EntryBearing(const FactionDef& f, const WaveScene& s) {
        const FrontDef* fr = def_->theatre.FrontFor(f.id);
        if (fr == nullptr || fr->entries.empty() || s.frame == nullptr) return Uniform(0.0, 2.0 * kPi);
        const LocalFrame th(def_->theatre.origin);
        const Vec3 e = s.frame->GeodeticToEnu(th.EnuToGeodetic(Pick(fr->entries).pos));
        return BearingOf(e - s.centre) + DegToRad(Uniform(-8.0, 8.0));
    }
    Vec3 CityPoint(const WaveScene& s) {
        const double r = def_->city_radius_m * std::sqrt(Uniform(0.0, 1.0));
        const double a = Uniform(0.0, 2.0 * kPi);
        return s.centre + Toward(a) * r;
    }
    Vec3 Aim(const WaveScene& s, bool defences) {
        if (defences && !s.defences.empty()) return Pick(s.defences);
        if (!s.objects.empty() && Uniform(0.0, 1.0) < 0.7) return Pick(s.objects);
        if (!s.defences.empty() && Uniform(0.0, 1.0) < 0.5) return Pick(s.defences);
        return CityPoint(s);
    }

    // The target with its route: `low` legs above the ground under them (terrain following), else heights above the
    // centre's ground.
    static void Route(ScenarioTarget& t, const WaveScene& s, const std::vector<Leg>& legs, bool low, double climb_mps) {
        t.start = low ? OnGround(s, legs[0].at.x, legs[0].at.y, legs[0].height_m) : AtHeight(s, legs[0].at.x, legs[0].at.y, legs[0].height_m);
        t.waypoints.clear();
        t.behaviour = TargetBehaviourDef();
        for (std::size_t i = 1; i < legs.size(); ++i) {
            if (low) {
                Vec3 w = legs[i].at;
                w.z = t.start.z;
                t.waypoints.push_back(w);
                t.behaviour.waypoint_agl_m.push_back(legs[i].height_m);
            } else {
                t.waypoints.push_back(AtHeight(s, legs[i].at.x, legs[i].at.y, legs[i].height_m));
                t.behaviour.waypoint_agl_m.push_back(-1.0);
            }
        }
        if (low) {
            t.behaviour.kind = TargetBehaviourDef::Kind::TerrainFollowing;
            t.behaviour.agl_m = legs[0].height_m;
            t.behaviour.max_climb_mps = climb_mps;
            t.behaviour.lookahead_s = 6.0;
        }
    }
    // A racetrack about `centre` along `axis`, `laps` times, then out along `out_bearing`.
    static std::vector<Leg> Orbit(const Vec3& from, const Vec3& centre, double axis_rad, double long_m, double wide_m, int laps,
                                  const Vec3& exit, double h) {
        std::vector<Leg> legs{{from, h}};
        const Vec3 a = Toward(axis_rad), b = Toward(axis_rad + 0.5 * kPi);
        const Vec3 corners[4] = {centre + a * long_m + b * wide_m, centre + a * long_m - b * wide_m, centre - a * long_m - b * wide_m,
                                 centre - a * long_m + b * wide_m};
        for (int l = 0; l < laps; ++l) {
            for (const Vec3& c : corners) legs.push_back({c, h});
        }
        legs.push_back({exit, h});
        return legs;
    }

    ScenarioTarget Base(const std::string& id, const std::string& name, const std::string& model, const std::string& cls,
                        bool helicopter, double speed_mps, double turn_deg_s, double rcs_m2, double at_s, int formation) {
        ScenarioTarget t;
        t.id = id;
        t.name = name;
        t.type = helicopter ? "Air+Rotorcraft" : "Air+FixedWing";
        t.model = model;
        t.target_class = cls;
        t.speed_mps = speed_mps;
        t.max_turn_rate_rad_s = DegToRad(turn_deg_s);
        t.rcs_m2 = rcs_m2;
        t.appear_at_s = at_s;
        t.formation = formation;
        t.size_m = helicopter ? 14.0 : 10.0;
        t.bottom_m = helicopter ? 2.5 : 1.5;  // its belly under its centre (roadmap R5; a weapon's is set with its load)
        return t;
    }

    // Its type's flight limits (roadmap R3): it flies by bank-to-turn.
    static void FlightLimits(const AircraftType& a, ScenarioTarget& t) {
        t.max_load_g = a.max_load_g;
        t.sustained_turn_rate_rad_s = DegToRad(a.sustained_turn_deg_s);
        t.roll_rate_rad_s = DegToRad(a.roll_rate_deg_s);
        t.max_speed_mps = a.max_mps;
        t.accel_mps2 = a.accel_mps2;
        t.max_climb_mps = a.climb_mps;
    }

    // Sets the k-th aircraft of a flight (place k, the leader's 0) on its slot of the formation (flights.hpp), in each
    // leg's frame: its own plan, which it flies when it has lost its leader (and a stream of weapons always).
    static void Slot(ScenarioTarget& t, const std::map<std::string, std::vector<Vec3>>& forms, int k) {
        // The start set back by the slot's depth too; a turn point only to the side (the man in trail passes the
        // leader's turn point when he gets there).
        const Vec3 o = FormationSlot(forms, "transit", k);
        const auto shift = [&o](const Vec3& from, const Vec3& to, bool back) {
            Vec3 d = to - from;
            d.z = 0.0;
            d = d.LengthSquared() > 1.0 ? d.Normalized() : Vec3{0.0, 1.0, 0.0};
            const double ob = back ? o.y : 0.0;
            return Vec3{d.y * o.x - d.x * ob, -d.x * o.x - d.y * ob, 0.0};
        };
        const Vec3 s0 = t.start;
        const std::vector<Vec3> w0 = t.waypoints;
        if (!w0.empty()) t.start += shift(s0, w0[0], true);
        for (std::size_t i = 0; i < w0.size(); ++i) t.waypoints[i] += shift(i == 0 ? s0 : w0[i - 1], w0[i], false);
    }
    // The flight as the session keeps it (flights.hpp): its aircraft name it.
    static ScenarioFlight FlightDef(const std::string& id, const std::string& callsign, bool ours, bool keep_slots,
                                    const std::map<std::string, std::vector<Vec3>>& forms) {
        ScenarioFlight f;
        f.id = id;
        f.callsign = callsign;
        f.side = ours ? "ours" : "enemy";
        f.keep_slots = keep_slots;
        f.formations = forms;
        return f;
    }

    void EnemyFlight(double at_s, const WaveScene& s, Wave& w) {
        if (deck_.empty()) {
            for (std::size_t f = 0; f < def_->factions.size(); ++f) {
                for (std::size_t a = 0; a < def_->factions[f].aircraft.size(); ++a) deck_.push_back({f, a});
            }
            std::shuffle(deck_.begin(), deck_.end(), rng_);
        }
        if (deck_.empty()) return;
        const std::pair<std::size_t, std::size_t> card = deck_.back();
        deck_.pop_back();
        const FactionDef& f = def_->factions[card.first];
        const AircraftType& a = f.aircraft[card.second];
        const FlightRole role = Pick(a.roles);
        Note(enemy_flown_, std::make_pair(f.id, a.id));
        const double bearing = EntryBearing(f, s);
        const double R = a.helicopter ? Uniform(def_->helicopter_min_m, def_->helicopter_max_m) : Uniform(def_->entry_min_m, def_->entry_max_m);
        const Vec3 entry = s.centre + Toward(bearing) * R;
        const Vec3 home = s.centre + Toward(bearing + DegToRad(Uniform(-50.0, 50.0))) * (R + 10000.0);
        int n = 2;
        switch (role) {
            case FlightRole::Missile: n = UniformInt(2, 4); break;
            case FlightRole::Decoy: n = UniformInt(2, 4); break;
            case FlightRole::Awacs: case FlightRole::Tanker: case FlightRole::Elint: case FlightRole::Jammer: n = 1; break;
            case FlightRole::Recce: n = UniformInt(1, 2); break;
            default: n = Uniform(0.0, 1.0) < 0.5 ? 2 : 4; break;
        }
        const std::string cs = f.callsigns.empty() ? f.short_name : Pick(f.callsigns);
        const int number = ++flight_no_;
        std::string flight_id = cs + std::to_string(number);
        flight_id.erase(std::remove(flight_id.begin(), flight_id.end(), ' '), flight_id.end());
        // Its formation (the faction's "flights" block for its type class; a salvo of missiles keeps none).
        const std::map<std::string, std::vector<Vec3>> forms = f.FormationsFor(FormationClassOf(a.helicopter, role));
        const bool high_strike = Uniform(0.0, 1.0) < 0.4;
        const Vec3 aim = Aim(s, role == FlightRole::Dead || role == FlightRole::Sead || role == FlightRole::Hunter);
        const double low_h = std::max(15.0, a.low_agl_m);
        const double mid_h = a.medium_alt_m;
        // The load for a strike: one weapon of its loads.
        std::pair<std::string, int> load;
        if (!a.loads.empty() && (role == FlightRole::Strike || role == FlightRole::Dead || role == FlightRole::Helicopter ||
                                 (role == FlightRole::Recce && Uniform(0.0, 1.0) < 0.3))) {
            load = Pick(a.loads);
            if (std::find(weapons_loaded_.begin(), weapons_loaded_.end(), load.first) == weapons_loaded_.end()) weapons_loaded_.push_back(load.first);
        }
        for (int k = 0; k < n; ++k) {
            const std::string id = flight_id + "-" + std::to_string(k + 1);
            const std::string name = cs + " " + std::to_string(number) + "-" + std::to_string(k + 1) + ": " + a.name + " (" +
                                     FlightRoleName(role) + ", " + f.short_name + ")";
            const double t0 = at_s + (role == FlightRole::Missile ? 2.0 * k : 0.0);
            ScenarioTarget t = Base(id, name, a.id, a.target_class, a.helicopter, a.cruise_mps, a.max_turn_deg_s, a.rcs_m2, t0, n);
            t.type = a.missile ? "Air+Missile" : t.type;
            FlightLimits(a, t);
            if (a.drone) {
                t.decoy = role == FlightRole::Decoy;
                t.size_m = 4.0;
            }
            bool low = false;
            std::vector<Leg> legs;
            double release_aim_m = 0.0;
            switch (role) {
                case FlightRole::Strike:
                case FlightRole::Dead: {
                    low = !high_strike || a.helicopter;
                    const double h = low ? low_h : mid_h;
                    const double in = BearingOf(entry - aim) + DegToRad(Uniform(-40.0, 40.0));
                    const Vec3 ip = aim + Toward(in) * 14000.0;
                    const Vec3 past = aim + (aim - ip).Normalized() * 5000.0;
                    legs = {{entry, h}, {ip, h}, {aim, h}, {past, h}, {home, h}};
                    t.speed_mps = a.attack_mps;
                    release_aim_m = 1.0;
                    break;
                }
                case FlightRole::Sead:
                case FlightRole::Hunter: {
                    const double stand = role == FlightRole::Hunter ? 14000.0 : 24000.0;
                    const Vec3 shot = aim + (entry - aim).Normalized() * stand;
                    const Vec3 side = shot + Toward(bearing + 0.5 * kPi) * 15000.0;
                    legs = {{entry, mid_h}, {shot, mid_h}, {side, mid_h}, {home, mid_h}};
                    break;
                }
                case FlightRole::Escort:
                case FlightRole::Sweep:
                case FlightRole::Cap: {
                    const Vec3 over = s.centre + Toward(bearing + 0.5 * kPi) * Uniform(-8000.0, 8000.0);
                    const Vec3 out = s.centre + Toward(bearing + kPi + DegToRad(Uniform(-30.0, 30.0))) * 45000.0;
                    legs = {{entry, mid_h}, {over, mid_h}, {out, mid_h}, {home, mid_h}};
                    break;
                }
                case FlightRole::Recce: {
                    // A helicopter on reconnaissance flies low (roadmap R5); the coin is tossed for every type, so the
                    // waves' draws stay as they were.
                    const bool coin = Uniform(0.0, 1.0) < 0.5;
                    low = a.helicopter || coin;
                    const double h = low ? low_h : mid_h;
                    const Vec3 over = s.centre + Toward(Uniform(0.0, 2.0 * kPi)) * Uniform(0.0, 3000.0);
                    const Vec3 out = s.centre + Toward(bearing + kPi + DegToRad(Uniform(-40.0, 40.0))) * 40000.0;
                    legs = {{entry, h}, {aim, h}, {over, h}, {out, h}, {home, h}};
                    release_aim_m = load.first.empty() ? 0.0 : 1.0;
                    break;
                }
                case FlightRole::Decoy: {
                    const Vec3 out = s.centre + Toward(bearing + kPi) * 30000.0;
                    legs = {{entry, mid_h}, {s.centre, mid_h}, {out, mid_h}};
                    break;
                }
                case FlightRole::Helicopter: {
                    low = true;
                    const double h = std::max(15.0, std::min(40.0, low_h + 10.0));
                    const Vec3 hold = aim + (entry - aim).Normalized() * 3500.0;
                    const Vec3 past = aim + Toward(bearing + 0.5 * kPi) * 1200.0;
                    legs = {{entry, h}, {hold, h}, {past, h}, {home, h}};
                    t.speed_mps = std::min(a.cruise_mps, 70.0);
                    release_aim_m = 1.0;
                    break;
                }
                case FlightRole::Missile: {
                    t.weapon = true;
                    t.type = "Weapon+Missile";
                    const Vec3 last = aim + (entry - aim).Normalized() * 12000.0;
                    if (a.low_agl_m <= 120.0) {
                        low = true;
                        legs = {{entry, low_h}, {last, low_h}, {aim, 0.0}};
                    } else {
                        legs = {{entry, mid_h}, {last, mid_h}, {aim, 0.0}};
                    }
                    break;
                }
                case FlightRole::Jammer: {
                    const Vec3 c = s.centre + Toward(bearing) * def_->jammer_orbit_m;
                    legs = Orbit(entry + Toward(bearing) * 20000.0, c, bearing + 0.5 * kPi, 20000.0, 6000.0, 5, home + Toward(bearing) * 20000.0, mid_h);
                    break;
                }
                case FlightRole::Awacs:
                case FlightRole::Tanker:
                case FlightRole::Elint: {
                    const Vec3 c = s.centre + Toward(bearing) * def_->support_orbit_m;
                    legs = Orbit(c + Toward(bearing) * 30000.0, c, bearing + 0.5 * kPi, 30000.0, 10000.0, 3, c + Toward(bearing) * 60000.0, mid_h);
                    break;
                }
                case FlightRole::Count: break;
            }
            if (legs.size() < 2) continue;
            Route(t, s, legs, low, a.helicopter ? 8.0 : 120.0);
            if (t.weapon) {
                // On to the aim point and into the ground there. A low one flies at the ground's height (agl 0: the
                // pilot's floor) onto the aim and ends there, its last waypoint (roadmap R5: it no longer flies on 3 km
                // past at 2 m, over the ground it never meets); a high one dives on through the aim, 2 km under the
                // ground 3 km beyond, and ends where it strikes.
                const Vec3 impact = OnGround(s, aim.x, aim.y, 0.0);
                t.waypoints.back() = low ? Vec3{impact.x, impact.y, t.start.z} : impact;
                if (!low) {
                    const Vec3 dir = Vec3{aim.x - legs[legs.size() - 2].at.x, aim.y - legs[legs.size() - 2].at.y, 0.0}.Normalized();
                    t.waypoints.push_back(impact + dir * 3000.0 - Vec3{0.0, 0.0, 2000.0});
                    t.behaviour.waypoint_agl_m.push_back(-1.0);
                }
            }
            // How far its belly is below its centre (the pilot's ground floor, roadmap R5): game values.
            t.bottom_m = t.weapon ? 0.3 : (a.helicopter ? 2.5 : 1.5);
            t.flight = flight_id;
            t.slot = k + 1;
            Slot(t, forms, k);
            if (!a.drone && !a.missile) {
                // Its pilot reacts by his faction's tables to what his picture holds (roadmap B4, 04_REACTIONS.md
                // 12.3): the faction's id names its profile; countermeasures and tanks come by its type and the year.
                t.reactions.on = true;
                t.reactions.profile = f.id;
                t.reactions.aircraft = a.id;
                if (release_aim_m > 0.0) t.reactions.stores = load.first;
            }
            t.board_type = role == FlightRole::Jammer ? 0 : 9;
            t.seats = a.seats;
            if (s.em != nullptr && !a.RwrEm(def_->theatre.year).empty() && s.em->Esm(a.RwrEm(def_->theatre.year)) >= 0) {
                t.rwr = a.RwrEm(def_->theatre.year);
                const std::string& arm = a.ArmEm(def_->theatre.year);
                if (a.arms > 0 && (role == FlightRole::Sead || role == FlightRole::Hunter) && !arm.empty() && s.em->Arm(arm) >= 0) {
                    t.arms.weapon = arm;
                    t.arms.count = a.arms;
                    t.arms.launch_range_m = a.arm_range_m;
                    t.arms.min_range_m = std::min(3000.0, 0.5 * a.arm_range_m);
                    t.arms.has_aim = true;
                    t.arms.aim = aim;
                }
            }
            const std::string jam = role == FlightRole::Jammer && !a.standoff_jammer_em.empty() ? a.standoff_jammer_em : a.self_jammer_em;
            if (s.em != nullptr && !jam.empty() && s.em->Jammer(jam) >= 0) {
                EmJammerDef j;
                j.jammer_id = jam;
                j.target_id = id;
                j.on_at_s = t0;
                w.jammers.push_back(j);
            }
            if (release_aim_m > 0.0 && !load.first.empty()) {
                const WeaponDef* wd = def_->weapons.Find(load.first);
                const WaveWeaponKind* kind = wd != nullptr ? def_->Kind(wd->kind) : nullptr;
                if (kind != nullptr) {
                    Pending p;
                    p.carrier = id;
                    p.carrier_name = name;
                    p.aim = aim + Vec3{Uniform(-60.0, 60.0), Uniform(-60.0, 60.0), 0.0};
                    p.weapon = load.first;
                    p.delivery = DeliveryFor(*wd, low, a.helicopter);
                    p.count = load.second;
                    p.release_m = std::max(kind->release_m, wd->standoff_m);
                    // A gun or rockets from a fixed-wing aircraft: a pass that opens fire at its slant range (roadmap B6,
                    // attack_pass.hpp), a strafe or a gentle dive.
                    if (wd->pass.present && !a.helicopter) {
                        p.release_m = wd->pass.open_fire_m;
                        if (wd->Can(Delivery::Strafe)) p.delivery = DeliveryName(Delivery::Strafe);
                        else if (wd->Can(Delivery::GentleDive)) p.delivery = DeliveryName(Delivery::GentleDive);
                    }
                    p.not_before_s = t0;
                    p.give_up_s = t0 + 3600.0;
                    pending_.push_back(p);
                }
            }
            w.aircraft.push_back(std::move(t));
        }
        char line[200];
        std::snprintf(line, sizeof(line), "%s %d: %d %s, %s (%s)%s%s", cs.c_str(), number, n, a.name.c_str(), FlightRoleName(role),
                      f.short_name.c_str(), load.first.empty() ? "" : ", ", load.first.c_str());
        w.flights.push_back(line);
        w.flight_defs.push_back(FlightDef(flight_id, cs + " " + std::to_string(number), false, role != FlightRole::Missile, forms));
    }

    void FriendlyFlight(double at_s, const WaveScene& s, Wave& w) {
        if (fdeck_.empty()) {
            for (std::size_t i = 0; i < def_->friendly.size(); ++i) fdeck_.push_back(i);
            std::shuffle(fdeck_.begin(), fdeck_.end(), rng_);
        }
        const WaveFriendlyType& a = def_->friendly[fdeck_.back()];
        fdeck_.pop_back();
        Note(friendly_flown_, a.id);
        const std::string role = Pick(a.roles);
        const int n = UniformInt(a.count_min, a.count_max);
        const std::string cs = Pick(def_->friendly_callsigns);
        const int number = ++flight_no_;
        // Toward the enemy: a front's bearing.
        double enemy = Uniform(0.0, 2.0 * kPi);
        if (!def_->factions.empty()) enemy = EntryBearing(Pick(def_->factions), s);
        const double from = Uniform(0.0, 2.0 * kPi);
        const bool low = a.helicopter || role == "ground_attack";
        const double h = a.alt_m;
        std::vector<Leg> legs;
        if (role == "cap") {
            const Vec3 start = s.centre + Toward(from) * 35000.0;
            legs = Orbit(start, s.centre + Toward(enemy) * 8000.0, enemy + 0.5 * kPi, 15000.0, 5000.0, 4, s.centre + Toward(from) * 45000.0, h);
        } else if (role == "intercept") {
            legs = {{s.centre + Toward(from) * 25000.0, h}, {s.centre, h}, {s.centre + Toward(enemy) * 55000.0, h},
                    {s.centre + Toward(enemy + 0.6) * 40000.0, h}, {s.centre + Toward(from) * 40000.0, h}};
        } else if (role == "ground_attack") {
            legs = {{s.centre + Toward(enemy + kPi) * 30000.0, h}, {CityPoint(s), h}, {s.centre + Toward(enemy) * 60000.0, h}};
        } else {  // transport, helicopter, trainer: across the city
            const double r = a.helicopter ? 14000.0 : 35000.0;
            legs = {{s.centre + Toward(from) * r, h}, {CityPoint(s), h}, {s.centre + Toward(from + kPi + Uniform(-0.6, 0.6)) * r, h}};
        }
        // Our flights in an echelon right, a game value (the Pact pattern's pairs: 02_FLIGHTS_AND_PILOTS.md §1.3).
        std::map<std::string, std::vector<Vec3>> forms;
        for (int k = 0; k < std::max(1, n); ++k) forms["transit"].push_back(Vec3{(a.helicopter ? 100.0 : 300.0) * k, 0.0, 0.0});
        const std::string flight_id = cs + std::to_string(number);
        for (int k = 0; k < n; ++k) {
            const std::string id = cs + std::to_string(number) + "-" + std::to_string(k + 1);
            const std::string name = cs + " " + std::to_string(number) + "-" + std::to_string(k + 1) + ": " + a.name + " (" + role + ", ours)";
            ScenarioTarget t = Base(id, name, a.model, a.target_class, a.helicopter, a.speed_mps, a.helicopter ? 6.0 : 10.0, a.rcs_m2, at_s, n);
            t.friendly = true;
            t.board_type = 1;
            Route(t, s, legs, low, a.helicopter ? 8.0 : 60.0);
            t.flight = flight_id;
            t.slot = k + 1;
            Slot(t, forms, k);
            w.aircraft.push_back(std::move(t));
        }
        w.flight_defs.push_back(FlightDef(flight_id, cs + " " + std::to_string(number), true, true, forms));
        char line[200];
        std::snprintf(line, sizeof(line), "%s %d: %d %s, %s (ours)", cs.c_str(), number, n, a.name.c_str(), role.c_str());
        w.flights.push_back(line);
    }

    template <typename T>
    static void Note(std::vector<T>& v, const T& x) {
        if (std::find(v.begin(), v.end(), x) == v.end()) v.push_back(x);
    }

    std::shared_ptr<const WavesDef> def_;
    std::mt19937 rng_;
    double next_s_ = 0.0;
    int waves_ = 0;
    int flight_no_ = 10;
    int drops_ = 0;
    std::vector<std::pair<std::size_t, std::size_t>> deck_;  // (faction, aircraft type) still to come round
    std::vector<std::size_t> fdeck_;
    std::vector<Pending> pending_;
    std::vector<std::pair<std::string, std::string>> enemy_flown_;
    std::vector<std::string> friendly_flown_;
    std::vector<std::string> weapons_loaded_;
};

// The chance a defence of the testbed shoots down an enemy aircraft on its pass, decided once as the aircraft comes into
// its reach: the raids' own kill chance per pass (planner.hpp, PassKillChance), on what is left of the chord the
// aircraft will fly through the reach on its present heading, at the height band it flies (above ground: under
// kWaveLowAglM low, under kWaveMediumAglM medium, else high; game values, sam3d). Nothing suppresses or jams it and the
// pilot does not evade: the testbed is not the air war. 0 for an aircraft faster than the defence can engage. `dark`:
// the session's sun is down (roadmap R7, light.hpp): a defence without radar fire control aims by eye and its chance takes
// the air war's night factor (attrition.night, as mission_bridge.hpp has it for the abstract raids).
constexpr double kWaveLowAglM = 1500.0;
constexpr double kWaveMediumAglM = 8000.0;
inline double WavePassKillChance(const DefenceDef& d, double radius_m, const Vec3& site, const Vec3& position, const Vec3& velocity,
                                 double agl_m, bool helicopter, const TuningTable& tune, bool dark = false) {
    const double speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    if (d.max_target_mps > 0.0 && speed > d.max_target_mps) return 0.0;
    const double dx = position.x - site.x, dy = position.y - site.y;
    const double h = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    double chord = radius_m;  // hovering or climbing straight up: as if through the middle
    if (h > 1.0) {
        const double along = (dx * velocity.x + dy * velocity.y) / h;  // past the closest point: > 0
        const double cross2 = std::max(0.0, dx * dx + dy * dy - along * along);
        chord = std::max(0.0, std::sqrt(std::max(0.0, radius_m * radius_m - cross2)) - along);
    }
    const Profile p = agl_m < kWaveLowAglM ? Profile::Low : agl_m < kWaveMediumAglM ? Profile::Medium : Profile::High;
    const double env = !d.radar && dark ? tune.Get("attrition.night") : 1.0;
    return PassKillChance(d.lethality, chord, radius_m, p, helicopter, d.low_cover, d.medium_cover, d.radar, 0.0, 0.0, env, 0.0, tune);
}

}  // namespace airwar
}  // namespace simcore
