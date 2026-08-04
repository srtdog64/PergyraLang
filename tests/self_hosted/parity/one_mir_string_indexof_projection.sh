#!/usr/bin/env bash
# StringIndexOf result-range and checked String windows in one GraphPlan.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-string-indexof"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_string_indexof"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/str_indexof.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

[[ -x "$DRIVER" ]] || fail "current-source self-host driver is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"

while IFS='|' read -r owner cap; do
    [[ -z "$owner" || "$owner" == \#* ]] && continue
    lines="$(wc -l <"$ROOT_DIR/$owner")"
    [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: $owner=$lines/$cap"
done <"$ROOT_DIR/tests/self_hosted/parity/scalar_program_owner_caps.tsv"

GRAPH="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
SIGNATURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_string_window_builtin_signature_owner.pgy"
CONTRACT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_string_index_runtime_owner.pgy"
RANGE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_string_index_range_owner.pgy"
C_RUNTIME="$ROOT_DIR/src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy"
LLVM_RUNTIME="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_index_materialization_owner.pgy"
require_text "$GRAPH" 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v22'
require_text "$SIGNATURE" 'StringIndexOf'
require_text "$CONTRACT" 'missing_value == -1'
require_text "$CONTRACT" 'length_headroom == 1'
require_text "$RANGE" 'DirectMirScalarCfgProgramStringIndexWindowReady'
require_text "$C_RUNTIME" 'StringRuntimeCStringIndexOfBlock'
require_text "$LLVM_RUNTIME" 'phi i64 [ %index, %found ], [ -1, %absent ]'
for owner in "$SIGNATURE" "$CONTRACT" "$RANGE" "$C_RUNTIME" "$LLVM_RUNTIME" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_string_search_expression_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_search_expression_owner.pgy"; do
    reject_text "$owner" 'str_indexof.pgy'
    reject_text "$owner" 'hello,world'
    reject_text "$owner" 'hello'
done

mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/producer.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "current producer rejected source"
mir_sha="$(sha256sum "$WORK_DIR/producer.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$mir_sha" == "28F0C0C026E62F749AEF2150B5100444962B6260D242F4560E9A2262954F1C75" ]] ||
    fail "source MIR identity changed: $mir_sha"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_string_indexof_mutations.py" \
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

goods=(program display-only semantic-change absent-search empty-needle)
bads=(bad-result-type bad-argument-chain bad-argument-type \
    bad-unregistered-target bad-target-syntax bad-index-step \
    bad-length-index-relation)
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    for good in "${goods[@]}"; do
        stem="$good"; [[ "$good" == program ]] && stem=base
        project "$good" "$stem" "$target" "$suffix" || fail "$target rejected $good"
    done
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" ||
        fail "$target display text changed the artifact"
    ! cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/semantic-change.$suffix" ||
        fail "$target semantic mutation did not change the artifact"
    ! cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/absent-search.$suffix" ||
        fail "$target absent-search mutation did not change the artifact"
    ! cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/empty-needle.$suffix" ||
        fail "$target empty-needle mutation did not change the artifact"
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

for symbol in pgy_strindexof pgy_strlen pgy_substr; do
    require_text "$WORK_DIR/base.c" "$symbol"
    require_text "$WORK_DIR/base.ll" "$symbol"
done
require_text "$WORK_DIR/base.c" 'if (raw_len >= 9223372036854775807ULL || start < 0 || len <= 0'
require_text "$WORK_DIR/base.ll" '%source.large = icmp uge i64 %source.length, 9223372036854775807'
expected_base=$'5\n-1\nhello\nworld'
expected_semantic=$'3\n-1\nhey\nworld'
expected_absent=$'-1\n-1\n\nhello,world'
expected_empty=$'0\n-1\n\nello,world'
for stem in base display-only semantic-change absent-search empty-needle; do
    "$CC" -std=c11 "$WORK_DIR/$stem.c" -o "$WORK_DIR/$stem.c.exe" ||
        fail "C compile failed: $stem"
    "$CLANG" "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem.llvm.exe" ||
        fail "LLVM compile failed: $stem"
    expected="$expected_base"
    [[ "$stem" == semantic-change ]] && expected="$expected_semantic"
    [[ "$stem" == absent-search ]] && expected="$expected_absent"
    [[ "$stem" == empty-needle ]] && expected="$expected_empty"
    c_out="$("$WORK_DIR/$stem.c.exe" | sed 's/\r$//')" || fail "C execution failed: $stem"
    llvm_out="$("$WORK_DIR/$stem.llvm.exe" | sed 's/\r$//')" || fail "LLVM execution failed: $stem"
    [[ "$c_out" == "$expected" ]] || fail "C stdout changed: $stem=$c_out"
    [[ "$llvm_out" == "$expected" ]] || fail "LLVM stdout changed: $stem=$llvm_out"
done
final_sha="$(sha256sum "$WORK_DIR/producer.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$final_sha" == "$mir_sha" ]] || fail "projection mutated admitted MIR"
echo "[$LABEL] ok: StringIndexOf range and checked windows execute from one typed GraphPlan in C and LLVM"
