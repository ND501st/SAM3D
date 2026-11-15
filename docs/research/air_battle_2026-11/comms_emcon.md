# Communications, emission control and intercept in the air battle, 1980-85: research (November 2026)

Why: the project owner's request of 15 November 2026 for the enemy's air war in its hot phase. This note covers one part
of it: how the period's air forces talked in the air, when they kept silent, who jammed their nets, who listened to them,
how fast what was heard became a warning, and what that means for SAM3D's EM layer and information pipeline. The war is
the 1985 war (the nation's theatre is dated 15 October 1985). Anything that entered service or doctrine after 1985 is in
section 5 only.

Method and limits. The note was written on 15 November 2026. Thirteen web searches were run before this session's shared
search budget ran out; after that only documents whose addresses were already known were fetched (about 30 fetches in
all). Several gaps in section 7 are gaps for that reason, not because nothing exists. Three of the best documents found
are Soviet *Военная мысль* (Military Thought) articles translated by the CIA and the NSA's own history of the PURPLE
DRAGON study; their texts were read as OCR from archive.org copies. Pages are the translation's or the history's own.

Tiers are those of `docs/SOURCES.md`. Every source says whose view it gives: an adversary's assessment of the other side
is recorded as what that side believed. Ids already registered in `docs/SOURCES.md` are reused; new ids are listed in
section 6.

Related notes in this run (not repeated here): `aam_weapons.md` (Teaball's once-a-minute plot and Combat Tree from
`checo_1978_linebacker`; the Lazur on Pact fighters), `air_combat.md` (wp-02, the Lazur and Vozdukh-1), `c2_retasking.md`
(the FAC check-in, retasking latencies, the 1985 brevity words as an open item), `factions_cases.md` (nato-04, comm-out
packages at Red Flag, from `rafhs_j20_1999`; the per-faction radio discipline values).

## 1. Summary for SAM3D's design

1. **Intercept gives warning in two kinds, and they come by different roads.** The period's best-documented case is the
   NSA's own study of how North Vietnam knew of US raids. Administrative and airspace traffic gave hours: in December 1966
   two North Vietnamese army broadcast stations alerted 34 % of B-52 ARC LIGHT strikes with an average of 8.5 hours'
   warning, the likely source being the flight plans, altitude reservations and NOTAMs of civil air traffic control;
   after the PURPLE DRAGON study's fixes, by April 1967, 5 % with under 30 minutes (`nsa_purple_dragon_1993`
   pp. 22-26). Tactical calls in the clear gave minutes: the US "Heavy Artillery Warnings" broadcast before B-52 strikes gave
   the target's coordinates and time on target "usually ten to thirty minutes before the bombs started falling" (p. 69).
   SAM3D should model both: a slow product (traffic analysis, a daily summary) and a fast one (a call heard and passed
   straight to an air defence command post).
2. **For a strike already in the air, radar usually warns first.** The same study recommended no change to Rolling
   Thunder's tanker communications, because the changes would complicate refuelling "without significantly lessening the
   enemy's warning time"; the reason given is the "radar surveillance problem" (p. 27; the detail is redacted). So COMINT
   adds who, how many, where to and when, not first detection (inference). In SAM3D intercept should rarely beat the
   warning net's radars to a raid; its value is identity, intent, target and timing.
3. **Static habits are what intercept lives on.** Unchanging call signs and frequencies let enemy radio units follow
   the same allied units "for years at a time"; B-52s used static call signs; new US signal operating instructions were
   broken out "in as little as six hours" and brevity codes "within six hours" (pp. 68-69, from prisoners); an EC-121
   early-warning aircraft in Korea used one call sign for years (p. 78). Call-sign rotation and radio discipline should be
   staff and officer traits, and our analysts should lose a unit's continuity for some hours after each change.
4. **Deep NATO strikes flew silent; packages near the front talked.** Black Buck (1982) flew silent and reported success
   with one codeword, "superfuse", heard at Northwood by 44 minutes later (`wiki_black_buck` as a pointer, after Burden et
   al. 1986 p. 365 and White 2012 p. 337; the books not read);
   El Dorado Canyon (April 1986, just after the period) flew and refuelled in radio silence (`mobility_forum_2020`);
   Red Flag packages of 60-70 aircraft went through in 20-30 minutes "with no radio calls required" (sibling note
   `factions_cases.md`, nato-04). Against that, a 1982 package over the Bekaa was run by voice from an airborne and a
   ground command post, with fighters fed in four at a time (`grant_2002`), and the Soviet reading of that battle made the
   E-2C "one of the main elements" of the formation, controlling tactical aviation through the penetration
   (`mt_1984_babich`). SAM3D's NATO flights should be silent on deep ingress and talkative where an AWACS, a CRC or a FAC
   runs them, which is where our posts can hear them.
5. **The Pact's own aviation was silent on the move and dependent on ground control in the fight.** A 1964 *Военная
   мысль* article has an air army rebasing "in small groups at low altitudes under conditions of complete radio silence"
   (`mt_1964_air_army` p. 6); NATO believed "radio silence is standard operating procedure" (`lawrence_1982`). But Pact
   fighters fought on GCI, and GCI had a channel limit: in 1962 an air army with three division and nine regiment command
   posts with automated control could guide 30-40 groups or single aircraft at once, and fighters beyond that flew "a sortie
   for nothing" (`mt_1962_front_aviation`). Cut that link and the fighters were lost: over the Bekaa selective jamming cut
   Syrian MiGs off from ground control (`grant_2002`). In SAM3D a Pact enemy's strike groups are nearly silent; its GCI and
   warning nets keep talking (radio silence exempted them, `vst_1988` as registered) and are what our side hears.
6. **Who jammed whom in 1985.** NATO's dedicated jammer of fighter control, the EC-130H Compass Call, reached initial
   operating capability in 1983 but was based in Europe only from 1986-87 (registered: `dvids_2018_ec130h`,
   `wiki_usafe_ew_units`; see `VOICE_JAMMING_2026-10.md` §1.2). NATO believed the Pact would jam the whole tactical air
   control system: HF for immediate air requests, VHF-FM to the forward air controllers and "UHF radio links for strike
   control" (`fm_100_2_1_1984` p. 15-3; `lawrence_1982`). The NATO answer, HAVE QUICK (UHF frequency hopping keyed by
   time of day, word of day and net number), was introduced in 1980 (`fh_radios`); when it reached USAFE's aircraft was
   not found. SAM3D should carry it as a per-unit flag, not a NATO-wide given.
7. **Language is a real cost, and the period solved it in two ways.** Soviet intercept operators in East Germany listened
   to US VHF voice in English learnt in civilian life and at the junior specialists' school, and a telegraphist with weak
   English mis-transcribed words (`osipov_2013`, 1981-83); a hostile relay
   net in Vietnam passed US aircraft traffic on as English transcriptions, untranslated, within about five minutes
   (`nsa_purple_dragon_1993` pp. 60-61); Teaball's EC-121s carried Vietnamese-speaking specialists to listen to MiG
   pilots and their controllers (`boyne_2008_teaball`). The Pact's own coalition air defence needed procedure charts "in
   the languages of their countries" and frequencies and call signs agreed beforehand (`mt_coord_wp_air`). Each of our
   intercept posts should have linguists per language with a skill that sets delay and error.
8. **What this means for the EM layer and the pipeline (design, inference).** Every enemy radio call (a flight's
   check-in, a push, a SAM-launch call, a bingo, a codeword, an HQ's re-tasking, a GCI vector) is a transmission from a
   position, on a band, for some seconds. Our side's listening posts (the army intelligence air section's radio
   intercept company and the radio-technical troops' posts: `docs/command/02_CHAIN_OF_COMMAND.md` already names the air
   section) hear it by link budget and line of sight, need a linguist for its language, and produce either a fast
   tactical warning or a slow intelligence item. The fast path copies the East German arrangement found here: air defence
   liaison groups sitting at the intercept centres passed the air situation straight to the air defence division's
   command post (`bischoff_fufutakl`, state of 1989-90; when the groups began is **not found**, so for the 1985 war this
   is a design choice, not a documented 1985 arrangement). Both paths go through the information pipeline; the player hears a
   warning on a net or from a person, never from a panel.

