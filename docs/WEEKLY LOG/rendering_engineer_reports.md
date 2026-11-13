# Rendering Engineer Weekly Reports

**Name:** Tan, Charles James I.
**Role:** R2 Rendering Engineer

## WEEK 1

**Dates:** Oct 29-31
**Hours this week:** 1 Hour

### WHAT I DID

* I changed the project files, specifically Shader.cpp and all 9 files in the Shaders/ directory.
* Identified that while Window.cpp requests GL 4.6, every shader file was still using #version 330. 
* I Updated all 9 shaders to #version 460.
* I also added a debugShadowMode uniform to shader.frag to prepare for Proof P3 (visualizing the GPU shadow factor for comparison).

### WHAT I FINISHED

* The #version 460 change.
* The debugShadowMode implementation.
* T2 Texture Unit Ledger (inherited rows mapped out).
* T9 Shader Change Log (started).

### WHAT I TRIED THAT DID NOT WORK

* I originally wanted the shadow debug mode to show blended colors, but it would be too hard to read. I switched to simple black and white (white is lit, black is shadow) so it's easier to test later.

### WHAT I AM WAITING ON, AND FROM WHOM

* I am waiting for the scene loader to be updated so the point lights can actually move. I need the lights to move before I can test if the shadows update correctly for the game mechanic.

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME

* Nothing. My changes are isolated to the shader files.

### FILES I CHANGED THIS WEEK

* Shaders/shader.vert, shader.frag, skybox.vert, skybox.frag, directional_shadow_map.vert, directional_shadow_map.frag, omni_shadow_map.vert, omni_shadow_map.geom, omni_shadow_map.frag

### NEXT WEEK

* Draft the D3 bias derivation.
* To test the debugShadowMode output.

## WEEK 2

**Dates:** Nov 1 - Nov 3
**Hours this week:** 4

### WHAT I DID

* Read through R1's massive engine overhaul, especially how LitTest.cpp (CPU) and shader.frag (GPU) calculate light.
* Checked the 20-tap PCF shadows in the omni shader.
* Started the D3 bias derivation draft.

### WHAT I FINISHED

* Drafted the D3 Bias Derivation.
* Figured out the difference between the directional bias (normalized depth) and omni bias (world units).

### WHAT I TRIED THAT DID NOT WORK

* I initially thought I needed to write a whole separate debug shader to test the shadows for the P3 proof. But then I saw R1 added `debugMode == 2` in the main fragment shader, which reads the data way easier. I scrapped my extra shader idea.

### WHAT I AM WAITING ON, AND FROM WHOM

* Waiting on R1 to finish the scene loader so we can actually play the game and test the lights.

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME

* Nothing.

### FILES I CHANGED THIS WEEK

* Shaders/shader.frag (reviewed)
* Docs/D3_bias_derivation_draft.txt

### NEXT WEEK

* Run the P3 proof and fix the drone lights for R1.

## WEEK 3

**Dates:** Nov 4 - Nov 6
**Hours this week:** 5

### WHAT I DID

* I disabled the white drone lights because they looked like glowing bulbs. I commented out the DrawMesh code in main.cpp but kept the JSON data so we don't fail B8.
* Ran the F9 P3 test to compare CPU vs GPU lighting.
* Updated the T2 Texture Ledger.
* Tested the '1', '2', and '3' break keys for the demo.

### WHAT I FINISHED

* Got the p3_results.csv file. It shows where the CPU and GPU agree and disagree.
* Fixed the T2 Ledger to include the HUD unit.

### WHAT I TRIED THAT DID NOT WORK

* I almost deleted the drones entirely from level01.json. But I realized we would lose marks for the "two point lights" requirement, so I just hid the visuals instead.
* Accidentally pushed CppProperties.json to GitHub. Had to run `git rm --cached` and update .gitignore so it doesn't fail the clean clone test (B14).

### WHAT I AM WAITING ON, AND FROM WHOM

* R1, R3, R4, R5 to sign my T2 ledger.

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME

* R5 needs the p3_results.csv for the test matrix, but I already generated it.

### FILES I CHANGED THIS WEEK

