// simcore/command/intercept.hpp — our side's intercept service: what the posts understood of the enemy's air radio, and how it
// reaches the battery (roadmap B9, docs/air_battle/09_COMMS_AND_INTERCEPT.md sections 4 and 5).
//
// air_war/air_radio.hpp has the enemy's calls on the air and what each of our posts heard of them physically (the time, the
// bearing, the length). Hearing is not understanding: each post has linguists for some languages, with a skill tier. The
// linguist writes the call on the form after a delay by his tier, may write a digit wrong or drop a word, and sends a doubtful
// call up the chain for a check by telephone; a post with no linguist for the language logs time, bearing and length only
// (traffic), or passes the call untranslated to a post that has one. What the post wrote is its record, right or wrong; the
// truth of the call stays in the call log for the debrief.
//
// Two roads to the battery (section 5). The fast road: a call that names a time and a place goes from the linguist's form to
// the post's air defence liaison, by telephone to the division's air defence command post, and down the warning net or the
// regiment's command net to the battery: a fact (FactKind::Intercept) in the information pipeline (info_net.hpp), whose
// delivery to the battery commander the pipeline times by its means. The slow road: everything logged goes into the air
// section's day summary (FactKind::Intsum), carried by teleprinter and despatch rider to the regiment's evening briefing. A
// heard report that names the battery's square also tells the regiment's chief of staff what the enemy knows.
//
// Nothing here is shown on a screen: the battery learns of an intercept only as a message on its nets, written into the
// radiotelephonist's journal as heard, with its slips (docs/command/01_PRINCIPLES.md).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/air_war/air_radio.hpp"
#include "simcore/command/em_radio.hpp"
#include "simcore/command/info_net.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace command {

// ------------------------------------------------------------------------------------------------ the data

struct TierRule {
    double delay_s = 60.0;
    double digit_error = 0.1;
    double word_drop = 0.1;
    double doubt_chance = 0.2;
};
struct Linguist {
    std::string language;
    std::string tier;
    int count = 1;
};
struct InterceptPostData {
    std::string id;        // the post's id in the scenario's "air_radio" posts (its chain_post)
    std::string position;  // its chief in the chain of command
    std::string kind;      // "centre" or "forward"
    std::string relay_to;  // a post with no linguist for a language passes the call untranslated here
    std::string liaison;   // the air defence liaison at the post ("" none)
    std::vector<Linguist> linguists;
    const Linguist* For(const std::string& language) const {
        for (const Linguist& l : linguists) {
            if (l.language == language && l.count > 0) return &l;
        }
        return nullptr;
    }
};
struct ListenerData {
    std::string id;
    double east_km = 0.0, north_km = 0.0;
    double height_m = 10.0, gain_db = 0.0, sensitivity_dbw = -130.0;
};
struct InterceptData {
    std::map<std::string, TierRule> tiers;
    double liaison_s = 300.0;
    double doubt_check_s = 300.0;
    double relay_untranslated_s = 300.0;
    double area_m = 20000.0;
    double names_battery_m = 3000.0;
    int habit_after = 2;
    double daily_summary_hour = 18.0;
    std::vector<InterceptPostData> posts;
    std::map<std::string, std::vector<ListenerData>> listeners;  // by the enemy's faction id

    const InterceptPostData* Post(const std::string& id) const {
        for (const InterceptPostData& p : posts) {
            if (p.id == id) return &p;
        }
        return nullptr;
    }
    const TierRule& Tier(const std::string& tier) const {
        const auto it = tiers.find(tier);
        static const TierRule fallback;
        return it == tiers.end() ? fallback : it->second;
    }
};

namespace detail {
inline bool UnknownKey(const json::Value& v, const std::string& path, std::initializer_list<const char*> allowed, std::string* error) {
    for (const std::string& k : v.keys) {
        bool ok = false;
        for (const char* a : allowed) ok = ok || k == a;
        if (!ok) {
            if (error != nullptr) *error = path + ": unknown key '" + k + "'";
            return true;
        }
    }
    return false;
}
inline double Num(const json::Value& v, const char* key, double def) {
    const json::Value* f = v.Find(key);
    return f != nullptr && f->Is(json::Type::Number) ? f->number : def;
}
inline std::string Str(const json::Value& v, const char* key) {
    const json::Value* f = v.Find(key);
    return f != nullptr && f->Is(json::Type::String) ? f->string : std::string();
}
}  // namespace detail

