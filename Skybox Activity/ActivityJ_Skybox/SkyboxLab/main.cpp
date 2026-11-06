// ---------------------------------------------------------------------------
//  OBSOLETE - industrial cyberpunk stealth, built on the Activity J (skybox) project.        owner: R1
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
	bool  timing = false;
	GLuint probeTex = 0, probeFbo = 0, probeDepth = 0;
	GLuint query = 0;
	bool  queryPending = false;
};

static Renderer R;
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
static Mesh* sphereMesh = nullptr, * unitCylMesh = nullptr, * coneMesh = nullptr, * bodyMesh = nullptr, * headMesh = nullptr, * padMesh = nullptr, * beaconMesh = nullptr;
static Texture* whiteTex = nullptr;
static Texture* padTex = nullptr;
static Texture* scannerTex = nullptr;      // floor marker: rim, rings, crosshair ticks, radar sweep
static Texture* beamTex = nullptr;         // beam: bright at the emitter, fading toward the floor, with streaks
static Mesh* decalMesh = nullptr;       // 2 x 2 flat quad, UVs 0..1, scaled to the exposure ellipse
static int      drawDecalLight = -1;       // >= 0 while a scanner marker is drawn (see shader.frag)

// the exit pathway (level file: game_logic.exit_zone.pathway): floor strip with scrolling chevrons, two glowing
// rails, a glowing doorway set into the wall, and a soft additive glow in the doorway
struct ExitGfx
{
	bool  enabled = false;
	Mesh* floorMesh = nullptr, * railMesh = nullptr, * doorMesh = nullptr, * postMesh = nullptr, * topMesh = nullptr, * glowMesh = nullptr;
	glm::mat4 floorModel{ 1.0f }, railModel[2], doorModel{ 1.0f }, postModel[2], topModel{ 1.0f }, glowModel{ 1.0f };
	float length = 1.0f;
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

	sphereMesh = MakeSphereMesh(0.25f, 24, 12);
	unitCylMesh = MakeCylinderMesh(1.0f, 1.0f, 40);
	coneMesh = MakeConeMesh(1.0f, 1.0f, 40);
	bodyMesh = MakeCylinderMesh(0.30f, 0.60f, 32);
	headMesh = MakeSphereMesh(0.22f, 24, 12);
	padMesh = MakeCylinderMesh(scene.exitRadius, 0.06f, 48);
	beaconMesh = MakeCylinderMesh(0.12f, 6.0f, 20);
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
		if (!scene.objects[i].castShadow) continue;
		glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(objModel[i]));
		objMesh[i]->RenderMesh();
	}
}

