// A look-ahead bot that plays the whole level: hack the four GENUINE code terminals, type the code at the door, reach the exit.
//
// It may read internal state (which terminals are genuine, the code), but it has no special powers over the rules: every
// action goes through Game::Update exactly like a keyboard would (movement, sprint, hold E, digits, Enter), and it picks its
// next move by simulating 1.2 s of the real game for each of 18 candidate actions (9 directions x walk / sprint) and keeping
// the best one. Treat its times as a LOWER bound for a human.
//
//   cd SkyboxLab
//   g++ -std=c++17 -O2 -I. ../tests/bot.cpp Scene.cpp SceneLoader.cpp Json.cpp LitTest.cpp Game.cpp -o bot
//   ./bot [--seed N] [--levels N] [--verbose]
#include "SceneLoader.h"
#include "Game.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <vector>

static const float YAW = -1.57079632f;
static const int   HORIZON = 36;               // look-ahead frames at 30 fps = 1.2 s: it must be LONGER than the strike delay, or the bot cannot see a strike coming
static const float CELL = 0.5f;
static const int   N = 64;                       // the grid covers -16..16

static int Idx(float x, float z) { int i = (int)((x + 16) / CELL), j = (int)((z + 16) / CELL); if (i < 0 || j < 0 || i >= N || j >= N) return -1; return j * N + i; }
static glm::vec2 CellCentre(int c) { return glm::vec2(-16.0f + (c % N + 0.5f) * CELL, -16.0f + (c / N + 0.5f) * CELL); }

// breadth-first distance (metres) over free floor to a set of seed cells
static std::vector<float> BuildField(const Scene& sc, const glm::vec2& centre, float rMin, float rMax)
{
    std::vector<float> f(N * N, 1e9f);
    std::queue<int> q;
    auto blocked = [&](int c) { glm::vec2 p = CellCentre(c); return PointInsideSolid(sc, p.x, p.y, 0.45f); };
    for (int c = 0; c < N * N; ++c)
    {
        float d = glm::length(CellCentre(c) - centre);
        if (d >= rMin && d <= rMax && !blocked(c)) { f[c] = 0.0f; q.push(c); }
    }
    while (!q.empty())
    {
        int c = q.front(); q.pop();
        int ci = c % N, cj = c / N;
        for (int d = 0; d < 4; ++d)
        {
            int ni = ci + (d == 0) - (d == 1), nj = cj + (d == 2) - (d == 3);
            if (ni < 0 || nj < 0 || ni >= N || nj >= N) continue;
            int n = nj * N + ni;
            if (blocked(n)) continue;
            if (f[n] > f[c] + CELL) { f[n] = f[c] + CELL; q.push(n); }
        }
    }
    return f;
}
static float FieldAt(const std::vector<float>& f, const glm::vec3& p) { int i = Idx(p.x, p.z); return i < 0 ? 1e4f : std::min(f[i], 1e4f); }

#include <map>
#include <string>
struct Result { std::map<std::string,int> locksBy; std::string killer; std::vector<int> goalsLeftIdx; bool escaped = false, lost = false; float time = 0; int lockOns = 0, hacks = 0, wrongCodes = 0, strikes = 0, destroyed = 0; const char* why = ""; };

