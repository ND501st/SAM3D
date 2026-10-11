# Sources

Public and declassified material only. Every system number in `data/` must cite one of these (by id), or a new entry
added here. Revised 2 October 2026 (`docs/research/OSA_RESEARCH_2026-10.md`, `docs/research/OSA_CONSOLE_MAP_2026-10.md`).

## Source tiers

Prefer the highest tier. When tiers disagree, use the higher one and record the disagreement in `notes`.

| Tier | Kind | Examples |
|---|---|---|
| 1 | Documents from countries that operated the system: official manuals and handbooks, manufacturer statements, unit records, historic and declassified documents | NVA handbooks and unit records (via `flak11`), Kupol's chief designer (`nvo_2015`), Soviet/Russian regiment descriptions (`wrk`) |
| 2 | First-hand operator and veteran accounts | ePap (Greek Osa officer, in `samsim_thread`), `ilya_prosto`, Polish service articles (`zbiam_2019`) |
| 3 | SAM Simulator: the creator's manuals, site and development threads | `samsim_osa`, `samsim_site`, `samsim_thread` |
| 4 | Encyclopaedic and press pages from operator countries; contemporary press reports of events | `vpvo`, `ru_wiki`, `mis`, `wapo_1982` |
| 5 | Western "facts and stats" pages | `gs`, `apa`, `janes`. **Not used for new decisions**; existing citations are being replaced (roadmap 6.8) |

## Osa sources

