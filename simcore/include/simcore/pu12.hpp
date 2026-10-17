// simcore/pu12.hpp — the battery command post's plotting board (PU-12M; roadmap 7.1).
//
// The regiment's radars feed the battery command post an air picture, which the crew writes on a glass plotting
// board once a minute (samsim_osa p20). Each target gets a mark: a four-digit number (the tactical number of the
// radar battery that found it, then a sequence number), its height in hectometres over a two-digit code (the tens
// digit is the target's type, e.g. 1 friend, 7 practice target, 9 no IFF; the ones digit is the formation size),
// and a minute stamp. Positions carry the PU-12M's navigation error (flak11: up to 5 degrees and 3 km), drawn once
// per board from the scenario's seed.
//
// The board is what the vehicle crews aim by when their radars are silent: a minute old at worst, and off by the
// navigation error. The battery commander gives each target inside `designation_range_m` to the vehicle by voice
// radio, azimuth and range, one message at a time (roadmap 7.2): the operator's delay, then the message, then the
// vehicle commander passes it on to his crew. The vehicle only knows a designation from the moment it is passed on.
// It only reads the world; it decides nothing.
//
// The board is also what any other early-warning source writes (c2_sources.hpp, roadmap E4): an EWR site on a voice net,
// a neighbouring battery, an observation post. Such a source has its own board; the battery's PU-12 stays the main one.
// A board can sit away from the battery (`SetSource`), share its voice channel with other boards (`SetChannel`: one
// message at a time on the net), have its messages delivered or lost by someone else (`SetGated`, `ResolveMessages`),
// and draw a fresh error on every plot (`plot_error_*`, a radar's measurement noise) on top of its fixed navigation error.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/geo.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

struct PlottingBoardDef {
    std::string id;
    std::string display_name;
    double update_period_s = 60.0;
    double max_range_m = 200000.0;      // the outer range ring: targets beyond aren't plotted
    std::vector<double> range_rings_m;  // for the display
    int radar_battery_number = 0;       // first two digits of every target number
    double navigation_error_deg = 0.0;  // up to this much in azimuth ...
    double navigation_error_m = 0.0;    // ... and this much in position
    double designation_range_m = 0.0;   // targets nearer than this are designated to the vehicle (0 = none are)
    double operator_delay_s = 0.0;      // the radio operator's delay before a message
    double message_s = 0.0;             // reading one designation out: number, azimuth, range
    double pass_on_s = 0.0;             // the vehicle commander passing it on to his crew
    double plot_error_deg = 0.0;        // a fresh error on each plot, up to this much in azimuth (a radar's measurement noise)
    double plot_error_m = 0.0;          // ... and this much in position
};

// A radio net's voice channel: one message at a time. Boards on the same net share one (c2_sources.hpp).
struct VoiceChannel {
    double free_s = 0.0;  // when the channel is next free
};

// One plotted position of one target.
struct BoardMark {
    std::string target_id;  // the scenario's id (who it really is; the crew sees `number`)
    int number = 0;         // e.g. 6201
    int stamp = 0;          // the minute it was plotted: 1, 2, ...
    double time_s = 0.0;
    Vec3 position;          // where the board puts it (ENU, with the navigation error)
    int height_hm = 0;      // height above sea level in hectometres
    int type = 9;           // tens digit of the code: 0 jamming, 1 friend, ..., 7 practice, 8 enemy, 9 no IFF
    int formation = 1;      // ones digit: aircraft in the formation

    // The code under the height, as written on the board (e.g. 71: practice target, one aircraft).
    int Code() const { return type * 10 + (formation > 9 ? 9 : formation); }
};

// A target as the regiment's radars report it.
struct BoardContact {
    std::string target_id;
    Vec3 position;     // true, ENU
    double altitude_m = 0.0;
    double height_agl_m = -1.0;  // above the ground, for a radar horizon (< 0: take altitude_m)
    int type = 9;
    int formation = 1;
};

namespace detail {

class BoardLoader : public ConsoleLoader {
public:
    bool LoadBoard(const json::Value& root, PlottingBoardDef& d) {
        if (!Keys(root, "", {"board_id", "display_name", "status", "notes", "board"}) ||
            !RequireString(root, "", "board_id", d.id) || !OptionalString(root, "", "display_name", d.display_name)) {
            return false;
        }
        const json::Value* b = root.Find("board");
        if (b == nullptr || !b->Is(json::Type::Object)) return Fail("board", "required, an object");
        return LoadBoardBody(*b, d);
    }