static Result PlayLevel(const Scene& base, int level, unsigned seed, bool verbose, bool diag = false)
{
    Result res;
    Scene s = base;
    Game g; g.SeedRng(seed);
    g.Init(s);
    g.level = level; g.speedScale = std::pow(s.speedStep, (float)(level - 1));
    g.Reset(s);
    g.state = GameState::Playing;
    g.ToggleLamp();                                  // lamp off: it adds 1.5 m to the noise radius, and a sneaking player does not need it

    // one distance field per genuine terminal, one for the door and one for the exit
    std::vector<int> genuine;
    for (size_t i = 0; i < s.terminals.size(); ++i) if (s.terminals[i].genuine) genuine.push_back((int)i);
    std::vector<std::vector<float>> termField(s.terminals.size());
    for (int ti : genuine) termField[ti] = BuildField(s, s.terminals[ti].pos, 0.6f, 1.45f);      // any free cell within hack range (the grid is 0.5 m, so a ring that is too thin can contain no free cell at all)
    std::vector<float> doorField = BuildField(s, s.exitPos, 0.3f, s.codePuzzle.doorRadius * 0.7f);
    std::vector<float> exitField = BuildField(s, s.exitPos, 0.0f, 0.6f);

    if (verbose) { for (int ti : genuine) std::printf("   field from start to %s (%.1f,%.1f): %.1f m\n", s.terminals[ti].id.c_str(), s.terminals[ti].pos.x, s.terminals[ti].pos.y, FieldAt(termField[ti], g.playerPos)); std::printf("   field start -> door %.1f m\n", FieldAt(doorField, g.playerPos)); }
    const float dt = 1.0f / 30.0f;
    const glm::vec2 dirs[9] = { {0,0},{1,0},{-1,0},{0,1},{0,-1},{.7f,.7f},{.7f,-.7f},{-.7f,.7f},{-.7f,-.7f} };
    glm::vec2 anchor(g.playerPos.x, g.playerPos.z); float anchorT = 0; int wander = 0, wdir = 1; unsigned wr = 12345u + level;
    bool prevLock = false; int prevDestroyedVersion = g.coverVersion;
    std::vector<Input> typing; size_t typingPos = 0;
    float t = 0;

    while (g.state == GameState::Playing && t < 240.0f)
    {
        // ---- what is the goal right now? ----
        int goalTerm = -1; float bestD = 1e9f;
        for (int ti : genuine)
        {
            if (s.terminals[ti].used) continue;
            float d = FieldAt(termField[ti], g.playerPos);
            if (d < bestD) { bestD = d; goalTerm = ti; }
        }
        const std::vector<float>* field = nullptr;
        bool wantHack = false, wantType = false;
        if (goalTerm >= 0)
        {
            field = &termField[goalTerm];
            wantHack = glm::length(glm::vec2(g.playerPos.x, g.playerPos.z) - s.terminals[goalTerm].pos) < s.hackRadius * 0.8f;
        }
        else if (g.doorLocked)
        {
            field = &doorField;
            wantType = glm::length(glm::vec2(g.playerPos.x, g.playerPos.z) - s.exitPos) < s.codePuzzle.doorRadius * 0.85f;
        }
        else field = &exitField;

        // ---- typing the code: one key per frame, only when nothing is about to hit us ----
        if (wantType && typingPos < typing.size())
        {
            Input in = typing[typingPos++];
            g.Update(dt, s, in, YAW); t += dt;
            if (g.message == "WRONG CODE") ++res.wrongCodes;
            continue;
        }
        if (wantType && typing.empty() && g.lockedCount == 0 && g.alarm < 0.5f && g.KeypadActive())
        {
            for (int k = 0; k < 4; ++k) { Input in; in.digit = g.code[k]; typing.push_back(in); }
            Input e; e.enter = true; typing.push_back(e);
            typingPos = 0;
            continue;
        }

        if (verbose && t > 14.0f && t < 14.2f)
        {
            std::printf("   DEBUG goal %s  player (%.2f,%.2f)  field here %.2f | neighbours:", goalTerm >= 0 ? s.terminals[goalTerm].id.c_str() : "-", g.playerPos.x, g.playerPos.z, FieldAt(*field, g.playerPos));
            for (int d = 1; d < 9; ++d) { glm::vec3 q = g.playerPos + glm::vec3(dirs[d].x, 0, dirs[d].y) * 0.5f; std::printf(" (%.1f,%.1f)=%.1f", dirs[d].x, dirs[d].y, FieldAt(*field, q)); }
            std::printf("\n   terminal pos (%.1f,%.1f) dist %.2f hackRadius*0.8 %.2f wantHack %d used %d progress %.2f\n", s.terminals[goalTerm].pos.x, s.terminals[goalTerm].pos.y,
                glm::length(glm::vec2(g.playerPos.x, g.playerPos.z) - s.terminals[goalTerm].pos), s.hackRadius * 0.8f, wantHack ? 1 : 0, s.terminals[goalTerm].used ? 1 : 0, s.terminals[goalTerm].progress);
        }
        // ---- stuck recovery ----
        if (glm::length(glm::vec2(g.playerPos.x, g.playerPos.z) - anchor) > 1.5f || wantHack || wantType) { anchor = glm::vec2(g.playerPos.x, g.playerPos.z); anchorT = t; }
        if (t - anchorT > 4.0f && wander == 0) { wander = 8; wr = wr * 1664525u + 1013904223u; wdir = 1 + (wr >> 16) % 8; anchorT = t; }

        // ---- choose the best action by simulating 1.2 s of the real game for each ----
        // candidates: 9 directions x {walk, sprint, sneak}, plus "stand still and keep quiet" (no E held), which lets the bot
        // wait for a patrol to pass instead of hacking while a drone is coming
        struct Act { int dir; int mode; bool hack; };
        std::vector<Act> acts;
        for (int d = 0; d < 9; ++d) for (int m = 0; m < 3; ++m) acts.push_back({ d, m, wantHack });
        if (wantHack) acts.push_back({ 0, 2, false });
        Act bestAct = { 0, 0, wantHack }; float best = 1e18f;
        for (const Act& ac : acts)
        {
            glm::vec2 dv = dirs[ac.dir];
            Scene s2 = s; Game g2 = g;
            Input in; in.forward = -dv.y; in.right = dv.x; in.run = (ac.mode == 1); in.sneak = (ac.mode == 2); in.hack = ac.hack;
            float acc = 0;
            std::vector<int> prevState(s2.bulbs.size());
            for (size_t bi = 0; bi < s2.bulbs.size(); ++bi) prevState[bi] = (int)s2.bulbs[bi].state;
            for (int k = 0; k < HORIZON && g2.state == GameState::Playing; ++k)
            {
                g2.Update(dt, s2, in, YAW);
                // when a strike lands, reward the SAFETY MARGIN (how far outside the blast the droid is): survive robustly, not by a hair,
                // because the bot re-plans every 0.13 s and a thin margin is easily lost by a change of heart
                for (size_t bi = 0; bi < s2.bulbs.size(); ++bi)
                {
                    int st = (int)s2.bulbs[bi].state;
                    if (prevState[bi] == (int)ScanState::Locked && st == (int)ScanState::Striking && g2.state == GameState::Playing)
                    {
                        float margin = glm::length(glm::vec2(g2.playerPos.x, g2.playerPos.z) - s2.bulbs[bi].aim) - s2.strikeRadius;
                        acc -= 150.0f * glm::clamp(margin, -1.0f, 2.5f);
                    }
                    prevState[bi] = st;
                }
                if (g2.state == GameState::Lost) { acc += 20000.0f; break; }       // a predicted death is always catastrophic, whenever it happens
                if (g2.state == GameState::Won)  { acc -= 3000.0f; break; }
                float p = FieldAt(*field, g2.playerPos);
                p += 150.0f * g2.alarm + 120.0f * g2.lockedCount + (g2.lit ? 4.0f : 0.0f);      // avoid being seen at all: it is far cheaper than escaping a strike
                if (wantHack && goalTerm >= 0) p -= 400.0f * s2.terminals[goalTerm].progress + (s2.terminals[goalTerm].used ? 800.0f : 0.0f);
                acc += p * (k == HORIZON - 1 ? 1.0f : 0.15f);
            }
            if (ac.dir == 0) acc += 0.5f;                  // a slight penalty for standing still
            if (acc < best) { best = acc; bestAct = ac; }
        }
        if (wander > 0) { bestAct = { wdir, 0, wantHack }; --wander; }
        const int bestA = bestAct.dir; const bool bestSprint = (bestAct.mode == 1);

        Input in; in.forward = -dirs[bestA].y; in.right = dirs[bestA].x; in.run = bestSprint; in.sneak = (bestAct.mode == 2); in.hack = bestAct.hack;
        if (diag && g.lockedCount > 0) std::printf("   t=%5.2f LOCKED  droid(%5.2f,%5.2f) action dir(%.1f,%.1f) mode %d hack %d  best score %.0f  nextStrike %.2f\n", t, g.playerPos.x, g.playerPos.z, dirs[bestA].x, dirs[bestA].y, bestAct.mode, bestAct.hack ? 1 : 0, best, g.nextStrike);
        int usedBefore = 0; for (const auto& tt : s.terminals) usedBefore += tt.used ? 1 : 0;
        for (int k = 0; k < 4 && g.state == GameState::Playing; ++k) { g.Update(dt, s, in, YAW); t += dt; }
        int usedAfter = 0; for (const auto& tt : s.terminals) usedAfter += tt.used ? 1 : 0;
        res.hacks += usedAfter - usedBefore;
        {   // bookkeeping: which light locked on how often, and which one finally killed the droid
            static std::vector<int> lastSt; if (lastSt.size() != s.bulbs.size() || t < 0.2f) lastSt.assign(s.bulbs.size(), 0);
            for (size_t bi = 0; bi < s.bulbs.size(); ++bi)
            {
                int st = (int)s.bulbs[bi].state;
                if (st == 1 && lastSt[bi] != 1) { res.locksBy[s.bulbs[bi].name]++; res.killer = s.bulbs[bi].name; }
                lastSt[bi] = st;
            }
        }
        if (diag)
        {
            static std::vector<int> last; if (last.size() != s.bulbs.size() || t < 0.2f) last.assign(s.bulbs.size(), 0);
            for (size_t bi = 0; bi < s.bulbs.size(); ++bi)
            {
                int st = (int)s.bulbs[bi].state;
                if (st != last[bi])
                {
                    const char* nm[] = { "Scanning", "Locked", "Striking", "Cooling" };
                    std::printf("   t=%5.1f %-8s -> %-8s droid(%5.1f,%5.1f) aim(%5.1f,%5.1f) dist %.2f speedMult %.2f oil %d lit %d\n", t, s.bulbs[bi].name.c_str(), nm[st],
                        g.playerPos.x, g.playerPos.z, s.bulbs[bi].aim.x, s.bulbs[bi].aim.y, glm::length(glm::vec2(g.playerPos.x, g.playerPos.z) - s.bulbs[bi].aim), g.speedMult, g.onOil ? 1 : 0, g.lit ? 1 : 0);
                    last[bi] = st;
                }
            }
        }
        if (g.lockedCount > 0 && !prevLock) ++res.lockOns;
        prevLock = g.lockedCount > 0;
        if (g.coverVersion != prevDestroyedVersion) { ++res.destroyed; prevDestroyedVersion = g.coverVersion; }
        if (verbose && ((int)(t * 10) % 100) < 4) std::printf("   t=%5.1f pos(%5.1f,%5.1f) alarm %.2f locked %d goal %s hacks %d locked-door %d mult %.2f\n", t, g.playerPos.x, g.playerPos.z, g.alarm, g.lockedCount,
            goalTerm >= 0 ? "terminal" : (g.doorLocked ? "door" : "exit"), res.hacks, g.doorLocked ? 1 : 0, g.speedMult);
    }
    res.time = t; res.escaped = (g.state == GameState::Won); res.lost = (g.state == GameState::Lost);
    res.why = res.escaped ? "" : res.lost ? "incinerated" : "timeout";
    if (!res.escaped) { std::printf("   locks by light:"); for (auto& kv : res.locksBy) std::printf(" %s=%d", kv.first.c_str(), kv.second); std::printf(" | last locked: %s\n   genuine terminals still to hack:", res.killer.c_str()); for (int ti : genuine) if (!s.terminals[ti].used) std::printf(" %s(%.1f,%.1f)", s.terminals[ti].id.c_str(), s.terminals[ti].pos.x, s.terminals[ti].pos.y); std::printf("\n"); }
    if (!res.escaped) std::printf("   ended at (%.1f,%.1f) doorLocked=%d hacks=%d goalsLeft=%d\n", g.playerPos.x, g.playerPos.z, g.doorLocked ? 1 : 0, res.hacks, (int)(genuine.size()) - res.hacks);
    return res;
}

