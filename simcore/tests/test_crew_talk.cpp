// The crew's talk (roadmap 8.15, docs/crew/15_TALK.md): turn-taking on the bus, the ARM moment's one order, stress graded
// from what each man heard, the voice it gives him, the radial menu's layout, the player's replies and the glosses.
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "simcore/console_data.hpp"
#include "simcore/crew_callouts.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/crew_voice.hpp"
#include "simcore/helper.hpp"
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

struct Crew {
    ConsoleData console;
    CrewData crew;
    VoiceData voices;
    bool ok = false;
};

Crew LoadCrew(const std::string& system) {
    Crew o;
    std::string e;
    o.ok = LoadConsoleFile(kData + "/procedures/" + system + ".json", o.console, &e) &&
           LoadCrewDataFile(kData + "/crew/" + system + "_crew.json", o.console, o.crew, &e) &&
           LoadVoiceDataFile(kData + "/crew/voices.json", &o.crew, o.voices, &e);
    if (!o.ok) std::printf("    load: %s\n", e.c_str());
    return o;
}

std::unique_ptr<ScenarioRunner> Runner(const std::string& file, unsigned seed = 1) {
    ScenarioDef def;
    ConsoleData console;
    std::string err;
    if (!LoadScenario(ReadFile(kData + "/scenarios/" + file), kData, def, console, &err)) {
        std::printf("    load %s: %s\n", file.c_str(), err.c_str());
        return nullptr;
    }
    def.seed = seed;
    return std::make_unique<ScenarioRunner>(std::move(def), std::move(console));
}

const SpokenCallout* Find(const std::vector<SpokenCallout>& log, const std::string& id) {
    for (const SpokenCallout& s : log) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

void StepTo(CalloutBus& bus, double from_s, double to_s) {
    for (double t = from_s; t <= to_s + 1e-9; t += 0.05) bus.Step(t);
}

}  // namespace

SC_TEST("crew talk: a moment between speakers; an urgent line breaks in and the cut line is said again whole") {
    Crew o = LoadCrew("osa_ak");
    SC_EXPECT(o.ok);
    CalloutBus bus(&o.crew);
    SC_EXPECT(bus.Say(0.0, "order_search", "commander"));
    SC_EXPECT(bus.Say(0.0, "captured", "operator"));
    bus.Step(0.0);
    SC_EXPECT(bus.Now().size() == 1u && bus.Now()[0].id == "captured");  // the urgent one first
    StepTo(bus, 0.05, 4.0);
    const SpokenCallout* first = Find(bus.Log(), "captured");
    const SpokenCallout* next = Find(bus.Log(), "order_search");
    SC_EXPECT(first != nullptr && next != nullptr);
    if (first != nullptr && next != nullptr) {
        SC_EXPECT(next->start_s >= first->end_s + CalloutBus::kTurnGapS - 1e-9);
        SC_EXPECT(next->start_s < first->end_s + CalloutBus::kTurnGapS + 0.05 + 1e-9);
    }
    // Break-in: "Срыв сопровождения!" during a routine order cuts it off; the order is said again after, whole.
    CalloutBus cut(&o.crew);
    SC_EXPECT(cut.Say(0.0, "order_search_on_march", "commander"));
    cut.Step(0.0);
    SC_EXPECT(cut.Say(0.3, "lock_lost", "operator"));
    StepTo(cut, 0.3, 8.0);
    SC_EXPECT(cut.CutOff().size() == 1u && cut.CutOff()[0].id == "order_search_on_march" && cut.CutOff()[0].cut);
    SC_EXPECT(cut.Log().size() == 2u);
    if (cut.Log().size() == 2u) {
        SC_EXPECT(cut.Log()[0].id == "lock_lost" && cut.Log()[0].start_s < 0.3 + 1e-9);
        SC_EXPECT(cut.Log()[1].id == "order_search_on_march" && cut.Log()[1].start_s >= cut.Log()[0].end_s + CalloutBus::kTurnGapS - 1e-9);
        SC_EXPECT(!cut.Log()[1].cut);
    }
    // An urgent line never cuts another urgent one: it waits its turn.
    CalloutBus both(&o.crew);
    SC_EXPECT(both.Say(0.0, "lock_lost", "operator") && both.Say(0.0, "missile_away", "deputy_commander"));
    StepTo(both, 0.0, 6.0);
    SC_EXPECT(both.CutOff().empty() && both.Log().size() == 2u);
}

