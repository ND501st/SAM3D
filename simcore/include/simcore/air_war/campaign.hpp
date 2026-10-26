// simcore/air_war/campaign.hpp — the enemy air command's war, hour by hour (docs/air_war/).
//
// AirCampaign owns the enemy (its staff, squadrons, ARMs, politics, intelligence picture) and the player's side as it
// really is (every object's place, state and rounds; the player's battery and its posture). Advance() runs the war:
//   * every planning cycle the staff plans an air tasking order (planner.hpp);
//   * the collection disciplines feed the picture: satellite passes, agents, ELINT on what radiates, radio intercept
//     of the battery's traffic (and of its dummy traffic), reconnaissance flights; reports arrive late;
//   * every package is flown when its time comes: resolved abstractly (resolve.hpp), or, when it comes through the
//     player's sector and the pacing allows, handed to the game as the player's mission (raid_scenario.hpp builds the
//     scenario, mission_bridge.hpp brings the outcome back to CompletePlayerMission);
//   * what the enemy believes happened (pilots' claims, inflated; launch sightings; ELINT fixes) goes into its picture
//     and into its officers: what they remember of each tactic, their stress, reputation and grudges;
//   * each day: politics (pressure, support, events), replacements and ARM resupply, careers (relief, promotion,
//     replacement), and the player's side's intelligence summary of the enemy (what the player can know);
//   * every hour: repairs, mobile units moving, reports arriving, the picture ageing.
// Deterministic for a seed. The journal keeps the enemy's mind (hidden until the debrief) and what the player's side
// learns (visible).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/intel.hpp"
#include "simcore/air_war/logistics.hpp"
#include "simcore/air_war/officers.hpp"
#include "simcore/air_war/planner.hpp"
#include "simcore/air_war/resolve.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/air_war/types.hpp"

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
};

// One thing the player did to the campaign, in order: what a save keeps (the campaign is deterministic for its seed,
// so replaying the same actions in the same order rebuilds it exactly; save.hpp).
struct CampaignAction {
    enum class Kind { Advance, Relocate, Skip, Complete };
    Kind kind = Kind::Advance;
    BatteryPosture posture;  // the battery's posture when it was done
    double until_h = 0.0;    // Advance
    Vec3 to;                 // Relocate
    PackageOutcome outcome;  // Complete
    double end_h = 0.0;      // Complete
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

class AirCampaign {
public:
    bool Init(const AirWarData& data, unsigned seed, std::string* error = nullptr) {
        data_ = data;
        seed_ = seed;
        actions_.clear();
        rng_ = Rng(static_cast<std::uint64_t>(seed) * 0x9E3779B97F4A7C15ull + 1234567u);
        now_h_ = 0.0;
        const FrontDef* front = data_.theatre.FrontFor(data_.faction.front);
        if (front == nullptr) {
            if (error != nullptr) *error = "the faction's front is not in the theatre";
            return false;
        }
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
        weather_.clear();
        const double clear = data_.tuning.Get("weather.clear_share");
        const double poor = data_.tuning.Get("weather.poor_share");
        for (int day = 0; day < 120; ++day) {
            const double u = rng_.Uniform();
            weather_.push_back(u < clear ? Weather::Clear : (u < 1.0 - poor ? Weather::Cloudy : Weather::Poor));
        }
        PriorIntel();
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
            const double step_end = std::min(until_h, std::floor(now_h_ + 1.0 + 1e-9));
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
        Log("player_mission", "result", "Mission against '" + p.name + "' flown: " +
                                            (p.Missiles() > 0 ? std::to_string(outcome.missiles_downed.size()) + " of " + std::to_string(p.Missiles()) + " cruise missiles"
                                                              : std::to_string(outcome.Lost()) + " of " + std::to_string(p.Aircraft())) +
                                            " shot down in the battery's sector.", true);
        ApplyOutcome(p, outcome);
        // The rest of the raid, without the player's battery: every part of every route but what the scenario flew
        // (inside its circle while it ran, where the other defences took their chances: mission_bridge.hpp).
        ResolveContext rc = Context();
        rc.skip_defence = data_.theatre.objects[static_cast<std::size_t>(player_)].id;
        rc.played_centre = truth_[static_cast<std::size_t>(player_)].pos;
        rc.played_radius_m = data_.tuning.Get("scenario.clip_m");
        rc.played_from_h = now_h_;
        rc.played_to_h = end_h;
        Resolver r;
        PackageOutcome rest = r.Resolve(p, rc);
        if (!rest.losses.empty() || rest.delivered || !rest.recce_seen.empty()) ApplyOutcome(p, rest, false);
        ++player_missions_;
        last_player_mission_h_ = now_h_;
        pending_ = -1;
    }

