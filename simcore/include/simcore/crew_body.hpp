// simcore/crew_body.hpp — the crew's bodies (roadmap 8.10, docs/crew/10_BODIES_AND_VOICES.md).
//
// Where each crewmate sits, how he reaches a control and where he looks, in the cabin layout's body frame (metres;
// x right, y forward, z up). Presentation, but for one fact the drill needs: the controls a seat can't reach sitting
// down, which its crewmate stands up and steps to (data/crew/<system>_crew.json "stand_for", checked against this
// model by the tests; CrewMember adds the time). Nothing here moves a control.
//
//   BodyData         data/crew/body.json: segment lengths from the stature (Winter's fractions), the seat height and
//                    the head's turn (GOST 12.2.032-78), each seat's rest posture and gaze
//   SeatFrame        a station's eye point and heading -> the hip joints, the floor
//   TwoBoneIk        the elbow (knee) of a two-segment chain
//   TouchOf          where a hand meets a control: the point, the panel's normal, a finger or a grip
//   PlanReach        the hand, bend and twist (or standing) that reach a control from a seat
//   SolveBody        the whole pose from the hands' targets, the bend and the gaze
//   PartsOf          the pose as rigid parts (origin, bone, up, length) for a renderer (Unreal, Blender)
//   BodyAnimator     a seat's body over time from what its crewmate is doing (SeatActivity)
#pragma once

#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/cabin.hpp"
#include "simcore/console_data.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/json.hpp"
#include "simcore/vec3.hpp"

namespace simcore {

// The body of a crewman (data/crew/body.json). Lengths are fractions of the stature; the defaults are the file's.
struct BodyDims {
    double stature_m = 1.72;
    // Standing heights of the joints and segment lengths as fractions of the stature (winter_biomech).
    double eye_frac = 0.936;
    double shoulder_frac = 0.818;
    double hip_frac = 0.530;
    double knee_frac = 0.285;
    double ankle_frac = 0.039;
    double upper_arm_frac = 0.186;
    double forearm_frac = 0.146;
    double hand_frac = 0.108;
    double foot_frac = 0.152;
    double shoulder_breadth_frac = 0.259;
    double hip_breadth_frac = 0.191;
    double seat_height_m = 0.43;      // gost_12_2_032_78 Table 2 (men)
    double hip_above_seat_m = 0.09;   // the hip joints above the seat's surface
    double eye_ahead_m = 0.07;        // the eye ahead of the hip joints, sitting upright
    double max_bend_rad = DegToRad(40.0);   // forward and sideways at the hips, sitting
    double max_twist_rad = DegToRad(45.0);  // the shoulders turned about the spine
    double max_turn_rad = DegToRad(45.0);   // turned on the seat (the pelvis), sitting
    double max_stand_bend_rad = DegToRad(70.0);  // forward at the hips, standing
    double crouch_frac = 0.55;              // standing: the hips as low as this fraction of their standing height
    double head_yaw_max_rad = DegToRad(60.0);
    double head_pitch_max_rad = DegToRad(45.0);
    double hand_speed_mps = 1.2;      // a hand on its way to a control
    double hold_s = 0.3;              // a hand stays on a control after moving it
    double standing_off_m = 0.35;     // standing at a panel: the hip joints this far from its face

    double UpperArm() const { return upper_arm_frac * stature_m; }
    double Forearm() const { return forearm_frac * stature_m; }
    double Hand() const { return hand_frac * stature_m; }    // wrist to the fingertips
    double GripOffset() const { return 0.45 * Hand(); }     // wrist to the middle of the palm
    double PointOffset() const { return 0.95 * Hand(); }    // wrist to the pad of the index finger
    double Thigh() const { return (hip_frac - knee_frac) * stature_m; }
    double Shank() const { return (knee_frac - ankle_frac) * stature_m; }
    double AnkleHeight() const { return ankle_frac * stature_m; }
    double Foot() const { return foot_frac * stature_m; }
    double Torso() const { return (shoulder_frac - hip_frac) * stature_m; }  // hip joints up to the shoulder joints
    double EyeAboveShoulder() const { return (eye_frac - shoulder_frac) * stature_m; }
    double SittingEye() const { return (eye_frac - hip_frac) * stature_m + hip_above_seat_m; }  // above the seat
    double ShoulderHalf() const { return 0.5 * shoulder_breadth_frac * stature_m - 0.04; }  // joint centres
    double HipHalf() const { return 0.5 * hip_breadth_frac * stature_m - 0.07; }
    double ArmLength() const { return UpperArm() + Forearm(); }  // shoulder to wrist
};

// A seat's rest posture and gaze (data/crew/body.json "seats").
struct BodySeatDef {
    std::string station;
    std::string look;      // "forward", or a display id of the cabin: where he looks when nothing else calls; "a|b": the
                           // first of these the cabin has (a station of the same name in two vehicles)
    std::string hands_on;  // a shell part his hands rest on (the driver's "steering"); "" = his thighs
    std::string feet_on;   // a shell part his feet rest on (the driver's "pedals"); "" = the floor
};

struct BodyData {
    BodyDims dims;
    std::vector<BodySeatDef> seats;