SC_TEST("crew talk: a line left waiting goes stale (never the player's); one caller per fact") {
    SC_EXPECT(CalloutBus::ShelfS(CalloutKind::Ack, 2, false) == 5.0);
    SC_EXPECT(CalloutBus::ShelfS(CalloutKind::Remind, 1, false) == 10.0);
    SC_EXPECT(CalloutBus::ShelfS(CalloutKind::Report, 1, true) == 8.0);
    SC_EXPECT(CalloutBus::ShelfS(CalloutKind::Report, 1, false) == 15.0);
    SC_EXPECT(CalloutBus::ShelfS(CalloutKind::Order, 1, false) < 0.0);
    SC_EXPECT(CalloutBus::ShelfS(CalloutKind::Report, 0, false) < 0.0);  // urgent: always said
    SC_EXPECT(CalloutBus::ShelfS(CalloutKind::Radio, 1, false) < 0.0);
    Crew o = LoadCrew("osa_ak");
    SC_EXPECT(o.ok);
    // Three contacts ahead of an «Есть!» (less urgent): it waits past its 5 s and isn't said.
    const auto busy = [](CalloutBus& bus) {
        for (const char* az : {"05-40", "12-10", "33-00"}) bus.Say(0.0, "contact", "senior_operator", {{"azimuth", az}, {"range_km", "24"}});
        bus.Say(0.0, "ack", "operator");
        StepTo(bus, 0.0, 20.0);
    };
    CalloutBus bus(&o.crew);
    busy(bus);
    SC_EXPECT(bus.Dropped() == 1 && Find(bus.Log(), "ack") == nullptr && bus.Log().size() == 3u);
    CalloutBus player(&o.crew);
    player.SetPlayerSeat([](const std::string& station) { return station == "operator"; });
    busy(player);
    SC_EXPECT(player.Dropped() == 0 && Find(player.Log(), "ack") != nullptr);
    // The same report in the same words from a second seat is not said again; other words are.
    CalloutBus fact(&o.crew);
    SC_EXPECT(fact.Say(0.0, "jamming", "senior_operator", {{"azimuth", "05-40"}}));
    SC_EXPECT(fact.Say(0.5, "jamming", "operator", {{"azimuth", "05-40"}}));
    SC_EXPECT(fact.Say(0.5, "jamming", "operator", {{"azimuth", "20-00"}}));
    StepTo(fact, 0.0, 10.0);
    SC_EXPECT(fact.Merged() == 1 && fact.Log().size() == 2u);
    // Long after, it is news again.
    SC_EXPECT(fact.Say(30.0, "jamming", "operator", {{"azimuth", "05-40"}}));
    StepTo(fact, 30.0, 35.0);
    SC_EXPECT(fact.Merged() == 1 && fact.Log().size() == 3u);
}

SC_TEST("crew talk: an NPC waits while another channel he listens to talks in his ear; the player doesn't") {
    Crew o = LoadCrew("osa_ak");
    SC_EXPECT(o.ok);
    const auto play = [&o](bool commander_is_player) {
        CalloutBus bus(&o.crew);
        bus.SetRouter([](const CalloutDef& def, const std::string&) {
            CalloutBus::Route r;
            r.channel = def.kind == CalloutKind::Radio ? "battery net" : "intercom";
            return r;
        });
        bus.SetListens([](const std::string& station, const std::string& channel) {
            return channel == "intercom" || (station == "commander" && channel == "battery net");
        });
        if (commander_is_player) bus.SetPlayerSeat([](const std::string& station) { return station == "commander"; });
        bus.Say(0.0, "order_assign", "battery", {{"callsign", "21"}, {"number", "3"}});
        bus.Say(0.0, "order_search", "commander");
        bus.Say(0.0, "contact", "senior_operator", {{"azimuth", "05-40"}, {"range_km", "24"}});
        StepTo(bus, 0.0, 10.0);
        const SpokenCallout* net = Find(bus.Log(), "order_assign");
        const SpokenCallout* order = Find(bus.Log(), "order_search");
        const SpokenCallout* report = Find(bus.Log(), "contact");
        SC_EXPECT(net != nullptr && order != nullptr && report != nullptr);
        if (net == nullptr || order == nullptr || report == nullptr) return false;
        // The senior operator doesn't listen to the net: while the commander waits, he speaks.
        if (!commander_is_player) SC_EXPECT(report->start_s < 1e-9);
        return order->start_s >= net->end_s - 1e-9;
    };
    SC_EXPECT(play(false));
    SC_EXPECT(!play(true));
}

