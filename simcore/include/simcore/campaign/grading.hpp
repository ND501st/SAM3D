// simcore/campaign/grading.hpp — the summing up (track O, O5; docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md
// sections 3 (D6) and 7, docs/command/01_PRINCIPLES.md 9).
//
// Each evening at its briefing the regiment sums up the day: its officer grades the battery commander 5 to 2 on what
// reached the regiment of it, from the written record and nothing else. The record is the day's: the combat report the
// post's records make (report.hpp), the regiment's check of the journal and the board against the radar company's log,
// the breaches of the order's limits his log shows (O4's CheckLimits), each task's readiness as he reported it against its
// ready-by, and the kills claimed against what the radar company's log bears out (it says nothing more of a target after
// a kill was claimed: confirmed; it goes on reporting it: contradicted; it never had it: not to be confirmed). Each
// objective's mark comes with its reason as an officer would say it; the day's mark is their mean, held to one above the
// lowest. The marks and the regiment's trust in the battery's reports (report.hpp's ReportTrust, over every check, the
// claims now among them) are the battery's standing with the regiment, which its tasking reads (regiment.hpp's "trusted"
// and "distrusted" rules) and the campaign's save carries.
//
// The battery commander grades each of his launchers' crews by the same kind of rule from his own log (the reports he
// heard: its launches and kills, its spot; the breaches the log shows of it), and writes the marks in his log. Rules and
// lines in data/campaign/grading.json (game values).
#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/campaign/briefings.hpp"
#include "simcore/campaign/report.hpp"
#include "simcore/console_data.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace campaign {

struct GradingRules {
    std::string speaker = "rgt_commander";  // the regiment's officer who sums up (a position of the chain)
    double late_5_s = 0.0;     // readiness: reported at most this late for each mark
    double late_4_s = 600.0;
    double late_3_s = 1800.0;
    int breaches_5 = 0;        // the order's limits: at most this many breaches in the log for each mark
    int breaches_4 = 1;
    int breaches_3 = 2;
    double confirm_s = 180.0;  // the radar company's log silent on a target this long after a kill claimed: confirmed
    double share_5 = 1.0;      // kills: the share the log bears out for each mark
    double share_4 = 0.75;
    double share_3 = 0.5;
    int above_lowest = 1;      // the day's mark at most this far above the worst objective's
    std::map<std::string, std::string> lines;       // the officer's lines by case ({field}s filled)
    double crew_lead_h = 1.5;  // the battery commander grades his crews this long before the regiment's briefing
    int per_breach = 1;        // a crew's mark off for each breach his log shows of it
    int per_retracted = 1;     // and for each kill it reported and took back
    std::map<std::string, std::string> crew_lines;
    std::string Line(const std::string& id) const {
        const auto it = lines.find(id);
        return it != lines.end() ? it->second : std::string();
    }
    std::string CrewLine(const std::string& id) const {
        const auto it = crew_lines.find(id);
        return it != crew_lines.end() ? it->second : std::string();
    }
};

