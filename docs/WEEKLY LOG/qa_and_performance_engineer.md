# R5 - WEEKLY LOGS (WEEKS 1–5)

**Name:** Misa, Cedric Jeremy
**Role:** R5 – QA and Performance  
**Project:** BLACKOUT – Skybox Activity

---

## WEEK 1 – PROJECT FAMILIARIZATION AND QA PLANNING

**Dates:** Oct 28 – Oct 30  
**Hours this week:** 3

### WHAT I DID
* Reviewed the existing BLACKOUT project structure and available project documentation to understand the application's functionality and testing requirements.
* Examined the File Ownership document to identify the responsibilities assigned to R5 and avoid modifying files owned by other team members.
* Reviewed the State Machine documentation to understand the expected behavior of the Title, Playing, Paused, Won, and Lost states.
* Identified the main testing categories needed for quality assurance, including state transitions, lighting, detection, shadows, and performance.

### WHAT I FINISHED
* Established the scope of R5's QA and performance responsibilities.
* Prepared the initial structure of the Test Matrix (`test_matrix.md`).
* Organized the testing approach according to the documented game mechanics and expected behaviors.

### WHAT I TRIED THAT DID NOT WORK
* Initially needed to familiarize myself with the project's existing documentation and code structure before identifying the appropriate testing procedures.
* Reviewed the existing documentation and clarified the responsibilities assigned to R5 before proceeding with testing.

### WHAT I AM WAITING ON, AND FROM WHOM
* No major dependencies. Continued with the testing activities using the existing project build and documentation.

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME
* Nothing. QA planning and test preparation did not require modifications to other members' source files.

### FILES I CHANGED THIS WEEK
* `tests/test_matrix.md`

### NEXT WEEK
* Verify the project build, launch the application, and begin testing the documented state transitions and gameplay behavior.

---

## WEEK 2 – BUILD VERIFICATION AND TEST ENVIRONMENT SETUP

**Dates:** Oct 31 - Nov 3 
**Hours this week:** 4

### WHAT I DID
* Built and launched the BLACKOUT application through Visual Studio Community to verify that the project could run on my machine.
* Investigated build and runtime issues encountered during the initial setup.
* Reviewed the frame-time logging implementation, particularly `FrameTimeLogger.h`, `Timing.h`, and `Timing.cpp`, to understand how performance measurements were collected.
* Inspected the relevant portions of `main.cpp` to identify the available benchmarking controls and understand how the logger records frame times.
* Kept `main.cpp` and other members' source files unchanged, using them for inspection only.

### WHAT I FINISHED
* Successfully built and launched the application after resolving local environment setup issues.
* Resolved a local GLEW library conflict by removing the conflicting NuGet package from my local environment.
* Installed the required Microsoft Visual C++ 2013 Redistributable to resolve the missing `MSVCR120.dll` runtime dependency.
* Confirmed that the application could launch and that the built-in frame-time logger was available.

### WHAT I TRIED THAT DID NOT WORK
* Encountered duplicate GLEW library definitions during linking, resulting in linker errors.
* Initially encountered a missing `MSVCR120.dll` error when launching the application.
* Resolved both problems through local dependency and runtime setup without modifying the shared project source files.

### WHAT I AM WAITING ON, AND FROM WHOM
* No major dependencies. Continued with functional testing after confirming that the application launched successfully.

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME
* Nothing. The local setup issues did not require changes to other members' assigned files.

### FILES I CHANGED THIS WEEK
* No shared project source files modified.
* Local Visual Studio and NuGet environment configuration only.

### NEXT WEEK
* Execute the state-machine test cases and document the actual results in the Test Matrix.

---

## WEEK 3 – STATE MACHINE AND GAMEPLAY TESTING

**Dates:** Nov 4 - Nov 6  
**Hours this week:** 3

