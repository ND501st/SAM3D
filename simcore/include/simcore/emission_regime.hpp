// simcore/emission_regime.hpp — the battery's emission regime (режим излучения) and the words that pass it down
// (data/command/emission_regimes.json, docs/crew/16_EMISSION.md).
//
// How long the radars may radiate is decided above the crew and travels down by voice:
//   - the regiment may order it on its command net (a ПТ-1 group);
//   - the battery commander decides it (voenizdat_osa_1991 p243: his decision fixes the measures against jamming and
//     anti-radiation missiles), or relays the regiment's, and says it on the battery net;
//   - each launcher's commander passes it to his crew on the intercom and tells the radar seats how many antenna turns
//     on and off (and how many seconds that is), before they deploy or as soon as it changes, and reminds them when a
//     hostile appears or a burst runs long.
// Nobody acts on a regime he hasn't been told: the senior operator keeps the rhythm only once his commander has given it
// to him (ScenarioRunner::CrewRegime), the commander orders by what he heard on the net (CommanderRegime).
//
// The regimes: free (as the fight needs), limited (the search transmitter in bursts against direction finding, on all the
// time once a hostile is within the tracking radar's reach), arm (the least emission of p262: bursts of 2 turns after 3-4,
// the tracking transmitter only from Table 7.3's range and off after the burst), silent (both radars off: the TV and the
// battery's designations). The list and the words are SAM3D's; the numbers are cited in the data.
#pragma once

#include <cmath>
#include <cstdio>
#include <map>
#include <string>

#include "simcore/json.hpp"
#include "simcore/pu12.hpp"
#include "simcore/talk_tables.hpp"

namespace simcore {

enum class EmissionKind { Free, Limited, Arm, Silent };

inline const char* EmissionKindName(EmissionKind k) {
    switch (k) {
        case EmissionKind::Free: return "free";
        case EmissionKind::Limited: return "limited";
        case EmissionKind::Arm: return "arm";
        case EmissionKind::Silent: return "silent";
    }
    return "?";
}
inline bool ParseEmissionKind(const std::string& s, EmissionKind& out) {
    for (EmissionKind k : {EmissionKind::Free, EmissionKind::Limited, EmissionKind::Arm, EmissionKind::Silent}) {
        if (s == EmissionKindName(k)) {
            out = k;
            return true;
        }
    }
    return false;
}
// The talk table's effect for a regime (data/command/talk_tables.json, ПТ-1).
inline std::string EmissionEffect(EmissionKind k) { return std::string("emission_") + EmissionKindName(k); }

// One regime as ordered: its kind, the search transmitter's bursts, and who ordered it.
struct EmissionRegime {
    EmissionKind kind = EmissionKind::Free;
    double on_turns = 0.0;   // limited, arm: the search transmitter's high voltage on for this many antenna turns
    double off_turns = 0.0;  // then off for this many
    // Limited given in seconds (a scenario's doctrine "search_bursts"): kept as given; the turns are said from them.
    double on_s = 0.0;
    double off_s = 0.0;
    std::string from;  // "regiment", "battery" (the battery commander's decision); "" before anyone ordered one

    bool Bursts() const { return kind == EmissionKind::Limited || kind == EmissionKind::Arm; }
    double OnS(double turn_s) const { return on_s > 0.0 ? on_s : on_turns * turn_s; }
    double OffS(double turn_s) const { return off_s > 0.0 ? off_s : off_turns * turn_s; }
    // The same order (who gave it aside).
    bool Same(const EmissionRegime& o) const {
        if (kind != o.kind) return false;
        if (!Bursts()) return true;
        return std::fabs(on_turns - o.on_turns) < 1e-9 && std::fabs(off_turns - o.off_turns) < 1e-9 && std::fabs(on_s - o.on_s) < 1e-9 &&
               std::fabs(off_s - o.off_s) < 1e-9;
    }
};

// data/command/emission_regimes.json: each regime's words and default bursts, and the chain's times.
struct EmissionDef {
    struct Regime {
        std::string ru;
        std::string en;
        double on_turns = 0.0;
        double off_turns = 0.0;
    };
    Regime free{"без ограничений", "unrestricted", 0.0, 0.0};
    Regime limited{"ограниченный", "limited", 2.0, 6.0};
    Regime arm{"минимальный, угроза ПРР", "least emission, ARM threat", 2.0, 3.0};
    Regime silent{"излучение запрещено", "radiation forbidden", 0.0, 0.0};
    double relay_s = 20.0;          // the battery commander passes the regiment's order to the battery
    double pass_on_s = 4.0;         // the vehicle commander passes the battery's order to his crew
    double repeat_s = 45.0;         // an order on the battery net not heard is said again
    int repeats = 2;
    double arm_on_report_s = 15.0;  // a launcher's report of an ARM attack: the battery under the least emission
    double remind_quiet_s = 120.0;  // the radar seats reminded when a hostile appears after this long without one
    double breach_turns = 1.0;      // a burst this much longer than ordered: the senior operator told again

