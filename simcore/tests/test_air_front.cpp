// What the enemy's air staff weighs by sector and what it decides (track F, F2.2-F2.3; simcore/air_war/air_front.hpp,
// docs/fronts/02_AIR_FRONT.md §3): the believed cover over a sector's next band against its pre-war baseline, our mobile
// air defence there, the losses by sector; the air front moved on them. Generic placeholder pictures, checked by hand.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/air_war/air_front.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

Vec3 At(double e_km, double n_km) { return Vec3{e_km * 1000.0, n_km * 1000.0, 0.0}; }

// A straight 100 km front along east, the enemy's bases south: our side is north (depth = north). Two sectors of 50 km.
LineOfContact StraightLine() {
    FrontDef f;
    f.faction = "test";
    f.name = "Test";
    f.line = {At(0, 0), At(100, 0)};
    f.bases = At(50, -100);
    SectorDef a;
    a.id = "a";
    a.name = "A";
    a.from_m = 0.0;
    a.to_m = 50000.0;
    SectorDef b = a;
    b.id = "b";
    b.name = "B";
    b.from_m = 50000.0;
    b.to_m = 100000.0;
    f.sectors = {a, b};
    LineOfContact l;
    l.Build(f, 5000.0);
    return l;
}

IntelEntry Sam(const std::string& key, const Vec3& pos, double radius_km, const std::string& system) {
    IntelEntry e;
    e.key = key;
    e.object_id = key;
    e.kind = ObjectKind::SamSite;
    e.pos = pos;
    e.threat_radius_m = radius_km * 1000.0;
    e.threat_lethality = 0.2;
    e.threat_radar = true;
    e.system = system;
    return e;
}

}  // namespace

SC_TEST("air front (F2.2): the cover over a sector's band by hand, against its pre-war baseline") {
    const LineOfContact l = StraightLine();
    IntelPicture pic;
    pic.entries.push_back(Sam("s1", At(25, 10), 10.0, "s125"));   // in A's band [0, 20 km]
    pic.entries.push_back(Sam("s2", At(25, 35), 10.0, "s75m2"));  // 35 km deep, its ring 15 km short of the band: not counted
    pic.entries.push_back(Sam("s3", At(25, 28), 10.0, "osa"));    // 28 km deep, its ring 2 km into the band
    pic.entries.push_back(Sam("s4", At(55, 5), 10.0, "kub"));     // B's, its ring 5 km into A's run: both
    IntelEntry decoy = Sam("s5", At(75, 10), 10.0, "osa");        // B's alone, half suspected a dummy
    decoy.suspected_decoy = 0.5;
    pic.entries.push_back(decoy);
    pic.entries[2].exists = 0.8;
    SectorCoverBook book;
    book.Start(pic, l);
    SC_EXPECT(book.PrewarUnits() == 5);
    // Since the war began: s3 half working, s4 believed destroyed.
    pic.entries[2].function = 0.5;
    pic.entries[3].function = 0.0;
    std::vector<SectorCover> out;
    const std::vector<double> at_border;  // every air front at the border
    book.Measure(pic, l, at_border, 20000.0, out);
    SC_EXPECT(out.size() == 2);
    // A: there 1 + 0.8 + 1 = 2.8 (the baseline the same), working 1 + 0.4 + 0 = 1.4.
    SC_EXPECT(out[0].sam_units == 3);
    SC_EXPECT_NEAR(out[0].sam_there, 2.8, 1e-12);
    SC_EXPECT_NEAR(out[0].sam_baseline, 2.8, 1e-12);
    SC_EXPECT_NEAR(out[0].cover, 0.5, 1e-12);
    SC_EXPECT_NEAR(out[0].near_m, 0.0, 1e-12);
    SC_EXPECT_NEAR(out[0].far_m, 20000.0, 1e-12);
    // B: s4 (there 1, working 0) and the suspected dummy (there and working 1 x (1 - 0.35) = 0.65).
    SC_EXPECT(out[1].sam_units == 2);
    SC_EXPECT_NEAR(out[1].cover, 0.65 / 1.65, 1e-12);
    // Our mobile air defence in A: s3 (Osa) and s4 (Kub), not the S-125: working 0.4 of 1.8 there.
    SC_EXPECT(out[0].mobile_units == 2);
    SC_EXPECT_NEAR(out[0].mobile_destroyed, 1.0 - 0.4 / 1.8, 1e-12);
    // s1 pulls out of the sector: A's cover falls as if it were destroyed (the baseline keeps it).
    pic.entries[0].pos = At(25, 60);
    book.Measure(pic, l, at_border, 20000.0, out);
    SC_EXPECT_NEAR(out[0].sam_there, 1.8, 1e-12);
    SC_EXPECT_NEAR(out[0].cover, 0.4 / 2.8, 1e-12);
    // Believed destroyed and believed gone are the same to the cover.
    pic.entries[0].pos = At(25, 10);
    pic.entries[0].function = 0.0;
    book.Measure(pic, l, at_border, 20000.0, out);
    SC_EXPECT_NEAR(out[0].cover, 0.4 / 2.8, 1e-12);
    // More found than the war began with: the baseline is raised to them.
    pic.entries.push_back(Sam("s6", At(10, 5), 10.0, "s125"));
    book.Measure(pic, l, at_border, 20000.0, out);
    SC_EXPECT_NEAR(out[0].sam_baseline, 3.8, 1e-12);
    SC_EXPECT_NEAR(out[0].cover, 1.4 / 3.8, 1e-12);
    pic.entries.pop_back();
    book.Measure(pic, l, at_border, 20000.0, out);
    SC_EXPECT_NEAR(out[0].sam_baseline, 3.8, 1e-12);  // and stays raised
}

