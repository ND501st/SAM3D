// simcore/command_post.hpp — the PU-12M battery command post as the player's own vehicle (docs/PU12_DEVELOPMENT.md P3).
//
// The post's chain of information (docs/research/PU12_RESEARCH_2026-10.md, docs/research/PLOTTING_BOARD_2026-10.md):
//   * The regiment's command post sends its air picture by telecode on the R-407 net (FuG-5) every cycle. The post
//     receives it while its ASPD-U is working and it is within the net's reach (data/command/aspd_u.json). The sending
//     post takes its targets off its own indicator by hand, so with many targets each comes less often, and in steps.
//   * The regiment's radar battery reads the air picture out by voice on the air-picture net (FuG-1, the plotters'
//     net): one report at a time, each target about once a minute, out to the board's extent (beyond the tube's
//     100 km). The board is the long-range picture, the tube the near one.
//   * Or a radar cabled to the post gives the primary picture, which the battery commander, the post's
//     commander-operator at the IT-45 (rwd_mb3), tracks semi-automatically: up to 12 targets, 2-3 s to take each off
//     (ru_pu12_text).
//   * The ASPD-U shows the picture on the IT-45 tube: up to 100 tracks, each with its number, its height in 0.2 km steps
//     and zone H1-H3, and its speed as a vector in 12.5 m/s steps (AspdTrack, Scope).
//   * The operator-plotter writes what he hears on the air-picture net on the glass board by hand, one mark at a time
//     (the board's cadence is his work, not a constant; pvo_bu_x_1984 art. 23: he takes the reports and plots them).
//     He does not copy the tube: it is the commander's, and the board is the back-up when it is dark. The battery
//     commander gives targets out from the tube and the board (P4).
// Everything the post shows carries its navigation error: the largest published (5 degrees, 3 km, flak11) until the post
// is oriented with the aiming circle, then small; on the move it grows with the distance driven (the KP-4: 1 %).
// The crew, the console's subsystems and the scenario decide the inputs; the runner feeds them (scenario_runner.hpp).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/c2_sources.hpp"
#include "simcore/json.hpp"
#include "simcore/pu12.hpp"
#include "simcore/pvo_grid.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

// The ASPD-U's numbers (data/command/aspd_u.json).
struct AspdDef {
    std::string id;
    std::string display_name;
    double max_targets = 100.0;
    double semi_auto_tracks = 12.0;
    double take_off_s = 2.5;
    double height_step_m = 200.0;
    double speed_step_mps = 12.5;
    double max_speed_mps = 787.5;
    double zone1_top_m = 1400.0;
    double zone2_top_m = 3000.0;
    std::vector<double> scales_m{35000.0, 50000.0, 100000.0};
    double telecode_period_s = 10.0;
    double telecode_range_m = 15000.0;
    double radar_period_s = 10.0;
    double plot_mark_s = 6.0;
    double oriented_az_error_deg = 0.6;
    double oriented_position_error_m = 100.0;
    double drift_fraction = 0.01;
    double unoriented_az_error_deg = 5.0;
    double unoriented_position_error_m = 3000.0;
    // The regiment's telecode as its sending post makes it (optional; 0: off): the targets are taken off that post's
    // indicator by hand, one every `telecode_take_off_s`, so with many targets each is refreshed less often than once a
    // cycle; and the coordinates go in steps of `telecode_step_m` (p19_to: 1 000 m on the 100 km scale).
    double telecode_take_off_s = 0.0;
    double telecode_step_m = 0.0;
    // The air-picture voice net (FuG-1, the plotters' net; optional, `air_net_report_s` 0: none): the regiment's radar
    // battery reads one report at a time (number, azimuth and range from its radar, identity and size, height, minute),
    // each target at most once every `air_net_period_s`; the post hears it within `air_net_range_m` of the regiment's
    // post. The plotter writes what he hears; the reports carry the radar's steps of `air_net_az_step_deg` and
    // `air_net_range_step_m`.
    double air_net_report_s = 0.0;
    double air_net_period_s = 60.0;
    double air_net_range_m = 20000.0;
    double air_net_az_step_deg = 1.0;
    double air_net_range_step_m = 1000.0;
    // The reports' form: 0 azimuth and range from the radar; 2 the PVO grid to the small square (the usual report,
    // bguir_tsp), 3 with the additional square (pvo_grid.hpp). The board's mark goes at the square's centre.
    double air_net_grid_levels = 0.0;
    // The regiment's command net (FuG-3, the R-111): its post's voice reaches this far (flak11: 25-30 km).
    double regiment_net_range_m = 30000.0;
};

