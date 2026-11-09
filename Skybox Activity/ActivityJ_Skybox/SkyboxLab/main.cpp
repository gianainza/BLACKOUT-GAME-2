// ---------------------------------------------------------------------------
// BLACKOUT- industrial cyberpunk stealth, built on the Activity J (skybox) project.        owner: R1
//
//  The frame, in order (S1 of the report):
//    1. DirectionalShadowMapPass     directional_shadow_map.*   -> 2048^2 depth texture (cached: nothing moves)
//    2. OmniShadowMapPass x N        omni_shadow_map.* (+geom)  -> one cube map per light, ONE draw call per
//                                    light, the geometry shader fans each triangle out to all six faces.
//                                    Static lights are skipped while their map is still valid (E1 cache).
//    3. RenderPass                   skybox.*  then shader.*    -> skybox first with depth writes off, then the
//                                    world with every light and shadow map; then additive effects (scanner
//                                    beams, lock-on ring, strike column) with depth writes off
//    4. DrawHud                      hud.*                      -> flat, glm::ortho, no depth
//
//  The game (Game.cpp, LitTest.cpp) asks the same question the shader answers - "is the droid lit?" -
//  once per frame on the CPU, from the same Scene data the meshes and shadow casters come from.
// ---------------------------------------------------------------------------
#define STB_IMAGE_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "CommonValues.h"

#include "Window.h"
#include "Mesh.h"
#include "MeshFactory.h"
#include "Shader.h"
#include "Camera.h"
#include "Texture.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include "SpotLight.h"
#include "Material.h"
#include "Skybox.h"
#include "Scene.h"
#include "SceneLoader.h"
#include "LitTest.h"
#include "Game.h"
#include "Hud.h"
#include "Timing.h"
#include "FrameTimeLogger.h"

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------
struct Renderer                      // debug / measurement state (R2, R5)
{
	Hud   hud;
	bool  liveProbe = false;         // E2: show the GPU pixel value next to the CPU number
	float liveGpuIllum = 0.0f;
	float shadowGpuMs = 0.0f;
	bool  cacheEnabled = true;       // E1: only re-render a cube map when its light moved
	int   mapsRebuilt = 0;           // cube maps (+ directional) rendered this frame
	int   breakMode = 0;             // "break it on purpose": 1 GPU shadows off, 2 CPU ignores walls, 3 huge bias
	int   debugView = 0;             // 1 = paint the game's opinion (scanned / hidden) on the world
	float omniBias = 0.05f;          // world units
	bool  vsync = true;
	GLuint probeTex = 0, probeFbo = 0, probeDepth = 0;
};

static Renderer R;
static GpuTimer   shadowTimer;      // GPU time of the shadow passes (Timing.cpp, R5)
static FrameTimeLogger frameLogger("frame_times");   // CPU frame times, key T (FrameTimeLogger.h, R5)
static Scene scene;
static Game game;
static Window* mainWindow = nullptr;
static Camera camera;
static int g_fbW = 1366, g_fbH = 768;

static Shader mainShader;
static Shader directionalShadowShader;
static Shader omniShadowShader;

static DirectionalLight mainLight;
static PointLight pointLights[MAX_POINT_LIGHTS];
static SpotLight spotLights[MAX_SPOT_LIGHTS];
static unsigned int pointLightCount = 0;
static unsigned int spotLightCount = 0;
static Skybox skybox;

// world data on the GPU, one entry per scene entry
static std::vector<Texture*>  matTexture;
static std::vector<Material*> matMaterial;
static std::vector<Mesh*>     objMesh;
static std::vector<glm::mat4> objModel;
static std::vector<Mesh*>     conveyorMesh;
static Mesh *sphereMesh = nullptr, *unitCylMesh = nullptr, *unitBoxMesh = nullptr, *coneMesh = nullptr, *headMesh = nullptr, *padMesh = nullptr, *beaconMesh = nullptr;
static Texture* whiteTex = nullptr;
static Texture* padTex = nullptr;
static Texture* scannerTex = nullptr;      // floor marker: rim, rings, crosshair ticks, radar sweep
static Texture* beamTex = nullptr;         // beam: bright at the emitter, fading toward the floor, with streaks
static Mesh*    decalMesh = nullptr;       // 2 x 2 flat quad, UVs 0..1, scaled to the exposure ellipse
static int      drawDecalLight = -1;       // >= 0 while a scanner marker is drawn (see shader.frag)

// the exit pathway (level file: game_logic.exit_zone.pathway): floor strip with scrolling chevrons, two glowing
// rails, a glowing doorway set into the wall, and a soft additive glow in the doorway
struct ExitGfx
{
	bool  enabled = false;
	Mesh *floorMesh = nullptr, *railMesh = nullptr, *doorMesh = nullptr, *postMesh = nullptr, *topMesh = nullptr, *glowMesh = nullptr;
	glm::mat4 floorModel{ 1.0f }, railModel[2], doorModel{ 1.0f }, postModel[2], topModel{ 1.0f }, glowModel{ 1.0f };
	float length = 1.0f;
	glm::mat4 endBase{ 1.0f };          // frame at the wall end of the path (local +x points into the wall)
};
static ExitGfx exitGfx;

static float  drawAlpha = 1.0f;          // alphaMul for the next DrawMesh
static glm::vec2 drawUV(0.0f);           // uvOffset for the next DrawMesh (conveyor scroll)

// E1 cache: the position each cube map was last rendered at
static bool      dirValid = false;
static bool      omniValid[MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS] = {};
static glm::vec3 omniLastPos[MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS];

static const char* vShader = "Shaders/shader.vert";
static const char* fShader = "Shaders/shader.frag";

// rotation that takes the cone/beam axis (0,-1,0) onto the unit vector 'dir'
static glm::mat4 AlignDown(const glm::vec3& dir)
{
	const glm::vec3 from(0.0f, -1.0f, 0.0f);
	float c = glm::dot(from, dir);
	if (c > 0.9999f)  return glm::mat4(1.0f);
	if (c < -0.9999f) return glm::rotate(glm::mat4(1.0f), PI_F, glm::vec3(1, 0, 0));
	return glm::rotate(glm::mat4(1.0f), std::acos(c), glm::normalize(glm::cross(from, dir)));
}

static std::string Fmt(const char* f, double v)
{
	char b[64];
	std::snprintf(b, sizeof b, f, v);
	return b;
}

// ---------------------------------------------------------------------------
// building the world from the Scene (R3 / R1)
// ---------------------------------------------------------------------------
static void CreateObjects()
{
	// materials: texture + specular + shininess + tint + emissive, all from the level file
	for (const auto& m : scene.materials)
	{
		Texture* t = new Texture(m.texturePath.c_str());
		t->LoadTextureA();                       // a missing file becomes a magenta checker, never silent black
		matTexture.push_back(t);
		matMaterial.push_back(new Material(m.specular, m.shininess, m.colour, m.emissive));
	}

	// every scene object gets its own mesh (UVs repeat every material.tile metres)
	for (const auto& o : scene.objects)
	{
		float tile = scene.materials[o.material].tile;
		glm::mat4 model(1.0f);
		Mesh* mesh = nullptr;
		if (o.mesh == "primitive:cylinder")
		{
			mesh = MakeCylinderMesh(0.5f, 1.0f, 40);
			model = glm::translate(glm::mat4(1.0f), o.pos - glm::vec3(0.0f, o.scale.y * 0.5f, 0.0f));
			model = glm::scale(model, glm::vec3(o.scale.x, o.scale.y, o.scale.z));
		}
		else if (o.mesh == "primitive:sphere")
		{
			mesh = MakeSphereMesh(0.5f, 32, 16);
			model = glm::scale(glm::translate(glm::mat4(1.0f), o.pos), o.scale);
		}
		else
		{
			mesh = MakeBoxMesh(o.scale.x, o.scale.y, o.scale.z, tile);
			model = glm::translate(glm::mat4(1.0f), o.pos);
		}
		if (o.rot.y != 0.0f) model = glm::rotate(model, glm::radians(o.rot.y), glm::vec3(0, 1, 0));
		objMesh.push_back(mesh);
		objModel.push_back(model);
	}

	for (const auto& c : scene.conveyors)
		conveyorMesh.push_back(MakeBoxMesh(c.size.x, 0.10f, c.size.y, scene.materials[c.material].tile));

	whiteTex = new Texture("Textures/plain.png");
	whiteTex->LoadTextureA();
	padTex = new Texture("Textures/exitpad.png");
	padTex->LoadTextureA();
	scannerTex = new Texture("Textures/scanner.png");
	scannerTex->LoadTextureA();
	beamTex = new Texture("Textures/beam.png");
	beamTex->LoadTextureA();
	decalMesh = MakeBoxMesh(2.0f, 0.02f, 2.0f, 2.0f);       // one tile = the whole quad, so the texture is not repeated

	// ---- the glowing exit pathway, built from the numbers in the level file ----
	if (scene.exitPath.enabled)
	{
		const ExitPath& ep = scene.exitPath;
		glm::vec2 d2 = ep.to - ep.from;
		float len = glm::length(d2);
		d2 = d2 / len;
		float yaw = std::atan2(-d2.y, d2.x);                         // rotation about Y that turns local +x into the path direction
		glm::vec2 mid2 = (ep.from + ep.to) * 0.5f;
		glm::mat4 base = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(mid2.x, 0.0f, mid2.y)), yaw, glm::vec3(0, 1, 0));
		const float W = ep.width, H = ep.doorHeight;
		float tile = scene.materials[ep.material].tile;

		exitGfx.enabled = true;
		exitGfx.length = len;
		exitGfx.floorMesh = MakeBoxMesh(len, 0.05f, W, tile);
		exitGfx.floorModel = glm::translate(base, glm::vec3(0.0f, 0.085f, 0.0f));          // just above the conveyor belts (top at 0.10)
		exitGfx.railMesh = MakeBoxMesh(len, 0.22f, 0.10f, 1.0f);
		for (int s = 0; s < 2; ++s)
			exitGfx.railModel[s] = glm::translate(base, glm::vec3(0.0f, 0.17f, (s ? 1.0f : -1.0f) * (W * 0.5f - 0.05f)));

		// the doorway sits at the wall end of the path: a bright panel, a frame, and a glow in front of it
		glm::mat4 endBase = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(ep.to.x, 0.0f, ep.to.y)), yaw, glm::vec3(0, 1, 0));
		exitGfx.endBase = endBase;
		exitGfx.doorMesh = MakeBoxMesh(0.10f, H, W, 1.0f);
		exitGfx.doorModel = glm::translate(endBase, glm::vec3(-0.06f, H * 0.5f, 0.0f));
		exitGfx.postMesh = MakeBoxMesh(0.16f, H + 0.2f, 0.20f, 1.0f);
		for (int s = 0; s < 2; ++s)
			exitGfx.postModel[s] = glm::translate(endBase, glm::vec3(-0.08f, (H + 0.2f) * 0.5f, (s ? 1.0f : -1.0f) * (W * 0.5f + 0.1f)));
		exitGfx.topMesh = MakeBoxMesh(0.16f, 0.20f, W + 0.4f, 1.0f);
		exitGfx.topModel = glm::translate(endBase, glm::vec3(-0.08f, H + 0.1f, 0.0f));
		exitGfx.glowMesh = MakeBoxMesh(2.0f, H, W, 1.0f);
		exitGfx.glowModel = glm::translate(endBase, glm::vec3(-1.05f, H * 0.5f, 0.0f));
	}

	sphereMesh   = MakeSphereMesh(0.25f, 24, 12);
	unitCylMesh  = MakeCylinderMesh(1.0f, 1.0f, 40);
	coneMesh     = MakeConeMesh(1.0f, 1.0f, 40);
	unitBoxMesh  = MakeBoxMesh(1.0f, 1.0f, 1.0f, 1.0f);       // scaled per droid part
	headMesh     = MakeSphereMesh(0.22f, 24, 12);
	padMesh      = MakeCylinderMesh(scene.exitRadius, 0.06f, 48);
	beaconMesh   = MakeCylinderMesh(0.12f, 6.0f, 20);
}

