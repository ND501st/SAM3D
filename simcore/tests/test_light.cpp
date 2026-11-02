// One sun for a session (roadmap R7): dates, the sun's crossings, the light and its phases (simcore/solar.hpp,
// simcore/light.hpp), the scenario's sky clock and the menu's time of day (scenario.hpp), the runner's light and what
// reads it, the battery link's two clocks, the waves' night, the couriers' night and the air war's dated theatre.
// Hand values: the Kesmark range (46.9 N, 19.7 E) on 15 October (day 288), UTC+1, from the same formulas worked by hand
// (the design's sun2.py): 33.06 deg at 10:30, -1.8 deg at 17:00, -22.2 deg at 19:00; the sun sets (0 deg) at 16:49, is
// 3 deg down at 17:08 and 6 deg down at 17:25; in the morning 6 deg down at 05:28 and 3 deg down at 05:47.
#include <cmath>
#include <cstdio>
#include <string>

#include "simcore/air_war/campaign.hpp"
#include "simcore/air_war/raid_scenario.hpp"
#include "simcore/air_war/test_waves.hpp"
#include "simcore/battery_link.hpp"
#include "simcore/command/info_net.hpp"
#include "simcore/light.hpp"
#include "simcore/missions.hpp"
#include "simcore/nav/session.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "simcore/solar.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;
constexpr double kMin = 1.0 / 60.0;

SkyClock Kesmark(double start_h) {
    SkyClock c;
    c.start_local_h = start_h;
    return c;
}

std::string ReadFile(const std::string& rel) {
    std::string text;
    detail::ReadTextFile(kData + "/" + rel, text);
    return text;
}

std::string Replaced(std::string text, const std::string& from, const std::string& to) {
    const std::size_t at = text.find(from);
    SC_EXPECT(at != std::string::npos);
    if (at != std::string::npos) text.replace(at, from.size(), to);
    return text;
}

bool LoadText(const std::string& text, ScenarioDef& def, ConsoleData& console, std::string* e) {
    return LoadScenario(text, kData, def, console, e);
}

}  // namespace

SC_TEST("light: dates to days of the year and back, leap years, and the dates that do not exist") {
    SC_EXPECT(DayOfYear(1985, 10, 15) == 288);
    SC_EXPECT(DayOfYear(1988, 2, 20) == 51);  // a leap year, but 20 February is before the leap day
    SC_EXPECT(DayOfYear(1982, 7, 24) == 205);
    SC_EXPECT(DayOfYear(1984, 12, 31) == 366);
    SC_EXPECT(DayOfYear(1985, 2, 29) == 0 && DayOfYear(1988, 2, 30) == 0 && DayOfYear(1985, 13, 1) == 0);
    int y = 0, d = 0, m = 0, dd = 0;
    SC_EXPECT(ParseIsoDate("1985-10-15", y, d) && y == 1985 && d == 288);
    SC_EXPECT(!ParseIsoDate("1988-02-30", y, d) && !ParseIsoDate("15.10.1985", y, d) && !ParseIsoDate("1985-1-15", y, d));
    SC_EXPECT(DateOfDay(1985, 288, m, dd) && m == 10 && dd == 15);
    SC_EXPECT(DateOfDay(1984, 366, m, dd) && m == 12 && dd == 31);
    SC_EXPECT(!DateOfDay(1985, 366, m, dd));
}

