# Our fighters and our ground air defence as one system, 1985: research (November 2026)

Why: the project owner's request of 15 November 2026 (the air war in its hot phase, our fighters as live actors, air-to-air
combat) and roadmap 17.2 (friendly fire when a notice or the codes did not arrive). This note answers one question: **how a
Warsaw Pact-like small nation's fighters, its national air defence and its army air defence (the Osa battery among it)
worked as one system in 1985, and how the battery learned of its own aircraft.** It also covers, briefly, how NATO tied its
ground air defence to its fighters, because the enemy's aircraft go home over their own guns and missiles.

Period rule: the war is 1985. Everything below is dated. Where a document was written after 1985 it says so and is used only
as hindsight about equipment or structure that already existed. Read 15 November 2026. Sources are listed in section 6 with
tiers as in `docs/SOURCES.md`; ids already registered there are reused. A Soviet or Pact document translated by the CIA is
the Pact's own view (tier 1); the CIA's own estimates and FM 100-2-x are the adversary's view and say so.

Limits of this pass: the CIA reading room and DTIC refused automated downloads; the archive.org copies of the CIA
translations were read instead. The session's web-search budget ran out before the Falklands, Iran-Iraq, Polish and
Hungarian searches; those are listed in section 7.

## 1. Summary for SAM3D's design

1. **Two ways to share the sky, written down by the Pact itself.** The Warsaw Pact's own staff journal (Mitronin, 1976)
   gives the rule set: cooperation **by zones** (the SAM unit's zone is everything inside the far edge of its launch zone;
   the fighters own the rest and cross the SAM zone "in compliance with a prescribed procedure") or **in one zone**, sharing
   it out by altitudes (upper levels preferably to the fighters), axes or sectors, control lines, time, and targets
   (`mitronin_1976` pp. 5-6). By zones is used against the first massed strike and over slow-moving covered objects; one
   zone by altitude when the ground air defence cannot engage every target (pp. 6, 9). This is the data model: a
   per-mission **cooperation method** with its parameters, set by the front (here, the national) air defence plan and passed
   down as combat instructions.
2. **The battery is in the zone fighters are told to stay out of.** Regimental and divisional air defence (ZSU, Strela,
   guns, and in the 1960s-70s view much of it without IFF) sits inside the SAM zones. Mitronin says fighters should not work
   in their fire zones; over dense troop air defence they get an altitude band above the SAMs' upper kill limit, and
   corridors are set "in the majority of cases", several of them, covered by units that have IFF (p. 7). So for an Osa
   battery our fighters mostly appear as aircraft **passing through** on a notified route and height, or fighting above it.
3. **The battery learns of its own aircraft by paper and voice, late.** The 1984 combat regulations for the troop air
   defence's anti-aircraft **artillery** battery (guns and ZSU; part IX, used here as the nearest analogy for the SAM
   battery, as `docs/SOURCES.md` registers it) require the battery commander to tell every man the time, place, height and direction of our aircraft's flights, the fighters' airborne duty
   zones and types, and the current recognition and fire-prohibition signals, and to update them as the senior commander
   sends them (`pvo_bu_ix_1984` art. 34); to exclude targets our fighters are attacking (art. 45); to report at once when one
   of ours is near a target about to be engaged (art. 53). The flight notices are "applications for flights" (заявки на
   полёты своей авиации); radar units compare what they see with them and report violators (`pvo_bu_x_1984` art. 56). In
   the Druzhba-76 exercise, passing our aircraft's actions from the division's air defence chief to the guns **took 10-15
   minutes** because it went by code table (`mitronin_1976` p. 11). A low-level enemy raid needed **3-5 minutes** from
   detection to reach the first echelon (`vr_roucka_1988` p. 18). The notice is slower than the enemy: that gap is roadmap
   17.2's friendly fire.
4. **IFF in 1985 is Kremniy-2M for a small Pact nation, not Parol.** Parol entered Soviet service in 1977 (unverified: a
   search excerpt of a mil-airfields.de page that now returns 404); the East German change-over, planned for 1989-90, was
   never carried out (German Wikipedia only; its footnoted source, the RWD MB III catalogue, says only that a new
   interrogator was "planned": treat the 1989-90 date as unverified). Kremniy-2 had 12 code filters
   (1-8 peacetime, 9-12 wartime) changed on a secret schedule every 1-6 hours, was unencrypted and could be triggered by US
   interrogators (`soviet_iff`, Crypto Museum page). The Osa-AKM's automatic launch block works only in Parol's guaranteed
   mode (`voenizdat_osa_1991` p. 64), so **our Osa-AK has no launch block** (an inference: the AK's own interrogator and
   modes are not yet confirmed, section 7): the senior operator's reading of the reply arc is the only guard. A battery or a fighter on the wrong code shows no friend.
5. **The Pact measured its own fratricide.** In a Baltic Military District exercise, under the recognition and warning
   arrangements of the time, **up to 25% of the targets "fired upon" by SAMs and guns were our own aircraft**
   (`mt_turchin_coop` p. 11-12). Exercise umpires counted a fighter attacking a target at the moment missiles were launched at
   it as **destroyed by its own SAMs** (`mt_lavrinovich_mdex` p. 12). Turchin wrote that a warning "in no way compensates" for
   missing IFF and that warnings reveal our flights to the enemy.
6. **Holding fire for our fighters is a weakness the enemy uses.** North Vietnam, 14 July 1966: two MiG-21s were shot down
   partly because the guns were not authorised to fire while their own fighters were up (`mt_1973_nvn` p. 13). Syria, June
   1967: aircraft were destroyed while landing because the airfield guns could not fire with their own planes in the air
   (`mt_umnov_1968` pp. 10-11); the same article says that in the UAR (Egypt), not Syria, warning data reached the active
   air defence five to eight minutes late and more through the intermediate levels (p. 8). Mitronin warns that the enemy may find and use our corridors (p. 7).
7. **Who runs it.** Control posts are co-located: a fighter combat control centre at the front air defence command post
   under the air army's deputy commander, two or three fighter guidance and target designation posts (ПНЦ) co-located with
   SAM brigade and regiment command posts, combat control groups at first-echelon division command posts; the SAM commander
   and the fighter guidance officer should sit at **the same plotting board** (`mitronin_1976` pp. 7-11). At the start of a
   war the **national** air defence organises cooperation and covers the field army's troops on home ground; the field air
   defence chief sends a group to its command posts (`mt_rozhdestvenskiy_depth` pp. 8-10). For our small nation that means
   our fighters are controlled by the national air defence's GCI, and the battery is told through the army's chain.
8. **NATO, the enemy going home, had the same problem with different words.** Weapons control status (free, tight, hold) set
   by the region air defence commander, tightened but never loosened by army commanders; minimum risk routes, a coordinating
   altitude between helicopters and jets, high-density airspace control zones with safe-passage procedures, an IFF line and
   return procedures across the front (`fm44_3_1981`, `fm100_42_1976`, `fm1_103_1981`; all in force by 1985). Chaparral and
   Vulcan identified by eye against hostile criteria; Stinger and FAAR had IFF.

