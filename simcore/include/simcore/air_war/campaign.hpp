// simcore/air_war/campaign.hpp — the enemy air command's war, hour by hour (docs/air_war/).
//
// AirCampaign owns the enemy (its staff, squadrons, ARMs, politics, intelligence picture) and the player's side as it
// really is (every object's place, state and rounds; the player's battery and its posture). Advance() runs the war:
//   * every planning cycle the staff plans an air tasking order (planner.hpp);
//   * the collection disciplines feed the picture: satellite passes, agents, ELINT on what radiates, radio intercept
//     of the battery's traffic (and of its dummy traffic), reconnaissance flights; reports arrive late;
//   * every package is flown when its time comes: in the air leg by leg in time (resolve.hpp, roadmap B5's A0: the
//     clock steps c2.step_h while anything is in the air), or, when it comes through the player's sector and the pacing
//     allows, handed to the game as the player's mission (raid_scenario.hpp builds the scenario, mission_bridge.hpp
//     brings the outcome back to CompletePlayerMission; a session that takes it, TakePlayerMission, lets the campaign
//     go on while it flies there);
//   * the enemy's command in the air (c2.hpp, roadmap B5): its posts hear their flights' reports, warn hunters, divert
//     what they may, scramble the alert pools the plan held back, and pass the rest to intelligence in minutes;
//   * what the enemy believes happened (pilots' claims, inflated; launch sightings; ELINT fixes) goes into its picture
//     and into its officers: what they remember of each tactic, their stress, reputation and grudges;
//   * each day: politics (pressure, support, events), replacements and ARM resupply, careers (relief, promotion,
//     replacement), and the player's side's intelligence summary of the enemy (what the player can know);
//   * every hour: repairs, mobile units moving, reports arriving, the picture ageing.
// Deterministic for a seed. The journal keeps the enemy's mind (hidden until the debrief) and what the player's side
// learns (visible).
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/air_front.hpp"
#include "simcore/air_war/c2.hpp"
#include "simcore/air_war/data.hpp"
#include "simcore/air_war/front_line.hpp"
#include "simcore/air_war/hot_phase.hpp"
#include "simcore/air_war/intel.hpp"
#include "simcore/air_war/logistics.hpp"
#include "simcore/air_war/officers.hpp"
#include "simcore/air_war/planner.hpp"
#include "simcore/air_war/resolve.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/air_war/types.hpp"
#include "simcore/air_war/unit_moves.hpp"
#include "simcore/ground_war/ground_war.hpp"
#include "simcore/nav/routes.hpp"

namespace simcore {
namespace airwar {

// What the player's side has learned about one enemy officer (docs/air_war/02_AIR_STAFF.md §6).
struct OfficerFile {
    bool name_known = false;
    std::string nickname;              // what our analysts call him before they know his name
    std::set<std::string> revealed;    // quirk ids our analysts are confident of
    std::set<std::string> told;        // quirks already reported
    bool character_known = false;      // his traits, from interrogations
};

// A raid as our side saw it (the warning net sees every raid into our territory): for the map and the INTSUM.
struct ObservedRaid {
    double h = 0.0;          // time over its target
    std::string entry;       // where it came in
    Vec3 entry_pos;
    Vec3 target_pos;         // what it went for (where our side saw it go)
    std::string target;      // the target's name, as our side knows it
    DayPart part = DayPart::Day;
    int aircraft = 0;
    int lost = 0;
    bool delivered = false;
    bool near_battery = false;  // through the battery's sector
    bool played = false;
    // How it came, as the warning net saw it (our side's indicators of the enemy's air front, F2.5): at low level, with
    // helicopters, and our object it went for (-1: none of ours).
    bool low = false;
    bool helicopters = false;
    int target_object = -1;
};

// One thing the player did to the campaign, in order: what a save keeps (the campaign is deterministic for its seed,
// so replaying the same actions in the same order rebuilds it exactly; save.hpp).
// A call the battery's people made on the air (our side's information pipeline: command/air_war_feed.hpp, roadmap P3).
struct Transmission {
    double h = 0.0;
    double intercept = 1.0;  // how much one such message tells the enemy's intercept service (the radio: everything)
    std::string key;         // the call's own name, so it is heard once
};

struct CampaignAction {
    // Take: a session took the raid waiting (the campaign goes on while it is flown there); Report: a played package's
    // pilot's report that reached the enemy's posts while the session ran (roadmap B5 §4.7).
    // PostAttack: an enemy air control post struck from outside the campaign's own processes (a session's strike, a call
    // for fire: roadmap S2), `what` the post's id, `by` who struck it.
    // UnitMove: one of our units began a move on an order its commander heard (track F, F4.2; unit_moves.hpp): `what` the
    // unit, `by` the order's key, `to` where to, `until_h` the hour it began, `observed_h` when he heard it; `in_step` the
    // order source's, taken at a whole hour inside a step (a load puts it back at that hour), else the caller's (MoveUnit).
    // FighterCover: our fighter regiment's duty by sector changed on its commander's priorities as its post held them
    // (track F, F4.3): `shares` the front's fighters by sector, `by` the order's key, `until_h` the hour, `observed_h` when
    // the post had it, `in_step` as UnitMove's (else the caller's, GiveFighterCover).
    enum class Kind { Advance, Relocate, Skip, Complete, Transmit, Take, Report, PostAttack, UnitMove, FighterCover };
    Kind kind = Kind::Advance;
    BatteryPosture posture;  // the battery's posture when it was done
    double until_h = 0.0;    // Advance
    Vec3 to;                 // Relocate
    double march_h = 0.0;    // Relocate: the march on the roads (0: there at once)
    PackageOutcome outcome;  // Complete
    double end_h = 0.0;      // Complete
    std::vector<Transmission> calls;  // Transmit
    int package_id = -1;     // Take, Report; Complete of a taken raid (else -1: the raid waiting)
    int flight = -1;         // Report: the flight whose pilot saw it
    std::string what;        // Report: "launch"
    Vec3 from;               // Report: where he was
    double sigma_m = 0.0;    // Report
    double observed_h = 0.0; // Report: when he saw it (the place is `to`)
    std::string by;          // PostAttack; UnitMove, FighterCover: the order's key
    bool in_step = false;    // UnitMove, FighterCover
    std::vector<double> shares;  // FighterCover: by sector
};

// The evidence our analysts gather from the raids they see.
struct RaidPatterns {
    int packages = 0;
    int by_part[kDayParts] = {};
    std::map<std::string, int> by_entry;
    int feints = 0;
    int decoy_waves = 0;
    int aircraft = 0;
    int double_taps = 0;
    int arm_attacks = 0;  // our radars hit, or made to go silent, by anti-radiation missiles (their sites report it)
};

// What the air war did with the ground war's units (track F, F3.7; docs/fronts/03_GROUND_WAR.md section 6), counted only
// with the ground war on: the enemy's strikes on our tied troops objects and what they took off their units, march
// columns' strength brought, our objects overrun and taken back, our mobile objects moved back from the line at once, tied
// troops moved with their units and the supply lines routed again to them, and the packages by C2's label (close support,
// battlefield interdiction, air interdiction) with the FSCL and as c2.bai_depth_m would have labelled the same strikes.
struct GroundAirStats {
    int troop_strikes = 0;
    double strength_lost = 0.0;    // summed, each as a share of its formation's establishment at the war's start
    int reinforced = 0;
    double strength_brought = 0.0; // the same measure
    int overrun = 0;
    int retaken = 0;
    int escapes = 0;
    int troop_moves = 0;           // a tied object moved with its unit (by more than a metre: a step of the line or an order)
    double troops_moved_km = 0.0;  // their moves summed
    int supply_reroutes = 0;
    int cas = 0;
    int bai = 0;
    int ai = 0;
    int bai_fixed = 0;
    int ai_fixed = 0;
    // The parts' cost (steady_clock, ms) while the ground war is profiled (AirCampaign::ProfileGround): the strikes found and
    // taken off the units, the supply lines' rises and the units' supply, the troops' shares, places and supply ends, the
    // overrun and escapes on a moved line, the packages' labels.
    enum Part { Strikes, Supply, Sync, Overrun, Labels, kParts };
    double ms[kParts] = {};
    double Ms() const {
        double s = 0.0;
        for (double v : ms) s += v;
        return s;
    }
    static const char* PartName(int k) {
        static const char* const kNames[] = {"strikes", "supply", "sync", "overrun", "labels"};
        return k >= 0 && k < kParts ? kNames[k] : "?";
    }
};

class AirCampaign {
public:
    bool Init(const AirWarData& data, unsigned seed, std::string* error = nullptr) {
        data_ = data;
        ground_.reset();

        seed_ = seed;
        actions_.clear();
        rng_ = Rng(static_cast<std::uint64_t>(seed) * 0x9E3779B97F4A7C15ull + 1234567u);
        corridor_intel_.Init(data_.own_air.learning, static_cast<std::uint64_t>(seed) * 0x9E3779B97F4A7C15ull + 0xB10ull);
        corridor_days_fed_ = 0;
        now_h_ = 0.0;
        const FrontDef* front = data_.theatre.FrontFor(data_.faction.front);
        if (front == nullptr) {
            if (error != nullptr) *error = "the faction's front is not in the theatre";
            return false;
        }
        // The front's line of contact (track F, F2.1): its base line, sectors and stations, every offset zero.
        front_line_.Build(*front, data_.tuning.Get("air_front.station_m"));
        truth_.clear();
        for (const TheatreObject& o : data_.theatre.objects) {
            ObjectState s;
            s.id = o.id;
            s.pos = o.pos;
            s.missiles = o.defence.present ? o.defence.missiles : -1;
            truth_.push_back(s);
        }
        player_ = data_.theatre.IndexOf(data_.theatre.PlayerBattery()->id);
        dummies_.clear();
        for (std::size_t i = 0; i < data_.theatre.objects.size(); ++i) {
            const TheatreObject& o = data_.theatre.objects[i];
            if (o.decoy && o.looks_like == ObjectKind::SamSite && Dist2d(o.pos, truth_[static_cast<std::size_t>(player_)].pos) < 8000.0) {
                dummies_.push_back(static_cast<int>(i));
            }
        }
        // In a war on this front the battery deploys where the front says (its dummies keep their places around it).
        if (front->has_battery_position) {
            const Vec3 shift = front->battery_position - truth_[static_cast<std::size_t>(player_)].pos;
            truth_[static_cast<std::size_t>(player_)].pos = front->battery_position;
            for (int di : dummies_) truth_[static_cast<std::size_t>(di)].pos += shift;
        }
        battery_track_.clear();
        battery_track_.push_back({0.0, truth_[static_cast<std::size_t>(player_)].pos});
        squadrons_.clear();
        for (const SquadronDef& s : data_.faction.squadrons) {
            SquadronState st;
            st.id = s.id;
            st.aircraft = s.aircraft;
            st.total = Arrived(s, 1);
            squadrons_.push_back(st);
        }
        support_ = SupportNow();
        supply_.Init(data_, truth_);
        arm_stock_ = data_.faction.logistics.arm_stock;
        for (const AircraftType& a : data_.faction.aircraft) {
            if (a.missile && a.InService(data_.theatre.year)) missile_stock_[a.id] = a.stock;
        }
        const PoliticsDef& pd = data_.faction.politics;
        politics_.support = pd.support;
        politics_.pressure = pd.pressure;
        politics_.banned = pd.banned;
        politics_.city_radius_m = pd.no_strike_city_radius_m;
        politics_.max_depth_m = pd.max_depth_m;
        Rng staff_rng = rng_.Fork(17);
        staff_.Build(data_, staff_rng, 0.0);
        // The command posts in the air and their duty officers (roadmap B5), after the staff; every C2 and in-the-air draw
        // from forks of a root fixed here, never from the campaign's stream.
        airborne_.clear();
        live_.clear();
        on_call_.clear();
        last_fac_hour_ = -1000000;
        c2_root_ = Rng(static_cast<std::uint64_t>(seed) * 0xD6E8FEB86659FD93ull + 0xB5B5B5ull);
        Rng post_rng = rng_.Fork(23);
        c2_.Init(data_, staff_, post_rng, c2_root_, 0.0);
        weather_.clear();
        const double clear = data_.tuning.Get("weather.clear_share");
        const double poor = data_.tuning.Get("weather.poor_share");
        // By month (roadmap S7, weather.mode 1, a dated theatre): the same draw a day, the shares the month's.
        const bool by_month = data_.tuning.Get("weather.mode") >= 1.0 && data_.theatre.day_of_year > 0;
        const double poor_part = (1.0 - clear) > 0.0 ? poor / (1.0 - clear) : 0.0;
        for (int day = 0; day < 120; ++day) {
            const double u = rng_.Uniform();
            double c = clear, p = poor;
            if (by_month) {
                const double not_clear = NotClearShare((data_.theatre.day_of_year - 1 + day) % 365 + 1);
                c = 1.0 - not_clear;
                p = poor_part * not_clear;
            }
            weather_.push_back(u < c ? Weather::Clear : (u < 1.0 - p ? Weather::Cloudy : Weather::Poor));
        }
        PriorIntel();
        StartAirFront();
        if (!StartUnitMoves(error)) return false;
        if (!StartFighterCover(error)) return false;
        if (!StartGroundWar(error)) return false;
        day_ = 1;
        Log("career", "staff", StaffLine(), false);
        Log("own_intel", "intelligence", "War with " + data_.faction.name + ". " + data_.faction.description, true);
        // The opening plan was made before the war: its first raids come at the start.
        const double lead = data_.faction.doctrine.lead_time_h;
        now_h_ = -lead;
        Plan();
        now_h_ = 0.0;
        next_plan_h_ = data_.faction.doctrine.planning_cycle_h - lead;
        ok_ = true;
        return true;
    }

    // ------------------------------------------------------------------ running the war

    // Run until `until_h`, or until a raid the player is to fly comes due (then true, and PlayerMission() is set).
    bool Advance(double until_h) {
        if (!ok_ || pending_ >= 0) return pending_ >= 0;
        Record(CampaignAction::Kind::Advance).until_h = until_h;
        while (now_h_ < until_h - 1e-9) {
            double step_end = std::min(until_h, std::floor(now_h_ + 1.0 + 1e-9));
            if (marching_ && march_until_h_ > now_h_ + 1e-9) step_end = std::min(step_end, march_until_h_);  // it arrives then
            if (StepTo(step_end)) return true;
        }
        return false;
    }

    bool HasPlayerMission() const { return pending_ >= 0; }
    const Package* PlayerMission() const { return pending_ >= 0 ? &packages_[static_cast<std::size_t>(pending_)] : nullptr; }
    Package* PlayerMissionMutable() { return pending_ >= 0 ? &packages_[static_cast<std::size_t>(pending_)] : nullptr; }

    // The player's mission is over: `outcome` is what happened inside the scenario (mission_bridge.hpp); the rest of
    // the raid (beyond the player's area, after `end_h`) is resolved abstractly.
    void CompletePlayerMission(PackageOutcome outcome, double end_h) {
        if (pending_ < 0) return;
        CampaignAction& a = Record(CampaignAction::Kind::Complete);
        a.outcome = outcome;
        a.end_h = end_h;
        Package& p = packages_[static_cast<std::size_t>(pending_)];
        CompletePlayed(p, outcome, now_h_, end_h);
        last_player_mission_h_ = now_h_;
        pending_ = -1;
    }

    // A session takes the raid waiting (package_injector.hpp's LivePackages, roadmap B5 §4.6): it is flown there, and
    // the campaign no longer stands still meanwhile; other packages fly and resolve while it does. Recorded.
    bool TakePlayerMission() {
        if (pending_ < 0) return false;
        Package& p = packages_[static_cast<std::size_t>(pending_)];
        Record(CampaignAction::Kind::Take).package_id = p.id;
        p.played = true;
        live_[p.id] = now_h_;
        pending_ = -1;
        return true;
    }
    // A raid a session took, still flying there.
    bool LiveMission(int package_id) const { return live_.count(package_id) > 0; }
    const Package* PackageById(int id) const {
        for (const Package& p : packages_) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }
    // A taken raid is over in the session: its outcome, as CompletePlayerMission's, the rest of it resolved from where the
    // session left it. Recorded.
    void CompleteLiveMission(int package_id, PackageOutcome outcome, double end_h) {
        const auto it = live_.find(package_id);
        if (it == live_.end()) return;
        CampaignAction& a = Record(CampaignAction::Kind::Complete);
        a.outcome = outcome;
        a.end_h = end_h;
        a.package_id = package_id;
        const double from_h = it->second;
        live_.erase(it);
        for (Package& p : packages_) {
            if (p.id == package_id) CompletePlayed(p, outcome, from_h, end_h);
        }
    }
    // A played package's pilot saw a launch while the session ran (roadmap B5 §4.7): his report goes to the enemy's posts
    // now (in flight, where a post's radio covers him). Recorded, so a save replays it at the same hour.
    void NoteReport(int package_id, int flight, const std::string& what, const Vec3& where, double sigma_m, double observed_h, const Vec3& from) {
        CampaignAction& a = Record(CampaignAction::Kind::Report);
        a.package_id = package_id;
        a.flight = flight;
        a.what = what;
        a.to = where;
        a.sigma_m = sigma_m;
        a.observed_h = observed_h;
        a.from = from;
        ApplyReport(a);
    }

    // An enemy air control post struck from outside the campaign's own processes (a session's strike on it, a call for
    // fire: roadmap S2): out now, back after its kind's reconstitution. Recorded. False: no such post, or out already.
    bool StrikePost(const std::string& post_id, const std::string& by) {
        CampaignAction& a = Record(CampaignAction::Kind::PostAttack);
        a.what = post_id;
        a.by = by;
        C2Context cx = C2Ctx();
        return c2_.StrikePost(post_id, now_h_, by, cx);
    }
    // The posts' orders for the flights of packages a session is flying (roadmap S2), decided since the last call. Not
    // recorded: the campaign decides them again on a replay, and the session's flights' records keep what they did.
    std::vector<C2SessionOrder> TakeSessionOrders() { return c2_.TakeSessionOrders(); }

    // The enemy's command in the air (roadmap B5): its posts, pools, messages and what they did.
    const AirC2& C2() const {
        c2_.Bind(data_, staff_, FrontView());  // a copied campaign's posts read its own data
        return c2_;
    }
    // Packages in the air now (the abstract ones, stepped in time).
    int AirborneCount() const { return static_cast<int>(airborne_.size()); }

private:
    void CompletePlayed(Package& p, const PackageOutcome& outcome, double from_h, double end_h) {
        p.played = true;
        for (const LossRecord& l : outcome.losses) {
            if (l.flight >= 0 && l.flight < static_cast<int>(p.flights.size())) ++p.flights[static_cast<std::size_t>(l.flight)].lost;
        }
        for (int i : outcome.released_flights) {
            if (i >= 0 && i < static_cast<int>(p.flights.size())) p.flights[static_cast<std::size_t>(i)].released = true;
        }
        for (int i : outcome.aborted_flights) {
            if (i >= 0 && i < static_cast<int>(p.flights.size())) p.flights[static_cast<std::size_t>(i)].aborted = true;
        }
        const std::string& own = data_.theatre.objects[static_cast<std::size_t>(player_)].id;
        int kills = 0;
        for (const LossRecord& l : outcome.losses) kills += l.by == own ? 1 : 0;
        const int missiles_down = static_cast<int>(std::count(outcome.missiles_downed.begin(), outcome.missiles_downed.end(), own));
        kills_by_player_ += kills;
        CrewProgress(kills + missiles_down);
        if (outcome.reported) {
            // Our side knows what the battery's records say (roadmap 18.1), not what happened.
            Log("player_mission", "result", "Mission against '" + p.name + "' flown: the battery reports " +
                                                std::to_string(outcome.reported_destroyed) + " destroyed of " +
                                                std::to_string(outcome.reported_targets) + " targets plotted in its sector, " +
                                                std::to_string(outcome.reported_missiles) + (outcome.reported_missiles == 1 ? " missile" : " missiles") +
                                                " fired.", true);
        } else {
            Log("player_mission", "result", "Mission against '" + p.name + "' flown: " +
                                                (p.Missiles() > 0 ? std::to_string(outcome.missiles_downed.size()) + " of " + std::to_string(p.Missiles()) + " cruise missiles"
                                                                  : std::to_string(outcome.Lost()) + " of " + std::to_string(p.Aircraft())) +
                                                " shot down in the battery's sector.", true);
        }
        ApplyOutcome(p, outcome);
        // The routes as flown (roadmap S7, route.mode 2): each flight that left the session goes on from where it was.
        if (data_.tuning.Get("route.mode") >= 2.0) {
            for (const PackageOutcome::FlownRecord& f : outcome.flown) {
                if (f.flight >= 0 && f.flight < static_cast<int>(p.flights.size())) FlownInto(p.flights[static_cast<std::size_t>(f.flight)], f.at, f.t_h);
            }
        }
        // The rest of the raid, without the player's battery: every part of every route but what the scenario flew
        // (inside its circle while it ran, where the other defences took their chances: mission_bridge.hpp).
        ResolveContext rc = Context();
        rc.skip_defence = data_.theatre.objects[static_cast<std::size_t>(player_)].id;
        rc.played_centre = truth_[static_cast<std::size_t>(player_)].pos;
        rc.played_radius_m = data_.tuning.Get("scenario.clip_m");
        rc.played_from_h = from_h;
        rc.played_to_h = end_h;
        Resolver r;
        PackageOutcome rest = r.Resolve(p, rc);
        if (!rest.losses.empty() || rest.delivered || !rest.recce_seen.empty()) ApplyOutcome(p, rest, false);
        ++player_missions_;
    }

public:

    // The player declined the mission: it is resolved abstractly like any other.
    void SkipPlayerMission() {
        if (pending_ < 0) return;
        Record(CampaignAction::Kind::Skip);
        const int i = pending_;
        pending_ = -1;
        if (Legacy()) {
            ResolveAbstract(packages_[static_cast<std::size_t>(i)]);
            return;
        }
        TakeOff(i);  // it flies on in time like any other
    }

    // ------------------------------------------------------------------ the player's side

    BatteryPosture& Posture() { return posture_; }
    const BatteryPosture& Posture() const { return posture_; }

    // Can the battery move there? Within `max_m` of where it is, and no closer than 8 km to the enemy's front.
    bool CanRelocateBattery(const Vec3& to, std::string* why = nullptr, double max_m = 30000.0) const {
        if (marching_) {
            if (why != nullptr) *why = "on the march already";
            return false;
        }
        const Vec3 here = truth_[static_cast<std::size_t>(player_)].pos;
        if (Dist2d(here, to) > max_m) {
            if (why != nullptr) *why = "too far for one move (" + std::to_string(static_cast<int>(max_m / 1000.0)) + " km at most)";
            return false;
        }
        if (BehindFront(to) < 8000.0) {
            if (why != nullptr) *why = "too close to the front";
            return false;
        }
        return true;
    }

    // The player moved the battery (and its dummies with it): at once, or (roadmap G9.5) as a march on the roads that takes
    // `march_h` (nav::SessionNav::MarchHours), the battery out of the fight and still at its old position on the enemy's
    // picture until it arrives.
    void RelocateBattery(const Vec3& to, double march_h = 0.0) {
        CampaignAction& a = Record(CampaignAction::Kind::Relocate);
        a.to = to;
        a.march_h = std::max(0.0, march_h);
        if (a.march_h <= 0.0) {
            MoveBattery(to);
            return;
        }
        marching_ = true;
        march_to_ = to;
        march_until_h_ = now_h_ + a.march_h;
        Log("own_intel", "battery", "The battery is on the march to its new position, " + std::to_string(static_cast<int>(a.march_h * 60.0 + 0.5)) +
                                        " minutes by the roads.", true);
    }
    // On the march to a new position (G9.5): where to, and the hour it arrives.
    bool Marching() const { return marching_; }
    const Vec3& MarchTo() const { return march_to_; }
    double MarchArriveH() const { return march_until_h_; }