SC_TEST("crew talk: the ARM moment is one order; the target is let go only once the transmitters are off") {
    auto r = Runner("osa_arm_drill.json", 1);
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    r->RunToEnd(0.1, 1200.0);
    const std::vector<SpokenCallout>& log = r->Callouts()->Log();
    const SpokenCallout* off = Find(log, "order_emcon_off");
    SC_EXPECT(off != nullptr);
    if (off == nullptr) return;
    int offs = 0;
    for (const SpokenCallout& s : log) {
        if (s.id == "order_emcon_off" && s.start_s - off->start_s < 10.0) ++offs;
        // "Излучение выключить!" covers the tracking transmitter: no separate order with it.
        if (s.id == "order_ssc_off") SC_EXPECT(s.start_s < off->start_s - 5.0 || s.start_s > off->start_s + 5.0);
        if (s.id == "order_drop_target" && s.start_s > off->start_s) {
            std::printf("    radiation off at %.1f s, the target let go at %.1f s\n", off->start_s, s.start_s);
            SC_EXPECT(s.start_s >= off->start_s + 3.0);
        }
    }
    SC_EXPECT(offs == 1);
}

SC_TEST("crew talk: silent against the ARM, the commander orders nothing that radiates, and doesn't repeat himself") {
    // NATO's DEAD raid with a player at the senior operator's seat doing what the helper shows (the run of
    // helper_coverage that found it): the crew on a jammer by method Н when the ARM is seen. Before, the commander
    // ordered method Н again every 10 s against his own «Излучение выключить!», and the target was never let go.
    ScenarioDef def;
    ConsoleData console;
    std::string err;
    SC_EXPECT(LoadScenarioFile(kData + "/scenarios/air_war_nato_dead.json", kData, def, console, &err));
    if (!err.empty()) std::printf("    load: %s\n", err.c_str());
    ScenarioRunner r(std::move(def), std::move(console));
    r.TakeSeat("senior_operator", "Player");
    r.SetAssistLevel(AssistLevel::Guided);
    r.SetHelper(true);
    HelpFollower f("senior_operator");
    // The first ARM comes at about 1080 s since the crew is told its emission regime and reminded of it (16_EMISSION.md):
    // their talk moves the fight's timings (before, about 600 s).
    while (!r.Finished() && r.Time() < 1300.0 && Find(r.Callouts()->Log(), "order_emcon_off") == nullptr) {
        r.Step(0.1);
        f.Update(r);
    }
    for (int i = 0; i < 700 && !r.Finished(); ++i) {  // the minute after it
        r.Step(0.1);
        f.Update(r);
    }
    const std::vector<SpokenCallout>& log = r.Callouts()->Log();
    const SpokenCallout* off = Find(log, "order_emcon_off");
    SC_EXPECT(off != nullptr);
    if (off == nullptr) return;
    int offs = 0;
    for (const SpokenCallout& s : log) {
        if (s.start_s < off->start_s || s.start_s > off->start_s + 60.0) continue;
        if (s.id == "order_emcon_off") ++offs;
        SC_EXPECT(s.id != "order_jammer" && s.id != "order_take_target" && s.id != "order_prepare");
        if (s.id == "order_drop_target") std::printf("    radiation off at %.1f s, the target let go at %.1f s\n", off->start_s, s.start_s);
    }
    std::printf("    «Излучение выключить!» %d time(s) in the minute after the first\n", offs);
    SC_EXPECT(offs <= 2);
}

