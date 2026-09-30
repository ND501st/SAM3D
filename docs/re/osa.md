# 9K33 Osa — SAM Simulator findings

Source: SAM Simulator v930.2 data files `osa001.dat`–`osa245.dat` (204 files), viewed locally from
`reference/samsim/` (git-ignored). Only facts and label transcriptions are recorded here. **No art, sounds or
data from the game are committed.** `sam.exe` has not been examined.

## What the files contain

| Kind | Count | Notes |
|---|---|---|
| JPEG | 184 | Control sprites (one image per position), scope frames, target photos and cards, maps |
| BMP | 15 | Scope backgrounds and overlays, blank screens, map layers |
| WAV | 5 | Two long loops (~10 MB, ~3.4 MB) and three short effects |

There is **no full console background**. Each control is a small sprite with one image per position, so the
console layout (where each sprite sits) and all the behaviour are in `sam.exe`'s forms. The install has no manual
or help file.

## Control types seen

These drive what `simcore` must support. Guarded and stepwise controls were added to `ControlPanel` because of them.

| Type | Example sprites | States | simcore model |
|---|---|---|---|
| Guarded push-button (hinged cover) | `osa010`–`011` ПЕРЕДАТ. ВКЛ. | cover closed / open | cover is its own control; button has `guard_id` |
| Guarded toggle (hinged cover) | `osa020`–`021` ВКЛ.; `osa034`–`035` СПК ВКЛ | cover closed / open | same |
| Guarded emergency button | `osa042`–`043` АВАРИЙНЫЙ ПУСК | cover closed / open | same |
| Key switch | `osa195`–`197` | no key / key in / key turned | 3 positions, `stepwise` |
| Rotary selector, 3 positions | `osa014`–`016` (next to ГОРИЗ. СМЕЩ.), `osa031`–`033` БОРТ | 3 | `positions = 3` |
| Rotary selector, 7 positions | `osa024`–`030` (centre ОТКЛ.) | 7 | `positions = 7` |
| Rotary with a 5-way voltage check | `osa050`–`052` +27В … | 3 sprites | `positions = 3` (TBC) |
| Lever / toggle, 3 positions | `osa017`–`019` range: 0–15 KM / 0–35 KM / 10–45 KM | 3 | `positions = 3` |
| Toggle, 2 positions | `osa036`–`041`, `osa044`–`049` (ПОДСВЕТ…, ОТКЛ., ШТ.) | 2 | default |
| Indicator lamp | `osa094`–`097` (β, ε) | off / lit | output from `Equipment`, not a control |

## Label transcriptions

Read from low-resolution sprites; partial words are marked with "…". Translations are literal.

| Label | Meaning | Where |
|---|---|---|
| ПЕРЕДАТ(ЧИК) ВКЛ. | transmitter on | `osa010` |
| СПК ВКЛ | "SPK" on (abbreviation not yet identified) | `osa034` |
| АВАРИЙНЫЙ ПУСК | emergency launch | `osa042` |
| УГОЛ МЕСТА | elevation angle | `osa042` |
| ГОРИЗ. СМЕЩ. | horizontal offset | `osa014` |
| ДАЛЬНОСТЬ 0÷15 KM / 0÷35 KM / 10÷45 KM | range scale | `osa017` |
| БОРТ | "board / on-board" | `osa031` |
| +27В ОБЩ., +27В ОТКЛ. ДУС I / II, +27В ИСХ. ДР. I / II | +27 V common; +27 V off, DUS I/II; +27 V source, DR I/II | `osa050` |
| ПОДСВЕТ… | backlight / illumination | `osa036`, `osa038` |
| ОТКЛ. | off | several |
| β, ε | azimuth and elevation (lamps) | `osa094`–`097` |

TV sight monitor, `osa220` (388×287, screen area blank for the game to draw into):

| Label | Meaning |
|---|---|
| РАБ. РЕЖ. ВКЛ. / ВЫКЛ. | operating mode on / off |
| ЯРКОСТЬ | brightness (two knobs) |
| ПЕРЕКРЕСТ. | crosshair |
| ЗАКР. (arrow) | closed (knob direction) |
| СВЕТОФИЛЬТР | light filter |
| КИНЕСКОПА | (of the) picture tube |
| ФОКУС | focus |
| ВИДИКОНА | (of the) vidicon |
| УГОЛ ЗРЕНИЯ ШИРОКИЙ / УЗКИЙ | field of view wide / narrow |
| КОНТРОЛЬ / РАБОТА | test / operate |
| КОНТРАСТ | contrast |
| … ЭЛЕКТР. ПЕРЕКРЕСТ. | electronic crosshair |

## Displays

- `osa175`–`176`: round PPI-style scope face, 268×268, with a bearing scale.
- `osa053`–`089` (37 frames), `osa100`–`136` (37), `osa137`–`172` (36): rotation animation sets (antenna or indicator
  drums); if each set is one full turn, that is about 10° per frame.
- `osa098`, `osa223`: blank 256×192 screens the game draws into.
- `osa199`–`201`: target drone cards with speeds (350, 450, 850 km/h), used for training targets.

## Not known yet (needs `sam.exe` or the creator)

- Console layout: which sprites sit on which panel, and how many operator stations there are.
- Procedure order and timings: power-up, deploy, search, track, launch, stow.
- What each subsystem does when switched out of order (fault, no effect, or a message).
- Lamp logic: which state lights which lamp.

Those come from running the game and watching it, or from `sam.exe`'s forms. Decompiling needs the creator's OK
first (`REVERSE_ENGINEERING.md`).
