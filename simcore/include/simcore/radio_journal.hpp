// simcore/radio_journal.hpp — the radiotelephonist's journal: what came over the post's radios, written by hand.
//
// pvo_bu_x_1984 art. 24: the radiotelephonist takes and writes into the journal every radiogram, signal, report and
// designation with the time he took it, and reports them to the commander; art. 37: the air-situation reports are
// written where nothing records them; pvo_bu_ix_1984 App. 7: the journal's columns (time, content, who passed it).
// rrs_1986 art. 153: no notes on loose sheets; under load (art. 154) the numbers and times only.
//
// A message he heard waits in his head until he writes it; one not written within `remember_s` of hearing it is gone.
// An NPC writes the oldest one in `write_s` (his speed), now and then a digit wrong (his tier's slip). A player takes
// it down as dictation (a draft): the message's words stand printed on the line, its digit groups are boxes he fills
// from memory, and what he writes is what the page says, right or wrong. The page is the only record: the plotter or
// he himself finds a missed message only by reading it (the cabin's document "radio_journal"), never on a screen.
#pragma once

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace simcore {

struct RadioJournalEntry {
    double heard_s = 0.0;    // when he heard it (the journal's time column)
    double written_s = 0.0;  // when the writing is finished: legible from then
    std::string net;         // the radio log's word for the net ("air picture", "regiment net", ...)
    std::string from;
    std::string said;        // as heard
    std::string text;        // as written
    int said_number = 0;     // an air report's board number as said (0: not an air report)
    int number = 0;          // as written
    int digits = 0;          // a player's dictation: the digits of the message, and how many he wrote right
    int digits_right = 0;
    bool by_hand = false;    // a player's dictation
    bool brief = false;      // the short form: the number and the time only (rrs_1986 art. 154)
};

// A message being taken down by a player: its words cut at its digit groups, the groups he has filled so far.
struct JournalDraft {
    RadioJournalEntry message;
    std::vector<std::string> words;   // the printed words: one more than the groups (before, between, after)
    std::vector<std::string> said;    // the digit groups as said
    std::vector<std::string> typed;   // as he writes them
    std::size_t box = 0;              // the group his pencil is on

    // The line as it stands: the printed words with each box ("_" for each digit still to write; the box he is on in
    // [brackets] when `cursor`).
    std::string Line(bool cursor) const {
        std::string out = words.empty() ? std::string() : words[0];
        for (std::size_t i = 0; i < said.size(); ++i) {
            std::string b = typed[i] + std::string(said[i].size() > typed[i].size() ? said[i].size() - typed[i].size() : 0, '_');
            out += cursor && i == box ? "[" + b + "]" : b;
            out += words[i + 1];
        }
        return out;
    }
};

class RadioJournal {
public:
    // A message the keeper heard: it waits to be written.
    void Heard(double t, const std::string& net, const std::string& from, const std::string& text, int number = 0) {
        RadioJournalEntry e;
        e.heard_s = t;
        e.net = net;
        e.from = from;
        e.said = text;
        e.text = text;
        e.said_number = number;
        e.number = number;
        waiting_.push_back(e);
    }
    // Messages heard longer ago than `remember_s` and still not written are gone (not one he is writing).
    void Forget(double t, double remember_s) {
        std::vector<RadioJournalEntry> kept;
        for (const RadioJournalEntry& e : waiting_) {
            if (t - e.heard_s <= remember_s + 1e-9) {
                kept.push_back(e);
            } else {
                ++forgotten_;
            }
        }
        waiting_.swap(kept);
    }
    const std::vector<RadioJournalEntry>& Waiting() const { return waiting_; }
    bool Busy(double t) const { return t + 1e-9 < free_s_ || drafting_; }

    // An NPC writes the oldest message waiting; it is on the page `write_s` from now. `slip` >= 0: one digit written
    // wrong (which, and how, from it). False while still writing or with nothing to write.
    bool WriteNext(double t, double write_s, int slip = -1) {
        if (Busy(t) || waiting_.empty()) return false;
        RadioJournalEntry e = waiting_.front();
        waiting_.erase(waiting_.begin());
        if (slip >= 0) e.text = Slipped(e.text, slip);
        e.number = e.said_number > 0 ? LeadingNumber(e.text) : 0;
        e.written_s = t + write_s;
        free_s_ = e.written_s;
        written_.push_back(e);
        return true;
    }

    // ---- a player's dictation
    // His pencil on the page: the oldest message waiting becomes the line he writes (or the one he left half-written).
    // False with nothing to write.
    bool BeginDraft(double t) {
        if (drafting_) return true;
        if (t + 1e-9 < free_s_ || waiting_.empty()) return false;
        draft_ = JournalDraft();
        draft_.message = waiting_.front();
        waiting_.erase(waiting_.begin());
        Cut(draft_.message.said, draft_.words, draft_.said);
        draft_.typed.assign(draft_.said.size(), std::string());
        drafting_ = true;
        return true;
    }
    const JournalDraft* Draft() const { return drafting_ ? &draft_ : nullptr; }
    // A digit into the box: a full box moves the pencil to the next.
    void Type(char digit) {
        if (!drafting_ || digit < '0' || digit > '9' || draft_.said.empty()) return;
        std::string& b = draft_.typed[draft_.box];
        if (b.size() < draft_.said[draft_.box].size()) b += digit;
        if (b.size() == draft_.said[draft_.box].size() && draft_.box + 1 < draft_.said.size()) ++draft_.box;
    }
    // The last digit out; in an empty box, back to the one before.
    void Rub() {
        if (!drafting_ || draft_.said.empty()) return;
        if (draft_.typed[draft_.box].empty() && draft_.box > 0) --draft_.box;
        std::string& b = draft_.typed[draft_.box];
        if (!b.empty()) b.pop_back();
    }
    void NextBox(int step) {
        if (!drafting_ || draft_.said.empty()) return;
        const int n = static_cast<int>(draft_.said.size());
        draft_.box = static_cast<std::size_t>(((static_cast<int>(draft_.box) + step) % n + n) % n);
    }
    // The line done: written as it stands (an empty box stays a gap of dashes); `brief`: the number and the time only.
    bool Finish(double t, bool brief) {
        if (!drafting_) return false;
        RadioJournalEntry e = draft_.message;
        e.by_hand = true;
        e.brief = brief;
        if (brief) {
            e.text = draft_.said.empty() ? draft_.message.said : Filled(draft_.typed[0], draft_.said[0].size()) + " — кратко";
        } else {
            e.text = draft_.words.empty() ? draft_.message.said : draft_.words[0];
            for (std::size_t i = 0; i < draft_.said.size(); ++i) e.text += Filled(draft_.typed[i], draft_.said[i].size()) + draft_.words[i + 1];
        }
        const std::size_t boxes = brief ? (draft_.said.empty() ? 0 : 1) : draft_.said.size();
        for (std::size_t i = 0; i < boxes; ++i) {
            e.digits += static_cast<int>(draft_.said[i].size());
            for (std::size_t k = 0; k < draft_.said[i].size(); ++k) {
                e.digits_right += k < draft_.typed[i].size() && draft_.typed[i][k] == draft_.said[i][k] ? 1 : 0;
            }
        }
        e.number = e.said_number > 0 ? LeadingNumber(e.text) : 0;
        e.written_s = t;
        written_.push_back(e);
        drafting_ = false;
        return true;
    }

    // Every entry written, the newest last; Legible() only those finished by `t`.
    const std::vector<RadioJournalEntry>& Written() const { return written_; }
    std::vector<RadioJournalEntry> Legible(double t) const {
        std::vector<RadioJournalEntry> out;
        for (const RadioJournalEntry& e : written_) {
            if (e.written_s <= t + 1e-9) out.push_back(e);
        }
        return out;
    }
    // The newest legible entry of the air report said under this board number, heard no earlier than `since_s`.
    const RadioJournalEntry* AirReport(int said_number, double since_s, double t) const {
        for (auto it = written_.rbegin(); it != written_.rend(); ++it) {
            if (it->said_number == said_number && it->written_s <= t + 1e-9 && it->heard_s + 1e-9 >= since_s) return &*it;
        }
        return nullptr;
    }
    int Forgotten() const { return forgotten_; }

    // The page as it is written, the newest `rows` entries, and the line being written last: the time heard from the
    // start (Ч+mm:ss, the radio data's own reckoning), the net, who passed it, what was written.
    std::vector<std::string> Page(double t, std::size_t rows) const {
        std::vector<std::string> out;
        out.push_back("ЖУРНАЛ РАДИОСВЯЗИ (радиотелефонист)");
        out.push_back("Время     Сеть        От кого: содержание");
        const std::vector<RadioJournalEntry> legible = Legible(t);
        const std::size_t keep = drafting_ && rows > 0 ? rows - 1 : rows;
        const std::size_t first = legible.size() > keep ? legible.size() - keep : 0;
        for (std::size_t i = first; i < legible.size(); ++i) out.push_back(Stamp(legible[i]) + legible[i].text);
        if (drafting_) out.push_back(Stamp(draft_.message) + draft_.Line(true));
        if (legible.empty() && !drafting_) out.push_back("(пусто)");
        return out;
    }

    // A message cut at its digit groups: the words before, between and after them, and the groups.
    static void Cut(const std::string& text, std::vector<std::string>& words, std::vector<std::string>& groups) {
        words.assign(1, std::string());
        groups.clear();
        for (std::size_t i = 0; i < text.size();) {
            if (text[i] >= '0' && text[i] <= '9') {
                std::size_t j = i;
                while (j < text.size() && text[j] >= '0' && text[j] <= '9') ++j;
                groups.push_back(text.substr(i, j - i));
                words.push_back(std::string());
                i = j;
            } else {
                words.back() += text[i++];
            }
        }
    }

private:
    static std::string Filled(const std::string& typed, std::size_t width) {
        return typed + std::string(width > typed.size() ? width - typed.size() : 0, '-');
    }
    static int LeadingNumber(const std::string& text) {
        return !text.empty() && text[0] >= '0' && text[0] <= '9' ? std::atoi(text.c_str()) : 0;
    }
    // One digit of the message written wrong: the `slip`-th digit (mod their count), as the next-but-`slip` digit.
    static std::string Slipped(const std::string& text, int slip) {
        std::vector<std::size_t> at;
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] >= '0' && text[i] <= '9') at.push_back(i);
        }
        if (at.empty()) return text;
        std::string out = text;
        const std::size_t k = at[static_cast<std::size_t>(slip) % at.size()];
        out[k] = static_cast<char>('0' + (out[k] - '0' + 1 + slip % 9) % 10);
        return out;
    }
    static std::string Stamp(const RadioJournalEntry& e) {
        const int s = e.heard_s > 0.0 ? static_cast<int>(e.heard_s) : 0;
        char stamp[32];
        std::snprintf(stamp, sizeof(stamp), "Ч+%02d:%02d  ", s / 60, s % 60);
        return std::string(stamp) + NetWord(e.net) + "  " + e.from + ": ";
    }
    static std::string NetWord(const std::string& net) {
        if (net == "air picture") return "ВОЗД.обст.";
        if (net == "regiment net") return "КП полка  ";
        if (net == "battery net") return "батарейная";
        if (net == "covered net") return "прикрыв.  ";
        return net;
    }

    std::vector<RadioJournalEntry> waiting_;
    std::vector<RadioJournalEntry> written_;
    JournalDraft draft_;
    bool drafting_ = false;
    double free_s_ = 0.0;
    int forgotten_ = 0;
};

}  // namespace simcore
