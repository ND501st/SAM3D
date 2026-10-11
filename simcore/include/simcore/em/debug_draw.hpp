// simcore/em/debug_draw.hpp — engine-agnostic debug overlays for the electromagnetic layer.
//
// BuildDebugScene turns the scene into a flat list of primitives (lines, circles, ellipses,
// sectors, text) in ENU metres, each tagged with a layer bit. Every front end draws the same list:
// Unreal with DrawDebug* behind the `sam3d.em.Debug <mask>` console variable, the browser viewer
// (tools/em_view.py) from the JSON that examples/em_demo.cpp writes. Overlays may show truth
// (that is their job); the simulation itself never reads them.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/em/comms_system.hpp"
#include "simcore/em/detection.hpp"
#include "simcore/em/geolocation.hpp"
#include "simcore/em/model.hpp"
#include "simcore/em/radar_system.hpp"
#include "simcore/em/scene_ops.hpp"

namespace simcore {
namespace em {

enum DebugLayer : std::uint32_t {
    kLayerTruth = 1u << 0,          // real positions and velocities
    kLayerEmitters = 1u << 1,       // who is radiating
    kLayerBeams = 1u << 2,          // antenna pointing, scan
    kLayerCoverage = 1u << 3,       // instrumented range, calibrated detection ring
    kLayerLineOfSight = 1u << 4,    // radar -> candidate: clear or masked
    kLayerSignalPaths = 1u << 5,    // emitter -> receiver for every intercept this tick
    kLayerBearings = 1u << 6,       // ESM lines of bearing with their error wedge
    kLayerFixes = 1u << 7,          // triangulated / TDOA emitter positions
    kLayerUncertainty = 1u << 8,    // 95 % ellipses of a node's picture
    kLayerTracks = 1u << 9,         // sensor tracks, quality
    kLayerC2Links = 1u << 10,       // radio links with margin
    kLayerCommsTraffic = 1u << 11,  // transmissions on the air
    kLayerJamming = 1u << 12,       // jammer beams, strobes, jammed detection envelope
    kLayerSensorStatus = 1u << 13,  // text: modes, C2 status
    kLayerDetection = 1u << 14,     // per-look Pd and SINR
    kLayerAll = 0xFFFFFFFFu
};

inline const char* DebugLayerName(std::uint32_t bit) {
    switch (bit) {
        case kLayerTruth: return "truth";
        case kLayerEmitters: return "emitters";
        case kLayerBeams: return "beams";
        case kLayerCoverage: return "coverage";
        case kLayerLineOfSight: return "line_of_sight";
        case kLayerSignalPaths: return "signal_paths";
        case kLayerBearings: return "bearings";
        case kLayerFixes: return "fixes";
        case kLayerUncertainty: return "uncertainty";
        case kLayerTracks: return "tracks";
        case kLayerC2Links: return "c2_links";
        case kLayerCommsTraffic: return "comms_traffic";
        case kLayerJamming: return "jamming";
        case kLayerSensorStatus: return "sensor_status";
        case kLayerDetection: return "detection";
        default: return "?";
    }
}

enum class DebugShape { Line, Circle, Ellipse, Sector, Text, Point, Polygon };

struct DebugPrim {
    DebugShape shape = DebugShape::Line;
    std::uint32_t layer = 0;
    Vec3 a;               // start / centre / anchor
    Vec3 b;               // end (lines)
    double r1 = 0.0;      // radius, semi-major, or sector radius
    double r2 = 0.0;      // semi-minor
    double angle = 0.0;   // ellipse major-axis azimuth, sector centre azimuth
    double angle2 = 0.0;  // sector half-width
    std::uint32_t rgba = 0xFFFFFFFFu;
    float width = 1.0f;
    bool dashed = false;
    std::vector<Vec3> points;  // polygons
    std::string text;
    int owner = 0;             // knowledge overlays: the node whose belief this is (0 = scene-wide)
};

namespace colors {
constexpr std::uint32_t kBlue = 0x3D8BFFFFu;
constexpr std::uint32_t kRed = 0xFF4B3EFFu;
constexpr std::uint32_t kNeutral = 0xC8C8C8FFu;
constexpr std::uint32_t kGood = 0x3CD070FFu;
constexpr std::uint32_t kBad = 0xFF3B30FFu;
constexpr std::uint32_t kAmber = 0xFFB020FFu;
constexpr std::uint32_t kCyan = 0x30D5E0FFu;
constexpr std::uint32_t kMagenta = 0xE040E0FFu;
constexpr std::uint32_t kWhite = 0xF0F0F0FFu;
constexpr std::uint32_t kGrey = 0x8C8C8CFFu;
}  // namespace colors

inline std::uint32_t SideColor(Side s) {
    return s == Side::Blue ? colors::kBlue : s == Side::Red ? colors::kRed : colors::kNeutral;
}

inline std::uint32_t WithAlpha(std::uint32_t rgba, std::uint32_t a) { return (rgba & 0xFFFFFF00u) | (a & 0xFFu); }

inline std::string Fmt(const char* f, double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), f, v);
    return buf;
}

