// simcore/campaign/stocks.hpp — the battery's missiles from the rear to the launchers (roadmap G5;
// docs/campaign/11_BATTERY_LIFE_PROPOSAL.md §5.6).
//
// Stocks are things in places: the missiles in each launcher, on each TZM, tested and untested at the regiment's technical
// battery by the Horka siding (data/campaign/logistics.json). The technical battery tests what the trains bring at its
// station's rate; a train from the rear takes its rail route's time, and with a bridge down on it, the time round or
// until the line is open. The TZMs load the launchers at the battery, then run to the siding for more on the road net's
// route, load as tested missiles come, and drive back: a delivery is a mover with an NPC crew (the TZM and its crew are
// not playable).
//
// What the battery commander knows is his ledger: what the radiotelephonist logged as launched and what the TZMs'
// commanders reported loading, never the stocks themselves. The NPC asks the regiment for missiles by it (a game rule),
// and the answer is an order: the TZMs' run, recorded as a battery order from given to reported (command/orders.hpp). A
// launch logged twice makes his ledger short by one, and his request one too many.
//
// In a session (roadmap 17.4) one TZM's run is a mover on the session's real roads (TzmRun): its chief plans on the
// bridges he was told were down when he got the order, finds the others when he comes to them, stops, plans again from
// where he stands, and loads at the technical battery (TechnicalBattery) as its station tests.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "simcore/command/orders.hpp"
#include "simcore/console_data.hpp"
#include "simcore/json.hpp"
#include "simcore/nav/march.hpp"
#include "simcore/nav/movers.hpp"

