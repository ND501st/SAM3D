// The fronts' sectors and the line of contact (track F, F2.1; simcore/air_war/front_line.hpp, docs/fronts/02_AIR_FRONT.md §2).
// Generic geometry for the hand cases; the theatres' own fronts only for the identity with the base line.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/air_war/front_line.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

Vec3 Km(double e, double n) { return Vec3{e * 1000.0, n * 1000.0, 0.0}; }

// A bent line: east 40 km, then 50 km north-east (direction 0.6, 0.8); the enemy's bases north, so our side is south of
// the first leg and south-east of the second. 90 km long.
FrontDef BentFront(bool sectors) {
    FrontDef f;
    f.faction = "test";
    f.name = "Test front";
    f.line = {Km(0, 0), Km(40, 0), Km(70, 40)};
    f.bases = Km(20, 100);
    if (sectors) {
        SectorDef a;
        a.id = "west";
        a.name = "West";
        a.from_m = 0.0;
        a.to_m = 35000.0;
        SectorDef b = a;
        b.id = "east";
        b.name = "East";
        b.from_m = 35000.0;
        b.to_m = 70000.0;  // short of the line's end: the rest is the last sector's
        f.sectors = {a, b};
    }
    return f;
}

AirWarData LoadOn(const std::string& theatre, const std::string& faction) {
    AirWarData d;
    std::string e;
    if (!LoadAirWarData(kData, "air_war/" + theatre, "air_war/faction_" + faction + ".json", d, &e)) std::printf("    %s\n", e.c_str());
    return d;
}

std::uint64_t JournalHash(const AirCampaign& c) {
    std::uint64_t h = 1469598103934665603ull;
    for (const auto& j : c.Journal()) {
        for (char ch : j.text) {
            h ^= static_cast<unsigned char>(ch);
            h *= 1099511628211ull;
        }
        h ^= 0x0a;
        h *= 1099511628211ull;
    }
    return h;
}

}  // namespace

SC_TEST("front line (F2.1): distances, sides and places along a bent line by hand") {
    LineOfContact l;
    l.Build(BentFront(false), 5000.0);
    SC_EXPECT_NEAR(l.Length(), 90000.0, 1e-6);
    SC_EXPECT(l.StationCount() == 19);  // 0, 5, ... 90 km
    SC_EXPECT_NEAR(l.StationAlong(18), 90000.0, 1e-9);
    // 7 km south of the first leg, 10 km along it: our side.
    FrontPlace p = l.Place(Km(10, -7));
    SC_EXPECT_NEAR(p.along_m, 10000.0, 1e-6);
    SC_EXPECT_NEAR(p.distance_m, 7000.0, 1e-6);
    SC_EXPECT_NEAR(p.side, 1.0, 1e-12);
    SC_EXPECT_NEAR(l.DistanceTo(Km(10, -7)), 7000.0, 1e-6);
    // 3 km north of it: the enemy's side.
    SC_EXPECT_NEAR(l.DistanceTo(Km(10, 3)), -3000.0, 1e-6);
    SC_EXPECT_NEAR(l.AbsDistance(Km(10, 3)), 3000.0, 1e-6);
    // Half way up the second leg (55, 20) and 10 km out along its right normal (0.8, -0.6): (63, 14).
    p = l.Place(Km(63, 14));
    SC_EXPECT_NEAR(p.along_m, 65000.0, 1e-6);
    SC_EXPECT_NEAR(l.DistanceTo(Km(63, 14)), 10000.0, 1e-6);
    SC_EXPECT_NEAR(l.DistanceTo(Km(47, 26)), -10000.0, 1e-6);  // the same the other way
    const Vec3 n = l.InwardNormal(65000.0);
    SC_EXPECT_NEAR(n.x, 0.8, 1e-12);
    SC_EXPECT_NEAR(n.y, -0.6, 1e-12);
    // Before the line's start: the nearest point is its first, the distance to it.
    p = l.Place(Km(-3, -4));
    SC_EXPECT_NEAR(p.along_m, 0.0, 1e-9);
    SC_EXPECT_NEAR(p.distance_m, 5000.0, 1e-6);
    // The bases on the other side make the other side ours.
    FrontDef other = BentFront(false);
    other.bases = Km(20, -100);
    LineOfContact o;
    o.Build(other, 5000.0);
    SC_EXPECT_NEAR(o.DistanceTo(Km(10, -7)), -7000.0, 1e-6);
    SC_EXPECT_NEAR(o.InwardNormal(10000.0).y, 1.0, 1e-12);
}