SC_TEST("crew talk: stress graded from what each man heard; it fades; a war starts tense, an exercise calm") {
    // A crewman's stress in proportion: his skill's stress factor at full stress, none when calm.
    auto practice = Runner("osa_practice.json");
    SC_EXPECT(practice != nullptr);
    if (practice == nullptr) return;
    practice->Step(0.1);
    for (const CrewMember& c : practice->CrewMembers()) {
        SC_EXPECT(c.StressLevel() == 0.0 && c.StressMultiplier() == 1.0);
    }
    CrewMember m = practice->CrewMembers().front();
    m.SetStressLevel(0.5);
    SC_EXPECT(m.Stressed());
    const double full = 1.0 + (m.StressMultiplier() - 1.0) * 2.0;
    m.SetStressLevel(1.0);
    SC_EXPECT_NEAR(m.StressMultiplier(), full, 1e-9);
    m.SetStressLevel(2.0);
    SC_EXPECT(m.StressLevel() == 1.0);
    // The ARM drill is set in a war: tense from the start; a launch heard sends it near the top, then it fades.
    auto r = Runner("osa_arm_drill.json", 1);
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    r->Step(0.1);
    r->Step(0.1);
    for (const CrewMember& c : r->CrewMembers()) SC_EXPECT_NEAR(c.StressLevel(), 0.2, 1e-9);
    double launch_heard = -1.0, peak = 0.0, after = -1.0;
    while (!r->Finished() && r->Time() < 1200.0) {
        r->Step(0.1);
        if (launch_heard < 0.0) {
            for (const SpokenCallout& s : r->Callouts()->Log()) {
                if (s.id.compare(0, 4, "arm_") == 0) launch_heard = s.end_s;  // a launch, or the turn away that warns of one
            }
        }
        for (const CrewMember& c : r->CrewMembers()) {
            if (c.Role() == "commander") {
                peak = std::max(peak, c.StressLevel());
                if (launch_heard > 0.0 && r->Time() > launch_heard + 300.0 && after < 0.0) after = c.StressLevel();
            }
        }
    }
    std::printf("    ARM sign heard at %.0f s; the commander's stress peaked at %.2f, %.2f five minutes on\n", launch_heard, peak, after);
    SC_EXPECT(launch_heard > 0.0);
    SC_EXPECT(peak >= 0.9 - 1e-9);
    if (after >= 0.0) SC_EXPECT(after < 0.5);
}

SC_TEST("crew talk: the voice in proportion to stress, louder; the intercom through the throat microphones") {
    Crew o = LoadCrew("osa_ak");
    SC_EXPECT(o.ok);
    SC_EXPECT(o.voices.stress_gain > 0.0);
    const VoiceSettings calm = VoiceFor(o.voices, "commander", 0.0, "intercom");
    const VoiceSettings half = VoiceFor(o.voices, "commander", 0.5, "intercom");
    const VoiceSettings full = VoiceFor(o.voices, "commander", 1.0, "intercom");
    SC_EXPECT(full.pitch == calm.pitch + o.voices.stress_pitch && full.rate == calm.rate + o.voices.stress_rate);
    SC_EXPECT(half.pitch >= calm.pitch && half.pitch <= full.pitch);
    SC_EXPECT_NEAR(calm.volume, 1.0, 1e-9);
    SC_EXPECT_NEAR(half.volume, 1.0 + o.voices.stress_gain * 0.5, 1e-9);
    SC_EXPECT_NEAR(full.volume, 1.0 + o.voices.stress_gain, 1e-9);
    // Each headset of the R-124's kit is a laryngophone set: the intercom is the throat microphone's voice; a radio net
    // and a word said aloud are not.
    SC_EXPECT(calm.throat);
    SC_EXPECT(!VoiceFor(o.voices, "commander", 0.0, "battery net").throat);
    SC_EXPECT(!VoiceFor(o.voices, "commander", 0.0, "cabin").throat);
    // The bool form is the two ends.
    SC_EXPECT(VoiceFor(o.voices, "commander", true, "intercom").pitch == full.pitch);
}

