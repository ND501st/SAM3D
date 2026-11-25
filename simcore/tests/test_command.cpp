// Our side's chain of command and the information pipeline (simcore/command/, data/command/, docs/command/).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/command.hpp"
#include "simcore/nav.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::command;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

ChainData LoadChain() {
    ChainData d;
    std::string e;
    if (!LoadChainData(kData, "command/chain_pannonia.json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

double StartHour() {
    airwar::TheatreDef t;
    std::string e;
    if (!airwar::LoadTheatreFile(kData + "/air_war/theatre_pannonia.json", t, &e)) std::printf("    %s\n", e.c_str());
    return t.start_hour;
}

// Campaign hour of a local clock time on the first day.
double At(double local, double start) { return local - start + (local < start ? 24.0 : 0.0); }

Fact Warning(double h) {
    Fact f;
    f.key = "test:warning";
    f.kind = FactKind::AirWarning;
    f.h = h;
    f.origin = "sector_cp_duty";
    f.text = "'Воздух!' Raid from W2, expected over the sector in 12 minutes.";
    f.need_level = 5;
    f.precedence = 2;
    f.expires_h = h + 1.0;
    return f;
}

}  // namespace

SC_TEST("command: the Kesmark district's chain loads; the loader refuses unknown keys, dangling posts and loops") {
    const ChainData d = LoadChain();
    SC_EXPECT(d.positions.size() >= 30);
    SC_EXPECT(d.places.size() >= 25);
    SC_EXPECT(d.FindMean("despatch_rider") != nullptr && d.FindMean("warning_receiver") != nullptr);
    // Senior officers are on the map: the army's chief of air defence in the city, the division's in the countryside.
    const Place* city = d.FindPlace(d.FindPosition("army_ad_chief")->place);
    SC_EXPECT(city != nullptr && city->urban && city->settlement == "novo_varosd");
    SC_EXPECT(d.FindPlace(d.FindPosition("div_ad_chief")->place)->kind == PlaceKind::MilitaryPost);
    SC_EXPECT(d.InSubtree("veh21_1_operator", "rgt_commander") && !d.InSubtree("kesmark_police", "rgt_commander"));

    const std::string places = R"({"places_id": "p", "places": [{"id": "a", "name": "A", "kind": "town", "east_km": 0, "north_km": 0}]})";
    const std::string means = R"({"means_id": "m", "means": [{"id": "in_person", "name": "In person", "kind": "in_person"}]})";
    auto chain = [](const std::string& extra, const std::string& sup) {
        return std::string(R"({"chain_id": "c", "places": "p", "means": "m", "unit_regiment": "x", "player_default": "x", "positions": [)") +
               R"({"id": "x", "title": "X", "unit": "U", "kind": "command", "level": 2, "place": "a", "player": true, "superior": ")" + sup +
               R"(", "holder": {"name": "N", "rank": "R"}, "means": ["in_person"])" + extra + R"(},)" +
               R"({"id": "y", "title": "Y", "unit": "U", "kind": "command", "level": 3, "place": "a", "superior": "x", "holder": {"name": "N", "rank": "R"}, "means": ["in_person"]}]})";
    };
    ChainData out;
    std::string e;
    SC_EXPECT(LoadChainJson(chain("", ""), places, means, out, &e));
    SC_EXPECT(!LoadChainJson(chain(R"(, "colour": "red")", ""), places, means, out, &e) && e.find("unknown key") != std::string::npos);
    SC_EXPECT(!LoadChainJson(chain("", "nobody"), places, means, out, &e) && e.find("no such position") != std::string::npos);
    SC_EXPECT(!LoadChainJson(chain("", "y"), places, means, out, &e) && e.find("loops") != std::string::npos);
}

SC_TEST("command: a raid warning reaches the battery in minutes; the player's character passes nothing on by itself") {
    const ChainData d = LoadChain();
    const double start = StartHour();
    const double h0 = At(10.0, start);
    InfoNet net;
    SC_EXPECT(net.Init(d, 1u, start));
    const int fi = net.AddFact(Warning(h0));
    net.AdvanceTo(h0 + 0.5);
    // The player commands the battery: told by the regiment's duty officer within minutes; the vehicles not (yet).
    const int bty = d.PositionIndex("bty21_commander");
    const Delivery* got = net.Known(bty, fi);
    SC_EXPECT(got != nullptr);
    if (got != nullptr) {
        std::printf("    battery commander warned after %.1f min: %s\n", (got->h - h0) * 60.0, net.Describe(*got).c_str());
        SC_EXPECT(got->h - h0 < 10.0 / 60.0);
        SC_EXPECT(d.positions[static_cast<std::size_t>(got->from)].id == "rgt_cp_duty");
    }
    SC_EXPECT(net.Known(d.PositionIndex("veh21_1_commander"), fi) == nullptr);
    SC_EXPECT(net.Known(d.PositionIndex("veh21_1_operator"), fi) == nullptr);
    // The town hears the sirens; the commandant is told too.
    SC_EXPECT(net.Known(d.PositionIndex("kesmark_civil_defence"), fi) != nullptr);

    // The player is vehicle 21-1's operator: the battery commander warns the vehicles, the vehicle commander the crew.
    InfoNet crew;
    crew.Init(d, 1u, start);
    crew.SetPlayer("veh21_1_operator");
    const int fc = crew.AddFact(Warning(h0));
    crew.AdvanceTo(h0 + 0.5);
    const Delivery* op = crew.Known(d.PositionIndex("veh21_1_operator"), fc);
    SC_EXPECT(op != nullptr);
    if (op != nullptr) {
        std::printf("    operator warned after %.1f min: %s\n", (op->h - h0) * 60.0, crew.Describe(*op).c_str());
        SC_EXPECT(op->h - h0 < 12.0 / 60.0);
        SC_EXPECT(d.positions[static_cast<std::size_t>(op->from)].id == "veh21_1_commander");
        SC_EXPECT(d.means[static_cast<std::size_t>(op->mean)].kind == MeanKind::InPerson);
    }
    // A preview of the warning's lead time agrees with the run.
    InfoNet fresh;
    fresh.Init(d, 1u, start);
    fresh.SetPlayer("veh21_1_operator");
    const int ff = fresh.AddFact(Warning(h0));
    fresh.AdvanceTo(h0);
    SC_EXPECT(op != nullptr && std::fabs(fresh.WhenKnown(ff, d.PositionIndex("veh21_1_operator"), h0 + 0.5) - op->h) < 1e-9);
}

SC_TEST("command: routine news waits for a contact: the army's note reaches the battery commander by the evening, the vehicles next morning") {
    const ChainData d = LoadChain();
    const double start = StartHour();
    InfoNet net;
    net.Init(d, 1u, start);
    net.SetPlayer("veh21_1_driver");  // the battery commander is an NPC here
    Fact f;
    f.key = "test:note";
    f.kind = FactKind::EnemyOfficer;
    f.h = At(10.0, start);
    f.origin = "army_intel";
    f.text = "The officer planning the raids, our analysts' \"the feint artist\": opens with a feint from another direction.";
    f.summary = "Army intelligence has a note on the officer planning the raids.";
    f.audience = "rgt_commander";
    f.need_level = 4;  // down to the vehicle commanders
    f.document = true;
    const int fi = net.AddFact(f);
    net.AdvanceTo(At(23.0, start));
    // The same day, by the evening at the latest, by a contact (the courier round or the briefing), the paper itself.
    const Delivery* got = net.Known(d.PositionIndex("bty21_commander"), fi);
    SC_EXPECT(got != nullptr);
    if (got != nullptr) {
        std::printf("    %s%s\n", net.Describe(*got).c_str(), got->full ? " (the paper)" : " (a summary)");
        const MeanKind k = d.means[static_cast<std::size_t>(got->mean)].kind;
        SC_EXPECT(got->h > At(10.0, start) + 1.0 && got->h <= At(20.0, start));
        SC_EXPECT(k == MeanKind::Courier || k == MeanKind::InPerson);
        SC_EXPECT(got->full);
    }
    // The vehicle commanders hear it at the next morning's orders, not before.
    const Delivery* veh = net.Known(d.PositionIndex("veh21_2_commander"), fi);
    SC_EXPECT(veh == nullptr);
    net.AdvanceTo(At(10.0, start) + 24.0);
    veh = net.Known(d.PositionIndex("veh21_2_commander"), fi);
    if (veh != nullptr) std::printf("    vehicle 21-2: %s\n", net.Describe(*veh).c_str());
    SC_EXPECT(veh != nullptr && veh->h >= At(6.0, start) + 24.0 && veh->h <= At(6.5, start) + 24.0);
}