What SAM3D should build from this (detailed in the findings' design notes):

- **Our fighters as actors of a national-air-defence GCI**, with flights planned as notices (time, route, height, crossing
  times of the SAM zones, return time, duty zones and heights: `mt_franchuk_front` p. 6). The notice is a fact that travels
  the pipeline (air army or national air defence → division air defence chief → regiment → PU-12M → crews) by code table,
  with the Druzhba-76 delay as the base value. The battery acts only on what reached it.
- **Standing cooperation data in the battery's order**: method (zones or one zone with an altitude split), the fighters'
  height band, corridors and duty zones on the commander's working map (`voenizdat_osa_1991` p. 244), the visual recognition
  signal, the fire-prohibition signals, and the IFF code schedule. A late code table is a missed change.
- **IFF as physics plus paperwork**: a reply arc only if the transponder is on, set to the period's code, in the beam, and
  the battery is on the same code. No automatic launch block for the AK.
- **Friendly fire as the result of missing information**, calibrated as a game value against the 25% exercise figure and
  the period's cases, and **always traceable in the records**: the radio journal shows which notice arrived and when, so the
  after-action report can say why (principle 9 of `docs/command/01_PRINCIPLES.md`).
- **The enemy exploits our caution**: raids timed to our fighters' return, or following them through corridors, against a
  battery that holds fire.

## 2. Findings by faction

### 2.1 Warsaw Pact doctrine (our side follows it; the Pact enemy flies by it)

**ownair-01. Two methods: by zones and in one zone** (1976). The SAM unit's combat zone is the space bounded by the far
limits of its launch zone; the fighters' zone is all the rest. By zones is simple and removes the flight-safety problem
("fighter aviation operates in an independent zone, and flies through the zone of the surface-to-air missile units in
compliance with a prescribed procedure"), but disperses effort and is hard to keep when SAM units move. In one zone, efforts
are shared by altitudes (ranges where each operates without restriction; upper levels preferably to fighters), by axes
(sectors where each works at all heights), by control lines (one after the other), by time (alternating) and by targets
(selective assignment), in combinations, set before or during the raid; it needs reliable control and identification or the
SAMs' fire is restricted by the danger to our aircraft. [`mitronin_1976` pp. 5-6]
*Design*: a `CooperationMethod` per area and period: `by_zones` or `one_zone` with an altitude split, sectors, lines, time
windows and target assignments. The battery's NPC commander reads only the version he was told.

**ownair-02. Which method when** (1976). Exercises showed by zones is easier against the first massed strike, while SAM
battle formations are unchanged, and over slow-moving covered objects (rear bases, second echelons). In a fast offensive,
when fighters have to work behind the first-echelon air defence, by zones is the main method. Against massed strikes that the
air defence cannot fully engage, one zone shared by altitude. Plans provide for both. [`mitronin_1976` pp. 6, 8-9]
*Design*: the first raid of a campaign day runs by zones; later raids may switch to one zone, announced by the regiment.

**ownair-03. Troop air defence of regiments: fighters keep out or fly above** (1976). The regiments' air defence sits inside
the SAM zones. In one zone, "aviation is granted unrestricted right of entry even into the firing zones of the antiaircraft
artillery which is firing on targets not under attack by our fighters". Fighters acting in the fire zones of the motor rifle
and tank regiments' air defence are inadvisable, "since these air defense means frequently do not have identification
systems"; over dense modern air defence fighters should get an altitude band above the upper limit of the SAM troops' kill
zone. [`mitronin_1976` p. 7]
*Design*: over the Osa battery the fighters' default is a height band above the regiment's highest kill limit; a fighter
that descends into it is "in our zone" and the rules of ownair-11/12 apply.

**ownair-04. Corridors** (1976; 1984). "In the majority of cases, corridors are established in order to ensure the safety of
the flights of our aircraft"; the enemy may discover and exploit them, so several corridors, reliably covered by units with
identification means. [`mitronin_1976` p. 7] The 1984 regulations put "the corridor of passage and the duty zone of our
aviation" on the working map of the radar company (battery) commander (BU-X art. 37) and of the AA artillery battery
commander (BU-IX); the 1991 Osa textbook puts it on the Osa battery commander's map. [`pvo_bu_ix_1984` p. 34; `pvo_bu_x_1984` art. 37, pp. 28-29;
`voenizdat_osa_1991` p. 244]
*Design*: corridors and duty zones as map objects in the battery's order; the enemy's intelligence picture can learn them
(COMINT of notices, observed traffic) and route raids along them.

**ownair-05. Co-located control posts** (1976). The front air defence chief and the air army commander set the method in
the air defence plan approved by the front commander. A fighter combat control centre (ЦБУ ИА), usually under the air
army's deputy commander, sits at the front air defence command post; the air army's combat control centre at each army
(ЦБУ ВА) has a cooperation group and two or three **guidance and target designation posts (ПНЦ)**, co-located with SAM
brigade and regiment command posts, able to guide fighters, cooperate with the SAMs and guide aviation against ground
targets. "The commander of the surface-to-air missile unit in control of its firing and the officer guiding fighter aviation
must be at the same display unit (plotting board)". [`mitronin_1976` pp. 4, 7-10] Turchin (late 1960s) argued the same and
added that the co-located army air defence post warns the SAM battalions of the fighters' alert zones, the targets fighters
are working and our aircraft's paths across the front line, and tells the fighter guidance posts which targets each SAM
battalion is firing at. [`mt_turchin_coop` p. 13]
*Design*: our GCI is a post on the map (`simcore::command`), co-located with the SAM regiment's or brigade's command post;
it is the source of the "fighters' targets" facts the battery gets.

**ownair-06. Down to the gunner, by code table, slowly** (1976). Combat control groups go to first-echelon division command
posts; an officer from the group sits at the division's air defence control post. "The essence of cooperation at this level
primarily comes down to ensuring the safety of the flights of our aviation" by telling the division air defence chief the
flight times, routes and altitudes of our aircraft, fighters included; he passes it to the SAM and gun units and the
regiments' air defence chiefs, who give it to their subunits and gunners. In Druzhba-76 it took **10 to 15 minutes** for the
division's air defence chief to pass our aircraft's actions to the air defence means, "because they were transmitted
according to a code table"; Mitronin proposes representatives at regiments on axes with intense friendly air traffic.
[`mitronin_1976` pp. 10-11]
*Design*: the base delay of a flight notice from division to battery is 10-15 min (code-table encoding and decoding as
work at each post); an air liaison officer at the regiment shortens it.

**ownair-07. Fighters lower where the SAMs cannot fire** (1976). When some SAM units cannot fire (moving, out of missiles,
faulty), lower the altitude of the fighters' unrestricted use in their zones to **3,000 m**. [`mitronin_1976` p. 9]
*Design*: when the regiment reports a battery on the march or empty, the fighters' floor over its zone drops to 3,000 m; a
battery that thinks it is up while the plan thinks it is down meets our fighters at 3,000 m.

**ownair-08. Echelons and heights in a field army's air defence** (late 1960s, Soviet). Turchin's three echelons: the first,
forward of the front line beyond the SAM launch zones, fighters only, in independent search; the second, from the front line
to the army air defence zone's rear boundary **about 80-100 km** back, holding SAM and gun positions and the fighters'
airborne alert zones: against low targets at **1,500-2,000 m**, against high targets at **10-12 km** over own territory
beyond the Nike-Hercules kill zone, the zone's centre **about 50-60 km** inside own territory (alert zones may move over enemy
territory before or during a raid); the third, behind, with fighters on airfield alert and SAMs covering second echelons. The
figure's key adds a reserve in the air at **5-6 km**, jammer aircraft at **5-10 km**, a **tactical aviation overflight zone at
50-100 m**, and MiG-21s at 1.5-2 km and 10-11 km. [`mt_turchin_coop` pp. 6-7, 16]
*Design*: default heights and depths for our fighters' alert zones and for our strike aircraft crossing the battery's area
(our fighter-bombers cross low, 50-100 m, in the overflight zone).

**ownair-09. Low-level cooperation by lines** (late 1960s). Most low-flying targets are seen briefly by radar or not at
all, so centralised control is limited. Then cooperate in one zone by consecutive actions with start and stop lines: fighters
in independent search in the first echelon **break off attacks 25-30 km beyond the front line** and report it by radio with
their positions; SAM and gun units fire from the limit of detection to the line of the first-echelon SAM positions
(**10-20 km behind the front line**); fighters on airborne alert work from the line of the SAM battalions' launch sites, at
1,500-2,000 m. [`mt_turchin_coop` pp. 9-10]
*Design*: a "fighters stop" line and a "SAMs start" line in the order; the fighters' radio report of breaking off is a
pipeline fact the battery may or may not hear.

**ownair-10. Fighter attack geometry near the SAM zone** (late 1960s). Military Air Academy calculations: against targets at
10-14 km and 1,300 km/h, fighters from airborne alert attacking from the rear hemisphere can destroy them and disengage before
the far border of the kill zone; faster targets are intercepted inside or behind it, and in that case ("In this case",
p. 9) fighters are brought to a starting point **not closer than 6-8 km** and launch missiles from **4-6 km** (the text
ties these two figures to the faster-target case, not to every attack). All-aspect weapons would let fighters start attacks from their
own territory outside the kill zone but need finer organisation. [`mt_turchin_coop` pp. 8-10]
*Design*: a fighter chasing a target toward the battery reaches the SAM zone edge; the rule decides who gives way. By 1985
our MiG-23s carry all-aspect R-23 (Turchin's caveat now applies) (unverified here: no source for the small nation's
MiG-23 variant and missiles was checked in this note).

