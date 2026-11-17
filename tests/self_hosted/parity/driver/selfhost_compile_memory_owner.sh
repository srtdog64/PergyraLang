#!/usr/bin/env bash
# A fixed middle-size compiler input exercises the installed MIR consumer.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PYTHON="${PYTHON_BIN:-python3}"
BOUNDARY="$ROOT_DIR/tests/self_hosted/parity/mir/full_mir_phase_process_owner.py"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/selfhost-memory.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
fail() { echo "[selfhost-compile-memory] $*; evidence=$REL" >&2; exit 1; }
[[ "$(uname -s)" == Linux ]] || fail "the RSS gate requires Linux"
pgy_require_runnable_binary_here selfhost-compile-memory "$DRIVER" || exit 1
[[ -x /usr/bin/time ]] || fail "GNU time is required for peak RSS"
"$PYTHON" "$BOUNDARY" --receipt "$WORK/producer.receipt.json" -- \
    "$DRIVER" --emit-mir-json-verified src/self_hosted/semantic/main.pgy \
    -o "$REL/input.json" >"$WORK/producer.log" 2>&1 || {
        tail -n 30 "$WORK/producer.log" >&2; fail "the complete semantic input was refused";
    }
"$PYTHON" "$BOUNDARY" --receipt "$WORK/consumer.receipt.json" -- \
    /usr/bin/time -f '%M' -o "$WORK/peak-rss-kib" \
    "$DRIVER" --mir-json "$REL/input.json" -o "$REL/output.c" \
    >"$WORK/consumer.log" 2>&1 || {
        tail -n 30 "$WORK/consumer.log" >&2; fail "the MIR consumer failed";
    }
"$PYTHON" "$BOUNDARY" --validate-receipt "$WORK/producer.receipt.json"
"$PYTHON" "$BOUNDARY" --validate-receipt "$WORK/consumer.receipt.json"
[[ -s "$WORK/output.c" ]] || fail "the consumer published no C"
peak="$(tr -d '\r\n' <"$WORK/peak-rss-kib")"
case "$peak" in ''|*[!0-9]*) fail "peak RSS was not measured" ;; esac
[[ "$peak" -le 819200 ]] || fail "peak RSS ${peak}KiB exceeds 800MiB"
echo "[selfhost-compile-memory] PASS: complete semantic MIR consumer peak=${peak}KiB <=800MiB; evidence=$REL"
