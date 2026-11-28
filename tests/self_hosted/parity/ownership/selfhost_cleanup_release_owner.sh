#!/usr/bin/env bash
# Compiler-owned cleanup in the installed self-host driver's general MIR
# emitters, one release cause per run (the argument names the fixture
# directory tests/self_hosted/fixtures/cleanup_release/<cause>).
#
# Every fixture goes source -> verified MIR -> C and LLVM
# (--mir-json-backend), runs under AddressSanitizer with the native oracle's
# output, and must show no invalid access and no double free. The LLVM leg is
# built at -O0: at -O1 LLVM deletes a second free of one pointer.
# - A *_released fixture must also run LeakSanitizer-clean: the cause's owner
#   releases every allocation exactly once.
# - A *_kept fixture keeps storage that no owner may release yet (the
#   2026-11-15 hand-off decision, or a cause still open); it checks memory
#   safety and output only.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
pgy_selfhost_select_linked_runtime_compile_profile
pgy_prepend_windows_runtime_paths

CAUSE="${1:-}"
LABEL="self-host-cleanup-release-$CAUSE"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
FIXTURE_REL="tests/self_hosted/fixtures/cleanup_release/$CAUSE"
WORK_REL=".tmp/self_hosted/cleanup_release/$CAUSE"
WORK_DIR="$ROOT_DIR/$WORK_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
[[ "$CAUSE" =~ ^[a-z][a-z-]*$ && -d "$ROOT_DIR/$FIXTURE_REL" ]] || fail "unknown cause: '$CAUSE'"
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

mkdir -p "$WORK_DIR"
rm -rf "${WORK_DIR:?}"/*
runtime_object="$WORK_DIR/pgy-runtime-lib-asan.o"
"$CLANG" "${PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS[@]}" -g -fsanitize=address \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_object" \
    >"$WORK_DIR/runtime.out" 2>"$WORK_DIR/runtime.err" || {
        cat "$WORK_DIR/runtime.err" >&2; fail "ASan runtime object did not compile"; }

released=0
kept=0
for source in "$ROOT_DIR/$FIXTURE_REL"/*.pgy; do
    name="$(basename "$source" .pgy)"
    rel="$FIXTURE_REL/$name.pgy"
    case "$name" in
        *_released) mode=released ;;
        *_kept) mode=kept ;;
        *) fail "$name: a fixture name ends in _released or _kept" ;;
    esac
    (cd "$ROOT_DIR" && "$PGY" "$rel" --native-pipeline --backend=c -o "$WORK_REL/$name.native.exe") \
        >"$WORK_DIR/$name.native.out" 2>&1 ||
        { cat "$WORK_DIR/$name.native.out" >&2; fail "$name: native compile failed"; }
    "$WORK_DIR/$name.native.exe" </dev/null | tr -d '\r' >"$WORK_DIR/$name.expected"
    [[ -s "$WORK_DIR/$name.expected" ]] || fail "$name: native printed nothing"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$rel" -o "$WORK_REL/$name.mir.json") \
        >"$WORK_DIR/$name.producer.out" 2>&1 ||
        { cat "$WORK_DIR/$name.producer.out" >&2; fail "$name: MIR production failed"; }
    for backend in c llvm; do
        artifact="$WORK_REL/$name.$backend"
        (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/$name.mir.json" -o "$artifact") \
            >"$WORK_DIR/$name.$backend.out" 2>&1 ||
            { cat "$WORK_DIR/$name.$backend.out" >&2; fail "$name: $backend emission refused"; }
        bin="$WORK_DIR/$name-$backend.exe"
        if [[ "$backend" == c ]]; then
            "$CC" -x c -std=c11 -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer \
                -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread "$ROOT_DIR/$artifact" -lm \
                -o "$bin" 2>"$WORK_DIR/$name.c.compile.err" ||
                { cat "$WORK_DIR/$name.c.compile.err" >&2; fail "$name: C artifact did not compile"; }
        else
            # Instrument every emitted definition, as the four-leg memory check does.
            sed -E 's/^(define [^{]*\))([^{]*)\{[[:space:]]*$/\1\2 sanitize_address {/' \
                "$ROOT_DIR/$artifact" >"$WORK_DIR/$name.asan.ll"
            "$CLANG" -g -O0 -fsanitize=address -x ir "$WORK_DIR/$name.asan.ll" -x none "$runtime_object" \
                -pthread -lm -o "$bin" 2>"$WORK_DIR/$name.llvm.compile.err" ||
                { cat "$WORK_DIR/$name.llvm.compile.err" >&2; fail "$name: LLVM artifact did not compile"; }
        fi
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "$bin" </dev/null 2>"$WORK_DIR/$name.$backend.run.err" |
            tr -d '\r' >"$WORK_DIR/$name.$backend.run" ||
            { cat "$WORK_DIR/$name.$backend.run.err" >&2; fail "$name: $backend run failed under ASan"; }
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name.$backend.run" || {
            diff "$WORK_DIR/$name.expected" "$WORK_DIR/$name.$backend.run" >&2 || true
            fail "$name: $backend output differs from native"; }
        if [[ "$mode" == released ]]; then
            # Storage only a dead frame still names counts as leaked.
            if ! ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=use_stacks=0:use_registers=0 \
                    "$bin" </dev/null >/dev/null 2>"$WORK_DIR/$name.$backend.leaks"; then
                cat "$WORK_DIR/$name.$backend.leaks" >&2
                fail "$name: $backend leaks storage its owner must release"
            fi
        fi
    done
    if [[ "$mode" == released ]]; then released=$((released + 1)); else kept=$((kept + 1)); fi
done
[[ "$released" -gt 0 ]] || fail "no _released fixture"
echo "[$LABEL] PASS: $released released and $kept kept fixtures equal to native on C and LLVM under ASan (LLVM -O0); released ones LeakSanitizer-clean"
