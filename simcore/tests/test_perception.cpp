// Roadmap B3: what a pilot sees and hears (simcore/air_war/perception.hpp, docs/air_battle/03_PERCEPTION.md section 7).
// The eye law against NWC TP 5709's example, the ground law against TP 6005's points, the eye's ranges for a trail, an
// aircraft and a contrail, the night, a ridge and a cloud deck, the warning receivers' displays, and the draws' keys.
// Every number here is a generic placeholder, not data of a real aircraft.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/air_war/perception.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::airwar;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

class FlatTerrain : public Terrain {
public:
    double HeightAt(double, double) const override { return 0.0; }
};

// A ridge across the track: 400 m high, 600 m wide, centred 1.5 km north of the origin.
class RidgeTerrain : public Terrain {
public:
    double HeightAt(double, double north) const override { return std::fabs(north - 1500.0) < 300.0 ? 400.0 : 0.0; }
};

struct World {
    FlatTerrain flat;
    RidgeTerrain ridge;
    LocalFrame frame;
    EnvIn env;
    World() {
        env.terrain = &flat;
        env.frame = &frame;
        env.light.sun_elevation_deg = 35.0;
        env.light.sun_azimuth_deg = 180.0;
        env.light.night_share = 0.0;
        env.light.daylight = true;
        env.visibility_m = 10000.0;
    }
    void Night() {
        env.light.sun_elevation_deg = -30.0;
        env.light.night_share = 1.0;
        env.light.daylight = false;
        env.light.dark = true;
    }
};

ObserverIn Observer(const std::string& id, const Vec3& pos, const Vec3& vel) {
    ObserverIn o;
    o.id = id;
    o.key = ObjectKeyOf(id);
    o.side = 0;
    o.pos = pos;
    o.vel = vel;
    o.alt_m = pos.z;
    o.agl_m = pos.z;
    return o;
}

// Ticks an observer that holds still for `seconds`, every 0.5 s.
void Hold(PerceptionHub& hub, const ObserverIn& ob, const EnvIn& env, double from_s, double seconds, const std::vector<AirObject>& air = {},
          const std::vector<RwrHeard>& rwr = {}, std::vector<Sighting>* fresh = nullptr) {
    for (double t = from_s; t < from_s + seconds; t += 0.5) {
        if (hub.Due(ob.key, t)) hub.Tick(ob, env, t, air, rwr, fresh);
    }
}

bool Holds(const PerceptionHub& hub, const ObserverIn& ob, SightKind k, double now) { return hub.Holds(ob.key, k, now, 1e9); }

}  // namespace

// ---------------------------------------------------------------------------------------------- the laws

SC_TEST("perception: the eye law reproduces NWC TP 5709's example, halves for the empty field and carries the field factors") {
    const EyeData d;
    // 55 ft2 at 30 %, no haze, the factors off: 6.5 nmi (TP 5709's example).
    const double area_m2 = 55.0 * 0.09290304;
    const double r = EyeRangeM(d, 1.0, 30.0, area_m2, 1e12, 1.0, false);
    SC_EXPECT_NEAR(r / 1852.0, 6.5, 6.5 * 0.02);
    // Empty-field myopia halves it.
    const double half = EyeRangeM(d, 1.0, 30.0, area_m2, 1e12, 1.0, true);
    SC_EXPECT_NEAR(half / r, 0.5, 0.005);
    // The formula inverted: the threshold at that range is the contrast.
    SC_EXPECT_NEAR(EyeThresholdPct(d, 1.0, r, area_m2), 30.0, 0.05);
    // The untrained, unknown-location case: 1.64 x 1.31 x 1.19 x 2.00 = 5.12 (rounded) of threshold, with the ordinary factor off.
    EyeData bare = d;
    bare.factor_ordinary = 1.0;
    SC_EXPECT_NEAR(EyeFactor(bare, false, 0.0), 5.12, 0.02);
    // A trained pilot cued to the place has none of the last two.
    SC_EXPECT_NEAR(EyeFactor(bare, true, 1.0), 1.64 * 1.19, 1e-9);
    // Haze shortens it; a thinner threshold (a larger factor) shortens it.
    SC_EXPECT(EyeRangeM(d, 1.0, 30.0, area_m2, 10000.0, 1.0, false) < r);
    SC_EXPECT(EyeRangeM(d, 2.0, 30.0, area_m2, 1e12, 1.0, false) < r);
    // A cloud between lets so little through that nothing is seen.
    SC_EXPECT(EyeRangeM(d, 1.0, 30.0, area_m2, 40000.0, 0.02, false) < 0.5 * EyeRangeM(d, 1.0, 30.0, area_m2, 40000.0, 1.0, false));
    // The glimpse lobe falls to zero at the threshold and a cue shrinks the search.
    SC_EXPECT(GlimpseLobeSr(d, 1.0) == 0.0 && GlimpseLobeSr(d, 3.0) > GlimpseLobeSr(d, 1.5));
    SC_EXPECT(GlimpseChance(d, 4.0, true) > 3.0 * GlimpseChance(d, 4.0, false));
}

