# Air and ground C2 in the hot phase, and retasking a flight in the air, 1980-85

Research note for SAM3D's enemy air war (`simcore::airwar`, `docs/air_war/`). The war's year is 1985.

**Scope.** Who controls attack aircraft once a war is hot. Who may change a flight's route or target while it is airborne, by which means, and with what delay. How a SAM site found by one aircraft is passed on. What a live headquarters loop in SAM3D must contain.

**Period rule.** Only what was in service or in doctrine by 1985. Documents written up to 1988 count as period evidence when they describe the system as it stood by 1985. They are marked where they speak of 1986-88. Material from after 1988, the Gulf War included, is hindsight. It is flagged where used and never carries a design value on its own.

**The user's instruction:** keep this to Cold War practice, not modern tactics. Section 5 lists what was left out for that reason.

**How the sources were read.**
- Most NATO and US material is DTIC theses and training texts by serving officers, read in full from archive.org's DTIC mirror.
- Most Warsaw Pact material is the adversary's view: FM 100-2-1, a DIA report, and US Army Russian Institute papers.
- The Soviets' own voice comes from «Военная мысль» (Military Thought) articles in CIA translation, and from an operator-country historian on Afghanistan.
- Section 6 says which sources were read in full and which only as snippets.

---

## 1 Summary for SAM3D's design

### 1.1 The answer in one table

Who may change a flight in the air, 1985:

