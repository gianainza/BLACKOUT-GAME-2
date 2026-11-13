# R5 – QA Test Matrix

**Project:** Skybox Activity  
**Role:** R5 – QA and Performance  
**Test Environment:** Visual Studio Community  
**Build Configuration:** Debug x64  
**Testing Method:** Manual Testing

---

## 1. Testing Overview

This document records the functional and gameplay tests performed on the integrated Skybox Activity project.

The purpose of testing is to verify that the implemented game mechanics and state transitions behave according to the project's documented specifications.

### Test Status Definitions

| Status | Description |
|---|---|
| PASS | Actual behavior matches the expected result. |
| FAIL | Actual behavior does not match the expected result. |
| BLOCKED | Testing cannot proceed due to an unresolved issue or dependency. |
| NOT RUN | Test has not yet been performed. |

### Test Categories

| Prefix | Category |
|---|---|
| SM | State Machine / Gameplay |
| LT | Lighting and Detection |
| SH | Shadow Rendering |
| TX | Texture and Material |
| SKY | Skybox |
| PERF | Performance |

---

## 2. Gameplay and State Machine Tests

### SM-01 — Title to Playing

- **Status:** PASS
- **Preconditions:** Game successfully launched at the Title screen.
- **Procedure:**
  1. Launch the game.
  2. Wait for the Title screen.
  3. Press Enter once.
- **Expected Result:** Game transitions from Title to Playing.
- **Actual Result:** Game transitioned directly into gameplay immediately after pressing Enter.
- **Observations:** No visible bugs, errors, or unexpected behavior were observed.
- **Evidence:** Pending screenshot.
- **Remarks:** Gameplay reset behavior during a subsequent restart has not been independently verified.

### SM-02 — Playing to Paused

- **Status:** PASS
- **Preconditions:** Game is currently in Playing state.
- **Procedure:**
  1. Start the game.
  2. Press P once.
  3. Observe the game.
  4. Wait approximately 5–10 seconds.
- **Expected Result:** Game transitions to Paused and the game world stops progressing.
- **Actual Result:** Game successfully paused. Gameplay and world activity stopped.
- **Observations:** Mouse movement and POV camera rotation remained responsive while the game was paused.
- **Evidence:** Pending screenshot.
- **Remarks:** Camera responsiveness is not explicitly addressed in the current STATE_MACHINE.md specification. No confirmed defect.


### SM-03 — Paused to Playing

- **Status:** PASS
- **Preconditions:** Game is currently in Paused state.
- **Procedure:**
  1. Pause the game using P.
  2. Press P again.
  3. Observe the game.
- **Expected Result:** Game resumes Playing and gameplay activity continues.
- **Actual Result:** Game successfully resumed after pressing P.
- **Observations:** Gameplay continued after resuming. No unexpected behavior was reported.
- **Evidence:** Pending screenshot.
- **Remarks:** Resume functionality works as expected.


### SM-04 — Restart During Playing

- **Status:** PASS
- **Preconditions:** Game is currently in Playing state.
- **Procedure:**
  1. Start the game.
  2. Move the droid away from its initial position.
  3. Allow gameplay to progress.
  4. Press R.
  5. Observe the resulting game state.
- **Expected Result:** Current level restarts, the droid returns to its initial position, and gameplay state resets.
- **Actual Result:** All expected restart behaviors were observed successfully.
- **Observations:** No unexpected behavior or visible defects were reported.
- **Evidence:** Pending screenshot or recording.
- **Remarks:** Restart functionality works as expected.


### SM-05 — Victory Condition

- **Status:** PASS
- **Preconditions:** Game is currently in Playing state.
- **Procedure:**
  1. Start gameplay.
  2. Locate the level exit.
  3. Move the droid toward the exit.
  4. Enter the exit radius.
  5. Observe the resulting state.
- **Expected Result:** Game transitions to Won.
- **Actual Result:** Victory condition triggered successfully.
- **Observations:** Expected victory behavior was observed. No issues reported.
- **Evidence:** Pending screenshot or recording.
- **Remarks:** Victory condition works as expected.


### SM-06 — Loss Condition

- **Status:** PASS
- **Preconditions:** Game is currently in Playing state and a strike can be triggered.
- **Procedure:**
  1. Start gameplay.
  2. Allow a scanner to detect and lock onto the droid.
  3. Remain within the strike radius.
  4. Allow the strike to land.
  5. Observe the resulting state.
- **Expected Result:** Game transitions to Lost when the strike lands within the specified radius.
- **Actual Result:** The loss condition was successfully triggered.
- **Observations:** Expected loss behavior was observed.
- **Evidence:** Pending screenshot or recording.
- **Remarks:** Loss condition works as expected under the tested conditions.


### SM-07 — Avoiding a Strike

- **Status:** PASS
- **Preconditions:** Game is currently in Playing state and a scanner can trigger a strike.
- **Procedure:**
  1. Start gameplay.
  2. Allow a scanner to detect and lock onto the droid.
  3. Move the droid outside the strike radius before the strike lands.
  4. Allow the strike to land.
  5. Observe the resulting state.
- **Expected Result:** A strike outside the specified radius does not transition the game to Lost.
- **Actual Result:** The droid successfully avoided the strike.
- **Observations:** The droid survived the strike and the test passed.
- **Evidence:** Pending screen recording.
- **Remarks:** Strike avoidance functionality works as expected under the tested conditions.


### SM-08 — Restart After Losing

