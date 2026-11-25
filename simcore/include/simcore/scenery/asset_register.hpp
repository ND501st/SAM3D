// simcore/scenery/asset_register.hpp — the register of third-party art the map may draw in place of its procedural meshes
// (roadmap R6, data/scenery/assets.json, docs/scenery/ASSETS.md). Each asset says where it came from, who made it, its
// licence, whether it may be redistributed, when it was fetched and its checksum; each map part (scenery/map_meshes.hpp's
// keys) names the asset that replaces it, or none (null): procedural; so do the basic models and the munitions drawn
// whole by an asset (the aircraft and air-to-ground weapons from Sketchfab, below). A real asset is added only with the
// owner's say-so.
//
// The rules Load enforces (false with the reason, never a throw):
// - every asset has an id (unique), an Unreal path under /Game/, an origin, a known licence and a fetched date;
// - an asset that may not be redistributed (redistributable false: Fab's Standard licence, Epic's content-only one)
//   lives under /Game/ThirdParty/, which git ignores (.gitignore, tools/check_assets.py): it is fetched per machine;
// - CC-BY needs an author (the game's credits name them);
// - every part key is a known part (MapMeshParts) and names a registered asset, or null;
// - "models" maps a basic model's id (data/scenery/basic_models.json: an aircraft, a vehicle) and "munitions" a
//   munition's id (data/scenery/munitions.json: a bomb, a missile, a rocket) to a registered asset, or null. Such an
//   asset is a whole model at full size in the model's own frame (x forward, y right, z up, metres; the origin at the
//   middle of the fuselage or the body), prepared so (tools/blender/sketchfab_prepare.py): a munition's asset takes no
//   fix (scale 1, no yaw, no offset), since the effects draw it by the munition's own transform. The ids themselves are
//   checked against the libraries by test_scenery.cpp and tools/check_assets.py, not here.
#pragma once

#include <cstddef>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/json.hpp"
#include "simcore/scenery/map_meshes.hpp"

namespace simcore {
namespace scenery {

// The licences an asset may come under.
inline const std::vector<std::string>& AssetLicences() {
    static const std::vector<std::string> kLicences = {"CC0-1.0", "CC-BY-4.0", "Fab-Standard", "Epic-Content-Only", "Own"};
    return kLicences;
}

struct AssetEntry {
    std::string id;
    std::string unreal_path;  // the asset's object path, e.g. /Game/ThirdParty/Trees/SM_Oak.SM_Oak
    std::string origin;       // where it was fetched from (a page's URL, or "own work")
    std::string author;
    std::string licence;      // one of AssetLicences()
    bool redistributable = false;
    std::string fetched;      // the date it was fetched, YYYY-MM-DD
    std::string sha256;       // of the file as fetched (may be empty for an asset made here)
    // The fix from the asset's own frame to the part's (map_meshes.hpp: a unit cube, a tree one unit tall), applied before
    // each instance's own transform: scaled (a 10 m house modelled at full size: 0.1), turned about z (Unreal's yaw) to
    // put its length along x, raised in the part's units so its base sits where the procedural one's does.
    double scale = 1.0;
    double yaw_deg = 0.0;
    double z_offset_m = 0.0;
    std::string notes;
};

// A part drawn by an asset: the asset's id for every variant, or one per variant (an empty id is procedural).
struct AssetPart {
    std::string asset;                  // empty: procedural
    std::vector<std::string> variants;  // optional: variant k's asset (empty string: procedural)
    // Optional (A.3.25): other models of the same thing, drawn in place of `asset` instance by instance (a hornbeam
    // row of four different hornbeams, not one repeated); the first is `asset` itself. Not levels of detail: those are
    // `variants` (tools/ue_tree_lods.py).
    std::vector<std::string> alternates;
    // Optional (A.3.26): the models drawn in its place near the camera, within `near_m` (a Fab plant too dear to draw
    // everywhere), picked tree by tree as the alternates are; beyond, `asset` or `variants` as before, which hide
    // themselves within `near_m` (M_SimFoliage's NearHideM).
    std::vector<std::string> near_models;
    double near_m = 0.0;
    // Optional (A.3.40): the asset drawn when `asset` is not on this machine (a Fab model, never committed, falling back to
    // our own card rather than the procedural placeholder).
    std::string fallback;
};

class AssetRegister {
public:
    std::vector<AssetEntry> assets;
    std::map<std::string, AssetPart> parts;  // by part key; a key not listed is procedural
    std::map<std::string, std::string> models;     // a basic model's id -> an asset's id (a model not listed: basic shapes)
    std::map<std::string, std::string> munitions;  // a munition's id -> an asset's id (not listed: its procedural mesh)