inline bool LoadInterceptData(const std::string& data_dir, const std::string& file, InterceptData& out, std::string* error = nullptr) {
    const auto fail = [error](const std::string& w) {
        if (error != nullptr) *error = w;
        return false;
    };
    std::ifstream in(data_dir + "/" + file, std::ios::binary);
    if (!in) return fail("cannot open " + data_dir + "/" + file);
    std::stringstream ss;
    ss << in.rdbuf();
    json::Value root;
    std::string e;
    if (!json::Parse(ss.str(), root, &e)) return fail(file + ": " + e);
    if (detail::UnknownKey(root, file, {"intercept_id", "status", "source", "confidence", "notes", "tiers", "fast_road", "posts", "enemy_listeners"}, &e)) return fail(e);
    out = InterceptData();
    const json::Value* tiers = root.Find("tiers");
    if (tiers == nullptr || !tiers->Is(json::Type::Object)) return fail(file + ": tiers: expected an object");
    for (std::size_t i = 0; i < tiers->keys.size(); ++i) {
        if (tiers->keys[i] == "notes") continue;
        const json::Value& t = tiers->items[i];
        if (detail::UnknownKey(t, "tiers." + tiers->keys[i], {"delay_s", "digit_error", "word_drop", "doubt_chance"}, &e)) return fail(file + ": " + e);
        TierRule r;
        r.delay_s = detail::Num(t, "delay_s", r.delay_s);
        r.digit_error = detail::Num(t, "digit_error", r.digit_error);
        r.word_drop = detail::Num(t, "word_drop", r.word_drop);
        r.doubt_chance = detail::Num(t, "doubt_chance", r.doubt_chance);
        out.tiers[tiers->keys[i]] = r;
    }
    if (const json::Value* f = root.Find("fast_road")) {
        if (detail::UnknownKey(*f, "fast_road", {"liaison_s", "doubt_check_s", "relay_untranslated_s", "area_m", "names_battery_m", "habit_after", "daily_summary_hour", "notes"}, &e)) return fail(file + ": " + e);
        out.liaison_s = detail::Num(*f, "liaison_s", out.liaison_s);
        out.doubt_check_s = detail::Num(*f, "doubt_check_s", out.doubt_check_s);
        out.relay_untranslated_s = detail::Num(*f, "relay_untranslated_s", out.relay_untranslated_s);
        out.area_m = detail::Num(*f, "area_m", out.area_m);
        out.names_battery_m = detail::Num(*f, "names_battery_m", out.names_battery_m);
        out.habit_after = static_cast<int>(detail::Num(*f, "habit_after", out.habit_after));
        out.daily_summary_hour = detail::Num(*f, "daily_summary_hour", out.daily_summary_hour);
    }
    const json::Value* posts = root.Find("posts");
    if (posts == nullptr || !posts->Is(json::Type::Array)) return fail(file + ": posts: expected an array");
    for (const json::Value& p : posts->items) {
        if (detail::UnknownKey(p, "posts", {"id", "position", "kind", "relay_to", "liaison", "linguists", "notes"}, &e)) return fail(file + ": " + e);
        InterceptPostData d;
        d.id = detail::Str(p, "id");
        d.position = detail::Str(p, "position");
        d.kind = detail::Str(p, "kind");
        d.relay_to = detail::Str(p, "relay_to");
        d.liaison = detail::Str(p, "liaison");
        if (d.id.empty() || d.position.empty()) return fail(file + ": a post needs its id and position");
        if (const json::Value* ls = p.Find("linguists")) {
            for (const json::Value& l : ls->items) {
                if (detail::UnknownKey(l, "linguists", {"language", "tier", "count"}, &e)) return fail(file + ": " + e);
                Linguist g;
                g.language = detail::Str(l, "language");
                g.tier = detail::Str(l, "tier");
                g.count = static_cast<int>(detail::Num(l, "count", 1.0));
                if (out.tiers.find(g.tier) == out.tiers.end()) return fail(file + ": post " + d.id + ": no tier '" + g.tier + "'");
                d.linguists.push_back(g);
            }
        }
        out.posts.push_back(std::move(d));
    }
    for (const InterceptPostData& p : out.posts) {
        if (!p.relay_to.empty() && out.Post(p.relay_to) == nullptr) return fail(file + ": post " + p.id + " relays to unknown '" + p.relay_to + "'");
    }
    if (const json::Value* el = root.Find("enemy_listeners")) {
        for (std::size_t i = 0; i < el->keys.size(); ++i) {
            if (el->keys[i] == "notes") continue;
            for (const json::Value& l : el->items[i].items) {
                if (detail::UnknownKey(l, "enemy_listeners", {"id", "east_km", "north_km", "height_m", "gain_db", "sensitivity_dbw", "notes"}, &e)) return fail(file + ": " + e);
                ListenerData d;
                d.id = detail::Str(l, "id");
                d.east_km = detail::Num(l, "east_km", 0.0);
                d.north_km = detail::Num(l, "north_km", 0.0);
                d.height_m = detail::Num(l, "height_m", d.height_m);
                d.gain_db = detail::Num(l, "gain_db", d.gain_db);
                d.sensitivity_dbw = detail::Num(l, "sensitivity_dbw", d.sensitivity_dbw);
                out.listeners[el->keys[i]].push_back(d);
            }
        }
    }
    return true;
}