    // The battery's calls on the air, from our side's information pipeline (roadmap P3, docs/command/06_INTEGRATION.md
    // §1): from the first note on, the enemy's radio intercept hears these instead of guessing the battery's traffic
    // from its posture's radio discipline. Each call once (by its key); recorded, so a save replays them.
    void NoteTransmissions(const std::vector<Transmission>& calls) {
        std::vector<Transmission> fresh;
        for (const Transmission& t : calls) {
            if (noted_.insert(t.key).second) fresh.push_back(t);
        }
        if (fresh.empty() && traffic_fed_) return;
        Record(CampaignAction::Kind::Transmit).calls = fresh;
        traffic_fed_ = true;
        on_air_.insert(on_air_.end(), fresh.begin(), fresh.end());
    }
    // The enemy hears the battery's own calls (else it guesses from the posture).
    bool TrafficFromPipeline() const { return traffic_fed_; }
    // The battery's calls noted so far.
    int CallsOnAir() const { return static_cast<int>(noted_.size()); }
    // Radio intercept reports about the battery the enemy has had (the dummy traffic's ghosts not counted).
    int ComintHeard() const { return comint_heard_; }
    // The sympathisers' reports of our mobile air defence (roadmap S7), for the balance tool and the tests.
    int SympathiserReports() const { return sympathiser_reports_; }

    // The player's actions so far, in order (save.hpp), and the seed the campaign started from.
    const std::vector<CampaignAction>& Actions() const { return actions_; }
    unsigned Seed() const { return seed_; }

    // Rebuild a campaign from its actions (after Init with the same data and seed). False if they don't fit it.
    bool Replay(const std::vector<CampaignAction>& actions, std::string* error = nullptr) {
        // Our units' moves on the orders heard inside a step go back at their hours (F4.2), whatever the order source now.
        for (const CampaignAction& a : actions) {
            if ((a.kind != CampaignAction::Kind::UnitMove && a.kind != CampaignAction::Kind::FighterCover) || !a.in_step) continue;
            Recorded r;
            r.h = a.until_h;
            r.order.key = a.by;
            r.order.unit = a.what;
            r.order.to = a.to;
            r.order.heard_h = a.observed_h;
            r.order.fighter_shares = a.shares;  // empty for a move
            inbox_.push_back(r);
        }
        for (std::size_t i = 0; i < actions.size(); ++i) {
            const CampaignAction& a = actions[i];
            posture_ = a.posture;
            switch (a.kind) {
                case CampaignAction::Kind::UnitMove:
                    if (!a.in_step) MoveUnit(a.what, a.to, a.by, a.observed_h);
                    break;
                case CampaignAction::Kind::FighterCover:
                    if (!a.in_step) GiveFighterCover(a.shares, a.by, a.observed_h);
                    break;
                case CampaignAction::Kind::Advance: Advance(a.until_h); break;
                case CampaignAction::Kind::Relocate: RelocateBattery(a.to, a.march_h); break;
                case CampaignAction::Kind::Transmit: NoteTransmissions(a.calls); break;
                case CampaignAction::Kind::Take:
                    if (pending_ < 0 || packages_[static_cast<std::size_t>(pending_)].id != a.package_id) {
                        if (error != nullptr) *error = "action " + std::to_string(i) + ": the raid taken is not the one waiting (the save doesn't fit the data)";
                        return false;
                    }
                    TakePlayerMission();
                    break;
                case CampaignAction::Kind::Report:
                    NoteReport(a.package_id, a.flight, a.what, a.to, a.sigma_m, a.observed_h, a.from);
                    break;
                case CampaignAction::Kind::PostAttack: StrikePost(a.what, a.by); break;
                case CampaignAction::Kind::Complete:
                    if (a.package_id >= 0) {
                        if (!LiveMission(a.package_id)) {
                            if (error != nullptr) *error = "action " + std::to_string(i) + ": raid " + std::to_string(a.package_id) + " is not in a session";
                            return false;
                        }
                        CompleteLiveMission(a.package_id, a.outcome, a.end_h);
                        break;
                    }
                    if (pending_ < 0) {
                        if (error != nullptr) *error = "action " + std::to_string(i) + ": no raid was waiting (the save doesn't fit the data)";
                        return false;
                    }
                    CompletePlayerMission(a.outcome, a.end_h);
                    break;
                case CampaignAction::Kind::Skip:
                    if (pending_ < 0) {
                        if (error != nullptr) *error = "action " + std::to_string(i) + ": no raid was waiting (the save doesn't fit the data)";
                        return false;
                    }
                    SkipPlayerMission();
                    break;
            }
        }
        return true;
    }

private:
    void MoveBattery(const Vec3& to) {
        ObjectState& b = truth_[static_cast<std::size_t>(player_)];
        const Vec3 shift = to - b.pos;
        b.pos = to;
        b.last_moved_h = now_h_;
        battery_track_.push_back({now_h_, to});
        for (int di : dummies_) truth_[static_cast<std::size_t>(di)].pos += shift;
        Log("own_intel", "battery", "The battery moved " + std::to_string(static_cast<int>(Dist2d(Vec3{}, shift) / 1000.0 + 0.5)) +
                                        " km to a new position.", true);
    }

public:

    std::string CrewTier() const { return crew_tier_; }
    void SetCrewTier(const std::string& t) { crew_tier_ = t; }

    // ------------------------------------------------------------------ views

    double Now() const { return now_h_; }
    int Day() const { return day_; }
    std::string Clock() const { return FormatHour(now_h_, data_.theatre.start_hour); }
    double LocalHour() const { return std::fmod(data_.theatre.start_hour + now_h_, 24.0); }
    const AirWarData& Data() const { return data_; }
    const AirStaff& Staff() const { return staff_; }
    const IntelPicture& Intel() const { return intel_; }
    // The front's line of contact and its sectors (track F, front_line.hpp); the ground war moves it (F3.3).
    const LineOfContact& FrontLine() const { return front_line_; }
    // The ground war (track F, F3.3; ground.mode 1 on a theatre with a country map, else null).
    const groundwar::GroundWar* Ground() const { return ground_on_ ? &ground_war_ : nullptr; }
    // fronts_bench's: time the ground war's parts (steady_clock; nothing in the war reads it).
    void ProfileGround(bool on) { ground_war_.SetProfiling(on); }
    // The air front's measures (track F, F2.2; air_front.mode 1, else empty): each sector's believed cover over its next
    // band as of the last plan, the book that keeps their baselines, and the losses to ground fire per low-level sortie in
    // a sector over the last `days` days (1-3) to now.
    bool AirFrontOn() const { return air_front_on_; }
    const std::vector<SectorCover>& SectorCovers() const { return sector_cover_; }
    const SectorCoverBook& CoverBook() const { return cover_book_; }
    const SectorLossWindow& SectorLosses() const { return sector_losses_; }
    double SectorLossRate(int sector, int days) const { return sector_losses_.Rate(sector, SectorLossWindow::DayOf(now_h_), days); }
    // The air front itself (F2.3; air_front.mode 1, else empty): each sector's, as the air staff last decided it, and its
    // fixed-wing depth beyond the line of contact by sector (what the planner, routes and C2 read from F2.4).
    const AirFrontState& AirFront() const { return air_front_; }
    const std::vector<double>& AirFrontDepths() const { return air_front_depth_; }
    const std::vector<double>& HeloFrontDepths() const { return air_front_helo_depth_; }
    // And each sector's reward over its next band as the last decision weighed it (fronts_bench's account, F6.1).
    const std::vector<double>& AirFrontRewards() const { return air_front_reward_; }
    // Its own axes advancing by sector, as its picture had them at the last decision (F7.2; air_front.mode 2 with the
    // ground war, else empty): what the close support need of a step's worth counted.
    const std::vector<int>& AirFrontAdvancing() const { return air_front_advancing_; }
    // Both as the planner, the route planner and C2 read them (F2.4): off at mode 0, so their fixed depths stand.
    AirFrontView FrontView() const {
        AirFrontView v;
        v.line = &front_line_;
        v.depth_m = &air_front_depth_;
        v.helo_depth_m = &air_front_helo_depth_;
        v.fscl_m = &fscl_m_;  // NATO's FSCL with the ground war (F3.7); empty otherwise
        return v;
    }
    // The air war and the ground war's units (F3.7; zero without the ground war).
    const GroundAirStats& GroundAir() const { return ground_air_; }

    // ------------------------------------------------------------------ our units' moves on orders (track F, F4.2)

    // On under ad_strategy.mode 1 where the front has a line: the national SAM units, the army's Kub and the radar
    // companies (the classes of data/campaign/ad_strategy.json "moves") move only on orders, by their class's norms, on the
    // roads; off the air while they move; lost with the ground if the line overruns them before they get away.
    bool UnitMovesOn() const { return moves_on_; }
    const UnitMoveRules& MoveRules() const { return move_rules_; }
    // The class object i moves by on orders (-1: none, or off).
    int MoveClassOf(std::size_t i) const { return moves_on_ && i < move_class_.size() ? move_class_[i] : -1; }
    // Who tells the campaign the orders its units heard (command::CampaignFeed::OrderSource()): asked at whole hours inside
    // the steps, from now on. A copy of the campaign keeps it: give the copy its own, or none. Not saved: the moves it gave
    // are, and a load puts them back at their hours with or without a source.
    void SetOrderSource(UnitOrderSource source) {
        order_source_ = std::move(source);
        next_source_h_ = now_h_;
    }
    // A move begun now on an order heard at heard_h (-1: now): by its class's norms, on the roads. Recorded. False when it
    // does not move on orders (or the mode is off), is wrecked, overrun or on a move already, or the key was used.
    bool MoveUnit(const std::string& id, const Vec3& to, const std::string& key = "", double heard_h = -1.0) {
        UnitOrder o;
        o.unit = id;
        o.to = to;
        o.heard_h = heard_h < 0.0 ? now_h_ : heard_h;
        o.key = key.empty() ? "move:" + id + "@" + std::to_string(static_cast<long long>(std::llround(now_h_ * 100.0))) : key;
        return MoveUnitNow(o, false);
    }
    // Other rules than data/campaign/ad_strategy.json's "moves" (tests, balance): only where the moves are on, after Init and
    // before the first step. Not saved: a load reads the data's.
    bool UseMoveRules(const UnitMoveRules& rules) {
        if (!moves_on_ || now_h_ > 0.0 || !moves_.empty() || rules.classes.empty()) return false;
        move_rules_ = rules;
        ApplyMoveRules();
        return true;
    }
    // Every move begun, in order, as it went (the stages, the hours, the march on the roads).
    const std::vector<UnitMove>& UnitMoves() const { return moves_; }
    // Units that move on orders the enemy's line overran before they got away (lost with the ground).
    int UnitsOverrun() const { return units_overrun_; }

    // ------------------------------------------------------------------ our fighters by sector (track F, F4.3)

    // On under ad_strategy.mode 1 where the front has a line and data/campaign/ad_strategy.json has its "fighters" block:
    // the front's fighters (FrontDef::friendly_fighters) are shared over its sectors as the fighter regiment's post holds its
    // commander's priorities (campaign::PriorityShares), each change heard through the order source (or given by a caller)
    // and recorded; the sum over the sectors stays the front's number times their count. Until the first change, and off,
    // every sector has the front's number.
    bool FighterCoverOn() const { return fighters_on_; }
    const FighterCoverRules& FighterRules() const { return fighter_rules_; }
    // The front's number (0 without a front).
    double FrontFighters() const {
        const FrontDef* f = data_.theatre.FrontFor(data_.faction.front);
        return f != nullptr ? f->friendly_fighters : 0.0;
    }
    // Our fighters' threat at a point: its sector's cover (the air fight's), else the front's number. A cached lookup.
    double FighterCoverAt(const Vec3& p) const { return FighterView().At(p, FrontFighters()); }
    // The same as the enemy's staff believes it (its planner's): the front's number until its packages flew there.
    double BelievedFighterCoverAt(const Vec3& p) const { return BelievedFighterView().At(p, FrontFighters()); }
    FighterCoverView FighterView() const {
        FighterCoverView v;
        if (fighters_on_) {
            v.line = &front_line_;
            v.by_sector = &fighter_cover_;
        }
        return v;
    }
    FighterCoverView BelievedFighterView() const {
        FighterCoverView v;
        if (fighters_on_) {
            v.line = &front_line_;
            v.by_sector = &fighter_belief_;
        }
        return v;
    }
    // By sector (empty: none given yet, the front's number): the regiment's shares, the cover, the enemy's belief.
    const std::vector<double>& FighterShares() const { return fighter_shares_; }
    const std::vector<double>& FighterCover() const { return fighter_cover_; }
    const std::vector<double>& FighterBelief() const { return fighter_belief_; }
    // Every change of the regiment's duty by sector, in order.
    struct FighterCoverChange {
        std::string key;
        double h = 0.0;        // taken (a whole hour inside a step, or the caller's hour)
        double heard_h = 0.0;  // when the regiment's post had the order
        std::vector<double> shares, cover;
    };
    const std::vector<FighterCoverChange>& FighterCoverChanges() const { return fighter_changes_; }
    // The regiment's duty by sector given now (a caller's: tests, a tool), on an order heard at heard_h (-1: now). Recorded.
    // False when off, the key was used, or the shares are not one a sector.
    bool GiveFighterCover(const std::vector<double>& shares, const std::string& key = "", double heard_h = -1.0) {
        UnitOrder o;
        o.unit = fighter_rules_.post;
        o.heard_h = heard_h < 0.0 ? now_h_ : heard_h;
        o.key = key.empty() ? "fighters@" + std::to_string(static_cast<long long>(std::llround(now_h_ * 100.0))) : key;
        o.fighter_shares = shares;
        return FighterCoverNow(o, false);
    }
    // The fixed-wing and the helicopters' air front at a point: its sector's (0 and 0 at mode 0).
    double AirFrontAt(const Vec3& p) const {
        if (!air_front_on_ || air_front_.sectors.empty()) return 0.0;
        const int s = front_line_.SectorOf(p);
        return s >= 0 && static_cast<std::size_t>(s) < air_front_.sectors.size() ? air_front_.sectors[static_cast<std::size_t>(s)].depth_m : 0.0;
    }
    double HeloFrontAt(const Vec3& p) const {
        if (!air_front_on_ || air_front_.sectors.empty()) return 0.0;
        const int s = front_line_.SectorOf(p);
        return s >= 0 && static_cast<std::size_t>(s) < air_front_.sectors.size() ? air_front_.sectors[static_cast<std::size_t>(s)].helo_depth_m : 0.0;
    }
    const std::vector<JournalEntry>& Journal() const { return journal_; }
    const std::vector<Package>& Packages() const { return packages_; }
    const std::vector<ObjectState>& Truth() const { return truth_; }
    const std::vector<SquadronState>& Squadrons() const { return squadrons_; }
    const PoliticsState& Politics() const { return politics_; }
    double ArmStock() const { return arm_stock_; }
    // Cruise missiles of a type left to the enemy (0 for a type not in service in the war's year).
    // The enemy's support orbits as of the last plan, the support sorties flown, and the aircraft arrived in its build-up
    // after the first day (docs/air_war/11_SUPPORT.md, 12_FORCES.md).
    const SupportLevel& Support() const { return support_; }
    // Our supply lines: their ways, and what they delivered and lost (docs/air_war/14_LOGISTICS.md).
    const SupplyNetwork& Supply() const { return supply_; }
    int SupportSorties() const { return support_sorties_; }
    int Reinforcements() const { return reinforcements_; }
    // Aircraft in theatre now, manned (not cruise missiles or drones), whole or damaged.
    int AircraftInTheatre() const {
        int n = 0;
        for (const SquadronState& st : squadrons_) {
            const AircraftType* a = data_.faction.Aircraft(st.aircraft);
            if (a != nullptr && !a->missile && !a->drone) n += std::max(0, st.total - st.lost);
        }
        return n;
    }

    double MissileStock(const std::string& id) const {
        const auto it = missile_stock_.find(id);
        return it == missile_stock_.end() ? 0.0 : it->second;
    }
    int PlayerIndex() const { return player_; }
    const std::vector<int>& Dummies() const { return dummies_; }
    // Where the battery has been: (campaign hour, position) from the start, one entry per move (the information
    // pipeline's battery post follows it: simcore/command/air_war_feed.hpp).
    const std::vector<std::pair<double, Vec3>>& BatteryTrack() const { return battery_track_; }
    Weather WeatherOn(int day) const {
        return weather_[static_cast<std::size_t>(std::min(std::max(day - 1, 0), static_cast<int>(weather_.size()) - 1))];
    }
    Weather WeatherNow() const { return WeatherAt(now_h_); }
    int Phase() const { return phase_; }
    int PlayerMissions() const { return player_missions_; }
    // Enemy aircraft the player's battery shot down in the missions the player fought (what the crew saw itself).
    int KillsByPlayer() const { return kills_by_player_; }
    const std::map<int, OfficerFile>& Files() const { return files_; }
    const std::vector<ObservedRaid>& ObservedRaids() const { return observed_; }
    int AircraftLost() const {
        int n = 0;
        for (const SquadronState& s : squadrons_) n += s.lost;
        return n;
    }
    // An observer of each package's outcome as it is applied (the balance tool's tallies, roadmap B14): it reads, it
    // changes nothing, and it is not saved.
    using OutcomeObserver = std::function<void(const Package&, const PackageOutcome&)>;
    void ObserveOutcomes(OutcomeObserver f) { observer_ = std::move(f); }

    // Our side has seen the enemy send decoys (or knows its planner likes them): the battery is warned.
    bool DecoysExpected() const {
        if (patterns_.decoy_waves > 0) return true;
        for (const auto& f : files_) {
            const Officer& o = staff_.officers[static_cast<std::size_t>(f.first)];
            for (const QuirkDef& q : o.quirks) {
                if (q.Effect("decoys") > 0.0 && f.second.revealed.count(q.id) > 0) return true;
            }
        }
        return false;
    }

    // Our radars have come under ARM attack in this war: the batteries fight under the ARM drill from then on (the
    // scenario doctrine's arm_threat, voenizdat_osa_1991 pp262-264).
    bool ArmsExpected() const { return patterns_.arm_attacks > 0; }

    // How well the enemy knows where the player's battery is, 0..1 (the XCOM "knowledge" of the player).
    double EnemyKnowledgeOfBattery() const {
        const IntelEntry* e = intel_.Find(data_.theatre.objects[static_cast<std::size_t>(player_)].id);
        if (e == nullptr) return 0.0;
        const double off = Dist2d(e->pos, truth_[static_cast<std::size_t>(player_)].pos);
        const double where = 1.0 / (1.0 + std::pow((off + e->sigma_m) / 2500.0, 2.0));
        return Clamp01(e->exists * where * (e->function > 0.2 ? 1.0 : 0.5));
    }

    // Our intelligence summary: what the player's side knows and believes (visible entries of the last `hours`).
    std::string Intsum(double hours = 24.0) const {
        std::ostringstream s;
        s << "INTSUM " << Clock() << " - " << data_.faction.name << "\n";
        s << "Weather today: " << WeatherName(WeatherNow()) << ".\n";
        for (const JournalEntry& j : journal_) {
            if (j.visible && j.h >= now_h_ - hours) s << "  " << FormatHour(j.h, data_.theatre.start_hour) << "  " << j.text << "\n";
        }
        s << "Enemy staff as we know it:\n";
        for (int i : staff_.Active()) {
            const Officer& o = staff_.officers[static_cast<std::size_t>(i)];
            if (o.def.role == StaffRole::SquadronLeader) continue;
            s << "  " << DossierLine(i) << "\n";
        }
        return s.str();
    }

    // One line on an officer, as our side knows him.
    std::string DossierLine(int idx) const {
        const Officer& o = staff_.officers[static_cast<std::size_t>(idx)];
        const auto it = files_.find(idx);
        const OfficerFile* f = it == files_.end() ? nullptr : &it->second;
        std::string who = std::string(StaffRoleName(o.def.role)) + ": ";
        if (f != nullptr && f->name_known) {
            who += o.Title();
        } else if (f != nullptr && !f->nickname.empty()) {
            who += "\"" + f->nickname + "\" (name unknown)";
        } else {
            who += "unknown";
        }
        if (f != nullptr && f->character_known) who += "; " + DescribeTraits(o.def.traits);
        if (f != nullptr) {
            for (const std::string& q : f->revealed) {
                for (const QuirkDef& qd : o.quirks) {
                    if (qd.id == q) who += "; " + qd.intel_hint;
                }
            }
        }
        return who;
    }

    // The whole truth about the enemy's staff (the debrief's reveal).
    std::string EnemyMind() const {
        std::ostringstream s;
        s << "The enemy air staff (" << data_.faction.name << "), " << Clock() << "\n";
        for (std::size_t i = 0; i < staff_.officers.size(); ++i) {
            const Officer& o = staff_.officers[i];
            s << "  " << StaffRoleName(o.def.role) << (o.def.squadron.empty() ? "" : " " + o.def.squadron) << ": " << o.Title()
              << (o.active ? "" : " [" + o.fate + "]") << " - " << DescribeTraits(o.def.traits) << ". Quirks:";
            for (const QuirkDef& q : o.quirks) s << " " << q.name << (q.weakness ? " (weakness)" : "") << ";";
            // How he moves his flights in the air (10_FACTIONS.md §10): only for those who lead flights.
            if (o.def.role == StaffRole::SquadronLeader || o.def.flies) {
                const std::string hot = HotShiftsLine(data_.faction, o);
                if (!hot.empty()) s << " In the air: " << hot << ";";
            }
            s << " stress " << static_cast<int>(o.stress * 100) << "%, reputation " << static_cast<int>(o.reputation * 100) << "%";
            if (!o.grudges.empty()) {
                s << ", grudges:";
                for (const auto& g : o.grudges) s << " " << g.first << "(" << static_cast<int>(g.second + 0.5) << ")";
            }
            s << "\n";
            for (const std::string& h : o.history) s << "      " << h << "\n";
        }
        return s.str();
    }

