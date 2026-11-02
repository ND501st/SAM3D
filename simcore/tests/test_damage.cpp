// Damage states by chance (simcore/damage.hpp, roadmap R1) and the guns' fire by range (simcore/aaa.hpp, R2):
// hand calculations of the fragments' strikes and a round's hit chance, the owner's adding-up rule, and the guns'
// accuracy falling with range.
#include <cmath>
#include <cstdio>
#include <random>
#include <string>

#include "simcore/aaa.hpp"
#include "simcore/damage.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

DamageTable LoadDamage() {
    DamageTable t;
    std::string e;
    if (!LoadDamageFile(kData + "/gameplay/damage.json", t, &e)) std::printf("    %s\n", e.c_str());
    return t;
}

AaaTable LoadAaa() {
    AaaTable t;
    std::string e;
    if (!LoadAaaFile(kData, "gameplay/aaa.json", t, &e)) std::printf("    %s\n", e.c_str());
    return t;
}

bool LoadsDamageText(const std::string& text, std::string* why) {
    json::Value root;
    if (!json::Parse(text, root, why)) return false;
    detail::DamageLoader loader;
    DamageTable t;
    const bool ok = loader.Load(root, t);
    if (why != nullptr) *why = loader.Message();
    return ok;
}

// The share of `n` bursts `miss_m` from a fighter (20 m2) that leave it at least `level`.
double ShareAtLeast(const DamageTable& t, const GunDef& g, double miss_m, DamageLevel level, int n, unsigned seed) {
    std::mt19937 rng(seed);
    int k = 0;
    for (int i = 0; i < n; ++i) {
        AircraftDamage a;
        ApplyDamage(t, a, DrawBurst(t, g.mass_kg, g.charge_kg, 0.0, miss_m, 20.0, rng), 0.0, rng, "test");
        k += a.level >= level ? 1 : 0;
    }
    return static_cast<double>(k) / n;
}

// A fighter flying straight and level at 250 m/s across a gun's front, `offset_m` from it at 1,000 m up: the mean
// chance one of a gun's rounds strikes it, its pass's errors drawn `n` times, at the moment it is `range_m` off.
double MeanRoundChance(const GunDef& g, const FireControlDef& fc, double range_m, int n, unsigned seed) {
    std::mt19937 rng(seed);
    const Vec3 muzzle{0.0, 0.0, 2.0};
    const double up = 1000.0;
    const double ground = std::sqrt(std::max(range_m * range_m - up * up, 1.0));
    const Vec3 pos{-ground * 0.6, ground * 0.8, up};
    const Vec3 vel{250.0, 0.0, 0.0};
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        const PassErrors e = DrawPassErrors(fc, vel, false, rng);
        const GunAim aim = AimGun(g, muzzle, pos, vel, e);
        double miss = 0.0, spread = 0.0;
        if (!GroupMiss(g, muzzle, aim, pos + vel * aim.tof_s, miss, spread)) continue;
        sum += RoundHitChance(miss, spread, 20.0);
    }
    return sum / n;
}

}  // namespace

SC_TEST("damage: the tables load with every value cited, and bad ones are refused") {
    const DamageTable t = LoadDamage();
    SC_EXPECT(t.rounds.size() == 3);
    SC_EXPECT(t.lights_per_moderate == 3 && t.moderates_per_heavy == 2);
    SC_EXPECT_NEAR(t.crash_min_s, 10.0, 1e-9);
    SC_EXPECT_NEAR(t.crash_max_s, 300.0, 1e-9);
    const AaaTable a = LoadAaa();
    SC_EXPECT(a.guns.size() == 5);
    const GunDef* ks30 = a.Gun("ks30");
    const GunDef* s60 = a.Gun("s60");
    SC_EXPECT(ks30 != nullptr && ks30->time_fuze && ks30->fuze_error_s > 0.0);
    SC_EXPECT(s60 != nullptr && !s60->time_fuze && s60->sd_lo_s > 0.0);  // the S-60's rounds burst on contact
    SC_EXPECT(a.Control("zap23_sight") != nullptr && a.Control("zap23_sight")->optical);
    std::string why;
    SC_EXPECT(!LoadsDamageText(R"({"damage_id": "d", "source": "s", "confidence": "estimate", "values": {"bogus": 1}})", &why));
    SC_EXPECT(why.find("unknown key") != std::string::npos);
    SC_EXPECT(!LoadsDamageText(R"({"damage_id": "d", "source": "s", "confidence": "estimate", "values": {}})", &why));
    SC_EXPECT(why.find("required") != std::string::npos);
}

