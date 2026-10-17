// Electromagnetic layer: end-to-end validation scenarios through EmWorld (docs/EM_ARCHITECTURE.md,
// "Testing"). Generic placeholder library (data/em/generic.json) — no real system data.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "simcore/em.hpp"
#include "test_framework.hpp"

#ifndef SIMCORE_DATA_DIR
#define SIMCORE_DATA_DIR "data"
#endif

using namespace simcore;
using namespace simcore::em;

namespace {

EmLibrary GenericLib() {
    EmLibrary lib;
    const EmLoadResult r = LoadEmLibraryFile(std::string(SIMCORE_DATA_DIR) + "/em/generic.json", lib);
    if (!r.ok) std::printf("    %s\n", r.error.c_str());
    return lib;
}

const GeoPoint kOrigin = GeoPoint::FromDegrees(49.5, 17.0, 0.0);

// ENU point at an altitude above the ellipsoid near the origin (curvature drop included).
Vec3 EnuAt(double e, double n, double alt) { return {e, n, alt - (e * e + n * n) / (2.0 * wgs84::kMeanRadius)}; }

int AddAircraft(EmWorld& w, const char* name, Side side, double e, double n, double alt, const Vec3& vel,
                const RcsModel& rcs = RcsModel{3.0, 6.0, 3.0}) {
    Platform p;
    p.name = name;
    p.side = side;
    p.kind = PlatformKind::FixedWing;
    p.rcs = rcs;
    p.velocity = vel;
    p.position = EnuAt(e, n, alt);
    return w.AddPlatform(p);
}

// Move air platforms along their velocity, holding altitude over the curved Earth.
void Fly(EmWorld& w, double dt) {
    for (Platform& p : w.Scene().platforms) {
        if (!p.alive || p.kind == PlatformKind::Ground) continue;
        const double alt = w.Scene().frame.AltitudeOf(p.position);
        p.position += p.velocity * dt;
        p.position.z += alt + p.velocity.z * dt - w.Scene().frame.AltitudeOf(p.position);
    }
}

const SensorTrack* TrackOn(const RadarState& r, int platform) {
    for (const SensorTrack& t : r.tracker.Tracks())
        if (t.debug_truth_platform == platform && t.status == TrackStatus::Confirmed) return &t;
    return nullptr;
}

// A 400 m east-west ridge at north = 15 km, 2 km thick, on flat ground at 0 m.
GridTerrain Ridge() {
    const int cols = 121, rows = 61;  // 1 km grid from (-60 km, -10 km)
    std::vector<double> h(static_cast<std::size_t>(cols * rows), 0.0);
    for (int r = 0; r < rows; ++r) {
        const double north = -10000.0 + 1000.0 * r;
        if (std::fabs(north - 15000.0) <= 1000.0)
            for (int c = 0; c < cols; ++c) h[static_cast<std::size_t>(r * cols + c)] = 400.0;
    }
    return GridTerrain(-60000.0, -10000.0, 1000.0, cols, rows, h);
}

}  // namespace

SC_TEST("em scenario: detection probability rises as an aircraft approaches") {
    EmWorld w(kOrigin, nullptr, 21);
    SC_EXPECT(w.SetLibrary(GenericLib()));
    const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
    const int radar = w.AddRadar(site, "generic_search_c", true);
    const int jet = AddAircraft(w, "striker", Side::Blue, 0.0, 70000.0, 3000.0, {0.0, -250.0, 0.0});
    // Count programme cycles (two beams x 3 s): was the aircraft seen at least once in each?
    int looks[8] = {0}, hits[8] = {0};
    double confirmed_at_m = -1.0;
    bool seen_in_cycle = false;
    double cycle_range = 0.0;
    for (int i = 0; i < 2600; ++i) {
        Fly(w, 0.1);
        w.Step(0.1);
        const RadarState& r = *w.Scene().FindRadar(radar);
        for (const LookRecord& l : r.last_looks) {
            if (l.platform_id != jet) continue;
            seen_in_cycle = seen_in_cycle || l.detected;
            cycle_range = l.range_m;
        }
        if (i % 60 == 59) {
            const int bin = static_cast<int>(cycle_range / 10000.0);
            if (bin >= 0 && bin < 8 && cycle_range > 0.0) {
                ++looks[bin];
                hits[bin] += seen_in_cycle ? 1 : 0;
            }
            seen_in_cycle = false;
        }
        if (confirmed_at_m < 0.0 && TrackOn(r, jet) != nullptr)
            confirmed_at_m = (w.Scene().FindPlatform(jet)->position - w.Scene().FindPlatform(site)->position).Length();
    }
    auto frac = [&](int b) { return looks[b] > 0 ? static_cast<double>(hits[b]) / looks[b] : -1.0; };
    std::printf("    seen per 6 s cycle by 10 km bin: 10-20 %.2f, 20-30 %.2f, 30-40 %.2f, 40-50 %.2f, 50-60 %.2f, 60-70 %.2f; track at %.1f km\n",
                frac(1), frac(2), frac(3), frac(4), frac(5), frac(6), confirmed_at_m / 1000.0);
    SC_EXPECT(frac(1) > 0.95);
    SC_EXPECT(frac(2) > 0.7);  // 20-30 km at 3 km altitude straddles the two beams' crossover
    SC_EXPECT(frac(3) >= frac(5));
    SC_EXPECT(frac(4) > frac(6));
    SC_EXPECT(frac(6) < 0.4);
    SC_EXPECT(confirmed_at_m > 25000.0 && confirmed_at_m < 55000.0);
}