| Id | Tier | Source | Notes |
|---|---|---|---|
| `mon_2112_80` | 1 | *Przeciwlotniczy zestaw rakietowy 9K33M2 OSA-AK. Wiadomości ogólne. Użytkowanie zestawu*, MON, Szefostwo Służby Uzbrojenia i Elektroniki, Uzbr. 2112/80, Warszawa 1981. Marked **JAWNE** (unclassified). `reference/sources/`, not in git | The Polish Army's manual for exactly the Osa-AK: data, composition, functional description and operating modes (ЦУ, АЗ, ТСЦ, ПА, НЛЦ, beams, stabilisation), the vehicle's equipment layout (Fig. 3: seats, racks, GTA panel at the driver, antenna-lowering board at the commander), operating rules. Summary: `docs/research/OSA_MANUALS_2026-10.md` |
| `grezicki_wzu` | 2 | M. Gręzicki, J. Walczak (Wojskowe Zakłady Uzbrojenia SA, Grudziądz), *Modernizacja i modyfikacja jako determinanty rozwoju przeciwlotniczych zestawów rakietowych*, journal pp. 217–232 (after 2018). `reference/sources/`, not in git | Polish overhaul plant's paper on the Osa-P/PM upgrade. Camera views of a crew at work (Fig. 4) give the seat order and Polish crew titles (guidance, search and range operators); the equipment shown is post-1990s, so nothing else is used. Notes in `docs/research/OSA_MANUALS_2026-10.md` |
| `cabin_photos` | 2 | Photographs of real 9A33BM2/BM3 cabins: [opisybroni.pl, 9K33 Osa](https://opisybroni.pl/9k33-osa/) (museum vehicle; interior shots of all three console columns and the cab), US Air Force photo 170120-F-DV125-019 (2017, looking aft along the bench), Ukrainian 39th brigade crew photos on Instagram (devinsanluis, Jan 2026; front cab, live TV and search screens), a photo with English tape labels supplied by the user, and a photo of a Polish-modernised console supplied by the user (`user_modernised_2`: CWOO-1 flat display and BZK01 TV monitor in place of the PPI and CRT TV; the SOC/SSC readout strip, the four-meter unit, the lamp clusters and the elevation-search lamp are unchanged). Copies in `reference/sources/photos/` (not in git) | Block order of the console columns, where the scopes, wheels and launch block sit, the single bench and the cab seats. Labels mostly unreadable at these resolutions |
| `usaf_shaw_2017` | 2 | US Air Force photographs 170120-F-DV125-019 (looking aft along the operators' bench), -024 (launch-block close-up) and -036 (roof hatch), taken in a 9A33BM2 cabin in January 2017 (the `shaw` in the id is the handover's label; the location is not verified). **Public domain** (US government work); view at media.defense.gov (direct download is blocked) | Paint wear, black condensed lettering, bar knobs, toggles and lamp caps on the real blocks, and the equipment seen along the bench. Also cited inside `cabin_photos`. Used for the cabin look in `data/cabins/osa_ak.json` and `tools/cabin_textures.py` |
| `samsim_panels` | 3 | SAM Simulator v930.2 console art: straight-on photographs of the main console's three columns (range, search, guidance) embedded in `sam.exe`. Extracted to `reference/samsim/exe_img/` (not in git; the art belongs to the creator) | Positions of every operator control, lamp and screen in `data/cabins/osa_ak.json` (1 mm per pixel). Photos of real hardware with readable block plates that match `mon_2112_80` Fig. 3, so better than tier 3 for layout |
| `voenizdat_osa_1991` | 1 (status: see notes) | *Автономный войсковой зенитный ракетный комплекс «Оса»*, USSR MoD, Voenizdat, Moscow 1991; textbook approved by the C-in-C Ground Forces for PVO SV troops, on the 9K33M3 Osa-AKM. `reference/sources/`, not in git | **Not public by its markings:** copy-numbered (Экз. №), a marking above the number blacked out, imprint Изд. № 13/8918с and Зак. 2278с (a trailing "с" usually marks a secret edition). Used by the project owner's decision of 3 Oct 2026, against the public-only rule in `CLAUDE.md`; only facts with page references go into the repo, never the document or long quotations. Content: every block's controls, lamps, start positions and signs of normal operation; the power cabinet's timers; launch automation; the crew's readiness drill ("К бою"). Describes the AKM: AK differences noted. Summary: `docs/research/OSA_MANUALS_2026-10.md` |
| `flak11` | 1 | R. Wagner, *Fla-Raketenkomplex 9K33* and *PU-12* pages, [flak11.de](http://www.flak11.de/9K33-Start.htm) (2005; site of veterans of NVA Fla-Raketen-Regiment 11, Weißenfels). Pages 9K33-0, 9K33-3, 9A33-1, 9A33-3, PU12-0, 9K33-4 | Crew of five and their stations, firing sequence, battery and regiment, timings, engagement zones, PU-12. Built on the GDR handbooks below and unit chronicles |
| `nva_handbooks` | 1 | *Handbuch für Fla-Raketenschützen und Funkorter der Fla-Einheiten der Truppenluftabwehr* (Militärverlag der DDR, 1984); *Handbuch für Fla-Einheiten* (1990); *Handbuch für Planzeichner und Funkorter der Truppenluftabwehr* (1980); K.-H. Otto, *MTH Fla-Raketen* (1987); *Auskunftsbuch der Truppenluftabwehr 1985* (ex GVS B 597030, declassified) | Cited by `flak11`. **To acquire** |
| `nvo_2015` | 1 | И. Рыков (deputy chief designer, IEMZ "Kupol"), [«Смертоносная „Оса“ в новом исполнении»](https://nvo.ng.ru/concepts/2015-08-21/12_osa.html), Независимое военное обозрение, 21 Aug 2015 | March-to-combat ≤ 4 min, reaction 26–40 s, six missiles, one or two per target, AKM1 crew reduced by one |
| `korovin_tiv_2010` | 1 | В. Коровин, «Зенитный ракетный комплекс «Оса» — история создания», parts 1–3, «Техника и вооружение» (part 3: September 2010); republished on otvaga2004.ru ([part 2](http://otvaga2004.ru/tiv/zenitnyj-raketnyj-kompleks-osa-istoriya-sozdaniya-chast-2/), [part 3](http://otvaga2004.ru/tiv/zenitnyj-raketnyj-kompleks-osa-istoriya-sozdaniya-chast-3/)) | Development history from the developers' archives: crew staffing decision, search and tracking radar coverage and capture ranges, single-shot kill chances, modernisations (Osa-A, -K, -AK, -AKM), panel ergonomics. Part 3 includes the Angola memoir of A. P. Pavlov (BAZ). A 2012 compilation of all three parts with the photos and a summary table is in `reference/sources/OSA_Entw_geschichte_RU.pdf` (not in git) |
| `wrk` | 1 | «2ПВО МСД. Часть 2. Зенитные ракетные полки (зрп)», Word attachment on the wrk.ru (Aviabaza) forum ([file](https://www.wrk.ru/forums/attachment.php?item=1005313)) | Crew of five, power sources, march/combat positions, readiness norms. Unsigned; medium confidence |
| `rwd_mb3` | 1 | *Raketen- und Waffentechnischer Dienst im Kdo. MB III*, Technikkatalog, [rwd-mb3.de](https://www.rwd-mb3.de/pages/9k33.htm) (veterans of the NVA's missile and weapons technical service). Pages 9A33 (data, [interior photos](https://www.rwd-mb3.de/pages/9a33_3.htm), exterior photos, [layout drawings](https://www.rwd-mb3.de/pages/9a33_4.htm)), 9I120, 9S456, 9W611, 1S51, 1L24, 9Sch38, 9T217, PAB-2M. Cites NVA catalogue K 050/3/016 and regulation A 327/1/153 | Block designations and seat order in the operators' cabinet, power panel PU-303, switch-on panels, TV sight data, vehicle figures. Used in `docs/research/OSA_CONSOLE_MAP_2026-10.md`. Read through an automated reader: recheck designations before they enter `data/` |
| `barch_a327` | 1 | NVA regulations in the German Federal Archives (Bundesarchiv, *Vorschriften der NVA*): **A 327/1/154** *Fla-Raketenkomplex 9K33M2 (Osa-AK), Start- und Leitstation 9A33BM2, Aufklärungsstation und Stabilisierungssystem 9W611BM2* (1984; BArch DVW 1-DRUCK/4835; [record](https://www.deutsche-digitale-bibliothek.de/item/BM2XYPTT64MFFGCNBOWEM6G3QBDTA6HD)); **A 327/1/156** *… 9A33BM3, Funkkommandosender und Funkmeßgerät "Rakete"* (1985; BArch DVW 1-DRUCK/4953; [record](https://www.deutsche-digitale-bibliothek.de/item/TKXF2UJQC4UNDRLWEKRW742XNBCFCI2M)); ***Ausbildungsanleitung Fla-Raketenkomplex 9K33 M2 (Osa-AK)*** (1983; BArch DVW 2-DRUCK/130; [record](https://www.deutsche-digitale-bibliothek.de/item/PWTXKFUKMBYU54G56EDRFFJLSBAYISKC)) | The NVA's own descriptions and training instruction for the vehicle. Listed as digitised. **To acquire** (not opened yet) |
| `bsu_2020` | 1 | Ю. Б. Гринкевич, Е. П. Дударенок, *Боевое применение ЗРК 9К33М3: учеб. пособие*, БГУ, Минск 2020, ISBN 978-985-566-960-0 ([record](https://elib.bsu.by/handle/123456789/289581)) | Firing rules, combat work and fire control of an Osa-AKM battery. Closed access online. **To acquire** |
| `samsim_thread` | 2/3 | *9K33M2 OSA-AK (SA-8B Gecko)*, SimHQ forum, 2013–2022, 52 pages ([live](https://simhq.net/forum/ubbthreads.php/topics/3755797/1/9k33m2-osa-ak-sa-8b-gecko); [archived 8 Apr 2022](http://web.archive.org/web/20220408074420/https://simhq.com/forum/ubbthreads.php/ubb/printthread/Board/339/main/371999/type/thread)) | The creator ("Hpasp") designing the Osa; **ePap**, a Greek Osa-AK/AKM officer (30+ live missiles), answers as an operator (tier 2); piston79 relays an Osa-MA officer |
| `ilya_prosto` | 2 | «Из первых рук: 6-я отдельная танковая бригада (ЗВО), служба в ЗРДн», [LiveJournal](https://ilya-prosto.livejournal.com/135701.html) | Deputy crew chief, Osa-AKM, 2012–13: crew of four, drills, Kapustin Yar |
| `zbiam_2019` | 2 | M. Gyűrösi, «Modernizacja wozu bojowego Osa-AK/AKM», *Wojsko i Technika* 11/2019 ([zbiam.pl](https://zbiam.pl/artykuly/modernizacja-wozu-bojowego-osa-ak-akm/)) | Operators' cabin behind the driver's cab; why the vehicles needed modernising |
| `wwp_gecko` | 4 | *SA-8 Gecko in Detail: Russian Amphibious AA System 9K33 Osa*, Wings & Wheels Publications (Prague), ISBN 978-80-87509-73-9, 120 pp., 450+ photographs | Photographs of preserved vehicles (a Czech publisher; the Czech army used the Osa-AKM): front cab and operators' compartment of the launcher vehicle, and the reload vehicle. **To acquire** |
| `kuito_memoirs` | 2 | *Куито-Куанавале. Неизвестная война. Мемуары ветеранов войны в Анголе* | Soviet advisers in Angola, Osa-AK. **To acquire** (online copy unreachable) |
| `samsim_osa` | 3 | *9K33M2 OSA-AK (SA-8B Gecko) Simulator Documentation*, v1 (2014), SAM Simulator documentation set v210109 ([MediaFire](https://www.mediafire.com/file/cgovijhk4hukl7n/SAMSIM_docs_210109.rar)); `reference/manuals/SA-8B_English.pdf`, not in git | Console behaviour, procedures A–F, PU-12 board, missile, TZM, ARMs. Summarised in `docs/re/osa.md` |
| `samsim_site` | 3 | SAM Simulator site ([archived 5 Nov 2016](https://web.archive.org/web/20161105024427/https://sites.google.com/site/samsimulator1972/home); live site now needs a Google sign-in) | Philosophy, FAQ, version history, tutorial videos |
| `samsim_sprites` | 3 | SAM Simulator v930.2 Osa control sprites (`osa*.dat`), `reference/samsim/`, not in git | Number of positions per control. See `docs/re/osa.md` |
| `samsim_game` | 3 | SAM Simulator v930.2, running (`sam.exe`, installed on the PC; Practice "Hungary" and Live Practice "Ashuluk", 3 Oct 2026) | Start position of every sprite-backed control, click conventions, what controls do at idle. See `docs/research/OSA_CONSOLE_MAP_2026-10.md` §12 |
| `video_samsim_osa` | 3 | "SAM simulator 9K33 OSA", YouTube, uploaded by Gruya Gruja about 2016, 12:41, 360p ([link](https://www.youtube.com/watch?v=188fQ06O8vw)) | A player's full Osa engagement at the NAMFI range: range indicator, elevation indicator and TV during tracking and launch |
| `vpvo` | 4 | [Вооружение войск ПВО: ВЗРК «Оса-АКМ»](https://vpvo.narod.ru/Weapon/Zrk/Osaakm/osaakm.html) | 9И120 power unit, composition |
| `ru_wiki` | 4 | Russian Wikipedia, «Оса (зенитный ракетный комплекс)» | Cross-checks only |
| `mis` | 4 | missilery.info, [9K33M3 Osa-AKM](https://en.missilery.info/missile/osa-akm) | Russian reference site; cross-checks only |
| `valka_osa` | 4 | valka.cz / armedconflicts.com, [9K33M2 Osa-AK / 9K33M3 Osa-AKM](https://www.armedconflicts.com/9K33-Osa-t6071) and [9F632 trainer](https://www.armedconflicts.com/-t69120) | Two interior photos of the 9A33, a labelled 9A33BM2 drawing, the trainer on a ZIL-131. Reference images only |
| `nuou` | 4 | Ukrainian National Defence University museum page, 9A33BM Osa | Crew, water speed |
| `video_osa` | 4 | YouTube documentary on the 9K33 Osa (title and channel not recorded; transcript supplied by the user, 2 Oct 2026; analysis in `docs/research/OSA_RESEARCH_2026-10.md` §12) | History and figures retell `korovin_tiv_2010`; battery tactics and ARM/ECCM drills from unnamed manuals, **unverified** |
| `wapo_1982` | 4 | "Missile Sites Hit in Bekaa", *The Washington Post*, 25 July 1982 ([archive](https://www.washingtonpost.com/archive/politics/1982/07/25/missile-sites-hit-in-bekaa/6dfe0793-6bba-4654-a1f1-852d3787f234/)) | Syrian SA-8 shoots down an Israeli F-4, 24 July 1982 |
| `gs` | 5 | GlobalSecurity.org, SA-8 GECKO | To be replaced in `data/` |
| `apa` | 5 | Air Power Australia, 9K33 Osa/Romb; [Soviet SAM System Simulator](https://www.ausairpower.net/APA-Sov-SAM-Simulator.html) (review of SAM Simulator) | To be replaced in `data/`; the review stays useful for SAM Simulator's history |
| `janes` | 5 | *Jane's Land-Based Air Defence* (as quoted by the SAM Simulator creator) | Combat claims only, to be confirmed |

## Electromagnetic-layer sources (added 7 October 2026)

Used by `simcore/em/`, `data/em/` and `docs/research/EM_RESEARCH_2026-10.md`. Tiers as above; for a
*threat library* (one side's belief about the other's emitters) Western sources are the right tier,
because that is what the Western side believed.

| Id | Tier | Source | Used for |
|---|---|---|---|
| `vr_1985_plss` | 1 | V. Lhotský et al., «Možnosti ničení systému PLSS a ochrana vlastních vojsk», *Vojenské rozhledy* 1985/6, pp. 43–49 ([PDF](https://www.vojenskerozhledy.cz/pdfs/vor/1985/06/08.pdf)) | Czechoslovak army view of TDOA emitter location (PLSS/TR-1): "every newly switched-on source detected within seconds"; passive countermeasures: transmitter discipline, dummy loads, night moves, active dummies on the same parameters, roaming decoy transmitters; radar gaps → decentralised warning and autonomous combat; ARMs listed (Shrike, Standard ARM, HARM) |
| `vr_1989_tr1` | 1 | M. Janošek, «Možnosti příletu stíhacích letounů do prostoru činnosti letounu TR-1», *Vojenské rozhledy* 1989/3, pp. 48–53 ([PDF](https://vojenskerozhledy.cz/pdfs/vor/1989/03/10.pdf)) | Radio horizon R = 4.12 √H; detection-range scaling D = D_ref (σ/σ_ref)^¼; Ramona detecting emitters at 500–600 km; RCS fluctuation of 20 dB causing intermittent tracking |
| `western_bands` | 4/5 | 9K33 Osa, [English Wikipedia](https://en.wikipedia.org/wiki/9K33_Osa) (Western references): Land Roll search radar H band (6–8 GHz), tracking radar J band (14.5 GHz), "frequency agile" | Osa band centres (propagation only) and the NATO threat library; the "agile" claim is not adopted (`mon_2112_80` p67 describes fixed working frequencies) |
| `r123_refs` | 4 | R-123: [Wikipedia](https://en.wikipedia.org/wiki/R-123); [greenradio.de](https://greenradio.de/e_r123.htm) | R-123M: 20–51.5 MHz, 4 presets, 25 kHz, ~20 W, 2.5 µV |
| `czech_tdoa` | 4 | [Ramona](https://en.wikipedia.org/wiki/Ramona_passive_sensor), [Tamara](https://en.wikipedia.org/wiki/Tamara_passive_sensor) (Wikipedia); [KRTP-81 Ramona](https://www.armedconflicts.com/-t12985) (ArmedConflicts) | KRTP-81 (1979) and KRTP-86 (1987) TDOA systems: sites, band, capacity |
| `avtobaza` | 4 | [1L222 Avtobaza](https://en.wikipedia.org/wiki/Avtobaza) (Wikipedia) | Soviet ground ELINT against airborne radars (8–17.5 GHz) — a future red ESM entry |
| `soviet_iff` | 4 | [Kremniy-2](https://mil-airfields.de/equipment/soviet-iff-kremniy.htm), [Parol](https://mil-airfields.de/equipment/soviet-russian-iff-parol.htm) (mil-airfields.de); [Crypto Museum](https://cryptomuseum.com/radar/kremnij2/index.htm) | IFF families and dates (Parol in Soviet service 1977, NVA late 1980s) |
| `fh_radios` | 4 | [HAVE QUICK](https://en.wikipedia.org/wiki/Have_Quick) (Wikipedia; [Crypto Museum](https://cryptomuseum.com/radio/havequick)); [SINCGARS](https://cryptomuseum.com/radio/sincgars) (Crypto Museum) | HAVE QUICK introduced 1980 (225–400 MHz); SINCGARS RT-1439 fielded 1988 (30–88 MHz) |
| `decoys` | 4 | [ADM-141 TALD](https://en.wikipedia.org/wiki/ADM-141_TALD); [Operation Mole Cricket 19](https://en.wikipedia.org/wiki/Operation_Mole_Cricket_19) (Wikipedia) | TALD in US Navy service 1987; 1982 Bekaa decoys, RPVs and EW (secondary accounts) |
| `fm100_2_3` | 4 | US Army FM 100-2-3, *The Soviet Army: Troops, Organization and Equipment* (1991), [FAS copy](https://irp.fas.org/doddir/army/fm100-2-3.pdf) | **To read**: the NATO view for blue threat libraries |
| `games_ew` | — | [Skynet IADS](https://github.com/walder/Skynet-IADS); [MIXR](https://github.com/doughodson/mixr) and Garland et al., CSCI 2020 ([record](https://scholar.afit.edu/facpub/1857)); [Stone Soup](https://github.com/dstl/Stone-Soup); [DCS ECM analysis](https://flyandwire.com/2022/11/11/electronic-countermeasures-f-14-avionics/); [Sea Power EMCON](https://sea-power.pmctactical.org/emcon.php) | Design lessons only (`EM_RESEARCH` §12) |

## Physics textbooks (for simcore's generic models)

| Topic | Reference |
|---|---|
| Radar range equation, detection, noise | M. Skolnik, *Introduction to Radar Systems*, 3rd ed., ch. 1–2 |
| Propagation, 4/3-Earth refraction, multipath, clutter | M. Skolnik (ed.), *Radar Handbook*, 3rd ed. |
| Radar modelling with worked code | B. Mahafza, *Radar Systems Analysis and Design Using MATLAB* |
| Missile guidance (PN, command guidance, miss distance) | P. Zarchan, *Tactical and Strategic Missile Guidance* (AIAA) |
| Missile flyout, aerodynamics, propulsion | E. Fleeman, *Missile Design and System Engineering* (AIAA) |
| Standard atmosphere | ISO 2533:1975; U.S. Standard Atmosphere 1976 |
| Geodesy | NIMA TR8350.2 (WGS-84) |
| Detection statistics, measurement accuracy | M. A. Richards, *Fundamentals of Radar Signal Processing*; D. K. Barton, *Radar System Analysis and Modeling* |
| Low-angle propagation, multipath | L. V. Blake, *Radar Range-Performance Analysis*; J. D. Parsons, *The Mobile Radio Propagation Channel* (plane earth) |
| Diffraction, gases, rain | ITU-R P.526 (eq. 31, knife edge), P.676, P.838 |
| Electronic warfare | D. Adamy, *EW 101*, *EW 102*; D. C. Schleher, *Electronic Warfare in the Information Age*; R. G. Wiley, *ELINT* |
| Emitter location | R. G. Stansfield, J. IEE 94 IIIA (1947); W. H. Foy, IEEE TAES 12 (1976); D. J. Torrieri, IEEE TAES 20 (1984); R. A. Poisel, *Electronic Warfare Target Location Methods* |
| Tracking and fusion | S. Blackman, R. Popoli, *Design and Analysis of Modern Tracking Systems*; Y. Bar-Shalom et al., *Estimation with Applications to Tracking and Navigation*; S. Julier, J. Uhlmann, ACC 1997 (covariance intersection); W. Niehsen, FUSION 2002 |
| Random numbers | G. Steele, D. Lea, C. Flood, OOPSLA 2014 (SplitMix64) |

These cover the generic simcore models (geodesy, line of sight, the demo flyout). The Osa's outcomes are decided by
the gameplay-level model, not by physics (`DEVELOPMENT_ROADMAP.md`, "Decision").

## Validation scenarios (Osa)

| Event | What to check | Sources |
|---|---|---|
| Training-range shoots (Ashuluk, Kapustin Yar, Ustka, NAMFI): drones and P-15 targets | Procedures, timings, two-missile salvo, a kill near maximum range (10.3 km at Kapustin Yar, 2013) | `samsim_thread`, `ilya_prosto`, `flak11` |
| Lebanon, 24 July 1982 | First combat use: one vehicle downs an F-4E while others are attacked | `wapo_1982` (`janes` as quoted) |
| Angola, 1983–88 | An Osa-AK unit on the border and the north–south road (dug in, night moves, night bombing by Impala, Mirage, Buccaneer; a Puma low in a canyon); Cuito Cuanavale 1987–88, Mirage F1AZ 20 February 1988 | `korovin_tiv_2010` (Pavlov memoir), `samsim_osa` p31, `kuito_memoirs` |

The S-75/S-125 events (U-2 1960, Vietnam, F-117 1999) belong to later systems.

## Tools and formats

| Item | Link |
|---|---|
| Tacview ACMI 2.2 format | https://www.tacview.net/documentation/acmi/en/ |
| Tacview real-time telemetry (needs Tacview Advanced) | https://www.tacview.net/documentation/realtime/ |
| Cesium for Unreal | https://cesium.com/platform/cesium-for-unreal/ |