SC_TEST("perception: the ground law gives 0.2, 0.5 and 0.8 of P_max at 0.625, 0.5 and 0.375 R*, never more than P_max") {
    const GroundData g;
    const double rstar = 10000.0;
    for (const double pmax : {0.25, 0.5, 1.0}) {
        SC_EXPECT_NEAR(GroundAcquireP(g, pmax, 6250.0, rstar), 0.2 * pmax, 1e-9);
        SC_EXPECT_NEAR(GroundAcquireP(g, pmax, 5000.0, rstar), 0.5 * pmax, 1e-9);
        SC_EXPECT_NEAR(GroundAcquireP(g, pmax, 3750.0, rstar), 0.8 * pmax, 1e-9);
        SC_EXPECT(GroundAcquireP(g, pmax, 7600.0, rstar) == 0.0);
        double last = 0.0;
        for (double r = 9000.0; r > 100.0; r -= 50.0) {
            const double p = GroundAcquireP(g, pmax, r, rstar);
            SC_EXPECT(p <= pmax + 1e-12 && p + 1e-12 >= last);  // never more than P_max, and rising as it closes
            last = p;
        }
        SC_EXPECT_NEAR(GroundAcquireP(g, pmax, 1000.0, rstar), pmax, 1e-9);
    }
    // TP 6005's table.
    SC_EXPECT_NEAR(GroundPmax(g, Pattern::Large, GroundContrast::High), 1.0, 1e-9);
    SC_EXPECT_NEAR(GroundPmax(g, Pattern::Medium, GroundContrast::Medium), 0.56, 1e-9);
    SC_EXPECT_NEAR(GroundPmax(g, Pattern::Small, GroundContrast::Low), 0.25, 1e-9);
}

SC_TEST("perception: the same geometry gives the same acquisition at 350 and 550 knots") {
    // The chance does not depend on speed (jones_atg_1974): the pass's draw decides, and only where the observer is.
    const World w;
    const Vec3 site{0.0, 0.0, 0.0};
    int same = 0, n = 0;
    for (std::uint64_t seed = 1; seed <= 60; ++seed) {
        bool got[2] = {false, false};
        const double speeds[2] = {350.0 * 0.5144, 550.0 * 0.5144};
        for (int k = 0; k < 2; ++k) {
            PerceptionHub hub(PerceptionData(), seed);
            hub.AddGround("osa_battery_silent", site, "site");
            ObserverIn ob = Observer("a", Vec3{0.0, -30000.0, 150.0}, Vec3{0.0, speeds[k], 0.0});
            for (double t = 0.0; t < 30000.0 / speeds[k] + 10.0; t += 0.25) {
                ob.pos = Vec3{0.0, -30000.0 + speeds[k] * t, 150.0};
                if (hub.Due(ob.key, t)) hub.Tick(ob, w.env, t, {}, {}, nullptr);
            }
            got[k] = Holds(hub, ob, SightKind::GroundObject, 1e9);
        }
        ++n;
        if (got[0] == got[1]) ++same;
    }
    SC_EXPECT(same >= n - 3);  // the same draw; the ticks' phase can move the first ranges a little at the margin
}

// ---------------------------------------------------------------------------------------------- a battery on a pass