SC_TEST("command: wire if it is whole, radio if not (and the enemy hears the radio); couriers die where bombs fall") {
    const ChainData d = LoadChain();
    const double start = StartHour();
    auto order = [&](const std::string& key, double h) {
        Fact f;
        f.key = key;
        f.kind = FactKind::Order;
        f.h = h;
        f.origin = "rgt_chief_of_staff";
        f.text = "Move to the reserve position by 02:00.";
        f.audience = "bty21_commander";
        f.need_level = 3;
        f.precedence = 1;
        return f;
    };
    const double h = At(12.0, start);
    InfoNet wire;
    wire.Init(d, 1u, start);
    wire.SetPlayer("veh21_1_driver");
    const int a = wire.AddFact(order("test:wire", h));
    wire.AdvanceTo(h + 2.0);
    const Delivery* by_wire = wire.Known(d.PositionIndex("bty21_commander"), a);
    SC_EXPECT(by_wire != nullptr && d.means[static_cast<std::size_t>(by_wire->mean)].kind == MeanKind::FieldTelephone);
    SC_EXPECT(wire.RadioCalls().empty());

    InfoNet cut;
    cut.Init(d, 1u, start);
    cut.SetPlayer("veh21_1_driver");
    cut.CutWire("bty21_cp", h - 1.0, h + 6.0);
    const int b = cut.AddFact(order("test:cut", h));
    cut.AdvanceTo(h + 2.0);
    const Delivery* by_radio = cut.Known(d.PositionIndex("bty21_commander"), b);
    SC_EXPECT(by_radio != nullptr && d.means[static_cast<std::size_t>(by_radio->mean)].kind == MeanKind::RadioVoice);
    SC_EXPECT(cut.RadioCalls().size() == 1);

    // A despatch rider through a raid: lost, the message goes again an hour later.
    auto rider = [&](bool bombed) {
        InfoNet n;
        n.Init(d, 1u, start);
        n.SetPlayer("veh21_1_driver");
        n.CutWire("bty21_cp", h - 1.0, h + 6.0);
        n.JamRadio(h - 1.0, h + 6.0);
        if (bombed) {
            Danger z;
            z.center = (n.PlacePos(d.PlaceIndex("rgt_cp"), h) + n.PlacePos(d.PlaceIndex("bty21_cp"), h)) * 0.5;
            z.radius_m = 4000.0;
            z.t0_h = h;
            z.t1_h = h + 2.0;
            z.chance = 1.0;
            z.known_h = h;
            n.AddDanger(z);
        }
        const int fi = n.AddFact(order("test:rider", h));
        n.AdvanceTo(h + 6.0);
        const Delivery* g = n.Known(d.PositionIndex("bty21_commander"), fi);
        return g == nullptr ? -1.0 : g->h;
    };
    const double clear = rider(false), bombed = rider(true);
    std::printf("    the order by despatch rider: %.0f min on a clear road, %.0f min through the raid\n", (clear - h) * 60.0, (bombed - h) * 60.0);
    SC_EXPECT(clear > h && bombed > clear + 0.9);
}

SC_TEST("command: the net is the same advanced in steps or at once") {
    const ChainData d = LoadChain();
    const double start = StartHour();
    auto build = [&](InfoNet& n) {
        n.Init(d, 7u, start);
        n.SetPlayer("veh21_3_commander");
        n.AddFact(Warning(At(9.0, start)));
        Fact f;
        f.key = "test:report";
        f.kind = FactKind::RaidSeen;
        f.h = At(11.0, start);
        f.origin = "div_ad_cp_duty";
        f.text = "8 aircraft from W2 went for the Kesmark road bridge.";
        f.audience = "rgt_commander";
        f.need_level = 4;
        n.AddFact(f);
        Danger z;
        z.center = n.PlacePos(d.PlaceIndex("rgt_cp"), 0.0);
        z.radius_m = 5000.0;
        z.t0_h = At(17.0, start);
        z.t1_h = At(19.0, start);
        z.chance = 0.5;
        z.known_h = At(16.0, start);
        n.AddDanger(z);
    };
    InfoNet once, steps;
    build(once);
    build(steps);
    once.AdvanceTo(At(9.0, start) + 30.0);
    for (double t = At(9.0, start); t <= At(9.0, start) + 30.0 + 1e-9; t += 0.37) steps.AdvanceTo(t);
    steps.AdvanceTo(At(9.0, start) + 30.0);
    bool same = true;
    int count = 0;
    for (std::size_t p = 0; p < d.positions.size(); ++p) {
        const auto a = once.KnownBy(static_cast<int>(p));
        const auto b = steps.KnownBy(static_cast<int>(p));
        same = same && a.size() == b.size();
        for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
            same = same && a[i].fact == b[i].fact && a[i].h == b[i].h && a[i].mean == b[i].mean && a[i].from == b[i].from;
            ++count;
        }
    }
    std::printf("    %d deliveries, identical: %s\n", count, same ? "yes" : "no");
    SC_EXPECT(same && count > 20);
}

SC_TEST("command: the air war's news reaches the player's character late, partly, and from someone") {
    airwar::AirWarData aw;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", aw, &e));
    const ChainData d = LoadChain();
    // "Partly" is a property of a campaign's course, not of every seed's: a war whose last raid the warning net saw
    // long enough before the end reaches the battery commander whole. Take the first seed from 3 on whose two days
    // show it (the course moves with the resolver, roadmap B2 and B5), and check everything else on that one.
    for (unsigned seed = 3u; seed <= 14u; ++seed) {
        airwar::AirCampaign c;
        SC_EXPECT(c.Init(aw, seed));
        c.Posture().relocate_every_h = 20.0;
        InfoNet net;
        SC_EXPECT(net.Init(d, seed, aw.theatre.start_hour));
        CampaignFeed feed;
        feed.Bind(&net);
        int warned_in_time = 0, warnings = 0;
        while (c.Now() < 48.0) {
            const bool raid = c.Advance(48.0);
            feed.Pump(c);
            if (!raid) continue;
            // The raid warning: the player (the battery commander) gets it before the raid is over the sector.
            const airwar::Package* p = c.PlayerMission();
            const int fi = net.FactIndex("journal:" + std::to_string(c.Journal().size() - 1));
            if (fi >= 0 && net.Facts()[static_cast<std::size_t>(fi)].kind == FactKind::AirWarning) {
                ++warnings;
                const double when = net.WhenKnown(fi, net.Player(), p->tot_h);
                if (when >= 0.0 && when < p->tot_h) ++warned_in_time;
            }
            c.SkipPlayerMission();
            feed.Pump(c);
        }
        const auto known = net.KnownBy(net.Player());
        int raids_known = 0, firsthand = 0, late = 0;
        for (const Delivery& k : known) {
            const Fact& f = net.Facts()[static_cast<std::size_t>(k.fact)];
            SC_EXPECT(k.h >= f.h - 1e-9);  // nobody knows a thing before it happened
            if (f.kind == FactKind::RaidSeen) ++raids_known;
            if (k.from == -1) ++firsthand;
            if (k.h - f.h > 1.0) ++late;
        }
        if (raids_known >= static_cast<int>(c.ObservedRaids().size()) && seed < 14u) continue;
        std::printf("    seed %u, two days against NATO: %zu facts in the pipeline, the battery commander knows %zu (%d firsthand, %d learned more than an hour late); "
                    "raids seen by the warning net %zu, known to the battery commander %d; raid warnings in time %d of %d\n",
                    seed, net.Facts().size(), known.size(), firsthand, late, c.ObservedRaids().size(), raids_known, warned_in_time, warnings);
        SC_EXPECT(!known.empty() && known.size() < net.Facts().size());
        SC_EXPECT(raids_known < static_cast<int>(c.ObservedRaids().size()));
        SC_EXPECT(late > 0);
        SC_EXPECT(warnings == 0 || warned_in_time * 2 >= warnings);
        // Every delivery says who told them and how (or that they saw it).
        for (const Delivery& k : known) SC_EXPECT(!net.Describe(k).empty());
        break;
    }
}

SC_TEST("command: deliveries as scenes (P1): the warning on the battery net, the evening briefing as one scene, the map case") {
    const ChainData d = LoadChain();
    const double start = StartHour();
    const double h0 = At(10.0, start);
    InfoNet net;
    net.Init(d, 1u, start);
    net.SetPlayer("veh21_1_operator");
    const int fw = net.AddFact(Warning(h0));
    Fact note;
    note.key = "test:note";
    note.kind = FactKind::EnemyOfficer;
    note.h = h0;
    note.origin = "army_intel";
    note.text = "The officer planning the raids opens with a feint from another direction.";
    note.summary = "Army intelligence has a note on the officer planning the raids.";
    note.audience = "rgt_commander";
    note.need_level = 4;
    note.document = true;
    const int fn = net.AddFact(note);
    net.AdvanceTo(At(23.0, start));

    // The vehicle commander hears the warning from the battery commander on the radio; the code word is in the words.
    const Delivery* vw = net.Known(d.PositionIndex("veh21_1_commander"), fw);
    SC_EXPECT(vw != nullptr);
    if (vw != nullptr) {
        const Scene s = SceneOf(net, *vw);
        SC_EXPECT(s.scene == SceneKind::Radio);
        SC_EXPECT(s.teller == d.PositionIndex("bty21_commander") && !s.paper);
        SC_EXPECT(s.words.find("Воздух") != std::string::npos);
        const DeliveryDef def = DeliveryOf(net, s, "commander", h0);
        SC_EXPECT(def.means == DeliveryMeans::Radio && def.net == "battery net" && def.air_warning);
        SC_EXPECT(std::fabs(def.after_s - (vw->h - h0) * 3600.0) < 1e-6);
    }
    // The operator hears it from his commander inside the vehicle: on the intercom.
    const Delivery* ow = net.Known(d.PositionIndex("veh21_1_operator"), fw);
    SC_EXPECT(ow != nullptr && DeliveryOf(net, SceneOf(net, *ow), "operator", h0).means == DeliveryMeans::Intercom);

    // The battery commander gets the note's paper by a contact; it is in his map case, with who gave it.
    const int bty = d.PositionIndex("bty21_commander");
    const std::vector<Paper> papers = MapCase(net, bty);
    bool has_note = false;
    for (const Paper& p : papers) {
        if (p.fact_key != "test:note") continue;
        has_note = true;
        SC_EXPECT(p.title == "Notes on the enemy's officers" && !p.from.empty() && p.received_h >= h0);
        std::printf("    map case: %s, from %s at %s\n", p.title.c_str(), p.from.c_str(), FormatClock(p.received_h, start).c_str());
    }
    SC_EXPECT(has_note);
    // The air-raid warning is not a paper.
    for (const Paper& p : papers) SC_EXPECT(p.fact_key != "test:warning");
    // If the note came at the evening briefing, the briefing is one scene with it among its items.
    const Delivery* got = net.Known(bty, fn);
    if (got != nullptr && got->mean == d.MeanIndex("in_person")) {
        bool in_briefing = false;
        for (const BriefingScene& b : Briefings(net, bty)) {
            for (const Scene& s : b.items) in_briefing = in_briefing || s.fact_key == "test:note";
            std::printf("    briefing: %s at %s, %zu items\n", b.name.c_str(), FormatClock(b.start_h, start).c_str(), b.items.size());
        }
        SC_EXPECT(in_briefing);
    }
}