**ownair-11. Fratricide measured; warnings are not enough** (late 1960s). Turchin: corridors and timely reporting to
regimental and battalion fire means are not all that is needed; interrogators on all missile guidance and gun fire-control
radars are required; and a device to let a fighter disengage safely "within a 600-meter radius of the target under fire" by
blocking the missile's fuze and launcher or the illumination. In the Baltic Military District's troop air defence
exercises, "under the existing system of recognition and warning", **up to 25 percent** of the targets fired upon by SAMs and
guns were our own aircraft (the translation reads "from a total number of targets 'fired upon' ... up to 25 percent of
our own aircraft were 'fired upon'": the share-of-targets reading is the likelier one, but the sentence is awkward). He
opposes corridors (they delay fighter-bombers, cannot fit fighters guided by programme or
returning aircraft, form a pattern, let the enemy concentrate there) and warns that warning the troops of our flights reveals
them to the enemy, while "a single warning in no way compensates for the absence of recognition devices". [`mt_turchin_coop`
pp. 10-12]
*Design*: the exercise figure is the upper anchor for friendly fire when notices and IFF work only as the 1960s allowed;
1985's Osa with IFF should do better, but a missed code change returns it to that state.

**ownair-12. Fighters stay under the air army** (late 1960s). Turchin rejects putting fighter units under the front air
defence chief: they would be split among armies and lose their manoeuvre; control stays centralised with the air army
commander (his deputy for air defence). [`mt_turchin_coop` p. 12]
*Design*: our GCI belongs to the air force or national air defence, not to the regiment; the regiment can ask, not order.

**ownair-13. Double engagements; fighters given preference in a moving battle** (Soviet, 1960s-70s; CIA translation 1976).
Fighters sometimes attack on their own radar's fixes before the air defence system has told them of the target; then a target
is attacked by a fighter inside the SAM zone or fired on at the moment the fighter attacks, while the senior chief, not
knowing, gives it to other means. The exercise rule: "all fighters attacking targets at the moment that missiles were being
launched against these targets should be considered destroyed by the fire of their own surface-to-air missile troops". In
an offensive, as SAM zones keep changing, cooperation by zones is hard, and "the fighter aircraft should be given preference
in attacks on targets": fighters cannot see SAM zones from the air, while the SAM and gun units have means, "although they
are not sufficiently reliable", to identify their own fighters. [`mt_lavrinovich_mdex` pp. 11-12, 16 (the OCR loses
the page-16 header; the preference passage sits between the page-15 and page-17 headers)]
*Design*: a fighter's own lock does not wait for the notice; the double engagement is a real event the after-action report
can find (the board's record against the fighter regiment's photo-control).

**ownair-14. What a report on our aviation must contain** (Soviet, about 1966-68; CIA translation 1975). Reports to the SAM
and gun regiments' command posts must hold the time, directions and altitudes of our aircraft; the time they cross the SAM
and gun zones and the expected time of return to their airfields; the areas and heights of airborne duty; continuously, as
decisions and the air situation change, and for planned as well as actual flights, even outside our radars' coverage. The
authors propose a group at the front air defence command post for reporting our aviation's actions (one or two officers from
the air army command post), groups with plotting boards at army air defence posts, and new code tables for this secret
information; "up to now it has not been decided by whom, how, and by which means" it is to be passed. [`mt_franchuk_front`
p. 6]
*Design*: the fields of the `OwnFlightNotice` fact: flight id, type, count, route, height band, zone-crossing times, return
time, duty zone and height, recognition signal; planned and amended versions.

**ownair-15. The national air defence leads at the start** (Soviet, *Military Thought* 1966 No. 2 (78), as Franchuk's
footnote dates it; CIA translation 1976). While front forces deploy inside
the national air defence's area, its commanders organise cooperation and hold full responsibility for covering the front's
troops on home territory, because its control system is stable and automated; on increased readiness the front air defence
chief or his deputy with a group goes to their command posts; the cooperation plan is made in peacetime and practised; by
the third or fourth day the front's air defence becomes a separate system; operations groups from the national air defence
and air army staffs go to the front air defence command post; cooperation with allied countries' air defence is run by the
Combined Command on the same principles. [`mt_rozhdestvenskiy_depth` pp. 8-10]
*Design*: on our own ground our GCI is the national air defence's; the regiment's air defence chief sits in its net.

**ownair-16. National air defence: one zone by targets since 1959; automation** (history written 2023 about 1956-60s). Joint
exercises of 1956-59 showed that cooperation by zones, heights and directions limited the fighters; the August 1959 research
exercise made action **by targets in one zone** the main method: fighters keep attacking targets inside the SAM zone and the
SAM and gun units destroy targets the fighters have not attacked. It became possible through SAM identification equipment,
automated fighter command posts and longer-range air-to-air missiles. With Vozdukh-1 the time to pass information to a
fighter regiment's command post fell to **about 10 s from 3-5 min**. [`vizh_tikshaev_pvo`] Vozdukh-1M passed guidance commands
to interceptors by the Lazur datalink (unverified: only a Russian Wikipedia search excerpt, which may not be cited; the
VIZh article does not mention Lazur). [`ru_wiki_mig25`, snippet]
*Design*: national air defence fighters near fixed SAM sites work "by targets in one zone"; the field army's batteries are
not on Vozdukh and get voice and telecode.

**ownair-17. The 1984 regulation on cooperation with aviation** (in force from the order of 5 August 1983). Note: part IX
is the regulation for the troop air defence's **anti-aircraft artillery** battery, platoon and crew (guns and ZSU), not
for a SAM battery; ownair-17 and -18 apply to the Osa battery by analogy (as `docs/SOURCES.md` registers `pvo_bu_ix_1984`),
and this check did not re-read the articles. Cooperation with
aviation is "the rational distribution of the fire of the air defence unit and of our attacking fighter over targets or over
one target, by time, heights, lines and sectors, and ensuring the safety of the passage of our aircraft through the unit's
fire (kill) zone". It rests on every man knowing the control and mutual recognition signals, on **timely delivery of the
applications for our aircraft's flights to every fire unit**, and on the commanders' constant watch over our aircraft
flying near targets under fire. [`pvo_bu_ix_1984` art. 15, pp. 13-14]

**ownair-18. The AA artillery battery commander's duties toward our aircraft** (1984; analogy for the Osa battery). He tells all personnel the time, place, height
and direction of our aircraft's flights, our fighters' airborne duty zones, their types and the current recognition and
fire-prohibition signals, and updates them as the senior commander sends them; he identifies detected aircraft with the
identification system (art. 34). From the senior commander's information he establishes which targets our fighters are
attacking and on which fire is prohibited, and drops them or lowers their priority (art. 45). In all combat he ensures the
safety of our aircraft and helicopters in the fire zone: the crews recognise them by their features and **the way they behave
when crossing fire zones**, know the signals and the flight applications, follow our aircraft on the scopes and by eye, and
report at once when one of ours is close to a target about to be engaged (art. 53). Before the battle he gives the order for
cooperation with fighter aviation (art. 68). [`pvo_bu_ix_1984` art. 34 p. 32, art. 45 pp. 37-38, art. 53 p. 40, art. 68 p. 52]
*Design*: these are NPC duties at the PU-12M and in the vehicle: the commander's "fire prohibited on track N" order, the
crew's visual recognition by behaviour, the immediate report.

**ownair-19. Radar units and our aircraft** (1984). The commander gives the radar identification code and the visual
recognition signal (art. 41); crews identify each target at once after detection, then again at set intervals and whenever
targets split, join or cross, and visually with optics or the naked eye (art. 53); data on our aircraft sending a distress
signal go out of turn (art. 55); on order, radar units **control our aviation's flight regime**, comparing detected tracks'
routes, heights and times with those in the flight applications and reporting violators at once (art. 56); the commander
checks the timely change of identification codes, talk-table keys and the PVO grid (art. 17); every man must tell our
aircraft from the enemy's by sight (art. 19); positions of air targets and our aircraft go by automated systems or coded
squares of the PVO grid (art. 30). [`pvo_bu_x_1984` pp. 15-16, 18, 24, 31, 42-44]
*Design*: the PU-12M board keeps notified own flights; a track squawking friend but off its notified route is a "violator"
reported up, not shot; the radio journal logs the code change.

**ownair-20. The Osa battery's textbook** (Soviet, printed 1991, describing the Osa-AKM). The battery commander's working map
carries "the corridor of passage and the duty zone of our aviation" (p. 244); in battle he informs his subordinates of the
air situation and **ensures the safe passage of our aircraft and helicopters** (p. 245). The crew chief assesses the air
situation from the warning data and then his scopes, including "the position and actions of our aviation" (p. 251). The
senior operator decides a target's allegiance by the presence or absence of the «Я свой самолёт» mark on the PPI (p. 254).
The battery commander's fire decision takes into account "the actions of our aviation and other air defence means" (p. 258).
[`voenizdat_osa_1991` pp. 244-245, 251, 254, 258]