SC_TEST("air front (F2.2): a band further in takes its baseline from the pre-war picture; an empty band has no cover") {
    const LineOfContact l = StraightLine();
    IntelPicture pic;
    pic.entries.push_back(Sam("s1", At(25, 10), 10.0, "s125"));  // reaches [20, 40] at its ring's edge
    pic.entries.push_back(Sam("s2", At(25, 35), 10.0, "s75m2"));
    pic.entries.push_back(Sam("s3", At(25, 55), 10.0, "s75m2"));  // 5 km short of [20, 40]
    SectorCoverBook book;
    book.Start(pic, l);
    pic.entries[1].function = 0.25;
    std::vector<SectorCover> out;
    book.Measure(pic, l, {20000.0, 0.0}, 20000.0, out);
    SC_EXPECT_NEAR(out[0].near_m, 20000.0, 1e-12);
    SC_EXPECT_NEAR(out[0].far_m, 40000.0, 1e-12);
    SC_EXPECT(out[0].sam_units == 2);
    SC_EXPECT_NEAR(out[0].sam_baseline, 2.0, 1e-12);
    SC_EXPECT_NEAR(out[0].cover, 1.25 / 2.0, 1e-12);
    // Sector B has no SAM believed in its band, and never had: no cover, all its mobile air defence gone.
    SC_EXPECT(out[1].sam_units == 0);
    SC_EXPECT_NEAR(out[1].cover, 0.0, 1e-12);
    SC_EXPECT_NEAR(out[1].mobile_destroyed, 1.0, 1e-12);
    // The guns count with the mobile air defence, not with the cover; an unknown SAM (a launch nobody explained) too.
    IntelEntry gun;
    gun.key = "g";
    gun.kind = ObjectKind::Aaa;
    gun.pos = At(80, 5);
    gun.threat_radius_m = 2500.0;
    gun.threat_radar = false;
    gun.function = 0.0;
    IntelEntry unknown = Sam("u", At(70, 3), 6.0, "");
    unknown.threat_radar = false;
    SC_EXPECT(IsMobileAirDefence(gun) && IsMobileAirDefence(unknown) && !IsMobileAirDefence(pic.entries[0]));
    pic.entries.push_back(gun);
    pic.entries.push_back(unknown);
    book.Measure(pic, l, {20000.0, 0.0}, 20000.0, out);
    SC_EXPECT(out[1].sam_units == 0 && out[1].mobile_units == 2);
    SC_EXPECT_NEAR(out[1].mobile_baseline, 2.0, 1e-12);
    SC_EXPECT_NEAR(out[1].mobile_destroyed, 0.5, 1e-12);
    // A line that has moved re-places the band: 10 km moved in all along A puts s3 within reach of [20, 40].
    LineOfContact moved = l;
    for (std::size_t i = 0; i < moved.StationCount(); ++i) moved.SetStationOffset(i, 10000.0);
    book.Measure(pic, moved, {20000.0, 0.0}, 20000.0, out);
    SC_EXPECT(out[0].sam_units == 2);  // s2 (25 km beyond the contact) and s3 (45 km, ring 5 km in); s1 no longer
    SC_EXPECT_NEAR(out[0].sam_baseline, 2.0, 1e-12);
    SC_EXPECT_NEAR(out[0].cover, 1.25 / 2.0, 1e-12);
    // Back on the first line the pre-war places are taken again: s1 and s2. Then the same line moved in place: s2 and s3.
    book.Measure(pic, l, {20000.0, 0.0}, 20000.0, out);
    SC_EXPECT(out[0].sam_units == 2);
    SC_EXPECT_NEAR(out[0].cover, 1.25 / 2.0, 1e-12);
    LineOfContact same = l;
    book.Measure(pic, same, {20000.0, 0.0}, 20000.0, out);  // a copy unmoved: the same line
    const unsigned v = same.Version();
    for (std::size_t i = 0; i < same.StationCount(); ++i) same.SetStationOffset(i, 10000.0);
    SC_EXPECT(same.Version() != v);
    book.Measure(pic, same, {20000.0, 0.0}, 20000.0, out);
    SC_EXPECT(out[0].sam_units == 2);
    SC_EXPECT_NEAR(out[0].sam_there, 2.0, 1e-12);  // s2 and s3; s1 (0 km beyond the contact now) out of reach
}

SC_TEST("air front (F2.2): losses per low-level sortie by sector over the last days") {
    SectorLossWindow w;
    w.Reset(2);
    SC_EXPECT(SectorLossWindow::DayOf(23.9) == 0 && SectorLossWindow::DayOf(24.0) == 1 && SectorLossWindow::DayOf(-0.5) == -1);
    w.NoteSorties(0, 1, 20);
    w.NoteLosses(0, 1, 1);
    w.NoteSorties(0, 2, 30);
    w.NoteLosses(0, 2, 2);
    w.NoteSorties(0, 3, 50);
    w.NoteSorties(1, 3, 10);
    w.NoteLosses(1, 3, 1);
    SC_EXPECT_NEAR(w.Rate(0, 3, 1), 0.0, 1e-12);
    SC_EXPECT_NEAR(w.Rate(0, 3, 2), 2.0 / 80.0, 1e-12);
    SC_EXPECT_NEAR(w.Rate(0, 3, 3), 3.0 / 100.0, 1e-12);
    SC_EXPECT_NEAR(w.Rate(1, 3, 3), 0.1, 1e-12);
    // Day 6 pushes day 1 and 2 out of the ring; a late note for a day already gone is dropped.
    w.NoteSorties(0, 5, 10);
    w.NoteSorties(0, 6, 10);
    w.NoteLosses(0, 2, 5);
    SC_EXPECT(w.Sorties(0, 6, 4) == 70 && w.Losses(0, 6, 4) == 0);
    SC_EXPECT_NEAR(w.Rate(0, 9, 3), 0.0, 1e-12);  // nothing flown in the window
    SC_EXPECT(w.Sorties(-1, 3, 3) == 0 && w.Sorties(2, 3, 3) == 0);
}

