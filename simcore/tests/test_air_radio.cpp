// Roadmap B9: the enemy's radio and our intercept (simcore/air_war/air_radio.hpp, command/intercept.hpp,
// em/comms_intercept.hpp; docs/air_battle/09_COMMS_AND_INTERCEPT.md). Powers and sensitivities come from the library's
// estimates; every scenario number here is a generic placeholder.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/air_war/air_radio.hpp"
#include "simcore/air_war/save.hpp"
#include "simcore/campaign/mission_session.hpp"
#include "simcore/command/air_war_feed.hpp"
#include "simcore/command/intercept.hpp"
#include "simcore/em/data.hpp"
#include "simcore/flights.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// A point at an altitude over the sphere, in the ENU frame at the origin: the tangent plane lies above the surface by
// d^2 / 2R, so the same altitude is lower in z the farther out (CLAUDE.md: ENU z is not altitude far from the origin).
Vec3 Enu(double east_m, double north_m, double alt_m) {
    return Vec3{east_m, north_m, alt_m - (east_m * east_m + north_m * north_m) / (2.0 * 6371000.0)};
}

AirRadioDef Def(const std::string& radio = "nato_air_uhf_am") {
    AirRadioDef d;
    d.on = true;
    d.faction = "nato";
    d.library_file = "em/air_radios_1985.json";
    const em::EmLoadResult r = em::LoadEmLibraryFile(kData + "/" + d.library_file, d.library);
    if (!r.ok) std::printf("    library: %s\n", r.error.c_str());
    d.air_radio = radio;
    d.ground_radio = "air_ground_uhf_am";
    d.freq_hz = 300e6;
    d.language = "en";
    d.discipline = 0.8;
    d.callsign_rotation_h = 24.0;
    return d;
}

AirRadioPostDef Listener(const std::string& id, double east_m, double north_m, const std::string& chain = "intercept_post_1",
                                 const std::string& esm = "air_intercept_vhf_uhf") {
    AirRadioPostDef p;
    p.id = id;
    p.kind = "intercept";
    p.at = Vec3{east_m, north_m, 0.0};
    p.mast_m = 30.0;
    p.esm = esm;
    p.chain_post = chain;
    return p;
}

// A session of flights of one aircraft each, standing where they are put, and the radio on them.
struct Air {
    Flights flights;
    std::vector<Aircraft> aircraft;
    airwar::AirRadio radio;
    double t = 0.0;

    explicit Air(const AirRadioDef& def, std::uint64_t seed = 1) {
        std::string e;
        if (!radio.Init(def, GeoPoint::FromDegrees(47.0, 19.0, 0.0), nullptr, seed, &e)) std::printf("    radio: %s\n", e.c_str());
        radio.SetFlights(&flights);
        flights.SetRadio(&radio);
    }
    // A flight of n aircraft in a loose line at `at`, id `id`.
    int Add(const std::string& id, const Vec3& at, int n = 1, const FlightRadioInfo& info = FlightRadioInfo(), const std::string& callsign = "") {
        FlightState f;
        f.id = id;
        f.callsign = callsign.empty() ? id : callsign;
        std::vector<Vec3> starts;
        for (int k = 0; k < n; ++k) {
            Aircraft a;
            a.position = at + Vec3{800.0 * k, -500.0 * k, 0.0};
            a.velocity = Vec3{0.0, 200.0, 0.0};
            a.waypoints = {a.position + Vec3{0.0, 100000.0, 0.0}};
            aircraft.push_back(a);
            starts.push_back(a.position);
            FlightSlot s;
            s.target = static_cast<int>(aircraft.size()) - 1;
            s.number = k + 1;
            f.slots.push_back(s);
        }
        const int fi = flights.Add(f);
        radio.AddFlight(fi, flights.All()[static_cast<std::size_t>(fi)], info, starts);
        return fi;
    }
    Flights::Lookup Lookup() {
        return [this](int i) -> Aircraft* { return i >= 0 && i < static_cast<int>(aircraft.size()) ? &aircraft[static_cast<std::size_t>(i)] : nullptr; };
    }
    // Hold the aircraft where they are (no flying): the calls are what is tested.
    void Run(double until_s, double dt = 1.0) {
        const Flights::Lookup air = Lookup();
        while (t < until_s - 1e-9) {
            t += dt;
            radio.Step(t, dt, air);
        }
    }
    bool HeardBy(const std::string& post, int call) const {
        for (const airwar::Heard& h : radio.HeardLog()) {
            if (h.post == post && h.call == call) return true;
        }
        return false;
    }
};

}  // namespace

SC_TEST("air radio B9: a call is heard by a post's link budget and the horizon of its speaker (the table of section 4.2)") {
    AirRadioDef def = Def();
    def.posts.push_back(Listener("p1", 0.0, 0.0));
    AirRadioPostDef awacs;
    awacs.id = "awacs";
    awacs.kind = "awacs";
    awacs.at = Vec3{300000.0, 0.0, 0.0};
    awacs.mast_m = 9000.0;
    def.posts.push_back(awacs);
    Air a(def);
    a.Add("high", Enu(120000.0, 0.0, 1000.0));
    a.Add("low_far", Enu(80000.0, 5000.0, 50.0));
    a.Add("low_near", Enu(30000.0, -5000.0, 50.0));
    a.Run(1.0);
    const int hi = a.radio.SayAs("high", airwar::CallKind::CheckIn, "high, two aircraft, checking in", a.t);
    const int far_low = a.radio.SayAs("low_far", airwar::CallKind::CheckIn, "low far, two aircraft, checking in", a.t);
    const int near_low = a.radio.SayAs("low_near", airwar::CallKind::CheckIn, "low near, checking in", a.t);
    const int orbit = a.radio.SayFrom("awacs", airwar::CallKind::Admin, "high", "high, picture clean", a.t);
    SC_EXPECT(hi >= 0 && far_low >= 0 && near_low >= 0 && orbit >= 0);
    a.Run(60.0);
    SC_EXPECT(a.HeardBy("p1", hi));       // 1,000 m, 120 km: inside its 153 km horizon
    for (const airwar::Heard& h : a.radio.HeardLog()) std::printf("    heard call %d at %.1f dBW, margin %.1f dB, los %d\n", h.call, h.rx_dbw, h.margin_db, h.line_of_sight ? 1 : 0);
    SC_EXPECT(!a.HeardBy("p1", far_low)); // 50 m, 80 km: beyond its 52 km horizon
    SC_EXPECT(a.HeardBy("p1", near_low)); // 50 m, 30 km: inside it
    SC_EXPECT(a.HeardBy("p1", orbit));    // an E-3A's orbit, 300 km away: heard
    // What it heard is the time, the bearing and the length, nothing of the words.
    for (const airwar::Heard& h : a.radio.HeardLog()) {
        if (h.call != hi) continue;
        SC_EXPECT(h.end_s > h.start_s && std::fabs(h.bearing_rad - 1.5708) < 0.3);  // from the east
    }
}

SC_TEST("air radio B9: a net is half duplex: a call waits, a threat call breaks in after the one on the air, a stale call is dropped") {
    AirRadioDef def = Def();
    def.posts.push_back(Listener("p1", 0.0, 0.0));
    Air a(def);
    a.Add("f", Enu(20000.0, 0.0, 3000.0), 2);
    a.Run(1.0);
    const int first = a.radio.SayAs("f", airwar::CallKind::Admin, "f, admin one", a.t);
    const int second = a.radio.SayAs("f", airwar::CallKind::Admin, "f, admin two", a.t);
    const int threat = a.radio.SayAs("f", airwar::CallKind::ThreatCall, "f, SAM launch, break", a.t, false, Vec3(), false, 0.0, 2);
    SC_EXPECT(first >= 0 && second >= 0 && threat >= 0);
    const std::vector<airwar::Call>& c = a.radio.Calls();
    SC_EXPECT(c[static_cast<std::size_t>(second)].start_s >= c[static_cast<std::size_t>(first)].end_s - 1e-9);   // waits its turn
    SC_EXPECT(c[static_cast<std::size_t>(threat)].start_s >= c[static_cast<std::size_t>(first)].end_s - 1e-9);   // after the one on the air
    SC_EXPECT(c[static_cast<std::size_t>(threat)].start_s <= c[static_cast<std::size_t>(second)].start_s + 1e-9); // ahead of the one queued
    // A stressed pilot's call is longer.
    SC_EXPECT(c[static_cast<std::size_t>(threat)].end_s - c[static_cast<std::size_t>(threat)].start_s > a.radio.LengthOf(airwar::CallKind::ThreatCall, 0.0));
    // A net that has been busy for longer than a call is worth is not used: it is dropped.
    int dropped = 0;
    for (int i = 0; i < 40; ++i) {
        if (a.radio.SayAs("f", airwar::CallKind::Admin, "f, admin", a.t) == FlightRadio::kDropped) ++dropped;
    }
    SC_EXPECT(dropped > 0);
}

