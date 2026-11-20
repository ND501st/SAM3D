// Command in the air (roadmap B5, docs/air_battle/05_C2_AND_RETASKING.md §7): the enemy's posts and their authority
// (air_war/c2.hpp, c2_def.hpp), the messages between them, the alert pools, who may order a session's flight
// (air_war/package_control.hpp), the time-stepped resolver (resolve.hpp, A0) and the campaign that no longer stands
// still while a session flies a raid.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/air_war/package_control.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

AirWarData LoadWar(const std::string& faction, const std::string& theatre = "theatre_pannonia.json") {
    AirWarData d;
    std::string e;
    if (!LoadAirWarData(kData, "air_war/" + theatre, "air_war/faction_" + faction + ".json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

// The tuned theatre undated, its parts of the day the buckets (light.mode 0, roadmap S8): the tests written on them
// before the theatre had a date, whose seeds and hours were picked on the buckets' dusk and dawn.
AirWarData Buckets(AirWarData d) {
    d.tuning.values["light.mode"] = 0.0;
    ApplyLightMode(d);
    return d;
}

const std::vector<std::string> kMissions = {"strike", "bai", "cas", "dead", "hunter", "recce", "armed_recce", "helo_raid", "fighters"};

// Every (post kind, act, mission, where) the faction's 1985 table allows.
std::vector<std::string> Allowed(const AirWarData& d) {
    std::vector<std::string> out;
    const std::vector<C2Where> wheres = {{false, false}, {true, false}, {false, true}, {true, true}};
    for (const std::string& k : C2PostKinds()) {
        for (const std::string& a : C2Acts()) {
            for (const std::string& m : kMissions) {
                for (const C2Where& w : wheres) {
                    if (C2Authority::May(d.faction.c2, k, a, m, w, d.theatre.year)) out.push_back(k + " " + a + " " + m);
                }
            }
        }
    }
    return out;
}

bool Has(const std::vector<std::string>& v, const std::string& s) {
    for (const std::string& x : v) {
        if (x == s) return true;
    }
    return false;
}

// A context for an AirC2 alone: the posts read the data, the staff, the truth (the FACs' eyes only) and where our
// battery listens; what goes to intelligence and the journal is kept.
struct Bench {
    AirWarData d;
    AirStaff staff;
    std::vector<ObjectState> truth;
    std::vector<Package> packages;
    AirC2 c2;
    std::vector<IntelReport> intel;
    std::vector<std::string> journal;

    // `root`: the posts' keyed draws (a test that needs many different decisions takes a different root for each case).
    explicit Bench(AirWarData data, std::uint64_t root = 11) : d(std::move(data)) {
        AirCampaign c;
        SC_EXPECT(c.Init(d, 1));
        truth = c.Truth();
        Rng r(5);
        staff.Build(d, r, 0.0);
        Rng pr(9);
        c2.Init(d, staff, pr, Rng(root), 0.0);
    }
    C2Context Ctx() {
        C2Context cx;
        cx.data = &d;
        cx.truth = &truth;
        cx.staff = &staff;
        cx.packages = &packages;
        cx.battery = truth.empty() ? Vec3() : truth[0].pos;
        cx.intel = [this](const IntelReport& r) { intel.push_back(r); };
        cx.log = [this](const std::string&, const std::string&, const std::string& text, bool) { journal.push_back(text); };
        return cx;
    }
};

// A one-flight package for the posts to hear from.
Package OneFlight(const AirWarData& d, MissionKind kind) {
    Package p;
    p.id = 1;
    p.name = "Test";
    p.kind = kind;
    Flight fl;
    fl.callsign = "Rook 1";
    for (const AircraftType& a : d.faction.aircraft) {
        if (a.Has(FlightRole::Strike) && !a.missile && !a.drone) {
            fl.aircraft = a.id;
            break;
        }
    }
    fl.count = 2;
    fl.profile = Profile::High;
    RoutePoint a, b;
    a.t_h = 0.0;
    b.t_h = 1.0;
    b.pos = Vec3{50000.0, 0.0, 0.0};
    fl.route = {a, b};
    p.flights.push_back(fl);
    return p;
}

}  // namespace

SC_TEST("c2 (B5): the 1985 table: no NATO divert of a deep strike, the E-3A only warns, the neutral warns, scrambles fighters and recalls, the rebels nothing") {
    // (Roadmap S3: a controller vectoring fighters in the air fight is its own act, `vector`, read only by the fight.)
    for (const char* f : {"nato", "warsaw_pact", "neutral", "rebels"}) {
        const AirWarData d = LoadWar(f);
        SC_EXPECT(d.faction.c2.present);
        SC_EXPECT(d.theatre.year == 1985);
        const std::vector<std::string> allowed = Allowed(d);
        for (const std::string& k : C2PostKinds()) {
            SC_EXPECT(!Has(allowed, k + " divert_airborne strike") || std::string(f) == "warsaw_pact");
        }
        for (const std::string& a : allowed) {
            if (a.rfind("airborne_warning ", 0) == 0) SC_EXPECT(a.find(" warn ") != std::string::npos || a == "airborne_warning scramble fighters" || a == "airborne_warning vector fighters");
            if (std::string(f) == "neutral") {
                SC_EXPECT(a.find(" warn ") != std::string::npos || a.find(" recall ") != std::string::npos || a.find(" scramble fighters") != std::string::npos ||
                          a.find(" vector fighters") != std::string::npos);
            }
        }
        if (std::string(f) == "rebels") SC_EXPECT(allowed.empty() && d.faction.c2.posts.empty());
        if (std::string(f) == "nato") {
            SC_EXPECT(Has(allowed, "tasking divert_airborne bai"));
            SC_EXPECT(!Has(allowed, "tasking divert_airborne strike"));
            SC_EXPECT(Has(allowed, "support_centre divert_airborne cas"));
        }
        // Nothing later than the war's year is in force, and every line and post has its source.
        for (const C2AuthorityDef& a : d.faction.c2.authority) {
            SC_EXPECT(!a.source.empty() && !a.confidence.empty());
            if (a.from_year > d.theatre.year) SC_EXPECT(!C2Authority::InForce(a, d.theatre.year));
        }
        Bench b(d);
        for (const C2PostDef& p : d.faction.c2.posts) {
            SC_EXPECT(!p.source.empty() && !p.confidence.empty());
            bool placed = false;
            for (const C2PostState& s : b.c2.Posts()) placed = placed || d.faction.c2.posts[static_cast<std::size_t>(s.def)].id == p.id;
            SC_EXPECT(placed == (p.from_year <= d.theatre.year));  // the A-50 stays out of the 1985 war
        }
    }
    // A strike within c2.bai_depth_m of the front is BAI, beyond it AI.
    const AirWarData nato = LoadWar("nato");
    const FrontDef* front = nato.theatre.FrontFor(nato.faction.front);
    SC_EXPECT(front != nullptr && !front->line.empty());
    if (front == nullptr || front->line.empty()) return;
    Package p;
    p.kind = MissionKind::Strike;
    p.target_pos = front->line[0];
    SC_EXPECT(C2MissionOf(p, nato) == "bai");
    p.target_pos = front->line[0] + (front->line[0] - front->bases) * 10.0;
    SC_EXPECT(C2MissionOf(p, nato) == "strike");
}

SC_TEST("c2 (B5): a session's flight takes a post's order only by the table; its own command and the route always") {
    const AirWarData d = LoadWar("nato");
    const PackageControl::PostKind kind_of = [](const std::string& by) {
        if (by == "atoc") return std::string("tasking");
        if (by == "asoc@W1") return std::string("support_centre");
        if (by == "fac@L1") return std::string("fac");
        return std::string();
    };
    std::string mission = "bai";
    const PackageControl::MissionOf mission_of = [&mission](const FlightState&) { return mission; };
    const PackageControl::WhereOf where_of = [](const std::string&, const FlightState&, const FlightOrder&) { return C2Where{true, false}; };
    FlightState f;
    f.id = "Rook1";
    FlightOrder o;
    const auto may = [&](OrderKind k, const std::string& by) {
        o.kind = k;
        o.by = by;
        return PackageControl::May(d.faction.c2, d.theatre.year, f, o, kind_of, mission_of, where_of);
    };
    SC_EXPECT(may(OrderKind::FollowRoute, "a stranger"));
    SC_EXPECT(may(OrderKind::Divert, "leader") && may(OrderKind::ReturnHome, "package") && may(OrderKind::Evade, "pilot"));
    SC_EXPECT(!may(OrderKind::Divert, "a stranger"));
    SC_EXPECT(may(OrderKind::Divert, "atoc"));       // BAI: the ATOC's alone (J-SAK)
    SC_EXPECT(may(OrderKind::ReturnHome, "atoc"));   // recall
    SC_EXPECT(!may(OrderKind::Divert, "asoc@W1"));   // the ASOC diverts CAS, not BAI
    mission = "strike";
    SC_EXPECT(!may(OrderKind::Divert, "atoc"));      // a deep strike is not diverted in the air
    mission = "cas";
    SC_EXPECT(may(OrderKind::Divert, "asoc@W1"));
    SC_EXPECT(may(OrderKind::Attack, "fac@L1"));     // the FAC designates for CAS
    SC_EXPECT(!may(OrderKind::ReturnHome, "fac@L1"));
    f.side = "ours";
    SC_EXPECT(may(OrderKind::Divert, "a stranger"));  // our own flights are not the enemy's table's
    // The rule on the flights themselves: a refused order changes nothing.
    f.side = "enemy";
    Flights fl;
    FlightSlot s;
    s.target = 0;
    f.slots.push_back(s);
    const int fi = fl.Add(f);
    mission = "strike";
    fl.SetAuthority(PackageControl::Rule(d, kind_of, mission_of, where_of));
    FlightOrder div;
    div.kind = OrderKind::Divert;
    div.by = "atoc";
    SC_EXPECT(!fl.Order(fi, div, 10.0));
    SC_EXPECT(fl.All()[0].orders.empty());
    mission = "bai";
    SC_EXPECT(fl.Order(fi, div, 10.0));
    SC_EXPECT(fl.All()[0].orders.size() == 1);
}

SC_TEST("c2 (B5): the stepped resolver walks every package to the same outcome as the atomic one, with the same draws") {
    const AirWarData d = LoadWar("nato");
    AirCampaign c;
    SC_EXPECT(c.Init(d, 3));
    c.Advance(0.5);
    int compared = 0;
    for (const Package& planned : c.Packages()) {
        if (planned.resolved || planned.played || planned.flights.empty()) continue;
        Package pa = planned, ps = planned;
        std::vector<ObjectState> ta = c.Truth(), ts = c.Truth();
        BatteryPosture posture = c.Posture();
        ResolveContext ra, rs;
        for (ResolveContext* r : {&ra, &rs}) {
            r->data = &d;
            r->posture = &posture;
            r->staff = &c.Staff();
        }
        ra.truth = &ta;
        rs.truth = &ts;
        Rng rng(77);
        ra.rng = &rng;
        const PackageOutcome oa = Resolver().Resolve(pa, ra);
        PackageRun run;
        Resolver res;
        res.Begin(ps, rs, run, Rng(77));
        for (double t = ps.FirstH(); !run.Done(ps) && t < ps.LastH() + 2.0; t += 1.0 / 12.0) res.StepTo(ps, rs, run, t);
        res.StepTo(ps, rs, run, 1e18);
        const PackageOutcome& os = run.out;
        SC_EXPECT(oa.Lost() == os.Lost());
        SC_EXPECT(oa.delivered == os.delivered && oa.aborted == os.aborted);
        SC_EXPECT(oa.damage.size() == os.damage.size());
        for (std::size_t i = 0; i < ta.size(); ++i) SC_EXPECT_NEAR(ta[i].function, ts[i].function, 1e-12);
        ++compared;
    }
    SC_EXPECT(compared >= 3);
}

SC_TEST("c2 (B5): a jammed hop is said again after c2.retry_s or lost; a call near our battery is heard and written down") {
    AirWarData d = LoadWar("warsaw_pact");
    d.tuning.values["c2.intercept"] = 1.0;
    d.faction.radio.opsec = 0.0;
    Bench b(d);
    SC_EXPECT(!b.c2.Posts().empty());
    if (b.c2.Posts().empty()) return;
    Package p = OneFlight(b.d, MissionKind::Strike);
    const Vec3 from = b.c2.Posts()[0].at;  // over a post: in its cover
    C2Context cx = b.Ctx();
    cx.battery = from + Vec3{10000.0, 0.0, 0.0};
    // Clear air: one try, heard (our listeners 10 km away), in intelligence minutes later.
    SC_EXPECT(b.c2.Report(p, 0, "launch", "", ObjectKind::SamSite, true, from, 700.0, 0.5, from, Profile::High, 0.5, cx));
    SC_EXPECT(b.c2.Log().size() == 1 && b.c2.Log()[0].tries <= 1 && !b.c2.Log()[0].lost && b.c2.Log()[0].heard);
    SC_EXPECT(!b.journal.empty() && b.journal.back().find("Radio intercept") != std::string::npos);
    b.c2.StepTo(1.0, cx);
    SC_EXPECT(b.c2.Stats().to_intel >= 1 && !b.intel.empty());
    if (!b.intel.empty()) {
        SC_EXPECT(b.intel[0].source == IntelSource::InFlightReport);
        SC_EXPECT_NEAR(b.intel[0].observed_h, 0.5, 1e-12);  // the report keeps when he saw it
        SC_EXPECT(b.intel[0].arrive_h > 0.5 && b.intel[0].arrive_h < 0.5 + 10.0 / 60.0);
    }
    // Jammed out: said again each c2.retry_s, lost after c2.max_retries.
    b.c2.SetJam([](const C2Message&) { return 1.0; });
    p.id = 2;
    b.c2.Report(p, 0, "launch", "", ObjectKind::SamSite, true, from, 700.0, 1.5, from, Profile::High, 1.5, cx);
    const C2Message& lost = b.c2.Log().back();
    SC_EXPECT(lost.lost && lost.tries == static_cast<int>(d.tuning.Get("c2.max_retries") + 0.5) && !lost.heard);
    // Half jammed: some tries late by c2.retry_s each.
    b.c2.SetJam([](const C2Message&) { return 0.5; });
    int late = 0;
    for (int k = 0; k < 20; ++k) {
        p.id = 10 + k;
        b.c2.Report(p, 0, "launch", "", ObjectKind::SamSite, true, from, 700.0, 2.0, from, Profile::High, 2.0, cx);
        const C2Message& m = b.c2.Log().back();
        if (!m.lost && m.tries > 1) {
            ++late;
            SC_EXPECT((m.arrive_h - m.sent_h) * 3600.0 >= (m.tries - 1) * d.tuning.Get("c2.retry_s") - 1e-6);
        }
    }
    SC_EXPECT(late > 0);
    // Out of every post's cover: no report in flight (it waits for the debrief).
    const int out = b.c2.Stats().out_of_cover;
    SC_EXPECT(!b.c2.Report(p, 0, "launch", "", ObjectKind::SamSite, true, from, 700.0, 2.0, from + Vec3{1e7, 0.0, 0.0}, Profile::Low, 2.0, cx));
    SC_EXPECT(b.c2.Stats().out_of_cover == out + 1);
}

SC_TEST("c2 (B5): the FAC briefs close support where he saw our troops, with its age, never where they are now") {
    for (const char* f : {"nato", "warsaw_pact"}) {
        Bench b(LoadWar(f));
        C2Context cx = b.Ctx();
        b.c2.Hourly(10.0, cx);
        SC_EXPECT(!b.c2.Sightings().empty());
        if (b.c2.Sightings().empty()) continue;
        const FacSighting s = b.c2.Sightings()[0];
        SC_EXPECT_NEAR(s.observed_h, 10.0, 1e-12);
        const int oi = b.d.theatre.IndexOf(s.object_id);
        SC_EXPECT(oi >= 0);
        if (oi < 0) continue;
        b.truth[static_cast<std::size_t>(oi)].pos += Vec3{5000.0, 3000.0, 0.0};  // they moved since
        Package p = OneFlight(b.d, MissionKind::Cas);
        p.target_object = s.object_id;
        p.target_key = "troops-key";
        p.flights[0].aim_key = "troops-key";
        SC_EXPECT(b.c2.Brief(p, 10.5, cx));
        SC_EXPECT_NEAR(p.flights[0].aim_pos.x, s.pos.x, 1e-9);
        SC_EXPECT_NEAR(p.flights[0].aim_pos.y, s.pos.y, 1e-9);
        SC_EXPECT(Dist2d(p.flights[0].aim_pos, b.truth[static_cast<std::size_t>(oi)].pos) > 4000.0);
        // Before he saw them there is nothing to brief.
        Package early = p;
        SC_EXPECT(!b.c2.Brief(early, 9.0, cx));
    }
}

SC_TEST("c2 (B5): a mobile target reported longer ago than max_report_age_h is not struck without a fresh report") {
    Bench b(LoadWar("warsaw_pact"));
    if (b.c2.Posts().empty()) return;
    Package p = OneFlight(b.d, MissionKind::Strike);
    const Vec3 from = b.c2.Posts()[0].at;
    C2Context cx = b.Ctx();
    const double now = 20.0;
    const double old = now - b.d.faction.c2.max_report_age_h - 1.0;
    b.c2.Report(p, 0, "launch", "", ObjectKind::SamSite, true, from, 700.0, old, from, Profile::High, now, cx);
    b.c2.StepTo(now + 1.0, cx);
    SC_EXPECT(b.c2.Stats().stale == 1);
    SC_EXPECT(b.c2.Stats().scrambles == 0 && b.c2.Stats().diverts == 0);
}

SC_TEST("c2 (B5): over days of war the alert pools fly on call, no more than the held-back share; unused pools stand down; the Pact's opening share is small") {
    for (const char* f : {"warsaw_pact", "nato"}) {
        const AirWarData d = Buckets(LoadWar(f));
        double on_call = 0.0, sorties = 0.0, stood_down = 0.0;
        double pools_phase0 = 0.0, plans_phase0 = 0.0, pools_later = 0.0, plans_later = 0.0;
        for (unsigned seed = 1; seed <= 3; ++seed) {
            AirCampaign c;
            SC_EXPECT(c.Init(d, seed));
            int before = 0;
            for (double t = 12.0; t <= 4.0 * 24.0 + 1e-9; t += 12.0) {
                const int phase = c.Phase();
                while (c.Now() < t - 1e-9) {
                    if (c.Advance(t)) c.SkipPlayerMission();
                }
                const int now = c.C2().Stats().pool_aircraft;
                (phase == 0 ? pools_phase0 : pools_later) += now - before;
                (phase == 0 ? plans_phase0 : plans_later) += 1.0;
                before = now;
            }
            on_call += c.C2().Stats().on_call_sorties;
            stood_down += c.C2().Stats().pools_stood_down;
            for (const Package& p : c.Packages()) {
                if (!p.resolved) continue;
                for (const Flight& fl : p.flights) sorties += fl.count;
            }
            // Every report carries the hour it was seen, never later than it was sent.
            for (const C2Message& m : c.C2().Log()) {
                if (m.kind == airwar::C2Kind::InFlightReport) SC_EXPECT(m.observed_h <= m.sent_h + 1e-9);
            }
        }
        std::printf("    %s: %.0f on-call sorties of %.0f (%.1f%%), %.0f pools stood down unused; pool aircraft a half day %.1f in phase 1, %.1f later\n", f,
                    on_call, sorties, sorties > 0.0 ? 100.0 * on_call / sorties : 0.0, stood_down, plans_phase0 > 0.0 ? pools_phase0 / plans_phase0 : 0.0,
                    plans_later > 0.0 ? pools_later / plans_later : 0.0);
        SC_EXPECT(on_call > 0.0);
        SC_EXPECT(on_call <= d.faction.doctrine.on_call_fraction * sorties + 1e-9);
        SC_EXPECT(stood_down > 0.0);
        if (std::string(f) == "warsaw_pact" && plans_phase0 > 0.0 && plans_later > 0.0) {
            SC_EXPECT(pools_phase0 / plans_phase0 < pools_later / plans_later);
        }
    }
}

SC_TEST("c2 (B5): a Pact launch report within a division's allocation brings an on-call package from readiness 1, approved, then up in 3-5 min") {
    const AirWarData d = Buckets(LoadWar("warsaw_pact"));
    AirCampaign c;
    SC_EXPECT(c.Init(d, 1));
    // Into the front's offensive (phase 2), when the readiness pools are full size.
    while (c.Now() < 62.0 - 1e-9) {
        if (c.Advance(62.0)) c.SkipPlayerMission();
    }
    const C2PostState* pua = nullptr;
    for (const C2PostState& s : c.C2().Posts()) {
        if (s.kind == "support_centre" && pua == nullptr) pua = &s;
    }
    SC_EXPECT(pua != nullptr && !c.Packages().empty());
    if (pua == nullptr || c.Packages().empty()) return;
    const Vec3 post_at = pua->at;
    const std::string pua_id = pua->id;
    // A pilot over the division's element reports a launch 10 km from it (a package of the campaign's: its mission
    // reports in flight). Try a report each hour until a readiness 1 pool is standing.
    const Package* p = nullptr;
    double report_h = -1.0;
    for (int k = 0; k < 24 && p == nullptr; ++k) {
        report_h = c.Now();
        const Vec3 at = post_at + Vec3{6000.0, -8000.0, 0.0};
        c.NoteReport(c.Packages().front().id, 0, "launch", at, 700.0, report_h, post_at);
        const double until = report_h + 1.0;
        while (c.Now() < until - 1e-9) {
            if (c.Advance(until)) c.SkipPlayerMission();
        }
        for (const C2Message& m : c.C2().Log()) {
            if (m.kind != airwar::C2Kind::Scramble || m.what != "launch" || std::fabs(m.observed_h - report_h) > 1e-9) continue;
            for (const AlertPool& pool : c.C2().Pools()) {
                if (pool.id != m.pool || pool.kind != "readiness_1") continue;
                for (const Package& q : c.Packages()) {
                    if (q.rationale.empty() || q.rationale[0] != "on call from the alert") continue;
                    if (std::fabs(m.arrive_h + pool.takeoff_s / 3600.0 - q.FirstH()) > 1e-6) continue;
                    p = &q;
                    // Up 3-5 min after the scramble reached the pool; the approval came first, from the division's element.
                    SC_EXPECT(pool.takeoff_s >= 180.0 - 1e-9 && pool.takeoff_s <= 300.0 + 1e-9);
                    bool approved = false;
                    for (const C2Message& a : c.C2().Log()) {
                        if (a.kind == airwar::C2Kind::Request && a.text == "approve" && std::fabs(a.arrive_h - m.sent_h) < 1e-9) approved = approved || a.to == pua_id;
                    }
                    SC_EXPECT(approved);
                    const double over_min = (q.tot_h - report_h) * 60.0;
                    std::printf("    report %.2f h: scramble %.1f min after it, up %.1f min later, over the place %.0f min after the report\n", report_h,
                                (m.arrive_h - report_h) * 60.0, pool.takeoff_s / 60.0, over_min);
                    SC_EXPECT(over_min > 10.0 && over_min < 45.0);
                }
            }
        }
    }
    SC_EXPECT(p != nullptr);
    // The same report out of every post's cover goes in only at the debrief.
    const int out = c.C2().Stats().out_of_cover;
    c.NoteReport(c.Packages().front().id, 0, "launch", post_at, 700.0, c.Now(), post_at + Vec3{2e6, 0.0, 0.0});
    SC_EXPECT(c.C2().Stats().out_of_cover == out + 1);
}

namespace {

// An airborne package for the posts to act on: one flight of `role` from 100 km west of `near` to 60 km east of it in
// an hour, its run begun.
struct Airborne {
    Package p;
    PackageRun run;
};
void Fly(Bench& b, Airborne& a, MissionKind kind, FlightRole role, const std::string& aircraft, const Vec3& near, const Vec3& target, int arms) {
    a.p = OneFlight(b.d, kind);
    a.p.id = 40 + static_cast<int>(b.packages.size());
    a.p.target_pos = target;
    Flight& fl = a.p.flights[0];
    fl.role = role;
    fl.aircraft = aircraft;
    fl.arms = arms;
    fl.route[0].pos = near + Vec3{-100000.0, 0.0, 0.0};
    fl.route[1].pos = near + Vec3{60000.0, 0.0, 0.0};
    fl.route[1].tag = "target";
    fl.release_index = 1;
    static BatteryPosture posture;
    ResolveContext rc;
    rc.data = &b.d;
    rc.truth = &b.truth;
    rc.posture = &posture;
    rc.staff = &b.staff;
    Resolver().Begin(a.p, rc, a.run, Rng(3));
}

}  // namespace

SC_TEST("c2 (B5): NATO's CAS divert waits for the corps or its silence; a BAI divert does not; a Weasel near a launch is told and no strike is diverted") {
    const AirWarData d = LoadWar("nato");
    std::string strike_type;
    for (const AircraftType& a : d.faction.aircraft) {
        if (strike_type.empty() && a.Has(FlightRole::Strike) && !a.missile && !a.drone) strike_type = a.id;
    }
    const FrontDef* front = d.theatre.FrontFor(d.faction.front);
    SC_EXPECT(front != nullptr && front->line.size() >= 2);
    if (front == nullptr || front->line.size() < 2) return;
    int cas_done = 0, bai_done = 0;
    for (int attempt = 0; attempt < 6; ++attempt) {
        for (const MissionKind kind : {MissionKind::Cas, MissionKind::Strike}) {
            Bench b(d);
            const C2PostState* asoc = nullptr;
            for (const C2PostState& s : b.c2.Posts()) {
                if (s.kind == "support_centre" && asoc == nullptr) asoc = &s;
            }
            SC_EXPECT(asoc != nullptr);
            if (asoc == nullptr) return;
            const Vec3 place = asoc->at + Vec3{10000.0, 0.0, 0.0};
            Airborne a;
            b.packages.reserve(4);
            Fly(b, a, kind, FlightRole::Strike, strike_type, asoc->at, front->line[1], 0);
            a.p.id += attempt * 10;
            b.packages.push_back(a.p);
            C2Context cx = b.Ctx();
            cx.airborne.push_back({0, &a.run});
            Package& flying = b.packages[0];
            const std::string mission = C2MissionOf(flying, d);
            SC_EXPECT(mission == (kind == MissionKind::Cas ? "cas" : "bai"));
            b.c2.Report(flying, 0, "launch", "", ObjectKind::SamSite, true, place, 700.0, 0.5, place, Profile::High, 0.5, cx);
            for (double t = 0.5; t < 1.2; t += 1.0 / 120.0) b.c2.StepTo(t, cx);
            const C2Message* ask = nullptr;
            const C2Message* divert = nullptr;
            const C2Message* report = nullptr;
            for (const C2Message& m : b.c2.Log()) {
                if (m.kind == airwar::C2Kind::Request && m.to == "army") ask = &m;
                if (m.kind == airwar::C2Kind::Divert) divert = &m;
                if (m.kind == airwar::C2Kind::InFlightReport) report = &m;
            }
            if (divert == nullptr) continue;  // the duty officer waited for a second report, or the corps refused
            if (kind == MissionKind::Cas) {
                SC_EXPECT(ask != nullptr);
                if (ask != nullptr) {
                    SC_EXPECT(divert->sent_h > ask->sent_h);
                    SC_EXPECT(divert->sent_h <= ask->sent_h + d.faction.c2.silence_consent_s / 3600.0 + 1e-9);  // the corps' yes, or silence
                }
                ++cas_done;
            } else {
                SC_EXPECT(ask == nullptr);
                SC_EXPECT(report != nullptr && std::fabs(divert->sent_h - report->arrive_h) < 1e-9);  // at once
                ++bai_done;
            }
            // The flight flies to the reported place.
            SC_EXPECT(b.c2.Stats().diverts == 1);
            SC_EXPECT(Dist2d(flying.flights[0].aim_pos, place) < 1.0);
        }
    }
    std::printf("    %d CAS and %d BAI diverts of 6 tries each\n", cas_done, bai_done);
    SC_EXPECT(cas_done > 0 && bai_done > 0);
    // A Weasel team near the launch is warned (its leader's own leave takes it there); no strike flight is diverted to it.
    Bench b(d);
    const C2PostState* asoc = nullptr;
    for (const C2PostState& s : b.c2.Posts()) {
        if (s.kind == "support_centre" && asoc == nullptr) asoc = &s;
    }
    if (asoc == nullptr) return;
    const Vec3 place = asoc->at + Vec3{10000.0, 0.0, 0.0};
    Airborne weasel, strike;
    b.packages.reserve(4);
    Fly(b, weasel, MissionKind::Hunter, FlightRole::Hunter, "f4g", asoc->at, place, 4);
    b.packages.push_back(weasel.p);
    Fly(b, strike, MissionKind::Strike, FlightRole::Strike, strike_type, asoc->at, front->line[1], 0);
    b.packages.push_back(strike.p);
    C2Context cx = b.Ctx();
    cx.airborne.push_back({0, &weasel.run});
    cx.airborne.push_back({1, &strike.run});
    b.c2.Report(b.packages[1], 0, "launch", "", ObjectKind::SamSite, true, place, 700.0, 0.5, place, Profile::High, 0.5, cx);
    for (double t = 0.5; t < 1.2; t += 1.0 / 120.0) b.c2.StepTo(t, cx);
    SC_EXPECT(b.c2.Stats().warnings == 1 && b.c2.Stats().diverts == 0);
    std::vector<RetaskRecord> hunts = b.c2.TakeRetasks(b.packages[0].id);
    SC_EXPECT(hunts.size() == 1 && hunts[0].kind == "hunt" && hunts[0].by == "leader");
}

SC_TEST("c2 (B14): a Pact armed reconnaissance diverted in the air has its new task 10-30 min after the decision, not in the order's hops") {
    // Centralised control makes a change to an airborne flight slow (10_FACTIONS.md section 2): the faction's retask delay
    // (hot_phase.retask_air, 600-1800 s, mt_1962_kontsevoy and factions_cases) floors the order's hops (B5's c2.timing,
    // 60-210 s for an order); before B14 (balance.mode 0) the hops alone. Diverts are occasional (chance 0.2), so cases
    // (each with its own root for the posts' draws) are taken until a few are found.
    AirWarData d = LoadWar("warsaw_pact");
    std::string strike_type;
    for (const AircraftType& a : d.faction.aircraft) {
        if (strike_type.empty() && a.Has(FlightRole::Strike) && !a.missile && !a.drone && !a.helicopter) strike_type = a.id;
    }
    const C2Band& band = d.faction.hot.retask_air.delay_s;
    SC_EXPECT(band.Set() && band.lo >= 600.0 - 1e-9 && band.hi <= 1800.0 + 1e-9);
    int found = 0;
    for (int attempt = 0; attempt < 60 && found < 4; ++attempt) {
        double delay_min[2] = {-1.0, -1.0};
        for (int mode = 0; mode < 2; ++mode) {
            d.tuning.values["balance.mode"] = mode;
            Bench b(d, 1000u + static_cast<std::uint64_t>(attempt));
            const C2PostState* sq = nullptr;
            int seen = 0;
            for (const C2PostState& s : b.c2.Posts()) {
                if (s.kind == "squadron" && seen++ == attempt % 3) sq = &s;
            }
            if (sq == nullptr) {
                for (const C2PostState& s : b.c2.Posts()) {
                    if (s.kind == "squadron" && sq == nullptr) sq = &s;
                }
            }
            SC_EXPECT(sq != nullptr);
            if (sq == nullptr) return;
            const Vec3 place = sq->at + Vec3{10000.0, 0.0, 0.0};
            Airborne a;
            b.packages.reserve(4);
            Fly(b, a, MissionKind::ArmedRecce, FlightRole::Strike, strike_type, sq->at, sq->at + Vec3{60000.0, 0.0, 0.0}, 0);
            a.p.id += attempt * 10;
            b.packages.push_back(a.p);
            C2Context cx = b.Ctx();
            cx.airborne.push_back({0, &a.run});
            Package& flying = b.packages[0];
            SC_EXPECT(C2MissionOf(flying, d) == "armed_recce");
            b.c2.Report(flying, 0, "column", "", ObjectKind::Column, true, place, 700.0, 0.5, place, Profile::High, 0.5, cx);
            for (double t = 0.5; t < 1.4; t += 1.0 / 120.0) b.c2.StepTo(t, cx);
            const C2Message* divert = nullptr;
            const C2Message* report = nullptr;
            for (const C2Message& m : b.c2.Log()) {
                if (m.kind == airwar::C2Kind::Divert) divert = &m;
                if (m.kind == airwar::C2Kind::InFlightReport) report = &m;
            }
            if (divert == nullptr || report == nullptr) continue;
            SC_EXPECT(std::fabs(divert->sent_h - report->arrive_h) < 1e-9);  // decided on the report's arrival
            delay_min[mode] = (divert->arrive_h - divert->sent_h) * 60.0;
            if (b.c2.Stats().diverts == 1) SC_EXPECT(!b.c2.Stats().divert_min.empty());
        }
        // The decision's own draws are the same in both modes; B14 adds only the retask's, whose "never" can stop a divert.
        SC_EXPECT(delay_min[1] < 0.0 || delay_min[0] >= 0.0);
        if (delay_min[0] < 0.0 || delay_min[1] < 0.0) continue;
        ++found;
        std::printf("    divert: the order's hops %.1f min (before B14), the flight has it %.1f min after the decision\n", delay_min[0], delay_min[1]);
        SC_EXPECT(delay_min[0] < 6.0);
        SC_EXPECT(delay_min[1] >= band.lo * (1.0 - kHotOfficerBound) / 60.0 - 1e-6);
        SC_EXPECT(delay_min[1] <= band.hi * (1.0 + kHotOfficerBound) / 60.0 + 1e-6);
    }
    SC_EXPECT(found >= 1);
}

SC_TEST("c2 (B5): a package elsewhere flies and resolves while a raid taken by a session is in it; the clock goes on") {
    const AirWarData d = LoadWar("nato");
    AirCampaign c;
    SC_EXPECT(c.Init(d, 1));
    c.Posture().emission = 0.1;
    while (!c.HasPlayerMission() && c.Now() < 6.0 * 24.0) c.Advance(6.0 * 24.0);
    SC_EXPECT(c.HasPlayerMission());
    if (!c.HasPlayerMission()) return;
    const int id = c.PlayerMission()->id;
    const double taken = c.Now();
    SC_EXPECT(c.TakePlayerMission());
    SC_EXPECT(c.LiveMission(id) && !c.HasPlayerMission());
    // Hour by hour until a package that took off after the raid was taken has flown and been applied (a day at most).
    int elsewhere = 0;
    while (elsewhere == 0 && c.Now() < taken + 24.0 - 1e-9) {
        const double until = c.Now() + 1.0;
        while (c.Now() < until - 1e-9) {
            if (c.Advance(until)) c.SkipPlayerMission();
        }
        for (const Package& p : c.Packages()) elsewhere += p.id != id && p.resolved && p.FirstH() >= taken ? 1 : 0;
    }
    std::printf("    %d packages elsewhere resolved %.1f h after the raid was taken\n", elsewhere, c.Now() - taken);
    SC_EXPECT(c.Now() > taken + 1.0 - 1e-9);  // never stood still for it
    SC_EXPECT(elsewhere > 0);
    SC_EXPECT(c.LiveMission(id));
    PackageOutcome o;
    o.package_id = id;
    o.played = true;
    c.CompleteLiveMission(id, o, c.Now());
    SC_EXPECT(!c.LiveMission(id));
    const Package* p = c.PackageById(id);
    SC_EXPECT(p != nullptr && p->played);
}

SC_TEST("c2 (B5): a seed replays exactly with the posts on, and a save with a taken raid, its pilot's report and its end loads the same") {
    const AirWarData d = LoadWar("warsaw_pact");
    AirCampaign a, b;
    SC_EXPECT(a.Init(d, 4) && b.Init(d, 4));
    for (AirCampaign* c : {&a, &b}) {
        while (c->Now() < 3.0 * 24.0 - 1e-9) {
            if (c->Advance(3.0 * 24.0)) c->SkipPlayerMission();
        }
    }
    SC_EXPECT(a.Journal().size() == b.Journal().size());
    for (std::size_t i = 0; i < a.Journal().size() && i < b.Journal().size(); ++i) SC_EXPECT(a.Journal()[i].text == b.Journal()[i].text);
    SC_EXPECT(a.C2().Log().size() == b.C2().Log().size());
    SC_EXPECT(a.C2().Stats().reports == b.C2().Stats().reports);
    // A raid taken, a report from it while it flies, its end, and on: saved and loaded.
    AirCampaign c;
    SC_EXPECT(c.Init(d, 6));
    while (!c.HasPlayerMission() && c.Now() < 6.0 * 24.0) c.Advance(6.0 * 24.0);
    SC_EXPECT(c.HasPlayerMission());
    if (!c.HasPlayerMission()) return;
    const Package taken = *c.PlayerMission();
    c.TakePlayerMission();
    const Vec3 at = c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].pos + Vec3{400.0, -300.0, 0.0};
    const Vec3 from = at + Vec3{8000.0, 6000.0, 300.0};
    c.NoteReport(taken.id, 0, "launch", at, 700.0, c.Now(), from);
    while (c.Now() < taken.LastH() - 1e-9) {
        if (c.Advance(taken.LastH())) c.SkipPlayerMission();
    }
    PackageOutcome o;
    o.package_id = taken.id;
    o.played = true;
    c.CompleteLiveMission(taken.id, o, c.Now());
    while (c.Now() < taken.LastH() + 6.0 - 1e-9) {
        if (c.Advance(taken.LastH() + 6.0)) c.SkipPlayerMission();
    }
    const std::string text = SaveCampaignJson(c, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json");
    SC_EXPECT(text.find("\"take\"") != std::string::npos && text.find("\"report\"") != std::string::npos);
    SC_EXPECT(text.find("\"save_version\": 2") != std::string::npos);
    AirCampaign l;
    std::string e;
    SC_EXPECT(LoadCampaignJson(text, kData, l, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT_NEAR(l.Now(), c.Now(), 1e-9);
    SC_EXPECT(l.Journal().size() == c.Journal().size());
    for (std::size_t i = 0; i < l.Journal().size() && i < c.Journal().size(); ++i) SC_EXPECT(l.Journal()[i].text == c.Journal()[i].text);
    SC_EXPECT(l.C2().Log().size() == c.C2().Log().size());
}

SC_TEST("c2 (F2.4): the BAI area reaches a sector's air front further; at 0, and with no line, the base line's 30 km; free hunt does not move") {
    const AirWarData nato = LoadWar("nato");
    const FrontDef* front = nato.theatre.FrontFor(nato.faction.front);
    SC_EXPECT(front != nullptr);
    if (front == nullptr) return;
    LineOfContact line;
    line.Build(*front, nato.tuning.Get("air_front.station_m"));
    SC_EXPECT(line.SectorCount() >= 2);
    if (line.SectorCount() < 2) return;
    const double bai = nato.tuning.Get("c2.bai_depth_m");
    const double a0 = 0.5 * (line.Sector(0).from_m + line.Sector(0).to_m), a1 = 0.5 * (line.Sector(1).from_m + line.Sector(1).to_m);
    const auto at = [&](double along, double beyond) { return line.PointAt(along) + line.InwardNormal(along) * beyond; };
    Package p;
    p.kind = MissionKind::Strike;
    const std::vector<double> none, zero(line.SectorCount(), 0.0);
    std::vector<double> deep(line.SectorCount(), 0.0);
    deep[0] = 20000.0;
    AirFrontView off, flat, fwd;
    off.line = &line;
    off.depth_m = &none;
    flat = off;
    flat.depth_m = &zero;
    fwd = off;
    fwd.depth_m = &deep;
    // 10 km either side of the BAI area's edge, in both sectors: the same with no view, the view off and every sector at 0.
    for (double along : {a0, a1}) {
        for (double beyond : {bai - 10000.0, bai + 10000.0, bai + 25000.0}) {
            p.target_pos = at(along, beyond);
            const std::string plain = C2MissionOf(p, nato);
            SC_EXPECT(plain == (DistanceToLine(front->line, p.target_pos) <= bai ? "bai" : "strike"));
            SC_EXPECT(C2MissionOf(p, nato, off) == plain && C2MissionOf(p, nato, flat) == plain);
        }
    }
    // The first sector's air front at 20 km: 40 km in is battlefield interdiction there, air interdiction in the next.
    p.target_pos = at(a0, bai + 10000.0);
    SC_EXPECT(C2MissionOf(p, nato, fwd) == "bai");
    p.target_pos = at(a0, bai + 25000.0);
    SC_EXPECT(C2MissionOf(p, nato, fwd) == "strike");
    p.target_pos = at(a1, bai + 10000.0);
    SC_EXPECT(C2MissionOf(p, nato, fwd) == "strike");
    // Close support stays close support whatever the depth.
    p.kind = MissionKind::Cas;
    SC_EXPECT(C2MissionOf(p, nato, fwd) == "cas");
    // The posts: the Pact's free hunt zone is measured from the line of contact, not moved by the air front.
    Bench b(LoadWar("warsaw_pact"));
    const FrontDef* pf = b.d.theatre.FrontFor(b.d.faction.front);
    SC_EXPECT(pf != nullptr);
    if (pf == nullptr) return;
    LineOfContact pl;
    pl.Build(*pf, b.d.tuning.Get("air_front.station_m"));
    std::vector<double> pdeep(pl.SectorCount(), 20000.0);
    AirFrontView pv;
    pv.line = &pl;
    pv.depth_m = &pdeep;
    pv.helo_depth_m = &pdeep;
    const double hunt_m = b.d.faction.c2.free_hunt.depth_km * 1000.0;
    SC_EXPECT(hunt_m > 0.0);
    const double pa = 0.5 * (pl.Sector(0).from_m + pl.Sector(0).to_m);
    for (double beyond : {hunt_m - 5000.0, hunt_m + 5000.0}) {
        const Vec3 place = pl.PointAt(pa) + pl.InwardNormal(pa) * beyond;
        b.c2.Bind(b.d, b.staff);
        const bool plain = b.c2.FreeHunt(place, 1);
        b.c2.Bind(b.d, b.staff, pv);
        SC_EXPECT(b.c2.FreeHunt(place, 1) == plain);
        SC_EXPECT(plain == (beyond < hunt_m));
    }
}
