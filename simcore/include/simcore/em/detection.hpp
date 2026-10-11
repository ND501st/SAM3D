// simcore/em/detection.hpp — detection statistics, measurement accuracy and interference.
//
// The radar equation gives a signal-to-noise ratio; this file turns it into what an operator gets:
// a probability of seeing the echo, how accurate the range and angle are, and what jamming and
// clutter do to both. References (docs/SOURCES.md, "EM layer"):
//   * Swerling I target, square-law detector, one look:  Pd = Pfa^(1 / (1 + SNR))
//     (Skolnik, Introduction to Radar Systems, ch. 2; Richards, Fundamentals of Radar Signal
//     Processing, ch. 6). Inverted: SNR needed = ln(Pfa) / ln(Pd) - 1.
//     Swerling I (scan-to-scan fluctuation) is the conservative textbook choice for aircraft.
//   * Thermal accuracy: sigma_R = c tau / (2 sqrt(2 SNR)), sigma_theta = theta_3dB / (k_m sqrt(2 SNR))
//     (Barton, Radar System Analysis and Modeling, ch. 18), each with a floor for bias and servo noise.
//   * Noise jamming adds to the receiver noise: SINR = SNR / (1 + J/N + C/N).
//     J/N = ERP_j G_r lambda^2 / ((4 pi)^2 R_j^2 L) / (k T0 F B_j)   (per unit bandwidth: a barrage
//     jammer spreads its power over B_j; only the part inside the radar's bandwidth counts)
//     (Adamy, EW 101, ch. 9; Schleher, Electronic Warfare in the Information Age, ch. 4).
#pragma once

#include <cmath>

#include "simcore/em/propagation.hpp"
#include "simcore/radar.hpp"

namespace simcore {
namespace em {

constexpr double kNoiseDensityW_Hz = kBoltzmann * kReferenceNoiseTemp;  // k T0 ~ 4.0e-21 W/Hz (-204 dBW/Hz)

// Probability of detection, Swerling I, one look, given linear SNR and false-alarm probability.
inline double SwerlingIPd(double snr_linear, double pfa) {
    if (snr_linear <= 0.0) return pfa;
    return std::pow(pfa, 1.0 / (1.0 + snr_linear));
}

// Linear SNR needed for a Pd (Swerling I, one look).
inline double SwerlingIRequiredSnr(double pd, double pfa) {
    if (pd <= pfa) return 0.0;
    return std::log(pfa) / std::log(pd) - 1.0;
}

// Range accuracy (1 sigma, m) from pulse width and SNR, with a floor.
inline double RangeSigmaM(double pulse_width_s, double snr_linear, double floor_m) {
    const double s = snr_linear > 0.1 ? snr_linear : 0.1;
    const double thermal = kSpeedOfLight * pulse_width_s / (2.0 * std::sqrt(2.0 * s));
    return std::sqrt(thermal * thermal + floor_m * floor_m);
}

// Angle accuracy (1 sigma, rad) from beamwidth and SNR, with a floor. k_m ~ 1.6 for monopulse.
inline double AngleSigmaRad(double beamwidth_rad, double snr_linear, double slope_km, double floor_rad) {
    const double s = snr_linear > 0.1 ? snr_linear : 0.1;
    const double thermal = beamwidth_rad / ((slope_km > 0.1 ? slope_km : 0.1) * std::sqrt(2.0 * s));
    return std::sqrt(thermal * thermal + floor_rad * floor_rad);
}

// Received power (W) at the end of a one-way path: EIRP towards the receiver, receiver gain,
// wavelength, range and extra one-way losses (diffraction, gases, multipath as a dB number).
inline double OneWayReceivedPowerW(double eirp_w, double rx_gain_db, double freq_hz, double range_m,
                                   double extra_loss_db) {
    if (range_m <= 0.0 || freq_hz <= 0.0) return 0.0;
    const double lambda = WavelengthOf(freq_hz);
    const double spread = lambda * lambda / (16.0 * kPi * kPi * range_m * range_m);
    return eirp_w * DbToLinear(rx_gain_db - extra_loss_db) * spread;
}

// Fraction of a noise jammer's power that falls inside a receiver's bandwidth (0..1).
// Spot noise narrower than the receiver counts in full; barrage noise counts B_r / B_j.
// Frequencies that don't overlap at all give 0.
inline double JammerBandOverlap(double jam_centre_hz, double jam_bw_hz, double rx_centre_hz, double rx_bw_hz) {
    if (jam_bw_hz <= 0.0 || rx_bw_hz <= 0.0) return 0.0;
    const double lo = (jam_centre_hz - 0.5 * jam_bw_hz) > (rx_centre_hz - 0.5 * rx_bw_hz)
                          ? (jam_centre_hz - 0.5 * jam_bw_hz)
                          : (rx_centre_hz - 0.5 * rx_bw_hz);
    const double hi = (jam_centre_hz + 0.5 * jam_bw_hz) < (rx_centre_hz + 0.5 * rx_bw_hz)
                          ? (jam_centre_hz + 0.5 * jam_bw_hz)
                          : (rx_centre_hz + 0.5 * rx_bw_hz);
    if (hi <= lo) return 0.0;
    return (hi - lo) / jam_bw_hz;
}

// Jamming-to-noise ratio (linear) in a receiver.
//   jam_power_in_band_w: jammer power reaching the receiver input inside its bandwidth (W).
inline double JammingToNoise(double jam_power_in_band_w, double noise_figure_db, double rx_bandwidth_hz) {
    const double n = kNoiseDensityW_Hz * DbToLinear(noise_figure_db) * rx_bandwidth_hz;
    return n > 0.0 ? jam_power_in_band_w / n : 0.0;
}

// Self-screening burn-through range (m), free space: the range at which SNR / (1 + J/N) reaches
// the SNR needed. For a self-screening jammer J/N falls as 1/R^2 and S/N as 1/R^4, so
//   K/R^4 / (1 + J0/R^2) = req   ->   req J0 x + req x^2 ... solved for x = R^2:
//   req x^2 + req J0 x - K = 0,  K = SNR at 1 m, J0 = J/N at 1 m.
inline double SelfScreenBurnThroughRangeM(double snr_at_1m, double jn_at_1m, double required_snr) {
    if (snr_at_1m <= 0.0 || required_snr <= 0.0) return 0.0;
    const double a = required_snr;
    const double b = required_snr * jn_at_1m;
    const double c = -snr_at_1m;
    const double x = (-b + std::sqrt(b * b - 4.0 * a * c)) / (2.0 * a);
    return x > 0.0 ? std::sqrt(x) : 0.0;
}

// Probability-ellipse scale for a 2-D Gaussian: the ellipse with semi-axes s*sigma contains
// probability p. s = sqrt(-2 ln(1 - p)): 1.177 (50%), 2.146 (90%), 2.448 (95%).
inline double EllipseScaleForProbability(double p) {
    if (p <= 0.0) return 0.0;
    if (p >= 0.999999) p = 0.999999;
    return std::sqrt(-2.0 * std::log(1.0 - p));
}

}  // namespace em
}  // namespace simcore
