// air_combat_bench: the live air fight (simcore/air_war/air_fight_session.hpp) in a small world of its own, B8's fight
// (mode 0) beside roadmap S3's (mode 1), and the far fight (A1) on and off (docs/air_battle/07_AIR_TO_AIR.md section 9).
//
// Usage: air_combat_bench [data_dir] [seeds]
//
// The world: aircraft that turn at their turn rate toward the point the fight steers them at, at the speed it asks (or
// their cruise), climbing or diving at 50 m/s; a pilot's eyes see every aircraft within 9 km and recognise its type within
// 5 km; his warning receiver hears a lock (a radar of the other side locked on him), a launch (a radar round on its way to
// him); he sees a launch within 10 km. Strikers carry their type's flares and chaff of 1985 (countermeasures.json) and
// dispense by the fight's call; their decoys work as the reaction table says (flare_decoy, chaff_factor). No ground, no
// weather, no sun: the figures are the model's in this one geometry, not a claim about a battle.
//
// Each case is flown `seeds` times from the same start with different seeds; the table sums them:
//   kills A / B   aircraft each side shot down          shots   missiles and gun bursts resolved
//   drags         drags on a warning                     extends  extensions after a slashing pass
//   setups        beam or slash set-ups begun            disp     strikers' dispenses against a shot
//   energy        flown rounds out of energy             notch    radar rounds that ended in the beam against the ground
//   thinks        thoughts (far: of them at A1)          senses   looks by a fighter's own sensors
//   ms            wall time of the fight's steps
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "simcore/air_war/air_combat_data.hpp"
#include "simcore/air_war/air_fight_session.hpp"
#include "simcore/reactions.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

struct Plane {
    Aircraft a;
    std::string id, type, faction, side;
    double cruise_mps = 230.0;
    Vec3 route_dir;
    bool overriding = false;
    PilotOverride ov;
    bool gone = false;
    double flare_s = -1e9, chaff_s = -1e9;
    int flares = 0, chaff = 0;
};

struct Case {
    std::string name;
    struct Member {
        std::string type, faction, role, controller;
        Vec3 at, vel;
        double cruise_mps = 230.0;
    };
    std::vector<Member> members;
    std::vector<AirFightControllerDef> controllers;
    bool far_ = false;       // every fighter far from our ground units
    double until_s = 600.0;
};

struct Tally {
    int lost_a = 0, lost_b = 0, shots = 0;
    AirFightSession::Counts c;
    double ms = 0.0;
    void Add(const Tally& o) {
        lost_a += o.lost_a;
        lost_b += o.lost_b;
        shots += o.shots;
        c.thinks += o.c.thinks;
        c.thinks_far += o.c.thinks_far;
        c.senses += o.c.senses;
        c.drags += o.c.drags;
        c.extends += o.c.extends;
        c.setups += o.c.setups;
        c.dispenses += o.c.dispenses;
        c.out_of_energy += o.c.out_of_energy;
        c.notched += o.c.notched;
        ms += o.ms;
    }
};

class World {
public:
    World(const Case& k, const ReactionTable& rt) : rt_(&rt) {
        for (std::size_t i = 0; i < k.members.size(); ++i) {
            const Case::Member& m = k.members[i];
            Plane p;
            p.id = m.faction + "_" + std::to_string(i);
            p.type = m.type;
            p.faction = m.faction;
            p.side = m.faction == "ours" ? "ours" : "theirs";
            p.a.position = m.at;
            p.a.velocity = m.vel;
            p.a.max_turn_rate_rad_s = DegToRad(12.0);
            p.a.max_load_g = 7.0;
            p.a.max_speed_mps = 1.6 * m.cruise_mps;
            p.a.rcs_m2 = 5.0;
            p.cruise_mps = m.cruise_mps;
            p.route_dir = m.vel * (1.0 / std::max(1.0, m.vel.Length()));
            const Loadout lo = rt.LoadoutOf(m.type, 1985);
            p.flares = lo.flares;
            p.chaff = lo.chaff;
            planes_.push_back(p);
        }
        far_ = k.far_;
        pictures_.resize(planes_.size());
        locked_by_.assign(planes_.size(), -1);
    }
    const std::vector<Plane>& Planes() const { return planes_; }

