// The ground clutter of the camera's ring (simcore/scenery/clutter.hpp, roadmap T6.1): the point layer, the plants a cell
// draws, the ring's density, determinism, the layer under every plant, and the loader's refusals.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include "simcore/scenery/clutter.hpp"
#include "simcore/terrain_map.hpp"
#include "test_framework.hpp"

using namespace simcore;
using namespace simcore::scenery;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

const DressingRules& Dressing() {
    static DressingRules rules;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string e;
        if (!LoadDressingRules(kData + "/scenery/map_dressing.json", rules, &e)) std::printf("    %s\n", e.c_str());
    }
    return rules;
}

const GroundLayerRules& Layers() {
    static GroundLayerRules rules;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string e;
        if (!LoadGroundLayers(kData + "/scenery/ground_layers.json", Dressing(), rules, &e)) std::printf("    %s\n", e.c_str());
    }
    return rules;
}

const ClutterRules& Rules() {
    static ClutterRules rules;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string e;
        if (!LoadClutterRules(kData + "/scenery/clutter.json", rules, &e) || !LoadClutterRecipes(kData + "/scenery/recipes", Layers(), rules, &e)) {
            std::printf("    %s\n", e.c_str());
        }
    }
    return rules;
}

int LayerId(const std::string& id) {
    for (std::size_t i = 0; i < Layers().layers.size(); ++i) {
        if (Layers().layers[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

const CoverRaster& Raster() {
    static CoverRaster raster;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string e;
        const std::shared_ptr<const MapData> map = LoadMapCached(kData, "maps/kesmark_range.json", &e);
        if (map) raster = BakeCover(*map, Dressing());
        else std::printf("    %s\n", e.c_str());
    }
    return raster;
}

constexpr int kDay = 288;  // 15 October

}  // namespace

SC_TEST("clutter: the point layer is the mask's layer at every cell centre") {
    const CoverRaster& cover = Raster();
    SC_EXPECT(!cover.grids.empty());
    if (cover.grids.empty()) return;
    const CoverGrid& g = cover.grids.back();
    const GroundMask mask = BakeGroundMask(g, Dressing(), Layers(), kDay);
    int looked = 0, same = 0;
    for (int r = 0; r < g.rows; r += 23) {
        for (int c = 0; c < g.cols; c += 23) {
            ++looked;
            same += PointLayer(cover, Dressing(), Layers(), g.CentreEast(c), g.CentreNorth(r), kDay) == static_cast<int>(mask.At(c, r)[0]) ? 1 : 0;
        }
    }
    std::printf("    the point layer against the mask: %d of %d cell centres\n", same, looked);
    SC_EXPECT(looked > 100);
    SC_EXPECT(same == looked);
    SC_EXPECT(PointLayer(cover, Dressing(), Layers(), 1e9, 1e9, kDay) == -1);  // off the map
}

// The kinds a layer's recipe scatters, in any band.
static bool Listed(const ClutterRules& rules, int layer, int kind) {
    const Recipe* r = rules.recipes.ForLayer(layer);
    if (r == nullptr) return false;
    for (const RecipeBand& b : r->bands) {
        for (const RecipeScatter& s : b.scatter) {
            if (s.kind_index == kind) return true;
        }
    }
    return false;
}

SC_TEST("clutter: the rules load, and the ring is deterministic, inside the ring and on the layer its kind is listed for") {
    const ClutterRules& rules = Rules();
    // grass in two variants, tall grass in two, straw, reed, the plough's clod, the maize's lying stalk, the standing
    // maize plant's six cards, the sunflower's three and the wheat's swath
    SC_EXPECT(!rules.kinds.empty() && rules.parts.size() == 18);
    // Find a few points on different layers near the battery and ring each.
    const double centres[][2] = {{5750.0, -9050.0}, {-9000.0, -6000.0}, {2500.0, -12500.0}, {500.0, 500.0}};
    for (const auto& c : centres) {
        std::vector<ClutterPlant> a, b;
        ClutterAround(rules, Raster(), Dressing(), Layers(), kDay, c[0], c[1], a);
        ClutterAround(rules, Raster(), Dressing(), Layers(), kDay, c[0], c[1], b);
        SC_EXPECT(a.size() == b.size());
        for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) {
            SC_EXPECT(a[i].east_m == b[i].east_m && a[i].north_m == b[i].north_m && a[i].height_m == b[i].height_m);
        }
        std::size_t wrong = 0;
        for (const ClutterPlant& p : a) {
            SC_EXPECT(std::hypot(p.east_m - c[0], p.north_m - c[1]) <= rules.ring_m + 1e-6);
            const int layer = PointLayer(Raster(), Dressing(), Layers(), p.east_m, p.north_m, kDay);
            wrong += Listed(rules, layer, p.kind) ? 0 : 1;
            const ClutterKind& k = rules.kinds[p.kind];
            SC_EXPECT(p.part < rules.parts.size() && std::find(k.variants.begin(), k.variants.end(), rules.parts[p.part]) != k.variants.end());
            const float stand = k.to_stand ? static_cast<float>(Layers().TallestVaried()) : 1.0f;  // a share of its field's stand
            SC_EXPECT(p.height_m > 0.0f && p.height_m <= k.height_max_m * stand + 1e-5f);
            if (!k.to_stand) SC_EXPECT(p.height_m >= k.height_min_m - 1e-6f);
            // Wider farther out, standing for the plants the falloff thinned round it (not a near kind).
            const double dp = std::hypot(p.east_m - c[0], p.north_m - c[1]);
            const double widen = k.near_m > 0.0 ? 1.0 : std::min(rules.widen_max, std::sqrt(1.0 + (dp / rules.falloff_m) * (dp / rules.falloff_m)));
            SC_EXPECT(p.width_m >= k.width_min_m - 1e-6f && p.width_m <= static_cast<float>(k.width_max_m * widen) + 1e-5f);
        }
        SC_EXPECT(wrong == 0);
        std::printf("    ring at (%.0f, %.0f): %zu plants\n", c[0], c[1], a.size());
    }
}

SC_TEST("clutter: a plant is the same plant wherever the ring is (moving it thins and thickens, it does not reshuffle)") {
    const ClutterRules& rules = Rules();
    std::vector<ClutterPlant> a, b;
    ClutterAround(rules, Raster(), Dressing(), Layers(), kDay, 5750.0, -9050.0, a);
    ClutterAround(rules, Raster(), Dressing(), Layers(), kDay, 5766.0, -9050.0, b);  // 16 m east: the rebuild step
    // A plant of the first ring within 40 m of both centres: if the second ring has it, it is the same plant. Its width
    // alone changes, by the widening at its new distance (A.3.40: a plant farther out stands for those thinned round it).
    const auto widen = [&](const ClutterPlant& p, double ce) {
        if (rules.kinds[p.kind].near_m > 0.0) return 1.0;
        const double d = std::hypot(p.east_m - ce, p.north_m + 9050.0) / rules.falloff_m;
        return std::min(rules.widen_max, std::sqrt(1.0 + d * d));
    };
    int near_a = 0, in_both = 0, differ = 0;
    for (const ClutterPlant& p : a) {
        if (std::hypot(p.east_m - 5750.0, p.north_m + 9050.0) > 40.0 || std::hypot(p.east_m - 5766.0, p.north_m + 9050.0) > 40.0) continue;
        ++near_a;
        for (const ClutterPlant& q : b) {
            if (q.east_m == p.east_m && q.north_m == p.north_m) {
                ++in_both;
                const double base_p = p.width_m / widen(p, 5750.0), base_q = q.width_m / widen(q, 5766.0);
                differ += (q.height_m != p.height_m || std::fabs(base_q - base_p) > 1e-5 || q.kind != p.kind || q.yaw_rad != p.yaw_rad ||
                           q.tint != p.tint)
                              ? 1
                              : 0;
                break;
            }
        }
    }
    std::printf("    %d plants near both centres, %d in both rings\n", near_a, in_both);
    SC_EXPECT(near_a > 100);
    SC_EXPECT(differ == 0);
    SC_EXPECT(in_both * 10 >= near_a * 6);  // most of them stay: the density changes little over 16 m
}

SC_TEST("clutter: the density falls with the distance, a meadow is thick, a ploughed field has clods only, and the ring is within budget") {
    const ClutterRules& rules = Rules();
    // Ring a spot of the map, count by distance band: nearer bands are denser per square metre.
    std::vector<ClutterPlant> all;
    const auto t0 = std::chrono::steady_clock::now();
    ClutterAround(rules, Raster(), Dressing(), Layers(), kDay, 5750.0, -9050.0, all);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    double inner = 0.0, outer = 0.0;
    for (const ClutterPlant& p : all) {
        const double d = std::hypot(p.east_m - 5750.0, p.north_m + 9050.0);
        if (d < 30.0) inner += 1.0 / (3.14159265 * 30.0 * 30.0);
        else if (d > 70.0) outer += 1.0 / (3.14159265 * (100.0 * 100.0 - 70.0 * 70.0));
    }
    std::printf("    the ring at the battery: %zu plants in %.1f ms, %.3f a m2 within 30 m, %.3f beyond 70 m\n", all.size(), ms, inner, outer);
    SC_EXPECT(all.size() < 20000);  // the ring's instance budget
    SC_EXPECT(inner > outer);
    // Layers: plants only on layers a recipe applies to; standing wheat has none (the crop blocks are L5's).
    const int ploughed = LayerId("ploughed"), sown = LayerId("sown"), wheat = LayerId("wheat");
    SC_EXPECT(rules.recipes.ForLayer(wheat) == nullptr);
    // The plough's fields' bodies have clods and nothing else, a ploughed field thicker with them than a sown one (plot lab
    // p01); weeds only in the strip at the edge.
    double body[2] = {0.0, 0.0};
    for (int i = 0; i < 2; ++i) {
        const Recipe* r = rules.recipes.ForLayer(i == 0 ? ploughed : sown);
        SC_EXPECT(r != nullptr);
        if (r == nullptr) continue;
        const RecipeBand& b = r->bands.back();
        SC_EXPECT(b.whole && b.scatter.size() == 1);
        for (const RecipeScatter& s : b.scatter) {
            SC_EXPECT(rules.kinds[static_cast<std::size_t>(s.kind_index)].part == "clutter_clod");
            body[i] = s.per_ha;
        }
    }
    SC_EXPECT(body[0] > body[1]);
    SC_EXPECT(rules.recipes.ForLayer(LayerId("meadow")) != nullptr);
}

SC_TEST("clutter: standing maize is drawn in full within its near_m and not beyond, as tall as its field stands") {
    const ClutterRules& rules = Rules();
    int maize = -1;
    for (std::size_t i = 0; i < rules.kinds.size(); ++i) {
        if (rules.kinds[i].id == "maize") maize = static_cast<int>(i);
    }
    SC_EXPECT(maize >= 0);
    if (maize < 0) return;
    const ClutterKind& k = rules.kinds[static_cast<std::size_t>(maize)];
    SC_EXPECT(k.near_m > 0.0 && k.to_stand);
    SC_EXPECT(rules.crop_fade_to_m > rules.crop_fade_from_m && rules.crop_fade_to_m + rules.rebuild_m <= k.near_m);
    // The middle of a dry maize field near the battery: a cell centre with its whole 50 m square on the layer and the crop
    // (a dry_stalks field may be sunflower's).
    const int dry = LayerId("dry_stalks");
    const int maize_crop = Layers().CropIndex("maize");
    SC_EXPECT(maize_crop >= 0);
    const CoverRaster& cover = Raster();
    const auto dry_maize = [&](double e, double n) {
        if (PointLayer(cover, Dressing(), Layers(), e, n, kDay) != dry) return false;
        FieldCell f;
        return ParcelAt(cover, e, n, f) && CropOf(Dressing(), f.id) == maize_crop;
    };
    double ce = 0.0, cn = 0.0, best = 1e300;
    for (const CoverGrid& g : cover.grids) {
        for (int r = 0; r < g.rows; r += 2) {
            for (int c = 0; c < g.cols; c += 2) {
                const double e = g.CentreEast(c), n = g.CentreNorth(r);
                const double d = std::hypot(e - 8000.0, n + 8000.0);
                if (d >= best || !dry_maize(e, n)) continue;
                bool whole = true;
                for (double de = -25.0; de <= 25.0 && whole; de += 12.5) {
                    for (double dn = -25.0; dn <= 25.0 && whole; dn += 12.5) whole = dry_maize(e + de, n + dn);
                }
                if (whole) best = d, ce = e, cn = n;
            }
        }
    }
    SC_EXPECT(best < 1e300);
    if (best >= 1e300) return;
    std::vector<ClutterPlant> all;
    const auto t0 = std::chrono::steady_clock::now();
    ClutterAround(rules, cover, Dressing(), Layers(), kDay, ce, cn, all);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::size_t plants = 0, beyond = 0, wrong_height = 0, within_25 = 0;
    std::vector<std::size_t> per_card(k.variants.size(), 0);
    for (const ClutterPlant& p : all) {
        if (p.kind != maize) continue;
        ++plants;
        for (std::size_t v = 0; v < k.variants.size(); ++v) per_card[v] += rules.parts[p.part] == k.variants[v] ? 1 : 0;
        const double d = std::hypot(p.east_m - ce, p.north_m - cn);
        beyond += d > k.near_m + 1e-9 ? 1 : 0;
        within_25 += d <= 25.0 ? 1 : 0;
        FieldCell f;
        const double stand = ParcelAt(cover, p.east_m, p.north_m, f) ? Layers().StandAtPoint(CropOf(Dressing(), f.id), f.id, kDay, p.east_m, p.north_m) : 0.0;
        wrong_height += (p.height_m < k.height_min_m * stand - 1e-4 || p.height_m > k.height_max_m * stand + 1e-4) ? 1 : 0;
    }
    // Within 25 m the square is the field's (its 50 m square is): drawn in full, at the rows' density.
    const double per_ha = static_cast<double>(within_25) / (kPi * 25.0 * 25.0 / 1e4);
    std::printf("    a dry_stalks field at (%.0f, %.0f): %zu maize plants in the ring (%zu in all, %.1f ms), %.0f a hectare within 25 m\n", ce, cn,
                plants, all.size(), ms, per_ha);
    SC_EXPECT(plants > 1000);
    SC_EXPECT(beyond == 0);
    SC_EXPECT(wrong_height == 0);
    SC_EXPECT(per_ha > 23000.0 && per_ha < 30000.0);
    // Its cards in even shares (each within a fifth of its share either way).
    SC_EXPECT(k.variants.size() == 6);
    std::printf("    its cards:");
    for (std::size_t v = 0; v < per_card.size(); ++v) {
        std::printf(" %zu", per_card[v]);
        const double share = static_cast<double>(per_card[v]) * static_cast<double>(per_card.size()) / static_cast<double>(plants);
        SC_EXPECT(share > 0.8 && share < 1.2);
    }
    std::printf("\n");
    SC_EXPECT(all.size() < 45000);  // the ring's budget with a standing crop all round
}

SC_TEST("clutter: a meadow's grass is greener and drier in patches, and farther out each clump is wider") {
    const ClutterRules& rules = Rules();
    int grass = -1;
    for (std::size_t i = 0; i < rules.kinds.size(); ++i) {
        if (rules.kinds[i].id == "grass") grass = static_cast<int>(i);
    }
    SC_EXPECT(grass >= 0 && rules.widen_max > 1.0);
    if (grass < 0) return;
    const ClutterKind& k = rules.kinds[static_cast<std::size_t>(grass)];
    SC_EXPECT(k.tints.size() >= 2);
    // A meadow near the battery: the cell centre nearest it with its whole 60 m square on the meadow layer.
    const int meadow = LayerId("meadow");
    const CoverRaster& cover = Raster();
    double ce = 0.0, cn = 0.0, best = 1e300;
    for (const CoverGrid& g : cover.grids) {
        for (int r = 0; r < g.rows; r += 2) {
            for (int c = 0; c < g.cols; c += 2) {
                const double e = g.CentreEast(c), n = g.CentreNorth(r);
                const double d = std::hypot(e - 5750.0, n + 9050.0);
                if (d >= best || PointLayer(cover, Dressing(), Layers(), e, n, kDay) != meadow) continue;
                bool whole = true;
                for (double de = -30.0; de <= 30.0 && whole; de += 10.0) {
                    for (double dn = -30.0; dn <= 30.0 && whole; dn += 10.0) whole = PointLayer(cover, Dressing(), Layers(), e + de, n + dn, kDay) == meadow;
                }
                if (whole) best = d, ce = e, cn = n;
            }
        }
    }
    SC_EXPECT(best < 1e300);
    if (best >= 1e300) return;
    std::vector<ClutterPlant> all;
    ClutterAround(rules, cover, Dressing(), Layers(), kDay, ce, cn, all);
    // Within 30 m (the meadow's): every tint between the kind's ends, a spread of them, and neighbours alike.
    std::vector<const ClutterPlant*> close_by;
    std::size_t outside = 0, untinted_other = 0;
    double width_in = 0.0, width_out = 0.0;
    int n_in = 0, n_out = 0;
    for (const ClutterPlant& p : all) {
        const double d = std::hypot(p.east_m - ce, p.north_m - cn);
        if (p.kind != grass) {
            const ClutterKind& o = rules.kinds[static_cast<std::size_t>(p.kind)];
            untinted_other += o.tints.empty() && (p.tint[0] != 1.0f || p.tint[1] != 1.0f || p.tint[2] != 1.0f) ? 1 : 0;
            continue;
        }
        for (std::size_t ch = 0; ch < 3; ++ch) {
            float lo = 10.0f, hi = 0.0f;
            for (const auto& t : k.tints) lo = std::min(lo, t[ch]), hi = std::max(hi, t[ch]);
            outside += p.tint[ch] < lo - 1e-6f || p.tint[ch] > hi + 1e-6f ? 1 : 0;
        }
        if (d < 30.0) close_by.push_back(&p), width_in += p.width_m, ++n_in;
        else if (d > 70.0) width_out += p.width_m, ++n_out;
    }
    SC_EXPECT(outside == 0 && untinted_other == 0);
    SC_EXPECT(close_by.size() > 500 && n_out > 50);
    if (close_by.size() < 2 || n_out == 0) return;
    // The green channel's spread, and the gap between neighbours (within 1.5 m) against pairs drawn anywhere.
    float g_lo = 10.0f, g_hi = 0.0f;
    for (const ClutterPlant* p : close_by) g_lo = std::min(g_lo, p->tint[0]), g_hi = std::max(g_hi, p->tint[0]);
    double gap_near = 0.0, gap_any = 0.0;
    int pairs_near = 0, pairs_any = 0;
    for (std::size_t i = 0; i < close_by.size(); i += 3) {
        const ClutterPlant& a = *close_by[i];
        const ClutterPlant& b = *close_by[(i * 7919 + 13) % close_by.size()];
        gap_any += std::fabs(a.tint[0] - b.tint[0]), ++pairs_any;
        for (std::size_t j = i + 1; j < close_by.size() && j < i + 60; ++j) {
            if (std::hypot(close_by[j]->east_m - a.east_m, close_by[j]->north_m - a.north_m) < 1.5) gap_near += std::fabs(a.tint[0] - close_by[j]->tint[0]), ++pairs_near;
        }
    }
    gap_near /= std::max(1, pairs_near);
    gap_any /= std::max(1, pairs_any);
    std::printf("    a meadow at (%.0f, %.0f): %zu clumps within 30 m, red tint %.2f..%.2f, neighbours differ %.3f, any two %.3f; width %.2f m within 30 m, %.2f beyond 70\n",
                ce, cn, close_by.size(), g_lo, g_hi, gap_near, gap_any, width_in / n_in, width_out / n_out);
    SC_EXPECT(g_hi - g_lo > 0.25f);  // green to straw
    SC_EXPECT(pairs_near > 50 && gap_near < 0.8 * gap_any);
    SC_EXPECT(width_out / n_out > 1.5 * width_in / n_in);
}

SC_TEST("clutter: the loader refuses what is wrong") {
    ClutterRules r;
    std::string e;
    const std::string kinds = "\"kinds\": {\"grass\": {\"part\": \"clutter_grass\", \"height_m\": [0.1, 0.3], \"width_m\": [0.1, 0.3], \"brightness\": 0.1}}";
    SC_EXPECT(LoadClutterRules("{" + kinds + "}", r, &e));
    SC_EXPECT(r.kinds[0].along_rows_deg < 0.0);  // any way, unless told
    SC_EXPECT(!LoadClutterRules("{\"kinds\": {\"grass\": {\"part\": \"clutter_grass\", \"height_m\": [0.1, 0.3], \"width_m\": [0.1, 0.3], \"brightness\": 0.1, \"along_rows_deg\": 120}}}", r, &e));
    SC_EXPECT(e.find("along_rows_deg") != std::string::npos);
    const std::string maize = "\"maize\": {\"part\": \"clutter_maize\", \"height_m\": [0.9, 1.05], \"width_m\": [0.5, 0.8], \"brightness\": 0.1";
    SC_EXPECT(LoadClutterRules("{\"kinds\": {" + maize + ", \"near_m\": 60, \"to_stand\": true}}, \"crop_fade_m\": [28, 42]}", r, &e));
    SC_EXPECT(r.kinds[0].near_m == 60.0 && r.kinds[0].to_stand && r.crop_fade_to_m == 42.0);
    SC_EXPECT(!LoadClutterRules("{\"kinds\": {" + maize + ", \"near_m\": 2}}}", r, &e));  // under 5 m
    SC_EXPECT(e.find("near_m") != std::string::npos);
    SC_EXPECT(!LoadClutterRules("{\"kinds\": {" + maize + ", \"to_stand\": 1}}}", r, &e));
    // Variants: a list of drawn parts, one to eight.
    SC_EXPECT(LoadClutterRules("{\"kinds\": {\"maize\": {\"part\": [\"clutter_maize\", \"clutter_maize_b\"], \"height_m\": [0.9, 1.05], \"width_m\": [0.5, 0.8], \"brightness\": 0.1}}}", r, &e));
    SC_EXPECT(r.kinds[0].variants.size() == 2 && r.kinds[0].part == "clutter_maize" && r.parts.size() == 2);
    SC_EXPECT(!LoadClutterRules("{\"kinds\": {\"maize\": {\"part\": [], \"height_m\": [0.9, 1.05], \"width_m\": [0.5, 0.8], \"brightness\": 0.1}}}", r, &e));
    SC_EXPECT(!LoadClutterRules("{\"kinds\": {\"maize\": {\"part\": [\"clutter_maize\", \"lava\"], \"height_m\": [0.9, 1.05], \"width_m\": [0.5, 0.8], \"brightness\": 0.1}}}", r, &e));    SC_EXPECT(!LoadClutterRules("{\"kinds\": {\"maize\": {\"part\": \"clutter_maize\", \"height_m\": [1.8, 2.6], \"width_m\": [0.5, 0.8], \"brightness\": 0.1, \"to_stand\": true}}}", r, &e));
    SC_EXPECT(e.find("shares") != std::string::npos);  // metres where shares belong
    // The block must not stand bare: its fade's far end plus the ring's lag lies within the plants' reach.
    SC_EXPECT(!LoadClutterRules("{\"kinds\": {" + maize + ", \"near_m\": 50}}, \"crop_fade_m\": [28, 42]}", r, &e));
    SC_EXPECT(e.find("bare") != std::string::npos);
    SC_EXPECT(!LoadClutterRules("{\"kinds\": {" + maize + "}}, \"crop_fade_m\": [42, 28]}", r, &e));
    // Tints: one to six linear colours of three channels in 0.1..3, a patch 0.5..100 m; widen_max 1..4.
    const std::string grass = "\"grass\": {\"part\": \"clutter_grass\", \"height_m\": [0.1, 0.3], \"width_m\": [0.1, 0.3], \"brightness\": 0.1";
    SC_EXPECT(LoadClutterRules("{\"widen_max\": 2, \"kinds\": {" + grass + ", \"tints\": [[0.2, 0.4, 0.1], [0.6, 0.6, 0.5]], \"tint_patch_m\": 5}}}", r, &e));
    SC_EXPECT(r.widen_max == 2.0 && r.kinds[0].tints.size() == 2 && r.kinds[0].tint_patch_m == 5.0);
    SC_EXPECT(r.kinds[0].tints[1][2] == 0.5f);
    SC_EXPECT(!LoadClutterRules("{\"kinds\": {" + grass + ", \"tints\": []}}}", r, &e));
    SC_EXPECT(e.find("tints") != std::string::npos);
    SC_EXPECT(!LoadClutterRules("{\"kinds\": {" + grass + ", \"tints\": [[0.2, 0.4]]}}}", r, &e));
    SC_EXPECT(!LoadClutterRules("{\"kinds\": {" + grass + ", \"tints\": [[0.2, 0.4, 5]]}}}", r, &e));
    SC_EXPECT(e.find("channel") != std::string::npos);
    SC_EXPECT(!LoadClutterRules("{\"kinds\": {" + grass + ", \"tints\": [[1,1,1],[1,1,1],[1,1,1],[1,1,1],[1,1,1],[1,1,1],[1,1,1]]}}}", r, &e));
    SC_EXPECT(!LoadClutterRules("{\"kinds\": {" + grass + ", \"tint_patch_m\": 0.1}}}", r, &e));
    SC_EXPECT(e.find("tint_patch_m") != std::string::npos);
    SC_EXPECT(!LoadClutterRules("{\"widen_max\": 0.5, \"kinds\": {" + grass + "}}}", r, &e));
    SC_EXPECT(e.find("widen_max") != std::string::npos);
    SC_EXPECT(!LoadClutterRules("{\"ring_m\": 100}", r, &e));  // no kinds
    SC_EXPECT(!LoadClutterRules("{\"kinds\": {\"grass\": {\"part\": \"lava\", \"height_m\": [0.1, 0.3], \"width_m\": [0.1, 0.3], \"brightness\": 0.1}}}", r, &e));
    // Where the kinds go is the recipes': the old per-layer list is refused, not silently ignored.
    SC_EXPECT(!LoadClutterRules("{" + kinds + ", \"layers\": {\"meadow\": [{\"kind\": \"grass\", \"per_m2\": 1}]}}", r, &e));
    SC_EXPECT(e.find("recipes") != std::string::npos);
    SC_EXPECT(!LoadClutterRules(kData + "/scenery/nothing.json", r, &e));
    SC_EXPECT(!LoadClutterRecipes(kData + "/scenery/no_recipes_here", Layers(), r, &e));
}
