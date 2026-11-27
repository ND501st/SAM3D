// simcore/vehicle_systems.hpp — the vehicle's own systems as its crew works them (roadmap controls C1-C3, C7, C8).
//
// The 9A33BM2's chassis 5937 and what it carries besides the fire-control equipment, read from the cabin's controls and
// the world, every step (data/vehicles/<system>_systems.json; research in docs/research/OSA_VEHICLE_SYSTEMS_2026-11.md):
//
//   * The drive engine (C1). An equipment subsystem ("engine", latching on the starter, released by the stop control),
//     so the drill master and the helper plan its start like any other. This model gives it its numbers: speed from the
//     hand throttle or the road, the coolant and oil warming up and cooling down, oil pressure, the starter's draw. A
//     cold engine will not start until the pre-heater has run (input "engine_cold"). The gearbox, handbrake, water-jet
//     flaps and wave shield cap the vehicle's speed (SpeedCapMps): the battery moves the own vehicle no faster.
//   * The 24 V network (C7): the battery switch and two 12СТ-70, charged at 27.5 V while a generator runs, drawn down by
//     the starter and the network's loads otherwise (input "battery_flat"). The С-75 generators' voltages set by hand
//     (НАПР. ГТА, НАПР. ГОМ) and read on 0096-9M1's voltmeter; the take-off generator's frequency follows the engine
//     speed the hand throttle holds, and the УКЧ sheds the load below 360 Hz (input "ukch_shed").
//   * The lights (C2): what the switches light outside, so a lit vehicle is seen farther at night (SeenAtNightM) and a
//     blacked-out column is found less often by the enemy's armed reconnaissance (BlackoutFindFactor).
//   * The cabin's climate (C3): the outside air, the racks' heat, the ventilation's ЛЕТО and ЗИМА, the heater; the
//     racks overheat when the air does not go through them, the antenna unit's transmitters when it is very hot and they
//     radiate long (ПЕРЕГРЕВ, then a fault); the gyro wants its heater below +5 C; the equipment warms up slower in deep
//     cold, and after damp air and a frost it stays on its filaments 15-20 min; the crew works slower out of the comfort
//     band (WorkSlowFactor).
//   * Fire (C8): a fire in the engine or the turbine compartment (a hit, Ignite), the «Роса» system's sensors, its stop
//     of the engine and the turbine, a bottle fired after its delay, the driver's button and the disc he turns on to the
//     next full bottle; a fire left burning takes the vehicle.
//
// What it sets is inputs on the console's equipment (so lamps, subsystems and the drill master read them) and readings
// for the dash; its events go to the debrief. Nothing here acts for a crewman: the runner's NPC driver works the
// controls through NpcDoes.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/json.hpp"

