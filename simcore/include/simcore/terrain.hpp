// simcore/terrain.hpp — terrain height interface.
//
// The sim core never owns terrain data. The game (Unreal/Cesium) or a test
// supplies an implementation of `Terrain`. Heights are metres above the
// WGS-84 ellipsoid, looked up by local ENU east/north coordinates.
#pragma once

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace simcore {

class Terrain {
public:
    Terrain() = default;
    Terrain(const Terrain&) = default;
    Terrain& operator=(const Terrain&) = default;
    virtual ~Terrain() = default;
    virtual double HeightAt(double east_m, double north_m) const = 0;
};

// Flat terrain at a constant height.
class FlatTerrain final : public Terrain {
public:
    explicit FlatTerrain(double height_m = 0.0) : height_m_(height_m) {}
    double HeightAt(double, double) const override { return height_m_; }

private:
    double height_m_;
};

// Regular grid heightmap with bilinear interpolation. Outside the grid the edge value is used.
class GridTerrain final : public Terrain {
public:
    // `heights` is row-major: heights[row * cols + col]; row 0 = southmost, col 0 = westmost.
    GridTerrain(double origin_east_m, double origin_north_m, double spacing_m, int cols, int rows,
                std::vector<double> heights)
        : origin_east_(origin_east_m), origin_north_(origin_north_m), spacing_(spacing_m),
          cols_(cols), rows_(rows), heights_(std::move(heights)) {}

    double HeightAt(double east_m, double north_m) const override {
        if (cols_ < 1 || rows_ < 1 || heights_.empty()) return 0.0;
        const double fx = ClampIndex((east_m - origin_east_) / spacing_, cols_);
        const double fy = ClampIndex((north_m - origin_north_) / spacing_, rows_);
        const int x0 = static_cast<int>(std::floor(fx));
        const int y0 = static_cast<int>(std::floor(fy));
        const int x1 = x0 + 1 < cols_ ? x0 + 1 : x0;
        const int y1 = y0 + 1 < rows_ ? y0 + 1 : y0;
        const double tx = fx - x0;
        const double ty = fy - y0;
        const double h00 = At(x0, y0), h10 = At(x1, y0), h01 = At(x0, y1), h11 = At(x1, y1);
        return (h00 * (1 - tx) + h10 * tx) * (1 - ty) + (h01 * (1 - tx) + h11 * tx) * ty;
    }

private:
    static double ClampIndex(double f, int count) {
        if (f < 0.0) return 0.0;
        const double hi = static_cast<double>(count - 1);
        return f > hi ? hi : f;
    }
    double At(int x, int y) const { return heights_[static_cast<std::size_t>(y) * static_cast<std::size_t>(cols_) + static_cast<std::size_t>(x)]; }

    double origin_east_, origin_north_, spacing_;
    int cols_, rows_;
    std::vector<double> heights_;
};

// A straight ridge on flat ground: a bank `height_m` high along the segment from (a_east, a_north) to (b_east, b_north),
// `width_m` wide at its foot, with cosine shoulders. Cover for scenarios played without a terrain grid (in the game,
// Cesium supplies the real ground instead).
struct TerrainRidge {
    double a_east_m = 0.0;
    double a_north_m = 0.0;
    double b_east_m = 0.0;
    double b_north_m = 0.0;
    double height_m = 0.0;
    double width_m = 500.0;
};

class RidgeTerrain final : public Terrain {
public:
    explicit RidgeTerrain(double base_m = 0.0, std::vector<TerrainRidge> ridges = {}) : base_m_(base_m), ridges_(std::move(ridges)) {}

    double HeightAt(double east_m, double north_m) const override {
        double top = 0.0;
        for (const TerrainRidge& r : ridges_) {
            const double dx = r.b_east_m - r.a_east_m, dy = r.b_north_m - r.a_north_m;
            const double len2 = dx * dx + dy * dy;
            double f = len2 > 0.0 ? ((east_m - r.a_east_m) * dx + (north_m - r.a_north_m) * dy) / len2 : 0.0;
            f = f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
            const double d = std::hypot(east_m - (r.a_east_m + f * dx), north_m - (r.a_north_m + f * dy));
            const double half = 0.5 * r.width_m;
            if (d >= half || half <= 0.0) continue;
            const double h = r.height_m * 0.5 * (1.0 + std::cos(3.14159265358979323846 * d / half));
            if (h > top) top = h;
        }
        return base_m_ + top;
    }
    const std::vector<TerrainRidge>& Ridges() const { return ridges_; }

private:
    double base_m_;
    std::vector<TerrainRidge> ridges_;
};

}  // namespace simcore
