// The player's two outcomes, measured (roadmap 14.6, first form): simcore/campaign/outcomes.hpp.
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/campaign/mission_session.hpp"
#include "simcore/campaign/outcomes.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

airwar::Flight MakeFlight(airwar::FlightRole role, int count) {
    airwar::Flight f;
    f.role = role;
    f.count = count;
    return f;
}

}  // namespace

SC_TEST("outcomes (14.6): a package's ledger: what it brought, what the battery downed, the releases it prevented") {
    // A strike on the bridge with suppression and a jammer: four flights, three of them to release in the session.
    airwar::Package p;
    p.id = 7;
    p.name = "201 80";
    p.kind = airwar::MissionKind::Strike;
    p.target_label = "road bridge near Kesmark";
    p.target_object = "kesmark_road_bridge";
    p.flights = {MakeFlight(airwar::FlightRole::Strike, 4), MakeFlight(airwar::FlightRole::Strike, 2), MakeFlight(airwar::FlightRole::Sead, 2),
                 MakeFlight(airwar::FlightRole::Jammer, 1), MakeFlight(airwar::FlightRole::Dead, 2)};
    p.rationale = {"SEAD: 2 aircraft for 1 radar threat(s) believed on the route"};
    airwar::PackageOutcome o;
    o.package_id = 7;
    o.arms_fired = 3;
    // Flight 1 lost both to the battery before its release; flight 4 lost one to a neighbour and broke away; flight 0
    // released on the bridge.
    o.losses = {{1, "osa_21", Vec3(), false, false}, {1, "osa_21", Vec3(), false, false}, {4, "zu23_bridges", Vec3(), false, false}};
    o.released_flights = {0};
    o.aborted_flights = {4};
    o.damage = {{"kesmark_road_bridge", 0.25}};
    const campaign::PackageLedger l = campaign::LedgerOf(p, o, {0, 1, 4}, "osa_21");
    SC_EXPECT(l.id == 7 && l.kind == "strike" && !l.against_battery);
    SC_EXPECT(l.aircraft == 11 && l.sead == 2 && l.jammers == 1 && l.decoys == 0);
    SC_EXPECT(l.arms_fired == 3 && l.downed_by_battery == 2 && l.downed_by_others == 1);
    SC_EXPECT(l.release_flights == 3 && l.released == 1 && l.prevented_downed == 1 && l.prevented_broke_away == 1 && l.Prevented() == 2);
    SC_EXPECT(l.why.size() == 1);
    // A DEAD package on the battery itself is against it.
    airwar::Package dead = p;
    dead.kind = airwar::MissionKind::Dead;
    dead.target_object = "osa_21";
    SC_EXPECT(campaign::LedgerOf(dead, o, {}, "osa_21").against_battery);

    // The covered bridge: two hours ready over it, the second at half working after the hit.
    campaign::OutcomeLedger led;
    led.Cover("kesmark_bridges", "kesmark_road_bridge", "Kesmark bridges", 1.0);
    led.Cover("kesmark_bridges", "kesmark_road_bridge", "again", 0.5);  // once per place
    led.Covered("kesmark_bridges", 3600.0, 1.0);
    led.Played(p, o, {0, 1, 4}, "osa_21");
    led.Covered("kesmark_bridges", 3600.0, 0.5);
    const campaign::CoveredAsset* a = led.Find("kesmark_bridges");
    SC_EXPECT(a != nullptr && led.Assets().size() == 1);
    if (a == nullptr) return;
    SC_EXPECT_NEAR(a->covered_h, 2.0, 1e-9);
    SC_EXPECT_NEAR(a->working_h, 1.5, 1e-9);
    SC_EXPECT(a->hits == 1 && a->releases_prevented == 2);
    SC_EXPECT_NEAR(a->damage, 0.25, 1e-9);
    SC_EXPECT_NEAR(a->function_now, 0.5, 1e-9);
    const std::string text = led.Text();
    SC_EXPECT(text.find("Assets kept") != std::string::npos && text.find("Freedom of action denied") != std::string::npos);
    SC_EXPECT(text.find("2 shot down by the battery") != std::string::npos && text.find("its reason: SEAD") != std::string::npos);
    SC_EXPECT(text.find(" destroys ") == std::string::npos);  // the mission bridge's word for a kill stays the log's
}

