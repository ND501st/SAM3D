// Roadmap E6, anti-radiation missiles in the EM layer (em/arm.hpp): the seeker hears the radar's sidelobes and homes;
// a radar that goes silent beats a missile without memory, and one with a memory unless it goes silent early; a decoy
// on the radar's own parameters inside the seeker's resolution draws the burst off. Geometry and ranges are generic
// test numbers; the missiles are the library's classes (data/em/nato_1985.json, estimates as marked there).
#include <cmath>
#include <cstdio>
#include <string>

#include "simcore/em/data.hpp"
#include "simcore/em/world.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::em;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

struct ArmRange {
    EmWorld world{GeoPoint{}, nullptr, 1};
    int radar_platform = 0;
    int radar = 0;
    int shooter = 0;
    int decoy_platform = 0;
    bool ok = false;

    explicit ArmRange(std::uint64_t seed, double decoy_east_m = -1.0) : world(GeoPoint{}, nullptr, seed) {
        EmLibrary lib;
        if (!LoadEmLibraryFile(kData + "/em/osa_ak.json", lib).ok || !LoadEmLibraryFile(kData + "/em/nato_1985.json", lib).ok) return;
        if (!world.SetLibrary(lib)) return;
        radar_platform = world.AddGroundPlatform("Osa", Side::Red, 0.0, 0.0, 5.0);
        radar = world.AddRadar(radar_platform, "osa_ak_soc", true);
        if (decoy_east_m >= 0.0) {
            decoy_platform = world.AddGroundPlatform("Decoy", Side::Red, decoy_east_m, 0.0, 5.0);
            world.AddDecoy(decoy_platform, radar, DecoyMode::Always);
        }
        Platform f;
        f.name = "Carrier";
        f.side = Side::Blue;
        f.kind = PlatformKind::FixedWing;
        f.position = {0.0, 15000.0, 300.0};
        f.velocity = {0.0, -250.0, 0.0};
        shooter = world.AddPlatform(f);
        ok = radar != 0;
    }

    // Fire at the radar, then step until it bursts; `silent_at_m`: the radar goes off when the missile is this close.
    const ArmState* Fire(const std::string& arm, double silent_at_m = -1.0) {
        const EmScene& s = world.Scene();
        const int def = s.lib.Arm(arm);
        if (def < 0) return nullptr;
        const int id = world.FireArm(shooter, arm, s.FindPlatform(radar_platform)->position, 7.0e9, 2800.0);
        for (int i = 0; i < 2000; ++i) {
            world.Step(0.1);
            const ArmState* a = world.Scene().FindArm(id);
            if (a == nullptr) return nullptr;
            const Platform* m = world.Scene().FindPlatform(a->platform_id);
            const Platform* r = world.Scene().FindPlatform(radar_platform);
            if (silent_at_m >= 0.0 && m != nullptr && r != nullptr && (m->position - r->position).Length() < silent_at_m) {
                world.Scene().FindRadar(radar)->on = false;
            }
            if (a->outcome != ArmOutcome::Flying) return a;
        }
        return world.Scene().FindArm(id);
    }
};

}  // namespace

SC_TEST("em arm: the library's ARM classes load with their bands, memory and ranges") {
    EmLibrary lib;
    const EmLoadResult r = LoadEmLibraryFile(kData + "/em/nato_1985.json", lib);
    SC_EXPECT(r.ok);
    const int shrike = lib.Arm("agm45_class_shrike");
    const int harm = lib.Arm("agm88a_class_harm");
    SC_EXPECT(shrike >= 0 && harm >= 0 && lib.Arm("kh58_class") >= 0);
    if (shrike < 0 || harm < 0) return;
    SC_EXPECT(!lib.arms[static_cast<std::size_t>(shrike)].memory);
    SC_EXPECT(lib.arms[static_cast<std::size_t>(harm)].memory);
    // The Shrike's broadest head ends in the I band: the Osa's tracking radar (J band) is out of its reach.
    SC_EXPECT(lib.arms[static_cast<std::size_t>(shrike)].freq_max_hz < 14.5e9);
    SC_EXPECT(lib.arms[static_cast<std::size_t>(harm)].freq_max_hz > 14.5e9);
}

SC_TEST("em arm: the seeker's one-way power, its tuning and the memory point match hand calculations") {
    // One-way radar equation: 100 kW EIRP at 7 GHz heard 10 km away (prototype/tests/test_em.py checks the same).
    const double lambda = kSpeedOfLight / 7.0e9;
    const double hand = 1.0e5 * lambda * lambda / (16.0 * kPi * kPi * 1.0e8);
    SC_EXPECT_NEAR(OneWayReceivedPowerW(1.0e5, 0.0, 7.0e9, 10000.0, 0.0), hand, hand * 1e-12);
    SC_EXPECT_NEAR(hand, 1.1617e-8, 1.2e-11);
    // Tuned +-10 % around the signal heard at launch.
    ArmDef d;
    d.freq_min_hz = 2e9;
    d.freq_max_hz = 20e9;
    d.tune_fraction = 0.1;
    d.prf_fraction = 0.1;
    ArmState a;
    a.tuned_freq_hz = 7.0e9;
    a.tuned_prf_hz = 2800.0;
    EmitterState e;
    e.def.waveform.prf_hz = 2800.0;
    e.freq_hz = 7.3e9;
    SC_EXPECT(ArmTunedTo(d, a, e));
    e.freq_hz = 14.5e9;
    SC_EXPECT(!ArmTunedTo(d, a, e));
    // Lofted 10 degrees above the line to a point level with it.
    const Vec3 dir = ArmLaunchDirection({0.0, -10000.0, 0.0}, {0.0, 0.0, 0.0}, DegToRad(10.0));
    SC_EXPECT_NEAR(dir.x, 0.0, 1e-12);
    SC_EXPECT_NEAR(RadToDeg(ElevationOf(dir)), 10.0, 1e-9);
    // The memory point from 1000 m up, the line of sight 10 degrees down, over flat ground at the origin's height:
    // 1000 / tan 10 = 5671 m, and the Earth's drop over that distance (d^2 / 2R) carries it about 14 m further.
    EmWorld w(GeoPoint{}, nullptr, 1);
    Vec3 p;
    SC_EXPECT(ArmGroundPoint(w.Scene(), {0.0, 0.0, 1000.0}, DirectionFromAzEl(0.0, DegToRad(-10.0)), 100000.0, p));
    const double flat = 1000.0 / std::tan(DegToRad(10.0));
    const double drop = flat * flat / (2.0 * 6371000.0) / std::tan(DegToRad(10.0));
    SC_EXPECT_NEAR(p.HorizontalLength(), flat + drop, 1.5);
    SC_EXPECT(!ArmGroundPoint(w.Scene(), {0.0, 0.0, 1000.0}, DirectionFromAzEl(0.0, 0.1), 20000.0, p));
}

