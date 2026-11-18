// simcore/command/scenes.hpp — how a delivery of the pipeline happens in the world (roadmap P1,
// docs/command/06_INTEGRATION.md §6).
//
// InfoNet says who told whom, when, by what means and where. A Scene says how that looks and sounds to the listener:
// the duty officer's voice on the radio, the telephone ringing in the PU-12M, the R-311's speaker, a runner or a
// despatch rider coming over the field and handing in a paper, the chief of staff reading the summary at the evening
// briefing. The papers a person has been handed are his map case, each dated and with who gave it. A briefing is one
// scene: every fact told there, in the order the briefer told them.
//
// SessionSource is the pipeline seen from a session: the deliveries to the posts sitting in one vehicle of it
// (Position::seat), as the scenario's "deliveries" (session_deliveries.hpp), and when that vehicle learned of a raid.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "simcore/command/data.hpp"
#include "simcore/command/info_net.hpp"
#include "simcore/session_deliveries.hpp"

namespace simcore {
namespace command {

enum class SceneKind {
    Seen,            // the person saw it himself
    InPerson,        // told face to face (a visitor walks up, or they were together)
    Runner,          // a messenger on foot
    DespatchRider,   // a motorcycle comes up the road
    Courier,         // the courier's run stops here
    FieldTelephone,  // the TA-57 rings
    CityTelephone,   // the town's exchange puts a call through
    Radio,           // a voice on the net
    WarningNet,      // the R-311's speaker: the division's early-warning broadcast
    Teleprinter,     // a tape at the regiment's CP
    Siren,           // the town's sirens heard across the fields
    FieldPost,       // letters and newspapers with the sergeant major's run
    StateRadio       // the state radio's news bulletin
};

inline const char* SceneKindName(SceneKind k) {
    switch (k) {
        case SceneKind::Seen: return "seen";
        case SceneKind::InPerson: return "in_person";
        case SceneKind::Runner: return "runner";
        case SceneKind::DespatchRider: return "despatch_rider";
        case SceneKind::Courier: return "courier";
        case SceneKind::FieldTelephone: return "field_telephone";
        case SceneKind::CityTelephone: return "city_telephone";
        case SceneKind::Radio: return "radio";
        case SceneKind::WarningNet: return "warning_net";
        case SceneKind::Teleprinter: return "teleprinter";
        case SceneKind::Siren: return "siren";
        case SceneKind::FieldPost: return "field_post";
        case SceneKind::StateRadio: return "state_radio";
    }
    return "?";
}

inline SceneKind SceneKindOf(const Delivery& d, const ChainData& data) {
    if (d.from == -1) return SceneKind::Seen;
    if (d.from == -2) return SceneKind::StateRadio;
    if (d.mean < 0) return SceneKind::InPerson;
    switch (data.means[static_cast<std::size_t>(d.mean)].kind) {
        case MeanKind::InPerson: return SceneKind::InPerson;
        case MeanKind::Messenger: return SceneKind::Runner;
        case MeanKind::DespatchRider: return SceneKind::DespatchRider;
        case MeanKind::Courier: return SceneKind::Courier;
        case MeanKind::FieldTelephone: return SceneKind::FieldTelephone;
        case MeanKind::CityTelephone: return SceneKind::CityTelephone;
        case MeanKind::RadioVoice: return SceneKind::Radio;
        case MeanKind::WarningReceiver: return SceneKind::WarningNet;
        case MeanKind::Teleprinter: return SceneKind::Teleprinter;
        case MeanKind::Siren: return SceneKind::Siren;
        case MeanKind::FieldPost: return SceneKind::FieldPost;
        case MeanKind::Broadcast: return SceneKind::StateRadio;
    }
    return SceneKind::InPerson;
}

// What a paper of this kind is called in the map case.
inline std::string PaperTitle(FactKind k) {
    switch (k) {
        case FactKind::Order: return "Combat order";
        case FactKind::Intsum: return "Intelligence summary";
        case FactKind::EnemyOfficer: return "Notes on the enemy's officers";
        case FactKind::EnemyKnowsBattery: return "The regiment's estimate: what the enemy knows of the battery";
        case FactKind::RaidSeen: return "Raids seen by the warning net";
        case FactKind::RaidResult: return "Combat report";
        case FactKind::OwnDamage: return "Damage report";
        case FactKind::Prisoner: return "Prisoner report";
        case FactKind::BatteryNews: return "The battery's news";
        case FactKind::Press: return "Newspaper";
        case FactKind::Mail: return "Letter";
        case FactKind::Intercept: return "Intercept report";
        case FactKind::OwnAirNotice: return "Notice of our aircraft";
        case FactKind::War: return "Announcement";
        case FactKind::AirWarning: return "Air-raid warning";
        case FactKind::Rumour: return "Note";
    }
    return "Message";
}

// The words said: an urgent air warning opens with its code word ("Воздух": utin_2018 §3.1) unless the text has it.
inline std::string SpokenWords(const Fact& f) {
    const std::string told = f.Told();
    if (f.kind == FactKind::AirWarning && told.find("Воздух") == std::string::npos && told.find("Vozdukh") == std::string::npos) {
        return "Vozdukh! " + told;
    }
    return told;
}

struct Scene {
    int fact = -1;
    std::string fact_key;
    FactKind kind = FactKind::Intsum;
    double h = 0.0;          // when it happens: the listener hears it, or the carrier arrives
    SceneKind scene = SceneKind::InPerson;
    int teller = -1;         // the post that tells (-1 none: seen, the state radio)
    std::string teller_label;
    int listener = -1;
    std::string place;       // where the listener is
    std::string from_place;  // where the teller is (a carrier sets out from there)
    Vec3 from_pos, to_pos;   // the two places (theatre frame, m)
    std::string words;
    bool paper = false;      // a paper is handed over with it
    std::string paper_title;
};

inline Scene SceneOf(const InfoNet& net, const Delivery& d) {
    const ChainData& data = net.Data();
    const Fact& f = net.Facts()[static_cast<std::size_t>(d.fact)];
    Scene s;
    s.fact = d.fact;
    s.fact_key = f.key;
    s.kind = f.kind;
    s.h = d.h;
    s.scene = SceneKindOf(d, data);
    s.teller = d.from >= 0 ? d.from : -1;
    if (s.teller >= 0) s.teller_label = data.positions[static_cast<std::size_t>(s.teller)].Label();
    s.listener = d.position;
    s.place = d.place;
    const int lp = data.PlaceIndex(d.place);
    s.to_pos = net.PlacePos(lp, d.h);
    if (s.teller >= 0) {
        const int tp = net.PlaceOf(s.teller, d.h);
        if (tp >= 0) s.from_place = data.places[static_cast<std::size_t>(tp)].id;
        s.from_pos = net.PlacePos(tp, d.h);
    }
    s.words = SpokenWords(f);
    // Handed over: a paper that came in full by hand, or any written message a carrier brings.
    const bool by_hand = s.scene == SceneKind::InPerson || s.scene == SceneKind::Runner || s.scene == SceneKind::DespatchRider ||
                         s.scene == SceneKind::Courier || s.scene == SceneKind::Teleprinter || s.scene == SceneKind::FieldPost;
    const bool carrier = s.scene == SceneKind::Runner || s.scene == SceneKind::DespatchRider || s.scene == SceneKind::Courier ||
                         s.scene == SceneKind::Teleprinter || s.scene == SceneKind::FieldPost;
    s.paper = by_hand && d.full && (f.document || carrier);
    if (s.paper) s.paper_title = PaperTitle(f.kind);
    return s;
}

// A paper a person holds, from the pipeline: dated, and who gave it.
struct Paper {
    std::string fact_key;
    FactKind kind = FactKind::Intsum;
    std::string title;
    std::string text;
    std::string from;      // the teller's label ("" the person wrote it or saw it)
    double written_h = 0;  // the fact's hour
    double received_h = 0; // when it reached the map case
};

// The papers in a person's map case by now, in the order they came: every document he has in full (by hand, or a
// voice message whose paper followed), and the written messages carriers brought.
inline std::vector<Paper> MapCase(const InfoNet& net, int position) {
    std::vector<Paper> out;
    for (const Delivery& d : net.KnownBy(position)) {
        const Fact& f = net.Facts()[static_cast<std::size_t>(d.fact)];
        if (!d.full) continue;
        const Scene s = SceneOf(net, d);
        const bool paper = f.document || s.paper;
        if (!paper || d.from == -1) continue;  // what he saw himself he writes down himself: not a paper handed to him
        Paper p;
        p.fact_key = f.key;
        p.kind = f.kind;
        p.title = PaperTitle(f.kind);
        p.text = f.text;
        p.from = d.from >= 0 ? net.Data().positions[static_cast<std::size_t>(d.from)].Label() : std::string();
        p.written_h = f.h;
        p.received_h = d.full_h >= 0.0 ? d.full_h : d.h;
        if (p.received_h <= net.Now() + 1e-9) out.push_back(p);
    }
    std::stable_sort(out.begin(), out.end(), [](const Paper& a, const Paper& b) { return a.received_h < b.received_h; });
    return out;
}

// A briefing the person attended, as one scene: who held it, where, and what was told there in order.
struct BriefingScene {
    std::string routine;
    std::string name;
    std::string place;
    double start_h = 0.0;
    double end_h = 0.0;
    int leader = -1;
    std::vector<Scene> items;
};

inline std::vector<BriefingScene> Briefings(const InfoNet& net, int position) {
    std::vector<BriefingScene> out;
    if (position < 0) return out;
    const ChainData& data = net.Data();
    const std::vector<Delivery> known = net.KnownBy(position);
    if (known.empty()) return out;
    const double first = known.front().h, last = known.back().h;
    for (const Routine& r : data.routines) {
        if (r.kind != RoutineKind::Briefing) continue;
        for (double s : net.RoutineTimes(r, first - r.duration_h, last)) {
            BriefingScene b;
            b.routine = r.id;
            b.name = r.name;
            b.place = r.place;
            b.start_h = s;
            b.end_h = s + r.duration_h;
            b.leader = data.PositionIndex(r.by);
            for (const Delivery& d : known) {
                if (d.from < 0 || d.mean < 0 || data.means[static_cast<std::size_t>(d.mean)].kind != MeanKind::InPerson) continue;
                if (d.place != r.place || d.h < s - 1e-9 || d.h > s + r.duration_h + 0.05) continue;
                b.items.push_back(SceneOf(net, d));
            }
            if (!b.items.empty()) out.push_back(b);
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const BriefingScene& a, const BriefingScene& b) { return a.start_h < b.start_h; });
    return out;
}

// The vehicle and station of a post's seat ("osa:commander").
inline bool SplitSeat(const std::string& seat, std::string& vehicle, std::string& station) {
    const std::size_t c = seat.find(':');
    if (c == std::string::npos) return false;
    vehicle = seat.substr(0, c);
    station = seat.substr(c + 1);
    return true;
}

// A scene as a session plays it.
inline DeliveryDef DeliveryOf(const InfoNet& net, const Scene& s, const std::string& station, double from_h) {
    const ChainData& data = net.Data();
    DeliveryDef d;
    d.id = s.fact_key + "@" + data.positions[static_cast<std::size_t>(s.listener)].id;
    d.after_s = std::max(0.0, (s.h - from_h) * 3600.0);
    d.to = station;
    d.from = s.teller_label;
    d.words = s.words;
    if (s.paper) {
        d.paper_title = s.paper_title;
        d.paper_text = net.Facts()[static_cast<std::size_t>(s.fact)].text;
    }
    d.air_warning = s.kind == FactKind::AirWarning;
    // A crewmate is at home here; anyone else came over from his own post (a visit, a briefing's officer).
    const Position* teller = s.teller >= 0 ? &data.positions[static_cast<std::size_t>(s.teller)] : nullptr;
    const bool crewmate = teller != nullptr && teller->place == s.place;
    Vec3 from = s.from_pos;
    if (teller != nullptr && s.from_place == s.place && !crewmate) from = net.PlacePos(data.PlaceIndex(teller->place), s.h);
    const double de = from.x - s.to_pos.x, dn = from.y - s.to_pos.y;
    d.from_bearing_deg = (de == 0.0 && dn == 0.0) ? 0.0 : std::fmod(std::atan2(de, dn) * 180.0 / 3.14159265358979323846 + 360.0, 360.0);
    const int teller_level = s.teller >= 0 ? data.positions[static_cast<std::size_t>(s.teller)].level : 9;
    switch (s.scene) {
        case SceneKind::Radio:
            d.means = DeliveryMeans::Radio;
            d.net = teller_level <= 2 ? "regiment net" : "battery net";
            break;
        case SceneKind::FieldTelephone:
        case SceneKind::CityTelephone: d.means = DeliveryMeans::Telephone; break;
        case SceneKind::WarningNet: d.means = DeliveryMeans::WarningNet; break;
        case SceneKind::Siren: d.means = DeliveryMeans::Siren; break;
        case SceneKind::Runner: d.means = DeliveryMeans::Runner; break;
        case SceneKind::DespatchRider: d.means = DeliveryMeans::DespatchRider; break;
        case SceneKind::Courier: d.means = DeliveryMeans::Courier; break;
        case SceneKind::Teleprinter: d.means = DeliveryMeans::Teleprinter; break;
        case SceneKind::FieldPost: d.means = DeliveryMeans::Post; break;
        case SceneKind::StateRadio: d.means = DeliveryMeans::Broadcast; break;
        case SceneKind::Seen:
        case SceneKind::InPerson:
            // A crewmate in the vehicle: on the intercom. Anyone else walks over.
            d.means = crewmate ? DeliveryMeans::Intercom : DeliveryMeans::InPerson;
            break;
    }
    return d;
}

// The pipeline seen from one vehicle of a session: the deliveries to the posts seated in it (roadmap P1).
class SessionSource : public DeliverySource {
public:
    // `net` must outlive the source; `vehicle` names the seats ("osa", "post").
    SessionSource(const InfoNet& net, std::string vehicle) : net_(net), vehicle_(std::move(vehicle)) {
        const ChainData& data = net_.Data();
        for (std::size_t i = 0; i < data.positions.size(); ++i) {
            std::string v, st;
            if (SplitSeat(data.positions[i].seat, v, st) && v == vehicle_) seats_.push_back({static_cast<int>(i), st});
        }
    }