SC_TEST("air front (F2.2): under air_front.mode 1 the campaign measures its sectors each plan and counts their losses") {
    AirWarData d;
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", d, &e));
    d.tuning.values["air_front.mode"] = 1.0;
    AirCampaign c;
    SC_EXPECT(c.Init(d, 21));
    SC_EXPECT(c.AirFrontOn() && c.CoverBook().Started());
    const int n = static_cast<int>(c.FrontLine().SectorCount());  // the Pact's front's five sectors (the theatre's data)
    SC_EXPECT(n == 5 && c.SectorCovers().size() == c.FrontLine().SectorCount());
    for (const SectorCover& s : c.SectorCovers()) SC_EXPECT(s.cover >= 0.0 && s.cover <= 1.0 && s.sam_baseline >= s.sam_there);
    while (c.Now() < 72.0) {
        if (c.Advance(72.0)) c.SkipPlayerMission();
    }
    int sorties = 0;
    for (int s = 0; s < n; ++s) sorties += c.SectorLosses().Sorties(s, SectorLossWindow::DayOf(c.Now()), 4);
    int low = 0;
    for (const Package& p : c.Packages()) {
        if (p.resolved && p.profile == Profile::Low && SectorLossWindow::DayOf(p.tot_h) > SectorLossWindow::DayOf(c.Now()) - 4) low += p.Aircraft();
    }
    std::printf("    low-level sorties in the window %d (packages say %d); covers", sorties, low);
    for (const SectorCover& s : c.SectorCovers()) std::printf(" %.2f", s.cover);
    std::printf("\n");
    SC_EXPECT(sorties == low);
    for (int s = 0; s < n; ++s) SC_EXPECT(c.SectorLossRate(s, 3) >= 0.0 && c.SectorLossRate(s, 3) <= 1.0);
}

// ------------------------------------------------------------------------------------------------ the decision (F2.3)

namespace {

// The Pact's rules as its "air_front" block gives them (the numbers are the test's, placeholders like the picture's).
AirFrontDef PeerDef() {
    AirFrontDef d;
    d.present = true;
    d.kind = AirFrontKind::Advance;
    d.cover_at_most = 0.2;
    d.mobile_destroyed_at_least = 0.5;
    d.loss_line = 0.04;
    d.loss_days = 2;
    d.delay_lo_h = 6.0;
    d.delay_hi_h = 24.0;
    d.max_depth_m = 60000.0;
    d.min_reward = 1.0;
    d.crisis = AirFrontCrisis::Advance;
    d.crisis_lo_h = 24.0;
    d.crisis_hi_h = 72.0;
    return d;
}

IntelEntry Target(const std::string& key, ObjectKind kind, const Vec3& pos, double value) {
    IntelEntry e;
    e.key = key;
    e.object_id = key;
    e.kind = kind;
    e.pos = pos;
    e.value = value;
    return e;
}

// Two sectors, each with an Osa and a Kub over its first band and our troops in it (worth 1 each to the enemy).
IntelPicture TwoSectors() {
    IntelPicture pic;
    pic.entries.push_back(Sam("a1", At(25, 10), 10.0, "osa"));
    pic.entries.push_back(Sam("a2", At(15, 12), 15.0, "kub"));
    pic.entries.push_back(Sam("b1", At(75, 10), 10.0, "osa"));
    pic.entries.push_back(Sam("b2", At(85, 12), 15.0, "kub"));
    pic.entries.push_back(Target("ta", ObjectKind::Troops, At(25, 8), 1.0));
    pic.entries.push_back(Target("tb", ObjectKind::Troops, At(75, 8), 1.0));
    return pic;
}

// The game values with the reward weighed over the next band itself (reach 0), so the hand cases put the targets where
// the cover is; the campaign's reach (40 km beyond the air front) is checked on its own.
AirFrontRules HandRules() {
    AirFrontRules r;
    r.reward_reach_m = 0.0;
    return r;
}

// One planning cycle as the campaign runs it: the measure at the current depths, then the decision.
struct Staff {
    AirFrontDef def = PeerDef();
    AirFrontRules rules = HandRules();
    AirFrontChief chief;
    LineOfContact line = StraightLine();
    SectorCoverBook book;
    SectorLossWindow losses;
    AirFrontState st;
    std::vector<SectorCover> cover;
    std::vector<AirFrontMove> moves;
    std::vector<char> crisis;
    double politics_m = 0.0;
    bool event = false;