SC_TEST("crew talk: the radial menu holds every line a seat can say, at most 8 sectors of at most 8 lines") {
    for (const char* system : {"osa_ak", "pu12m"}) {
        Crew o = LoadCrew(system);
        SC_EXPECT(o.ok);
        SC_EXPECT(!o.crew.menu.empty() && o.crew.menu.size() <= kMenuSectors);
        for (const CrewSeatDef& seat : o.crew.seats) {
            // What the commander says on the air himself, as ScenarioRunner::PlayerRadioReports offers it.
            std::vector<std::string> radio;
            if (seat.station == o.crew.commander) {
                for (const char* id : {"report_ready", "readback", "report_arm_attack"}) {
                    const CalloutDef* c = o.crew.FindCallout(id);
                    if (c != nullptr && c->from == seat.station) radio.push_back(id);
                }
            }
            const std::vector<std::string> lines = MenuLines(o.crew, seat.station, radio);
            std::vector<std::string> unplaced;
            const std::vector<std::vector<std::string>> rings = PlaceInMenu(o.crew, lines, &unplaced);
            for (const std::string& u : unplaced) std::printf("    %s %s: '%s' has no sector\n", system, seat.station.c_str(), u.c_str());
            SC_EXPECT(unplaced.empty());
            std::size_t placed = 0;
            for (std::size_t k = 0; k < rings.size(); ++k) {
                placed += rings[k].size();
                if (rings[k].size() > kMenuSectorLines) {
                    std::printf("    %s %s: sector '%s' holds %zu lines\n", system, seat.station.c_str(), o.crew.menu[k].id.c_str(), rings[k].size());
                }
                SC_EXPECT(rings[k].size() <= kMenuSectorLines);
            }
            SC_EXPECT(placed == lines.size());
        }
    }
    // Who carries an order out: one seat's drill, or the crew's.
    Crew o = LoadCrew("osa_ak");
    const OrderDef* search = nullptr;
    const OrderDef* silent = nullptr;
    for (const OrderDef& d : o.crew.orders) {
        if (d.callout == "order_search") search = &d;
        if (d.callout == "order_emcon_off") silent = &d;
    }
    SC_EXPECT(search != nullptr && OrderDoer(o.crew, *search) == "senior_operator");
    SC_EXPECT(silent != nullptr && OrderDoer(o.crew, *silent) == "crew");
    // A menu naming a line twice, or one the catalogue doesn't have, is refused.
    std::string text = ReadFile(kData + "/crew/osa_ak_crew.json");
    const std::string from = "\"items\": [\"order_prepare\"";
    const std::size_t at = text.find(from);
    SC_EXPECT(at != std::string::npos);
    if (at == std::string::npos) return;
    CrewData bad;
    std::string e;
    SC_EXPECT(!LoadCrewData(std::string(text).replace(at, from.size(), "\"items\": [\"order_prepare\", \"order_search\""), o.console, bad, &e));
    SC_EXPECT(e.find("order_search") != std::string::npos);
    SC_EXPECT(!LoadCrewData(std::string(text).replace(at, from.size(), "\"items\": [\"order_no_such_line\""), o.console, bad, &e));
}

SC_TEST("crew talk: the player's replies; «Повторите!» has the last line to him said again") {
    auto r = Runner("osa_practice.json");
    SC_EXPECT(r != nullptr);
    if (r == nullptr) return;
    SC_EXPECT(r->TakeSeat("operator", "p1"));
    r->Step(0.1);
    SC_EXPECT(!r->PlayerReply("operator", "say_again"));  // nothing said to him yet
    SC_EXPECT(r->LastRefusal().find("nothing said") != std::string::npos);
    SC_EXPECT(!r->PlayerReply("operator", "fire"));      // not a reply
    const SpokenCallout* heard = nullptr;
    while (heard == nullptr && r->Time() < 300.0) {
        r->Step(0.1);
        for (const SpokenCallout& s : r->Callouts()->Log()) {
            if (s.from != "operator" && (s.to == "operator" || s.to == "crew") && r->CalloutHeardAt(s, "operator")) heard = &s;
        }
    }
    SC_EXPECT(heard != nullptr);
    if (heard == nullptr) return;
    const std::string id = heard->id;
    const auto repeats = [&r, &id]() {
        int n = 0;
        for (const SpokenCallout& s : r->Callouts()->Log()) n += s.id == id && s.repeats > 0 ? 1 : 0;
        return n;
    };
    const int before = r->SayAgains();
    const int said_again = repeats();
    SC_EXPECT(r->PlayerReply("operator", "say_again"));
    SC_EXPECT(r->SayAgains() == before + 1);
    bool repeated = false;
    for (int i = 0; i < 200 && !repeated; ++i) {
        r->Step(0.1);
        repeated = repeats() > said_again;
    }
    std::printf("    the operator asked '%s' again\n", id.c_str());
    SC_EXPECT(repeated);
    SC_EXPECT(r->PlayerReply("operator", "ack"));
}