SC_TEST("em scenario: terrain masks a low flier until it pops up") {
    const GridTerrain ridge = Ridge();
    EmWorld w(kOrigin, &ridge, 4);
    SC_EXPECT(w.SetLibrary(GenericLib()));
    const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
    const int radar = w.AddRadar(site, "generic_search_c", true);
    // Behind the ridge at 100 m, crossing from west to east 25 km out.
    // Behind the ridge at 100 m, slanting in (a purely crossing target would also hide in the MTI notch).
    const int heli = AddAircraft(w, "low flier", Side::Blue, -20000.0, 30000.0, 100.0, {100.0, -60.0, 0.0});
    int masked_looks = 0, masked_hits = 0, popup_hits = 0;
    for (int i = 0; i < 1800; ++i) {
        if (i == 1000) w.Scene().FindPlatform(heli)->velocity = {100.0, -60.0, 30.0};  // pop-up climb
        if (i == 1400) w.Scene().FindPlatform(heli)->velocity = {100.0, -60.0, 0.0};   // ~1300 m: over the ridge line
        Fly(w, 0.1);
        w.Step(0.1);
        for (const LookRecord& l : w.Scene().FindRadar(radar)->last_looks) {
            if (l.platform_id != heli) continue;
            if (i < 1000) {
                ++masked_looks;
                masked_hits += l.detected ? 1 : 0;
                SC_EXPECT(!l.line_of_sight);
                SC_EXPECT(l.diffraction_db > 30.0);
            } else if (i > 1450) {
                popup_hits += l.detected ? 1 : 0;
            }
        }
    }
    SC_EXPECT(masked_looks > 20);
    SC_EXPECT(masked_hits == 0);
    SC_EXPECT(popup_hits > 3);
    SC_EXPECT(TrackOn(*w.Scene().FindRadar(radar), heli) != nullptr);
}

SC_TEST("em scenario: a radiating aircraft is detected passively, and vanishes when it goes silent") {
    EmWorld w(kOrigin, nullptr, 8);
    SC_EXPECT(w.SetLibrary(GenericLib()));
    const int site = w.AddGroundPlatform("ESM site", Side::Red, 0.0, 0.0, 10.0);
    const int esm = w.AddEsm(site, "generic_esm_site");
    // The SAM radar exists but is OFF: nothing illuminates the aircraft.
    w.AddRadar(w.AddGroundPlatform("SAM", Side::Red, 2000.0, 0.0, 5.0), "generic_search_c", false);
    const int jet = AddAircraft(w, "striker", Side::Blue, 0.0, 120000.0, 6000.0, {0.0, -200.0, 0.0});
    const int fcr = w.AddEmitter(jet, w.Scene().lib.emitters[static_cast<std::size_t>(w.Scene().lib.Emitter("generic_airborne_fcr_x"))]);
    const int rwr = w.AddEsm(jet, "generic_rwr");
    w.Scene().FindEmitter(fcr)->radiating = true;
    for (int i = 0; i < 200; ++i) {
        Fly(w, 0.1);
        w.Step(0.1);
    }
    const EsmState& e = *w.Scene().FindEsm(esm);
    SC_EXPECT(e.contacts.size() == 1);
    if (!e.contacts.empty()) {
        SC_EXPECT(e.contacts[0].sig.library_id == "blue_fighter_fcr");
        SC_EXPECT(e.contacts[0].sig.hostile);
        const double truth_az = AzimuthFromTo(w.Scene().FindPlatform(site)->position, w.Scene().FindPlatform(jet)->position);
        SC_EXPECT(std::fabs(WrapPi(e.contacts[0].bearing_rad - truth_az)) < DegToRad(5.0));
    }
    SC_EXPECT(w.Scene().FindEsm(rwr)->contacts.empty());  // the SAM is dark: nothing for the RWR
    // EMCON: the aircraft switches its radar off; the contact times out.
    w.Scene().FindEmitter(fcr)->radiating = false;
    for (int i = 0; i < 150; ++i) {
        Fly(w, 0.1);
        w.Step(0.1);
    }
    SC_EXPECT(w.Scene().FindEsm(esm)->contacts.empty());
}

