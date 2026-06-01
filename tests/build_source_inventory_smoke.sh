#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAKE_BIN="${MAKE:-}"

if [[ -z "$MAKE_BIN" ]]; then
    if command -v make >/dev/null 2>&1; then
        MAKE_BIN="$(command -v make)"
    elif command -v mingw32-make >/dev/null 2>&1; then
        MAKE_BIN="$(command -v mingw32-make)"
    else
        echo "[build-source-inventory] missing make" >&2
        exit 1
    fi
fi

inventory="$(
    MAKEFLAGS= "$MAKE_BIN" --no-print-directory -C "$ROOT_DIR" -s __pgy_build_source_inventory_print \
        | grep -Ev '^make\[[0-9]+\]: (Entering|Leaving) directory '
)"
missing=0

if ! grep -Fq 'pgy_mkdir_p = $(BASH) -c "mkdir -p $(1)"' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] pgy_mkdir_p must avoid nested login-shell bash" >&2
    missing=1
fi
if grep -Fq 'pgy_mkdir_p = $(BASH) -lc' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] pgy_mkdir_p regressed to nested login-shell bash" >&2
    missing=1
fi
if ! grep -Fq 'pgy_touch_ref = $(BASH) -c "touch -c -r $(1) $(2)"' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] pgy_touch_ref must avoid nested login-shell bash" >&2
    missing=1
fi
if grep -Fq 'pgy_touch_ref = $(BASH) -lc' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] pgy_touch_ref regressed to nested login-shell bash" >&2
    missing=1
fi
if ! grep -Fq 'ENABLE_ASM_FASTPATH ?= 0' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] assembly fast path must be explicit opt-in" >&2
    missing=1
fi
if ! grep -Fq 'ifneq ($(ENABLE_ASM_FASTPATH),1)' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] runtime asm objects must stay disabled by default" >&2
    missing=1
fi

duplicate_sources="$(
    printf '%s\n' "$inventory" \
        | sed '/^$/d' \
        | sort \
        | uniq -d
)"
if [[ -n "$duplicate_sources" ]]; then
    printf '%s\n' "$duplicate_sources" >&2
    echo "[build-source-inventory] duplicate source inventory entries" >&2
    missing=1
fi

ignored_inventory=""
if command -v git >/dev/null 2>&1 \
    && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    ignored_inventory="$(
        printf '%s\n' "$inventory" \
            | git -C "$ROOT_DIR" check-ignore --stdin --no-index 2>/dev/null \
            || true
    )"
fi

while IFS= read -r src; do
    src="${src%$'\r'}"
    [[ -n "$src" ]] || continue

    if [[ ! -f "$ROOT_DIR/$src" ]]; then
        echo "[build-source-inventory] missing required build file: $src" >&2
        missing=1
        continue
    fi

    if [[ -n "$ignored_inventory" ]] \
        && grep -Fxq "$src" <<< "$ignored_inventory"; then
        echo "[build-source-inventory] required build file is ignored: $src" >&2
        missing=1
    fi

    if [[ "${PGY_REQUIRE_TRACKED_SOURCES:-0}" == "1" ]] \
        && command -v git >/dev/null 2>&1 \
        && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1 \
        && ! git -C "$ROOT_DIR" ls-files --error-unmatch "$src" >/dev/null 2>&1; then
        echo "[build-source-inventory] required build file is not tracked: $src" >&2
        missing=1
    fi
done <<< "$inventory"

if command -v git >/dev/null 2>&1 \
    && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    deleted_examples="$(
        git -C "$ROOT_DIR" ls-files --deleted -- 'tests/cases' 'examples'
    )"
    while IFS= read -r tracked; do
        [[ -n "$tracked" ]] || continue
        if [[ -n "$deleted_examples" ]] \
            && grep -Fxq "$tracked" <<< "$deleted_examples"; then
            continue
        fi
        case "$tracked" in
            *.exe|*.dll|*.so|*.dylib|*.o|*.obj|*.a|*.lib|*.wasm)
                echo "[build-source-inventory] tracked executable artifact is not allowed: $tracked" >&2
                missing=1
                ;;
        esac
    done < <(git -C "$ROOT_DIR" ls-files 'tests/cases' 'examples')
fi

typo_tokens="$(
    grep -RInE '\b(retun|stncmp|retun_type|infer_spawn_retun)\b' \
        "$ROOT_DIR/src" \
        --include='*.c' --include='*.h' || true
)"
typo_tokens="$(
    printf '%s\n' "$typo_tokens" \
        | grep -v '/src/test_' \
        | grep -v '/src/tests/' || true
)"
if [[ -n "$typo_tokens" ]]; then
    printf '%s\n' "$typo_tokens" >&2
    echo "[build-source-inventory] production source contains typo-like C tokens" >&2
    missing=1
fi

local_windows_path_helpers="$(
    cd "$ROOT_DIR"
    grep -RInE 'cygpath[[:space:]]+-(m|w)|wslpath[[:space:]]+-(m|w)' tests \
        --include='*.sh' || true
)"
local_windows_path_helpers="$(
    printf '%s\n' "$local_windows_path_helpers" \
        | grep -v 'tests/pgy_binary_path_helpers.sh' || true
)"
if [[ -n "$local_windows_path_helpers" ]]; then
    printf '%s\n' "$local_windows_path_helpers" >&2
    echo "[build-source-inventory] Windows path conversion must use tests/pgy_binary_path_helpers.sh" >&2
    missing=1
fi