    std::string StaffLine() const {
        std::string s = "The staff at the start: ";
        bool first = true;
        for (int i : staff_.Active()) {
            const Officer& o = staff_.officers[static_cast<std::size_t>(i)];
            if (o.def.role == StaffRole::SquadronLeader) continue;
            s += (first ? "" : "; ") + std::string(StaffRoleName(o.def.role)) + " " + o.Title() + " (" + DescribeTraits(o.def.traits) + ")";
            first = false;
        }
        return s + ".";
    }

private:
    AirWarData data_;
    Rng rng_{1};
    // The map's features and ground for the route planner and the resolver (route.mode 1, route_planner.hpp), built on
    // first use; caches only, so a copy may share it.
    std::shared_ptr<RouteGround> ground_;
    const RouteGround* RouteGroundPtr() {
        if (data_.tuning.Get("route.mode") < 1.0) return nullptr;
        if (!ground_) {
            ground_ = std::make_shared<RouteGround>();
            ground_->Build(data_);
        }
        return ground_.get();
    }
    bool ok_ = false;
    double now_h_ = 0.0;
    bool marching_ = false;  // the player's battery on the march to a new position (G9.5)
    Vec3 march_to_;
    double march_until_h_ = 0.0;
    int day_ = 1;
    SupportLevel support_;         // the enemy's support orbits, as of the last plan (docs/air_war/11_SUPPORT.md)
    SupplyNetwork supply_;         // the player's side's supply lines (docs/air_war/14_LOGISTICS.md)
    double tanker_left_ = 0.0;     // aircraft the tankers can still refuel this cycle
    int support_sorties_ = 0;      // support sorties flown so far
    int reinforcements_ = 0;       // aircraft arrived in the build-up after day 1
    int phase_ = 0;
    double next_plan_h_ = 0.0;
    std::vector<ObjectState> truth_;
    int player_ = -1;
    std::vector<int> dummies_;
    std::vector<std::pair<double, Vec3>> battery_track_;  // (campaign hour, where the battery went), from the start
    std::vector<SquadronState> squadrons_;
    double arm_stock_ = 0.0;
    std::map<std::string, double> missile_stock_;  // cruise missile type -> left
    PoliticsState politics_;
    AirStaff staff_;
    IntelPicture intel_;
    LineOfContact front_line_;             // the faction's front (track F, F2.1)
    bool ground_on_ = false;               // ground.mode 1 on a theatre with a country map (F3.3)
    groundwar::GroundWar ground_war_;      // its formations at U0, stepped with the hour, moving front_line_'s stations
    // The air war following the ground units (F3.7), only with ground_on_.
    std::vector<double> tie_fn_;           // by tie: the working share last set from its unit (a drop since: a strike)
    std::vector<double> tie_before_;       // by tie: the working share before the supply lines' step (a rise: a march column)
    std::vector<Vec3> tie_place_;          // by tie: its unit's place last applied (its own moves round it are kept)
    unsigned tie_line_version_ = 0;
    std::uint64_t tie_places_version_ = 0;
    std::uint64_t tie_strength_version_ = 0;
    double tie_sync_h_ = 0.0;
    unsigned overrun_line_version_ = 0;
    std::vector<char> home_ours_;          // by object: its home on our side of the base line (what the enemy can overrun)
    std::vector<FrontPlace> place_;        // by object: its projection on the base line at place_at_ (BehindLine)
    std::vector<Vec3> place_at_;
    std::vector<char> placed_;
    std::vector<double> fscl_m_;           // NATO's FSCL by sector, beyond the base line (empty: none)
    std::uint64_t fscl_version_ = 0;
    std::vector<double> supply_sum_;       // by formation: scratch for its stocks
    std::vector<double> supply_weight_;
    GroundAirStats ground_air_;
    bool air_front_on_ = false;            // air_front.mode 1: the measures below kept (F2.2)
    SectorCoverBook cover_book_;
    std::vector<SectorCover> sector_cover_;  // as of the last plan
    SectorLossWindow sector_losses_;
    std::vector<double> air_front_depth_;  // each sector's fixed-wing air front beyond its line of contact (F2.3 moves it)
    std::vector<double> air_front_helo_depth_;  // and its helicopters' (F2.4 reads both)
    AirFrontState air_front_;              // each sector's air front and what its chief waits on (F2.3)
    std::vector<AirFrontMove> air_front_moves_;  // the last decision's (kept so a cycle allocates nothing once sized)
    std::vector<double> air_front_reward_;
    std::vector<double> air_front_scratch_;
    bool air_front_support_ = false;       // air_front.mode 2 with the ground war: a step's worth counts its own axes' need (F7.2)
    std::vector<int> air_front_advancing_;  // its own axes advancing by sector, its picture's, as of the last decision (F7.2)
    command::EnemyCorridorIntel corridor_intel_;  // what its COMINT heard of our fighters' corridors (roadmap S1)
    std::map<std::pair<int, std::string>, bool> session_heard_;  // (day, false corridor) -> heard in a played session (S6 follow-ups)
    int corridor_days_fed_ = 0;
    std::vector<Package> packages_;
    std::vector<JournalEntry> journal_;
    std::vector<Weather> weather_;
    BatteryPosture posture_;
    int next_package_id_ = 1;
    int pending_ = -1;
    std::set<int> player_candidates_;
    int player_missions_ = 0;
    int player_missions_today_ = 0;
    double last_player_mission_h_ = -1e9;
    std::string crew_tier_ = "class2";
    int kills_by_player_ = 0;
    int press_day_ = 0;
    int crew_kills_ = 0;
    std::map<int, OfficerFile> files_;
    RaidPatterns patterns_;
    unsigned seed_ = 1;
    std::vector<CampaignAction> actions_;
    bool traffic_fed_ = false;
    OutcomeObserver observer_;
    std::set<std::string> noted_;
    std::vector<Transmission> on_air_;  // noted, not yet heard
    int comint_heard_ = 0;
    int sympathiser_reports_ = 0;
    // In the air (roadmap B5): the packages flying now, each walked in time; the raids a session took (package id ->
    // the hour it took them); the command posts; the root every in-the-air and C2 draw is forked from.
    struct Airborne {
        int package = -1;  // index into packages_
        Weather weather = Weather::Clear;
        PackageRun run;
    };
    std::vector<Airborne> airborne_;
    std::map<int, double> live_;
    std::set<int> on_call_;  // packages the posts launched from their alert pools (roadmap B5)
    mutable AirC2 c2_;
    Rng c2_root_{1};
    int last_fac_hour_ = -1000000;

    CampaignAction& Record(CampaignAction::Kind kind) {
        actions_.emplace_back();
        actions_.back().kind = kind;
        actions_.back().posture = posture_;
        return actions_.back();
    }
    std::vector<ObservedRaid> observed_;

    void Log(const std::string& kind, const std::string& who, const std::string& text, bool visible) {
        journal_.push_back({now_h_, kind, who, text, visible});
    }
    std::string ObjName(const std::string& id) const {
        const TheatreObject* o = data_.theatre.Find(id);
        return o != nullptr ? o->name : id;
    }
    Weather WeatherAt(double h) const {
        const int d = static_cast<int>(std::floor((data_.theatre.start_hour + h) / 24.0)) + 1;
        return WeatherOn(d);
    }

    ResolveContext Context() {
        ResolveContext rc;
        rc.data = &data_;
        rc.truth = &truth_;
        rc.posture = &posture_;
        rc.staff = &staff_;
        rc.weather = WeatherAt(now_h_);
        rc.rng = &rng_;
        rc.awacs = support_.awacs;
        rc.ground = RouteGroundPtr();
        rc.opportunity_seed = static_cast<std::uint64_t>(seed_) * 0x9E3779B97F4A7C15ull + 0xB6A0ull;  // roadmap B6's keyed draws
        rc.phase = phase_;
        rc.own_corridors = OwnCorridors();
        rc.losses_today = politics_.losses_today;
        rc.front_line = &front_line_;
        rc.fighters = FighterView();  // our fighters by sector as they are (F4.3; off: the front's number)
        // On the march, or fighting a raid in a session (whose rounds the session counts): not in the abstract fight.
        if (marching_ || !live_.empty()) rc.skip_defence = data_.theatre.objects[static_cast<std::size_t>(player_)].id;
        return rc;
    }

    // A squadron's aircraft in theatre on `day` (the force build-up, docs/air_war/12_FORCES.md): its share on day 1,
    // growing to the whole by its day.
    int Arrived(const SquadronDef& s, int day) const {
        const LogisticsDef& l = data_.faction.logistics;
        const double initial = s.initial >= 0.0 ? s.initial : l.buildup_initial;
        const int full = s.full_by_day >= 0 ? s.full_by_day : l.buildup_full_by_day;
        double share = 1.0;
        if (full > 1) share = initial + (1.0 - initial) * Clamp01((day - 1.0) / (full - 1.0));
        return std::max(1, static_cast<int>(std::lround(s.count * Clamp01(share))));
    }

    // What the support squadrons keep up today (docs/air_war/11_SUPPORT.md): each serviceable support aircraft flies
    // a long sortie a day; an AWACS or ELINT orbit round the clock takes a few, and a tanker sortie refuels about four
    // aircraft (gwaps_vol3 table 16).
    SupportLevel SupportNow() const {
        const FactionDef& f = data_.faction;
        const TuningTable& tune = data_.tuning;
        SupportLevel s;
        double awacs = 0.0, elint = 0.0, tanker = 0.0;
        for (const SquadronState& st : squadrons_) {
            const AircraftType* a = f.Aircraft(st.aircraft);
            if (a == nullptr || !a->InService(data_.theatre.year)) continue;
            const double sorties = st.Serviceable(now_h_, f.doctrine.serviceability) * tune.Get("support.sorties_per_aircraft_day");
            if (a->Has(FlightRole::Awacs)) awacs += sorties;
            if (a->Has(FlightRole::Elint)) {
                elint += sorties;
                s.has_elint_aircraft = true;
            }
            if (a->Has(FlightRole::Tanker)) {
                tanker += sorties;
                s.has_tankers = true;
            }
        }
        s.awacs = Clamp01(awacs / tune.Get("support.awacs_sorties_per_orbit_day"));
        s.elint = Clamp01(elint / tune.Get("support.elint_sorties_per_orbit_day"));
        s.tanker_receivers = tanker * tune.Get("support.receivers_per_tanker_sortie");
        s.sorties = static_cast<int>(std::lround(awacs + elint + tanker));
        return s;
    }

    // One step of up to an hour, from now_h_ to t1. True if a player mission came due.
    bool StepTo(double t1) {
        const double t0 = now_h_;
        // A new day: politics, logistics, careers, our intelligence summary.
        const int day_at_t1 = static_cast<int>(std::floor((data_.theatre.start_hour + t1 - 1e-9) / 24.0)) + 1;
        while (day_ < day_at_t1) {
            ++day_;
            Daily();
        }
        if (t0 + 1e-9 >= next_plan_h_) {
            Plan();
            next_plan_h_ += data_.faction.doctrine.planning_cycle_h;
        }
        Collect(t0, t1);
        if (Legacy()) {
            // Before roadmap B5 (a save of version 1 replayed, c2.mode 0): each package resolved whole at its first hour.
            for (std::size_t i = 0; i < packages_.size(); ++i) {
                Package& p = packages_[i];
                if (p.resolved || p.played) continue;
                if (p.FirstH() >= t1) continue;
                if (player_candidates_.count(static_cast<int>(i)) > 0 && Pacing()) {
                    pending_ = static_cast<int>(i);
                    player_candidates_.erase(static_cast<int>(i));
                    ++player_missions_today_;
                    now_h_ = std::max(t0, p.FirstH());
                    last_player_mission_h_ = now_h_;
                    AimOnCall(p);
                    Log("player_mission", "warning", "Raid warning: aircraft approaching the battery's sector from " + p.entry + ".", true);
                    return true;
                }
                ResolveAbstract(p);
            }
        } else {
            // The FACs' eyes on our troops, once an hour.
            const int hour = static_cast<int>(std::floor(t0 + 1e-9));
            if (hour != last_fac_hour_) {
                last_fac_hour_ = hour;
                C2Context cx = C2Ctx();
                c2_.Hourly(t0, cx);
                c2_.OurFires(t0, cx);  // our fires on the posts our listeners fixed (roadmap S2)
            }
            // The packages: each takes off at its first aircraft's hour and flies in time; while any is in the air (or about to
            // be) or a message is on its way, the step is cut to c2.step_h (roadmap B5's A0).
            const double sub = std::max(1.0 / 120.0, data_.tuning.Get("c2.step_h"));
            double t = t0;
            while (t < t1 - 1e-12) {
                const double ts = AirBusy(t1) ? std::min(t1, t + sub) : t1;
                for (int round = 0; round < 4; ++round) {
                    bool began = false;
                    for (std::size_t i = 0; i < packages_.size(); ++i) {
                        Package& p = packages_[i];
                        if (p.resolved || p.played || Flying(static_cast<int>(i))) continue;
                        if (p.FirstH() >= ts) continue;
                        if (player_candidates_.count(static_cast<int>(i)) > 0 && (Pacing() || OnCallLive(p))) {
                            pending_ = static_cast<int>(i);
                            player_candidates_.erase(static_cast<int>(i));
                            ++player_missions_today_;
                            now_h_ = std::max(t, p.FirstH());
                            last_player_mission_h_ = now_h_;
                            C2Context cx = C2Ctx();
                            c2_.Brief(p, now_h_, cx);  // close support aimed by the FAC's own sighting (never the truth)
                            Log("player_mission", "warning", "Raid warning: aircraft approaching the battery's sector from " + p.entry + ".", true);
                            return true;
                        }
                        now_h_ = std::max(t, std::min(ts, p.FirstH()));
                        TakeOff(static_cast<int>(i));
                        began = true;
                    }
                    now_h_ = ts;
                    StepAir(ts);
                    if (!began && !Launched()) break;
                }
                t = ts;
            }
            now_h_ = t0;  // the hour's own processes as before the air was stepped
        }
        const double dt = t1 - t0;
        if (ground_on_) TroopsStruck();  // the hour's strikes on our tied troops off their units (F3.7)
        intel_.Decay(dt, data_.tuning);
        Repair(dt);
        Relocate();
        {
            // Our supply lines (logistics.hpp): what our side's people on them would report goes in the journal.
            std::vector<std::string> events;
            if (ground_on_) TroopsBefore();
            supply_.Step(data_, truth_, dt, TheatrePartAt(data_.theatre, data_.theatre.start_hour + t0) != DayPart::Day, &events);
            if (ground_on_) TroopsBrought();
            for (const std::string& ev : events) Log("logistics", "our side", ev, false);
        }
        Deliver();
        if (ground_on_) {
            TroopsSupply();  // the stocks as the units' supply, for the hour's combat (F3.7)
            StepGround(t1);  // after the hour's air results, before the next plan reads the line (F3.3)
            TroopsSync();    // the tied troops from their units; overrun, escapes and the FSCL on the line moved (F3.7)
        }
        now_h_ = t1;
        if (marching_ && now_h_ + 1e-9 >= march_until_h_) {  // the march's end: at the new position
            marching_ = false;
            MoveBattery(march_to_);
        }
        if (moves_on_ || fighters_on_) StepUnitMoves();  // our units' moves and fighters on orders: stages, orders heard (F4.2, F4.3)
        return false;
    }

    // An on-call package the posts launched while a session is flying a raid: it enters that session too, whatever the
    // pacing (roadmap S2, c2.live_mode 1).
    bool OnCallLive(const Package& p) const {
        return !live_.empty() && on_call_.count(p.id) > 0 && data_.tuning.Get("c2.live_mode") >= 1.0;
    }

    bool Pacing() const {
        const TuningTable& t = data_.tuning;
        if (truth_[static_cast<std::size_t>(player_)].function <= 0.3) return false;  // the battery is being repaired
        if (marching_) return false;                                                    // on the march to a new position
        if (now_h_ - last_player_mission_h_ < t.Get("pacing.min_gap_h")) return false;
        return player_missions_today_ < static_cast<int>(t.Get("pacing.max_per_day") + 0.5);
    }

    void Deliver() {
        const Officer* io = staff_.HolderPtr(StaffRole::Intelligence);
        const double skill = Clamp01(data_.faction.intel.analyst_skill * 0.5 +
                                     0.5 * (io != nullptr ? io->Effective(io->def.skills.intelligence) : 0.4));
        intel_.Deliver(now_h_ + 1.0, skill);
        // The truth comes out: something the pilots claimed destroyed is seen working again. The intelligence officer
        // and the planner lose face; the commander trusts claims less from now on.
        for (const std::string& key : intel_.resurrected) {
            const IntelEntry* e = intel_.Find(key);
            const std::string what = e != nullptr ? e->label : key;
            for (StaffRole r : {StaffRole::Intelligence, StaffRole::Planner}) {
                if (Officer* o = staff_.HolderPtr(r)) o->reputation = Clamp01(o->reputation - 0.08);
            }
            if (Officer* cdr = staff_.HolderPtr(StaffRole::Commander)) {
                const double before = cdr->def.traits.skepticism;
                cdr->def.traits.skepticism = Clamp01(before + 0.1 * (1.0 - cdr->def.traits.rigidity));
                Log("decision", cdr->Title(), "The " + what + " claimed destroyed is working again. From now on the commander wants proof before believing a claim.", false);
            }
        }
        intel_.resurrected.clear();
        for (const std::string& key : intel_.repaired) {
            const IntelEntry* e = intel_.Find(key);
            if (e == nullptr) continue;
            if (Officer* pln = staff_.HolderPtr(StaffRole::Planner)) {
                Log("decision", pln->Title(), "The " + e->label + " is working again, repaired faster than expected: it goes back on the list.", false);
            }
        }
        intel_.repaired.clear();
    }

    // The player's crew gets better as it fights (pillar 5: skill from the player's progression): every three aircraft
    // it brings down, one tier up (data/crew/skill_levels.json).
    void CrewProgress(int kills) {
        static const char* tiers[] = {"recruit", "class3", "class2", "class1", "master"};
        if (kills <= 0) return;
        crew_kills_ += kills;
        while (crew_kills_ >= 3) {
            crew_kills_ -= 3;
            for (int i = 0; i < 4; ++i) {
                if (crew_tier_ == tiers[i]) {
                    crew_tier_ = tiers[i + 1];
                    Log("own_intel", "battery", std::string("The crew's experience shows: now rated ") + tiers[i + 1] + ".", true);
                    break;
                }
            }
        }
    }

    // ------------------------------------------------------------------ planning

    void Plan() {
        const FactionDef& f = data_.faction;
        // Phase changes. Back to a phase that ended on the air defences when they are believed to have recovered.
        if (phase_ > 0) {
            const PhaseDef& before = f.doctrine.phases[static_cast<std::size_t>(phase_ - 1)];
            if (before.until_iads >= 0.0 && intel_.BelievedIadsShare() > before.until_iads + data_.tuning.Get("adapt.reopen_margin")) {
                --phase_;
                const Officer* cdr = staff_.HolderPtr(StaffRole::Commander);
                Log("decision", cdr != nullptr ? cdr->Title() : "commander",
                    "The air defences are working again: back to '" + before.name + "'.", false);
            }
        }
        while (phase_ + 1 < static_cast<int>(f.doctrine.phases.size())) {
            const PhaseDef& ph = f.doctrine.phases[static_cast<std::size_t>(phase_)];
            const bool by_day = ph.until_day > 0 && day_ > ph.until_day;
            const bool by_iads = ph.until_iads >= 0.0 && intel_.BelievedIadsShare() < ph.until_iads;
            if (!by_day && !by_iads) break;
            ++phase_;
            const Officer* cdr = staff_.HolderPtr(StaffRole::Commander);
            Log("decision", cdr != nullptr ? cdr->Title() : "commander",
                "The campaign moves on to '" + f.doctrine.phases[static_cast<std::size_t>(phase_)].name + "' (" +
                    (by_iads ? "the air defences are believed broken" : "the plan's day has come") + ").",
                false);
        }
        FeedCorridors();
        if (air_front_on_) {
            MeasureAirFront();
            DecideAirFrontNow();
        }
        PlanContext pc;
        pc.data = &data_;
        pc.staff = &staff_;
        pc.intel = &intel_;
        pc.squadrons = &squadrons_;
        pc.politics = &politics_;
        pc.weather = weather_;
        pc.now_h = now_h_;
        pc.phase = phase_;
        pc.arm_stock = &arm_stock_;
        pc.missile_stock = &missile_stock_;
        pc.rng = &rng_;
        pc.next_id = &next_package_id_;
        pc.journal = &journal_;
        pc.truth = &truth_;
        // The support orbits for this cycle (docs/air_war/11_SUPPORT.md).
        support_ = SupportNow();
        pc.awacs = support_.awacs;
        tanker_left_ = support_.tanker_receivers * data_.faction.doctrine.planning_cycle_h / 24.0;
        pc.tanker_receivers = &tanker_left_;
        pc.supply = &supply_;
        pc.ground = RouteGroundPtr();
        pc.front_line = &front_line_;
        pc.air_front = FrontView();
        pc.fighters = BelievedFighterView();  // our fighters by sector as its staff believes them (F4.3; off: the front's number)
        // The sorties the plan holds back for on-call work (doctrine.on_call_fraction), squadron by squadron, before the
        // planner spends the rest (roadmap B5: the alert pools fly them).
        std::vector<double> held;
        {
            const PhaseDef& ph = data_.faction.doctrine.phases[static_cast<std::size_t>(std::max(0, std::min(phase_, static_cast<int>(data_.faction.doctrine.phases.size()) - 1)))];
            const DoctrineDef& dd = data_.faction.doctrine;
            for (const SquadronState& s : squadrons_) {
                held.push_back(s.Serviceable(now_h_, dd.serviceability) * dd.sortie_rate_per_day * ph.tempo * std::min(1.0, dd.planning_cycle_h / 24.0) *
                               dd.on_call_fraction);
            }
        }
        Planner planner;
        std::vector<Package> plan = planner.Plan(pc);
        int planned = 0;
        for (Package& p : plan) {
            ExploitCorridor(p);
            planned += p.Aircraft();
            if (ground_on_) CountLabel(p);
            packages_.push_back(p);
            const int idx = static_cast<int>(packages_.size()) - 1;
            if (TouchesPlayer(p)) player_candidates_.insert(idx);
        }
        c2_.Bind(data_, staff_, FrontView());
        const double begin = now_h_ + data_.faction.doctrine.lead_time_h;
        c2_.BuildPools(squadrons_, held, planned, begin, begin + data_.faction.doctrine.planning_cycle_h, phase_);
    }

    // ------------------------------------------------------------------ our fighters' corridors (roadmap S1)

