// simcore/session_deliveries.hpp — deliveries from the chain of command played inside a session (roadmap P1,
// docs/command/06_INTEGRATION.md §6).
//
// Our side's information pipeline (command/info_net.hpp) decides who learns what, when, from whom and by what means. In a
// session those deliveries happen in the world instead of on a screen: the regiment's duty officer's voice on the
// battery net, the vehicle commander telling his crew on the intercom, a runner or a despatch rider seen coming over the
// field who hands a paper in at the hatch. A scenario lists them ("deliveries"); the runner plays each at its time on the
// channel its means has, to the seats that channel reaches, and puts the paper into the map case of the seat it was for.
//
// A carrier (someone walking over, a runner, a despatch rider, the courier's car) is a Visitor: he sets out so as to
// arrive at the delivery's time, waits while the paper is handed over and read, and goes back the way he came. The
// game draws him (a basic model) where he is.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "simcore/vec3.hpp"

namespace simcore {

// How a delivery comes (a scenario's "means").
enum class DeliveryMeans { Radio, Telephone, WarningNet, Siren, Intercom, InPerson, Runner, DespatchRider, Courier, Teleprinter, Post, Broadcast };

inline const char* DeliveryMeansName(DeliveryMeans m) {
    switch (m) {
        case DeliveryMeans::Radio: return "radio";
        case DeliveryMeans::Telephone: return "telephone";
        case DeliveryMeans::WarningNet: return "warning_net";
        case DeliveryMeans::Siren: return "siren";
        case DeliveryMeans::Intercom: return "intercom";
        case DeliveryMeans::InPerson: return "in_person";
        case DeliveryMeans::Runner: return "runner";
        case DeliveryMeans::DespatchRider: return "despatch_rider";
        case DeliveryMeans::Courier: return "courier";
        case DeliveryMeans::Teleprinter: return "teleprinter";
        case DeliveryMeans::Post: return "post";
        case DeliveryMeans::Broadcast: return "broadcast";
    }
    return "?";
}

inline bool ParseDeliveryMeans(const std::string& s, DeliveryMeans& out) {
    for (DeliveryMeans m : {DeliveryMeans::Radio, DeliveryMeans::Telephone, DeliveryMeans::WarningNet, DeliveryMeans::Siren,
                            DeliveryMeans::Intercom, DeliveryMeans::InPerson, DeliveryMeans::Runner, DeliveryMeans::DespatchRider,
                            DeliveryMeans::Courier, DeliveryMeans::Teleprinter, DeliveryMeans::Post, DeliveryMeans::Broadcast}) {
        if (s == DeliveryMeansName(m)) {
            out = m;
            return true;
        }
    }
    return false;
}

// Someone comes over the ground with it, and is seen coming.
inline bool Carried(DeliveryMeans m) {
    return m == DeliveryMeans::InPerson || m == DeliveryMeans::Runner || m == DeliveryMeans::DespatchRider || m == DeliveryMeans::Courier;
}

// One delivery in a scenario.
struct DeliveryDef {
    std::string id;                 // the fact's key, or the scenario's own name for it
    double after_s = 0.0;           // when it reaches the listener (a carrier: when he arrives)
    std::string to;                 // the station it is for ("" whoever its channel reaches; the paper to the commander)
    DeliveryMeans means = DeliveryMeans::Radio;
    std::string net;                // radio: the net it is said on ("battery net", "regiment net")
    std::string from;               // who says it: "Captain Pavel Liska (duty officer, regiment command post)"
    std::string words;              // what is said
    std::string paper_title;        // a paper handed over ("" none)
    std::string paper_text;
    bool air_warning = false;       // the battery takes the air threat from it
    double from_bearing_deg = 0.0;  // a carrier comes from there (true, clockwise from north, from the vehicle)
    bool has_azimuth = false;       // it gives a raid's direction: those who hear it have it in their picture (P4)
    double azimuth_deg = 0.0;       // true, from the vehicle
};

// A paper a seat holds: in its map case from the time it was handed over.
struct HeldPaper {
    std::string id;
    std::string station;
    double time_s = 0.0;
    std::string title;
    std::string text;
    std::string from;
};

// How each carrier comes. Game values: a man walks over from the battery's command post, a runner covers a few
// hundred metres at a messenger's 5 km/h, a despatch rider and the courier's car come up the road (docs/command/03_MEANS.md).
struct CarrierKind {
    double speed_mps = 1.4;
    double distance_m = 250.0;  // where he is first in sight
    const char* model = "soldier";
};

inline CarrierKind CarrierOf(DeliveryMeans m) {
    switch (m) {
        case DeliveryMeans::Runner: return {1.4, 300.0, "soldier"};
        case DeliveryMeans::DespatchRider: return {11.1, 2000.0, "motorcycle"};
        case DeliveryMeans::Courier: return {9.7, 2000.0, "uaz469"};
        default: return {1.4, 250.0, "soldier"};
    }
}

// Someone on the ground bringing a delivery: seen coming, at the vehicle, going back.
struct Visitor {
    static constexpr double kStopM = 6.0;   // he stops this far from the vehicle
    static constexpr double kWaitS = 60.0;  // handing over, waiting while it is read
    std::string id;                         // "visitor:<delivery id>"
    std::string model;                      // its basic model (data/scenery/basic_models.json)
    std::string from;                       // who it is
    double bearing_deg = 0.0;
    double distance_m = 0.0;  // from the vehicle where he sets out (or is first in the world)
    double depart_s = 0.0;    // he is in the world from then
    double arrive_s = 0.0;
    double gone_s = 0.0;      // and out of it (back where he came from)
    double speed_mps = 1.4;

