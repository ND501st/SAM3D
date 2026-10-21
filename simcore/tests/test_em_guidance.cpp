// Roadmap E5, missiles and optics: the TV sight as a passive optical sensor (em/optical.hpp), the command transmitters
// and the missiles' responders as emitters (em_link.hpp), and a target's radar warning receiver hearing the lock and
// the launch. Emitter values are the library's labelled estimates; the target numbers are placeholders.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "simcore/em/optical.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

std::string ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string ReplaceOnce(std::string s, const std::string& from, const std::string& to) {
    const std::size_t at = s.find(from);
    if (at == std::string::npos) {
        std::printf("    test text not found: %s\n", from.c_str());
        return s;
    }
    return s.replace(at, from.size(), to);
}

// The EM practice shoot, its drone given NATO's warning receiver and an evasion (placeholders: a practice drone
// carries neither).
std::string PracticeWithRwr(const std::string& evade_on, bool rwr = true) {
    std::string s = ReadFile(kData + "/scenarios/osa_practice_em.json");
    s = ReplaceOnce(s, "\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\" }",
                    "\"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\", "
                    "\"extra_libraries\": [\"em/nato_1985.json\"] }");
    s = ReplaceOnce(s, "\"id\": \"t1\",",
                    std::string("\"id\": \"t1\", ") + (rwr ? "\"rwr\": \"nato_alr46_class_rwr\", " : "") +
                        "\"evade\": { \"on\": \"" + evade_on + "\", \"kind\": \"break\" },");
    return s;
}

std::unique_ptr<ScenarioRunner> Runner(const std::string& text) {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    if (!LoadScenario(text, kData, def, console, &error)) {
        std::printf("    load: %s\n", error.c_str());
        return nullptr;
    }
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

bool Has(const std::string& debrief, const std::string& text) {
    const bool found = debrief.find(text) != std::string::npos;
    if (!found) std::printf("    missing \"%s\"\n", text.c_str());
    return found;
}

// Seconds into the run of the first debrief line holding `text` (-1 if none). Lines start with the time.
double TimeOf(const std::string& debrief, const std::string& text) {
    const std::size_t at = debrief.find(text);
    if (at == std::string::npos) return -1.0;
    const std::size_t line = debrief.rfind('\n', at);
    return std::atof(debrief.c_str() + (line == std::string::npos ? 0 : line + 1));
}

}  // namespace

SC_TEST("em optical: Koschmieder's contrast and Johnson's lines against hand calculations") {
    // 0.3 exp(-3.912 x 20/40) = 0.3 x 0.14140 = 0.042420
    SC_EXPECT_NEAR(em::ApparentContrast(0.3, 20000.0, 40000.0), 0.042420, 1e-5);
    const double th = em::AnchorThreshold(0.3, 20000.0, 40000.0);
    SC_EXPECT_NEAR(em::ContrastRangeM(0.3, th, 40000.0), 20000.0, 1e-6);
    SC_EXPECT_NEAR(em::ContrastRangeM(0.3, th, 10000.0), 5000.0, 1e-6);  // a quarter of the visibility, a quarter of the range
    em::OpticalSensorDef d;
    // The wide lens: 4.8 deg x 0.75 = 3.6 deg high, 0.0628319 rad / 575 lines = 1.09273e-4 rad a line.
    const double wide = DegToRad(4.8 * 0.75), narrow = DegToRad(1.44 * 0.75);
    SC_EXPECT_NEAR(em::LinesAcross(d, 4.5, 20000.0, wide), 4.5 / 20000.0 / 1.09273e-4, 1e-3);  // 2.059 lines
    SC_EXPECT_NEAR(em::LinesAcross(d, 1.5, 10000.0, narrow), 1.5 / 10000.0 / 3.27819e-5, 1e-3);  // 4.576 lines
}

