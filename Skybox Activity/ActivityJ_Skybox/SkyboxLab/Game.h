#pragma once
// ---------------------------------------------------------------------------
// Game.h / Game.cpp - state machine, player, collision, alarm   (owner: R4)
//
//   TITLE   --Enter-->                      PLAYING
//   PLAYING --P-->  PAUSED  --P-->          PLAYING
//   PLAYING --reach exit-->                 WON
//   PLAYING --strike lands on the droid--> LOST   (a scanner's detection reaching 1.0 only LOCKS ON;
//                                                 the strike gameplay.strike.delay seconds later is what kills)
//   PLAYING --R-->                          PLAYING (restart, same level)
//   WON     --R or Enter-->                 PLAYING (next level: bulbs faster)
//   LOST    --R or Enter-->                 PLAYING (same level)
// ---------------------------------------------------------------------------
#include "Scene.h"
#include "LitTest.h"
#include <string>
#include <vector>
#include <random>
#include <cstdint>

enum class GameState { Title, Playing, Paused, Won, Lost };

// Flip to true to show the lock colour next to every notebook entry (makes the code puzzle much easier).
constexpr bool NOTEBOOK_SHOWS_COLOR = false;

struct Input
{
    float forward = 0.0f, right = 0.0f;       // -1..1
    bool enter = false, restart = false, pause = false, hack = false;
    bool run = false, sneak = false;          // Left Shift / Left Ctrl
    int  digit = -1;                          // 0..9 typed this frame at the door keypad (-1 = none)
    bool backspace = false;                   // delete the last typed digit (Enter = in.enter submits)
};

class Game
{
public:
    GameState state = GameState::Title;

    glm::vec3 playerPos{ 0.0f };
    glm::vec2 facing{ 1.0f, 0.0f };

    float alarm = 0.0f;                       // highest scanner detection, 0..1 (1 = that scanner locks on)
    float illum = 0.0f;                       // what the CPU lit test says at the player's feet
    LitResult litResult;                      // per-scanner contributions at the player's feet
    int   lockedCount = 0;                    // scanners currently locked on
    float nextStrike = -1.0f;                 // seconds until the soonest strike lands (-1 = none)
    int   nearTerminal = -1;                  // terminal in hacking range (-1 = none)
    float hackProgress = 0.0f;                // 0..1 for that terminal
    bool  onBelt = false;
    bool  onOil = false;                      // the droid is on an oil puddle
    float speedMult = 1.0f;                   // the multiplier applied to PLAYER_SPEED this frame
    std::string message;                      // short status line ("ROUTES REWRITTEN")
    float messageTime = 0.0f;
    unsigned int visMask = 0;
    bool  lit = false;
    bool  flashOn = true;
    float battery = 1.0f;                     // lamp charge 0..1
    bool  lampLockout = false;                // the lamp ran flat: it cannot be switched on until battery >= lights.player_lamp.min_to_turn_on
    float lampPower = 1.0f;                   // multiplier for the lamp's light (flickers when the battery is low); main.cpp applies it

    int   level = 1;
    float speedScale = 1.0f;
    float elapsed = 0.0f;                     // seconds in the current attempt
    float finalTime = 0.0f;

    // destroyed cover: every strike that lands within strikeRadius of a destructible object destroys it
    int   coverVersion = 0;                   // incremented whenever the set of destroyed objects changes; main.cpp watches it to refresh the shadow-map cache
    std::vector<glm::vec2> scorchMarks;       // where strikes landed (drawn as dark discs); cleared by Reset

    // ---- code door puzzle (Scene::puzzleEnabled) ----
    struct NoteEntry { int digit; int colourIdx; };
    bool  doorLocked = false;                 // the win test needs  inside the exit zone AND !doorLocked
    int   lockColour = -1;                    // palette index, chosen at random every Reset
    int   code[4] = { 0, 0, 0, 0 };
    std::vector<int> entered;                 // digits typed at the keypad so far
    std::vector<NoteEntry> notebook[4];       // every digit collected for each slot (genuine AND decoy); the colour is NOT shown unless NOTEBOOK_SHOWS_COLOR
    std::string bigMessage;                   // "SLOT 2 = 7", shown large for 3 s
    float bigMessageTime = 0.0f;
    glm::vec2 doorPos{ 0.0f };
    float doorRadius = 0.0f;
    bool  hacking = false;                    // the droid is holding E on a usable terminal this frame
    float noiseRadius = 0.5f;                 // metres within which a drone can hear the droid (smoothed)
    float noiseTarget = 0.5f;                 // the value noiseRadius is moving toward

    bool  lineOfSightEnabled = true;          // false = "break it on purpose" (CPU ignores walls)

    Game();
    void SeedRng(std::uint32_t seed);         // force a seed (command line "--seed N"): the puzzle assignment becomes deterministic
    bool KeypadActive() const;                // playing, door locked, and the droid is within the door radius
    void AssignPuzzle(Scene& sc);             // lock colour, code, genuine / decoy terminals (called by Reset)
    void SubmitCode(Scene& sc);
    void LockOn(Bulb& b);
    void SendNearestDroneTo(Scene& sc, const glm::vec2& at);   // a wrong code sends the nearest drone to the door
    void UpdateDrone(Bulb& b, float dt, const Scene& sc);

    void Init(Scene& sc);
    void Reset(Scene& sc);
    void Update(float dt, Scene& sc, const Input& in, float camYaw);

    // re-evaluate the lit test at the player's current position (also used by the probe tools)
    void EvaluateLighting(const Scene& sc);

    // F key. The GAME decides whether the lamp may be switched on (main.cpp just mirrors flashOn onto the spot light).
    bool ToggleLamp();

    void UpdateScanners(float dt, Scene& sc);
    void ApplyStrike(Scene& sc, const glm::vec2& at);          // scorch mark + destroy nearby cover
    void UpdateTerminals(float dt, Scene& sc, const Input& in);

    std::mt19937 rng;
    std::uint32_t seedUsed = 0;
    bool seedForced = false;

    glm::vec3 SamplePoint() const { return glm::vec3(playerPos.x, SAMPLE_HEIGHT, playerPos.z); }
};
