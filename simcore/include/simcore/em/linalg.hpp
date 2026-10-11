// simcore/em/linalg.hpp — small fixed-size matrices for filters and geolocation.
//
// Just enough linear algebra for 2x2, 3x3 and 6x6 work (Kalman filters, covariance
// intersection, bearing fixes, error ellipses), with no heap allocation. Row-major storage.
#pragma once

#include <array>
#include <cmath>
#include <cstddef>

#include "simcore/vec3.hpp"

namespace simcore {
namespace em {

template <std::size_t R, std::size_t C>
struct Mat {
    std::array<double, R * C> a{};

    double& operator()(std::size_t r, std::size_t c) { return a[r * C + c]; }
    double operator()(std::size_t r, std::size_t c) const { return a[r * C + c]; }

    static Mat Zero() { return Mat{}; }
    static Mat Identity() {
        Mat m;
        for (std::size_t i = 0; i < R && i < C; ++i) m(i, i) = 1.0;
        return m;
    }
    static Mat Diagonal(double v) {
        Mat m;
        for (std::size_t i = 0; i < R && i < C; ++i) m(i, i) = v;
        return m;
    }

    Mat operator+(const Mat& o) const {
        Mat m;
        for (std::size_t i = 0; i < R * C; ++i) m.a[i] = a[i] + o.a[i];
        return m;
    }
    Mat operator-(const Mat& o) const {
        Mat m;
        for (std::size_t i = 0; i < R * C; ++i) m.a[i] = a[i] - o.a[i];
        return m;
    }
    Mat operator*(double s) const {
        Mat m;
        for (std::size_t i = 0; i < R * C; ++i) m.a[i] = a[i] * s;
        return m;
    }
    Mat& operator+=(const Mat& o) {
        for (std::size_t i = 0; i < R * C; ++i) a[i] += o.a[i];
        return *this;
    }

    Mat<C, R> Transposed() const {
        Mat<C, R> t;
        for (std::size_t r = 0; r < R; ++r)
            for (std::size_t c = 0; c < C; ++c) t(c, r) = (*this)(r, c);
        return t;
    }

    double Trace() const {
        double t = 0.0;
        for (std::size_t i = 0; i < R && i < C; ++i) t += (*this)(i, i);
        return t;
    }

    // Make exactly symmetric (guards against round-off drift in covariance updates).
    void Symmetrize() {
        for (std::size_t r = 0; r < R; ++r)
            for (std::size_t c = r + 1; c < C; ++c) {
                const double v = 0.5 * ((*this)(r, c) + (*this)(c, r));
                (*this)(r, c) = v;
                (*this)(c, r) = v;
            }
    }
};

template <std::size_t R, std::size_t K, std::size_t C>
Mat<R, C> operator*(const Mat<R, K>& x, const Mat<K, C>& y) {
    Mat<R, C> m;
    for (std::size_t r = 0; r < R; ++r)
        for (std::size_t c = 0; c < C; ++c) {
            double s = 0.0;
            for (std::size_t k = 0; k < K; ++k) s += x(r, k) * y(k, c);
            m(r, c) = s;
        }
    return m;
}

// v^T M v, written out (a 1x1 matrix product trips MSVC's C4686 inside Unreal builds).
template <std::size_t N>
double QuadraticForm(const Mat<N, 1>& v, const Mat<N, N>& m) {
    double s = 0.0;
    for (std::size_t r = 0; r < N; ++r)
        for (std::size_t c = 0; c < N; ++c) s += v(r, 0) * m(r, c) * v(c, 0);
    return s;
}

using Mat2 = Mat<2, 2>;
using Mat3 = Mat<3, 3>;
using Mat6 = Mat<6, 6>;
using Col2 = Mat<2, 1>;
using Col3 = Mat<3, 1>;
using Col6 = Mat<6, 1>;

inline Col3 ToCol(const Vec3& v) {
    Col3 c;
    c(0, 0) = v.x;
    c(1, 0) = v.y;
    c(2, 0) = v.z;
    return c;
}
inline Vec3 ToVec(const Col3& c) { return {c(0, 0), c(1, 0), c(2, 0)}; }

inline double Determinant(const Mat2& m) { return m(0, 0) * m(1, 1) - m(0, 1) * m(1, 0); }
inline double Determinant(const Mat3& m) {
    return m(0, 0) * (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)) - m(0, 1) * (m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0)) +
           m(0, 2) * (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0));
}

// Inverses return false (and leave `out` untouched) when the matrix is singular.
inline bool Inverse(const Mat2& m, Mat2& out) {
    const double d = Determinant(m);
    if (std::fabs(d) < 1e-300) return false;
    out(0, 0) = m(1, 1) / d;
    out(0, 1) = -m(0, 1) / d;
    out(1, 0) = -m(1, 0) / d;
    out(1, 1) = m(0, 0) / d;
    return true;
}

inline bool Inverse(const Mat3& m, Mat3& out) {
    const double d = Determinant(m);
    if (std::fabs(d) < 1e-300) return false;
    Mat3 r;
    r(0, 0) = (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)) / d;
    r(0, 1) = (m(0, 2) * m(2, 1) - m(0, 1) * m(2, 2)) / d;
    r(0, 2) = (m(0, 1) * m(1, 2) - m(0, 2) * m(1, 1)) / d;
    r(1, 0) = (m(1, 2) * m(2, 0) - m(1, 0) * m(2, 2)) / d;
    r(1, 1) = (m(0, 0) * m(2, 2) - m(0, 2) * m(2, 0)) / d;
    r(1, 2) = (m(0, 2) * m(1, 0) - m(0, 0) * m(1, 2)) / d;
    r(2, 0) = (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0)) / d;
    r(2, 1) = (m(0, 1) * m(2, 0) - m(0, 0) * m(2, 1)) / d;
    r(2, 2) = (m(0, 0) * m(1, 1) - m(0, 1) * m(1, 0)) / d;
    out = r;
    return true;
}

// Eigen-decomposition of a symmetric 2x2 matrix: major and minor eigenvalues and the major axis
// direction as an angle measured like an azimuth (clockwise from +y = north, towards +x = east).
struct Eigen2 {
    double major = 0.0;
    double minor = 0.0;
    double major_azimuth_rad = 0.0;
};

inline Eigen2 SymmetricEigen(const Mat2& m) {
    const double a = m(0, 0), b = 0.5 * (m(0, 1) + m(1, 0)), c = m(1, 1);
    const double mean = 0.5 * (a + c);
    const double diff = 0.5 * (a - c);
    const double r = std::sqrt(diff * diff + b * b);
    Eigen2 e;
    e.major = mean + r;
    e.minor = mean - r;
    // Eigenvector of the major value in (east, north): angle from east = 0.5 atan2(2b, a - c).
    const double from_east = 0.5 * std::atan2(2.0 * b, a - c);
    double az = 0.5 * kPi - from_east;
    while (az < 0.0) az += kPi;
    while (az >= kPi) az -= kPi;
    e.major_azimuth_rad = az;
    return e;
}

}  // namespace em
}  // namespace simcore
