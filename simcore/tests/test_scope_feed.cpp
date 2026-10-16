// The scopes' feed: the same from the engagement model and from the knowledge stream; target ids never travel.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/readouts.hpp"
#include "simcore/scope_feed.hpp"
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