portable_grep_violations="$(
    cd "$ROOT_DIR"
    grep -RInE 'grep[[:space:]][^;&|]*-[A-Za-z]*P' tests \
        --include='*.sh' \
        | grep -v '^tests/build_source_inventory_smoke.sh:' || true
)"
if [[ -n "$portable_grep_violations" ]]; then
    printf '%s\n' "$portable_grep_violations" >&2
    echo "[build-source-inventory] smoke scripts must not depend on grep -P; use portable grep -E filters instead" >&2
    missing=1
fi

legacy_c_declarator_consumers="$(
    cd "$ROOT_DIR"
    grep -RInE 'pergyra_(ast_typed_declarator|func_pointer_declarator_from_decl)\(' \
        src/codegen --include='*.c' --include='*.h' \
        | grep -v '^src/codegen/transpiler_type_declarator\.[ch]:' || true
)"
if [[ -n "$legacy_c_declarator_consumers" ]]; then
    printf '%s\n' "$legacy_c_declarator_consumers" >&2
    echo "[build-source-inventory] C backend declarator consumers must use ctx-aware _in_ctx APIs" >&2
    missing=1
fi

legacy_c_type_copy_consumers="$(
    cd "$ROOT_DIR"
    grep -RInE 'pergyra_ast_type_to_c_copy\(' \
        src/codegen --include='*.c' --include='*.h' \
        | grep -v '^src/codegen/transpiler_type_render\.[ch]:' || true
)"
if [[ -n "$legacy_c_type_copy_consumers" ]]; then
    printf '%s\n' "$legacy_c_type_copy_consumers" >&2
    echo "[build-source-inventory] C backend AST type consumers must use ctx-aware pergyra_ast_type_to_c_copy_in_ctx" >&2
    missing=1
fi

legacy_type_name_render_consumers="$(
    cd "$ROOT_DIR"
    grep -RInE '(^|[^_[:alnum:]])render_type_name\(' \
        src/codegen --include='*.c' --include='*.h' \
        | grep -v '^src/codegen/transpiler_type_render\.[ch]:' || true
)"
if [[ -n "$legacy_type_name_render_consumers" ]]; then
    printf '%s\n' "$legacy_type_name_render_consumers" >&2
    echo "[build-source-inventory] C backend type-name render consumers must use render_type_name_in_ctx outside wrapper owners" >&2
    missing=1
fi

c_type_requirement_owner_violations="$(
    cd "$ROOT_DIR"
    grep -n 'pergyra_type_to_c_copy(' \
        src/codegen/transpiler_block_intent_rebind_helpers.c \
        src/codegen/transpiler_domain_role_ability_emit.c \
        src/codegen/transpiler_control_flow_emit.c \
        src/codegen/transpiler_destructure_emit.c \
        src/codegen/transpiler_expr_composite_literal_emit.c \
        src/codegen/transpiler_expr_call_member_emit.c \
        src/codegen/transpiler_expr_dispatch_emit.c \
        src/codegen/transpiler_expr_stdlib_builtin.c \
        src/codegen/transpiler_expr_stdlib_channel_builtin.c \
        src/codegen/transpiler_expr_stdlib_collection_support.c \
        src/codegen/transpiler_lambda_emit.c \
        src/codegen/transpiler_let_collection_emit.c \
        src/codegen/transpiler_let_emit.c \
        src/codegen/transpiler_let_slot_emit.c \
        src/codegen/transpiler_match_bindings.c \
        src/codegen/transpiler_match_emit.c \
        src/codegen/transpiler_mir_func_emit.c \
        src/codegen/transpiler_mir_cfg_control_emit.c \
        src/codegen/transpiler_mir_func_ssa_locals_emit.c \
        src/codegen/transpiler_mir_destructure_emit.c \
        src/codegen/transpiler_mir_match_condition_emit.c \
        src/codegen/transpiler_mir_preserved_let_emit.c \
        src/codegen/transpiler_mir_resource_op_core.c \
        src/codegen/transpiler_select.c \
        src/codegen/transpiler_spawn_channel_emit.c || true
)"
if [[ -n "$c_type_requirement_owner_violations" ]]; then
    printf '%s\n' "$c_type_requirement_owner_violations" >&2
    echo "[build-source-inventory] closed C backend owners must use transpiler_require_type_name_c_type_copy for C-type lowering" >&2
    missing=1
fi

raw_c_type_copy_consumers="$(
    cd "$ROOT_DIR"
    grep -RIn 'pergyra_type_to_c_copy(' \
        src/codegen --include='*.c' --include='*.h' \
        | grep -v '^src/codegen/transpiler\.h:' \
        | grep -v '^src/codegen/transpiler_type_mapping\.[ch]:' \
        | grep -v '^src/codegen/transpiler_type_render\.c:' \
        | grep -v '^src/codegen/transpiler_type_require\.c:' || true
)"
if [[ -n "$raw_c_type_copy_consumers" ]]; then
    printf '%s\n' "$raw_c_type_copy_consumers" >&2
    echo "[build-source-inventory] C backend raw pergyra_type_to_c_copy use must stay in wrapper/type-require owners" >&2
    missing=1
fi

