#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate:
#   the native C backend's declared structure or end-to-end compile speed
#   regressed.
# That is a fact about the native pipeline, so the gate compiles
# in-process instead of delegating to the installed self-host driver.
# Delegated, a self-host coverage gap would read as a regression in
# the subject above. Declared per harness because the compiler is
# reached through make and nested scripts, and the variable is the
# same declared opt-out as --native-pipeline -- never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE
trap 'rc=$?; printf "[perf-contract] unhandled failure at line %s (rc=%s)\n" "$LINENO" "$rc" >&2' ERR

case ":$PATH:" in
    *:/usr/bin:*) ;;
    *) if [ -d /usr/bin ]; then PATH="/usr/bin:$PATH"; fi ;;
esac
case ":$PATH:" in
    *:/bin:*) ;;
    *) if [ -d /bin ]; then PATH="/bin:$PATH"; fi ;;
esac
export PATH

SCRIPT_PATH="${BASH_SOURCE[0]}"
case "$SCRIPT_PATH" in
    */*) SCRIPT_DIR="${SCRIPT_PATH%/*}" ;;
    *) SCRIPT_DIR="." ;;
esac
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

TMPDIR="${TMPDIR:-/tmp}"
WORK_DIR="$(mktemp -d "$TMPDIR/pgy_perf_contract.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
SRC_INDEX="$WORK_DIR/src-index.txt"
CODEGEN_INDEX="$WORK_DIR/codegen-index.txt"

grep -RIn -e '.' "$ROOT_DIR/src" > "$SRC_INDEX"
grep -F "/src/codegen/" "$SRC_INDEX" > "$CODEGEN_INDEX" || true

c_fail_open_zero="$WORK_DIR/c-fail-open-zero.txt"
grep -F 'return pergyra_strdup("0")' "$CODEGEN_INDEX" \
    | grep -Fv 'src/codegen/transpiler_let_box_emit.c:' \
    > "$c_fail_open_zero" || true
if [ -s "$c_fail_open_zero" ]; then
    echo "[perf-contract] C backend reintroduced silent zero fail-open fallback" >&2
    cat "$c_fail_open_zero" >&2
    exit 1
fi
if grep -Fq 'return pergyra_strdup("false")' "$CODEGEN_INDEX"; then
    echo "[perf-contract] C backend reintroduced silent false fail-open fallback" >&2
    grep -F 'return pergyra_strdup("false")' "$CODEGEN_INDEX" >&2
    exit 1
fi
if grep -Fq 'return pergyra_strdup("/*' "$CODEGEN_INDEX"; then
    echo "[perf-contract] C backend reintroduced comment-only fail-open fallback" >&2
    grep -F 'return pergyra_strdup("/*' "$CODEGEN_INDEX" >&2
    exit 1
fi

LOG="$WORK_DIR/test-abi-perf.log"
SUMMARY="$WORK_DIR/perf-summary.txt"

cat > "$LOG" <<'EOF'
  c/projection_abi: wrote test source /tmp/projection_abi.pgy
metrics: compile=0.500s run=0.002s
  c/intent_authority_snapshot_abi: wrote test source /tmp/intent_authority_snapshot_abi.pgy
metrics: compile=1.750s run=0.004s
  llvm/projection_abi: wrote test source /tmp/projection_abi.pgy
metrics: compile=0.250s run=0.003s
EOF

bash "$ROOT_DIR/tests/perf_summary.sh" "$LOG" > "$SUMMARY"

grep -Fq "backend cases compile_avg_s compile_max_s compile_max_case run_avg_s run_max_s run_max_case" "$SUMMARY"
grep -Fq "c 2 1.125 1.750 intent_authority_snapshot_abi 0.003 0.004 intent_authority_snapshot_abi" "$SUMMARY"
grep -Fq "llvm 1 0.250 0.250 projection_abi 0.003 0.003 projection_abi" "$SUMMARY"

# --- Measured compile-speed contract (docs/189 C14-④) -------------------
# The synthetic block above unit-tests the summary PARSER on fixed input;
# it never measured anything, so compiler-speed regressions landed
# silently. This block times a real end-to-end C-backend compile of a
# small fixed fixture. The ceiling is deliberately catastrophic-only
# (default 60s, PGY_COMPILE_SPEED_CEILING_MS overrides): machine variance
# is ~9% on this corpus, so a tight threshold would flake -- the contract
# here is "compiling a 60-line program never becomes pathological"
# (the hang/blow-up class), not a benchmark. The measured number is
# printed so CI logs accumulate a history.
COMPILE_SPEED_PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [ ! -x "$COMPILE_SPEED_PGY" ] && [ -x "${COMPILE_SPEED_PGY}.exe" ]; then
    COMPILE_SPEED_PGY="${COMPILE_SPEED_PGY}.exe"
fi
# Windows: pgy links LLVM/runtime DLLs; without the path helpers the
# binary cannot even launch from a bare shell.
if [ -f "$ROOT_DIR/tests/pgy_binary_path_helpers.sh" ]; then
    # shellcheck source=/dev/null
    source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
    pgy_prepend_windows_runtime_paths
fi
if [ -x "$COMPILE_SPEED_PGY" ]; then
    SPEED_SRC="$WORK_DIR/compile_speed_probe.pgy"
    {
        printf 'func Main() -> Void {\n    var total: Int = 0;\n'
        i=0
        while [ "$i" -lt 60 ]; do
            printf '    total = total + %d;\n' "$i"
            i=$((i + 1))
        done
        printf '    Log(ToString(total));\n}\n'
    } > "$SPEED_SRC"
    SPEED_CEILING_MS="${PGY_COMPILE_SPEED_CEILING_MS:-60000}"
    speed_start_ns="$(date +%s%N)"
    if ! "$COMPILE_SPEED_PGY" "$SPEED_SRC" -o "$WORK_DIR/compile_speed_probe.out" \
        >"$WORK_DIR/compile_speed_probe.log" 2>&1; then
        echo "[perf-contract] compile-speed probe failed to compile" >&2
        tail -5 "$WORK_DIR/compile_speed_probe.log" >&2
        exit 1
    fi
    speed_end_ns="$(date +%s%N)"
    speed_elapsed_ms=$(( (speed_end_ns - speed_start_ns) / 1000000 ))
    echo "[perf-contract] measured end-to-end C compile: ${speed_elapsed_ms}ms (ceiling ${SPEED_CEILING_MS}ms)"
    if [ "$speed_elapsed_ms" -gt "$SPEED_CEILING_MS" ]; then
        echo "[perf-contract] compile-speed contract breached: ${speed_elapsed_ms}ms > ${SPEED_CEILING_MS}ms" >&2
        exit 1
    fi
else
    echo "[perf-contract] NOTE: pgy binary not present; measured compile-speed section skipped" >&2
fi

PARSER_TREE_FIXTURE="$WORK_DIR/parser-tree-census.ast"
PARSER_TREE_SUMMARY="$WORK_DIR/parser-tree-census.txt"
cat > "$PARSER_TREE_FIXTURE" <<'EOF'
Root:
  A:
    X
  A:
    X
EOF
bash "$ROOT_DIR/scripts/parser_tree_network_census.sh" \
    "$PARSER_TREE_FIXTURE" > "$PARSER_TREE_SUMMARY"
grep -Fqx \
    "nodes=5 edges=4 roots=1 unique_subtrees=3 hash_cons_removed=2 hash_cons_removed_pct=40.0" \
    "$PARSER_TREE_SUMMARY"

scope_mixed_files=$(
    grep -R -l "llvm_scope_lookup(" "$ROOT_DIR/src/codegen" \
        --include='*.c' --include='*.h' | \
    while IFS= read -r file; do
        if ! grep -Eq "llvm_scope_(declare|push|pop)\(" "$file"; then
            continue
        fi
        rel=${file#"$ROOT_DIR"/}
        if [ "$rel" = "src/codegen/llvm_registry.c" ] \
            || [ "$rel" = "src/codegen/llvm_internal_api.h" ] \
            || [ "$rel" = "src/codegen/llvm_stmt_parallel_async.c" ]; then
            continue
        fi
        printf '%s\n' "$rel"
    done
)
if [ -n "$scope_mixed_files" ]; then
    echo "[perf-contract] LLVM borrowed scope lookup mixed with scope mutation outside approved owners:" >&2
    printf '%s\n' "$scope_mixed_files" >&2
    exit 1
fi
# A borrowed LLVMVarEntry * dies on scope realloc (AGENTS.md); only the
# registry owner may take one.
scope_lookup_outside_registry="$(
    grep -RIl --include='*.c' --include='*.h' 'llvm_scope_lookup(ctx,'         "$ROOT_DIR/src/codegen"         | sed -E 's#^.*src/codegen/#src/codegen/#'         | grep -vx 'src/codegen/llvm_registry.c' || true
)"
if [ -n "$scope_lookup_outside_registry" ]; then
    echo "[perf-contract] LLVM borrowed scope lookup outside llvm_registry.c:" >&2
    printf '%s
' "$scope_lookup_outside_registry" >&2
    exit 1
fi

forward_allowed_type_names="$(
    sed -n '/static const char \*allowed_names\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/transpiler_func_forward_policy.c" \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
forward_allowed_type_names_sorted="$(
    printf '%s\n' "$forward_allowed_type_names" | sort
)"
if [[ "$forward_allowed_type_names" != "$forward_allowed_type_names_sorted" ]]; then
    echo "function forward allowed type names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$forward_allowed_type_names_sorted") \
        <(printf '%s\n' "$forward_allowed_type_names") >&2 || true
    exit 1
fi

lexer_keyword_names="$(
    grep -E '^[[:space:]]*"[A-Za-z0-9_]+",[[:space:]]+PGY_KEYWORD_CLASS_RESERVED,' \
        "$ROOT_DIR/src/lexer/language_keyword_registry.def" \
        | sed -E 's/^[[:space:]]*"([A-Za-z0-9_]+)".*/\1/'
)"
if [[ -z "$lexer_keyword_names" ]]; then
    echo "lexer keyword registry must own at least one reserved row" >&2
    exit 1
