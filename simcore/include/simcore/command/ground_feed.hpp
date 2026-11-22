// simcore/command/ground_feed.hpp — our units' situation reports on the ground war, put into the information pipeline at
// the people who compile and receive them (track F, F3.8; docs/fronts/03_GROUND_WAR.md section 10).
//
// The ground war (simcore/ground_war/) logs what our side's units compiled: each formation's return twice a day and at
// once when it reports, asks, stops, withdraws, breaks or re-forms, with the axes it reports and the enemy it believes it
// faces (what the unit believed, never the truth: the picture's own error), the orders our commanders gave and the army's
// intelligence summary. Here each becomes a fact held by a post of the chain and reaches the others only as the pipeline
// carries it (info_net.hpp):
//   * a unit's report: from its staff's duty officer to the army's operations section (and, for the division the SAM
//     regiment belongs to, to the regiment's chief of staff and the battery commander on the way down: the regiment's word
//     to the battery);
//   * the army's summary: its operations section, twice a day, sums the latest report of each unit that has reached it
//     (what it knows then, never more) for the national sector's command post and the chief of air defence;
//   * an order: from the commander's staff to the unit it is to (and the chief of air defence, and the regiment and
//     battery commander where it is their division);
//   * the army's intelligence summary of the enemy it does not touch.
// A unit that was struck from the air reports it as its staff counted it: the strikes into the army's air defence staff's
// watch through CampaignFeed::ReportAirAttack (F2.5's left), and the count in its next return.
// F4's commanders and the regiment's tasking read these records (LatestKnownBy), never the ground war's state.
//
// Only while the ground war runs (ground.mode 1: AirCampaign::Ground() non-null) and the chain has the posts; otherwise
// nothing is added. Nothing here polls: the logs are read by index when the campaign is pumped (a pump after every step and
// one after a load make the same facts), and the summaries are made at their hours.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/command/info_net.hpp"
#include "simcore/ground_war/ground_war.hpp"

namespace simcore {
namespace command {

// Who first writes and who receives each kind of report (game values: docs/fronts/03_GROUND_WAR.md section 10).
struct GroundFeedRoles {
    std::string army_staff = "army_ops";           // receives the formations' reports, sums them, sends the army's orders
    std::string army_intel = "army_intel";         // the army's intelligence summary
    std::string sector = "sector_cp_duty";         // the national air defence sector's command post
    std::string air_chief = "army_ad_chief";       // the army's chief of air defence
    std::string regiment = "rgt_chief_of_staff";   // the SAM regiment's staff
    std::string battery = "bty21_commander";       // the battery commander, down the regiment's way
    std::string regiment_unit = "sam_15";          // the order of battle's SAM regiment: the unit it is under is its division
    // The order of battle's units whose staffs have a post of the chain; a unit with none reports by the nearest above it
    // that has, or by the army's staff (the army's own brigades).
    std::map<std::string, std::string> staff_posts = {{"mrd_7", "div_chief_of_staff"},
                                                      {"mrd_9", "div9_staff_duty"},
                                                      {"sw_brigade", "swbde_staff_duty"},
                                                      {"tank_brigade", "tankbde_staff_duty"}};
    // The army's summary is made twice a day at these local hours, three hours after the units' 06:00 and 18:00 returns.
    std::vector<double> summary_local_h = {9.0, 21.0};
};

// One unit's report as the pipeline holds it: its fact and what it says, for the posts' later reading.
struct SituationReport {
    std::string key;
    int fact = -1;
    int formation = -1;
    std::string unit;      // the order of battle's id
    std::string post;      // who compiled it
    double h = 0.0;        // compiled
    groundwar::SitCause cause = groundwar::SitCause::Scheduled;
    bool own = true;
    double fraction = 0.0;
    double supply = 1.0;
    int sector = -1;
    bool out = false;
    int strikes = 0;
    double struck = 0.0;
    std::vector<groundwar::SitAxis> axes;
};

// An order as the pipeline holds it.
struct OrderRecord {
    std::string key;
    int fact = -1;
    groundwar::GroundOrder order = groundwar::GroundOrder::WithdrawOrdered;
    int formation = -1;
    std::string unit;
    int axis = -1;
    double h = 0.0;
    int other = -1;  // the formation a relief relieves, a withdrawal goes behind (-1 none)
};

// The army's summary: which reports it was made from (each had reached the army's staff by `h`).
struct ArmySummary {
    std::string key;
    int fact = -1;
    double h = 0.0;
    std::vector<int> reports;  // indices into GroundFeed::Reports()
    // The axes it gives: each axis's latest depth and pace in the reports that had reached the army's staff, with the hour
    // that report was compiled (what F4's commanders read of it, campaign/ad_strategy.hpp).
    std::vector<groundwar::SitAxis> axes;
    std::vector<double> axes_h;
};

// The army's intelligence summary as the pipeline holds it (F5.1): which of the enemy's formations not in contact it
// believed at about what strength, compiled `compiled_h`, the fact that carries it.
struct IntelRecord {
    std::string key;
    int fact = -1;
    double h = 0.0;
    double compiled_h = 0.0;
    struct Line {
        int formation = -1;
        double fraction = 0.0;
        bool out = false;
        int sector = -1;
    };
    std::vector<Line> lines;
};

class GroundFeed {
public:
    void Bind(InfoNet* net, const GroundFeedRoles& roles = GroundFeedRoles()) {
        net_ = net;
        roles_ = roles;
        Reset();
    }

