#!/usr/bin/env bash
# A refused public binary compile must not leave the previous executable at
# the requested output path, and a published one appears by a single rename
# from a staging file. Compare all four selectors to an external oracle.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/binary_output_refusal.XXXXXX")"
WORK_REL=".tmp/self_hosted/$(basename "$WORK_DIR")"
VALID="tests/cases/backend_compare/basic/main.pgy"
INVALID="tests/self_hosted/fixtures/compile_rejected_stale_output.pgy"
fail() { echo "[binary-output-refusal] $*" >&2; exit 1; }
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ "$SELF_DRIVER" != *.exe ]] &&
    pgy_binary_expects_windows_paths "${SELF_DRIVER}.exe"; then
    SELF_DRIVER="${SELF_DRIVER}.exe"
fi
[[ -x "$PGY" && -x "$SELF_DRIVER" ]] || fail "compiler or installed driver missing"
PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"
export PGY_SELF_DRIVER_BIN="$SELF_DRIVER"
unset PGY_NATIVE_PIPELINE
suffix=""
[[ "$PGY" == *.exe ]] && suffix=".exe"

(cd "$ROOT_DIR" && "$PGY" "$VALID" --native-pipeline --backend=c \
    -o "$WORK_REL/seed$suffix") >"$WORK_DIR/seed.out" 2>"$WORK_DIR/seed.err" ||
    fail "could not build executable sentinel"
[[ -f "$WORK_DIR/seed$suffix" ]] || fail "sentinel executable missing"

for lane in native-c native-llvm self-c self-llvm; do
    output="$WORK_DIR/$lane$suffix"
    cp "$WORK_DIR/seed$suffix" "$output"
    args=("$INVALID")
    case "$lane" in
        native-c) args+=(--native-pipeline --backend=c) ;;
        native-llvm) args+=(--native-pipeline --backend=llvm) ;;
        self-c) args+=(--backend=c) ;;
        self-llvm) args+=(--backend=llvm) ;;
    esac
    set +e
    (cd "$ROOT_DIR" && "$PGY" "${args[@]}" \
        -o "$WORK_REL/$lane$suffix") >"$WORK_DIR/$lane.out" \
        2>"$WORK_DIR/$lane.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -e "$output" ]] ||
        fail "$lane retained an executable after compile refusal (rc=$rc)"

    (cd "$ROOT_DIR" && "$PGY" "$VALID" "${args[@]:1}" \
        -o "$WORK_REL/$lane$suffix") >"$WORK_DIR/$lane.valid.out" \
        2>"$WORK_DIR/$lane.valid.err" ||
        fail "$lane no longer compiles a valid program"
    "$output" 2>"$WORK_DIR/$lane.run.err" | tr -d '\r' \
        >"$WORK_DIR/$lane.run" ||
        fail "$lane valid executable did not run"
    printf '42\n' >"$WORK_DIR/expected.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$lane.run" ||
        fail "$lane valid executable changed behavior"
    # The toolchain writes a staging file and one rename publishes it; neither
    # the refused nor the published compile may leave that file behind.
    leftover="$(find "$WORK_DIR" -maxdepth 1 -name '*.pgy-staging-*' | head -1)"
    [[ -z "$leftover" ]] || fail "$lane left a staging binary: $leftover"
done

# Every lane above appends the platform suffix itself, so the request a
# user actually types -- `-o name` with no extension -- was never
# exercised. On Windows that request is published as "<name>.exe", and
# invalidation has to drop the name publication writes, not the one the
# command line spelled.
bare_output="$WORK_DIR/bare$suffix"
cp "$WORK_DIR/seed$suffix" "$bare_output"
set +e
(cd "$ROOT_DIR" && "$PGY" "$INVALID" --backend=c \
    -o "$WORK_REL/bare") >"$WORK_DIR/bare.out" 2>"$WORK_DIR/bare.err"
rc=$?
set -e
[[ "$rc" -ne 0 && ! -e "$bare_output" ]] ||
    fail "an extension-less request kept an executable after refusal (rc=$rc)"

(cd "$ROOT_DIR" && "$PGY" "$VALID" --backend=c \
    -o "$WORK_REL/bare") >"$WORK_DIR/bare.valid.out" \
    2>"$WORK_DIR/bare.valid.err" ||
    fail "an extension-less request no longer compiles a valid program"