fi
lexer_keyword_names_sorted="$(
    printf '%s\n' "$lexer_keyword_names" | sort
)"
if [[ "$lexer_keyword_names" != "$lexer_keyword_names_sorted" ]]; then
    echo "lexer keyword names must stay sorted for binary lookup" >&2
    diff -u <(printf '%s\n' "$lexer_keyword_names_sorted") \
        <(printf '%s\n' "$lexer_keyword_names") >&2 || true
    exit 1
fi
lexer_keyword_name_duplicates="$(
    printf '%s\n' "$lexer_keyword_names" | uniq -d
)"
if [[ -n "$lexer_keyword_name_duplicates" ]]; then
    echo "lexer keyword registry must not contain duplicate spellings" >&2
    printf '%s\n' "$lexer_keyword_name_duplicates" >&2
    exit 1
fi

match_variant_names="$(
    sed -n '/static const PgyMatchVariantSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/common/match_variant_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
match_variant_names_sorted="$(
    printf '%s\n' "$match_variant_names" | sort
)"
if [[ "$match_variant_names" != "$match_variant_names_sorted" ]]; then
    echo "match variant destructor names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$match_variant_names_sorted") \
        <(printf '%s\n' "$match_variant_names") >&2 || true
    exit 1
fi

llvm_registry_type_names="$(
    sed -n '/static const LLVMRegistryTypeSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_backend_type_registry.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_registry_type_names_sorted="$(
    printf '%s\n' "$llvm_registry_type_names" | sort
)"
if [[ "$llvm_registry_type_names" != "$llvm_registry_type_names_sorted" ]]; then
    echo "LLVM type registry names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_registry_type_names_sorted") \
        <(printf '%s\n' "$llvm_registry_type_names") >&2 || true
    exit 1
