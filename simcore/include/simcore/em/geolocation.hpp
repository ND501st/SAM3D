// simcore/em/geolocation.hpp — locating an emitter from bearings or arrival-time differences.
//
// Two classic methods, both with a covariance so the answer is an area, never a point:
//
//   * Bearing fix (triangulation): weighted least squares on bearing lines, weighted by
//     1 / (sigma_i * range_i)^2 and iterated (Stansfield, "Statistical theory of d.f. fixing",
//     J. IEE 94 IIIA, 1947). Lines can come from several stations at once, or from one moving
//     station over time (a "synthetic baseline": how a single aircraft's RWR or ESM pod
//     ranges a SAM that keeps radiating). Old lines are trusted less (AgeInflatedSigma).
//   * TDOA (multilateration, as the Czechoslovak KRTP-81 Ramona / KRTP-86 Tamara did): range
//     differences to a reference site, solved by Gauss–Newton (Foy, "Position-location solutions
//     by Taylor-series estimation", IEEE TAES 1976), with the correlated-noise weighting of
//     Torrieri, "Statistical theory of passive location systems", IEEE TAES 1984.
//
// Error ellipses: semi-axes sqrt(eigenvalues) of the 2x2 horizontal covariance, scaled by
// EllipseScaleForProbability (detection.hpp) for the probability wanted.
#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

#include "simcore/em/common.hpp"
#include "simcore/em/detection.hpp"
#include "simcore/em/linalg.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace em {

// A line of bearing: where the receiver was, the azimuth it measured (clockwise from north) and
// its 1-sigma error. `time_s` lets older lines count for less against a possibly moving emitter.
struct BearingLine {
    Vec3 origin;
    double azimuth_rad = 0.0;
    double sigma_rad = 0.0;
    double time_s = 0.0;
};

struct EllipseDesc {
    double semi_major_m = 0.0;  // 1 sigma
    double semi_minor_m = 0.0;
    double major_azimuth_rad = 0.0;
};

inline EllipseDesc EllipseOf(const Mat2& cov) {
    const Eigen2 e = SymmetricEigen(cov);
    EllipseDesc d;
    d.semi_major_m = std::sqrt(e.major > 0.0 ? e.major : 0.0);
    d.semi_minor_m = std::sqrt(e.minor > 0.0 ? e.minor : 0.0);
    d.major_azimuth_rad = e.major_azimuth_rad;
    return d;
}

// Circular error probable (50% radius), the usual approximation 0.59 (sigma_1 + sigma_2),
// good when the axes differ by less than ~3:1 (Torrieri 1984).
inline double CepM(const Mat2& cov) {
    const EllipseDesc e = EllipseOf(cov);
    return 0.59 * (e.semi_major_m + e.semi_minor_m);
}

struct FixResult {
    bool valid = false;
    Vec3 position;          // z = mean of the receivers' heights (bearing fixes are 2-D)
    Mat2 covariance;        // east/north, m^2
    double chi2 = 0.0;      // sum of squared normalised miss distances (consistency of the lines)
    int lines_used = 0;
    double max_crossing_angle_rad = 0.0;  // best geometry among the lines (~90 deg ideal)
};

// Bearing sigma inflated for age: an emitter that may move at `max_speed_mps` can be up to
// speed * age off the old line; at range r that is an extra angle of speed * age / r.
inline double AgeInflatedSigma(double sigma_rad, double age_s, double max_speed_mps, double range_m) {
    if (age_s <= 0.0 || max_speed_mps <= 0.0 || range_m <= 1.0) return sigma_rad;
    const double extra = max_speed_mps * age_s / range_m;
    return std::sqrt(sigma_rad * sigma_rad + extra * extra);
}