    AirFightWorld Hooks() {
        AirFightWorld w;
        w.air = [this](int t) -> const Aircraft* { return Valid(t) && !planes_[static_cast<std::size_t>(t)].gone ? &planes_[static_cast<std::size_t>(t)].a : nullptr; };
        w.picture = [this](int t) -> const std::vector<Sighting>* { return Valid(t) ? &pictures_[static_cast<std::size_t>(t)] : nullptr; };
        w.altitude = [](const Vec3& p) { return p.z; };
        w.agl = [](const Vec3& p) { return p.z; };
        w.steer = [this](int t, const PilotOverride& ov) {
            Plane& p = planes_[static_cast<std::size_t>(t)];
            p.overriding = true;
            p.ov = ov;
        };
        w.release = [this](int t) { planes_[static_cast<std::size_t>(t)].overriding = false; };
        w.destroy = [this](int t, const std::string&) {
            planes_[static_cast<std::size_t>(t)].gone = true;
            return true;
        };
        w.radar = [this](int t, const std::string&, int state, int locked) {
            for (int& by : locked_by_) {
                if (by == t) by = -1;
            }
            if (state == 2 && Valid(locked)) locked_by_[static_cast<std::size_t>(locked)] = t;
        };
        w.fx_launch = [this](int shooter, int target, const AamDef& aam, int, double t, double) {
            launches_.push_back({shooter, target, IsSemiActive(aam.guidance), t});
        };
        w.fx_end = [this](int shooter, int target, const AamDef&, int, double, bool, bool) {
            for (Launch& l : launches_) {
                if (l.shooter == shooter && l.target == target) l.done = true;
            }
        };
        w.dispense = [this](int t, bool infrared, double at) {
            Plane& p = planes_[static_cast<std::size_t>(t)];
            if (infrared && p.flares > 0) {
                p.flares -= std::min(p.flares, static_cast<int>(rt_->flare_salvo));
                p.flare_s = at;
                return true;
            }
            if (!infrared && p.chaff > 0) {
                p.chaff -= std::min(p.chaff, static_cast<int>(rt_->chaff_salvo));
                p.chaff_s = at;
                return true;
            }
            return false;
        };
        w.countermeasure = [this](int t, bool infrared, double at) {
            const Plane& p = planes_[static_cast<std::size_t>(t)];
            if (infrared) return at - p.flare_s <= rt_->flare_window_s ? rt_->flare_decoy : 0.0;
            return at - p.chaff_s <= rt_->chaff_window_s ? 1.0 - rt_->chaff_factor : 0.0;
        };
        w.leader = [this](int t) {  // pairs: the even one leads
            const int lead = t - (t % 2);
            return lead != t && Valid(lead) && planes_[static_cast<std::size_t>(lead)].faction == planes_[static_cast<std::size_t>(t)].faction ? lead : -1;
        };
        w.far_from_ours = [this](int) { return far_; };
        return w;
    }

