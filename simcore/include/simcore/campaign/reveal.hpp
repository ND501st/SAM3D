// SAM3D simcore: the reveal (track O, O5; docs/campaign/12_ORDERS_MAPS_AND_THE_CAMPAIGN.md section 7). At the war's end,
// or asked for (mission_demo --reveal, the campaign's pause menu in Unreal), what our side believed beside the truth: the
// battery's records and the regiment's marks on them on one side; the outcome ledgers (outcomes.hpp) and the enemy's own
// journal (as tools/air_war_view.py shows it) on the other. This is the only place in the game where the truth is shown;
// nothing in the simulation reads it.
#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "simcore/campaign/grading.hpp"
#include "simcore/campaign/mission_session.hpp"

namespace simcore {
namespace campaign {

// One matter put both ways: what the records said, what was so.
struct RevealPair {
    std::string topic;
    std::string believed;
    std::string truth;
};
// A line of the enemy's journal: what its staff thought (hidden from our side to the end) or what our side learned of it.
struct RevealJournalLine {
    double h = 0.0;
    std::string who;
    std::string text;
    bool known = false;  // our side knew it at the time
};
struct Reveal {
    double campaign_h = -1.0;
    std::vector<RevealPair> pairs;
    std::vector<std::string> marks;          // the regiment's summings up, as said (the record's side)
    std::vector<std::string> crews;          // the battery commander's marks for his crews, as written
    std::vector<RevealJournalLine> journal;  // the enemy's journal, the latest lines
    std::string ledgers;                     // the two outcomes from the truth (OutcomeLedger::Text)
    int journal_hidden = 0;                  // the journal's lines our side never saw, in all
    int journal_known = 0;

    std::string Text() const {
        std::string out = "THE REVEAL: what our side believed, and what was so\n";
        for (const RevealPair& p : pairs) out += "  " + p.topic + "\n    believed: " + p.believed + "\n    truth:    " + p.truth + "\n";
        out += "The regiment's marks for the battery (from its records):\n";
        if (marks.empty()) out += "  none summed up\n";
        for (const std::string& m : marks) out += "  " + m + "\n";
        out += "The battery commander's marks for his crews (from his log):\n";
        if (crews.empty()) out += "  none written\n";
        for (const std::string& c : crews) out += "  " + c + "\n";
        out += "The enemy's journal (" + std::to_string(journal_hidden) + " lines our side never saw, " + std::to_string(journal_known) +
               " it learned; the latest):\n";
        char h[32];
        for (const RevealJournalLine& j : journal) {
            std::snprintf(h, sizeof(h), "%6.1f h", j.h);
            out += std::string("  ") + h + (j.known ? "  [we learned] " : "  [their mind] ") + j.who + ": " + j.text + "\n";
        }
        return out + ledgers;
    }
};

// The reveal of a session as it stands: `journal_lines` of the enemy's latest journal lines.
inline Reveal MakeReveal(const MissionSession& s, std::size_t journal_lines = 40) {
    Reveal v;
    const ScenarioRunner& r = s.Runner();
    const airwar::AirCampaign* war = s.Campaign();
    v.campaign_h = war != nullptr ? war->Now() : s.CampaignHourAtStart() + r.Time() / 3600.0;
    // The records' side: the combat report from the post's records since the session's start, the claims as checked.
    DayRecord rec;
    const bool records = r.DayRecordSince(0.0, rec, s.Standing().unit);
    const OutcomeLedger& led = s.Outcomes();
    int downed = 0, downed_others = 0, aircraft = 0, packages = 0, arms = 0;
    for (const PackageLedger& p : led.Packages()) {
        downed += p.downed_by_battery;
        downed_others += p.downed_by_others + p.downed_by_fighters;
        aircraft += p.aircraft + p.missiles;
        arms += p.arms_fired;
        ++packages;
    }
    const auto n = [](int x) { return std::to_string(x); };
    if (records) {
        const CombatReport& rep = rec.report;
        v.pairs.push_back({"Kills",
                           n(rep.destroyed) + " destroyed in the battery's log (" + n(rep.missiles) + " missiles), " + n(rec.claims.Confirmed()) +
                               " of them borne out by the radar company's log, " + n(rec.claims.Contradicted()) + " contradicted",
                           n(downed) + " shot down by the battery in the packages played back (" + n(downed_others) + " by our others)"});
        v.pairs.push_back({"Raids", n(static_cast<int>(rep.raids.size())) + " raids written, " + n(rep.Aircraft()) + " aircraft",
                           n(packages) + " packages came at the battery, " + n(aircraft) + " aircraft and missiles, " + n(arms) +
                               " anti-radiation missiles fired"});
    } else {
        v.pairs.push_back({"Kills", "no post's records", n(downed) + " shot down by the battery"});
    }
    const RegimentStanding& st = s.Standing();
    v.pairs.push_back({"Trust", "the regiment's trust in the battery's reports " + std::to_string(static_cast<int>(st.Trust(r.AfterAction()) * 100.0 + 0.5)) +
                                    " % (" + n(st.agreed) + " of " + n(st.checked) + " entries checked agreed)",
                       "what the records got right is above; the radar company's log is a record too"});
    for (const DayMark& d : st.days) {
        char h[32];
        std::snprintf(h, sizeof(h), "%.1f h", d.campaign_h);
        std::string line = std::string(h) + ": " + GradeWord(d.grade) + " (" + n(d.grade) + ")";
        for (const auto& o : d.objectives) line += ", " + o.first + " " + n(o.second);
        v.marks.push_back(line);
    }
    for (const CrewGrade& c : s.CrewGradesAll()) v.crews.push_back(CrewGradeText(c));
    // The truth's side: the enemy's journal and the ledgers.
    if (war != nullptr) {
        const std::vector<airwar::JournalEntry>& j = war->Journal();
        for (const airwar::JournalEntry& e : j) (e.visible ? v.journal_known : v.journal_hidden) += 1;
        const std::size_t from = j.size() > journal_lines ? j.size() - journal_lines : 0;
        for (std::size_t i = from; i < j.size(); ++i) v.journal.push_back({j[i].h, j[i].who, j[i].text, j[i].visible});
    }
    const SuppressionCheck now = war != nullptr ? CheckSuppression(*war, war->Now()) : SuppressionCheck();
    v.ledgers = led.Text(war != nullptr ? &now : nullptr);
    return v;
}

}  // namespace campaign
}  // namespace simcore
