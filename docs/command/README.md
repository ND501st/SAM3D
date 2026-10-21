# Our side's chain of command and the information pipeline: design reference

Written 18 October 2026, after the project owner's direction:

> This game is first person, from the perspective of only the character the player is playing. Intelligence, early
> warning, after-action reports etc. need to feed through organic gameplay, not just HUD updates: officers who meet the
> player in person, intermediaries, radio, messengers, couriers, datalink. A realistic chain of command for the NPCs,
> senior officers included, present on a map of a small city, towns, villages, industrial facilities and countryside.

This folder is that design. The code is `simcore/include/simcore/command/` (namespace `simcore::command`, umbrella
`simcore/command.hpp`); the data is `data/command/chain_pannonia.json`, `places_pannonia.json` and `signal_means.json`;
the research is `docs/research/CHAIN_OF_COMMAND_RESEARCH_2026-10.md`. It extends the crew pillar that knowledge moves only
through instruments and voice (`docs/crew/01_ROLES_AND_KNOWLEDGE.md` §4) from the vehicle to the whole army around it.

| Document | What |
|---|---|
| `01_PRINCIPLES.md` | The first-person rule and what follows from it |
| `02_CHAIN_OF_COMMAND.md` | Who is who: the national sector, the army, the division, the regiment, battery 21, the town's offices |
| `03_MEANS.md` | The means of signal and their properties: in person, runners, despatch riders, couriers, telephones, radio, the warning net, sirens, the post, the news |
| `04_PIPELINE.md` | How a fact travels: origin, audience, the route, the means each hop takes, contacts, losses, papers and summaries |
| `05_PLACES.md` | The map: the city, the town, the villages, the works, the posts in the countryside, and who is where when |
| `06_INTEGRATION.md` | The air war, the crew, the early-warning sources, the EM layer, multiplayer, and how Unreal presents it |
| `07_IMPLEMENTATION_PLAN.md` | What is built, what is next |

## Running it

```bash
cmake --build simcore/build --config Release && ctest --test-dir simcore/build -C Release   # test_command.cpp: 6 tests
```

In the game: start a campaign from the missions menu; the campaign screen's first panel is now **What you know** (only
what has reached the player's character, each line saying who told them, how and when) and the second **Your chain**
(the player's post, superior, contacts, the people below and the coming briefings, visits and courier runs). These panels
are the interim view; the aim is to present each delivery in the world (`06_INTEGRATION.md` §6).
