# The Osa from *SA-8 Gecko in Detail* (WWP G061): page-by-page research and cross-reference (November 2026)

Read on 6 and 8 November 2026 from the project owner's 600 dpi scans (124 pages, five PDFs). Eight readers went through
every page at overview and tile resolution, with crops for stencils and plates, and wrote per-photo notes (frame, subject,
vehicle, camera azimuth and height, coverage, parts, whether a whole-vehicle camera solve is possible, problems). A ninth
searched the open web for what the book can be checked against. The notes are in `reference/sources/wwp_gecko/notes/`
(git-ignored, with the scans); this document is the synthesis. Everything is **paraphrased**; only designations, stencils,
labels and numbers are quoted. Companion: the pipeline plan, `docs/OSA_EXTERIOR_RECONSTRUCTION.md`.

## 1. The book

*SA-8 Gecko in Detail: Russian Amphibious AA System 9K33 Osa*, Wings & Wheels Publications G061 (Prague, February 2020),
text and photographs by Jan Horák and František Kořán, English, ISBN 978-80-87509-73-9, `wwp_gecko` in `docs/SOURCES.md`.
Scan page N is book page N − 2 (the cover is page 1). 450 photographs and drawings catalogued (446 with a printed frame).
Sections: introduction (book pp. 3-13), the launcher 9A33BM2/3 "walk around" (pp. 14-46: low transport mode, hull front,
sides, rear, roof, wheels, engine and generator bays), the turret and its antennas (pp. 46-78), the cab and operators'
compartment (pp. 79-99), and the reload vehicle 9T217BM2 (pp. 100-120). Most photographs are by Tomáš Bouchal (Czech
Army vehicles, 2003-2006), the rest are museum vehicles (an ex-NVA vehicle indoors, a hangar vehicle, a German hall), a
Greek launcher in Athens and Iraqi vehicles restored in the UK. The authors compare the vehicles with the Trumpeter 1/35
kit and name its errors; those are useful as a checklist of what a first model gets wrong.

**What the book is as a source.** It is a photographic walk-round of preserved Czech, German and Greek vehicles, with
short text. It is not a manual: no dimensioned drawings (two drawings are copied from manuals), few numbers. For the
project it is the first source for the **shape** of the outside and the **look and labels** of the inside, from preserved
Warsaw Pact vehicles of the late 1980s onward. Proposed tiers: the photographs of the vehicles are primary evidence of
what the hardware looked like (tier 2, in the sense of `cabin_photos`); the text is tier 4 (operator-country press, Czech
authors). Czech additions of the 2000s (below) are not 1980s Soviet fit.

## 2. Facts from the text

