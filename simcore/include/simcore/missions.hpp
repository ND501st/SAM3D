// simcore/missions.hpp — the missions a player chooses from (data/scenarios/missions.json; roadmap M2: a person plays
// the whole mission without touching the editor).
//
// The catalogue only orders and groups the scenarios and gives each a short title. Everything else comes from the
// scenario itself: its name and description, its vehicle's seats (the console's stations) and the seat that commands
// (the crew data's commander_seat). Taking that seat is the game's "in command" half; any other seat is "in the crew".
// A battery link (simcore/battery_link.hpp) lists the vehicles the player may start in, each with its own seats.
// Loading checks every scenario as the game would load it, so a mission in the menu always starts.
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "simcore/battery_link.hpp"
#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/json.hpp"
#include "simcore/scenario.hpp"

namespace simcore {

struct MissionSeat {
    std::string id;     // a station of the vehicle's console
    std::string label;
    bool commands = false;  // the crew data's commander_seat: whoever sits here gives the orders
};

// A vehicle the player can start in. A plain scenario has one (id ""); a battery link has "osa" and "post".
struct MissionVehicle {
    std::string id;
    std::string label;          // the catalogue's, or the console's display name
    std::string scenario_file;  // relative to the data folder
    std::string system_id;
    std::vector<MissionSeat> seats;

    const MissionSeat* Commander() const {
        for (const MissionSeat& s : seats) {
            if (s.commands) return &s;
        }
        return nullptr;
    }
};

struct MissionDef {
    std::string id;           // the file's base name, e.g. "osa_crew_practice"
    std::string file;         // relative to the data folder: "scenarios/osa_crew_practice.json"
    std::string title;        // short, for the list
    std::string group;        // its group's id
    std::string name;         // the scenario's (the link's) name
    std::string description;  // and description
    bool link = false;        // a battery link: the player chooses the vehicle
    std::string level;        // the game level it plays in ("" the menu's own; "/Game/Maps/KesmarkRange" for the map)
    std::vector<MissionVehicle> vehicles;

    const MissionVehicle* Vehicle(const std::string& vehicle_id) const {
        for (const MissionVehicle& v : vehicles) {
            if (v.id == vehicle_id) return &v;
        }
        return nullptr;
    }
};

struct MissionGroup {
    std::string id;
    std::string label;
    std::string notes;  // what playing one of them is like, for the menu
    std::vector<MissionDef> missions;
};

struct MissionCatalog {
    std::vector<MissionGroup> groups;

    const MissionDef* Find(const std::string& mission_id) const {
        for (const MissionGroup& g : groups) {
            for (const MissionDef& m : g.missions) {
                if (m.id == mission_id) return &m;
            }
        }
        return nullptr;
    }
    std::size_t Count() const {
        std::size_t n = 0;
        for (const MissionGroup& g : groups) n += g.missions.size();
        return n;
    }
};

namespace detail {

// "scenarios/osa_practice.json" -> "osa_practice"
inline std::string MissionIdOf(const std::string& file) {
    const std::size_t slash = file.find_last_of("/\\");
    std::string base = slash == std::string::npos ? file : file.substr(slash + 1);
    const std::size_t dot = base.rfind('.');
    return dot == std::string::npos ? base : base.substr(0, dot);
}

class MissionCatalogLoader : public ConsoleLoader {
public:
    explicit MissionCatalogLoader(std::string data_dir) : data_dir_(std::move(data_dir)) {}