SC_TEST("perception: a silent camouflaged battery is found at most at its P_max; once it fires its launch area is found") {
    const World w;
    const double v = 450.0 * 0.5144;
    int silent_found = 0, fired_found = 0, runs = 500;
    double worst_err = 0.0;
    for (int seed = 1; seed <= runs; ++seed) {
        for (int fired = 0; fired < 2; ++fired) {
            PerceptionHub hub(PerceptionData(), static_cast<std::uint64_t>(seed));
            hub.AddGround("osa_battery_silent", Vec3{0.0, 0.0, 0.0}, "site");
            std::uint64_t trail = 0;
            if (fired) {
                VisibleEvent e;
                e.kind = SightKind::LaunchSign;
                e.what = "SAM launch";
                e.pos = Vec3{0.0, 0.0, 0.0};
                e.start_s = 0.0;
                e.until_s = 1e9;
                e.ground = true;
                e.pattern = Pattern::Large;
                e.ground_contrast = GroundContrast::High;
                e.area_m2 = 400.0;
                e.launch_id = 1;
                hub.AddEvent(e);
                VisibleEvent t;
                t.kind = SightKind::Trail;
                t.what = "SAM trail";
                t.pos = Vec3{0.0, 0.0, 0.0};
                t.start_s = 0.0;
                t.until_s = 1e9;
                t.contrast_pct = 20.0;
                t.launch_id = 1;
                trail = hub.AddEvent(t);
            }
            ObserverIn ob = Observer("a", Vec3{0.0, -20000.0, 150.0}, Vec3{0.0, v, 0.0});
            for (double t = 0.0; t < 20000.0 / v; t += 0.25) {
                ob.pos = Vec3{0.0, -20000.0 + v * t, 150.0};
                if (trail != 0) {  // the missile climbs out of the site and toward him, 1 km a second
                    const double k = std::min(t, 8.0);
                    hub.ExtendTrail(trail, Vec3{0.0, 1000.0 * k * 0.1, 300.0 * k}, t, t < 8.0);
                }
                if (hub.Due(ob.key, t)) hub.Tick(ob, w.env, t, {}, {}, nullptr);
            }
            bool found = false;
            for (const Sighting& s : hub.Picture(ob.key)) {
                if (!fired && s.kind == SightKind::GroundObject) found = true;
                if (fired && (s.kind == SightKind::LaunchSign || s.kind == SightKind::Trail)) {
                    found = true;
                    worst_err = std::max(worst_err, (s.believed_enu - Vec3{0.0, 0.0, 0.0}).HorizontalLength());
                    // He believes it is somewhere around the site; the error is the eye's, not zero.
                }
            }
            (fired ? fired_found : silent_found) += found ? 1 : 0;
        }
    }
    const double silent_share = static_cast<double>(silent_found) / runs;
    const double fired_share = static_cast<double>(fired_found) / runs;
    std::printf("    silent battery found in %.1f %%, once firing in %.1f %% of %d passes (worst believed place %.0f m off)\n",
                100.0 * silent_share, 100.0 * fired_share, runs, worst_err);
    SC_EXPECT(silent_share <= 0.25 + 0.04);  // P_max of a small, low-contrast object
    SC_EXPECT(silent_share > 0.02);
    SC_EXPECT(fired_share > 0.5);
    SC_EXPECT(fired_share > 2.0 * silent_share);
    SC_EXPECT(worst_err < 6000.0);  // his estimate is off by the eye's errors (a trail's nearest point is not the site), not wild
}