int main(int argc, char** argv)
{
    unsigned seed = 1; int levels = 4, only = 0; bool verbose = false, diag = false;
    std::vector<std::pair<std::string, float>> overrides;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = (unsigned)std::strtoul(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--levels") && i + 1 < argc) levels = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--verbose")) verbose = true;
        else if (!std::strcmp(argv[i], "--diag")) diag = true;
        else if (!std::strcmp(argv[i], "--only") && i + 1 < argc) only = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--set") && i + 1 < argc)           // --set strike=1.5   (ablation: relax ONE rule and see what changes)
        {
            std::string kv = argv[++i]; size_t eq = kv.find('=');
            if (eq != std::string::npos) overrides.push_back({ kv.substr(0, eq), (float)std::atof(kv.c_str() + eq + 1) });
        }
    }
    Scene sc; if (!SceneLoader::Load("level01.json", sc)) { std::printf("level failed to load\n"); return 2; }
    for (const auto& ov : overrides)
    {
        if (ov.first == "strike") sc.strikeDelay = sc.minStrikeDelay = ov.second;           // strike delay (and its floor)
        else if (ov.first == "hack") sc.codePuzzle.hackTime = ov.second;                    // seconds to hack a code terminal
        else if (ov.first == "noise") { sc.noiseWalk *= ov.second; sc.noiseSprint *= ov.second; sc.noiseHacking *= ov.second; sc.noiseSneak *= ov.second; sc.noiseStill *= ov.second; sc.noiseLampBonus *= ov.second; }
        else if (ov.first == "lit") sc.litSpeedMult = ov.second;                            // slowdown while scanned
        else if (ov.first == "nhack") sc.noiseHacking = ov.second;                          // individual noise radii (metres)
        else if (ov.first == "nwalk") sc.noiseWalk = ov.second;
        else if (ov.first == "nsprint") sc.noiseSprint = ov.second;
        else if (ov.first == "nlamp") sc.noiseLampBonus = ov.second;
        else if (ov.first == "minstrike") sc.minStrikeDelay = ov.second;                    // floor of the per-level strike delay
        else if (ov.first == "step") sc.speedStep = ov.second;                              // how much faster each level is
        else if (ov.first == "fill") sc.alarmFill = ov.second;                              // how fast detection fills
        else if (ov.first == "droneR") { for (auto& b : sc.bulbs) if (!b.scanner) b.diffuse *= ov.second; }   // drone brightness (reach)
        else std::printf("unknown override %s\n", ov.first.c_str());
    }
    std::printf("bot: seed %u, strike delay %.2f s, %d code terminals\n", seed, sc.strikeDelay, (int)sc.terminals.size());
    int wins = 0;
    for (int level = 1; level <= levels; ++level)
    {
        if (only && level != only) continue;
        Result r = PlayLevel(sc, level, seed, verbose, diag);
        std::printf("level %d: %s at t=%.1fs | hacks %d | wrong codes %d | lock-ons endured %d | cover destroyed %d%s%s\n", level,
            r.escaped ? "ESCAPED" : (r.lost ? "INCINERATED" : "TIMEOUT"), r.time, r.hacks, r.wrongCodes, r.lockOns, r.destroyed, r.escaped ? "" : " | ", r.why);
        wins += r.escaped ? 1 : 0;
    }
    std::printf("%d of %d levels escaped\n", wins, levels);
    return wins == levels ? 0 : 1;
}