// ------------------------------------------------------------------------------------------------ the desk

// A call as a post wrote it up: the record, right or wrong.
struct Logged {
    int call = -1;
    std::string post;
    std::string chain_post;         // the post in intercept.json it is
    double heard_s = 0.0;
    double length_s = 0.0;
    double bearing_deg = 0.0;       // true, as measured
    bool understood = false;        // a linguist read it
    bool doubted = false;           // checked by telephone first
    bool via_relay = false;         // passed untranslated to a post with a linguist
    double written_s = 0.0;         // when the form was done (traffic: when the call ended)
    std::string text;               // as written
    std::string said;               // what was said (the debrief's)
    airwar::CallKind kind = airwar::CallKind::Admin;
    std::string callsign;           // as heard (understood calls only)
    std::string unit_label;         // "an unidentified flight", or the unit once the post has its continuity
    double continuity = 0.0;
    bool has_place = false;         // the written text names a square
    Vec3 place;
    bool names_battery = false;
    bool wrong = false;             // the written text is not what was said
    bool codeword = false;
    bool habit = false;             // a codeword the analysts know the meaning of
    bool fast = false;              // goes on the fast road
    int precedence = 1;
    std::string language;
};

inline std::string BearingText(double deg) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%03d", static_cast<int>(std::lround(deg)) % 360);
    return buf;
}

class InterceptDesk {
public:
    void Init(const InterceptData* data, std::uint64_t seed, double clock_start_s) {
        data_ = data;
        rng_ = em::Random(seed ^ 0x1D7E5C0Bull);
        clock_start_s_ = clock_start_s;
        cursor_ = 0;
        log_.clear();
        continuity_.clear();
        codewords_.clear();
        habits_.clear();
    }
    // The places the battery stands at and covers (the session's ENU): a square this near is "the battery's square".
    void Watch(const Vec3& at) { watch_.push_back(at); }
    void ClearWatch() { watch_.clear(); }
    // The analysts know what a codeword means (learned from earlier raids: army_intel's habit, docs/air_battle/09 section 4.4).
    void LearnHabit(const std::string& word) { habits_.insert(word); }
    bool HasHabit(const std::string& word) const { return habits_.count(word) != 0; }

    // Write up what our posts heard since the last call, as each post's linguists would. Returns the new entries.
    std::vector<Logged> Process(const airwar::AirRadio& radio) {
        std::vector<Logged> fresh;
        const std::vector<airwar::Heard>& heard = radio.HeardLog();
        for (; cursor_ < heard.size(); ++cursor_) {
            const airwar::Heard& h = heard[cursor_];
            if (h.call < 0 || static_cast<std::size_t>(h.call) >= radio.Calls().size()) continue;
            fresh.push_back(WriteUp(radio.Def(), h, radio.Calls()[static_cast<std::size_t>(h.call)]));
            log_.push_back(fresh.back());
        }
        return fresh;
    }
    // Write up one call heard outside a session's radio (roadmap S6: an abstract package's call, heard in closed form by
    // air_war/abstract_radio.hpp). `def` gives the tuning, the band and where the posts stand (its `posts`). Kept in the log.
    Logged WriteUpCall(const AirRadioDef& def, const airwar::Heard& h, const airwar::Call& c) {
        log_.push_back(WriteUp(def, h, c));
        return log_.back();
    }

