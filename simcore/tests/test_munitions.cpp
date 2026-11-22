// Every munition's shape as data and its mesh (simcore/scenery/munitions.hpp, data/scenery/munitions.json,
// docs/fx/README.md section 3.2).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/air_war.hpp"
#include "simcore/air_war/weapon_flight.hpp"
#include "simcore/json.hpp"
#include "simcore/scenery.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::scenery;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

MunitionLibrary Munitions() {
    MunitionLibrary lib;
    std::string e;
    if (!lib.Load(kData + "/scenery/munitions.json", &e)) std::printf("    %s\n", e.c_str());
    return lib;
}

std::string ReadText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

// The farthest vertex from the body's axis.
double RadialReach(const TriMesh& m) {
    double r = 0.0;
    for (const Vec3& p : m.positions) r = std::max(r, std::sqrt(p.y * p.y + p.z * p.z));
    return r;
}

// The volume a closed mesh holds, from its triangles alone: the sum of a . (b x c) / 6. Positive when every triangle is
// wound outward, negative when wound inward: a check of the winding that does not use the builder's own normals.
double SignedVolume(const TriMesh& m) {
    double v = 0.0;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const Vec3& a = m.positions[m.indices[t]];
        const Vec3& b = m.positions[m.indices[t + 1]];
        const Vec3& c = m.positions[m.indices[t + 2]];
        v += Dot(a, Cross(b, c)) / 6.0;
    }
    return v;
}

// The body of revolution's volume, the integral of pi r(s)^2 over its length (Simpson's rule, 2000 steps).
double LatheVolume(const MunitionSpec& m) {
    const int n = 2000;
    const double h = m.length_m / n;
    double sum = 0.0;
    for (int i = 0; i <= n; ++i) {
        const double r = BodyRadius(m, std::min(h * i, m.length_m));
        sum += (i == 0 || i == n ? 1.0 : i % 2 == 1 ? 4.0 : 2.0) * r * r;
    }
    return kPi * sum * h / 3.0;
}

// Unit normals, one normal and colour per vertex, indices in range, colours in 0..1, and every triangle's face normal
// along its vertices' normals. Counts the failures. (The builder winds each triangle to agree with its normals, so the
// last part only says the two agree; that they point out is SignedVolume's and the radial check's to show.)
int MeshFaults(const TriMesh& m) {
    int bad = 0;
    if (m.normals.size() != m.positions.size() || m.colours.size() != m.positions.size() || m.indices.size() % 3 != 0) return 1;
    for (const Vec3& n : m.normals) bad += std::fabs(n.Length() - 1.0) > 1e-6 ? 1 : 0;
    for (const std::uint32_t i : m.indices) bad += i >= m.positions.size() ? 1 : 0;
    if (bad > 0) return bad;
    for (const auto& c : m.colours) {
        for (const float v : c) bad += (v >= 0.0f && v <= 1.0f) ? 0 : 1;
    }
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const std::uint32_t a = m.indices[t], b = m.indices[t + 1], c = m.indices[t + 2];
        const Vec3 f = Cross(m.positions[b] - m.positions[a], m.positions[c] - m.positions[a]);
        bad += Dot(f, m.normals[a] + m.normals[b] + m.normals[c]) < 0.0 ? 1 : 0;
    }
    return bad;
}

// A one-munition library around `body` (the munition's fields after its id), for hand-checked shapes.
std::string OneMunition(const std::string& body) {
    return "{\"colours\": {\"grey\": \"#808080\"}, \"munitions\": [{\"id\": \"test\", \"name\": \"Test\", " + body +
           ", \"source\": \"placeholder\", \"confidence\": \"estimate\"}]}";
}

}  // namespace

