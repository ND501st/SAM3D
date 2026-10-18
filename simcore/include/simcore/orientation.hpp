// simcore/orientation.hpp — the vehicle's heading reference beyond the drill (roadmap 6.5): the gyro half-compass's drift
// and the crew chief's compass work.
//
// Drift. The GA-3 is a gyro half-compass (voenizdat_osa_1991 p48): its rotor keeps a direction in space, so as the
// Earth turns under it its heading runs off at the vertical component of the Earth's rate, 15.041 deg/h * sin(latitude)
// (textbook physics of the directional gyro). Block 0005-8M carries a latitude correction (p51: the ШИРОТА
// potentiometer feeds the site's latitude into the GA-3's latitude-correction circuit, used with РЕЛЕ — Qc at РЕЛЕ once
// oriented, p48), which cancels the part set on its scale. What is left is the error of that setting and the gyro's
// own drift. In the northern hemisphere a gyro left uncorrected reads a heading that falls with time: the meridian turns
// anticlockwise under a direction fixed in space.
//
// Compass work (p249–250). The crew chief sets up the artillery compass (ПАБ) 20–50 m in front of the vehicle, the crew
// lay the launcher on it with the TV sight, he reads the launcher's angle and the compass's azimuth on the vehicle by
// mutual sighting, turns it by 30-00, applies the compass correction and gets Qc: the vehicle's heading, from north,
// clockwise. In SAM3D this is one activity of the commander's at a halt: it takes `survey_s` and gives the true
// heading plus a normal error of `survey_sigma_deg`; moving off spoils it. Both values are estimates in the data.
#pragma once

#include <cmath>
#include <random>

#include "simcore/console_data.hpp"

namespace simcore {

// The gyro's drift in degrees per second of its heading reading. `corrected`: the latitude correction is switched in
// (with `set_latitude_deg` on its scale). `own_deg_h`: the gyro's own drift, signed.
inline double GyroDriftDegPerS(double earth_rate_deg_h, double latitude_deg, double set_latitude_deg, bool corrected,
                               double own_deg_h) {
    constexpr double kDeg = 3.14159265358979323846 / 180.0;
    const double earth = std::sin(latitude_deg * kDeg) - (corrected ? std::sin(set_latitude_deg * kDeg) : 0.0);
    return (-earth_rate_deg_h * earth + own_deg_h) / 3600.0;
}

// The crew chief's compass survey at a halt: Start, then Step each tick while the vehicle stands; when it is done the
// measured Qc is ready. Moving off aborts it (the compass and the sighting are for this halt only).
class CompassSurvey {
public:
    enum class State { Idle, Running, Done };

    CompassSurvey() = default;
    CompassSurvey(double survey_s, double sigma_deg, unsigned seed) : survey_s_(survey_s), sigma_deg_(sigma_deg), rng_(seed) {}

    void Start(double t) {
        state_ = State::Running;
        start_s_ = t;
    }
    // One step. `stopped`: the vehicle stands. `true_heading_deg`: the hull's heading (what the compass measures).
    // Returns true on the step the survey finishes.
    bool Step(double t, bool stopped, double true_heading_deg) {
        if (!stopped) {
            state_ = State::Idle;  // moved off: the survey (or its result) belongs to the halt it was made at
            return false;
        }
        if (state_ != State::Running || t + 1e-9 < start_s_ + survey_s_) return false;
        std::normal_distribution<double> error(0.0, sigma_deg_);
        result_deg_ = Wrap360(true_heading_deg + (sigma_deg_ > 0.0 ? error(rng_) : 0.0));
        state_ = State::Done;
        return true;
    }
    // Broken off (the crew chief is needed at his post): no result.
    void Abort() {
        if (state_ == State::Running) state_ = State::Idle;
    }
    State GetState() const { return state_; }
    bool Running() const { return state_ == State::Running; }
    bool Known() const { return state_ == State::Done; }
    double ResultDeg() const { return result_deg_; }
    double Progress(double t) const {
        if (state_ == State::Done) return 1.0;
        if (state_ != State::Running || survey_s_ <= 0.0) return 0.0;
        const double f = (t - start_s_) / survey_s_;
        return f < 0.0 ? 0.0 : f > 1.0 ? 1.0 : f;
    }

    static double Wrap360(double deg) {
        double d = std::fmod(deg, 360.0);
        if (d < 0.0) d += 360.0;
        return d;
    }

private:
    double survey_s_ = 0.0;
    double sigma_deg_ = 0.0;
    std::mt19937 rng_{1u};
    State state_ = State::Idle;
    double start_s_ = 0.0;
    double result_deg_ = 0.0;
};

}  // namespace simcore
