#pragma once
// ---------------------------------------------------------------------------
// Scene.h / Scene.cpp - the level as plain data (no OpenGL in here)       owner: R1
//
// SceneLoader.cpp fills a Scene from level01.json. Three consumers read the SAME numbers:
//   * main.cpp  builds the meshes, the light objects and the draw calls from it,
//   * LitTest   (the CPU "is the player lit?" test) ray-tests the same boxes the shadow
//               maps draw,
//   * Game      collides against the same boxes.
// That is how the project avoids having two copies of the level that can disagree.
// ---------------------------------------------------------------------------
#include "CommonValues.h"

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct MaterialDef
{
    std::string id;
    std::string texturePath;
    float specular  = 0.3f;
    float shininess = 16.0f;
    glm::vec3 colour{ 1.0f };      // tint multiplied into the texture
    float emissive  = 0.0f;
    float tile      = 2.0f;        // world metres per texture repeat
};

struct SceneObject
{
    std::string id;
    std::string mesh = "primitive:box";
    int   material = 0;
    glm::vec3 pos{ 0.0f }, rot{ 0.0f }, scale{ 1.0f };   // rot in degrees (cosmetic only for non-solid objects)
    bool  solid = true;            // blocks the droid
    bool  castShadow = true;       // drawn into the cube maps AND ray-tested by the CPU lit-test
    // strikes can destroy this object (level file: "destructible": true). Only crates and barriers are marked:
    // walls, pillars, scrap stacks and terminals never are, so a level can never become unwinnable.
    bool  destructible = false;
    bool  destroyed = false;       // runtime: set by a strike, cleared by Game::Reset
    bool  origSolid = true, origCast = true;   // the values to restore on Reset
    glm::vec3 minB{ 0.0f }, maxB{ 0.0f };
    int   line = 0;
};

// A QC scanner: a fixed overhead emitter whose cone is steered over the floor.
//   Scanning : aim follows the active route (a drone follows its patrol loop); seeing the droid fills 'detect'
//   Locked   : red; the strike point chases the droid, then freezes; an incineration strike is coming
//              (a drone hovers while locked)
//   Striking : the strike lands (hit test), beam is off
//   Cooling  : beam off, then back to Scanning
enum class ScanState { Scanning, Locked, Striking, Cooling };

// A drone is normally on PATROL. When the droid makes noise within earshot it flies to the noise point (INVESTIGATE),
// hovers there, then goes back to the closest point of its patrol loop (RETURN) and resumes. Scanners never use this.
enum class BulbMode { Patrol, Investigate, Return };

// Every light the GAME cares about. A drone is an omnidirectional patrolling point light, a scanner is a
// cone light (an Activity J SpotLight) with a fixed mount point and two selectable aim routes.
struct Bulb
{
    std::string name;
    bool  scanner = false;
    glm::vec3 baseColour{ 1.0f };
    glm::vec3 colour{ 1.0f };          // colour as drawn (scanners go yellow -> orange -> red)
    float ambient = 0.0f, diffuse = 1.0f;
    float attC = 1.0f, attL = 0.0f, attE = 0.0f;   // att = attE*d*d + attL*d + attC  (same formula as shader.frag)
    float farPlane = 40.0f;
    float speed = 2.0f;                // drones: metres/second along the loop; scanners: aim speed on the floor
    glm::vec3 position{ 0.0f };
    int   line = 0;

    // ---- drone ----
    std::vector<glm::vec3> path;       // closed loop of (x,y,z) waypoints
    float pathLen = 0.0f, s = 0.0f;

    // ---- scanner ----
    glm::vec3 mount{ 0.0f };
    glm::vec2 aim{ 0.0f };             // floor point the cone is pointing at (x,z)
    std::vector<glm::vec2> route[2];   // two waypoint loops for the aim point; a terminal hack swaps them
    int    routeId = 0;
    size_t wp = 0;
    float  edgeDeg = 20.0f;            // cone half angle (SpotLight "edge")
    float  edgeCos = -2.0f;            // cos(edgeDeg); -2 = no cone (drone)
    float  power = 1.0f;               // 0 while striking / cooling
    ScanState state = ScanState::Scanning;
    float  detect = 0.0f;              // 0..1, 1 = lock-on
    float  stateTime = 0.0f;

    // ---- drone investigation ----
    BulbMode  mode = BulbMode::Patrol;
    glm::vec3 investigateTarget{ 0.0f };
    float     hoverTime = 0.0f;

    bool IsStatic() const { return path.size() < 2; }
    void Finalise();
    void Place();
    glm::vec3 PathPoint(float s) const;            // the point at distance s along the closed patrol loop
    float     NearestPathS() const;                // projects 'position' onto the loop and returns the s of the closest point
    bool      MoveToward(const glm::vec3& target, float speed, float dt);   // moves 'position' directly; true when it has arrived
    void Advance(float ds) { s += ds; Place(); }
    void Reset();
    glm::vec3 Dir() const;                       // unit vector emitter -> aim point on the floor
    void SweepAim(float dt, float aimSpeed);
    void SwitchRoute();
    float Power() const { return power; }
};

struct Conveyor
{
    std::string id;
    glm::vec2 centre{ 0.0f }, size{ 1.0f }, dir{ 1.0f, 0.0f };
    float speed = 2.0f;
    int   material = 0;
};

// A slick of oil on the floor: not solid, casts no shadow, slows the droid while its centre is inside the circle.
struct OilPuddle
{
    std::string id;
    glm::vec2 pos{ 0.0f };
    float radius = 1.0f;
    int   line = 0;
};