namespace detail {

class AspdLoader : public BoardLoader {
public:
    bool Load(const json::Value& root, AspdDef& d) {
        if (!Keys(root, "", {"aspd_id", "display_name", "status", "notes", "aspd"}) || !RequireString(root, "", "aspd_id", d.id) ||
            !OptionalString(root, "", "display_name", d.display_name)) {
            return false;
        }
        const json::Value* a = root.Find("aspd");
        if (a == nullptr || !a->Is(json::Type::Object)) return Fail("aspd", "required, an object");
        cite_path_ = "aspd";
        if (!Keys(*a, "aspd", {"max_targets", "semi_auto_tracks", "take_off_s", "height_step_m", "speed_step_mps", "max_speed_mps",
                               "zone1_top_m", "zone2_top_m", "scales_m", "telecode_period_s", "telecode_range_m", "radar_period_s",
                               "plot_mark_s", "oriented_az_error_deg", "oriented_position_error_m", "drift_fraction",
                               "unoriented_az_error_deg", "unoriented_position_error_m", "telecode_take_off_s", "telecode_step_m",
                               "air_net_report_s", "air_net_period_s", "air_net_range_m", "air_net_az_step_deg", "air_net_range_step_m",
                               "air_net_grid_levels", "regiment_net_range_m"})) {
            return false;
        }
        // Optional: the telecode's sending post and the air-picture voice net (absent: off, or the defaults).
        for (const auto& f : {std::make_pair("telecode_take_off_s", &d.telecode_take_off_s), std::make_pair("telecode_step_m", &d.telecode_step_m),
                              std::make_pair("air_net_report_s", &d.air_net_report_s), std::make_pair("air_net_period_s", &d.air_net_period_s),
                              std::make_pair("air_net_range_m", &d.air_net_range_m), std::make_pair("air_net_az_step_deg", &d.air_net_az_step_deg),
                              std::make_pair("air_net_range_step_m", &d.air_net_range_step_m),
                              std::make_pair("air_net_grid_levels", &d.air_net_grid_levels),
                              std::make_pair("regiment_net_range_m", &d.regiment_net_range_m)}) {
            if (a->Find(f.first) != nullptr && !Cited(*a, f.first, *f.second)) return false;
        }
        if (d.air_net_report_s > 0.0 && !(d.air_net_period_s > 0.0)) return Fail("aspd.air_net_period_s", "must be > 0");
        if (d.air_net_grid_levels != 0.0 && d.air_net_grid_levels != 2.0 && d.air_net_grid_levels != 3.0) {
            return Fail("aspd.air_net_grid_levels.value", "0 (azimuth and range), 2 or 3 (the grid's squares)");
        }
        for (const auto& f : {std::make_pair("max_targets", &d.max_targets), std::make_pair("semi_auto_tracks", &d.semi_auto_tracks),
                              std::make_pair("take_off_s", &d.take_off_s), std::make_pair("height_step_m", &d.height_step_m),
                              std::make_pair("speed_step_mps", &d.speed_step_mps), std::make_pair("max_speed_mps", &d.max_speed_mps),
                              std::make_pair("zone1_top_m", &d.zone1_top_m), std::make_pair("zone2_top_m", &d.zone2_top_m),
                              std::make_pair("telecode_period_s", &d.telecode_period_s), std::make_pair("telecode_range_m", &d.telecode_range_m),
                              std::make_pair("radar_period_s", &d.radar_period_s), std::make_pair("plot_mark_s", &d.plot_mark_s),
                              std::make_pair("oriented_az_error_deg", &d.oriented_az_error_deg),
                              std::make_pair("oriented_position_error_m", &d.oriented_position_error_m),
                              std::make_pair("drift_fraction", &d.drift_fraction),
                              std::make_pair("unoriented_az_error_deg", &d.unoriented_az_error_deg),
                              std::make_pair("unoriented_position_error_m", &d.unoriented_position_error_m)}) {
            if (!Cited(*a, f.first, *f.second)) return false;
        }
        for (const auto& f : {std::make_pair("max_targets", d.max_targets), std::make_pair("semi_auto_tracks", d.semi_auto_tracks),
                              std::make_pair("take_off_s", d.take_off_s), std::make_pair("height_step_m", d.height_step_m),
                              std::make_pair("speed_step_mps", d.speed_step_mps), std::make_pair("telecode_period_s", d.telecode_period_s),
                              std::make_pair("radar_period_s", d.radar_period_s), std::make_pair("plot_mark_s", d.plot_mark_s)}) {
            if (!(f.second > 0.0)) return Fail(Join("aspd", f.first), "must be > 0");
        }
        if (!(d.zone2_top_m > d.zone1_top_m)) return Fail("aspd.zone2_top_m", "must be above zone1_top_m");
        const json::Value* s = a->Find("scales_m");
        const json::Value* list = s != nullptr ? s->Find("value") : nullptr;
        if (list == nullptr || !list->Is(json::Type::Array) || list->items.empty()) return Fail("aspd.scales_m.value", "expected a list");
        if (!Provenance(*s, "aspd.scales_m")) return false;
        d.scales_m.clear();
        for (const json::Value& v : list->items) {
            if (!v.Is(json::Type::Number) || !(v.number > 0.0)) return Fail("aspd.scales_m", "distances > 0");
            d.scales_m.push_back(v.number);
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadAspdFile(const std::string& path, AspdDef& out, std::string* error = nullptr) {
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
    AspdDef d;
    detail::AspdLoader loader;
    if (!loader.Load(root, d)) {
        if (error != nullptr) *error = path + ": " + loader.Message();
        return false;
    }
    out = std::move(d);
    return true;
}

// One target on the IT-45 as the ASPD-U holds it.
struct AspdTrack {
    std::string target_id;  // who it really is (the crew sees `number`)
    int number = 0;         // 00-99: the regiment's number (telecode) or the post's own (primary)
    Vec3 position;          // where the post puts it (ENU), its navigation error in it
    double height_m = 0.0;  // in the ASPD-U's 0.2 km steps
    int zone = 1;           // H1, H2, H3
    Vec3 velocity;          // course and speed, the speed in 12.5 m/s steps (zero until the second fix)
    int type = 9;           // the board's code tens digit as sent (1 friend, 2 training target, 8 enemy, 9 no IFF)
    bool friendly = false;
    bool group = false;
    bool primary = false;   // from a radar cabled to the post (else the regiment's telecode)
    bool designated = false;  // the regiment's designation to this battery came with it (ЦУ with the battery's address)
    bool imitated = false;    // the ФР block's imitated echo (КОНТРОЛЬ: a training target, no aircraft)
    double time_s = 0.0;    // last fix
};

// A command the regiment's post sends by telecode: a two-digit decimal shown on the IT-45 (rwd_mb3 pu12: commands and
// reports "in Form von Dezimalzahlen"; flak11 PU12-0: "in Form zweistelliger Ziffern"). What each code means is the
// table's (data/command/aspd_codes.json). Or a group of the talk table ПТ-1 said on the regiment's command net only, with
// no telecode of the same meaning («Рябина, я Тополь: 707. Приём.»: docs/crew/12_COMMUNICATIONS.md §2.9). A command held
// on cue goes out when a lesson calls for it (tutorial.hpp "regiment:<group or code>"), not at a time.
struct RegimentCommand {
    double at_s = 0.0;
    int code = -1;        // the telecode's two digits (-1: a group only)
    std::string group;    // a talk-table group said on the command net ("" : the code's, if ПТ-1 has one)
    bool on_cue = false;
};

// The code table: each code's meaning and what the battery does on it.
struct AspdCode {
    int code = 0;
    std::string meaning;
    std::string effect;  // "readiness", "fire_ban", "fire_free", "cease_fire" or "" (shown only)
};
struct AspdCodeTable {
    std::string id;
    std::vector<AspdCode> codes;
    const AspdCode* Find(int code) const {
        for (const AspdCode& c : codes) {
            if (c.code == code) return &c;
        }
        return nullptr;
    }
};

namespace detail {

class AspdCodeLoader : public BoardLoader {
public:
    bool Load(const json::Value& root, AspdCodeTable& t) {
        if (!Keys(root, "", {"codes_id", "display_name", "status", "notes", "source", "confidence", "codes"}) ||
            !RequireString(root, "", "codes_id", t.id) || !Provenance(root, "")) {
            return false;
        }
        const json::Value* list = root.Find("codes");
        if (list == nullptr || !list->Is(json::Type::Array)) return Fail("codes", "required, a list");
        for (std::size_t i = 0; i < list->items.size(); ++i) {
            const json::Value& e = list->items[i];
            const std::string p = "codes[" + std::to_string(i) + "]";
            AspdCode c;
            double code = -1.0;
            if (e.Find("code") == nullptr) return Fail(Join(p, "code"), "required");
            if (!Keys(e, p, {"code", "meaning", "effect", "notes"}) || !OptionalNumber(e, p, "code", code) ||
                !RequireString(e, p, "meaning", c.meaning) || !OptionalString(e, p, "effect", c.effect)) {
                return false;
            }
            if (code < 0.0 || code > 99.0 || code != std::floor(code)) return Fail(Join(p, "code"), "a two-digit number, 00-99");
            if (!c.effect.empty() && c.effect != "readiness" && c.effect != "fire_ban" && c.effect != "fire_free" && c.effect != "cease_fire") {
                return Fail(Join(p, "effect"), "readiness, fire_ban, fire_free or cease_fire");
            }
            c.code = static_cast<int>(code);
            if (t.Find(c.code) != nullptr) return Fail(Join(p, "code"), "duplicate");
            t.codes.push_back(std::move(c));
        }
        return true;
    }
};

}  // namespace detail

// The player's hand on the board (data/gameplay/plotting.json; game values, not system data): he clicks where he thinks
// the report put the target. Within `exact_board_m` of the right place (on the board's surface) his mark goes exactly
// there; up to `scatter_board_m` it goes somewhere off the right place, further than exact but within his own miss;
// beyond, further off still, by `far_min` to `far_max` times his miss.
struct PlottingTuning {
    double exact_board_m = 0.05;
    double scatter_board_m = 0.20;
    double far_min = 1.0;
    double far_max = 1.6;
};

namespace detail {

class PlottingTuningLoader : public BoardLoader {
public:
    bool Load(const json::Value& root, PlottingTuning& t) {
        if (!Keys(root, "", {"plotting_id", "status", "notes", "source", "confidence", "exact_board_m", "scatter_board_m", "far_min", "far_max"}) ||
            !Provenance(root, "")) {
            return false;
        }
        for (const auto& f : {std::make_pair("exact_board_m", &t.exact_board_m), std::make_pair("scatter_board_m", &t.scatter_board_m),
                              std::make_pair("far_min", &t.far_min), std::make_pair("far_max", &t.far_max)}) {
            if (!OptionalNumber(root, "", f.first, *f.second)) return false;
            if (!(*f.second > 0.0)) return Fail(f.first, "must be > 0");
        }
        if (t.scatter_board_m < t.exact_board_m) return Fail("scatter_board_m", "must be at least exact_board_m");
        if (t.far_max < t.far_min) return Fail("far_max", "must be at least far_min");
        return true;
    }
};

}  // namespace detail

inline bool LoadPlottingTuningFile(const std::string& path, PlottingTuning& out, std::string* error = nullptr) {
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
    PlottingTuning t;
    detail::PlottingTuningLoader loader;
    if (!loader.Load(root, t)) {
        if (error != nullptr) *error = path + ": " + loader.Message();
        return false;
    }
    out = t;
    return true;
}

inline bool LoadAspdCodesFile(const std::string& path, AspdCodeTable& out, std::string* error = nullptr) {
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
    AspdCodeTable t;
    detail::AspdCodeLoader loader;
    if (!loader.Load(root, t)) {
        if (error != nullptr) *error = path + ": " + loader.Message();
        return false;
    }
    out = std::move(t);
    return true;
}

// One launcher on the IT-1 / IT-2 boards: what its reports on the battery net have told the post (valka_pu12: the
// boards show the units' state, the vehicles' activity and their ammunition).
struct LauncherStatus {
    enum class State { Unknown, March, Ready, Engaging, Launched, Empty };
    std::string callsign;
    State state = State::Unknown;
    int missiles = -1;        // -1: not reported
    int target_number = 0;    // the target it reported (0: none)
    double reported_s = -1.0; // when the post last heard from it
};
// A launcher's full load: the 9A33's two packs of three (battery_fire.hpp's launchers start with it).
constexpr int kLauncherFullLoad = 6;

// The lamps one launcher's column lights on the IT-1M / IT-2M boards (rwd_mb3, pu12e: the signal groups КОМПЛЕКТ,
// РАБОТА and ТЕХ. СОСТОЯНИЕ; legends read from its photographs). Which report lights which lamp is SAM3D's reading.
struct BoardLamps {
    int load = 0;              // КОМПЛЕКТ: 0 dark, 1 the lamp "1" (three quarters or more), 2 "1/2", 3 "1/4"
    bool ready = false;        // ГОТОВ
    bool sees = false;         // ВИЖУ: on a target
    bool fired = false;        // ВЫП.: launched
    bool fault = false;        // ТЕХ. СОСТОЯНИЕ (РПК, БЧ, ШАССИ): a fault reported (none modelled: dark)
};
inline BoardLamps BoardLampsOf(const LauncherStatus& s, int full_load = kLauncherFullLoad) {
    BoardLamps b;
    if (s.state == LauncherStatus::State::Unknown) return b;
    if (s.missiles > 0 && full_load > 0) {
        const double f = static_cast<double>(s.missiles) / full_load;
        b.load = f >= 0.75 ? 1 : (f >= 0.4 ? 2 : 3);
    }
    b.ready = s.state == LauncherStatus::State::Ready;
    b.sees = s.state == LauncherStatus::State::Engaging;
    b.fired = s.state == LauncherStatus::State::Launched;
    return b;
}

inline const char* LauncherStateName(LauncherStatus::State s) {
    switch (s) {
        case LauncherStatus::State::March: return "march";
        case LauncherStatus::State::Ready: return "ready";
        case LauncherStatus::State::Engaging: return "engaging";
        case LauncherStatus::State::Launched: return "launched";
        case LauncherStatus::State::Empty: return "out of missiles";
        default: return "unknown";
    }
}

// The regiment's command post that sends the telecode, its radars, and when its net is off the air (scenario "post").
struct PostRegiment {
    bool has_position = false;
    Vec3 position;                     // ENU; the R-407 net's reach is measured from here
    std::vector<RegimentRadar> radars;  // empty: the regiment sees every target
    std::vector<C2Outage> outages;
    bool telecode = true;              // false: the regiment sends no telecode (peacetime: the ASPD-U left off)
    double designate_within_m = 0.0;   // the regiment designates to the battery the targets this near it (0: none)
    std::vector<RegimentCommand> commands;  // its commands: by telecode and its voice net, oldest first; held on cue anywhere
};

class CommandPost {
public:
    CommandPost(AspdDef aspd, PlottingBoardDef board, const Vec3& position, unsigned seed)
        : def_(std::move(aspd)), board_(std::move(board), position, seed), position_(position), rng_(seed ^ 0x2545f491u) {
        Unoriented();
    }

    const AspdDef& Def() const { return def_; }
    const PlottingBoard& Board() const { return board_; }
    PlottingBoard& Board() { return board_; }
    const std::vector<AspdTrack>& Tracks() const { return tracks_; }
    const Vec3& Position() const { return position_; }

    // ---- Navigation: the error everything the post shows carries.
    // Never oriented, or the gyro lost: the largest published error, drawn once (flak11).
    void Unoriented() {
        az_error_rad_ = DegToRad(def_.unoriented_az_error_deg) * (2.0 * U() - 1.0);
        offset_ = RandomOffset(def_.unoriented_position_error_m);
        oriented_ = false;
    }
    // Oriented with the aiming circle: a small error; `quality` scales it (1 = a trained squad leader; larger is worse).
    void Oriented(double quality = 1.0) {
        const double q = quality > 0.0 ? quality : 1.0;
        az_error_rad_ = DegToRad(def_.oriented_az_error_deg * q) * (2.0 * U() - 1.0);
        offset_ = RandomOffset(def_.oriented_position_error_m * q);
        drift_dir_ = 2.0 * kPi * U();
        drift_rate_ = def_.drift_fraction * U();  // up to 1 % of the distance (flak11)
        oriented_ = true;
    }
    bool IsOriented() const { return oriented_; }
    double AzErrorRad() const { return az_error_rad_; }
    const Vec3& OffsetM() const { return offset_; }
    // The post moved: the KP-4's dead reckoning drifts with the distance driven.
    void MoveTo(const Vec3& at) {
        const double d = (Vec3{at.x, at.y, 0.0} - Vec3{position_.x, position_.y, 0.0}).Length();
        position_ = at;
        board_.SetCentre(at);
        if (d <= 0.0) return;
        offset_ = offset_ + Vec3{std::sin(drift_dir_), std::cos(drift_dir_), 0.0} * (drift_rate_ * d);
    }
    // Where the post puts a target that is really at `truth`.
    Vec3 Shown(const Vec3& truth) const {
        const Vec3 rel = truth - position_;
        const double c = std::cos(az_error_rad_), s = std::sin(az_error_rad_);
        return position_ + Vec3{rel.x * c + rel.y * s, -rel.x * s + rel.y * c, rel.z} + offset_;
    }

    // ---- The regiment's net: within the R-407's reach of the regiment's post, not off the air, and sending at all.
    bool TelecodeInReach(const PostRegiment& r, double t) const { return r.telecode && InReach(r, t, def_.telecode_range_m); }
    // The regiment designates to this battery the targets within this range of it (ЦУ with the battery's address, carried
    // by the telecode; flak11 PU12-0: "Empfang und Abgabe adressierter Zielzuweisungen"). 0: it designates none.
    void SetRegimentDesignationRange(double m) { designate_within_m_ = m; }

    // The ДХ keypad's characteristics for a track (rwd_mb3 pu12a: the Pult DCh enters friend or foe, group, the height
    // zone): kept on that target through later fixes. False if no track has the number.
    struct Attributes {
        int friendly = -1;  // -1 unchanged, 0 foe, 1 friend
        int group = -1;     // -1 unchanged, 0 single, 1 group
        int zone = 0;       // 0 unchanged, 1-3 H1-H3
    };
    bool SetAttributes(int number, const Attributes& a) {
        for (AspdTrack& k : tracks_) {
            if (k.number != number) continue;
            Attributes& o = overrides_[k.target_id];
            if (a.friendly >= 0) o.friendly = a.friendly;
            if (a.group >= 0) o.group = a.group;
            if (a.zone > 0) o.zone = a.zone;
            ApplyOverrides(k);
            return true;
        }
        return false;
    }

    // The ФР block's imitated echo (rwd_mb3 pu12a: the ФР makes the sweep, the range marks and an imitated echo signal):
    // with the block at КОНТРОЛЬ a training target goes round the post, 30 km out at 200 m/s and 2 000 m (SAM3D's),
    // and is taken off the tube like a cabled radar's echo.
    static BoardContact ImitatedEcho(double t) {
        const double r = 30000.0, v = 200.0;
        const double a = t * v / r;
        BoardContact c;
        c.target_id = "imitation";
        c.position = Vec3{r * std::sin(a), r * std::cos(a), 2000.0};
        c.altitude_m = 2000.0;
        c.type = 9;
        return c;
    }
    // The regiment's command net (the R-111): within its reach of the regiment's post and not off the air.
    bool RegimentNetInReach(const PostRegiment& r, double t) const { return InReach(r, t, def_.regiment_net_range_m); }
    // The air-picture voice net: within the R-123M's reach of the regiment's post and not off the air.
    bool AirNetInReach(const PostRegiment& r, double t) const { return InReach(r, t, def_.air_net_range_m); }
    // Where the air-picture net's reports are measured from (the regiment's radar); the post itself until set.
    void SetAirNetSource(const Vec3& at) {
        air_source_ = at;
        has_air_source_ = true;
    }
    // The scenario's frame, for the reports' grid squares (latitude and longitude).
    void SetFrame(const LocalFrame& frame) {
        frame_ = frame;
        has_frame_ = true;
    }
    // The targets the regiment's radars cover (all of them without radars).
    static std::vector<BoardContact> RegimentSees(const PostRegiment& r, const std::vector<BoardContact>& truth) {
        if (r.radars.empty()) return truth;
        std::vector<BoardContact> out;
        for (const BoardContact& c : truth) {
            for (const RegimentRadar& radar : r.radars) {
                if (Covers(radar, c)) {
                    out.push_back(c);
                    break;
                }
            }
        }
        return out;
    }
    static bool Covers(const RegimentRadar& radar, const BoardContact& c) {
        const double range = (Vec3{c.position.x, c.position.y, 0.0} - Vec3{radar.position.x, radar.position.y, 0.0}).Length();
        const double h = c.height_agl_m >= 0.0 ? c.height_agl_m : c.altitude_m;
        return range <= radar.max_range_m && range <= C2Network::RadarHorizonM(radar.antenna_height_m, h);
    }

    // What the post's crew and equipment do this step.
    struct Inputs {
        bool telecode = false;       // the ASPD-U receiving the regiment's telecode (console telecode_reception)
        bool primary = false;        // a radar cabled to the post and the ASPD-U showing it (console primary_picture)
        bool air_net = false;        // the post listening on the air-picture net (console air_net, within its reach)
        bool imitation = false;      // the ФР block at КОНТРОЛЬ: the imitated echo on the tube (console imitation)
        bool board_open = true;      // the board swung open (console board_open); folded, nothing can be written
        bool operator_free = false;  // the operator-plotter at his place, not busy with a drill
        double operator_speed = 1.0; // his tier's speed at handwork (1 = a trained operator)
        bool tube_free = false;      // the commander-operator at the IT-45, free to take the cabled radar's targets off
        double tube_speed = 1.0;     // his tier's speed at it
        // The air-picture net through the crew (docs/crew/12_COMMUNICATIONS.md): heard by a seat whose box is on FuG-1, who
        // reads each report out to the plotter (pvo_bu_x_1984 art. 23); until then it is held. A jammed net loses a
        // report with this chance.
        bool relay = false;
        double air_net_loss = 0.0;
    };

    // `regiment`: the targets the regiment sends (RegimentSees); `radar`: those the cabled radar covers.
    void Update(double t, const std::vector<BoardContact>& regiment, const std::vector<BoardContact>& radar, const Inputs& in) {
        // The regiment's picture, once a cycle while it is received.
        if (in.telecode && (!telecode_started_ || t + 1e-9 >= next_telecode_s_)) {
            telecode_started_ = true;
            next_telecode_s_ = t + def_.telecode_period_s;
            ReceiveTelecode(t, regiment);
        }
        if (!in.telecode) telecode_started_ = false;
        // The air-picture net talks whether the post listens or not; what the post hears waits for the plotter.
        relay_ = in.relay;
        air_net_loss_ = in.air_net_loss;
        if (def_.air_net_report_s > 0.0) ReadAirNet(t, regiment, in.air_net);
        // A track not refreshed for three of its rounds goes off the tube: the telecode's (a cycle, or the sending
        // post's round of its targets taken off by hand), or the operator's round of the semi-automatic tracks (12
        // targets at 2-3 s each).
        const double telecode_stale = 3.0 * std::max(def_.telecode_period_s, telecode_round_s_);
        const double primary_stale = 3.0 * std::max(def_.radar_period_s, def_.semi_auto_tracks * def_.take_off_s);
        tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                                     [&](const AspdTrack& k) { return t - k.time_s > (k.primary ? primary_stale : telecode_stale); }),
                      tracks_.end());
        // The commander-operator's hands at the tube take the primary picture off; the plotter's write the board.
        imitation_on_ = in.imitation;
        imitation_t_ = t;
        board_open_ = in.board_open;
        if (!in.imitation) {
            tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(), [](const AspdTrack& k) { return k.imitated; }), tracks_.end());
        }
        if ((in.primary || in.imitation) && in.tube_free && t + 1e-9 >= tube_free_s_) {
            const std::vector<BoardContact> echoes = in.imitation ? std::vector<BoardContact>{ImitatedEcho(t)} : radar;
            if (const BoardContact* c = NextTakeOff(t, echoes)) {
                TakeOff(t, *c);
                tube_free_s_ = t + def_.take_off_s / (in.tube_speed > 0.0 ? in.tube_speed : 1.0);
            }
        }
        // The plotter writes what he heard on the air-picture net, oldest first (a report is read out once); folded,
        // the board takes nothing.
        if (!in.board_open || !in.operator_free || t + 1e-9 < operator_free_s_) return;
        if (!heard_.empty()) WriteHeard(t, 0, in.operator_speed > 0.0 ? in.operator_speed : 1.0);
    }

