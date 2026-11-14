# 1. The enemy: war aims, target picking, and why raids come to battery 21

The adversaries and their officers are as built (`docs/air_war/01_FACTIONS.md`, `02_AIR_STAFF.md`, `04_PLANNING.md`;
`data/air_war/faction_*.json`). This document states what each one is fighting for and how that becomes the raids
battery 21 meets. It changes nothing in their doctrine. It adds one term to their target picking (§3) and gives war
aims a place in the data and the debrief (§2).

## 1. One enemy per campaign

A campaign is a war with one adversary on its own border, started from the missions menu's campaign box as today
("four borders, four possible wars", `theatre_pannonia.json`). Battery 21 deploys where that front needs it
(`FrontDef::battery_position`). A campaign lasts as long as its data says. It can also end early if the faction's
politics pause it for good: today's pause events (NATO's ceasefire talks, the neutral's mediation, the rebels'
ceasefire for the funerals) may be made final, as a small data change. Nothing more is planned for how a war starts
or ends.

## 2. The war aims

Each adversary's aims, read from its file's description, phases, target weights, bans and politics. In phase 13 they go
into each faction file as an `aims` block. The block is text with the phases it drives, so the debrief's reveal and our
analysts' estimates can name it.

| Adversary (tier) | What it is fighting for | How (its phases) | What it will not do (bans, depth, politics) |
|---|---|---|---|
| **The Rava Liberation Front** (inferior) | Keep the rising alive and seen: supply its fighters, hurt the army in the valley, and show the state cannot protect its own. Every raid is also propaganda | Harassment, supply and propaganda: night harassment by An-2s (40 %), helicopter raids (25 %), small strikes on headquarters and troops (weights 1.4, 1.3), a pilot's-eye reconnaissance | Bomb its own towns; strike deeper than 80 km; lose airframes it cannot replace (a third of an aircraft a day) |
| **The Noric Republic** (near peer) | Hold its border and win a limited war along the line, without letting it grow | Three days of defence of the border (close support, helicopter raids, reconnaissance), then limited raids along the front on troops (1.4), bridges (1.1), depots (1.0) and command posts (0.9), at first and last light | Towns, power stations, factories, headquarters; more than 40 km deep (60 if pressure allows); lose more than an aircraft a day (two in a day, and its parliament orders the raids flown above the light flak) |
| **The Warsaw Pact** (peer) | Take the air in two days, then carry the front's offensive through | The air operation: massed, timed blows on airfields (1.4), command posts (1.3), radars (1.2) and headquarters, through corridors cleared by suppression groups, at a surge tempo. Then support of the front's offensive | Towns; little else: loss tolerance six a day, and a front commander who demands maximum efforts under pressure |
| **NATO** (superior) | Win air superiority, isolate the battlefield, then support the land battle, within coalition politics | The rollback of the air defences until it believes fewer than 45 % still work (radars 1.4, SAM sites 1.3). Then interdiction of bridges (1.2), junctions (1.1), depots and airfields to day 12. Then support of the land battle | Towns, and power until the alliance escalates; prisoners and civilian casualties (very sensitive); ARMs it cannot spare (scarce until the HARM shipment) |

**Our country's aim** is to stay whole: keep the forces, the logistics, the infrastructure and the state working, and
make the enemy pay until its politics stop the war. The battery's part is the object or the troops it is told to cover
(`02`, `03`).

## 3. How the enemy's officers pick targets

The officers decide as they do now (`docs/air_war/04_PLANNING.md`):

1. **The commander** sets the tempo and the apportionment of sorties to mission kinds by the phase, bent by character:
   more DEAD for the methodical, more strikes for the bold and the pressed, more reconnaissance when the picture is
   stale.
2. **The intelligence officer's picture** gives the candidates: strikes on believed targets that are not air defences;
   DEAD on located radar air defences and warning radars (uncertainty under 4 km); hunters and deception against radar
   air defences, located or not; reconnaissance of mobile entries seen more than 8 h ago; BDA of what was struck in the
   last 30 h; close support on troops within 40 km of the front; helicopter raids within 25 km; harassment within 70 km.
   Bans, the no-strike zones and the depth limit remove candidates.
3. **The planner** weighs each candidate × entry × height × time of day by utility: the expected effect (the worth ×
   the belief it is there and working × the chance the aim point is right × the chance the main flights get through ×
   the effect against its hardness), against the expected losses × the loss aversion, plus quirks, tactic memory,
   exploration and noise by skill.
4. **The commander approves or rejects** each package by its expected loss per aircraft; higher authority sometimes moves
   one earlier.

**The one addition: what a target's loss would cut.** Today the worth is the object's value × the faction's weight for
its kind + the grudge. The battery's war adds the share of what the object supports (`02` §1) that its loss would stop,
**as the enemy's picture believes it**:

- each faction knows our objects' roles only through its intelligence. The pre-war survey knows fixed infrastructure
  well. Beyond it: the rebels through their sympathisers and captured radio sets; the neutral through its border
  observers and a border ELINT station; the Pact through its satellites (36 h latency), ELINT aircraft and agents;
  NATO through satellites twice a day, reconnaissance and TDOA ELINT;
- so the Kesmark road bridge is worth more to an enemy that knows the rail bridge beside it is down, and the
  technical battery's siding more to one that knows the battery's reloads come through it. A bridge with a ferry beside
  it that the enemy has not found is worth less than it believes;
