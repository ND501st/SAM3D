# How the best game tutorials teach, and what SAM3D's lessons take from them

Research for the seat lessons (19 Oct 2026; the request: "far more beginner friendly ... a tutorial for each seat in the
Osa, step by step; each instruction highlights which knob or switch to press, and you can only proceed once that step is
completed; refer to DCS and other complex game tutorials"). What follows is what the sources say, then the rules SAM3D's
lessons are built to (`docs/TUTORIALS.md` is how they are built; `docs/PLAYING.md` how they are played).

## 1. What the sources say

### Study simulators

**DCS World's interactive training missions.** The built-in lessons are the first thing the community sends a new pilot
to: cockpit familiarisation, start-up, navigation and weapons, one lesson at a time ([FlyAway Simulation][fa-dcs]). Under
the hood a lesson is mission-editor triggers: a COCKPIT HIGHLIGHT ELEMENT trigger draws a bracket on a named control,
and a COCKPIT ARGUMENT IN RANGE trigger watches that control's position; when the pilot moves it, the next step plays
([DCS forum: training missions][dcs-train], [DCS forum: control highlights][dcs-hl]). Narration steps wait for the space
bar or resume on their own after a pause. What players complain about: brackets that cover the label they point at;
inconsistent "press space to continue" prompts; lessons that **stall** when a trigger doesn't fire, leaving the pilot
waiting with no idea why ([DCS forum][dcs-train]); and lessons that wait for an *exact* switch position, so clicking ahead
breaks them, and that drift out of step with the aircraft after updates ([FlyAway Simulation][fa-dcs]).

**Microsoft Flight Simulator 2020 flight training.** 43 lessons in 7 series, most **2 to 7 minutes** long. An instructor's
voice, **highlighted instruments and controls and glowing markers** guide each lesson; the objectives are listed on
screen and **turn green when validated**. Targets are concrete ("rotate at 55 kt, climb at 75 kt to 5,500 ft"). Each
series ends in a **test flown without the aids**. Lessons can be taken in any order and repeated at any time
([Flight Simulator wiki][msfs]). Players have reported checkpoints that fail them without saying why (an FSX
instrument checkride failed on a heading instruction: [Steam discussion][fsx-check]).

**Train Sim World.** Seven guided tutorials from first steps to operating a locomotive; on-screen objectives state the
goal of each step ("enter the cab, unlock the doors, set the reverser, depart"); per-train training modules introduce
each control of that cab; the HUD keeps the next objective visible ([FlyAway: Train Sim World][fa-tsw]).

### Game design

**Portal (Valve).** The whole game is "an extended player training exercise": one tool at a time, then the tools layered
into harder puzzles; the button-and-box mechanic is trained before portals; a newly learnt skill is reused at once in a
new context to cement it; the environment teaches before the narrator states the rule
([Portal developer commentary][portal], [Mechanics of Magic][portal-mom]).

**"Prime, Teach, Observe"** (Asher Vollmer, GDC 2014): prime the player for a mechanic, help them internalise it, watch
real players and iterate; "empower your players without patronizing them" ([GDC Vault][gdc-pto]).

**Factorio's new-player experience.** The developers found the old demo took a brand-new player 30-45 minutes to reach
the game's point (automation), and reworked it, twice ([FFF #241][fff241], [FFF #342][fff342]): get to the point early.

**Hints.** Good hint systems are opt-in and graduated: a vague nudge first, more specific only as the player stays stuck,
the answer last; idle timers and repeated failures trigger the next level ([adv3Lite hints][ifhints],
[kindatechnical][kt-hints]).

**Accessibility.** Introduce concepts gradually and in context rather than in one front-loaded tutorial; make practice
repeatable; don't make precise timing essential, let the player pause; make every segment reachable again later
([Game Accessibility Guidelines: contextual help][gag-help], [GAG: timing][gag], [Xbox XAG][xag]).

### Learning science

**Mayer's principles of multimedia learning** ([Mayer 2020][mayer], [Dartmouth summary][mayer12]): *segmenting*
(learner-paced chunks beat a continuous stream); *pre-training* (learn the names and characteristics of the main parts
before the procedure); *signalling* (highlights, arrows and call-outs show what matters); *spatial contiguity* (words
next to the thing they describe); *modality* (narration with pictures beats on-screen text with pictures); *coherence*
(cut what is extraneous); *redundancy* (don't print the narration word for word).

**Cognitive load: worked examples, guidance fading, expertise reversal** ([Worked-example effect][wee],
[Teacher Magazine][tm-clt]): novices learn more from worked, step-by-step solutions than from solving the problem
themselves; the guidance should then **fade** as they learn, because for someone who knows the steps it becomes
extraneous load (the expertise-reversal effect).

**The military method: EDIP.** Skills are taught Explain, Demonstrate, Imitate, Practise: the instructor explains,
demonstrates the perfect picture (at full speed, then slowly), the learners copy it step by step with every fault
checked, then practise to proficiency; about half of a lesson's development is EDIP ([ARRSE][arrse], [USMC][usmc],
[US Army drill instruction][drill], [NATO Mission Iraq][nmi]). It descends from Charles Allen's four-step shipyard method
of the First World War and Training Within Industry ([Samizdata][samizdata]).

## 2. What SAM3D's lessons do (the rules)

1. **One action per step, gated on the real control.** Each step asks for one thing: a switch to a position, a button
   pressed, a wheel turned onto a mark, an order given. The lesson goes on only when the cabin's control is there (DCS's
   cockpit argument), never on a key press that says "done". Buttons, which spring back, count when the learner pressed
   them during the step.
