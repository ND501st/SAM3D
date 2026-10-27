"""Record the crew's fixed callouts in each seat's voice: art/crew_voices/<language>/<seat>/<callout>.wav (docs/crew/10).

Usage:  python tools/voice_lines.py [--models DIR] [--force] [--only SEAT/ID ...] [--no-transcribe] [--review FILE]
        python tools/voice_lines.py --list | --check

    --models         default out/voice_gen/models: kokoro-v1.0.onnx, voices-v1.0.bin and whisper-base.en/
    --force          record every line again (default: only lines whose words, voice, settings or model changed)
    --only           record just these lines (seat/callout, e.g. commander/order_k_boyu)
    --no-transcribe  skip the word check
    --review         default out/voice_gen/review.html: every line with its player and its checks, the flagged first
    --list           print the lines and record nothing
    --check          exit 1 if the manifest doesn't match the data now (lines missing, stale or to be removed)

Recording needs Kokoro (kokoro-onnx), numpy and, for the word check, faster-whisper: run it with the Python that has
them (docs/crew/10_BODIES_AND_VOICES.md section 4 says how to make that environment). --list, --check and the parts
the tests use are standard library only.

What is recorded: every callout of data/crew/recorded_voices.json's crews whose Russian has nothing to fill in (the game
plays a recording only for those: SimCrewVoiceComponent::PlayRecorded), in the voice of each seat that says it ("any":
every seat of that crew). The words are the English line as the game hands it to a speech engine (simcore/crew_voice.hpp
SpeechText, ported here: azimuths in mils in digit pairs, four-figure numbers digit by digit, voices.json's "say" table,
whole words only), without the quote marks and with letters spelled from an "A" written "A.F.C." (Kokoro reads a lone
"A" as the article). A seat and callout said in different words by two crews would be one file
for two lines, and the game would play one crew's words for the other's: it is not recorded (both crews' engine says
it), and the run reports it.

Each file: the silence trimmed to 30 ms before and 120 ms after, the speech brought to -18 dBFS RMS with the peaks
under -1 dBFS, 16-bit mono at the engine's rate. The checks, kept in art/crew_voices/<language>/manifest.json:
    empty    less than 0.25 s of speech
    clipped  a flattened peak in the engine's samples
    words    the transcript differs from the words (word error rate over 0.25, digits and spelled letters normalised;
             transcribed with and without the crew's vocabulary as a hint, the closer counts). One-word lines ("Air!")
             are often misheard: the manifest and the review page give the phonemes the engine was given, and a line
             listened to and found right goes in recorded_voices.json's "heard_right" with the transcript it was
             flagged for ("heard right" then, not a failure, until a new recording is heard differently). A line
             heard said wrong gets a retake in recorded_voices.json's "retakes": its own speed, the phonemes to give
             the engine in place of the words, or the voice's style for a longer input ("style_length": Kokoro
             styles a voice by the input's length, and some short lines come out wrong in the short style); a line
             no retake fixes is "record": false, left to the game's speech engine (its recording removed)
    long     longer than the time the crew's bus gives the line (crew_callouts.hpp SpeakingTimeS: 0.4 s and 0.35 s a
             word of the Russian), so the next line on the channel cuts it off in the game; reported, not a failure
The first three are failures: the run exits 1.
"""
import argparse
import hashlib
import html
import json
import os
import re
import sys
import time
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG = "data/crew/recorded_voices.json"
LEAD_S, TAIL_S = 0.03, 0.12
TARGET_RMS_DB, PEAK_DB = -18.0, -1.0
MIN_SPEECH_S = 0.25
MAX_WORD_ERROR = 0.25
FAILURES = ("empty", "clipped", "words")
FLAGS = FAILURES + ("heard right", "long")


def read_json(rel):
    return json.loads((ROOT / rel).read_text(encoding="utf-8"))


# ---------- the words, as the game gives them to a speech engine (simcore/crew_voice.hpp) ----------

def is_digit(c):
    return "0" <= c <= "9"


def is_word(c):
    # IsWordByte: letters, digits and every byte of a multi-byte character count as inside a word.
    return ord(c) >= 0x80 or is_digit(c) or ("a" <= c <= "z") or ("A" <= c <= "Z")