    // The player declined the mission: it is resolved abstractly like any other.
    void SkipPlayerMission() {
        if (pending_ < 0) return;
        Record(CampaignAction::Kind::Skip);
        Package& p = packages_[static_cast<std::size_t>(pending_)];
        pending_ = -1;
        ResolveAbstract(p);
    }

    // ------------------------------------------------------------------ the player's side

    BatteryPosture& Posture() { return posture_; }
    const BatteryPosture& Posture() const { return posture_; }

    // Can the battery move there? Within `max_m` of where it is, and no closer than 8 km to the enemy's front.
    bool CanRelocateBattery(const Vec3& to, std::string* why = nullptr, double max_m = 30000.0) const {
        const Vec3 here = truth_[static_cast<std::size_t>(player_)].pos;
        if (Dist2d(here, to) > max_m) {
            if (why != nullptr) *why = "too far for one move (" + std::to_string(static_cast<int>(max_m / 1000.0)) + " km at most)";
            return false;
        }
        if (DistanceToFront(to) < 8000.0) {
            if (why != nullptr) *why = "too close to the front";
            return false;
        }
        return true;
    }

    // The player moved the battery (and its dummies with it).
    void RelocateBattery(const Vec3& to) {
        Record(CampaignAction::Kind::Relocate).to = to;
        MoveBattery(to);
    }

    // The player's actions so far, in order (save.hpp), and the seed the campaign started from.
    const std::vector<CampaignAction>& Actions() const { return actions_; }
    unsigned Seed() const { return seed_; }