    void Move(double t, double dt) {
        for (Plane& p : planes_) {
            if (p.gone) continue;
            Aircraft& a = p.a;
            Vec3 want = p.route_dir;
            double speed = p.cruise_mps;
            double climb_to = a.position.z;
            if (p.overriding && t > p.ov.until_s) p.overriding = false;
            if (p.overriding) {
                Vec3 d = p.ov.point - a.position;
                climb_to = p.ov.point.z;
                d.z = 0.0;
                if (d.Length() > 1.0) want = d * (1.0 / d.Length());
                if (p.ov.speed_mps > 0.0) speed = std::min(p.ov.speed_mps, a.max_speed_mps);
            }
            Vec3 h{a.velocity.x, a.velocity.y, 0.0};
            const double v = std::max(50.0, h.Length());
            const double hd = std::atan2(h.x, h.y), wd = std::atan2(want.x, want.y);
            double turn = wd - hd;
            while (turn > 3.14159265358979) turn -= 6.28318530717959;
            while (turn < -3.14159265358979) turn += 6.28318530717959;
            const double step = a.max_turn_rate_rad_s * dt;
            const double nh = hd + std::max(-step, std::min(step, turn));
            const double nv = v + std::max(-10.0 * dt, std::min(10.0 * dt, speed - v));
            const double vz = std::max(-50.0, std::min(50.0, (climb_to - a.position.z) * 0.2));
            a.velocity = Vec3{nv * std::sin(nh), nv * std::cos(nh), vz};
            a.load_g = std::fabs(turn) > step ? std::min(a.max_load_g, 1.0 + nv * a.max_turn_rate_rad_s / 9.81) : 1.0;
            a.position += a.velocity * dt;
        }
    }

    void See(double t) {
        for (std::size_t i = 0; i < planes_.size(); ++i) {
            std::vector<Sighting>& pic = pictures_[i];
            pic.erase(std::remove_if(pic.begin(), pic.end(), [&](const Sighting& s) { return s.kind != SightKind::LaunchSign || t - s.time_s > 5.0; }),
                      pic.end());
            const Plane& me = planes_[i];
            if (me.gone) continue;
            for (std::size_t j = 0; j < planes_.size(); ++j) {
                const Plane& o = planes_[j];
                if (j == i || o.gone) continue;
                const double r = (o.a.position - me.a.position).Length();
                if (r < 9000.0) {
                    Sighting s;
                    s.kind = SightKind::Aircraft;
                    s.believed_enu = o.a.position;
                    s.range_est_m = r;
                    s.time_s = t;
                    s.recognised = r < 5000.0;
                    s.what = s.recognised ? o.type : "aircraft";
                    pic.push_back(s);
                }
            }
            const int by = locked_by_[i];
            if (by >= 0 && !planes_[static_cast<std::size_t>(by)].gone) {
                Sighting s;
                s.kind = SightKind::Emitter;
                s.by = SightBy::Rwr;
                s.rwr = RwrState::Lock;
                for (const Launch& l : launches_) {
                    if (!l.done && l.radar && l.shooter == by && l.target == static_cast<int>(i)) s.rwr = RwrState::Launch;
                }
                const Vec3 d = planes_[static_cast<std::size_t>(by)].a.position - me.a.position;
                s.bearing_rad = std::atan2(d.x, d.y);
                s.time_s = t;
                s.what = "lock";
                pic.push_back(s);
            }
        }
        for (Launch& l : launches_) {
            if (l.seen || !Valid(l.target) || !Valid(l.shooter)) continue;
            l.seen = true;
            const Plane& sh = planes_[static_cast<std::size_t>(l.shooter)];
            const Plane& tg = planes_[static_cast<std::size_t>(l.target)];
            const double r = (sh.a.position - tg.a.position).Length();
            if (r > 10000.0) continue;
            Sighting s;
            s.kind = SightKind::LaunchSign;
            s.believed_enu = sh.a.position;
            s.range_est_m = r;
            s.time_s = l.t;
            pictures_[static_cast<std::size_t>(l.target)].push_back(s);
        }
    }

private:
    struct Launch {
        int shooter = -1, target = -1;
        bool radar = false;
        double t = 0.0;
        bool done = false, seen = false;
    };
    bool Valid(int t) const { return t >= 0 && static_cast<std::size_t>(t) < planes_.size(); }
    const ReactionTable* rt_;
    std::vector<Plane> planes_;
    std::vector<std::vector<Sighting>> pictures_;
    std::vector<int> locked_by_;
    std::vector<Launch> launches_;
    bool far_ = false;
};

