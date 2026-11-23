// simcore/ground_war/picture.hpp — each side's picture of the ground war (track F, F3.5; docs/fronts/03_GROUND_WAR.md
// section 9).
//
// Each side's commanders above a formation decide on what has reached their staffs, never on the truth:
//   * its own units (strength fraction and power) from the units' reports: scheduled (reports.division_per_day, from the
//     local hour picture.report_first_h) and at once when a unit reports, asks, stops, breaks or re-forms, each arriving
//     picture.own_delay_h after it was compiled, with a small spread (own_sigma);
//   * the other side's units with an age and an error: ours of the enemy in contact from our units' contact reports
//     (riding with them, contact_sigma) and of the rest from the army's intelligence summary (army_intel: once a day,
//     late, army_intel_sigma); the enemy's of ours from its air reconnaissance where its IntelPicture holds a troops
//     entry seen lately in the sector (or near a unit off the line: air_sigma) and from its ground reconnaissance at the
//     line elsewhere (ground_recon_sigma, its own delay); the prior picture at the war's start (prior_sigma);
//   * each axis's depth, pace and whether our counterattack is on it, and the line's offsets by station, from the same
//     scheduled reports (the depths without error: a unit knows where it stands and where the enemy faces it).
// A believed strength is the truth when the report was compiled times a lognormal factor of mean 1 drawn once per
// (side, source, sector, report hour), a fork of the ground war's own root: order-free, and the same for every
// formation of that sector in that report. A formation's own commander knows his own unit's state (GroundWar's
// OwnFraction): his reaction time is his staff's count.
//
// The picture is a few numbers per formation and axis and a line per side, sized when the war is built and changed only
// when a report arrives (GroundWar's event queue): nothing every hour, no allocation in the hour.
//
// First person: our side's picture is a staff's belief, not the player's; F3.8 routes it into the pipeline as situation
// reports. Nothing here feeds a HUD.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "simcore/air_war/types.hpp"
#include "simcore/ground_war/ground_data.hpp"
#include "simcore/ground_war/units.hpp"

namespace simcore {
namespace groundwar {

// Where a belief came from.
enum class PictureSource : std::uint8_t { None, Prior, Own, Contact, ArmyIntel, Air, GroundRecon };
constexpr std::size_t kPictureSourceCount = 7;

inline const char* PictureSourceName(PictureSource s) {
    switch (s) {
        case PictureSource::None: return "none";
        case PictureSource::Prior: return "prior";
        case PictureSource::Own: return "own";
        case PictureSource::Contact: return "contact";
        case PictureSource::ArmyIntel: return "army_intel";
        case PictureSource::Air: return "air";
        case PictureSource::GroundRecon: return "ground_recon";
    }
    return "?";
}

// The spread of a source's believed strengths (picture's game values).
inline double SourceSigma(const PictureTables& t, PictureSource s) {
    switch (s) {
        case PictureSource::None: return 0.0;
        case PictureSource::Prior: return t.prior_sigma;
        case PictureSource::Own: return t.own_sigma;
        case PictureSource::Contact: return t.contact_sigma;
        case PictureSource::ArmyIntel: return t.army_intel_sigma;
        case PictureSource::Air: return t.air_sigma;
        case PictureSource::GroundRecon: return t.ground_recon_sigma;
    }
    return 0.0;
}

// How long a source's report takes to reach the staff (the prior is there at the start).
inline double SourceDelayHours(const PictureTables& t, PictureSource s) {
    switch (s) {
        case PictureSource::None: return 0.0;
        case PictureSource::Prior: return 0.0;
        case PictureSource::Own: return t.own_delay_h;
        case PictureSource::Contact: return t.own_delay_h;  // rides with the unit's own report
        case PictureSource::ArmyIntel: return t.army_intel_delay_h;
        case PictureSource::Air: return t.ground_recon_delay_h;  // the enemy's staff collates its air and ground sightings together
        case PictureSource::GroundRecon: return t.ground_recon_delay_h;
    }
    return 0.0;
}

// The factor a report's strengths are believed at: lognormal, mean 1, keyed by (side, source, sector, report hour) on
// the war's root (the same for every formation of the sector in that report, whatever else is drawn).
inline double BeliefFactor(const airwar::Rng& root, Side by, PictureSource source, int sector, double report_h, double sigma) {
    if (!(sigma > 0.0)) return 1.0;
    const std::uint64_t hour = static_cast<std::uint64_t>(static_cast<std::int64_t>(std::floor(report_h + 0.5)) + 1000000);
    const std::uint64_t k = 0x9C70000000000000ull ^ (static_cast<std::uint64_t>(by == Side::Ours ? 1 : 2) << 52) ^
                            (static_cast<std::uint64_t>(source) << 44) ^ (static_cast<std::uint64_t>(sector + 1) << 32) ^ hour;
    airwar::Rng r = root.Fork(k);
    return std::exp(sigma * r.Normal() - 0.5 * sigma * sigma);
}

// What a side believes of one formation (its own or the other side's).
struct UnitBelief {
    double fraction = 1.0;     // believed strength over its establishment
    double power = 0.0;        // believed power (strength x supply and cohesion)
    double observed_h = 0.0;   // when the report was compiled (its age is the hour now less this)
    double factor = 1.0;       // the error drawn: believed over the truth when compiled
    int sector = -1;           // where it was seen (-1 behind the line, or not known)
    Posture posture = Posture::HastyDefence;
    PictureSource source = PictureSource::None;
    bool out = false;          // believed out of the fight (withdrawn or dispersed when it was reported)
};

// What a side believes of an axis.
struct AxisBelief {
    double depth_m = 0.0;
    double pace_km_day = 0.0;
    double observed_h = 0.0;
    bool counterattacked = false;  // our counterattack on it
};

// One side's picture: by formation and axis (the war's own indices), the line by station, and, for the enemy's picture,
// the sectors where its own ground front is collapsing (the air front's crisis input, air_front.hpp).
struct GroundPicture {
    Side side = Side::Ours;
    std::vector<UnitBelief> units;
    std::vector<AxisBelief> axes;
    std::vector<double> line_m;          // the believed offsets by station
    double line_observed_h = 0.0;
    std::vector<double> pending_line_m;  // the scheduled report's line, in flight
    double pending_line_h = -1.0;        // when it was compiled (-1: none in flight)
    std::vector<char> crisis;            // by sector: its own front collapsing in this picture
    std::uint64_t reports = 0;           // reports merged (a formation's or an axis's)

    void Size(Side s, std::size_t formations, std::size_t axes_n, std::size_t stations, std::size_t sectors) {
        side = s;
        units.assign(formations, UnitBelief());
        axes.assign(axes_n, AxisBelief());
        line_m.assign(stations, 0.0);
        pending_line_m.assign(stations, 0.0);
        line_observed_h = 0.0;
        pending_line_h = -1.0;
        crisis.assign(sectors, 0);
        reports = 0;
    }

    // A formation's report arrives: newer news replaces older (a late scheduled report never undoes a later one).
    bool Merge(std::size_t i, const UnitBelief& b) {
        UnitBelief& u = units[i];
        if (b.observed_h < u.observed_h - 1e-9) return false;
        u = b;
        ++reports;
        return true;
    }
    bool MergeAxis(std::size_t a, const AxisBelief& b) {
        AxisBelief& x = axes[a];
        if (b.observed_h < x.observed_h - 1e-9) return false;
        x = b;
        ++reports;
        return true;
    }
};

}  // namespace groundwar
}  // namespace simcore
