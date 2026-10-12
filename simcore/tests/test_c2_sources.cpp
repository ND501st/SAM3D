// The battery's early-warning and command sources (roadmap E4 / 7.8): the library, what each source can see, shared voice
// nets, the crew switching source (by hand and by the commander's fallback), links that fail, and the scenario hooks.
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "simcore/c2_sources.hpp"
#include "simcore/scenario.hpp"
#include "simcore/scenario_runner.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

C2Library Library() {
    C2Library lib;
    std::string error;
    if (!LoadC2LibraryFile(kData + "/command/c2_sources.json", lib, &error)) std::printf("    library: %s\n", error.c_str());
    return lib;
}

// A library type with its errors removed, so positions are the truth's.
C2SourceDef Make(const std::string& type, const std::string& id, const Vec3& at, const std::string& net = std::string()) {
    const C2Library lib = Library();
    const C2SourceDef* t = lib.Find(type);
    C2SourceDef d = t != nullptr ? *t : C2SourceDef();
    d.id = id;
    d.type = type;
    d.net = net;
    d.position = at;
    d.has_position = true;
    d.board.navigation_error_deg = 0.0;
    d.board.navigation_error_m = 0.0;
    d.board.plot_error_deg = 0.0;
    d.board.plot_error_m = 0.0;
    return d;
}

C2SourceDef Pu12(const std::string& net = std::string()) {
    C2SourceDef d;
    d.id = "pu12";
    d.display_name = "PU-12M";
    d.short_name = "PU-12";
    d.kind = C2Kind::BatteryPost;
    d.sense = C2Sense::Relayed;
    std::string error;
    if (!LoadPlottingBoardFile(kData + "/command/pu12.json", d.board, &error)) std::printf("    board: %s\n", error.c_str());
    d.board.navigation_error_deg = 0.0;
    d.board.navigation_error_m = 0.0;
    d.net = net;
    return d;
}

BoardContact At(const std::string& id, double east, double north, double alt_m, double agl_m = -1.0) {
    BoardContact c;
    c.target_id = id;
    c.position = Vec3{east, north, alt_m};
    c.altitude_m = alt_m;
    c.height_agl_m = agl_m;
    c.type = 9;
    return c;
}

void Run(C2Network& n, double from, double to, const std::vector<BoardContact>& truth, const Vec3& unit = Vec3{}, double dt = 0.5) {
    for (double t = from; t <= to + 1e-9; t += dt) n.Update(t, truth, unit);
}

bool HasEvent(const std::vector<C2Network::Event>& events, const std::string& actor, const std::string& text_part) {
    for (const C2Network::Event& e : events) {
        if (e.actor == actor && e.text.find(text_part) != std::string::npos) return true;
    }
    return false;
}

double Wrap(double d) { return std::fmod(d + 540.0, 360.0) - 180.0; }

}  // namespace

SC_TEST("c2: the source library loads with every value cited and bad files are refused") {
    const C2Library lib = Library();
    SC_EXPECT(lib.types.size() >= 5);
    for (const char* id : {"ewr_voice", "division_ew_net", "neighbour_radio", "observation_post", "data_link"}) {
        const C2SourceDef* d = lib.Find(id);
        SC_EXPECT(d != nullptr);
        if (d == nullptr) continue;
        SC_EXPECT(d->board.update_period_s > 0.0 && d->board.max_range_m > 0.0);
        SC_EXPECT(d->short_name.size() <= 14);
    }
    SC_EXPECT(lib.Find("ewr_voice")->sense == C2Sense::Radar && lib.Find("ewr_voice")->antenna_height_m > 0.0);
    SC_EXPECT(lib.Find("observation_post")->sense == C2Sense::Visual);
    SC_EXPECT(lib.Find("data_link")->sense == C2Sense::Relayed);
    // a source library file with an uncited value, a radar without an antenna height, an unknown kind
    const std::string head = R"({ "library_id": "x", "sources": [ { "id": "a", "display_name": "A", "kind": ")";
    const std::string board = R"(, "board": { "update_period_s": { "value": 30, "source": "estimate", "confidence": "estimate" },
        "max_range_m": { "value": 1000, "source": "estimate", "confidence": "estimate" },
        "radar_battery_number": { "value": 41, "source": "estimate", "confidence": "estimate" },
        "navigation_error_deg": { "value": 0, "source": "estimate", "confidence": "estimate" },
        "navigation_error_m": { "value": 0, "source": "estimate", "confidence": "estimate" },
        "designation_range_m": { "value": 1000, "source": "estimate", "confidence": "estimate" },
        "operator_delay_s": { "value": 1, "source": "estimate", "confidence": "estimate" },
        "message_s": { "value": 1, "source": "estimate", "confidence": "estimate" },
        "pass_on_s": { "value": 1, "source": "estimate", "confidence": "estimate" } } } ] })";
    auto parse = [](const std::string& text, std::string& error) {
        json::Value root;
        std::string perr;
        if (!json::Parse(text, root, &perr)) {
            error = perr;
            return false;
        }
        C2Library out;
        detail::C2LibraryLoader loader;
        const bool ok = loader.Load(root, out);
        error = loader.Message();
        return ok;
    };
    std::string error;
    SC_EXPECT(parse(head + "ewr_voice\", \"sense\": \"visual\"" + board, error));  // the good one
    SC_EXPECT(!parse(head + "ewr_voice\", \"sense\": \"radar\"" + board, error));  // radar, no antenna height
    SC_EXPECT(error.find("antenna_height_m") != std::string::npos);
    SC_EXPECT(!parse(head + "carrier_pigeon\", \"sense\": \"visual\"" + board, error));
    SC_EXPECT(error.find("kind") != std::string::npos);
    SC_EXPECT(!parse(head + "ewr_voice\", \"sense\": \"magic\"" + board, error));
    SC_EXPECT(error.find("sense") != std::string::npos);
    SC_EXPECT(!parse(head + "ewr_voice\", \"sense\": \"visual\", \"board\": { \"update_period_s\": 30 } } ] })", error));
    SC_EXPECT(!error.empty());  // refused with a path (an uncited number)
}