    const BodySeatDef* Seat(const std::string& station) const {
        for (const BodySeatDef& s : seats) {
            if (s.station == station) return &s;
        }
        return nullptr;
    }
};

namespace detail {

class BodyLoader : public ConsoleLoader {
public:
    bool LoadBody(const json::Value& root, BodyData& out) {
        if (!Keys(root, "", {"body_id", "status", "notes", "source", "confidence", "body", "fractions", "seat", "limits",
                             "motion", "seats"}) ||
            !Provenance(root, "")) {
            return false;
        }
        BodyDims& d = out.dims;
        struct Field {
            const char* key;
            double* value;
            double low;
            double high;
            bool degrees;
        };
        struct Block {
            const char* key;
            std::vector<Field> fields;
        };
        double bend_deg = RadToDeg(d.max_bend_rad), twist_deg = RadToDeg(d.max_twist_rad);
        double head_yaw_deg = RadToDeg(d.head_yaw_max_rad), head_pitch_deg = RadToDeg(d.head_pitch_max_rad);
        double turn_deg = RadToDeg(d.max_turn_rad), stand_bend_deg = RadToDeg(d.max_stand_bend_rad);
        const Block blocks[] = {
            {"body", {{"stature_m", &d.stature_m, 1.4, 2.1, false}}},
            {"fractions",
             {{"eye", &d.eye_frac, 0.5, 1.0, false},
              {"shoulder", &d.shoulder_frac, 0.5, 1.0, false},
              {"hip", &d.hip_frac, 0.3, 0.7, false},
              {"knee", &d.knee_frac, 0.1, 0.5, false},
              {"ankle", &d.ankle_frac, 0.0, 0.1, false},
              {"upper_arm", &d.upper_arm_frac, 0.1, 0.3, false},
              {"forearm", &d.forearm_frac, 0.1, 0.3, false},
              {"hand", &d.hand_frac, 0.05, 0.2, false},
              {"foot", &d.foot_frac, 0.1, 0.25, false},
              {"shoulder_breadth", &d.shoulder_breadth_frac, 0.1, 0.4, false},
              {"hip_breadth", &d.hip_breadth_frac, 0.1, 0.3, false}}},
            {"seat",
             {{"seat_height_m", &d.seat_height_m, 0.2, 0.7, false},
              {"hip_above_seat_m", &d.hip_above_seat_m, 0.0, 0.2, false},
              {"eye_ahead_m", &d.eye_ahead_m, -0.2, 0.3, false}}},
            {"limits",
             {{"bend_deg", &bend_deg, 0.0, 90.0, true},
              {"twist_deg", &twist_deg, 0.0, 90.0, true},
              {"turn_deg", &turn_deg, 0.0, 90.0, true},
              {"stand_bend_deg", &stand_bend_deg, 0.0, 110.0, true},
              {"crouch_frac", &d.crouch_frac, 0.2, 1.0, false},
              {"head_yaw_deg", &head_yaw_deg, 0.0, 120.0, true},
              {"head_pitch_deg", &head_pitch_deg, 0.0, 90.0, true}}},
            {"motion",
             {{"hand_speed_mps", &d.hand_speed_mps, 0.1, 10.0, false},
              {"hold_s", &d.hold_s, 0.0, 5.0, false},
              {"standing_off_m", &d.standing_off_m, 0.1, 1.0, false}}},
        };
        for (const Block& b : blocks) {
            const json::Value* v = root.Find(b.key);
            if (v == nullptr) return Fail(b.key, "required");
            std::vector<const char*> allowed = {"source", "confidence", "notes"};
            for (const Field& f : b.fields) allowed.push_back(f.key);
            if (!v->Is(json::Type::Object)) return Fail(b.key, "expected an object");
            for (const std::string& k : v->keys) {
                bool ok = false;
                for (const char* a : allowed) ok = ok || k == a;
                if (!ok) return Fail(Join(b.key, k), "unknown key");
            }
            if (!Provenance(*v, b.key)) return false;
            for (const Field& f : b.fields) {
                if (v->Find(f.key) == nullptr) return Fail(Join(b.key, f.key), "required");
                if (!OptionalNumber(*v, b.key, f.key, *f.value)) return false;
                if (*f.value < f.low || *f.value > f.high) {
                    return Fail(Join(b.key, f.key), "must be from " + std::to_string(f.low) + " to " + std::to_string(f.high));
                }
            }
        }
        d.max_bend_rad = DegToRad(bend_deg);
        d.max_twist_rad = DegToRad(twist_deg);
        d.max_turn_rad = DegToRad(turn_deg);
        d.max_stand_bend_rad = DegToRad(stand_bend_deg);
        d.head_yaw_max_rad = DegToRad(head_yaw_deg);
        d.head_pitch_max_rad = DegToRad(head_pitch_deg);
        if (d.knee_frac >= d.hip_frac || d.ankle_frac >= d.knee_frac || d.hip_frac >= d.shoulder_frac ||
            d.shoulder_frac >= d.eye_frac) {
            return Fail("fractions", "the joints must rise: ankle < knee < hip < shoulder < eye");
        }
        if (const json::Value* arr = OptionalArray(root, "", "seats")) {
            for (std::size_t i = 0; i < arr->items.size(); ++i) {
                const json::Value& s = arr->items[i];
                const std::string p = Index("seats", i);
                if (!Keys(s, p, {"station", "look", "hands_on", "feet_on", "notes"})) return false;
                BodySeatDef def;
                if (!RequireString(s, p, "station", def.station) || !OptionalString(s, p, "look", def.look) ||
                    !OptionalString(s, p, "hands_on", def.hands_on) || !OptionalString(s, p, "feet_on", def.feet_on)) {
                    return false;
                }
                if (def.look.empty()) def.look = "forward";
                for (const BodySeatDef& other : out.seats) {
                    if (other.station == def.station) return Fail(Join(p, "station"), "duplicate seat '" + def.station + "'");
                }
                out.seats.push_back(std::move(def));
            }
        } else if (!message_.empty()) {
            return false;
        }
        return true;
    }
};

}  // namespace detail

inline bool LoadBodyData(const std::string& json_text, BodyData& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(json_text, root, &parse_error)) {
        if (error != nullptr) *error = parse_error;
        return false;
    }
    BodyData b;
    detail::BodyLoader loader;
    if (!loader.LoadBody(root, b)) {
        if (error != nullptr) *error = loader.Message();
        return false;
    }
    out = std::move(b);
    return true;
}

inline bool LoadBodyDataFile(const std::string& path, BodyData& out, std::string* error = nullptr) {
    std::string text;
    if (!detail::ReadTextFile(path, text)) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    if (!LoadBodyData(text, out, error)) {
        if (error != nullptr) *error = path + ": " + *error;
        return false;
    }
    return true;
}

// A seat: where the crewmate's eye is and which way he faces.
struct SeatFrame {
    std::string station;
    Vec3 eye;
    Vec3 forward;
    Vec3 right;
    Vec3 up{0.0, 0.0, 1.0};
    double view_pitch_rad = 0.0;  // the station's view: how far down he looks at rest
    Vec3 hip;                     // between the hip joints, sitting upright
    double floor_z = 0.0;         // the floor his feet stand on
};