namespace simcore {

struct VehicleEngineDef {
    std::string subsystem, network_subsystem, preheater_subsystem;
    std::string starter_control, stop_control, hand_throttle_control;
    std::string cold_input, running_input;
    double idle_rpm = 750.0, rated_rpm = 2600.0, max_rpm = 2800.0;
    double working_c = 90.0, max_c = 120.0;
    double warm_tau_s = 480.0, cool_tau_s = 5400.0;
    double preheat_below_c = 5.0, preheat_rate_c_per_s = 0.05;
    double oil_idle = 2.0, oil_rated = 6.0, cold_oil_factor = 1.4;
    double start_current_a = 600.0;
};

struct VehicleDriveDef {
    std::string gear_control, handbrake_control;
    int handbrake_on_position = 1, neutral_position = 0, reverse_position = -1;
    std::vector<double> gear_max_mps;  // per gear lever position
    double road_mps = 60.0 / 3.6, afloat_mps = 8.0 / 3.6, blackout_night_mps = 25.0 / 3.6;
    std::vector<std::string> waterjet_controls;
    int waterjet_open_position = 1;
    std::string wave_shield_control;
    int wave_shield_up_position = 1;
};

struct VehicleElectricsDef {
    struct VoltageControl {
        std::string subsystem, control;
    };
    std::string battery_switch_control, network_subsystem, flat_input;
    double battery_v = 24.0, battery_ah = 70.0, charged_v = 25.4, flat_v = 23.0, charge_v = 27.5;
    double min_start_fraction = 0.25, network_load_a = 15.0, charge_a = 20.0;
    double supply_nominal_v = 220.0, supply_low_v = 218.0, supply_high_v = 230.0, regulator_span_v = 24.0;
    std::vector<VoltageControl> voltage_controls;
    double frequency_hz = 400.0, shed_hz = 360.0, gom_full_rpm = 1700.0;
    std::string ukch_control;
    int ukch_on_position = 1;
    std::string frequency_input, voltage_input;
};

struct VehicleLightsDef {
    std::string network_subsystem, central_control, outside_control, sidelights_control, front_lamp_control,
        rear_lamp_control, inside_control, instrument_control, lit_input;
    int headlights_position = 2, sidelights_position = 1;
    double blackout_seen_m = 150.0, headlights_seen_m = 6000.0, blackout_find_factor = 0.25;
};

struct VehicleClimateDef {
    std::string season_control, equipment_control, power_subsystem, heater_control, heat_vent_control,
        workplace_fan_control, gyro_heat_control;
    int summer_position = 1, cover_summer_position = 1, hatch_open_position = 1, heat_position = 1;
    std::vector<std::string> cover_controls, hatch_controls;
    double winter_below_c = -10.0, summer_cabin_above_c = 10.0;
    double rack_rise_summer_c = 15.0, rack_rise_still_c = 45.0, cabin_rise_winter_c = 25.0, heater_rise_c = 20.0,
           crew_rise_c = 4.0;
    double cabin_tau_s = 900.0, rack_tau_s = 600.0;
    double rack_fault_c = 75.0, rack_fault_after_s = 180.0;
    std::string rack_fault_input;
    double transmitter_rise_c = 35.0, transmitter_tau_s = 1200.0, transmitter_overheat_c = 85.0,
           transmitter_fault_after_s = 300.0;
    std::vector<std::string> transmitter_subsystems;
    std::string overheat_input, transmitter_fault_input, cold_input, frost_input;
    double cold_below_c = 5.0, slow_warm_below_c = 0.0, slow_warm_factor_at_minus_40 = 2.0;
    std::vector<std::string> slow_warm_subsystems;
    double frost_humidity_pct = 80.0, frost_filament_s = 900.0;
    double comfort_low_c = 12.0, comfort_high_c = 30.0, comfort_slow_per_c = 0.02, comfort_slow_max = 1.5,
           workplace_fan_relief_c = 3.0;
    // The magnetrons' drift with their temperature (voenizdat_osa_1991 p33: mainly the cavities' temperature; the rate
    // is SAM3D's estimate): Hz per degree away from the reference, felt only with the frequency control on manual.
    double magnetron_drift_hz_per_c = 0.0, magnetron_reference_c = 40.0;
};

struct VehicleFireDef {
    struct Zone {
        std::string id, input, name;
    };
    std::vector<Zone> zones;
    int bottles = 3;
    double auto_delay_s = 5.0, put_out_chance = 0.8, burn_to_destroyed_s = 120.0, hit_fire_chance = 0.15;
    std::string disc_control, cover_control, button_control, check_control, bottles_input;
};

struct VehicleSystemsDef {
    std::string system_id;
    bool has_engine = false, has_drive = false, has_electrics = false, has_lights = false, has_climate = false,
         has_fire = false;
    VehicleEngineDef engine;
    VehicleDriveDef drive;
    VehicleElectricsDef electrics;
    VehicleLightsDef lights;
    VehicleClimateDef climate;
    VehicleFireDef fire;
};

namespace detail {

// A number, or a sourced number {"value": ..., "source": ..., "confidence": ...}; `fallback` when absent.
inline double VsNum(const json::Value& o, const char* key, double fallback) {
    const json::Value* v = o.Find(key);
    if (v == nullptr) return fallback;
    if (v->Is(json::Type::Number)) return v->number;
    const json::Value* n = v->Find("value");
    return n != nullptr && n->Is(json::Type::Number) ? n->number : fallback;
}
inline int VsInt(const json::Value& o, const char* key, int fallback) {
    return static_cast<int>(std::lround(VsNum(o, key, static_cast<double>(fallback))));
}
inline std::string VsStr(const json::Value& o, const char* key) {
    const json::Value* v = o.Find(key);
    return v != nullptr && v->Is(json::Type::String) ? v->string : std::string();
}
inline std::vector<std::string> VsStrs(const json::Value& o, const char* key) {
    std::vector<std::string> out;
    const json::Value* v = o.Find(key);
    if (v == nullptr || !v->Is(json::Type::Array)) return out;
    for (const json::Value& s : v->items) {
        if (s.Is(json::Type::String)) out.push_back(s.string);
    }
    return out;
}
inline std::vector<double> VsNums(const json::Value& o, const char* key) {
    std::vector<double> out;
    const json::Value* v = o.Find(key);
    if (v != nullptr && v->Is(json::Type::Object)) v = v->Find("value");
    if (v == nullptr || !v->Is(json::Type::Array)) return out;
    for (const json::Value& n : v->items) {
        if (n.Is(json::Type::Number)) out.push_back(n.number);
    }
    return out;
}

}  // namespace detail

// Loads the file's blocks; a block absent from the file leaves its part off. Controls, inputs and subsystems are checked
// against the console when the systems are bound (VehicleSystems::Bind), not here.
inline bool LoadVehicleSystems(const std::string& json_text, VehicleSystemsDef& out, std::string* error = nullptr) {
    using detail::VsNum;
    using detail::VsInt;
    using detail::VsStr;
    using detail::VsStrs;
    json::Value root;
    std::string parse_error;
    if (!json::Parse(json_text, root, &parse_error)) {
        if (error != nullptr) *error = parse_error;
        return false;
    }
    if (!root.Is(json::Type::Object)) {
        if (error != nullptr) *error = "the vehicle systems file is not an object";
        return false;
    }
    VehicleSystemsDef d;
    d.system_id = VsStr(root, "system_id");
    if (const json::Value* e = root.Find("engine")) {
        d.has_engine = true;
        VehicleEngineDef& g = d.engine;
        g.subsystem = VsStr(*e, "subsystem");
        g.network_subsystem = VsStr(*e, "network_subsystem");
        g.preheater_subsystem = VsStr(*e, "preheater_subsystem");
        g.starter_control = VsStr(*e, "starter_control");
        g.stop_control = VsStr(*e, "stop_control");
        g.hand_throttle_control = VsStr(*e, "hand_throttle_control");
        g.cold_input = VsStr(*e, "cold_input");
        g.running_input = VsStr(*e, "running_input");
        g.idle_rpm = VsNum(*e, "idle_rpm", g.idle_rpm);
        g.rated_rpm = VsNum(*e, "rated_rpm", g.rated_rpm);
        g.max_rpm = VsNum(*e, "max_rpm", g.max_rpm);
        g.working_c = VsNum(*e, "working_temperature_c", g.working_c);
        g.max_c = VsNum(*e, "max_temperature_c", g.max_c);
        g.warm_tau_s = VsNum(*e, "warm_up_time_constant_s", g.warm_tau_s);
        g.cool_tau_s = VsNum(*e, "cool_down_time_constant_s", g.cool_tau_s);
        g.preheat_below_c = VsNum(*e, "preheat_below_c", g.preheat_below_c);
        g.preheat_rate_c_per_s = VsNum(*e, "preheat_rate_c_per_s", g.preheat_rate_c_per_s);
        g.oil_idle = VsNum(*e, "oil_pressure_idle_kgf_cm2", g.oil_idle);
        g.oil_rated = VsNum(*e, "oil_pressure_rated_kgf_cm2", g.oil_rated);
        g.cold_oil_factor = VsNum(*e, "cold_oil_pressure_factor", g.cold_oil_factor);
        g.start_current_a = VsNum(*e, "start_current_a", g.start_current_a);
        if (g.subsystem.empty() || g.rated_rpm <= g.idle_rpm || g.warm_tau_s <= 0.0 || g.cool_tau_s <= 0.0) {
            if (error != nullptr) *error = "engine: a subsystem, rated_rpm above idle_rpm and positive time constants are needed";
            return false;
        }
    }
    if (const json::Value* v = root.Find("drive")) {
        d.has_drive = true;
        VehicleDriveDef& g = d.drive;
        g.gear_control = VsStr(*v, "gear_control");
        g.handbrake_control = VsStr(*v, "handbrake_control");
        g.handbrake_on_position = VsInt(*v, "handbrake_on_position", g.handbrake_on_position);
        g.neutral_position = VsInt(*v, "neutral_position", g.neutral_position);
        g.reverse_position = VsInt(*v, "reverse_position", g.reverse_position);
        for (double kmh : detail::VsNums(*v, "gear_max_kmh")) g.gear_max_mps.push_back(kmh / 3.6);
        g.road_mps = VsNum(*v, "road_kmh", 60.0) / 3.6;
        g.afloat_mps = VsNum(*v, "afloat_kmh", 8.0) / 3.6;
        g.blackout_night_mps = VsNum(*v, "blackout_night_kmh", 25.0) / 3.6;
        g.waterjet_controls = VsStrs(*v, "waterjet_controls");
        g.waterjet_open_position = VsInt(*v, "waterjet_open_position", g.waterjet_open_position);
        g.wave_shield_control = VsStr(*v, "wave_shield_control");
        g.wave_shield_up_position = VsInt(*v, "wave_shield_up_position", g.wave_shield_up_position);
    }
    if (const json::Value* v = root.Find("electrics")) {
        d.has_electrics = true;
        VehicleElectricsDef& g = d.electrics;
        g.battery_switch_control = VsStr(*v, "battery_switch_control");
        g.network_subsystem = VsStr(*v, "network_subsystem");
        g.flat_input = VsStr(*v, "flat_input");
        g.battery_v = VsNum(*v, "battery_v", g.battery_v);
        g.battery_ah = VsNum(*v, "battery_ah", g.battery_ah);
        g.charged_v = VsNum(*v, "charged_v", g.charged_v);
        g.flat_v = VsNum(*v, "flat_v", g.flat_v);
        g.charge_v = VsNum(*v, "charge_v", g.charge_v);
        g.min_start_fraction = VsNum(*v, "min_start_fraction", g.min_start_fraction);
        g.network_load_a = VsNum(*v, "network_load_a", g.network_load_a);
        g.charge_a = VsNum(*v, "charge_a", g.charge_a);
        g.supply_nominal_v = VsNum(*v, "supply_nominal_v", g.supply_nominal_v);
        g.supply_low_v = VsNum(*v, "supply_low_v", g.supply_low_v);
        g.supply_high_v = VsNum(*v, "supply_high_v", g.supply_high_v);
        g.regulator_span_v = VsNum(*v, "regulator_span_v", g.regulator_span_v);
        if (const json::Value* vc = v->Find("voltage_controls")) {
            for (const json::Value& c : vc->items) g.voltage_controls.push_back({VsStr(c, "subsystem"), VsStr(c, "control")});
        }
        g.frequency_hz = VsNum(*v, "frequency_hz", g.frequency_hz);
        g.shed_hz = VsNum(*v, "shed_hz", g.shed_hz);
        g.gom_full_rpm = VsNum(*v, "gom_full_rpm", g.gom_full_rpm);
        g.ukch_control = VsStr(*v, "ukch_control");
        g.ukch_on_position = VsInt(*v, "ukch_on_position", g.ukch_on_position);
        g.frequency_input = VsStr(*v, "frequency_input");
        g.voltage_input = VsStr(*v, "voltage_input");
        if (g.battery_ah <= 0.0 || g.charged_v <= g.flat_v) {
            if (error != nullptr) *error = "electrics: battery_ah > 0 and charged_v above flat_v are needed";
            return false;
        }
    }
    if (const json::Value* v = root.Find("lights")) {
        d.has_lights = true;
        VehicleLightsDef& g = d.lights;
        g.network_subsystem = VsStr(*v, "network_subsystem");
        g.central_control = VsStr(*v, "central_control");
        g.outside_control = VsStr(*v, "outside_control");
        g.sidelights_control = VsStr(*v, "sidelights_control");
        g.front_lamp_control = VsStr(*v, "front_lamp_control");
        g.rear_lamp_control = VsStr(*v, "rear_lamp_control");
        g.inside_control = VsStr(*v, "inside_control");
        g.instrument_control = VsStr(*v, "instrument_control");
        g.lit_input = VsStr(*v, "lit_input");
        g.headlights_position = VsInt(*v, "headlights_position", g.headlights_position);
        g.sidelights_position = VsInt(*v, "sidelights_position", g.sidelights_position);
        g.blackout_seen_m = VsNum(*v, "blackout_seen_m", g.blackout_seen_m);
        g.headlights_seen_m = VsNum(*v, "headlights_seen_m", g.headlights_seen_m);
        g.blackout_find_factor = VsNum(*v, "blackout_find_factor", g.blackout_find_factor);
    }
    if (const json::Value* v = root.Find("climate")) {
        d.has_climate = true;
        VehicleClimateDef& g = d.climate;
        g.season_control = VsStr(*v, "season_control");
        g.summer_position = VsInt(*v, "summer_position", g.summer_position);
        g.cover_controls = VsStrs(*v, "cover_controls");
        g.cover_summer_position = VsInt(*v, "cover_summer_position", g.cover_summer_position);
        g.hatch_controls = VsStrs(*v, "hatch_controls");
        g.hatch_open_position = VsInt(*v, "hatch_open_position", g.hatch_open_position);
        g.equipment_control = VsStr(*v, "equipment_control");
        g.power_subsystem = VsStr(*v, "power_subsystem");
        g.heater_control = VsStr(*v, "heater_control");
        g.heat_vent_control = VsStr(*v, "heat_vent_control");
        g.heat_position = VsInt(*v, "heat_position", g.heat_position);
        g.workplace_fan_control = VsStr(*v, "workplace_fan_control");
        g.gyro_heat_control = VsStr(*v, "gyro_heat_control");
        g.winter_below_c = VsNum(*v, "winter_below_c", g.winter_below_c);
        g.summer_cabin_above_c = VsNum(*v, "summer_cabin_above_c", g.summer_cabin_above_c);
        g.rack_rise_summer_c = VsNum(*v, "rack_rise_summer_c", g.rack_rise_summer_c);
        g.rack_rise_still_c = VsNum(*v, "rack_rise_still_c", g.rack_rise_still_c);
        g.cabin_rise_winter_c = VsNum(*v, "cabin_rise_winter_c", g.cabin_rise_winter_c);
        g.heater_rise_c = VsNum(*v, "heater_rise_c", g.heater_rise_c);
        g.crew_rise_c = VsNum(*v, "crew_rise_c", g.crew_rise_c);
        g.cabin_tau_s = VsNum(*v, "cabin_time_constant_s", g.cabin_tau_s);
        g.rack_tau_s = VsNum(*v, "rack_time_constant_s", g.rack_tau_s);
        g.rack_fault_c = VsNum(*v, "rack_fault_c", g.rack_fault_c);
        g.rack_fault_after_s = VsNum(*v, "rack_fault_after_s", g.rack_fault_after_s);
        g.rack_fault_input = VsStr(*v, "rack_fault_input");
        g.transmitter_rise_c = VsNum(*v, "transmitter_rise_c", g.transmitter_rise_c);
        g.transmitter_tau_s = VsNum(*v, "transmitter_time_constant_s", g.transmitter_tau_s);
        g.transmitter_overheat_c = VsNum(*v, "transmitter_overheat_c", g.transmitter_overheat_c);
        g.transmitter_fault_after_s = VsNum(*v, "transmitter_fault_after_s", g.transmitter_fault_after_s);
        g.transmitter_subsystems = VsStrs(*v, "transmitter_subsystems");
        g.overheat_input = VsStr(*v, "overheat_input");
        g.transmitter_fault_input = VsStr(*v, "transmitter_fault_input");
        g.cold_input = VsStr(*v, "cold_input");
        g.cold_below_c = VsNum(*v, "cold_below_c", g.cold_below_c);
        g.slow_warm_below_c = VsNum(*v, "slow_warm_below_c", g.slow_warm_below_c);
        g.slow_warm_factor_at_minus_40 = VsNum(*v, "slow_warm_factor_at_minus_40", g.slow_warm_factor_at_minus_40);
        g.slow_warm_subsystems = VsStrs(*v, "slow_warm_subsystems");
        g.frost_humidity_pct = VsNum(*v, "frost_humidity_pct", g.frost_humidity_pct);
        g.frost_filament_s = VsNum(*v, "frost_filament_s", g.frost_filament_s);
        g.frost_input = VsStr(*v, "frost_input");
        g.magnetron_drift_hz_per_c = VsNum(*v, "magnetron_drift_hz_per_c", g.magnetron_drift_hz_per_c);
        g.magnetron_reference_c = VsNum(*v, "magnetron_reference_c", g.magnetron_reference_c);
        g.comfort_low_c = VsNum(*v, "comfort_low_c", g.comfort_low_c);
        g.comfort_high_c = VsNum(*v, "comfort_high_c", g.comfort_high_c);
        g.comfort_slow_per_c = VsNum(*v, "comfort_slow_per_c", g.comfort_slow_per_c);
        g.comfort_slow_max = VsNum(*v, "comfort_slow_max", g.comfort_slow_max);
        g.workplace_fan_relief_c = VsNum(*v, "workplace_fan_relief_c", g.workplace_fan_relief_c);
        if (g.cabin_tau_s <= 0.0 || g.rack_tau_s <= 0.0 || g.transmitter_tau_s <= 0.0) {
            if (error != nullptr) *error = "climate: the time constants must be positive";
            return false;
        }
    }
    if (const json::Value* v = root.Find("fire")) {
        d.has_fire = true;
        VehicleFireDef& g = d.fire;
        if (const json::Value* zs = v->Find("zones")) {
            for (const json::Value& z : zs->items) g.zones.push_back({VsStr(z, "id"), VsStr(z, "input"), VsStr(z, "name")});
        }
        g.bottles = VsInt(*v, "bottles", g.bottles);
        g.auto_delay_s = VsNum(*v, "auto_delay_s", g.auto_delay_s);
        g.put_out_chance = VsNum(*v, "put_out_chance", g.put_out_chance);
        g.burn_to_destroyed_s = VsNum(*v, "burn_to_destroyed_s", g.burn_to_destroyed_s);
        g.hit_fire_chance = VsNum(*v, "hit_fire_chance", g.hit_fire_chance);
        g.disc_control = VsStr(*v, "disc_control");
        g.cover_control = VsStr(*v, "cover_control");
        g.button_control = VsStr(*v, "button_control");
        g.check_control = VsStr(*v, "check_control");
        g.bottles_input = VsStr(*v, "bottles_input");
        if (g.bottles < 1 || g.zones.empty()) {
            if (error != nullptr) *error = "fire: at least one bottle and one zone";
            return false;
        }
    }
    out = d;
    return true;
}

inline bool LoadVehicleSystemsFile(const std::string& path, VehicleSystemsDef& out, std::string* error = nullptr) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    if (!LoadVehicleSystems(text.str(), out, error)) {
        if (error != nullptr) *error = path + ": " + *error;
        return false;
    }
    return true;
}

