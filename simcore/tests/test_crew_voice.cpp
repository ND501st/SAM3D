// The crew's voices (roadmap 8.10, docs/crew/10_BODIES_AND_VOICES.md): the words a speech engine says for a callout,
// each seat's voice, and the bus's "now speaking" for the voice and the bodies.
#include <cstdio>
#include <string>

#include "simcore/console_data.hpp"
#include "simcore/crew_callouts.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/crew_voice.hpp"
#include "test_framework.hpp"

using namespace simcore;

namespace {

const std::string kData = SIMCORE_DATA_DIR;

struct OsaVoices {
    ConsoleData console;
    CrewData crew;
    VoiceData voices;
    bool ok = false;
};

OsaVoices LoadOsaVoices() {
    OsaVoices o;
    std::string e;
    o.ok = LoadConsoleFile(kData + "/procedures/osa_ak.json", o.console, &e) &&
           LoadCrewDataFile(kData + "/crew/osa_ak_crew.json", o.console, o.crew, &e) &&
           LoadVoiceDataFile(kData + "/crew/voices.json", &o.crew, o.voices, &e);
    if (!o.ok) std::printf("    load: %s\n", e.c_str());
    return o;
}

SpokenCallout Line(const std::string& ru, const std::string& en) {
    SpokenCallout s;
    s.ru = ru;
    s.en = en;
    s.channel = "intercom";
    return s;
}

}  // namespace

SC_TEST("crew voice: Russian numbers in words") {
    SC_EXPECT(RussianNumber(0) == "ноль");
    SC_EXPECT(RussianNumber(7) == "семь");
    SC_EXPECT(RussianNumber(12) == "двенадцать");
    SC_EXPECT(RussianNumber(20) == "двадцать");
    SC_EXPECT(RussianNumber(35) == "тридцать пять");
    SC_EXPECT(RussianNumber(100) == "сто");
    SC_EXPECT(RussianNumber(240) == "двести сорок");
    SC_EXPECT(RussianNumber(999) == "девятьсот девяносто девять");
}

SC_TEST("crew voice: azimuths, ranges and board numbers read as the crew says them") {
    OsaVoices o = LoadOsaVoices();
    SC_EXPECT(o.ok);
    SpokenCallout c = Line("Цель, азимут 35-00, дальность 20!", "Target, azimuth 35-00, range 20!");
    SC_EXPECT(SpeechText(c, "ru", o.voices) == "Цель, азимут тридцать пять ноль-ноль, дальность двадцать!");
    SC_EXPECT(SpeechText(c, "en", o.voices) == "Target, azimuth three five, zero zero, range 20!");
    c = Line("Помеха, азимут 05-40!", "Jamming, azimuth 05-40!");
    SC_EXPECT(SpeechText(c, "ru", o.voices) == "Помеха, азимут ноль пять сорок!");
    c = Line("Цель номер 0012 — взять!", "Take target 0012!");
    SC_EXPECT(SpeechText(c, "ru", o.voices) == "Цель номер ноль-ноль двенадцать — взять!");
    SC_EXPECT(SpeechText(c, "en", o.voices) == "Take target zero zero one two!");
}

SC_TEST("crew voice: abbreviations spelled out, whole words only") {
    OsaVoices o = LoadOsaVoices();
    SC_EXPECT(o.ok);
    SC_EXPECT(SpeechText(Line("Включить ГТА!", ""), "ru", o.voices) == "Включить гэ тэ а!");
    SC_EXPECT(SpeechText(Line("Готов ССЦ!", ""), "ru", o.voices) == "Готов эс эс цэ!");
    SC_EXPECT(SpeechText(Line("НЛЦ!", ""), "ru", o.voices) == "эн эл цэ!");  // not "эн" + ЛЦ
    SC_EXPECT(SpeechText(Line("По постановщику — метод Н!", ""), "ru", o.voices) == "По постановщику — метод эн!");
    SC_EXPECT(SpeechText(Line("", "MTI on!"), "en", o.voices) == "M T I on!");
}