SC_TEST("front line (F2.1): sectors by distance along the line, the ends to the end sectors; none is one sector") {
    LineOfContact l;
    l.Build(BentFront(true), 5000.0);
    SC_EXPECT(l.SectorCount() == 2);
    SC_EXPECT(l.SectorOf(Km(10, -7)) == 0);
    SC_EXPECT(l.SectorAt(34999.0) == 0);
    SC_EXPECT(l.SectorAt(35000.0) == 1);
    SC_EXPECT(l.SectorOf(Km(63, 14)) == 1);   // 65 km along
    SC_EXPECT(l.SectorOf(Km(90, 60)) == 1);   // past the line's end and the last sector's written end
    SC_EXPECT(l.SectorOf(Km(-20, -5)) == 0);  // before its start
    SC_EXPECT(l.SectorIndex("east") == 1 && l.SectorIndex("north") == -1);
    SC_EXPECT(l.SectorFrom(0) < -1e17 && l.SectorTo(1) > 1e17);
    SC_EXPECT_NEAR(l.SectorTo(0), 35000.0, 1e-9);
    LineOfContact one;
    one.Build(BentFront(false), 5000.0);
    SC_EXPECT(one.SectorCount() == 1);
    SC_EXPECT(one.Sector(0).id == "front" && one.Sector(0).name == "Test front");
    SC_EXPECT_NEAR(one.Sector(0).to_m, 90000.0, 1e-6);
    SC_EXPECT(one.SectorOf(Km(63, 14)) == 0);
}

SC_TEST("front line (F2.1): offsets move the line of contact into our country, linear between stations") {
    LineOfContact l;
    l.Build(BentFront(false), 5000.0);
    SC_EXPECT(!l.Moved());
    SC_EXPECT_NEAR(l.Offset(10000.0), 0.0, 1e-12);
    const unsigned v0 = l.Version();
    l.SetStationOffset(2, 4000.0);  // 10 km along
    SC_EXPECT(l.Moved() && l.Version() != v0);
    SC_EXPECT_NEAR(l.Offset(10000.0), 4000.0, 1e-9);
    SC_EXPECT_NEAR(l.Offset(7500.0), 2000.0, 1e-9);
    SC_EXPECT_NEAR(l.Offset(12500.0), 2000.0, 1e-9);
    SC_EXPECT_NEAR(l.Offset(20000.0), 0.0, 1e-12);
    // 7 km south of the base line at 10 km along is 3 km beyond the line of contact; the contact point is on it.
    SC_EXPECT_NEAR(l.DistanceTo(Km(10, -7)), 3000.0, 1e-6);
    const Vec3 cp = l.ContactPoint(2);
    SC_EXPECT_NEAR(cp.x, 10000.0, 1e-6);
    SC_EXPECT_NEAR(cp.y, -4000.0, 1e-6);
    SC_EXPECT_NEAR(l.DistanceTo(cp), 0.0, 1e-6);
    SC_EXPECT_NEAR(l.AbsDistance(Km(10, -1)), 3000.0, 1e-6);  // 1 km south: 3 km short of it, on the enemy's side now
    SC_EXPECT_NEAR(l.DistanceTo(Km(10, -1)), -3000.0, 1e-6);
    // The last interval is short (none here): the line's end station still interpolates.
    l.SetStationOffset(18, 1000.0);
    SC_EXPECT_NEAR(l.Offset(87500.0), 500.0, 1e-9);
    SC_EXPECT_NEAR(l.Offset(95000.0), 1000.0, 1e-9);
    l.SetStationOffset(2, 0.0);
    l.SetStationOffset(18, 0.0);
    SC_EXPECT(!l.Moved());
    // A line not a multiple of the spacing: a last, shorter interval.
    FrontDef f = BentFront(false);
    f.line = {Km(0, 0), Km(12, 0)};
    LineOfContact s;
    s.Build(f, 5000.0);
    SC_EXPECT(s.StationCount() == 4);  // 0, 5, 10, 12 km
    SC_EXPECT_NEAR(s.StationAlong(3), 12000.0, 1e-9);
    s.SetStationOffset(3, 2000.0);
    SC_EXPECT_NEAR(s.Offset(11000.0), 1000.0, 1e-9);
}

