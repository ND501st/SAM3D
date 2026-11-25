// simcore/campaign/briefings.hpp — the orders as people give them (track O, O3; docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md
// section 4): the regiment's evening briefing at its command post, the battery commander's oral combat order and the
// vehicle commander's briefing of his crew, each a scene of spoken lines in its items' order; and the state radio's
// morning news.
//
// This file holds the data (data/campaign/briefings.json) and the lines' shape. Who goes where and when, and what a line
// changes, are the mission session's (campaign/mission_session.hpp: the battery commander's drive to the regiment's
// command post and back, the task traced at the briefing, the papers handed there, the news) and the runner's (the
// combat order given at the map table, on the ground or by radio; the crew's briefing at the vehicle).
//
// The items' order and the papers come from docs/campaign/12 section 4 (voenizdat_osa_1991 pp. 224-258 for the battery
// commander's sequence and the crew's work on arrival); what R0 could not confirm is labelled a game value in the data.
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace campaign {

// One item of a briefing or order: who says it (a position of the chain at the regiment's, else the giver) and its
// words with {fields}.
struct BriefingItem {
    std::string id;
    std::string by;
    std::string en;
};

// A paper handed out at the regiment's briefing (D5: the code table comes so, never by radio).
struct BriefingPaper {
    std::string id;   // "code_table", "cooperation_plan", "call_signs"
    std::string title;
    std::string by;   // the position who hands it over
};

struct RegimentBriefingDef {
    std::string place = "rgt_cp";       // the chain's place (an anchor of the session's roads)
    double hour = 19.0;                 // local
    std::string car_mobility = "truck";  // the battery commander's car on the roads (its class's speeds)
    std::string car_model = "uaz469";
    double leave_margin_s = 600.0;      // he means to be there this early
    double wait_s = 1800.0;             // the briefing waits no longer for a late battery commander
    double line_s = 7.0;                // a line's time in the scene
    std::vector<BriefingItem> items;
    std::vector<BriefingPaper> papers;
};

struct BatteryOrderDef {
    double urgent_s = 1800.0;   // less than this to the ready-by: the order goes by radio in short form
    double line_s = 6.0;
    double gather_m = 30.0;     // the vehicle commanders stand this far from the giver (the map table, the spot)
    std::vector<BriefingItem> items;
    std::string radio_en;       // the short form said on the battery net
};

struct CrewBriefingDef {
    double line_s = 5.0;
    std::vector<BriefingItem> items;
};

struct NewsDef {
    std::vector<double> at_local_h{7.0};
    std::string from = "the state radio";
    std::string opening;
    std::string quiet;  // a bulletin with nothing of the war in it
};

struct BriefingsData {
    bool present = false;
    RegimentBriefingDef regiment;
    BatteryOrderDef battery;
    CrewBriefingDef crew;
    NewsDef news;
};

// A line spoken in a scene: who, which item, the words, and when (the session's or the runner's seconds).
struct SceneLine {
    std::string scene;    // "regiment_briefing", "battery_order", "crew_briefing"
    double at_s = 0.0;
    std::string speaker;  // as the listener knows him: "Lt Col Josef Vrana, the regiment commander"
    std::string item;
    std::string text;
};

// How the battery commander gives his oral combat order (D1).
enum class OrderWay { Default, MapTable, Ground, Radio };

inline const char* OrderWayName(OrderWay w) {
    switch (w) {
        case OrderWay::Default: return "default";
        case OrderWay::MapTable: return "at the PU-12M's map table";
        case OrderWay::Ground: return "on the ground";
        case OrderWay::Radio: return "by radio in short form";
    }
    return "?";
}
inline bool InPerson(OrderWay w) { return w == OrderWay::MapTable || w == OrderWay::Ground; }

// {field} replaced by its value; a field the map does not have is left as written.
inline std::string FillLine(std::string text, const std::map<std::string, std::string>& fields) {
    for (const auto& f : fields) {
        const std::string key = "{" + f.first + "}";
        std::size_t at = 0;
        while ((at = text.find(key, at)) != std::string::npos) {
            text.replace(at, key.size(), f.second);
            at += f.second.size();
        }
    }
    return text;
}

namespace detail {

class BriefingsLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, BriefingsData& out) {
        if (!Keys(root, "", {"briefings_id", "status", "notes", "source", "confidence", "regiment", "battery_order", "crew_briefing", "news"}) ||
            !Provenance(root, "")) {
            return false;
        }
        const json::Value* r = root.Find("regiment");
        const json::Value* b = root.Find("battery_order");
        const json::Value* c = root.Find("crew_briefing");
        const json::Value* n = root.Find("news");
        if (r == nullptr) return Fail("regiment", "required");
        if (b == nullptr) return Fail("battery_order", "required");
        if (c == nullptr) return Fail("crew_briefing", "required");
        if (n == nullptr) return Fail("news", "required");
        RegimentBriefingDef& rg = out.regiment;
        if (!Keys(*r, "regiment", {"place", "hour", "car_mobility", "car_model", "leave_margin_s", "wait_s", "line_s", "items", "papers", "notes"}) ||
            !RequireString(*r, "regiment", "place", rg.place) || !Value(*r, "regiment", "hour", rg.hour) ||
            !RequireString(*r, "regiment", "car_mobility", rg.car_mobility) || !RequireString(*r, "regiment", "car_model", rg.car_model) ||
            !Value(*r, "regiment", "leave_margin_s", rg.leave_margin_s) || !Value(*r, "regiment", "wait_s", rg.wait_s) ||
            !Value(*r, "regiment", "line_s", rg.line_s) || !Items(*r, "regiment", true, rg.items)) {
            return false;
        }
        if (const json::Value* ps = OptionalArray(*r, "regiment", "papers")) {
            for (std::size_t i = 0; i < ps->items.size(); ++i) {
                const std::string p = Index("regiment.papers", i);
                const json::Value& x = ps->items[i];
                BriefingPaper paper;
                if (!Keys(x, p, {"id", "title", "by", "source", "confidence", "notes"}) || !Provenance(x, p) || !RequireString(x, p, "id", paper.id) ||
                    !RequireString(x, p, "title", paper.title) || !RequireString(x, p, "by", paper.by)) {
                    return false;
                }
                rg.papers.push_back(paper);
            }
        } else if (!Message().empty()) {
            return false;
        }
        BatteryOrderDef& bo = out.battery;
        if (!Keys(*b, "battery_order", {"urgent_s", "line_s", "gather_m", "items", "radio_en", "radio_source", "notes"}) ||
            !Value(*b, "battery_order", "urgent_s", bo.urgent_s) || !Value(*b, "battery_order", "line_s", bo.line_s) ||
            !Value(*b, "battery_order", "gather_m", bo.gather_m) || !Items(*b, "battery_order", false, bo.items) ||
            !RequireString(*b, "battery_order", "radio_en", bo.radio_en)) {
            return false;
        }
        if (!Keys(*c, "crew_briefing", {"line_s", "items", "notes"}) || !Value(*c, "crew_briefing", "line_s", out.crew.line_s) ||
            !Items(*c, "crew_briefing", false, out.crew.items)) {
            return false;
        }
        NewsDef& nw = out.news;
        if (!Keys(*n, "news", {"at_local_h", "from", "opening", "quiet", "source", "confidence", "notes"}) || !Provenance(*n, "news") ||
            !RequireString(*n, "news", "from", nw.from) || !RequireString(*n, "news", "opening", nw.opening) || !RequireString(*n, "news", "quiet", nw.quiet)) {
            return false;
        }
        if (const json::Value* hs = OptionalArray(*n, "news", "at_local_h")) {
            nw.at_local_h.clear();
            for (const json::Value& h : hs->items) {
                if (!h.Is(json::Type::Number) || h.number < 0.0 || h.number >= 24.0) return Fail("news.at_local_h", "hours 0-24");
                nw.at_local_h.push_back(h.number);
            }
        } else if (!Message().empty()) {
            return false;
        }
        out.present = true;
        return true;
    }
    using ConsoleLoader::Fail;
    using ConsoleLoader::Message;

private:
    // A game value or a sourced number: {"value", "source", "confidence", "notes"}.
    bool Value(const json::Value& obj, const std::string& path, const char* key, double& out) {
        const std::string p = Join(path, key);
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return Fail(p, "required");
        if (!Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) return false;
        const json::Value* n = v->Find("value");
        if (n == nullptr || !n->Is(json::Type::Number) || !(n->number >= 0.0)) return Fail(Join(p, "value"), "a number >= 0");
        out = n->number;
        return true;
    }
    bool Items(const json::Value& obj, const std::string& path, bool need_by, std::vector<BriefingItem>& out) {
        const json::Value* arr = OptionalArray(obj, path, "items");
        if (arr == nullptr) return Fail(Join(path, "items"), "required");
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string p = Index(Join(path, "items"), i);
            const json::Value& x = arr->items[i];
            BriefingItem it;
            if (!Keys(x, p, {"id", "by", "en", "source", "confidence", "notes"}) || !Provenance(x, p) || !RequireString(x, p, "id", it.id) ||
                !RequireString(x, p, "en", it.en) || !OptionalString(x, p, "by", it.by)) {
                return false;
            }
            if (need_by && it.by.empty()) return Fail(Join(p, "by"), "required (a position of the chain)");
            out.push_back(it);
        }
        if (out.empty()) return Fail(Join(path, "items"), "at least one");
        return true;
    }
};

}  // namespace detail

inline bool LoadBriefings(const std::string& data_dir, const std::string& rel, BriefingsData& out, std::string* error = nullptr) {
    std::string text;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text)) {
        if (error != nullptr) *error = "briefings: cannot read " + rel;
        return false;
    }
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "briefings: " + rel + ": " + e;
        return false;
    }
    detail::BriefingsLoader loader;
    if (!loader.Load(root, out)) {
        if (error != nullptr) *error = "briefings: " + rel + ": " + loader.Message();
        return false;
    }
    return true;
}

}  // namespace campaign
}  // namespace simcore
