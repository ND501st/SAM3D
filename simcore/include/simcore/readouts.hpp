#pragma once
// What the counters of a "counters" display read (the SOC/SSC readout strip, unit 0094-5): the tracking radar's angles in
// the Soviet military angle and its two deviations.
//
// Angles are in "thousandths" (mils): 6000 to the circle, written XX-YY (60-00 is 360 degrees; samsim_osa p16, the
// beta-s dial). A rough ("Г.О.") counter shows XX and a fine ("Т.О.") counter YY of the same angle.
//
// The strip's eight counters, left to right (data/cabins/osa_ak.json, display soc_counters): ΔД, Δε, then the SSC azimuth
// rough and fine, the SSC elevation rough and fine, and the two missile-channel (СВР I, II) places. The manual names
// the strip's contents as "ΔД, Δε, β and ε of the tracking radar" (docs/research/OSA_MANUALS_2026-10.md); which
// counter shows which, and what the deviations are exactly, is our reading of that and of the photographs.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "simcore/cabin.hpp"
#include "simcore/console_data.hpp"
#include "simcore/engagement.hpp"

namespace simcore {

inline double MilsFromDegrees(double degrees) { return degrees * (6000.0 / 360.0); }

// An azimuth in mils as 0..5999.9 (the circle wraps).
inline double WrapMils(double mils) {
    double m = std::fmod(mils, 6000.0);
    if (m < 0.0) m += 6000.0;
    return m;
}

// The rough counter of an angle: the hundreds of mils (0..59 round the circle).
inline int CoarseCounter(double mils) { return static_cast<int>(std::floor(std::fabs(mils) / 100.0)); }

// The fine counter: the mils within the hundred (0..99).
inline int FineCounter(double mils) { return static_cast<int>(std::floor(std::fmod(std::fabs(mils), 100.0))); }

// The tracked echo nearest the range gate (within 5 km of it), or nullptr (nothing is tracked in range).
inline const Contact* GatedReturn(const ConsoleData& console, const EngagementModel& engagement) {
    if (!console.has_sensors) return nullptr;
    const double gate = console.panel.GetValue(console.track.range_wheel);
    const Contact* best = nullptr;
    for (const Contact& c : engagement.TrackReturns()) {
        if (std::fabs(c.range_m - gate) > 5000.0) continue;
        if (best == nullptr || std::fabs(c.range_m - gate) < std::fabs(best->range_m - gate)) best = &c;
    }
    return best;
}

// The value (0..999) the counter named `readout` shows now.
//   range_error      ΔД: metres between the range gate and the echo in it (0 with no echo)
//   elevation_error  Δε: mils between the antenna's elevation and the echo's (0 with no echo)
//   azimuth_*        the SSC azimuth wheel's reading; elevation_* the elevation wheel's (its sign is not shown)
//   none             0 (places the data doesn't give a reading for yet)
// The value (0..999) for counters fed by the tracking radar's echoes and the antenna elevation (a ScopeFeed on a client).
inline int CounterReading(const std::string& readout, const ConsoleData& console, const std::vector<Contact>& track_returns,
                          double antenna_el_deg) {
    if (!console.has_sensors) return 0;
    const TrackSensorDef& d = console.track;
    auto clamp = [](double v) { return static_cast<int>(std::min(999.0, std::max(0.0, std::floor(v + 0.5)))); };
    if (readout == "azimuth_coarse") return CoarseCounter(WrapMils(MilsFromDegrees(console.panel.GetValue(d.az_wheel))));
    if (readout == "azimuth_fine") return FineCounter(WrapMils(MilsFromDegrees(console.panel.GetValue(d.az_wheel))));
    if (readout == "elevation_coarse") return CoarseCounter(MilsFromDegrees(console.panel.GetValue(d.el_wheel)));
    if (readout == "elevation_fine") return FineCounter(MilsFromDegrees(console.panel.GetValue(d.el_wheel)));
    if (readout == "range_error" || readout == "elevation_error") {
        const double gate = console.panel.GetValue(d.range_wheel);
        const Contact* echo = nullptr;
        for (const Contact& c : track_returns) {
            if (std::fabs(c.range_m - gate) > 5000.0) continue;
            if (echo == nullptr || std::fabs(c.range_m - gate) < std::fabs(echo->range_m - gate)) echo = &c;
        }
        if (echo == nullptr) return 0;
        if (readout == "range_error") return clamp(std::fabs(echo->range_m - gate));
        return clamp(std::fabs(MilsFromDegrees(antenna_el_deg - echo->elevation_deg)));
    }
    return 0;
}

// The value a dial shows: its control's, or for a readout dial what the simulation gives (from the scope feed, so a
// network client draws the server's value).
//   gyro_heading  the gyro half-compass's heading in degrees (the Qc scale of block OR-31M, which turns while the gyro is
//                 being slaved to the course setter and stops once they agree: voenizdat_osa_1991 p48)
//   fuel          the tank's level in litres (the driver's fuel gauge; 0 without a fuel model)
inline double DialReading(const CabinDisplay& display, const ConsoleData& console, double gyro_heading_deg, double fuel_l = -1.0) {
    if (display.readout == "gyro_heading") return gyro_heading_deg;
    if (display.readout == "fuel") return fuel_l > 0.0 ? fuel_l : 0.0;
    return console.panel.GetValue(display.control);
}

inline int CounterReading(const std::string& readout, const ConsoleData& console, const EngagementModel& engagement) {
    return CounterReading(readout, console, engagement.TrackReturns(), engagement.AntennaElevationDeg());
}

}  // namespace simcore
