// The scopes' feed: the same from the engagement model and from the knowledge stream; target ids never travel.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/readouts.hpp"
#include "simcore/scope_feed.hpp"
#include "simcore/scopes.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {
const std::string kData = SIMCORE_DATA_DIR;
}

SC_TEST("scope feed: the practice scenario's scopes reach a client through the stream, without target ids") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice_em.json", kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner r(std::move(def), std::move(console));
    // run until the search radar shows the drone
    ScopeFeed feed;
    for (int i = 0; i < 20 * 400 && feed.search.empty(); ++i) {
        r.Step(0.05);
        feed = ScopeFeedOf(r.Engagement(), r.GetLauncher(), r.Time());
    }
    std::printf("    first echo on the search scope at %.1f s\n", r.Time());
    SC_EXPECT(!feed.search.empty());
    EchoKeys keys;
    em::KnowledgeView v;
    v.scope = ToViewScope(feed, keys);
    const std::vector<std::uint8_t> bytes = em::EncodeView(v);
    for (const Contact& c : feed.search) {
        SC_EXPECT(std::search(bytes.begin(), bytes.end(), c.target_id.begin(), c.target_id.end()) == bytes.end());
    }
    em::KnowledgeView back;
    SC_EXPECT(em::DecodeView(bytes, back));
    const ScopeFeed client = FromViewScope(back.scope);
    SC_EXPECT(client.search.size() == feed.search.size());
    if (!client.search.empty()) {
        SC_EXPECT(std::fabs(client.search[0].range_m - feed.search[0].range_m) < 1.0);
        SC_EXPECT(std::fabs(client.search[0].azimuth_deg - feed.search[0].azimuth_deg) < 1e-3);
        SC_EXPECT(client.search[0].target_id != feed.search[0].target_id);
    }
    // the same echo keeps its key from one step to the next
    const int k1 = keys.Key(feed.search[0].target_id);
    SC_EXPECT(keys.Key(feed.search[0].target_id) == k1);
    // counters and the TV's pointing read the same from either feed
    for (const char* ro : {"range_error", "elevation_error", "azimuth_fine"}) {
        SC_EXPECT(CounterReading(ro, r.Console(), r.Engagement()) == CounterReading(ro, r.Console(), feed.track_returns, feed.antenna_el_deg));
    }
    SC_EXPECT(std::fabs(feed.IndicatedToTrueDeg(30.0) - r.Engagement().IndicatedToTrueDeg(30.0)) < 1e-9);
}

SC_TEST("search scope: a strong echo paints brighter than a weak one; a jammer's strobe paints as the sweep passes it") {
    SearchScope scope(2.0, 4.0);  // placeholder: one turn in 2 s, 4 s of afterglow
    Contact strong{"a", 10000.0, 90.0, 1.0};
    strong.snr_db = 40.0;
    Contact weak{"b", 20000.0, 90.0, 1.0};
    weak.snr_db = 14.0;
    const std::vector<ScopeStrobe> strobes{{180.0, 20.0}};
    for (int i = 0; i < 20; ++i) scope.Update(0.1, true, {strong, weak}, {}, strobes);  // one full turn
    double b_strong = -1.0, b_weak = -1.0, b_strobe = -1.0;
    for (const SearchScope::Blip& b : scope.Blips()) {
        if (b.strobe) b_strobe = b.brightness;
        else if (b.target_id == "a") b_strong = b.brightness;
        else if (b.target_id == "b") b_weak = b.brightness;
    }
    std::printf("    brightness: strong %.2f, weak %.2f, strobe %.2f\n", b_strong, b_weak, b_strobe);
    SC_EXPECT(b_strong > 0.0 && b_weak > 0.0 && b_strobe > 0.0);
    SC_EXPECT(b_strong > b_weak + 0.3);
    // The strengths before fading: 40 dB full, 14 dB 0.35 + 0.65/25 = 0.376, a 20 dB strobe 0.25 + 0.75 x 17/30 = 0.675.
    SC_EXPECT_NEAR(SearchScope::EchoStrength(40.0), 1.0, 1e-9);
    SC_EXPECT_NEAR(SearchScope::EchoStrength(14.0), 0.376, 1e-9);
    SC_EXPECT_NEAR(SearchScope::StrobeStrength(20.0), 0.675, 1e-9);
    SC_EXPECT_NEAR(SearchScope::EchoStrength(Contact::kUnknownSnrDb), 1.0, 1e-9);  // the gameplay rules: full
}

