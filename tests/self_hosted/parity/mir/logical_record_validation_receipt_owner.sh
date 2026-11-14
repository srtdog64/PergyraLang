#!/usr/bin/env bash
# Exact same-generation receipt controls over the real logical-record owner.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL=self-host-record-validation-receipt
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
fail() { echo "[$LABEL] $*" >&2; exit 1; }
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/record-validation-receipt.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
SOURCE=tests/self_hosted/fixtures/logical_record_negative_lookup_shortcut.pgy
printf 'true\n%.0s' {1..14} >"$WORK/expected.run"
sha256sum "$PGY" "$ROOT_DIR/$SOURCE" >"$WORK/input.sha256"
for backend in c llvm; do
    output="$REL/$backend.exe"
    if ! (cd "$ROOT_DIR" && timeout 90s "$PGY" --native-pipeline \
            "--backend=$backend" --opt=dev "$SOURCE" -o "$output") \
            >"$WORK/$backend.compile.log" 2>&1; then
        cat "$WORK/$backend.compile.log" >&2
        fail "$backend compilation failed"
    fi
    timeout 30s "$WORK/$backend.exe" >"$WORK/$backend.raw" \
        2>"$WORK/$backend.err" || fail "$backend execution failed"
    [[ ! -s "$WORK/$backend.err" ]] || fail "$backend emitted stderr"
    tr -d '\r' <"$WORK/$backend.raw" >"$WORK/$backend.run"
    cmp -s "$WORK/expected.run" "$WORK/$backend.run" || {
        diff -u "$WORK/expected.run" "$WORK/$backend.run" >&2
        fail "$backend receipt verdicts differed"
    }
done
sha256sum --quiet -c "$WORK/input.sha256"
echo "[$LABEL] PASS: C/LLVM production reuse, replay mode, stale/forged/mutated refusal and 2048 repeated queries; evidence=$REL"
