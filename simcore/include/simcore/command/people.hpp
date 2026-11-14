// simcore/command/people.hpp — the visible chain of command: people and papers (roadmap G8;
// docs/campaign/11_BATTERY_LIFE_PROPOSAL.md §5.9).
//
// One registry of people: every post's holder in the chain (data/command/chain_pannonia.json), down to the vehicle
// commanders, and each man who takes a post over later (a replacement, a promotion: the info net's post changes, P8), so
// the vehicle commander who is a post of the chain is the same man in 21-2's seat. Each person is at his post's place.
//
// Papers the character holds, dated and written from what reached him, never from the truth:
// - the staff list: who holds each post of his unit, as it stood when he was given it, corrected only by the news that
//   reaches him through the pipeline (a vehicle commander replaced after a raid is a new name on it once he is told);
// - the communications diagram: the people he deals with, each with the means they have in common (the wire, the net,
//   a man sent in a vehicle), from which he picks one to call or send (P2's `InfoNet::Tell`).
// Never a chart of the truth: who is alive, where everyone is, what every post knows.
#pragma once

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/command/info_net.hpp"

namespace simcore {
namespace command {

struct Person {
    std::string id;    // "<post>" for the first holder, "<post>@<hour>" for each who took it over later
    std::string post;
    std::string name;
    std::string rank;
    std::string skill;
    double from_h = -1e18;  // holds the post from (the start: -1e18)
    double until_h = 1e18;  // until (killed, wounded, replaced, promoted)
    std::string why_left;
};

class PeopleRegistry {
public:
    // Every holder of every post, the first and the later ones (the net's post changes).
    static PeopleRegistry FromNet(const InfoNet& net) {
        PeopleRegistry r;
        const ChainData& d = net.Data();
        for (const Position& p : d.positions) {
            if (p.holder.name.empty()) continue;
            r.people_.push_back({p.id, p.id, p.holder.name, p.holder.rank, p.holder.skill, -1e18, 1e18, ""});
        }
        for (const PostChange& c : net.Changes()) {
            if (!c.vacant && c.holder.name.empty()) continue;  // the post only moves
            for (Person& q : r.people_) {
                if (q.post == c.position && q.until_h >= 1e17 && q.from_h <= c.h) {
                    q.until_h = c.h;
                    q.why_left = c.why;
                }
            }
            if (!c.vacant) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "@%.2f", c.h);
                r.people_.push_back({c.position + buf, c.position, c.holder.name, c.holder.rank, c.holder.skill, c.h, 1e18, ""});
            }
        }
        return r;
    }
    const std::vector<Person>& All() const { return people_; }
    // Who holds the post at hour h (null: nobody, it is vacant).
    const Person* HolderOf(const std::string& post, double h) const {
        for (const Person& p : people_) {
            if (p.post == post && p.from_h <= h && h < p.until_h) return &p;
        }
        return nullptr;
    }
    // The people at a place at hour h: their posts' place then (a post working from its alternate place included).
    std::vector<const Person*> At(const InfoNet& net, const std::string& place, double h) const {
        std::vector<const Person*> out;
        const int pi = net.Data().PlaceIndex(place);
        for (const Person& p : people_) {
            if (p.from_h > h || h >= p.until_h) continue;
            if (net.PlaceOf(net.Data().PositionIndex(p.post), h) == pi) out.push_back(&p);
        }
        return out;
    }

private:
    std::vector<Person> people_;
};

// ---------------------------------------------------------------------------------------------- the staff list

struct StaffLine {
    std::string post;
    std::string title;
    std::string name;  // "" : vacant, as he knows it
    std::string rank;
    double as_of_h = 0.0;  // the line written then
};

struct StaffList {
    std::string unit;
    std::string holder;  // the post that holds the paper
    double issued_h = 0.0;
    std::vector<StaffLine> lines;

    const StaffLine* Line(const std::string& post) const {
        for (const StaffLine& l : lines) {
            if (l.post == post) return &l;
        }
        return nullptr;
    }
    std::vector<std::string> Text(double start_hour) const {
        std::vector<std::string> out;
        out.push_back("STAFF LIST, " + unit + " (as of " + FormatClock(issued_h, start_hour) + ")");
        for (const StaffLine& l : lines) {
            out.push_back("  " + l.title + ": " + (l.name.empty() ? std::string("vacant") : l.rank + " " + l.name) +
                          (l.as_of_h > issued_h ? "  (corrected " + FormatClock(l.as_of_h, start_hour) + ")" : ""));
        }
        return out;
    }
};