SC_TEST("air radio B9: a flight flying comm-out keeps routine calls off the air, passes the act by signal, and its codeword is traffic until the habit is known") {
    AirRadioDef def = Def();
    def.posts.push_back(Listener("p1", 0.0, 0.0));
    Air a(def);
    FlightRadioInfo info;
    info.role = "strike";
    info.comm_out = true;
    info.unit = "sq1";
    a.Add("silent", Enu(50000.0, 0.0, 4000.0), 2, info);
    a.Run(1.0);
    // A routine call: nothing on the air, passed by signal.
    SC_EXPECT(a.radio.SayAs("silent", airwar::CallKind::CheckIn, "silent, checking in", a.t) == FlightRadio::kSignalled);
    SC_EXPECT(a.radio.Calls().empty());
    // A threat call breaks the silence.
    const int threat = a.radio.SayAs("silent", airwar::CallKind::ThreatCall, "silent, break", a.t);
    SC_EXPECT(threat >= 0);
    // The codeword after the attack.
    const int word = a.radio.SayAs("silent", airwar::CallKind::Codeword, "SUPERFUSE", a.t + 30.0);
    SC_EXPECT(word >= 0);
    a.Run(120.0);
    command::InterceptData data;
    std::string e;
    SC_EXPECT(command::LoadInterceptData(kData, "command/intercept.json", data, &e));
    command::InterceptDesk desk;
    desk.Init(&data, 1, 0.0);
    const std::vector<command::Logged> logged = desk.Process(a.radio);
    bool saw_word = false;
    for (const command::Logged& l : logged) {
        if (l.call != word) continue;
        saw_word = true;
        SC_EXPECT(l.codeword && !l.habit && l.text.find("meaning not known") != std::string::npos);
    }
    SC_EXPECT(saw_word);
    // Once army intelligence has learned the habit, the same word is "strike complete".
    command::InterceptDesk habit;
    habit.Init(&data, 1, 0.0);
    habit.LearnHabit("SUPERFUSE");
    Air b(def);
    b.Add("silent", Enu(50000.0, 0.0, 4000.0), 2, info);
    b.Run(1.0);
    const int word2 = b.radio.SayAs("silent", airwar::CallKind::Codeword, "SUPERFUSE", b.t);
    b.Run(60.0);
    for (const command::Logged& l : habit.Process(b.radio)) {
        if (l.call == word2) SC_EXPECT(l.habit && l.text.find("strike complete") != std::string::npos);
    }
}

SC_TEST("air radio B9: a flight's order reaches a member only if he heard it; one who did not keeps the last order; a silent flight signals") {
    AirRadioDef def = Def();
    Air a(def);
    // Two members of one flight, the second 200 km off (beyond any horizon of a 50 m call): he hears nothing.
    a.Add("duo", Enu(20000.0, 0.0, 50.0), 2);
    a.aircraft[1].position = Enu(220000.0, 0.0, 50.0);
    a.Run(1.0);
    FlightOrder hold;
    hold.kind = OrderKind::Orbit;
    hold.has_aim = true;
    hold.aim = a.aircraft[0].position;
    hold.by = "leader";
    SC_EXPECT(a.flights.Order(0, hold, a.t));
    a.Run(60.0);
    SC_EXPECT(a.flights.All()[0].slots[0].order.kind == OrderKind::Orbit);
    SC_EXPECT(a.flights.All()[0].slots[1].order.kind == OrderKind::FollowRoute);  // not heard
    SC_EXPECT(a.radio.Calls().size() == 1);
    // The same two close by: he hears it, after the call has been said and he has taken a moment.
    Air b(def);
    b.Add("duo", Enu(20000.0, 0.0, 3000.0), 2);
    b.Run(1.0);
    SC_EXPECT(b.flights.Order(0, hold, b.t));
    const airwar::Call& c = b.radio.Calls().front();
    b.flights.After(c.end_s - 0.5);
    SC_EXPECT(b.flights.All()[0].slots[1].order.kind == OrderKind::FollowRoute);
    b.flights.After(c.end_s + b.flights.All()[0].call_s + 0.5);
    SC_EXPECT(b.flights.All()[0].slots[1].order.kind == OrderKind::Orbit);
    // A flight briefed silent says nothing on the air for an administrative order; a member in sight gets it by signal, later.
    FlightRadioInfo info;
    info.role = "strike";
    info.comm_out = true;
    Air s(def);
    s.Add("dumb", Enu(20000.0, 0.0, 3000.0), 2, info);
    s.Run(1.0);
    FlightOrder rejoin;
    rejoin.kind = OrderKind::Rejoin;
    rejoin.by = "leader";
    SC_EXPECT(s.flights.Order(0, rejoin, s.t));
    SC_EXPECT(s.radio.Calls().empty());
    s.flights.After(s.t + s.flights.All()[0].call_s + 0.1);
    SC_EXPECT(s.flights.All()[0].slots[1].order.kind == OrderKind::FollowRoute);  // not yet: a signal is slower than a call
    s.flights.After(s.t + 1.5 * s.flights.All()[0].call_s + 0.1);
    SC_EXPECT(s.flights.All()[0].slots[1].order.kind == OrderKind::Rejoin);
}

SC_TEST("air radio B9: a GCI post keeps to its channels: a group past them gets no vector") {
    AirRadioDef def = Def();
    AirRadioPostDef gci;
    gci.id = "gci1";
    gci.kind = "gci";
    gci.at = Vec3{0.0, 0.0, 0.0};
    gci.mast_m = 15.0;
    gci.channels = 2;
    def.posts.push_back(gci);
    Air a(def);
    for (const char* id : {"m1", "m2", "m3"}) a.Add(id, Enu(30000.0, 0.0, 6000.0));
    a.Run(1.0);
    const int v1 = a.radio.SayFrom("gci1", airwar::CallKind::Vector, "m1", "m1, heading 090", a.t);
    const int v2 = a.radio.SayFrom("gci1", airwar::CallKind::Vector, "m2", "m2, heading 090", a.t + 1.0);
    const int v3 = a.radio.SayFrom("gci1", airwar::CallKind::Vector, "m3", "m3, heading 090", a.t + 2.0);
    SC_EXPECT(v1 >= 0 && v2 >= 0 && v3 == FlightRadio::kDropped);
    SC_EXPECT(a.radio.GciChannelsInUse("gci1", a.t + 2.0) == 2);
    // The first group's channel stays its own while its vectors continue; the third gets one once the first lets go.
    SC_EXPECT(a.radio.SayFrom("gci1", airwar::CallKind::Vector, "m1", "m1, heading 080", a.t + 10.0) >= 0);
    SC_EXPECT(a.radio.SayFrom("gci1", airwar::CallKind::Vector, "m3", "m3, heading 090", a.t + 300.0) >= 0);
}

SC_TEST("air radio B9: a post with no linguist for the language logs time, bearing and length only; a weak one writes some digits wrong") {
    AirRadioDef def = Def();
    def.language = "ru";  // the centre has Russian; the forward post 1 has English and Pannonian only
    def.posts.push_back(Listener("intercept_post_1", 0.0, 0.0, "intercept_post_1"));
    command::InterceptData data;
    std::string e;
    SC_EXPECT(command::LoadInterceptData(kData, "command/intercept.json", data, &e));
    Air a(def);
    a.Add("ru1", Enu(25000.0, 0.0, 3000.0));
    a.Run(1.0);
    const int call = a.radio.SayAs("ru1", airwar::CallKind::Divert, "ru1, new target KA 4417", a.t, true, Vec3{44000.0, 17000.0, 0.0});
    a.Run(100.0);
    command::InterceptDesk desk;
    desk.Init(&data, 1, 0.0);
    std::vector<command::Logged> got = desk.Process(a.radio);
    SC_EXPECT(!got.empty() && got[0].call == call);
    // Post 1 relays it untranslated to the centre, which has a Russian linguist: read, later by the relay.
    SC_EXPECT(got[0].understood && got[0].via_relay && got[0].written_s > a.radio.Calls()[0].end_s + data.relay_untranslated_s - 1.0);
    // A language nobody has: traffic only.
    AirRadioDef odd = def;
    odd.language = "xx";
    Air b(odd);
    b.Add("xx1", Enu(25000.0, 0.0, 3000.0));
    b.Run(1.0);
    b.radio.SayAs("xx1", airwar::CallKind::Divert, "xx1, new target KA 4417", b.t, true, Vec3{44000.0, 17000.0, 0.0});
    b.Run(100.0);
    command::InterceptDesk d2;
    d2.Init(&data, 1, 0.0);
    const std::vector<command::Logged> traffic = d2.Process(b.radio);
    SC_EXPECT(traffic.size() == 1 && !traffic[0].understood && !traffic[0].has_place && !traffic[0].fast);
    SC_EXPECT(traffic[0].text.find("not read") != std::string::npos && traffic[0].text.find("KA") == std::string::npos);
    // A weak linguist (a recruit at the post): over many calls some digit is written wrong, and the wrong digit is in the form.
    command::InterceptData weak = data;
    weak.posts[1].linguists = {{"en", "recruit", 1}};
    AirRadioDef en = Def();
    en.posts.push_back(Listener("intercept_post_1", 0.0, 0.0, "intercept_post_1"));
    Air c(en);
    c.Add("en1", Enu(25000.0, 0.0, 3000.0));
    c.Run(1.0);
    for (int i = 0; i < 80; ++i) {
        c.radio.SayAs("en1", airwar::CallKind::Divert, "en1, new target KA 4417", c.t, true, Vec3{44000.0, 17000.0, 0.0});
        c.Run(c.t + 200.0);
    }
    command::InterceptDesk d3;
    d3.Init(&weak, 7, 0.0);
    int wrong = 0, right = 0;
    for (const command::Logged& l : d3.Process(c.radio)) {
        if (!l.understood) continue;
        if (l.text.find("KA 4417") != std::string::npos) ++right;
        else if (l.wrong) ++wrong;
    }
    SC_EXPECT(wrong > 3 && right > 20);
}