static void CreateShaders()
{
	mainShader.CreateFromFiles(vShader, fShader);
	directionalShadowShader.CreateFromFiles("Shaders/directional_shadow_map.vert", "Shaders/directional_shadow_map.frag");
	omniShadowShader.CreateFromFiles("Shaders/omni_shadow_map.vert", "Shaders/omni_shadow_map.geom", "Shaders/omni_shadow_map.frag");
}

static void CreateLights()
{
	mainLight = DirectionalLight(scene.dirRes, scene.dirRes,
		scene.dirColour.x, scene.dirColour.y, scene.dirColour.z,
		scene.dirAmbient, scene.dirDiffuse,
		scene.dirDirection.x, scene.dirDirection.y, scene.dirDirection.z);

	pointLightCount = (unsigned int)scene.droneCount;
	for (unsigned int i = 0; i < pointLightCount; ++i)
	{
		const Bulb& b = scene.bulbs[i];
		pointLights[i] = PointLight(scene.omniRes, scene.omniRes, 0.1f, b.farPlane,
			b.baseColour.x, b.baseColour.y, b.baseColour.z, b.ambient, b.diffuse,
			b.position.x, b.position.y, b.position.z, b.attC, b.attL, b.attE);
	}

	// spot 0: the droid's own lamp (Activity J's flashlight, re-aimed every frame)
	spotLights[0] = SpotLight(scene.omniRes, scene.omniRes, 0.1f, 40.0f,
		scene.flashColour.x, scene.flashColour.y, scene.flashColour.z, 0.0f, scene.flashDiffuse,
		0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f,
		scene.flashAttC, scene.flashAttL, scene.flashAttE, scene.flashEdgeDeg);
	if (!scene.flashOn) spotLights[0].Toggle();
	spotLightCount = 1;

	// spots 1..: the ceiling scanners
	for (size_t i = (size_t)scene.droneCount; i < scene.bulbs.size() && spotLightCount < (unsigned)MAX_SPOT_LIGHTS; ++i)
	{
		const Bulb& b = scene.bulbs[i];
		glm::vec3 d = b.Dir();
		spotLights[spotLightCount] = SpotLight(scene.omniRes, scene.omniRes, 0.1f, b.farPlane,
			b.baseColour.x, b.baseColour.y, b.baseColour.z, b.ambient, b.diffuse,
			b.position.x, b.position.y, b.position.z, d.x, d.y, d.z,
			b.attC, b.attL, b.attE, b.edgeDeg);
		++spotLightCount;
	}
}

// the game owns where the lights are and what colour they glow; copy that into the Activity J light objects
static void SyncLightsFromGame()
{
	for (unsigned int i = 0; i < pointLightCount; ++i)
	{
		pointLights[i].SetPosition(scene.bulbs[i].position);
		pointLights[i].SetColour(scene.bulbs[i].colour);
		pointLights[i].SetPower(scene.bulbs[i].power);
	}

	for (size_t i = (size_t)scene.droneCount; i < scene.bulbs.size(); ++i)
	{
		unsigned int s = 1 + (unsigned int)(i - scene.droneCount);
		if (s >= spotLightCount) break;
		const Bulb& b = scene.bulbs[i];
		spotLights[s].SetPosition(b.position);
		spotLights[s].SetAim(b.Dir());
		spotLights[s].SetColour(b.colour);
		spotLights[s].SetPower(b.power);
	}

	// the droid's lamp rides on the droid and looks where it faces, tilted a little toward the floor
	if (spotLights[0].IsOn() != game.flashOn) spotLights[0].Toggle();
	spotLights[0].SetPower(game.lampPower);
	glm::vec3 p = game.playerPos + glm::vec3(game.facing.x * 0.30f, 0.65f, game.facing.y * 0.30f);
	glm::vec3 d = glm::normalize(glm::vec3(game.facing.x, -0.28f, game.facing.y));
	spotLights[0].SetFlash(p, d);
}

// ---------------------------------------------------------------------------
// drawing
// ---------------------------------------------------------------------------
static void DrawMesh(Mesh* mesh, const glm::mat4& model, Texture* tex, float spec, float shine, const glm::vec3& colour, float emissive)
{
	glUniformMatrix4fv(mainShader.GetModelLocation(), 1, GL_FALSE, glm::value_ptr(model));
	tex->UseTexture();
	glUniform1f(mainShader.GetSpecularIntensityLocation(), spec);
	glUniform1f(mainShader.GetShininessLocation(), shine);
	mainShader.SetVec3("materialColour", colour);
	mainShader.SetFloat("emissive", emissive);
	mainShader.SetFloat("alphaMul", drawAlpha);
	mainShader.SetVec2("uvOffset", drawUV);
	mainShader.SetInt("decalLight", drawDecalLight);
	mesh->RenderMesh();
}

// everything that casts a shadow: the SAME list the CPU lit-test ray-tests (SegmentBlocked)
static void DrawCasters(GLuint modelLocation)
{
	for (size_t i = 0; i < scene.objects.size(); ++i)
	{
		if (!scene.objects[i].castShadow || scene.objects[i].destroyed) continue;
		glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(objModel[i]));
		objMesh[i]->RenderMesh();
	}
}

