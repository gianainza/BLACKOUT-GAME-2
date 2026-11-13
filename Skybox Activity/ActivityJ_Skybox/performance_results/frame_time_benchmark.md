# Frame-Time Benchmarking Report

**Project:** BLACKOUT  
**Activity:** Skybox Activity  
**Role:** R5 – QA and Performance  
**Benchmark Type:** Frame-Time Performance Testing  
**Test Scene:** Play (`level01.json`)

---

## 1. Benchmark Overview

The purpose of this benchmarking activity is to measure the application's frame-time performance under normal gameplay conditions and compare the results across different hardware configurations.

The benchmark uses the project's built-in `FrameTimeLogger` to collect frame-time measurements and calculate performance statistics, including mean, median, 95th percentile (P95), minimum, maximum, and FPS equivalent.

Due to limited hardware availability, testing was conducted on two physical computers instead of the originally intended three.

## 2. Testing Methodology

The built-in frame-time logger was used to collect performance measurements.

The logger uses the following procedure:

- **Warm-up frames:** 120 frames.
- **Measured frames:** 300 frames.
- **Timing method:** CPU-observed application loop time using `steady_clock`.
- **Additional measurement:** GPU time spent on shadow passes.

The frame-time measurement includes the application loop and buffer swapping. Therefore, it does not represent isolated GPU rendering time.

### Intended Benchmark Settings

| Setting | Configuration |
|---|---|
| Scene | Play |
| Vertical Synchronization (VSync) | OFF |
| Shadow-map Cache | ON |
| Cube Map Resolution | 1024 × 1024 |
| Warm-up Frames | 120 |
| Measured Frames | 300 |

Machine 1 was tested using the settings above. Machine 2's VSync and shadow-cache settings could not be independently confirmed before the reporting deadline.

## 3. Frame-Time Benchmark Results

### 3.1 Machine 1 – NVIDIA GeForce GTX 1050 Ti

Two valid benchmark runs were completed on the same computer.

| Metric | Trial 1 | Trial 2 |
|---|---:|---:|
| Mean Frame Time (ms) | 57.715 | 60.350 |
| Median Frame Time (ms) | 56.328 | 60.290 |
| P95 Frame Time (ms) | 79.129 | 80.383 |
| Minimum Frame Time (ms) | 7.769 | 41.538 |
| Maximum Frame Time (ms) | 159.313 | 96.483 |
| FPS Equivalent | 17.326 | 16.570 |
| Average Shadow GPU Time (ms) | 2.297 | 1.847 |
| Average Maps Rebuilt per Frame | 2.540 | 2.477 |
| VSync | OFF | OFF |
| Shadow Cache | ON | ON |

Both trials completed all 300 measured frames successfully.

The average of the two mean frame-time results was **59.033 ms**, equivalent to approximately **16.940 FPS**.

The initial benchmark attempt was excluded because VSync was enabled during that run.

### 3.2 Machine 2 – NVIDIA GeForce GTX 1080 Ti (Assumed)

A second computer supplied 300 individual frame-time measurements.

The GPU model is an estimate and was not independently verified because the teammate who performed the test was unavailable before the reporting deadline.

| Metric | Result |
|---|---:|
| Mean Frame Time (ms) | 38.043 |
| Median Frame Time (ms) | 38.129 |
| P95 Frame Time (ms) | 42.733 |
| Minimum Frame Time (ms) | 32.019 |
| Maximum Frame Time (ms) | 47.195 |
| FPS Equivalent | 26.286 |
| Average Shadow GPU Time | Not Available |
| Average Maps Rebuilt per Frame | Not Available |
| VSync | Unconfirmed |
| Shadow Cache | Unconfirmed |

The statistics were calculated directly from the 300 frame-time measurements supplied by the teammate.

### 3.3 Combined Benchmark Comparison

| Machine | GPU | Mean (ms) | Median (ms) | P95 (ms) | FPS Equivalent |
|---|---|---:|---:|---:|---:|
| Machine 1 – Trial 1 | GTX 1050 Ti | 57.715 | 56.328 | 79.129 | 17.326 |
| Machine 1 – Trial 2 | GTX 1050 Ti | 60.350 | 60.290 | 80.383 | 16.570 |
| Machine 2 – Trial 1 | GTX 1080 Ti (Assumed) | 38.043 | 38.129 | 42.733 | 26.286 |

## 4. Performance Analysis

The two benchmark trials on Machine 1 produced mean frame times of 57.715 ms and 60.350 ms. Their average mean frame time was 59.033 ms.

Machine 2 recorded a mean frame time of 38.043 ms, which is approximately 35.6% lower than Machine 1's average mean frame time.

Machine 2 also recorded a higher FPS equivalent of approximately 26.286 FPS, compared with Machine 1's average of approximately 16.940 FPS.

The results indicate that Machine 2 completed the measured application loop faster under its recorded test conditions.

However, the comparison should be interpreted cautiously because Machine 2's hardware identification and benchmark settings were not independently verified.

Machine 1's two trials also demonstrate some variation in frame-time performance, emphasizing the importance of consistent testing conditions when comparing hardware.

## 5. Benchmarking Limitations

The following limitations were encountered during testing:

1. **Limited Hardware Availability:** Only two physical computers were available for benchmarking instead of the originally intended three.

2. **Unverified Machine 2 Hardware:** The NVIDIA GeForce GTX 1080 Ti identification is an estimate and was not confirmed through the logger's GPU identification output.

3. **Unconfirmed Machine 2 Settings:** VSync and shadow-cache settings could not be independently verified for the second computer.

4. **Different Number of Trials:** Machine 1 completed two valid trials, while Machine 2 supplied one set of 300 measurements.

5. **Frame-Time Measurement Scope:** The logger records CPU-observed application loop time, including buffer swapping. It is not an isolated measurement of total GPU rendering time.

6. **Unlit Baseline Verification:** The collected measurements represent the normal Play scene. A separate unlit rendering configuration was not independently verified during this activity.

Despite these limitations, the collected measurements provide useful preliminary information about the application's frame-time performance.

## 6. Conclusion

The frame-time benchmarking activity successfully collected performance data from two physical computers using the project's built-in timing system.

Machine 1, equipped with an NVIDIA GeForce GTX 1050 Ti, completed two valid benchmark trials with VSync disabled and shadow caching enabled. Machine 2 supplied 300 frame-time measurements that produced a lower mean frame time and higher FPS equivalent than Machine 1.

Although the original benchmarking target of three different machines was not achieved due to hardware availability, the collected results provide a preliminary comparison of application performance across two systems.

The benchmarking process also demonstrated the functionality of the project's frame-time logger, including its warm-up period, measurement collection, statistical calculations, and CSV output.

All reported results and limitations are documented to maintain transparency and reproducibility.

---

**R5 Deliverable:** Frame-Time Benchmarking Table  
**Supporting Evidence:** CSV frame-time measurements and benchmark console output where available.