SC_TEST("em optical: a fighter shows to 20 km on a clear day, to 5 km in a 10 km haze, never at night; a small drone needs the narrow lens") {
    em::OpticalSensorDef d;
    const double th = em::AnchorThreshold(d.inherent_contrast, 20000.0, em::kReferenceVisibilityM);
    const double wide = DegToRad(4.8 * 0.75), narrow = DegToRad(1.44 * 0.75);
    em::OpticalConditions clear, haze, night;
    haze.visibility_m = 10000.0;
    night.daylight = false;
    SC_EXPECT(em::Look(d, th, clear, 19500.0, 4.5, wide, true).seen);
    SC_EXPECT(!em::Look(d, th, clear, 20500.0, 4.5, narrow, true).seen);
    SC_EXPECT(em::Look(d, th, haze, 4800.0, 4.5, wide, true).seen);
    const em::OpticalLook hazy = em::Look(d, th, haze, 6000.0, 4.5, wide, true);
    SC_EXPECT(!hazy.seen && std::string(hazy.why_not) == "lost in the haze");
    SC_EXPECT(std::string(em::Look(d, th, night, 2000.0, 4.5, wide, true).why_not) == "no daylight for the TV");
    SC_EXPECT(std::string(em::Look(d, th, clear, 5000.0, 4.5, wide, false).why_not) == "no line of sight");
    // A 1.5 m drone at 10 km: 1.37 lines in the wide lens (not seen), 4.58 in the narrow (seen, not made out); at 5 km
    // the narrow lens gives 9.15 lines: recognised.
    const em::OpticalLook small = em::Look(d, th, clear, 10000.0, 1.5, wide, true);
    SC_EXPECT(!small.seen && std::string(small.why_not) == "too small on the screen");
    const em::OpticalLook narrow10 = em::Look(d, th, clear, 10000.0, 1.5, narrow, true);
    SC_EXPECT(narrow10.seen && !narrow10.recognised);
    SC_EXPECT(em::Look(d, th, clear, 5000.0, 1.5, narrow, true).recognised);
}

SC_TEST("em optical: in a thick haze the emissions-controlled crew's TV can't take the crossing target; in a light one it can; at night not at all") {
    const std::string clear_text = ReadFile(kData + "/scenarios/osa_emcon_tv.json");
    auto play = [&](const std::string& env) {
        const std::string text = ReplaceOnce(clear_text, "\"targets\":", "\"environment\": " + env + ", \"targets\":");
        auto r = Runner(text);
        if (r == nullptr) return std::string();
        r->RunToEnd(0.1, 700.0);
        return r->Debrief();
    };
    const auto tv_range_km = [](const std::string& d) {
        const std::size_t at = d.find("TV track, ");
        return at == std::string::npos ? -1.0 : std::atof(d.c_str() + at + 10);
    };
    const std::string clear = play("{ \"visibility_km\": 40 }");
    const std::string light = play("{ \"visibility_km\": 30 }");  // the contrast holds to 15 km
    const std::string thick = play("{ \"visibility_km\": 12 }");  // to 6 km
    const std::string night = play("{ \"daylight\": false }");
    std::printf("    TV track: clear %.1f km, light haze %.1f km, thick haze %.1f km\n", tv_range_km(clear), tv_range_km(light),
                tv_range_km(thick));
    SC_EXPECT(tv_range_km(clear) > 0.0 && clear.find("LAUNCH") != std::string::npos);
    SC_EXPECT(tv_range_km(light) > 0.0 && tv_range_km(light) <= 15.0 + 0.1);
    // Within 6 km the drone crosses fast and the board's designations are a minute old: the TV search never finds it.
    SC_EXPECT(tv_range_km(thick) < 0.0 || tv_range_km(thick) <= 6.0 + 0.1);
    SC_EXPECT(night.find("TV track") == std::string::npos);  // nothing to see in the dark
    SC_EXPECT(night.find("LAUNCH") == std::string::npos);
}

SC_TEST("em guidance: the command transmitters wait in their loads from ВНИМАНИЕ until the launch, then radiate; a missile in flight carries its responder") {
    auto r = Runner(PracticeWithRwr("launch", false));  // no warning receiver: the drone flies on to the launch
    SC_EXPECT(r != nullptr && r->Em() != nullptr);
    if (r == nullptr || r->Em() == nullptr) return;
    double switched_on = -1.0, first_on = -1.0, launch_at = -1.0;
    bool missile_seen = false;
    while (!r->Finished() && r->Time() < 700.0) {
        r->Step(0.1);
        if (switched_on < 0.0 && r->GetLauncher()->TransmitterState(1) != 0) switched_on = r->Time();
        if (r->Em()->CommandLinkRadiating(1) && first_on < 0.0) first_on = r->Time();
        if (r->Log().Summary().launches > 0 && launch_at < 0.0) launch_at = r->Time();
        if (r->Em()->MissilesInWorld() > 0) missile_seen = true;
    }
    std::printf("    channel I on (into its load) at %.1f s, on the air at %.1f s, launch at %.1f s\n", switched_on, first_on, launch_at);
    SC_EXPECT(switched_on > 0.0 && launch_at > switched_on);  // on from ВНИМАНИЕ, before the launch
    // Silent until the launch switches it from the load to the antenna (voenizdat_osa_1991 p21, p140, p155).
    SC_EXPECT(launch_at > 0.0 && first_on >= launch_at - 0.15);
    SC_EXPECT(missile_seen);
    SC_EXPECT_NEAR(r->Em()->CommandLinkFreqHz(1), 8.6e9, 1.0);  // the 1st vehicle's letter 3 (library estimate)
}