SC_TEST("em scenario: more ESM stations, tighter triangulation") {
    double cep[5] = {0, 0, 0, 0, 0}, err[5] = {0, 0, 0, 0, 0};
    for (int n = 2; n <= 4; n += 2) {
        EmWorld w(kOrigin, nullptr, 13);
        SC_EXPECT(w.SetLibrary(GenericLib()));
        const int hq_p = w.AddGroundPlatform("ESM centre", Side::Red, 0.0, -5000.0, 10.0);
        const int hq = w.AddNode("ESM centre", hq_p, Side::Red, true);
        const double xs[4] = {-30000.0, 30000.0, -10000.0, 15000.0};
        for (int i = 0; i < n; ++i) {
            const int p = w.AddGroundPlatform("DF", Side::Red, xs[i], i < 2 ? 0.0 : 8000.0, 10.0);
            w.AttachSensor(hq, w.AddEsm(p, "generic_esm_site"));
        }
        // An airborne emitter holding station (a ground one 70 km away would be below the horizon).
        const int decoy = AddAircraft(w, "emitter", Side::Blue, 12000.0, 70000.0, 6000.0, {0.0, 0.0, 0.0});
        const int em = w.AddEmitter(decoy, w.Scene().lib.emitters[static_cast<std::size_t>(w.Scene().lib.Emitter("generic_decoy_emitter_x"))]);
        w.Scene().FindEmitter(em)->radiating = true;
        for (int i = 0; i < 600; ++i) w.Step(0.1);
        const std::vector<FusedTrack> pic = w.Scene().FindNode(hq)->kb.Picture(w.Time());
        const FusedTrack* fix = nullptr;
        for (const FusedTrack& f : pic)
            if (f.from_fix) fix = &f;
        SC_EXPECT(fix != nullptr);
        if (fix == nullptr) continue;
        cep[n] = CepM(Horizontal(fix->fused.cov));
        err[n] = GroundRange(fix->fused.position, w.Scene().FindPlatform(decoy)->position);
        SC_EXPECT(fix->fused.identity == Identity::Hostile);
    }
    std::printf("    2 stations: CEP %.0f m, error %.0f m; 4 stations: CEP %.0f m, error %.0f m\n", cep[2], err[2], cep[4], err[4]);
    SC_EXPECT(cep[4] < cep[2]);
    SC_EXPECT(err[2] < 4.0 * cep[2] + 200.0);
    SC_EXPECT(err[4] < 4.0 * cep[4] + 200.0);
}

SC_TEST("em scenario: noise jamming shrinks detection range and shows as a strobe") {
    double confirmed_km[2] = {0.0, 0.0};
    double strobe_err_deg = 999.0;
    for (int jam = 0; jam < 2; ++jam) {
        EmWorld w(kOrigin, nullptr, 31);
        SC_EXPECT(w.SetLibrary(GenericLib()));
        const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
        const int radar = w.AddRadar(site, "generic_search_c", true);
        const int node = w.AddNode("SAM", site, Side::Red);
        w.AttachSensor(node, radar);
        const int jet = AddAircraft(w, "striker", Side::Blue, 5000.0, 70000.0, 3000.0, {0.0, -250.0, 0.0});
        // Stand-off jammer orbiting 80 km east, its antenna on the SAM.
        const int soj = AddAircraft(w, "SOJ", Side::Blue, 80000.0, 20000.0, 8000.0, {0.0, 0.0, 0.0});
        const int j = w.AddJammer(soj, "generic_soj_barrage_c", jam == 1);
        JammerState* js = w.Scene().FindJammer(j);
        js->aim_at_point = true;
        js->aim_point = w.Scene().FindPlatform(site)->position;
        for (int i = 0; i < 2600 && confirmed_km[jam] == 0.0; ++i) {
            Fly(w, 0.1);
            w.Step(0.1);
            const RadarState& r = *w.Scene().FindRadar(radar);
            if (TrackOn(r, jet) != nullptr)
                confirmed_km[jam] = GroundRange(w.Scene().FindPlatform(jet)->position, w.Scene().FindPlatform(site)->position) / 1000.0;
            if (jam == 1 && !r.strobe_tracks.empty()) {
                const double truth = AzimuthFromTo(w.Scene().FindPlatform(site)->position, w.Scene().FindPlatform(soj)->position);
                strobe_err_deg = RadToDeg(std::fabs(WrapPi(r.strobe_tracks[0].azimuth_rad - truth)));
            }
        }
        if (jam == 1) {
            // The strobe is knowledge: a hostile bearing-only item at the SAM's node.
            bool strobe_known = false;
            for (const KnowledgeTrack& k : w.Scene().FindNode(node)->kb.Entries())
                strobe_known = strobe_known || (k.source == InfoSource::OwnStrobe && k.identity == Identity::Hostile);
            SC_EXPECT(strobe_known);
        }
    }
    std::printf("    track confirmed at %.1f km clear, %.1f km jammed; strobe bearing error %.1f deg\n", confirmed_km[0],
                confirmed_km[1], strobe_err_deg);
    SC_EXPECT(confirmed_km[0] > 25.0);
    SC_EXPECT(confirmed_km[1] > 0.0 && confirmed_km[1] < 0.8 * confirmed_km[0]);
    SC_EXPECT(strobe_err_deg < 2.0);
}