    // On: the ground war runs and the chain has the posts (decided once, at the first pump).
    bool On() const { return on_; }
    const GroundFeedRoles& Roles() const { return roles_; }
    const std::vector<SituationReport>& Reports() const { return reports_; }
    const std::vector<OrderRecord>& Orders() const { return orders_; }
    const std::vector<ArmySummary>& Summaries() const { return summaries_; }
    const std::vector<IntelRecord>& Intels() const { return intels_; }
    // Intelligence summaries made (facts of kind Intsum, keys "ground_intsum:<n>").
    int IntelSummaries() const { return intel_made_; }
    // Strikes passed to the air defence watch, and those it took as signs.
    int AirReports() const { return air_reports_; }
    int AirSigns() const { return air_signs_; }

    // The post that reports a formation (an index into the chain's positions, -1 none).
    int PostOf(int formation) const {
        return formation >= 0 && static_cast<std::size_t>(formation) < post_of_.size() ? post_of_[static_cast<std::size_t>(formation)] : -1;
    }
    // Is the formation the SAM regiment's division, or under it?
    bool WithRegiment(int formation) const {
        return formation >= 0 && static_cast<std::size_t>(formation) < with_regiment_.size() && with_regiment_[static_cast<std::size_t>(formation)] != 0;
    }

    // The latest report of a formation that has reached `position` (by its compile hour), nullptr if none: what a post
    // holds of the unit's own state. `own_only`: a return with the unit's own state (not an axis's news alone).
    const SituationReport* LatestKnownBy(int position, int formation, bool own_only = true) const {
        if (net_ == nullptr) return nullptr;
        const SituationReport* best = nullptr;
        for (const SituationReport& r : reports_) {
            if (r.formation != formation || (own_only && !r.own) || (best != nullptr && r.h < best->h)) continue;
            if (net_->Known(position, r.fact) != nullptr) best = &r;
        }
        return best;
    }
    // The latest report on an axis that has reached `position`: its depth and pace as the reporting unit gave them.
    const groundwar::SitAxis* LatestAxisKnownBy(int position, int axis, double* compiled_h = nullptr) const {
        if (net_ == nullptr) return nullptr;
        const groundwar::SitAxis* best = nullptr;
        double best_h = -1e18;
        for (const SituationReport& r : reports_) {
            if (r.h < best_h || net_->Known(position, r.fact) == nullptr) continue;
            for (const groundwar::SitAxis& a : r.axes) {
                if (a.axis != axis) continue;
                best = &a;
                best_h = r.h;
            }
        }
        if (compiled_h != nullptr) *compiled_h = best != nullptr ? best_h : 0.0;
        return best;
    }