    bool OwnAirOn() const { return data_.has_own_air && data_.tuning.Get("own_air.mode") >= 1.0; }
public:
    // Our fighters' corridors as they are (docs/air_battle/08_OUR_FIGHTERS.md section 9): one from each of our airfields to the
    // nearest point of the front, at the task file's heights (2,000-6,000 m) and by day from 06:00 for hours_per_day, theatre
    // ENU and hours of the day. None without our aviation's rules or before S1.
    std::vector<command::OwnAirZone> OwnCorridors() const {
        std::vector<command::OwnAirZone> out;
        const FrontDef* front = data_.theatre.FrontFor(data_.faction.front);
        if (!OwnAirOn() || front == nullptr || front->line.empty()) return out;
        for (std::size_t i = 0; i < data_.theatre.objects.size(); ++i) {
            const TheatreObject& o = data_.theatre.objects[i];
            if (o.kind != ObjectKind::Airfield) continue;
            command::OwnAirZone z;
            z.id = "corridor " + o.id;
            const Vec3 a = truth_.size() > i ? truth_[i].pos : o.pos;
            Vec3 best = front->line.front();
            for (std::size_t k = 0; k + 1 < front->line.size(); ++k) {
                const Vec3 p0 = front->line[k], p1 = front->line[k + 1];
                const Vec3 d = p1 - p0;
                const double l2 = d.x * d.x + d.y * d.y;
                double u = l2 > 1e-9 ? ((a.x - p0.x) * d.x + (a.y - p0.y) * d.y) / l2 : 0.0;
                u = std::max(0.0, std::min(1.0, u));
                const Vec3 q = p0 + d * u;
                if (Dist2d(q, a) < Dist2d(best, a)) best = q;
            }
            if (front->line.size() == 1) best = front->line.front();
            z.path = {Vec3{a.x, a.y, 0.0}, Vec3{best.x, best.y, 0.0}};
            z.half_width_m = data_.own_air.notice.corridor_half_width_m;
            z.floor_m = 2000.0;
            z.ceiling_m = 6000.0;
            z.from_h = 6.0;
            z.to_h = 6.0 + data_.own_air.learning.hours_per_day;
            out.push_back(z);
        }
        return out;
    }
    // What the enemy has heard of them.
    const command::EnemyCorridorIntel& CorridorIntel() const { return corridor_intel_; }
    // Our dummy traffic's false corridors on campaign day `day` (0: the first; FeedDummyCorridors' rule), none before
    // radio.mode 1 or without our aviation. A use's hours are each corridor's from_h and to_h on that day (clock hours).
    std::vector<command::OwnAirZone> FalseCorridorsOn(int day) const {
        if (!OwnAirOn() || data_.tuning.Get("radio.mode") < 1.0) return {};
        return FalseCorridorsOf(OwnCorridors(), day);
    }
    // A played session's word on a false corridor's use that day (roadmap S6's follow-ups, radio.mode 2): whether the enemy's
    // listeners in its EM world heard the dummy transmitter's traffic. The daily feed then counts it as heard or not, with no
    // chance drawn. Not saved: a session that played in a saved campaign hands back no such word (a save replays as played).
    void SessionHeardCorridor(int day, const std::string& corridor_id, bool heard) {
        bool& h = session_heard_[{day, corridor_id}];
        h = h || heard;
    }

private:
    // Each day gone by, each corridor from a working airfield used once (by day): the enemy's COMINT on our GCI's and the
    // notices' traffic may hear it (EnemyCorridorIntel's keyed draws, kOwnAirKey + 0x500).
    void FeedCorridors() {
        if (!OwnAirOn()) return;
        const int day = static_cast<int>(std::floor(now_h_ / 24.0));
        const std::vector<command::OwnAirZone> zs = OwnCorridors();
        for (; corridor_days_fed_ < day; ++corridor_days_fed_) {
            for (const command::OwnAirZone& z : zs) {
                const int i = data_.theatre.IndexOf(z.id.substr(9));
                if (i < 0 || truth_[static_cast<std::size_t>(i)].function <= 0.05) continue;
                const double h = corridor_days_fed_ * 24.0 + z.from_h - data_.theatre.start_hour;
                if (corridor_intel_.Use(z, h)) {
                    Log("intel", "COMINT", "our fighters' traffic in " + z.id + " heard (" + std::to_string(corridor_days_fed_ + 1) + ". day)", false);
                }
            }
            FeedDummyCorridors(zs);
        }
    }
    // Our dummy traffic (roadmap S6, docs/air_battle/09 section 8; radio.mode 1): each day radio.our_dummy_corridors false
    // corridors, a working airfield's real one turned radio.our_dummy_turn_deg about the airfield (the side alternating by
    // day), are talked on by our GCI and notices at the real one's hours, so the enemy's COMINT can learn a corridor where
    // our fighters never fly (vr_1985_plss: dummy transmitters and imitated traffic; nazarenko_1984: radio disinformation and
    // imitation). A raid timed into one gets no held fire. The enemy's journal names it as it heard it.
    // From radio.mode 2 a played session's EM world may have decided whether the enemy's listeners heard a false corridor's
    // traffic that day (SessionHeardCorridor): that use is counted as heard or not without the chance.
    void FeedDummyCorridors(const std::vector<command::OwnAirZone>& zs) {
        if (data_.tuning.Get("radio.mode") < 1.0 || zs.empty()) return;
        const bool sessions = data_.tuning.Get("radio.mode") >= 2.0;
        for (const command::OwnAirZone& z : FalseCorridorsOf(zs, corridor_days_fed_)) {
            const double h = corridor_days_fed_ * 24.0 + z.from_h - data_.theatre.start_hour;
            const auto told = session_heard_.find({corridor_days_fed_, z.id});
            const bool heard = sessions && told != session_heard_.end() ? corridor_intel_.UseHeard(z, h, told->second) : corridor_intel_.Use(z, h);
            if (heard) {
                Log("intel", "COMINT", "our fighters' traffic in " + z.id + " heard (" + std::to_string(corridor_days_fed_ + 1) + ". day)", false);
            }
        }
    }
    // The day's false corridors (command::FalseCorridors' rule) from the real ones, a real one's airfield working.
    std::vector<command::OwnAirZone> FalseCorridorsOf(const std::vector<command::OwnAirZone>& zs, int day) const {
        const int n = static_cast<int>(std::floor(data_.tuning.Get("radio.our_dummy_corridors") + 1e-9));
        const double turn = data_.tuning.Get("radio.our_dummy_turn_deg") * 3.14159265358979323846 / 180.0;
        return command::FalseCorridors(zs, day, n, turn, [this](const command::OwnAirZone& real) {
            const int i = data_.theatre.IndexOf(real.id.substr(9));
            return i >= 0 && truth_[static_cast<std::size_t>(i)].function > 0.05;
        });
    }
    // A creative planner with a fresh belief of our corridor near the package's target times it into the corridor's hours,
    // when that is at most shift_h later (mt_umnov_1968 pp. 10-11, the Syrian landing case; Mitronin's warning, mitronin_1976
    // p. 7). Whether the belief found the real corridor decides whether our defences then hold fire (resolve.hpp).
    void ExploitCorridor(Package& p) {
        if (!OwnAirOn() || p.planner < 0 || static_cast<std::size_t>(p.planner) >= staff_.officers.size()) return;
        const command::EnemyLearningDef& L = data_.own_air.learning;
        const double creativity = staff_.officers[static_cast<std::size_t>(p.planner)].def.traits.creativity;
        if (!corridor_intel_.Exploits(creativity, now_h_)) return;
        const command::BelievedCorridor* use = nullptr;
        std::vector<command::BelievedCorridor> believed = corridor_intel_.Believed(now_h_);
        command::OwnAirZone bz;
        for (const command::BelievedCorridor& b : believed) {
            command::OwnAirZone z;
            z.path = b.path;
            if (z.DistanceM(p.target_pos) > 2.0 * L.near_m + L.margin_m) continue;
            use = &b;
            bz = z;
            break;
        }
        if (use == nullptr) return;
        double hod = std::fmod(data_.theatre.start_hour + p.tot_h, 24.0);
        if (hod < 0.0) hod += 24.0;
        double delta = 0.0;
        if (hod < use->from_h || hod > use->to_h) {
            double ahead = use->from_h + 0.25 - hod;
            if (ahead < 0.0) ahead += 24.0;
            if (ahead > L.shift_h) return;
            delta = ahead;
        }
        p.tot_h += delta;
        if (p.gate_h >= 0.0) p.gate_h += delta;
        for (Flight& fl : p.flights) {
            for (RoutePoint& r : fl.route) r.t_h += delta;
        }
        p.own_corridor = use->id;
        for (const command::OwnAirZone& z : OwnCorridors()) {
            if (z.id != use->id || use->path.empty()) continue;
            p.own_corridor_found = z.DistanceM(use->path.front()) <= z.half_width_m + L.margin_m;
        }
        const std::string why = "timed into our fighters' " + use->id + " (believed, heard " + std::to_string(use->heard) + " times): their defences hold fire then" +
                                (delta > 0.0 ? ", " + std::to_string(static_cast<int>(std::lround(delta * 60.0))) + " min later than planned" : std::string());
        p.rationale.push_back(why);
        Log("decision", staff_.officers[static_cast<std::size_t>(p.planner)].Title(), p.name + ": " + why, false);
    }

    // Does the raid come through the player's battery's sector (or at it)?
    bool TouchesPlayer(const Package& p) const {
        const Vec3 b = truth_[static_cast<std::size_t>(player_)].pos;
        const double sector = data_.tuning.Get("pacing.sector_m");
        if (p.target_object == data_.theatre.objects[static_cast<std::size_t>(player_)].id) return true;
        for (int di : dummies_) {
            if (p.target_object == data_.theatre.objects[static_cast<std::size_t>(di)].id) return true;
        }
        // What the battery defends: a target inside its reach.
        if (!p.target_object.empty()) {
            const int ti = data_.theatre.IndexOf(p.target_object);
            if (ti >= 0 && Dist2d(truth_[static_cast<std::size_t>(ti)].pos, b) < sector) return true;
        }
        for (const Flight& fl : p.flights) {
            for (std::size_t i = 0; i + 1 < fl.route.size(); ++i) {
                if (SegmentDistance2d(b, fl.route[i].pos, fl.route[i + 1].pos) < sector) return true;
            }
        }
        return false;
    }

    // ------------------------------------------------------------------ in the air (roadmap B5)

    // Close support and helicopter raids against troops are talked onto the target by the own troops' forward air
    // controllers (mai_tactics_vvs: control passes to controllers with the ground troops): at check-in the FAC briefs
    // them where he last saw our troops, with that sighting's age (c2.hpp AirC2::Brief), never where they are now.
    // Then the package flies, its draws from a fork keyed by its id.
    // Before roadmap B5 (c2.mode 0, a save of version 1 replayed to the bit): the package resolved whole at its first hour
    // from the campaign's stream, close support aimed where our troops really were (the FAC's sighting came with B5).
    bool Legacy() const { return data_.tuning.Get("c2.mode") < 1.0; }
    // Legacy only (roadmap S4 made it explicit): with c2.mode 1 the FAC's brief (c2.hpp AirC2::Brief) aims close support
    // at his believed sighting; this truth-read aim is kept so a save from before B5 replays to the bit.
    void AimOnCall(Package& p) {
        if (!Legacy()) return;
        if (p.kind != MissionKind::Cas && p.kind != MissionKind::HeloRaid) return;
        const int i = p.target_object.empty() ? -1 : data_.theatre.IndexOf(p.target_object);
        if (i < 0 || data_.theatre.objects[static_cast<std::size_t>(i)].kind != ObjectKind::Troops) return;
        const Vec3 at = truth_[static_cast<std::size_t>(i)].pos + Vec3{rng_.Normal(0.0, 300.0), rng_.Normal(0.0, 300.0), 0.0};
        for (Flight& fl : p.flights) {
            if (fl.aim_key == p.target_key) fl.aim_pos = at;
        }
    }
    void ResolveAbstract(Package& p) {
        AimOnCall(p);
        ResolveContext rc = Context();
        rc.weather = WeatherAt(p.tot_h);
        PackageOutcome o = Resolver().Resolve(p, rc, true);
        ApplyOutcome(p, o);
    }

    void TakeOff(int i) {
        Package& p = packages_[static_cast<std::size_t>(i)];
        C2Context cx = C2Ctx();
        c2_.Brief(p, now_h_, cx);
        Airborne a;
        a.package = i;
        a.weather = WeatherAt(p.tot_h);
        ResolveContext rc = Context();
        rc.weather = a.weather;
        Resolver().Begin(p, rc, a.run, c2_root_.Fork(0x9AC4A6E000ull + static_cast<std::uint64_t>(p.id)));
        airborne_.push_back(std::move(a));
    }
    bool Flying(int i) const {
        for (const Airborne& a : airborne_) {
            if (a.package == i) return true;
        }
        return false;
    }
    // Anything in the air, or taking off, before t1, or a message on its way: the step is cut.
    bool AirBusy(double t1) const {
        if (!airborne_.empty() || c2_.Busy()) return true;
        for (std::size_t i = 0; i < packages_.size(); ++i) {
            const Package& p = packages_[i];
            if (!p.resolved && !p.played && p.FirstH() < t1) return true;
        }
        return false;
    }
    // A package the posts launched this step that should already be in the air.
    bool Launched() const {
        for (std::size_t i = 0; i < packages_.size(); ++i) {
            const Package& p = packages_[i];
            if (!p.resolved && !p.played && !Flying(static_cast<int>(i)) && p.FirstH() < now_h_) return true;
        }
        return false;
    }
    // Every package in the air to `ts`: its legs, its flights' reports to their posts, the posts' messages and decisions;
    // a package home is applied.
    void StepAir(double ts) {
        for (Airborne& a : airborne_) {
            ResolveContext rc = Context();
            rc.weather = a.weather;
            Resolver().StepTo(packages_[static_cast<std::size_t>(a.package)], rc, a.run, ts);
        }
        C2Context cx = C2Ctx();
        for (Airborne& a : airborne_) c2_.OnEvents(packages_[static_cast<std::size_t>(a.package)], a.run, ts, cx);
        c2_.StepTo(ts, cx);
        c2_.Recalls(ts, cx);  // a package whose target is believed destroyed since it took off (roadmap S2)
        for (std::size_t k = 0; k < airborne_.size();) {
            Package& p = packages_[static_cast<std::size_t>(airborne_[k].package)];
            if (!airborne_[k].run.Done(p)) {
                ++k;
                continue;
            }
            PackageOutcome o = std::move(airborne_[k].run.out);
            airborne_.erase(airborne_.begin() + static_cast<std::ptrdiff_t>(k));
            p.resolved = true;
            TakeC2(p, o);
            ApplyOutcome(p, o);
        }
    }
    // What the posts did about a package, into its outcome.
    void TakeC2(const Package& p, PackageOutcome& o) {
        for (const RetaskRecord& r : c2_.TakeRetasks(p.id)) o.retasks.push_back(r);
        for (const C2Message& m : c2_.TakeCalls(p.id)) {
            PackageOutcome::CallRecord cr;
            cr.kind = C2KindName(m.kind);
            cr.from = m.from;
            cr.to = m.to;
            cr.net = m.net;
            cr.t_h = m.sent_h;
            cr.lost = m.lost;
            cr.heard = m.heard;
            o.calls.push_back(cr);
        }
        for (const C2Message& m : c2_.TakeReports(p.id)) {
            PackageOutcome::ReportRecord rr;
            rr.flight = m.flight;
            rr.what = m.what;
            rr.east_m = m.where.x;
            rr.north_m = m.where.y;
            rr.sigma_m = m.sigma_m;
            rr.observed_h = m.observed_h;
            o.reports.push_back(rr);
        }
    }
    // A played package's pilot's report (CampaignAction::Report), to the posts now.
    void ApplyReport(const CampaignAction& a) {
        const Package* p = PackageById(a.package_id);
        if (p == nullptr) return;
        const Profile prof = a.flight >= 0 && a.flight < static_cast<int>(p->flights.size()) ? p->flights[static_cast<std::size_t>(a.flight)].profile : p->profile;
        C2Context cx = C2Ctx();
        // A launch or a battery seen is a SAM site; a column, a column; parked vehicles, troops (roadmap B6).
        const ObjectKind kind = a.what == "column" ? ObjectKind::Column : a.what == "vehicles" ? ObjectKind::Troops : ObjectKind::SamSite;
        const std::string what = a.what == "battery" ? std::string("launch") : a.what;
        c2_.Report(*p, a.flight, what, std::string(), kind, true, a.to, a.sigma_m, a.observed_h, a.from, prof, now_h_, cx);
    }
    // The posts' view of the campaign: what they may read and do.
    C2Context C2Ctx() {
        C2Context c;
        c.data = &data_;
        c.truth = &truth_;
        c.staff = &staff_;
        c.packages = &packages_;
        for (Airborne& a : airborne_) c.airborne.push_back({a.package, &a.run});
        for (const auto& lv : live_) {  // the raids a session flies: the posts know their plan (roadmap S2)
            for (std::size_t i = 0; i < packages_.size(); ++i) {
                if (packages_[i].id == lv.first) c.live.push_back(static_cast<int>(i));
            }
        }
        c.phase = phase_;
        c.battery = truth_[static_cast<std::size_t>(player_)].pos;
        c.on_call = [this](MissionKind mk, const IntelEntry& e, const AlertPool& pool, double takeoff_h, Package& out) {
            PlanContext pc;
            pc.data = &data_;
            pc.staff = &staff_;
            pc.intel = &intel_;
            pc.squadrons = &squadrons_;
            pc.politics = &politics_;
            pc.weather = weather_;
            pc.now_h = now_h_;
            pc.phase = phase_;
            pc.arm_stock = &arm_stock_;
            pc.missile_stock = &missile_stock_;
            Rng r = c2_root_.Fork(0x0CA11000ull + static_cast<std::uint64_t>(next_package_id_));
            pc.rng = &r;
            pc.next_id = &next_package_id_;
            pc.journal = &journal_;
            pc.truth = &truth_;
            pc.awacs = support_.awacs;
            pc.tanker_receivers = &tanker_left_;
            pc.supply = &supply_;
            pc.ground = RouteGroundPtr();  // the route planner's ground (route.mode 1, roadmap B2)
            pc.front_line = &front_line_;
        pc.air_front = FrontView();
            pc.fighters = BelievedFighterView();
            Planner planner;
            return planner.OnCall(pc, mk, e, pool.squadron, pool.aircraft, takeoff_h, out);
        };
        c.launch = [this](Package& p) {
            if (ground_on_) CountLabel(p);
            packages_.push_back(p);
            const int idx = static_cast<int>(packages_.size()) - 1;
            if (TouchesPlayer(p)) player_candidates_.insert(idx);
            on_call_.insert(p.id);
        };
        c.intel = [this](const IntelReport& r) { intel_.Queue(r); };
        c.believed = [this](const std::string& key) -> const IntelEntry* { return key.empty() ? nullptr : intel_.Find(key); };
        c.log = [this](const std::string& kind, const std::string& who, const std::string& text, bool visible) { Log(kind, who, text, visible); };
        c2_.Bind(data_, staff_, FrontView());
        return c;
    }

    // ------------------------------------------------------------------ what happened, and what they believe happened

    // `observe`: our side's record of the raid (once per raid: not again for the rest of a played one).
    void ApplyOutcome(Package& p, const PackageOutcome& o, bool observe = true) {
        if (observer_) observer_(p, o);
        if (air_front_on_) NoteSectorLosses(p, o, observe);
        const FactionDef& f = data_.faction;
        const TuningTable& tune = data_.tuning;
        p.resolved = true;
        if (fighters_on_) LearnFighterCover(p);  // its pilots' debrief: what it met of our fighters there (F4.3)
        // The truth.
        if (!o.truth_applied) {
            for (const auto& d : o.damage) {
                const int i = data_.theatre.IndexOf(d.first);
                if (i < 0) continue;
                ObjectState& st = truth_[static_cast<std::size_t>(i)];
                st.function = std::max(0.0, st.function - d.second);
            }
            for (const std::string& id : o.decoys_hit) {
                const int i = data_.theatre.IndexOf(id);
                if (i >= 0) truth_[static_cast<std::size_t>(i)].function = 0.0;
            }
            if (o.missiles_spent > 0) {
                ObjectState& b = truth_[static_cast<std::size_t>(player_)];
                if (b.missiles > 0) b.missiles = std::max(0, b.missiles - o.missiles_spent);
            }
            for (const auto& m : o.missiles_by) {
                const int i = data_.theatre.IndexOf(m.first);
                if (i < 0) continue;
                ObjectState& st = truth_[static_cast<std::size_t>(i)];
                if (st.missiles > 0) st.missiles = std::max(0, st.missiles - m.second);
            }
        }
        for (ObjectState& st : truth_) {
            if (st.function <= 0.0 && st.wrecked_h < 0.0) st.wrecked_h = std::max(now_h_, p.tot_h);
        }
        // The squadrons.
        int lost = 0;
        for (const LossRecord& l : o.losses) {
            if (l.flight < 0 || l.flight >= static_cast<int>(p.flights.size())) continue;
            const Flight& fl = p.flights[static_cast<std::size_t>(l.flight)];
            const AircraftType* a = f.Aircraft(fl.aircraft);
            if (a != nullptr && a->drone) continue;
            for (SquadronState& s : squadrons_) {
                if (s.id == fl.squadron) ++s.lost;
            }
            ++lost;
        }
        // The combat sorties its pilots came back from (roadmap S5, 10_FACTIONS.md §8.3): each flight's aircraft less its
        // losses. Counted always; read only under hot_phase.mode 2 (HotPilotOf), so an older mode's replay is unmoved.
        for (std::size_t fi = 0; fi < p.flights.size(); ++fi) {
            const Flight& fl = p.flights[fi];
            const AircraftType* a = f.Aircraft(fl.aircraft);
            if (a != nullptr && (a->drone || a->missile)) continue;
            int down = 0;
            for (const LossRecord& l : o.losses) down += l.flight == static_cast<int>(fi) ? 1 : 0;
            for (SquadronState& s : squadrons_) {
                if (s.id == fl.squadron) s.combat_sorties += std::max(0, fl.count - down);
            }
        }
        // The damaged back in service after their repair (damage.mode 1, roadmap S4: by its level, a heavy one
        // damage.heavy_repair_factor times as long; a played session's on their own flights, the rest drawn as before and
        // heavy by a keyed draw of damage.heavy_share); damage.mode 0 as before S4.
        const bool by_level = data_.tuning.Get("damage.mode") >= 1.0;
        for (int k = 0; k < o.damaged; ++k) {
            if (p.flights.empty()) break;
            const bool listed = by_level && static_cast<std::size_t>(k) < o.damaged_list.size() && o.damaged_list[static_cast<std::size_t>(k)].flight >= 0 &&
                                o.damaged_list[static_cast<std::size_t>(k)].flight < static_cast<int>(p.flights.size());
            const std::size_t fi = listed ? static_cast<std::size_t>(o.damaged_list[static_cast<std::size_t>(k)].flight)
                                          : static_cast<std::size_t>(rng_.Index(static_cast<int>(p.flights.size())));
            const Flight& fl = p.flights[fi];
            double repair_h = f.logistics.damaged_return_h;
            if (by_level) {
                const bool heavy = listed ? o.damaged_list[static_cast<std::size_t>(k)].heavy
                                          : em::Uniform01(em::DrawHash(seed_, em::Stream::Opportunity, static_cast<std::uint64_t>(p.id), 0xDA5u,
                                                                       static_cast<std::uint64_t>(k))) < data_.tuning.Get("damage.heavy_share");
                if (heavy) repair_h *= data_.tuning.Get("damage.heavy_repair_factor");
            }
            for (SquadronState& s : squadrons_) {
                if (s.id == fl.squadron) s.damaged_until.push_back(now_h_ + repair_h);
            }
        }
        politics_.losses_today += lost;
        politics_.losses_total += lost;
        double true_damage = 0.0;
        for (const auto& d : o.damage) true_damage += d.second;
        politics_.damage_today += true_damage;
        if (o.collateral > 0.5) Collateral(p, o.collateral);
        // Officers shot down.
        for (const LossRecord& l : o.losses) {
            if (!l.officer || l.flight < 0 || l.flight >= static_cast<int>(p.flights.size())) continue;
            const int oi = p.flights[static_cast<std::size_t>(l.flight)].officer;
            if (oi < 0 || !staff_.officers[static_cast<std::size_t>(oi)].active) continue;
            const std::string title = staff_.officers[static_cast<std::size_t>(oi)].Title();
            const std::string fate = l.captured ? "captured" : "killed";
            Rng r = rng_.Fork(static_cast<std::uint64_t>(oi) * 31u + 7u);
            const int succ = staff_.Replace(oi, fate, "shot down leading '" + p.name + "'", data_, r, now_h_);
            Log("career", title, title + " was " + fate + " leading '" + p.name + "'.", false);
            if (l.captured) {
                OfficerFile& file = files_[oi];
                file.name_known = true;
                file.character_known = true;
                Log("own_intel", "interrogation", "We hold " + title + ", leader of " +
                                                     staff_.officers[static_cast<std::size_t>(oi)].def.squadron + ", shot down over our lines.", true);
            }
            if (succ >= 0) {
                Log("career", staff_.officers[static_cast<std::size_t>(succ)].Title(),
                    "Takes over from " + title + ": " + DescribeTraits(staff_.officers[static_cast<std::size_t>(succ)].def.traits) + ".", false);
            }
        }
        // Prisoners: our side interrogates them.
        int pows = 0;
        for (const LossRecord& l : o.losses) pows += l.captured ? 1 : 0;
        politics_.pows += pows;
        if (pows > 0) {
            politics_.pressure = Clamp01(politics_.pressure + pows * tune.Get("politics.pow_pressure") * f.politics.pow_sensitivity);
            Interrogate(p, pows);
        }
        // What they believe: pilots' reports, sightings, ELINT, photographs.
        BeliefsFrom(p, o);
        // What the officers make of it.
        Lessons(p, o, lost);
        // The journal (the enemy's mind) and what our side saw.
        std::string text = std::string(MissionKindName(p.kind)) + " '" + p.name + "' on " + p.target_label + ": " +
                           (p.Missiles() > 0 ? std::to_string(o.missiles_downed.size()) + " of " + std::to_string(p.Missiles()) + " missiles shot down"
                                             : "lost " + std::to_string(lost) + " of " + std::to_string(p.Aircraft())) +
                           (o.aborted ? ", turned back" : "") + (o.delivered ? ", weapons delivered" : "") + ".";
        for (const std::string& n : o.notes) text += " " + n + ".";
        Log("result", "result", text, false);
        if (observe) ObserveRaid(p, o, lost);
    }

    void Collateral(const Package& p, double amount) {
        const FactionDef& f = data_.faction;
        const double add = data_.tuning.Get("politics.collateral_restriction_m") * f.politics.collateral_sensitivity;
        politics_.city_radius_m += add;
        politics_.support = Clamp01(politics_.support - 0.05 * amount * f.politics.collateral_sensitivity);
        Log("politics", "politics", "Civilian casualties after '" + p.name + "': the no-strike zone around towns grows to " +
                                        std::to_string(static_cast<int>(politics_.city_radius_m / 1000.0 + 0.5)) + " km.", false);
        if (press_day_ != day_) {
            press_day_ = day_;
            Log("own_intel", "press", "Foreign press reports civilian casualties from an enemy raid near a town.", true);
        }
    }

