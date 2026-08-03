#!/usr/bin/env bash
# One typed scalar-CFG plan executes sequential Int and String foreach loops.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-mixed-collection-foreach"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_mixed_collection_foreach"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/for_each.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$DRIVER" ]] || fail "installed self-host driver is unavailable"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required for structured falsifiers"
ROUTE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_route_owner.pgy"
TYPE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_type_family_owner.pgy"
STRING_OP="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_expression_owner.pgy"
ELEMENTS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_element_owner.pgy"
BACKEND="$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy"
require_text "$TYPE_OWNER" 'DirectMirScalarCfgIterationTypePairSupported('
require_text "$STRING_OP" 'DirectMirScalarCfgOpConcatString()'
require_text "$STRING_OP" 'SemanticCallTargetDirect()'
require_text "$ELEMENTS" 'DirectMirScalarCfgForEachElementFacts'
require_text "$BACKEND" 'DirectMirScalarCfgGraphRouteClaimed(admitted)'
reject_text "$ROUTE" 'block_count == 7'
reject_text "$ROUTE" 'source_local_types['
for owner in "$TYPE_OWNER" "$STRING_OP" "$ELEMENTS"; do
    reject_text "$owner" 'for_each.pgy'
    reject_text "$owner" '"expr0"'
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") || fail "installed producer rejected source"
[[ -s "$WORK_DIR/program.json" ]] || fail "installed producer emitted no MIR"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_mixed_collection_foreach_mutations.py" \
    "$WORK_DIR/program.json" "$WORK_DIR"

project() {
    local input="$1" stem="$2" target="$3" suffix="$4"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$stem.$suffix") \
        >"$WORK_DIR/$stem.$target.out" 2>"$WORK_DIR/$stem.$target.err"
}
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    project program base "$target" "$suffix" || fail "$target rejected mixed foreach"
    project iteration-order iteration-order "$target" "$suffix" || fail "$target rejected iteration order"
    project graph-strings graph-strings "$target" "$suffix" || fail "$target rejected graph strings"
    project display-only display-only "$target" "$suffix" || fail "$target read display expr0"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/iteration-order.$suffix" || fail "$target iteration order changed artifact"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" || fail "$target display expr0 changed artifact"
    for bad in bad-binding bad-iterable bad-source-local bad-abi-type bad-abi-offset bad-runtime bad-inner-type bad-string-spine bad-concat-target bad-concat-edge bad-local-ref-id bad-local-ref-node stale-log-use; do
        if project "$bad" "$bad" "$target" "$suffix"; then fail "$target accepted $bad"; fi
        [[ ! -s "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        grep -Eq 'direct MIR (scalar CFG|Array<String>)' "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || fail "$target lost owned diagnostic for $bad"
        ! grep -Fq 'direct MIR Option match' "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || fail "$target retried $bad as Option"
    done
done

"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/base.c" -o "$WORK_DIR/base-c.exe" || fail "C artifact did not compile"
"$CLANG" -x ir "$WORK_DIR/base.ll" -o "$WORK_DIR/base-llvm.exe" || fail "LLVM artifact did not compile"
"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/graph-strings.c" -o "$WORK_DIR/graph-c.exe" || fail "graph C did not compile"
"$CLANG" -x ir "$WORK_DIR/graph-strings.ll" -o "$WORK_DIR/graph-llvm.exe" || fail "graph LLVM did not compile"
printf '60\nabbccc\n' >"$WORK_DIR/expected.txt"
printf '60\nxyyzzz\n' >"$WORK_DIR/graph-expected.txt"
for exe in base-c base-llvm; do "$WORK_DIR/$exe.exe" | tr -d '\r' >"$WORK_DIR/$exe.run"; cmp -s "$WORK_DIR/$exe.run" "$WORK_DIR/expected.txt" || fail "$exe output drift"; done
for exe in graph-c graph-llvm; do "$WORK_DIR/$exe.exe" | tr -d '\r' >"$WORK_DIR/$exe.run"; cmp -s "$WORK_DIR/$exe.run" "$WORK_DIR/graph-expected.txt" || fail "$exe ignored string graph"; done
echo "[$LABEL] one typed receipt owns mixed Int/String foreach C/LLVM execution"
