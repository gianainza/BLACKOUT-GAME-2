# T1 - WEEKLY LOGS (WEEKS 1–5)
**Name:** Rosos, John Wilfred G.  
**Role:** R3 Technical Artist  
**Project:** BLACKOUT-GAME-2  

---

## WEEK 1
======================================================================  
**Dates:** Oct 28 – Oct 30  
**Hours this week:** 3  

### WHAT I DID  
* Audited the baseline OpenGL repository structure (`Models/`, `Textures/`, `Shaders/`) and verified existing environment meshes, skybox faces, and texture files.  
* Established the relative path conventions for all art assets so future builds work cleanly across team members' machines.  
* Drafted the initial Asset Manifest (`T6`) cataloging all base `.obj` models, 2D textures, skybox cubemaps, and GLSL shader files.  

### WHAT I FINISHED  
* Completed the initial asset inventory and verified that all baseline textures and 3D models load cleanly in the starter build.  
* Created the initial draft of `Docs/T6_asset_manifest.md`.  

### WHAT I TRIED THAT DID NOT WORK  
* Initially referenced a few test textures using local absolute file paths during early scene testing, which broke loading when tested from another directory. Converted all asset references to project-relative paths (`Textures/...`, `Models/...`).  

### WHAT I AM WAITING ON, AND FROM WHOM  
* Waiting on R1 (Architecture Lead) and R2 (Lighting/Shadow Lead) to finalize the shader uniform layout and texture unit allocations so I can map material samplers accurately.  

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME  
* Nothing. Baseline assets and relative paths are ready for the team.  

### FILES I CHANGED THIS WEEK  
* `Docs/T6_asset_manifest.md`  
* `Textures/` and `Models/` (directory cleanup & verification)  

### NEXT WEEK  
* Build out the 5 distinct environment and prop materials, organize the skybox cubemap faces, and integrate them into the main scene configuration.  

---

## WEEK 2
======================================================================  
**Dates:** Oct 31 – Nov 3  
**Hours this week:** 4  

### WHAT I DID  
* Configured 5 distinct materials (Matte Concrete, Brushed Metal, Aged Wood, Rough Brick, Satin Polymer) with tailored diffuse textures, specular intensities, and shininess exponents.  
* Verified UV mapping and normal orientation across all environment meshes (`environment_floor.obj`, `wall_segment.obj`, `prop_crate.obj`, `pillar.obj`) to ensure proper lighting response.  
* Updated the Asset Manifest (`T6`) with vertex/polygon counts and texture resolutions.  

### WHAT I FINISHED  
* Integrated all 5 distinct material definitions and skybox cubemap textures into `level01.json`.  
* Updated `Docs/T6_asset_manifest.md` with complete asset metadata.  

### WHAT I TRIED THAT DID NOT WORK  
* Imported one prop `.obj` mesh without smoothed/recalculated vertex normals, which caused flat, faceted shading under point lights. Re-exported the mesh with proper vertex normals so specular highlights interpolate smoothly across surfaces.  

### WHAT I AM WAITING ON, AND FROM WHOM  
* Waiting on R1 and R2 to merge the lighting and shadow branches so I can audit texture unit bindings during integration.  

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME  
* Nothing. All 5 materials and scene assets are committed and ready for Gate 4 integration.  

### FILES I CHANGED THIS WEEK  
* `level01.json`  
* `Models/prop_crate.obj`  
* `Docs/T6_asset_manifest.md`  

### NEXT WEEK  
* Audit texture sampler units with the team, sign off on the Texture Unit Ledger (`T2`), and verify that materials, skybox, and shadows render without slot conflicts.  

---

## WEEK 3
======================================================================  
**Dates:** Nov 4 – Nov 6  
**Hours this week:** 3  

### WHAT I DID  
* Audited all texture sampler units used by diffuse/specular material textures and the cubemap skybox during the Gate 4 integration sprint.  
* Filled out and signed my section of the Texture Unit Ledger (`T2`) in coordination with R1, R2, R4, and R5 to prevent texture slot collisions.  
* Checked visual alignment of environment meshes, material specular highlights, and skybox rendering in the merged master build.  

### WHAT I FINISHED  
* Signed and verified the Texture Unit Ledger (`T2`) for all art and material texture units.  
* Confirmed that skybox, diffuse textures, and shadow maps render simultaneously without overwriting each other's texture units.  

### WHAT I TRIED THAT DID NOT WORK  
* Initially bound material diffuse textures to `GL_TEXTURE0` and `GL_TEXTURE1` without resetting active texture units after skybox rendering, which caused the skybox cubemap to bleed onto wall surfaces. Explicitly assigned dedicated texture units per the `T2` ledger and restored active unit state after draw calls.  