SC_TEST("search scope: the sweep follows the EM antenna and paints what its beam swept") {
    SearchScope scope(2.0, 4.0);
    const Contact c{"a", 10000.0, 100.0, 1.0};
    // The antenna is at 95 deg after this step: nothing at 100 deg painted yet.
    scope.Update(0.1, true, {c}, {}, {}, DegToRad(95.0));
    SC_EXPECT(scope.Blips().empty());
    SC_EXPECT_NEAR(RadToDeg(scope.SweepAzimuthRad()), 95.0, 1e-9);  // where the antenna is
    // Next step it reaches 113 deg: the beam passed 100 deg, so the echo paints.
    scope.Update(0.1, true, {c}, {}, {}, DegToRad(113.0));
    SC_EXPECT(scope.Blips().size() == 1);
}

SC_TEST("scope feed: a stand-off jammer's strobe and the echoes' strength reach the scope and a client") {
    // The EM practice shoot with an ALQ-99-class jammer in its band 8 (4-7.8 GHz, the search radar's) standing off
    // 30 km east and 50 km north (placeholder placement).
    std::ifstream f(kData + "/scenarios/osa_practice_em.json", std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string text = ss.str();
    const std::string em = "\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\" }";
    const std::size_t at = text.find(em);
    SC_EXPECT(at != std::string::npos);
    if (at == std::string::npos) return;
    text.replace(at, em.size(),
                 "\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\", "
                 "\"extra_libraries\": [\"em/nato_1985.json\"], \"jammers\": [ { \"jammer\": \"nato_alq99_band8\", "
                 "\"position\": { \"east_m\": 30000, \"north_m\": 50000, \"up_m\": 6000 }, \"on_at_s\": 0 } ] }");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenario(text, kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner r(std::move(def), std::move(console));
    ScopeFeed feed;
    for (int i = 0; i < 20 * 600 && feed.strobes.empty(); ++i) {
        r.Step(0.05);
        feed = ScopeFeedOf(r.Engagement(), r.GetLauncher(), r.Time());
    }
    std::printf("    first strobe at %.1f s, azimuth %.1f deg, J/N %.1f dB; antenna at %.1f deg\n", r.Time(),
                feed.strobes.empty() ? 0.0 : feed.strobes[0].azimuth_deg, feed.strobes.empty() ? 0.0 : feed.strobes[0].jn_db,
                feed.search_antenna_az_deg);
    SC_EXPECT(!feed.strobes.empty());
    SC_EXPECT(feed.search_antenna_az_deg > -180.1 && feed.search_antenna_az_deg < 180.1);
    // In the crew's frame (the gyro oriented to true north: the scenario orients the vehicle).
    if (!feed.strobes.empty()) SC_EXPECT(std::fabs(feed.strobes[0].azimuth_deg - RadToDeg(std::atan2(30000.0, 50000.0))) < 6.0);
    EchoKeys keys;
    em::KnowledgeView v;
    v.scope = ToViewScope(feed, keys);
    em::KnowledgeView back;
    SC_EXPECT(em::DecodeView(em::EncodeView(v), back));
    const ScopeFeed client = FromViewScope(back.scope);
    SC_EXPECT(client.strobes.size() == feed.strobes.size());
    if (!client.strobes.empty()) {
        SC_EXPECT(std::fabs(client.strobes[0].azimuth_deg - feed.strobes[0].azimuth_deg) < 1e-3);
        SC_EXPECT(std::fabs(client.strobes[0].jn_db - feed.strobes[0].jn_db) < 0.01);
    }
    SC_EXPECT(std::fabs(client.search_antenna_az_deg - feed.search_antenna_az_deg) < 1e-3);
    // The drone shows with its echo's strength once it is near enough.
    for (int i = 0; i < 20 * 600; ++i) {
        r.Step(0.05);
        feed = ScopeFeedOf(r.Engagement(), r.GetLauncher(), r.Time());
        if (!feed.search.empty()) break;
    }
    if (!feed.search.empty()) std::printf("    echo at %.1f km, %.1f dB\n", feed.search[0].range_m / 1000.0, feed.search[0].snr_db);
    SC_EXPECT(!feed.search.empty() && feed.search[0].snr_db > 0.0);
    if (!feed.search.empty()) SC_EXPECT(client.search.size() <= feed.search.size() + 10);
}
