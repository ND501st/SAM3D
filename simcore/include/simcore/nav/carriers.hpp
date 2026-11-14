// simcore/nav/carriers.hpp — the information pipeline's carriers on the road net (roadmap G2.5;
// docs/campaign/11_BATTERY_LIFE_PROPOSAL.md §5.2, "Carriers and routines on routes").
//
// A messenger walks, a despatch rider rides his motorcycle, a courier run, the field post and a person going in person
// drive, and a means that names its class (the sergeant major sent in his truck) goes by that: each on the route service's routes between the two places at its class's speeds, by night at its night factor,
// round a bridge the planner has down. The info net (command/info_net.hpp) takes the route's time for the carrier's travel
// (`InfoNet::SetRouteTime`), so a delivery takes its route's time; a means that carries nothing (the wire, the radio) is
// left to the net.
#pragma once

#include <memory>
#include <string>

#include "simcore/command/info_net.hpp"
#include "simcore/nav/routes.hpp"

namespace simcore {
namespace nav {

// The mobility class that carries a means ("" : it carries nothing on the roads).
inline std::string CarrierClass(command::MeanKind k) {
    switch (k) {
        case command::MeanKind::Messenger: return "foot";
        case command::MeanKind::DespatchRider: return "motorcycle";
        case command::MeanKind::Courier:
        case command::MeanKind::InPerson:
        case command::MeanKind::FieldPost: return "truck";
        default: return std::string();
    }
}

// The route time for the info net: the carrier's class on the graph (shared, so the net may outlive the caller's copies),
// on the overlay the planner believes.
inline command::InfoNet::RouteTime CarrierRoutes(std::shared_ptr<const RoadGraph> graph, std::shared_ptr<const MobilityLibrary> lib,
                                                 std::shared_ptr<const Overlay> overlay = nullptr) {
    return [graph, lib, overlay](const command::SignalMean& m, const Vec3& a, const Vec3& b, bool night) {
        const std::string cls = m.mobility.empty() ? CarrierClass(m.kind) : m.mobility;
        if (cls.empty() || !graph || !lib) return -1.0;
        const Mobility* mob = lib->Find(cls);
        if (mob == nullptr) return -1.0;
        RouteRequest q;
        q.from = {a.x, a.y};
        q.to = {b.x, b.y};
        q.mobility = mob;
        q.night = night;
        q.swim = false;
        const Route r = overlay ? graph->Find(q, *overlay) : graph->Find(q);
        return r.found ? r.time_s : -1.0;
    };
}

}  // namespace nav
}  // namespace simcore
