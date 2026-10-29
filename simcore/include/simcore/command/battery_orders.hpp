// simcore/command/battery_orders.hpp — the battery's orders carried by a means (roadmap G10.2-G10.5;
// docs/campaign/11_BATTERY_LIFE_PROPOSAL.md §5.5, decision G-13).
//
// The battery commander gives an order in words by a means, and it reaches a vehicle commander only by that means:
//   - the battery net, on his R-123M while it is on, warmed up, on the net's frequency and not jammed out. Said once: a
//     set that is off or still warming misses it, and he can call back only once its transmitter is ready too;
//   - flags by day and lamps by night, to a vehicle in sight, each vehicle commander passing the signal on to the next;
//   - the field telephone, where a line is laid to his vehicle;
//   - in person, within a shout;
//   - a runner on foot, who brings the repeat-back with him if nothing quicker carries it.
// He hears it, gives his crew the word (his skill tier's reaction) and repeats it back by whatever way back he has; his
// vehicle acts on it only once he has heard it (the caller: Battery::Join). The giver knows only the repeat-backs he
// hears: a silence makes him say it again, then try the next means, then send a runner (orders.hpp ChaseRule; the
// telephone skipped where no line is laid). A commander who learns of an order with his set off switches it on.
// When his vehicle has done it he reports by the same ways; what reaches the giver his radiotelephonist writes in the
// journal (the caller marks it logged). The record is the OrderBook's (orders.hpp), and the debrief reads it.
#pragma once

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/command/data.hpp"
#include "simcore/command/orders.hpp"
#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/crew_mobility.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace command {

// data/command/battery_orders.json; the runner's pace from signal_means.json's messenger.
struct BatteryOrdersDef {
    double set_warm_s = 600.0;  // an R-123M switched on receives after this (r123m_training)
    double set_tx_s = 180.0;    // and can transmit this long after that (r123m_training)
    double flags_m = 500.0;     // flag signals read by day, one vehicle to the next
    double lamps_m = 1000.0;    // lamp signals by night
    double pass_s = 5.0;        // a vehicle commander passing a signal on
    double say_s = 4.0;         // an order, a repeat-back or a report said
    double ring_s = 15.0;       // a field telephone on a direct line rung and answered
    double write_s = 20.0;      // the giver's radiotelephonist writes a line in the journal (an NPC's)
    double runner_mps = 5.0 / 3.6;
    double runner_handling_s = 120.0;
    double road_factor = 1.3;   // the way on foot over the straight line
    // In person: a shout over the quiet of the countryside (crew_mobility.hpp's earshot).
    double earshot_m = EarshotRange(EarshotDef().shout_db, EarshotDef().quiet_db, EarshotDef().needed_snr_db);
    ChaseRule chase;
};

// A vehicle commander as the order net sees him: where his vehicle is and what his means can do now (the caller keeps
// it current).
struct OrderVehicle {
    std::string vehicle;    // the battery's vehicle id
    std::string callsign;
    std::string post;       // his post in the chain of command: the order book's recipient
    std::string commander;  // his name
    double reaction_s = 1.1;       // his skill tier's
    double callout_delay_s = 0.6;
    Vec3 at;
    bool set_on = true;
    double set_on_s = -1.0e9;  // switched on then
    bool on_net = true;        // on the battery net's frequency
    bool jammed = false;
    bool may_call = true;      // radio allowed him now (on the march only for fire control: voenizdat_osa_1991 p240)
    bool line = false;         // a field telephone line laid to the giver
    // Modelled seat by seat (level C2, the player's own vehicle): what reaches him on the radio and what he says on it are
    // his seat's (Reach, SaidOnNet); the other means the net carries as for any vehicle.
    bool seats = false;
};

