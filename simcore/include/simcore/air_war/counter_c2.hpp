// simcore/air_war/counter_c2.hpp — our side against the enemy's command in the air, as data (roadmap S2,
// docs/air_battle/05_C2_AND_RETASKING.md section 9; data/command/counter_c2.json).
//
// The Soviet plan against NATO's tactical air control: destroy a third of it, jam a third, listen to the rest
// (miller_1980_tacs_jamming). Each enemy post is marked one of the three by a draw keyed on its id (AirC2::Marked); a post
// marked for destruction is struck by our fires once our listeners have heard enough of its calls to fix it by direction
// finding (fm_100_2_1_1984 p. 15-2: 3.5 degrees to about 40 km); a post marked for jamming has its net jammed by our
// air-net jammers standing behind our line, judged by FM 34-40-7's J/S rule (comms_jamming.hpp). Every value carries its
// source; the estimates say so.
#pragma once

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/json.hpp"

namespace simcore {
namespace airwar {

struct CounterC2Jammer {
    std::string id;
    std::string name;
    std::string em_jammer;  // its spot jammer in the session's air radio library (data/em/air_radios_1985.json)
    int from_year = 0;
    double band_lo_hz = 0.0;
    double band_hi_hz = 0.0;
    double power_w = 0.0;
    double behind_m = 0.0;  // behind our line, at the line's point nearest our battery
    bool Covers(double freq_hz) const { return freq_hz >= band_lo_hz && freq_hz <= band_hi_hz; }
};

struct CounterC2Def {
    bool present = false;
    double destroy = 0.0;  // shares of the enemy's posts marked to be destroyed, jammed, listened to
    double jam = 0.0;
    double listen = 1.0;
    double df_bearing_rad = 0.0;
    double df_reach_m = 0.0;
    double fires_reach_m = 0.0;
    double enemy_set_w = 10.0;
    std::vector<CounterC2Jammer> jammers;
};

namespace detail {

// A value given as {"value": x, "source": ..., "confidence": ...}: x, or false with why.
inline bool CounterValue(const json::Value& obj, const std::string& key, double lo, double hi, double& out, std::string& why) {
    const json::Value* v = obj.Find(key);
    if (v == nullptr || !v->Is(json::Type::Object)) {
        why = key + ": expected {value, source, confidence}";
        return false;
    }
    const json::Value* x = v->Find("value");
    const json::Value* s = v->Find("source");
    const json::Value* c = v->Find("confidence");
    if (x == nullptr || !x->Is(json::Type::Number) || s == nullptr || !s->Is(json::Type::String) || c == nullptr || !c->Is(json::Type::String)) {
        why = key + ": expected {value, source, confidence}";
        return false;
    }
    if (x->number < lo || x->number > hi) {
        why = key + ": out of range";
        return false;
    }
    out = x->number;
    return true;
}

}  // namespace detail

inline bool LoadCounterC2Json(const std::string& text, CounterC2Def& out, std::string* error = nullptr) {
    const auto fail = [error](const std::string& w) {
        if (error != nullptr) *error = "counter_c2: " + w;
        return false;
    };
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) return fail(e);
    CounterC2Def d;
    const json::Value* split = root.Find("split");
    if (split == nullptr || !split->Is(json::Type::Object) || split->Find("source") == nullptr) return fail("split: expected {destroy, jam, listen, source}");
    const json::Value* sd = split->Find("destroy");
    const json::Value* sj = split->Find("jam");
    const json::Value* sl = split->Find("listen");
    if (sd == nullptr || sj == nullptr || sl == nullptr || !sd->Is(json::Type::Number) || !sj->Is(json::Type::Number) || !sl->Is(json::Type::Number))
        return fail("split: destroy, jam and listen are numbers");
    d.destroy = sd->number;
    d.jam = sj->number;
    d.listen = sl->number;
    if (d.destroy < 0.0 || d.jam < 0.0 || d.listen < 0.0 || std::fabs(d.destroy + d.jam + d.listen - 1.0) > 1e-6) return fail("split: shares of one");
    double deg = 0.0, km = 0.0;
    if (!detail::CounterValue(root, "df_bearing_deg", 0.0, 30.0, deg, e)) return fail(e);
    d.df_bearing_rad = deg * 3.14159265358979323846 / 180.0;
    if (!detail::CounterValue(root, "df_reach_km", 0.0, 500.0, km, e)) return fail(e);
    d.df_reach_m = km * 1000.0;
    if (!detail::CounterValue(root, "fires_reach_km", 0.0, 500.0, km, e)) return fail(e);
    d.fires_reach_m = km * 1000.0;
    if (!detail::CounterValue(root, "enemy_set_w", 0.0, 1e5, d.enemy_set_w, e)) return fail(e);
    const json::Value* js = root.Find("jammers");
    if (js == nullptr || !js->Is(json::Type::Array)) return fail("jammers: expected a list");
    for (std::size_t i = 0; i < js->items.size(); ++i) {
        const json::Value& v = js->items[i];
        const std::string p = "jammers[" + std::to_string(i) + "]";
        CounterC2Jammer j;
        const json::Value* id = v.Find("id");
        const json::Value* name = v.Find("name");
        const json::Value* em = v.Find("em_jammer");
        const json::Value* year = v.Find("from_year");
        if (id == nullptr || !id->Is(json::Type::String) || v.Find("source") == nullptr || v.Find("confidence") == nullptr)
            return fail(p + ": id, source and confidence required");
        j.id = id->string;
        if (name != nullptr && name->Is(json::Type::String)) j.name = name->string;
        if (em != nullptr && em->Is(json::Type::String)) j.em_jammer = em->string;
        if (year != nullptr && year->Is(json::Type::Number)) j.from_year = static_cast<int>(year->number);
        const json::Value* band = v.Find("band_mhz");
        const json::Value* bv = band != nullptr ? band->Find("value") : nullptr;
        if (bv == nullptr || !bv->Is(json::Type::Array) || bv->items.size() != 2 || !bv->items[0].Is(json::Type::Number) ||
            !bv->items[1].Is(json::Type::Number) || band->Find("source") == nullptr || bv->items[0].number > bv->items[1].number)
            return fail(p + ".band_mhz: expected {value: [lo, hi], source, confidence}");
        j.band_lo_hz = bv->items[0].number * 1e6;
        j.band_hi_hz = bv->items[1].number * 1e6;
        if (!detail::CounterValue(v, "power_w", 0.0, 1e6, j.power_w, e)) return fail(p + "." + e);
        if (!detail::CounterValue(v, "behind_km", 0.0, 200.0, km, e)) return fail(p + "." + e);
        j.behind_m = km * 1000.0;
        d.jammers.push_back(j);
    }
    d.present = true;
    out = d;
    return true;
}

// The file, if there is one: none is no counter-C2 (`present` false), not an error.
inline bool LoadCounterC2File(const std::string& path, CounterC2Def& out, std::string* error = nullptr) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        out = CounterC2Def();
        return true;
    }
    std::ostringstream s;
    s << f.rdbuf();
    return LoadCounterC2Json(s.str(), out, error);
}

}  // namespace airwar
}  // namespace simcore
