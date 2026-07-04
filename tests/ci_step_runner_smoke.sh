#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp="${TMPDIR:-/tmp}/pgy-ci-runner-smoke.$$"

rm -rf "$tmp"
mkdir -p "$tmp"
trap 'rm -rf "$tmp"' EXIT

cat >"$tmp/steps.sh" <<'STEPS'
run 'printf "before-failure\n"; printf "ROOT_CAUSE_LINE\n" >&2; exit 7'
run 'printf "after-failure\n"'
STEPS

set +e
PGY_CI_NAME=ci-runner-smoke \
PGY_CI_FAILURE_TAIL_LINES=8 \
PGY_CI_LOG_DIR="$tmp/logs" \
    bash "$ROOT_DIR/scripts/ci_step_runner.sh" "$tmp/steps.sh" \
    >"$tmp/output.txt" 2>&1
rc=$?
set -e

if [[ "$rc" -eq 0 ]]; then
    echo "[ci-step-runner] nested failure unexpectedly passed" >&2
    exit 1
fi

for required in \
    '[ci-runner-smoke step 1 FAILED rc=7]' \
    '========== [ci-runner-smoke step 2]' \
    'after-failure' \
    '========== [ci-runner-smoke SUMMARY] 1/2 steps FAILED ==========' \
    'step 1 rc=7:' \
    '--- tail (8 lines) ---' \
    'before-failure' \
    'ROOT_CAUSE_LINE'; do
    if ! grep -Fq -- "$required" "$tmp/output.txt"; then
        echo "[ci-step-runner] missing expected runner output: $required" >&2
        echo "--- output ---" >&2
        cat "$tmp/output.txt" >&2
        echo "--- end output ---" >&2
        exit 1
    fi
done

echo "[ci-step-runner] failure summary tail ok"
