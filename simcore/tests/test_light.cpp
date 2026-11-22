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
#include "simcore/air_war/save.hpp"
#include "simcore/air_war/test_waves.hpp"
#include "simcore/command/air_war_feed.hpp"
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
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT(r.Nav() != nullptr);
    if (r.Nav() == nullptr) return;
    SC_EXPECT(r.LightNow().phase == LightPhase::Dusk && r.LightNow().sun_elevation_deg > 0.0 && r.LightNow().daylight &&
              !r.GetBattery()->Night());
    bool dusk = false, night = false;
    double night_at = -1.0;
    double day_range = 0.0, dusk_range = 0.0, night_range = 0.0;
    while (r.Time() < 3600.0 && !r.Finished()) {
        r.Step(0.1);
        const SunLight& l = r.LightNow();
        const double range = r.Nav()->RangeNow();  // the range the runner gave the session's roads this step
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
    // The runner's own march to a place (the battery commander's order to the woods, planned when he gives it a minute in):
    // by day at 10:30, by night once the session's clock is moved to 19:00, the sun 22 deg down.
    auto march = [](double start_h, bool& night, double& takes_s) {
        ScenarioDef def;
        ConsoleData console;
        std::string e;
        SC_EXPECT(LoadScenarioFile(kData + "/scenarios/pu12m_battery_march.json", kData, def, console, &e));
        if (!e.empty()) std::printf("    %s\n", e.c_str());
        ApplyTimeChoice(def, TimePreset::Hour, start_h);
        ScenarioRunner r(std::move(def), std::move(console));
        while (!r.Finished() && r.Time() < 900.0 && r.MarchPlanned() == nullptr) r.Step(0.05);
        const nav::MarchPlan* p = r.MarchPlanned();
        SC_EXPECT(p != nullptr && p->found);
        if (p == nullptr) return false;
        night = p->request.night;
        takes_s = p->end_s - p->move_off_s;
        return r.LightNow().dark;
    };
    bool night = true;
    double day_s = 0.0, night_s = 0.0;
    SC_EXPECT(!march(10.5, night, day_s) && !night);
    SC_EXPECT(march(19.0, night, night_s) && night);
    std::printf("    the march to the woods: %.0f s by day, %.0f s by night\n", day_s, night_s);
    SC_EXPECT(night_s > day_s);  // the night march's speeds (march.json), not only its flag
    std::string e;
    // The fixed bucket said 18:00 was not yet night; the sun on 15 October says it is.
    command::ChainData chain;
    SC_EXPECT(command::LoadChainData(kData, "command/chain_pannonia.json", chain, &e));
    command::InfoNet net;
    SC_EXPECT(net.Init(chain, 1u, 18.0));
    SC_EXPECT(!net.NightAt(0.0));
    net.SetSky(Kesmark(10.5), LightValues());
    SC_EXPECT(net.HasSky() && net.NightAt(0.0) && !net.NightAt(-6.0));
    // The campaign's net (as the game's campaign screen starts it): the dated nation's sky, the undated theatre none.
    for (const char* theatre : {"air_war/theatre_pannonia_nation.json", "air_war/theatre_pannonia.json"}) {
        airwar::AirWarData d;
        SC_EXPECT(airwar::LoadAirWarData(kData, theatre, "air_war/faction_nato.json", d, &e));
        if (!e.empty()) std::printf("    %s\n", e.c_str());
        command::InfoNet campaign_net;
        SC_EXPECT(campaign_net.Init(chain, 1u, d.theatre.start_hour));
        const bool dated = d.theatre.day_of_year > 0;
        SC_EXPECT(command::GiveTheatreSky(campaign_net, d.theatre) == dated && campaign_net.HasSky() == dated);
        // Its first evening at 18:00: dark by the sun on 15 October, not yet by the fixed 20:00.
        const double six_pm = 18.0 - d.theatre.start_hour;
        SC_EXPECT(campaign_net.NightAt(six_pm) == dated);
        SC_EXPECT(!campaign_net.NightAt(12.0 - d.theatre.start_hour));
    }
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
                // A battle on its date: the scenario says its own clock.
                if (g.id == "ranges_history") {
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
    SC_EXPECT(fixed >= 3 + 11);
    // The air war's sample raids take the menu's hour: their planned one is only the first offered.
    const MissionDef* raid = c.Find("air_war_nato_strike");
    SC_EXPECT(raid != nullptr && !raid->time_fixed);
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
    // The tuned theatre is dated (roadmap S8) under light.mode 1; a save made before S8 (light.mode 0) has it undated.
    SC_EXPECT(d.theatre.day_of_year == 288 && d.theatre.date_day_of_year == 288 && d.theatre.dated_from_light_mode == 1);
    d.tuning.values["light.mode"] = 0.0;
    airwar::ApplyLightMode(d);
    // Undated: the buckets (dusk 18-21), the planner's hours as before.
    SC_EXPECT(d.theatre.day_of_year == 0);
    SC_EXPECT(airwar::TheatrePartAt(d.theatre, 19.0) == airwar::DayPart::Dusk);
    SC_EXPECT(airwar::TheatrePartHour(d.theatre, 19.0, airwar::DayPart::Dusk) == airwar::DayPartHour(airwar::DayPart::Dusk));
    // Dated 15 October: 19:00 is night, 17:00 dusk, 10:00 day; the dusk aimed at is sunset.
    d.tuning.values["light.mode"] = 1.0;
    airwar::ApplyLightMode(d);
    SC_EXPECT(d.theatre.day_of_year == 288 && d.theatre.utc_offset_h == 1.0);
    SC_EXPECT(airwar::TheatrePartAt(d.theatre, 19.0) == airwar::DayPart::Night);
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

SC_TEST("light: an undated theatre's raid is lit as it was planned; the sample raids play in the twilight they were planned for") {
    airwar::AirWarData d;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_nato.json", d, &e));
    d.tuning.values["light.mode"] = 0.0;  // the tuned theatre as a save made before S8 has it: undated
    airwar::ApplyLightMode(d);
    SC_EXPECT(d.theatre.day_of_year == 0);
    airwar::AirCampaign c;
    SC_EXPECT(c.Init(d, 1));
    c.Posture().emission = 0.1;
    int built = 0;
    while (c.Now() < 6.0 * 24.0 && built < 4) {
        if (!c.Advance(6.0 * 24.0)) continue;
        const airwar::Package p = *c.PlayerMission();
        airwar::RaidScenarioBuilder builder;
        const airwar::RaidScenario rs = builder.Build(c, p);
        c.SkipPlayerMission();
        if (!rs.ok) continue;
        ++built;
        // No clock and no date: the planner's part in the legacy keys, the session's light from them.
        SC_EXPECT(rs.json_text.find("\"clock_start\"") == std::string::npos && rs.json_text.find("\"date\"") == std::string::npos);
        const bool night = p.part == airwar::DayPart::Night;
        SC_EXPECT(rs.manifest.night == night);
        ScenarioDef def;
        ConsoleData console;
        const bool loaded = LoadScenario(rs.json_text, kData, def, console, &e);
        if (!loaded) std::printf("    %s\n", e.c_str());
        SC_EXPECT(loaded && !def.clock_given && def.battery_night == night && def.optics.daylight == !night);
        const SunLight l = LightAt(def.clock, 0.0, def.light_values);
        std::printf("    a raid at %s: the session %s\n", airwar::DayPartName(p.part), l.dark ? "dark" : "in daylight");
        SC_EXPECT(l.dark == night && l.daylight == !night);
    }
    SC_EXPECT(built >= 1);
    // The menu's samples (regenerated from the dated tuned theatre, roadmap S8): each has its planner's clock and date
    // in the war's year, plays at the part of the day its description names, in daylight for the TV at its second 0 (the
    // dusk raids run on into the dark, as their planners' clocks have them), and every aircraft in it carries its type's flight limits (R3).
    MissionCatalog cat;
    SC_EXPECT(LoadMissionCatalog(kData, cat, &e));
    int samples = 0;
    for (const MissionGroup& g : cat.groups) {
        if (g.id != "air_war") continue;
        for (const MissionDef& m : g.missions) {
            ScenarioDef def;
            ConsoleData console;
            SC_EXPECT(LoadScenarioFile(kData + "/" + m.file, kData, def, console, &e));
            SC_EXPECT(def.clock_given && def.year == 1985 && def.clock.day_of_year >= 288);
            for (const ScenarioTarget& t : def.targets) {
                if (t.max_load_g <= 0.0) std::printf("    %s: %s has no flight limits\n", m.file.c_str(), t.name.c_str());
                SC_EXPECT(t.max_load_g > 0.0 && t.roll_rate_rad_s > 0.0);
            }
            const bool dawn = def.description.find(", dawn,") != std::string::npos;
            const bool dusk = def.description.find(", dusk,") != std::string::npos;
            SC_EXPECT(dawn != dusk);
            const SunLight first = LightAt(def.clock, 0.0, def.light_values);
            const SunLight last = LightAt(def.clock, def.time_limit_s, def.light_values);
            std::printf("    %s at %s: the sun %.1f deg, at the end %.1f deg\n", m.file.c_str(), FormatLocalHour(def.clock.start_local_h).c_str(),
                        first.sun_elevation_deg, last.sun_elevation_deg);
            SC_EXPECT(first.phase == (dawn ? LightPhase::Dawn : LightPhase::Dusk));
            SC_EXPECT(first.daylight && !first.dark);
            if (dawn) SC_EXPECT(last.daylight && !last.dark);  // dawn only brightens
            ++samples;
        }
    }
    SC_EXPECT(samples == 7);
}

// ---------------------------------------------------------------------------------------------- roadmap S8

SC_TEST("light (S8): the moon's place and phase against two eclipses, and its light by hand") {
    // The total lunar eclipse of 21 January 2000 (mid-eclipse about 04:44 UT): the moon full.
    const MoonPosition full = MoonAt(46.9, 19.7, 2000, 21, 4.73);
    std::printf("    21 Jan 2000 04:44 UT: %.4f lit, phase angle %.2f deg\n", full.illuminated, RadToDeg(full.phase_angle_rad));
    SC_EXPECT(full.illuminated > 0.995);
    // The total solar eclipse of 11 August 1999, its greatest about 11:03 UT near 45.1 N 24.3 E: the moon new, and on the
    // sun (the parallax taken off) to within its own size and the formulas' few tenths of a degree.
    const MoonPosition dark = MoonAt(45.1, 24.3, 1999, 223, 11.05);
    const SunPosition sun = SunAt(45.1, 24.3, 223, 11.05);
    const double cos_sep = std::sin(dark.elevation_rad) * std::sin(sun.elevation_rad) +
                           std::cos(dark.elevation_rad) * std::cos(sun.elevation_rad) * std::cos(dark.azimuth_rad - sun.azimuth_rad);
    const double sep = RadToDeg(std::acos(Clamp(cos_sep, -1.0, 1.0)));
    std::printf("    11 Aug 1999 11:03 UT: %.4f lit, the moon %.2f deg from the sun (sun at %.1f deg)\n", dark.illuminated, sep,
                RadToDeg(sun.elevation_rad));
    SC_EXPECT(dark.illuminated < 0.005);
    SC_EXPECT(sep < 0.8);
    // The Julian day of J2000.0 and of 1 January 1985 (Meeus 1998, ch. 7).
    SC_EXPECT_NEAR(JulianDay(2000, 1, 12.0), 2451545.0, 1e-9);
    SC_EXPECT_NEAR(JulianDay(1985, 1, 0.0), 2446066.5, 1e-9);
    // The moon's light by hand: full and overhead, magnitude -12.73, 2.54e-6 x 10^5.092 = 0.3140 lux above the air,
    // times 10^(-0.08) through one air mass: 0.2610 lux. At a phase angle of 90 deg, magnitude
    // -12.73 + 2.34 + 0.2624 = -10.1276: 0.02376 lux.
    SC_EXPECT_NEAR(MoonLux(90.0, 0.0), 0.2610, 0.0005);
    SC_EXPECT_NEAR(MoonLux(90.0, 90.0), 0.02376, 0.0002);
    SC_EXPECT(MoonLux(-1.0, 0.0) == 0.0);
    SC_EXPECT(MoonLux(10.0, 0.0) < MoonLux(40.0, 0.0));
}

SC_TEST("light (S8): the light on the ground through the twilight, and the TV's threshold rising with its fall") {
    SC_EXPECT_NEAR(SkyLuxOfSun(-6.0), 3.4, 1e-9);
    SC_EXPECT_NEAR(SkyLuxOfSun(-0.8), 400.0, 1e-9);
    SC_EXPECT_NEAR(SkyLuxOfSun(-30.0), 0.001, 1e-12);
    double last = 0.0;
    for (int deg = -20; deg <= 90; ++deg) {
        const double lux = SkyLuxOfSun(static_cast<double>(deg));
        SC_EXPECT(lux >= last);
        last = lux;
    }
    LightValues v;
    std::string e;
    SC_EXPECT(LoadLightValues(kData, "gameplay/light.json", v, &e));
    SC_EXPECT(v.tv_full_lux > 0.0);
    // Full light down to the sunset's 400 lux; Rose's square root below it: 4 x the threshold at a sixteenth of it.
    SC_EXPECT_NEAR(TvThresholdFactor(5000.0, v), 1.0, 1e-12);
    SC_EXPECT_NEAR(TvThresholdFactor(v.tv_full_lux / 16.0, v), 4.0, 1e-9);
    SC_EXPECT_NEAR(TvThresholdFactor(10.0, LightValues()), 1.0, 1e-12);  // the code's defaults: no gradient
    // The range a factor k takes off the TV's contrast range is ln(k) V / 3.912 (Koschmieder): 20 km at 40 km
    // visibility in full light, 20 - ln 4 x 40 / 3.912 = 5.825 km at a factor of 4.
    const em::OpticalSensorDef cam;
    const double th = em::AnchorThreshold(cam.inherent_contrast, 20000.0, em::kReferenceVisibilityM);
    SC_EXPECT_NEAR(em::ContrastRangeM(cam.inherent_contrast, th * 4.0, em::kReferenceVisibilityM), 20000.0 - std::log(4.0) * 40000.0 / 3.912, 1.0);
    // A target at 12 km: seen in full light, too dark for the TV with the sun 3 deg down.
    em::OpticalConditions c;
    SC_EXPECT(em::Look(cam, th, c, 12000.0, 15.0, 0.05, true).seen);
    const SunLight dusk = LightOfSun(-3.0, 260.0, false, v);
    std::printf("    the sun 3 deg down: %.1f lux, the TV's threshold x %.2f\n", dusk.lux, dusk.tv_threshold_factor);
    SC_EXPECT(dusk.daylight && dusk.tv_threshold_factor > 2.0);
    c.threshold_factor = dusk.tv_threshold_factor;
    const em::OpticalLook dim = em::Look(cam, th, c, 12000.0, 15.0, 0.05, true);
    SC_EXPECT(!dim.seen && dim.why_not != nullptr && std::string(dim.why_not) == "too dark for the TV");
    SC_EXPECT(em::Look(cam, th, c, 3000.0, 15.0, 0.05, true).seen);  // near, still seen
    // The factor through the evening: 1 while the sun is up, rising through the twilight.
    double prev = 0.0;
    for (int i = 0; i <= 22; ++i) {
        const double el = 5.0 - 0.5 * i;
        const double k = LightOfSun(el, 260.0, false, v).tv_threshold_factor;
        SC_EXPECT(k >= prev - 1e-12);
        if (el >= 0.0) SC_EXPECT_NEAR(k, 1.0, 1e-12);
        prev = k;
    }
}

SC_TEST("light (S8): the runner tells the engagement the TV's threshold as the sun sets, and the night's moon in the debrief") {
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice.json", kData, def, console, &e));
    ApplyTimeChoice(def, TimePreset::Hour, 16.75);  // 16:45, the sun about to set; 17:25 night
    def.time_limit_s = 2700.0;  // held past the dusk: the practice's targets gone, the sky still turns
    def.end_when_targets_gone = false;
    ScenarioRunner r(std::move(def), std::move(console));
    SC_EXPECT_NEAR(r.Engagement().OpticalConditions().threshold_factor, 1.0, 1e-12);
    double at_dusk = 0.0;
    while (r.Time() < 2700.0 && !r.Finished()) {
        r.Step(0.5);
        if (r.LightNow().daylight) at_dusk = r.Engagement().OpticalConditions().threshold_factor;
        SC_EXPECT_NEAR(r.Engagement().OpticalConditions().threshold_factor, r.LightNow().tv_threshold_factor, 0.01);
    }
    std::printf("    the TV's threshold x %.1f just before its daylight ended\n", at_dusk);
    SC_EXPECT(at_dusk > 5.0);
    if (!r.Finished()) {
        SC_EXPECT(!r.LightNow().daylight);
        const std::string d = r.Debrief();
        SC_EXPECT(d.find("night: the sun") != std::string::npos);
        SC_EXPECT(d.find("; no moon") != std::string::npos || d.find("% lit,") != std::string::npos);
    }
}

SC_TEST("light (S8): the war's moon, and the night's visual range by it") {
    // 15 October 1985 over the Kesmark range: two weeks before the total lunar eclipse of 28 October 1985, the moon new.
    const SkyClock war = Kesmark(22.0);
    LightValues v;
    std::string e;
    SC_EXPECT(LoadLightValues(kData, "gameplay/light.json", v, &e));
    const SunLight night = LightAt(war, 0.0, v);
    std::printf("    15 Oct 1985 22:00: the moon %.0f %% lit, %.1f deg up, %.4f lux, share %.2f\n", 100.0 * night.moon_illuminated,
                night.moon_elevation_deg, night.moon_lux, night.moon_share);
    SC_EXPECT(night.moon_illuminated < 0.05 && night.moon_share == 0.0);
    // A fortnight later, the full moon high at midnight.
    SkyClock full = Kesmark(0.0);
    full.day_of_year = 302;  // 29 October 1985, the night after the eclipse
    const SunLight bright = LightAt(full, 0.0, v);
    std::printf("    29 Oct 1985 00:00: the moon %.0f %% lit, %.1f deg up, %.3f lux, share %.2f\n", 100.0 * bright.moon_illuminated,
                bright.moon_elevation_deg, bright.moon_lux, bright.moon_share);
    SC_EXPECT(bright.moon_illuminated > 0.97 && bright.moon_elevation_deg > 30.0 && bright.moon_share > 0.8);
    SC_EXPECT(bright.lux > night.lux);
    // The eye's range at night: night_m without a moon, towards moonlit_m with a full one.
    nav::VisualRangeValues vr;
    SC_EXPECT(nav::LoadVisualRange(kData, "gameplay/visual_range.json", vr, &e));
    SC_EXPECT(vr.moonlit_m > vr.night_m);
    SC_EXPECT_NEAR(nav::VisualRangeM(40000.0, -20.0, vr, 0.0), vr.night_m, 1e-9);
    SC_EXPECT_NEAR(nav::VisualRangeM(40000.0, -20.0, vr, 1.0), vr.moonlit_m, 1e-9);
    SC_EXPECT_NEAR(nav::VisualRangeM(40000.0, -20.0, vr, 0.5), 0.5 * (vr.night_m + vr.moonlit_m), 1e-9);
    SC_EXPECT_NEAR(nav::VisualRangeM(40000.0, 10.0, vr, 1.0), vr.day_m, 1e-9);  // the moon by day changes nothing
    SC_EXPECT_NEAR(nav::VisualRangeM(1000.0, -20.0, vr, 1.0), 1000.0, 1e-9);   // the weather's cap
}

SC_TEST("light (S8): the air war reads light.json, and a save keeps its light mode") {
    airwar::AirWarData d;
    std::string e;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia_nation.json", "air_war/faction_nato.json", d, &e));
    // The nation's theatre keeps its own date under either mode (dated before S8).
    SC_EXPECT(d.theatre.day_of_year == 288 && d.theatre.dated_from_light_mode == 0);
    LightValues file;
    SC_EXPECT(LoadLightValues(kData, "gameplay/light.json", file, &e));
    SC_EXPECT_NEAR(d.theatre.light.tv_full_lux, file.tv_full_lux, 1e-12);
    // The parts of the day follow the theatre's light values: a twilight ending 3 deg down makes 17:15 night.
    const double quarter_past_five = 17.25;
    SC_EXPECT(airwar::TheatrePartAt(d.theatre, quarter_past_five) == airwar::DayPart::Dusk);
    d.theatre.light.twilight_deg = 3.0;
    SC_EXPECT(airwar::TheatrePartAt(d.theatre, quarter_past_five) == airwar::DayPart::Night);
    d.tuning.values["light.mode"] = 0.0;
    airwar::ApplyLightMode(d);
    SC_EXPECT(d.theatre.day_of_year == 288 && d.theatre.light.tv_full_lux == 0.0);  // the code's defaults
    // A save now says its light mode and replays with it; one without the key replays with 0, the tuned theatre undated.
    airwar::AirWarData t;
    SC_EXPECT(airwar::LoadAirWarData(kData, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json", t, &e));
    airwar::AirCampaign c;
    SC_EXPECT(c.Init(t, 5));
    c.Posture().emission = 0.1;
    c.Advance(30.0);
    const std::string saved = airwar::SaveCampaignJson(c, "air_war/theatre_pannonia.json", "air_war/faction_warsaw_pact.json");
    const std::size_t at = saved.find("\"light_mode\"");
    SC_EXPECT(at != std::string::npos);
    airwar::AirCampaign back;
    SC_EXPECT(airwar::LoadCampaignJson(saved, kData, back, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(back.Data().theatre.day_of_year == 288 && back.Journal().size() == c.Journal().size());
    SC_EXPECT(back.Packages().size() == c.Packages().size());
    std::string old = saved;
    if (at != std::string::npos) old.replace(at, 12, "\"light_mode_before_s8\"");
    airwar::AirCampaign before;
    SC_EXPECT(airwar::LoadCampaignJson(old, kData, before, &e));
    SC_EXPECT(before.Data().theatre.day_of_year == 0);
}