## 2. Findings by faction

Each finding: id, what the evidence says, its date, [source, page or section], then what it means for SAM3D.

### 2.1 General: the period's wars as evidence of what intercept could do

**comms-01 Hours of warning from administrative and airspace traffic.** For ARC LIGHT the study found likely sources of
warning in the civil air traffic procedure: at each hand-over between air traffic control centres a flight plan and an
altitude reservation were filed, and the new centre published a NOTAM giving "altitude, flight path, and entry and exit
times and locations", broadcast to all adjacent centres (p. 22; the recommendations themselves are redacted, p. 24). In
December 1966 two North Vietnamese army broadcast stations alerted 34 % of ARC LIGHT (B-52) missions, average warning 8.5
hours, sometimes with "detailed targeting information". After PURPLE DRAGON's recommendations, in April 1967, they
alerted 5 %, with an average under 30 minutes. (Separately, the drone programme's traffic got a KW-26 secure teletype link
within a week, and SAC began upgrading its worldwide operations codes by 1 June 1967.) [1966-67;
`nsa_purple_dragon_1993` pp. 11, 22-26] Confidence high for the figures. *SAM3D:* a staff's operations security is a
trait; a careless staff's planned raids can be known hours ahead from administrative traffic, a careful one's only
minutes ahead or not at all.

**comms-02 Minutes of warning from tactical calls in the clear.** "Intercept of Heavy Artillery Warnings" gave the enemy
"tactical forewarning of almost all ARC LIGHT missions, giving target coordinates and TOT, usually ten to thirty minutes
before the bombs started falling". The enemy's "major source of COMINT was always the exploitation of Allied nonsecure
voice transmissions". One battalion twice escaped in 1967 after intercepting US reconnaissance aircraft's calls that its
position would be struck. [1966-69; `nsa_purple_dragon_1993` p. 69] High. *SAM3D:* safety and deconfliction broadcasts,
FAC talk-ons and recce reports are the calls that name a target and a time; heard, they make a 10-30 minute warning.

**comms-03 Intercept to relay in about five minutes, in the original language.** A hostile net (its owner redacted)
carried reports of US Navy and Marine aircraft activity off the carriers in the Gulf of Tonkin, some of them "direct
transcriptions of U.S. aircraft communications traffic"; everything on it except operator chatter was in English; at its
peak one station sent over 250 messages a day; the content of intercepted US signals was retransmitted "on average within
five minutes". The carriers reported their positions to Pearl Harbor every day at 0700 local. [1960s, the circuit went
dead in late 1970; `nsa_purple_dragon_1993` pp. 60-61; heavily redacted] Medium. *SAM3D:* a dedicated intercept post with
a direct line can pass a heard call on in about 5 minutes; passing it untranslated saves time and costs understanding
further up; fixed reporting times are a habit our analysts can learn.

**comms-04 Static call signs and how fast new ones fell.** Enemy radio units followed allied units "for years at a time"
because of "static callsigns and frequencies"; B-52s on ARC LIGHT "regularly used the callsign CAPTAIN CONTROL and
DINBACK" (prisoners' statements); new US SOIs were broken out "in as little as six hours" (ARVN two hours); brevity and
slang codes "could be broken out and read within six hours" and let the enemy tell units and echelons apart. In PACOM's
peacetime surveys an EC-121 early-warning aircraft in Korea had used an unchanging call sign and a home-made status code
for years. [1966-71, the Korea survey 1970s; `nsa_purple_dragon_1993` pp. 68-69, 78] Medium (prisoners' claims, quoted by
the NSA). *SAM3D:* each enemy unit has a call-sign rotation interval; our analysts' "continuity" on a unit is a value that
grows while its call signs stay the same and drops for some hours after a change.

**comms-05 For raids already flying, radar outran COMINT.** PURPLE DRAGON considered changing tanker refuelling
communications for Rolling Thunder and decided against: the changes "would only needlessly complicate refueling
operations without significantly lessening the enemy's warning time". It made no major Rolling Thunder recommendations
"because of the radar surveillance problem". [1967; `nsa_purple_dragon_1993` p. 27] Medium (the reasoning is redacted;
the reading that North Vietnam's radars saw the strikes anyway is an inference). *SAM3D:* intercept's warning of a raid in
flight seldom beats the warning net's radars; what it adds is identity, target and time.

**comms-06 A SIGINT-fed warning service for fighters (the NATO pattern, 1972).** Teaball, at Nakhon Phanom from 26 July
1972, fused RC-135 and EC-121 intercepts with radar; EC-121K Rivet Tops carried Vietnamese-speaking specialists who
"monitored voice communications between MiG pilots and their ground controllers"; warnings went to the F-4 patrols through
a KC-135A relay, "Luzon", about 300 miles away on line-of-sight UHF, which had "unexpected outages"; the calls gave only
heading, speed and vector information, "in the same format" as the Navy's Red Crown (the EC-121T flew as "Disco"), and
pilots were not told which sources were exploited; pilots
registered their call signs with Teaball before take-off. From 29 July to the end of the war, US aircraft shot down 30
MiGs and lost 10. [1972; `boyne_2008_teaball`] Medium (tier 4, written 2008; the sibling note `aam_weapons.md` gives
the CHECO figures, a plot once a minute and 19 MiGs for 5 losses, early August to mid-October; `checo_1978_linebacker`
is not yet registered in `docs/SOURCES.md`). This is US practice of 1972 in Southeast Asia; that a Teaball-type warning
service existed for NATO aircrews in Europe by 1985 is **not found** (section 7). *SAM3D:* a NATO enemy can
turn our GCI voice into MiG warnings for its flights, with a relay that can fail and a delay of about a minute or more;
silencing or jamming our GCI voice denies it.

**comms-07 SAM-launch warnings by radio, as the Soviets saw them.** A 1967 *Военная мысль* article: an American pilot
"knew that he would be informed at any moment of the launch of the first SAM by radio from observation aircraft located
outside the SAM troop kill zone", or by a light on his panel, and then dived to 300-400 m with a turn of 90° or more.
Babich (1984) repeats it: launch information "was sent by radio from observers -- from special reconnaissance aircraft,
which participated in every US air raid". [1965-67, written 1967 and 1984; `mt_1973_nvn` pp. 17-18; `mt_1984_babich`,
Vietnam section] High. *SAM3D:* a NATO package's SAM-launch call is a real transmission on the strike frequency, seconds
after our launch; our intercept hearing it confirms a launch was seen; the flight's evasion starts on it.

**comms-08 KAL 007: pilots heard far, ground controllers not (disputed).** The US had a transcript of the Soviet
interceptor pilot's exchange within about an hour of Secretary Shultz's 8:20 a.m. call on 1 September 1983, and he cited
it in public at 10:45 a.m. EST (the shoot-down was at about 18:26 GMT the day before); Shultz wrote that ground control
"ordered him to shoot the aircraft down" [`frus_1981_88_v4_d84`]. A later analysis says the recordings had the pilots'
frequency and not the ground controllers' [`acig_kal007`, snippet only]. [1983] Low-medium. *SAM3D (inference from
propagation and this case):* an aircraft at altitude is heard at long range; a ground controller's transmitter behind the
horizon is not. Our posts will hear enemy aircrew far more than their ground stations, unless an enemy controller is
airborne (AWACS, E-2C, an airborne command post).

### 2.2 NATO