static void DrawWorld(bool extras)
{
	for (size_t i = 0; i < scene.objects.size(); ++i)
	{
		const SceneObject& o = scene.objects[i];
		if (o.destroyed) continue;                              // destroyed cover is simply gone (the scorch mark remains)
		const MaterialDef& m = scene.materials[o.material];
		DrawMesh(objMesh[i], objModel[i], matTexture[o.material], m.specular, m.shininess, m.colour, m.emissive);
	}
	if (!extras) return;                        // the GPU probe only needs the floor and the blockers

	// conveyor belts: the texture scrolls the way they push the droid
	const double tnow = glfwGetTime();
	for (size_t i = 0; i < scene.conveyors.size(); ++i)
	{
		const Conveyor& c = scene.conveyors[i];
		const MaterialDef& m = scene.materials[c.material];
		float k = (float)std::fmod(c.speed * tnow / m.tile, 1.0);
		drawUV = glm::vec2(-c.dir.x, c.dir.y) * k;
		DrawMesh(conveyorMesh[i], glm::translate(glm::mat4(1.0f), glm::vec3(c.centre.x, 0.05f, c.centre.y)), matTexture[c.material],
			m.specular, m.shininess, m.colour, m.emissive);
	}
	drawUV = glm::vec2(0.0f);

	// scorch marks: where strikes landed
	for (const auto& sm : game.scorchMarks)
		DrawMesh(unitCylMesh, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(sm.x, 0.035f, sm.y)), glm::vec3(scene.strikeRadius * 0.9f, 0.01f, scene.strikeRadius * 0.9f)),
			whiteTex, 0.0f, 1.0f, glm::vec3(0.025f, 0.02f, 0.015f), 0.0f);

	// oil puddles: thin glossy dark discs just above the floor (not solid, no shadow, so not in the casters list)
	static int oilMat = -2;
	if (oilMat == -2)
	{
		oilMat = scene.FindMaterial("mat_oil");
		if (oilMat < 0) { std::printf("[warning] material 'mat_oil' is missing from the level file - puddles use material 0\n"); oilMat = 0; }
	}
	for (const auto& p : scene.oilPuddles)
	{
		const MaterialDef& om = scene.materials[oilMat];
		DrawMesh(unitCylMesh, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(p.pos.x, 0.02f, p.pos.y)), glm::vec3(p.radius, 0.01f, p.radius)),
			matTexture[oilMat], om.specular, om.shininess, om.colour, om.emissive);
	}

	if (exitGfx.enabled)
	{
		// the way out: a green pathway attached to the wall, chevrons sliding toward the doorway
		const ExitPath& xp = scene.exitPath;
		const MaterialDef& fm = scene.materials[xp.material];
		const MaterialDef& gm = scene.materials[xp.glowMaterial];
		const float pulse = 0.75f + 0.25f * std::sin((float)tnow * 3.0f);
		drawUV = glm::vec2(-(float)std::fmod(1.2 * tnow / fm.tile, 1.0), 0.0f);
		DrawMesh(exitGfx.floorMesh, exitGfx.floorModel, matTexture[xp.material], fm.specular, fm.shininess, fm.colour, fm.emissive * pulse);
		drawUV = glm::vec2(0.0f);
		for (int s = 0; s < 2; ++s)
			DrawMesh(exitGfx.railMesh, exitGfx.railModel[s], matTexture[xp.glowMaterial], gm.specular, gm.shininess, gm.colour, gm.emissive * pulse);
		// the door panel and a lock light show the lock colour STEADILY while locked (so it is not mistaken for the pulsing red
		// strike warning); once unlocked they turn the pathway green
		const bool locked = game.doorLocked && game.lockColour >= 0;
		const glm::vec3 doorCol = locked ? scene.codePuzzle.palette[game.lockColour].colour : gm.colour;
		DrawMesh(exitGfx.doorMesh, exitGfx.doorModel, matTexture[xp.glowMaterial], gm.specular, gm.shininess, doorCol, locked ? gm.emissive * 1.2f : gm.emissive * pulse * 1.2f);
		{
			const float H2 = scene.exitPath.doorHeight;
			glm::mat4 rimM = glm::scale(glm::translate(exitGfx.endBase, glm::vec3(-0.08f, H2 + 0.52f, 0.0f)), glm::vec3(0.10f, 0.46f, 0.46f));
			glm::mat4 lightM = glm::scale(glm::translate(exitGfx.endBase, glm::vec3(-0.16f, H2 + 0.52f, 0.0f)), glm::vec3(0.10f, 0.34f, 0.34f));
			DrawMesh(unitBoxMesh, rimM, whiteTex, 0.2f, 16.0f, glm::vec3(0.95f, 0.95f, 1.0f), 1.3f);
			DrawMesh(unitBoxMesh, lightM, whiteTex, 0.2f, 16.0f, locked ? doorCol : glm::vec3(0.2f, 1.0f, 0.35f), 3.0f);
		}
		for (int s = 0; s < 2; ++s)
			DrawMesh(exitGfx.postMesh, exitGfx.postModel[s], matTexture[xp.glowMaterial], gm.specular, gm.shininess, gm.colour, gm.emissive * 1.5f);
		DrawMesh(exitGfx.topMesh, exitGfx.topModel, matTexture[xp.glowMaterial], gm.specular, gm.shininess, gm.colour, gm.emissive * 1.5f);
	}
	else
	{
		// fallback when the level has no pathway: a green hatch on the floor with a beacon
		glm::vec3 ep(scene.exitPos.x, 0.0f, scene.exitPos.y);
		DrawMesh(padMesh, glm::translate(glm::mat4(1.0f), ep + glm::vec3(0, 0.10f, 0)), padTex, 0.2f, 8.0f, glm::vec3(1.0f), 0.9f);
		DrawMesh(beaconMesh, glm::translate(glm::mat4(1.0f), ep), whiteTex, 0.0f, 1.0f, glm::vec3(0.3f, 1.0f, 0.4f), 1.5f);
	}

	// terminals. ROUTE terminals: a status lamp (cyan = ready, amber = being hacked, grey = rebooting).
	// CODE terminals: a lit colour screen on the top and on all four sides, with a bright rim so even a near-black
	// decoy can be seen against the dark plant. A terminal that has already given its digit goes dim and grey.
	for (const auto& t : scene.terminals)
	{
		if (t.kind == Terminal::Kind::Route)
		{
			glm::vec3 col = (t.cooldown > 0.0f) ? glm::vec3(0.35f, 0.35f, 0.4f)
				: (t.progress > 0.01f) ? glm::vec3(1.0f, 0.7f, 0.1f) : glm::vec3(0.1f, 1.0f, 0.85f);
			DrawMesh(sphereMesh, glm::translate(glm::mat4(1.0f), glm::vec3(t.pos.x, 1.5f, t.pos.y)), whiteTex, 0.0f, 1.0f, col, 2.0f);
			continue;
		}
		const glm::vec3 c = (t.colourIdx >= 0) ? scene.codePuzzle.palette[t.colourIdx].colour : glm::vec3(1.0f);
		const bool usedUp = t.used, working = t.progress > 0.01f;
		const float k = usedUp ? 0.22f : 1.0f;
		const glm::vec3 rim = usedUp ? glm::vec3(0.35f, 0.35f, 0.38f) : (working ? glm::vec3(1.0f, 0.75f, 0.15f) : glm::vec3(0.95f, 0.95f, 1.0f));
		const float rimEm = usedUp ? 0.35f : 1.5f;
		auto bx = [&](const glm::vec3& at, const glm::vec3& size, const glm::vec3& col, float em)
		{ DrawMesh(unitBoxMesh, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(t.pos.x, 0.0f, t.pos.y) + at), size), whiteTex, 0.2f, 16.0f, col, em); };
		bx(glm::vec3(0.0f, 1.205f, 0.0f), glm::vec3(0.98f, 0.05f, 0.98f), rim, rimEm);          // top: rim, then the colour cap
		bx(glm::vec3(0.0f, 1.235f, 0.0f), glm::vec3(0.80f, 0.06f, 0.80f), c * k, 2.4f);
		for (int sd = 0; sd < 4; ++sd)                                                       // four side screens
		{
			const float sx = (sd == 0) ? 1.0f : (sd == 1) ? -1.0f : 0.0f, sz = (sd == 2) ? 1.0f : (sd == 3) ? -1.0f : 0.0f;
			const bool alongX = (sd < 2);
			bx(glm::vec3(sx * 0.455f, 0.80f, sz * 0.455f), alongX ? glm::vec3(0.03f, 0.55f, 0.70f) : glm::vec3(0.70f, 0.55f, 0.03f), rim, rimEm);
			bx(glm::vec3(sx * 0.462f, 0.80f, sz * 0.462f), alongX ? glm::vec3(0.04f, 0.45f, 0.60f) : glm::vec3(0.60f, 0.45f, 0.04f), c * k, 2.4f);
		}
	}

	// the lights: scanner heads hang from a pylon; drones are bare glowing spheres
	for (const auto& b : scene.bulbs)
	{
		if (!b.scanner)
		{
			DrawMesh(sphereMesh, glm::translate(glm::mat4(1.0f), b.position), whiteTex, 0.0f, 1.0f, b.colour * ((b.power > 0.0f) ? 1.0f : 0.15f), 2.0f);
			continue;
		}
		float on = (b.power > 0.0f) ? 1.0f : 0.15f;
		DrawMesh(sphereMesh, glm::translate(glm::mat4(1.0f), b.position), whiteTex, 0.0f, 1.0f, b.colour * on, 2.0f);
		glm::mat4 pm = glm::scale(glm::translate(glm::mat4(1.0f), b.position), glm::vec3(0.07f, 14.0f - b.position.y, 0.07f));
		DrawMesh(unitCylMesh, pm, whiteTex, 0.2f, 8.0f, glm::vec3(0.08f), 0.0f);
	}

	// ---- the droid: a small, worn-out maintenance robot (all primitives; local +x = the way it faces) ----
	if (game.state != GameState::Title)
	{
		static glm::vec3 lastPos = game.playerPos;
		static float wheelSpin = 0.0f;
		glm::vec3 dp = game.playerPos - lastPos;
		lastPos = game.playerPos;
		float stepLen = glm::length(dp);
		bool moving = stepLen > 0.0005f && stepLen < 1.0f;       // a restart teleports the droid: do not spin the wheels for that
		if (stepLen < 1.0f) wheelSpin += stepLen / 0.12f;         // radians = distance rolled / wheel radius

		const float t = (float)tnow;
		const float bob = moving ? 0.010f * std::sin(t * 14.0f) : 0.004f * std::sin(t * 2.0f);
		const float lift = game.onBelt ? 0.10f : 0.0f;
		const float yaw = std::atan2(-game.facing.y, game.facing.x);     // turns local +x into the facing direction
		const glm::mat4 I(1.0f);
		const glm::mat4 B = glm::rotate(glm::translate(I, game.playerPos + glm::vec3(0.0f, lift, 0.0f)), yaw, glm::vec3(0, 1, 0));

		auto part = [&](Mesh* mesh, const glm::mat4& xf, const glm::vec3& col, float spec, float shin, float emit)
		{ DrawMesh(mesh, xf, whiteTex, spec, shin, col, emit); };
		auto box = [&](const glm::mat4& frame, const glm::vec3& at, const glm::vec3& size, const glm::vec3& col, float spec, float shin, float emit)
		{ part(unitBoxMesh, glm::scale(glm::translate(frame, at), size), col, spec, shin, emit); };

		const glm::vec3 steel(0.20f, 0.21f, 0.23f), grey(0.55f, 0.57f, 0.60f), dark(0.05f, 0.05f, 0.06f);
		const glm::vec3 paint(0.72f, 0.52f, 0.18f);               // faded safety-yellow paint, a lot of wear

		// chassis and two wheels (the hubs spin as the droid rolls)
		box(B, glm::vec3(0.0f, 0.19f + bob * 0.3f, 0.0f), glm::vec3(0.44f, 0.10f, 0.30f), steel, 0.7f, 48.0f, 0.0f);
		for (int sgn = -1; sgn <= 1; sgn += 2)
		{
			const float side = (float)sgn;
			glm::mat4 W = glm::translate(B, glm::vec3(0.0f, 0.12f, side * 0.23f));
			glm::mat4 tyre = glm::rotate(W, PI_F * 0.5f, glm::vec3(1, 0, 0));
			tyre = glm::translate(tyre, glm::vec3(0.0f, -0.055f, 0.0f));
			tyre = glm::scale(tyre, glm::vec3(0.12f, 0.11f, 0.12f));
			part(unitCylMesh, tyre, dark, 0.1f, 4.0f, 0.0f);
			glm::mat4 H = glm::rotate(glm::translate(W, glm::vec3(0.0f, 0.0f, side * 0.058f)), -wheelSpin, glm::vec3(0, 0, 1));
			part(unitBoxMesh, glm::scale(H, glm::vec3(0.17f, 0.035f, 0.012f)), grey, 0.8f, 64.0f, 0.0f);
			part(unitBoxMesh, glm::scale(glm::rotate(H, PI_F * 0.5f, glm::vec3(0, 0, 1)), glm::vec3(0.17f, 0.035f, 0.012f)), grey, 0.8f, 64.0f, 0.0f);
		}

		// torso with an amber status light that blinks
		box(B, glm::vec3(-0.02f, 0.40f + bob, 0.0f), glm::vec3(0.30f, 0.30f, 0.26f), paint, 0.5f, 24.0f, 0.0f);
		box(B, glm::vec3(0.135f, 0.44f + bob, 0.0f), glm::vec3(0.03f, 0.07f, 0.12f), glm::vec3(1.0f, 0.65f, 0.1f), 0.0f, 1.0f, (std::sin(t * 3.0f) > 0.5f) ? 2.4f : 0.5f);
		box(B, glm::vec3(0.135f, 0.35f + bob, 0.0f), glm::vec3(0.025f, 0.04f, 0.16f), dark, 0.5f, 32.0f, 0.0f);   // a vent grille

		// arms: they swing while the droid rolls; each ends in a small claw
		for (int sgn = -1; sgn <= 1; sgn += 2)
		{
			const float side = (float)sgn;
			float swing = moving ? 0.55f * std::sin(wheelSpin * 0.8f) * side : 0.0f;
			glm::mat4 A = glm::rotate(glm::translate(B, glm::vec3(-0.02f, 0.52f + bob, side * 0.19f)), swing, glm::vec3(0, 0, 1));
			part(unitBoxMesh, glm::scale(glm::translate(A, glm::vec3(0.0f, -0.12f, 0.0f)), glm::vec3(0.07f, 0.24f, 0.07f)), steel, 0.6f, 32.0f, 0.0f);
			part(unitBoxMesh, glm::scale(glm::translate(A, glm::vec3(0.0f, -0.27f, 0.0f)), glm::vec3(0.10f, 0.06f, 0.11f)), grey, 0.7f, 48.0f, 0.0f);
		}

		// neck + dome head; a defective droid: the head twitches now and then and the visor flickers
		part(unitCylMesh, glm::scale(glm::translate(B, glm::vec3(-0.02f, 0.55f + bob, 0.0f)), glm::vec3(0.07f, 0.06f, 0.07f)), steel, 0.6f, 32.0f, 0.0f);
		const float twitch = (std::sin(t * 1.3f) > 0.97f) ? 0.5f * std::sin(t * 40.0f) : 0.0f;
		glm::mat4 H = glm::rotate(glm::rotate(glm::translate(B, glm::vec3(-0.02f, 0.67f + bob, 0.0f)), twitch, glm::vec3(0, 1, 0)), 0.06f, glm::vec3(1, 0, 0));
		part(headMesh, glm::scale(H, glm::vec3(0.80f, 0.70f, 0.80f)), grey, 0.8f, 64.0f, 0.0f);
		const float flick = (std::sin(t * 7.1f) * std::sin(t * 13.7f) > 0.92f) ? 0.12f : 1.0f;
		box(H, glm::vec3(0.12f, 0.02f, 0.0f), glm::vec3(0.06f, 0.06f, 0.24f), dark, 0.9f, 96.0f, 0.0f);                // visor slit
		part(sphereMesh, glm::scale(glm::translate(H, glm::vec3(0.145f, 0.02f, 0.0f)), glm::vec3(0.30f)), glm::vec3(0.2f, 0.95f, 1.0f), 0.0f, 1.0f, 2.6f * flick);   // eye

		// a bent antenna with a blinking tip (and a spark when the visor flickers)
		glm::mat4 AN = glm::rotate(glm::translate(H, glm::vec3(-0.05f, 0.10f, 0.07f)), 0.5f, glm::vec3(1, 0, 0));
		part(unitCylMesh, glm::scale(AN, glm::vec3(0.012f, 0.20f, 0.012f)), grey, 0.7f, 32.0f, 0.0f);
		part(sphereMesh, glm::scale(glm::translate(AN, glm::vec3(0.0f, 0.205f, 0.0f)), glm::vec3(0.16f)), glm::vec3(1.0f, 0.1f, 0.1f), 0.0f, 1.0f, (std::sin(t * 5.0f) > 0.0f) ? 2.5f : 0.2f);
		if (flick < 1.0f)
			part(sphereMesh, glm::scale(glm::translate(AN, glm::vec3(0.0f, 0.17f, 0.02f)), glm::vec3(0.22f)), glm::vec3(1.0f, 0.9f, 0.5f), 0.0f, 1.0f, 4.0f);
	}

	// ---- additive effects: scanner beams, terminal links, lock-on rings, strike columns ----
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glDepthMask(GL_FALSE);

	if (exitGfx.enabled)                                       // soft green light spilling out of the doorway
	{
		const MaterialDef& gm = scene.materials[scene.exitPath.glowMaterial];
		drawAlpha = 0.10f + 0.05f * std::sin((float)tnow * 3.0f);
		DrawMesh(exitGfx.glowMesh, exitGfx.glowModel, matTexture[scene.exitPath.glowMaterial], 0.0f, 1.0f, gm.colour, 1.5f);
	}

	for (size_t bi = 0; bi < scene.bulbs.size(); ++bi)
	{
		const Bulb& b = scene.bulbs[bi];
		const float pulse = 0.5f + 0.5f * std::sin((float)tnow * 18.0f);

		if (b.scanner && b.power > 0.0f)                       // the volumetric beam: streaked, brightest at the emitter
		{
			glm::vec3 P = b.position, A(b.aim.x, 0.0f, b.aim.y), d = A - P;
			float L = glm::length(d);
			if (L > 0.01f)
			{
				float r = L * std::tan(glm::radians(b.edgeDeg));
				glm::mat4 bm = glm::scale(glm::translate(glm::mat4(1.0f), P) * AlignDown(d / L), glm::vec3(r, L, r));
				drawAlpha = (b.state == ScanState::Locked ? 0.30f + 0.15f * pulse : 0.15f) + 0.12f * b.detect;
				DrawMesh(coneMesh, bm, beamTex, 0.0f, 1.0f, b.colour, 1.0f);
			}

			// the floor marker: a radar disc sized to the area the game really treats as "scanned" (ExposureEllipse),
			// spinning (faster once locked on), tinted by the scanner colour and hidden wherever a blocker shades the floor
			glm::vec2 ec; float eyaw, ea, eb;
			if (ExposureEllipse(scene, b, ec, eyaw, ea, eb))
			{
				float spin = (float)tnow * (b.state == ScanState::Locked ? 3.5f : 0.9f);
				glm::mat4 dm = glm::translate(glm::mat4(1.0f), glm::vec3(ec.x, 0.14f, ec.y));
				dm = glm::rotate(dm, eyaw, glm::vec3(0, 1, 0));
				dm = glm::scale(dm, glm::vec3(ea, 1.0f, eb));
				dm = glm::rotate(dm, spin, glm::vec3(0, 1, 0));
				drawAlpha = 0.95f;
				drawDecalLight = (int)bi;
				DrawMesh(decalMesh, dm, scannerTex, 0.0f, 1.0f, b.colour, 1.6f);
				drawDecalLight = -1;
			}
		}
		if (b.state == ScanState::Locked)                      // where the strike will land
		{
			float rr = scene.strikeRadius * (1.0f - 0.12f * pulse);
			drawAlpha = 0.30f + 0.25f * pulse;
			DrawMesh(unitCylMesh, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(b.aim.x, 0.14f, b.aim.y)), glm::vec3(rr, 0.02f, rr)),
				whiteTex, 0.0f, 1.0f, glm::vec3(1.0f, 0.05f, 0.03f), 2.0f);
		}
		else if (b.state == ScanState::Striking)               // the incineration column
		{
			float k = glm::clamp(b.stateTime / 0.6f, 0.0f, 1.0f);
			float rr = scene.strikeRadius * (1.0f - 0.3f * k);
			drawAlpha = 0.65f * (1.0f - k);
			DrawMesh(unitCylMesh, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(b.aim.x, 0.0f, b.aim.y)), glm::vec3(rr, 14.0f, rr)),
				whiteTex, 0.0f, 1.0f, glm::vec3(1.0f, 0.55f, 0.12f), 2.0f);
		}
	}

	// thin cyan lines from every terminal to the scanners it controls (bright when the droid is close)
	for (size_t ti = 0; ti < scene.terminals.size(); ++ti)
	{
		const Terminal& t = scene.terminals[ti];
		glm::vec3 from(t.pos.x, 1.5f, t.pos.y);
		for (int li : t.links)
		{
			glm::vec3 d = scene.bulbs[li].position - from;
			float L = glm::length(d);
			if (L < 0.01f) continue;
			glm::mat4 lm = glm::scale(glm::translate(glm::mat4(1.0f), from) * AlignDown(-d / L), glm::vec3(0.02f, L, 0.02f));
			drawAlpha = ((int)ti == game.nearTerminal) ? 0.55f : 0.12f;
			DrawMesh(unitCylMesh, lm, whiteTex, 0.0f, 1.0f, glm::vec3(0.1f, 0.9f, 1.0f), 2.0f);
		}
	}

	drawAlpha = 1.0f;
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}