SC_TEST("c2: a radar source sees to its radar horizon, a low target hides below it") {
    // 4.12 (sqrt 8 + sqrt h) km: 142 km for a target 1000 m up, 34 km for one at 30 m.
    SC_EXPECT_NEAR(C2Network::RadarHorizonM(8.0, 1000.0), 4120.0 * (std::sqrt(8.0) + std::sqrt(1000.0)), 1e-6);
    SC_EXPECT_NEAR(C2Network::RadarHorizonM(8.0, 30.0), 34262.0, 100.0);
    C2Policy policy;
    policy.auto_fallback = false;
    C2Network n(policy, 1);
    n.AddSource(Pu12(), Vec3{});
    n.AddSource(Make("ewr_voice", "ewr", Vec3{0.0, 0.0, 0.0}), Vec3{});
    // low at 40 km (below the horizon) and high at 40 km, and a low one at 20 km
    Run(n, 0.0, 1.0, {At("low_far", 0.0, 40000.0, 30.0), At("high_far", 5000.0, 40000.0, 1000.0), At("low_near", -5000.0, 20000.0, 30.0)});
    const PlottingBoard* ewr = n.BoardOf("ewr");
    SC_EXPECT(ewr != nullptr);
    if (ewr == nullptr) return;
    auto plotted = [&](const std::string& id) {
        for (const BoardMark& m : ewr->Marks()) {
            if (m.target_id == id) return true;
        }
        return false;
    };
    SC_EXPECT(!plotted("low_far"));
    SC_EXPECT(plotted("high_far"));
    SC_EXPECT(plotted("low_near"));
    // the relayed PU-12 has no horizon of its own: it plots all three
    SC_EXPECT(n.BoardOf("pu12")->Marks().size() == 3);
}

SC_TEST("c2: an observer sees only as far as a man can tell an aircraft") {
    C2Policy policy;
    policy.auto_fallback = false;
    C2Network n(policy, 1);
    n.AddSource(Pu12(), Vec3{});
    n.AddSource(Make("observation_post", "obs", Vec3{0.0, 3000.0, 0.0}), Vec3{});
    Run(n, 0.0, 1.0, {At("close", 1000.0, 5000.0, 500.0), At("far", 0.0, 12000.0, 500.0)});
    const PlottingBoard* obs = n.BoardOf("obs");
    SC_EXPECT(obs->Marks().size() == 1 && obs->Marks()[0].target_id == "close");
}

SC_TEST("c2: with only the PU-12 the crew hears what the board says, 14 s after the plot") {
    C2Policy policy;
    C2Network n(policy, 1);
    n.AddSource(Pu12(), Vec3{});
    const std::vector<BoardContact> truth = {At("a", 0.0, 40000.0, 1000.0), At("b", 45000.0, 0.0, 1000.0)};
    Run(n, 0.0, 13.5, truth);
    SC_EXPECT(n.Known(13.5).empty());
    SC_EXPECT(n.ActiveId() == "pu12");
    Run(n, 14.0, 14.0, truth);
    SC_EXPECT(n.Known(14.0).size() == 1 && n.Known(14.0)[0].target_id == "a");
    Run(n, 14.5, 26.0, truth);  // "b" waits for the radio: 12 s + 4 + 8 + 2 = 26 s
    SC_EXPECT(n.Known(26.0).size() == 2);
    SC_EXPECT(n.HeardFrom("a") == "pu12");
    const std::vector<BoardMark> known = n.Known(26.0);
    const PlottingBoard::Designation d = C2Network::DesignationFor(known[0], Vec3{});
    SC_EXPECT_NEAR(d.range_m, 40000.0, 1e-6);
    SC_EXPECT_NEAR(d.azimuth_deg, 0.0, 1e-6);
}