// Relative humidity (per cent) from the air's temperature and dewpoint (Magnus, over water).
inline double RelativeHumidityPct(double t_c, double dewpoint_c) {
    const double e = std::exp(17.625 * dewpoint_c / (243.04 + dewpoint_c));
    const double es = std::exp(17.625 * t_c / (243.04 + t_c));
    return std::min(100.0, 100.0 * e / es);
}

// What the driver's dash and the cabinet's voltmeter read (the scope feed carries it, so a network client draws the
// server's values). Not valid: no vehicle model, the gauges at rest.
struct VehicleDash {
    bool valid = false;
    double engine_rpm = 0.0;
    double speed_kmh = 0.0;
    double coolant_c = 0.0;
    double oil_c = 0.0;
    double oil_pressure = 0.0;  // kgf/cm2
    double ammeter_a = 0.0;     // charge positive
    double network_v = 0.0;     // the 24 V network
    double supply_v = 0.0;      // the 220 V 400 Hz supply on line (0: none)
};

// Which of the vehicle's lamps are lit, read from the console alone (its controls and its network's subsystem), so a
// client that mirrors the console draws the hull's lamps without stepping the systems (roadmap C2).
struct OutsideLamps {
    bool headlights = false, sidelights = false, front = false, rear = false, inside = false, instruments = false;
    bool Any() const { return headlights || sidelights || front || rear; }
};

