// A look-ahead bot: BFS distance field to the exit + 0.8 s simulated rollouts of 9 actions. It cannot see the future
// of player-less physics any better than a human can plan, but it knows the lights exactly, so treat its time as a
// LOWER bound for a human. Used to prove the level is beatable.
//   cd SkyboxLab && g++ -std=c++17 -O2 -I. ../tests/bot.cpp Scene.cpp SceneLoader.cpp Json.cpp LitTest.cpp Game.cpp -o bot && ./bot
#include "SceneLoader.h"
#include "Game.h"
#include <cstdio>
#include <queue>
#include <vector>
static const float YAW = -1.57079632f;
static const float CELL = 0.5f; static const int N = 64; // covers -16..16
static std::vector<float> field;
static int Idx(float x, float z) { int i = (int)((x + 16) / CELL), j = (int)((z + 16) / CELL); if (i<0||j<0||i>=N||j>=N) return -1; return j * N + i; }
static void BuildField(const Scene& sc) {
    field.assign(N * N, 1e9f); std::queue<int> q;
    auto blocked = [&](int i, int j) { float x = -16 + (i + .5f) * CELL, z = -16 + (j + .5f) * CELL; return PointInsideSolid(sc, x, z, 0.45f); };
    int s = Idx(sc.exitPos.x, sc.exitPos.y); field[s] = 0; q.push(s);
    while (!q.empty()) { int c = q.front(); q.pop(); int ci = c % N, cj = c / N;
        for (int d = 0; d < 4; ++d) { int ni = ci + (d==0) - (d==1), nj = cj + (d==2) - (d==3);
            if (ni<0||nj<0||ni>=N||nj>=N||blocked(ni,nj)) continue; int n = nj*N+ni;
            if (field[n] > field[c] + CELL) { field[n] = field[c] + CELL; q.push(n); } } }
}
static float Score(const Game& g, const Scene& sc) {
    int i = Idx(g.playerPos.x, g.playerPos.z); float f = (i < 0) ? 1e4f : std::min(field[i], 1e4f);
    float p = f;
    if (g.state == GameState::Lost) p += 5000;
    if (g.state == GameState::Won) p -= 1000;
    p += 40.0f * g.alarm + 120.0f * g.lockedCount;
    return p;
}
int main() {
    Scene sc; SceneLoader::Load("level01.json", sc);
    BuildField(sc);
    for (int level = 1; level <= 4; ++level) {
        Scene s = sc; Game g; g.Init(s); g.level = level; g.speedScale = std::pow(sc.speedStep, level - 1); g.Reset(s); g.state = GameState::Playing;
        const float dt = 1.0f / 30.0f; float t = 0; int locks = 0; bool prevLock = false;
        const float dirs[9][2] = {{0,0},{1,0},{-1,0},{0,1},{0,-1},{.7f,.7f},{.7f,-.7f},{-.7f,.7f},{-.7f,-.7f}};
        glm::vec2 anchor(g.playerPos.x, g.playerPos.z); float anchorT = 0; int wander = 0; int wdir = 1; unsigned rng = 12345 + level;
        while (g.state == GameState::Playing && t < 150) {
            if (glm::length(glm::vec2(g.playerPos.x, g.playerPos.z) - anchor) > 1.5f) { anchor = glm::vec2(g.playerPos.x, g.playerPos.z); anchorT = t; }
            if (t - anchorT > 4.0f && wander == 0) { wander = 8; rng = rng * 1664525u + 1013904223u; wdir = 1 + (rng >> 16) % 8; anchorT = t; }
            int bestA = 0; float best = 1e18f;
            for (int a = 0; a < 9; ++a) {
                Scene s2 = s; Game g2 = g; Input in; in.forward = -dirs[a][1]; in.right = dirs[a][0];
                float acc = 0; for (int k = 0; k < 24 && g2.state == GameState::Playing; ++k) { g2.Update(dt, s2, in, YAW); acc += Score(g2, s2) * (k == 23 ? 1.0f : 0.15f); }
                if (a == 0) acc += 0.5f; // slight stay penalty
                if (acc < best) { best = acc; bestA = a; }
            }
            if (wander > 0) { bestA = wdir; --wander; }
            Input in; in.forward = -dirs[bestA][1]; in.right = dirs[bestA][0];
            for (int k = 0; k < 4 && g.state == GameState::Playing; ++k) { g.Update(dt, s, in, YAW); t += dt; }
            if (g.lockedCount > 0 && !prevLock) ++locks; prevLock = g.lockedCount > 0;
        }
        std::printf("level %d: %s at t=%.1fs, lock-ons endured: %d, final pos (%.1f,%.1f)\n", level,
            g.state == GameState::Won ? "ESCAPED" : g.state == GameState::Lost ? "INCINERATED" : "timeout", t, locks, g.playerPos.x, g.playerPos.z);
    }
}
