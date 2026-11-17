// simcore/air_war/c2.hpp — command in the air: the enemy's posts, their authority, the messages between them and the
// alert pools (roadmap B5, docs/air_battle/05_C2_AND_RETASKING.md).
//
// In 1985 a flight in the air is changed "rarely, by voice, through a ground node, and slowly" (05 §1). This header gives
// the enemy that and no more:
//
//   * C2Authority::May: who may do what to a flight of which mission, where, in which year (the faction's "c2"
//     authority table, c2_def.hpp). The session's flights ask it for every order given by a post (package_control.hpp).
//   * AirC2: the faction's posts as places with people (a duty officer each, his traits bending the post's decisions by
//     at most c2.trait_bound), the messages between them with a delay per hop (relay, decide, encode), lost or late when
//     jammed, heard by our listeners near our battery; the alert pools built from the sorties the plan holds back,
//     standing their shifts and standing down; and the post's decision on a report, in the design's order (§4.3): a
//     hunter already there with leave is warned; an airborne flight the post may divert is diverted (a CAS divert after
//     the corps agrees, or after silence_consent_s); an alert pool within authority is scrambled; otherwise the report
//     goes to intelligence as an in-flight report, there in minutes.
//
// Everything AirC2 knows comes from reports: a flight's aircraft that saw a launch (the resolver's AirEvent, or a played
// package's pilots' sightings), a FAC's own sighting of our troops. It never reads where our side really is. Every draw
// comes from a stream keyed by (message id, step) or (post, hour) of a root fixed at the campaign's start, so the order
// packages and messages are handled in, and the campaign's own stream, never change a C2 draw (§4.7).
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

#include "simcore/air_war/c2_def.hpp"
#include "simcore/air_war/data.hpp"
#include "simcore/air_war/intel.hpp"
#include "simcore/air_war/officers.hpp"
#include "simcore/air_war/resolve.hpp"
#include "simcore/air_war/state.hpp"
#include "simcore/air_war/types.hpp"

namespace simcore {
namespace airwar {

// Where a place lies for a post: inside its own area (a corps', a division's allocation, its radio cover), inside a
// free-hunt zone open now.
struct C2Where {
    bool own = false;
    bool free_hunt = false;
};

class C2Authority {
public:
    static bool Covers(const C2AuthorityDef& a, const std::string& mission) {
        for (const std::string& m : a.missions) {
            if (m == mission || (m == "any" && mission != "fighters")) return true;
        }
        return false;
    }
    static bool WhereFits(const C2AuthorityDef& a, const C2Where& w) {
        if (a.where == "any") return true;
        if (a.where == "free_hunt_zone") return w.free_hunt;
        return w.own;
    }
    // In force in the war's year (a later war's practice stays out of 1985: §6).
    static bool InForce(const C2AuthorityDef& a, int year) { return a.from_year <= year; }
    // The authority line that lets a post of `post_kind` do `act` to a flight of `mission` at a place `where` in `year`,
    // or null: nobody of that kind may.
    static const C2AuthorityDef* Find(const FactionC2& c2, const std::string& post_kind, const std::string& act, const std::string& mission,
                                      const C2Where& where, int year) {
        if (!c2.present) return nullptr;
        for (const C2AuthorityDef& a : c2.authority) {
            if (a.post == post_kind && a.act == act && InForce(a, year) && Covers(a, mission) && WhereFits(a, where)) return &a;
        }
        return nullptr;
    }
    static bool May(const FactionC2& c2, const std::string& post_kind, const std::string& act, const std::string& mission, const C2Where& where,
                    int year) {
        return Find(c2, post_kind, act, mission, where, year) != nullptr;
    }
};

// A package's mission as the authority table names it: a strike within c2.bai_depth_m of the front is battlefield
// interdiction ("bai"), beyond it air interdiction ("strike"); the rest by their kind.
inline std::string C2MissionOf(const Package& p, const AirWarData& d) {
    if (p.kind == MissionKind::Strike) {
        const FrontDef* front = d.theatre.FrontFor(d.faction.front);
        if (front != nullptr && DistanceToLine(front->line, p.target_pos) <= d.tuning.Get("c2.bai_depth_m")) return "bai";
    }
    return MissionKindName(p.kind);
}

enum class C2Kind { Warning, InFlightReport, Request, Approval, Refusal, Divert, Scramble, CheckIn, Brief, Recall, Codeword, DebriefReport };

inline const char* C2KindName(C2Kind k) {
    switch (k) {
        case C2Kind::Warning: return "warning";
        case C2Kind::InFlightReport: return "in_flight_report";
        case C2Kind::Request: return "request";
        case C2Kind::Approval: return "approval";
        case C2Kind::Refusal: return "refusal";
        case C2Kind::Divert: return "divert";
        case C2Kind::Scramble: return "scramble";
        case C2Kind::CheckIn: return "check_in";
        case C2Kind::Brief: return "brief";
        case C2Kind::Recall: return "recall";
        case C2Kind::Codeword: return "codeword";
        case C2Kind::DebriefReport: return "debrief_report";
    }
    return "?";
}
// On the air each message is one of 09_COMMS_AND_INTERCEPT.md §2.2's call kinds (air_radio.hpp CallKind's names); a
// Scramble goes by land line or the command radio to the pool's airfield and a DebriefReport is said on the ground: "".
inline const char* C2CallKind(C2Kind k) {
    switch (k) {
        case C2Kind::Warning: return "threat_call";
        case C2Kind::InFlightReport: return "report";
        case C2Kind::Request:
        case C2Kind::Approval:
        case C2Kind::Refusal: return "request";
        case C2Kind::Divert:
        case C2Kind::Brief:
        case C2Kind::Recall: return "divert";
        case C2Kind::CheckIn: return "check_in";
        case C2Kind::Codeword: return "codeword";
        case C2Kind::Scramble:
        case C2Kind::DebriefReport: return "";
    }
    return "";
}

// A message between the posts, a flight and a pool (§4.3): what it is about carries its place, sigma and the time it was
// observed, so every position has an age.
struct C2Message {
    int id = 0;
    C2Kind kind = C2Kind::InFlightReport;
    std::string from;  // a post's id, a flight's call sign, "army", "silence"
    std::string to;
    std::string net;
    double sent_h = 0.0;
    double arrive_h = 0.0;
    double length_s = 6.0;
    std::string language;
    int precedence = 1;  // 0 routine, 1 priority, 2 flash
    int package_id = -1;
    int flight = -1;
    int pool = -1;
    int decision = -1;      // the decision it belongs to (a request and its silence)
    std::string what;       // "launch", "column", "troops"
    std::string object_id;  // the object the report is about, as the enemy keys it ("" : a place only)
    ObjectKind object_kind = ObjectKind::SamSite;
    bool mobile = true;
    Vec3 where;
    double sigma_m = 0.0;
    double observed_h = 0.0;
    std::string mission;    // the mission concerned (the authority's name)
    int post = -1;          // the post that handles it on arrival
    int tries = 1;
    bool lost = false;
    bool heard = false;     // our listeners heard it (a fact for our side's journal)
    std::string text;
};

struct C2PostState {
    int def = -1;
    std::string id;
    std::string kind;
    Vec3 at;
    std::string sector;  // the front entry it serves ("" : the whole front)
    int officer = -1;
    bool alive = true;
    int queued = 0;  // messages waiting on it
};

// A FAC's own sighting of our troops, with its time (§4.3: close support aimed where the FAC believes them).
struct FacSighting {
    int post = -1;
    std::string object_id;
    Vec3 pos;
    double sigma_m = 0.0;
    double observed_h = 0.0;
};

// An alert pool (§4.4): aircraft of one squadron held at a readiness for on-call work.
struct AlertPool {
    int id = 0;
    std::string kind;  // the faction's alert id
    std::string squadron;
    int aircraft = 0;
    int flown = 0;
    double takeoff_s = 0.0;
    bool airborne = false;
    double begin_h = 0.0;
    double until_h = 0.0;      // the end of this shift
    double cycle_end_h = 0.0;  // stands down for good then
    int shift = 0;
    bool stood_down = false;
    bool Ready(double now_h) const { return !stood_down && now_h >= begin_h && now_h < until_h && aircraft > 0; }
};

struct C2Stats {
    int reports = 0;          // in-flight reports sent
    int out_of_cover = 0;     // a flight that saw something out of every post's cover (its report waits for the debrief)
    int warnings = 0;
    int diverts = 0;
    int requests = 0;
    int army_waits = 0;
    int refusals = 0;
    int scrambles = 0;
    int on_call_packages = 0;
    int on_call_sorties = 0;
    int pool_aircraft = 0;     // held in pools, all cycles
    int pools = 0;
    int pools_stood_down = 0;  // stood down unused
    int lost = 0;              // messages lost to jamming
    int retried = 0;           // hops said again
    int heard = 0;             // calls our listeners heard
    int to_intel = 0;          // reports that went to intelligence (nobody acted)
    int stale = 0;             // too old to strike
    int briefs = 0;            // FAC briefs that aimed close support
    int fac_sightings = 0;
};

// What the campaign gives the posts each step.
struct C2Context {
    const AirWarData* data = nullptr;
    std::vector<ObjectState>* truth = nullptr;  // only the FACs' own eyes on our troops read it, as their sightings
    AirStaff* staff = nullptr;
    std::vector<Package>* packages = nullptr;
    struct Air {
        int package = -1;  // index into packages
        PackageRun* run = nullptr;
    };
    std::vector<Air> airborne;
    int phase = 0;
    Vec3 battery;  // our battery: where our listeners are
    // The planner's on-call builder (Planner::OnCall): a package from the pool, its first aircraft up at takeoff_h.
    std::function<bool(MissionKind, const IntelEntry&, const AlertPool&, double, Package&)> on_call;
    std::function<void(Package&)> launch;                       // a new package into the campaign
    std::function<void(const IntelReport&)> intel;              // into the enemy's picture
    std::function<const IntelEntry*(const std::string&)> believed;  // the enemy's picture of an object
    std::function<void(const std::string&, const std::string&, const std::string&, bool)> log;  // kind, who, text, visible
};

class AirC2 {
public:
    using Jam = std::function<double(const C2Message&)>;  // the share of a hop's tries jammed out (0: none)