class DebugSceneBuilder {
public:
    DebugSceneBuilder(std::vector<DebugPrim>& out, std::uint32_t layers) : out_(out), layers_(layers) {}

    bool On(std::uint32_t layer) const { return (layers_ & layer) != 0; }
    void SetOwner(int node) { owner_ = node; }

    void Line(std::uint32_t layer, const Vec3& a, const Vec3& b, std::uint32_t c, float w = 1.0f, bool dashed = false) {
        if (!On(layer)) return;
        DebugPrim p;
        p.shape = DebugShape::Line;
        p.layer = layer;
        p.a = a;
        p.b = b;
        p.rgba = c;
        p.width = w;
        p.dashed = dashed;
        Push(p);
    }
    void Circle(std::uint32_t layer, const Vec3& c, double r, std::uint32_t col, bool dashed = false) {
        if (!On(layer)) return;
        DebugPrim p;
        p.shape = DebugShape::Circle;
        p.layer = layer;
        p.a = c;
        p.r1 = r;
        p.rgba = col;
        p.dashed = dashed;
        Push(p);
    }
    void Ellipse(std::uint32_t layer, const Vec3& c, double major, double minor, double az, std::uint32_t col) {
        if (!On(layer)) return;
        DebugPrim p;
        p.shape = DebugShape::Ellipse;
        p.layer = layer;
        p.a = c;
        p.r1 = major;
        p.r2 = minor;
        p.angle = az;
        p.rgba = col;
        Push(p);
    }
    void Sector(std::uint32_t layer, const Vec3& c, double r, double az, double half, std::uint32_t col) {
        if (!On(layer)) return;
        DebugPrim p;
        p.shape = DebugShape::Sector;
        p.layer = layer;
        p.a = c;
        p.r1 = r;
        p.angle = az;
        p.angle2 = half;
        p.rgba = col;
        Push(p);
    }
    void Text(std::uint32_t layer, const Vec3& at, const std::string& t, std::uint32_t col) {
        if (!On(layer)) return;
        DebugPrim p;
        p.shape = DebugShape::Text;
        p.layer = layer;
        p.a = at;
        p.text = t;
        p.rgba = col;
        Push(p);
    }
    void Point(std::uint32_t layer, const Vec3& at, std::uint32_t col, float size = 4.0f, const std::string& label = "") {
        if (!On(layer)) return;
        DebugPrim p;
        p.shape = DebugShape::Point;
        p.layer = layer;
        p.a = at;
        p.rgba = col;
        p.width = size;
        p.text = label;
        Push(p);
    }
    void Polygon(std::uint32_t layer, const std::vector<Vec3>& pts, std::uint32_t col, bool dashed = false) {
        if (!On(layer)) return;
        DebugPrim p;
        p.shape = DebugShape::Polygon;
        p.layer = layer;
        p.points = pts;
        p.rgba = col;
        p.dashed = dashed;
        Push(p);
    }

private:
    void Push(DebugPrim& p) {
        p.owner = owner_;
        out_.push_back(p);
    }
    std::vector<DebugPrim>& out_;
    std::uint32_t layers_;
    int owner_ = 0;
};

inline Vec3 AlongAzimuth(const Vec3& from, double az, double len) {
    return from + Vec3{std::sin(az), std::cos(az), 0.0} * len;
}