    const std::vector<Logged>& Log() const { return log_; }
    // A post's continuity on a unit now: 1 - exp(-heard hours / tau) under an unchanged call sign; 0 after a change.
    double Continuity(const std::string& post, const std::string& unit) const {
        const auto it = continuity_.find(post + "|" + unit);
        return it == continuity_.end() ? 0.0 : Of(it->second);
    }

    // The air section's day summary for [from_s, to_s) of the session: what was heard, in numbers (section 5.2).
    std::string DailySummary(double from_s, double to_s, double name_at, const airwar::AirRadio& radio) const {
        return DailySummary(from_s, to_s, name_at, radio.Def(), radio.HeardLog());
    }
    // The same from the posts' hearing as a list (roadmap S6: the campaign's abstract calls); `def.posts` places the posts.
    std::string DailySummary(double from_s, double to_s, double name_at, const AirRadioDef& def, const std::vector<airwar::Heard>& heard) const {
        std::map<std::string, std::pair<int, double>> units;  // label -> calls, best continuity
        std::map<int, int> per_hour;
        int traffic = 0, read = 0, wrong = 0;
        std::set<std::string> codes, signs;
        std::vector<std::string> named;  // the squares the read calls named, as read from the forms, with the hour
        for (const Logged& l : log_) {
            if (l.heard_s < from_s || l.heard_s >= to_s) continue;
            ++per_hour[static_cast<int>(std::floor((clock_start_s_ + l.heard_s) / 3600.0)) % 24];
            if (!l.understood) {
                ++traffic;
                continue;
            }
            ++read;
            wrong += l.wrong ? 1 : 0;
            std::pair<int, double>& u = units[l.unit_label];
            u.first += 1;
            u.second = std::max(u.second, l.continuity);
            if (!l.callsign.empty()) signs.insert(l.callsign);
            if (l.has_place && !l.codeword && named.size() < 8) named.push_back(airwar::GridOf(l.place) + " at " + Hhmm(clock_start_s_ + l.heard_s));
            if (l.codeword) codes.insert(l.text);
        }
        char buf[200];
        std::string s = "Air section, radio intercept summary, " + airwar::ClockOf(clock_start_s_ + from_s) + " to " + airwar::ClockOf(clock_start_s_ + to_s) + ":\n";
        std::snprintf(buf, sizeof(buf), "  %d calls logged, %d read, %d on the form not read (no linguist, or secure).\n", read + traffic, read, traffic);
        s += buf;
        for (const auto& u : units) {
            std::snprintf(buf, sizeof(buf), "  %s: %d calls%s.\n", u.first.c_str(), u.second.first, u.second.second >= name_at ? " (known by its habits)" : "");
            s += buf;
        }
        if (!per_hour.empty()) {
            int peak_h = 0, peak = -1;
            for (const auto& ph : per_hour) {
                if (ph.second > peak) {
                    peak = ph.second;
                    peak_h = ph.first;
                }
            }
            std::snprintf(buf, sizeof(buf), "  Most traffic in the hour from %02d00: %d calls.\n", peak_h, peak);
            s += buf;
        }
        if (signs.size() > units.size()) s += "  More call signs than units: the signs were changed.\n";
        if (!named.empty()) {
            s += "  Squares named:";
            for (std::size_t i = 0; i < named.size(); ++i) s += (i == 0 ? " " : ", ") + named[i];
            s += ".\n";
        }
        for (const std::string& c : codes) s += "  " + c + "\n";
        const std::string fixes = DfFixes(def, heard, from_s, to_s);
        if (!fixes.empty()) s += fixes;
        if (wrong > 0) s += "  (Some forms were corrected by telephone or are doubtful.)\n";
        return s;
    }

