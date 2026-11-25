# Five posts on Claude, Unreal and free assets: what SAM3D takes (4 Nov 2026)

The project owner passed on five posts on X, asking whether any would make a better game, better assets, terrain or
gameplay systems. Each was read in full. What was taken went into `unreal/README.md` ("Working in the live editor"),
`docs/scenery/TERRAIN_ROADMAP.md` (rule 6, section 4.2, section 8) and `CLAUDE.md`.

| Post | What it says | For SAM3D |
|---|---|---|
| [Elliot Padfield, 28 Oct 2026](https://x.com/ElliotPadfield/status/2115191478779785268) | ambientCG: 2,000+ CC0 PBR materials and 400+ HDRIs, a free API without a key; tell the agent to search it, take the 2k version, pair it with Poly Haven | **Already done.** Track T's texture register fetches from exactly these two (`tools/fetch_textures.py`, ambientCG's v3 API and Poly Haven's). The HDRIs are not wanted: the sky is the engine's physical atmosphere, lit by the scenario's sun (R7). A search mode for the fetch tool would only save a browser visit; not built |
| [Rimsha Bhardwaj, 29 Oct 2026](https://x.com/heyrimsha/status/2115738391467946090) | Ten open-source repositories: Jolt Physics, Terrain3D, Material Maker, Effekseer, Voxel Tools, Recast, ozz-animation, The Forge, meshoptimizer, FastNoise2 | **One kept, as an option: Material Maker** (MIT), procedural PBR textures from node graphs, for the surfaces no free scan exists of (a standing crop's rows, a drilled seedbed); its output would be our own work. The rest duplicate what Unreal has (Chaos for Jolt, Niagara for Effekseer, its navmesh built on Recast, its animation system, Nanite and its LODs for meshoptimizer), are Godot's (Terrain3D, Voxel Tools) or are for engines of one's own (The Forge, ozz). simcore takes no third-party libraries, so FastNoise2 and Recast could not go there either, and simcore's routes are its own road graph (`simcore/nav/`) |
| [Meng To, 1 Nov 2026](https://x.com/MengTo/status/2116694299043840075), quoting Shikhar | A city built in Unreal by Opus 5.5 with every asset modelled in Blender; Epic's Unreal MCP | **Confirms the route already set**: own models made in Blender by script (`docs/MODELLING_FROM_PHOTOS.md`, the Blender MCP), Unreal reached through Epic's MCP. No claim to check beyond the video |
| [rewind, 7 Oct 2026](https://x.com/rewind02/status/2107488247739732250) | Eight steps: template, MCP and AllToolsets, Epic's plugin, read the project first, a greybox level, test the route before art, a rigged character, assets in small batches. "A successful tool call doesn't mean the level is right"; never change terrain, lighting and character in one prompt | **Taken**, as the live editor's working rules (below) |
| [rewind, 5 Oct 2026](https://x.com/rewind02/status/2107063404691132663), the full course | Setup of Epic's MCP in 5.8, four builds to a playable slice, and four tests read together: a Souls-like, a boss fight (half a day of waiting, an unusable first model), Tripo's demo, and Puget Systems' sober test (changing the controls left the game unplayable; a request for a procedural city came back as hand-placed meshes). Six habits for a prompt, a safety net, four layers of testing, a bug-report form, the cost | **Taken**: the rules below. The earlier report (`reports/AI tooling for Blender and Unreal.md`) could not read it then (paywalled) |

## What changes in the project

1. **Working in the live editor** (`unreal/README.md`): commit before a session; one system per request, with what
   "done" means and what must not change; a report of every asset touched; no silent workarounds; read each tool's
   status, then a screenshot and the log after every batch; no edits while playing or compiling; a bug as steps,
   expected and actual, its cause found before a fix; two failed fixes and the work goes back to the last commit.
2. **The map is never placed by hand in the editor.** Puget's procedural city came back as hand-placed meshes. In
   SAM3D that would break the rule that the map's data is simcore's (`TERRAIN_ROADMAP.md` rule 4): a tree, field or
   house placed in the editor is not in the sight lines and not on every client. The live editor is for inspection,
   materials, imported assets, screenshots and logs; what stands where comes from simcore, drawn by the map actor.
3. **Greybox before art, gates between** is what track T already does (T2's views and costs before T5's models); the
   course's gates become the roadmap's "Done when" column, checked in the shots.
4. **Claude can work in the editor.** `CLAUDE.md` said it could not; with the editor open the MCP reaches it, and the
   commandlet and offscreen renders run without anyone at the PC.
5. **Material Maker** joins the ways to fill a texture gap (`TERRAIN_ROADMAP.md` 4.2), with Blender's own procedural
   nodes.