**ownair-21. The Osa's interrogator and launch block** (Osa-AKM, in Soviet service from 1980; printed 1991). The 1L24-2
works with the Parol, Kremniy-2 and Kremniy-2M systems. Mode I: general, not imitation-resistant, "a limited number of codes
set by schedule", bands III and VII; mode II: guaranteed "friend", imitation-resistant (Parol), band VII; a control
interrogation (ЗАПРОС К) confirms a mode I friend. Azimuth resolution no worse than 5-00 (30°), range 0.9-1 km; continuous
work 72 h with interrogation on average 30 min an hour; the key-entry device with its key plates is kept apart, in the secret
section; an erase panel destroys the keys. **Launch blocking works only in mode VII-II**: when the tracked target is on
automatic tracking and a guaranteed friend reply falls in the 1 km gate, the launch circuit is broken for ±1.9 s each time the
search antenna passes the target's bearing. [`voenizdat_osa_1991` pp. 17, 53-56, 64]
*Design*: our 1985 Osa-AK (Kremniy-2M, mode I only: an inference until the AK's interrogator is confirmed) has no launch block; a friend shows as a 20° arc that the operator must
read (`docs/research/OSA_MANUALS_2026-10.md`).

**ownair-22. Kremniy-2's codes and weakness** (late 1950s on; tier 4). Interrogation in band III (668 MHz); 12 plug-in code
filters, 1-8 for peacetime, 9-12 kept for war, filter 4 "only used for system and alignment"; filters changed "on a secret, pre-arranged schedule every one to
six hours"; a distress signal by answering twice (a wider curve); Warsaw Pact allies had a limited set of the codes; being
unencrypted it could be intercepted and spoofed, and the US QRC-248 and APX-80/81 ("Combat Tree") could trigger its
transponders. [`soviet_iff` (Crypto Museum page)]
*Design*: the code in force is a timed fact in the battery's signal documents; a NATO fighter with an interrogator can
identify our fighters beyond visual range by their Kremniy replies (an input for the air-to-air note).

**ownair-23. Parol's dates** (1977; GDR to 1990). Parol entered Soviet service in 1977 (development finished 1970, per a
search excerpt; the 1976 MiG-25 defection, which exposed Kremniy equipment, added urgency, per the same excerpt)
(unverified: the mil-airfields.de pages behind `soviet_iff` return 404 and the Crypto Museum page does not mention Parol).
The NVA's change-over, planned for 1989-90, was not carried out; the P-15's already fitted IFF parts and the Mi-24's Parol
equipment and documents went back to the USSR in 1990 as sensitive items (partly unverified: the 1989-90 date and the
return rest on German Wikipedia, whose footnoted source, the RWD MB III catalogue's P-15 page, says only that a new
interrogator, "1L22 (NRZ-4P)", was "geplant"; the flak11 page's text confirms a 1990 proposal to return the NVA's Osa,
Strela-10, Tochka, S-200 and S-300 to the USSR as sensitive technology, the IFF items being in a photographed page not
re-read). [`soviet_iff`; `dewiki_p15`; `nva_sensitive_1990`] The direction of the finding (no Parol in a small Pact
nation's forces in 1985) is not in doubt.
*Design*: Kremniy-2M for our nation and for the Pact's allied air forces in 1985; Parol for Soviet units of a Warsaw Pact
enemy (their own ground air defence identifies their own aircraft in mode II and cannot be spoofed by Kremniy tricks).

**ownair-24. Czechoslovak view, 1988** (just after the period; doctrine article). Repulsing a massed air strike may start
either with the first echelon's air defence and part of the fighters or with fighters on the approaches; a key factor is the
right choice of cooperation methods between air defence and fighters and securing the SAM troops' manoeuvre by fire;
command of air defence and fighters from **joint aviation and air defence command posts** at operational and tactical level.
A low-level raid takes **3 to 5 minutes** from detection by our radars to the first-echelon troops. [`vr_roucka_1988` pp. 17-18]

**ownair-25. Czechoslovak structure** (described in 1991; hindsight about the 1980s). The national air defence (PVOS) had
two divisions with SAM brigades and regiments, fighter regiments and radio-technical brigades; the ground forces' air
defence was organic to districts, armies, divisions and regiments, with permanent air defence command posts at district,
army and division and mobile joint aviation-and-air-defence command posts of front and army ("Centrum" type); fighters
defended areas the SAMs did not cover. The **3,000 m** figure is Hrbek's 1991 planning assumption for a proposed unified
air defence system ("when considering the unified system, the division's defence zone can be regarded as a continuous
zone of AA fire against targets up to 3,000 m"), not a stated 1980s doctrine. [`vr_hrbek_1991` pp. 32-34]
*Design*: the small nation's layout: national air defence (fixed SAMs and fighters) and army air defence (mobile SAMs,
Osa among them) as separate chains joined at the front's (here, the army's) joint command post.

**ownair-26. Andersen et al. on control timing** (Soviet book, 1979). In repelling a raid, time is measured in minutes or
seconds; delayed data on a target "is equivalent to letting the target through"; the time budget for assigning missions and
coordinating with fighters is critical at low altitude and under jamming and depends on automation. [`andersen_1979` p. 12]

### 2.2 Cases from the period's wars (Pact-equipped air defences)

**ownair-27. North Vietnam, 1966.** On 14 July 1966 two MiG-21s scrambled to cover an airfield were shot down; the outcome is
explained partly because they were not covered by other fighters and "antiaircraft means were not authorized to deliver fire
on the enemy aircraft because their own fighters were in the air". VPA fighters were controlled from plotting boards with a
**3-6 minute delay**; their actions were not covered by the air defence for lack of cooperation in one zone. [`mt_1973_nvn`
pp. 13-14 (Military Thought No. 2 (81), 1967)]

**ownair-28. Syria, June 1967.** Israeli aircraft destroyed Syrian aircraft landing after their missions "at a moment when
antiaircraft artillery could not fire because its own planes were in the air". [`mt_umnov_1968` pp. 10-11 (Military Thought
No. 2 (84), 1968)] The same article's "five to eight minutes and more" delay of warning data through the intermediate
levels is said of the **UAR (Egypt)** on the first morning, not of Syria (p. 8).

**ownair-29. Egypt, about 1970.** With poor coordination with the Egyptians, Soviet SAM crews shot down three Egyptian combat
aircraft and damaged an airliner that landed safely; the cause given is old IFF. [`vedomosti_2016_egypt`; tier 4]

**ownair-30. Egypt, 1973 (unverified).** A popular source estimates 45-60 Egyptian aircraft brought down by Egyptian SAMs and
guns during the October war. Only a search excerpt was seen; its origin is not clear. Not used as data. [`egypt_1973_snippet`]

**ownair-31. Angola, 1982.** A TAAG Yak-40 airliner was shot down in error by a missile of an S-125 «Pechora» "(or
«Volga»)", that is S-125 or S-75, the veteran is not sure, when approaching Lubango in 1982 (a veteran's note of
24 January 2008). [`kuito_memoirs` p. 295]

**ownair-32. Peacetime authority in the GDR** (unverified: a Wikipedia search excerpt only, which may not be the cited
source; find its underlying source before use). In the GDR's standing air defence duty system, the authority to open fire
on airspace violators lay with Soviet representatives. [`dewiki_dhs`, snippet only]

### 2.3 NATO (the enemy going home over its own air defence)

**nato-01. Weapons control status and rules of engagement** (FM 44-3, 30 September 1981). Rules of engagement tell the
squad leader what, when and where he may shoot; they come from the region air defence commander. Hostile criteria for
Chaparral and Vulcan are visual: attacking friendly forces or assets; markings or configuration of an enemy aircraft.
**Weapons free**: fire at any aircraft not positively identified as friendly; **weapons tight**: only at aircraft positively
identified as hostile by the hostile criteria; **weapons hold**: do not fire except in self-defence, which is never denied.
The region air defence commander changes the status as the battle goes; army commanders may impose more restrictive controls,
but less restrictive changes need his approval; at division the airspace management element (DAME) disseminates changes, for
example "WEAPONS TIGHT, 1ST BRIGADE AREA FOR HELICOPTERS ONLY—0800 TO 0900"; brigade and battalion commanders may tighten
to avoid giving away a position. [`fm44_3_1981` ch. 5, pp. 5-9 to 5-10; not re-read in the check] The three definitions
are confirmed in FM 44-1-2 of 15 June 1984, p. 9-1, which takes them from NATO's ATP 42 (so NATO-wide, not US only);
there weapons hold is "do not fire except in self-defense or in response to a formal order". [`fm44_1_2_1984` p. 9-1]
*Design*: NATO's ground air defence has a status per area, type and time window; a Pact fighter following a NATO strike home
enters "weapons free" belts; NATO's own returning aircraft rely on the status and IFF.

