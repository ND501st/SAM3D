// simcore/command/air_war_feed.hpp — what our side learns in the air war, put into the information pipeline at the
// people who would first know it (docs/command/06_INTEGRATION.md).
//
// The air war (simcore/air_war/) decides what our side's organisation learns and when: its journal's visible entries
// (the war, raid warnings, the battery's own fights and moves, prisoners, intercepts, analysts, the press), the raids the
// warning net saw, the objects wrecked. Before this file the campaign screen showed all of it to the player at once.
// Now each becomes a fact held by a post of the chain, and reaches the player's character only as the pipeline carries
// it (info_net.hpp). The air war also shapes the roads and lines the pipeline uses: where bombs fall, carriers on the
// road can be lost and the posts nearby lose their wire for a while; the battery's post moves with the battery.
// The feed only reads the campaign; call Pump after every campaign step (and once after loading a save: the net is a
// function of the campaign's history, so it comes out the same).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "simcore/air_war/abstract_radio.hpp"
#include "simcore/air_war/campaign.hpp"
#include "simcore/command/air_front_watch.hpp"
#include "simcore/command/ground_feed.hpp"
#include "simcore/command/info_net.hpp"
#include "simcore/command/intercept.hpp"

namespace simcore {
namespace command {

// The campaign's sky on its information net (roadmap R7, docs/LIGHT.md): on a dated theatre the couriers' night is the
// sun's at the theatre's origin, as the air war's parts of the day are (airwar::TheatrePartAt); an undated theatre keeps
// the net's fixed 20:00-06:00. Call after InfoNet::Init with the theatre's start_hour. True when a sky was given. The
// light's values are the theatre's (light.json's, roadmap S8) unless others are given.
inline bool GiveTheatreSky(InfoNet& net, const airwar::TheatreDef& t, const LightValues& values) {
    if (t.day_of_year <= 0) return false;
    net.SetSky(airwar::TheatreSky(t), values);
    return true;
}
inline bool GiveTheatreSky(InfoNet& net, const airwar::TheatreDef& t) { return GiveTheatreSky(net, t, t.light); }

// Who first knows each kind of news, and the air war's effects on the pipeline (game values, docs/command/06).
struct FeedRoles {
    std::string sector = "sector_cp_duty";            // raid warnings (the national sector sees them coming)
    std::string warning_net = "div_ad_cp_duty";       // the raids the warning net saw
    std::string army_intel = "army_intel";            // intercepts, the analysts
    std::string div_intel = "div_intel";              // prisoners' interrogations
    std::string police = "kesmark_police";            // airmen picked up in the district
    std::string intercept = "intercept_centre";       // the hours warning from the enemy's administrative traffic (roadmap B9)
    std::string staff = "rgt_chief_of_staff";         // the regiment's estimate of what the enemy knows of the battery
    std::string battery = "bty21_commander";          // the battery's own news and fights
    std::string announcer = "army_ad_chief";          // the war itself
    std::string political = "rgt_political_deputy";   // the press
    std::string air_staff = "army_ad_staff";          // the signs of the enemy's air front, and its conclusions (F2.5)
    std::string air_staff_for = "div_ad_chief";       // whose tree the conclusions are for (to the battery commanders)
    double danger_radius_m = 6000.0;                  // around a raid's target, while it is over it
    double danger_chance = 0.25;                      // a carrier on the road there then
    double wire_cut_radius_m = 1500.0;                // posts this close to where bombs fell lose their wire
    double wire_repair_h = 6.0;                       // for this long
};

class CampaignFeed {
public:
    void Bind(InfoNet* net, const FeedRoles& roles = FeedRoles()) {
        net_ = net;
        roles_ = roles;
        journal_read_ = raids_read_ = track_read_ = 0;
        packages_seen_.clear();
        wrecks_seen_.clear();
        battery_people_.clear();
        battery_places_.clear();
        if (net_ != nullptr) regiment_ = net_->Data().unit_regiment;
        ResetIntercept();
        ResetWatch();
        ground_.Bind(net_);
    }

    // Our units' reports on the ground war (F3.8, command/ground_feed.hpp): on only where the ground war runs
    // (ground.mode 1) and the chain has the posts. Other posts or hours than the data's: call after Bind, before the first Pump.
    void UseGroundRoles(const GroundFeedRoles& roles) { ground_.Bind(net_, roles); }
    const GroundFeed& Ground() const { return ground_; }

    // Our radio intercept service in the campaign (roadmap S6, radio.mode 1): the posts of data/command/intercept.json hear
    // the abstract packages' calls (air_war/abstract_radio.hpp) and the desk writes them up for the air section's day summary;
    // the calls our posts logged in a played session (PackageOutcome::heard) become the same facts they made there. Call after
    // Bind, before the first Pump; without it (or at radio.mode 0) the feed is as before S6.
    void UseIntercept(const InterceptData& data) {
        idata_ = data;
        intercept_ = true;
        ResetIntercept();
    }
    // What the posts wrote of the abstract calls (the debrief's), and the hearing behind it.
    const InterceptDesk& AbstractDesk() const { return desk_; }
    const std::vector<airwar::Heard>& AbstractHeard() const { return heard_; }
    const std::vector<airwar::AbstractCall>& AbstractCalls() const { return calls_; }