SC_TEST("air radio B9: a call sign change drops a post's continuity on a unit below 0.2, and about six hours of steady traffic bring it above 0.8") {
    AirRadioDef def = Def();
    def.callsign_rotation_h = 8.0;
    def.posts.push_back(Listener("intercept_post_1", 0.0, 0.0, "intercept_post_1"));
    command::InterceptData data;
    std::string e;
    SC_EXPECT(command::LoadInterceptData(kData, "command/intercept.json", data, &e));
    Air a(def);
    a.Add("hawk", Enu(25000.0, 0.0, 3000.0), 1, FlightRadioInfo(), "Hawk 21");
    command::InterceptDesk desk;
    desk.Init(&data, 3, 0.0);
    double before = 0.0, just_after = 1.0, later = 0.0;
    for (double h = 0.0; h < 15.0; h += 1.0 / 6.0) {  // a call every ten minutes
        a.Run(h * 3600.0 + 1.0, 60.0);
        a.radio.SayAs("hawk", airwar::CallKind::Admin, "Hawk 21, admin", a.t);
        a.Run(a.t + 120.0, 60.0);
        desk.Process(a.radio);
        const std::string unit = "hawk";
        if (h > 7.7 && h < 7.9) before = desk.Continuity("intercept_post_1", unit);
        if (h > 8.0 && h < 8.3) just_after = desk.Continuity("intercept_post_1", unit);
        if (h > 14.0 && h < 14.2) later = desk.Continuity("intercept_post_1", unit);
    }
    SC_EXPECT(before > 0.8);
    SC_EXPECT(just_after < 0.2);
    SC_EXPECT(later > 0.8);
}

SC_TEST("air radio B9: a HAVE QUICK unit is heard at the normal rate and understood at the reduced rate") {
    AirRadioDef def = Def();
    def.posts.push_back(Listener("intercept_post_1", 0.0, 0.0, "intercept_post_1"));
    command::InterceptData data;
    std::string e;
    SC_EXPECT(command::LoadInterceptData(kData, "command/intercept.json", data, &e));
    FlightRadioInfo hq;
    hq.have_quick = true;
    hq.unit = "hq";
    Air a(def);
    a.Add("hq", Enu(25000.0, 0.0, 3000.0), 1, hq);
    Air n(def);
    n.Add("plain", Enu(25000.0, 0.0, 3000.0));
    for (int i = 0; i < 150; ++i) {
        a.Run(a.t + 30.0, 5.0);
        a.radio.SayAs("hq", airwar::CallKind::Admin, "hq, admin", a.t);
        n.Run(n.t + 30.0, 5.0);
        n.radio.SayAs("plain", airwar::CallKind::Admin, "plain, admin", n.t);
    }
    a.Run(a.t + 60.0, 5.0);
    n.Run(n.t + 60.0, 5.0);
    SC_EXPECT(a.radio.HeardLog().size() == n.radio.HeardLog().size() && a.radio.HeardLog().size() > 100);
    command::InterceptDesk da, dn;
    da.Init(&data, 5, 0.0);
    dn.Init(&data, 5, 0.0);
    int ua = 0, un = 0;
    for (const command::Logged& l : da.Process(a.radio)) ua += l.understood ? 1 : 0;
    for (const command::Logged& l : dn.Process(n.radio)) un += l.understood ? 1 : 0;
    SC_EXPECT(un > 100);
    SC_EXPECT(ua > 10 && static_cast<double>(ua) < 0.4 * static_cast<double>(un));
    // No unit has HAVE QUICK II or secure voice in the 1985 theatre's data.
    for (const char* faction : {"nato", "warsaw_pact", "neutral", "rebels"}) {
        airwar::FactionDef f;
        std::string err;
        SC_EXPECT(airwar::LoadFactionJson(ReadFile(kData + "/air_war/faction_" + faction + ".json"), f, &err));
        SC_EXPECT(f.radio.present && f.radio.have_quick_share == 0.0 && f.radio.secure_voice_share == 0.0);
        SC_EXPECT(!f.radio.source.empty() && !f.radio.confidence.empty());
    }
}

SC_TEST("air radio B9: the staff's opsec draw: careless staff alerted about a third of its raids hours ahead, a careful one few with little lead") {
    AirRadioDef d;
    int careless = 0, careful = 0;
    double lead0 = 0.0, lead1 = 0.0;
    for (int k = 0; k < 400; ++k) {
        const airwar::OpsecAlert a0 = airwar::DrawOpsecAlert(0.0, 11, static_cast<std::uint64_t>(k), d);
        const airwar::OpsecAlert a1 = airwar::DrawOpsecAlert(1.0, 11, static_cast<std::uint64_t>(k), d);
        if (a0.alerted) {
            ++careless;
            lead0 += a0.lead_h;
        }
        if (a1.alerted) {
            ++careful;
            lead1 += a1.lead_h;
        }
    }
    SC_EXPECT(careless > 400 * 0.34 * 0.8 && careless < 400 * 0.34 * 1.2);
    SC_EXPECT(lead0 / careless > 8.5 * 0.9 && lead0 / careless < 8.5 * 1.1);
    SC_EXPECT(careful > 400 * 0.05 * 0.4 && careful < 400 * 0.05 * 1.8);
    SC_EXPECT(lead1 / careful < 0.5);
    // The same key and seed give the same draw.
    SC_EXPECT(airwar::DrawOpsecAlert(0.3, 4, 9, d).alerted == airwar::DrawOpsecAlert(0.3, 4, 9, d).alerted);
    // Loose lips lower the staff's opsec; a chatterbox lowers a flight's discipline.
    SC_EXPECT(airwar::StaffOpsec(0.8, 1.0, d) < 0.8 && airwar::StaffOpsec(0.8, 0.0, d) == 0.8);
    SC_EXPECT(airwar::RadioDiscipline(0.8, 0.0, 1.0, 0.0, d) < 0.8 && airwar::RadioDiscipline(0.8, 0.0, -1.0, 0.0, d) > 0.8 &&
              airwar::RadioDiscipline(0.8, 0.0, 0.0, 1.0, d) < 0.8);
    // The alert as a fact at the intercept centre.
    const std::vector<command::Fact> fs = command::OpsecAlertFacts(airwar::DrawOpsecAlert(0.0, 11, 3, d), 3, 20.0, "the Kesmark district");
    for (const command::Fact& f : fs) {
        SC_EXPECT(f.kind == command::FactKind::Intercept && f.origin == "intercept_centre" && f.text.find("Kesmark") != std::string::npos);
    }
}