    // Take what the ground war compiled since the last call into the net as facts. `air_attack(origin, h, at, what)` is
    // the air defence watch's (CampaignFeed::ReportAirAttack). Call before the net is advanced.
    template <class AirAttack>
    void Feed(const airwar::AirCampaign& c, AirAttack&& air_attack) {
        if (net_ == nullptr || !Ready(c)) return;
        const groundwar::GroundWar& g = *c.Ground();
        const std::vector<groundwar::SitReport>& reps = g.SitReports();
        for (; reports_read_ < reps.size(); ++reports_read_) AddReport(c, g, reps[reports_read_], reports_read_);
        const std::vector<groundwar::SitOrder>& ords = g.SitOrders();
        for (; orders_read_ < ords.size(); ++orders_read_) AddOrder(c, g, ords[orders_read_], orders_read_);
        const std::vector<groundwar::SitAir>& airs = g.SitAirs();
        for (; air_read_ < airs.size(); ++air_read_) {
            const groundwar::SitAir& a = airs[air_read_];
            const int post = PostOf(a.formation);
            if (post < 0) continue;
            char buf[160];
            std::snprintf(buf, sizeof(buf), "%s was attacked from the air; its staff counts about %.0f%% of its strength lost.",
                          g.Formations()[static_cast<std::size_t>(a.formation)].name.c_str(), 100.0 * a.lost);
            ++air_reports_;
            if (air_attack(net_->Data().positions[static_cast<std::size_t>(post)].id, a.h, a.at, std::string(buf))) {
                ++air_signs_;
            }
        }
        const std::vector<groundwar::SitIntel>& ints = g.SitIntels();
        for (; intel_read_ < ints.size(); ++intel_read_) AddIntel(c, g, ints[intel_read_]);
    }

    // The army's summaries due: at each hour up to what is settled (the earliest a package not yet resolved could still
    // change what reached the army's staff), sum the latest report of each unit that had reached it by then. True when one
    // was added (the net is to be advanced again).
    bool Summarise(const airwar::AirCampaign& c) {
        if (net_ == nullptr || !on_) return false;
        double watermark_h = c.Now();
        for (const airwar::Package& p : c.Packages()) {
            if (!p.resolved) watermark_h = std::min(watermark_h, p.FirstH());
        }
        bool added = false;
        if (next_summary_h_ < 0.0) next_summary_h_ = NextSummaryAfter(-1e-3);
        while (next_summary_h_ <= watermark_h + 1e-9) {
            added = SummariseAt(c, next_summary_h_) || added;
            next_summary_h_ = NextSummaryAfter(next_summary_h_);
        }
        return added;
    }

private:
    InfoNet* net_ = nullptr;
    GroundFeedRoles roles_;
    bool ready_ = false;
    bool on_ = false;
    int army_ = -1;
    std::vector<int> post_of_;          // by formation
    std::vector<char> with_regiment_;   // by formation
    std::size_t reports_read_ = 0, orders_read_ = 0, air_read_ = 0, intel_read_ = 0;
    std::vector<SituationReport> reports_;
    std::vector<OrderRecord> orders_;
    std::vector<ArmySummary> summaries_;
    std::vector<IntelRecord> intels_;
    double next_summary_h_ = -1.0;
    int intel_made_ = 0;
    int air_reports_ = 0;
    int air_signs_ = 0;

    void Reset() {
        ready_ = on_ = false;
        army_ = -1;
        post_of_.clear();
        with_regiment_.clear();
        reports_read_ = orders_read_ = air_read_ = intel_read_ = 0;
        reports_.clear();
        orders_.clear();
        summaries_.clear();
        intels_.clear();
        next_summary_h_ = -1.0;
        intel_made_ = air_reports_ = air_signs_ = 0;
    }