    // Two posts' bearings on one call make a fix (the existing BearingFix idea, flat earth over the session's ground).
    static bool Intersect(const Vec3& pa, double ba, const Vec3& pb, double bb, Vec3& out) {
        const double sa = std::sin(ba), ca = std::cos(ba), sb = std::sin(bb), cb = std::cos(bb);
        const double det = sa * cb - ca * sb;  // east/north: direction (sin, cos)
        if (std::fabs(det) < 0.05) return false;
        const double dx = pb.x - pa.x, dy = pb.y - pa.y;
        const double ta = (dx * cb - dy * sb) / det;
        if (ta < 0.0) return false;
        out = Vec3{pa.x + ta * sa, pa.y + ta * ca, 0.0};
        return true;
    }

private:
    struct Cont {
        std::string callsign;
        double heard_h = 0.0;
        double last_s = -1e18;
    };
    double Tau() const { return tau_h_; }
    double Of(const Cont& c) const { return 1.0 - std::exp(-c.heard_h / Tau()); }

    double Draw(const airwar::Call& c, const std::string& post, std::uint64_t salt) const {
        return rng_.Uniform(em::Stream::AirRadio, static_cast<std::uint64_t>(c.id) + 1, airwar::RadioKeyOf(post, stable_keys_), salt, 0xD35C);
    }
    static std::string Hhmm(double s) { return airwar::ClockOf(s); }

    Logged WriteUp(const AirRadioDef& def, const airwar::Heard& h, const airwar::Call& c) {
        tau_h_ = def.Tune("radio.continuity_tau_h");
        stable_keys_ = airwar::StableRadioKeys(def);
        Logged l;
        l.call = c.id;
        l.post = h.post;
        l.chain_post = h.chain_post.empty() ? h.post : h.chain_post;
        l.heard_s = h.start_s;
        l.length_s = h.end_s - h.start_s;
        l.bearing_deg = RadToDegrees(h.bearing_rad);
        l.kind = c.kind;
        l.said = c.words;
        l.language = c.language;
        l.written_s = h.end_s;
        l.unit_label = "an unidentified flight";
        const InterceptPostData* pd = data_ != nullptr ? data_->Post(h.chain_post.empty() ? h.post : h.chain_post) : nullptr;
        // Can it be followed? A secure net not at all; a hopping one by the content chance.
        bool content = !c.secure;
        if (content && c.have_quick) content = Draw(c, h.post, 1) < def.Tune("radio.have_quick_content");
        const Linguist* ling = nullptr;
        bool relayed = false;
        const InterceptPostData* reader = pd;
        if (pd != nullptr) {
            ling = pd->For(c.language);
            if (ling == nullptr && !pd->relay_to.empty()) {
                reader = data_->Post(pd->relay_to);
                ling = reader != nullptr ? reader->For(c.language) : nullptr;
                relayed = ling != nullptr;
            }
        }
        char buf[160];
        if (!content || ling == nullptr) {
            std::snprintf(buf, sizeof(buf), "traffic on %.3f MHz at %s, bearing %s, %d s, not read", def.freq_hz / 1e6, Hhmm(clock_start_s_ + h.start_s).c_str(),
                          BearingText(l.bearing_deg).c_str(), static_cast<int>(std::lround(l.length_s)));
            l.text = buf;
            return l;
        }
        const TierRule& rule = data_->Tier(ling->tier);
        l.understood = true;
        l.via_relay = relayed;
        double delay = rule.delay_s;
        if (Draw(c, h.post, 2) < rule.doubt_chance) {
            l.doubted = true;
            delay += data_->doubt_check_s;
        }
        if (relayed) delay += data_->relay_untranslated_s;
        l.written_s = h.end_s + delay;
        l.text = WithSlips(c, h.post, c.words, rule, l.wrong);
        // The call sign as heard, and the post's continuity on its unit.
        l.callsign = c.callsign;
        Cont& ct = continuity_[h.post + "|" + c.unit];
        if (ct.callsign != c.callsign) {
            ct.callsign = c.callsign;
            ct.heard_h = 0.0;
        } else if (ct.last_s > -1e17) {
            ct.heard_h += std::min(h.start_s - ct.last_s, def.Tune("radio.continuity_gap_cap_s")) / 3600.0;
        }
        ct.last_s = h.start_s;
        l.continuity = Of(ct);
        if (l.continuity >= def.Tune("radio.continuity_name_at")) l.unit_label = c.unit + " (" + c.callsign + ")";
        // A codeword is traffic until the analysts know the habit.
        if (c.kind == airwar::CallKind::Codeword) {
            l.codeword = true;
            const int n = ++codewords_[c.words];
            if (n >= data_->habit_after) habits_.insert(c.words);
            l.habit = habits_.count(c.words) != 0;
            l.text = l.habit ? "codeword " + c.words + ": strike complete" : "codeword " + c.words + " (meaning not known)";
            return l;
        }
        // A place it names, read against where the post stands.
        const AirRadioPostDef* at = nullptr;
        for (const AirRadioPostDef& p : def.posts) {
            if (p.id == h.post) at = &p;
        }
        if (airwar::FindGrid(l.text, at != nullptr ? at->at : Vec3(), l.place)) {
            l.has_place = true;
            for (const Vec3& w : watch_) l.names_battery = l.names_battery || std::hypot(l.place.x - w.x, l.place.y - w.y) <= data_->names_battery_m;
        }
        l.fast = airwar::CallNamesTarget(c.kind) && l.has_place;
        bool close = false;
        for (const Vec3& w : watch_) close = close || (l.has_place && std::hypot(l.place.x - w.x, l.place.y - w.y) <= data_->area_m);
        l.precedence = close ? 2 : 1;
        return l;
    }