- the faction's bans and depth limits are never lifted by this term;
- re-strikes "to prevent restoration" (`mai_tactics_vvs`) follow the repair the enemy's reconnaissance sees, as now.
  They also reach what a rebuild needs: the repair site and its teams, the materials depot, the pontoon park, the
  line that brings them (`02` §3), by each faction's weights for those kinds and within its bans.

## 4. Why raids come to battery 21

A package comes to the battery's area for one of five reasons. Each traces back to an aim, a phase and an officer:

| Reason | Packages | Traced to |
|---|---|---|
| **What the battery covers is a target** | Strikes (with SEAD, escorts, jammers and a reconnaissance lead where the template has them); close support and helicopter raids on covered troops; harassment at night (the rebels) | The phase's apportionment; the object's worth (kind weight, what its loss would cut, grudge); the planner's best way in |
| **The battery itself is a target** | DEAD on a located battery; hunter pairs where one is suspected; deception (decoy waves) to make it radiate | Radar air defences are DEAD candidates once located within 4 km, and hunter and deception candidates always. NATO's rollback runs until it believes under 45 % of our air defences work. The Pact's air operation weights radars 1.2 and SAM sites 1.0. A grudge after losses adds worth: "this one has cost us 3 aircraft" |
| **The enemy needs to see** | Reconnaissance of the battery's area; BDA after a strike | A mobile entry seen more than 8 h ago or poorly located; a strike in the last 30 h; an "imagery man" who will not strike a mobile target without fresh photographs; the Pact's reconnaissance lead 8 min ahead of its strikes |
| **The way to something else passes over the battery** | Any package whose route crosses the battery's rings | The planner's corridor: its entry point, its tactic memory ("the same way in as last time"), the threats it believes are on the route. A battery the enemy does not know of is not on its map of threats |
| **To cover a strike elsewhere** | A stand-off jammer's orbit; a feint pair from another entry | The template's jammers (one in the Pact's and NATO's strike and DEAD packages); the "feint artist" |

Which of these reach the player is the pacing's choice among the enemy's real packages: those that come within 10 km of
the battery, or are aimed at it, its dummies, or what it defends (`docs/air_war/05_EXECUTION.md` §1). In the continuous
mission they arrive live (`05` §6).

## 5. What the enemy learns from battery 21

As built (`docs/air_war/03_INTELLIGENCE.md`): every second its radars radiate is an ELINT fix for the enemy's collectors in
reach; every launch seen by a surviving aircraft locates it; its radio calls are heard (P3); reconnaissance photographs
it, or its nets halve the chance; a DEAD strike on a dummy is claimed as a kill. `EnemyKnowledgeOfBattery` measures
the result, and our regiment's estimate of it (`docs/command/06_INTEGRATION.md` §1) is one of the reasons for the next
order (`03` §1).

How far and when the enemy can reach (its depth limit, its sorties, the hours its aircraft can fly, air superiority) is
game logic of its own, and our side's estimate of it shapes our doctrine and the battery's day (`08` §1).

## 6. What battery 21 costs the enemy: its freedom of action

The player cannot change the course of the war. Battery 21 is one battery of many, and the enemy's aims, phases and
targets are its own (§2, §3). What the battery does change is what the enemy must pay to act near it. That cost follows
from its officers' logic as built (`docs/air_war/04_PLANNING.md`), and it lasts only as long as the battery lives and
is believed to be there:

| What the battery's believed threat does | Through | The cost to the enemy |
|---|---|---|
| Raises the expected losses on every route through its rings | `PassKillChance` on each believed threat, its ring widened by half its uncertainty | The planner adds SEAD (one aircraft per believed radar threat, up to the template's maximum, while ARMs are left above the hoarder's reserve), decoys and jammers: aircraft that carry no bombs |
| Makes some packages not worth flying | The commander's approval: the expected loss per aircraft against `0.04 + 0.16 boldness + 0.08 pressure + 0.06 risk` | Packages rejected; after two rejections the planner stops offering that kind |
| Pushes routes and heights away | The utility's loss term, and the planner's memory of losses on a way in | Longer routes, other corridors, other heights: what the battery covers is harder to reach |
| Draws sorties onto itself | DEAD, hunter and deception packages against radar air defences, more under a methodical commander | Sorties that strike none of the country's targets |
| Draws ARMs | SEAD and hunters fire on their warning receivers | A scarce stock spent (NATO's 100 and six a day, until the HARM shipment) |
| Downs and damages aircraft | The fight | Squadrons shrink until replacements come; damaged aircraft are out for `damaged_return_h`; stress lowers the commander's tempo; losses beyond tolerance cost support at home and bring their events (raids flown above the light flak, the losses reviewed) |
| Keeps the enemy in its suppression phase | A phase that ends on the air defences ends only when the enemy believes few enough of them work (NATO's 45 %) | Its interdiction of the bridges and junctions waits while battery 21, and the dummies believed to be batteries, count as working |

None of this needs a kill. A battery that survives and is believed to threaten does it; a battery destroyed stops
costing anything. That is why the battery's survival is an objective of its own (`03` §6), and why our doctrine against
the stronger adversaries puts survival first (`08` §2). How the costs are measured: `05` §14.