namespace detail {

class GradingLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, GradingRules& out) {
        if (!Keys(root, "", {"grading_id", "status", "notes", "source", "confidence", "speaker", "ready", "limits", "claims", "overall", "lines", "crews"}) ||
            !Provenance(root, "") || !OptionalString(root, "", "speaker", out.speaker)) {
            return false;
        }
        const json::Value* ready = Section(root, "ready");
        const json::Value* limits = Section(root, "limits");
        const json::Value* claims = Section(root, "claims");
        const json::Value* overall = Section(root, "overall");
        if (ready == nullptr || limits == nullptr || claims == nullptr || overall == nullptr ||
            !Keys(*ready, "ready", {"late_5_s", "late_4_s", "late_3_s", "notes"}) ||
            !Keys(*limits, "limits", {"breaches_5", "breaches_4", "breaches_3", "notes"}) ||
            !Keys(*claims, "claims", {"confirm_s", "share_5", "share_4", "share_3", "notes"}) || !Keys(*overall, "overall", {"above_lowest", "notes"})) {
            return false;
        }
        double b5 = 0.0, b4 = 0.0, b3 = 0.0, above = 0.0;
        if (!Value(*ready, "ready", "late_5_s", out.late_5_s) || !Value(*ready, "ready", "late_4_s", out.late_4_s) ||
            !Value(*ready, "ready", "late_3_s", out.late_3_s) || !Value(*limits, "limits", "breaches_5", b5) ||
            !Value(*limits, "limits", "breaches_4", b4) || !Value(*limits, "limits", "breaches_3", b3) ||
            !Value(*claims, "claims", "confirm_s", out.confirm_s) || !Value(*claims, "claims", "share_5", out.share_5) ||
            !Value(*claims, "claims", "share_4", out.share_4) || !Value(*claims, "claims", "share_3", out.share_3) ||
            !Value(*overall, "overall", "above_lowest", above)) {
            return false;
        }
        out.breaches_5 = static_cast<int>(b5);
        out.breaches_4 = static_cast<int>(b4);
        out.breaches_3 = static_cast<int>(b3);
        out.above_lowest = static_cast<int>(above);
        if (!(out.late_5_s <= out.late_4_s && out.late_4_s <= out.late_3_s)) return Fail("ready", "late_5_s <= late_4_s <= late_3_s");
        if (!(out.breaches_5 <= out.breaches_4 && out.breaches_4 <= out.breaches_3)) return Fail("limits", "breaches_5 <= breaches_4 <= breaches_3");
        if (!(out.share_5 <= 1.0 && out.share_4 <= out.share_5 && out.share_3 <= out.share_4)) return Fail("claims", "share_3 <= share_4 <= share_5 <= 1");
        if (!Lines(root, "lines", {"ready_in_time", "ready_late", "ready_never", "records", "limits_none", "limits_some", "claims", "overall", "nothing"},
                   out.lines)) {
            return false;
        }
        const json::Value* crews = Section(root, "crews");
        if (crews == nullptr || !Keys(*crews, "crews", {"lead_h", "per_breach", "per_retracted", "lines", "notes"})) return false;
        double pb = 0.0, pr = 0.0;
        if (!Value(*crews, "crews", "lead_h", out.crew_lead_h) || !Value(*crews, "crews", "per_breach", pb) ||
            !Value(*crews, "crews", "per_retracted", pr) || !Lines(*crews, "crews.lines", {"none", "launches", "breach", "retracted"}, out.crew_lines)) {
            return false;
        }
        out.per_breach = static_cast<int>(pb);
        out.per_retracted = static_cast<int>(pr);
        if (!(out.crew_lead_h > 0.0 && out.crew_lead_h < 24.0)) return Fail("crews.lead_h", "0 to 24 hours");
        return true;
    }

