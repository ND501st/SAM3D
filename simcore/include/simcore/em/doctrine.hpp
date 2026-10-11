// simcore/em/doctrine.hpp — decisions taken from knowledge, never from truth.
//
// Two small pieces of behaviour that prove the information model works for AI:
//   * EMCON schedules: scripted on/off times for radars, jammers and receivers (an aircraft
//     switching its radar off, a jammer coming on at the initial point).
//   * SamDoctrine: a SAM unit decides when to radiate from *its own picture* — tracks it holds
//     itself or heard from its command post. Connected to C2 and with nothing in its cue zone, it
//     stays dark and lets the network watch; a cue from the network (or losing the network) makes
//     it radiate; with nothing to shoot at for a while it goes dark again. It can designate its
//     tracking radar onto the nearest threat. Reasons are logged for the debrief.
// The Czechoslovak army's own advice of the period was the same: keep transmitters off, switch on
// for the shortest time, use dummy loads for checks (Vojenské rozhledy 1985/6; mon_2112_80 p67).
#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "simcore/em/knowledge.hpp"
#include "simcore/em/model.hpp"

namespace simcore {
namespace em {

enum class EmconTarget { Radar, Jammer, Esm };

struct EmconStep {
    double time_s = 0.0;
    EmconTarget target = EmconTarget::Radar;
    int id = 0;
    bool on = false;
    std::string note;
    bool done = false;
};

struct SamDoctrine {
    int node_id = 0;
    int search_radar_id = 0;
    int track_radar_id = 0;
    double cue_range_m = 30000.0;         // radiate when a non-friendly air track is predicted inside this
    double max_cue_sigma_m = 8000.0;      // ... and known at least this well
    bool radiate_when_autonomous = true;  // no picture from above: use the own radar
    double min_on_s = 15.0;               // don't flicker
    double quiet_off_s = 40.0;            // dark again after this long without a threat in the cue zone
    double engage_range_m = 0.0;          // > 0: designate the tracking radar on threats inside this
    bool keep_search_on = false;          // the unit is the battery's eyes: never goes dark by itself
    // State.
    double on_since_s = -1.0;
    double last_threat_s = -1e9;
};

inline void ApplyEmconSteps(EmScene& s, std::vector<EmconStep>& steps) {
    for (EmconStep& st : steps) {
        if (st.done || st.time_s > s.time_s) continue;
        st.done = true;
        switch (st.target) {
            case EmconTarget::Radar:
                if (RadarState* r = s.FindRadar(st.id)) {
                    r->on = st.on;
                    s.Log(st.on ? EmEventKind::RadarOn : EmEventKind::RadarOff, st.id, 0, st.note);
                }
                break;
            case EmconTarget::Jammer:
                if (JammerState* j = s.FindJammer(st.id)) {
                    j->on = st.on;
                    j->started_s = s.time_s;
                    s.Log(st.on ? EmEventKind::JammerOn : EmEventKind::JammerOff, st.id, 0, st.note);
                }
                break;
            case EmconTarget::Esm:
                if (EsmState* e = s.FindEsm(st.id)) {
                    e->on = st.on;
                    s.Log(EmEventKind::Info, st.id, 0, st.note);
                }
                break;
        }
    }
}

inline void RunSamDoctrine(EmScene& s, SamDoctrine& d) {
    NodeState* node = s.FindNode(d.node_id);
    RadarState* radar = s.FindRadar(d.search_radar_id);
    if (node == nullptr || radar == nullptr || !node->alive) return;
    const Platform* own = s.FindPlatform(node->platform_id);
    if (own == nullptr || !own->alive) return;
    const std::vector<FusedTrack> picture = node->kb.Picture(s.time_s);
    const FusedTrack* nearest = nullptr;
    double nearest_d = 1e18;
    for (const FusedTrack& f : picture) {
        const KnowledgeTrack& k = f.fused;
        if (!k.position_valid || f.from_fix) continue;
        if (k.category != Category::Air && k.category != Category::Unknown) continue;
        if (k.identity == Identity::Friend || k.identity == Identity::AssumedFriend) continue;
        if (k.HorizontalSigmaM() > d.max_cue_sigma_m) continue;
        const double dist = GroundRange(own->position, k.position);
        if (dist < nearest_d) {
            nearest_d = dist;
            nearest = &f;
        }
    }
    const bool threat = nearest != nullptr && nearest_d <= d.cue_range_m;
    if (threat) d.last_threat_s = s.time_s;
    const bool autonomous = node->status == C2Status::Autonomous && node->parent != 0;
    if (d.keep_search_on && !radar->on) {
        radar->on = true;
        d.on_since_s = s.time_s;
        s.Log(EmEventKind::DoctrineDecision, node->id, radar->id, node->name + " search radar ON - surveillance duty");
    } else if (!radar->on && (threat || (autonomous && d.radiate_when_autonomous))) {
        radar->on = true;
        d.on_since_s = s.time_s;
        const std::string why =
            threat ? "cue: track " + std::to_string(nearest->id) + " at " +
                         std::to_string(static_cast<int>(nearest_d / 1000.0 + 0.5)) + " km (" +
                         InfoSourceName(nearest->fused.source) + ")"
                   : std::string("no picture from command: radiating autonomously");
        s.Log(EmEventKind::DoctrineDecision, node->id, radar->id, node->name + " search radar ON - " + why);
    } else if (radar->on && !d.keep_search_on && !threat && !(autonomous && d.radiate_when_autonomous) &&
               s.time_s - d.on_since_s >= d.min_on_s && s.time_s - d.last_threat_s >= d.quiet_off_s) {
        radar->on = false;
        s.Log(EmEventKind::DoctrineDecision, node->id, radar->id,
              node->name + " search radar OFF - nothing in the cue zone, back to EMCON");
    }
    if (d.track_radar_id != 0 && d.engage_range_m > 0.0) {
        RadarState* tr = s.FindRadar(d.track_radar_id);
        if (tr != nullptr && tr->locked_track == 0 && threat && nearest_d <= d.engage_range_m) {
            if (!tr->on) {
                tr->on = true;
                s.Log(EmEventKind::DoctrineDecision, node->id, tr->id, node->name + " tracking radar ON");
            }
            tr->has_designation = true;
            tr->designation = nearest->fused.position;
        }
    }
}

}  // namespace em
}  // namespace simcore
