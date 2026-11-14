// simcore/command/player_inputs.hpp — the player's own inputs to the pipeline, kept with the campaign (roadmap P2).
//
// The air war's news is fed into the pipeline again from the campaign when a save is loaded (air_war_feed.hpp); what the
// player's character did with it cannot be: the reports and orders he wrote, and whom he told what, by what means and
// when. Nor can what the crews called in the fights he played (session_reports.hpp, keys "crew:..."). They are saved
// beside the campaign and put back after its news, so the pipeline plays out as it did. So are the posts that changed
// hands and the player's career (roadmap P8).
#pragma once

#include <string>
#include <vector>

#include "simcore/air_war/raid_scenario.hpp"
#include "simcore/command/info_net.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace command {

inline bool ParseFactKind(const std::string& s, FactKind& out) {
    for (FactKind k : {FactKind::War, FactKind::AirWarning, FactKind::Order, FactKind::Intsum, FactKind::RaidSeen, FactKind::RaidResult,
                       FactKind::OwnDamage, FactKind::Prisoner, FactKind::EnemyOfficer, FactKind::EnemyKnowsBattery, FactKind::BatteryNews,
                       FactKind::Press, FactKind::Rumour, FactKind::Mail}) {
        if (s == FactKindName(k)) {
            out = k;
            return true;
        }
    }
    return false;
}

inline std::string SavePlayerInputs(const InfoNet& net) {
    using namespace airwar::jsonb;
    const ChainData& data = net.Data();
    json::Value root = Obj();
    Put(root, "save_version", Num(1));
    json::Value reports = Arr();
    for (const Fact& f : net.Facts()) {
        if (!f.held && f.key.compare(0, 5, "crew:") != 0) continue;
        json::Value r = Obj();
        Put(r, "key", Str(f.key));
        Put(r, "kind", Str(FactKindName(f.kind)));
        Put(r, "h", Num(f.h));
        Put(r, "origin", Str(f.origin));
        Put(r, "text", Str(f.text));
        Put(r, "summary", Str(f.summary));
        Put(r, "subject", Str(f.subject));
        Put(r, "audience", Str(f.audience));
        Put(r, "need_level", Num(f.need_level));
        Put(r, "precedence", Num(f.precedence));
        Put(r, "document", Bool(f.document));
        Put(r, "held", Bool(f.held));
        if (!f.witnesses.empty()) {
            json::Value w = Arr();
            for (const std::string& s : f.witnesses) Push(w, Str(s));
            Put(r, "witnesses", w);
        }
        Push(reports, r);
    }
    Put(root, "reports", reports);
    json::Value tells = Arr();
    for (const Telling& t : net.Tellings()) {
        json::Value j = Obj();
        Put(j, "h", Num(t.h));
        Put(j, "from", Str(data.positions[static_cast<std::size_t>(t.from)].id));
        Put(j, "to", Str(data.positions[static_cast<std::size_t>(t.to)].id));
        Put(j, "fact", Str(net.Facts()[static_cast<std::size_t>(t.fact)].key));
        Put(j, "mean", Str(t.mean));
        Push(tells, j);
    }
    Put(root, "tellings", tells);
    json::Value changes = Arr();
    for (const PostChange& c : net.Changes()) {
        json::Value j = Obj();
        Put(j, "position", Str(c.position));
        Put(j, "h", Num(c.h));
        Put(j, "vacant", Bool(c.vacant));
        if (!c.holder.name.empty()) {
            json::Value who = Obj();
            Put(who, "name", Str(c.holder.name));
            Put(who, "rank", Str(c.holder.rank));
            Put(who, "skill", Str(c.holder.skill));
            Put(j, "holder", who);
        }
        Put(j, "place", Str(c.place));
        Put(j, "why", Str(c.why));
        Push(changes, j);
    }
    Put(root, "post_changes", changes);
    json::Value career = Arr();
    for (const std::pair<double, int>& c : net.Career()) {
        json::Value j = Obj();
        if (c.first > -1e17) Put(j, "h", Num(c.first));
        Put(j, "post", Str(c.second >= 0 ? data.positions[static_cast<std::size_t>(c.second)].id : std::string()));
        Push(career, j);
    }
    Put(root, "career", career);
    return Dump(root, 2, 17);
}