    // What the linguist wrote: a digit wrong and a word dropped by the tier's chances (the Torgau telegraphist's weak English).
    std::string WithSlips(const airwar::Call& c, const std::string& post, std::string text, const TierRule& rule, bool& wrong) const {
        if (Draw(c, post, 3) < rule.digit_error) {
            std::vector<std::size_t> digits;
            for (std::size_t i = 0; i < text.size(); ++i) {
                if (text[i] >= '0' && text[i] <= '9') digits.push_back(i);
            }
            if (!digits.empty()) {
                const std::size_t at = digits[std::min(digits.size() - 1, static_cast<std::size_t>(Draw(c, post, 4) * static_cast<double>(digits.size())))];
                const int d = text[at] - '0';
                text[at] = static_cast<char>('0' + (d + 1 + static_cast<int>(Draw(c, post, 5) * 8.0)) % 10);
                wrong = true;
            }
        }
        if (Draw(c, post, 6) < rule.word_drop) {
            std::vector<std::pair<std::size_t, std::size_t>> words;  // [start, end)
            std::size_t i = 0;
            while (i < text.size()) {
                while (i < text.size() && text[i] == ' ') ++i;
                const std::size_t s = i;
                while (i < text.size() && text[i] != ' ') ++i;
                if (i > s) words.push_back({s, i});
            }
            if (words.size() > 2) {
                const std::size_t w = 1 + std::min(words.size() - 2, static_cast<std::size_t>(Draw(c, post, 7) * static_cast<double>(words.size() - 1)));
                text.erase(words[w].first, words[w].second - words[w].first + (words[w].second < text.size() ? 1 : 0));
                wrong = true;
            }
        }
        return text;
    }

    std::string DfFixes(const AirRadioDef& def, const std::vector<airwar::Heard>& heard, double from_s, double to_s) const {
        std::map<int, std::vector<const airwar::Heard*>> by_call;
        for (const airwar::Heard& h : heard) {
            if (h.start_s >= from_s && h.start_s < to_s) by_call[h.call].push_back(&h);
        }
        int fixes = 0;
        std::string first;
        for (const auto& kv : by_call) {
            if (kv.second.size() < 2) continue;
            const AirRadioPostDef* a = nullptr;
            const AirRadioPostDef* b = nullptr;
            for (const AirRadioPostDef& p : def.posts) {
                if (p.id == kv.second[0]->post) a = &p;
                if (p.id == kv.second[1]->post) b = &p;
            }
            Vec3 at;
            if (a == nullptr || b == nullptr || !Intersect(a->at, kv.second[0]->bearing_rad, b->at, kv.second[1]->bearing_rad, at)) continue;
            if (fixes++ == 0) first = airwar::GridOf(at);
        }
        if (fixes == 0) return std::string();
        char buf[120];
        std::snprintf(buf, sizeof(buf), "  %d transmitters fixed by two posts' bearings, the first near %s.\n", fixes, first.c_str());
        return buf;
    }
    static double RadToDegrees(double r) {
        double d = r * 180.0 / 3.14159265358979323846;
        d = std::fmod(d, 360.0);
        return d < 0.0 ? d + 360.0 : d;
    }

