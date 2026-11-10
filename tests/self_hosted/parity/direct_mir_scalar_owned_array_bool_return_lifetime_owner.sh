#!/usr/bin/env bash
# Owned ArrayBool results keep one cleanup owner across C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
pgy_prepend_windows_runtime_paths
pgy_selfhost_select_linked_runtime_compile_profile

LABEL="self-host-owned-array-bool-return-lifetime"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/owned-array-bool-return.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR/"}"
MUTATOR="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_owned_array_bool_return_lifetime_mutations.py"
EXPECTED="$WORK_DIR/expected.run"
printf 'true\nfalse\n' >"$EXPECTED"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

sanitizer=()
case "$(uname -s)" in
    Linux*) sanitizer=(-fsanitize=address,leak -fno-omit-frame-pointer) ;;
esac
"$CLANG" "${PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS[@]}" "${sanitizer[@]}" \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" \
    >"$WORK_DIR/runtime.compile.out" 2>"$WORK_DIR/runtime.compile.err" ||
    fail "runtime object did not compile"

project_and_run() {
    local stem="$1" source_rel="$2"
    local mir_rel="$WORK_REL/$stem.mir.json"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$source_rel" \
        -o "$mir_rel") >"$WORK_DIR/$stem.producer.out" \
        2>"$WORK_DIR/$stem.producer.err" || fail "$stem MIR production failed"
    local backend
    for backend in c llvm; do
        local artifact_rel="$WORK_REL/$stem.$backend"
        local artifact="$ROOT_DIR/$artifact_rel"
        local binary="$WORK_DIR/$stem-$backend"
        (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mir_rel" -o "$artifact_rel") >"$WORK_DIR/$stem.$backend.project.out" \
            2>"$WORK_DIR/$stem.$backend.project.err" || fail "$stem $backend projection failed"
        if [[ "$backend" == c ]]; then
            "$CC" "${PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS[@]}" "${sanitizer[@]}" \
                -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" "$artifact" -lm \
                -o "$binary" >"$WORK_DIR/$stem.c.compile.out" \
                2>"$WORK_DIR/$stem.c.compile.err" || fail "$stem C compile failed"
        else
            "$CLANG" -x ir "${sanitizer[@]}" "$artifact" -x none \
                "$WORK_DIR/runtime.o" -pthread -lm -o "$binary" \
                >"$WORK_DIR/$stem.llvm.compile.out" \
                2>"$WORK_DIR/$stem.llvm.compile.err" || fail "$stem LLVM compile failed"
        fi
        ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 "$binary" \
            >"$WORK_DIR/$stem.$backend.run" 2>"$WORK_DIR/$stem.$backend.sanitizer"
        tr -d '\r' <"$WORK_DIR/$stem.$backend.run" >"$WORK_DIR/$stem.$backend.normalized"
        cmp -s "$EXPECTED" "$WORK_DIR/$stem.$backend.normalized" ||
            fail "$stem $backend runtime output drifted"
        [[ ! -s "$WORK_DIR/$stem.$backend.sanitizer" ]] ||
            fail "$stem $backend sanitizer reported a lifetime failure"
    done
}

project_and_run fresh tests/self_hosted/fixtures/direct_mir_scalar_owned_array_bool_return.pgy
project_and_run transfer tests/self_hosted/fixtures/direct_mir_scalar_owned_array_bool_parameter_return.pgy

if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        tests/self_hosted/fixtures/direct_mir_scalar_borrowed_array_bool_return_escape.pgy \
        -o "$WORK_REL/borrowed-source.mir.json") >"$WORK_DIR/borrowed-source.out" \
        2>"$WORK_DIR/borrowed-source.err"; then
    fail "source producer accepted a borrowed ArrayBool return"
fi
[[ ! -e "$WORK_DIR/borrowed-source.mir.json" ]] || fail "borrowed source published MIR"
grep -Fq 'borrow_boundary_escape' "$WORK_DIR/borrowed-source.out" ||
    grep -Fq 'borrow_boundary_escape' "$WORK_DIR/borrowed-source.err" ||
    fail "borrowed source used the wrong refusal"

python "$MUTATOR" "$WORK_DIR/transfer.mir.json" "$WORK_DIR/borrowed.mir.json"
for backend in c llvm; do
    output_rel="$WORK_REL/borrowed.$backend"; rm -f "$ROOT_DIR/$output_rel"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$WORK_REL/borrowed.mir.json" -o "$output_rel") \
            >"$WORK_DIR/borrowed.$backend.out" 2>"$WORK_DIR/borrowed.$backend.err"; then
        fail "$backend accepted a forged borrowed ArrayBool result"
    fi
    [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published a borrowed result"
    grep -Fq 'owned array result has no fresh or transferred storage owner' \
        "$WORK_DIR/borrowed.$backend.out" || fail "$backend used the wrong lifetime refusal"
done

echo "[$LABEL] fresh/transfer C+LLVM parity, sanitizer cleanup and borrowed refusals: PASS"
