# T3 and T4: the plain's layout and one colour for the whole view — the plan (4 Nov 2026)

Phases T3 and T4 of `TERRAIN_ROADMAP.md`, designed together because they meet in one place: the fields. T4 lays the
plain out in simcore (collective farm fields, household strips, farm tracks, tree rows, the tanyas' groves, the crop
calendar). T3 draws that layout with exact edges at every distance, out to the far ground, under haze that matches the
weather. Written on Opus as the design; built on Sonnet step by step, each step ending in its check.

**Done when:** the views V1, V3, V6, V9 and V10 against their photographs show large fields with straight edges and no
25 m steps; tracks with their tree rows; the tanyas in their trees; the far ground's fields continuing past the map
proper's edge without a seam; the haze at V7 and V9 matching the day's visibility; the run's costs written beside
`t1`'s; and the sight-line consumers re-read (the trees now block sight). Pannonia is fictional: judged by eye, not
measured (no crop statistics).

## What is wrong now, and why

- **Fields are small** (`parcel_length_m` 250-700 by 80-250 m: 2-17 ha). A 1980s collective farm (TSZ) field is
  100-200 ha (research table T2). From V3 and V10 the plain reads as a quilt, not as the photographs' open sheets.
- **Field edges step every 25 m.** Each field's layer reaches the ground material only through the 25 m land cover mask.
  The material's wander and height blend soften the steps but cannot straighten them.
- **The far ground is another colour source.** Beyond the map proper its tiles are tinted from the country's coarse
  cover, so an edge shows where the two meet (V9, V10).
- **No farm tracks or tree rows**, and the tanyas stand bare. Tree rows block sight, so they must be in simcore's map,
  not only drawn.

## Decisions

1. **Fields are a closed-form function of position, the same in simcore and in the shader.** Today's `FieldParcel`
   already is one: a heading per square, a width per square, and a length and offset per row. T4 keeps that form at TSZ
   scale and changes its hash to a 32-bit integer hash (`Hash32`, written identically in C++ and HLSL), so the ground
   material computes a pixel's field exactly. Every field edge is straight at every distance, at no memory cost.
   Rejected: a runtime virtual texture holding the vector shapes. It is either blurry near the eye at a size that fits
   40 km, or it has to carry layer ids that its filtering mixes. It also adds an engine-feature risk T9 may yet bring.
2. **A field's properties come from the mask at the field's centre.** The shader finds the pixel's field analytically,
   then loads the mask texel under that field's centre, and reads the layer, shade and flags simcore baked there. The
   crop, the calendar and the household-strip flag therefore stay in simcore, and HLSL holds no crop rules. Where the
   centre cell is not farmland (a wood, a road), the shader falls back to the pixel's own cell.
3. **Estates are squares of the map's frame** (`estate_m`, 4 km), each a collective farm's land with its own heading
   and field sizes. A square's borders are straight lines, so the shader finds the estate by `floor(P / estate_m)`.
   Each estate's numbers come from an **estate table** (a small texture, a texel per estate) that simcore fills. The
   heading can then follow the nearest authored road (fields line up with roads, as in the photographs) without the
   shader knowing any roads.
4. **Non-field cover keeps the 25 m mask.** Woods are 100 m blocks on the map's grid (`block_m` 100, snapped), so their
   edges fall on cell edges exactly. Towns' gardens, military ground and water keep the cell blend; roads and tracks
   are drawn as strips over it. The 25 m steps that remain are at gardens and site edges, which are rarely seen from
   the views.
5. **Tree rows, tracks and groves are the map's own** (`terrain_map.hpp`), because they block sight or carry vehicles.
   Fields, crops and the calendar are the dressing's (`scenery/`, presentation). Everything new is laid **after** the
   existing layout, from its own seed streams, so no existing box, wood or farmstead moves. The crops change (expected).
