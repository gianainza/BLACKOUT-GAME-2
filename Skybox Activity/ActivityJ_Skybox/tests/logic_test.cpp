// Headless test of the OBSOLETE rules: JSON loader (including broken files), cone + line-of-sight lit test,
// lock-on, strikes, conveyors, hacking. No OpenGL needed.
//
//   cd SkyboxLab
//   g++ -std=c++17 -I. ../tests/logic_test.cpp Scene.cpp SceneLoader.cpp Json.cpp LitTest.cpp Game.cpp -o logic_test
//   ./logic_test
#include "SceneLoader.h"
#include "Game.h"
#include <cstdio>
#include <fstream>

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { ++fails; std::printf("FAIL: %s\n", msg); } else std::printf("ok:   %s\n", msg); } while (0)

static const float YAW = -1.57079632f;   // forward = -z, right = +x
static Input Move(float dx, float dz) { Input i; i.forward = -dz; i.right = dx; return i; }

static bool HasMessage(const Scene& sc, const std::string& needle)
{
    for (const auto& m : sc.messages) if (m.find(needle) != std::string::npos) return true;
    return false;
}

static std::string Slurp(const char* p) { std::ifstream f(p, std::ios::binary); return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); }
static void Write(const char* p, const std::string& s) { std::ofstream f(p, std::ios::binary); f << s; }
static std::string Replace(std::string s, const std::string& a, const std::string& b)
{ size_t i = s.find(a); if (i != std::string::npos) s.replace(i, a.size(), b); return s; }
// replace the first 'a' that comes after 'marker'
static std::string ReplaceAfter(std::string s, const std::string& marker, const std::string& a, const std::string& b)
{ size_t m = s.find(marker); if (m == std::string::npos) return s; size_t i = s.find(a, m); if (i != std::string::npos) s.replace(i, a.size(), b); return s; }
static int LineOf(const std::string& s, const std::string& needle)
{ size_t i = s.find(needle); int line = 1; for (size_t k = 0; k < i && k < s.size(); ++k) if (s[k] == '\n') ++line; return line; }