inline SeatFrame SeatFrameOf(const CabinStation& s, const BodyDims& d) {
    SeatFrame f;
    f.station = s.id;
    f.eye = s.eye_m;
    f.forward = Vec3{std::sin(s.yaw_rad), std::cos(s.yaw_rad), 0.0};
    f.right = Vec3{std::cos(s.yaw_rad), -std::sin(s.yaw_rad), 0.0};
    f.view_pitch_rad = s.pitch_rad;
    f.hip = f.eye - f.up * (d.SittingEye() - d.hip_above_seat_m) - f.forward * d.eye_ahead_m;
    f.floor_z = f.hip.z - d.hip_above_seat_m - d.seat_height_m;
    return f;
}

// The middle joint of a chain root -> end of segment lengths a and b, bent towards `pole` (a direction). The end lands
// on the target when it is in reach, else as near as the chain gets along the line to it.
struct IkResult {
    Vec3 mid;
    Vec3 end;
    bool reached = true;
};

inline Vec3 AnyPerpendicular(const Vec3& v) {
    const Vec3 a = std::fabs(v.z) < 0.9 ? Vec3{0.0, 0.0, 1.0} : Vec3{1.0, 0.0, 0.0};
    return Cross(v, a).Normalized();
}

inline IkResult TwoBoneIk(const Vec3& root, const Vec3& target, double a, double b, const Vec3& pole) {
    IkResult r;
    const Vec3 d = target - root;
    const double len = d.Length();
    const double longest = a + b - 1e-6;
    const double shortest = std::fabs(a - b) + 1e-6;
    r.reached = len <= longest + 1e-9 && len >= shortest - 1e-9;
    const double l = Clamp(len, shortest, longest);
    const Vec3 dir = len > 1e-9 ? d / len : AnyPerpendicular(pole);
    const double cos_root = Clamp((a * a + l * l - b * b) / (2.0 * a * l), -1.0, 1.0);
    const double sin_root = std::sqrt(1.0 - cos_root * cos_root);
    Vec3 bend = pole - dir * Dot(pole, dir);
    bend = bend.Length() > 1e-9 ? bend.Normalized() : AnyPerpendicular(dir);
    r.mid = root + dir * (a * cos_root) + bend * (a * sin_root);
    r.end = root + dir * l;
    return r;
}

// How a hand meets a control: a finger on toggles, buttons and covers; a grip on knobs, keys and hand wheels.
enum class HandShape { Rest, Point, Grip };

inline const char* ToString(HandShape s) {
    switch (s) {
        case HandShape::Rest: return "rest";
        case HandShape::Point: return "point";
        case HandShape::Grip: return "grip";
    }
    return "?";
}

struct ControlTouch {
    std::string control_id;
    std::string panel_id;
    Vec3 point;       // where the finger's pad or the palm goes
    Vec3 normal;      // the panel's face, outwards
    Vec3 face_right;  // along the face, the operator's right
    Vec3 face_up;
    HandShape shape = HandShape::Point;
    bool wheel = false;
    double rim_m = 0.0;  // hand wheels: the radius the hand holds
    double units_per_turn = 360.0;
};

inline bool TouchOf(const CabinLayout& cabin, const std::string& control_id, ControlTouch& out) {
    const CabinControl* c = cabin.FindControl(control_id);
    if (c == nullptr) return false;
    const CabinPanel* p = cabin.FindPanel(c->panel_id);
    if (p == nullptr) return false;
    out = ControlTouch();
    out.control_id = control_id;
    out.panel_id = p->id;
    out.normal = p->Normal();
    out.face_right = p->Right();
    out.face_up = p->Up();
    const Vec3 at = p->PointAt(c->u_m, c->v_m);
    switch (c->style) {
        case CabinStyle::Wheel:
            out.shape = HandShape::Grip;
            out.wheel = true;
            out.rim_m = c->look.width_m > 0.0 ? 0.5 * c->look.width_m : 0.05;
            out.units_per_turn = c->units_per_turn;
            out.point = at + out.normal * 0.03;
            break;
        case CabinStyle::Rotary:
        case CabinStyle::Key:
            out.shape = HandShape::Grip;
            out.point = at + out.normal * 0.025;
            break;
        case CabinStyle::Toggle:
        case CabinStyle::Button:
        case CabinStyle::Cover:
            out.shape = HandShape::Point;
            out.point = at + out.normal * 0.012;
            break;
    }
    return true;
}

// A hand wheel's grip on its rim, `angle_rad` round from the operator's right (anticlockwise on the face).
inline Vec3 RimPoint(const ControlTouch& t, double angle_rad) {
    return t.point + (t.face_right * std::cos(angle_rad) + t.face_up * std::sin(angle_rad)) * t.rim_m;
}

// The body's turn and bend: turned on the seat (the pelvis, + to the right), bent forward at the hips, sideways (+ to
// the right), the shoulders twisted about the spine (+ turning right), and (standing) the hips lowered by a crouch.
struct TorsoLean {
    double turn_rad = 0.0;
    double forward_rad = 0.0;
    double side_rad = 0.0;
    double twist_rad = 0.0;
    double crouch_m = 0.0;
};

// The pelvis's heading for a body facing `forward` turned by `lean`.
inline Vec3 PelvisForward(const Vec3& forward, const TorsoLean& lean) {
    return RotateAboutAxis(forward, Vec3{0.0, 0.0, 1.0}, -lean.turn_rad).Normalized();
}

// The upper body's frame for a lean, about the hip `hip` of a body facing `forward`.
struct TorsoFrame {
    Vec3 hip;
    Vec3 up;
    Vec3 forward;
    Vec3 right;
    Vec3 chest;  // between the shoulder joints
    Vec3 Shoulder(int side, const BodyDims& d) const { return chest + right * (side * d.ShoulderHalf()); }
};

