# Mission routing and penetration, 1980-85: research (November 2026)

Why: SAM3D's enemy air command (`simcore::airwar`, `docs/air_war/04_PLANNING.md`, `10_WEAPONS.md`) flies a raid
entry, offset IP, target, egress turn, exit, and counts the expected losses per believed threat ring along the way.
That is direct routing. The project owner wants routes flown as the period flew them, and asks that everything be
consistent with the Cold War, not with later tactics. This note gathers what public period sources say about how
routes were built around known air defences, how they were navigated, at what heights, speeds and times, how several
axes were kept apart, and how crews changed route, left the target and came home. The war is 1980-85 and the air
war's year is 1985. Anything later is listed in section 5 only. Read 14-15 November 2026.

Whose view: FM 100-2-1, the CIA's papers and Hines give what the United States believed about the Soviet side. Babich,
Bobrov and the other «Военная мысль» articles give what Soviet officers believed about NATO and about local wars. Each
finding says which.

## 1. Summary for SAM3D's design

1. **A period route is a planned line of turn points on features, not a straight line.** US navigation doctrine put
   turn points over or close to identifiable features, kept the route as direct as possible to cut time in reach of
   the defences, computed a procedure turn at each, and began the attack run at an IP, a fixed point (`afm_51_40_1973`
   ch. 26 and glossary). Canadian CF-104s in Germany in the tactical air support role (from about 1972) flew a route
   preplanned on the forecast weather and the known threats such as SAM sites; a new map for each route was the
   earlier reconnaissance role's practice (`agard_cp267_1979` paper 23). The planner should place turn points on
   map features (river bends, road junctions, villages, hilltops, lake ends) and not on bare grid points.
2. **Routes were chosen to minimise exposure to the defences the planners believed in, not to avoid them
   entirely.** Both sides wrote this. A Soviet analyst of local wars said a clean bypass works "only in map exercises"
   and that a route of minimum air defence activity is all one can count on, and that it always ends inside the
   target's own firing zone (`mt_1984_babich`). A US Army model chose a helicopter force's route as minimum exposure
   over the lowest terrain (`dorsett_1980`), and RAND found that choosing the path could cut a site's intercepts by up to a factor of three,
   and that exposure kept to 20 s or less badly degrades mobile defences (`crawford_1977_rand`). SAM3D's planner
   should search a path through the believed threat picture that weights time in each believed ring's line of sight,
   prefers low ground, and accepts the target's own defences on the last leg.
3. **NATO crossed the front through gates and corridors at set times.** US doctrine set Minimum Risk Routes through
   the friendly area for fast low fliers, built from the known enemy and friendly air defence (`fm_100_42_1976`
   §4-6d); the Air Force asked the corps to suppress the enemy's air defence along a mission's route and time where it
   crossed the FLOT (`jsead_1982` ch. 4); at Red Flag (from 1977) 60-70 aircraft entered the target areas through "a
   series of corridors" within 20-30 minutes, deconflicted by precise timings, with no radio except a fighter warning,
   which an RAF Buccaneer leader called similar to the system for crossing the inner-German border in war
   (`rafhs_j20_1999`). Returning
   aircraft came back under established air defence procedures (`fm_100_42_1976`). SAM3D should give each NATO package
   a FLOT gate on an MRR with a time window, a suppression window on that gate, and a different gate or the same MRR
   home.
4. **NATO heights and speeds by phase (1979-85).** Peacetime training floors were 250 ft in Britain (100 ft in three
   areas from 1979); in West Germany the normal band was 150-450 m, with seven "250 ft" (75 m) areas, and nothing
   below 75 m over the FRG (below that only abroad, Goose Bay from 1981) (`hansard_lowflying`, `btd_10_2980_1985`,
   `btd_11_558_1987`). Training profiles were 500 ft at 420 kt for the CF-104 in tactical air support (its earlier
   nuclear strike role, to about 1972, flew 500 ft at 450 kt with the last leg at 50 ft and 550 kt); 400 ft at 420-480 kt for
   the FB-111 (1000 ft at night); 500 ft day and 800 ft night at 325-350 kt for the B-52; under 100 ft at 200-300 kt
   for the A-10 (`agard_cp267_1979`). RAF crews trained in four-ships at 100 ft; Vulcans at 500 ft (300 ft for the
   experienced) at 250 kt, 350 kt to attack (`rafhs_j20_1999`). F-111F crews used TFR down to 200 ft and preferred 540
   kt, never under 480 kt (`crandall_twz`). The general US rule was 200-500 ft AGL (`afm_51_40_1973`). War heights
   were expected to be lower than training heights (`agard_cp267_1979` paper 4).
5. **The Pact flew a rigid plan through its own air defence and low over the enemy.** In NATO's 1984 view a Soviet
   strike was planned on large-scale maps and terrain models, went to a checkpoint behind its own lines, confirmed the
   target with ground control, passed its own air defence by strict time and path (which the US authors read as
   "probable" safe corridors; their airspace division is labelled hypothetical), then approached at
   50-100 m with radio silence, decoy flights ahead, 3-6 s to find the target and 10 s to clear it
   (`fm_100_2_1_1984` pp. 11-11, 12-3, 12-4, 12-8). It kept to the plan unless ground control changed it. A Soviet
   open-press author wanted a group 1-2 minutes ahead to "blast a corridor" when the route could not avoid the defence
   (`hines_1981`, after Tomilin 1980). The Soviet rules of 1985 class 0-200 m as extremely low (`opp_85` §45).
6. **Helicopters.** US attack helicopters flew low level in the rear, contour further forward, and NOE near the enemy,
   weaving along a broad corridor of terrain features from a holding area to a battle position, NOE seldom at night
   (`fm_17_50_1977`). In NATO's view the Soviets preferred to use helicopters in ground support only to the range of
   their own air defence umbrella, and, when helicopters and artillery worked at the same time, gave them entrance and
   exit corridors between artillery concentrations (`fm_100_2_1_1984`). In Afghanistan, against guns,
   Mi-24s searched at 1500-1700 m (`airwar_mi24_markovsky`): that is the rebel war's pattern, not the European one.
7. **Deviation rules differ by faction.** NATO leaders could choose whether to use an MRR and had a free choice of
   attack mode, direction, speed and release height (`fm_100_42_1976`, `rafhs_j20_1999`). Soviet groups kept to the
   plan unless ground control changed it (`fm_100_2_1_1984` p. 12-4). An Arab, Soviet-trained air arm (Iraq) kept to
   rigid profiles under tight GCI and piled onto easy targets (`cia_1987_iraq_af`).
8. **No public period source gives leg lengths, turn-point counts or a fixed stand-off from a SAM ring.** Those stay
   game values, labelled as such. What the sources do give is a rule (minimise time in a believed site's view, keep
   continuous exposure under about 20-35 s, fly the lowest ground, cross the front at a gate and time) and the heights,
   speeds and timings in section 3.
9. **The neutral and the rebel air arms have no period routing source here.** Their routes are design inferences:
   the neutral flies short, simple low-level routes inside its own airspace; the rebels fly at night, low, by roads
   and rivers, and avoid known guns.

What this means for the code, in order of value:

- Build the route from the believed threat picture (`IntelPicture`), never the truth, as now.
- Add a **let-down point** in own territory, a **FLOT gate** (NATO: MRR, corridor suppression window; Pact: checkpoint
  with GCI and a safe corridor through own air defence), **turn points on map features** chosen by an exposure-weighted
  search over the terrain, an **IP on a feature**, the **target**, an **egress turn** and a **return route** that is not
  the ingress line (NATO: back to an MRR gate; Pact: the planned departure route through its own corridor).
