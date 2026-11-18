// simcore/air_war/air_combat_data.hpp — loaders for the air battle's air-to-air data (roadmap track B, B7):
// data/air_war/air_combat.json (AirCombatDef) and data/air_war/sensors.json (SensorsDef).
//
// Strict like the other air war loaders: unknown keys fail, every block names its source and confidence, numbers are
// range-checked, and the cross-references (a fit's missiles and guns, an export fit, a doctrine's faction) are checked at
// load. data.hpp includes this file last, so AirWarData can hold both.
#pragma once

#include "simcore/air_war/air_combat_types.hpp"
#include "simcore/air_war/data.hpp"

namespace simcore {
namespace airwar {
namespace detail {

class AirCombatLoader : public AirWarLoader {
public:
    bool LoadAirCombat(const json::Value& root, AirCombatDef& out) {
        if (!root.Is(json::Type::Object)) return Fail("", "expected an object");
        if (!Keys(root, "", {"air_combat_id", "status", "source", "confidence", "notes", "aams", "guns", "fits", "doctrine", "calibration", "fight",
                             "ours"}) ||
            !Provenance(root, "")) {
            return false;
        }
        const json::Value* aams = OptionalArray(root, "", "aams");
        if (aams == nullptr || aams->items.empty()) return message_.empty() ? Fail("aams", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < aams->items.size(); ++i) {
            AamDef a;
            if (!Aam(aams->items[i], Index("aams", i), a)) return false;
            if (out.Aam(a.id) != nullptr) return Fail(Index("aams", i), "duplicate missile id '" + a.id + "'");
            out.aams.push_back(std::move(a));
        }
        const json::Value* guns = OptionalArray(root, "", "guns");
        if (guns == nullptr || guns->items.empty()) return message_.empty() ? Fail("guns", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < guns->items.size(); ++i) {
            AirGunDef g;
            if (!Gun(guns->items[i], Index("guns", i), g)) return false;
            if (out.Gun(g.id) != nullptr) return Fail(Index("guns", i), "duplicate gun id '" + g.id + "'");
            out.guns.push_back(std::move(g));
        }
        const json::Value* fits = OptionalArray(root, "", "fits");
        if (fits == nullptr || fits->items.empty()) return message_.empty() ? Fail("fits", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < fits->items.size(); ++i) {
            FitDef f;
            if (!Fit(fits->items[i], Index("fits", i), f)) return false;
            if (out.Fit(f.id) != nullptr) return Fail(Index("fits", i), "duplicate fit id '" + f.id + "'");
            for (const std::string& al : f.aliases) {
                if (out.Fit(al) != nullptr) return Fail(Index("fits", i), "alias '" + al + "' is already a fit");
            }
            out.fits.push_back(std::move(f));
        }
        for (std::size_t i = 0; i < out.fits.size(); ++i) {
            const FitDef& f = out.fits[i];
            const std::string p = Index("fits", i);
            for (const LoadEra& e : f.loads_by_year) {
                for (const auto& a : e.aams) {
                    if (out.Aam(a.first) == nullptr) return Fail(p, f.id + " carries an unknown missile '" + a.first + "'");
                }
                if (!e.gun.empty() && out.Gun(e.gun) == nullptr) return Fail(p, f.id + " carries an unknown gun '" + e.gun + "'");
            }
            if (!f.export_fit.empty() && out.Fit(f.export_fit) == nullptr) return Fail(Join(p, "export_fit"), "no fit '" + f.export_fit + "'");
        }
        const json::Value* doc = OptionalArray(root, "", "doctrine");
        if (doc == nullptr || doc->items.empty()) return message_.empty() ? Fail("doctrine", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < doc->items.size(); ++i) {
            AirDoctrineDef d;
            if (!Doctrine(doc->items[i], Index("doctrine", i), d)) return false;
            if (out.Doctrine(d.faction) != nullptr) return Fail(Index("doctrine", i), "duplicate faction '" + d.faction + "'");
            out.doctrines.push_back(std::move(d));
        }
        const json::Value* cal = root.Find("calibration");
        if (cal == nullptr) return Fail("calibration", "required");
        if (!Calibration(*cal, "calibration", out.calibration)) return false;
        if (const json::Value* fj = root.Find("fight")) {
            if (!Fight(*fj, "fight", out.fight)) return false;
        }
        if (const json::Value* oj = root.Find("ours")) {
            if (!Ours(*oj, "ours", out)) return false;
        }
        return true;
    }