SC_TEST("damage: a burst's fragment strikes and a round's hit chance against hand calculations") {
    const DamageTable t = LoadDamage();
    // The KS-30's shell (33.4 kg, 3.5 kg of TNT) 15 m from a fighter of 20 m2: 70 x 29.9 = 2093 fragments; their reach
    // 26 x 3.5^(1/3) = 39.48 m; 2093 x 20 / (4 pi 225) x exp(-15 / 39.48) = 14.80 x 0.6839 = 10.12.
    const double n = BurstFragments(t, 33.4, 3.5);
    SC_EXPECT_NEAR(n, 2093.0, 0.5);
    SC_EXPECT_NEAR(ExpectedFragmentStrikes(t, n, 3.5, 15.0, 20.0), 10.12, 0.03);
    // Four times as far, a sixteenth of the spray and the reach's fall: 0.6325 x exp(-60 / 39.48) / exp(-15 / 39.48)
    SC_EXPECT_NEAR(ExpectedFragmentStrikes(t, n, 3.5, 60.0, 20.0), 10.12 / 16.0 * std::exp(-45.0 / 39.48), 0.01);
    // A round's chance: 20 m2 is a disc of r^2 = 6.366; a 5 m spread: 1 - exp(-6.366 / 50) = 0.1196 at the centre, and
    // 5 m off it, times exp(-25 / (2 (25 + 3.183))) = 0.6417: 0.0767.
    SC_EXPECT_NEAR(RoundHitChance(0.0, 5.0, 20.0), 0.1196, 0.0005);
    SC_EXPECT_NEAR(RoundHitChance(5.0, 5.0, 20.0), 0.0767, 0.0005);
    // Between the table's calibres, interpolated: 30 mm half way from 23 to 37.
    const DamageRoundHit h = RoundHitFor(t, 30.0);
    SC_EXPECT_NEAR(h.critical, 0.5 * (0.25 + 0.40), 1e-9);
    SC_EXPECT_NEAR(h.serious, 0.25, 1e-9);
}

SC_TEST("damage: three light strikes make moderate damage and two moderate make heavy, which crashes 10 s to 5 min later") {
    DamageTable t = LoadDamage();
    t.moderate_smoke = t.moderate_jettison = t.moderate_abort = 1.0;
    t.heavy_crash = 1.0;
    std::mt19937 rng(7u);
    AircraftDamage a;
    DamageDraw light;
    light.light = 1;
    SC_EXPECT(ApplyDamage(t, a, light, 0.0, rng, "zu23").after == DamageLevel::Light);
    ApplyDamage(t, a, light, 1.0, rng, "zu23");
    SC_EXPECT(a.level == DamageLevel::Light && a.light == 2);
    const DamageChange third = ApplyDamage(t, a, light, 2.0, rng, "zu23");
    SC_EXPECT(third.before == DamageLevel::Light && third.after == DamageLevel::Moderate);
    SC_EXPECT(third.smoke && third.jettison && third.abort && a.light == 0 && a.moderate == 1);
    DamageDraw serious;
    serious.serious = 1;
    const DamageChange heavy = ApplyDamage(t, a, serious, 100.0, rng, "s60");
    SC_EXPECT(heavy.after == DamageLevel::Heavy && heavy.crashes && a.by == "s60");
    SC_EXPECT(a.crash_at_s >= 110.0 - 1e-9 && a.crash_at_s <= 400.0 + 1e-9);
    SC_EXPECT(!heavy.smoke);  // it was smoking already
    // A blast inside its reach destroys outright; nothing more happens to a destroyed aircraft.
    DamageDraw blast;
    blast.blast_destroy = true;
    SC_EXPECT(ApplyDamage(t, a, blast, 101.0, rng, "ks30").after == DamageLevel::Destroyed);
    SC_EXPECT(!ApplyDamage(t, a, light, 102.0, rng, "zu23").Raised());
    // A missile that missed does at most moderate damage, whatever its fragments did.
    int above = 0, moderate = 0;
    for (int i = 0; i < 2000; ++i) {
        AircraftDamage m;
        ApplyMissile(t, m, false, 70.0, 20.0, 0.0, rng, "s75m2");
        above += m.level > DamageLevel::Moderate ? 1 : 0;
        moderate += m.level == DamageLevel::Moderate ? 1 : 0;
    }
    SC_EXPECT(above == 0 && moderate > 200);
}

SC_TEST("damage: a heavy shell bursting 30-50 m off mostly does nothing; close, it brings the aircraft down") {
    const DamageTable t = LoadDamage();
    const GunDef* ks30 = LoadAaa().Gun("ks30");
    SC_EXPECT(ks30 != nullptr);
    if (ks30 == nullptr) return;
    const double close_down = ShareAtLeast(t, *ks30, 8.0, DamageLevel::Heavy, 2000, 1u);
    const double mid_down = ShareAtLeast(t, *ks30, 20.0, DamageLevel::Heavy, 2000, 2u);
    const double far_any = ShareAtLeast(t, *ks30, 40.0, DamageLevel::Moderate, 2000, 3u);
    const double far_down = ShareAtLeast(t, *ks30, 50.0, DamageLevel::Heavy, 2000, 4u);
    std::printf("    KS-30 burst: heavy or worse %.2f at 8 m, %.2f at 20 m, %.3f at 50 m; moderate or worse %.2f at 40 m\n", close_down, mid_down,
                far_down, far_any);
    SC_EXPECT(close_down > 0.8);
    SC_EXPECT(mid_down > 0.15 && mid_down < 0.7);
    SC_EXPECT(far_any < 0.3);
    SC_EXPECT(far_down < 0.05);
}