inline OutsideLamps LampsOf(const VehicleLightsDef& l, const ConsoleData& c, bool power) {
    OutsideLamps o;
    const bool outside = power && (l.outside_control.empty() || c.panel.Get(l.outside_control) == 1);
    const int central = l.central_control.empty() ? 0 : c.panel.Get(l.central_control);
    o.headlights = outside && !l.central_control.empty() && central == l.headlights_position;
    o.sidelights = outside && ((!l.central_control.empty() && central >= l.sidelights_position) ||
                               (!l.sidelights_control.empty() && c.panel.Get(l.sidelights_control) == 1));
    o.front = outside && !l.front_lamp_control.empty() && c.panel.Get(l.front_lamp_control) == 1;
    o.rear = outside && (o.sidelights || (!l.rear_lamp_control.empty() && c.panel.Get(l.rear_lamp_control) == 1));
    o.inside = power && !l.inside_control.empty() && c.panel.Get(l.inside_control) == 1;
    o.instruments = power && !l.instrument_control.empty() && c.panel.Get(l.instrument_control) == 1;
    return o;
}

// The same from the whole definition: the lamps have power while the battery switch is on and the network's subsystem
// is ready (with no electrics part, always).
inline OutsideLamps OutsideLampsOf(const VehicleSystemsDef& d, const ConsoleData& c) {
    if (!d.has_lights || d.lights.central_control.empty() || !c.panel.Has(d.lights.central_control)) return OutsideLamps();
    bool power = true;
    if (d.has_electrics) {
        const VehicleElectricsDef& g = d.electrics;
        power = c.panel.Get(g.battery_switch_control) == 1 &&
                (g.network_subsystem.empty() || c.equipment.IsReady(g.network_subsystem));
    }
    return LampsOf(d.lights, c, power);
}

class VehicleSystems {
public:
    // What the world gives the vehicle this step.
    struct World {
        double outside_c = 15.0;
        double dewpoint_c = 5.0;
        double speed_mps = 0.0;  // its speed now
        bool afloat = false;
        bool dark = false;       // the battery's dark (light.hpp)
        double fuel_l = -1.0;    // in the tank (< 0: not modelled)
    };
    struct Event {
        std::string who;   // the seat or system it is about
        std::string what;  // a short subject
        std::string text;
    };

    // Takes the definition and checks it against the console: a part whose controls or subsystems the console lacks is
    // left off. `seed` drives the fire's chances.
    void Bind(const VehicleSystemsDef& def, const ConsoleData& console, std::uint32_t seed = 1u) {
        def_ = def;
        rng_ = seed == 0u ? 1u : seed;
        const ControlPanel& p = console.panel;
        const Equipment& eq = console.equipment;
        engine_on_ = def_.has_engine && eq.Has(def_.engine.subsystem);
        drive_on_ = def_.has_drive && p.Has(def_.drive.gear_control) && p.Has(def_.drive.handbrake_control);
        electrics_on_ = def_.has_electrics && p.Has(def_.electrics.battery_switch_control);
        lights_on_ = def_.has_lights && p.Has(def_.lights.central_control);
        climate_on_ = def_.has_climate && p.Has(def_.climate.season_control);
        fire_on_ = def_.has_fire && p.Has(def_.fire.button_control) && p.Has(def_.fire.disc_control);
        bottle_full_.assign(static_cast<std::size_t>(std::max(0, def_.fire.bottles)), true);
        burning_.assign(def_.fire.zones.size(), Burn());
    }
    bool Active() const { return engine_on_ || electrics_on_ || lights_on_ || climate_on_ || fire_on_; }
    const VehicleSystemsDef& Def() const { return def_; }