    // The staff's operational security shows (or not) in its administrative traffic hours before a raid (roadmap B9,
    // docs/air_battle/09 section 5.3): a draw keyed on the package, so a loaded campaign replays it. Our centre has it as a fact.
    void AddOpsecAlert(const airwar::AirCampaign& c, const airwar::Package& p, int target_index) {
        const airwar::FactionRadio& fr = c.Data().faction.radio;
        if (!fr.present || net_->Data().PositionIndex(roles_.intercept) < 0) return;
        AirRadioDef td;
        for (const auto& kv : AirRadioTuningDefaults()) td.tune[kv.first] = c.Data().tuning.Get(kv.first);
        double leak = 0.0;
        if (p.planner >= 0 && static_cast<std::size_t>(p.planner) < c.Staff().officers.size()) leak = c.Staff().officers[static_cast<std::size_t>(p.planner)].Quirk("comsec_leak");
        const airwar::OpsecAlert alert = airwar::DrawOpsecAlert(airwar::StaffOpsec(fr.opsec, leak, td), c.Seed(), static_cast<std::uint64_t>(p.id), td);
        const std::string where = target_index >= 0 ? c.Data().theatre.objects[static_cast<std::size_t>(target_index)].name : std::string("the district");
        for (const Fact& f : OpsecAlertFacts(alert, p.id, p.tot_h, where, roles_.intercept)) net_->AddFact(f);
    }

    // Read what the campaign added since the last call, then bring the net up to the campaign's hour.
    void Pump(const airwar::AirCampaign& c) {
        if (net_ == nullptr) return;
        const ChainData& d = net_->Data();
        // The battery's places (its command post, its vehicles) stand at the player's battery and move with it, each
        // keeping its offset; the people posted there are the battery's own (they see its fights themselves).
        const airwar::TheatreObject& battery = c.Data().theatre.objects[static_cast<std::size_t>(c.PlayerIndex())];
        if (battery_places_.empty()) {
            for (const Place& pl : d.places) {
                if (pl.theatre_object == battery.id) battery_places_.push_back(pl.id);
            }
            for (const Position& p : d.positions) {
                if (std::find(battery_places_.begin(), battery_places_.end(), p.place) != battery_places_.end()) battery_people_.push_back(p.id);
            }
        }
        const auto& track = c.BatteryTrack();
        for (; track_read_ < track.size(); ++track_read_) {
            for (const std::string& id : battery_places_) {
                const Place* pl = d.FindPlace(id);
                net_->MovePlace(id, track[track_read_].first, track[track_read_].second + (pl->pos - battery.pos));
            }
        }
        // Where bombs fall: danger on the roads while the raid is over its target; the posts close by lose their wire.
        const auto& packages = c.Packages();
        for (const airwar::Package& p : packages) {
            if (!p.resolved || packages_seen_.count(p.id) > 0) continue;
            packages_seen_.insert(p.id);
            const int ti = p.target_object.empty() ? -1 : c.Data().theatre.IndexOf(p.target_object);
            const Vec3 at = ti >= 0 ? c.Truth()[static_cast<std::size_t>(ti)].pos : p.target_pos;
            Danger dz;
            dz.center = at;
            dz.radius_m = roles_.danger_radius_m;
            dz.t0_h = p.tot_h - 0.25;
            dz.t1_h = p.tot_h + 0.25;
            dz.chance = roles_.danger_chance;
            dz.known_h = std::min(p.FirstH(), p.tot_h);
            net_->AddDanger(dz);
            AddOpsecAlert(c, p, ti);
            bool released = false;
            for (const airwar::Flight& fl : p.flights) released = released || fl.released;
            if (!released) continue;
            for (const Place& pl : d.places) {
                if (std::hypot(pl.pos.x - at.x, pl.pos.y - at.y) < roles_.wire_cut_radius_m) {
                    net_->CutWire(pl.id, p.tot_h, p.tot_h + roles_.wire_repair_h);
                }
            }
        }
        // What was wrecked: those close by saw it; the nearest post reports it.
        const auto& objs = c.Data().theatre.objects;
        for (std::size_t i = 0; i < objs.size() && i < c.Truth().size(); ++i) {
            const airwar::ObjectState& st = c.Truth()[i];
            const airwar::TheatreObject& o = objs[i];
            if (st.wrecked_h < 0.0 || o.decoy || o.kind == airwar::ObjectKind::City) continue;
            const std::string key = "wreck:" + o.id + ":" + std::to_string(static_cast<long long>(std::llround(st.wrecked_h * 100.0)));
            if (!wrecks_seen_.insert(key).second) continue;
            Fact f;
            f.key = key;
            f.kind = FactKind::OwnDamage;
            f.h = st.wrecked_h;
            f.subject = o.id;
            f.text = o.name + " was wrecked in an air raid.";
            f.audience = regiment_;
            f.need_level = 3;
            const bool player_battery = static_cast<int>(i) == c.PlayerIndex();
            f.precedence = player_battery || o.kind == airwar::ObjectKind::Bridge ? 1 : 0;
            // Witnesses: everyone posted within 3 km; the reporter: the nearest post within 10 km, else the division.
            double best = 1e18;
            std::string reporter = roles_.warning_net;
            for (const Position& p : d.positions) {
                const int pl = d.PlaceIndex(p.place);
                if (pl < 0) continue;
                const Vec3 pp = net_->PlacePos(pl, st.wrecked_h);
                const double dist = std::hypot(pp.x - st.pos.x, pp.y - st.pos.y);
                if (dist < 3000.0) f.witnesses.push_back(p.id);
                if (dist < best && dist < 10000.0) {
                    best = dist;
                    reporter = p.id;
                }
            }
            f.origin = reporter;
            net_->AddFact(f);
        }
        // The raids the warning net saw (and the battery saw itself when they came through its sector).
        const auto& raids = c.ObservedRaids();
        for (; raids_read_ < raids.size(); ++raids_read_) {
            const airwar::ObservedRaid& r = raids[raids_read_];
            Fact f;
            f.key = "raid:" + std::to_string(raids_read_);
            f.kind = FactKind::RaidSeen;
            f.h = r.h;
            f.origin = roles_.warning_net;
            f.subject = std::to_string(raids_read_);
            f.text = FormatClock(r.h, c.Data().theatre.start_hour) + ": " + std::to_string(r.aircraft) + " aircraft from " + r.entry + " at " +
                     airwar::DayPartName(r.part) + " went for " + r.target + "; " + std::to_string(r.lost) + " shot down" +
                     (r.delivered ? "." : ", no bombs on target.");
            f.audience = regiment_;
            f.need_level = 3;
            f.precedence = 0;
            if (r.near_battery || r.played) f.witnesses = battery_people_;
            net_->AddFact(f);
            if (ReadyWatch(c)) WatchRaid(c, r, raids_read_);
        }
        // The journal's entries our side knows.
        const auto& journal = c.Journal();
        for (; journal_read_ < journal.size(); ++journal_read_) {
            const airwar::JournalEntry& j = journal[journal_read_];
            if (!j.visible) continue;
            Fact f;
            f.key = "journal:" + std::to_string(journal_read_);
            f.h = j.h;
            f.text = j.text;
            f.audience = regiment_;
            if (!FromJournal(c, j, f)) continue;
            net_->AddFact(f);
        }
        if (intercept_ && c.Data().tuning.Get("radio.mode") >= 1.0 && c.Data().faction.radio.present) {
            PumpHeard(c);
            PumpAbstract(c);
            if (ReadyWatch(c)) WatchDesk(c);
        }
        // Our units' reports and orders on the ground war, and their strikes from the air into the watch (F3.8).
        if (c.Ground() != nullptr) {
            ground_.Feed(c, [&](const std::string& origin, double h, const Vec3& at, const std::string& what) { return ReportAirAttack(c, origin, h, at, what); });
        }
        net_->AdvanceTo(c.Now());
        bool more = ReadyWatch(c) && ReviewWatch(c);  // the staff's conclusions on their way
        more = ground_.Summarise(c) || more;          // the army's summary of the reports that reached it
        if (more) net_->AdvanceTo(c.Now());
    }