fi

mir_resource_op_names="$(
    sed -n '/static const TranspilerMIRResourceOpSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/transpiler_mir_resource_name.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
mir_resource_op_names_sorted="$(
    printf '%s\n' "$mir_resource_op_names" | sort
)"
if [[ "$mir_resource_op_names" != "$mir_resource_op_names_sorted" ]]; then
    echo "MIR resource op names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$mir_resource_op_names_sorted") \
        <(printf '%s\n' "$mir_resource_op_names") >&2 || true
    exit 1
fi

common_intent_obs_names="$(
    grep -o '"Intent[A-Za-z0-9_]*"' "$ROOT_DIR/src/common/intent_observability_abi.def" \
        | tr -d '"' \
        | grep -vx "Intent" \
        | sort -u
)"
common_intent_obs_names_in_order="$(
    grep -o '"Intent[A-Za-z0-9_]*"' "$ROOT_DIR/src/common/intent_observability_abi.def" \
        | tr -d '"' \
        | grep -vx "Intent"
)"
if [[ "$common_intent_obs_names_in_order" != "$common_intent_obs_names" ]]; then
    echo "common intent observability names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$common_intent_obs_names") \
        <(printf '%s\n' "$common_intent_obs_names_in_order") >&2 || true
    exit 1
fi
mir_intent_carrier_names="$(
    sed -n '/k_mir_intent_semantic_carrier_names\[\]/,/^};/p' \
        "$ROOT_DIR/src/compiler/mir_intent_fact.c" \
        | grep -o '"Intent[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
mir_intent_carrier_names_sorted="$(
    printf '%s\n' "$mir_intent_carrier_names" | sort
)"
if [[ "$mir_intent_carrier_names" != "$mir_intent_carrier_names_sorted" ]]; then
    echo "MIR intent semantic carrier names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$mir_intent_carrier_names_sorted") \
        <(printf '%s\n' "$mir_intent_carrier_names") >&2 || true
    exit 1
fi
parser_call_type_arg_names="$(
    sed -n '/parser_name_accepts_call_type_arguments/,/^}/p' \
        "$ROOT_DIR/src/parser/parser_expr.c" \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
parser_call_type_arg_names_sorted="$(
    printf '%s\n' "$parser_call_type_arg_names" | sort
)"
if [[ "$parser_call_type_arg_names" != "$parser_call_type_arg_names_sorted" ]]; then
    echo "parser call type-argument names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$parser_call_type_arg_names_sorted") \
        <(printf '%s\n' "$parser_call_type_arg_names") >&2 || true
    exit 1
fi
parser_builtin_like_names="$(
    sed -n '/parser_name_is_builtin_like_identifier/,/^}/p' \
        "$ROOT_DIR/src/parser/parser_expr.c" \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
parser_builtin_like_names_sorted="$(
    printf '%s\n' "$parser_builtin_like_names" | sort
)"
if [[ "$parser_builtin_like_names" != "$parser_builtin_like_names_sorted" ]]; then
    echo "parser builtin-like identifier names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$parser_builtin_like_names_sorted") \
        <(printf '%s\n' "$parser_builtin_like_names") >&2 || true
    exit 1
fi
parser_intent_value_type_names="$(
    sed -n '/intent_header_value_type_name/,/^}/p' \
        "$ROOT_DIR/src/parser/parser_intent_bindings.c" \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
parser_intent_value_type_names_sorted="$(
    printf '%s\n' "$parser_intent_value_type_names" | sort
)"
if [[ "$parser_intent_value_type_names" != "$parser_intent_value_type_names_sorted" ]]; then
    echo "parser intent header value type names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$parser_intent_value_type_names_sorted") \
        <(printf '%s\n' "$parser_intent_value_type_names") >&2 || true
    exit 1