SC_TEST("crew talk: a scenario says whether it is an exercise or a war") {
    auto practice = Runner("osa_practice.json");
    auto bekaa = Runner("osa_bekaa_1982.json");
    SC_EXPECT(practice != nullptr && practice->Def().setting == "exercise");
    SC_EXPECT(bekaa != nullptr && bekaa->Def().setting == "war");
    std::string text = ReadFile(kData + "/scenarios/osa_bekaa_1982.json");
    const std::size_t at = text.find("\"setting\": \"war\"");
    SC_EXPECT(at != std::string::npos);
    if (at == std::string::npos) return;
    ScenarioDef def;
    ConsoleData console;
    std::string e;
    SC_EXPECT(!LoadScenario(text.replace(at, 16, "\"setting\": \"duel\""), kData, def, console, &e));
    SC_EXPECT(e.find("setting") != std::string::npos);
}

SC_TEST("crew talk: a heard line glossed: its abbreviations whole words only, an azimuth in degrees") {
    Crew o = LoadCrew("osa_ak");
    SC_EXPECT(o.ok);
    const std::string arm = GlossLine("Пуск ПРР! Азимут 35-00!", o.voices);
    SC_EXPECT(arm.find("ПРР = anti-radiation missile") != std::string::npos);
    SC_EXPECT(arm.find("35-00 = 210°") != std::string::npos);
    const std::string apu = GlossLine("АПУ расстопорить!", o.voices);
    SC_EXPECT(apu.find("АПУ = ") != std::string::npos && apu.find("control post") == std::string::npos);  // not ПУ in АПУ
    SC_EXPECT(GlossLine("Поиск!", o.voices).empty());
    SC_EXPECT(GlossLine("Цель, азимут 59-99, дальность 24!", o.voices).find("59-99 = 0°") != std::string::npos);
    SC_EXPECT(GlossLine("Номер 123-456", o.voices).empty());  // not an azimuth
    // Every abbreviation the crews say (a word of two or more capital Cyrillic letters) is glossed, or knowingly left out
    // until a source gives what it stands for.
    const auto capital = [](const std::string& s, std::size_t i) {
        if (i + 1 >= s.size() || static_cast<unsigned char>(s[i]) != 0xD0) return false;
        const unsigned char b = static_cast<unsigned char>(s[i + 1]);
        return b == 0x81 || (b >= 0x90 && b <= 0xAF);
    };
    for (const char* system : {"osa_ak", "pu12m"}) {
        Crew c = LoadCrew(system);
        for (const CalloutDef& d : c.crew.callouts) {
            const std::string& s = d.ru;
            for (std::size_t i = 0; i < s.size();) {
                std::size_t j = i;
                while (capital(s, j)) j += 2;
                const bool alone = (i == 0 || static_cast<unsigned char>(s[i - 1]) < 0x80) &&
                                   (j >= s.size() || static_cast<unsigned char>(s[j]) < 0x80);
                if (j - i >= 4 && alone) {
                    const std::string word = s.substr(i, j - i);
                    const bool known = o.voices.gloss.count(word) == 1u || word == "СРП" || word == "АСПД";
                    if (!known) std::printf("    %s '%s': '%s' has no gloss\n", system, d.id.c_str(), word.c_str());
                    SC_EXPECT(known);
                }
                i = j > i ? j : i + 1;
            }
        }
    }
    SC_EXPECT(o.voices.gloss.count("СРП") == 0u && o.voices.gloss.count("АСПД") == 0u);
    SC_EXPECT(o.voices.gloss.count("ССЦ") == 1u && o.voices.gloss.count("Р-111") == 1u);
}