    void Start(const IntelPicture& prewar) {
        book.Start(prewar, line);
        losses.Reset(line.SectorCount());
        st.Reset(line.SectorCount(), 0.0);
    }
    const std::vector<AirFrontMove>& Cycle(const IntelPicture& pic, double now_h) {
        std::vector<double> depth;
        st.Depths(depth);
        book.Measure(pic, line, depth, rules.step_m, cover);
        AirFrontInputs in;
        in.picture = &pic;
        in.line = &line;
        in.cover = &cover;
        in.losses = &losses;
        in.politics_max_depth_m = politics_m;
        in.event_fired = event;
        in.crisis = crisis.empty() ? nullptr : &crisis;
        in.now_h = now_h;
        DecideAirFront(def, rules, in, chief, st, moves);
        return moves;
    }
    double Depth(int s) const { return st.sectors[static_cast<std::size_t>(s)].depth_m; }
    double Helo(int s) const { return st.sectors[static_cast<std::size_t>(s)].helo_depth_m; }
};

bool Has(const std::vector<AirFrontMove>& moves, AirFrontMove::What what, int sector) {
    for (const AirFrontMove& m : moves) {
        if (m.what == what && m.sector == sector) return true;
    }
    return false;
}

std::uint64_t JournalHashBut(const AirCampaign& c, const std::string& kind) {
    std::uint64_t h = 1469598103934665603ull;
    for (const JournalEntry& j : c.Journal()) {
        if (j.kind == kind) continue;
        for (char ch : j.text) {
            h ^= static_cast<unsigned char>(ch);
            h *= 1099511628211ull;
        }
        h ^= 0x0a;
        h *= 1099511628211ull;
    }
    return h;
}

}  // namespace

SC_TEST("air front (F2.3): a hole opens one sector only, after the chief's delay, a step at a time, while it is worth it") {
    Staff s;
    IntelPicture pic = TwoSectors();
    s.Start(pic);
    // A's Osa and Kub believed destroyed; B's believed working (one a dummy in truth: the staff cannot know).
    pic.entries[0].function = 0.0;
    pic.entries[1].function = 0.0;
    s.Cycle(pic, 0.0);
    SC_EXPECT(s.cover.size() == 2 && s.cover[0].cover == 0.0 && s.cover[1].cover == 1.0);
    SC_EXPECT(Has(s.moves, AirFrontMove::What::Seen, 0) && s.moves.size() == 1);
    // A middling chief (every trait 0.5): the delay's middle, 6 + 18 x 0.5 = 15 h.
    SC_EXPECT_NEAR(s.moves[0].after_h, 15.0, 1e-12);
    SC_EXPECT_NEAR(s.moves[0].reward, 1.0, 1e-12);
    s.Cycle(pic, 12.0);
    SC_EXPECT(s.moves.empty() && s.Depth(0) == 0.0);
    s.Cycle(pic, 15.0);
    SC_EXPECT(Has(s.moves, AirFrontMove::What::Advance, 0));
    SC_EXPECT_NEAR(s.Depth(0), 20000.0, 1e-9);
    SC_EXPECT_NEAR(s.Depth(1), 0.0, 1e-12);
    SC_EXPECT_NEAR(s.Helo(0), 0.0, 1e-12);
    // The next band [20, 40 km]: its cover broken too, but nothing believed there worth a step.
    for (double t = 24.0; t <= 96.0; t += 12.0) s.Cycle(pic, t);
    SC_EXPECT_NEAR(s.Depth(0), 20000.0, 1e-9);
    SC_EXPECT_NEAR(s.Depth(1), 0.0, 1e-12);
    // Troops found deeper: one more step after the delay, never two a cycle.
    pic.entries.push_back(Target("ta2", ObjectKind::Troops, At(30, 30), 1.5));
    s.Cycle(pic, 108.0);
    SC_EXPECT(Has(s.moves, AirFrontMove::What::Seen, 0));
    s.Cycle(pic, 123.0);
    SC_EXPECT_NEAR(s.Depth(0), 40000.0, 1e-9);
    // A bold chief sees it sooner and takes more cover as broken; a methodical one waits the long end.
    Staff b;
    b.chief.traits.boldness = 1.0;
    b.chief.traits.methodical = 0.0;
    IntelPicture half = TwoSectors();
    b.Start(half);
    half.entries[0].function = 0.0;
    half.entries[1].function = 0.45;  // A's cover 0.45 x 1 / 2 = 0.225: over the gate 0.2, under the bold chief's 0.24
    b.Cycle(half, 0.0);
    SC_EXPECT(Has(b.moves, AirFrontMove::What::Seen, 0));
    SC_EXPECT_NEAR(b.moves[0].after_h, 6.0, 1e-12);
    Staff m;
    m.chief.traits.methodical = 1.0;
    m.chief.traits.boldness = 0.0;
    m.Start(TwoSectors());
    m.Cycle(half, 0.0);
    SC_EXPECT(m.moves.empty());  // 0.225 is over his 0.16
    m.Cycle(pic, 1.0);
    SC_EXPECT(Has(m.moves, AirFrontMove::What::Seen, 0));
    SC_EXPECT_NEAR(m.moves[0].after_h, 24.0, 1e-12);
    // With the campaign's reach a step is worth what it brings into close support's reach, 40-60 km beyond the air front:
    // the troops at 8 km are in reach already and count for nothing; a column at 50 km does (worth 1 x 0.6 x 1.5).
    Staff r;
    r.rules.reward_reach_m = 40000.0;
    r.def.min_reward = 0.5;  // the cycle reads no weights: the column is worth 0.6 to it
    IntelPicture far_pic = TwoSectors();
    r.Start(far_pic);
    far_pic.entries[0].function = 0.0;
    far_pic.entries[1].function = 0.0;
    r.Cycle(far_pic, 0.0);
    SC_EXPECT(r.moves.empty());
    IntelEntry col = Target("col", ObjectKind::Column, At(30, 50), 0.6);
    far_pic.entries.push_back(col);
    std::vector<double> worth, at_border(2, 0.0);
    double weight[static_cast<int>(ObjectKind::Count)];
    for (double& w : weight) w = 1.0;
    weight[static_cast<int>(ObjectKind::Column)] = 1.5;
    SectorRewards(far_pic, r.line, at_border, 40000.0, 20000.0, weight, nullptr, worth);
    SC_EXPECT(worth.size() == 2);
    SC_EXPECT_NEAR(worth[0], 0.9, 1e-12);
    SC_EXPECT_NEAR(worth[1], 0.0, 1e-12);
    const std::vector<ObjectKind> banned = {ObjectKind::Column};
    SectorRewards(far_pic, r.line, at_border, 40000.0, 20000.0, weight, &banned, worth);
    SC_EXPECT_NEAR(worth[0], 0.0, 1e-12);
    r.Cycle(far_pic, 12.0);
    SC_EXPECT(Has(r.moves, AirFrontMove::What::Seen, 0));
}