static void DrawWorld(bool extras)
{
	for (size_t i = 0; i < scene.objects.size(); ++i)
	{
		const SceneObject& o = scene.objects[i];
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
		DrawMesh(exitGfx.doorMesh, exitGfx.doorModel, matTexture[xp.glowMaterial], gm.specular, gm.shininess, gm.colour, gm.emissive * pulse * 1.2f);
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

	// terminal status lamps: cyan = ready, amber = being hacked, grey = rebooting
	for (const auto& t : scene.terminals)
	{
		glm::vec3 col = (t.cooldown > 0.0f) ? glm::vec3(0.35f, 0.35f, 0.4f)
			: (t.progress > 0.01f) ? glm::vec3(1.0f, 0.7f, 0.1f) : glm::vec3(0.1f, 1.0f, 0.85f);
		DrawMesh(sphereMesh, glm::translate(glm::mat4(1.0f), glm::vec3(t.pos.x, 1.5f, t.pos.y)), whiteTex, 0.0f, 1.0f, col, 2.0f);
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

	// the droid: a squat metal body, a flat head and one glowing eye
	if (game.state != GameState::Title)
	{
		glm::vec3 pp = game.playerPos + (game.onBelt ? glm::vec3(0, 0.10f, 0) : glm::vec3(0.0f));
		glm::vec3 fwd(game.facing.x, 0.0f, game.facing.y);
		DrawMesh(bodyMesh, glm::translate(glm::mat4(1.0f), pp + glm::vec3(0, 0.03f, 0)), whiteTex, 0.6f, 32.0f, glm::vec3(0.55f, 0.58f, 0.62f), 0.0f);
		DrawMesh(headMesh, glm::scale(glm::translate(glm::mat4(1.0f), pp + glm::vec3(0, 0.70f, 0)), glm::vec3(1.0f, 0.65f, 1.0f)), whiteTex,
			0.5f, 24.0f, glm::vec3(0.35f, 0.37f, 0.40f), 0.0f);
		DrawMesh(sphereMesh, glm::scale(glm::translate(glm::mat4(1.0f), pp + glm::vec3(0, 0.72f, 0) + fwd * 0.19f), glm::vec3(0.32f)), whiteTex,
			0.0f, 1.0f, glm::vec3(0.2f, 0.9f, 1.0f), 2.5f);
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

	if (R.query && !R.queryPending) { glBeginQuery(GL_TIME_ELAPSED, R.query); }
	DirectionalShadowMapPass(&mainLight);
	for (unsigned int i = 0; i < pointLightCount; i++)
		OmniShadowMapPass(&pointLights[i], (int)i);
	for (unsigned int i = 0; i < spotLightCount; i++)
		OmniShadowMapPass(&spotLights[i], (int)(MAX_POINT_LIGHTS + i));
	if (R.query && !R.queryPending) { glEndQuery(GL_TIME_ELAPSED); R.queryPending = true; }
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
			if (t.cooldown > 0.0f)
				centre("TERMINAL REBOOTING " + Fmt("%.1f", t.cooldown), H * 0.30f, ts * 2, grey);
			else
			{
				centre("HOLD E TO HACK", H * 0.30f, ts * 2, glm::vec4(0.3f, 1.0f, 0.9f, 1));
				float hw = W * 0.22f, hh = 5.0f * ts, hx = (W - hw) * 0.5f, hy = H * 0.30f - 8.0f * ts;
				hud.Rect(hx - 2, hy - 2, hw + 4, hh + 4, glm::vec4(0, 0, 0, 0.7f));
				hud.Rect(hx, hy, hw * g.hackProgress, hh, glm::vec4(0.1f, 0.9f, 0.8f, 1));
			}
		}
		if (g.messageTime > 0.0f) centre(g.message, H * 0.64f, ts * 2, glm::vec4(0.3f, 1.0f, 0.9f, 1));
		if (g.onBelt) hud.Text("CONVEYOR", 12.0f, 12.0f + 44.0f * ts, ts, glm::vec4(1.0f, 0.85f, 0.2f, 1));
	}

	// "break it on purpose" banner
	const char* breakNames[] = { "", "BROKEN: GPU SHADOWS OFF", "BROKEN: CPU IGNORES WALLS", "BROKEN: HUGE SHADOW BIAS" };
	if (R.breakMode > 0) centre(breakNames[R.breakMode], H * 0.82f, ts * 2, glm::vec4(1.0f, 0.5f, 0.1f, 1));
	if (R.debugView == 1) centre("GAME VIEW  RED = SCANNED  BLUE = HIDDEN", H * 0.78f, ts, glm::vec4(0.6f, 1, 0.6f, 1));

	// ---- state screens ----
	if (g.state == GameState::Title)
	{
		hud.Rect(0, 0, (float)W, (float)H, glm::vec4(0, 0, 0, 0.62f));
		centre("OBSOLETE", H * 0.68f, big * 2, white);
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
	std::string levelPath = (argc > 1) ? argv[1] : "level01.json";
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
	glGenQueries(1, &R.query);
	mainWindow->setVSync(true);

	game.Init(scene);
	ResetCamera();

	std::printf("\nOBSOLETE   %s\n", glGetString(GL_VERSION) ? (const char*)glGetString(GL_VERSION) : "?");
	std::printf("keys: WASD move | mouse look | hold E hack | F lamp | Enter start | R restart | P pause\n"
		"      1 GPU shadows off | 2 CPU ignores walls | 3 huge bias | 0 fix | [ ] bias | V game-view overlay\n"
		"      C shadow cache (E1) | Y vsync | T timing (300 frames) | L live GPU probe (E2) | G compare here | F9 P3 study\n\n");

	GLfloat lastTime = (GLfloat)glfwGetTime();
	GameState prevState = game.state;
	float prevElapsed = 0.0f;

	// timing harness (R5): average over 300 frames, milliseconds
	int   tFrames = 0; double tSum = 0.0, tMin = 1e9, tMax = 0.0, tShadow = 0.0, tMaps = 0.0;

	while (!mainWindow->getShouldClose())
	{
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
		in.right = (keys[GLFW_KEY_D] ? 1.0f : 0.0f) - (keys[GLFW_KEY_A] ? 1.0f : 0.0f);
		in.hack = keys[GLFW_KEY_E];
		in.enter = mainWindow->consumeKey(GLFW_KEY_ENTER);
		in.restart = mainWindow->consumeKey(GLFW_KEY_R);
		in.pause = mainWindow->consumeKey(GLFW_KEY_P);

		bool doCompare = false, doStudy = false;
		if (mainWindow->consumeKey(GLFW_KEY_F)) { spotLights[0].Toggle(); game.flashOn = !game.flashOn; }
		if (mainWindow->consumeKey(GLFW_KEY_1)) { R.breakMode = 1; std::printf("BREAK 1: GPU shadows off - the picture says lit everywhere, the game still hides you\n"); }
		if (mainWindow->consumeKey(GLFW_KEY_2)) { R.breakMode = 2; std::printf("BREAK 2: CPU lit-test ignores walls - the picture shows shadow, the game says you are scanned\n"); }
		if (mainWindow->consumeKey(GLFW_KEY_3)) { R.breakMode = 3; std::printf("BREAK 3: huge shadow bias - shadows detach from their casters\n"); }
		if (mainWindow->consumeKey(GLFW_KEY_0)) { R.breakMode = 0; R.omniBias = scene.omniBias; std::printf("break modes cleared, bias %.3f\n", R.omniBias); }
		if (mainWindow->consumeKey(GLFW_KEY_LEFT_BRACKET)) { R.omniBias = std::max(0.0f, R.omniBias - 0.01f); std::printf("omni bias %.3f world units\n", R.omniBias); }
		if (mainWindow->consumeKey(GLFW_KEY_RIGHT_BRACKET)) { R.omniBias += 0.01f; std::printf("omni bias %.3f world units\n", R.omniBias); }
		if (mainWindow->consumeKey(GLFW_KEY_V)) { R.debugView = 1 - R.debugView; }
		if (mainWindow->consumeKey(GLFW_KEY_C)) { R.cacheEnabled = !R.cacheEnabled; std::printf("shadow-map cache (E1): %s\n", R.cacheEnabled ? "ON" : "OFF"); }
		if (mainWindow->consumeKey(GLFW_KEY_Y)) { R.vsync = !R.vsync; mainWindow->setVSync(R.vsync); std::printf("vsync: %s\n", R.vsync ? "ON" : "OFF"); }
		if (mainWindow->consumeKey(GLFW_KEY_T)) { R.timing = !R.timing; tFrames = 0; tSum = tShadow = tMaps = 0.0; tMin = 1e9; tMax = 0.0; std::printf("timing harness: %s (press Y to turn vsync off first)\n", R.timing ? "RUNNING" : "stopped"); }
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
		UpdateShadowMaps(false);
		if (R.queryPending)
		{
			GLint avail = 0;
			glGetQueryObjectiv(R.query, GL_QUERY_RESULT_AVAILABLE, &avail);
			if (avail) { GLuint64 ns = 0; glGetQueryObjectui64v(R.query, GL_QUERY_RESULT, &ns); R.shadowGpuMs = (float)((double)ns / 1.0e6); R.queryPending = false; }
		}

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

		// ---- timing harness ----
		if (R.timing)
		{
			double ms = deltaTime * 1000.0;
			tSum += ms; tMin = std::min(tMin, ms); tMax = std::max(tMax, ms); tShadow += R.shadowGpuMs; tMaps += R.mapsRebuilt; ++tFrames;
			if (tFrames == 300)
			{
				std::printf("[TIMING] 300 frames: %.2f ms avg (min %.2f, max %.2f) = %.1f fps | shadow passes %.2f ms GPU avg | %.2f maps rebuilt per frame | cache %s | omni %d^2\n",
					tSum / 300.0, tMin, tMax, 300000.0 / tSum, tShadow / 300.0, tMaps / 300.0, R.cacheEnabled ? "ON" : "OFF", scene.omniRes);
				tFrames = 0; tSum = tShadow = tMaps = 0.0; tMin = 1e9; tMax = 0.0;
			}
		}

		mainWindow->swapBuffers();
	}

	return 0;
}
