// simcore/em/optical.hpp — the TV sight as a passive optical sensor (roadmap E5).
//
// What a daylight TV camera on the tracking antenna can make of a target. Nothing leaves the vehicle, so no receiver
// knows it is being watched (the EMCON engagement, phase 9.2). Two limits, both textbook:
//   * Contrast. The target's apparent contrast against the sky falls with range as exp(-3.912 R / V): Koschmieder's
//     law, V the meteorological visibility, the range at which a black object's contrast has fallen to 2 %
//     (Middleton, Vision through the Atmosphere, 1952). It is seen while that stays above the camera and operator's
//     threshold.
//   * Resolution. The target must span enough of the camera's lines across its critical dimension (Johnson's
//     criteria, 1958: detection at one line pair, two lines; recognition at four pairs, eight lines). The narrow lens
//     resolves farther than the wide one.
// The threshold is not a new number: it is the one that puts a target at the tuning's TV range (tv_max_range_m, a game
// value) in the reference visibility, so a clear day plays as before; haze shortens it. The camera's lines are an
// estimate (a 625-line camera's 575 visible lines). At night the camera sees nothing.
#pragma once

#include <cmath>

namespace simcore {
namespace em {

constexpr double kKoschmiederConstant = 3.912;          // -ln(0.02)
constexpr double kReferenceVisibilityM = 40000.0;        // the clear day the tuning's TV range is for (estimate)

struct OpticalSensorDef {
    double tv_lines = 575.0;          // visible lines across the picture's height (estimate)
    double detect_lines = 2.0;        // Johnson: detection, one line pair
    double recognise_lines = 8.0;     // Johnson: recognition, four line pairs
    double inherent_contrast = 0.3;   // an aircraft against the sky (estimate)
    double contrast_threshold = 0.0;  // 0: anchored on the TV range (AnchorThreshold)
};

struct OpticalConditions {
    double visibility_m = kReferenceVisibilityM;  // meteorological visibility
    bool daylight = true;
};

struct OpticalLook {
    bool seen = false;        // detected: bright enough against the sky and big enough on the screen
    bool recognised = false;  // its type made out (a drone told from an aircraft)
    double contrast = 0.0;    // apparent contrast now
    double lines = 0.0;       // camera lines across its critical dimension
    const char* why_not = nullptr;
};

// Koschmieder: the apparent contrast of a target of inherent contrast `inherent` at `range_m`.
inline double ApparentContrast(double inherent, double range_m, double visibility_m) {
    if (visibility_m <= 0.0) return 0.0;
    return inherent * std::exp(-kKoschmiederConstant * range_m / visibility_m);
}

// The threshold at which a target is seen out to exactly `range_m` in `visibility_m`.
inline double AnchorThreshold(double inherent, double range_m, double visibility_m) {
    return ApparentContrast(inherent, range_m, visibility_m);
}

// How far the contrast lets a target be seen.
inline double ContrastRangeM(double inherent, double threshold, double visibility_m) {
    if (threshold <= 0.0 || inherent <= threshold) return 0.0;
    return std::log(inherent / threshold) * visibility_m / kKoschmiederConstant;
}

// Camera lines across a target `size_m` big at `range_m`, the picture `fov_height_rad` high.
inline double LinesAcross(const OpticalSensorDef& d, double size_m, double range_m, double fov_height_rad) {
    if (range_m <= 0.0 || fov_height_rad <= 0.0) return 1e9;
    const double per_line_rad = fov_height_rad / d.tv_lines;
    return (size_m / range_m) / per_line_rad;
}

// One look at a target. `threshold` is the contrast threshold in use (the anchored one when the def's is 0).
// `cloud_transmission`: the light a cloud between lets through (weather.hpp); the contrast falls with it.
inline OpticalLook Look(const OpticalSensorDef& d, double threshold, const OpticalConditions& c, double range_m,
                        double size_m, double fov_height_rad, bool line_of_sight, double cloud_transmission = 1.0) {
    OpticalLook l;
    if (!c.daylight) {
        l.why_not = "no daylight for the TV";
        return l;
    }
    if (!line_of_sight) {
        l.why_not = "no line of sight";
        return l;
    }
    l.contrast = ApparentContrast(d.inherent_contrast, range_m, c.visibility_m) * cloud_transmission;
    l.lines = LinesAcross(d, size_m, range_m, fov_height_rad);
    if (l.contrast < threshold) {
        l.why_not = cloud_transmission < 0.5 ? "in cloud" : (c.visibility_m < kReferenceVisibilityM ? "lost in the haze" : "beyond TV range");
        return l;
    }
    if (l.lines < d.detect_lines) {
        l.why_not = "too small on the screen";
        return l;
    }
    l.seen = true;
    l.recognised = l.lines >= d.recognise_lines;
    return l;
}

}  // namespace em
}  // namespace simcore