direct_option_result_vocabulary="$(
    cd "$ROOT_DIR"
    {
        grep -RIn 'strcmp(' src/parser src/semantic src/codegen \
            --include='*.c' --include='*.h' \
            | grep -E '"(Some|None|Ok|Err)"' || true
        grep -RInE '\{[[:space:]]*"(Some|None|Ok|Err)"' \
            src/parser src/semantic src/codegen \
            --include='*.c' --include='*.h' || true
    } | grep -v '^src/common/match_variant_policy\.[ch]:' \
        | grep -v '^src/codegen/codegen_match_variant_policy\.[ch]:' || true
)"
if [[ -n "$direct_option_result_vocabulary" ]]; then
    printf '%s\n' "$direct_option_result_vocabulary" >&2
    echo "[build-source-inventory] Result/Option constructor vocabulary must use match_variant_policy" >&2
    missing=1
fi
direct_option_result_tag_spelling="$(
    cd "$ROOT_DIR"
    grep -RInE 'Pgy(Option(Some|None)|Result(Ok|Err))' src/codegen \
        --include='*.c' --include='*.h' \
        | grep -v '^src/codegen/codegen_match_variant_policy\.[ch]:' \
        | grep -v '^src/codegen/transpiler_specialization_registry\.c:' || true
)"
if [[ -n "$direct_option_result_tag_spelling" ]]; then
    printf '%s\n' "$direct_option_result_tag_spelling" >&2
    echo "[build-source-inventory] generated Option/Result tag spelling must use codegen_match_variant_policy" >&2
    missing=1
fi

for smoke in \
    tests/abi_pipeline_smoke.sh \
    tests/air_backend_nonimpact_smoke.sh \
    tests/air_json_schema_smoke.sh \
    tests/cfg_body_dataflow_smoke.sh \
    tests/codegen_determinism_smoke.sh \
    tests/diagnostics_json_smoke.sh \
    tests/dogfood_webgl_smoke.sh \
    tests/example_contract_smoke.sh \
    tests/fmt_smoke.sh \
    tests/ir_pipeline_probe.sh \
    tests/llvm_campaign_projection_smoke.sh \
    tests/llvm_dnd_campaign_smoke.sh \
    tests/llvm_smoke.sh \
    tests/memory_concurrency_model_smoke.sh \
    tests/module_smoke.sh \
    tests/observability_schema_smoke.sh \
    tests/package_module_resolver_smoke.sh \
    tests/perf_c_baseline_smoke.sh \
    tests/raw_escape_contract_smoke.sh \
    tests/runtime_frontier_contract_smoke.sh \
    tests/runtime_none_contract_smoke.sh \
    tests/runtime_panic_codegen_smoke.sh \
    tests/stdlib_surface_smoke.sh \
    tests/tooling_conformance_smoke.sh \
    tests/unicode_policy_smoke.sh
do
    if ! grep -Fq 'pgy_binary_path_helpers.sh' "$ROOT_DIR/$smoke"; then
        echo "[build-source-inventory] beta executable smoke must use shared Windows path helper: $smoke" >&2
        missing=1
    fi
    if ! grep -Fq 'pgy_path_for_compiler' "$ROOT_DIR/$smoke"; then
        if grep -Fq 'tests/compare_backends.sh' "$ROOT_DIR/$smoke"; then
            :
        else
            echo "[build-source-inventory] beta executable smoke must convert compiler paths: $smoke" >&2
            missing=1
        fi
    fi
done

if ! grep -Fq 'run_windows_native_binary_fallback' "$ROOT_DIR/tests/compare_backends.sh"; then
    echo "[build-source-inventory] backend compare must keep Windows native 126/127 fallback" >&2
    missing=1
fi
if ! grep -Fq 'pgy_powershell_quote' "$ROOT_DIR/tests/compare_backends.sh"; then
    echo "[build-source-inventory] backend compare PowerShell fallback must quote paths explicitly" >&2
    missing=1
fi

bash4_lint_dirs=(tests)
if [[ -d "$ROOT_DIR/src/self_hosted/parity" ]]; then
    bash4_lint_dirs+=(src/self_hosted/parity)
fi

bash4_only_smoke_terms="$(
    cd "$ROOT_DIR"
    grep -RInE '(^|[^A-Za-z0-9_])(mapfile|readarray)([^A-Za-z0-9_]|$)|(^|[[:space:]])(declare|typeset)[[:space:]]+-A([^A-Za-z0-9_]|$)|\$\{[^}]+(,,|\^\^)[^}]*\}' "${bash4_lint_dirs[@]}" --include='*.sh' \
        | grep -v '^tests/build_source_inventory_smoke.sh:' || true
)"
if [[ -n "$bash4_only_smoke_terms" ]]; then
    printf '%s\n' "$bash4_only_smoke_terms" >&2
    echo "[build-source-inventory] smoke / self_hosted parity scripts must remain compatible with macOS Bash 3.2; avoid Bash 4-only mapfile/readarray, associative arrays, and case-conversion expansions" >&2
    missing=1
fi

multiline_case_continuations="$(
    cd "$ROOT_DIR"
    grep -RInE '\|\\$' "${bash4_lint_dirs[@]}" --include='*.sh' \
        | grep -v '^tests/build_source_inventory_smoke.sh:' || true
)"
if [[ -n "$multiline_case_continuations" ]]; then
    printf '%s\n' "$multiline_case_continuations" >&2
    echo "[build-source-inventory] smoke / self_hosted parity scripts must not use case-pattern line continuations; macOS Bash 3.2 parses these unreliably inside command substitution" >&2
    missing=1
fi

