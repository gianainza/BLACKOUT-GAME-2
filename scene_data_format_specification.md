# Scene Data Format Specification (BLACKOUT)

*Owner: R1. Reader: R3 (assets) and R4 (collision, patrol paths). Everything below can be edited in `SkyboxLab/level01.json` without recompiling.*

## 1. Introduction
One JSON file describes the whole level. The loader (`SceneLoader.cpp`, parser `Json.cpp`) reads it **before** any OpenGL object exists, so a broken file produces a readable message instead of a half-built window.

## 2. General formatting rules
* **Vectors / colours:** JSON arrays of numbers. Positions, rotations, scales, colours are `[x, y, z]` / `[r, g, b]`. Floor-plane values (belt size, belt direction, scanner routes) are `[x, z]` pairs, because height means nothing for them.
* **Coordinates:** x to the right, y up, z toward the viewer at the start. Units are metres.
* **File paths:** strings relative to the folder the program runs in (`SkyboxLab/`).
* **IDs:** unique strings. Objects refer to materials by `id`; terminals refer to scanners by `id`.
* **Angles:** degrees.
* **Comments:** `// line comments` are allowed (an extension to JSON).
* **Unknown keys are ignored**, so notes can be left in the file.

## 3. Sections

### 3.0 `window` and `rendering` (optional)
| key | meaning | default |
|---|---|---|
| `window.width`, `window.height`, `window.title` | window size and title | 1366 x 768, "OBSOLETE" |
| `rendering.point_shadow_resolution` | cube-map face size for every point/spot light | 1024 |
| `rendering.directional_shadow_resolution` | directional shadow map size | 2048 |
| `rendering.omni_bias` | shadow bias for cube maps, **world units** | 0.05 |

### 3.1 `materials` (required, at least 5 for the checklist)
* `id`, `texture_path`, `specular_intensity`, `shininess` – as before.
* `colour` `[r,g,b]` – tint multiplied into the texture (default `[1,1,1]`).
* `emissive` – self-lit amount, used by the terminals (default 0).
* `tile` – world metres per texture repeat (default 2).

### 3.2 `lights`
* `directional_light`: `direction`, `color`, `ambient_intensity`, `diffuse_intensity` (the dim ambient plant light).
* `player_lamp` (optional): the droid's own spot light (`enabled`, `color`, `diffuse_intensity`, `edge`, `attenuation`). It is **spot light 0**, re-aimed every frame, and is not a gameplay light.
* `point_lights` – **drones**: omnidirectional lamps that patrol a closed loop. Required: `id`, `color`, `ambient_intensity`, `diffuse_intensity`, `attenuation` `[constant, linear, exponent]`, `patrol_speed`, `patrol_path` (`[x,y,z]` waypoints). Optional: `far_plane` (default 40). Maximum 3.
* `spot_lights` – **scanners**: ceiling cone lights. Required: `position` (the emitter, fixed), `color`, `diffuse_intensity`, `edge` (cone half angle in degrees, 3–80). Optional: `id`, `ambient_intensity`, `attenuation` (default `[1, 0.09, 0.032]`), `far_plane`, `direction`, and a `scanner` block:
  * `aim_speed` – how fast the aim point moves over the floor (m/s)
  * `route_a`, `route_b` – loops of `[x, z]` floor points the aim point follows. **A terminal hack switches between A and B.**
  
  With no `scanner` block the beam is fixed along `direction`. Maximum 3 scanners (spot 0 is the lamp).
* Every light in `point_lights` and `spot_lights` fills the **detection meter** while it sees the droid (see 3.7).

### 3.3 `objects`
`id`, `mesh_path`, `material_id`, `position` (the **centre**), `rotation`, `scale` (the **full size** of the box). `mesh_path` is `primitive:box`, `primitive:cylinder` or `primitive:sphere` (model loading is off, `USE_MODELS 0`).
* `solid` (default true): blocks the droid. Collision uses the same boxes.
* `cast_shadow` (default true): drawn into every shadow map **and** ray-tested by the CPU lit-test.
* Rotation is ignored for solid / shadow-casting objects (both tests use axis-aligned boxes) and the loader says so.

### 3.4 `conveyors`
`id`, `position` `[x,y,z]` (centre), `size` `[x, z]`, `direction` `[x, z]`, `speed` (m/s, may be negative). Optional `material_id` (default `mat_conveyor`). While the droid's centre is on the belt it is carried at `speed`; the texture scrolls at the same speed.

### 3.5 `terminals`
`id`, `position`, `controls` (array of scanner ids). Optional `material_id` (default `mat_terminal`). Each terminal also becomes a solid, shadow-casting 0.9 x 1.2 x 0.9 cabinet.

### 3.6 `skybox`
`faces`: six paths in the order **right, left, up, down, back, front**.

### 3.7 `game_logic` and `gameplay`
* `game_logic.player_start` `[x,y,z]`, `game_logic.exit_zone` `{position, radius}`, `game_logic.bounds` `[minX, minZ, maxX, maxZ]` (fail-safe so the droid can never leave the level).
* `gameplay` (all optional): `lit_threshold` (illumination at which a light counts as "seeing" the droid), `alarm_fill`, `alarm_drain`, `speed_step` (difficulty growth per level), `strike {delay, radius, cooldown}`, `hack {time, radius, cooldown}`.

## 4. What the loader does with a bad file
| problem | result |
|---|---|
| file missing / not valid JSON | `level01.json:10: ERROR not valid JSON: ':' expected after the key "omni_bias"`; program stops with a message, no crash |
| wrong type or out-of-range value | `ERROR lights.spot_lights[1] 'sweep_b': 'edge' must be a number between 3 and 80` – that entry is skipped, the rest loads |
| unknown `material_id` | warning, material 0 is used |
| texture / skybox file missing | warning at load time; the texture becomes a **magenta checker** (never silent black) |
| terminal names a scanner that does not exist | error naming both ids |
| no lights, no objects, no `game_logic`, player inside a wall | level is unplayable, message lists why |
| fewer than 2 moving lights / fewer than 4 shadow casters | warning (the brief's minimums) |

All messages are printed to the console and kept in `Scene::messages`.

## 5. A minimal valid scene
See `SkyboxLab/level01.json` (the shipped level) – it uses every section above.
