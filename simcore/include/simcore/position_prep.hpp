// simcore/position_prep.hpp — a prepared (peacetime) position (roadmap 7.6): the emplacement, camouflage nets, wire and
// dummy positions around the battery's vehicles. The grid and diesel power of such a position are in scenario.hpp
// (own_unit.position_power).
//
// Effects: the emplacement and the nets lower the vehicle's echo for the other side's radars (the EM layer's ground
// platform); each dummy position is an echo of its own (corner reflectors) and, if asked, a decoy emitter copying the
// search radar (roadmap E7); leaving the emplacement costs time before a vehicle can drive off. The wire is for the
// picture only. Numbers in data/command/prepared_positions.json (estimates where no source gives them).
#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/em/deception.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

struct PreparedPositionValues {
    double vehicle_rcs_m2 = 20.0;
    double emplacement_rcs_factor = 0.2;
    double nets_rcs_factor = 0.5;
    double dummy_rcs_m2 = 20.0;
    double leave_emplacement_s = 60.0;
};

struct DummyPositionDef {
    Vec3 position;
    bool decoy = false;  // a decoy emitter copying the search radar
    em::DecoyMode mode = em::DecoyMode::WhenRadarOff;
};

struct PreparedPositionDef {
    bool present = false;
    bool emplacement = false;
    bool nets = false;
    bool wire = false;
    std::vector<DummyPositionDef> dummies;
    PreparedPositionValues values;

    // The vehicle's echo for the other side's radars (m2).
    double VehicleRcsM2() const {
        double rcs = values.vehicle_rcs_m2;
        if (emplacement) rcs *= values.emplacement_rcs_factor;
        if (nets) rcs *= values.nets_rcs_factor;
        return rcs;
    }
};

namespace detail {

class PreparedPositionLoader : public ConsoleLoader {
public:
    bool Load(const json::Value& root, PreparedPositionValues& v) {
        if (!Keys(root, "", {"position_id", "display_name", "status", "notes", "values"})) return false;
        const json::Value* b = root.Find("values");
        if (b == nullptr || !b->Is(json::Type::Object)) return Fail("values", "required, an object");
        const std::pair<const char*, double*> fields[] = {{"vehicle_rcs_m2", &v.vehicle_rcs_m2},
                                                          {"emplacement_rcs_factor", &v.emplacement_rcs_factor},
                                                          {"nets_rcs_factor", &v.nets_rcs_factor},
                                                          {"dummy_rcs_m2", &v.dummy_rcs_m2},
                                                          {"leave_emplacement_s", &v.leave_emplacement_s}};
        for (const auto& f : fields) {
            const std::string p = Join("values", f.first);
            const json::Value* x = b->Find(f.first);
            if (x == nullptr) return Fail(p, "required");
            if (!x->Is(json::Type::Object) || !Keys(*x, p, {"value", "source", "confidence", "notes"}) || !Provenance(*x, p)) {
                return message_.empty() ? Fail(p, "expected {value, source, confidence}") : false;
            }
            const json::Value* n = x->Find("value");
            if (n == nullptr || !n->Is(json::Type::Number) || !(n->number > 0.0)) return Fail(Join(p, "value"), "a number > 0");
            *f.second = n->number;
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadPreparedPositionFile(const std::string& path, PreparedPositionValues& out, std::string* error = nullptr) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    json::Value root;
    std::string parse_error;
    if (!json::Parse(text.str(), root, &parse_error)) {
        if (error != nullptr) *error = path + ": " + parse_error;
        return false;
    }
    PreparedPositionValues v;
    detail::PreparedPositionLoader loader;
    if (!loader.Load(root, v)) {
        if (error != nullptr) *error = path + ": " + loader.Message();
        return false;
    }
    out = v;
    return true;
}

}  // namespace simcore
