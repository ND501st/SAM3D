// Roadmap B3, what radiates and what a pilot's receiver makes of it (simcore/em_link.hpp RwrPictureOf, SetOsaSite;
// data/em/pact_1985.json; docs/air_battle/03_PERCEPTION.md section 3). Every number here is a generic placeholder.
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "simcore/air_war/data.hpp"
#include "simcore/air_war/perception_feed.hpp"
#include "simcore/em_link.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

struct RwrBench {
    EmLinkDef def;
    FlatTerrain ground{0.0};
    std::unique_ptr<EmSensorLink> link;
    Vec3 unit{0.0, 0.0, 3.0};
    Vec3 target{0.0, 9000.0, 1500.0};
    bool ok = false;

    RwrBench(const std::string& rwr, const std::vector<std::string>& libraries) {
        ok = LoadEmLinkDef(kData, "em/osa_ak.json", "osa_ak_soc", "osa_ak_ssc", def, nullptr, libraries);
        link = std::make_unique<EmSensorLink>(GeoPoint::FromDegrees(46.9, 19.7, 0.0), &ground, 5u, def, unit);
        ok = ok && link->Ok();
        link->SetTargetRwr("t", rwr);
        link->SetSearch(true, false);
    }
    void Tick(double seconds, bool lock, bool link_radiating) {
        for (double t = 0.0; t < seconds; t += 0.1) {
            link->SyncTarget("t", true, target, Vec3{0.0, 0.0, 0.0}, 5.0, false);
            if (lock) {
                link->SetTrack(true);
                const Vec3 rel = target - unit;
                link->Designate(0.0, RadToDeg(ElevationOf(rel)), rel.Length());
            }
            if (link_radiating) link->SetCommandLink(1, 2, 0, target);
            link->Advance(0.1);
        }
    }
};

const EmSensorLink::RwrContactView* Find(const std::vector<EmSensorLink::RwrContactView>& v, int state) {
    for (const EmSensorLink::RwrContactView& c : v) {
        if (c.state == state) return &c;
    }
    return nullptr;
}

}  // namespace

SC_TEST("perception em: a NATO receiver hears the Osa's search, lock and launch; the Pact's own library names them too") {
    RwrBench nato("nato_alr46_class_rwr", {"em/nato_1985.json", "em/generic.json"});
    SC_EXPECT(nato.ok);
    if (!nato.ok) return;
    nato.Tick(6.0, false, false);
    {
        const auto v = nato.link->RwrPictureOf("t");
        const auto* search = Find(v, 0);
        SC_EXPECT(search != nullptr && search->known);
        if (search != nullptr) SC_EXPECT(search->name.find("SA-8 search") != std::string::npos);
        SC_EXPECT(Find(v, 1) == nullptr && Find(v, 2) == nullptr);  // radiating a search: no lock, no launch
    }
    nato.Tick(4.0, true, false);
    {
        const auto v = nato.link->RwrPictureOf("t");
        const auto* lock = Find(v, 1);
        SC_EXPECT(lock != nullptr);
        if (lock != nullptr) {
            SC_EXPECT(lock->name.find("SA-8 track") != std::string::npos);
            // The bearing is the line to the radar, with the receiver's error: south of the target.
            const double truth = AzimuthOf(nato.unit - nato.target);
            SC_EXPECT(std::fabs(std::remainder(lock->bearing_rad - truth, kTwoPi)) < DegToRad(35.0));
        }
    }
    nato.Tick(3.0, true, true);
    SC_EXPECT(Find(nato.link->RwrPictureOf("t"), 2) != nullptr);  // the command link: a launch
    // The Pact's SPO-10 class (the data's receiver, not generic_rwr): the same radars, named by its own library.
    RwrBench pact("pact_spo10_class_rwr", {"em/pact_1985.json", "em/generic.json"});
    SC_EXPECT(pact.ok);
    if (!pact.ok) return;
    pact.Tick(10.0, true, true);
    const auto pv = pact.link->RwrPictureOf("t");
    SC_EXPECT(Find(pv, 1) != nullptr && Find(pv, 2) != nullptr);  // the library knows the link; the DISPLAY hides the launch (rwr_sets)
    if (const auto* plock = Find(pv, 1)) SC_EXPECT(plock->known);
}