    // ---- The air-picture voice net (FuG-1; rwd_mb3 pu12e: the board carries the targets' tracks in the designation
    // grid's squares about the radar; flak11 PU12-1: the air-picture net, the plotters').
    struct AirReport {
        BoardContact contact;  // where the report puts the target: the radar's steps, the post's own error in it
        int number = 0;        // the board's number of the target
        double heard_s = 0.0;  // when its reading ended
        std::string text;      // as read out
    };
    // Reports heard and not yet written on the board, oldest first. One not written within a reporting period is
    // forgotten (the next report of that target has come or is coming).
    const std::vector<AirReport>& Heard() const { return heard_; }
    // Reports heard on the net and not yet read out to the plotter (Inputs::relay), oldest first; forgotten likewise.
    const std::vector<AirReport>& Held() const { return held_; }
    // The report with this board number read out to the plotter now: it waits for his hand. False if none is held.
    bool ReleaseHeld(int board_number, double t) {
        for (std::size_t i = 0; i < held_.size(); ++i) {
            if (held_[i].number != board_number) continue;
            AirReport r = held_[i];
            r.heard_s = t;
            held_.erase(held_.begin() + static_cast<std::ptrdiff_t>(i));
            heard_.push_back(r);
            return true;
        }
        return false;
    }
    // Every report read out on the net since the last call, and whether the post heard it (for the radio log).
    struct Spoken {
        AirReport report;
        bool heard = false;
    };
    std::vector<Spoken> TakeSpoken() {
        std::vector<Spoken> out;
        out.swap(spoken_);
        return out;
    }
    // The plotter (a player) writes the heard report of the target with this board number. False if none is waiting
    // or he is still writing the last one.
    bool PlotHeard(double t, int board_number, double speed) {
        if (!board_open_ || t + 1e-9 < operator_free_s_) return false;
        for (std::size_t i = 0; i < heard_.size(); ++i) {
            if (heard_[i].number != board_number) continue;
            WriteHeard(t, i, speed > 0.0 ? speed : 1.0);
            return true;
        }
        return false;
    }

