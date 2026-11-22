// Track F, F5.3: the map the campaign screen draws (simcore/command/campaign_situation.hpp, docs/fronts/05_WHAT_THE_PLAYER_SEES.md
// section 3). Under the fronts it is the battery's regiment's situation map and nothing else: the same marks the regiment's post
// holds, none the regiment was not told, cached between asks; with the fronts off there is no map and the screen stays as it was.
#include <cmath>
#include <cstdio>
#include <string>

#include "simcore/air_war.hpp"
#include "simcore/command.hpp"
#include "simcore/command/campaign_situation.hpp"
#include "simcore/ground_war.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;
namespace command = simcore::command;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

struct ScreenRun {
    AirWarData data;
    AirCampaign campaign;
    command::InfoNet net;
    command::CampaignFeed feed;

    bool Go(bool fronts, unsigned seed, double until_h) {
        std::string e;
        if (!LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_nato.json", data, &e)) return false;
        if (fronts) {
            data.tuning.values["ground.mode"] = 1.0;
            data.tuning.values["air_front.mode"] = 1.0;
            data.tuning.values["ad_strategy.mode"] = 1.0;
        }
        if (!campaign.Init(data, seed, &e)) return false;
        command::ChainData chain;
        if (!command::LoadChainData(kData, "command/chain_pannonia.json", chain, &e)) return false;
        net.Init(chain, seed, data.theatre.start_hour);
        feed.Bind(&net);
        campaign.SetOrderSource(feed.OrderSource());
        To(until_h);
        return true;
    }
    void To(double h) {
        while (campaign.Now() < h) {
            if (campaign.Advance(h)) campaign.SkipPlayerMission();
            feed.Pump(campaign);
        }
    }
};

}  // namespace

SC_TEST("campaign screen (F5.3): the regiment's map and no other, cached, aging with the hour") {
    ScreenRun r;
    SC_EXPECT(r.Go(true, 9, 48.0));
    command::CampaignSituation screen;
    const command::SituationMap* m = screen.RegimentMap(r.campaign, r.net, r.feed);
    SC_EXPECT(m != nullptr);
    if (m == nullptr) return;
    SC_EXPECT(m->post == r.feed.Ground().Roles().regiment);
    // The same as the regiment's post builds from its facts: the screen has no marks of its own.
    command::SituationMap direct;
    const command::SituationLayout layout = command::SituationLayoutOf(r.campaign, &r.feed.Ad());
    command::BuildSituationMap(r.net, r.feed, layout, r.net.Data().PositionIndex(m->post), direct);
    SC_EXPECT(command::SituationMapJson(*m) == command::SituationMapJson(direct));
    SC_EXPECT(!m->Empty());
    // Every mark's fact is held by the regiment's post (or it is the plan).
    const int post = r.net.Data().PositionIndex(m->post);
    int bad = 0;
    const auto held = [&](const command::SituationMark& k) {
        if (k.key.empty()) {
            bad += k.source == "plan" ? 0 : 1;
        } else if (r.net.Known(post, r.net.FactIndex(k.key)) == nullptr) {
            ++bad;
        }
    };
    for (const command::MapAxis& a : m->axes) held(a.mark);
    for (const command::MapUnit& a : m->ours) held(a.mark);
    for (const command::MapEnemy& a : m->enemy) held(a.mark);
    for (const command::MapAirFront& a : m->air_front) held(a.mark);
    SC_EXPECT(bad == 0);
    // The enemy's air front only as a staff's conclusion that reached the regiment, never the air war's own state.
    for (const command::MapAirFront& a : m->air_front) SC_EXPECT(!a.mark.key.empty() && a.mark.h >= 0.0);
    // Asking again builds nothing; the army's post has its own map, different from the regiment's.
    const int builds = screen.Builds();
    SC_EXPECT(screen.RegimentMap(r.campaign, r.net, r.feed) == m && screen.Builds() == builds);
    const command::SituationMap* army = screen.MapFor(r.campaign, r.net, r.feed, "army_ops");
    SC_EXPECT(army != nullptr && army->post == "army_ops" && command::SituationMapJson(*army) != command::SituationMapJson(*m));
    SC_EXPECT(screen.MapFor(r.campaign, r.net, r.feed, "no_such_post") == nullptr);
    // An hour on with no new fact the marks are as old as they were and an hour older (stale stays stale).
    const double age = m->OldestAgeH();
    r.To(r.campaign.Now() + 1.0);
    const command::SituationMap* later = screen.RegimentMap(r.campaign, r.net, r.feed);
    SC_EXPECT(later != nullptr && later->now_h > m->now_h - 1e-9);
    SC_EXPECT(later->OldestAgeH() >= age - 1e-9);
    std::printf("    regiment's map: %zu axes, %zu units, %zu enemy, %zu air front conclusions; oldest %.1f h\n", later->axes.size(), later->ours.size(),
                later->enemy.size(), later->air_front.size(), later->OldestAgeH());
}

SC_TEST("campaign screen (F5.3): with the fronts off there is no map, the screen stays as it was") {
    ScreenRun r;
    SC_EXPECT(r.Go(false, 3, 24.0));
    command::CampaignSituation screen;
    SC_EXPECT(!r.feed.Ground().On());
    SC_EXPECT(screen.RegimentMap(r.campaign, r.net, r.feed) == nullptr);
    SC_EXPECT(screen.MapFor(r.campaign, r.net, r.feed, "army_ops") == nullptr);
    SC_EXPECT(screen.Builds() == 0);
}