Tally Fly(const Case& k, const AirFightDef& base, int mode, int far_fight, const ReactionTable& rt, std::uint64_t seed) {
    AirFightDef def = base;
    def.mode = mode;
    def.far_fight = far_fight;
    def.controllers = k.controllers;
    World w(k, rt);
    std::vector<std::string> ids;
    for (std::size_t i = 0; i < k.members.size(); ++i) {
        const Case::Member& m = k.members[i];
        AirFightFighterDef f;
        f.target = w.Planes()[i].id;
        f.type = m.type;
        f.faction = m.faction;
        f.role = m.role;
        f.controller = m.controller;
        f.regiment = m.faction == "ours" ? "fighter_rgt_1" : "";
        def.fighters.push_back(f);
        ids.push_back(f.target);
    }
    AirFightSession s;
    std::string e;
    Tally out;
    if (!s.Init(def, ids, seed, &e)) {
        std::printf("  %s: %s\n", k.name.c_str(), e.c_str());
        return out;
    }
    const double dt = 0.25;
    AirFightWorld hooks = w.Hooks();
    for (double t = dt; t <= k.until_s; t += dt) {
        w.Move(t, dt);
        w.See(t);
        const auto t0 = std::chrono::steady_clock::now();
        s.Step(t, hooks);
        out.ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    }
    for (const AirFightSession::Kill& kl : s.Kills()) {
        (kl.target.rfind("ours", 0) == 0 ? out.lost_b : out.lost_a) += 1;
    }
    out.shots = static_cast<int>(s.Shots().size());
    out.c = s.Tally();
    return out;
}

Case Pairs(const std::string& name, const std::string& a_type, const std::string& a_faction, const std::string& a_ctl, const std::string& b_type,
           int per_side, bool far_) {
    Case k;
    k.name = name;
    k.far_ = far_;
    for (int i = 0; i < per_side; ++i) {
        const double x = 1500.0 * i;
        k.members.push_back({a_type, a_faction, "sweep", a_ctl, Vec3{x, 0.0, 6000.0}, Vec3{0.0, 250.0, 0.0}, 250.0});
    }
    for (int i = 0; i < per_side; ++i) {
        const double x = 1500.0 * i;
        k.members.push_back({b_type, "ours", "intercept", "our_gci", Vec3{x, 80000.0, 5000.0}, Vec3{0.0, -240.0, 0.0}, 240.0});
    }
    AirFightControllerDef gci;
    gci.id = "our_gci";
    gci.side = "ours";
    gci.kind = "radar_control";
    gci.pos = Vec3{0.0, 120000.0, 0.0};
    gci.range_m = 300000.0;
    k.controllers.push_back(gci);
    if (a_ctl == "e3a") {
        AirFightControllerDef c;
        c.id = "e3a";
        c.side = "enemy";
        c.kind = "airborne_warning";
        c.pos = Vec3{-150000.0, -50000.0, 9000.0};
        c.range_m = 400000.0;
        k.controllers.push_back(c);
    } else if (a_ctl == "their_gci") {
        AirFightControllerDef c;
        c.id = "their_gci";
        c.side = "enemy";
        c.kind = "radar_control";
        c.pos = Vec3{0.0, -60000.0, 0.0};
        c.range_m = 300000.0;
        k.controllers.push_back(c);
    }
    return k;
}

Case Strike() {
    Case k;
    k.name = "nato 4 tornado_ids low + 2 f15c escorts vs our 2 mig23ml";
    for (int i = 0; i < 4; ++i) k.members.push_back({"tornado_ids", "nato", "strike", "", Vec3{1000.0 * i, 0.0, 300.0}, Vec3{0.0, 250.0, 0.0}, 250.0});
    for (int i = 0; i < 2; ++i) k.members.push_back({"f15c", "nato", "escort", "", Vec3{1000.0 * i, -2000.0, 900.0}, Vec3{0.0, 250.0, 0.0}, 250.0});
    for (int i = 0; i < 2; ++i) k.members.push_back({"mig23ml", "ours", "intercept", "our_gci", Vec3{1500.0 * i, 70000.0, 3000.0}, Vec3{0.0, -240.0, 0.0}, 240.0});
    AirFightControllerDef gci;
    gci.id = "our_gci";
    gci.side = "ours";
    gci.kind = "radar_control";
    gci.pos = Vec3{0.0, 120000.0, 0.0};
    gci.range_m = 300000.0;
    k.controllers.push_back(gci);
    return k;
}

