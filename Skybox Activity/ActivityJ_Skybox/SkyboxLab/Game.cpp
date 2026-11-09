#include "Game.h"
#include <cmath>
#include <algorithm>
#include <chrono>

Game::Game()
{
    // seeded from the clock unless SeedRng() is called (tests and the bot force a seed)
    seedUsed = (std::uint32_t)std::chrono::steady_clock::now().time_since_epoch().count();
    rng.seed(seedUsed);
}

void Game::SeedRng(std::uint32_t seed)
{
    seedUsed = seed;
    seedForced = true;
    rng.seed(seed);
}

bool Game::KeypadActive() const
{
    return state == GameState::Playing && doorLocked && glm::length(glm::vec2(playerPos.x, playerPos.z) - doorPos) < doorRadius;
}

void Game::LockOn(Bulb& b)
{
    b.state = ScanState::Locked; b.stateTime = 0.0f;
    if (!b.scanner) b.aim = glm::vec2(playerPos.x, playerPos.z);     // a drone has no cone: its strike point starts on the droid
}

// A new puzzle every attempt: one lock colour, a 4-digit code, and the code terminals shuffled into
//   4 GENUINE terminals (slot 0..3, the lock colour, the true digit) and any number of DECOYS
//   (random slot, a digit that is NOT the true digit for that slot, a colour that is NOT the lock colour).
void Game::AssignPuzzle(Scene& sc)
{
    entered.clear();
    for (auto& n : notebook) n.clear();
    bigMessage.clear(); bigMessageTime = 0.0f;
    doorPos = sc.exitPos;
    doorRadius = sc.codePuzzle.doorRadius;
    for (auto& t : sc.terminals) { t.codeSlot = -1; t.digit = -1; t.colourIdx = -1; t.genuine = false; t.used = false; }

    doorLocked = sc.puzzleEnabled;
    lockColour = -1;
    if (!sc.puzzleEnabled) return;

    const int P = (int)sc.codePuzzle.palette.size();
    lockColour = (int)(rng() % (unsigned)P);
    for (int i = 0; i < 4; ++i) code[i] = (int)(rng() % 10u);

    std::vector<int> terms;
    for (size_t i = 0; i < sc.terminals.size(); ++i) if (sc.terminals[i].kind == Terminal::Kind::Code) terms.push_back((int)i);
    std::shuffle(terms.begin(), terms.end(), rng);

    std::vector<int> bag;                                   // decoy colours: every non-lock colour, dealt out in shuffled rounds so decoys differ
    auto refill = [&]() { bag.clear(); for (int c = 0; c < P; ++c) if (c != lockColour) bag.push_back(c); std::shuffle(bag.begin(), bag.end(), rng); };
    refill();

    for (size_t k = 0; k < terms.size(); ++k)
    {
        Terminal& t = sc.terminals[terms[k]];
        if (k < 4)
        {
            t.codeSlot = (int)k; t.digit = code[k]; t.colourIdx = lockColour; t.genuine = true;
        }
        else
        {
            t.codeSlot = (int)(rng() % 4u);
            int d = (int)(rng() % 9u);                       // 0..8, then skip the true digit: uniform over the other 9
            if (d >= code[t.codeSlot]) ++d;
            t.digit = d;
            if (bag.empty()) refill();
            t.colourIdx = bag.back(); bag.pop_back();
            t.genuine = false;
        }
    }
}

void Game::SendNearestDroneTo(Scene& sc, const glm::vec2& at)
{
    Bulb* best = nullptr; float bestD = 1e30f;
    for (auto& b : sc.bulbs)
    {
        if (b.scanner || b.state != ScanState::Scanning) continue;      // only a drone that is free to fly
        float d = glm::length(glm::vec2(b.position.x, b.position.z) - at);
        if (d < bestD) { bestD = d; best = &b; }
    }
    if (!best) return;
    best->mode = BulbMode::Investigate;
    best->investigateTarget = glm::vec3(at.x, best->position.y, at.y);
    best->hoverTime = 0.0f;
}

