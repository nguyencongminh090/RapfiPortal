#!/usr/bin/env bash
# =============================================================================
# protocol-test.sh — Portal Engine Protocol Layer Test Suite
#
# Tests the protocol-level behaviour of pbrain-portal, focusing on:
#   - WALL / YXPORTAL initialization order
#   - START / RESTART portal persistence
#   - INFO CLEARPORTALS
#   - BOARD command with inline WALLs
#   - INFO RULE change preserving portals
# =============================================================================

set -euo pipefail

ENGINE="$(dirname "$0")/pbrain-portal"

PASS=0
FAIL=0
TOTAL=0

# ANSI colours
GREEN="\033[0;32m"
RED="\033[0;31m"
BOLD="\033[1m"
RESET="\033[0m"

# ---------------------------------------------------------------------------
# run_engine <cmd_string>
#   Pipes cmd_string to the engine and captures stdout+stderr.
run_engine() {
    printf '%s' "$1" | "$ENGINE" 2>&1
}

# check_pass <description> <output> <expected_pattern>
check_pass() {
    local desc="$1" output="$2" pattern="$3"
    TOTAL=$((TOTAL + 1))
    if echo "$output" | grep -qE "$pattern"; then
        printf "  ${GREEN}PASS${RESET}: %s\n" "$desc"
        PASS=$((PASS + 1))
    else
        printf "  ${RED}FAIL${RESET}: %s\n" "$desc"
        printf "       expected pattern: %s\n" "$pattern"
        printf "       actual output snippet:\n"
        echo "$output" | head -6 | sed 's/^/         /'
        FAIL=$((FAIL + 1))
    fi
}

# check_absent <description> <output> <absent_pattern>
check_absent() {
    local desc="$1" output="$2" pattern="$3"
    TOTAL=$((TOTAL + 1))
    if ! echo "$output" | grep -qE "$pattern"; then
        printf "  ${GREEN}PASS${RESET}: %s\n" "$desc"
        PASS=$((PASS + 1))
    else
        printf "  ${RED}FAIL${RESET}: %s\n" "$desc"
        printf "       pattern should NOT appear: %s\n" "$pattern"
        echo "$output" | grep -E "$pattern" | head -3 | sed 's/^/         /'
        FAIL=$((FAIL + 1))
    fi
}

section() { printf "\n${BOLD}=== %s ===${RESET}\n" "$1"; }