semantic_self_include_leaks="$(
    cd "$ROOT_DIR"
    grep -RIn '#include "\.\./semantic/' src/semantic \
        --include='*.c' --include='*.h' || true
)"
if [[ -n "$semantic_self_include_leaks" ]]; then
    printf '%s\n' "$semantic_self_include_leaks" >&2
    echo "[build-source-inventory] semantic files must include same-directory headers directly" >&2
    missing=1
fi

rir_program_root_bridge="$(
    cd "$ROOT_DIR"
    grep -RInE 'g_rir_program_root|rir_set_program_root|rir_program_root\(\)' src/compiler \
        --include='*.c' --include='*.h' || true
)"
if [[ -n "$rir_program_root_bridge" ]]; then
    printf '%s\n' "$rir_program_root_bridge" >&2
    echo "[build-source-inventory] RIR program root must be owned by RIRProgram/RIRScope, not a global bridge" >&2
    missing=1
fi

if ! grep -Fq 'ASTNode  *program_root;' "$ROOT_DIR/src/compiler/rir.h"; then
    echo "[build-source-inventory] RIRProgram must carry program_root explicitly" >&2
    missing=1
fi

if ! grep -Fq 'ASTNode         *program_root;' "$ROOT_DIR/src/compiler/rir.h"; then
    echo "[build-source-inventory] RIRScope must carry program_root explicitly for fact helpers" >&2
    missing=1
fi

lsp_ast_payload_leaks="$(
    cd "$ROOT_DIR"
    grep -RInE 'data\.program|data\.(func_decl|class_decl|ability_decl|role_decl|party_decl|roster_decl|world_decl|relation_decl|effect_decl|zone_decl|enum_decl|type_alias)\.name' src/lsp \
        --include='*.c' --include='*.h' || true
)"
if [[ -n "$lsp_ast_payload_leaks" ]]; then
    printf '%s\n' "$lsp_ast_payload_leaks" >&2
    echo "[build-source-inventory] LSP must consume parser AST accessors for declaration inventory" >&2
    missing=1
fi

lsp_unclamped_offsets="$(
    cd "$ROOT_DIR"
    grep -RInE 'off[[:space:]]*\+=[[:space:]]*\(size_t\)n|if[[:space:]]*\(n[[:space:]]*>[[:space:]]*0\)[[:space:]]*off[[:space:]]*\+=' src/lsp \
        --include='*.c' --include='*.h' || true
)"
if [[ -n "$lsp_unclamped_offsets" ]]; then
    printf '%s\n' "$lsp_unclamped_offsets" >&2
    echo "[build-source-inventory] LSP JSON builders must clamp snprintf offsets" >&2
    missing=1
fi

lsp_ignored_offset_status="$(
    cd "$ROOT_DIR"
    grep -RInE '^[[:space:]]*lsp_advance_json_offset[[:space:]]*\([^;]*\);' src/lsp \
        --include='*.c' --include='*.h' || true
)"
if [[ -n "$lsp_ignored_offset_status" ]]; then
    printf '%s\n' "$lsp_ignored_offset_status" >&2
    echo "[build-source-inventory] LSP JSON builders must handle clamp failure" >&2
    missing=1
fi

if grep -RInq 'json_find_string(' "$ROOT_DIR/src/lsp" \
    --include='*.c' --include='*.h'; then
    grep -RIn 'json_find_string(' "$ROOT_DIR/src/lsp" \
        --include='*.c' --include='*.h' >&2 || true
    echo "[build-source-inventory] LSP string extraction must use copy/dup ownership lanes" >&2
    missing=1
fi
for lsp_json_term in \
    "json_find_string_copy" \
    "json_find_string_dup"; do
    if ! grep -RInq "$lsp_json_term" "$ROOT_DIR/src/lsp" \
        --include='*.c' --include='*.h'; then
        echo "[build-source-inventory] LSP missing JSON string ownership lane: $lsp_json_term" >&2
        missing=1
    fi
done
if grep -RInq 'atoi(' "$ROOT_DIR/src/lsp" --include='*.c' --include='*.h'; then
    grep -RIn 'atoi(' "$ROOT_DIR/src/lsp" --include='*.c' --include='*.h' >&2 || true
    echo "[build-source-inventory] LSP external numeric parsing must use checked strtol-style parsing, not atoi" >&2
    missing=1
fi
unchecked_atoi_uses="$(
    cd "$ROOT_DIR"
    grep -RIn 'atoi(' src --include='*.c' --include='*.h' || true
)"
if [[ -n "$unchecked_atoi_uses" ]]; then
    printf '%s\n' "$unchecked_atoi_uses" >&2
    echo "[build-source-inventory] production C numeric parsing must use checked strtol-style parsing, not atoi" >&2
    missing=1
fi
unchecked_numeric_parse_uses="$(
    cd "$ROOT_DIR"
    grep -RInE 'strtol\(|strtoul\(|strtoll\(|strtoull\(' \
        src/compiler src/parser src/lsp src/codegen src/semantic \
        --include='*.c' --include='*.h' \
        | grep -v '^src/common/numeric_parse.c:' || true
)"
if [[ -n "$unchecked_numeric_parse_uses" ]]; then
    printf '%s\n' "$unchecked_numeric_parse_uses" >&2
    echo "[build-source-inventory] checked numeric parsing must route through src/common/numeric_parse" >&2
    missing=1
fi

