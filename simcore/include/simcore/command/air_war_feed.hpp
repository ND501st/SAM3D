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
#include <set>
#include <string>
#include <vector>

#include "simcore/air_war/campaign.hpp"
#include "simcore/command/info_net.hpp"

namespace simcore {
namespace command {

// Who first knows each kind of news, and the air war's effects on the pipeline (game values, docs/command/06).
struct FeedRoles {
    std::string sector = "sector_cp_duty";            // raid warnings (the national sector sees them coming)
    std::string warning_net = "div_ad_cp_duty";       // the raids the warning net saw
    std::string army_intel = "army_intel";            // intercepts, the analysts
    std::string div_intel = "div_intel";              // prisoners' interrogations
    std::string police = "kesmark_police";            // airmen picked up in the district
    std::string staff = "rgt_chief_of_staff";         // the regiment's estimate of what the enemy knows of the battery
    std::string battery = "bty21_commander";          // the battery's own news and fights
    std::string announcer = "army_ad_chief";          // the war itself
    std::string political = "rgt_political_deputy";   // the press
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
        net_->AdvanceTo(c.Now());
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

}  // namespace command
}  // namespace simcore
