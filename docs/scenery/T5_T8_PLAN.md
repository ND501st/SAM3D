# T5-T8: trees, clutter, villages and farms, roads, rivers and the position (plan, 4 Nov 2026)

Roadmap phases T5-T8 (`TERRAIN_ROADMAP.md` section 7). T0-T4 are built; this plan makes the plain *read* from 1.7 m and from
the battery: trees, ground clutter, the villages and farms, the roads, the rivers and the position itself. Written for
efficiency: what is reused, what is done in what order, what waits on the owner, and what each step is checked by.

## 1. What is already there (so none of it is rebuilt)

- **A part slot with variants.** `data/scenery/assets.json` names each part (`tree_oak`, `house`, `barn`, `flats`,
  `industrial`, ...) with an optional registered model per variant; `FSimMapAssets::MeshFor(part, variant)` returns the
  model or simcore's procedural mesh (`map_meshes.hpp`). A new part or a better model is a data change, not a new draw path.
- **Instanced drawing.** `SimMapActor.cpp` draws every part as hierarchical instanced meshes from simcore's lists (houses,
  barns, trees, wood masses, bridges). New kits join it; they do not need their own architecture.
- **simcore decides where, Unreal draws.** Positions, footprints and heights are simcore's (they block sight and are
  tested headless); Unreal only presents them. Nothing in T5-T8 changes what a model *is* to the simulation unless a step
  says so.
- **The check harness.** `tools/terrain_views.py run --label X --against Y`: V1-V11 with photographs and cost per view.
  Baseline now (v7out2): GPU 5.9-7.7 ms, render 8.4-10.8 ms per view, game 5.2-5.5 ms.
- **The register's rules**: a model is added only with the owner's say-so, file by file; redistributable ones may be
  committed, Fab and Epic ones live in ignored `Content/ThirdParty/`; `tools/check_assets.py` checks.
- Not there yet: a farmyard site kind (V5), dykes, impostors or LODs for trees in the map actor, pole lines, per-instance
  tint, the clutter ring, standing-crop blocks, road surfaces by class.

## 2. Rules for the four phases (what keeps it fast)

1. **Source order, cheapest first:** our own procedural mesh (simcore or Blender by script, no licence, no download) ->
   Poly Haven CC0 (scriptable, no account) -> CC-BY Sketchfab (the owner approves each; a login to download) -> Fab and
   Megascans (the owner claims them on the machine; per-machine, never committed). Anything the roadmap marks "must be
   built" (the kocka, the long house, TSZ sheds, water towers, the B12 pole, the slab fence) is our own: they are boxes,
   hips and prisms that the procedural builder already does.
2. **simcore first, Unreal second.** Layout, footprints and rules are written and tested headless (minutes), then Unreal
   is edited once per phase and built once (a UBT build is 8-26 min and builds queue on one mutex, so Unreal lanes never run
   in parallel; simcore lanes can).
3. **One Unreal build and one views run per step**, not per edit: batch the C++, the material builder and the data first.
   Every run has a time limit (the hung run of 4 Nov).
4. **Each step ends with its views and a cost line.** Proposed budget (owner to confirm): no step adds more than 1.5 ms of
   GPU at any view, and no view passes 12 ms GPU or 16 ms render by T9. Measured on this machine's GPU until a target GPU is named.
5. **The obstacle is simcore's, the picture is Unreal's.** A tree model is scaled so its crown matches the simulation's
   obstacle footprint (unit tree: one tall, one across); the balance runs then stay identical, which `ground_check` and the
   sight tests prove after every phase. A phase that does change sight (dykes) is reviewed by Opus.