SC_TEST("em scenario: a self-screening jammer is burned through at short range") {
    double first_km[2] = {0.0, 0.0};
    double strobe_km = 0.0;
    for (int jam = 0; jam < 2; ++jam) {
        EmWorld w(kOrigin, nullptr, 5);
        SC_EXPECT(w.SetLibrary(GenericLib()));
        const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
        const int radar = w.AddRadar(site, "generic_search_c", true);
        const int jet = AddAircraft(w, "striker", Side::Blue, 0.0, 70000.0, 3000.0, {0.0, -250.0, 0.0});
        const int j = w.AddJammer(jet, "generic_selfscreen_spot_c", jam == 1);
        JammerState* js = w.Scene().FindJammer(j);
        js->aim_at_point = true;
        js->aim_point = w.Scene().FindPlatform(site)->position;
        for (int i = 0; i < 2700 && first_km[jam] == 0.0; ++i) {
            Fly(w, 0.1);
            w.Step(0.1);
            if (TrackOn(*w.Scene().FindRadar(radar), jet) != nullptr)
                first_km[jam] = GroundRange(w.Scene().FindPlatform(jet)->position, w.Scene().FindPlatform(site)->position) / 1000.0;
            if (jam == 1 && strobe_km == 0.0 && !w.Scene().FindRadar(radar)->strobe_tracks.empty())
                strobe_km = GroundRange(w.Scene().FindPlatform(jet)->position, w.Scene().FindPlatform(site)->position) / 1000.0;
        }
    }
    std::printf("    self-screening: track at %.1f km clear, burn-through track at %.1f km, strobe from %.1f km\n", first_km[0],
                first_km[1], strobe_km);
    SC_EXPECT(strobe_km > first_km[0]);  // the jammer announces itself before the radar would have seen the aircraft
    SC_EXPECT(first_km[1] > 1.0);                     // burned through before overflight
    SC_EXPECT(first_km[1] < 0.6 * first_km[0]);       // but much later than without the pod
}

SC_TEST("em scenario: a vanished target's track grows uncertain and is dropped") {
    EmWorld w(kOrigin, nullptr, 2);
    SC_EXPECT(w.SetLibrary(GenericLib()));
    const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
    const int radar = w.AddRadar(site, "generic_search_c", true);
    const int jet = AddAircraft(w, "striker", Side::Blue, 0.0, 30000.0, 3000.0, {0.0, -150.0, 0.0});
    int track_id = 0;
    for (int i = 0; i < 300; ++i) {
        Fly(w, 0.1);
        w.Step(0.1);
        if (const SensorTrack* t = TrackOn(*w.Scene().FindRadar(radar), jet)) track_id = t->id;
    }
    SC_EXPECT(track_id != 0);
    w.DestroyPlatform(jet);  // shot down (or landed): no more echoes
    double last_sigma = 0.0;
    bool grew = true, dropped = false;
    for (int i = 0; i < 300 && !dropped; ++i) {
        w.Step(0.1);
        const SensorTrack* t = w.Scene().FindRadar(radar)->tracker.Find(track_id);
        if (t == nullptr) {
            dropped = true;
            break;
        }
        if (i % 30 == 29) {  // once per scan
            grew = grew && t->PositionSigmaM() >= last_sigma;
            last_sigma = t->PositionSigmaM();
        }
    }
    SC_EXPECT(grew);
    SC_EXPECT(dropped);
    bool logged = false;
    for (const EmEvent& e : w.Scene().events) logged = logged || (e.kind == EmEventKind::TrackDropped && e.object == track_id);
    SC_EXPECT(logged);
}

SC_TEST("em scenario: a crossing target hides in the MTI notch") {
    int hits[2] = {0, 0};
    for (int mti = 0; mti < 2; ++mti) {
        EmWorld w(kOrigin, nullptr, 2);
        SC_EXPECT(w.SetLibrary(GenericLib()));
        const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
        const int radar = w.AddRadar(site, "generic_search_c", true);
        w.Scene().FindRadar(radar)->mti = mti == 1 ? MtiMode::Internal : MtiMode::Off;
        // Flying a circle around the radar: no radial speed at all ("beaming").
        const int jet = AddAircraft(w, "striker", Side::Blue, 0.0, 25000.0, 3000.0, {200.0, 0.0, 0.0});
        for (int i = 0; i < 600; ++i) {
            Platform* p = w.Scene().FindPlatform(jet);
            const Vec3 radial = Vec3{p->position.x, p->position.y, 0.0}.Normalized();
            p->velocity = Vec3{radial.y, -radial.x, 0.0} * 200.0;
            Fly(w, 0.1);
            w.Step(0.1);
            for (const LookRecord& l : w.Scene().FindRadar(radar)->last_looks) hits[mti] += l.detected ? 1 : 0;
        }
    }
    std::printf("    beaming target detections: MTI off %d, MTI on %d\n", hits[0], hits[1]);
    SC_EXPECT(hits[0] > 10);
    SC_EXPECT(hits[1] == 0);
}

