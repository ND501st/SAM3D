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

## FM 100-2-3 (added 11 October 2026, read by hand)

The user supplied the manual (`reference/sources/fm100-2-3(91).pdf`, 456 pp.; text extracted to
`reference/sources/fm100_2_3_extract/`, not in git). Used for `blue_rwr_nato_view`, which now holds thirteen entries
(SA-8 search, tracking and command-guidance radars; SA-6 Straight Flush; SA-4 Pat Hand; SA-3 Low Blow; SA-2 Fan Song;
Flat Face / Squat Eye; Long Track; Thin Skin; Spoon Rest), each with a page citation and **low** confidence.

* SA-8 (p. 5-132): the Land Roll "probably operates in the H band" with "around 30 kilometers" against a typical target;
  the tracking radar is pulsed, "probably ... J band", 20 to 25 km; two I-band guidance radars, "each one responding to
  a different frequency to frustrate ECM"; slant range 12 km; SA-8a 1974, SA-8b seen 1980; regiment of 20 TELARs in five
  batteries plus ten resupply vehicles.
* Radar table (p. 5-138): letter bands only, e.g. Land Roll H (acquisition) / J (tracking), Flat Face C (UHF) 250 km,
  Long Track E (over 150 km), Straight Flush G/H acquisition and I tracking (60 to 90 km), Thin Skin H (240 km), Pat Hand H,
  Low Blow I (40 to 85 km), Fan Song E/F and G, Spoon Rest A (VHF).
* The letters are read with the contemporary NATO/ECM scheme (C 0.5-1, E 2-3, G 4-6, H 6-8, I 8-10, J 10-20 GHz). The
  manual does not give GHz; that conversion is mine. It agrees with the manual's own "C (UHF)" for Flat Face and with the
  Wikipedia P-19 band (830-875 MHz).
* **Not gospel.** It is what the US Army believed in 1991. Against the Polish manual and Korovin: its search range (about
  30 km against a typical target) sits inside the Polish 20-35 km and Korovin's 27-40 km; its tracking range (20-25 km)
  matches Korovin's 23 km. Its bands are hedged with "probably". Nothing from it enters the Osa's own data except the
  NATO-view library entries.
* Several Soviet radars share a letter band, so an RWR matching by band cannot tell them apart: the test shows the NATO RWR
  calling the Osa's search radar an SA-8 with under 50 % confidence (H band is also Pat Hand and Thin Skin).
* The radio table (p. 5-234) is a low-resolution scan I could not read reliably; the R-123M row appears to give 1 W and
  6-8 km, against 20 W and 12-15 km in the Polish manual. Not used.

## Next data steps

1. ZSU-23-4, SA-9/13 and the Soviet radios from FM 100-2-3 when a legible scan of the tables is at hand.
2. Add the other period NATO emitters that Osa-scenario aircraft will carry: F-4E APQ-120, Tornado, A-7, F-16 as
   named variants only when a source gives them distinct bands/powers; until then the class entry stands.
3. HARM/Shrike seeker bands and their ESM-visible behaviour (phase E6 needs these).
4. Red jammers (SPN-30, R-330) and Warsaw Pact radios beyond the R-123M (R-111, R-173) for the blue side's COMINT.