    // The plotter (a player) puts the mark of a report he heard (its board number) where he clicks on the board: `clicked` in ENU, `metres_per_board_m` the board's scale. How near he came decides
    // where the mark really goes (PlottingTuning). Not ok if the board is folded, no report waiting has the number, or he
    // is still writing the last one.
    struct PlotResult {
        bool ok = false;
        double miss_board_m = 0.0;   // how far his click was from the right place, on the board
        double error_board_m = 0.0;  // how far the mark went from it
        Vec3 placed;
    };
    PlotResult PlotAt(double t, int number, const Vec3& clicked, double metres_per_board_m, const PlottingTuning& tuning, double speed) {
        PlotResult out;
        if (!board_open_ || t + 1e-9 < operator_free_s_ || !(metres_per_board_m > 0.0)) return out;
        const double sp = speed > 0.0 ? speed : 1.0;
        for (std::size_t i = 0; i < heard_.size(); ++i) {
            if (heard_[i].number != number) continue;
            heard_[i].contact.position = Placed(heard_[i].contact.position, clicked, metres_per_board_m, tuning, out);
            WriteHeard(t, i, sp);
            out.ok = true;
            return out;
        }
        return out;
    }
    bool BoardOpen() const { return board_open_; }
    // When the plotter's hands are next free.
    double OperatorFreeS() const { return operator_free_s_; }