    // On where the ground war runs, the chain has the roles' posts and every formation has one to report by; decided once.
    bool Ready(const airwar::AirCampaign& c) {
        if (ready_) return on_;
        ready_ = true;
        const groundwar::GroundWar* g = c.Ground();
        if (g == nullptr) return false;
        const ChainData& d = net_->Data();
        army_ = d.PositionIndex(roles_.army_staff);
        if (army_ < 0 || d.PositionIndex(roles_.army_intel) < 0 || d.PositionIndex(roles_.sector) < 0 || d.PositionIndex(roles_.air_chief) < 0 ||
            d.PositionIndex(roles_.regiment) < 0 || d.PositionIndex(roles_.battery) < 0) {
            return false;
        }
        for (const auto& kv : roles_.staff_posts) {
            if (d.PositionIndex(kv.second) < 0) return false;
        }
        const command::OrderOfBattle& oob = g->Data().ours.oob;
        const command::OobUnit* sam = oob.Find(roles_.regiment_unit);
        const std::string division = sam != nullptr ? sam->parent : std::string();
        const std::vector<groundwar::Formation>& forms = g->Formations();
        post_of_.assign(forms.size(), army_);
        with_regiment_.assign(forms.size(), 0);
        for (std::size_t i = 0; i < forms.size(); ++i) {
            if (forms[i].unit.side != groundwar::Side::Ours) continue;
            const command::OobUnit* u = oob.Find(forms[i].unit.id);
            for (std::size_t guard = 0; u != nullptr && guard <= oob.units.size(); ++guard) {
                const auto it = roles_.staff_posts.find(u->id);
                if (it != roles_.staff_posts.end()) {
                    post_of_[i] = d.PositionIndex(it->second);
                    break;
                }
                u = u->parent.empty() ? nullptr : oob.Find(u->parent);
            }
            if (!division.empty() && oob.Under(forms[i].unit.id, division)) with_regiment_[i] = 1;
        }
        on_ = true;
        return true;
    }

    const std::string& IdOf(int position) const { return net_->Data().positions[static_cast<std::size_t>(position)].id; }

    static std::string SectorName(const airwar::AirCampaign& c, int sector) {
        const airwar::LineOfContact& line = c.FrontLine();
        if (sector < 0 || static_cast<std::size_t>(sector) >= line.SectorCount()) return "the rear";
        const airwar::SectorDef& s = line.Sector(static_cast<std::size_t>(sector));
        return s.name.empty() ? s.id : s.name;
    }

    // Who is to hold a unit's report: the army's staff, and the regiment's chief of staff and the battery commander
    // (down its way) for the division it belongs to.
    std::vector<std::string> Addressees(int formation) const {
        std::vector<std::string> to{roles_.army_staff};
        if (WithRegiment(formation)) {
            to.push_back(roles_.regiment);
            to.push_back(roles_.battery);
        }
        return to;
    }

    static const char* CauseText(groundwar::SitCause why) {
        switch (why) {
            case groundwar::SitCause::Scheduled: return "combat report";
            case groundwar::SitCause::Reported: return "reports at once: under its degraded line";
            case groundwar::SitCause::Asked: return "asks to be relieved or to withdraw";
            case groundwar::SitCause::AttackStopped: return "reports its attack stopped";
            case groundwar::SitCause::Withdrawn: return "reports it has withdrawn from the line to re-form";
            case groundwar::SitCause::Dispersed: return "reports it has been broken and dispersed";
            case groundwar::SitCause::Reformed: return "reports it has re-formed behind the line";
            case groundwar::SitCause::AxisNews: return "reports the enemy's advance";
        }
        return "report";
    }

