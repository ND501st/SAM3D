# 1. Anchors: the map as the source of truth (F1)

The project owner's first point (19 November 2026): every placed object takes its place from the map by id, so that
shifting a factory by 2 km on the map moves everything anchored to it. F1 is being built in parallel in the worktree
`SAM3D-wt/f-f1` (branch `tf-f1`). Its design is in that branch's survey commit (`23f2e4e`, "WIP F1 (paused): anchors
survey only") and will be written up as built in `docs/fronts/F1_ANCHORS_AS_BUILT.md`. This file only says what F1
gives the rest of track F and what the rest of the track assumes of it. It does not redesign it.

## 1. What F1 does, in brief

- The country map (`data/maps/pannonia.json`) gets a list of **sites** (id, name, kind, position). With its
  settlements, landmarks and river crossings they make one set of **places**, ids unique across the lists.
- A theatre names the map its places are read from (a `place_map` key, kept apart from `country_map` so the tuned
  district theatre does not start routing its supply lines on roads). A theatre with a place map refuses coordinates
  for its objects, prepared positions and fronts' points; a theatre without one (the tests' inline theatres) keeps
  them.
- Positions stay exact: a site stores the object's old position, so every number the balance was tuned on is
  bit-identical after the move to ids.
- Bridges get their own sites, linked to the computed crossing and checked by a test.
- A test moves one site by 2 km and checks everything anchored to it moves with it.

## 2. What the rest of track F takes from it

| F1 gives | Used by |
|---|---|
| Each front's line and entry points as places on the map | F2.1: the line's stations are measured along it, so moving the line on the map moves the sectors with it |
| Prepared positions as places | F4.2: national SAM units move between prepared positions by id |
| Objects anchored by id | F3.7: a `Troops` object that follows its unit changes its position in the campaign's state (`ObjectState::pos`), never in the theatre's data; the anchor stays the unit's peacetime place |
| One origin for the country and the theatres (46.9 N, 19.7 E) | everything: the line of contact, the ground units and the route service share one frame |

## 3. What track F assumes of F1, and what it does if F1 is late

- Track F's data names places by id where it can: sectors by station ranges along the front's line (not by
  coordinates), a ground unit's peacetime location by place id, its axes by places.
- F2.1 and F3.1 do not wait for F1. Where a place id does not yet exist they take a position as the theatres do today,
  marked in the data for F1's migration script to convert (`"anchor_todo": true`). The sectors are relative to the
  line, so they stay valid when F1 anchors it.
- The ground war's units are new data. They are written from the start against the map's settlements and sites, as
  F1 will leave them.
