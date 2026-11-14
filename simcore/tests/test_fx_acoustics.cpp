// How a sound reaches the listener (simcore/fx/acoustics.hpp, docs/fx/README.md "Sound"): the speed of sound, ISO 9613-1
// absorption against the standard's tables, the delay and Doppler shift of a moving source against the geometry, the Mach
// cone's arrival over a listener against h sqrt(M^2 - 1) / (c M), one boom a cone passage at any frame phase, and
// Carlson's boom against a hand calculation of his formula with K_p = K_t = 1.
#include <cmath>
#include <cstdio>
#include <vector>

#include "simcore/fx/acoustics.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::fx;

namespace {

// A source flying a straight line at `speed` along +x, at height `height` over the listener at the origin, overhead at
// t = 0, sampled every `dt` from t0 to t1.
SourceTrack StraightPass(double speed, double height, double dt, double t0, double t1) {
    SourceTrack track(300.0);
    for (double t = t0; t <= t1 + 1e-9; t += dt) track.Push(t, {speed * t, 0.0, height});
    return track;
}

// The times a BoomWatch booms, listening at the origin in frames of `frame_s` from t0 to t1.
std::vector<double> BoomTimes(const SourceTrack& track, double c, double t0, double t1, double frame_s) {
    BoomWatch watch;
    std::vector<double> booms;
    const int frames = static_cast<int>(std::lround((t1 - t0) / frame_s));
    for (int i = 0; i <= frames; ++i) {
        const double t = t0 + frame_s * i;
        if (watch.Update(Hear(track, {}, t, c), frame_s, track.Oldest().t_s)) booms.push_back(t);
    }
    return booms;
}

}  // namespace

SC_TEST("fx: speed of sound and humidity") {
    SC_EXPECT_NEAR(SpeedOfSound(293.15), 343.2, 0.3);
    SC_EXPECT_NEAR(SpeedOfSound(288.15), 340.3, 0.2);
    SC_EXPECT_NEAR(SpeedOfSound(216.65), 295.1, 0.2);  // the tropopause
    SC_EXPECT_NEAR(RelativeHumidityPct(15.0, 15.0), 100.0, 1e-9);
    SC_EXPECT_NEAR(RelativeHumidityPct(20.0, 10.0), 52.5, 0.5);  // Magnus: 12.28 / 23.39 hPa
}

SC_TEST("fx: iso9613 absorption matches the tables") {
    // ISO 9613-2 table 2 (dB/km): 20 °C 70 %: 1 kHz 5.0, 2 kHz 9.0, 4 kHz 22.9, 8 kHz 76.6; 10 °C 70 %: 4 kHz 32.8,
    // 8 kHz 117.
    const double p = 101325.0;
    SC_EXPECT_NEAR(AirAbsorptionDbPerM(1000.0, 293.15, 70.0, p) * 1000.0, 5.0, 0.1);
    SC_EXPECT_NEAR(AirAbsorptionDbPerM(2000.0, 293.15, 70.0, p) * 1000.0, 9.0, 0.15);
    SC_EXPECT_NEAR(AirAbsorptionDbPerM(4000.0, 293.15, 70.0, p) * 1000.0, 22.9, 0.4);
    SC_EXPECT_NEAR(AirAbsorptionDbPerM(8000.0, 293.15, 70.0, p) * 1000.0, 76.6, 1.5);
    SC_EXPECT_NEAR(AirAbsorptionDbPerM(4000.0, 283.15, 70.0, p) * 1000.0, 32.8, 0.5);
    SC_EXPECT_NEAR(AirAbsorptionDbPerM(8000.0, 283.15, 70.0, p) * 1000.0, 117.0, 2.0);
}

