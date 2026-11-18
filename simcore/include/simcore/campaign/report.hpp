// simcore/campaign/report.hpp — the battery's combat report from its records, the regiment's check of them, and the
// trust it keeps in each unit's reports (roadmap P9 and 18.1; docs/command/01_PRINCIPLES.md 9,
// docs/campaign/05_THE_SYSTEMS.md §11).
//
// The written record is the war's memory. The report is compiled from what the battery's people wrote at the time: the
// radiotelephonist's journal (radio_journal.hpp: every air report with the time he heard it, as he wrote it), the
// plotter's marks on the board, and the battery commander's log of his launchers' reports (launches, results). The
// report never sees the truth: the record types here carry nothing the page or the board doesn't show, and a line is
// read as written. So a digit written wrong puts a target in the wrong square or gives it a second number, a line lost
// unwritten leaves a gap, and a raid nobody wrote down is not in the report at all.
//
// The regiment checks a record against another (Dedyaev, BGUIR 2010, sets an operator's written record against a live
// plot; docs/research/PLOTTING_BOARD_2026-10.md): here the journal and the board against the radar company's log of what
// it said on the air-picture net. Each report said is found in the record word for word, found with a digit wrong,
// or missing; a line or a mark that answers to nothing said is a ghost. The check's agreement grades the crew's records
// (5 to 2, the Soviet marks) and moves the regiment's trust in the unit's reports, a belief kept over every check
// (`ReportTrust`): a unit whose records have proved wrong is believed less. Game values in data/gameplay/after_action.json.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/json.hpp"
#include "simcore/pu12.hpp"
#include "simcore/radio_journal.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace campaign {

struct AfterActionValues {
    double raid_gap_s = 600.0;  // a target first written longer than this after the raid's last line opens a new raid
    double match_s = 20.0;      // a journal line answers to a report said within this of it
    double board_s = 90.0;      // a board mark answers to a report said up to this before it
    double grade_5 = 0.95;      // the agreement for each mark
    double grade_4 = 0.85;
    double grade_3 = 0.70;
    double trust_prior_checked = 5.0;  // the regiment's belief before a unit's first check: agreed of checked
    double trust_prior_agreed = 4.0;
};

namespace detail {

class AfterActionLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, AfterActionValues& out) {
        if (!Keys(root, "", {"after_action_id", "status", "notes", "source", "confidence", "values"}) || !Provenance(root, "")) return false;
        const json::Value* values = root.Find("values");
        if (values == nullptr) return Fail("values", "required");
        const std::pair<const char*, double*> each[] = {
            {"raid_gap_s", &out.raid_gap_s}, {"match_s", &out.match_s}, {"board_s", &out.board_s},
            {"grade_5", &out.grade_5},       {"grade_4", &out.grade_4}, {"grade_3", &out.grade_3},
            {"trust_prior_checked", &out.trust_prior_checked}, {"trust_prior_agreed", &out.trust_prior_agreed}};
        if (!Keys(*values, "values", {"raid_gap_s", "match_s", "board_s", "grade_5", "grade_4", "grade_3", "trust_prior_checked",
                                      "trust_prior_agreed"})) {
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
        if (!(out.grade_5 <= 1.0 && out.grade_4 < out.grade_5 && out.grade_3 < out.grade_4)) {
            return Fail("values", "the grades' agreements must fall from grade_5 (at most 1) to grade_3");
        }
        if (out.trust_prior_agreed > out.trust_prior_checked) return Fail("values.trust_prior_agreed", "at most trust_prior_checked");
        return true;
    }
};

}  // namespace detail