    // The "board" object itself: every number cited. `cite_path_` names where it sits, for the error messages.
    bool LoadBoardBody(const json::Value& body, PlottingBoardDef& d) {
        const json::Value* b = &body;
        if (!Keys(*b, cite_path_, {"update_period_s", "max_range_m", "range_rings_m", "radar_battery_number",
                                "navigation_error_deg", "navigation_error_m", "designation_range_m", "operator_delay_s",
                                "message_s", "pass_on_s", "plot_error_deg", "plot_error_m"})) {
            return false;
        }
        for (const auto& f : {std::make_pair("update_period_s", &d.update_period_s), std::make_pair("max_range_m", &d.max_range_m),
                              std::make_pair("navigation_error_deg", &d.navigation_error_deg),
                              std::make_pair("navigation_error_m", &d.navigation_error_m),
                              std::make_pair("designation_range_m", &d.designation_range_m),
                              std::make_pair("operator_delay_s", &d.operator_delay_s),
                              std::make_pair("message_s", &d.message_s), std::make_pair("pass_on_s", &d.pass_on_s)}) {
            if (!Cited(*b, f.first, *f.second)) return false;
        }
        for (const auto& f : {std::make_pair("plot_error_deg", &d.plot_error_deg), std::make_pair("plot_error_m", &d.plot_error_m)}) {
            if (b->Find(f.first) != nullptr && !Cited(*b, f.first, *f.second)) return false;  // optional: none
        }
        double number = 0.0;
        if (!Cited(*b, "radar_battery_number", number)) return false;
        if (number < 0.0 || number > 99.0 || number != std::floor(number)) {
            return Fail(Join(cite_path_, "radar_battery_number"), "a two-digit tactical number");
        }
        d.radar_battery_number = static_cast<int>(number);
        if (!(d.update_period_s > 0.0)) return Fail(Join(cite_path_, "update_period_s"), "must be > 0");
        if (const json::Value* rings = b->Find("range_rings_m")) {
            const json::Value* list = rings->Find("value");
            if (list == nullptr || !list->Is(json::Type::Array)) return Fail(Join(cite_path_, "range_rings_m.value"), "expected a list");
            if (!Provenance(*rings, Join(cite_path_, "range_rings_m"))) return false;
            for (const json::Value& r : list->items) {
                if (!r.Is(json::Type::Number) || !(r.number > 0.0)) return Fail(Join(cite_path_, "range_rings_m"), "distances > 0");
                d.range_rings_m.push_back(r.number);
            }
        }
        return true;
    }

protected:
    std::string cite_path_ = "board";  // where the values sit, for the error messages

    // A cited value: {"value": n, "source": "...", "confidence": "..."}.
    bool Cited(const json::Value& g, const char* key, double& out) {
        const std::string p = Join(cite_path_, key);
        const json::Value* v = g.Find(key);
        if (v == nullptr) return Fail(p, "required");
        if (!v->Is(json::Type::Object)) return Fail(p, "expected {\"value\", \"source\", \"confidence\"}");
        if (!Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) return false;
        const json::Value* n = v->Find("value");
        if (n == nullptr || !n->Is(json::Type::Number)) return Fail(Join(p, "value"), "expected a number");
        out = n->number;
        if (out < 0.0) return Fail(Join(p, "value"), "must be >= 0");
        return true;
    }
};

}  // namespace detail

inline bool LoadPlottingBoardFile(const std::string& path, PlottingBoardDef& out, std::string* error = nullptr) {
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
    PlottingBoardDef d;
    detail::BoardLoader loader;
    if (!loader.LoadBoard(root, d)) {
        if (error != nullptr) *error = path + ": " + loader.Message();
        return false;
    }
    out = std::move(d);
    return true;
}

