#!/usr/bin/env bash
# Runtime String/Array<String> expressions through one typed scalar GraphPlan.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-string-collection-builtin"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_string_collection_builtin"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/str_builtins2.pgy"

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
INPUT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_input_owner.pgy"
KIND="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_id_owner.pgy"
ABI="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_abi_owner.pgy"
require_text "$GRAPH" 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v78'
require_text "$INPUT" 'DirectMirScalarCfgProgramGraphInputFromAdmitted'
require_text "$KIND" 'DirectMirScalarProgramExpressionKindLast'
require_text "$ABI" 'DirectMirScalarProgramArrayStringAbiFactFromAdmitted'
for owner in "$INPUT" "$KIND" "$ABI" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_collection_expression_kind_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_collection_builtin_signature_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_string_collection_expression_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_string_collection_expression_owner.pgy"; do
    reject_text "$owner" 'str_builtins2.pgy'
    reject_text "$owner" 'a,bb,c'
    reject_text "$owner" 'hello world'
done

mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/producer.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "current producer rejected source"
mir_sha="$(sha256sum "$WORK_DIR/producer.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$mir_sha" == "035EE7F341BC388E2B1CB2F36D6792FEC1DDF7C4D7433B10B8DEDED6E0648E97" ]] ||
    fail "source MIR identity changed: $mir_sha"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_string_collection_builtin_mutations.py" \
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

goods=(program display-only semantic-change)
bads=(bad-split-result-type bad-split-argument-chain \
    bad-contains-argument-type bad-join-argument-type bad-join-order \
    bad-array-index-type bad-unregistered-target \
    bad-split-syntax-identity bad-array-layout)
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
    for bad in "${bads[@]}"; do
        if project "$bad" "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -e "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        reject_text "$WORK_DIR/$bad.$target.out" 'direct MIR String array collection is invalid'
        reject_text "$WORK_DIR/$bad.$target.err" 'direct MIR String array collection is invalid'
        reject_text "$WORK_DIR/$bad.$target.out" 'direct MIR CFG merge phi'
        reject_text "$WORK_DIR/$bad.$target.err" 'direct MIR CFG merge phi'
        reject_text "$WORK_DIR/$bad.$target.out" 'local type inventory is missing'
        reject_text "$WORK_DIR/$bad.$target.err" 'local type inventory is missing'
    done
done

for symbol in pgy_strcontains pgy_split pgy_strjoin pgy_toint pgy_as_len pgy_as_get pgy_as_drop_owned; do
    require_text "$WORK_DIR/base.c" "$symbol"
    require_text "$WORK_DIR/base.ll" "$symbol"
done
require_text "$WORK_DIR/base.c" 'offsetof(pgy_as, allocator)'
require_text "$WORK_DIR/base.ll" '%pgy.array.string = type { ptr, i64, i64, ptr }'
expected_base=$'yes\n3\nbb\na|bb|c\n43\n1\n2\nleft\nleft/right'
expected_semantic=$'no\n3\nbbbb\na|bbbb|c\n8\n1\n2\nleft\nleft/right'
for stem in base display-only semantic-change; do
    c_command=("$CC" -std=c11 "$WORK_DIR/$stem.c")
    if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK_DIR/$stem.c"; then
        c_command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
    fi
    c_command+=(-o "$WORK_DIR/$stem.c.exe")
    "${c_command[@]}" || fail "C compile failed: $stem"
    "$CLANG" "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem.llvm.exe" ||
        fail "LLVM compile failed: $stem"
    expected="$expected_base"; [[ "$stem" == semantic-change ]] && expected="$expected_semantic"
    c_out="$("$WORK_DIR/$stem.c.exe" | sed 's/\r$//')" || fail "C execution failed: $stem"
    llvm_out="$("$WORK_DIR/$stem.llvm.exe" | sed 's/\r$//')" || fail "LLVM execution failed: $stem"
    [[ "$c_out" == "$expected" ]] || fail "C stdout changed: $stem=$c_out"
    [[ "$llvm_out" == "$expected" ]] || fail "LLVM stdout changed: $stem=$llvm_out"
done
final_sha="$(sha256sum "$WORK_DIR/producer.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$final_sha" == "$mir_sha" ]] || fail "projection mutated admitted MIR"
echo "[$LABEL] ok: runtime String/Array<String> expressions execute from one typed GraphPlan in C and LLVM"