**nato-02. Airspace control measures** (AFM 2-14 / FM 100-42, 1 November 1976). Electronic identification is the most rapid
and reliable; visual or procedural identification is used when electronic is not possible. A **coordinating altitude**
separates helicopters (below) from jets (above); penetration is notified but needs no approval; conflict avoidance is
see-and-be-seen in visual conditions. **High-density airspace control zones** carry safe-passage procedures. **Minimum risk
routes** let high-speed aircraft cross the tactical area at low altitude, coordinated with air defence procedures, rules of
engagement and the air defence firing units' locations. [`fm100_42_1976` paras 4-2, 4-3, 4-5 (b)-(d), pp. 4-2 to 4-7]

**nato-03. Army aviation and the IFF line** (FM 1-103, 1981). The division's airspace element disseminates air defence
warnings and the weapons control status, and the air traffic and air defence procedures for aviation units crossing the
front line "to include return procedures", and coordinates SIF/IFF procedures for Army aircraft "to include the location of
the SIF/IFF line". The G2 Air tells it of preplanned low-level reconnaissance flights (time, route, altitude, number,
electronic countermeasures). Division aircraft in visual conditions need not file flight plans; the flight coordination
centre coordinates with the HAWK air defence post. Air defence fires are controlled by the area air defence commander's rules
to prevent engagement of friendly aircraft and incidents before hostilities. [`fm1_103_1981` ch. 3; app. E, p. E-10]

**nato-04. Positive and procedural management; IFF modes** (FM 44-1-2, 1984). Air battle management is positive (real-time
radar, IFF, computers, data links) or procedural (airspace segmented by volume and time, weapons control status); strict
procedural management only when a mix is not achievable. Sample orders give IFF Mode 3/A and Mode 4 by code book. The Stinger
gunner interrogates and keeps tracking if the reply is not a positive friend; with the Chaparral FLIR the gunner gets
identification and engagement authority, the squad leader may override (the 1984 text says the FLIR's introduction "will
change the engagement sequence": in 1985 it was only entering service, so a Chaparral squad may still be on the older,
squad-leader identification sequence). The FAAR radar with an AN/TPX Mark XII interrogator
gives tentative identification over a data link to the squads' TADDS. [`fm44_1_2_1984` pp. 8-11, 9-1, Stinger and Chaparral
sections; `fm44_3_1981` FAAR section]

**nato-05. The Soviet reading of NATO** (1979). Hawk and Nike-Hercules, field army or corps assets, reinforce division air
defence "in coordination with fighter interceptors"; the US division has Chaparral/Vulcan and Redeye, the West German Gepard
replacing 40 mm guns, the British Rapier and 40 mm. [`andersen_1979` p. 5; the Soviet view of NATO]

### 2.4 The neutral neighbour

**neutral-01.** No period source was found on a small neutral's fighter and ground air defence cooperation. The registered
analogue (Austria) had a ban on guided missiles until the early 1990s, and its Drakens of 1988 carried guns only
(`austrian_af_wiki`: English Wikipedia, a pointer only under the sourcing rules; the missile ban, from the 1955 State
Treaty, should be cited to the treaty text or an Austrian source before use). So in 1985 such an air arm integrates fighters and guns only: no SAM zones, identification by sight and
procedure. Design inference, low confidence.

### 2.5 The rebels

**rebels-01.** No source found. A rebel faction with a few light aircraft and guns or MANPADS has no IFF; its own aircraft
are protected only by time and place arrangements and by sight. Design inference, low confidence.

## 3. Numbers usable as data

| Quantity | Value | Faction | Date | Source | Confidence |
|---|---|---|---|---|---|
| Delay passing our aircraft's actions, division air defence chief to the guns, by code table | 10-15 min | Pact | 1976 (Druzhba-76) | `mitronin_1976` p. 11 | high |
| Warning data delayed through intermediate levels to the active air defence | 5-8 min and more | Pact-equipped (UAR/Egypt) | 1967 | `mt_umnov_1968` p. 8 | medium |
| Low target seen by a P-15 before the forward edge; manual passing of reports to the AD chief's post; extra delay to the active means' plotting boards | 4-5 min; 2-3 min; about 1 min more | USSR (front AD) | about 1966-67 | `mt_franchuk_front` p. 5 | high (added in the check) |
| Low-level enemy raid, detection by our radars to first-echelon troops | 3-5 min | Pact (ČSSR) | 1988 | `vr_roucka_1988` p. 18 | high |
| Fighter control from plotting boards, delay | 3-6 min | VPA (Pact-trained) | 1966-67 | `mt_1973_nvn` p. 14 | high |
| Information to a fighter regiment's command post, before and with Vozdukh-1 | 3-5 min → about 10 s | USSR national AD | 1960s | `vizh_tikshaev_pvo` | medium |
| Own aircraft among targets "fired upon" in a troop air defence exercise | up to 25% | USSR (Baltic MD) | late 1960s | `mt_turchin_coop` pp. 11-12 | high (as reported) |
| Fighters' unrestricted floor over SAM units that cannot fire | 3,000 m | Pact | 1976 | `mitronin_1976` p. 9 | high |
| Fighters' height over dense regimental air defence | above the SAM troops' upper kill limit | Pact | 1976 | `mitronin_1976` p. 7 | high |
| Fighter airborne alert zone, against low targets | 1,500-2,000 m | USSR | late 1960s | `mt_turchin_coop` p. 6 | high |
| Fighter airborne alert zone, against high targets | 10-12 km, centre 50-60 km inside own territory | USSR | late 1960s | `mt_turchin_coop` p. 6 | high |
| Fighter reserve in the air; jammer aircraft | 5-6 km; 5-10 km | USSR | late 1960s | `mt_turchin_coop` p. 16 | medium (figure key) |
| Tactical aviation overflight zone over own air defence | 50-100 m | USSR | late 1960s | `mt_turchin_coop` p. 16 | medium (figure key) |
| Army air defence zone, rear boundary from the front line | about 80-100 km | USSR | late 1960s | `mt_turchin_coop` p. 6 | high |
| Fighters in independent search break off attacks, beyond the front line | 25-30 km | USSR | late 1960s | `mt_turchin_coop` p. 9 | high |
| First-echelon SAM positions behind the front line | 10-20 km | USSR | late 1960s | `mt_turchin_coop` p. 10 | high |
| Fighter attack on a target faster than 1,300 km/h: start point; missile launch | ≥ 6-8 km; 4-6 km | USSR | late 1960s | `mt_turchin_coop` p. 9 | high |
| Proposed safe-disengagement radius around a target under SAM fire | 600 m | USSR (requirement, not fielded) | late 1960s | `mt_turchin_coop` p. 11 | high |
| Guidance and target designation posts per air army combat control centre | 2-3 | Pact | 1976 | `mitronin_1976` p. 10 | high |
| Division's air defence as a continuous fire zone | up to 3,000 m | ČSSR | 1991 planning assumption (post-period) | `vr_hrbek_1991` p. 32 | low |
| National air defence leads cooperation for the field army's troops on home ground | to about day 3-4 of an offensive | USSR | 1966 | `mt_rozhdestvenskiy_depth` p. 9 | medium |
| Kremniy-2 code filters | 12 (1-8 peace, 9-12 war, 4 test) | Pact | late 1950s on | `soviet_iff` | medium |
| Kremniy-2 code change interval | every 1-6 h, secret schedule | Pact | late 1950s on | `soviet_iff` | medium |
| Kremniy-2 interrogation frequency | 668 MHz (band III) | Pact | late 1950s on | `soviet_iff` | medium |
| Parol in Soviet service | 1977 | USSR | 1977 | `soviet_iff` (search excerpt; unverified) | low |
| Parol in the NVA | planned 1989-90, not done | GDR | 1989-90 | `dewiki_p15` (Wikipedia; its source says only "planned") | low |
| 1L24-2 interrogator resolution | azimuth ≤ 5-00 (30°), range 0.9-1 km | USSR (Osa-AKM) | 1980s | `voenizdat_osa_1991` p. 53 | high |
| 1L24-2 duty | 72 h continuous, interrogating 30 min an hour on average | USSR (Osa-AKM) | 1980s | `voenizdat_osa_1991` p. 53 | high |
| 1L24-2 pulse power | ≥ 920 W (Kremniy-2M), ≥ 870 W (Parol) | USSR (Osa-AKM) | 1980s | `voenizdat_osa_1991` p. 53 | high |
| Launch block hold per antenna pass (Parol mode II only) | ± 1.9 s, 1 km gate | USSR (Osa-AKM) | 1980s | `voenizdat_osa_1991` p. 64 | high |
| Corridor widths and heights for our aircraft | **not found** | Pact | - | - | - |
| IFF reply probability or failure rate | **not found** | any | - | - | - |
| Notice lead time required by regulation (before the flight) | **not found** | Pact | - | - | - |
| NATO weapons control status | free / tight / hold; tightened locally, loosened only by the region ADC | NATO (US) | 1981 | `fm44_3_1981` ch. 5 | high |
| NATO temporary status window example | one brigade area, helicopters only, 0800-0900 | NATO (US) | 1981 | `fm44_3_1981` ch. 5 | high (example) |