SC_TEST("em guidance: СМЕНА ЛИТ. moves the command transmitters to the vehicle's other frequency, the АПЧ lamps out a moment") {
    auto r = Runner(PracticeWithRwr("launch", false));
    if (r == nullptr || r->Em() == nullptr) return;
    // On to ВНИМАНИЕ (the command transmitters on, into their loads), then the letter key.
    while (!r->Finished() && r->Time() < 700.0 && r->GetLauncher()->TransmitterState(1) == 0) r->Step(0.1);
    SC_EXPECT(r->GetLauncher()->TransmitterState(1) != 0);
    const double f0 = r->Em()->CommandLinkFreqHz(1), g0 = r->Em()->CommandLinkFreqHz(2);
    ConsoleData& c = r->Console();
    c.panel.Set("spk.letter", 1);  // the latch open (НОРМА), then the key to СМЕНА ЛИТ.
    r->Step(0.1);
    c.panel.Set("spk.letter", 2);
    r->Step(0.1);
    const bool lamp_out = !c.equipment.GetInput("ch1_apch_lock");
    for (int i = 0; i < 20; ++i) r->Step(0.1);
    std::printf("    channel I %.2f -> %.2f GHz, channel II %.2f -> %.2f GHz\n", f0 / 1e9, r->Em()->CommandLinkFreqHz(1) / 1e9,
                g0 / 1e9, r->Em()->CommandLinkFreqHz(2) / 1e9);
    SC_EXPECT(lamp_out);
    SC_EXPECT(std::fabs(r->Em()->CommandLinkFreqHz(1) - f0) > 1e6);
    SC_EXPECT_NEAR(r->Em()->CommandLinkFreqHz(1), 8.2e9, 1.0);  // the vehicle's other frequency
}

SC_TEST("em guidance: the target's warning receiver hears the lock and the command link, named through NATO's library; it breaks on the warning") {
    // On the lock: the tracking radar's steady beam, NATO's SA-8 tracking entry.
    auto lock = Runner(PracticeWithRwr("lock"));
    if (lock == nullptr) return;
    lock->RunToEnd(0.1, 700.0);
    const std::string dl = lock->Debrief();
    SC_EXPECT(Has(dl, "warning receiver: locked by SA-8 track (Land Roll"));
    SC_EXPECT(Has(dl, "t1  breaks"));
    SC_EXPECT(TimeOf(dl, "t1  breaks") >= TimeOf(dl, "warning receiver: locked by"));
    // On the launch: the command link heard. It waits in its load until the launch, so the warning comes with the
    // missile, not before it.
    auto combat = Runner(PracticeWithRwr("launch"));
    if (combat == nullptr) return;
    combat->RunToEnd(0.1, 700.0);
    const std::string dc = combat->Debrief();
    SC_EXPECT(Has(dc, "warning receiver: missile guidance, SA-8 command-guidance radars"));
    const double warned = TimeOf(dc, "warning receiver: missile guidance");
    const double launched = TimeOf(dc, "LAUNCH");
    std::printf("    warned at %.1f s, launch at %.1f s\n", warned, launched);
    SC_EXPECT(launched > 0.0 && warned >= launched - 0.15);
}

SC_TEST("em guidance: a target's warning receiver must be an RWR of the EM libraries, with the EM layer on") {
    std::string s = PracticeWithRwr("launch");
    s = ReplaceOnce(s, "\"rwr\": \"nato_alr46_class_rwr\"", "\"rwr\": \"red_ground_esm_class\"");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(!LoadScenario(s, kData, def, console, &error) && error.find("not a radar warning receiver") != std::string::npos);
    s = PracticeWithRwr("launch");
    s = ReplaceOnce(s, "\"rwr\": \"nato_alr46_class_rwr\"", "\"rwr\": \"nothing_like_it\"");
    SC_EXPECT(!LoadScenario(s, kData, def, console, &error) && error.find("not an ESM receiver") != std::string::npos);
}