* main.cpp (just the DrawMesh line for drones)
* Docs/T2_texture_unit_ledger.txt

### NEXT WEEK

* Write the S2 report section.

## WEEK 4

**Dates:** Nov 4 - Nov 6
**Hours this week:** 4

### WHAT I DID

* Put my shaders into the one build with everyone else's work (Gate 4)
* Finished my part of the texture unit list. Every texture and its number.
* Fixed problems where my shaders expected something the merged code did not do
* Kept working on the bias document. Checked that the two biases use different units in the real code.
* Got one point light working with a cube shadow map
* Made the debug view work. It shows shadow as colour. I need this for the P3 table.

### WHAT I FINISHED

* Two point lights with cube shadow maps. One draw pass per light.
* Debug view working
* Texture unit list done and signed by all five
* Shader change log entries 1 to __

### WHAT I TRIED THAT DID NOT WORK

* Tried to pick the shadow map by light number inside a loop. The driver said no on this machine. I changed to reading the shadow map using a switch and wrote it in the log.
* Tried to make the light move itself. That copied R4's work. We agreed R4 gives the position and my code uses it. I wrote this in the decision log. (Do not leave this empty. The rubric says an empty one means you are either not doing the hard part or not writing it down.)

### WHAT I AM WAITING ON

* R4: the final CPU light formula, so I can start P1 and P3
* R1: the scene file must send light paths to my light classes
* R5: measuring time on all three computers in week 5

### WHAT I AM BLOCKING

* R4 needs my light numbers to finish the CPU formula. The starter uses constant 0.3, linear 0.2, exponent 0.1 at main.cpp line 332.
* R5 cannot measure until my lighting stops changing. It is close now.

### FILES I CHANGED

* Shaders/shader.frag, Shaders/omni_depth.geom, Shaders/omni_depth.frag, Shader.cpp, OmniShadowMap.cpp, PointLight.cpp, T9, T2

### NEXT WEEK (week 5, Gate 5)

* Write the P2 prediction before I measure anything
* Do 20 rows of the P3 table. Five rows near shadow edges.
* Write P4 (the disagreements)
* Pick my extension
* Give R5 the unlit and lit times

### THING THAT TURNED OUT NOT TO MATTER

* The driver error about the loop looked like a big problem. It was not. The fix was small. I still wrote it down because at the time it looked like it would stop us.

## WEEK 5

**Dates:** Nov 5 - Nov 7
**Hours this week:** 3

### WHAT I DID

* Finished lighting and shadows. This was the Gate 5 target.
* Both point lights move now and their shadow maps update when they move. This took longer than I thought.
* Wrote the P2 prediction and put the date on it first, before I measured anything
* Started the P3 table. Got . rows done. 5 of them are near a shadow edge.
* Started P4
* Did the first time tests with R5 on three computers
* Wrote the answer to the far plane question from Activity I G6

### WHAT I FINISHED

* P2 prediction sheet
* P3 rows 1 to .
* Bias document parts A, B and C
* Shader log up to entry 

### WHAT I TRIED THAT DID NOT WORK

* First try at P3 I used glReadPixels to read the shadow. It was really slow and only gave me one pixel. I changed to reading the pixel from the debug colour view instead. Much faster.
* I guessed that if I doubled the far plane, the omni bias would also double. That was wrong. The far plane is in two places in the code and I only changed one at first. My guess was wrong and I wrote down why.

### WHAT I AM WAITING ON, AND FROM WHOM

* R4 to finish the CPU formula so my CPU column is final
* R5 to finish the three computer time table

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME

* R5 needs my bias notes and the reason unit 0 is kept clear. I am sending these this week.

### FILES I CHANGED THIS WEEK

* shader.frag, T9, T14, R2_D3_BiasDerivation.md, R2_P3_MeasurementTable.txt, T3

### NEXT WEEK

* Finish P3 and P4
* Clean up my shader log to 10 or more entries
* Practice for the viva. Pick two shader entries and write four sentences for each.
* Give the S2 draft to R5

### LOOKING BACK AT EARLIER WEEKS

* In week 3 I said I would fix the loop problem with the shadow maps. It is fixed and it has not come back