SC_TEST("command: the pipeline seen from the Osa (P1): its seats' deliveries in a session's window, the raid's warning time") {
    const ChainData d = LoadChain();
    const double start = StartHour();
    const double h0 = At(5.0, start);
    InfoNet net;
    net.Init(d, 1u, start);
    net.SetPlayer("veh21_1_operator");
    Fact w = Warning(h0);
    w.subject = "package:7";
    net.AddFact(w);
    net.AdvanceTo(h0 - 0.1);  // the session is written before the warning goes out
    const SessionSource osa(net, "osa");
    SC_EXPECT(!osa.Empty());
    const std::vector<DeliveryDef> msgs = osa.Between(7, h0 - 0.05, h0 + 0.5);
    // The commander on the battery net, then the crew on the intercom; the commander's is the raid's warning.
    bool commander = false, crew = false;
    for (const DeliveryDef& m : msgs) {
        std::printf("    %+.0f s to %s by %s: %s\n", m.after_s, m.to.c_str(), DeliveryMeansName(m.means), m.from.c_str());
        if (m.to == "commander") commander = m.means == DeliveryMeans::Radio && m.air_warning && m.after_s > 0.0;
        if (m.means == DeliveryMeans::Intercom) crew = m.to.empty();  // once, to the whole crew
    }
    SC_EXPECT(commander && crew);
    // Another raid's warning would only be said.
    for (const DeliveryDef& m : osa.Between(8, h0 - 0.05, h0 + 0.5)) SC_EXPECT(!m.air_warning);
    // The first of the vehicle's people to know it, as the raid builder asks.
    const double warned = osa.AirWarnedAt(7, h0 + 0.5);
    SC_EXPECT(warned > h0 && warned < h0 + 0.2);
    SC_EXPECT(osa.AirWarnedAt(8, h0 + 0.5) < -1e17);
    // The PU-12M holds the battery commander's seat.
    SC_EXPECT(!SessionSource(net, "post").Between(7, h0 - 0.05, h0 + 0.5).empty());
    // Played from the commander's seat instead: the battery commander's call is his, and nobody tells him on the intercom.
    InfoNet from_cmdr = net;
    SC_EXPECT(PlayFromSeat(from_cmdr, "osa:commander"));
    SC_EXPECT(from_cmdr.Player() == PositionAtSeat(d, "osa:commander"));
    SC_EXPECT(!PlayFromSeat(from_cmdr, "osa:loader"));
    bool told = false;
    for (const DeliveryDef& m : SessionSource(from_cmdr, "osa").Between(7, h0 - 0.05, h0 + 0.5)) {
        SC_EXPECT(m.means != DeliveryMeans::Intercom);
        told = told || (m.to == "commander" && m.air_warning);
    }
    SC_EXPECT(told);
}
SC_TEST("command: the player tells people (P2): the battery commander warns a vehicle by radio, its crew in turn; the means and their cost") {
    const ChainData d = LoadChain();
    const double start = StartHour();
    const double h0 = At(10.0, start);
    InfoNet net;
    SC_EXPECT(net.Init(d, 1u, start));  // the player commands the battery
    const int fi = net.AddFact(Warning(h0));
    net.AdvanceTo(h0 + 0.1);  // he is warned in about 3 minutes
    const int bty = d.PositionIndex("bty21_commander");
    const int v1 = d.PositionIndex("veh21_1_commander");
    const Delivery* got = net.Known(bty, fi);
    SC_EXPECT(got != nullptr);
    if (got == nullptr) return;
    const double now = got->h + 0.002;
    // Whom he can tell, and how: the radio at once (the enemy hears it), a runner on foot, no telephone line to the
    // vehicle; the vehicle commander is under him.
    const std::vector<int> contacts = net.Contacts(bty);
    SC_EXPECT(std::find(contacts.begin(), contacts.end(), v1) != contacts.end());
    bool radio = false, runner = false, phone_refused = false;
    for (const TellOption& o : net.TellOptions(bty, v1, fi, now)) {
        std::printf("    %-16s %s%s\n", o.mean.c_str(), o.arrive_h >= 0.0 ? ("in " + std::to_string(static_cast<int>(std::lround((o.arrive_h - now) * 3600.0))) + " s").c_str() : "",
                    o.why_not.empty() ? "" : ("no: " + o.why_not).c_str());
        if (o.mean == "radio_voice") radio = o.arrive_h > now && o.arrive_h < now + 0.05 && o.intercept >= 1.0;
        if (o.mean == "messenger") runner = o.arrive_h > now && o.road;
        if (o.mean == "field_telephone") phone_refused = o.arrive_h < 0.0 && !o.why_not.empty();
    }
    SC_EXPECT(radio && runner && phone_refused);
    // He can't tell what he doesn't know yet, nor by a means he lacks.
    std::string why;
    SC_EXPECT(!net.Tell({got->h - 0.01, bty, v1, fi, "radio_voice"}, &why) && why == "you don't know it yet");
    SC_EXPECT(!net.Tell({now, bty, v1, fi, "field_telephone"}, &why));
    const std::size_t calls = net.RadioCalls().size();
    SC_EXPECT(net.Tell({now, bty, v1, fi, "radio_voice"}, &why));
    net.AdvanceTo(h0 + 0.5);
    // Vehicle 21-1's commander has it from him by radio, and tells his crew; vehicle 21-2 heard nothing.
    const Delivery* cmdr = net.Known(v1, fi);
    SC_EXPECT(cmdr != nullptr && cmdr->told && cmdr->from == bty && d.means[static_cast<std::size_t>(cmdr->mean)].kind == MeanKind::RadioVoice);
    const Delivery* op = net.Known(d.PositionIndex("veh21_1_operator"), fi);
    SC_EXPECT(op != nullptr && op->from == v1 && !op->told && cmdr != nullptr && op->h > cmdr->h);
    SC_EXPECT(net.Known(d.PositionIndex("veh21_2_commander"), fi) == nullptr);
    // The enemy's intercept service heard the call.
    SC_EXPECT(net.RadioCalls().size() == calls + 1);
}