| Faction | Who may change a flight in the air | By what means | Delay |
|---|---|---|---|
| **NATO, Central Region** | The **flight leader** only within his fragged mission. He attacks, takes the frag's alternate target, aborts or jettisons, and evades on a warning. He does not take a new target outside his frag. A **FAC** (on the ground or in an Army helicopter; no airborne FAC squadrons left in Europe) briefs and clears flights handed to him. He cannot pull flights off other missions. The **ASOC** at corps fills immediate requests. It draws on its allocated sorties, on airborne and ground alert, and on authorised diversion of preplanned CAS sorties. Under the 1976 test rules an airborne divert needs the Army's approval. The **ATOC** (the TACC in US terms) has tactical control. It tasks wings, directs execution, may divert BAI, and may scramble outside one corps' allocation. Under J-SAK (1984) it diverts any BAI and only informs the Army. The **CRC/CRP and the E-3A** warn and control. A CRC scrambles or diverts *air defence* fighters with the TACC's concurrence. No period source gives them authority to retask strike flights. A **Wild Weasel** lead commands only his own hunter-killer team. | Requests go up the Air Force request net to the ASOC. Diverts go by voice (UHF/VHF), directly or relayed through a CRC, CRP or E-3A. There is no data link to a strike aircraft. Nets can be jammed and are listened to. | An immediate CAS request takes about 15-20 min to bombs at best (airborne alert; the Vietnam goal) and 40-50 min as achieved (Korea, USAF/Army; before the 1965 changes in Vietnam over 90 min). Silence counted as consent after a preset time (1976 doctrine); 5 min was the figure in the early Vietnam ARVN/VNAF net. A BAI nomination took 48 h from division to the flight (1983). AI and deep strike are not retasked in the air. |
| **Warsaw Pact** | The **pilot** flies the plan. He may attack targets of opportunity along his route, and on armed reconnaissance or a free hunt he picks targets in his area. The **flight leader** assigns individual targets once the FAC confirms. **Ground control** (the control and target-identification posts, the ГБУ, the regiment's FAC) may change the route through friendly air defence and designates or confirms the target at the checkpoint. The **division commander**, through his aviation control element, approves immediate strikes within his allocation; army or front approves anything more. The **air army and air regiment KPs** scramble aircraft on readiness 1 or 2 and occasionally divert an armed reconnaissance aircraft within its area. | Command radio from the KP and the control posts. At low level radio is minimal or forbidden. Beacons, flares, smoke and panels mark friendly troops. | Takeoff takes 3-5 min from readiness 1 and 15 min from readiness 2. Doctrine gave 20-30 min from request to strike from airfield alert; a 1961 exercise took 17-28 min. Airborne alert is faster but costly. Air reconnaissance takes 2-8 h to process. |
| **Neutral** | No period source found. **SAM3D assumption:** a small, centralised national system. Fighters are ground-controlled. Ground attack flies planned missions only. There are no FACs in the air. | Voice, ground radar control | Assumed to follow ground-alert timings |
| **Rebels** | No source. **SAM3D assumption:** no airborne control. Each sortie flies its own plan, and the leader is his own retasker. | Ground radio to their own troops at most | No in-air retasking |

### 1.2 What this means for SAM3D

1. **Planned strikes fly as planned in 1985, on both sides.** NATO's interdiction (AI) and deep strikes were hardly retasked in the air: the means to retask them "up to the very point of target engagement" did not exist, though the ATOC's FIDO could redirect an airborne mission before the IP or divert its recovery. A 1987 USAFE-sponsored thesis still wrote that "someday airborne diverts of our air interdiction assets will be possible" (c2-05). Pact strikes follow the flight plan unless ground control changes it (c2-21). The enemy air command's ATO-like plan, as SAM3D already has it, is the right core.
2. **In-flight changes happen near the front, for close support, through a ground node.** On NATO's side this is the ASOC or ATOC with a FAC. On the Pact's side it is the division's aviation control element, the ГБУ or the regimental FAC at the checkpoint. A live HQ loop belongs on those nodes, not on the strike aircraft.
3. **A battery that reveals itself can be attacked in three ways, each with its own clock:**
   - by aircraft already in the area that are allowed to attack threats: a Weasel team inside its own team, or a Pact free hunt or armed reconnaissance inside its area. This takes minutes.
   - through an immediate request if the battery is near the forward edge and a ground unit asks for it. This takes 15-50 min.
   - through nomination into the next air tasking cycle. This takes 24-48 h or more.

   Pilots' reports reach the enemy's staff after landing (5-10 min to the regiment's KP in Soviet practice) or in flight only when asked for. A position found by photograph is 6-8 h old by the time it is used (c2-15). This fits the first-person pipeline: the enemy too learns only through people and means.
4. **Threat warnings are not retasking.** An E-3A, a CRC or another flight may warn by voice of a SAM or MiGs. The flight leader then evades, jettisons or aborts on his own. These are the reactions SAM3D's track R already models.
5. **Voice nets are the medium, and they can be jammed and intercepted.** A retask or a request is a message that can be lost or late. Our side's intercept (`docs/command/`, P3) can hear it.

### 1.3 What a live HQ loop in SAM3D must contain

- **Nodes with authority by mission type.**
  - NATO: TACP/FAC (brief and clear), ASOC (CAS divert and scramble within its corps), ATOC (BAI divert, cross-corps), CRC/CRP and E-3A (warn, control fighters), Weasel lead (own team).
  - Pact: FAC/ГБУ (designate at the checkpoint), division aviation control element (approve within its allocation), army/front (above it), air army or regiment KP (scramble, divert armed reconnaissance).
- **Pools with timings.**
  - NATO: airborne alert, ground alert at 5 and 15 min, and preplanned CAS sorties that can be diverted.
  - Pact: readiness 1 (3-5 min, held 1-2 h), readiness 2 (15 min, held 2-4 h), readiness 3 (1-2 h), and airborne-alert zones in local wars.
  - Each pool has its endurance. An A-10 sortie lasted about 2 h, and a pilot's attention flags after 30 min of low-level airborne alert.
- **Requests as messages.** Each request has a source, a target, a time sent, a chain of nodes with delays, a silence-is-consent window (a "preset time limit" in TACR 55-46 TEST, 1976; 5 min in the early Vietnam ARVN/VNAF net, the only number found) and a refusal.
- **Means.** Voice nets with line-of-sight range to low-flying aircraft, relays (CRC, E-3A; the An-26RT in Afghanistan), jamming, interception.
- **Check-in and attack.** A check-in gives the call sign, ordnance and time on station. The FAC's brief follows. In a high threat there is one pass.
- **Reports.**
  - In-flight reports only if requested.
  - Pilots' debrief after landing.
  - Reconnaissance processing of 2-8 h.
  - The age of every reported position.
- **Failures.** Late or stale nominations (48 h against a 24 h horizon). Refusals. A wrong target, as when an Su-17 leader refused the FAC near Kandahar in 1984 and bombed his own battalion.

It must **not** contain data-linked retasking, kill boxes, "push CAS", JSTARS-cued strikes, an AWACS that retasks strike packages, Link 16 in fighters or 1990s brevity calls (section 5).

---

## 2 Findings by faction

Each finding gives its id, date and [source id, page or section].

### 2.1 NATO and the United States (Central Region)

**c2-01. Structure of offensive air C2 in the Central Region, 1974-85.** [rifenburg_1989_c2, ch. on AAFCE; rippe_1985_airland, ch. 3, figs 3-4 and 3-5, pp. 31-33]
- AAFCE was re-formed at Ramstein in June 1974.
- Each ATAF had an Air Command Operations Centre (ACOC), "essentially NATO's version of the TACC". Under it were two ATOCs: 2ATAF's at Kalkar and Maastricht, 4ATAF's at Sembach and Messstetten. An ASOC sat with each corps.
- The ATOC had tactical control, meaning "detailed and local direction and control", over the offensive aircraft allocated to it. It tasked the flying units and directed the execution of offensive air support missions. Rippe sums it up: the ACOC plans the war and the ATOC executes it.
- Offensive forces stayed national until handed to NATO.
- Defensive fighters were controlled separately by Sector Operations Centres (SOCs). Rifenburg found the coordination of offence and defence "costly, time consuming, and inefficient".
- Collocating SOC III with ATOC Sembach began in October 1983.
- *Date:* 1974-85.

**c2-02. NATO's offensive air support was target-oriented, not mission-oriented.** [rippe_1985_airland, pp. 33-35]
- In NATO, offensive air support (OAS) was tactical air reconnaissance, BAI and CAS. It was apportioned to the army group as direct support.
- BAI belonged to the corps' area forward of the FSCL, bounded by the Reconnaissance and Interdiction Planning Line (RIPL). Beyond the RIPL was AI, tasked by echelons above corps.
- The US J-SAK manual allowed air support "on a mission oriented basis"; Central Region procedures did not.
- *Date:* 1984-85.
- *Design:* in a NATO war a ground unit cannot hold a flight and use it as it likes. Each flight goes to a target that the ATOC tasks or approves.

**c2-03. Who may divert, in US doctrine.** [morrow_1977_tacs, citing TACR 55-46 (TEST), Aug 1976; hoak_1987_retasking, citing TACR 55-46, 1986; rippe_1985_airland, J-SAK summary]
- Immediate requests went from a TACP (battalion and up) on the Air Force request net straight to the DASC/ASOC.
- The levels in between acknowledged. A request was approved unless one of them denied it within a preset time (no number in the 1976 text).
- Morrow describes the late Vietnam system the same way: the DASC filled requests by scrambling fighters or, if the unit operations officer (G-3/A-3) approved, by diverting inflight sorties.
- Requests were filled "from scrambles, airborne alert, or mission diverts", and "airborne diverts must be approved by the Army" (TACR 55-46 TEST, 1976).
- By 1986 TACR 55-46 had the ASOC filling immediate needs "from allocated sorties or by authorized diversion of preplanned sorties".
- Under the J-SAK interim procedures (1984), "the Tactical Air Control Center has the authority to divert any BAI mission". It need only inform the Army's Battlefield Coordination Element.
- *Date:* 1976-86.

**c2-04. Dynamic retasking in the air was practised but not official, and short of time.** [hoak_1987_retasking, ch. 1-2]
- The ASOC's procedures were manual ("antiquated manual procedures"). (That the system dated from the 1960s is unverified: not found in Hoak's text.)
- "Officially, the concept of dynamic retasking being done by the ASOC is not currently recognized", although "considerable retasking has occurred in recent conflicts".
- Retasking "is even more viable from the ABCCC, which occasionally serves as an airborne ASOC".
- With the technology of the time, a retasked crew would rarely have time to replan unless the target area was low-threat.
- The thesis cites a 1987 TAF statement of need: missions "may not be alerted, scrambled, or diverted within acceptable time limits".
- *Date:* written 1987, describing the system of the mid-1980s.

**c2-05. Interdiction was hardly retasked in the air.** [schoeck_1987_ai_retasking, ch. 1-2 and the DSS storyboards; sponsor 17th Air Force, USAFE]
- Even where aircraft could be reallocated and diverted, crews and C3 elements lacked the means "to change rapidly and repeatedly". The capability "to dynamically retask AI assets up to the very point of target engagement" did "not presently exist".
- The FIDO sits at the ATOC. The ATO covers 24 h, and air tasking messages (ATMs) modify its preplanned sorties. The thesis does treat the FIDO redirecting airborne aircraft (before the IP) and diverting returning aircraft to another base as things he must decide; what was lacking was speed and repetition, not all authority. *(Corrected 15 Nov 2026: the earlier wording "not retasked in the air" was too absolute.)*
- The thesis's notes say airborne diverts of AI aircraft will be possible "someday", "both in terms of communications and pilot workload".
- Reasons to retask: the target changes (weather, movement, destroyed, priority); the aircraft's load changes (jettisoned when engaged by an airborne adversary); range limits from diverts or route changes; the recovery base is under attack.
- "It will take more time to reconfigure an aircraft than it will to find a new target." (unverified: not found in the OCR text on re-check)
- The FIDO's highest priority is his airborne assets. TOT windows of 20 min are assumed.
- *Date:* 1987, USAFE, describing practice up to then.
- *Design:* SAM3D's deep strikes and OCA in 1985 fly their target, take a fragged alternate, or go home. A rare, slow ATOC redirect before the IP, by voice, may be allowed; a divert to another recovery base is normal.

**c2-06. BAI was slow from nomination to strike.** [gallant_knight_1983, AAR; rippe_1985_airland, J-SAK planning]
- In JTX Gallant Knight 83: "It now takes 48 hours from the time a division nominates a target for BAI until the requested mission is flown". Divisions planned no more than 24 h ahead, so many targets were no longer valid.
- Under J-SAK, corps planned 72 h ahead.
- The joint force commander apportioned 30-36 h before the ATO period. Corps learned its sortie numbers 30 h before.
- *Date:* 1983-84.
- *Design:* a battery found and nominated for BAI is struck a day or two later, if at all. It will likely have moved.

**c2-07. Immediate CAS response times.** [luke_2002_cas, pp. 18-29, citing Cooling, *Case Studies in the Development of CAS* (1990), which took its Vietnam statistics from the JCS Close Air Support Study Phase II, 11 Oct 1972; hindsight paper on period data; morrow_1977_tacs]
- After 1965, Vietnam's immediate requests were answered within 20 min by airborne aircraft and 40 min by ground alert. "Immediate" meant needed within 3 h or less.
- The 1972 joint study set goals of 50% within 15 min, 75% within 20 min and 100% within 40 min.
- **Korea (1950-53):** the Army/USAF system averaged 40-50 min against the Marines' 10-15 min. **Vietnam before the 1965 changes:** over 90 min, and only about half of requests were met. *(Corrected 15 Nov 2026: the report had these the other way round, the 40-50/10-15 min as Vietnam and the 90 min as Korea. Luke gives them as above.)*
- In Vietnam the DASC could divert airborne-alert aircraft and FACs. Only the TACC could scramble ground alert or divert across corps.
- In the early Vietnam ARVN/VNAF request net, if no disapproval came within 5 min, the ASOC took the request as approved (Morrow). The 1976 TACR 55-46 (TEST) says only "a preset time limit".
- In Italy in 1944, airborne patrols answered requests in 10-30 min. "Rover Joe" controllers were given a flight of four every 30 min.
- *Date:* 1944-72 data, the basis of 1980s doctrine.

**c2-08. Alert categories as a duty officer saw them.** [hoak_1987_retasking, ch. 3-4]
- The ASOC's fighter duty officer chooses among airborne alert, ground alert, or both, by urgency.
- His examples are a mission that only aircraft on 15-minute or shorter alert can meet, and a warning when five-minute alert aircraft drop below a number.
- He may borrow aircraft from an adjacent corps only when his own are short.
- *Date:* 1987, describing the mid-1980s.

**c2-09. The A-10 and the joint air attack team (JAAT).** [tradoc_jaat_1983; mt_1986_dobedin, Soviet view]
- An A-10's normal sortie was about 2 h. Preplanned CAS sorties could be diverted for immediate requests.
- The ASOC or TACC "scrambles, diverts, or requests additional A-10s" and gives the flight leader a point of contact, normally the TACP.
- At check-in the flight leader gives his call sign or mission number, ordnance and time available.
- "The battlefield environment may not permit direct control by the FAC". The FAC may give only the first target brief.
- The first A-10 flight leader in the area coordinates the target handover with later flights. The FAC rides in an Army helicopter. An air battle captain coordinates the whole.
- From the Soviet journal:
  - trials used 4 A-10s, 5 AH-1s and 4 OH-58s;
  - A-10s spent no more than 20 s in the AAA zone, and helicopter attacks lasted 40-45 s;
  - A-10s flew 4-5 sorties a day alone, 2 in a mixed group.
- *Date:* 31 Oct 1983 (TT 17-50-3); Dobedin 1986 on earlier trials.

**c2-10. No airborne FACs in Europe by the mid-1980s.** [skattum_1988_oa10, ch. 2; hoover_1979_fac]
- OV-10 FACs were part of the CAS plan in Europe until the ground-launched cruise missiles' manpower ceilings forced the airborne FAC squadrons out. By 1988 "no forces are in place in Europe".
- The USAF had 60 OV-10s, 73 OA-37s and 29 OT-37s in all.
- At the NTC, FAC aircraft marking targets in a high-threat environment were destroyed "at the rate of two aircraft for every enemy tank destroyed".
- The Air Force relied on ground FACs, sometimes in an Army helicopter. In 1979 FACs were usually flown in an Army light helicopter with an Army pilot, pooled at brigade, and could be diverted to other areas.
- *Date:* withdrawal between about 1984 and 1988; the exact year is open. The OV-10s were at Sembach until about 1984 (search snippet only).
- *Design:* in 1985 NATO's in-flight target control over the battlefield is a ground or helicopter FAC, not an aircraft.

**c2-11. Fast FACs and one pass.** [smith_1979_cas]
- A 1970s concept had the DASC direct deeper strikes "under flight lead control procedures or through ... Fast FAC or SCAR aircraft".
- Interdiction sorties were to be diverted to CAS once first-echelon targets were gone.
- "Most pilots of high performance aircraft consider more than one attack in the target area to be suicidal."
- *Date:* 1979.
- *Design:* in a high threat a flight does not loiter to be retargeted. It attacks once and leaves.

**c2-12. The ground radar control system in 1985.** [smart_1985_tacs, ch. 3]
- The CRC is the senior radar element. It provides "aircraft control or monitoring for both offensive and defensive missions".
- It "scrambles or diverts, with TACC concurrence, air defense capable aircraft".
- The FACP was manual: two operator positions, everything passed by voice to a CRP. It took 59 people and 12 h to mobilise.
- The TPS-43E radar reached beyond 200 NM. A TSQ-91 CRC could take up to 24 h to set up.
- Data links (TADIL A/B) joined the CRC/CRP to the E-3A through the message processing centre.
- The Modular Control Equipment was not expected before mid-1988.
- *Date:* 7 June 1985.

**c2-13. The NATO E-3A.** [afmag_1984_nato; rippe_1985_airland, citing Hansen, International Defense Review 1984; mt_1984_bobrov]
- Eighteen E-3As were planned, all expected operational by mid-1985. They operated from Geilenkirchen from February 1982 (the exact day, 24 Feb, is unverified in the sources cited), and 12 had been delivered by late 1984 (Rippe).
- From over western West Germany an E-3A could detect low-flying aircraft deep in East German and Czechoslovak airspace.
- No period source found gives the NATO E-3A authority to retask strike flights. Its role in the sources is detection, warning and fighter control.
- The Soviet view of Lebanon in 1982: Israeli E-2Cs "guided the strike aviation to ground targets", tracked Syrian fighters as they took off, and committed and controlled the F-15s and F-16s.
- *Date:* 1982-85.
- *Design:* SAM3D's NATO E-3A warns and vectors fighters. Strike guidance by an airborne radar is Israeli practice in 1982 (relevant to the Bekaa scenarios), with a US precedent in Rivet Top's "direction of the strike force" in 1967 (c2-16); no source read gives it to NATO's E-3A in 1985.

**c2-14. The control system could be jammed, and nothing was data-linked to fighters.** [miller_1980_tacs_jamming]
- The European TACS was vulnerable to communications jamming (TACM 2-1, 15 Apr 1978).
- The doctrine of the time held that the Soviets planned to destroy a third of NATO's C3, jam a third and listen to the rest.
- JTIDS was not fielded and its future was still debated.
- *Date:* 1980.

**c2-15. Wild Weasels and passing on found sites.** [checo_1969_tactics; checo_1967_tactics; neitzel_1985_weasel; haseloff_1988_weasel; afmag_1989_slam]
- In Nov 1966 the Ubon tactics symposium recommended that Iron Hand flights be allowed to divert strike aircraft onto an active SAM site. 7th AF replied that policy "does not permit WILD WEASEL blanket authority to divert strike flights".
- A ground-alert Iron Hand force answering ELINT reports (Aug 1965) "did not prove out". Within a week it was fragged for armed reconnaissance.
- Photo reconnaissance "gave the SAM site six to eight hours to relocate".
- The F-4G was introduced in 1978; 116 F-4Es were converted (Neitzel 1985). It worked in two-ship hunter-killer teams, the F-4G finding emitters and attacking them or passing them to the killer; in 1989 the F-4G was paired with F-4Es at George and with F-16Cs at Spangdahlem (afmag_1989_slam). That the killers in Europe in 1985 were F-4Es is plausible but unverified: no 1985 source read names them.
- "A critical factor in this team operation is the effective and reliable transfer of information on the relative position of the threat" (Haseloff 1988). *(Corrected 15 Nov 2026: Haseloff writes this of the Vietnam-era hunter-killer teams, not of 1985 teams.)* That the transfer was by voice is the report's inference (no data link to fighters in the period, c2-14), unverified in a source.
- *Date:* 1965-88.
- *Design:*
  - a Weasel lead does not retask strike packages;
  - he and his killers attack what they find;
  - other flights hear of it by voice warning or after landing;
  - a site reported from photographs is 6-8 h stale.

**c2-16. Airborne radar warnings to strike forces.** [checo_1967_tactics; checo_1969_tactics]
- Big Eye EC-121Ds gave MiG warnings from April 1965.
- Rivet Top (EC-121K/M, first fragged mission 30 Aug 1967) gave SAM and MiG warnings. Its priorities were, in order, "assistance and direction of the strike force", SAM and MiG warning, and EOB search. It won permission for direct communication with the strike force on 6 Oct 1967.
- *Date:* 1965-67. This is the precedent for E-3A warnings in 1985. Note that Rivet Top's first priority was "direction of the strike force": an airborne platform assisting and directing (not retasking) strike flights is a Vietnam precedent, not only an Israeli one (c2-13).

**c2-17. In-flight reports and fast reaction.** [army_it0654_inflightrep; brant_1986_bai_1973]
- Reconnaissance and strike crews (OV-1D, RF-4C, F-4E, F-111F) sent results in flight only if requested. They used secure voice to the requester's TACP or collection manager, in NATO or USMTF format.
- On 6 Oct 1973 Israeli F-4s and A-4s were in action within 26 min of the first reports of the Egyptian crossing.
- *Date:* the Army page is undated (Pave Tack era); 1973.

**c2-18. The Soviet view of US retasking and alert.** [mt_1986_dobedin, pp. 31-40]
- In Vietnam there were frequent shifts of mission. Switching to an alternative target was "usually undesirable and sometimes impossible".
- US tests of low-level airborne alert found a pilot's attention flagging after 30 min.
- Actual reaction times "diverged sharply" from planned ones.
- VSTOL aircraft based up to 50 km from the front cut the modelled request reaction time to 10-11 min, at 8-10 flights a day.
- No more than 2% of air support missions were flown at night in the Near East wars and the Falklands. In the Falklands Harriers flew 2,376 sorties, 282 of them at night.
- *Date:* July 1986, on earlier wars and trials.

### 2.2 Warsaw Pact

**c2-20. The Pact's air support control organisation, as NATO saw it in 1984.** [fm_100_2_1_1984, pp. 12-2 to 12-6]
- An aviation control element at army and at division: an air controller, an intelligence officer, a liaison officer and communications, in two sections, each in a BTR-60.
- A forward air controller was assigned to regiments when air support was allotted. He was an air forces officer, generally a pilot, and rare at battalion.
- Control and target-identification posts, on the ground or in the air, handle aircraft in a designated sector. They can direct aircraft to targets.
- Marking was by radio beacons, flares, smoke and panels. The FAC normally does not mark the target.
- *Date:* 1984.

**c2-21. Planned strikes follow the plan.** [fm_100_2_1_1984, p. 12-4 (checkpoint, flight plan), p. 12-7 (the plan's 1-2 h)]
- The air plan covers the first 1-2 h of an offensive, up to 24 h if static, and holds a reserve.
- On a planned mission the aircraft confirm the target with ground control at a checkpoint. Once the FAC confirms it, the flight leader assigns individual targets.
- "Aircraft follow the original flight plan through friendly antiaircraft defenses unless changed by ground control."
- *Date:* 1984.

**c2-22. Immediate requests and diverts.** [fm_100_2_1_1984, p. 12-5]
- A request goes up the chain. The division commander can order the strike through his aviation control element if it fits within his allocation. Otherwise army or front must approve.
- Aircraft for immediate missions wait in holding areas in the air or on the ground.
- "Occasionally, an aircraft on armed reconnaissance patrol can be diverted to respond to an air support request within its area of operations."
- Before takeoff pilots get a short brief naming a checkpoint and possibly the target. At the checkpoint they call the ground unit's air representative for target designation or confirmation.
- *Date:* 1984.

**c2-23. Readiness categories.** [fm_100_2_1_1984, p. 12-7]

| Category | Crew position | Time to takeoff | Held for |
|---|---|---|---|
| 1 | In the cockpit | 3-5 min | 1-2 h |
| 2 | Near the aircraft | 15 min | 2-4 h |
| 3 | — | 1-2 h | 2-4 days |

Categories 1 and 2 answer requests for immediate support. *Date:* 1984.

**c2-24. Request to strike from airfield alert, in Soviet doctrine.** [mt_1962_kontsevoy, Military Thought 4(65) 1962, CIA translation 15 Oct 1975]
- Fighter-bombers on airfield alert could strike in the tactical and near operational depth within 20 to 30 minutes of a radio request from the air army commander's KP, which sat near the front commander's.
- In a Northern Group of Forces exercise in November 1961, MiG-17s from airfield alert attacked targets in the tactical zone 17 to 28 minutes after the request.
- In the most crucial periods part of the force should be on airborne alert. This is faster but costs forces and radius.
- The best-trained pilots fly free search ("hunting") against nuclear means.
- *Date:* 1961-62. This is out of period and serves as background to the 1984 categories.

**c2-25. Centralised and rigid, with initiative urged from above.** [fm_100_2_1_1984, pp. 12-3 (coordination difficulties), 12-4 and 12-6 (centralised reallocation), 12-9 (the front commander's role); streb_1980_frontal_aviation; olmsted_1977_pvo_training]
- Centralised control lets the front or army reallocate aircraft quickly to sudden important missions, such as nuclear means, aviation or reserves.
- After the initial allocation the front commander plays little part unless large reallocations are needed.
- Lower commanders are often not told what is allotted for immediate missions. A Soviet commentator wrote that the aviators fight by their rules and the combined-arms troops by theirs.
- Streb (1980) found that Soviet press accounts left the lower ground commander restricted to preplanned sorties, with no forward ground-based air controllers and liaison officers who only advise. Some targets of opportunity were attacked along the route on the pilot's own initiative, and the Soviet press urged that a pilot not be bound to the KP's commands.
- Olmsted (1977) found the Soviet press complaining that pilots (here PVO interceptor pilots) depended too much on controllers. Training added simulated radio failures to build initiative.
- *Date:* 1977-84.

**c2-26. Free hunt and armed reconnaissance.** [fm_100_2_1_1984, p. 12-6; mai_tactics_vvs, §3.3.3; mt_1962_pstygo]
- Free hunting is held back until air superiority is won.
- Armed reconnaissance is one reconnaissance aircraft with 2-4 attack aircraft, against targets up to 480 km deep.
- The MAI teaching aid gives four methods: simultaneous, successive, on call (from airfield or airborne alert) and free hunt.
- In 1962, Pstygo, Ganichev and Reshetnikov wrote of the following:
  - free hunting and reconnaissance in force from airfield and airborne alert;
  - search sectors flown by permanent crews;
  - control by the air army's operations groups;
  - one-time command signals from the air army's KP straight to the regiments' KPs.

  They found the request method "considerably increased" the call-up time.
- *Date:* 1962 (background) to 1984.

**c2-27. Low level, little radio, short attack.** [fm_100_2_1_1984, p. 12-8 (approach, identification, radio), p. 12-5 (safety distances)]
- Approach "ideally" at 50-100 m over enemy territory (fixed-wing aircraft).
- 3-6 s to identify the target and clear within 10 s.
- The FAC passes target location, time and ground situation by "brief coded messages and prearranged signals" (p. 12-5).
- Radio use minimal or prohibited.
- Peacetime safety distances from friendly troops were 200-700 m.
- *Date:* 1984.
- *Design:* in-air changes are hard below radar and radio cover. They happen at the checkpoint, inside the ground control post's radio range.

**c2-28. Reports and reconnaissance timing.** [fm_100_2_1_1984, ch. 12; markovsky_afghan_sky]
- Aircrew on any mission report enemy activity they saw.
- Air reconnaissance takes 2-8 h to process.
- In Afghanistan pilots reported to the regiment's HQ within 5-10 min of landing, and it went at once to the VVS KP.
- *Date:* 1980-85.

**c2-29. Afghanistan, 1980-85: the Soviet local-war practice.** [markovsky_afghan_sky; kz_2007_pan, snippet; pchelov_2018_pan]
- Su-25s held airborne-alert zones at 3,000-3,500 m. They flew there on a schedule or on the KP's order, and the infantry could call them at once onto firing points.
- A support attack lasted up to 15-20 min.
- VVS forward air controllers went with units, later with every motor rifle or air assault battalion in an operation. They were drawn from pilots and navigators who had stopped flying, and the enemy hunted them.
- An-26RT relay aircraft hung for hours over strike areas, because aircraft below the ridges fell out of radar and radio cover.
- A pair of Su-25s stood ground alert at Bagram. Crews flew 4-5 sorties a shift.
- A suppression group of 2-6 Su-17s struck first, and the main group followed 1-2 min later.
- MiG-21R crews flew free hunts with drop tanks and two RBK-250-275 or two to four S-24.
- Helicopter safety distances were 1,500 m for bombs, 500 m for rockets and 300 m for machine guns.
- In summer 1984 near Kandahar, an Su-17 leader refused the FAC's guidance and bombed his own battalion: 4 killed, 9 wounded.
- A Krasnaya Zvezda article (2007, search snippet only) gives six ГБУ and 26 ПАН created on deployment, with a recommendation of one ГБУ and three ПАН per division.
- A ПАН carried an R-809M radio of about 28 kg with a 10 kg battery.
- Flying command posts over large operations came only from summer 1986 (section 5).
- *Date:* 1980-85.

**c2-30. Ground support only after the air operation.** [dia_1977_fa_support; cia_1985_air_op]
- Frontal aviation committed to the opening air operation would strike NATO airbases, nuclear means and C2, with suppression of air defences.
- Aircraft would be released to the front commanders only after the air operation. The initial allocation to the fronts would be relatively small and would grow as the war went on.
- *Date:* 1977 (DIA); 1985 (CIA, excerpt only).
- *Design:* in SAM3D's Pact war, on-call strikes against a battery are rare in the first phase and grow later.

**c2-31. The Soviet view of a NATO massed strike.** [mt_1984_bobrov, pp. 37-49]
- The first echelon suppresses air defences in a zone up to 120 km wide.
- *Date:* October 1984.

### 2.3 Neutral

**c2-40. No period source found.** Nothing public was found on in-flight retasking for a 1980s neutral air force. Switzerland's FLORIDA and Austria's Goldhaube systems are open items.

*SAM3D assumption* (not sourced): a small, centralised national air defence system. Fighters are ground-controlled. Ground attack is planned, on ground alert at most. There are no airborne FACs and no in-air retasking beyond recall.

### 2.4 Rebels

**c2-41. No source.** *SAM3D assumption:* a handful of aircraft. Each flies a preplanned or self-chosen sortie, and the leader decides in the air. There is ground radio to their own troops at most. There is no warning system beyond their own eyes and agents. Nothing is retasked in the air.

### 2.5 General

**c2-50. A latency budget built from the parts above.** [c2-03, c2-07, c2-09, c2-22, c2-23, c2-24]
- An immediate request runs: request up the net → approval or silence (about 5 min) → scramble (5-15 min NATO; 3-5 or 15 min Pact) or divert → transit → check-in → FAC brief → one pass.
- The totals the sources give lie between 15 and 50 min. The best case is 10-20 min (airborne alert, or the Marines' Korea-era system of aircraft preassigned to front-line units on air alert). The typical case is 40-50 min (USAF/Army in Korea; Vietnam after 1965 met 20 min from airborne alert and 40 min from ground alert). The Soviet doctrine and exercise give 17-30 min.
- The approval-by-silence step has no sourced number for 1985 (5 min is the early Vietnam figure).
- This is a SAM3D composition, not one source's number.

**c2-51. No number found for the share of sorties retasked in 1980-85.** Hoak (1987) says "considerable" retasking happened in recent conflicts and gives no figure. The question stays open (section 7).

---

## 3 Numbers usable as data

| Quantity | Value | Faction | Date | Source | Confidence |
|---|---|---|---|---|---|
| Immediate CAS goal, airborne alert | 20 min | NATO/US | 1965-72 | luke_2002_cas (citing JCS 1972) | medium |
| Immediate CAS goal, ground alert | 40 min | NATO/US | 1965-72 | luke_2002_cas | medium |
| JCS 1972 response goals | 50% ≤15 min, 75% ≤20 min, 100% ≤40 min | NATO/US | 1972 | luke_2002_cas | medium |
| Achieved immediate response, Army/USAF | 40-50 min average | US | Korea 1950-53 | luke_2002_cas | medium |
| Achieved immediate response, USMC | 10-15 min | US | Korea 1950-53 | luke_2002_cas | medium |
| Achieved immediate response, Vietnam before the 1965 changes | > 90 min; about half met | US | to 1965 | luke_2002_cas | medium |
| "Immediate" defined as | needed within ≤ 3 h | US | Vietnam | luke_2002_cas | medium |
| Silence-is-consent window at the ASOC | 5 min (early Vietnam ARVN/VNAF net); "a preset time limit" in TACR 55-46 TEST, 1976 | US | 1960s; 1976 | morrow_1977_tacs | medium |
| Airborne patrol response (cab rank) | 10-30 min | Allied | 1944 | morrow_1977_tacs | medium |
| Rover Joe allotment | 1 flight of 4 per 30 min | Allied | 1944 | morrow_1977_tacs | medium |
| BAI, division nomination to mission flown | 48 h | NATO/US | 1983 | gallant_knight_1983 | high |
| Division planning horizon | ≤ 24 h | NATO/US | 1983-84 | gallant_knight_1983; rippe_1985_airland | high |
| Corps planning horizon | 72 h | NATO/US | 1984 | rippe_1985_airland | high |
| Apportionment before ATO period | 30-36 h | NATO/US | 1984 | rippe_1985_airland | high |
| Corps told its sorties before ATO period | 30 h | NATO/US | 1984 | rippe_1985_airland | high |
| A-10 normal sortie length | about 2 h | NATO/US | 1983 | tradoc_jaat_1983 | high |
| Ground alert levels named by an ASOC planner | 5 min and 15 min | NATO/US | 1987 | hoak_1987_retasking | medium |
| TOT window assumed for AI | 20 min | NATO/US | 1987 | schoeck_1987_ai_retasking | medium |
| Low-level airborne alert, attention flags after | 30 min | US (Soviet view) | before 1986 | mt_1986_dobedin | medium |
| VSTOL 50 km from front, modelled reaction | 10-11 min; 8-10 flights/day | NATO (Soviet view) | before 1986 | mt_1986_dobedin | low |
| Night share of air support missions | ≤ 2% | general | 1967-82 | mt_1986_dobedin | medium |
| Harrier sorties, Falklands | 2,376 (282 at night) | UK | 1982 | mt_1986_dobedin | medium |
| JAAT: A-10 time in AAA zone | ≤ 20 s | NATO (Soviet view) | before 1986 | mt_1986_dobedin | medium |
| JAAT: helicopter attack | 40-45 s | NATO (Soviet view) | before 1986 | mt_1986_dobedin | medium |
| A-10 sorties per day | 4-5 alone; 2 in JAAT | NATO (Soviet view) | before 1986 | mt_1986_dobedin | low |
| JAAT trial team | 4 A-10, 5 AH-1, 4 OH-58 | US | before 1986 | mt_1986_dobedin | medium |
| IAF reaction to the canal crossing | 26 min | Israel | 6 Oct 1973 | brant_1986_bai_1973 | medium |
| NATO E-3A force | 18 planned; at Geilenkirchen from Feb 1982; 12 by late 1984 | NATO | 1982-84 | afmag_1984_nato; rippe_1985_airland | high |
| TPS-43E range | > 200 NM (> 370 km) | NATO/US | 1985 | smart_1985_tacs | high |
| TSQ-91 CRC set-up time | up to 24 h | NATO/US | 1985 | smart_1985_tacs | high |
| FACP manning and move | 59 people; 12 h to mobilise; 11 C-141 | US | 1985 | smart_1985_tacs | high |
| Modular Control Equipment fielding | not before mid-1988 | US | 1985 | smart_1985_tacs | high |
| USAF FAC aircraft | 60 OV-10, 73 OA-37, 29 OT-37 | US | 1988 | skattum_1988_oa10 | high |
| NATO offensive aircraft, Central Region | about 2,400 | NATO | 1980s | rifenburg_1989_c2 | medium |
| F-4G conversions | 116 F-4E; in service 1978 | US | 1978-85 | neitzel_1985_weasel | high |
| SAM site relocation time after photo reconnaissance | 6-8 h | NVA (US view) | 1965-66 | checo_1967_tactics | medium |
| Pact air plan covers | first 1-2 h (≤ 24 h if static) | Pact | 1984 | fm_100_2_1_1984 p. 12-7 | high |
| Pact readiness 1 | 3-5 min to takeoff, held 1-2 h | Pact | 1984 | fm_100_2_1_1984 p. 12-7 | high |
| Pact readiness 2 | 15 min to takeoff, held 2-4 h | Pact | 1984 | fm_100_2_1_1984 p. 12-7 | high |
| Pact readiness 3 | 1-2 h to takeoff, held 2-4 days | Pact | 1984 | fm_100_2_1_1984 p. 12-7 | high |
| Pact target identification and attack | 3-6 s to identify, clear within 10 s | Pact | 1984 | fm_100_2_1_1984 | medium |
| Pact approach height | 50-100 m | Pact | 1984 | fm_100_2_1_1984 | medium |
| Pact peacetime safety distance | 200-700 m | Pact | 1984 | fm_100_2_1_1984 | medium |
| Pact air reconnaissance processing | 2-8 h | Pact | 1984 | fm_100_2_1_1984 | medium |
| Armed reconnaissance group | 1 reconnaissance + 2-4 attack aircraft; ≤ 480 km deep | Pact | 1984 | fm_100_2_1_1984 | medium |
| Request to strike from airfield alert | 20-30 min | USSR | 1962 | mt_1962_kontsevoy | medium |
| Request to strike, NGF exercise | 17-28 min (MiG-17) | USSR | Nov 1961 | mt_1962_kontsevoy | high |
| Su-25 airborne alert zone | 3,000-3,500 m | USSR | 1980-85 | markovsky_afghan_sky | medium |
| Support attack duration | up to 15-20 min | USSR | 1980-85 | markovsky_afghan_sky | medium |
| Pilot report after landing | 5-10 min | USSR | 1980-85 | markovsky_afghan_sky | medium |
| Sorties per shift | 4-5 | USSR | 1980-85 | markovsky_afghan_sky | medium |
| Suppression group then main group | 2-6 Su-17; main group 1-2 min later | USSR | 1980-85 | markovsky_afghan_sky | medium |
| Helicopter safety distances | 1,500 m bombs; 500 m rockets; 300 m MG | USSR | 1980-85 | markovsky_afghan_sky | medium |
| FAC organisation in Afghanistan | 6 ГБУ, 26 ПАН; 1 ГБУ + 3 ПАН per division recommended | USSR | 1980s | kz_2007_pan (snippet) | low |
| ПАН radio | R-809M about 28 kg + 10 kg battery | USSR | 1980s | pchelov_2018_pan | low |
| NATO first-echelon AD suppression zone | up to 120 km wide | NATO (Soviet view) | 1984 | mt_1984_bobrov | medium |

---

## 4 Disagreements

1. **Was there a Soviet FAC?**
   - Streb (1980), from the Soviet press: no forward ground-based air controllers. The lower commander is held to preplanned sorties.
   - FM 100-2-1 (1984): a FAC at regiment when air is allotted, a pilot.
   - Afghanistan: FACs with every battalion in operations.
   - *Used:* FM 100-2-1 for a Pact war in Europe; Afghanistan practice for local wars and Soviet-advised forces. The difference is probably a real change between the 1970s and the mid-1980s, pushed by Afghanistan.
2. **Who approves a US airborne divert?**
   - TACR 55-46 (TEST, 1976): the Army must approve airborne diverts.
   - J-SAK (1984): the TACC may divert any BAI mission and only inform the Army.
   - TACR 55-46 (1986): the ASOC may divert preplanned sorties when authorised.
   - These may cover different missions (CAS vs BAI). *Used:* a CAS divert needs the corps' agreement; a BAI divert is the ATOC's alone.
3. **NATO's own concepts differed.**
   - J-SAK treated BAI as part of AI and allowed mission-oriented support.
   - NATO's Central Region treated BAI as part of offensive air support to the army group and did not allow mission-oriented support.
   - *Used:* NATO's for the Central Region.
4. **How fast is immediate CAS?**
   - Goals: 15-20 min.
   - Achieved: 40-50 min (USAF/Army, Korea); Marines 10-15 min (Korea). Vietnam after 1965: 20 min airborne alert, 40 min ground alert.
   - *Used:* the achieved figures as typical; the goals only with airborne alert.
5. **How common was retasking?**
   - Dobedin (1986, Soviet view of Vietnam): switching to another target was undesirable and sometimes impossible.
   - Hoak (1987): considerable retasking happened in recent conflicts.
   - Schoeck (1987): airborne AI diverts not yet possible.
   - *Used:* CAS and BAI flights can be retasked in low or medium threat; AI and deep strike are not retasked in the air.
6. **Soviet timing, 1962 vs 1984.**
   - 1962 doctrine: 20-30 min request to strike (1961 exercise: 17-28 min).
   - FM 100-2-1: 3-5 or 15 min to takeoff.
   - These agree if transit and the checkpoint take about 10-15 min. *Used:* both, as parts of one budget.

---

## 5 Post-period items excluded

- **The OA-10A as a FAC aircraft** (conversion 1987-88).
- **F-16Cs as the killers with F-4Gs at Spangdahlem** (in place by June 1989 per `afmag_1989_slam`; the start year, 1987, is unverified: the article gives no date). In 1985 the killers are taken to be F-4Es (unverified, see c2-15).
- **ABCCC "push CAS", kill boxes, "killer scouts"** (1991, Gulf War). Hindsight only. (Luke 2002 applies the label "push-CAS" retrospectively to the Marines' Korea-era air alert system; the 1991 ABCCC practice is still excluded.)
- **Automation of the TACC and ASOC** (decision aids proposed 1987, systems late 1980s and later). Hoak calls the 1987 system manual.
- **Modular Control Equipment** (mid-1988 at the earliest).
- **JSTARS** (first used 1991).
- **JTIDS/Link 16 in fighters** (1990s; not fielded in 1980).
- **Multi-service brevity codes** as one published list (first edition 1997, per ALSA's history). The 1985 brevity words are open.
- **AMRAAM** (1991) and **LANTIRN** (1987).
- **TALD decoys** (1987).
- **An EC-130H Compass Call unit in Europe** (1987).
- **HARM "Magnum" call:** its date was not verified, so it is left out.
- **Soviet flying command posts over large operations in Afghanistan** (summer 1986; `markovsky_afghan_sky`).
- **NATO ACCS communications** (IDA paper P-2570, October 1991). Read only for its scope, not used.
- **Luke (2002)** and **Shoemaker (CGSC, after 1994)** are modern papers. Luke is used only for the 1972 JCS figures he cites.

---

## 6 Sources

Tiers follow `docs/SOURCES.md`. "Whose view" says whose practice the source describes, and through whose eyes.

| Id | Tier | Citation | URL | Read | Whose view |
|---|---|---|---|---|---|
| `fm_100_2_1_1984` (registered) | 1 (for what NATO believed) | US Army FM 100-2-1, *The Soviet Army: Operations and Tactics*, 16 July 1984, ch. 12 | https://archive.org/details/fm100-2-1 | ch. 12 in full | US view of the Pact |
| `checo_1967_tactics` (registered) | 1 | Project CHECO, *Air Tactics Against NVN Air/Ground Defenses*, Feb 1967, ADA586117 | https://archive.org/details/DTIC_ADA586117 | in full | US, own campaign |
| `checo_1969_tactics` (registered) | 1 | Project CHECO, *Air Tactics Against NVN Air/Ground Defenses, Dec 1966 - 1 Nov 1968*, 30 Aug 1969, ADA486448 | https://archive.org/details/DTIC_ADA486448 | in full | US, own campaign |
| `afmag_1984_nato` (registered) | 4 | «NATO on the Upbeat», *Air Force Magazine*, Sep 1984 | https://www.airandspaceforces.com/article/0984nato/ | earlier session | US press |
| `afmag_1989_slam` (registered) | 4 | J. P. Rhodes, «Slam 'Em and Jam 'Em», *Air Force Magazine*, June 1989 | https://www.airandspaceforces.com/article/0689slam/ | earlier session | US press |
| `mai_tactics_vvs` (registered) | 1 (see its notes) | «Тактика военно-воздушных сил», MAI teaching aid, §3.3.3 | https://files.mai.ru/site/unit/institute-of-military-science/tvvs/data/g3/3-3-3.html | earlier session | Russian/Soviet |
| `cia_1985_air_op` (registered) | 1 (for what the CIA believed) | CIA, Warsaw Pact air operations in Central Europe, Jan 1985 | https://www.cia.gov/readingroom/docs/1985-01-01.pdf | excerpt only | US view of the Pact |
| `morrow_1977_tacs` | 2 | Morrow, *Tactical Air Control System: Effects of Army Organizational Changes on Close Air Support Coordination*, CGSC MMAS thesis, 1977, ADA042922 (cites TACR 55-46 TEST, Aug 1976) | https://archive.org/details/DTIC_ADA042922 | in full | US, own system |
| `luke_2002_cas` | 2 (hindsight) | B. K. Luke, *Will Close Air Support be Where Needed and When to Support Objective Force Operations in 2015?*, SAMS monograph, 2002, ADA403678 (its response-time figures come from B. F. Cooling, ed., *Case Studies in the Development of Close Air Support*, 1990, which cites the JCS CAS Study Phase II, 1972; Cooling not read) | https://archive.org/details/DTIC_ADA403678 | in full | US, own system |
| `rippe_1985_airland` | 2 | Rippe, *An Army and Air Force Joint Issue: Principles and Procedures for AirLand Warfare*, CGSC MMAS thesis, 1985, ADA164740 | https://archive.org/details/DTIC_ADA164740 | in full | US and NATO, own system |
| `gallant_knight_1983` | 1 | TRADOC, *After Action Report, JTX Gallant Knight 83*, HQ CAC, 7 Jul 1983, ADA164986 | https://archive.org/details/DTIC_ADA164986 | in full | US, own exercise |
| `tradoc_jaat_1983` | 1 | TRADOC Training Text 17-50-3, *Joint Air Attack Team Operations*, 31 Oct 1983, ADA393157 | https://archive.org/details/DTIC_ADA393157 | in full | US doctrine |
| `hoak_1987_retasking` | 2 | P. W. Hoak, *Adaptive Design of a Decision Support System for Dynamic Retasking of CAS and BAI Assets*, AFIT/GCS/ENG/87D-14, 1987, ADA188860 | https://archive.org/details/DTIC_ADA188860 | in full | US, own system |
| `schoeck_1987_ai_retasking` | 2 | J. A. Schoeck, *Design of a Decision Support System for the Dynamic Retasking of Air Interdiction Assets*, AFIT/GST/ENS/87M-15, June 1987 (sponsor 17th AF, USAFE), ADA185234 | https://archive.org/details/DTIC_ADA185234 | in full | US, own system |
| `rifenburg_1989_c2` | 2 | G. L. Rifenburg, *Command and Control of the NATO Central Region Air Forces*, Air War College, March 1989, ADA217946 (author at HQ 4ATAF 1982-85) | https://archive.org/details/DTIC_ADA217946 | in full | NATO, own system |
| `skattum_1988_oa10` | 2 | Skattum, *The OA-10: How Can We Best Employ it in AirLand Battle?*, SAMS monograph, 1988, ADA210984 | https://archive.org/details/DTIC_ADA210984 | in full | US, own system |
| `hoover_1979_fac` | 2 | D. A. Hoover, *Forward Air Control Today: Will It Work in Europe?*, 8 June 1979, ADA077287 | https://archive.org/details/DTIC_ADA077287 | in full | US, own system |
| `miller_1980_tacs_jamming` | 2 | Miller, *The European Tactical Air Control System versus Communication Jamming*, CGSC MMAS thesis, 1980, ADA094002 | https://archive.org/details/DTIC_ADA094002 | in full | US, own system |
| `smith_1979_cas` | 2 | R. L. Smith, *Close Air Support: Can it Survive the 80s?*, 8 June 1979, ADA077539 | https://archive.org/details/DTIC_ADA077539 | in full | US, own system |
| `smart_1985_tacs` | 2 | B. W. Smart, *The Ground Surveillance and Control Elements of the Tactical Air Control System (TACS) with Modular Control Equipment*, CGSC MMAS thesis, 1985, ADA164775 | https://archive.org/details/DTIC_ADA164775 | searched in full text; ch. 3 read | US, own system |
| `haseloff_1988_weasel` | 2 | Haseloff, *The Requirement for Wild Weasel Defense Suppression Assets in Reducing Aircraft Attrition*, CGSC MMAS thesis, 1988, ADA198044 | https://archive.org/details/DTIC_ADA198044 | in part | US, own system |
| `neitzel_1985_weasel` | 2 | M. J. Neitzel, *What USAF Aircraft Should Be the Wild Weasel of the 1990's?*, CGSC MMAS thesis, 7 June 1985, ADA164727 | https://archive.org/details/DTIC_ADA164727 | in part | US, own system |
| `brant_1986_bai_1973` | 2 | Brant, *Battlefield Air Interdiction in the 1973 Middle East War and Its Significance to NATO Air Operations*, CGSC MMAS thesis, 1986, ADA186417 | https://archive.org/details/DTIC_ADA186417 | in part | US view of Israel |
| `army_it0654_inflightrep` | 1 (undated) | US Army Intelligence correspondence course IT0654, the in-flight report page | https://armyintelligence.tpub.com/IT0654/IT06540029.htm | the page only | US, own system |
| `streb_1980_frontal_aviation` | 2 (adversary's view, from the Soviet press) | G. Streb, *Soviet Frontal Aviation*, US Army Russian Institute, June 1980, ADA098505 | https://archive.org/details/DTIC_ADA098505 | in full | US view of the USSR |
| `olmsted_1977_pvo_training` | 2 (adversary's view, from the Soviet press) | W. S. Olmsted, *Current Aspects of the Combat Training of Soviet National Air Defense Troops*, US Army Institute for Advanced Russian and East European Studies, 1977, ADA039627 | https://archive.org/details/DTIC_ADA039627 | in part | US view of the USSR |
| `dia_1977_fa_support` | 1 (for what the DIA believed) | DIA, *Soviet Frontal Aviation Support to Ground Forces Operations*, August 1977, CIA-RDP83M00171R001100080001-7 (sanitised) | https://archive.org/details/cia-readingroom-document-cia-rdp83m00171r001100080001-7 | in full (heavily redacted) | US view of the USSR |
| `mt_1962_kontsevoy` | 1 | Kontsevoy, «Air Support of the Ground Forces», Military Thought 4(65), 1962; CIA translation 15 Oct 1975, CIA-RDP10-00105R000201380001-2 | https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000201380001-2 | in full | Soviet |
| `mt_1962_pstygo` | 1 | Pstygo, Ganichev, Reshetnikov, on air support of ground forces, Military Thought 5(66), 1962; CIA translation 12 Oct 1976, CIA-RDP10-00105R000302430001-4 | https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000302430001-4 | in full | Soviet |
| `mt_1984_bobrov` | 1 | D. V. Bobrov, «Massed Aviation Strikes (Historical Experience and the Present)», Military Thought No. 10, Oct 1984, pp. 37-49; CIA-RDP10-00105R000403340001-2 | https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000403340001-2 | in full | Soviet view of NATO and Israel |
| `mt_1986_dobedin` | 1 | Dobedin, on US air support of ground forces, Military Thought No. 7, July 1986, pp. 31-40; CIA-RDP10-00105R000403380001-8 | https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000403380001-8 | in full | Soviet view of the US |
| `markovsky_afghan_sky` | 4 | В. Марковский, «Жаркое небо Афганистана» | https://lib.ru/MEMUARY/AFGAN/nebo.txt_with-big-pictures.html | in part (the chapters on attack aviation and control) | Russian historian, from veterans |
| `kz_2007_pan` | 4 | «Красная звезда», 2007, on aviation controllers in Afghanistan | not opened; seen as a search excerpt (a forum repost at bagerovo.borda.ru refused access) | snippet only | Russian press |
| `pchelov_2018_pan` | 4 | O. Pchelov, LiveJournal post on ПАН in Afghanistan, 2018 | https://olegpchelov.livejournal.com/132674.html | in full | Russian, modern blog |

Period documents (by 1988): every source above except `luke_2002_cas`, `markovsky_afghan_sky` (1990s-2000s), `kz_2007_pan`, `pchelov_2018_pan`, `rifenburg_1989_c2` (March 1989, just past) and `afmag_1989_slam` (June 1989). The 1962 Military Thought articles are period documents but describe an earlier period.

Downloaded and not used: Combest, *Apportionment and Tactical Airpower in AirLand Battle* (CGSC 1987, ADA185546; nothing on latency); IDA P-2570 (1991; post-period).

---

## 7 Open items

- **The NATO E-3A's wartime role in 1985 in a primary source.** Did it control or warn strike packages, or only fighters? The awacs.nato.int and NAEW brochures were refused or empty.
- **ABCCC in Europe in 1985.** Was an EC-130E ABCCC deployed or planned for the Central Region? The AFHRA factsheet was refused.
- **Doctrine texts to acquire:**
  - ATP-27(B) (NATO offensive air support);
  - TACR 55-46 (TEST 1976 and 1986);
  - TACM 2-1 (1978);
  - TRADOC Pam 525-45 (J-SAK) in full;
  - McCaffrey's BAI paper (Air University refused);
  - `jsead_1982` in full.
- **A Soviet period manual on ПУА/ГБУ and the авианаводчик**, for example a Боевой устав of fighter-bomber aviation or a frontal aviation textbook of the 1980s. Also the 2007 Krasnaya Zvezda article in full.
- **The CIA's January 1985 paper on the Pact air operation**, in full.
- **The brevity words of 1985** (the MCM 3-1 of the time is classified). Partly answered on verification: TRADOC TT 17-50-3 (31 Oct 1983), glossary p. 49-50, defines for US CAS and JAAT use: Bandit (enemy aircraft), Bingo (minimum fuel to return or to an alternate), Blind (friendly not in sight), No Joy (enemy/target not in sight), Tally-ho (enemy/target in sight), Visual (friendly in sight), Winchester (out of ordnance), and names Have Quick (frequency-hopping UHF) and Vinson (secure FM voice). Those words may be used for 1985 US/NATO voice; anything else stays plain speech until sourced.
- **When the OV-10s left Sembach.** The search snippet says about 1984; it needs a primary source.
- **Latency of British and German CAS in the Central Region** (RAF Harrier, Luftwaffe Alpha Jet), and of Harrier CAS in the Falklands.
- **The neutral:** Switzerland's FLORIDA and Austria's Goldhaube, for how a neutral controlled its fighters and whether it retasked anything in the air.
- **Hansen, International Defense Review 1984,** on the E-3A's low-level coverage over the Pact.
- **The share of sorties retasked in flight, 1980-85.** No number was found. The only figures seen are Gulf War hindsight, which does not belong in this period's data.

---

## Verification (15 Nov 2026)

An adversarial check of this report. The cited texts were fetched again and searched: the archive.org OCR full texts of FM 100-2-1, the DTIC theses and documents (ADA164986, ADA164740, ADA042922, ADA403678, ADA188860, ADA185234, ADA217946, ADA210984, ADA164775, ADA164727, ADA198044, ADA393157, ADA586117, ADA486448), the CIA translations of Kontsevoy (1962) and Dobedin (1986), Markovsky's text on lib.ru, and the two Air Force Magazine articles (read through a fetch summary). Verdicts: confirmed, corrected, unsupported.

| Finding | Verdict | Note |
|---|---|---|
| c2-01 | confirmed | Rippe fig. 3-5 and pp. 31-33 (ACOC "essentially NATO's version of the TACC"; ATOC tactical control; "ACOC plans the war. The ATOC executes it."); Rifenburg: AAFCE re-formed June 1974, "costly, time consuming, and inefficient", SOC III/ATOC Sembach collocation begun Oct 1983 but finished only in the late 1980s. |
| c2-02 | confirmed | Rippe pp. 33-35: BAI apportioned to the army group as OAS; the RIPL; Central European procedures "do not allow for tactical air support on a MISSION ORIENTED BASIS". |
| c2-03 | corrected | Morrow quotes TACR 55-46 (TEST) 1976: "a preset time limit" and "Airborne diverts must be approved by the Army". The 5 min is the early Vietnam ARVN/VNAF net, not 1976 doctrine; the data table and the request model are changed. J-SAK "authority to divert any BAI mission" and Hoak's 1986 TACR quote confirmed. |
| c2-04 | corrected | Hoak's quotes confirmed ("not currently recognized", "considerable retasking", ABCCC "occasionally serves as an airborne ASOC", TAF SON 1987). "Dated from the 1960s" not found; marked unverified. |
| c2-05 | corrected | Schoeck confirmed ("change rapidly and repeatedly", "Someday airborne diverts", triggers, 20 min TOT windows). But he also has the FIDO redirecting airborne AI missions before the IP and diverting recoveries; "not retasked in the air" softened to "hardly". The "reconfigure" quote was not found; marked unverified. |
| c2-06 | confirmed | Gallant Knight 83 obs. GK 83-6: 48 h, division horizons no more than 24 h, revalidation 8 h and 4 h before; Rippe: corps horizon 72 h, apportionment 30-36 h, corps told 30 h before. |
| c2-07 | corrected | Luke (p. 18-19) gives 10-15 min (Marines) against 40-50 min (Army/USAF) for **Korea**; over 90 min and half met is **Vietnam before 1965**. The report had them swapped. The 20/40 min and the 1972 50/75/100% goals confirmed (Luke via Cooling 1990, hindsight). |
| c2-09 | confirmed | TT 17-50-3, 31 Oct 1983: sortie about two hours, ASOC/TACC "scrambles, diverts, or requests", check-in of call sign, ordnance, time available, "may not permit direct control by the FAC", first A-10 leader coordinates handover. Dobedin: 4 A-10, 5 AH-1G, 4 "AH-58" (OH-58), ≤ 20 s, 40-45 s, 4-5 vs 2 sorties a day. |
| c2-10 | confirmed | Skattum: GLCM manpower ceilings, "no forces are in place in Europe", 60 OV-10/73 OA-37/29 OT-37; NTC loss rate added (two FAC aircraft per enemy tank). |
| c2-12 | confirmed | Smart: CRC "scrambles or diverts, with TACC concurrence, air defense capable aircraft"; FACP 59 people, 12 h, 11 C-141; > 200 NM; up to 24 h to set up; TADIL A/B; MCE mid-1988. |
| c2-13 | corrected | 12 of 18 by late 1984 and the Hansen quote confirmed in Rippe; Air Force Magazine (Sep 1984) gives Geilenkirchen "since February 1982", not the day: 24 Feb is unverified. |
| c2-15 | corrected | CHECO quotes confirmed (7th AF "blanket authority", Iron Hand ground alert "did not prove out", photo reconnaissance six to eight hours); F-4G 1978 and 116 F-4Es confirmed (Neitzel). Haseloff's "critical factor" sentence is about Vietnam-era hunter-killer teams, not 1985; F-4E killers in Europe in 1985 and voice transfer marked unverified. |
| c2-16 | corrected | Confirmed (Rivet Top 30 Aug 1967, direct communication 6 Oct 1967); added that its first priority was "assistance and direction of the strike force". |
| c2-18 | confirmed | Dobedin: "undesirable and sometimes impossible", 30 min, "diverged sharply", VSTOL 50 km, 10-11 min (modelling), 8-10 flights a day, ≤ 2% at night, 2,376 sorties with 282 at night. Soviet view of Western press. |
| c2-20 | confirmed | FM 100-2-1 pp. 12-2 to 12-6, US view of the Pact. |
| c2-21 | corrected | Confirmed; the 1-2 h plan is on p. 12-7, not p. 12-3. |
| c2-22 | confirmed | FM 100-2-1 p. 12-5, quote exact. |
| c2-23 | confirmed | FM 100-2-1 p. 12-7 chart (fighter-bomber aircraft; "probably also applicable" to other ground attack). |
| c2-24 | confirmed | Kontsevoy 1962: 20 to 30 min from the air army commander's KP; NGF Nov 1961 MiG-17s 17 to 28 min; airborne alert costly; "hunting" by the most trained fliers. |
| c2-25 | corrected | Content confirmed; page references corrected (12-3, 12-4, 12-6, 12-9). Streb and Olmsted not re-read. |
| c2-26 | corrected | FM 100-2-1 content confirmed, but on p. 12-6, not 12-8 to 12-9. MAI aid and Pstygo not re-read. |
| c2-27 | corrected | Confirmed; approach, 3-6 s and radio are on p. 12-8, safety distances on p. 12-5. |
| c2-29 | confirmed | Markovsky: 3,000-3,500 m zone by schedule or KP, attack up to 15-20 min, report 5-10 min after landing, suppression group 2-6 Su-17 with the main group 1-2 min later, An-26RT relays, a Bagram ground-alert pair (from spring 1985), Kandahar summer 1984 (4 killed, 9 wounded), helicopter limits 1,500/500/300 m. kz_2007_pan and pchelov not re-checked. |
| c2-50 | corrected | Follows from the c2-03 and c2-07 corrections: no sourced 1985 silence-window number; the 40-50 min typical case is Korea. |

**Period scan.** No post-1985 equipment or practice is presented as 1985 practice after the corrections. Notes: the F-16C killer date (1987) in section 5 is unverified; "push-CAS" appears in Luke as a retrospective label for the Marines' Korea system (the 1991 ABCCC practice stays excluded); the 1983 JAAT glossary gives a short list of period brevity words (section 7).

**Tiers.** `luke_2002_cas` is hindsight and its figures are third-hand (Luke ← Cooling 1990 ← JCS 1972): keep at medium confidence and never as the sole basis. `mt_1986_dobedin` is a Soviet officer's compilation of Western press: tier 1 for the Soviet view, not for US practice. `fm_100_2_1_1984` and the DIA and CIA papers are the adversary's view, already labelled.