class PlottingBoard {
public:
    // `centre`: the battery's position (ranges and designations are from it). `seed` draws the navigation error.
    PlottingBoard(PlottingBoardDef def, const Vec3& centre, unsigned seed) : def_(std::move(def)), centre_(centre) {
        std::mt19937 rng(seed ^ 0x9e3779b9u);
        auto u = [&rng] { return static_cast<double>(rng()) / 4294967296.0; };
        az_error_rad_ = DegToRad(def_.navigation_error_deg) * (2.0 * u() - 1.0);
        const double r = def_.navigation_error_m * std::sqrt(u());
        const double a = 2.0 * kPi * u();
        offset_ = Vec3{r * std::sin(a), r * std::cos(a), 0.0};
        noise_.seed(seed ^ 0x85ebca6bu);
    }

    // The board is written away from the battery: ranges to cover and the error's rotation are about `at` (a radar
    // site, an observation post), the designation range still about the battery (the centre).
    void SetSource(const Vec3& at) {
        source_ = at;
        has_source_ = true;
    }
    // Share a voice channel with other boards (the net's). Null: the board's own.
    void SetChannel(VoiceChannel* channel) { channel_ = channel; }
    // Gated: a message is only heard once someone has resolved it with `ResolveMessages` (a link that can lose it).
    void SetGated(bool gated) { gated_ = gated; }

    // Plot the contacts if a minute has passed since the last time (the first plot is at the first call), and queue
    // a voice designation for each target in range. Returns true when it plotted.
    bool Update(double t, const std::vector<BoardContact>& contacts) {
        if (plotted_ && t + 1e-9 < next_s_) return false;
        plotted_ = true;
        next_s_ = t + def_.update_period_s;
        ++stamp_;
        const Vec3& origin = has_source_ ? source_ : centre_;
        for (const BoardContact& c : contacts) {
            const Vec3 rel = c.position - origin;
            if (rel.HorizontalLength() > def_.max_range_m) continue;
            BoardMark m;
            m.target_id = c.target_id;
            m.number = NumberFor(c.target_id);
            m.stamp = stamp_;
            m.time_s = t;
            m.position = origin + Rotate(Vec3{rel.x, rel.y, 0.0}, az_error_rad_) + offset_ + Vec3{0.0, 0.0, rel.z};
            if (def_.plot_error_deg > 0.0 || def_.plot_error_m > 0.0) {
                auto un = [this] { return static_cast<double>(noise_()) / 4294967296.0; };
                const Vec3 flat = Rotate(Vec3{m.position.x - origin.x, m.position.y - origin.y, 0.0},
                                         DegToRad(def_.plot_error_deg) * (2.0 * un() - 1.0));
                const double r = def_.plot_error_m * std::sqrt(un());
                const double a = 2.0 * kPi * un();
                m.position = Vec3{origin.x + flat.x + r * std::sin(a), origin.y + flat.y + r * std::cos(a), m.position.z};
            }
            m.height_hm = static_cast<int>(std::lround(c.altitude_m / 100.0));
            m.type = c.type;
            m.formation = c.formation;
            marks_.push_back(m);
            const Vec3 to_battery = c.position - centre_;
            if (def_.designation_range_m > 0.0 && to_battery.HorizontalLength() <= def_.designation_range_m) Queue(t, m);
        }
        last_plot_s_ = t;
        return true;
    }

    // Is a plot due at t? (`Update` plots only then.)
    bool Due(double t) const { return !plotted_ || t + 1e-9 >= next_s_; }

    // One mark written by hand (the PU-12M's own plotter, command_post.hpp): `c.position` is where the post's own
    // picture puts the target (its navigation error already in it); the plotter's own error is added. Stamped with the
    // minute it is written. Nothing is read out on a net: designations from it are the commander's (P4).
    BoardMark PlotByHand(double t, const BoardContact& c) {
        const int minute = static_cast<int>(t / 60.0) + 1;
        if (minute > stamp_) stamp_ = minute;
        plotted_ = true;
        BoardMark m;
        m.target_id = c.target_id;
        m.number = NumberFor(c.target_id);
        m.stamp = minute;
        m.time_s = t;
        m.position = c.position;
        if (def_.plot_error_m > 0.0) {
            auto un = [this] { return static_cast<double>(noise_()) / 4294967296.0; };
            const double r = def_.plot_error_m * std::sqrt(un());
            const double a = 2.0 * kPi * un();
            m.position = Vec3{m.position.x + r * std::sin(a), m.position.y + r * std::cos(a), m.position.z};
        }
        m.height_hm = static_cast<int>(std::lround(c.altitude_m / 100.0));
        m.type = c.type;
        m.formation = c.formation;
        marks_.push_back(m);
        last_plot_s_ = t;
        return m;
    }