# =============================================================================
section "TEST 1: Basic START responds OK"
OUT=$(run_engine "START 15
END
")
check_pass "START 15 returns OK" "$OUT" "^OK$"

# =============================================================================
section "TEST 2: YXPORTAL before START (pre-configuration)"
# Portal registered before START → board must be initialized with it on START
OUT=$(run_engine "INFO YXPORTAL 3,6 7,3
START 15
TRACEBOARD
END
")
check_pass "YXPORTAL before START → board trace shows portal" "$OUT" "P0.*3,6.*7,3|Portals: 1"
check_absent "No ERROR from pre-START YXPORTAL" "$OUT" "^ERROR"

# =============================================================================
section "TEST 3: YXPORTAL after START (no RESTART needed)"
OUT=$(run_engine "START 15
INFO YXPORTAL 3,6 7,3
TRACEBOARD
END
")
check_pass "YXPORTAL after START → board has portal without RESTART" "$OUT" "P0.*3,6.*7,3|Portals: 1"
check_absent "No ERROR" "$OUT" "^ERROR"

# =============================================================================
section "TEST 4: RESTART preserves portals (same board size)"
OUT=$(run_engine "START 15
INFO YXPORTAL 3,6 7,3
RESTART
TRACEBOARD
END
")
check_pass "RESTART keeps portals — trace shows portal" "$OUT" "P0.*3,6.*7,3|Portals: 1"

# =============================================================================
section "TEST 5: START with same size preserves portals"
OUT=$(run_engine "START 15
INFO YXPORTAL 3,6 7,3
START 15
TRACEBOARD
END
")
# Same-size START should keep portals (our design: only size-change clears)
check_pass "START same-size keeps portals" "$OUT" "P0.*3,6.*7,3|Portals: 1"

# =============================================================================
section "TEST 6: START with different size clears portals"
OUT=$(run_engine "START 15
INFO YXPORTAL 3,6 7,3
START 19
TRACEBOARD
END
")
# After size change, portals should be cleared
check_pass "START different-size → size-change message" "$OUT" "Board size changed"
check_absent "START different-size → portal gone from trace" "$OUT" "Portals: 1"

# =============================================================================
section "TEST 7: INFO CLEARPORTALS removes portals"
OUT=$(run_engine "START 15
INFO YXPORTAL 3,6 7,3
INFO CLEARPORTALS
TRACEBOARD
END
")
check_absent "CLEARPORTALS → portal removed" "$OUT" "Portals: 1"

# =============================================================================
section "TEST 8: Multiple portals and WALLs"
OUT=$(run_engine "START 15
INFO YXPORTAL 1,5 5,1
INFO YXPORTAL 10,3 3,10
INFO WALL 7,7
TRACEBOARD
END
")
check_pass "2 portal pairs registered" "$OUT" "Portals: 2"

# =============================================================================
section "TEST 9: INFO WALL reported as WALL in board trace"
OUT=$(run_engine "START 15
INFO WALL 7,7
TRACEBOARD
END
")
# WALL cells appear as board boundary in trace — no crash, no ERROR
check_absent "No ERROR from INFO WALL" "$OUT" "^ERROR"
check_pass "WALL accepted" "$OUT" "^OK$"

# =============================================================================
section "TEST 10: INFO RULE change preserves existing portals"
OUT=$(run_engine "START 15
INFO YXPORTAL 3,6 7,3
INFO RULE 1
TRACEBOARD
END
")
check_pass "RULE change → portal still in trace" "$OUT" "P0.*3,6.*7,3|Portals: 1"
check_absent "No ERROR from rule change" "$OUT" "^ERROR"

# =============================================================================
section "TEST 11: BOARD command with inline WALL (color=3)"
OUT=$(run_engine "START 15
BOARD
7,7,3
5,5,1
7,5,2
DONE
TRACEBOARD
END
")
check_absent "No ERROR from BOARD inline WALL" "$OUT" "^ERROR"

# =============================================================================
section "TEST 12: BEGIN on portal board — engine must respond with valid move"
OUT=$(run_engine "START 15
INFO YXPORTAL 3,6 7,3
BEGIN
END
")
# Engine should output a coordinate like "x,y"
check_pass "BEGIN returns a coord x,y" "$OUT" "^[0-9]+,[0-9]+$"

# =============================================================================
section "TEST 13: TURN on portal board — engine must respond"
OUT=$(run_engine "START 15
INFO YXPORTAL 3,6 7,3
TURN 7,7
END
")
check_pass "TURN returns a coord x,y" "$OUT" "^[0-9]+,[0-9]+$"

# =============================================================================
section "TEST 14: YXPORTAL with invalid coords reports ERROR"
OUT=$(run_engine "START 15
INFO YXPORTAL 99,99 50,50
END
")
check_pass "Invalid YXPORTAL coords → ERROR" "$OUT" "^ERROR"

# =============================================================================
section "TEST 15: Duplicate YXPORTAL (A == B) reports ERROR"
OUT=$(run_engine "START 15
INFO YXPORTAL 5,5 5,5
END
")
check_pass "YXPORTAL A==B → ERROR" "$OUT" "^ERROR"

# =============================================================================
# Final summary
printf "\n${BOLD}================================================================${RESET}\n"
printf "${BOLD}  RESULTS: ${GREEN}%d passed${RESET}, ${BOLD}%s%d failed${RESET}\n" \
    "$PASS" "$( [ "$FAIL" -gt 0 ] && echo "$RED" || echo "" )" "$FAIL"
printf "${BOLD}================================================================${RESET}\n"

[ "$FAIL" -eq 0 ] && exit 0 || exit 1