SC_TEST("em arm: a radar left on is hit, by either class") {
    int shrike_hits = 0, harm_hits = 0;
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        ArmRange a(seed), b(seed);
        SC_EXPECT(a.ok && b.ok);
        const ArmState* s = a.Fire("agm45_class_shrike");
        const ArmState* h = b.Fire("agm88a_class_harm");
        if (s != nullptr && s->outcome == ArmOutcome::Hit && s->hit_platform == a.radar_platform) ++shrike_hits;
        if (h != nullptr && h->outcome == ArmOutcome::Hit && h->hit_platform == b.radar_platform) ++harm_hits;
        if (s != nullptr && s->outcome != ArmOutcome::Hit) std::printf("    shrike seed %d missed by %.0f m\n", static_cast<int>(seed), s->miss_m);
    }
    SC_EXPECT(shrike_hits >= 3);
    SC_EXPECT(harm_hits >= 3);
    ArmRange c(1);
    c.Fire("agm45_class_shrike");
    SC_EXPECT(!c.world.Scene().FindPlatform(c.radar_platform)->alive);  // out of action
    SC_EXPECT(!c.world.Scene().FindRadar(c.radar)->on);
}

SC_TEST("em arm: silent in time, the radar beats a Shrike; a HARM remembers it unless it went silent early") {
    int shrike_hits = 0, harm_late_hits = 0, harm_early_hits = 0;
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        ArmRange a(seed), b(seed), c(seed);
        const ArmState* s = a.Fire("agm45_class_shrike", 4000.0);   // off with the missile 4 km out: it falls
        const ArmState* late = b.Fire("agm88a_class_harm", 1500.0);  // off 2 s before: it flies on to the place
        const ArmState* early = c.Fire("agm88a_class_harm", 11000.0);
        if (s != nullptr && s->outcome == ArmOutcome::Hit) ++shrike_hits;
        if (late != nullptr && late->outcome == ArmOutcome::Hit) ++harm_late_hits;
        if (early != nullptr && early->outcome == ArmOutcome::Hit) ++harm_early_hits;
        if (early != nullptr) std::printf("    harm off at 11 km, seed %d: burst %.0f m away\n", static_cast<int>(seed), early->miss_m);
    }
    SC_EXPECT(shrike_hits == 0);
    SC_EXPECT(harm_late_hits >= 3);
    SC_EXPECT(harm_early_hits <= 1);
}

SC_TEST("em arm: a decoy on the radar's parameters 150 m away draws the burst off the radar") {
    int radar_hits = 0;
    for (std::uint64_t seed = 1; seed <= 4; ++seed) {
        ArmRange a(seed, 150.0);
        const ArmState* s = a.Fire("agm45_class_shrike");
        SC_EXPECT(s != nullptr && s->outcome != ArmOutcome::Flying);
        if (s != nullptr && s->hit_platform == a.radar_platform) ++radar_hits;
        if (s != nullptr) {
            const Vec3 radar = a.world.Scene().FindPlatform(a.radar_platform)->position;
            std::printf("    decoy seed %d: burst %.0f m from the radar\n", static_cast<int>(seed), (s->burst_point - radar).Length());
        }
    }
    SC_EXPECT(radar_hits <= 1);
}

SC_TEST("em arm: the defenders' radar sees the ARM as a small fast echo") {
    ArmRange a(2);
    const EmScene& s = a.world.Scene();
    const int def = s.lib.Arm("agm88a_class_harm");
    const Vec3 radar = s.FindPlatform(a.radar_platform)->position;
    const Vec3 dir = ArmLaunchDirection(s.FindPlatform(a.shooter)->position, radar, 0.0);
    const int id = a.world.FireArm(a.shooter, "agm88a_class_harm", radar, 7.0e9, 2800.0, &dir);
    SC_EXPECT(def >= 0 && id != 0);
    bool fast = false;
    for (int i = 0; i < 150 && !fast; ++i) {
        a.world.Step(0.1);
        for (const SensorTrack& t : a.world.Scene().FindRadar(a.radar)->tracker.Tracks()) {
            if (t.status == TrackStatus::Confirmed && t.Velocity().Length() > 450.0) fast = true;
        }
    }
    SC_EXPECT(fast);
}
