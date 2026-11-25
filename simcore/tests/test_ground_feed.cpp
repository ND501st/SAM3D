// Track F, F3.8: our units' situation reports into the information pipeline (simcore/command/ground_feed.hpp,
// docs/fronts/03_GROUND_WAR.md section 10). The ground war logs what our units compiled (their own counts, the picture's
// errors); the feed makes each a fact at the post that compiled it, and the net carries it with the means each hop has:
// the army's operations section holds a report only once it reached it, the army's summary is made from what had by then,
// the regiment and the battery hear of their own division and nobody else's, and the enemy's air strikes on our troops go
// to the air defence staff's watch as the unit counted them. At ground.mode 0 nothing is added.
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/command.hpp"
#include "simcore/ground_war.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;
namespace command = simcore::command;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

command::ChainData LoadChain() {
    command::ChainData d;
    std::string e;
    if (!command::LoadChainData(kData, "command/chain_pannonia.json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

// A faction's war on the nation with the ground war on, the feed pumped after every step (or once at the end, as after a
// load); `tune` changes the game values.
struct FeedRun {
    AirWarData data;
    AirCampaign campaign;
    command::InfoNet net;
    command::CampaignFeed feed;
    bool ok = false;

    bool Go(const std::string& faction, unsigned seed, double until_h, const std::map<std::string, double>& tune, bool once, bool ground_on = true) {
        std::string e;
        if (!LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_" + faction + ".json", data, &e)) {
            std::printf("    %s\n", e.c_str());
            return false;
        }
        if (ground_on) data.tuning.values["ground.mode"] = 1.0;
        for (const auto& kv : tune) data.tuning.values[kv.first] = kv.second;
        if (!campaign.Init(data, seed, &e)) {
            std::printf("    %s\n", e.c_str());
            return false;
        }
        net.Init(LoadChain(), seed, data.theatre.start_hour);
        feed.Bind(&net);
        while (campaign.Now() < until_h) {
            if (campaign.Advance(until_h)) campaign.SkipPlayerMission();
            if (!once) feed.Pump(campaign);
        }
        if (once) feed.Pump(campaign);
        ok = true;
        return true;
    }
    int Post(const std::string& id) const { return net.Data().PositionIndex(id); }
    const command::Delivery* At(const std::string& post, int fact) const { return net.Known(Post(post), fact); }
};

}  // namespace

SC_TEST("ground feed (F3.8): our units' reports reach the army's staff with the pipeline's delays, from the posts that compiled them") {
    auto r = std::make_unique<FeedRun>();
    SC_EXPECT(r->Go("warsaw_pact", 21, 96.0, {}, false));
    if (!r->ok || !r->feed.Ground().On()) return;
    const command::GroundFeed& gf = r->feed.Ground();
    SC_EXPECT(gf.Reports().size() >= 20u);
    int carried = 0, army_level = 0, late = 0;
    double delay_sum = 0.0;
    for (const command::SituationReport& s : gf.Reports()) {
        const command::Fact& f = r->net.Facts()[static_cast<std::size_t>(s.fact)];
        SC_EXPECT(f.kind == command::FactKind::Situation && f.h == s.h && f.origin == s.post && !f.to.empty() && f.to[0] == "army_ops");
        const command::Delivery* d = r->At("army_ops", s.fact);
        if (d == nullptr) {
            late += s.h > 90.0 ? 0 : 1;  // everything compiled before the last hours has arrived by day 4
            continue;
        }
        SC_EXPECT(d->h >= s.h);
        if (s.post == "army_ops") {
            ++army_level;
            SC_EXPECT(d->from == -1 && d->h == s.h);  // the army's own brigades: the staff that compiled it holds it at once
        } else {
            ++carried;
            SC_EXPECT(d->from >= 0 && d->h > s.h);
            delay_sum += d->h - s.h;
        }
    }
    SC_EXPECT(late == 0);
    SC_EXPECT(carried > 0);
    std::printf("    %zu reports in 4 days (%d from the divisions' and brigades' staffs carried to the army, mean delay %.2f h; %d by its own brigades)\n",
                gf.Reports().size(), carried, carried > 0 ? delay_sum / carried : 0.0, army_level);
    // The scheduled reports come at the compile hours, twice a day for each of our formations.
    int scheduled = 0;
    for (const command::SituationReport& s : gf.Reports()) scheduled += s.cause == groundwar::SitCause::Scheduled ? 1 : 0;
    SC_EXPECT(scheduled >= 8 * 4);
}

SC_TEST("ground feed (F3.8): a report is what the unit believed (the picture's own count), and a post holds only what reached it") {
    auto r = std::make_unique<FeedRun>();
    SC_EXPECT(r->Go("warsaw_pact", 21, 96.0, {}, false));
    if (!r->ok || !r->feed.Ground().On()) return;
    const command::GroundFeed& gf = r->feed.Ground();
    const groundwar::GroundWar& g = *r->campaign.Ground();
    // The believed strength in a return is the picture's: the latest scheduled return the staff had by now (compiled two
    // hours before it reached the staff) is the belief the picture holds, to the bit, error and all.
    int compared = 0, erred = 0;
    for (std::size_t i = 0; i < g.Formations().size(); ++i) {
        if (g.Formations()[i].unit.side != groundwar::Side::Ours) continue;
        const groundwar::UnitBelief& b = g.Picture(groundwar::Side::Ours).units[i];
        for (const command::SituationReport& s : gf.Reports()) {
            if (s.formation != static_cast<int>(i) || !s.own || s.h != b.observed_h || s.cause != groundwar::SitCause::Scheduled) continue;
            SC_EXPECT(s.fraction == b.fraction && s.out == b.out && s.sector == b.sector);
            ++compared;
            erred += std::fabs(b.factor - 1.0) > 1e-6 ? 1 : 0;
        }
    }
    SC_EXPECT(compared >= 4 && erred >= 1);
    // Nobody not meant to has it: the sector's command post, the regiment and the battery hold none of the 9th Division's, the
    // brigades' or the army's reports; the battery commander holds the 7th Division's only once someone told him.
    const int sector = r->Post("sector_cp_duty"), reg = r->Post("rgt_chief_of_staff"), bty = r->Post("bty21_commander");
    int division_seen = 0;
    for (const command::SituationReport& s : gf.Reports()) {
        if (!gf.WithRegiment(s.formation)) {
            SC_EXPECT(r->net.Known(sector, s.fact) == nullptr && r->net.Known(reg, s.fact) == nullptr && r->net.Known(bty, s.fact) == nullptr);
            continue;
        }
        SC_EXPECT(r->net.Known(sector, s.fact) == nullptr);
        const command::Delivery* d = r->net.Known(reg, s.fact);
        const command::Delivery* b = r->net.Known(bty, s.fact);
        SC_EXPECT(d == nullptr || (d->h >= s.h && d->from >= 0));
        if (b == nullptr) continue;
        ++division_seen;
        SC_EXPECT(b->h >= s.h && b->from >= 0);  // it was told, by whoever the pipeline's way to him passes it through
    }
    SC_EXPECT(division_seen >= 4);
    // What is not there yet is on its way: a report not held by the army's staff arrives after the net's hour.
    int pending = 0;
    for (const command::SituationReport& s : gf.Reports()) {
        if (r->At("army_ops", s.fact) != nullptr) continue;
        command::Delivery d;
        SC_EXPECT(r->net.PreviewDelivery(s.fact, r->Post("army_ops"), r->net.Now() + 48.0, d) && d.h > r->net.Now());
        ++pending;
    }
    std::printf("    %d returns equal to the picture's belief (%d with an error); %d of the 7th Division's reached the battery commander; %d still on their way\n",
                compared, erred, division_seen, pending);
    // The army's summaries: at 09:00 and 21:00 local, to the sector and the chief of air defence, made only of the latest
    // return of each unit that had reached the army's staff by then.
    SC_EXPECT(gf.Summaries().size() >= 6u);
    const command::GroundFeedRoles& roles = gf.Roles();
    const int chief = r->Post(roles.air_chief);
    for (const command::ArmySummary& s : gf.Summaries()) {
        const double local = std::fmod(r->data.theatre.start_hour + s.h, 24.0);
        SC_EXPECT(std::fabs(local - 9.0) < 1e-6 || std::fabs(local - 21.0) < 1e-6);
        const command::Fact& f = r->net.Facts()[static_cast<std::size_t>(s.fact)];
        SC_EXPECT(f.kind == command::FactKind::Situation && f.origin == "army_ops" && f.h == s.h && f.to.size() == 2u);
        SC_EXPECT(!s.reports.empty());
        std::map<int, double> latest_in;
        for (int ri : s.reports) {
            const command::SituationReport& rep = gf.Reports()[static_cast<std::size_t>(ri)];
            const command::Delivery* d = r->At("army_ops", rep.fact);
            SC_EXPECT(d != nullptr && d->h <= s.h && rep.h <= s.h);
            latest_in[rep.formation] = rep.h;
        }
        for (const command::SituationReport& rep : gf.Reports()) {  // nothing that reached the staff later is in it, and no older one
            if (!rep.own || rep.h > s.h) continue;
            const command::Delivery* d = r->At("army_ops", rep.fact);
            const auto it = latest_in.find(rep.formation);
            if (d != nullptr && d->h <= s.h) {
                SC_EXPECT(it != latest_in.end() && it->second >= rep.h);
            } else {
                SC_EXPECT(it == latest_in.end() || it->second != rep.h);
            }
        }
        const command::Delivery* a = r->net.Known(sector, s.fact);
        const command::Delivery* b = r->net.Known(chief, s.fact);
        if (a != nullptr) SC_EXPECT(a->h >= s.h && a->from >= 0);
        if (b != nullptr) SC_EXPECT(b->h >= s.h);
    }
    // The posts' reading: the army holds the latest return of the 7th Division, the sector's post none of any unit's (only
    // the army's summary), and no return older than what it holds.
    const int army = r->Post("army_ops");
    const command::SituationReport* mine = nullptr;
    for (std::size_t i = 0; i < g.Formations().size(); ++i) {
        if (g.Formations()[i].unit.id == "mrd_7") mine = gf.LatestKnownBy(army, static_cast<int>(i));
    }
    SC_EXPECT(mine != nullptr);
    SC_EXPECT(mine == nullptr || gf.LatestKnownBy(sector, mine->formation) == nullptr);
    double depth_h = 0.0;
    int axes_seen = 0;
    for (std::size_t a = 0; a < g.Axes().size(); ++a) axes_seen += gf.LatestAxisKnownBy(army, static_cast<int>(a), &depth_h) != nullptr ? 1 : 0;
    SC_EXPECT(axes_seen >= 1);
    std::printf("    %zu summaries by the army's operations section; it holds %d axes' depth and pace\n", gf.Summaries().size(), axes_seen);
    // The army's intelligence summary of the enemy it does not touch reaches the army's staff, the chief and the sector.
    int intsums = 0;
    for (const command::Fact& f : r->net.Facts()) {
        if (f.kind != command::FactKind::Intsum || f.key.compare(0, 13, "ground_intsum") != 0) continue;
        ++intsums;
        const int fi = r->net.FactIndex(f.key);
        SC_EXPECT(f.origin == "army_intel" && r->net.Known(sector, fi) != nullptr);
    }
    SC_EXPECT(intsums >= 1 && intsums == gf.IntelSummaries());
    // What a post's view sends its player is what reached it, as the pipeline gave it: the army's operations section has the
    // units' reports, the sector's post only the army's summaries (and intelligence summary), the battery commander the 7th
    // Division's reports and no other unit's.
    const auto view_of = [&](const std::string& post) {
        command::PostView v;
        SC_EXPECT(command::ReadPostView(command::PostViewJson(r->net, r->Post(post)), v));
        return v;
    };
    const command::PostView army_view = view_of("army_ops"), sector_view = view_of("sector_cp_duty"), battery_view = view_of("bty21_commander");
    std::size_t reports_in_view = 0, held_by_army = 0;
    for (const command::SituationReport& s : gf.Reports()) held_by_army += r->At("army_ops", s.fact) != nullptr ? 1 : 0;
    for (const command::ViewItem& it : army_view.items) reports_in_view += it.key.compare(0, 7, "sitrep:") == 0 && it.kind == "situation" ? 1 : 0;
    SC_EXPECT(reports_in_view == held_by_army && held_by_army >= 20u);
    for (const command::ViewItem& it : sector_view.items) SC_EXPECT(it.key.compare(0, 7, "sitrep:") != 0 && it.key.compare(0, 8, "sitorder") != 0);
    bool summary_in_sector_view = false;
    for (const command::ViewItem& it : sector_view.items) summary_in_sector_view = summary_in_sector_view || it.key.compare(0, 11, "sitsummary:") == 0;
    SC_EXPECT(summary_in_sector_view);
    for (const command::ViewItem& it : battery_view.items) {
        if (it.key.compare(0, 7, "sitrep:") != 0) continue;
        bool division = false;
        for (const command::SituationReport& s : gf.Reports()) division = division || (s.key == it.key && gf.WithRegiment(s.formation));
        SC_EXPECT(division);
    }
}

SC_TEST("ground feed (F3.8): pumped once after a load, the same facts reach the same posts at the same hours") {
    auto a = std::make_unique<FeedRun>();
    auto b = std::make_unique<FeedRun>();
    SC_EXPECT(a->Go("warsaw_pact", 21, 72.0, {}, false));
    SC_EXPECT(b->Go("warsaw_pact", 21, 72.0, {}, true));
    if (!a->ok || !b->ok || !a->feed.Ground().On() || !b->feed.Ground().On()) return;
    const command::GroundFeed& x = a->feed.Ground();
    const command::GroundFeed& y = b->feed.Ground();
    SC_EXPECT(x.Reports().size() == y.Reports().size() && x.Summaries().size() == y.Summaries().size() && x.Orders().size() == y.Orders().size());
    // (The wrecks a campaign's objects had before they were rebuilt are another feed's, the same either way round here.)
    const auto ground_facts = [](const FeedRun& run) {
        std::size_t n = 0;
        for (const command::Fact& f : run.net.Facts()) n += !f.to.empty() || f.key.compare(0, 13, "ground_intsum") == 0 ? 1 : 0;
        return n;
    };
    SC_EXPECT(ground_facts(*a) == ground_facts(*b) && ground_facts(*a) >= x.Reports().size() + x.Summaries().size());
    for (std::size_t i = 0; i < x.Reports().size() && i < y.Reports().size(); ++i) {
        SC_EXPECT(x.Reports()[i].key == y.Reports()[i].key && x.Reports()[i].h == y.Reports()[i].h && x.Reports()[i].fraction == y.Reports()[i].fraction);
        const command::Delivery* p = a->At("army_ops", x.Reports()[i].fact);
        const command::Delivery* q = b->At("army_ops", y.Reports()[i].fact);
        SC_EXPECT((p == nullptr) == (q == nullptr) && (p == nullptr || (p->h == q->h && p->from == q->from && p->mean == q->mean)));
    }
    for (std::size_t i = 0; i < x.Summaries().size() && i < y.Summaries().size(); ++i) {
        SC_EXPECT(x.Summaries()[i].key == y.Summaries()[i].key && x.Summaries()[i].h == y.Summaries()[i].h && x.Summaries()[i].reports == y.Summaries()[i].reports);
        const command::Delivery* p = a->At("sector_cp_duty", x.Summaries()[i].fact);
        const command::Delivery* q = b->At("sector_cp_duty", y.Summaries()[i].fact);
        SC_EXPECT((p == nullptr) == (q == nullptr) && (p == nullptr || p->h == q->h));
    }
}

SC_TEST("ground feed (F3.8): ground.mode 0, or a chain without the posts: nothing added") {
    auto off = std::make_unique<FeedRun>();
    SC_EXPECT(off->Go("warsaw_pact", 21, 24.0, {}, false, false));
    if (!off->ok) return;
    SC_EXPECT(!off->feed.Ground().On() && off->campaign.Ground() == nullptr && off->feed.Ground().Reports().empty());
    for (const command::Fact& f : off->net.Facts()) SC_EXPECT(f.kind != command::FactKind::Situation && f.to.empty() && f.key.compare(0, 9, "ground_in") != 0);
    // The ground war on but the army's staff not in the chain: the feed stays off and the pipeline is the same.
    auto none = std::make_unique<FeedRun>();
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_warsaw_pact.json", none->data, &e));
    none->data.tuning.values["ground.mode"] = 1.0;
    SC_EXPECT(none->campaign.Init(none->data, 21, &e));
    none->net.Init(LoadChain(), 21, none->data.theatre.start_hour);
    none->feed.Bind(&none->net);
    command::GroundFeedRoles roles;
    roles.army_staff = "no_such_post";
    none->feed.UseGroundRoles(roles);
    while (none->campaign.Now() < 24.0) {
        if (none->campaign.Advance(24.0)) none->campaign.SkipPlayerMission();
        none->feed.Pump(none->campaign);
    }
    SC_EXPECT(none->campaign.Ground() != nullptr && !none->feed.Ground().On() && none->feed.Ground().Reports().empty());
    for (const command::Fact& f : none->net.Facts()) SC_EXPECT(f.kind != command::FactKind::Situation);
}

SC_TEST("ground feed (F3.8): our commanders' orders reach the unit they are to, and the enemy's air strikes on our troops go to the air defence staff's watch as the unit counted them") {
    auto r = std::make_unique<FeedRun>();
    // The watch at air_front.mode 1 with its usual reach would take few strikes for signs: it is told to take a strike anywhere
    // behind the line as a sign (reach 0), so every strike a unit reports shows.
    const std::map<std::string, double> tune = {{"air_front.mode", 1.0}, {"air_front.watch_cas_reach_m", 0.0}, {"air_front.watch_margin_m", 0.0}};
    SC_EXPECT(r->Go("nato", 8, 240.0, tune, false));
    if (!r->ok || !r->feed.Ground().On()) return;
    const command::GroundFeed& gf = r->feed.Ground();
    const groundwar::GroundWar& g = *r->campaign.Ground();
    std::printf("    %zu orders, %zu air strikes on our troops (%d to the watch, %d signs), %zu signs in all\n", gf.Orders().size(), g.SitAirs().size(),
                gf.AirReports(), gf.AirSigns(), r->feed.Signs().size());
    for (const command::OrderRecord& o : gf.Orders()) {
        const command::Fact& f = r->net.Facts()[static_cast<std::size_t>(o.fact)];
        SC_EXPECT(f.kind == command::FactKind::Order && f.h == o.h && f.to.size() >= 2u);
        const command::Delivery* d = r->net.Known(r->Post(f.to[0]), o.fact);
        const command::Delivery* c = r->net.Known(r->Post("army_ad_chief"), o.fact);
        std::printf("    %s\n", f.text.c_str());
        SC_EXPECT(d != nullptr && d->h >= o.h && c != nullptr && c->h >= o.h);
        if (gf.WithRegiment(o.formation)) SC_EXPECT(f.to.size() == 4u);
    }
    // Every strike a unit counted was told to the watch (the unit's staff compiling it), and where it was deep enough the
    // watch took it as a sign from that post, which reached the air defence staff after the strike.
    int counted = 0;
    for (const groundwar::SitAir& a : g.SitAirs()) counted += gf.PostOf(a.formation) >= 0 ? 1 : 0;
    SC_EXPECT(gf.AirReports() == counted);
    SC_EXPECT(gf.AirSigns() <= gf.AirReports());
    int troop_signs = 0;
    for (const command::Fact& f : r->net.Facts()) {
        if (f.key.compare(0, 19, "front_watch:troops:") != 0) continue;
        ++troop_signs;
        SC_EXPECT(f.kind == command::FactKind::RaidSeen && f.audience == "army_ad_staff" && r->Post(f.origin) >= 0);
        const command::Delivery* d = r->At("army_ad_staff", r->net.FactIndex(f.key));
        SC_EXPECT(d == nullptr || (d->h >= f.h && d->from >= 0));
    }
    SC_EXPECT(troop_signs == gf.AirSigns());
    // The strikes are in the returns as the unit counted them: the next return of that unit after a strike carries it.
    int in_returns = 0;
    for (const command::SituationReport& s : gf.Reports()) in_returns += s.strikes;
    SC_EXPECT(in_returns <= static_cast<int>(g.SitAirs().size()));
}