    // The faction's posts on its front, each with a duty officer (appointed after the staff, from `rng`); every C2 draw
    // from forks of `root`.
    void Init(const AirWarData& d, AirStaff& staff, Rng& rng, Rng root, double now_h) {
        d_ = &d;
        staff_ = &staff;
        root_ = root;
        posts_.clear();
        pools_.clear();
        messages_.clear();
        log_.clear();
        sightings_.clear();
        decided_.clear();
        recent_.clear();
        calls_.clear();
        retasks_.clear();
        reports_.clear();
        reported_.clear();
        stats_ = C2Stats();
        carry_.clear();
        next_message_ = 1;
        next_pool_ = 1;
        next_decision_ = 0;
        const FactionC2& c2 = d.faction.c2;
        if (!c2.present) return;
        const FrontDef* front = d.theatre.FrontFor(d.faction.front);
        if (front == nullptr) return;
        for (std::size_t k = 0; k < c2.posts.size(); ++k) {
            const C2PostDef& pd = c2.posts[k];
            if (pd.from_year > d.theatre.year) continue;
            std::vector<std::pair<std::string, Vec3>> places;
            if (pd.at == "entries") {
                for (const EntryPoint& e : front->entries) places.push_back({e.id, Back(e.pos, front->bases, pd.back_km)});
            } else if (pd.at == "line_points") {  // along the front: one post at each point of the line
                for (std::size_t li = 0; li < front->line.size(); ++li) places.push_back({"L" + std::to_string(li + 1), Back(front->line[li], front->bases, pd.back_km)});
            } else if (pd.at == "line") {
                Vec3 mid = front->line.empty() ? front->bases : front->line[front->line.size() / 2];
                places.push_back({std::string(), Back(mid, front->bases, pd.back_km)});
            } else {
                places.push_back({std::string(), front->bases});
            }
            for (const auto& pl : places) {
                C2PostState ps;
                ps.def = static_cast<int>(k);
                ps.id = pl.first.empty() ? pd.id : pd.id + "@" + pl.first;
                ps.kind = pd.kind;
                ps.at = pl.second;
                ps.sector = pl.first;
                ps.officer = staff.AppointPost(RoleOf(pd.kind), ps.id, d, rng, now_h);
                posts_.push_back(ps);
            }
        }
    }

    // The data and staff it reads (again after its owner was copied or moved: the campaign binds before every use).
    void Bind(const AirWarData& d, const AirStaff& staff) {
        d_ = &d;
        staff_ = &staff;
    }
    bool On() const { return d_ != nullptr && d_->faction.c2.present; }
    void SetJam(Jam j) { jam_ = std::move(j); }
    const std::vector<C2PostState>& Posts() const { return posts_; }
    const std::vector<AlertPool>& Pools() const { return pools_; }
    const std::vector<C2Message>& Log() const { return log_; }  // every message sent, in order
    const std::vector<FacSighting>& Sightings() const { return sightings_; }
    const C2Stats& Stats() const { return stats_; }
    bool Busy() const { return !messages_.empty(); }
    const C2PostState* Post(const std::string& id) const {
        for (const C2PostState& p : posts_) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }
    const C2PostDef& Def(const C2PostState& p) const { return d_->faction.c2.posts[static_cast<std::size_t>(p.def)]; }
    // Where a place lies for this post.
    C2Where WhereFor(const C2PostState& p, const Vec3& place, int phase) const {
        C2Where w;
        w.own = Dist2d(p.at, place) <= Def(p).reach_km * 1000.0;
        w.free_hunt = FreeHunt(place, phase);
        return w;
    }
    bool FreeHunt(const Vec3& place, int phase) const {
        const C2FreeHuntDef& fh = d_->faction.c2.free_hunt;
        if (fh.from_phase < 0 || phase < fh.from_phase) return false;
        const FrontDef* front = d_->theatre.FrontFor(d_->faction.front);
        return front != nullptr && DistanceToLine(front->line, place) <= fh.depth_km * 1000.0;
    }

    // ---- the alert pools (§4.4)

