#pragma once
// ---------------------------------------------------------------------------
// LitTest.h / LitTest.cpp - THE CPU ANSWER TO "IS THE DROID LIT?"      (R4 + R2)
//
// For every gameplay light i (drones first, then scanners), with d = |p - light_i|:
//
//   visible_i = (d <= farPlane_i) AND (cone_i > 0)
//               AND (segment p -> light_i is not blocked by any shadow-casting box)
//   cone_i    = 1 for a drone (omnidirectional point light)
//             = 1 - (1 - c) / (1 - edgeCos)   if c = dot(normalize(p - emitter), aimDir) > edgeCos, else 0
//               for a scanner  (the SAME cone and fade as the Activity J spot light in shader.frag)
//   att_i     = exponent*d*d + linear*d + constant            (same as shader.frag)
//   illum_i   = visible_i * cone_i * diffuseIntensity_i * power_i / att_i
//   illum     = sum_i illum_i
//   scanned   = illum_i >= litThreshold   (per scanner)
//
// There is deliberately NO normal (N.L) term: the droid is a capsule, not a flat face, and the
// gameplay question is "can the light see me", not "how much does this floor point face it".
// There is no colour term either: scanners change colour (yellow -> red) and that must never
// change whether you are seen.  shader.frag computes the identical 'gIllum' in debugMode 2, so the
// two answers can be compared pixel by pixel (key G, key F9).
// ---------------------------------------------------------------------------
#include "Scene.h"

struct LitResult
{
    float illum = 0.0f;          // summed illumination
    unsigned int visMask = 0;    // bit i set = light i can see the point
    float per[MAX_GAME_LIGHTS] = {};   // contribution of each light on its own (scanner detection uses this)
};

float BulbAttenuation(const Bulb& b, float d);
float ConeFactor(const Bulb& b, const glm::vec3& p);
bool  SegmentBlocked(const Scene& sc, const glm::vec3& a, const glm::vec3& b);
LitResult ComputeIllumination(const Scene& sc, const glm::vec3& p, bool lineOfSight = true);