6. **The far ground draws the same fields.** Its tiles get a light version of the ground material: the same field
   function and estate table, a far mask of 250 m cells from the country's land cover (fields are about 1 km, so their
   centres find the right cell), and each layer's average colour (the albedo arrays' last mip, as the near material's
   far fade already uses). Near and far are one colour source, so the edge at the map proper goes.
7. **The haze is already physical** (R6, R7: Koschmieder's extinction from the weather preset's visibility, split
   between the atmosphere's Mie and the height fog, in `ASimSkyWeatherActor::ApplyWeather`). T3 checks it rather than
   rebuilding it: a contrast measurement at V7 and V9 (step T3.4).
8. **Generated tracks are drawn, stamped as road cover and planted with rows, but not yet routed.** Adding them to the
   route service would change every route and its tests. The crews already drive across fields (`nav/ground.hpp`). The
   tracks go into the route graph later, as their own step with `nav_check` re-run.

## Steps

### T4.1 The estates and the fields (simcore, Sonnet)

- New `simcore/include/simcore/scenery/farmland.hpp`, holding `Hash32(uint32)` (one fixed integer mix, for example
  lowbias32) and `HashCell(seed, ix, iy)`. A test pins a table of values, which `tools/ue_build_ground_material.py`'s
  HLSL must reproduce (the builder prints its own table under `SAM3D_GROUND_PATTERN=1` for an eye check).
- `EstateTable BakeEstates(const MapData&, const DressingRules&)` gives each `estate_m` square:
  - a heading: the bearing of the longest authored road inside or within 1 km of it, folded to [0, π), with a few
    degrees' draw; or a draw where no road is near;
  - a field length and width drawn from `field_length_m` (900-1500 m) and `field_width_m` (700-1100 m);
  - a strip width (`strip_width_m`, 20-40 m) for the household strips.
  It covers the map proper and the far ground out to `far_km`.
- `FieldAt(table, e, n) -> FieldCell {id, heading, centre_e, centre_n, strips}`: the estate's rotated frame, rows
  across, each row its own length and offset by `HashCell`, as `FieldParcel` does now. In a household-strip field, the
  field is cut across into strips of the strip width, each its own id.
- `FieldParcel` and `FieldHeading` keep their signatures as wrappers over `FieldAt`, so `views.hpp`, `ground_layers.hpp`
  and their tests are untouched. `parcel_length_m`, `parcel_width_m` and `parcel_heading_cell_m` are replaced in
  `data/scenery/map_dressing.json` by the new values. All game values, labelled `sam3d estimate`.
- **Household strips:** a field is cut into strips when its centre lies within `strip_reach_m` (400 m) of a village's or
  town's edge (`MapSettlementDef` radius). `BakeCover` sets a flag the mask carries (bit 7 of B; the shade keeps 7 bits).
- `BakeCover` takes each cell's crop from `FieldAt`'s id rather than the old parcel.
- Tests:
  - fields are 70-200 ha away from the villages and strips 0.4-1.6 ha near them;
  - a field's centre lies in the field;
  - two points either side of an edge get different ids, and a point's id never depends on the grid it is asked from;
  - the estate headings within 1 km of a road follow it within the draw;
  - the old wrappers still give the same heading and parcel at a point.
- Check: `python tools/terrain_views.py run --label t4a --against t1`. V3 and V10 show big fields, still stepped,
  because T3 is not built yet.

### T4.2 Tracks, tree rows and groves (simcore `terrain_map.hpp`, Sonnet; Opus reviews the sight lines)

- **Farm tracks** (dűlőút): along each estate's row lines (the fields' long sides) and its borders, cut where they
  meet woods, settlements, water and sites. Added to the layout as `MapSurface::Track` strips 4 m wide and stamped as
  road cover. Not in the route graph yet (decision 8).
- **Tree rows:** `MapPart::TreeRow` boxes along a share of the tracks (`row_share`, 0.3) and along main and secondary
  roads (`road_row_share`, 0.5). The rows break where a noise says so, are 12-22 m tall and 7 m wide, and stop 30 m short
  of junctions. They are opaque (`Opaque`), so `ObstacleTop` and the map's obstacle grid see them; R6's third pass made
  rotated footprints exact. **None within `site_clear_m` (600 m) of a site**, so the battery positions keep their
  sectors.
