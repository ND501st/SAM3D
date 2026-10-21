// Our side's chain of command and the information pipeline (simcore/command/, data/command/, docs/command/).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/command.hpp"
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
    airwar::AirCampaign c;
    SC_EXPECT(c.Init(aw, 3u));
    c.Posture().relocate_every_h = 20.0;
    const ChainData d = LoadChain();
    InfoNet net;
    SC_EXPECT(net.Init(d, 3u, aw.theatre.start_hour));
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
    std::printf("    two days against NATO: %zu facts in the pipeline, the battery commander knows %zu (%d firsthand, %d learned more than an hour late); "
                "raids seen by the warning net %zu, known to the battery commander %d; raid warnings in time %d of %d\n",
                net.Facts().size(), known.size(), firsthand, late, c.ObservedRaids().size(), raids_known, warned_in_time, warnings);
    SC_EXPECT(!known.empty() && known.size() < net.Facts().size());
    SC_EXPECT(raids_known < static_cast<int>(c.ObservedRaids().size()));
    SC_EXPECT(late > 0);
    SC_EXPECT(warnings == 0 || warned_in_time * 2 >= warnings);
    // Every delivery says who told them and how (or that they saw it).
    for (const Delivery& k : known) SC_EXPECT(!net.Describe(k).empty());
}