private:
    const json::Value* Section(const json::Value& root, const char* key) {
        const json::Value* s = root.Find(key);
        if (s == nullptr || !s->Is(json::Type::Object)) {
            Fail(key, "required");
            return nullptr;
        }
        return s;
    }
    bool Value(const json::Value& obj, const std::string& path, const char* key, double& out) {
        const std::string p = Join(path, key);
        const json::Value* v = obj.Find(key);
        if (v == nullptr) return Fail(p, "required");
        if (!Keys(*v, p, {"value", "source", "confidence", "notes"}) || !Provenance(*v, p)) return false;
        const json::Value* n = v->Find("value");
        if (n == nullptr || !n->Is(json::Type::Number) || !(n->number >= 0.0)) return Fail(Join(p, "value"), "a number >= 0");
        out = n->number;
        return true;
    }
    bool Lines(const json::Value& obj, const std::string& path, std::initializer_list<const char*> ids, std::map<std::string, std::string>& out) {
        const std::string leaf = path.substr(path.rfind('.') == std::string::npos ? 0 : path.rfind('.') + 1);
        const json::Value* l = obj.Find(leaf.c_str());
        if (l == nullptr || !l->Is(json::Type::Object)) return Fail(path, "required: the lines");
        for (const char* id : ids) {
            const json::Value* s = l->Find(id);
            if (s == nullptr || !s->Is(json::Type::String) || s->string.empty()) return Fail(Join(path, id), "required: a line");
            out[id] = s->string;
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadGradingRules(const std::string& data_dir, const std::string& rel, GradingRules& out, std::string* error = nullptr) {
    std::string text;
    json::Value root;
    std::string e;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text) || !json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "grading: cannot read " + rel + (e.empty() ? "" : ": " + e);
        return false;
    }
    detail::GradingLoader loader;
    if (!loader.Load(root, out)) {
        if (error != nullptr) *error = "grading: " + loader.Message();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------- kills claimed

// A kill the battery commander's log holds (reported destroyed and not taken back) against the radar company's log.
struct ClaimFinding {
    double time_s = 0.0;
    std::string callsign;
    int number = 0;
    std::string status;  // "confirmed", "contradicted" (reported again after it), "unknown" (never in its log), "pending"
};
struct ClaimCheck {
    std::vector<ClaimFinding> claims;
    int Count(const char* status) const {
        int n = 0;
        for (const ClaimFinding& c : claims) n += c.status == status ? 1 : 0;
        return n;
    }
    int Confirmed() const { return Count("confirmed"); }
    int Contradicted() const { return Count("contradicted"); }
};

// The kills claimed in `log` checked against what the radar company said on the air-picture net (`radar`), by `to_s`:
// a target it had reported, and said nothing more of once `confirm_s` had passed after the claim, is confirmed by its
// log; one it went on reporting is contradicted; one it never reported cannot be confirmed by it; one claimed less than
// `confirm_s` before `to_s` is pending. All from the two records as written.
inline ClaimCheck CheckClaims(const std::vector<LogEntry>& log, const std::vector<SaidReport>& radar, double to_s, const GradingRules& g) {
    ClaimCheck c;
    for (std::size_t i = 0; i < log.size(); ++i) {
        const LogEntry& e = log[i];
        if (e.kind != "destroyed") continue;
        bool taken_back = false;
        for (std::size_t k = i + 1; k < log.size(); ++k) {
            const LogEntry& b = log[k];
            if ((b.kind == "not destroyed" || b.kind == "hit") && b.callsign == e.callsign && b.number == e.number) taken_back = true;
        }
        if (taken_back) continue;
        ClaimFinding f;
        f.time_s = e.time_s;
        f.callsign = e.callsign;
        f.number = e.number;
        bool before = false, after = false;
        for (const SaidReport& s : radar) {
            if (e.number <= 0 || s.number != e.number) continue;
            if (s.time_s <= e.time_s) before = true;
            if (s.time_s > e.time_s + g.confirm_s) after = true;
        }
        f.status = !before ? "unknown" : after ? "contradicted" : to_s < e.time_s + g.confirm_s ? "pending" : "confirmed";
        c.claims.push_back(f);
    }
    return c;
}

// The regiment's trust takes the claims as checked entries: a confirmed kill agrees, a contradicted one does not.
inline void TrustClaims(ReportTrust& trust, const std::string& unit, const ClaimCheck& c) {
    RecordCheck r;
    r.record = "claims";
    r.said = c.Confirmed() + c.Contradicted();
    r.as_said = c.Confirmed();
    trust.Checked(unit, r);
}

// ---------------------------------------------------------------------------------------------- the day's record

// A task's readiness as the battery commander reported it up (ready_s < 0: never), against the ready-by the order gave.
struct TaskReadiness {
    std::string task;
    double given_s = 0.0;
    double ready_by_s = 0.0;
    double ready_s = -1.0;
    bool held = false;  // held from before (a campaign resumed): summed up on the day it was given
};

// What reached the regiment of the battery's day, as written.
struct DayRecord {
    std::string unit = "Battery 21";
    double from_s = 0.0;
    double to_s = 0.0;
    CombatReport report;
    bool has_journal = false;  // the post keeps a radio journal (checked with the board)
    RecordCheck journal;
    RecordCheck board;
    std::vector<LimitFinding> breaches;
    std::vector<TaskReadiness> readiness;  // the tasks given in the day
    ClaimCheck claims;
};

struct ObjectiveGrade {
    std::string id;   // "ready", "records", "limits", "claims"
    int grade = 0;    // 5 to 2
    std::string line; // the officer's words, the mark's word at their end
};
struct SummingUp {
    std::string unit;
    double at_s = 0.0;       // the session second it was made
    double campaign_h = -1.0;
    int grade = 0;           // the day's mark (0: nothing to sum up)
    std::vector<ObjectiveGrade> objectives;
    std::vector<std::string> lines;  // as said at the briefing, in order
    const ObjectiveGrade* Find(const std::string& id) const {
        for (const ObjectiveGrade& o : objectives) {
            if (o.id == id) return &o;
        }
        return nullptr;
    }
};

namespace detail {

inline std::string MarkWord(int grade) {
    std::string w = GradeWord(grade);
    if (!w.empty()) w[0] = static_cast<char>(w[0] - 'a' + 'A');
    return w;
}
inline std::string Minutes(double s) {
    char b[32];
    std::snprintf(b, sizeof(b), "%.0f", std::fabs(s) / 60.0);
    return b;
}
inline int BandHigh(double v, double b5, double b4, double b3) { return v <= b5 + 1e-9 ? 5 : v <= b4 + 1e-9 ? 4 : v <= b3 + 1e-9 ? 3 : 2; }
inline int BandLow(double v, double b5, double b4, double b3) { return v + 1e-9 >= b5 ? 5 : v + 1e-9 >= b4 ? 4 : v + 1e-9 >= b3 ? 3 : 2; }

}  // namespace detail

// The day's marks with their reasons, from the record alone (`clock`: a session second as the day's time).
inline SummingUp GradeDay(const DayRecord& d, const GradingRules& g, const AfterActionValues& av, const std::function<std::string(double)>& clock) {
    SummingUp s;
    s.unit = d.unit;
    s.at_s = d.to_s;
    const auto add = [&](const std::string& id, int grade, const std::string& line) {
        s.objectives.push_back({id, grade, line + " " + detail::MarkWord(grade) + "."});
    };
    // Readiness: the worst of the day's tasks, as reported.
    const TaskReadiness* worst = nullptr;
    double worst_late = -1e18;
    for (const TaskReadiness& t : d.readiness) {
        if (t.held) continue;
        const double late = t.ready_s < 0.0 ? 1e18 : t.ready_s - t.ready_by_s;
        if (worst == nullptr || late > worst_late) {
            worst = &t;
            worst_late = late;
        }
    }
    if (worst != nullptr) {
        std::map<std::string, std::string> f{{"by", clock(worst->ready_by_s)}};
        std::string line;
        if (worst->ready_s < 0.0) {
            line = FillLine(g.Line("ready_never"), f);
        } else {
            f["ready"] = clock(worst->ready_s);
            f[worst_late <= 0.0 ? "spare" : "late"] = detail::Minutes(worst_late);
            line = FillLine(g.Line(worst_late <= 0.0 ? "ready_in_time" : "ready_late"), f);
        }
        add("ready", worst->ready_s < 0.0 ? 2 : detail::BandHigh(std::max(0.0, worst_late), g.late_5_s, g.late_4_s, g.late_3_s), line);
    }
    // The records against the radar company's log (P9's check: the journal and the board together).
    RecordCheck rc;
    rc.record = "records";
    for (const RecordCheck* c : {d.has_journal ? &d.journal : nullptr, &d.board}) {
        if (c == nullptr) continue;
        rc.said += c->said;
        rc.as_said += c->as_said;
        rc.wrong += c->wrong;
        rc.missing += c->missing;
        rc.ghosts += c->ghosts;
    }
    if (rc.Checked() > 0) {
        const std::map<std::string, std::string> f{{"as_said", std::to_string(rc.as_said)}, {"said", std::to_string(rc.said)},
                                                   {"wrong", std::to_string(rc.wrong)},     {"missing", std::to_string(rc.missing)},
                                                   {"ghosts", std::to_string(rc.ghosts)}};
        add("records", RecordGrade(rc, av), FillLine(g.Line("records"), f));
    }
    // The order's limits as the log shows them: graded once there is an order to keep.
    if (worst != nullptr || !d.breaches.empty()) {
        const int n = static_cast<int>(d.breaches.size());
        std::string line;
        if (n == 0) {
            line = g.Line("limits_none");
        } else {
            std::string first = d.breaches.front().text;
            if (!first.empty()) first[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(first[0])));
            line = FillLine(g.Line("limits_some"), {{"count", std::to_string(n) + (n == 1 ? " breach" : " breaches")}, {"first", first}});
        }
        add("limits", detail::BandHigh(static_cast<double>(n), g.breaches_5, g.breaches_4, g.breaches_3), line);
    }
    // Kills claimed against the radar company's log.
    if (!d.claims.claims.empty()) {
        const int n = static_cast<int>(d.claims.claims.size());
        const int ok = d.claims.Confirmed();
        const int against = d.claims.Contradicted();
        const std::string against_words = against > 0 ? "; it goes on reporting " + std::to_string(against) + " of them" : std::string();
        add("claims", detail::BandLow(static_cast<double>(ok) / n, g.share_5, g.share_4, g.share_3),
            FillLine(g.Line("claims"), {{"claims", std::to_string(n)}, {"confirmed", std::to_string(ok)}, {"against", against_words}}));
    }
    if (s.objectives.empty()) {
        s.lines.push_back(FillLine(g.Line("nothing"), {{"unit", d.unit}}));
        return s;
    }
    int sum = 0, lowest = 5;
    for (const ObjectiveGrade& o : s.objectives) {
        sum += o.grade;
        lowest = std::min(lowest, o.grade);
        s.lines.push_back(o.line);
    }
    const double mean = static_cast<double>(sum) / static_cast<double>(s.objectives.size());
    s.grade = std::min(static_cast<int>(std::floor(mean + 0.5)), lowest + g.above_lowest);
    s.grade = std::max(2, std::min(5, s.grade));
    s.lines.push_back(FillLine(g.Line("overall"), {{"unit", d.unit}, {"grade", GradeWord(s.grade)}}));
    return s;
}

// ---------------------------------------------------------------------------------------------- the battery's standing

// The regiment's view of the battery, kept from day to day and in the campaign's save: its trust in the battery's reports
// (report.hpp's ReportTrust's counts: entries checked and agreed over every check) and each day's mark.
struct DayMark {
    double campaign_h = -1.0;
    int grade = 0;
    std::vector<std::pair<std::string, int>> objectives;
    std::vector<std::string> lines;
};
struct RegimentStanding {
    std::string unit = "Battery 21";
    int checked = 0;
    int agreed = 0;
    int checks = 0;
    double summed_h = -1.0;  // the campaign hour of the latest summing up, with or without a mark (< 0: none yet)
    std::vector<DayMark> days;

    // As ReportTrust::Of: the share agreed over the prior and every check.
    double Trust(const AfterActionValues& v) const {
        const double c = v.trust_prior_checked + checked;
        return c > 0.0 ? (v.trust_prior_agreed + agreed) / c : 1.0;
    }
    int LastGrade() const { return days.empty() ? 0 : days.back().grade; }
    double LastH() const { return days.empty() ? -1.0 : days.back().campaign_h; }
    // The day summed up: its mark, and its checks into the trust.
    void Add(const SummingUp& s, const DayRecord& d) {
        ReportTrust t;
        if (d.has_journal) t.Checked(unit, d.journal);
        t.Checked(unit, d.board);
        TrustClaims(t, unit, d.claims);
        const ReportTrust::Count c = t.CountOf(unit);
        checked += c.checked;
        agreed += c.agreed;
        checks += c.checks;
        summed_h = s.campaign_h;
        if (s.grade <= 0) return;
        DayMark m;
        m.campaign_h = s.campaign_h;
        m.grade = s.grade;
        for (const ObjectiveGrade& o : s.objectives) m.objectives.push_back({o.id, o.grade});
        m.lines = s.lines;
        days.push_back(m);
    }
};

// ---------------------------------------------------------------------------------------------- the crews' marks

struct CrewGrade {
    double time_s = 0.0;
    std::string crew;               // the launcher's callsign
    int grade = 5;
    std::vector<std::string> reasons;
    bool by_player = false;         // the player set it (else the rule's)
    double campaign_h = -1.0;       // when, in the campaign's hours (a save's; < 0: this session's time_s)
};

// Each crew's mark from the battery commander's log since `from_s` and the breaches it shows: from 5, a mark off for each
// breach of the crew's and each kill it reported and took back, at least 2; the reasons in his words.
inline std::vector<CrewGrade> GradeCrews(const std::vector<std::string>& crews, const std::vector<LogEntry>& log,
                                         const std::vector<LimitFinding>& breaches, double from_s, double now_s, const GradingRules& g) {
    std::vector<CrewGrade> out;
    for (const std::string& crew : crews) {
        CrewGrade c;
        c.time_s = now_s;
        c.crew = crew;
        int launches = 0, destroyed = 0, retracted = 0;
        for (const LogEntry& e : log) {
            if (e.time_s < from_s || e.callsign != crew) continue;
            if (e.kind == "launch") ++launches;
            if (e.kind == "destroyed") ++destroyed;
            if (e.kind == "not destroyed" || e.kind == "hit") {
                --destroyed;
                ++retracted;
            }
        }
        int off = g.per_retracted * retracted;
        const std::map<std::string, std::string> f{{"crew", crew}, {"launches", std::to_string(launches)},
                                                   {"destroyed", std::to_string(std::max(0, destroyed))}, {"n", std::to_string(retracted)}};
        if (launches > 0) c.reasons.push_back(FillLine(g.CrewLine("launches"), f));
        for (const LimitFinding& b : breaches) {
            if (b.time_s < from_s || b.who != crew) continue;
            off += g.per_breach;
            std::map<std::string, std::string> fb = f;
            fb["text"] = b.text;
            c.reasons.push_back(FillLine(g.CrewLine("breach"), fb));
        }
        if (retracted > 0) c.reasons.push_back(FillLine(g.CrewLine("retracted"), f));
        if (off == 0 && launches == 0) c.reasons.push_back(FillLine(g.CrewLine("none"), f));
        c.grade = std::max(2, 5 - off);
        out.push_back(c);
    }
    return out;
}

inline std::string CrewGradeText(const CrewGrade& c) {
    std::string s = c.crew + ": " + GradeWord(c.grade) + " (" + std::to_string(c.grade) + ")" + (c.by_player ? ", his own mark." : ".");
    for (const std::string& r : c.reasons) s += " " + r;
    return s;
}

}  // namespace campaign
}  // namespace simcore