    // The session's start. `engine_warm`: the engine has been running (a readiness state with it ready);
    // `equipment_running`: the equipment has been on long enough to reach its temperatures. The crew has set the
    // ventilation for the season and the gyro's heater for the cabin (mon_2112_80 p68, voenizdat_osa_1991 p51), and the
    // inputs are set before the readiness state warms the subsystems.
    void Start(ConsoleData& c, const World& w, bool engine_warm, bool equipment_running) {
        outside_c_ = w.outside_c;
        coolant_c_ = engine_warm ? def_.engine.working_c : w.outside_c;
        oil_c_ = coolant_c_;
        charge_ = 1.0;
        frost_damp_ = false;
        if (climate_on_) {
            const VehicleClimateDef& k = def_.climate;
            const bool winter = w.outside_c < k.winter_below_c;
            const int season = winter ? 1 - k.summer_position : k.summer_position;
            c.panel.Set(k.season_control, season);
            for (const std::string& id : k.cover_controls) c.panel.Set(id, winter ? 1 - k.cover_summer_position : k.cover_summer_position);
            // Summer: the outside intakes opened by hand when the equipment is on (mon_2112_80 p68); winter: they stay shut.
            for (const std::string& id : k.hatch_controls) {
                c.panel.Set(id, !winter && equipment_running ? k.hatch_open_position : 1 - k.hatch_open_position);
            }
            cabin_c_ = CabinTarget(c, equipment_running);
            rack_c_ = RackTarget(c, equipment_running);
            transmitter_c_ = w.outside_c;
            if (!k.gyro_heat_control.empty() && cabin_c_ < k.cold_below_c) c.panel.Set(k.gyro_heat_control, 1);
        }
        equipment_was_on_ = equipment_running;
        SetInputs(c);
    }

    // One step.
    void Step(double dt, ConsoleData& c, const World& w, double t) {
        if (dt <= 0.0) return;
        outside_c_ = w.outside_c;
        StepEngine(dt, c, w);
        StepElectrics(dt, c);
        StepLights(c, w);
        StepClimate(dt, c, w, t);
        StepFire(dt, c, t);
        SetInputs(c);
    }

    std::vector<Event> TakeEvents() {
        std::vector<Event> out;
        out.swap(events_);
        return out;
    }

    // ---- The engine and the drive (C1)
    bool EngineRunning() const { return running_; }
    double Rpm() const { return rpm_; }
    double CoolantC() const { return coolant_c_; }
    double OilC() const { return oil_c_; }
    double OilPressureKgfCm2() const { return oil_kgf_; }
    double EngineHours() const { return engine_s_ / 3600.0; }
    // The fastest the vehicle can go now, by its gear, brake, engine, flaps and lights (m/s).
    double SpeedCapMps(const ConsoleData& c, const World& w) const {
        if (!drive_on_) return -1.0;
        const VehicleDriveDef& d = def_.drive;
        if (destroyed_) return 0.0;
        if (engine_on_ && !running_) return 0.0;
        if (c.panel.Get(d.handbrake_control) == d.handbrake_on_position) return 0.0;
        const int gear = c.panel.Get(d.gear_control);
        if (gear == d.neutral_position || gear == d.reverse_position || gear < 0) return 0.0;
        double cap = gear < static_cast<int>(d.gear_max_mps.size()) ? d.gear_max_mps[static_cast<std::size_t>(gear)] : d.road_mps;
        cap = std::min(cap, d.road_mps);
        if (w.afloat) {
            // Afloat the water jets drive her: both flaps open and the wave shield up (voenizdat_osa_1991 p175, p224).
            bool open = true;
            for (const std::string& id : d.waterjet_controls) open = open && c.panel.Get(id) == d.waterjet_open_position;
            const bool shield = d.wave_shield_control.empty() || c.panel.Get(d.wave_shield_control) == d.wave_shield_up_position;
            return open && shield ? d.afloat_mps : 0.0;
        }
        if (w.dark && lights_on_ && !headlights_) cap = std::min(cap, d.blackout_night_mps);
        return cap;
    }
    // The lowest gear whose speed reaches `mps` (the top gear past them all); the neutral position for 0.
    int GearFor(double mps) const {
        const VehicleDriveDef& d = def_.drive;
        if (mps <= 0.0) return d.neutral_position;
        int best = -1;
        for (int g = 0; g < static_cast<int>(d.gear_max_mps.size()); ++g) {
            if (g == d.neutral_position || g == d.reverse_position) continue;
            if (d.gear_max_mps[static_cast<std::size_t>(g)] + 1e-9 >= mps) return g;
            if (best < 0 || d.gear_max_mps[static_cast<std::size_t>(g)] > d.gear_max_mps[static_cast<std::size_t>(best)]) best = g;
        }
        return best < 0 ? d.neutral_position : best;
    }

    // ---- The network and the supply (C7)
    double NetworkV() const { return network_v_; }
    double BatteryCharge() const { return charge_; }
    double AmmeterA() const { return ammeter_a_; }
    bool NetworkOn() const { return network_ready_; }
    double SupplyV() const { return supply_v_; }
    double SupplyHz() const { return supply_hz_; }
    bool Shedding() const { return shed_; }
    // The R-123M's КОНТРОЛЬ НАПРЯЖЕНИЙ at БС: the network's voltage, in the meter's shaded sector at 23-28 V
    // (bsuir_r123m_2019).
    bool RadioSupplyInNorm() const { return network_v_ >= 23.0 && network_v_ <= 28.0; }

    // The magnetrons' drift off their tune now (Hz): their temperature away from the reference.
    double MagnetronDriftHz() const {
        if (!climate_on_) return 0.0;
        return def_.climate.magnetron_drift_hz_per_c * (transmitter_c_ - def_.climate.magnetron_reference_c);
    }
    // The dash's readings at this speed over the ground.
    VehicleDash Dash(double speed_mps) const {
        VehicleDash d;
        d.valid = true;
        d.engine_rpm = rpm_;
        d.speed_kmh = std::max(0.0, speed_mps) * 3.6;
        d.coolant_c = coolant_c_;
        d.oil_c = oil_c_;
        d.oil_pressure = oil_kgf_;
        d.ammeter_a = ammeter_a_;
        d.network_v = network_v_;
        d.supply_v = supply_v_;
        return d;
    }

    // ---- The lights (C2)
    bool Headlights() const { return headlights_; }
    bool Sidelights() const { return sidelights_; }
    bool FrontLamp() const { return front_lamp_; }
    bool RearLamps() const { return rear_lamps_; }
    bool InsideLights() const { return inside_; }
    bool InstrumentLights() const { return instruments_; }
    bool OutsideLit() const { return headlights_ || sidelights_ || front_lamp_ || rear_lamps_; }
    // How far she is seen at night: by her headlights, or only her masked lamps under blackout.
    double SeenAtNightM() const {
        if (!lights_on_) return -1.0;
        return headlights_ ? def_.lights.headlights_seen_m : def_.lights.blackout_seen_m;
    }
    // The enemy's chance to find her on a night road, against one by its headlights.
    double NightFindFactor() const { return lights_on_ && !OutsideLit() ? def_.lights.blackout_find_factor : 1.0; }

