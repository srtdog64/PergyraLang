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

STEP_NUM=0
STEP_NAME=""

# Explicit if/then on `eval` rather than `set -e` + trap ERR: errexit's
# interaction with function context + sourced files + eval is not portable,
# and an ERR trap fired from inside a function may not propagate the
# intended exit code. Check the eval status directly and exit with a
# labeled message so the last log line names the failing step.
run() {
    STEP_NUM=$((STEP_NUM + 1))
    STEP_NAME="$1"
    printf '\n========== [%s step %d] %s ==========\n' \
        "$CI_NAME" "$STEP_NUM" "$STEP_NAME" >&2
    local rc=0
    eval "$1" || rc=$?
    if [[ $rc -ne 0 ]]; then
        local fail_rc=$rc
        printf '\n[%s FAILED at step %d: %s] rc=%d\n' \
            "$CI_NAME" "$STEP_NUM" "$STEP_NAME" "$fail_rc" >&2
        exit "$fail_rc"
    fi
}

if [[ ! -r "$STEPS_FILE" ]]; then
    printf '[%s] step list not readable: %s\n' "$CI_NAME" "$STEPS_FILE" >&2
    exit 2
fi

# shellcheck disable=SC1090
. "$STEPS_FILE"

printf '\n[%s] all %d steps ok\n' "$CI_NAME" "$STEP_NUM" >&2