SC_TEST("c2: two sources on one voice net delay each other; on separate nets they don't") {
    const std::vector<BoardContact> truth = {At("a", 0.0, 40000.0, 1000.0)};
    C2Policy policy;
    policy.auto_fallback = false;
    C2Network shared(policy, 1);
    shared.AddSource(Pu12("battery_net"), Vec3{});
    shared.AddSource(Make("ewr_voice", "ewr", Vec3{0.0, 10000.0, 0.0}, "battery_net"), Vec3{});
    Run(shared, 0.0, 30.0, truth);
    C2Network apart(policy, 1);
    apart.AddSource(Pu12("battery_net"), Vec3{});
    apart.AddSource(Make("ewr_voice", "ewr", Vec3{0.0, 10000.0, 0.0}, "ewr_net"), Vec3{});
    Run(apart, 0.0, 30.0, truth);
    // separate nets: the EWR's message is read at 4-12 s and passed on by 14 s
    SC_EXPECT(apart.BoardOf("ewr")->Known(20.0).size() == 1);
    // shared net: the PU-12 holds it until 12 s; the EWR starts at 12 + 4 = 16 s, ends 24 s, known at 26 s
    SC_EXPECT(shared.BoardOf("ewr")->Known(20.0).empty());
    SC_EXPECT(shared.BoardOf("ewr")->Known(26.5).size() == 1);
    // and the other way round: the source added first is never delayed
    SC_EXPECT(shared.BoardOf("pu12")->Known(14.5).size() == 1);
}

SC_TEST("c2: the crew changes source: the old one is heard until the sets are changed, then the new one only") {
    C2Policy policy;
    policy.auto_fallback = false;
    policy.select_s = 6.0;
    C2Network n(policy, 1);
    n.AddSource(Pu12(), Vec3{});
    n.AddSource(Make("ewr_voice", "ewr", Vec3{0.0, 10000.0, 0.0}), Vec3{});
    const std::vector<BoardContact> truth = {At("t1", 0.0, 30000.0, 1000.0)};
    Run(n, 0.0, 30.0, truth);
    SC_EXPECT(n.HeardFrom("t1") == "pu12");
    SC_EXPECT(!n.RequestSource("pu12", 30.0, "Commander"));       // already listening to it
    SC_EXPECT(!n.RequestSource("nobody", 30.0, "Commander"));     // unknown
    SC_EXPECT(n.RequestSource("ewr", 30.0, "Commander", "test"));
    SC_EXPECT(n.Switching() && n.ActiveId() == "pu12" && n.PendingId() == "ewr");  // the old one is heard until the change is made
    Run(n, 30.5, 35.5, truth);
    SC_EXPECT(n.Switching());
    Run(n, 36.0, 36.0, truth);
    SC_EXPECT(!n.Switching() && n.ActiveId() == "ewr");
    Run(n, 36.5, 50.0, truth);
    // the EWR plotted at 30 s (it plots every 30 s), 14 s to tell: heard at 44 s. The PU-12's plot at 60 s is not heard.
    SC_EXPECT(n.HeardFrom("t1") == "ewr");
    SC_EXPECT(n.Known(50.0).size() == 1 && std::fabs(n.Known(50.0)[0].time_s - 30.0) < 1e-9);
    Run(n, 50.5, 80.0, truth);
    SC_EXPECT(n.HeardFrom("t1") == "ewr" && std::fabs(n.Known(80.0)[0].time_s - 60.0) < 1e-9);  // the EWR's 60 s plot, not the PU-12's
    SC_EXPECT(n.BoardOf("pu12")->Marks().size() == 2);  // the PU-12 kept plotting (0 s and 60 s); the crew wasn't listening
    const std::vector<C2Network::Event> ev = n.TakeEvents();
    SC_EXPECT(HasEvent(ev, "Commander", "changes to"));
    SC_EXPECT(HasEvent(ev, "Commander", "has "));
    // a designation heard from a source other than the main one says so
    SC_EXPECT(HasEvent(ev, "Commander", "[EWR voice]"));
}

SC_TEST("c2: going off the net leaves the vehicle on its own radar; its old designations age out") {
    C2Policy policy;
    policy.auto_fallback = false;
    policy.heard_life_s = 100.0;
    C2Network n(policy, 1);
    n.AddSource(Pu12(), Vec3{});
    const std::vector<BoardContact> truth = {At("t1", 0.0, 30000.0, 1000.0)};
    Run(n, 0.0, 20.0, truth);
    SC_EXPECT(n.Known(20.0).size() == 1);
    SC_EXPECT(n.RequestSource("none", 20.0, "Commander"));
    Run(n, 20.5, 90.0, truth);
    SC_EXPECT(n.ActiveId().empty() && !n.Switching());
    SC_EXPECT(n.Known(90.0).size() == 1);                     // the plot of t=0 is still inside its life
    SC_EXPECT(std::fabs(n.Known(90.0)[0].time_s) < 1e-9);     // and nothing newer was heard
    Run(n, 90.5, 110.0, truth);
    SC_EXPECT(n.Known(110.0).empty());                        // older than 100 s: forgotten
}

