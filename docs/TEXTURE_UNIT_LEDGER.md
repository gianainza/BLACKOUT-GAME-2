# T2 - Texture unit ledger  (R2 adds rows, all five sign)

Why this exists: two sampler *types* on one unit in one program is a validation failure (Activity I F3), and the symptom is an object that draws black with no error.

| program | sampler | type | unit | added by |
|---|---|---|---|---|
| skybox | `skybox` | samplerCube | 0 (`UNIT_SKY`, default - never assigned with glUniform1i) | inherited |
| main | `theTexture` | sampler2D | 1 (`UNIT_TEXTURE`) | inherited |
| main | `directionalShadowMap` | sampler2D | 2 (`UNIT_DIR_SHADOW`) | inherited |
| main | `omniShadowMaps[0..1]` | samplerCube | 3, 4 = the two **drone** point lights | inherited, used by OBSOLETE |
| main | `omniShadowMaps[2..5]` | samplerCube | 5 = droid lamp (spot 0), 6, 7, 8 = the three **scanners** | inherited, used by OBSOLETE |
| main | `omniShadowMaps[6]` | samplerCube | never assigned -> default 0 (a cube map, which is why unit 0 must never hold a sampler2D in this program) | OBSOLETE (array grew from 6 to 7) |
| hud | `hudTexture` | sampler2D | 15 (`UNIT_HUD`, own program, only bound while the HUD is drawn) | OBSOLETE |

Reserved range is 3..9 (`MAX_POINT_LIGHTS 3` + `MAX_SPOT_LIGHTS 4` = 7 samplers). With the shipped level 3..8 are in use.
Point lights start at 3 and spot lights at `3 + pointLightCount` (`SetPointLights(.., UNIT_OMNI_FIRST, 0)`, `SetSpotLights(.., UNIT_OMNI_FIRST + pointLightCount, pointLightCount)`).

**Unit 0 is reserved for the sky.** Nothing else is ever bound to it, except that `Hud::Init` briefly binds the HUD strip to the *currently active* unit while creating it, then unbinds it.

Signed: R1 ______ R2 ______ R3 ______ R4 ______ R5 ______  date ______