| Fact | Where | Cross-check |
|---|---|---|
| Development ordered 27 Oct 1960 against targets to 1 800 km/h, 0-5 km; a tracked prototype dropped; the wheeled chassis chosen Sept 1964; tests at Emba July 1970; in service 1972; Osa-AK (six missiles in containers) 1976; Osa-AKM early 1980s; production ended 1988, 1 200 built | book pp. 3-5 | Korovin (`korovin_tiv_2010`) has the same history; the BM2/BM3 dates there are 1975-80. The book's "9K33M1 with 2-4 missiles", "altitude 12 000-15 000 m" and "NII-20" are marked [sic] by the reader and are errors or garbled |
| BAZ-5937, axles 1 and 3 steered, 5D20 300 hp (220 kW) V6 diesel, 5 gears, two water jets, 9I120 power unit on the right, 320 l fuel tank on the left | p. 6 | flak11, Kochnev (web agent): 320 l, 5D20B-300. The book's "BAZ-5939" for the reload vehicle's chassis is the 9T217's own (BAZ-5939), not the launcher's |
| Crew of five: commander front right, driver front left, deputy commander and two operators on the bench behind facing the left wall | p. 6 | `mon_2112_80` Fig. 3, `flak11`: same |
| Equipment: 9Sh38-2 TV sight, 9P35M2 launcher, 9M33M missiles, 9V611BM2 antenna stabiliser, R-123M radio, R-124 intercom, PNV-57E night device. Support vehicles 9V210BM3, 9V914, 9T217BM2, 9V242-1, 9F372M2, 9F16M2 | pp. 6-7 | rwd-mb3 lists the same equipment |
| TV guidance good to 15 km in clear weather; the search radar folds down for TV-only work | p. 10 | project docs say the same of the TV channel; the book also says the radar folds while firing on the optical channel, which the reader marks as doubtful |
| **Data table:** 9K33M2 launcher 9 270 × 2 750 × 4 250 mm, 17 500 kg; **9T217BM2 9 225 × 2 782 × 3 100 mm, 16 464 kg loaded, 14 500 kg empty**; 60 km/h road, 10-30 off road, 8 water; crane 610 kg; a launcher reloaded in 5-8 min; 1-3 containers per lift | p. 102 | Length 9.27 m = flak11 (tier 1). **TZM mass: book 16 464 kg against rwd-mb3 and valka 16 564 kg**: one digit; recorded as a conflict, neither used until a third source. Launcher height 4.25 m is neither the march figure of flak11/rwd-mb3 (3.99) nor valka's 4.20 |
| Tyre 1200 × 500-508 | p. 36 | Kochnev: same (ИД-П284); 1.20 m outside, 0.50 m section, 20 in rim |
| The 9T217BM2 carries 12 containers ("4 × 3"), a three-man crew (driver, commander, crane operator in a third seat behind the driver), crane 4033B, 610 kg | pp. 101-104, 117-122 | rwd-mb3 and flak11: 12 missiles, crane 610 kg. Consistent |
| Czech TZMs left service about July 2006; last crane service April 2006 | p. 104 | Czech vehicle history, not needed |
| A Czech launcher's own record painted on its sides: in service 21.3.1984 (Žatec), retired 30.6.2006 (Strakonice), 113 missiles fired, Ashuluk 1982/85/87/89, Baltic 1995-2005 | p. 13 | local history; the live Czech firing over the Baltic (Clean Hunter 2004) is on p. 75 |

## 3. The launcher's exterior: what the photographs show

### 3.1 Whole-vehicle views (for camera solves)
Only about 19 of the 450 pictures show a whole vehicle. Candidates: pp. 2-14 (g004-a, g008-a, g009-a, g010-a/b, g011-b,
g013-a, g015-a/b, g016-a/b; **g011-a is a near-orthographic left elevation**, the best silhouette; g011 is the Greek
vehicle), g017-a (rail low transport mode), the Greek launcher on the back cover (g124-a, front right quarter), and for the
reload vehicle g102-a/b, g103-a/b and g107-b. **No photo in pages 033-096 shows enough of the hull for a whole-vehicle
solve:** the book's interest is the parts. This is the reason the plan (§1 of the exterior plan) solves cameras against a
3-D prior and treats close-ups as **part evidence**, positioned by landmarks the whole-vehicle solves provide.

### 3.2 Hull front (pp. 14-23)
- **Low transport mode for rail** (pp. 14-15): both roof decks off, the front deck stowed on the hull's right side, the
  rear deck on the left; the search antenna folds forward over the cab and the tracking antenna folds down over the open
  generator compartment, tied with a steel rope (kept in the round canister on the hull front, p. 17).
- **Breakwater** folded on the upper glacis on the march (plan trapezoid, three hinge pads and end pads, six small clips at
  the windscreen frame), raised by one hydraulic strut in a booted slot in the glacis. Under it **two battery bays** with
  lift-off X-ribbed hatches (pp. 20-23).
- **Windscreens:** two wide rounded rectangles in black rubber on a shared pillar; a brow plate ("trim vane") on a row of
  gussets with round lightening holes and U-brackets, two rectangular apertures; two wipers per screen; two **quarter
  windows with their own wipers**. The book says the right windscreen hinges open; the reader's reading of the brackets
  suggests the driver's (left): unresolved. Side windows were added from the 1975 parade (Korovin via the web agent): a
  first-issue hull had a flat front and none, so **a 1980s model carries them**.
- **Right corner only:** twin air horns on a cast pod with a guard hoop, and an antenna stalk with a two-hole diamond
  flange. Mirrors on long rods that fold flat, both corners.
- **Light boxes:** early (clear dome side lamp) and **late Czech (orange side repeater and a slit blackout lamp over the
  headlamp)**; the late form and the black anti-skid plates are Czech additions: leave off a 1980s Soviet or NVA model.