SC_TEST("command: the player's own report goes only where he tells it (P2), and his tellings come back with a save") {
    const ChainData d = LoadChain();
    const double start = StartHour();
    const double h0 = At(14.0, start);
    InfoNet net;
    net.Init(d, 3u, start);
    net.AdvanceTo(h0);
    Fact r;
    r.key = "report:bty21:1";
    r.kind = FactKind::RaidResult;
    r.h = h0;
    r.text = "Battery 21 engaged a pair of fighter-bombers at 13:52 from the west; one shot down, three missiles spent.";
    r.summary = "Battery 21: one of a pair shot down.";
    r.audience = "rgt_commander";
    r.need_level = 2;
    r.precedence = 1;
    const int fr = net.Report(r);
    SC_EXPECT(fr >= 0 && net.Facts()[static_cast<std::size_t>(fr)].held && net.Facts()[static_cast<std::size_t>(fr)].origin == "bty21_commander");
    net.AdvanceTo(h0 + 2.0);
    const int rgt = d.PositionIndex("rgt_commander");
    SC_EXPECT(net.Known(rgt, fr) == nullptr);  // not told, not known
    const int bty = d.PositionIndex("bty21_commander");
    std::string why;
    SC_EXPECT(net.Tell({h0 + 2.0, bty, rgt, fr, "field_telephone"}, &why));
    net.AdvanceTo(h0 + 3.0);
    const Delivery* got = net.Known(rgt, fr);
    SC_EXPECT(got != nullptr && got->told && d.means[static_cast<std::size_t>(got->mean)].kind == MeanKind::FieldTelephone);
    if (got != nullptr) std::printf("    %s\n", net.Describe(*got).c_str());

    // Saved and put back into a pipeline fed the same news: the same deliveries.
    const std::string saved = SavePlayerInputs(net);
    InfoNet again;
    again.Init(d, 3u, start);
    again.AdvanceTo(h0 + 3.0);
    std::string err;
    SC_EXPECT(LoadPlayerInputs(again, saved, &err) && err.empty());
    const int fr2 = again.FactIndex("report:bty21:1");
    const Delivery* got2 = again.Known(rgt, fr2);
    SC_EXPECT(fr2 >= 0 && got2 != nullptr && got != nullptr && std::fabs(got2->h - got->h) < 1e-9 && got2->told);
    SC_EXPECT(again.Tellings().size() == 1 && again.KnownBy(rgt).size() == net.KnownBy(rgt).size());
    SC_EXPECT(!LoadPlayerInputs(again, "{ not json", &err));
}
SC_TEST("command: the enemy's radio intercept hears the calls the battery made (P3), not a guess from its posture") {
    airwar::AirWarData aw;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", aw, &e));
    const ChainData d = LoadChain();
    // Two days; the battery commander (the player) passes every piece of news to his vehicle commanders by radio, or
    // keeps it to himself.
    auto run = [&](bool talk, airwar::AirCampaign& c) {
        c.Init(aw, 5u);  // seed 5: the war's course since roadmap S7 (seed 4's two days give the battery's net nothing to say)
        InfoNet net;
        net.Init(d, 5u, aw.theatre.start_hour);
        CampaignFeed feed;
        feed.Bind(&net);
        feed.Pump(c);
        c.NoteTransmissions(feed.BatteryCalls());
        std::vector<int> vehicles;
        for (const char* v : {"veh21_1_commander", "veh21_2_commander", "veh21_3_commander", "veh21_4_commander"}) vehicles.push_back(d.PositionIndex(v));
        std::vector<char> told;
        while (c.Now() < 48.0) {
            if (c.Advance(48.0)) c.SkipPlayerMission();
            feed.Pump(c);
            if (talk) {
                told.resize(net.Facts().size(), 0);
                for (const Delivery& k : net.KnownBy(net.Player())) {
                    if (told[static_cast<std::size_t>(k.fact)]) continue;
                    told[static_cast<std::size_t>(k.fact)] = 1;
                    for (int v : vehicles) net.Tell({net.Now(), net.Player(), v, k.fact, "radio_voice"});
                }
            }
            c.NoteTransmissions(feed.BatteryCalls());
        }
    };
    airwar::AirCampaign silent, chatty;
    run(false, silent);
    run(true, chatty);
    std::printf("    radio intercept reports on the battery: silent %d, telling every piece of news by radio %d\n", silent.ComintHeard(), chatty.ComintHeard());
    SC_EXPECT(silent.TrafficFromPipeline() && chatty.TrafficFromPipeline());
    SC_EXPECT(silent.ComintHeard() == 0 && chatty.ComintHeard() > 0);
    // Without the pipeline the posture's radio discipline still stands for the traffic (headless runs, the balance).
    airwar::AirCampaign guessed;
    guessed.Init(aw, 5u);
    while (guessed.Now() < 48.0) {
        if (guessed.Advance(48.0)) guessed.SkipPlayerMission();
    }
    SC_EXPECT(!guessed.TrafficFromPipeline() && guessed.ComintHeard() > 0);
    // The calls are in the save: loaded, the same campaign.
    airwar::AirCampaign loaded;
    SC_EXPECT(airwar::LoadCampaignJson(airwar::SaveCampaignJson(chatty, "air_war/theatre_pannonia.json", "air_war/faction_nato.json"), kData, loaded, &e));
    SC_EXPECT(loaded.ComintHeard() == chatty.ComintHeard() && loaded.Actions().size() == chatty.Actions().size() && loaded.Clock() == chatty.Clock());
}
SC_TEST("command: the crew's calls become its commander's report (P4): the battery commander has it, and reports it up as he tells it") {
    const ChainData d = LoadChain();
    const double start = StartHour();
    const double h0 = At(11.0, start);
    std::vector<ScenarioRunner::CrewCall> calls = {{40.0, "launch", "T1"}, {52.0, "destroyed", "T1"}, {80.0, "launch", "T2"}, {93.0, "missed", "T2"}};
    InfoNet net;
    net.Init(d, 5u, start);  // the player commands the battery
    net.AdvanceTo(h0);
    Fact f;
    SC_EXPECT(!CommanderReport(net, {}, "osa", h0, "crew:raid:1", f));
    SC_EXPECT(CommanderReport(net, calls, "osa", h0, "crew:raid:1", f));
    std::printf("    %s\n    %s\n", f.summary.c_str(), f.text.c_str());
    SC_EXPECT(f.origin == "veh21_1_commander" && !f.held && f.kind == FactKind::RaidResult && std::fabs(f.h - (h0 + 93.0 / 3600.0)) < 1e-9);
    SC_EXPECT(f.summary.find("2 missiles launched, 1 target destroyed") != std::string::npos);
    SC_EXPECT(f.text.find("target T2 missed") != std::string::npos);
    const int fi = net.AddFact(f);
    net.AdvanceTo(h0 + 1.0);
    const int bty = d.PositionIndex("bty21_commander");
    const int rgt = d.PositionIndex("rgt_commander");
    const Delivery* got = net.Known(bty, fi);
    SC_EXPECT(got != nullptr && got->from == d.PositionIndex("veh21_1_commander"));
    SC_EXPECT(net.Known(rgt, fi) == nullptr);  // the battery commander (the player) passes nothing on by himself
    SC_EXPECT(net.Tell({h0 + 1.0, bty, rgt, fi, "field_telephone"}));
    net.AdvanceTo(h0 + 2.0);
    SC_EXPECT(net.Known(rgt, fi) != nullptr);
    // The campaign cannot make the report again: it is kept with the player's inputs, and plays out the same.
    InfoNet loaded;
    loaded.Init(d, 5u, start);
    loaded.AdvanceTo(h0 + 2.0);
    SC_EXPECT(LoadPlayerInputs(loaded, SavePlayerInputs(net)));
    const int fl = loaded.FactIndex("crew:raid:1");
    SC_EXPECT(fl >= 0 && !loaded.Facts()[static_cast<std::size_t>(fl)].held && loaded.Facts()[static_cast<std::size_t>(fl)].witnesses == f.witnesses);
    SC_EXPECT(fl >= 0 && loaded.Known(rgt, fl) != nullptr);
    // The player is the vehicle commander: the report is his own, and goes nowhere until he tells it.
    InfoNet mine;
    mine.Init(d, 5u, start);
    mine.SetPlayer("veh21_1_commander");
    mine.AdvanceTo(h0);
    SC_EXPECT(CommanderReport(mine, calls, "osa", h0, "crew:raid:1", f) && f.held);
    const int fm = mine.AddFact(f);
    mine.AdvanceTo(h0 + 1.0);
    SC_EXPECT(mine.Known(bty, fm) == nullptr && mine.Known(mine.Player(), fm) != nullptr);
}

namespace {
em::EmLibrary LoadOsaEm() {
    em::EmLibrary lib;
    const em::EmLoadResult r = em::LoadEmLibraryFile(kData + "/em/osa_ak.json", lib);
    if (!r.ok) std::printf("    %s\n", r.error.c_str());
    return lib;
}
}  // namespace

SC_TEST("command: radio hops through the EM layer (P5): each set's range, a mixed pair, jamming, the enemy listening") {
    const em::EmLibrary lib = LoadOsaEm();
    const int r123 = lib.Radio("r123m"), r111 = lib.Radio("r111");
    SC_EXPECT(r123 >= 0 && r111 >= 0);
    if (r123 < 0 || r111 < 0) return;
    EmRadio radio(lib.radios[static_cast<std::size_t>(r123)], nullptr);
    const int big = radio.AddSet(lib.radios[static_cast<std::size_t>(r111)]);
    radio.Equip(1, big);
    radio.Equip(2, big);
    const Vec3 o{0.0, 0.0, 0.0};
    auto at = [](double km) { return Vec3{0.0, km * 1000.0, 0.0}; };
    // Two R-123Ms over flat ground: sure at 5 km, at the published 15 km on the edge (margin 0: one call in two said
    // again), nothing at 25 km. Two R-111s reach their 35 km; an R-111 to an R-123M lies between.
    const RadioPath near = radio.Hop(0, 3, o, at(5.0), 1.0);
    const RadioPath edge = radio.Hop(0, 3, o, at(15.0), 1.0);
    const RadioPath beyond = radio.Hop(0, 3, o, at(25.0), 1.0);
    std::printf("    R-123M: %.1f dB at 5 km, %.1f at 15, %.1f at 25; R-111 %.1f at 35; R-111 to R-123M %.1f at 25\n", near.margin_db, edge.margin_db,
                beyond.margin_db, radio.Hop(1, 2, o, at(35.0), 1.0).margin_db, radio.Hop(1, 3, o, at(25.0), 1.0).margin_db);
    SC_EXPECT(near.up && near.loss < 0.01 && near.heard == 1.0);
    SC_EXPECT_NEAR(edge.margin_db, 0.0, 0.5);
    SC_EXPECT_NEAR(edge.loss, 0.5, 0.06);
    SC_EXPECT(!beyond.up && beyond.why == "out of range");
    SC_EXPECT_NEAR(radio.Hop(1, 2, o, at(35.0), 1.0).margin_db, 0.0, 0.5);
    const double mixed = radio.Hop(1, 3, o, at(25.0), 1.0).margin_db;
    SC_EXPECT(mixed > beyond.margin_db + 3.0 && mixed < radio.Hop(1, 2, o, at(25.0), 1.0).margin_db - 3.0);
    // A barrage jammer aloft 30 km off, from hour 2 to 3, drowns even the near call (generic placeholder numbers).
    RadioJammer j;
    j.id = "barrage";
    j.pos = Vec3{30000.0, 5000.0, 0.0};
    j.height_m = 3000.0;
    j.erp_w = 2000.0;
    j.bandwidth_hz = 40e6;
    j.from_h = 2.0;
    j.until_h = 3.0;
    radio.AddJammer(j);
    SC_EXPECT(radio.Hop(0, 3, o, at(5.0), 1.5).up);
    const RadioPath jammed = radio.Hop(0, 3, o, at(5.0), 2.5);
    SC_EXPECT(!jammed.up && jammed.why == "the net is jammed");
    SC_EXPECT(radio.Hop(0, 3, o, at(5.0), 3.5).up);
    // The enemy's listener: one 10 km off hears every call, one 120 km off hears none; when he is not there, nothing.
    RadioListener l;
    l.id = "intercept";
    l.pos = Vec3{-10000.0, 0.0, 0.0};
    l.sensitivity_dbw = -140.0;
    l.from_h = 0.0;
    l.until_h = 10.0;
    radio.AddListener(l);
    SC_EXPECT(radio.Hop(0, 3, o, at(5.0), 1.0).heard > 0.99);
    SC_EXPECT(radio.Hop(0, 3, o, at(5.0), 11.0).heard == 0.0);
    EmRadio far(lib.radios[static_cast<std::size_t>(r123)], nullptr);
    l.pos = Vec3{-120000.0, 0.0, 0.0};
    far.AddListener(l);
    SC_EXPECT(far.Hop(0, 3, o, at(5.0), 1.0).heard < 0.01);
}

