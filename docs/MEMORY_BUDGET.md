# T8 - Memory Budget (Byte-Exact)
**Author:** Rosos, John Wilfred G. (R3 Technical Artist)  
**Project:** BLACKOUT-GAME-2  

## 1. 2D Textures (Uncompressed GPU VRAM)
*Formula: `Width × Height × Bytes Per Pixel (4 for RGBA8)`*

| Texture Name | Dimensions | Bytes/Pixel | Calculation | Total Bytes |
| :--- | :--- | :--- | :--- | :--- |
| `concrete_diffuse.png` | `1024 × 1024` | 4 | `1024 × 1024 × 4` | `4,194,304 B` |
| `metal_diffuse.png` | `1024 × 1024` | 4 | `1024 × 1024 × 4` | `4,194,304 B` |
| `wood_diffuse.png` | `512 × 512` | 4 | `512 × 512 × 4` | `1,048,576 B` |
| `brick_diffuse.png` | `1024 × 1024` | 4 | `1024 × 1024 × 4` | `4,194,304 B` |
| `polymer_diffuse.png` | `512 × 512` | 4 | `512 × 512 × 4` | `1,048,576 B` |
| **Subtotal (2D Textures)** | | | | **`14,680,064 B`** |

## 2. Skybox Cubemap (6 Faces)
*Formula: `Width × Height × Bytes Per Pixel × 6 Faces`*

| Asset | Face Dimensions | Bytes/Pixel | Faces | Calculation | Total Bytes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| Skybox Cubemap | `1024 × 1024` | 4 | 6 | `1024 × 1024 × 4 × 6` | **`25,165,824 B`** |

## 3. Omnidirectional Point Light Shadow Maps
*Formula: `Width × Height × Depth Bytes (4 for GL_DEPTH_COMPONENT32F) × 6 Faces × Point Light Count`*

| Shadow Buffer | Resolution | Bytes/Texel | Faces | Active Lights | Calculation | Total Bytes |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Omni Shadow Cubemaps | `1024 × 1024` | 4 | 6 | `[N]` (e.g., 2) | `1024 × 1024 × 4 × 6 × [N]` | **`[50,331,648] B`** |
| Directional Shadow Map | `2048 × 2048` | 4 | 1 | 1 | `2048 × 2048 × 4 × 1` | **`16,777,216 B`** |

## 4. Geometry Buffers (VBO & IBO)
*Formula: `(Vertex Count × Stride in Bytes) + (Index Count × 4 Bytes)`*
*(Assuming standard vertex layout: 3 pos + 2 UV + 3 normal = 8 floats = 32 bytes per vertex)*

| Mesh Asset | Vertex Count | Vertex Bytes (`×32`) | Index Count | Index Bytes (`×4`) | Total Bytes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `environment_floor.obj` | `[V]` | `[V × 32]` | `[I]` | `[I × 4]` | `[Total] B` |
| `wall_segment.obj` | `[V]` | `[V × 32]` | `[I]` | `[I × 4]` | `[Total] B` |
| `prop_crate.obj` | `[V]` | `[V × 32]` | `[I]` | `[I × 4]` | `[Total] B` |
| `pillar.obj` | `[V]` | `[V × 32]` | `[I]` | `[I × 4]` | `[Total] B` |
| **Subtotal (Geometry)** | | | | | **`[Sum] B`** |

## 5. Total GPU Memory Footprint Summary
* **2D Textures:** `14,680,064 B`
* **Skybox Cubemap:** `25,165,824 B`
* **Shadow Maps (Directional + Omni):** `[67,108,864] B`
* **Vertex & Index Buffers:** `[Sum] B`
* **Grand Total:** **`[Total Bytes] B`** (`[Total MB] MB`)