- Keep each package on its **time over target window**; packages on several axes are deconflicted by time and gate,
  not by height alone.
- Give heights and speeds per **faction, type and phase** from section 3, with war heights at or below training
  heights.
- Give deviation rules per faction: NATO leader may re-route round a newly seen threat; Pact holds the plan unless GCI
  changes it; a rigid air arm holds the profile; rebels improvise.

## 2. Findings by faction

### NATO (USAFE, RAF Germany, Luftwaffe, 2ATAF and 4ATAF)

**routing-01. Minimum Risk Routes through the friendly area (1976, in force through the period).** MRRs are temporary
routes recommended for Air Force use that present the minimum known hazards to low fliers transiting the tactical
operations area. The airspace control centre sets them up for fast aircraft transiting low, on the corps'
recommendations. Their parameters include friendly air defence and indirect-fire unit locations, ground manoeuvre and
fire support plans, terrain, the enemy's known indirect and air defence fire, and areas of heavy Army aviation activity.
The aircrew decide whether to use one, weighing its suitability, the risk they accept and the control and reporting
centre's advice. CAS sorties do not normally use them. Corps recommends that they be updated, changed or cancelled as
the situation changes. [`fm_100_42_1976` §4-6d and glossary A-2] US doctrine, US view.

**routing-02. Coming home under air defence procedures (1976).** Aircraft returning through the tactical operations
area from enemy territory follow established air defence procedures. Other means of keeping aircraft apart are the
coordinating altitude, the high-density airspace control zone (HIDACZ), restricted areas and standard-use Army aircraft
routes in the rear. Most Army aircraft fly low under procedural control in VMC; much of tactical air is under radar
advisory control. [`fm_100_42_1976` §4-6a, §4-6d(5), §4-6e (standard-use Army aircraft routes)]

**routing-03. Corridor suppression where a mission crosses the FLOT (1982).** The Air Force conducts or asks the Army
for suppression of enemy air defences for its missions transiting the FLOT ("corridor suppression"). The TACC tells the
corps the mission's route and time and passes the target type, location if known, effects required (usually
disruptive), suppression time period and friendly aircraft information. Air Force control elements pass any deviation
of the mission to the corps at once. The joint SEAD plan also sets airspace controls (corridors, altitudes) to prevent
interference. [`jsead_1982` ch. 3 (campaign plan) and ch. 4 (Air Force-initiated J-SEAD, fig. 4-2)] US doctrine.

**routing-04. Low-level navigation by turn points on features and an IP (1973, in force to 1983).** Low level is flown
for surprise, to avoid detection and interception and to reduce the effect of enemy defences. Turn points should be
over or close to identifiable points (land and water contrast, good radar return). The route should be as direct as
possible to save time and fuel and cut the time in reach of the defences. A procedure turn is worked out for each turn
point. Normal low-level combat heights are 200-500 ft AGL. The IP is "a preselected geographical position which is
used as a reference for the beginning of a run on a target". [`afm_51_40_1973` (1 July 1973) ch. 26 "Low Level
Navigation", p. 26-1; glossary, "Initial point"] US doctrine.

**routing-05. CF-104 routes in Germany, planned on weather and known threats (paper of 1979).** Dating (checked
15 Nov 2026): the paper says the CF-104 flew nine years in the strike/reconnaissance role and "more recently" six
years in tactical air support, and describes the strike and reconnaissance missions in the past tense. The strike and
reconnaissance profiles below are therefore of about 1963-72 (pre-period lineage); only the tactical air support
profile is current practice for 1979-85. A strike training route was
preplanned on the forecast weather and flown at 500 ft and 450 kt (800 ft in the bird season, April-October), the last
leg at 50 ft AGL and 550 kt to the simulated drop; in bad weather the pilot could climb to 2000 ft AGL and finish on
ground-mapping radar. Reconnaissance was flown at 500 ft and 420 kt, a new map for each route. Tactical air support
was planned at 500 ft and 420 kt on a route preplanned on the forecast weather and known threats such as SAM sites;
with a FAC, the pilot flew a preplanned route to an area, took a map reference, and at 400 kt and more at 500-1000 ft
worked out his run and his escape route on a 1:250,000 map. [`agard_cp267_1979` paper 23 (Rud and Leben, DCIEM),
pp. 23-1 to 23-2] Canadian training practice.

**routing-06. US low-level profiles: B-52, FB-111, A-10 (1979).** B-52 training: 500 ft by day, 800 ft at night over
flat ground, 325-350 kt, low-level legs of 1 to 2.5 hours, hazards avoided by waypoint navigation. FB-111: typically
400 ft at 0.63-0.73 Mach (420-480 kt), about 1000 ft at night, automatic terrain following. A-10: often under 100 ft
at 200-300 kt, 40-60% of a tactical mission under 500 ft, missions of 1.5-2 hours. The paper warns that in war the
permitted heights may well be lower and speeds higher. [`agard_cp267_1979` paper 4, pp. 4-2 to 4-3] US view of its
own training.

**routing-07. Very low penetration as the answer to Pact defences, and the value of path choice (1977).** Judging
that penetrating Pact-type defences again at medium height would be very costly, the US emphasis was on very low penetration (200 ft or less) of heavily defended areas to avoid GCI,
acquisition and guidance radars. Choosing the path past a missile site cut the number of intercepts by up to a factor
of three in one example; if terrain could hold exposure to 20 s or less, mobile defences would be badly degraded.
Crews got very little training below 500 ft. [`crawford_1977_rand` p. 2, pp. 7-8 (Figs. 4-6), pp. 20-21]
US (RAND) view.

**routing-08. Route selection as minimum exposure through defended terrain (1980).** A US Army Missile Command
model picked a minimum-exposure, minimum-elevation route from terrain high and low points, line of sight to each
sensor and exposure penalties. Its scenario is a **helicopter force** flying terrain following across the FEBA through
a heavily defended 35 x 35 km area, the sensors having good low-altitude coverage to 10 km; it is an air defence war
gaming tool, not fixed-wing doctrine. Low fliers ordinarily fly within 200 m of the terrain. Points within 90 degrees of the heading to the goal
weigh 1, points behind weigh 2; inside 1 km of the goal the window narrows to 45 degrees. A cone to each radar, its base
twice the kill radius, makes points ahead of the kill zone weigh 2 and beyond it 10. The best route generally follows
the lowest terrain, and the defender covers the low corridors. On the test case the model's route came close to the
preferred of three manually planned routes (route exposure 0.888 against 0.844; the other manual routes 1.325 and
1.582); the claim that it did so "at far less time" is (unverified): not found in the text. Small changes in one
node's weight gave an entirely different route. [`dorsett_1980` ch. III-IV, §4.4-4.7, §7.2-7.4, §8.1] US Army study.