def english_digits(digits):
    names = ["zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"]
    return " ".join(names[ord(c) - ord("0")] for c in digits)


def replace_words(text, say):
    # ReplaceWords: each written form, in sorted order, where it stands as a whole word.
    for written in sorted(say):
        spoken = say[written]
        if not written:
            continue
        at = text.find(written)
        while at != -1:
            end = at + len(written)
            if (at == 0 or not is_word(text[at - 1])) and (end >= len(text) or not is_word(text[end])):
                text = text[:at] + spoken + text[end:]
                at = text.find(written, at + len(spoken))
            else:
                at = text.find(written, end)
    return text


def speech_text(text, say):
    """SpeechText for English: an azimuth in mils ("35-00") in digit pairs, a four-figure number digit by digit, other
    numbers as they are (the engine reads them), then the say table."""
    out, i, n = [], 0, len(text)
    while i < n:
        if not is_digit(text[i]) or (i > 0 and is_word(text[i - 1]) and not is_digit(text[i - 1])):
            out.append(text[i])
            i += 1
            continue
        j = i
        while j < n and is_digit(text[j]):
            j += 1
        digits = text[i:j]
        if j < n and is_word(text[j]) and not is_digit(text[j]):  # part of a name ("123M")
            out.append(digits)
            i = j
            continue
        if (len(digits) == 2 and j + 2 < n and text[j] == "-" and is_digit(text[j + 1]) and is_digit(text[j + 2])
                and (j + 3 >= n or not is_digit(text[j + 3]))):
            out.append(english_digits(digits) + ", " + english_digits(text[j + 1:j + 3]))
            i = j + 3
            continue
        out.append(english_digits(digits) if len(digits) == 4 else digits)
        i = j
    return replace_words("".join(out), say)


def engine_text(spoken):
    """What the engine is given: no quote marks; a dash is a pause (as InstructorSpeech has it); letters spelled out
    from an "A" ("A F C") written with full stops, "A.F.C.": Kokoro reads a lone "A" as the article. Other spelled
    letters ("M T I") it reads well as they are, and worse with the stops."""
    text = spoken.replace("«", "").replace("»", "").replace("—", ",").replace("–", ",").replace("…", "")
    return re.sub(r"\bA(?: [A-Z]\b)+", lambda m: m.group(0).replace(" ", ".") + ".", text)


def speaking_time_s(words):
    # SpeakingTimeS: 0.4 s plus 0.35 s a word (crew_callouts.hpp), counted on the line the bus times (the Russian).
    n, in_word = 0, False
    for c in words:
        space = c in " \t,!.-"
        if not space and not in_word:
            n += 1
        in_word = not space
    return 0.4 + 0.35 * n


def say_table(voices, language):
    table = voices.get("say", {}).get(language, {})
    return {k: v for k, v in table.items() if k not in ("source", "confidence", "notes")}


def plan(config=None, voices=None):
    """The lines to record, {"seat/id": line}, and the clashes [(seat/id, [(crew, words), ...])] left out."""
    config = config or read_json(CONFIG)
    voices = voices or read_json("data/crew/voices.json")
    say = say_table(voices, config["language"])
    found = {}
    for crew_file in config["crews"]:
        crew = read_json(f"data/crew/{crew_file}")
        seats = [s["station"] for s in crew["seats"]]
        for c in crew["callouts"]:
            if "{" in c["ru"]:
                continue  # filled in when said: the game never plays a recording for it
            for seat in seats if c.get("from") == "any" else [c.get("from")]:
                if seat not in config["seats"]:
                    raise SystemExit(f"{CONFIG}: no voice for '{seat}', who says {crew_file} {c['id']}")
                spoken = speech_text(c["en"], say)
                line = found.setdefault(f"{seat}/{c['id']}", {
                    "seat": seat, "id": c["id"], "crews": [], "en": c["en"], "spoken": spoken,
                    "voice": config["seats"][seat], "slot_s": round(speaking_time_s(c["ru"]), 2), "words_by_crew": []})
                line["crews"].append(crew_file)
                line["words_by_crew"].append((crew_file, c["en"]))
    lines, clashes = {}, []
    for key, line in found.items():
        if len({w for _, w in line["words_by_crew"]}) > 1:
            clashes.append((key, line["words_by_crew"]))
            continue
        if key in dict(left_to_engine(config)):
            continue
        del line["words_by_crew"]
        lines[key] = line
    return lines, clashes