## 4. Disagreements between sources, and which to use

1. **By zones or one zone.** Levchenko (Military Thought No. 3 (82), 1967, as quoted by Turchin) held that cooperation by
   zones would be the basic method and one zone "practically impossible, given the present means of control" until control
   was automated and every guidance station had IFF. Turchin answered that in the second echelon one zone is unavoidable,
   because fighter alert zones and most SAM positions are there, and zones give the enemy a free run inside the SAM belt.
   Mitronin (1976) settles it for the Pact: plan both, zones for the first massed strike and static objects, one zone by
   altitude when the air defence is overwhelmed. Lavrinovich gives fighters preference in a moving battle. **Use Mitronin as
   the rule and Turchin for the geometry; Lavrinovich for the offensive case.**
2. **Corridors.** Mitronin (1976) and the 1984 regulations (corridors on every battery map) keep corridors; Turchin (late
   1960s) calls them a pattern the enemy will use and a delay for fighter-bombers. **Use corridors (the regulations of 1984);
   model Turchin's objection as the enemy's ability to learn and use them.**
3. **Warnings or IFF.** Turchin: a warning cannot replace recognition devices. Mitronin: regimental air defence often lacks
   IFF, so keep fighters out of it. The 1984 regulations require both notices and IFF. **Use both; the Osa has IFF, the
   regiment's ZSU-23-4 and Strela teams identify by sight and notice only** (this note found no source giving the 1985 ZSU or
   Strela an interrogator; see section 7).
4. **Who commands the fighters.** Proposals of the late 1960s to put fighters under the front's air defence chief were
   rejected by Turchin; Mitronin's 1976 arrangement keeps them under the air army's combat control centre at the air defence
   command post. **Use the air force (or national air defence) GCI, co-located but not subordinate.**
5. **Kremniy and Parol compatibility.** A search excerpt says the two systems cannot interrogate or answer each other; the
   Osa-AKM's ground interrogator works with both. Both can hold: a transponder answers only its own system, a multi-system
   interrogator asks both. **Use: our Kremniy-2M fighters answer only Kremniy interrogators; a Soviet Parol fighter answers our
   Osa-AK only if it still carries a Kremniy transponder (not known).**
6. **Reliability of identification.** Lavrinovich calls the SAM and gun units' identification means "not sufficiently
   reliable"; the 1991 textbook offers guaranteed identification only with Parol. **Use: Kremniy-2M as unreliable and
   spoofable in 1985.**
7. **Egypt 1973.** Only a popular estimate (45-60 aircraft) was seen. Not used until a period source is found.
8. **Vozdukh-1M's dates.** The VIZh article's web text mentions Vozdukh-1M and Luch-1 in the context of a 1958 exercise,
   which is inconsistent; no date is used.

## 5. Post-period items excluded (later wars' options)