    // Extra time the commander needs to pass a designation on (no intercom: he shouts over the cabin noise). Applies to
    // the messages queued from now on.
    void SetPassOnExtra(double seconds) { pass_on_extra_s_ = seconds > 0.0 ? seconds : 0.0; }

    // Designations the vehicle's crew has been told since the last call (oldest first).
    std::vector<BoardMark> PollHeard(double t) {
        std::vector<BoardMark> out;
        for (Message& m : messages_) {
            if (!m.reported && Heard(m, t)) {
                m.reported = true;
                out.push_back(m.mark);
            }
        }
        return out;
    }

    // The crew was not listening (the vehicle had the radio on another net): what was passed on up to t is never heard.
    void DiscardHeard(double t) {
        for (Message& m : messages_) {
            if (m.known_s <= t + 1e-9) m.reported = true;
        }
    }

    // Messages whose voice transmission ended by t and whose fate nobody has decided yet: `deliver(mark, ended_s)`
    // says whether the crew gets it (a gated board only; see SetGated).
    // `deliver(mark, ended_s, index)` returns 1 delivered, 0 lost, -1 not known yet (asked again next time).
    template <typename F>
    void ResolveMessages(double t, F&& deliver) {
        for (std::size_t i = 0; i < messages_.size(); ++i) {
            Message& m = messages_[i];
            if (m.state != Message::State::Pending || m.air_end_s > t + 1e-9) continue;
            const int r = deliver(m.mark, m.air_end_s, i);
            if (r >= 0) m.state = r > 0 ? Message::State::Delivered : Message::State::Lost;
        }
    }

    // Messages queued since the last call, with the times their voice transmission starts and ends (for a radio that
    // carries them, c2_sources.hpp). `index` identifies the message to `ResolveMessages`.
    struct Sent {
        std::size_t index = 0;
        double start_s = 0.0;
        double end_s = 0.0;
    };
    std::vector<Sent> TakeUnsent() {
        std::vector<Sent> out;
        for (std::size_t i = 0; i < messages_.size(); ++i) {
            if (!messages_[i].sent) {
                messages_[i].sent = true;
                out.push_back({i, messages_[i].start_s, messages_[i].air_end_s});
            }
        }
        return out;
    }

    // The source went quiet at t: what it had queued but not yet begun to read out is never sent, and the channel is
    // free again (if this board was the last to use it).
    void CancelQueued(double t) {
        double first_start = -1.0;
        for (Message& m : messages_) {
            if (m.state != Message::State::Pending || m.start_s <= t + 1e-9) continue;
            if (first_start < 0.0 || m.start_s < first_start) first_start = m.start_s;
            m.state = Message::State::Lost;
        }
        VoiceChannel* ch = Channel();
        if (first_start >= 0.0 && ch->free_s >= first_start) {
            const double reopen = first_start - def_.operator_delay_s;
            ch->free_s = reopen > t ? reopen : t;
        }
    }

    int LostCount() const {
        int n = 0;
        for (const Message& m : messages_) n += m.state == Message::State::Lost ? 1 : 0;
        return n;
    }
    double LastPlotS() const { return last_plot_s_; }

    // What the vehicle knows about each target at time t: the latest designation passed on to the crew.
    std::vector<BoardMark> Known(double t) const {
        std::vector<BoardMark> out;
        for (const Message& m : messages_) {
            if (!Heard(m, t)) continue;
            bool replaced = false;
            for (BoardMark& k : out) {
                if (k.target_id == m.mark.target_id) {
                    k = m.mark;  // later messages are later in the list
                    replaced = true;
                }
            }
            if (!replaced) out.push_back(m.mark);
        }
        return out;
    }
    std::size_t MessageCount() const { return messages_.size(); }

    const std::vector<BoardMark>& Marks() const { return marks_; }
    const PlottingBoardDef& Def() const { return def_; }
    int Stamp() const { return stamp_; }