SC_TEST("fx: absorption cutoff falls with distance") {
    const double t = 288.15, rh = 60.0, p = 101325.0;
    SC_EXPECT_NEAR(AbsorptionCutoffHz(10.0, t, rh, p), 20000.0, 1e-6);  // close by: everything arrives
    const double c1 = AbsorptionCutoffHz(1000.0, t, rh, p);
    const double c5 = AbsorptionCutoffHz(5000.0, t, rh, p);
    const double c20 = AbsorptionCutoffHz(20000.0, t, rh, p);
    SC_EXPECT(c1 > c5 && c5 > c20);
    // At the cut-off the air has taken exactly the budget.
    SC_EXPECT_NEAR(AirAbsorptionDbPerM(c5, t, rh, p) * 5000.0, 6.0, 0.05);
    // 15 °C, 60 %: 20 kHz loses 0.52 dB/m, so even 100 m takes the hiss off (5.8 kHz); 1 km leaves 1.4 kHz, 5 km a
    // thud (250 Hz), 20 km a rumble (100 Hz).
    SC_EXPECT_NEAR(AirAbsorptionDbPerM(20000.0, t, rh, p), 0.516, 0.002);
    SC_EXPECT_NEAR(AbsorptionCutoffHz(100.0, t, rh, p), 5772.0, 30.0);
    SC_EXPECT_NEAR(c1, 1427.0, 10.0);
    SC_EXPECT_NEAR(c5, 254.0, 3.0);
    SC_EXPECT(c20 < 120.0);
    SC_EXPECT_NEAR(SpreadingLossDb(200.0, 100.0), 6.02, 0.01);
    SC_EXPECT_NEAR(SpreadingLossDb(50.0, 100.0), 0.0, 1e-12);  // inside the reference distance: no gain
}

SC_TEST("fx: a still source is heard after the sound has travelled") {
    const double c = 343.0;
    SourceTrack track;
    for (double t = 0.0; t <= 3.0 + 1e-9; t += 0.1) track.Push(t, {343.0, 0.0, 0.0});
    SC_EXPECT(!Hear(track, {}, 0.5, c).heard);  // it started making a sound at t = 0: still on its way
    const Reception r = Hear(track, {}, 2.5, c);
    SC_EXPECT(r.heard);
    SC_EXPECT_NEAR(r.delay_s, 1.0, 1e-9);
    SC_EXPECT_NEAR(r.emit_time_s, 1.5, 1e-9);
    SC_EXPECT_NEAR(r.doppler, 1.0, 1e-9);
    SC_EXPECT_NEAR(r.distance_m, 343.0, 1e-9);
}

SC_TEST("fx: a passing jet is heard where it was and shifted") {
    // 250 m/s at 300 m: the roar comes from behind where the jet now is, pitched up coming, down going.
    const double c = 340.0, v = 250.0, height = 300.0;
    const SourceTrack track = StraightPass(v, height, 0.05, -60.0, 60.0);
    const Vec3 ear{};
    for (double t : {-10.0, -1.0, 0.0, 1.0, 3.0, 20.0}) {
        const Reception r = Hear(track, ear, t, c);
        SC_EXPECT(r.heard);
        // The sound now arriving left exactly its travel time ago.
        SC_EXPECT_NEAR(c * (t - r.emit_time_s), (ear - Vec3{v * r.emit_time_s, 0.0, height}).Length(), 1e-6);
        SC_EXPECT(r.emit_time_s < t);
        const Vec3 to_ear = (ear - r.emit_position).Normalized();
        SC_EXPECT_NEAR(r.doppler, 1.0 / (1.0 - v * to_ear.x / c), 1e-6);
        SC_EXPECT_NEAR(r.source_mach, v / c, 1e-9);
    }
    SC_EXPECT(Hear(track, ear, -10.0, c).doppler > 1.5);  // coming: 1 / (1 - 0.74)
    SC_EXPECT(Hear(track, ear, 20.0, c).doppler < 0.65);  // going
    // Overhead now, the sound is from where it was 1.30 s ago (a jet heard "behind its noise"): c^2 tau^2 = (v tau)^2 +
    // h^2, so tau = -h / sqrt(c^2 - v^2) = -300 / 230.43 = -1.3019 s, at x = v tau = -325.47 m.
    const Reception now = Hear(track, ear, 0.0, c);
    SC_EXPECT_NEAR(now.emit_time_s, -1.3019, 0.0005);
    SC_EXPECT_NEAR(now.emit_position.x, -325.47, 0.05);
}