    // A file's path, or the JSON text itself (it starts with '{'). False with the reason in `error`; the register is left
    // as it was.
    bool Load(const std::string& path_or_json, std::string* error = nullptr) {
        std::string text = path_or_json;
        const std::size_t first = text.find_first_not_of(" \t\r\n");
        if (first == std::string::npos || text[first] != '{') {
            std::ifstream f(path_or_json, std::ios::binary);
            if (!f) return Fail(error, "cannot open " + path_or_json);
            std::ostringstream s;
            s << f.rdbuf();
            text = s.str();
        }
        json::Value root;
        std::string parse_error;
        if (!json::Parse(text, root, &parse_error)) return Fail(error, "assets: " + parse_error);
        if (!root.Is(json::Type::Object)) return Fail(error, "assets: the file is not an object");

        AssetRegister out;
        const json::Value* list = root.Find("assets");
        if (list == nullptr || !list->Is(json::Type::Array)) return Fail(error, "assets: no \"assets\" array");
        std::set<std::string> ids;
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const json::Value& a = list->items[i];
            const std::string where = "assets[" + std::to_string(i) + "]";
            if (!a.Is(json::Type::Object)) return Fail(error, where + " is not an object");
            AssetEntry e;
            e.id = Text(a, "id");
            e.unreal_path = Text(a, "unreal_path");
            e.origin = Text(a, "origin");
            e.author = Text(a, "author");
            e.licence = Text(a, "licence");
            e.fetched = Text(a, "fetched");
            e.sha256 = Text(a, "sha256");
            e.notes = Text(a, "notes");
            e.scale = Number(a, "scale", 1.0);
            e.yaw_deg = Number(a, "yaw_deg", 0.0);
            e.z_offset_m = Number(a, "z_offset_m", 0.0);
            const json::Value* r = a.Find("redistributable");
            if (r == nullptr || !r->Is(json::Type::Bool)) return Fail(error, where + ": \"redistributable\" must be true or false");
            e.redistributable = r->boolean;
            std::string rule;
            if (!Valid(e, rule)) return Fail(error, where + " (" + e.id + "): " + rule);
            if (!ids.insert(e.id).second) return Fail(error, where + ": id " + e.id + " given twice");
            out.assets.push_back(e);
        }
        const json::Value* part_map = root.Find("parts");
        if (part_map != nullptr) {
            if (!part_map->Is(json::Type::Object)) return Fail(error, "assets: \"parts\" is not an object");
            const std::vector<std::string> known = MapMeshParts();
            for (std::size_t i = 0; i < part_map->keys.size(); ++i) {
                const std::string& key = part_map->keys[i];
                bool is_known = false;
                for (const std::string& k : known) is_known = is_known || k == key;
                if (!is_known) return Fail(error, "parts: " + key + " is not a part the map draws (map_meshes.hpp MapMeshParts)");
                const json::Value& p = part_map->items[i];
                if (!p.Is(json::Type::Object)) return Fail(error, "parts: " + key + " is not an object");
                AssetPart part;
                const json::Value* asset = p.Find("asset");
                if (asset == nullptr) return Fail(error, "parts: " + key + " has no \"asset\" (null for procedural)");
                if (asset->Is(json::Type::String)) {
                    part.asset = asset->string;
                } else if (!asset->Is(json::Type::Null)) {
                    return Fail(error, "parts: " + key + ": \"asset\" must be an asset's id or null");
                }
                if (const json::Value* vs = p.Find("variants")) {
                    if (!vs->Is(json::Type::Array)) return Fail(error, "parts: " + key + ": \"variants\" is not an array");
                    for (const json::Value& v : vs->items) {
                        if (v.Is(json::Type::String)) {
                            part.variants.push_back(v.string);
                        } else if (v.Is(json::Type::Null)) {
                            part.variants.push_back(std::string());
                        } else {
                            return Fail(error, "parts: " + key + ": a variant must be an asset's id or null");
                        }
                    }
                }
                if (const json::Value* as = p.Find("alternates")) {
                    if (!as->Is(json::Type::Array)) return Fail(error, "parts: " + key + ": \"alternates\" is not an array");
                    for (const json::Value& v : as->items) {
                        if (!v.Is(json::Type::String) || v.string.empty()) return Fail(error, "parts: " + key + ": an alternate must be an asset's id");
                        part.alternates.push_back(v.string);
                    }
                    if (!part.alternates.empty() && part.alternates.front() != part.asset) {
                        return Fail(error, "parts: " + key + ": the first alternate must be its \"asset\"");
                    }
                }
                if (const json::Value* nr = p.Find("near")) {
                    if (!nr->Is(json::Type::Object)) return Fail(error, "parts: " + key + ": \"near\" is not an object");
                    const json::Value* ns = nr->Find("alternates");
                    if (ns == nullptr || !ns->Is(json::Type::Array) || ns->items.empty()) {
                        return Fail(error, "parts: " + key + ": \"near\" needs its \"alternates\"");
                    }
                    for (const json::Value& v : ns->items) {
                        if (!v.Is(json::Type::String) || v.string.empty()) return Fail(error, "parts: " + key + ": a near model must be an asset's id");
                        part.near_models.push_back(v.string);
                    }
                    part.near_m = Number(*nr, "within_m", 0.0);
                    if (!(part.near_m > 0.0)) return Fail(error, "parts: " + key + ": \"near\" needs \"within_m\" above 0");
                }
                if (const json::Value* fb = p.Find("fallback")) {
                    if (!fb->Is(json::Type::String) || fb->string.empty()) return Fail(error, "parts: " + key + ": \"fallback\" must be an asset's id");
                    if (part.asset.empty()) return Fail(error, "parts: " + key + ": a \"fallback\" needs an \"asset\" to stand in for");
                    part.fallback = fb->string;
                }
                std::vector<std::string> named = part.variants;
                named.insert(named.end(), part.near_models.begin(), part.near_models.end());
                named.push_back(part.asset);
                if (!part.fallback.empty()) named.push_back(part.fallback);
                named.insert(named.end(), part.alternates.begin(), part.alternates.end());
                for (const std::string& id : named) {
                    if (!id.empty() && ids.count(id) == 0) return Fail(error, "parts: " + key + " names " + id + ", which is not registered");
                }
                out.parts[key] = part;
            }
        }
        if (!LoadIdMap(root, "models", ids, out.models, error)) return false;
        if (!LoadIdMap(root, "munitions", ids, out.munitions, error)) return false;
        for (const auto& m : out.munitions) {
            const AssetEntry* a = nullptr;
            for (const AssetEntry& e : out.assets) {
                if (e.id == m.second) a = &e;
            }
            if (a != nullptr && (a->scale != 1.0 || a->yaw_deg != 0.0 || a->z_offset_m != 0.0)) {
                return Fail(error, "munitions: " + m.first + "'s asset " + a->id +
                                       " must be prepared in the munition's frame (scale 1, yaw 0, z offset 0)");
            }
        }
        *this = out;
        return true;
    }