SC_TEST("c2: when the main source goes silent the commander falls back, and returns once it has been steady") {
    C2Policy policy;
    policy.fallback_order = {"ewr"};
    policy.primary_stable_s = 60.0;
    C2SourceDef pu = Pu12();
    pu.outages.push_back({60.0, 400.0, "line cut"});
    C2Network n(policy, 1);
    n.AddSource(pu, Vec3{});
    n.AddSource(Make("ewr_voice", "ewr", Vec3{0.0, 10000.0, 0.0}), Vec3{});
    const std::vector<BoardContact> truth = {At("t1", 0.0, 30000.0, 1000.0)};
    Run(n, 0.0, 100.0, truth);
    SC_EXPECT(n.ActiveId() == "pu12");
    // the PU-12's last word was at 12 s; degraded after 2.5 periods (150 s), silent after 5 (300 s)
    Run(n, 100.5, 200.0, truth);
    C2State st = C2State::Up;
    for (const C2Network::SourceView& v : n.Views()) {
        if (v.id == "pu12") st = v.state;
    }
    SC_EXPECT(st == C2State::Degraded);
    SC_EXPECT(n.ActiveId() == "pu12");  // not yet: he waits for silence
    Run(n, 200.5, 330.0, truth);
    SC_EXPECT(n.ActiveId() == "ewr");   // 12 + 300 s, then the 6 s retune
    Run(n, 330.5, 440.0, truth);
    SC_EXPECT(n.ActiveId() == "ewr");   // the PU-12 is back at 400 s, but has not been steady for 60 s
    Run(n, 440.5, 480.0, truth);
    SC_EXPECT(n.ActiveId() == "pu12");  // steady from 400 s: he goes back at 460 s and has it on the net at 466 s
    const std::vector<C2Network::Event> ev = n.TakeEvents();
    SC_EXPECT(HasEvent(ev, "PU-12", "off the air (line cut)"));
    SC_EXPECT(HasEvent(ev, "PU-12", "back on the air"));
    SC_EXPECT(HasEvent(ev, "Commander", "PU-12 silent"));
    SC_EXPECT(HasEvent(ev, "Commander", "the main source is back"));
}

SC_TEST("c2: with no source answering the vehicle fights on its own radar and says so once") {
    C2Policy policy;
    policy.fallback_order = {"ewr"};
    C2SourceDef pu = Pu12();
    pu.outages.push_back({20.0, 1e18, "destroyed"});
    C2SourceDef ewr = Make("ewr_voice", "ewr", Vec3{0.0, 10000.0, 0.0});
    ewr.outages.push_back({20.0, 1e18, "jammed"});
    C2Network n(policy, 1);
    n.AddSource(pu, Vec3{});
    n.AddSource(ewr, Vec3{});
    const std::vector<BoardContact> truth = {At("t1", 0.0, 30000.0, 1000.0)};
    Run(n, 0.0, 500.0, truth);
    int told = 0;
    for (const C2Network::Event& e : n.TakeEvents()) told += e.text.find("no source answers") != std::string::npos ? 1 : 0;
    SC_EXPECT(told == 1);
}

SC_TEST("c2: the scenario can fail a source and bring it back; a failed source drops what it had queued") {
    C2Policy policy;
    policy.auto_fallback = false;
    C2Network n(policy, 1);
    n.AddSource(Pu12(), Vec3{});
    const std::vector<BoardContact> truth = {At("a", 0.0, 40000.0, 1000.0), At("b", 45000.0, 0.0, 1000.0)};
    Run(n, 0.0, 5.0, truth);
    n.SetFailed("pu12", true, 5.0, "destroyed");     // "a" was being read out (4-12 s), "b" was waiting (16-24 s)
    Run(n, 5.5, 40.0, truth);
    SC_EXPECT(n.Known(40.0).empty());                // the message on the air when the line went was lost
    SC_EXPECT(n.BoardOf("pu12")->LostCount() == 2);
    n.SetFailed("pu12", false, 40.0);
    Run(n, 40.5, 120.0, truth);                      // the plot due at 60 s is made late and told
    SC_EXPECT(n.Known(120.0).size() == 2);
}

namespace {

// A link that is never up for one source, or carries half its messages.
class FixedLink : public C2LinkModel {
public:
    FixedLink(std::string id, double p) : id_(std::move(id)), p_(p) {}
    double DeliveryProbability(const C2SourceDef& source, const Vec3&, double) override { return source.id == id_ ? p_ : 1.0; }

private:
    std::string id_;
    double p_;
};

int LostWith(double p, unsigned seed) {
    C2Policy policy;
    policy.auto_fallback = false;
    C2Network n(policy, seed);
    n.AddSource(Pu12(), Vec3{});
    n.AddSource(Make("ewr_voice", "ewr", Vec3{0.0, 10000.0, 0.0}), Vec3{});
    FixedLink link("ewr", p);
    n.SetLinkModel(&link);
    Run(n, 0.0, 900.0, {At("t1", 0.0, 30000.0, 1000.0)});
    return n.BoardOf("ewr")->LostCount();
}

}  // namespace

SC_TEST("c2: an EM link model decides which words get through, repeatably") {
    SC_EXPECT(LostWith(1.0, 1) == 0);
    const int all = LostWith(0.0, 1);
    SC_EXPECT(all >= 25);                  // 30 plots in 900 s, each with a message, none through
    const int half = LostWith(0.5, 1);
    SC_EXPECT(half > 5 && half < 25);
    SC_EXPECT(LostWith(0.5, 1) == half);   // the same seed gives the same losses
    std::printf("    lost with p = 1, 0.5, 0: %d, %d, %d of about 31\n", LostWith(1.0, 1), half, all);
    // a link that carries nothing makes the source silent
    C2Policy policy;
    policy.auto_fallback = false;
    C2Network n(policy, 1);
    n.AddSource(Make("ewr_voice", "ewr", Vec3{0.0, 10000.0, 0.0}), Vec3{});
    FixedLink link("ewr", 0.0);
    n.SetLinkModel(&link);
    Run(n, 0.0, 200.0, {At("t1", 0.0, 30000.0, 1000.0)});
    SC_EXPECT(n.Views()[0].state == C2State::Silent && n.Known(200.0).empty());
}