    bool Load(const json::Value& root, MissionCatalog& out) {
        if (!Keys(root, "", {"catalog_id", "status", "notes", "source", "groups"})) return false;
        const json::Value* groups = root.Find("groups");
        if (groups == nullptr || !groups->Is(json::Type::Array) || groups->items.empty()) {
            return Fail("groups", "required, a non-empty list");
        }
        for (std::size_t gi = 0; gi < groups->items.size(); ++gi) {
            const json::Value& g = groups->items[gi];
            const std::string gp = Index("groups", gi);
            MissionGroup group;
            if (!Keys(g, gp, {"id", "label", "notes", "missions"}) || !RequireString(g, gp, "id", group.id) ||
                !RequireString(g, gp, "label", group.label) || !OptionalString(g, gp, "notes", group.notes)) {
                return false;
            }
            const json::Value* list = g.Find("missions");
            if (list == nullptr || !list->Is(json::Type::Array) || list->items.empty()) {
                return Fail(Join(gp, "missions"), "required, a non-empty list");
            }
            for (std::size_t mi = 0; mi < list->items.size(); ++mi) {
                MissionDef m;
                m.group = group.id;
                if (!LoadMission(list->items[mi], Index(Join(gp, "missions"), mi), m)) return false;
                if (out.Find(m.id) != nullptr) return Fail(Index(Join(gp, "missions"), mi), "'" + m.id + "' is listed twice");
                for (const MissionDef& other : group.missions) {
                    if (other.id == m.id) return Fail(Index(Join(gp, "missions"), mi), "'" + m.id + "' is listed twice");
                }
                group.missions.push_back(std::move(m));
            }
            out.groups.push_back(std::move(group));
        }
        return true;
    }

private:
    bool LoadMission(const json::Value& e, const std::string& p, MissionDef& m) {
        if (!Keys(e, p, {"file", "title", "vehicles", "level"}) || !RequireString(e, p, "file", m.file) ||
            !RequireString(e, p, "title", m.title) || !OptionalString(e, p, "level", m.level)) {
            return false;
        }
        m.id = MissionIdOf(m.file);
        if (!m.level.empty() && m.level.rfind("/Game/", 0) != 0) return Fail(Join(p, "level"), "a level path starting /Game/");
        const std::string path = data_dir_ + "/" + m.file;
        std::string text;
        if (!ReadTextFile(path, text)) return Fail(Join(p, "file"), "cannot open " + m.file);
        json::Value root;
        std::string parse_error;
        if (!json::Parse(text, root, &parse_error)) return Fail(Join(p, "file"), m.file + ": " + parse_error);
        m.link = root.Find("link_id") != nullptr;
        const json::Value* vehicles = e.Find("vehicles");
        if (!m.link) {
            if (vehicles != nullptr) return Fail(Join(p, "vehicles"), "only a battery link has vehicles to choose");
            MissionVehicle v;
            if (!LoadVehicle(m.file, Join(p, "file"), v, &m)) return false;
            m.vehicles.push_back(std::move(v));
            return true;
        }
        // A battery link: both halves load together (LoadBatteryLinkFile checks they fit), then each vehicle's seats.
        std::string error;
        BatteryLinkDef def;
        if (LoadBatteryLinkFile(path, data_dir_, &error, &def) == nullptr) return Fail(Join(p, "file"), error);
        m.name = def.name;
        m.description = def.description;
        if (vehicles == nullptr || !vehicles->Is(json::Type::Array) || vehicles->items.empty()) {
            return Fail(Join(p, "vehicles"), "a battery link needs the vehicles the player may start in (osa, post)");
        }
        for (std::size_t i = 0; i < vehicles->items.size(); ++i) {
            const json::Value& ve = vehicles->items[i];
            const std::string vp = Index(Join(p, "vehicles"), i);
            MissionVehicle v;
            if (!Keys(ve, vp, {"id", "label"}) || !RequireString(ve, vp, "id", v.id) || !RequireString(ve, vp, "label", v.label)) {
                return false;
            }
            if (v.id != "osa" && v.id != "post") return Fail(Join(vp, "id"), "must be \"osa\" or \"post\"");
            const std::string label = v.label;
            if (!LoadVehicle(v.id == "osa" ? def.osa_file : def.post_file, vp, v, nullptr)) return false;
            v.label = label;
            m.vehicles.push_back(std::move(v));
        }
        return true;
    }

    // One scenario as the game loads it: its seats and the one that commands; its name and description into `m`.
    bool LoadVehicle(const std::string& file, const std::string& p, MissionVehicle& v, MissionDef* m) {
        ScenarioDef def;
        ConsoleData console;
        std::string error;
        if (!LoadScenarioFile(data_dir_ + "/" + file, data_dir_, def, console, &error)) return Fail(p, error);
        v.scenario_file = file;
        v.system_id = console.system_id;
        if (v.label.empty()) v.label = console.display_name.empty() ? console.system_id : console.display_name;
        bool commander = false;
        for (const ConsoleStation& st : console.stations) {
            MissionSeat seat{st.id, st.label, st.id == def.crew_data.commander};
            commander = commander || seat.commands;
            v.seats.push_back(std::move(seat));
        }
        if (v.seats.empty()) return Fail(p, file + ": its console has no stations to sit at");
        if (!commander) return Fail(p, file + ": no station is the crew data's commander_seat '" + def.crew_data.commander + "'");
        if (m != nullptr) {
            m->name = def.name;
            m->description = def.description;
        }
        return true;
    }

    std::string data_dir_;
};

}  // namespace detail

// A catalogue from its JSON text, the scenarios it names under `data_dir`. False with `error` naming the mission and the
// problem if any of them would not start.
inline bool LoadMissionCatalogJson(const std::string& json_text, const std::string& data_dir, MissionCatalog& out,
                                   std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(json_text, root, &parse_error)) {
        if (error != nullptr) *error = parse_error;
        return false;
    }
    MissionCatalog catalog;
    detail::MissionCatalogLoader loader(data_dir);
    if (!loader.Load(root, catalog)) {
        if (error != nullptr) *error = loader.Message();
        return false;
    }
    out = std::move(catalog);
    return true;
}

// The game's catalogue: data/scenarios/missions.json and every scenario it names (the menu's list).
inline bool LoadMissionCatalog(const std::string& data_dir, MissionCatalog& out, std::string* error = nullptr) {
    const std::string path = data_dir + "/scenarios/missions.json";
    std::string text;
    if (!detail::ReadTextFile(path, text)) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::string e;
    if (!LoadMissionCatalogJson(text, data_dir, out, &e)) {
        if (error != nullptr) *error = path + ": " + e;
        return false;
    }
    return true;
}

}  // namespace simcore
