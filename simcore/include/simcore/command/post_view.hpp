// simcore/command/post_view.hpp — what one player's character knows, as the server sends it to his machine (roadmap P6,
// docs/command/06_INTEGRATION.md section 5).
//
// In a game with several players the server runs the one pipeline, and each player's character is a post of it
// (InfoNet::SetPlayers): none of them passes anything on by himself. A client never holds the net; it holds its post's
// view, the deliveries that reached him with the words as he has them (the short form until the paper comes), who told
// him and how, and the people he can tell. The server sends each client what is new since the last view it sent
// (PostViewJson with since_h); the client merges it (MergePostView). What the client's player tells someone comes back
// as a request (TellRequestJson) that the server applies only from that client's own post (ApplyTellRequest).
#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "simcore/air_war/raid_scenario.hpp"
#include "simcore/command/info_net.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace command {

// One thing the post's holder knows.
struct ViewItem {
    std::string key;     // the fact's key (one item per fact; a later view replaces it)
    std::string kind;    // FactKindName
    double h = 0.0;      // when he learned it
    double full_h = -1.0;  // when the full text reached him (-1 not yet, or with the first word)
    bool full = false;   // he has the full text
    std::string from;    // who told him (a post's id; "" he saw it, "news" the state radio)
    std::string means;   // how (a means' id; "" he saw it)
    std::string said;    // "D2 19:20, Major ..., on the field telephone"
    std::string text;    // the words as he has them: the full text, or the summary until it comes
    bool own = false;    // his own report or order (P2)
};

struct ViewContact {
    std::string post;
    std::string label;
};

struct PostView {
    std::string post;
    double now_h = 0.0;
    std::vector<ViewItem> items;  // by the hour he learned them
    std::vector<ViewContact> contacts;
};

// The view of `post` as the server sends it: what reached him after since_h (or became full after it).
inline std::string PostViewJson(const InfoNet& net, int post, double since_h = -1e18) {
    using namespace airwar::jsonb;
    const ChainData& data = net.Data();
    json::Value root = Obj();
    Put(root, "post", Str(post >= 0 && static_cast<std::size_t>(post) < data.positions.size() ? data.positions[static_cast<std::size_t>(post)].id : ""));
    Put(root, "now_h", Num(net.Now()));
    json::Value items = Arr();
    for (const Delivery& d : net.KnownBy(post)) {
        const bool full_now = d.full && d.full_h <= net.Now() + 1e-9;
        if (d.h <= since_h && !(full_now && d.full_h > since_h)) continue;
        const Fact& f = net.Facts()[static_cast<std::size_t>(d.fact)];
        json::Value it = Obj();
        Put(it, "key", Str(f.key));
        Put(it, "kind", Str(FactKindName(f.kind)));
        Put(it, "h", Num(d.h));
        Put(it, "full", Bool(full_now));
        if (full_now && d.full_h > d.h) Put(it, "full_h", Num(d.full_h));
        Put(it, "from", Str(d.from >= 0 ? data.positions[static_cast<std::size_t>(d.from)].id : d.from == -2 ? "news" : ""));
        Put(it, "means", Str(d.mean >= 0 ? data.means[static_cast<std::size_t>(d.mean)].id : ""));
        Put(it, "said", Str(net.Describe(d)));
        Put(it, "text", Str(full_now || f.summary.empty() ? f.text : f.summary));
        if (f.held && f.origin == data.positions[static_cast<std::size_t>(post)].id) Put(it, "own", Bool(true));
        Push(items, it);
    }
    Put(root, "items", items);
    json::Value contacts = Arr();
    for (int c : net.Contacts(post)) {
        json::Value j = Obj();
        Put(j, "post", Str(data.positions[static_cast<std::size_t>(c)].id));
        Put(j, "label", Str(data.positions[static_cast<std::size_t>(c)].Label()));
        Push(contacts, j);
    }
    Put(root, "contacts", contacts);
    return Dump(root, 0, 17);
}

