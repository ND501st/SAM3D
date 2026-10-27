// The crew's voices (roadmap 8.10, docs/crew/10_BODIES_AND_VOICES.md): the words a speech engine says for a callout,
// each seat's voice, and the bus's "now speaking" for the voice and the bodies.
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>

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

// A WAV file of silence: `format` 1 is PCM.
void WriteWav(const std::string& path, int channels, int rate, int frames, int bits = 16, int format = 1) {
    std::ofstream f(path, std::ios::binary);
    const auto put16 = [&f](long v) {
        const char b[2] = {static_cast<char>(v & 0xff), static_cast<char>((v >> 8) & 0xff)};
        f.write(b, 2);
    };
    const auto put32 = [&put16](long v) {
        put16(v & 0xffff);
        put16((v >> 16) & 0xffff);
    };
    const long data = static_cast<long>(frames) * channels * (bits / 8);
    f.write("RIFF", 4);
    put32(36 + data);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    put32(16);
    put16(format);
    put16(channels);
    put32(rate);
    put32(static_cast<long>(rate) * channels * (bits / 8));
    put16(channels * (bits / 8));
    put16(bits);
    f.write("data", 4);
    put32(data);
    for (long i = 0; i < data; ++i) f.put('\0');
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

SC_TEST("crew voice: the instructor says the Russian in Latin letters") {
    SC_EXPECT(InstructorSpeech("Order «К бою»") == "Order K boyu");
    SC_EXPECT(InstructorSpeech("ЩЁЛКь, ГОТОВ") == "ShchYoLK, GOTOV");  // the soft sign is silent
    // A dash is a pause, an unfilled field silent, everything else as it was.
    SC_EXPECT(InstructorSpeech("Range — 35 km, target …") == "Range , 35 km, target ");
    SC_EXPECT(InstructorSpeech("Qc = 15 deg (Ctrl+Z)") == "Qc = 15 deg (Ctrl+Z)");
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
    // Said aloud in the vehicle: never a radio voice (not even a seat whose usual voice is on a set), and wider than
    // the intercom's band; the net stays narrower than both.
    for (const char* ch : {"cabin", "cabin (shouted)"}) {
        const VoiceSettings aloud = VoiceFor(o.voices, "commander", false, ch);
        SC_EXPECT(!aloud.radio && aloud.low_hz < calm.low_hz && aloud.high_hz > calm.high_hz);
        SC_EXPECT(!VoiceFor(o.voices, "battery", false, ch).radio);
        SC_EXPECT(aloud.high_hz > net.high_hz);
    }
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

SC_TEST("crew voice: a recorded line takes its recording's time on the bus, so the next line doesn't cut it off") {
    OsaVoices o = LoadOsaVoices();
    SC_EXPECT(o.ok);
    CalloutBus bus(&o.crew);
    // Without recordings: the speaking time of the Russian words ("Поиск!": 0.4 + 0.35 s).
    SC_EXPECT_NEAR(bus.LineTimeS("order_search", "commander", "Поиск!"), 0.75, 1e-9);
    bus.SetRecordedLengths({{"commander/order_search", 1.37}, {"done", 0.9}});
    SC_EXPECT_NEAR(bus.LineTimeS("order_search", "commander", "Поиск!"), 1.37, 1e-9);     // his own recording
    SC_EXPECT_NEAR(bus.LineTimeS("order_search", "deputy_commander", "Поиск!"), 0.75, 1e-9);  // none of his
    SC_EXPECT_NEAR(bus.LineTimeS("done", "operator", "Выполнено!"), 0.9, 1e-9);           // the shared one
    // A line with words filled in is never a recording.
    const std::string contact = "Цель, азимут 05-40, дальность 24!";
    bus.SetRecordedLengths({{"senior_operator/contact", 5.0}});
    SC_EXPECT_NEAR(bus.LineTimeS("contact", "senior_operator", contact), SpeakingTimeS(contact), 1e-9);
    // On the bus: the next line on the intercom starts when the recording ends.
    bus.SetRecordedLengths({{"commander/order_search", 1.37}});
    SC_EXPECT(bus.Say(0.0, "order_search", "commander"));
    SC_EXPECT(bus.Say(0.0, "done", "operator"));
    for (int i = 0; i <= 30; ++i) bus.Step(i * 0.1);
    const std::vector<SpokenCallout>& log = bus.Log();
    SC_EXPECT(log.size() == 2u);
    if (log.size() == 2u) {
        SC_EXPECT_NEAR(log[0].end_s - log[0].start_s, 1.37, 1e-9);
        // ... a moment after it ends (the gap between speakers, docs/crew/15_TALK.md section 2).
        SC_EXPECT(log[1].start_s >= log[0].end_s + CalloutBus::kTurnGapS - 1e-9 && log[1].start_s < log[0].end_s + CalloutBus::kTurnGapS + 0.1 + 1e-9);
        SC_EXPECT_NEAR(log[1].end_s - log[1].start_s, SpeakingTimeS("Выполнено!"), 1e-9);
    }
}

SC_TEST("crew voice: a WAV's length from its headers, and the recorded lines found where the game looks for them") {
    WriteWav("sc_test_mono.wav", 1, 24000, 6000);      // 0.25 s
    WriteWav("sc_test_stereo.wav", 2, 8000, 4000);     // 0.5 s
    WriteWav("sc_test_float.wav", 1, 24000, 6000, 32, 3);  // not 16-bit PCM: the game can't play it
    SC_EXPECT_NEAR(WavLengthS("sc_test_mono.wav"), 0.25, 1e-9);
    SC_EXPECT_NEAR(WavLengthS("sc_test_stereo.wav"), 0.5, 1e-9);
    SC_EXPECT(WavLengthS("sc_test_float.wav") == 0.0);
    SC_EXPECT(WavLengthS("sc_test_no_such_file.wav") == 0.0);
    // A shared recording, <dir>/<id>.wav, for every speaker.
    OsaVoices o = LoadOsaVoices();
    WriteWav("order_search.wav", 1, 24000, 24000);  // 1 s
    const std::map<std::string, double> lengths = RecordedLineLengths(".", o.crew);
    SC_EXPECT(lengths.count("order_search") == 1u && lengths.size() == 1u);
    if (lengths.count("order_search") == 1u) SC_EXPECT_NEAR(lengths.at("order_search"), 1.0, 1e-9);
    for (const char* f : {"sc_test_mono.wav", "sc_test_stereo.wav", "sc_test_float.wav", "order_search.wav"}) std::remove(f);
}

SC_TEST("crew voice: the English recordings (tools/voice_lines.py) are found for the seats that say them") {
    OsaVoices o = LoadOsaVoices();
    SC_EXPECT(o.ok);
    const std::map<std::string, double> lengths = RecordedLineLengths(kData + "/../art/crew_voices/en", o.crew);
    SC_EXPECT(lengths.size() > 100u);
    SC_EXPECT(lengths.count("commander/order_k_boyu") == 1u);
    SC_EXPECT(lengths.count("commander/air_alert") == 0u);         // left to the speech engine: no take was right
    SC_EXPECT(lengths.count("deputy_commander/air_alert") == 1u);
    SC_EXPECT(lengths.count("senior_operator/contact") == 0u);     // words filled in: never recorded
    for (const auto& l : lengths) {
        const std::string seat = l.first.substr(0, l.first.find('/'));
        SC_EXPECT(o.crew.FindSeat(seat) != nullptr);
        SC_EXPECT(l.second > 0.3 && l.second < 6.0);
    }
}