SC_TEST("air front (F2.3): on belief, not truth; and never on a band nothing was ever believed to cover") {
    // The staff's inputs are its picture alone. A unit believed working holds the gate whatever it is; one believed
    // destroyed opens it whatever it is.
    Staff s;
    IntelPicture pic = TwoSectors();
    s.Start(pic);
    pic.entries[2].function = 0.0;  // B's Osa believed destroyed, its Kub working: B's cover 0.5
    for (double t = 0.0; t <= 72.0; t += 12.0) s.Cycle(pic, t);
    SC_EXPECT(s.Depth(0) == 0.0 && s.Depth(1) == 0.0);
    pic.entries[3].function = 0.0;  // and its Kub: B opens
    s.Cycle(pic, 84.0);
    SC_EXPECT(Has(s.moves, AirFrontMove::What::Seen, 1));
    // The tuned district's case: our SAMs far behind the line, so every band beyond the border is empty and always was.
    // No cover and nothing to destroy is not a hole: the gates stay shut, troops or none.
    Staff d;
    IntelPicture far_back;
    far_back.entries.push_back(Sam("deep1", At(25, 94), 10.0, "osa"));
    far_back.entries.push_back(Sam("deep2", At(75, 100), 15.0, "kub"));
    far_back.entries.push_back(Target("ta", ObjectKind::Troops, At(25, 8), 3.0));
    far_back.entries.push_back(Target("tb", ObjectKind::Troops, At(75, 8), 3.0));
    d.Start(far_back);
    far_back.entries[0].function = 0.0;
    for (double t = 0.0; t <= 96.0; t += 12.0) d.Cycle(far_back, t);
    SC_EXPECT(d.cover.size() == 2 && d.cover[0].cover == 0.0 && d.cover[0].mobile_destroyed == 1.0 && d.cover[0].sam_baseline == 0.0);
    SC_EXPECT(d.Depth(0) == 0.0 && d.Depth(1) == 0.0 && !d.st.sectors[0].contested);
    // The guard is what holds it: with no minimum baseline the same picture opens both sectors.
    Staff g;
    g.rules.min_baseline = 0.0;
    g.Start(far_back);
    for (double t = 0.0; t <= 24.0; t += 12.0) g.Cycle(far_back, t);
    SC_EXPECT(g.Depth(0) == 20000.0 && g.Depth(1) == 20000.0);
    // A band once covered keeps its sector contested: once through the broken band, an empty one beyond may follow.
    Staff c;
    IntelPicture pic2 = TwoSectors();
    c.Start(pic2);
    pic2.entries[0].function = 0.0;
    pic2.entries[1].function = 0.0;
    pic2.entries.push_back(Target("deep", ObjectKind::Bridge, At(25, 50), 2.0));
    pic2.entries.push_back(Target("deeper", ObjectKind::Bridge, At(25, 30), 2.0));
    for (double t = 0.0; t <= 96.0; t += 12.0) c.Cycle(pic2, t);
    SC_EXPECT(c.st.sectors[0].contested && c.Depth(0) == 60000.0);  // three steps: to its greatest depth and no further
    SC_EXPECT(c.st.sectors[0].advances == 3 && c.Depth(1) == 0.0);
}