def left_to_engine(config):
    """Lines the engine couldn't be made to say right in their voice (a retake with "record": false), [(key, why)]:
    not recorded, so the game's speech engine says them."""
    return [(k, r.get("why", "")) for k, r in sorted(config.get("retakes", {}).items()) if r.get("record") is False]


# ---------- the word check's arithmetic ----------

ORDINALS = {"1st": "first", "2nd": "second", "3rd": "third", "4th": "fourth", "5th": "fifth"}


def normalise(text):
    words, letters = [], []
    text = re.sub(r"\b(\d)(st|nd|rd|th)\b", lambda m: ORDINALS.get(m.group(0), m.group(0)), text.replace("'", "").lower())
    for w in re.findall(r"[^\W\d_]+|\d+", text):  # "R111" is "r", "111"
        for part in ([english_digits(w)] if w.isdigit() else [w]):
            for token in part.split():
                if len(token) == 1 and token.isalpha():
                    letters.append(token)  # spelled letters ("a r m") and an initialism ("arm") compare alike
                    continue
                if letters:
                    words.append("".join(letters))
                    letters = []
                words.append(token)
    if letters:
        words.append("".join(letters))
    return words


def word_error(said, heard):
    ref, hyp = normalise(said), normalise(heard)
    if not ref:
        return 0.0 if not hyp else 1.0
    if "".join(ref) == "".join(hyp):  # "on line" heard as "online"
        return 0.0
    row = list(range(len(hyp) + 1))
    for i, r in enumerate(ref, 1):
        prev, row[0] = row[0], i
        for j, h in enumerate(hyp, 1):
            prev, row[j] = row[j], min(row[j] + 1, row[j - 1] + 1, prev + (r != h))
    return row[len(hyp)] / len(ref)


def vocabulary(lines):
    """A hint for the transcriber: the crew's longer words ("azimuth", "launcher"), not any one line's, so it knows the
    jargon without being told what a line should say."""
    words = sorted({w.strip(".,!?:;«»'").lower() for line in lines.values() for w in line["en"].split()
                    if len(w.strip(".,!?:;«»'")) >= 6 and "{" not in w})
    return "An air defence crew on the intercom: " + ", ".join(words) + "."


def judge_words(key, entry, heard_right):
    """The entry's flags with the word check settled: "words" if the transcript is off, "heard right" if someone listened
    to this very recording (its transcript is the one they approved) and it was said right."""
    flags = [f for f in entry["flags"] if f not in ("words", "heard right")]
    if entry.get("word_error", 0.0) > MAX_WORD_ERROR:
        flags.append("heard right" if heard_right.get(key) == entry.get("transcript") else "words")
    return sorted(flags, key=FLAGS.index)


def retake_of(config, key):
    """The line's retake (recorded_voices.json "retakes"): speed, phonemes, style_length; None if it has none."""
    retake = config.get("retakes", {}).get(key)
    return {k: v for k, v in retake.items() if k != "why"} if retake else None


def settings_key(line, config, hashes):
    key = {"spoken": line["spoken"], "engine_text": engine_text(line["spoken"]), "voice": line["voice"],
           "speed": config["speed"], "accent": config["accent"], "model": hashes["model"],
           "voice_pack": hashes["voice_pack"]}
    retake = retake_of(config, f"{line['seat']}/{line['id']}")
    if retake:
        key["retake"] = retake
    return key


# ---------- recording ----------

