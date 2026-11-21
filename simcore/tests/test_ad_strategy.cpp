// Track F, F4.1: our air defence commanders' sector priorities (simcore/campaign/ad_strategy.hpp,
// docs/fronts/04_AIR_DEFENCE_STRATEGY.md section 2). The national sector's commander and the army's chief of air defence
// keep a priority per sector (defend, the offence, economy) and change it on their rhythm, on what reached their posts
// through our pipeline only, at most one sector a decision; each change is an order through the pipeline to the posts
// below, the SAM regiment's commander among them. At ad_strategy.mode 0 nothing is built or added.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/campaign/ad_strategy.hpp"
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

cmp::AdStrategyData LoadStrategy() {
    cmp::AdStrategyData d;
    std::string e;
    if (!cmp::LoadAdStrategyData(kData, "campaign/ad_strategy.json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

// A faction's war on the nation with the ground war and the air front on and our air defence commanders at `mode`, the
// feed pumped after every step (or once at the end, as after a load). `reports` false keeps our units' reports out of the
// pipeline (the ground feed's posts named none). `rules` replaces the data's (nullptr: the data's).
struct AdRun {
    AirWarData data;
    AirCampaign campaign;
    command::InfoNet net;
    command::CampaignFeed feed;
    bool ok = false;

    bool Go(const std::string& faction, unsigned seed, double until_h, double mode, bool once, bool reports, const cmp::AdStrategyData* rules,
            const std::string& theatre = "air_war/theatre_pannonia_nation.json", const std::map<std::string, double>& tune = {}) {
        std::string e;
        if (!LoadAirWarData(kData, theatre, "air_war/faction_" + faction + ".json", data, &e)) {
            std::printf("    %s\n", e.c_str());
            return false;
        }
        data.tuning.values["ground.mode"] = 1.0;
        data.tuning.values["air_front.mode"] = 1.0;
        data.tuning.values["ad_strategy.mode"] = mode;
        for (const auto& kv : tune) data.tuning.values[kv.first] = kv.second;
        if (!campaign.Init(data, seed, &e)) {
            std::printf("    %s\n", e.c_str());
            return false;
        }
        net.Init(LoadChain(), seed, data.theatre.start_hour);
        feed.Bind(&net);
        if (!reports) {
            command::GroundFeedRoles off;
            off.army_staff = "off";
            feed.UseGroundRoles(off);
        }
        if (rules != nullptr) feed.UseAdStrategy(*rules);
        while (campaign.Now() < until_h) {
            if (campaign.Advance(until_h)) campaign.SkipPlayerMission();
            if (!once) feed.Pump(campaign);
        }
        if (once) feed.Pump(campaign);
        ok = true;
        return true;
    }
    int Post(const std::string& id) const { return net.Data().PositionIndex(id); }
};

std::uint64_t Fnv(const AirCampaign& c) {
    std::uint64_t h = 1469598103934665603ull;
    for (const JournalEntry& j : c.Journal()) {
        for (char ch : j.text) {
            h ^= static_cast<unsigned char>(ch);
            h *= 1099511628211ull;
        }
    }
    return h;
}

cmp::AdPicture Sectors(std::size_t n, double h) {
    cmp::AdPicture p;
    p.h = h;
    p.sectors.assign(n, cmp::AdSectorPicture());
    for (cmp::AdSectorPicture& s : p.sectors) s.fire_units = 4;
    return p;
}

airwar::Traits Plain() { return airwar::Traits(); }  // every trait 0.5

}  // namespace

SC_TEST("ad strategy (F4.1): the commanders' data loads with every value sourced, and the switch is off in the game's data") {
    const cmp::AdStrategyData d = LoadStrategy();
    SC_EXPECT(d.commanders.size() == 2u);
    if (d.commanders.size() != 2u) return;
    const command::ChainData chain = LoadChain();
    for (const cmp::AdCommanderDef& c : d.commanders) {
        SC_EXPECT(chain.PositionIndex(c.post) >= 0);
        for (const std::string& to : c.to) SC_EXPECT(chain.PositionIndex(to) >= 0);
        SC_EXPECT(c.rhythm_h.lo > 0.0 && c.rhythm_h.hi >= c.rhythm_h.lo);
    }
    SC_EXPECT(d.commanders[0].role == cmp::AdRole::National && d.commanders[0].post == "sector_cp_duty");
    SC_EXPECT(d.commanders[0].rhythm_h.lo == 6.0 && d.commanders[0].rhythm_h.hi == 12.0);
    SC_EXPECT(d.commanders[1].role == cmp::AdRole::Troops && d.commanders[1].post == "army_ad_chief");
    SC_EXPECT(d.commanders[1].rhythm_h.lo == 2.0 && d.commanders[1].rhythm_h.hi == 4.0);
    bool regiment = false;
    for (const std::string& to : d.commanders[1].to) regiment = regiment || to == "rgt_commander";
    SC_EXPECT(regiment);  // the army's chief's orders are for the SAM regiment (F4.4 tasks its battery on them)
    SC_EXPECT_NEAR(d.rules.main_share, 0.6, 1e-12);  // andersen_1979 p. 40
    SC_EXPECT(d.rules.National(ObjectKind::Airfield) && !d.rules.National(ObjectKind::Troops) && !d.rules.National(ObjectKind::SamSite));
    AirWarData w;
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_warsaw_pact.json", w, &e));
    SC_EXPECT_NEAR(w.tuning.Get("ad_strategy.mode"), 0.0, 1e-12);
}

SC_TEST("ad strategy (F4.1): the rule table by hand; a bold man takes the offence on less; the shares by priority") {
    const cmp::AdRules r = LoadStrategy().rules;
    cmp::AdPicture p = Sectors(6, 20.0);
    p.sectors[0].enemy = 2.0;  // the main thrust
    p.sectors[0].thrust_m = 12000.0;
    p.sectors[1].enemy = 1.0;  // a lesser thrust, with an object in reach
    p.sectors[1].objects_in_reach = 1;
    p.sectors[2].counterattack = true;
    p.sectors[3].air_front_m = 15000.0;  // the staff's estimate, fresh, over thin cover
    p.sectors[3].air_front_h = 10.0;
    p.sectors[3].fire_units = 1;
    p.sectors[4].air_front_m = 15000.0;  // the same over thick cover
    p.sectors[4].air_front_h = 10.0;
    const cmp::AdRuleResult plain = cmp::RulePriorities(p, r, Plain());
    SC_EXPECT(plain.priority[0] == cmp::SectorPriority::Defend);
    SC_EXPECT(plain.priority[1] == cmp::SectorPriority::Defend);
    SC_EXPECT(plain.priority[2] == cmp::SectorPriority::Defend);
    SC_EXPECT(plain.priority[3] == cmp::SectorPriority::Offence);
    SC_EXPECT(plain.priority[4] == cmp::SectorPriority::Economy);
    SC_EXPECT(plain.priority[5] == cmp::SectorPriority::Economy);
    SC_EXPECT(plain.why[0].find("main thrust") != std::string::npos && plain.why[5].find("quiet") != std::string::npos);
    // The offence's threshold: the range's middle for a plain man (15 km), lower for a bold one, higher for a timid one.
    airwar::Traits bold = Plain(), timid = Plain();
    bold.boldness = 1.0;
    timid.boldness = 0.0;
    SC_EXPECT_NEAR(cmp::OffenceDepthOf(r, Plain()), 15000.0, 1e-9);
    SC_EXPECT_NEAR(cmp::OffenceDepthOf(r, bold), 13000.0, 1e-9);
    SC_EXPECT_NEAR(cmp::OffenceDepthOf(r, timid), 17000.0, 1e-9);
    SC_EXPECT(cmp::RulePriorities(p, r, timid).priority[3] == cmp::SectorPriority::Economy);
    p.sectors[3].air_front_m = 14000.0;
    SC_EXPECT(cmp::RulePriorities(p, r, Plain()).priority[3] == cmp::SectorPriority::Economy);
    SC_EXPECT(cmp::RulePriorities(p, r, bold).priority[3] == cmp::SectorPriority::Offence);
    p.h = 10.0 + r.offence_fresh_h + 1.0;  // an estimate older than the staff's window no longer moves him
    SC_EXPECT(cmp::RulePriorities(p, r, bold).priority[3] == cmp::SectorPriority::Economy);
    // On a tie of the enemy reported, the deeper thrust is the main one.
    cmp::AdPicture tie = Sectors(2, 0.0);
    tie.sectors[0].enemy = tie.sectors[1].enemy = 1.5;
    tie.sectors[1].thrust_m = 5000.0;
    SC_EXPECT(cmp::MainThrustOf(tie) == 1);
    SC_EXPECT(cmp::MainThrustOf(Sectors(3, 0.0)) == -1);
    // The rhythm: a methodical man's cycle is longer, inside his range.
    cmp::AdCommanderDef c;
    c.rhythm_h = {2.0, 4.0};
    c.traits.methodical = 1.0;
    SC_EXPECT_NEAR(cmp::CycleHoursOf(c, r), 3.4, 1e-9);
    c.traits.methodical = 0.0;
    SC_EXPECT_NEAR(cmp::CycleHoursOf(c, r), 2.6, 1e-9);
    // The shares: the defended sectors main_share together, the rest by the weights; they sum to 1.
    using P = cmp::SectorPriority;
    const std::vector<double> s = cmp::PriorityShares({P::Defend, P::Economy, P::Offence, P::Economy}, r);
    SC_EXPECT_NEAR(s[0], 0.6, 1e-12);
    SC_EXPECT_NEAR(s[2], 0.4 * 2.0 / 4.0, 1e-12);
    SC_EXPECT_NEAR(s[1], 0.1, 1e-12);
    SC_EXPECT_NEAR(s[0] + s[1] + s[2] + s[3], 1.0, 1e-12);
    const std::vector<double> all = cmp::PriorityShares({P::Defend, P::Defend}, r);
    SC_EXPECT_NEAR(all[0], 0.5, 1e-12);
    const std::vector<double> none = cmp::PriorityShares({P::Economy, P::Offence}, r);
    SC_EXPECT_NEAR(none[0] + none[1], 1.0, 1e-12);
    SC_EXPECT_NEAR(none[1], 2.0 / 3.0, 1e-12);
}

SC_TEST("ad strategy (F4.1): one change a decision; a methodical man waits for a second report, a rigid one holds the pre-war plan longer") {
    const cmp::AdRules r = LoadStrategy().rules;
    using P = cmp::SectorPriority;
    cmp::AdPicture p = Sectors(3, 0.0);
    p.sectors[0].enemy = 2.0;           // the main thrust: defend
    p.sectors[1].counterattack = true;  // our counterattack: defend
    const std::vector<P> plan{P::Economy, P::Economy, P::Economy};
    // A plain man: the main thrust first, then the counterattack, one a decision; then nothing more to change.
    cmp::AdCommanderState s;
    s.Init(plan);
    cmp::AdChange a = cmp::DecideChange(p, r, Plain(), s);
    SC_EXPECT(a.sector == 0 && a.from == P::Economy && a.to == P::Defend);
    SC_EXPECT(s.now[0] == P::Defend && s.now[1] == P::Economy);
    a = cmp::DecideChange(p, r, Plain(), s);
    SC_EXPECT(a.sector == 1 && a.to == P::Defend);
    SC_EXPECT(cmp::DecideChange(p, r, Plain(), s).sector == -1);
    // A methodical man waits for a second report before each.
    airwar::Traits methodical = Plain();
    methodical.methodical = 0.8;
    s.Init(plan);
    SC_EXPECT(cmp::DecideChange(p, r, methodical, s).sector == -1);
    SC_EXPECT(cmp::DecideChange(p, r, methodical, s).sector == 0);
    SC_EXPECT(cmp::DecideChange(p, r, methodical, s).sector == 1);  // its second ask came with the first change's
    // A different ask in between starts his count again.
    s.Init(plan);
    cmp::AdPicture q = Sectors(3, 0.0);
    q.sectors[0].air_front_m = 20000.0;
    q.sectors[0].air_front_h = 0.0;
    q.sectors[0].fire_units = 0;  // the offence asked in sector 0
    SC_EXPECT(cmp::DecideChange(q, r, methodical, s).sector == -1);
    SC_EXPECT(cmp::DecideChange(p, r, methodical, s).sector == -1);  // now the defence: a first ask
    SC_EXPECT(cmp::DecideChange(p, r, methodical, s).sector == 0);
    // A rigid man holds the pre-war plan one decision longer; once off it, he changes as a plain man.
    airwar::Traits rigid = Plain();
    rigid.rigidity = 0.8;
    s.Init(plan);
    SC_EXPECT(cmp::DecideChange(p, r, rigid, s).sector == -1);
    SC_EXPECT(cmp::DecideChange(p, r, rigid, s).sector == 0);
    SC_EXPECT(cmp::DecideChange(p, r, rigid, s).sector == 1);
    cmp::AdPicture quiet = Sectors(3, 0.0);
    a = cmp::DecideChange(quiet, r, rigid, s);  // the thrust gone from what he holds: back to economy at once (not his plan he leaves)
    SC_EXPECT(a.sector == 0 && a.to == P::Economy);
}

SC_TEST("ad strategy (F4.1): on the nation, our commanders decide on what reached their posts, one change a decision, and the orders reach the regiment") {
    // The data's commanders, with no national object counted within the enemy's reach so the pre-war plan is economy
    // everywhere and the main thrust's report is what moves the army's chief.
    cmp::AdStrategyData rules = LoadStrategy();
    rules.rules.object_reach_m = -1e9;
    auto r = std::make_unique<AdRun>();
    SC_EXPECT(r->Go("warsaw_pact", 7, 36.0, 1.0, false, true, &rules));
    if (!r->ok) return;
    const cmp::AdStrategy& ad = r->feed.Ad();
    SC_EXPECT(ad.On());
    if (!ad.On()) {
        std::printf("    %s\n", ad.Error().c_str());
        return;
    }
    const double start = r->data.theatre.start_hour;
    for (std::size_t k = 0; k < ad.Commanders(); ++k) {
        for (cmp::SectorPriority x : ad.PreWar(static_cast<int>(k))) SC_EXPECT(x == cmp::SectorPriority::Economy);
    }
    SC_EXPECT(!ad.Decisions().empty());
    const int army = ad.CommanderIndex("army_ad_chief");
    const int rgt = r->Post("rgt_commander"), bty = r->Post("bty21_commander");
    std::map<std::pair<int, long long>, int> per_hour;
    int to_regiment = 0;
    for (const cmp::AdDecision& d : ad.Decisions()) {
        const cmp::AdCommanderDef& def = ad.Def(d.commander);
        const int post = r->Post(def.post);
        std::printf("    %s %-15s sector %d %s -> %s on %zu facts: %s\n", command::FormatClock(d.h, start).c_str(), def.id.c_str(), d.sector,
                    cmp::SectorPriorityName(d.from), cmp::SectorPriorityName(d.to), d.basis.size(), d.why.c_str());
        // On his cycle: a decision hour of his.
        const double cycles = d.h / ad.CycleH(d.commander);
        SC_EXPECT(std::fabs(cycles - std::round(cycles)) < 1e-6);
        // One change a decision.
        const std::pair<int, long long> hour_key(d.commander, std::llround(d.h * 1000.0));
        SC_EXPECT(++per_hour[hour_key] == 1);
        // On reports only: every fact it rested on had reached his post by the decision's hour, when it says.
        SC_EXPECT(!d.basis.empty());
        for (std::size_t i = 0; i < d.basis.size(); ++i) {
            const command::Delivery* got = r->net.Known(post, d.basis[i]);
            SC_EXPECT(got != nullptr && got->h <= d.h + 1e-9 && std::fabs(got->h - d.basis_h[i]) < 1e-9);
        }
        // The order: a fact from his post after his work on it, for the posts the data names, carried by the pipeline.
        const command::Fact& f = r->net.Facts()[static_cast<std::size_t>(d.fact)];
        SC_EXPECT(f.kind == command::FactKind::Order && f.origin == def.post && f.to == def.to);
        SC_EXPECT_NEAR(f.h, d.h + def.order_work_h, 1e-9);
        if (d.commander != army) continue;
        const command::Delivery* at = r->net.Known(rgt, d.fact);
        if (at == nullptr) continue;
        ++to_regiment;
        SC_EXPECT(at->h > f.h && at->from >= 0);
        SC_EXPECT(r->net.Known(bty, d.fact) == nullptr);  // the battery hears of it only by the regiment's tasking (F4.4)
        double when = 0.0;
        const cmp::SectorPriority known = ad.PriorityKnownBy(army, rgt, d.sector, &when);
        SC_EXPECT(when >= at->h - 1e-9);
        (void)known;
    }
    SC_EXPECT(to_regiment >= 1);
    // What the regiment holds of the army chief's priorities is his latest order on each sector that reached it.
    for (std::size_t s = 0; s < ad.Priorities(army).size(); ++s) {
        double when = 0.0;
        const cmp::SectorPriority known = ad.PriorityKnownBy(army, rgt, static_cast<int>(s), &when);
        cmp::SectorPriority latest = ad.PreWar(army)[s];
        for (const cmp::AdDecision& d : ad.Decisions()) {
            if (d.commander == army && d.sector == static_cast<int>(s) && r->net.Known(rgt, d.fact) != nullptr) latest = d.to;
        }
        SC_EXPECT(known == latest);
    }
    // Pumped once after a load: the same decisions, the same orders at the same hours.
    auto once = std::make_unique<AdRun>();
    SC_EXPECT(once->Go("warsaw_pact", 7, 36.0, 1.0, true, true, &rules));
    if (!once->ok) return;
    const std::vector<cmp::AdDecision>& a = ad.Decisions();
    const std::vector<cmp::AdDecision>& b = once->feed.Ad().Decisions();
    SC_EXPECT(a.size() == b.size());
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
        SC_EXPECT(a[i].commander == b[i].commander && a[i].h == b[i].h && a[i].sector == b[i].sector && a[i].to == b[i].to && a[i].key == b[i].key);
        const command::Delivery* x = r->net.Known(rgt, a[i].fact);
        const command::Delivery* y = once->net.Known(rgt, b[i].fact);
        SC_EXPECT((x == nullptr) == (y == nullptr));
        if (x != nullptr && y != nullptr) SC_EXPECT(x->h == y->h);
    }
    for (std::size_t k = 0; k < ad.Commanders(); ++k) SC_EXPECT(ad.Reviews(static_cast<int>(k)) == once->feed.Ad().Reviews(static_cast<int>(k)));
}

SC_TEST("ad strategy (F4.1): a thrust not reported moves nothing; at mode 0 nothing is added; the air war is the same either way; a save keeps the mode") {
    cmp::AdStrategyData rules = LoadStrategy();
    rules.rules.object_reach_m = -1e9;
    // The same war with our units' reports kept out of the pipeline: the ground war moves, our commanders hear nothing of it.
    auto blind = std::make_unique<AdRun>();
    SC_EXPECT(blind->Go("warsaw_pact", 7, 36.0, 1.0, false, false, &rules));
    if (!blind->ok) return;
    const cmp::AdStrategy& ad = blind->feed.Ad();
    SC_EXPECT(ad.On());
    SC_EXPECT(ad.Decisions().empty());
    bool moved = false;
    if (blind->campaign.Ground() != nullptr) {
        for (const groundwar::Axis& x : blind->campaign.Ground()->Axes()) moved = moved || x.depth_m > 0.0;
    }
    SC_EXPECT(moved);  // the truth moved; nothing reached them
    // Mode 0: nothing built, no order in the pipeline; and the war itself is the same as with the commanders on (they move
    // nothing yet).
    auto zero = std::make_unique<AdRun>();
    SC_EXPECT(zero->Go("warsaw_pact", 7, 36.0, 0.0, false, true, &rules));
    if (!zero->ok) return;
    SC_EXPECT(!zero->feed.Ad().On() && zero->feed.Ad().Decisions().empty());
    for (const command::Fact& f : zero->net.Facts()) SC_EXPECT(f.key.compare(0, 9, "ad_order:") != 0);
    SC_EXPECT(Fnv(zero->campaign) == Fnv(blind->campaign) && zero->campaign.Journal().size() == blind->campaign.Journal().size());
    // The save keeps the mode; a save without it replays at 0.
    std::string saved = SaveCampaignJson(blind->campaign, "air_war/theatre_pannonia_nation.json", "air_war/faction_warsaw_pact.json");
    SC_EXPECT(saved.find("\"ad_strategy_mode\"") != std::string::npos);
    AirCampaign back;
    std::string e;
    SC_EXPECT(LoadCampaignJson(saved, kData, back, &e));
    SC_EXPECT_NEAR(back.Data().tuning.Get("ad_strategy.mode"), 1.0, 1e-12);
    const std::size_t at = saved.find("\"ad_strategy_mode\"");
    const std::size_t end = saved.find(',', at);
    if (at != std::string::npos && end != std::string::npos) saved.erase(at, end - at + 1);
    AirCampaign old;
    SC_EXPECT(LoadCampaignJson(saved, kData, old, &e));
    SC_EXPECT_NEAR(old.Data().tuning.Get("ad_strategy.mode"), 0.0, 1e-12);
}

SC_TEST("ad strategy (F4.1): the air defence staff's estimate of the enemy's air front, once it reached the army's chief, takes him to the offence where cover is thin") {
    // The district (no ground war) with F2.5's credulous staff: ordinary raids taken for signs, so it states an estimate in
    // a few days; every sector's cover counted thin, no object in reach, so the estimate is all that can move the chief.
    cmp::AdStrategyData rules = LoadStrategy();
    rules.rules.object_reach_m = -1e9;
    rules.rules.thin_cover_units = 1000;
    const std::map<std::string, double> credulous = {{"air_front.watch_cas_reach_m", 0.0}, {"air_front.watch_helo_reach_m", 0.0}, {"air_front.watch_count", 1.0}};
    auto r = std::make_unique<AdRun>();
    SC_EXPECT(r->Go("warsaw_pact", 21, 72.0, 1.0, false, true, &rules, "air_war/theatre_pannonia.json", credulous));
    if (!r->ok) return;
    const cmp::AdStrategy& ad = r->feed.Ad();
    SC_EXPECT(ad.On());
    const std::vector<command::AirFrontConclusion>& ks = r->feed.Watch().Conclusions();
    SC_EXPECT(!ks.empty());
    if (ks.empty()) return;
    const int army = ad.CommanderIndex("army_ad_chief");
    const int chief = r->Post("army_ad_chief"), rgt = r->Post("rgt_commander");
    int offence = 0;
    for (const cmp::AdDecision& d : ad.Decisions()) {
        std::printf("    %.1f h %-16s sector %d %s -> %s: %s\n", d.h, ad.Def(d.commander).id.c_str(), d.sector, cmp::SectorPriorityName(d.from),
                    cmp::SectorPriorityName(d.to), d.why.c_str());
        if (d.commander != army || d.to != cmp::SectorPriority::Offence) continue;
        ++offence;
        // It rested on an estimate of that sector that had reached him by then, and only after the staff stated it.
        bool estimate = false;
        for (std::size_t i = 0; i < d.basis.size(); ++i) {
            const command::Fact& f = r->net.Facts()[static_cast<std::size_t>(d.basis[i])];
            if (f.kind != command::FactKind::AirFront) continue;
            estimate = true;
            SC_EXPECT(f.h <= d.basis_h[i] && d.basis_h[i] <= d.h);
            const command::Delivery* got = r->net.Known(chief, d.basis[i]);
            SC_EXPECT(got != nullptr && got->h <= d.h);
        }
        SC_EXPECT(estimate);
        // The regiment holds the order once the pipeline brought it: the offence in that sector, as the chief ordered.
        if (r->net.Known(rgt, d.fact) != nullptr) SC_EXPECT(ad.PriorityKnownBy(army, rgt, d.sector) == cmp::SectorPriority::Offence);
    }
    std::printf("    %zu estimates stated; the chief held the first %s; %d changes to the offence\n", ks.size(),
                r->net.Known(chief, r->net.FactIndex(ks[0].key)) != nullptr ? "by the end" : "never", offence);
    SC_EXPECT(offence >= 1);
}
