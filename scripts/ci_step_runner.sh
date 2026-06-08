#!/usr/bin/env bash
# CI step runner.
#
# Usage:
#   PGY_CI_NAME=ci-linux bash scripts/ci_step_runner.sh scripts/ci_linux_steps.sh
#
# Reads the step list at $1 (sourced after `run` is defined). Each `run '<cmd>'`
# call labels the step and executes it. On failure, the trap prints a final
# `[<ci-name> FAILED at step N: <name>]` line so the last log line names the
# failing step. On success, the runner prints `[<ci-name>] all N steps ok`.

set -uo pipefail

CI_NAME="${PGY_CI_NAME:-ci}"
STEPS_FILE="${1:?usage: $0 <steps-file>}"
# PGY_CI_FAIL_FAST=1 reverts to abort-on-first-failure. The default is
# collect-mode: run every step, then summarize the failures at the end.
# Collect-mode is what an operator wants when a single CI run takes 20+
# minutes: stopping at step 3 means a second push is needed to see what
# else is broken; running every step surfaces every failure in one run.
PGY_CI_FAIL_FAST="${PGY_CI_FAIL_FAST:-0}"

STEP_NUM=0
STEP_NAME=""
FAILED_STEPS=()

run() {
    STEP_NUM=$((STEP_NUM + 1))
    STEP_NAME="$1"
    printf '\n========== [%s step %d] %s ==========\n' \
        "$CI_NAME" "$STEP_NUM" "$STEP_NAME" >&2
    local rc=0
    # Subshell isolation: if the step command contains `exit N`, that
    # exits the subshell, not the runner. The runner captures the
    # subshell's rc and decides whether to abort (fail-fast) or
    # continue (collect mode).
    ( eval "$1" ) || rc=$?
    if [[ $rc -ne 0 ]]; then
        local fail_rc=$rc
        if [[ "$PGY_CI_FAIL_FAST" == "1" ]]; then
            printf '\n[%s FAILED at step %d: %s] rc=%d\n' \
                "$CI_NAME" "$STEP_NUM" "$STEP_NAME" "$fail_rc" >&2
            exit "$fail_rc"
        fi
        FAILED_STEPS+=("step $STEP_NUM rc=$fail_rc: $STEP_NAME")
        printf '\n[%s step %d FAILED rc=%d] continuing to surface further failures\n' \
            "$CI_NAME" "$STEP_NUM" "$fail_rc" >&2
    fi
}

if [[ ! -r "$STEPS_FILE" ]]; then
    printf '[%s] step list not readable: %s\n' "$CI_NAME" "$STEPS_FILE" >&2
    exit 2
fi

# shellcheck disable=SC1090
. "$STEPS_FILE"

if [[ ${#FAILED_STEPS[@]} -eq 0 ]]; then
    printf '\n[%s] all %d steps ok\n' "$CI_NAME" "$STEP_NUM" >&2
    exit 0
fi

printf '\n========== [%s SUMMARY] %d/%d steps FAILED ==========\n' \
    "$CI_NAME" "${#FAILED_STEPS[@]}" "$STEP_NUM" >&2
for entry in "${FAILED_STEPS[@]}"; do
    printf '  - %s\n' "$entry" >&2
done
exit 1
