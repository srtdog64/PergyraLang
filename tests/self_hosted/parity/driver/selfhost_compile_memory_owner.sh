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
TIME_BIN="$(type -P time)" || fail "GNU time is required for peak RSS"
"$TIME_BIN" --version >"$WORK/time-version" 2>&1 || fail "GNU time is required for peak RSS"
"$PYTHON" "$BOUNDARY" --receipt "$WORK/producer.receipt.json" -- \
    "$DRIVER" --emit-mir-json-verified src/self_hosted/semantic/main.pgy \
    -o "$REL/input.json" >"$WORK/producer.log" 2>&1 || {
        tail -n 30 "$WORK/producer.log" >&2; fail "the complete semantic input was refused";
    }
"$PYTHON" "$BOUNDARY" --receipt "$WORK/consumer.receipt.json" -- \
    "$TIME_BIN" -f '%M' -o "$WORK/peak-rss-kib" \
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
"$PYTHON" "$BOUNDARY" --receipt "$WORK/source-c.receipt.json" -- \
    "$TIME_BIN" -f '%M' -o "$WORK/source-c.peak-rss-kib" \
    "$DRIVER" --emit-c-artifact-verified src/self_hosted/mir_lower/main.pgy \
    "$REL/source-c.c" >"$WORK/source-c.log" 2>&1 || {
        tail -n 30 "$WORK/source-c.log" >&2; fail "the complete MIR-lower source input was refused";
    }
"$PYTHON" "$BOUNDARY" --validate-receipt "$WORK/source-c.receipt.json"
[[ -s "$WORK/source-c.c" ]] || fail "source-to-C published no C"
source_peak="$(tr -d '\r\n' <"$WORK/source-c.peak-rss-kib")"
case "$source_peak" in ''|*[!0-9]*) fail "source-to-C peak RSS was not measured" ;; esac
[[ "$source_peak" -le 8388608 ]] || fail "source-to-C peak RSS ${source_peak}KiB exceeds 8GiB"
"$PYTHON" "$BOUNDARY" --receipt "$WORK/source-mir.receipt.json" -- \
    "$DRIVER" --emit-mir-json-verified src/self_hosted/mir_lower/main.pgy \
    -o "$REL/source-mir.json" >"$WORK/source-mir.log" 2>&1 || {
        tail -n 30 "$WORK/source-mir.log" >&2; fail "the complete MIR-lower JSON input was refused";
    }
"$PYTHON" "$BOUNDARY" --receipt "$WORK/source-mir-c.receipt.json" -- \
    "$DRIVER" --mir-json "$REL/source-mir.json" -o "$REL/source-mir.c" \
    >"$WORK/source-mir-c.log" 2>&1 || {
        tail -n 30 "$WORK/source-mir-c.log" >&2; fail "the complete MIR-lower JSON consumer failed";
    }
"$PYTHON" "$BOUNDARY" --validate-receipt "$WORK/source-mir.receipt.json"
"$PYTHON" "$BOUNDARY" --validate-receipt "$WORK/source-mir-c.receipt.json"
cmp "$WORK/source-c.c" "$WORK/source-mir.c" || fail "source-to-C and MIR-to-C bytes differ"
echo "[selfhost-compile-memory] PASS: semantic MIR consumer ${peak}KiB <=800MiB; complete MIR-lower source-to-C ${source_peak}KiB <=8GiB; source/MIR C bytes equal; evidence=$REL"
