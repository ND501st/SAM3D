# E1: NATO-side electromagnetic data (October 2026)

First data pass for the blue side and for each side's view of the other (`data/em/nato_1985.json`, roadmap E1).

## What is in the file

| Section | Entries | Basis |
|---|---|---|
| `emitters` | NATO fighter fire-control radar, X band (APG-63/66 class): 9.4 GHz, sector scan ±60 deg | Band, scan sector and year are cited (`nato_radar_refs`); power, PRF, pulse width, gain, beamwidth are labelled estimates of the class |
| `esm` | AN/ALR-46-class RWR (2–18 GHz); a Warsaw Pact ground ESM/DF station class (0.8–18 GHz, 1.5 deg) | Bands cited (`nato_ew_refs`, `czech_tdoa`); sensitivity and bearing accuracy are class estimates (`ew_textbooks`) |
| `jammers` | AN/ALQ-99-class stand-off noise jammer in its bands 8, 9 and 10 (4–7.8, 7.8–11, 11–20 GHz): the bands of the Osa's search radar, X-band fighter radars and the Osa's tracking radar | Band plan and 10.8 kW maximum output cited (`nato_ew_refs`); 3.6 kW per band is a derived, low-confidence split; gain is an estimate |
| `threat_libraries` | `blue_rwr_nato_view` (NATO's belief about the SA-8's two radars, and nothing else it can be sure of); `red_esm_nato_view` (the Pact's belief: X-band fighter radar with a wide PRF window, wideband noise jammer, UHF surveillance radar) | Bands from public references; PRF windows are estimates |

`simcore/tests/test_em_data.cpp` shows the two sides identifying each other in one run: a Pact ESM station names the
NATO fighter's radar and noise pod as hostile, and a NATO RWR names the Osa's calibrated search radar as an SA-8
search radar through the NATO library.

## What could not be done, and what to read

* **US Army FM 100-2-3, *The Soviet Army: Troops, Organization and Equipment* (1991)** is the intended source for
  the NATO view of Soviet emitters. The FAS copy (`irp.fas.org/doddir/army/fm100-2-3.pdf`) returned an empty
  asynchronous response from this environment, and the archive.org mirror returned 503; it was **not read**. The
  SA-8 entries therefore still rest on the same Wikipedia-level band report as before (`western_bands`, low
  confidence). To do: download the PDF by hand, extract the SA-8, SA-6, ZSU-23-4 and P-series radar tables, and
  replace the low-confidence entries with page-cited ones.
* No public source was found for the PRF, pulse width or power of the APG-63/66; they remain class estimates. A
  tier-1 route would be NATO or national air-force technical manuals; none is online.
* Soviet and Czechoslovak views of NATO jammers and ARMs: `vr_1985_plss` names Shrike, Standard ARM and HARM and
  treats TR-1/PLSS location; nothing in it gives emitter parameters.
* Operator-country sources for NATO systems (US/UK/German manuals) are mostly not public; the Wikipedia tier is the
  best available and is labelled as such.

## Next data steps

1. FM 100-2-3 by hand (above).
2. Add the other period NATO emitters that Osa-scenario aircraft will carry: F-4E APQ-120, Tornado, A-7, F-16 as
   named variants only when a source gives them distinct bands/powers; until then the class entry stands.
3. HARM/Shrike seeker bands and their ESM-visible behaviour (phase E6 needs these).
4. Red jammers (SPN-30, R-330) and Warsaw Pact radios beyond the R-123M (R-111, R-173) for the blue side's COMINT.