// route = the cyan terminals that rewrite a scanner's patrol route; code = terminals that hold one digit of the door code
struct PaletteColour
{
    std::string name;
    glm::vec3 colour{ 1.0f };
};

struct Terminal
{
    enum class Kind { Route, Code };
    Kind kind = Kind::Route;
    // ---- code terminals: filled in by Game::Reset (a new random assignment every attempt) ----
    int  codeSlot = -1;            // which of the 4 digits this terminal belongs to (0..3)
    int  digit = -1;               // the digit it gives (genuine: code[codeSlot]; decoy: a different digit)
    int  colourIdx = -1;           // index into Scene::codePuzzle.palette; genuine terminals have the LOCK colour
    bool genuine = false;
    bool used = false;             // one use per terminal

    std::string id;
    glm::vec2 pos{ 0.0f };
    std::vector<std::string> linkNames;   // as written in the file
    std::vector<int> links;               // indices into Scene::bulbs
    float cooldown = 0.0f;                // > 0 : rebooting
    float progress = 0.0f;                // 0..1 hack progress
    int   uses = 0;
    int   line = 0;
};

// The way out: a glowing green pathway that runs along the floor up to a glowing doorway in a wall
// (level file: game_logic.exit_zone.pathway). Purely visual - the win test is the exit zone circle.
struct ExitPath
{
    bool  enabled = false;
    glm::vec2 from{ 0.0f }, to{ 0.0f };     // floor points: where the pathway starts and where it meets the wall
    float width = 2.4f;
    float doorHeight = 2.6f;
    int   material = 0;                      // scrolling chevron floor
    int   glowMaterial = 0;                  // door panel, rails, frame (emissive green)
};

struct CodePuzzleDef
{
    std::vector<PaletteColour> palette;    // lock colours; genuine terminals show the lock colour, decoys any other
    float doorRadius = -1.0f;              // keypad range around the exit; < 0 = exitRadius + 1.5 (resolved by the loader)
    float hackTime = 2.0f;                 // seconds holding E on a code terminal
    float wrongPenalty = 0.5f;             // added to every light's detection after a wrong code
};

struct Scene
{
    std::string file;
    std::string title = "OBSOLETE";
    int windowW = 1366, windowH = 768;

    // rendering
    int   omniRes = 1024, dirRes = 2048;
    float omniBias = 0.05f;                // world units; shader.frag CalcOmniShadowFactor
    glm::vec3 dirDirection{ -0.4f, -0.7f, -0.5f }, dirColour{ 0.4f, 0.5f, 0.8f };
    float dirAmbient = 0.1f, dirDiffuse = 0.1f;

    // the droid's own lamp (spot light 0)
    bool  flashOn = true;
    glm::vec3 flashColour{ 0.6f, 0.9f, 1.0f };
    float flashDiffuse = 1.0f, flashEdgeDeg = 22.0f, flashAttC = 1.0f, flashAttL = 0.09f, flashAttE = 0.032f;
    // lamp battery (lights.player_lamp): fraction of a full charge per second
    float lampDrain = 0.04f, lampRecharge = 0.02f;
    float lampMinOn = 0.15f;               // after running flat the lamp cannot be switched on until the charge is back to this
    float lampFlickerBelow = 0.25f;        // below this charge the lamp flickers

    std::string skyFaces[6];

    std::vector<MaterialDef> materials;
    std::vector<SceneObject> objects;
    std::vector<Bulb>        bulbs;        // drones first, then scanners (the order the shader numbers them)
    int droneCount = 0;
    std::vector<Conveyor>    conveyors;
    std::vector<Terminal>    terminals;
    std::vector<OilPuddle>   oilPuddles;
    CodePuzzleDef            codePuzzle;
    bool                     puzzleEnabled = false;    // true when the level has code terminals; otherwise the door is never locked

    // game logic
    glm::vec2 playerStart{ 0.0f }, exitPos{ 0.0f };
    float exitRadius = 1.3f;
    ExitPath exitPath;
    glm::vec4 bounds{ -15.0f, -15.0f, 15.0f, 15.0f };   // minX, minZ, maxX, maxZ

    float litThreshold = 0.15f;
    float alarmFill = 1.1f, alarmDrain = 0.5f;
    float speedStep = 1.2f;
    float strikeDelay = 1.9f, strikeRadius = 2.0f, strikeCooldown = 3.0f;
    float minStrikeDelay = 0.45f;

    // movement speed multipliers (level file: "movement")
    float sprintMult = 1.35f, sneakMult = 0.55f;
    float litSpeedMult = 0.7f;             // while scanned (and no light is locked on)
    float oilSpeedMult = 0.55f;            // while on an oil puddle
    float minSpeedMult = 0.35f;            // the product of all multipliers never goes below this

    // noise (level file: "noise"): how far the droid can be heard, in metres
    float noiseStill = 0.5f, noiseSneak = 1.5f, noiseWalk = 4.5f, noiseSprint = 9.0f, noiseHacking = 5.0f;
    float noiseLampBonus = 1.5f;           // added while the lamp is on
    float noiseSmoothing = 12.0f;          // how quickly the reported radius follows the target (per second)
    float droneInvestigateSpeedMult = 1.3f, droneHoverTime = 2.0f, droneArriveDistance = 1.5f;          // the per-level shortening of strikeDelay never goes below this
    float hackTime = 1.2f, hackRadius = 1.9f, hackCooldown = 6.0f;

    std::vector<std::string> messages;      // every warning / error produced while loading
    bool playable = false;

    int FindMaterial(const std::string& id) const
    {
        for (size_t i = 0; i < materials.size(); ++i) if (materials[i].id == id) return (int)i;
        return -1;
    }
};

bool PointInsideSolid(const Scene& sc, float x, float z, float margin);