- **Tanya groves:** each farmstead gets canopy boxes 25-40 m deep on two or three sides (the photographs' island of
  poplar and locust), 12-18 m tall.
- Dressing: `PlaceTrees` plants a `TreeRow` as one line of trees 6-10 m apart (poplar or locust, a species per row by
  its draw), never above the row's obstacle height. That is the same rule the woods keep.
- Rules in the map file, not the dressing: a `"farmland"` block in `data/maps/kesmark_range.json` (`row_share`,
  `road_row_share`, `site_clear_m`, the grove sizes). They change sight lines, as `canopy_m` does.
- Tests: no row within `site_clear_m` of any site; no row on water or in a settlement; every row's trees under its
  obstacle top; the existing layout's boxes unchanged (counted and hashed before and after); a sight line across a row
  is blocked, and one through a gap is not.
- **Sight-line consumers re-read (Opus):**
  - `ground_check data 3600`;
  - the battery fire model's tests;
  - `air_war_balance data 20 7` against its last numbers (`docs/air_war/09_BALANCE.md`);
  - `helper_coverage` for the Osa's search and lock missions;
  - the TV and radar horizon from `bty21` drawn before and after (`tools/map_view.py`).
  Differences are written into `09_BALANCE.md`, not tuned away unless a mission breaks.

### T4.3 The crop calendar (simcore + data, Sonnet)

- `ground_layers.json`'s `summer` and `autumn` give way to a calendar. Per crop there is a list of states, each with
  its layer and a window of days in the year. A field's own day in each window is drawn by its id, so the plain changes
  field by field as a campaign's days pass, the same on every client.
- Mid-October is a mix judged against the photographs (research table T11):
  - fresh ploughland;
  - harrowed and sown ground with a faint green (a new layer, `sown`: Poly Haven `farm_soil`, fetched with the
    others);
  - maize stubble, and some maize still standing (land cover colour until T6's field blocks);
  - wheat stubble mostly ploughed under.
- `GroundSeason(world)` becomes the scenario's day of the year; `BakeGroundMask` takes the day.
- Tests: on 15 October every crop's state is one of its October layers; a field's state never goes backwards within a
  year; two clients bake the same mask.

### T3.0 First, the green ground (Sonnet, ten minutes)

Render the views once with `SAM3D_GROUND_PATTERN=1` (`TERRAIN_ROADMAP.md`, "What the baseline views show"). If the
flat cells are a layer the pattern shows, fix that layer. If they are the land cover's colour, the `Has` lookup is at
fault: fix it before T3.1 changes the same shader.

### T3.1 Exact field edges in the ground material (Unreal + the builder, Sonnet)

- `ASimMapActor` makes the estate table texture (`PF_A32B32G32R32F`, one texel per estate: heading, length, width,
  strip width; nearest, uncompressed) and passes it, with `estate_m` and the table's origin, as parameters.
- `GROUND_HLSL` gains `FieldAt`, a port of simcore's (`Hash32`, the estate frame, the rows):
  - A field pixel takes its layer, shade and heading from the mask at its field's centre (decision 2). The heading
    comes from the estate, so furrows turn exactly at field edges.
  - Non-field cells blend as now.
  - Where a field meets a non-field cell, the cell blend decides, as now.
  - Two fields meeting give a hard edge, softened by 0.5 m of the height blend, so the textures interlock, not a
    staircase.
- The 25 m wander stays for non-field cover only.
- Check: V3 and V10 show straight field edges; V1's furrows turn exactly at the edge; the pattern mode shows the field
  ids as flat colours with straight borders. The costs go beside `t4a`'s and `t1`'s.

### T3.2 The far ground's colour (simcore + Unreal, Sonnet)

- simcore bakes a far mask from the country's land cover (`CountryMap::LandCoverAt`) at 250 m cells over the far
  ground. Farmland cells take their field's layer via `FieldAt` at the cell's centre; other covers take their layer.
  The map proper's 25 m mask wins where both exist.
- `M_SimGroundFar`: the same field function and estate table, the far mask, and each layer's average colour (the
  albedo arrays' last mip), with the land cover's tint where the country has no layer. No detail textures, normals or
  furrows: it is 500 m to 4 km cells seen from far.
- The far tiles wear it instead of the vertex colours. The towns not drawn keep the town colour.
- The near material's fade to vertex colours (6-12 km) becomes a fade to the same layer averages, so the map proper
  and the far ground meet at the same colour.
- Check: V9 and V10 show no edge at the map proper's border; the far ground's fields continue at the same scale. Cost:
  the far tiles' shading is measured at V9.

### T3.3 Verges and the town edges (Sonnet, only if T3.1's shots need it)

Where the views still show 25 m steps worth fixing (garden edges along the village street in V2 and V3), add the
analytic treatment to a second shape kind: a settlement's plots as strips perpendicular to its street (the same
closed form, along the road). Otherwise leave this for T7, which lays villages out again.