SC_TEST("air radio B9: a heard divert naming the battery's square reaches the battery commander through the pipeline minutes later; a heard report naming it tells the regiment's chief of staff") {
    AirRadioDef def = Def();
    def.posts.push_back(Listener("intercept_post_1", 0.0, 0.0, "intercept_post_1"));
    command::InterceptData data;
    std::string e;
    SC_EXPECT(command::LoadInterceptData(kData, "command/intercept.json", data, &e));
    Air a(def);
    a.Add("f", Enu(25000.0, 0.0, 3000.0));
    a.Run(1.0);
    const Vec3 battery{6400.0, -9600.0, 0.0};
    const std::string square = airwar::GridOf(battery);
    const int div = a.radio.SayAs("f", airwar::CallKind::Divert, "f, new target " + square, a.t, true, battery);
    const int rep = a.radio.SayAs("f", airwar::CallKind::Report, "f, site found at " + square, a.t, true, battery);
    a.Run(120.0);
    command::InterceptDesk desk;
    data.tiers["class2"].digit_error = 0.0;  // read right: the slips are tested above
    data.tiers["class2"].word_drop = 0.0;
    data.tiers["class2"].doubt_chance = 0.0;
    desk.Init(&data, 1, 0.0);
    desk.Watch(battery);
    const std::vector<command::Logged> fresh = desk.Process(a.radio);
    command::InterceptBridge bridge;
    SC_EXPECT(bridge.Init(kData, "command/chain_pannonia.json", 1, 0.0, &e));
    const std::vector<command::InterceptBridge::Arrival> told = bridge.Feed(fresh, data, a.radio);
    bool seen_divert = false, knows = false;
    for (const command::Logged& l : fresh) {
        if (l.call == div) {
            seen_divert = true;
            SC_EXPECT(l.understood && l.fast && l.names_battery && l.precedence == 2);
            // It reached the battery: the form's delay, the liaison's five minutes, and the pipeline's means after.
            bool arrived = false;
            for (const command::InterceptBridge::Arrival& t : told) {
                if (t.key != "intercept:" + std::to_string(div) + ":intercept_post_1") continue;
                arrived = true;
                SC_EXPECT(t.at_s >= l.written_s + data.liaison_s - 1e-6);
                SC_EXPECT(t.at_s < l.written_s + data.liaison_s + 3600.0);  // within the hour
                SC_EXPECT(t.text.find(square) != std::string::npos && t.text.find("Regiment to the battery") == 0);
            }
            SC_EXPECT(arrived);
        }
        if (l.call == rep) {
            for (const command::Fact& f : command::InterceptFacts(l, data, l.chain_post, 0.0)) {
                if (f.kind == command::FactKind::EnemyKnowsBattery) knows = f.audience == "rgt_chief_of_staff";
            }
        }
    }
    SC_EXPECT(seen_divert && knows);
    // A call that names a place far from the battery is only priority 1.
    command::InterceptDesk far_desk;
    far_desk.Init(&data, 1, 0.0);
    far_desk.Watch(Vec3{-60000.0, 40000.0, 0.0});
    for (const command::Logged& l : far_desk.Process(a.radio)) SC_EXPECT(!l.fast || l.precedence == 1);
}

SC_TEST("air radio B9: the same seed gives the same log, and a package outcome's heard calls survive a save") {
    AirRadioDef def = Def();
    def.posts.push_back(Listener("intercept_post_1", 0.0, 0.0, "intercept_post_1"));
    command::InterceptData data;
    std::string e;
    SC_EXPECT(command::LoadInterceptData(kData, "command/intercept.json", data, &e));
    const auto run = [&](std::uint64_t seed, std::vector<command::Logged>& out) {
        Air a(def, seed);
        a.Add("f", Enu(25000.0, 0.0, 3000.0), 2);
        a.Run(1.0);
        for (int i = 0; i < 12; ++i) {
            a.radio.SayAs("f", airwar::CallKind::Divert, "f, new target KA 4417", a.t, true, Vec3{44000.0, 17000.0, 0.0});
            a.Run(a.t + 90.0);
        }
        command::InterceptDesk d;
        d.Init(&data, seed, 0.0);
        out = d.Process(a.radio);
        return a.radio.Calls().size();
    };
    std::vector<command::Logged> x, y, z;
    SC_EXPECT(run(1, x) == run(1, y));
    SC_EXPECT(x.size() == y.size() && !x.empty());
    for (std::size_t i = 0; i < x.size() && i < y.size(); ++i) SC_EXPECT(x[i].text == y[i].text && x[i].written_s == y[i].written_s && x[i].bearing_deg == y[i].bearing_deg);
    run(2, z);
    bool differs = z.size() != x.size();
    for (std::size_t i = 0; i < x.size() && i < z.size(); ++i) differs = differs || x[i].bearing_deg != z[i].bearing_deg;
    SC_EXPECT(differs);
    // The save.
    airwar::PackageOutcome o;
    o.package_id = 3;
    for (const command::Logged& l : x) {
        airwar::HeardRecord h;
        h.post = l.post;
        h.t_s = l.heard_s;
        h.kind = l.understood ? airwar::CallKindId(l.kind) : "traffic";
        h.text = l.text;
        h.understood = l.understood;
        h.fact = l.fast ? "intercept:" + std::to_string(l.call) + ":" + l.post : "";
        h.chain_post = l.chain_post;  // roadmap S6: what a load rebuilds the facts from
        h.h = 10.0 + l.heard_s / 3600.0;
        h.written_h = 10.0 + l.written_s / 3600.0;
        h.bearing_deg = l.bearing_deg;
        h.precedence = l.precedence;
        h.names_battery = l.names_battery;
        o.heard.push_back(h);
    }
    const json::Value v = airwar::save_detail::OutcomeJson(o);
    json::Value back;
    std::string perr;
    SC_EXPECT(json::Parse(airwar::jsonb::Dump(v, 1, 17), back, &perr));
    airwar::save_detail::Reader reader;
    const airwar::PackageOutcome o2 = reader.Outcome(back);
    SC_EXPECT(o2.heard.size() == o.heard.size() && !o2.heard.empty());
    for (std::size_t i = 0; i < o.heard.size() && i < o2.heard.size(); ++i) {
        SC_EXPECT(o2.heard[i].text == o.heard[i].text && o2.heard[i].fact == o.heard[i].fact && o2.heard[i].understood == o.heard[i].understood);
        SC_EXPECT(o2.heard[i].chain_post == o.heard[i].chain_post && std::fabs(o2.heard[i].h - o.heard[i].h) < 1e-9 &&
                  std::fabs(o2.heard[i].written_h - o.heard[i].written_h) < 1e-9 && std::fabs(o2.heard[i].bearing_deg - o.heard[i].bearing_deg) < 1e-9 &&
                  o2.heard[i].precedence == o.heard[i].precedence && o2.heard[i].names_battery == o.heard[i].names_battery);
    }
}

SC_TEST("air radio B9: the enemy's placed listeners hear our nets by link margin, and with none in reach hear nothing") {
    command::InterceptData data;
    std::string e;
    SC_EXPECT(command::LoadInterceptData(kData, "command/intercept.json", data, &e));
    em::RadioDef set;
    set.id = "t";
    set.freq_min_hz = 30e6;
    set.freq_max_hz = 76e6;
    set.power_w = 20.0;
    set.antenna_height_m = 3.0;
    set.sensitivity_dbw = -131.0;
    set.required_snr_db = 12.0;
    command::EmRadio radio(set, nullptr, 50e6);
    // Roadmap S6: the Pact's front intercept post behind its line (30 km east, 160 km north of the theatre's origin) and an
    // Il-20 on its orbit (20 km east, 200 km north, 8,000 m).
    SC_EXPECT(command::PlaceEnemyListeners(radio, data, "warsaw_pact") == 2);
    const command::RadioPath near_p = radio.Hop(0, 1, Vec3{30000.0, 155000.0, 0.0}, Vec3{25000.0, 155000.0, 0.0}, 1.0);
    // The Il-20 at 8,000 m sees a 3 m whip out to its radio horizon, about 376 km over the 4/3 earth: 350 km off, still short
    // of it, it hears the net (an airborne post "extends intercept" over the whole country: FM 100-2-1). 460 km off, some 85 km
    // past the horizon, the smooth earth's diffraction (ITU-R P.526-16 section 3) loses tens of dB more and nothing is heard.
    // (Until the S6 follow-ups the EM layer had only one knife edge over the bulge, about 24 dB at 670 km, and this point had
    // to be put 1,200 km off.)
    const command::RadioPath edge_p = radio.Hop(0, 1, Vec3{20000.0, -150000.0, 0.0}, Vec3{15000.0, -150000.0, 0.0}, 1.0);
    const command::RadioPath far_p = radio.Hop(0, 1, Vec3{20000.0, -260000.0, 0.0}, Vec3{15000.0, -260000.0, 0.0}, 1.0);
    std::printf("    the Il-20 hears a whip 350 km off %.2f, 460 km off %.3f\n", edge_p.heard, far_p.heard);
    SC_EXPECT(near_p.heard > 0.9);
    SC_EXPECT(edge_p.heard > 0.9);
    SC_EXPECT(far_p.heard < 0.05);
    command::EmRadio none(set, nullptr, 50e6);  // none placed: the abstract service hears everything (P3)
    SC_EXPECT(none.Hop(0, 1, Vec3{-90000.0, 60000.0, 0.0}, Vec3{-95000.0, 60000.0, 0.0}, 1.0).heard == 1.0);
}