SC_TEST("light: the sun's crossings over the Kesmark range on 15 October, by hand to two minutes") {
    double h = 0.0;
    SC_EXPECT(SunCrossing(46.9, 19.7, 288, 1.0, 0.0, false, h));
    SC_EXPECT_NEAR(h, 16.0 + 49.0 * kMin, 2.0 * kMin);
    SC_EXPECT(SunCrossing(46.9, 19.7, 288, 1.0, -6.0, false, h));
    SC_EXPECT_NEAR(h, 17.0 + 25.0 * kMin, 2.0 * kMin);
    SC_EXPECT(SunCrossing(46.9, 19.7, 288, 1.0, -6.0, true, h));
    SC_EXPECT_NEAR(h, 5.0 + 28.0 * kMin, 2.0 * kMin);
    SC_EXPECT(SunCrossing(46.9, 19.7, 288, 1.0, -3.0, false, h));
    SC_EXPECT_NEAR(h, 17.0 + 8.0 * kMin, 2.0 * kMin);
    SC_EXPECT(SunCrossing(46.9, 19.7, 288, 1.0, -3.0, true, h));
    SC_EXPECT_NEAR(h, 5.0 + 47.0 * kMin, 2.0 * kMin);
    // The polar summer: the sun never goes 6 deg down at 80 N at midsummer.
    SC_EXPECT(!SunCrossing(80.0, 0.0, 172, 0.0, -6.0, false, h));
    // Cuito Cuanavale on 20 February 1988 near local noon: the sun almost overhead (declination about -11 deg, latitude
    // -15.16: 85.6 deg up), due north of the zenith.
    const SunPosition cuito = SunAt(-15.16, 19.17, 51, 10.954);
    SC_EXPECT_NEAR(RadToDeg(cuito.elevation_rad), 85.6, 0.5);
    const double az = RadToDeg(cuito.azimuth_rad);
    SC_EXPECT(az < 2.0 || az > 358.0);
}

SC_TEST("light: the phases through a day at the range, the smooth night share, and the menu's dusk at 17:08") {
    const LightValues v;
    const SunLight morning = LightAt(Kesmark(10.5), 0.0, v);
    SC_EXPECT(morning.phase == LightPhase::Day && morning.daylight && !morning.dark);
    SC_EXPECT_NEAR(morning.sun_elevation_deg, 33.06, 0.3);
    const SunLight five = LightAt(Kesmark(17.0), 0.0, v);
    SC_EXPECT(five.phase == LightPhase::Dusk && five.daylight && !five.dark);
    SC_EXPECT_NEAR(five.sun_elevation_deg, -1.8, 0.3);
    SC_EXPECT(five.night_share > 0.0 && five.night_share < 1.0);
    const SunLight seven = LightAt(Kesmark(19.0), 0.0, v);
    SC_EXPECT(seven.phase == LightPhase::Night && seven.dark && !seven.daylight);
    SC_EXPECT_NEAR(seven.sun_elevation_deg, -22.2, 0.3);
    SC_EXPECT(LightAt(Kesmark(5.75), 0.0, v).phase == LightPhase::Dawn);
    SC_EXPECT_NEAR(NightShare(0.0), 0.5, 1e-12);
    SC_EXPECT_NEAR(NightShare(20.0), 0.0, 1e-12);
    SC_EXPECT_NEAR(NightShare(-12.0), 1.0, 1e-12);
    SC_EXPECT_NEAR(PresetHour(Kesmark(10.5), TimePreset::Dusk), 17.0 + 8.0 * kMin, 2.0 * kMin);
    SC_EXPECT_NEAR(PresetHour(Kesmark(10.5), TimePreset::Dawn), 5.0 + 47.0 * kMin, 2.0 * kMin);
    SC_EXPECT(LightAt(Kesmark(PresetHour(Kesmark(10.5), TimePreset::Night)), 0.0, v).phase == LightPhase::Night);
    SC_EXPECT(LightAt(Kesmark(PresetHour(Kesmark(10.5), TimePreset::Day)), 0.0, v).phase == LightPhase::Day);
    SC_EXPECT(FormatLocalHour(17.0 + 8.0 * kMin) == "17:08");
    // The clock across midnight and across the year's end.
    SC_EXPECT(Kesmark(23.5).DayOfYearAt(3600.0) == 289);
    SC_EXPECT_NEAR(Kesmark(23.5).LocalHourAt(3600.0), 0.5, 1e-9);
    SkyClock end = Kesmark(23.5);
    end.day_of_year = 365;
    SC_EXPECT(end.DayOfYearAt(3600.0) == 1);
    SC_EXPECT(end.DayOfYearAt(0.0) == 365);
}