    void BeliefsFrom(const Package& p, const PackageOutcome& o) {
        const FactionDef& f = data_.faction;
        const IntelDef& in = f.intel;
        const TuningTable& tune = data_.tuning;
        // Seen at the time over the target (the package resolved in the air ends after it: roadmap B5).
        const double t = p.played || Legacy() ? std::max(now_h_, p.tot_h) : p.tot_h;
        const Officer* io = staff_.HolderPtr(StaffRole::Intelligence);
        const Officer* cdr = staff_.HolderPtr(StaffRole::Commander);
        const double skeptic = 0.5 * ((io != nullptr ? io->def.traits.skepticism : 0.5) + (cdr != nullptr ? cdr->def.traits.skepticism : 0.5));
        int survivors = 0;
        for (const Flight& fl : p.flights) survivors += fl.count - fl.lost;
        double pirep_h = t + in.pirep_delay_h;
        // A faction whose crews report on landing (the Pact: 5-10 min to the regiment's staff, markovsky_afghan_sky): the
        // debrief report after its last aircraft is home (roadmap B5, c2.debrief_s).
        if (f.c2.present && !Legacy() && f.c2.debrief_s.Set() && !p.flights.empty()) {
            Rng r = c2_root_.Fork(0xDEB21EF000ull + static_cast<std::uint64_t>(p.id));
            pirep_h = p.LastH() + r.Uniform(f.c2.debrief_s.lo, f.c2.debrief_s.hi) / 3600.0;
        }
        // The target, as the pilots saw it.
        if (o.delivered && !p.target_key.empty()) {
            double true_d = 0.0;
            for (const auto& d : o.damage) {
                if (d.first == p.target_object) true_d += d.second;
            }
            double inflate = in.bda_overclaim * rng_.Uniform(0.5, 1.5) * (1.0 - 0.5 * skeptic);
            if (cdr != nullptr) inflate += 0.3 * cdr->Quirk("report_inflation");
            double claim = o.hit_nothing ? Clamp01(0.3 + inflate * 0.4) : Clamp01(true_d * (1.0 + inflate) + 0.1 * inflate);
            const IntelEntry* e = intel_.Find(p.target_key);
            const double before = e != nullptr ? e->function : 1.0;
            IntelReport r;
            r.arrive_h = pirep_h;
            r.observed_h = t;
            r.source = IntelSource::PilotReport;
            r.object_id = p.target_object;
            r.ghost = p.target_object.empty();
            r.kind = e != nullptr ? e->kind : ObjectKind::Depot;
            r.pos = p.target_pos;
            r.sigma_m = e != nullptr ? e->sigma_m : 1000.0;
            r.function = Clamp01(before - claim);
            r.claim_destroyed = r.function < 0.2;
            if (!p.target_object.empty()) {
                const int ti = data_.theatre.IndexOf(p.target_object);
                r.claim_false = ti >= 0 && truth_[static_cast<std::size_t>(ti)].function > 0.4;
            } else {
                r.claim_false = true;  // a ghost: nothing was ever there
            }
            r.label = p.target_label;
            intel_.Queue(r);
            if (IntelEntry* me = intel_.Find(p.target_key)) {
                ++me->strikes;
                me->last_struck_h = t;
            }
            for (const Flight& fl : p.flights) {
                if (fl.officer >= 0) staff_.officers[static_cast<std::size_t>(fl.officer)].damage_claimed += claim;
            }
            if (Officer* pln = staff_.HolderPtr(StaffRole::Planner)) pln->damage_claimed += claim;
        }
        // Dummies they destroyed: claimed as SAM sites destroyed.
        for (const std::string& id : o.decoys_hit) {
            IntelReport r;
            r.arrive_h = pirep_h;
            r.observed_h = t;
            r.source = IntelSource::PilotReport;
            r.object_id = id;
            r.kind = ObjectKind::SamSite;
            const int i = data_.theatre.IndexOf(id);
            r.pos = i >= 0 ? truth_[static_cast<std::size_t>(i)].pos : p.target_pos;
            r.sigma_m = 500.0;
            r.function = 0.0;
            r.claim_destroyed = true;
            r.label = "SAM site";
            intel_.Queue(r);
        }
        // Radars under ARM attack: killed, or switched off and claimed.
        for (const std::string& id : o.radars_killed) {
            const int i = data_.theatre.IndexOf(id);
            if (i < 0) continue;
            IntelReport r = ReportAbout(i, IntelSource::PilotReport, t, pirep_h, 1500.0);
            r.function = 0.0;
            r.claim_destroyed = true;
            intel_.Queue(r);
        }
        for (const std::string& id : o.radars_suppressed) {
            const int i = data_.theatre.IndexOf(id);
            if (i < 0 || !rng_.Chance(tune.Get("intel.claim_destroy_when_off"))) continue;
            IntelReport r = ReportAbout(i, IntelSource::PilotReport, t, pirep_h, 2000.0);
            r.function = 0.1;
            r.claim_destroyed = true;
            r.claim_false = truth_[static_cast<std::size_t>(i)].function > 0.4;
            r.note = "radar went silent under ARM attack";
            intel_.Queue(r);
        }
        // Targets of opportunity (roadmap B6): what its flights found and claimed, as their pilots claimed it (never the
        // kills): a claim reads as the thing there destroyed, unless a skeptical staff doubts a lone claim; a report
        // without a claim, that something is there. No draw: the main stream falls as without them.
        for (const PackageOutcome::OpportunityRecord& op : o.opportunities) {
            if (op.object.empty() || (op.claimed <= 0 && op.act != "report")) continue;
            const int i = data_.theatre.IndexOf(op.object);
            if (i < 0) continue;
            IntelReport r = ReportAbout(i, IntelSource::PilotReport, t, pirep_h, tune.Get("opportunity.report_sigma_m"));
            if (op.claimed > 0) {
                r.function = skeptic > 0.6 && op.claimed < 2 ? 0.5 : 0.0;
                r.claim_destroyed = r.function < 0.2;
                r.claim_false = truth_[static_cast<std::size_t>(i)].function > 0.4;
                r.note = "claimed by a flight that found it";
            } else {
                r.note = "reported by a flight that passed it";
            }
            intel_.Queue(r);
        }
        // Launches seen by the survivors: where the defences are, and who shot whom.
        std::set<std::string> fired(o.fired.begin(), o.fired.end());
        for (const std::string& id : fired) {
            const int i = data_.theatre.IndexOf(id);
            if (i < 0 || survivors <= 0) continue;
            IntelReport r = ReportAbout(i, IntelSource::LaunchSighting, t, pirep_h, 700.0);
            for (const LossRecord& l : o.losses) {
                if (l.by == id) r.losses += 1.0;
            }
            intel_.Queue(r);
        }
        // Losses to defences nobody saw fire: something is there.
        for (const LossRecord& l : o.losses) {
            if (l.by.empty() || fired.count(l.by) > 0) continue;
            const int i = data_.theatre.IndexOf(l.by);
            if (i < 0) continue;
            IntelReport r = ReportAbout(i, IntelSource::PilotReport, t, pirep_h, 4000.0);
            r.pos = l.where;
            r.losses = 1.0;
            r.exists = tune.Get("intel.suspect_after_loss");
            r.label = "unidentified air defence";
            intel_.Queue(r);
        }
        // Losses nobody can explain (roadmap S7, learning.mode 1): the defence fired but nobody came home to say so, or
        // nobody knows who shot. The staff suspects an air defence where the flight was last heard of: a ghost report
        // about the place, never about the true killer, its error drawn from a fork keyed by the package and the loss.
        if (tune.Get("learning.mode") >= 1.0) {
            for (std::size_t li = 0; li < o.losses.size(); ++li) {
                const LossRecord& l = o.losses[li];
                if (l.fighter || l.own_fire) continue;  // our fighters or its own guns: not a defence of ours on the ground
                const bool unseen = l.by.empty() || (fired.count(l.by) > 0 && survivors <= 0);
                if (!unseen) continue;
                Rng g = rng_.Fork(0x5E7A1EA2000ull + static_cast<std::uint64_t>(p.id) * 64ull + li);
                const double sigma = tune.Get("learning.unexplained_sigma_m");
                IntelReport r;
                r.arrive_h = pirep_h;
                r.observed_h = t;
                r.source = IntelSource::PilotReport;
                r.ghost = true;
                r.kind = ObjectKind::SamSite;
                r.pos = l.where + Vec3{g.Normal(0.0, sigma), g.Normal(0.0, sigma), 0.0};
                r.sigma_m = sigma;
                r.exists = tune.Get("learning.unexplained_exists");
                r.losses = 1.0;
                r.threat_radius_m = tune.Get("learning.unexplained_radius_m");
                r.threat_lethality = tune.Get("learning.unexplained_lethality");
                r.label = "suspected air defence (a loss nobody saw)";
                r.note = "inferred from a flight lost without a report";
                intel_.Queue(r);
            }
        }
        // What radiated while the raid was up: ELINT fixes.
        if (in.elint) {
            for (const auto& em : o.emissions) Elint(em.first, em.second, t);
        }
        // Photographs.
        for (const std::string& id : o.recce_seen) {
            const int i = data_.theatre.IndexOf(id);
            if (i < 0) continue;
            IntelReport r = ReportAbout(i, IntelSource::AirRecce, t, t + in.recce_latency_h, 300.0);
            r.function = truth_[static_cast<std::size_t>(i)].function;
            const TheatreObject& ob = data_.theatre.objects[static_cast<std::size_t>(i)];
            if (ob.decoy) {
                const double spot = tune.Get("intel.decoy_spot_skill") * (io != nullptr ? io->Effective(io->def.skills.intelligence) : 0.4);
                r.decoy_spotted = rng_.Chance(spot);
            }
            intel_.Queue(r);
        }
    }

    // A report about a real object, with the believed defence from its type.
    IntelReport ReportAbout(int i, IntelSource src, double observed_h, double arrive_h, double sigma_m) {
        const TheatreObject& o = data_.theatre.objects[static_cast<std::size_t>(i)];
        const ObjectState& st = truth_[static_cast<std::size_t>(i)];
        IntelReport r;
        r.arrive_h = arrive_h;
        r.observed_h = observed_h;
        r.source = src;
        r.object_id = o.id;
        r.kind = o.decoy ? o.looks_like : o.kind;
        r.pos = st.pos + Vec3{rng_.Normal(0.0, sigma_m * 0.7), rng_.Normal(0.0, sigma_m * 0.7), 0.0};
        r.sigma_m = sigma_m;
        r.exists = 0.9;
        r.label = Label(i);
        r.move_mps = o.move_mps;
        const DefenceDef* d = &o.defence;
        if (o.decoy) {
            const TheatreObject& pb = data_.theatre.objects[static_cast<std::size_t>(player_)];
            d = &pb.defence;
        }
        if (d->present) {
            r.threat_radius_m = d->radius_m * rng_.Uniform(0.85, 1.15);
            r.threat_lethality = d->lethality * rng_.Uniform(0.8, 1.2);
            r.threat_low = d->low_cover;
            r.threat_medium = d->medium_cover;
            r.threat_radar = d->radar;
            r.threat_channels = d->channels;
            r.threat_max_target_mps = d->max_target_mps;
            r.system = d->system;
        }
        return r;
    }

    std::string Label(int i) const {
        const TheatreObject& o = data_.theatre.objects[static_cast<std::size_t>(i)];
        const Vec3 at = truth_[static_cast<std::size_t>(i)].pos;
        if (o.decoy) return std::string(o.looks_like == ObjectKind::SamSite ? "SAM battery" : ObjectKindName(o.looks_like)) + " near " + NearestPlace(at);
        if (o.defence.present && IsAirDefence(o.kind)) return SystemName(o.defence.system) + " near " + NearestPlace(at);
        if (o.mobile) return o.name + " near " + NearestPlace(at);
        return o.name;
    }
    static std::string SystemName(const std::string& s) {
        const AirDefenceSystem* sys = FindAirDefenceSystem(s);
        return sys != nullptr ? std::string(sys->label) : s + " battery";
    }
    std::string NearestPlace(const Vec3& p) const {
        double best = 1e18;
        std::string name = "the front";
        for (const TheatreObject& o : data_.theatre.objects) {
            if (o.kind != ObjectKind::City) continue;
            const double d = Dist2d(o.pos, p);
            if (d < best) {
                best = d;
                name = o.name;
            }
        }
        return name;
    }

    // From the line of contact, either side (F2.1: at zero offsets the front's line, to the bit).
    double DistanceToFront(const Vec3& p) const { return front_line_.AbsDistance(p); }
    // How far behind the line of contact on our side (the relocation rule's, F3.7): with the ground war on, signed (a place
    // on the enemy's side of it is never far enough); off, DistanceToFront as before.
    double BehindFront(const Vec3& p) const { return ground_on_ ? front_line_.DistanceTo(p) : DistanceToFront(p); }

    // ------------------------------------------------------------------ the ground war (track F, F3.3-F3.4)

    // Under ground.mode 1, on a theatre with a country map and a faction with ground files: both sides at U0 on the line
    // of contact (groundwar::GroundWar). At 0, or without the map or the files, nothing is built and no draw is made.
    bool StartGroundWar(std::string* error) {
        ground_on_ = false;
        ground_war_ = groundwar::GroundWar();
        tie_fn_.clear();
        tie_before_.clear();
        tie_place_.clear();
        home_ours_.clear();
        fscl_m_.clear();
        fscl_version_ = 0;
        ground_air_ = GroundAirStats();
        if (data_.tuning.Get("ground.mode") < 1.0 || data_.country == nullptr || data_.data_dir.empty()) return true;
        if (!groundwar::HasGroundData(data_.data_dir, data_.faction.id)) return true;  // its front stays the border (03 section 8)
        auto gd = std::make_shared<groundwar::GroundWarData>();
        std::string e;
        if (!groundwar::LoadGroundWarData(data_.data_dir, data_.faction.id, *gd, &e) ||
            !ground_war_.Build(gd, front_line_, data_.theatre, data_.country.get(), seed_, &e)) {
            if (error != nullptr) *error = e;
            return false;
        }
        ground_on_ = true;
        // The air war follows the units (F3.7): the tied troops at their homes and whole, what the enemy can overrun (ours
        // at home), NATO's FSCL as first drawn.
        const std::vector<groundwar::TroopsTie>& ties = ground_war_.Ties();
        tie_fn_.assign(ties.size(), 1.0);
        tie_before_.assign(ties.size(), 1.0);
        for (std::size_t k = 0; k < ties.size(); ++k) {
            tie_place_.push_back(ties[k].home);
            truth_[static_cast<std::size_t>(ties[k].object)].function = tie_fn_[k] = ground_war_.TroopsFunction(k, 0.0);
        }
        tie_line_version_ = overrun_line_version_ = front_line_.Version();
        tie_places_version_ = ground_war_.PlacesVersion();
        tie_strength_version_ = ground_war_.StrengthVersion();
        tie_sync_h_ = 0.0;
        home_ours_.assign(truth_.size(), 0);
        place_.assign(truth_.size(), FrontPlace());
        place_at_.assign(truth_.size(), Vec3{});
        placed_.assign(truth_.size(), 0);
        for (std::size_t i = 0; i < truth_.size(); ++i) home_ours_[i] = front_line_.Place(truth_[i].pos).side > 0.0 ? 1 : 0;
        fscl_m_ = ground_war_.Fscl();
        fscl_version_ = ground_war_.FsclVersion();
        return true;
    }

    // ---- our units' moves on orders (track F, F4.2; unit_moves.hpp, docs/fronts/04_AIR_DEFENCE_STRATEGY.md section 3)

    // At ad_strategy.mode 1 on a front with a line: the classes and which objects move by them. At 0 nothing is loaded.
    bool StartUnitMoves(std::string* error) {
        moves_on_ = false;
        move_rules_ = UnitMoveRules();
        move_class_.clear();
        move_anchor_.clear();
        move_mobility_.clear();
        moves_.clear();
        active_moves_.clear();
        move_keys_.clear();
        inbox_.clear();
        inbox_next_ = 0;
        next_source_h_ = 0.0;
        next_move_event_h_ = 1e18;
        units_overrun_ = 0;
        move_routes_ = nav::RouteCache(64);
        move_roads_.reset();
        if (data_.tuning.Get("ad_strategy.mode") < 1.0 || front_line_.Empty() || data_.data_dir.empty()) return true;
        std::string e;
        if (!LoadUnitMoveRules(data_.data_dir, "campaign/ad_strategy.json", move_rules_, &e)) {
            if (error != nullptr) *error = e;
            return false;
        }
        moves_on_ = true;
        ApplyMoveRules();
        return true;
    }

    // The classes each object moves by, and the classes' mobility for the route service.
    void ApplyMoveRules() {
        const std::size_t n = data_.theatre.objects.size();
        move_class_.assign(n, -1);
        move_anchor_.assign(n, Vec3{});
        move_mobility_.clear();
        for (std::size_t i = 0; i < n; ++i) {
            move_class_[i] = move_rules_.ClassOf(data_.theatre.objects[i]);
            move_anchor_[i] = data_.theatre.objects[i].pos;
        }
        for (const MoveClassDef& c : move_rules_.classes) {
            nav::Mobility m;
            m.id = "ad_move:" + c.id;
            m.name = c.name;
            const double v = c.road_kmh / 3.6;
            m.road_mps = {v, v, v};
            m.cross_country_mps = c.cross_kmh / 3.6;
            m.night_factor = std::min(1.0, c.night_kmh / c.road_kmh);
            move_mobility_.push_back(m);
        }
    }

    static std::string PointKey(const Vec3& p) {
        return "p" + std::to_string(static_cast<long long>(std::llround(p.x))) + ":" + std::to_string(static_cast<long long>(std::llround(p.y)));
    }

    // The march's hours from `from` to `to` leaving at depart_h: the route service on the country's roads (cached by the
    // ends, the class and the bridges down: the ground war's roads and overlay when it runs, else the country's roads),
    // its roads at the class's speed by day or by night as the march starts; a way off them up to no_road_near_m at the
    // cross-country speed, a longer one on the local roads the country's map does not draw (x road_factor at the road
    // speed); no way on the roads, or one longer than detour_max straight lines: the straight line x road_factor.
    double MarchHours(int cls, const Vec3& from, const Vec3& to, double depart_h, UnitMove& m) {
        const MoveClassDef& c = move_rules_.classes[static_cast<std::size_t>(cls)];
        const bool night = TheatrePartAt(data_.theatre, data_.theatre.start_hour + depart_h) != DayPart::Day;
        const double kmh = night ? c.night_kmh : c.road_kmh;
        const nav::RoadGraph* roads = nullptr;
        const nav::Overlay* overlay = &move_open_;
        if (ground_on_ && ground_war_.Roads() != nullptr) {
            overlay = &ground_war_.RoadOverlayNow();
            roads = ground_war_.Roads();
        } else if (data_.country != nullptr) {
            if (!move_roads_) move_roads_ = std::make_shared<const nav::RoadGraph>(nav::RoadGraph::ForCountry(data_.country->Def()));
            roads = move_roads_.get();
        }
        if (roads != nullptr) {
            const nav::Route& r = move_routes_.Get(*roads, PointKey(from), {from.x, from.y}, PointKey(to), {to.x, to.y},
                                                   move_mobility_[static_cast<std::size_t>(cls)], *overlay);
            if (r.found && r.length_m <= move_rules_.detour_max * Dist2d(from, to)) {
                double road_km = 0.0, cross_km = 0.0;
                for (const nav::RouteLeg& l : r.legs) {
                    if (l.kind == "road") {
                        road_km += l.length_m / 1000.0;
                    } else if (l.length_m <= move_rules_.no_road_near_m) {
                        cross_km += l.length_m / 1000.0;
                    } else {
                        road_km += l.length_m * move_rules_.road_factor / 1000.0;
                    }
                }
                m.road_km = road_km;
                m.cross_km = cross_km;
                m.on_roads = true;
                return road_km / kmh + cross_km / c.cross_kmh;
            }
        }
        m.road_km = Dist2d(from, to) / 1000.0 * move_rules_.road_factor;
        m.cross_km = 0.0;
        m.on_roads = false;
        return m.road_km / kmh;
    }

    // A move begun now on an order its unit heard (in_step: the order source's, at a whole hour of a step).
    bool MoveUnitNow(const UnitOrder& o, bool in_step) {
        if (!moves_on_ || o.key.empty() || move_keys_.count(o.key) > 0) return false;
        const int idx = data_.theatre.IndexOf(o.unit);
        if (idx < 0) return false;
        const std::size_t i = static_cast<std::size_t>(idx);
        const int cls = move_class_[i];
        ObjectState& st = truth_[i];
        if (cls < 0 || st.overrun || st.function <= 0.05 || st.march != 0) return false;
        move_keys_.insert(o.key);
        CampaignAction& a = Record(CampaignAction::Kind::UnitMove);
        a.what = o.unit;
        a.by = o.key;
        a.to = o.to;
        a.until_h = now_h_;
        a.observed_h = o.heard_h;
        a.in_step = in_step;
        const MoveClassDef& c = move_rules_.classes[static_cast<std::size_t>(cls)];
        UnitMove m;
        m.key = o.key;
        m.object = idx;
        m.cls = cls;
        m.from = st.pos;
        m.to = o.to;
        m.heard_h = o.heard_h;
        m.start_h = now_h_;
        m.depart_h = now_h_ + c.pack_h;
        m.arrive_h = m.depart_h + MarchHours(cls, m.from, m.to, m.depart_h, m);
        m.ready_h = m.arrive_h + c.setup_h + c.checks_h;
        st.march = 1;
        moves_.push_back(m);
        active_moves_.push_back(static_cast<int>(moves_.size()) - 1);
        next_move_event_h_ = std::min(next_move_event_h_, m.depart_h);
        const TheatreObject& ob = data_.theatre.objects[i];
        char buf[192];
        std::snprintf(buf, sizeof(buf), " packs up for a move on orders: %.0f km %s, on the road at %s, ready at %s.",
                      m.road_km + m.cross_km, m.on_roads ? "by the roads" : "across the country", FormatHour(m.depart_h, data_.theatre.start_hour).c_str(),
                      FormatHour(m.ready_h, data_.theatre.start_hour).c_str());
        Log("ad_move", ob.id, ob.name + buf, false);
        return true;
    }

    // The moves' stages due by now: packed (on the road, at no place), arrived (setting up at the new place), ready (on the
    // air again). A unit wrecked while it packs or sets up stops where it is.
    void AdvanceMoves() {
        next_move_event_h_ = 1e18;
        std::size_t keep = 0;
        for (std::size_t k = 0; k < active_moves_.size(); ++k) {
            UnitMove& m = moves_[static_cast<std::size_t>(active_moves_[k])];
            const std::size_t i = static_cast<std::size_t>(m.object);
            ObjectState& st = truth_[i];
            const TheatreObject& o = data_.theatre.objects[i];
            if (m.stage != 1 && st.function <= 0.05) {
                m.stage = -2;
                m.end_h = now_h_;
                st.march = 0;
                Log("ad_move", o.id, o.name + " was wrecked on its move.", false);
                continue;
            }
            if (m.stage == 0 && now_h_ + 1e-9 >= m.depart_h) {
                m.stage = 1;
                st.march = 2;
            }
            if (m.stage == 1 && now_h_ + 1e-9 >= m.arrive_h) {
                m.stage = 2;
                st.march = 1;
                st.pos = m.to;
                move_anchor_[i] = m.to;
            }
            if (m.stage == 2 && now_h_ + 1e-9 >= m.ready_h) {
                m.stage = 3;
                m.end_h = m.ready_h;
                st.march = 0;
                st.last_moved_h = now_h_;
                Log("ad_move", o.id, o.name + " is ready at its new position.", false);
                continue;
            }
            next_move_event_h_ = std::min(next_move_event_h_, m.stage == 0 ? m.depart_h : m.stage == 1 ? m.arrive_h : m.ready_h);
            active_moves_[keep++] = active_moves_[k];
        }
        active_moves_.resize(keep);
    }