    std::vector<DeliveryDef> Between(int package, double from_h, double to_h) const override {
        std::vector<std::pair<double, DeliveryDef>> all;
        const std::string raid = "package:" + std::to_string(package);
        for (const auto& seat : seats_) {
            for (const Delivery& d : net_.PreviewBetween(seat.first, from_h, to_h)) {
                if (d.from == -1) continue;  // seen himself: nothing to play
                const Scene s = SceneOf(net_, d);
                DeliveryDef def = DeliveryOf(net_, s, seat.second, from_h);
                def.air_warning = s.kind == FactKind::AirWarning && net_.Facts()[static_cast<std::size_t>(d.fact)].subject == raid;
                all.push_back({d.h, def});
            }
        }
        std::stable_sort(all.begin(), all.end(), [](const std::pair<double, DeliveryDef>& a, const std::pair<double, DeliveryDef>& b) {
            return a.first < b.first;
        });
        // One telling heard by several of the vehicle's people (the commander to his crew on the intercom) is said once.
        std::vector<DeliveryDef> out;
        for (auto& p : all) {
            DeliveryDef& d = p.second;
            DeliveryDef* same = nullptr;
            for (DeliveryDef& o : out) {
                if (o.means == d.means && o.net == d.net && o.from == d.from && o.words == d.words && std::fabs(o.after_s - d.after_s) < 1.0 &&
                    o.paper_title.empty() && d.paper_title.empty()) {
                    same = &o;
                }
            }
            if (same != nullptr) {
                same->to.clear();
                same->air_warning = same->air_warning || d.air_warning;
            } else {
                out.push_back(std::move(d));
            }
        }
        return out;
    }