// ---------------------------------------------------------------------------
// passes
// ---------------------------------------------------------------------------
static void DirectionalShadowMapPass(DirectionalLight* light)
{
	// nothing in the level that casts a shadow ever moves, so the directional map is rendered once
	if (R.cacheEnabled && dirValid) return;
	directionalShadowShader.UseShader();

	glViewport(0, 0, light->getShadowMap()->GetShadowWidth(), light->getShadowMap()->GetShadowHeight());

	light->getShadowMap()->Write();
	glClear(GL_DEPTH_BUFFER_BIT);

	directionalShadowShader.SetDirectionalLightTransform(light->CalculateLightTransform());
	directionalShadowShader.Validate();

	DrawCasters(directionalShadowShader.GetModelLocation());

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	dirValid = true;
	++R.mapsRebuilt;
}

// ONE draw call per light: omni_shadow_map.geom sends every triangle to all six cube faces (gl_Layer)
static void OmniShadowMapPass(PointLight* light, int slot)
{
	glm::vec3 p = light->GetPosition();
	if (R.cacheEnabled && omniValid[slot] && omniLastPos[slot] == p) return;     // E1: this light did not move
	omniValid[slot] = true;
	omniLastPos[slot] = p;
	++R.mapsRebuilt;

	omniShadowShader.UseShader();

	glViewport(0, 0, light->getShadowMap()->GetShadowWidth(), light->getShadowMap()->GetShadowHeight());

	light->getShadowMap()->Write();
	glClear(GL_DEPTH_BUFFER_BIT);

	glUniform3f(omniShadowShader.GetOmniLightPosLocation(), p.x, p.y, p.z);
	glUniform1f(omniShadowShader.GetFarPlaneLocation(), light->GetFarPlane());
	omniShadowShader.SetLightMatrices(light->CalculateLightTransform());

	omniShadowShader.Validate();

	DrawCasters(omniShadowShader.GetModelLocation());

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void UpdateShadowMaps(bool forceAll)
{
	if (forceAll) { dirValid = false; for (bool& v : omniValid) v = false; }
	R.mapsRebuilt = 0;

	shadowTimer.Begin();
	DirectionalShadowMapPass(&mainLight);
	for (unsigned int i = 0; i < pointLightCount; i++)
		OmniShadowMapPass(&pointLights[i], (int)i);
	for (unsigned int i = 0; i < spotLightCount; i++)
		OmniShadowMapPass(&spotLights[i], (int)(MAX_POINT_LIGHTS + i));
	shadowTimer.End();
}

// binds the lights, their shadow maps and the camera matrices for the main program
static void SetSceneUniforms(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& eye, int debugMode)
{
	mainShader.UseShader();

	glUniformMatrix4fv(mainShader.GetProjectionLocation(), 1, GL_FALSE, glm::value_ptr(proj));
	glUniformMatrix4fv(mainShader.GetViewLocation(), 1, GL_FALSE, glm::value_ptr(view));
	glUniform3f(mainShader.GetEyePositionLocation(), eye.x, eye.y, eye.z);

	mainShader.SetDirectionalLight(&mainLight);
	mainShader.SetPointLights(pointLights, pointLightCount, UNIT_OMNI_FIRST, 0);
	mainShader.SetSpotLights(spotLights, spotLightCount, UNIT_OMNI_FIRST + pointLightCount, pointLightCount);
	mainShader.SetDirectionalLightTransform(mainLight.CalculateLightTransform());

	mainLight.getShadowMap()->Read(GL_TEXTURE0 + UNIT_DIR_SHADOW);
	mainShader.SetTexture(UNIT_TEXTURE);
	mainShader.SetDirectionalShadowMap(UNIT_DIR_SHADOW);

	float bias = R.omniBias;
	if (R.breakMode == 1) bias = 1000.0f;       // GPU shadows effectively off
	if (R.breakMode == 3) bias = 2.5f;          // shadows detach from their casters
	mainShader.SetFloat("omniBias", bias);
	mainShader.SetFloat("litThreshold", scene.litThreshold);
	mainShader.SetInt("debugMode", debugMode);
	mainShader.SetFloat("alphaMul", 1.0f);
	mainShader.SetVec2("uvOffset", glm::vec2(0.0f));
	drawAlpha = 1.0f; drawUV = glm::vec2(0.0f);

	mainShader.Validate();
}

static void RenderPass(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
{
	glViewport(0, 0, g_fbW, g_fbH);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	skybox.DrawSkybox(viewMatrix, projectionMatrix);          // depth writes off inside: the sky never occludes anything

	SetSceneUniforms(viewMatrix, projectionMatrix, camera.getCameraPosition(), R.debugView);
	DrawWorld(true);
}

// ---------------------------------------------------------------------------
// GPU probe: render the floor point (x,0,z) from straight above through a tiny orthographic window
// into a float framebuffer with debugMode 2 and read the centre pixel back. Returns exactly what the
// fragment shader computed there (gIllum, gMask).                                            (R2 / E2)
// ---------------------------------------------------------------------------
static bool CreateProbe()
{
	const int S = 15;       // odd size: the centre pixel is exactly at the probe position
	glGenTextures(1, &R.probeTex);
	glBindTexture(GL_TEXTURE_2D, R.probeTex);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, S, S);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenRenderbuffers(1, &R.probeDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, R.probeDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, S, S);
	glGenFramebuffers(1, &R.probeFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, R.probeFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R.probeTex, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, R.probeDepth);
	GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	return st == GL_FRAMEBUFFER_COMPLETE;
}

static bool ProbeGPU(float x, float z, float& illum, unsigned& mask)
{
	const int S = 15;
	glBindFramebuffer(GL_FRAMEBUFFER, R.probeFbo);
	glViewport(0, 0, S, S);
	glClearColor(-1.0f, -1.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 view = glm::lookAt(glm::vec3(x, 6.0f, z), glm::vec3(x, 0.0f, z), glm::vec3(0, 0, -1));
	glm::mat4 proj = glm::ortho(-0.0375f, 0.0375f, -0.0375f, 0.0375f, 0.1f, 20.0f);   // 15 px * 0.005 = 0.075 wide

	SetSceneUniforms(view, proj, glm::vec3(x, 6.0f, z), 2);
	DrawWorld(false);

	float px[4] = { -1, -1, 0, 0 };
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glReadPixels(S / 2, S / 2, 1, 1, GL_RGBA, GL_FLOAT, px);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, g_fbW, g_fbH);

	if (px[0] < 0.0f) return false;           // nothing drawn at that pixel
	illum = px[0];
	mask = (unsigned)(px[1] + 0.5f);
	return true;
}

static std::string MaskStr(unsigned m)
{
	std::string s;
	for (int i = (int)scene.bulbs.size() - 1; i >= 0; --i) s += ((m >> i) & 1u) ? '1' : '0';
	return s.empty() ? "-" : s;
}

// ---------------------------------------------------------------------------
// P3 study: 20 sample positions (10 within a hand's width of a shadow edge),
// CPU answer vs GPU answer, with the distance to the nearest shadow edge.
// Writes p3_results.csv next to the working directory.                                    (R5 / R2)
// ---------------------------------------------------------------------------
struct Lcg
{
	unsigned s = 20260429u;
	float next() { s = s * 1664525u + 1013904223u; return (float)((s >> 8) & 0xFFFFFF) / 16777216.0f; }
	float range(float a, float b) { return a + (b - a) * next(); }
};

static unsigned MaskAt(const Scene& sc, const glm::vec2& p, bool los)
{
	return ComputeIllumination(sc, glm::vec3(p.x, SAMPLE_HEIGHT, p.y), los).visMask;
}

static float NearestEdgeDistance(const Scene& sc, const glm::vec2& p, bool los, float maxR)
{
	unsigned m0 = MaskAt(sc, p, los);
	float best = maxR + 1.0f;
	for (int a = 0; a < 72; ++a)
	{
		float ang = (float)a * (2.0f * PI_F / 72.0f);
		glm::vec2 dir(std::cos(ang), std::sin(ang));
		for (float r = 0.0025f; r <= maxR; r += 0.0025f)
		{
			glm::vec2 q = p + dir * r;
			if (PointInsideSolid(sc, q.x, q.y, 0.0f)) break;      // an object's boundary is not a shadow edge
			if (MaskAt(sc, q, los) != m0) { best = std::min(best, r); break; }
		}
	}
	return best <= maxR ? best : -1.0f;
}

static void RunP3Study()
{
	Scene& sc = scene;
	std::printf("\n========== P3 STUDY: CPU lit-test vs GPU shadow lookup ==========\n");
	std::vector<Bulb> saved = sc.bulbs;
	for (auto& b : sc.bulbs) b.Reset();                  // freeze every light at its start state
	SyncLightsFromGame();
	UpdateShadowMaps(true);

	const bool los = true;
	Lcg rng;
	struct Sample { glm::vec2 p; bool edge; };
	std::vector<Sample> samples;
	const float offs[3] = { 0.02f, 0.05f, 0.09f };

	// --- 10 edge samples: find a CPU shadow boundary along a ray, then step either side of it ---
	int edges = 0, guard = 0;
	while (edges < 5 && guard++ < 5000)
	{
		glm::vec2 p0(rng.range(sc.bounds.x + 1, sc.bounds.z - 1), rng.range(sc.bounds.y + 1, sc.bounds.w - 1));
		if (PointInsideSolid(sc, p0.x, p0.y, 0.5f)) continue;
		float ang = rng.range(0.0f, 2.0f * PI_F);
		glm::vec2 dir(std::cos(ang), std::sin(ang));
		unsigned m0 = MaskAt(sc, p0, los);
		glm::vec2 prev = p0;
		for (float t = 0.05f; t <= 6.0f; t += 0.05f)
		{
			glm::vec2 q = p0 + dir * t;
			if (PointInsideSolid(sc, q.x, q.y, 0.3f) || q.x < sc.bounds.x + 0.5f || q.x > sc.bounds.z - 0.5f ||
				q.y < sc.bounds.y + 0.5f || q.y > sc.bounds.w - 0.5f) break;
			if (MaskAt(sc, q, los) != m0)
			{
				glm::vec2 a = prev, b = q;
				for (int k = 0; k < 30; ++k)
				{
					glm::vec2 mid = (a + b) * 0.5f;
					if (MaskAt(sc, mid, los) == m0) a = mid; else b = mid;
				}
				glm::vec2 e = (a + b) * 0.5f;
				float o = offs[edges % 3];
				samples.push_back({ e - dir * o, true });
				samples.push_back({ e + dir * o, true });
				++edges;
				break;
			}
			prev = q;
		}
	}
	// --- 10 ordinary samples ---
	guard = 0;
	while (samples.size() < 20 && guard++ < 5000)
	{
		glm::vec2 p(rng.range(sc.bounds.x + 1, sc.bounds.z - 1), rng.range(sc.bounds.y + 1, sc.bounds.w - 1));
		if (PointInsideSolid(sc, p.x, p.y, 0.5f)) continue;
		samples.push_back({ p, false });
	}

	std::ofstream csv("p3_results.csv");
	csv << "n,x,z,edge_sample,cpu_mask,gpu_mask,cpu_illum,gpu_illum,cpu_lit,gpu_lit,dist_to_nearest_shadow_edge,agree\n";
	std::printf("%-3s %-17s %-6s %-26s %-26s %-10s %s\n", "n", "position (x,z)", "edge?", "CPU says", "GPU says", "edge dist", "agree");

	int disagree = 0, probed = 0;
	for (size_t i = 0; i < samples.size(); ++i)
	{
		const glm::vec2 p = samples[i].p;
		LitResult cpu = ComputeIllumination(sc, glm::vec3(p.x, SAMPLE_HEIGHT, p.y), los);
		float gi = 0.0f; unsigned gm = 0;
		bool ok = ProbeGPU(p.x, p.y, gi, gm);
		if (!ok) { std::printf("%-3zu probe failed at (%.3f, %.3f)\n", i + 1, p.x, p.y); continue; }
		++probed;
		bool cl = cpu.illum >= sc.litThreshold, gl = gi >= sc.litThreshold;
		bool agree = (cpu.visMask == gm) && (cl == gl);
		if (!agree) ++disagree;
		float ed = NearestEdgeDistance(sc, p, los, 0.6f);

		char c1[96], c2[96], pos[48], edb[24];
		std::snprintf(c1, sizeof c1, "%s i=%.3f m=%s", cl ? "LIT   " : "SHADOW", cpu.illum, MaskStr(cpu.visMask).c_str());
		std::snprintf(c2, sizeof c2, "%s i=%.3f m=%s", gl ? "LIT   " : "SHADOW", gi, MaskStr(gm).c_str());
		std::snprintf(pos, sizeof pos, "(%7.3f,%7.3f)", p.x, p.y);
		if (ed >= 0) std::snprintf(edb, sizeof edb, "%.4f", ed); else std::snprintf(edb, sizeof edb, ">0.60");
		std::printf("%-3zu %-17s %-6s %-26s %-26s %-10s %s\n", i + 1, pos, samples[i].edge ? "yes" : "no", c1, c2, edb, agree ? "ok" : "DISAGREE");
		csv << (i + 1) << "," << p.x << "," << p.y << "," << (samples[i].edge ? 1 : 0) << "," << cpu.visMask << "," << gm << ","
			<< cpu.illum << "," << gi << "," << (cl ? 1 : 0) << "," << (gl ? 1 : 0) << "," << (ed >= 0 ? std::to_string(ed) : ">0.6")
			<< "," << (agree ? 1 : 0) << "\n";
	}
	std::printf("---- %d samples probed, %d disagreement(s). Saved p3_results.csv (mask bits: last light ... first light)\n", probed, disagree);
	std::printf("omni map %d x %d, bias %.3f world units, threshold %.3f\n\n", sc.omniRes, sc.omniRes, R.omniBias, sc.litThreshold);

	sc.bulbs = saved;                                       // put every light back where the game had it
	SyncLightsFromGame();
	UpdateShadowMaps(true);
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// HUD (R4): alarm bar, light meter, state banners, performance read-out
// ---------------------------------------------------------------------------

static void DrawHud(const Scene& sc, const Game& g, float frameMs, bool vsync)
{
    const int W = g_fbW, H = g_fbH;
    const int ts = std::max(2, H / 240);          // text scale (pixels per glyph pixel)
    const int big = ts * 3;
    Hud& hud = R.hud;
    hud.Begin(W, H);

    const glm::vec4 white(1, 1, 1, 1), grey(0.75f, 0.75f, 0.8f, 1), dim(0, 0, 0, 0.55f);
    auto centre = [&](const std::string& s, float y, int scale, const glm::vec4& c) {
        while (scale > 1 && hud.TextWidth(s, scale) > W * 0.96f) --scale;     // shrink to fit narrow windows
        hud.Text(s, (W - hud.TextWidth(s, scale)) * 0.5f, y, scale, c);
    };

    // red vignette that grows with the alarm
    if (g.state == GameState::Playing && g.alarm > 0.3f)
        hud.Rect(0, 0, (float)W, (float)H, glm::vec4(0.8f, 0.0f, 0.0f, 0.30f * (g.alarm - 0.3f) / 0.7f));

    // ---- ALARM METER (top centre) ----
    float barW = W * 0.40f, barH = 10.0f * ts, bx = (W - barW) * 0.5f, by = H - barH - 6.0f * ts - 8.0f * ts;
    hud.Text("SCANNER DETECTION", bx, by + barH + 2.0f * ts, ts, grey);
    hud.Rect(bx - 3, by - 3, barW + 6, barH + 6, glm::vec4(0, 0, 0, 0.7f));
    glm::vec3 col = (g.alarm < 0.5f) ? glm::mix(glm::vec3(0.1f, 0.8f, 0.2f), glm::vec3(1.0f, 0.85f, 0.1f), g.alarm * 2.0f)
                                     : glm::mix(glm::vec3(1.0f, 0.85f, 0.1f), glm::vec3(1.0f, 0.1f, 0.05f), (g.alarm - 0.5f) * 2.0f);
    hud.Rect(bx, by, barW * g.alarm, barH, glm::vec4(col, 1.0f));
    std::string pct = Fmt("%.0f", g.alarm * 100.0) + "%";
    hud.Text(pct, bx + barW - hud.TextWidth(pct, ts), by + barH + 2.0f * ts, ts, white);

    // ---- NOTEBOOK (top left): every digit collected for each slot - genuine AND decoy - WITHOUT its colour ----
    if (sc.puzzleEnabled && (g.state == GameState::Playing || g.state == GameState::Paused))
    {
        float nx = 12.0f, ny = H - 10.0f * ts;
        hud.Text("NOTEBOOK", nx, ny, ts, glm::vec4(1.0f, 0.95f, 0.4f, 1));
        for (int slot = 0; slot < 4; ++slot)
        {
            float rowY = ny - (float)(slot + 1) * 9.0f * ts;
            hud.Text("SLOT " + std::to_string(slot + 1) + ":", nx, rowY, ts, grey);
            float cx = nx + hud.TextWidth("SLOT 4:  ", ts);
            std::vector<int> shown;
            for (const auto& e : g.notebook[slot])
            {
                if (std::find(shown.begin(), shown.end(), e.digit) != shown.end() && !NOTEBOOK_SHOWS_COLOR) continue;
                shown.push_back(e.digit);
                if (NOTEBOOK_SHOWS_COLOR && e.colourIdx >= 0)                    // debug / easy mode: a swatch behind the digit
                    hud.Rect(cx - 2, rowY - 1, 7.0f * ts, 8.0f * ts, glm::vec4(sc.codePuzzle.palette[e.colourIdx].colour, 1.0f));
                hud.Text(std::to_string(e.digit), cx, rowY, ts, white);
                cx += 8.0f * ts;
            }
        }
    }

    // ---- LAMP BATTERY (under the detection bar). The lamp is not part of the detection test. ----
    if (g.state == GameState::Playing || g.state == GameState::Paused)
    {
        float lw = barW * 0.35f, lh = 5.0f * ts, lxb = (W - lw) * 0.5f, lyb = by - 11.0f * ts;
        hud.Rect(lxb - 2, lyb - 2, lw + 4, lh + 4, glm::vec4(0, 0, 0, 0.7f));
        glm::vec3 bc = (g.battery < sc.lampFlickerBelow) ? glm::vec3(1.0f, 0.3f, 0.2f) : glm::vec3(0.3f, 0.85f, 1.0f);
        hud.Rect(lxb, lyb, lw * g.battery, lh, glm::vec4(bc, g.flashOn ? 1.0f : 0.55f));
        std::string ls = g.lampLockout ? std::string("LAMP FLAT  RECHARGING") : std::string("LAMP ") + (g.flashOn ? "ON " : "OFF ") + Fmt("%.0f", g.battery * 100.0) + "%";
        hud.Text(ls, lxb, lyb + lh + 1.5f * ts, ts, g.lampLockout ? glm::vec4(1.0f, 0.4f, 0.3f, 1) : grey);

        // NOISE: how far a drone can hear the droid right now (scanners ignore it)
        float nyb = lyb - 14.0f * ts;
        float nmax = std::max(1.0f, sc.noiseSprint + sc.noiseLampBonus), nf = glm::clamp(g.noiseRadius / nmax, 0.0f, 1.0f);
        hud.Rect(lxb - 2, nyb - 2, lw + 4, lh + 4, glm::vec4(0, 0, 0, 0.7f));
        hud.Rect(lxb, nyb, lw * nf, lh, glm::vec4(glm::mix(glm::vec3(0.3f, 0.9f, 0.4f), glm::vec3(1.0f, 0.35f, 0.15f), nf), 1.0f));
        hud.Text("NOISE " + Fmt("%.1f", g.noiseRadius) + " M", lxb, nyb + lh + 1.5f * ts, ts, grey);
    }

    // ---- LIGHT METER (bottom left): the number the alarm is actually driven by (E2) ----
    float lx = 12.0f, ly = 12.0f;
    hud.Rect(lx - 6, ly - 6, 88.0f * ts + 12.0f, 40.0f * ts, dim);
    hud.Text(g.lit ? "SCANNED" : "HIDDEN", lx, ly + 20.0f * ts, ts * 2, g.lit ? glm::vec4(1.0f, 0.35f, 0.25f, 1) : glm::vec4(0.3f, 0.9f, 0.5f, 1));
    hud.Text("LIGHT " + Fmt("%.3f", g.illum), lx, ly + 11.0f * ts, ts, white);
    if (R.liveProbe) hud.Text("GPU   " + Fmt("%.3f", R.liveGpuIllum), lx, ly + 2.0f * ts, ts, glm::vec4(0.5f, 0.9f, 1.0f, 1));
    else             hud.Text("THRESHOLD " + Fmt("%.2f", sc.litThreshold), lx, ly + 2.0f * ts, ts, grey);

    // ---- performance (top right) ----
    float rx = W - 12.0f;
    auto right = [&](const std::string& s, float y, const glm::vec4& c) { hud.Text(s, rx - hud.TextWidth(s, ts), y, ts, c); };
    right(Fmt("%.1f", frameMs) + " MS " + (vsync ? "VSYNC" : "NO VSYNC"), H - 10.0f * ts, grey);
    right("SHADOW GPU " + Fmt("%.2f", R.shadowGpuMs) + " MS", H - 19.0f * ts, grey);
    right(std::string("CACHE ") + (R.cacheEnabled ? "ON " : "OFF ") + std::to_string(R.mapsRebuilt) + "/" +
          std::to_string(1 + 1 + scene.bulbs.size()) + " MAPS", H - 28.0f * ts, grey);
    if (frameLogger.IsRunning())
        right(frameLogger.IsWarmingUp() ? std::string("TIMING RUN  WARMING UP")
              : "TIMING RUN  " + std::to_string(frameLogger.MeasuredFrames()) + " OF " + std::to_string(FrameTimeLogger::MeasurementFrames),
              H - 37.0f * ts, glm::vec4(1.0f, 0.85f, 0.2f, 1.0f));

    // ---- level / time (bottom right) ----
    {
        std::string s = "LEVEL " + std::to_string(g.level);
        hud.Text(s, W - 12.0f - hud.TextWidth(s, ts), 12.0f + 9.0f * ts, ts, white);
        std::string t = "TIME " + Fmt("%.1f", g.elapsed);
        hud.Text(t, W - 12.0f - hud.TextWidth(t, ts), 12.0f, ts, white);
    }

    // ---- lock-on warning, hacking prompts, status line (only while playing) ----
    if (g.state == GameState::Playing)
    {
        if (g.lockedCount > 0)
        {
            bool blink = std::fmod(glfwGetTime() * 6.0, 2.0) < 1.0;
            centre("LOCK ON  STRIKE IN " + Fmt("%.1f", std::max(g.nextStrike, 0.0f)) + "  RUN", H * 0.74f, ts * 3,
                   blink ? glm::vec4(1.0f, 0.15f, 0.1f, 1) : glm::vec4(1.0f, 0.6f, 0.2f, 1));
        }
        if (g.nearTerminal >= 0)
        {
            const Terminal& t = sc.terminals[g.nearTerminal];
            if (t.kind == Terminal::Kind::Code)
            {
                if (t.used) centre("TERMINAL EMPTY", H * 0.30f, ts * 2, grey);
                else
                {
                    centre("HOLD E TO HACK", H * 0.30f, ts * 2, glm::vec4(0.3f, 1.0f, 0.9f, 1));
                    float hw = W * 0.22f, hh = 5.0f * ts, hx = (W - hw) * 0.5f, hy = H * 0.30f - 8.0f * ts;
                    hud.Rect(hx - 2, hy - 2, hw + 4, hh + 4, glm::vec4(0, 0, 0, 0.7f));
                    hud.Rect(hx, hy, hw * g.hackProgress, hh, glm::vec4(0.1f, 0.9f, 0.8f, 1));
                    // the colour of this terminal, as a swatch AND as a word (helps colour-blind players)
                    if (t.colourIdx >= 0)
                    {
                        const PaletteColour& pc = sc.codePuzzle.palette[t.colourIdx];
                        std::string nm = pc.name + " TERMINAL";
                        float sw = 10.0f * ts, tw = (float)hud.TextWidth(nm, ts * 2), x0 = (W - (tw + sw + 6.0f * ts)) * 0.5f, y0 = H * 0.30f - 20.0f * ts;
                        hud.Rect(x0 - 2, y0 - 2, sw + 4, 8.0f * ts + 4, glm::vec4(1, 1, 1, 1));
                        hud.Rect(x0, y0, sw, 8.0f * ts, glm::vec4(pc.colour, 1.0f));
                        hud.Text(nm, x0 + sw + 6.0f * ts, y0, ts * 2, white);
                    }
                }
            }
            else if (t.cooldown > 0.0f)
                centre("TERMINAL REBOOTING " + Fmt("%.1f", t.cooldown), H * 0.30f, ts * 2, grey);
            else
            {
                centre("HOLD E TO HACK", H * 0.30f, ts * 2, glm::vec4(0.3f, 1.0f, 0.9f, 1));
                float hw = W * 0.22f, hh = 5.0f * ts, hx = (W - hw) * 0.5f, hy = H * 0.30f - 8.0f * ts;
                hud.Rect(hx - 2, hy - 2, hw + 4, hh + 4, glm::vec4(0, 0, 0, 0.7f));
                hud.Rect(hx, hy, hw * g.hackProgress, hh, glm::vec4(0.1f, 0.9f, 0.8f, 1));
            }
        }
        if (g.bigMessageTime > 0.0f && !g.bigMessage.empty())                  // "SLOT 2 = 7"
            centre(g.bigMessage, H * 0.52f, ts * 6, glm::vec4(1.0f, 0.95f, 0.4f, 1));
        if (g.KeypadActive())                                                  // the door keypad
        {
            const PaletteColour& pc = sc.codePuzzle.palette[g.lockColour];
            const float py = H * 0.40f;
            hud.Rect(W * 0.5f - 34.0f * ts, py - 22.0f * ts, 68.0f * ts, 44.0f * ts, glm::vec4(0, 0, 0, 0.6f));
            centre("DOOR LOCKED", py + 12.0f * ts, ts * 3, white);
            std::string nm = "LOCK COLOUR  " + pc.name;
            float sw = 12.0f * ts, tw = (float)hud.TextWidth(nm, ts * 2), x0 = (W - (tw + sw + 6.0f * ts)) * 0.5f, y0 = py + 1.0f * ts;
            hud.Rect(x0 - 2, y0 - 2, sw + 4, 8.0f * ts + 4, glm::vec4(1, 1, 1, 1));
            hud.Rect(x0, y0, sw, 8.0f * ts, glm::vec4(pc.colour, 1.0f));
            hud.Text(nm, x0 + sw + 6.0f * ts, y0, ts * 2, white);
            std::string digits;
            for (int i = 0; i < 4; ++i) digits += (i < (int)g.entered.size() ? std::to_string(g.entered[i]) : std::string("-")) + (i < 3 ? " " : "");
            centre(digits, py - 10.0f * ts, ts * 5, glm::vec4(0.4f, 1.0f, 0.9f, 1));
            centre("TYPE 0-9   BACKSPACE   ENTER", py - 18.0f * ts, ts, grey);
        }
        if (g.messageTime > 0.0f) centre(g.message, H * 0.64f, ts * 2, glm::vec4(0.3f, 1.0f, 0.9f, 1));
        // status tags above the light meter
        float tagY = 12.0f + 44.0f * ts;
        if (g.onBelt)             { hud.Text("CONVEYOR", 12.0f, tagY, ts, glm::vec4(1.0f, 0.85f, 0.2f, 1)); tagY += 9.0f * ts; }
        if (g.onOil)              { hud.Text("OIL", 12.0f, tagY, ts, glm::vec4(0.55f, 0.7f, 1.0f, 1)); tagY += 9.0f * ts; }
        if (g.speedMult < 0.99f)  { hud.Text("SLOWED", 12.0f, tagY, ts, glm::vec4(1.0f, 0.6f, 0.2f, 1)); tagY += 9.0f * ts; }
        else if (g.speedMult > 1.01f) { hud.Text("SPRINT", 12.0f, tagY, ts, glm::vec4(0.5f, 1.0f, 0.6f, 1)); tagY += 9.0f * ts; }
    }

    // "break it on purpose" banner
    const char* breakNames[] = { "", "BROKEN: GPU SHADOWS OFF", "BROKEN: CPU IGNORES WALLS", "BROKEN: HUGE SHADOW BIAS" };
    if (R.breakMode > 0) centre(breakNames[R.breakMode], H * 0.82f, ts * 2, glm::vec4(1.0f, 0.5f, 0.1f, 1));
    if (R.debugView == 1) centre("GAME VIEW  RED = SCANNED  BLUE = HIDDEN", H * 0.78f, ts, glm::vec4(0.6f, 1, 0.6f, 1));

    // ---- state screens ----
    if (g.state == GameState::Title)
    {
        hud.Rect(0, 0, (float)W, (float)H, glm::vec4(0, 0, 0, 0.62f));
        centre("BLACKOUT", H * 0.68f, big * 2, white);
        centre("YOU ARE SCHEDULED FOR THE INCINERATOR", H * 0.59f, ts * 2, glm::vec4(1.0f, 0.55f, 0.2f, 1));
        centre("REACH THE GREEN HATCH", H * 0.48f, ts * 2, glm::vec4(0.4f, 1, 0.5f, 1));
        centre("YELLOW SCANNER BEAMS FILL DETECTION  HIDE BEHIND SCRAP", H * 0.42f, ts * 2, white);
        centre("RED MEANS LOCK ON  RUN BEFORE THE STRIKE LANDS", H * 0.36f, ts * 2, white);
        centre("HACK TERMINALS TO REROUTE THE SCANNERS", H * 0.30f, ts * 2, glm::vec4(0.3f, 1.0f, 0.9f, 1));
        centre("WASD MOVE   MOUSE LOOK   HOLD E HACK   F LAMP   P PAUSE   R RESTART", H * 0.23f, ts, grey);
        centre("PRESS ENTER TO START", H * 0.13f, big, white);
    }
    else if (g.state == GameState::Paused)
    {
        hud.Rect(0, 0, (float)W, (float)H, glm::vec4(0, 0, 0, 0.5f));
        centre("PAUSED", H * 0.52f, big * 2, white);
        centre("PRESS P TO CONTINUE", H * 0.42f, ts * 2, grey);
    }
    else if (g.state == GameState::Won)
    {
        hud.Rect(0, 0, (float)W, (float)H, glm::vec4(0, 0.25f, 0, 0.55f));
        centre("ESCAPED", H * 0.58f, big * 2, glm::vec4(0.4f, 1, 0.5f, 1));
        centre("TIME " + Fmt("%.1f", g.finalTime) + " SECONDS", H * 0.48f, ts * 2, white);
        centre("PRESS R FOR LEVEL " + std::to_string(g.level + 1) + "  SCANNERS SWEEP FASTER", H * 0.38f, ts * 2, white);
    }
    else if (g.state == GameState::Lost)
    {
        hud.Rect(0, 0, (float)W, (float)H, glm::vec4(0.35f, 0, 0, 0.6f));
        centre("INCINERATED", H * 0.58f, big * 2, glm::vec4(1, 0.3f, 0.25f, 1));
        centre("LASTED " + Fmt("%.1f", g.finalTime) + " SECONDS", H * 0.48f, ts * 2, white);
        centre("PRESS R TO TRY AGAIN", H * 0.38f, ts * 2, white);
    }
    hud.End();
}


// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
static int Fail(const char* msg)
{
	std::cerr << "\n" << msg << "\n\nPress Enter to close...";
	std::cin.get();
	return 1;
}

static void ResetCamera()
{
	glm::vec2 d = scene.exitPos - scene.playerStart;
	float yaw = glm::degrees(std::atan2(d.y, d.x));          // look from the start toward the exit
	camera.setView(yaw, -38.0f);
}

int main(int argc, char** argv)
{
	// ---- 1. the level (CPU only; nothing here needs OpenGL, so a bad file never leaves a half-built window) ----
	// usage:  BLACKOUT.exe [level.json] [--seed N]     (--seed makes the code puzzle deterministic)
	std::string levelPath = "level01.json";
	bool seedGiven = false; unsigned long seedValue = 0;
	for (int i = 1; i < argc; ++i)
	{
		std::string a = argv[i];
		if (a == "--seed" && i + 1 < argc) { seedValue = std::strtoul(argv[++i], nullptr, 10); seedGiven = true; }
		else if (a.rfind("--", 0) != 0) levelPath = a;
	}
	if (!SceneLoader::Load(levelPath, scene))
	{
		std::cerr << "\nThe level cannot be played:\n";
		for (const auto& m : scene.messages) std::cerr << "  " << m << "\n";
		return Fail("Fix the level file (or run the program from the SkyboxLab folder) and start again.");
	}

	// ---- 2. window + GL ----
	mainWindow = new Window(scene.windowW, scene.windowH, scene.title.c_str());
	if (mainWindow->Initialise() != 0) return Fail("Could not create an OpenGL 4.6 window. Update the graphics driver, or see decision-log entry 2 for the 3.3 fallback.");
	g_fbW = mainWindow->getBufferWidth(); g_fbH = mainWindow->getBufferHeight();

	CreateShaders();
	if (!mainShader.IsValid() || !directionalShadowShader.IsValid() || !omniShadowShader.IsValid())
		return Fail("A shader failed to compile or link - the message above says which line.");

	CreateObjects();
	CreateLights();

	std::vector<std::string> skyboxFaces;
	for (int i = 0; i < 6; ++i) skyboxFaces.push_back(scene.skyFaces[i]);
	skybox = Skybox(skyboxFaces);

	camera = Camera(glm::vec3(0.0f, 5.0f, 10.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, -38.0f, 5.0f, 0.15f);

	R.omniBias = scene.omniBias;
	if (!R.hud.Init()) return Fail("The HUD shader failed to build.");
	if (!CreateProbe()) std::cerr << "[warning] the GPU probe framebuffer is incomplete: keys G, L and F9 will not work\n";
	shadowTimer.Init();
	mainWindow->setVSync(true);

	if (seedGiven) { game.SeedRng((std::uint32_t)seedValue); std::printf("random seed forced to %lu\n", seedValue); }
	game.Init(scene);
	ResetCamera();

	std::printf("\nBLACKOUT   %s\n", glGetString(GL_VERSION) ? (const char*)glGetString(GL_VERSION) : "?");
	std::printf("keys: WASD move | mouse look | hold E hack | F lamp | Enter start | R restart | P pause\n"
		"      1 GPU shadows off | 2 CPU ignores walls | 3 huge bias | 0 fix | [ ] bias | V game-view overlay\n"
		"      C shadow cache (E1) | Y vsync | T timing (300 frames) | L live GPU probe (E2) | G compare here | F9 P3 study\n\n");

	GLfloat lastTime = (GLfloat)glfwGetTime();
	GameState prevState = game.state;
	float prevElapsed = 0.0f;

	while (!mainWindow->getShouldClose())
	{
		frameLogger.BeginFrame();                           // R5: whole-frame timing starts here ...

		GLfloat now = (GLfloat)glfwGetTime();
		GLfloat deltaTime = now - lastTime;
		lastTime = now;
		if (deltaTime > 0.1f) deltaTime = 0.1f;            // after a stall, do not teleport the droid

		glfwPollEvents();
		g_fbW = std::max(1, (int)mainWindow->getBufferWidth());
		g_fbH = std::max(1, (int)mainWindow->getBufferHeight());

		// ---- input ----
		bool* keys = mainWindow->getsKeys();
		camera.orbitControl(mainWindow->getXChange(), mainWindow->getYChange());

		Input in;
		in.forward = (keys[GLFW_KEY_W] ? 1.0f : 0.0f) - (keys[GLFW_KEY_S] ? 1.0f : 0.0f);
		in.right   = (keys[GLFW_KEY_D] ? 1.0f : 0.0f) - (keys[GLFW_KEY_A] ? 1.0f : 0.0f);
		in.hack    = keys[GLFW_KEY_E];
		in.run     = keys[GLFW_KEY_LEFT_SHIFT];
		in.sneak   = keys[GLFW_KEY_LEFT_CONTROL];
		in.enter   = mainWindow->consumeKey(GLFW_KEY_ENTER);
		in.restart = mainWindow->consumeKey(GLFW_KEY_R);
		in.pause   = mainWindow->consumeKey(GLFW_KEY_P);

		// the door keypad: while it is active the digit keys type the code, so the debug keys 1/2/3/0 are ignored
		const bool keypadActive = game.KeypadActive();
		if (keypadActive)
		{
			for (int k = 0; k <= 9; ++k) if (mainWindow->consumeKey(GLFW_KEY_0 + k)) in.digit = k;
			if (mainWindow->consumeKey(GLFW_KEY_BACKSPACE)) in.backspace = true;
		}

		bool doCompare = false, doStudy = false;
		if (mainWindow->consumeKey(GLFW_KEY_F)) game.ToggleLamp();      // the game decides (battery); SyncLightsFromGame mirrors it
		if (!keypadActive && mainWindow->consumeKey(GLFW_KEY_1)) { R.breakMode = 1; std::printf("BREAK 1: GPU shadows off - the picture says lit everywhere, the game still hides you\n"); }
		if (!keypadActive && mainWindow->consumeKey(GLFW_KEY_2)) { R.breakMode = 2; std::printf("BREAK 2: CPU lit-test ignores walls - the picture shows shadow, the game says you are scanned\n"); }
		if (!keypadActive && mainWindow->consumeKey(GLFW_KEY_3)) { R.breakMode = 3; std::printf("BREAK 3: huge shadow bias - shadows detach from their casters\n"); }
		if (!keypadActive && mainWindow->consumeKey(GLFW_KEY_0)) { R.breakMode = 0; R.omniBias = scene.omniBias; std::printf("break modes cleared, bias %.3f\n", R.omniBias); }
		if (mainWindow->consumeKey(GLFW_KEY_LEFT_BRACKET))  { R.omniBias = std::max(0.0f, R.omniBias - 0.01f); std::printf("omni bias %.3f world units\n", R.omniBias); }
		if (mainWindow->consumeKey(GLFW_KEY_RIGHT_BRACKET)) { R.omniBias += 0.01f; std::printf("omni bias %.3f world units\n", R.omniBias); }
		if (mainWindow->consumeKey(GLFW_KEY_V)) { R.debugView = 1 - R.debugView; }
		if (mainWindow->consumeKey(GLFW_KEY_C)) { R.cacheEnabled = !R.cacheEnabled; std::printf("shadow-map cache (E1): %s\n", R.cacheEnabled ? "ON" : "OFF"); }
		if (mainWindow->consumeKey(GLFW_KEY_Y)) { R.vsync = !R.vsync; mainWindow->setVSync(R.vsync); std::printf("vsync: %s\n", R.vsync ? "ON" : "OFF"); }
		if (mainWindow->consumeKey(GLFW_KEY_T))
		{
			if (frameLogger.IsRunning()) frameLogger.Cancel();
			else
			{
				if (R.vsync) std::printf("[R5] WARNING: vsync is ON - every machine will read about 16.7 ms. Press Y to turn it off, then T again.\n");
				const char* gpu = (const char*)glGetString(GL_RENDERER);
				std::string label = (game.state == GameState::Title) ? "title" : "play";
				if (!R.cacheEnabled) label += "_nocache";
				frameLogger.Start(label, gpu ? gpu : "unknown", R.cacheEnabled, scene.omniRes);
			}
		}
		if (mainWindow->consumeKey(GLFW_KEY_L)) { R.liveProbe = !R.liveProbe; std::printf("live GPU probe (E2): %s\n", R.liveProbe ? "ON" : "OFF"); }
		if (mainWindow->consumeKey(GLFW_KEY_G)) doCompare = true;
		if (mainWindow->consumeKey(GLFW_KEY_F9)) doStudy = true;

		// ---- the game ----
		game.lineOfSightEnabled = (R.breakMode != 2);
		game.Update(deltaTime, scene, in, camera.getYawRadians());
		if ((prevState != GameState::Playing && game.state == GameState::Playing) || game.elapsed < prevElapsed - 0.001f) ResetCamera();
		prevState = game.state; prevElapsed = game.elapsed;
		camera.followTarget(game.playerPos + glm::vec3(0.0f, 0.6f, 0.0f), 7.5f);

		// ---- shadows ----
		SyncLightsFromGame();
		static int lastCoverVersion = -1;
		const bool coverChanged = (game.coverVersion != lastCoverVersion);     // a strike destroyed (or Reset restored) cover: the cached cube maps are stale
		lastCoverVersion = game.coverVersion;
		UpdateShadowMaps(coverChanged);
		shadowTimer.Poll();
		R.shadowGpuMs = shadowTimer.LastMs();

		// ---- E2 / P3 tools ----
		if (R.liveProbe || doCompare)
		{
			float gi = 0.0f; unsigned gm = 0;
			if (ProbeGPU(game.playerPos.x, game.playerPos.z, gi, gm))
			{
				R.liveGpuIllum = gi;
				if (doCompare)
					std::printf("[G] at (%.3f, %.3f): CPU illum %.4f mask %s | GPU pixel illum %.4f mask %s | threshold %.3f\n",
						game.playerPos.x, game.playerPos.z, game.illum, MaskStr(game.visMask).c_str(), gi, MaskStr(gm).c_str(), scene.litThreshold);
			}
		}
		if (doStudy) RunP3Study();

		// ---- draw ----
		glm::mat4 projection = glm::perspective(glm::radians(60.0f), (GLfloat)g_fbW / (GLfloat)g_fbH, 0.1f, 100.0f);   // rebuilt every frame: B3
		RenderPass(camera.calculateViewMatrix(), projection);
		DrawHud(scene, game, deltaTime * 1000.0f, R.vsync);

		mainWindow->swapBuffers();

		frameLogger.RecordExtras(R.shadowGpuMs, R.mapsRebuilt);
		frameLogger.EndFrame();                             // ... and ends here, after the buffer swap
	}

	return 0;
}