SC_TEST("munitions: the file loads and every weapon, system, model and alias resolves") {
    const MunitionLibrary lib = Munitions();
    SC_EXPECT(lib.munitions.size() >= 60);
    const std::vector<std::string> ids = lib.Ids();
    SC_EXPECT(ids.size() == lib.munitions.size());
    SC_EXPECT(std::set<std::string>(ids.begin(), ids.end()).size() == ids.size());

    // Every weapon of the air war's catalogue, a gun as a gun (no model) and the rest with a shape.
    json::Value root;
    SC_EXPECT(json::Parse(ReadText(kData + "/air_war/weapons.json"), root));
    const json::Value* weapons = root.Find("weapons");
    SC_EXPECT(weapons != nullptr && weapons->items.size() >= 37);
    if (weapons != nullptr) {
        for (const json::Value& w : weapons->items) {
            const json::Value* id = w.Find("id");
            const json::Value* kind = w.Find("kind");
            if (id == nullptr || kind == nullptr) continue;
            const MunitionSpec* m = lib.Resolve(id->string);
            if (m == nullptr) std::printf("    no munition for weapon %s\n", id->string.c_str());
            SC_EXPECT(m != nullptr);
            if (m == nullptr) continue;
            SC_EXPECT(lib.weapons.count(id->string) == 1);
            SC_EXPECT((kind->string == "gun") == (m->kind == "gun"));
            SC_EXPECT(m->HasMesh() == (kind->string != "gun"));
        }
    }
    // Every air defence system the theatre may hold fires something.
    for (const airwar::AirDefenceSystem& s : airwar::AirDefenceSystems()) {
        const MunitionSpec* m = lib.Resolve(s.id);
        if (m == nullptr) std::printf("    no munition for system %s\n", s.id);
        SC_EXPECT(m != nullptr && lib.systems.count(s.id) == 1);
    }
    const MunitionSpec* osa = lib.Resolve("osa");
    SC_EXPECT(osa != nullptr && osa->id == "9m33m2");
    const MunitionSpec* zu = lib.Resolve("zu23");
    SC_EXPECT(zu != nullptr && zu->calibre_mm == 23.0 && zu->kind == "shell");
    // The basic models a missile is drawn as.
    for (const char* model : {"missile_9m33", "missile_arm", "kh22m", "tlam_c", "agm86c"}) {
        SC_EXPECT(lib.Resolve(model) != nullptr && lib.models.count(model) == 1);
    }
    // Our missile under its short name, and the EM library's anti-radiation missiles.
    SC_EXPECT(lib.Resolve("9m33") != nullptr && lib.Resolve("9m33")->id == "9m33m2");
    SC_EXPECT(lib.Resolve("9m33m3") != nullptr);
    const char* const arms[][2] = {{"agm45_class_shrike", "agm45"}, {"agm88a_class_harm", "agm88a"}, {"agm78b_class_standard_arm", "agm78b"},
                                   {"kh58_class", "kh58"}, {"kh25mp", "kh25mp"}};
    for (const auto& a : arms) {
        const MunitionSpec* m = lib.Resolve(a[0]);
        SC_EXPECT(m != nullptr && m->id == a[1] && m->kind == "arm");
    }
    // The neighbours' missiles and the cruise missiles.
    for (const char* id : {"3m9m3", "v601", "v750", "v755", "9m37m", "9m31m", "9m32m", "9m36", "9m39", "kh22m", "bgm109c", "agm86c"}) {
        SC_EXPECT(lib.Find(id) != nullptr);
    }
    // The air battle's (roadmap B13): every air-to-air missile of air_combat.json by its munition, a missile with a shape
    // and a motor; every fighter's gun a gun; the drop tank a fighter throws off a shape with no motor and no charge.
    json::Value combat;
    SC_EXPECT(json::Parse(ReadText(kData + "/air_war/air_combat.json"), combat));
    const json::Value* aams = combat.Find("aams");
    const json::Value* guns = combat.Find("guns");
    SC_EXPECT(aams != nullptr && aams->items.size() >= 18 && guns != nullptr && guns->items.size() >= 10);
    if (aams != nullptr) {
        for (const json::Value& a : aams->items) {
            const json::Value* mun = a.Find("munition");
            const MunitionSpec* m = mun != nullptr ? lib.Resolve(mun->string) : nullptr;
            if (m == nullptr) std::printf("    no munition for the AAM %s\n", mun != nullptr ? mun->string.c_str() : "(none)");
            SC_EXPECT(m != nullptr && m->kind == "missile" && m->HasMesh() && m->has_motor && m->charge_kg > 0.0);
        }
    }
    if (guns != nullptr) {
        for (const json::Value& g : guns->items) {
            const json::Value* id = g.Find("id");
            const MunitionSpec* m = id != nullptr ? lib.Resolve(id->string) : nullptr;
            if (m == nullptr) std::printf("    no munition for the gun %s\n", id != nullptr ? id->string.c_str() : "(none)");
            SC_EXPECT(m != nullptr && m->kind == "gun");
        }
    }
    const MunitionSpec* tank = lib.Resolve("drop_tank");
    SC_EXPECT(tank != nullptr && tank->kind == "tank" && tank->HasMesh() && !tank->has_motor && tank->charge_kg == 0.0);
    SC_EXPECT(lib.Resolve("no_such_thing") == nullptr);
    SC_EXPECT(lib.Find("osa") == nullptr);  // a system's id is not a munition's
}

