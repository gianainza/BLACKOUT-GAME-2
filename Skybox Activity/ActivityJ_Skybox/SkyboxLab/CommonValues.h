#ifndef COMMONVALS
#define COMMONVALS

#include "stb_image.h"

// ---------------------------------------------------------------------------
// OBSOLETE  (built on the Activity J project)         owner: R1
//
// MAX_POINT_LIGHTS / MAX_SPOT_LIGHTS must match shader.frag.
//   point lights  = patrolling drone lamps
//   spot lights   = spot 0 is the droid's own lamp (inherited from Activity J: re-aimed from
//                   the camera every frame); spots 1.. are the ceiling scanners.
// ---------------------------------------------------------------------------
const int MAX_POINT_LIGHTS = 3;
const int MAX_SPOT_LIGHTS  = 4;

// Gameplay lights = every drone + every scanner (NOT the droid's lamp). The CPU lit-test, the
// debug mask and the shader all number them the same way: drones first, then scanners.
const int MAX_GAME_LIGHTS  = (MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS - 1);

#define USE_MODELS 0

const float PI_F            = 3.14159265358979f;
const float PLAYER_RADIUS   = 0.35f;     // droid collision circle (metres)
const float PLAYER_SPEED    = 4.5f;      // metres per second (multiplied by deltaTime)
const float SAMPLE_HEIGHT   = 0.01f;     // the lit test samples the floor at the droid's feet

// texture units (see README / texture unit ledger)
const int UNIT_SKY          = 0;         // sky cube map (skybox program only)
const int UNIT_TEXTURE      = 1;         // theTexture
const int UNIT_DIR_SHADOW   = 2;         // directionalShadowMap
const int UNIT_OMNI_FIRST   = 3;         // omniShadowMaps[0..5]  -> units 3..(3 + lights - 1)
const int UNIT_HUD          = 15;        // HUD strip (hud program only)

#endif
