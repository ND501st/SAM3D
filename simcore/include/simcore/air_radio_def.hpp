// simcore/air_radio_def.hpp — what a scenario says about the enemy's air radio (roadmap B9,
// docs/air_battle/09_COMMS_AND_INTERCEPT.md).
//
// A scenario's "air_radio" block puts the flights' calls on the air as transmissions in a session-level EM world
// (air_war/air_radio.hpp): the radio sets (a library of em/), the faction's band and language, its radio discipline and the
// tuning of the calls, the ground posts that speak (a GCI, a CRC, a FAC) and our intercept posts that listen. A flight's
// "radio" block says which unit it belongs to and how the people in it move the faction's discipline. A scenario without the
// block keeps B1's fixed delay for a flight's calls and has no intercept.
//
// Std only; the loader is in scenario.hpp.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "simcore/em/model.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

// One flight's own radio: its unit (call sign continuity, HAVE QUICK and secure voice are per squadron), its role (a strike
// flies comm-out on its way in), and how its people move the faction's discipline (the officer's quirks, the pilots'
// nerve: docs/air_battle/09 section 3.2).
struct FlightRadioInfo {
    std::string unit;               // its squadron's id ("" the flight's own)
    std::string role;               // "strike", "dead", "sead", "hunter", "escort", "recce", "helicopter", "support"
    double discipline_adjust = 0.0; // added to the faction's discipline (the officer's, at most about +-0.2; pilots' +-0.15)
    bool have_quick = false;        // the unit's net hops (content followed with difficulty; 0 by default)
    bool secure_voice = false;
    bool comm_out = false;          // briefed to fly in radio silence until its attack (the builder's decision)
    std::string codeword;           // what a comm-out strike says after its attack ("" none)
};

// A ground or airborne post of the enemy's that speaks on a net (a GCI, a CRC, a FAC, an E-3A on its orbit), or one of
// ours that listens (an intercept post). Positions are the session's ENU.
struct AirRadioPostDef {
    std::string id;
    std::string name;
    std::string kind = "intercept";  // "intercept" (ours, listening), "gci", "crc", "fac", "awacs" (the enemy's, speaking),
                                     // "our_gci" (ours, speaking to our flights on our_radio: roadmap B10)
    Vec3 at;                         // east, north; z unused (the antenna's height is mast_m above the ground)
    double mast_m = 30.0;            // antenna height above the ground (an airborne post: its orbit height)
    std::string esm;                 // intercept: the receiver's id in the library
    std::string radio;               // a speaker: the set's id in the library ("" the scenario's ground set)
    double freq_hz = 0.0;            // a speaker: its net (0: the scenario's first channel)
    int channels = 3;                // a GCI: the groups it can guide at once (docs/air_battle/09 section 2.6)
    std::string chain_post;          // intercept: the post in the chain of command it is (data/command/intercept.json)
};

// The tuning of the calls (data/gameplay/air_war.json "radio"; the air war's TuningTable lists the same keys). Game values
// unless the notes there say otherwise.
inline const std::map<std::string, double>& AirRadioTuningDefaults() {
    static const std::map<std::string, double> d = {
        {"radio.call_check_in_s", 15.0},    // lengths of a call by kind: the mid-range of docs/air_battle/09 section 2.2
        {"radio.call_threat_call_s", 3.5},
        {"radio.call_report_s", 20.0},
        {"radio.call_request_s", 40.0},
        {"radio.call_divert_s", 18.0},
        {"radio.call_vector_s", 5.0},
        {"radio.call_codeword_s", 2.0},
        {"radio.call_admin_s", 6.0},
        {"radio.stale_threat_call_s", 15.0},  // a call older than its use is dropped (section 2.5)
        {"radio.stale_vector_s", 10.0},
        {"radio.stale_other_s", 120.0},
        {"radio.stress_repeat", 0.5},         // a call at full stress is this much longer (said again)
        {"radio.have_quick_content", 0.2},    // the chance a listener follows the content of a hopping net
        {"radio.signal_factor", 1.5},         // a silent flight's act passed by signal takes this much longer than a call
        {"radio.signal_range_m", 5000.0},     // and reaches only those this close to the leader
        {"radio.vector_hold_s", 30.0},        // a GCI channel stays with a group this long after its last vector
        {"radio.continuity_tau_h", 3.6},      // continuity 1 - exp(-heard hours / tau): 0.8 after about 6 h of steady traffic
        {"radio.continuity_gap_cap_s", 600.0},// a gap between calls counts as listening time up to this
        {"radio.continuity_name_at", 0.5},    // from this continuity the analysts name the unit
        {"radio.opsec_alert_at0", 0.34},      // share of raids alerted ahead at opsec 0 (nsa_purple_dragon_1993 pp. 22-26)
        {"radio.opsec_alert_at1", 0.05},      // ... and at opsec 1
        {"radio.opsec_lead_at0_h", 8.5},      // average warning at opsec 0 (hours), at opsec 1 below
        {"radio.opsec_lead_at1_h", 0.4},
        {"radio.opsec_lead_spread", 0.3},     // the lead drawn within this share of its average, either way
        {"radio.opsec_wrong_share", 0.2},     // an alert whose window names the wrong hours
        {"radio.opsec_loose_lips", 0.4},      // opsec lost to a planner with the comsec_leak quirk
        {"radio.discipline_loose_lips", 0.2}, // discipline lost on his nets
        {"radio.discipline_chatter", 0.15},   // the chatterbox's and the silent runner's step
        {"radio.abstract_calls", 6.0},        // calls an abstract package's flight makes on the way (section 2.4)
    };
    return d;
}

struct AirRadioDef {
    bool on = false;
    std::string faction;             // "nato", "warsaw_pact", ...
    std::string library_file;        // the EM library of the sets and receivers ("em/air_radios_1985.json")
    std::string air_radio;           // the aircraft's set (library id)
    std::string ground_radio;        // controllers' set
    std::string our_radio;           // our side's air set (roadmap B10): our GCI's posts ("our_gci") and our flights speak on it
    double our_freq_hz = 124e6;      // our flights' first net (the R-832M's VHF band, valka_r832m: low confidence)
    double freq_hz = 300e6;          // the first flight net; each flight's is a channel above it
    double channel_hz = 25e3;
    std::string language = "en";     // what its calls are in: "en", "ru", "de", "pannonian" (ours)
    double discipline = 0.5;         // silence on the attack, 0..1
    double callsign_rotation_h = 24.0;
    double have_quick_share = 0.0;
    double secure_voice_share = 0.0;
    double opsec = 0.5;
    std::map<std::string, double> tune;  // overrides of AirRadioTuningDefaults() (air_war.json "radio")
    std::vector<AirRadioPostDef> posts;
    em::EmLibrary library;
    std::string source;
    std::string confidence;

    double Tune(const std::string& key) const {
        const auto it = tune.find(key);
        if (it != tune.end()) return it->second;
        const auto d = AirRadioTuningDefaults().find(key);
        return d == AirRadioTuningDefaults().end() ? 0.0 : d->second;
    }
};

}  // namespace simcore