inline TorsoFrame TorsoFrameOf(const Vec3& hip, const Vec3& forward, const TorsoLean& lean, const BodyDims& d) {
    const Vec3 z{0.0, 0.0, 1.0};
    TorsoFrame t;
    t.hip = hip - z * lean.crouch_m;
    // Turn and twist about the vertical (clockwise from above for + right), then bend forward about the twisted right
    // axis, then sideways about the bent forward axis.
    Vec3 f = RotateAboutAxis(forward, z, -(lean.turn_rad + lean.twist_rad));
    Vec3 r = Cross(f, z).Normalized();
    Vec3 u = z;
    u = RotateAboutAxis(u, r, -lean.forward_rad);
    f = RotateAboutAxis(f, r, -lean.forward_rad);
    u = RotateAboutAxis(u, f, lean.side_rad);
    r = RotateAboutAxis(r, f, lean.side_rad);
    t.up = u.Normalized();
    t.forward = f.Normalized();
    t.right = r.Normalized();
    t.chest = t.hip + t.up * d.Torso();
    return t;
}

// Where the wrist goes for a hand to meet `touch` from a shoulder, and how the hand lies (fingers, back of the hand).
struct WristTarget {
    Vec3 wrist;
    Vec3 dir;  // the fingers
    Vec3 up;   // the back of the hand
    HandShape shape = HandShape::Point;
};

inline WristTarget WristFor(const Vec3& point, const Vec3& normal, HandShape shape, const Vec3& shoulder, const BodyDims& d) {
    WristTarget w;
    w.shape = shape;
    const Vec3 from_shoulder = (point - shoulder).Normalized();
    w.dir = (normal * -0.6 + from_shoulder * 0.4).Normalized();
    if (w.dir.Length() < 0.5) w.dir = from_shoulder;
    const Vec3 z{0.0, 0.0, 1.0};
    Vec3 up = z - w.dir * Dot(z, w.dir);
    if (up.Length() < 0.2) up = normal - w.dir * Dot(normal, w.dir);
    w.up = up.Normalized();
    w.wrist = point - w.dir * (shape == HandShape::Grip ? d.GripOffset() : d.PointOffset());
    return w;
}

// How a seat reaches a control: which hand, how he bends, and whether he has to stand up and step to it.
struct ReachPlan {
    bool valid = false;
    int side = 1;              // -1 the left hand, +1 the right
    TorsoLean lean;
    bool seated = true;        // reached sitting down
    bool reachable = true;     // reached at all (seated, or standing at the panel)
    Vec3 stand_hip;            // standing: where his hip joints are, facing stand_forward
    Vec3 stand_forward;
    double shoulder_distance_m = 0.0;  // from the nearer shoulder sitting upright (the reach check's measure)
};

namespace detail {

// The cheapest lean from which `side`'s wrist meets the touch, searched on a grid: sitting, a turn on the seat, the
// bend and the twist; standing, the bend, the twist and a crouch (`standing_hip_m` > 0: the hips' standing height).
inline bool SearchLean(const Vec3& hip, const Vec3& forward, const ControlTouch& t, const BodyDims& d, int side,
                       double standing_hip_m, TorsoLean& best, double& best_cost) {
    const bool standing = standing_hip_m > 0.0;
    const double bend = standing ? d.max_stand_bend_rad : d.max_bend_rad;
    const int n = 8;
    const int turns = standing ? 0 : 2;
    const int crouches = standing ? 3 : 0;
    bool found = false;
    for (int c = 0; c <= crouches; ++c) {
        const double crouch = standing ? standing_hip_m * (1.0 - d.crouch_frac) * c / 3 : 0.0;
        for (int p = -turns; p <= turns; ++p) {
            const double turn = turns > 0 ? d.max_turn_rad * p / turns : 0.0;
            for (int i = 0; i <= n; ++i) {
                const double fwd = bend * i / n;
                for (int j = -n; j <= n; ++j) {
                    const double sd = d.max_bend_rad * j / n;
                    if (std::fabs(sd) + fwd > bend * 1.25) continue;  // a combined bend has its limit too
                    for (int k = -4; k <= 4; ++k) {
                        const double tw = d.max_twist_rad * k / 4;
                        TorsoLean l{turn, fwd, sd, tw, crouch};
                        const double cost =
                            fwd + std::fabs(sd) * 1.3 + std::fabs(tw) * 0.6 + std::fabs(turn) * 0.9 + crouch * 2.0;
                        if (found && cost >= best_cost) continue;
                        const TorsoFrame tf = TorsoFrameOf(hip, PelvisForward(forward, l), TorsoLean{0.0, fwd, sd, tw, crouch}, d);
                        const Vec3 sh = tf.Shoulder(side, d);
                        const WristTarget w = WristFor(t.point, t.normal, t.shape, sh, d);
                        if ((w.wrist - sh).Length() <= d.ArmLength() - 0.01) {
                            best = l;
                            best_cost = cost;
                            found = true;
                        }
                    }
                }
            }
        }
    }
    return found;
}

}  // namespace detail

inline ReachPlan PlanReach(const SeatFrame& seat, const BodyDims& d, const ControlTouch& t) {
    ReachPlan plan;
    plan.valid = true;
    const TorsoFrame upright = TorsoFrameOf(seat.hip, seat.forward, TorsoLean(), d);
    const double dl = (t.point - upright.Shoulder(-1, d)).Length();
    const double dr = (t.point - upright.Shoulder(1, d)).Length();
    plan.shoulder_distance_m = dl < dr ? dl : dr;
    // The hand on the control's side of him first; the other hand only if it bends less.
    const int preferred = Dot(t.point - seat.hip, seat.right) >= 0.0 ? 1 : -1;
    double best_cost = 1e9;
    bool found = false;
    for (int side : {preferred, -preferred}) {
        TorsoLean l;
        double cost = 1e9;
        if (detail::SearchLean(seat.hip, seat.forward, t, d, side, 0.0, l, cost)) {
            if (side != preferred) cost += 0.15;
            if (!found || cost < best_cost) {
                plan.side = side;
                plan.lean = l;
                best_cost = cost;
                found = true;
            }
        }
    }
    if (found) return plan;
    // Out of seated reach: he stands in front of the panel, facing it, and reaches from there.
    plan.seated = false;
    Vec3 n_h{t.normal.x, t.normal.y, 0.0};
    n_h = n_h.Length() > 1e-6 ? n_h.Normalized() : -seat.forward;
    plan.stand_forward = -n_h;
    const Vec3 at_panel = t.point + n_h * d.standing_off_m;
    plan.stand_hip = Vec3{at_panel.x, at_panel.y, seat.floor_z + d.hip_frac * d.stature_m};
    const Vec3 standing_right = Cross(plan.stand_forward, Vec3{0.0, 0.0, 1.0}).Normalized();
    const int stand_pref = Dot(t.point - plan.stand_hip, standing_right) >= 0.0 ? 1 : -1;
    found = false;
    for (int side : {stand_pref, -stand_pref}) {
        TorsoLean l;
        double cost = 1e9;
        if (detail::SearchLean(plan.stand_hip, plan.stand_forward, t, d, side, d.hip_frac * d.stature_m, l, cost) &&
            (!found || cost < best_cost)) {
            plan.side = side;
            plan.lean = l;
            best_cost = cost;
            found = true;
        }
    }
    plan.reachable = found;
    return plan;
}