SC_TEST("c2: a source out of reach of the vehicle says nothing until the vehicle comes within its link range") {
    C2Policy policy;
    policy.auto_fallback = false;
    C2Network n(policy, 1);
    n.AddSource(Pu12(), Vec3{});
    n.AddSource(Make("observation_post", "obs", Vec3{0.0, 20000.0, 0.0}), Vec3{});  // link range 8 km
    const std::vector<BoardContact> truth = {At("t1", 0.0, 22000.0, 500.0)};
    Run(n, 0.0, 20.0, truth, Vec3{});
    SC_EXPECT(!n.Views()[1].available && n.BoardOf("obs")->Marks().empty());
    Run(n, 20.5, 40.0, truth, Vec3{0.0, 15000.0, 0.0});  // the vehicle drives up: 5 km from the post
    SC_EXPECT(n.Views()[1].available && !n.BoardOf("obs")->Marks().empty());
}

namespace {

std::string C2ScenarioText(const std::string& c2, const std::string& triggers_extra) {
    return std::string(R"({
      "scenario_id": "c2", "name": "C2", "source": "sam3d", "confidence": "estimate", "seed": 3,
      "origin": { "lat_deg": 45, "lon_deg": 25, "alt_m": 0 }, "terrain": { "type": "flat", "height_m": 0 },
      "command_post": "command/pu12.json",
      "c2": )") + c2 + R"(,
      "own_unit": { "console": "procedures/osa_ak.json", "tuning": "gameplay/osa_ak.json", "readiness": "deployed",
                    "position": { "east_m": 0, "north_m": 0, "up_m": 3 }, "inputs": { "vehicle_stopped": true } },
      "crew": [ { "name": "Commander", "role": "commander" }, { "name": "Deputy", "role": "deputy_commander" },
                { "name": "Senior operator", "role": "senior_operator" }, { "name": "Operator", "role": "operator" },
                { "name": "Driver", "role": "driver" } ],
      "targets": [ { "id": "t1", "name": "T", "class": "drone", "board_type": 7,
                     "start": { "east_m": 2000, "north_m": 45000, "up_m": 1000 },
                     "waypoints": [ { "east_m": 2000, "north_m": -30000, "up_m": 1000 } ], "speed_mps": 150 } ],
      "triggers": [ { "id": "tv", "when": {}, "do": [ { "order": "emcon_tv_azimuth", "crew": "Senior operator" },
                                                     { "order": "emcon_tv_engagement", "crew": "Deputy" } ] })" + triggers_extra + R"( ],
      "end": { "time_limit_s": 700, "when_targets_gone": false } })";
}

const char* kC2Block = R"({ "library": "command/c2_sources.json", "primary": "pu12", "fallback_order": ["ewr_north"],
        "sources": [ { "id": "ewr_north", "type": "ewr_voice", "position": { "east_m": 0, "north_m": 25000, "up_m": 0 }, "net": "battery_net" },
                     { "id": "pu12", "net": "battery_net" } ] })";

bool Load(const std::string& text, ScenarioDef& def, ConsoleData& console, std::string& error) {
    return LoadScenario(text, kData, def, console, &error);
}

}  // namespace