namespace simcore {
namespace campaign {

struct LogisticsValues {
    double launcher_missiles = 6.0;
    double tzm_missiles = 12.0;
    double reload_launcher_s = 390.0;
    double tests_per_h = 6.5;
    double load_tzm_s_per_missile = 150.0;
    double request_when_short = 6.0;
    double request_answer_s = 1800.0;
    double train_kmh = 40.0;
    double train_missiles = 48.0;
};

namespace detail {

class LogisticsLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, LogisticsValues& out) {
        if (!Keys(root, "", {"logistics_id", "status", "notes", "source", "confidence", "values"}) || !Provenance(root, "")) return false;
        const json::Value* values = root.Find("values");
        if (values == nullptr) return Fail("values", "required");
        const std::pair<const char*, double*> each[] = {
            {"launcher_missiles", &out.launcher_missiles}, {"tzm_missiles", &out.tzm_missiles},
            {"reload_launcher_s", &out.reload_launcher_s}, {"tests_per_h", &out.tests_per_h},
            {"load_tzm_s_per_missile", &out.load_tzm_s_per_missile}, {"request_when_short", &out.request_when_short},
            {"request_answer_s", &out.request_answer_s},   {"train_kmh", &out.train_kmh},
            {"train_missiles", &out.train_missiles}};
        if (!Keys(*values, "values", {"launcher_missiles", "tzm_missiles", "reload_launcher_s", "tests_per_h", "load_tzm_s_per_missile",
                                      "request_when_short", "request_answer_s", "train_kmh", "train_missiles"})) {
            return false;
        }
        for (const auto& e : each) {
            const std::string p = Join("values", e.first);
            const json::Value* v = values->Find(e.first);
            if (v == nullptr) return Fail(p, "required");
            if (!Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) return false;
            const json::Value* n = v->Find("value");
            if (n == nullptr || !n->Is(json::Type::Number) || !(n->number > 0.0)) return Fail(Join(p, "value"), "a number > 0");
            *e.second = n->number;
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadLogisticsValues(const std::string& data_dir, const std::string& rel, LogisticsValues& out, std::string* error = nullptr) {
    std::string text;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text)) {
        if (error != nullptr) *error = "logistics: cannot read " + rel;
        return false;
    }
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "logistics: " + e;
        return false;
    }
    detail::LogisticsLoader loader;
    if (!loader.Load(root, out)) {
        if (error != nullptr) *error = "logistics: " + loader.Message();
        return false;
    }
    return true;
}

// What the battery commander reckons the battery has: from the journal's launches and the TZMs' reports.
struct Ledger {
    std::vector<int> launchers;
    std::vector<int> tzms;
    int Total() const {
        int n = 0;
        for (int k : launchers) n += k;
        for (int k : tzms) n += k;
        return n;
    }
};

struct MissileRequest {
    double t = 0.0;
    int missiles = 0;    // what his ledger says the battery lacks
    double answer_s = 0.0;
};

struct StockEvent {
    double t = 0.0;
    std::string who;
    std::string text;
};

// The regiment's technical battery by the siding: the trains from the rear bring untested missiles, its station tests
// them at its rate, and the TZMs' crane takes the tested ones.
class TechnicalBattery {
public:
    void Init(const LogisticsValues& v, int tested, int untested) {
        v_ = v;
        tested_ = tested;
        untested_ = untested;
        testing_ = 0.0;
        trains_.clear();
        train_in_.clear();
        last_s_ = 0.0;
    }
    // The rear's train: it arrives after its rail route's time (`rail_s` < 0: no way through).
    void TrainDue(double depart_s, double rail_s) {
        if (rail_s < 0.0) return;
        trains_.push_back(depart_s + rail_s);
    }
    // To time t: the trains in (how many came this step), then the station tests what it has at its rate.
    int Step(double t) {
        const double dt = std::max(0.0, t - last_s_);
        last_s_ = t;
        int in = 0;
        for (std::size_t i = 0; i < trains_.size(); ++i) {
            if (trains_[i] >= 0.0 && t >= trains_[i]) {
                untested_ += static_cast<int>(v_.train_missiles);
                train_in_.push_back(trains_[i]);
                trains_[i] = -1.0;
                ++in;
            }
        }
        if (untested_ > 0) {
            testing_ += dt * v_.tests_per_h / 3600.0;
            while (testing_ >= 1.0 && untested_ > 0) {
                testing_ -= 1.0;
                --untested_;
                ++tested_;
            }
        } else {
            testing_ = 0.0;
        }
        return in;
    }
    // One tested missile onto the crane's hook; false: none tested yet.
    bool TakeTested() {
        if (tested_ <= 0) return false;
        --tested_;
        return true;
    }
    int Tested() const { return tested_; }
    int Untested() const { return untested_; }
    int TrainsDue() const {  // on their way, not in yet
        int n = 0;
        for (double a : trains_) n += a >= 0.0 ? 1 : 0;
        return n;
    }
    const std::vector<double>& TrainsIn() const { return train_in_; }

private:
    LogisticsValues v_;
    int tested_ = 0;
    int untested_ = 0;
    double testing_ = 0.0;
    std::vector<double> trains_;
    std::vector<double> train_in_;
    double last_s_ = 0.0;
};

class BatteryStocks {
public:
    enum class TzmState { AtBattery, Reloading, ToSiding, Loading, Back };

    struct Tzm {
        std::string id;      // "tzm_a"
        std::string post;    // its commander's post ("tzm21_chief")
        int aboard = 0;
        TzmState state = TzmState::AtBattery;
        int serving = -1;    // the launcher it loads
        double busy_until_s = 0.0;
        double left_s = -1.0;      // set off for the siding
        double arrived_s = -1.0;   // at the siding
        double back_s = -1.0;      // back at the battery, loaded
        int run_order = -1;        // its run in the order book
    };