    // The tube at a scale: each track's place as a fraction of the scale's radius (east, north), and its speed vector
    // drawn as the distance it covers in a minute.
    struct ScopeMark {
        int number = 0;
        double x = 0.0;
        double y = 0.0;
        double vx = 0.0;
        double vy = 0.0;
        int zone = 1;
        bool friendly = false;
        bool group = false;
        bool primary = false;  // a cabled radar's track: its echo is on the tube too
        bool designated = false;  // the regiment's designation to this battery
        bool imitated = false;    // the ФР block's imitated echo (number -1: the echo not yet taken off)
    };
    std::vector<ScopeMark> Scope(double scale_m) const {
        std::vector<ScopeMark> out;
        if (!(scale_m > 0.0)) return out;
        if (imitation_on_) {  // the imitated echo itself, taken off or not
            const Vec3 rel = Shown(ImitatedEcho(imitation_t_).position) - position_;
            if (rel.HorizontalLength() <= scale_m) {
                ScopeMark m;
                m.number = -1;
                m.x = rel.x / scale_m;
                m.y = rel.y / scale_m;
                m.primary = true;
                m.imitated = true;
                out.push_back(m);
            }
        }
        for (const AspdTrack& k : tracks_) {
            const Vec3 rel = k.position - position_;
            if (rel.HorizontalLength() > scale_m) continue;
            ScopeMark m;
            m.number = k.number;
            m.x = rel.x / scale_m;
            m.y = rel.y / scale_m;
            m.vx = 60.0 * k.velocity.x / scale_m;
            m.vy = 60.0 * k.velocity.y / scale_m;
            m.zone = k.zone;
            m.friendly = k.friendly;
            m.group = k.group;
            m.primary = k.primary;
            m.designated = k.designated;
            m.imitated = k.imitated;
            out.push_back(m);
        }
        return out;
    }