    // At a plan: the sorties held back from the cycle (`held_per_squadron`, the squadrons' per-cycle sorties times the
    // doctrine's on_call_fraction) become pools by the faction's alert mix, on the squadrons whose types fly on-call work,
    // in whole flights, from `begin_h` to `end_h`; in all no more than on_call_fraction/(1 - on_call_fraction) of the
    // sorties the plan flies (`planned`), so the share flown on call never passes on_call_fraction.
    void BuildPools(const std::vector<SquadronState>& squadrons, const std::vector<double>& held_per_squadron, int planned, double begin_h,
                    double end_h, int phase) {
        if (!On()) return;
        const FactionDef& f = d_->faction;
        const double ocf = f.doctrine.on_call_fraction;
        if (ocf <= 0.0 || f.c2.alert.empty()) return;
        int cap = static_cast<int>(std::floor(ocf / std::max(1e-6, 1.0 - ocf) * planned + 1e-9));
        // The squadrons that fly on-call work and what each holds back.
        std::vector<double> left(squadrons.size(), 0.0);
        std::vector<int> fsize(squadrons.size(), 0);
        double total = 0.0;
        for (std::size_t si = 0; si < squadrons.size() && si < held_per_squadron.size(); ++si) {
            const SquadronState& s = squadrons[si];
            const AircraftType* a = f.Aircraft(s.aircraft);
            if (a == nullptr || a->missile || a->drone || !a->InService(d_->theatre.year)) continue;
            if (!(a->Has(FlightRole::Strike) || (a->Has(FlightRole::Sead) && a->arms > 0))) continue;
            left[si] = std::max(0.0, held_per_squadron[si]);
            fsize[si] = std::max(2, FlightSize(*a));
            total += left[si];
        }
        // Each pool kind's share of the whole, its fraction of a flight carried to the next cycle (a squadron alone holds
        // back less than a flight); whole flights from the squadrons with the most held back.
        for (const C2AlertDef& al : f.c2.alert) {
            const double scale = phase < static_cast<int>(al.phase_scale.size()) ? al.phase_scale[static_cast<std::size_t>(phase)] : 1.0;
            double& carry = carry_[al.id];
            carry += total * al.share * scale;
            std::map<std::size_t, int> got;  // squadron -> aircraft
            for (;;) {
                int best = -1;
                for (std::size_t si = 0; si < left.size(); ++si) {
                    if (fsize[si] <= 0 || left[si] <= 0.0) continue;
                    if (best < 0 || left[si] > left[static_cast<std::size_t>(best)]) best = static_cast<int>(si);
                }
                if (best < 0) break;
                const int n = fsize[static_cast<std::size_t>(best)];
                if (carry + 1e-9 < n || cap < n) break;
                carry -= n;
                cap -= n;
                left[static_cast<std::size_t>(best)] -= n;
                got[static_cast<std::size_t>(best)] += n;
            }
            carry = std::min(carry, 4.0);  // never more than a flight owed
            for (const auto& g : got) {
                AlertPool p;
                p.id = next_pool_++;
                p.kind = al.id;
                p.squadron = squadrons[g.first].id;
                p.aircraft = g.second;
                p.airborne = al.airborne;
                Rng r = Draw(0xA1E47000ull + static_cast<std::uint64_t>(p.id), 0);
                p.takeoff_s = al.airborne ? 0.0 : r.Uniform(al.takeoff_s.lo, al.takeoff_s.hi);
                p.begin_h = begin_h;
                p.cycle_end_h = end_h;
                p.until_h = std::min(end_h, begin_h + r.Uniform(al.hold_h.lo, al.hold_h.hi));
                pools_.push_back(p);
                stats_.pool_aircraft += p.aircraft;
                ++stats_.pools;
            }
        }
    }

    // A FAC's eyes, each hour: our troops within its sight, with its own error and the hour (and, now and then, a request
    // for immediate support on them, c2.fac_request_per_h).
    void Hourly(double now_h, C2Context& c) {
        if (!On()) return;
        const TuningTable& tune = d_->tuning;
        for (std::size_t pi = 0; pi < posts_.size(); ++pi) {
            const C2PostState& ps = posts_[pi];
            const C2PostDef& pd = Def(ps);
            if (!ps.alive || ps.kind != "fac" || pd.sight_km <= 0.0) continue;
            for (std::size_t i = 0; i < d_->theatre.objects.size(); ++i) {
                const TheatreObject& o = d_->theatre.objects[i];
                const ObjectState& st = (*c.truth)[i];
                if (o.kind != ObjectKind::Troops || st.function <= 0.05 || Dist2d(st.pos, ps.at) > pd.sight_km * 1000.0) continue;
                Rng r = Draw(0xFAC00000ull + pi * 7919u + i, static_cast<std::uint64_t>(std::llround(now_h * 12.0)));
                FacSighting s;
                s.post = static_cast<int>(pi);
                s.object_id = o.id;
                s.sigma_m = tune.Get("c2.fac_sigma_m");
                s.pos = st.pos + Vec3{r.Normal(0.0, s.sigma_m), r.Normal(0.0, s.sigma_m), 0.0};
                s.observed_h = now_h;
                Sight(s);
                if (!r.Chance(tune.Get("c2.fac_request_per_h"))) continue;
                // A request for immediate close support on what he sees, up his request net.
                C2Message m;
                m.kind = C2Kind::Request;
                m.from = ps.id;
                m.what = "troops";
                m.object_id = o.id;
                m.object_kind = o.kind;
                m.mobile = o.mobile;
                m.where = s.pos;
                m.sigma_m = s.sigma_m;
                m.observed_h = now_h;
                m.mission = "cas";
                const int to = Covering(s.pos, {"support_centre", "tasking"});
                if (to < 0) continue;
                m.post = to;
                m.to = posts_[static_cast<std::size_t>(to)].id;
                m.text = "request immediate air support on troops";
                ++stats_.requests;
                Send(m, "request", now_h, Duty(static_cast<int>(pi)), c);
            }
        }
    }

    // A close support package's brief at its check-in (§4.3): aimed where the FAC with our troops in sight last saw them,
    // with that sighting's age, never where they are now. False: no FAC has them (the plan's aim stays).
    bool Brief(Package& p, double now_h, C2Context& c) {
        if (!On() || (p.kind != MissionKind::Cas && p.kind != MissionKind::HeloRaid) || p.target_object.empty()) return false;
        const FacSighting* best = nullptr;
        for (const FacSighting& s : sightings_) {
            if (s.object_id != p.target_object || s.observed_h > now_h) continue;
            if (best == nullptr || s.observed_h > best->observed_h) best = &s;
        }
        if (best == nullptr) return false;
        for (Flight& fl : p.flights) {
            if (fl.aim_key == p.target_key) fl.aim_pos = best->pos;
        }
        C2Message m;
        m.kind = C2Kind::Brief;
        m.from = posts_[static_cast<std::size_t>(best->post)].id;
        m.to = p.name;
        m.package_id = p.id;
        m.flight = 0;
        m.what = "troops";
        m.object_id = best->object_id;
        m.where = best->pos;
        m.sigma_m = best->sigma_m;
        m.observed_h = best->observed_h;
        m.text = "the FAC's brief: the target and where he saw it";
        m.post = -1;
        ++stats_.briefs;
        Send(m, "order", now_h, Duty(best->post), c, true);
        return true;
    }