// One step of a drone that is in state Scanning:  Patrol -> (noise) -> Investigate -> (hover) -> Return -> Patrol
void Game::UpdateDrone(Bulb& b, float dt, const Scene& sc)
{
    const glm::vec2 me(playerPos.x, playerPos.z);
    const float horiz = glm::length(glm::vec2(b.position.x, b.position.z) - me);

    switch (b.mode)
    {
    case BulbMode::Patrol:
        b.Advance(b.speed * speedScale * dt);
        if (horiz < noiseRadius)                                  // the droid is within earshot: go and look
        {
            b.mode = BulbMode::Investigate;
            b.investigateTarget = glm::vec3(me.x, b.position.y, me.y);       // keep the patrol height
            b.hoverTime = 0.0f;
        }
        break;

    case BulbMode::Investigate:
    {
        const glm::vec2 here(b.position.x, b.position.z);
        const float toTarget = glm::length(glm::vec2(b.investigateTarget.x, b.investigateTarget.z) - here);
        if (toTarget <= sc.droneArriveDistance)                  // close enough: hover, then head home
        {
            b.hoverTime += dt;
            if (b.hoverTime >= sc.droneHoverTime) b.mode = BulbMode::Return;
        }
        else
        {
            if (horiz < noiseRadius) b.investigateTarget = glm::vec3(me.x, b.position.y, me.y);   // still hearing it: follow the noise
            b.MoveToward(b.investigateTarget, b.speed * speedScale * sc.droneInvestigateSpeedMult, dt);
        }
        break;
    }

    case BulbMode::Return:
    {
        float sNear = b.NearestPathS();
        if (b.MoveToward(b.PathPoint(sNear), b.speed * speedScale, dt)) { b.s = sNear; b.mode = BulbMode::Patrol; b.Place(); }
        break;
    }
    }
}

void Game::SubmitCode(Scene& sc)
{
    bool right = (entered.size() == 4);
    for (int i = 0; right && i < 4; ++i) if (entered[i] != code[i]) right = false;
    entered.clear();
    if (right)
    {
        doorLocked = false;
        message = "DOOR UNLOCKED"; messageTime = 2.5f;
        return;
    }
    message = "WRONG CODE"; messageTime = 2.5f;
    SendNearestDroneTo(sc, sc.exitPos);                      // the nearest drone goes to investigate the door
    // the alarm trips: every light that is still scanning gets suspicious (>= 1 locks it on straight away)
    for (auto& b : sc.bulbs)
    {
        if (b.state != ScanState::Scanning) continue;
        b.detect = std::min(1.0f, b.detect + sc.codePuzzle.wrongPenalty);
        if (b.detect >= 1.0f) LockOn(b);
    }
}

void Game::Init(Scene& sc)
{
    flashOn = sc.flashOn;
    level = 1;
    speedScale = 1.0f;
    Reset(sc);
    state = GameState::Title;
}

bool Game::ToggleLamp()
{
    if (flashOn) { flashOn = false; return true; }
    if (lampLockout || battery <= 0.0f) return false;      // flat battery: refuse
    flashOn = true;
    return true;
}

void Game::Reset(Scene& sc)
{
    playerPos = glm::vec3(sc.playerStart.x, 0.0f, sc.playerStart.y);
    facing = glm::normalize(glm::vec2(sc.exitPos.x - sc.playerStart.x, sc.exitPos.y - sc.playerStart.y));
    alarm = 0.0f;
    elapsed = 0.0f;
    onOil = false; speedMult = 1.0f; hacking = false; noiseRadius = noiseTarget = sc.noiseStill;
    flashOn = sc.flashOn; battery = 1.0f; lampLockout = false; lampPower = 1.0f;
    lockedCount = 0; nextStrike = -1.0f; nearTerminal = -1; hackProgress = 0.0f;
    message.clear(); messageTime = 0.0f;
    for (auto& b : sc.bulbs) b.Reset();
    {   // put every destroyed crate / barrier back
        bool changed = false;
        for (auto& o : sc.objects)
            if (o.destroyed) { o.destroyed = false; o.solid = o.origSolid; o.castShadow = o.origCast; changed = true; }
        scorchMarks.clear();
        if (changed) ++coverVersion;
    }
    for (auto& t : sc.terminals) { t.cooldown = 0.0f; t.progress = 0.0f; t.uses = 0; }
    AssignPuzzle(sc);
    EvaluateLighting(sc);
}