mutable_static_char_arrays="$(
    cd "$ROOT_DIR"
    grep -RInE 'static[[:space:]]+char[[:space:]]+[A-Za-z0-9_]+[[:space:]]*\[[^]]+\]' \
        src/codegen src/semantic src/compiler src/lsp \
        --include='*.c' --include='*.h' || true
)"
unexpected_static_char_arrays="$(
    printf '%s\n' "$mutable_static_char_arrays" \
        | grep -v '^$' \
        || true
)"
if [[ -n "$unexpected_static_char_arrays" ]]; then
    printf '%s\n' "$unexpected_static_char_arrays" >&2
    echo "[build-source-inventory] mutable static char arrays are forbidden outside explicit process caches" >&2
    missing=1
fi
mutable_static_char_pointers="$(
    cd "$ROOT_DIR"
    grep -RInE 'static[[:space:]]+char[[:space:]]+\*[[:space:]]*[A-Za-z0-9_]+[[:space:]]*(=|;)' \
        src/codegen src/semantic src/compiler src/lsp \
        --include='*.c' --include='*.h' || true
)"
if [[ -n "$mutable_static_char_pointers" ]]; then
    printf '%s\n' "$mutable_static_char_pointers" >&2
    echo "[build-source-inventory] mutable static char pointers are forbidden in compiler helpers" >&2
    missing=1
fi
top_level_mutable_statics="$(
    cd "$ROOT_DIR"
    grep -RInE '^static[[:space:]]+[A-Za-z_][A-Za-z0-9_[:space:]]*\*?[[:space:]]*[A-Za-z_][A-Za-z0-9_]*(\[[^]]+\])?[[:space:]]*(=|;)' \
        src/codegen src/semantic src/compiler src/lsp \
        --include='*.c' --include='*.h' \
        | grep -Ev '^([^:]+:){2}static[[:space:]]+(const|inline)([[:space:]]|$)' || true
)"
unexpected_top_level_mutable_statics="$(
    printf '%s\n' "$top_level_mutable_statics" \
        | grep -v '^$' \
        || true
)"
if [[ -n "$unexpected_top_level_mutable_statics" ]]; then
    printf '%s\n' "$unexpected_top_level_mutable_statics" >&2
    echo "[build-source-inventory] top-level mutable static state requires an explicit source-of-truth owner" >&2
    missing=1
fi
backend_local_match_subject_lookup="$(
    cd "$ROOT_DIR"
    grep -RInE '(llvm|transpiler).*find.*match.*subject.*case|match.*subject.*for.*case' \
        src/codegen \
        --include='*.c' --include='*.h' \
        | grep -v 'ast_find_match_subject_for_case' || true
)"
if [[ -n "$backend_local_match_subject_lookup" ]]; then
    printf '%s\n' "$backend_local_match_subject_lookup" >&2
    echo "[build-source-inventory] match subject lookup must use parser AST accessor source-of-truth" >&2
    missing=1
fi
if ! grep -Fq 'ast_find_match_subject_for_case(ast_func_body(func_decl)' \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c"; then
    echo "[build-source-inventory] LLVM MIR match condition must consume ast_find_match_subject_for_case" >&2
    missing=1
fi
if ! grep -Fq 'ast_find_match_subject_for_case(ast_func_body(func_decl)' \
    "$ROOT_DIR/src/codegen/transpiler_mir_match_condition_emit.c"; then
    echo "[build-source-inventory] C MIR match condition must consume ast_find_match_subject_for_case" >&2
    missing=1
fi
for select_atomic_term in \
    "static _Atomic unsigned int _sel_rr_" \
    "atomic_fetch_add_explicit(&_sel_rr_"; do
    if ! grep -Fq "$select_atomic_term" "$ROOT_DIR/src/codegen/transpiler_select.c"; then
        echo "[build-source-inventory] C select round-robin state must be atomic: $select_atomic_term" >&2
        missing=1
    fi
done
if grep -Fq "_sel_rr_%d++" "$ROOT_DIR/src/codegen/transpiler_select.c"; then
    echo "[build-source-inventory] C select round-robin state regressed to non-atomic increment" >&2
    missing=1
fi
for select_atomic_term in \
    "LLVMBuildAtomicRMW(ctx->builder" \
    "LLVMAtomicOrderingMonotonic"; do
    if ! grep -Fq "$select_atomic_term" "$ROOT_DIR/src/codegen/llvm_stmt_select.c"; then
        echo "[build-source-inventory] LLVM select round-robin state must be atomic: $select_atomic_term" >&2
        missing=1
    fi
done

if grep -Fq 'pgy_detect_c_compiler' "$ROOT_DIR/src/compiler/compiler_toolchain.h" \
    || grep -RInq 'pgy_cc_extra_target_flag' "$ROOT_DIR/src/compiler" \
        --include='*.c' --include='*.h'; then
    echo "[build-source-inventory] compiler toolchain selection must use caller-owned PgyCCompilerSelection" >&2
    missing=1
fi
if awk '
    prev_endif && /selection->cc = "gcc";/ { found=1 }
    /^#endif$/ { prev_endif=1; next }
    /^[[:space:]]*$/ { next }
    { prev_endif=0 }
    END { exit(found ? 0 : 1) }
' "$ROOT_DIR/src/compiler/compiler_toolchain.c"; then
    echo "[build-source-inventory] compiler toolchain detection must not invent an unprobed gcc fallback" >&2
    missing=1
fi

local_compiler_file_readers="$(
    cd "$ROOT_DIR"
    grep -RInE 'fseek\(|ftell\(|fread\(buf' \
        src/compiler/debugger.c src/compiler/fmt_io.c || true
)"
if [[ -n "$local_compiler_file_readers" ]]; then
    printf '%s\n' "$local_compiler_file_readers" >&2
    echo "[build-source-inventory] debugger/fmt_io must use path_read_file instead of local source-file readers" >&2
    missing=1
