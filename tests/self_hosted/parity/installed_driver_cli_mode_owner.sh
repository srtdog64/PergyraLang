#!/usr/bin/env bash
# Installed composition-root proof for distinct source-C stdout and artifact
# effects. The removed source/output positional form must stay rejected.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
WORK_REL=".tmp/self_hosted/installed-driver-cli-mode"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/semantic/fixture/valid_call_int.pgy"
MODE="--emit-c-artifact-verified"
FLAG_PATH="$ROOT_DIR/--emit-c-verified"
UNKNOWN_FLAG="--unknown-installed-driver-mode"
UNKNOWN_PATH="$ROOT_DIR/$UNKNOWN_FLAG"
OUTPUT_FLAG="--invalid-output-option"
OUTPUT_FLAG_PATH="$ROOT_DIR/$OUTPUT_FLAG"

fail() {
    echo "[self-host-installed-cli-mode] $*" >&2
    exit 1
}

if [[ "$DRIVER" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${DRIVER}.exe"; then
    DRIVER="${DRIVER}.exe"
fi
[[ -x "$DRIVER" ]] || fail "installed self-host driver is missing: $DRIVER"
for path in "$FLAG_PATH" "$UNKNOWN_PATH" "$OUTPUT_FLAG_PATH"; do
    [[ ! -e "$path" ]] || fail "pre-existing flag-named path blocks proof: $path"
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

(cd "$ROOT_DIR" && "$DRIVER" "$SOURCE_REL" --emit-c-verified \
    >"$WORK_DIR/stdout-1.c" 2>"$WORK_DIR/stdout-1.err") ||
    fail "explicit stdout mode failed"
(cd "$ROOT_DIR" && "$DRIVER" "$SOURCE_REL" --emit-c-verified \
    >"$WORK_DIR/stdout-2.c" 2>"$WORK_DIR/stdout-2.err") ||
    fail "repeated stdout mode failed"
[[ -s "$WORK_DIR/stdout-1.c" ]] || fail "stdout mode emitted empty C"
cmp -s "$WORK_DIR/stdout-1.c" "$WORK_DIR/stdout-2.c" ||
    fail "stdout mode is not byte-stable"
grep -Fq '#include' "$WORK_DIR/stdout-1.c" ||
    fail "stdout mode did not emit a C artifact"
[[ ! -e "$FLAG_PATH" ]] || fail "stdout option became a flag-named artifact"

(cd "$ROOT_DIR" && "$DRIVER" "$MODE" "$SOURCE_REL" \
    "$WORK_REL/artifact.c" >"$WORK_DIR/artifact.out" \
    2>"$WORK_DIR/artifact.err") || fail "explicit artifact mode failed"
[[ -s "$WORK_DIR/artifact.c" ]] || fail "artifact mode emitted no C"
[[ ! -s "$WORK_DIR/artifact.out" ]] || fail "artifact mode leaked payload to stdout"
grep -Fq '#include' "$WORK_DIR/artifact.c" ||
    fail "artifact mode published a non-C payload"
tr -d '\r' <"$WORK_DIR/stdout-1.c" >"$WORK_DIR/stdout.normalized.c"
tr -d '\r' <"$WORK_DIR/artifact.c" >"$WORK_DIR/artifact.normalized.c"
cmp -s "$WORK_DIR/stdout.normalized.c" "$WORK_DIR/artifact.normalized.c" ||
    fail "stdout and artifact modes emitted different C payloads"
if compgen -G "$WORK_DIR/artifact.c.pgy-tmp-*" >/dev/null; then
    fail "artifact transaction left a temporary publication"
fi

source_hash_before="$(sha256sum "$ROOT_DIR/$SOURCE_REL" | awk '{print $1}')"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    >"$WORK_DIR/source.mir.json" 2>"$WORK_DIR/source-mir.err") ||
    fail "source-MIR stdout mode remained shadowed"
grep -Fq '"schema":"pgy.mir.v1"' "$WORK_DIR/source.mir.json" ||
    fail "source-MIR stdout mode emitted no canonical MIR"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$WORK_REL/source.mir.json" \
    >"$WORK_DIR/mir.c" 2>"$WORK_DIR/mir.err") ||
    fail "MIR-C stdout mode remained shadowed"
grep -Fq '#include' "$WORK_DIR/mir.c" ||
    fail "MIR-C stdout mode emitted no C"
source_hash_after="$(sha256sum "$ROOT_DIR/$SOURCE_REL" | awk '{print $1}')"
[[ "$source_hash_before" == "$source_hash_after" ]] ||
    fail "stdout mode overwrote its source operand"

set +e
(cd "$ROOT_DIR" && "$DRIVER" "$SOURCE_REL" "$WORK_REL/legacy.c" \
    >"$WORK_DIR/legacy.out" 2>"$WORK_DIR/legacy.err")
legacy_rc=$?
(cd "$ROOT_DIR" && "$DRIVER" "$SOURCE_REL" "$UNKNOWN_FLAG" \
    >"$WORK_DIR/unknown.out" 2>"$WORK_DIR/unknown.err")
unknown_rc=$?
(cd "$ROOT_DIR" && "$DRIVER" "$MODE" "$SOURCE_REL" \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err")
missing_rc=$?
(cd "$ROOT_DIR" && "$DRIVER" "$MODE" "$SOURCE_REL" "$OUTPUT_FLAG" \
    >"$WORK_DIR/output-flag.out" 2>"$WORK_DIR/output-flag.err")
output_flag_rc=$?
set -e

[[ "$legacy_rc" -ne 0 && ! -e "$WORK_DIR/legacy.c" ]] ||
    fail "removed positional artifact form was accepted"
grep -Fq 'unknown driver rung-2 mode' "$WORK_DIR/legacy.out" ||
    fail "removed positional form lost its owned diagnostic"
[[ "$unknown_rc" -ne 0 && ! -e "$UNKNOWN_PATH" ]] ||
    fail "unknown option was accepted as an output path"
grep -Fq 'unknown driver rung-2 mode' "$WORK_DIR/unknown.out" ||
    fail "unknown option lost its owned diagnostic"
[[ "$missing_rc" -ne 0 ]] || fail "artifact mode accepted a missing output"
grep -Fq 'requires source and output paths' "$WORK_DIR/missing.out" ||
    fail "missing artifact output lost its owned diagnostic"
[[ "$output_flag_rc" -ne 0 && ! -e "$OUTPUT_FLAG_PATH" ]] ||
    fail "artifact mode accepted an option token as output"
grep -Fq 'output path must not be an option token' \
    "$WORK_DIR/output-flag.out" || fail "output-option rejection lost its diagnostic"

echo "[self-host-installed-cli-mode] stdout and atomic artifact modes are disjoint; positional fallback rejected"
