#pragma once
// ---------------------------------------------------------------------------
// MeshFactory.h / MeshFactory.cpp - the shapes, built from numbers          owner: R3
//
// Model loading is switched off (USE_MODELS 0), so every shape is generated here, in the vertex
// layout Activity J's Mesh class expects:   x y z | u v | nx ny nz     (8 floats per vertex)
//
// Vertex counts (B2 / R3 D5): a lit, textured BOX needs 24 vertices, not 8 - each corner is shared by
// three faces that point in three different directions, so it needs three different normals (and, with
// tiling, different texture coordinates). Splitting a corner for a different normal is correct; what B2
// forbids is sending the same corner several times with IDENTICAL attributes. The skybox, which ignores
// normals, really does use 8.
//    box 24 v / 36 i      sphere (s+1)(t+1) v      cylinder 4(s+1)+2 v      cone 2(s+1) v
// ---------------------------------------------------------------------------
#include "Mesh.h"

Mesh* MakeBoxMesh(float sx, float sy, float sz, float tileSize);     // centred on the origin; UVs repeat every tileSize metres
Mesh* MakeSphereMesh(float radius, int slices, int stacks);
Mesh* MakeCylinderMesh(float radius, float height, int slices);      // base at y = 0, top at y = height
Mesh* MakeConeMesh(float baseRadius, float height, int slices);      // apex at the origin, opens toward -Y (scanner beams)
