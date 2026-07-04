#!/usr/bin/env bash
# CI step runner.
#
# Usage:
#   PGY_CI_NAME=ci-linux bash scripts/ci_step_runner.sh scripts/ci_linux_steps.sh
#
# Reads the step list at $1 (sourced after `run` is defined). Each `run '<cmd>'`
# call labels the step and executes it. On failure, the runner records the
# step log and prints a final summary that includes the failing command and a
# short tail, so CI screenshots still carry the root-cause line. On success,
# the runner prints `[<ci-name>] all N steps ok`.

set -uo pipefail

CI_NAME="${PGY_CI_NAME:-ci}"
STEPS_FILE="${1:?usage: $0 <steps-file>}"
# PGY_CI_FAIL_FAST=1 reverts to abort-on-first-failure. The default is
# collect-mode: run every step, then summarize the failures at the end.
# Collect-mode is what an operator wants when a single CI run takes 20+
# minutes: stopping at step 3 means a second push is needed to see what
# else is broken; running every step surfaces every failure in one run.
PGY_CI_FAIL_FAST="${PGY_CI_FAIL_FAST:-0}"
PGY_CI_FAILURE_TAIL_LINES="${PGY_CI_FAILURE_TAIL_LINES:-40}"
PGY_CI_LOG_DIR="${PGY_CI_LOG_DIR:-${TMPDIR:-/tmp}/pgy-ci-step-logs.$$}"

STEP_NUM=0
STEP_NAME=""
FAILED_STEPS=()
FAILED_STEP_LOGS=()
KNOWN_FAILED_STEPS=()
KNOWN_FAILED_STEP_LOGS=()

pgy_ci_safe_name() {
    printf '%s' "$1" | sed 's/[^A-Za-z0-9_.-]/_/g'
}

pgy_ci_log_file_for_step() {
    local safe_ci
    safe_ci="$(pgy_ci_safe_name "$CI_NAME")"
    printf '%s/%s_step_%03d.log' "$PGY_CI_LOG_DIR" "$safe_ci" "$STEP_NUM"
}

pgy_ci_run_to_log() {
    local cmd="$1"
    local log_file="$2"
    local rc=0

    mkdir -p "$PGY_CI_LOG_DIR" 2>/dev/null || :
    ( eval "$cmd" ) 2>&1 | tee "$log_file"
    rc=${PIPESTATUS[0]}
    return "$rc"
}

pgy_ci_tail_lines() {
    case "$PGY_CI_FAILURE_TAIL_LINES" in
        ''|*[!0-9]*)
            printf '40'
            ;;
        *)
            printf '%s' "$PGY_CI_FAILURE_TAIL_LINES"
            ;;
    esac
}

pgy_ci_print_failure_tail() {
    local log_file="$1"
    local lines
    lines="$(pgy_ci_tail_lines)"

    if [[ -r "$log_file" ]]; then
        printf '    --- tail (%s lines) ---\n' "$lines" >&2
        tail -n "$lines" "$log_file" >&2 || :
        printf '    --- end tail ---\n' >&2
    else
        printf '    --- tail unavailable: %s ---\n' "$log_file" >&2
    fi
}

if ! command -v make >/dev/null 2>&1 \
    && command -v mingw32-make >/dev/null 2>&1; then
    PGY_CI_MINGW32_MAKE="$(command -v mingw32-make)"
    PGY_CI_MINGW32_BIN_DIR="${PGY_CI_MINGW32_MAKE%/*}"
    make() {
        PATH="$PGY_CI_MINGW32_BIN_DIR:$PATH" MSYSTEM= mingw32-make "$@"
    }
fi

