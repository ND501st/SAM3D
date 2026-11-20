// The command live in the session (roadmap S2, docs/air_battle/05_C2_AND_RETASKING.md section 9): the posts' orders on a
// raid a session flies (air_war/c2.hpp's session orders, package_injector.hpp), recalls, posts struck and set up again,
// the nomination cycle (planner.hpp), our side's jamming and fires on the enemy's command (counter_c2.hpp), the FAC's mark
// and the strike's clearance, a post's order by radio taken only if heard (flights.hpp), and the save.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/air_war/package_control.hpp"
#include "simcore/em/data.hpp"
#include "simcore/flights.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

AirWarData LoadWar(const std::string& faction) {
    AirWarData d;
    std::string e;
    if (!LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_" + faction + ".json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

// The tuned theatre undated, its parts of the day the buckets (light.mode 0, roadmap S8): the tests written on them
// before the theatre had a date, whose seeds and hours were picked on the buckets' dusk and dawn.
AirWarData Buckets(AirWarData d) {
    d.tuning.values["light.mode"] = 0.0;
    ApplyLightMode(d);
    return d;
}

// An AirC2 alone, as test_c2.cpp's: the posts read the data, the staff, the truth and where our battery listens.
struct Bench {
    AirWarData d;
    AirStaff staff;
    std::vector<ObjectState> truth;
    std::vector<Package> packages;
    AirC2 c2;
    std::vector<IntelReport> intel;
    std::vector<std::string> journal;
    IntelEntry believed;  // the enemy's picture of one target ("tgt")

    explicit Bench(AirWarData data, std::uint64_t root = 11) : d(std::move(data)) {
        AirCampaign c;
        SC_EXPECT(c.Init(d, 1));
        truth = c.Truth();
        Rng r(5);
        staff.Build(d, r, 0.0);
        Rng pr(9);
        c2.Init(d, staff, pr, Rng(root), 0.0);
        believed.key = "tgt";
    }
    C2Context Ctx() {
        C2Context cx;
        cx.data = &d;
        cx.truth = &truth;
        cx.staff = &staff;
        cx.packages = &packages;
        cx.battery = truth.empty() ? Vec3() : truth[0].pos;
        cx.intel = [this](const IntelReport& r) { intel.push_back(r); };
        cx.believed = [this](const std::string& key) -> const IntelEntry* { return key == believed.key ? &believed : nullptr; };
        cx.log = [this](const std::string&, const std::string&, const std::string& text, bool) { journal.push_back(text); };
        return cx;
    }
    // No jamming at all (ours is tested on its own): the posts' decisions alone.
    void Quiet() { c2.SetJam([](const C2Message&) { return 0.0; }); }
    const C2PostState* FirstOf(const std::string& kind) const {
        for (const C2PostState& p : c2.Posts()) {
            if (p.kind == kind) return &p;
        }
        return nullptr;
    }
    int CountOf(const std::string& kind) const {
        int n = 0;
        for (const C2PostState& p : c2.Posts()) n += p.kind == kind ? 1 : 0;
        return n;
    }
};

std::string StrikeType(const AirWarData& d) {
    for (const AircraftType& a : d.faction.aircraft) {
        if (a.Has(FlightRole::Strike) && !a.missile && !a.drone && a.InService(d.theatre.year)) return a.id;
    }
    return std::string();
}

// A one-flight package from 100 km west of `near` to 60 km east of it in an hour, its release at the end.
Package Through(const AirWarData& d, int id, MissionKind kind, FlightRole role, const Vec3& near, const Vec3& target) {
    Package p;
    p.id = id;
    p.name = "Test " + std::to_string(id);
    p.kind = kind;
    p.target_pos = target;
    p.target_key = "tgt";
    Flight fl;
    fl.callsign = "Rook " + std::to_string(id);
    fl.role = role;
    fl.aircraft = StrikeType(d);
    fl.count = 2;
    fl.profile = Profile::High;
    fl.aim_key = "tgt";
    RoutePoint a, b;
    a.t_h = 0.0;
    a.pos = near + Vec3{-100000.0, 0.0, 0.0};
    b.t_h = 1.0;
    b.pos = near + Vec3{60000.0, 0.0, 0.0};
    b.tag = "target";
    fl.route = {a, b};
    fl.release_index = 1;
    p.flights.push_back(fl);
    return p;
}

void Begin(Bench& b, Package& p, PackageRun& run) {
    static BatteryPosture posture;
    ResolveContext rc;
    rc.data = &b.d;
    rc.truth = &b.truth;
    rc.posture = &posture;
    rc.staff = &b.staff;
    Resolver().Begin(p, rc, run, Rng(3));
}

}  // namespace

SC_TEST("c2 live (S2): our side's plan against the enemy's command loads; its jammer is in the air radios' library; the thirds hold over many posts") {
    CounterC2Def k;
    std::string e;
    SC_EXPECT(LoadCounterC2File(kData + "/command/counter_c2.json", k, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(k.present && !k.jammers.empty());
    SC_EXPECT_NEAR(k.destroy + k.jam + k.listen, 1.0, 1e-9);
    SC_EXPECT_NEAR(k.df_bearing_rad, 3.5 * 3.14159265358979 / 180.0, 1e-9);  // fm_100_2_1_1984 p. 15-2
    em::EmLibrary lib;
    SC_EXPECT(em::LoadEmLibraryFile(kData + "/em/air_radios_1985.json", lib).ok);
    for (const CounterC2Jammer& j : k.jammers) {
        SC_EXPECT(lib.Jammer(j.em_jammer) >= 0);
        SC_EXPECT(j.from_year <= 1985);  // in the war's year
        SC_EXPECT(j.Covers(300e6) && j.Covers(130e6));  // NATO's UHF and the Pact's VHF air nets
    }
    // Every post gets one mark; over many campaigns' roots the shares come near the thirds.
    const AirWarData d = LoadWar("nato");
    int n = 0, destroy = 0, jam = 0;
    for (std::uint64_t root = 1; root <= 120; ++root) {
        Bench b(d, root);
        for (const C2PostState& p : b.c2.Posts()) {
            const std::string m = b.c2.Marked(p);
            SC_EXPECT(m == "destroy" || m == "jam" || m == "listen");
            SC_EXPECT(m == b.c2.Marked(p));  // the same each time asked
            ++n;
            destroy += m == "destroy" ? 1 : 0;
            jam += m == "jam" ? 1 : 0;
        }
    }
    std::printf("    %d posts: %.2f destroy, %.2f jam\n", n, destroy / static_cast<double>(n), jam / static_cast<double>(n));
    SC_EXPECT(std::fabs(destroy / static_cast<double>(n) - k.destroy) < 0.06);
    SC_EXPECT(std::fabs(jam / static_cast<double>(n) - k.jam) < 0.06);
    // Without S2 (a save made before it) there is no plan.
    AirWarData old = d;
    old.tuning.values["c2.live_mode"] = 0.0;
    Bench o(old);
    for (const C2PostState& p : o.c2.Posts()) SC_EXPECT(o.c2.Marked(p).empty());
}

SC_TEST("c2 live (S2): a post's divert of a raid a session flies becomes an order for its flight at the hour decided, where the plan has it") {
    const AirWarData d = LoadWar("nato");
    const FrontDef* front = d.theatre.FrontFor(d.faction.front);
    SC_EXPECT(front != nullptr && front->line.size() >= 2);
    if (front == nullptr || front->line.size() < 2) return;
    int done = 0;
    for (std::uint64_t root = 11; root < 23; ++root) {
        Bench b(d, root);
        b.Quiet();
        const C2PostState* asoc = b.FirstOf("support_centre");
        SC_EXPECT(asoc != nullptr);
        if (asoc == nullptr) return;
        const Vec3 place = asoc->at + Vec3{10000.0, 0.0, 0.0};
        b.packages.push_back(Through(b.d, 50, MissionKind::Strike, FlightRole::Strike, asoc->at, front->line[1]));
        C2Context cx = b.Ctx();
        cx.live.push_back(0);  // a session flies it: no run here, only its plan
        b.c2.Report(b.packages[0], 0, "launch", "", ObjectKind::SamSite, true, place, 700.0, 0.5, place, Profile::High, 0.5, cx);
        for (double t = 0.5; t < 1.0; t += 1.0 / 120.0) b.c2.StepTo(t, cx);
        const std::vector<C2SessionOrder> orders = b.c2.TakeSessionOrders();
        if (orders.empty()) continue;  // the duty officer waited for a second report, or the hop was jammed
        SC_EXPECT(orders.size() == 1);
        const C2SessionOrder& o = orders[0];
        SC_EXPECT(o.kind == "divert" && o.package_id == 50 && o.flight == 0);
        SC_EXPECT(b.c2.Post(o.by) != nullptr);  // a post's order, by its id
        SC_EXPECT(o.at_h > 0.5 && o.at_h < 1.0);
        SC_EXPECT(Dist2d(o.where, place) < 1.0);
        SC_EXPECT(b.c2.TakeSessionOrders().empty());  // taken once
        // The campaign's own package stays as planned: the session flies the divert.
        SC_EXPECT(Dist2d(b.packages[0].flights[0].aim_pos, place) > 1.0);
        ++done;
    }
    std::printf("    %d of 12 roots diverted the session's flight\n", done);
    SC_EXPECT(done > 0);
    // A flight past its release by the plan is not diverted.
    Bench late(d);
    late.Quiet();
    const C2PostState* asoc = late.FirstOf("support_centre");
    if (asoc == nullptr) return;
    late.packages.push_back(Through(late.d, 51, MissionKind::Strike, FlightRole::Strike, asoc->at, front->line[1]));
    C2Context cx = late.Ctx();
    cx.live.push_back(0);
    const Vec3 place = asoc->at + Vec3{10000.0, 0.0, 0.0};
    late.c2.Report(late.packages[0], 0, "launch", "", ObjectKind::SamSite, true, place, 700.0, 1.05, place, Profile::High, 1.05, cx);
    for (double t = 1.05; t < 1.6; t += 1.0 / 120.0) late.c2.StepTo(t, cx);
    SC_EXPECT(late.c2.TakeSessionOrders().empty());
}

SC_TEST("c2 live (S2): a strike whose target is believed destroyed since it took off is recalled before its release; a double tap is not") {
    for (const char* f : {"nato", "warsaw_pact", "neutral"}) {
        const AirWarData d = LoadWar(f);
        Bench b(d);
        b.Quiet();
        if (b.c2.Posts().empty()) continue;
        const FrontDef* front = d.theatre.FrontFor(d.faction.front);
        if (front == nullptr || front->line.empty()) continue;
        const Vec3 near = front->line[front->line.size() / 2];
        // In the air: recalled, its main flight turned for home with its load.
        b.packages.reserve(4);
        b.packages.push_back(Through(b.d, 60, MissionKind::Strike, FlightRole::Strike, near, near));
        PackageRun run;
        Begin(b, b.packages[0], run);
        b.believed.exists = 1.0;
        b.believed.function = 0.05;
        b.believed.claimed_destroyed_h = 0.2;  // claimed after it took off (0.0)
        C2Context cx = b.Ctx();
        cx.airborne.push_back({0, &run});
        b.c2.Recalls(0.3, cx);
        for (double t = 0.3; t < 0.7; t += 1.0 / 120.0) b.c2.StepTo(t, cx);
        const Flight& fl = b.packages[0].flights[0];
        std::printf("    %s: %d recalls\n", f, b.c2.Stats().recalls);
        SC_EXPECT(b.c2.Stats().recalls == 1);
        SC_EXPECT(fl.aborted && fl.release_index < 0);
        SC_EXPECT(std::find(run.out.aborted_flights.begin(), run.out.aborted_flights.end(), 0) != run.out.aborted_flights.end());
        const std::vector<RetaskRecord> rt = b.c2.TakeRetasks(60);
        SC_EXPECT(rt.size() == 1 && rt[0].kind == "recall" && b.c2.Post(rt[0].by) != nullptr);
        // Once only.
        b.c2.Recalls(0.4, cx);
        SC_EXPECT(b.c2.Stats().recalls == 1);
        // Flown by a session: the recall is an order for every flight of it.
        Bench s(d);
        s.Quiet();
        s.packages.push_back(Through(s.d, 61, MissionKind::Strike, FlightRole::Strike, near, near));
        s.believed = b.believed;
        C2Context sx = s.Ctx();
        sx.live.push_back(0);
        s.c2.Recalls(0.3, sx);
        for (double t = 0.3; t < 0.7; t += 1.0 / 120.0) s.c2.StepTo(t, sx);
        const std::vector<C2SessionOrder> o = s.c2.TakeSessionOrders();
        SC_EXPECT(o.size() == 1 && o[0].kind == "recall" && o[0].flight == -1 && o[0].package_id == 61);
        // A double tap (planned on a claim already in) and a target still believed working: no recall.
        for (int k = 0; k < 2; ++k) {
            Bench n(d);
            n.Quiet();
            n.packages.push_back(Through(n.d, 62, MissionKind::Strike, FlightRole::Strike, near, near));
            n.believed.claimed_destroyed_h = k == 0 ? -0.5 : 0.2;
            n.believed.function = k == 0 ? 0.05 : 0.9;
            C2Context nx = n.Ctx();
            nx.live.push_back(0);
            n.c2.Recalls(0.3, nx);
            for (double t = 0.3; t < 0.7; t += 1.0 / 120.0) n.c2.StepTo(t, nx);
            SC_EXPECT(n.c2.TakeSessionOrders().empty() && n.c2.Stats().recalls == 0);
        }
    }
}

SC_TEST("c2 live (S2): a struck post's traffic goes to its alternate late (NATO) or is lost (the Pact); it is set up again after its kind's time") {
    for (const char* f : {"nato", "warsaw_pact"}) {
        const AirWarData d = LoadWar(f);
        // A pilot's report from over a post of a kind with more than one (one per entry or per point of the line) goes to
        // that post; the first such post a report reaches is struck while the report is on its way.
        std::string id;
        C2Message sent;
        std::size_t tries = 0;
        for (std::size_t k = 0; id.empty() && k < 40; ++k) {
            Bench probe(d);
            if (k >= probe.c2.Posts().size()) break;
            const C2PostState& cand = probe.c2.Posts()[k];
            if (cand.kind == "squadron" || probe.CountOf(cand.kind) < 2) continue;
            ++tries;
            probe.Quiet();
            Package p = Through(probe.d, 70, MissionKind::Strike, FlightRole::Strike, cand.at, cand.at);
            C2Context px = probe.Ctx();
            probe.c2.Report(p, 0, "launch", "", ObjectKind::SamSite, true, cand.at, 700.0, 0.5, cand.at, Profile::High, 0.5, px);
            if (probe.c2.Log().empty()) continue;
            const C2Message& m = probe.c2.Log().back();
            if (m.lost || m.post < 0 || static_cast<std::size_t>(m.post) != k) continue;
            id = cand.id;
            sent = m;
        }
        std::printf("    %s: %zu posts tried, %s struck\n", f, tries, id.empty() ? "none" : id.c_str());
        SC_EXPECT(!id.empty());
        if (id.empty()) continue;
        Bench b(d);
        b.Quiet();
        const C2PostState* target = b.c2.Post(id);
        Package p = Through(b.d, 70, MissionKind::Strike, FlightRole::Strike, target->at, target->at);
        C2Context cx = b.Ctx();
        b.c2.Report(p, 0, "launch", "", ObjectKind::SamSite, true, target->at, 700.0, 0.5, target->at, Profile::High, 0.5, cx);
        SC_EXPECT(!b.c2.Log().empty() && b.c2.Log().back().post == sent.post);  // the same, keyed
        SC_EXPECT(b.c2.StrikePost(id, 0.5, "test", cx));
        SC_EXPECT(!b.c2.Post(id)->alive);
        SC_EXPECT(!b.c2.StrikePost(id, 0.5, "test", cx));  // out already
        b.c2.StepTo(sent.arrive_h + 1e-6, cx);
        if (d.faction.c2.post_loss.takeover == "alternate") {
            SC_EXPECT(b.c2.Stats().to_alternate == 1 && b.c2.Stats().lost_with_post == 0);
            SC_EXPECT(b.c2.Busy());  // late by the takeover
            const C2Band& tk = d.faction.c2.post_loss.takeover_s;
            b.c2.StepTo(sent.arrive_h + (tk.lo - 1.0) / 3600.0, cx);
            SC_EXPECT(b.c2.Busy());
            b.c2.StepTo(sent.arrive_h + (tk.hi + 1.0) / 3600.0, cx);
            SC_EXPECT(!b.c2.Busy());
        } else {
            SC_EXPECT(b.c2.Stats().lost_with_post == 1 && b.c2.Stats().to_alternate == 0);
            SC_EXPECT(!b.c2.Busy());
        }
        // Reports go to the posts still up meanwhile; the struck one is set up again within its kind's band.
        const C2PostState* dead = b.c2.Post(id);
        const auto it = d.faction.c2.post_loss.reconstitute_h.find(dead->kind);
        SC_EXPECT(it != d.faction.c2.post_loss.reconstitute_h.end());
        if (it == d.faction.c2.post_loss.reconstitute_h.end()) continue;
        SC_EXPECT(dead->back_h >= 0.5 + it->second.lo - 1e-9 && dead->back_h <= 0.5 + it->second.hi + 1e-9);
        const double back = dead->back_h;
        b.c2.StepTo(back - 0.01, cx);
        SC_EXPECT(!b.c2.Post(id)->alive);
        b.c2.StepTo(back + 0.01, cx);
        SC_EXPECT(b.c2.Post(id)->alive && b.c2.Stats().posts_back == 1);
        std::printf("    %s: %s struck at 0.5 h, back at %.1f h\n", f, id.c_str(), back);
    }
}

SC_TEST("c2 live (S2): our fires go in on a post marked for destruction only once our listeners have heard enough of it, and near enough to fix it") {
    AirWarData d = LoadWar("nato");
    d.tuning.values["c2.intercept"] = 1.0;
    d.tuning.values["c2.fac_request_per_h"] = 1.0;
    d.tuning.values["c2.post_strike_per_h"] = 1.0;
    d.faction.radio.opsec = 0.0;
    int tried = 0;
    for (std::uint64_t root = 1; root < 200 && tried < 3; ++root) {
        Bench probe(d, root);
        const C2PostState* fac = nullptr;
        for (const C2PostState& p : probe.c2.Posts()) {
            if (fac == nullptr && p.kind == "fac" && probe.c2.Marked(p) == "destroy") fac = &p;
        }
        if (fac == nullptr) continue;
        const std::string id = fac->id;
        for (const double footprint : {1e6, 1.0}) {
            d.tuning.values["c2.post_footprint_m"] = footprint;
            Bench b(d, root);
            C2Context cx = b.Ctx();
            cx.battery = b.c2.Post(id)->at + Vec3{4000.0, 0.0, 0.0};
            b.c2.OurFires(0.0, cx);  // nothing heard yet
            SC_EXPECT(b.c2.Post(id)->alive && b.journal.empty());
            int heard = 0;
            for (int h = 0; h < 24 && heard < 3; ++h) {
                b.c2.Hourly(static_cast<double>(h), cx);
                heard = 0;
                for (const C2Message& m : b.c2.Log()) heard += m.heard && m.from == id ? 1 : 0;
                if (heard < 3) {
                    b.c2.OurFires(static_cast<double>(h), cx);
                    SC_EXPECT(b.c2.Post(id)->alive);  // not fixed yet
                }
            }
            if (heard < 3) continue;  // this FAC saw none of our troops
            ++tried;
            b.c2.OurFires(30.0, cx);
            bool went_in = false;
            for (const std::string& j : b.journal) went_in = went_in || j.find("Our fires went in") != std::string::npos;
            SC_EXPECT(went_in);
            // A fix a few km out is hundreds of metres off (3.5 degrees): a footprint of a metre is missed.
            SC_EXPECT(b.c2.Post(id)->alive == (footprint < 10.0));
        }
        // Beyond direction finding's reach of our battery's area: never fixed.
        d.tuning.values["c2.post_footprint_m"] = 1e6;
        Bench far(d, root);
        C2Context fx = far.Ctx();
        fx.battery = far.c2.Post(id)->at + Vec3{d.counter_c2.df_reach_m + 5000.0, 0.0, 0.0};
        for (int h = 0; h < 24; ++h) {
            far.c2.Hourly(static_cast<double>(h), fx);
            far.c2.OurFires(static_cast<double>(h), fx);
        }
        SC_EXPECT(far.c2.Post(id)->alive);
    }
    std::printf("    %d FACs marked for destruction fixed and fired on\n", tried);
    SC_EXPECT(tried > 0);
}

SC_TEST("c2 live (S2): our jamming takes a share of the hops of a post marked for it, near our jammers and not far from them, never a post we listen to") {
    const AirWarData d = LoadWar("nato");
    int checked = 0;
    for (std::uint64_t root = 1; root < 60 && checked < 4; ++root) {
        Bench b(d, root);
        const Vec3 battery = b.truth.empty() ? Vec3() : b.truth[0].pos;
        const auto jammers = b.c2.OurJammers(battery);
        SC_EXPECT(!jammers.empty());
        if (jammers.empty()) return;
        const Vec3 jam_at = jammers[0].second;
        for (const C2PostState& p : b.c2.Posts()) {
            const std::string mark = b.c2.Marked(p);
            if (mark == "destroy") continue;
            C2Message m;
            m.kind = airwar::C2Kind::Divert;
            m.from = p.id;
            m.to = "Rook 1";
            m.net = b.c2.Def(p).net;
            m.where = jam_at + Vec3{3000.0, 0.0, 0.0};  // the flight over our jammer
            const double near = b.c2.OurJamShare(m, battery);
            m.where = jam_at + Vec3{0.0, 400000.0, 0.0};
            const double far = b.c2.OurJamShare(m, battery);
            if (mark == "listen") {
                SC_EXPECT(near == 0.0 && far == 0.0);
                continue;
            }
            ++checked;
            SC_EXPECT(near >= d.tuning.Get("c2.jam_weak_share") && near >= far);
            // A message the post sends far from our jammers but close to the receiver: nothing jammed out.
            m.where = p.at + Vec3{2000.0, 0.0, 0.0};
            if (Dist2d(m.where, jam_at) > 60000.0) SC_EXPECT(b.c2.OurJamShare(m, battery) == 0.0);
        }
    }
    SC_EXPECT(checked > 0);
}

SC_TEST("c2 live (S2): a BAI target learned in the war waits nomination_h before the planner strikes it; one known before the war and a deep one do not") {
    // The wars this was written on: B8's abstract fight (air_fight.mode 0). S3's moves which targets the recce learns, and
    // in these four wars only one in-war target falls in the BAI area even then.
    AirWarData nato = Buckets(LoadWar("nato"));
    nato.tuning.values["air_fight.mode"] = 0.0;
    const AirWarData pact = Buckets(LoadWar("warsaw_pact"));
    const double nom = nato.faction.c2.nomination_h;
    SC_EXPECT(nom == 48.0);  // gallant_knight_1983
    IntelEntry e;
    e.first_h = 30.0;
    const double bai = nato.tuning.Get("c2.bai_depth_m");
    for (double now = 30.0; now < 120.0; now += 7.0) {
        SC_EXPECT(NominationWaits(nato, e, bai * 0.5, now) == (now < 30.0 + nom));
        SC_EXPECT(!NominationWaits(nato, e, bai * 2.0, now));  // air interdiction: the ATO's own targets
        SC_EXPECT(!NominationWaits(pact, e, bai * 0.5, now));  // no nomination cycle
    }
    IntelEntry prior;
    prior.first_h = 0.0;  // the prior picture, delivered at the war's start
    SC_EXPECT(!NominationWaits(nato, prior, bai * 0.5, 5.0));
    AirWarData old = nato;
    old.tuning.values["c2.live_mode"] = 0.0;
    SC_EXPECT(!NominationWaits(old, e, bai * 0.5, 31.0));
    // NATO wars of five days: no strike the cycle plans goes for a target in the BAI area learned less than nomination_h
    // before (a target that drifted across the area's edge since is left out); some in-war targets were there to wait.
    const FrontDef* front = nato.theatre.FrontFor(nato.faction.front);
    SC_EXPECT(front != nullptr);
    if (front == nullptr) return;
    int learned = 0, planned = 0;
    for (unsigned seed = 3; seed <= 6; ++seed) {
        AirCampaign c;
        SC_EXPECT(c.Init(nato, seed));
        while (c.Now() < 5.0 * 24.0 - 1e-9) {
            if (c.Advance(5.0 * 24.0)) c.SkipPlayerMission();
        }
        for (const IntelEntry& t : c.Intel().entries) {
            if (t.first_h > 0.0 && !t.ghost && DistanceToLine(front->line, t.pos) <= bai) ++learned;
        }
        for (const Package& p : c.Packages()) {
            if (p.kind != MissionKind::Strike) continue;
            bool on_call = false;
            for (const std::string& r : p.rationale) on_call = on_call || r.find("on call") != std::string::npos;
            if (on_call) continue;  // an immediate request, not the cycle
            const IntelEntry* t = c.Intel().Find(p.target_key);
            if (t == nullptr || t->first_h <= 0.0 || DistanceToLine(front->line, t->pos) > bai - 5000.0) continue;
            ++planned;
            SC_EXPECT(p.FirstH() >= t->first_h + nom - 1e-9);
        }
    }
    std::printf("    four NATO wars of five days: %d targets learned in the war in the BAI area, %d strikes planned on them\n", learned, planned);
    SC_EXPECT(learned > 0);
}

SC_TEST("c2 live (S2): a close support strike whose FAC's brief is jammed out holds and goes home (NATO) or attacks on its plan (the Pact)") {
    for (const char* f : {"nato", "warsaw_pact"}) {
        AirWarData d = LoadWar(f);
        for (const double live : {1.0, 0.0}) {
            d.tuning.values["c2.live_mode"] = live;
            Bench b(d);
            C2Context cx = b.Ctx();
            b.c2.Hourly(10.0, cx);
            if (b.c2.Sightings().empty()) continue;
            const FacSighting s = b.c2.Sightings()[0];
            Package p = Through(b.d, 80, MissionKind::Cas, FlightRole::Strike, s.pos, s.pos);
            p.target_object = s.object_id;
            p.flights[0].aim_pos = s.pos + Vec3{5000.0, 0.0, 0.0};  // the plan's aim, from the last picture
            const Vec3 plan = p.flights[0].aim_pos;
            // Clear: briefed, aimed where the FAC saw them.
            b.c2.SetJam([](const C2Message&) { return 0.0; });
            Package clear = p;
            SC_EXPECT(b.c2.Brief(clear, 10.5, cx));
            SC_EXPECT(Dist2d(clear.flights[0].aim_pos, s.pos) < 1.0 && !clear.flights[0].aborted);
            // Jammed out.
            b.c2.SetJam([](const C2Message&) { return 1.0; });
            Package jammed = p;
            const bool briefed = b.c2.Brief(jammed, 10.6, cx);
            b.c2.SetJam(nullptr);
            const C2ClearanceDef& clr = d.faction.c2.clearance;
            if (live < 1.0) {  // before S2: the aim set whatever the net
                SC_EXPECT(briefed && Dist2d(jammed.flights[0].aim_pos, s.pos) < 1.0 && !jammed.flights[0].aborted);
                continue;
            }
            SC_EXPECT(!briefed);
            SC_EXPECT(Dist2d(jammed.flights[0].aim_pos, plan) < 1.0);  // never had the mark
            SC_EXPECT(jammed.flights[0].aborted == (clr.on_jammed != "attack"));
            if (clr.on_jammed == "hold_then_abort") SC_EXPECT(b.c2.Stats().clearance_holds == 1 && b.c2.Stats().clearance_aborts == 1);
            std::printf("    %s: jammed brief, %s\n", f, clr.on_jammed.c_str());
        }
    }
}

SC_TEST("c2 live (S2): a contact near the FAC's heard mark is designated: close support may attack it, and not one elsewhere") {
    const AirWarData d = LoadWar("nato");
    RaidManifest m;
    m.flight_ids = {"Rook 1", "Rook 2"};
    OpportunityRun run;
    run.Init(m, d, 1);
    run.Designate(0, Vec3{1000.0, 2000.0, 0.0}, 600.0, "fac@L2", 30.0);
    SC_EXPECT(run.Designated(0, Vec3{1300.0, 2200.0, 0.0}));
    SC_EXPECT(!run.Designated(0, Vec3{3000.0, 2000.0, 0.0}));  // too far from the mark
    SC_EXPECT(!run.Designated(1, Vec3{1000.0, 2000.0, 0.0}));  // another flight's
    // The rules: NATO's close support strikes what the controller designates.
    OppSituation s;
    s.mission = MissionKind::Cas;
    s.depth_km = 5.0;
    s.ease = 0.3;
    std::string why;
    const bool own = MayAttack(d.faction.opportunity, s, &why);
    s.designated = true;
    SC_EXPECT(MayAttack(d.faction.opportunity, s, &why));
    std::printf("    undesignated: %s; designated: %s\n", own ? "may" : "may not", why.c_str());
    if (d.faction.opportunity.cas != "attack") SC_EXPECT(!own);
}

SC_TEST("c2 live (S2): a post's order by radio is the flight's only if its leader hears it; our spot jammer on the post's net takes it out") {
    AirRadioDef def;
    def.on = true;
    def.faction = "nato";
    def.library_file = "em/air_radios_1985.json";
    SC_EXPECT(em::LoadEmLibraryFile(kData + "/" + def.library_file, def.library).ok);
    def.air_radio = "nato_air_uhf_am";
    def.ground_radio = "air_ground_uhf_am";
    def.freq_hz = 300e6;
    AirRadioPostDef crc;
    crc.id = "crc";
    crc.kind = "crc";
    crc.at = Vec3{0.0, 0.0, 0.0};
    crc.mast_m = 15.0;
    int clear_heard = 0, jammed_heard = 0;
    const int tries = 12;
    for (int k = 0; k < tries; ++k) {
        for (const bool jam : {false, true}) {
            Flights flights;
            std::vector<Aircraft> aircraft;
            AirRadio radio;
            std::string e;
            SC_EXPECT(radio.Init(def, GeoPoint::FromDegrees(47.0, 19.0, 0.0), nullptr, 100u + static_cast<unsigned>(k), &e));
            radio.SetFlights(&flights);
            flights.SetRadio(&radio);
            SC_EXPECT(radio.AddSpeakerPost(crc, &e) && radio.HasPost("crc") && radio.PostFreqHz("crc") > 0.0);
            const Vec3 at{40000.0 + 3000.0 * k, 0.0, 4000.0};
            FlightState f;
            f.id = "Rook";
            f.callsign = "Rook";
            Aircraft a;
            a.position = at;
            aircraft.push_back(a);
            FlightSlot s;
            s.target = 0;
            f.slots.push_back(s);
            const int fi = flights.Add(f);
            radio.AddFlight(fi, flights.All()[0], FlightRadioInfo(), {at});
            if (jam) SC_EXPECT(radio.JamPostNet("crc", "our_air_net_jammer", Vec3{at.x + 2000.0, at.y, 0.0}) != 0);
            const Flights::Lookup air = [&aircraft](int i) -> Aircraft* { return i == 0 ? &aircraft[0] : nullptr; };
            radio.Step(1.0, 1.0, air);
            FlightOrder o;
            o.kind = OrderKind::Orbit;
            o.has_aim = true;
            o.aim = at;
            o.by = "crc";
            o.by_radio = true;
            SC_EXPECT(flights.Order(fi, o, 1.0));  // said, either way
            const FlightState& fs = flights.All()[0];
            SC_EXPECT(!fs.orders.empty());
            const bool heard = !fs.orders.empty() && fs.orders.back().heard;
            SC_EXPECT(heard == (fs.order.kind == OrderKind::Orbit));  // taken only if heard
            (jam ? jammed_heard : clear_heard) += heard ? 1 : 0;
        }
    }
    std::printf("    a post's order heard %d of %d in clear air, %d of %d with our jammer 2 km from the flight\n", clear_heard, tries, jammed_heard, tries);
    SC_EXPECT(clear_heard > jammed_heard);
    SC_EXPECT(clear_heard >= tries / 2);
}

SC_TEST("c2 live (S2): a post struck is a recorded action: a save replays it; a save without c2_live_mode replays without S2") {
    const AirWarData d = LoadWar("warsaw_pact");
    AirCampaign c;
    SC_EXPECT(c.Init(d, 5));
    while (c.Now() < 24.0 - 1e-9) {
        if (c.Advance(24.0)) c.SkipPlayerMission();
    }
    std::string post;
    for (const C2PostState& p : c.C2().Posts()) {
        if (post.empty() && p.alive && p.kind == "radar_control") post = p.id;
    }
    SC_EXPECT(!post.empty());
    SC_EXPECT(c.StrikePost(post, "a strike on its radar"));
    SC_EXPECT(!c.C2().Post(post)->alive);
    while (c.Now() < 30.0 - 1e-9) {
        if (c.Advance(30.0)) c.SkipPlayerMission();
    }
    const std::string text = SaveCampaignJson(c, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json");
    SC_EXPECT(text.find("\"post_attack\"") != std::string::npos && text.find("\"c2_live_mode\"") != std::string::npos);
    AirCampaign l;
    std::string e;
    SC_EXPECT(LoadCampaignJson(text, kData, l, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(l.C2().Post(post) != nullptr && l.C2().Post(post)->alive == c.C2().Post(post)->alive);
    SC_EXPECT(l.Journal().size() == c.Journal().size());
    for (std::size_t i = 0; i < l.Journal().size() && i < c.Journal().size(); ++i) SC_EXPECT(l.Journal()[i].text == c.Journal()[i].text);
    SC_EXPECT(l.C2().Log().size() == c.C2().Log().size());
    // A save made before S2 has no key: it replays with S2 off.
    std::string old = text;
    const std::size_t k = old.find("\"c2_live_mode\"");
    if (k != std::string::npos) old.replace(k, 14, "\"c2_live_mode_x\"");
    AirCampaign o;
    SC_EXPECT(LoadCampaignJson(old, kData, o, &e));
    SC_EXPECT(o.Data().tuning.Get("c2.live_mode") == 0.0);
}

SC_TEST("c2 live (S2): an on-call package the posts launch near the battery while a session flies a raid comes to that session, whatever the pacing") {
    int came = 0, came_old = 0, launched = 0;
    for (const double live : {1.0, 0.0}) {
        AirWarData d = LoadWar("warsaw_pact");
        d.tuning.values["c2.live_mode"] = live;
        d.tuning.values["pacing.min_gap_h"] = 1000.0;  // after the first, no raid by the pacing
        for (unsigned seed = 1; seed <= 3; ++seed) {
            AirCampaign c;
            SC_EXPECT(c.Init(d, seed));
            c.Posture().emission = 0.1;
            c.Posture().relocate_every_h = 1000.0;
            while (!c.HasPlayerMission() && c.Now() < 6.0 * 24.0) c.Advance(6.0 * 24.0);
            if (!c.HasPlayerMission()) continue;
            const Package taken = *c.PlayerMission();
            c.TakePlayerMission();  // in a session from now on (never completed here)
            // Into the front's offensive, when the readiness pools are full size (as test_c2's scramble test).
            const auto take = [&](double until) {
                while (c.Now() < until - 1e-9) {
                    if (!c.Advance(until)) continue;
                    bool on_call = false;
                    for (const std::string& r : c.PlayerMission()->rationale) on_call = on_call || r.find("on call") != std::string::npos;
                    SC_EXPECT(on_call);  // the pacing lets nothing else come
                    (live >= 1.0 ? came : came_old) += 1;
                    c.TakePlayerMission();
                }
            };
            take(std::max(c.Now(), 62.0));
            const C2PostState* pua = nullptr;
            for (const C2PostState& sp : c.C2().Posts()) {
                if (sp.kind == "support_centre" && pua == nullptr) pua = &sp;
            }
            SC_EXPECT(pua != nullptr);
            if (pua == nullptr) continue;
            const Vec3 post_at = pua->at;
            // The battery moved up near a division's element; its pilots report a launch 10 km from the element each hour
            // for a day: a scramble within the allocation is over a place a few km from the battery.
            const Vec3 at = post_at + Vec3{6000.0, -8000.0, 0.0};
            c.RelocateBattery(at + Vec3{3000.0, 0.0, 0.0});
            const std::size_t before = c.Packages().size();
            for (int h = 0; h < 24; ++h) {
                c.NoteReport(taken.id, 0, "launch", at, 700.0, c.Now(), post_at);
                take(c.Now() + 1.0);
            }
            for (std::size_t i = before; i < c.Packages().size(); ++i) {
                bool on_call = false;
                for (const std::string& r : c.Packages()[i].rationale) on_call = on_call || r.find("on call") != std::string::npos;
                if (on_call && live >= 1.0 && Dist2d(c.Packages()[i].target_pos, at) < 1000.0) ++launched;
            }
        }
    }
    std::printf("    on-call packages launched at the reports beside the battery: %d; to the session: %d with S2, %d before it\n", launched, came, came_old);
    SC_EXPECT(launched > 0);
    SC_EXPECT(came >= launched && came_old == 0);
}

SC_TEST("c2 live (S2): a raid in a session: the campaign's posts speak on its radio, our jammer stands on the nets of the posts marked for it, and every post's order said there is counted as heard or not; with live_mode 0 none of it") {
    for (const char* f : {"nato", "warsaw_pact"}) {
        for (const double live_mode : {1.0, 0.0}) {
            AirWarData d = LoadWar(f);
            d.tuning.values["c2.live_mode"] = live_mode;
            AirCampaign c;
            SC_EXPECT(c.Init(d, 2));
            c.Posture().emission = 0.1;
            c.Posture().relocate_every_h = 20.0;
            ScenarioFormat fmt;
            fmt.air_radio = true;  // the enemy's calls on the air (B9)
            RaidScenario rs;
            while (c.Now() < 6.0 * 24.0 && !rs.ok) {
                if (!c.Advance(6.0 * 24.0)) continue;
                RaidScenarioBuilder builder;
                rs = builder.Build(c, *c.PlayerMission(), fmt);
                if (!rs.ok) c.SkipPlayerMission();
            }
            SC_EXPECT(rs.ok);
            if (!rs.ok) continue;
            ScenarioDef def;
            ConsoleData console;
            std::string e;
            SC_EXPECT(LoadScenario(rs.json_text, kData, def, console, &e));
            def.targets.clear();
            def.triggers.clear();
            def.em.jammers.clear();
            def.time_limit_s = 2400.0;
            def.end_when_targets_gone = false;
            ScenarioRunner r(std::move(def), std::move(console));
            LivePackages lp(c, kData, c.Now(), fmt);  // takes the raid waiting into the session: the posts know its plan
            while (!r.Finished() && r.Time() < 2400.0) {
                r.Step(0.1);
                lp.Step(r);
                if (!lp.PlayedPackages().empty() && !lp.Active()) break;
            }
            AirRadio* radio = r.AirRadio();
            int posts = 0, on_air = 0, jam_marked = 0, jammed_nets = 0;
            for (const C2PostState& p : c.C2().Posts()) {
                if (p.kind == "squadron") continue;
                ++posts;
                on_air += radio != nullptr && radio->HasPost(p.id) ? 1 : 0;
                jam_marked += c.C2().Marked(p) == "jam" ? 1 : 0;
            }
            for (const std::string& n : lp.Notes()) jammed_nets += n.find("on the net of") != std::string::npos ? 1 : 0;
            int orders = 0, unheard = 0, marks = 0;
            for (const LivePackages::Played& pl : lp.PlayedPackages()) {
                orders += pl.orders;
                unheard += pl.orders_unheard;
                marks += pl.marks + pl.marks_unheard;
            }
            int noted_unheard = 0;
            for (const LivePackages::OrderNote& n : lp.OrdersGiven()) noted_unheard += n.heard ? 0 : 1;
            std::printf("    %s, live_mode %.0f: %d of %d posts on the air, %d marked for jamming, %d nets jammed; %d orders (%d unheard), %d FAC marks\n", f,
                        live_mode, on_air, posts, jam_marked, jammed_nets, orders, unheard, marks);
            SC_EXPECT(orders == static_cast<int>(lp.OrdersGiven().size()) && unheard == noted_unheard);
            if (live_mode < 1.0) {
                SC_EXPECT(on_air == 0 && jammed_nets == 0 && orders == 0 && marks == 0);
                continue;
            }
            SC_EXPECT(radio != nullptr);
            SC_EXPECT(on_air == posts);
            SC_EXPECT(jammed_nets == jam_marked);  // our jammer covers both sides' air bands
        }
    }
}