SC_TEST("munitions: every shape's mesh matches its length, diameter and span, at every level of detail") {
    const MunitionLibrary lib = Munitions();
    int checked = 0;
    for (const MunitionSpec& m : lib.munitions) {
        if (m.kind == "gun") {
            SC_EXPECT(!m.HasMesh() && BuildMesh(m, 0).Empty());
            continue;
        }
        SC_EXPECT(m.HasMesh() && m.length_m > 0.0 && m.diameter_m > 0.0 && m.mass_kg > 0.0);
        std::size_t previous = 0;
        for (int lod = 0; lod <= 3; ++lod) {
            const TriMesh mesh = BuildMesh(m, lod);
            SC_EXPECT(!mesh.Empty());
            const int faults = MeshFaults(mesh);
            if (faults > 0) std::printf("    %s lod %d: %d faults\n", m.id.c_str(), lod, faults);
            SC_EXPECT(faults == 0);
            Vec3 lo, hi;
            mesh.Bounds(lo, hi);
            // Nose at +length/2, tail at -length/2.
            SC_EXPECT(std::fabs((hi.x - lo.x) - m.length_m) <= 0.02 * m.length_m);
            SC_EXPECT(std::fabs(hi.x - 0.5 * m.length_m) <= 0.02 * m.length_m);
            SC_EXPECT(std::fabs(lo.x + 0.5 * m.length_m) <= 0.02 * m.length_m);
            if (lod == 0) {
                // The body as wide as its diameter; the farthest point the fins' span (or a ring, or a booster).
                SC_EXPECT(hi.y - lo.y >= 0.98 * m.diameter_m && hi.z - lo.z >= 0.98 * m.diameter_m);
                const double reach = RadialReach(mesh), want = MaxRadius(m);
                if (std::fabs(reach - want) > 0.02 * want) std::printf("    %s: reach %.4f m, span/diameter say %.4f m\n", m.id.c_str(), reach, want);
                SC_EXPECT(std::fabs(reach - want) <= 0.02 * want);
                if (mesh.Triangles() > 6000) std::printf("    %s: %zu triangles\n", m.id.c_str(), mesh.Triangles());
                SC_EXPECT(mesh.Triangles() <= 6000);
            } else {
                SC_EXPECT(mesh.Triangles() < previous);
            }
            if (lod == 3) {  // a box as wide as the body, no fins
                const double w = 2.0 * MaxBodyRadius(m);
                SC_EXPECT(std::fabs((hi.y - lo.y) - w) <= 0.02 * w && std::fabs((hi.z - lo.z) - w) <= 0.02 * w);
                // The box's every normal leans out from the axis (or along it, on a disc or a step): a body turned
                // inside out, its triangles wound to match, would fail here and in the volume.
                int inward = 0;
                for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
                    const Vec3& q = mesh.positions[i];
                    inward += Dot(mesh.normals[i], Vec3{0.0, q.y, q.z}) < -1e-9 ? 1 : 0;
                }
                SC_EXPECT(inward == 0);
                // The box holds 4 r^2 a metre of length, less where its chords cut inside a curved nose: at most 4
                // times the integral of r^2 (a plain cylinder's, the improvised bomb's 1.0000), and the least of these
                // shapes, the shells that are mostly nose, 0.89 of it. A piece turned inside out takes twice its part.
                const double box = SignedVolume(mesh) / (4.0 / kPi * LatheVolume(m));
                if (box < 0.85 || box > 1.01) std::printf("    %s: box volume %.4f of the outline's\n", m.id.c_str(), box);
                SC_EXPECT(box >= 0.85 && box <= 1.01);
            }
            // Wound outward: the triangles hold a positive volume. At level 0 about the 24-sided body's (12 sin 15° / pi =
            // 0.98862 of the round one's, the nose's eight chords a little inside its curve; the shells 0.998 of that),
            // and the fins' plates add (up to an eighth more: the AGM-78B's long strakes), so only a floor is set.
            const double volume = SignedVolume(mesh);
            if (!(volume > 0.0)) std::printf("    %s lod %d: volume %.6f m3\n", m.id.c_str(), lod, volume);
            SC_EXPECT(volume > 0.0);
            if (lod == 0) {
                const double ratio = volume / (12.0 * std::sin(DegToRad(15.0)) / kPi * LatheVolume(m));
                if (ratio < 0.97) std::printf("    %s: volume %.4f of the lathe's\n", m.id.c_str(), ratio);
                SC_EXPECT(ratio >= 0.97);
            }
            previous = mesh.Triangles();
        }
        ++checked;
    }
    SC_EXPECT(checked >= 55);
}