6. **Generate the ordinary, hand-place the special**, as data (the roadmap's rule 10): kits are rules over the settlement,
   the farm and the road, never a placement list per building.

## 3. T5: trees (3 steps, about 3 Unreal cycles)

| Step | Work | Model | Checked by |
|---|---|---|---|
| T5.1 | **Tint and variety, no downloads.** Per-instance custom data on the tree instances (species tint by date, a lean and a scale draw from the tree's own hash), so one mesh is many trees; October's locust yellow and poplar gold. Measured on the current procedural trees. | Sonnet | V1, V7, V8 with the grey models now coloured and varied; cost |
| T5.2 | **Rings.** The tree meshes already have four LODs (8, 6 and 4 sides, then the crown alone as a spindle: `TreeMesh`) and the woods' masses exist; what is left is to check the cull and LOD distances against the views (full to 1.5 km, the spindle to 6 km, masses beyond) and keep the TV sight's far trees (field-of-view-scaled culling). No octahedral impostors: they need an editor baking plugin for a gain the spindle gets at this range. | Sonnet | V10 and V11 (far trees), cost at V7 |
| T5.3 | **Real models for four species** (oak, black locust, hybrid poplar, pine) if the owner approves a source (section 7). Registered per part; scaled to the obstacle's footprint; autumn tint through the custom data of T5.1. If none is approved, T5.1-T5.2 stand and the procedural trees stay. | Sonnet; Opus reads the footprint match | V1, V7, V8 against P-photos; `ground_check`, sight tests, `air_war_balance` unchanged byte for byte |

## 4. T6: clutter and crops (3 steps, about 3 cycles)

Reeds belong here (they are a ring of clutter at the marsh edge), not with trees.

| Step | Work | Model | Checked by |
|---|---|---|---|
| T6.1 | **The camera's ring.** A ring of instances to about 100 m from the camera, generated from simcore's land cover by the same cell hash on every client (no pop on a return, no network): grass tufts on meadow and verges, maize stumps on `maize_stubble`, weeds, reeds at the marsh edge. Rebuilt when the camera has moved 25 m, in chunks, no shadows. Stubble rows stay the ground material's job (the layers exist). | Sonnet | V1 at 1.7 m reads as a field; cost of the ring |
| T6.2 | **Standing crops as field blocks.** Fields in a standing state (maize 14% in October, sunflower, summer wheat) get a block mesh from `FieldAt` (centre, heading, width and length: a rotated box with a crop texture and a normal map of rows), only within about 3 km; plants as instances only in the near ring. | Sonnet | V1, V10 with a standing maize field; cost |
| T6.3 | **Check and tune** the ring and blocks against the budget; the TV sight's field. | Sonnet | the cost page against `t5`; V1, V6, V10 |

## 5. T7: villages and farms (5 steps, about 5 cycles; the largest phase)

Own procedural art throughout; building surfaces from Poly Haven CC0 textures (`painted_plaster_wall`, `ceramic_roof_01`,
`asbestos_sheet_02`, `corrugated_iron_02`, `brick_wall_006`: section 4.3 of the roadmap), the colour a per-house parameter.

| Step | Work | Model | Checked by |
|---|---|---|---|
| T7.1 | **The building types in simcore** (`map_meshes.hpp`, data): the kocka (10 x 10 m, pyramid roof, two-colour render), the gable-end long house, TSZ livestock sheds and dryers, a water tower (40-45 m), a silo, a village church; each a variant of an existing part (or a new part) with its footprint. A headless test per type: size, footprint, obstacle height. | Sonnet | tests; the model gallery render |
| T7.2 | **Layout rules.** Street villages on long strip plots (house at the street end, gable to the street, farm buildings and garden behind), tanyas in their own islands of trees, the TSZ farmyard (adds the missing farmyard site kind: V5 stops being missing), the landmarks at the right places (a tower in every town, silos at stations). Data rules over the settlement, its heading and the farm block; buildings keep their obstacle footprints. | Sonnet | V2, V3, V5, V6; sight and balance tests |
| T7.3 | **Poles and wires.** Pole lines along roads and into villages (spacing 35-40 m, the B12 concrete pole as our own mesh), positions laid in simcore, wires drawn as one thin mesh per span. Pylons for the high-voltage line. | Sonnet | V4, V10 |
| T7.4 | **Props.** Fences (the slab fence, wooden), wells and the shadoof, haystacks and bales, parked period vehicles at farms, bus stops, road signs: small instanced kits placed by the farm and road rules. | Sonnet | V2, V6 |
| T7.5 | **Surfaces and tune.** The building materials on the new types, roof and wall colours per house, the verges at the village edges (T3.3's open item), the cost against `t6`. | Sonnet | V2, V3, V6; cost |

## 6. T8: roads, rivers and the position (4 steps, about 4 cycles)

| Step | Work | Model | Checked by |
|---|---|---|---|
| T8.1 | **Road surfaces by class.** Asphalt (side streets 2.8-3.2 m, main streets 3.5-4.5 m), concrete slabs with joints drawn from the road's UV, gravel, earth tracks; verges and ditches as material work (a darker verge strip, a ditch normal-map line), not geometry. Widths from the data by class. | Sonnet | V4, V2 |
| T8.2 | **Dykes and the floodplain.** A *line* ground feature (the map has raised circles only; ridges were chains of them) for flood dykes 4-6 m high with a crest road, the floodplain wood inside, reeds at the oxbows (T6's ring). This **changes the ground and so the sight lines near the Rava and the bridges**: a land change, the sight and balance re-read, and an Opus review before it is kept. | Sonnet builds; **Opus reviews** | V8 (the eye then stands on the dyke, not the bank); `ground_check`, `air_war_balance`, sight tests |
| T8.3 | **The position's works in simcore** (the roadmap's rule 1: game logic in simcore). The pad, the revetment's parapets and the camouflage net's poles become simcore objects from the unit's footprint; `ASimPositionWork` keeps drawing them and the net's toggle. This also decides the net-height issue found in V7 (the net is 4.2 m up; the radar's antenna is higher). | Sonnet | V7 from outside and at the battery's own eye; tests |
| T8.4 | **Check and tune** against the budget. | Sonnet | cost page against `t7`; all views |

## 7. What needs the owner (one batch, so nothing stops mid-run)

1. **Tree models** (T5.3): the registry allows a model only with the owner's say-so. Proposal: Poly Haven CC0 pine first
   (scriptable, no account); the four Megaplants/Megascans picks need you to claim them on Fab on this machine (free,
   per-machine, never committed); or approve the CC-BY Sketchfab alternates (login). If you approve none, T5 ends at T5.2.
2. **Textures** (T7.5, T8.1): the Poly Haven CC0 list above plus `farm_soil`, in one fetch run (`tools/fetch_textures.py`,
   downloads: your OK first).
3. **The budget** in rule 4 (1.5 ms a step, 12 ms GPU a view), and whether this machine's GPU is the reference.
4. **Own art for the period buildings** (the plan's default). If you would rather have photographed models, T7.1 grows by
   the `model-from-photos` process per building type (about a cycle each).

## 8. Order, parallel work and rough effort

Sequence: **T5 -> T6 -> T7 -> T8**, each ending in its own commit and its own views run (`t5`, `t6`, `t7`, `t8`).
Parallel where it is free: the simcore halves (T7.1-T7.2 layout, T7.3 pole lines, T8.2 dyke feature, T8.3 position works)
can be written and tested while an Unreal build or a views run is in progress, because simcore builds in a separate
folder. The Unreal edits and builds stay one lane (the build mutex, `SimMapActor.cpp` shared by every step).

Effort: about 15 Unreal build-and-render cycles in all (each 15-35 min of waiting, the hang excepted), so 5-8 hours of
waiting spread over the work, plus the simcore and data writing between them. Rough, not measured; a header change in
`views.hpp` cost a 26-minute build today, so the simcore headers Unreal includes (`terrain_map.hpp`, `map_meshes.hpp`) are
touched once per phase, not per step.

Models: Sonnet for every step by default (volume work); **Opus for T8.2** (the dykes change the ground the sight lines
read, as T4.2's rows did) and for a short review of the whole set before each phase's commit.

## 9. Risks

- **Sight lines** (T5.3, T7.2, T8.2): a model whose crown or a building whose footprint differs from simcore's obstacle
  makes a tree look like it blocks sight and not block, or the reverse. Rule 5 and the byte-for-byte balance check.
- **Instance counts.** The map has 100-plus buildings a village and tens of thousands of trees; T6's ring and T7.3's poles
  add more. HISM culling and the three LODs are the answer; the cost line at each step is the proof.
- **Fab availability and licences rotate** (the roadmap's note): a Fab pick may stop being free the day you claim it; the
  Sketchfab and Poly Haven alternates are listed per part.
- **Look vs statistics**: Pannonia is fictional; the photographs and the eye judge, not the numbers.
- **The leftovers of T3/T4** (`T3_T4_PLAN.md`, "Left, as found"): tree rows' clearance at row ends, rivers dotting the far
  cover, no strips in the capital's detail area. They are small and are picked up in T7.2 and T8.2 where they touch.

## 10. T5.1 built (4 Nov 2026)

- **simcore** `scenery/tree_tint.hpp` and `data/scenery/tree_seasons.json`: each tree's colour multiplier on the day of the
  year (the kind's onset and full-colour days, each tree's own turn day from its position plus or minus a spread, its own
  brightness); the woods' masses take the mix's mean so a tree and the mass behind it meet at one colour. Four tests
  (`test_tree_tint.cpp`): summer before the onset, autumn after the full day, never backwards, the pine unchanged, on 15
  October the locust mostly turned and the oak later and no kind one colour, deterministic, a bad file refused.
- **Unreal**: `tools/ue_build_tree_material.py` makes `M_SimMapTree` (the vertex-colour material times the instance's
  three custom-data floats, laid on green-dominant vertex colours only so a trunk keeps its bark); `BuildTrees` fills each
  instance's multiplier. Without the material or the rules the trees are drawn as before, and the log says so once.
- **Not done, on purpose**: a random lean. A tilt moves a crown off the obstacle's footprint (plan rule 5) for a gain the
  tint already gives; sizes already vary (simcore draws each tree's height and crown). Leaf drop is not drawn: a tree keeps
  its autumn colour through the winter (the scenarios are October; a later phase).
- **Run `t51`**: the trees along the roads and at the horizon are gold and yellow on 15 October (V10, V9, V7); the oak
  groves green-brown. Cost against `v7out2`: within 0.15 ms GPU at every view (budget 1.5 ms). Sight lines untouched
  (no simulation data changed): 1036 simcore tests pass. The gold is by eye and may want a little less saturation when the
  owner sees it against the photographs (`autumn` in the data file).
