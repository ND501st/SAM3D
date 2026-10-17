// simcore/command_post.hpp — the PU-12M battery command post as the player's own vehicle (docs/PU12_DEVELOPMENT.md P3).
//
// The post's chain of information (docs/research/PU12_RESEARCH_2026-10.md):
//   * The regiment's command post sends its air picture by telecode on the R-407 net (FuG-5) every cycle. The post
//     receives it while its ASPD-U is working and it is within the net's reach (data/command/aspd_u.json).
//   * Or a radar cabled to the post gives the primary picture, which the operator tracks semi-automatically: up to 12
//     targets, 2-3 s to take each off (ru_pu12_text).
//   * The ASPD-U shows the picture on the IT-45 tube: up to 100 tracks, each with its number, its height in 0.2 km steps
//     and zone H1-H3, and its speed as a vector in 12.5 m/s steps (AspdTrack, Scope).
//   * The operator-plotter writes the tube's targets on the glass board by hand, one mark at a time (the board's
//     cadence is his work, not a constant). The battery commander gives targets out from the board and the tube (P4).
// Everything the post shows carries its navigation error: the largest published (5 degrees, 3 km, flak11) until the post
// is oriented with the aiming circle, then small; on the move it grows with the distance driven (the KP-4: 1 %).
// The crew, the console's subsystems and the scenario decide the inputs; the runner feeds them (scenario_runner.hpp).
#pragma once

#include <algorithm>
#include <cmath>
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
                               "unoriented_az_error_deg", "unoriented_position_error_m"})) {
            return false;
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
    int type = 9;           // the board's code tens digit as sent (1 friend, 7 practice, 8 enemy, 9 no IFF)
    bool friendly = false;
    bool group = false;
    bool primary = false;   // from a radar cabled to the post (else the regiment's telecode)
    double time_s = 0.0;    // last fix
};

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

    // ---- The regiment's net: within the R-407's reach of the regiment's post and not off the air.
    bool TelecodeInReach(const PostRegiment& r, double t) const {
        for (const C2Outage& o : r.outages) {
            if (t + 1e-9 >= o.from_s && t < o.to_s) return false;
        }
        if (!r.has_position) return true;
        return (Vec3{r.position.x, r.position.y, 0.0} - Vec3{position_.x, position_.y, 0.0}).Length() <= def_.telecode_range_m;
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
        bool operator_free = false;  // the operator-plotter at his place, not busy with a drill
        double operator_speed = 1.0; // his tier's speed at handwork (1 = a trained operator)
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
        // A track not refreshed for three of its cycles goes off the tube: the telecode's, or the operator's round of
        // the semi-automatic tracks (12 targets at 2-3 s each).
        const double telecode_stale = 3.0 * def_.telecode_period_s;
        const double primary_stale = 3.0 * std::max(def_.radar_period_s, def_.semi_auto_tracks * def_.take_off_s);
        tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                                     [&](const AspdTrack& k) { return t - k.time_s > (k.primary ? primary_stale : telecode_stale); }),
                      tracks_.end());
        // The operator's hands: semi-automatic take-off of the primary picture first, then the board.
        const double speed = in.operator_speed > 0.0 ? in.operator_speed : 1.0;
        if (!in.operator_free || t + 1e-9 < operator_free_s_) return;
        if (in.primary) {
            if (const BoardContact* c = NextTakeOff(t, radar)) {
                TakeOff(t, *c);
                operator_free_s_ = t + def_.take_off_s / speed;
                return;
            }
        }
        if (const AspdTrack* k = NextToPlot()) Write(t, *k, speed);
    }

    // The plotter (a player) writes the track with this ASPD-U number on the board. False if no track has it or he is
    // still writing the last one.
    bool PlotTrack(double t, int number, double speed) {
        if (t + 1e-9 < operator_free_s_) return false;
        for (const AspdTrack& k : tracks_) {
            if (k.number != number) continue;
            Write(t, k, speed > 0.0 ? speed : 1.0);
            return true;
        }
        return false;
    }
    // When the operator's hands are next free.
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
    };
    std::vector<ScopeMark> Scope(double scale_m) const {
        std::vector<ScopeMark> out;
        if (!(scale_m > 0.0)) return out;
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
        k->time_s = t;
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
        for (const BoardContact& c : regiment) Fix(t, c, false, RegimentNumber(c.target_id));
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

    void Write(double t, const AspdTrack& k, double speed) {
        BoardContact c;
        c.target_id = k.target_id;
        c.position = k.position;
        c.altitude_m = k.height_m;
        c.type = k.type;
        c.formation = k.group ? 2 : 1;
        board_.PlotByHand(t, c);
        plotted_[k.target_id] = t;
        operator_free_s_ = t + def_.plot_mark_s / speed;
    }

    // The track to write on the board next: one never written (the nearest first), else the one written longest ago
    // that has had a fix since.
    const AspdTrack* NextToPlot() const {
        const AspdTrack* best = nullptr;
        double best_key = 1e18;
        for (const AspdTrack& k : tracks_) {
            const auto it = plotted_.find(k.target_id);
            double key;
            if (it == plotted_.end()) {
                key = -1e12 + (k.position - position_).HorizontalLength();  // never written: before any other
            } else if (k.time_s > it->second + 1e-9) {
                key = it->second;
            } else {
                continue;
            }
            if (key < best_key) {
                best_key = key;
                best = &k;
            }
        }
        return best;
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
    std::map<std::string, int> regiment_numbers_;
    std::map<std::string, int> post_numbers_;
    std::map<std::string, double> plotted_;
};

}  // namespace simcore
