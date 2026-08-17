#!/usr/bin/env bash
# StringTrim expression and runtime contract through one GraphPlan.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-string-trim"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_string_trim"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/str_trim.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

[[ -x "$DRIVER" ]] || fail "current-source self-host driver is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"

while IFS='|' read -r owner cap; do
    cap="${cap%$'\r'}"
    [[ -z "$owner" || "$owner" == \#* ]] && continue
    lines="$(wc -l <"$ROOT_DIR/$owner")"
    [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: $owner=$lines/$cap"
done <"$ROOT_DIR/tests/self_hosted/parity/scalar_program_owner_caps.tsv"

GRAPH="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
SIGNATURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_string_transform_builtin_signature_owner.pgy"
CONTRACT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_string_trim_runtime_owner.pgy"
C_RUNTIME="$ROOT_DIR/src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy"
LLVM_RUNTIME="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_trim_materialization_owner.pgy"
LLVM_DECLS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_foreign_declaration_owner.pgy"
require_text "$GRAPH" 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v78'
require_text "$SIGNATURE" 'StringTrim'
require_text "$CONTRACT" 'ascii_whitespace_mask == 15'
require_text "$CONTRACT" 'returns_owned_string'
require_text "$C_RUNTIME" 'StringRuntimeCStringTrimBlock'
require_text "$LLVM_RUNTIME" '%leading.ws = or i1'
require_text "$LLVM_DECLS" 'needs_memcpy'
reject_text "$LLVM_RUNTIME" 'declare ptr @memcpy'
for owner in "$SIGNATURE" "$CONTRACT" "$C_RUNTIME" "$LLVM_RUNTIME" "$LLVM_DECLS" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_string_transform_expression_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_transform_expression_owner.pgy"; do
    reject_text "$owner" 'str_trim.pgy'
    reject_text "$owner" 'hello world'
    reject_text "$owner" 'hello codex'
done

mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/producer.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "current producer rejected source"
mir_sha="$(sha256sum "$WORK_DIR/producer.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$mir_sha" == "1A10A12B315C2B48E715441966738724C0E1D8E5A120766DC87987E494D52BE8" ]] ||
    fail "source MIR identity changed: $mir_sha"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_string_trim_mutations.py" \
    "$WORK_DIR/producer.json" "$WORK_DIR"

project() {
    local input="$1" stem="$2" target="$3" suffix="$4" code=0
    rm -f "$WORK_DIR/$stem.$suffix"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$stem.$suffix") \
        >"$WORK_DIR/$stem.$target.out" 2>"$WORK_DIR/$stem.$target.err" || code=$?
    [[ "$code" -ne 0 ]] && return "$code"
    ! grep -Fq 'CODEGEN ERROR' "$WORK_DIR/$stem.$target.out" \
        "$WORK_DIR/$stem.$target.err"
}

goods=(program display-only semantic-change already-trimmed empty-source)
bads=(bad-result-type bad-argument-chain bad-argument-type \
    bad-unregistered-target bad-target-syntax bad-missing-argument)
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    for good in "${goods[@]}"; do
        stem="$good"; [[ "$good" == program ]] && stem=base
        project "$good" "$stem" "$target" "$suffix" || fail "$target rejected $good"
    done
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" ||
        fail "$target display text changed the artifact"
    for changed in semantic-change already-trimmed empty-source; do
        ! cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/$changed.$suffix" ||
            fail "$target $changed did not change the artifact"
    done
    for bad in "${bads[@]}"; do
        if project "$bad" "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -e "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        grep -Fq -- 'direct MIR scalar CFG program' "$WORK_DIR/$bad.$target.out" \
            "$WORK_DIR/$bad.$target.err" || fail "$target $bad escaped scalar owner"
        reject_text "$WORK_DIR/$bad.$target.out" 'local type inventory is missing'
        reject_text "$WORK_DIR/$bad.$target.err" 'local type inventory is missing'
    done
done

require_text "$WORK_DIR/base.c" 'pgy_strtrim'
require_text "$WORK_DIR/base.ll" 'define internal ptr @pgy_strtrim'
for symbol in strlen malloc memcpy abort; do
    count="$(grep -Ec "^declare .*@$symbol\\(" "$WORK_DIR/base.ll")"
    [[ "$count" == 1 ]] || fail "LLVM declaration count drift: $symbol=$count"
done
expected_base=$'hello world\n11\n0\n[x]'
expected_semantic=$'hello codex\n11\n0\n[x]'
expected_already_trimmed=$'alpha\n5\n0\n[x]'
expected_empty=$'\n0\n0\n[x]'
for stem in base display-only semantic-change already-trimmed empty-source; do
    "$CC" -std=c11 "$WORK_DIR/$stem.c" -o "$WORK_DIR/$stem.c.exe" ||
        fail "C compile failed: $stem"
    "$CLANG" "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem.llvm.exe" ||
        fail "LLVM compile failed: $stem"
    expected="$expected_base"
    [[ "$stem" == semantic-change ]] && expected="$expected_semantic"
    [[ "$stem" == already-trimmed ]] && expected="$expected_already_trimmed"
    [[ "$stem" == empty-source ]] && expected="$expected_empty"
    c_out="$("$WORK_DIR/$stem.c.exe" | sed 's/\r$//')" || fail "C execution failed: $stem"
    llvm_out="$("$WORK_DIR/$stem.llvm.exe" | sed 's/\r$//')" || fail "LLVM execution failed: $stem"
    [[ "$c_out" == "$expected" ]] || fail "C stdout changed: $stem=$c_out"
    [[ "$llvm_out" == "$expected" ]] || fail "LLVM stdout changed: $stem=$llvm_out"
done
final_sha="$(sha256sum "$WORK_DIR/producer.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$final_sha" == "$mir_sha" ]] || fail "projection mutated admitted MIR"
echo "[$LABEL] ok: StringTrim executes from one typed GraphPlan with one LLVM declaration owner"
