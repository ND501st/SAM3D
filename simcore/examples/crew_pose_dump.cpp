// crew_pose_dump: the crew's bodies (simcore/crew_body.hpp, roadmap 8.10) posed at rest and at work, as JSON for
// tools/blender/render_crew_poses.py, which draws them in the cabin so a person can check that each crewman sits in
// his seat and his hands meet the controls.
//
//   crew_pose_dump <data dir> <out.json> [osa_ak | pu12m]   (the vehicle; default the Osa)
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/cabin.hpp"
#include "simcore/console_data.hpp"
#include "simcore/crew_body.hpp"

using namespace simcore;

namespace {

struct Shot {
    const char* name;
    const char* station;
    const char* control;  // the control he reaches for ("" for rest)
    bool wheel;           // he turns it (a hand wheel)
    bool standing;        // he stands for it (CrewMember says so)
};

void Vec(std::ostream& f, const char* key, const Vec3& v) { f << "\"" << key << "\": [" << v.x << ", " << v.y << ", " << v.z << "]"; }

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: crew_pose_dump <data dir> <out.json> [osa_ak | pu12m]\n");
        return 2;
    }
    const std::string data = argv[1];
    const std::string system = argc > 3 ? argv[3] : "osa_ak";
    ConsoleData console;
    CabinLayout cabin;
    BodyData body;
    std::string e;
    if (!LoadConsoleFile(data + "/procedures/" + system + ".json", console, &e) ||
        !LoadCabinFile(data + "/cabins/" + system + ".json", console, cabin, &e) ||
        !LoadBodyDataFile(data + "/crew/body.json", body, &e)) {
        std::printf("%s\n", e.c_str());
        return 1;
    }
    const std::vector<Shot> osa = {
        {"driver_rest", "driver", "", false, false},
        {"driver_gta", "driver", "pwr.gta", false, false},
        {"commander_rest", "commander", "", false, false},
        {"commander_radio_mode", "commander", "radio.right.mode", false, false},
        {"commander_radio_preset", "commander", "radio.right.preset", false, true},
        {"commander_hatches", "commander", "veh.hatches", false, false},
        {"senior_operator_rest", "senior_operator", "", false, false},
        {"senior_operator_search", "senior_operator", "soc.search", false, false},
        {"senior_operator_az_wheel", "senior_operator", "ssc.az_wheel", true, false},
        {"operator_rest", "operator", "", false, false},
        {"operator_equipment", "operator", "pwr.equipment", false, false},
        {"operator_range_wheel", "operator", "ssc.range_wheel", true, false},
        {"deputy_rest", "deputy_commander", "", false, false},
        {"deputy_launch", "deputy_commander", "fo.launch", false, false},
        {"deputy_key", "deputy_commander", "fo.key", false, false},
        {"deputy_orient", "deputy_commander", "orient.qc", false, true},
    };
    // The PU-12M: each seat at rest and at a control of its own (the crew data's stand_for: standing).
    const std::vector<Shot> pu12m = {
        {"driver_rest", "driver", "", false, false},
        {"driver_mass", "driver", "pwr.mass", false, false},
        {"squad_leader_rest", "squad_leader", "", false, false},
        {"squad_leader_gak", "squad_leader", "gak.power", false, false},
        {"squad_leader_parallax", "squad_leader", "dtp.parallax", false, true},
        {"commander_rest", "battery_commander", "", false, false},
        {"commander_keypad", "battery_commander", "dch.key.5", false, false},
        {"commander_marker", "battery_commander", "dt2.azimuth", true, false},
        {"commander_rtk1", "battery_commander", "rtk1.power", false, false},
        {"commander_fr", "battery_commander", "fr.scale", false, false},
        {"operator_rest", "operator", "", false, false},
        {"radio_rest", "radio_operator", "", false, false},
        {"radio_fug3", "radio_operator", "radio.fug3.power", false, false},
        {"radio_mast", "radio_operator", "mast.raise", false, true},
    };
    const std::vector<Shot>& shots = system == "pu12m" ? pu12m : osa;
    std::ostringstream f;
    f << std::fixed << std::setprecision(4);
    f << "{\"poses\": [\n";
    bool first = true;
    for (const Shot& s : shots) {
        BodyAnimator anim(&cabin, &body, s.station);
        if (!anim.Valid()) continue;
        SeatActivity a;
        a.station = s.station;
        if (s.wheel) {
            a.wheels.push_back(s.control);
        } else if (s.control[0] != '\0') {
            a.control = s.control;
            a.standing = s.standing;
            a.stand_control = s.standing ? s.control : "";
        }
        const double dt = 1.0 / 60.0;
        for (double t = 0.0; t < 2.0; t += dt) {
            a.act_in_s = a.control.empty() ? -1.0 : (2.0 - t > 0.0 ? 2.0 - t : 0.0);
            anim.Update(dt, a, &console.panel);
        }
        if (!a.control.empty()) a.act_in_s = 0.0;
        anim.Update(dt, a, &console.panel);
        f << (first ? "" : ",\n") << "  {\"name\": \"" << s.name << "\", \"station\": \"" << s.station << "\", \"control\": \"" << s.control
          << "\", ";
        first = false;
        ControlTouch touch;
        if (s.control[0] != '\0' && TouchOf(cabin, s.control, touch)) {
            Vec(f, "touch", touch.point);
            f << ", ";
        }
        const ReachPlan& plan = anim.PlanFor(s.control);
        f << "\"seated\": " << (plan.seated ? "true" : "false") << ", \"standing\": " << anim.Pose().standing << ", ";
        Vec(f, "hip", anim.Pose().hip);
        f << ", ";
        Vec(f, "forward", anim.Seat().forward);
        f << ", \"parts\": [\n";
        const std::vector<BodyPart> parts = anim.Parts();
        for (std::size_t i = 0; i < parts.size(); ++i) {
            const BodyPart& p = parts[i];
            f << "    {\"name\": \"" << p.name << "\", \"hand\": \"" << ToString(p.hand) << "\", \"length\": " << p.length_m << ", ";
            Vec(f, "origin", p.origin);
            f << ", ";
            Vec(f, "bone", p.bone);
            f << ", ";
            Vec(f, "up", p.up);
            f << "}" << (i + 1 < parts.size() ? "," : "") << "\n";
        }
        f << "  ]}";
    }
    f << "\n]}\n";
    std::ofstream out(argv[2]);
    out << f.str();
    if (!out) {
        std::printf("cannot write %s\n", argv[2]);
        return 1;
    }
    std::printf("wrote %s\n", argv[2]);
    return 0;
}
