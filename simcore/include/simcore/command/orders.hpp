// simcore/command/orders.hpp — a battery order as a record (roadmap G10.1; docs/campaign/11_BATTERY_LIFE_PROPOSAL.md
// §5.5, decision G-13): words carried by a means to a person who carries it out. Who gave it, to whom, what, by what
// means and in what words, and for each recipient the hour of each step:
//
//   given -> reached -> heard -> repeated back -> carrying out -> done -> reported -> logged
//
// A step needs the one before it (carrying out needs only "heard": a repeat-back the giver never heard does not stop
// the vehicle), so no vehicle acts on an order its commander did not hear. A recipient may report he cannot, with why.
// The giver knows only who repeated back; a silence past its time makes him say it again, then try the next means,
// then send a runner (G10.4, `ChaseRule`). The debrief reads each recipient's times from here (G10.6).
#pragma once

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/command/info_net.hpp"

namespace simcore {
namespace command {

enum class OrderStep { Given, Reached, Heard, RepeatedBack, CarryingOut, Done, Reported, Logged, Count };

// "D1 10:00:30": an order's steps are seconds apart.
inline std::string FormatClockSeconds(double h, double start_hour) {
    const long long total = std::llround((start_hour + h) * 3600.0);
    const long long day = total / 86400;
    const long long in_day = total - day * 86400;
    char buf[96];  // room for any long long in each field, so GCC's -Wformat-truncation has nothing to say
    std::snprintf(buf, sizeof(buf), "D%lld %02lld:%02lld:%02lld", day + 1, in_day / 3600, (in_day / 60) % 60, in_day % 60);
    return buf;
}

inline const char* OrderStepName(OrderStep s) {
    switch (s) {
        case OrderStep::Given: return "given";
        case OrderStep::Reached: return "reached";
        case OrderStep::Heard: return "heard";
        case OrderStep::RepeatedBack: return "repeated back";
        case OrderStep::CarryingOut: return "carrying out";
        case OrderStep::Done: return "done";
        case OrderStep::Reported: return "reported";
        case OrderStep::Logged: return "logged";
        case OrderStep::Count: break;
    }
    return "?";
}

// The step a step needs before it.
inline OrderStep OrderStepNeeds(OrderStep s) {
    switch (s) {
        case OrderStep::CarryingOut: return OrderStep::Heard;
        case OrderStep::Given: return OrderStep::Given;
        default: return static_cast<OrderStep>(static_cast<int>(s) - 1);
    }
}

// One recipient of an order.
struct OrderLeg {
    std::string post;  // a vehicle commander's post ("veh21_3_commander")
    std::array<double, static_cast<std::size_t>(OrderStep::Count)> at{};  // the hour of each step; < 0 not yet
    std::string mean;        // the means that reached him
    int tries = 0;           // how often it was sent to him
    double last_sent_h = 0.0;
    std::string last_mean;   // the means it was last sent by
    std::vector<std::pair<double, std::string>> sent;  // every sending: the hour and the means
    std::string cannot;      // he reported he cannot, and why ("the bridge is down")
    double cannot_h = -1.0;

    OrderLeg() { at.fill(-1.0); }
    bool Has(OrderStep s) const { return at[static_cast<std::size_t>(s)] >= 0.0; }
    double At(OrderStep s) const { return at[static_cast<std::size_t>(s)]; }
    // The furthest step reached.
    OrderStep Last() const {
        OrderStep out = OrderStep::Given;
        for (int i = 0; i < static_cast<int>(OrderStep::Count); ++i) {
            if (at[static_cast<std::size_t>(i)] >= 0.0) out = static_cast<OrderStep>(i);
        }
        return out;
    }
};

struct BatteryOrderRecord {
    std::string id;
    std::string giver;  // the post that gave it
    std::string kind;   // battery.hpp's order names ("march", "short_halt", "combat_formation", "relocate", ...)
    std::string words;  // what was said («К маршу!»)
    double given_h = 0.0;
    std::string mean;   // the means it was given by first
    std::vector<OrderLeg> to;

    const OrderLeg* For(const std::string& post) const {
        for (const OrderLeg& l : to) {
            if (l.post == post) return &l;
        }
        return nullptr;
    }
};

// What the giver does about a silence (G10.4): wait so long for the repeat-back, say it again on the same means so many
// times, then try each next means in turn (the last a runner). Game values until G0 reads the drill; a skill tier
// scales the wait.
struct ChaseRule {
    double wait_s = 30.0;
    int repeats = 2;
    std::vector<std::string> next_means = {"field_telephone", "messenger"};
};

// The giver's next sending to one recipient.
struct Chase {
    std::string post;
    std::string mean;
    double h = 0.0;
    int attempt = 0;  // the sending's number, 2 for the first repeat
};

class OrderBook {
public:
    // An order given at its hour by its means to its recipients (their posts). Returns its index.
    int Give(const BatteryOrderRecord& order, const std::vector<std::string>& recipients) {
        BatteryOrderRecord o = order;
        o.to.clear();
        for (const std::string& p : recipients) {
            OrderLeg l;
            l.post = p;
            l.at[static_cast<std::size_t>(OrderStep::Given)] = o.given_h;
            l.tries = 1;
            l.last_sent_h = o.given_h;
            l.last_mean = o.mean;
            l.sent.push_back({o.given_h, o.mean});
            o.to.push_back(l);
        }
        orders_.push_back(o);
        return static_cast<int>(orders_.size()) - 1;
    }