SC_TEST("fx: a supersonic jet is silent until its cone passes then booms once") {
    // Mach 2 at 1000 m over the listener: the cone arrives h sqrt(M^2 - 1) / (c M) after it is overhead (2.53 s).
    const double c = 343.0, mach = 2.0, v = mach * c, height = 1000.0, dt = 0.01;
    const SourceTrack track = StraightPass(v, height, dt, -40.0, 40.0);
    const double t_cone = height * std::sqrt(mach * mach - 1.0) / (c * mach);
    SC_EXPECT(!Hear(track, {}, 0.0, c).heard);  // overhead: not a sound yet
    SC_EXPECT(!Hear(track, {}, t_cone - 0.05, c).heard);
    SC_EXPECT(Hear(track, {}, t_cone + 0.05, c).heard);
    BoomWatch watch;
    int booms = 0;
    double boom_at = -100.0;
    for (double t = -5.0; t <= 20.0; t += 1.0 / 60.0) {
        if (watch.Update(Hear(track, {}, t, c), 1.0 / 60.0)) {
            ++booms;
            boom_at = t;
        }
    }
    SC_EXPECT(booms == 1);
    SC_EXPECT_NEAR(boom_at, t_cone, 1.0 / 60.0 + 1e-6);
    // After the boom the jet is heard going away, pitched down, in its own order.
    const Reception after = Hear(track, {}, 10.0, c);
    SC_EXPECT(after.heard && after.doppler < 1.0 && after.source_mach > 1.9);
}

SC_TEST("fx: a jet breaking the barrier booms when its cone catches up") {
    // Subsonic and heard, then through Mach 1 on the way past: one boom, after the old subsonic sound.
    const double c = 340.0, height = 2000.0, dt = 0.02;
    SourceTrack track(300.0);
    double x = -20000.0, v = 280.0;
    for (double t = -60.0; t <= 60.0; t += dt) {
        if (t > -20.0) v = std::min(v + 4.0 * dt * 10.0, 480.0);  // accelerates through Mach 1 near the listener
        x += v * dt;
        track.Push(t, {x, 0.0, height});
    }
    BoomWatch watch;
    int booms = 0;
    bool heard_before_boom = false;
    for (double t = -40.0; t <= 50.0; t += 1.0 / 30.0) {
        const Reception r = Hear(track, {}, t, c);
        if (watch.Update(r, 1.0 / 30.0)) ++booms;
        if (booms == 0 && r.heard) heard_before_boom = true;
    }
    SC_EXPECT(heard_before_boom);
    SC_EXPECT(booms == 1);
}

SC_TEST("fx: a jet just over Mach 1 high up booms once at every frame phase") {
    // 359 m/s at 10 km in air of path-mean c 318 m/s (Mach 1.129), the track sampled every 0.1 s: the cone arrives
    // h sqrt(M^2 - 1) / (c M) = 14.594 s after it is overhead. Just after, the later root runs forward as
    // sqrt(2 (t - t_cone) / t''), t'' = c (M^2 - 1)^1.5 / (h M) = 0.0041 /s: one 1/60 s frame can move it 2.9 s, far
    // past the 50 frames' advance the Doppler cap allows. That is the same cone: one boom, whatever the frame phase.
    const double c = 318.0, v = 359.0, height = 10000.0, mach = v / c;
    const SourceTrack track = StraightPass(v, height, 0.1, -120.0, 60.0);
    const double t_cone = height * std::sqrt(mach * mach - 1.0) / (c * mach);
    SC_EXPECT_NEAR(t_cone, 14.594, 0.001);
    for (const double fps : {60.0, 144.0}) {
        for (int phase = 0; phase < 20; ++phase) {
            const std::vector<double> booms = BoomTimes(track, c, 10.0 + phase / (20.0 * fps), 25.0, 1.0 / fps);
            if (booms.size() != 1) std::printf("    %.0f fps, phase %d: %zu booms\n", fps, phase, booms.size());
            SC_EXPECT(booms.size() == 1);
            if (!booms.empty()) SC_EXPECT_NEAR(booms[0], t_cone, 1.0 / fps + 0.01);
        }
    }
}

