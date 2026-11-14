# Damage by chance and the guns' fire by range (roadmap R1, R2)

The project owner's review of the dynamic campaign test (2 Nov 2026, the roadmap's track R) found two faults:

- every burst near an aircraft either killed it or did nothing;
- the guns were as accurate at 3 km as at 300 m.

Two models answer them, each a header in simcore with a data file. Together they decide what every shell and missile
does in the testbed:

| Model | Header | Data |
|---|---|---|
| Damage | `simcore/damage.hpp` | `data/gameplay/damage.json` |
| The guns' fire by range | `simcore/aaa.hpp` | `data/gameplay/aaa.json` |

The scenario runner plays both for the neighbours' guns and missiles of a scenario with `"waves"`, when its
`test_waves.json` names `"damage"` and `"aaa"`. Unreal draws the bursts simcore computed.

The research behind the numbers is in `docs/research/AAA_DAMAGE_2026-11.md`. Every value carries a source and a
confidence. Most are estimates or game values, to be tuned.

## 1. Damage states (`damage.hpp`)

An aircraft's damage is one of five levels:

| Level | What it means |
|---|---|
| none | |
| light | largely unaffected |
| moderate | may trail smoke, jettison its tanks or load, and turn for home |
| heavy | trails smoke, may burn; crashes 10 s to 5 min later unless it gets home |
| destroyed | brought down outright |

The owner's rule accumulates them:

- three light make a moderate;
- two moderate make a heavy.

A strike's level only ever rises.

### A burst's fragments

A shell or warhead of mass *M* with charge *W* (kg) bursts at miss distance *d* from an aircraft. The aircraft
presents area *A* = `area_per_size2` · size² to the burst (about 20 m² for a fighter of 10 m).

The number of fragments that strike it is Poisson with mean

    λ = N · A / (4π d²) · exp(−d / R)

where:

- the lethal fragments are N = `fragments_per_kg` · (*M* − *W*);
- for a missile's warhead, N = `missile_fragments_per_kg` · *W*;
- the fragments' reach is R = `fragment_reach_m` · *W*^(1/3), the losses to drag and the falling lethality of slower
  fragments together.

Each strike, worst first, is:

| Strike | Chance | Effect |
|---|---|---|
| critical | `fragment_critical` | destroys outright with `critical_destroys`, else heavy |
| serious | `fragment_serious` | a moderate |
| light | the rest | a light |

The blast acts alone inside its own radii:

- destroyed inside `blast_destroy_m` · *W*^(1/3);
- heavy inside `blast_heavy_m` · *W*^(1/3).

So a 130 mm shell (3.5 kg of charge):

- 8 m away brings a fighter down;
- 20 m away does so about half the time;
- 50 m away almost never.

This is the owner's "flak bursting 5-50 m away often does nothing" (`test_damage.cpp` prints the shares).

### A contact round's hit

A 23 or 57 mm round that strikes is judged by `round_hits` for its calibre:

- critical, with `critical`;
- serious, with `serious`;
- light otherwise.

Several hits in one group are judged one by one and accumulate.

### A missile

The calibrated kill chances of 6.7 stay, so the totals of aircraft brought down do not move:

- **The missile kills.** It is destroyed outright, or, with `missile_heavy_share`, heavily damaged and falling later.
- **The missile misses.** With `missile_near_burst`, its fuze still bursts it 15-60 m off. That burst is judged as a
  burst, but can do no worse than moderate.

### What follows

When the damage reaches moderate, the aircraft draws its effects:

- smoke (`moderate_smoke`);
- jettisoning its tanks and load (`moderate_jettison`);
- turning for home (`moderate_abort`).

When it reaches heavy, it draws:

- fire (`heavy_fire`);
- jettisoning (`heavy_jettison`);
- whether it crashes (`heavy_crash`) and when, 10-300 s later.

## 2. The guns' fire by range (`aaa.hpp`)

### The fire control's errors

A gun's fire control (`fire_controls` in `aaa.json`) makes three errors, drawn once per pass:

- **the aim across the line of sight** (`aim_mrad`), which grows with range;
- **the target's velocity as measured** (`speed_error`, a share of its speed), which carries the shell's time of
  flight into a miss: 5% of 250 m/s over a 4 s flight is 50 m;
- **the range as measured** (`range_error`, a share).

An optical sight (`optical`) works by day only:

- by night its errors grow by `night_factor`;
- a layer watching his tracers walks the errors down as exp(−`correction_per_s` · t) through a burst.

### The gun's fire

The gun lays on the prediction: lead and ballistics come from `fx::LeadAim` and `fx::RoundAt`, with drag scaled to the
calibre. Each round then scatters by the gun's own `dispersion_mrad`.