// Calibrated free-space detection range of a radar against `rcs_m2` with its main beam on the
// target (no terrain, no multipath), optionally reduced by noise jamming J/N in that direction.
inline double FreeSpaceDetectionRange(const RadarDef& d, double rcs_m2, double pd, double jn_linear = 0.0) {
    const double req = SwerlingIRequiredSnr(pd, d.pfa) * (1.0 + jn_linear);
    if (d.loop_gain <= 0.0 || req <= 0.0) return 0.0;
    return std::pow(d.loop_gain * rcs_m2 / req, 0.25);
}

// Build the overlay list. `picture_node` selects whose knowledge to draw: a node id, 0 = every
// command post, -1 = every node (each primitive carries its owner so a viewer can filter).
inline std::vector<DebugPrim> BuildDebugScene(EmScene& s, std::uint32_t layers, int picture_node = 0,
                                              double reference_rcs_m2 = 1.0) {
    std::vector<DebugPrim> out;
    DebugSceneBuilder b(out, layers);

    for (const Platform& p : s.platforms) {
        if (!p.alive) continue;
        b.Point(kLayerTruth, p.position, SideColor(p.side), 4.0f, p.name);
        if (p.velocity.LengthSquared() > 1.0) b.Line(kLayerTruth, p.position, p.position + p.velocity * 30.0, SideColor(p.side));
    }

    for (const EmitterState& e : s.emitters) {
        const Platform* p = s.FindPlatform(e.platform_id);
        if (p == nullptr || !e.InAir()) continue;
        const std::uint32_t c = e.def.kind == EmitterKind::Jammer ? colors::kMagenta
                                : e.def.kind == EmitterKind::Comms ? colors::kCyan
                                                                   : colors::kAmber;
        b.Circle(kLayerEmitters, p->position, 1500.0, c);
        b.Text(kLayerEmitters, p->position + Vec3{0.0, -2500.0, 0.0}, e.def.name, c);
        if (e.def.scan == ScanKind::Circular) {
            b.Sector(kLayerBeams, p->position, 25000.0, e.pointing_az_rad, MaxOf(0.5 * e.def.antenna.beamwidth_az_rad, 0.01),
                     WithAlpha(colors::kAmber, 0x80));
        } else if (e.def.scan == ScanKind::Tracking || e.def.scan == ScanKind::Fixed) {
            if (e.def.antenna.beamwidth_az_rad > 0.0)
                b.Sector(kLayerBeams, p->position, 30000.0, e.pointing_az_rad, MaxOf(0.5 * e.def.antenna.beamwidth_az_rad, 0.005),
                         WithAlpha(e.def.kind == EmitterKind::Jammer ? colors::kMagenta : colors::kAmber, 0x60));
        }
    }

    for (RadarState& r : s.radars) {
        const Platform* p = s.FindPlatform(r.platform_id);
        if (p == nullptr || !p->alive) continue;
        const RadarDef& d = s.lib.radars[static_cast<std::size_t>(r.def_index)];
        const std::uint32_t sc = SideColor(p->side);
        b.Text(kLayerSensorStatus, p->position + Vec3{0.0, 2500.0, 0.0},
               d.name + (r.on ? " ON" : " off") + (d.has_mti ? std::string(" MTI ") + MtiModeName(r.mti) : std::string()) +
                   (r.locked_track != 0 ? " LOCK" : ""),
               r.on ? colors::kWhite : colors::kGrey);
        if (!r.on) continue;
        b.Circle(kLayerCoverage, p->position, d.max_range_m, WithAlpha(sc, 0x60), true);
        const double r50 = FreeSpaceDetectionRange(d, reference_rcs_m2, 0.5);
        b.Circle(kLayerCoverage, p->position, r50, WithAlpha(sc, 0xA0));
        b.Text(kLayerCoverage, AlongAzimuth(p->position, 0.0, r50), "R50 " + Fmt("%.0f", reference_rcs_m2) + " m2", sc);
        // Jammed envelope: the same ring shrunk by J/N in each direction.
        if (b.On(kLayerJamming)) {
            bool any = false;
            for (const JammerState& j : s.jammers) any = any || (j.on && j.silent_until_s <= s.time_s);
            if (any) {
                std::vector<Vec3> poly;
                const EmitterState* e = s.FindEmitter(r.emitter_id);
                const double f = e != nullptr && e->freq_hz > 0.0 ? e->freq_hz : d.emitter.waveform.freq_hz;
                const ElevationBeam beam = BeamFor(d.emitter, 0.02);
                const AntennaDef ant = d.role == RadarRole::Search ? BeamAntenna(d.emitter, beam) : d.emitter.antenna;
                for (int k = 0; k < 72; ++k) {
                    const double az = kTwoPi * static_cast<double>(k) / 72.0;
                    const double jn = NoiseJammingToNoise(s, *p, ant, az, beam.centre_rad, f, d.RxBandwidthHz(), d.noise_figure_db);
                    poly.push_back(AlongAzimuth(p->position, az, MinOf(FreeSpaceDetectionRange(d, reference_rcs_m2, 0.5, jn),
                                                                        d.max_range_m)));
                }
                b.Polygon(kLayerJamming, poly, colors::kMagenta);
            }
        }
        for (const LookRecord& l : r.last_looks) {
            const Platform* t = s.FindPlatform(l.platform_id);
            if (t == nullptr) continue;
            b.Line(kLayerLineOfSight, p->position, t->position, l.line_of_sight ? WithAlpha(colors::kGood, 0x90)
                                                                                : WithAlpha(colors::kBad, 0x90),
                   1.0f, !l.line_of_sight);
            b.Text(kLayerDetection, t->position + Vec3{0.0, 1200.0, 0.0},
                   "Pd " + Fmt("%.2f", l.pd) + " SINR " + Fmt("%.1f", l.sinr_db) + " dB" +
                       (l.jn_db > -100.0 ? " J/N " + Fmt("%.0f", l.jn_db) : std::string()),
                   l.detected ? colors::kGood : colors::kGrey);
        }
        for (const SensorTrack& t : r.tracker.Tracks()) {
            const Vec3 pos = t.Position();
            const std::uint32_t c = t.status == TrackStatus::Confirmed ? sc : WithAlpha(sc, 0x70);
            b.Point(kLayerTracks, pos, c, 3.0f);
            b.Line(kLayerTracks, pos, pos + t.Velocity() * 30.0, c);
            const EllipseDesc el = EllipseOf(Horizontal(t.PositionCov()));
            b.Ellipse(kLayerTracks, pos, 2.0 * el.semi_major_m, 2.0 * el.semi_minor_m, el.major_azimuth_rad, c);
            b.Text(kLayerTracks, pos + Vec3{1500.0, 0.0, 0.0},
                   d.name + " T" + std::to_string(t.id) + " q" + Fmt("%.2f", TrackQuality(t, d.tracker)) +
                       (t.iff_replies > 0 ? " IFF" : ""),
                   c);
        }
        for (const StrobeTrack& st : r.strobe_tracks)
            b.Line(kLayerJamming, p->position, AlongAzimuth(p->position, st.azimuth_rad, d.max_range_m), colors::kMagenta, 2.0f);
    }

    for (const JammerState& j : s.jammers) {
        const Platform* p = s.FindPlatform(j.platform_id);
        if (p == nullptr || !p->alive || !j.on) continue;
        if (j.aim_at_point) b.Line(kLayerJamming, p->position, j.aim_point, WithAlpha(colors::kMagenta, 0x90), 1.0f, true);
    }

    for (const EsmState& e : s.esm) {
        const Platform* rx = s.FindPlatform(e.platform_id);
        if (rx == nullptr || !rx->alive || !e.on) continue;
        for (const EsmIntercept& in : e.intercepts) {
            const EmitterState* te = s.FindEmitter(in.debug_truth_emitter);
            const Platform* tx = te != nullptr ? s.FindPlatform(te->platform_id) : nullptr;
            if (tx != nullptr)
                b.Line(kLayerSignalPaths, tx->position, rx->position, in.main_beam ? colors::kAmber : WithAlpha(colors::kAmber, 0x50));
        }
        for (const EsmContact& c : e.contacts) {
            const double len = 80000.0;
            const std::uint32_t col = c.sig.hostile ? colors::kBad : c.sig.library_id.empty() ? colors::kWhite : colors::kGood;
            b.Line(kLayerBearings, rx->position, AlongAzimuth(rx->position, c.bearing_rad, len), col, 1.5f);
            b.Line(kLayerBearings, rx->position, AlongAzimuth(rx->position, c.bearing_rad - c.bearing_sigma_rad, len),
                   WithAlpha(col, 0x50), 1.0f, true);
            b.Line(kLayerBearings, rx->position, AlongAzimuth(rx->position, c.bearing_rad + c.bearing_sigma_rad, len),
                   WithAlpha(col, 0x50), 1.0f, true);
            b.Text(kLayerBearings, AlongAzimuth(rx->position, c.bearing_rad, 0.35 * len),
                   (c.sig.library_name.empty() ? std::string("unknown") : c.sig.library_name) +
                       (c.sig.steady ? " [LOCK]" : "") + " " + Fmt("%.0f", c.sig.library_confidence * 100.0) + "%",
                   col);
        }
    }

    for (const NodeState& n : s.nodes) {
        const Platform* p = s.FindPlatform(n.platform_id);
        if (p == nullptr) continue;
        b.Text(kLayerSensorStatus, p->position + Vec3{0.0, -4000.0, 0.0},
               n.name + (n.alive ? (n.parent != 0 ? std::string(" [") + C2StatusName(n.status) + "]" : std::string(" [C2]"))
                                 : std::string(" [destroyed]")),
               n.alive ? colors::kWhite : colors::kBad);
        if (!n.alive) continue;
        if (picture_node > 0 ? n.id != picture_node : (picture_node == 0 && !n.c2_center)) continue;
        b.SetOwner(n.id);
        for (const FusedTrack& f : n.kb.Picture(s.time_s)) {
            const KnowledgeTrack& k = f.fused;
            const std::uint32_t col = k.identity == Identity::Hostile ? colors::kBad
                                      : k.identity == Identity::Friend || k.identity == Identity::AssumedFriend ? colors::kBlue
                                                                                                               : colors::kAmber;
            if (f.bearing_only) {
                // The newest few lines: direction known, range unknown.
                const std::size_t first = k.bearings.size() > 4 ? k.bearings.size() - 4 : 0;
                for (std::size_t li = first; li < k.bearings.size(); ++li)
                    b.Line(kLayerBearings, k.bearings[li].origin,
                           AlongAzimuth(k.bearings[li].origin, k.bearings[li].azimuth_rad, 90000.0), WithAlpha(col, 0x60));
                continue;
            }
            const EllipseDesc el = EllipseOf(Horizontal(k.cov));
            const double s95 = EllipseScaleForProbability(0.95);
            const std::uint32_t layer = f.from_fix ? kLayerFixes : kLayerUncertainty;
            b.Ellipse(layer, k.position, s95 * el.semi_major_m, s95 * el.semi_minor_m, el.major_azimuth_rad, col);
            b.Point(layer, k.position, col, 3.0f);
            std::string label = n.name + " #" + std::to_string(f.id) + " " + IdentityName(k.identity);
            if (k.has_emitter && !k.emitter.library_name.empty()) label += " " + k.emitter.library_name;
            if (!k.type_guess.empty() && !f.from_fix) label += " (" + k.type_guess + ")";
            label += " CEP " + Fmt("%.0f", CepM(Horizontal(k.cov))) + " m";
            if (f.contributors > 1) label += " x" + std::to_string(f.contributors);
            b.Text(layer, k.position + Vec3{0.0, -1500.0, 0.0}, label, col);
            if (f.from_fix) {
                // The newest line from each receiver position that went into the fix.
                std::vector<BearingLine> shown;
                for (std::size_t li = f.fix_lines.size(); li-- > 0;) {
                    const BearingLine& l = f.fix_lines[li];
                    bool close_by = false;
                    for (const BearingLine& m : shown) close_by = close_by || GroundRange(m.origin, l.origin) < 3000.0;
                    if (close_by) continue;
                    shown.push_back(l);
                    b.Line(kLayerFixes, l.origin, AlongAzimuth(l.origin, l.azimuth_rad, GroundRange(l.origin, k.position) * 1.3),
                           WithAlpha(col, 0x70));
                    if (shown.size() >= 6) break;
                }
            }
        }
        b.SetOwner(0);
    }

    // Radio links (one line per node pair, coloured by the better direction's margin).
    for (const LinkState& l : s.links) {
        if (l.a > l.b) continue;
        const LinkState* back = FindLink(s, l.net, l.b, l.a);
        const double margin = back != nullptr && back->margin_db > l.margin_db ? back->margin_db : l.margin_db;
        const bool up = l.up || (back != nullptr && back->up);
        const NodeState* na = s.FindNode(l.a);
        const NodeState* nb = s.FindNode(l.b);
        const Platform* pa = na != nullptr ? s.FindPlatform(na->platform_id) : nullptr;
        const Platform* pb = nb != nullptr ? s.FindPlatform(nb->platform_id) : nullptr;
        if (pa == nullptr || pb == nullptr) continue;
        b.Line(kLayerC2Links, pa->position, pb->position, up ? colors::kGood : colors::kBad, up ? 2.0f : 1.0f, !up);
        b.Text(kLayerC2Links, (pa->position + pb->position) * 0.5, Fmt("%+.0f dB", margin), up ? colors::kGood : colors::kBad);
    }
    for (const Message& m : s.messages) {
        if (s.time_s < m.start_s || s.time_s >= m.end_s) continue;
        const NodeState* from = s.FindNode(m.from_node);
        const NetState* net = s.FindNet(m.net);
        if (from == nullptr || net == nullptr) continue;
        const Platform* pf = s.FindPlatform(from->platform_id);
        if (pf == nullptr) continue;
        for (int mem : net->members) {
            const NodeState* to = s.FindNode(mem);
            if (to == nullptr || to->id == from->id) continue;
            const Platform* pt = s.FindPlatform(to->platform_id);
            if (pt != nullptr) b.Line(kLayerCommsTraffic, pf->position, pt->position, colors::kCyan, 3.0f);
        }
    }
    return out;
}

