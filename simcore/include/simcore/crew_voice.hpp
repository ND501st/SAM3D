// simcore/crew_voice.hpp — the crew's voices (roadmap 8.10, docs/crew/10_BODIES_AND_VOICES.md).
//
// What a speech engine (or a recorded line) says for a callout of the bus (crew_callouts.hpp), and in whose voice:
//
//   VoiceData        data/crew/voices.json: the languages in order of preference, each seat's voice (pitch and rate
//                    on the speech engine's -10..10 scale, intercom or radio), what stress does to it, the band of the
//                    intercom and the radio, and how the crew pronounces its abbreviations in each language
//   SpeechText       a callout's words with the azimuths in mils, the numbers and the abbreviations spelled out
//   InstructorSpeech a lesson's words for an English voice (tutorial.hpp): the Russian in Latin letters
//   RussianNumber    0..999 in words
//   VoiceFor         the voice settings for a speaker, stressed or not
//
// The simulation's timing stays the bus's (SpeakingTimeS): a voice plays alongside it, it doesn't move it.
#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "simcore/crew_callouts.hpp"
#include "simcore/crew_data.hpp"
#include "simcore/json.hpp"

namespace simcore {

struct VoiceSeat {
    std::string station;  // a console station, or "battery" (the battery commander on the net)
    int pitch = 0;        // -10..10
    int rate = 0;         // -10..10
    bool radio = false;   // heard over a radio set rather than the intercom
};

struct VoiceData {
    std::vector<std::string> languages;  // "ru", "en": the first one there is a voice for
    std::vector<VoiceSeat> seats;
    int stress_pitch = 0;  // added while the speaker is stressed
    int stress_rate = 0;
    double intercom_low_hz = 300.0;
    double intercom_high_hz = 3400.0;
    double radio_low_hz = 300.0;
    double radio_high_hz = 3000.0;
    std::map<std::string, std::map<std::string, std::string>> say;  // language -> written -> spoken