SC_TEST("perception (B6 left, S4): a battery deployed and silent is found far less often than the same battery on the march") {
    // The runner names the battery's class by what it does (scenario_runner.hpp: osa_battery_silent halted,
    // column_moving on the road): small and low-contrast against a line feature of medium contrast (nwc_tp6005_1978's
    // P_max 0.25 against 0.75; agard_cp100_1972, movement and line features help). The same passes, the same seeds.
    const World w;
    const double v = 450.0 * 0.5144;
    int found[2] = {0, 0};
    const int runs = 400;
    for (int seed = 1; seed <= runs; ++seed) {
        for (int march = 0; march < 2; ++march) {
            PerceptionHub hub(PerceptionData(), static_cast<std::uint64_t>(seed));
            hub.AddGround(march ? "column_moving" : "osa_battery_silent", Vec3{0.0, 0.0, 0.0}, "battery");
            ObserverIn ob = Observer("a", Vec3{0.0, -20000.0, 150.0}, Vec3{0.0, v, 0.0});
            for (double t = 0.0; t < 20000.0 / v; t += 0.25) {
                ob.pos = Vec3{0.0, -20000.0 + v * t, 150.0};
                if (hub.Due(ob.key, t)) hub.Tick(ob, w.env, t, {}, {}, nullptr);
            }
            found[march] += Holds(hub, ob, SightKind::GroundObject, 1e9) ? 1 : 0;
        }
    }
    const double silent = static_cast<double>(found[0]) / runs, marching = static_cast<double>(found[1]) / runs;
    std::printf("    deployed and silent found in %.1f %%, on the march in %.1f %% of %d passes\n", 100.0 * silent, 100.0 * marching, runs);
    SC_EXPECT(marching > 2.0 * silent);
    SC_EXPECT(marching > 0.4);
}

// ---------------------------------------------------------------------------------------------- the eye's ranges

SC_TEST("perception: a small SAM's trail in day VMC is seen at 3-6 km; first sightings of a fighter fall in 1-5 nm; a contrail at up to 20 nm") {
    const PerceptionData d;
    // A trail: the data's width and length, in the reference haze (5 nmi visibility), cued and trained.
    const double trail_area = d.trail.width_m * d.trail.max_length_m;
    const double vmc = 5.0 * 1852.0;
    const double trail_cued = EyeRangeM(d.eye, EyeFactor(d.eye, true, 1.0), d.trail.contrast_pct, trail_area, vmc, 1.0, false);
    const double trail_uncued = EyeRangeM(d.eye, EyeFactor(d.eye, false, d.eye.default_skill), d.trail.contrast_pct, trail_area, vmc, 1.0, false);
    std::printf("    trail seen at %.1f km cued and trained, %.1f km uncued (5 nmi visibility)\n", trail_cued / 1000.0, trail_uncued / 1000.0);
    SC_EXPECT(trail_cued >= 3000.0 && trail_cued <= 6000.0);
    SC_EXPECT(trail_uncued >= 2000.0 && trail_uncued < trail_cued);
    // A fighter (a 4.5 m critical size: 10 m2 nose-on, 49 m2 from the side), unwarned, against the sky. The eye law's own
    // threshold lands at the low end of the period's band (typically 1-5 nm, WSEG 116): about 1 nm head-on to 3 nm from the
    // side in clear air, less in haze ("haze often caused misses"); the glimpses make the first sighting later still.
    const double size2 = 4.5 * 4.5;
    for (const double vis : {10000.0, 20000.0, 40000.0}) {
        const double nose = EyeRangeM(d.eye, EyeFactor(d.eye, false, d.eye.default_skill), d.eye.aircraft_contrast_pct, d.eye.area_nose_per_size2 * size2, vis, 1.0, true);
        const double beam = EyeRangeM(d.eye, EyeFactor(d.eye, false, d.eye.default_skill), d.eye.aircraft_contrast_pct, d.eye.area_beam_per_size2 * size2, vis, 1.0, true);
        std::printf("    fighter first seen at %.1f nm nose-on, %.1f nm from the side, visibility %.0f km\n", nose / 1852.0, beam / 1852.0, vis / 1000.0);
        SC_EXPECT(nose >= 0.7 * 1852.0 && nose <= 5.0 * 1852.0);
        SC_EXPECT(beam >= 0.9 * 1852.0 && beam <= 5.0 * 1852.0);
        SC_EXPECT(beam > nose);
    }
    // Recognised (its type) at about 5-6 km in clear air.
    const double rec = RecognitionRangeM(d.eye, 4.5);
    SC_EXPECT(rec >= 5000.0 && rec <= 6000.0);
}