    // ---------------------------------------------------------------- our side learns the enemy's air front (F2.5)

    // The army's air defence staff's watch on the enemy's air front (command/air_front_watch.hpp): on only where the air
    // war runs its air front (air_front.mode 1) and the chain has the staff's post; its signs and conclusions are facts.
    bool WatchOn() const { return watch_on_; }
    const AirFrontWatch& Watch() const { return watch_; }
    // Every sign sent to the staff (the warning net's, the intercept centre's, the troops'), with when it reached it (-1: not
    // yet, or not yet taken), in the order they were sent.
    const std::vector<FrontSignReport>& Signs() const { return signs_; }
    // Our troops report an air attack (F3.8 will call it): to the army's air defence staff as a sign, when it lies deeper
    // than the close support's usual reach. `origin` is the post that reports it. False when the watch is off or not deep.
    bool ReportAirAttack(const airwar::AirCampaign& c, const std::string& origin, double h, const Vec3& at, const std::string& what) {
        if (net_ == nullptr || !ReadyWatch(c) || net_->Data().PositionIndex(origin) < 0) return false;
        return SendSign(c, FrontSign::Troops, at, h, origin, "front_watch:troops:" + std::to_string(++troop_reports_), what);
    }
    // The staff's estimate of a sector's air front beyond the line of contact as a post knows it now: the latest
    // conclusion on it that has reached that post (0: none). What our side's sector priorities and tasking read (F4).
    double AirFrontKnownBy(int position, int sector) const {
        if (net_ == nullptr) return 0.0;
        const auto& cs = watch_.Conclusions();
        for (std::size_t i = cs.size(); i-- > 0;) {
            if (cs[i].sector != sector || cs[i].key.empty()) continue;
            if (net_->Known(position, net_->FactIndex(cs[i].key)) != nullptr) return cs[i].depth_m;
        }
        return 0.0;
    }