// One pose of the body. Index 0 is the left side, 1 the right.
struct BodyPose {
    Vec3 hip;           // between the hip joints
    Vec3 pelvis_forward;
    TorsoFrame torso;
    Vec3 neck;          // the base of the neck
    Vec3 head;          // the head's pivot (top of the neck)
    Vec3 head_forward;
    Vec3 head_up;
    Vec3 eye;
    Vec3 shoulder[2];
    Vec3 elbow[2];
    Vec3 wrist[2];
    Vec3 hand_dir[2];
    Vec3 hand_up[2];
    HandShape hand_shape[2] = {HandShape::Rest, HandShape::Rest};
    bool hand_reached[2] = {true, true};
    Vec3 hip_joint[2];
    Vec3 knee[2];
    Vec3 ankle[2];
    Vec3 foot_dir[2];
    double standing = 0.0;  // 0 seated, 1 standing
};

inline int SideIndex(int side) { return side < 0 ? 0 : 1; }

// What the body is asked to do in one pose.
struct BodyTargets {
    TorsoLean lean;
    double standing = 0.0;  // 0 seated .. 1 standing at stand_hip facing stand_forward
    Vec3 stand_hip;
    Vec3 stand_forward;
    bool hand_set[2] = {false, false};
    WristTarget hand[2];
    bool look = false;
    Vec3 look_at;
    double nod_rad = 0.0;   // speaking, breathing: the head pitched by this much more
    bool feet_set = false;
    Vec3 feet[2];           // ankles
};

namespace detail {

inline Vec3 Lerp(const Vec3& a, const Vec3& b, double t) { return a + (b - a) * t; }

}  // namespace detail

inline BodyPose SolveBody(const SeatFrame& seat, const BodyDims& d, const BodyTargets& tg) {
    BodyPose p;
    const Vec3 z{0.0, 0.0, 1.0};
    const double s = Clamp(tg.standing, 0.0, 1.0);
    p.standing = s;
    p.hip = s > 0.0 ? detail::Lerp(seat.hip, tg.stand_hip, s) : seat.hip;
    const Vec3 seated_forward = PelvisForward(seat.forward, tg.lean);
    p.pelvis_forward = s > 0.0 ? detail::Lerp(seated_forward, tg.stand_forward, s).Normalized() : seated_forward;
    if (p.pelvis_forward.Length() < 0.5) p.pelvis_forward = seat.forward;
    const Vec3 pelvis_right = Cross(p.pelvis_forward, z).Normalized();
    TorsoLean upper = tg.lean;
    upper.turn_rad = 0.0;  // the pelvis carries the turn
    upper.crouch_m = 0.0;  // and the crouch
    p.hip = p.hip - z * tg.lean.crouch_m;
    p.torso = TorsoFrameOf(p.hip, p.pelvis_forward, upper, d);
    for (int i = 0; i < 2; ++i) {
        const int side = i == 0 ? -1 : 1;
        p.shoulder[i] = p.torso.Shoulder(side, d);
        p.hip_joint[i] = p.hip + pelvis_right * (side * d.HipHalf());
    }
    // Head: turned towards what he looks at, within the neck's limits, about the torso's frame.
    p.neck = p.torso.chest + p.torso.up * 0.06;
    p.head = p.neck + p.torso.up * 0.07;
    const Vec3 eye_rel_up = p.torso.up * (d.EyeAboveShoulder() - 0.13);
    const Vec3 rest_eye = p.head + eye_rel_up + p.torso.forward * d.eye_ahead_m;
    double yaw = 0.0;
    double pitch = seat.view_pitch_rad * (1.0 - s);
    if (tg.look) {
        const Vec3 to = tg.look_at - rest_eye;
        const double x = Dot(to, p.torso.right);
        const double y = Dot(to, p.torso.forward);
        const double h = Dot(to, p.torso.up);
        yaw = Clamp(std::atan2(x, y), -d.head_yaw_max_rad, d.head_yaw_max_rad);
        pitch = Clamp(std::atan2(h, std::sqrt(x * x + y * y)), -d.head_pitch_max_rad, d.head_pitch_max_rad);
    }
    pitch -= tg.nod_rad;
    const Vec3 hf_level = (p.torso.forward * std::cos(yaw) + p.torso.right * std::sin(yaw)).Normalized();
    p.head_forward = (hf_level * std::cos(pitch) + p.torso.up * std::sin(pitch)).Normalized();
    const Vec3 head_right = Cross(p.head_forward, p.torso.up).Normalized();
    p.head_up = Cross(head_right, p.head_forward).Normalized();
    p.eye = p.head + p.head_up * (d.EyeAboveShoulder() - 0.13) + p.head_forward * d.eye_ahead_m;
    // Arms: each hand at its target, or resting on the thigh.
    for (int i = 0; i < 2; ++i) {
        const int side = i == 0 ? -1 : 1;
        WristTarget w;
        if (tg.hand_set[i]) {
            w = tg.hand[i];
        } else {
            w.shape = HandShape::Rest;
            const Vec3 thigh_dir = s > 0.5 ? -z : p.pelvis_forward;
            w.wrist = p.hip_joint[i] + thigh_dir * (s > 0.5 ? 0.25 : 0.22) + z * (s > 0.5 ? 0.0 : 0.11) +
                      pelvis_right * (side * (s > 0.5 ? 0.08 : 0.02));
            w.dir = s > 0.5 ? -z : (p.pelvis_forward - z * 0.3).Normalized();
            w.up = s > 0.5 ? pelvis_right * side : z;
        }
        const Vec3 pole = (-p.torso.up + p.torso.right * (0.7 * side) - p.torso.forward * 0.4).Normalized();
        const IkResult ik = TwoBoneIk(p.shoulder[i], w.wrist, d.UpperArm(), d.Forearm(), pole);
        p.elbow[i] = ik.mid;
        p.wrist[i] = ik.end;
        p.hand_dir[i] = w.dir;
        p.hand_up[i] = w.up;
        p.hand_shape[i] = w.shape;
        p.hand_reached[i] = ik.reached;
    }
    // Legs: seated, the thighs on the seat and the shins down to the floor (or to the feet's targets); standing,
    // straight down.
    for (int i = 0; i < 2; ++i) {
        Vec3 ankle_target;
        if (tg.feet_set && s < 0.5) {
            ankle_target = tg.feet[i];
        } else {
            const Vec3 seated = p.hip_joint[i] + p.pelvis_forward * (d.Thigh() + 0.06) +
                                z * (seat.floor_z + d.AnkleHeight() - p.hip_joint[i].z);
            const Vec3 standing = Vec3{p.hip_joint[i].x, p.hip_joint[i].y, seat.floor_z + d.AnkleHeight()} +
                                  p.pelvis_forward * 0.02;
            ankle_target = detail::Lerp(seated, standing, s);
        }
        const Vec3 pole = (p.pelvis_forward + z * 0.3).Normalized();
        const IkResult ik = TwoBoneIk(p.hip_joint[i], ankle_target, d.Thigh(), d.Shank(), pole);
        p.knee[i] = ik.mid;
        p.ankle[i] = ik.end;
        p.foot_dir[i] = p.pelvis_forward;
    }
    return p;
}