    const AssetEntry* Find(const std::string& id) const {
        for (const AssetEntry& e : assets) {
            if (e.id == id) return &e;
        }
        return nullptr;
    }

    // The asset that draws a part's variant, or null: procedural (MapPartMesh).
    const AssetEntry* AssetFor(const std::string& part, int variant) const {
        const auto it = parts.find(part);
        if (it == parts.end()) return nullptr;
        const AssetPart& p = it->second;
        if (variant >= 0 && static_cast<std::size_t>(variant) < p.variants.size()) {
            const std::string& v = p.variants[static_cast<std::size_t>(variant)];
            return v.empty() ? nullptr : Find(v);
        }
        return p.asset.empty() ? nullptr : Find(p.asset);
    }

    // The asset drawn for a part when its own is not on this machine (A.3.40), or null.
    const AssetEntry* FallbackFor(const std::string& part) const {
        const auto it = parts.find(part);
        return it == parts.end() || it->second.fallback.empty() ? nullptr : Find(it->second.fallback);
    }

    // The near models of a part (A.3.26): how many, the k-th (null past the end), and the distance they are drawn to (0:
    // none).
    int NearCount(const std::string& part) const {
        const auto it = parts.find(part);
        return it == parts.end() ? 0 : static_cast<int>(it->second.near_models.size());
    }
    const AssetEntry* NearFor(const std::string& part, int k) const {
        const auto it = parts.find(part);
        if (it == parts.end() || k < 0 || static_cast<std::size_t>(k) >= it->second.near_models.size()) return nullptr;
        return Find(it->second.near_models[static_cast<std::size_t>(k)]);
    }
    double NearM(const std::string& part) const {
        const auto it = parts.find(part);
        return it == parts.end() || it->second.near_models.empty() ? 0.0 : it->second.near_m;
    }

    // How many models a part is drawn by, instance by instance (its alternates; 1 with none).
    int AlternateCount(const std::string& part) const {
        const auto it = parts.find(part);
        return it == parts.end() || it->second.alternates.empty() ? 1 : static_cast<int>(it->second.alternates.size());
    }