fi
io_boundary_builtin_names="$(
    sed -n '/kPgyCompilerIOBoundaryBuiltinNames\[\]/,/^};/p' \
        "$ROOT_DIR/src/compiler/io_boundary_builtin.c" \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
io_boundary_builtin_names_sorted="$(
    printf '%s\n' "$io_boundary_builtin_names" | sort
)"
if [[ "$io_boundary_builtin_names" != "$io_boundary_builtin_names_sorted" ]]; then
    echo "compiler IO boundary builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$io_boundary_builtin_names_sorted") \
        <(printf '%s\n' "$io_boundary_builtin_names") >&2 || true
    exit 1
fi
codegen_claim_slot_names="$(
    sed -n '/PgyCodegenClaimSlotSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/codegen_slot_type_policy.c" \
        | grep -o '"Claim[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
codegen_claim_slot_names_sorted="$(
    printf '%s\n' "$codegen_claim_slot_names" | sort
)"
if [[ "$codegen_claim_slot_names" != "$codegen_claim_slot_names_sorted" ]]; then
    echo "codegen claim-slot builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$codegen_claim_slot_names_sorted") \
        <(printf '%s\n' "$codegen_claim_slot_names") >&2 || true
    exit 1
fi
codegen_slot_call_names="$(
    sed -n '/PgyCodegenSlotCallSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/codegen_slot_type_policy.c" \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
codegen_slot_call_names_sorted="$(
    printf '%s\n' "$codegen_slot_call_names" | sort
)"
if [[ "$codegen_slot_call_names" != "$codegen_slot_call_names_sorted" ]]; then
    echo "codegen slot call names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$codegen_slot_call_names_sorted") \
        <(printf '%s\n' "$codegen_slot_call_names") >&2 || true
    exit 1
fi
codegen_hashmap_key_names="$(
    sed -n '/pgy_hashmap_key_specs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/codegen_hashmap_key_policy.c" \
        | grep -o '{ "[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
codegen_hashmap_key_names_sorted="$(
    printf '%s\n' "$codegen_hashmap_key_names" | sort
)"
if [[ "$codegen_hashmap_key_names" != "$codegen_hashmap_key_names_sorted" ]]; then
    echo "codegen HashMap key names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$codegen_hashmap_key_names_sorted") \
        <(printf '%s\n' "$codegen_hashmap_key_names") >&2 || true
    exit 1
fi
slot_runtime_fn_names="$(
    sed -n '/TranspilerMIRResourceOpSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/transpiler_mir_resource_name.c" \
        | grep -o '{ "[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
slot_runtime_fn_names_sorted="$(
    printf '%s\n' "$slot_runtime_fn_names" | sort
)"
if [[ "$slot_runtime_fn_names" != "$slot_runtime_fn_names_sorted" ]]; then
    echo "transpiler slot runtime op names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$slot_runtime_fn_names_sorted") \
        <(printf '%s\n' "$slot_runtime_fn_names") >&2 || true
    exit 1
fi
driver_diag_code_names="$(
    sed -n '/kDriverDiagCodeMaps\[\]/,/^};/p' \
        "$ROOT_DIR/src/compiler/driver_diag.c" \
        | grep -o 'PGY_CODE_[A-Z0-9_]*' \
        | grep -v '^PGY_CODE_$'
)"
driver_diag_code_names_sorted="$(
    printf '%s\n' "$driver_diag_code_names" | sort
)"
if [[ "$driver_diag_code_names" != "$driver_diag_code_names_sorted" ]]; then
    echo "driver diagnostic code map must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$driver_diag_code_names_sorted") \
        <(printf '%s\n' "$driver_diag_code_names") >&2 || true
    exit 1
fi
codegen_intent_obs_names="$common_intent_obs_names"
codegen_intent_obs_names_in_order="$common_intent_obs_names_in_order"
codegen_builtin_names="$(
    sed -n '/static const PgyBuiltinInfo entries\[\]/,/^    };/p' \
        "$ROOT_DIR/src/common/pgy_builtin_type_table.c" \
        | grep -o '{ "[^"]*"' \
        | sed 's/^{ "//'
)"
codegen_builtin_names_sorted="$(
    printf '%s\n' "$codegen_builtin_names" | sort
)"
if [[ "$codegen_builtin_names" != "$codegen_builtin_names_sorted" ]]; then
    echo "codegen builtin type names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$codegen_builtin_names_sorted") \
        <(printf '%s\n' "$codegen_builtin_names") >&2 || true
    exit 1
fi
if [[ "$codegen_intent_obs_names_in_order" != "$codegen_intent_obs_names" ]]; then
    echo "codegen intent observability names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$codegen_intent_obs_names") \
        <(printf '%s\n' "$codegen_intent_obs_names_in_order") >&2 || true
    exit 1
fi
if [[ "$common_intent_obs_names" != "$codegen_intent_obs_names" ]]; then
    echo "intent observability builtin names drifted between common and codegen tables" >&2
    diff -u <(printf '%s\n' "$common_intent_obs_names") \
        <(printf '%s\n' "$codegen_intent_obs_names") >&2 || true
    exit 1