void Game::EvaluateLighting(const Scene& sc)
{
    LitResult r = ComputeIllumination(sc, SamplePoint(), lineOfSightEnabled);
    litResult = r;
    illum = r.illum;
    visMask = r.visMask;
    lit = illum >= sc.litThreshold;
}

// circle (player) against axis-aligned boxes taken from the scene file
static void ResolveCollisions(glm::vec3& p, const Scene& sc)
{
    for (int iter = 0; iter < 3; ++iter)
    {
        for (const auto& o : sc.objects)
        {
            if (!o.solid) continue;
            float cx = glm::clamp(p.x, o.minB.x, o.maxB.x);
            float cz = glm::clamp(p.z, o.minB.z, o.maxB.z);
            float dx = p.x - cx, dz = p.z - cz;
            float d2 = dx * dx + dz * dz;
            if (d2 >= PLAYER_RADIUS * PLAYER_RADIUS) continue;

            if (d2 > 1e-10f)
            {
                float d = std::sqrt(d2);
                float push = (PLAYER_RADIUS - d) / d;
                p.x += dx * push;
                p.z += dz * push;
            }
            else
            {
                // centre is inside the box: leave through the nearest side
                float l = p.x - o.minB.x, r = o.maxB.x - p.x, t = p.z - o.minB.z, b = o.maxB.z - p.z;
                float m = std::min(std::min(l, r), std::min(t, b));
                if (m == l)      p.x = o.minB.x - PLAYER_RADIUS;
                else if (m == r) p.x = o.maxB.x + PLAYER_RADIUS;
                else if (m == t) p.z = o.minB.z - PLAYER_RADIUS;
                else             p.z = o.maxB.z + PLAYER_RADIUS;
            }
        }
    }
    // fail-safe so the player can never leave the level even if the walls are edited away
    p.x = glm::clamp(p.x, sc.bounds.x + PLAYER_RADIUS, sc.bounds.z - PLAYER_RADIUS);
    p.z = glm::clamp(p.z, sc.bounds.y + PLAYER_RADIUS, sc.bounds.w - PLAYER_RADIUS);
}

