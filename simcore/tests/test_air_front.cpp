// What the enemy's air staff weighs by sector (track F, F2.2; simcore/air_war/air_front.hpp, docs/fronts/02_AIR_FRONT.md §3):
// the believed cover over a sector's next band against its pre-war baseline, our mobile air defence there, the losses
// by sector. Generic placeholder pictures, checked by hand.
#include <cmath>
#include <cstdio>
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