fi

resolver_builtin_names="$(
    sed -n '/k_builtin_entries\[\]/,/^};/p' \
        "$ROOT_DIR/src/semantic/type_checker_builtins_resolve.c" \
        | grep -o '{"[^"]*"' \
        | sed 's/^{"//'
)"
resolver_builtin_names_sorted="$(
    printf '%s\n' "$resolver_builtin_names" | sort
)"
if [[ "$resolver_builtin_names" != "$resolver_builtin_names_sorted" ]]; then
    echo "semantic builtin resolver names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$resolver_builtin_names_sorted") \
        <(printf '%s\n' "$resolver_builtin_names") >&2 || true
    exit 1
fi

for semantic_dispatch_file in \
    "$ROOT_DIR/src/semantic/type_checker_builtins_channel_state.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_state_tools.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_channel_transport.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_collections.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_body.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_map.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_scalar.c" \
    "$ROOT_DIR/src/semantic/type_checker_builtins_stdlib_variant.c" \
    "$ROOT_DIR/src/semantic/slot_analyzer_builtin.c" \
    "$ROOT_DIR/src/semantic/type_checker_resolution_metadata_constructed.c" \
    "$ROOT_DIR/src/semantic/type_checker_resolution_metadata_diagnostics.c"; do
    semantic_dispatch_names="$(
        sed -n '/static const .*\[\]/,/^};/p' "$semantic_dispatch_file" \
            | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
            | grep -o '"[A-Za-z0-9_]*"' \
            | tr -d '"' \
            || true
    )"
    semantic_dispatch_names_sorted="$(
        printf '%s\n' "$semantic_dispatch_names" | sort
    )"
    if [[ "$semantic_dispatch_names" != "$semantic_dispatch_names_sorted" ]]; then
        echo "semantic dispatch names must stay sorted for bsearch: $semantic_dispatch_file" >&2
        diff -u <(printf '%s\n' "$semantic_dispatch_names_sorted") \
            <(printf '%s\n' "$semantic_dispatch_names") >&2 || true
        exit 1
    fi
done

transpiler_infer_call_names="$(
    sed -n '/kTranspilerInferCallSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/transpiler_expr_type_infer_call_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_infer_call_names_sorted="$(
    printf '%s\n' "$transpiler_infer_call_names" | sort
)"
if [[ "$transpiler_infer_call_names" != "$transpiler_infer_call_names_sorted" ]]; then
    echo "C expression type inference builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_infer_call_names_sorted") \
        <(printf '%s\n' "$transpiler_infer_call_names") >&2 || true
    exit 1
fi

transpiler_box_let_names="$(
    sed -n '/static const TranspilerBoxLetSpec kTranspilerBoxLetSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/transpiler_let_box_emit.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_box_let_names_sorted="$(
    printf '%s\n' "$transpiler_box_let_names" | sort
)"
if [[ "$transpiler_box_let_names" != "$transpiler_box_let_names_sorted" ]]; then
    echo "C backend Box/Rc let builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_box_let_names_sorted") \
        <(printf '%s\n' "$transpiler_box_let_names") >&2 || true
    exit 1
fi

channel_builtin_owner="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_channel_builtin.c"

transpiler_result_option_names="$(
    sed -n '/static const TranspilerResultOptionSpec kTranspilerResultOptionSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/transpiler_call_result_option_builtin_emit.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_result_option_names_sorted="$(
    printf '%s\n' "$transpiler_result_option_names" | sort
)"
if [[ "$transpiler_result_option_names" != "$transpiler_result_option_names_sorted" ]]; then
    echo "C backend Result/Option builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_result_option_names_sorted") \
        <(printf '%s\n' "$transpiler_result_option_names") >&2 || true
    exit 1
fi

result_option_builtin_names="$(
    sed -n '/static const LLVMResultOptionSpec kLLVMResultOptionSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_result_option_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
result_option_builtin_names_sorted="$(
    printf '%s\n' "$result_option_builtin_names" | sort
)"
if [[ "$result_option_builtin_names" != "$result_option_builtin_names_sorted" ]]; then
    echo "LLVM Result/Option builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$result_option_builtin_names_sorted") \
        <(printf '%s\n' "$result_option_builtin_names") >&2 || true
    exit 1
fi

llvm_stmt_collection_ctor_names="$(
    sed -n '/kLLVMStmtCollectionCtorSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_stmt_let_collection_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_stmt_collection_ctor_names_sorted="$(
    printf '%s\n' "$llvm_stmt_collection_ctor_names" | sort
)"
if [[ "$llvm_stmt_collection_ctor_names" != "$llvm_stmt_collection_ctor_names_sorted" ]]; then
    echo "LLVM let collection ctor names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_stmt_collection_ctor_names_sorted") \
        <(printf '%s\n' "$llvm_stmt_collection_ctor_names") >&2 || true
    exit 1