2. **The control is signalled where it is, without hiding it.** The control pulses in the cabin; a bracket frames it
   on the screen with the position to set beside it, outside the control so its label stays readable (DCS's complaint);
   when it is off the screen an arrow at the screen's edge points to it with its name. **L** turns the head onto it.
3. **Words beside the thing, and few of them.** The step card says the action in one line (the control's Russian name,
   its English sense, the position), where it is (the panel, the side), and **why** in one sentence. Nothing else.
4. **Names before procedures** (pre-training): each seat lesson opens with a short tour of the seat: each panel and the
   few controls and lamps that matter, highlighted one after the other, the learner reading at their own pace.
5. **The learner sets the pace.** Reading steps wait for **Enter**, always the same key, always shown. Nothing is timed;
   the target only comes when the lesson releases it. Waiting for equipment (warm-up, the turbine, the gyro) runs time
   faster by itself and says what it is waiting for.
6. **The lesson never stalls silently.** A step that waits shows what for ("the driver's turbine: power in about a
   minute") and, if it can't come, why. Every lesson is played through headless by a perfect student in the tests, so a
   lesson that can't be finished doesn't ship.
7. **Only the step's control works** (strict lessons): any other switch answers "Not now: that's X; the highlighted one
   is Y" instead of moving, so a beginner can't break the drill by clicking about. (The missions and drills are free.)
8. **Immediate feedback.** A done step flashes green with a tick; a wrong move is answered at once, saying what it was.
   The crew answers too, as in the real drill: the callouts are the confirmation that a step had its effect.
9. **Hints escalate.** The card is always there; after some seconds without a move the "how" line lights up (left click,
   right click, drag the wheel); later the card offers L to look at the control.
10. **The drill is real.** Each lesson follows the console's procedures (`data/procedures/osa_ak.json`): the same
    controls, positions, order and waits the NPCs use and the debrief judges, so what is learnt is the drill. The orders
    come from the commander in the lesson's own time, and the learner's moves go through his seat like any player's.
11. **Explain, demonstrate, imitate, practise.** The tour and the "why" lines explain; the NPC crewmates demonstrate the
    other seats' parts around the learner (and the H overlay's *demonstrate* level shows any seat); the gated steps are
    the imitation; practice is the missions with the overlay on *hint*, then *unassisted*, the guidance fading as the
    expertise-reversal research says it should.
12. **Short, repeatable, in any order.** One lesson per seat, each a few minutes of play (MSFS: 2-7 min), restartable
    from the pause menu and reachable from the missions menu at any time.
13. **A voice.** The instructor speaks each step's action (narration over text: the modality principle); the card
    carries the words for anyone who can't hear them.

[fa-dcs]: https://flyawaysimulation.com/ask/answers/best-dcs-world-training-missions-resources/
[dcs-train]: https://forum.dcs.world/topic/179364-training-missions/
[dcs-hl]: https://forum.dcs.world/topic/104222-dynamic-cockpit-control-highlights/
[msfs]: https://flight.wiki.gg/wiki/Microsoft_Flight_Simulator_(2020)/Flight_Training
[fsx-check]: https://steamcommunity.com/app/314160/discussions/1/360672383119509286
[fa-tsw]: https://flyawaysimulation.com/ask/answers/get-started-train-sim-world/
[portal]: https://combineoverwiki.net/wiki/Developer_commentary/Portal
[portal-mom]: https://mechanicsofmagic.com/2026/05/08/critical-play-puzzles-204/
[gdc-pto]: https://gdcvault.com/play/1020512/Prime-Teach-Observe-Tutorializing-Innovative
[fff241]: https://factorio.com/blog/post/fff-241
[fff342]: https://cf-www.factorio.com/blog/post/fff-342
[ifhints]: https://1w72wjp5mg.unbox.ifarchive.org/1w72wjp5mg/adv3Lite/tutorial/hints.htm
[kt-hints]: https://kindatechnical.com/game-development/tutorials-onboarding-and-player-guidance.html
[gag-help]: https://gameaccessibilityguidelines.com/include-contextual-in-game-helpguidancetips/
[gag]: https://gameaccessibilityguidelines.com/intermediate/
[xag]: https://devdocs.xbox.com/build/game-principles/accessibility/xbox-accessibility-guidelines.md
[mayer]: https://ugc.futurelearn.com/uploads/files/7d/d6/7dd6188d-c343-4311-b064-ac98d2c95abc/Multimedia_Principles._R._E._Mayer__2020.pdf
[mayer12]: https://services.dartmouth.edu/TDClient/1806/Portal/KB/PrintArticle?ID=171655
[wee]: https://en.wikipedia.org/wiki/Worked-example_effect
[tm-clt]: https://www.teachermagazine.com/au_en/articles/cognitive-load-theory-teaching-strategies
[arrse]: https://www.arrse.co.uk/community/threads/lesson-observation-form.121681/
[usmc]: https://www.tbs.marines.mil/Portals/230/MAIB1010%20DELIVER%20A%20MCMAP%20PERIOD%20OF%20INSTRUCTION_MEDIA_2015.pdf
[drill]: https://www.drillpad.net/DPdig(A).htm
[nmi]: https://jfcnaples.nato.int/nmi/news-room/news/2021n/nato-mission-iraq-instructors-share-teaching-expertise-in-baghdad.aspx
[samizdata]: https://www.samizdata.net/2003/04/on-how-the-british-army-does-i/
