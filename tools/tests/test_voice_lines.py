"""The crew's recorded lines (tools/voice_lines.py): the words as the game speaks them, which lines, the word check, and
the recordings up to date with the data."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import voice_lines  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent.parent
SAY = voice_lines.say_table(json.loads((ROOT / "data/crew/voices.json").read_text(encoding="utf-8")), "en")


def test_speech_text_matches_the_games():
    # simcore/tests/test_crew_voice.cpp's English cases.
    assert voice_lines.speech_text("Target, azimuth 35-00, range 20!", SAY) == "Target, azimuth three five, zero zero, range 20!"
    assert voice_lines.speech_text("Take target 0012!", SAY) == "Take target zero zero one two!"
    assert voice_lines.speech_text("MTI on!", SAY) == "M T I on!"


def test_the_say_table_takes_whole_words_only():
    assert voice_lines.speech_text("ARM launch!", SAY) == "A R M launch!"
    assert voice_lines.speech_text("R-111 tuned!", SAY) == "R one one one tuned!"
    assert voice_lines.speech_text("TVs and HARM", SAY) == "TVs and HARM"  # neither is a whole TV or ARM
    assert voice_lines.speech_text("Tracking on the TV!", SAY) == "Tracking on the T V!"


def test_the_engine_gets_no_quote_marks_and_letters_with_full_stops():
    assert voice_lines.engine_text("Warn the covered unit: «Air»!") == "Warn the covered unit: Air!"
    # Kokoro reads a lone "A" as the article: "A F C" came out "LFC", "A.F.C." as the letters.
    assert voice_lines.engine_text("A F C off!") == "A.F.C. off!"
    assert voice_lines.engine_text("A S P D ready!") == "A.S.P.D. ready!"
    assert voice_lines.engine_text("M T I on!") == "M T I on!"  # heard right as it is, wrong with the stops
    assert voice_lines.engine_text("R one one one tuned!") == "R one one one tuned!"
    assert voice_lines.engine_text("Take a target!") == "Take a target!"


def test_speaking_time_is_the_buses():
    # crew_callouts.hpp SpeakingTimeS: 0.4 s plus 0.35 s a word; "35-00" is two words to it.
    assert abs(voice_lines.speaking_time_s("К бою!") - 1.1) < 1e-9
    assert abs(voice_lines.speaking_time_s("Цель, азимут 35-00!") - (0.4 + 0.35 * 4)) < 1e-9


def test_only_fixed_lines_every_speaker_and_the_clash_left_out():
    lines, clashes = voice_lines.plan()
    assert "commander/order_k_boyu" in lines
    assert not any("{" in line["en"] for line in lines.values())
    # "any" is every seat of the crew that has the callout.
    for seat in ("commander", "deputy_commander", "senior_operator", "operator", "driver", "battery_commander",
                 "squad_leader", "radio_operator"):
        assert f"{seat}/ack" in lines, seat
    # The driver's power_on is two lines in two crews: one file can't be both.
    assert [k for k, _ in clashes] == ["driver/power_on"]
    assert "driver/power_on" not in lines
    # The same words in both crews are one file for both.
    assert lines["operator/ack"]["crews"] == ["osa_ak_crew.json", "pu12m_crew.json"]


def test_every_seat_has_a_recorded_voice_and_no_two_heard_together_share_one():
    config = json.loads((ROOT / voice_lines.CONFIG).read_text(encoding="utf-8"))
    for crew_file in config["crews"]:
        crew = json.loads((ROOT / "data/crew" / crew_file).read_text(encoding="utf-8"))
        seats = [s["station"] for s in crew["seats"]]
        assert all(s in config["seats"] for s in seats), crew_file
        voices = [config["seats"][s] for s in seats]
        assert len(set(voices)) == len(voices), crew_file
    assert config["seats"]["commander"] != config["seats"]["battery_commander"]  # they talk on the battery net


def test_word_error_compares_spelled_letters_and_digits_alike():
    assert voice_lines.word_error("A R M launch!", "ARM launch.") == 0.0
    assert voice_lines.word_error("R one one one tuned!", "R111 tuned.") == 0.0
    assert voice_lines.word_error("Launch permitted!", "Launch permitted.") == 0.0
    assert voice_lines.word_error("Launch permitted!", "Lunch permitted.") == 0.5
    assert voice_lines.word_error("Second missile, launch!", "2nd missile launch") == 0.0
    assert voice_lines.word_error("Generator on line!", "generator online") == 0.0
    assert voice_lines.word_error("A F C off!", "A. F. C. Off.") == 0.0


def test_a_line_heard_right_is_not_a_failure_until_it_is_heard_differently():
    entry = {"flags": ["words", "long"], "transcript": "Where?", "word_error": 1.0}
    assert voice_lines.judge_words("commander/air_alert", entry, {}) == ["words", "long"]
    assert voice_lines.judge_words("commander/air_alert", entry, {"commander/air_alert": "Where?"}) == ["heard right", "long"]
    # Recorded again and heard as something else: the approval was for the other recording.
    again = dict(entry, flags=["heard right", "long"], transcript="There.")
    assert voice_lines.judge_words("commander/air_alert", again, {"commander/air_alert": "Where?"}) == ["words", "long"]
    # Heard well now: nothing to approve.
    good = dict(entry, transcript="Air!", word_error=0.0)
    assert voice_lines.judge_words("commander/air_alert", good, {"commander/air_alert": "Where?"}) == ["long"]


def test_a_retake_is_part_of_the_lines_settings_so_changing_it_records_the_line_again():
    config = {"speed": 1.0, "accent": "en-us",
              "retakes": {"deputy_commander/miss": {"speed": 0.9, "why": "heard as 'Messer'"}}}
    hashes = {"model": "m", "voice_pack": "v"}
    miss = {"seat": "deputy_commander", "id": "miss", "spoken": "Miss!", "voice": "am_michael"}
    hit = dict(miss, id="hit", spoken="Hit!")
    assert voice_lines.retake_of(config, "deputy_commander/miss") == {"speed": 0.9}  # the why is only a note
    assert voice_lines.retake_of(config, "deputy_commander/hit") is None
    assert voice_lines.settings_key(miss, config, hashes)["retake"] == {"speed": 0.9}
    assert "retake" not in voice_lines.settings_key(hit, config, hashes)  # lines without one keep their recordings


def test_every_retake_is_for_a_line_and_says_why():
    config = json.loads((ROOT / voice_lines.CONFIG).read_text(encoding="utf-8"))
    lines, _ = voice_lines.plan()
    unrecorded = dict(voice_lines.left_to_engine(config))
    for key, retake in config.get("retakes", {}).items():
        assert (key in unrecorded) == (retake.get("record") is False), key
        assert key in lines or key in unrecorded, key
        assert retake.get("why"), key
        assert set(retake) <= {"speed", "phonemes", "style_length", "record", "why"}, key
    for key in config.get("heard_right", {}):
        assert key in lines, key


def test_a_line_left_to_the_engine_is_not_recorded():
    config = json.loads((ROOT / voice_lines.CONFIG).read_text(encoding="utf-8"))
    config["retakes"] = {"driver/air_alert": {"record": False, "why": "no take heard right"}}
    lines, _ = voice_lines.plan(config)
    assert "driver/air_alert" not in lines and "commander/air_alert" in lines
    assert voice_lines.left_to_engine(config) == [("driver/air_alert", "no take heard right")]


def test_the_transcribers_hint_is_the_crews_jargon_not_a_line():
    lines, _ = voice_lines.plan()
    hint = voice_lines.vocabulary(lines)
    assert "azimuth" in hint and "launcher-antenna" in hint
    assert "Search radar ready" not in hint


def test_the_recordings_match_the_data():
    if not (ROOT / "art/crew_voices/en/manifest.json").exists():
        return  # nothing recorded on this machine
    assert voice_lines.main(["--check"]) == 0, "run: python tools/voice_lines.py (docs/crew/10_BODIES_AND_VOICES.md)"