fi
llvm_stmt_let_call_names="$(
    sed -n '/static const LLVMStmtLetCallSpec kLLVMStmtLetCallSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_stmt_let_collection_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_stmt_let_call_names_sorted="$(
    printf '%s\n' "$llvm_stmt_let_call_names" | sort
)"
if [[ "$llvm_stmt_let_call_names" != "$llvm_stmt_let_call_names_sorted" ]]; then
    echo "LLVM let special call names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_stmt_let_call_names_sorted") \
        <(printf '%s\n' "$llvm_stmt_let_call_names") >&2 || true
    exit 1
fi

llvm_collection_base_names="$(
    sed -n '/static const LLVMCollectionBaseSpec kLLVMCollectionBaseSpecs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_collection_base_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_collection_base_names_sorted="$(
    printf '%s\n' "$llvm_collection_base_names" | sort
)"
if [[ "$llvm_collection_base_names" != "$llvm_collection_base_names_sorted" ]]; then
    echo "LLVM base collection builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_collection_base_names_sorted") \
        <(printf '%s\n' "$llvm_collection_base_names") >&2 || true
    exit 1
fi

collection_builtin_owner="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_collection_builtin.c"

transpiler_collection_names="$(
    sed -n '/static const TranspilerCollectionSpec kTranspilerCollectionSpecs\[\]/,/^};/p' \
        "$collection_builtin_owner" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_collection_names_sorted="$(
    printf '%s\n' "$transpiler_collection_names" | sort
)"
if [[ "$transpiler_collection_names" != "$transpiler_collection_names_sorted" ]]; then
    echo "C backend collection builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_collection_names_sorted") \
        <(printf '%s\n' "$transpiler_collection_names") >&2 || true
    exit 1
fi

transpiler_array_names="$(
    sed -n '/static const TranspilerArrayStdlibSpec kTranspilerArraySpecs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_array_names_sorted="$(
    printf '%s\n' "$transpiler_array_names" | sort
)"
if [[ "$transpiler_array_names" != "$transpiler_array_names_sorted" ]]; then
    echo "C backend Array builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_array_names_sorted") \
        <(printf '%s\n' "$transpiler_array_names") >&2 || true
    exit 1
fi

transpiler_to_string_names="$(
    sed -n '/static const TranspilerToStringSpec kTranspilerToStringSpecs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_to_string_names_sorted="$(
    printf '%s\n' "$transpiler_to_string_names" | sort
)"
if [[ "$transpiler_to_string_names" != "$transpiler_to_string_names_sorted" ]]; then
    echo "C backend ToString type names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_to_string_names_sorted") \
        <(printf '%s\n' "$transpiler_to_string_names") >&2 || true
    exit 1
fi
transpiler_stdlib_names="$(
    sed -n '/static const TranspilerStdlibSpec kTranspilerStdlibSpecs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/transpiler_expr_stdlib_builtin_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_stdlib_names_sorted="$(
    printf '%s\n' "$transpiler_stdlib_names" | sort
)"
if [[ "$transpiler_stdlib_names" != "$transpiler_stdlib_names_sorted" ]]; then
    echo "C backend stdlib builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_stdlib_names_sorted") \
        <(printf '%s\n' "$transpiler_stdlib_names") >&2 || true
    exit 1
fi

misc_builtin_owner="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_misc_builtin.c"
transpiler_misc_names="$(
    sed -n '/static const TranspilerMiscSpec kTranspilerMiscSpecs\[\]/,/^};/p' \
        "$misc_builtin_owner" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_misc_names_sorted="$(
    printf '%s\n' "$transpiler_misc_names" | sort
)"
if [[ "$transpiler_misc_names" != "$transpiler_misc_names_sorted" ]]; then
    echo "C backend misc builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_misc_names_sorted") \
        <(printf '%s\n' "$transpiler_misc_names") >&2 || true
    exit 1
fi

llvm_stmt_collection_get_names="$(
    sed -n '/static const LLVMCollectionGetSpec kLLVMCollectionGetSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_stmt_type_infer_helpers.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_stmt_collection_get_names_sorted="$(
    printf '%s\n' "$llvm_stmt_collection_get_names" | sort
)"
if [[ "$llvm_stmt_collection_get_names" != "$llvm_stmt_collection_get_names_sorted" ]]; then
    echo "LLVM stmt collection-get names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_stmt_collection_get_names_sorted") \
        <(printf '%s\n' "$llvm_stmt_collection_get_names") >&2 || true
    exit 1
fi

transpiler_let_collection_ctor_names="$(
    sed -n '/kTranspilerLetCollectionCtorSpecs\[\]/,/^        };/p' \
        "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_let_collection_ctor_names_sorted="$(
    printf '%s\n' "$transpiler_let_collection_ctor_names" | sort
)"
if [[ "$transpiler_let_collection_ctor_names" != "$transpiler_let_collection_ctor_names_sorted" ]]; then
    echo "C backend let collection ctor names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_let_collection_ctor_names_sorted") \
        <(printf '%s\n' "$transpiler_let_collection_ctor_names") >&2 || true
    exit 1
fi

domain_query_names="$(
    sed -n '/static const LLVMDomainQuerySpec kLLVMDomainQuerySpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_domain_query_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
domain_query_names_sorted="$(
    printf '%s\n' "$domain_query_names" | sort
)"
if [[ "$domain_query_names" != "$domain_query_names_sorted" ]]; then
    echo "LLVM domain query names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$domain_query_names_sorted") \
        <(printf '%s\n' "$domain_query_names") >&2 || true
    exit 1