    const VoiceSeat* Seat(const std::string& station) const {
        for (const VoiceSeat& s : seats) {
            if (s.station == station) return &s;
        }
        return nullptr;
    }
};

// The voice for one line.
struct VoiceSettings {
    int pitch = 0;
    int rate = 0;
    bool radio = false;
    double low_hz = 300.0;
    double high_hz = 3400.0;
};

inline VoiceSettings VoiceFor(const VoiceData& v, const std::string& station, bool stressed, const std::string& channel) {
    VoiceSettings out;
    if (const VoiceSeat* s = v.Seat(station)) {
        out.pitch = s->pitch;
        out.rate = s->rate;
        out.radio = s->radio;
    }
    if (channel != "intercom") out.radio = true;
    if (stressed) {
        out.pitch += v.stress_pitch;
        out.rate += v.stress_rate;
    }
    out.pitch = out.pitch < -10 ? -10 : (out.pitch > 10 ? 10 : out.pitch);
    out.rate = out.rate < -10 ? -10 : (out.rate > 10 ? 10 : out.rate);
    out.low_hz = out.radio ? v.radio_low_hz : v.intercom_low_hz;
    out.high_hz = out.radio ? v.radio_high_hz : v.intercom_high_hz;
    return out;
}

// 0..999 in Russian words (masculine): 35 -> "тридцать пять".
inline std::string RussianNumber(int n) {
    static const char* const ones[] = {"ноль", "один", "два", "три", "четыре", "пять", "шесть", "семь", "восемь", "девять"};
    static const char* const teens[] = {"десять", "одиннадцать", "двенадцать", "тринадцать", "четырнадцать",
                                        "пятнадцать", "шестнадцать", "семнадцать", "восемнадцать", "девятнадцать"};
    static const char* const tens[] = {"", "", "двадцать", "тридцать", "сорок", "пятьдесят", "шестьдесят", "семьдесят",
                                       "восемьдесят", "девяносто"};
    static const char* const hundreds[] = {"", "сто", "двести", "триста", "четыреста", "пятьсот", "шестьсот", "семьсот",
                                           "восемьсот", "девятьсот"};
    if (n < 0 || n > 999) return std::to_string(n);
    if (n < 10) return ones[n];
    std::string out;
    const int h = n / 100;
    const int r = n % 100;
    if (h > 0) out = hundreds[h];
    if (r == 0) return out;
    if (!out.empty()) out += " ";
    if (r < 10) return out + ones[r];
    if (r < 20) return out + teens[r - 10];
    out += tens[r / 10];
    if (r % 10 != 0) out += std::string(" ") + ones[r % 10];
    return out;
}

namespace detail {

inline bool IsDigit(char c) { return c >= '0' && c <= '9'; }
// Letters, digits and every byte of a multi-byte UTF-8 character count as inside a word.
inline bool IsWordByte(char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    return u >= 0x80 || IsDigit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

inline std::string EnglishDigits(const std::string& digits) {
    static const char* const names[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"};
    std::string out;
    for (char c : digits) {
        if (!out.empty()) out += " ";
        out += names[c - '0'];
    }
    return out;
}

// A pair of digits as the crew reads azimuths and board numbers: "00" -> "ноль-ноль", "05" -> "ноль пять", "35" ->
// "тридцать пять".
inline std::string RussianPair(const std::string& two) {
    if (two == "00") return "ноль-ноль";
    if (two[0] == '0') return std::string("ноль ") + RussianNumber(two[1] - '0');
    return RussianNumber((two[0] - '0') * 10 + (two[1] - '0'));
}

inline std::string ReplaceWords(std::string text, const std::map<std::string, std::string>& say) {
    for (const auto& w : say) {
        if (w.first.empty()) continue;
        std::size_t at = 0;
        while ((at = text.find(w.first, at)) != std::string::npos) {
            const bool start_ok = at == 0 || !IsWordByte(text[at - 1]);
            const std::size_t end = at + w.first.size();
            const bool end_ok = end >= text.size() || !IsWordByte(text[end]);
            if (start_ok && end_ok) {
                text.replace(at, w.first.size(), w.second);
                at += w.second.size();
            } else {
                at = end;
            }
        }
    }
    return text;
}

}  // namespace detail

// The words to speak for a callout in `language` ("ru" or "en"): the azimuths in mils ("35-00") read in pairs, four-
// figure numbers (board numbers) in pairs (Russian) or digit by digit (English), other numbers in words (Russian; an
// English engine reads digits itself), and the abbreviations as the crew says them (voices.json "say").
inline std::string SpeechText(const SpokenCallout& c, const std::string& language, const VoiceData& v) {
    const bool ru = language == "ru";
    const std::string& text = ru ? c.ru : c.en;
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        if (!detail::IsDigit(text[i]) || (i > 0 && detail::IsWordByte(text[i - 1]) && !detail::IsDigit(text[i - 1]))) {
            out += text[i++];
            continue;
        }
        std::size_t j = i;
        int value = 0;
        while (j < text.size() && detail::IsDigit(text[j])) value = value * 10 + (text[j++] - '0');
        const std::string digits = text.substr(i, j - i);
        if (j < text.size() && detail::IsWordByte(text[j]) && !detail::IsDigit(text[j])) {  // part of a name ("123M")
            out += digits;
            i = j;
            continue;
        }
        // dd-dd: an azimuth in mils.
        if (digits.size() == 2 && j + 2 < text.size() && text[j] == '-' && detail::IsDigit(text[j + 1]) &&
            detail::IsDigit(text[j + 2]) && (j + 3 >= text.size() || !detail::IsDigit(text[j + 3]))) {
            const std::string second = text.substr(j + 1, 2);
            out += ru ? detail::RussianPair(digits) + " " + detail::RussianPair(second)
                      : detail::EnglishDigits(digits) + ", " + detail::EnglishDigits(second);
            i = j + 3;
            continue;
        }
        if (digits.size() == 4) {
            out += ru ? detail::RussianPair(digits.substr(0, 2)) + " " + detail::RussianPair(digits.substr(2, 2))
                      : detail::EnglishDigits(digits);
        } else if (ru && digits.size() <= 3) {
            out += RussianNumber(value);
        } else {
            out += digits;
        }
        i = j;
    }
    const auto it = v.say.find(language);
    return it != v.say.end() ? detail::ReplaceWords(out, it->second) : out;
}

// A lesson's words for an English voice (the instructor of tutorial.hpp): an English engine skips Cyrillic, so the
// Russian goes into Latin letters as an English reader would say them (BGN/PCGN, without diacritics); the quote marks
// go, a dash is a pause, and "..." (a field nobody has filled yet) is silent. "Order «К бою»" -> "Order K boyu".
inline std::string InstructorSpeech(const std::string& text) {
    static const char* const letters[] = {"a", "b", "v", "g", "d", "e", "zh", "z", "i", "y", "k", "l", "m", "n", "o", "p",
                                          "r", "s", "t", "u", "f", "kh", "ts", "ch", "sh", "shch", "", "y", "", "e", "yu", "ya"};
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        const unsigned b0 = static_cast<unsigned char>(text[i]);
        if (b0 < 0x80u) {
            out += text[i++];
            continue;
        }
        std::size_t len = b0 >= 0xF0u ? 4 : b0 >= 0xE0u ? 3 : b0 >= 0xC0u ? 2 : 1;
        if (i + len > text.size()) len = text.size() - i;
        if (len == 2) {
            const unsigned b1 = static_cast<unsigned char>(text[i + 1]);
            const unsigned cp = ((b0 & 0x1Fu) << 6u) | (b1 & 0x3Fu);
            if (cp >= 0x410u && cp <= 0x44Fu) {  // А..Я, а..я
                std::string latin = letters[(cp - 0x410u) % 32u];
                if (cp < 0x430u && !latin.empty()) latin[0] = static_cast<char>(latin[0] - 'a' + 'A');
                out += latin;
                i += 2;
                continue;
            }
            if (cp == 0x401u || cp == 0x451u) {  // Ё, ё
                out += cp == 0x401u ? "Yo" : "yo";
                i += 2;
                continue;
            }
            if (cp == 0xABu || cp == 0xBBu) {  // « »
                i += 2;
                continue;
            }
        } else if (len == 3 && b0 == 0xE2u && static_cast<unsigned char>(text[i + 1]) == 0x80u) {
            const unsigned b2 = static_cast<unsigned char>(text[i + 2]);
            if (b2 == 0x93u || b2 == 0x94u) {  // – —
                out += ",";
                i += 3;
                continue;
            }
            if (b2 == 0xA6u) {  // …
                i += 3;
                continue;
            }
        }
        out.append(text, i, len);
        i += len;
    }
    return out;
}

namespace detail {

class VoiceLoader : public ConsoleLoader {
public:
    bool LoadVoices(const json::Value& root, const CrewData* crew, VoiceData& out) {
        if (!Keys(root, "", {"voices_id", "status", "notes", "source", "confidence", "languages", "seats", "stress", "bands",
                             "say"}) ||
            !Provenance(root, "") || !StringList(root, "", "languages", out.languages)) {
            return false;
        }
        if (out.languages.empty()) return Fail("languages", "required, e.g. [\"ru\", \"en\"]");
        for (std::size_t i = 0; i < out.languages.size(); ++i) {
            if (out.languages[i] != "ru" && out.languages[i] != "en") return Fail(Index("languages", i), "must be ru or en");
        }
        const json::Value* seats = OptionalArray(root, "", "seats");
        if (seats == nullptr) return message_.empty() ? Fail("seats", "required") : false;
        for (std::size_t i = 0; i < seats->items.size(); ++i) {
            const json::Value& s = seats->items[i];
            const std::string p = Index("seats", i);
            VoiceSeat vs;
            if (!Keys(s, p, {"station", "pitch", "rate", "radio", "notes"}) || !RequireString(s, p, "station", vs.station) ||
                !OptionalInt(s, p, "pitch", vs.pitch) || !OptionalInt(s, p, "rate", vs.rate) || !OptionalBool(s, p, "radio", vs.radio)) {
                return false;
            }
            if (vs.pitch < -10 || vs.pitch > 10) return Fail(Join(p, "pitch"), "must be from -10 to 10");
            if (vs.rate < -10 || vs.rate > 10) return Fail(Join(p, "rate"), "must be from -10 to 10");
            // One file voices every vehicle's crew: a seat this crew hasn't got is another crew's (the tests check that
            // each one is somebody's).
            if (crew != nullptr && vs.station != "battery" && crew->FindSeat(vs.station) == nullptr) continue;
            out.seats.push_back(std::move(vs));
        }
        if (crew != nullptr) {
            for (const CrewSeatDef& seat : crew->seats) {
                if (out.Seat(seat.station) == nullptr) return Fail("seats", "the crew's seat '" + seat.station + "' has no voice");
            }
        }
        if (const json::Value* st = root.Find("stress")) {
            if (!Keys(*st, "stress", {"pitch", "rate", "source", "confidence", "notes"}) || !Provenance(*st, "stress") ||
                !OptionalInt(*st, "stress", "pitch", out.stress_pitch) || !OptionalInt(*st, "stress", "rate", out.stress_rate)) {
                return false;
            }
        }
        if (const json::Value* b = root.Find("bands")) {
            if (!Keys(*b, "bands", {"intercom_hz", "radio_hz", "source", "confidence", "notes"}) || !Provenance(*b, "bands") ||
                !Band(*b, "intercom_hz", out.intercom_low_hz, out.intercom_high_hz) ||
                !Band(*b, "radio_hz", out.radio_low_hz, out.radio_high_hz)) {
                return false;
            }
        }
        if (const json::Value* say = root.Find("say")) {
            if (!say->Is(json::Type::Object)) return Fail("say", "expected an object of languages");
            for (const std::string& lang : say->keys) {
                const json::Value* words = say->Find(lang);
                const std::string p = Join("say", lang);
                if (!words->Is(json::Type::Object)) return Fail(p, "expected an object: written -> spoken");
                for (const std::string& w : words->keys) {
                    const json::Value* spoken = words->Find(w);
                    if (w == "source" || w == "confidence" || w == "notes") continue;
                    if (!spoken->Is(json::Type::String)) return Fail(Join(p, w), "expected a string");
                    out.say[lang][w] = spoken->string;
                }
                if (!Provenance(*words, p)) return false;
            }
        }
        return true;
    }

private:
    bool StringList(const json::Value& obj, const std::string& path, const char* key, std::vector<std::string>& out) {
        const json::Value* arr = OptionalArray(obj, path, key);
        if (arr == nullptr) return message_.empty();
        for (std::size_t i = 0; i < arr->items.size(); ++i) {
            if (!arr->items[i].Is(json::Type::String)) return Fail(Index(Join(path, key), i), "expected a string");
            out.push_back(arr->items[i].string);
        }
        return true;
    }