// A vehicle commander from the chain of command (his name and skill tier) and the skill table (his reaction and how quick
// he is to speak); a post the chain does not hold keeps the defaults (class 2's).
inline OrderVehicle CommanderFromChain(const ChainData& chain, const SkillTable& skills, const std::string& vehicle,
                                       const std::string& callsign, const std::string& post) {
    OrderVehicle v;
    v.vehicle = vehicle;
    v.callsign = callsign;
    v.post = post;
    v.commander = callsign + " commander";
    const Position* p = chain.FindPosition(post);
    if (p == nullptr) return v;
    v.commander = p->holder.name;
    const SkillTier* tier = skills.Find(p->holder.skill);
    if (tier != nullptr) {
        v.reaction_s = tier->reaction_s;
        v.callout_delay_s = tier->callout_delay_s;
    }
    return v;
}

// What happened to an order at a vehicle, for the caller: "heard", "set on", "carrying out" (his vehicle acts on it:
// Battery::Join), "repeated back", "sent again", "reported" (the giver has his report: write it), "cannot" (the giver
// has his report that he cannot, and why).
struct OrderEvent {
    double t = 0.0;
    int order = -1;
    std::string vehicle;
    std::string callsign;
    std::string what;
    std::string mean;
    std::string text;
};

class BatteryOrderNet {
public:
    BatteryOrderNet(BatteryOrdersDef def, std::string giver_post, std::string giver_callsign, double hour_at_zero)
        : def_(std::move(def)), giver_post_(std::move(giver_post)), giver_callsign_(std::move(giver_callsign)), hour_at_zero_(hour_at_zero) {}

    void Add(const OrderVehicle& v) { vehicles_.push_back(v); }
    OrderVehicle* Vehicle(const std::string& id) {
        for (OrderVehicle& v : vehicles_)
            if (v.vehicle == id) return &v;
        return nullptr;
    }
    const std::vector<OrderVehicle>& Vehicles() const { return vehicles_; }
    void SetGiverAt(const Vec3& at) { giver_at_ = at; }
    // A runner's walk between two points, seconds (the route service's way on foot); none: the straight line times the
    // relay's road factor at the messenger's pace.
    void SetWalk(std::function<double(const Vec3&, const Vec3&)> walk_s) { walk_s_ = std::move(walk_s); }
    void SetNight(bool night) { night_ = night; }
    const OrderBook& Book() const { return book_; }
    const BatteryOrdersDef& Def() const { return def_; }

    // The order said by `mean` at t to these vehicles ("radio_voice", "flags" (lamps by night), "field_telephone",
    // "in_person", "messenger"). Returns the order's index in the book.
    int Give(const std::string& kind, const std::string& words, const std::string& mean, double t, const std::vector<std::string>& to) {
        BatteryOrderRecord r;
        r.id = kind + "@" + std::to_string(static_cast<long long>(std::llround(t)));
        r.giver = giver_post_;
        r.kind = kind;
        r.words = words;
        r.given_h = Hour(t);
        r.mean = mean;
        std::vector<std::string> posts;
        for (const std::string& id : to) {
            const OrderVehicle* v = Find(id);
            if (v == nullptr) continue;
            posts.push_back(v->post);
        }
        const int order = book_.Give(r, posts);
        for (const std::string& id : to) {
            const int k = Index(id);
            if (k < 0) continue;
            Track tr;
            tr.order = order;
            tr.v = static_cast<std::size_t>(k);
            tr.words = words;
            tracks_.push_back(tr);
            Send(tracks_.back(), mean, t);
        }
        return order;
    }

    // His vehicle has done it (out of its emplacement, in position): he reports `text` by the first way back he has.
    void Done(int order, const std::string& vehicle, double t, const std::string& text) {
        Track* tr = Of(order, vehicle);
        if (tr == nullptr || tr->done_s >= 0.0 || tr->heard_s < 0.0) return;
        if (!book_.Mark(order, vehicles_[tr->v].post, OrderStep::Done, Hour(t))) return;
        tr->done_s = t;
        tr->report = text;
    }
    // He cannot carry it out (his vehicle broken down on the march, after his «К маршу готов»): he says so, and why, by
    // the first way back he has. Only an order he has heard, and once.
    void Cannot(int order, const std::string& vehicle, double t, const std::string& why, const std::string& text) {
        Track* tr = Of(order, vehicle);
        if (tr == nullptr || tr->heard_s < 0.0 || tr->cannot_s >= 0.0) return;
        tr->cannot_s = t;
        tr->cannot_why = why;
        tr->cannot_text = text;
    }
    bool CannotSaid(int order, const std::string& vehicle) const {
        const Track* tr = Of(order, vehicle);
        return tr != nullptr && tr->cannot_s >= 0.0;
    }
    // The giver's picture: the last words he had from each vehicle (a read-back, a report, a «не могу»), never what the
    // vehicle does unseen. Null: nothing yet.
    struct Word {
        double t = 0.0;
        int order = -1;
        std::string what;
        std::string mean;
        std::string text;
    };
    const Word* LastFrom(const std::string& vehicle) const {
        const auto it = last_.find(vehicle);
        return it != last_.end() ? &it->second : nullptr;
    }