void Row(const char* label, const Tally& t) {
    std::printf("  %-10s %6d %6d %6d %6d %7d %6d %5d %6d %5d %7d %6d %6d %8.1f\n", label, t.lost_a, t.lost_b, t.shots, t.c.drags, t.c.extends,
                t.c.setups, t.c.dispenses, t.c.out_of_energy, t.c.notched, t.c.thinks, t.c.thinks_far, t.c.senses, t.ms);
}

}  // namespace

int main(int argc, char** argv) {
    const std::string data = argc > 1 ? argv[1] : "data";
    const int seeds = argc > 2 ? std::max(1, std::atoi(argv[2])) : 50;
    AirFightDef base;
    base.present = true;
    base.year = 1985;
    std::string e;
    if (!LoadAirCombatFile(data + "/air_war/air_combat.json", base.combat, &e) || !LoadSensorsFile(data + "/air_war/sensors.json", base.sensors, &e) ||
        !FillLookDown(base.sensors, data, &e)) {
        std::fprintf(stderr, "%s\n", e.c_str());
        return 1;
    }
    for (const char* f : {"nato", "warsaw_pact", "neutral"}) {  // their vector lines (roadmap S3)
        std::ifstream in(data + "/air_war/faction_" + f + ".json", std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        VectorLinesFromJson(text, base.vector_lines[f]);
    }
    ReactionTable rt;
    if (!LoadReactionTable(data, rt, &e)) {
        std::fprintf(stderr, "%s\n", e.c_str());
        return 1;
    }
    std::printf("air_combat_bench: %d seeds a case; A is the first faction, B ours\n", seeds);
    std::printf("  %-10s %6s %6s %6s %6s %7s %6s %5s %6s %5s %7s %6s %6s %8s\n", "", "A lost", "B lost", "shots", "drags", "extends", "setups", "disp",
                "energy", "notch", "thinks", "far", "senses", "ms");
    const std::vector<Case> cases = {
        Pairs("nato 2 f15c (beam, E-3A) vs our 2 mig23ml (slash, GCI)", "f15c", "nato", "e3a", "mig23ml", 2, false),
        Pairs("warsaw_pact 2 mig23ml (slash, GCI) vs our 2 mig21bis (slash, GCI)", "mig23ml", "warsaw_pact", "their_gci", "mig21bis", 2, false),
        Pairs("neutral 2 draken_gun (turn) vs our 2 mig21bis (slash, GCI)", "draken_gun", "neutral", "", "mig21bis", 2, false),
        Strike(),
    };
    for (const Case& k : cases) {
        std::printf("%s\n", k.name.c_str());
        for (int mode = 0; mode <= 1; ++mode) {
            Tally sum;
            for (int sd = 0; sd < seeds; ++sd) sum.Add(Fly(k, base, mode, 0, rt, 1000u + static_cast<std::uint64_t>(sd)));
            Row(mode == 0 ? "B8" : "S3", sum);
        }
    }
    // The far fight (A1): the same 4 v 4 far from all our ground units, thought at a1_think_s on its leaders' pictures.
    const Case far_case = Pairs("far: nato 4 f15c (E-3A) vs our 4 mig23ml (GCI), far from our ground units", "f15c", "nato", "e3a", "mig23ml", 4, true);
    std::printf("%s\n", far_case.name.c_str());
    for (int ff = 0; ff <= 1; ++ff) {
        Tally sum;
        for (int sd = 0; sd < seeds; ++sd) sum.Add(Fly(far_case, base, 1, ff, rt, 2000u + static_cast<std::uint64_t>(sd)));
        Row(ff == 0 ? "S3 A2" : "S3 A1", sum);
    }
    return 0;
}