**routing-09. RAF packages at Red Flag: corridors, timings, silence (from 1977).** Corrected 15 Nov 2026: this is
Pitchfork's account of **Red Flag at Nellis** with 208 Squadron's Buccaneers (UK-based), not of RAF Germany's own
planning. The FRAG (the equivalent of the NATO air tasking message) came in the afternoon for the next day; crews
planned to coordinate and deconflict with the rest of a package. Frequently 60-70 aircraft entered the target areas
through a series of corridors within 20-30 minutes; entering the range "through a series of corridors" was, he says,
"a similar procedure to the system we would have used to cross the Inner-German border". No radio calls were
required; his formations were very loose twos or fours, with no R/T except to call a fighter threat, and precise
timings so that following aircraft were safe. Crews had a free choice of attack mode, direction, speed and release
height (a range freedom, with no range safety officer). Red Flag work-ups ended with four-ships at 100 ft against
fighters; a 100 ft area over northern Scotland was used because the Germany-based Buccaneer and Jaguar squadrons
could not train adequately in German airspace. [`rafhs_j20_1999`, G. Pitchfork] Veteran's account.

**routing-10. Vulcan low level (to 1982).** About an hour at low level, mostly 250 kt, accelerating to 350 kt to
attack; most crews at 500 ft AGL, experienced crews at 300 ft. At night, contour flying at 1000 ft AGL on TFR at Red
Flag 79/2, 800 ft at Goose Bay. The rule: fly low, on track and on time. [`rafhs_j20_1999`] Veterans' account.

**routing-11. Tornado GR1 in RAF Germany (mid-1980s; years not given).** Low level down to 100 ft in places; TFR at
500 ft and 420 kt in cloud; loft at 480 kt and 150 ft; inertial navigation corrected by radar fixes on surveyed
fixpoints; a typical low-level sortie of 1 h 45 min. [`napier_hushkit_2022`] Veteran's account; date uncertain.