def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def finish(samples, rate, np):
    """Trim the silence, set the level; returns the samples, the seconds of speech and whether the engine clipped.
    The engine's samples are floats and may go past 1 unharmed (scaled down here); clipped is a flattened peak: three
    equal samples in a row near full scale."""
    loud = np.abs(samples[:-2]) >= 0.98
    clipped = bool(np.any(loud & (samples[:-2] == samples[1:-1]) & (samples[1:-1] == samples[2:])))
    hop = max(1, int(0.01 * rate))
    count = len(samples) // hop
    if count == 0:
        return samples, 0.0, clipped
    frames = samples[:count * hop].reshape(count, hop)
    level = np.sqrt(np.mean(frames ** 2, axis=1))
    active = level > max(float(np.max(level)) * 10 ** (-45 / 20), 1e-5)
    if not active.any():
        return samples[:0], 0.0, clipped
    first, last = int(np.argmax(active)), int(count - 1 - np.argmax(active[::-1]))
    begin = max(0, first * hop - int(LEAD_S * rate))
    end = min(len(samples), (last + 1) * hop + int(TAIL_S * rate))
    out = samples[begin:end].astype(np.float64)
    rms = float(np.sqrt(np.mean(frames[active].astype(np.float64) ** 2)))
    out *= 10 ** (TARGET_RMS_DB / 20) / max(rms, 1e-9)
    peak = float(np.max(np.abs(out)))
    if peak > 10 ** (PEAK_DB / 20):
        out *= 10 ** (PEAK_DB / 20) / peak
    return out, float(np.sum(active)) * hop / rate, clipped


def write_wav(path, samples, rate, np):
    path.parent.mkdir(parents=True, exist_ok=True)
    pcm = np.clip(np.round(samples * 32767.0), -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm.tobytes())


def level_db(samples, np):
    rms = float(np.sqrt(np.mean(samples ** 2))) if len(samples) else 0.0
    peak = float(np.max(np.abs(samples))) if len(samples) else 0.0
    return round(20 * np.log10(max(rms, 1e-9)), 1), round(20 * np.log10(max(peak, 1e-9)), 1)