    const InterceptData* data_ = nullptr;
    em::Random rng_{1};
    double clock_start_s_ = 0.0;
    double tau_h_ = 3.6;
    bool stable_keys_ = true;  // the radio's draws keyed by em::StringKey (radio.mode 2), else std::hash (WriteUp sets it)
    std::size_t cursor_ = 0;
    std::vector<Logged> log_;
    std::vector<Vec3> watch_;
    std::map<std::string, Cont> continuity_;
    std::map<std::string, int> codewords_;
    std::set<std::string> habits_;
};

// ------------------------------------------------------------------------------------------------ the facts

// The fast road's fact for a written call (empty key: it does not go on the fast road): origin the post's liaison (else its
// chief), when the form was written plus the liaison's handling; for the regiment, division and battery below the army's
// air defence chief. A heard report naming the battery's square is the other: an EnemyKnowsBattery fact for the regiment's
// chief of staff, from the post's chief.
inline std::vector<Fact> InterceptFacts(const Logged& l, const InterceptData& data, const std::string& post_id, double clock_start_s) {
    std::vector<Fact> out;
    const InterceptPostData* pd = data.Post(post_id);
    if (pd == nullptr || !l.understood || l.codeword) return out;
    char head[80];
    std::snprintf(head, sizeof(head), "Intercepted at %s from post %s, bearing %s: ", airwar::ClockOf(clock_start_s + l.heard_s).c_str(), pd->id.c_str(),
                  BearingText(l.bearing_deg).c_str());
    if (l.fast) {
        Fact f;
        f.key = "intercept:" + std::to_string(l.call) + ":" + l.post;
        f.kind = FactKind::Intercept;
        f.origin = pd->liaison.empty() ? pd->position : pd->liaison;
        f.h = (l.written_s + data.liaison_s) / 3600.0;
        f.text = std::string(head) + l.text;
        f.summary = "radio intercept: " + l.text;
        f.subject = "call:" + std::to_string(l.call);
        f.audience = "div_ad_chief";
        f.need_level = 3;
        f.precedence = l.precedence;
        f.expires_h = f.h + 4.0;
        out.push_back(f);
    }
    if (l.names_battery && l.kind == airwar::CallKind::Report) {
        Fact f;
        f.key = "intercept_knows:" + std::to_string(l.call) + ":" + l.post;
        f.kind = FactKind::EnemyKnowsBattery;
        f.origin = pd->position;
        f.h = l.written_s / 3600.0;
        f.text = std::string(head) + l.text + " (the enemy has reported the battery's square)";
        f.summary = "the enemy has reported the battery's square";
        f.subject = "call:" + std::to_string(l.call);
        f.audience = "rgt_chief_of_staff";
        f.need_level = 2;
        f.precedence = 1;
        out.push_back(f);
    }
    return out;
}

// The day summary as a fact: a document from army intelligence, routine, down to the battery commanders.
inline Fact SummaryFact(const std::string& text, int day, double to_s) {
    Fact f;
    f.key = "intercept_intsum:" + std::to_string(day);
    f.kind = FactKind::Intsum;
    f.origin = "army_intel";
    f.h = to_s / 3600.0;
    f.text = text;
    f.summary = "the air section's radio intercept summary";
    f.subject = "intercept";
    f.need_level = 3;
    f.precedence = 0;
    f.document = true;
    return f;
}

// The hours warning of a committed package (section 5.3): a fact at the intercept centre with the lead drawn from the staff's
// opsec, partial and sometimes wrong. `raid_h`: the hour the raid is over its target; `where`: the district it names.
inline std::vector<Fact> OpsecAlertFacts(const airwar::OpsecAlert& a, int package_id, double raid_h, const std::string& where,
                                         const std::string& origin = "intercept_centre") {
    std::vector<Fact> out;
    if (!a.alerted) return out;
    Fact f;
    f.key = "intercept_alert:" + std::to_string(package_id);
    f.kind = FactKind::Intercept;
    f.origin = origin;
    f.h = raid_h - a.lead_h;
    const double centre = raid_h + (a.wrong ? a.wrong_shift_h : 0.0);
    char buf[200];
    std::snprintf(buf, sizeof(buf), "Administrative traffic: a raid towards %s expected between %.0f and %.0f hours from now.", where.c_str(),
                  std::max(0.0, centre - raid_h + a.lead_h - 1.0), centre - raid_h + a.lead_h + 1.0);
    f.text = buf;
    f.summary = "radio intercept: a raid towards " + where + " expected";
    f.subject = "package:" + std::to_string(package_id);
    f.audience = "div_ad_chief";
    f.need_level = 3;
    f.precedence = 1;
    f.expires_h = raid_h + 1.0;
    out.push_back(f);
    return out;
}