// Put the player's inputs back, after the campaign's news has been fed in. A telling that no longer can be (the chain's
// data changed) is left out and named in `error`; false only if the text does not read.
inline bool LoadPlayerInputs(InfoNet& net, const std::string& text, std::string* error = nullptr) {
    json::Value root;
    if (!json::Parse(text, root, error) || !root.Is(json::Type::Object)) return false;
    auto str = [](const json::Value& o, const char* k) {
        const json::Value* v = o.Find(k);
        return v != nullptr && v->Is(json::Type::String) ? v->string : std::string();
    };
    auto num = [](const json::Value& o, const char* k, double dflt) {
        const json::Value* v = o.Find(k);
        return v != nullptr && v->Is(json::Type::Number) ? v->number : dflt;
    };
    std::string skipped;
    std::vector<PostChange> changes;
    if (const json::Value* list = root.Find("post_changes")) {
        for (const json::Value& j : list->items) {
            PostChange c;
            c.position = str(j, "position");
            c.h = num(j, "h", 0.0);
            const json::Value* vacant = j.Find("vacant");
            c.vacant = vacant != nullptr && vacant->boolean;
            if (const json::Value* who = j.Find("holder")) {
                c.holder.name = str(*who, "name");
                c.holder.rank = str(*who, "rank");
                c.holder.skill = str(*who, "skill");
            }
            c.place = str(j, "place");
            c.why = str(j, "why");
            if (net.Data().PositionIndex(c.position) < 0 || (!c.place.empty() && net.Data().PlaceIndex(c.place) < 0)) {
                skipped += " change of '" + c.position + "'";
                continue;
            }
            changes.push_back(c);
        }
    }
    std::vector<std::pair<double, int>> career;
    if (const json::Value* list = root.Find("career")) {
        for (const json::Value& j : list->items) career.push_back({num(j, "h", -1e18), net.Data().PositionIndex(str(j, "post"))});
    }
    if (!changes.empty() || !career.empty()) net.RestorePosts(changes, career);
    if (const json::Value* reports = root.Find("reports")) {
        for (const json::Value& r : reports->items) {
            Fact f;
            f.key = str(r, "key");
            if (f.key.empty() || !ParseFactKind(str(r, "kind"), f.kind)) {
                skipped += " report '" + f.key + "'";
                continue;
            }
            f.h = num(r, "h", 0.0);
            f.origin = str(r, "origin");
            f.text = str(r, "text");
            f.summary = str(r, "summary");
            f.subject = str(r, "subject");
            f.audience = str(r, "audience");
            f.need_level = static_cast<int>(num(r, "need_level", 3.0));
            f.precedence = static_cast<int>(num(r, "precedence", 0.0));
            const json::Value* doc = r.Find("document");
            f.document = doc != nullptr && doc->boolean;
            const json::Value* held = r.Find("held");
            f.held = held == nullptr || held->boolean;
            if (const json::Value* w = r.Find("witnesses")) {
                for (const json::Value& s : w->items) {
                    if (s.Is(json::Type::String)) f.witnesses.push_back(s.string);
                }
            }
            net.AddFact(f);
        }
    }
    net.AdvanceTo(net.Now());
    if (const json::Value* tells = root.Find("tellings")) {
        for (const json::Value& j : tells->items) {
            Telling t;
            t.h = num(j, "h", 0.0);
            t.from = net.Data().PositionIndex(str(j, "from"));
            t.to = net.Data().PositionIndex(str(j, "to"));
            t.fact = net.FactIndex(str(j, "fact"));
            t.mean = str(j, "mean");
            std::string why;
            if (!net.Tell(t, &why)) skipped += " telling of '" + str(j, "fact") + "' to " + str(j, "to") + " (" + why + ")";
        }
    }
    if (!skipped.empty() && error != nullptr) *error = "left out:" + skipped;
    return true;
}

}  // namespace command
}  // namespace simcore