SC_TEST("aaa: a perfect fire control meets a straight flier; its errors and the range spread the rounds; a turn beats the prediction") {
    const AaaTable a = LoadAaa();
    const GunDef* zsu = a.Gun("zsu234");
    const GunDef* zu = a.Gun("zu23");
    SC_EXPECT(zsu != nullptr && zu != nullptr);
    if (zsu == nullptr || zu == nullptr) return;
    const Vec3 muzzle{0.0, 0.0, 2.0};
    const Vec3 pos{-600.0, 1500.0, 500.0};
    const Vec3 vel{250.0, 0.0, 0.0};
    // No errors: the pattern's centre meets the aircraft where it flies straight on.
    const GunAim perfect = AimGun(*zsu, muzzle, pos, vel, PassErrors{});
    double miss = 0.0, spread = 0.0;
    SC_EXPECT(GroupMiss(*zsu, muzzle, perfect, pos + vel * perfect.tof_s, miss, spread));
    SC_EXPECT(miss < 0.5);
    SC_EXPECT_NEAR(spread, zsu->dispersion_mrad * 1e-3 * (perfect.point - muzzle).Length(), 0.01);
    // A 5 g turn across the line of fire begun as the rounds leave: the miss is half its acceleration times the time of
    // flight squared.
    Vec3 right, up;
    AcrossBasis(perfect.dir, right, up);
    const Vec3 accel = up * (5.0 * 9.80665);
    const double t = perfect.tof_s;
    SC_EXPECT(GroupMiss(*zsu, muzzle, perfect, pos + vel * t + accel * (0.5 * t * t), miss, spread));
    SC_EXPECT_NEAR(miss, 0.5 * 5.0 * 9.80665 * t * t, 0.05 * 0.5 * 5.0 * 9.80665 * t * t);
    // The rounds destroy themselves past their self-destruct: an aircraft beyond it cannot be struck.
    const GunAim beyond = AimGun(*zsu, muzzle, Vec3{0.0, 9000.0, 3000.0}, vel, PassErrors{});
    SC_EXPECT(beyond.tof_s > zsu->sd_lo_s);
    SC_EXPECT(!GroupMiss(*zsu, muzzle, beyond, Vec3{0.0, 9000.0, 3000.0} + vel * beyond.tof_s, miss, spread));
    // With the fire control's errors, a round's chance falls steeply with the range, and the sight is worse than the radar.
    const FireControlDef* radar = a.Control(zsu->fire_control);
    const FireControlDef* sight = a.Control(zu->fire_control);
    const double near_radar = MeanRoundChance(*zsu, *radar, 1200.0, 400, 11u);
    const double mid_radar = MeanRoundChance(*zsu, *radar, 2000.0, 400, 12u);
    const double far_radar = MeanRoundChance(*zsu, *radar, 3000.0, 400, 13u);
    const double near_sight = MeanRoundChance(*zu, *sight, 1200.0, 400, 14u);
    std::printf("    a 23 mm round's chance on a fighter: radar %.4f at 1.2 km, %.4f at 2 km, %.5f at 3 km; sight %.4f at 1.2 km\n", near_radar,
                mid_radar, far_radar, near_sight);
    SC_EXPECT(near_radar > 2.5 * mid_radar && mid_radar > 2.0 * far_radar);
    SC_EXPECT(near_sight < near_radar);
}

SC_TEST("aaa: a time-fuzed shell bursts round the predicted meeting, further off the longer it flies") {
    const AaaTable a = LoadAaa();
    const GunDef* ks30 = a.Gun("ks30");
    SC_EXPECT(ks30 != nullptr);
    if (ks30 == nullptr) return;
    const FireControlDef* fc = a.Control(ks30->fire_control);
    std::mt19937 rng(5u);
    const Vec3 muzzle{0.0, 0.0, 2.0};
    const Vec3 vel{220.0, 0.0, 0.0};
    const auto mean_miss = [&](double range_m) {
        const Vec3 pos{0.0, range_m * 0.8, range_m * 0.6};
        double sum = 0.0;
        for (int i = 0; i < 500; ++i) {
            const GunAim aim = AimGun(*ks30, muzzle, pos, vel, DrawPassErrors(*fc, vel, false, rng));
            const ShellBurst b = TimeShellBurst(*ks30, muzzle, aim, rng);
            sum += (b.position - (pos + vel * b.after_s)).Length();
        }
        return sum / 500.0;
    };
    const double miss5 = mean_miss(5000.0), miss12 = mean_miss(12000.0);
    std::printf("    KS-30 shells' mean miss: %.0f m at 5 km, %.0f m at 12 km\n", miss5, miss12);
    SC_EXPECT(miss12 > 1.8 * miss5);
    // Most heavy shells burst well clear: the fuze's error alone is ~0.15 s of the shell's flight, ~100 m at 5 km.
    SC_EXPECT(miss5 > 60.0 && miss5 < 400.0);
}
