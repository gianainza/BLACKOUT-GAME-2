# T9 - Shader change log  (R2)

Seeded with every change made while moving Activity J's shaders to OBSOLETE. **Add before/after screenshots and keep logging your own tuning.** Entries must say WHY.

| # | file | what changed | why |
|---|---|---|---|
| 1 | all `Shaders/*` | `#version 330` -> `#version 460` | B1 asks for GL 4.6 and `#version 460` in every shader. `Window.cpp` already requested a 4.6 context. If a lab machine refuses 4.6, change `Window.cpp` and the `#version` lines together and log it in the decision log (entry 2). |
| 2 | `shader.frag` (`CalcLightByDirection`) | diffuse term `dot(N, normalize(direction))` -> `dot(N, -normalize(direction))` | `direction` is the way the light *travels*, so a surface faces the light when N . (-direction) > 0. Activity J used `+direction` and only worked because its demo meshes had inward-pointing normals (the floor normal was `(0,-1,0)`). The generated meshes use correct outward normals, so the sign was fixed. Before: a normally lit box face was black. |
| 3 | `shader.frag` (`CalcDirectionalShadowFactor`) | bias `max(0.05*(1 - dot(normal, lightDir)), 0.0005)` -> `... dot(normal, -lightDir)` | same sign fix: the slope term must be small when the surface faces the light and large at grazing angles. |
| 4 | `shader.frag` | hard-coded `float bias = 0.05;` -> `uniform float omniBias` | the omni bias is in **world units**. A uniform lets the demo raise it live (keys `[` `]`, key 3) to show shadows detaching, and lets R2 do the bias study without recompiling. Default still 0.05, set from `level01.json`. |
| 5 | `shader.frag` | added `materialColour`, `emissive`, `alphaMul` | tinted materials (terminals), self-lit objects (lamps, beacon) and the additive scanner beams / strike columns. Default behaviour (1, 0, 1) equals Activity J. |
| 6 | `shader.vert` | added `uniform vec2 uvOffset`, `TexCoord = tex + uvOffset` | scrolls the conveyor-belt texture in the belt's direction. |
| 7 | `shader.frag` | `CalcPointLight` split into `CalcPointLightCore(..., cone, gameIndex)`; added globals `gIllum`, `gMask` | **gameplay accounting.** For every drone and scanner the shader sums `(1 - shadowFactor) * cone * diffuseIntensity / attenuation` and sets a mask bit when the shadow factor is below 0.5. This is the GPU twin of `ComputeIllumination` in `LitTest.cpp`. Spot light 0 (the droid's lamp) is excluded (`gameIndex = -1`). |
| 8 | `shader.frag` | `debugMode` 1 and 2 | 1 paints the *game's* opinion on the world (red = scanned, blue = hidden) - the demo overlay (key V). 2 writes `(gIllum, gMask)` into a float framebuffer; `ProbeGPU` in `main.cpp` reads one pixel back (keys G, L, F9). |
| 9 | `shader.frag` | `MAX_SPOT_LIGHTS` 3 -> 4 | droid lamp + three scanners. Must match `CommonValues.h`. The omni sampler array grows from 6 to 7 (texture unit ledger). |
| 10 | `Light.cpp`, `DirectionalLight.cpp` (C++) | `Light` no longer allocates a flat `ShadowMap` for every light | Activity J created a 2D shadow map in the base class and then overwrote it with an `OmniShadowMap` for point/spot lights, leaking a depth texture and framebuffer per light. Only `DirectionalLight` creates one now. |

Cache note (E1): `main.cpp` skips an omni pass when the light has not moved since its map was rendered. The scanners' emitters are fixed, so their cube maps are rendered once; the drones and the droid's lamp move and are re-rendered every frame. Key C turns the cache off for the comparison.
