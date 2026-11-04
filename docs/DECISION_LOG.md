# T3 - Decision log  (R1 keeps it, anyone may add)

**Entries 1-10 below were written when the project was ported onto Activity J; the group must read each one, correct it where it disagrees with what you actually decided, and add the date.** An entry without a rejected alternative is worth half an entry.

| # | date | decision | we rejected | because |
|---|---|---|---|---|
| 1 | | ASSIMP / model loading: **off** (`USE_MODELS 0`); every shape is generated in `MeshFactory.cpp` | loading `.obj` files | no assimp in the repo, and generated shapes are marked more generously |
| 2 | | GL version **4.6** with `#version 460` everywhere (`Window.cpp` already asked for 4.6) | 3.3 fallback | the project machine supports 4.6. If a lab machine refuses, record the machine and driver here. |
| 3 | | shadow map resolution: **1024** per cube face, **2048** directional (in `level01.json`) | 512 / 2048 cube maps | 512 gave visible blocky edges; 2048 costs 96 MiB per light. 1024 -> texel footprint 2d/1024 = 0.0117 m at d = 6. |
| 4 | | the level format is **JSON**, parsed by our own `Json.cpp` | nlohmann `json.hpp`; the old text format | B14 (nothing outside the repo to install), and our parser can report the *line* of an error |
| 5 | | **collision and the lit-test read the same `Scene` boxes** that are drawn and drawn into the shadow maps | a separate hard-coded collision list | two copies of the level can disagree (R4 D2) |
| 6 | | the game asks "is the droid lit?" on the **CPU** (`LitTest.cpp`); the GPU twin is only used for proof tools | reading the GPU result back every frame | a per-frame `glReadPixels` stalls the pipeline; the CPU test is deterministic. The GPU twin exists to *prove* the CPU is right (P3, E2). |
| 7 | | lit-test has **no normal term and no colour term** | N.L like the shader | the droid is a capsule, not a flat face; and a scanner turning red must not change whether you are seen |
| 8 | | scanners are Activity J **spot lights**; drones are **point lights**; the droid's lamp is spot 0 | custom light types | reuses the inherited omnidirectional shadow maps and the spot cone + fade (B9) |
| 9 | | detection full = **lock-on**, strike 1.9 s later kills (not an instant loss) | instant loss when the meter fills (the brief's wording) | gives the player something to *do* once detected - see README "Deviations". **Ask the instructor if this is acceptable.** |
| 10 | | **E1 extension** (cache unmoved cube maps) | E2 / E3 | scanners are fixed, so most maps never need re-rendering; E2 (HUD light meter) is also partly built and can be added to the proof. |

## Outside or generated code (must be declared, with file and lines)

| file | lines | where it came from |
|---|---|---|
| `Json.h`, `Json.cpp`, `Scene.h/.cpp`, `SceneLoader.h/.cpp`, `MeshFactory.h/.cpp`, `LitTest.h/.cpp`, `Game.h/.cpp`, `Hud.h/.cpp`, `main.cpp`, `tests/*`, `tools/make_textures.py`, the changes to `Light`, `PointLight`, `SpotLight`, `Shader`, `Texture`, `Material`, `Camera`, `Window`, all shader edits listed in `SHADER_CHANGE_LOG.md`, `level01.json` | whole files / the listed edits | **written with the AI assistant Claude (Anthropic), 31 Oct 2026**, adapting an earlier standalone version of this game. Every member must read and be able to explain the file(s) they own before the viva - declared code is marked on whether you understood it. |
| `Camera`, `Window`, `Light*`, `Shader`, `ShadowMap`, `OmniShadowMap`, `Skybox`, `Mesh`, shaders (unchanged parts) | - | Activity J starter project (course material, no declaration needed) |

## Help from other groups
| date | group | what |
|---|---|---|
| | | |