    // Rebuild a campaign from its actions (after Init with the same data and seed). False if they don't fit it.
    bool Replay(const std::vector<CampaignAction>& actions, std::string* error = nullptr) {
        for (std::size_t i = 0; i < actions.size(); ++i) {
            const CampaignAction& a = actions[i];
            posture_ = a.posture;
            switch (a.kind) {
                case CampaignAction::Kind::Advance: Advance(a.until_h); break;
                case CampaignAction::Kind::Relocate: RelocateBattery(a.to); break;
                case CampaignAction::Kind::Skip:
                case CampaignAction::Kind::Complete:
                    if (pending_ < 0) {
                        if (error != nullptr) *error = "action " + std::to_string(i) + ": no raid was waiting (the save doesn't fit the data)";
                        return false;
                    }
                    if (a.kind == CampaignAction::Kind::Skip) {
                        SkipPlayerMission();
                    } else {
                        CompletePlayerMission(a.outcome, a.end_h);
                    }
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
    bool ok_ = false;
    double now_h_ = 0.0;
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
        // Packages whose first aircraft takes off in this step.
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
        const double dt = t1 - t0;
        intel_.Decay(dt, data_.tuning);
        Repair(dt);
        Relocate();
        {
            // Our supply lines (logistics.hpp): what our side's people on them would report goes in the journal.
            std::vector<std::string> events;
            supply_.Step(data_, truth_, dt, DayPartAt(data_.theatre.start_hour + t0) != DayPart::Day, &events);
            for (const std::string& ev : events) Log("logistics", "our side", ev, false);
        }
        Deliver();
        now_h_ = t1;
        return false;
    }

    bool Pacing() const {
        const TuningTable& t = data_.tuning;
        if (truth_[static_cast<std::size_t>(player_)].function <= 0.3) return false;  // the battery is being repaired
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
        Planner planner;
        std::vector<Package> plan = planner.Plan(pc);
        for (Package& p : plan) {
            packages_.push_back(p);
            const int idx = static_cast<int>(packages_.size()) - 1;
            if (TouchesPlayer(p)) player_candidates_.insert(idx);
        }
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

    // Close support and helicopter raids against troops are talked onto the target by the own troops' forward air
    // controllers (mai_tactics_vvs: control passes to controllers with the ground troops), so they aim at where the
    // troops really are, not at the morning's picture.
    void AimOnCall(Package& p) {
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
        Resolver r;
        PackageOutcome o = r.Resolve(p, rc);
        ApplyOutcome(p, o);
    }

    // ------------------------------------------------------------------ what happened, and what they believe happened

    // `observe`: our side's record of the raid (once per raid: not again for the rest of a played one).
    void ApplyOutcome(Package& p, const PackageOutcome& o, bool observe = true) {
        const FactionDef& f = data_.faction;
        const TuningTable& tune = data_.tuning;
        p.resolved = true;
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
        for (int k = 0; k < o.damaged; ++k) {
            if (p.flights.empty()) break;
            const Flight& fl = p.flights[static_cast<std::size_t>(rng_.Index(static_cast<int>(p.flights.size())))];
            for (SquadronState& s : squadrons_) {
                if (s.id == fl.squadron) s.damaged_until.push_back(now_h_ + f.logistics.damaged_return_h);
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
        const double t = std::max(now_h_, p.tot_h);
        const Officer* io = staff_.HolderPtr(StaffRole::Intelligence);
        const Officer* cdr = staff_.HolderPtr(StaffRole::Commander);
        const double skeptic = 0.5 * ((io != nullptr ? io->def.traits.skepticism : 0.5) + (cdr != nullptr ? cdr->def.traits.skepticism : 0.5));
        int survivors = 0;
        for (const Flight& fl : p.flights) survivors += fl.count - fl.lost;
        const double pirep_h = t + in.pirep_delay_h;
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

    double DistanceToFront(const Vec3& p) const {
        const FrontDef* f = data_.theatre.FrontFor(data_.faction.front);
        return f == nullptr ? 1e18 : DistanceToLine(f->line, p);
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
        // Radars that radiate in their routine (the player's battery by its posture).
        if (in.elint) {
            for (std::size_t i = 0; i < data_.theatre.objects.size(); ++i) {
                const TheatreObject& o = data_.theatre.objects[i];
                if (!o.emitter || truth_[i].function <= 0.05 || o.decoy) continue;
                const double share = static_cast<int>(i) == player_ ? posture_.emission : o.emission;
                if (share <= 0.0) continue;
                // Bursts: a share of the hour, in a few pieces; the collector catches some.
                if (!rng_.Chance(std::min(1.0, 0.3 + share))) continue;
                Elint(o.id, share * 3600.0 * dt * 0.25, t0 + 0.5 * dt);
            }
        }
        // Radio intercept of the battery's net, and of its dummy traffic.
        if (in.comint) {
            const double talk = (1.0 - posture_.radio_discipline) * dt;
            if (rng_.Chance(Clamp01(talk))) {
                IntelReport r = ReportAbout(player_, IntelSource::Comint, t0, t0 + in.comint_delay_h, in.comint_sigma_m);
                r.exists = 0.6;
                r.label = "SAM battery (radio intercept)";
                intel_.Queue(r);
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

    void ObserveRaid(const Package& p, const PackageOutcome& o, int lost) {
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
            r.lost = lost;
            r.delivered = o.delivered;
            r.near_battery = TouchesPlayer(p);
            r.played = p.played;
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
                                                FormatHour(p.tot_h, data_.theatre.start_hour) + ", " + std::to_string(o.missiles_downed.size()) +
                                                " shot down" + (o.delivered ? "" : ", none reached its target") + ".", true);
        } else if (TouchesPlayer(p) || p.played) {
            Log("own_intel", "warning net", std::to_string(p.Aircraft()) + " aircraft from " + p.entry + " at " +
                                                FormatHour(p.tot_h, data_.theatre.start_hour) + " (" + DayPartName(p.part) + "), " +
                                                std::to_string(lost) + " shot down" + (o.delivered ? "" : ", no bombs on target") + ".", true);
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
            const double every = player ? posture_.relocate_every_h : o.relocate_h;
            if (every <= 0.0 || now_h_ - st.last_moved_h < every || st.function <= 0.0) continue;
            if (!player && !o.prepared.empty()) {
                // To another of its prepared positions, none of them within 8 km of the enemy's front.
                std::vector<Vec3> options;
                for (const Vec3& q : o.prepared) {
                    if (Dist2d(q, st.pos) > 1.0 && DistanceToFront(q) >= 8000.0) options.push_back(q);
                }
                if (!options.empty()) {
                    st.pos = options[static_cast<std::size_t>(rng_.Index(static_cast<int>(options.size())))];
                    st.last_moved_h = now_h_;
                }
                continue;
            }
            const double dist = player ? 5000.0 : o.relocate_m;
            for (int tries = 0; tries < 6; ++tries) {
                const double a = rng_.Uniform(0.0, 6.283185307179586);
                const Vec3 to = st.pos + Vec3{std::cos(a) * dist, std::sin(a) * dist, 0.0};
                if (DistanceToFront(to) < 8000.0) continue;
                if (player) {
                    MoveBattery(to);
                } else {
                    st.pos = to;
                    st.last_moved_h = now_h_;
                }
                break;
            }
        }
    }
};

}  // namespace airwar
}  // namespace simcore
