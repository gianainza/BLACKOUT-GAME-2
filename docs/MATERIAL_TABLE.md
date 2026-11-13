# T7 - Material Table
**Author:** Rosos, John Wilfred G. (R3 Technical Artist)  
**Project:** BLACKOUT-GAME-2  

## 1. Configured Materials (5 Distinct Profiles)
*Note: Specular and shininess values are balanced (32–64) to preserve shadow boundary readability under omnidirectional point lights.*

| Material ID | Material Name | Diffuse Texture (`GL_TEXTURE0`) | Specular Texture / Intensity | Shininess Exponent | Visual & Lighting Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **MAT_01** | Matte Concrete | `Textures/concrete_diffuse.png` | `0.15` (Low specular) | `32.0` | Broad, soft highlights; keeps dark floor shadow edges crisp. |
| **MAT_02** | Brushed Metal | `Textures/metal_diffuse.png` | `0.75` (High specular) | `64.0` | Distinct metallic sheen without blowing out shadow contrast. |
| **MAT_03** | Aged Wood | `Textures/wood_diffuse.png` | `0.25` (Low-mid specular) | `32.0` | Warm diffuse response for crates and props. |
| **MAT_04** | Rough Brick | `Textures/brick_diffuse.png` | `0.10` (Minimal specular) | `32.0` | High diffuse absorption; prevents glare on vertical pillars. |
| **MAT_05** | Satin Polymer | `Textures/polymer_diffuse.png` | `0.50` (Medium specular) | `48.0` | Balanced mid-range specular reflection for interactive/accent props. |

## 2. Texture Unit Assignments (Aligned with T2 Ledger)
* `GL_TEXTURE0`: Active Material Diffuse Map
* `GL_TEXTURE1`: Active Material Specular Map
* `GL_TEXTURE2`: Directional Shadow Map
* `GL_TEXTURE3`: Skybox Cubemap
* `GL_TEXTURE4+`: Omnidirectional Point Light Shadow Cubemaps