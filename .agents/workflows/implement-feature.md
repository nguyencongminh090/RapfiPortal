---
description: implement a new feature or module in portal_src
---

# Workflow: Implement Portal Engine Feature

// turbo-all

## Phase 1 — Understand
1. Read the relevant `portal_src/` files that will be touched.
   - For board changes: read `portal_src/game/board.h` and `board.cpp` in full.
   - For config changes: read `portal_src/config.h` and `config.cpp`.
   - Summarize: current architecture, key classes, how the change point integrates.

## Phase 2 — Locate Impact
2. Identify ALL call sites and data flows affected by the planned change.
   - Search with grep for the function/struct being changed.
   - List every file that includes or calls the modified code.
   - Explicitly state what breaks if the interface changes.

## Phase 3 — Plan
3. Write a concrete implementation plan:
   - List files to CREATE or MODIFY (never touch "copied, unchanged" files).
   - For each function: write a pseudocode sketch with portal semantics.
   - Flag any `[ASSUMPTION: ...]` about Rapfi behavior.
   - **STOP and wait for user approval before writing code.**

## Phase 4 — Implement
4. Implement incrementally, one logical unit at a time:
   - Add `// PORTAL:` comment on every line that differs from Rapfi.
   - Write doc comments for every new function (purpose, portal case, edge cases).
   - Never modify `PATTERN2x`, `PCODE`, or `P4SCORES` tables.
   - After each file: confirm the file compiles cleanly if possible.

## Phase 5 — Verify
5. Run sanity checks:
   - Build: `cd portal_src && cmake ... && ninja`
   - Run a quick test game or benchmark if available.
   - Confirm pattern output matches expected for a portal WIN case.
   - List any known edge cases that are NOT yet handled.