    // A vehicle modelled seat by seat: its commander heard the order on the radio at `at_s` (said at `sent_s`)...
    void Reach(int order, const std::string& vehicle, double at_s, double sent_s) {
        Track* tr = Of(order, vehicle);
        if (tr == nullptr || tr->heard_s >= 0.0 || (tr->arrive_s >= 0.0 && at_s >= tr->arrive_s)) return;
        tr->arrive_s = at_s;
        tr->arrive_mean = "radio_voice";
        tr->arrive_sent_s = sent_s;
    }
    // ...and said his repeat-back (else his report of it done) on the net at t.
    void SaidOnNet(int order, const std::string& vehicle, double t, bool repeat_back) {
        Track* tr = Of(order, vehicle);
        if (tr == nullptr || tr->heard_s < 0.0) return;
        const OrderVehicle& v = vehicles_[tr->v];
        if (repeat_back) {
            if (tr->back_s >= 0.0) return;
            tr->back_s = t;
            book_.Mark(order, v.post, OrderStep::RepeatedBack, Hour(t));
            said_.push_back({t, order, v.vehicle, v.callsign, "repeated back", "radio_voice", "Есть " + tr->words});
        } else {
            if (tr->done_s < 0.0 || tr->reported_s >= 0.0) return;
            tr->reported_s = t;
            book_.Mark(order, v.post, OrderStep::Reported, Hour(t));
            said_.push_back({t, order, v.vehicle, v.callsign, "reported", "radio_voice", tr->report});
        }
        Note(said_.back());
    }
    bool RepeatedBack(int order, const std::string& vehicle) const {
        const Track* tr = Of(order, vehicle);
        return tr != nullptr && tr->back_s >= 0.0;
    }
    bool Acting(int order, const std::string& vehicle) const {
        const Track* tr = Of(order, vehicle);
        return tr != nullptr && tr->acting;
    }
    bool IsDone(int order, const std::string& vehicle) const {
        const Track* tr = Of(order, vehicle);
        return tr != nullptr && tr->done_s >= 0.0;
    }
    // The giver's radiotelephonist has written the report: the order's last step.
    void Logged(int order, const std::string& vehicle, double t) {
        const Track* tr = Of(order, vehicle);
        if (tr != nullptr) book_.Mark(order, vehicles_[tr->v].post, OrderStep::Logged, Hour(t));
    }
    bool Heard(int order, const std::string& vehicle) const {
        const Track* tr = Of(order, vehicle);
        return tr != nullptr && tr->heard_s >= 0.0;
    }
    bool Reported(int order, const std::string& vehicle) const {
        const Track* tr = Of(order, vehicle);
        return tr != nullptr && tr->reported_s >= 0.0;
    }

