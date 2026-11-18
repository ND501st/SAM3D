// The enemy's jamming of our voice nets (roadmap 16.2, first form): simcore/air_war/comms_jamming.hpp.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/air_war/comms_jamming.hpp"
#include "simcore/radio_nets.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

}  // namespace

SC_TEST("comms jamming (16.2): FM 34-40-7's rule reproduces its worked examples") {
    // A 5 W set 9 km from its receiver, the jammer 17 km from it, n = 4: J/S = 2 (capture) needs 2 x 5 x (17/9)^4 W.
    const double need_w = 2.0 * 5.0 * std::pow(17.0 / 9.0, 4.0);
    SC_EXPECT_NEAR(need_w, 127.30, 0.01);  // the handbook rounds to about 125 W
    SC_EXPECT_NEAR(airwar::JammingToSignal(need_w, 5.0, 17000.0, 9000.0, 4.0), 2.0, 1e-9);
    // 1.5 kW against the same set captures its receiver from at most 31.5 km.
    SC_EXPECT_NEAR(airwar::JammerReach(1500.0, 5.0, 9000.0, 4.0, 2.0), 31496.0, 5.0);
    // Five times the power on our side (the R-111's 20 % to 100 %) cuts the jammer's reach by 5^(1/n).
    SC_EXPECT_NEAR(airwar::JammerReach(1500.0, 25.0, 9000.0, 4.0, 2.0) / airwar::JammerReach(1500.0, 5.0, 9000.0, 4.0, 2.0),
                   std::pow(0.2, 0.25), 1e-9);
    SC_EXPECT(airwar::JammingToSignal(0.0, 5.0, 1000.0, 1000.0, 3.0) == 0.0);
}

SC_TEST("comms jamming (16.2): a spot jammer with a band reaches only the net's frequencies in it; the spare outside escapes") {
    RadioDataDef d;
    RadioNetDef battery;
    battery.id = "battery";
    battery.frequencies_mhz = {44.0, 42.2};
    d.nets = {battery};
    d.present = true;
    JammingDef j;
    j.nets = {"battery"};
    j.band_lo_mhz = 43.0;
    j.band_hi_mhz = 50.0;
    j.spot = true;
    j.strong = true;
    j.follow_s = 60.0;
    d.jamming = {j};
    RadioNets n(d, CommsTuning(), 3u);
    n.Step(1.0);
    SC_EXPECT(n.Jam("battery", 1.0) == 2);
    SC_EXPECT(n.MoveTo("battery", 2, 10.0, "test"));
    n.Step(100.0);  // past its follow time: it would have found no. 2, but 42.2 MHz is below its band
    SC_EXPECT(n.Jam("battery", 100.0) == 0);
}