SC_TEST("outcomes (14.6): the battery holds the enemy's phase open when without its entry the believed share falls below the threshold") {
    campaign::SuppressionCheck s;
    s.threshold = 0.5;
    s.known = true;
    s.with_battery = 0.55;
    s.without_battery = 0.45;
    SC_EXPECT(s.HoldsOpen());
    s.without_battery = 0.52;
    SC_EXPECT(!s.HoldsOpen());
    s.threshold = -1.0;
    SC_EXPECT(!s.HoldsOpen());
    // The share without an entry is the same sum without it: two SAM radars believed there, one working, one half.
    airwar::IntelPicture pic;
    airwar::IntelEntry a, b, guns;
    a.key = "osa_21";
    b.key = "s125_lake";
    b.function = 0.5;
    guns.key = "zu23_bridges";
    guns.threat_radar = false;  // guns: not a radar SAM site, not counted
    guns.function = 0.0;
    pic.entries = {a, b, guns};
    SC_EXPECT_NEAR(pic.BelievedIadsShare(), 0.75, 1e-12);
    SC_EXPECT_NEAR(pic.BelievedIadsShare("osa_21"), 0.5, 1e-12);
    SC_EXPECT_NEAR(pic.BelievedIadsShare("zu23_bridges"), 0.75, 1e-12);
    // In a running war.
    airwar::AirWarData d;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", d, &e));
    airwar::AirCampaign c;
    SC_EXPECT(c.Init(d, 1));
    while (c.Now() < 48.0 && c.Advance(48.0)) c.SkipPlayerMission();
    const campaign::SuppressionCheck now = campaign::CheckSuppression(c, c.Now());
    SC_EXPECT(!now.phase.empty() && now.with_battery >= 0.0 && now.with_battery <= 1.0);
    SC_EXPECT(c.Intel().BelievedIadsShare("no such entry") == now.with_battery);
    SC_EXPECT(campaign::OwnObject(c) == "osa_21");
}

namespace {

// A straight 100 km front along east, the enemy's bases south: our side north. Two sectors of 50 km (generic placeholder).
airwar::LineOfContact OutcomesLine() {
    airwar::FrontDef f;
    f.faction = "test";
    f.name = "Test";
    f.line = {Vec3{0.0, 0.0, 0.0}, Vec3{100000.0, 0.0, 0.0}};
    f.bases = Vec3{50000.0, -100000.0, 0.0};
    airwar::SectorDef a;
    a.id = "a";
    a.name = "A";
    a.from_m = 0.0;
    a.to_m = 50000.0;
    airwar::SectorDef b = a;
    b.id = "b";
    b.name = "B";
    b.from_m = 50000.0;
    b.to_m = 100000.0;
    f.sectors = {a, b};
    airwar::LineOfContact l;
    l.Build(f, 5000.0);
    return l;
}

airwar::IntelEntry OutcomesSam(const std::string& key, double e_km, double n_km, const std::string& system) {
    airwar::IntelEntry e;
    e.key = key;
    e.object_id = key;
    e.kind = airwar::ObjectKind::SamSite;
    e.pos = Vec3{e_km * 1000.0, n_km * 1000.0, 0.0};
    e.threat_radius_m = 10000.0;
    e.threat_lethality = 0.2;
    e.threat_radar = true;
    e.system = system;
    return e;
}

}  // namespace