SC_TEST("fx: a jet that turns back over the listener booms on each pass") {
    // Mach 1.5 at 3 km straight over the listener, a turn of 5 km radius once 15 km past, and back along a line 10 km to
    // the side. The first cone arrives h sqrt(M^2 - 1) / (c M) = 6.58 s after the overhead; the second, from the leg
    // back, about 113 s: two passages of the cone, far more than the hold-off apart, both heard.
    const double c = 340.0, v = 1.5 * c, height = 3000.0, radius = 5000.0;
    SourceTrack track(400.0);
    double t = -60.0;
    for (; v * t < 15000.0; t += 0.1) track.Push(t, {v * t, 0.0, height});
    const double t_turn = t, x_turn = v * t;
    for (; v / radius * (t - t_turn) < kPi; t += 0.1) {
        const double a = v / radius * (t - t_turn);
        track.Push(t, {x_turn + radius * std::sin(a), radius - radius * std::cos(a), height});
    }
    const double t_back = t;
    for (; t < 160.0; t += 0.1) track.Push(t, {x_turn - v * (t - t_back), 2.0 * radius, height});
    const std::vector<double> booms = BoomTimes(track, c, 0.0, 118.0, 1.0 / 60.0);
    SC_EXPECT(booms.size() == 2);
    if (booms.size() == 2) {
        SC_EXPECT_NEAR(booms[0], height * std::sqrt(1.5 * 1.5 - 1.0) / (c * 1.5), 1.0 / 60.0 + 0.01);
        SC_EXPECT(booms[1] > 110.0 && booms[1] < 116.0);
    }
}

SC_TEST("fx: a supersonic source appearing while going away does not boom") {
    // A missile track begun at Mach 2.5 flying away (the scenario started it there): its first sound is its start, not
    // a cone.
    const double c = 340.0;
    SourceTrack track(300.0);
    for (double t = 0.0; t <= 20.0; t += 0.02) track.Push(t, {500.0 + 850.0 * t, 0.0, 100.0});
    BoomWatch watch;
    int booms = 0;
    for (double t = 0.0; t <= 20.0; t += 1.0 / 60.0) {
        if (watch.Update(Hear(track, {}, t, c), 1.0 / 60.0, track.Oldest().t_s)) ++booms;
    }
    SC_EXPECT(booms == 0);
    SC_EXPECT(Hear(track, {}, 10.0, c).doppler < 0.5);  // going away at Mach 2.5: 1 / 3.5
}