    // `to_siding`: the TZMs' route from the battery to the technical battery (the route service's, its time the run's).
    void Init(const LogisticsValues& v, int launchers, const std::vector<std::pair<std::string, std::string>>& tzms, int tested, int untested,
              const nav::Route& to_siding) {
        v_ = v;
        launchers_.assign(static_cast<std::size_t>(launchers), static_cast<int>(v.launcher_missiles));
        tzms_.clear();
        for (const auto& t : tzms) {
            Tzm z;
            z.id = t.first;
            z.post = t.second;
            z.aboard = static_cast<int>(v.tzm_missiles);
            tzms_.push_back(z);
        }
        ledger_.launchers = launchers_;
        ledger_.tzms.assign(tzms_.size(), static_cast<int>(v.tzm_missiles));
        siding_.Init(v, tested, untested);
        route_ = to_siding;
    }

    // Launches in the truth, and what the radiotelephonist wrote (the ledger's only input about them).
    void Fire(double t, int launcher, int n) {
        int& k = launchers_[static_cast<std::size_t>(launcher)];
        const int fired = std::min(k, n);
        k -= fired;
        events_.push_back({t, "launcher " + std::to_string(launcher + 1), std::to_string(fired) + " launched"});
    }
    void Logged(double t, int launcher, int n) {
        int& k = ledger_.launchers[static_cast<std::size_t>(launcher)];
        k = std::max(0, k - n);
        events_.push_back({t, "radiotelephonist", "logged " + std::to_string(n) + " launched from launcher " + std::to_string(launcher + 1)});
    }
    // The rear's train to the technical battery: it arrives after its rail route's time (`rail_s` < 0: no way through).
    void TrainDue(double depart_s, double rail_s) { siding_.TrainDue(depart_s, rail_s); }

    void Step(double t) {
        // The technical battery: trains in; its station tests what it has, at its rate.
        const int trains = siding_.Step(t);
        for (int i = 0; i < trains; ++i) {
            events_.push_back({t, "technical battery", "a train in: " + std::to_string(static_cast<int>(v_.train_missiles)) + " missiles"});
        }
        NpcBatteryCommander(t);
        for (Tzm& z : tzms_) StepTzm(z, t);
    }