SC_TEST("comms jamming (16.2): the faction's jammers by the front: a battery near NATO's line is jammed, the Kesmark battery is beyond reach") {
    airwar::AirWarData d;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", d, &e));
    SC_EXPECT(!d.faction.comms_jamming.empty());
    if (d.faction.comms_jamming.empty()) return;
    airwar::Package p;
    p.id = 3;
    p.kind = airwar::MissionKind::Dead;
    airwar::Flight sead;
    sead.role = airwar::FlightRole::Sead;
    sead.count = 2;
    sead.arms = 2;
    sead.route = {{}, {}};
    sead.route[0].t_h = 1.0;
    sead.route[1].t_h = 2.0;
    p.flights = {sead};
    p.tot_h = 1.5;
    // The battery: the post at its centre, four launchers 1.8 km out (each link 1803 m).
    airwar::JammedBattery b;
    b.receivers = {Vec3{1500.0, 1000.0, 0.0}, Vec3{-1500.0, 1000.0, 0.0}, Vec3{1500.0, -1000.0, 0.0}, Vec3{-1500.0, -1000.0, 0.0}};
    b.net_mhz = {44.0, 42.2};
    const airwar::FrontDef* front = nullptr;
    for (const airwar::FrontDef& f : d.theatre.fronts) front = f.faction == "nato" ? &f : front;
    SC_EXPECT(front != nullptr);
    if (front == nullptr) return;
    // 5 km east of NATO's line.
    const Vec3 on_line = airwar::NearestOnLine(front->line, Vec3{-150000.0, 40000.0, 0.0});
    b.centre = on_line + Vec3{5000.0, 0.0, 0.0};
    const std::vector<airwar::VoiceJammer> near = airwar::VoiceJammersFor(d, p, b);
    SC_EXPECT(!near.empty());
    const double n = d.tuning.Get("comms_jam.terrain_n"), own = d.tuning.Get("comms_jam.own_power_w");
    for (const airwar::VoiceJammer& v : near) {
        std::printf("    %s: %s\n", v.name.c_str(), v.why.c_str());
        const airwar::CommsJammerDef* c = nullptr;
        for (const airwar::CommsJammerDef& x : d.faction.comms_jamming) c = x.id == v.id ? &x : c;
        if (c == nullptr || c->power_w <= 0.0 || c->barrage) continue;
        // By hand: its power against our 20 W, the worst receiver's 1803 m link against its distance.
        SC_EXPECT_NEAR(v.js, c->power_w / own * std::pow(std::hypot(1500.0, 1000.0) / v.distance_m, n), 1e-9);
        SC_EXPECT(v.strength == (v.js >= 2.0 ? 2 : v.js >= 0.5 ? 1 : 0));
    }
    bool any = false;
    for (const airwar::VoiceJammer& v : near) any = any || v.strength > 0;
    SC_EXPECT(any);
    // Its windows: from the take-off to the last out, off 3 minutes before the time on target (the SEAD carries ARMs).
    const std::vector<JammingDef> defs = airwar::VoiceJammingDefs(d, p, near, 0.0);
    SC_EXPECT(!defs.empty());
    for (const JammingDef& j : defs) {
        SC_EXPECT(j.nets.size() == 1 && j.nets[0] == "battery");
        SC_EXPECT((std::fabs(j.from_s - 3600.0) < 1e-6 && std::fabs(j.to_s - (5400.0 - 180.0)) < 1e-6) ||
                  (std::fabs(j.from_s - 5400.0) < 1e-6 && std::fabs(j.to_s - 7200.0) < 1e-6));
    }
    // The bridges' battery near Kesmark: 150 km from the line, beyond every one.
    b.centre = Vec3{6400.0, -9600.0, 0.0};
    for (const airwar::VoiceJammer& v : airwar::VoiceJammersFor(d, p, b)) {
        SC_EXPECT(v.strength == 0 && v.why.find("beyond") != std::string::npos);
    }
    // The rebels have none.
    airwar::AirWarData rebels;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_rebels.json", rebels, &e));
    SC_EXPECT(rebels.faction.comms_jamming.empty());
}

SC_TEST("comms jamming (16.2): the battery net jammed on the package's times, the battery commander moves it to the spare") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_mission_cover.json", kData, def, console, &e));
    ScenarioRunner r(std::move(def), std::move(console));
    JammingDef j;
    j.nets = {"battery"};
    j.band_lo_mhz = 30.0;
    j.band_hi_mhz = 100.0;
    j.spot = true;
    j.strong = true;
    j.follow_s = -1.0;
    j.from_s = 20.0;
    j.to_s = 600.0;
    SC_EXPECT(r.AddNetJamming(j, "package 9: a test jammer"));
    JammingDef none = j;
    none.nets = {"no such net"};
    SC_EXPECT(!r.AddNetJamming(none, "nobody"));
    while (r.Time() < 200.0) r.Step(0.1);
    bool logged = false, moved = false;
    for (const DebriefEntry& entry : r.Log().Entries()) {
        logged = logged || (entry.what == "battery net" && entry.detail.find("package 9: a test jammer: strong spot jamming") == 0);
        moved = moved || entry.detail.find("to frequency no. 2") != std::string::npos;
    }
    SC_EXPECT(logged);
    SC_EXPECT(moved);
}