    // The calls the battery's people have made on the radio by now (the player's own among them), for the enemy's
    // radio intercept (roadmap P3: AirCampaign::NoteTransmissions; it hears each once). After Pump.
    std::vector<airwar::Transmission> BatteryCalls() const {
        std::vector<airwar::Transmission> out;
        if (net_ == nullptr) return out;
        const ChainData& d = net_->Data();
        const SignalMean* radio = d.FindMean("radio_voice");
        for (const RadioCall& call : net_->RadioCalls()) {
            const std::string& who = d.positions[static_cast<std::size_t>(call.position)].id;
            if (std::find(battery_people_.begin(), battery_people_.end(), who) == battery_people_.end()) continue;
            airwar::Transmission t;
            t.h = call.h;
            t.intercept = (radio != nullptr ? radio->intercept : 1.0) * call.heard;  // the EM layer's listeners, when on (P5)
            t.key = net_->Facts()[static_cast<std::size_t>(call.fact)].key + "@" + who + "@" + std::to_string(call.h);
            out.push_back(t);
        }
        return out;
    }

private:
    InfoNet* net_ = nullptr;
    FeedRoles roles_;
    std::size_t journal_read_ = 0;
    std::size_t raids_read_ = 0;
    std::size_t track_read_ = 0;
    std::set<int> packages_seen_;
    std::set<std::string> wrecks_seen_;
    std::vector<std::string> battery_people_;
    std::vector<std::string> battery_places_;
    std::string regiment_;
    // Roadmap S6: the intercept service's state (a function of the campaign's history, rebuilt the same on a load).
    bool intercept_ = false;
    bool intercept_ready_ = false;
    InterceptData idata_;
    InterceptDesk desk_;
    AirRadioDef def_;
    std::vector<airwar::ListeningPost> posts_;
    std::size_t actions_read_ = 0;
    std::size_t c2_read_ = 0;
    std::set<int> abstract_seen_;
    std::vector<std::size_t> c2_pending_;                          // messages not yet past the watermark
    std::vector<std::pair<airwar::Heard, airwar::Call>> pending_;  // heard, not yet written up
    std::vector<airwar::Heard> heard_;
    std::vector<airwar::AbstractCall> calls_;
    double next_summary_h_ = -1.0;
    double summary_from_h_ = 0.0;
    int summaries_ = 0;
    // F2.5: the army's air defence staff's watch on the enemy's air front (a function of the campaign's history).
    bool watch_ready_ = false;
    bool watch_on_ = false;
    int watch_staff_ = -1;
    AirFrontWatch watch_;
    std::vector<FrontSignReport> signs_;  // every sign sent
    std::vector<char> taken_;             // the staff has taken signs_[i]
    std::size_t desk_read_ = 0;
    int troop_reports_ = 0;
    GroundFeed ground_;  // F3.8

    void ResetWatch() {
        watch_ready_ = watch_on_ = false;
        watch_staff_ = -1;
        watch_ = AirFrontWatch();
        signs_.clear();
        taken_.clear();
        desk_read_ = 0;
        troop_reports_ = 0;
    }

    // On where the air front runs (air_front.mode 1), the chain has the staff's post and the front its line; decided once.
    bool ReadyWatch(const airwar::AirCampaign& c) {
        if (watch_ready_) return watch_on_;
        watch_ready_ = true;
        watch_staff_ = net_->Data().PositionIndex(roles_.air_staff);
        watch_on_ = c.Data().tuning.Get("air_front.mode") >= 1.0 && watch_staff_ >= 0 && net_->Data().PositionIndex(roles_.air_staff_for) >= 0 &&
                    !c.FrontLine().Empty();
        if (watch_on_) watch_.Init(c.FrontLine().SectorCount(), AirFrontWatchRules::From(c.Data().tuning));
        return watch_on_;
    }

    static std::string SectorName(const airwar::AirCampaign& c, int sector) {
        const airwar::LineOfContact& line = c.FrontLine();
        if (sector < 0 || static_cast<std::size_t>(sector) >= line.SectorCount()) return "the front";
        const airwar::SectorDef& s = line.Sector(static_cast<std::size_t>(sector));
        return s.name.empty() ? s.id : s.name;
    }