SC_TEST("crew voice: every seat has a voice; stress and the radio change it") {
    OsaVoices o = LoadOsaVoices();
    SC_EXPECT(o.ok);
    SC_EXPECT(o.voices.languages.size() == 2u && o.voices.languages[0] == "ru");
    for (const CrewSeatDef& s : o.crew.seats) SC_EXPECT(o.voices.Seat(s.station) != nullptr);
    const VoiceSettings calm = VoiceFor(o.voices, "commander", false, "intercom");
    const VoiceSettings tense = VoiceFor(o.voices, "commander", true, "intercom");
    SC_EXPECT(tense.pitch == calm.pitch + o.voices.stress_pitch && tense.rate == calm.rate + o.voices.stress_rate);
    SC_EXPECT(!calm.radio && calm.high_hz == 3400.0);
    const VoiceSettings net = VoiceFor(o.voices, "commander", false, "battery net");
    SC_EXPECT(net.radio && net.high_hz == 3000.0);
    SC_EXPECT(VoiceFor(o.voices, "battery", false, "battery net").radio);
    // A voice file that leaves a seat out is refused.
    std::string e;
    VoiceData v;
    SC_EXPECT(!LoadVoiceData("{\"voices_id\": \"x\", \"source\": \"sam3d\", \"confidence\": \"estimate\", \"languages\": [\"en\"],"
                             " \"seats\": [{\"station\": \"driver\"}]}",
                             &o.crew, v, &e));
    SC_EXPECT(e.find("has no voice") != std::string::npos);
}

SC_TEST("crew voice: one file voices the Osa's crew and the PU-12M's; every seat it names is somebody's") {
    ConsoleData osa, post;
    CrewData osa_crew, post_crew;
    VoiceData osa_voices, post_voices, all;
    std::string e;
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/osa_ak.json", osa, &e) && LoadCrewDataFile(kData + "/crew/osa_ak_crew.json", osa, osa_crew, &e));
    SC_EXPECT(LoadConsoleFile(kData + "/procedures/pu12m.json", post, &e) && LoadCrewDataFile(kData + "/crew/pu12m_crew.json", post, post_crew, &e));
    SC_EXPECT(LoadVoiceDataFile(kData + "/crew/voices.json", &post_crew, post_voices, &e));
    if (!e.empty()) std::printf("    %s\n", e.c_str());
    SC_EXPECT(post_voices.Seat("battery_commander") != nullptr && post_voices.Seat("radio_operator") != nullptr);
    SC_EXPECT(post_voices.Seat("deputy_commander") == nullptr);  // the Osa's, not the post's
    SC_EXPECT(LoadVoiceDataFile(kData + "/crew/voices.json", &osa_crew, osa_voices, &e) && osa_voices.Seat("squad_leader") == nullptr);
    SC_EXPECT(LoadVoiceDataFile(kData + "/crew/voices.json", nullptr, all, &e));
    for (const VoiceSeat& s : all.seats) {
        const bool known = s.station == "battery" || osa_crew.FindSeat(s.station) != nullptr || post_crew.FindSeat(s.station) != nullptr;
        if (!known) std::printf("    no crew has a seat '%s'\n", s.station.c_str());
        SC_EXPECT(known);
    }
}

SC_TEST("crew voice: the bus says who is speaking now and numbers each line") {
    OsaVoices o = LoadOsaVoices();
    SC_EXPECT(o.ok);
    CalloutBus bus(&o.crew);
    SC_EXPECT(bus.Say(0.0, "captured", "operator"));
    SC_EXPECT(bus.Say(0.0, "order_battery_dummy", "commander") == false);  // unknown id
    SC_EXPECT(bus.Say(0.0, "order_search", "commander"));
    bus.Step(0.0);
    SC_EXPECT(bus.Now().size() == 1u);  // one voice on the intercom
    const std::size_t first = bus.Now()[0].seq;
    SC_EXPECT(first == 1u);
    SC_EXPECT(bus.SpeakingFrom(bus.Now()[0].from));
    bus.Step(10.0);  // the first finished, the second starts
    SC_EXPECT(bus.Now().size() == 1u && bus.Now()[0].seq == 2u);
    bus.Step(20.0);
    SC_EXPECT(bus.Now().empty());
    SC_EXPECT(!bus.SpeakingFrom("operator") && !bus.SpeakingFrom("commander"));
}