### T3.4 The haze check (Sonnet)

- `terrain_views.py` gains a contrast reading: in each view, the far ground's luminance against the sky at the horizon
  band, beside the expected `exp(-σ d)` for the weather preset's visibility (σ = 3.912 / V) at the distance of that band
  (from the view's eye height and the map's ground).
- V7 and V9 are within the eye's tolerance of it (a factor of 1.5: by eye, not by measurement).
- `visual_range.json`'s `day_m` stays the movers' range and never exceeds the preset's visibility (already capped in
  `nav/session.hpp`: confirm it in a test).
- The TV sight's narrow field is checked once at V7 h180 at 8 km, by eye.

## Order and models

1. T3.0 (Sonnet): the green ground, first, because T3.1 changes the same shader.
2. T4.1 (Sonnet), then `t4a` run.
3. T4.2 (Sonnet), then **Opus** for the sight-line re-read and its balance notes.
4. T3.1 (Sonnet), then the `t3a` run against `t1`.
5. T4.3 and T3.2 (Sonnet, independent of each other), then T3.4 (Sonnet).
6. **Opus** reviews the whole (`/code-review`) before the commit, then the owner looks at the page.

Come back to Opus early if:
- the HLSL field function disagrees with simcore's at a pinned point after two fixes;
- a sight-line test or balance run moves by more than its usual spread;
- the far ground's material costs more than 0.5 ms at V9.

## Costs expected

- Near ground: about 30 more ALU instructions and 2 more loads per pixel (the field function, the centre texel, the
  estate texel). Nothing removed.
- Far ground: from vertex colours to 2-5 loads and one averaged sample per pixel. It covers most of the screen at V9,
  so measure it.