    // A sign to the staff, when it lies deeper than its kind's usual reach by the margin (the reporting post judges by the
    // usual reach; the staff weighs it against its own estimate when it arrives). Our line of contact and where our side
    // saw it only: never the enemy's depths.
    bool SendSign(const airwar::AirCampaign& c, FrontSign sign, const Vec3& at, double h, const std::string& origin, const std::string& key,
                  const std::string& what) {
        const airwar::LineOfContact& line = c.FrontLine();
        const double depth = line.DistanceTo(at);
        if (depth <= watch_.Reach(sign) + watch_.Rules().margin_m) return false;
        FrontSignReport s;
        s.sign = sign;
        s.sector = line.SectorOf(at);
        s.depth_m = depth;
        s.seen_h = h;
        s.key = key;
        const std::string sector = SectorName(c, s.sector);
        char buf[192];
        std::snprintf(buf, sizeof(buf), ", %.0f km beyond the line of contact in %s: deeper than the enemy's %s usually work.", depth / 1000.0,
                      sector.c_str(), sign == FrontSign::Helicopters ? "helicopters" : "close support aircraft");
        Fact f;
        f.key = key;
        f.kind = sign == FrontSign::Intercept ? FactKind::Intercept : FactKind::RaidSeen;
        f.h = h;
        f.origin = origin;
        f.subject = "front_sign:" + line.Sector(static_cast<std::size_t>(s.sector)).id;
        f.text = FormatClock(h, c.Data().theatre.start_hour) + ": " + what + buf;
        f.audience = roles_.air_staff;
        f.need_level = 0;
        f.precedence = sign == FrontSign::Intercept ? 0 : 1;  // an attack that deep goes up at once; the intercept's by its paper
        net_->AddFact(f);
        signs_.push_back(s);
        taken_.push_back(0);
        return true;
    }

    // The warning net's raid: a sign when it attacked at low level our troops by day, or with helicopters.
    void WatchRaid(const airwar::AirCampaign& c, const airwar::ObservedRaid& r, std::size_t index) {
        if (!r.delivered || r.target_object < 0 || static_cast<std::size_t>(r.target_object) >= c.Data().theatre.objects.size()) return;
        const airwar::TheatreObject& o = c.Data().theatre.objects[static_cast<std::size_t>(r.target_object)];
        FrontSign sign = FrontSign::Helicopters;
        if (!r.helicopters) {
            if (!r.low || o.kind != airwar::ObjectKind::Troops || r.part == airwar::DayPart::Night) return;
            sign = FrontSign::Attack;
        }
        const std::string what =
            sign == FrontSign::Helicopters ? "helicopters attacked " + r.target : std::to_string(r.aircraft) + " aircraft at low level attacked " + r.target;
        SendSign(c, sign, r.target_pos, r.h, roles_.warning_net, "front_watch:raid:" + std::to_string(index), what);
    }

    // Our intercept desk's forms: a close support request on the enemy's request net naming a square (as the linguist wrote
    // it, slips and all) is a sign from the intercept centre once the form is written.
    void WatchDesk(const airwar::AirCampaign& c) {
        const auto& log = desk_.Log();
        for (; desk_read_ < log.size(); ++desk_read_) {
            const Logged& l = log[desk_read_];
            if (!l.understood || !l.has_place || l.kind != airwar::CallKind::Request) continue;
            SendSign(c, FrontSign::Intercept, l.place, l.written_s / 3600.0, roles_.intercept, "front_watch:intercept:" + std::to_string(desk_read_),
                     "the enemy's close support request net, heard at " + l.chain_post + ": a request for aircraft on square " + airwar::GridOf(l.place));
        }
    }

    // The staff takes the signs that have reached it, in the order they did, up to the watermark: the earliest hour a
    // package not yet resolved could still add a sign (so a campaign pumped step by step and one pumped once after its load
    // take the same signs in the same order). A conclusion is a fact from the staff after its working delay. True when one
    // was added.
    bool ReviewWatch(const airwar::AirCampaign& c) {
        double watermark_h = c.Now();
        for (const airwar::Package& p : c.Packages()) {
            if (!p.resolved) watermark_h = std::min(watermark_h, p.FirstH());
        }
        std::vector<std::size_t> ready;
        for (std::size_t i = 0; i < signs_.size(); ++i) {
            if (taken_[i] != 0) continue;
            FrontSignReport& s = signs_[i];
            if (s.known_h < 0.0) {
                const Delivery* d = net_->Known(watch_staff_, net_->FactIndex(s.key));
                if (d != nullptr) s.known_h = d->h;
            }
            if (s.known_h >= 0.0 && s.known_h < watermark_h) ready.push_back(i);
        }
        std::stable_sort(ready.begin(), ready.end(), [&](std::size_t a, std::size_t b) {
            const FrontSignReport& x = signs_[a];
            const FrontSignReport& y = signs_[b];
            if (x.known_h != y.known_h) return x.known_h < y.known_h;
            if (x.seen_h != y.seen_h) return x.seen_h < y.seen_h;
            return x.key < y.key;
        });
        bool added = false;
        for (std::size_t i : ready) {
            taken_[i] = 1;
            AirFrontConclusion k;
            if (!watch_.Take(signs_[i], k)) continue;
            AddConclusion(c, k, watch_.Conclusions().size() - 1);
            added = true;
        }
        return added;
    }