SC_TEST("munitions: the colours, bands and the motors' numbers") {
    // sRGB to linear: mid grey, black, white, and a name that is no colour.
    SC_EXPECT_NEAR(LinearColour("#808080")[0], 0.2158605, 1e-6);
    SC_EXPECT_NEAR(LinearColour("#000000")[1], 0.0, 1e-9);
    SC_EXPECT_NEAR(LinearColour("#FFFFFF")[2], 1.0, 1e-6);
    SC_EXPECT_NEAR(LinearColour("olive")[0], 0.2158605, 1e-6);
    const MunitionLibrary lib = Munitions();
    // US bombs olive drab with a yellow band; US missiles white with yellow and brown bands (MIL-STD-709C).
    const MunitionSpec* mk82 = lib.Find("mk82");
    SC_EXPECT(mk82 != nullptr && mk82->colour == lib.colours.at("olive_drab") && mk82->bands.size() == 1 &&
              mk82->bands[0].colour == lib.colours.at("us_yellow"));
    const MunitionSpec* harm = lib.Find("agm88a");
    SC_EXPECT(harm != nullptr && harm->colour == lib.colours.at("missile_white") && harm->bands.size() == 2);
    // The band shows on the mesh: a yellow vertex near the Mk 82's nose, none aft of the middle.
    if (mk82 != nullptr) {
        const TriMesh mesh = BuildMesh(*mk82, 0);
        const std::array<float, 3> yellow = LinearColour(lib.colours.at("us_yellow"));
        int fore = 0, aft = 0;
        for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
            if (mesh.colours[i] != yellow) continue;
            (mesh.positions[i].x > 0.0 ? fore : aft) += 1;
        }
        SC_EXPECT(fore > 0 && aft == 0);
    }
    // Our 9M33M2: its tracers for the TV sight, the dual-thrust motor's burn, its smoke lingering.
    const MunitionSpec* osa = lib.Find("9m33m2");
    SC_EXPECT(osa != nullptr && osa->has_motor && osa->motor.tracer && osa->motor.smoke == "dense");
    if (osa != nullptr) {
        SC_EXPECT(osa->motor.Burn() >= 13.2 && osa->motor.Burn() <= 19.2);
        SC_EXPECT_NEAR(osa->length_m, 3.158, 1e-9);
    }
    // A liquid or ramjet sustainer smokes less than its booster.
    const MunitionSpec* kub = lib.Find("3m9m3");
    SC_EXPECT(kub != nullptr && kub->motor.smoke == "dense" && kub->motor.SustainSmoke() == "minimal");
    // Every smoky motor's trail lingers 30-60 s; every motor has a plume colour; bombs have none.
    for (const MunitionSpec& m : lib.munitions) {
        if (m.has_motor && m.motor.smoke == "dense") SC_EXPECT(m.smoke_s >= 30.0 && m.smoke_s <= 60.0);
        if (m.has_motor) SC_EXPECT(m.motor.plume_colour.size() == 7 && m.motor.Burn() > 0.0);
        if (m.kind == "bomb") SC_EXPECT(!m.has_motor);
        if (m.kind == "cluster") SC_EXPECT(m.has_cluster && m.cluster.submunitions > 0);
    }
}

SC_TEST("munitions: every gun round has its ballistics and tracer") {
    const MunitionLibrary lib = Munitions();
    int rounds = 0;
    for (const MunitionSpec& m : lib.munitions) {
        if (!m.IsRound()) continue;
        ++rounds;
        SC_EXPECT(m.calibre_mm > 0.0 && m.muzzle_mps > 0.0 && m.rate_rpm > 0.0 && m.tracer_s >= 0.0 && m.barrels >= 1);
        if (m.tracer_s > 0.0) SC_EXPECT(m.tracer_colour.size() == 7);
        SC_EXPECT(m.burst == "contact" || m.burst == "time");
        SC_EXPECT(m.sd_hi_s >= m.sd_lo_s);
    }
    SC_EXPECT(rounds >= 8);
    // The 23 mm's self-destruct window and the 57 mm's (docs/fx/README.md), the KS-30's time fuze and no tracer.
    const MunitionSpec* shilka = lib.Resolve("zsu234");
    SC_EXPECT(shilka != nullptr && shilka->barrels == 4);
    if (shilka != nullptr) {
        SC_EXPECT_NEAR(shilka->sd_lo_s, 5.6, 1e-9);
        SC_EXPECT_NEAR(shilka->sd_hi_s, 11.2, 1e-9);
    }
    const MunitionSpec* s60 = lib.Resolve("s60");
    SC_EXPECT(s60 != nullptr && s60->calibre_mm == 57.0 && s60->sd_lo_s == 13.0 && s60->sd_hi_s == 17.0);
    const MunitionSpec* ks30 = lib.Resolve("ks30");
    SC_EXPECT(ks30 != nullptr && ks30->burst == "time" && ks30->tracer_s == 0.0 && ks30->calibre_mm == 130.0);
    SC_EXPECT(lib.Resolve("aaa") != nullptr && lib.Resolve("aaa")->calibre_mm == 23.0);   // the theatres' "aaa" sites are ZU-23s
    SC_EXPECT(lib.Resolve("gau8") != nullptr && lib.Resolve("gau8")->kind == "gun");
}

