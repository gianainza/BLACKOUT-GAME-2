#include "Game.h"
#include <cmath>
#include <algorithm>

void Game::Init(Scene& sc)
{
    flashOn = sc.flashOn;
    level = 1;
    speedScale = 1.0f;
    Reset(sc);
    state = GameState::Title;
}

void Game::Reset(Scene& sc)
{
    playerPos = glm::vec3(sc.playerStart.x, 0.0f, sc.playerStart.y);
    facing = glm::normalize(glm::vec2(sc.exitPos.x - sc.playerStart.x, sc.exitPos.y - sc.playerStart.y));
    walkPhase = 0.0f;
    moveBlend = 0.0f;
    alarm = 0.0f;
    elapsed = 0.0f;
    lockedCount = 0; nextStrike = -1.0f; nearTerminal = -1; hackProgress = 0.0f;
    message.clear(); messageTime = 0.0f;
    for (auto& b : sc.bulbs) b.Reset();
    for (auto& t : sc.terminals) { t.cooldown = 0.0f; t.progress = 0.0f; t.uses = 0; }
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
    glm::vec3 prevPos = playerPos;

    // movement relative to the camera, scaled by dt (frame-rate independent, B4)
    glm::vec3 camFwd(std::cos(camYaw), 0.0f, std::sin(camYaw));
    glm::vec3 camRight(-std::sin(camYaw), 0.0f, std::cos(camYaw));
    glm::vec3 wish = camFwd * in.forward + camRight * in.right;
    if (glm::length(wish) > 1e-4f)
    {
        wish = glm::normalize(wish);
        playerPos += wish * PLAYER_SPEED * dt;
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

    glm::vec2 step(playerPos.x - prevPos.x, playerPos.z - prevPos.z);
    float moved = glm::length(step);
    float speedNorm = glm::clamp(moved / (PLAYER_SPEED * glm::max(dt, 1e-4f)), 0.0f, 1.0f);
    moveBlend = glm::mix(moveBlend, speedNorm, glm::clamp(dt * 8.0f, 0.0f, 1.0f));
    walkPhase += moved * 6.0f;

    // THE MECHANIC: every scanner is driven by the same illumination test the renderer must agree with
    EvaluateLighting(sc);
    UpdateTerminals(dt, sc, in);
    UpdateScanners(dt, sc);

    float dx = playerPos.x - sc.exitPos.x, dz = playerPos.z - sc.exitPos.y;
    if (state != GameState::Playing) { finalTime = elapsed; return; }      // a strike just landed
    if (std::sqrt(dx * dx + dz * dz) < sc.exitRadius)
    {
        finalTime = elapsed;
        state = GameState::Won;
    }
}

static const float AIM_TRACK_SPEED = 5.5f;     // how fast a locked scanner chases the droid (m/s) - faster than the droid walks

void Game::UpdateScanners(float dt, Scene& sc)
{
    const float fillScale = 1.0f + 0.15f * (float)(level - 1);
    const float delay = sc.strikeDelay / (1.0f + 0.08f * (float)(level - 1));
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
            else           b.Advance(b.speed * speedScale * dt);     // drone: fly its patrol loop
            float c = litResult.per[i];                        // this scanner's own contribution at the droid
            if (c >= sc.litThreshold) b.detect += sc.alarmFill * fillScale * (0.7f + glm::min(c, 1.0f)) * dt;
            else                      b.detect -= sc.alarmDrain * dt;
            b.detect = glm::clamp(b.detect, 0.0f, 1.0f);
            b.colour = glm::mix(b.baseColour, glm::vec3(1.0f, 0.35f, 0.05f), b.detect);
            if (b.detect >= 1.0f)
            {
                b.state = ScanState::Locked; b.stateTime = 0.0f;
                if (!b.scanner) b.aim = me;          // a drone has no cone: its strike point starts on the droid
            }
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

    // nearest ready terminal within hacking range
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
    if (best < 0) return;
    Terminal& t = sc.terminals[best];
    if (t.cooldown > 0.0f) return;

    if (in.hack) t.progress += dt / sc.hackTime;
    else         t.progress = std::max(0.0f, t.progress - 2.0f * dt);
    hackProgress = glm::min(t.progress, 1.0f);

    if (t.progress >= 1.0f)
    {
        t.progress = 0.0f; hackProgress = 0.0f;
        t.cooldown = sc.hackCooldown; ++t.uses;
        for (int li : t.links) sc.bulbs[li].SwitchRoute();
        message = "PATROL ROUTES REWRITTEN"; messageTime = 2.5f;
    }
}