    // ---- The climate (C3)
    double CabinC() const { return cabin_c_; }
    double RackC() const { return rack_c_; }
    double TransmitterC() const { return transmitter_c_; }
    bool Overheat() const { return overheat_; }
    bool CabinCold() const { return climate_on_ && cabin_c_ < def_.climate.cold_below_c; }
    bool FrostDamp() const { return frost_damp_; }
    // How much slower the crew works in this cabin (1 within the comfort band).
    double WorkSlowFactor(const ConsoleData& c) const {
        if (!climate_on_) return 1.0;
        const VehicleClimateDef& k = def_.climate;
        double high = k.comfort_high_c;
        if (!k.workplace_fan_control.empty() && c.panel.Get(k.workplace_fan_control) == 1) high += k.workplace_fan_relief_c;
        const double beyond = cabin_c_ < k.comfort_low_c ? k.comfort_low_c - cabin_c_ : (cabin_c_ > high ? cabin_c_ - high : 0.0);
        return std::min(k.comfort_slow_max, 1.0 + k.comfort_slow_per_c * beyond);
    }

    // ---- Fire (C8)
    // A fire in a compartment (a hit): `zone` one of the data's zones. False if there is no such zone or it burns already.
    bool Ignite(const std::string& zone, double t) {
        for (std::size_t i = 0; i < def_.fire.zones.size(); ++i) {
            if (def_.fire.zones[i].id != zone || burning_[i].on) continue;
            burning_[i] = Burn{true, t, t + def_.fire.auto_delay_s, false};
            events_.push_back({"Driver", "fire", "fire in the " + def_.fire.zones[i].name + ": the «Роса» lamps light"});
            return true;
        }
        return false;
    }
    // A hit on the vehicle: a fire starts by the data's chance in one of the zones. Returns the zone set burning, or "".
    std::string Hit(double t) {
        if (!fire_on_ || def_.fire.zones.empty() || destroyed_) return std::string();
        if (Uniform() >= def_.fire.hit_fire_chance) return std::string();
        const std::size_t i = std::min(def_.fire.zones.size() - 1, static_cast<std::size_t>(Uniform() * static_cast<double>(def_.fire.zones.size())));
        return Ignite(def_.fire.zones[i].id, t) ? def_.fire.zones[i].id : std::string();
    }
    bool Burning(const std::string& zone) const {
        for (std::size_t i = 0; i < def_.fire.zones.size(); ++i) {
            if (def_.fire.zones[i].id == zone) return burning_[i].on;
        }
        return false;
    }
    bool AnyBurning() const {
        for (const Burn& b : burning_) {
            if (b.on) return true;
        }
        return false;
    }
    int BottlesLeft() const {
        int n = 0;
        for (bool f : bottle_full_) n += f ? 1 : 0;
        return n;
    }
    // The bottle the disc points at (0-based) and whether it is full: the disc reads the bottles left, 3, 2, 1, and the
    // next shot goes to the bottle that count leaves (3 left: the first).
    int DiscBottle(const ConsoleData& c) const {
        const int shown = def_.fire.bottles - std::max(0, c.panel.Get(def_.fire.disc_control));
        return std::min(def_.fire.bottles - 1, std::max(0, def_.fire.bottles - shown));
    }
    bool DiscOnFull(const ConsoleData& c) const {
        const int b = DiscBottle(c);
        return b >= 0 && b < static_cast<int>(bottle_full_.size()) && bottle_full_[static_cast<std::size_t>(b)];
    }
    // The disc position that points at the next full bottle (-1 when none is left).
    int DiscForNextFull() const {
        for (int b = 0; b < static_cast<int>(bottle_full_.size()); ++b) {
            if (bottle_full_[static_cast<std::size_t>(b)]) return b;
        }
        return -1;
    }
    bool Destroyed() const { return destroyed_; }

private:
    struct Burn {
        bool on = false;
        double since_s = 0.0;
        double auto_at_s = 0.0;
        bool auto_done = false;
    };

    double Uniform() {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return static_cast<double>(rng_ % 1000000u) / 1000000.0;
    }
    static double Toward(double from, double to, double dt, double tau) {
        return to + (from - to) * std::exp(-dt / std::max(1e-6, tau));
    }
    static bool Ready(const ConsoleData& c, const std::string& id) { return !id.empty() && c.equipment.IsReady(id); }
    static bool NotOff(const ConsoleData& c, const std::string& id) {
        return !id.empty() && c.equipment.State(id) != SubsystemState::Off;
    }

    // ---- Engine
    void StepEngine(double dt, ConsoleData& c, const World& w) {
        if (!engine_on_) return;
        const VehicleEngineDef& e = def_.engine;
        const SubsystemState s = c.equipment.State(e.subsystem);
        const bool cranking = s == SubsystemState::Starting;
        running_ = s == SubsystemState::Ready;
        cranking_ = cranking;
        double throttle = 0.5;
        if (!e.hand_throttle_control.empty() && c.panel.Has(e.hand_throttle_control)) throttle = c.panel.GetValue(e.hand_throttle_control);
        throttle = std::min(1.0, std::max(0.0, throttle));
        if (running_) {
            engine_s_ += dt;
            // On the road the engine turns as fast as the speed asks of its gear, at a halt as the hand throttle holds it.
            double target = e.idle_rpm + throttle * (e.max_rpm - e.idle_rpm);
            if (w.speed_mps > 0.1 && drive_on_) target = std::max(e.idle_rpm, std::min(e.rated_rpm, e.idle_rpm + (e.rated_rpm - e.idle_rpm) * 0.8));
            rpm_ = Toward(rpm_, target, dt, 2.0);
        } else {
            rpm_ = cranking ? 200.0 : Toward(rpm_, 0.0, dt, 1.0);
        }
        // Coolant and oil: toward the working temperature while it runs (the faster the harder it works), the air's when
        // stopped; the pre-heater warms them.
        const bool preheating = NotOff(c, e.preheater_subsystem);
        if (running_) {
            const double load = std::min(1.0, std::max(0.0, (rpm_ - e.idle_rpm) / (e.rated_rpm - e.idle_rpm)));
            coolant_c_ = Toward(coolant_c_, e.working_c, dt, e.warm_tau_s / (0.6 + 0.8 * load));
        } else {
            coolant_c_ = Toward(coolant_c_, w.outside_c, dt, e.cool_tau_s);
        }
        if (preheating && !running_) coolant_c_ = std::min(e.working_c, coolant_c_ + e.preheat_rate_c_per_s * dt);
        oil_c_ = Toward(oil_c_, coolant_c_, dt, 120.0);
        if (running_) {
            const double frac = std::min(1.0, std::max(0.0, (rpm_ - e.idle_rpm) / (e.rated_rpm - e.idle_rpm)));
            double p = e.oil_idle + (e.oil_rated - e.oil_idle) * frac;
            if (oil_c_ < 55.0) p *= 1.0 + (e.cold_oil_factor - 1.0) * std::min(1.0, (55.0 - oil_c_) / 55.0);
            oil_kgf_ = p;
        } else {
            oil_kgf_ = Toward(oil_kgf_, 0.0, dt, 1.0);
        }
    }