    void AddReport(const airwar::AirCampaign& c, const groundwar::GroundWar& g, const groundwar::SitReport& r, std::size_t index) {
        const int post = PostOf(r.formation);
        if (post < 0) return;
        const double start = c.Data().theatre.start_hour;
        const std::string unit = g.Formations()[static_cast<std::size_t>(r.formation)].name;
        std::string text = FormatClock(r.h, start) + ". " + unit + " " + CauseText(r.cause);
        char buf[256];
        if (r.own) {
            std::snprintf(buf, sizeof(buf), ": strength about %.0f%% of establishment, supplies %.0f%%, %s", 100.0 * r.fraction, 100.0 * r.supply,
                          r.out ? "out of the fight" : "in the fight");
            text += buf;
            if (r.sector >= 0) text += ", in " + SectorName(c, r.sector);
            text += ".";
            if (r.strikes > 0) {
                std::snprintf(buf, sizeof(buf), " Enemy air attacks since its last report: %d, about %.0f%% of its strength.", r.strikes, 100.0 * r.struck);
                text += buf;
            }
        } else {
            text += ".";
        }
        SituationReport rec;
        rec.formation = r.formation;
        rec.unit = g.Formations()[static_cast<std::size_t>(r.formation)].unit.id;
        rec.post = IdOf(post);
        rec.h = r.h;
        rec.cause = r.cause;
        rec.own = r.own;
        rec.fraction = r.fraction;
        rec.supply = r.supply;
        rec.sector = r.sector;
        rec.out = r.out;
        rec.strikes = r.strikes;
        rec.struck = r.struck;
        const std::vector<groundwar::SitAxis>& axes = g.SitAxes();
        for (std::uint32_t k = 0; k < r.axes_count; ++k) {
            const groundwar::SitAxis& a = axes[r.axes_first + k];
            rec.axes.push_back(a);
            const std::string& name = g.Axes()[static_cast<std::size_t>(a.axis)].name;
            std::snprintf(buf, sizeof(buf), " On the %s axis the enemy is %.0f km in, advancing %.0f km a day", name.c_str(), a.depth_m / 1000.0, a.pace_km_day);
            text += buf;
            if (a.counterattacked) text += "; our counterattack is on it";
            if (a.enemy_n > 0) {
                std::snprintf(buf, sizeof(buf), "; %d of his formations in contact, at about %.0f%% of their strength", a.enemy_n, 100.0 * a.enemy_fraction);
                text += buf;
            }
            text += ".";
        }
        Fact f;
        f.key = "sitrep:" + std::to_string(index);
        f.kind = FactKind::Situation;
        f.h = r.h;
        f.origin = rec.post;
        f.subject = "situation:" + rec.unit;
        f.text = text;
        std::snprintf(buf, sizeof(buf), "%s: about %.0f%% of strength%s.", unit.c_str(), 100.0 * r.fraction, r.cause == groundwar::SitCause::Scheduled ? "" : " (it reports at once)");
        f.summary = r.own ? std::string(buf) : unit + " reports the enemy's advance.";
        f.to = Addressees(r.formation);
        f.precedence = 1;  // a combat report is a priority message: the wire if it is whole, else the radio, else a despatch rider
        f.document = r.cause == groundwar::SitCause::Scheduled;
        rec.key = f.key;
        rec.fact = net_->AddFact(f);
        reports_.push_back(std::move(rec));
    }

