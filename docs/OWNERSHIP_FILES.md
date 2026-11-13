# T4 - File ownership (proposal - the group must confirm and edit)

| file(s) | owner | may be edited by |
|---|---|---|
| `main.cpp`, `SkyboxLab.sln/.vcxproj`, `vcpkg.json`, `Window.cpp/.h`, `CommonValues.h`, `Json.*`, `Scene.*`, `SceneLoader.*`, `level01.json` (structure) | R1 | R1 only (R4 sends input changes for `Window::handleKeys` to R1) |
| `Shaders/*`, `Shader.*`, `ShadowMap.*`, `OmniShadowMap.*`, `Light.*`, `DirectionalLight.*`, `PointLight.*`, `SpotLight.*` | R2 | R2 only |
| `Textures/`, `tools/make_textures.py`, `Material.*`, `Texture.*`, `Mesh.*`, `MeshFactory.*`, `Skybox.*`, `Model.*`, materials in `level01.json` | R3 | R3 only |
| `Game.*`, `LitTest.*`, `Hud.*`, `Camera.*`, gameplay numbers in `level01.json` | R4 | R4 only |
| `FrameTimeLogger.h` + `Timing.h/.cpp` (the timing harness), `tests/`, test matrix, bug log, frame-time table, report | R5 | R5 only |

Note: the timing harness now lives in R5's own files (`FrameTimeLogger.h` for the CPU frame times, `Timing.h/.cpp` for the GPU timer). `main.cpp` still contains the render loop, the shadow passes, the GPU probe and the P3 study, which belong to R2 and R5 in spirit; decide at Gate 1 whether to split those out too.

## Merge conflicts that cost more than ten minutes
| date | files | who | what we changed afterwards |
|---|---|---|---|
| | | | |