// The pose as rigid parts: each has its origin at its proximal joint, `bone` along it (the mesh's +X), `up` across it
// (the mesh's +Z), and its length. Hands carry their shape (the mesh to use).
struct BodyPart {
    std::string name;  // pelvis, torso, neck, head, upper_arm_l, forearm_l, hand_l, thigh_l, shin_l, foot_l, ... _r
    Vec3 origin;
    Vec3 bone;
    Vec3 up;
    double length_m = 0.0;
    HandShape hand = HandShape::Rest;
};

namespace detail {

inline BodyPart MakePart(const std::string& name, const Vec3& from, const Vec3& to, const Vec3& up_hint) {
    BodyPart b;
    b.name = name;
    b.origin = from;
    const Vec3 d = to - from;
    b.length_m = d.Length();
    b.bone = b.length_m > 1e-9 ? d / b.length_m : Vec3{0.0, 0.0, 1.0};
    Vec3 u = up_hint - b.bone * Dot(up_hint, b.bone);
    b.up = u.Length() > 1e-6 ? u.Normalized() : AnyPerpendicular(b.bone);
    return b;
}

}  // namespace detail

inline std::vector<BodyPart> PartsOf(const BodyPose& p, const BodyDims& d) {
    std::vector<BodyPart> out;
    const Vec3 z{0.0, 0.0, 1.0};
    // The pelvis tips with half the bend of the back (the hips flex as he leans).
    const Vec3 pelvis_up = (z + p.torso.up).Normalized();
    out.push_back(detail::MakePart("pelvis", p.hip, p.hip + pelvis_up * 0.18, p.pelvis_forward));
    out.push_back(detail::MakePart("torso", p.hip, p.torso.chest, p.torso.forward));
    out.push_back(detail::MakePart("neck", p.neck, p.head, p.head_forward));
    BodyPart head;
    head.name = "head";
    head.origin = p.head;
    head.bone = p.head_forward;
    head.up = p.head_up;
    head.length_m = 0.2;
    out.push_back(head);
    const char* sfx[2] = {"_l", "_r"};
    for (int i = 0; i < 2; ++i) {
        const std::string x = sfx[i];
        out.push_back(detail::MakePart("upper_arm" + x, p.shoulder[i], p.elbow[i], p.torso.forward));
        out.push_back(detail::MakePart("forearm" + x, p.elbow[i], p.wrist[i], p.hand_up[i]));
        BodyPart hand = detail::MakePart("hand" + x, p.wrist[i], p.wrist[i] + p.hand_dir[i] * d.Hand(), p.hand_up[i]);
        hand.hand = p.hand_shape[i];
        out.push_back(hand);
        out.push_back(detail::MakePart("thigh" + x, p.hip_joint[i], p.knee[i], z));
        out.push_back(detail::MakePart("shin" + x, p.knee[i], p.ankle[i], p.foot_dir[i]));
        out.push_back(detail::MakePart("foot" + x, p.ankle[i], p.ankle[i] + p.foot_dir[i] * (0.75 * d.Foot()), z));
    }
    return out;
}

// What a crewmate is doing, for his body (ScenarioRunner::CrewActivity).
struct SeatActivity {
    std::string station;
    bool npc = true;              // an NPC works the seat (a player's body follows the player instead)
    std::string control;          // his next action's control ("" for none)
    double act_in_s = -1.0;       // seconds until he moves it (0: now); < 0: not due (waiting, or nothing to do)
    bool waiting = false;         // waiting for equipment: he watches his instruments
    std::string last_control;     // the control he moved last
    double since_last_s = 1e9;
    bool standing = false;        // he is up at a panel out of seated reach (CrewMember::Intent)
    std::string stand_control;    // the control he stood up for
    std::vector<std::string> wheels;  // hand wheels he is turning (tracking by hand)
    bool speaking = false;
    bool stressed = false;
};

// A seat's body over time: the hands go to the controls the crewmate is about to move, stay a moment, and come back;
// hand wheels are held while he tracks; he bends to reach, stands for a panel out of seated reach, and looks at
// what he works or at his instruments. Deterministic: the same activity gives the same motion.
class BodyAnimator {
public:
    BodyAnimator(const CabinLayout* cabin, const BodyData* body, std::string station) : cabin_(cabin), body_(body) {
        if (cabin_ != nullptr && body_ != nullptr) {
            if (const CabinStation* st = cabin_->FindStation(station)) {
                seat_ = SeatFrameOf(*st, body_->dims);
                valid_ = true;
            }
            seat_def_ = body_->Seat(station);
        }
        seat_.station = std::move(station);
        if (valid_) pose_ = SolveBody(seat_, body_->dims, BodyTargets());
        for (int i = 0; i < 2; ++i) {
            hands_[i].wrist = hands_[i].from.wrist = pose_.wrist[i];
            hands_[i].dir = hands_[i].from.dir = pose_.hand_dir[i];
            hands_[i].up = hands_[i].from.up = pose_.hand_up[i];
        }
    }