    bool LoadSensors(const json::Value& root, SensorsDef& out) {
        if (!root.Is(json::Type::Object)) return Fail("", "expected an object");
        if (!Keys(root, "", {"sensors_id", "status", "source", "confidence", "notes", "sensors", "slots", "rwr_sets"}) || !Provenance(root, "")) return false;
        const json::Value* arr = OptionalArray(root, "", "sensors");
        if (arr == nullptr || arr->items.empty()) return message_.empty() ? Fail("sensors", "required, a non-empty list") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            SensorDef s;
            if (!Sensor(arr->items[i], Index("sensors", i), s)) return false;
            if (out.Find(s.id) != nullptr) return Fail(Index("sensors", i), "duplicate sensor id '" + s.id + "'");
            out.sensors.push_back(std::move(s));
        }
        if (const json::Value* sl = OptionalArray(root, "", "slots")) {
            for (std::size_t i = 0; i < sl->items.size(); ++i) {
                const json::Value& v = sl->items[i];
                const std::string p = Index("slots", i);
                SensorSlotDef s;
                std::string kind;
                if (!Keys(v, p, {"id", "name", "kind", "carriers", "needs", "notes"}) || !RequireString(v, p, "id", s.id) ||
                    !RequireString(v, p, "name", s.name) || !RequireString(v, p, "kind", kind) || !RequireString(v, p, "needs", s.needs) ||
                    !StringList(v, p, "carriers", s.carriers)) {
                    return false;
                }
                if (!ParseSensorKind(kind, s.kind)) return Fail(Join(p, "kind"), "unknown sensor kind '" + kind + "'");
                if (out.Find(s.id) != nullptr) return Fail(p, "'" + s.id + "' is a sensor already: a slot has no year");
                out.slots.push_back(std::move(s));
            }
        } else if (!message_.empty()) {
            return false;
        }
        return true;
    }

private:
    // A value is either a game value (named in the entry's game_values) or sourced by the entry's source.
    bool GameValues(const json::Value& v, const std::string& p, std::initializer_list<const char*> keys, std::vector<std::string>& out) {
        if (!StringList(v, p, "game_values", out)) return false;
        for (const std::string& k : out) {
            bool known = false;
            for (const char* a : keys) known = known || k == a;
            if (!known) return Fail(Join(p, "game_values"), "'" + k + "' is not a key of this entry");
        }
        return true;
    }

    bool Aam(const json::Value& v, const std::string& p, AamDef& a) {
        static const std::initializer_list<const char*> kKeys = {
            "id", "name", "guidance", "in_service_year", "allies_year", "max_head_m", "max_rear_m", "low_factor", "min_m", "launch_g",
            "seeker_cone_deg", "target_g", "sun_exclusion_deg", "clutter_below", "autonomous_s", "flare_rejection", "reliability",
            "design_pk", "lethal_m", "mean_mps", "munition", "game_values", "source", "confidence", "notes"};
        std::string guidance;
        double cone_deg = 12.0;
        double sun_deg = 0.0;
        if (!Keys(v, p, kKeys) || !RequireString(v, p, "id", a.id) || !RequireString(v, p, "name", a.name) ||
            !RequireString(v, p, "guidance", guidance) || !Int(v, p, "in_service_year", a.in_service_year, 0, 2050) ||
            !Int(v, p, "allies_year", a.allies_year, 0, 2050) || !Num(v, p, "max_head_m", a.max_head_m, 0.0, 300000.0) ||
            !Num(v, p, "max_rear_m", a.max_rear_m, 0.0, 300000.0) || !Num(v, p, "low_factor", a.low_factor, 0.05, 1.0) ||
            !Num(v, p, "min_m", a.min_m, 0.0, 20000.0) || !Num(v, p, "launch_g", a.launch_g, 0.5, 12.0) ||
            !Num(v, p, "seeker_cone_deg", cone_deg, 1.0, 90.0) || !Num(v, p, "target_g", a.target_g, 1.0, 40.0) ||
            !Num(v, p, "sun_exclusion_deg", sun_deg, 0.0, 90.0) || !Num(v, p, "clutter_below", a.clutter_below, 0.0, 1.0) ||
            !Num(v, p, "autonomous_s", a.autonomous_s, 0.0, 60.0) || !Num(v, p, "flare_rejection", a.flare_rejection, 0.0, 1.0) ||
            !Num(v, p, "reliability", a.reliability, 0.0, 1.0) || !Num(v, p, "design_pk", a.design_pk, 0.0, 1.0) ||
            !Num(v, p, "lethal_m", a.lethal_m, 0.1, 100.0) || !Num(v, p, "mean_mps", a.mean_mps, 50.0, 2000.0) ||
            !OptionalString(v, p, "munition", a.munition) || !GameValues(v, p, kKeys, a.game_values) || !Provenance(v, p) ||
            !OptionalString(v, p, "source", a.source) || !OptionalString(v, p, "confidence", a.confidence) ||
            !OptionalString(v, p, "notes", a.notes)) {
            return false;
        }
        if (!ParseGuidance(guidance, a.guidance)) return Fail(Join(p, "guidance"), "ir_rear, ir_all_aspect, sarh or sarh_inertial");
        a.seeker_cone_rad = cone_deg * kDegToRad;
        a.sun_exclusion_rad = sun_deg * kDegToRad;
        if (a.max_rear_m <= 0.0) return Fail(Join(p, "max_rear_m"), "a missile reaches something from astern");
        if (a.guidance == Guidance::IrRear && a.max_head_m > 0.0) return Fail(Join(p, "max_head_m"), "a rear-aspect seeker has no head-on zone (0)");
        if (a.guidance != Guidance::IrRear && a.max_head_m <= 0.0) return Fail(Join(p, "max_head_m"), "needs a head-on range");
        if (a.min_m >= a.max_rear_m) return Fail(Join(p, "min_m"), "inside the maximum range");
        if (a.guidance == Guidance::SarhInertial && a.autonomous_s <= 0.0) return Fail(Join(p, "autonomous_s"), "an inertial stretch of flight is the class");
        if (a.guidance != Guidance::SarhInertial && a.autonomous_s > 0.0) return Fail(Join(p, "autonomous_s"), "only sarh_inertial flies on after its lock is lost");
        if (a.allies_year > 0 && a.allies_year < a.in_service_year) return Fail(Join(p, "allies_year"), "not before the year it entered service");
        return true;
    }