int main()
{
    const std::string good = Slurp("level01.json");
    CHECK(!good.empty(), "level01.json is readable (run from the SkyboxLab folder)");

    Scene sc;
    CHECK(SceneLoader::Load("level01.json", sc), "level loads");
    bool anyError = false; for (const auto& m : sc.messages) if (m.find(": ERROR") != std::string::npos) anyError = true;
    CHECK(!anyError, "no ERROR messages for the shipped level");
    CHECK(sc.droneCount == 2 && sc.bulbs.size() == 5 && sc.conveyors.size() == 5 && sc.terminals.size() == 3, "2 drones, 3 scanners, 5 belts, 3 terminals");
    CHECK(sc.materials.size() >= 5, "at least five materials");

    // ---------------- loader robustness (R1 D2) ----------------
    {
        Scene b; Write("_bad1.json", Replace(good, "\"omni_bias\": 0.05", "\"omni_bias\" 0.05"));
        int expect = LineOf(good, "\"omni_bias\"");
        bool ok = SceneLoader::Load("_bad1.json", b);
        std::printf("      broken line is %d; loader said: %s\n", expect, b.messages.empty() ? "(nothing)" : b.messages[0].c_str());
        CHECK(!ok && HasMessage(b, "_bad1.json:" + std::to_string(expect) + ":") && HasMessage(b, "':' expected"), "a missing colon is reported with the right line number and does not crash");
    }
    {
        Scene b; Write("_bad2.json", Replace(good, "\"edge\": 18", "\"edge\": 400"));
        bool ok = SceneLoader::Load("_bad2.json", b);
        CHECK(ok && HasMessage(b, "'edge' must be a number between 3 and 80") && b.bulbs.size() == 4, "a bad scanner 'edge' skips that scanner, names the key and line, and the level still loads");
    }
    {
        Scene b; Write("_bad3.json", Replace(good, "\"material_id\": \"mat_wood\"", "\"material_id\": \"mat_nope\""));
        bool ok = SceneLoader::Load("_bad3.json", b);
        CHECK(ok && HasMessage(b, "material_id 'mat_nope' does not exist"), "an unknown material is a warning with a fallback, not a crash");
    }
    {
        Scene b; Write("_bad4.json", ReplaceAfter(good, "\"terminals\"", "\"sweep_a\"", "\"sweep_zzz\""));
        SceneLoader::Load("_bad4.json", b);
        CHECK(HasMessage(b, "no scanner (spot light) with that id"), "a terminal naming a scanner that does not exist is reported");
    }
    {
        Scene b; Write("_bad5.json", Replace(good, "Textures/metal.png", "Textures/does_not_exist.png"));
        bool ok = SceneLoader::Load("_bad5.json", b);
        CHECK(ok && HasMessage(b, "not found - a magenta checker"), "a missing texture is reported and survivable");
    }
    {
        Scene b; bool ok = SceneLoader::Load("_does_not_exist.json", b);
        CHECK(!ok && HasMessage(b, "cannot open the level file"), "a missing level file is a message, not a crash");
    }
    {
        Scene b; Write("_bad6.json", "{ \"lights\": {}, \"objects\": [] }");
        bool ok = SceneLoader::Load("_bad6.json", b);
        CHECK(!ok, "a file with no lights / objects / game_logic is rejected as unplayable");
    }
    std::remove("_bad1.json"); std::remove("_bad2.json"); std::remove("_bad3.json"); std::remove("_bad4.json"); std::remove("_bad5.json"); std::remove("_bad6.json");

    Game g; g.Init(sc); g.state = GameState::Playing;
    const int D0 = sc.droneCount;           // first scanner index
    CHECK(sc.bulbs[D0].scanner && !sc.bulbs[0].scanner, "bulbs are ordered drones first, then scanners");

    // ---------------- cone / line of sight ----------------
    {
        Bulb& b = sc.bulbs[D0];
        b.aim = glm::vec2(-13.0f, 9.5f);
        LitResult r = ComputeIllumination(sc, glm::vec3(-13.0f, 0.01f, 9.5f), true);
        CHECK(r.per[D0] >= sc.litThreshold, "droid at the aim point is exposed to the scanner");
        r = ComputeIllumination(sc, glm::vec3(5.0f, 0.01f, -5.0f), true);
        CHECK(r.per[D0] == 0.0f, "droid far outside the cone is not exposed");
        b.Reset();
    }
    {
        Scene s2 = sc;
        for (auto& b : s2.bulbs) b.power = 0.0f;
        Bulb& b = s2.bulbs[D0 + 1];                       // sweep_b
        b.power = 1.0f; b.mount = glm::vec3(2.5f, 6.5f, 6.5f); b.Place(); b.aim = glm::vec2(9.0f, 6.5f);
        glm::vec3 p(9.0f, 0.01f, 6.5f);                   // scrap_2 (x 4.8..7.2, z 4..9) is between the emitter and the point
        LitResult open = ComputeIllumination(s2, p, false), shad = ComputeIllumination(s2, p, true);
        CHECK(open.per[D0 + 1] > 0.0f && shad.per[D0 + 1] == 0.0f, "scrap metal blocks a scanner beam (line of sight on / off)");
    }
    {   // a drone is omnidirectional
        Scene s2 = sc;
        for (auto& b : s2.bulbs) b.power = 0.0f;
        s2.bulbs[0].power = 1.0f;
        glm::vec3 below = s2.bulbs[0].position; below.y = 0.01f;
        LitResult r = ComputeIllumination(s2, below, true);
        CHECK(r.per[0] > sc.litThreshold, "a drone lights the floor under it");
    }

    // ---------------- detection -> lock -> strike if you stand still (scanner AND drone) ----------------
    for (int which = 0; which < 2; ++which)
    {
        int idx = which == 0 ? D0 : 0;
        Scene s = sc; Game gg; gg.Init(s); gg.state = GameState::Playing;
        s.conveyors.clear();
        for (auto& b : s.bulbs) b.power = 0.0f;
        for (size_t k = 0; k < s.bulbs.size(); ++k) s.bulbs[k].power = ((int)k == idx) ? 1.0f : 0.0f;
        Bulb& b = s.bulbs[idx];
        if (which == 0)
        {
            gg.playerPos = glm::vec3(-13.0f, 0, 9.5f);
            b.route[0] = { glm::vec2(-13.0f, 9.5f) }; b.wp = 0; b.aim = glm::vec2(-13.0f, 9.5f);
        }
        else
        {
            b.speed = 0.0f;                                      // park the drone
            gg.playerPos = glm::vec3(b.position.x + 1.0f, 0, b.position.z);
        }
        float tLock = -1, t = 0;
        while (t < 9.0f && gg.state == GameState::Playing)
        {
            // keep the other lights off every frame (Update re-arms power when a light is Scanning)
            for (size_t k = 0; k < s.bulbs.size(); ++k) if ((int)k != idx) { s.bulbs[k].state = ScanState::Cooling; s.bulbs[k].stateTime = 0; s.bulbs[k].power = 0; }
            gg.Update(1.0f / 60.0f, s, Input(), YAW); t += 1.0f / 60.0f;
            if (tLock < 0 && b.state == ScanState::Locked) tLock = t;
        }
        std::printf("      %s: lock at %.2fs, ended at %.2fs\n", which == 0 ? "scanner" : "drone", tLock, t);
        CHECK(tLock > 0.4f && tLock < 2.5f, which == 0 ? "standing in a scanner beam locks it on in about a second" : "standing under a drone locks it on in about a second");
        CHECK(gg.state == GameState::Lost && t - tLock > 1.8f && t - tLock < 2.05f, which == 0 ? "scanner strike lands strikeDelay after lock and kills a droid that stood still" : "drone strike lands strikeDelay after lock and kills a droid that stood still");
    }
    // ---------------- running away after lock survives ----------------
    {
        Scene s = sc; Game gg; gg.Init(s); gg.state = GameState::Playing;
        s.conveyors.clear();
        gg.playerPos = glm::vec3(-13.5f, 0, 11.5f);
        Bulb& b = s.bulbs[D0];
        b.route[0] = { glm::vec2(-13.5f, 11.5f) }; b.wp = 0; b.aim = glm::vec2(-13.5f, 11.5f);
        float t = 0;
        while (t < 2.0f && b.state != ScanState::Locked) { gg.Update(1.0f / 60.0f, s, Input(), YAW); t += 1.0f / 60.0f; }
        for (float u = 0; u < 2.6f && gg.state == GameState::Playing; u += 1.0f / 60.0f) gg.Update(1.0f / 60.0f, s, Move(0, -1), YAW);
        CHECK(gg.state == GameState::Playing, "sprinting away after lock-on survives the strike");
    }
    // ---------------- conveyor ----------------
    {
        Scene s = sc; Game gg; gg.Init(s); gg.state = GameState::Playing;
        gg.playerPos = glm::vec3(-12.0f, 0, 9.5f);
        float x0 = gg.playerPos.x;
        for (int i = 0; i < 60; ++i) { for (auto& b : s.bulbs) b.power = 0; gg.Update(1.0f / 60.0f, s, Input(), YAW); }
        std::printf("      belt moved the droid %.2f m in 1 s (belt speed %.1f)\n", gg.playerPos.x - x0, s.conveyors[0].speed);
        CHECK(std::fabs((gg.playerPos.x - x0) - s.conveyors[0].speed) < 0.1f && gg.onBelt, "belt carries the droid at belt speed");
    }
    // ---------------- hacking ----------------
    {
        Scene s = sc; Game gg; gg.Init(s); gg.state = GameState::Playing;
        gg.playerPos = glm::vec3(-10.5f, 0, 12.2f);      // beside t1 (-10.5, 13.4)
        Input in; in.hack = true;
        const int sa = D0;                                // sweep_a
        int before = s.bulbs[sa].routeId;
        for (int i = 0; i < 90; ++i) { for (auto& b : s.bulbs) b.power = 0; gg.Update(1.0f / 60.0f, s, in, YAW); }
        CHECK(gg.nearTerminal == 0, "terminal detected in reach");
        CHECK(s.bulbs[sa].routeId != before, "holding E reroutes the linked scanner");
        CHECK(s.terminals[0].cooldown > 0, "terminal reboots after a hack");
        Scene s3 = sc; Game g3; g3.Init(s3); g3.state = GameState::Playing;
        in.hack = true; for (int i = 0; i < 120; ++i) { for (auto& b : s3.bulbs) b.power = 0; g3.Update(1.0f / 60.0f, s3, in, YAW); }
        CHECK(s3.bulbs[sa].routeId == 0, "hacking out of range does nothing");
    }
    std::printf("\n%s (%d failure(s))\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails);
    return fails ? 1 : 0;
}