**comms-09 The UHF band and HAVE QUICK.** NATO's air voice ran on AM in 225-400 MHz. HAVE QUICK, "introduced in 1980",
is an anti-jam measure, "not an encryption system", hopping by a pseudo-random sequence keyed by time of day (TOD), a word
of day (WOD) and a net number, so that several nets share one WOD; it was added to synthesiser radios and can be combined
with KY-57/KY-58 secure voice [`fh_radios` (Crypto Museum)]. The WOD is six segments of six digits [`fh_radios`
(Wikipedia), which cites a US Navy training plan for the AN/ARC-182 of March 2000: a post-period document that may
describe HAVE QUICK II's format; (unverified) for HAVE QUICK I in 1985]. The Crypto Museum's note that the time of day
"usually" comes from GPS is modern practice: GPS was not operational in 1985, and how TOD was distributed then is not
found. A 1987 article describes Have Quick transmitters that "jump to a new frequency every tenth of a second"
[`correll_1987`]. HAVE QUICK II development is reported from May 1983 and production in 1986 [`forecast_intl_hq`,
snippet only, tier 5, recorded only]. When HAVE QUICK I reached USAFE's aircraft was **not found**. [1980-87] Medium for
the scheme, low for 1985 fielding. *SAM3D:* in the 1985 war a NATO unit's UHF nets are plain AM by default, with HAVE
QUICK as a unit flag set by a game value until a fielding date is found. A hopping net resists spot jamming; our intercept
still detects its traffic and can direction-find it, but follows the content only with difficulty (inference).

**comms-10 NATO's C2 by 1983-85.** Four NATO E-3As were at Geilenkirchen in April 1983, eighteen planned by 1985; the
EIFEL 1 system at USAFE's ATOC at Sembach automated air tasking and linked computer-to-computer with the German ATOCs at
Kalkar and Messstetten; Maastricht's ATOC was to get it; the Tactical Leadership Programme at Jever ran eight four-week
courses a year [1983; `afmag_1983_usafe`; also `afmag_1984_nato` as registered]. The US defined command, control and
communications countermeasures (C3CM: OPSEC, deception, jamming and destruction) in DoD Directive 4600.4 of 27 August
1979, and CJCS MOP 185 of 20 December 1983 set denying the enemy commander effective C2 while keeping one's own as the
goal [`hutcherson_1994`, ch. 1-2]. Medium. *SAM3D:* a NATO air staff's re-tasking moves between ATOCs by data in minutes
(inference), but reaches a flight in the air only by voice from a CRC, an AWACS or a FAC: each such call is a transmission
our side can hear.

**comms-11 Comm-out deep strikes and the post-strike codeword.** Black Buck One (1 May 1982): the Vulcan's crew sent the
codeword "superfuse" for a successful attack at 0746Z as they passed the task force; Northwood had it by 0830Z; a Victor
tanker captain kept radio silence "lest he compromise the mission" until he heard the codeword, then called Ascension for
help [`wiki_black_buck` as a pointer only, which cites Burden et al. 1986 p. 365 (0746Z), White 2012 p. 337 (0830Z) and
Burden et al. 1986 p. 395 (the Victor captain); the books not read]. El Dorado Canyon (14-15 April
1986): the F-111s "flew and refueled in radio silence"; a pilot listed "air-refueled under radio silence" among things he
had not done before [`mobility_forum_2020`; April 1986, after the period: evidence that the practice existed by then,
not a 1985 event]. The sibling note `factions_cases.md` (nato-04) records Red Flag packages
with no radio calls required. [1977-86] Medium. *SAM3D:* NATO deep strikes (F-111, Tornado, Buccaneer, Vulcan-type
bombers) fly comm-out after a pre-briefed plan; an officer breaks silence only for an emergency, an abort or the codeword;
their COMINT trace is a few single words on HF or UHF, worth a great deal to an analyst who knows the habit.

**comms-12 Talkative packages under airborne control (the Bekaa pattern).** Over the Bekaa (9-11 June 1982) E-2Cs
downlinked their picture to the command post in Tel Aviv; Ivry spoke "by voice ... with anyone from my command post";
pilots relied on VHF for tactical talk and links to the post; RPV video let him pass target information to fighters
within "seconds"; four-ships were sent into the fight one at a time, each fight lasting one to two minutes; about 26 MiGs
were shot down within half an hour [`grant_2002`]. The Soviet reading (Babich, 1984): the E-2C "regulated" the cover
groups' movement and coordinated the strike groups, becoming "one of the main elements of the dispersed combat
formation" [`mt_1984_babich`, the Lebanon section]. [1982, written 1984 and 2002] Medium-high. *SAM3D:* an enemy package
with an airborne controller produces a steady stream of controller-to-flight voice near the battle; our posts can hear
the controller (airborne, so in line of sight far away) and learn the package's shape and timing from it.

**comms-13 Emission imitation as deception.** Operation Bolo (2 January 1967): 8th TFW F-4s entered North Vietnam "using
the same route, altitude, and formation" as an F-105 strike and carried and operated the F-105s' jamming pods; seven
MiG-21s were shot down in 12 minutes [`nmusaf_bolo`, read from the search excerpt; the fact sheet refused the fetch
again on 15 Nov 2026 (HTTP 403): (unverified) against the full text].
Whether the F-4s also used F-105 call signs is claimed only by a blog and is not used. [1967] Medium. *SAM3D:* an enemy
officer with a deception trait can make a fighter sweep look like a strike in our intercept and radar picture; our
analysts' identification of a raid's type from its emissions can be wrong on purpose.

**comms-14 Jammers that stay silent until they are seen (1967, the Soviet reading).** In the June 1967 war, radio jamming
"was directed against the radio control networks and against the radiotechnical means of air and air defense forces",
first from ships near the UAR's waters and then from the air; aircraft jammers were "activated only upon entering the
range of enemy radar stations, reaching ... maximum emission when nearing the strike targets"; jamming of missile guidance
and gun-laying was "not very successful". [1967, written 1968; `mt_1968_umnov` p. 7] Medium. *SAM3D:* a NATO or Israeli-
style package's jammers have their own emission control: off on the approach, on at detection; jamming of our GCI and air
defence control nets starts with the raid.

**comms-15 Listen or jam: a commander's choice.** Ivry (Bekaa, 1982): "You can jam it when you need it to assist your
fighter planes", but "you cannot jam it when you want to get information, when you want to listen"; the IAF ran "mock
jamming" of fighter and ground communications [`grant_2002`]. [1982] Medium-high. *SAM3D:* an enemy EW officer chooses per
net whether to listen (our calls feed his picture) or to jam (our calls fail); the choice can change in the raid.

### 2.3 Warsaw Pact

**comms-16 GCI had a channel limit.** "If there are in the air army three division command posts and nine regimental
command posts equipped with automated control systems, the capability for simultaneous guidance constitutes up to 30 to 40
groups or individual aircraft against the same number of targets. Therefore, putting a larger complement of fighter forces
into the air will lead to part of them not being provided with guidance and carrying out a sortie for nothing." Fighters
were therefore committed in echelons, first from airborne alert zones 70 km and more over enemy territory, then from
readiness 1 and 2. [doctrine of 1962, Military Thought No. 2 (63); `mt_1962_front_aviation`] Medium (early in the period
of interest; the principle, not the number, is used). *SAM3D:* every Pact (and our side's) GCI post has a number of
guidance channels; fighters without one search on their own, worse.

**comms-17 Coalition air coordination: agreed beforehand, in several languages.** A *Военная мысль* article by a
General-Major of Aviation (name garbled in the OCR, "M. ?rslamov"), on the GSFG's, GDR's, Polish and Czechoslovak combined
exercises of 1965-67: coordination would reach "down to aircraft crews"; directives should fix "the lines of transfer of
control of aircraft in the air"; each coordinating post needs "one or two ultra-short-wave (decimetric wave) radio sets"
for the battlefield control of allied fighters and fighter-bombers, with "prior arrangements on operating procedures
with aircraft aloft (including the use of an automated system)" and knowledge of "the operating frequencies and
callsigns"; means should deploy in no more than 30 minutes (10-15 for some) and have a reliability of at least 0.98;
secure devices must be part of every channel; plotting boards fed by telecode; "draw up procedure charts for discussions
by specialists in the languages of their countries"; and "conceal the operation of all radioelectronic means from enemy
reconnaissance". [late 1960s; `mt_coord_wp_air` pp. 4-9] Medium. *SAM3D:* Pact control is by agreed frequencies, call
signs and hand-over lines, prepared in advance; an impromptu change (a new target, a flight handed to another nation's
GCI) costs time and can fail; language is part of the cost.

**comms-18 Radio silence on the move.** An air army's dispersal to concealed airfields: "The flight movement is carried
out in small groups at low altitudes under conditions of complete radio silence, and also with the carrying out of
measures for operational camouflage." [doctrine of 1964, Military Thought No. 2 (72); `mt_1964_air_army` p. 6]. NATO's
1982 view: for Soviet forces "radio silence is standard operating procedure", with landlines, couriers, flags and flares
emphasised [`lawrence_1982`]. The Soviet military dictionary of 1988 defines radio silence as a ban on transmitting from
which air defence and the warning system are exempt [`vst_1988`, as registered; not re-read]. [1964-88] Medium. *SAM3D:*
a Pact enemy's strike groups and transfers give our intercept almost nothing on the way; its warning and air defence nets,
GCI and command nets do keep talking, so those are what our posts hear and direction-find.

**comms-19 What the Pact jams in a NATO air war (NATO's view).** Soviet REC doctrine "establishes a requirement to jam at
critical times enemy command and control and weapon system communications"; "jamming targets will include the tactical
air control system, which uses HF radios for immediate air requests, VHF-FM radios to link forward air controllers, and
UHF radio links for strike control"; named jammers are the R-330A and R-834; a front could include "at least a radio and
radar intercept regiment, a radio intercept regiment, an air defense jamming regiment, and a radio jamming regiment"
[1984; `fm_100_2_1_1984` pp. 15-2 to 15-3; `lawrence_1982` has the same tactical air control system targets (HF, VHF-FM
to ground and airborne FACs, UHF for strike control) but names neither the jammers nor the front's regiments]. Medium (the adversary's view; no Soviet text
naming NATO's UHF air nets as a target was found). *SAM3D:* when our side (Pact-equipped) fights NATO, the army's jamming
units can be given NATO's strike-control and FAC nets; a jammed net delays or loses the enemy's re-tasking calls.

**comms-20 Pact intercept reach (NATO's view).** Intercept units "are moved forward immediately behind leading
regiments" and hear: artillery ground radar about 25 km, VHF about 40 km, HF ground wave about 80 km, HF sky wave without
limit, much further with airborne intercept; VHF DF "usually within ±3.5 degrees" (the OCR reads "±3 5"; Lawrence 1982
gives ±3.5°, about 7° total plotting error), low-power VHF picked up beyond 10 km, high power to 40 km; for ground radios "the danger point is reached when radio
transmissions exceed 20 to 25 seconds" [1984; `fm_100_2_1_1984` p. 15-2]. A 1981 US imagery report found two army-level
"radio and radar intercept companies" at Naushki with 75-80 vehicles each [`npic_1981_intercept`]. [1981-84] Medium
(the adversary's view; ranges are for ground signals: an aircraft at altitude is heard further, by line of sight).
*SAM3D:* the ranges set the ground-to-ground case; aircraft are heard to the radio horizon (inference, the EM layer's
propagation).

**comms-21 Soviet radio intelligence in East Germany listened in English (veteran).** S. Osipov served 1981-83 as a
sergeant in the 82nd separate radio-technical brigade OSNAZ at Torgau, attached to the GSFG and under the GRU's radio
intelligence: a radio intercept battalion and a synchronous direction-finding battalion at Torgau, separate battalions on
mountain sites (Brocken, Schneekopf, where the 220th separate radio-electronic reconnaissance centre stood). He spent six
months as a «микрофонщик», a voice intercept operator on English VHF traffic of US Army units (army aviation flying from
Bamberg, air defence, military police, armoured cavalry patrols among them); his English came from civilian life and the
junior specialists' school (not "school English" as first written); a telegraphist with weak English mis-wrote words; intercepts went on ruled forms to the duty
officer and on tape, and a doubtful translation was checked the same day by playing the tape over the telephone; REFORGER
umpire nets gave the front line away [`osipov_2013`]. A veterans' forum on the same brigade mentions a «Борт-Земля»
(air-ground) post listening to NATO pilots in plain English; the thread could not be read (encoding) and is recorded from
the search excerpt only [`torgau_forum`]. [1981-83] Medium for the organisation and the language problem, low for the
air-ground post. *SAM3D:* our side's (Pact-pattern) intercept operators work in a foreign language at varying skill: a
translation delay and a chance of a wrong word per message; a doubtful message is checked by telephone up the chain.

**comms-22 Strategic listening to NATO's air forces.** The Soviet "Krug" direction-finding network (eight separate radio
direction-finding nodes round the USSR, a central node at Klimovsk, under the General Staff) tracked "any information"
on SAC, TAC and NATO nuclear-capable aircraft and reconnaissance aircraft (AWACS, B-52, U-2, KC-135), including crews'
radio traffic, and fixed positions "at thousands of kilometres" by triangulation [`osnaz_874_lj`, a 2016 visitor's post
crediting warfare.ru]. [from the 1950s; formation ordered 12 May 1951] Low (tier 4, second hand; the post's list of
aircraft mixes eras, naming the B-1 and B-2 as well, so it describes the system over its whole life, not 1985; "crews'
radio traffic" (unverified) in the re-read). *SAM3D:* background only: the theatre's strategic
listening exists above the player's chain and feeds the army's intelligence by summaries.

**comms-23 Pact aircraft radios and the command link (low confidence).** The R-832M is a two-band set, 100-149.999 MHz and
220-399.999 MHz, 15 W, fitted to some L-39 series (later replaced by the LPR-80) and used at ATC stations; a reader's
comment says the R-832's control panel (not specifically the M) was in the MiG-23BN, MF and ML [`valka_r832m`; no dates
given]. The
Lazur(-M) command link (ARL-SM) worked with the ground system Vozdukh-1; courses, speeds, heights and single commands
for the aircraft were computed from the ground system or entered by hand [read only as search excerpts of a game wiki and a
DCS forum quoting a manual; not used; the sibling note `air_combat.md` wp-02 has it from `ru_wiki_mig23`]. [1960s-80s] Low.
*SAM3D:* Pact air voice can be on VHF 100-150 MHz or UHF 220-400 MHz, overlapping NATO's UHF band; datalinked GCI to
some fighter types sends no voice for our intercept to hear (inference), but its ground transmitter still radiates.

### 2.4 The neutral neighbour

**comms-24 Nothing found.** No period source on a small neutral air arm's radio procedure, emission control or
intercept service was found in this pass (the search budget ran out first). The Austrian analogue (`austrian_af_wiki`,
for scale only) would talk German on its nets (inference). *SAM3D:* game values: a few nets, short flights near the
border, little emission doctrine; our border posts hear them well; our intercept needs German-speaking (or the neutral's
language) operators. Low.

### 2.5 The rebels

**comms-25 A guerrilla army's intercept service, as the model for the rebels' listening.** The VC/NVA's technical
reconnaissance units (TRUs) were large and steady: one company of 130 processed 7,745 messages in September 1966; a
platoon of 69 averaged 500 a day (920 at most); one unit worked 31 voice nets of the US 1st Marine Division; they did
traffic analysis, DF and limited cryptanalysis, read tactical codes and lived on plain voice; they used prisoners and
reportedly deserters as operators [1966-69; `nsa_purple_dragon_1993` p. 68]. Medium. *SAM3D:* a rebel faction with a few
captured sets and native speakers of our language can read our plain voice well near its home ground. Its own few
aircraft (crop sprayers, light aircraft) would talk on civil or captured sets in our language (inference): no language
barrier for our posts, near-total content when heard.

### 2.6 Our side

**comms-26 An intercept service wired to the air defence command posts (East German pattern).** The NVA's radio
intelligence regiment FuAR-2 (Dessau; from 1 December 1989 the Zentraler Funkdienst): reconnaissance depth 400-450 km
(of the stationary complexes); "direct cooperation with the central command post of the LSK/LV" noted with the first
detection of a TR-1 on 17 July 1984; the KRTP-81 Ramona passive radar locator from 1984; the build-up of an airborne
intercept centre (An-26) begun in August 1985 (when it became operational is not given) [`militaermuseum_fuar2`]. The air
reconnaissance centre (LuAZ) in Dresden appears in the page's 1986 restructuring (post-period), and the 5 kW transmitters
belong to the new Scheuder transmitter site, not to FuAR-2 as such. By 1989-90: air
defence division (LVD) operative groups were stationed at the intercept centres North and South "to pass air-situation
information to the LVD command post"; daily reports went to the LSK/LV central command post; immediate reports covered
changes of readiness and exercises; reconnaissance covered 14 kHz to about 40 GHz with about 600 HF/VHF receivers
[`bischoff_fufutakl`]. [1984-89] Medium (tier 4 and an unsigned veterans' page; the operative groups' start date not
given; for the 1985 war the liaison arrangement below is a design choice modelled on the 1989-90 state, not a documented
1985 practice). *SAM3D:* our army intelligence's air section gets two outputs: an **immediate** air warning passed by a liaison at
the intercept post to the air defence division's command post (and from there down the warning net), and a **daily**
summary up the intelligence chain. Both are pipeline messages.

**comms-27 Our side's EMCON and dummy traffic (registered).** The Czechoslovak army's 1985 doctrine against TDOA
location: transmitter discipline, dummy loads, brief bursts, active dummies on the same parameters, and dummy traffic
[`vr_1985_plss`, as registered in `EM_RESEARCH_2026-10.md` §6-7; not re-read]. Medium. *SAM3D:* already in the EM layer
(`em/deception.hpp`); the same rules apply to our air defence and GCI nets that a NATO enemy hears (comms-06).

## 3. Numbers usable as data

| Quantity | Value | Faction | Date | Source | Confidence |
|---|---|---|---|---|---|
| Share of B-52 strikes alerted by enemy broadcast, before the fixes | 34 % | general (NVN vs US) | Dec 1966 | `nsa_purple_dragon_1993` pp. 22-26 | high |
| Average warning of those alerts | 8.5 h | general | Dec 1966 | same | high |
| Share alerted after the fixes | 5 % | general | Apr 1967 | same | high |
| Average warning after the fixes | < 30 min | general | Apr 1967 | same | high |
| Tactical warning from intercepted strike warnings in the clear | 10-30 min before bombs | general | 1966-69 | same p. 69 | high |
| Intercept to retransmission on a dedicated relay net | about 5 min on average | general | 1960s | same p. 61 | medium |
| Messages a day on one relay station at its peak | over 250 | general | 1960s | same p. 60 | medium |
| Time to break out a new set of US signal instructions | as little as 6 h (ARVN 2 h) | general | 1966-71 | same p. 69 (prisoners) | medium |
| Time to read brevity and slang codes | within 6 h | general | 1966-71 | same p. 68 (prisoners) | medium |
| A guerrilla intercept company's monthly load | 7,745 messages, 130 men | rebels (analogue) | Sep 1966 | same p. 68 | medium |
| A guerrilla intercept platoon's daily load | 500 a day, peak 920, 69 men | rebels (analogue) | 1968 | same p. 68 | medium |
| Teaball relay distance | about 300 miles (KC-135A on UHF line of sight) | NATO pattern (US) | 1972 | `boyne_2008_teaball` | medium |
| Kills to losses after Teaball | 30 MiGs to 10 US (29 Jul 1972 to war's end) | NATO pattern | 1972 | `boyne_2008_teaball`; CHECO's 19 to 5 in `aam_weapons.md` | medium |
| GCI capacity of an air army (3 division and 9 regiment CPs with automated control) | 30-40 groups or single aircraft at once | Warsaw Pact | 1962 doctrine | `mt_1962_front_aviation` | medium |
| Airborne alert zones of front fighters | 70 km and more over enemy territory | Warsaw Pact | 1962 doctrine | same | medium |
| Deployment time of coalition coordination means | ≤ 30 min (10-15 min some) | Warsaw Pact | late 1960s | `mt_coord_wp_air` p. 6 | medium |
| Required reliability of those means | ≥ 0.98 | Warsaw Pact | late 1960s | same p. 6 | medium |
| Tropospheric link distance between coordinating posts | 250-500 km | Warsaw Pact | late 1960s | same p. 7 | medium |
| Pact intercept reach from the front line (ground signals) | VHF ~40 km; HF ground wave ~80 km; artillery radar ~25 km | Warsaw Pact (NATO view) | 1984 | `fm_100_2_1_1984` p. 15-2 | medium |
| VHF DF accuracy | ±3.5° (about 7° total plotting error) | Warsaw Pact (NATO view) | 1982-84 | `fm_100_2_1_1984` p. 15-2 (OCR "±3 5"); `lawrence_1982` | medium |
| Transmission length at which a ground radio is located | 20-25 s | Warsaw Pact (NATO view) | 1984 | `fm_100_2_1_1984` p. 15-2 | medium |
| Army-level radio and radar intercept company | 75-80 vehicles | Warsaw Pact (US imagery) | 1981 | `npic_1981_intercept` | medium |
| East German radio intelligence reach | 400-450 km depth (stationary complexes; year not given) | our side (pattern) | 1980s | `militaermuseum_fuar2` | low-medium |
| East German intercept receivers | about 600 HF/VHF; 14 kHz-40 GHz | our side (pattern) | 1989-90 (post-period state; use as an upper bound for 1985) | `bischoff_fufutakl` | low-medium |
| R-832M bands and power | 100-149.999 and 220-399.999 MHz; 15 W | Warsaw Pact | 1970s-80s | `valka_r832m` | low |
| NATO air voice band | 225-400 MHz, AM | NATO | 1980s | `fh_radios` | high |
| HAVE QUICK keys | TOD, WOD, net number (WOD of 6 × 6 digits: (unverified) for 1985, from a 2000 Navy document via Wikipedia) | NATO | from 1980 | `fh_radios` | medium (keys), low (format) |
| HAVE QUICK hop interval (as described in 1987) | 0.1 s | NATO | 1987 | `correll_1987` | low |
| NATO E-3As at Geilenkirchen | 4 in April 1983; 18 planned by 1985 | NATO | 1983 | `afmag_1983_usafe` | medium |
| Black Buck One codeword to Northwood | 0746Z sent, by 0830Z received (at most 44 min) | NATO (UK) | 1 May 1982 | Burden et al. 1986 p. 365; White 2012 p. 337 (both via `wiki_black_buck`, not read) | medium |
| Bekaa air battle | four-ships fed in one at a time; fights of 1-2 min; ~26 MiGs in 30 min | NATO pattern (Israel) | 9 Jun 1982 | `grant_2002` | medium |
| Time from a Soviet recce find to attack (one analysts' estimate) | about 2 h | Warsaw Pact (NATO view) | 1982 | `lawrence_1982` | low |
| Intercept of KAL 007 pilot's calls to a US public statement | about 20-21 h (18:26 GMT to the 10:45 a.m. statement next day) | general | 1983 | `frus_1981_88_v4_d84` | medium |

Recorded only, not used (tier 5): AN/ARC-164 in its HAVE QUICK II form, 225-399.975 MHz, 7,000 channels, 10 W, guard
243.000 MHz (FAS DoD-101 page, 1999); HAVE QUICK production radios from December 1980, HAVE QUICK II development from May
1983 and in 1986 (`forecast_intl_hq`, snippet).

## 4. Disagreements between sources and which to use

| Question | One side | Other side | SAM3D uses |
|---|---|---|---|
| KAL 007: was the ground controller heard? | Shultz: the transcript showed ground control "ordered him to shoot" (`frus_1981_88_v4_d84`) | the NSA had the pilots' frequency, not ground control's (`acig_kal007`, snippet) | Pilots heard far, ground stations hard to hear (propagation); the case is low weight either way |
| Teaball's result | 30 MiGs for 10 losses, 29 July 1972 to the end (`boyne_2008_teaball`) | 19 MiGs for 5 losses, early August to mid-October (`checo_1978_linebacker`, in `aam_weapons.md`) | Different windows; both say the ratio rose; use the CHECO figure (tier 1) where a number is needed |
| Compass Call's target | "selected portions of the Soviet force management system" (`ulsamer_1981`) | "for the most part" airborne interceptors' air-to-air and air-to-ground communications (`hutcherson_1994`) | Fighter control voice (as `VOICE_JAMMING_2026-10.md` already decided) |
| HAVE QUICK's dates | "introduced in 1980" (`fh_radios`) | development from 1977, production radios December 1980 (`forecast_intl_hq`, tier 5) | 1980 as the introduction; fielding in USAFE by 1985 open |
| PURPLE DRAGON's start | a 1965 JCS study (Wikipedia) | the survey ran December 1966 to April 1967 (`nsa_purple_dragon_1993`) | the NSA's dates |
| El Dorado Canyon refuellings | four | four to six (popular accounts, snippets) | not needed: radio silence is the point |
| Lazur | a command link steering by autopilot (`ru_wiki_mig23` in `air_combat.md`) | commands shown to the pilot, computed on the ground or entered by hand (forum quote of a manual, snippet) | open: both may hold for different modes |

## 5. Post-period items excluded from the 1985 war (with dates)

- **HAVE QUICK II**: development reported from May 1983, production in 1986 (tier 5 snippet). Treated as later than 1985
  until a fielding date says otherwise. **SATURN** (STANAG 4372): ratified 1992.
- **SINCGARS** ground VHF hopping: fielded 1988 (`fh_radios`).
- **Compass Call based in Europe**: the 43d ECS activated at Sembach 1 October 1986, aircraft 1987-91
  (`wiki_usafe_ew_units`). In a 1985 war it would have to come from the United States (inference, `VOICE_JAMMING`).
- **JTIDS / Link 16 in fighters**: in 1981 only the EF-111A and the E-3A were named as planned users (`ulsamer_1981`);
  fighter terminals came later (date not checked here).
- **LANTIRN**: still "eagerly awaited" by USAFE in April 1983 (`afmag_1983_usafe`).
- **Mark XV IFF** (about 1993) and **ASPJ** (deliveries from 1990) (`correll_1987`).
- **Multi-service brevity codes as one published list** and "Fox three" for an active radar missile: 1990s (the sibling
  note `c2_retasking.md` gives 1997 for the first ALSA edition; AMRAAM 1991). The words used in 1985 are open.
- **Rivet Joint as a near-real-time provider "to tactical forces"**: the modern description (USAF fact pages of 2007-08,
  snippets); not used for 1985.
- **AMRAAM, R-77, killer scouts, post-1991 SEAD/DEAD**: per the brief; nothing here depends on them.
- **El Dorado Canyon** (14-15 April 1986): months after the period; used only as evidence that comm-out refuelling was
  practised by then, not as a 1985 event.
- **East German air intercept as of 1986-90** (added in verification, 15 Nov 2026): the air reconnaissance centre (LuAZ)
  in Dresden appears in FuAR-2's 1986 restructuring; the An-26 airborne centre's build-up only began in August 1985; the
  LVD operative groups at the intercept centres North and South, the ~600 receivers and 14 kHz-40 GHz coverage are the
  state of 1989-90 with no start date (`militaermuseum_fuar2`, `bischoff_fufutakl`). For the 1985 war they are a pattern
  for game values, not documented 1985 fact.
- **GPS time of day for HAVE QUICK** (Crypto Museum's modern description): GPS was not operational in 1985.
- **HAVE QUICK WOD format of six six-digit segments**: known only from a US Navy training plan of March 2000 (via
  Wikipedia); may be HAVE QUICK II's.
- **The Krug network's later targets** (B-1B from 1986, B-2 from 1989-97) named in `osnaz_874_lj`: not 1985.
- **Later documents used only for categories or as pointers**: FM 90-24 (1991), FM 34-40-7 (1992), Hutcherson (1994),
  the ICAO KAL 007 report (1993), the NSA's PURPLE DRAGON history (1993, about 1966-71).

## 6. Sources

| Id | Tier | Citation | URL | Read | Whose view |
|---|---|---|---|---|---|
| `nsa_purple_dragon_1993` (new) | 1 | NSA Center for Cryptologic History, *PURPLE DRAGON: The Origin and Development of the United States OPSEC Program*, 1993 (declassified, heavily redacted) | [archive.org text](https://archive.org/stream/purple_dragon-nsa/purple_dragon_djvu.txt); [NSA PDF](https://www.nsa.gov/portals/75/documents/news-features/declassified-documents/cryptologic-histories/purple_dragon.pdf) | full OCR text, searched and the cited pages read | the US on its own security and on what the enemy got |
| `mt_1962_front_aviation` (new) | 1 | «Военная мысль», secret collection, No. 2 (63) 1962, article on the actions of front aviation in the first front offensive operation of the initial period of war; CIA translation, CIA-RDP10-00105R000302480001-9 | [archive.org](https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000302480001-9) | OCR text searched; the cited passage read | Soviet |
| `mt_1964_air_army` (new) | 1 | «Военная мысль», No. 2 (72) 1964, article on the actions of the air army during the movement forward and commitment to action; CIA translation of 21 Sept 1976, CIA-RDP10-00105R000302360001-2 | [archive.org](https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000302360001-2) | OCR text searched; p. 6 read | Soviet |
| `mt_1968_umnov` (new) | 1 | Maj. Gen. of Artillery M. Umnov, «Air and Air Defense Forces in the Near East War», «Военная мысль» No. 2 (84) 1968; CIA translation of 23 Oct 1973 | [archive.org](https://archive.org/details/cia-readingroom-document-5166d4f899326091c6a60590) | OCR text searched; p. 7 read | Soviet (on Israel, 1967) |
| `mt_coord_wp_air` (new) | 1 | Gen.-Maj. of Aviation M. ?rslamov (garbled in OCR), «Some Problems in Ensuring the Coordination of the Air Forces and Air Defenses of the Socialist Countries», «Военная мысль» (issue on the denied pages; cites exercises of 1965-67); CIA-RDP10-00105R000100900001-6, released 16 Oct 2012 | [archive.org](https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000100900001-6) | full text read (pp. 4-10) | Soviet |
| `mt_1973_nvn` (registered) | 1 | «Военная мысль» No. 2 (81) 1967, on the DRV's air defence against US aircraft; CIA translation 18 Sept 1973 (read from the copy 5166d4f899326091c6a60591) | [archive.org](https://archive.org/details/cia-readingroom-document-5166d4f899326091c6a60591) | OCR text searched; pp. 17-18 read | Soviet |
| `mt_1984_babich` (registered) | 1 | V. K. Babich, «Военная мысль» No. 5, 1984, pp. 34-43; CIA-RDP10-00105R000403350001-1 | [archive.org](https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000403350001-1) | OCR text searched; Vietnam and Lebanon sections read | Soviet |
| `fm_100_2_1_1984` (registered) | 1 | US Army FM 100-2-1, *The Soviet Army: Operations and Tactics*, 16 July 1984, ch. 15 | [archive.org](https://archive.org/details/fm100-2-1) | OCR text searched; pp. 15-2 to 15-4 read | NATO's view of the Pact |
| `npic_1981_intercept` (new) | 1 | NPIC, *New Radio and Radar Intercept Companies in the Transbaikal Military District, USSR*, IAR-0269/80, March 1981; CIA-RDP81T00380R000100140001-7 | [archive.org](https://archive.org/details/cia-readingroom-document-cia-rdp81t00380r000100140001-7) | full (2 pp.) | US imagery view of the Soviets |
| `frus_1981_88_v4_d84` (new) | 1 | *Foreign Relations of the United States, 1981-1988*, vol. IV, document 84, editorial note on KAL 007 | [history.state.gov](https://history.state.gov/historicaldocuments/frus1981-88v04/d84) | full | US |
| `hutcherson_1994` (registered) | 2 | Lt Col N. B. Hutcherson, *Command and Control Warfare*, AU-ARI-94-1, 1994 | [OCR](https://nsarchive.gwu.edu/media/16145/ocr) | first 100,000 characters of the OCR | US officer, hindsight |
| `osipov_2013` (new) | 2 (veteran, through press) | С. Осипов, «Шпион по призыву», *Независимое военное обозрение*, 25 Jan 2013 | [nvo.ng.ru](https://nvo.ng.ru/spforces/2013-01-25/1_spy.html) | full | Soviet veteran (1981-83) |
| `bischoff_fufutakl` (new) | 2 (unsigned veterans' page) | «Die Funk- und Funktechnische Aufklärung (FuFuTAkl) der NVA», manfred-bischoff.de | [page](https://www.manfred-bischoff.de/fufutakl.htm) | full | East German (state of 1989-90) |
| `torgau_forum` (new) | 2 | «Назад в ГСВГ» forum, thread on the 82nd ORTBr OSNAZ, Torgau (2009-10) | [thread](https://nazadvgsvg.ru/viewtopic.php?id=485) | search excerpt only (page unreadable) | Soviet veterans |
| `grant_2002` (registered) | 4 (Ivry's words: 2) | R. Grant, «The Bekaa Valley War», *Air Force Magazine*, June 2002 | [airandspaceforces.com](https://www.airandspaceforces.com/article/0602bekaa/) | full | Israeli commander, US press |
| `lawrence_1982` (registered) | 4 | Lt Col D. B. Lawrence, «Soviet Radioelectronic Combat», *Air Force Magazine*, March 1982 | [airandspaceforces.com](https://www.airandspaceforces.com/article/0382radioelectronic/) | full | NATO's view of the Pact |
| `ulsamer_1981` (registered) | 4 | E. Ulsamer, «New Era in Electronic Warfare», *Air Force Magazine*, Sept 1981 | [airandspaceforces.com](https://www.airandspaceforces.com/article/0981era/) | full | US |
| `correll_1987` (registered) | 4 | J. T. Correll, «With Waveforms and Wits», *Air Force Magazine*, June 1987 | [airandspaceforces.com](https://www.airandspaceforces.com/article/0687electronic/) | full | US |
| `afmag_1983_usafe` (new here; also cited by `factions_cases.md`) | 4 | «USAFE in the Dangerous Decade», *Air Force Magazine*, April 1983 | [airandspaceforces.com](https://www.airandspaceforces.com/article/0483usafe/) | full | US (period press) |
| `boyne_2008_teaball` (new) | 4 | W. J. Boyne, «The Teaball Tactic», *Air & Space Forces Magazine*, 1 July 2008 | [airandspaceforces.com](https://www.airandspaceforces.com/article/0708teaball/) | full | US, hindsight |
| `wiki_black_buck` (registered) | 4 (pointer only; Wikipedia is never the cited source) | Wikipedia, «Operation Black Buck», citing Burden, Draper, Rough, Smith and Wilton, *Falklands: The Air War* (1986) pp. 365 and 395, and White, *Vulcan 607* (2012) p. 337 for the 0830Z receipt (corrected in verification: not Middlebrook p. 78). The books should be registered as their own ids once read | [wikipedia](https://en.wikipedia.org/wiki/Operation_Black_Buck) | full (the books not read) | British |
| `mobility_forum_2020` (new) | 4 (quotes a pilot: 2) | T. Mulligan, «Mobility Operations: Global Reach and Operation El Dorado Canyon», *The Mobility Forum*, 9 Sept 2020 | [themobilityforum.net](https://themobilityforum.net/2020/09/09/mobility-operations-global-reach-and-operation-el-dorado-canyon-the-1986-strike-on-libya/) | full | US |
| `nmusaf_bolo` (new) | 4 | National Museum of the USAF, fact sheet «Operation Bolo» | [nationalmuseum.af.mil](https://www.nationalmuseum.af.mil/Visit/Museum-Exhibits/Fact-Sheets/Display/Article/196006/operation-bolo/) | search excerpt only (fetch refused) | US |
| `fh_radios` (registered) | 4 | Crypto Museum, «HAVE QUICK»; Wikipedia, «Have Quick» | [cryptomuseum.com](https://www.cryptomuseum.com/radio/havequick/index.htm); [wikipedia](https://en.wikipedia.org/wiki/Have_Quick) | full | reference pages |
| `militaermuseum_fuar2` (new) | 4 | Militärmuseum Anhalt, «NVA Funkaufklärung» | [page](https://militaermuseum-anhalt.de/nva-funkaufklaerung.html) | full | East German, museum |
| `valka_r832m` (new) | 4 | S. Vystavěl and a commenter, «SOV - R-832M (letecká VKV/UKV radiostanice)», valka.cz / armedconflicts.com | [armedconflicts.com](https://www.armedconflicts.com/topic/view/80222) | full | Czech enthusiasts |
| `osnaz_874_lj` (new) | 4 | shvedov_dm_a (2016), repost on osnaz-cikle.livejournal.com, «874 отдельный радиопеленгаторный узел ... Система "Круг"», history credited to warfare.ru (2013) | [livejournal](https://osnaz-cikle.livejournal.com/133846.html) | full | Russian, second hand |
| `acig_kal007` (new) | 4 | ACIG.org article on KAL 007 | [acig.org](https://acig.org/artman/publish/article_300.shtml) | search excerpt only | enthusiast analysis |
| `forecast_intl_hq` (new) | 5 | Forecast International archive, «HAVE QUICK II/IIA» and «ARC-164(V)» | [forecastinternational.com](https://www.forecastinternational.com/archive/disp_old_pdf.cfm?ARC_ID=563) | search excerpt only; recorded, not used | industry |
| `vst_1988`, `vr_1985_plss`, `dvids_2018_ec130h`, `wiki_usafe_ew_units`, `austrian_af_wiki` (registered) | as registered | see `docs/SOURCES.md` | see there | not re-read in this pass | as registered |

Also seen and not used: Wikipedia «Teufelsberg» (Field Station Berlin's listening began in July 1961; nothing on air
traffic), Wikipedia «Multiservice tactical brevity code» (no history of the words), FAS «AN/ARC-164» (tier 5), five
further *Военная мысль* translations searched without a relevant passage.

## 7. Open items and documents to acquire

- **The search budget ran out after 13 searches.** A follow-up pass should take the items below first.
- **HAVE QUICK in USAFE**: when HAVE QUICK I reached the F-15, F-16, A-10, F-111 and F-4 in Europe, and whether KY-58
  secure voice was in fighters by 1985 (GAO reports, USAFE histories, *Air Force Magazine* 1982-86).
- **NATO's radio procedure in 1985**: check-ins with a CRC or an E-3A, call-sign rotation, frequency plans, the brevity
  words then in use (MCM 3-1 of the time was classified; look for a public NATO or RAF training text, or aircrew
  memoirs). Whether "Fox three" then meant the gun.
- **NATO's intercept of Pact GCI in Europe**: Field Station Berlin, RAF Gatow, the RAF's Nimrod R1 and the RC-135 against
  Pact air defence voice; whether tactical warnings reached NATO aircrews in the 1980s (a Teaball for Europe) and how fast.
  NSA's *American Cryptology during the Cold War*, Book IV (1980-89), and the NSA *Teaball* and *Spartans in Darkness*
  PDFs (both refused the fetch).
- **Soviet aviation's own rules for radio exchange**: the НПП (flight regulations) and the radio exchange rules for
  military aviation; the R-832M's and R-862's technical descriptions; the Lazur/Vozdukh-1 description (dates, which types,
  update rate); the MAI tactics text's sections on control (its site's certificate refused the fetch).
- **Soviet air OSNAZ**: the air armies' radio intelligence units (the 16th Air Army in the GDR) and the «Борт-Земля» post
  at Torgau; what Pact intercept of NATO air traffic yielded in exercises (warning of raids from check-ins and tanker
  traffic). The forum thread needs a browser.
- **Pact jamming of NATO UHF air nets**: a Soviet or Pact text naming the R-834 or another jammer against aviation
  command nets, its band and power (Palii 1989, registered, may have it).
- **Strike radars and TFR emissions**: whether F-111 and Tornado crews kept their terrain-following and attack radars in
  their lowest emission modes on ingress, and whether Pact ELINT could find them (period studies or RAF Tornado accounts).
- **The period's other wars**: mujahideen listening to Soviet air-ground radio in Afghanistan to 1985; Iranian and Iraqi
  intercept of each other's air traffic; Argentine Stanley radar warning Skyhawks of Sea Harrier patrols (1982); SADF
  COMINT over Angola (registered `oosthuizen_2014`, `saaf_707`).
- **Dummy radio traffic for aviation**: a Soviet or Pact text on radio deception («радиоигра», «ложный радиообмен») by air
  units, and a NATO case of dummy air traffic in the period.
- **The neutral's air radio**: an Austrian, Swiss or Swedish document on radio procedure and listening in the 1980s.

## Verification (15 Nov 2026)

An adversarial check of the findings that carry numbers, drive design rules or might be misdated. The web-search budget
was already spent when the check began, so only documents at known addresses were fetched (the archive.org OCR texts of
PURPLE DRAGON, FM 100-2-1 and six *Военная мысль* translations were downloaded and searched in full; the other pages
were fetched). Verdicts: confirmed, corrected (claim kept, wording or citation fixed), unsupported (marked
(unverified) in the text), post-period (moved to section 5).

| Finding | Verdict | Note |
|---|---|---|
| comms-01 | confirmed | PURPLE DRAGON OCR: NOTAM "altitude, flight path, and entry and exit times" to all adjacent ATCs; Dec 1966 34 % with 8.5 h; Apr 1967 5 % under 30 min; KW-26 within a week; all as written. |
| comms-02 | confirmed | p. 69: Heavy Artillery Warnings gave "target coordinates and TOT, usually ten to thirty minutes"; nonsecure voice the "major source of COMINT"; the battalion escaping twice is there. |
| comms-03 | confirmed | pp. 60-61: 0700 local carrier position reports, "over 250" a day, retransmitted "on average within five minutes"; heavily redacted, medium kept. |
| comms-04 | confirmed | p. 68-69: "for years at a time", CAPTAIN CONTROL and DINBACK (PWs), SOIs in six hours (ARVN two), brevity codes within six hours; p. 78 the EC-121's unchanging call sign in Korea. |
| comms-05 | confirmed | p. 27 wording exact; reading that radar outran COMINT stays an inference. |
| comms-06 | corrected | Boyne 2008 confirmed (26 July 1972, Luzon relay ~300 miles, UHF outages, 30 for 10); calls were heading, speed and vector in Red Crown's format; flagged as 1972 US practice, a European equivalent by 1985 not found; `checo_1978_linebacker` is not registered. |
| comms-07 | confirmed | `mt_1973_nvn` (Issue 2 (81) 1967) pp. 17-18 and Babich (No. 5, 1984) quoted correctly; the dive to 400-300 m also followed a panel light or the SAM command transmitter coming on. |
| comms-08 | confirmed | FRUS 1981-88 v. IV d. 84: 8:20 a.m. call, "the CIA had a transcript" within an hour, 10:45 a.m. EST press conference on 1 Sept 1983 (about 21 h after the shoot-down); Shultz's memoir is hindsight. |
| comms-09 | corrected | Crypto Museum: introduced 1980, ECCM not encryption, TOD/WOD/net, KY-57/58; Correll 1987 "every tenth of a second" confirmed. The six-by-six WOD is from a 2000 Navy document via Wikipedia (unverified for 1985); GPS TOD is post-period; fielding in USAFE by 1985 still not found. |
| comms-10 | confirmed | *Air Force Magazine* Apr 1983: four E-3As, eighteen by 1985, EIFEL 1 computer-to-computer with Kalkar and Messstetten, Maastricht to get it, TLP eight four-week courses; Hutcherson's OCR confirms DoDD 4600.4 of 27 Aug 1979 and MOP 185 of 20 Dec 1983. |
| comms-11 | corrected | 0746Z (Burden p. 365) and the Victor captain (Burden p. 395) confirmed via Wikipedia; 0830Z is cited there to White 2012 p. 337, not Burden or Middlebrook; Wikipedia demoted to pointer. El Dorado Canyon wording confirmed but flagged April 1986. |
| comms-12 | confirmed | Grant 2002: E-2C downlink, voice from the command post, VHF, RPV data in "seconds", four-ships one at a time, 1-2 min fights, "about 26 MiGs" in half an hour (Ivry's own figure); Babich's E-2C passage confirmed. |
| comms-13 | unsupported | The museum fact sheet refused the fetch again (HTTP 403); the claim rests on a search excerpt only. |
| comms-14 | confirmed | Umnov, Issue 2 (84) 1968, p. 7: jamming from ships then aircraft, against control nets and centimetre-band radars, jammers on only in radar range, missile and gun-laying jamming "not very successful". |
| comms-15 | confirmed | Grant 2002 quotes exact, including "mock jamming of fighter and ground communications". |
| comms-16 | confirmed | `mt_1962_front_aviation` (Issue 2 (63) 1962): three division and nine regiment CPs, 30-40 groups, "a sortie for nothing", alert zones 70 km and more over enemy territory. |
| comms-17 | confirmed | Coordination article: exercises 1965-67, down to aircraft crews, decimetric sets, frequencies and call signs, 30 min (10-15), reliability 0.98, 250-500 km tropo links, languages, concealment. |
| comms-18 | confirmed | `mt_1964_air_army` (Issue 2 (72) 1964) "complete radio silence" quoted exactly; Lawrence 1982 "Radio silence is standard operating procedure" confirmed; `vst_1988` is registered as described (not re-read). |
| comms-19 | corrected | FM 100-2-1 (1984) confirmed for the TACS targets, R-330A and R-834 and the front's four regiments; Lawrence 1982 has only the TACS targets, not the jammers or regiments. Adversary's view, labelled. |
| comms-20 | corrected | FM 100-2-1 ranges (25/40/80 km), 20-25 s danger point confirmed; DF accuracy is ±3.5° (OCR "±3 5", Lawrence ±3.5°), not ±3-5°; NPIC March 1981 confirmed (Naushki, two companies, 75-80 vehicles each, Transbaikal not GSFG). |
| comms-21 | corrected | NVO 2013: 1981-83, 82nd ORTBr OSNAZ Torgau, attached to the GSFG, GRU; English VHF of the US Army incl. army aviation from Bamberg; his English came from civilian life and the specialists' school, not "school English"; forms, Zvuk-1M tape and the telephone check confirmed; nothing on NATO pilots. |
| comms-22 | corrected | LiveJournal repost confirmed (Klimovsk centre, eight DF nodes, General Staff, 1951); its aircraft list includes B-1 and B-2, so it is not a 1985 snapshot; crews' radio traffic not confirmed in the re-read. |
| comms-23 | corrected | valka.cz: bands and 15 W confirmed, L-39 (some series) and ATC stations; the MiG-23 comment concerns the R-832's panel, not the M; no dates. |
| comms-26 | corrected | FuAR-2 page: 400-450 km, TR-1 17 Jul 1984, KRTP-81 from 1984 confirmed; the An-26 centre's build-up only began Aug 1985; the Dresden LuAZ is in the 1986 structure; 5 kW is the Scheuder site's. Bischoff: operative groups at Räggow and Zella-Mehlis, daily and immediate reports, ~600 receivers, 14 kHz-40 GHz, state of 1989-90, no start date. Post-period parts moved to section 5. |
| comms-24, -25, -27 | not re-checked separately | comms-25's figures (7,745 in Sept 1966; 500 a day, peak 920, late 1968; 31 nets of the 1st Marine Division) were seen in the PURPLE DRAGON OCR at p. 68 and are confirmed. |

Scan for modern tactics: none of AMRAAM, R-77, Link 16 in fighters, LANTIRN, NVG strikes, killer scouts or post-1991
SEAD/DEAD is presented as 1985 practice; they stay in section 5. Items that read as 1985 practice but are dated later or
undated have been flagged: Teaball's European equivalent (not found), the East German liaison groups (1989-90), GPS
time for HAVE QUICK, the WOD format, El Dorado Canyon (1986).