    bool Band(const json::Value& obj, const char* key, double& low, double& high) {
        const json::Value* arr = OptionalArray(obj, "bands", key);
        if (arr == nullptr) return message_.empty();
        if (arr->items.size() != 2 || !arr->items[0].Is(json::Type::Number) || !arr->items[1].Is(json::Type::Number) ||
            arr->items[0].number <= 0.0 || arr->items[1].number <= arr->items[0].number) {
            return Fail(Join("bands", key), "expected [low, high] in Hz, low < high");
        }
        low = arr->items[0].number;
        high = arr->items[1].number;
        return true;
    }
};

}  // namespace detail

// `crew` (may be null) checks that every seat has a voice, and keeps only its own seats' voices.
inline bool LoadVoiceData(const std::string& json_text, const CrewData* crew, VoiceData& out, std::string* error = nullptr) {
    json::Value root;
    std::string parse_error;
    if (!json::Parse(json_text, root, &parse_error)) {
        if (error != nullptr) *error = parse_error;
        return false;
    }
    VoiceData v;
    detail::VoiceLoader loader;
    if (!loader.LoadVoices(root, crew, v)) {
        if (error != nullptr) *error = loader.Message();
        return false;
    }
    out = std::move(v);
    return true;
}

inline bool LoadVoiceDataFile(const std::string& path, const CrewData* crew, VoiceData& out, std::string* error = nullptr) {
    std::string text;
    if (!detail::ReadTextFile(path, text)) {
        if (error != nullptr) *error = "cannot open " + path;
        return false;
    }
    if (!LoadVoiceData(text, crew, out, error)) {
        if (error != nullptr) *error = path + ": " + *error;
        return false;
    }
    return true;
}

}  // namespace simcore