- **Nose:** vertical pressed ribs, a centre strip and two wide panels, ring bosses with hex plugs, a towing hook in a
  recessed pocket at each lower corner, shackles; the tow cable stowed as two strands across the nose top.
- **Crew hatch lid is not flat** (the kit's first error): a D-shaped bulge with a lock stud, strap strips and ribs, a hinge
  rod; the deck around it has longitudinal pressed ribs; a round canister and a flush cap beside it.

### 3.3 Hull sides, rear and roof (pp. 24-45)
- Right side: the thermal shield on hinged legs over a perforated box, a pipe with drop nipples along the edge, a
  water-jet inspection hatch behind the last wheel, and a **connector panel behind the middle wheel**, legible: 13 coaxial
  sockets bracketed "ТРУ" and Ш8-Ш22 including "ЮА", "115В 400Гц", "ТПУ", "КЗА", "ТРУ-ЗЦ" under a top-hinged door (it joins the
  launcher to the support vehicle and a diagnostic board).
- Left side: fuel-filler recesses, the external power socket box (three-pin socket and earth stud), two service caps
  behind the rear wheel (the kit has one, misplaced).
- **Rear:** two water-jet covers with fan ribs (the reader saw them pivoting at opposite ends: to be checked on other
  rear views), a gas-turbine exhaust port (a drum with a round door on one vehicle, a flat teardrop flange on the Czech
  one: two variants), a three-cell louvre box at the top, tow fittings in pockets, tools stowed across the plate.
- **Roof:** fibreglass hatches for engine, turbine and service-liquid bays; **early vs late engine-deck grilles** (diamond
  expanded metal against square mesh, two side by side); **early vs late thermal shields** (the late one chamfered at all
  corners with a downturned holed skirt and mushroom studs; both hinge at the front edge on corner legs and are propped by
  a rod); the generator compartment's X-ribbed hatch behind the turret.
- **Wheels:** front arch with a sloped front facet, mid and rear arches symmetric; every arch has three ribbed return
  faces (the kit omits them); tyre 1200 × 500-508 with two treads (standard lugs, a late chevron "new design"); hub
  covers with inflation pipe. Torsion bars run lengthwise under the belly; suspension pockets on the sides (pp. 34-37).
  Square-on arch photos (g038-a, g038-c) suit an orthographic trace.

### 3.4 The turret and antennas (pp. 46-78)
- Tracking antenna front plate shot face-on (g050-c): straight sides, an arc at the bottom, shoulders with a raised
  central tab, 10 bolts, standing 30-40 mm off the dish on 10 rods. Two round capture antennas with white windows; a
  white pyramidal horn under each is the command-link transmitting antenna (g059-d); the TV sight 9Sh38-2 with a red cap
  (the book says it is the 1S91M2's device); the colour key in the text names parts by colour of a drawing the scan
  does not contain.
- **What the book calls the "Guiding Radar" on pp. 63-69 is the search radar antenna (СОЦ, 1S51).** Rear elevation of the
  reflector (g068-a: bays, box-section chords, tube diagonals), front elevation (g069-b: seven horizontal bars, vertical
  wires), plan curvature (g068-c), the hole pattern, the feed with its two travel locks, a manual's drawing of the whole
  unit and its base box (g066-c). Empty launcher frames from above (g078-a/b) give the tube layout and the 3-clamp
  latch pitch. Containers: square section, **3 235 × 340 × 397 mm, 161.3 kg loaded** (printed on the container, p. 81),
  end covers with stencils (9М33М3, ДМ92xx, ПРОДУВКА, ПИРОБОЛТ, "29" in a triangle).
- Dummies "9М33М3Г ГАБАРИТНО-ВЕСОВОЙ МАКЕТ" and a training round "9М33М3У УЧЕБНАЯ" are shown (p. 75-76).

## 4. Interior and cab (pp. 79-99, 108-122)

All photographs of pp. 82-96 are of **one ex-East-German (NVA) vehicle whose roof plate reads 9А33БМ3-1 and whose launch
block reads 9М33М3: an Osa-AKM, though the book calls it "early production" by its colours.** Block suffixes are М1, М2
and М3 throughout. This matters: the project's console data is for the AK (9K33M2) and the AKM's differences are
recorded in `docs/research/OSA_MANUALS_2026-10.md`. New against the project's `docs/research/OSA_CONSOLE_MAP_2026-10.md`:

- **Main console СЛ-2М2**, whole photo and a full drawing (17 blocks). **The range indicator's plate reads ОС81-9М**, which
  settles an open question there. Search scope ОП81-16М3 with amplifier ОП63-6М3 (as rwd-mb3). Launch block ОО04-11М2,
  elevation block ОС52-14М2, with the IFF ЗАПРОС / КОНТР. controls legible.
- **The "lamp strip" is eight small dials:** СВР I/II Δβ Δε and АЗИМУТ / УГОЛ МЕСТА ССЦ, coarse and fine.
- **Left-wall rack СЛ-1М** (photo and drawing, five blocks), **right-wall rack СП-2М2** (18 blocks, drawing and a square-on
  photo, power blocks 0091-17М1 and 0091-18), СП-1М and СП-3 (drawings), СЛ-3М (full elevation), **power cabinet
  ОО96-9М2** (face fully legible; 25 repair toggles under a lid with a lamp), GA-3 on its pedestal, **ПУ-303** (ГТА / ГОМ /
  ИВП) with a drawing.
- **"ПРОХОД ПОД МОСТОМ" board** on the commander's dash: five buttons including "ГАБАРИТ 3800": probably the
  antenna-lowering board of `mon_2112_80`.
- Cab: the dash, DP-3B, both R-123M sets with БП-26 power supplies (the book's text says R-123M1, the plate reads
  Р-123М), a curtain, how the seats fold; driver left with a large steering wheel and light grey cabinets on both walls.
  Interior colour: light grey walls (original Soviet paint), blue-grey electrics. A late Czech overhaul relabels the dash
  and gas-turbine panel in Czech and fits black seat cushions (p. 99).
- Rear bulkhead door to the engine compartment with the ГОМ warning plate; the book's "SP-3W2" is the project's СП-3.
- Not in the project's block lists: a "ПАНЕЛЬ ИНДИКАЦИИ" box beside the guidance column; block plates ОК81-14М1 and
  СА0004-13М1 on the turret roof (single readings; confirm elsewhere).

## 5. The reload vehicle 9T217BM2 (pp. 100-122)
First good coverage of the 9T217 outside Czech/NVA catalogue pages: left and right side views (g103-a, g102-a/b), a rear
quarter (g107-b), the three-part canvas tilt (hexagonal section, front part swings up) and its frame, the crane (round
telescopic boom, box root with a luffing cylinder, stowed down the centreline on an A-frame rest; control stand with
**6+1 levers and three gauges**, drawing with labels under СТРЕЛА and ДОМКРАТЫ), a seat fixed on the crane column for the
operator, folded-out striped walkways, stabilisers behind the front wheels with round foot pads, the container racks on
two levels with a three-lug gripper, the rear deck's service-fluid bay (in place of the turbine's), and the cab with a
third seat. The BTR-60PB-PU12 is covered by WWP G041 (`g123`): a possible source for the PU-12M, to acquire.

## 6. Differences found between vehicles (so one exterior needs variants)

| Feature | Early | Late (Czech overhaul / 2000s) |
|---|---|---|
| Engine-deck grilles | diamond expanded metal | square mesh, two side by side |
| Thermal shield | rectangular, 6×3 underside grid | chamfered, holed skirt, studs |
| Tyre tread | standard transverse lugs | chevron blocks |
| Light boxes | clear dome side lamp | orange repeater and slit blackout lamp |
| Anti-skid | none | black plates on covers (Czech workshops) |
| Interior paint, seats | light grey, green leatherette | pale grey and blue ducts, black seats |
| Hull front (1970-71) | flat front, no side windows | pointed nose, side windows (from 1975) |

BM2 and BM3 look the same outside (Korovin, web agent), so **one exterior serves both**; the interior vehicle is a BM3-1.
Western-held vehicles may be ex-Iraqi or ex-NVA: prefer NVA/Soviet fittings for the 1980s.

## 7. Cross-reference with other sources

| Question | Book | Other sources | Used |
|---|---|---|---|
| Launcher length | 9 270 mm | flak11 9.27 m (tier 1); valka/ru.wiki 9.14 m (4); Kochnev 9.165 m bare chassis (4); rwd-mb3 9.72 m (garbled block) | **9.27 m ±0.05** |
| Launcher width | 2 750 mm | flak11 2.78; valka 2.80; Kochnev 2 782 | **2.78 m** (hull, tyres flush); 2.75 a rounding |
| Height, march | 4.25 m | flak11 3.99; valka/ru.wiki 4.20; rwd-mb3 3 998 | **conflict**: 3.99-4.25, decided by the side-view fit (the sources' "march" state differs in the antenna's fold) |
| Height, combat | not given | 4.85 m (flak11, valka) | 4.85 m |
| Axle spacing | not given | valka 3 075 + 2 788; flak11/Kochnev 2 800 + 2 600; web agent's own photo check 5.7-5.85 m total | **3 075 + 2 788 working value**, 2.8 + 2.6 recorded as a conflict; re-measure on g011-a |
| Track | not given | Kochnev 2 275 mm | 2.275 m (tyres flush to 2.78: consistent) |
| Tyre | 1200 × 500-508 | Kochnev: same | 1.20 m, 0.50 m, 20 in |
| Container | 3 235 × 340 × 397 mm; 161.3 kg loaded | rwd-mb3 3 235 mm; Korovin: square section | **3.235 × 0.34 × 0.397 m** (the 0.397 m is the book's) |
| Carrier elevation | - | flak11: fixed 22.3° | 22.3° |
| Missile | 3 158 mm, 126.3 kg (on the container) | rwd-mb3, Korovin: same; diameter 206-210 mm | 3.158 m, 0.208 m |
| TZM mass | 16 464 kg | rwd-mb3, valka 16 564 kg | unresolved |
| Hull height | not given | flak11/Kochnev 1.95 m; the web agent's own check 2.1-2.2 m over the middle axle | **conflict**; fit from the side view |
| Interior block names | many, legible | `rwd_mb3`, `mon_2112_80`, `voenizdat_osa_1991`, `cabin_photos` | new plates listed in §4 are additions for the console map |

Photographs not in the book: the Commons categories listed by the web agent (the IWM Duxford walk-round, CC BY 2.0, 21 photos;
Kuzmin's 9A33BM3 set, CC BY-SA 4.0; public-domain US Marine Corps antenna close-ups; the Kyiv rear view), rwd-mb3's 15
captioned NVA exterior photos and its 9T217 page (no licence for reuse: references only). **No dimensioned general
arrangement is public**; the Bundesarchiv A 327/1/153 (1984, with Anlagen 2-5) is the likely one and is not acquired
(`docs/SOURCES.md`, `barch_a327`). Paint: Soviet 4BO single green (about FS 34102, later ХВ-518); NVA olive near RAL 6003
with white four-digit numbers (6315, 5305 seen); no sourced 1980s Czech or Polish scheme (the book's Czech
vehicles are in 2000s three-tone or olive with water-based camouflage).

## 8. Conflicts and items to look at again
- Axle spacing, hull height, march height, TZM mass (§7).
- Whether the right or left windscreen hinges (p. 21); the asymmetric water-jet covers; drum or teardrop exhaust flange.
- The raised ribbed lid on the left roof (g008, g011-b) and the tall X-stiffened side panel behind the cab (g014-d, g016):
  purpose unidentified.
- g006/g007 "one vehicle" with a loaded and an empty right cradle; the g005 wagon number.
- Data plates on the hatch lids and the crane panel's second lever label are unreadable at this scan.
- The book's "early production" label for an NVA BM3-1: its colours and paint, not its equipment.
- The notes of pp. 067-073 were written before a re-check; the catalogue's vehicle column is free text (129 groups from
  450 photos), so grouping needs the person's pass before cameras are solved (a checkpoint in the plan).

## 9. What this changes in the project
1. `tools/recon/osa_9a33bm2.json` `dimensions` take the sourced values above, each with its source and sigma.
2. The console map gains the plates in §4; the AKM's interior is the book's, so `data/cabins/` stays on the AK's layout
   until the BM3's differences are applied (a task for the interior, not for this pipeline).
3. The exterior model needs early and late variants for the grilles, shield, tyres and lights; 1980s Soviet/NVA fit
   excludes the Czech additions.
4. The reload vehicle's outside can be traced from g102-g107 once the launcher is done (R5 in the exterior plan).
5. `wwp_gecko` becomes an acquired source; the web agent's candidates (`kochnev_baz`, `barch_a327_153`, `commons_osa`) are
   registered.