    // The asset that draws alternate k of a part (k 0 is AssetFor(part, 0)), or null.
    const AssetEntry* AlternateFor(const std::string& part, int k) const {
        const auto it = parts.find(part);
        if (it == parts.end() || k <= 0 || static_cast<std::size_t>(k) >= it->second.alternates.size()) return AssetFor(part, 0);
        return Find(it->second.alternates[static_cast<std::size_t>(k)]);
    }

    // Which alternate draws the instance at (east, north): a hash of the place, so it is the same on every machine and
    // every run, and a tree keeps its model when the map is rebuilt.
    static int AlternateAt(double east_m, double north_m, int count) {
        if (count <= 1) return 0;
        const auto e = static_cast<long long>(east_m * 10.0), n = static_cast<long long>(north_m * 10.0);
        unsigned long long h = static_cast<unsigned long long>(e) * 0x9E3779B97F4A7C15ull ^ static_cast<unsigned long long>(n) * 0xC2B2AE3D27D4EB4Full;
        h ^= h >> 31;
        h *= 0xBF58476D1CE4E5B9ull;
        h ^= h >> 29;
        return static_cast<int>(h % static_cast<unsigned long long>(count));
    }

    // The asset that draws a basic model (an aircraft, a vehicle) or a munition whole, or null: its own procedural shape.
    const AssetEntry* AssetForModel(const std::string& model_id) const { return InMap(models, model_id); }
    const AssetEntry* AssetForMunition(const std::string& munition_id) const { return InMap(munitions, munition_id); }

    // The register's rules for one asset: true, or false with the rule it breaks.
    static bool Valid(const AssetEntry& e, std::string& rule) {
        if (e.id.empty()) return Broken(rule, "no id");
        if (e.unreal_path.compare(0, 6, "/Game/") != 0) return Broken(rule, "\"unreal_path\" must be under /Game/");
        if (e.origin.empty()) return Broken(rule, "no \"origin\" (where it was fetched from)");
        bool licence = false;
        for (const std::string& l : AssetLicences()) licence = licence || l == e.licence;
        if (!licence) return Broken(rule, "licence \"" + e.licence + "\" is not one of CC0-1.0, CC-BY-4.0, Fab-Standard, Epic-Content-Only, Own");
        if (!e.redistributable && e.unreal_path.compare(0, 17, "/Game/ThirdParty/") != 0) {
            return Broken(rule, "it may not be redistributed, so it must live under /Game/ThirdParty/ (ignored by git)");
        }
        if ((e.licence == "Fab-Standard" || e.licence == "Epic-Content-Only") && e.redistributable) {
            return Broken(rule, e.licence + " does not allow the files to be redistributed");
        }
        if (e.licence == "CC-BY-4.0" && e.author.empty()) return Broken(rule, "CC-BY needs its author, for the credits");
        if (e.fetched.empty()) return Broken(rule, "no \"fetched\" date");
        if (e.scale <= 0.0) return Broken(rule, "\"scale\" must be positive");
        return true;
    }

private:
    const AssetEntry* InMap(const std::map<std::string, std::string>& m, const std::string& key) const {
        const auto it = m.find(key);
        return it == m.end() ? nullptr : Find(it->second);
    }
    // "models" or "munitions": an object of id -> a registered asset's id or null (null entries are left out).
    static bool LoadIdMap(const json::Value& root, const char* name, const std::set<std::string>& ids,
                          std::map<std::string, std::string>& into, std::string* error) {
        const json::Value* m = root.Find(name);
        if (m == nullptr) return true;
        const std::string what(name);
        if (!m->Is(json::Type::Object)) return Fail(error, "assets: \"" + what + "\" is not an object");
        for (std::size_t i = 0; i < m->keys.size(); ++i) {
            const std::string& key = m->keys[i];
            const json::Value& v = m->items[i];
            if (key.empty()) return Fail(error, what + ": an empty id");
            if (v.Is(json::Type::Null)) continue;
            if (!v.Is(json::Type::String)) return Fail(error, what + ": " + key + " must name an asset's id or null");
            if (ids.count(v.string) == 0) return Fail(error, what + ": " + key + " names " + v.string + ", which is not registered");
            into[key] = v.string;
        }
        return true;
    }
    static bool Fail(std::string* error, const std::string& why) {
        if (error != nullptr) *error = why;
        return false;
    }
    static bool Broken(std::string& rule, const std::string& why) {
        rule = why;
        return false;
    }
    static std::string Text(const json::Value& v, const char* key) {
        const json::Value* f = v.Find(key);
        return f != nullptr && f->Is(json::Type::String) ? f->string : std::string();
    }
    static double Number(const json::Value& v, const char* key, double fallback) {
        const json::Value* f = v.Find(key);
        return f != nullptr && f->Is(json::Type::Number) ? f->number : fallback;
    }
};

}  // namespace scenery
}  // namespace simcore
