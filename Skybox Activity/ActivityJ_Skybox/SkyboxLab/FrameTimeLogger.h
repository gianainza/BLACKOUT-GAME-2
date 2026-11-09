#pragma once
// R5 - QA and Performance: whole-frame CPU wall-clock timing harness.
// C++14, standard library only. No OpenGL dependency in this file.
//
// Written by R5. Extended (with AI assistance, declared in docs/DECISION_LOG.md) so that it can be run again and again
// from the game (key T) instead of once per program launch:
//   * Start() / Cancel()   start a run for a named scene; each run writes its own CSV of all 300 frame times
//   * a summary row is APPENDED to <prefix>_summary.csv after every run (scene, GPU, cache, cube-map size, mean, median,
//     P95, min, max, plus the average shadow-pass GPU time and shadow maps rebuilt per frame): that file is the raw
//     material for the three-machine frame-time table (T12)
//   * RecordExtras() adds those two numbers per measured frame
// What it measures is unchanged: BeginFrame() at the top of the main loop, EndFrame() after the buffer swap, so the
// time includes the swap (and any vsync wait - turn vsync off first, or every machine will read about 16.7 ms).

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

class FrameTimeLogger {
public:
    static constexpr std::size_t WarmupFrames = 120;
    static constexpr std::size_t MeasurementFrames = 300;

    // filePrefix: run files are  <prefix>_<scene>_run<N>.csv  and the summary is  <prefix>_summary.csv
    explicit FrameTimeLogger(std::string filePrefix = "frame_times")
        : prefix_(std::move(filePrefix)) {
        samplesMs_.reserve(MeasurementFrames);
    }

    // Begin a run. 'scene' names it (e.g. "title", "play", "play_nocache"); the other arguments are written to the summary.
    void Start(const std::string& scene, const std::string& gpuName, bool cacheOn, int omniRes) {
        scene_ = Sanitize(scene);
        gpu_ = gpuName;
        cacheOn_ = cacheOn;
        omniRes_ = omniRes;
        ++runCounter_;
        samplesMs_.clear();
        framesSeen_ = 0;
        frameStarted_ = false;
        extraFrames_ = 0;
        shadowSumMs_ = 0.0;
        mapsSum_ = 0.0;
        finished_ = false;
        running_ = true;
        std::cout << "[R5] Run " << runCounter_ << " (" << scene_ << ") started: " << WarmupFrames
                  << " warm-up frames, then " << MeasurementFrames << " measured frames. GPU: " << gpu_ << "\n";
    }

    void Cancel() {
        if (!running_) return;
        running_ = false;
        frameStarted_ = false;
        std::cout << "[R5] Run cancelled, nothing was saved.\n";
    }

    void BeginFrame() {
        if (!running_) return;
        frameStart_ = Clock::now();
        frameStarted_ = true;
    }

    // Optional, call once per frame BEFORE EndFrame(): the shadow-pass GPU time and how many shadow maps were rendered.
    void RecordExtras(double shadowGpuMs, int mapsRebuilt) {
        if (!running_ || framesSeen_ < WarmupFrames || samplesMs_.size() >= MeasurementFrames) return;
        shadowSumMs_ += shadowGpuMs;
        mapsSum_ += mapsRebuilt;
        ++extraFrames_;
    }

    void EndFrame() {
        if (!running_ || !frameStarted_) return;

        const auto frameEnd = Clock::now();
        frameStarted_ = false;
        const double milliseconds =
            std::chrono::duration<double, std::milli>(frameEnd - frameStart_).count();

        ++framesSeen_;
        if (framesSeen_ <= WarmupFrames) return;
        samplesMs_.push_back(milliseconds);

        if (samplesMs_.size() == MeasurementFrames) {
            WriteResults();
            running_ = false;
            finished_ = true;
        }
    }

    bool IsRunning() const { return running_; }
    bool IsWarmingUp() const { return running_ && framesSeen_ < WarmupFrames; }
    bool IsFinished() const { return finished_; }                       // the last run completed and was saved
    std::size_t MeasuredFrames() const { return samplesMs_.size(); }    // 0..300 (for a HUD progress line)

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point frameStart_{};
    std::string prefix_;
    std::string scene_;
    std::string gpu_;
    bool cacheOn_ = true;
    int omniRes_ = 0;
    int runCounter_ = 0;
    std::vector<double> samplesMs_;
    std::size_t framesSeen_ = 0;
    std::size_t extraFrames_ = 0;
    double shadowSumMs_ = 0.0;
    double mapsSum_ = 0.0;
    bool frameStarted_ = false;
    bool running_ = false;
    bool finished_ = false;