    // At the end of each step: the moves' stages due; at a whole hour, a load's moves of the hour and the order source's.
    void StepUnitMoves() {
        if (now_h_ + 1e-9 >= next_move_event_h_) AdvanceMoves();
        if (std::fabs(now_h_ - std::round(now_h_)) > 1e-9) return;
        bool began = false;
        for (; inbox_next_ < inbox_.size() && inbox_[inbox_next_].h <= now_h_ + 1e-9; ++inbox_next_) began = OrderNow(inbox_[inbox_next_].order) || began;
        if (order_source_ && now_h_ + 1e-9 >= next_source_h_) {
            std::vector<UnitOrder> heard;
            double next = now_h_ + 1.0;
            order_source_(*this, heard, &next);
            next_source_h_ = std::max(next, now_h_ + 1.0);
            for (const UnitOrder& o : heard) began = OrderNow(o) || began;
        }
        if (began && now_h_ + 1e-9 >= next_move_event_h_) AdvanceMoves();
    }

    // An order heard inside a step: the fighters' duty by sector (F4.3), else a move (true when a move began).
    bool OrderNow(const UnitOrder& o) {
        if (!o.fighter_shares.empty()) {
            FighterCoverNow(o, true);
            return false;
        }
        return MoveUnitNow(o, true);
    }

    // ---- our fighters by sector (track F, F4.3; docs/fronts/04_AIR_DEFENCE_STRATEGY.md section 5)

    bool StartFighterCover(std::string* error) {
        fighters_on_ = false;
        fighter_rules_ = FighterCoverRules();
        fighter_shares_.clear();
        fighter_cover_.clear();
        fighter_belief_.clear();
        fighter_changes_.clear();
        fighter_keys_.clear();
        if (data_.tuning.Get("ad_strategy.mode") < 1.0 || front_line_.Empty() || data_.data_dir.empty()) return true;
        std::string e;
        if (!LoadFighterCoverRules(data_.data_dir, "campaign/ad_strategy.json", fighter_rules_, &e)) {
            if (error != nullptr) *error = e;
            return false;
        }
        fighters_on_ = fighter_rules_.present && front_line_.SectorCount() > 0;
        return true;
    }

    // The regiment's duty by sector from now: the shares cached as the cover by sector (SpreadFighterCover), recorded.
    bool FighterCoverNow(const UnitOrder& o, bool in_step) {
        if (!fighters_on_ || o.key.empty() || fighter_keys_.count(o.key) > 0 || o.fighter_shares.size() != front_line_.SectorCount()) return false;
        fighter_keys_.insert(o.key);
        CampaignAction& a = Record(CampaignAction::Kind::FighterCover);
        a.what = o.unit;
        a.by = o.key;
        a.until_h = now_h_;
        a.observed_h = o.heard_h;
        a.in_step = in_step;
        a.shares = o.fighter_shares;
        const double front = FrontFighters();
        fighter_shares_ = o.fighter_shares;
        fighter_cover_ = SpreadFighterCover(front, fighter_shares_, fighter_rules_.cap);
        if (fighter_belief_.empty()) fighter_belief_.assign(fighter_cover_.size(), front);  // the enemy's: the front's number
        FighterCoverChange ch;
        ch.key = o.key;
        ch.h = now_h_;
        ch.heard_h = o.heard_h;
        ch.shares = fighter_shares_;
        ch.cover = fighter_cover_;
        fighter_changes_.push_back(ch);
        std::string text = "Our fighter regiment's duty by sector:";
        for (std::size_t i = 0; i < fighter_cover_.size(); ++i) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "%s %.40s %.2f", i == 0 ? "" : ",", front_line_.Sector(i).id.c_str(), fighter_cover_[i]);
            text += buf;
        }
        Log("ad_fighters", o.unit, text + ".", false);
        return true;
    }

    // The enemy's staff learns a sector's cover from its packages' debriefs there: each moves its belief of the target's
    // sector belief_learn of the way to the cover as it is (game value).
    void LearnFighterCover(const Package& p) {
        if (fighter_belief_.empty() || fighter_cover_.empty()) return;
        const int s = front_line_.SectorOf(p.target_pos);
        if (s < 0 || static_cast<std::size_t>(s) >= fighter_belief_.size()) return;
        double& b = fighter_belief_[static_cast<std::size_t>(s)];
        b += fighter_rules_.belief_learn * (fighter_cover_[static_cast<std::size_t>(s)] - b);
    }

    // The line overran a unit that moves on orders (Overrun): lost with the ground, its move (packing, or setting up where
    // it went) ended.
    void UnitOverrun(std::size_t i) {
        ++units_overrun_;
        for (std::size_t k = 0; k < active_moves_.size(); ++k) {
            UnitMove& m = moves_[static_cast<std::size_t>(active_moves_[k])];
            if (static_cast<std::size_t>(m.object) != i) continue;
            m.stage = -1;
            m.end_h = now_h_;
            active_moves_.erase(active_moves_.begin() + static_cast<std::ptrdiff_t>(k));
            break;
        }
        truth_[i].march = 0;
    }

    // The line has passed where a unit stood after it left (not overrun itself then): it got away.
    void MovesEscaped() {
        for (UnitMove& m : moves_) {
            if (m.escaped || m.stage < 1 || truth_[static_cast<std::size_t>(m.object)].overrun) continue;
            if (front_line_.DistanceTo(m.from) < 0.0) m.escaped = true;
        }
    }

    struct Recorded {
        double h = 0.0;
        UnitOrder order;
    };
    bool moves_on_ = false;
    UnitMoveRules move_rules_;
    std::vector<int> move_class_;               // by object: its class (-1 none)
    std::vector<Vec3> move_anchor_;             // by object: what its prepared positions are about (home, then its last order's place)
    std::vector<nav::Mobility> move_mobility_;  // by class: for the route service
    std::vector<UnitMove> moves_;
    std::vector<int> active_moves_;             // moves_ not ended
    std::set<std::string> move_keys_;           // the orders' keys taken
    std::vector<Recorded> inbox_;               // a load's moves heard inside a step, at their hours
    std::size_t inbox_next_ = 0;
    UnitOrderSource order_source_;
    double next_source_h_ = 0.0;
    double next_move_event_h_ = 1e18;
    int units_overrun_ = 0;
    nav::RouteCache move_routes_{64};
    std::shared_ptr<const nav::RoadGraph> move_roads_;  // without the ground war's: the country's, built on the first move
    // F4.3
    bool fighters_on_ = false;
    FighterCoverRules fighter_rules_;
    std::vector<double> fighter_shares_;  // by sector: the regiment's duty as last given (empty: none yet)
    std::vector<double> fighter_cover_;   // by sector: the cover cached from it
    std::vector<double> fighter_belief_;  // by sector: the enemy's staff's belief of it
    std::vector<FighterCoverChange> fighter_changes_;
    std::set<std::string> fighter_keys_;
    nav::Overlay move_open_;                    // no bridges down (without the ground war's overlay)

    // ---- the air war follows the units (track F, F3.7; docs/fronts/03_GROUND_WAR.md section 6)

    // The enemy's strikes since the last hour on our tied troops: every drop of a tied object's working share below what
    // its unit last set is the enemy's (the abstract resolution's, a played raid's, an armed reconnaissance's), and comes
    // off its unit (GroundWar::AirStrike).
    using GaClock = std::chrono::steady_clock;
    GaClock::time_point GaStart() const { return ground_war_.Profiling() ? GaClock::now() : GaClock::time_point(); }
    void GaLap(GroundAirStats::Part part, const GaClock::time_point& t0) {
        if (ground_war_.Profiling()) ground_air_.ms[part] += std::chrono::duration<double, std::milli>(GaClock::now() - t0).count();
    }
    void TroopsStruck() {
        const GaClock::time_point t0 = GaStart();
        const std::vector<groundwar::TroopsTie>& ties = ground_war_.Ties();
        const double h = ground_war_.Now();
        for (std::size_t k = 0; k < ties.size(); ++k) {
            const double damage = tie_fn_[k] - truth_[static_cast<std::size_t>(ties[k].object)].function;
            if (!(damage > 1e-12)) continue;
            const double lost = ground_war_.AirStrike(k, damage, h, &truth_[static_cast<std::size_t>(ties[k].object)].pos);
            const double est = ground_war_.Formations()[static_cast<std::size_t>(ties[k].formation)].base_establishment;
            ++ground_air_.troop_strikes;
            if (est > 0.0) ground_air_.strength_lost += lost / est;
        }
        GaLap(GroundAirStats::Strikes, t0);
    }
    // Around the supply lines' step: a tied object's working share risen is a march column's strength brought to its unit
    // (GroundWar::Reinforce); fallen, the lines' own stocks rule, which the unit's supply carries instead (TroopsSupply).
    void TroopsBefore() {
        const GaClock::time_point t0 = GaStart();
        const std::vector<groundwar::TroopsTie>& ties = ground_war_.Ties();
        for (std::size_t k = 0; k < ties.size(); ++k) tie_before_[k] = truth_[static_cast<std::size_t>(ties[k].object)].function;
        GaLap(GroundAirStats::Supply, t0);
    }
    void TroopsBrought() {
        const GaClock::time_point t0 = GaStart();
        const std::vector<groundwar::TroopsTie>& ties = ground_war_.Ties();
        const double h = ground_war_.Now();
        for (std::size_t k = 0; k < ties.size(); ++k) {
            double& fn = truth_[static_cast<std::size_t>(ties[k].object)].function;
            const double rise = fn - tie_before_[k];
            if (!(rise > 1e-12)) {
                fn = tie_before_[k];  // the lines' decay out of stocks: the unit's supply carries it, not its strength
                continue;
            }
            const double added = ground_war_.Reinforce(k, rise, h);
            const double est = ground_war_.Formations()[static_cast<std::size_t>(ties[k].formation)].base_establishment;
            ++ground_air_.reinforced;
            if (est > 0.0) ground_air_.strength_brought += added / est;
        }
        GaLap(GroundAirStats::Supply, t0);
    }
    // Each tied formation's supply from its objects' stocks: troops.full_supply_days of the air war's consumption or more is
    // full, below in proportion; a formation's is the mean of its objects' by their share of it (objects no line feeds keep
    // their stocks whole). Formations with no tied object keep theirs.
    void TroopsSupply() {
        const std::vector<groundwar::TroopsTie>& ties = ground_war_.Ties();
        if (ties.empty()) return;
        const GaClock::time_point t0 = GaStart();
        const std::size_t nf = ground_war_.Formations().size();
        supply_sum_.assign(nf, 0.0);
        supply_weight_.assign(nf, 0.0);
        const double full = data_.tuning.Get("logistics.consumption_per_day") * ground_war_.Data().tables.troops.full_supply_days;
        for (const groundwar::TroopsTie& t : ties) {
            const double stock = truth_[static_cast<std::size_t>(t.object)].supply;
            const double w = t.share * t.unit_base;
            const std::size_t f = static_cast<std::size_t>(t.formation);
            supply_sum_[f] += w * (full > 0.0 ? std::min(1.0, stock / full) : 1.0);
            supply_weight_[f] += w;
        }
        for (std::size_t f = 0; f < nf; ++f) {
            if (supply_weight_[f] > 0.0) ground_war_.SetSupply(f, supply_sum_[f] / supply_weight_[f]);
        }
        GaLap(GroundAirStats::Supply, t0);
    }
    // After the ground war's hour: the tied troops' working shares from their units; their places when the line or an order
    // moved them (their own moves round their place kept), and the supply lines' ends with them; where the line has moved,
    // our mobile objects back from it and our objects behind it overrun; NATO's FSCL as last drawn.
    void TroopsSync() {
        GaClock::time_point t0 = GaStart();
        const std::vector<groundwar::TroopsTie>& ties = ground_war_.Ties();
        const double h = ground_war_.Now();
        // On a loss or gain the ground war's units took, else once a day (their rest rate between).
        if (ground_war_.StrengthVersion() != tie_strength_version_ || h - tie_sync_h_ >= 24.0) {
            tie_strength_version_ = ground_war_.StrengthVersion();
            tie_sync_h_ = h;
            for (std::size_t k = 0; k < ties.size(); ++k) {
                ObjectState& st = truth_[static_cast<std::size_t>(ties[k].object)];
                st.function = tie_fn_[k] = ground_war_.TroopsFunction(k, h);
                if (st.function > 0.0) {
                    st.wrecked_h = -1.0;
                } else if (st.wrecked_h < 0.0) {
                    st.wrecked_h = now_h_;
                }
            }
        }
        if (front_line_.Version() != tie_line_version_ || ground_war_.PlacesVersion() != tie_places_version_) {
            tie_line_version_ = front_line_.Version();
            tie_places_version_ = ground_war_.PlacesVersion();
            for (std::size_t k = 0; k < ties.size(); ++k) {
                const Vec3 p = ground_war_.TroopsPlace(k, front_line_);
                const Vec3 d = p - tie_place_[k];
                const double moved = d.HorizontalLength();
                if (moved <= 1.0) continue;
                ObjectState& st = truth_[static_cast<std::size_t>(ties[k].object)];
                st.pos += Vec3{d.x, d.y, 0.0};
                tie_place_[k] = p;
                ++ground_air_.troop_moves;
                ground_air_.troops_moved_km += moved / 1000.0;
                ground_air_.supply_reroutes += supply_.MoveEnd(data_, ties[k].object, st.pos, truth_);
            }
        }
        if (ground_war_.FsclVersion() != fscl_version_) {
            fscl_version_ = ground_war_.FsclVersion();
            fscl_m_ = ground_war_.Fscl();
        }
        GaLap(GroundAirStats::Sync, t0);
        if (front_line_.Version() != overrun_line_version_) {
            t0 = GaStart();
            overrun_line_version_ = front_line_.Version();
            Escape();
            Overrun();
            if (moves_on_) MovesEscaped();
            GaLap(GroundAirStats::Overrun, t0);
        }
    }
    bool IsDummy(std::size_t i) const { return std::find(dummies_.begin(), dummies_.end(), static_cast<int>(i)) != dummies_.end(); }
    // How far object i stands behind the line of contact (FrontLine::DistanceTo to the bit), its projection on the base
    // line kept until it moves: a step of the line costs an offset's lookup an object.
    double BehindLine(std::size_t i) {
        const Vec3& p = truth_[i].pos;
        if (place_at_[i].x != p.x || place_at_[i].y != p.y || !placed_[i]) {
            place_[i] = front_line_.Place(p);
            place_at_[i] = p;
            placed_[i] = 1;
        }
        const FrontPlace& pl = place_[i];
        return pl.side * pl.distance_m - front_line_.Offset(pl.along_m);
    }
    // The relocation rule on the line of contact: our mobile objects (not the player's battery, its dummies, the supply
    // columns or the tied troops) it has come within 8 km of move back at once, to the nearest of their prepared positions
    // 8 km or more behind it, else straight back along the line's normal to 8 km and their relocation distance behind it.
    void Escape() {
        for (std::size_t i = 0; i < truth_.size(); ++i) {
            const TheatreObject& o = data_.theatre.objects[i];
            ObjectState& st = truth_[i];
            if (!o.mobile || home_ours_[i] == 0 || static_cast<int>(i) == player_ || IsDummy(i) || o.supply_line >= 0 ||
                ground_war_.TieOf(i) >= 0 || st.overrun || st.function <= 0.0) {
                continue;
            }
            if (moves_on_ && move_class_[i] >= 0) continue;  // it moves on its commander's order, or is lost with the ground (F4.2)
            const double behind = BehindLine(i);
            if (behind >= 8000.0) continue;
            double best = 1e18;
            const Vec3* to = nullptr;
            for (const Vec3& q : o.prepared) {
                const double dq = Dist2d(q, st.pos);
                if (dq < best && front_line_.DistanceTo(q) >= 8000.0) {
                    best = dq;
                    to = &q;
                }
            }
            if (to != nullptr) {
                st.pos = *to;
            } else {
                const FrontPlace pl = front_line_.Place(st.pos);
                st.pos += front_line_.InwardNormal(pl.along_m) * (8000.0 + std::max(0.0, o.relocate_m) - behind);
            }
            st.last_moved_h = now_h_;
            ++ground_air_.escapes;
            Log("ground", "the ground war", o.name + " moves back from the line of contact.", false);
        }
    }
    // Our objects behind the enemy's line of contact (their homes ours; not cities, the player's battery, its dummies, the
    // supply columns or the tied troops, which move with their units) are overrun: the enemy holds them, out of its targets
    // (IntelPicture::Exclude) and ours (no function, no missiles, no repair). Ours again once the line is back past them,
    // wrecked, to be mended as a wreck is.
    void Overrun() {
        for (std::size_t i = 0; i < truth_.size(); ++i) {
            const TheatreObject& o = data_.theatre.objects[i];
            ObjectState& st = truth_[i];
            if (home_ours_[i] == 0 || o.kind == ObjectKind::City || static_cast<int>(i) == player_ || IsDummy(i) || o.supply_line >= 0 ||
                ground_war_.TieOf(i) >= 0) {
                continue;
            }
            if (st.march == 2) continue;  // on the road: not there (F4.2)
            const bool behind = BehindLine(i) < 0.0;
            if (behind == st.overrun) continue;
            st.overrun = behind;
            if (behind && moves_on_ && move_class_[i] >= 0) UnitOverrun(i);  // lost with the ground: no order, or too late (F4.2)
            if (behind) {
                st.function = 0.0;
                if (st.missiles > 0) st.missiles = 0;
                st.wrecked_h = now_h_;
                intel_.Exclude(o.id);
                ++ground_air_.overrun;
                Log("ground", "the ground war", o.name + " is overrun: the enemy holds it.", false);
            } else {
                st.wrecked_h = now_h_;
                intel_.Include(o.id);
                ++ground_air_.retaken;
                Log("ground", "the ground war", o.name + " is ours again, wrecked.", false);
            }
        }
    }
    // A package as C2's authority table names its mission (CAS, BAI, AI), with the FSCL and by c2.bai_depth_m.
    void CountLabel(const Package& p) {
        if (p.kind == MissionKind::Cas) {
            ++ground_air_.cas;
            return;
        }
        if (p.kind != MissionKind::Strike) return;
        const GaClock::time_point t0 = GaStart();
        const AirFrontView v = FrontView();
        if (InBaiArea(p.target_pos, data_, v)) {
            ++ground_air_.bai;
        } else {
            ++ground_air_.ai;
        }
        AirFrontView fixed = v;
        fixed.fscl_m = nullptr;
        if (InBaiArea(p.target_pos, data_, fixed)) {
            ++ground_air_.bai_fixed;
        } else {
            ++ground_air_.ai_fixed;
        }
        GaLap(GroundAirStats::Labels, t0);
    }

