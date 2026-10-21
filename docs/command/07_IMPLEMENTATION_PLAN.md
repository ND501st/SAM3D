# 7. Implementation plan

| # | Task | Status |
|---|---|---|
| C0 | Research: command posts, the regiment's officials, the means of signal and their trade-offs, the courier schemes, precedence, signals in a city, the territorial offices (`docs/research/CHAIN_OF_COMMAND_RESEARCH_2026-10.md`); sources registered | **Done 18 Oct 2026** |
| C1 | Data: the chain (38 posts, from the national sector to vehicle 21-1's driver, the town's offices and the village), the places (37: the city, the town, five villages, the posts, the countryside), the means (12), the routines (11); strict loader | **Done** (`data/command/`, `simcore/command/data.hpp`) |
| C2 | The pipeline: facts, audiences, routes, hops, contacts, losses, papers and summaries, the player who passes nothing on; deterministic | **Done** (`info_net.hpp`) |
| C3 | The air war's news into it, and the air war's raids onto its roads and wire | **Done** (`air_war_feed.hpp`) |
| C4 | Tests | **Done**: 6 tests (`tests/test_command.cpp`); the Unreal raid test checks the pipeline in the engine |
| C5 | The campaign screen shows only what reached the player's character, with who told them and how; the chain and its coming contacts; the raid warning as received | **Built 18 Oct 2026** (`USimAirCampaignSubsystem` owns the net; `ASimCampaignHUD`) |
| P1 | **Deliveries in the world**: an NPC officer who walks up and speaks; the telephone in the PU-12M; the radio voice; the despatch rider's motorcycle; papers in a map case; the evening briefing as a scene at the regiment's KP | Next |
| P2 | **The player tells people**: talk, telephone, radio, a written report for the courier; the choice of means and its cost (time, interception); the battery commander's orders to NPC vehicle commanders through it | Next |
| P3 | The pipeline's radio calls feed the air war's COMINT (replacing the posture's "radio discipline" with what was actually said on the air) | Next |
| P4 | Join the crew's knowledge model: a delivered fact becomes a picture entry; the crew's callouts become facts the battery commander can report | Later |
| P5 | Radio hops through the EM layer's nets (link budgets, jamming, ESM) when it is on | Later |
| P6 | Multiplayer: each player's character a post; the server's net sends each client its own deliveries | Later |
| P7 | The district as an Unreal level from `places_pannonia.json`: the city, the town, the villages, the works, the posts | Later (art) |
| P8 | Posts change hands: casualties, replacements, the player's career; the alternate command post under a deputy | Later |
