# Weapons effects, munition models and sound (roadmap FX)

Contrails, missile and bomb models, their employment, explosions, tracers and flak, lights and sound that travels at the
speed of sound, drawn in Unreal at no cost to the frame. The simulation decides what happens; this layer only shows and
sounds it, and never feeds anything back (first person: the crew still learn what happened only through their
instruments, their eyes and ears, and the people who tell them).

## 1. What other games taught us (sources in section 9)

| Idea | From | Used here |
|---|---|---|
| Missile smoke lifetime per type, 30-60 s, not 10 s; smoke independent of draw-distance settings | DCS player complaints | `munitions.json` `smoke_s`; trails are ribbons, never culled by a draw setting |
| Contrails that pop with zoom; "the farthest LOD is a white line" | DCS, MSFS | a contrail is one ribbon at every range, thinned, never culled while on screen |
| Contrails for every aircraft by its own air, not the player's | MSFS 2020's bug | Schmidt-Appleman per aircraft (`fx/contrails.hpp`) |
| Distance-banded sound sets (near, far) crossfaded, a moving low-pass for distance | Arma 3 (`CfgSoundShaders`, `distanceFilter`) | near/far samples per event; the low-pass from ISO 9613-1 (`AbsorptionCutoffHz`) |
| Speed-of-sound delay for explosions *and* moving sources; supersonic crack only where the cone passes | Arma 3 OPREP, IL-2 | retarded-time `Hear()` per source; `BoomWatch` |
| Doppler as a pitch multiplier computed in C++ (UE's delay-based Doppler is broken, UE-393204) | Epic forum | `Reception::doppler` drives `SetPitchMultiplier` |
| Doppler from a smoothed, clamped listener velocity, zeroed on a camera jump | DCS bug report | listener velocity filtered; jumps over 2 km in a frame reset the tracks' listener |
| HDR audio: a loudness window, quiet sounds culled as a side effect | Frostbite, Wwise | `LoudnessWindow` |
| Photometric brightness for flashes, particles lit by explosions | War Thunder, Battlefield 3 | emissive in a shared scale; the light pool lights nearby smoke |
| Flipbook billboards for distant explosions; one flipbook for every size, scaled by W^1/3 | realtimevfx, Hopkinson-Cranz | the impostor tier |
| Tracer: stretched, velocity-aligned, a minimum screen size | Unity, Arma `tracerScale` | tracer streak sprites with a pixel floor |

## 2. Shape

```
simcore (engine-agnostic, tested)                         Unreal (presentation)
  fx/acoustics.hpp   speed of sound, absorption, Hear()      USimFxSubsystem    events -> effects; particles, ribbons,
  fx/contrails.hpp   Schmidt-Appleman, persistence                              lights, decals, LOD, budgets
  fx/events.hpp      fx::Feed: what happened, for show       USimFxAudio        sources, propagation, booms, HDR window
  fx/explosions.hpp  blast scaling, fireball, sound level    FSimMunitionModels munition meshes from simcore's geometry
  fx/trajectories.hpp missile/bomb/wreck/tracer paths        ASimEntityActor    interpolated, munition models, wrecks
  scenery/munitions.hpp munition shapes -> triangle meshes
data/scenery/munitions.json   every munition's shape, motor, smoke, charge (sourced)
data/fx/effects.json          effect and sound parameters (game values)
art/fx/                       generated textures and sounds (tools/fx_textures.py, tools/fx_sounds.py)
```

Unreal reads the feed with its own cursor (`Feed::Since(seq)`), never the private state of the models. Effects run on
the simulation's clock (paused with it, faster with its time scale); positions are drawn between simulation steps by
velocity (render time = sim time + the step's fraction).

## 3. simcore

### 3.1 `fx/events.hpp`: the feed

```cpp
namespace simcore::fx {
enum class EventKind {
    MissileLaunch,   // our missile leaves the launcher (number, launch position, direction, target)
    MissileEnd,      // our missile's end: burst now (Hit, NearBurst) or flies on to self-destruct (Wide, Early)
    SamLaunch,       // a neighbour's or the bridge's defence missile leaves its site (system, site, target)
    SamEnd,          // that missile bursts (hit or not)
    AircraftDown,    // an aircraft (or helicopter, drone) destroyed: the wreck's start (fire, falls)
    WeaponRelease,   // bombs, rockets, missiles, gun bursts leave an aircraft (weapon id, count, delivery, impact)
    WeaponImpact,    // they arrive (weapon id, count, where, charge)
    CruiseImpact,    // a cruise missile's warhead where it arrives
    ArmLaunch,       // an anti-radiation missile fired (bridge model; EM ARMs are entities)
    ArmBurst,        // an ARM bursts (EM results and the bridge model)
    GunPass,         // a defence's guns fire at an aircraft through a pass (system, site, target, duration, rounds)
};
enum class BurstEnd { Hit, NearBurst, Wide, Early, SelfDestruct, Ground };
struct Event {
    std::uint64_t seq = 0;      // set by Feed::Push, rising from 1
    EventKind kind;
    double t_s = 0.0;           // simulation time it happens (a future impact's time is in `end_t_s`)
    Vec3 position, velocity;    // ENU m, m/s: where it starts and how it moves
    Vec3 aim;                   // ENU: impact point, burst point, the target's position at the time
    double end_t_s = 0.0;       // when it ends (impact, burst, end of a gun pass)
    std::string source;         // who: the target id of an aircraft, a neighbour id, "own" for our launcher
    std::string target;         // at whom/what: a target id
    int world_id = -1;          // the World id (= Unreal entity id) of the aircraft: AircraftDown, WeaponRelease/Impact,
                                // GunPass, SamLaunch/SamEnd (their target); of the carrier (ArmLaunch), the missile (CruiseImpact)
    int number = 0;             // our missile's number (MissileLaunch/End: 1000 + its count for one thrown off), a
                                // launcher's launch count (SamLaunch/End), or the count (bombs, rounds)
    std::string weapon;         // munitions.json id (weapons.json id, "9m33", a system's missile, "zu23" for guns)
    std::string delivery;       // weapons.json delivery for WeaponRelease (laydown, loft, popup, dive, level, standoff, hover_fire)
    BurstEnd end = BurstEnd::Hit;
    double charge_kg = 0.0;     // TNT-equivalent of one warhead (0: munitions.json's)
    bool fire = false;          // AircraftDown: it burns; WeaponImpact: it set something alight
    bool falls = false;         // AircraftDown: it drops out of the sky at once
    std::string model;          // AircraftDown: the basic model id of the wreck
};
class Feed {   // bounded: the oldest are dropped past `capacity` (default 4096); a reader that fell behind skips them
public:
    std::uint64_t Push(Event e);                         // returns its seq
    std::vector<Event> Since(std::uint64_t seq) const;   // every event with seq > seq, in order
    std::uint64_t Last() const;                          // the newest seq (0: none)
    void Clear();
};
}
```

`ScenarioRunner` owns one: `fx::Feed& Fx()`, `const fx::Feed& Fx() const`. Nothing in the simulation reads it, and its
draws come from its own generators (the runner's, the bridge's), never a model's: `ScenarioRunner::SetFxOn(false)` stops
every push and draw of the runner's and of a `MissionBridge` stepped with it, and the session is the same to the last
log entry (`test_fx_events.cpp` and `test_fx_bridge.cpp` compare a session with it on and off). The game's own test
shots are pushed into it whatever the switch says. It is filled where the models already decide:

* `MissileLaunch` at our launch (`EngagementModel::OnLaunch`'s caller): `position` the launcher's rails, 3.5 m over the
  ground where the vehicle stands (ENU z is not the ground far from the origin), `velocity` the launch direction x 30
  m/s (azimuth to the tracked target, elevation 27°, voenizdat_osa_1991: the 9M33 leaves at a constant 27°), `target`
  the tracked target, `end_t_s` the shot's planned meeting time. A launch with no target track, and a missile thrown off
  by АВАРИЙНЫЙ ПУСК, goes off unguided along the hull's heading: its `MissileLaunch` and at once its `MissileEnd` (Wide)
  where it destroys itself. A missile thrown off is numbered 1000 + its count (`Launcher::EmergencyThrown`), never one
  of the launches' numbers, and flies `launcher.emergency.flight_s` (25.5 s).
* `MissileEnd` where the runner reads the engagement events: `end` agrees with `KillAssessment`'s own draw for that shot
  (Hit, NearBurst, Wide, Early; `KillAssessment::EndOf(shot)`, public), `fire`/`falls` with its `tv_fire`/`falls`;
  with no callouts the runner draws them from a presentation-only generator (never a model's). Hit: `aim` = the target's
  position + a few metres (inside the warhead's reach), and the wreck's `AircraftDown` follows with the same `fire` and
  `falls`; NearBurst: 10-25 m off; Wide and Early: `aim` = where it destroys itself (it flies on to launch + 25-28 s,
  voenizdat_osa_1991 p22, and at least 0.5 s past the moment its end is decided: an "out of reach" miss is decided at the
  end of its flight time) and `end_t_s` that time, always after `t_s`. `position`, `velocity`: the missile's last
  display state.
* `AircraftDown` in `DestroyTarget` and our hit (not for a cruise missile's own warhead: that is a `CruiseImpact`).
* `SamLaunch`/`SamEnd` from the battery's launchers and the fighting neighbours (`BatteryFire` events: Launch, Hit,
  Miss): `position` the launcher's rails, `source` its callsign ("22", "<neighbour>/<n>"), `number` its launch count,
  `world_id`/`target` the target; a hit's wreck (`AircraftDown`, named for the launcher or the neighbour) follows the
  burst. The bridge's missile neighbours (`MissionBridge::Engage`) too, with the aircraft's World id; a round on its way
  to an aircraft that is gone (shot down by another, past its last waypoint) flies on and destroys itself
  (`SelfDestruct`). In a linked battery (`BatteryLink`) the post's launchers fire in the post's runner: their
  `SamLaunch`/`SamEnd` are copied into the Osa's feed, the one the game reads, with the Osa's World ids. `GunPass` for
  the bridge's gun neighbours (`zsu234`, `zu23`, `s60`, `ks30`, `aaa`): `position` the site, `world_id`/`target` the
  aircraft, `t_s`..`end_t_s` the pass through the zone, `number` the rounds: the munitions library's `rate_rpm` for the
  system's round when the bridge has the library (`MissionBridge::SetMunitions`, what the game draws by), else the
  bridge's own figures (`munitions.json`'s cyclic rates, wiki_guns: the ZSU-23-4 3400 a minute, the ZU-23-2 2000, the
  S-60 120, the KS-30 12; other AAA as the ZU-23, which the theatres' `aaa` sites are), times the pass's length. (The
  library's `systems` map sends `aaa` to `shell_23_zu23` too.)
* `WeaponRelease` at the bridge's release (weapon id, count, delivery from the flight: `RaidAircraft` gains `weapon`,
  `weapons`, `delivery` copied from `Flight`). A load that falls (bombs and dispensers, retarded, chute and runway bombs,
  a guided bomb released on the free fall's solution) is let go where its throw from the aircraft's state best meets the
  aim (`MissionBridge::Releases`): each step it compares the aim's distance ahead along the aircraft's track with the
  weapon's throw from here (`airwar::WeaponFlightOf(...).throw_m`), the aim within the release capture of its line, and
  lets go at the step the throw comes to reach the aim (their ratio crosses 1), or, where it never quite does, at the
  step after the two came closest. The window for it is the attack run: for a level or laydown attack (whose planned
  release point is over the target) the farthest throw in a vacuum and 500 m more short of it; for a loft or a dive
  from where the attack starts (`delivery.loft_pull_m`, `dive_roll_in_m`); for a pop-up from its apex (the planner's,
  half way between `popup_pull_m` and its release point) and only once it is past the top of its climb, its path at or
  below the horizontal: a pop-up attack rolls in and releases in its dive (a release in the climb would be a toss);
  never less than the capture. At the latest it goes at the release point (within the runner's 500 m waypoint capture,
  or turning away from it inside the capture). Played against the planner's routes (`test_fx_bridge.cpp`), level,
  laydown and guided loads leave on the throw, a pop-up just past the top of its climb (0.5-0.6: section 7) and a dive
  on the roll-in (1.0-1.2); the routes' tosses level off at 600 m before their release point, from which no throw reaches the aim, so a
  toss leaves in its climb with the aim a third farther than its throw (1.3-1.4: section 7). Rockets, missiles, guns,
  an unknown weapon and a recce's camera: within the capture.
  `position`/`velocity` the aircraft's, `aim` the impact point on the ground, `end_t_s` the impact time: the
  weapon's physical flight (`airwar::WeaponFlightOf`, `air_war/weapon_flight.hpp`: game logic, not presentation) from
  the aircraft's height over the ground where the load lands, its speed and its own path angle (its velocity against
  the local up: `ReleaseStateOf`), with `scenario.weapon_fall_s` for an unknown weapon; the impact point is the bridge's
  (where it was aimed, scattered). `WeaponImpact` when the bridge explodes it; the loads still falling when the session
  ends land in `MissionBridge::Finish` at their own time (their `WeaponImpact`'s `t_s` is then after the session's last
  step), until our vehicle is out of action. A package in a session that runs on (`LivePackages`) that is over by its
  clock with loads still falling lands them in the session at their own times (`MissionBridge::FallingUntil`,
  `LandFalling`) and is handed back once the last has.
* `ArmBurst` from `EmSensorLink::ArmResult` (burst point, hit or miss) and the bridge's own ARM model (`ArmLaunch` at
  its firing, aimed at our radar's antenna 4 m over the ground where the vehicle stands; `ArmBurst` at its arrival, on
  the antenna, or 30-80 m wide of it when it missed or lost the radar).
* `CruiseImpact` when a cruise missile reaches its target: `position`/`velocity` the missile's, `aim` where its warhead
  goes off on the ground, `world_id` the missile's; no wreck.
* The dynamic campaign testbed's waves (`air_war/test_waves.hpp`, `docs/DYNAMIC_CAMPAIGN_TEST.md`): each load a
  `WeaponRelease` within its weapon kind's release range of its aim (a delivery its weapon allows, `end_t_s` from
  `WeaponFlightOf` as the bridge's) and its `WeaponImpact` at that time; a wave's cruise missile a `CruiseImpact` where
  it ends (`weapon` its model). Our air defence round it (the neighbours the battery fire model does not fight,
  `ScenarioRunner::FireNeighbourDefences`): a gun site's `GunPass` of 1.5-4 s at the nearest enemy aircraft in its
  reach, then a pause of 2-6 s (rounds as the bridge's own figures); a missile site's `SamLaunch` (`weapon` its system,
  `end_t_s` the meeting) and its `SamEnd`, `Hit` on the aircraft, `NearBurst` 15-60 m off it or `SelfDestruct` past it.
  With the damage model (`docs/DAMAGE.md`): a contact gun's `GunPass` carries its pass's fire-control errors
  (`aim_across_rad`, `aim_up_rad`, `range_share`, `lead_error`, `correction_per_s`, `spread_mrad`), so the game's
  tracers miss as simcore's rounds did; a time-fuzed gun's shells are each a `FlakBurst` where simcore burst it; a
  rise to moderate or heavy damage is an `AircraftDamaged` (`number` the level, `fire`, `falls` for smoke), whose
  smoke and fire the game trails from the aircraft; a jettisoned load is a `WeaponRelease` with delivery `"jettison"`,
  landing inert. An aircraft destroyed, or heavily damaged when its time comes, is an `AircraftDown`.

### 3.2 `scenery/munitions.hpp` and `data/scenery/munitions.json`

Every munition a scenario can show: the 37 weapons of `data/air_war/weapons.json` (bombs, cluster dispensers, retarded
and parachute bombs, guided bombs, rockets, air-to-ground missiles; guns have no model but have a `gun` entry for
their tracers and sound), the SAMs (9M33/M2/M3, the neighbours' systems' missiles, MANPADS), the ARMs (Shrike, HARM,
Standard ARM, Kh-58, Kh-25MP), the cruise missiles (Kh-22M, Tomahawk, CALCM), and the AAA guns' rounds. Each:

```json
{ "id": "mk82", "name": "Mk 82 LDGP 500 lb bomb", "kind": "bomb", "length_m": 2.21, "diameter_m": 0.273,
  "mass_kg": 241, "charge_kg": 95, "nose": "ogive", "nose_l": 0.27, "tail": "boattail", "tail_l": 0.33, "tail_r": 0.4,
  "fins": [ { "count": 4, "span_m": 0.38, "root_chord_m": 0.4, "tip_chord_m": 0.24, "x": 0.8, "sweep_deg": 35, "roll_deg": 45 } ],
  "colour": "olive_drab", "bands": [ { "x": 0.08, "w": 0.02, "colour": "us_yellow" } ],
  "retarder": null, "motor": null,
  "source": "wiki_weapons, maps_museum_bombs (length 87.4 in, diameter, mass), tpub_ordnance (fin span 15 in), mil_std_709c (olive drab, yellow nose band); charge: derived (87-89 kg of tritonal at about 1.07 TNT); nose, tail and fin chords: estimate",
  "confidence": "medium" }
```

`kind`: bomb, retarded (Snakeye blades: `retarder` `{ "type": "blades" }`), ballute, parachute (`{ "type": "chute",
"area_m2": 6, "open_s": 1.2, "descent_mps": 47 }`: `area_m2` is the nominal area, the canopy's cloth; it is drawn
open at `kChuteInflatedShare` 0.67 of its nominal diameter, Knacke; `descent_mps` its steady descent, which may only be
the simulation's own 47 m/s until `air_war/weapon_flight.hpp` reads it), cluster (`{ "opens_agl_m": 500,
"submunitions": 202, "pattern_m": [120, 240] }`), guided (Paveway, KAB), glide (GBU-15), rocket, missile
(air-to-ground), sam, arm, cruise, shell (gun rounds).
`motor`: `{ "boost_s", "sustain_s", "smoke": "dense|reduced|minimal", "plume_colour" }`. Positions along the body
(`x`) are fractions of the length from the nose. Values without a source are named `estimate` (fin chords: no source
found any).

`MunitionLibrary` loads it (no exceptions; errors as text), finds by id, maps the weapons' ids, the basic models'
missile ids (`missile_9m33`, `missile_arm`, `kh22m`, `tlam_c`, `agm86c`) and the systems' missiles. `BuildMesh(spec,
lod)` returns a `TriMesh { positions (m, x forward, y right, z up, origin at the centre of mass ~ the middle), normals,
colours (linear RGB), indices }`: a body of revolution (`lod` 0: 24 sides, 1: 12, 2: 6, 3: a 4-sided box) with the
nose and tail profiles, fins and wings as thin double-sided plates (none at `lod` 3), the bands as coloured rings, and
optional parts (open retarder blades, a chute) as separate meshes. Tests: every weapons.json id resolves; mesh
bounds match the spec's length, diameter and fin span; normals unit; indices in range; each LOD smaller.

### 3.3 `fx/explosions.hpp`, `fx/trajectories.hpp`

* Blast: Kinney-Graham peak overpressure `KinneyGraham(z)`, scaled distance, `PeakSplDb(charge_kg, range_m)`; fireball
  radius and duration by W^(1/3) (Hopkinson-Cranz; fireball 0.71-1.48 m/kg^1/3); dust column and crater by charge and
  ground; the explosion's look as a struct (`ExplosionLook { fireball_m, fireball_s, smoke_m, smoke_s, debris, ground }`).
* `BombPath`: from release (position, velocity) to the impact (point, time): a Hermite fit whose start is the
  aircraft's velocity and whose end is the steep fall a bomb really has; orientation along the velocity; retarded and
  parachute paths with their own slowing; cluster dispensers opening at their height.
* `MissileDisplay`: our missile's drawn position: it leaves at 27° and gathers onto the simulation's line in the first
  2 s; boost and sustain phases from `munitions.json` (`MotorPhase(t)`).
* `WreckFall`: a killed aircraft's fall: ballistic with drag, a tumble rate, the ground's height; deterministic from a
  seed so every client draws the same.
* `TracerRound`: a gun round's flight with drag and gravity from its muzzle velocity, aimed with lead at the target's
  velocity plus dispersion; its tracer burns `tracer_s`; its self-destruct at `sd_s` (23 mm: 5.6-11.2 s; 57 mm: 13-17 s,
  data in `munitions.json` with sources).
* Humidity aloft for contrails: `fx::RelativeHumidityAloft(weather, alt_m)` (a cloud layer is saturated; a cirrus
  layer ice-supersaturated; between, a preset's optional `"upper_rh_pct"`, else 40 %, an estimate).

## 4. Unreal

### 4.1 `USimFxSubsystem` (UTickableWorldSubsystem, Game and PIE worlds)

Each frame: one entity snapshot from `USimWorldSubsystem`, the new feed events, the views (the player's camera and the
TV sight's scene capture when it captures), then:

| Layer | Drawn as | Budget (cvar) |
|---|---|---|
| Smoke and fire (trails' puffs, explosion smoke, dust, wreck smoke, fireballs, flipbook impostors) | one ISM of camera-facing quads, premultiplied (AlphaComposite), back-to-front together: soot hides a fireball behind it, a fireball in front glows over the smoke | `sam3d.fx.MaxSprites` 4000 over all layers |
| Glow (flashes, motor plumes, tracers, sparks, muzzle flashes) | ISMs, additive | |
| Ribbons (missile smoke trails, contrails, wreck trails) | procedural mesh sections, camera-facing strips | `sam3d.fx.MaxRibbons` 48, 256 points each; with none free, a missile's or a wreck's smoke takes the contrail farthest from the camera, and so does a contrail when that one is over 1.5 times as far as itself (`SimFxLogic::RibbonTakeOver`); a ribbon is stopped only by its owner |
| Lights | a pool of shadowless point lights given to the brightest by intensity / distance² | `sam3d.fx.MaxLights` 8 |
| Decals | scorch and crater decals at impacts, the oldest reused | `sam3d.fx.MaxDecals` 32 |
| Munitions | static mesh components from `FSimMunitionModels` | `sam3d.fx.MaxMunitions` 48 |

The pools of lights, decals and munition components grow when their cvar is raised at run time; lowered, the rest
wait unused. `sam3d.fx.Enable` 0 draws nothing; `sam3d.fx.FullPx` 48 and `ImpostorPx` 6 set the tiers;
`EmissiveScale` scales every brightness.

Per-instance custom data on every sprite (the materials' contract, 13 floats): 0 frame, 1 opacity, 2 heat, 3-5 tint
RGB, 6 erosion, 7 lit (0: unlit side to the camera, 1: sunlit), 8 kind (the smoke material's picture: 0 the smoke or
debris atlas, 1 the generated fireball flipbook, 2 the recorded colour flipbook, Unity's CC0 Explosion02HD with its
emission; in the additive material 3 is a detonation's spiky flash, `T_FX_Flash`, and 4 an afterburner's flame, a
nozzle-wide column tapering aft, `T_FX_Afterburner`), 9-11 the flashes' and fires' light on it (the frame's eight brightest lights, T9), 12 the soft-particle
distance (0.3 × the drawn width). The materials (`/Game/SAM3D/FX/Materials`, made by `tools/ue_make_fx_assets.py`,
whose docstring has the formulas): `M_FX_Smoke` (smoke, debris and fire; unlit, its tints albedos it lights by
`SunColor` on a sphere's normal per pixel, `AmbientColor` times the sky's share `lerp(SkyLow, 1, saturate(0.5 + 0.5 n.z))`
so an underside that sees the ground gets less, and the local light; a fire's glow rises as temperature⁴ so a cooling fireball goes dark;
soft against the ground by DepthFade and gone within 1-4 m of the camera; the C++ also fades a whole smoke sprite the
camera is inside, by its size, `SimFxLogic::CameraFade`), `M_FX_Additive` (a `Tex` parameter: glow,
spark, streak, ring), `M_FX_Ribbon` (vertex colour lit per point, `T_FX_Contrail`'s billows, erosion and density by
distance along the trail, the frame's lights in UV2-UV3.x, faded near the camera by the trail's width in UV3.y),
`M_FX_Munition` (vertex colour, lit),
`M_FX_Scorch` (decal). Every sprite and ribbon material is fogged per pixel; every texture is never streamed and
sampled trilinear. A fireball's heat follows Gross's curve: held 0.05 s, then falling with τ = 0.15 s (W/100 kg)^1/3.
A puff makes most of its growth within its growth time (0.4 s in a blast, 1.5 s in a launch cloud); erosion eats it
only in the second half of its life.

**Explosions** (LOOK 2-8; counts, colours and times in `effects.json`'s `explosion`, `bombs` and `wreck`): a detonation's
flash is a spiky star lasting 1-2 frames (`spike_s`), with a soft flash only on the impostor tiers; by day the flashes
light the smoke at most `day_light_cap` of the sun's white, so a burst never turns its own dust white. A ground burst's
fireball is swallowed by soot within 0.3 s; soil ejecta are rooted axis sprites in a V; its column rises as
K t^1/2 to Church's 92.6 W^0.25 m (`SimFxLogic::ColumnTopM`, scaled by `column_scale`, stabilised at 60 s (W/100)^0.23),
graded from dark soil low to dust high, darker for the TNT fills `soot_munitions` names, and drifts with the wind for
`column_life_s`; a low dust skirt races out (r = R(1 - e^-t/0.6)); clods fly in clumps with dust trails laid along their
path (puffs with a negative age wait unmoved and undrawn); a faint ring by day only, faded at grazing angles
(`SimFxLogic::RingAlpha`). An airburst's puffs (a SAM's warhead, flak) grow by Sachs's (P0/P)^1/3 with altitude, hang
`air_puff_s` (flak: the calibre's row of `flak_by_calibre`, black or light grey) and grey as they spread; a warhead's
fragments fly in a cone along the missile's velocity (`SimFxLogic::DirInCone`). An aircraft hit is a fuel fireball of
5.8 M^1/3 m for 0.45 M^1/3 s (M = `kill_fuel_kg`) carried with it; a burning wreck draws a torch `torch_length_m` aft. A
cluster's bomblets ripple over `cluster_spread_s` and leave a low haze over the pattern; a cruise missile's burst throws
its debris flatter, its column paler with masonry; an ARM bursts `arm_burst_up_m` over the ground with its fragments'
dust kicked up along its path, a Shrike (`wp_munitions`) with white phosphorus. The sky's light on the smoke falls with the sun's elevation, a decade every 2°
below −6°, to a moonlit night's (`ambient_night`), and only as far as the level's sky light gives it. Smoke and trails
take the sun at `lighting.smoke_sun` and the sky at `smoke_sky` of what lights the ground (a photograph exposed for the
ground holds sunlit smoke short of white), the sky falling to `smoke_sky_low` on undersides (`SimFxLogic::SkyShare`,
`PuffNormal`; a ribbon's two edges are lit the same way). By night a burst's light on its smoke is capped at
`night_light_cap`. Bursts crowd: a full ground burst within `crowd_radius_m` and `crowd_s` of others draws fewer fire
sprites, ejecta jets, skirt puffs, clods and sparks (`SimFxLogic::CrowdShare`: 2/(n+2), never under a quarter), since a
stick of six makes one cloud, not six. A burning wreck's torch is two motor-flame sprites along its fall (the full
length and a wider 0.55), with flames shed into the slipstream. Missing
assets fall back to the engine's basic shape material (the colour flipbook to the generated one) and a log line,
never a crash.

**Trails, guns, contrails and afterburners** (LOOK 9-14; `effects.json`'s `missile`, `guns`, `contrail`,
`afterburner`, every number with its origin). A ribbon point carries its distance along the trail, its own widening
(`SpreadK`, two smooth waves of `WaveM` so the trail swells and pinches into puffs instead of stepping), a drift and,
for a contrail, its side of the pair; `SimFxLogic::RibbonPointAt` moves it by age (drift, the vortex pair rolling up
over `roll_s`, sinking at `sink_mps` to `sink_max_m`), `CrowAt` pinches the pair into Crow rings from `crow_from_s`,
`TrailOpacityAt` thins it (0.8 at the missile to 0.5 by 15 s) and `TrailErosionAt` eats it from `erode_from` of its
life. Each point is lit by the sun on the side seen from the camera (a trail turning across the sky goes from lit to
backlit along its length) and by the frame's flashes; the texture runs by distance (`tile_m`) so its billows stay in
the air; a far trail stays an unbroken hairline (1.2 px, alpha by its true width). The strip turns the same way as the
point before (no folds) and fades where it is seen along its length (`SimFxLogic::EndOnAlpha`) and near the camera. A
missile's trail starts as wide as its fin span (`trail_span_share_*` by its smoke: 0.65 m for the 9M33); a MANPADS
flies a corkscrew (`HelixOffset`, LOOK 9.1's pitch); our launch puts a low cloud round the launcher (puffs within
`launch_cloud_spread_m`, five on a far impostor). A rocket's smoke ends in mid-air where its motor burns out. Gun
passes fire bursts of `burst_min_s`-`burst_max_s` with `pause_min_s`-`pause_max_s` pauses; only the belt's traced
rounds are drawn (`traced_every`, `SimFxLogic::IsTracedRound`), each tracer as long as its travel in a frame
(`TracerLengthCm`, never under `tracer_min_frame_s`) and coloured by day and night (`TracerColourFor`: a round's own
warm tracer colour or `tracer_day_colour` by day, `tracer_night_colour` at night, by the sun's elevation, `NightShare`;
`tracer_day_brightness` by day against the bright sky, `tracer_brightness` at night); each barrel flashes
(`barrel_spacing_m`) and a burst leaves a little grey gun smoke. A MANPADS corkscrew's radius wanders along it
(`HelixRadiusAt`: two slow waves, ±50 %), so it is not a regular spring. An afterburner by day is a short pale flame;
by night a long orange one (blue for `blue_models`): one nozzle-wide flame sprite per nozzle (additive kind 4), a
small glow just aft of the nozzle, and the shock diamonds as knots inside it; its light (`light_lumens`) sits
`light_aft_m` behind the nozzle so the basic model is not lit as a block. Under `point_px` the flame is its glow
alone.

**Night glints.** At night a point light (an afterburner's glow, a burning motor's glow) widened to its pixel floor
is drawn `lod.glint_sprite_px` wide and keeps at least `lod.glint_keep` of its opacity (`SimFxLogic::GlintAlpha`) in
place of its true area's share, so it shows as a clear point to 15 km and beyond (LOOK 13, 15) instead of one dim
pixel. A burning motor at night lights its own smoke for `missile.night_lit_trail_m` behind it (LOOK 9.3: the trail
shows only a few metres behind the flame at night). A burning wreck's torch at night is `wreck.torch_night_share` of
the fire's brightness, its light (`wreck.night_light_lumens`) from the torch's middle.

**Level of detail by projected size** (pixels on the largest view: the camera or the TV sight at 1.44°/4.8°):
an explosion over 48 px is a full particle explosion (fireball sprites, smoke, debris, sparks, flash, light); 6-48 px a
2D impostor: one flipbook sprite and a flash; under 6 px a glow sprite at night (the flash) and by day a grey speck
(one smoke sprite at its true size, never under `speck_px` 1.5 px; a sprite widened to a pixel floor is dimmed by its
true area over the drawn, and a ribbon is never under 1.2 px wide, its alpha by its true width), nothing by day under
1.5 px; past
`sam3d.fx.MaxRange` (40 km) or past the weather's visibility nothing is drawn and only the sound is played (a sprite
farther than either from the camera is refused when it is added, so a motor's or an afterburner's glow at its pixel
floor never shows past them). Munition meshes show over 3 px; otherwise only their plume and smoke. Effects outside
every view's frustum are simulated cheaply (lifetime only) and spawn no particles; a ribbon whose nearest point is
farther than `lod.far_ribbon_m` (5 km) rebuilds every fourth frame (judged by its nearest point, not its head, so a
trail that passes the camera with its missile far away stays smooth). Trails are laid and drawn only within the
effects' range or the visibility, whichever is nearer, plus `lod.trail_margin_m` (2 km); a contrail to
`lod.contrail_range_scale` (2) times the range, never past the visibility (`SimFxLogic::TrailRangeM`): a missile's or a
wreck's smoke stops being laid past it, and a ribbon wholly past it is neither moved nor meshed. Nothing is animated
outside the visual range. The effects' step is the render clock's advance, its hitch cap 0.25 s times the time scale
(`SimFxLogic::FrameDt`, as the simulation's own step), and the particles are integrated in slices of at most 0.25 s
(`SubSteps`), so at 32x they keep the clock their events are timed by. `USimFxSubsystem` is not made on a dedicated
server (as `USimFxAudioSubsystem`).
A missile that flies on after its end (Wide, Early) flies on from where it was last drawn, at its last drawn velocity,
so it does not jump to the event's state.

### 4.2 `USimFxAudio`

Sources: every aircraft (engine sounds by `data/fx/sounds.json`'s profile for its type: turbojet, turbofan, heavy
jet, turboprop, helicopter, piston, cruise missile), every missile in flight while its motor burns, every one-shot
event (explosions, launches, gun bursts, booms), fires. For each, a `fx::SourceTrack` of its positions and, every
frame, `fx::Hear(track, listener, now, c)` with `c` from the air at the mid-height: the audio component sits at the
emission point, pitch = Doppler (clamped 0.5-2), volume from the source level - spreading - the loudness window,
low-pass at `AbsorptionCutoffHz(distance)`. Supersonic sources are silent until their cone arrives, then `BoomWatch`
plays the boom with its Carlson overpressure; booms reach the ground only above the cutoff Mach. One-shots start when
their wavefront arrives (distance / c after the event). Near and far samples crossfade by distance (Arma). Inside the
closed cabin everything outside is muffled (low-pass 700 Hz, -14 dB). Voices: `sam3d.fx.MaxVoices` 32 by loudness;
the rest are virtual (tracked, not playing). Paused or at a time scale over 2: silent.

Recordings (`sounds.json` `"recordings"`, `art/fx/recorded/PROVENANCE.md` section 4). A profile's layer may have
`"band": "near"` or `"far"`. It then plays only on its side of the crossfade, which is equal power (cos/sin) between
the profile's `near_m` and `far_m` (default 600 m and 2.5 km). Each recording has `recorded_at_m`: it already carries
that much air, so it is low-passed only for `distance - recorded_at_m`. Its level is matched to the synthesized sound
it stands for, heard at the match distance through the air (`match_db`, checked by `fx_sounds.py --check`).
Recordings play as steady loops: far jets (fighters), the heavy jet, the turboprop, the helicopter near and far, and
the rockets' far roar (small and large) and the large rocket's body. The near fighter roar, the small rocket's near
layer, the whines, the afterburner, near blasts, booms, the bullet crack and fire stay synthesized. Every blast class
over 3 kg of charge (the classes up to 50 kg, up to 250 kg and the largest) has a `tail` (`S_FX_Rec_Blast_Tail`, a thunder roll), played `tail_delay_s` after the blast,
`tail_db` under it, weighted by the far side of the crossfade. An aircraft shot down adds `S_FX_Fireball_Roar`
(`events.fireball`).

The weapons' sound follows the picture. A gun pass fires the same bursts as the drawing:
`SimFxLogic::GunBursts(seed = the event's seq)`, with the burst and pause lengths from `effects.json`'s guns. Within a
burst the rounds go at the munition's rate (`munitions.json` `rate_rpm`, else `sounds.json`). From 50 mm up (flak),
each round's burst is heard at the aim, after its time of flight, scattered by `effects.json`'s flak jitter and
offset, with the munition's charge (else `flak_default_kg`). Smaller rounds with a self-destruct time pop
`sd_pops_per_burst` times at their self-destruct point. An aircraft's gun release sounds for its rounds / rate
(0.5-4 s), with its impacts spread over that time. Rocket ripples, bomb sticks and the cap on each release come from
`effects.json`, as in the picture. `S_FX_Cluster_Ripple` spreads its 200 bomblets evenly over 1.5 s, `effects.json`'s
`cluster_spread_s` (made by `fx_sounds.py`: change both together).

### 4.3 Entity actors

`USimWorldSubsystem` keeps one snapshot per frame (`GetEntityStates()` was rebuilt per actor per frame: O(N²)), built
at the end of its tick after the steps: the effects and their sound after it and the actors early the next frame read
that one (built by the actors' first read and again after a step, it was two a frame). Actors
draw at render time by velocity. Our missiles, ARMs and cruise missiles wear `FSimMunitionModels` meshes; a finished
missile is hidden where it was (not at the origin). A destroyed aircraft is not hidden: it follows its wreck's fall
(`USimFxSubsystem::GetWreckPose`) burning and smoking, and is hidden when it reaches the ground (an impact explosion and
a smoke column take its place). Vehicle models are not changed.

## 5. Performance rules

* No per-effect actors or components: a few ISMs, one procedural mesh component, a fixed pool of lights, audio
  components and decals, all made once.
* CPU particles in flat arrays, updated in one pass; instance data written once per frame (`BatchUpdateInstancesData`).
* Nothing spawns outside the views; nothing is animated past the visual range; impostors past 48 px; lights only for
  the brightest few; sound voices only for the loudest.
* Effect lights cast no shadows; sprites, ribbons and munitions cast none.
* Every munition mesh is built once at start (`FSimMunitionModels::BuildAll`, about 66 meshes in 0.2 s), never on the
  frame a munition first appears (that was the 3.5-4.6 ms one-frame peaks at a launch or a stick's release).
* A trail's puffs are seeded by their point's distance along the trail, not its index, so they do not pop when the
  oldest point expires.
* `stat SAM3DFx` shows the counts and the time; cvars `sam3d.fx.*` change every budget at run time. The photographer
  (section 8) logs each subject's mean and peak effects time, its most sprites, ribbons and lights, and the sound's
  time (`USimFxAudioSubsystem::GetLastFrameMs`). Target: the effects well under 0.5 ms a frame on average.
* `sam3d.fx.LogSpikes <ms>` (0: off) logs every frame whose effects take longer, stage by stage (views and sun,
  events, effects, trails with their points and quads and whether the section was rebuilt, sprites, lights, decals,
  munitions). Each sprite layer gets its first 64 instances at start and grows by doubling; a layer goes hidden only
  after 300 empty frames (each show or hide rebuilt its render state: 0.8 ms frames). The trails' mesh section is
  not pre-sized: every quad of it is uploaded every frame, so it grows at once but shrinks to fit again once under
  half of it has been used for 30 frames running (`SimFxLogic::RibbonSectionQuads`).
* The recorded colour flipbook and its emission map import at 1024² (`MAX_SIZE` in `ue_make_fx_assets.py`), a
  quarter of their 2048² memory.

## 6. Data and art

* `data/scenery/munitions.json` (shapes, motors, smoke, charges; sourced, `estimate` where no source).
* `data/fx/effects.json` (lifetimes, sizes, colours, counts, light intensities; game values, `"source": "sam3d game value"`).
* `data/fx/sounds.json` (engine profiles and the aircraft that use them, sound levels at 1 m, event sounds by charge,
  guns by system, booms, the cabin's muffling; estimates from published levels, `"source"` says which).
* `art/fx/textures/*.png` by `tools/fx_textures.py` and `art/fx/sounds/*.wav` by `tools/fx_sounds.py`, synthesized or
  made from the public-domain and CC0 recordings and pictures in `art/fx/recorded/` (each listed with its licence and
  processing in its `PROVENANCE.md`; no other licence is taken).
* `tools/ue_make_fx_assets.py` imports them and builds the materials under `/Game/SAM3D/FX`.

## 7. Known limitations

* Clients in multiplayer get no feed yet (the server's effects are not replicated).
* Aircraft attitude, throttle and afterburner are not simulated: afterburner is shown when supersonic or climbing hard.
* The engagement model's missile path is illustrative (a line to the target); the drawn path only gathers onto it.
* A toss lets its load go about 1.8 km before the planner's toss point, with the aim a third farther than its throw
  (`test_fx_bridge.cpp`: ratio 1.33-1.36 from 363 m climbing 17 degrees): the planner's toss route levels off at
  `delivery.loft_release_m` (600 m) before its release point, from where no throw reaches the aim, so the bridge lets go
  in the climb where the throw came nearest. The drawn path stretches to meet the aim. Left until the planner's toss
  points are set from the throw (`delivery.release_loft_m`, `loft_pull_m`, `loft_release_m` in
  `data/gameplay/air_war.json`, a balance pass of their own); an aircraft shot down after that early release has still
  delivered.
* A pop-up lets go in its dive, but the runner's terrain-following climb (`max_climb_mps` 120, 1.5 g) tops out near
  the release point, about 1140 m up and 2 km short of the aim, rather than at the planner's apex (1200 m, 3.75 km
  short), and the descent to `delivery.popup_release_m` is almost level: from there the throw (about 3.9 km) already
  reaches past the aim, so it lets go at the first step past the top with the aim half its throw (`test_fx_bridge.cpp`:
  ratio 0.49-0.58, path -0.4 degrees). The bombs still land at the aim (the bridge's impact point); the drawn fall is
  long and shallow. Left until the runner flies a pop-up's roll-in and dive (a dive angle the route can command).

## 8. Tests

simcore: `test_fx_acoustics.cpp`, `test_fx_bridge.cpp`, `test_fx_contrails.cpp`, `test_fx_events.cpp`,
`test_fx_explosions.cpp`, `test_fx_trajectories.cpp`, `test_munitions.cpp`; the falling loads of a package over by its
clock in `test_packages_live.cpp`. Unreal: automation `SAM3D.Fx.*` (the subsystem's LOD choice, budgets,
event consumption, audio arrival times, the explosions' rules in `SAM3D.Fx.Explosions`, the trails', guns' and
contrails' in `SAM3D.Fx.TrailsGuns`, the gun bursts shared by picture and sound in `SAM3D.Fx.GunRhythm`, the recorded
layers' bands and air in `SAM3D.Fx.Audio.Recordings`, the far blasts' tails in `SAM3D.Fx.Audio.BlastTail`, the art passes' rules (smoke's sun and sky shares, the flash
light's colour, crowding, gun bursts per munition) in `SAM3D.Fx.Polish`, the night glints, the tracers' colours and
the corkscrew's wandering radius in `SAM3D.Fx.GlintTracer`, the review's fixes (the effects' step under time
compression, trails cut at visual range, the ribbon pool by owner, the ribbon section shrinking) in `SAM3D.Fx.FixUp`)
run headless. `tools/test_fx_tools.py` checks the air absorption against ISO 9613-1, pitching, level flattening and the
recordings' level match.

Pictures: the game run with `-SimFxShots=<folder>` (and `-SimFxShotsOnly=a+b`, `-SimFxShotsNight`) photographs each
subject at its delays, near and far, and logs the effects' cost (`SimFxShots.cpp`: per shot the sprites, ribbons,
lights and the frame's effects time; per subject the mean and peak of the effects' and the sound's time, the most
sprites, ribbons and lights, and the frame); the effects are cleared between
subjects (`USimFxSubsystem::ClearEffects`) so one subject's smoke never stands in the next one's picture. The trails' subjects:
`sam_launch_neighbour`, `own_launch` (our 9M33, also `_from_vehicle`), `manpads_launch`, `s8_rocket_pass`,
`zsu234_gunpass`, `contrail`, `afterburner_f15`, `afterburner_mig23` (an aircraft put into the scenario for it).

## 9. Sources

Physics: ISO 9613-1 (absorption), Carlson NASA TP-1122 (sonic boom), NASA FS-016, Kinney-Graham (blast), Schumann 1996
(contrails), Sonntag 1994 (saturation pressures). Games: DCS forums and newsletters, Arma 3 wiki (`CfgSoundShaders`,
`CfgSoundSets`, `CfgAmmo`) and OPREP, Frostbite's HDR audio talks, Wwise HDR notes, War Thunder dev blogs 5340 and 6911,
IL-2 forums, MSFS forums and SDK, realtimevfx. Munition shapes: the sources registered in `docs/SOURCES.md` and cited
per value in `munitions.json`.

The look of each effect (targets against time, at distance and at night, techniques, free assets, sources): `LOOK.md`.