    bool Present(double t) const { return t >= depart_s && t < gone_s; }
    // Metres from the vehicle along his bearing at t.
    double Out(double t) const {
        if (t <= arrive_s) return std::max(kStopM, kStopM + (distance_m - kStopM) - (t - depart_s) * speed_mps);
        if (t <= arrive_s + kWaitS) return kStopM;
        return std::min(distance_m, kStopM + (t - arrive_s - kWaitS) * speed_mps);
    }
    // Where he is relative to the vehicle (east, north, m) and which way he faces (true, deg).
    Vec3 Offset(double t) const {
        const double b = bearing_deg * 3.14159265358979323846 / 180.0;
        const double r = Out(t);
        return Vec3{r * std::sin(b), r * std::cos(b), 0.0};
    }
    double HeadingDeg(double t) const {
        const double toward = std::fmod(bearing_deg + 180.0, 360.0);
        return t <= arrive_s + kWaitS ? toward : bearing_deg;
    }
};

inline Visitor VisitorFor(const DeliveryDef& d) {
    const CarrierKind k = CarrierOf(d.means);
    Visitor v;
    v.id = "visitor:" + d.id;
    v.model = k.model;
    v.from = d.from;
    v.bearing_deg = d.from_bearing_deg;
    v.speed_mps = k.speed_mps;
    v.arrive_s = d.after_s;
    const double travel = (k.distance_m - Visitor::kStopM) / k.speed_mps;
    if (d.after_s >= travel) {
        v.distance_m = k.distance_m;
        v.depart_s = d.after_s - travel;
    } else {  // under way when the session starts: he is first seen where he is by then
        v.distance_m = Visitor::kStopM + d.after_s * k.speed_mps;
        v.depart_s = 0.0;
    }
    v.gone_s = v.arrive_s + Visitor::kWaitS + (v.distance_m - Visitor::kStopM) / k.speed_mps;
    return v;
}

// A session's deliveries: which come due, who is on the way with one, and the papers each seat holds.
class SessionDeliveries {
public:
    void Set(std::vector<DeliveryDef> defs) {
        defs_ = std::move(defs);
        std::stable_sort(defs_.begin(), defs_.end(), [](const DeliveryDef& a, const DeliveryDef& b) { return a.after_s < b.after_s; });
        next_ = 0;
        papers_.clear();
        visitors_.clear();
        for (const DeliveryDef& d : defs_) {
            if (Carried(d.means)) visitors_.push_back(VisitorFor(d));
        }
    }

    // The deliveries due by t not yet given, in order (each once).
    std::vector<const DeliveryDef*> Due(double t) {
        std::vector<const DeliveryDef*> out;
        while (next_ < defs_.size() && defs_[next_].after_s <= t + 1e-9) out.push_back(&defs_[next_++]);
        return out;
    }

    // The paper of a delivery handed to a station at t.
    void Hand(const DeliveryDef& d, const std::string& station, double t) {
        if (d.paper_title.empty()) return;
        papers_.push_back({d.id, station, t, d.paper_title, d.paper_text, d.from});
    }

    const std::vector<DeliveryDef>& Defs() const { return defs_; }
    const std::vector<Visitor>& Visitors() const { return visitors_; }
    // The papers a station holds ("" every station's), in the order they came.
    std::vector<HeldPaper> MapCase(const std::string& station) const {
        std::vector<HeldPaper> out;
        for (const HeldPaper& p : papers_) {
            if (station.empty() || p.station == station) out.push_back(p);
        }
        return out;
    }

private:
    std::vector<DeliveryDef> defs_;
    std::size_t next_ = 0;
    std::vector<HeldPaper> papers_;
    std::vector<Visitor> visitors_;
};

// Where a session's deliveries come from when the scenario is written for a campaign (the raid builder asks it):
// our side's pipeline, for the vehicle the session plays (command/scenes.hpp).
struct DeliverySource {
    virtual ~DeliverySource() = default;
    // The deliveries to the session's seats from campaign hour from_h to to_h, after_s counted from from_h; the warning
    // of the raid `package` marked air_warning (others are only said).
    virtual std::vector<DeliveryDef> Between(int package, double from_h, double to_h) const = 0;
    // When the session's vehicle learned of the raid `package` (campaign hours); < -1e17 never by to_h.
    virtual double AirWarnedAt(int package, double to_h) const = 0;
};

}  // namespace simcore