    std::vector<OrderEvent> Step(double t) {
        std::vector<OrderEvent> out;
        out.swap(said_);
        for (Track& tr : tracks_) {
            OrderVehicle& v = vehicles_[tr.v];
            // It reaches him: he hears it, and acts on it after his reaction.
            if (tr.heard_s < 0.0 && tr.arrive_s >= 0.0 && t + 1e-9 >= tr.arrive_s) {
                tr.heard_s = tr.arrive_s;
                tr.heard_mean = tr.arrive_mean;
                tr.heard_sent_s = tr.arrive_sent_s;
                book_.Mark(tr.order, v.post, OrderStep::Reached, Hour(tr.heard_s), tr.heard_mean);
                book_.Mark(tr.order, v.post, OrderStep::Heard, Hour(tr.heard_s));
                out.push_back({tr.heard_s, tr.order, v.vehicle, v.callsign, "heard", tr.heard_mean, tr.words});
                if (!v.set_on) {  // he learns of an order with his set off: on with it, to be on the net again
                    v.set_on = true;
                    v.set_on_s = tr.heard_s + v.reaction_s;
                    out.push_back({v.set_on_s, tr.order, v.vehicle, v.callsign, "set on", "", "R-123M on: warming up"});
                }
            }
            if (tr.heard_s >= 0.0 && !tr.acting && t + 1e-9 >= tr.heard_s + v.reaction_s) {
                tr.acting = true;
                book_.Mark(tr.order, v.post, OrderStep::CarryingOut, Hour(tr.heard_s + v.reaction_s));
                out.push_back({tr.heard_s + v.reaction_s, tr.order, v.vehicle, v.callsign, "carrying out", tr.heard_mean, tr.words});
            }
            // The repeat-back, by the first way back he has.
            if (tr.heard_s >= 0.0 && tr.back_s < 0.0) {
                std::string mean;
                const double at = WayBack(tr, tr.heard_s + v.callout_delay_s, t, true, mean);
                if (at >= 0.0) {
                    tr.back_s = at;
                    book_.Mark(tr.order, v.post, OrderStep::RepeatedBack, Hour(at));
                    out.push_back({at, tr.order, v.vehicle, v.callsign, "repeated back", mean, "Есть " + tr.words});
                    Note(out.back());
                }
            }
            // That he cannot, and why.
            if (tr.cannot_s >= 0.0 && tr.cannot_reported_s < 0.0) {
                std::string mean;
                const double at = WayBack(tr, tr.cannot_s + v.callout_delay_s, t, false, mean);
                if (at >= 0.0) {
                    tr.cannot_reported_s = at;
                    book_.Cannot(tr.order, v.post, Hour(at), tr.cannot_why);
                    out.push_back({at, tr.order, v.vehicle, v.callsign, "cannot", mean, tr.cannot_text});
                    Note(out.back());
                }
            }
            // The report of it done.
            if (tr.done_s >= 0.0 && tr.reported_s < 0.0) {
                std::string mean;
                const double at = WayBack(tr, tr.done_s + v.callout_delay_s, t, false, mean);
                if (at >= 0.0) {
                    tr.reported_s = at;
                    book_.Mark(tr.order, v.post, OrderStep::Reported, Hour(at));
                    out.push_back({at, tr.order, v.vehicle, v.callsign, "reported", mean, tr.report});
                    Note(out.back());
                }
            }
        }
        // A silence: say it again, then the next means, then a runner.
        for (Track& tr : tracks_) {
            if (tr.back_s >= 0.0 || tr.cannot_reported_s >= 0.0) continue;
            const OrderVehicle& v = vehicles_[tr.v];
            ChaseRule rule = def_.chase;
            rule.next_means.erase(std::remove_if(rule.next_means.begin(), rule.next_means.end(),
                                                 [&](const std::string& m) { return m == "field_telephone" && !v.line; }),
                                  rule.next_means.end());
            for (const Chase& c : book_.Due(tr.order, Hour(t), rule)) {
                if (c.post != v.post) continue;
                book_.Resend(tr.order, v.post, c.h, c.mean);
                Send(tr, c.mean, t);
                out.push_back({t, tr.order, v.vehicle, v.callsign, "sent again", c.mean,
                               c.mean == "messenger" ? "a runner to " + v.callsign : tr.words});
            }
        }
        std::sort(out.begin(), out.end(), [](const OrderEvent& a, const OrderEvent& b) { return a.t < b.t; });
        return out;
    }