SC_TEST("perception: a contrail is seen as far as the cap and no further") {
    World w;
    w.env.visibility_m = 100000.0;
    PerceptionData d;
    const double cap = d.eye.contrail_cap_m;
    for (const double range : {0.8 * cap, 1.15 * cap}) {
        PerceptionHub hub(d, 5);
        AirObject a;
        a.id = "target";
        a.key = ObjectKeyOf(a.id);
        a.side = 1;
        a.pos = Vec3{0.0, range, 9000.0};
        a.vel = Vec3{200.0, 0.0, 0.0};
        a.alt_m = 9000.0;
        a.contrail_area_m2 = d.eye.contrail_width_m * 3000.0;
        ObserverIn ob = Observer("watcher", Vec3{0.0, 0.0, 9000.0}, Vec3{0.0, 200.0, 0.0});
        hub.AddCue(ob.key, 0.0, 1e9);
        Hold(hub, ob, w.env, 0.0, 40.0, {a});
        const bool seen = Holds(hub, ob, SightKind::Contrail, 1e9);
        SC_EXPECT(seen == (range < cap));
    }
}

// ---------------------------------------------------------------------------------------------- night, ridge, cloud

SC_TEST("perception: by night no aircraft and no smoke is seen, but the tracers and a burst's flash are") {
    World w;
    w.Night();
    PerceptionHub hub(PerceptionData(), 11);
    AirObject a;
    a.id = "other";
    a.key = ObjectKeyOf("other");
    a.side = 1;
    a.pos = Vec3{0.0, 2000.0, 1000.0};
    a.vel = Vec3{0.0, -200.0, 0.0};
    a.alt_m = 1000.0;
    ObserverIn ob = Observer("watcher", Vec3{0.0, 0.0, 1000.0}, Vec3{0.0, 200.0, 0.0});
    hub.AddCue(ob.key, 0.0, 1e9);
    VisibleEvent smoke;  // a trail by night with the motor out: smoke not seen
    smoke.kind = SightKind::Trail;
    smoke.what = "SAM trail";
    smoke.pos = Vec3{0.0, 3000.0, 800.0};
    smoke.contrast_pct = 20.0;
    smoke.until_s = 1e9;
    smoke.width_m = 1.5;
    const std::uint64_t sm = hub.AddEvent(smoke);
    hub.ExtendTrail(sm, Vec3{0.0, 3000.0, 800.0}, 0.0, false);
    hub.ExtendTrail(sm, Vec3{0.0, 3300.0, 900.0}, 0.0, false);
    VisibleEvent tracer;  // tracers: a light
    tracer.kind = SightKind::Tracer;
    tracer.what = "tracer";
    tracer.pos = Vec3{0.0, 3000.0, 600.0};
    tracer.start_s = 0.0;
    tracer.until_s = 30.0;
    tracer.area_m2 = PerceptionData().tracer.area_m2;
    tracer.contrast_pct = 100.0;
    tracer.shows_at_night = true;
    tracer.night_range_m = 8000.0;
    hub.AddEvent(tracer);
    Hold(hub, ob, w.env, 0.0, 20.0, {a});
    SC_EXPECT(!Holds(hub, ob, SightKind::Aircraft, 1e9));
    SC_EXPECT(!Holds(hub, ob, SightKind::Trail, 1e9));
    SC_EXPECT(Holds(hub, ob, SightKind::Tracer, 1e9));
    // The same aircraft with its afterburner lit is seen.
    a.afterburner = true;
    PerceptionHub hub2(PerceptionData(), 11);
    hub2.AddCue(ob.key, 0.0, 1e9);
    Hold(hub2, ob, w.env, 0.0, 20.0, {a});
    SC_EXPECT(Holds(hub2, ob, SightKind::Aircraft, 1e9));
}