    // What an airborne package's aircraft saw since the last step (the resolver's events): a launch or a column, reported
    // in flight by the flight's leader where a post's radio covers him and the mission reports in flight; otherwise it
    // waits for the debrief (the campaign's BeliefsFrom).
    void OnEvents(const Package& p, PackageRun& run, double now_h, C2Context& c) {
        if (!On()) {
            run.events.clear();
            return;
        }
        for (const AirEvent& e : run.events) {
            if (e.alive <= 0) continue;
            if (e.kind != AirEvent::Kind::LaunchSeen && e.kind != AirEvent::Kind::ColumnFound) continue;
            const std::string key = std::to_string(p.id) + ":" + e.object_id;
            if (!reported_.insert(key).second) continue;  // once per package and object
            const Flight& fl = p.flights[static_cast<std::size_t>(e.flight)];
            const int oi = d_->theatre.IndexOf(e.object_id);
            if (oi < 0) continue;
            const TheatreObject& o = d_->theatre.objects[static_cast<std::size_t>(oi)];
            Rng r = Draw(0x5E500000ull + static_cast<std::uint64_t>(p.id), static_cast<std::uint64_t>(oi));
            const double sigma = d_->tuning.Get("c2.report_sigma_m");
            const Vec3 seen = e.where + Vec3{r.Normal(0.0, sigma * 0.7), r.Normal(0.0, sigma * 0.7), 0.0};
            const Vec3 from = PackageRun::PlaceOf(fl, e.t_h);
            Report(p, e.flight, e.kind == AirEvent::Kind::LaunchSeen ? "launch" : "column", o.id, o.decoy ? o.looks_like : o.kind, o.mobile, seen, sigma,
                   e.t_h, from, fl.profile, std::max(now_h, e.t_h), c);
        }
        run.events.clear();
    }

    // A flight's report (its leader's, or a played package's pilot's) of what he saw at `seen` at `observed_h`, from his
    // place `from`. False: out of every post's cover, or the mission does not report in flight.
    bool Report(const Package& p, int flight, const std::string& what, const std::string& object_id, ObjectKind kind, bool mobile, const Vec3& seen,
                double sigma_m, double observed_h, const Vec3& from, Profile profile, double now_h, C2Context& c) {
        if (!On()) return false;
        const FactionC2& c2 = d_->faction.c2;
        const std::string mission = C2MissionOf(p, *d_);
        bool reports = false;
        for (const std::string& m : c2.report_missions) reports = reports || m == "any" || m == mission;
        if (!reports) return false;
        const double low = profile == Profile::Low ? d_->tuning.Get("c2.low_cover") : 1.0;
        int to = -1;
        double best = 1e18;
        for (std::size_t i = 0; i < posts_.size(); ++i) {
            const C2PostState& ps = posts_[i];
            if (!ps.alive || ps.kind == "squadron") continue;
            const double dist = Dist2d(ps.at, from);
            if (dist > Def(ps).reach_km * 1000.0 * low || dist >= best) continue;
            best = dist;
            to = static_cast<int>(i);
        }
        if (to < 0) {
            ++stats_.out_of_cover;
            return false;
        }
        C2Message m;
        m.kind = C2Kind::InFlightReport;
        m.from = flight >= 0 && flight < static_cast<int>(p.flights.size()) ? p.flights[static_cast<std::size_t>(flight)].callsign : p.name;
        m.to = posts_[static_cast<std::size_t>(to)].id;
        m.post = to;
        m.package_id = p.id;
        m.flight = flight;
        m.what = what;
        m.object_id = object_id;
        m.object_kind = kind;
        m.mobile = mobile;
        m.where = seen;
        m.sigma_m = sigma_m;
        m.observed_h = observed_h;
        m.mission = what == "launch" ? "dead" : "strike";
        m.text = what == "launch" ? "a launch seen" : "a column on the road";
        m.precedence = 2;
        ++stats_.reports;
        reports_[p.id].push_back(m);
        Send(m, "report", now_h, nullptr, c, false, &from);
        return true;
    }

    // Deliver every message due by `t_h` in the order of arrival, and act on it; stand pools down at the end of a shift.
    void StepTo(double t_h, C2Context& c) {
        if (!On()) return;
        Refresh(t_h);
        for (int guard = 0; guard < 10000; ++guard) {
            int k = -1;
            for (std::size_t i = 0; i < messages_.size(); ++i) {
                if (messages_[i].arrive_h > t_h) continue;
                if (k < 0 || messages_[i].arrive_h < messages_[static_cast<std::size_t>(k)].arrive_h ||
                    (messages_[i].arrive_h == messages_[static_cast<std::size_t>(k)].arrive_h && messages_[i].id < messages_[static_cast<std::size_t>(k)].id)) {
                    k = static_cast<int>(i);
                }
            }
            if (k < 0) break;
            C2Message m = messages_[static_cast<std::size_t>(k)];
            messages_.erase(messages_.begin() + k);
            if (m.post >= 0) posts_[static_cast<std::size_t>(m.post)].queued = std::max(0, posts_[static_cast<std::size_t>(m.post)].queued - 1);
            Handle(m, c);
        }
    }

    // The package's calls, retasks and reports so far (for its PackageOutcome), taken once.
    std::vector<C2Message> TakeCalls(int package_id) { return Take(calls_, package_id); }
    std::vector<C2Message> TakeReports(int package_id) { return Take(reports_, package_id); }
    std::vector<RetaskRecord> TakeRetasks(int package_id) {
        std::vector<RetaskRecord> out;
        const auto it = retasks_.find(package_id);
        if (it == retasks_.end()) return out;
        out = it->second;
        retasks_.erase(it);
        return out;
    }

    // The post kind's role.
    static StaffRole RoleOf(const std::string& kind) {
        if (kind == "radar_control" || kind == "airborne_warning") return StaffRole::GciController;
        if (kind == "fac") return StaffRole::Fac;
        return StaffRole::CpController;
    }

private:
    const AirWarData* d_ = nullptr;
    Rng root_{1};
    Jam jam_;
    std::vector<C2PostState> posts_;
    std::vector<AlertPool> pools_;
    std::vector<C2Message> messages_;  // in transit
    std::vector<C2Message> log_;       // every one sent
    std::vector<FacSighting> sightings_;
    std::map<int, bool> decided_;  // a decision already carried out (a request answered, or its silence run out)
    std::vector<C2Message> recent_;  // reports a methodical officer waits on
    std::set<std::string> reported_;
    std::map<int, std::vector<C2Message>> calls_;
    std::map<int, std::vector<C2Message>> reports_;
    std::map<int, std::vector<RetaskRecord>> retasks_;
    std::map<std::string, double> carry_;  // per pool kind: the held-back sorties short of a flight, to the next cycle
    C2Stats stats_;
    int next_message_ = 1;
    int next_pool_ = 1;
    int next_decision_ = 0;

    Rng Draw(std::uint64_t a, std::uint64_t b) const { return root_.Fork(a * 0x100000001B3ull + b * 0x9E3779B97F4A7C15ull + 0xC2u); }

    static Vec3 Back(const Vec3& from, const Vec3& toward, double km) {
        const Vec3 d = toward - from;
        const double len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len <= 1.0 || km <= 0.0) return from;
        const double k = std::min(1.0, km * 1000.0 / len);
        return from + d * k;
    }

    int FlightSize(const AircraftType& a) const {
        int best = 2;
        for (int k = 0; k < static_cast<int>(MissionKind::Count); ++k) {
            const PackageTemplate& t = d_->faction.doctrine.packages[k];
            if (!t.allowed) continue;
            for (const std::string& id : t.aircraft) {
                if (id == a.id) best = std::max(best, t.flight_size);
            }
        }
        return best;
    }