    static std::string Sanitize(const std::string& s) {
        std::string out;
        for (char c : s) out += (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') ? c : '_';
        return out.empty() ? std::string("run") : out;
    }

    static std::string CsvQuote(const std::string& s) {
        std::string out = "\"";
        for (char c : s) { if (c == '"') out += '"'; out += c; }
        return out + "\"";
    }

    void WriteResults() const {
        const std::size_t n = samplesMs_.size();
        const double total = std::accumulate(samplesMs_.begin(), samplesMs_.end(), 0.0);
        const double mean = total / static_cast<double>(n);
        std::vector<double> sorted = samplesMs_;
        std::sort(sorted.begin(), sorted.end());
        const double median = (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;                       // n is even (300)
        const double p95 = sorted[static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(n))) - 1];   // zero-based
        const double shadowAvg = extraFrames_ ? shadowSumMs_ / static_cast<double>(extraFrames_) : 0.0;
        const double mapsAvg = extraFrames_ ? mapsSum_ / static_cast<double>(extraFrames_) : 0.0;

        // File writing happens only AFTER the timed 300 frames.
        const std::string runPath = prefix_ + "_" + scene_ + "_run" + std::to_string(runCounter_) + ".csv";
        std::ofstream csv(runPath.c_str(), std::ios::out | std::ios::trunc);
        if (!csv.is_open()) {
            std::cerr << "[R5] ERROR: Could not write " << runPath
                      << ". Check the Visual Studio working directory.\n";
        } else {
            csv << "frame,frame_time_ms\n";
            csv << std::fixed << std::setprecision(4);
            for (std::size_t i = 0; i < samplesMs_.size(); ++i) {
                csv << (i + 1) << ',' << samplesMs_[i] << '\n';
            }
            csv.close();
            if (!csv) {
                std::cerr << "[R5] ERROR: Could not finish writing " << runPath << "\n";
            } else {
                std::cout << "[R5] Saved 300 frame times to " << runPath << "\n";
            }
        }

        // one summary row per run, appended: this is what the T12 table is built from
        const std::string summaryPath = prefix_ + "_summary.csv";
        bool needHeader = true;
        {
            std::ifstream probe(summaryPath.c_str());
            needHeader = !(probe.good() && probe.peek() != std::ifstream::traits_type::eof());
        }
        std::ofstream sum(summaryPath.c_str(), std::ios::out | std::ios::app);
        if (!sum.is_open()) {
            std::cerr << "[R5] ERROR: Could not append to " << summaryPath << "\n";
        } else {
            if (needHeader)
                sum << "date_time,run,scene,gpu,cache,omni_res,mean_ms,median_ms,p95_ms,min_ms,max_ms,fps_equivalent,shadow_gpu_ms_avg,maps_rebuilt_avg\n";
            char stamp[32] = "";
            const std::time_t now = std::time(nullptr);
            std::strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
            sum << stamp << ',' << runCounter_ << ',' << scene_ << ',' << CsvQuote(gpu_) << ',' << (cacheOn_ ? "on" : "off") << ','
                << omniRes_ << std::fixed << std::setprecision(4) << ',' << mean << ',' << median << ',' << p95 << ','
                << sorted.front() << ',' << sorted.back() << ',' << (1000.0 / mean) << ',' << shadowAvg << ',' << mapsAvg << '\n';
        }

        std::cout << std::fixed << std::setprecision(3)
                  << "[R5] Mean: " << mean << " ms/frame (" << (1000.0 / mean)
                  << " FPS equivalent), median: " << median
                  << " ms, P95: " << p95 << " ms, min: " << sorted.front()
                  << " ms, max: " << sorted.back() << " ms.\n";
        std::cout << "[R5] Shadow passes: " << shadowAvg << " ms GPU avg, " << mapsAvg
                  << " maps rebuilt per frame, cache " << (cacheOn_ ? "ON" : "OFF") << ", cube map " << omniRes_ << "^2. Summary row appended to "
                  << summaryPath << "\n";
        std::cout << "[R5] CPU-observed loop time, including buffer swap; "
                     "NOT an isolated GPU timer (the shadow-pass figure above is the GPU timer).\n";
    }
};
