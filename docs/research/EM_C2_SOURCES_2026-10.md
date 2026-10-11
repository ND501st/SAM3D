# Early-warning and command sources for an Osa battery (October 2026)

Why: the first E4 design had one source, the battery's PU-12M. Armies that operated the Osa used other feeds as well, and a
vehicle that lost its feed fought on its own radar. The PU-12 stays the main source; the crew must be able to change source.
Implementation: `simcore/c2_sources.hpp`, `data/command/c2_sources.json`, scenario `c2` block (roadmap E4 / 7.8).

## What the sources say

| Source | Evidence | Tier | Used as |
|---|---|---|---|
| PU-12M battery post fed by the regiment's radars | `samsim_osa` p20-21, `flak11`, `korovin_tiv_2010` (regiment control battery: PU-12M and P-19; NVA: PU-12M, P-40, 2 × P-19, P-18) | 1-3 | The main source (`command/pu12.json`) |
| Early-warning radar sites speaking on VHF | the regiment's own EWR equipment above; no document gives the voice timings | 1 (existence) | `ewr_voice`, timings are estimates |
| Division early-warning radio net | `fm100_2_3` (SA-9 limitations, PDF p. 322): crews "depend on preliminary target data broadcast over the division early warning radio net" (US Army view, 1991) | 1 for what NATO believed | `division_ew_net` (coarse, slow, broadcast) |
| Early-warning battery in the SAM regiment | `fm100_2_3` p. 4-62, 4-66 (SA-6 regiment: 55-man target acquisition / early warning battery with Long Track, Thin Skin, R-123 radios) | 1 for what NATO believed | Organisation parallel; the SA-8 regiment is the alternative divisional SAM regiment |
| Neighbouring batteries | `video_osa` (tier 4): batteries exchange data, a jammed battery can be fed by neighbours or the regiment | 4 | `neighbour_radio` (range from `mon_2112_80`, R-123M from `data/em/osa_ak.json`) |
| Visual air-observation posts | `voj_serbia` (tier 4): Serbian VOJ branch from 1915, 126th brigade passed on the air situation in 1999. Mechanics not described | 4 | `observation_post`, every number an estimate |
| Digital links to higher posts | `pu12m7_page`: PU-12M7 takes 1 analogue + 3 digital sources, 99 targets, radio 40 km, wire 15 km; `ppru1_refs`: PPRU-1 runs up to 3 batteries; `vr_1994_pvov`: Czechoslovak plan for PU-12 posts to receive the air force's automated picture | 1-4 | `data_link`, late era, off for 1980s Osa-AK |
| Decentralised, autonomous action when the picture is cut | `vr_1985_plss`: gaps in the radar system "will lead to decentralisation of warning and autonomous conduct of combat" | 1 | The fall-back to own radar; auto-fallback |

## Model decisions

* A source is a plotting board with its own coverage (relayed / own radar to the 4.12 (√h₁ + √h₂) km horizon / by eye), cadence,
  fixed navigation error plus fresh error per plot, voice timing and link range.
* One voice net carries one message at a time: sources on the same net delay one another. Only the active source is heard.
* The crew listens to one source. The Osa cabin has two R-123M sets (`mon_2112_80` Fig. 3), the basis for keeping a second net
  on watch: the commander can tell, without losing his picture, that another source is alive. The model lets him know the state
  of every listed source, which is optimistic when more than two are configured.
* Contact = a delivered transmission from the source. 2.5 / 5 reporting periods without contact = degraded / silent (the EM
  layer's thresholds). An NPC commander falls back down his list when the active source is silent, and goes back to the main
  source once it has been up for `primary_stable_s` (60 s). A player commander turns `auto_fallback` off and uses
  `RequestSource`.
* Changing takes `switch_s` (6 s, an estimate): nothing is heard meanwhile; what the other source said meanwhile is lost.
* Links: outages (scripted or `c2_fail`), link range from the vehicle, and a `C2LinkModel` (the EM link budget plugs in here).

## Not done / open

* Done 11 Oct: with an `own_unit.em` block, every source that has a `position` gets an EM radio (`radio`, default `r123m`) on a two-member net with the unit (`EmSensorLink::AddRadioSource`, `EmC2LinkModel`). Its words get through with the EM layer's delivery law (propagation, terrain, noise jamming, co-site desense from the unit's radars): in the test a source 4 km away carries (p = 1.00) and one 60 km away does not (p = 0.00). Still open: a VHF comms jammer scenario, and sources without a position (the PU-12 by default) are unaffected.
* A cabin control for the R-123M presets, and the Unreal UI for switching.
* Observation-post and EWR timings are estimates; a tier 1-2 source for VNOS practice and EWR voice procedure is still to find
  (Bogdanov 2020, Yugoslav air defence in 1999, unread).
* Fusing two sources (the crew uses one at a time).