    const AirStaff* staff_ = nullptr;

    // The post's duty officer (null: none).
    const airwar::Officer* Duty(int post) const {
        if (post < 0 || staff_ == nullptr || post >= static_cast<int>(posts_.size())) return nullptr;
        const int oi = posts_[static_cast<std::size_t>(post)].officer;
        return oi >= 0 && oi < static_cast<int>(staff_->officers.size()) ? &staff_->officers[static_cast<std::size_t>(oi)] : nullptr;
    }

    // The nearest live post of one of `kinds` whose own area holds `place` (-1: none).
    int Covering(const Vec3& place, std::initializer_list<const char*> kinds) const {
        int best = -1;
        double bd = 1e18;
        for (std::size_t i = 0; i < posts_.size(); ++i) {
            const C2PostState& ps = posts_[i];
            bool fits = false;
            for (const char* k : kinds) fits = fits || ps.kind == k;
            if (!fits || !ps.alive) continue;
            const double dist = Dist2d(ps.at, place);
            if (dist > Def(ps).reach_km * 1000.0 || dist >= bd) continue;
            bd = dist;
            best = static_cast<int>(i);
        }
        return best;
    }

    // The nearest live post that may do `act` to `mission` at `place` (-1: none), and its authority line.
    int Authorizer(const std::string& act, const std::string& mission, const Vec3& place, int phase, const C2AuthorityDef** line = nullptr) const {
        int best = -1;
        double bd = 1e18;
        for (std::size_t i = 0; i < posts_.size(); ++i) {
            const C2PostState& ps = posts_[i];
            if (!ps.alive) continue;
            const C2AuthorityDef* a = C2Authority::Find(d_->faction.c2, ps.kind, act, mission, WhereFor(ps, place, phase), d_->theatre.year);
            if (a == nullptr) continue;
            const double dist = Dist2d(ps.at, place);
            if (dist >= bd) continue;
            bd = dist;
            best = static_cast<int>(i);
            if (line != nullptr) *line = a;
        }
        return best;
    }
    // Does any line in force give `act` for `mission` (wherever)?
    bool AnyLine(const std::string& act, const std::string& mission) const {
        for (const C2AuthorityDef& a : d_->faction.c2.authority) {
            if (a.act == act && C2Authority::InForce(a, d_->theatre.year) && C2Authority::Covers(a, mission)) return true;
        }
        return false;
    }

    void Sight(const FacSighting& s) {
        ++stats_.fac_sightings;
        for (FacSighting& x : sightings_) {
            if (x.post == s.post && x.object_id == s.object_id) {
                x = s;
                return;
            }
        }
        sightings_.push_back(s);
    }

    // A post officer's bend on his decision time: the rigid and those by the book decide slower, a crowded post slows the
    // less composed; within c2.trait_bound either way.
    double Bend(const airwar::Officer* o, int queued) const {
        if (o == nullptr) return 1.0;
        const double b = d_->tuning.Get("c2.trait_bound");
        double k = 1.0 + 2.0 * b * (o->def.traits.rigidity - 0.5) + (o->HasQuirk("by_the_book") ? 0.5 * b : 0.0);
        k += b * std::min(1.0, queued / 5.0) * (1.0 - o->def.traits.composure);
        return ClampTo(k, 1.0 - b, 1.0 + b);
    }

    // A hop's time in seconds: relay, the officer's decision, encoding.
    double HopS(const std::string& hop, Rng& r, const airwar::Officer* o, int queued) const {
        const C2HopDef* h = d_->faction.c2.Hop(hop);
        if (h == nullptr) return 0.0;
        auto draw = [&](const C2Band& b) { return b.Set() ? r.Uniform(b.lo, b.hi) : 0.0; };
        const double relay = draw(h->relay_s);
        const double decide = draw(h->decide_s) * Bend(o, queued);
        const double encode = draw(h->encode_s);
        return relay + decide + encode;
    }

    // How long a call of each kind is on the air (sam3d estimates: a short report, a longer brief).
    static double LengthS(C2Kind k) {
        switch (k) {
            case C2Kind::Warning: return 4.0;
            case C2Kind::InFlightReport: return 8.0;
            case C2Kind::Request: return 10.0;
            case C2Kind::Divert: return 12.0;
            case C2Kind::Brief: return 20.0;
            case C2Kind::CheckIn: return 4.0;
            case C2Kind::Codeword: return 2.0;
            default: return 3.0;
        }
    }

    // "12 km north-east of the battery".
    static std::string FromBattery(const Vec3& at, const Vec3& battery) {
        static const char* dirs[] = {"north", "north-east", "east", "south-east", "south", "south-west", "west", "north-west"};
        const double dx = at.x - battery.x, dy = at.y - battery.y;
        const double km = std::sqrt(dx * dx + dy * dy) / 1000.0;
        double brg = std::atan2(dx, dy) * 57.29577951308232;
        if (brg < 0.0) brg += 360.0;
        const int i = static_cast<int>(std::floor((brg + 22.5) / 45.0)) % 8;
        return std::to_string(static_cast<int>(km + 0.5)) + " km " + dirs[i] + " of the battery";
    }

    // Send a message by `hop` (its time drawn, a jammed try said again after c2.retry_s, lost after c2.max_retries; heard
    // by our listeners near our battery). `from_at`: where the sender is.
    void Send(C2Message m, const std::string& hop, double now_h, const airwar::Officer* o, C2Context& c, bool record_only = false,
              const Vec3* from_at = nullptr) {
        const TuningTable& tune = d_->tuning;
        m.id = next_message_++;
        m.sent_h = now_h;
        m.language = d_->faction.radio.language;
        m.length_s = LengthS(m.kind);
        const int sender = PostIndex(m.from);
        if (m.net.empty()) m.net = sender >= 0 ? Def(posts_[static_cast<std::size_t>(sender)]).net : std::string("air");
        Rng r = Draw(0xC2000000ull + static_cast<std::uint64_t>(m.id), 1);
        double s = HopS(hop, r, o, m.post >= 0 ? posts_[static_cast<std::size_t>(m.post)].queued : 0);
        const std::string call = C2CallKind(m.kind);
        if (!call.empty() && jam_) {
            const double share = Clamp01(jam_(m));
            const int max_tries = std::max(1, static_cast<int>(tune.Get("c2.max_retries") + 0.5));
            int tries = 1;
            while (share > 0.0 && r.Chance(share)) {
                if (tries >= max_tries) {
                    m.lost = true;
                    break;
                }
                ++tries;
                s += tune.Get("c2.retry_s");
                ++stats_.retried;
            }
            m.tries = tries;
        }
        m.arrive_h = now_h + s / 3600.0;
        if (!call.empty() && !m.lost) {
            const Vec3 at = from_at != nullptr ? *from_at : (sender >= 0 ? posts_[static_cast<std::size_t>(sender)].at : m.where);
            if (Dist2d(at, c.battery) <= tune.Get("c2.intercept_reach_m")) {
                double chance = tune.Get("c2.intercept") * (1.0 - d_->faction.radio.opsec);
                if (o != nullptr && o->HasQuirk("loose_lips")) chance = std::min(1.0, chance * 1.5);
                if (r.Chance(chance)) {
                    m.heard = true;
                    ++stats_.heard;
                    if (c.log) {
                        c.log("own_intel", "radio intercept",
                              "Radio intercept: an enemy air " + call + " call, " + FromBattery(at, c.battery) +
                                  (m.kind == C2Kind::Divert || m.kind == C2Kind::Warning || m.kind == C2Kind::Brief
                                       ? ", naming a place " + FromBattery(m.where, c.battery)
                                       : std::string()) +
                                  ".",
                              true);
                    }
                }
            }
        }
        log_.push_back(m);
        if (m.package_id >= 0) calls_[m.package_id].push_back(m);
        if (m.lost) {
            ++stats_.lost;
            return;
        }
        if (record_only) return;
        if (m.post >= 0) ++posts_[static_cast<std::size_t>(m.post)].queued;
        messages_.push_back(m);
    }
    // A timer, not a transmission (silence as consent): due at `due_h`.
    void Timer(C2Message m, double now_h, double due_h) {
        m.id = next_message_++;
        m.sent_h = now_h;
        m.arrive_h = due_h;
        log_.push_back(m);
        messages_.push_back(m);
    }