    void AddOrder(const airwar::AirCampaign& c, const groundwar::GroundWar& g, const groundwar::SitOrder& o, std::size_t index) {
        const int to_post = PostOf(o.formation);
        if (to_post < 0) return;
        const groundwar::GroundCommander* who =
            o.issuer >= 0 && static_cast<std::size_t>(o.issuer) < g.Commanders().size() ? &g.Commanders()[static_cast<std::size_t>(o.issuer)] : nullptr;
        const int origin = who != nullptr && !who->top && who->formation >= 0 ? PostOf(who->formation) : army_;
        const std::string& name = g.Formations()[static_cast<std::size_t>(o.formation)].name;
        const std::string other = o.other >= 0 && static_cast<std::size_t>(o.other) < g.Formations().size() ? g.Formations()[static_cast<std::size_t>(o.other)].name : "";
        const std::string axis = o.axis >= 0 && static_cast<std::size_t>(o.axis) < g.Axes().size() ? g.Axes()[static_cast<std::size_t>(o.axis)].name : "";
        std::string text = FormatClock(o.h, c.Data().theatre.start_hour) + ". Order";
        char buf[256];
        switch (o.order) {
            case groundwar::GroundOrder::WithdrawOrdered:
                text += ": " + name + " is to withdraw from the line to re-form" + (other.empty() ? std::string() : ", behind " + other) + ".";
                break;
            case groundwar::GroundOrder::ReliefOrdered:
                std::snprintf(buf, sizeof(buf), ": %s is to relieve %s%s%s (due in about %.0f h).", name.c_str(), other.c_str(), axis.empty() ? "" : " on the ",
                              axis.empty() ? "" : (axis + " axis").c_str(), o.value);
                text += buf;
                break;
            case groundwar::GroundOrder::CounterattackOrdered:
                std::snprintf(buf, sizeof(buf), ": %s is to counterattack on the %s axis (the army's staff reckons %.1f to 1).", name.c_str(), axis.c_str(),
                              std::min(o.value, 99.0));
                text += buf;
                break;
            default:
                text += ": " + name + ".";
                break;
        }
        Fact f;
        f.key = "sitorder:" + std::to_string(index);
        f.kind = FactKind::Order;
        f.h = o.h;
        f.origin = IdOf(origin);
        f.subject = "ground_order:" + g.Formations()[static_cast<std::size_t>(o.formation)].unit.id;
        f.text = text;
        f.summary = std::string(groundwar::GroundOrderName(o.order)) + ": " + name + ".";
        f.to = {IdOf(to_post), roles_.air_chief};
        if (WithRegiment(o.formation)) {
            f.to.push_back(roles_.regiment);
            f.to.push_back(roles_.battery);
        }
        f.precedence = 1;
        f.document = true;
        OrderRecord rec;
        rec.key = f.key;
        rec.order = o.order;
        rec.formation = o.formation;
        rec.unit = g.Formations()[static_cast<std::size_t>(o.formation)].unit.id;
        rec.axis = o.axis;
        rec.h = o.h;
        rec.other = o.other;
        rec.fact = net_->AddFact(f);
        orders_.push_back(std::move(rec));
    }

    void AddIntel(const airwar::AirCampaign& c, const groundwar::GroundWar& g, const groundwar::SitIntel& s) {
        const std::vector<groundwar::SitIntelLine>& lines = g.SitIntelLines();
        std::string text = FormatClock(s.h, c.Data().theatre.start_hour) + ". The army's intelligence summary of the enemy not in contact (compiled " +
                           FormatClock(s.compiled_h, c.Data().theatre.start_hour) + "):";
        char buf[192];
        IntelRecord rec;
        rec.h = s.h;
        rec.compiled_h = s.compiled_h;
        for (std::uint32_t k = 0; k < s.lines_count; ++k) {
            const groundwar::SitIntelLine& l = lines[s.lines_first + k];
            rec.lines.push_back({l.formation, l.fraction, l.out, l.sector});
            std::snprintf(buf, sizeof(buf), " %s about %.0f%%%s;", g.Formations()[static_cast<std::size_t>(l.formation)].name.c_str(), 100.0 * l.fraction,
                          l.out ? " (out of the fight)" : "");
            text += buf;
        }
        text.back() = '.';
        Fact f;
        f.key = "ground_intsum:" + std::to_string(++intel_made_);
        f.kind = FactKind::Intsum;
        f.h = s.h;
        f.origin = roles_.army_intel;
        f.subject = "ground_intsum";
        f.text = text;
        f.to = {roles_.army_staff, roles_.air_chief, roles_.sector};
        f.precedence = 0;
        f.document = true;
        rec.key = f.key;
        rec.fact = net_->AddFact(f);
        intels_.push_back(std::move(rec));
    }