    bool Valid() const { return valid_; }
    const SeatFrame& Seat() const { return seat_; }
    const BodyPose& Pose() const { return pose_; }
    std::vector<BodyPart> Parts() const { return valid_ ? PartsOf(pose_, body_->dims) : std::vector<BodyPart>(); }

    // The reach plan for a control from this seat (cached).
    const ReachPlan& PlanFor(const std::string& control_id) {
        auto it = plans_.find(control_id);
        if (it != plans_.end()) return it->second;
        ReachPlan p;
        ControlTouch t;
        if (valid_ && TouchOf(*cabin_, control_id, t)) p = PlanReach(seat_, body_->dims, t);
        return plans_.emplace(control_id, p).first->second;
    }

    // Advance by dt. `panel` gives the hand wheels' angles (may be null).
    void Update(double dt, const SeatActivity& a, const ControlPanel* panel) {
        if (!valid_) return;
        time_s_ += dt;
        const BodyDims& d = body_->dims;
        // What each hand should hold now.
        std::string want[2];
        bool on_wheel[2] = {false, false};
        const double lead_s = 0.55;
        std::string reach;
        if (!a.control.empty() && a.act_in_s >= 0.0 && a.act_in_s <= lead_s) {
            reach = a.control;
        } else if (!a.last_control.empty() && a.since_last_s < d.hold_s) {
            reach = a.last_control;
        }
        int reach_side = 0;
        ReachPlan reach_plan;
        if (!reach.empty()) {
            reach_plan = PlanFor(reach);
            if (reach_plan.valid && reach_plan.reachable) {
                reach_side = reach_plan.side;
                want[SideIndex(reach_side)] = reach;
            }
        }
        for (const std::string& w : a.wheels) {
            const ReachPlan& wp = PlanFor(w);
            if (!wp.valid || !wp.reachable) continue;
            int i = SideIndex(wp.side);
            if (!want[i].empty()) i = 1 - i;
            if (!want[i].empty()) continue;
            want[i] = w;
            on_wheel[i] = true;
        }
        // Standing: he stands while CrewMember says so (it counts the time); the body follows over about a second.
        const ReachPlan* stand_plan = nullptr;
        if (a.standing && !a.stand_control.empty()) {
            const ReachPlan& sp = PlanFor(a.stand_control);
            if (sp.valid && !sp.seated) stand_plan = &sp;
        }
        const double stand_target = stand_plan != nullptr ? 1.0 : 0.0;
        const double stand_rate = dt / 1.0;
        standing_ += Clamp(stand_target - standing_, -stand_rate, stand_rate);
        if (stand_plan != nullptr) {
            stand_hip_ = stand_plan->stand_hip;
            stand_forward_ = stand_plan->stand_forward;
        }
        BodyTargets tg;
        tg.standing = Smooth01(standing_);
        tg.stand_hip = stand_hip_;
        tg.stand_forward = stand_forward_;
        // The bend: what the reaching hand's plan needs (seated plans sitting, standing plans once up).
        TorsoLean lean_target;
        if (reach_side != 0 && (reach_plan.seated ? standing_ < 0.5 : standing_ > 0.5)) lean_target = reach_plan.lean;
        const double k = 1.0 - std::exp(-dt / 0.2);
        lean_.forward_rad += (lean_target.forward_rad - lean_.forward_rad) * k;
        lean_.side_rad += (lean_target.side_rad - lean_.side_rad) * k;
        lean_.twist_rad += (lean_target.twist_rad - lean_.twist_rad) * k;
        lean_.turn_rad += (lean_target.turn_rad - lean_.turn_rad) * k;
        lean_.crouch_m += (lean_target.crouch_m - lean_.crouch_m) * k;
        // Breathing: a slow rise of the chest.
        tg.lean = lean_;
        tg.lean.forward_rad += 0.008 * std::sin(time_s_ * 2.0 * kPi * 0.25);
        // The hands: each to its target, along a short arc.
        const BodyPose rest = SolveBody(seat_, d, tg);  // where the hands rest in this posture
        for (int i = 0; i < 2; ++i) {
            Hand& h = hands_[i];
            WristTarget goal;
            bool held = false;
            if (!want[i].empty()) {
                ControlTouch t;
                if (TouchOf(*cabin_, want[i], t)) {
                    Vec3 point = t.point;
                    if (on_wheel[i] && t.wheel) point = WheelGrip(i, t, panel);
                    goal = WristFor(point, t.normal, t.shape, rest.shoulder[i], d);
                    held = true;
                    h.normal = t.normal;
                }
            } else if (seat_def_ != nullptr && !seat_def_->hands_on.empty() && standing_ < 0.5) {
                held = HandsOn(seat_def_->hands_on, i, rest, goal);
                h.normal = -seat_.forward;
            }
            if (!held) {
                goal.wrist = rest.wrist[i];
                goal.dir = rest.hand_dir[i];
                goal.up = rest.hand_up[i];
                goal.shape = HandShape::Rest;
            }
            const std::string key = held ? (want[i].empty() ? std::string("@rest_on") : want[i]) : std::string();
            if (key != h.target) {  // a new target: start a move from where the hand is
                h.from.wrist = h.wrist;
                h.from.dir = h.dir;
                h.from.up = h.up;
                h.target = key;
                h.t = 0.0;
                const double dist = (goal.wrist - h.wrist).Length();
                h.duration = Clamp(dist / d.hand_speed_mps, 0.2, 0.8);
            }
            h.t += dt;
            const double u = Smooth01(Clamp(h.duration > 0.0 ? h.t / h.duration : 1.0, 0.0, 1.0));
            const double lift = held ? 0.05 * std::sin(kPi * u) : 0.0;
            h.wrist = detail::Lerp(h.from.wrist, goal.wrist, u) + h.normal * lift;
            h.dir = detail::Lerp(h.from.dir, goal.dir, u).Normalized();
            h.up = detail::Lerp(h.from.up, goal.up, u).Normalized();
            h.shape = u > 0.6 ? goal.shape : h.from_shape;
            if (u >= 1.0) h.from_shape = goal.shape;
            tg.hand_set[i] = true;
            tg.hand[i].wrist = h.wrist;
            tg.hand[i].dir = h.dir;
            tg.hand[i].up = h.up;
            tg.hand[i].shape = h.shape;
        }
        // The gaze: what he reaches for, else his instruments (or the road).
        tg.look = true;
        tg.look_at = LookTarget(a, reach);
        if (a.speaking) tg.nod_rad = 0.04 * std::sin(time_s_ * 2.0 * kPi * 3.0);
        // The feet: on the pedals (the driver), else the floor.
        if (seat_def_ != nullptr && !seat_def_->feet_on.empty()) FeetOn(seat_def_->feet_on, tg);
        pose_ = SolveBody(seat_, d, tg);
    }

private:
    struct HandFrom {
        Vec3 wrist;
        Vec3 dir;
        Vec3 up;
    };
    struct Hand {
        Vec3 wrist;
        Vec3 dir;
        Vec3 up;
        HandShape shape = HandShape::Rest;
        HandShape from_shape = HandShape::Rest;
        std::string target;  // the control (or "@rest_on") it is going to; "" = its rest
        double t = 0.0;      // into the current move
        double duration = 0.0;
        Vec3 normal{0.0, 0.0, 1.0};  // the face it reaches for: the move arcs out along it
        HandFrom from;               // where the current move started
    };