// Weighted least-squares fix of bearing lines (2-D, ENU east/north).
//   now_s, max_speed_mps: inflate old lines (0 = trust every line equally).
//   min_crossing_rad: refuse fixes whose lines are nearly parallel (no information along them).
inline FixResult BearingFix(const std::vector<BearingLine>& lines, double now_s = 0.0, double max_speed_mps = 0.0,
                            double min_crossing_rad = 0.05) {
    FixResult r;
    if (lines.size() < 2) return r;
    // Geometry check: the widest angle between any two lines.
    double best = 0.0;
    for (std::size_t i = 0; i < lines.size(); ++i)
        for (std::size_t j = i + 1; j < lines.size(); ++j) {
            double d = std::fabs(WrapPi(lines[i].azimuth_rad - lines[j].azimuth_rad));
            if (d > 0.5 * kPi) d = kPi - d;
            if (d > best) best = d;
        }
    r.max_crossing_angle_rad = best;
    if (best < min_crossing_rad) return r;

    // Pass 0 assumes equal ranges; later passes weight by the range implied by the estimate.
    Col2 est;
    bool have_est = false;
    Mat2 info;
    for (int pass = 0; pass < 4; ++pass) {
        Mat2 a;
        Col2 b;
        for (const BearingLine& l : lines) {
            const double s = std::sin(l.azimuth_rad), c = std::cos(l.azimuth_rad);
            // Normal to the line: n . (p - origin) = 0, n = (cos az, -sin az) in (east, north).
            const double nx = c, ny = -s;
            double range = 10000.0;
            if (have_est) {
                const double ex = est(0, 0) - l.origin.x, ey = est(1, 0) - l.origin.y;
                range = std::sqrt(ex * ex + ey * ey);
                if (range < 100.0) range = 100.0;
            }
            double sigma = AgeInflatedSigma(l.sigma_rad, now_s - l.time_s, max_speed_mps, range);
            if (sigma < 1e-6) sigma = 1e-6;
            const double w = 1.0 / (sigma * sigma * range * range);
            const double d = nx * l.origin.x + ny * l.origin.y;
            a(0, 0) += w * nx * nx;
            a(0, 1) += w * nx * ny;
            a(1, 0) += w * ny * nx;
            a(1, 1) += w * ny * ny;
            b(0, 0) += w * nx * d;
            b(1, 0) += w * ny * d;
        }
        Mat2 inv;
        if (!Inverse(a, inv)) return r;
        est = inv * b;
        info = a;
        have_est = true;
    }
    // Maximum-likelihood refinement on the angle residuals themselves (Gauss–Newton). The
    // pseudolinear solution above is biased towards the receivers when bearings are poor and the
    // baseline short ("range collapse"); the ML estimate and its covariance are consistent.
    for (int it = 0; it < 15; ++it) {
        Mat2 jtj;
        Col2 jtr;
        for (const BearingLine& l : lines) {
            const double ex = est(0, 0) - l.origin.x, ey = est(1, 0) - l.origin.y;
            double d2 = ex * ex + ey * ey;
            if (d2 < 1e4) d2 = 1e4;
            const double range = std::sqrt(d2);
            double sigma = AgeInflatedSigma(l.sigma_rad, now_s - l.time_s, max_speed_mps, range);
            if (sigma < 1e-6) sigma = 1e-6;
            const double w = 1.0 / (sigma * sigma);
            const double res = WrapPi(l.azimuth_rad - std::atan2(ex, ey));
            const double jx = ey / d2, jy = -ex / d2;  // d(azimuth)/d(east, north)
            jtj(0, 0) += w * jx * jx;
            jtj(0, 1) += w * jx * jy;
            jtj(1, 0) += w * jy * jx;
            jtj(1, 1) += w * jy * jy;
            jtr(0, 0) += w * jx * res;
            jtr(1, 0) += w * jy * res;
        }
        Mat2 inv;
        if (!Inverse(jtj, inv)) break;
        Col2 step = inv * jtr;
        // Damp very long steps (flat likelihood along the lines of bearing).
        const double len = std::sqrt(step(0, 0) * step(0, 0) + step(1, 0) * step(1, 0));
        if (len > 20000.0) step = step * (20000.0 / len);
        est = est + step;
        info = jtj;
        if (len < 1.0) break;
    }
    // The emitter must lie ahead of the receivers on their lines of bearing.
    int behind = 0;
    for (const BearingLine& l : lines) {
        const double ex = est(0, 0) - l.origin.x, ey = est(1, 0) - l.origin.y;
        if (ex * std::sin(l.azimuth_rad) + ey * std::cos(l.azimuth_rad) < 0.0) ++behind;
    }
    if (2 * behind > static_cast<int>(lines.size())) return r;
    Mat2 cov;
    if (!Inverse(info, cov)) return r;
    cov.Symmetrize();
    double chi2 = 0.0, z = 0.0;
    for (const BearingLine& l : lines) {
        const double ex = est(0, 0) - l.origin.x, ey = est(1, 0) - l.origin.y;
        const double range = std::sqrt(ex * ex + ey * ey);
        const double az = std::atan2(ex, ey);
        const double miss = WrapPi(az - l.azimuth_rad);
        const double sigma = AgeInflatedSigma(l.sigma_rad, now_s - l.time_s, max_speed_mps, range > 1.0 ? range : 1.0);
        chi2 += (miss * miss) / (sigma * sigma);
        z += l.origin.z;
    }
    r.valid = true;
    r.position = {est(0, 0), est(1, 0), z / static_cast<double>(lines.size())};
    r.covariance = cov;
    r.chi2 = chi2;
    r.lines_used = static_cast<int>(lines.size());
    return r;
}

// ------------------------------------------------------------------------------------ TDOA

struct TdoaSite {
    Vec3 position;
    double arrival_time_s = 0.0;  // measured time of arrival of the same pulse
};

struct TdoaResult {
    bool valid = false;
    Vec3 position;
    Mat3 covariance;  // full 3-D (or 2-D embedded with the fixed axis ~0) in m^2
    int iterations = 0;
    double residual_rms_m = 0.0;
};

