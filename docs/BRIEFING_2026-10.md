# Briefing: research review of SAM Simulator and the 9K33 Osa (October 2026)

For Nicholas. What I found, how SAM3D compares, what changes in the plan, and what I need you to decide. Detail and
every citation: `docs/research/OSA_RESEARCH_2026-10.md`.

## 1. Bottom line

- **The foundations are right.** The console model (covers, key, wheels, latching buttons), the two power sources,
  the ±330° turret, the instruments, the gameplay-level outcome model and the "no points, debrief only" approach all
  match SAM Simulator and the real Osa-AK.
- **The crew is wrong.** The real vehicle has five men: commander, deputy commander, senior operator, operator and
  driver. SAM3D has four seats and merges the two radar operators into one "range officer". Everything downstream
  (stations, procedures, NPC orders, cabin) depends on this, so it is the first thing to fix.
- **Timings are far too fast.** A real cold start is about 6.8 minutes (3.8 min switch-on plus a 3.0 min function
  check); we power up in about a minute. March to combat position is at most 4 minutes; reaction time 26–40 s.
- **The real Osa got most of its targets by voice radio from the battery's PU-12 command post**, not from its own
  radar. SAM Simulator models this with its plotting board; SAM3D doesn't have it at all yet.
- **Several details we modelled are wrong or missing:** the TV channel is tracked by hand (we and SAM Simulator
  automate it), the missile-guidance lamps mean different things, there is no missile load or missile selection,
  the search radar's elevation beams do nothing, and the vehicle is assumed to face north.
- **Plan change:** insert an *accuracy pass* before any more Unreal feature work, then build the *battery and PU-12*,
  then the crew/mission slice. Historic scenarios change from S-75/S-125 events to Osa ones (Angola 1987–88, Lebanon
  1982, training ranges).

## 2. What I researched

| Tier | Material | Verdict |
|---|---|---|
| 1 | NVA regiment veteran's record of FRR-11's Osa-AK (flak11.de, 2005), built on the GDR's official troop handbooks and the declassified 1985 air-defence reference book | Best source found: crew, stations, firing sequence, battery, timings |
| 1 | Kupol (Osa manufacturer) deputy chief designer, NVO, 2015 | Times, missile load, crew change on AKM1 |
| 1 | Russian article on divisional SAM regiments (wrk.ru, already used) | Crew and power, confirmed |
| 2 | A Greek Osa-AK/AKM officer ("ePap", 30+ live missiles) on the SAM Simulator forum; a Russian deputy crew chief (2012–13); a Polish defence magazine (2019) | Operator detail: lamps, TV, scopes, fuel, tactics |
| 3 | SAM Simulator: the full Osa manual, the creator's site (archived), and the 400,000-character development thread where he designed the Osa with operators | How the game does it, and why |
| 5 | Western "facts and stats" pages (GlobalSecurity etc.) | Dropped from decisions, as you asked; old citations flagged for replacement |