    // The ASPD-U's height zone of a height.
    int ZoneOf(double height_m) const { return height_m <= def_.zone1_top_m ? 1 : height_m <= def_.zone2_top_m ? 2 : 3; }
    double QuantiseHeight(double height_m) const {
        return std::max(0.0, std::round(height_m / def_.height_step_m) * def_.height_step_m);
    }
    double QuantiseSpeed(double speed_mps) const {
        return std::min(def_.max_speed_mps, std::round(speed_mps / def_.speed_step_mps) * def_.speed_step_mps);
    }
    double LastTelecodeS() const { return last_telecode_s_; }

private:
    double U() { return static_cast<double>(rng_()) / 4294967296.0; }
    // Where the player's mark goes: exact within the tuning's first radius, else off by a random amount in a random way.
    Vec3 Placed(const Vec3& right, const Vec3& clicked, double mpb, const PlottingTuning& tu, PlotResult& out) {
        const double miss = Vec3{clicked.x - right.x, clicked.y - right.y, 0.0}.Length() / mpb;
        double off = 0.0;
        if (miss > tu.scatter_board_m) {
            off = miss * (tu.far_min + (tu.far_max - tu.far_min) * U());
        } else if (miss > tu.exact_board_m) {
            off = tu.exact_board_m + (miss - tu.exact_board_m) * U();
        }
        const double a = 2.0 * kPi * U();
        out.miss_board_m = miss;
        out.error_board_m = off;
        out.placed = Vec3{right.x + off * mpb * std::sin(a), right.y + off * mpb * std::cos(a), right.z};
        return out.placed;
    }
    bool InReach(const PostRegiment& r, double t, double range_m) const {
        for (const C2Outage& o : r.outages) {
            if (t + 1e-9 >= o.from_s && t < o.to_s) return false;
        }
        if (!r.has_position) return true;
        return (Vec3{r.position.x, r.position.y, 0.0} - Vec3{position_.x, position_.y, 0.0}).Length() <= range_m;
    }
    Vec3 RandomOffset(double max_m) {
        const double r = max_m * std::sqrt(U());
        const double a = 2.0 * kPi * U();
        return Vec3{r * std::sin(a), r * std::cos(a), 0.0};
    }