SC_TEST("munitions: a cylinder and the nose and tail profiles by hand") {
    MunitionLibrary lib;
    std::string e;
    // A cylinder 2 m long, 0.2 m across, flat at both ends: 24 sides, a disc of 24 triangles at each end and 48 round it.
    SC_EXPECT(lib.LoadJson(OneMunition("\"kind\": \"bomb\", \"length_m\": 2.0, \"diameter_m\": 0.2, \"nose\": \"flat\", \"tail\": \"flat\", \"colour\": \"grey\""), &e));
    SC_EXPECT(lib.munitions.size() == 1);
    if (lib.munitions.size() == 1) {
        const TriMesh m0 = BuildMesh(lib.munitions[0], 0);
        SC_EXPECT(m0.Triangles() == 96);
        SC_EXPECT(m0.positions.size() == 6 * 24);  // each piece its own two rings: creases are sharp
        SC_EXPECT_NEAR(m0.colours[0][0], 0.2158605, 1e-6);
        SC_EXPECT(BuildMesh(lib.munitions[0], 1).Triangles() == 48);
        SC_EXPECT(BuildMesh(lib.munitions[0], 2).Triangles() == 24);
        SC_EXPECT(BuildMesh(lib.munitions[0], 3).Triangles() == 16);
        SC_EXPECT(MeshFaults(m0) == 0);
        // The front disc faces forward, the back disc aft.
        int forward = 0, back = 0;
        for (std::size_t i = 0; i < m0.positions.size(); ++i) {
            if (m0.positions[i].x > 0.999 && m0.normals[i].x > 0.999) ++forward;
            if (m0.positions[i].x < -0.999 && m0.normals[i].x < -0.999) ++back;
        }
        SC_EXPECT(forward == 48 && back == 48);
        // Wound outward, from the triangles alone: an n-sided prism of radius 0.1 m holds (n / 2) r^2 sin(2 pi / n) L,
        // 0.0621166 m3 at 24 sides, 0.06 at 12, 0.0519615 at 6; the level-3 box, 0.2 m square, 0.08 m3.
        SC_EXPECT_NEAR(SignedVolume(m0), 12.0 * 0.01 * std::sin(DegToRad(15.0)) * 2.0, 1e-12);
        SC_EXPECT_NEAR(SignedVolume(m0), 0.0621166, 1e-7);
        SC_EXPECT_NEAR(SignedVolume(BuildMesh(lib.munitions[0], 1)), 0.06, 1e-12);
        SC_EXPECT_NEAR(SignedVolume(BuildMesh(lib.munitions[0], 2)), 0.0519615, 1e-7);
        SC_EXPECT_NEAR(SignedVolume(BuildMesh(lib.munitions[0], 3)), 0.08, 1e-12);
    }
    // A white band a quarter of the length long from a quarter back splits the side into three pieces.
    SC_EXPECT(lib.LoadJson(OneMunition("\"kind\": \"bomb\", \"length_m\": 2.0, \"diameter_m\": 0.2, \"nose\": \"flat\", \"tail\": \"flat\", \"colour\": \"grey\", "
                                       "\"bands\": [{\"x\": 0.25, \"w\": 0.25, \"colour\": \"#ffffff\"}]"),
                           &e));
    if (lib.munitions.size() == 1) {
        const TriMesh m0 = BuildMesh(lib.munitions[0], 0);
        SC_EXPECT(m0.Triangles() == 192);
        int white = 0, misplaced = 0;
        for (std::size_t i = 0; i < m0.positions.size(); ++i) {
            if (m0.colours[i][0] < 0.999f) continue;
            ++white;
            misplaced += (m0.positions[i].x < -1e-9 || m0.positions[i].x > 0.5 + 1e-9) ? 1 : 0;  // 0.5-1.0 m aft of the nose
        }
        SC_EXPECT(white == 48 && misplaced == 0);
    }
    // A tangent ogive 0.4 m long on a 0.1 m radius: rho = (R^2 + n^2) / 2R = 0.85 m; 0.2 m aft of the tip
    // r = sqrt(rho^2 - (n - s)^2) + R - rho = sqrt(0.6825) - 0.75 = 0.0761356 m. A boattail 0.5 m long to half the
    // radius: 0.25 m into it, 0.075 m.
    SC_EXPECT(lib.LoadJson(OneMunition("\"kind\": \"bomb\", \"length_m\": 2.0, \"diameter_m\": 0.2, \"nose\": \"ogive\", \"nose_l\": 0.2, "
                                       "\"tail\": \"boattail\", \"tail_l\": 0.25, \"tail_r\": 0.5, \"colour\": \"grey\""),
                           &e));
    if (lib.munitions.size() == 1) {
        const MunitionSpec& m = lib.munitions[0];
        SC_EXPECT_NEAR(BodyRadius(m, 0.0), 0.0, 1e-9);
        SC_EXPECT_NEAR(BodyRadius(m, 0.2), std::sqrt(0.6825) - 0.75, 1e-9);
        SC_EXPECT_NEAR(BodyRadius(m, 0.4), 0.1, 1e-9);
        SC_EXPECT_NEAR(BodyRadius(m, 1.0), 0.1, 1e-9);
        SC_EXPECT_NEAR(BodyRadius(m, 1.75), 0.075, 1e-9);
        SC_EXPECT_NEAR(BodyRadius(m, 2.0), 0.05, 1e-9);
        SC_EXPECT_NEAR(BodyRadius(m, 2.1), 0.0, 1e-9);
        // The mesh's vertices lie on that outline.
        const TriMesh mesh = BuildMesh(m, 0);
        double worst = 0.0;
        for (const Vec3& p : mesh.positions) {
            const double s = 1.0 - p.x, r = std::sqrt(p.y * p.y + p.z * p.z);
            if (r > 1e-9 && s > 1e-6 && s < 2.0 - 1e-6) worst = std::max(worst, std::fabs(r - BodyRadius(m, s)));
        }
        SC_EXPECT(worst < 1e-9);
        SC_EXPECT(MeshFaults(mesh) == 0);
        // At level 3 the nose is one chord, a cone 0.4 m long, and the box is 2r square: its volume is 4 (the integral
        // of r^2) = 4 (0.4 x 0.01 / 3 + 1.1 x 0.01 + 0.5 (0.01 + 0.005 + 0.0025) / 3) = 0.061 m3. A nose whose normals
        // turned in (and its triangles with them, as the builder winds them) would change it.
        SC_EXPECT_NEAR(SignedVolume(BuildMesh(m, 3)), 0.061, 1e-12);
    }
    // A hemisphere 0.1 m long on a 0.1 m radius: halfway along it, r = 0.1 sqrt(0.75).
    SC_EXPECT(lib.LoadJson(OneMunition("\"kind\": \"bomb\", \"length_m\": 2.0, \"diameter_m\": 0.2, \"nose\": \"hemisphere\", \"nose_l\": 0.05, \"colour\": \"grey\""), &e));
    if (lib.munitions.size() == 1) SC_EXPECT_NEAR(BodyRadius(lib.munitions[0], 0.05), 0.1 * std::sqrt(0.75), 1e-9);
    // A booster wider than its sustainer: the step faces forward and the box at level 3 is the booster's width.
    SC_EXPECT(lib.LoadJson(OneMunition("\"kind\": \"sam\", \"length_m\": 4.0, \"diameter_m\": 0.2, \"nose\": \"conical\", \"nose_l\": 0.1, "
                                       "\"sections\": [{\"x\": 0.6, \"diameter_m\": 0.3}], \"colour\": \"grey\", "
                                       "\"fins\": [{\"count\": 4, \"span_m\": 0.8, \"root_chord_m\": 0.4, \"tip_chord_m\": 0.2, \"x\": 0.85, \"sweep_deg\": 30}]"),
                           &e));
    if (lib.munitions.size() == 1) {
        const MunitionSpec& m = lib.munitions[0];
        SC_EXPECT_NEAR(BodyRadius(m, 2.0), 0.1, 1e-9);
        SC_EXPECT_NEAR(BodyRadius(m, 3.0), 0.15, 1e-9);
        int step = 0;
        const TriMesh mesh = BuildMesh(m, 0);
        for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
            if (std::fabs(mesh.positions[i].x - (2.0 - 2.4)) < 1e-9 && mesh.normals[i].x > 0.999) ++step;
        }
        SC_EXPECT(step == 48);
        SC_EXPECT_NEAR(RadialReach(mesh), 0.4, 0.4 * 0.02);
        Vec3 lo, hi;
        BuildMesh(m, 3).Bounds(lo, hi);
        SC_EXPECT_NEAR(hi.y - lo.y, 0.3, 1e-9);
        SC_EXPECT_NEAR(hi.z - lo.z, 0.3, 1e-9);
    }
}