SC_TEST("front line (F2.1): at zero offsets every distance is the base line's to the bit, on every theatre front") {
    LineOfContact l;
    l.Build(BentFront(true), 5000.0);
    int same = 0, total = 0;
    for (int i = -40; i <= 120; i += 7) {
        for (int j = -60; j <= 80; j += 9) {
            const Vec3 p{i * 1013.7, j * 977.3, 0.0};
            const double d = DistanceToLine(BentFront(true).line, p);
            same += (l.AbsDistance(p) == d && std::fabs(l.DistanceTo(p)) == d && l.Place(p).distance_m == d) ? 1 : 0;
            ++total;
        }
    }
    SC_EXPECT(same == total);
    for (const char* theatre : {"theatre_pannonia.json", "theatre_pannonia_nation.json"}) {
        TheatreDef t;
        std::string e;
        SC_EXPECT(LoadTheatreFile(kData + "/air_war/" + theatre, t, &e));
        for (const FrontDef& f : t.fronts) {
            LineOfContact fl;
            fl.Build(f, 5000.0);
            int bad = 0;
            for (const TheatreObject& o : t.objects) bad += fl.AbsDistance(o.pos) == DistanceToLine(f.line, o.pos) ? 0 : 1;
            for (const EntryPoint& en : f.entries) bad += fl.AbsDistance(en.pos) == DistanceToLine(f.line, en.pos) ? 0 : 1;
            SC_EXPECT(bad == 0);
            // Our objects are on our side of every front's line.
            SC_EXPECT(fl.DistanceTo(f.bases) < 0.0);
        }
    }
}

SC_TEST("front line (F2.1): the campaign measures from its line of contact; the air front's measures change nothing") {
    const AirWarData d = LoadOn("theatre_pannonia.json", "warsaw_pact");
    AirCampaign c;
    SC_EXPECT(c.Init(d, 21));
    const FrontDef* f = d.theatre.FrontFor(d.faction.front);
    SC_EXPECT(f != nullptr);
    SC_EXPECT(c.FrontLine().SectorCount() == 1);
    int bad = 0;
    for (const TheatreObject& o : d.theatre.objects) bad += c.FrontLine().AbsDistance(o.pos) == DistanceToLine(f->line, o.pos) ? 0 : 1;
    SC_EXPECT(bad == 0);
    SC_EXPECT(!c.AirFrontOn() && c.SectorCovers().empty());
    // Two days of the war with air_front.mode 0 and 1: the same journal, packages and truth, to the bit.
    AirWarData on = d;
    on.tuning.values["air_front.mode"] = 1.0;
    AirCampaign a, b;
    a.Init(d, 21);
    b.Init(on, 21);
    for (AirCampaign* x : {&a, &b}) {
        while (x->Now() < 48.0) {
            if (x->Advance(48.0)) x->SkipPlayerMission();
        }
    }
    SC_EXPECT(JournalHash(a) == JournalHash(b));
    SC_EXPECT(a.Journal().size() == b.Journal().size() && a.Packages().size() == b.Packages().size());
    SC_EXPECT(b.AirFrontOn() && b.SectorCovers().size() == 1);
}

SC_TEST("front line (F2.1): a theatre front's sectors load in order, contiguous from 0; a gap, an overlap or a repeat fails") {
    std::ifstream in(kData + "/air_war/theatre_pannonia.json", std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::string key = "\"faction\": \"warsaw_pact\",";
    SC_EXPECT(text.find(key) != std::string::npos);
    auto with = [&](const std::string& sectors) {
        std::string t = text;
        t.replace(t.find(key), key.size(), key + " \"sectors\": " + sectors + ",");
        return t;
    };
    TheatreDef t;
    std::string e;
    SC_EXPECT(LoadTheatreJson(with(R"([{"id": "west", "name": "West", "from_km": 0, "to_km": 60},
                                       {"id": "east", "name": "East", "from_km": 60, "to_km": 230, "notes": "to the end"}])"),
                              t, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    const FrontDef* f = t.FrontFor("warsaw_pact");
    SC_EXPECT(f != nullptr && f->sectors.size() == 2);
    if (f != nullptr && f->sectors.size() == 2) {
        SC_EXPECT(f->sectors[1].id == "east");
        SC_EXPECT_NEAR(f->sectors[1].from_m, 60000.0, 1e-9);
        SC_EXPECT_NEAR(f->sectors[1].to_m, 230000.0, 1e-9);
    }
    SC_EXPECT(t.FrontFor("nato") != nullptr && t.FrontFor("nato")->sectors.empty());
    TheatreDef bad;
    SC_EXPECT(!LoadTheatreJson(with(R"([{"id": "a", "name": "A", "from_km": 5, "to_km": 60}])"), bad, &e));
    SC_EXPECT(!LoadTheatreJson(with(R"([{"id": "a", "name": "A", "from_km": 0, "to_km": 60}, {"id": "b", "name": "B", "from_km": 70, "to_km": 90}])"), bad, &e));
    SC_EXPECT(!LoadTheatreJson(with(R"([{"id": "a", "name": "A", "from_km": 0, "to_km": 60}, {"id": "a", "name": "B", "from_km": 60, "to_km": 90}])"), bad, &e));
    SC_EXPECT(!LoadTheatreJson(with(R"([{"id": "a", "name": "A", "from_km": 0, "to_km": 0}])"), bad, &e));
    SC_EXPECT(!LoadTheatreJson(with(R"([])"), bad, &e));
}