    bool Gun(const json::Value& v, const std::string& p, AirGunDef& g) {
        static const std::initializer_list<const char*> kKeys = {"id", "name", "firing_s", "effective_m", "kill_per_s", "in_service_year",
                                                                "game_values", "source", "confidence", "notes"};
        if (!Keys(v, p, kKeys) || !RequireString(v, p, "id", g.id) || !RequireString(v, p, "name", g.name) ||
            !Num(v, p, "firing_s", g.firing_s, 0.5, 60.0) || !Num(v, p, "effective_m", g.effective_m, 50.0, 5000.0) ||
            !Num(v, p, "kill_per_s", g.kill_per_s, 0.0, 5.0) || !Int(v, p, "in_service_year", g.in_service_year, 0, 2050) ||
            !GameValues(v, p, kKeys, g.game_values) || !Provenance(v, p) || !OptionalString(v, p, "source", g.source) ||
            !OptionalString(v, p, "confidence", g.confidence) || !OptionalString(v, p, "notes", g.notes)) {
            return false;
        }
        return true;
    }

    bool Fit(const json::Value& v, const std::string& p, FitDef& f) {
        if (!Keys(v, p, {"id", "aliases", "iff", "command_link", "vid_aid_m", "loads_by_year", "export_fit", "source", "confidence", "notes"}) ||
            !RequireString(v, p, "id", f.id) || !StringList(v, p, "aliases", f.aliases) || !OptionalString(v, p, "iff", f.iff) ||
            !OptionalBool(v, p, "command_link", f.command_link) || !Num(v, p, "vid_aid_m", f.vid_aid_m, 0.0, 50000.0) ||
            !OptionalString(v, p, "export_fit", f.export_fit) || !Provenance(v, p) || !OptionalString(v, p, "source", f.source) ||
            !OptionalString(v, p, "confidence", f.confidence) || !OptionalString(v, p, "notes", f.notes)) {
            return false;
        }
        if (f.iff != "mk12" && f.iff != "sro2m" && f.iff != "parol" && f.iff != "kremniy2m" && f.iff != "none") {
            return Fail(Join(p, "iff"), "mk12, sro2m, parol, kremniy2m or none");
        }
        const json::Value* eras = OptionalArray(v, p, "loads_by_year");
        if (eras == nullptr) return message_.empty() ? Fail(Join(p, "loads_by_year"), "required (an empty list: it carries nothing)") : false;
        for (std::size_t i = 0; i < eras->items.size(); ++i) {
            const json::Value& e = eras->items[i];
            const std::string ep = Index(Join(p, "loads_by_year"), i);
            LoadEra era;
            if (!Keys(e, ep, {"from", "aams", "gun", "notes"}) || !OptionalString(e, ep, "gun", era.gun)) return false;
            if (e.Find("from") == nullptr) return Fail(Join(ep, "from"), "required");
            if (!Int(e, ep, "from", era.from_year, 1950, 2050)) return false;
            if (!f.loads_by_year.empty() && era.from_year <= f.loads_by_year.back().from_year) return Fail(Join(ep, "from"), "years in rising order");
            if (const json::Value* list = OptionalArray(e, ep, "aams")) {
                for (std::size_t k = 0; k < list->items.size(); ++k) {
                    const json::Value& pair = list->items[k];
                    const std::string pp = Index(Join(ep, "aams"), k);
                    if (!pair.Is(json::Type::Array) || pair.items.size() != 2 || !pair.items[0].Is(json::Type::String) ||
                        !pair.items[1].Is(json::Type::Number) || pair.items[1].number < 1.0 || pair.items[1].number > 12.0) {
                        return Fail(pp, "[missile id, count 1..12]");
                    }
                    era.aams.push_back({pair.items[0].string, static_cast<int>(pair.items[1].number)});
                }
            } else if (!message_.empty()) {
                return false;
            }
            f.loads_by_year.push_back(std::move(era));
        }
        return true;
    }