    // The first summary hour after h (campaign hours): the local hours of the roles, each day.
    double NextSummaryAfter(double h) const {
        const double start = net_->StartHour();
        const double day = std::floor((start + h) / 24.0);
        for (int k = 0; k < 3; ++k) {
            for (double hr : roles_.summary_local_h) {
                const double t = 24.0 * (day + k) + hr - start;
                if (t > h + 1e-9) return t;
            }
        }
        return h + 12.0;
    }

    // The army's summary at hour t: for each unit the latest return that had reached the army's staff by t.
    bool SummariseAt(const airwar::AirCampaign& c, double t) {
        const groundwar::GroundWar& g = *c.Ground();
        std::map<int, std::size_t> latest;  // formation -> report
        for (std::size_t i = 0; i < reports_.size(); ++i) {
            const SituationReport& r = reports_[i];
            if (!r.own || r.h > t) continue;
            const Delivery* d = net_->Known(army_, r.fact);
            if (d == nullptr || d->h > t) continue;
            const auto it = latest.find(r.formation);
            if (it == latest.end() || reports_[it->second].h <= r.h) latest[r.formation] = i;
        }
        if (latest.empty()) return false;
        std::string text = FormatClock(t, c.Data().theatre.start_hour) + ". The army's summary of its formations, from the reports that have reached its operations section:";
        char buf[256];
        ArmySummary sum;
        for (const auto& kv : latest) {
            const SituationReport& r = reports_[kv.second];
            std::snprintf(buf, sizeof(buf), " %s about %.0f%% (reported %s%s%s);", g.Formations()[static_cast<std::size_t>(r.formation)].name.c_str(),
                          100.0 * r.fraction, FormatClock(r.h, c.Data().theatre.start_hour).c_str(), r.out ? ", out of the fight" : "",
                          r.strikes > 0 ? ", struck from the air" : "");
            text += buf;
            sum.reports.push_back(static_cast<int>(kv.second));
        }
        // The axes: the latest depth and pace of each in the reports that reached it.
        std::map<int, std::pair<double, const groundwar::SitAxis*>> axes;
        for (const SituationReport& r : reports_) {
            if (r.h > t) continue;
            const Delivery* d = net_->Known(army_, r.fact);
            if (d == nullptr || d->h > t) continue;
            for (const groundwar::SitAxis& a : r.axes) {
                const auto it = axes.find(a.axis);
                if (it == axes.end() || it->second.first <= r.h) axes[a.axis] = {r.h, &a};
            }
        }
        for (const auto& kv : axes) {
            std::snprintf(buf, sizeof(buf), " the %s axis %.0f km in at %.0f km a day%s;", g.Axes()[static_cast<std::size_t>(kv.first)].name.c_str(),
                          kv.second.second->depth_m / 1000.0, kv.second.second->pace_km_day, kv.second.second->counterattacked ? ", our counterattack on it" : "");
            text += buf;
            sum.axes.push_back(*kv.second.second);
            sum.axes_h.push_back(kv.second.first);
        }
        text.back() = '.';
        Fact f;
        f.key = "sitsummary:" + std::to_string(summaries_.size() + 1);
        f.kind = FactKind::Situation;
        f.h = t;
        f.origin = roles_.army_staff;
        f.subject = "situation:army";
        f.text = text;
        f.summary = "The army's summary of the ground situation (the paper follows).";
        f.to = {roles_.sector, roles_.air_chief};
        f.precedence = 0;
        f.document = true;
        sum.key = f.key;
        sum.h = t;
        sum.fact = net_->AddFact(f);
        summaries_.push_back(std::move(sum));
        return true;
    }
};

}  // namespace command
}  // namespace simcore
