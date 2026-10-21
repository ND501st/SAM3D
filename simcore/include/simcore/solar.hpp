// simcore/solar.hpp — where the sun stands: elevation and azimuth for a place, a day of the year and a time (UTC).
//
// The low-precision formulas of the Astronomical Almanac (solar declination and the equation of time from the mean
// anomaly and ecliptic longitude; J. Meeus, Astronomical Algorithms, 2nd ed., 1998, ch. 25 accuracy: about 0.01 deg in
// declination, well under a minute in time between 1950 and 2050). Refraction is ignored: the sun on the horizon is
// at 0 deg. For the game's light and for daylight: the TV needs the sun above about -6 deg (civil twilight).
#pragma once

#include <cmath>

#include "simcore/vec3.hpp"

namespace simcore {

struct SunPosition {
    double elevation_rad = 0.0;
    double azimuth_rad = 0.0;  // true, clockwise from north
    double declination_rad = 0.0;
    double equation_of_time_min = 0.0;
};

// `day_of_year` 1-366, `utc_hours` 0-24, latitude and longitude in degrees (east positive).
inline SunPosition SunAt(double lat_deg, double lon_deg, int day_of_year, double utc_hours) {
    // Days from J2000.0, the day taken in 2001 (a common year: day 288 is 15 October; the sun's place repeats year on
    // year to a few hundredths of a degree).
    const double n = 366.0 + (day_of_year - 1) + (utc_hours - 12.0) / 24.0;
    const double mean_long = DegToRad(std::fmod(280.460 + 0.9856474 * n, 360.0));
    const double g = DegToRad(std::fmod(357.528 + 0.9856003 * n, 360.0));  // mean anomaly
    const double lambda = mean_long + DegToRad(1.915) * std::sin(g) + DegToRad(0.020) * std::sin(2.0 * g);
    const double eps = DegToRad(23.439 - 0.0000004 * n);
    SunPosition s;
    s.declination_rad = std::asin(std::sin(eps) * std::sin(lambda));
    const double ra = std::atan2(std::cos(eps) * std::sin(lambda), std::cos(lambda));
    double eot = mean_long - ra;  // radians; wrap to +-pi
    eot = std::atan2(std::sin(eot), std::cos(eot));
    s.equation_of_time_min = RadToDeg(eot) * 4.0;
    const double solar_time_h = utc_hours + lon_deg / 15.0 + s.equation_of_time_min / 60.0;
    const double hour_angle = DegToRad(15.0 * (solar_time_h - 12.0));
    const double lat = DegToRad(lat_deg);
    const double sin_el = std::sin(lat) * std::sin(s.declination_rad) + std::cos(lat) * std::cos(s.declination_rad) * std::cos(hour_angle);
    s.elevation_rad = std::asin(Clamp(sin_el, -1.0, 1.0));
    // Azimuth from north: atan2 of the east and north parts of the sun's direction.
    const double east = -std::cos(s.declination_rad) * std::sin(hour_angle);
    const double north = std::sin(s.declination_rad) * std::cos(lat) - std::cos(s.declination_rad) * std::sin(lat) * std::cos(hour_angle);
    s.azimuth_rad = std::atan2(east, north);
    if (s.azimuth_rad < 0.0) s.azimuth_rad += kTwoPi;
    return s;
}

}  // namespace simcore