    // Its hours up to t1, the line of contact moved; what happened into the journal (the truth, hidden: our side learns
    // it from its units' reports, F3.8).
    void StepGround(double t1) {
        groundwar::GroundEnv env;
        env.theatre = &data_.theatre;
        env.weather = &weather_;
        env.truth = &truth_;
        env.intel = &intel_;  // the enemy's air sightings of our troops, for its picture of our side (F3.5)
        // The player's battery's sector: U1 there and either side (F3.6).
        if (player_ >= 0) env.player_sector = front_line_.SectorOf(truth_[static_cast<std::size_t>(player_)].pos);
        ground_war_.Step(t1, env, front_line_);
        if (ground_war_.Log().empty()) return;
        if (!ground_war_.Profiling()) {
            JournalGround();
            return;
        }
        const auto r0 = std::chrono::steady_clock::now();
        JournalGround();
        ground_war_.AddReportsNs(
            static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - r0).count()), t1 - 1.0);
    }

    void JournalGround() {
        const groundwar::GroundWarData& gd = ground_war_.Data();
        const std::vector<groundwar::Formation>& forms = ground_war_.Formations();
        const std::vector<groundwar::Axis>& axes = ground_war_.Axes();
        const std::vector<groundwar::GroundCommander>& cmdrs = ground_war_.Commanders();
        const std::vector<groundwar::GroundLogEntry>& log = ground_war_.Log();
        const std::vector<groundwar::Regiment>& regs = ground_war_.Regiments();
        const auto reg_name = [&regs](int r) {
            return r >= 0 && static_cast<std::size_t>(r) < regs.size() ? regs[static_cast<std::size_t>(r)].name : std::string("?");
        };
        const auto axis_name = [&axes](int a) {
            return a >= 0 && static_cast<std::size_t>(a) < axes.size() ? axes[static_cast<std::size_t>(a)].name : std::string("?");
        };
        const auto sector_name = [this](int s) {
            return s >= 0 && static_cast<std::size_t>(s) < front_line_.SectorCount() ? front_line_.Sector(static_cast<std::size_t>(s)).name
                                                                                     : std::string("?");
        };
        const auto form_name = [&forms](int f) {
            return f >= 0 && static_cast<std::size_t>(f) < forms.size() ? forms[static_cast<std::size_t>(f)].name : std::string("?");
        };
        const auto cmdr_name = [&cmdrs](int c) {
            return c >= 0 && static_cast<std::size_t>(c) < cmdrs.size() ? cmdrs[static_cast<std::size_t>(c)].Title() : std::string("?");
        };
        const auto halt_text = [](groundwar::HaltReason r) {
            switch (r) {
                case groundwar::HaltReason::None: return "halted";
                case groundwar::HaltReason::Objective: return "its objective reached";
                case groundwar::HaltReason::Spent: return "its attackers spent";
                case groundwar::HaltReason::Stalled: return "stalled, with no echelon coming, stopped by its senior";
                case groundwar::HaltReason::Breakpoint: return "its attackers stopped at their breakpoints";
                case groundwar::HaltReason::Counterattacked: return "held where our counterattack stopped it";
            }
            return "halted";
        };
        char buf[256];
        for (std::size_t k = 0; k < log.size(); ++k) {
            const groundwar::GroundLogEntry& g = log[k];
            std::string text;
            std::string who = gd.enemy.name;
            switch (g.what) {
                case groundwar::GroundNote::AxisStarts: {
                    std::string names;
                    for (int fi : axes[static_cast<std::size_t>(g.axis)].first) names += (names.empty() ? "" : ", ") + form_name(fi);
                    std::snprintf(buf, sizeof buf, " attack (the objective %.0f km in).", g.value / 1000.0);
                    text = "The ground offensive opens on " + axis_name(g.axis) + " in " + sector_name(g.sector) + ": " + names + buf;
                    break;
                }
                case groundwar::GroundNote::RiverReached:
                    std::snprintf(buf, sizeof buf, ": the attack reaches a river %.0f km in; the bridge there is %s.", g.value / 1000.0,
                                  g.intact ? "standing" : "down");
                    text = axis_name(g.axis) + buf;
                    break;
                case groundwar::GroundNote::RiverCrossed:
                    std::snprintf(buf, sizeof buf, ": across the river %.0f km in.", g.value / 1000.0);
                    text = axis_name(g.axis) + buf;
                    break;
                case groundwar::GroundNote::AxisHalted:
                    std::snprintf(buf, sizeof buf, " halts %.0f km in: %s.", g.value / 1000.0, halt_text(g.reason));
                    text = axis_name(g.axis) + buf;
                    break;
                case groundwar::GroundNote::DayLine: {
                    // One entry for the day's end: every sector the line has moved in.
                    text = "The line of contact at the day's end:";
                    std::size_t j = k;
                    for (; j < log.size() && log[j].what == groundwar::GroundNote::DayLine && log[j].h == g.h; ++j) {
                        std::snprintf(buf, sizeof buf, "%s %s %.1f km", j == k ? "" : ",", sector_name(log[j].sector).c_str(), log[j].value / 1000.0);
                        text += buf;
                    }
                    text += ".";
                    k = j - 1;
                    who = "the ground war";
                    break;
                }
                case groundwar::GroundNote::AirAttack: {
                    // The enemy's strike on a tied troops object, off its unit (F3.7).
                    who = gd.ours.name;
                    const std::string obj = g.other >= 0 && static_cast<std::size_t>(g.other) < data_.theatre.objects.size()
                                                ? data_.theatre.objects[static_cast<std::size_t>(g.other)].name
                                                : std::string("?");
                    std::snprintf(buf, sizeof buf, " loses %.1f%% of its strength to the enemy's air attack on ", 100.0 * g.value);
                    text = form_name(g.formation) + (g.regiment >= 0 ? " (" + reg_name(g.regiment) + ")" : std::string()) + buf + obj + ".";
                    break;
                }
                case groundwar::GroundNote::CommandHit: {
                    // Its staff learns its command post or artillery hit (F8.2).
                    if (g.formation >= 0 && static_cast<std::size_t>(g.formation) < forms.size() &&
                        forms[static_cast<std::size_t>(g.formation)].unit.side == groundwar::Side::Ours) {
                        who = gd.ours.name;
                    }
                    const char* what = g.other == static_cast<int>(groundwar::CommandHitKind::CommandPost) ? "its command post"
                                       : g.other == static_cast<int>(groundwar::CommandHitKind::ArtilleryStruck) ? "its artillery, struck from the air"
                                                                                                              : "its artillery, in the fighting";
                    std::snprintf(buf, sizeof buf, " hit: judged against the three-quarters line until command is restored, in %.0f h.", g.value);
                    text = form_name(g.formation) + "'s staff learns " + what + buf;
                    break;
                }
                case groundwar::GroundNote::Order: {
                    // A commander's decision (F3.4): who decided, about whom, where.
                    if (g.cmdr >= 0 && static_cast<std::size_t>(g.cmdr) < cmdrs.size() &&
                        cmdrs[static_cast<std::size_t>(g.cmdr)].side == groundwar::Side::Ours) {
                        who = gd.ours.name;
                    }
                    const std::string by = cmdr_name(g.cmdr);
                    const std::string unit = form_name(g.formation);
                    const std::string axis = axis_name(g.axis);
                    switch (g.order) {
                        case groundwar::GroundOrder::EchelonCommitted:
                            std::snprintf(buf, sizeof buf, " (due in %.0f h)%s.", g.value,
                                          g.other >= 0 && g.other != g.axis ? ", switched from its planned axis to the one that moves" : "");
                            text = by + " commits " + unit + " on " + axis + buf;
                            break;
                        case groundwar::GroundOrder::EchelonHeld:
                            text = by + " holds " + unit + " back: no axis moves.";
                            break;
                        case groundwar::GroundOrder::AxisStopped:
                            std::snprintf(buf, sizeof buf, " %.0f km in: it has stalled and no echelon is coming.", g.value / 1000.0);
                            text = by + " stops " + axis + buf;
                            break;
                        case groundwar::GroundOrder::AttackResumed:
                            std::snprintf(buf, sizeof buf, " from %.0f km in.", g.value / 1000.0);
                            text = by + " resumes the attack on " + axis + buf;
                            break;
                        case groundwar::GroundOrder::AttackStopped:
                            std::snprintf(buf, sizeof buf, " stops attacking at %.0f%% of its strength (past its breakpoint) and holds.", 100.0 * g.value);
                            text = unit + buf;
                            break;
                        case groundwar::GroundOrder::Reported:
                            std::snprintf(buf, sizeof buf, " reports his unit at %.0f%% of its strength.", 100.0 * g.value);
                            text = by + " (" + unit + ")" + buf;
                            break;
                        case groundwar::GroundOrder::WithdrawAsked:
                            std::snprintf(buf, sizeof buf, " asks to withdraw his unit at %.0f%% of its strength.", 100.0 * g.value);
                            text = by + " (" + unit + ")" + buf;
                            break;
                        case groundwar::GroundOrder::WithdrawOrdered:
                            std::snprintf(buf, sizeof buf, " to withdraw at %.0f%% and re-form%s.", 100.0 * g.value, g.other >= 0 ? " behind the next" : "");
                            text = by + " orders " + unit + buf;
                            break;
                        case groundwar::GroundOrder::WithdrawDecided:
                            std::snprintf(buf, sizeof buf, " withdraws at %.0f%% of its strength on its commander's own decision.", 100.0 * g.value);
                            text = unit + buf;
                            break;
                        case groundwar::GroundOrder::WithdrawRefused:
                            std::snprintf(buf, sizeof buf, " to hold at %.0f%%: nobody behind it, nobody to relieve it.", 100.0 * g.value);
                            text = by + " orders " + unit + buf;
                            break;
                        case groundwar::GroundOrder::Dispersed:
                            text = unit + (g.value >= 1.5   ? " breaks contact and disperses, as its kind does."
                                           : g.value >= 0.5 ? " breaks and disperses: its cohesion is gone."
                                                            : " breaks and disperses: no order to withdraw came.");
                            break;
                        case groundwar::GroundOrder::Reformed:
                            std::snprintf(buf, sizeof buf, " has re-formed behind the line at %.0f%% of its old strength.", 100.0 * g.value);
                            text = unit + buf;
                            break;
                        case groundwar::GroundOrder::ReliefOrdered:
                            std::snprintf(buf, sizeof buf, " (due in %.0f h).", g.value);
                            text = by + " sends " + unit + " to relieve " + form_name(g.other) + " on " + axis + buf;
                            break;
                        case groundwar::GroundOrder::Relieved:
                            text = unit + " is relieved by " + form_name(g.other) + " on " + axis + " and withdraws.";
                            break;
                        case groundwar::GroundOrder::CounterattackOrdered:
                            std::snprintf(buf, sizeof buf, " (his force %.1f times the penetration's).", g.value);
                            text = by + " orders a counterattack on " + axis + buf;
                            break;
                        case groundwar::GroundOrder::CounterattackStarts:
                            std::snprintf(buf, sizeof buf, " %.0f km in.", g.value / 1000.0);
                            text = "Our counterattack goes in on " + axis + buf;
                            break;
                        case groundwar::GroundOrder::CounterattackEnds:
                            std::snprintf(buf, sizeof buf, " ends with the line %.0f km in.", g.value / 1000.0);
                            text = "Our counterattack on " + axis + buf;
                            break;
                        case groundwar::GroundOrder::CoveringBack:
                            std::snprintf(buf, sizeof buf, " back to the line %.0f km in.", g.value / 1000.0);
                            text = by + " orders the covering force on " + axis + buf;
                            break;
                        case groundwar::GroundOrder::RaidStarts:
                            if (g.value > 0.0) {
                                std::snprintf(buf, sizeof buf, " tonight (%.1f to 1 where it goes in).", g.value);
                            } else {
                                std::snprintf(buf, sizeof buf, " tonight, where nobody of ours holds.");
                            }
                            text = by + " sends " + unit + " raiding on " + axis + buf;
                            break;
                        case groundwar::GroundOrder::RaidEnds:
                            std::snprintf(buf, sizeof buf, " ends at dawn, %.0f km in; it goes back to its line.", g.value / 1000.0);
                            text = "The raid on " + axis + buf;
                            break;
                        case groundwar::GroundOrder::LevelUp:
                            text = unit + " is followed regiment by regiment (near the battery's sector, " + sector_name(static_cast<int>(g.value)) + ").";
                            break;
                        case groundwar::GroundOrder::LevelDown:
                            text = unit + " is followed as a whole again (away from the battery's sector).";
                            break;
                        case groundwar::GroundOrder::RegimentReported:
                            std::snprintf(buf, sizeof buf, " reports at %.0f%% of its strength.", 100.0 * g.value);
                            text = reg_name(g.regiment) + " (" + unit + ")" + buf;
                            break;
                        case groundwar::GroundOrder::RegimentAsked:
                            std::snprintf(buf, sizeof buf, " asks to be relieved at %.0f%% of its strength.", 100.0 * g.value);
                            text = reg_name(g.regiment) + " (" + unit + ")" + buf;
                            break;
                        case groundwar::GroundOrder::RegimentReliefOrdered:
                            std::snprintf(buf, sizeof buf, " (due in %.0f h on the roads).", g.value);
                            text = by + " sends " + reg_name(g.other_regiment) + " to relieve " + reg_name(g.regiment) + buf;
                            break;
                        case groundwar::GroundOrder::RegimentRelieved:
                            text = reg_name(g.regiment) + " (" + unit + ") is relieved by " + reg_name(g.other_regiment) + " and goes back to recover.";
                            break;
                    }
                    break;
                }
            }
            journal_.push_back({g.h, "ground", who, text, false});
        }
        ground_war_.ClearLog();
    }

    // ------------------------------------------------------------------ the air front's measures (track F, F2.2)

    // Under air_front.mode 1: the pre-war picture kept for the sectors' baselines, the losses by sector from now on.
    void StartAirFront() {
        air_front_on_ = data_.tuning.Get("air_front.mode") >= 1.0;
        air_front_support_ = data_.tuning.Get("air_front.mode") >= 2.0;
        air_front_advancing_.clear();
        cover_book_ = SectorCoverBook();
        sector_cover_.clear();
        sector_losses_ = SectorLossWindow();
        air_front_depth_.clear();
        air_front_helo_depth_.clear();
        air_front_ = AirFrontState();
        air_front_moves_.clear();
        if (!air_front_on_) return;
        cover_book_.Start(intel_, front_line_);
        sector_losses_.Reset(front_line_.SectorCount());
        air_front_depth_.assign(front_line_.SectorCount(), 0.0);  // at the border: the period's default (02_AIR_FRONT.md section 2)
        air_front_helo_depth_.assign(front_line_.SectorCount(), 0.0);
        air_front_.Reset(front_line_.SectorCount(), now_h_);
    }
    // Each planning cycle: the believed cover over every sector's next band (what the decision reads).
    void MeasureAirFront() { cover_book_.Measure(intel_, front_line_, air_front_depth_, data_.tuning.Get("air_front.step_m"), sector_cover_); }
    // And the air staff's decision on it (F2.3, air_front.hpp DecideAirFront): its air operations chief (the planner's
    // post; the commander's while it is empty) on the picture, the measure, the losses and the politics; each move in the
    // journal; the depths written for the planner (F2.4). No draw: the war's sequence is the same as at mode 0.
    void DecideAirFrontNow() {
        // F7.2 (air_front.mode 2): its own axes advancing by sector, as its own units last reported them.
        if (air_front_support_ && ground_on_) ground_war_.EnemyAdvancing(front_line_.SectorCount(), air_front_advancing_);
        AirFrontRules rules;
        AirFrontInputs in;
        AirFrontChief who;
        const Officer* chief = AirFrontDecisionInputs(rules, in, who);
        DecideAirFront(data_.faction.air_front, rules, in, who, air_front_, air_front_moves_, &air_front_reward_, &air_front_scratch_);
        air_front_.Depths(air_front_depth_);
        air_front_.HeloDepths(air_front_helo_depth_);
        const std::string title = chief != nullptr ? chief->Title() : std::string("air staff");
        for (const AirFrontMove& m : air_front_moves_) Log("air_front", title, AirFrontLine(m), false);
    }

public:
    // What the decision reads now (fronts_bench times it on these): the game values, the enemy's own picture, the last
    // measure, its losses and politics, and its air operations chief (returned; null: the post empty, default traits).
    const Officer* AirFrontDecisionInputs(AirFrontRules& rules, AirFrontInputs& in, AirFrontChief& who) const {
        const TuningTable& tune = data_.tuning;
        rules.step_m = tune.Get("air_front.step_m");
        rules.min_baseline = tune.Get("air_front.min_baseline");
        rules.min_sorties = tune.Get("air_front.min_sorties");
        rules.helo_lag_h = tune.Get("air_front.helo_lag_h");
        rules.trait_shift = tune.Get("air_front.trait_shift");
        rules.reward_reach_m = tune.Get("air_front.reward_reach_m");
        const Officer* chief = staff_.HolderPtr(StaffRole::Planner);
        if (chief == nullptr) chief = staff_.HolderPtr(StaffRole::Commander);
        who = AirFrontChief();
        if (chief != nullptr) {
            who.traits = chief->def.traits;
            who.stress = chief->stress;
        }
        const AirFrontDef& def = data_.faction.air_front;
        in = AirFrontInputs();
        in.picture = &intel_;
        in.line = &front_line_;
        in.cover = &sector_cover_;
        in.losses = &sector_losses_;
        in.target_weight = data_.faction.politics.target_weight;
        in.banned = &politics_.banned;
        in.politics_max_depth_m = politics_.max_depth_m;
        in.event_fired = !def.after_event.empty() && politics_.fired_events.count(def.after_event) > 0;
        // Its own ground front collapsing, by sector, in its own ground picture (F3.5): only while the ground war runs.
        in.crisis = ground_on_ ? &ground_war_.EnemyCrisis() : nullptr;
        // Its own axes advancing, by sector, in the same picture (F7.2): only at air_front.mode 2 while the ground war runs.
        in.advancing = air_front_support_ && ground_on_ && air_front_advancing_.size() == front_line_.SectorCount() ? &air_front_advancing_ : nullptr;
        in.now_h = now_h_;
        return chief;
    }

