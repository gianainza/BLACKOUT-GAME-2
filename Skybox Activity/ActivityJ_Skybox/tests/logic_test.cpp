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
#include <set>
#include <algorithm>

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
    int routeT = 0, codeT = 0; for (const auto& tt : sc.terminals) (tt.kind == Terminal::Kind::Code ? codeT : routeT)++;
    CHECK(sc.droneCount == 2 && sc.bulbs.size() == 5 && sc.conveyors.size() == 5 && routeT == 3 && codeT >= 8, "2 drones, 3 scanners, 5 belts, 3 route terminals, at least 8 code terminals");
    CHECK(sc.materials.size() >= 5, "at least five materials");
    CHECK(sc.exitPath.enabled && glm::length(sc.exitPath.to - sc.exitPos) < sc.exitRadius + 2.0f, "the exit has a glowing pathway that leads to the exit zone");

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
    {
        Scene b; Write("_bad7.json", Replace(good, "\"to\": [14.95, -11.5]", "\"to\": [9.0, -11.5]"));
        bool ok = SceneLoader::Load("_bad7.json", b);
        CHECK(HasMessage(b, "at least 0.5 m apart") && !b.exitPath.enabled, "a zero-length exit pathway is reported and the plain hatch is used instead");
    }
    std::remove("_bad7.json");
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

    // ---------------- the scanner's floor marker must show exactly what the game treats as "scanned" ----------------
    {
        Scene s2 = sc;
        for (auto& b : s2.bulbs) b.power = 0.0f;
        Bulb& b = s2.bulbs[D0];
        b.power = 1.0f; b.aim = glm::vec2(-8.0f, 9.5f);        // open floor under sweep_a's route
        glm::vec2 c; float yaw, a, bb;
        bool ok = ExposureEllipse(s2, b, c, yaw, a, bb);
        std::printf("      marker ellipse: centre (%.2f, %.2f) semi-axes %.2f x %.2f m\n", c.x, c.y, a, bb);
        CHECK(ok && a > 0.5f && bb > 0.5f, "an aimed scanner has a floor marker of sensible size");
        // walk outwards from the aim point in 8 directions: inside the ellipse => scanned, clearly outside => not scanned
        int wrongIn = 0, wrongOut = 0, total = 0;
        for (int k = 0; k < 8; ++k)
        {
            float ang = (float)k * (2.0f * PI_F / 8.0f);
            glm::vec2 dir(std::cos(ang), std::sin(ang));
            // ellipse radius in this direction (local axes: +x = yaw direction)
            glm::vec2 u(std::cos(yaw), -std::sin(yaw)), v(-u.y, u.x);
            for (float r = 0.0f; r < 9.0f; r += 0.02f)
            {
                glm::vec2 q = b.aim + dir * r;
                glm::vec2 rel = q - c;
                float lx = glm::dot(rel, u) / a, ly = glm::dot(rel, v) / bb;
                float e = lx * lx + ly * ly;
                glm::vec3 p(q.x, SAMPLE_HEIGHT, q.y);
                bool scanned = ComputeIllumination(s2, p, false).per[D0] >= s2.litThreshold;
                if (e < 0.80f)       { ++total; if (!scanned) ++wrongIn; }
                else if (e > 1.35f)  { ++total; if (scanned)  ++wrongOut; }
            }
        }
        std::printf("      %d samples: %d inside-but-not-scanned, %d outside-but-scanned\n", total, wrongIn, wrongOut);
        CHECK(wrongIn == 0 && wrongOut == 0, "the marker's ellipse matches the game's own scanned test (honest picture)");
        Scene s3 = sc; for (auto& x : s3.bulbs) x.power = 0.0f;
        CHECK(!ExposureEllipse(s3, s3.bulbs[D0], c, yaw, a, bb), "a scanner that is off (striking / cooling) has no marker");
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
        std::printf("      %s: lock at %.2fs, ended at %.2fs (strike delay %.2f)\n", which == 0 ? "scanner" : "drone", tLock, t, sc.strikeDelay);
        CHECK(tLock > 0.4f && tLock < 2.5f, which == 0 ? "standing in a scanner beam locks it on in about a second" : "standing under a drone locks it on in about a second");
        CHECK(gg.state == GameState::Lost && std::fabs((t - tLock) - sc.strikeDelay) < 0.06f, which == 0 ? "scanner strike lands strikeDelay after lock and kills a droid that stood still" : "drone strike lands strikeDelay after lock and kills a droid that stood still");
    }
    // ---------------- running away after lock: with a 0.8 s strike only a SPRINT gets out of the blast ----------------
    for (int sprint = 0; sprint < 2; ++sprint)
    {
        Scene s = sc; Game gg; gg.Init(s); gg.state = GameState::Playing;
        s.conveyors.clear(); s.oilPuddles.clear(); s.objects.clear();      // open floor: only the speed matters here
        for (auto& b : s.bulbs) b.power = 0.0f;
        gg.playerPos = glm::vec3(-13.5f, 0, 11.5f);
        Bulb& b = s.bulbs[D0];
        b.power = 1.0f;
        b.route[0] = { glm::vec2(-13.5f, 11.5f) }; b.wp = 0; b.aim = glm::vec2(-13.5f, 11.5f);
        float t = 0;
        while (t < 2.0f && b.state != ScanState::Locked) { gg.Update(1.0f / 60.0f, s, Input(), YAW); t += 1.0f / 60.0f; }
        Input in = Move(0, -1); in.run = (sprint == 1);
        for (float u = 0; u < 1.6f && gg.state == GameState::Playing; u += 1.0f / 60.0f) gg.Update(1.0f / 60.0f, s, in, YAW);
        CHECK(sprint == 1 ? gg.state == GameState::Playing : gg.state == GameState::Lost,
              sprint == 1 ? "sprinting away right after lock-on survives the 0.8 s strike" : "walking away after lock-on does NOT outrun the 0.8 s strike (that is why sprint exists)");
    }
    // ---------------- speed multiplier ----------------
    {
        auto speedOf = [&](bool run, bool sneak, bool litOn, bool oil, int locked) {
            Scene s = sc; Game gg; gg.Init(s); gg.state = GameState::Playing;
            s.conveyors.clear(); s.oilPuddles.clear();
            gg.playerPos = glm::vec3(0.0f, 0, 12.0f);
            if (oil) { OilPuddle p; p.pos = glm::vec2(0.0f, 12.0f); p.radius = 3.0f; s.oilPuddles.push_back(p); }
            gg.lit = litOn; gg.lockedCount = locked;
            Input in = Move(1, 0); in.run = run; in.sneak = sneak;
            // one step only: Update re-evaluates 'lit' afterwards, so look at the multiplier it just used
            gg.Update(1.0f / 60.0f, s, in, YAW);
            return gg.speedMult;
        };
        // the multiplier is computed from the 'lit' / 'lockedCount' state the previous frame left behind, so set it directly
        auto base = [&](bool run, bool sneak, bool oil, bool litOn, int locked) { return speedOf(run, sneak, litOn, oil, locked); };
        CHECK(std::fabs(base(false, false, false, false, 0) - 1.0f) < 1e-4f, "walking: multiplier 1.0");
        CHECK(std::fabs(base(true, false, false, false, 0) - sc.sprintMult) < 1e-4f, "Shift: sprint multiplier");
        CHECK(std::fabs(base(false, true, false, false, 0) - sc.sneakMult) < 1e-4f, "Ctrl: sneak multiplier");
        CHECK(std::fabs(base(true, true, false, false, 0) - sc.sneakMult) < 1e-4f, "Shift + Ctrl: sneak wins");
        CHECK(std::fabs(base(false, false, false, true, 0) - sc.litSpeedMult) < 1e-4f, "being lit slows the droid");
        CHECK(std::fabs(base(false, false, false, true, 1) - 1.0f) < 1e-4f, "NO light slowdown while any light is locked on (the strike must be escapable)");
        CHECK(std::fabs(base(false, false, true, false, 0) - sc.oilSpeedMult) < 1e-4f, "oil puddle slows the droid");
        CHECK(std::fabs(base(true, false, true, true, 0) - sc.sprintMult * sc.litSpeedMult * sc.oilSpeedMult) < 1e-4f, "sprint x lit x oil multiply together");
        CHECK(base(false, true, true, true, 0) >= sc.minSpeedMult - 1e-6f && std::fabs(base(false, true, true, true, 0) - sc.minSpeedMult) < 1e-4f, "the product is clamped to min_speed_mult");
    }
    // ---------------- oil puddle geometry ----------------
    {
        bool ok = true;
        for (const auto& p : sc.oilPuddles)
        {
            if (PointInsideSolid(sc, p.pos.x, p.pos.y, 0.0f)) ok = false;
            if (glm::length(p.pos - sc.exitPos) < sc.exitRadius + p.radius) ok = false;
        }
        CHECK(sc.oilPuddles.size() >= 6 && ok, "at least 6 oil puddles, none inside a solid object or on the exit pad");
        Scene b; Write("_bad8.json", Replace(good, "\"position\": [-9.0, 0, 12.0]", "\"position\": [-7.0, 0, -10.0]"));   // inside scrap_3
        SceneLoader::Load("_bad8.json", b);
        CHECK(HasMessage(b, "the puddle centre is inside a solid object"), "a puddle placed inside a solid object is rejected with a message");
    }
    std::remove("_bad8.json");
    // ---------------- code door puzzle: genuine vs decoy terminals ----------------
    {
        int badSeeds = 0, colours[16] = {}; bool diffAcrossResets = false;
        for (unsigned seed = 1; seed <= 60; ++seed)
        {
            Scene s = sc; Game gg; gg.SeedRng(seed); gg.Init(s);
            int genuine = 0, slotSeen[4] = {}; bool ok = true; std::set<int> decoyColours;
            for (const auto& tt : s.terminals)
            {
                if (tt.kind != Terminal::Kind::Code) continue;
                if (tt.colourIdx < 0 || tt.codeSlot < 0 || tt.codeSlot > 3 || tt.digit < 0 || tt.digit > 9) ok = false;
                if (tt.genuine)
                {
                    ++genuine; ++slotSeen[tt.codeSlot];
                    if (tt.digit != gg.code[tt.codeSlot] || tt.colourIdx != gg.lockColour) ok = false;
                }
                else
                {
                    if (tt.colourIdx == gg.lockColour) ok = false;                 // a decoy NEVER shows the lock colour
                    if (tt.digit == gg.code[tt.codeSlot]) ok = false;              // ...and never gives the true digit for its slot
                    decoyColours.insert(tt.colourIdx);
                }
            }
            for (int k = 0; k < 4; ++k) if (slotSeen[k] != 1) ok = false;
            if (genuine != 4 || decoyColours.size() < 3) ok = false;               // several different decoy colours
            if (!ok) ++badSeeds;
            ++colours[gg.lockColour];
        }
        CHECK(badSeeds == 0, "60 seeds: exactly 4 genuine terminals (one per slot, true digit, lock colour); decoys never match the lock colour or the true digit; >= 3 decoy colours");
        int distinct = 0; for (int c : colours) if (c) ++distinct;
        CHECK(distinct >= 4, "the lock colour is chosen at random (>= 4 different colours over 60 seeds)");
        Scene s1 = sc, s2 = sc; Game a, b; a.SeedRng(777); b.SeedRng(777); a.Init(s1); b.Init(s2);
        bool same = a.lockColour == b.lockColour; for (int k = 0; k < 4; ++k) same = same && a.code[k] == b.code[k];
        for (size_t k = 0; k < s1.terminals.size(); ++k) same = same && s1.terminals[k].digit == s2.terminals[k].digit && s1.terminals[k].colourIdx == s2.terminals[k].colourIdx;
        CHECK(same, "the same seed gives the same puzzle (deterministic for tests and the bot)");
        int c0 = a.lockColour; int code0 = a.code[0] * 1000 + a.code[1] * 100 + a.code[2] * 10 + a.code[3];
        for (int i = 0; i < 20 && !diffAcrossResets; ++i) { a.Reset(s1); int c1 = a.code[0] * 1000 + a.code[1] * 100 + a.code[2] * 10 + a.code[3]; diffAcrossResets = (a.lockColour != c0) || (c1 != code0); }
        CHECK(diffAcrossResets, "every Reset() draws a new puzzle");
        CHECK(a.doorLocked, "the door is locked at Reset");
        // a level without code terminals has no puzzle and the door is never locked
        Scene s3 = sc; s3.terminals.erase(std::remove_if(s3.terminals.begin(), s3.terminals.end(), [](const Terminal& x) { return x.kind == Terminal::Kind::Code; }), s3.terminals.end());
        s3.puzzleEnabled = false; Game g3; g3.Init(s3);
        CHECK(!g3.doorLocked, "no code terminals: the door is never locked (backward compatible)");
        // cabinets: none overlaps another solid object (terminals must be reachable and visible)
        bool overlap = false;
        for (const auto& tt : sc.terminals)
        {
            if (tt.kind != Terminal::Kind::Code) continue;
            for (const auto& o : sc.objects)
            {
                if (o.id == "terminal_" + tt.id || !o.solid) continue;
                if (tt.pos.x + 0.45f > o.minB.x && tt.pos.x - 0.45f < o.maxB.x && tt.pos.y + 0.45f > o.minB.z && tt.pos.y - 0.45f < o.maxB.z) overlap = true;
            }
            if (glm::length(tt.pos - sc.exitPos) < sc.exitRadius + 1.0f) overlap = true;
        }
        CHECK(!overlap, "no code terminal cabinet overlaps another solid object or the exit");
    }
    // ---------------- code terminals: hacking, notebook ----------------
    {
        Scene s = sc; Game gg; gg.SeedRng(5); gg.Init(s); gg.state = GameState::Playing;
        s.bulbs.clear();                                      // no light can interfere
        Terminal* g0 = nullptr; for (auto& tt : s.terminals) if (tt.genuine && tt.codeSlot == 2) g0 = &tt;
        gg.playerPos = glm::vec3(g0->pos.x + 1.0f, 0, g0->pos.y + 0.8f);
        Input in; in.hack = true;
        int frames = 0; while (g0->used == false && frames < 60 * 5) { gg.Update(1.0f / 60.0f, s, in, YAW); ++frames; }
        std::printf("      code terminal hacked after %.2f s (code_hack_time %.2f)\n", frames / 60.0f, s.codePuzzle.hackTime);
        CHECK(g0->used && std::fabs(frames / 60.0f - s.codePuzzle.hackTime) < 0.1f, "holding E on a code terminal takes code_hack_time and then it is used");
        CHECK(gg.notebook[2].size() == 1 && gg.notebook[2][0].digit == gg.code[2], "the digit goes into the notebook slot of that terminal");
        CHECK(gg.bigMessage == "SLOT 3 = " + std::to_string(gg.code[2]) && gg.bigMessageTime > 2.5f, "the big message SLOT n = d is raised for 3 s");
        for (int i = 0; i < 90; ++i) gg.Update(1.0f / 60.0f, s, in, YAW);
        CHECK(gg.notebook[2].size() == 1, "one use per terminal: holding E again gives nothing");
        // a decoy goes into the notebook too, and no colour is shown (NOTEBOOK_SHOWS_COLOR is false)
        Terminal* d0 = nullptr; for (auto& tt : s.terminals) if (tt.kind == Terminal::Kind::Code && !tt.genuine) d0 = &tt;
        gg.playerPos = glm::vec3(d0->pos.x + 1.0f, 0, d0->pos.y + 0.8f);
        for (int i = 0; i < 60 * 3; ++i) gg.Update(1.0f / 60.0f, s, in, YAW);
        CHECK(gg.notebook[d0->codeSlot].size() >= 1 && d0->used, "a decoy digit is collected into the notebook as a candidate as well");
        CHECK(!NOTEBOOK_SHOWS_COLOR, "NOTEBOOK_SHOWS_COLOR is false by default");
        gg.Reset(s);
        CHECK(gg.notebook[0].empty() && gg.notebook[1].empty() && gg.notebook[2].empty() && gg.notebook[3].empty() && gg.entered.empty(), "Reset clears the notebook and the keypad entry");
        bool anyUsed = false; for (const auto& tt : s.terminals) anyUsed = anyUsed || tt.used; CHECK(!anyUsed, "Reset makes every code terminal usable again");
    }
    // ---------------- the keypad and the door ----------------
    {
        Scene s = sc; Game gg; gg.SeedRng(11); gg.Init(s); gg.state = GameState::Playing;
        s.bulbs.clear(); s.conveyors.clear();
        gg.playerPos = glm::vec3(s.exitPos.x - 0.2f, 0, s.exitPos.y);          // inside the exit zone
        for (int i = 0; i < 10; ++i) gg.Update(1.0f / 60.0f, s, Input(), YAW);
        CHECK(gg.state == GameState::Playing, "standing in the exit zone with the door LOCKED does not win");
        CHECK(gg.KeypadActive(), "the keypad is active near a locked door");
        auto press = [&](int d) { Input in; in.digit = d; gg.Update(1.0f / 60.0f, s, in, YAW); };
        auto back = [&]() { Input in; in.backspace = true; gg.Update(1.0f / 60.0f, s, in, YAW); };
        auto enter = [&]() { Input in; in.enter = true; gg.Update(1.0f / 60.0f, s, in, YAW); };
        press(1); press(2); press(3); back(); press(9);
        CHECK(gg.entered.size() == 3 && gg.entered[2] == 9 && gg.entered[1] == 2, "digits type in and Backspace deletes the last one");
        press(4); press(5);
        CHECK(gg.entered.size() == 4, "at most 4 digits are accepted");
        enter();
        CHECK(gg.doorLocked && gg.message == "WRONG CODE" && gg.entered.empty(), "a wrong code: WRONG CODE, the door stays locked, the entry is cleared");
        // fix the entry to be certainly wrong for the next test, then type the right one
        for (int k = 0; k < 4; ++k) press(gg.code[k]);
        enter();
        CHECK(!gg.doorLocked && gg.message == "DOOR UNLOCKED", "the right code unlocks the door");
        for (int i = 0; i < 5; ++i) gg.Update(1.0f / 60.0f, s, Input(), YAW);
        CHECK(gg.state == GameState::Won, "inside the exit zone with the door unlocked wins");
        // keypad digits are ignored away from the door
        Scene s2 = sc; Game g2; g2.SeedRng(11); g2.Init(s2); g2.state = GameState::Playing; s2.bulbs.clear();
        g2.playerPos = glm::vec3(-12.5f, 0, 12.5f);
        Input in; in.digit = 4; g2.Update(1.0f / 60.0f, s2, in, YAW);
        CHECK(!g2.KeypadActive() && g2.entered.empty(), "far from the door the keypad does nothing");
        // wrong code penalty: detection rises by wrong_code_penalty, and >= 1 locks the light on at once
        Scene s4 = sc; Game g4; g4.SeedRng(11); g4.Init(s4); g4.state = GameState::Playing;
        g4.playerPos = glm::vec3(s4.exitPos.x - 0.2f, 0, s4.exitPos.y);
        for (auto& b : s4.bulbs) b.power = 0.0f;
        g4.UpdateScanners(0.0f, s4);
        const int a = D0, b2 = D0 + 1;
        s4.bulbs[a].detect = 0.2f; s4.bulbs[b2].detect = 0.7f;
        g4.entered = { (g4.code[0] + 1) % 10, g4.code[1], g4.code[2], g4.code[3] };
        g4.SubmitCode(s4);
        CHECK(std::fabs(s4.bulbs[a].detect - (0.2f + s4.codePuzzle.wrongPenalty)) < 1e-4f, "a wrong code adds wrong_code_penalty to a light's detection");
        CHECK(s4.bulbs[b2].detect >= 1.0f && s4.bulbs[b2].state == ScanState::Locked, "...and a light pushed to >= 1.0 locks on immediately");
    }
    // ---------------- noise radius ----------------
    {
        auto settle = [&](bool run, bool sneak, bool moving, bool lamp, bool hackNow) {
            Scene s = sc; s.bulbs.clear(); s.conveyors.clear(); s.oilPuddles.clear(); s.objects.clear();
            Game gg; gg.SeedRng(3); gg.Init(s); gg.state = GameState::Playing; gg.flashOn = lamp;
            gg.playerPos = glm::vec3(0.0f, 0, 12.0f);
            Terminal tt; tt.kind = Terminal::Kind::Route; tt.pos = glm::vec2(0.0f, 12.0f); tt.links = {}; s.terminals.clear(); s.terminals.push_back(tt);   // a terminal underfoot, for the hacking case
            s.lampDrain = 0.0f;                                         // keep the lamp on for the whole run
            Input in; in.run = run; in.sneak = sneak; in.hack = hackNow; if (moving) in.right = 1.0f;
            for (int i = 0; i < (hackNow ? 60 : 120); ++i) { if (moving) gg.playerPos = glm::vec3(0.0f, 0, 12.0f); gg.Update(1.0f / 60.0f, s, in, YAW); }   // a hack takes 1.2 s, so stop at 1.0 s
            return gg.noiseRadius;
        };
        auto near = [](float a, float b) { return std::fabs(a - b) < 0.05f; };
        CHECK(near(settle(false, false, false, false, false), sc.noiseStill), "noise: standing still = 0.5 m");
        CHECK(near(settle(false, true, true, false, false), sc.noiseSneak), "noise: sneaking = 1.5 m");
        CHECK(near(settle(false, false, true, false, false), sc.noiseWalk), "noise: walking = 4.5 m");
        CHECK(near(settle(true, false, true, false, false), sc.noiseSprint), "noise: sprinting = 9.0 m");
        CHECK(near(settle(false, false, false, false, true), sc.noiseHacking), "noise: hacking a terminal = 5.0 m");
        CHECK(near(settle(false, false, false, true, false), sc.noiseStill + sc.noiseLampBonus) && near(settle(true, false, true, true, false), sc.noiseSprint + sc.noiseLampBonus), "noise: +1.5 m while the lamp is on");
        CHECK(near(settle(false, true, false, false, false), sc.noiseStill), "noise: holding sneak while NOT moving is still 'standing still'");
        // smoothing: a step from still to sprint rises gradually
        Scene s = sc; s.bulbs.clear(); s.conveyors.clear(); s.objects.clear(); Game gg; gg.Init(s); gg.state = GameState::Playing; gg.flashOn = false; gg.playerPos = glm::vec3(0, 0, 12);
        for (int i = 0; i < 30; ++i) gg.Update(1.0f / 60.0f, s, Input(), YAW);
        Input sp; sp.run = true; sp.right = 1.0f; gg.Update(1.0f / 60.0f, s, sp, YAW);
        CHECK(gg.noiseRadius > s.noiseStill + 0.5f && gg.noiseRadius < s.noiseSprint - 1.0f, "noise is smoothed: one frame after starting to sprint it has not jumped to 9 m");
    }
    // ---------------- drones investigate noise and return ----------------
    {
        Scene s = sc; s.terminals.clear(); s.conveyors.clear(); s.oilPuddles.clear();
        s.bulbs.erase(s.bulbs.begin() + 1, s.bulbs.end());               // keep only drone_a
        s.droneCount = 1;
        Bulb& d = s.bulbs[0];
        d.diffuse = 0.0001f;                                             // it cannot detect the droid: only the movement is tested
        d.s = 0.0f; d.Place();
        Game gg; gg.SeedRng(2); gg.Init(s); gg.state = GameState::Playing; gg.flashOn = false; s.lampDrain = 0.0f;
        Bulb& dd = s.bulbs[0];
        glm::vec3 start = dd.position;
        // the droid stands 3 m from the drone, walking on the spot: noise 4.5 m > 3 m
        gg.playerPos = glm::vec3(start.x + 3.0f, 0, start.z);
        Input walk; walk.right = 1.0f;
        for (int i = 0; i < 90; ++i) { glm::vec3 keep = gg.playerPos; gg.Update(1.0f / 60.0f, s, walk, YAW); gg.playerPos = keep; if (dd.mode == BulbMode::Investigate) break; }
        CHECK(dd.mode == BulbMode::Investigate, "a drone in Scanning whose distance to the droid is < noiseRadius switches to INVESTIGATE");
        // flight speed = patrol speed x 1.3, at the patrol height
        glm::vec3 p0 = dd.position; float y0 = dd.position.y;
        for (int i = 0; i < 30; ++i) { glm::vec3 keep = gg.playerPos; gg.Update(1.0f / 60.0f, s, walk, YAW); gg.playerPos = keep; }
        float moved = glm::length(dd.position - p0);
        std::printf("      drone flew %.2f m in 0.5 s (patrol speed %.1f x %.1f = %.2f m/s)\n", moved, dd.speed, s.droneInvestigateSpeedMult, dd.speed * s.droneInvestigateSpeedMult);
        CHECK(std::fabs(dd.position.y - y0) < 1e-4f, "while investigating the drone keeps its patrol height");
        CHECK(moved > 0.2f && moved < 0.5f * dd.speed * s.droneInvestigateSpeedMult + 0.05f, "...and flies at about speed x 1.3 toward the noise");
        // now the droid goes quiet and stands still: the drone arrives, hovers 2 s, returns and resumes its loop
        glm::vec3 hoverAt; float hoverStart = -1, hoverEnd = -1, t = 0; bool returned = false, patrolAgain = false;
        gg.playerPos = glm::vec3(start.x + 3.0f, 0, start.z);
        for (int i = 0; i < 60 * 40; ++i)
        {
            gg.Update(1.0f / 60.0f, s, Input(), YAW); t += 1.0f / 60.0f;
            if (dd.mode == BulbMode::Investigate && glm::length(glm::vec2(dd.position.x, dd.position.z) - glm::vec2(dd.investigateTarget.x, dd.investigateTarget.z)) <= s.droneArriveDistance && hoverStart < 0) hoverStart = t;
            if (dd.mode == BulbMode::Return && hoverEnd < 0) hoverEnd = t;
            if (dd.mode == BulbMode::Return) returned = true;
            if (returned && dd.mode == BulbMode::Patrol) { patrolAgain = true; break; }
        }
        std::printf("      hover began %.2fs, return began %.2fs (hover %.2f s)\n", hoverStart, hoverEnd, hoverEnd - hoverStart);
        CHECK(hoverStart > 0 && std::fabs((hoverEnd - hoverStart) - s.droneHoverTime) < 0.1f, "the drone hovers for 2 s once within 1.5 m of the noise point");
        CHECK(patrolAgain, "then it RETURNs and resumes PATROL");
        // back on the loop: its position is on the patrol path, and it keeps moving along it
        float sNear = dd.NearestPathS(); glm::vec3 onLoop = dd.PathPoint(sNear);
        glm::vec3 beforeStep = dd.position;
        for (int i = 0; i < 30; ++i) gg.Update(1.0f / 60.0f, s, Input(), YAW);
        CHECK(glm::length(onLoop - beforeStep) < 0.2f && glm::length(dd.position - beforeStep) > 0.1f, "after returning the drone is on its patrol loop and moving along it again");
        // NearestPathS / PathPoint sanity: a point on the loop projects onto itself
        Bulb probe = s.bulbs[0]; probe.position = probe.PathPoint(5.5f);
        CHECK(std::fabs(probe.NearestPathS() - 5.5f) < 0.05f, "NearestPathS projects a point on the loop back to its own s");
    }
    {   // a wrong code at the door sends the NEAREST free drone to the door
        Scene s = sc; Game gg; gg.SeedRng(4); gg.Init(s); gg.state = GameState::Playing;
        gg.playerPos = glm::vec3(s.exitPos.x - 0.2f, 0, s.exitPos.y);
        // drone_b patrols the east side (6..13, 3..-9): it is nearer to the exit than drone_a
        gg.entered = { (gg.code[0] + 1) % 10, gg.code[1], gg.code[2], gg.code[3] };
        gg.SubmitCode(s);
        bool bGoes = s.bulbs[1].mode == BulbMode::Investigate && glm::length(glm::vec2(s.bulbs[1].investigateTarget.x, s.bulbs[1].investigateTarget.z) - s.exitPos) < 1e-3f;
        CHECK(bGoes && s.bulbs[0].mode == BulbMode::Patrol, "a wrong code sends the nearest drone (and only that one) to investigate the door");
        // the attract mode on the title screen keeps drones on plain patrol
        Scene s2 = sc; Game g2; g2.Init(s2); g2.state = GameState::Title;
        glm::vec3 p0 = s2.bulbs[0].position; for (int i = 0; i < 60; ++i) g2.Update(1.0f / 60.0f, s2, Input(), YAW);
        CHECK(s2.bulbs[0].mode == BulbMode::Patrol && glm::length(s2.bulbs[0].position - p0) > 0.5f, "title screen attract mode: drones patrol normally");
    }
    // ---------------- strikes destroy cover ----------------
    {
        Scene s = sc; Game gg; gg.Init(s); gg.state = GameState::Playing;
        auto findObj = [&](Scene& sc2, const char* id) -> SceneObject& { for (auto& o : sc2.objects) if (o.id == id) return o; return sc2.objects[0]; };
        CHECK(findObj(s, "crate_1").destructible && findObj(s, "barrier_1").destructible, "crates and barriers are marked destructible");
        bool onlyThose = true;
        for (const auto& o : s.objects) if (o.destructible && o.id.rfind("crate_", 0) != 0 && o.id.rfind("barrier_", 0) != 0) onlyThose = false;
        CHECK(onlyThose, "walls, pillars, scrap stacks and terminals are NEVER destructible");
        for (auto& o : s.objects) if (o.id.rfind("scrap_", 0) == 0 || o.id.rfind("wall_", 0) == 0 || o.id.rfind("pillar_", 0) == 0 || o.id.rfind("terminal_", 0) == 0) { if (o.destructible) onlyThose = false; }

        SceneObject& crate = findObj(s, "crate_1");            // (-4.5, 0.6, 11), 2 x 2
        glm::vec3 beyond(-4.5f, SAMPLE_HEIGHT, 12.4f);          // a floor point on the far side of the crate from the scanner below
        Scene s2 = s;
        // a light at (-4.5, 6.5, 8) looking at the floor: the crate blocks the point behind it
        for (auto& b : s2.bulbs) b.power = 0.0f;
        Bulb& lamp = s2.bulbs[D0]; lamp.power = 1.0f; lamp.mount = glm::vec3(-4.5f, 6.5f, 8.2f); lamp.Place(); lamp.aim = glm::vec2(-4.5f, 12.0f);
        bool blockedBefore = SegmentBlocked(s2, beyond, lamp.position);
        // strike on the crate: put scanner 0 into Locked just before the strike, aim on the crate, droid far away
        Bulb& sb = s.bulbs[D0];
        sb.state = ScanState::Locked; sb.stateTime = s.strikeDelay - 0.01f; sb.aim = glm::vec2(-4.5f, 11.0f); sb.route[0] = { sb.aim }; sb.wp = 0;
        gg.playerPos = glm::vec3(0.0f, 0, -13.0f);
        int v0 = gg.coverVersion;
        gg.Update(1.0f / 30.0f, s, Input(), YAW);
        SceneObject& crateAfter = findObj(s, "crate_1");
        CHECK(crateAfter.destroyed && !crateAfter.solid && !crateAfter.castShadow, "a strike within strike.radius destroys a crate: not solid, casts no shadow");
        CHECK(!findObj(s, "scrap_1").destroyed && findObj(s, "scrap_1").solid, "...but a scrap stack at the same distance survives");
        CHECK(gg.coverVersion > v0 && gg.scorchMarks.size() == 1, "coverVersion is bumped and a scorch mark is recorded");
        CHECK(gg.messageTime > 0.0f && gg.message == "COVER DESTROYED", "the HUD message COVER DESTROYED is raised");
        // collision: the droid can now walk through where the crate stood
        gg.playerPos = glm::vec3(-4.5f, 0, 11.0f); gg.Update(1.0f / 60.0f, s, Input(), YAW);
        CHECK(std::fabs(gg.playerPos.x + 4.5f) < 0.5f && std::fabs(gg.playerPos.z - 11.0f) < 0.5f, "collision no longer pushes the droid out of the destroyed crate");
        CHECK(!PointInsideSolid(s, -4.5f, 11.0f, 0.0f) && PointInsideSolid(sc, -4.5f, 11.0f, 0.0f), "PointInsideSolid respects destroyed objects");
        // lit test: the shadow behind the crate is gone
        Scene s3 = s2; for (auto& o : s3.objects) if (o.id == "crate_1") { o.destroyed = true; o.solid = false; o.castShadow = false; }
        CHECK(blockedBefore && !SegmentBlocked(s3, beyond, lamp.position), "the CPU lit-test no longer sees the destroyed crate (the shadow disappears)");
        // Reset restores everything
        int v1 = gg.coverVersion;
        gg.Reset(s);
        CHECK(!findObj(s, "crate_1").destroyed && findObj(s, "crate_1").solid && findObj(s, "crate_1").castShadow && gg.scorchMarks.empty() && gg.coverVersion > v1,
              "Reset restores destroyed cover, clears the scorch marks and bumps coverVersion so the shadow cache refreshes");
    }
    // ---------------- lamp battery ----------------
    {
        Scene s = sc; s.bulbs.clear(); s.terminals.clear(); s.oilPuddles.clear(); s.conveyors.clear();
        Game gg; gg.Init(s); gg.state = GameState::Playing;
        CHECK(gg.flashOn && std::fabs(gg.battery - 1.0f) < 1e-6f, "the lamp starts on with a full battery");
        for (int i = 0; i < 60 * 10; ++i) gg.Update(1.0f / 60.0f, s, Input(), YAW);
        std::printf("      after 10 s on: battery %.3f (expected %.3f)\n", gg.battery, 1.0f - 10.0f * s.lampDrain);
        CHECK(std::fabs(gg.battery - (1.0f - 10.0f * s.lampDrain)) < 0.01f, "the lamp drains lamp_drain_per_sec while on");
        for (int i = 0; i < 60 * 15 + 6; ++i) gg.Update(1.0f / 60.0f, s, Input(), YAW);   // 25.1 s total: just past 1 / 0.04 = 25 s
        CHECK(!gg.flashOn && gg.battery < 0.01f && gg.lampLockout, "at 0 the lamp is forced off and locked out");
        CHECK(!gg.ToggleLamp() && !gg.flashOn, "a flat lamp cannot be switched on");
        float need = s.lampMinOn / s.lampRecharge;                           // seconds to get back to min_to_turn_on
        for (int i = 0; i < (int)((need - 1.0f) * 60.0f); ++i) gg.Update(1.0f / 60.0f, s, Input(), YAW);
        CHECK(!gg.ToggleLamp(), "still refused just below the turn-on threshold");
        for (int i = 0; i < 2 * 60; ++i) gg.Update(1.0f / 60.0f, s, Input(), YAW);
        CHECK(gg.battery >= s.lampMinOn && !gg.lampLockout && gg.ToggleLamp() && gg.flashOn, "once recharged to min_to_turn_on the lamp can be switched on again");
        // flicker: a low battery makes the lamp stutter below 1.0
        bool dipped = false; float maxP = 0.0f;
        for (int i = 0; i < 120; ++i) { gg.Update(1.0f / 60.0f, s, Input(), YAW); dipped = dipped || gg.lampPower < 0.99f; maxP = std::max(maxP, gg.lampPower); }
        CHECK(dipped && maxP < 1.0f, "below flicker_below the lamp power dips below 1.0");
        // a manual switch-off recharges
        gg.ToggleLamp(); float b0 = gg.battery; for (int i = 0; i < 120; ++i) gg.Update(1.0f / 60.0f, s, Input(), YAW);
        CHECK(!gg.flashOn && gg.battery > b0, "with the lamp off the battery recharges");
        gg.Reset(s);
        CHECK(gg.flashOn == s.flashOn && std::fabs(gg.battery - 1.0f) < 1e-6f && !gg.lampLockout && gg.lampPower == 1.0f, "Reset restores the battery and the lamp");
        // the lamp is NOT part of the detection test
        Scene s2 = sc; Game g2; g2.Init(s2); g2.state = GameState::Playing;
        g2.playerPos = glm::vec3(0.0f, 0, 12.8f);
        LitResult a = ComputeIllumination(s2, g2.SamplePoint(), true); g2.flashOn = !g2.flashOn;
        LitResult b = ComputeIllumination(s2, g2.SamplePoint(), true);
        CHECK(a.illum == b.illum, "switching the lamp on or off does not change the lit test");
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