SC_TEST("light: data/gameplay/light.json loads, and its civil twilight is the visual range's") {
    LightValues v;
    std::string e;
    SC_EXPECT(LoadLightValues(kData, "gameplay/light.json", v, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    nav::VisualRangeValues vr;
    SC_EXPECT(nav::LoadVisualRange(kData, "gameplay/visual_range.json", vr, &e));
    SC_EXPECT_NEAR(v.twilight_deg, vr.twilight_deg, 1e-12);
    SC_EXPECT_NEAR(v.tv_min_elevation_deg, -6.0, 1e-12);
}

SC_TEST("light: a scenario's sky clock: its place, its date, the legacy keys as the start hour, a clash refused") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    // osa_practice: no clock, no light keys: the Kesmark range at 10:30 on 15 October.
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice.json", kData, def, console, &e));
    SC_EXPECT(!def.clock_given && def.clock_start_s < 0.0);
    SC_EXPECT_NEAR(def.clock.lat_deg, 46.9, 1e-9);
    SC_EXPECT(def.clock.day_of_year == 288 && def.clock.utc_offset_h == 1.0);
    SC_EXPECT_NEAR(def.clock.start_local_h, 10.5, 1e-12);
    // The Angola night raid without its clock: "daylight": false starts it at 23:00, dark.
    const std::string angola = ReadFile("scenarios/osa_angola_night.json");
    ScenarioDef a;
    SC_EXPECT(LoadText(Replaced(angola, "\"date\": \"1985-10-15\", \"utc_offset_h\": 1, \"clock_start\": \"23:00\",", ""), a, console, &e));
    SC_EXPECT(!a.clock_given && a.clock.start_local_h == 23.0);
    SC_EXPECT(LightAt(a.clock, 0.0, a.light_values).dark);
    // With it: the same, and its date.
    ScenarioDef dated;
    SC_EXPECT(LoadText(angola, dated, console, &e));
    SC_EXPECT(dated.clock_given && dated.year == 1985 && dated.clock.day_of_year == 288);
    // A clock at 10:00 with "daylight": false contradicts the sun.
    const std::string practice = ReadFile("scenarios/osa_practice.json");
    const std::string origin = "\"origin\": { \"lat_deg\": 46.9, \"lon_deg\": 19.7, \"alt_m\": 110 },";
    ScenarioDef bad;
    e.clear();
    SC_EXPECT(!LoadText(Replaced(practice, origin, origin + " \"clock_start\": \"10:00\", \"environment\": { \"daylight\": false },"), bad,
                        console, &e));
    SC_EXPECT(e.find("environment.daylight") != std::string::npos);
    e.clear();
    SC_EXPECT(!LoadText(Replaced(practice, origin, origin + " \"date\": \"1988-02-30\","), bad, console, &e));
    SC_EXPECT(e.find("date") != std::string::npos);
    // At 19:00 with "daylight": false it agrees.
    SC_EXPECT(LoadText(Replaced(practice, origin, origin + " \"clock_start\": \"19:00\", \"environment\": { \"daylight\": false },"), bad,
                       console, &e));
    // The menu's hour moves the paper clock only when the scenario has one.
    SetStartHour(def, 17.0);
    SC_EXPECT(def.clock_start_s < 0.0 && def.clock.start_local_h == 17.0);
    SetStartHour(dated, 6.25);
    SC_EXPECT_NEAR(dated.clock_start_s, 6.25 * 3600.0, 1e-9);
    ApplyTimeChoice(def, TimePreset::Night);
    SC_EXPECT(!def.optics.daylight && def.battery_night);
    const double night_h = def.clock.start_local_h;
    ApplyTimeChoice(def, TimePreset::Mission);
    SC_EXPECT(def.clock.start_local_h == night_h);
}

SC_TEST("light: the runner reads one sun: the TV's daylight by the menu's night, the battery's night at dusk, the range's steps") {
    {
        ScenarioDef def;
        ConsoleData console;
        std::string e;
        SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice.json", kData, def, console, &e));
        ApplyTimeChoice(def, TimePreset::Night);
        ScenarioRunner r(std::move(def), std::move(console));
        SC_EXPECT(r.LightNow().dark && !r.LightNow().daylight);
        SC_EXPECT(!r.Engagement().OpticalConditions().daylight);
        const em::OpticalLook look = em::Look(em::OpticalSensorDef{}, 0.01, r.Engagement().OpticalConditions(), 5000.0, 5.0, 0.1, true);
        SC_EXPECT(look.why_not != nullptr && std::string(look.why_not) == "no daylight for the TV");
    }
    // The bridges mission moved to 16:30: the sun low but up (dusk, daylight), then set, then night; the battery's night
    // comes after 17:25.
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    const std::string text = Replaced(ReadFile("scenarios/pu12m_mission_cover.json"), "\"clock_start\": \"19:00\"", "\"clock_start\": \"16:30\"");
    SC_EXPECT(LoadText(text, def, console, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    if (!def.has_battery) return;
    nav::VisualRangeValues vr;
    if (def.nav) vr = def.nav->Range();
    const double vis = def.weather.visibility_km * 1000.0;
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.LightNow().phase == LightPhase::Dusk && r.LightNow().sun_elevation_deg > 0.0 && r.LightNow().daylight &&
              !r.GetBattery()->Night());
    bool dusk = false, night = false;
    double night_at = -1.0;
    double day_range = 0.0, dusk_range = 0.0, night_range = 0.0;
    while (r.Time() < 3600.0 && !r.Finished()) {
        r.Step(0.1);
        const SunLight& l = r.LightNow();
        const double range = nav::VisualRangeM(vis, l.sun_elevation_deg, vr);
        if (l.sun_elevation_deg >= 0.0) day_range = range;
        if (l.phase == LightPhase::Dusk && l.sun_elevation_deg < 0.0) {
            dusk = true;
            dusk_range = range;
        }
        if (l.phase == LightPhase::Night && night_at < 0.0) night_at = r.Time();
        if (l.phase == LightPhase::Night) night_range = range;
        night = night || r.GetBattery()->Night();
    }
    std::printf("    16:30 start: night (the sun 6 deg down) at %s; visual range %.0f / %.0f / %.0f m\n", FormatLocalHour(16.5 + night_at / 3600.0).c_str(),
                day_range, dusk_range, night_range);
    SC_EXPECT(dusk && night && r.LightNow().dark);
    SC_EXPECT_NEAR(16.5 + night_at / 3600.0, 17.0 + 25.0 * kMin, 3.0 * kMin);
    SC_EXPECT(day_range > dusk_range && dusk_range > night_range);
    SC_EXPECT(r.Debrief().find("night: the sun") != std::string::npos);
}

SC_TEST("light: the waves' optical defences take the night factor in the dark; radar-directed ones do not") {
    airwar::WavesDef waves;
    std::string e;
    SC_EXPECT(airwar::LoadWavesFile(kData, "gameplay/test_waves.json", waves, &e));
    const airwar::TuningTable& tune = waves.tuning;
    airwar::DefenceDef gun;
    gun.lethality = 0.3;
    gun.radar = false;
    gun.low_cover = true;
    gun.medium_cover = true;
    airwar::DefenceDef radar = gun;
    radar.radar = true;
    const Vec3 site{0.0, 0.0, 0.0};
    const Vec3 at{-3000.0, 0.0, 300.0};
    const Vec3 vel{200.0, 0.0, 0.0};
    const double day = airwar::WavePassKillChance(gun, 3000.0, site, at, vel, 300.0, false, tune);
    const double dark = airwar::WavePassKillChance(gun, 3000.0, site, at, vel, 300.0, false, tune, true);
    SC_EXPECT(day > 0.0);
    SC_EXPECT_NEAR(dark, day * tune.Get("attrition.night"), 1e-12);
    SC_EXPECT(tune.Get("attrition.night") < 1.0);
    SC_EXPECT_NEAR(airwar::WavePassKillChance(radar, 3000.0, site, at, vel, 300.0, false, tune, true),
                   airwar::WavePassKillChance(radar, 3000.0, site, at, vel, 300.0, false, tune), 1e-12);
}

SC_TEST("light: a march planned in the dark is a night march; the couriers' night is the sun's") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_mission_cover.json", kData, def, console, &e));
    if (def.nav && def.has_battery) {
        const nav::Anchor* to = def.nav->GetAnchors().Find("bty21_bridges");
        SC_EXPECT(to != nullptr);
        std::vector<nav::MarchVehicle> column;
        for (const BatteryVehicleDef& v : def.battery_vehicles) {
            nav::MarchVehicle m;
            m.def = v;
            column.push_back(m);
        }
        if (to != nullptr && !column.empty()) {
            const Vec3 head = column.front().def.start;
            const nav::MarchPlan p = def.nav->PlanTo({head.x, head.y}, "bty21_bridges", column, def.battery_tactics, 0.0, nullptr, true);
            SC_EXPECT(p.found && p.request.night);
        }
    }
    // The fixed bucket said 18:00 was not yet night; the sun on 15 October says it is.
    command::ChainData chain;
    SC_EXPECT(command::LoadChainData(kData, "command/chain_pannonia.json", chain, &e));
    command::InfoNet net;
    SC_EXPECT(net.Init(chain, 1u, 18.0));
    SC_EXPECT(!net.NightAt(0.0));
    net.SetSky(Kesmark(10.5), LightValues());
    SC_EXPECT(net.HasSky() && net.NightAt(0.0) && !net.NightAt(-6.0));
}

SC_TEST("light: the battery link's two halves take the menu's time together, and must keep one clock") {
    std::string e;
    auto link = LoadBatteryLinkFile(kData + "/scenarios/battery_link_practice.json", kData, &e, nullptr,
                                    [](ScenarioDef& d) { ApplyTimeChoice(d, TimePreset::Night); });
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(link != nullptr);
    if (link != nullptr) SC_EXPECT(link->Osa().LightNow().dark && link->Post().LightNow().dark);
    e.clear();
    auto split = LoadBatteryLinkFile(kData + "/scenarios/battery_link_practice.json", kData, &e, nullptr, [](ScenarioDef& d) {
        if (d.has_post) ApplyTimeChoice(d, TimePreset::Dusk);
    });
    SC_EXPECT(split == nullptr && e.find("same clock") != std::string::npos);
}

SC_TEST("light: the missions menu's time: the battles and lessons held, the rest offer all six, labelled by the sun") {
    MissionCatalog c;
    std::string e;
    SC_EXPECT(LoadMissionCatalog(kData, c, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    int fixed = 0;
    for (const MissionGroup& g : c.groups) {
        for (const MissionDef& m : g.missions) {
            if (m.time_fixed) {
                ++fixed;
                SC_EXPECT(m.time_options.size() == 1 && m.time_options[0] == TimePreset::Mission);
                // A battle on its date, an air war raid at its planned hour: the scenario says its own clock.
                if (g.id == "ranges_history" || g.id == "air_war") {
                    ScenarioDef def;
                    ConsoleData console;
                    SC_EXPECT(LoadScenarioFile(kData + "/" + m.file, kData, def, console, &e) && def.clock_given);
                    if (!def.clock_given) std::printf("    %s has no clock_start\n", m.file.c_str());
                }
            } else {
                SC_EXPECT(m.time_options.size() == 6);
            }
        }
    }
    SC_EXPECT(fixed >= 3 + 7 + 11);
    const MissionDef* angola = c.Find("osa_angola_night");
    SC_EXPECT(angola != nullptr && angola->time_fixed);
    const MissionDef* range = c.Find("kesmark_range_test");
    SC_EXPECT(range != nullptr);
    if (range != nullptr) {
        // Dusk there is 17:08 by hand, to a couple of minutes (the range's origin is a little south-west of 46.9 N 19.7 E).
        const double dusk_h = PresetHour(range->clock, TimePreset::Dusk);
        SC_EXPECT_NEAR(dusk_h, 17.0 + 8.0 * kMin, 2.0 * kMin);
        bool dusk = false;
        std::string all;
        for (const std::string& l : TimeChoiceLabels(*range)) {
            dusk = dusk || l == "Dusk " + FormatLocalHour(dusk_h);
            all += " | " + l;
        }
        std::printf("    Kesmark range test:%s\n", all.c_str());
        SC_EXPECT(dusk);
        SC_EXPECT(TimeChoiceLabels(*range).front() == "The mission's own 10:30");
    }
    // Bad options are named.
    const std::string entry = R"({"catalog_id":"x","status":"x","notes":"x","source":"sam3d","groups":[{"id":"g","label":"g","missions":[
        {"file":"scenarios/osa_practice.json","title":"t",)";
    MissionCatalog bad;
    SC_EXPECT(!LoadMissionCatalogJson(entry + R"("time_options":["mission","noon"]}]}]})", kData, bad, &e) && e.find("time_options") != std::string::npos);
    SC_EXPECT(!LoadMissionCatalogJson(entry + R"("time":"fixed","time_options":["mission"]}]}]})", kData, bad, &e));
    SC_EXPECT(!LoadMissionCatalogJson(entry + R"("time":"sometimes"}]}]})", kData, bad, &e));
    SC_EXPECT(!LoadMissionCatalogJson(entry + R"("time_options":["dusk","mission"]}]}]})", kData, bad, &e));
    MissionCatalog good;
    SC_EXPECT(LoadMissionCatalogJson(entry + R"("time_options":["mission","dusk","night"]}]}]})", kData, good, &e));
    if (!good.groups.empty()) SC_EXPECT(good.groups[0].missions[0].time_options.size() == 3);
}

SC_TEST("light: the air war by the sun once its theatre has a date; undated, the buckets as tuned") {
    airwar::AirWarData d;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", d, &e));
    // Undated: the buckets (dusk 18-21), the planner's hours as before.
    SC_EXPECT(d.theatre.day_of_year == 0);
    SC_EXPECT(airwar::TheatrePartAt(d.theatre, 19.0) == airwar::DayPart::Dusk);
    SC_EXPECT(airwar::TheatrePartHour(d.theatre, 19.0, airwar::DayPart::Dusk) == airwar::DayPartHour(airwar::DayPart::Dusk));
    // Dated 15 October: 19:00 is night, 17:00 dusk, 10:00 day; the dusk aimed at is sunset.
    d.theatre.day_of_year = 288;
    d.theatre.utc_offset_h = 1.0;
    SC_EXPECT(airwar::DayPartBySun(d.theatre, 19.0) == airwar::DayPart::Night);
    SC_EXPECT(airwar::DayPartBySun(d.theatre, 17.0) == airwar::DayPart::Dusk);
    SC_EXPECT(airwar::DayPartBySun(d.theatre, 10.0) == airwar::DayPart::Day);
    SC_EXPECT(airwar::DayPartBySun(d.theatre, 24.0 + 19.0) == airwar::DayPart::Night);  // the next day too
    const double sunset = airwar::TheatrePartHour(d.theatre, 12.0, airwar::DayPart::Dusk);
    SC_EXPECT(sunset > 16.5 && sunset < 17.2);
    SC_EXPECT(airwar::DayPartBySun(d.theatre, sunset) == airwar::DayPart::Dusk);
    SC_EXPECT(airwar::DayPartBySun(d.theatre, airwar::TheatrePartHour(d.theatre, 12.0, airwar::DayPart::Dawn)) == airwar::DayPart::Dawn);
    SC_EXPECT(airwar::DayPartBySun(d.theatre, airwar::TheatrePartHour(d.theatre, 12.0, airwar::DayPart::Night)) == airwar::DayPart::Night);
    // A dated raid: its scenario has the clock and the date, and loads.
    airwar::AirCampaign c;
    SC_EXPECT(c.Init(d, 1));
    c.Posture().emission = 0.1;
    bool built = false;
    while (c.Now() < 6.0 * 24.0 && !built) {
        if (!c.Advance(6.0 * 24.0)) continue;
        airwar::RaidScenarioBuilder builder;
        const airwar::RaidScenario rs = builder.Build(c, *c.PlayerMission());
        if (!rs.ok) {
            c.SkipPlayerMission();
            continue;
        }
        built = true;
        SC_EXPECT(rs.json_text.find("\"clock_start\"") != std::string::npos && rs.json_text.find("\"date\": \"1985-10-") != std::string::npos);
        SC_EXPECT(rs.json_text.find("\"daylight\"") == std::string::npos);
        ScenarioDef def;
        ConsoleData console;
        const bool loaded = LoadScenario(rs.json_text, kData, def, console, &e);
        if (!loaded) std::printf("    %s\n", e.c_str());
        SC_EXPECT(loaded && def.clock_given && def.clock.day_of_year >= 288);
        double local = d.theatre.start_hour + rs.manifest.start_h;
        local -= 24.0 * std::floor(local / 24.0);
        SC_EXPECT(std::fabs(def.clock.start_local_h - local) < 1.0 / 60.0 + 1e-9);
    }
    SC_EXPECT(built);
}
