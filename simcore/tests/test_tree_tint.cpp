// The trees in their season (simcore/scenery/tree_tint.hpp, roadmap T5.1): each tree's colour multiplier on a day of the
// year: summer before the kind's onset, its autumn colour after the full day, each tree on its own day and brightness,
// the woods' masses the mix's mean, the same for every client.
#include <cmath>
#include <cstdio>
#include <string>

#include "simcore/scenery/tree_tint.hpp"
#include "test_framework.hpp"

using namespace simcore::scenery;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

const TreeSeasons& Rules() {
    static TreeSeasons rules;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string e;
        if (!LoadTreeSeasons(kData + "/scenery/tree_seasons.json", rules, &e)) std::printf("    %s\n", e.c_str());
    }
    return rules;
}

constexpr int Oak = static_cast<int>(TreeKind::Oak), Poplar = static_cast<int>(TreeKind::Poplar), Pine = static_cast<int>(TreeKind::Pine),
              Acacia = static_cast<int>(TreeKind::Acacia);

}  // namespace

SC_TEST("tree tint: summer before the onset, the autumn colour after the full day, each within its own brightness") {
    const TreeSeasons& r = Rules();
    for (int kind = 0; kind < static_cast<int>(TreeKind::Count); ++kind) {
        const TreeSeason& s = r.kinds[static_cast<std::size_t>(kind)];
        for (int i = 0; i < 200; ++i) {
            const double e = 37.0 * i, n = -91.0 * i + 5.0;
            const TreeTintRgb summer = TreeTintOn(r, kind, e, n, 150);
            const TreeTintRgb autumn = TreeTintOn(r, kind, e, n, 360);
            for (std::size_t c = 0; c < 3; ++c) {
                // The same brightness draw at both ends: the autumn colour is the summer's times the kind's multiplier.
                SC_EXPECT_NEAR(autumn[c] / summer[c], s.autumn[c], 1e-4);
                SC_EXPECT(summer[c] > 1.0 - s.brightness - 1e-6 && summer[c] < 1.0 + s.brightness + 1e-6);
            }
        }
    }
}

SC_TEST("tree tint: a tree's autumn share only grows with the day, and the pine does not change") {
    const TreeSeasons& r = Rules();
    for (int i = 0; i < 100; ++i) {
        const double e = 211.0 * i, n = 77.0 * i;
        for (int kind : {Oak, Poplar, Acacia}) {
            double last = -1.0;
            for (int day = 200; day <= 366; ++day) {
                const double a = AutumnShare(r.kinds[static_cast<std::size_t>(kind)], e, n, day);
                SC_EXPECT(a >= last - 1e-12 && a >= 0.0 && a <= 1.0);
                last = a;
            }
            SC_EXPECT_NEAR(last, 1.0, 1e-12);
        }
        const TreeTintRgb p0 = TreeTintOn(r, Pine, e, n, 150), p1 = TreeTintOn(r, Pine, e, n, 360);
        for (std::size_t c = 0; c < 3; ++c) SC_EXPECT_NEAR(p0[c], p1[c], 1e-6);
    }
}

SC_TEST("tree tint: trees of one kind do not turn on one day, and the locust turns before the oak (15 October)") {
    const TreeSeasons& r = Rules();
    int turned_acacia = 0, turned_oak = 0, looked = 0;
    double lo = 1.0, hi = 0.0;
    for (int i = 0; i < 1000; ++i) {
        const double e = 13.0 * i, n = 29.0 * (i % 37);
        const double a = AutumnShare(r.kinds[Acacia], e, n, 288), o = AutumnShare(r.kinds[Oak], e, n, 288);
        lo = std::fmin(lo, a);
        hi = std::fmax(hi, a);
        turned_acacia += a > 0.5 ? 1 : 0;
        turned_oak += o > 0.5 ? 1 : 0;
        ++looked;
    }
    std::printf("    on 15 October: %d of %d locusts and %d oaks past half colour; a locust's share %.2f to %.2f\n", turned_acacia, looked,
                turned_oak, lo, hi);
    SC_EXPECT(hi - lo > 0.1);              // spread: not one colour
    SC_EXPECT(turned_acacia > looked / 2);  // the locust is mostly turned by mid-October
    SC_EXPECT(turned_oak < turned_acacia);  // the oak later
}

SC_TEST("tree tint: the same tree is the same colour twice, the woods' masses are the mix's mean, and a bad file is refused") {
    const TreeSeasons& r = Rules();
    const TreeTintRgb a = TreeTintOn(r, Oak, 1234.5, -6789.25, 290), b = TreeTintOn(r, Oak, 1234.5, -6789.25, 290);
    SC_EXPECT(a == b);
    // A mass in summer is the crown colour itself; in deep autumn the mix's mean of the kinds' autumn multipliers.
    const TreeTintRgb m0 = MassTintOn(r, 100), m1 = MassTintOn(r, 366);
    for (std::size_t c = 0; c < 3; ++c) {
        SC_EXPECT_NEAR(m0[c], 1.0, 1e-5);
        double mean = 0.0;
        for (std::size_t k = 0; k < r.kinds.size(); ++k) mean += r.mass_mix[k] * r.kinds[k].autumn[c];
        SC_EXPECT_NEAR(m1[c], mean, 1e-5);
    }
    TreeSeasons bad;
    std::string e;
    SC_EXPECT(!LoadTreeSeasons("{\"trees\": {}}", bad, &e));
    SC_EXPECT(!LoadTreeSeasons("{\"trees\": {\"oak\": {\"onset_day\": 300, \"full_day\": 280, \"spread_days\": 0, \"autumn\": [1,1,1], \"brightness\": 0}}}", bad, &e));
    SC_EXPECT(!LoadTreeSeasons(kData + "/scenery/nothing.json", bad, &e));
}