void Game::Update(float dt, Scene& sc, const Input& in, float camYaw)
{
    switch (state)
    {
    case GameState::Title:
        for (auto& b : sc.bulbs)                                           // attract mode
        {
            if (b.scanner) b.SweepAim(dt, b.speed * speedScale);
            else           b.Advance(b.speed * speedScale * dt);
        }
        if (in.enter) { Reset(sc); state = GameState::Playing; }
        return;

    case GameState::Paused:
        if (in.pause) state = GameState::Playing;
        return;

    case GameState::Won:
    case GameState::Lost:
        if (in.restart || in.enter)
        {
            if (state == GameState::Won) { ++level; speedScale *= sc.speedStep; }
            Reset(sc);
            state = GameState::Playing;
        }
        return;

    case GameState::Playing:
        break;
    }

    // ---------------------------- PLAYING ----------------------------
    if (in.restart) { Reset(sc); return; }
    if (in.pause)   { state = GameState::Paused; return; }

    elapsed += dt;
    if (messageTime > 0.0f) messageTime -= dt;
    if (bigMessageTime > 0.0f) bigMessageTime -= dt;

    // ---- lamp battery: drains while on, recharges while off ----
    if (flashOn)
    {
        battery -= sc.lampDrain * dt;
        if (battery <= 0.0f) { battery = 0.0f; flashOn = false; lampLockout = true; message = "LAMP BATTERY EMPTY"; messageTime = 2.0f; }
    }
    else
    {
        battery = std::min(1.0f, battery + sc.lampRecharge * dt);
        if (lampLockout && battery >= sc.lampMinOn) lampLockout = false;
    }
    lampPower = 1.0f;
    if (flashOn && battery < sc.lampFlickerBelow)            // low battery: the lamp stutters and dims
    {
        float dim = std::max(0.3f, battery / std::max(sc.lampFlickerBelow, 1e-3f));
        bool on = (std::sin(elapsed * 43.0f) * std::sin(elapsed * 17.0f)) > -0.15f;
        lampPower = on ? dim : 0.12f * dim;
    }

    // movement relative to the camera, scaled by dt (frame-rate independent, B4)
    glm::vec3 camFwd(std::cos(camYaw), 0.0f, std::sin(camYaw));
    glm::vec3 camRight(-std::sin(camYaw), 0.0f, std::cos(camYaw));
    glm::vec3 wish = camFwd * in.forward + camRight * in.right;
    // ---- one speed multiplier: sprint / sneak, scanned, oil ----
    //   Shift = sprint, Left Ctrl = sneak (sneak wins if both are held).
    //   Being lit slows the droid ONLY while no light is locked on: a strike lands 0.8 s after lock-on, so a
    //   lit-and-slowed droid could never get out of the blast radius. The slowdown is a penalty for being seen,
    //   not a second way to die. Conveyors are applied separately and are not affected.
    float mult = 1.0f;
    if (in.sneak)      mult = sc.sneakMult;
    else if (in.run)   mult = sc.sprintMult;
    if (lit && lockedCount == 0) mult *= sc.litSpeedMult;
    onOil = false;
    for (const auto& p : sc.oilPuddles)
        if (glm::length(glm::vec2(playerPos.x, playerPos.z) - p.pos) < p.radius) { onOil = true; break; }
    if (onOil) mult *= sc.oilSpeedMult;
    mult = std::max(mult, sc.minSpeedMult);
    speedMult = mult;

    if (glm::length(wish) > 1e-4f)
    {
        wish = glm::normalize(wish);
        playerPos += wish * PLAYER_SPEED * mult * dt;
        glm::vec2 want(wish.x, wish.z);
        glm::vec2 f = glm::mix(facing, want, 0.3f);
        if (glm::length(f) > 1e-4f) facing = glm::normalize(f);
    }

    // conveyor belts carry the droid (same data as the drawn belts)
    onBelt = false;
    for (const auto& c : sc.conveyors)
        if (std::fabs(playerPos.x - c.centre.x) <= c.size.x * 0.5f && std::fabs(playerPos.z - c.centre.y) <= c.size.y * 0.5f)
        {
            playerPos.x += c.dir.x * c.speed * dt;
            playerPos.z += c.dir.y * c.speed * dt;
            onBelt = true;
        }
    ResolveCollisions(playerPos, sc);

    // THE MECHANIC: every scanner is driven by the same illumination test the renderer must agree with
    EvaluateLighting(sc);
    UpdateTerminals(dt, sc, in);

    // ---- noise: how far a drone can hear the droid. Scanners are fixed mounts and ignore it. ----
    {
        const bool moving = glm::length(wish) > 1e-4f;
        float target = sc.noiseStill;
        if (moving) target = in.sneak ? sc.noiseSneak : (in.run ? sc.noiseSprint : sc.noiseWalk);
        if (hacking) target = std::max(target, sc.noiseHacking);
        if (flashOn) target += sc.noiseLampBonus;
        noiseTarget = target;
        noiseRadius += (target - noiseRadius) * std::min(1.0f, sc.noiseSmoothing * dt);     // smoothed so the HUD bar does not flicker
    }

    // ---- the door keypad: digits type, Backspace deletes, Enter submits. The world keeps running (no extra GameState). ----
    if (KeypadActive())
    {
        if (in.digit >= 0 && in.digit <= 9 && entered.size() < 4) entered.push_back(in.digit);
        if (in.backspace && !entered.empty()) entered.pop_back();
        if (in.enter)
        {
            if (entered.size() == 4) SubmitCode(sc);
            else { message = "ENTER 4 DIGITS"; messageTime = 1.5f; }
        }
    }
    else entered.clear();

    UpdateScanners(dt, sc);

    float dx = playerPos.x - sc.exitPos.x, dz = playerPos.z - sc.exitPos.y;
    if (state != GameState::Playing) { finalTime = elapsed; return; }      // a strike just landed
    if (std::sqrt(dx * dx + dz * dz) < sc.exitRadius && !doorLocked)       // the exit only counts once the door is unlocked
    {
        finalTime = elapsed;
        state = GameState::Won;
    }
}