- simcore: the estate table is about 75 by 75 texels. The far mask is about 1200 by 1200 cells (6 MB). Tree rows add
  a few thousand boxes to the obstacle footprints (R6's index takes them).

## What was built, and where it differs (4 Nov 2026)

**T3.0** found V8's flat green (the map actor drew the marsh as slabs over the ground: fixed) and left V7's open
(`TERRAIN_ROADMAP.md`, "What the baseline views show").

**T4.1**, built in first form:
- `FieldAt` in `scenery/map_dressing.hpp` is the closed-form field function (32-bit `Hash32`/`FieldHash`, pinned by a
  test for the shader's port): estates of `parcel_heading_cell_m` (4 km), fields of 84-200 ha (`parcel_length_m` 1200-2000
  by `parcel_width_m` 700-1000), a straight edge at every distance. `FieldHeading` and `FieldParcel` are wrappers over
  it, so the views, the ground layers and the cover raster needed no new calls.
- Household strips: a field whose centre lies within `strip_reach_m` (600 m) of a settlement's edge is flagged
  (`CoverGrid::strips`, the mask's B bit 7; the shade keeps B's low 7 bits). The material gives each strip (30 m across
  the furrows, `StripM`) a shade of its own, first form; a strip's own crop and layer are for later.
- Differences from the plan: **no estate table** (an estate's heading, width and draws are hashes of its square, so
  the shader needs no texture for them: T3.1 is simpler); **the heading does not follow the nearest road yet** (that
  needed the table; to be added as a hash-and-snap if the views ask for it); the nominal field centre is in its own
  field for 86% of fields (an estate's border cuts the rest), so T3.1's shader reads the mask at the centre only when
  `FieldAt(centre).id` matches and falls back to the pixel's cell otherwise.
- The first render (`t4a`): V3 and V10 show large cut fields and strips at the village's edge; the field edges still
  step at 25 m (T3.1).

**T3.1**, built in first form (4 Nov 2026):
- The ground material's HLSL has a port of `FieldAt` (`Hash32`, `FieldHash`, the estate frame, the row and column), with
  the field grid passed as two vector parameters (`FieldGridA`, `FieldGridB`: the estate, the lengths and widths, the
  32-bit seed as two exact 16-bit halves; `ASimMapActor` sets them from `MapData::Fields()`; no "farmland" block, no port).
  No estate texture was needed (T4.1's finding).
- A pixel takes its field from the function at its exact position, reads the mask at the field's centre and, when that
  cell is farmland and `FieldAt(centre)` is the same field, gives every farmland cell of its four the centre's layer,
  shade, strip flag and the estate's exact heading. Two fields meet along a straight edge; where a field meets other
  cover, the 25 m cells and their wander decide, as before. Where the centre lies outside the mask, off farmland or in
  another field (about 14%: an estate's border cuts the field), the cells keep their own texels as in T4.1.
- The mask's A is now 0 off the fields, 128 on a field, 255 on a field whose layer turns with the furrows (the shader
  finds a field's own cells by it).
- Not done: the 0.5 m softening of the edge between two fields (the edge is hard, one pixel); the pattern mode was not
  re-read. The views `t31` (against `t4b`): V3 and V10 show straight edges, no 25 m staircase, the furrows turning exactly
  at the edge; the costs equal `t4b`'s (GPU 6.0-7.0 ms, the same within 0.1 ms).

**T3.4**, done as far as the views allow (4 Nov 2026):
- `tools/haze_reading.py <run>` reads each view's haze: the views are rendered twice, with the atmosphere and fog and without
  (`run_game(..., extra=["-ExecCmds=show Fog,show Atmosphere"])` into `<label>_clear`), each picture row gets its ground
  distance from the eye height, the pitch and the earth's curvature, and the share of the ground's light that reaches the eye,
  `(L - Ls) / (L0 - Ls)` with `Ls` the sky over the horizon, is set beside Koschmieder's `exp(-3.912 d / V)` for the scenario's
  weather (`cloudy`: V = 20 km). Tests: `tools/tests/test_haze_reading.py` (the rows' distances, a known transmittance recovered).
- **The reading on `t32`:** V3 (300 m up) 0.93 against 0.90 at 0.5-1 km and V8 (5.5 m) 0.89 against 0.87 and 0.76 at 0.5-2 km:
  within 1.2 of the law. V10 (300 m up, the clearest picture of the plain): 0.72, 0.87, 0.77, 0.72 against 0.88, 0.77, 0.59, 0.42 at
  0.5-1, 1-2, 2-4 and 4-8 km: within 1.5 out to 4 km, the far ground 1.7 times clearer than the law at 4-8 km, where the ground's
  luminance is close to the sky's and the ratio is soft.
- **V7 and V9 cannot be read this way**: at 1.7-4 m a kilometre of ground is a pixel or two, and trees and the horizon's hills fill
  those rows. By eye the haze greys the far trees as the photographs do (V9). A measurement there needs a flat, tree-free test
  field; left.
- `VisualRangeM` already caps the movers' range at the weather's visibility (`test_battery_life.cpp`: 40 km clear gives the 8 km
  day range, a 500 m fog gives 500 m). The TV sight's narrow field at V7 h180 at 8 km is not checked: the views do not render the TV.
- The haze is **not changed**: the engine's extinction matches the law where it can be read.

**T3.2**, built in first form (4 Nov 2026):
- `simcore/scenery/far_cover.hpp` (`BakeFarCover`): the country's land cover (`CountryMap::LandCoverAt`: lake and river as water, marsh,
  forest, town, open land) over the whole far ground (the map's `far_m`, 150 km each way) at 250 m cells, 1200 x 1200, as one
  `CoverGrid` ("far") in the map's frame, about a second to bake. Open land is farmland: the field grid is closed-form and goes on
  past the map proper, so each far cell takes its field's crop and the far ground's fields are the map's fields at the same scale.
  The mask, the calendar and the material's exact field edges (T3.1) then work on it unchanged.
- No new material: the far level's tiles wear the same ground material with their own mask and two parameter sets
  (`ASimMapActor::TexturedGround`). The far level: the albedo's last mip from the first metre (no detail textures that far), the
  vertex colours only from 60 km. The near levels: the layers' averages carry to 30 km before the vertex colours take over (it was
  6 km), so the map proper's own edge shows the same colour as the far ground beyond it. Beyond the far level (the country's 1 km
  grid and the 4 km land) the vertex colours stay.
- Differences from the plan: no separate `M_SimGroundFar` and no per-layer average-colour table (the albedo array's last mip is
  that); the far level's fade to vertex colours is 60-120 km, not to the layer averages.
- A new view, V11 (`v11_map_border`, the map proper's east edge from 600 m up, 3 km short of it): fields and woods go on at the same
  scale and colour past the edge, no seam. Cost: V9, V10 and the rest within 0.2 ms of `t43b` (GPU 5.9-7.1 ms).
- Left: the far ground's crops follow the calendar like the near; the far fields near a settlement are not household strips; the
  country's other levels (beyond 150 km) keep the vertex colours; the far cover is not re-baked on a new day.

**T4.3**, built in first form (4 Nov 2026):
- `ground_layers.json`'s `summer` and `autumn` are gone: each crop has `states`, the layer it starts the year with and then
  states each with a window of days (`from_day`, `to_day`, in order, not overlapping). A field enters a state on a day of
  its own inside the window, drawn from its number (`GroundLayerRules::LayerOnDay`, `OwnDay`), so the plain changes field
  by field, the same on every client. `Season` is gone; `BakeGroundMask` takes the day of the year; the actor reads the
  scenario's (`GroundDay`: the sky clock's `day_of_year`, 288 without one).
- The windows are estimates from the harvest calendars of T11: wheat stubble 4-29 July, ploughed 23 Aug-27 Oct, sown 28 Oct
  on; maize standing from late April, harvested 2 Sep-22 Oct, then ploughed; sunflower harvested and ploughed 7 Sep-27 Oct;
  a ploughed field sown 27 Sep-17 Nov. On 15 October: 14% of the maize still standing, 82% of the wheat ground ploughed, 37%
  of the ploughed fields sown (printed by the test).
- A new layer, `sown`, wears the ploughed texture until Poly Haven's `farm_soil` is fetched (a download: the user's say).
- Tests: every crop's state on 15 October is one of its October layers, the shares above, a field's state never goes back
  within a year and starts the year in the first state, two bakes are equal, the loader refuses overlaps and bad windows.
- The render (`t43b`): V10 and V3 show the patchwork of ploughland, darker ploughland and stubble. Standing maize and
  sunflower dry out before the harvest (a `dry_stalks` state in August-September on the stubble texture, tan), so mid-October
  shows no summer green (the first render, `t43`, had the maize in its flat summer green). The sown ground has no green yet:
  it wears the ploughed texture until `farm_soil` is fetched. The mask is baked once at the start, so the days of a
  campaign do not yet change the ground; a re-bake on a new day is left.

**T4.2**, built in first form (4 Nov 2026):
- **The field grid moved to the map** (`simcore/farm_fields.hpp`, the map file's `"farmland"` block): the tracks and tree
  rows block sight, so they and the fields they follow are the map's, and the dressing's `parcel_*` values are gone.
  `FieldAt(grid, e, n)`; the cover raster carries the grid (`CoverGrid::fields`); the pinned hash test is unchanged.
- **`LayFarmland`** (`terrain_map.hpp`), after everything else, from the grid's hash: farm tracks (`farm_track_<n>`
  strips, 4 m, `MapSurface::Track`) along half the fields' long sides inside the map proper, cut where they meet woods,
  marsh, water, buildings, sites, settlements and roads; `MapPart::TreeRow` boxes (7 m wide, 12-22 m) along 30% of the
  tracks and 50% of the main and secondary roads; a grove of three belts round each tanya (12-18 m). None within 600 m
  of a site's centre. The existing layout is unmoved (the pinned layout test passes with the rows skipped).
- **Planting** (`PlaceTrees`): a lattice of poplar and black locust (some oak in a grove) in each row, under the box's
  height. The map actor does not draw the row boxes (it draws their trees).
- Kesmark: 412 tracks, 3,523 row and grove boxes. The first render (`t4b`): tree rows along the main road and on the
  horizon from V1 and V10, as in the photographs.
- **Not done:** rows stop 30 m short of junctions (they may cross a track); the tracks are not in the route graph; the
  fields' heading does not follow roads. The Opus re-read is done (`docs/air_war/09_BALANCE.md` §13): no check moved, but the rows raise battery 21's low-level horizon in two thirds of its directions; so the positions stand on low rises and the rows keep 2 km off them (§13, the battery on a rise): the battery now sees a 50 m target at 20 km all round.

## The review (Opus, 4 Nov 2026)

Two readers went over the whole set (simcore and the data; Unreal, the material builder and the tools). Fixed:

- **Winter wheat stood in January.** The calendar starts each year at a crop's first state, and wheat's was the standing crop,
  so from January to early July every wheat field stood ripe. Wheat now starts the year sown (the autumn's winter wheat) and
  stands from days 100-130; October is unchanged. A crop whose last state differs from its first still changes on 1 January
  (the `ploughed` crop: sown in autumn, ploughed from New Year): only a session across midnight on 31 December sees it.
- **The far-cover test could not fail.** It compared the far crop with the expression the bake itself uses; it now compares
  the far cell's field and crop with the map proper's own cell there (every field the same, 95 % or more of the crops: the
  25 m cell may lie across an edge).
- **The far ground and the map proper faded to the vertex colours over different distances** (30-60 km and 60-120 km), a
  step where they meet more than 30 km off; both now fade over 60-120 km.
- **Two probes read only the first value of their lists** (`-SimShotHide=a,b`, `-SimAtmoGround=R,G,B`: `FParse::Value` stops
  at a comma unless told not to). Fixed; what that changes in V7's findings is in TERRAIN_ROADMAP.md.
- **The debug branch indexed local arrays at run time**, which can put them in scratch memory for every pixel; unrolled.
- **The haze check read views aimed at their anchor as level** (V5, V6, V9); it now says it cannot read them.
- **The loader** refuses tree rows or groves of no height and fields longer or wider than their estate.

Left, as found:

- Site clearance is tested at a row's or a grove belt's centre: a farmstead 2000-2050 m from a position could put a belt's
  edge inside `position_clear_m` (none does).
- A tree row's draw is numbered by its run over the whole map, so one new building or road moves every later row (clients
  still agree; the rows block sight, so a map edit changes the sight lines further than it looks).
- Rivers come out of the far cover as scattered cells (a 250 m cell is river only when its centre is in the channel).
- The capital's detail area has no household strips (`BakeCover` reads the map's settlements, not the detail area's).
- The far cover is cached per map and country file for the editor session; an edit to the JSON needs the editor restarted.
- The `MapFrameLink` between the country and the map is the identity today (the same origin), so it is untested in practice.