    void AddConclusion(const airwar::AirCampaign& c, const AirFrontConclusion& k, std::size_t index) {
        const std::string sector = SectorName(c, k.sector);
        const AirFrontWatchRules& r = watch_.Rules();
        const int first_day = static_cast<int>(std::floor((c.Data().theatre.start_hour + 24.0 * k.first_day) / 24.0)) + 1;
        char buf[512];
        std::snprintf(buf, sizeof(buf),
                      "The army's air defence staff: the enemy's air front has moved forward in %s. His close support aircraft now work "
                      "to about %.0f km beyond the line of contact and his helicopters to about %.0f km, %.0f km deeper than we "
                      "reckoned (%d attacks seen beyond the usual reach since D%d). Expect low-level attacks that deep there.",
                      sector.c_str(), (r.cas_reach_m + k.depth_m) / 1000.0, (r.helo_reach_m + k.depth_m) / 1000.0, (k.depth_m - k.from_m) / 1000.0,
                      k.signs, first_day);
        char said[256];
        std::snprintf(said, sizeof(said), "Army air defence staff: the enemy's close support works about %.0f km deeper in %s now (the paper follows).",
                      (k.depth_m - k.from_m) / 1000.0, sector.c_str());
        Fact f;
        f.key = "air_front_estimate:" + std::to_string(index + 1);
        f.kind = FactKind::AirFront;
        f.h = k.h;
        f.origin = roles_.air_staff;
        f.subject = "air_front:" + c.FrontLine().Sector(static_cast<std::size_t>(k.sector)).id;
        f.text = FormatClock(k.h, c.Data().theatre.start_hour) + ". " + buf;
        f.summary = said;
        f.audience = roles_.air_staff_for;
        f.need_level = 3;  // down to the battery commanders: the low-level threat to what they cover
        f.precedence = 1;
        f.document = true;
        net_->AddFact(f);
        watch_.SetKey(index, f.key);
    }

    void ResetIntercept() {
        intercept_ready_ = false;
        posts_.clear();
        actions_read_ = c2_read_ = 0;
        abstract_seen_.clear();
        c2_pending_.clear();
        pending_.clear();
        heard_.clear();
        calls_.clear();
        next_summary_h_ = -1.0;
        summary_from_h_ = 0.0;
        summaries_ = 0;
    }

    static double ClockStartS(const airwar::AirCampaign& c) { return c.Data().theatre.start_hour * 3600.0; }

    // The desk, the tuning and our posts at their chain places (theatre frame), once.
    void ReadyIntercept(const airwar::AirCampaign& c) {
        if (intercept_ready_) return;
        intercept_ready_ = true;
        const airwar::FactionRadio& fr = c.Data().faction.radio;
        desk_.Init(&idata_, c.Seed(), ClockStartS(c));
        def_ = AirRadioDef();
        def_.on = true;
        def_.faction = c.Data().faction.id;
        def_.freq_hz = fr.band_mhz * 1e6;
        def_.language = fr.language;
        def_.discipline = fr.discipline;
        for (const auto& kv : AirRadioTuningDefaults()) def_.tune[kv.first] = c.Data().tuning.Get(kv.first);
        const ChainData& d = net_->Data();
        for (const InterceptPostData& p : idata_.posts) {
            const Position* pos = d.FindPosition(p.position);
            const Place* pl = pos != nullptr ? d.FindPlace(pos->place) : nullptr;
            if (pl == nullptr) continue;
            for (int hf = 0; hf < 2; ++hf) {  // a VHF and UHF receiver and an HF one at each post, as in a session
                airwar::ListeningPost lp;
                lp.id = p.id + (hf != 0 ? "_hf" : "");
                lp.chain_post = p.id;
                lp.at = pl->pos;
                lp.mast_m = kPostMastM;
                lp.hf = hf != 0;
                posts_.push_back(lp);
                AirRadioPostDef pd;
                pd.id = lp.id;
                pd.name = pl->name;
                pd.at = lp.at;
                pd.mast_m = lp.mast_m;
                pd.chain_post = p.id;
                def_.posts.push_back(pd);
            }
        }
        // The first day summary at the section's hour (local; the campaign's hour 0 is the theatre's start hour).
        next_summary_h_ = idata_.daily_summary_hour - c.Data().theatre.start_hour;
        while (next_summary_h_ <= 0.0) next_summary_h_ += 24.0;
    }

    // The calls our posts logged in the played sessions (PackageOutcome::heard, kept in the campaign's Complete actions and
    // its save), made into the facts they made in the session: the fast road's to the division, a report naming the battery's
    // square to the regiment's chief of staff. A save made before S6 has no hours: the package's time over target stands in.
    void PumpHeard(const airwar::AirCampaign& c) {
        ReadyIntercept(c);
        const auto& actions = c.Actions();
        for (; actions_read_ < actions.size(); ++actions_read_) {
            const airwar::CampaignAction& a = actions[actions_read_];
            if (a.kind != airwar::CampaignAction::Kind::Complete) continue;
            double tot_h = a.end_h;
            for (const airwar::Package& p : c.Packages()) {
                if (p.id == a.outcome.package_id) tot_h = p.tot_h;
            }
            for (std::size_t ri = 0; ri < a.outcome.heard.size(); ++ri) {
                const airwar::HeardRecord& r = a.outcome.heard[ri];
                airwar::CallKind k = airwar::CallKind::Admin;
                if (!r.understood || !airwar::ParseCallKind(r.kind, k)) continue;
                Logged l;
                l.post = r.post;
                l.chain_post = r.chain_post.empty() ? r.post : r.chain_post;
                l.understood = true;
                l.text = r.text;
                l.kind = k;
                l.codeword = k == airwar::CallKind::Codeword;
                l.heard_s = (r.h >= 0.0 ? r.h : tot_h) * 3600.0;
                l.written_s = (r.written_h >= 0.0 ? r.written_h : (r.h >= 0.0 ? r.h : tot_h)) * 3600.0;
                l.bearing_deg = r.bearing_deg;
                l.precedence = r.precedence;
                l.names_battery = r.names_battery;
                l.fast = !r.fact.empty();
                // The call's number from the fact's key ("intercept:<call>:<post>"), else the record's place in the outcome.
                l.call = -1;
                if (r.fact.compare(0, 10, "intercept:") == 0) {
                    const std::size_t colon = r.fact.find(':', 10);
                    if (colon != std::string::npos) l.call = std::atoi(r.fact.substr(10, colon - 10).c_str());
                }
                if (l.call < 0) l.call = 100000 + static_cast<int>(ri);
                const std::string call_key = std::to_string(a.outcome.package_id) + ":" + std::to_string(l.call);
                for (Fact f : InterceptFacts(l, idata_, l.chain_post, ClockStartS(c))) {
                    // One telling per call: a second post's record of it has the same key.
                    f.key = (f.kind == FactKind::Intercept ? "intercept_played:" : "intercept_played_knows:") + call_key;
                    net_->AddFact(f);
                }
            }
        }
    }