    const std::vector<int>& Launchers() const { return launchers_; }
    const std::vector<Tzm>& Tzms() const { return tzms_; }
    const Ledger& GetLedger() const { return ledger_; }
    int Tested() const { return siding_.Tested(); }
    int Untested() const { return siding_.Untested(); }
    const std::vector<double>& TrainsIn() const { return siding_.TrainsIn(); }
    const std::vector<MissileRequest>& Requests() const { return requests_; }
    const std::vector<StockEvent>& Events() const { return events_; }
    const command::OrderBook& Orders() const { return orders_; }
    int TrueTotal() const {
        int n = 0;
        for (int k : launchers_) n += k;
        for (const Tzm& z : tzms_) n += z.aboard;
        return n;
    }
    // The TZMs on the road as movers (for drawing them, and for the enemy's armed reconnaissance: G3.4, G11).
    std::vector<nav::Mover> Movers(double t) const {
        std::vector<nav::Mover> out;
        for (const Tzm& z : tzms_) {
            if (z.state != TzmState::ToSiding && z.state != TzmState::Back) continue;
            nav::Mover m;
            m.id = z.id;
            m.kind = "vehicle";
            m.mobility = "tzm_9t217";
            m.level = nav::MoverLevel::M0;
            m.state = nav::MoverState::Moving;
            const double since = z.state == TzmState::ToSiding ? z.left_s : z.busy_until_s - route_.time_s;
            m.moved_s = std::max(0.0, t - since);
            m.route = z.state == TzmState::ToSiding ? route_ : Reversed(route_);
            m.arc_m = nav::ArcAfter(m.route, m.moved_s);
            out.push_back(m);
        }
        return out;
    }

private:
    static nav::Route Reversed(const nav::Route& r) {
        nav::Route b = r;
        std::reverse(b.path.begin(), b.path.end());
        std::reverse(b.stretches.begin(), b.stretches.end());
        return b;
    }
    // The battery commander by his ledger: each launcher he reckons short is loaded by a TZM he reckons has missiles; once
    // his ledger is short of the battery's full load by the rule's amount he asks the regiment; the answer is the TZMs' run.
    void NpcBatteryCommander(double t) {
        for (std::size_t k = 0; k < ledger_.launchers.size(); ++k) {
            if (ledger_.launchers[k] >= static_cast<int>(v_.launcher_missiles) || Serving(static_cast<int>(k))) continue;
            for (std::size_t j = 0; j < tzms_.size(); ++j) {
                Tzm& z = tzms_[j];
                if (z.state != TzmState::AtBattery || ledger_.tzms[j] <= 0) continue;
                z.state = TzmState::Reloading;
                z.serving = static_cast<int>(k);
                z.busy_until_s = t + v_.reload_launcher_s;
                events_.push_back({t, "battery commander", z.id + ": load launcher " + std::to_string(k + 1)});
                break;
            }
        }
        const int full = static_cast<int>(v_.launcher_missiles) * static_cast<int>(launchers_.size()) +
                         static_cast<int>(v_.tzm_missiles) * static_cast<int>(tzms_.size());
        const int short_by = full - ledger_.Total();
        bool on_run = false;
        for (const Tzm& z : tzms_) on_run = on_run || (z.run_order >= 0 && z.back_s < 0.0);
        if (short_by >= static_cast<int>(v_.request_when_short) && answered_ && !on_run) {
            requests_.push_back({t, short_by, t + v_.request_answer_s});
            answered_ = false;
            events_.push_back({t, "battery commander", "asks the regiment for " + std::to_string(short_by) + " missiles"});
        }
        if (!requests_.empty() && !answered_ && t >= requests_.back().answer_s) {
            // The answer: the TZMs' run to the siding, an order to each TZM's commander.
            std::vector<std::string> posts;
            for (const Tzm& z : tzms_) posts.push_back(z.post);
            command::BatteryOrderRecord o;
            o.id = "tzm_run_" + std::to_string(requests_.size());
            o.giver = "bty21_commander";
            o.kind = "tzm_run";
            o.words = "ТЗМ — на техническую батарею за ракетами";
            o.given_h = t / 3600.0;
            o.mean = "in_person";
            const int idx = orders_.Give(o, posts);
            for (Tzm& z : tzms_) {
                z.left_s = -1.0;
                z.arrived_s = -1.0;
                z.back_s = -1.0;
                orders_.Mark(idx, z.post, command::OrderStep::Reached, t / 3600.0, "in_person");
                orders_.Mark(idx, z.post, command::OrderStep::Heard, t / 3600.0);
                orders_.Mark(idx, z.post, command::OrderStep::RepeatedBack, t / 3600.0);
                z.run_order = idx;
            }
            answered_ = true;
            events_.push_back({t, "regiment", "the TZMs to the Horka siding for missiles"});
        }
    }
    bool Serving(int k) const {
        for (const Tzm& z : tzms_) {
            if (z.state == TzmState::Reloading && z.serving == k) return true;
        }
        return false;
    }
    void StepTzm(Tzm& z, double t) {
        const std::size_t j = static_cast<std::size_t>(&z - tzms_.data());
        switch (z.state) {
            case TzmState::Reloading:
                if (t >= z.busy_until_s) {
                    int& l = launchers_[static_cast<std::size_t>(z.serving)];
                    const int n = std::min(z.aboard, static_cast<int>(v_.launcher_missiles) - l);
                    l += n;
                    z.aboard -= n;
                    // His report: what he loaded; the ledger takes it.
                    ledger_.launchers[static_cast<std::size_t>(z.serving)] += n;
                    ledger_.tzms[j] -= n;
                    if (n == 0) ledger_.launchers[static_cast<std::size_t>(z.serving)] = static_cast<int>(v_.launcher_missiles);  // "it is full"
                    events_.push_back({t, z.id, "loaded launcher " + std::to_string(z.serving + 1) + " with " + std::to_string(n)});
                    z.state = TzmState::AtBattery;
                    z.serving = -1;
                }
                break;
            case TzmState::AtBattery:
                if (z.run_order >= 0 && z.left_s < 0.0 && z.back_s < 0.0 && z.aboard < static_cast<int>(v_.tzm_missiles)) {
                    z.state = TzmState::ToSiding;
                    z.left_s = t;
                    z.busy_until_s = t + route_.time_s;
                    orders_.Mark(z.run_order, z.post, command::OrderStep::CarryingOut, t / 3600.0);
                    events_.push_back({t, z.id, "sets off for the Horka siding"});
                } else if (z.run_order >= 0 && z.left_s < 0.0 && z.back_s < 0.0) {  // full already: nothing to fetch
                    z.back_s = t;
                    orders_.Mark(z.run_order, z.post, command::OrderStep::CarryingOut, t / 3600.0);
                    orders_.Mark(z.run_order, z.post, command::OrderStep::Done, t / 3600.0);
                    orders_.Mark(z.run_order, z.post, command::OrderStep::Reported, t / 3600.0);
                }
                break;
            case TzmState::ToSiding:
                if (t >= z.busy_until_s) {
                    z.state = TzmState::Loading;
                    z.arrived_s = t;
                    z.busy_until_s = t;
                    events_.push_back({t, z.id, "at the technical battery"});
                }
                break;
            case TzmState::Loading:
                // One crane: the TZMs at the siding load in turn, a tested missile at a time.
                if (LoadingFirst() != &z) break;
                if (t >= z.busy_until_s && z.aboard < static_cast<int>(v_.tzm_missiles)) {
                    if (crane_missile_) {
                        ++z.aboard;
                        crane_missile_ = false;
                    }
                    if (z.aboard < static_cast<int>(v_.tzm_missiles) && siding_.TakeTested()) {
                        crane_missile_ = true;
                        z.busy_until_s = t + v_.load_tzm_s_per_missile;
                    }
                }
                if (z.aboard >= static_cast<int>(v_.tzm_missiles)) {
                    z.state = TzmState::Back;
                    z.busy_until_s = t + route_.time_s;
                    events_.push_back({t, z.id, "loaded, back to the battery"});
                }
                break;
            case TzmState::Back:
                if (t >= z.busy_until_s) {
                    z.state = TzmState::AtBattery;
                    z.back_s = t;
                    ledger_.tzms[j] = z.aboard;  // his report on his return
                    if (z.run_order >= 0) {
                        orders_.Mark(z.run_order, z.post, command::OrderStep::Done, t / 3600.0);
                        orders_.Mark(z.run_order, z.post, command::OrderStep::Reported, t / 3600.0);
                    }
                    events_.push_back({t, z.id, "back at the battery with " + std::to_string(z.aboard)});
                }
                break;
        }
    }
    const Tzm* LoadingFirst() const {
        const Tzm* first = nullptr;
        for (const Tzm& z : tzms_) {
            if (z.state == TzmState::Loading && (first == nullptr || z.arrived_s < first->arrived_s)) first = &z;
        }
        return first;
    }