SC_TEST("air front (F2.3): losses over the line pull a sector back; a composed chief waits a cycle, a stressed one does not") {
    for (int k = 0; k < 3; ++k) {
        Staff s;
        if (k == 1) s.chief.traits.composure = 0.8;
        if (k == 2) {
            s.chief.traits.composure = 0.8;
            s.chief.stress = 0.8;
        }
        IntelPicture pic = TwoSectors();
        s.Start(pic);
        pic.entries[0].function = 0.0;
        pic.entries[1].function = 0.0;
        s.Cycle(pic, 0.0);
        s.Cycle(pic, 24.0);
        SC_EXPECT_NEAR(s.Depth(0), 20000.0, 1e-9);
        // A few sorties and one lost: not yet a rate (air_front.min_sorties 6).
        s.losses.NoteSorties(0, 1, 4);
        s.losses.NoteLosses(0, 1, 1);
        s.Cycle(pic, 36.0);
        SC_EXPECT_NEAR(s.Depth(0), 20000.0, 1e-9);
        // Twenty sorties, two lost over the two days: 10 % a sortie, over the 4 % line.
        s.losses.NoteSorties(0, 1, 16);
        s.losses.NoteLosses(0, 1, 1);
        s.Cycle(pic, 40.0);
        if (k == 1) {
            SC_EXPECT_NEAR(s.Depth(0), 20000.0, 1e-9);  // the composed chief waits a cycle
            s.Cycle(pic, 44.0);
        }
        SC_EXPECT(Has(s.moves, AirFrontMove::What::PullBack, 0));
        SC_EXPECT_NEAR(s.Depth(0), 0.0, 1e-12);
        SC_EXPECT_NEAR(s.moves[0].loss_rate, 0.1, 1e-12);
        // While the losses stay in the window the gates stay shut; once they leave it the hole is seen again.
        s.Cycle(pic, 47.0);
        SC_EXPECT(s.moves.empty() && s.Depth(0) == 0.0);
        s.Cycle(pic, 96.0);  // day 4: day 1's losses out of the two-day window
        SC_EXPECT(Has(s.moves, AirFrontMove::What::Seen, 0));
    }
}

SC_TEST("air front (F2.3): the helicopters follow a step behind in time, after a day of clean low-level work, never past it") {
    Staff s;
    IntelPicture pic = TwoSectors();
    s.Start(pic);
    pic.entries[0].function = 0.0;
    pic.entries[1].function = 0.0;
    pic.entries.push_back(Target("ta2", ObjectKind::Troops, At(30, 30), 1.5));
    s.Cycle(pic, 0.0);
    s.Cycle(pic, 15.0);
    SC_EXPECT_NEAR(s.Depth(0), 20000.0, 1e-9);
    // A day on and no low-level work flown: the helicopters stay at the border; the troops at 30 km seen.
    s.Cycle(pic, 39.0);
    SC_EXPECT(Has(s.moves, AirFrontMove::What::Seen, 0));
    s.Cycle(pic, 41.0);
    SC_EXPECT_NEAR(s.Helo(0), 0.0, 1e-12);
    // The fixed-wing front goes on to 40 km; the helicopters wait their day of clean work.
    s.Cycle(pic, 54.0);
    SC_EXPECT_NEAR(s.Depth(0), 40000.0, 1e-9);
    s.losses.NoteSorties(0, 2, 8);
    s.Cycle(pic, 66.0);
    SC_EXPECT_NEAR(s.Helo(0), 0.0, 1e-12);  // 12 h since the fixed-wing step
    s.Cycle(pic, 78.0);
    SC_EXPECT(Has(s.moves, AirFrontMove::What::Helicopters, 0));
    SC_EXPECT_NEAR(s.Helo(0), 20000.0, 1e-9);
    s.losses.NoteSorties(0, 3, 8);
    s.Cycle(pic, 90.0);
    SC_EXPECT_NEAR(s.Helo(0), 20000.0, 1e-9);  // 12 h after its own step
    s.Cycle(pic, 102.0);
    SC_EXPECT_NEAR(s.Helo(0), 40000.0, 1e-9);
    for (double t = 114.0; t <= 200.0; t += 12.0) {
        s.losses.NoteSorties(0, SectorLossWindow::DayOf(t), 4);
        s.Cycle(pic, t);
        SC_EXPECT(s.Helo(0) <= s.Depth(0) + 1e-9);
    }
    // Losses pull both back.
    s.losses.NoteSorties(0, SectorLossWindow::DayOf(204.0), 10);
    s.losses.NoteLosses(0, SectorLossWindow::DayOf(204.0), 3);
    s.Cycle(pic, 204.0);
    SC_EXPECT(s.Depth(0) == 40000.0 - 20000.0 && s.Helo(0) == s.Depth(0));
}

SC_TEST("air front (F2.3): the rebels surge for days and cut back; the neutral stays at its border until its event; the politics cap it") {
    // The rebels: forward only where our mobile SAMs are believed gone, for 24-72 h (a middling chief 48 h), then back and a rest.
    Staff r;
    r.def.kind = AirFrontKind::Surge;
    r.def.cover_at_most = 0.0;
    r.def.mobile_destroyed_at_least = 1.0;
    r.def.loss_line = 0.01;
    r.def.max_depth_m = 40000.0;
    r.def.surge_lo_h = 24.0;
    r.def.surge_hi_h = 72.0;
    r.def.surge_rest_h = 72.0;
    r.def.crisis = AirFrontCrisis::Helicopters;
    IntelPicture pic = TwoSectors();
    r.Start(pic);
    pic.entries[0].function = 0.0;
    pic.entries[1].function = 0.1;  // one working a little: no surge
    r.Cycle(pic, 0.0);
    SC_EXPECT(r.moves.empty());
    pic.entries[1].function = 0.0;
    r.Cycle(pic, 12.0);
    r.Cycle(pic, 27.0);
    SC_EXPECT_NEAR(r.Depth(0), 20000.0, 1e-9);
    r.Cycle(pic, 72.0);
    SC_EXPECT_NEAR(r.Depth(0), 20000.0, 1e-9);
    r.Cycle(pic, 75.0);
    SC_EXPECT(Has(r.moves, AirFrontMove::What::SurgeEnd, 0) && r.Depth(0) == 0.0);
    r.Cycle(pic, 120.0);
    SC_EXPECT(r.moves.empty());  // resting
    r.Cycle(pic, 147.0);
    SC_EXPECT(Has(r.moves, AirFrontMove::What::Seen, 0));
    // The neutral: the same hole, and nothing until its political event.
    Staff n;
    n.def.kind = AirFrontKind::Border;
    n.def.after_event = "deeper_raids";
    n.Start(TwoSectors());
    for (double t = 0.0; t <= 72.0; t += 12.0) n.Cycle(pic, t);
    SC_EXPECT(n.Depth(0) == 0.0 && n.st.sectors[0].advances == 0);
    n.event = true;
    n.Cycle(pic, 84.0);
    n.Cycle(pic, 99.0);
    SC_EXPECT_NEAR(n.Depth(0), 20000.0, 1e-9);
    // Its politics hold it to 30 km from the border: one step, and back to it when they shrink to 10 km.
    pic.entries.push_back(Target("ta2", ObjectKind::Troops, At(30, 30), 1.5));
    n.politics_m = 30000.0;
    for (double t = 111.0; t <= 200.0; t += 12.0) n.Cycle(pic, t);
    SC_EXPECT_NEAR(n.Depth(0), 20000.0, 1e-9);
    n.politics_m = 10000.0;
    n.Cycle(pic, 210.0);
    SC_EXPECT(Has(n.moves, AirFrontMove::What::Politics, 0) && n.Depth(0) == 0.0);
}

