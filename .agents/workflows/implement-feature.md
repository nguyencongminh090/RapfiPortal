---
description: implement a new feature or module in portal_src
---

# Workflow: Implement Portal Engine Feature

// turbo-all

## Phase 1 — Understand (read before touching anything)

1. **Read the relevant `portal_src/` files that will be touched:**
   - Board changes → read `game/board.h` and `game/board.cpp` in full.
   - NNUE changes → read `eval/mix9svqnnue.h` and `eval/mix9svqnnue.cpp` in full.
   - Eval/strategy changes → read `eval/eval.cpp` in full.
   - Config changes → read `config.h` and `config.cpp`.
   
2. **Summarize** before writing any code:
   - Current architecture at the change point.
   - How `move()`, `undo()`, and `newGame()` are affected (always check all three).
   - Whether the change touches `PatternCode` pipeline (flag as high regression risk).

## Phase 2 — Locate Impact

3. **Find ALL call sites** affected by the planned change:
   ```bash
   grep -rn "functionOrStruct" portal_src/
   ```
   - List every file that includes or calls the modified code.
   - Explicitly state what breaks if the interface changes (especially for `board.h`).
   - Confirm the file is marked **MODIFIED** in `portal-engine` skill — never touch "copied, unchanged" files.

## Phase 3 — Plan (STOP — wait for approval)

4. Write a concrete plan:
   - List files to **CREATE** or **MODIFY** (with justification).
   - For each new function: write pseudocode with portal semantics and edge cases.
   - For NNUE changes: specify which layers are frozen and which are trained.
   - Flag any `[ASSUMPTION: ...]` about Rapfi internals.
   - **STOP and wait for user approval before writing any code.**

## Phase 4 — Implement (one logical unit at a time)

5. Implement incrementally:
   - Add `// PORTAL:` comment on **every line** that differs from Rapfi baseline.
   - Write doc comments for every new function: purpose, portal case, edge cases.
   - **Never** modify `PATTERN2x[]`, `PCODE[]`, or `P4SCORES[]` tables.
   - Use `Pos::PASS` (-1) as sentinel — **never** `Pos::NONE` (0), which is cell (0,0).
   - For `move()` changes: always handle `undo()` symmetrically in the same commit.
   - After each file: confirm it compiles before touching the next file.

## Phase 5 — Build & Verify

6. Build:
   ```bash
   cd portal_src
   ninja -C build
   # If first build or CMake changes needed:
   # cmake --preset linux-clang-release && ninja -C build
   ```

7. Run sanity checks:
   ```bash
   ./build/portal-pattern-test   # pattern/key generation tests
   ./build/portal-test           # portal mechanics integration tests
   ```

8. Confirm correctness:
   - Portal WIN case: a 5-in-a-row passing through a portal pair is detected.
   - WALL blocking: a 5-in-a-row interrupted by a WALL is NOT detected.
   - For NNUE changes: verify `initIndexTable` produces different indices near WALLs vs. open board.

9. **Document known limitations** — list any edge cases NOT yet handled in a comment at the top of modified files.