    double AirWarnedAt(int package, double to_h) const override {
        const int f = net_.FindFact(FactKind::AirWarning, "package:" + std::to_string(package));
        double best = -1e18;
        if (f < 0) return best;
        for (const auto& seat : seats_) {
            Delivery d;
            if (net_.PreviewDelivery(f, seat.first, to_h, d) && (best < -1e17 || d.h < best)) best = d.h;
        }
        return best;
    }

    bool Empty() const { return seats_.empty(); }

private:
    const InfoNet& net_;
    std::string vehicle_;
    std::vector<std::pair<int, std::string>> seats_;
};

// The post that sits at a session's seat ("osa:operator"); -1 none.
inline int PositionAtSeat(const ChainData& data, const std::string& seat) {
    for (std::size_t i = 0; i < data.positions.size(); ++i) {
        if (data.positions[i].seat == seat) return static_cast<int>(i);
    }
    return -1;
}

// The pipeline as it runs with the player's character at a session's seat: `net` with that seat's post as the player's
// (the commander then relays to his crew, and the player passes nothing on). False, and `net` unchanged: no post sits there.
inline bool PlayFromSeat(InfoNet& net, const std::string& seat) {
    const int p = PositionAtSeat(net.Data(), seat);
    if (p < 0) return false;
    net.SetPlayer(net.Data().positions[static_cast<std::size_t>(p)].id);
    return true;
}

}  // namespace command
}  // namespace simcore