SC_TEST("c2: a scenario's c2 block builds the network; mistakes in it are refused with a path") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(Load(C2ScenarioText(kC2Block, ""), def, console, error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    SC_EXPECT(def.c2_sources.size() == 2 && def.c2_sources[0].id == "pu12" && def.c2_sources[1].id == "ewr_north");
    SC_EXPECT(def.c2_sources[1].has_position && def.c2_sources[1].net == "battery_net" && def.c2_sources[0].net == "battery_net");
    SC_EXPECT_NEAR(def.c2_policy.heard_life_s, 180.0, 1e-9);  // a network makes the crew forget stale designations
    auto bad = [&](const std::string& c2, const std::string& part, const std::string& extra = std::string()) {
        ScenarioDef d;
        ConsoleData c;
        std::string e;
        const bool ok = Load(C2ScenarioText(c2, extra), d, c, e);
        if (ok || e.find(part) == std::string::npos) std::printf("    expected '%s', got '%s'\n", part.c_str(), e.c_str());
        return !ok && e.find(part) != std::string::npos;
    };
    SC_EXPECT(bad(R"({ "library": "command/c2_sources.json", "sources": [ { "id": "x", "type": "nonsense" } ] })", "unknown source type"));
    SC_EXPECT(bad(R"({ "library": "command/c2_sources.json", "sources": [ { "id": "x", "type": "ewr_voice" } ] })", "place of its site"));
    SC_EXPECT(bad(R"({ "sources": [ { "id": "x" } ] })", "needs a library"));
    SC_EXPECT(bad(R"({ "fallback_order": ["ghost"] })", "unknown source 'ghost'"));
    SC_EXPECT(bad(R"({ "active": "ghost" })", "unknown source 'ghost'"));
    SC_EXPECT(bad(R"({ "library": "command/missing.json" })", "cannot open"));
    SC_EXPECT(bad(R"({ "sources": [ { "id": "pu12", "outages": [ { "from_s": 50, "to_s": 40 } ] } ] })", "after from_s"));
    SC_EXPECT(bad(kC2Block, "unknown source 'ghost'", R"(, { "id": "x", "when": {}, "do": [ { "c2_source": "ghost" } ] })"));
    SC_EXPECT(bad(kC2Block, "needs exactly one", R"(, { "id": "x", "when": {}, "do": [ { "c2_source": "pu12", "c2_fail": "pu12" } ] })"));
}

SC_TEST("c2: a PU-12 lost mid-engagement: the commander falls back to the EWR net and the crew aims by it") {
    // The PU-12 plots at 0 s and is told at 14 s. At 40 s it is destroyed. Its designation is forgotten after 180 s; at
    // 312 s (12 + 300) the commander gives up on it and changes to the EWR net; at 318 s the EWR is on the air.
    const std::string fail = R"(, { "id": "pu12_lost", "when": { "after_s": 40 }, "do": [ { "c2_fail": "pu12", "reason": "destroyed" } ] })";
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(Load(C2ScenarioText(kC2Block, fail), def, console, error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner runner(std::move(def), std::move(console));
    SC_EXPECT(runner.C2() != nullptr);
    if (runner.C2() == nullptr) return;
    for (int i = 0; i < 300; ++i) runner.Step(0.1);  // 30 s
    SC_EXPECT(runner.C2()->ActiveId() == "pu12" && runner.C2()->HeardFrom("t1") == "pu12");
    const double wheel_pu12 = runner.Console().panel.GetValue("ssc.az_wheel");
    for (int i = 0; i < 3200; ++i) runner.Step(0.1);  // to 350 s: the EWR plot of 330 s is told at 344 s
    SC_EXPECT(runner.C2()->ActiveId() == "ewr_north");
    SC_EXPECT(runner.C2()->HeardFrom("t1") == "ewr_north");
    const std::string debrief = runner.Debrief();
    SC_EXPECT(debrief.find("PU-12            event      link  off the air (destroyed)") != std::string::npos);
    SC_EXPECT(debrief.find("changes to") != std::string::npos && debrief.find("silent") != std::string::npos);
    SC_EXPECT(debrief.find("[EWR voice]") != std::string::npos);
    // the wheel follows the EWR's designation now (the PU-12 board has an offset of up to 5 degrees and 3 km; the EWR's
    // site is surveyed), so it points at the target's true bearing to within the EWR's plot error
    const std::vector<BoardMark> known = runner.C2()->Known(runner.Time());
    SC_EXPECT(known.size() == 1);
    if (known.size() == 1) {
        const double az = RadToDeg(AzimuthOf(known[0].position - runner.UnitPosition()));
        const double wheel = runner.Console().panel.GetValue("ssc.az_wheel");
        std::printf("    wheel before the loss %.2f deg, now %.2f deg; EWR designation at %.2f deg\n", wheel_pu12, wheel, az);
        SC_EXPECT(std::fabs(Wrap(wheel - az)) < 90.0);  // heading the right way; the wheel is rate-limited and the target is overhead by now
    }
}

SC_TEST("c2: a trigger or the player changes source; the debrief names who and why") {
    const std::string go = R"(, { "id": "tune", "when": { "after_s": 30 }, "do": [ { "c2_source": "ewr_north" } ] })";
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(Load(C2ScenarioText(kC2Block, go), def, console, error));
    ScenarioRunner runner(std::move(def), std::move(console));
    for (int i = 0; i < 400; ++i) runner.Step(0.1);  // 40 s
    SC_EXPECT(runner.C2()->ActiveId() == "ewr_north");
    SC_EXPECT(runner.Debrief().find("Scenario         event      ewr_north  changes to") != std::string::npos);
    // the player (through the UI) goes back to the PU-12
    SC_EXPECT(runner.RequestC2Source("pu12", "Player", "his picture is better"));
    for (int i = 0; i < 80; ++i) runner.Step(0.1);
    SC_EXPECT(runner.C2()->ActiveId() == "pu12");
    SC_EXPECT(runner.Debrief().find("Player           event      pu12  changes to") != std::string::npos);
    SC_EXPECT(runner.Debrief().find("his picture is better") != std::string::npos);
}

SC_TEST("c2: a scenario with no c2 block and a command post has the PU-12 alone, as before") {
    const std::string text = C2ScenarioText("{}", "");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(Load(text, def, console, error));
    SC_EXPECT(def.c2_sources.size() == 1 && def.c2_sources[0].id == "pu12");
    ScenarioRunner runner(std::move(def), std::move(console));
    SC_EXPECT(runner.Board() != nullptr && runner.C2() != nullptr);
    for (int i = 0; i < 300; ++i) runner.Step(0.1);
    SC_EXPECT(runner.C2()->ActiveId() == "pu12");
    SC_EXPECT(runner.Board()->Known(runner.Time()).size() == 1);
    SC_EXPECT(runner.C2()->Known(runner.Time()).size() == 1);
}

SC_TEST("c2: the shipped scenario loses the PU-12 and ends on the EWR net") {
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/osa_practice_c2.json", kData, def, console, &error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner runner(std::move(def), std::move(console));
    for (int i = 0; i < 4000; ++i) runner.Step(0.1);  // 400 s
    SC_EXPECT(runner.C2() != nullptr && runner.C2()->ActiveId() == "ewr_north");
}

SC_TEST("c2: with the EM layer a source's link is decided by propagation: near carries, far does not") {
    std::string text = C2ScenarioText(R"({ "library": "command/c2_sources.json", "primary": "pu12", "fallback_order": ["near", "far"],
        "sources": [ { "id": "near", "type": "ewr_voice", "position": { "east_m": 0, "north_m": 4000, "up_m": 0 }, "link_range_m": 200000 },
                     { "id": "far", "type": "ewr_voice", "position": { "east_m": 0, "north_m": 60000, "up_m": 0 }, "link_range_m": 200000 } ] })", "");
    const std::string from = "\"readiness\": \"deployed\",";
    text.replace(text.find(from), from.size(),
                 from + " \"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\" },");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(Load(text, def, console, error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner runner(std::move(def), std::move(console));
    SC_EXPECT(runner.Em() != nullptr && runner.C2() != nullptr);
    if (runner.Em() == nullptr) return;
    for (int i = 0; i < 20; ++i) runner.Step(0.1);
    const double pn = runner.Em()->RadioDeliveryProbability("near");
    const double pf = runner.Em()->RadioDeliveryProbability("far");
    std::printf("    delivery probability: near %.3f, far %.3f\n", pn, pf);
    SC_EXPECT(pn > 0.9);
    SC_EXPECT(pf < 0.05);
    SC_EXPECT(runner.Em()->RadioDeliveryProbability("pu12") < 0.0);  // no place, no EM radio: unaffected
}

SC_TEST("c2: a VHF barrage jammer cuts a source's EM link; a PU-12 with a place is subject to it too") {
    std::string text = C2ScenarioText(R"({ "library": "command/c2_sources.json", "primary": "pu12",
        "sources": [ { "id": "pu12", "position": { "east_m": 0, "north_m": 6000, "up_m": 0 } } ] })", "");
    const std::string from = "\"readiness\": \"deployed\",";
    text.replace(text.find(from), from.size(),
                 from + " \"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\","
                        " \"extra_libraries\": [\"em/generic.json\"],"
                        " \"jammers\": [ { \"jammer\": \"generic_vhf_barrage\", \"position\": { \"east_m\": 500, \"north_m\": 1500, \"up_m\": 0 },"
                        " \"on_at_s\": 5 } ] },");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(Load(text, def, console, error));
    if (!error.empty()) std::printf("    %s\n", error.c_str());
    ScenarioRunner runner(std::move(def), std::move(console));
    if (runner.Em() == nullptr) {
        SC_EXPECT(false);
        return;
    }
    for (int i = 0; i < 20; ++i) runner.Step(0.1);  // 2 s: jammer still off
    const double before = runner.Em()->RadioDeliveryProbability("pu12");
    for (int i = 0; i < 100; ++i) runner.Step(0.1);  // 12 s: on
    const double after = runner.Em()->RadioDeliveryProbability("pu12");
    std::printf("    PU-12 link: %.3f jammer off, %.3f jammer on\n", before, after);
    SC_EXPECT(before > 0.9);
    SC_EXPECT(after < 0.5);
}

SC_TEST("c2: designations on an EM radio go on the air as voice messages: radiated, delayed, and lost to a jammer") {
    std::string text = C2ScenarioText(R"({ "library": "command/c2_sources.json", "primary": "pu12",
        "sources": [ { "id": "pu12", "position": { "east_m": 0, "north_m": 6000, "up_m": 0 } } ] })", "");
    const std::string from = "\"readiness\": \"deployed\",";
    text.replace(text.find(from), from.size(),
                 from + " \"em\": { \"library\": \"em/osa_ak.json\", \"search_radar\": \"osa_ak_soc\", \"track_radar\": \"osa_ak_ssc\","
                        " \"extra_libraries\": [\"em/generic.json\"],"
                        " \"jammers\": [ { \"jammer\": \"generic_vhf_barrage\", \"position\": { \"east_m\": 500, \"north_m\": 1500, \"up_m\": 0 },"
                        " \"on_at_s\": 100 } ] },");
    ScenarioDef def;
    ConsoleData console;
    std::string error;
    SC_EXPECT(Load(text, def, console, error));
    ScenarioRunner runner(std::move(def), std::move(console));
    if (runner.Em() == nullptr) {
        SC_EXPECT(false);
        return;
    }
    for (int i = 0; i < 300; ++i) runner.Step(0.1);  // 30 s, jammer off
    // the plot of 0 s was read out 4-12 s (EM message), delivered, and told at 14 s
    SC_EXPECT(runner.C2()->HeardFrom("t1") == "pu12");
    int sent = 0, delivered = 0;
    for (const em::EmEvent& e : runner.Em()->World().Scene().events) {
        if (e.kind == em::EmEventKind::MessageSent) ++sent;
        if (e.kind == em::EmEventKind::MessageDelivered) ++delivered;
    }
    std::printf("    EM events: %d voice messages sent, %d delivered\n", sent, delivered);
    SC_EXPECT(sent >= 1 && delivered >= 1);
    // the plot of 120 s goes out under the jammer: lost
    for (int i = 0; i < 1500; ++i) runner.Step(0.1);  // to 180 s
    SC_EXPECT(runner.C2()->BoardOf("pu12")->LostCount() >= 1);
    SC_EXPECT(runner.Debrief().find("PU-12") != std::string::npos);
}

SC_TEST("c2: the two R-123M sets: presets, the standby is monitored, a source on no preset needs a retune") {
    C2Policy policy;
    policy.auto_fallback = false;
    policy.select_s = 3.0;
    policy.retune_s = 30.0;
    C2Network n(policy, 1);
    n.AddSource(Pu12(), Vec3{});
    n.AddSource(Make("ewr_voice", "ewr", Vec3{0.0, 10000.0, 0.0}), Vec3{});
    for (const char* id : {"a", "b", "c"}) n.AddSource(Make("observation_post", id, Vec3{0.0, 3000.0, 0.0}), Vec3{});
    const std::vector<BoardContact> truth = {At("t1", 0.0, 30000.0, 1000.0)};
    Run(n, 0.0, 1.0, truth);
    // the plan: pu12, ewr, a, b on presets 1-4; left on 1 (listening), right on 2; "c" is on no preset
    SC_EXPECT(n.PresetSource(1) == "pu12" && n.PresetSource(2) == "ewr" && n.PresetSource(3) == "a" && n.PresetSource(4) == "b");
    const std::vector<C2Network::RadioView> r = n.Radios();
    SC_EXPECT(r[0].preset == 1 && r[0].listening && r[1].preset == 2 && !r[1].listening && r[1].source == "ewr");
    SC_EXPECT(n.Monitored("pu12") && n.Monitored("ewr") && !n.Monitored("a"));
    // "ewr" is on the standby set: select_s only
    SC_EXPECT(n.RequestSource("ewr", 1.0, "Commander"));
    Run(n, 1.5, 4.5, truth);
    SC_EXPECT(n.ActiveId() == "ewr" && n.Radios()[1].listening);
    // "a" is on a preset but on neither set: the standby (now the left set) turns to preset 3: select_s
    SC_EXPECT(n.RequestSource("a", 5.0, "Commander"));
    Run(n, 5.5, 8.5, truth);
    SC_EXPECT(n.ActiveId() == "a" && n.Radios()[0].preset == 3 && n.Radios()[0].listening);
    // "c" is on no preset: the standby set is re-tuned onto it, 30 s, and the old source is heard meanwhile
    SC_EXPECT(n.RequestSource("c", 10.0, "Commander"));
    Run(n, 10.5, 39.5, truth);
    SC_EXPECT(n.ActiveId() == "a" && n.Switching());
    Run(n, 40.0, 41.0, truth);
    SC_EXPECT(n.ActiveId() == "c" && !n.Switching());
    SC_EXPECT(n.PresetSource(n.Radios()[1].preset) == "c");
    // the sets can also be turned by hand
    SC_EXPECT(!n.TuneSet(0, 9, 41.0, "Player") && !n.TuneSet(5, 1, 41.0, "Player"));
    SC_EXPECT(n.TuneSet(0, 1, 41.0, "Player"));
    Run(n, 41.5, 45.0, truth);
    SC_EXPECT(n.Radios()[0].preset == 1 && n.Monitored("pu12") && n.ActiveId() == "c");
    SC_EXPECT(n.ListenToSet(0, 45.0, "Player"));
    Run(n, 45.5, 49.0, truth);
    SC_EXPECT(n.ActiveId() == "pu12");
}

SC_TEST("c2: a silent source that is on no set is found by turning the standby set to it") {
    C2Policy policy;
    policy.fallback_order = {"ewr", "a"};
    policy.presets = {"pu12", "ewr", "a"};
    C2SourceDef pu = Pu12();
    pu.outages.push_back({20.0, 1e18, "destroyed"});
    C2SourceDef ewr = Make("ewr_voice", "ewr", Vec3{0.0, 10000.0, 0.0});
    ewr.outages.push_back({20.0, 1e18, "jammed"});
    C2Network n(policy, 1);
    n.AddSource(pu, Vec3{});
    n.AddSource(ewr, Vec3{});
    n.AddSource(Make("observation_post", "a", Vec3{0.0, 3000.0, 0.0}), Vec3{});
    Run(n, 0.0, 600.0, {At("t1", 500.0, 2500.0, 500.0)});
    // pu12 and ewr (the two sets) go silent; the commander turns the standby to preset 3, finds "a" alive and moves to it
    SC_EXPECT(n.ActiveId() == "a");
    bool probed = false;
    for (const C2Network::Event& e : n.TakeEvents()) probed = probed || e.text.find("listens for") != std::string::npos;
    SC_EXPECT(probed);
}