run() {
    STEP_NUM=$((STEP_NUM + 1))
    STEP_NAME="$1"
    printf '\n========== [%s step %d] %s ==========\n' \
        "$CI_NAME" "$STEP_NUM" "$STEP_NAME" >&2
    local rc=0
    local log_file
    log_file="$(pgy_ci_log_file_for_step)"
    # Subshell isolation: if the step command contains `exit N`, that
    # exits the subshell, not the runner. The runner captures the
    # subshell's rc and decides whether to abort (fail-fast) or
    # continue (collect mode).
    pgy_ci_run_to_log "$1" "$log_file" || rc=$?
    if [[ $rc -ne 0 ]]; then
        local fail_rc=$rc
        if [[ "$PGY_CI_FAIL_FAST" == "1" ]]; then
            printf '\n[%s FAILED at step %d: %s] rc=%d\n' \
                "$CI_NAME" "$STEP_NUM" "$STEP_NAME" "$fail_rc" >&2
            pgy_ci_print_failure_tail "$log_file"
            exit "$fail_rc"
        fi
        FAILED_STEPS+=("step $STEP_NUM rc=$fail_rc: $STEP_NAME")
        FAILED_STEP_LOGS+=("$log_file")
        printf '\n[%s step %d FAILED rc=%d] continuing to surface further failures\n' \
            "$CI_NAME" "$STEP_NUM" "$fail_rc" >&2
    fi
}

# run_known_fail: a step that is allowed to fail without turning the whole
# CI run red. Use it for fails tracked in docs/100d as "known unrecovered"
# -- they still produce log noise that operators can grep for, but they
# don't gate the rest of the run. Pass the issue tag as $2 so the summary
# names what's being tolerated. SUCCESS on a known-fail step is silently
# upgraded to a normal pass (the registry just expected a *possible*
# failure, not a guaranteed one).
run_known_fail() {
    STEP_NUM=$((STEP_NUM + 1))
    STEP_NAME="$1"
    local issue="${2:-unspecified}"
    printf '\n========== [%s step %d] %s (known-fail: %s) ==========\n' \
        "$CI_NAME" "$STEP_NUM" "$STEP_NAME" "$issue" >&2
    local rc=0
    local log_file
    log_file="$(pgy_ci_log_file_for_step)"
    pgy_ci_run_to_log "$1" "$log_file" || rc=$?
    if [[ $rc -ne 0 ]]; then
        KNOWN_FAILED_STEPS+=("step $STEP_NUM rc=$rc ($issue): $STEP_NAME")
        KNOWN_FAILED_STEP_LOGS+=("$log_file")
        printf '\n[%s step %d KNOWN-FAIL rc=%d (%s)] continuing\n' \
            "$CI_NAME" "$STEP_NUM" "$rc" "$issue" >&2
    else
        printf '\n[%s step %d known-fail step now passing (%s)]\n' \
            "$CI_NAME" "$STEP_NUM" "$issue" >&2
    fi
}

if [[ ! -r "$STEPS_FILE" ]]; then
    printf '[%s] step list not readable: %s\n' "$CI_NAME" "$STEPS_FILE" >&2
    exit 2
fi

# shellcheck disable=SC1090
. "$STEPS_FILE"

if [[ ${#KNOWN_FAILED_STEPS[@]} -gt 0 ]]; then
    printf '\n========== [%s] %d known-fail step(s) (informational, does not gate CI) ==========\n' \
        "$CI_NAME" "${#KNOWN_FAILED_STEPS[@]}" >&2
    for idx in "${!KNOWN_FAILED_STEPS[@]}"; do
        printf '  - %s\n' "${KNOWN_FAILED_STEPS[$idx]}" >&2
        pgy_ci_print_failure_tail "${KNOWN_FAILED_STEP_LOGS[$idx]}"
    done
fi

if [[ ${#FAILED_STEPS[@]} -eq 0 ]]; then
    printf '\n[%s] all %d steps ok\n' "$CI_NAME" "$STEP_NUM" >&2
    exit 0
fi

printf '\n========== [%s SUMMARY] %d/%d steps FAILED ==========\n' \
    "$CI_NAME" "${#FAILED_STEPS[@]}" "$STEP_NUM" >&2
for idx in "${!FAILED_STEPS[@]}"; do
    printf '  - %s\n' "${FAILED_STEPS[$idx]}" >&2
    pgy_ci_print_failure_tail "${FAILED_STEP_LOGS[$idx]}"
done
exit 1