SC_TEST("perception em: a neighbour battery's radars now warn a receiver: its search, its lock on what it engages, its command link") {
    RwrBench nato("nato_alr46_class_rwr", {"em/nato_1985.json", "em/generic.json"});
    SC_EXPECT(nato.ok);
    if (!nato.ok) return;
    // Our own radars silent: only the neighbour, 9 km east of the target's ground track, radiates.
    nato.link->SetSearch(false, false);
    const Vec3 site{9000.0, 9000.0, 0.0};
    for (double t = 0.0; t < 6.0; t += 0.1) {
        nato.link->SyncTarget("t", true, nato.target, Vec3{0.0, 0.0, 0.0}, 5.0, false);
        nato.link->SetOsaSite("n:1", site, true, false, std::string(), false);
        nato.link->Advance(0.1);
    }
    SC_EXPECT(nato.link->SiteCount() == 1 && nato.link->SiteSearchRadiating("n:1") && !nato.link->SiteTrackRadiating("n:1"));
    {
        const auto v = nato.link->RwrPictureOf("t");
        const auto* s = Find(v, 0);
        SC_EXPECT(s != nullptr);
        if (s != nullptr) SC_EXPECT(std::fabs(std::remainder(s->bearing_rad - AzimuthOf(site - nato.target), kTwoPi)) < DegToRad(40.0));
    }
    for (double t = 0.0; t < 5.0; t += 0.1) {  // it engages: the tracking radar on the target
        nato.link->SyncTarget("t", true, nato.target, Vec3{0.0, 0.0, 0.0}, 5.0, false);
        nato.link->SetOsaSite("n:1", site, true, true, "t", false);
        nato.link->Advance(0.1);
    }
    SC_EXPECT(Find(nato.link->RwrPictureOf("t"), 1) != nullptr);
    for (double t = 0.0; t < 3.0; t += 0.1) {  // and launches: the command link
        nato.link->SyncTarget("t", true, nato.target, Vec3{0.0, 0.0, 0.0}, 5.0, false);
        nato.link->SetOsaSite("n:1", site, true, true, "t", true);
        nato.link->Advance(0.1);
    }
    SC_EXPECT(Find(nato.link->RwrPictureOf("t"), 2) != nullptr);
    // Off the air again: it falls silent.
    for (double t = 0.0; t < 2.0; t += 0.1) {
        nato.link->SyncTarget("t", true, nato.target, Vec3{0.0, 0.0, 0.0}, 5.0, false);
        nato.link->SetOsaSite("n:1", site, false, false, std::string(), false);
        nato.link->Advance(0.1);
    }
    SC_EXPECT(!nato.link->SiteSearchRadiating("n:1"));
}

SC_TEST("perception em: no type of any faction carries generic_rwr; the MLD has its SPO-15L from its year; seats are read") {
    for (const char* f : {"nato", "warsaw_pact", "neutral", "rebels"}) {
        AirWarData d;
        std::string e;
        SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", std::string("air_war/faction_") + f + ".json", d, &e));
        for (const AircraftType& t : d.faction.aircraft) {
            SC_EXPECT(t.rwr_em != "generic_rwr");
            for (const auto& y : t.rwr_by_year) SC_EXPECT(y.first != "generic_rwr");
        }
    }
    AirWarData pact;
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", pact, &e));
    bool saw_mld = false, saw_su24_two_seat = false;
    for (const AircraftType& t : pact.faction.aircraft) {
        if (t.id == "mig23mld") {
            saw_mld = true;
            // Since B8 the MLD is its own type from 1984 with the SPO-15L, beside the MiG-23ML with the SPO-10, so the
            // receiver changes with the type, not the year (rwr_by_year stays for a type refitted in service).
            SC_EXPECT(t.RwrEm(1985) == "pact_spo15l_class_rwr" && t.in_service_year == 1984 && !t.InService(1983));
        } else if (!t.rwr_em.empty()) {
            SC_EXPECT(t.RwrEm(1985) == "pact_spo10_class_rwr");
        }
        if (t.id == "su24") saw_su24_two_seat = t.seats == 2;
    }
    SC_EXPECT(saw_mld && saw_su24_two_seat);
    // A placeholder receiver on a faction's type is refused.

    {
        std::string err;
        FactionDef f;
        // The faction file with its Su-24's receiver put back to the placeholder.
        std::string raw;
        if (!simcore::detail::ReadTextFile(kData + "/air_war/faction_warsaw_pact.json", raw)) return;
        const std::string from = "\"rwr_em\": \"pact_spo10_class_rwr\"";
        const std::size_t at = raw.find(from);
        SC_EXPECT(at != std::string::npos);
        if (at == std::string::npos) return;
        raw.replace(at, from.size(), "\"rwr_em\": \"generic_rwr\"");
        SC_EXPECT(!LoadFactionJson(raw, f, &err) && err.find("generic_rwr is a placeholder") != std::string::npos);
    }
}

