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