    bool OneOf(const json::Value& v, const std::string& p, const char* key, std::string& out, std::initializer_list<const char*> allowed) {
        if (!RequireString(v, p, key, out)) return false;
        for (const char* a : allowed) {
            if (out == a) return true;
        }
        std::string list;
        for (const char* a : allowed) list += std::string(list.empty() ? "" : ", ") + a;
        return Fail(Join(p, key), "one of " + list);
    }

    bool Doctrine(const json::Value& v, const std::string& p, AirDoctrineDef& d) {
        if (!Keys(v, p, {"faction", "roe", "commit_by", "initiative_at_contact", "formation", "sort", "shot_order", "tactic", "cooperative_id",
                         "strikers_answer", "skill", "on_control_lost", "commit_odds", "fighter_plan", "source", "confidence", "notes"}) ||
            !Num(v, p, "skill", d.skill, 0.0, 1.0) || !Num(v, p, "commit_odds", d.commit_odds, 0.0, 10.0) ||
            !OneOf(v, p, "faction", d.faction, {"nato", "warsaw_pact", "neutral", "rebels", "ours"}) ||
            !OneOf(v, p, "roe", d.roe, {"vid", "cleared_bvr", "controller"}) ||
            !OneOf(v, p, "commit_by", d.commit_by, {"leader", "controller"}) ||
            !OneOf(v, p, "initiative_at_contact", d.initiative_at_contact, {"flight_leader", "controller", "by_skill"}) ||
            !OneOf(v, p, "formation", d.formation, {"line_abreast_pair", "sword_shield", "pair", "single"}) ||
            !OneOf(v, p, "sort", d.sort, {"left_low_lead", "controller", "nearest"}) ||
            !OneOf(v, p, "tactic", d.tactic, {"turn", "slash", "beam"}) || !StringList(v, p, "shot_order", d.shot_order) ||
            !OptionalBool(v, p, "cooperative_id", d.cooperative_id) || !Provenance(v, p) || !OptionalString(v, p, "source", d.source) ||
            !OptionalString(v, p, "confidence", d.confidence) || !OptionalString(v, p, "notes", d.notes)) {
            return false;
        }
        if (d.shot_order.empty()) return Fail(Join(p, "shot_order"), "at least one of sarh, ir, gun");
        for (const std::string& s : d.shot_order) {
            if (s != "sarh" && s != "ir" && s != "gun") return Fail(Join(p, "shot_order"), "sarh, ir or gun");
        }
        const json::Value* sa = v.Find("strikers_answer");
        if (sa == nullptr) return Fail(Join(p, "strikers_answer"), "required: {jettison, press_on, abort} as shares 0..1");
        const std::string sp = Join(p, "strikers_answer");
        if (!Keys(*sa, sp, {"jettison", "press_on", "abort", "source", "confidence", "notes"}) || !Num(*sa, sp, "jettison", d.strikers_jettison, 0.0, 1.0) ||
            !Num(*sa, sp, "press_on", d.strikers_press_on, 0.0, 1.0) || !Num(*sa, sp, "abort", d.strikers_abort, 0.0, 1.0) || !Provenance(*sa, sp)) {
            return false;
        }
        if (const json::Value* cl = v.Find("on_control_lost")) {
            const std::string cp = Join(p, "on_control_lost");
            if (!Keys(*cl, cp, {"green", "skilled", "source", "confidence", "notes"}) || !Provenance(*cl, cp) ||
                !OneOf(*cl, cp, "green", d.control_lost_green, {"abort", "pairs", "continue"}) ||
                !OneOf(*cl, cp, "skilled", d.control_lost_skilled, {"abort", "pairs", "continue"})) {
                return false;
            }
        }
        if (const json::Value* fp = v.Find("fighter_plan")) {
            const std::string pp = Join(p, "fighter_plan");
            if (!Keys(*fp, pp, {"close", "sweep", "cap", "sweep_lead_s", "source", "confidence", "notes"}) || !Provenance(*fp, pp) ||
                !Num(*fp, pp, "close", d.plan_close, 0.0, 1.0) || !Num(*fp, pp, "sweep", d.plan_sweep, 0.0, 1.0) ||
                !Num(*fp, pp, "cap", d.plan_cap, 0.0, 1.0) || !Num(*fp, pp, "sweep_lead_s", d.sweep_lead_s, 0.0, 3600.0)) {
                return false;
            }
            if (d.plan_close + d.plan_sweep + d.plan_cap <= 0.0) return Fail(pp, "close, sweep and cap may not all be 0");
        }
        return true;
    }

