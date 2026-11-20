#!/usr/bin/env bash
# The general MIR emitter compiles one Option<Int> match on C and LLVM, and
# refuses seven forged pattern, binding type, use, successor and ABI layout
# (tag, field offset, layout id) rows without publishing an artifact: the
# captured Option layout row is the only layout authority. This program used
# to be claimed by a per-program-shape Option match route, deleted on
# 2026-11-20.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-option-match"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_option_match"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/mir_lower/fixture/option_match.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }

[[ -x "$DRIVER" ]] || fail "current-source self-host driver is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "current producer rejected source"
"$PYTHON_BIN" - "$WORK_DIR/program.json" "$WORK_DIR" <<'PY'
import copy, json, pathlib, sys
source, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
base = json.loads(source.read_text(encoding="utf-8"))
def emit(name, mutate):
    doc = copy.deepcopy(base); mutate(doc["routines"][0]["blocks"])
    (out / f"{name}.json").write_text(
        json.dumps(doc, separators=(",", ":")), encoding="utf-8")
emit("pattern", lambda b: b[2]["instructions"][0].__setitem__("match_patterns", ["Bogus(v)"]))
emit("binding-type", lambda b: b[2]["instructions"][0].__setitem__("match_binding_types", ["Bool"]))
emit("extra-use", lambda b: b[0]["instructions"][1].__setitem__("uses", ["val.1", "val.1"]))
emit("successor", lambda b: b[4].__setitem__("succ_true", 6))
emit("abi-tag", lambda b: b[0]["instructions"][0]["abi_layout"].__setitem__("primary_tag", 1))
emit("abi-offset", lambda b: b[0]["instructions"][0]["abi_layout"]["fields"][1].__setitem__("offset", 0))
emit("abi-layout-id", lambda b: b[0]["instructions"][0].__setitem__("abi_layout_id", 576045278))
PY

project() {
    local input="$1" target="$2" suffix="$3" code=0
    rm -f "$WORK_DIR/$input.$suffix"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$input.$suffix") \
        >"$WORK_DIR/$input.$target.out" 2>"$WORK_DIR/$input.$target.err" || code=$?
    [[ "$code" -ne 0 ]] && return "$code"
    ! grep -Fq 'CODEGEN ERROR' "$WORK_DIR/$input.$target.out" \
        "$WORK_DIR/$input.$target.err"
}

"$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -O0 -g \
    -fsanitize=address -fno-omit-frame-pointer \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" \
    >"$WORK_DIR/runtime.compile.log" 2>&1 || fail "LLVM runtime did not compile"

run_artifact() {
    local target="$1" suffix="$2" exe="$WORK_DIR/program-$1.exe"
    if [[ "$target" == c ]]; then
        (cd "$ROOT_DIR" && "$CC" -x c -std=c11 -O0 -g -fsanitize=address \
            -fno-omit-frame-pointer -Isrc -Isrc/runtime -pthread \
            "$WORK_DIR/program.c" -lm -o "$exe") >"$WORK_DIR/program-$target.compile" 2>&1 ||
            fail "C did not compile"
    else
        "$CLANG" -x ir -O0 -g -fsanitize=address -fno-omit-frame-pointer \
            "$WORK_DIR/program.ll" -x none "$WORK_DIR/runtime.o" -pthread -lm \
            -o "$exe" >"$WORK_DIR/program-$target.compile" 2>&1 ||
            fail "LLVM did not compile"
    fi
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "$exe" 2>"$WORK_DIR/program-$target.asan" |
        tr -d '\r' >"$WORK_DIR/program-$target.run" || fail "$target failed under ASan"
}

bads=(pattern binding-type extra-use successor abi-tag abi-offset abi-layout-id)
printf '42\n42\n' >"$WORK_DIR/expected.run"
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    project program "$target" "$suffix" || fail "$target rejected the Option match"
    [[ -s "$WORK_DIR/program.$suffix" ]] || fail "$target published no artifact"
    run_artifact "$target" "$suffix"
    cmp -s "$WORK_DIR/program-$target.run" "$WORK_DIR/expected.run" ||
        fail "$target printed other output"
    for bad in "${bads[@]}"; do
        if project "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -e "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        grep -Eq '(CODEGEN ERROR|MIR)' "$WORK_DIR/$bad.$target.out" \
            "$WORK_DIR/$bad.$target.err" || fail "$target refused $bad without a diagnostic"
    done
done

echo "[$LABEL] general emitter: Option<Int> match 42/42 on C/LLVM under ASan; ${#bads[@]} forged MIR refused with no artifact"
