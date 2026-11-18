// simcore/talk_tables.hpp — the talk tables ПТ-1 and ПТ-2 and the communications' game values
// (data/command/talk_tables.json, data/gameplay/comms.json; docs/crew/12_COMMUNICATIONS.md §2.9).
//
// Orders and signals on the voice nets go as three-digit groups of a talk table (переговорная таблица, «ПТ-<n>»:
// fsrbit_pt, bstudy_tactics); air data, designations and warnings may go in clear (pvo_bu_x_1984 art. 30). An NPC who hears
// a group decodes it with the table he keeps; a player reads the document on his desk. The groups are placeholders.
#pragma once

#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/json.hpp"
#include "simcore/pu12.hpp"
#include "simcore/radio_nets.hpp"

namespace simcore {

struct TalkEntry {
    std::string code;     // "143"
    std::string ru, en;   // what it means
    std::string effect;   // "readiness", "fire_ban", "fire_free", "cease_fire", "silence_on", "silence_off",
                          // "spare_frequency", "report_readiness", "air_warning", "all_clear", "emission_free",
                          // "emission_limited", "emission_arm", "emission_silent" (emission_regime.hpp), or "" (said only)
    int aspd = -1;        // the ASPD-U's two-digit command of the same meaning (-1: none)
    std::string visual;   // a signal table's sign seen instead of heard ("зелёная ракета"), "" if none
};
struct TalkTable {
    std::string id;       // "pt1"
    std::string name_ru;  // "ПТ-1"
    std::string name_en;
    std::vector<TalkEntry> entries;
};
struct TalkTables {
    std::vector<TalkTable> tables;
    const TalkTable* Table(const std::string& id) const {
        for (const TalkTable& t : tables) {
            if (t.id == id) return &t;
        }
        return nullptr;
    }
    // The entry with this group in any table (and the table it is in).
    const TalkEntry* Decode(const std::string& code, const TalkTable** in = nullptr) const {
        for (const TalkTable& t : tables) {
            for (const TalkEntry& e : t.entries) {
                if (e.code == code) {
                    if (in != nullptr) *in = &t;
                    return &e;
                }
            }
        }
        return nullptr;
    }
    // The group for an effect (in table `id`, or any), or for an ASPD-U command.
    const TalkEntry* ByEffect(const std::string& effect, const std::string& id = std::string()) const {
        for (const TalkTable& t : tables) {
            if (!id.empty() && t.id != id) continue;
            for (const TalkEntry& e : t.entries) {
                if (e.effect == effect) return &e;
            }
        }
        return nullptr;
    }
    const TalkEntry* ByAspd(int aspd) const {
        for (const TalkTable& t : tables) {
            for (const TalkEntry& e : t.entries) {
                if (e.aspd == aspd && aspd >= 0) return &e;
            }
        }
        return nullptr;
    }
};

namespace detail {

class TalkTablesLoader : public BoardLoader {
public:
    bool Load(const json::Value& root, TalkTables& out) {
        if (!Keys(root, "", {"tables_id", "display_name", "status", "notes", "source", "confidence", "tables"}) || !Provenance(root, "")) {
            return false;
        }
        const json::Value* list = root.Find("tables");
        if (list == nullptr || !list->Is(json::Type::Array)) return Fail("tables", "required, a list");
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const std::string p = "tables[" + std::to_string(i) + "]";
            const json::Value& e = list->items[i];
            TalkTable t;
            if (!Keys(e, p, {"id", "name_ru", "name_en", "entries", "notes"}) || !RequireString(e, p, "id", t.id) ||
                !RequireString(e, p, "name_ru", t.name_ru) || !RequireString(e, p, "name_en", t.name_en)) {
                return false;
            }
            const json::Value* entries = e.Find("entries");
            if (entries == nullptr || !entries->Is(json::Type::Array)) return Fail(Join(p, "entries"), "required, a list");
            for (std::size_t k = 0; k < entries->items.size(); ++k) {
                const std::string ep = Index(Join(p, "entries"), k);
                const json::Value& x = entries->items[k];
                TalkEntry te;
                double aspd = -1.0;
                if (!Keys(x, ep, {"code", "ru", "en", "effect", "aspd", "visual", "notes"}) || !RequireString(x, ep, "code", te.code) ||
                    !RequireString(x, ep, "ru", te.ru) || !RequireString(x, ep, "en", te.en) || !OptionalString(x, ep, "effect", te.effect) ||
                    !OptionalNumber(x, ep, "aspd", aspd) || !OptionalString(x, ep, "visual", te.visual)) {
                    return false;
                }
                te.aspd = static_cast<int>(aspd);
                for (const TalkTable& o : out.tables) {
                    for (const TalkEntry& oe : o.entries) {
                        if (oe.code == te.code) return Fail(Join(ep, "code"), "group " + te.code + " is in " + o.name_ru + " already");
                    }
                }
                for (const TalkEntry& oe : t.entries) {
                    if (oe.code == te.code) return Fail(Join(ep, "code"), "duplicate group " + te.code);
                }
                t.entries.push_back(std::move(te));
            }
            out.tables.push_back(std::move(t));
        }
        return true;
    }
};

class CommsTuningLoader : public BoardLoader {
public:
    bool Load(const json::Value& root, CommsTuning& t) {
        if (!Keys(root, "", {"comms_id", "display_name", "status", "notes", "headset_s", "box_select_s", "answer_line_s", "recognise_jamming_s",
                             "order_spare_s", "weak_jam_loss", "call_wait_s", "both_frequencies_s", "loss_report_s", "df_window_s", "df_fix_s",
                             "df_reaction_s", "warn_covered_m", "notice_board_s", "plot_lost_s", "plot_turn_deg",
                             "plot_stale_s"})) {
            return false;
        }
        cite_path_ = "comms";
        for (const auto& f : {std::make_pair("headset_s", &t.headset_s), std::make_pair("box_select_s", &t.box_select_s),
                              std::make_pair("answer_line_s", &t.answer_line_s), std::make_pair("recognise_jamming_s", &t.recognise_jamming_s),
                              std::make_pair("order_spare_s", &t.order_spare_s), std::make_pair("weak_jam_loss", &t.weak_jam_loss),
                              std::make_pair("call_wait_s", &t.call_wait_s), std::make_pair("both_frequencies_s", &t.both_frequencies_s),
                              std::make_pair("loss_report_s", &t.loss_report_s), std::make_pair("df_window_s", &t.df_window_s),
                              std::make_pair("df_fix_s", &t.df_fix_s), std::make_pair("df_reaction_s", &t.df_reaction_s),
                              std::make_pair("warn_covered_m", &t.warn_covered_m),
                              std::make_pair("notice_board_s", &t.notice_board_s), std::make_pair("plot_lost_s", &t.plot_lost_s),
                              std::make_pair("plot_turn_deg", &t.plot_turn_deg), std::make_pair("plot_stale_s", &t.plot_stale_s)}) {
            if (!Cited(root, f.first, *f.second)) return false;
        }
        if (t.weak_jam_loss > 1.0) return Fail("comms.weak_jam_loss", "a chance, 0-1");
        if (t.both_frequencies_s < t.call_wait_s || t.loss_report_s < t.both_frequencies_s) {
            return Fail("comms.loss_report_s", "the clock runs call, both frequencies, report");
        }
        return true;
    }
};

template <typename T, typename Loader>
bool LoadJsonFile(const std::string& path, T& out, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text.str(), root, &parse_error)) {
        if (error != nullptr) *error = path + ": " + parse_error;
        return false;
    }
    T t{};
    Loader loader;
    if (!loader.Load(root, t)) {
        if (error != nullptr) *error = path + ": " + loader.Message();
        return false;
    }
    out = std::move(t);
    return true;
}

}  // namespace detail

inline bool LoadTalkTablesFile(const std::string& path, TalkTables& out, std::string* error = nullptr) {
    return detail::LoadJsonFile<TalkTables, detail::TalkTablesLoader>(path, out, error);
}
inline bool LoadCommsTuningFile(const std::string& path, CommsTuning& out, std::string* error = nullptr) {
    return detail::LoadJsonFile<CommsTuning, detail::CommsTuningLoader>(path, out, error);
}

}  // namespace simcore