    bool Fight(const json::Value& v, const std::string& p, FightParams& f) {
        const std::vector<Slot> slots = {
            {"think_s", &f.think_s, 1.0, 0.05, 10.0},
            {"radar_scan_s", &f.radar_scan_s, 1.0, 0.1, 30.0},
            {"radar_az_deg", &f.radar_az_rad, kDegToRad, 5.0, 180.0},
            {"contact_hold_s", &f.contact_hold_s, 1.0, 0.5, 120.0},
            {"vid_m", &f.vid_m, 1.0, 100.0, 50000.0},
            {"friendly_clear_m", &f.friendly_clear_m, 1.0, 0.0, 200000.0},
            {"gci_vector_s", &f.gci_vector_s, 1.0, 1.0, 300.0},
            {"gci_update_s", &f.gci_update_s, 1.0, 1.0, 600.0},
            {"gci_error_m", &f.gci_error_m, 1.0, 0.0, 20000.0},
            {"control_lost_s", &f.control_lost_s, 1.0, 5.0, 1800.0},
            {"joker_s", &f.joker_s, 1.0, 10.0, 7200.0},
            {"bingo_s", &f.bingo_s, 1.0, 10.0, 7200.0},
            {"break_g", &f.break_g, 1.0, 1.0, 12.0},
            {"gun_cone_deg", &f.gun_cone_rad, kDegToRad, 1.0, 90.0},
            {"gun_aspect_deg", &f.gun_aspect_rad, kDegToRad, 1.0, 180.0},
            {"extend_s", &f.extend_s, 1.0, 1.0, 300.0},
            {"claim_on_miss", &f.claim_on_miss, 1.0, 0.0, 1.0},
            {"slow_target_mps", &f.slow_target_mps, 1.0, 0.0, 400.0},
            {"a0_intercept", &f.a0_intercept, 1.0, 0.0, 1.0},
            {"a0_detect_day", &f.a0_detect_day, 1.0, 0.0, 1.0},
            {"a0_detect_night", &f.a0_detect_night, 1.0, 0.0, 1.0},
            {"a0_detect_poor", &f.a0_detect_poor, 1.0, 0.0, 1.0},
            {"a0_detect_low", &f.a0_detect_low, 1.0, 0.0, 1.0},
            {"a0_rounds", &f.a0_rounds, 1.0, 1.0, 6.0},
            {"a0_meet_sweep", &f.a0_meet_sweep, 1.0, 0.0, 1.0},
            {"a0_meet_cap", &f.a0_meet_cap, 1.0, 0.0, 1.0},
            {"a0_meet_close", &f.a0_meet_close, 1.0, 0.0, 1.0},
            {"a0_awacs_meet", &f.a0_awacs_meet, 1.0, 0.0, 5.0},
            {"a0_awacs_meet_mode0", &f.a0_awacs_meet_mode0, 1.0, 0.0, 5.0},
            {"a0_surprise_gci", &f.a0_surprise_gci, 1.0, 0.0, 1.0},
            {"a0_surprise_jammed", &f.a0_surprise_jammed, 1.0, 0.0, 1.0},
            {"a0_seen_awacs", &f.a0_seen_awacs, 1.0, 0.0, 1.0},
            {"a0_seen_eye", &f.a0_seen_eye, 1.0, 0.0, 1.0},
            {"a0_gci_jammed", &f.a0_gci_jammed, 1.0, 0.0, 1.0},
            {"a0_green_home", &f.a0_green_home, 1.0, 0.0, 1.0},
            {"a0_sarh_bvr_m", &f.a0_sarh_bvr_m, 1.0, 100.0, 100000.0},
            {"a0_sarh_vid_m", &f.a0_sarh_vid_m, 1.0, 100.0, 100000.0},
            {"a0_ir_m", &f.a0_ir_m, 1.0, 100.0, 100000.0},
            {"a0_gun_m", &f.a0_gun_m, 1.0, 50.0, 5000.0},
            {"a0_range_sigma", &f.a0_range_sigma, 1.0, 0.0, 3.0},
        };
        if (!Group(v, p, slots)) return false;
        if (f.bingo_s < f.joker_s) return Fail(Join(p, "bingo_s"), "not before joker_s");
        return true;
    }

