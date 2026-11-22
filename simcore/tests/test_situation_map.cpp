// Track F, F5.1: the situation map a post holds (simcore/command/situation_map.hpp, docs/fronts/05_WHAT_THE_PLAYER_SEES.md
// sections 1 and 6). A post's map is built from the facts it holds and from nothing else: the believed line by axis, our
// units and the enemy as reported, the orders and sector priorities held, the air front's conclusions, our SAM units'
// places; each mark with its source, its hour and its age. Two posts hold two maps, a stale report stays stale while the
// truth moves, and the view a client gets holds nothing its post's facts do not.
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/campaign/mission_session.hpp"
#include "simcore/command.hpp"
#include "simcore/ground_war.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;
namespace command = simcore::command;
namespace cmp = simcore::campaign;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

command::ChainData LoadChain() {
    command::ChainData d;
    std::string e;
    if (!command::LoadChainData(kData, "command/chain_pannonia.json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

// A faction's war on the nation with every front mode on (the ground war, the air front, our commanders), our units moving
// on their commanders' orders, the feed pumped after every step.
struct MapRun {
    AirWarData data;
    AirCampaign campaign;
    command::InfoNet net;
    command::CampaignFeed feed;
    command::SituationLayout layout;
    command::OwnAirDef own_air;
    bool ok = false;

    bool Go(const std::string& faction, unsigned seed, double until_h) {
        std::string e;
        if (!LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_" + faction + ".json", data, &e)) {
            std::printf("    %s\n", e.c_str());
            return false;
        }
        data.tuning.values["ground.mode"] = 1.0;
        data.tuning.values["air_front.mode"] = 1.0;
        data.tuning.values["ad_strategy.mode"] = 1.0;
        if (!campaign.Init(data, seed, &e)) {
            std::printf("    %s\n", e.c_str());
            return false;
        }
        net.Init(LoadChain(), seed, data.theatre.start_hour);
        feed.Bind(&net);
        campaign.SetOrderSource(feed.OrderSource());
        if (!command::LoadOwnAirFile(kData + "/command/own_air.json", own_air, &e)) std::printf("    %s\n", e.c_str());
        To(until_h);
        layout = command::SituationLayoutOf(campaign, &feed.Ad(), &own_air);
        ok = true;
        return true;
    }
    void To(double h) {
        while (campaign.Now() < h) {
            if (campaign.Advance(h)) campaign.SkipPlayerMission();
            feed.Pump(campaign);
        }
    }
    int Post(const std::string& id) const { return net.Data().PositionIndex(id); }
    command::SituationMap MapOf(const std::string& id) const {
        command::SituationMap m;
        command::BuildSituationMap(net, feed, layout, Post(id), m);
        return m;
    }
};

template <class F>
void EachMark(const command::SituationMap& m, F&& f) {
    for (const command::MapAxis& x : m.axes) f(x.mark);
    for (const command::MapUnit& x : m.ours) f(x.mark);
    for (const command::MapEnemy& x : m.enemy) f(x.mark);
    for (const command::MapPriority& x : m.priorities) f(x.mark);
    for (const command::MapAirFront& x : m.air_front) f(x.mark);
    for (const command::MapSam& x : m.sams) f(x.mark);
    for (const command::MapOrder& x : m.orders) f(x.mark);
}

// Every mark rests on a fact its post holds (or on the plan), reached it no earlier than it was made and no later than now.
int MarksNotHeld(const MapRun& r, const command::SituationMap& m, int post) {
    int bad = 0;
    EachMark(m, [&](const command::SituationMark& k) {
        if (k.key.empty()) {
            bad += k.source == "plan" ? 0 : 1;
            return;
        }
        const int fact = r.net.FactIndex(k.key);
        const command::Delivery* d = r.net.Known(post, fact);
        if (d == nullptr || d->h > m.now_h + 1e-9 || std::fabs(d->h - k.got_h) > 1e-9 || k.h > k.got_h + 1e-9 || std::fabs(k.age_h - (m.now_h - k.h)) > 1e-9) ++bad;
    });
    return bad;
}

std::size_t Marks(const command::SituationMap& m) {
    std::size_t n = 0;
    EachMark(m, [&](const command::SituationMark&) { ++n; });
    return n;
}

// One shared run: NATO on the nation, seed 9, 72 h (the army's chief takes western_crossings to defend on day 2, F4.1).
MapRun* Shared() {
    static std::unique_ptr<MapRun> r;
    static bool tried = false;
    if (!tried) {
        tried = true;
        r = std::make_unique<MapRun>();
        if (!r->Go("nato", 9, 72.0)) r.reset();
    }
    return r.get();
}

}  // namespace

SC_TEST("situation map (F5.1): two posts, two maps, each mark resting on a fact its post holds") {
    MapRun* r = Shared();
    SC_EXPECT(r != nullptr && r->ok && r->feed.Ground().On() && r->feed.Ad().On());
    if (r == nullptr || !r->ok) return;
    const command::SituationMap army = r->MapOf("army_ops"), sector = r->MapOf("sector_cp_duty"), regiment = r->MapOf("rgt_chief_of_staff"),
                                battery = r->MapOf("bty21_commander");
    SC_EXPECT(army.post == "army_ops" && battery.post == "bty21_commander" && army.now_h == r->net.Now());
    // The army's operations section holds the units' own reports: our units, the axes in contact and the line they give.
    SC_EXPECT(army.ours.size() >= 3u && !army.axes.empty() && !army.line.empty());
    for (const command::MapUnit& u : army.ours) SC_EXPECT(!u.by_summary && !u.name.empty() && u.fraction >= 0.0 && u.fraction <= 1.5);
    // The national sector's post holds only the army's summaries: the same axes, but by the summary, and the army's marks are newer or as new.
    SC_EXPECT(!sector.axes.empty());
    for (const command::MapAxis& a : sector.axes) SC_EXPECT(a.by_summary && a.mark.source == "army_ops");
    for (const command::MapUnit& u : sector.ours) SC_EXPECT(u.by_summary);
    // The battery commander holds the 7th Division's reports, the regiment's division, and no other unit's.
    std::set<std::string> army_units, battery_units;
    for (const command::MapUnit& u : army.ours) army_units.insert(u.unit);
    for (const command::MapUnit& u : battery.ours) {
        battery_units.insert(u.unit);
        SC_EXPECT(!u.by_summary && r->feed.Ground().WithRegiment(u.formation));
    }
    SC_EXPECT(!battery_units.empty() && battery_units.size() < army_units.size());
    int on_army_only = 0;
    for (const std::string& u : army_units) on_army_only += battery_units.count(u) == 0 ? 1 : 0;
    SC_EXPECT(on_army_only >= 1);
    // A report that reached the army but not the regiment is on the army's map only (by the key of the report it rests on).
    const command::SituationReport* only_army = nullptr;
    for (const command::SituationReport& s : r->feed.Ground().Reports()) {
        if (s.own && r->net.Known(r->Post("army_ops"), s.fact) != nullptr && r->net.Known(r->Post("bty21_commander"), s.fact) == nullptr) only_army = &s;
    }
    SC_EXPECT(only_army != nullptr);
    if (only_army != nullptr) {
        for (const command::MapUnit& u : battery.ours) SC_EXPECT(u.mark.key != only_army->key);
    }
    // Four posts, four maps; each mark holds.
    std::set<std::string> dumps;
    for (const command::SituationMap* m : {&army, &sector, &regiment, &battery}) dumps.insert(command::SituationMapJson(*m));
    SC_EXPECT(dumps.size() == 4u);
    SC_EXPECT(MarksNotHeld(*r, army, r->Post("army_ops")) == 0 && MarksNotHeld(*r, sector, r->Post("sector_cp_duty")) == 0 &&
              MarksNotHeld(*r, regiment, r->Post("rgt_chief_of_staff")) == 0 && MarksNotHeld(*r, battery, r->Post("bty21_commander")) == 0);
    // Sector priorities: the army's chief's and the sector commander's, held by the posts they are for; the sharing follows the priority;
    // an order that reached the regiment's commander gives its hour, the plan's mark none.
    const command::SituationMap commander = r->MapOf("rgt_commander");
    SC_EXPECT(!commander.priorities.empty());
    for (const command::MapPriority& p : commander.priorities) {
        SC_EXPECT(p.priority == "economy" || p.priority == "defend" || p.priority == "offence");
        SC_EXPECT(r->layout.sky.present ? !p.sharing.empty() : p.sharing.empty());
        SC_EXPECT(p.mark.source == "plan" ? p.mark.h < 0.0 : p.mark.h >= 0.0);
    }
    SC_EXPECT(MarksNotHeld(*r, commander, r->Post("rgt_commander")) == 0);
    SC_EXPECT(battery.priorities.empty());  // the battery hears of it by the regiment's tasking, not by the commanders' orders
    // The enemy not in contact, by the army's intelligence summary, where it reached.
    SC_EXPECT(r->feed.Ground().IntelSummaries() == 0 || !army.enemy.empty());
    // Our SAM units: the sector's commander holds his units' plan; the battery commander none.
    SC_EXPECT(!r->layout.units.empty());
    std::size_t sector_sams = sector.sams.size();
    SC_EXPECT(sector_sams > 0u && battery.sams.empty());
    for (const command::MapSam& s : sector.sams) SC_EXPECT(s.basis != "plan" || s.mark.source == "plan");
    // The moves the commanders ordered: a post that holds an order shows the unit where it was sent, by that order's fact.
    for (const cmp::AdStrategy::MoveOrder& o : r->feed.Ad().MoveOrders()) {
        const command::SituationMap unit_post = r->MapOf(r->net.Data().positions[static_cast<std::size_t>(o.post)].id);
        const command::MapSam* s = nullptr;
        for (const command::MapSam& x : unit_post.sams) s = x.id == o.unit ? &x : s;
        if (o.fact >= 0 && r->net.Known(o.post, o.fact) != nullptr) SC_EXPECT(s != nullptr && s->basis != "plan");
        SC_EXPECT(MarksNotHeld(*r, unit_post, o.post) == 0);
    }
    std::printf("    marks: army %zu, sector %zu, regiment %zu, battery %zu; the army's line %zu points, %zu moves ordered\n", Marks(army), Marks(sector),
                Marks(regiment), Marks(battery), army.line.size(), r->feed.Ad().MoveOrders().size());
}

SC_TEST("situation map (F5.1): stale stays stale while the truth moves, a newer report replaces it") {
    auto r = std::make_unique<MapRun>();
    SC_EXPECT(r->Go("warsaw_pact", 7, 36.0));  // the Pact's axes advance by the hour: the truth moves between a unit's returns
    if (!r->ok || r->feed.Ground().Reports().empty()) return;
    r->layout.stale_h = 6.0;
    const int sector_post = r->Post("sector_cp_duty");
    SC_EXPECT(sector_post >= 0);
    std::map<int, command::MapAxis> last;     // by axis: the previous hour's mark
    std::map<int, double> truth;              // the truth's depth then
    int unchanged = 0, truth_moved_under_stale = 0, replaced = 0, stale_seen = 0, flag_wrong = 0;
    for (double h = 37.0; h <= 84.0; h += 1.0) {
        r->To(h);
        const command::SituationMap m = r->MapOf("sector_cp_duty");
        for (const command::MapAxis& a : m.axes) {
            if (a.mark.stale != (a.mark.age_h > 6.0)) ++flag_wrong;
            stale_seen += a.mark.stale ? 1 : 0;
            const double now_truth = r->campaign.Ground()->Axes()[static_cast<std::size_t>(a.index)].depth_m;
            const auto it = last.find(a.index);
            if (it != last.end()) {
                const command::MapAxis& was = it->second;
                if (was.mark.key == a.mark.key) {
                    ++unchanged;
                    // the same report: the same depth and hour, only older; never carried on at its pace
                    SC_EXPECT(a.depth_m == was.depth_m && a.mark.h == was.mark.h && std::fabs(a.mark.age_h - was.mark.age_h - 1.0) < 1e-6);
                    truth_moved_under_stale += std::fabs(now_truth - truth[a.index]) > 1.0 ? 1 : 0;
                } else {
                    ++replaced;
                    SC_EXPECT(a.mark.h > was.mark.h);  // a later report only
                }
            }
            last[a.index] = a;
            truth[a.index] = now_truth;
        }
        // the believed line is the reported depths in their shapes: no point deeper than the deepest reported
        double deepest = 0.0;
        for (const command::MapAxis& a : m.axes) deepest = std::max(deepest, a.depth_m);
        for (const command::MapLinePoint& p : m.line) SC_EXPECT(p.depth_m >= 0.0 && p.depth_m <= deepest + 1e-6);
    }
    SC_EXPECT(unchanged >= 5 && replaced >= 1 && stale_seen >= 1 && flag_wrong == 0);
    SC_EXPECT(truth_moved_under_stale >= 1);
    std::printf("    the sector's post, hourly: %d marks unchanged an hour on (the truth moved under %d of them), %d replaced by a later report, %d stale hours\n",
                unchanged, truth_moved_under_stale, replaced, stale_seen);
}

SC_TEST("situation map (F5.1): the view holds nothing its post's facts do not; the truth moving changes nothing; the cache builds once") {
    auto run = std::make_unique<MapRun>();  // the Pact's war: its axes advance by the hour
    SC_EXPECT(run->Go("warsaw_pact", 7, 60.0));
    MapRun* r = run.get();
    if (!r->ok) return;
    const int post = r->Post("bty21_commander");
    command::SituationMaps maps;
    const command::SituationMap& first = maps.For(r->net, r->feed, r->layout, post);
    SC_EXPECT(maps.Builds() == 1);
    for (int i = 0; i < 100; ++i) maps.For(r->net, r->feed, r->layout, post);
    SC_EXPECT(maps.Builds() == 1 && &maps.For(r->net, r->feed, r->layout, post) == &first);
    maps.For(r->net, r->feed, r->layout, r->Post("army_ops"));
    SC_EXPECT(maps.Builds() == 2);
    // The view: PostViewJson with the map, read back.
    const std::string json = command::PostViewJson(r->net, post, first);
    command::PostView view;
    SC_EXPECT(command::ReadPostView(json, view) && view.has_situation);
    SC_EXPECT(command::SituationMapJson(view.situation) == command::SituationMapJson(first));
    std::set<std::string> keys;
    for (const command::ViewItem& it : view.items) keys.insert(it.key);
    int not_in_items = 0;
    EachMark(view.situation, [&](const command::SituationMark& k) { not_in_items += k.key.empty() || keys.count(k.key) != 0 ? 0 : 1; });
    SC_EXPECT(not_in_items == 0);
    // Without the map the view is as before (the key is not there).
    command::PostView bare;
    SC_EXPECT(command::ReadPostView(command::PostViewJson(r->net, post), bare) && !bare.has_situation);
    // A client merges the newer map.
    command::MergePostView(bare, view);
    SC_EXPECT(bare.has_situation && command::SituationMapJson(bare.situation) == command::SituationMapJson(first));
    // The truth moves on (the campaign runs with the feed not pumped, so the net holds what it held): the map is the same.
    // The cache keeps the post's map by the net's revision, a run of the campaign without the net's change leaves it.
    r->campaign.SetOrderSource(nullptr);
    double depth_sum_before = 0.0, depth_sum_after = 0.0;
    for (const groundwar::Axis& a : r->campaign.Ground()->Axes()) depth_sum_before += a.depth_m;
    const std::string before = command::SituationMapJson(r->MapOf("army_ops"));
    const double stop = r->campaign.Now() + 6.0;
    while (r->campaign.Now() < stop) {
        if (r->campaign.Advance(stop)) r->campaign.SkipPlayerMission();
    }
    for (const groundwar::Axis& a : r->campaign.Ground()->Axes()) depth_sum_after += a.depth_m;
    SC_EXPECT(std::fabs(depth_sum_after - depth_sum_before) > 1.0);  // the truth moved
    SC_EXPECT(command::SituationMapJson(r->MapOf("army_ops")) == before);
    // A newer fact into the net rebuilds the cached map.
    const std::size_t builds = static_cast<std::size_t>(maps.Builds());
    command::Fact f;
    f.key = "test:news";
    f.kind = command::FactKind::Press;
    f.h = r->net.Now();
    f.origin = "army_ops";
    f.text = "test";
    r->net.AddFact(f);
    maps.For(r->net, r->feed, r->layout, post);
    SC_EXPECT(static_cast<std::size_t>(maps.Builds()) == builds + 1);
}

SC_TEST("situation map (F5.1): nothing without the fronts, a post unknown, an empty post") {
    auto r = std::make_unique<MapRun>();
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_nato.json", r->data, &e));
    SC_EXPECT(r->campaign.Init(r->data, 3, &e));  // the fronts' modes as the data has them: 0
    r->net.Init(LoadChain(), 3, r->data.theatre.start_hour);
    r->feed.Bind(&r->net);
    while (r->campaign.Now() < 24.0) {
        if (r->campaign.Advance(24.0)) r->campaign.SkipPlayerMission();
        r->feed.Pump(r->campaign);
    }
    r->layout = command::SituationLayoutOf(r->campaign, &r->feed.Ad());
    SC_EXPECT(r->layout.axes.empty() && r->layout.units.empty());
    const command::SituationMap m = r->MapOf("army_ops");
    SC_EXPECT(m.Empty() && m.sams.empty() && m.line.empty());
    command::SituationMap none;
    command::BuildSituationMap(r->net, r->feed, r->layout, -1, none);
    SC_EXPECT(none.post.empty() && none.Empty());
    SC_EXPECT(command::SituationMapText(m).find("nothing has reached the post") != std::string::npos);
}

SC_TEST("situation map (F5.1): the battery's session debrief ends with its commander's and the regiment's maps") {
    cmp::MissionSessionDef md;
    md.scenario = kData + "/scenarios/pu12m_mission_cover.json";
    md.faction = "nato";
    md.seed = 1;
    md.day = 3;
    md.theatre = "air_war/theatre_pannonia_nation.json";
    md.tasking = "campaign/tasking.json";
    md.hours = 0.2;
    md.fronts = true;
    cmp::MissionSession s;
    std::string e;
    SC_EXPECT(s.Start(md, kData, &e));
    if (s.Feed() == nullptr) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    ScenarioRunner& r = s.Runner();
    while (!r.Finished()) s.Step(0.1);
    const std::string debrief = s.Debrief();
    SC_EXPECT(debrief.find("Situation map of bty21_commander") != std::string::npos);
    SC_EXPECT(debrief.find("Situation map of rgt_chief_of_staff") != std::string::npos);
    const std::size_t at = debrief.find("Situation map of bty21_commander");
    if (at != std::string::npos) std::printf("%s", debrief.substr(at, 900).c_str());
}