SC_TEST("em scenario: destroying the command post cuts the picture; the SAM goes autonomous") {
    EmWorld w(kOrigin, nullptr, 17);
    SC_EXPECT(w.SetLibrary(GenericLib()));
    const int cp_p = w.AddGroundPlatform("battery CP", Side::Red, 0.0, -8000.0, 8.0);
    const int sam_p = w.AddGroundPlatform("SAM 2", Side::Red, 6000.0, 0.0, 5.0);
    const int cp = w.AddNode("battery CP", cp_p, Side::Red, true);
    w.Scene().FindNode(cp)->picture_period_s = 10.0;
    const int sam = w.AddNode("SAM 2", sam_p, Side::Red, false, cp);
    w.Scene().FindNode(sam)->kb.MutableParams().max_age_s = 40.0;
    const int cp_radar = w.AddRadar(cp_p, "generic_search_c", true);  // the battery's own surveillance radar
    w.AttachSensor(cp, cp_radar);
    const int sam_radar = w.AddRadar(sam_p, "generic_search_c", false);
    w.AttachSensor(sam, sam_radar);
    w.AddNet("battery net", "generic_vhf_fm", 40e6, {cp, sam});
    SamDoctrine d;
    d.node_id = sam;
    d.search_radar_id = sam_radar;
    d.cue_range_m = 20000.0;
    w.Doctrines().push_back(d);
    AddAircraft(w, "striker", Side::Blue, 30000.0, 30000.0, 4000.0, {-150.0, 0.0, 0.0});
    bool had_network_track = false;
    for (int i = 0; i < 1200; ++i) {
        Fly(w, 0.1);
        w.Step(0.1);
        for (const KnowledgeTrack& k : w.Scene().FindNode(sam)->kb.Entries())
            had_network_track = had_network_track || (k.source == InfoSource::Network && k.position_valid);
    }
    SC_EXPECT(had_network_track);
    SC_EXPECT(w.Scene().FindNode(sam)->status == C2Status::Connected);
    SC_EXPECT(!w.Scene().FindRadar(sam_radar)->on);  // the striker is outside the cue zone: stay dark
    w.DestroyPlatform(cp_p);
    for (int i = 0; i < 900; ++i) {
        Fly(w, 0.1);
        w.Step(0.1);
    }
    int network_items = 0;
    for (const KnowledgeTrack& k : w.Scene().FindNode(sam)->kb.Entries()) network_items += k.source == InfoSource::Network ? 1 : 0;
    SC_EXPECT(network_items == 0);
    SC_EXPECT(w.Scene().FindNode(sam)->status == C2Status::Autonomous);
    SC_EXPECT(w.Scene().FindRadar(sam_radar)->on);
    bool reason = false;
    for (const EmEvent& e : w.Scene().events)
        reason = reason || (e.kind == EmEventKind::DoctrineDecision && e.text.find("autonomously") != std::string::npos);
    SC_EXPECT(reason);
}

SC_TEST("em scenario: a network cue wakes a dark SAM") {
    EmWorld w(kOrigin, nullptr, 23);
    SC_EXPECT(w.SetLibrary(GenericLib()));
    const int cp_p = w.AddGroundPlatform("battery CP", Side::Red, 0.0, -8000.0, 8.0);
    const int sam_p = w.AddGroundPlatform("SAM 2", Side::Red, 0.0, 10000.0, 5.0);
    const int cp = w.AddNode("battery CP", cp_p, Side::Red, true);
    w.Scene().FindNode(cp)->picture_period_s = 8.0;
    const int sam = w.AddNode("SAM 2", sam_p, Side::Red, false, cp);
    const int cp_radar = w.AddRadar(cp_p, "generic_search_c", true);
    w.AttachSensor(cp, cp_radar);
    const int sam_radar = w.AddRadar(sam_p, "generic_search_c", false);
    w.AttachSensor(sam, sam_radar);
    w.AddNet("battery net", "generic_vhf_fm", 40e6, {cp, sam});
    SamDoctrine d;
    d.node_id = sam;
    d.search_radar_id = sam_radar;
    d.cue_range_m = 25000.0;
    w.Doctrines().push_back(d);
    AddAircraft(w, "striker", Side::Blue, 0.0, 60000.0, 3000.0, {0.0, -220.0, 0.0});
    double on_at = -1.0;
    for (int i = 0; i < 2000 && on_at < 0.0; ++i) {
        Fly(w, 0.1);
        w.Step(0.1);
        if (w.Scene().FindRadar(sam_radar)->on) on_at = w.Time();
    }
    SC_EXPECT(on_at > 0.0);
    bool cued = false;
    for (const EmEvent& e : w.Scene().events)
        cued = cued || (e.kind == EmEventKind::DoctrineDecision && e.text.find("(network)") != std::string::npos);
    SC_EXPECT(cued);
}

