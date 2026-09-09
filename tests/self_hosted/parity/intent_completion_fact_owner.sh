#!/usr/bin/env bash
# Executable falsifiers of the target-neutral completion fact, not a driver build.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_BASE="$ROOT_DIR/.tmp/self_hosted/intent_completion_fact"
mkdir -p "$WORK_BASE"
WORK_DIR="$(mktemp -d "$WORK_BASE/run.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR"/}"
suffix=""
[[ "$PGY" != *.exe ]] || suffix=".exe"
fail() { echo "[intent-completion-fact] $* (evidence: $WORK_DIR)" >&2; exit 1; }
(cd "$ROOT_DIR" && "$PGY" --native-pipeline --backend=c --opt=dev \
    tests/self_hosted/parity/fixture/intent_completion_fact_probe.pgy \
    -o "$WORK_REL/probe$suffix") >"$WORK_DIR/compile.out" 2>"$WORK_DIR/compile.err" || fail "probe compile failed"
"$WORK_DIR/probe$suffix" >"$WORK_DIR/run.out" 2>"$WORK_DIR/run.err" || fail "fact contract failed"
[[ ! -s "$WORK_DIR/run.err" ]] || fail "runtime diagnostics"
tr -d '\r' <"$WORK_DIR/run.out" | grep -Fxq \
    'completion fact: bounds, canonical literal, and four-field digest PASS' || fail "observation drifted"
echo '[intent-completion-fact] five admitted values, five refusals and ten digest distinctions: PASS'
