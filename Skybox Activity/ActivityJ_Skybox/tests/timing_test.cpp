// Headless test of FrameTimeLogger (no OpenGL, standard library only).
//   cd SkyboxLab
//   g++ -std=c++14 -I. ../tests/timing_test.cpp -o timing_test && ./timing_test
#include "FrameTimeLogger.h"
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <thread>

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { ++fails; std::printf("FAIL: %s\n", msg); } else std::printf("ok:   %s\n", msg); } while (0)

static void Frames(FrameTimeLogger& L, int n, int sleepUs)
{
    for (int i = 0; i < n; ++i)
    {
        L.BeginFrame();
        std::this_thread::sleep_for(std::chrono::microseconds(sleepUs + (i % 7) * 150));   // a slightly uneven "frame"
        L.RecordExtras(1.5, 3);
        L.EndFrame();
    }
}

int main()
{
    std::remove("tt_summary.csv");
    FrameTimeLogger L("tt");

    // nothing is recorded until Start()
    Frames(L, 50, 100);
    CHECK(!L.IsRunning() && !L.IsFinished(), "idle: frames before Start() are ignored");

    L.Start("play", "Test GPU, \"quoted\"", true, 1024);
    CHECK(L.IsRunning() && L.IsWarmingUp(), "Start() begins a run in warm-up");
    Frames(L, (int)FrameTimeLogger::WarmupFrames, 200);
    CHECK(!L.IsWarmingUp() && L.MeasuredFrames() == 0, "after 120 frames the warm-up is over and nothing has been measured yet");
    Frames(L, 150, 200);
    CHECK(L.IsRunning() && L.MeasuredFrames() == 150, "measuring: 150 of 300 frames");
    Frames(L, 150, 200);
    CHECK(!L.IsRunning() && L.IsFinished(), "after 300 measured frames the run stops by itself and is saved");

    // the run CSV has exactly 300 rows; recompute the statistics independently
    std::vector<double> v;
    {
        std::ifstream f("tt_play_run1.csv"); std::string line; std::getline(f, line);
        CHECK(line == "frame,frame_time_ms", "run CSV has the header");
        while (std::getline(f, line)) { size_t c = line.find(','); v.push_back(std::atof(line.c_str() + c + 1)); }
    }
    CHECK(v.size() == 300, "run CSV holds exactly 300 frame times (warm-up excluded)");
    double mean = 0; for (double x : v) mean += x; mean /= v.size();
    std::vector<double> s = v; std::sort(s.begin(), s.end());
    double median = (s[149] + s[150]) / 2.0, p95 = s[284];

    std::ifstream sf("tt_summary.csv"); std::string hdr, row; std::getline(sf, hdr); std::getline(sf, row);
    CHECK(hdr.find("mean_ms,median_ms,p95_ms") != std::string::npos, "summary CSV has a header");
    CHECK(row.find("\"Test GPU, \"\"quoted\"\"\"") != std::string::npos, "GPU name with a comma and quotes is escaped correctly");
    // fields after the quoted GPU name: cache,omni_res,mean,median,p95,min,max,fps,shadow,maps
    size_t after = row.rfind("\"\"\",");
    std::stringstream ss(row.substr(after + 4)); std::string cell; std::vector<std::string> cells;
    while (std::getline(ss, cell, ',')) cells.push_back(cell);
    CHECK(cells.size() == 10 && cells[0] == "on" && cells[1] == "1024", "summary row records cache state and cube-map size");
    CHECK(std::fabs(std::atof(cells[2].c_str()) - mean) < 1e-3, "summary mean matches an independent recomputation");
    CHECK(std::fabs(std::atof(cells[3].c_str()) - median) < 1e-3, "summary median matches");
    CHECK(std::fabs(std::atof(cells[4].c_str()) - p95) < 1e-3, "summary P95 matches (index ceil(0.95*300)-1 = 284)");
    CHECK(std::fabs(std::atof(cells[8].c_str()) - 1.5) < 1e-6 && std::fabs(std::atof(cells[9].c_str()) - 3.0) < 1e-6, "average shadow GPU ms and maps rebuilt are recorded");

    // a second run appends a row and writes its own file; a cancelled run saves nothing
    L.Start("play_nocache", "Test GPU", false, 512);
    Frames(L, 120 + 300, 150);
    L.Start("title", "Test GPU", true, 1024);
    Frames(L, 200, 150);
    L.Cancel();
    CHECK(!L.IsRunning(), "Cancel() stops a run");
    std::ifstream f2("tt_play_nocache_run2.csv"); CHECK(f2.good(), "second run wrote its own CSV");
    std::ifstream f3("tt_title_run3.csv"); CHECK(!f3.good(), "a cancelled run saved no CSV");
    int rows = 0; { std::ifstream a("tt_summary.csv"); std::string l; while (std::getline(a, l)) ++rows; }
    CHECK(rows == 3, "summary file: one header + one appended row per finished run");

    std::remove("tt_play_run1.csv"); std::remove("tt_play_nocache_run2.csv"); std::remove("tt_summary.csv");
    std::printf("\n%s (%d failure(s))\n", fails ? "SOME TESTS FAILED" : "ALL TESTS PASSED", fails);
    return fails ? 1 : 0;
}
