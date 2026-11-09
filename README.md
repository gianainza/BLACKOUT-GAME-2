# OBSOLETE  -  Group 1 (BLACKOUT mechanic), built on the Activity J project

You are a tiny, defective maintenance droid in a fully automated recycling plant, and you are scheduled for the incinerator.
Overhead **scanners** (a streaked beam that lands as a spinning radar disc on the floor) and patrolling **drones** (white lamps) fill your **detection** meter while they see you.
A full meter **locks that light on** (it turns red) and **0.8 s later** it calls down an **incineration strike** (it gets shorter on later levels, never below 0.45 s). **Sprint** out of the red ring.
Hide behind scrap metal (real cube-map shadows), ride the **conveyor belts**, and **hack terminals** to reroute the scanners.
Follow the **glowing green pathway** to the doorway in the wall - but the door is **locked**: find the 4-digit code first.

This is the Activity J (skybox) project with the game added on top. Every Activity J class is still here (`Window`, `Camera`, `Shader`, `Mesh`, `Texture`, `Material`, `Light`, `DirectionalLight`, `PointLight`, `SpotLight`, `ShadowMap`, `OmniShadowMap`, `Skybox`); the changes to them are listed in `docs/SHADER_CHANGE_LOG.md` and below.

## Build and run (Windows, Visual Studio 2019/2022/2026, "Desktop development with C++")
1. One time on the machine: open *Developer PowerShell for VS* and run `vcpkg integrate install`.
2. Open `Skybox Activity/ActivityJ_Skybox/SkyboxLab.sln`, set **x64**, press **F5**. The first build downloads GLEW, GLFW and GLM through `SkyboxLab/vcpkg.json` (a few minutes).
3. The working directory is the `SkyboxLab` folder, so `level01.json`, `Shaders/` and `Textures/` are found. A post-build step also copies them next to `OBSOLETE.exe`, so double-clicking the exe works too.
4. No absolute paths anywhere (Activity J's project pointed at `E:\Computer Graphics\...`; that is gone).
5. If the level file is broken the console prints `level01.json:LINE: ERROR ...` and the program stops cleanly.

## Controls
WASD move (relative to the camera) - mouse turns the camera around the droid - **Left Shift sprint** - **Left Ctrl sneak** - **hold E** next to a terminal to hack it - F droid lamp - at the locked door: **digits 0-9** type the code, **Backspace** deletes, **Enter** submits - Enter start - R restart / next level - P pause - Esc quit.

Debug / demo keys (the digit keys `1/2/3/0` are **ignored while the door keypad is active**, because then they type the code): `1` GPU shadows off, `2` CPU lit-test ignores walls, `3` huge shadow bias, `0` fix, `[` `]` bias, `V` paint the game's opinion on the world (red = scanned, blue = hidden), `C` shadow cache on/off (E1), `Y` vsync, `T` timing run (`FrameTimeLogger.h`: 120 warm-up + 300 measured frames, writes `frame_times_<scene>_run<N>.csv` and appends a row to `frame_times_summary.csv`; press `Y` first to turn vsync off), `L` live GPU probe number on the HUD (E2), `G` print CPU vs GPU at the droid, `F9` run the P3 study (writes `p3_results.csv`).

## The brief's checklist - where each item lives
| | item | where | note |
|---|---|---|---|
| B1 | GL 4.6, `#version 460` | `Window.cpp`, every shader | |
| B2 | indexed draws | `Mesh.cpp` (`glDrawElements`), `MeshFactory.cpp` | box = 24 vertices (see header comment) |
| B3 | resize | `Window::handleResize`, projection rebuilt every frame in `main.cpp` | |
| B4 | camera + deltaTime movement | `Camera.cpp` (orbit mode), `Game::Update` (`PLAYER_SPEED * dt`) | |
| B5 | five visible materials | `level01.json` (7: concrete, brick, metal, wood, hazard, terminal, conveyor) | **take the one-frame screenshot** |
| B6 | ambient + diffuse + specular | `shader.frag` | |
| B7 | directional light + shadow | `DirectionalShadowMapPass` | |
| B8 | >= 2 patrolling point lights, omni shadows | the two drones (`point_lights`) | |
| B9 | spot light with cone + fade | the scanners and the droid's lamp | |
| B10 | skybox | `Skybox.cpp`, night faces | |
| B11 | level from a file | `level01.json`, `SceneLoader.cpp`, `scene_data_format_specification.md` | |
| B12 | HUD with `glm::ortho` | `Hud.cpp` (`Hud::Begin`) | letter strip built in code, unit 15 |
| B13 | win / lose / restart | `Game.cpp`, `docs/STATE_MACHINE.md` | |
| B14 | clean-machine build | `vcpkg.json`, relative paths | **run the fresh copy test (T5) on a machine that has never built it** |

## The BLACKOUT mechanic spec - where each item lives
* patrolling point lights on file-defined paths, omnidirectional shadows: two drones.
* alarm meter on the HUD driven by a **real illumination test**, not a trigger volume: `LitTest.cpp` (CPU) = `gIllum` in `shader.frag` (GPU).
* four or more hiding-shadow casters: 4 scrap stacks, 6 crates, 5 pillars, 2 barriers, the terminals.
* win by reaching the exit, loss by a strike landing on the droid.

### Deviations from the spec text (decide with your instructor)
1. **Loss condition.** The spec says "the meter filling completely is a loss". Here a full meter *locks the light on* and the **strike 0.8 s later** is the loss (`gameplay.strike.delay` in the level file; the per-level shortening stops at `gameplay.min_strike_delay`). See decision log entry 9.
2. **Scanners** have fixed emitters and a moving aim point, so their cube maps never move. That is what makes E1 (cache) worthwhile, but the spec's "lights moving on paths" is satisfied by the **two drones**.
3. **Theme:** the game was re-themed from "BLACKOUT yard" to "OBSOLETE recycling plant". Confirm with your instructor that the re-theme is acceptable.

## Mechanics added in the second iteration (all numbers live in `level01.json`)
| mechanic | what it does | level-file keys |
|---|---|---|
| **Short strike** | the strike lands 0.8 s after lock-on; later levels shorten it, never below 0.45 s | `gameplay.strike.delay`, `gameplay.min_strike_delay` |
| **Speed** | Shift sprint x1.35, Ctrl sneak x0.55 (sneak wins), being scanned x0.7 **only while no light is locked on**, oil x0.55, product never below 0.35. Belts are unaffected. | `movement` |
| **Oil puddles** | glossy discs that slow you; not solid, no shadow | `oil_puddles`, material `mat_oil` |
| **Lamp battery** | the F lamp drains 4 %/s and recharges 2 %/s when off; at 0 it is forced off until 15 %; below 25 % it flickers. It is **not** part of the detection test. | `lights.player_lamp` |
| **Destructible cover** | a strike within its radius destroys crates and barriers (never walls, pillars, scrap stacks or terminals); scorch marks stay; Reset restores them | `objects[].destructible` |
| **Code door** | the exit door is locked with a random lock colour. 10 code terminals: 4 genuine (lock colour, true digit, one per slot) and 6 decoys (other colours, wrong digit). Hack for 2 s, collect digits in the **notebook** (colours are NOT recorded), type the code at the door. A wrong code adds detection and sends the nearest drone to the door. | `terminals[].kind = "code"`, `code_puzzle` |
| **Noise** | standing 0.5 m, sneaking 1.5, walking 4.5, sprinting 9, hacking 5, +1.5 with the lamp on. A drone that is patrolling and within that radius flies to the noise, hovers 2 s, then returns to its loop. Scanners ignore noise. | `noise` |
| **Random seed** | one `std::mt19937` in `Game`, seeded from the clock; `OBSOLETE.exe --seed N` makes the puzzle deterministic | command line |

`NOTEBOOK_SHOWS_COLOR` (in `Game.h`) is `false`; set it to `true` to show the colour of every collected digit (a much easier game).

## P1: the CPU lit-test as a formula (write the justification in your own words)
For each light i with d = |p - light_i|, p = the floor at the droid's feet (y = 0.01):

```
visible_i = d <= far_i  AND  cone_i > 0  AND  segment(p, light_i) hits no shadow-casting box
cone_i    = 1                                     (drone)
          = 1 - (1 - c)/(1 - cos(edge))  if c = dot(normalize(p - emitter), aim) > cos(edge), else 0   (scanner = Activity J spot light)
att_i     = exponent*d*d + linear*d + constant
illum_i   = visible_i * cone_i * diffuseIntensity_i * power_i / att_i
a light "sees" the droid when illum_i >= lit_threshold;  its detection fills while that is true and drains otherwise
```
No normal term (the droid is a capsule) and no colour term (a scanner turning red must not change who is seen).
`F9` produces the 20-sample CPU-vs-GPU table for P3; **write your P2 prediction on the T14 sheet before you press it.**

## Tests (no GPU needed)
Timing harness: `g++ -std=c++14 -I. ../tests/timing_test.cpp -o timing_test && ./timing_test` (from `SkyboxLab`).
```
cd "Skybox Activity/ActivityJ_Skybox/SkyboxLab"
g++ -std=c++17 -I. ../tests/logic_test.cpp Scene.cpp SceneLoader.cpp Json.cpp LitTest.cpp Game.cpp -o logic_test && ./logic_test
g++ -std=c++17 -O2 -I. ../tests/bot.cpp Scene.cpp SceneLoader.cpp Json.cpp LitTest.cpp Game.cpp -o bot && ./bot --seed 1
```
`bot` options: `--seed N`, `--levels N`, `--only N`, `--verbose`, `--diag`, and `--set name=value` to relax one rule for an ablation (`strike`, `hack`, `noise`, `nhack`, `nwalk`, `nsprint`, `nlamp`, `lit`, `step`, `minstrike`, `droneR`, `fill`).
**Honest status of the bot:** with the rules exactly as shipped it plays the whole level (four hacks, code, exit, zero wrong codes) but escapes only about 5 of 12 levels over seeds 1-3. `docs/BOT_ABLATION.md` shows which rule decides that and what to change.
(needs GLM on the include path). `logic_test` checks the loader against broken files, the cone and line of sight, lock-on timing for a scanner and a drone, strikes, dodging, belts and hacking. `bot` is a look-ahead bot that proves the level is beatable.

## What is NOT done here (the PDF marks all of it - it is yours)
Weekly logs, the report (S1-S5), the three-machine frame-time table (T12, use key `T`), the memory budget (T8), the test matrix (T11), the bug log (T13), the prediction sheets (T14), the six cube-map screenshots, the five-materials screenshot, the fresh-copy tests (T5), peer forms. Seeded versions of T2, T3, T4, T6, T9, T10 are in `docs/` - read them, correct them, sign them.

## Honest status
Verified without a GPU: every source file compiles against stand-in GL/GLM/GLFW headers; the loader, lit-test and game logic pass the headless tests; the bot beats levels 1-4. **Not verified: the picture on a real GPU.** If a shader fails to compile, the console prints the line; send it to whoever owns the shader (R2).