SC_TEST("perception: a launch behind a ridge is unseen until the trail climbs clear of it") {
    World w;
    w.env.terrain = &w.ridge;
    w.env.visibility_m = 20000.0;
    PerceptionData d;
    PerceptionHub hub(d, 3);
    // The observer is south of the ridge at 30 m; the site and the trail's foot are north of it.
    ObserverIn ob = Observer("low", Vec3{0.0, 0.0, 30.0}, Vec3{0.0, 100.0, 0.0});
    hub.AddCue(ob.key, 0.0, 1e9);
    VisibleEvent t;
    t.kind = SightKind::Trail;
    t.what = "SAM trail";
    t.start_s = 0.0;
    t.contrast_pct = 20.0;
    t.hang_s = 60.0;
    const std::uint64_t id = hub.AddEvent(t);
    // The foot and the first stretch are low and behind the ridge.
    hub.ExtendTrail(id, Vec3{0.0, 2500.0, 20.0}, 0.0, true);
    hub.ExtendTrail(id, Vec3{0.0, 2650.0, 100.0}, 1.0, true);
    hub.ExtendTrail(id, Vec3{0.0, 2800.0, 200.0}, 2.0, true);
    Hold(hub, ob, w.env, 2.0, 4.0);
    SC_EXPECT(!Holds(hub, ob, SightKind::Trail, 1e9));
    // It climbs clear (above the line over the ridge).
    for (int i = 0; i < 6; ++i) hub.ExtendTrail(id, Vec3{0.0, 3000.0 + 200.0 * i, 300.0 + 250.0 * i}, 4.0 + i, true);
    Hold(hub, ob, w.env, 6.0, 20.0);
    SC_EXPECT(Holds(hub, ob, SightKind::Trail, 1e9));
}

SC_TEST("perception: a pilot above an overcast sees a missile only once it leaves the cloud") {
    WeatherPreset p;
    p.surface_temperature_c = 10.0;
    p.surface_dewpoint_c = 8.0;
    CloudLayerDef deck;
    deck.id = "deck";
    deck.kind = "stratus";
    deck.base_m = 1500.0;
    deck.thickness_m = 800.0;
    deck.cover_oktas = 8.0;
    deck.lwc_g_m3 = 0.3;
    p.layers = {deck};
    const Weather overcast(p, 0.0, 1u);
    World w;
    w.env.weather = &overcast;
    w.env.visibility_m = 40000.0;
    PerceptionData d;
    // The observer is at 4 km above the deck; the trail's head is under it, then above it.
    for (const bool above : {false, true}) {
        PerceptionHub hub(d, 9);
        ObserverIn ob = Observer("high", Vec3{0.0, 0.0, 4000.0}, Vec3{0.0, 200.0, 0.0});
        hub.AddCue(ob.key, 0.0, 1e9);
        VisibleEvent t;
        t.kind = SightKind::Trail;
        t.what = "SAM trail";
        t.start_s = 0.0;
        t.contrast_pct = 20.0;
        t.hang_s = 60.0;
        const std::uint64_t id = hub.AddEvent(t);
        const double z = above ? 2800.0 : 1000.0;  // above the tops, or under the base
        hub.ExtendTrail(id, Vec3{0.0, 3000.0, z}, 0.0, true);
        hub.ExtendTrail(id, Vec3{0.0, 3200.0, z + 100.0}, 1.0, true);
        hub.ExtendTrail(id, Vec3{0.0, 3400.0, z + 200.0}, 2.0, true);
        Hold(hub, ob, w.env, 2.0, 20.0);
        SC_EXPECT(Holds(hub, ob, SightKind::Trail, 1e9) == above);
    }
}

// ---------------------------------------------------------------------------------------------- the warning receiver