SC_TEST("outcomes (F6.2): the battery's entry holds the enemy's air front: the cover without it by hand, the hours and steps kept") {
    // Sector A's next band [0, 20 km]: an S-125, the battery's Osa and a Kub, all in it before the war.
    const airwar::LineOfContact l = OutcomesLine();
    airwar::IntelPicture pic;
    pic.entries = {OutcomesSam("s125", 25, 10, "s125"), OutcomesSam("osa_21", 25, 12, "osa"), OutcomesSam("kub", 30, 5, "kub")};
    airwar::SectorCoverBook book;
    book.Start(pic, l);
    // The S-125 and the Kub believed destroyed since: the Osa alone holds a third of the cover and half our mobile air defence.
    pic.entries[0].function = 0.0;
    pic.entries[2].function = 0.0;
    std::vector<airwar::SectorCover> out;
    book.Measure(pic, l, std::vector<double>(), 20000.0, out);
    SC_EXPECT_NEAR(out[0].cover, 1.0 / 3.0, 1e-12);
    SC_EXPECT_NEAR(out[0].mobile_destroyed, 0.5, 1e-12);
    double cover = out[0].cover, mobile = out[0].mobile_destroyed;
    SC_EXPECT(campaign::CoverWithout(l, 0, out[0], pic.entries[1], cover, mobile));
    // Without it: what the book measures with the Osa gone (destroyed and gone are the same to the cover).
    airwar::IntelPicture gone = pic;
    gone.entries[1].function = 0.0;
    std::vector<airwar::SectorCover> out_gone;
    book.Measure(gone, l, std::vector<double>(), 20000.0, out_gone);
    SC_EXPECT_NEAR(cover, out_gone[0].cover, 1e-12);
    SC_EXPECT_NEAR(mobile, out_gone[0].mobile_destroyed, 1e-12);
    SC_EXPECT_NEAR(cover, 0.0, 1e-12);
    SC_EXPECT_NEAR(mobile, 1.0, 1e-12);
    // An entry whose ring is far from the band leaves the measure as it is.
    double c2 = 0.7, m2 = 0.2;
    SC_EXPECT(!campaign::CoverWithout(l, 0, out[0], OutcomesSam("far", 25, 60, "osa"), c2, m2));
    SC_EXPECT_NEAR(c2, 0.7, 1e-12);
    SC_EXPECT_NEAR(m2, 0.2, 1e-12);

    // The gates at 0.3 and 0.5 (the Pact's as written): shut with the battery (a third of the cover), open without it.
    campaign::AirFrontCheck a;
    a.sector = 0;
    a.sector_id = "a";
    a.contested = true;
    a.others_open = true;
    a.known = true;
    a.cover_with = out[0].cover;
    a.mobile_with = out[0].mobile_destroyed;
    a.cover_without = cover;
    a.mobile_without = mobile;
    a.gate_cover = 0.3;
    a.gate_mobile = 0.5;
    SC_EXPECT(!a.OpenWith() && a.OpenWithout() && a.HoldsBack());
    campaign::AirFrontCheck nothing_to_fly_for = a;
    nothing_to_fly_for.others_open = false;  // a shut gate of its own: the battery is not what holds it
    SC_EXPECT(!nothing_to_fly_for.HoldsBack());
    campaign::AirFrontCheck open = a;
    open.cover_with = 0.25;  // open even with the battery
    SC_EXPECT(open.OpenWith() && !open.HoldsBack());

    // The ledger: four hours watched, three of them held, then a step forward; a moment with no air front is not counted.
    campaign::OutcomeLedger led;
    led.AirFront(campaign::AirFrontCheck(), 1.0);
    SC_EXPECT(led.LastAirFront() == nullptr && led.AirFrontWatchedH() == 0.0);
    led.AirFront(a, 1.0);
    led.AirFront(a, 1.0);
    led.AirFront(nothing_to_fly_for, 1.0);
    led.AirFront(a, 1.0);
    campaign::AirFrontCheck stepped = open;
    stepped.advances = 1;
    stepped.depth_m = 20000.0;
    led.AirFront(stepped, 1.0);
    SC_EXPECT_NEAR(led.AirFrontHeldH(), 3.0, 1e-12);
    SC_EXPECT_NEAR(led.AirFrontWatchedH(), 5.0, 1e-12);
    SC_EXPECT(led.AirFrontSteps() == 1 && led.AirFrontPullbacks() == 0);
    // The battery moves to sector B, whose front has made two steps already: they are not the session's.
    campaign::AirFrontCheck b = a;
    b.sector = 1;
    b.sector_id = "b";
    b.advances = 2;
    led.AirFront(b, 1.0);
    b.advances = 3;
    led.AirFront(b, 1.0);
    SC_EXPECT(led.AirFrontSteps() == 2);
    const std::string text = led.Text();
    SC_EXPECT(text.find("The enemy's air front in the battery's sector 'b'") != std::string::npos);
    SC_EXPECT(text.find("held there by the battery's entry 5.0 h") != std::string::npos);
}

SC_TEST("outcomes (F6.2): in a running war the air front in the battery's sector is checked on the staff's own measure") {
    airwar::AirWarData d;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", d, &e));
    airwar::AirWarData off = d;
    d.tuning.values["air_front.mode"] = 1.0;
    airwar::AirCampaign c, c0;
    SC_EXPECT(c.Init(d, 21) && c0.Init(off, 21));
    while (c.Now() < 48.0) {
        if (c.Advance(48.0)) c.SkipPlayerMission();
    }
    const campaign::AirFrontCheck a = campaign::CheckAirFront(c, c.Now());
    SC_EXPECT(a.sector >= 0 && !a.sector_id.empty());
    SC_EXPECT(a.sector == c.FrontLine().SectorOf(c.Truth()[static_cast<std::size_t>(c.PlayerIndex())].pos));
    SC_EXPECT_NEAR(a.cover_with, c.SectorCovers()[static_cast<std::size_t>(a.sector)].cover, 1e-12);
    // Taking an entry off never raises the cover or lowers our mobile air defence's losses.
    SC_EXPECT(a.cover_without <= a.cover_with + 1e-12 && a.mobile_without >= a.mobile_with - 1e-12);
    // The gates are the faction's, moved by its chief's traits by at most air_front.trait_shift.
    const double shift = d.tuning.Get("air_front.trait_shift");
    SC_EXPECT(a.gate_cover >= d.faction.air_front.cover_at_most * (1.0 - shift) - 1e-12 && a.gate_cover <= d.faction.air_front.cover_at_most * (1.0 + shift) + 1e-12);
    SC_EXPECT(a.depth_m == c.AirFront().sectors[static_cast<std::size_t>(a.sector)].depth_m);
    // At air_front.mode 0 there is no air front to check.
    SC_EXPECT(campaign::CheckAirFront(c0, c0.Now()).sector == -1);
}