    const Regime& Of(EmissionKind k) const {
        switch (k) {
            case EmissionKind::Free: return free;
            case EmissionKind::Limited: return limited;
            case EmissionKind::Arm: return arm;
            case EmissionKind::Silent: return silent;
        }
        return free;
    }
    // A regime of this kind with the data's bursts.
    EmissionRegime Make(EmissionKind k, const std::string& from) const {
        EmissionRegime r;
        r.kind = k;
        r.on_turns = Of(k).on_turns;
        r.off_turns = Of(k).off_turns;
        r.from = from;
        return r;
    }
};

// "оборот", "оборота", "оборотов" for a count of turns.
inline const char* TurnsWordRu(int n) {
    const int t = n % 100;
    if (t >= 11 && t <= 14) return "оборотов";
    if (n % 10 == 1) return "оборот";
    if (n % 10 >= 2 && n % 10 <= 4) return "оборота";
    return "оборотов";
}
inline const char* SecondsWordRu(int n) {
    const int t = n % 100;
    if (t >= 11 && t <= 14) return "секунд";
    if (n % 10 == 1) return "секунда";
    if (n % 10 >= 2 && n % 10 <= 4) return "секунды";
    return "секунд";
}

// The words a regime is said in (the callouts' fields): {regime}, {regime_en}, and for bursts {on}, {off} (turns),
// {on_s}, {off_s} (seconds at this antenna's turn) with their Russian nouns, and the whole rule {bursts}, {bursts_en}.
inline std::map<std::string, std::string> EmissionWords(const EmissionDef& d, const EmissionRegime& r, double turn_s) {
    std::map<std::string, std::string> f;
    f["regime"] = d.Of(r.kind).ru;
    f["regime_en"] = d.Of(r.kind).en;
    if (!r.Bursts()) {
        f["bursts"] = r.kind == EmissionKind::Silent ? "радары не включать" : "по обстановке";
        f["bursts_en"] = r.kind == EmissionKind::Silent ? "no radar on" : "as the fight needs";
        return f;
    }
    const double ts = turn_s > 0.0 ? turn_s : 60.0 / 33.0;
    const int on = static_cast<int>(std::lround(r.on_s > 0.0 ? r.on_s / ts : r.on_turns));
    const int off = static_cast<int>(std::lround(r.off_s > 0.0 ? r.off_s / ts : r.off_turns));
    const int on_s = static_cast<int>(std::lround(r.OnS(ts)));
    const int off_s = static_cast<int>(std::lround(r.OffS(ts)));
    f["on"] = std::to_string(on);
    f["off"] = std::to_string(off);
    f["on_s"] = std::to_string(on_s);
    f["off_s"] = std::to_string(off_s);
    char ru[160], en[160];
    std::snprintf(ru, sizeof(ru), "высокое на %d %s (%d %s), через %d %s (%d %s)", on, TurnsWordRu(on), on_s, SecondsWordRu(on_s), off,
                  TurnsWordRu(off), off_s, SecondsWordRu(off_s));
    std::snprintf(en, sizeof(en), "the transmitter on for %d turn%s (%d s), then off for %d turn%s (%d s)", on, on == 1 ? "" : "s", on_s, off,
                  off == 1 ? "" : "s", off_s);
    f["bursts"] = ru;
    f["bursts_en"] = en;
    return f;
}

namespace detail {

class EmissionLoader : public BoardLoader {
public:
    bool Load(const json::Value& root, EmissionDef& out) {
        if (!Keys(root, "", {"regimes_id", "display_name", "status", "notes", "source", "confidence", "regimes", "chain"}) ||
            !Provenance(root, "")) {
            return false;
        }
        const json::Value* rs = root.Find("regimes");
        if (rs == nullptr || !rs->Is(json::Type::Object)) return Fail("regimes", "required, an object");
        for (EmissionKind k : {EmissionKind::Free, EmissionKind::Limited, EmissionKind::Arm, EmissionKind::Silent}) {
            const std::string p = Join("regimes", EmissionKindName(k));
            const json::Value* r = rs->Find(EmissionKindName(k));
            if (r == nullptr || !r->Is(json::Type::Object)) return Fail(p, "required, an object");
            EmissionDef::Regime& x = k == EmissionKind::Free ? out.free : k == EmissionKind::Limited ? out.limited : k == EmissionKind::Arm ? out.arm : out.silent;
            if (!Keys(*r, p, {"ru", "en", "on_turns", "off_turns", "notes"}) || !RequireString(*r, p, "ru", x.ru) || !RequireString(*r, p, "en", x.en)) {
                return false;
            }
            const bool bursts = k == EmissionKind::Limited || k == EmissionKind::Arm;
            if (!bursts && (r->Find("on_turns") != nullptr || r->Find("off_turns") != nullptr)) return Fail(p, "only limited and arm have bursts");
            if (bursts) {
                cite_path_ = p;
                if (!Cited(*r, "on_turns", x.on_turns) || !Cited(*r, "off_turns", x.off_turns)) return false;
                if (x.on_turns <= 0.0 || x.off_turns <= 0.0) return Fail(p, "on_turns and off_turns must be > 0");
            }
        }
        const json::Value* c = root.Find("chain");
        if (c == nullptr || !c->Is(json::Type::Object)) return Fail("chain", "required, an object");
        if (!Keys(*c, "chain", {"relay_s", "pass_on_s", "repeat_s", "repeats", "arm_on_report_s", "remind_quiet_s", "breach_turns", "notes"})) {
            return false;
        }
        cite_path_ = "chain";
        double repeats = 0.0;
        if (!Cited(*c, "relay_s", out.relay_s) || !Cited(*c, "pass_on_s", out.pass_on_s) || !Cited(*c, "repeat_s", out.repeat_s) ||
            !Cited(*c, "repeats", repeats) || !Cited(*c, "arm_on_report_s", out.arm_on_report_s) || !Cited(*c, "remind_quiet_s", out.remind_quiet_s) ||
            !Cited(*c, "breach_turns", out.breach_turns)) {
            return false;
        }
        out.repeats = static_cast<int>(repeats);
        if (out.repeat_s <= 0.0) return Fail("chain.repeat_s", "must be > 0");
        return true;
    }
};

}  // namespace detail

inline bool LoadEmissionFile(const std::string& path, EmissionDef& out, std::string* error = nullptr) {
    return detail::LoadJsonFile<EmissionDef, detail::EmissionLoader>(path, out, error);
}

}  // namespace simcore