    bool Ours(const json::Value& v, const std::string& p, AirCombatDef& out) {
        if (!Keys(v, p, {"regiments", "source", "confidence", "notes"}) || !Provenance(v, p)) return false;
        const json::Value* arr = OptionalArray(v, p, "regiments");
        if (arr == nullptr) return message_.empty() ? Fail(Join(p, "regiments"), "required") : false;
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            const json::Value& r = arr->items[i];
            const std::string rp = Index(Join(p, "regiments"), i);
            OurRegimentDef rg;
            if (!Keys(r, rp, {"id", "types", "notes"}) || !RequireString(r, rp, "id", rg.id)) return false;
            const json::Value* ts = OptionalArray(r, rp, "types");
            if (ts == nullptr || ts->items.empty()) return message_.empty() ? Fail(Join(rp, "types"), "required: [[fit id, count], ...]") : false;
            for (std::size_t k = 0; k < ts->items.size(); ++k) {
                const json::Value& pair = ts->items[k];
                const std::string tp = Index(Join(rp, "types"), k);
                if (!pair.Is(json::Type::Array) || pair.items.size() != 2 || !pair.items[0].Is(json::Type::String) ||
                    !pair.items[1].Is(json::Type::Number) || pair.items[1].number < 1.0) {
                    return Fail(tp, "[fit id, count >= 1]");
                }
                if (out.Fit(pair.items[0].string) == nullptr) return Fail(tp, "no fit '" + pair.items[0].string + "'");
                rg.types.push_back({pair.items[0].string, static_cast<int>(pair.items[1].number)});
            }
            out.ours.regiments.push_back(std::move(rg));
        }
        return true;
    }

    // One number of a group: its JSON key, where it goes, the unit scale to the stored one and its range.
    struct Slot {
        const char* key;
        double* where;
        double scale;
        double lo;
        double hi;
    };

    bool Group(const json::Value& v, const std::string& p, const std::vector<Slot>& slots) {
        if (!v.Is(json::Type::Object)) return Fail(p, "expected an object");
        for (const std::string& k : v.keys) {
            bool ok = k == "source" || k == "confidence" || k == "notes";
            for (const Slot& s : slots) ok = ok || k == s.key;
            if (!ok) return Fail(Join(p, k), "unknown key");
        }
        if (!Provenance(v, p)) return false;
        for (const Slot& s : slots) {
            double x = 0.0;
            const bool has = v.Find(s.key) != nullptr;
            if (has && !Num(v, p, s.key, x, s.lo, s.hi)) return false;
            if (has) *s.where = x * s.scale;
        }
        return true;
    }

