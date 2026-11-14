// simcore/solar.hpp — where the sun stands: elevation and azimuth for a place, a day of the year and a time (UTC).
//
// The low-precision formulas of the Astronomical Almanac (solar declination and the equation of time from the mean
// anomaly and ecliptic longitude; J. Meeus, Astronomical Algorithms, 2nd ed., 1998, ch. 25 accuracy: about 0.01 deg in
// declination, well under a minute in time between 1950 and 2050). Refraction is ignored: the sun on the horizon is
// at 0 deg. For the game's light and for daylight: the TV needs the sun above about -6 deg (civil twilight).
#pragma once

#include <cmath>
#include <cstddef>
#include <string>

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

// ---------------------------------------------------------------------------------------------- dates (roadmap R7)

inline bool IsLeapYear(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

// The day of the year 1-366 of a Gregorian date; 0 for a date that does not exist (30 February, month 13).
inline int DayOfYear(int year, int month, int day) {
    static const int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12 || day < 1) return 0;
    const int in_month = kDays[month - 1] + (month == 2 && IsLeapYear(year) ? 1 : 0);
    if (day > in_month) return 0;
    int doy = day;
    for (int m = 1; m < month; ++m) doy += kDays[m - 1] + (m == 2 && IsLeapYear(year) ? 1 : 0);
    return doy;
}

// The month and day of a year's day 1-366 (the inverse of DayOfYear); false out of the year.
inline bool DateOfDay(int year, int doy, int& month, int& day) {
    const int days = IsLeapYear(year) ? 366 : 365;
    if (doy < 1 || doy > days) return false;
    for (int m = 1; m <= 12; ++m) {
        const int before = DayOfYear(year, m, 1) - 1;
        const int last = m == 12 ? days : DayOfYear(year, m + 1, 1) - 1;
        if (doy <= last) {
            month = m;
            day = doy - before;
            return true;
        }
    }
    return false;
}

// "1985-10-15" -> year 1985, day of the year 288. False for any other form or a date that does not exist.
inline bool ParseIsoDate(const std::string& s, int& year, int& doy) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    int digits[8] = {};
    const std::size_t at[8] = {0, 1, 2, 3, 5, 6, 8, 9};
    for (std::size_t i = 0; i < 8; ++i) {
        const char c = s[at[i]];
        if (c < '0' || c > '9') return false;
        digits[i] = c - '0';
    }
    const int y = digits[0] * 1000 + digits[1] * 100 + digits[2] * 10 + digits[3];
    const int d = DayOfYear(y, digits[4] * 10 + digits[5], digits[6] * 10 + digits[7]);
    if (d == 0) return false;
    year = y;
    doy = d;
    return true;
}

// The local hour 0-24 at which the sun crosses `elevation_deg` rising (or setting) on a day at a place, the clock
// `utc_offset_h` ahead of UTC. Scans the local day in 120 s steps and bisects to a second; false when it does not cross
// (the polar day or night, or a sun that never reaches that elevation).
inline bool SunCrossing(double lat_deg, double lon_deg, int day_of_year, double utc_offset_h, double elevation_deg, bool rising,
                        double& local_h) {
    const double target = DegToRad(elevation_deg);
    const auto el = [&](double h) { return SunAt(lat_deg, lon_deg, day_of_year, h - utc_offset_h).elevation_rad - target; };
    const double step_h = 120.0 / 3600.0;
    double a = 0.0;
    double ea = el(a);
    for (int i = 1; i <= 720; ++i) {
        const double b = step_h * i;
        const double eb = el(b);
        const bool up = ea < 0.0 && eb >= 0.0;
        const bool down = ea >= 0.0 && eb < 0.0;
        if ((rising && up) || (!rising && down)) {
            double lo = a;
            double hi = b;
            while ((hi - lo) * 3600.0 > 1.0) {
                const double mid = 0.5 * (lo + hi);
                const bool above = el(mid) >= 0.0;
                if (above == rising) {
                    hi = mid;
                } else {
                    lo = mid;
                }
            }
            local_h = 0.5 * (lo + hi);
            return true;
        }
        a = b;
        ea = eb;
    }
    return false;
}

}  // namespace simcore
