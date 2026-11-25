#!/usr/bin/env bash
# `match` over an Option of a record on the general direct-MIR route: `case
# Some(p)` binds the payload record, `case None` tests absence, and a match
# with both arms ends the routine (no trailing return). Direct C (ASan) and
# LLVM print native's output. A Some binding whose type is not the payload
# record, and an Option of a record that holds storage, are refused.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
source "$ROOT_DIR/tests/self_hosted/parity/linked_runtime_compile_profile_owner.sh"
pgy_selfhost_select_linked_runtime_compile_profile
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-option-record-match"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/direct_mir_option_record_match"
WORK_DIR="$ROOT_DIR/$WORK_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"

mkdir -p "$WORK_DIR"
rm -rf "${WORK_DIR:?}"/*
runtime_object="$WORK_DIR/pgy-runtime-lib.o"
"$CLANG" "${PGY_SELFHOST_RUNTIME_C_COMPILE_FLAGS[@]}" \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_object" \
    >"$WORK_DIR/runtime.out" 2>"$WORK_DIR/runtime.err" || {
        cat "$WORK_DIR/runtime.err" >&2; fail "LLVM runtime object did not compile"; }

project() {
    local name="$1" backend="$2" mir_rel="$3" output_rel="$4"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$mir_rel" -o "$output_rel") \
        >"$WORK_DIR/$name.$backend.out" 2>"$WORK_DIR/$name.$backend.err"
}

for source in tests/self_hosted/fixtures/direct_mir_option_record_match.pgy; do
    name="$(basename "$source" .pgy)"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$source" -o "$WORK_REL/$name.mir.json") \
        >"$WORK_DIR/$name.producer.out" 2>"$WORK_DIR/$name.producer.err" || {
            cat "$WORK_DIR/$name.producer.err" >&2; fail "$name: MIR production failed"; }
    (cd "$ROOT_DIR" && "$PGY" "$source" --native-pipeline --backend=c -o "$WORK_REL/$name.native.exe") \
        >"$WORK_DIR/$name.native.out" 2>&1 ||
        { cat "$WORK_DIR/$name.native.out" >&2; fail "$name: native compile failed"; }
    "$WORK_DIR/$name.native.exe" | tr -d '\r' >"$WORK_DIR/$name.expected"
    [[ -s "$WORK_DIR/$name.expected" ]] || fail "$name: native printed nothing"
    for backend in c llvm; do
        project "$name" "$backend" "$WORK_REL/$name.mir.json" "$WORK_REL/$name.$backend" || {
            cat "$WORK_DIR/$name.$backend.out" "$WORK_DIR/$name.$backend.err" >&2
            fail "$name: $backend projection refused"; }
        bin="$WORK_DIR/$name-$backend.exe"
        if [[ "$backend" == c ]]; then
            command=("$CC" -x c -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer "$ROOT_DIR/$WORK_REL/$name.c")
            if pgy_selfhost_emitted_c_uses_runtime_headers "$ROOT_DIR/$WORK_REL/$name.c"; then
                command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
            fi
            "${command[@]}" -lm -o "$bin" 2>"$WORK_DIR/$name.c.compile.err" ||
                { cat "$WORK_DIR/$name.c.compile.err" >&2; fail "$name: C artifact did not compile"; }
        else
            "$CLANG" -x ir "$ROOT_DIR/$WORK_REL/$name.llvm" -x none "$runtime_object" -pthread -lm \
                -o "$bin" 2>"$WORK_DIR/$name.llvm.compile.err" ||
                { cat "$WORK_DIR/$name.llvm.compile.err" >&2; fail "$name: LLVM artifact did not compile"; }
        fi
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "$bin" 2>"$WORK_DIR/$name.$backend.run.err" |
            tr -d '\r' >"$WORK_DIR/$name.$backend.run" ||
            { cat "$WORK_DIR/$name.$backend.run.err" >&2; fail "$name: $backend run failed"; }
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name.$backend.run" || {
            diff "$WORK_DIR/$name.expected" "$WORK_DIR/$name.$backend.run" >&2 || true
            fail "$name: $backend output differs from native"; }
    done
done

refuse() {
    local name="$1" mir_rel="$2"
    for backend in c llvm; do
        if project "$name" "$backend" "$mir_rel" "$WORK_REL/$name.$backend"; then
            fail "$backend admitted $name"
        fi
        [[ ! -e "$ROOT_DIR/$WORK_REL/$name.$backend" ]] || fail "$backend published $name"
        grep -aq 'direct MIR scalar CFG' "$WORK_DIR/$name.$backend.out" "$WORK_DIR/$name.$backend.err" || {
            cat "$WORK_DIR/$name.$backend.out" "$WORK_DIR/$name.$backend.err" >&2
            fail "$backend refused $name before the general route"; }
    done
}

# A Some binding typed other than the payload record.
"$PYTHON_BIN" - "$WORK_DIR/direct_mir_option_record_match.mir.json" "$WORK_DIR/binding-type.mir.json" <<'PY'
import json, sys
document = json.load(open(sys.argv[1], encoding="utf-8"))
cases = [i for r in document["routines"] if r["name"] == "SumOrZero" for b in r["blocks"]
         for i in b["instructions"] if i.get("match_variant") == "Some"]
if len(cases) != 1 or cases[0].get("match_binding_types") != ["Pair"]:
    raise SystemExit("fixture has no single Some(Pair) case in SumOrZero")
cases[0]["match_binding_types"] = ["Int"]
json.dump(document, open(sys.argv[2], "w", encoding="utf-8"), separators=(",", ":"))
PY
refuse binding-type "$WORK_REL/binding-type.mir.json"

# An Option of a record holding a String: its binding would share the
# payload's storage, which no release plan owns here.
source=tests/self_hosted/fixtures/direct_mir_option_record_storage_match_refused.pgy
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$source" -o "$WORK_REL/storage.mir.json") \
    >"$WORK_DIR/storage.producer.out" 2>"$WORK_DIR/storage.producer.err" || {
        cat "$WORK_DIR/storage.producer.err" >&2; fail "storage: MIR production failed"; }
refuse storage "$WORK_REL/storage.mir.json"

echo "[$LABEL] PASS: Option<record> match program equal to native on C (ASan) and LLVM; 2 refusals"