SC_TEST("munitions: the loader refuses what it does not know") {
    MunitionLibrary lib;
    std::string e;
    const std::string good = "\"kind\": \"bomb\", \"length_m\": 2.0, \"diameter_m\": 0.2, \"colour\": \"grey\", \"nose\": \"flat\"";
    // A chute's descent under it loads (ended with the number and "}"); blades take none.
    const std::string chute_head = "\"kind\": \"parachute\", \"length_m\": 2.0, \"diameter_m\": 0.2, \"colour\": \"grey\", \"nose\": \"flat\", "
                               "\"retarder\": {\"type\": \"chute\", \"area_m2\": 3, \"descent_mps\": ";
    const std::string blades_head = "\"kind\": \"retarded\", \"length_m\": 2.0, \"diameter_m\": 0.2, \"colour\": \"grey\", \"nose\": \"flat\", "
                                "\"fins\": [{\"span_m\": 0.5, \"root_chord_m\": 0.2, \"tip_chord_m\": 0.1, \"x\": 0.8, \"blades\": true}], "
                                "\"retarder\": {\"type\": \"blades\", \"length_m\": 0.5, \"width_m\": 0.2";
    MunitionLibrary chute_lib;
    SC_EXPECT(chute_lib.LoadJson(OneMunition(chute_head + "40}"), &e) && chute_lib.munitions.size() == 1 &&
              chute_lib.munitions[0].retarder.descent_mps == 40.0);
    SC_EXPECT(chute_lib.LoadJson(OneMunition(blades_head + "}"), &e) && chute_lib.munitions[0].retarder.descent_mps == 0.0);
    SC_EXPECT(lib.LoadJson(OneMunition(good), &e));
    const std::vector<std::string> bad = {
        OneMunition(good + ", \"wingspan\": 1"),                                          // an unknown key
        OneMunition("\"kind\": \"zeppelin\", \"length_m\": 2.0, \"diameter_m\": 0.2, \"colour\": \"grey\""),  // an unknown kind
        OneMunition("\"kind\": \"bomb\", \"length_m\": 2.0, \"diameter_m\": 0.2, \"colour\": \"puce\""),     // an unknown colour
        OneMunition("\"kind\": \"bomb\", \"length_m\": 2.0, \"colour\": \"grey\""),                         // no diameter
        OneMunition("\"kind\": \"bomb\", \"length_m\": 2.0, \"diameter_m\": 0.2, \"colour\": \"grey\", \"nose\": \"ogive\""),  // a nose with no length
        OneMunition(good + ", \"fins\": [{\"span_m\": 0.1, \"root_chord_m\": 0.2, \"tip_chord_m\": 0.1, \"x\": 0.8}]"),  // tips inside the body
        OneMunition(good + ", \"fins\": [{\"span_m\": 0.5, \"root_chord_m\": 0.6, \"tip_chord_m\": 0.1, \"x\": 0.8}]"),  // past the tail
        OneMunition(good + ", \"fins\": [{\"span_m\": 0.5, \"root_chord_m\": 0.2, \"tip_chord_m\": 0.1, \"x\": 0.5, \"cant\": 2}]"),  // a fin's unknown key
        OneMunition("\"kind\": \"retarded\", \"length_m\": 2.0, \"diameter_m\": 0.2, \"colour\": \"grey\""),  // a retarded bomb with no retarder
        OneMunition(good + ", \"retarder\": {\"type\": \"chute\", \"area_m2\": 3}"),     // a retarder on a plain bomb
        OneMunition(chute_head + "-5}"),                                                           // a negative descent
        OneMunition(blades_head + ", \"descent_mps\": 40}"),                                       // blades don't descend
        OneMunition(good + ", \"cluster\": {\"submunitions\": 10}"),                        // a cluster on a plain bomb
        OneMunition(good + ", \"motor\": {\"boost_s\": 1, \"smoke\": \"thick\"}"),          // an unknown smoke
        OneMunition("\"kind\": \"shell\", \"length_m\": 0.1, \"diameter_m\": 0.023, \"colour\": \"grey\", \"calibre_mm\": 23, \"rate_rpm\": 100, \"tracer_s\": 0"),  // no muzzle velocity
        OneMunition("\"kind\": \"gun\", \"calibre_mm\": 30, \"muzzle_mps\": 1000, \"rate_rpm\": 3900, \"tracer_s\": 3"),  // a tracer with no colour
        OneMunition(good + ", \"bands\": [{\"x\": 0.9, \"w\": 0.2, \"colour\": \"grey\"}]"),  // a band past the tail
        "{\"munitions\": [{\"id\": \"test\", \"name\": \"Test\", " + good + "}]}",         // no source
        "{\"weapons\": {\"mk82\": \"test\"}, \"munitions\": [{\"id\": \"test\", \"name\": \"Test\", " + good +
            ", \"source\": \"placeholder\", \"confidence\": \"estimate\"}], \"systems\": {\"osa\": \"nothing\"}}",  // a map to no munition
        "{\"munitions\": [], \"extra\": 1}",                                                   // an unknown key at the top
        "{\"munitions\": []}",                                                                 // none at all
        "not json",
    };
    for (std::size_t i = 0; i < bad.size(); ++i) {
        std::string err;
        const bool ok = lib.LoadJson(bad[i], &err);
        if (ok) std::printf("    case %zu loaded\n", i);
        SC_EXPECT(!ok && !err.empty());
    }
    // A failed load leaves the library as it was.
    SC_EXPECT(lib.munitions.size() == 1 && lib.munitions[0].id == "test");
    SC_EXPECT(!lib.Load(kData + "/scenery/no_such_file.json", &e) && !e.empty());
}