    bool Calibration(const json::Value& v, const std::string& p, CalibrationDef& c) {
        if (!Keys(v, p, {"chain", "mix", "targets", "source", "confidence", "notes"}) || !Provenance(v, p)) return false;
        ChainParams& n = c.chain;
        const std::vector<Slot> chain = {
            {"alt_ref_m", &n.alt_ref_m, 1.0, 1000.0, 30000.0},
            {"rear_aspect_limit_deg", &n.rear_aspect_limit_rad, kDegToRad, 10.0, 180.0},
            {"judge_range_sigma", &n.judge_range_sigma, 1.0, 0.0, 2.0},
            {"judge_aspect_sigma_deg", &n.judge_aspect_sigma_rad, kDegToRad, 0.0, 90.0},
            {"judge_cone_sigma_deg", &n.judge_cone_sigma_rad, kDegToRad, 0.0, 45.0},
            {"judge_g_sigma", &n.judge_g_sigma, 1.0, 0.0, 2.0},
            {"judge_skill_gain", &n.judge_skill_gain, 1.0, 0.0, 1.0},
            {"judge_stress_gain", &n.judge_stress_gain, 1.0, 0.0, 3.0},
            {"judge_sarh_scale", &n.judge_sarh_scale, 1.0, 0.1, 5.0},
            {"switch_error_base", &n.switch_error_base, 1.0, 0.0, 1.0},
            {"switch_idle_share", &n.switch_idle_share, 1.0, 0.0, 1.0},
            {"switch_skill_gain", &n.switch_skill_gain, 1.0, 0.0, 1.0},
            {"switch_stress_gain", &n.switch_stress_gain, 1.0, 0.0, 3.0},
            {"guides_ir_rear", &n.guides_ir_rear, 1.0, 0.0, 1.0},
            {"guides_ir_all_aspect", &n.guides_ir_all_aspect, 1.0, 0.0, 1.0},
            {"guides_sarh", &n.guides_sarh, 1.0, 0.0, 1.0},
            {"guides_sarh_inertial", &n.guides_sarh_inertial, 1.0, 0.0, 1.0},
            {"aspect_ir_rear_edge", &n.aspect_ir_rear_edge, 1.0, 0.0, 1.0},
            {"aspect_ir_all_head", &n.aspect_ir_all_head, 1.0, 0.0, 1.0},
            {"sun_soft_factor", &n.sun_soft_factor, 1.0, 1.0, 5.0},
            {"sun_soft_floor", &n.sun_soft_floor, 1.0, 0.0, 1.0},
            {"beam_notch", &n.beam_notch, 1.0, 0.0, 1.0},
            {"beam_notch_half_deg", &n.beam_notch_half_rad, kDegToRad, 1.0, 60.0},
            {"break_max", &n.break_max, 1.0, 0.0, 1.0},
            {"break_miss_m", &n.break_miss_m, 1.0, 0.0, 100.0},
            {"nominal_miss_m", &n.nominal_miss_m, 1.0, 0.1, 50.0},
            {"gun_skill_floor", &n.gun_skill_floor, 1.0, 0.0, 1.0},
            {"gun_range_edge", &n.gun_range_edge, 1.0, 0.0, 1.0},
            {"gun_g_loss", &n.gun_g_loss, 1.0, 0.0, 1.0},
            {"gun_g_full", &n.gun_g_full, 1.0, 1.0, 20.0},
        };
        const json::Value* cj = v.Find("chain");
        if (cj == nullptr) return Fail(Join(p, "chain"), "required");
        if (!Group(*cj, Join(p, "chain"), chain)) return false;
        CalibrationMix& m = c.mix;
        const std::vector<Slot> mix = {
            {"skill", &m.skill, 1.0, 0.0, 1.0},
            {"stress", &m.stress, 1.0, 0.0, 1.0},
            {"mode_change_share", &m.mode_change_share, 1.0, 0.0, 1.0},
            {"range_median_m", &m.range_median_m, 1.0, 100.0, 100000.0},
            {"range_sigma_ln", &m.range_sigma_ln, 1.0, 0.0, 3.0},
            {"aspect_rear_share", &m.aspect_rear_share, 1.0, 0.0, 1.0},
            {"off_boresight_sigma_deg", &m.off_boresight_sigma_rad, kDegToRad, 1.0, 60.0},
            {"altitude_m", &m.altitude_m, 1.0, 0.0, 20000.0},
            {"launch_g_mean", &m.launch_g_mean, 1.0, 1.0, 8.0},
            {"break_share", &m.break_share, 1.0, 0.0, 1.0},
            {"break_g", &m.break_g, 1.0, 1.0, 12.0},
            {"look_down_share", &m.look_down_share, 1.0, 0.0, 1.0},
            {"sun_share", &m.sun_share, 1.0, 0.0, 1.0},
            {"countermeasure_share", &m.countermeasure_share, 1.0, 0.0, 1.0},
        };
        const json::Value* mj = v.Find("mix");
        if (mj == nullptr) return Fail(Join(p, "mix"), "required");
        if (!Group(*mj, Join(p, "mix"), mix)) return false;
        const json::Value* tj = v.Find("targets");
        if (tj == nullptr || !tj->Is(json::Type::Object) || tj->keys.empty()) return Fail(Join(p, "targets"), "required: {name: {value, tolerance, source, confidence}}");
        for (std::size_t i = 0; i < tj->keys.size(); ++i) {
            const std::string tp = Join(Join(p, "targets"), tj->keys[i]);
            const json::Value& t = tj->items[i];
            CalibrationTarget ct;
            if (!Keys(t, tp, {"value", "tolerance", "source", "confidence", "notes"}) || !Provenance(t, tp) ||
                !OptionalString(t, tp, "source", ct.source) || !OptionalString(t, tp, "confidence", ct.confidence) ||
                !OptionalString(t, tp, "notes", ct.notes)) {
                return false;
            }
            if (t.Find("value") == nullptr) return Fail(Join(tp, "value"), "required");
            if (!Num(t, tp, "value", ct.value, -1.0e6, 1.0e6) || !Num(t, tp, "tolerance", ct.tolerance, 0.0, 1.0e6)) return false;
            c.targets[tj->keys[i]] = ct;
        }
        return true;
    }