SC_TEST("em scenario: VHF radios talk over a ridge that blinds the radar between them") {
    const GridTerrain ridge = Ridge();
    EmWorld w(kOrigin, &ridge, 1);
    SC_EXPECT(w.SetLibrary(GenericLib()));
    const int a_p = w.AddGroundPlatform("A", Side::Red, 0.0, 9000.0, 4.0);
    const int b_p = w.AddGroundPlatform("B", Side::Red, 0.0, 21000.0, 4.0);
    const int a = w.AddNode("A", a_p, Side::Red, true);
    const int b = w.AddNode("B", b_p, Side::Red, false, a);
    w.AddNet("net", "generic_vhf_fm", 40e6, {a, b});
    w.Step(0.1);
    const LinkState* l = FindLink(w.Scene(), w.Scene().nets[0].id, a, b);
    SC_EXPECT(l != nullptr);
    if (l != nullptr) {
        SC_EXPECT(!l->line_of_sight);
        SC_EXPECT(l->up);
    }
    // The same path at C band: tens of dB of diffraction.
    const LinkLossResult c = LinkLoss(w.Scene(), *w.Scene().FindPlatform(a_p), *w.Scene().FindPlatform(b_p), 6e9);
    SC_EXPECT(c.diffraction_db > 35.0);
}

SC_TEST("em scenario: barrage jamming on the battery net cuts the link") {
    EmWorld w(kOrigin, nullptr, 1);
    SC_EXPECT(w.SetLibrary(GenericLib()));
    const int a_p = w.AddGroundPlatform("A", Side::Red, 0.0, 0.0, 4.0);
    const int b_p = w.AddGroundPlatform("B", Side::Red, 0.0, 12000.0, 4.0);
    const int a = w.AddNode("A", a_p, Side::Red, true);
    const int b = w.AddNode("B", b_p, Side::Red, false, a);
    w.AddNet("net", "generic_vhf_fm", 40e6, {a, b});
    const int jp = w.AddGroundPlatform("comms jammer", Side::Blue, 0.0, 30000.0, 20.0);
    const int j = w.AddJammer(jp, "generic_vhf_barrage", false);
    w.Step(0.1);
    const double clear = FindLink(w.Scene(), w.Scene().nets[0].id, a, b)->margin_db;
    w.Scene().FindJammer(j)->on = true;
    for (int i = 0; i < 12; ++i) w.Step(0.1);
    const LinkState* l = FindLink(w.Scene(), w.Scene().nets[0].id, a, b);
    std::printf("    battery net margin %.1f dB clear, %.1f dB jammed (J/N %.1f dB at B)\n", clear, l->margin_db, l->jn_db);
    SC_EXPECT(clear > 3.0);
    SC_EXPECT(l->margin_db < clear - 10.0);
    SC_EXPECT(l->jn_db > 10.0);
}

SC_TEST("em scenario: range-gate pull-off breaks a tracking radar's lock") {
    int lost_after[2] = {-1, -1};
    for (int jam = 0; jam < 2; ++jam) {
        EmWorld w(kOrigin, nullptr, 3);
        SC_EXPECT(w.SetLibrary(GenericLib()));
        const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 4.5);
        const int tr = w.AddRadar(site, "generic_track_ku", true);
        const int jet = AddAircraft(w, "striker", Side::Blue, 2000.0, 15000.0, 2000.0, {0.0, -150.0, 0.0});
        const int j = w.AddJammer(jet, "generic_rgpo_ku", false);
        RadarState* r = w.Scene().FindRadar(tr);
        r->has_designation = true;
        r->designation = w.Scene().FindPlatform(jet)->position;
        bool locked = false;
        for (int i = 0; i < 400; ++i) {
            if (i == 50) w.Scene().FindJammer(j)->on = jam == 1;
            Fly(w, 0.1);
            w.Step(0.1);
            r = w.Scene().FindRadar(tr);
            if (r->locked_track != 0) locked = true;
            if (locked && r->locked_track == 0 && lost_after[jam] < 0) lost_after[jam] = i;
        }
        SC_EXPECT(locked);
    }
    std::printf("    lock lost at tick %d with RGPO (never without: %d)\n", lost_after[1], lost_after[0]);
    SC_EXPECT(lost_after[0] < 0);
    SC_EXPECT(lost_after[1] > 50);
}