SC_TEST("munitions: the parts: Snakeye blades, folded wings, a chute and a ballute") {
    const MunitionLibrary lib = Munitions();
    SC_EXPECT(PartReplacesBody("blades_open") && PartReplacesBody("wings_folded") && !PartReplacesBody("chute") && !PartReplacesBody("ballute"));
    // The Snakeye: closed, it is no wider than its fins; open, the blades stand out from the tail's end in an X.
    const MunitionSpec* snakeye = lib.Find("mk82r");
    SC_EXPECT(snakeye != nullptr && snakeye->has_retarder && snakeye->retarder.type == "blades");
    if (snakeye != nullptr) {
        const std::vector<std::string> parts = PartsOf(*snakeye);
        SC_EXPECT(parts.size() == 1 && parts[0] == "blades_open");
        const TriMesh closed = BuildMesh(*snakeye, 0), open = BuildPart(*snakeye, "blades_open", 0);
        SC_EXPECT(MeshFaults(open) == 0);
        SC_EXPECT(RadialReach(closed) < 0.3);
        SC_EXPECT(RadialReach(open) > snakeye->retarder.length_m);
        Vec3 lo, hi;
        open.Bounds(lo, hi);
        SC_EXPECT(lo.x > -0.5 * snakeye->length_m - 0.02 && hi.x <= 0.5 * snakeye->length_m + 1e-9);
        SC_EXPECT(BuildPart(*snakeye, "chute", 0).Empty() && BuildPart(*snakeye, "nonsense", 0).Empty());
        SC_EXPECT(BuildPart(*snakeye, "blades_open", 3).Triangles() < open.Triangles());
    }
    const MunitionSpec* mk82 = lib.Find("mk82");
    SC_EXPECT(mk82 != nullptr && PartsOf(*mk82).empty() && BuildPart(*mk82, "blades_open", 0).Empty());
    // The 9M33M2's wings fold in pairs for its container: folded, it is narrower than with them open.
    const MunitionSpec* osa = lib.Find("9m33m2");
    SC_EXPECT(osa != nullptr);
    if (osa != nullptr) {
        const std::vector<std::string> parts = PartsOf(*osa);
        SC_EXPECT(std::find(parts.begin(), parts.end(), "wings_folded") != parts.end());
        const TriMesh folded = BuildPart(*osa, "wings_folded", 0);
        SC_EXPECT(MeshFaults(folded) == 0);
        const double reach = RadialReach(folded);
        SC_EXPECT(reach < 0.8 * MaxRadius(*osa) && reach > 0.5 * osa->diameter_m);
        Vec3 lo, hi;
        folded.Bounds(lo, hi);
        SC_EXPECT_NEAR(hi.x - lo.x, osa->length_m, 1e-9);
    }
    // The Durandal's chute open behind its tail on its lines. Its area is the canopy's nominal (cloth) area: open, the
    // canopy spreads to 0.67 of the nominal diameter (Knacke), so 3 m2 is drawn 0.67 sqrt(3 / pi) = 0.6547 m in radius.
    const MunitionSpec* durandal = lib.Find("durandal");
    SC_EXPECT(durandal != nullptr && durandal->kind == "parachute");
    if (durandal != nullptr) {
        const TriMesh chute = BuildPart(*durandal, "chute", 0);
        SC_EXPECT(!chute.Empty() && MeshFaults(chute) == 0);
        Vec3 lo, hi;
        chute.Bounds(lo, hi);
        SC_EXPECT(lo.x < -0.5 * durandal->length_m - durandal->retarder.length_m);
        SC_EXPECT_NEAR(durandal->retarder.area_m2, 3.0, 1e-12);
        SC_EXPECT_NEAR(RadialReach(chute), 0.6547, 0.01);
        SC_EXPECT_NEAR(RadialReach(chute), kChuteInflatedShare * std::sqrt(durandal->retarder.area_m2 / kPi), 0.01);
        SC_EXPECT(BuildPart(*durandal, "chute", 3).Triangles() < chute.Triangles());
        SC_EXPECT(durandal->retarder.descent_mps == 0.0);  // not given: the effects keep their 47 m/s
    }
    // The FAB-500ShN's 6 m2 chute (airwar_ru): 2.76 m across nominally, drawn 1.85 m; its descent of 47 m/s (airwar_ru),
    // the rate the simulation times its fall by (air_war/weapon_flight.hpp's kChuteDescentMps).
    const MunitionSpec* fab = lib.Find("fab500sh");
    SC_EXPECT(fab != nullptr && fab->has_retarder && fab->retarder.type == "chute");
    if (fab != nullptr) {
        SC_EXPECT_NEAR(2.0 * RadialReach(BuildPart(*fab, "chute", 0)), 1.852, 0.02);
        SC_EXPECT_NEAR(fab->retarder.descent_mps, 47.0, 1e-12);
        SC_EXPECT_NEAR(fab->retarder.descent_mps, airwar::weapon_flight::kChuteDescentMps, 1e-12);
    }
    // Until the simulation reads a munition's own descent, no munition may give another: the drawn bomb would plunge
    // or hang against the impact time the simulation sets.
    for (const MunitionSpec& m : lib.munitions) {
        SC_EXPECT(m.retarder.descent_mps == 0.0 || m.retarder.descent_mps == airwar::weapon_flight::kChuteDescentMps);
    }
    // The Mk 82 AIR's ballute.
    const MunitionSpec* air = lib.Find("mk82air");
    SC_EXPECT(air != nullptr && air->kind == "ballute");
    if (air != nullptr) {
        const TriMesh ballute = BuildPart(*air, "ballute", 0);
        SC_EXPECT(!ballute.Empty() && MeshFaults(ballute) == 0);
        Vec3 lo, hi;
        ballute.Bounds(lo, hi);
        SC_EXPECT(lo.x < -0.5 * air->length_m - air->retarder.length_m);
        SC_EXPECT_NEAR(RadialReach(ballute), 0.5 * air->retarder.diameter_m, 0.02);
    }
    // The Kh-22's ventral fin folds for carriage under the bomber.
    const MunitionSpec* kh22 = lib.Find("kh22m");
    SC_EXPECT(kh22 != nullptr);
    if (kh22 != nullptr) {
        Vec3 lo, hi, flo, fhi;
        BuildMesh(*kh22, 0).Bounds(lo, hi);
        BuildPart(*kh22, "wings_folded", 0).Bounds(flo, fhi);
        SC_EXPECT(flo.z > lo.z + 0.5);  // the fin no longer hangs below
    }
}