Worth buying or borrowing (I couldn't read them): the Belarusian State University textbook *Боевое применение ЗРК
9К33М3* (2020, ISBN 978-985-566-960-0, closed online), the GDR *Handbuch für Fla-Raketenschützen und Funkorter*
(1984) and *Handbuch für Fla-Einheiten* (1990), and the Soviet veterans' memoir collection *Куито-Куанавале.
Неизвестная война*.

## 3. Comparison

| Area | Real Osa-AK / SAM Simulator | SAM3D now | Gap |
|---|---|---|---|
| Crew and seats | 5; operators' cabin behind the driver's cab; 3 working operator seats | 4 seats, 2 operators | **High** |
| Target designation | Mainly from the battery PU-12 by voice (azimuth, range); own radar autonomously | Own radar only | **High** |
| Start-up and deploy | 3.8 min switch-on + 3.0 min check; ≤ 4 min march-to-combat; computer 100 s | ~1 min power-up | **High** |
| Missiles | 6 in containers, automatic selection (outer first), selector switch, 2 channels, ≥ 4 s apart, 15 s preparation; TZM 12, reload 6 in 6 min | Unlimited launches, one at a time | **High** |
| Guidance lamps | Per channel: СХОД → ЗАХВАТ → СР.Л → УЗКИЙ ЛУЧ → К3; ГОТОВ СПК, ВКЛ ВЫС; ammunition lamps | ЗАХВАТ/ВКЛ ВЫС mapped to preparation | **Medium** (wrong meaning) |
| Search radar | 33 rpm; 3 elevation beams; IFF; rings every 5 km (2/7/12 on 0–15) | 30 rpm; beams ignored; no IFF; rings wrong | Medium |
| TV channel | Manual tracking only; 3-mil reticle; range judged by size | Auto-lock and follow | Medium (realism) |
| Heading | Vehicle oriented with its navigation set; designation errors otherwise | Hull assumed to face north | **High** for a driving game |
| Zones and methods | Head-on, pursuit, optical "Ф"; methods T/T, Горка, Ф, Н chosen automatically | One zone box | Medium |
| Kill chances | NVA single-shot: manoeuvring 0.55–0.85, low strike aircraft 0.12–0.55, low helicopters 0.07–0.12 | Game values (drone 0.8) | Low (calibrate) |
| Threats | Jamming (RGPO on the scope), ARMs (stop emitting, relocate), EMCON TV engagement | None | Medium |
| Logistics | Fuel (~300 l; turbine ~75–100 l/h), TZM, battery of 4 | None | Low–Medium |
| Training | 9Ф632 trainer: instructor changes targets live | None | Idea for later |
| Done right | Covers, key, wheels, buttons; GTA/take-off power; ±330°; range indicator (28 km, 1.5 km lower sweep); voltmeter; no points; ПУСК ГОТОВ relights | – | – |

## 4. Re-adjusted objectives

**Project goal (refined):** a first-person Osa-AK *battery* in the 1980s, where the player takes any of the five
seats (or the battery commander's PU-12) and the rest are NPCs working the same controls, with SAM Simulator's
console fidelity plus what it couldn't do: driving, deploying, a crew, a battery, and a 3D world.

**New phase order** (details in `DEVELOPMENT_ROADMAP.md`):

1. **Phase 6, accuracy pass (simcore, data, cabin):** five-man crew and stations; real start-up and function check;
   missile load, selection, channels and lamps; search beams, IFF and rings; vehicle heading and orientation; manual
   TV tracking; zones and guidance methods; kill chances calibrated to the NVA ranges; citations cleaned.
2. **Phase 7, battery and PU-12:** plotting board in SAM Simulator's notation; battery commander giving azimuth and
   range by radio; four vehicles per battery; TZM reload; fuel.
3. **Phase 8, crew and mission** (was phase 6): order menu, NPC bodies, one complete mission from march to stow.
4. **Phase 9, threats:** jamming, anti-radiation missiles and the emissions drill, target behaviour.
5. **Phase 10, scenarios:** training ranges (Ashuluk / Kapustin Yar style), Angola 1987–88, Lebanon 1982;
   instructor mode modelled on the 9Ф632 trainer.

**Unchanged:** the gameplay-level outcome model, public sources only, simcore first, Unreal for presentation.

## 5. Decisions

**Decided 2 Oct 2026: Nicholas adopted every recommendation below.** The creator message is drafted in
`docs/outreach/creator_message_draft.md` for him to review and send; acquiring documents (7) is his.

| # | Decision | My recommendation |
|---|---|---|
| 1 | Adopt the five-man crew and re-seat the console (breaking change to data, cabin and scenarios) | **Yes**, now, before the order menu |
| 2 | Real start-up times (minutes) | **Yes**, with time compression like SAM Simulator's F1–F4 |
| 3 | TV tracking by hand for the player | **Yes** by default; NPC operators track with a skill model; an assist option later if it's too hard |
| 4 | Calibrate hit chances to the NVA single-shot ranges | **Yes**, stated in the game-balance file as calibration targets, not as a physics model |
| 5 | Which variant: Osa-AK (SAM Simulator's) or Osa-AKM | **Osa-AK** first; AKM as a later variant |
| 6 | Scenario set: Angola 1987–88, Lebanon 1982, training ranges, instead of the S-75/S-125 events | **Yes** |
| 7 | Buy or borrow the BSU textbook, the GDR handbooks and the Angola memoirs | Your call; the BSU textbook is the most useful |
| 8 | Write to the SAM Simulator creator now, with the specific open conflicts (§10 of the research file) | **Yes**; I can draft it |

## 6. Update, 2 October: the video transcript

You sent a transcript of a long YouTube documentary on the Osa. Analysis in the research file, §12.

- **What it is:** its history and technical figures closely retell Vladimir Korovin's series in «Техника и
  вооружение» (2010), which I found while checking it and added as a tier-1 source. Its battery-tactics chapter
  cites unnamed manuals, so it stays unverified until a manual turns up (the BSU textbook is the best candidate).
- **What it confirms:** 33 rpm search radar with three beams; IFF, then a short halt and auto-track; two missiles on
  one target, 3–5 s apart; the "Горка" low-target method; search on the move, fire from a short halt; and the crew
  decision: an officer commanding, and a sergeant (later a warrant officer) in the guidance seat. That supports the
  five-man crew change (decision 1).
- **What's new for SAM3D:**
  - The radar sees low targets much closer in: detection at 27 km and lock at 14 km against a target at 50 m.
  - Kill chances at 50 m are lower (0.35–0.4).
  - Battery tactics for NPC behaviour and missions: march spacing, the short-halt drill, pairs about 3 km apart,
    river crossings, ambushes, halts every 3–4 h, dug-in positions on grid power.
  - Drills against anti-radiation missiles and jamming.
  - Crew-skill timings: a battery deploying in 35–50 s from a powered march (unverified).
  - A cabin detail: red dots marking each control's starting position.
- **What I didn't adopt:** "80 missiles" per regiment (it should be 120), the 27 m minimum altitude (operators say
  25 m), the decoys against anti-radiation missiles (the video itself says they're unconfirmed), and the claims about
  jamming results in Syria.
- **Plan changes:**
  - 6.2 now models readiness states: a cold start takes minutes, a powered battery stopping from the march is quick.
    This also explains why the sources' timings conflict.
  - 6.4 and 6.7 get the low-altitude figures.
  - Phase 7 gets NPC battery tactics and prepared positions.
  - Phase 9 gets the anti-radiation and jamming drills.
  - The Angola scenario covers 1983–88, using a Soviet engineer's memoir of an Osa-AK unit there (in Korovin's
    series).

Could you tell me the video's title or channel? I'd like to cite it properly.

## 7. What I changed today

Documentation only; no code or behaviour changes. New: this briefing and `docs/research/OSA_RESEARCH_2026-10.md`.
Rewritten or updated: `PROJECT_PLAN.md`, `DEVELOPMENT_ROADMAP.md`, `HANDOFF.md`, `SOURCES.md`, `ARCHITECTURE.md`,
`re/osa.md`, `REVERSE_ENGINEERING.md`, `CLAUDE.md`, the `data/*/README.md` files, and the sources, crew and
validation sections of `data/systems/osa.json` (not read by any code).
