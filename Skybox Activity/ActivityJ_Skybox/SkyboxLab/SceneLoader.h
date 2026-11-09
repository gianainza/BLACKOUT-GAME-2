#pragma once
// ---------------------------------------------------------------------------
// SceneLoader.h / SceneLoader.cpp - reads level01.json into a Scene          owner: R1
//
// Format: scene_data_format_specification.md (repo root).
//
//   * a malformed file never crashes the program: every problem becomes a message
//         level01.json:57: ERROR lights.spot_lights[1] 'sweep_b': 'edge' must be a number between 3 and 80
//     stored in Scene::messages (and printed), and the offending entry is skipped
//   * a missing texture is reported and replaced by a visible magenta checker (see Texture.cpp)
//   * Scene::playable is false only when the level cannot be played at all
// ---------------------------------------------------------------------------
#include "Scene.h"
#include <string>

class SceneLoader
{
public:
    // Fills 'sc'. Returns sc.playable.
    static bool Load(const std::string& filepath, Scene& sc);
};