    // The debrief's lines for each order given (orders.hpp's), the session starting at `start_hour`.
    std::vector<std::string> Debrief(double start_hour) const {
        std::vector<std::string> out;
        for (std::size_t i = 0; i < book_.Orders().size(); ++i) {
            for (const std::string& s : book_.Debrief(static_cast<int>(i), start_hour)) out.push_back(s);
        }
        return out;
    }

private:
    struct Track {
        int order = -1;
        std::size_t v = 0;
        std::string words;
        double arrive_s = -1.0;  // the earliest sending on its way reaches him then
        std::string arrive_mean;
        double arrive_sent_s = 0.0;
        double heard_s = -1.0;
        std::string heard_mean;
        double heard_sent_s = 0.0;
        bool acting = false;
        double back_s = -1.0;
        double done_s = -1.0;
        std::string report;
        double reported_s = -1.0;
        double cannot_s = -1.0;
        std::string cannot_why;
        std::string cannot_text;
        double cannot_reported_s = -1.0;
    };

    double Hour(double t) const { return hour_at_zero_ + t / 3600.0; }
    void Note(const OrderEvent& e) {
        Word& w = last_[e.vehicle];
        if (w.order >= 0 && w.t > e.t) return;
        w = {e.t, e.order, e.what, e.mean, e.text};
    }
    static double Dist(const Vec3& a, const Vec3& b) { return std::hypot(a.x - b.x, a.y - b.y); }
    int Index(const std::string& id) const {
        for (std::size_t k = 0; k < vehicles_.size(); ++k)
            if (vehicles_[k].vehicle == id) return static_cast<int>(k);
        return -1;
    }
    const OrderVehicle* Find(const std::string& id) const {
        const int k = Index(id);
        return k < 0 ? nullptr : &vehicles_[static_cast<std::size_t>(k)];
    }
    Track* Of(int order, const std::string& vehicle) {
        for (Track& tr : tracks_)
            if (tr.order == order && vehicles_[tr.v].vehicle == vehicle) return &tr;
        return nullptr;
    }
    const Track* Of(int order, const std::string& vehicle) const {
        for (const Track& tr : tracks_)
            if (tr.order == order && vehicles_[tr.v].vehicle == vehicle) return &tr;
        return nullptr;
    }

    bool Receives(const OrderVehicle& v, double t) const {
        return v.set_on && t + 1e-9 >= v.set_on_s + def_.set_warm_s && v.on_net && !v.jammed;
    }
    double TransmitsFrom(const OrderVehicle& v) const { return v.set_on_s + def_.set_warm_s + def_.set_tx_s; }
    double RunnerS(const OrderVehicle& v) const {
        const double walk = walk_s_ ? walk_s_(giver_at_, v.at) : -1.0;
        return def_.runner_handling_s + (walk >= 0.0 ? walk : Dist(giver_at_, v.at) * def_.road_factor / def_.runner_mps);
    }
    // A flag (lamp) signal shown by the giver at t: when each vehicle reads it, passed on from one in sight to the next.
    std::vector<double> SignalTimes(double t) const {
        const double range = night_ ? def_.lamps_m : def_.flags_m;
        std::vector<double> at(vehicles_.size(), -1.0);
        std::vector<bool> done(vehicles_.size(), false);
        for (std::size_t k = 0; k < vehicles_.size(); ++k)
            if (Dist(giver_at_, vehicles_[k].at) <= range) at[k] = t + def_.say_s;
        for (;;) {
            int best = -1;
            for (std::size_t k = 0; k < vehicles_.size(); ++k)
                if (!done[k] && at[k] >= 0.0 && (best < 0 || at[k] < at[static_cast<std::size_t>(best)])) best = static_cast<int>(k);
            if (best < 0) break;
            const std::size_t b = static_cast<std::size_t>(best);
            done[b] = true;
            for (std::size_t k = 0; k < vehicles_.size(); ++k) {
                if (done[k] || Dist(vehicles_[b].at, vehicles_[k].at) > range) continue;
                const double next = at[b] + def_.pass_s;
                if (at[k] < 0.0 || next < at[k]) at[k] = next;
            }
        }
        return at;
    }