    // A fix of a target into its track (made if new): the post's error, the ASPD-U's steps, the speed from the last fix.
    void Fix(double t, const BoardContact& c, bool primary, int number) {
        AspdTrack* k = nullptr;
        for (AspdTrack& e : tracks_) {
            if (e.target_id == c.target_id) k = &e;
        }
        const Vec3 shown = Shown(c.position);
        if (k == nullptr) {
            if (static_cast<double>(tracks_.size()) >= def_.max_targets) return;
            tracks_.push_back(AspdTrack{});
            k = &tracks_.back();
            k->target_id = c.target_id;
            k->velocity = Vec3{};
        } else if (t - k->time_s > 1e-6) {
            const Vec3 v = (shown - k->position) / (t - k->time_s);
            const double speed = Vec3{v.x, v.y, 0.0}.Length();
            const double q = QuantiseSpeed(speed);
            k->velocity = speed > 1e-9 ? Vec3{v.x, v.y, 0.0} * (q / speed) : Vec3{};
        }
        k->number = number;
        k->position = shown;
        k->height_m = QuantiseHeight(c.altitude_m);
        k->zone = ZoneOf(c.altitude_m);
        k->type = c.type;
        k->friendly = c.type == 1;
        k->group = c.formation > 1;
        k->primary = primary;
        k->imitated = c.target_id == "imitation";
        // The regiment's designation to this battery rides with its telecode (its own picture decides it).
        k->designated = !primary && designate_within_m_ > 0.0 && (c.position - position_).HorizontalLength() <= designate_within_m_;
        k->time_s = t;
        ApplyOverrides(*k);
    }
    void ApplyOverrides(AspdTrack& k) const {
        const auto it = overrides_.find(k.target_id);
        if (it == overrides_.end()) return;
        if (it->second.friendly >= 0) {
            k.friendly = it->second.friendly == 1;
            k.type = k.friendly ? 1 : 8;
        }
        if (it->second.group >= 0) k.group = it->second.group == 1;
        if (it->second.zone > 0) k.zone = it->second.zone;
    }

    int RegimentNumber(const std::string& id) {
        const auto it = regiment_numbers_.find(id);
        if (it != regiment_numbers_.end()) return it->second;
        const int n = static_cast<int>(regiment_numbers_.size() % 100);
        regiment_numbers_[id] = n;
        return n;
    }

    void ReceiveTelecode(double t, const std::vector<BoardContact>& regiment) {
        last_telecode_s_ = t;
        if (!(def_.telecode_take_off_s > 0.0)) {
            for (const BoardContact& c : regiment) Fix(t, Telecoded(c), false, RegimentNumber(c.target_id));
            return;
        }
        // The sending post's operator takes its targets off by hand (p19_to: semi-automatic take-off), one every
        // telecode_take_off_s: in a cycle those never sent first (nearest the post), then those sent longest ago.
        const std::size_t budget = static_cast<std::size_t>(std::max(1.0, std::floor(def_.telecode_period_s / def_.telecode_take_off_s + 1e-9)));
        const std::size_t rounds = regiment.empty() ? 1 : (regiment.size() + budget - 1) / budget;
        telecode_round_s_ = static_cast<double>(rounds) * def_.telecode_period_s;
        for (std::size_t n = 0; n < budget; ++n) {
            const BoardContact* best = nullptr;
            double best_key = 1e18;
            for (const BoardContact& c : regiment) {
                const auto it = telecode_sent_.find(c.target_id);
                if (it != telecode_sent_.end() && it->second + 1e-9 >= t) continue;  // sent this cycle
                const double key = it == telecode_sent_.end() ? -1e12 + (c.position - position_).HorizontalLength() : it->second;
                if (key < best_key) {
                    best_key = key;
                    best = &c;
                }
            }
            if (best == nullptr) break;
            telecode_sent_[best->target_id] = t;
            Fix(t, Telecoded(*best), false, RegimentNumber(best->target_id));
        }
    }
    // A target as the telecode carries it: its coordinates in the sending post's steps (p19_to).
    BoardContact Telecoded(const BoardContact& c) const {
        if (!(def_.telecode_step_m > 0.0)) return c;
        BoardContact q = c;
        q.position.x = std::round(c.position.x / def_.telecode_step_m) * def_.telecode_step_m;
        q.position.y = std::round(c.position.y / def_.telecode_step_m) * def_.telecode_step_m;
        return q;
    }

