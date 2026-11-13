# Technical Director Weekly Reports

**Name:** AINZA, GIANNA TRISHA M.
**Role:** TECHNICAL DIRECTOR

---

## WEEK 1
**Dates:** October 27-28
**Hours this week:** 3 hrs

### WHAT I DID
- I created a repository in Github and added my groupmates as collaborators. 
- I commit the Activity J wherein it will serve as our based line in doing our project.
- I push the tasks that I need to accomplish as a Technical Director.

### WHAT I FINISHED
- I finished the scene data format specification.
- I created an initial scene loader file and edited the main.cpp.

### WHAT I TRIED THAT DID NOT WORK
- I tried pushing the zip file of the activity J. However, after committing it, I encountered a problem and needed to put a git ignore file to successfully commit it. 

### WHAT I AM WAITING ON, AND FROM WHOM
- I am waiting for my other groupmates to make their first commit as we needed it for Gate 1.

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME
- **R4 (Gameplay Programmer) is blocked by me:** They need to make the lights move and write the collision logic. Because I have not yet written the C++ code to extract the patrol_path arrays or load the objects geometry from the JSON, they cannot fully test their gameplay code in the engine.
- **R3 (Technical Artist) is blocked by me:** They are creating textures and 3D models. Because I have not yet expanded the SceneLoader to read the materials and objects arrays, they cannot see if their art looks correct inside the game.

### FILES I CHANGED / CREATED THIS WEEK
- Window.cpp
- Main.cpp
- SceneLoader.cpp
- SceneLoader.h
- scene_data_format_specification.md

### NEXT WEEK
- I will expand the json file, and Scene Loader.

---

## WEEK 2
**Dates:** November 1 - 3
**Hours this week:** 2 hrs

### WHAT I DID 
- Expanded the C++ JSON parser to handle object geometry and materials. 
- Integrated nlohmann/json to read and parse level01.json so that R3 and R4 have the data structures they need for their tasks.

### WHAT I FINISHED
- Fully implemented the scene loader for level01.json.
- Verified console loading output to ensure materials and objects are successfully being read into memory.

### WHAT I TRIED THAT DID NOT WORK
- Initially, I tried parsing the JSON fields directly, but the nlohmann/json library threw exceptions and crashed the game when certain optional material or geometry fields were missing in the file. I had to go back and implement `.contains()` checks and assign fallback default values to prevent the parser from breaking.

### WHAT I AM WAITING ON, AND FROM WHOM
- Waiting on R4 to fully hook up the collision logic using the object geometry data I am passing them.
- Waiting on R3 (and R2) to apply the loaded materials/textures to the renderer.

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME 
- R4 and R3 rely entirely on the data structures my parser generates. If my loader misses a specific object ID or fails to parse a transform correctly, they cannot test collisions or visuals for that object.

### FILES I CHANGED THIS WEEK 
- SceneLoader.cpp 
- SceneLoader.h 
- Json.cpp 
- level01.json

### NEXT WEEK
- I will try to expand the parsing for lights, automated patrol paths, or trigger zones. I will also try to add more data in level1.json

---

## WEEK 3
**Dates:** November 4 - 6
**Hours this week:** 3 hrs

### WHAT I DID 
- Managed the full code integration for Gate 4, pulling everyone's branches into a single unified master build.
- Logged severe merge conflicts (anything taking 10+ minutes to resolve) into the `docs/FILE_OWNERSHIP.md` document to clarify structural boundaries.
- Filled out and signed my portion of the `docs/TEXTURE_UNIT_LEDGER.md` alongside the rest of the team to ensure skybox and shadow texture units do not get overwritten.

### WHAT I FINISHED 
- Successfully produced a compiling, integrated build containing R4's new HUD alarm meter, R2's shader updates, and my earlier JSON parser logic. 
- Finalized T4 (File Ownership) and T2 (Texture Unit Ledger) documentation for this sprint.

### WHAT I TRIED THAT DID NOT WORK 
- Attempted to rely on auto-merge for the core initialization loops (like Game.cpp and Scene.cpp), but conflicting instantiation orders broke the build. I had to revert the auto-merge, manually lock down the include order, and resolve the files line-by-line while coordinating with R2 and R4.

### WHAT I AM WAITING ON, AND FROM WHOM 
- Waiting on R5 to complete the first full pass of the Test Matrix (T11) on this new master build and begin logging visual bugs in the Bug Log (T13).

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME 
- If the master build crashes or drops frames due to a botched merge on my end, R5 cannot execute the Test Matrix, and R4 cannot verify if the orthographic projection for the HUD is scaling correctly.

### FILES I CHANGED THIS WEEK 
- FILE_OWNERSHIP.md 
- docs/TEXTURE_UNIT_LEDGER.md 
- Game.cpp (merge conflict resolution) 
- Scene.cpp (merge conflict resolution)

### NEXT WEEK
- I will fix bugs in the ScreenLoader file or Json.cpp based on R5’s upcoming bug reports. 

---

## WEEK 4
**Dates:** November 7 - 9
**Hours this week:** 3 hrs

### WHAT I DID 
- Assisted R2 and R4 in debugging the CPU/GPU lit-test connections for the HUD alarm meter.

### WHAT I FINISHED 
- Verified that R2's moving bulbs properly and updated their shadow maps each frame.

### WHAT I TRIED THAT DID NOT WORK
- I didn’t do anything that would not work because I only assisted R2 and R4 in the alarm meter.

### WHAT I AM WAITING ON, AND FROM WHOM 
- Waiting on R2 and R4 to finish the Required Proof and fill out the Prediction Sheet (T14). 

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME 
- Nothing

### FILES I CHANGED THIS WEEK (with the permission of R2)
- Light.cpp
- Light.h 
- Scene.cpp 
- PointLight.cpp

### NEXT WEEK
- I will try to perform the clean clone test. 

---

## WEEK 5
**Dates:** November 10 - 11
**Hours this week:** 2 hrs

### WHAT I DID 
- Performed Clean Clone Test 2 (T5) by cloning the repository onto a completely fresh machine to verify portability. 
- Scanned the entire codebase and project settings for absolute file paths (like `C:/Users/Gianna/...`).

### WHAT I FINISHED 
- Successfully built and ran the project on the fresh machine without any missing dependencies or asset loading failures. 
- Submitted my final section for the Technical Report to R5.

### WHAT I TRIED THAT DID NOT WORK 
- During the first attempt at the Clean Clone Test, the game crashed immediately upon startup. I realised that I haven’t done the configuration and installing first. 

### WHAT I AM WAITING ON, AND FROM WHOM 
- Waiting on R5 to compile everyone's sections (S1-S5) into the final 2500-4000 word Technical Report and submit the final build.

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME 
- Nothing. 

### FILES I CHANGED THIS WEEK 
- SceneLoader.cpp (fixing hardcoded absolute paths) 
- SkyboxLab.vcxproj (cleaning up local include directories)

### NEXT WEEK 
- None.