    // A sending by `mean` at t: when it reaches him, if it does.
    void Send(Track& tr, const std::string& mean, double t) {
        const OrderVehicle& v = vehicles_[tr.v];
        double at = -1.0;
        if (mean == "radio_voice") {
            if (!v.seats && Receives(v, t)) at = t + def_.say_s;
        } else if (mean == "flags" || mean == "lamps") {
            at = SignalTimes(t)[tr.v];
        } else if (mean == "field_telephone") {
            if (v.line) at = t + def_.ring_s + def_.say_s;
        } else if (mean == "in_person") {
            if (Dist(giver_at_, v.at) <= def_.earshot_m) at = t + def_.say_s;
        } else if (mean == "messenger") {
            at = t + RunnerS(v);
        }
        if (at < 0.0 || (tr.arrive_s >= 0.0 && at >= tr.arrive_s)) return;
        tr.arrive_s = at;
        tr.arrive_mean = mean == "flags" && night_ ? "lamps" : mean;
        tr.arrive_sent_s = t;
    }

    // The first time from `from` (and no later than now) that his words reach the giver, and by what: the battery net
    // once his set can transmit and radio is his to use (the giver listens on it all the time); the signal back the way it came; the telephone; a shout;
    // a repeat-back carried by the runner who brought the order. -1: not yet.
    double WayBack(const Track& tr, double from, double t, bool repeat_back, std::string& mean) const {
        const OrderVehicle& v = vehicles_[tr.v];
        double best = -1.0;
        auto offer = [&](double at, const char* m) {
            if (at <= t + 1e-9 && (best < 0.0 || at < best)) {
                best = at;
                mean = m;
            }
        };
        if (!v.seats && v.set_on && v.on_net && !v.jammed && v.may_call) offer(std::max(from, TransmitsFrom(v)) + def_.say_s, "radio_voice");
        if (tr.heard_mean == "flags" || tr.heard_mean == "lamps") {
            if (repeat_back) {
                offer(from + (tr.heard_s - tr.heard_sent_s), tr.heard_mean.c_str());
            } else {
                const double seen = SignalTimes(from)[tr.v];
                if (seen >= 0.0) offer(seen, night_ ? "lamps" : "flags");
            }
        }
        // An order taken on the telephone is repeated back on the same call: no ring.
        const bool on_the_call = repeat_back && tr.heard_mean == "field_telephone";
        if (v.line) offer(from + (on_the_call ? 0.0 : def_.ring_s) + def_.say_s, "field_telephone");
        if (Dist(giver_at_, v.at) <= def_.earshot_m) offer(from + def_.say_s, "in_person");
        if (repeat_back && tr.heard_mean == "messenger") offer(from + RunnerS(v), "messenger");
        return best;
    }

    BatteryOrdersDef def_;
    std::string giver_post_;
    std::string giver_callsign_;
    double hour_at_zero_ = 0.0;
    Vec3 giver_at_;
    std::function<double(const Vec3&, const Vec3&)> walk_s_;
    bool night_ = false;
    std::vector<OrderVehicle> vehicles_;
    std::vector<Track> tracks_;
    OrderBook book_;
    std::vector<OrderEvent> said_;  // a seat's words on the net since the last step
    std::map<std::string, Word> last_;
};