    // The regiment's radar battery reads its next report once the net is free: a target never reported first (the
    // nearest to the radar), else the one reported longest ago, each at most once a reporting period, within the
    // board's extent. A report heard goes to the plotter; the net goes on whether the post listens or not.
    void ReadAirNet(double t, const std::vector<BoardContact>& regiment, bool listening) {
        for (std::size_t i = 0; i < air_pending_.size();) {
            if (air_pending_[i].heard_s > t + 1e-9) {
                ++i;
                continue;
            }
            const bool got = listening && !(air_net_loss_ > 0.0 && Uniform() < air_net_loss_);
            spoken_.push_back({air_pending_[i], got});
            if (got) (relay_ ? held_ : heard_).push_back(air_pending_[i]);
            air_pending_.erase(air_pending_.begin() + static_cast<std::ptrdiff_t>(i));
        }
        heard_.erase(std::remove_if(heard_.begin(), heard_.end(), [&](const AirReport& r) { return t - r.heard_s > def_.air_net_period_s; }),
                     heard_.end());
        held_.erase(std::remove_if(held_.begin(), held_.end(), [&](const AirReport& r) { return t - r.heard_s > def_.air_net_period_s; }),
                    held_.end());
        if (t + 1e-9 < air_free_s_) return;
        const Vec3 src = has_air_source_ ? air_source_ : position_;
        const BoardContact* best = nullptr;
        double best_key = 1e18;
        for (const BoardContact& c : regiment) {
            if ((c.position - position_).HorizontalLength() > board_.Def().max_range_m) continue;
            const auto it = air_said_.find(c.target_id);
            if (it != air_said_.end() && t - it->second + 1e-9 < def_.air_net_period_s) continue;
            const double key = it == air_said_.end() ? -1e12 + (c.position - src).HorizontalLength() : it->second;
            if (key < best_key) {
                best_key = key;
                best = &c;
            }
        }
        if (best == nullptr) return;
        // The report: azimuth and range from the radar in its steps (degrees, kilometres), the identity and size, the
        // height in hectometres and the minute (bguir_tsp: the azimuth-range form of the air-situation report).
        const Vec3 rel = best->position - src;
        double az_deg = RadToDeg(std::atan2(rel.x, rel.y));
        if (az_deg < 0.0) az_deg += 360.0;
        const double az_step = def_.air_net_az_step_deg > 0.0 ? def_.air_net_az_step_deg : 1.0;
        const double r_step = def_.air_net_range_step_m > 0.0 ? def_.air_net_range_step_m : 1000.0;
        az_deg = std::fmod(std::round(az_deg / az_step) * az_step, 360.0);
        const double range = std::round(rel.HorizontalLength() / r_step) * r_step;
        AirReport r;
        r.contact = *best;
        r.contact.position = Shown(Vec3{src.x + range * std::sin(DegToRad(az_deg)), src.y + range * std::cos(DegToRad(az_deg)), best->position.z});
        r.number = board_.NumberFor(best->target_id);
        r.heard_s = t + def_.air_net_report_s;
        const int height_hm = static_cast<int>(std::lround(best->altitude_m / 100.0));
        const int code = best->type * 10 + (best->formation > 9 ? 9 : best->formation);
        const int minute = static_cast<int>(t / 60.0) + 1;
        char text[112];
        const int levels = static_cast<int>(def_.air_net_grid_levels);
        if (levels > 0 && has_frame_) {
            // The PVO grid's squares (pvo_grid.hpp): the plotter puts the mark at the square's centre.
            const GeoPoint g = frame_.EnuToGeodetic(best->position);
            const double lat = RadToDeg(g.lat_rad), lon = RadToDeg(g.lon_rad);
            const std::string sq = pvo_grid::Encode(lat, lon, levels);
            double clat = lat, clon = lon;
            pvo_grid::Centre(sq, std::floor(lat / pvo_grid::kBigLatDeg) * pvo_grid::kBigLatDeg,
                             std::floor(lon / pvo_grid::kBigLonDeg) * pvo_grid::kBigLonDeg, clat, clon);
            const Vec3 centre = frame_.GeodeticToEnu(GeoPoint::FromDegrees(clat, clon, g.alt_m));
            r.contact.position = Shown(Vec3{centre.x, centre.y, best->position.z});
            std::snprintf(text, sizeof(text), "%04d: square %s, %02d, height %d, minute %02d", r.number, sq.c_str(), code, height_hm, minute % 100);
        } else {
            std::snprintf(text, sizeof(text), "%04d: azimuth %03.0f, %.0f km, %02d, height %d, minute %02d", r.number, az_deg, range / 1000.0,
                          code, height_hm, minute % 100);
        }
        r.text = text;
        air_pending_.push_back(r);
        air_said_[best->target_id] = t;
        air_free_s_ = r.heard_s;
    }
    void WriteHeard(double t, std::size_t index, double speed) {
        const AirReport r = heard_[index];
        heard_.erase(heard_.begin() + static_cast<std::ptrdiff_t>(index));
        board_.PlotByHand(t, r.contact);
        operator_free_s_ = t + def_.plot_mark_s / speed;
    }

    // The primary target to take off next: one already held whose fix is oldest, else the nearest new one while fewer
    // than 12 are held. Only once a radar turn has passed since its last fix.
    const BoardContact* NextTakeOff(double t, const std::vector<BoardContact>& radar) const {
        const BoardContact* best = nullptr;
        double best_age = -1.0;
        int held = 0;
        for (const AspdTrack& k : tracks_) held += k.primary ? 1 : 0;
        double nearest = 1e18;
        const BoardContact* new_one = nullptr;
        for (const BoardContact& c : radar) {
            const AspdTrack* k = nullptr;
            for (const AspdTrack& e : tracks_) {
                if (e.target_id == c.target_id && e.primary) k = &e;
            }
            if (k != nullptr) {
                const double age = t - k->time_s;
                if (age + 1e-9 >= def_.radar_period_s && age > best_age) {
                    best_age = age;
                    best = &c;
                }
            } else if (static_cast<double>(held) < def_.semi_auto_tracks) {
                const double r = (c.position - position_).HorizontalLength();
                if (r < nearest) {
                    nearest = r;
                    new_one = &c;
                }
            }
        }
        return new_one != nullptr ? new_one : best;
    }
    void TakeOff(double t, const BoardContact& c) {
        auto it = post_numbers_.find(c.target_id);
        if (it == post_numbers_.end()) it = post_numbers_.emplace(c.target_id, static_cast<int>(post_numbers_.size() % 100)).first;
        Fix(t, c, true, it->second);
    }

    AspdDef def_;
    PlottingBoard board_;
    Vec3 position_;
    std::mt19937 rng_;
    double az_error_rad_ = 0.0;
    Vec3 offset_;
    bool oriented_ = false;
    double drift_dir_ = 0.0;
    double drift_rate_ = 0.0;
    std::vector<AspdTrack> tracks_;
    bool telecode_started_ = false;
    double next_telecode_s_ = 0.0;
    double last_telecode_s_ = -1.0;
    double operator_free_s_ = 0.0;
    double tube_free_s_ = 0.0;
    std::map<std::string, int> regiment_numbers_;
    std::map<std::string, int> post_numbers_;
    std::map<std::string, double> telecode_sent_;  // when the sending post last took each target off
    double designate_within_m_ = 0.0;
    std::map<std::string, Attributes> overrides_;   // the ДХ's characteristics, by target
    bool imitation_on_ = false;
    double imitation_t_ = 0.0;
    bool board_open_ = true;
    double telecode_round_s_ = 0.0;                // how long its round of all the targets takes
    Vec3 air_source_;
    bool has_air_source_ = false;
    LocalFrame frame_;
    bool has_frame_ = false;
    double air_free_s_ = 0.0;                      // when the air-picture net is next free
    std::map<std::string, double> air_said_;       // when each target was last reported on it
    std::vector<AirReport> air_pending_;           // being read out (heard_s: when the reading ends)
    std::vector<AirReport> held_;                  // heard by a seat on FuG-1, not yet read out to the plotter
    bool relay_ = false;
    double air_net_loss_ = 0.0;
    double Uniform() { return static_cast<double>(rng_()) / 4294967296.0; }
    std::vector<AirReport> heard_;
    std::vector<Spoken> spoken_;
};

}  // namespace simcore