// 2D distance from a point to an axis-aligned box on the floor (0 when the point is inside it)
static float DistPointBox2D(const glm::vec2& p, const glm::vec3& mn, const glm::vec3& mx)
{
    float dx = std::max(std::max(mn.x - p.x, 0.0f), p.x - mx.x);
    float dz = std::max(std::max(mn.z - p.y, 0.0f), p.y - mx.z);
    return std::sqrt(dx * dx + dz * dz);
}

void Game::ApplyStrike(Scene& sc, const glm::vec2& at)
{
    scorchMarks.push_back(at);
    if (scorchMarks.size() > 40) scorchMarks.erase(scorchMarks.begin());

    bool any = false;
    for (auto& o : sc.objects)
    {
        if (!o.destructible || o.destroyed) continue;
        if (DistPointBox2D(at, o.minB, o.maxB) < sc.strikeRadius)
        {
            o.destroyed = true; o.solid = false; o.castShadow = false;     // collision, lit-test, shadow maps and drawing all follow these flags
            any = true;
        }
    }
    if (any) { ++coverVersion; message = "COVER DESTROYED"; messageTime = 2.0f; }
}

static const float AIM_TRACK_SPEED = 5.5f;     // how fast a locked scanner chases the droid (m/s) - faster than the droid walks