SC_TEST("command: the pipeline's radio through the EM layer (P5): over the country's ground, the command posts on R-111s, jammed, heard") {
    const ChainData d = LoadChain();
    const double start = StartHour();
    const double h0 = At(9.0, start);
    const em::EmLibrary lib = LoadOsaEm();
    std::string e;
    const std::shared_ptr<const CountryMap> country = LoadCountryMap(kData, "maps/pannonia.json", &e);
    SC_EXPECT(country != nullptr && lib.Radio("r111") >= 0);
    if (country == nullptr || lib.Radio("r111") < 0) {
        std::printf("    %s\n", e.c_str());
        return;
    }
    auto model = std::make_shared<EmRadio>(lib.radios[static_cast<std::size_t>(lib.Radio("r123m"))], std::make_shared<CountryGround>(country));
    model->EquipCommandPosts(d, 3, model->AddSet(lib.radios[static_cast<std::size_t>(lib.Radio("r111"))]));
    const int bty = d.PositionIndex("bty21_commander");
    const int rgt = d.PositionIndex("rgt_commander");
    const int v1 = d.PositionIndex("veh21_1_commander");
    SC_EXPECT(model->SetOf(bty).id == "r111" && model->SetOf(rgt).id == "r111" && model->SetOf(v1).id == "r123m");
    // Who can be called: the battery commander has the regiment 17 km off on his R-111; a vehicle's R-123M does not
    // reach it, and the vehicle is close.
    auto pos = [&](int p) { return d.FindPlace(d.positions[static_cast<std::size_t>(p)].place)->pos; };
    const RadioPath up_the_chain = model->Hop(bty, rgt, pos(bty), pos(rgt), h0);
    const RadioPath from_vehicle = model->Hop(v1, rgt, pos(v1), pos(rgt), h0);
    const RadioPath down = model->Hop(bty, v1, pos(bty), pos(v1), h0);
    std::printf("    battery to regiment %.1f dB, vehicle to regiment %.1f dB, battery to vehicle %.1f dB\n", up_the_chain.margin_db,
                from_vehicle.margin_db, down.margin_db);
    SC_EXPECT(up_the_chain.up && down.up && down.loss < 0.01);
    SC_EXPECT(from_vehicle.margin_db < up_the_chain.margin_db - 6.0);
    // In the pipeline: the battery commander tells the regiment's duty officer by radio, unless a jammer near the regiment's
    // command post drowns it (its commander may be anywhere: at this hour he is visiting the battery).
    const int duty = d.PositionIndex("rgt_cp_duty");
    InfoNet net;
    SC_EXPECT(net.Init(d, 1u, start));
    const int fi = net.AddFact(Warning(h0));
    net.AdvanceTo(h0 + 0.1);
    const Delivery* got = net.Known(bty, fi);
    SC_EXPECT(got != nullptr);
    if (got == nullptr) return;
    const double now = got->h + 0.002;
    auto radio_option = [&](const InfoNet& n) {
        for (const TellOption& o : n.TellOptions(bty, duty, fi, now)) {
            if (o.mean == "radio_voice") return o;
        }
        return TellOption();
    };
    net.SetRadioModel(model);
    net.AdvanceTo(h0 + 0.1);
    SC_EXPECT(radio_option(net).arrive_h > 0.0 && radio_option(net).intercept == 1.0);
    RadioJammer j;
    j.id = "barrage";
    j.pos = net.PlacePos(net.PlaceOf(duty, now), now) + Vec3{15000.0, 0.0, 0.0};
    j.height_m = 3000.0;
    j.erp_w = 2000.0;
    j.bandwidth_hz = 40e6;
    j.from_h = now - 0.5;
    j.until_h = now + 0.5;
    auto jammed = std::make_shared<EmRadio>(*model);
    jammed->AddJammer(j);
    InfoNet under_jamming = net;
    under_jamming.SetRadioModel(jammed);
    under_jamming.AdvanceTo(h0 + 0.1);
    const TellOption refused = radio_option(under_jamming);
    SC_EXPECT(refused.arrive_h < 0.0 && refused.why_not == "the net is jammed");
    // An enemy listener far off hears the battery's calls only now and then: the intercept share falls with it.
    RadioListener l;
    l.id = "intercept";
    l.pos = pos(bty) + Vec3{-90000.0, 0.0, 0.0};
    l.height_m = 15.0;
    l.sensitivity_dbw = -140.0;
    auto listened = std::make_shared<EmRadio>(*model);
    listened->AddListener(l);
    InfoNet heard = net;
    heard.SetRadioModel(listened);
    heard.AdvanceTo(h0 + 0.1);
    const TellOption o = radio_option(heard);
    std::printf("    a listener 90 km off hears a call with chance %.2f\n", o.intercept);
    SC_EXPECT(o.arrive_h > 0.0 && o.intercept < 0.9);
    SC_EXPECT(heard.Tell({now, bty, duty, fi, "radio_voice"}));
    heard.AdvanceTo(h0 + 0.5);
    bool call_heard = false;
    for (const RadioCall& c : heard.RadioCalls()) call_heard = call_heard || (c.position == bty && c.heard < 0.9);
    SC_EXPECT(call_heard);
}

SC_TEST("command: two players' characters in one pipeline (P6): neither passes anything on by himself, each machine gets its own post's view, a client tells only from his own post") {
    const ChainData d = LoadChain();
    const double start = StartHour();
    const double h0 = At(10.0, start);
    InfoNet net;
    SC_EXPECT(net.Init(d, 1u, start));
    net.SetPlayers({"bty21_commander", "veh21_1_commander", "nobody"});
    const int bty = d.PositionIndex("bty21_commander");
    const int v1 = d.PositionIndex("veh21_1_commander");
    const int v1_operator = d.PositionIndex("veh21_1_operator");
    SC_EXPECT(net.Player() == bty && net.IsPlayer(v1) && !net.IsPlayer(v1_operator) && net.Players().size() == 2);
    const int fi = net.AddFact(Warning(h0));
    const std::string key = net.Facts()[static_cast<std::size_t>(fi)].key;
    net.AdvanceTo(h0 + 0.1);
    SC_EXPECT(net.Known(bty, fi) != nullptr && net.Known(v1, fi) == nullptr);
    // The vehicle commander's machine: nothing of it yet.
    PostView v1_view;
    SC_EXPECT(ReadPostView(PostViewJson(net, v1), v1_view));
    SC_EXPECT(v1_view.post == "veh21_1_commander" && !v1_view.contacts.empty());
    for (const ViewItem& it : v1_view.items) SC_EXPECT(it.key != key);
    // His client can't tell for the battery commander; the battery commander's client tells him by radio.
    std::string why;
    SC_EXPECT(!ApplyTellRequest(net, v1, TellRequestJson(key, "bty21_commander", "veh21_1_operator", "radio_voice"), &why));
    SC_EXPECT(why == "you can tell only what you know yourself");
    SC_EXPECT(ApplyTellRequest(net, bty, TellRequestJson(key, "bty21_commander", "veh21_1_commander", "radio_voice"), &why));
    net.AdvanceTo(h0 + 0.3);
    SC_EXPECT(net.Known(v1, fi) != nullptr);
    // A player passes nothing on by himself: the vehicle commander knows, his operator not until he tells him.
    SC_EXPECT(net.Known(v1_operator, fi) == nullptr);
    // The server sends what is new since the last view; the client merges it.
    PostView update;
    SC_EXPECT(ReadPostView(PostViewJson(net, v1, v1_view.now_h), update));
    SC_EXPECT(update.items.size() == 1 && update.items[0].key == key && update.items[0].from == "bty21_commander" &&
              update.items[0].means == "radio_voice" && !update.items[0].full);
    std::printf("    %s: %s\n", update.items[0].said.c_str(), update.items[0].text.c_str());
    const std::size_t before = v1_view.items.size();
    MergePostView(v1_view, update);
    SC_EXPECT(v1_view.items.size() == before + 1 && v1_view.now_h > h0 + 0.29);
    // His own report starts with him and goes nowhere by itself; the battery commander's view has none of it.
    Fact own;
    own.key = "report:v1:1";
    own.kind = FactKind::BatteryNews;
    own.h = net.Now();
    own.text = "Vehicle 21-1 ready at the alternate position.";
    own.summary = "21-1 ready.";
    own.audience = d.positions[static_cast<std::size_t>(bty)].unit;
    own.need_level = 3;
    const int fo = net.Report(own, v1);
    SC_EXPECT(fo >= 0 && net.Facts()[static_cast<std::size_t>(fo)].origin == "veh21_1_commander");
    SC_EXPECT(net.Report(own, v1_operator) < 0);  // not a player's post
    net.AdvanceTo(h0 + 0.5);
    PostView mine, his;
    SC_EXPECT(ReadPostView(PostViewJson(net, v1), mine) && ReadPostView(PostViewJson(net, bty), his));
    bool own_seen = false;
    for (const ViewItem& it : mine.items) own_seen = own_seen || (it.key == "report:v1:1" && it.own);
    SC_EXPECT(own_seen);
    for (const ViewItem& it : his.items) SC_EXPECT(it.key != "report:v1:1");
    // Played by one player instead, the vehicle commander passes the warning to his crew himself.
    InfoNet single;
    single.Init(d, 1u, start);
    const int fs = single.AddFact(Warning(h0));
    single.AdvanceTo(h0 + 0.1);
    SC_EXPECT(single.Tell({single.Now(), bty, v1, fs, "radio_voice"}));
    single.AdvanceTo(h0 + 0.3);
    SC_EXPECT(single.Known(v1_operator, fs) != nullptr);
}