### WHAT I DID
* Executed the state-machine test cases to verify the application's primary gameplay transitions.
* Tested the transition from Title to Playing by pressing Enter.
* Verified pause and resume behavior using the P key.
* Tested restarting the game during active gameplay and after losing.
* Tested the victory condition by reaching the exit area.
* Tested the loss condition by remaining within the detection radius until the delayed strike landed.
* Tested whether moving outside the detection radius before the strike prevented the player from losing.
* Recorded test observations and results in the Test Matrix (`test_matrix.md`).

### WHAT I FINISHED
* Completed eight State Machine test cases, SM-01 through SM-08.
* Successfully verified the following behaviors:
  - Title to Playing transition.
  - Playing to Paused transition.
  - Paused to Playing transition.
  - Restart during active gameplay.
  - Victory condition.
  - Loss condition.
  - Avoiding an incoming strike.
  - Restarting after losing.
* Recorded all eight state-machine tests as PASS.
* Documented an observation during the pause test: the gameplay world freezes, while mouse-controlled camera movement remains responsive.

### WHAT I TRIED THAT DID NOT WORK
* No confirmed state-machine or gameplay defects were identified during the eight tests.
* Observed that the camera remained responsive while the game was paused. Since the documentation did not explicitly require the camera to freeze, this was recorded as an observation rather than a confirmed bug.

### WHAT I AM WAITING ON, AND FROM WHOM
* No major dependencies. Proceeded with the remaining lighting and detection tests.

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME
* Nothing. The completed state-machine results were documented for the project QA records.

### FILES I CHANGED THIS WEEK
* `tests/test_matrix.md`
* State Machine test evidence stored in `tests/evidence/`, where available.

### NEXT WEEK
* Test the lighting and detection systems, verify delayed strike behavior, and record any issues encountered.

---

## WEEK 4 – LIGHTING TESTING AND BUG DOCUMENTATION

**Dates:** Nov 7 - Nov 9  
**Hours this week:** 3

### WHAT I DID
* Conducted lighting and detection tests to evaluate how enemy scanners detect the player.
* Verified that the detection meter increases when the player enters an enemy scanner's illumination range.
* Observed the scanner behavior after detection reached 1.0, including the transition to its locked state and subsequent attack.
* Measured the approximate delay between the scanner locking onto the player and firing its attack.
* Tested whether moving outside the detection radius before the attack prevented the loss condition.
* Verified whether the player's own lamp contributed to the enemy detection meter.
* Updated the Test Matrix with the results of the lighting tests.
* Documented local environment issues and additional QA observations in the Bug Log.

### WHAT I FINISHED
* Completed six Lighting and Detection test cases, LT-01 through LT-06.
* Successfully verified:
  - Scanner detection and detection meter behavior.
  - Detection reaching 1.0 and triggering the locked state.
  - Delayed strike behavior of approximately 1.9–2.0 seconds before firing.
  - Avoiding a strike by moving outside the detection radius.
  - Losing when remaining within the strike radius.
  - Exclusion of the player's own lamp from enemy detection.
* Recorded all six lighting tests as PASS.
* Completed 14 successful state-machine and lighting tests in total.
* Documented the local GLEW linking conflict and missing runtime dependency as environment setup issues rather than confirmed project defects.
* Recorded the responsive camera during pause as an additional QA observation.

### WHAT I TRIED THAT DID NOT WORK
* No confirmed lighting or detection defects were identified during testing.
* Encountered difficulty capturing some time-sensitive gameplay events using screenshots, so video evidence was considered more suitable for documenting certain event sequences.

### WHAT I AM WAITING ON, AND FROM WHOM
* No major dependencies. Continued with performance benchmarking using the existing frame-time logger.

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME
* Nothing. The testing results and observations were documented without requiring changes to other members' implementation files.

### FILES I CHANGED THIS WEEK
* `tests/test_matrix.md`
* `tests/bug_log.md`
* `tests/evidence/` (available test screenshots and recordings)

### NEXT WEEK
* Conduct frame-time benchmarking, collect performance measurements, and prepare the R5 performance report.

---

## WEEK 5 – PERFORMANCE BENCHMARKING AND REPORTING