inline bool ReadPostView(const std::string& text, PostView& out, std::string* error = nullptr) {
    json::Value root;
    if (!json::Parse(text, root, error) || !root.Is(json::Type::Object)) return false;
    auto str = [](const json::Value& o, const char* k) {
        const json::Value* v = o.Find(k);
        return v != nullptr && v->Is(json::Type::String) ? v->string : std::string();
    };
    auto num = [](const json::Value& o, const char* k, double dflt) {
        const json::Value* v = o.Find(k);
        return v != nullptr && v->Is(json::Type::Number) ? v->number : dflt;
    };
    auto flag = [](const json::Value& o, const char* k) {
        const json::Value* v = o.Find(k);
        return v != nullptr && v->boolean;
    };
    out = PostView();
    out.post = str(root, "post");
    out.now_h = num(root, "now_h", 0.0);
    if (const json::Value* items = root.Find("items")) {
        for (const json::Value& j : items->items) {
            ViewItem it;
            it.key = str(j, "key");
            it.kind = str(j, "kind");
            it.h = num(j, "h", 0.0);
            it.full_h = num(j, "full_h", -1.0);
            it.full = flag(j, "full");
            it.from = str(j, "from");
            it.means = str(j, "means");
            it.said = str(j, "said");
            it.text = str(j, "text");
            it.own = flag(j, "own");
            out.items.push_back(it);
        }
    }
    if (const json::Value* contacts = root.Find("contacts")) {
        for (const json::Value& j : contacts->items) out.contacts.push_back({str(j, "post"), str(j, "label")});
    }
    return true;
}

// A newer view onto the one the client holds: each fact's item replaced by its newer form, new ones added.
inline void MergePostView(PostView& into, const PostView& update) {
    if (into.post.empty()) into.post = update.post;
    if (update.post != into.post) return;
    into.now_h = std::max(into.now_h, update.now_h);
    for (const ViewItem& it : update.items) {
        auto same = std::find_if(into.items.begin(), into.items.end(), [&](const ViewItem& x) { return x.key == it.key; });
        if (same != into.items.end()) *same = it;
        else into.items.push_back(it);
    }
    std::stable_sort(into.items.begin(), into.items.end(), [](const ViewItem& a, const ViewItem& b) { return a.h < b.h; });
    if (!update.contacts.empty()) into.contacts = update.contacts;
}

// The client's player tells someone: the request his machine sends.
inline std::string TellRequestJson(const std::string& fact_key, const std::string& from, const std::string& to, const std::string& mean) {
    using namespace airwar::jsonb;
    json::Value root = Obj();
    Put(root, "fact", Str(fact_key));
    Put(root, "from", Str(from));
    Put(root, "to", Str(to));
    Put(root, "mean", Str(mean));
    return Dump(root, 0, 17);
}

// On the server: a client's telling, at the server's hour, accepted only from that client's own post and only as the
// pipeline allows it.
inline bool ApplyTellRequest(InfoNet& net, int client_post, const std::string& text, std::string* why = nullptr) {
    json::Value root;
    std::string error;
    if (!json::Parse(text, root, &error) || !root.Is(json::Type::Object)) {
        if (why != nullptr) *why = "the request does not read";
        return false;
    }
    auto str = [&](const char* k) {
        const json::Value* v = root.Find(k);
        return v != nullptr && v->Is(json::Type::String) ? v->string : std::string();
    };
    Telling t;
    t.from = net.Data().PositionIndex(str("from"));
    t.to = net.Data().PositionIndex(str("to"));
    t.fact = net.FactIndex(str("fact"));
    t.mean = str("mean");
    t.h = net.Now();
    if (t.from != client_post || !net.IsPlayer(client_post)) {
        if (why != nullptr) *why = "you can tell only what you know yourself";
        return false;
    }
    return net.Tell(t, why);
}

}  // namespace command
}  // namespace simcore
