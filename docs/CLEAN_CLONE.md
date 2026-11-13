# T5 - Clean Clone Record

*One per group. Required at Gate 2 and again before submission.*

## ATTEMPT 1, GATE 2

**Date:** 2026-11-06
**Done by:** Gianna Trisha Ainza
**Machine, and has it ever built this project before:** Dell Latitude 5420 (Never built this project before)
**Clock time from clone to a running window:** 15 minutes

**What went wrong the first time:** 
The vcpkg baseline configuration failed to install glew, glfw3, and glm dependencies because the MSBuild manifest mode was not properly reading `vcpkg.json` from the ActivityJ_Skybox project path.

**What we changed in the repo so it will not happen again:** 
We ran `vcpkg x-update-baseline --add-initial-baseline` to lock the dependency versions and updated the `SkyboxLab.vcxproj` configuration to explicitly enable vcpkg manifest mode.

---

## ATTEMPT 2, BEFORE SUBMISSION

**Date:** 2026-11-11
**Done by:** Gianna Trisha Ainza
**Machine:** ASUS PRIME A520M-K desktop (Never built this project before)
**Clock time from clone to a running window:** 8 minutes

**What went wrong (and what was fixed):** 
The game crashed on startup because a texture loading path in `SceneLoader.cpp` was accidentally hardcoded to a local `C:/Users/Gianna/...` absolute path during Sprint 4 debugging. I stripped the absolute path out, updated it to a relative `./Textures/...` path, pushed the fix, and re-cloned to verify it worked.