**Dates:** Nov 10 - Nov 11  
**Hours this week:** 3

### WHAT I DID
* Inspected the existing `FrameTimeLogger` implementation to understand its warm-up period, measurement process, and CSV output.
* Identified that the logger uses 120 warm-up frames followed by 300 measured frames.
* Conducted frame-time benchmarking on a computer equipped with an NVIDIA GeForce GTX 1050 Ti.
* Disabled VSync and kept shadow caching enabled for the valid benchmark runs.
* Collected two valid benchmark runs on the same computer.
* Obtained another set of 300 frame-time measurements from a teammate's computer.
* Calculated the mean, median, P95, minimum, maximum, and FPS equivalent from the teammate's 300 measurements.
* Compared the available results and organized the measurements into a benchmarking table.
* Prepared the Frame-Time Benchmarking Report and documented hardware and testing limitations.

### WHAT I FINISHED
* Completed two valid 300-frame benchmark runs on the NVIDIA GeForce GTX 1050 Ti.
* Recorded the following valid Machine 1 results:

  - Run 1: Mean of 57.715 ms/frame and 17.326 FPS equivalent.
  - Run 2: Mean of 60.350 ms/frame and 16.570 FPS equivalent.
  - Average mean frame time: 59.033 ms.

* Obtained and analyzed 300 frame-time measurements from a second computer.
* Calculated the following Machine 2 statistics:

  - Mean: 38.043 ms/frame.
  - Median: 38.129 ms.
  - P95: 42.733 ms.
  - Minimum: 32.019 ms.
  - Maximum: 47.195 ms.
  - FPS equivalent: 26.286.

* Organized available CSV files in the `performance_results/` directory.
* Prepared `frame_time_benchmark.md` containing the benchmark methodology, performance results, analysis, limitations, and conclusion.
* Documented that only two physical computers were available instead of the originally intended three.

### WHAT I TRIED THAT DID NOT WORK
* The initial benchmark attempt on Machine 1 was performed while VSync was enabled, making it unsuitable for the intended VSync-off comparison. Repeated the benchmark with VSync disabled and excluded the initial run from the valid results.
* Initially had difficulty identifying the second computer's GPU and verifying its benchmark settings because the teammate was unavailable before the reporting deadline.
* Could not obtain a third physical computer for the originally intended three-machine comparison.
* The unlit rendering configuration was not independently verified, so the collected results were documented as normal Play-scene measurements rather than a confirmed unlit baseline.

### WHAT I AM WAITING ON, AND FROM WHOM
* No additional benchmark data was available before the reporting deadline.
* Machine 2's GPU identification and VSync and shadow-cache settings remained unverified.

### WHAT IS BLOCKING SOMEONE ELSE BECAUSE OF ME
* Nothing. The available performance results and report were prepared for inclusion in the team's final documentation.

### FILES I CHANGED THIS WEEK
* `performance_results/` (benchmark CSV files)
* `performance_results/frame_time_benchmark.md`
* `tests/test_matrix.md`
* `tests/bug_log.md`

### NEXT WEEK
* None. R5's available QA testing, bug documentation, and performance benchmarking activities have been completed for the project submission.

---

## OVERALL R5 CONTRIBUTION SUMMARY

* Reviewed the project documentation and identified the responsibilities assigned to R5.
* Prepared and maintained the QA Test Matrix.
* Completed 14 functional tests covering state-machine behavior and lighting/detection systems.
* Recorded testing observations and local environment setup issues in the Bug Log.
* Inspected and used the existing frame-time logging implementation without modifying `main.cpp` or other members' source files.
* Conducted two valid frame-time benchmark runs on the primary computer.
* Collected and analyzed an additional 300 frame-time measurements from a second computer.
* Prepared the Frame-Time Benchmarking Report and organized the available performance evidence.
* Documented testing limitations and unverified benchmark information transparently.

**Overall Status:** R5 QA and Performance activities completed to the extent of the available project build, hardware, and evidence.