fi

array_builtin_names="$(
    sed -n '/static const LLVMArrayBuiltinSpec kArrayBuiltinSpecs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_array_calls.c" |
        sed -n 's/.*{"\([^"]*\)".*/\1/p'
)"
array_builtin_names_sorted="$(printf '%s\n' "$array_builtin_names" | sort)"
if [[ "$array_builtin_names" != "$array_builtin_names_sorted" ]]; then
    echo "LLVM Array builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$array_builtin_names_sorted") \
        <(printf '%s\n' "$array_builtin_names") >&2 || true
    exit 1
fi

math_builtin_names="$(
    sed -n '/static const LLVMMathSpec kLLVMMathSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm/expression/math_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
math_builtin_names_sorted="$(
    printf '%s\n' "$math_builtin_names" | sort
)"
if [[ "$math_builtin_names" != "$math_builtin_names_sorted" ]]; then
    echo "LLVM math builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$math_builtin_names_sorted") \
        <(printf '%s\n' "$math_builtin_names") >&2 || true
    exit 1
fi

log_builtin_names="$(
    sed -n '/static const LLVMLogSpec kLLVMLogSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_log_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
log_builtin_names_sorted="$(
    printf '%s\n' "$log_builtin_names" | sort
)"
if [[ "$log_builtin_names" != "$log_builtin_names_sorted" ]]; then
    echo "LLVM log builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$log_builtin_names_sorted") \
        <(printf '%s\n' "$log_builtin_names") >&2 || true
    exit 1
fi

call_inline_names="$(
    sed -n '/static const LLVMCallInlineSpec kLLVMCallInlineSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_call_inline_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
call_inline_names_sorted="$(
    printf '%s\n' "$call_inline_names" | sort
)"
if [[ "$call_inline_names" != "$call_inline_names_sorted" ]]; then
    echo "LLVM central inline call names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$call_inline_names_sorted") \
        <(printf '%s\n' "$call_inline_names") >&2 || true
    exit 1
fi

rc_builtin_names="$(
    sed -n '/static const LLVMRcSpec kLLVMRcSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_rc_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
rc_builtin_names_sorted="$(
    printf '%s\n' "$rc_builtin_names" | sort
)"
if [[ "$rc_builtin_names" != "$rc_builtin_names_sorted" ]]; then
    echo "LLVM Rc/Weak builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$rc_builtin_names_sorted") \
        <(printf '%s\n' "$rc_builtin_names") >&2 || true
    exit 1
fi

task_runtime_names="$(
    sed -n '/static const LLVMTaskRuntimeSpec kTaskRuntimeSpecs\[\]/,/^};/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_task_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
task_runtime_names_sorted="$(
    printf '%s\n' "$task_runtime_names" | sort
)"
if [[ "$task_runtime_names" != "$task_runtime_names_sorted" ]]; then
    echo "LLVM task runtime names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$task_runtime_names_sorted") \
        <(printf '%s\n' "$task_runtime_names") >&2 || true
    exit 1
fi

task_channel_names="$(
    sed -n '/static const LLVMTaskChannelNameSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_task_channel_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
task_channel_names_sorted="$(
    printf '%s\n' "$task_channel_names" | sort
)"
if [[ "$task_channel_names" != "$task_channel_names_sorted" ]]; then
    echo "LLVM task/channel builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$task_channel_names_sorted") \
        <(printf '%s\n' "$task_channel_names") >&2 || true
    exit 1
fi
task_channel_op_names="$(
    sed -n '/static const LLVMTaskChannelOpSpec kTaskChannelOpSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_task_channel_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
task_channel_op_names_sorted="$(
    printf '%s\n' "$task_channel_op_names" | sort
)"
if [[ "$task_channel_op_names" != "$task_channel_op_names_sorted" ]]; then
    echo "LLVM task/channel operation names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$task_channel_op_names_sorted") \
        <(printf '%s\n' "$task_channel_op_names") >&2 || true
    exit 1
fi
channel_query_names="$(
    sed -n '/static const LLVMChannelQueryOpSpec specs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_task_channel_policy.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
channel_query_names_sorted="$(
    printf '%s\n' "$channel_query_names" | sort
)"
if [[ "$channel_query_names" != "$channel_query_names_sorted" ]]; then
    echo "LLVM channel query op names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$channel_query_names_sorted") \
        <(printf '%s\n' "$channel_query_names") >&2 || true
    exit 1
fi
llvm_stdlib_string_file_names="$(
    sed -n '/static const LLVMStdlibRuntimeCallSpec kLLVMStdlibStringFileRuntimeSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_stdlib_string_file_names_sorted="$(
    printf '%s\n' "$llvm_stdlib_string_file_names" | sort
)"
if [[ "$llvm_stdlib_string_file_names" != "$llvm_stdlib_string_file_names_sorted" ]]; then
    echo "LLVM stdlib string/file runtime-call names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_stdlib_string_file_names_sorted") \
        <(printf '%s\n' "$llvm_stdlib_string_file_names") >&2 || true
    exit 1
fi