// Multilateration from arrival times at >= 3 sites (2-D with a known altitude) or >= 4 sites (3-D).
//   sigma_t_s: 1-sigma error of each arrival time.
//   known_altitude_z: if `fix_z`, the emitter's ENU z is held at this value (2-D solve).
//   initial: starting guess (e.g. the centroid of the sites pushed out along the mean bearing).
inline TdoaResult SolveTdoa(const std::vector<TdoaSite>& sites, double sigma_t_s, const Vec3& initial,
                            bool fix_z = false) {
    TdoaResult r;
    const std::size_t n = sites.size();
    const std::size_t dims = fix_z ? 2 : 3;
    if (n < dims + 1 || n > 9) return r;
    const std::size_t m = n - 1;  // range differences
    const double sigma_d = kSpeedOfLight * sigma_t_s;
    Vec3 x = initial;
    // Measured range differences relative to site 0.
    std::vector<double> dmeas(m);
    for (std::size_t i = 0; i < m; ++i)
        dmeas[i] = kSpeedOfLight * (sites[i + 1].arrival_time_s - sites[0].arrival_time_s);
    // Noise covariance of the differences: sigma_d^2 (I + 1 1^T); inverse by Sherman–Morrison:
    // (I + 1 1^T)^-1 = I - 1 1^T / (1 + m).
    const double inv_scale = 1.0 / (sigma_d * sigma_d > 1e-30 ? sigma_d * sigma_d : 1e-30);
    const double sm = 1.0 / (1.0 + static_cast<double>(m));
    Mat3 normal;
    for (int it = 0; it < 20; ++it) {
        std::vector<Vec3> jac(m);
        std::vector<double> res(m);
        const Vec3 u0v = x - sites[0].position;
        const double r0 = u0v.Length() > 1e-6 ? u0v.Length() : 1e-6;
        for (std::size_t i = 0; i < m; ++i) {
            const Vec3 uiv = x - sites[i + 1].position;
            const double ri = uiv.Length() > 1e-6 ? uiv.Length() : 1e-6;
            res[i] = dmeas[i] - (ri - r0);
            jac[i] = uiv / ri - u0v / r0;
            if (fix_z) jac[i].z = 0.0;
        }
        // J^T W J and J^T W res with W = inv_scale (I - sm 1 1^T).
        Mat3 jtwj;
        Col3 jtwr;
        Vec3 sum_j;
        double sum_r = 0.0;
        for (std::size_t i = 0; i < m; ++i) {
            sum_j += jac[i];
            sum_r += res[i];
        }
        for (std::size_t i = 0; i < m; ++i) {
            const Vec3 wj = (jac[i] - sum_j * sm) * inv_scale;  // row i of W J
            const double wr = (res[i] - sum_r * sm) * inv_scale;
            const double ji[3] = {jac[i].x, jac[i].y, jac[i].z};
            const double wji[3] = {wj.x, wj.y, wj.z};
            for (std::size_t a = 0; a < 3; ++a) {
                jtwr(a, 0) += ji[a] * wr;
                for (std::size_t b = 0; b < 3; ++b) jtwj(a, b) += ji[a] * wji[b];
            }
        }
        if (fix_z) jtwj(2, 2) = 1.0;  // keep the fixed axis invertible
        Mat3 inv;
        if (!Inverse(jtwj, inv)) return r;
        const Col3 step = inv * jtwr;
        x.x += step(0, 0);
        x.y += step(1, 0);
        if (!fix_z) x.z += step(2, 0);
        normal = jtwj;
        r.iterations = it + 1;
        const double step_len = std::sqrt(step(0, 0) * step(0, 0) + step(1, 0) * step(1, 0) +
                                          (fix_z ? 0.0 : step(2, 0) * step(2, 0)));
        if (step_len < 1e-3) break;
    }
    Mat3 cov;
    if (!Inverse(normal, cov)) return r;
    if (fix_z) cov(2, 2) = 0.0;
    cov.Symmetrize();
    double ss = 0.0;
    const double r0 = (x - sites[0].position).Length();
    for (std::size_t i = 0; i < m; ++i) {
        const double e = dmeas[i] - ((x - sites[i + 1].position).Length() - r0);
        ss += e * e;
    }
    r.valid = true;
    r.position = x;
    r.covariance = cov;
    r.residual_rms_m = std::sqrt(ss / static_cast<double>(m));
    return r;
}

// Horizontal 2x2 block of a 3x3 covariance.
inline Mat2 Horizontal(const Mat3& c) {
    Mat2 h;
    h(0, 0) = c(0, 0);
    h(0, 1) = c(0, 1);
    h(1, 0) = c(1, 0);
    h(1, 1) = c(1, 1);
    return h;
}

}  // namespace em
}  // namespace simcore