    // The abstract packages' calls (section 2.4): made when a package is resolved, heard in closed form, and written up in
    // order of time once no package still unresolved can say anything earlier (the watermark), so a campaign pumped step by
    // step and one pumped once after its load write the same forms in the same order. The day's summary at its hour is a
    // document from army intelligence, the slow road (section 5.2); abstract calls take no fast road.
    void PumpAbstract(const airwar::AirCampaign& c) {
        ReadyIntercept(c);
        const airwar::FactionRadio& fr = c.Data().faction.radio;
        const std::uint64_t seed = static_cast<std::uint64_t>(c.Seed());
        const auto hear = [&](const airwar::AbstractCall& a) {
            calls_.push_back(a);
            for (const airwar::ListeningPost& lp : posts_) {
                airwar::Heard h;
                if (airwar::HearAbstract(a, lp, def_, seed, h)) pending_.push_back({h, a.call});
            }
        };
        double watermark_h = c.Now();
        for (const airwar::Package& p : c.Packages()) {
            if (!p.resolved) watermark_h = std::min(watermark_h, p.FirstH());
        }
        for (const airwar::Package& p : c.Packages()) {
            if (!p.resolved || p.played || !abstract_seen_.insert(p.id).second) continue;
            for (const airwar::AbstractCall& a : airwar::AbstractPackageCalls(p, fr, c.Staff(), def_, seed)) hear(a);
        }
        // The command's messages: decided once past the watermark (by then a played package is known as played).
        const airwar::AirC2& c2 = c.C2();
        const auto& log = c2.Log();
        for (; c2_read_ < log.size(); ++c2_read_) c2_pending_.push_back(c2_read_);
        std::vector<std::size_t> keep;
        for (std::size_t i : c2_pending_) {
            const airwar::C2Message& m = log[i];
            if (m.sent_h >= watermark_h) {
                keep.push_back(i);
                continue;
            }
            const airwar::Package* p = nullptr;
            for (const airwar::Package& q : c.Packages()) {
                if (q.id == m.package_id) p = &q;
            }
            if (p != nullptr && p->played) continue;  // the session's posts heard what was said about it
            airwar::AbstractCall a;
            if (airwar::AbstractC2Call(m, p, c2, fr, def_, a)) hear(a);
        }
        c2_pending_.swap(keep);
        // Written up in order of time, up to the watermark.
        using HeardCall = std::pair<airwar::Heard, airwar::Call>;
        std::stable_sort(pending_.begin(), pending_.end(), [](const HeardCall& x, const HeardCall& y) {
            if (x.first.start_s != y.first.start_s) return x.first.start_s < y.first.start_s;
            if (x.first.call != y.first.call) return x.first.call < y.first.call;
            return x.first.post < y.first.post;
        });
        const airwar::TheatreObject& battery = c.Data().theatre.objects[static_cast<std::size_t>(c.PlayerIndex())];
        std::size_t done = 0;
        for (; done < pending_.size() && pending_[done].first.start_s < watermark_h * 3600.0; ++done) {
            const double h = pending_[done].first.start_s / 3600.0;
            while (next_summary_h_ <= h) Summary();  // the day's summary is cut at its hour: the forms before it are in it
            Vec3 at = battery.pos;  // the battery where it stood then: a square this near is the battery's
            for (const auto& t : c.BatteryTrack()) {
                if (t.first <= h) at = t.second;
            }
            desk_.ClearWatch();
            desk_.Watch(at);
            desk_.WriteUpCall(def_, pending_[done].first, pending_[done].second);
            heard_.push_back(pending_[done].first);
        }
        pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(done));
        while (next_summary_h_ <= watermark_h) Summary();
    }

