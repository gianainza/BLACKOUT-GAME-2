# R5 – Bug Log

**Project:** Skybox Activity  
**Role:** R5 – QA and Performance  
**Maintained by:** Misa

---

## 1. Bug Log Overview

This document records confirmed defects, unexpected behaviors, and environment-related issues discovered during quality assurance testing.

Issues encountered during local development are documented separately from confirmed project defects to avoid incorrectly classifying machine-specific problems as application-wide issues.

### Status Definitions

| Status | Description |
|---|---|
| OPEN | Issue has been identified but not resolved. |
| IN PROGRESS | Issue is currently being investigated or fixed. |
| FIXED | A fix has been implemented. |
| RETEST REQUIRED | The implemented fix requires QA verification. |
| CLOSED | The fix has been successfully verified. |
| RESOLVED LOCALLY | A local workaround or environment correction has resolved the issue. |

### Severity Levels

| Severity | Description |
|---|---|
| Critical | Application crashes or essential functionality is unusable. |
| Major | Important functionality or project integration is affected. |
| Minor | Small functional issue. |
| Cosmetic | Visual or presentation issue. |

---

## 2. Confirmed Project Bugs

No confirmed gameplay or lighting defects have been identified during the functional tests completed so far.

The following test categories have been completed:

- Gameplay and State Machine Testing (SM-01 to SM-08)
- Lighting and Detection Testing (LT-01 to LT-06)

All 14 executed test cases passed based on the observed results.

Further testing may identify additional defects.

---

## 3. Environment Setup and Compatibility Notes

### ENV-001 — Local GLEW Linking Conflict

- **Category:** Development Environment / Dependencies
- **Affected Machine:** R5 Workstation
- **Affected Configuration:** Debug x64
- **Status:** RESOLVED LOCALLY

**Description:**

During the initial Debug x64 build, Visual Studio encountered duplicate GLEW library definitions during the linking stage.

**Observed Libraries:**

- `glew32.lib` from `nupengl.core`
- `glew32sd.lib` from `glew.v140`

**Error Messages:**

- `LNK2005`: `glewInit` already defined.
- `LNK2005`: `glewGetErrorString` already defined.
- `LNK1169`: One or more multiply defined symbols found.

**Resolution:**

The `glew.v140` NuGet package was uninstalled locally through Visual Studio's NuGet Package Manager.

**Retest Result:**

- Debug x64 Build: Successful
- Application Launch: Successful

**Remarks:**

This issue was encountered on the R5 workstation. The local workaround resolved the build problem, but the shared repository configuration was not updated. This is not currently classified as a confirmed project-wide defect and may require review by R1.

---

### ENV-002 — Missing Microsoft Visual C++ Runtime

- **Category:** Development Environment / Runtime Dependencies
- **Affected Machine:** R5 Workstation
- **Status:** RESOLVED LOCALLY

**Description:**

The application initially failed to launch because `MSVCR120.dll` was unavailable on the R5 workstation.

**Error Message:**

`The code execution cannot proceed because MSVCR120.dll was not found.`

**Resolution:**

Installed the Microsoft Visual C++ 2013 Redistributable (x64).

**Retest Result:**

The application successfully launched after installing the required runtime.

**Remarks:**

This was a missing local runtime dependency, not a confirmed application defect.

---

## 4. Additional QA Observations

### OBS-001 — Camera Movement During Pause

- **Category:** Gameplay / Camera
- **Related Test:** SM-02
- **Status:** OBSERVATION ONLY
- **Severity:** Not classified as a defect

**Description:**

Mouse movement and POV camera rotation remain responsive while the game is paused.

**Expected Behavior:**

The documented pause behavior requires the game world to freeze. Camera input behavior is not explicitly specified.

**Actual Behavior:**

Gameplay and world activity stop, but mouse movement and camera rotation remain responsive.

**Remarks:**

No confirmed defect. Further clarification from the group may be required to determine whether camera input should also be disabled during pause.

---

## 5. QA Findings Summary

| Classification | Count |
|---|---:|
| Confirmed Project Bugs | 0 |
| Environment Setup Issues | 2 |
| Unconfirmed Observations | 1 |
| **Total Recorded Items** | **3** |

### Environment Setup Status

| Issue ID | Description | Status |
|---|---|---|
| ENV-001 | Local GLEW linking conflict | RESOLVED LOCALLY |
| ENV-002 | Missing Microsoft Visual C++ Runtime | RESOLVED LOCALLY |

---

## 6. General Remarks

The gameplay and lighting tests completed so far have not revealed any confirmed functional defects.

Two environment-related issues were encountered during local development and successfully resolved on the R5 workstation.

The camera behavior observed during pause remains documented for clarification but is not currently classified as a defect.

Additional issues discovered during subsequent QA activities will be recorded and assigned to the appropriate project owner.

**Prepared by:** R5 – QA and Performance