### WHAT I AM WAITING ON, AND FROM WHOM  
* Waiting on R5 (Misa) to run the Test Matrix (`T11`) and report any visual, UV, or material bugs in the Bug Log (`T13`).  

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME  
* Nothing. All art assets, materials, and texture unit assignments are integrated into the master build.  

### FILES I CHANGED THIS WEEK  
* `docs/TEXTURE_UNIT_LEDGER.md`  
* `Material.cpp` / `Texture.cpp` (texture unit bindings)  
* `level01.json`  

### NEXT WEEK  
* Refine material specular and shininess parameters under R2's updated point light shadows and assist with fixing any visual bugs logged by R5.  

---

## WEEK 4
======================================================================  
**Dates:** Nov 7 – Nov 9  
**Hours this week:** 3  

### WHAT I DID  
* Verified material contrast and readability under the updated omnidirectional point light shadows during Gate 5 lighting and proof testing.  
* Adjusted diffuse albedo and specular intensity values on dark surfaces so shadow edges remain clearly visible for the CPU vs. GPU Required Proof comparisons.  
* Started compiling exact byte-level texture and mesh sizes for the Material Table (`T7`) and Memory Budget (`T8`).  

### WHAT I FINISHED  
* Tuned all 5 materials so lit vs. shadowed regions are visually distinct across all sample test positions.  
* Completed initial raw byte calculations for all loaded diffuse textures, cubemap faces, and vertex buffers.  

### WHAT I TRIED THAT DID NOT WORK  
* Tried setting very high specular shininess (256) with bright white specular colors on metallic props, which created harsh glare spots that made it hard to visually distinguish shadow boundaries in debug/proof checks. Lowered specular intensity and balanced shininess values (32–64) for cleaner visual contrast.  

### WHAT I AM WAITING ON, AND FROM WHOM  
* Waiting on R2 (Tan) to confirm final shadow map resolution and face count parameters so I can finalize the exact shadow map byte calculations in the Memory Budget (`T8`).  

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME  
* R5 (Misa) needs my finalized Material Table (`T7`), Memory Budget (`T8`), and S3 Technical Report section in Week 5 to compile the final report.  

### FILES I CHANGED THIS WEEK  
* `level01.json` (material specular/shininess tuning)  
* `Docs/T7_material_table.md` (draft)  

### NEXT WEEK  
* Finalize the 5 distinct materials in the Material Table (`T7`), complete the Memory Budget (`T8`) strictly in bytes (including shadow maps), and submit my report section (S3) to R5.  

---

## WEEK 5
======================================================================  
**Dates:** Nov 10 – Nov 11  
**Hours this week:** 3  

### WHAT I DID  
* Finalized the 5 distinct materials and documented their diffuse, specular, shininess, and texture properties in the Material Table (`T7`).  
* Calculated the complete Memory Budget (`T8`) strictly in bytes, accounting for all 2D textures, 6-face skybox cubemap textures, vertex/index buffers, and omnidirectional shadow cube maps (`width × height × bytes-per-pixel × 6 faces × number of point lights`).  
* Verified all asset paths are relative so R1's Clean Clone Test 2 (`T5`) loads every model and texture cleanly on a fresh machine.  
* Wrote and submitted my Technical Report section (S3: Assets, Materials & Memory Budget) to R5.  

### WHAT I FINISHED  
* Completed and verified the Material Table (`T7`) and byte-exact Memory Budget (`T8`).  
* Delivered section S3 for the final 2500–4000 word Technical Report.  

### WHAT I TRIED THAT DID NOT WORK  
* Initially estimated the omnidirectional shadow map memory as a single 2D depth texture per light, which undercounted VRAM usage by a factor of 6. Corrected the Memory Budget (`T8`) formula to multiply by all 6 cube map faces per point light and convert dimensions strictly to bytes.  

### WHAT I AM WAITING ON, AND FROM WHOM  
* Waiting on R5 (Misa) to finish compiling all team sections (S1–S5) and submit the final Technical Report and build.  

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME  
* Nothing. All R3 deliverables (`T6`, `T7`, `T8`, `T2` sign-off, and S3) are complete.  

### FILES I CHANGED THIS WEEK  
* `Docs/T7_material_table.md`  
* `Docs/T8_memory_budget.md`  
* `Docs/T6_asset_manifest.md` (final verification)  

### NEXT WEEK  
* None (Sprint completed).