    LogisticsValues v_;
    std::vector<int> launchers_;
    std::vector<Tzm> tzms_;
    Ledger ledger_;
    TechnicalBattery siding_;
    bool crane_missile_ = false;  // a missile on the crane's hook
    nav::Route route_;
    std::vector<MissileRequest> requests_;
    bool answered_ = true;
    std::vector<StockEvent> events_;
    command::OrderBook orders_;
};

// One TZM's run in a session (roadmap 17.4): from the battery to the technical battery on the session's roads and back.
// Its chief plans on the bridges he was told were down when he got the order. One down that he did not know of he sees
// as he comes to it (the column's lookout, nav/march.hpp): he stops short of it, and after his time at the map plans
// again from where he stands; with no way on he turns back and will report that he cannot, and with no way back he waits
// where he stands, looking again after each time at the map (a bridge in sight open again he sees). At the siding the
// crane loads him a tested missile at a time as the station tests them, and with nothing left to test and no train due
// he comes back with what he has; he plans the way back there, on what he now believes. The caller moves the mover
// (Movers::Advance headless, the session's movers in a session, before or after the run's step); the run reads where it
// is.
class TzmRun {
public:
    enum class Leg { Out, Stopped, Loading, Back, Done };

    // From `home` to the `siding` with `aboard` on the TZM, on the overlay its chief was `told` of. False, with why, when
    // there is no way on it; else `out` is the mover on its first leg (its level left as it was).
    bool Start(const nav::RoadGraph& g, const nav::Mobility& cls, const LogisticsValues& v, const std::string& tzm_id, const MapPoint& home,
               const MapPoint& siding, int aboard, const nav::Overlay& told, bool night, double t, nav::Mover& out, std::string* why = nullptr) {
        v_ = v;
        cls_ = cls;
        id_ = tzm_id;
        home_ = home;
        siding_at_ = siding;
        aboard_ = aboard;
        believed_ = told;
        found_.clear();
        night_ = night;
        stuck_ = siding_empty_ = false;
        const nav::Route r = Plan(g, home, siding, t);
        if (!r.found) {
            if (why != nullptr) *why = r.why.empty() ? "no way to the technical battery" : r.why;
            return false;
        }
        leg_ = going_ = Leg::Out;
        cannot_.clear();
        left_s_ = t;
        at_siding_s_ = back_s_ = replan_at_ = -1.0;
        crane_ = waiting_ = false;
        out.id = tzm_id;
        out.kind = "vehicle";
        out.mobility = cls.id;
        Follow(out, r, t, t);
        events_.push_back({t, id_, "sets off for the technical battery with " + std::to_string(aboard_) + " aboard"});
        return true;
    }