void Game::UpdateScanners(float dt, Scene& sc)
{
    const float fillScale = 1.0f + 0.15f * (float)(level - 1);
    // strike delay shortens with the level but never drops below gameplay.min_strike_delay
    const float delay = std::max(sc.minStrikeDelay, sc.strikeDelay / (1.0f + 0.08f * (float)(level - 1)));
    const glm::vec2 me(playerPos.x, playerPos.z);

    alarm = 0.0f; lockedCount = 0; nextStrike = -1.0f;

    for (size_t i = 0; i < sc.bulbs.size(); ++i)
    {
        Bulb& b = sc.bulbs[i];          // drones and scanners share one state machine
        b.stateTime += dt;

        switch (b.state)
        {
        case ScanState::Scanning:
        {
            b.power = 1.0f;
            if (b.scanner) b.SweepAim(dt, b.speed * speedScale);     // scanner: steer the cone along its route
            else           UpdateDrone(b, dt, sc);                   // drone: patrol, or investigate the noise
            float c = litResult.per[i];                        // this scanner's own contribution at the droid
            if (c >= sc.litThreshold) b.detect += sc.alarmFill * fillScale * (0.7f + glm::min(c, 1.0f)) * dt;
            else                      b.detect -= sc.alarmDrain * dt;
            b.detect = glm::clamp(b.detect, 0.0f, 1.0f);
            b.colour = glm::mix(b.baseColour, glm::vec3(1.0f, 0.35f, 0.05f), b.detect);
            if (b.detect >= 1.0f) LockOn(b);
            break;
        }
        case ScanState::Locked:
        {
            b.power = 1.0f;
            // chase the droid for the first part of the countdown, then commit to a fixed spot
            if (b.stateTime < 0.55f * delay)
            {
                glm::vec2 d = me - b.aim;
                float len = glm::length(d), step = AIM_TRACK_SPEED * dt;
                b.aim = (len <= step) ? me : b.aim + d / len * step;
            }
            float pulse = 0.65f + 0.35f * std::sin(b.stateTime * 20.0f);
            b.colour = glm::vec3(1.0f, 0.06f, 0.04f) * pulse;
            b.detect = 1.0f;
            ++lockedCount;
            float left = delay - b.stateTime;
            if (nextStrike < 0.0f || left < nextStrike) nextStrike = left;
            if (b.stateTime >= delay)
            {
                b.state = ScanState::Striking; b.stateTime = 0.0f;
                // THE STRIKE: a disc of radius strikeRadius around where the beam was frozen
                ApplyStrike(sc, b.aim);
                if (glm::length(me - b.aim) < sc.strikeRadius) { state = GameState::Lost; message = "INCINERATED"; }
            }
            break;
        }
        case ScanState::Striking:
            b.power = 0.0f;
            b.colour = glm::vec3(1.0f, 0.4f, 0.1f);
            if (b.stateTime >= 0.6f) { b.state = ScanState::Cooling; b.stateTime = 0.0f; }
            break;

        case ScanState::Cooling:
            b.power = 0.0f;
            b.colour = glm::vec3(0.25f, 0.2f, 0.1f);
            if (b.stateTime >= sc.strikeCooldown)
            {
                b.state = ScanState::Scanning; b.stateTime = 0.0f; b.detect = 0.0f; b.power = 1.0f; b.colour = b.baseColour;
            }
            break;
        }
        alarm = std::max(alarm, b.detect);
    }
}

void Game::UpdateTerminals(float dt, Scene& sc, const Input& in)
{
    for (auto& t : sc.terminals) if (t.cooldown > 0.0f) t.cooldown = std::max(0.0f, t.cooldown - dt);

    // nearest terminal within hacking range
    int best = -1; float bestD = sc.hackRadius;
    for (size_t i = 0; i < sc.terminals.size(); ++i)
    {
        float d = glm::length(glm::vec2(playerPos.x, playerPos.z) - sc.terminals[i].pos);
        if (d < bestD) { bestD = d; best = (int)i; }
    }
    // a terminal we walked away from forgets its progress
    for (size_t i = 0; i < sc.terminals.size(); ++i)
        if ((int)i != best) sc.terminals[i].progress = std::max(0.0f, sc.terminals[i].progress - 2.0f * dt);

    nearTerminal = best;
    hackProgress = 0.0f;
    hacking = false;
    if (best < 0) return;
    Terminal& t = sc.terminals[best];
    const bool isCode = (t.kind == Terminal::Kind::Code);
    if (!isCode && t.cooldown > 0.0f) return;                    // route terminals reboot after a hack
    if (isCode && t.used) return;                                // a code terminal gives its digit once

    const float need = isCode ? sc.codePuzzle.hackTime : sc.hackTime;
    if (in.hack) { t.progress += dt / need; hacking = true; }
    else         t.progress = std::max(0.0f, t.progress - 2.0f * dt);
    hackProgress = glm::min(t.progress, 1.0f);

    if (t.progress >= 1.0f)
    {
        t.progress = 0.0f; hackProgress = 0.0f;
        if (isCode)
        {
            t.used = true;
            notebook[t.codeSlot].push_back({ t.digit, t.colourIdx });
            bigMessage = "SLOT " + std::to_string(t.codeSlot + 1) + " = " + std::to_string(t.digit);
            bigMessageTime = 3.0f;
        }
        else
        {
            t.cooldown = sc.hackCooldown; ++t.uses;
            for (int li : t.links) sc.bulbs[li].SwitchRoute();
            message = "PATROL ROUTES REWRITTEN"; messageTime = 2.5f;
        }
    }
}