namespace detail {

class BatteryOrdersLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, BatteryOrdersDef& d) {
        if (!Keys(root, "", {"orders_id", "display_name", "status", "notes", "orders", "chase"})) return false;
        const json::Value* b = root.Find("orders");
        if (b == nullptr || !b->Is(json::Type::Object)) return Fail("orders", "required, an object");
        const std::pair<const char*, double*> fields[] = {{"set_warm_s", &d.set_warm_s}, {"set_tx_s", &d.set_tx_s},
                                                          {"flags_m", &d.flags_m},       {"lamps_m", &d.lamps_m},
                                                          {"pass_s", &d.pass_s},         {"say_s", &d.say_s},
                                                          {"ring_s", &d.ring_s},         {"write_s", &d.write_s}};
        for (const auto& f : fields) {
            if (!Value(*b, Join("orders", f.first), f.first, *f.second)) return false;
        }
        const json::Value* c = root.Find("chase");
        if (c == nullptr || !c->Is(json::Type::Object)) return Fail("chase", "required, an object");
        if (!Keys(*c, "chase", {"wait_s", "repeats", "next_means", "source", "confidence", "notes"}) || !Provenance(*c, "chase")) {
            return message_.empty() ? Fail("chase", "expected {wait_s, repeats, next_means, source, confidence}") : false;
        }
        const json::Value* w = c->Find("wait_s");
        const json::Value* r = c->Find("repeats");
        const json::Value* n = c->Find("next_means");
        if (w == nullptr || !w->Is(json::Type::Number) || !(w->number > 0.0)) return Fail("chase.wait_s", "a number > 0");
        if (r == nullptr || !r->Is(json::Type::Number) || r->number < 0.0) return Fail("chase.repeats", "a number >= 0");
        if (n == nullptr || !n->Is(json::Type::Array)) return Fail("chase.next_means", "an array of means");
        d.chase.wait_s = w->number;
        d.chase.repeats = static_cast<int>(r->number);
        d.chase.next_means.clear();
        for (const json::Value& m : n->items) {
            if (!m.Is(json::Type::String)) return Fail("chase.next_means", "an array of means");
            d.chase.next_means.push_back(m.string);
        }
        return true;
    }

private:
    bool Value(const json::Value& parent, const std::string& p, const char* key, double& out) {
        const json::Value* v = parent.Find(key);
        if (v == nullptr) return Fail(p, "required");
        if (!v->Is(json::Type::Object) || !Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) {
            return message_.empty() ? Fail(p, "expected {value, source, confidence}") : false;
        }
        const json::Value* n = v->Find("value");
        if (n == nullptr || !n->Is(json::Type::Number) || !(n->number > 0.0)) return Fail(Join(p, "value"), "a number > 0");
        out = n->number;
        return true;
    }
};

inline bool ReadOrdersText(const std::string& path, std::string& out, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream s;
    s << file.rdbuf();
    out = s.str();
    return true;
}

}  // namespace detail

// data/command/battery_orders.json, and the runner's pace and the way on foot from signal_means.json's messenger and
// relay rules (paths under the data directory).
inline bool LoadBatteryOrders(const std::string& data_dir, BatteryOrdersDef& out, std::string* error = nullptr) {
    const std::string base = data_dir.empty() ? std::string() : data_dir + "/";
    std::string text, means_text;
    if (!detail::ReadOrdersText(base + "command/battery_orders.json", text, error)) return false;
    if (!detail::ReadOrdersText(base + "command/signal_means.json", means_text, error)) return false;
    json::Value root, means;
    std::string perr;
    if (!json::Parse(text, root, &perr) || !json::Parse(means_text, means, &perr)) {
        if (error != nullptr) *error = "battery_orders: " + perr;
        return false;
    }
    BatteryOrdersDef d;
    detail::BatteryOrdersLoader loader;
    if (!loader.Load(root, d)) {
        if (error != nullptr) *error = "battery_orders.json: " + loader.Message();
        return false;
    }
    const json::Value* list = means.Find("means");
    if (list != nullptr && list->Is(json::Type::Array)) {
        for (const json::Value& m : list->items) {
            const json::Value* id = m.Find("id");
            if (id == nullptr || !id->Is(json::Type::String) || id->string != "messenger") continue;
            const json::Value* speed = m.Find("speed_kmh");
            const json::Value* handling = m.Find("handling_s");
            if (speed != nullptr && speed->Is(json::Type::Number) && speed->number > 0.0) d.runner_mps = speed->number / 3.6;
            if (handling != nullptr && handling->Is(json::Type::Number)) d.runner_handling_s = handling->number;
        }
    }
    const json::Value* relay = means.Find("relay");
    const json::Value* road = relay != nullptr ? relay->Find("road_factor") : nullptr;
    if (road != nullptr && road->Is(json::Type::Number) && road->number >= 1.0) d.road_factor = road->number;
    out = d;
    return true;
}

}  // namespace command
}  // namespace simcore