fi
for term in \
    "PGY_MAX_TEXT_FILE_BYTES" \
    "read_len != (size_t)len" \
    "free(buf)"; do
    if ! grep -Fq "$term" "$ROOT_DIR/src/compiler/fmt.c"; then
        echo "[build-source-inventory] fmt stream reader must preserve size cap and partial-read failure handling: $term" >&2
        missing=1
    fi
done

for header in \
    src/compiler/compiler_process.h \
    src/compiler/mir.h \
    src/parser/ast_types.h \
    src/runtime/slot_manager.h
do
    if grep -Eq '\bsize_t\b' "$ROOT_DIR/$header" \
        && ! grep -Eq '#include[[:space:]]*<stddef\.h>' "$ROOT_DIR/$header"; then
        echo "[build-source-inventory] public header using size_t must include <stddef.h>: $header" >&2
        missing=1
    fi
done

if ! grep -Fq '$(BUILD_DIR)/compiler/hir_callgraph.o' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] HIR callgraph source is not linked by HIR_CORE_OBJECTS" >&2
    missing=1
fi

if ! grep -Fq '$(COMPILER_DIR)/mir_cfg_contract_validate_cleanup.c' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] MIR cleanup validator source is not linked by the compiler source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(BUILD_DIR)/compiler/mir_cfg_contract_validate_cleanup.o' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] MIR cleanup validator object is not linked by MIR_CORE_OBJECTS" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_expr_party_instance_emit.h"' \
    "$ROOT_DIR/src/codegen/transpiler_expr_emitters.h"; then
    echo "[build-source-inventory] party-instance expression emitter is not linked by the C expression emitter include chain" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_let_type_register_emit.h"' \
    "$ROOT_DIR/src/codegen/transpiler_base_a_emitters.h"; then
    echo "[build-source-inventory] let type-registration emitter is not linked by the C let emitter include chain" >&2
    missing=1
fi

if ! grep -Fq '#include "transpiler_collection_runtime_suffix.h"' \
    "$ROOT_DIR/src/codegen/transpiler_helpers_core_b.h"; then
    echo "[build-source-inventory] collection runtime suffix helper is not linked by the C helper include chain" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_roster_decl_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] roster declaration emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_zone_decl_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] zone declaration emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

for owner in \
    '$(CODEGEN_DIR)/transpiler_match_bindings.c' \
    '$(CODEGEN_DIR)/transpiler_mir_inventory_intent_alias_collect.c' \
    '$(CODEGEN_DIR)/transpiler_zone_frontier_emit.c' \
    '$(SEMANTIC_DIR)/slot_analyzer_lookup.c' \
    '$(SEMANTIC_DIR)/slot_analyzer_access.c' \
    '$(SEMANTIC_DIR)/type_checker_flow_loop_snapshot.c'
do
    if ! grep -Fq "$owner" "$ROOT_DIR/Makefile"; then
        echo "[build-source-inventory] split owner is not linked by the source inventory: $owner" >&2
        missing=1
    fi
done

grep -Fq "transpiler_emit_builtin_match_binding" \
    "$ROOT_DIR/src/codegen/transpiler_match_bindings.c" || {
    echo "[build-source-inventory] match binding lowering must stay in transpiler_match_bindings.c" >&2
    missing=1
}

if grep -Eq "^(transpiler_emit_builtin_match_binding|transpiler_match_is_enum_variant_destructor)\\(" \
    "$ROOT_DIR/src/codegen/transpiler_match_emit.c"; then
    echo "[build-source-inventory] match binding lowering regressed into transpiler_match_emit.c" >&2
    missing=1
fi

grep -Fq "transpiler_collect_mir_intent_who_aliases" \
    "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent_alias_collect.c" || {
    echo "[build-source-inventory] MIR intent alias collection must stay in its split owner" >&2
    missing=1
}

if grep -RIn "ASTNode \\*subject_type = ast_intent_involves_subject_type(involves);" \
    "$ROOT_DIR/src/codegen" \
    --include='*.c' --include='*.h'; then
    echo "[build-source-inventory] intent participant accessors must check involves before reading subject_type" >&2
    missing=1
fi

for intent_zone_slot_term in \
    "resolve_intent_zone_slot_name_for_zone_with_metadata" \
    "intent_zone_binding_type_name_with_metadata("; do
    grep -Fq "$intent_zone_slot_term" \
        "$ROOT_DIR/src/codegen/transpiler_intent_zone_slot.c" || {
        echo "[build-source-inventory] MIR-only intent zone-slot lookup must consume participant metadata: $intent_zone_slot_term" >&2
        missing=1
    }
done

for metadata_slot_consumer in \
    src/codegen/transpiler_block_intent_helpers.c \
    src/codegen/transpiler_block_intent_rebind_helpers.c \
    src/codegen/transpiler_intent_zone_binding_emit.c \
    src/codegen/transpiler_intent_emit.c; do
    grep -Fq "resolve_intent_zone_slot_name_for_zone_with_metadata" \
        "$ROOT_DIR/$metadata_slot_consumer" || {
        echo "[build-source-inventory] MIR-only intent zone-slot consumer must use metadata-aware resolver: $metadata_slot_consumer" >&2
        missing=1
    }
done