private:
    // A move as the enemy's journal tells it (its side of the debrief, tools/air_war_view.py).
    std::string AirFrontLine(const AirFrontMove& m) const {
        char buf[256];
        const std::string sector = front_line_.Sector(static_cast<std::size_t>(m.sector)).name;
        const double km = 1000.0;
        switch (m.what) {
            case AirFrontMove::What::Seen:
                std::snprintf(buf, sizeof buf, "our air defence there judged broken (cover %.0f%%, mobile air defence %.0f%% destroyed, worth %.1f): the air front to move forward in %.0f h",
                              m.cover * 100.0, m.mobile_destroyed * 100.0, m.reward, m.after_h);
                break;
            case AirFrontMove::What::Advance:
                std::snprintf(buf, sizeof buf, "the air front moves forward to %.0f km (cover %.0f%%, mobile air defence %.0f%% destroyed, worth %.1f)",
                              m.depth_to_m / km, m.cover * 100.0, m.mobile_destroyed * 100.0, m.reward);
                break;
            case AirFrontMove::What::PullBack:
                std::snprintf(buf, sizeof buf, "the air front pulled back to %.0f km: %.1f%% lost a low-level sortie over %d sorties",
                              m.depth_to_m / km, m.loss_rate * 100.0, m.sorties);
                break;
            case AirFrontMove::What::SurgeEnd:
                std::snprintf(buf, sizeof buf, "the surge is over: the air front back at the border from %.0f km", m.depth_from_m / km);
                break;
            case AirFrontMove::What::Politics:
                std::snprintf(buf, sizeof buf, "the air front held to %.0f km by the politics", m.depth_to_m / km);
                break;
            case AirFrontMove::What::Helicopters:
                std::snprintf(buf, sizeof buf, "the helicopters follow to %.0f km after a day of clean low-level work (%d sorties, %.1f%% lost)",
                              m.helo_to_m / km, m.sorties, m.loss_rate * 100.0);
                break;
            case AirFrontMove::What::HeloBack:
                std::snprintf(buf, sizeof buf, "the crisis over: the helicopters back to %.0f km", m.helo_to_m / km);
                break;
            case AirFrontMove::What::Crisis:
                std::snprintf(buf, sizeof buf, "a crisis on the ground: the air front to %.0f km, the helicopters to %.0f km, whatever the losses",
                              m.depth_to_m / km, m.helo_to_m / km);
                break;
        }
        // F7.2: what of the worth was its own advancing axes' need for close support (only where it counted).
        if (m.support > 0.0 && (m.what == AirFrontMove::What::Seen || m.what == AirFrontMove::What::Advance)) {
            char more[96];
            std::snprintf(more, sizeof more, "; of the worth %.1f the close support of its own attack there", m.support);
            return sector + ": " + buf + more;
        }
        return sector + ": " + buf;
    }
    // A raid's low-level sorties (once, when it is first resolved) and its aircraft lost to ground fire, in its target's sector.
    void NoteSectorLosses(const Package& p, const PackageOutcome& o, bool first) {
        if (p.profile != Profile::Low) return;
        const int sector = front_line_.SectorOf(p.target_pos);
        const int day = SectorLossWindow::DayOf(p.tot_h);
        if (first) sector_losses_.NoteSorties(sector, day, p.Aircraft());
        int ground = 0;
        for (const LossRecord& l : o.losses) ground += (l.fighter || l.own_fire) ? 0 : 1;
        if (ground > 0) sector_losses_.NoteLosses(sector, day, ground);
    }

    // An ELINT fix on an emitter that radiated `seconds` (vr_1985_plss: a new source is located within seconds), or a
    // ghost between the battery and a dummy whose decoy emitter radiates on the same parameters.
    void Elint(const std::string& id, double seconds, double t) {
        const IntelDef& in = data_.faction.intel;
        const TuningTable& tune = data_.tuning;
        const int i = data_.theatre.IndexOf(id);
        if (i < 0 || seconds <= 0.0) return;
        // A faction with stand-off ELINT aircraft hears farther and fixes finer while one is on station; without, its
        // ground stations alone (docs/air_war/11_SUPPORT.md).
        double reach = in.elint_reach_m, fine = 1.0;
        if (support_.has_elint_aircraft) {
            const double without = tune.Get("support.elint_reach_without");
            reach *= without + (1.0 - without) * support_.elint;
            fine = tune.Get("support.elint_sigma_without") - (tune.Get("support.elint_sigma_without") - 1.0) * support_.elint;
        }
        if (DistanceToFront(truth_[static_cast<std::size_t>(i)].pos) > reach) return;
        const double sigma = (in.elint_best_m + in.elint_k_m / std::sqrt(std::max(1.0, seconds))) * fine;
        const bool battery = i == player_;
        if (battery && !dummies_.empty() && !posture_.decoy_mode.empty() && posture_.dummies > 0 &&
            rng_.Chance(tune.Get("intel.ghost_fix_chance"))) {
            int alive = -1;
            for (int di : dummies_) {
                if (truth_[static_cast<std::size_t>(di)].function > 0.0) alive = di;
            }
            if (alive >= 0) {
                IntelReport g = ReportAbout(player_, IntelSource::Elint, t, t + in.elint_delay_h, sigma);
                g.object_id.clear();
                g.ghost = true;
                const Vec3 a = truth_[static_cast<std::size_t>(player_)].pos;
                const Vec3 b = truth_[static_cast<std::size_t>(alive)].pos;
                g.pos = rng_.Chance(0.5) ? (a + b) * 0.5 : b;
                g.label = "SAM battery (ELINT)";
                intel_.Queue(g);
                return;
            }
        }
        IntelReport r = ReportAbout(i, IntelSource::Elint, t, t + in.elint_delay_h, sigma);
        intel_.Queue(r);
    }

    // ------------------------------------------------------------------ collection between raids

    void PriorIntel() {
        const IntelDef& in = data_.faction.intel;
        for (std::size_t i = 0; i < data_.theatre.objects.size(); ++i) {
            const TheatreObject& o = data_.theatre.objects[i];
            if (o.decoy) continue;
            if (!rng_.Chance(o.prior_known)) continue;
            const double sigma = o.mobile ? in.prior_mobile_sigma_m : 200.0;
            IntelReport r = ReportAbout(static_cast<int>(i), IntelSource::Prior, -24.0, 0.0, sigma);
            r.exists = o.mobile ? 0.6 : 1.0;
            intel_.Queue(r);
        }
        intel_.Deliver(0.0, in.analyst_skill);
    }

    void Collect(double t0, double t1) {
        const IntelDef& in = data_.faction.intel;
        const TuningTable& tune = data_.tuning;
        const double dt = t1 - t0;
        const Weather w = WeatherAt(t0);
        const Officer* io = staff_.HolderPtr(StaffRole::Intelligence);
        // Satellite passes.
        if (in.satellite && w != Weather::Poor) {
            for (double ph : in.satellite_pass_hours) {
                const double local0 = std::fmod(data_.theatre.start_hour + t0, 24.0);
                double delta = std::fmod(ph - local0 + 24.0, 24.0);
                if (delta >= dt) continue;
                const double when = t0 + delta;
                const double cloud = w == Weather::Cloudy ? 0.5 : 1.0;
                for (std::size_t i = 0; i < data_.theatre.objects.size(); ++i) {
                    const TheatreObject& o = data_.theatre.objects[i];
                    double pd = o.mobile || o.decoy ? in.satellite_detect : 0.9;
                    if (static_cast<int>(i) == player_ && posture_.nets) pd *= 0.5;
                    if (!rng_.Chance(pd * cloud)) continue;
                    IntelReport r = ReportAbout(static_cast<int>(i), IntelSource::Satellite, when, when + in.satellite_latency_h, 150.0);
                    r.function = truth_[i].function;
                    if (o.decoy) {
                        r.decoy_spotted = rng_.Chance(tune.Get("intel.decoy_spot_skill") * 0.5 *
                                                      (io != nullptr ? io->Effective(io->def.skills.intelligence) : 0.4));
                    }
                    intel_.Queue(r);
                }
            }
        }
        // Agents.
        if (rng_.Chance(in.humint_per_day * dt / 24.0)) {
            std::vector<int> pool;
            for (std::size_t i = 0; i < data_.theatre.objects.size(); ++i) {
                if (DistanceToFront(truth_[i].pos) <= in.humint_reach_m && data_.theatre.objects[i].kind != ObjectKind::City) {
                    pool.push_back(static_cast<int>(i));
                }
            }
            if (!pool.empty()) {
                std::vector<double> weights;
                for (int pi : pool) weights.push_back(1.0 / (1.0 + DistanceToFront(truth_[static_cast<std::size_t>(pi)].pos) / 30000.0));
                const int i = pool[static_cast<std::size_t>(rng_.Weighted(weights))];
                IntelReport r = ReportAbout(i, IntelSource::Humint, t0, t0 + in.humint_delay_h, in.humint_sigma_m);
                r.exists = 0.6;
                if (rng_.Chance(in.humint_false)) {
                    r.object_id.clear();
                    r.ghost = true;
                    r.pos += Vec3{rng_.Normal(0.0, 8000.0), rng_.Normal(0.0, 8000.0), 0.0};
                    r.label = Label(i) + " (agent)";
                }
                intel_.Queue(r);
            }
        }
        // Sympathisers who watch our troops (roadmap S7, learning.mode 1): word of our mobile air defence, more often where
        // it stands by a town than out in the country, less under its nets. Every draw from a fork keyed by the hour.
        if (in.humint_air_defence_per_day > 0.0 && tune.Get("learning.mode") >= 1.0) {
            Rng g = rng_.Fork(0x5E7A1EA3000ull + static_cast<std::uint64_t>(std::max(0.0, std::floor(t0 * 60.0 + 1e-6))));
            if (g.Chance(std::min(1.0, in.humint_air_defence_per_day * dt / 24.0))) {
                std::vector<int> pool;
                std::vector<double> weights;
                for (std::size_t i = 0; i < data_.theatre.objects.size(); ++i) {
                    const TheatreObject& o = data_.theatre.objects[i];
                    const bool player = static_cast<int>(i) == player_;
                    if (o.kind != ObjectKind::SamSite || o.decoy || (!o.mobile && !player) || truth_[i].function <= 0.05) continue;
                    if (DistanceToFront(truth_[i].pos) > in.humint_reach_m) continue;
                    bool town = false;
                    for (std::size_t k = 0; k < data_.theatre.objects.size(); ++k) {
                        if (data_.theatre.objects[k].kind == ObjectKind::City && Dist2d(truth_[k].pos, truth_[i].pos) <= tune.Get("learning.watch_m")) town = true;
                    }
                    double wt = town ? 1.0 : tune.Get("learning.countryside_share");
                    if (player && posture_.nets) wt *= 0.5;
                    pool.push_back(static_cast<int>(i));
                    weights.push_back(wt);
                }
                double wmax = 0.0;
                for (double wt : weights) wmax = std::max(wmax, wt);
                if (!pool.empty() && wmax > 0.0) {
                    const std::size_t k = static_cast<std::size_t>(g.Weighted(weights));
                    const int i = pool[k];
                    if (g.Chance(weights[k] / wmax)) {
                        IntelReport r = ReportAbout(i, IntelSource::Humint, t0, t0 + in.humint_delay_h, in.humint_sigma_m);
                        r.exists = 0.6;
                        r.note = "sympathisers saw it";
                        if (g.Chance(in.humint_false)) {
                            r.object_id.clear();
                            r.ghost = true;
                            r.pos += Vec3{g.Normal(0.0, 8000.0), g.Normal(0.0, 8000.0), 0.0};
                            r.label = Label(i) + " (sympathisers)";
                        }
                        intel_.Queue(r);
                        ++sympathiser_reports_;
                    }
                }
            }
        }
        // Radars that radiate in their routine (the player's battery by its posture).
        if (in.elint) {
            for (std::size_t i = 0; i < data_.theatre.objects.size(); ++i) {
                const TheatreObject& o = data_.theatre.objects[i];
                if (!o.emitter || truth_[i].function <= 0.05 || o.decoy || truth_[i].march != 0) continue;  // packed or on the road: silent (F4.2)
                const double share = static_cast<int>(i) == player_ ? posture_.emission : o.emission;
                if (share <= 0.0) continue;
                // Bursts: a share of the hour, in a few pieces; the collector catches some.
                if (!rng_.Chance(std::min(1.0, 0.3 + share))) continue;
                Elint(o.id, share * 3600.0 * dt * 0.25, t0 + 0.5 * dt);
            }
        }
        // Radio intercept of the battery's net, and of its dummy traffic. Fed by our side's pipeline, it hears the calls
        // the battery's people actually made, each a bearing for its direction finders; else a guess from the posture.
        if (in.comint) {
            auto heard = [&](double when) {
                IntelReport r = ReportAbout(player_, IntelSource::Comint, when, when + in.comint_delay_h, in.comint_sigma_m);
                r.exists = 0.6;
                r.label = "SAM battery (radio intercept)";
                intel_.Queue(r);
                ++comint_heard_;
            };
            if (traffic_fed_) {
                for (const Transmission& call : on_air_) {
                    if (call.h >= t0 + dt) continue;
                    if (rng_.Chance(Clamp01(call.intercept * tune.Get("intel.comint_per_call")))) heard(call.h);
                }
                on_air_.erase(std::remove_if(on_air_.begin(), on_air_.end(), [&](const Transmission& call) { return call.h < t0 + dt; }),
                              on_air_.end());
            } else if (rng_.Chance(Clamp01((1.0 - posture_.radio_discipline) * dt))) {
                heard(t0);
            }
            if (posture_.dummy_traffic && rng_.Chance(0.5 * dt)) {
                IntelReport g = ReportAbout(player_, IntelSource::Comint, t0, t0 + in.comint_delay_h, in.comint_sigma_m);
                g.object_id.clear();
                g.ghost = true;
                g.pos += Vec3{rng_.Normal(0.0, 6000.0), rng_.Normal(0.0, 6000.0), 0.0};
                g.label = "SAM battery (radio intercept)";
                intel_.Queue(g);
            }
        }
    }

    // ------------------------------------------------------------------ the officers learn

    void Lessons(Package& p, const PackageOutcome& o, int lost) {
        const TuningTable& tune = data_.tuning;
        const int n = std::max(1, p.Aircraft());
        const double loss_rate = static_cast<double>(lost) / n;
        double claimed = 0.0;
        if (const IntelEntry* e = intel_.Find(p.target_key)) claimed = Clamp01(1.0 - e->function);
        if (!o.delivered && (p.kind == MissionKind::Strike || p.kind == MissionKind::Dead)) claimed = 0.0;
        if (p.kind == MissionKind::Recce || p.kind == MissionKind::Bda) claimed = o.recce_seen.empty() ? 0.1 : 0.8;
        if (p.planner >= 0) {
            Officer& pln = staff_.officers[static_cast<std::size_t>(p.planner)];
            if (pln.active) {
                const double lr = Clamp01(pln.LearningRate(tune.Get("adapt.learning_rate")) + 0.1);
                TacticMemory& m = pln.tactics[p.tactic];
                m.loss_rate = m.uses == 0 ? loss_rate : Lerp(m.loss_rate, loss_rate, lr);
                m.success = m.uses == 0 ? claimed : Lerp(m.success, claimed, lr);
                ++m.uses;
                m.last_used_h = p.tot_h;
                pln.aircraft_lost += lost;
                if (loss_rate > p.expected_losses / n + 0.1) {
                    pln.stress = Clamp01(pln.stress + lost * tune.Get("adapt.stress_per_loss") * (1.0 - 0.6 * pln.def.traits.composure));
                    pln.confidence = Clamp01(pln.confidence - 0.05 * lost);
                    pln.reputation = Clamp01(pln.reputation - tune.Get("adapt.reputation_per_loss") * lost);
                } else if (claimed > 0.5) {
                    pln.confidence = Clamp01(pln.confidence + 0.05);
                    pln.reputation = Clamp01(pln.reputation + tune.Get("adapt.reputation_per_success"));
                }
            }
        }
        if (p.commander >= 0) {
            Officer& cdr = staff_.officers[static_cast<std::size_t>(p.commander)];
            if (cdr.active) {
                cdr.stress = Clamp01(cdr.stress + lost * 0.5 * tune.Get("adapt.stress_per_loss") * (1.0 - 0.6 * cdr.def.traits.composure));
                if (claimed > 0.5) cdr.reputation = Clamp01(cdr.reputation + 0.5 * tune.Get("adapt.reputation_per_success"));
                // Grudges against what shot them down.
                for (const LossRecord& l : o.losses) {
                    if (l.by.empty()) continue;
                    cdr.grudges[l.by] += 1.0 + cdr.Quirk("grudge");
                    for (const Flight& fl : p.flights) {
                        if (fl.officer >= 0 && staff_.officers[static_cast<std::size_t>(fl.officer)].active) {
                            staff_.officers[static_cast<std::size_t>(fl.officer)].grudges[l.by] += 1.0;
                        }
                    }
                }
            }
        }
        // The SEAD lesson, for the methodical: losses to radar defences on a package with little SEAD.
        int radar_losses = 0;
        for (const LossRecord& l : o.losses) {
            const TheatreObject* d = data_.theatre.Find(l.by);
            if (d != nullptr && d->defence.radar) ++radar_losses;
        }
        if (radar_losses > 0 && p.planner >= 0 && p.Count(FlightRole::Sead) < 2 * std::max(1, radar_losses)) {
            Officer& pln = staff_.officers[static_cast<std::size_t>(p.planner)];
            if (pln.active) {
                const double before = pln.lesson_sead;
                pln.lesson_sead = std::min(1.5, pln.lesson_sead + 0.25 * pln.LearningRate(1.0) * radar_losses);
                if (pln.lesson_sead >= 0.25 && before < 0.25) {
                    pln.Note(now_h_, data_.theatre.start_hour, "after losses to radar SAMs over " + p.target_label + ", plans more SEAD");
                    Log("decision", pln.Title(), "Lost " + std::to_string(radar_losses) + " to radar-guided defences over " + p.target_label +
                                                     ": more SEAD with every package from now on.", false);
                }
            }
        }
    }

    // ------------------------------------------------------------------ the day

    void Daily() {
        const FactionDef& f = data_.faction;
        const TuningTable& tune = data_.tuning;
        // Politics.
        const double tol = std::max(0.3, f.politics.loss_tolerance_per_day);
        const double excess = std::max(0.0, politics_.losses_today - tol);
        if (politics_.damage_today < 0.3) {
            politics_.pressure = Clamp01(politics_.pressure + tune.Get("politics.pressure_per_quiet_day"));
        } else {
            politics_.pressure = Clamp01(politics_.pressure - tune.Get("politics.pressure_relief_per_success") * politics_.damage_today);
        }
        politics_.support = Clamp01(politics_.support - tune.Get("politics.support_per_excess_loss") * excess);
        Events();
        // The build-up: squadrons reach the theatre (docs/air_war/12_FORCES.md).
        int arrived = 0;
        for (std::size_t i = 0; i < squadrons_.size() && i < f.squadrons.size(); ++i) {
            const int target = Arrived(f.squadrons[i], day_);
            if (target > squadrons_[i].total) {
                arrived += target - squadrons_[i].total;
                squadrons_[i].total = target;
            }
        }
        if (arrived > 0) {
            reinforcements_ += arrived;
            Log("buildup", "logistics", std::to_string(arrived) + " more aircraft arrive in theatre", false);
        }
        support_sorties_ += support_.sorties;
        // Logistics.
        double repl = f.logistics.replacements_per_day;
        for (SquadronState& s : squadrons_) {
            if (repl <= 0.0) break;
            if (s.lost > 0) {
                const int back = std::min(s.lost, static_cast<int>(repl + 0.5));
                s.lost -= back;
                repl -= back;
            }
        }
        arm_stock_ += f.logistics.arm_resupply_per_day;
        for (auto& m : missile_stock_) {
            if (const AircraftType* a = f.Aircraft(m.first)) m.second += a->resupply_per_day;
        }
        // Careers.
        Careers(excess);
        // Stress eases.
        for (Officer& o : staff_.officers) {
            if (o.active) o.stress = Clamp01(o.stress - tune.Get("adapt.stress_recovery_per_day") * (0.5 + o.def.traits.composure));
        }
        // Our side's daily assessment.
        OwnIntelDaily();
        politics_.losses_today = 0.0;
        politics_.damage_today = 0.0;
        player_missions_today_ = 0;
    }

    void Events() {
        for (const PoliticalEventDef& e : data_.faction.politics.events) {
            if (e.once && politics_.fired_events.count(e.id) > 0) continue;
            if (day_ < e.day_at_least) continue;
            if (e.losses_today_at_least >= 0.0 && politics_.losses_today < e.losses_today_at_least) continue;
            if (e.pressure_at_least >= 0.0 && politics_.pressure < e.pressure_at_least) continue;
            if (!rng_.Chance(e.chance)) continue;
            politics_.fired_events.insert(e.id);
            for (ObjectKind k : e.unban) politics_.banned.erase(std::remove(politics_.banned.begin(), politics_.banned.end(), k), politics_.banned.end());
            for (ObjectKind k : e.ban) {
                if (!politics_.Banned(k)) politics_.banned.push_back(k);
            }
            if (e.pause_h > 0.0) politics_.paused_until_h = now_h_ + e.pause_h;
            if (e.sortie_factor != 1.0) politics_.sortie_factor *= e.sortie_factor;
            if (e.min_altitude_profile >= 1.0) politics_.min_profile = 1;
            if (e.city_radius_m >= 0.0) politics_.city_radius_m = e.city_radius_m;
            if (e.max_depth_m >= 0.0) politics_.max_depth_m = e.max_depth_m;
            if (!e.reinforce_squadron.empty()) {
                for (SquadronState& s : squadrons_) {
                    if (s.id == e.reinforce_squadron) s.total += e.reinforce_count;
                }
            }
            arm_stock_ += e.arm_resupply;
            Log("politics", "politics", e.name + ": " + e.text, false);
        }
    }

    void Careers(double excess_losses) {
        const TuningTable& tune = data_.tuning;
        for (StaffRole r : {StaffRole::Commander, StaffRole::Planner, StaffRole::Intelligence, StaffRole::Sead}) {
            const int idx = staff_.Holder(r);
            if (idx < 0) continue;
            Officer& o = staff_.officers[static_cast<std::size_t>(idx)];
            // Superiors judge the day: results against pressure, losses beyond what is tolerated.
            o.reputation = Clamp01(o.reputation - 0.03 * excess_losses * (1.0 - 0.5 * o.Effective(o.def.skills.politics)) -
                                   0.02 * politics_.pressure * (politics_.damage_today < 0.3 ? 1.0 : 0.0) +
                                   0.02 * std::min(2.0, politics_.damage_today));
            if (o.reputation < tune.Get("adapt.relieve_below") && day_ > 2) {
                const std::string title = o.Title();
                Rng rr = rng_.Fork(static_cast<std::uint64_t>(idx) * 131u + static_cast<std::uint64_t>(day_));
                const int succ = staff_.Replace(idx, "relieved", "the superiors lost faith", data_, rr, now_h_);
                Log("career", title, title + " has been relieved.", false);
                if (succ >= 0) {
                    const Officer& n = staff_.officers[static_cast<std::size_t>(succ)];
                    Log("career", n.Title(), "Appointed " + std::string(StaffRoleName(r)) + ": " + DescribeTraits(n.def.traits) + ".", false);
                    Log("own_intel", "intercepts", "Intercepts suggest a change in the enemy's air staff: a new " + std::string(StaffRoleName(r)) + ".", true);
                    files_.erase(succ);
                }
            } else if (o.reputation > tune.Get("adapt.promote_above") && o.def.traits.ambition > 0.6 && day_ > 4) {
                const std::string title = o.Title();
                Rng rr = rng_.Fork(static_cast<std::uint64_t>(idx) * 173u + static_cast<std::uint64_t>(day_));
                const int succ = staff_.Replace(idx, "promoted", "promoted for the results", data_, rr, now_h_);
                Log("career", title, title + " has been promoted out of the theatre.", false);
                if (succ >= 0) {
                    Log("own_intel", "intercepts", "The enemy's " + std::string(StaffRoleName(r)) + " appears to have changed.", true);
                }
            }
        }
    }

    // ------------------------------------------------------------------ the player's side learns about the enemy

    // A raid's aircraft (or cruise missiles) down as our side counts them: for a played mission whose battery reported
    // from its records (roadmap 18.1), its reported kills and the neighbours' as they happened (their records are not
    // kept); otherwise the truth.
    int CountedDown(const PackageOutcome& o, int lost, bool missiles) const {
        if (!o.played || !o.reported) return missiles ? static_cast<int>(o.missiles_downed.size()) : lost;
        const std::string& own = data_.theatre.objects[static_cast<std::size_t>(player_)].id;
        int theirs = 0;
        if (missiles) {
            for (const std::string& by : o.missiles_downed) theirs += by != own ? 1 : 0;
        } else {
            for (const LossRecord& l : o.losses) theirs += l.by != own ? 1 : 0;
        }
        return theirs + o.reported_destroyed;
    }

    void ObserveRaid(const Package& p, const PackageOutcome& o, int lost) {
        const int down = CountedDown(o, lost, p.Missiles() > 0);
        // Only raids our side saw: those through the battery's sector, or that our radars and observers watched (all
        // raids that crossed into our territory are seen by the warning net; the detail comes from the sector's).
        {
            ObservedRaid r;
            r.h = p.tot_h;
            r.entry = p.entry;
            if (const FrontDef* fr = data_.theatre.FrontFor(data_.faction.front)) {
                for (const EntryPoint& e : fr->entries) {
                    if (e.id == p.entry) r.entry_pos = e.pos;
                }
            }
            const int ti = p.target_object.empty() ? -1 : data_.theatre.IndexOf(p.target_object);
            r.target_pos = ti >= 0 ? truth_[static_cast<std::size_t>(ti)].pos : p.target_pos;
            r.target = ti >= 0 ? data_.theatre.objects[static_cast<std::size_t>(ti)].name : std::string("an empty field");
            r.part = p.part;
            r.aircraft = p.Aircraft();
            r.lost = p.Missiles() > 0 ? lost : down;
            r.delivered = o.delivered;
            r.near_battery = TouchesPlayer(p);
            r.played = p.played;
            r.low = p.profile == Profile::Low;
            for (const Flight& fl : p.flights) r.helicopters = r.helicopters || fl.role == FlightRole::Helicopter;
            r.target_object = ti;
            observed_.push_back(r);
        }
        const bool first_arms = patterns_.arm_attacks == 0;
        patterns_.arm_attacks += static_cast<int>(o.radars_killed.size() + o.radars_suppressed.size());
        if (first_arms && patterns_.arm_attacks > 0) {
            Log("own_intel", "regiment", "The enemy has fired anti-radiation missiles at our radars: the regiment orders the "
                                         "batteries to the ARM drill (least emission, silence after its signs).", true);
        }
        ++patterns_.packages;
        ++patterns_.by_part[static_cast<int>(p.part)];
        ++patterns_.by_entry[p.entry];
        patterns_.aircraft += p.Aircraft();
        for (const Flight& fl : p.flights) {
            const AircraftType* a = data_.faction.Aircraft(fl.aircraft);
            if (fl.role == FlightRole::Decoy && a != nullptr && !a->drone) ++patterns_.feints;
            if (fl.role == FlightRole::Decoy && a != nullptr && a->drone) ++patterns_.decoy_waves;
        }
        if ((TouchesPlayer(p) || p.played) && p.Missiles() > 0) {
            Log("own_intel", "warning net", std::to_string(p.Missiles()) + " cruise missiles from " + p.entry + " at " +
                                                FormatHour(p.tot_h, data_.theatre.start_hour) + ", " + std::to_string(down) +
                                                " shot down" + (o.delivered ? "" : ", none reached its target") + ".", true);
        } else if (TouchesPlayer(p) || p.played) {
            Log("own_intel", "warning net", std::to_string(p.Aircraft()) + " aircraft from " + p.entry + " at " +
                                                FormatHour(p.tot_h, data_.theatre.start_hour) + " (" + DayPartName(p.part) + "), " +
                                                std::to_string(down) + " shot down" + (o.delivered ? "" : ", no bombs on target") + ".", true);
        }
        RevealFromPatterns();
    }

    void RevealFromPatterns() {
        const int pln_idx = staff_.Holder(StaffRole::Planner);
        if (pln_idx < 0 || patterns_.packages < 4) return;
        const Officer& pln = staff_.officers[static_cast<std::size_t>(pln_idx)];
        OfficerFile& file = files_[pln_idx];
        auto reveal = [&](const std::string& quirk_effect, bool evidence) {
            if (!evidence) return;
            for (const QuirkDef& q : pln.quirks) {
                if (q.Effect(quirk_effect) <= 0.0 || file.revealed.count(q.id) > 0) continue;
                file.revealed.insert(q.id);
                if (file.nickname.empty()) file.nickname = "the " + q.name;
                Log("own_intel", "analysts", "The officer planning the raids, our analysts' \"" + file.nickname + "\": " + q.intel_hint + ".", true);
            }
        };
        const double n = patterns_.packages;
        const char* pref[] = {"pref_night", "pref_dawn", "pref_day", "pref_dusk"};
        for (int k = 0; k < kDayParts; ++k) reveal(pref[k], patterns_.by_part[k] / n >= 0.45);
        int top = 0;
        for (const auto& e : patterns_.by_entry) top = std::max(top, e.second);
        reveal("axis_repeat", top / n >= 0.6);
        reveal("feint", patterns_.feints >= 2);
        reveal("decoys", patterns_.decoy_waves >= 3);
        reveal("massing", patterns_.aircraft / n >= 10.0);
        reveal("drip_feed", patterns_.aircraft / n <= 3.0 && n >= 6);
        reveal("comsec_leak", true);
    }

    void Interrogate(const Package& p, int pows) {
        // A prisoner talks about his squadron and the staff: one more quirk of the planner or the commander, and his
        // squadron leader's name.
        for (int k = 0; k < pows; ++k) {
            const int who = staff_.Holder(rng_.Chance(0.5) ? StaffRole::Planner : StaffRole::Commander);
            if (who < 0) continue;
            const Officer& o = staff_.officers[static_cast<std::size_t>(who)];
            OfficerFile& file = files_[who];
            if (!rng_.Chance(data_.tuning.Get("own_intel.pow_chance"))) continue;
            file.name_known = true;
            for (const QuirkDef& q : o.quirks) {
                if (file.revealed.count(q.id) > 0) continue;
                file.revealed.insert(q.id);
                Log("own_intel", "interrogation", "A prisoner from '" + p.name + "' says the " + StaffRoleName(o.def.role) + " is " +
                                                      o.Title() + ": " + q.intel_hint + ".", true);
                break;
            }
        }
    }

    void OwnIntelDaily() {
        const double k = EnemyKnowledgeOfBattery();
        std::string est = k > 0.6 ? "probably knows" : (k > 0.3 ? "may know" : "is unlikely to know");
        Log("own_intel", "intelligence", "Day " + std::to_string(day_) + ": the enemy " + est + " the battery's position" +
                                            (k > 0.6 ? ": consider moving it." : "."), true);
    }

    // ------------------------------------------------------------------ the player's side repairs and moves

    // Damage is repaired in the field at 1/repair_h of the whole an hour. A wrecked object (function 0) has nothing left
    // to repair: it is out until a replacement comes (a span, a battery from the reserve, new vehicles), repair_h after
    // it was wrecked, and comes back at `repair.restored_function`, a makeshift that the field repair then completes.
    void Repair(double dt) {
        const double restored = data_.tuning.Get("repair.restored_function");
        const double needs_supply = data_.tuning.Get("logistics.repair_needs_supply");
        for (std::size_t i = 0; i < truth_.size(); ++i) {
            const TheatreObject& o = data_.theatre.objects[i];
            ObjectState& st = truth_[i];
            if (o.kind == ObjectKind::Troops && st.supply < needs_supply) continue;  // out of stocks, nothing is mended
            if (st.overrun || (ground_on_ && ground_war_.TieOf(i) >= 0)) continue;  // the enemy's; or its unit's strength (F3.7)
            if (st.function < 1.0 && o.repair_h > 0.0) {
                if (st.function <= 0.0 && st.wrecked_h >= 0.0) {
                    if (now_h_ + dt >= st.wrecked_h + o.repair_h) st.function = restored;
                } else {
                    st.function = std::min(1.0, st.function + dt / o.repair_h);
                }
                if (st.function >= 1.0) st.wrecked_h = -1.0;
            }
            if (o.defence.present && o.defence.missiles > 0 && st.missiles < o.defence.missiles && st.function > 0.3) {
                st.missiles = std::min(o.defence.missiles, st.missiles + static_cast<int>(std::ceil(dt)));
            }
        }
    }

    void Relocate() {
        for (std::size_t i = 0; i < truth_.size(); ++i) {
            const TheatreObject& o = data_.theatre.objects[i];
            ObjectState& st = truth_[i];
            const bool player = static_cast<int>(i) == player_;
            if (player && marching_) continue;
            if (st.overrun) continue;  // the enemy's (F3.7)
            // A unit that moves on orders (F4.2): not while it is on a move; its own hops about where its last order put it,
            // inside the sector it stands in and as far from the line as its orders would keep it.
            const int cls = moves_on_ ? move_class_[i] : -1;
            if (cls >= 0 && st.march != 0) continue;
            const double every = player ? posture_.relocate_every_h : o.relocate_h;
            if (every <= 0.0 || now_h_ - st.last_moved_h < every || st.function <= 0.0) continue;
            const int sector_now = cls >= 0 ? front_line_.SectorOf(st.pos) : -1;
            const double keep_m = cls >= 0 ? std::max(8000.0, move_rules_.classes[static_cast<std::size_t>(cls)].withdraw_within_m) : 8000.0;
            const Vec3 shift = cls >= 0 ? move_anchor_[i] - o.pos : Vec3{};
            // A tied troops object's prepared positions go with its unit (F3.7): about where its unit has put it as about its
            // home; its new place is where the supply lines to it end.
            const int tie = ground_on_ ? ground_war_.TieOf(i) : -1;
            if (!player && !o.prepared.empty()) {
                // To another of its prepared positions, none of them within 8 km of the enemy's front.
                std::vector<Vec3> options;
                for (const Vec3& q0 : o.prepared) {
                    const Vec3 q = tie >= 0 ? q0 + (tie_place_[static_cast<std::size_t>(tie)] - ground_war_.Ties()[static_cast<std::size_t>(tie)].home) : q0 + shift;
                    if (Dist2d(q, st.pos) > 1.0 && BehindFront(q) >= keep_m && (cls < 0 || front_line_.SectorOf(q) == sector_now)) options.push_back(q);
                }
                if (!options.empty()) {
                    st.pos = options[static_cast<std::size_t>(rng_.Index(static_cast<int>(options.size())))];
                    st.last_moved_h = now_h_;
                    if (tie >= 0) ground_air_.supply_reroutes += supply_.MoveEnd(data_, static_cast<int>(i), st.pos, truth_);
                }
                continue;
            }
            const double dist = player ? 5000.0 : o.relocate_m;
            for (int tries = 0; tries < 6; ++tries) {
                const double a = rng_.Uniform(0.0, 6.283185307179586);
                const Vec3 to = st.pos + Vec3{std::cos(a) * dist, std::sin(a) * dist, 0.0};
                if (BehindFront(to) < keep_m) continue;
                if (cls >= 0 && front_line_.SectorOf(to) != sector_now) continue;
                if (player) {
                    MoveBattery(to);
                } else {
                    st.pos = to;
                    st.last_moved_h = now_h_;
                    if (tie >= 0) ground_air_.supply_reroutes += supply_.MoveEnd(data_, static_cast<int>(i), st.pos, truth_);
                }
                break;
            }
        }
    }
};

}  // namespace airwar
}  // namespace simcore