// The unit's staff list as given to `holder` at hour h (the posts of the unit and who held them then).
inline StaffList IssueStaffList(const InfoNet& net, const std::string& unit, const std::string& holder, double h) {
    StaffList s;
    s.unit = unit;
    s.holder = holder;
    s.issued_h = h;
    const ChainData& d = net.Data();
    for (std::size_t i = 0; i < d.positions.size(); ++i) {
        const Position& p = d.positions[i];
        if (p.unit != unit) continue;
        const Holder who = net.HolderAt(static_cast<int>(i), h);
        s.lines.push_back({p.id, p.title, who.name, who.rank, h});
    }
    return s;
}

// The news of a post changing hands, as a fact of the pipeline: from `origin` (the regiment's staff for a replacement,
// the crew for a casualty) to the unit's tree. Its key "staff:<post>:<hour>" is what a staff list is corrected by.
inline int AddStaffNews(InfoNet& net, const PostChange& c, const std::string& origin, const std::string& audience) {
    const Position* p = net.Data().FindPosition(c.position);
    if (p == nullptr) return -1;
    Fact f;
    char key[96];
    std::snprintf(key, sizeof(key), "staff:%s:%.2f", c.position.c_str(), c.h);
    f.key = key;
    f.kind = FactKind::BatteryNews;
    f.h = c.h;
    f.origin = origin;
    f.audience = audience;
    f.need_level = p->level;
    f.document = false;
    f.subject = c.position;
    f.text = p->title + ": " + (c.vacant ? std::string("vacant") : c.holder.rank + " " + c.holder.name) + (c.why.empty() ? "" : " (" + c.why + ")");
    return net.AddFact(f);
}

// The staff list corrected by what reached its holder by hour h: each post's news he has, its newest line.
inline void CorrectStaffList(StaffList& s, const InfoNet& net, double h) {
    const int holder = net.Data().PositionIndex(s.holder);
    for (std::size_t i = 0; i < net.Facts().size(); ++i) {
        const Fact& f = net.Facts()[i];
        if (f.key.compare(0, 6, "staff:") != 0) continue;
        const Delivery* got = net.Known(holder, static_cast<int>(i));
        if (got == nullptr || got->h > h) continue;
        for (StaffLine& l : s.lines) {
            if (l.post != f.subject || l.as_of_h > got->h) continue;
            const Holder who = net.HolderAt(net.Data().PositionIndex(f.subject), f.h);
            l.name = who.name;
            l.rank = who.rank;
            l.as_of_h = got->h;
        }
    }
}

// ---------------------------------------------------------------------------------------------- the communications diagram

struct DiagramLine {
    std::string post;
    std::string title;
    std::string place;
    std::vector<std::string> means;  // the means they have in common (his own and the other's lists)
};

// The people `post` deals with (his superior, his subordinates, his links) and the means he has to reach each.
inline std::vector<DiagramLine> CommsDiagram(const InfoNet& net, const std::string& post) {
    std::vector<DiagramLine> out;
    const ChainData& d = net.Data();
    const int me = d.PositionIndex(post);
    if (me < 0) return out;
    const Position& mine = d.positions[static_cast<std::size_t>(me)];
    for (int c : net.Contacts(me)) {
        const Position& p = d.positions[static_cast<std::size_t>(c)];
        DiagramLine l;
        l.post = p.id;
        l.title = p.title;
        l.place = p.place;
        for (const std::string& m : mine.means) {
            const int mi = d.MeanIndex(m);
            const bool carried = mi >= 0 && (d.means[static_cast<std::size_t>(mi)].kind == MeanKind::Messenger ||
                                             d.means[static_cast<std::size_t>(mi)].kind == MeanKind::DespatchRider ||
                                             d.means[static_cast<std::size_t>(mi)].kind == MeanKind::InPerson);
            if (carried || std::find(p.means.begin(), p.means.end(), m) != p.means.end()) l.means.push_back(m);
        }
        out.push_back(l);
    }
    return out;
}

}  // namespace command
}  // namespace simcore