[[ -x "$bare_output" ]] ||
    fail "an extension-less request did not publish $bare_output"
"$bare_output" 2>"$WORK_DIR/bare.run.err" | tr -d '\r' \
    >"$WORK_DIR/bare.run" ||
    fail "the extension-less executable did not run"
printf '42\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/bare.run" ||
    fail "the extension-less executable changed behavior"
leftover="$(find "$WORK_DIR" -maxdepth 1 -name '*.pgy-staging-*' | head -1)"
[[ -z "$leftover" ]] ||
    fail "an extension-less request left a staging binary: $leftover"

# Parsing errors still have a known binary target once the full command line
# has been read. They must not leave the previous successful output behind.
for lane in native-c native-llvm self-c self-llvm; do
    output="$WORK_DIR/parse-$lane$suffix"
    cp "$WORK_DIR/seed$suffix" "$output"
    args=("$VALID" --emit-debug-lines)
    case "$lane" in
        native-c) args+=(--native-pipeline --backend=c) ;;
        native-llvm) args+=(--native-pipeline --backend=llvm) ;;
        self-c) args+=(--backend=c) ;;
        self-llvm) args+=(--backend=llvm) ;;
    esac
    set +e
    (cd "$ROOT_DIR" && "$PGY" "${args[@]}" \
        -o "$WORK_REL/parse-$lane$suffix") >"$WORK_DIR/parse-$lane.out" \
        2>"$WORK_DIR/parse-$lane.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -e "$output" ]] ||
        fail "$lane kept a stale binary after option parse refusal (rc=$rc)"
    grep -Fq "unknown option '--emit-debug-lines'" \
        "$WORK_DIR/parse-$lane.err" ||
        fail "$lane option parse refusal lost its diagnostic"
done

# A parsed but unsupported compile option is still a refused binary request.
# The selector must invalidate the old output before its contract guard exits.
for lane in self-c self-llvm; do
    output="$WORK_DIR/unsupported-$lane$suffix"
    cp "$WORK_DIR/seed$suffix" "$output"
    backend="${lane#self-}"
    set +e
    (cd "$ROOT_DIR" && "$PGY" "$VALID" --backend="$backend" \
        --debug-lines -o "$WORK_REL/unsupported-$lane$suffix") \
        >"$WORK_DIR/unsupported-$lane.out" \
        2>"$WORK_DIR/unsupported-$lane.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -e "$output" ]] ||
        fail "$lane kept a stale binary after option-contract refusal (rc=$rc)"
    grep -Fq 'outside the installed self-host driver contract' \
        "$WORK_DIR/unsupported-$lane.err" ||
        fail "$lane option-contract refusal lost its diagnostic"
done

cp "$ROOT_DIR/$INVALID" "$WORK_DIR/source-alias.pgy"
set +e
(cd "$ROOT_DIR" && "$PGY" "$WORK_REL/source-alias.pgy" \
    --native-pipeline --backend=c \
    -o "$WORK_REL/source-alias.pgy") >"$WORK_DIR/alias.out" \
    2>"$WORK_DIR/alias.err"
alias_rc=$?
set -e
[[ "$alias_rc" -ne 0 && -f "$WORK_DIR/source-alias.pgy" ]] ||
    fail "source-alias refusal removed or accepted the input source"
grep -Fq 'aliases the input source' "$WORK_DIR/alias.err" ||
    fail "source-alias refusal lost its diagnostic"

mkdir -p "$WORK_DIR/directory-target"
set +e
(cd "$ROOT_DIR" && "$PGY" "$INVALID" --native-pipeline --backend=c \
    -o "$WORK_REL/directory-target") >"$WORK_DIR/directory.out" \
    2>"$WORK_DIR/directory.err"
directory_rc=$?
set -e
[[ "$directory_rc" -ne 0 && -d "$WORK_DIR/directory-target" ]] ||
    fail "directory output was deleted or accepted"
grep -Fq 'names a directory' "$WORK_DIR/directory.err" ||
    fail "directory refusal lost its diagnostic"

echo "[binary-output-refusal] four selectors and guarded paths: PASS"