SC_TEST("command: the district drawn (P7): the posts, offices and villages as basic models, the people where they are at the hour") {
    const ChainData d = LoadChain();
    scenery::ModelLibrary lib;
    std::string e;
    SC_EXPECT(scenery::LoadModelLibrary(kData + "/scenery/basic_models.json", lib, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    const std::vector<DistrictPiece> places = DistrictPlaces(d, lib);
    std::map<std::string, int> per_place;
    for (const DistrictPiece& p : places) {
        SC_EXPECT(lib.Find(p.model) != nullptr && p.position.empty());
        ++per_place[p.place];
    }
    std::printf("    %d pieces at %d places\n", static_cast<int>(places.size()), static_cast<int>(per_place.size()));
    // The regiment's command post its own layout, an office its kind's; the battery (a theatre object), the town and the
    // woods drawn otherwise or not at all.
    SC_EXPECT(per_place["rgt_cp"] == 10 && per_place["kesmark_police"] == 2 && per_place["vrbice"] == 9);
    SC_EXPECT(per_place.count("bty21_cp") == 0 && per_place.count("kesmark") == 0 && per_place.count("lazne_woods") == 0);
    bool tent_at_rgt = false;
    for (const DistrictPiece& p : places) {
        if (p.place != "rgt_cp") continue;
        const Vec3 c = d.FindPlace("rgt_cp")->pos;
        tent_at_rgt = tent_at_rgt || (p.model == "cp_tent" && std::hypot(p.pos.x - c.x, p.pos.y - c.y) < 1.0);
        SC_EXPECT(std::hypot(p.pos.x - c.x, p.pos.y - c.y) < 41.0);
    }
    SC_EXPECT(tent_at_rgt);
    // The people: the regiment's duty officer at his post; the vehicles' crews in their vehicles, not drawn; the regiment's
    // commander drawn at his post only while he is there.
    const double start = StartHour();
    InfoNet net;
    net.Init(d, 1u, start);
    const std::vector<DistrictPiece> people = DistrictPeople(net, At(9.0, start) + 0.05);
    auto where = [](const std::vector<DistrictPiece>& v, const std::string& post) {
        for (const DistrictPiece& p : v) {
            if (p.position == post) return p.place;
        }
        return std::string();
    };
    SC_EXPECT(where(people, "rgt_cp_duty") == "rgt_cp");
    SC_EXPECT(where(people, "veh21_1_operator").empty() && where(people, "bty21_commander").empty());
    SC_EXPECT(where(people, "rgt_commander").empty());  // visiting the battery at this hour
    bool home_sometime = false;
    for (int hour = 0; hour < 24 && !home_sometime; ++hour) home_sometime = where(DistrictPeople(net, At(hour, start)), "rgt_commander") == "rgt_cp";
    SC_EXPECT(home_sometime);
    for (const DistrictPiece& p : people) SC_EXPECT(p.model == "soldier" && lib.Find(p.model) != nullptr);
}

SC_TEST("command: posts change hands (P8): a casualty's deputy acts for him, a replacement has only the papers, the player's career, the alternate command post") {
    const ChainData d = LoadChain();
    const double start = StartHour();
    const double h0 = At(10.0, start);
    const int bty = d.PositionIndex("bty21_commander");
    const int dep = d.PositionIndex("veh21_2_commander");
    const int v1 = d.PositionIndex("veh21_1_commander");
    const int v1_operator = d.PositionIndex("veh21_1_operator");
    const int duty = d.PositionIndex("rgt_cp_duty");
    SC_EXPECT(d.positions[static_cast<std::size_t>(bty)].deputy == "veh21_2_commander");

    // The battery commander killed before the raid: his deputy, vehicle 21-2's commander, has the warning from the
    // regiment in his place and warns the vehicles; vehicle 21-1's commander his crew.
    InfoNet net;
    SC_EXPECT(net.Init(d, 1u, start));
    net.SetPlayer("veh21_1_operator");
    PostChange killed;
    killed.position = "bty21_commander";
    killed.h = h0 - 1.0;
    killed.vacant = true;
    killed.why = "killed";
    SC_EXPECT(net.ChangeHands(killed));
    SC_EXPECT(net.VacantAt(bty, h0) && !net.VacantAt(bty, h0 - 1.5));
    SC_EXPECT(net.ActingFor(bty, h0) == dep && net.ActingFor(bty, h0 - 1.5) == bty && net.ActingFor(v1, h0) == v1);
    const int fi = net.AddFact(Warning(h0));
    net.AdvanceTo(h0 + 0.5);
    SC_EXPECT(net.Known(bty, fi) == nullptr);
    const Delivery* acting = net.Known(dep, fi);
    SC_EXPECT(acting != nullptr && acting->from == duty);
    if (acting != nullptr) std::printf("    the deputy warned after %.1f min: %s\n", (acting->h - h0) * 60.0, net.Describe(*acting).c_str());
    const Delivery* veh = net.Known(v1, fi);
    SC_EXPECT(veh != nullptr && veh->from == dep);
    SC_EXPECT(net.Known(v1_operator, fi) != nullptr);
    std::string why;
    SC_EXPECT(!net.ChangeHands({"nobody", h0, true, Holder(), "", "killed"}, &why) && why == "no such post: nobody");
    SC_EXPECT(!net.ChangeHands({"bty21_commander", h0, false, Holder(), "", "promoted"}, &why));

    // Killed while the warning was on its way to him: it reaches his deputy when it would have reached him.
    InfoNet before;
    before.Init(d, 1u, start);
    before.SetPlayer("veh21_1_operator");
    const int fb = before.AddFact(Warning(h0));
    before.AdvanceTo(h0 + 0.5);
    const Delivery* was = before.Known(bty, fb);
    SC_EXPECT(was != nullptr);
    if (was != nullptr) {
        const double arrive = was->h;
        PostChange late = killed;
        late.h = arrive - 0.5 / 60.0;
        SC_EXPECT(before.ChangeHands(late));
        const Delivery* in_place = before.Known(dep, fb);
        SC_EXPECT(before.Known(bty, fb) == nullptr && in_place != nullptr);
        if (in_place != nullptr) SC_EXPECT(std::fabs(in_place->h - arrive) < 1e-9 && in_place->from == duty);
        SC_EXPECT(before.Known(v1, fb) != nullptr && before.Known(v1_operator, fb) != nullptr);
    }

    // A replacement in the evening: what was said to the man before him is gone with him, the papers are on the desk.
    InfoNet repl;
    repl.Init(d, 1u, start);
    repl.SetPlayer("veh21_1_driver");
    const int fw = repl.AddFact(Warning(h0));
    Fact note;
    note.key = "test:note";
    note.kind = FactKind::EnemyOfficer;
    note.h = h0;
    note.origin = "army_intel";
    note.text = "The officer planning the raids opens with a feint from another direction.";
    note.summary = "Army intelligence has a note on the officer planning the raids.";
    note.audience = "rgt_commander";
    note.need_level = 4;
    note.document = true;
    const int fn = repl.AddFact(note);
    repl.AdvanceTo(At(23.0, start));
    SC_EXPECT(repl.Known(bty, fw) != nullptr && repl.Known(bty, fn) != nullptr && repl.Known(bty, fn)->full);
    PostChange fresh;
    fresh.position = "bty21_commander";
    fresh.h = At(22.0, start);
    fresh.holder = {"Novák", "Captain", "class3"};
    fresh.why = "replaced";
    SC_EXPECT(repl.ChangeHands(fresh));
    SC_EXPECT(repl.HolderAt(bty, repl.Now()).name == "Novák" && repl.HolderAt(bty, h0).name == d.positions[static_cast<std::size_t>(bty)].holder.name);
    SC_EXPECT(std::fabs(repl.HeldSince(bty, repl.Now()) - At(22.0, start)) < 1e-9 && !repl.VacantAt(bty, repl.Now()));
    SC_EXPECT(repl.Known(bty, fw) == nullptr && repl.Known(bty, fn) != nullptr);
    SC_EXPECT(repl.KnownBy(bty).size() == 1);

    // The player's career: vehicle 21-1's commander takes over the battery at noon. He keeps what he knew; what was said
    // to the battery commander before him is not his; his old post is vacant and his deputy acts for it.
    InfoNet career;
    career.Init(d, 1u, start);
    career.SetPlayer("veh21_1_commander");
    const int fc = career.AddFact(Warning(h0));
    career.AdvanceTo(h0 + 0.5);
    SC_EXPECT(career.Known(v1, fc) != nullptr);
    // He tells his crew himself (a player's character passes nothing on by himself).
    SC_EXPECT(career.Tell({career.Now(), v1, v1_operator, fc, "in_person"}));
    SC_EXPECT(career.PlayerTo("bty21_commander", At(12.0, start)));
    SC_EXPECT(!career.PlayerTo("bty21_commander", At(12.0, start), &why) && why == "he holds it already");
    career.AdvanceTo(At(13.0, start));
    SC_EXPECT(career.Player() == bty && career.PlayerAt(h0) == v1 && career.Career().size() == 2);
    SC_EXPECT(career.VacantAt(v1, career.Now()) && career.ActingFor(v1, career.Now()) == d.PositionIndex("veh21_1_deputy"));
    SC_EXPECT(career.HolderAt(bty, career.Now()).name == d.positions[static_cast<std::size_t>(v1)].holder.name);
    SC_EXPECT(career.Known(bty, fc) == nullptr && career.Known(v1, fc) == nullptr);  // the post's: not his any more
    const std::vector<Delivery> mine = career.KnownByPlayer();
    bool keeps = false;
    for (const Delivery& k : mine) keeps = keeps || (k.fact == fc && k.position == v1);
    SC_EXPECT(keeps);
    // The telling he made from his old post still stands; from his new one he can tell what he knew.
    SC_EXPECT(career.Known(v1_operator, fc) != nullptr);
    SC_EXPECT(career.Tell({career.Now(), bty, dep, fc, "radio_voice"}));
    // Saved with the campaign and put back: the same career, the same posts, the same tellings.
    InfoNet back;
    back.Init(d, 1u, start);
    back.SetPlayer("veh21_1_commander");
    back.AddFact(Warning(h0));
    back.AdvanceTo(career.Now());
    std::string e;
    SC_EXPECT(LoadPlayerInputs(back, SavePlayerInputs(career), &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(e.empty() && back.Player() == bty && back.PlayerAt(h0) == v1 && back.Changes().size() == career.Changes().size());
    SC_EXPECT(back.Tellings().size() == 2 && back.KnownByPlayer().size() == career.KnownByPlayer().size());
    SC_EXPECT(back.VacantAt(v1, back.Now()) && back.Known(v1_operator, fc) != nullptr);

    // The alternate command post: the battery commander's post moves to vehicle 21-2's position.
    InfoNet alt;
    alt.Init(d, 1u, start);
    SC_EXPECT(!alt.ToAlternate("veh21_2_commander", h0, "moved", &why) && why == "it has no alternate command post");
    SC_EXPECT(alt.ToAlternate("bty21_commander", h0, "the PU-12M lost"));
    const int home = d.PlaceIndex("bty21_cp");
    const int v2_place = d.PlaceIndex("bty21_v2");
    InfoNet plain;
    plain.Init(d, 1u, start);
    int moved = 0;
    for (int hour = 0; hour < 24; ++hour) {
        const double h = h0 + hour;
        if (plain.PlaceOf(bty, h) != home) continue;  // at a briefing or a visit
        SC_EXPECT(alt.PlaceOf(bty, h) == v2_place);
        ++moved;
    }
    SC_EXPECT(moved > 0 && alt.PlaceOf(bty, h0 - 0.5) == plain.PlaceOf(bty, h0 - 0.5));
}

SC_TEST("command: a battery order as a record (G10.1): «К маршу!» on the battery net, 21-3's set off, said again, then a runner in his route's time; nobody acts on what he did not hear") {
    const ChainData d = LoadChain();
    const double start = StartHour();
    const double h0 = At(10.0, start);
    const double s = 1.0 / 3600.0;
    const std::vector<std::string> vehicles = {"veh21_1_commander", "veh21_2_commander", "veh21_3_commander", "veh21_4_commander"};
    for (const std::string& v : vehicles) SC_EXPECT(d.FindPosition(v) != nullptr);
    OrderBook book;
    BatteryOrderRecord o;
    o.id = "bty21:march:1";
    o.giver = "bty21_commander";
    o.kind = "march";
    o.words = "«К маршу!»";
    o.given_h = h0;
    o.mean = "radio_voice";
    const int k = book.Give(o, vehicles);
    // Three sets on the net: each commander hears it, repeats it back, and his vehicle starts.
    for (int i : {0, 1, 3}) {
        const std::string& v = vehicles[static_cast<std::size_t>(i)];
        SC_EXPECT(book.Mark(k, v, OrderStep::Reached, h0 + 2 * s, "radio_voice") && book.Mark(k, v, OrderStep::Heard, h0 + 2 * s));
        SC_EXPECT(book.Mark(k, v, OrderStep::RepeatedBack, h0 + (8 + i) * s) && book.Mark(k, v, OrderStep::CarryingOut, h0 + 20 * s));
    }
    // 21-3's set is off: nothing reaches him, and nothing he does on it can be recorded.
    std::string why;
    SC_EXPECT(!book.Mark(k, "veh21_3_commander", OrderStep::CarryingOut, h0 + 20 * s, "", &why) && why == "not heard yet");
    SC_EXPECT(book.Due(k, h0 + 20 * s, ChaseRule()).empty());
    // The giver's silence rule: twice more on the net, then the telephone, then a runner.
    const ChaseRule rule;
    std::vector<std::string> tried;
    double now = h0;
    for (int round = 0; round < 6; ++round) {
        now += 31 * s;
        for (const Chase& c : book.Due(k, now, rule)) {
            SC_EXPECT(c.post == "veh21_3_commander");
            book.Resend(k, c.post, c.h, c.mean);
            tried.push_back(c.mean);
        }
    }
    SC_EXPECT(tried.size() == 4 && tried[0] == "radio_voice" && tried[1] == "radio_voice" && tried[2] == "field_telephone" && tried[3] == "messenger");
    // The runner goes on foot from the PU-12M to 21-3's position by the route service's way.
    std::string e, text;
    CountryDef country;
    MapDef district;
    nav::MobilityLibrary mobility;
    SC_EXPECT(simcore::detail::ReadTextFile(kData + "/maps/pannonia.json", text) && LoadCountryDefJson(text, country, &e));
    SC_EXPECT(simcore::detail::ReadTextFile(kData + "/maps/kesmark_range.json", text) && LoadMapDefJson(text, district, &e));
    SC_EXPECT(nav::LoadMobility(kData, "vehicles/mobility.json", mobility, &e));
    const nav::RoadGraph roads = nav::RoadGraph::ForCountry(country, &district);
    nav::RouteRequest q;
    q.from = {d.FindPlace("bty21_cp")->pos.x, d.FindPlace("bty21_cp")->pos.y};
    q.to = {d.FindPlace("bty21_v3")->pos.x, d.FindPlace("bty21_v3")->pos.y};
    q.mobility = mobility.Find("foot");
    const nav::Route run = roads.Find(q);
    SC_EXPECT(run.found && run.time_s > 60.0);
    const OrderLeg* l3 = book.Orders()[static_cast<std::size_t>(k)].For("veh21_3_commander");
    SC_EXPECT(l3 != nullptr && l3->tries == 5 && l3->last_mean == "messenger");
    if (l3 == nullptr) return;
    const double arrives = l3->last_sent_h + run.time_s / 3600.0;
    SC_EXPECT(book.Mark(k, "veh21_3_commander", OrderStep::Reached, arrives, "messenger") && book.Mark(k, "veh21_3_commander", OrderStep::Heard, arrives));
    SC_EXPECT(book.Mark(k, "veh21_3_commander", OrderStep::RepeatedBack, arrives) && book.Mark(k, "veh21_3_commander", OrderStep::CarryingOut, arrives + 20 * s));
    SC_EXPECT(book.Due(k, arrives + 1.0, rule).empty());
    // A step before the one it needs is refused; so is "done" before carrying out.
    SC_EXPECT(!book.Mark(k, "veh21_1_commander", OrderStep::Reported, h0 + 0.2, "", &why) && why == "not done yet");
    SC_EXPECT(!book.Mark(k, "veh21_2_commander", OrderStep::Done, h0, "", &why) && why == "before it was carrying out");
    SC_EXPECT(book.Cannot(k, "veh21_2_commander", h0 + 0.1, "the bridge is down"));
    // The debrief: every recipient's times, 21-3's sendings and his runner.
    const std::vector<std::string> lines = book.Debrief(k, start);
    for (const std::string& line : lines) std::printf("    %s\n", line.c_str());
    SC_EXPECT(lines.size() == 5 && lines[3].find("by messenger") != std::string::npos && lines[2].find("cannot (the bridge is down)") != std::string::npos);
}

SC_TEST("command: the army's air defence staff's rule on the enemy's air front (F2.5): enough signs deeper than it expects, in a few days, then its estimate") {
    AirFrontWatchRules r;  // the data's values
    {
        airwar::TuningTable t;
        r = AirFrontWatchRules::From(t);
    }
    SC_EXPECT(r.cas_reach_m == 40000.0 && r.helo_reach_m == 25000.0 && r.margin_m == 5000.0 && r.band_m == 40000.0 && r.count == 3 && r.days == 2 &&
              r.delay_h == 6.0);
    AirFrontWatch w;
    w.Init(2, r);
    const auto sign = [](FrontSign k, int sector, double depth_km, double h) {
        FrontSignReport s;
        s.sign = k;
        s.sector = sector;
        s.depth_m = depth_km * 1000.0;
        s.seen_h = h - 0.5;
        s.known_h = h;
        return s;
    };
    AirFrontConclusion k;
    // Within the usual reach (close support to 40 km, helicopters to 25, and the margin): nothing counts.
    SC_EXPECT(!w.Take(sign(FrontSign::Attack, 0, 44.0, 1.0), k) && !w.Take(sign(FrontSign::Helicopters, 0, 29.0, 2.0), k) && w.Counted(0) == 0);
    // Deeper: counted, but two are not enough; one in the other sector does not add to them.
    SC_EXPECT(!w.Take(sign(FrontSign::Attack, 0, 52.0, 3.0), k) && !w.Take(sign(FrontSign::Helicopters, 0, 41.0, 4.0), k));
    SC_EXPECT(!w.Take(sign(FrontSign::Intercept, 1, 70.0, 5.0), k) && w.Counted(0) == 2 && w.Counted(1) == 1);
    // A third outside the window (two campaign days): the first two have gone from it.
    SC_EXPECT(!w.Take(sign(FrontSign::Troops, 0, 60.0, 50.0), k) && w.Counted(0) == 1);
    // Two more within it: the conclusion, the third deepest's excess rounded down (52 - 40 = 12 -> 10 km), stated after the
    // staff's work; the window starts again from the new estimate.
    SC_EXPECT(!w.Take(sign(FrontSign::Attack, 0, 63.0, 55.0), k));
    SC_EXPECT(w.Take(sign(FrontSign::Helicopters, 0, 37.0, 60.0), k));
    std::printf("    sector 0: the air front estimated at %.0f km (was %.0f), stated at hour %.1f on %d signs\n", k.depth_m / 1000.0, k.from_m / 1000.0, k.h,
                k.signs);
    SC_EXPECT(k.sector == 0 && k.depth_m == 10000.0 && k.from_m == 0.0 && k.decided_h == 60.0 && k.h == 66.0 && k.signs == 3 && k.first_day == 2);
    SC_EXPECT(w.Believed(0) == 10000.0 && w.Believed(1) == 0.0 && w.Counted(0) == 0 && w.Conclusions().size() == 1);
    // Against the new estimate a sign at 52 km is no longer deep (52 - 40 - 10 < 5); three at 60-70 km move it again.
    SC_EXPECT(!w.Take(sign(FrontSign::Attack, 0, 52.0, 61.0), k) && w.Counted(0) == 0);
    SC_EXPECT(!w.Take(sign(FrontSign::Attack, 0, 66.0, 62.0), k) && !w.Take(sign(FrontSign::Attack, 0, 70.0, 63.0), k));
    SC_EXPECT(w.Take(sign(FrontSign::Attack, 0, 57.0, 64.0), k) && k.from_m == 10000.0 && k.depth_m == 15000.0);
    // Far deeper than any step of his front (beyond the band, 40 km past the estimate): interdiction, not counted.
    SC_EXPECT(!w.Take(sign(FrontSign::Intercept, 1, 120.0, 65.0), k) && w.Counted(1) == 0);  // (and the day-0 sign has left the window)
    // A sign the staff has not had yet is not taken.
    FrontSignReport early = sign(FrontSign::Attack, 1, 90.0, 70.0);
    early.known_h = -1.0;
    SC_EXPECT(!w.Take(early, k) && w.Counted(1) == 0);
}

namespace {

struct WatchRun {
    std::vector<AirFrontConclusion> conclusions;
    std::vector<FrontSignReport> signs;
    std::vector<double> enemy_depths;
    int enemy_moves = 0;
    double rgt_knows_h = -1.0;    // when the regiment's commander learned the first conclusion
    double rgt_estimate_m = 0.0;  // the estimate he holds at the end, its sector's
    bool through = true;          // every sign reached the staff through the pipeline, after it was seen
};

// The Pact's war on the district for three days at air_front.mode 1, the feed pumped after every step (or once at the end,
// as after a load); `tune` changes the game values.
WatchRun RunWatch(const airwar::AirWarData& base, const std::map<std::string, double>& tune, bool loose, bool once) {
    airwar::AirWarData d = base;
    d.tuning.values["air_front.mode"] = 1.0;
    if (loose) {  // F2.3's test's guards off: the enemy's sectors open on the district
        d.tuning.values["air_front.min_baseline"] = 0.0;
        d.faction.air_front.min_reward = 0.0;
    }
    for (const auto& kv : tune) d.tuning.values[kv.first] = kv.second;
    airwar::AirCampaign c;
    c.Init(d, 21);
    InfoNet net;
    net.Init(LoadChain(), 21, d.theatre.start_hour);
    CampaignFeed feed;
    feed.Bind(&net);
    while (c.Now() < 72.0) {
        if (c.Advance(72.0)) c.SkipPlayerMission();
        if (!once) feed.Pump(c);
    }
    if (once) feed.Pump(c);
    WatchRun out;
    out.conclusions = feed.Watch().Conclusions();
    out.signs = feed.Signs();
    out.enemy_depths = c.AirFrontDepths();
    for (const airwar::JournalEntry& j : c.Journal()) out.enemy_moves += j.kind == "air_front" ? 1 : 0;
    const int staff = net.Data().PositionIndex("army_ad_staff");
    for (const FrontSignReport& s : out.signs) {
        const Delivery* k = net.Known(staff, net.FactIndex(s.key));
        if (s.known_h >= 0.0) out.through = out.through && k != nullptr && k->h == s.known_h && k->h >= s.seen_h && k->from >= 0;
    }
    if (!out.conclusions.empty()) {
        const int rgt = net.Data().PositionIndex("rgt_commander");
        const Delivery* k = net.Known(rgt, net.FactIndex(out.conclusions[0].key));
        out.rgt_knows_h = k != nullptr ? k->h : -1.0;
        out.rgt_estimate_m = feed.AirFrontKnownBy(rgt, out.conclusions[0].sector);
    }
    return out;
}

}  // namespace

SC_TEST("command: our side learns the enemy's air front (F2.5) only from signs that reached the army's air defence staff, never from his depths") {
    airwar::AirWarData aw;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", aw, &e));
    const auto deepest = [](const WatchRun& r) {
        double x = 0.0;
        for (double v : r.enemy_depths) x = std::max(x, v);
        return x;
    };
    const auto print = [](const char* what, const WatchRun& r, double deep) {
        std::printf("    %s: the enemy's air front %d moves, deepest %.0f km; %zu signs to the staff, %zu conclusions\n", what, r.enemy_moves, deep / 1000.0,
                    r.signs.size(), r.conclusions.size());
        for (const AirFrontConclusion& k : r.conclusions) {
            std::printf("      sector %d: %.0f km from %.0f, decided at %.1f h, stated at %.1f h on %d signs\n", k.sector, k.depth_m / 1000.0, k.from_m / 1000.0,
                        k.decided_h, k.h, k.signs);
        }
    };
    // The enemy's front moves (the guards off) but the staff expects his close support and helicopters 500 km deep: no sign
    // is deep enough to send, so nothing is concluded although the depths are 20 km and more.
    const WatchRun blind = RunWatch(aw, {{"air_front.watch_cas_reach_m", 500000.0}, {"air_front.watch_helo_reach_m", 500000.0}}, true, false);
    print("the front moved, nothing seen", blind, deepest(blind));
    SC_EXPECT(blind.enemy_moves > 0 && deepest(blind) >= 20000.0);
    SC_EXPECT(blind.signs.empty() && blind.conclusions.empty());
    // The enemy's front never moves (the district's guard holds) but the staff expects his close support and helicopters
    // at the line itself and is told of one sign each: the ordinary raids mislead it into a conclusion while every depth is 0.
    const std::map<std::string, double> credulous = {{"air_front.watch_cas_reach_m", 0.0}, {"air_front.watch_helo_reach_m", 0.0}, {"air_front.watch_count", 1.0}};
    const WatchRun misled = RunWatch(aw, credulous, false, false);
    print("nothing moved, ordinary raids taken for signs", misled, deepest(misled));
    SC_EXPECT(misled.enemy_moves == 0 && deepest(misled) == 0.0);
    SC_EXPECT(!misled.signs.empty() && !misled.conclusions.empty() && misled.through);
    // Every conclusion is the staff's: after the signs that reached it, the working delay after the last; through the
    // pipeline to the regiment's commander no earlier than it was stated, who then holds its estimate.
    for (const AirFrontConclusion& k : misled.conclusions) {
        SC_EXPECT(k.h == k.decided_h + 6.0 && k.signs >= 1 && !k.key.empty());
        int before = 0;
        for (const FrontSignReport& s : misled.signs) before += s.known_h >= 0.0 && s.known_h <= k.decided_h && s.sector == k.sector ? 1 : 0;
        SC_EXPECT(before >= k.signs);
    }
    std::printf("    the regiment's commander learned the first at %.1f h (stated %.1f h); he holds %.0f km for its sector\n", misled.rgt_knows_h,
                misled.conclusions[0].h, misled.rgt_estimate_m / 1000.0);
    SC_EXPECT(misled.rgt_knows_h >= misled.conclusions[0].h && misled.rgt_estimate_m > 0.0);
    // The same pumped once at the end (a loaded campaign): the same signs taken in the same order, the same conclusions.
    const WatchRun loaded = RunWatch(aw, credulous, false, true);
    SC_EXPECT(loaded.conclusions.size() == misled.conclusions.size() && loaded.signs.size() == misled.signs.size());
    for (std::size_t i = 0; i < loaded.conclusions.size() && i < misled.conclusions.size(); ++i) {
        SC_EXPECT(loaded.conclusions[i].h == misled.conclusions[i].h && loaded.conclusions[i].depth_m == misled.conclusions[i].depth_m &&
                  loaded.conclusions[i].sector == misled.conclusions[i].sector && loaded.conclusions[i].key == misled.conclusions[i].key);
    }
    // At the data's values with the enemy's front moving: whatever the evidence says, each conclusion only after its count.
    const WatchRun moved = RunWatch(aw, {}, true, false);
    print("the front moved, the data's rule", moved, deepest(moved));
    for (const AirFrontConclusion& k : moved.conclusions) SC_EXPECT(k.signs >= 3);
    SC_EXPECT(moved.through);
    // At air_front.mode 0 (the data's) the watch is off: no sign, no conclusion, no fact of its kinds.
    airwar::AirCampaign c;
    c.Init(aw, 21);
    InfoNet net;
    net.Init(LoadChain(), 21, aw.theatre.start_hour);
    CampaignFeed feed;
    feed.Bind(&net);
    while (c.Now() < 24.0) {
        if (c.Advance(24.0)) c.SkipPlayerMission();
        feed.Pump(c);
    }
    SC_EXPECT(!feed.WatchOn() && feed.Signs().empty());
    for (const Fact& f : net.Facts()) SC_EXPECT(f.kind != FactKind::AirFront && f.key.compare(0, 12, "front_watch:") != 0);
}