SC_TEST("air front (F2.3): a crisis on its own ground front (F3.5's stub): a step and the loss gate ignored; the rebels' helicopters once") {
    Staff s;
    IntelPicture pic = TwoSectors();  // every unit working: no sector's gates open
    s.Start(pic);
    s.crisis = {0, 1};
    s.Cycle(pic, 0.0);
    SC_EXPECT(Has(s.moves, AirFrontMove::What::Crisis, 1) && s.Depth(1) == 20000.0 && s.Depth(0) == 0.0);
    // Losses over the line in B: ignored while the crisis lasts (a middling chief: 24 + 48 x 0.5 = 48 h).
    for (int day = 0; day < 2; ++day) {
        s.losses.NoteSorties(1, day, 20);
        s.losses.NoteLosses(1, day, 5);
    }
    s.Cycle(pic, 12.0);
    SC_EXPECT_NEAR(s.Depth(1), 20000.0, 1e-9);
    s.crisis = {0, 0};
    s.Cycle(pic, 47.0);
    SC_EXPECT_NEAR(s.Depth(1), 20000.0, 1e-9);
    s.Cycle(pic, 48.0);
    SC_EXPECT(Has(s.moves, AirFrontMove::What::PullBack, 1) && s.Depth(1) == 0.0);
    // The rebels: their helicopters a step ahead of the fixed-wing front once, back when it is over; never again.
    Staff r;
    r.def.kind = AirFrontKind::Surge;
    r.def.crisis = AirFrontCrisis::Helicopters;
    r.Start(pic);
    r.crisis = {1, 0};
    r.Cycle(pic, 0.0);
    SC_EXPECT(Has(r.moves, AirFrontMove::What::Crisis, 0) && r.Helo(0) == 20000.0 && r.Depth(0) == 0.0);
    r.crisis = {0, 0};
    r.Cycle(pic, 60.0);
    SC_EXPECT(Has(r.moves, AirFrontMove::What::HeloBack, 0) && r.Helo(0) == 0.0);
    r.crisis = {1, 0};
    r.Cycle(pic, 72.0);
    SC_EXPECT(r.Helo(0) == 0.0 && r.moves.empty());
}

