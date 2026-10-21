// simcore/em/emitter.hpp — what radiates: antennas, waveforms, scan patterns, emitter state.
//
// An emitter definition is pure data (loaded from data/em/*.json): power, antenna, waveform,
// scan. The same record serves every receiver of that emission: the radar's own echo model, an
// aircraft's RWR, a ground ELINT site, a jammer's set-on receiver. Nothing here is specific to
// one system; the Osa-AK's radars are entries in data/em/osa_ak.json.
//
// Level of detail (see docs/EM_ARCHITECTURE.md, "Signal representation"): a signal is described
// per simulation tick as a packet — carrier, bandwidth, pulse repetition frequency, pulse width,
// peak power and where the antenna points — not as individual pulses or waveforms. Receivers
// work with power, bearing and those parameters, which is what real ESM systems measure.
#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "simcore/em/common.hpp"
#include "simcore/radar.hpp"
#include "simcore/vec3.hpp"

namespace simcore {
namespace em {

// MissileUplink: a SAM's command transmitter guiding its missile; MissileResponder: the missile's own transmitter
// answering it (roadmap E5).
enum class EmitterKind { SearchRadar, TrackingRadar, MissileUplink, MissileResponder, Iff, Comms, Jammer, Navaid, Decoy };
enum class ScanKind { Fixed, Circular, Sector, Tracking, Omni };
enum class AgilityKind { Fixed, Channels, PulseToPulse };

inline const char* EmitterKindName(EmitterKind k) {
    switch (k) {
        case EmitterKind::SearchRadar: return "search_radar";
        case EmitterKind::TrackingRadar: return "tracking_radar";
        case EmitterKind::MissileUplink: return "missile_uplink";
        case EmitterKind::MissileResponder: return "missile_responder";
        case EmitterKind::Iff: return "iff";
        case EmitterKind::Comms: return "comms";
        case EmitterKind::Jammer: return "jammer";
        case EmitterKind::Navaid: return "navaid";
        case EmitterKind::Decoy: return "decoy";
    }
    return "?";
}

// Antenna: peak gain and a Gaussian main lobe per axis, floored at a sidelobe level.
// Beamwidth 0 on an axis = omnidirectional on that axis.
struct AntennaDef {
    double gain_db = 0.0;
    double beamwidth_az_rad = 0.0;  // 3 dB
    double beamwidth_el_rad = 0.0;  // 3 dB (for a search radar: the height of one elevation beam)
    double sidelobe_db = -25.0;     // relative to peak (negative)
};

// Gain (dB) at angles off boresight. Gaussian main lobe: -12 (theta / theta_3dB)^2 dB per axis
// (Mahafza), the classic approximation that is exact at the 3 dB points.
inline double AntennaGainDb(const AntennaDef& a, double off_az_rad, double off_el_rad) {
    double loss = 0.0;
    if (a.beamwidth_az_rad > 0.0) {
        const double r = off_az_rad / a.beamwidth_az_rad;
        loss -= 12.0 * r * r;
    }
    if (a.beamwidth_el_rad > 0.0) {
        const double r = off_el_rad / a.beamwidth_el_rad;
        loss -= 12.0 * r * r;
    }
    if (loss < a.sidelobe_db) loss = a.sidelobe_db;
    return a.gain_db + loss;
}

struct WaveformDef {
    double freq_hz = 0.0;           // carrier (centre of the channel in use)
    double bandwidth_hz = 0.0;      // instantaneous signal bandwidth (~1 / pulse width for simple pulses)
    double prf_hz = 0.0;            // 0 = continuous wave
    double pulse_width_s = 0.0;
    AgilityKind agility = AgilityKind::Fixed;
    std::vector<double> channels_hz;  // preset channels (AgilityKind::Channels)
    double agile_band_hz = 0.0;       // spread for pulse-to-pulse agility
    double DutyCycle() const { return prf_hz > 0.0 ? prf_hz * pulse_width_s : 1.0; }
};

// One elevation beam of a search radar's beam programme (Osa: three beams, one per turn).
struct ElevationBeam {
    double centre_rad = 0.0;
    double height_rad = 0.0;
};

struct EmitterDef {
    std::string id;
    std::string name;
    EmitterKind kind = EmitterKind::SearchRadar;
    double peak_power_w = 0.0;
    double tx_loss_db = 0.0;
    AntennaDef antenna;
    WaveformDef waveform;
    ScanKind scan = ScanKind::Omni;
    double scan_period_s = 0.0;        // circular: one revolution
    double sector_width_rad = 0.0;     // sector scan
    std::vector<ElevationBeam> beams;  // empty = one beam at antenna.beamwidth_el around the pointing elevation
    std::string source;
    std::string confidence;
};

// Runtime state of one emitter on one platform.
struct EmitterState {
    int id = 0;
    EmitterDef def;         // copied from the library when the emitter is created
    int platform_id = 0;
    bool radiating = false;
    bool dummy_load = false;  // transmitter on into its equivalent load: nothing leaves the antenna
    double pointing_az_rad = 0.0;    // boresight now
    double pointing_el_rad = 0.0;
    double sweep_start_az_rad = 0.0;  // boresight at the start of this tick (circular scan)
    double swept_rad = 0.0;           // azimuth covered this tick
    int beam_index = 0;               // current beam of the programme
    long long revolutions = 0;
    double freq_hz = 0.0;             // channel in use
    int track_target_platform = 0;    // tracking emitters: who they point at (0 = none)
    double on_since_s = 0.0;          // for exposure time (EMCON doctrine)
    bool InAir() const { return radiating && !dummy_load; }
};

// Is azimuth `az` inside the arc a clockwise sweep covered this tick, widened by half a beam?
inline bool InSweep(double sweep_start_az, double swept, double az, double half_beam) {
    if (swept + 2.0 * half_beam >= kTwoPi) return true;
    const double d = WrapTwoPi(az - (sweep_start_az - half_beam));
    return d <= swept + 2.0 * half_beam;
}

// Peak EIRP (W) towards a direction `off_az`, `off_el` from boresight.
inline double EirpW(const EmitterDef& d, double off_az_rad, double off_el_rad) {
    return d.peak_power_w * DbToLinear(AntennaGainDb(d.antenna, off_az_rad, off_el_rad) - d.tx_loss_db);
}

}  // namespace em
}  // namespace simcore