**routing-12. F-111F at Lakenheath (early-mid 1980s).** Dating: no years are given; the author flew with a WSO
killed in El Dorado Canyon (April 1986), so his tour was before or around 1986. TFR down to 200 ft AGL; manual ("raw
data") terrain following about 500 ft, practised by day in VFR only as a combat-emergency technique; 540 kt preferred, never under 480 kt; about 0.95 Mach on the deck; stopwatch timing; a minimum en-route
altitude planned for each leg; one pass on the target. [`crandall_twz`] Veteran's account.

**routing-13. Peacetime floors and why NATO flew low (1979-87).** Britain: since the revised system of 1979 all of
Great Britain open to low flying down to 250 ft, 100 ft only in three thinly peopled areas; no foreign air force below
250 ft (the "no foreign air force" clause was not found in the 1989 answer: (unverified)). West Germany (corrected
15 Nov 2026): the normal low-flying band was 150-450 m; seven "Tieffluggebiete 250 Fuß" (75 m floor), e.g. "Area 7"
at the Hesselberg in 1985, where Bundeswehr jets went below 500 ft only when the exercise target required it; in the
government's own usage since 1981 "Tiefstflug" means below 250 ft (75 m), which was not flown over the FRG at all but
only abroad (Goose Bay from 1981, down to 100 ft). The "Luftwaffe floor 150 m" is the premise of the 1985 question,
not the government's statement. The FRG government's reason (1987): low flight reduces the effect of the Pact's
ground-based air defences, and low flight with tactical and EW measures keeps the power to penetrate both ground and
airborne defences; its sentence that the radar and semi-active missiles of the look-down systems lose much effect
against very low fliers names FULCRUM, FOXHOUND and FLANKER in the GSFG as of 1987, so that part is post-period
(section 5).
[`hansard_lowflying` (HC Deb 30 Oct 1987 c482W; 29 Jun 1989 c532W); `btd_10_2980_1985`; `btd_11_558_1987`] Government
statements.

**routing-14. Attack helicopter terrain flight (1977).** Three modes. Low level: a preselected, generally straight
route at constant speed and height, where there is no danger of radar acquisition. Contour: constant airspeed, varying
height, usually forward of the division rear boundary. NOE: a broad corridor planned on terrain features along an axis
to the objective and flown as a weaving, slalom-like route; needed forward of a committed brigade's rear boundary and
near a known enemy; generally not feasible at night, when contour or low level is flown. Areas: assembly (out of
medium artillery range), holding, attack routes chosen to avoid detection using cover and features, battle positions.
The scan reads "remain unmasked for 35 ... seconds or less" to degrade the ZSU-23-4 (garbled). The enemy positions air
defence to ambush helicopters flying NOE. [`fm_17_50_1977` ch. 3] US doctrine.

**routing-15. Packages varied and decentralised routes cut losses (1972).** The Linebacker II study found B-52
operations tightly run from SAC headquarters, predictable and inflexible; losses after three days were unacceptable;
decentralised control and varied routes cut them. [`dtic_linebacker`, search excerpt only] Pre-period lesson, low
confidence on detail.

### Warsaw Pact frontal aviation

**routing-16. Soviet height classes and low-level rules (1985).** Extremely low (ПМВ) 0-200 m above the ground or
water; low 200-1000 m; medium 1000-4000 m; high 4000-12000 m; stratosphere above 12000 m. At ПМВ the commander must
know the terrain, obstacles and the descent and climb legs, and allow for short radio and navaid range and higher fuel
burn. Heights, weather and duration at ПМВ are set by НПП and КБП (not public). Terrain: flat under 200 m relief,
hilly 200-500 m, mountainous 500 m and more within 25 km, or 1000 m and more above sea level. [`opp_85` §45(7),
§159, §160; definitions «Местность равнинная / холмистая / горная»] Soviet regulation, in force 1
Dec 1985 (replacing ОПП-77).

**routing-17. The Soviet strike route as NATO saw it: approach (1984).** A low, fast approach; "Ideally, the approach
altitude over enemy territory is 50 to 100 meters"; 3-6 s to identify the target; clear the target area within 10 s;
radio minimal or forbidden; radar gaps exploited; decoy flights ahead; re-attacks from other directions or out of the
sun. [`fm_100_2_1_1984` p. 12-8] NATO's view.

**routing-18. Planning, checkpoint and ground control (1984).** The aviation commander's orders give targets,
sorties, air approach corridors, codes and timing. Large-scale maps and terrain models are used to choose the best
flight path. Airborne aircraft go to a checkpoint behind friendly lines and confirm the target with ground control;
the emphasis on strict timing and flight paths "indicates the probable use" of safe corridors through friendly air
defence (an inference by the US authors, not an observed fact); aircraft keep the original plan
through friendly air defence unless ground control changes it. Immediate missions go to a checkpoint and call the air
representative. [`fm_100_2_1_1984` pp. 12-3, 12-4] NATO's view.

**routing-19. Own airspace by geography, height and time (1984).** Soviet commanders divide the airspace by
geography, altitude and time. FM 100-2-1 gives a **hypothetical** geographic division: a boundary well forward of the
FEBA beyond SA-4 range, frontal aviation forward of it; there "may be" safe corridors through the ground air defence,
with periods when SAM units hold fire unless attacked, and periods of fire at all or at none. [`fm_100_2_1_1984`
p. 11-11] NATO's view, explicitly hypothetical.

**routing-20. Corridors through the enemy's air defence (1980-84).** The air operation gives a small part of its
resources to neutralising air defence and making approach corridors; artillery, helicopters and missiles help through
the forward defences [`fm_100_2_1_1984` p. 12-1]. In Soviet open press, writing of **airfield strikes**: surprise by time, by a secret approach and by
the direction the enemy thinks least dangerous, night or overcast preferred; the route to and from the target should
avoid the air defence; where it cannot, deception, EW against early warning, and a force 1-2 minutes ahead to blast a
corridor. Plans give targets, aircraft, time, attack, ordnance and approach and departure routes, worked through on sand
tables, photos and models "to the point of automatism". Forward controllers give a grid square or reference point and
a time; targets are seldom marked. [`hines_1981` pp. 10-12 (airfield strike, after E. Tomilin, *Авиация и
космонавтика* No. 12, 1980, p. 27), pp. 17-18 (plans and "automatism", cited by Hines to other Soviet press
footnotes, not to Tomilin)] US reading of Soviet open press.

**routing-21. Strong defences mean small groups, lowest height, heavy jamming (1977-81).** Weak air defence can be
overcome by massed, dense, echeloned formations; strong defence with guided missiles by small echeloned groups at the
lowest possible height with intensive jamming. [`hines_1981` after F. Shesterin, ВИЖ No. 10, 1977, pp. 77-80]

**routing-22. Bypass rarely works; minimum activity does (1984).** Methods of penetration: through the lethal zone at
top speed and lowest height; bypass round or over; overpowering; manoeuvre; formations. A free bypass is possible only
in staff map exercises; realistically one can count only on a route of minimum air defence activity, which needs
real-time ELINT on actual SAM sites, radar coverage and RWR warnings. Bypass always ended in the firing zone on the
final stage of the route. Diversions made an attack seem to come from one direction while the real one came from
another. Overpowering: a suppression group forms a corridor, and the bombers follow it in a column of flights at
minimum intervals. [`mt_1984_babich`] Soviet view of local wars.

**routing-23. The survival band and its limits (1984).** In the Soviet reading of US data from Vietnam, 60-90 m gave
over 75% survival, 30-60 m and 90-200 m gave 50-75%, under 30 m or over 200 m under 50% (the "death zones"). Near
sonic speed is the sensible limit (IR heating at transonic speed is seen at 8-16 km). The US left low level over the
DRV for medium height with ECM; very low stayed the main method only for the TFR F-111. Over the Bekaa (1982) an
airborne command post and EW aircraft in zones over the sea, fighters in loiter zones, a chaff band and jamming switched
on in advance let the strike group in undetected. An airborne radar sees low fliers at 250-400 km, so low height loses
value; the trend is concealed penetration by single aircraft or small groups to deep targets. Also: an anti-missile
spiral dive from 1500-3000 m, strikes from 500-800 m. [`mt_1984_babich`] Soviet view.

**routing-24. The Soviet method of route planning and the NATO massed strike (1984).** The wartime method: after the
mission, routes and profiles chosen; assembly, formation en route, over the target and on return; where the fighters
join and the lines where escort begins and ends. The Soviet view of a NATO massed strike: two to three echelons; the
first suppresses air defence and fighters across a zone up to 120 km wide; strike groups use corridors opened by the
support echelon, flying from very low to high; minimum intervals between echelons; 12-16 aircraft for an airfield, up
to a wing for a SAM system; up to 800 aircraft in all (100-150 support, 600-700 strike), after *Зарубежное военное
обозрение* 1980 and 1982; a trend to smaller, more numerous groups, cruise missiles and RPVs, and stand-off attack.
[`mt_1984_bobrov` pp. 37-49] Soviet view.

**routing-25. The lineage: 1962-64 (pre-period, for comparison only).** A 1962 «Военная мысль» article saw NATO's
massed strike flying on one axis in a zone 100-150 km wide, on two or more axes with one main, or over a broad front;
diversion groups 15-20 minutes ahead to draw fighters up early; waves two to three minutes apart, each lasting up to ten
minutes; groups of four to eight; 30-40% of targets at the lowest heights [`mt_1962_tacair` pp. 14-16]. A review of a
1964 Frunze Academy textbook said front aviation should work in small groups or single aircraft appearing by surprise
at extremely low (50-100 m) heights, with an air support depth of 150-200 km [`mt_kozhevnikov_1964` pp. 4-6]. These
show the 50-100 m rule and the diversion are old and stable; they are not used for 1985 numbers.

### Helicopters (Soviet Mi-24 and Mi-8)

**routing-26. Under the umbrella, through corridors (1984).** Corrected 15 Nov 2026: the Soviets, wary of enemy
fighters, "prefer to employ them in ground support only to the range of their air defense umbrella" (a preference,
not a rule). Attack helicopters, fixed-wing aircraft and artillery are not normally used in the same fire zone at the
same time; helicopters normally attack after the artillery preparation; when both are used together, the helicopters
get entrance and exit corridors parallel to and between artillery fire concentrations, under the trajectory of the
rounds. [`fm_100_2_1_1984` pp. 12-5 (corridors), 12-8 (umbrella)] NATO's view.

**routing-27. Afghanistan practice (1980-85).** Armed reconnaissance of caravans by a pair or a flight in echelon at
15-20 degrees, at 1500-1700 m, 600-800 m apart. Night search along tracks and roads in pairs 80-100 m apart in
height. In air assaults, groups of up to 60 transport helicopters (Mi-8, Mi-6) had one or two Mi-24 pairs on the
flanks 200-400 m above them on the route; at the landing zone, after artillery, air and Mi-24 strikes, several cover
pairs circled at 1200-1800 m over the landing force (corrected: the 1200-1800 m pairs cover the landing, not the
column). On the way to a target or on patrol, interval between helicopters 1200-1500 m against ambush.
IR exhaust suppressors compulsory from 1983, when MANPADS were felt. In 1985 the DShK and the ZGU caused 42% and 25% of
Mi-24 losses. [`airwar_mi24_markovsky`] Compiled from veterans; tier 4.

### Other air arms

**routing-28. A rigid, Soviet-trained air arm: Iraq (1980-87).** Highly centralised control, rigid flight plans and
tight GCI control of ground attack; only the leader knows the destination; pilots keep to their profiles rather than
deviate and be punished; they pile onto the easiest or least defended targets and use the order not to lose aircraft
to avoid risk. Early in the war few aircraft, bombing from high altitude at high speed; MiG-25s at high altitude until
the end of 1985; from 1985 F1s with AS-30L from low altitude at a safe distance. They seldom fly in poor visibility.
[`cia_1987_iraq_af`] The CIA's view.

**routing-29. The small neutral air arm: no routing source (design inference).** No public period source on how a
small neutral air arm (Saab 105Ö, helicopters) planned routes was found. Design inference, low confidence: short
low-level routes inside its own airspace, few turn points, by day, NATO-style map and stopwatch navigation.

**routing-30. The improvised rebel air arm: no routing source (design inference).** No period source. Design
inference, low confidence: light aircraft and helicopters at night, low, navigating by roads, rivers and villages,
avoiding known guns, one target per sortie, no gate or corridor.

### Our side (the defender's reading)

**routing-31. Where the low fliers come.** US route models and doctrine expect low fliers to follow the lowest ground
and the defender to cover the low corridors (`dorsett_1980`, a helicopter-raid model); US helicopter doctrine expects air defence to ambush NOE
routes (`fm_17_50_1977`). The Soviet reading is that the last leg always enters the target's firing zone
(`mt_1984_babich`). For SAM3D: the battery's position covering the valleys and low ground on the likely approach is
where the period's raids would come; an Osa sited on the target's last leg will see the raid even if the route
minimised exposure before it. Medium confidence.

## 3. Numbers usable as data

Heights in feet are as printed; metres in brackets are converted (1 ft = 0.3048 m). Speeds in knots as printed
(1 kt = 0.5144 m/s).

| Quantity | Value | Faction | Date | Source | Confidence |
|---|---|---|---|---|---|
| Normal low-level combat height | 200-500 ft AGL (61-152 m) | NATO (US) | 1973 | `afm_51_40_1973` p. 26-1 | high |
| CF-104 strike training route | 500 ft (152 m), 450 kt; 800 ft Apr-Oct | NATO (Canada, Germany) | c. 1963-72 (strike role, pre-period) | `agard_cp267_1979` p. 23-1 | high |
| CF-104 last leg to target | 50 ft AGL (15 m), 550 kt | NATO (Canada) | c. 1963-72 (pre-period) | `agard_cp267_1979` p. 23-1 | high |
| CF-104 bad-weather climb | 2000 ft AGL (610 m), radar | NATO (Canada) | c. 1963-72 (pre-period) | `agard_cp267_1979` p. 23-1 | high |
| CF-104 reconnaissance | 500 ft, 420 kt | NATO (Canada) | c. 1963-72 (pre-period) | `agard_cp267_1979` p. 23-1 | high |
| CF-104 tactical air support | planned 500 ft, 420 kt; with FAC 500-1000 ft at 400+ kt; 1:250,000 map | NATO (Canada) | c. 1973-79 (current in 1979) | `agard_cp267_1979` p. 23-1 | high |
| B-52 low level (training) | 500 ft day, 800 ft night (flat ground); 325-350 kt; legs 1-2.5 h | NATO (US) | 1979 | `agard_cp267_1979` p. 4-3 | high |
| FB-111 low level | 400 ft at 0.63-0.73 M (420-480 kt); ~1000 ft at night | NATO (US) | 1979 | `agard_cp267_1979` p. 4-3 | high |
| FB-111 TFR look-ahead | terrain seen 10 mi ahead, path computed at 6 mi, controls react at 2 mi | NATO (US) | 1979 | `agard_cp267_1979` p. 4-3 | medium |
| A-10 tactical mission | often under 100 ft, 200-300 kt; 40-60% of time under 500 ft; 1.5-2 h | NATO (US) | 1979 | `agard_cp267_1979` p. 4-3 | high |
| Very low penetration | 200 ft or less | NATO (US) | 1977 | `crawford_1977_rand` p. 2 | high |
| Exposure that degrades mobile SAMs | 20 s or less | NATO (US view) | 1977 | `crawford_1977_rand` pp. 7-8 | medium |
| Intercepts cut by path choice | up to x3 (one example site) | NATO (US view) | 1977 | `crawford_1977_rand` pp. 7-8 | medium |
| Low fliers' height over terrain | within 200 m | general (US model) | 1980 | `dorsett_1980` §3.1 | medium |
| Route model heading window | 90 deg (45 deg inside 1 km of goal); weights 1 ahead, 2 behind | US Army model of a helicopter raid (game method, not doctrine) | 1980 | `dorsett_1980` §4.5-4.6 | medium |
| Route model radar avoidance | cone base 2 x kill radius; weights 2 short of kill zone, 10 beyond | US Army model of a helicopter raid (game method, not doctrine) | 1980 | `dorsett_1980` §4.7 | medium |
| Package flow at Red Flag | 60-70 aircraft through a series of corridors in 20-30 min | NATO (USAF exercise, RAF participants) | from 1977 | `rafhs_j20_1999` (Pitchfork) | medium |
| RAF formation (Buccaneer, Red Flag) | loose 2 or 4 ship; R/T only for a fighter threat | NATO (RAF) | from 1977 | `rafhs_j20_1999` | medium |
| Buccaneer/Jaguar training height | 100 ft (30 m), four-ships | NATO (RAF) | 1977 on | `rafhs_j20_1999` | high |
| Vulcan low level | ~1 h; 250 kt, 350 kt to attack; 500 ft (300 ft experienced) | NATO (RAF) | to 1982 | `rafhs_j20_1999` | medium |
| Vulcan night TFR | 1000 ft AGL (Red Flag 79/2); 800 ft (Goose Bay) | NATO (RAF) | 1979-82 | `rafhs_j20_1999` | medium |
| Tornado GR1 | to 100 ft in places; TFR 500 ft at 420 kt; loft 480 kt at 150 ft; sortie 1 h 45 | NATO (RAF Germany) | mid-1980s (uncertain) | `napier_hushkit_2022` | low |
| F-111F | TFR to 200 ft; manual ~500 ft by day; 480-540 kt; ~0.95 M on the deck | NATO (USAFE) | early-mid 1980s | `crandall_twz` | medium |
| UK peacetime floor | 250 ft (76 m); 100 ft in three areas | NATO (UK) | from 1979 | `hansard_lowflying` | high |
| FRG peacetime bands | normal band 150-450 m; seven "250 ft" areas to 75 m; below 75 m (Tiefstflug) not over the FRG, abroad only (Goose Bay from 1981, to 100 ft) | NATO (FRG) | 1984-87 | `btd_10_2980_1985`, `btd_11_558_1987` | high |
| Helicopter unmasked time vs ZSU-23-4 | 35 s or less (scan garbled) | NATO (US Army) | 1977 | `fm_17_50_1977` ch. 3 | low |
| Soviet ПМВ (extremely low) | 0-200 m over terrain or water | Warsaw Pact | 1985 | `opp_85` §45(7) | high |
| Soviet low / medium / high | 200-1000 / 1000-4000 / 4000-12000 m | Warsaw Pact | 1985 | `opp_85` §45(7) | high |
| Soviet approach height over enemy territory | 50-100 m ("ideally") | Warsaw Pact (NATO view) | 1984 | `fm_100_2_1_1984` p. 12-8 | medium |
| Target identification time | 3-6 s | Warsaw Pact (NATO view) | 1984 | `fm_100_2_1_1984` p. 12-8 | medium |
| Clear the target area | within 10 s | Warsaw Pact (NATO view) | 1984 | `fm_100_2_1_1984` p. 12-8 | medium |
| Corridor-opening group ahead of the strike | 1-2 min | Warsaw Pact (Soviet press) | 1980 | `hines_1981` after Tomilin | medium |
| Survival by height (Vietnam, Soviet reading) | 60-90 m >75%; 30-60 and 90-200 m 50-75%; <30 or >200 m <50% | general | 1984 | `mt_1984_babich` | medium |
| IR heating at transonic speed seen at | 8-16 km | general (Soviet view) | 1984 | `mt_1984_babich` | low |
| Airborne radar sees low fliers at | 250-400 km | NATO (Soviet view) | 1984 | `mt_1984_babich` | medium |
| Anti-missile spiral dive start; strike height | 1500-3000 m; 500-800 m | general (Soviet view) | 1984 | `mt_1984_babich` | low |
| NATO first-echelon suppression zone | up to 120 km wide | NATO (Soviet view) | 1984 | `mt_1984_bobrov` | medium |
| NATO aircraft per target | 12-16 per airfield; up to a wing per SAM system | NATO (Soviet view) | 1984 | `mt_1984_bobrov` | medium |
| NATO massed strike | up to 800 (100-150 support, 600-700 strike) | NATO (Soviet view) | 1984 | `mt_1984_bobrov` after ZVO 1980, 1982 | medium |
| Diversion ahead of a massed strike | 15-20 min (1962) | NATO (Soviet view) | 1962, pre-period | `mt_1962_tacair` p. 14 | low |
| Mi-24 armed reconnaissance | 1500-1700 m; 600-800 m apart; echelon 15-20 deg | Warsaw Pact (Afghanistan) | 1980-85 | `airwar_mi24_markovsky` | medium |
| Mi-24 night search pairs | 80-100 m apart in height | Warsaw Pact (Afghanistan) | 1980-85 | `airwar_mi24_markovsky` | medium |
| Helicopter air-assault escort | Mi-24 pairs 200-400 m above the transport group (to 60 helicopters); cover pairs at 1200-1800 m over the landing zone | Warsaw Pact (Afghanistan) | 1980-85 | `airwar_mi24_markovsky` | medium |
| Mi-24 losses to guns, 1985 | DShK 42%, ZGU 25% | Warsaw Pact (Afghanistan) | 1985 | `airwar_mi24_markovsky` | medium |

Derived (textbook physics, not a source): a level turn at 480 kt (247 m/s) and 60 degrees of bank (2 g) has a radius of
about 3.6 km, so turn points closer than a few kilometres apart cannot be flown as legs at that speed. At 450 kt, 3-6 s
to identify a target is 0.7-1.4 km.

No public period number was found for: leg length, number of turn points, IP distance from the target (the SAAF's 4-5
nm pull-up is 1987-88, section 5), or a fixed stand-off from a SAM ring. These stay game values in
`data/gameplay/air_war.json`, labelled as unsourced.

## 4. Disagreements between sources and which to use

1. **How low NATO flew.** AFM 51-40 (1973) gives 200-500 ft as normal; CF-104 and B-52 training was at 500 ft; RAF
   Buccaneers and Jaguars trained at 100 ft in special UK areas and abroad; FRG peacetime band 150-450 m with 75 m in
   seven areas and nothing lower over the FRG; the 1979 AGARD paper
   expects war heights to be lower than training heights. Use: per type and per phase, 100-250 ft for RAF and
   Luftwaffe strike in war, 200 ft TFR for F-111 at night and in cloud, 500 ft for bombers and the night, 50 ft on the
   last leg for fighter-bombers; peacetime floors are not the war.
2. **The value of flying low.** The FRG government (1987, after the period; it names MiG-29, MiG-31 and Su-27 in the
   GSFG) said low flying with tactics and EW keeps the power to penetrate the Pact's ground and air defences;
   Babich (1984) said airborne radar sees low fliers at 250-400 km so low height loses value, and that the US left low
   level over the DRV for medium height with ECM. Both are period views. Use: NATO flies low in 1985 (its own doctrine
   and training); the look-down fighters that would change this were mostly fielded after 1985 (section 5).
3. **The best height band.** Babich's Soviet reading of US data puts the best survival at 60-90 m; FM 100-2-1 puts
   the Soviet ideal approach at 50-100 m; ОПП-85 classes all of 0-200 m as extremely low. They agree in substance. Use
   50-100 m for Pact strike over enemy territory, 30-75 m (100-250 ft) for NATO.
4. **Bypass or not.** The Soviet open press (Tomilin 1980 via Hines) and the MAI teaching aid (`mai_tactics_vvs`, in
   the earlier note) say the route should avoid the air defence; Babich says a full bypass works only on a staff map
   and that every route ends in the firing zone. Use Babich: minimise believed exposure, never require a route with no
   exposure, and accept the target's own defence on the last leg.
5. **Who decides the route in the air.** NATO: the crew decides whether to use an MRR and has a free choice of attack;
   the Pact (NATO's view): keep the plan unless ground control changes it; Iraq (CIA): keep the profile or be punished.
   These are not contradictions but faction differences; use them as such.
6. **Soviet helicopters: low under the umbrella or high above the guns.** FM 100-2-1 (Europe) keeps them under their
   own air defence, low through corridors; Afghanistan practice put armed reconnaissance at 1500-1700 m above the
   guns. Use the European rule against NATO and the Pact's peer foes; the Afghan heights only in a rebel war.
7. **Tornado TFR at 500 ft and F-111 at 200 ft.** Different aircraft and set clearances; both veterans' accounts; no
   conflict.
8. **The ZSU-23-4 exposure rule.** FM 17-50's scan is garbled ("35 ... seconds"). RAND's 20 s for mobile SAMs is
   clear. Use 20 s as the planner's target for continuous exposure to any believed site, with 35 s as the helicopter
   rule only once the scan is checked.
9. **Very low flying over West Germany (resolved 15 Nov 2026).** The Federal Government's answer of March 1985 says
   very low flights (Tiefstflüge) are not flown over the Federal territory, against a question that cited allies at
   75 m; its answer of June 1987 lists seven 250 ft (75 m) areas, one of them in use in 1985. The 1987 answer (to
   question 36) explains the terms: since training at Goose Bay began in 1981, "Tiefstflug" is used only for heights
   below 250 ft (75 m), and 250-1500 ft is "Tiefflug". So no contradiction: the seven areas go down to 75 m; below
   75 m only abroad. Either way it is peacetime law, not the war; use the war heights in item 1.

## 5. Post-period items excluded

- Stinger in Afghanistan (from September 1986) and the Soviet move to extremely low helicopter flight after it.
- Operation El Dorado Canyon (April 1986): F-111 routing round Europe; a later war's option.
- SAAF long-toss in Angola with a 100 ft ingress and a 4-5 nm pull-up (1987-88, `saaf_toss`): the technique is older,
  but no period source for it was read here.
- Croatia's improvised air arm (1991, `croatia_1991_migflug`): the rebel faction's shape only.
- Austrian Drakens (1988).
- FM 100-103, *Army Airspace Command and Control in a Combat Zone* (1987), and the later joint definitions of MRR and
  low-level transit route (JP 3-52, 1990s).
- ATP-40(C) (2002) and later NATO airspace control publications.
- FM 1-112, *Attack Helicopter Operations* (1997 edition), downloaded and not used.
- The German cuts to low flying (1990) after the 1988 crashes.
- Tornado GR1s below 100 ft in 1991 and the Gulf War move to medium level (1991): hindsight only.
- MiG-29 and Su-27 look-down fighters facing NATO in Germany: mostly fielded after 1985. The FRG government's 1987
  analysis of FULCRUM, FOXHOUND and FLANKER in the GSFG (`btd_11_558_1987`) is a later war's reading, used here only
  as hindsight.
- Tornado GR1 practice in `napier_hushkit_2022` (routing-11): the author's five years at Brüggen are undated; the GR1
  reached RAF Germany from about 1984, so some or all of it may be after 1985. Kept in section 2 at low confidence;
  prefer `agard_cp267_1979` and `crandall_twz` for 1985 numbers.
- CF-104 nuclear strike and reconnaissance profiles (routing-05): before the period (about 1963-72); lineage only.
- TALD decoys (1987), LANTIRN (from 1987), NVG strike, Link 16, AMRAAM, killer scouts.

## 6. Sources

| Id | Tier | Full citation | URL | Read | Whose view |
|---|---|---|---|---|---|
| `fm_100_42_1976` (new) | 1 | AFM 2-14 / FM 100-42, *US Air Force/US Army Airspace Management in an Area of Operations*, 1 Nov 1976 | https://archive.org/details/FM_100_42_US_A_F_US_A_A_M_A_O_1976 | in full | US doctrine |
| `jsead_1982` (registered) | 1 | TRADOC TT 100-44-1 / TACP 50-23, *Joint Suppression of Enemy Air Defenses (J-SEAD) Operations*, 11 June 1982, DTIC ADA393443 | https://archive.org/details/DTIC_ADA393443 | ch. 3-4 read from the archive.org text (registered as abstract only; update) | US doctrine |
| `afm_51_40_1973` (new) | 1 | AFM 51-40 / NAVAIR 00-80V-49, *Air Navigation*, 1973 edition, ERIC ED082051 | https://archive.org/details/ERIC_ED082051 | ch. 10 and 26 | US doctrine |
| `agard_cp267_1979` (new) | 1 | AGARD Conference Proceedings No. 267, *High-Speed, Low-Level Flight: Aircrew Factors*, Lisbon, 24-26 Oct 1979, DTIC ADA087592; paper 4 (US aeromedical) and paper 23 (R. C. Rud, D. Leben, DCIEM, Canada) | https://archive.org/details/DTIC_ADA087592 | papers 4 and 23 | NATO, own practice |
| `crawford_1977_rand` (new) | 1 | N. W. Crawford, *Low Level Attack of Armored Targets*, RAND P-5962, 1977, DTIC ADA054186 | https://archive.org/details/DTIC_ADA054186 | in full | US (RAND) |
| `dorsett_1980` (new) | 1 | M. J. Dorsett, *A Heuristic Route Selection Model for Low Level Aircraft Flight Through Defended Terrain*, US Army Missile Command, TR D-80-5, May 1980, DTIC ADA091793 | https://archive.org/details/DTIC_ADA091793 | most of it | US Army |
| `fm_17_50_1977` (new) | 1 | FM 17-50, *Attack Helicopter Operations*, 1 July 1977 | https://archive.org/details/FM_17_50_A_H_O_1977 | ch. 3 and terrain flight | US doctrine |
| `btd_10_2980_1985` (new) | 1 | Deutscher Bundestag, Drucksache 10/2980, answer of the Federal Government on low flying, 8 Mar 1985 | https://dserver.bundestag.de/btd/10/029/1002980.pdf | in full | FRG government |
| `btd_11_558_1987` (new) | 1 | Deutscher Bundestag, Drucksache 11/558, answer of the Federal Government on low flying, 25 Jun 1987 | https://dserver.bundestag.de/btd/11/005/1100558.pdf | in full | FRG government |
| `hansard_lowflying` (new) | 1 | Hansard, HC Deb 30 Oct 1987 c482W (jet pilots' low flying training); HC Deb 29 Jun 1989 vol 155 c532W (low flying) | https://api.parliament.uk/historic-hansard/written-answers/1989/jun/29/low-flying | in full | UK government |
| `rafhs_j20_1999` (new) | 2 | *Royal Air Force Historical Society Journal* 20, seminar "Training in Peace for War" (1999): G. Pitchfork and Vulcan contributors | https://www.rafmuseum.org.uk/documents/Research/RAF-Historical-Society-Journals/Journal-20-Seminar-Training-in-Peace-for-War.pdf | relevant parts | RAF veterans |
| `napier_hushkit_2022` (new) | 2 | M. Napier, "10 reasons I loved flying the Tornado", *Hush-Kit*, 21 Sep 2022 | https://hushkit.net/2022/09/21/10-reasons-i-loved-flying-the-tornado/ | in full | RAF veteran (press) |
| `crandall_twz` (new) | 2 | R. Crandall with T. Rogoway, "Flying the iconic swing-wing F-111 Aardvark at the height of the Cold War", *The War Zone* | https://www.twz.com/4595/flying-the-iconic-swing-wing-f-111-aardvark-at-the-height-of-the-cold-war | in full | USAF veteran (press) |
| `dtic_linebacker` (registered) | 1 | DTIC ADA241219, study of Linebacker II command and control | https://apps.dtic.mil/sti/tr/pdf/ADA241219.pdf | search excerpt only | US study of its own campaign |
| `opp_85` (new) | 1 | «Основные правила полётов в воздушном пространстве СССР (ОПП-85)», MoD USSR, in force 1 Dec 1985 (order of the C-in-C VVS No. 161, 3 Jul 1985) | https://ovdrf.ru/userfiles/files/O%D0%9F%D0%9F%2085.pdf | relevant parts | Soviet regulation |
| `fm_100_2_1_1984` (registered) | 1 | FM 100-2-1, *The Soviet Army: Operations and Tactics*, 16 July 1984, ch. 11-12 | https://archive.org/details/fm100-2-1 | relevant chapters | NATO's view of the Soviet side |
| `hines_1981` (new) | 1 | K. L. Hines, *Soviet Frontal Aviation Operations: Concepts and Problems*, US Army Russian Institute, June 1981, DTIC ADA114610 | https://archive.org/details/DTIC_ADA114610 | relevant parts | US reading of Soviet open press |
| `mt_1984_babich` (registered) | 1 | V. K. Babich, «Военная мысль» No. 5, 1984, pp. 34-43, CIA-RDP10-00105R000403350001-1 | https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000403350001-1 | in full | Soviet |
| `mt_1984_bobrov` (new) | 1 | D. V. Bobrov, "Massed Aviation Strikes", «Военная мысль» No. 10, 1984, pp. 37-49, CIA-RDP10-00105R000403340001-2 | https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000403340001-2 | relevant parts | Soviet |
| `mt_1962_tacair` (new) | 1 | "Strong and Weak Aspects of the Tactical Aviation of Our Probable Enemies", «Военная мысль» secret collection No. 5 (66), 1962; CIA translation of 26 Nov 1975, CIA-RDP10-00105R000201520001-6 | https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000201520001-6 | relevant parts | Soviet (pre-period) |
| `mt_kozhevnikov_1964` (new) | 1 | M. Kozhevnikov, review of the Frunze Academy textbook *Front Aviation in Army Operations and a Combined-Arms Battle* (1964), «Военная мысль» secret collection, issue not shown; CIA-RDP10-00105R000302560001-0 | https://archive.org/details/cia-readingroom-document-cia-rdp10-00105r000302560001-0 | in full (pages released) | Soviet (pre-period) |
| `airwar_mi24_markovsky` (new) | 4 | V. Markovsky, Mi-24 in Afghanistan, airwar.ru («Уголок неба») | http://www.airwar.ru/history/locwar/afgan/mi24/mi24.html | relevant parts | Russian, compiled from veterans |
| `cia_1987_iraq_af` (new) | 1 | CIA, *Iraq's Air Force: Improving Capabilities, Ineffective Strategy*, intelligence assessment, 1987, CIA-RDP88T00096R000700910001-5 | https://www.cia.gov/readingroom/document/cia-rdp88t00096r000700910001-5 | relevant parts | the CIA's view |
| `mai_tactics_vvs` (registered) | 1 (status as registered) | MAI military institute, «Тактика военно-воздушных сил», §3.3.3 | https://files.mai.ru/site/unit/institute-of-military-science/tvvs/data/g3/3-3-3.html | in the earlier note | Russian teaching aid on Soviet methods; not a period document |

Archive.org identifiers for the CIA documents follow the reading-room naming used for `mt_1984_babich`; the texts
were read from those copies.

## 7. Open items and documents to acquire

- TACM 3-1 (USAF tactics manual): classified; not to be sought.
- *USAF Fighter Weapons Review*, 1978-84 (Shaw's *Fighter Combat*, 1985, cites articles on low-level tactics): no
  public copy found.
- *Flying Safety*, "18 Hazards of Low Level Flying" (1981-82): the site refused automated downloads.
- AFM 51-40, 1983 revision: not found.
- ATP-40 / STANAG 3805 in their 1980s editions; FM 100-103 (1987) text; TC 1-201 (1984), *Tactical Flight Procedures*.
- НПП and КБП for frontal aviation (the ПМВ limits by skill class); the 1983 *Военный энциклопедический словарь*
  entry «Высота полёта».
- *Авиация и космонавтика* 1980-85, especially Tomilin, No. 12, 1980, p. 27, in the original.
- Soviet GCI along the route for strike aircraft: who controls the group between the checkpoint and the target.
- Luftwaffe and RAF Germany route planning documents (planning cards, the 1:500,000 and 1:250,000 maps' use, turn
  point spacing): none public found.
- Falklands 1982: Argentine low-level routes over the sea (the Carballo interview in *Air Forces Monthly*, 1991; a
  key.aero copy refused access); `navalgazing_falklands` as registered.
- Leg lengths, turn-point counts and IP distances in any period public document.

## Verification (15 Nov 2026)

Adversarial check of the load-bearing findings. Each cited text was downloaded and searched in full (archive.org OCR
text of the DTIC, Army and CIA documents; the OPP-85, RAF Historical Society and Bundestag PDFs; the airwar.ru page);
the two press articles (`napier_hushkit_2022`, `crandall_twz`) and the 1989 Hansard answer were read through a page
fetch. Not re-checked: routing-15 (`dtic_linebacker`, still a search excerpt), routing-21's exact pages beyond
Hines's footnotes (VIZh 1977 pp. 77 and 80), routing-25 (pre-period), routing-29/30 (design inferences), and the
HC Deb 30 Oct 1987 answer.

| Check | Verdict | Note |
|---|---|---|
| routing-01 MRR | confirmed | FM 100-42 (1 Nov 1976) §4-6d(1)-(6) and glossary say exactly this, incl. aircrew choice and CAS not normally using MRR. |
| routing-02 return under AD procedures | confirmed | §4-6d(5) and §4-6a; standard-use Army aircraft routes are §4-6e (citation corrected). |
| routing-03 corridor suppression | confirmed | J-SEAD TT 100-44-1, 11 June 1982, ch. 4: route and time, the five items, "usually disruptive", deviations passed at once. |
| routing-04 low-level navigation | corrected | AFM 51-40 (1 July 1973) ch. 26 text confirmed word for word; the IP definition is from the glossary, not ch. 10. |
| routing-05 CF-104 | corrected | Numbers confirmed (all on p. 23-1), but strike and recce profiles are the 1963-72 roles (past tense); only tactical air support (500 ft, 420 kt, routes on weather and SAM sites) is current. |
| routing-06 B-52, FB-111, A-10 | confirmed | Paper 4 pp. 4-2/4-3: every number as stated, incl. the warning that war heights may be lower. |
| routing-07 RAND | confirmed | Crawford P-5962, symposium 30 Mar 1977: 200 ft or less, factor of three (one site, paths 41/43), 20 s, little training below 500 ft; the 20 s rests on accurate site intelligence ("a big if"). |
| routing-08 Dorsett | corrected | Scenario is a helicopter force over a 35 x 35 km area; weights confirmed; "matched manual planning at far less time" not found: model 0.888 vs best manual 0.844. |
| routing-09 RAF corridors | corrected | It is Red Flag (Nellis, from 1977), not RAF Germany; the link to war is Pitchfork's remark that it resembled crossing the inner-German border. |
| routing-10 Vulcan | confirmed | 1 h, 250/350 kt, 500/300 ft, 1000 ft TFR at Red Flag 79/2, 800 ft Goose Bay, "fly low on track and on time" all in the journal. |
| routing-11 Tornado GR1 | unsupported (date) | Numbers are in the article, but no years; may be after 1985; listed in section 5 as possibly post-period. |
| routing-12 F-111F | confirmed | Numbers in the article; manual TF was raw-data, day VFR, combat-emergency only; tour dated by a WSO lost in April 1986. |
| routing-13 floors | corrected | UK 250/100 ft, three areas, 1979 confirmed; "no foreign air force below 250 ft" not found (unverified). FRG terms corrected (Tiefstflug = below 75 m, abroad only); 150 m Luftwaffe floor is a question's premise; look-down argument is 1987 and moved to section 5. |
| routing-14 helicopter terrain flight | confirmed | FM 17-50 (1 July 1977) ch. 3: three modes, NOE corridor and slalom, NOE not at night, ADA ambushes NOE; "35 seconds or less" vs ZSU-23-4 is split by the OCR but reads complete. |
| routing-16 OPP-85 | confirmed | §45(7) height classes, §159, §160 confirmed; in force 1 Dec 1985 by order No. 161 of 3 Jul 1985; terrain classes are in the definitions (mountainous also >=1000 m ASL). |
| routing-17 Soviet approach | confirmed | FM 100-2-1 p. 12-8 verbatim: 50-100 m ideal, 3-6 s, 10 s, radio minimal, gaps, decoys, other directions or sun. |
| routing-18 checkpoint and ground control | corrected | p. 12-4 confirmed; safe corridors are the US authors' inference ("probable use"), now said so. |
| routing-19 airspace division | corrected | p. 11-11 calls the geographic division "hypothetical" and the corridors "may be"; now labelled. |
| routing-20 corridors, Tomilin | corrected | Hines pp. 10-12 confirmed but the Tomilin passage is about airfield strikes; "automatism" is cited to other footnotes. |
| routing-21 Shesterin | confirmed | Hines p. 11 with footnotes to VIZh No. 10, 1977, pp. 77 and 80. |
| routing-22 Babich bypass | confirmed | Military Thought No. 5, May 1984: "only in map exercises for staffs", minimum activity route needs real-time ELINT and RWR, bypass ended in the firing zone. |
| routing-23 Babich survival band | confirmed | 60-90 m >75%, 30-60/90-200 m 50-75%, <30/>200 m <50%; IR at 8-16 km; 250-400 km airborne radar; spiral from 1500-3000 m, strike from 500-800 m (US practice in Vietnam). |
| routing-24 Bobrov | confirmed | Military Thought No. 10, Oct 1984: 2-3 echelons, 120 km, corridors very low to high, 12-16 per airfield, up to a wing per SAM system, 800 (100-150/600-700); after "Cold Fire" exercises. |
| routing-26 Soviet helicopters | corrected | "prefer ... only to the range of their air defense umbrella" (p. 12-8); corridors only when helicopters and artillery act together (p. 12-5). |
| routing-27 Mi-24 Afghanistan | corrected | All numbers in Markovsky's text; the 1200-1800 m pairs cover the landing zone, not the column; ЭВУ compulsory from 1983 and DShK 42%, ZGU 25% (1985) confirmed. |
| routing-28 Iraq (CIA) | confirmed | CIA intelligence assessment, October 1987: rigid plans, GCI for ground attack, only the leader knows the destination, profiles over punishment, pile-on, MiG-25 to end 1985, F1/AS-30L from 1985, seldom in low visibility. |

Period scan: no post-1985 tactic, datalink, weapon or brevity term is presented as 1985 practice. Items now also
excluded or flagged as later: the FRG's 1987 look-down analysis, the undated Tornado GR1 account; the CF-104 strike
and reconnaissance profiles are flagged as pre-period lineage.