SC_TEST("air radio B9: the scenario's air_radio block loads strictly and puts a flight's calls on the air in a running session") {
    const std::string base = ReadFile(kData + "/scenarios/osa_practice.json");
    const std::string block = R"("air_radio": { "faction": "nato", "library": "em/air_radios_1985.json", "radio": "nato_air_uhf_am", "ground_radio": "air_ground_uhf_am",
      "freq_mhz": 300, "language": "en", "discipline": 0.8, "opsec": 0.6, "source": "test", "confidence": "estimate",
      "tune": { "call_divert_s": 20 },
      "posts": [ { "id": "ip1", "kind": "intercept", "east_m": 0, "north_m": 0, "esm": "air_intercept_vhf_uhf", "chain_post": "intercept_post_1" } ] },
  "targets": [)";
    const std::string text = [&] {
        std::string s = base;
        const std::size_t at = s.find(R"("targets": [)");
        if (at != std::string::npos) s.replace(at, std::string(R"("targets": [)").size(), block);
        return s;
    }();
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenario(text, kData, def, console, &e));
    SC_EXPECT(def.air_radio.on && def.air_radio.posts.size() == 1 && def.air_radio.Tune("radio.call_divert_s") == 20.0);
    auto bad = [&](const std::string& t, const std::string& expect) {
        ScenarioDef d;
        ConsoleData c;
        std::string err;
        const bool ok = LoadScenario(t, kData, d, c, &err);
        if (ok || err.find(expect) == std::string::npos) std::printf("    expected \"%s\", got ok=%d \"%s\"\n", expect.c_str(), ok ? 1 : 0, err.c_str());
        return !ok && err.find(expect) != std::string::npos;
    };
    auto swap = [&](const std::string& from, const std::string& to) {
        std::string s = text;
        const std::size_t at = s.find(from);
        if (at != std::string::npos) s.replace(at, from.size(), to);
        return s;
    };
    SC_EXPECT(bad(swap(R"("faction": "nato",)", R"("faction": "nato", "pitch": 3,)"), "unknown key"));
    SC_EXPECT(bad(swap(R"("call_divert_s": 20)", R"("call_divert_x": 20)"), "unknown tuning value"));
    SC_EXPECT(bad(swap(R"("kind": "intercept")", R"("kind": "tower")"), "intercept, gci"));
    SC_EXPECT(bad(swap(R"("library": "em/air_radios_1985.json")", R"("library": "em/nowhere.json")"), "library"));
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.AirRadio() != nullptr && r.AirRadio()->On());
    for (int i = 0; i < 20; ++i) r.Step(0.1);
    // A scenario without the block has none.
    ScenarioDef d2;
    ConsoleData c2;
    SC_EXPECT(LoadScenario(base, kData, d2, c2, &e));
    ScenarioRunner r2(std::move(d2), std::move(c2));
    SC_EXPECT(r2.AirRadio() == nullptr);
}

SC_TEST("air radio B9: a Pact battery mission has our intercept service; a heard call reaches the battery's journal, written as heard, minutes after, and nothing is on a panel") {
    campaign::MissionSessionDef md;
    md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    md.faction = "warsaw_pact";
    md.seed = 1;
    md.day = 1;
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(md, kData, &e));
    if (s.Campaign() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    SC_EXPECT(s.InterceptOn());
    airwar::AirRadio* radio = s.Runner().AirRadio();
    SC_EXPECT(radio != nullptr);
    if (radio == nullptr || !s.InterceptOn()) return;
    // A synthetic flight of the enemy's, 30 km from the battery on the east flank, says a divert naming the battery's square.
    const Vec3 at = s.Runner().UnitPosition();
    FlightState f;
    f.id = "probe";
    f.callsign = "Probe 1";
    FlightSlot slot;
    slot.target = -1;
    slot.number = 1;
    f.slots.push_back(slot);
    const Vec3 where = Enu(at.x + 30000.0, at.y, 3000.0);
    radio->AddFlight(99, f, FlightRadioInfo(), {where});
    // Its place has to be one the posts hear: the intercept posts of the data are in the theatre, a kilometre or tens away.
    const int call = radio->SayAs("probe", airwar::CallKind::Divert, "Probe 1, new target " + airwar::GridOf(at), s.Runner().Time(), true, at);
    SC_EXPECT(call >= 0);
    const double said_s = s.Runner().Time();
    while (s.Runner().Time() < said_s + 1500.0) s.Step(0.5);
    // Whether the posts of the data were in reach of the probe is the geometry's; when they were, the call is in the journal.
    bool logged = false;
    for (const command::Logged& l : s.Desk().Log()) logged = logged || l.call == call;
    if (!logged) {
        std::printf("    no post heard the probe: geometry\n");
        return;
    }
    bool in_journal = false;
    double heard_at = -1.0;
    for (const RadioJournalEntry& j : s.Runner().Journal().Written()) {
        if (j.said.find("radio intercept") != std::string::npos) {
            in_journal = true;
            heard_at = j.heard_s;
            SC_EXPECT(j.said.find("Regiment to the battery") == 0);
        }
    }
    if (!in_journal) {
        for (const command::Logged& l : s.Desk().Log()) {
            if (l.call == call) std::printf("    logged: understood %d fast %d place %d names_battery %d written %.0f precedence %d text '%s'\n", l.understood ? 1 : 0, l.fast ? 1 : 0, l.has_place ? 1 : 0, l.names_battery ? 1 : 0, l.written_s, l.precedence, l.text.c_str());
        }
        for (const campaign::MissionSession::Told& t : s.InterceptTold()) std::printf("    told at %.0f: %s\n", t.at_s, t.text.c_str());
        for (const std::string& n : s.Notes()) std::printf("    note: %s\n", n.c_str());
        for (const ScenarioRunner::RadioMessage& m : s.Runner().RadioLog()) {
            if (m.text.find("intercept") != std::string::npos) {
                std::printf("    radio log %.0f net '%s' from '%s' heard_by %d routed %d\n", m.time_s, m.net.c_str(), m.from.c_str(), static_cast<int>(m.heard_by.size()), m.routed ? 1 : 0);
            }
        }
        for (const DebriefEntry& d : s.Runner().Log().Entries()) {
            if (d.what == "regiment net" || d.who.find("journal") != std::string::npos || d.detail.find("never written") != std::string::npos) {
                std::printf("    event %.0f %s %s: %s\n", d.time_s, d.who.c_str(), d.what.c_str(), d.detail.substr(0, 110).c_str());
            }
        }
        std::printf("    journal waiting %d forgotten %d\n", static_cast<int>(s.Runner().Journal().Waiting().size()), s.Runner().Journal().Forgotten());
        for (const RadioJournalEntry& j : s.Runner().Journal().Written()) std::printf("    written %.0f net '%s' said '%s'\n", j.heard_s, j.net.c_str(), j.said.c_str());
        std::printf("    now %.0f, said %.0f, journal entries %d\n", s.Runner().Time(), said_s, static_cast<int>(s.Runner().Journal().Written().size()));
    }
    SC_EXPECT(in_journal);
    // Minutes after the call, not before it: the linguist's delay, the liaison's five minutes and the pipeline's.
    SC_EXPECT(heard_at > said_s + 300.0 && heard_at < said_s + 1500.0);
    // The record of what was heard is the post's, by call.
    for (const command::Logged& l : s.Desk().Log()) {
        if (l.call == call) SC_EXPECT(!l.said.empty() && (l.understood ? l.text.size() > 0 : l.text.find("not read") != std::string::npos));
    }
}