    bool Sensor(const json::Value& v, const std::string& p, SensorDef& s) {
        std::string kind;
        if (!Keys(v, p, {"id", "name", "kind", "carriers", "in_service_year", "allies_year", "placeholder", "radar_def", "library", "rcs_m2",
                         "by_hand_m", "cued_m", "lock_m", "design_m", "clutter_m", "rear_m", "vid_m", "source", "confidence", "notes"}) ||
            !RequireString(v, p, "id", s.id) || !RequireString(v, p, "name", s.name) || !RequireString(v, p, "kind", kind) ||
            !StringList(v, p, "carriers", s.carriers) || !Int(v, p, "in_service_year", s.in_service_year, 1900, 2050) ||
            !Int(v, p, "allies_year", s.allies_year, 0, 2050) || !OptionalBool(v, p, "placeholder", s.placeholder) ||
            !OptionalString(v, p, "radar_def", s.radar_def) || !OptionalString(v, p, "library", s.library) ||
            !Num(v, p, "rcs_m2", s.rcs_m2, 0.0, 1000.0) || !Num(v, p, "by_hand_m", s.by_hand_m, 0.0, 500000.0) ||
            !Num(v, p, "cued_m", s.cued_m, 0.0, 500000.0) || !Num(v, p, "lock_m", s.lock_m, 0.0, 500000.0) ||
            !Num(v, p, "design_m", s.design_m, 0.0, 500000.0) || !Num(v, p, "clutter_m", s.clutter_m, 0.0, 500000.0) ||
            !Num(v, p, "rear_m", s.rear_m, 0.0, 500000.0) || !Num(v, p, "vid_m", s.vid_m, 0.0, 50000.0) || !Provenance(v, p) ||
            !OptionalString(v, p, "source", s.source) || !OptionalString(v, p, "confidence", s.confidence) ||
            !OptionalString(v, p, "notes", s.notes)) {
            return false;
        }
        if (!ParseSensorKind(kind, s.kind)) return Fail(Join(p, "kind"), "unknown sensor kind '" + kind + "'");
        if (s.carriers.empty()) return Fail(Join(p, "carriers"), "at least one aircraft type");
        if (s.in_service_year <= 0) return Fail(Join(p, "in_service_year"), "required: an entry whose year is unsourced is a slot, not a sensor");
        if (s.allies_year > 0 && s.allies_year < s.in_service_year) return Fail(Join(p, "allies_year"), "not before the year it entered service");
        if (s.kind == SensorKind::Radar && (s.radar_def.empty() || s.library.empty())) {
            return Fail(p, "a radar names its EM RadarDef (radar_def) and the library file (library)");
        }
        if (s.kind != SensorKind::Radar && (!s.radar_def.empty() || !s.library.empty())) return Fail(p, "only a radar has a radar_def");
        return true;
    }

    static constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
};

template <typename Out, typename Fn>
bool LoadCombatText(const std::string& text, const std::string& where, Out& out, std::string* error, Fn&& fn) {
    json::Value root;
    std::string perr;
    if (!json::Parse(text, root, &perr)) {
        if (error != nullptr) *error = where + perr;
        return false;
    }
    AirCombatLoader loader;
    Out tmp;
    if (!fn(loader, root, tmp)) {
        if (error != nullptr) *error = where + loader.Message();
        return false;
    }
    out = std::move(tmp);
    return true;
}

template <typename Out, typename Fn>
bool LoadCombatFile(const std::string& path, Out& out, std::string* error, Fn&& fn) {
    std::string text;
    if (!ReadFile(path, text, error)) return false;
    return LoadCombatText(text, path + ": ", out, error, fn);
}

}  // namespace detail

inline bool LoadAirCombatFile(const std::string& path, AirCombatDef& out, std::string* error) {
    return detail::LoadCombatFile(path, out, error, [](detail::AirCombatLoader& l, const json::Value& r, AirCombatDef& o) { return l.LoadAirCombat(r, o); });
}

inline bool LoadSensorsFile(const std::string& path, SensorsDef& out, std::string* error) {
    return detail::LoadCombatFile(path, out, error, [](detail::AirCombatLoader& l, const json::Value& r, SensorsDef& o) { return l.LoadSensors(r, o); });
}

// Text loaders (tests).
inline bool LoadAirCombatJson(const std::string& text, AirCombatDef& out, std::string* error = nullptr) {
    return detail::LoadCombatText(text, "", out, error, [](detail::AirCombatLoader& l, const json::Value& r, AirCombatDef& o) { return l.LoadAirCombat(r, o); });
}

inline bool LoadSensorsJson(const std::string& text, SensorsDef& out, std::string* error = nullptr) {
    return detail::LoadCombatText(text, "", out, error, [](detail::AirCombatLoader& l, const json::Value& r, SensorsDef& o) { return l.LoadSensors(r, o); });
}

}  // namespace airwar
}  // namespace simcore
