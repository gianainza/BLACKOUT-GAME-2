# T2 - Texture Unit Ledger

**Group 1 - BLACKOUT** | **R2 owns** | *All five sign at Gate 4*

**Source of truth:** `CommonValues.h`, `main.cpp` `SetSceneUniforms()` / `CreateLights()`, `Shader.cpp` `SetPointLights` / `SetSpotLights`, `Hud.cpp` `Begin()`, `Skybox.cpp` `DrawSkybox()`, `level01.json` "lights".

> Nobody adds a sampler without adding a row. The symptom of a missed row is an object that renders black with no compile error anywhere.

## TABLE 1 - EVERY SAMPLER IN EVERY PROGRAM

| PROGRAM | SAMPLER | TYPE | UNIT | ADDED BY |
| :--- | :--- | :--- | :--- | :--- |
| main | `theTexture` | sampler2D | 1 | inherited |
| main | `directionalShadowMap` | sampler2D | 2 | inherited |
| main | `omniShadowMaps[0..5]` | samplerCube | 3..8 | inherited (USED) |
| main | `omniShadowMaps[6]` | samplerCube | UNSET | inherited (defaults to 0) |
| skybox | `skybox` | samplerCube | 0 | inherited (never assigned) |
| hud | `hudTexture` | sampler2D | 15 | R4 (`Hud.cpp`, `UNIT_HUD`) |

## TABLE 2 - CONSTANTS (from CommonValues.h and shader.frag)

| CONSTANT | VALUE | SOURCE |
| :--- | :--- | :--- |
| `MAX_POINT_LIGHTS` | 3 | `CommonValues.h` \| `shader.frag` |
| `MAX_SPOT_LIGHTS` | 4 | `CommonValues.h` \| `shader.frag` |
| Array size (omni) | 7 | = MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS (0..6) |
| `UNIT_SKY` | 0 | `CommonValues.h` |
| `UNIT_TEXTURE` | 1 | `CommonValues.h` |
| `UNIT_DIR_SHADOW` | 2 | `CommonValues.h` |
| `UNIT_OMNI_FIRST` | 3 | `CommonValues.h` |
| `UNIT_HUD` | 15 | `CommonValues.h` |
| `pointLightCount` | 2 | `level01.json` "lights.point_lights" |
| `spotLightCount` | 4 | `level01.json` (1 player lamp + 3 scanners) |

## TABLE 3 - WHAT IS ACTUALLY ASSIGNED THIS RUN (the marked table)

| ARRAY SLOT | UNIT | LIGHT | SET BY |
| :--- | :--- | :--- | :--- |
| `[0]` | 3 | `pointLights[0]` drone_a | `SetPointLights(3, 0)` i=0 |
| `[1]` | 4 | `pointLights[1]` drone_b | `SetPointLights(3, 0)` i=1 |
| `[2]` | 5 | `spotLights[0]` player lamp | `SetSpotLights(5, 2)` i=0 |
| `[3]` | 6 | `spotLights[1]` scanner a | `SetSpotLights(5, 2)` i=1 |
| `[4]` | 7 | `spotLights[2]` scanner b | `SetSpotLights(5, 2)` i=2 |
| `[5]` | 8 | `spotLights[3]` scanner d | `SetSpotLights(5, 2)` i=3 |
| `[6]` | unset | NOTHING | never written |

**Status:**
* **Live units in use this run:** 3, 4, 5, 6, 7, 8.
* **No idle units.**
* **One unassigned slot:** `[6]`, keeps the sampler default of unit 0.

**Why the units start where they do:**
```cpp
main.cpp    SetPointLights(pointLights, 2, UNIT_OMNI_FIRST, 0);
main.cpp    SetSpotLights(spotLights, 4, UNIT_OMNI_FIRST + 2, 2);
Shader.cpp  glUniform1i(uniformOmniShadowMap[i + offset].shadowMap, textureUnit + i);
```
* -> point lights write slots 0..1 on units 3..4
* -> spot lights write slots 2..5 on units 5..8

**Why 4 spot lights, not 3:**
* `spotLights[0]` = player's own lamp (`main.cpp` `CreateLights`, always on);
* `spotLightCount` starts at 1, then scans of the level's 3 scanners push it to 4. Confirmed in `UpdateShadowMaps` loop.

## UNIT 0 - why it stays clear

`omniShadowMaps[6]` is the 7th slot. `Shader::SetPointLights` loops only `i < pointLightCount` (2) and `SetSpotLights` loops only `i < spotLightCount` (4), writing slots 0..5. Slot `[6]` is never touched and keeps its default value of 0 as a `samplerCube`.

Putting `theTexture` (`sampler2D`) on unit 0 as well would put two DIFFERENT SAMPLER TYPES on one unit inside the main program object, which fails `glValidateProgram` with the "different types on unit N" error. That is why `main.cpp` puts `theTexture` on unit 1 (`UNIT_TEXTURE`) rather than 0.

The sky sits on unit 0 legitimately because:
* `skybox` is a SEPARATE program (`Skybox.cpp`), so its unit 0 sampler does not overlap main's unit 0 sampler for the sampler-type rule;
* the skybox sampler is never assigned a unit in code, so it takes default 0, and `Skybox::DrawSkybox` binds with `glActiveTexture(GL_TEXTURE0)`.

Hud is on unit 15 because it is a separate program (`hud.vert` / `.frag`) and `Hud::Begin()` sets it explicitly with `SetInt("hudTexture", UNIT_HUD)` plus `glActiveTexture(GL_TEXTURE0 + UNIT_HUD)`. Unit 15 is outside the main pass's 0..8 range by construction, so no collision.

## THE RULE (Activity I Part F3)

> "It is not valid to have two different sampler types pointing to the same texture image unit within a single program object."

## AUDIT NOTE 
*(added when correcting this file from the inherited template)*

The template in `05_TEMPLATES.txt` was written for an older starter with `MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS = 6` (units 3..8, two idle). Our starter has `MAX_POINT_LIGHTS = 3` and `MAX_SPOT_LIGHTS = 4`, so the array has 7 slots (units 3..9 reserved) and our scene uses all 8 live units 3..8. The row above reflects the code as it exists, not the template.

## SIGNED 
*(all five sign at Gate 4, dated on the day of the last change)*

* **R1** ________________ 
* **R2** ________________ 
* **R3** ________________
* **R4** ________________ 
* **R5** ________________ 
* **Date:** __________