    // A recipient reached a step at hour h ("reached": by `mean`). Refused, with why, if he has not the step it needs,
    // or the hour is before it.
    bool Mark(int order, const std::string& post, OrderStep step, double h, const std::string& mean = "", std::string* why = nullptr) {
        OrderLeg* l = Leg(order, post);
        if (l == nullptr) return Fail(why, "no such order or recipient");
        if (step == OrderStep::Given) return Fail(why, "an order is given once");
        const OrderStep need = OrderStepNeeds(step);
        if (!l->Has(need)) return Fail(why, std::string("not ") + OrderStepName(need) + " yet");
        if (h < l->At(need) - 1e-9) return Fail(why, std::string("before it was ") + OrderStepName(need));
        if (l->Has(step)) return true;  // the first time stands
        l->at[static_cast<std::size_t>(step)] = h;
        if (step == OrderStep::Reached) l->mean = mean.empty() ? l->last_mean : mean;
        return true;
    }

    // He reports he cannot carry it out, and why: it goes up like a report (the "reported" step).
    bool Cannot(int order, const std::string& post, double h, const std::string& reason, std::string* why = nullptr) {
        OrderLeg* l = Leg(order, post);
        if (l == nullptr) return Fail(why, "no such order or recipient");
        if (!l->Has(OrderStep::Heard)) return Fail(why, "not heard yet");
        l->cannot = reason;
        l->cannot_h = h;
        return true;
    }

    // The giver sent it to him again.
    bool Resend(int order, const std::string& post, double h, const std::string& mean) {
        OrderLeg* l = Leg(order, post);
        if (l == nullptr) return false;
        ++l->tries;
        l->last_sent_h = h;
        l->last_mean = mean;
        l->sent.push_back({h, mean});
        return true;
    }

    // What the giver does by `now_h` about each recipient whose repeat-back (or "cannot") he has not had: after the
    // rule's wait since the last sending, the same means again while repeats are left, then the next means in turn.
    std::vector<Chase> Due(int order, double now_h, const ChaseRule& rule) const {
        std::vector<Chase> out;
        if (order < 0 || static_cast<std::size_t>(order) >= orders_.size()) return out;
        for (const OrderLeg& l : orders_[static_cast<std::size_t>(order)].to) {
            if (l.Has(OrderStep::RepeatedBack) || l.cannot_h >= 0.0) continue;
            const double due = l.last_sent_h + rule.wait_s / 3600.0;
            if (now_h < due - 1e-9) continue;
            Chase c;
            c.post = l.post;
            c.h = due;
            c.attempt = l.tries + 1;
            if (l.tries <= rule.repeats) {
                c.mean = l.last_mean;
            } else {
                const int k = l.tries - rule.repeats - 1;
                if (k >= static_cast<int>(rule.next_means.size())) continue;  // nothing left but to go himself
                c.mean = rule.next_means[static_cast<std::size_t>(k)];
            }
            out.push_back(c);
        }
        return out;
    }

    const std::vector<BatteryOrderRecord>& Orders() const { return orders_; }

    // The debrief's lines (G10.6): for each recipient, every sending and every step with its hour.
    std::vector<std::string> Debrief(int order, double start_hour) const {
        std::vector<std::string> out;
        if (order < 0 || static_cast<std::size_t>(order) >= orders_.size()) return out;
        const BatteryOrderRecord& o = orders_[static_cast<std::size_t>(order)];
        out.push_back(FormatClockSeconds(o.given_h, start_hour) + " " + o.giver + ": " + o.words + " (" + o.kind + ", by " + o.mean + ")");
        for (const OrderLeg& l : o.to) {
            std::string s = "  " + l.post + ":";
            for (std::size_t k = 1; k < l.sent.size(); ++k) s += " sent again " + FormatClockSeconds(l.sent[k].first, start_hour) + " by " + l.sent[k].second + ";";
            for (int i = 1; i < static_cast<int>(OrderStep::Count); ++i) {
                const OrderStep st = static_cast<OrderStep>(i);
                if (!l.Has(st)) continue;
                s += std::string(" ") + OrderStepName(st) + " " + FormatClockSeconds(l.At(st), start_hour);
                if (st == OrderStep::Reached) s += " by " + l.mean;
                s += ";";
            }
            if (l.cannot_h >= 0.0) s += " cannot (" + l.cannot + ") " + FormatClockSeconds(l.cannot_h, start_hour) + ";";
            if (!l.Has(OrderStep::Reached)) s += " never reached;";
            out.push_back(s);
        }
        return out;
    }

private:
    static bool Fail(std::string* why, const std::string& w) {
        if (why != nullptr) *why = w;
        return false;
    }
    OrderLeg* Leg(int order, const std::string& post) {
        if (order < 0 || static_cast<std::size_t>(order) >= orders_.size()) return nullptr;
        for (OrderLeg& l : orders_[static_cast<std::size_t>(order)].to) {
            if (l.post == post) return &l;
        }
        return nullptr;
    }

    std::vector<BatteryOrderRecord> orders_;
};

}  // namespace command
}  // namespace simcore