    int PostIndex(const std::string& id) const {
        for (std::size_t i = 0; i < posts_.size(); ++i) {
            if (posts_[i].id == id) return static_cast<int>(i);
        }
        return -1;
    }

    // The package in the air with this id, and its run (null: not airborne).
    static std::pair<Package*, PackageRun*> Airborne(C2Context& c, int package_id) {
        for (const C2Context::Air& a : c.airborne) {
            Package& p = (*c.packages)[static_cast<std::size_t>(a.package)];
            if (p.id == package_id) return {&p, a.run};
        }
        return {nullptr, nullptr};
    }

    void Handle(const C2Message& m, C2Context& c) {
        const double now = m.arrive_h;
        if (m.kind == C2Kind::InFlightReport || (m.kind == C2Kind::Request && m.text != "approve" && m.from != "army" && m.to != "army")) {
            Decide(m, c, now);
            return;
        }
        if (m.kind == C2Kind::Request && m.to == "army") {  // the corps answers
            if (decided_[m.decision]) return;
            decided_[m.decision] = true;
            Rng r = Draw(0xA4A40000ull + static_cast<std::uint64_t>(m.id), 2);
            if (r.Chance(d_->tuning.Get("c2.army_refusal"))) {
                C2Message no = m;
                no.kind = C2Kind::Refusal;
                no.from = "army";
                no.to = posts_[static_cast<std::size_t>(m.post)].id;
                no.text = "the corps refuses the divert";
                ++stats_.refusals;
                Send(no, "order", now, nullptr, c, true);
                ToIntel(m, c);
                return;
            }
            SendDivert(m, now, c, "the corps agreed");
            return;
        }
        if (m.kind == C2Kind::Approval && m.from == "silence") {  // nobody refused in time
            if (decided_[m.decision]) return;
            decided_[m.decision] = true;
            SendDivert(m, now, c, "silence is consent");
            return;
        }
        if (m.kind == C2Kind::Request && m.text == "approve") {  // approved within the allocation: the scramble
            C2Message sc = m;
            sc.kind = C2Kind::Scramble;
            sc.from = posts_[static_cast<std::size_t>(m.decision)].id;  // the scrambling post (kept in `decision`)
            sc.to = "pool " + std::to_string(m.pool);
            sc.post = -1;
            sc.text = "scramble";
            ++stats_.scrambles;
            Send(sc, "scramble", now, Duty(m.decision), c);
            return;
        }
        if (m.kind == C2Kind::Divert) {
            auto pr = Airborne(c, m.package_id);
            if (pr.first == nullptr || m.flight < 0 || m.flight >= static_cast<int>(pr.first->flights.size())) return;
            Flight& fl = pr.first->flights[static_cast<std::size_t>(m.flight)];
            const AircraftType* a = d_->faction.Aircraft(fl.aircraft);
            if (a == nullptr || fl.released || fl.aborted || fl.count - fl.lost <= 0) return;
            if (!RedirectFlight(*pr.first, *pr.second, static_cast<std::size_t>(m.flight), m.where, now, a->cruise_mps, true)) return;
            fl.aim_key = m.object_id.empty() ? fl.aim_key : m.object_id;
            RetaskRecord rr;
            rr.flight = m.flight;
            rr.kind = "divert";
            rr.by = m.from;
            rr.t_s = now * 3600.0;
            retasks_[m.package_id].push_back(rr);
            ++stats_.diverts;
            return;
        }
        if (m.kind == C2Kind::Warning) {  // a hunter told: the leader's own leave takes him there
            auto pr = Airborne(c, m.package_id);
            if (pr.first == nullptr || m.flight < 0 || m.flight >= static_cast<int>(pr.first->flights.size())) return;
            Flight& fl = pr.first->flights[static_cast<std::size_t>(m.flight)];
            const AircraftType* a = d_->faction.Aircraft(fl.aircraft);
            if (a == nullptr || fl.count - fl.lost <= 0) return;
            if (!RedirectFlight(*pr.first, *pr.second, static_cast<std::size_t>(m.flight), m.where, now, a->cruise_mps, false)) return;
            RetaskRecord rr;
            rr.flight = m.flight;
            rr.kind = "hunt";
            rr.by = "leader";
            rr.t_s = now * 3600.0;
            retasks_[m.package_id].push_back(rr);
            return;
        }
        if (m.kind == C2Kind::Scramble) {
            Scrambled(m, c, now);
            return;
        }
    }

    // A post's decision on a report or a request (§4.3), in order: too old; a methodical officer's wait; a hunter there with
    // leave; an airborne flight it may divert; an alert pool within authority; else intelligence.
    void Decide(const C2Message& m, C2Context& c, double now) {
        const FactionC2& c2 = d_->faction.c2;
        const TuningTable& tune = d_->tuning;
        const airwar::Officer* o = Duty(m.post);
        if (m.mobile && now - m.observed_h > c2.max_report_age_h) {
            ++stats_.stale;
            ToIntel(m, c);
            return;
        }
        Rng r = Draw(0xDEC10000ull + static_cast<std::uint64_t>(m.id), 3);
        if (o != nullptr) {
            const double b = tune.Get("c2.trait_bound");
            const double wait = 2.0 * b * std::max(0.0, o->def.traits.methodical - 0.5);
            bool second = false;
            for (const C2Message& x : recent_) {
                second = second || (Dist2d(x.where, m.where) <= tune.Get("c2.second_report_m") && now - x.arrive_h <= tune.Get("c2.second_report_h"));
            }
            if (!second && r.Chance(wait)) {
                C2Message keep = m;
                keep.arrive_h = now;
                recent_.push_back(keep);
                if (recent_.size() > 64) recent_.erase(recent_.begin());
                ToIntel(m, c);
                return;
            }
        }
        if (m.what == "launch" && Hunt(m, c, now)) return;
        if (Divert(m, c, now, r)) return;
        if (ScrambleFor(m, c, now)) return;
        ToIntel(m, c);
    }

    bool HunterLeave(const Vec3& place, int phase) const {
        const std::string& l = d_->faction.c2.hunter_leave;
        return l == "any" || (l == "free_hunt_zone" && FreeHunt(place, phase));
    }