    // The latest mark of each target plotted at the last update (what the crew reads off the board now).
    std::vector<BoardMark> Current() const {
        std::vector<BoardMark> out;
        for (const BoardMark& m : marks_) {
            if (m.stamp == stamp_) out.push_back(m);
        }
        return out;
    }

    // A designation as the battery commander gives it by voice: azimuth (degrees from north and mils) and range.
    struct Designation {
        double azimuth_deg = 0.0;
        int azimuth_mils = 0;  // of 60-00
        double range_m = 0.0;
        double elevation_deg = 0.0;
    };
    Designation DesignationFor(const BoardMark& m, const Vec3& from) const { return Designate(m, from); }
    static Designation Designate(const BoardMark& m, const Vec3& from) {
        Designation d;
        const Vec3 rel = m.position - from;
        double az = RadToDeg(AzimuthOf(rel));
        if (az < 0.0) az += 360.0;
        d.azimuth_deg = az;
        d.azimuth_mils = static_cast<int>(std::lround(az / 360.0 * 6000.0)) % 6000;
        d.range_m = rel.HorizontalLength();
        d.elevation_deg = RadToDeg(std::atan2(rel.z, d.range_m > 1.0 ? d.range_m : 1.0));
        return d;
    }

    // "6201 16/71 at 35 km, 15-50" as the board reads (number, height / code, range from the centre, azimuth in mils).
    std::string Describe(const BoardMark& m) const {
        const Designation d = DesignationFor(m, centre_);
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%04d %d/%02d at %.0f km, %02d-%02d", m.number, m.height_hm, m.Code(),
                      d.range_m / 1000.0, d.azimuth_mils / 100, d.azimuth_mils % 100);
        return buf;
    }

    void SetCentre(const Vec3& centre) { centre_ = centre; }

private:
    static Vec3 Rotate(const Vec3& v, double rad) {  // clockwise, as an azimuth error
        const double c = std::cos(rad), s = std::sin(rad);
        return Vec3{v.x * c + v.y * s, -v.x * s + v.y * c, v.z};
    }

    struct Message {
        enum class State { Pending, Delivered, Lost };
        BoardMark mark;
        double start_s = 0.0;    // the operator begins reading it out
        double air_end_s = 0.0;  // the voice transmission ends
        double known_s = 0.0;    // when the vehicle's crew has it
        bool reported = false;
        bool sent = false;       // handed to a radio (TakeUnsent)
        State state = State::Pending;  // an ungated board treats Pending as delivered
    };

    // Does the crew have this message at time t?
    bool Heard(const Message& m, double t) const {
        if (t + 1e-9 < m.known_s) return false;
        if (m.state == Message::State::Lost) return false;
        return m.state == Message::State::Delivered || !gated_;
    }

    double pass_on_extra_s_ = 0.0;
    VoiceChannel* Channel() { return channel_ != nullptr ? channel_ : &own_channel_; }

    // The radio carries one message at a time: each waits for the one before it.
    void Queue(double t, const BoardMark& m) {
        VoiceChannel* ch = Channel();
        const double start = (t > ch->free_s ? t : ch->free_s) + def_.operator_delay_s;
        ch->free_s = start + def_.message_s;
        Message msg;
        msg.mark = m;
        msg.start_s = start;
        msg.air_end_s = ch->free_s;
        msg.known_s = ch->free_s + def_.pass_on_s + pass_on_extra_s_;
        messages_.push_back(msg);
    }

    int NumberFor(const std::string& id) {
        for (const auto& n : numbers_) {
            if (n.first == id) return n.second;
        }
        const int number = def_.radar_battery_number * 100 + static_cast<int>(numbers_.size() % 99) + 1;
        numbers_.emplace_back(id, number);
        return number;
    }

    PlottingBoardDef def_;
    Vec3 centre_;
    double az_error_rad_ = 0.0;
    Vec3 offset_;
    bool plotted_ = false;
    double next_s_ = 0.0;
    int stamp_ = 0;
    std::vector<BoardMark> marks_;
    std::vector<Message> messages_;
    VoiceChannel own_channel_;
    VoiceChannel* channel_ = nullptr;
    bool gated_ = false;
    Vec3 source_;
    bool has_source_ = false;
    double last_plot_s_ = -1e9;
    std::mt19937 noise_;
    std::vector<std::pair<std::string, int>> numbers_;
};

}  // namespace simcore