| Item | Date | Why excluded |
|---|---|---|
| Parol IFF in the NVA and other allied armies (and so Parol replies for our nation's aircraft) | planned 1989-90, not carried out in the GDR | After 1985 for a small Pact nation; Soviet units had it from 1977 |
| Osa-AKM's automatic launch block on a guaranteed friend | needs Parol mode II; AKM in Soviet service from 1980 | Not available to our Osa-AK with Kremniy-2M in 1985 |
| Czechoslovakia's unified air defence system and the "Masiv" automated command system | proposed 1990-91 | After the period |
| A division's air defence treated as one continuous fire zone up to 3,000 m (Hrbek) | 1991 planning assumption for the unified system | After the period; not a 1985 rule (moved here by the check of 15 Nov 2026) |
| FM 100-103, *Army Airspace Command and Control in a Combat Zone* | 1987 | After 1985; FM 100-42 (1976) and FM 1-103 (1981) used instead |
| "Joint engagement zone" as a named NATO concept | late 1980s-1990s monographs (not read) | Not shown in a 1985 document |
| JTIDS / Link 16 picture shared by fighters and SAMs | fighters after 1985 | Excluded by the period rule |
| Joint Pub 3-01.4 SEAD categories, Gulf War airspace practice | 1991-95 | Hindsight only |

## 6. Sources

The quotations from Mitronin, Turchin, Lavrinovich, Umnov and Andersen were checked against the downloaded texts on
15 November 2026. The archive.org links of the CIA translations follow the reading room's pattern
(`cia-readingroom-document-<document number>`); Mitronin's, Umnov's and `mt_1973_nvn`'s identifiers were taken from
the items fetched, Turchin's was confirmed by its item metadata, and the others are built from the document numbers.

| Id | Tier | Full citation | URL | Read | Whose view |
|---|---|---|---|---|---|
| `mitronin_1976` | 1 | Maj. Gen. of Artillery V. Mitronin (deputy chief, Air Defence and Air Forces Directorate, Staff of the Combined Armed Forces), "The Organization of Cooperation Between Air Defense Troops of the Ground Forces and Fighter Aviation", *Information Collection of the Headquarters and the Technical Committee of the Combined Armed Forces* (SECRET Warsaw Pact journal) No. 12, 1976; CIA translation, Intelligence Information Special Report TS #798039, 16 Feb 1979; released 2012 | https://archive.org/details/cia-readingroom-document-5166d4f999326091c6a608a7 | full (11 pp.) | Warsaw Pact's own |
| `mt_turchin_coop` | 1 | Lt Gen of Aviation I. Turchin, "Cooperation of Fighter Aviation and Surface-to-Air Missile Troops", *Military Thought* (secret collection), answering P. Levchenko in No. 3 (82), 1967, so 1967 or later; CIA translation, 11 Nov 1976, CIA-RDP10-00105R000302550001-1 | https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000302550001-1 | full (OCR text) | Soviet own |
| `mt_lavrinovich_mdex` | 1 | Maj. Gen. P. Lavrinovich, "Military District Air Defense Exercises", *Military Thought*; CIA translation, 30 Mar 1976, CIA-RDP10-00105R000201720001-4 | https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000201720001-4 | relevant pages (12, 17) | Soviet own |
| `mt_franchuk_front` | 1 | Maj. Gen. A. Franchuk, Lt Col. G. Pomelnikov, "Air Defense in a Front Offensive Operation", *Military Thought* (cites No. 2 (78), 1966); CIA translation, 19 Mar 1975, CIA-RDP10-00105R000100990001-7 | https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000100990001-7 | full (7 pp., two denied) | Soviet own |
| `mt_rozhdestvenskiy_depth` | 1 | Maj. Gen. of Artillery V. Rozhdestvenskiy, Col. V. Bilanonov, "Problems of Air Defense in a Front Offensive Operation Conducted to the Entire Depth of a Theater of Military Operations", *Military Thought* 1966 No. 2 (78) (dated by Franchuk's footnote in `mt_franchuk_front`); CIA translation, 17 Mar 1976, CIA-RDP10-00105R000201690001-8 | https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000201690001-8 | relevant pages (8-10) | Soviet own |
| `mt_umnov_1968` | 1 | Maj. Gen. of Artillery M. Umnov, "Air and Air Defense Forces in the Near East War", *Military Thought* (secret collection) No. 2 (84), 1968; CIA translation | https://archive.org/details/cia-readingroom-document-5166d4f899326091c6a60590 | relevant pages (10-11) | Soviet own |
| `mt_1973_nvn` (registered) | 1 | *Military Thought* (secret collection) No. 2 (81), 1967, on the DRV's air defence against US aircraft 1965-67; CIA translation of 18 Sept 1973 (read here in the copy catalogued as by Col. A. Grazno) | https://archive.org/details/cia-readingroom-document-5166d4f899326091c6a60591 | relevant pages (13-14, 18) | Soviet own |
| `andersen_1979` | 1 | Yu. A. Andersen, A. I. Drozhzhin, P. M. Lozik, *Противовоздушная оборона сухопутных войск*, Voenizdat 1979 (304 pp.); excerpts translated as JPRS L/9337, 8 Oct 1980, CIA-RDP82-00850R000300040017-6 | https://archive.org/details/cia-readingroom-document-cia-rdp82-00850r000300040017-6 | searched in full text; read pp. 5, 12 | Soviet own (and its view of NATO) |
| `pvo_bu_ix_1984` (registered) | 1 | *Боевой устав Войск ПВО, ч. IX: зенитная артиллерийская батарея, взвод, расчёт войсковой ПВО*, order no. 093 of 5 Aug 1983, Voenizdat 1984 (ДСП) | https://sprotyvg7.com.ua/ (file as registered) | full text searched; arts. 15, 34, 45, 53, 68 read | Soviet own; AA **artillery** battery, an analogy for the SAM battery |
| `pvo_bu_x_1984` (registered) | 1 | *Боевой устав Войск ПВО, ч. X: радиолокационная рота (батарея), взвод, расчёт войсковой ПВО*, order no. 093 of 5 Aug 1983, Voenizdat 1984 (ДСП) | http://rtv-pvo-gsvg.narod.ru/doc/combat_statut.pdf | full text searched; arts. 17, 19, 30, 35, 37, 41, 53, 55, 56 read | Soviet own |
| `voenizdat_osa_1991` (registered) | 1 (status as in `docs/SOURCES.md`) | *Автономный войсковой зенитный ракетный комплекс «Оса»*, Voenizdat 1991 (Osa-AKM) | local, `reference/sources/` | relevant pages (17, 53-56, 64, 244-245, 251, 254, 258) | Soviet own |
| `vizh_tikshaev_pvo` | 1 (later study) | V. N. Tikshaev, V. M. Malinovsky, «Развитие тактики и оперативного искусства Войск ПВО страны в 50-60-е годы XX века», *Военно-исторический журнал* (web version 26 Jan 2023) | http://history.milportal.ru/razvitie-taktiki-i-operativnogo-iskusstva-vojsk-pvo-strany-v-50-60-e-gody-xx-veka/ | fetched page, read through a summary | Russian MoD history |
| `vr_roucka_1988` | 1 | Gen. Maj. doc. Ing. Václav Roučka, «Vojenské umění: Ničení nepřítele, který se připravuje k útoku», *Vojenské rozhledy* (Vojenská mysl section) 1988 No. 3, pp. 17-22 | https://vojenskerozhledy.cz/pdfs/vor/1988/03/04.pdf | full | Czechoslovak army's own |
| `vr_hrbek_1991` | 1 (post-period) | Pplk. Ing. Ivan Hrbek, «K vytvoření jednotného systému PVO», *Vojenské rozhledy* 1991 No. 2, pp. 31-34 | https://vojenskerozhledy.cz/pdfs/vor/1991/02/07.pdf | full | Czechoslovak army's own |
| `soviet_iff` (registered) | 4 | Crypto Museum, «Kremnij-2» (and the mil-airfields.de Kremniy and Parol pages as registered; those now return 404) | https://cryptomuseum.com/radar/kremnij2/index.htm | Crypto Museum page through a summary; mil-airfields by search excerpt | Western enthusiasts' reference |
| `dewiki_p15` (registered) | 4 (Wikipedia: pointer only, not citable) | German Wikipedia, «P-15 (Flat Face)»; its footnote is the RWD MB III Technikkatalog, «Rundblickstation 15», https://www.rwd-mb3.de/pages/p15.htm (veterans' catalogue), which lists only "1L22 (NRZ-4P) (geplant)" | https://de.wikipedia.org/wiki/P-15_(Flat_Face) | re-read in the check (both pages) | encyclopaedia |
| `nva_sensitive_1990` (registered) | 1 | NVA list of sensitive documents and equipment for destruction or return, 1990 (photographed page on flak11) | http://www.flak11.de/9K33-5.htm | as registered (not re-read) | GDR own |
| `kuito_memoirs` (registered) | 2 | G. V. Shubin (ed.), *Куито-Куанавале. Неизвестная война. Мемуары ветеранов войны в Анголе*, 2008; S. Kolomnin's note of 24 Jan 2008, p. 295 | https://www.militera.org/books/pdf/memo/sb_angola3.pdf | full text searched | Soviet veterans |
| `vedomosti_2016_egypt` | 4 | P. Aptekar, «Египет и Сирия – первая кровь», *Ведомости*, 30 Sept 2016 | https://www.vedomosti.ru/opinion/articles/2016/09/30/659152-egipet-i-siriya-pervaya-krov | fetched page through a summary | Russian press |
| `ru_wiki_mig25` | 4 (Wikipedia: pointer only, not citable) | Russian Wikipedia, «МиГ-25» (Vozdukh-1M and the Lazur datalink) | https://ru.wikipedia.org/wiki/МиГ-25 | search excerpt only | encyclopaedia |
| `dewiki_dhs` | 4 (Wikipedia: pointer only, not citable) | German Wikipedia, «Diensthabendes System der Luftverteidigung» | https://de.wikipedia.org/wiki/Diensthabendes_System_der_Luftverteidigung | search excerpt only | encyclopaedia |
| `egypt_1973_snippet` | 4/5 (origin unclear) | Popular web summaries of the Egyptian Air Force in 1973 (migflug.com, warhistory.org) giving 45-60 aircraft lost to own air defence | migflug.com, warhistory.org (exact pages not recorded) | search excerpt only; not used | popular |
| `fm44_3_1981` | 1 | US Army FM 44-3, *Air Defense Artillery Employment, Chaparral/Vulcan*, 30 Sept 1981 | archive.org copy (item identifier not recorded in this pass) | relevant chapters | US Army own |
| `fm100_42_1976` | 1 | AFM 2-14 / FM 100-42, *US Air Force/US Army Airspace Management in an Area of Operations*, 1 Nov 1976 | archive.org copy (item identifier not recorded in this pass) | relevant chapter (4) | US own |
| `fm1_103_1981` | 1 | US Army FM 1-103, *Airspace Management and Army Air Traffic in a Combat Zone*, 1981 | archive.org copy (item identifier not recorded in this pass) | relevant chapters | US Army own |
| `fm44_1_2_1984` | 1 | US Army FM 44-1-2, *Air Defense Artillery Reference Handbook*, 1984 | https://archive.org/details/fm-44-1-2-84-ada-reference | relevant pages | US Army own |
| `austrian_af_wiki` (registered) | 4 (Wikipedia: pointer only, not citable) | Wikipedia, Austrian Air Force | as registered | as registered | encyclopaedia |

## 7. Open items and documents to acquire

- **Corridor widths and heights, and the notice lead time required by regulation** (how long before a flight the
  application must reach the troops): not found in BU-IX, BU-X or the translations. Look in the Soviet *Наставление по
  производству полётов* (НПП) of the period, the front air army's combat regulations, or NVA/ČSLA instructions on «заявки на
  пролёт».
- **IFF reply rates and failure rates** (transponder on, right code, reply probability, garbling): no number found. A Soviet
  IFF technical description (Kremniy-2M SRZO-2M) or a Western test report would give them.
- **What the Osa-AK (not AKM) carried**: the interrogator's type and modes, and whether the regiment's ZSU-23-4 and Strela-1/-10
  had interrogators in 1985. The Polish manual `mon_2112_80` (local) should be checked for the AK's interrogator and codes.
- **Military Thought, "Fighter Aviation in the Front Air Defense System" (CIA, 29 May 1975, CIA-RDP10-00105R000201080001-5)**:
  the archive.org PDF is damaged; read it from another copy. Also Levchenko's article, *Military Thought* No. 3 (82), 1967.
- **Vozdukh-1M and the allied automated systems** (ČSSR, Hungary, Poland, GDR): names and dates not pinned.
- **Hungarian and Polish documents and veterans' accounts** on fighter and SAM cooperation (Wojska OPK, MN Légvédelem): not
  reached (search budget).
- **Fratricide cases to verify with period sources**: Egypt and Syria 1973 (General Shazly's *The Crossing of the Suez*,
  1980; DIA/CIA analyses of 1974-75); the Falklands 1982 (an Argentine Mirage reported shot down by Argentine guns over Port
  Stanley on 1 May; a British Army Gazelle reported shot down by HMS Cardiff on 6 June): these two were **not verified in this
  pass** and are not used; Iran-Iraq (HAWK against own aircraft): not searched.
- **NATO Central Region**: the HAWK and Nike belts' relation to fighter areas, 2nd and 4th ATAF procedures, the 1983 edition
  of FM 44-1, and "safe lane" or return-to-force procedures as written by 1985. DTIC and the CGSC library refused downloads.
- **Combat Tree (APX-80/81)**: its date and use against Kremniy, from a US source, for the air-to-air note.
- Re-read the registered `grant_2002` (Syrian MiGs cut off from ground control in 1982) and `mt_1984_babich` (local wars to
  1982) for how Pact-trained fighters depended on GCI and how that failed.
- Register the new ids in `docs/SOURCES.md` (a section "Our fighters and ground air defence") when the data is written.

## Verification (15 Nov 2026)

An adversarial check, done the same day. The CIA translations' OCR texts were downloaded from archive.org and read in full
(Mitronin, Turchin, Lavrinovich, Franchuk, Rozhdestvenskiy, Umnov, the 1967 DRV article, Andersen's JPRS excerpts). Also
read in full: Roučka 1988 and Hrbek 1991 (Vojenské rozhledy PDFs), BU part X (the veterans' PDF), FM 44-1-2 of 15 June
1984 (archive.org text), the Osa 1991 textbook (local extract), the Angola memoir (militera PDF). Read through a summary:
the VIZh article, the Vedomosti article, the Crypto Museum page, German Wikipedia's P-15 page and the RWD MB III page. The
web-search budget ran out before Parol's 1977 date could be searched; BU part IX, FM 44-3, FM 100-42 and FM 1-103 were not
re-read (no copy to hand or no item identifier recorded).

| Finding | Verdict | Note |
|---|---|---|
| ownair-01 | confirmed | Mitronin pp. 5-6: zones bounded by the far limit of the launch zone; one zone by altitudes ("upper flight echelons are preferably assigned to the fighter aircraft"), axes, lines, time, targets. |
| ownair-02 | confirmed | p. 6 (first massed strike, limitedly mobile installations), pp. 8-9 (zones the principal method in a fast offensive; one zone by altitude against massed strikes). |
| ownair-03 | confirmed | p. 7, quotations exact. |
| ownair-04 | corrected | Mitronin p. 7 confirmed; BU-X art. 37 puts the corridor on the radar company (battery) commander's map, BU-IX on the AA artillery battery's, the Osa textbook (p. 244-245 of the PDF) on the Osa battery's: not "every battery commander" by one rule. |
| ownair-05 | confirmed | Mitronin pp. 8-10 (TsBU IA, TsBU VA, 2-3 PNTs, "same display unit (plotting board)"); Turchin p. 13. |
| ownair-06 | confirmed | p. 11: "10 to 15 minutes ... because they were transmitted according to a code table" (Druzhba-76). |
| ownair-07 | confirmed | p. 9: 3,000 m over units relocating, without missiles or faulty. |
| ownair-08 | confirmed | Turchin pp. 6-7 and the key to figure 1 (p. 16): all heights and depths as stated. |
| ownair-09 | confirmed | pp. 9-10: 25-30 km beyond the front, 10-20 km SAM line, 1,500-2,000 m. |
| ownair-10 | corrected | p. 9: the 6-8 km start and 4-6 km launch are given for targets faster than 1,300 km/h; the design note's R-23 for our MiG-23s marked unverified. |
| ownair-11 | confirmed | pp. 10-12: 600 m, interrogators on all radars, "up to 25 percent", "a single warning in no way compensates"; the 25% sentence's wording noted as awkward. Late 1960s (answers MT No. 3 (82), 1967). |
| ownair-13 | corrected | Lavrinovich: the double-engagement passage is pp. 11-12 and the preference passage p. 16, not 17; claims exact. |
| ownair-14 | confirmed | Franchuk p. 6, exact; dated about 1966-67 (answers an article of 1966). |
| ownair-15 | corrected | Date: Rozhdestvenskiy's article is *Military Thought* 1966 No. 2 (78) (Franchuk's footnote), not the 1970s; content pp. 9-10 confirmed. |
| ownair-16 | corrected | VIZh confirmed (1959 exercise, by targets in one zone, 3-5 min to 10 s), but it is a 2023 history; the Lazur datalink rests on a Wikipedia excerpt only: unverified. |
| ownair-17, -18 | corrected | Not re-read; BU part IX is the AA **artillery** battery's regulation, so these duties apply to the Osa battery by analogy; the summary now says so. |
| ownair-19 | confirmed | BU-X arts. 37, 55, 56 read in the veterans' PDF (corridor on the map, distress data out of turn, flight applications compared and violators reported); arts. 17 and 19 located by their keywords. |
| ownair-21 | confirmed | Osa 1991 textbook pp. 53-55, 63-64: 1L24-2 with Parol, Kremniy-2, Kremniy-2M; 920/870 W; 72 h; launch block only in mode VII-II, 1 km gate, ±1.9 s. The AK conclusion (no launch block) marked as an inference. |
| ownair-22 | confirmed | Crypto Museum (tier 4): 12 filters, 1-8 peace, 9-12 war, filter 4 for alignment, every 1-6 h, 668 MHz, distress by double reply, limited set for the Pact, QRC-248 and APX-80/81 (Combat Tree, in US service from the Vietnam war, so in period). |
| ownair-23 | unsupported | Parol 1977 rests on a search excerpt of a dead page; the NVA 1989-90 date and the return rest on Wikipedia, whose source says only "planned"; flak11's text confirms the 1990 return proposal for the NVA's systems, not the IFF items. Direction of the finding stands. |
| ownair-24 | confirmed | Roučka, VR 1988/3 pp. 17-18: both openings of a repulse, joint aviation and AD command posts, "3 až 5 minut". Post-period by three years; used as hindsight. |
| ownair-25 | corrected | Hrbek 1991 pp. 32-34: the 3,000 m continuous fire zone is his planning assumption for a proposed unified system, moved to the post-period list; the 1980s structure (PVOS, Centrum posts) confirmed as described in 1991. |
| ownair-26 | confirmed | Andersen et al. 1979 (JPRS L/9337): "delayed data on a probable target is equivalent to letting the target through". |
| ownair-27 | confirmed | MT No. 2 (81), 1967, pp. 13-14: 14 July 1966, guns not authorised while own fighters up; plotting boards "with a three to six minute delay". |
| ownair-28 | corrected | Syria's landing losses confirmed (pp. 10-11); the 5-8 min warning delay is said of the UAR (Egypt), p. 8, not Syria. |
| ownair-29 | confirmed | Vedomosti 2016 (tier 4): three Egyptian combat aircraft shot down, an airliner damaged and landed, "old IFF systems"; the year (1970) is from context. |
| ownair-31 | corrected | Memoir: "С-125 «Печора» (или «Волга»)": S-125 or S-75, the author is unsure. |
| ownair-32 | unsupported | Wikipedia excerpt only; not citable. |
| nato-01 | confirmed | Definitions confirmed in FM 44-1-2 (15 June 1984) p. 9-1, taken from NATO ATP 42; FM 44-3's DAME and temporary-status example not re-read. |
| nato-04 | corrected | FM 44-1-2 confirmed (positive and procedural management p. 9-1, IFF Mode 3/A and Mode 4 by code book in the sample orders, Stinger interrogation, Chaparral FLIR authority); the FLIR "will change" the sequence, so in 1985 it was only entering service. |
| neutral-01 | unsupported | An inference resting on English Wikipedia (`austrian_af_wiki`), which may only point to sources. |

Added by the check: Franchuk p. 5 gives the P-15 seeing a low target 4-5 minutes before the forward edge, 2-3 minutes
spent passing the reports by hand to the AD chief's post and about a minute more to the active means' plotting boards
(about 1966-67): a period figure for the warning delay (section 3).

Period scan: nothing after 1985 is presented as 1985 practice. The terms used (zones, one zone, corridors, flight
applications, weapons free/tight/hold from ATP 42, minimum risk routes and HIDACZ in FM 100-42 of 1976, Mode 4, Combat
Tree) are all of the period; Link 16, joint engagement zones and post-1991 SEAD practice are already in section 5. The
Czechoslovak sources of 1988 and 1991 are labelled as hindsight, and Hrbek's 3,000 m assumption is now excluded.

Tier problems: four Wikipedia ids (`dewiki_p15`, `ru_wiki_mig25`, `dewiki_dhs`, `austrian_af_wiki`) carried claims and are
now marked as pointers only; `pvo_bu_ix_1984` is the AA artillery battery's regulation, not the SAM battery's; the CIA's
translations are rightly tier 1 (the Pact's own words), while `andersen_1979` p. 5 on NATO is the Soviet view of NATO and is
labelled so.
