// simcore/ground_war/session_units.hpp — the ground war in a session: its units at U2 and the front heard and seen from
// afar (track F, F5.2 built as F8.3; docs/fronts/03_GROUND_WAR.md section 3.3, 05_WHAT_THE_PLAYER_SEES.md section 2).
//
// U2. In a session, the regiments at U1 (ground_war.hpp, F3.6) near the battery come down to their battalions and
// companies, each the session's mover (nav::SessionNav::AddMover) of its main equipment type: deployed in line at its
// place across its regiment's run of the line, at its echelon's depth behind the line of contact as the war has it now;
// when the war moves its place, a column on the roads there at the ground war's march speeds (its own marches'), straight
// across country over a short move. Their places follow the war; U2 adds nothing to the fight: they are not in the
// session's air battle (the pilots' picture, the targets of opportunity, the detail levels: nav::Mover::ground_war), and
// the war's strength is never read back from them. What the player's character sees of them is the session's rule for
// every mover (nav::VisualRangeM by day, dusk and night, the weather's visibility the cap): only those within it are listed
// for drawing (SessionNav::Visible), so an enemy's battalion out of sight has no marker anywhere. Ours are drawn by our
// equipment's models, the enemy's by its own (U2Model): never as ours. Out of a session nothing of this is made.
//
// The front from afar. The sector's combat (an axis attacking or counterattacked) is heard as artillery and, at night,
// seen as the glow of fires (fx::EventKind::DistantFire, FireGlow): presentation only, in the session's fx feed, nothing
// in the simulation reads them. Each salvo's sound is the burst's peak level (fx/explosions.hpp) less the air's absorption
// of its thud (fx/acoustics.hpp, ISO 9613-1) at the battery, and carries its retarded time (when it reaches the battery at
// the speed of sound in the day's air); one too faint to hear over a battery position is not pushed, nor a glow farther than
// the night and the weather let it be seen. They tell the player nothing the crew could not hear or see.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/front_line.hpp"
#include "simcore/atmosphere.hpp"
#include "simcore/command/oob.hpp"
#include "simcore/fx/acoustics.hpp"
#include "simcore/fx/events.hpp"
#include "simcore/fx/explosions.hpp"
#include "simcore/ground_war/ground_war.hpp"
#include "simcore/nav/session.hpp"