for llvm_var_class_guard in \
    "ctx == NULL || var_name == NULL || class_name == NULL" \
    "ctx == NULL || var_name == NULL"; do
    grep -Fq "$llvm_var_class_guard" \
        "$ROOT_DIR/src/codegen/llvm_registry.c" || {
        echo "[build-source-inventory] LLVM var-class registry must fail closed on NULL metadata: $llvm_var_class_guard" >&2
        missing=1
    }
done

if grep -Eq "^(transpiler_collect_mir_intent_(who|authorized|dispatch)_aliases|transpiler_collect_mir_intent_participants)\\(" \
    "$ROOT_DIR/src/codegen/transpiler_mir_inventory_intent_collect.c"; then
    echo "[build-source-inventory] MIR intent alias/participant collection regressed into the step/eval collector" >&2
    missing=1
fi

grep -Fq "transpiler_emit_zone_frontier_change_checks" \
    "$ROOT_DIR/src/codegen/transpiler_zone_frontier_emit.c" || {
    echo "[build-source-inventory] zone frontier lowering must stay in transpiler_zone_frontier_emit.c" >&2
    missing=1
}

if grep -Eq "^(transpiler_emit_zone_frontier_change_checks|transpiler_emit_zone_frontier_overflow_guard)\\(" \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.c"; then
    echo "[build-source-inventory] zone frontier lowering regressed into transpiler_zone_decl_emit.c" >&2
    missing=1
fi

grep -Fq "slot_analyzer_find_function_decl" \
    "$ROOT_DIR/src/semantic/slot_analyzer_lookup.c" || {
    echo "[build-source-inventory] slot analyzer function lookup must stay in slot_analyzer_lookup.c" >&2
    missing=1
}

grep -Fq "collect_slot_accesses" \
    "$ROOT_DIR/src/semantic/slot_analyzer_access.c" || {
    echo "[build-source-inventory] slot analyzer access traversal must stay in slot_analyzer_access.c" >&2
    missing=1
}

if grep -Eq "^(slot_analyzer_find_function_decl|collect_slot_accesses|slot_access_mask_for_named_symbol)\\(" \
    "$ROOT_DIR/src/semantic/slot_analyzer_summary.c"; then
    echo "[build-source-inventory] slot summary owner must not reopen lookup/access traversal" >&2
    missing=1
fi

grep -Fq "loop_flow_record" \
    "$ROOT_DIR/src/semantic/type_checker_flow_loop_snapshot.c" || {
    echo "[build-source-inventory] loop snapshot merge must stay in type_checker_flow_loop_snapshot.c" >&2
    missing=1
}

if grep -Eq "^(copy_resource_snapshot|merge_resource_snapshots_or|loop_flow_record)\\(" \
    "$ROOT_DIR/src/semantic/type_checker_flow_loops.c"; then
    echo "[build-source-inventory] loop snapshot merge regressed into type_checker_flow_loops.c" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_zone_methods_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] zone hosted-method emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_overlay_zone_bind.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] zone effect bind owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_overlay_projection.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] overlay projection owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_projection_method_invalidation.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] projection method invalidation owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_overlay_zone_relation_bind.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] zone relation bind owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_expr_stdlib_queue_builtin.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] stdlib queue builtin emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_expr_domain_query_builtin.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] domain query builtin emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_expr_io_builtin.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] I/O builtin emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_expr_builtin_dispatch.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] expression builtin dispatch owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_expr_core_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] expression core emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_expr_composite_literal_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] composite literal emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_expr_array_access_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] array access emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_let_channel_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] channel let emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_let_box_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] Box/Rc let emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_let_type_register_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] let type-register owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_future_type_query.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] future type query owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_control_flow_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] control-flow emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_block_intent_helpers.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] intent zone bind helper owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_block_intent_rebind_helpers.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] intent zone rebind helper owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_intent_cleanup_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] intent cleanup tail owner is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_intent_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] intent declaration emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_intent_prologue_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] intent prologue emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_mir_cfg_control_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] MIR CFG control emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_mir_match_condition_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] MIR match condition emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_let_collection_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] collection let emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_let_slot_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] slot let emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_domain_constructor_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] domain constructor emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_zone_specialization_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] zone specialization emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_zone_struct_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] zone struct emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/transpiler_intent_zone_binding_emit.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] intent zone-binding emitter is not linked by the C backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/llvm_decl_authority.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] LLVM declaration authority owner is not linked by the LLVM backend source inventory" >&2
    missing=1
fi

if ! grep -Fq '$(CODEGEN_DIR)/llvm_decl_routines.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] LLVM function-routine inventory owner is not linked by the LLVM backend source inventory" >&2
    missing=1
fi

grep -Fq "emit_builtin_domain_query" \
    "$ROOT_DIR/src/codegen/transpiler_expr_builtin_dispatch.c" || {
    echo "[build-source-inventory] C builtin dispatch must delegate domain queries to the compiled owner" >&2
    missing=1
}

grep -Fq "emit_builtin_io" \
    "$ROOT_DIR/src/codegen/transpiler_expr_builtin_dispatch.c" || {
    echo "[build-source-inventory] C builtin dispatch must delegate I/O builtins to the compiled owner" >&2
    missing=1
}