    // 1. A hunter flight in the air near the place, with ARMs left, told: what it does is its leader's.
    bool Hunt(const C2Message& m, C2Context& c, double now) {
        if (!HunterLeave(m.where, c.phase)) return false;
        const double reach = d_->tuning.Get("c2.hunter_reach_m");
        int best_p = -1, best_f = -1;
        double bd = 1e18;
        for (const C2Context::Air& a : c.airborne) {
            const Package& p = (*c.packages)[static_cast<std::size_t>(a.package)];
            for (std::size_t fi = 0; fi < p.flights.size(); ++fi) {
                const Flight& fl = p.flights[fi];
                if (fl.role != FlightRole::Hunter || fl.count - fl.lost <= 0 || a.run->arms_left[fi] <= 0 || a.run->FlightDone(p, fi)) continue;
                const double dist = Dist2d(PackageRun::PlaceOf(fl, now), m.where);
                if (dist > reach || dist >= bd) continue;
                bd = dist;
                best_p = p.id;
                best_f = static_cast<int>(fi);
            }
        }
        if (best_p < 0) return false;
        const int by = AnyLine("warn", "hunter") ? (C2Authority::May(d_->faction.c2, posts_[static_cast<std::size_t>(m.post)].kind, "warn", "hunter",
                                                                      WhereFor(posts_[static_cast<std::size_t>(m.post)], m.where, c.phase), d_->theatre.year)
                                                         ? m.post
                                                         : Authorizer("warn", "hunter", m.where, c.phase))
                                                  : -1;
        if (by < 0) return false;
        auto pr = Airborne(c, best_p);
        C2Message w = m;
        w.kind = C2Kind::Warning;
        w.from = posts_[static_cast<std::size_t>(by)].id;
        w.to = pr.first->flights[static_cast<std::size_t>(best_f)].callsign;
        w.package_id = best_p;
        w.flight = best_f;
        w.post = -1;
        w.text = "SAM launch reported there";
        ++stats_.warnings;
        Send(w, "order", now, Duty(by), c);
        return true;
    }

    // 2. An airborne flight the post may divert (main flights, not yet released), nearest first. A divert that needs the
    // corps waits for its answer or for silence_consent_s, whichever first.
    bool Divert(const C2Message& m, C2Context& c, double now, Rng& r) {
        const double reach = d_->tuning.Get("c2.divert_reach_m");
        std::vector<std::pair<double, std::pair<int, int>>> cand;
        for (const C2Context::Air& a : c.airborne) {
            const Package& p = (*c.packages)[static_cast<std::size_t>(a.package)];
            for (std::size_t fi = 0; fi < p.flights.size(); ++fi) {
                const Flight& fl = p.flights[fi];
                const bool main = fl.role == FlightRole::Strike || fl.role == FlightRole::Dead || fl.role == FlightRole::Helicopter;
                if (!main || fl.released || fl.aborted || fl.count - fl.lost <= 0 || a.run->FlightDone(p, fi)) continue;
                if (fl.release_index >= 0 && static_cast<int>(a.run->next_leg[fi]) >= fl.release_index) continue;
                const double dist = Dist2d(PackageRun::PlaceOf(fl, now), m.where);
                if (dist <= reach) cand.push_back({dist, {a.package, static_cast<int>(fi)}});
            }
        }
        std::sort(cand.begin(), cand.end());
        for (const auto& k : cand) {
            const Package& p = (*c.packages)[static_cast<std::size_t>(k.second.first)];
            const std::string mission = C2MissionOf(p, *d_);
            if (m.what == "troops" && mission != "cas" && mission != "helo_raid") continue;  // close support goes to close support
            const C2AuthorityDef* line = nullptr;
            const int by = Authorizer("divert_airborne", mission, m.where, c.phase, &line);
            if (by < 0 || line == nullptr) continue;
            double chance = line->chance;
            if (const airwar::Officer* o = Duty(by)) {
                chance *= 1.0 - 2.0 * d_->tuning.Get("c2.trait_bound") * std::max(0.0, o->def.traits.rigidity - 0.5);
            }
            if (!r.Chance(chance)) continue;
            C2Message d = m;
            d.package_id = p.id;
            d.flight = k.second.second;
            d.post = by;
            d.from = posts_[static_cast<std::size_t>(by)].id;
            d.mission = mission;
            if (line->needs == "army") {
                d.decision = next_decision_++;
                C2Message ask = d;
                ask.kind = C2Kind::Request;
                ask.to = "army";
                ask.text = "the divert, for the corps' agreement";
                ++stats_.army_waits;
                ++stats_.requests;
                Send(ask, "army", now, Duty(by), c);
                if (d_->faction.c2.silence_consent_s > 0.0) {
                    C2Message quiet = d;
                    quiet.kind = C2Kind::Approval;
                    quiet.from = "silence";
                    quiet.to = d.from;
                    quiet.text = "nobody refused in time";
                    Timer(quiet, now, now + d_->faction.c2.silence_consent_s / 3600.0);
                }
                return true;
            }
            SendDivert(d, now, c, std::string());
            return true;
        }
        return false;
    }

    void SendDivert(const C2Message& m, double now, C2Context& c, const std::string& why) {
        auto pr = Airborne(c, m.package_id);
        if (pr.first == nullptr) return;
        C2Message d = m;
        d.kind = C2Kind::Divert;
        d.from = m.post >= 0 ? posts_[static_cast<std::size_t>(m.post)].id : m.from;
        d.to = pr.first->flights[static_cast<std::size_t>(m.flight)].callsign;
        const int by = m.post;
        d.post = -1;
        d.text = why.empty() ? "divert to the reported place" : "divert to the reported place (" + why + ")";
        Send(d, "order", now, Duty(by), c);
    }

    // The on-call mission for a report and a pool's type.
    MissionKind OnCallKind(const C2Message& m, const AlertPool& pool) const {
        const FactionDef& f = d_->faction;
        if (m.what == "troops") return MissionKind::Cas;
        if (m.what == "launch") {
            const SquadronDef* sd = f.Squadron(pool.squadron);
            const AircraftType* a = sd != nullptr ? f.Aircraft(sd->aircraft) : nullptr;
            if (a != nullptr && a->Has(FlightRole::Sead) && a->arms > 0 && f.doctrine.packages[static_cast<int>(MissionKind::Hunter)].allowed) {
                return MissionKind::Hunter;
            }
            if (f.doctrine.packages[static_cast<int>(MissionKind::Dead)].allowed) return MissionKind::Dead;
        }
        return MissionKind::Strike;
    }
    std::string MissionName(MissionKind mk, const Vec3& place) const {
        if (mk == MissionKind::Strike) {
            const FrontDef* front = d_->theatre.FrontFor(d_->faction.front);
            if (front != nullptr && DistanceToLine(front->line, place) <= d_->tuning.Get("c2.bai_depth_m")) return "bai";
        }
        return MissionKindName(mk);
    }