The prediction assumes the target flies straight. A target that turns is not where the shells arrive, so the miss grows
with its manoeuvre and the time of flight.

**Contact-fuzed guns** (ZSU-23-4, ZU-23, S-60) are worked a group of rounds at a time:

- The group's centre is the aimed point. It is judged against where the aircraft actually is when the rounds arrive.
- Each round's chance to strike is

      p = (1 − exp(−r² / 2s²)) · exp(−m² / (2(s² + r²/2)))

  where *s* is the scatter at that range, *r* the aircraft's radius and *m* the group's miss. It falls with the square
  of the range through *s* and *m*.
- The rounds self-destruct after their `self_destruct_s` from `munitions.json`.

**Time-fuzed guns** (the KS-30) are worked shell by shell:

- Each shell's fuze is set to the predicted time of flight, with `fuze_error_s`.
- Each shell bursts where it then is. The burst is judged by §1's fragments against the aircraft at that moment.
- A 0.15 s fuze error is about 100 m along the trajectory at 5 km. Most heavy shells burst well clear, which is what
  history recorded: the sky full of bursts, few aircraft hit.

## 3. What it gives (`aaa_bench`)

    ./simcore/build/aaa_bench data 1000

The bench flies a fighter of 20 m² across each gun's front at 250 m/s, straight and in a 4 g turn. It counts the rounds
fired per aircraft brought down (destroyed, or heavy and going down). With R0's values, at 2000 passes:

| Gun (fire control) | Rounds per aircraft down, by slant range |
|---|---|
| ZSU-23-4 (RPK-2) | 800 m: ~1 400; 1.5 km: ~3 800; 2.5 km: ~16 000; 3.5 km: none |
| ZU-23 (optical sight) | 800 m: ~7 400; 1.5 km: ~12 000; 2.5 km and beyond: almost none |
| S-60 (SON-9) | 800 m: ~700; 1.5 km: ~1 700; 2.5 km: ~4 000; 3.5 km: ~12 000 |
| KS-30 (SON-30), 30 s engagements at 5 km height | 6-9 km: ~6 000-12 000 (one or two kills in the sample); 12 km and beyond: none |

A 4 g turn cuts the light guns' kills two to forty times.

The historical bands to set these beside (`docs/research/AAA_DAMAGE_2026-11.md` §7, from `westermann_2000`):

- light flak 4 500-6 500 rounds per kill (worst about 20 000);
- heavy radar-directed guns 2 500-4 500 (worst 16 000).

Those are averages over whole engagements, mostly at middle and long range. The bench's light guns pass through that
band between 1.5 and 2.5 km and are deadlier inside it, which is the owner's "close range deadly, long range a sky full
of bursts". The S-60 is still deadlier up close than the band. The KS-30 is too few kills in the sample to judge; its
SON-30 has no public figures and stays the SON-9's estimate.

These figures are the model against one geometry, not a battle.

The session test's first 30 minutes give a ratio to set beside R0's 2-3 damaged aircraft for each one lost to guns:

- 25 752 rounds fired, 35 struck;
- damage risen to light 10 times, moderate 18 and heavy 7;
- 38 missiles burst near after a miss;
- 8 loads jettisoned.

## 4. In the session and the game

The runner fires a neighbour's guns every step through a pass (`FireGunPass`):

- a contact gun's groups are judged when they arrive (`EndGunRounds`);
- a time-fuzed gun's shells burst one by one, each pushed to the feed as a `FlakBurst`.

A missile's end is drawn by its damage (`EndNeighbourMissile`), as `Hit`, `NearBurst` or `SelfDestruct`.

When damage rises (`OnDamage`):

- the debrief says so;
- the feed gets an `AircraftDamaged` (moderate or heavy: its smoke and fire);
- a jettison releases the aircraft's loads, which fall inert (`TestWaves::Jettison`, delivery `"jettison"`);
- an abort sends it straight for its way out;
- a heavy aircraft's crash timer brings it down later (`CrashDamaged`).

Unreal (`SimFxEffects.cpp`):

- draws a contact gun's tracers with the pass's own errors, so the tracers miss as simcore's rounds do;
- draws each `FlakBurst` where simcore burst it;
- trails smoke and fire from a damaged aircraft;
- lands jettisoned stores as a puff of dirt.

## 5. Left

- The Osa's own resolver still destroys outright on a hit: its hits are not yet split between destroyed and heavy.
- The air war's abstract resolution does not keep damaged aircraft, and the enemy does not repair them (R1's
  "availability").
- The bridge mission's guns (`MissionFx`) still scatter their flak by `effects.json`'s `flak_offset_m`. Only the
  testbed's guns are computed.
- Our crews' journal does not yet record "hit" for a smoking aircraft that flies on (R1's first-person record).
- The defensive reactions to flak and warnings are R4.