SC_TEST("perception: a NATO receiver shows search, lock and launch by name and range class; a Pact one by quadrant with no launch") {
    PerceptionData d;
    std::string error;
    SC_EXPECT(LoadPerceptionData(kData, d, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    const RwrSetDisplay* nato = d.RwrSet("nato_alr46_class_rwr");
    const RwrSetDisplay* pact = d.RwrSet("pact_spo10_class_rwr");
    SC_EXPECT(nato != nullptr && pact != nullptr);
    if (nato == nullptr || pact == nullptr) return;
    World w;
    const Vec3 site{3000.0, 9000.0, 0.0};
    ObserverIn base = Observer("f", Vec3{0.0, 0.0, 800.0}, Vec3{0.0, 200.0, 0.0});
    const double true_az = AzimuthOf(site - base.pos);
    RwrHeard search;
    search.key = 1;
    search.state = RwrState::Search;
    search.name = "sa8_land_roll_search";
    search.known = true;
    search.bearing_rad = true_az;
    search.sigma_rad = 0.17;
    search.power_dbw = -85.0;
    RwrHeard launch = search;
    launch.key = 2;
    launch.state = RwrState::Launch;
    launch.name = "sa8_command_guidance";
    launch.power_dbw = -60.0;
    {
        PerceptionHub hub(d, 1);
        ObserverIn ob = base;
        ob.rwr_set = nato;
        std::vector<Sighting> fresh;
        Hold(hub, ob, w.env, 0.0, 3.0, {}, {search, launch}, &fresh);
        const std::vector<Sighting>& pic = hub.Picture(ob.key);
        SC_EXPECT(pic.size() >= 2);
        bool saw_launch = false, saw_search = false;
        for (const Sighting& s : pic) {
            if (s.by != SightBy::Rwr) continue;
            SC_EXPECT(s.kind == SightKind::Emitter);
            // A bearing and a class of range, never the emitter's place: the believed point is on the line at the class's range.
            SC_EXPECT((s.believed_enu - ob.pos).Length() <= nato->far_m + 1.0);
            SC_EXPECT(std::fabs((s.believed_enu - site).Length()) > 100.0);
            if (s.rwr == RwrState::Launch) {
                saw_launch = true;
                SC_EXPECT(s.what == "sa8_command_guidance");
                SC_EXPECT_NEAR(s.range_est_m, nato->near_m, 1.0);
            }
            if (s.rwr == RwrState::Search) {
                saw_search = true;
                SC_EXPECT(s.what == "sa8_land_roll_search");
                SC_EXPECT_NEAR(s.range_est_m, nato->far_m, 1.0);
            }
        }
        SC_EXPECT(saw_launch && saw_search);
        SC_EXPECT(!fresh.empty());
    }
    {
        PerceptionHub hub(d, 1);
        ObserverIn ob = base;
        ob.rwr_set = pact;
        Hold(hub, ob, w.env, 0.0, 3.0, {}, {search, launch});
        bool launch_shown = false, lock_shown = false;
        for (const Sighting& s : hub.Picture(ob.key)) {
            if (s.by != SightBy::Rwr) continue;
            if (s.rwr == RwrState::Launch) launch_shown = true;
            if (s.rwr == RwrState::Lock) {
                lock_shown = true;  // the command link reads as a lock
                SC_EXPECT(s.what == "lock");
                // The bearing is the quadrant's: dead ahead, behind or a beam.
                const double rel = std::fabs(std::remainder(s.bearing_rad - AzimuthOf(base.vel), kPi / 2.0));
                SC_EXPECT(rel < 1e-6);
            }
        }
        SC_EXPECT(!launch_shown && lock_shown);
    }
    {
        // No receiver: nothing is warned of, whatever radiates.
        PerceptionHub hub(d, 1);
        ObserverIn ob = base;
        Hold(hub, ob, w.env, 0.0, 3.0, {}, {search, launch});
        SC_EXPECT(hub.Picture(ob.key).empty());
    }
}

SC_TEST("perception: the display has room for max_display contacts and drops the weakest") {
    PerceptionData d;
    RwrSetDisplay set;
    set.em_esm = "x";
    set.max_display = 3;
    set.false_alarms_per_h = 0.0;
    World w;
    PerceptionHub hub(d, 1);
    ObserverIn ob = Observer("f", Vec3{0.0, 0.0, 800.0}, Vec3{0.0, 200.0, 0.0});
    ob.rwr_set = &set;
    std::vector<RwrHeard> heard;
    for (int i = 0; i < 6; ++i) {
        RwrHeard h;
        h.key = static_cast<std::uint64_t>(i + 1);
        h.state = RwrState::Search;
        h.bearing_rad = 0.5 * i;
        h.power_dbw = -60.0 - 5.0 * i;
        heard.push_back(h);
    }
    Hold(hub, ob, w.env, 0.0, 2.0, {}, heard);
    int shown = 0;
    for (const Sighting& s : hub.Picture(ob.key)) {
        if (s.by == SightBy::Rwr) {
            ++shown;
            SC_EXPECT(s.debug_truth_key <= 3);  // the three strongest
        }
    }
    SC_EXPECT(shown == 3);
}

SC_TEST("perception: a receiver's false alarms come at about their rate") {
    PerceptionData d;
    RwrSetDisplay set;
    set.em_esm = "x";
    set.false_alarms_per_h = 30.0;
    World w;
    PerceptionHub hub(d, 4);
    ObserverIn ob = Observer("f", Vec3{0.0, 0.0, 800.0}, Vec3{0.0, 200.0, 0.0});
    ob.rwr_set = &set;
    int alarms = 0;
    for (double t = 0.0; t < 3600.0; t += 1.0) {
        std::vector<Sighting> fresh;
        if (hub.Due(ob.key, t)) hub.Tick(ob, w.env, t, {}, {}, &fresh);
        alarms += static_cast<int>(fresh.size());
    }
    SC_EXPECT(alarms >= 15 && alarms <= 50);  // 30 an hour
}

// ---------------------------------------------------------------------------------------------- determinism and data

SC_TEST("perception: the same seed gives the same sightings, in any order of the objects; another seed gives others") {
    World w;
    auto run = [&](std::uint64_t seed, bool shuffled) {
        PerceptionHub hub(PerceptionData(), seed);
        ObserverIn ob = Observer("watcher", Vec3{0.0, 0.0, 3000.0}, Vec3{0.0, 220.0, 0.0});
        hub.AddCue(ob.key, 0.0, 1e9);
        std::vector<AirObject> air;
        for (int i = 0; i < 6; ++i) {
            AirObject a;
            a.id = "ac" + std::to_string(i);
            a.key = ObjectKeyOf(a.id);
            a.side = 1;
            a.pos = Vec3{-500.0 + 300.0 * i, 1500.0 + 350.0 * i, 3000.0};
            a.vel = Vec3{0.0, -60.0, 0.0};
            a.alt_m = 3000.0;
            air.push_back(a);
        }
        if (shuffled) std::reverse(air.begin(), air.end());
        std::string out;
        for (double t = 0.0; t < 60.0; t += 0.5) {
            for (AirObject& a : air) a.pos += a.vel * 0.5;
            if (hub.Due(ob.key, t)) hub.Tick(ob, w.env, t, air, {}, nullptr);
        }
        std::vector<Sighting> pic = hub.Picture(ob.key);
        std::sort(pic.begin(), pic.end(), [](const Sighting& a, const Sighting& b) { return a.debug_truth_key < b.debug_truth_key; });
        for (const Sighting& s : pic) {
            char buf[160];
            std::snprintf(buf, sizeof(buf), "%llu %.6f %.3f %.3f %.3f|", static_cast<unsigned long long>(s.debug_truth_key), s.first_s,
                          s.believed_enu.x, s.believed_enu.y, s.believed_enu.z);
            out += buf;
        }
        return out;
    };
    const std::string a = run(21, false), b = run(21, false), c = run(21, true), d = run(22, false);
    SC_EXPECT(!a.empty());
    SC_EXPECT(a == b);
    SC_EXPECT(a == c);
    SC_EXPECT(a != d);
}

SC_TEST("perception: the data file loads, every group is read, and it agrees with the code's defaults") {
    PerceptionData d, def;
    std::string error;
    SC_EXPECT(LoadPerceptionData(kData, d, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT_NEAR(d.eye.rm_constant_m, def.eye.rm_constant_m, 1e-9);
    SC_EXPECT_NEAR(d.eye.recognise_lines, def.eye.recognise_lines, 1e-9);
    SC_EXPECT_NEAR(d.trail.contrast_pct, def.trail.contrast_pct, 1e-9);
    SC_EXPECT_NEAR(d.ground.pmax[0][0], 0.25, 1e-9);
    SC_EXPECT_NEAR(d.ground.pmax[2][2], 1.00, 1e-9);
    SC_EXPECT(d.Class("osa_battery_silent") != nullptr && d.Class("launch_sign") != nullptr);
    SC_EXPECT(d.rwr_sets.size() == 3);
    const RwrSetDisplay* spo10 = d.RwrSet("pact_spo10_class_rwr");
    SC_EXPECT(spo10 != nullptr && spo10->quadrant && !spo10->launch_indication && !spo10->shows_name);
    // A broken file is refused with its path.
    json::Value root;
    std::string e;
    SC_EXPECT(json::Parse("{\"perception_id\":\"x\",\"eye\":{\"nope\":{}}}", root, &e));
    airwar::detail::PerceptionLoader loader;
    PerceptionData t;
    SC_EXPECT(!loader.Load(root, t));
}