    void Step(double t, const nav::RoadGraph& g, const nav::Overlay& truth, TechnicalBattery& siding, nav::Mover& m) {
        switch (leg_) {
            case Leg::Out:
            case Leg::Back:
                if (m.state == nav::MoverState::Arrived) {
                    Arrive(t, g, siding, m);
                } else {
                    Lookout(t, g, truth, m);
                }
                break;
            case Leg::Stopped:
                if (t + 1e-9 >= replan_at_) Replan(t, g, truth, m);
                break;
            case Leg::Loading:
                Load(t, g, siding, m);
                break;
            case Leg::Done:
                break;
        }
    }

    Leg GetLeg() const { return leg_; }
    bool Finished() const { return leg_ == Leg::Done; }
    bool Home() const { return leg_ == Leg::Done && back_s_ >= 0.0; }  // back at the battery (not stuck out on the roads)
    int Aboard() const { return aboard_; }
    const std::string& CannotWhy() const { return cannot_; }  // "" while he can, or came back loaded
    bool Stuck() const { return stuck_; }                      // no way back: waiting where he is for one
    bool SidingEmpty() const { return siding_empty_; }         // the technical battery had too few for him
    const nav::Overlay& Believed() const { return believed_; }  // his: what he was told, and what he found
    const std::vector<nav::OverlayEntry>& Found() const { return found_; }  // the crossings he found down on the way
    const std::vector<StockEvent>& Events() const { return events_; }
    double LeftS() const { return left_s_; }
    double AtSidingS() const { return at_siding_s_; }
    double BackS() const { return back_s_; }

private:
    nav::Route Plan(const nav::RoadGraph& g, const MapPoint& from, const MapPoint& to, double t) const {
        nav::RouteRequest q;
        q.from = from;
        q.to = to;
        q.mobility = &cls_;
        q.h = t / 3600.0;
        q.night = night_;
        q.swim = false;  // loaded, the TZM keeps out of the water (sam3d: the class swims empty)
        return g.Find(q, believed_);
    }
    // The mover on a new route, moving since `depart_s` (<= t): the time to t counted here, so Movers::Advance adds only
    // what comes after t, whether the caller moves it before the run's step or after it in the same step.
    void Follow(nav::Mover& m, const nav::Route& r, double depart_s, double t) {
        m.route = r;
        m.depart_s = depart_s;
        m.moved_s = std::max(0.0, t - depart_s);
        m.arc_m = nav::ArcAfter(r, m.moved_s);
        m.arrived_s = -1.0;
        m.halt_until_s = t;
        m.state = nav::MoverState::Moving;
        arcs_.assign(r.path.size(), 0.0);
        for (std::size_t i = 0; i + 1 < r.path.size(); ++i) {
            arcs_[i + 1] = arcs_[i] + std::hypot(r.path[i + 1].east_m - r.path[i].east_m, r.path[i + 1].north_m - r.path[i].north_m);
        }
        look_from_ = 0;
    }
    // A crossing down in the truth within sight ahead that he did not know of: he stops short of it.
    void Lookout(double t, const nav::RoadGraph& g, const nav::Overlay& truth, nav::Mover& m) {
        const nav::Route& r = m.route;
        const double h = t / 3600.0;
        for (std::size_t i = look_from_; i < r.path.size() && i < r.nodes.size() && i < arcs_.size(); ++i) {
            if (arcs_[i] + 1e-6 < m.arc_m) {
                look_from_ = i + 1;
                continue;
            }
            if (arcs_[i] > m.arc_m + s_.see_m) break;
            const int node = r.nodes[i];
            if (node < 0) continue;
            const int c = g.Nodes()[static_cast<std::size_t>(node)].crossing;
            if (c < 0) continue;
            const std::string& id = g.Crossings()[static_cast<std::size_t>(c)].id;
            const nav::OverlayEntry* down = truth.Down(id, h);
            if (down == nullptr || believed_.Down(id, h) != nullptr) continue;
            m.state = nav::MoverState::Blocked;
            believed_.entries.push_back(*down);
            found_.push_back(*down);
            going_ = leg_;
            leg_ = Leg::Stopped;
            replan_at_ = t + s_.replan_s;
            events_.push_back({t, id_, "the bridge " + id + " is down: stopped short of it"});
            return;
        }
    }
    // At his map, from where he stands, on what he now believes; with no way on to the siding, back to the battery; with
    // no way back, he waits there and looks again after his next time at the map.
    void Replan(double t, const nav::RoadGraph& g, const nav::Overlay& truth, nav::Mover& m) {
        const MapPoint here = nav::PointAlong(m.route, m.arc_m);
        LookAgain(t, g, truth, here);
        nav::Route r = Plan(g, here, going_ == Leg::Out ? siding_at_ : home_, t);
        if (!r.found && going_ == Leg::Out) {
            cannot_ = "every way is cut";
            going_ = Leg::Back;
            r = Plan(g, here, home_, t);
            events_.push_back({t, id_, "no way on to the technical battery: back to the battery"});
        }
        if (!r.found) {
            WaitForAWay(t, m, "no way back to the battery: waits where he is");
            return;
        }
        stuck_ = false;
        Follow(m, r, std::min(t, replan_at_), t);
        leg_ = going_;
        events_.push_back({t, id_, "a new route: " + std::to_string(static_cast<int>(std::lround(r.time_s / 60.0))) + " min"});
    }
    void WaitForAWay(double t, nav::Mover& m, const std::string& what) {
        if (!stuck_) events_.push_back({t, id_, what});
        stuck_ = true;
        going_ = Leg::Back;
        leg_ = Leg::Stopped;
        replan_at_ = t + s_.replan_s;
        m.state = nav::MoverState::Blocked;
    }
    // What he can see from where he stands: a bridge he took for down that stands open in the truth now.
    void LookAgain(double t, const nav::RoadGraph& g, const nav::Overlay& truth, const MapPoint& here) {
        const double h = t / 3600.0;
        for (nav::OverlayEntry& e : believed_.entries) {
            if (h < e.from_h || h >= e.until_h || truth.Down(e.crossing, h) != nullptr) continue;
            const nav::NavCrossing* c = g.Crossing(e.crossing);
            if (c == nullptr || std::hypot(c->at.east_m - here.east_m, c->at.north_m - here.north_m) > s_.see_m) continue;
            e.until_h = h;
            for (nav::OverlayEntry& f : found_) {
                if (f.id == e.id && h < f.until_h) f.until_h = h;
            }
            events_.push_back({t, id_, "the bridge " + e.crossing + " stands open again"});
        }
    }
    void Arrive(double t, const nav::RoadGraph& g, TechnicalBattery& siding, nav::Mover& m) {
        const double at = m.arrived_s >= 0.0 ? m.arrived_s : t;
        if (leg_ == Leg::Out) {
            leg_ = Leg::Loading;
            at_siding_s_ = at;
            crane_free_s_ = at;
            events_.push_back({t, id_, "at the technical battery"});
            Load(t, g, siding, m);
            return;
        }
        leg_ = Leg::Done;
        back_s_ = at;
        events_.push_back({t, id_, cannot_.empty() ? "back at the battery with " + std::to_string(aboard_) : "back at the battery: " + cannot_});
    }
    // One crane: a tested missile onto the hook, onto the TZM after the loading time; waits for the station when none is
    // tested. Full, he plans the way back.
    void Load(double t, const nav::RoadGraph& g, TechnicalBattery& siding, nav::Mover& m) {
        const int full = static_cast<int>(v_.tzm_missiles);
        for (;;) {
            if (crane_) {
                if (t + 1e-9 < busy_until_s_) return;
                ++aboard_;
                crane_ = false;
                crane_free_s_ = busy_until_s_;
            }
            if (aboard_ >= full) break;
            if (!siding.TakeTested()) {
                if (siding.Untested() > 0 || siding.TrainsDue() > 0) {
                    waiting_ = true;
                    return;
                }
                siding_empty_ = true;  // nothing left to test and no train due: back with what he has
                events_.push_back({t, id_, "the technical battery has no more missiles"});
                break;
            }
            crane_ = true;
            busy_until_s_ = (waiting_ ? t : crane_free_s_) + v_.load_tzm_s_per_missile;
            waiting_ = false;
        }
        const nav::Route r = Plan(g, siding_at_, home_, t);
        if (!r.found) {
            WaitForAWay(t, m, "loaded " + std::to_string(aboard_) + "; no way back to the battery: waits at the siding");
            return;
        }
        leg_ = going_ = Leg::Back;
        Follow(m, r, std::min(t, crane_free_s_), t);
        events_.push_back({t, id_, "loaded " + std::to_string(aboard_) + ", back to the battery"});
    }

    LogisticsValues v_;
    nav::Mobility cls_;
    nav::ColumnMarchSettings s_;
    std::string id_;
    MapPoint home_;
    MapPoint siding_at_;
    int aboard_ = 0;
    nav::Overlay believed_;
    std::vector<nav::OverlayEntry> found_;
    bool night_ = false;
    Leg leg_ = Leg::Done;
    Leg going_ = Leg::Done;  // the leg he is on while stopped
    std::string cannot_;
    bool stuck_ = false;         // no way back: waiting where he is
    bool siding_empty_ = false;  // came away with what the siding had
    double left_s_ = -1.0;
    double at_siding_s_ = -1.0;
    double back_s_ = -1.0;
    double replan_at_ = -1.0;
    bool crane_ = false;    // a missile on the hook
    bool waiting_ = false;  // for the station: none tested
    double busy_until_s_ = 0.0;
    double crane_free_s_ = 0.0;
    std::vector<double> arcs_;  // along the mover's route at each of its points
    std::size_t look_from_ = 0;
    std::vector<StockEvent> events_;
};

}  // namespace campaign
}  // namespace simcore