    // 3. An alert pool within authority: approved (where the faction approves within an allocation) and scrambled. The
    // pool quickest to take off first.
    bool ScrambleFor(const C2Message& m, C2Context& c, double now) {
        std::vector<std::size_t> order;
        for (std::size_t i = 0; i < pools_.size(); ++i) {
            if (pools_[i].Ready(now)) order.push_back(i);
        }
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return pools_[a].takeoff_s < pools_[b].takeoff_s; });
        for (std::size_t i : order) {
            const AlertPool& pool = pools_[i];
            const MissionKind mk = OnCallKind(m, pool);
            const std::string mission = MissionName(mk, m.where);
            const int scrambler = Authorizer("scramble", mission, m.where, c.phase);
            if (scrambler < 0) continue;
            int approver = -1;
            if (AnyLine("approve", mission)) {
                approver = Authorizer("approve", mission, m.where, c.phase);
                if (approver < 0) continue;  // outside every allocation
            }
            C2Message s = m;
            s.pool = pool.id;
            s.mission = mission;
            s.flight = static_cast<int>(mk);  // the on-call mission kind rides here to the pool
            if (approver >= 0 && approver != scrambler) {
                C2Message ask = s;
                ask.kind = C2Kind::Request;
                ask.from = posts_[static_cast<std::size_t>(m.post >= 0 ? m.post : approver)].id;
                ask.to = posts_[static_cast<std::size_t>(approver)].id;
                ask.post = approver;
                ask.decision = scrambler;
                ask.package_id = -1;
                ask.text = "approve";
                ++stats_.requests;
                Send(ask, "request", now, Duty(approver), c);
                return true;
            }
            s.kind = C2Kind::Scramble;
            s.from = posts_[static_cast<std::size_t>(scrambler)].id;
            s.to = "pool " + std::to_string(pool.id);
            s.post = -1;
            s.package_id = -1;
            s.text = "scramble";
            ++stats_.scrambles;
            Send(s, "scramble", now, Duty(scrambler), c);
            return true;
        }
        return false;
    }

    // The scramble at the pool's airfield: the on-call package built and committed if its expected loss passes the
    // post's threshold (the bold accept more; the loss-shy will not send aircraft at a SAM without SEAD).
    void Scrambled(const C2Message& m, C2Context& c, double now) {
        AlertPool* pool = nullptr;
        for (AlertPool& p : pools_) {
            if (p.id == m.pool) pool = &p;
        }
        if (pool == nullptr || !pool->Ready(now) || !c.on_call) {
            ++stats_.refusals;
            ToIntel(m, c);
            return;
        }
        const MissionKind mk = static_cast<MissionKind>(m.flight);
        IntelEntry e;
        if (c.believed) {
            if (const IntelEntry* b = c.believed(m.object_id)) e = *b;
        }
        e.key = m.object_id.empty() ? "c2-report-" + std::to_string(m.id) : m.object_id;
        e.object_id = m.object_id;
        e.ghost = m.object_id.empty();
        if (e.label.empty()) e.label = m.what == "troops" ? "troops the FAC sees" : (m.what == "column" ? "a column on the road" : "a reported launch");
        e.kind = m.object_kind;
        e.pos = m.where;
        e.sigma_m = m.sigma_m;
        e.observed_h = m.observed_h;
        e.mobile = m.mobile;
        e.exists = std::max(e.exists, 0.8);
        Package pk;
        const double takeoff_h = now + pool->takeoff_s / 3600.0;
        if (!c.on_call(mk, e, *pool, takeoff_h, pk) || pk.Aircraft() > pool->aircraft) {
            ++stats_.refusals;
            ToIntel(m, c);
            return;
        }
        const int by = PostIndex(m.from);
        const airwar::Officer* o = Duty(by);
        const double b = d_->tuning.Get("c2.trait_bound");
        double limit = d_->tuning.Get("c2.commit_risk") * (1.0 + 2.0 * b * ((o != nullptr ? o->def.traits.boldness : 0.5) - 0.5));
        const double risk = pk.expected_losses / std::max(1, pk.Aircraft());
        const bool shy = o != nullptr && o->HasQuirk("loss_shy") && e.IsSamRadar() && !pk.Has(FlightRole::Sead) && mk != MissionKind::Hunter;
        if (risk > limit || shy) {
            ++stats_.refusals;
            C2Message no = m;
            no.kind = C2Kind::Refusal;
            no.text = shy ? "not without SEAD" : "too dangerous for the alert";
            Send(no, "order", now, o, c, true);
            ToIntel(m, c);
            return;
        }
        pool->aircraft -= pk.Aircraft();
        pool->flown += pk.Aircraft();
        ++stats_.on_call_packages;
        stats_.on_call_sorties += pk.Aircraft();
        if (c.log) {
            c.log("decision", m.from,
                  "Scrambled " + std::to_string(pk.Aircraft()) + " from the " + pool->kind + " alert (" + pool->squadron + ") as '" + pk.name + "' on " +
                      e.label + ", reported " + std::to_string(static_cast<int>((now - m.observed_h) * 60.0 + 0.5)) + " min before.",
                  false);
        }
        c.launch(pk);
    }

    // 4. Nobody acts: the report goes to intelligence, there now (an in-flight report: minutes, not the debrief's hours).
    void ToIntel(const C2Message& m, C2Context& c) {
        if (!c.intel || m.what == "troops") return;
        IntelReport r;
        r.arrive_h = m.arrive_h;
        r.observed_h = m.observed_h;
        r.source = IntelSource::InFlightReport;
        r.object_id = m.object_id;
        r.ghost = m.object_id.empty();
        r.kind = m.object_kind;
        r.pos = m.where;
        r.sigma_m = m.sigma_m;
        r.exists = 0.8;
        if (c.believed) {
            if (const IntelEntry* b = c.believed(m.object_id)) {
                r.label = b->label;
                r.threat_radius_m = b->threat_radius_m;
                r.threat_lethality = b->threat_lethality;
                r.threat_low = b->threat_low;
                r.threat_medium = b->threat_medium;
                r.threat_radar = b->threat_radar;
                r.threat_channels = b->threat_channels;
                r.threat_max_target_mps = b->threat_max_target_mps;
            }
        }
        if (r.label.empty()) r.label = m.what == "column" ? "a column on the road" : "a SAM launch";
        r.note = "reported in flight";
        ++stats_.to_intel;
        c.intel(r);
    }

    // The pools' shifts: a new shift drawn when one ends inside the cycle; the pool stands down at the cycle's end.
    void Refresh(double t_h) {
        for (AlertPool& p : pools_) {
            if (p.stood_down || t_h < p.until_h) continue;
            const C2AlertDef* al = nullptr;
            for (const C2AlertDef& a : d_->faction.c2.alert) {
                if (a.id == p.kind) al = &a;
            }
            if (al != nullptr && p.until_h < p.cycle_end_h - 1e-9 && p.aircraft > 0) {
                ++p.shift;
                Rng r = Draw(0xA1E47000ull + static_cast<std::uint64_t>(p.id), static_cast<std::uint64_t>(p.shift));
                p.begin_h = p.until_h;
                p.until_h = std::min(p.cycle_end_h, p.until_h + std::max(0.05, r.Uniform(al->hold_h.lo, al->hold_h.hi)));
                continue;
            }
            p.stood_down = true;
            if (p.flown == 0) ++stats_.pools_stood_down;
        }
    }

    static std::vector<C2Message> Take(std::map<int, std::vector<C2Message>>& m, int package_id) {
        std::vector<C2Message> out;
        const auto it = m.find(package_id);
        if (it == m.end()) return out;
        out = it->second;
        m.erase(it);
        return out;
    }
};

}  // namespace airwar
}  // namespace simcore