// ------------------------------------------------------------------------------------------------ to the battery

// The pipeline for a session: the chain of command with our facts added as the posts write them, and when each reaches the
// battery's commander. The deliveries are then said on the battery's nets (ScenarioRunner::RegimentSays), where the
// radiotelephonist writes them in his journal.
class InterceptBridge {
public:
    struct Arrival {
        std::string key;
        double at_s = 0.0;       // session seconds
        std::string text;        // what the regiment tells the battery
        bool fast = false;
        int precedence = 1;
        std::string mean;        // the means that brings it (the pipeline's: "courier", "radio_voice", ...)
    };
    bool Init(const std::string& data_dir, const std::string& chain_file, unsigned seed, double clock_start_s, std::string* error = nullptr) {
        ChainData chain;
        if (!LoadChainData(data_dir, chain_file, chain, error)) return false;
        clock_start_s_ = clock_start_s;
        if (!net_.Init(chain, seed, clock_start_s / 3600.0, error)) return false;
        battery_ = net_.PlayerAt(0.0);
        if (battery_ < 0) {
            if (error != nullptr) *error = "the chain has no player's post";
            return false;
        }
        return true;
    }
    InfoNet& Net() { return net_; }
    const InfoNet& Net() const { return net_; }
    int BatteryPost() const { return battery_; }

    // Put the facts of fresh entries into the pipeline; returns those the battery learns of, with when.
    std::vector<Arrival> Feed(const std::vector<Logged>& fresh, const InterceptData& data, const airwar::AirRadio& radio) {
        std::vector<Arrival> out;
        for (const Logged& l : fresh) {
            const airwar::Call& c = radio.Calls()[static_cast<std::size_t>(l.call)];
            (void)c;
            for (const Fact& f : InterceptFacts(l, data, l.chain_post, clock_start_s_)) {
                const int fi = net_.AddFact(f);
                if (f.kind != FactKind::Intercept || fi < 0) continue;
                const double h = net_.WhenKnown(fi, battery_, f.h + 12.0);
                if (h < 0.0) continue;
                Arrival a;
                a.key = f.key;
                a.at_s = h * 3600.0;
                a.text = "Regiment to the battery: " + f.Told() + ".";
                a.fast = true;
                a.precedence = f.precedence;
                out.push_back(a);
            }
        }
        return out;
    }
    // A fact of the campaign's (an alert at the centre, the day's summary): its delivery to the battery, or none.
    bool Add(const Fact& f, Arrival& out) {
        const int fi = net_.AddFact(f);
        if (fi < 0) return false;
        Delivery d;
        if (!net_.PreviewDelivery(fi, battery_, f.h + 24.0, d)) return false;
        const double h = d.h;
        out.mean = d.mean >= 0 && static_cast<std::size_t>(d.mean) < net_.Data().means.size() ? net_.Data().means[static_cast<std::size_t>(d.mean)].id : std::string();
        out.key = f.key;
        out.at_s = h * 3600.0;
        out.text = "Regiment to the battery: " + f.Told() + ".";
        out.fast = false;
        out.precedence = f.precedence;
        return true;
    }

private:
    InfoNet net_;
    int battery_ = -1;
    double clock_start_s_ = 0.0;
};

// The enemy's listeners where the data puts them (section 6): our nets are heard by link margin, and with some placed and
// none in reach the enemy hears nothing. Positions are the theatre's ENU (the chain's places' frame).
inline int PlaceEnemyListeners(EmRadio& radio, const InterceptData& data, const std::string& faction) {
    const auto it = data.listeners.find(faction);
    if (it == data.listeners.end()) return 0;
    int n = 0;
    for (const ListenerData& d : it->second) {
        RadioListener l;
        l.id = d.id;
        l.pos = Vec3{d.east_km * 1000.0, d.north_km * 1000.0, 0.0};
        l.height_m = d.height_m;
        l.gain_db = d.gain_db;
        l.sensitivity_dbw = d.sensitivity_dbw;
        radio.AddListener(l);
        ++n;
    }
    return n;
}

}  // namespace command
}  // namespace simcore