SC_TEST("air front (F2.3): every faction's block loads; a bad one fails; the campaign decides at mode 1; no package changes while nothing moves") {
    const char* factions[] = {"nato", "warsaw_pact", "rebels", "neutral"};
    const AirFrontKind kinds[] = {AirFrontKind::Advance, AirFrontKind::Advance, AirFrontKind::Surge, AirFrontKind::Border};
    for (int i = 0; i < 4; ++i) {
        AirWarData d;
        std::string e;
        SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", std::string("air_war/faction_") + factions[i] + ".json", d, &e));
        if (!e.empty()) std::printf("    %s\n", e.c_str());
        SC_EXPECT(d.faction.air_front.present && d.faction.air_front.kind == kinds[i]);
        SC_EXPECT(d.faction.air_front.max_depth_m > 0.0 && d.faction.air_front.delay_lo_h <= d.faction.air_front.delay_hi_h);
    }
    // The loader's refusals, on the neutral's file.
    std::ifstream in(kData + "/air_war/faction_neutral.json", std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto loads = [&](const std::string& from, const std::string& to) {
        std::string t = text;
        const std::size_t at = t.find(from);
        SC_EXPECT(at != std::string::npos);
        if (at != std::string::npos) t.replace(at, from.size(), to);
        FactionDef f;
        std::string e;
        return LoadFactionJson(t, f, &e);
    };
    SC_EXPECT(loads("\"after_event\": \"deeper_raids\"", "\"after_event\": \"deeper_raids\""));
    SC_EXPECT(!loads("\"after_event\": \"deeper_raids\"", "\"after_event\": \"no_such_event\""));
    SC_EXPECT(!loads("\"kind\": \"border\", ", "\"kind\": \"sideways\", "));
    SC_EXPECT(!loads("\"loss_days\": 3", "\"loss_days\": 5"));
    SC_EXPECT(!loads("\"delay_h\": [12, 24]", "\"delay_h\": [24, 12]"));
    SC_EXPECT(!loads("\"kind\": \"border\", ", "\"kind\": \"border\", \"surge_h\": [24, 48], "));
    SC_EXPECT(!loads("\"kind\": \"border\", \"after_event\": \"deeper_raids\", ", "\"kind\": \"advance\", \"after_event\": \"deeper_raids\", "));
    SC_EXPECT(!loads("\"cover_at_most\": 0.20", "\"cover_at_most\": 0.20, \"gate\": 1"));
    // The campaign on the tuned district at mode 1: our SAMs stand far behind the Pact's line, so no sector is ever
    // contested and its air front stays at the border, and the war's packages and the journal but for the air front's own
    // entries are those of mode 0. With the guard off the sectors move, and since F2.4 the planner reads them: the war parts.
    AirWarData d;
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", d, &e));
    AirWarData on = d, loose = d;
    on.tuning.values["air_front.mode"] = 1.0;
    loose.tuning.values["air_front.mode"] = 1.0;
    loose.tuning.values["air_front.min_baseline"] = 0.0;
    loose.faction.air_front.min_reward = 0.0;  // and nothing needed to fly for: every sector's empty band opens
    AirCampaign a, b, c;
    a.Init(d, 21);
    b.Init(on, 21);
    c.Init(loose, 21);
    for (AirCampaign* x : {&a, &b, &c}) {
        while (x->Now() < 72.0) {
            if (x->Advance(72.0)) x->SkipPlayerMission();
        }
    }
    int b_moves = 0, c_moves = 0;
    for (const JournalEntry& j : b.Journal()) b_moves += j.kind == "air_front" ? 1 : 0;
    for (const JournalEntry& j : c.Journal()) c_moves += j.kind == "air_front" ? 1 : 0;
    double c_deep = 0.0;
    for (double x : c.AirFrontDepths()) c_deep = x > c_deep ? x : c_deep;
    std::printf("    the district, three days: %d air front entries; with the guard off %d, the deepest %.0f km\n", b_moves, c_moves, c_deep / 1000.0);
    SC_EXPECT(b_moves == 0 && c_moves > 0 && c_deep >= 20000.0);
    for (double x : b.AirFrontDepths()) SC_EXPECT(x == 0.0);
    SC_EXPECT(b.AirFrontDepths().size() == 5 && b.AirFront().sectors.size() == 5);
    SC_EXPECT(!a.AirFrontOn() && a.AirFrontDepths().empty() && a.AirFront().sectors.empty());
    const auto same_war = [&](const AirCampaign& x) {
        bool same = JournalHashBut(x, "air_front") == JournalHashBut(a, "air_front") && x.Packages().size() == a.Packages().size();
        for (std::size_t i = 0; same && i < a.Packages().size(); ++i) {
            same = x.Packages()[i].target_key == a.Packages()[i].target_key && x.Packages()[i].tot_h == a.Packages()[i].tot_h &&
                   x.Packages()[i].Aircraft() == a.Packages()[i].Aircraft();
        }
        return same;
    };
    SC_EXPECT(same_war(b));
    std::printf("    with the guard off the war is %s (%zu packages, %zu at mode 0)\n", same_war(c) ? "the same" : "another", c.Packages().size(),
                a.Packages().size());
    // At mode 1 a point's air front is its sector's.
    if (!c.AirFront().sectors.empty()) {
        const Vec3 p = c.FrontLine().PointAt(c.FrontLine().Sectors()[0].to_m * 0.5);
        SC_EXPECT(c.AirFrontAt(p) == c.AirFront().sectors[0].depth_m && c.HeloFrontAt(p) == c.AirFront().sectors[0].helo_depth_m);
    }
    SC_EXPECT(a.AirFrontAt(Vec3{0.0, 0.0, 0.0}) == 0.0);
}

SC_TEST("air front (F2.3): a save keeps air_front_mode; one made without it replays at 0") {
    AirWarData d;
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", d, &e));
    d.tuning.values["air_front.mode"] = 1.0;
    AirCampaign c;
    c.Init(d, 21);
    while (c.Now() < 48.0) {
        if (c.Advance(48.0)) c.SkipPlayerMission();
    }
    std::string saved = SaveCampaignJson(c, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json");
    SC_EXPECT(saved.find("\"air_front_mode\"") != std::string::npos);
    AirCampaign back;
    SC_EXPECT(LoadCampaignJson(saved, kData, back, &e));
    SC_EXPECT_NEAR(back.Data().tuning.Get("air_front.mode"), 1.0, 1e-12);
    SC_EXPECT(back.AirFrontOn() && back.Journal().size() == c.Journal().size());
    const std::size_t at = saved.find("\"air_front_mode\"");
    const std::size_t end = saved.find(',', at);
    if (at != std::string::npos && end != std::string::npos) saved.erase(at, end - at + 1);
    AirCampaign old;
    SC_EXPECT(LoadCampaignJson(saved, kData, old, &e));
    SC_EXPECT_NEAR(old.Data().tuning.Get("air_front.mode"), 0.0, 1e-12);
    SC_EXPECT(!old.AirFrontOn() && old.AirFront().sectors.empty());
    AirWarData off;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", off, &e));
    AirCampaign zero;
    zero.Init(off, 21);
    while (zero.Now() < 48.0) {
        if (zero.Advance(48.0)) zero.SkipPlayerMission();
    }
    SC_EXPECT(old.Journal().size() == zero.Journal().size() && JournalHashBut(old, "") == JournalHashBut(zero, ""));
}
