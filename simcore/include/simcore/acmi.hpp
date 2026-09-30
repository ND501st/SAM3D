// simcore/acmi.hpp — Tacview ACMI 2.2 text export (optional debrief/debug output).
//
// Format reference: https://www.tacview.net/documentation/acmi/en/
// Opening the written file in Tacview gives an instant 3D replay of an engagement.
// Real-time streaming to Tacview is NOT implemented (it needs Tacview Advanced).
#pragma once

#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_set>

#include "simcore/geo.hpp"
#include "simcore/world.hpp"

namespace simcore {

class AcmiWriter {
public:
    explicit AcmiWriter(const LocalFrame& frame, std::string reference_time_utc = "2026-01-01T00:00:00Z",
                        std::string title = "SAM3D engagement")
        : frame_(frame) {
        text_ += "FileType=text/acmi/tacview\n";
        text_ += "FileVersion=2.2\n";
        text_ += "0,ReferenceTime=" + reference_time_utc + "\n";
        text_ += "0,Title=" + title + "\n";
        text_ += "0,DataSource=SAM3D simcore\n";
    }

    void BeginFrame(double time_s) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "#%.2f\n", time_s);
        text_ += buf;
    }

    // Write an object's state. `properties` (e.g. "Type=Air+FixedWing,Name=F-4") is only
    // emitted the first time an id is seen.
    void WriteObject(int id, const Vec3& enu, const Vec3& velocity_enu, const std::string& properties) {
        const GeoPoint g = frame_.EnuToGeodetic(enu);
        const double heading = RadToDeg(AzimuthOf(velocity_enu));
        const double pitch = velocity_enu.LengthSquared() > 0.0 ? RadToDeg(ElevationOf(velocity_enu)) : 0.0;
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%x,T=%.7f|%.7f|%.1f|0|%.1f|%.1f", static_cast<unsigned>(id), g.LonDeg(),
                      g.LatDeg(), g.alt_m, pitch, heading);
        text_ += buf;
        if (seen_.insert(id).second && !properties.empty()) text_ += "," + properties;
        text_ += "\n";
    }

    void RemoveObject(int id) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "-%x\n", static_cast<unsigned>(id));
        text_ += buf;
        seen_.erase(id);
    }

    // Convenience: write every live aircraft and in-flight missile; remove finished ones once.
    void WriteWorld(const World& world) {
        BeginFrame(world.Time());
        for (const Aircraft& a : world.AircraftList()) {
            if (a.alive) {
                WriteObject(a.id, a.position, a.velocity, "Type=" + a.type + ",Name=" + a.name + ",Color=Blue");
            } else if (removed_.insert(a.id).second) {
                RemoveObject(a.id);
            }
        }
        for (const Missile& m : world.MissileList()) {
            if (m.Active()) {
                WriteObject(m.id, m.position, m.velocity, "Type=Weapon+Missile,Name=SAM,Color=Red");
            } else if (removed_.insert(m.id).second) {
                RemoveObject(m.id);
            }
        }
    }

    const std::string& Text() const { return text_; }

    bool SaveTo(const std::string& path) const {
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        f.write(text_.data(), static_cast<std::streamsize>(text_.size()));
        return static_cast<bool>(f);
    }

private:
    LocalFrame frame_;
    std::string text_;
    std::unordered_set<int> seen_;
    std::unordered_set<int> removed_;
};

}  // namespace simcore