    void Summary() {
        const double from_h = summary_from_h_, to_h = next_summary_h_;
        summary_from_h_ = next_summary_h_;
        next_summary_h_ += 24.0;
        int n = 0;
        for (const Logged& l : desk_.Log()) n += (l.heard_s >= from_h * 3600.0 && l.heard_s < to_h * 3600.0) ? 1 : 0;
        if (n == 0) return;  // nothing heard: no paper
        const std::string text = desk_.DailySummary(from_h * 3600.0, to_h * 3600.0, def_.Tune("radio.continuity_name_at"), def_, heard_);
        Fact f = SummaryFact(text, ++summaries_, to_h * 3600.0);
        f.key = "intercept_intsum_campaign:" + std::to_string(summaries_);
        f.audience = regiment_;
        net_->AddFact(f);
    }
    static constexpr double kPostMastM = 30.0;  // sam3d: the posts' antennas, as in the mission session (docs/air_battle/09 section 4.2)

    static bool Starts(const std::string& s, const char* p) { return s.compare(0, std::char_traits<char>::length(p), p) == 0; }
    static bool Has(const std::string& s, const char* p) { return s.find(p) != std::string::npos; }

    bool FromJournal(const airwar::AirCampaign& c, const airwar::JournalEntry& j, Fact& f) const {
        if (j.kind == "player_mission" && j.who == "warning") {
            f.kind = FactKind::AirWarning;
            f.origin = roles_.sector;
            f.audience.clear();
            f.need_level = 5;
            f.precedence = 2;
            f.text = "'Воздух!' " + j.text;
            const airwar::Package* p = c.PlayerMission();
            f.expires_h = (p != nullptr ? p->tot_h : j.h + 0.5) + 0.5;
            if (p != nullptr) f.subject = "package:" + std::to_string(p->id);
            return true;
        }
        if (j.kind == "player_mission" && j.who == "result") {
            f.kind = FactKind::RaidResult;
            f.origin = roles_.battery;
            f.witnesses = battery_people_;
            f.need_level = 2;
            f.precedence = 1;
            return true;
        }
        if (j.kind != "own_intel") return false;
        if (j.who == "intelligence" && Starts(j.text, "War with")) {
            f.kind = FactKind::War;
            f.origin = roles_.announcer;
            f.audience.clear();
            f.need_level = 5;
            f.precedence = 1;
            f.broadcast = true;
            return true;
        }
        if (j.who == "intelligence") {
            f.kind = FactKind::EnemyKnowsBattery;
            f.origin = roles_.staff;
            f.need_level = 3;
            f.precedence = Has(j.text, "consider moving") ? 1 : 0;
            f.text = "The regiment's estimate: " + j.text;
            return true;
        }
        if (j.who == "regiment") {  // the regiment's order to its batteries (the ARM drill once ARMs have been fired at us)
            f.kind = FactKind::Order;
            f.origin = roles_.staff;
            f.need_level = 4;  // down to the vehicle commanders, who put their crews to it
            f.precedence = 1;
            f.summary = "The regiment's order: the batteries to the ARM drill, the enemy fires anti-radiation missiles at our radars.";
            return true;
        }
        if (j.who == "battery") {
            f.kind = FactKind::BatteryNews;
            f.origin = roles_.battery;
            f.witnesses = battery_people_;
            f.need_level = 2;
            return true;
        }
        if (j.who == "interrogation" && Starts(j.text, "We hold")) {
            f.kind = FactKind::Prisoner;
            f.origin = roles_.police;
            f.need_level = 3;
            f.precedence = 0;
            f.summary = "An enemy airman was picked up in the district; the army has him now.";
            return true;
        }
        if (j.who == "interrogation") {
            f.kind = FactKind::EnemyOfficer;
            f.origin = roles_.div_intel;
            f.need_level = 3;
            f.document = true;
            f.summary = "Division intelligence: a prisoner has talked about the enemy's air staff (the paper follows).";
            return true;
        }
        if (j.who == "intercepts" || j.who == "analysts") {
            f.kind = FactKind::EnemyOfficer;
            f.origin = roles_.army_intel;
            f.need_level = 3;
            f.document = true;
            f.summary = j.who == "intercepts" ? "Army intelligence: the intercepts show a change in the enemy's air staff."
                                              : "Army intelligence has a new note on the officer planning the raids (the paper follows).";
            return true;
        }
        if (j.who == "press") {
            f.kind = FactKind::Press;
            f.origin = roles_.political;
            f.audience.clear();
            f.need_level = 5;
            f.broadcast = true;
            return true;
        }
        return false;  // "warning net": the observed raids carry it
    }
};

// The enemy's listeners on the campaign's radio (roadmap S6, docs/air_battle/09 section 6): the campaign screen's EmRadio
// hears our nets only at the receivers the data places for the campaign's enemy (PlaceEnemyListeners). At radio.mode 0 none
// is placed and the enemy hears everything, as before S6. Returns the number placed.
inline int PlaceCampaignListeners(EmRadio& radio, const InterceptData& data, const airwar::AirCampaign& c) {
    if (c.Data().tuning.Get("radio.mode") < 1.0) return 0;
    return PlaceEnemyListeners(radio, data, c.Data().faction.id);
}

}  // namespace command
}  // namespace simcore