SC_TEST("fx: carlson boom by hand with K_p = K_t = 1") {
    // A hand calculation of Carlson's formula with K_p = K_t = 1 (not one of TP-1122's worked examples, whose chart
    // factors are omitted): an F-15-sized fighter (19.4 m, K_s 0.1) at Mach 1.5 and 10 km (26436 Pa, 299.5 m/s; the
    // ground 101325 Pa): 2 sqrt(26436 x 101325) 1.25^0.125 10000^-0.75 19.4^0.75 0.1 = 98.4 Pa (about 2 psf), and
    // (3.42 / 299.5) (1.5 / 1.25^0.375) 10000^0.25 19.4^0.75 0.1 = 0.1456 s.
    const AtmosphereState aloft = StandardAtmosphere(10000.0), ground = StandardAtmosphere(0.0);
    const BoomSignature b = CarlsonBoom(1.5, 10000.0, aloft.pressure_pa, ground.pressure_pa, aloft.speed_of_sound_mps, 19.4);
    SC_EXPECT_NEAR(b.overpressure_pa, 98.4, 0.2);
    SC_EXPECT_NEAR(b.duration_s, 0.1456, 0.0005);
    // Lower is louder and shorter; subsonic is nothing.
    const AtmosphereState low = StandardAtmosphere(3000.0);
    const BoomSignature b3 = CarlsonBoom(1.5, 3000.0, low.pressure_pa, ground.pressure_pa, low.speed_of_sound_mps, 19.4);
    SC_EXPECT(b3.overpressure_pa > 2.0 * b.overpressure_pa && b3.duration_s < b.duration_s);
    SC_EXPECT(CarlsonBoom(0.95, 3000.0, low.pressure_pa, ground.pressure_pa, low.speed_of_sound_mps, 19.4).overpressure_pa == 0.0);
    SC_EXPECT(!BoomReachesGround(300.0, 340.0) && BoomReachesGround(400.0, 340.0));
    SC_EXPECT_NEAR(PressureToDb(20.0), 120.0, 1e-9);
    SC_EXPECT_NEAR(DbToPressure(PressureToDb(96.0)), 96.0, 1e-9);
}

SC_TEST("fx: loudness window ducks and recovers") {
    LoudnessWindow w;
    w.Update(90.0, 0.1);  // a jet at 90 dB: the window stays at its floor
    SC_EXPECT_NEAR(w.TopDb(), 105.0, 1e-9);
    const double jet_quiet_scene = w.Gain(90.0);
    SC_EXPECT_NEAR(jet_quiet_scene, std::pow(10.0, -15.0 * 0.7 / 20.0), 1e-9);
    w.Update(160.0, 0.5);  // a bomb at 160 dB: the top jumps up at once (attack 400 dB/s)
    SC_EXPECT_NEAR(w.TopDb(), 160.0, 1e-9);
    SC_EXPECT(!w.Audible(90.0) && w.Gain(90.0) == 0.0);  // the jet drops out under the bang
    SC_EXPECT_NEAR(w.Gain(160.0), 1.0, 1e-12);
    for (int i = 0; i < 40; ++i) w.Update(90.0, 0.1);  // 4 s later: 48 dB of release
    SC_EXPECT_NEAR(w.TopDb(), 112.0, 1e-9);
    SC_EXPECT(w.Audible(90.0));
}

SC_TEST("fx: jet directivity roars behind and whines ahead") {
    const double deg = 3.14159265358979323846 / 180.0;
    SC_EXPECT(JetRoarDirectivityDb(140.0 * deg) > JetRoarDirectivityDb(0.0) + 8.0);
    SC_EXPECT(JetRoarDirectivityDb(140.0 * deg) > JetRoarDirectivityDb(90.0 * deg));
    SC_EXPECT(JetWhineDirectivityDb(0.0) > JetWhineDirectivityDb(180.0 * deg) + 8.0);
}

SC_TEST("fx: source track keeps its span and interpolates") {
    SourceTrack track(10.0);
    for (int i = 0; i <= 1000; ++i) track.Push(i * 0.1, {i * 1.0, 0.0, 0.0});
    SC_EXPECT(track.Oldest().t_s >= 100.0 - 10.0 - 7.0);  // trimmed in batches, never far behind the span
    SC_EXPECT(track.Size() < 200);
    SC_EXPECT_NEAR(track.PositionAt(95.05).x, 950.5, 1e-9);
    SC_EXPECT_NEAR(track.VelocityAt(95.05).x, 10.0, 1e-9);
    track.Push(100.0, {5.0, 0.0, 0.0});  // the same time again replaces the newest sample
    SC_EXPECT_NEAR(track.Newest().position.x, 5.0, 1e-12);
}