llvm_stdlib_runtime_io_names="$(
    sed -n '/static const LLVMStdlibRuntimeCallSpec kLLVMStdlibRuntimeIoSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_stdlib_runtime_io_names_sorted="$(
    printf '%s\n' "$llvm_stdlib_runtime_io_names" | sort
)"
if [[ "$llvm_stdlib_runtime_io_names" != "$llvm_stdlib_runtime_io_names_sorted" ]]; then
    echo "LLVM stdlib runtime/io names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_stdlib_runtime_io_names_sorted") \
        <(printf '%s\n' "$llvm_stdlib_runtime_io_names") >&2 || true
    exit 1
fi
llvm_stdlib_string_special_names="$(
    sed -n '/static const LLVMStdlibStringSpecialSpec kLLVMStdlibStringSpecialSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_stdlib_string_special_names_sorted="$(
    printf '%s\n' "$llvm_stdlib_string_special_names" | sort
)"
if [[ "$llvm_stdlib_string_special_names" != "$llvm_stdlib_string_special_names_sorted" ]]; then
    echo "LLVM stdlib string special names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_stdlib_string_special_names_sorted") \
        <(printf '%s\n' "$llvm_stdlib_string_special_names") >&2 || true
    exit 1
fi
llvm_stdlib_io_special_names="$(
    sed -n '/static const LLVMStdlibIoSpecialSpec kLLVMStdlibIoSpecialSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_stdlib_scalar_io_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
llvm_stdlib_io_special_names_sorted="$(
    printf '%s\n' "$llvm_stdlib_io_special_names" | sort
)"
if [[ "$llvm_stdlib_io_special_names" != "$llvm_stdlib_io_special_names_sorted" ]]; then
    echo "LLVM stdlib io special names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$llvm_stdlib_io_special_names_sorted") \
        <(printf '%s\n' "$llvm_stdlib_io_special_names") >&2 || true
    exit 1
fi

slot_builtin_names="$(
    sed -n '/static const LLVMSlotBuiltinSpec kLLVMSlotBuiltinSpecs\[\]/,/^    };/p' \
        "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
slot_builtin_names_sorted="$(
    printf '%s\n' "$slot_builtin_names" | sort
)"
if [[ "$slot_builtin_names" != "$slot_builtin_names_sorted" ]]; then
    echo "LLVM Slot/DeviceSlot builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$slot_builtin_names_sorted") \
        <(printf '%s\n' "$slot_builtin_names") >&2 || true
    exit 1
fi

scalar_builtin_owner="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_scalar_builtin.c"

scalar_unary_owner="$ROOT_DIR/src/codegen/transpiler_expr_stdlib_scalar_unary.c"

transpiler_scalar_unary_names="$(
    sed -n '/static const TranspilerScalarUnarySpec specs\[\]/,/^    };/p' \
        "$scalar_unary_owner" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_scalar_unary_names_sorted="$(
    printf '%s\n' "$transpiler_scalar_unary_names" | sort
)"
if [[ "$transpiler_scalar_unary_names" != "$transpiler_scalar_unary_names_sorted" ]]; then
    echo "C backend scalar unary names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_scalar_unary_names_sorted") \
        <(printf '%s\n' "$transpiler_scalar_unary_names") >&2 || true
    exit 1
fi

transpiler_scalar_names="$(
    sed -n '/static const TranspilerScalarSpec kTranspilerScalarSpecs\[\]/,/^};/p' \
        "$scalar_builtin_owner" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_scalar_names_sorted="$(
    printf '%s\n' "$transpiler_scalar_names" | sort
)"
if [[ "$transpiler_scalar_names" != "$transpiler_scalar_names_sorted" ]]; then
    echo "C backend scalar builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_scalar_names_sorted") \
        <(printf '%s\n' "$transpiler_scalar_names") >&2 || true
    exit 1
fi
transpiler_channel_query_names="$(
    sed -n '/static const TranspilerChannelQuerySpec specs\[\]/,/^    };/p' \
        "$channel_builtin_owner" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_channel_query_names_sorted="$(
    printf '%s\n' "$transpiler_channel_query_names" | sort
)"
if [[ "$transpiler_channel_query_names" != "$transpiler_channel_query_names_sorted" ]]; then
    echo "C backend channel query names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_channel_query_names_sorted") \
        <(printf '%s\n' "$transpiler_channel_query_names") >&2 || true
    exit 1
fi

transpiler_channel_names="$(
    sed -n '/static const TranspilerChannelSpec specs\[\]/,/^    };/p' \
        "$channel_builtin_owner" \
        | grep -Eo '\{[[:space:]]*"[A-Za-z0-9_]*"' \
        | grep -o '"[A-Za-z0-9_]*"' \
        | tr -d '"'
)"
transpiler_channel_names_sorted="$(
    printf '%s\n' "$transpiler_channel_names" | sort
)"
if [[ "$transpiler_channel_names" != "$transpiler_channel_names_sorted" ]]; then
    echo "C backend channel builtin names must stay sorted for bsearch" >&2
    diff -u <(printf '%s\n' "$transpiler_channel_names_sorted") \
        <(printf '%s\n' "$transpiler_channel_names") >&2 || true
    exit 1
fi

echo "[perf-contract] perf summary contract is smoke-gated"