- **Status:** PASS
- **Preconditions:** Game is currently in Lost state.
- **Procedure:**
  1. Trigger the loss condition.
  2. Wait until the Lost state appears.
  3. Press R.
  4. Observe the resulting game state.
- **Expected Result:** The same level restarts and gameplay returns to its initial state.
- **Actual Result:** Restart after losing completed successfully.
- **Observations:** Expected restart behavior was observed.
- **Evidence:** Pending screenshot or recording.
- **Remarks:** Restart functionality after losing works as expected.

---

### LT-01 — Scanner Detection

- **Status:** PASS
- **Preconditions:** Game is currently in Playing state.
- **Procedure:**
  1. Start gameplay.
  2. Locate a scanner.
  3. Move the droid into the scanner's illuminated detection area.
  4. Observe the detection meter.
- **Expected Result:** Detection meter increases while the droid is exposed to scanner illumination.
- **Actual Result:** Detection meter increased successfully when the scanner illuminated the droid.
- **Observations:** Scanner detection behaved as expected.
- **Evidence:** Pending screenshot or recording.
- **Remarks:** Scanner illumination successfully contributes to detection.


### LT-02 — Detection Reaches 1.0

- **Status:** PASS
- **Preconditions:** Game is currently in Playing state and a scanner can detect the droid.
- **Procedure:**
  1. Start gameplay.
  2. Expose the droid to scanner illumination.
  3. Allow the detection meter to reach 1.0.
  4. Observe the scanner's state and behavior.
- **Expected Result:** Scanner transitions from Scanning to Locked. Game remains in Playing without immediately triggering the loss condition.
- **Actual Result:** Scanner successfully locked onto the droid after detection reached 1.0. It subsequently fired an incineration-cannon-like projectile toward the locked position.
- **Observations:** Detection and lock-on behavior worked as expected.
- **Evidence:** Pending screen recording.
- **Remarks:** Maximum detection triggers the Locked state rather than an immediate loss.


### LT-03 — Delayed Strike Behavior

- **Status:** PASS
- **Preconditions:** Game is currently in Playing state and a scanner can detect the droid.
- **Procedure:**
  1. Start gameplay.
  2. Allow a scanner to detect the droid.
  3. Wait until the scanner enters Locked.
  4. Observe the time between lock-on and the cannon firing.
- **Expected Result:** Scanner fires after the configured delay following lock-on (approximately 1.9 seconds).
- **Actual Result:** Scanner fired approximately 1.9–2.0 seconds after entering Locked.
- **Observations:** The delayed strike behavior matched the documented timing.
- **Evidence:** Pending screen recording.
- **Remarks:** Strike delay is consistent with the documented 1.9-second configuration.


### LT-04 — Strike Outside Radius

- **Status:** PASS
- **Preconditions:** Game is currently in Playing state and a scanner can trigger a strike.
- **Procedure:**
  1. Start gameplay.
  2. Allow a scanner to detect and lock onto the droid.
  3. Move the droid outside the strike radius before the cannon fires.
  4. Allow the strike sequence to complete.
  5. Observe the resulting game state.
- **Expected Result:** A strike outside the specified radius does not transition the game to Lost.
- **Actual Result:** The droid successfully escaped the strike radius before the cannon fired. The loss condition was not triggered.
- **Observations:** The droid survived after moving outside the strike radius.
- **Evidence:** Pending screen recording.
- **Remarks:** Strike avoidance behavior works as expected under the tested conditions.


### LT-05 — Strike Inside Radius

- **Status:** PASS
- **Preconditions:** Game is currently in Playing state and a scanner can trigger a strike.
- **Procedure:**
  1. Start gameplay.
  2. Allow a scanner to detect and lock onto the droid.
  3. Remain within the strike radius.
  4. Allow the projectile to reach its target.
  5. Observe the resulting game state.
- **Expected Result:** Game transitions to Lost when the strike lands within the specified radius.
- **Actual Result:** Game successfully transitioned to Lost while the droid remained within the strike radius.
- **Observations:** The loss condition triggered as expected.
- **Evidence:** Pending screen recording.
- **Remarks:** Strike impact correctly triggers the loss condition when the droid is within range.


### LT-06 — Droid's Own Lamp Exclusion

- **Status:** PASS
- **Preconditions:** Game is currently in Playing state, with the droid's own lamp active.
- **Procedure:**
  1. Start gameplay.
  2. Move the droid to an area outside enemy scanner illumination.
  3. Keep the droid's own lamp active.
  4. Observe the detection meter.
  5. Move the droid into an enemy scanner's light radius.
  6. Observe the detection meter again.
- **Expected Result:** The droid's own lamp does not contribute to enemy detection. Detection only increases when exposed to enemy illumination.
- **Actual Result:** The droid's own lamp did not increase enemy detection. Detection increased only after entering the enemy's light radius.
- **Observations:** The droid's own light source is excluded from enemy detection.
- **Evidence:** Pending screenshot or recording.
- **Remarks:** Own-light exclusion works as expected.



---

## 4. Test Execution Summary

| Status | Total |
|---|---:|
| PASS | 14 |
| FAIL | 0 |
| BLOCKED | 0 |
| NOT RUN | 0 |
| **Total Test Cases** | **14** |

---

## 4. General Remarks

Testing is being conducted incrementally using the integrated project build.

Any discovered defects will be documented separately in `bug_log.md` and coordinated with the responsible file owner.

Performance benchmarking is maintained separately in `performance_baseline.md`.

**Prepared by:** R5 – QA and Performance