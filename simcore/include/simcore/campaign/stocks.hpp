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
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "simcore/command/orders.hpp"
#include "simcore/console_data.hpp"
#include "simcore/json.hpp"
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
        tested_ = tested;
        untested_ = untested;
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
    void TrainDue(double depart_s, double rail_s) {
        if (rail_s < 0.0) return;
        trains_.push_back(depart_s + rail_s);
    }

    void Step(double t) {
        const double dt = std::max(0.0, t - last_s_);
        last_s_ = t;
        // The technical battery: trains in; its station tests what it has, at its rate.
        for (std::size_t i = 0; i < trains_.size(); ++i) {
            if (trains_[i] >= 0.0 && t >= trains_[i]) {
                untested_ += static_cast<int>(v_.train_missiles);
                events_.push_back({t, "technical battery", "a train in: " + std::to_string(static_cast<int>(v_.train_missiles)) + " missiles"});
                train_in_.push_back(trains_[i]);
                trains_[i] = -1.0;
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
        NpcBatteryCommander(t);
        for (Tzm& z : tzms_) StepTzm(z, t);
    }

    const std::vector<int>& Launchers() const { return launchers_; }
    const std::vector<Tzm>& Tzms() const { return tzms_; }
    const Ledger& GetLedger() const { return ledger_; }
    int Tested() const { return tested_; }
    int Untested() const { return untested_; }
    const std::vector<double>& TrainsIn() const { return train_in_; }
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
                    if (z.aboard < static_cast<int>(v_.tzm_missiles) && tested_ > 0) {
                        --tested_;
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
    int tested_ = 0;
    int untested_ = 0;
    double testing_ = 0.0;
    bool crane_missile_ = false;  // a missile on the crane's hook
    std::vector<double> trains_;
    std::vector<double> train_in_;
    nav::Route route_;
    double last_s_ = 0.0;
    std::vector<MissileRequest> requests_;
    bool answered_ = true;
    std::vector<StockEvent> events_;
    command::OrderBook orders_;
};

}  // namespace campaign
}  // namespace simcore