SC_TEST("perception em: a feed's launch lays a sign and a trail that a pilot may see; a synthetic trail runs toward its target") {
    PerceptionData d;
    PerceptionFeed feed(d, 3);
    const Vec3 site{0.0, 0.0, 0.0};
    feed.Launch(1, "own", site, 0.0, false);
    feed.Launch(1, "n:1", Vec3{500.0, 0.0, 0.0}, 0.0, false);
    feed.Synthetic("n:1", 1, Vec3{500.0, 8000.0, 2000.0}, 20.0, 450.0);
    for (double t = 0.0; t <= 10.0; t += 0.5) {
        feed.TrailHead("own", 1, Vec3{0.0, 300.0 * t, 200.0 * t}, t);
        feed.Update(t);
    }
    std::size_t own_trail = 0, other_trail = 0;
    int own_sign = 0, signs = 0;
    for (const VisibleEvent& e : feed.hub.Events()) {
        if (e.kind == SightKind::Trail && e.owner == "own") own_trail = e.trail_pts.size();
        if (e.kind == SightKind::Trail && e.owner == "n:1") other_trail = e.trail_pts.size();
        if (e.kind == SightKind::LaunchSign) {
            ++signs;
            if (e.owner == "own") {
                own_sign = e.launch_id;
                SC_EXPECT_NEAR(e.until_s - e.start_s, d.launch_sign_life_s, 1e-9);
            } else {
                SC_EXPECT(e.launch_id == 0);  // only our own vehicle's launches are counted for the enemy's report
            }
        }
    }
    SC_EXPECT(own_trail >= 10 && other_trail >= 10 && own_sign == 1 && signs == 2);
    // By night the sign is a flash of a couple of seconds.
    PerceptionFeed night(d, 3);
    night.Launch(2, "own", site, 0.0, true);
    for (const VisibleEvent& e : night.hub.Events()) {
        if (e.kind == SightKind::LaunchSign) SC_EXPECT(e.until_s - e.start_s <= 2.5 && e.shows_at_night);
    }
    // Events are pruned long after they end.
    feed.Update(30.0);  // the synthetic trails have run out
    feed.Update(1000.0);
    SC_EXPECT(feed.hub.Events().empty());
}

SC_TEST("perception em: a 1985 war loads no sensor dated after 1985 or without a source; the receivers' displays name loaded receivers") {
    AirWarData d;
    std::string e;
    SC_EXPECT(LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", d, &e));
    SC_EXPECT(!d.sensors.sensors.empty());
    for (const SensorDef& s : d.sensors.sensors) {
        SC_EXPECT(s.in_service_year > 0 && s.in_service_year <= 1985);
        SC_EXPECT(!s.source.empty() && !s.confidence.empty());
    }
    // A sensor of a later year is out of a 1985 war.
    SensorDef later;
    later.in_service_year = 1991;
    SC_EXPECT(!later.InService(1985) && later.InService(1991));
    // Every display is of an RWR the EM libraries hold.
    PerceptionData p;
    SC_EXPECT(LoadPerceptionData(kData, p, &e));
    em::EmLibrary lib;
    SC_EXPECT(em::LoadEmLibraryFile(kData + "/em/nato_1985.json", lib).ok);
    SC_EXPECT(em::LoadEmLibraryFile(kData + "/em/pact_1985.json", lib).ok);
    for (const RwrSetDisplay& set : p.rwr_sets) SC_EXPECT(lib.Esm(set.em_esm) >= 0);
}

SC_TEST("perception em (S3): a fighter's radar is an emitter: its search sweeps past, its lock is steady and the target's receiver calls it") {
    RwrBench b("nato_alr46_class_rwr", {"em/nato_1985.json", "em/generic.json"});
    SC_EXPECT(b.ok);
    if (!b.ok) return;
    em::EmLibrary radars;
    SC_EXPECT(em::LoadEmLibraryFile(kData + "/em/fighter_radars_1985.json", radars).ok);
    SC_EXPECT(b.link->MergeLibrary(radars));
    const Vec3 mig{0.0, 30000.0, 3000.0};  // ours, nose toward the target from the north
    const auto tick = [&](double seconds) {
        for (double t = 0.0; t < seconds; t += 0.1) {
            b.link->SyncTarget("t", true, b.target, Vec3{0.0, 0.0, 0.0}, 5.0, false);
            b.link->SyncTarget("mig", true, mig, Vec3{0.0, -230.0, 0.0}, 3.0, true);
            b.link->PointFighterRadars();
            b.link->Advance(0.1);
        }
    };
    tick(0.5);
    SC_EXPECT(b.link->SetFighterRadar("mig", "sapfir_23ml", 1, ""));
    SC_EXPECT(b.link->FighterRadarEmitter("mig") != 0);
    tick(6.0);
    const EmSensorLink::RwrWarning searched = b.link->WarningOf("t");
    SC_EXPECT(b.link->SetFighterRadar("mig", "sapfir_23ml", 2, "t"));
    tick(4.0);
    const EmSensorLink::RwrWarning locked = b.link->WarningOf("t");
    std::printf("    searching: lock %d; locked: lock %d by %s\n", searched.lock ? 1 : 0, locked.lock ? 1 : 0, locked.lock_by.c_str());
    SC_EXPECT(!searched.lock);
    SC_EXPECT(locked.lock && locked.lock_by.find("fighter") != std::string::npos);
    SC_EXPECT(b.link->SetFighterRadar("mig", "sapfir_23ml", 0, ""));
    tick(2.0);
    SC_EXPECT(!b.link->WarningOf("t").lock);
    SC_EXPECT(!b.link->SetFighterRadar("nobody", "sapfir_23ml", 1, ""));
}