grep -Fq "emit_builtin_has_zone_state" \
    "$ROOT_DIR/src/codegen/transpiler_expr_domain_query_builtin.c" || {
    echo "[build-source-inventory] domain query builtin lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_builtin_file_open" \
    "$ROOT_DIR/src/codegen/transpiler_expr_io_builtin.c" || {
    echo "[build-source-inventory] I/O builtin lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_binary" \
    "$ROOT_DIR/src/codegen/transpiler_expr_core_emit.c" || {
    echo "[build-source-inventory] expression core lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_composite_literal_expression" \
    "$ROOT_DIR/src/codegen/transpiler_expr_composite_literal_emit.c" || {
    echo "[build-source-inventory] composite literal lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_array_access_expression" \
    "$ROOT_DIR/src/codegen/transpiler_expr_array_access_emit.c" || {
    echo "[build-source-inventory] array access lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_try_emit_channel_let" \
    "$ROOT_DIR/src/codegen/transpiler_let_channel_emit.c" || {
    echo "[build-source-inventory] channel let lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_try_emit_box_family_let" \
    "$ROOT_DIR/src/codegen/transpiler_let_box_emit.c" || {
    echo "[build-source-inventory] Box/Rc let lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_register_let_type_after_emit" \
    "$ROOT_DIR/src/codegen/transpiler_let_type_register_emit.c" || {
    echo "[build-source-inventory] let type registration must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "infer_spawn_return_type_name" \
    "$ROOT_DIR/src/codegen/transpiler_future_type_query.c" || {
    echo "[build-source-inventory] Future<T> type query must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_for_loop" \
    "$ROOT_DIR/src/codegen/transpiler_control_flow_emit.c" || {
    echo "[build-source-inventory] control-flow lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_mir_render_branch_condition" \
    "$ROOT_DIR/src/codegen/transpiler_mir_cfg_control_emit.c" || {
    echo "[build-source-inventory] MIR CFG control lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_intent_step_bind_bound_zone" \
    "$ROOT_DIR/src/codegen/transpiler_block_intent_helpers.c" || {
    echo "[build-source-inventory] intent zone bind helpers must stay in their compiled owner" >&2
    missing=1
}

grep -Fq "emit_intent_step_rebind_bound_zone_aliases" \
    "$ROOT_DIR/src/codegen/transpiler_block_intent_rebind_helpers.c" || {
    echo "[build-source-inventory] intent zone rebind helpers must stay in their compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_emit_intent_cleanup_tail" \
    "$ROOT_DIR/src/codegen/transpiler_intent_cleanup_emit.c" || {
    echo "[build-source-inventory] intent cleanup tail lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_intent_decl" \
    "$ROOT_DIR/src/codegen/transpiler_intent_emit.c" || {
    echo "[build-source-inventory] intent declaration lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_zone_decl" \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.c" || {
    echo "[build-source-inventory] zone declaration lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_emit_intent_signature_and_entry" \
    "$ROOT_DIR/src/codegen/transpiler_intent_prologue_emit.c" || {
    echo "[build-source-inventory] intent prologue lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_mir_render_match_case_condition" \
    "$ROOT_DIR/src/codegen/transpiler_mir_match_condition_emit.c" || {
    echo "[build-source-inventory] MIR match condition lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "emit_intent_forward_decl" \
    "$ROOT_DIR/src/codegen/transpiler_intent_zone_binding_emit.c" || {
    echo "[build-source-inventory] intent zone-binding lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_try_emit_collection_ctor_let" \
    "$ROOT_DIR/src/codegen/transpiler_let_collection_emit.c" || {
    echo "[build-source-inventory] collection let lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_emit_domain_constructor_for_decl" \
    "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c" || {
    echo "[build-source-inventory] domain constructor lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_emit_zone_required_specializations" \
    "$ROOT_DIR/src/codegen/transpiler_zone_specialization_emit.c" || {
    echo "[build-source-inventory] zone specialization lowering must stay in its compiled owner" >&2
    missing=1
}

grep -Fq "transpiler_current_overlay_domain_slot_decl" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c" || {
    echo "[build-source-inventory] overlay domain-slot query must be owned by transpiler_projection.c" >&2
    missing=1
}

grep -Fq "transpiler_domain_slot_is_projection_target" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c" || {
    echo "[build-source-inventory] projection-target query must be owned by transpiler_projection.c" >&2
    missing=1
}

grep -Fq "transpiler_find_world_state_decl" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c" || {
    echo "[build-source-inventory] world-state query must be owned by transpiler_projection.c" >&2
    missing=1
}

if grep -Fq "domain_slot_is_projection_target_local" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_projection.h" \
    "$ROOT_DIR/src/codegen/transpiler_call_constructor_result_emit.c"; then
    echo "[build-source-inventory] projection-target query regressed to implementation-header local helper" >&2
    missing=1
fi

if grep -Eq '(^|[^A-Za-z0-9_])current_overlay_domain_slot_decl\(' \
    "$ROOT_DIR/src/codegen/transpiler_overlay_projection.c" \
    "$ROOT_DIR/src/codegen/transpiler_expr_builtin_dispatch.h"; then
    echo "[build-source-inventory] overlay domain-slot query regressed to implementation-header local helper" >&2
    missing=1
fi

if grep -Eq '(^|[^A-Za-z0-9_])find_world_state_decl\(' \
    "$ROOT_DIR/src/codegen/transpiler_projection_sync.h" \
    "$ROOT_DIR/src/codegen/transpiler_expr_builtin_dispatch.h" \
    "$ROOT_DIR/src/codegen/transpiler_world_select_event_emit.c"; then
    echo "[build-source-inventory] world-state query regressed to implementation-header local helper" >&2
    missing=1
fi

if [[ "$missing" -ne 0 ]]; then
    exit 1
fi

echo "[build-source-inventory] Makefile source inventory ok"