namespace simcore {
namespace groundwar {

// ------------------------------------------------------------------------------------------------------------ U2's units

// One battalion or company at U2.
struct U2Unit {
    std::string id;        // its mover's id in the session: "<regiment>/<type>/<n>"
    std::string regiment;  // its regiment (GroundWar::Regiments()'s id)
    int reg = -1;          // that regiment's index
    Side side = Side::Ours;
    std::string type;      // its equipment type
    std::string model;     // the basic model it is drawn as (U2Model)
    std::string kind;      // "battalion" or "company"
    int vehicles = 1;      // its vehicles now: its share of the regiment's establishment times the regiment's strength
    Vec3 place;            // where it stands (the theatre's ENU, on the ground)
    Vec3 along;            // a unit vector along the line of contact there: it stands in line along it
};

// The equipment types of our order of battle, sorted (the models the enemy's must never be).
inline std::vector<std::string> OurTypes(const GroundWarData& d) {
    std::vector<std::string> out;
    for (const command::OobUnit& u : d.ours.oob.units) {
        for (const std::pair<std::string, int>& e : u.equipment) out.push_back(e.first);
    }
    std::stable_sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// The basic model a unit of `type` is drawn as (data/scenery/basic_models.json): ours by its type; the enemy's by its type
// unless that type is also ours (the Pact's BMP-1, the rebels' T-55 and BTR-60), then its faction's variant,
// "<type>_<faction>". The enemy is never drawn as ours (test_scenery.cpp checks every faction's types have their model).
inline std::string U2Model(const std::string& type, Side side, const std::string& faction, const std::vector<std::string>& our_types) {
    if (side == Side::Ours) return type;
    return std::binary_search(our_types.begin(), our_types.end(), type) ? type + "_" + faction : type;
}

namespace detail {

constexpr double kFirstEchelonDepthM = 3000.0;   // as ground_war.hpp's: a first-echelon battalion up to 3 km behind the line
constexpr double kSecondEchelonDepthM = 12000.0; // as ground_war.hpp's: a regiment's depth in defence, up to 12 km
constexpr double kBattalionVehicles = 40.0;      // sam3d: one type's vehicles a battalion holds, about (a Pact tank battalion
                                                 // 31-40, a motor rifle battalion's BMPs 30-43, a NATO company team 14-17:
                                                 // fm_100_2_3_1984's and fm_71_2_1977's tables rounded to one figure)
constexpr int kMinVehicles = 6;                  // sam3d: fewer of a type in a regiment are not drawn as a unit of their own
constexpr double kSpreadM = 6000.0;              // sam3d: a regiment's battalions across at most this much of its run
constexpr double kCompanyBelow = 20.0;           // sam3d: under this many vehicles a unit is a company

// A regiment's place on the line now, its first echelon's or the run behind it: the stations it holds (from_m, to_m) at its
// echelon's depth behind the line of contact as the war has it (ours behind the line pushed in, the enemy's behind its
// axis's tip), as ground_war.hpp's AssignStations places it. False for one with no run (a reserve, an echelon not yet
// committed): its own place then.
inline bool RunNow(const GroundWar& w, const Regiment& r, const airwar::LineOfContact& line, double& base_m, double& sign) {
    const Formation& f = w.Formations()[static_cast<std::size_t>(r.formation)];
    if (!(r.to_m > r.from_m) || line.Empty()) return false;
    sign = 1.0;
    base_m = 0.0;
    if (f.unit.side == Side::Ours && f.first_sector >= 0) return true;
    const int a = f.unit.side == Side::Enemy ? f.axis : f.task_axis;
    if (a < 0 || static_cast<std::size_t>(a) >= w.Axes().size()) return false;
    base_m = w.Axes()[static_cast<std::size_t>(a)].depth_m;
    if (f.unit.side == Side::Enemy) sign = -1.0;
    return true;
}

}  // namespace detail

// The battalions and companies of every regiment at U1 within `radius_m` of `at` (the theatre's ENU): each weighted
// equipment type of the regiment's own tree, at least kMinVehicles of it, in battalions of about kBattalionVehicles, each
// drawn with its vehicles now (its share times the regiment's strength fraction), spread across the regiment's run. Our
// units first, then the enemy's, in the war's order of regiments. Nothing for a formation spent or not at U1.
inline std::vector<U2Unit> U2Near(const GroundWar& w, const airwar::LineOfContact& line, const Vec3& at, double radius_m,
                                  const std::vector<std::string>& our_types) {
    std::vector<U2Unit> out;
    if (!w.Built()) return out;
    const GroundWarData& d = w.Data();
    const GroundTables& t = d.tables;
    for (Side side : {Side::Ours, Side::Enemy}) {
        const command::OrderOfBattle& oob = side == Side::Ours ? d.ours.oob : d.enemy.oob;
        for (std::size_t k = 0; k < w.Regiments().size(); ++k) {
            const Regiment& r = w.Regiments()[k];
            const Formation& f = w.Formations()[static_cast<std::size_t>(r.formation)];
            if (f.unit.side != side || f.spent || f.level != UnitLevel::U1) continue;
            double base_m = 0.0, sign = 1.0;
            const bool run = detail::RunNow(w, r, line, base_m, sign);
            const Vec3 centre = run ? line.PointAt(0.5 * (r.from_m + r.to_m)) : r.pos;
            if (!run && r.pos.x == 0.0 && r.pos.y == 0.0) continue;  // never placed
            // Near enough by its run's middle at its depth (a regiment's run reaches no farther than kSpreadM / 2 from it).
            const double back = r.first ? detail::kFirstEchelonDepthM : detail::kSecondEchelonDepthM;
            const double mid = 0.5 * (r.from_m + r.to_m);
            const Vec3 where = run ? centre + line.InwardNormal(mid) * (std::max(base_m, sign > 0.0 ? line.Offset(mid) : base_m) + sign * back) : centre;
            if (std::hypot(where.x - at.x, where.y - at.y) > radius_m + 0.5 * detail::kSpreadM) continue;
            // Its types: the regiment's own tree.
            std::vector<std::pair<std::string, int>> types;
            for (const command::OobUnit& u : oob.units) {
                if (!oob.Under(u.id, r.id)) continue;
                for (const std::pair<std::string, int>& e : u.equipment) {
                    bool known = false;
                    t.WeightOf(e.first, &known);
                    if (!known || e.second <= 0) continue;
                    bool merged = false;
                    for (std::pair<std::string, int>& have : types) {
                        if (have.first == e.first) {
                            have.second += e.second;
                            merged = true;
                        }
                    }
                    if (!merged) types.push_back(e);
                }
            }
            std::vector<std::pair<std::string, int>> units;  // (type, vehicles) for each battalion
            for (const std::pair<std::string, int>& e : types) {
                if (e.second < detail::kMinVehicles) continue;
                const int n = std::max(1, static_cast<int>(std::lround(static_cast<double>(e.second) / detail::kBattalionVehicles)));
                for (int b = 0; b < n; ++b) units.push_back({e.first, e.second});
            }
            if (units.empty()) continue;
            const double fraction = std::min(1.0, std::max(0.0, w.RegimentFraction(k)));
            const double width = run ? std::min(r.to_m - r.from_m, detail::kSpreadM) : detail::kSpreadM;
            const double along0 = run ? mid : line.Empty() ? 0.0 : line.Place(r.pos).along_m;
            int index_of_type = 0;
            for (std::size_t b = 0; b < units.size(); ++b) {
                index_of_type = b > 0 && units[b].first == units[b - 1].first ? index_of_type + 1 : 0;
                int of_type = 0;
                for (const std::pair<std::string, int>& u : units) of_type += u.first == units[b].first ? 1 : 0;
                U2Unit u;
                u.id = r.id + "/" + units[b].first + "/" + std::to_string(index_of_type + 1);
                u.regiment = r.id;
                u.reg = static_cast<int>(k);
                u.side = side;
                u.type = units[b].first;
                u.model = U2Model(u.type, side, d.faction, our_types);
                const double share = static_cast<double>(units[b].second) / static_cast<double>(of_type);
                u.vehicles = std::max(1, static_cast<int>(std::lround(share * fraction)));
                u.kind = static_cast<double>(u.vehicles) < detail::kCompanyBelow ? "company" : "battalion";
                const double along = along0 + ((static_cast<double>(b) + 0.5) / static_cast<double>(units.size()) - 0.5) * width;
                const Vec3 n = line.Empty() ? Vec3{0.0, 1.0, 0.0} : line.InwardNormal(run ? along : along0);
                u.along = Vec3{n.y, -n.x, 0.0};
                if (run) {
                    const double base = sign > 0.0 ? std::max(base_m, line.Offset(along)) : base_m;
                    u.place = line.PointAt(along) + n * (base + sign * back);
                } else {
                    u.place = r.pos + u.along * (along - along0);
                }
                u.place.z = 0.0;
                if (std::hypot(u.place.x - at.x, u.place.y - at.y) <= radius_m) out.push_back(std::move(u));
            }
        }
    }
    return out;
}

// U2 in a session: its units as the session's movers. Made only by a session that has the war's ground war and roads
// (campaign/mission_session.hpp); each Apply takes the units near the battery now, in the session's frame.
class SessionUnits {
public:
    // The ground war's column (its own marches' speeds: the march table's day and cross-country middles, its night factor).
    void Start(const GroundWarData& d) {
        const GroundTables& t = d.tables;
        column_ = nav::Mobility();
        column_.id = "ground_war_column";
        column_.name = "a battalion's column (the ground war's, U2)";
        const double road = t.road_day_kmh.Mid() / 3.6;
        column_.road_mps = {road, road, road};
        column_.cross_country_mps = t.cross_country_kmh.Mid() / 3.6;
        column_.night_factor = t.road_day_kmh.Mid() > 0.0 ? std::min(1.0, t.road_night_kmh.Mid() / t.road_day_kmh.Mid()) : 1.0;
        our_types_ = OurTypes(d);
        faction_ = d.faction;
        started_ = true;
    }
    bool Started() const { return started_; }
    const std::vector<std::string>& OurTypeList() const { return our_types_; }

    // The units near the battery now (their places in the session's frame): each new one deployed at its place, each whose
    // place has moved more than kMoveM marching there (`night`: at the column's night speeds), each no longer near out of
    // the session's sight (M0: not listed, not stepped finely) until it is near again.
    void Apply(const std::vector<U2Unit>& now, nav::SessionNav& nav, double t_s, bool night = false) {
        for (Held& h : held_) h.nearby = false;
        for (const U2Unit& u : now) {
            Held* h = FindHeld(u.id);
            nav::Mover* m = h != nullptr ? nav.GetMovers().Find(u.id) : nullptr;
            if (m == nullptr) {
                nav::Mover nm;
                nm.id = u.id;
                nm.kind = "column";
                nm.mobility = column_.id;
                nm.vehicles = u.vehicles;
                nm.spacing_m = kSpacingM;
                nm.enemy = u.side == Side::Enemy;
                nm.ground_war = true;
                const double al = std::hypot(u.along.x, u.along.y);
                Deploy(nm, u.place, al > 1e-9 ? MapPoint{u.along.x / al, u.along.y / al} : MapPoint{1.0, 0.0}, t_s);
                nav.AddMover(nm, u.model);
                held_.push_back({u.id, u.side, u.place, true});
                ++made_;
                continue;
            }
            h->nearby = true;
            if (m->level == nav::MoverLevel::M0) m->level = nav::MoverLevel::M1;  // near again: the session's to see
            m->vehicles = u.vehicles;
            if (std::hypot(u.place.x - h->place.x, u.place.y - h->place.y) <= kMoveM) continue;
            h->place = u.place;
            March(*m, nav, u.place, t_s, night);
        }
        for (Held& h : held_) {
            if (h.nearby) continue;
            if (nav::Mover* m = nav.GetMovers().Find(h.id)) m->level = nav::MoverLevel::M0;
        }
    }

    // The units held (every one made since the session's start) and how many are near the battery now.
    std::size_t Made() const { return made_; }
    std::size_t Near() const {
        std::size_t n = 0;
        for (const Held& h : held_) n += h.nearby ? 1 : 0;
        return n;
    }
    std::size_t Marches() const { return marches_; }
    std::size_t RoadSearches() const { return searches_; }
    struct Held {
        std::string id;
        Side side = Side::Ours;
        Vec3 place;     // where it was last sent (the session's frame)
        bool nearby = false;
    };
    const std::vector<Held>& All() const { return held_; }

    static constexpr double kSpacingM = 50.0;  // between vehicles, as a column's 25-50 m (fm_100_2_1_1984 p. 5-3) and in line
    static constexpr double kMoveM = 1000.0;   // sam3d: a place moved this far or less is kept (no march for the line's creep)
    static constexpr double kRoadsFromM = 3000.0;  // sam3d: a march shorter than this goes straight across country

private:
    Held* FindHeld(const std::string& id) {
        for (Held& h : held_) {
            if (h.id == id) return &h;
        }
        return nullptr;
    }
    // Deployed in line at `at`, its vehicles kSpacingM apart along `dir`: arrived there.
    void Deploy(nav::Mover& m, const Vec3& at, const MapPoint& dir, double t_s) const {
        const double len = std::max(10.0, m.Length());
        nav::Route r;
        r.found = true;
        r.path = {MapPoint{at.x - dir.east_m * 0.5 * len, at.y - dir.north_m * 0.5 * len}, MapPoint{at.x + dir.east_m * 0.5 * len, at.y + dir.north_m * 0.5 * len}};
        nav::RouteStretch s;
        s.kind = "off_road";
        s.length_m = len;
        s.time_s = column_.cross_country_mps > 0.0 ? len / column_.cross_country_mps : 0.0;
        r.stretches.push_back(s);
        r.length_m = len;
        r.time_s = s.time_s;
        m.route = r;
        m.arc_m = len;
        m.moved_s = r.time_s;
        m.depart_s = t_s;
        m.arrived_s = t_s;
        m.halt_until_s = -1.0;
        m.state = nav::MoverState::Arrived;
    }
    // From where its head is to `to`: on the roads for the ground war's column, straight across country when short or when
    // the roads have no way.
    void March(nav::Mover& m, const nav::SessionNav& nav, const Vec3& to, double t_s, bool night) {
        const MapPoint from = nav::PointAlong(m.route, m.arc_m);
        const MapPoint dest{to.x, to.y};
        const double straight = std::hypot(dest.east_m - from.east_m, dest.north_m - from.north_m);
        nav::Route r;
        if (straight >= kRoadsFromM && nav.Ready()) {
            nav::RouteRequest q;
            q.from = from;
            q.to = dest;
            q.mobility = &column_;
            q.night = night;
            q.swim = false;
            r = nav.Graph().Find(q, nav.Truth());
            ++searches_;
        }
        if (!r.found || r.path.size() < 2) {
            r = nav::Route();
            r.found = true;
            r.path = {from, dest};
            nav::RouteStretch s;
            s.kind = "off_road";
            s.length_m = std::max(1.0, straight);
            const double v = column_.cross_country_mps * (night ? column_.night_factor : 1.0);
            s.time_s = v > 0.0 ? s.length_m / v : 0.0;
            r.stretches.push_back(s);
            r.length_m = s.length_m;
            r.time_s = s.time_s;
        }
        m.route = r;
        m.arc_m = 0.0;
        m.moved_s = 0.0;
        m.depart_s = t_s;
        m.arrived_s = -1.0;
        m.halt_until_s = -1.0;
        m.state = nav::MoverState::Waiting;
        ++marches_;
    }

    nav::Mobility column_;
    std::vector<std::string> our_types_;
    std::string faction_;
    std::vector<Held> held_;
    std::size_t made_ = 0;
    std::size_t marches_ = 0;
    std::size_t searches_ = 0;
    bool started_ = false;
};

// ------------------------------------------------------------------------------------------------- the front from afar

// Where the sector's combat is (the theatre's ENU): each axis attacking or counterattacked, at its tip on the line of
// contact, along its breakthrough frontage.
struct FrontContact {
    std::string axis;
    Vec3 at;                    // the tip
    Vec3 along;                 // a unit vector along the line there
    double half_width_m = 0.0;
};

inline std::vector<FrontContact> FrontContacts(const GroundWar& w, const airwar::LineOfContact& line) {
    std::vector<FrontContact> out;
    if (!w.Built() || line.Empty()) return out;
    for (const Axis& x : w.Axes()) {
        if (x.status != AxisStatus::Attacking && x.status != AxisStatus::Counterattacked) continue;
        FrontContact c;
        c.axis = x.id;
        const Vec3 n = line.InwardNormal(x.along_m);
        c.at = line.PointAt(x.along_m) + n * x.depth_m;
        c.at.z = 0.0;
        c.along = Vec3{n.y, -n.x, 0.0};
        c.half_width_m = x.half_width_m;
        out.push_back(c);
    }
    return out;
}

// The air between the front and the listener (the session's weather at the ground).
struct FrontAir {
    double temperature_k = 288.15;
    double rh_pct = 50.0;
    double pressure_pa = isa::kSeaLevelPressure;
};

struct FrontFxValues {
    double every_s = 20.0;         // sam3d: one salvo heard from each fight this often
    int rounds = 6;                // sam3d: its rounds, spread over every_s (a front's artillery heard as a rumble)
    double shell_kg = 6.0;         // a 152-155 mm shell's burst, TNT equivalent (sam3d: 2S3's and M109's HE about 6-7 kg)
    double thud_hz = 125.0;        // sam3d: the far burst's thud, the band its absorption is read at
    double heard_db = 70.0;        // sam3d: a peak this loud carries over a battery position's generators
    double hear_max_m = 50000.0;   // sam3d: and none farther (the war's other sectors are another battery's)
    double glow_every_s = 300.0;   // sam3d: a new fire glowing on each fight this often at night
    double glow_lasts_s = 900.0;   // sam3d: and burning so long
    double glow_seen_m = 30000.0;  // sam3d: a fire's glow on the horizon seen this far on a dark night, the visibility the cap
};

namespace detail {

inline std::uint64_t Mix(std::uint64_t x) {  // SplitMix64's finaliser
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}
inline double Unit01(std::uint64_t seed, std::uint64_t a, std::uint64_t b) {
    return static_cast<double>(Mix(seed ^ Mix(a * 0x100000001B3ull + b)) >> 11) * (1.0 / 9007199254740992.0);
}
inline std::uint64_t Hash(const std::string& s) {
    std::uint64_t h = 1469598103934665603ull;
    for (char c : s) h = (h ^ static_cast<std::uint64_t>(static_cast<unsigned char>(c))) * 1099511628211ull;
    return h;
}

}  // namespace detail

// The salvo each fight sends at `t_s` (its `step`, every_s apart) as heard at `listener` (the contacts and the listener
// in the session's frame): a DistantFire event at a point along the fight's frontage with its rounds, its level at the
// listener (the burst's peak less the air's absorption of its thud) and its retarded time (end_t_s: when it reaches the
// listener at the speed of sound). None too faint or too far.
inline std::vector<fx::Event> DistantFire(const std::vector<FrontContact>& contacts, const Vec3& listener, double t_s, std::uint64_t step,
                                          const FrontAir& air, const FrontFxValues& v, std::uint64_t seed) {
    std::vector<fx::Event> out;
    const double c_mps = fx::SpeedOfSound(air.temperature_k);
    for (const FrontContact& c : contacts) {
        const std::uint64_t key = detail::Hash(c.axis);
        const double u = 2.0 * detail::Unit01(seed, key, step) - 1.0;
        const Vec3 at = c.at + c.along * (u * c.half_width_m);
        const double d = std::hypot(at.x - listener.x, at.y - listener.y);
        if (d > v.hear_max_m) continue;
        const double level = fx::PeakSplDb(v.shell_kg, d, air.pressure_pa, fx::kSurfaceBurstFactor) -
                             fx::AirAbsorptionDbPerM(v.thud_hz, air.temperature_k, air.rh_pct, air.pressure_pa) * d;
        if (level < v.heard_db) continue;
        fx::Event e;
        e.kind = fx::EventKind::DistantFire;
        e.t_s = t_s;
        e.position = Vec3{at.x, at.y, 0.0};
        e.aim = e.position;
        e.end_t_s = t_s + d / c_mps;
        e.span_s = v.every_s;
        e.number = v.rounds;
        e.charge_kg = v.shell_kg;
        e.level_db = level;
        e.source = c.axis;
        e.weapon = "artillery";
        out.push_back(e);
    }
    return out;
}

// A fire glowing on each fight at night (`dark`), seen from `listener` within glow_seen_m and the weather's visibility:
// a FireGlow event from `t_s` to its end_t_s at a point along the fight's frontage. Light needs no retarded time.
inline std::vector<fx::Event> FireGlows(const std::vector<FrontContact>& contacts, const Vec3& listener, double t_s, std::uint64_t step, bool dark,
                                        double visibility_m, const FrontFxValues& v, std::uint64_t seed) {
    std::vector<fx::Event> out;
    if (!dark) return out;
    const double seen = std::min(v.glow_seen_m, visibility_m);
    for (const FrontContact& c : contacts) {
        const std::uint64_t key = detail::Hash(c.axis) ^ 0x61u;
        const double u = 2.0 * detail::Unit01(seed, key, step) - 1.0;
        const Vec3 at = c.at + c.along * (u * c.half_width_m);
        const double d = std::hypot(at.x - listener.x, at.y - listener.y);
        if (d > seen) continue;
        fx::Event e;
        e.kind = fx::EventKind::FireGlow;
        e.t_s = t_s;
        e.end_t_s = t_s + v.glow_lasts_s;
        e.position = Vec3{at.x, at.y, 0.0};
        e.aim = e.position;
        e.number = 1;
        e.fire = true;
        e.source = c.axis;
        out.push_back(e);
    }
    return out;
}

}  // namespace groundwar
}  // namespace simcore