SC_TEST("em scenario: chaff fools a radar without MTI; external coherence cancels drifting chaff") {
    int false_plots[3] = {0, 0, 0};
    for (int mode = 0; mode < 3; ++mode) {
        EmWorld w(kOrigin, nullptr, 9);
        SC_EXPECT(w.SetLibrary(GenericLib()));
        Environment env;
        env.wind_mps = {0.0, -25.0, 0.0};  // blowing straight at the radar: 25 m/s radial
        w.SetEnvironment(env);
        const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
        const int radar = w.AddRadar(site, "generic_search_c", true);
        w.Scene().FindRadar(radar)->mti = mode == 0 ? MtiMode::Off : mode == 1 ? MtiMode::Internal : MtiMode::External;
        w.AddChaff(EnuAt(0.0, 25000.0, 2000.0), 50.0, 120.0);
        for (int i = 0; i < 300; ++i) {
            w.Step(0.1);
            for (const RadarPlot& p : w.Scene().FindRadar(radar)->last_plots) false_plots[mode] += p.false_target ? 1 : 0;
        }
    }
    std::printf("    chaff plots: MTI off %d, internal coherence %d, external coherence %d\n", false_plots[0], false_plots[1],
                false_plots[2]);
    SC_EXPECT(false_plots[0] > 5);
    SC_EXPECT(false_plots[1] > 5);   // 25 m/s drift is outside the MTI notch
    SC_EXPECT(false_plots[2] < false_plots[1] / 3 + 1);
}

SC_TEST("em scenario: a TDOA group fixes an airborne radar (Ramona/Tamara style)") {
    EmWorld w(kOrigin, nullptr, 6);
    SC_EXPECT(w.SetLibrary(GenericLib()));
    const int hq_p = w.AddGroundPlatform("TDOA centre", Side::Red, 0.0, 0.0, 25.0);
    const int hq = w.AddNode("TDOA centre", hq_p, Side::Red, true);
    std::vector<int> sites;
    const double pos[4][2] = {{0.0, 0.0}, {-25000.0, 5000.0}, {25000.0, 3000.0}, {0.0, 20000.0}};
    for (const auto& p : pos) sites.push_back(w.AddEsm(w.AddGroundPlatform("site", Side::Red, p[0], p[1], 25.0), "generic_tdoa_site"));
    w.AddTdoaGroup("TDOA system", sites, hq, true);
    const int jet = AddAircraft(w, "fighter", Side::Blue, 30000.0, 110000.0, 8000.0, {150.0, 0.0, 0.0});
    const EmitterDef fcr = w.Scene().lib.emitters[static_cast<std::size_t>(w.Scene().lib.Emitter("generic_airborne_fcr_x"))];
    w.Scene().FindEmitter(w.AddEmitter(jet, fcr))->radiating = true;
    for (int i = 0; i < 100; ++i) {
        Fly(w, 0.1);
        w.Step(0.1);
    }
    const KnowledgeTrack* fix = nullptr;
    for (const KnowledgeTrack& k : w.Scene().FindNode(hq)->kb.Entries())
        if (k.position_valid && k.category == Category::Emitter) fix = &k;
    SC_EXPECT(fix != nullptr);
    if (fix != nullptr) {
        const double err = GroundRange(fix->position, w.Scene().FindPlatform(jet)->position);
        const double cep = CepM(Horizontal(fix->cov));
        std::printf("    TDOA fix horizontal error %.0f m at %.0f km (CEP %.0f m), height error %.0f m\n", err,
                    GroundRange(w.Scene().FindPlatform(jet)->position, Vec3{}) / 1000.0, cep,
                    fix->position.z - w.Scene().FindPlatform(jet)->position.z);
        SC_EXPECT(err < 4.0 * cep + 200.0);
        SC_EXPECT(err < 3000.0);
    }
}

SC_TEST("em scenario: an aircraft ranges a SAM radar passively by flying across its bearing") {
    double cep[2] = {0.0, 0.0}, err[2] = {0.0, 0.0};
    const char* receivers[2] = {"generic_esm_pod", "generic_rwr"};
    for (int k = 0; k < 2; ++k) {
        EmWorld w(kOrigin, nullptr, 12);
        SC_EXPECT(w.SetLibrary(GenericLib()));
        const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
        w.AddRadar(site, "generic_search_c", true);
        const int jet = AddAircraft(w, "striker", Side::Blue, -40000.0, 70000.0, 7000.0, {250.0, 0.0, 0.0});
        const int node = w.AddNode("striker", jet, Side::Blue);
        w.AttachSensor(node, w.AddEsm(jet, receivers[k]));
        for (int i = 0; i < 2400; ++i) {
            Fly(w, 0.1);
            w.Step(0.1);
        }
        for (const FusedTrack& f : w.Scene().FindNode(node)->kb.Picture(w.Time())) {
            if (!f.from_fix) continue;
            cep[k] = CepM(Horizontal(f.fused.cov));
            err[k] = GroundRange(f.fused.position, w.Scene().FindPlatform(site)->position);
            SC_EXPECT(f.fused.emitter.library_id == "red_sam_search");
        }
    }
    std::printf("    passive fix after 240 s, 60 km flown at 70 km: ESM pod (2 deg) error %.1f km CEP %.1f km; "
                "RWR (10 deg) error %.1f km CEP %.1f km\n",
                err[0] / 1000.0, cep[0] / 1000.0, err[1] / 1000.0, cep[1] / 1000.0);
    SC_EXPECT(cep[0] > 0.0 && err[0] < 10000.0);
    SC_EXPECT(err[0] < 4.0 * cep[0] + 1000.0);   // the area it reports contains the SAM
    SC_EXPECT(cep[1] > 2.0 * cep[0]);              // an RWR's bearings make a much looser area
    if (cep[1] > 0.0) SC_EXPECT(err[1] < 4.0 * cep[1] + 2000.0);
}