inline bool LoadAfterActionValues(const std::string& data_dir, const std::string& rel, AfterActionValues& out, std::string* error = nullptr) {
    std::string text;
    if (!simcore::detail::ReadTextFile(data_dir + "/" + rel, text)) {
        if (error != nullptr) *error = "after action: cannot read " + rel;
        return false;
    }
    json::Value root;
    std::string e;
    if (!json::Parse(text, root, &e)) {
        if (error != nullptr) *error = "after action: " + e;
        return false;
    }
    detail::AfterActionLoader loader;
    if (!loader.Load(root, out)) {
        if (error != nullptr) *error = "after action: " + loader.Message();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------- the records

// A line of the radiotelephonist's journal as the page has it: the time column, the net, who passed it, what was written.
struct JournalLine {
    double heard_s = 0.0;
    std::string net;
    std::string from;
    std::string text;
};
// A mark on the board as it stands: its number, the minute's time, where it is drawn, the height and code under it.
struct BoardRecord {
    int number = 0;
    double time_s = 0.0;
    Vec3 position;
    int height_hm = 0;
    int type = 9;
    int formation = 1;
};
// A line of the battery commander's log: a launcher's report as he heard it.
struct LogEntry {
    double time_s = 0.0;
    std::string callsign;
    std::string kind;  // "launch", "destroyed", "missed", "not destroyed" (a kill taken back)
    int number = 0;    // the target's board number as reported (0: not said)
};
// A report the radar company said on the air-picture net, as its own log has it.
struct SaidReport {
    double time_s = 0.0;
    int number = 0;
    std::string text;
};

// The journal's legible lines by `t`, as written (what was said stays out of the record).
inline std::vector<JournalLine> JournalLines(const RadioJournal& journal, double t) {
    std::vector<JournalLine> out;
    for (const RadioJournalEntry& e : journal.Legible(t)) out.push_back({e.heard_s, e.net, e.from, e.text});
    return out;
}
// The board's marks as drawn (who each target really is stays out of the record).
inline std::vector<BoardRecord> BoardRecords(const std::vector<BoardMark>& marks) {
    std::vector<BoardRecord> out;
    for (const BoardMark& m : marks) out.push_back({m.number, m.time_s, m.position, m.height_hm, m.type, m.formation});
    return out;
}

// An air-picture report as a line of the journal has it (command_post.hpp's forms: by square or by azimuth and range,
// or the short form, the number only). False for a line that doesn't read as one (a gap of dashes, another message).
struct WrittenAirReport {
    int number = 0;
    std::string place;   // "square 4215" or "azimuth 045, 32 km" ("" in the short form)
    int code = -1;       // identity and size (-1: not written)
    int height_hm = -1;
    int minute = -1;
    bool brief = false;
};
inline bool ReadAirReport(const std::string& text, WrittenAirReport& out) {
    std::vector<std::string> printed, groups;
    RadioJournal::Cut(text, printed, groups);
    const auto form = [&](std::initializer_list<const char*> want) {
        if (printed.size() != want.size()) return false;
        std::size_t i = 0;
        for (const char* w : want) {
            if (printed[i++] != w) return false;
        }
        return true;
    };
    out = WrittenAirReport();
    if (form({"", ": square ", ", ", ", height ", ", minute ", ""})) {
        out.number = std::atoi(groups[0].c_str());
        out.place = "square " + groups[1];
        out.code = std::atoi(groups[2].c_str());
        out.height_hm = std::atoi(groups[3].c_str());
        out.minute = std::atoi(groups[4].c_str());
    } else if (form({"", ": azimuth ", ", ", " km, ", ", height ", ", minute ", ""})) {
        out.number = std::atoi(groups[0].c_str());
        out.place = "azimuth " + groups[1] + ", " + groups[2] + " km";
        out.code = std::atoi(groups[3].c_str());
        out.height_hm = std::atoi(groups[4].c_str());
        out.minute = std::atoi(groups[5].c_str());
    } else if (form({"", " — кратко"})) {
        out.number = std::atoi(groups[0].c_str());
        out.brief = true;
    } else {
        return false;
    }
    return out.number > 0;
}

// ---------------------------------------------------------------------------------------------- the report

struct ReportTarget {
    int number = 0;
    double first_s = 0.0;
    double last_s = 0.0;
    std::vector<std::string> places;  // in order, as written (a place repeated is written once)
    int type = -1;                    // the code's tens digit as written (-1: never written)
    int formation = 0;                // the largest size written
    int height_hm = -1;               // the last height written
    int lines = 0;                    // journal lines
    int marks = 0;                    // board marks
    bool from_board = false;          // its places are the board's (the journal has none)
};
struct ReportRaid {
    double first_s = 0.0;
    double last_s = 0.0;
    std::vector<int> numbers;
    int aircraft = 0;  // as written (a target of unwritten size counts one)
};
struct CombatReport {
    std::string unit;
    double from_s = 0.0;
    double to_s = 0.0;
    std::vector<ReportTarget> targets;  // hostile or unidentified, by first written
    std::vector<int> friends;           // numbers written as our own aircraft
    std::vector<ReportRaid> raids;
    std::vector<LogEntry> claims;       // the launchers' reports
    int missiles = 0;                   // launches reported
    int destroyed = 0;                  // kills reported and not taken back
    int unreadable = 0;                 // air-picture lines of the journal that don't read as a report

    const ReportTarget* Target(int number) const {
        for (const ReportTarget& t : targets) {
            if (t.number == number) return &t;
        }
        return nullptr;
    }
    int Aircraft() const {
        int n = 0;
        for (const ReportRaid& r : raids) n += r.aircraft;
        return n;
    }
};

// How the post writes a place on the board (square or azimuth and range: command_post.hpp's PlaceWords). Null: a target
// only on the board is reported without its places.
using PlaceOf = std::function<std::string(const Vec3&)>;

// The report from the records. The journal first: a target's places, identity, size and height are the journal's where it
// has them, the board's where it has not; its times are from both. Friends apart; the rest grouped into raids by time.
inline CombatReport CompileReport(const std::string& unit, double from_s, double to_s, const std::vector<JournalLine>& journal,
                                  const std::vector<BoardRecord>& board, const std::vector<LogEntry>& log, const AfterActionValues& v,
                                  const PlaceOf& place_of = nullptr) {
    CombatReport r;
    r.unit = unit;
    r.from_s = from_s;
    r.to_s = to_s;
    std::map<int, ReportTarget> by_number;
    const auto seen = [&](int number, double t) -> ReportTarget& {
        ReportTarget& x = by_number[number];
        if (x.number == 0) {
            x.number = number;
            x.first_s = t;
            x.last_s = t;
        }
        x.first_s = std::min(x.first_s, t);
        x.last_s = std::max(x.last_s, t);
        return x;
    };
    const auto place = [](ReportTarget& x, const std::string& p) {
        if (!p.empty() && (x.places.empty() || x.places.back() != p)) x.places.push_back(p);
    };
    for (const JournalLine& l : journal) {
        if (l.net != "air picture") continue;
        WrittenAirReport w;
        if (!ReadAirReport(l.text, w)) {
            ++r.unreadable;
            continue;
        }
        ReportTarget& x = seen(w.number, l.heard_s);
        ++x.lines;
        place(x, w.place);
        if (w.code >= 0) {
            x.type = w.code / 10;
            x.formation = std::max(x.formation, w.code % 10);
        }
        if (w.height_hm >= 0) x.height_hm = w.height_hm;
    }
    for (const BoardRecord& m : board) {
        if (m.number <= 0) continue;
        const bool journal_has = by_number.count(m.number) != 0 && by_number[m.number].lines > 0;
        ReportTarget& x = seen(m.number, m.time_s);
        ++x.marks;
        if (journal_has) continue;
        x.from_board = true;
        if (place_of) place(x, place_of(m.position));
        x.type = m.type;
        x.formation = std::max(x.formation, m.formation);
        x.height_hm = m.height_hm;
    }
    std::vector<ReportTarget> all;
    for (auto& e : by_number) {
        if (e.second.type == 1) {
            r.friends.push_back(e.first);
        } else {
            all.push_back(e.second);
        }
    }
    std::stable_sort(all.begin(), all.end(), [](const ReportTarget& a, const ReportTarget& b) { return a.first_s < b.first_s; });
    r.targets = all;
    for (const ReportTarget& x : r.targets) {
        if (r.raids.empty() || x.first_s > r.raids.back().last_s + v.raid_gap_s) {
            ReportRaid raid;
            raid.first_s = x.first_s;
            raid.last_s = x.last_s;
            r.raids.push_back(raid);
        }
        ReportRaid& raid = r.raids.back();
        raid.numbers.push_back(x.number);
        raid.last_s = std::max(raid.last_s, x.last_s);
        raid.aircraft += std::max(1, x.formation);
    }
    for (const LogEntry& e : log) {
        r.claims.push_back(e);
        if (e.kind == "launch") ++r.missiles;
        if (e.kind == "destroyed") ++r.destroyed;
        if (e.kind == "not destroyed") --r.destroyed;
    }
    r.destroyed = std::max(0, r.destroyed);
    return r;
}

inline std::string BoardNumber(int number) {
    char n[64];
    std::snprintf(n, sizeof(n), "%04d", number);
    return n;
}

// The report in full, its times by `clock` (session seconds to the report's time of day).
inline std::string ReportText(const CombatReport& r, const std::function<std::string(double)>& clock) {
    std::string s = r.unit + "'s combat report from its records, " + clock(r.from_s) + " to " + clock(r.to_s) + ": ";
    if (r.raids.empty()) {
        s += "no raid written.";
    } else {
        s += std::to_string(r.raids.size()) + (r.raids.size() == 1 ? " raid." : " raids.");
    }
    for (std::size_t i = 0; i < r.raids.size(); ++i) {
        const ReportRaid& raid = r.raids[i];
        s += " Raid " + std::to_string(i + 1) + ", " + clock(raid.first_s) + " to " + clock(raid.last_s) + ": " + std::to_string(raid.aircraft) +
             " aircraft as written";
        for (int number : raid.numbers) {
            const ReportTarget* x = r.Target(number);
            if (x == nullptr) continue;
            s += "; " + BoardNumber(number);
            if (x->type == 8) s += " hostile";
            if (x->type == 9 || x->type < 0) s += " unidentified";
            if (x->type == 0) s += " jamming";
            if (x->formation > 1) s += " x" + std::to_string(x->formation);
            if (!x->places.empty()) {
                s += x->places.size() == 1 ? " in " + x->places.front() : " from " + x->places.front() + " to " + x->places.back();
            }
            if (x->height_hm >= 0) s += " at height " + std::to_string(x->height_hm);
            if (x->from_board) s += " (the board only)";
        }
        s += ".";
    }
    if (!r.friends.empty()) {
        s += " Our own:";
        for (int n : r.friends) s += " " + BoardNumber(n);
        s += ".";
    }
    s += " Missiles launched: " + std::to_string(r.missiles) + "; targets destroyed: " + std::to_string(r.destroyed);
    std::string kills;
    for (const LogEntry& e : r.claims) {
        if (e.kind == "destroyed") kills += (kills.empty() ? " (" : ", ") + (e.number > 0 ? BoardNumber(e.number) : std::string("a target")) + " by " + e.callsign;
    }
    s += kills.empty() ? "." : kills + ").";
    if (r.unreadable > 0) s += " Lines of the journal that do not read: " + std::to_string(r.unreadable) + ".";
    return s;
}
// As told by voice.
inline std::string ReportSummary(const CombatReport& r) {
    char buf[200];
    std::snprintf(buf, sizeof(buf), "%s: %d raid%s, %d aircraft, %d missile%s launched, %d target%s destroyed.", r.unit.c_str(),
                  static_cast<int>(r.raids.size()), r.raids.size() == 1 ? "" : "s", r.Aircraft(), r.missiles, r.missiles == 1 ? "" : "s",
                  r.destroyed, r.destroyed == 1 ? "" : "s");
    return buf;
}

// ---------------------------------------------------------------------------------------------- the regiment's check

struct RecordCheck {
    std::string record;  // "journal" or "board"
    int said = 0;        // reports in the radar company's log
    int as_said = 0;     // found as said (a journal line word for word; a mark under its number)
    int wrong = 0;       // found with a digit wrong (a line in its time not as said; a mark in its time under another number)
    int missing = 0;     // said and not in the record
    int ghosts = 0;      // in the record and never said

    int Checked() const { return said + ghosts; }
    double Agreement() const { return Checked() > 0 ? static_cast<double>(as_said) / Checked() : 1.0; }
};

namespace detail {

// Each report said takes, in turn, an entry of the record as said, then one under its number, then the nearest, each
// within its window; an entry left over is a ghost.
inline RecordCheck MatchToLog(const std::string& name, const std::vector<SaidReport>& log, std::size_t entries,
                              const std::function<bool(const SaidReport&, std::size_t)>& in_window,
                              const std::function<double(const SaidReport&, std::size_t)>& distance,
                              const std::function<bool(const SaidReport&, std::size_t)>& as_said,
                              const std::function<bool(const SaidReport&, std::size_t)>& same_number) {
    RecordCheck c;
    c.record = name;
    c.said = static_cast<int>(log.size());
    std::vector<bool> taken(entries, false);
    std::vector<int> pass_of(log.size(), -1);
    for (int pass = 0; pass < 3; ++pass) {
        for (std::size_t i = 0; i < log.size(); ++i) {
            if (pass_of[i] >= 0) continue;
            std::size_t best = entries;
            double best_d = 1e18;
            for (std::size_t k = 0; k < entries; ++k) {
                if (taken[k] || !in_window(log[i], k)) continue;
                if ((pass == 0 && !as_said(log[i], k)) || (pass == 1 && !same_number(log[i], k))) continue;
                const double d = distance(log[i], k);
                if (d < best_d) {
                    best = k;
                    best_d = d;
                }
            }
            if (best == entries) continue;
            taken[best] = true;
            pass_of[i] = pass;
        }
    }
    for (int p : pass_of) {
        if (p < 0) {
            ++c.missing;
        } else if (p == 0) {
            ++c.as_said;
        } else {
            ++c.wrong;
        }
    }
    for (bool t : taken) c.ghosts += t ? 0 : 1;
    return c;
}

}  // namespace detail

// The journal's air-picture lines against the radar company's log: a line heard within match_s of a report said.
inline RecordCheck CheckJournal(const std::vector<JournalLine>& journal, const std::vector<SaidReport>& log, const AfterActionValues& v) {
    std::vector<JournalLine> air;
    for (const JournalLine& l : journal) {
        if (l.net == "air picture") air.push_back(l);
    }
    return detail::MatchToLog(
        "journal", log, air.size(), [&](const SaidReport& s, std::size_t k) { return std::fabs(air[k].heard_s - s.time_s) <= v.match_s; },
        [&](const SaidReport& s, std::size_t k) { return std::fabs(air[k].heard_s - s.time_s); },
        [&](const SaidReport& s, std::size_t k) { return air[k].text == s.text; },
        [&](const SaidReport& s, std::size_t k) {
            WrittenAirReport w;
            return ReadAirReport(air[k].text, w) && w.number == s.number;
        });
}
// The board's marks against the same log: a mark drawn from a report said up to board_s before it (the plotter writes
// what he heard, or finds it on the journal's page). Under its number it is as said; where on the board is not checked.
inline RecordCheck CheckBoard(const std::vector<BoardRecord>& board, const std::vector<SaidReport>& log, const AfterActionValues& v) {
    return detail::MatchToLog(
        "board", log, board.size(),
        [&](const SaidReport& s, std::size_t k) { return board[k].time_s + 1.0 >= s.time_s && board[k].time_s <= s.time_s + v.board_s; },
        [&](const SaidReport& s, std::size_t k) { return std::fabs(board[k].time_s - s.time_s); },
        [&](const SaidReport& s, std::size_t k) { return board[k].number == s.number; },
        [&](const SaidReport& s, std::size_t k) { return board[k].number == s.number; });
}

// The crew's mark for a record: 5 to 2 by the check's agreement (0: nothing to check).
inline int RecordGrade(const RecordCheck& c, const AfterActionValues& v) {
    if (c.Checked() == 0) return 0;
    const double a = c.Agreement() + 1e-12;
    return a >= v.grade_5 ? 5 : a >= v.grade_4 ? 4 : a >= v.grade_3 ? 3 : 2;
}
inline const char* GradeWord(int grade) {
    switch (grade) {
        case 5: return "excellent";
        case 4: return "good";
        case 3: return "satisfactory";
        case 2: return "unsatisfactory";
        default: return "not graded";
    }
}

// The regiment's belief in each unit's reports: the share of its records' entries that have checked out, over every check,
// starting from a prior (a unit not yet checked is believed at prior_agreed / prior_checked).
class ReportTrust {
public:
    explicit ReportTrust(const AfterActionValues& v = AfterActionValues()) : v_(v) {}
    void Checked(const std::string& unit, const RecordCheck& c) {
        Unit& u = units_[unit];
        u.checked += c.Checked();
        u.agreed += c.as_said;
        ++u.checks;
    }
    double Of(const std::string& unit) const {
        const auto it = units_.find(unit);
        const double checked = v_.trust_prior_checked + (it != units_.end() ? it->second.checked : 0);
        const double agreed = v_.trust_prior_agreed + (it != units_.end() ? it->second.agreed : 0);
        return checked > 0.0 ? agreed / checked : 1.0;
    }
    int Checks(const std::string& unit) const {
        const auto it = units_.find(unit);
        return it != units_.end() ? it->second.checks : 0;
    }

private:
    struct Unit {
        int checked = 0;
        int agreed = 0;
        int checks = 0;
    };
    AfterActionValues v_;
    std::map<std::string, Unit> units_;
};

}  // namespace campaign
}  // namespace simcore
