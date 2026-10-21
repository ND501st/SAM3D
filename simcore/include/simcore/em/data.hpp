// simcore/em/data.hpp — loads electromagnetic definitions from data/em/*.json.
//
// One file holds a library: radars, ESM receivers, jammers, radios and threat libraries. The
// loader is strict like the console loader (unknown keys are errors). Every number may be written
// plainly or as {"value", "source", "confidence", "notes"}; files whose "provenance" is
// "per_value" (real systems, e.g. data/em/osa_ak.json) must use the second form for every
// physical value, and the loader records each one (Provenance) so tools can print the spec.
// Placeholder libraries ("provenance": "placeholder") may use plain numbers; they exist for
// tests and demos only. Angles are given in degrees (keys ending _deg) and stored in radians.
#pragma once

#include <cstddef>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/em/model.hpp"
#include "simcore/json.hpp"

namespace simcore {
namespace em {

struct ValueProvenance {
    std::string path;
    double value = 0.0;
    std::string source;
    std::string confidence;
    std::string notes;
};

struct EmLoadResult {
    bool ok = false;
    std::string error;
    std::string library_id;
    std::string provenance_mode;
    std::vector<ValueProvenance> values;
};

namespace detail {

class EmLoader : public simcore::detail::ConsoleLoader {
public:
    bool Load(const json::Value& root, EmLibrary& lib, EmLoadResult& res) {
        if (!Keys(root, "", {"library_id", "display_name", "status", "provenance", "notes", "sources", "radars", "esm",
                             "jammers", "radios", "emitters", "threat_libraries"})) {
            return false;
        }
        if (!RequireString(root, "", "library_id", res.library_id)) return false;
        std::string status;
        if (!RequireString(root, "", "status", status)) return false;
        if (!RequireString(root, "", "provenance", res.provenance_mode)) return false;
        if (res.provenance_mode != "per_value" && res.provenance_mode != "placeholder")
            return Fail("provenance", "must be per_value or placeholder");
        strict_ = res.provenance_mode == "per_value";
        out_ = &res.values;
        if (const json::Value* arr = OptionalArray(root, "", "sources")) {
            for (std::size_t i = 0; i < arr->items.size(); ++i) {
                const std::string path = Index("sources", i);
                if (!Keys(arr->items[i], path, {"id", "title", "url", "tier", "notes"})) return false;
                std::string t;
                if (!RequireString(arr->items[i], path, "id", t) || !RequireString(arr->items[i], path, "title", t))
                    return false;
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (!LoadArray(root, "radars", [&](const json::Value& v, const std::string& p) { return LoadRadar(v, p, lib); }))
            return false;
        if (!LoadArray(root, "esm", [&](const json::Value& v, const std::string& p) { return LoadEsm(v, p, lib); }))
            return false;
        if (!LoadArray(root, "jammers", [&](const json::Value& v, const std::string& p) { return LoadJammer(v, p, lib); }))
            return false;
        if (!LoadArray(root, "radios", [&](const json::Value& v, const std::string& p) { return LoadRadio(v, p, lib); }))
            return false;
        if (!LoadArray(root, "emitters",
                       [&](const json::Value& v, const std::string& p) { return LoadStandaloneEmitter(v, p, lib); }))
            return false;
        if (!LoadArray(root, "threat_libraries",
                       [&](const json::Value& v, const std::string& p) { return LoadThreatLibrary(v, p, lib); }))
            return false;
        return true;
    }

private:
    template <typename Fn>
    bool LoadArray(const json::Value& root, const char* key, Fn fn) {
        const json::Value* arr = OptionalArray(root, "", key);
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i)
            if (!fn(arr->items[i], Index(key, i))) return false;
        return true;
    }

    // A physical value: plain number (placeholder files) or {value, source, confidence, notes}.
    bool Val(const json::Value& obj, const std::string& path, const char* key, double& out, bool required,
             double scale = 1.0) {
        const json::Value* v = obj.Find(key);
        const std::string p = Join(path, key);
        if (v == nullptr) return required ? Fail(p, "required") : true;
        if (v->Is(json::Type::Number)) {
            if (strict_) return Fail(p, "needs {value, source, confidence} in a per_value file");
            out = v->number * scale;
            return true;
        }
        if (!v->Is(json::Type::Object)) return Fail(p, "expected a number or {value, source, confidence}");
        if (!Keys(*v, p, {"value", "source", "confidence", "notes"})) return false;
        const json::Value* num = v->Find("value");
        if (num == nullptr || !num->Is(json::Type::Number)) return Fail(Join(p, "value"), "expected a number");
        ValueProvenance vp;
        vp.path = p;
        vp.value = num->number;
        if (!RequireString(*v, p, "source", vp.source) || !RequireString(*v, p, "confidence", vp.confidence)) return false;
        if (vp.confidence != "high" && vp.confidence != "medium" && vp.confidence != "low" && vp.confidence != "estimate")
            return Fail(Join(p, "confidence"), "must be high, medium, low or estimate");
        if (!OptionalString(*v, p, "notes", vp.notes)) return false;
        out = num->number * scale;
        out_->push_back(vp);
        return true;
    }

    static constexpr double kDeg = kPi / 180.0;

    bool Item(const json::Value& v, const std::string& path, std::string& id, std::string& name, std::string& source,
              std::string& confidence) {
        if (!RequireString(v, path, "id", id) || !RequireString(v, path, "name", name)) return false;
        if (!RequireString(v, path, "source", source) || !RequireString(v, path, "confidence", confidence)) return false;
        return true;
    }

    bool LoadEmitter(const json::Value& v, const std::string& path, EmitterDef& e) {
        if (!Keys(v, path, {"kind", "freq_hz", "bandwidth_hz", "prf_hz", "pulse_width_s", "peak_power_w", "tx_loss_db",
                            "antenna", "scan", "scan_period_s", "sector_width_deg", "beams", "agility", "channels_hz",
                            "agile_band_hz", "notes"})) {
            return false;
        }
        std::string kind, scan, agility;
        if (!OptionalString(v, path, "kind", kind) || !OptionalString(v, path, "scan", scan) ||
            !OptionalString(v, path, "agility", agility))
            return false;
        if (!kind.empty()) {
            if (kind == "search_radar") e.kind = EmitterKind::SearchRadar;
            else if (kind == "tracking_radar") e.kind = EmitterKind::TrackingRadar;
            else if (kind == "missile_uplink") e.kind = EmitterKind::MissileUplink;
            else if (kind == "missile_responder") e.kind = EmitterKind::MissileResponder;
            else if (kind == "iff") e.kind = EmitterKind::Iff;
            else if (kind == "comms") e.kind = EmitterKind::Comms;
            else if (kind == "jammer") e.kind = EmitterKind::Jammer;
            else if (kind == "navaid") e.kind = EmitterKind::Navaid;
            else if (kind == "decoy") e.kind = EmitterKind::Decoy;
            else return Fail(Join(path, "kind"), "unknown emitter kind '" + kind + "'");
        }
        if (!scan.empty()) {
            if (scan == "fixed") e.scan = ScanKind::Fixed;
            else if (scan == "circular") e.scan = ScanKind::Circular;
            else if (scan == "sector") e.scan = ScanKind::Sector;
            else if (scan == "tracking") e.scan = ScanKind::Tracking;
            else if (scan == "omni") e.scan = ScanKind::Omni;
            else return Fail(Join(path, "scan"), "unknown scan '" + scan + "'");
        }
        if (!agility.empty()) {
            if (agility == "fixed") e.waveform.agility = AgilityKind::Fixed;
            else if (agility == "channels") e.waveform.agility = AgilityKind::Channels;
            else if (agility == "pulse_to_pulse") e.waveform.agility = AgilityKind::PulseToPulse;
            else return Fail(Join(path, "agility"), "unknown agility '" + agility + "'");
        }
        if (!Val(v, path, "freq_hz", e.waveform.freq_hz, true) || !Val(v, path, "bandwidth_hz", e.waveform.bandwidth_hz, false) ||
            !Val(v, path, "prf_hz", e.waveform.prf_hz, false) || !Val(v, path, "pulse_width_s", e.waveform.pulse_width_s, false) ||
            !Val(v, path, "peak_power_w", e.peak_power_w, true) || !Val(v, path, "tx_loss_db", e.tx_loss_db, false) ||
            !Val(v, path, "scan_period_s", e.scan_period_s, false) ||
            !Val(v, path, "sector_width_deg", e.sector_width_rad, false, kDeg) ||
            !Val(v, path, "agile_band_hz", e.waveform.agile_band_hz, false)) {
            return false;
        }
        if (const json::Value* a = v.Find("antenna")) {
            const std::string ap = Join(path, "antenna");
            if (!Keys(*a, ap, {"gain_db", "beamwidth_az_deg", "beamwidth_el_deg", "sidelobe_db", "notes"})) return false;
            if (!Val(*a, ap, "gain_db", e.antenna.gain_db, true) ||
                !Val(*a, ap, "beamwidth_az_deg", e.antenna.beamwidth_az_rad, false, kDeg) ||
                !Val(*a, ap, "beamwidth_el_deg", e.antenna.beamwidth_el_rad, false, kDeg) ||
                !Val(*a, ap, "sidelobe_db", e.antenna.sidelobe_db, false)) {
                return false;
            }
        }
        if (const json::Value* beams = OptionalArray(v, path, "beams")) {
            for (std::size_t i = 0; i < beams->items.size(); ++i) {
                const std::string bp = Index(Join(path, "beams"), i);
                if (!Keys(beams->items[i], bp, {"name", "centre_deg", "height_deg", "notes"})) return false;
                ElevationBeam b;
                if (!Val(beams->items[i], bp, "centre_deg", b.centre_rad, true, kDeg) ||
                    !Val(beams->items[i], bp, "height_deg", b.height_rad, true, kDeg))
                    return false;
                e.beams.push_back(b);
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (const json::Value* ch = OptionalArray(v, path, "channels_hz")) {
            for (std::size_t i = 0; i < ch->items.size(); ++i) {
                if (!ch->items[i].Is(json::Type::Number)) return Fail(Index(Join(path, "channels_hz"), i), "expected a number");
                e.waveform.channels_hz.push_back(ch->items[i].number);
            }
        } else if (!message_.empty()) {
            return false;
        }
        if (e.waveform.bandwidth_hz <= 0.0 && e.waveform.pulse_width_s > 0.0) e.waveform.bandwidth_hz = 1.0 / e.waveform.pulse_width_s;
        return true;
    }

    bool LoadAnchor(const json::Value& v, const std::string& path, RadarAnchor& a) {
        if (!Keys(v, path, {"range_m", "rcs_m2", "target_altitude_m", "pd", "source", "confidence", "notes"})) return false;
        std::string conf;
        if (!RequireString(v, path, "source", a.source) || !RequireString(v, path, "confidence", conf)) return false;
        const bool was = strict_;
        strict_ = false;  // the anchor carries its own provenance
        const bool ok = Val(v, path, "range_m", a.range_m, true) && Val(v, path, "rcs_m2", a.rcs_m2, true) &&
                        Val(v, path, "target_altitude_m", a.target_altitude_m, true) && Val(v, path, "pd", a.pd, false);
        strict_ = was;
        if (!ok) return false;
        a.valid = true;
        out_->push_back({path, a.range_m, a.source, conf, "anchor range"});
        return true;
    }

    bool LoadRadar(const json::Value& v, const std::string& path, EmLibrary& lib) {
        if (!Keys(v, path, {"id", "name", "role", "emitter", "noise_figure_db", "rx_bandwidth_hz", "pfa", "calibration",
                            "low_altitude_check", "antenna_height_m", "min_range_m", "max_range_m", "min_elevation_deg",
                            "max_elevation_deg", "range_sigma_floor_m", "angle_sigma_floor_deg", "monopulse_slope",
                            "range_resolution_m", "mti", "clutter", "tracker", "track_gate_m", "max_slew_deg_s", "iff",
                            "eccm", "low_target_elevation_deg", "source", "confidence", "notes"})) {
            return false;
        }
        RadarDef d;
        if (!Item(v, path, d.id, d.name, d.source, d.confidence)) return false;
        std::string role;
        if (!RequireString(v, path, "role", role)) return false;
        if (role == "search") d.role = RadarRole::Search;
        else if (role == "track") d.role = RadarRole::Track;
        else return Fail(Join(path, "role"), "must be search or track");
        const json::Value* em = v.Find("emitter");
        if (em == nullptr) return Fail(Join(path, "emitter"), "required");
        d.emitter.id = d.id;
        d.emitter.name = d.name;
        d.emitter.kind = d.role == RadarRole::Search ? EmitterKind::SearchRadar : EmitterKind::TrackingRadar;
        d.emitter.scan = d.role == RadarRole::Search ? ScanKind::Circular : ScanKind::Tracking;
        d.emitter.source = d.source;
        d.emitter.confidence = d.confidence;
        if (!LoadEmitter(*em, Join(path, "emitter"), d.emitter)) return false;
        if (!Val(v, path, "noise_figure_db", d.noise_figure_db, true) || !Val(v, path, "rx_bandwidth_hz", d.rx_bandwidth_hz, false) ||
            !Val(v, path, "pfa", d.pfa, false) || !Val(v, path, "antenna_height_m", d.antenna_height_m, false) ||
            !Val(v, path, "min_range_m", d.min_range_m, false) || !Val(v, path, "max_range_m", d.max_range_m, true) ||
            !Val(v, path, "min_elevation_deg", d.min_elevation_rad, false, kDeg) ||
            !Val(v, path, "max_elevation_deg", d.max_elevation_rad, false, kDeg) ||
            !Val(v, path, "range_sigma_floor_m", d.range_sigma_floor_m, false) ||
            !Val(v, path, "angle_sigma_floor_deg", d.angle_sigma_floor_rad, false, kDeg) ||
            !Val(v, path, "monopulse_slope", d.monopulse_slope, false) ||
            !Val(v, path, "range_resolution_m", d.range_resolution_m, false) ||
            !Val(v, path, "track_gate_m", d.track_gate_m, false) ||
            !Val(v, path, "max_slew_deg_s", d.max_slew_rad_s, false, kDeg) ||
            !Val(v, path, "low_target_elevation_deg", d.low_target_elevation_rad, false, kDeg)) {
            return false;
        }
        const json::Value* cal = v.Find("calibration");
        if (cal == nullptr) return Fail(Join(path, "calibration"), "required: the published range the radar is fitted to");
        if (!LoadAnchor(*cal, Join(path, "calibration"), d.calibration)) return false;
        if (const json::Value* low = v.Find("low_altitude_check"))
            if (!LoadAnchor(*low, Join(path, "low_altitude_check"), d.low_altitude_check)) return false;
        if (const json::Value* m = v.Find("mti")) {
            const std::string mp = Join(path, "mti");
            if (!Keys(*m, mp, {"improvement_db", "notch_mps", "notch_attenuation_db", "notes"})) return false;
            d.has_mti = true;
            if (!Val(*m, mp, "improvement_db", d.mti_improvement_db, true) || !Val(*m, mp, "notch_mps", d.mti_notch_mps, true) ||
                !Val(*m, mp, "notch_attenuation_db", d.mti_notch_attenuation_db, false))
                return false;
        }
        if (const json::Value* c = v.Find("clutter")) {
            const std::string cp = Join(path, "clutter");
            if (!Keys(*c, cp, {"cnr_db_at_10km", "max_elevation_deg", "notes"})) return false;
            if (!Val(*c, cp, "cnr_db_at_10km", d.clutter_cnr_db_at_10km, true) ||
                !Val(*c, cp, "max_elevation_deg", d.clutter_max_elevation_rad, false, kDeg))
                return false;
        }
        if (const json::Value* t = v.Find("tracker")) {
            const std::string tp = Join(path, "tracker");
            if (!Keys(*t, tp, {"confirm_hits", "confirm_window", "drop_tentative_misses", "drop_confirmed_misses",
                               "drop_position_sigma_m", "process_noise_accel", "gate_chi2", "initial_velocity_sigma_mps",
                               "notes"}))
                return false;
            const bool was = strict_;
            strict_ = false;  // tracker settings are processing choices, not system data
            double ch = d.tracker.confirm_hits, cw = d.tracker.confirm_window, dt = d.tracker.drop_tentative_misses,
                   dc = d.tracker.drop_confirmed_misses;
            const bool ok = Val(*t, tp, "confirm_hits", ch, false) && Val(*t, tp, "confirm_window", cw, false) &&
                            Val(*t, tp, "drop_tentative_misses", dt, false) && Val(*t, tp, "drop_confirmed_misses", dc, false) &&
                            Val(*t, tp, "drop_position_sigma_m", d.tracker.drop_position_sigma_m, false) &&
                            Val(*t, tp, "process_noise_accel", d.tracker.process_noise_accel, false) &&
                            Val(*t, tp, "gate_chi2", d.tracker.gate_chi2, false) &&
                            Val(*t, tp, "initial_velocity_sigma_mps", d.tracker.initial_velocity_sigma_mps, false);
            strict_ = was;
            if (!ok) return false;
            d.tracker.confirm_hits = static_cast<int>(ch);
            d.tracker.confirm_window = static_cast<int>(cw);
            d.tracker.drop_tentative_misses = static_cast<int>(dt);
            d.tracker.drop_confirmed_misses = static_cast<int>(dc);
        }
        if (const json::Value* f = v.Find("iff")) {
            const std::string fp = Join(path, "iff");
            if (!Keys(*f, fp, {"reply_probability", "source", "confidence", "notes"})) return false;
            d.has_iff = true;
            const bool was = strict_;
            strict_ = false;
            const bool ok = Val(*f, fp, "reply_probability", d.iff_reply_probability, false);
            strict_ = was;
            if (!ok) return false;
        }
        if (const json::Value* e = v.Find("eccm")) {
            const std::string ep = Join(path, "eccm");
            if (!Keys(*e, ep, {"angle_track_on_jam", "rgpo_susceptibility", "strobe_jn_threshold_db", "source", "confidence",
                               "notes"}))
                return false;
            if (!OptionalBool(*e, ep, "angle_track_on_jam", d.angle_track_on_jam)) return false;
            const bool was = strict_;
            strict_ = false;
            const bool ok = Val(*e, ep, "rgpo_susceptibility", d.rgpo_susceptibility, false) &&
                            Val(*e, ep, "strobe_jn_threshold_db", d.strobe_jn_threshold_db, false);
            strict_ = was;
            if (!ok) return false;
        }
        lib.radars.push_back(d);
        return true;
    }

    bool LoadEsm(const json::Value& v, const std::string& path, EmLibrary& lib) {
        if (!Keys(v, path, {"id", "name", "kind", "freq_min_hz", "freq_max_hz", "sensitivity_dbw", "antenna_gain_db",
                            "bearing_sigma_deg", "freq_sigma_fraction", "pri_sigma_fraction", "pw_sigma_fraction",
                            "time_sigma_s", "line_interval_s", "contact_timeout_s", "threat_library", "source",
                            "confidence", "notes"})) {
            return false;
        }
        EsmDef d;
        if (!Item(v, path, d.id, d.name, d.source, d.confidence)) return false;
        std::string kind;
        if (!RequireString(v, path, "kind", kind)) return false;
        if (kind == "rwr") d.kind = EsmKind::Rwr;
        else if (kind == "esm") d.kind = EsmKind::Esm;
        else if (kind == "elint") d.kind = EsmKind::Elint;
        else if (kind == "tdoa_site") d.kind = EsmKind::TdoaSite;
        else if (kind == "comms_intercept") d.kind = EsmKind::CommsIntercept;
        else return Fail(Join(path, "kind"), "unknown receiver kind '" + kind + "'");
        if (!Val(v, path, "freq_min_hz", d.freq_min_hz, true) || !Val(v, path, "freq_max_hz", d.freq_max_hz, true) ||
            !Val(v, path, "sensitivity_dbw", d.sensitivity_dbw, true) || !Val(v, path, "antenna_gain_db", d.antenna_gain_db, false) ||
            !Val(v, path, "bearing_sigma_deg", d.bearing_sigma_rad, true, kDeg) ||
            !Val(v, path, "freq_sigma_fraction", d.freq_sigma_fraction, false) ||
            !Val(v, path, "pri_sigma_fraction", d.pri_sigma_fraction, false) ||
            !Val(v, path, "pw_sigma_fraction", d.pw_sigma_fraction, false) || !Val(v, path, "time_sigma_s", d.time_sigma_s, false) ||
            !Val(v, path, "line_interval_s", d.line_interval_s, false) ||
            !Val(v, path, "contact_timeout_s", d.contact_timeout_s, false) ||
            !OptionalString(v, path, "threat_library", d.threat_library)) {
            return false;
        }
        lib.esm.push_back(d);
        return true;
    }

    bool LoadJammer(const json::Value& v, const std::string& path, EmLibrary& lib) {
        if (!Keys(v, path, {"id", "name", "technique", "emitter", "rgpo_walk_mps", "rgpo_max_m", "rgpo_off_s",
                            "rgpo_js_needed_db", "retune_s", "false_targets", "false_spacing_m", "false_jn_needed_db",
                            "source", "confidence", "notes"})) {
            return false;
        }
        JammerDef d;
        if (!Item(v, path, d.id, d.name, d.source, d.confidence)) return false;
        std::string tech;
        if (!RequireString(v, path, "technique", tech)) return false;
        if (tech == "barrage") d.technique = JamTechnique::Barrage;
        else if (tech == "spot") d.technique = JamTechnique::Spot;
        else if (tech == "rgpo") d.technique = JamTechnique::RangeGatePullOff;
        else if (tech == "false_targets") d.technique = JamTechnique::FalseTargets;
        else return Fail(Join(path, "technique"), "must be barrage, spot, rgpo or false_targets");
        const json::Value* em = v.Find("emitter");
        if (em == nullptr) return Fail(Join(path, "emitter"), "required");
        d.emitter.id = d.id;
        d.emitter.name = d.name;
        d.emitter.kind = EmitterKind::Jammer;
        d.emitter.scan = ScanKind::Fixed;
        if (!LoadEmitter(*em, Join(path, "emitter"), d.emitter)) return false;
        if (!Val(v, path, "rgpo_walk_mps", d.rgpo_walk_mps, false) || !Val(v, path, "rgpo_max_m", d.rgpo_max_m, false) ||
            !Val(v, path, "rgpo_off_s", d.rgpo_off_s, false) || !Val(v, path, "rgpo_js_needed_db", d.rgpo_js_needed_db, false))
            return false;
        double count = 0.0;
        if (!Val(v, path, "retune_s", d.retune_s, false) || !Val(v, path, "false_targets", count, false) ||
            !Val(v, path, "false_spacing_m", d.false_spacing_m, false) ||
            !Val(v, path, "false_jn_needed_db", d.false_jn_needed_db, false))
            return false;
        if (count < 0.0 || count > 50.0) return Fail(Join(path, "false_targets"), "0 to 50");
        d.false_targets = static_cast<int>(count);
        if (d.technique == JamTechnique::FalseTargets && d.false_targets < 1)
            return Fail(Join(path, "false_targets"), "a false-target repeater needs at least one");
        lib.jammers.push_back(d);
        return true;
    }

    bool LoadRadio(const json::Value& v, const std::string& path, EmLibrary& lib) {
        if (!Keys(v, path, {"id", "name", "freq_min_hz", "freq_max_hz", "power_w", "antenna_gain_db", "antenna_height_m",
                            "sensitivity_dbw", "required_snr_db", "bandwidth_hz", "system_loss_db", "cosite_desense_db",
                            "voice", "message_s", "operator_delay_s", "tracks_per_message", "range_anchor",
                            "source", "confidence", "notes"})) {
            return false;
        }
        RadioDef d;
        if (!Item(v, path, d.id, d.name, d.source, d.confidence)) return false;
        double per = d.tracks_per_message;
        if (!Val(v, path, "freq_min_hz", d.freq_min_hz, true) || !Val(v, path, "freq_max_hz", d.freq_max_hz, true) ||
            !Val(v, path, "power_w", d.power_w, true) || !Val(v, path, "antenna_gain_db", d.antenna_gain_db, false) ||
            !Val(v, path, "antenna_height_m", d.antenna_height_m, false) ||
            !Val(v, path, "sensitivity_dbw", d.sensitivity_dbw, true) ||
            !Val(v, path, "required_snr_db", d.required_snr_db, false) || !Val(v, path, "bandwidth_hz", d.bandwidth_hz, false) ||
            !Val(v, path, "system_loss_db", d.system_loss_db, false) ||
            !Val(v, path, "cosite_desense_db", d.cosite_desense_db, false) || !OptionalBool(v, path, "voice", d.voice) ||
            !Val(v, path, "message_s", d.message_s, false) || !Val(v, path, "operator_delay_s", d.operator_delay_s, false) ||
            !Val(v, path, "tracks_per_message", per, false)) {
            return false;
        }
        d.tracks_per_message = static_cast<int>(per);
        if (const json::Value* a = v.Find("range_anchor")) {
            const std::string ap = Join(path, "range_anchor");
            if (!Keys(*a, ap, {"range_m", "antenna_height_m", "freq_hz", "cosite_range_m", "source", "confidence", "notes"}))
                return false;
            std::string src, conf;
            if (!RequireString(*a, ap, "source", src) || !RequireString(*a, ap, "confidence", conf)) return false;
            const bool was = strict_;
            strict_ = false;  // the anchor carries its own provenance
            const bool ok = Val(*a, ap, "range_m", d.anchor_range_m, true) &&
                            Val(*a, ap, "antenna_height_m", d.anchor_height_m, true) &&
                            Val(*a, ap, "freq_hz", d.anchor_freq_hz, false) &&
                            Val(*a, ap, "cosite_range_m", d.cosite_range_m, false);
            strict_ = was;
            if (!ok) return false;
            out_->push_back({ap, d.anchor_range_m, src, conf, "radio range anchor"});
        }
        lib.radios.push_back(d);
        return true;
    }

    // An emitter that is not part of a sensor model: an aircraft's radar seen only as a signal,
    // a navigation beacon, a decoy emitter. Same keys as a radar's "emitter" plus identity.
    bool LoadStandaloneEmitter(const json::Value& v, const std::string& path, EmLibrary& lib) {
        if (!v.Is(json::Type::Object)) return Fail(path, "expected an object");
        EmitterDef d;
        if (!Item(v, path, d.id, d.name, d.source, d.confidence)) return false;
        json::Value body;
        body.type = json::Type::Object;
        for (std::size_t i = 0; i < v.keys.size(); ++i) {
            const std::string& k = v.keys[i];
            if (k == "id" || k == "name" || k == "source" || k == "confidence") continue;
            body.keys.push_back(k);
            body.items.push_back(v.items[i]);
        }
        if (!LoadEmitter(body, path, d)) return false;
        lib.emitters.push_back(d);
        return true;
    }

    bool LoadThreatLibrary(const json::Value& v, const std::string& path, EmLibrary& lib) {
        if (!Keys(v, path, {"id", "name", "owner", "notes", "entries"})) return false;
        ThreatLibrary tl;
        std::string name, owner;
        if (!RequireString(v, path, "id", tl.id) || !RequireString(v, path, "name", name) ||
            !OptionalString(v, path, "owner", owner))
            return false;
        const json::Value* arr = OptionalArray(v, path, "entries");
        if (arr == nullptr) return Fail(Join(path, "entries"), "required");
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const std::string ep = Index(Join(path, "entries"), i);
            const json::Value& e = arr->items[i];
            if (!Keys(e, ep, {"id", "name", "kind", "owner", "platform", "cw", "freq_min_hz", "freq_max_hz", "prf_min_hz",
                              "prf_max_hz", "pw_min_s", "pw_max_s", "source", "confidence", "notes"}))
                return false;
            ThreatEntry t;
            std::string side;
            if (!Item(e, ep, t.id, t.name, t.source, t.confidence) || !RequireString(e, ep, "kind", t.kind) ||
                !RequireString(e, ep, "owner", side))
                return false;
            if (side == "blue") t.owner = Side::Blue;
            else if (side == "red") t.owner = Side::Red;
            else if (side == "neutral") t.owner = Side::Neutral;
            else return Fail(Join(ep, "owner"), "must be blue, red or neutral");
            std::string platform = "unknown";
            if (!OptionalString(e, ep, "platform", platform) || !OptionalBool(e, ep, "cw", t.cw)) return false;
            if (platform == "air") t.max_speed_mps = 350.0;
            else if (platform == "ground") t.max_speed_mps = 20.0;
            else if (platform == "ship") t.max_speed_mps = 15.0;
            else if (platform != "unknown") return Fail(Join(ep, "platform"), "must be air, ground, ship or unknown");
            const bool was = strict_;
            strict_ = false;  // a library is one side's belief; the entry's source covers it
            const bool ok = Val(e, ep, "freq_min_hz", t.freq_min_hz, true) && Val(e, ep, "freq_max_hz", t.freq_max_hz, true) &&
                            Val(e, ep, "prf_min_hz", t.prf_min_hz, false) && Val(e, ep, "prf_max_hz", t.prf_max_hz, false) &&
                            Val(e, ep, "pw_min_s", t.pw_min_s, false) && Val(e, ep, "pw_max_s", t.pw_max_s, false);
            strict_ = was;
            if (!ok) return false;
            tl.entries.push_back(t);
        }
        lib.threat_libraries.push_back(tl);
        return true;
    }

    bool strict_ = false;
    std::vector<ValueProvenance>* out_ = nullptr;
};

}  // namespace detail

// Parse a library from JSON text and append its definitions to `lib`.
inline EmLoadResult LoadEmLibraryText(const std::string& text, EmLibrary& lib) {
    EmLoadResult res;
    json::Value root;
    std::string err;
    if (!json::Parse(text, root, &err)) {
        res.error = err;
        return res;
    }
    detail::EmLoader loader;
    EmLibrary staged = lib;
    if (!loader.Load(root, staged, res)) {
        res.error = loader.Message();
        return res;
    }
    lib = std::move(staged);
    res.ok = true;
    return res;
}

inline EmLoadResult LoadEmLibraryFile(const std::string& path, EmLibrary& lib) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        EmLoadResult res;
        res.error = "cannot open " + path;
        return res;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    EmLoadResult res = LoadEmLibraryText(ss.str(), lib);
    if (!res.ok) res.error = path + ": " + res.error;
    return res;
}

}  // namespace em
}  // namespace simcore