    // ---- Network and supply
    void StepElectrics(double dt, ConsoleData& c) {
        if (!electrics_on_) return;
        const VehicleElectricsDef& g = def_.electrics;
        const bool switch_on = c.panel.Get(g.battery_switch_control) == 1;
        network_ready_ = switch_on && (g.network_subsystem.empty() || Ready(c, g.network_subsystem));
        // A generator on line: the engine's own, or ВУ-1 from a running С-75 (voenizdat_osa_1991 p163), charges the batteries.
        const bool charging = switch_on && (running_ || Ready(c, "gta") || Ready(c, "pto_generator") || Ready(c, "external_supply"));
        double amps = 0.0;
        if (charging) {
            amps = g.charge_a * std::max(0.05, 1.0 - charge_);
            charge_ = std::min(1.0, charge_ + amps * dt / 3600.0 / g.battery_ah);
        } else if (switch_on) {
            amps = -(g.network_load_a + (cranking_ ? def_.engine.start_current_a : 0.0) + (InsideLights() || OutsideLit() ? 5.0 : 0.0));
            charge_ = std::max(0.0, charge_ + amps * dt / 3600.0 / g.battery_ah);
        }
        ammeter_a_ = amps;
        const double rest = g.flat_v + (g.charged_v - g.flat_v) * charge_;
        network_v_ = !switch_on ? 0.0 : charging ? g.charge_v : std::max(0.0, rest - (cranking_ ? 4.0 : 0.3));
        // The supply: the first source on line, its voltage as its НАПР. knob sets it, its frequency as it turns.
        supply_v_ = 0.0;
        supply_hz_ = 0.0;
        for (const VehicleElectricsDef::VoltageControl& vc : g.voltage_controls) {
            if (!Ready(c, vc.subsystem)) continue;
            const double knob = c.panel.Has(vc.control) ? c.panel.GetValue(vc.control) : 0.5;
            supply_v_ = g.supply_nominal_v + (knob - 0.5) * g.regulator_span_v;
            supply_hz_ = g.frequency_hz;
            if (vc.subsystem == "pto_generator" && engine_on_) supply_hz_ = g.frequency_hz * std::min(1.0, rpm_ / g.gom_full_rpm);
            break;
        }
        if (supply_hz_ <= 0.0 && Ready(c, "external_supply")) {
            supply_v_ = g.supply_nominal_v;
            supply_hz_ = g.frequency_hz;
        }
        const bool low_hz = supply_hz_ > 0.0 && supply_hz_ < g.shed_hz;
        const bool ukch = g.ukch_control.empty() || c.panel.Get(g.ukch_control) == g.ukch_on_position;
        // Shed only while the УКЧ is in; once shed the load stays off until the frequency is back (with a margin).
        if (ukch && low_hz) shed_ = true;
        if (shed_ && (!ukch || supply_hz_ <= 0.0 || supply_hz_ >= g.shed_hz + 10.0)) shed_ = false;
        low_hz_ = low_hz;
        voltage_off_ = supply_v_ > 0.0 && (supply_v_ < g.supply_low_v || supply_v_ > g.supply_high_v);
        const bool flat = charge_ < g.min_start_fraction * 0.2;
        if (flat && !flat_) events_.push_back({"Driver", "batteries", "the batteries are flat: the network is dead"});
        flat_ = flat;
    }

    // ---- Lights
    void StepLights(const ConsoleData& c, const World&) {
        if (!lights_on_) return;
        const OutsideLamps o = LampsOf(def_.lights, c, electrics_on_ ? network_ready_ : true);
        headlights_ = o.headlights;
        sidelights_ = o.sidelights;
        front_lamp_ = o.front;
        rear_lamps_ = o.rear;
        inside_ = o.inside;
        instruments_ = o.instruments;
    }

    // ---- Climate
    bool EquipmentHeating(const ConsoleData& c) const {
        const VehicleClimateDef& k = def_.climate;
        return c.panel.Get(k.equipment_control) == 1 && (k.power_subsystem.empty() || Ready(c, k.power_subsystem));
    }
    bool Summer(const ConsoleData& c) const { return c.panel.Get(def_.climate.season_control) == def_.climate.summer_position; }
    bool CoversAgree(const ConsoleData& c) const {
        const VehicleClimateDef& k = def_.climate;
        const int want = Summer(c) ? k.cover_summer_position : 1 - k.cover_summer_position;
        for (const std::string& id : k.cover_controls) {
            if (c.panel.Get(id) != want) return false;
        }
        return true;
    }
    bool HatchesOpen(const ConsoleData& c) const {
        for (const std::string& id : def_.climate.hatch_controls) {
            if (c.panel.Get(id) != def_.climate.hatch_open_position) return false;
        }
        return true;
    }
    bool HeaterOn(const ConsoleData& c) const {
        const VehicleClimateDef& k = def_.climate;
        if (k.heater_control.empty() || c.panel.Get(k.heater_control) != 1) return false;
        if (!k.heat_vent_control.empty() && c.panel.Get(k.heat_vent_control) != k.heat_position) return false;
        return electrics_on_ ? network_ready_ : true;
    }
    // Where the cabin's air goes with the ventilation as set: ЛЕТО (the racks' heat thrown out to the engine
    // compartment), ЗИМА (recirculated into the cabin), or half-set (some of it in).
    double CabinTarget(const ConsoleData& c, bool equipment) const {
        const VehicleClimateDef& k = def_.climate;
        double t = outside_c_ + k.crew_rise_c + (HeaterOn(c) ? k.heater_rise_c : 0.0);
        if (!equipment) return t;
        if (Summer(c) && CoversAgree(c)) return t;
        if (!Summer(c) && CoversAgree(c)) return t + k.cabin_rise_winter_c;
        return t + 0.5 * k.cabin_rise_winter_c;
    }
    double RackTarget(const ConsoleData& c, bool equipment) const {
        const VehicleClimateDef& k = def_.climate;
        if (!equipment) return cabin_c_;
        if (Summer(c) && CoversAgree(c) && HatchesOpen(c)) return outside_c_ + k.rack_rise_summer_c;
        if (!Summer(c) && CoversAgree(c)) return cabin_c_ + k.rack_rise_summer_c;
        return cabin_c_ + k.rack_rise_still_c;  // the separating fans draw against shut hatches, or the covers half set
    }
    void StepClimate(double dt, ConsoleData& c, const World& w, double t) {
        if (!climate_on_) return;
        const VehicleClimateDef& k = def_.climate;
        const bool equipment = EquipmentHeating(c);
        cabin_c_ = Toward(cabin_c_, CabinTarget(c, equipment), dt, k.cabin_tau_s);
        rack_c_ = Toward(rack_c_, RackTarget(c, equipment), dt, k.rack_tau_s);
        bool radiating = false;
        for (const std::string& id : k.transmitter_subsystems) radiating = radiating || Ready(c, id);
        transmitter_c_ = Toward(transmitter_c_, w.outside_c + (radiating ? k.transmitter_rise_c : (equipment ? 0.3 * k.transmitter_rise_c : 0.0)), dt, k.transmitter_tau_s);
        // The racks: past their limit long enough, a block fails (the computer, in the first form).
        if (rack_c_ > k.rack_fault_c) {
            rack_hot_s_ += dt;
            if (rack_hot_s_ >= k.rack_fault_after_s && !rack_failed_) {
                rack_failed_ = true;
                events_.push_back({"Operator", "ventilation", "the racks overheated: a block of the computer has failed"});
            }
        } else {
            rack_hot_s_ = 0.0;
        }
        // The transmitters: ПЕРЕГРЕВ, then a fault if they go on radiating (voenizdat_osa_1991 p169).
        const bool hot = transmitter_c_ > k.transmitter_overheat_c;
        if (hot && !overheat_) events_.push_back({"Deputy commander", "ПЕРЕГРЕВ", "ПЕРЕГРЕВ 0004-20M1: a transmitter compartment is overheating"});
        overheat_ = hot;
        if (hot && radiating) {
            hot_s_ += dt;
            if (hot_s_ >= k.transmitter_fault_after_s && !transmitter_failed_) {
                transmitter_failed_ = true;
                events_.push_back({"Deputy commander", "ПЕРЕГРЕВ", "a transmitter has failed from overheating"});
            }
        } else if (!hot) {
            hot_s_ = 0.0;
            if (transmitter_failed_ && transmitter_c_ < k.transmitter_overheat_c - 20.0 && c.panel.Get(k.equipment_control) != 1) {
                transmitter_failed_ = false;  // cooled and switched off: it works again
            }
        }
        // Damp air and a frost: on its filaments only for the drill's 15-20 min (voenizdat_osa_1991 p223).
        const bool on = c.panel.Get(k.equipment_control) == 1;
        if (on && !equipment_was_on_) {
            equipment_on_s_ = t;
            frost_damp_ = w.outside_c < 0.0 && RelativeHumidityPct(w.outside_c, w.dewpoint_c) > k.frost_humidity_pct;
        }
        if (!on) frost_damp_ = false;
        if (frost_damp_ && t - equipment_on_s_ >= k.frost_filament_s) frost_damp_ = false;
        equipment_was_on_ = on;
        // Deep cold: the equipment warms up slower (an estimate: no source gives it).
        double scale = 1.0;
        if (rack_c_ < k.slow_warm_below_c) {
            scale = 1.0 + (k.slow_warm_factor_at_minus_40 - 1.0) * std::min(1.0, (k.slow_warm_below_c - rack_c_) / (k.slow_warm_below_c + 40.0));
        }
        for (const std::string& id : k.slow_warm_subsystems) c.equipment.SetTimeScale(id, scale);
    }