SC_TEST("em scenario: identical seeds give identical histories") {
    auto run = [](std::uint64_t seed) {
        EmWorld w(kOrigin, nullptr, seed);
        w.SetLibrary(GenericLib());
        const int site = w.AddGroundPlatform("SAM", Side::Red, 0.0, 0.0, 5.0);
        w.AddRadar(site, "generic_search_c", true);
        w.AddEsm(w.AddGroundPlatform("ESM", Side::Red, 5000.0, 0.0, 10.0), "generic_esm_site");
        const int jet = AddAircraft(w, "striker", Side::Blue, 0.0, 60000.0, 3000.0, {0.0, -250.0, 0.0});
        const EmitterDef fcr = w.Scene().lib.emitters[static_cast<std::size_t>(w.Scene().lib.Emitter("generic_airborne_fcr_x"))];
        w.Scene().FindEmitter(w.AddEmitter(jet, fcr))->radiating = true;
        for (int i = 0; i < 1500; ++i) {
            Fly(w, 0.1);
            w.Step(0.1);
        }
        std::string log;
        for (const EmEvent& e : w.Scene().events) log += std::to_string(static_cast<long long>(e.time_s * 10.0 + 0.5)) + e.text + ";";
        return log;
    };
    const std::string a = run(99), b = run(99), c = run(100);
    SC_EXPECT(!a.empty());
    SC_EXPECT(a == b);
    SC_EXPECT(a != c);
}

namespace {

// A busy scene run with a given number of worker threads; returns everything observable.
struct RunOut {
    std::vector<std::string> events;
    std::vector<double> numbers;
};

RunOut RunBusyScene(unsigned threads) {
    EmWorld w(kOrigin, nullptr, 77);
    w.SetLibrary(GenericLib());
    w.Rates().threads = threads;
    for (int i = 0; i < 4; ++i) {
        w.AddRadar(w.AddGroundPlatform("SAM", Side::Red, -6000.0 + 4000.0 * i, 0.0, 5.0), "generic_search_c", true);
        w.AddEsm(w.AddGroundPlatform("ESM", Side::Red, -8000.0 + 5000.0 * i, -3000.0, 10.0), "generic_esm_site");
    }
    const EmitterDef fcr = w.Scene().lib.emitters[static_cast<std::size_t>(w.Scene().lib.Emitter("generic_airborne_fcr_x"))];
    for (int i = 0; i < 6; ++i) {
        const int jet = AddAircraft(w, "striker", Side::Blue, -20000.0 + 7000.0 * i, 90000.0 - 5000.0 * i, 3000.0 + 700.0 * i,
                                    {20.0 * (i - 3), -230.0, 0.0});
        w.Scene().FindEmitter(w.AddEmitter(jet, fcr))->radiating = i % 2 == 0;
        if (i == 3) w.Scene().FindJammer(w.AddJammer(jet, "generic_soj_barrage_c", true))->aim_at_point = false;
    }
    for (int i = 0; i < 600; ++i) {
        Fly(w, 0.1);
        w.Step(0.1);
    }
    RunOut out;
    for (const EmEvent& e : w.Scene().events) out.events.push_back(std::to_string(e.time_s) + " " + e.text);
    for (const EsmState& e : w.Scene().esm)
        for (const EsmContact& c : e.contacts) {
            out.numbers.push_back(c.bearing_rad);
            out.numbers.push_back(c.sig.freq_hz);
            out.numbers.push_back(static_cast<double>(c.intercepts));
        }
    for (const RadarState& r : w.Scene().radars)
        for (const SensorTrack& t : r.tracker.Tracks()) out.numbers.push_back(t.Position().x + t.Position().y);
    out.numbers.push_back(static_cast<double>(w.Scene().stats.intercepts));
    out.numbers.push_back(static_cast<double>(w.Scene().stats.radar_looks));
    out.numbers.push_back(static_cast<double>(w.Scene().stats.plots));
    return out;
}

}  // namespace

SC_TEST("em scale: the ESM sites and search radars give the same result on 1 and 4 threads") {
    const RunOut one = RunBusyScene(1);
    const RunOut four = RunBusyScene(4);
    std::printf("    %zu events, %zu numbers\n", one.events.size(), one.numbers.size());
    SC_EXPECT(one.events.size() > 10 && one.numbers.size() > 10);
    SC_EXPECT(one.events == four.events);
    SC_EXPECT(one.numbers == four.numbers);
}