    static double Smooth01(double x) { return x * x * (3.0 - 2.0 * x); }

    // A hand on a turning wheel's rim: it rides round with the wheel and takes a new grip every 120 degrees.
    Vec3 WheelGrip(int i, const ControlTouch& t, const ControlPanel* panel) {
        double value = 0.0;
        if (panel != nullptr) value = panel->GetValue(t.control_id);
        const double wheel = -value / (t.units_per_turn != 0.0 ? t.units_per_turn : 360.0) * kTwoPi;
        WheelHold& g = wheel_[i];
        if (!g.held || g.control != t.control_id) {
            g = WheelHold();
            g.held = true;
            g.control = t.control_id;
            g.grabbed_at = wheel;
        }
        double turned = wheel - g.grabbed_at;
        if (std::fabs(turned) > DegToRad(60.0)) {
            g.grabbed_at = wheel;
            turned = 0.0;
        }
        const double home = i == 0 ? kPi : 0.0;  // the left hand on the rim's left, the right on its right
        return RimPoint(t, home + turned);
    }

    bool HandsOn(const std::string& part_id, int i, const BodyPose& rest, WristTarget& goal) const {
        for (const CabinShellPart& p : cabin_->shell) {
            if (p.id != part_id) continue;
            // A steering wheel: the rim at ten to two, its face towards the driver.
            const double side = i == 0 ? -1.0 : 1.0;
            const double radius = 0.42 * (p.size_m.x < p.size_m.z ? p.size_m.x : p.size_m.z);
            const Vec3 face = (-seat_.forward + Vec3{0.0, 0.0, 1.0} * 0.6).Normalized();
            const Vec3 across = seat_.right;
            const Vec3 face_up = Cross(across, face).Normalized() * -1.0;
            const Vec3 grip = p.centre_m + across * (side * radius * 0.86) + face_up * (radius * 0.5) + face * 0.02;
            goal = WristFor(grip, face, HandShape::Grip, rest.shoulder[i], body_->dims);
            return true;
        }
        return false;
    }

    void FeetOn(const std::string& part_id, BodyTargets& tg) const {
        for (const CabinShellPart& p : cabin_->shell) {
            if (p.id != part_id) continue;
            for (int i = 0; i < 2; ++i) {
                const double side = i == 0 ? -1.0 : 1.0;
                const Vec3 pedal = p.centre_m + seat_.right * (side * 0.25 * p.size_m.x);
                tg.feet[i] = pedal - seat_.forward * (0.12 + 0.5 * p.size_m.y) + Vec3{0.0, 0.0, body_->dims.AnkleHeight() * 0.5};
            }
            tg.feet_set = true;
            return;
        }
    }

    Vec3 LookTarget(const SeatActivity& a, const std::string& reach) const {
        ControlTouch t;
        if (!reach.empty() && TouchOf(*cabin_, reach, t)) return t.point;
        if (!a.control.empty() && a.act_in_s >= 0.0 && a.act_in_s < 1.5 && TouchOf(*cabin_, a.control, t)) return t.point;
        const std::string look = seat_def_ != nullptr ? seat_def_->look : std::string("forward");
        std::size_t from = 0;
        while (look != "forward" && from <= look.size()) {
            const std::size_t bar = look.find('|', from);
            const std::string id = look.substr(from, bar == std::string::npos ? std::string::npos : bar - from);
            for (const CabinDisplay& disp : cabin_->displays) {
                if (disp.id != id) continue;
                if (const CabinPanel* p = cabin_->FindPanel(disp.panel_id)) return p->PointAt(disp.u_m, disp.v_m);
            }
            if (bar == std::string::npos) break;
            from = bar + 1;
        }
        const double pitch = seat_.view_pitch_rad;
        return seat_.eye + (seat_.forward * std::cos(pitch) + Vec3{0.0, 0.0, 1.0} * std::sin(pitch)) * 5.0;
    }

    struct WheelHold {
        bool held = false;
        std::string control;
        double grabbed_at = 0.0;
    };

    const CabinLayout* cabin_;
    const BodyData* body_;
    const BodySeatDef* seat_def_ = nullptr;
    SeatFrame seat_;
    bool valid_ = false;
    BodyPose pose_;
    std::map<std::string, ReachPlan> plans_;
    Hand hands_[2];
    WheelHold wheel_[2];
    TorsoLean lean_;
    double standing_ = 0.0;
    Vec3 stand_hip_;
    Vec3 stand_forward_{0.0, 1.0, 0.0};
    double time_s_ = 0.0;
};

}  // namespace simcore