// ------------------------------------------------------------------------------------------ JSON

inline void AppendJsonString(std::string& out, const std::string& s) {
    out += '"';
    for (const char ch : s) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(ch)));
                    out += buf;
                } else {
                    out += ch;
                }
        }
    }
    out += '"';
}

// One primitive as a compact JSON object (metres rounded to 1 m, angles to 1e-4 rad):
// {"s":shape,"l":layer,"a":[e,n],"b":[e,n],"r":[r1,r2],"g":[angle,angle2],"c":rgba,"w":width,
//  "d":dashed,"t":text,"p":[[e,n],...],"o":owner}  (keys omitted when unused)
inline void AppendPrimJson(std::string& out, const DebugPrim& p) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "{\"s\":%d,\"l\":%u,\"a\":[%.0f,%.0f]", static_cast<int>(p.shape), p.layer, p.a.x, p.a.y);
    out += buf;
    if (p.shape == DebugShape::Line) {
        std::snprintf(buf, sizeof(buf), ",\"b\":[%.0f,%.0f]", p.b.x, p.b.y);
        out += buf;
    }
    if (p.r1 != 0.0 || p.r2 != 0.0) {
        std::snprintf(buf, sizeof(buf), ",\"r\":[%.0f,%.0f]", p.r1, p.r2);
        out += buf;
    }
    if (p.angle != 0.0 || p.angle2 != 0.0) {
        std::snprintf(buf, sizeof(buf), ",\"g\":[%.4f,%.4f]", p.angle, p.angle2);
        out += buf;
    }
    std::snprintf(buf, sizeof(buf), ",\"c\":%u", p.rgba);
    out += buf;
    if (p.width != 1.0f) {
        std::snprintf(buf, sizeof(buf), ",\"w\":%.1f", static_cast<double>(p.width));
        out += buf;
    }
    if (p.dashed) out += ",\"d\":1";
    if (!p.text.empty()) {
        out += ",\"t\":";
        AppendJsonString(out, p.text);
    }
    if (!p.points.empty()) {
        out += ",\"p\":[";
        for (std::size_t i = 0; i < p.points.size(); ++i) {
            std::snprintf(buf, sizeof(buf), "%s[%.0f,%.0f]", i == 0 ? "" : ",", p.points[i].x, p.points[i].y);
            out += buf;
        }
        out += "]";
    }
    if (p.owner != 0) {
        std::snprintf(buf, sizeof(buf), ",\"o\":%d", p.owner);
        out += buf;
    }
    out += "}";
}

inline std::string PrimsToJson(const std::vector<DebugPrim>& prims) {
    std::string out = "[";
    for (std::size_t i = 0; i < prims.size(); ++i) {
        if (i != 0) out += ",";
        AppendPrimJson(out, prims[i]);
    }
    out += "]";
    return out;
}

}  // namespace em
}  // namespace simcore