def review_page(path, out_dir, manifest):
    rel = Path(os.path.relpath(out_dir, path.parent))  # the page and the files served together (.claude/launch.json)
    order = ("empty", "clipped", "words", "heard right")
    lines = sorted(manifest["lines"].items(),
                   key=lambda kv: (min([order.index(f) for f in kv[1]["flags"] if f in order], default=len(order)), kv[0]))
    rows = []
    for key, m in lines:
        flags = ", ".join(m["flags"]) or "ok"
        heard = html.escape(m.get("transcript", "")) if "transcript" in m else "<i>not checked</i>"
        if "words" in m["flags"] or "heard right" in m["flags"]:
            ticked = " checked" if "heard right" in m["flags"] else ""
            heard += (f"<br><label><input type='checkbox' data-key='{html.escape(key, quote=True)}' "
                      f"data-heard='{html.escape(m['transcript'], quote=True)}'{ticked}> said right</label>")
        rows.append(
            f"<tr class='{'flag' if any(f in FAILURES for f in m['flags']) else ''}'>"
            f"<td>{html.escape(key)}<br><span>{m['voice'][3:]}</span></td>"
            f"<td>{html.escape(m['en'])}<br><span>{html.escape(m.get('phonemes', ''))}"
            f"{'<br>retake: ' + html.escape(json.dumps(m['settings']['retake'], ensure_ascii=False)) if m.get('settings', {}).get('retake') else ''}"
            f"</span></td><td>{heard}</td>"
            f"<td>{flags}</td><td>{m['duration_s']:.1f} s / {m['slot_s']:.1f} s</td>"
            f"<td><audio controls preload='none' src='{(rel / (key + '.wav')).as_posix()}'></audio></td></tr>")
    counts = {f: sum(f in m["flags"] for m in manifest["lines"].values()) for f in FLAGS}
    summary = ", ".join(f"{n} {f}" for f, n in counts.items())
    clashes = "".join(f"<li>{html.escape(k)}: " + "; ".join(html.escape(f"{c}: {w}") for c, w in v) + "</li>"
                      for k, v in manifest["clashes"])
    clashes += "".join(f"<li>{html.escape(k)}: left to the game's speech engine ({html.escape(why)})</li>"
                       for k, why in manifest.get("left_to_engine", []))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f"""<!doctype html><meta charset="utf-8"><title>Crew voice lines</title>
<style>body{{font:14px system-ui,sans-serif;margin:24px;background:#fff;color:#222}}table{{border-collapse:collapse}}
td,th{{padding:4px 8px;border-bottom:1px solid #ddd;text-align:left;vertical-align:middle}}span{{color:#777}}
tr.flag td{{background:#fff4e5}}audio{{width:240px}}textarea{{width:100%;max-width:720px;height:6em}}</style>
<h1>Crew voice lines ({manifest['language']}, {len(manifest['lines'])} files)</h1>
<p>Checks: {summary}. "long": longer than the bus's time for the line (audio / slot). Under the words: the phonemes
the engine was given.</p>
<p>Not recorded (one file for two lines, or left to the game's speech engine):</p><ul>{clashes or '<li>none</li>'}</ul>
<p>Lines flagged "words" that you listened to and heard said right: tick "said right", then put this in
data/crew/recorded_voices.json as "heard_right" and run the tool again.</p>
<textarea id="heard" readonly></textarea>
<table><tr><th>Seat/line</th><th>Words</th><th>Heard back</th><th>Checks</th><th>Length</th><th></th></tr>{''.join(rows)}</table>
<script>
const boxes = document.querySelectorAll("input[data-key]");
function show() {{
  const right = {{}};
  boxes.forEach(b => {{ if (b.checked) right[b.dataset.key] = b.dataset.heard; }});
  document.getElementById("heard").value = '"heard_right": ' + JSON.stringify(right, null, 2);
}}
boxes.forEach(b => b.addEventListener("change", show));
show();
</script>
""", encoding="utf-8")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--models", default=str(ROOT / "out/voice_gen/models"))
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--only", nargs="*", default=None)
    ap.add_argument("--no-transcribe", action="store_true")
    ap.add_argument("--review", default=str(ROOT / "out/voice_gen/review.html"))
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args(argv)

    config = read_json(CONFIG)
    lines, clashes = plan(config)
    out_dir = ROOT / "art/crew_voices" / config["language"]
    manifest_path = out_dir / "manifest.json"
    old = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {"lines": {}}

    if args.list:
        for key, line in sorted(lines.items()):
            print(f"{key:45} {line['voice']:11} {line['spoken']}")
        for key, versions in clashes:
            print(f"{key:45} NOT RECORDED: {versions}")
        for key, why in left_to_engine(config):
            print(f"{key:45} NOT RECORDED, the game's speech engine says it: {why}")
        print(f"{len(lines)} lines, {len(clashes) + len(left_to_engine(config))} left out")
        return 0
    if args.check:
        problems = []
        for key, line in sorted(lines.items()):
            made = old["lines"].get(key)
            if made is None:
                problems.append(f"missing: {key}")
            elif ((made["spoken"], made["voice"], made.get("settings", {}).get("retake"))
                  != (line["spoken"], line["voice"], retake_of(config, key))):
                problems.append(f"stale: {key}")
        problems += [f"to remove: {k}" for k in sorted(old["lines"]) if k not in lines]
        for p in problems:
            print(p)
        if problems:
            print("run: python tools/voice_lines.py (with the voice environment's Python)")
        return 1 if problems else 0

    import numpy as np
    from kokoro_onnx import Kokoro

    models = Path(args.models)
    hashes = {"model": sha256(models / config["model"]), "voice_pack": sha256(models / config["voice_pack"])}
    kokoro = Kokoro(str(models / config["model"]), str(models / config["voice_pack"]))
    whisper, hint = None, vocabulary(lines)
    if not args.no_transcribe:
        from faster_whisper import WhisperModel
        whisper = WhisperModel(str(models / "whisper-base.en"), device="cpu", compute_type="int8")

    manifest = {"generator": "tools/voice_lines.py", "config": CONFIG, "language": config["language"],
                "engine": config["engine"], "model": config["model"], "model_sha256": hashes["model"],
                "voice_pack": config["voice_pack"], "voice_pack_sha256": hashes["voice_pack"],
                "speed": config["speed"], "accent": config["accent"], "lines": {},
                "clashes": [[k, v] for k, v in clashes],
                "left_to_engine": [[k, why] for k, why in left_to_engine(config)]}
    # Files an earlier run made that no line wants now (or that two crews would share): removed, so the game can't
    # play them. Only the manifest's own files: a hand-recorded one stays.
    for key in old["lines"]:
        if key not in lines and (out_dir / f"{key}.wav").exists():
            (out_dir / f"{key}.wav").unlink()
            print(f"removed {key}")

    start, made = time.time(), 0
    for key, line in sorted(lines.items()):
        path = out_dir / f"{key}.wav"
        prev = old["lines"].get(key)
        wanted = args.only is None or key in args.only
        same = prev is not None and prev.get("settings") == settings_key(line, config, hashes) and path.exists()
        if not wanted or (same and not args.force):
            if prev is not None:
                manifest["lines"][key] = prev
            continue
        said = engine_text(line["spoken"])
        retake = retake_of(config, key) or {}
        voice = line["voice"]
        if "style_length" in retake:  # every row of the voice set to the style of an input that long
            style = kokoro.get_voice_style(voice)
            voice = np.repeat(style[retake["style_length"] - 1:retake["style_length"]], len(style), axis=0)
        samples, rate = kokoro.create(retake.get("phonemes", said), voice=voice, speed=retake.get("speed", config["speed"]),
                                      lang=config["accent"], is_phonemes="phonemes" in retake)
        samples, speech_s, clipped = finish(np.asarray(samples, dtype=np.float32), rate, np)
        write_wav(path, samples, rate, np)
        rms_db, peak_db = level_db(samples, np)
        entry = {"seat": line["seat"], "id": line["id"], "crews": line["crews"], "en": line["en"],
                 "spoken": line["spoken"], "voice": line["voice"], "settings": settings_key(line, config, hashes),
                 "phonemes": retake.get("phonemes") or kokoro.tokenizer.phonemize(said, config["accent"]),
                 "duration_s": round(len(samples) / rate, 2), "speech_s": round(speech_s, 2),
                 "slot_s": line["slot_s"], "rms_db": rms_db, "peak_db": peak_db, "flags": []}
        if speech_s < MIN_SPEECH_S:
            entry["flags"].append("empty")
        if clipped:
            entry["flags"].append("clipped")
        if whisper is not None:
            # The samples at Whisper's 16 kHz, not the file: its file reader (PyAV) breaks between versions. Heard
            # twice, without and with the crew's vocabulary; the closer of the two counts.
            heard = np.interp(np.linspace(0, len(samples) - 1, int(len(samples) * 16000 / rate)),
                              np.arange(len(samples)), samples).astype(np.float32)
            best = None
            for prompt in (None, hint):
                segments, _ = whisper.transcribe(heard, language="en", beam_size=5, initial_prompt=prompt)
                text = " ".join(s.text.strip() for s in segments).strip()
                error = word_error(line["spoken"], text)
                if best is None or error < best[1]:
                    best = (text, error)
            entry["transcript"], entry["word_error"] = best[0], round(best[1], 2)
            if entry["word_error"] > MAX_WORD_ERROR:
                entry["flags"].append("words")
        if entry["duration_s"] > line["slot_s"]:
            entry["flags"].append("long")
        entry["flags"] = judge_words(key, entry, config.get("heard_right", {}))
        manifest["lines"][key] = entry
        made += 1
        print(f"{key:45} {entry['duration_s']:4.1f} s  {', '.join(entry['flags'])}", flush=True)

    for key, entry in manifest["lines"].items():  # a line heard right since it was recorded (or no longer)
        entry["flags"] = judge_words(key, entry, config.get("heard_right", {}))
        entry.setdefault("phonemes", kokoro.tokenizer.phonemize(entry["settings"]["engine_text"], config["accent"]))
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
    review_page(Path(args.review), out_dir, manifest)
    counts = {f: sum(f in m["flags"] for m in manifest["lines"].values()) for f in FLAGS}
    print(f"{made} recorded in {time.time() - start:.0f} s, {len(manifest['lines'])} in all; "
          + ", ".join(f"{n} {f}" for f, n in counts.items()) + f"; {len(clashes)} left out (one file for two lines)")
    for key, versions in clashes:
        print(f"  left out {key}: {versions}")
    for key, why in left_to_engine(config):
        print(f"  left to the game's speech engine {key}: {why}")
    return 1 if any(counts[f] for f in FAILURES) else 0


if __name__ == "__main__":
    sys.exit(main())
