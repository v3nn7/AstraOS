# SUFFIX.md
Official suffix specification for the versioning system.

Version format:

MAJOR.MINOR.FEATURE.PATCH-SUFFIX


Suffixes are optional but recommended for development and release stages.

---

## 1. Development Stages

### `dev`
Development build. Unstable, work-in-progress code.

### `exp`
Experimental build. Prototype features, may change at any time.

### `PA` — *Pre-Alpha*
Very early stage. Core systems may be incomplete. Not intended for general use.

### `A` — *Alpha*
Features implemented but not stable. Expect bugs. Suitable for internal testing.

### `B` — *Beta*
Most features are complete. Generally usable, but issues may still exist.

### `RC` — *Release Candidate*
Almost final. If no major issues appear, this becomes a stable release.

### `R` — *Release / Stable*
Official stable version. Recommended for general usage.

---

## 2. Special Suffixes

### `hotfix`
Immediate critical fix applied to a stable release.

### `sec`
Security-related fixes only.

### `perf`
Performance improvements without functional changes.

### `dbg`
Debug build with extra logging or assertions enabled.

### `lts`
Long-term support release.

---

## 3. Examples

0.0.1.0-PA
0.0.1.1-A
0.0.1.2-B
0.0.1.3-RC
0.0.1.4-R
0.1.0.0-hotfix
1.0.0.0-lts