SC_TEST("air radio B9: the radio and the intercept cost a fraction of a millisecond a step with 100 aircraft and five posts (the cost of section 4.5)") {
    AirRadioDef def = Def();
    for (int k = 0; k < 5; ++k) def.posts.push_back(Listener("p" + std::to_string(k), -40000.0 + 20000.0 * k, 0.0, "intercept_post_1"));
    Air a(def);
    for (int k = 0; k < 25; ++k) a.Add("f" + std::to_string(k), Enu(30000.0 + 3000.0 * k, 5000.0 * (k % 5), 3000.0), 4);
    a.Run(2.0, 0.5);
    // Some thirty calls a minute across the raid, a step of half a second.
    const auto start = std::chrono::steady_clock::now();
    int steps = 0;
    int said = 0;
    while (a.t < 602.0 - 1e-9) {
        if (steps % 40 == 0) {
            if (a.radio.SayAs("f" + std::to_string((steps / 40) % 25), airwar::CallKind::Report, "report", a.t) >= 0) ++said;
        }
        a.Run(a.t + 0.5, 0.5);
        ++steps;
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / static_cast<double>(steps);
    std::printf("    100 aircraft, 5 posts, %d calls in %d steps: %.3f ms a step (mean)\n", said, steps, ms);
    SC_EXPECT(said > 10);
    SC_EXPECT(ms < 5.0);  // a loaded machine is allowed far more than the 0.1 ms the design expects
}

// ------------------------------------------------------------------------------------------------ roadmap S6

namespace {

airwar::RoutePoint Point(double east_km, double north_km, double agl_m, double t_h, const char* tag) {
    airwar::RoutePoint r;
    r.pos = Vec3{east_km * 1000.0, north_km * 1000.0, 0.0};
    r.agl_m = agl_m;
    r.t_h = t_h;
    r.tag = tag;
    return r;
}

airwar::Flight S6Flight(const std::string& cs, airwar::FlightRole role) {
    airwar::Flight f;
    f.callsign = cs;
    f.role = role;
    f.squadron = cs + " squadron";
    f.aircraft = "f4e";
    f.count = 4;
    f.route = {Point(-200, 0, 1000, 1.0, "entry"), Point(-120, 0, 300, 1.2, "gate"), Point(-30, 0, 100, 1.4, "ip"),
               Point(0, 0, 100, 1.45, "target"), Point(20, 10, 300, 1.5, "egress"), Point(-200, 20, 3000, 1.9, "exit")};
    f.release_index = 3;
    f.aim_pos = Vec3{0.0, 0.0, 0.0};
    return f;
}

command::ChainData S6Chain() {
    command::ChainData d;
    std::string e;
    if (!command::LoadChainData(kData, "command/chain_pannonia.json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

command::InterceptData S6Intercept() {
    command::InterceptData d;
    std::string e;
    if (!command::LoadInterceptData(kData, "command/intercept.json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

bool Starts(const std::string& s, const char* p) { return s.compare(0, std::char_traits<char>::length(p), p) == 0; }

// The facts our feed made of the enemy's calls: key, hour and text.
std::vector<std::string> InterceptFactsOf(const command::InfoNet& net) {
    std::vector<std::string> out;
    for (const command::Fact& f : net.Facts()) {
        if (!Starts(f.key, "intercept")) continue;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.6f", f.h);
        out.push_back(f.key + "|" + buf + "|" + f.text);
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace

SC_TEST("air radio S6: an abstract package's calls keep the session's rules: a comm-out strike silent before its attack and its codeword after; threat calls always; the escort checks in") {
    airwar::FactionRadio fr;
    fr.present = true;
    fr.discipline = 1.0;  // every strike flight comm-out
    fr.language = "en";
    airwar::AirStaff staff;
    AirRadioDef td;
    airwar::Package p;
    p.id = 5;
    p.target_pos = Vec3{0.0, 0.0, 0.0};
    p.flights = {S6Flight("Hawk", airwar::FlightRole::Strike), S6Flight("Eagle", airwar::FlightRole::Escort)};
    p.flights[0].released = true;
    p.flights[0].lost = 1;
    const std::vector<airwar::AbstractCall> calls = airwar::AbstractPackageCalls(p, fr, staff, td, 7);
    int strike_before = 0, threat = 0, codeword = 0, escort_check_in = 0;
    std::set<int> ids;
    for (const airwar::AbstractCall& a : calls) {
        ids.insert(a.call.id);
        SC_EXPECT(a.package_id == 5 && a.call.id / 256 == 5 && !a.call.by_signal);
        if (a.call.flight == "Hawk") {
            if (a.call.start_s < 1.45 * 3600.0) ++strike_before;
            threat += a.call.kind == airwar::CallKind::ThreatCall ? 1 : 0;
            codeword += a.call.kind == airwar::CallKind::Codeword ? 1 : 0;
        } else {
            escort_check_in += a.call.kind == airwar::CallKind::CheckIn ? 1 : 0;
        }
    }
    SC_EXPECT(ids.size() == calls.size());
    SC_EXPECT(strike_before == 0 && threat == 1 && codeword == 1 && escort_check_in == 1);
    // A loose faction's strike talks on the way and names its target at the IP.
    fr.discipline = 0.2;
    bool named = false;
    for (const airwar::AbstractCall& a : airwar::AbstractPackageCalls(p, fr, staff, td, 7)) named = named || (a.call.flight == "Hawk" && a.call.has_where);
    SC_EXPECT(named);
    // A deception package's decoys talk as a strike would with the faction's demonstration share (section 8).
    airwar::Package dp;
    dp.id = 6;
    dp.kind = airwar::MissionKind::Deception;
    dp.target_pos = Vec3{5000.0, 5000.0, 0.0};
    dp.flights = {S6Flight("Ghost", airwar::FlightRole::Decoy)};
    fr.discipline = 1.0;
    const auto demonstrating = [&](double share) {
        fr.demonstration = share;
        int n = 0;
        for (const airwar::AbstractCall& a : airwar::AbstractPackageCalls(dp, fr, staff, td, 7)) n += a.call.has_where ? 1 : 0;
        return n;
    };
    SC_EXPECT(demonstrating(1.0) == 1 && demonstrating(0.0) == 0);
    int share = 0;
    fr.demonstration = 0.5;
    for (int id = 1; id <= 400; ++id) {
        dp.id = id;
        share += airwar::Demonstrates(dp, 0, fr, staff, td, 7) ? 1 : 0;
    }
    std::printf("    a demonstration share of 0.5 over 400 decoy flights: %d talk as strikes\n", share);
    SC_EXPECT(share > 150 && share < 250);
}

SC_TEST("air radio S6: our posts hear an abstract call by the radio horizon: a low flight far off not, an E-3A's at 300 km yes, an HF request only on the HF set") {
    AirRadioDef td;
    td.tune["radio.abstract_hear"] = 1.0;
    airwar::ListeningPost vhf, hf;
    vhf.id = "intercept_post_1";
    vhf.chain_post = "intercept_post_1";
    hf = vhf;
    hf.id = "intercept_post_1_hf";
    hf.hf = true;
    const auto heard = [&](const airwar::ListeningPost& post, double north_km, double height_m, bool on_hf) {
        airwar::AbstractCall a;
        a.call.id = 1;
        a.from = Vec3{0.0, north_km * 1000.0, 0.0};
        a.height_m = height_m;
        a.hf = on_hf;
        airwar::Heard h;
        return airwar::HearAbstract(a, post, td, 3, h);
    };
    SC_EXPECT(heard(vhf, 120.0, 1000.0, false));   // the horizon from 1,000 m to our 30 m mast: about 153 km
    SC_EXPECT(!heard(vhf, 80.0, 50.0, false));     // ... from 50 m about 52 km
    SC_EXPECT(heard(vhf, 300.0, 9000.0, false));   // an E-3A's orbit: about 413 km
    SC_EXPECT(!heard(vhf, 430.0, 9000.0, false));  // past the reach of any air set
    SC_EXPECT(heard(hf, 500.0, 15.0, true) && !heard(vhf, 50.0, 15.0, true) && !heard(hf, 20.0, 1000.0, false));
    // The bearing has the DF's error, about 3.5 degrees.
    double sum2 = 0.0;
    for (int id = 1; id <= 500; ++id) {
        airwar::AbstractCall a;
        a.call.id = id;
        a.from = Vec3{0.0, 100000.0, 0.0};
        a.height_m = 3000.0;
        airwar::Heard h;
        if (airwar::HearAbstract(a, vhf, td, 3, h)) sum2 += h.bearing_rad * h.bearing_rad;
    }
    const double sigma_deg = std::sqrt(sum2 / 500.0) * 180.0 / 3.14159265358979323846;
    std::printf("    the bearings' spread: %.2f deg\n", sigma_deg);
    SC_EXPECT(sigma_deg > 2.5 && sigma_deg < 4.5);
}

SC_TEST("air radio S6: a campaign's abstract raids reach the air section's day summary through the pipeline, the same pumped step by step or once after a load; none at radio.mode 0") {
    airwar::AirWarData aw;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", aw, &e));
    const command::ChainData chain = S6Chain();
    const command::InterceptData idata = S6Intercept();
    const unsigned seed = 3u;
    airwar::AirCampaign c;
    c.Init(aw, seed);
    command::InfoNet net;
    SC_EXPECT(net.Init(chain, seed, aw.theatre.start_hour));
    command::CampaignFeed feed;
    feed.Bind(&net);
    feed.UseIntercept(idata);
    while (c.Now() < 60.0) {
        if (c.Advance(60.0)) c.SkipPlayerMission();
        feed.Pump(c);
    }
    int summaries = 0;
    for (const command::Fact& f : net.Facts()) {
        if (f.kind != command::FactKind::Intsum) continue;
        ++summaries;
        SC_EXPECT(f.origin == "army_intel" && f.document && Starts(f.key, "intercept_intsum_campaign:"));
    }
    std::printf("    two and a half days against NATO: %zu abstract calls, %zu heard by our posts, %zu written up, %d day summaries\n", feed.AbstractCalls().size(),
                feed.AbstractHeard().size(), feed.AbstractDesk().Log().size(), summaries);
    SC_EXPECT(!feed.AbstractCalls().empty() && !feed.AbstractDesk().Log().empty() && summaries >= 1);
    // The canary (the first-person rule): every fact made of the enemy's calls is held by a person of the chain, and the
    // player's character has it only from someone who told him.
    for (const command::Fact& f : net.Facts()) {
        if (!Starts(f.key, "intercept")) continue;
        SC_EXPECT(chain.FindPosition(f.origin) != nullptr);
    }
    for (const command::Delivery& k : net.KnownBy(net.Player())) {
        const command::Fact& f = net.Facts()[static_cast<std::size_t>(k.fact)];
        if (Starts(f.key, "intercept")) SC_EXPECT(k.from >= 0 && k.h >= f.h - 1e-9);
    }
    // Loaded from its save, pumped once: the same facts, at the same hours, in the same words.
    airwar::AirCampaign loaded;
    SC_EXPECT(airwar::LoadCampaignJson(airwar::SaveCampaignJson(c, "air_war/theatre_pannonia.json", "air_war/faction_nato.json"), kData, loaded, &e));
    command::InfoNet net2;
    SC_EXPECT(net2.Init(chain, seed, aw.theatre.start_hour));
    command::CampaignFeed feed2;
    feed2.Bind(&net2);
    feed2.UseIntercept(idata);
    feed2.Pump(loaded);
    const std::vector<std::string> a = InterceptFactsOf(net), b = InterceptFactsOf(net2);
    SC_EXPECT(a == b && !a.empty());
    SC_EXPECT(feed2.AbstractDesk().Log().size() == feed.AbstractDesk().Log().size());
    for (std::size_t i = 0; i < feed.AbstractDesk().Log().size() && i < feed2.AbstractDesk().Log().size(); ++i) {
        SC_EXPECT(feed.AbstractDesk().Log()[i].text == feed2.AbstractDesk().Log()[i].text);
    }
    // At radio.mode 0 (a save made before S6) the service is silent.
    airwar::AirWarData old = aw;
    old.tuning.values["radio.mode"] = 0.0;
    airwar::AirCampaign c0;
    c0.Init(old, seed);
    command::InfoNet net0;
    SC_EXPECT(net0.Init(chain, seed, aw.theatre.start_hour));
    command::CampaignFeed feed0;
    feed0.Bind(&net0);
    feed0.UseIntercept(idata);
    while (c0.Now() < 30.0) {
        if (c0.Advance(30.0)) c0.SkipPlayerMission();
        feed0.Pump(c0);
    }
    SC_EXPECT(feed0.AbstractCalls().empty());
    for (const command::Fact& f : net0.Facts()) SC_EXPECT(f.kind != command::FactKind::Intsum);
}

SC_TEST("air radio S6: what our posts logged in a played session is rebuilt as the same facts from the save; a save made before S6 has none of it") {
    airwar::AirWarData aw;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", aw, &e));
    const command::ChainData chain = S6Chain();
    const command::InterceptData idata = S6Intercept();
    for (unsigned seed = 1u; seed <= 6u; ++seed) {
        airwar::AirCampaign c;
        c.Init(aw, seed);
        while (c.Now() < 72.0 && c.PlayerMission() == nullptr) c.Advance(72.0);
        const airwar::Package* pm = c.PlayerMission();
        if (pm == nullptr) continue;
        const int pid = pm->id;
        const double tot = pm->tot_h;
        airwar::PackageOutcome o;
        o.package_id = pid;
        airwar::HeardRecord divert;  // a divert naming a square, on the fast road
        divert.post = "intercept_post_1";
        divert.chain_post = "intercept_post_1";
        divert.kind = "divert";
        divert.text = "Hawk 1, new target KA 4417";
        divert.understood = true;
        divert.fact = "intercept:77:intercept_post_1";
        divert.h = tot - 0.2;
        divert.written_h = tot - 0.18;
        divert.bearing_deg = 123.0;
        divert.precedence = 2;
        o.heard.push_back(divert);
        airwar::HeardRecord report = divert;  // a report naming the battery's square
        report.kind = "report";
        report.text = "Hawk 1, SAM site at KA 4417";
        report.fact = "intercept:78:intercept_post_1";
        report.names_battery = true;
        o.heard.push_back(report);
        airwar::HeardRecord traffic = divert;  // not read: no fact
        traffic.kind = "traffic";
        traffic.understood = false;
        traffic.fact.clear();
        o.heard.push_back(traffic);
        c.CompletePlayerMission(o, tot + 0.5);
        command::InfoNet net;
        SC_EXPECT(net.Init(chain, seed, aw.theatre.start_hour));
        command::CampaignFeed feed;
        feed.Bind(&net);
        feed.UseIntercept(idata);
        feed.Pump(c);
        const std::string key = "intercept_played:" + std::to_string(pid) + ":77";
        const int fi = net.FactIndex(key);
        SC_EXPECT(fi >= 0);
        if (fi >= 0) {
            const command::Fact& f = net.Facts()[static_cast<std::size_t>(fi)];
            SC_EXPECT(f.kind == command::FactKind::Intercept && f.text.find("KA 4417") != std::string::npos && f.h > divert.written_h);
        }
        SC_EXPECT(net.FactIndex("intercept_played_knows:" + std::to_string(pid) + ":78") >= 0);
        int played = 0;
        for (const command::Fact& f : net.Facts()) played += Starts(f.key, "intercept_played") ? 1 : 0;
        SC_EXPECT(played == 3);  // the divert, the report on the fast road, the report's estimate; nothing of the traffic
        // The save, loaded: the same facts.
        const std::string text = airwar::SaveCampaignJson(c, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json");
        airwar::AirCampaign loaded;
        SC_EXPECT(airwar::LoadCampaignJson(text, kData, loaded, &e));
        command::InfoNet net2;
        SC_EXPECT(net2.Init(chain, seed, aw.theatre.start_hour));
        command::CampaignFeed feed2;
        feed2.Bind(&net2);
        feed2.UseIntercept(idata);
        feed2.Pump(loaded);
        SC_EXPECT(InterceptFactsOf(net2) == InterceptFactsOf(net));
        // Made before S6: no "radio_mode" in the save, so none of it.
        std::string old = text;
        const std::size_t at = old.find("\"radio_mode\"");
        SC_EXPECT(at != std::string::npos);
        if (at != std::string::npos) old.erase(at, old.find(',', at) - at + 1);
        airwar::AirCampaign before;
        SC_EXPECT(airwar::LoadCampaignJson(old, kData, before, &e));
        SC_EXPECT(before.Data().tuning.Get("radio.mode") == 0.0);
        command::InfoNet net0;
        SC_EXPECT(net0.Init(chain, seed, aw.theatre.start_hour));
        command::CampaignFeed feed0;
        feed0.Bind(&net0);
        feed0.UseIntercept(idata);
        feed0.Pump(before);
        for (const command::Fact& f : net0.Facts()) SC_EXPECT(!Starts(f.key, "intercept_played") && !Starts(f.key, "intercept_intsum_campaign"));
        std::printf("    seed %u: package %d's three records made %d facts; the loaded save the same\n", seed, pid, played);
        return;
    }
    SC_EXPECT(false);  // no seed gave a player mission in three days
}

SC_TEST("air radio S6: the enemy's listeners go on the campaign's radio from radio.mode 1; before it none, and the enemy hears everything") {
    airwar::AirWarData aw;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", aw, &e));
    const command::InterceptData idata = S6Intercept();
    em::RadioDef set;
    set.id = "t";
    set.freq_min_hz = 30e6;
    set.freq_max_hz = 76e6;
    set.power_w = 20.0;
    set.antenna_height_m = 3.0;
    set.sensitivity_dbw = -131.0;
    set.required_snr_db = 12.0;
    airwar::AirCampaign c;
    c.Init(aw, 1u);
    command::EmRadio radio(set, nullptr, 50e6);
    SC_EXPECT(command::PlaceCampaignListeners(radio, idata, c) == 2);
    aw.tuning.values["radio.mode"] = 0.0;
    airwar::AirCampaign c0;
    c0.Init(aw, 1u);
    command::EmRadio none(set, nullptr, 50e6);
    SC_EXPECT(command::PlaceCampaignListeners(none, idata, c0) == 0);
    SC_EXPECT(none.Hop(0, 1, Vec3{20000.0, -1000000.0, 0.0}, Vec3{15000.0, -1000000.0, 0.0}, 1.0).heard == 1.0);
    const double near_heard = radio.Hop(0, 1, Vec3{30000.0, 155000.0, 0.0}, Vec3{25000.0, 155000.0, 0.0}, 1.0).heard;
    // 460 km from the Il-20, past its 376 km radio horizon (the B9 listeners' test says why this distance)
    const double far_heard = radio.Hop(0, 1, Vec3{20000.0, -260000.0, 0.0}, Vec3{15000.0, -260000.0, 0.0}, 1.0).heard;
    std::printf("    the Pact's listeners hear a call by their post %.2f, 460 km off %.3f\n", near_heard, far_heard);
    SC_EXPECT(near_heard > 0.9 && far_heard < 0.05);
}

SC_TEST("air radio S6 follow-ups: our dummy transmitter talks on our fighters' net in its corridor's hours; the enemy's listeners hear it by the horizon, ours log nothing") {
    AirRadioDef def = Def();
    def.our_radio = "pact_air_r832m";
    def.our_freq_hz = 124e6;
    AirRadioPostDef dummy;
    dummy.id = "dummy_1";
    dummy.kind = "our_dummy";
    dummy.at = Vec3{0.0, 0.0, 0.0};
    dummy.mast_m = 10.0;
    dummy.corridor = "c1 B";
    dummy.from_s = 60.0;
    dummy.until_s = 600.0;
    dummy.period_s = 60.0;
    def.posts.push_back(dummy);
    // An Il-20 200 km off at 8,000 m (inside the 381 km horizon to a 10 m mast); a ground post 100 km off on a 30 m mast
    // (past the 36 km horizon between the two masts); our own intercept post beside the transmitter.
    AirRadioPostDef il20 = Listener("il20", 0.0, 200000.0, "");
    il20.kind = "enemy_intercept";
    il20.mast_m = 8000.0;
    def.posts.push_back(il20);
    AirRadioPostDef ground = Listener("ground", 0.0, 100000.0, "");
    ground.kind = "enemy_intercept";
    def.posts.push_back(ground);
    def.posts.push_back(Listener("p1", 5000.0, 0.0));
    Air a(def);
    a.Run(900.0);
    int heard = 0;
    for (const airwar::EnemyHeard& h : a.radio.EnemyHeardLog()) {
        SC_EXPECT(h.post == "il20");
        SC_EXPECT(h.dummy == "dummy_1" && h.corridor == "c1 B" && h.call == -1);
        // a call due by 600 s goes out with the next step and the net's turn (the last at 600.5 s)
        SC_EXPECT(h.start_s >= 60.0 - 1e-6 && h.start_s <= 600.0 + 2.0);
        ++heard;
    }
    std::printf("    the Il-20 heard the dummy %d times between 60 and 600 s; the ground post past the horizon none\n", heard);
    SC_EXPECT(heard >= 8 && heard <= 10);  // every hearing's post is checked above: the ground post's would have failed it
    SC_EXPECT(a.radio.EnemyHeardDummy("dummy_1"));
    SC_EXPECT(!a.radio.EnemyHeardDummy("p1"));
    SC_EXPECT(a.radio.HeardLog().empty());  // our own intercept post: not its business
}

SC_TEST("air radio S6 follow-ups: from radio.mode 2 the session and the campaign key names alike, the same on every compiler") {
    // FNV-1a's step by hand ("a" = 0x61), then SplitMix64's finaliser: no std::hash anywhere. The basis is the one every FNV
    // key in simcore has used since the air war began (1469598103934665603: the standard's 14695981039346656037 with its last
    // digit dropped); any odd basis spreads as well, and changing it would move every saved campaign's draws.
    SC_EXPECT(em::StringKey("") == em::Mix64(1469598103934665603ull));
    SC_EXPECT(em::StringKey("a") == em::Mix64((1469598103934665603ull ^ 0x61ull) * 1099511628211ull));
    SC_EXPECT(airwar::AbstractKeyOf("sq_1") == em::StringKey("sq_1"));
    SC_EXPECT(airwar::RadioKeyOf("sq_1", true) == em::StringKey("sq_1"));
    // A squadron's codeword and fit, and a flight's call sign on a later day, are one function in both levels.
    const airwar::SquadronFit f = airwar::SquadronRadioFit("sq_1", 0.5, 0.5, 7u);
    const airwar::SquadronFit g = airwar::SquadronRadioFit("sq_1", 0.5, 0.5, 7u);
    SC_EXPECT(!f.codeword.empty() && f.codeword == g.codeword && f.have_quick == g.have_quick && f.secure_voice == g.secure_voice);
    SC_EXPECT(airwar::CallsignAt("Ford 1", 30.0, 24.0, 7u, true) == airwar::CallsignAt("Ford 1", 30.0, 24.0, 7u, true));
    SC_EXPECT(airwar::CallsignAt("Ford 1", 30.0, 24.0, 7u, true) != "Ford 1");
    AirRadioDef def = Def();
    SC_EXPECT(airwar::StableRadioKeys(def));  // the data's radio.mode is 2
    def.tune["radio.mode"] = 1.0;
    SC_EXPECT(!airwar::StableRadioKeys(def));  // a save from before keeps std::hash's keys and replays as played
}

SC_TEST("air radio S6: the canary: an enemy call reaches the battery only as our intercept post's report through the pipeline, never on any other output") {
    campaign::MissionSessionDef md;
    md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    md.faction = "warsaw_pact";
    md.seed = 1;
    md.day = 1;
    campaign::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(md, kData, &e));
    if (s.Campaign() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    airwar::AirRadio* radio = s.Runner().AirRadio();
    SC_EXPECT(radio != nullptr && s.InterceptOn());
    if (radio == nullptr || !s.InterceptOn()) return;
    const Vec3 at = s.Runner().UnitPosition();
    FlightState f;
    f.id = "canary";
    f.callsign = "CANARY 7";
    FlightSlot slot;
    slot.target = -1;
    slot.number = 1;
    f.slots.push_back(slot);
    radio->AddFlight(98, f, FlightRadioInfo(), {Enu(at.x + 30000.0, at.y, 3000.0)});
    const int call = radio->SayAs("canary", airwar::CallKind::Divert, "CANARY 7, new target " + airwar::GridOf(at), s.Runner().Time(), true, at);
    SC_EXPECT(call >= 0);
    const double said_s = s.Runner().Time();
    while (s.Runner().Time() < said_s + 1800.0) s.Step(0.5);
    bool logged = false;
    for (const command::Logged& l : s.Desk().Log()) logged = logged || l.call == call;
    if (!logged) {
        std::printf("    no post heard the canary: geometry\n");
        return;
    }
    const auto has = [](const std::string& t) { return t.find("CANARY") != std::string::npos; };
    int by_pipeline = 0, elsewhere = 0;
    // The battery's nets: only the regiment's telling of the intercept post's report.
    for (const ScenarioRunner::RadioMessage& m : s.Runner().RadioLog()) {
        if (!has(m.text)) continue;
        if (m.from == "Regiment" && m.text.find("radio intercept") != std::string::npos) ++by_pipeline;
        else ++elsewhere;
    }
    // The radiotelephonist's journal: written as heard from the regiment.
    for (const RadioJournalEntry& j : s.Runner().Journal().Written()) {
        if (!has(j.said)) continue;
        if (Starts(j.said, "Regiment to the battery")) ++by_pipeline;
        else ++elsewhere;
    }
    // Papers and spoken deliveries: only the air section's summary.
    for (const DeliveryDef& d : s.Runner().Deliveries()) {
        if (!has(d.words) && !has(d.paper_text)) continue;
        if (Starts(d.id, "intercept_intsum")) ++by_pipeline;
        else ++elsewhere;
    }
    // Nothing anywhere else the player's character could see or hear.
    for (const campaign::LogEntry& l : s.Runner().CommanderLog()) elsewhere += has(l.callsign) || has(l.kind) ? 1 : 0;
    for (const campaign::SaidReport& r : s.Runner().RadarLog()) elsewhere += has(r.text) ? 1 : 0;
    for (const ScenarioRunner::CrewCall& c : s.Runner().CrewCalls()) elsewhere += has(c.target) ? 1 : 0;
    for (const ScenarioRunner::GroupHeard& g : s.Runner().GroupsHeard()) elsewhere += has(g.code) ? 1 : 0;
    for (const ScenarioRunner::MadeReport& r : s.Runner().ReportsMade()) elsewhere += has(r.callout) ? 1 : 0;
    for (const ScenarioRunner::GivenOrder& o : s.Runner().OrdersGiven()) elsewhere += has(o.order) ? 1 : 0;
    for (const campaign::MissionSession::Told& t : s.InterceptTold()) SC_EXPECT(!has(t.text) || t.text.find("radio intercept") != std::string::npos);
    std::printf("    the canary call: %d outputs by the pipeline, %d elsewhere\n", by_pipeline, elsewhere);
    SC_EXPECT(elsewhere == 0);
    SC_EXPECT(by_pipeline > 0);
}