    // ---- Fire
    void FireBottle(ConsoleData& c, double t, const char* how) {
        const int b = DiscBottle(c);
        if (b < 0 || b >= static_cast<int>(bottle_full_.size()) || !bottle_full_[static_cast<std::size_t>(b)]) {
            events_.push_back({"Driver", "fire", std::string(how) + ": the disc stands at a spent bottle, no agent"});
            return;
        }
        bottle_full_[static_cast<std::size_t>(b)] = false;
        events_.push_back({"Driver", "fire", std::string(how) + ": bottle " + std::to_string(b + 1) + " fired"});
        for (std::size_t i = 0; i < burning_.size(); ++i) {
            if (!burning_[i].on) continue;
            if (Uniform() < def_.fire.put_out_chance) {
                burning_[i].on = false;
                events_.push_back({"Driver", "fire", "the fire in the " + def_.fire.zones[i].name + " is out"});
            }
        }
        (void)t;
    }
    void StepFire(double, ConsoleData& c, double t) {
        if (!fire_on_ || destroyed_) return;
        const VehicleFireDef& f = def_.fire;
        // The driver's button under its cover (pressed this step: a momentary).
        const bool cover_open = f.cover_control.empty() || c.panel.Get(f.cover_control) == 1;
        if (cover_open && c.panel.Get(f.button_control) == 1) FireBottle(c, t, "the fire button");
        for (std::size_t i = 0; i < burning_.size(); ++i) {
            Burn& b = burning_[i];
            if (!b.on) continue;
            if (!b.auto_done && t >= b.auto_at_s) {
                b.auto_done = true;
                FireBottle(c, t, "the «Роса» automat");
            }
            if (b.on && t - b.since_s >= f.burn_to_destroyed_s) {
                destroyed_ = true;
                events_.push_back({"Commander", "fire", "the fire in the " + f.zones[i].name + " has taken the vehicle: burnt out"});
                return;
            }
        }
    }

    void SetInputs(ConsoleData& c) {
        Equipment& eq = c.equipment;
        auto set = [&eq](const std::string& id, bool v) {
            if (!id.empty() && eq.HasInput(id)) eq.SetInput(id, v);
        };
        if (engine_on_) {
            const VehicleEngineDef& e = def_.engine;
            set(e.cold_input, coolant_c_ < e.preheat_below_c);
            set(e.running_input, running_);
            set("engine_killed", destroyed_ || EngineZoneBurning());
        }
        if (electrics_on_) {
            const VehicleElectricsDef& g = def_.electrics;
            set(g.flat_input, flat_);
            set(g.frequency_input, low_hz_);
            set(g.voltage_input, voltage_off_);
            set("ukch_shed", shed_);
            set("battery_low", charge_ < g.min_start_fraction);
        }
        if (lights_on_) set(def_.lights.lit_input, OutsideLit());
        if (climate_on_) {
            const VehicleClimateDef& k = def_.climate;
            set(k.cold_input, CabinCold());
            set(k.overheat_input, overheat_);
            set(k.transmitter_fault_input, transmitter_failed_);
            set(k.frost_input, frost_damp_);
            if (rack_failed_) set(k.rack_fault_input, true);
        }
        if (fire_on_) {
            for (std::size_t i = 0; i < def_.fire.zones.size(); ++i) set(def_.fire.zones[i].input, burning_[i].on);
            set(def_.fire.bottles_input, DiscOnFull(c));
            set("vehicle_burnt", destroyed_);
        }
    }
    bool EngineZoneBurning() const {
        for (std::size_t i = 0; i < def_.fire.zones.size(); ++i) {
            if (def_.fire.zones[i].id == "engine" && burning_[i].on) return true;
        }
        return false;
    }

    VehicleSystemsDef def_;
    std::uint32_t rng_ = 1u;
    bool engine_on_ = false, drive_on_ = false, electrics_on_ = false, lights_on_ = false, climate_on_ = false, fire_on_ = false;
    std::vector<Event> events_;
    double outside_c_ = 15.0;
    // Engine
    bool running_ = false, cranking_ = false;
    double rpm_ = 0.0, coolant_c_ = 15.0, oil_c_ = 15.0, oil_kgf_ = 0.0, engine_s_ = 0.0;
    // Network
    bool network_ready_ = false, flat_ = false, shed_ = false, low_hz_ = false, voltage_off_ = false;
    double charge_ = 1.0, network_v_ = 0.0, ammeter_a_ = 0.0, supply_v_ = 0.0, supply_hz_ = 0.0;
    // Lights
    bool headlights_ = false, sidelights_ = false, front_lamp_ = false, rear_lamps_ = false, inside_ = false, instruments_ = false;
    // Climate
    double cabin_c_ = 15.0, rack_c_ = 15.0, transmitter_c_ = 15.0, rack_hot_s_ = 0.0, hot_s_ = 0.0, equipment_on_s_ = 0.0;
    bool overheat_ = false, transmitter_failed_ = false, rack_failed_ = false, frost_damp_ = false, equipment_was_on_ = false;
    // Fire
    std::vector<bool> bottle_full_;
    std::vector<Burn> burning_;
    bool destroyed_ = false;
};

}  // namespace simcore
