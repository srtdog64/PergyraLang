#!/usr/bin/env bash
set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v grep >/dev/null 2>&1 \
    || ! command -v sed >/dev/null 2>&1 \
    || ! command -v sort >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

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
PGY_RG_BIN=""
if command -v rg >/dev/null 2>&1; then
    pgy_rg_candidate="$(command -v rg)"
    if "$pgy_rg_candidate" --version >/dev/null 2>&1; then
        PGY_RG_BIN="$pgy_rg_candidate"
    fi
fi
PGY_GIT_AVAILABLE=0
if command -v git >/dev/null 2>&1 \
    && git -C "$ROOT_DIR" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    PGY_GIT_AVAILABLE=1
fi

pgy_normalize_scan_paths()
{
    sed 's#\\#/#g'
}

pgy_git_scan_E()
{
    local ext_pattern="$1"
    local pattern="$2"
    shift 2

    (
        cd "$ROOT_DIR"
        git grep -nE "$pattern" -- "$@" || true
        git ls-files --others --exclude-standard -- "$@" \
            | grep -E "$ext_pattern" \
            | xargs -r grep -InE "$pattern" || true
    ) | pgy_normalize_scan_paths
}

pgy_scan_ch_E()
{
    local pattern="$1"
    shift

    if [[ -n "$PGY_RG_BIN" ]]; then
        (cd "$ROOT_DIR" && "$PGY_RG_BIN" -n --glob '*.c' --glob '*.h' -e "$pattern" "$@" || true) \
            | pgy_normalize_scan_paths
    elif [[ "$PGY_GIT_AVAILABLE" == "1" ]]; then
        pgy_git_scan_E '\.(c|h)$' "$pattern" "$@"
    else
        (cd "$ROOT_DIR" && grep -RInE "$pattern" "$@" --include='*.c' --include='*.h' || true) \
            | pgy_normalize_scan_paths
    fi
}

pgy_scan_sh_E()
{
    local pattern="$1"
    shift

    if [[ -n "$PGY_RG_BIN" ]]; then
        (cd "$ROOT_DIR" && "$PGY_RG_BIN" -n --glob '*.sh' -e "$pattern" "$@" || true) \
            | pgy_normalize_scan_paths
    elif [[ "$PGY_GIT_AVAILABLE" == "1" ]]; then
        pgy_git_scan_E '\.sh$' "$pattern" "$@"
    else
        (cd "$ROOT_DIR" && grep -RInE "$pattern" "$@" --include='*.sh' || true) \
            | pgy_normalize_scan_paths
    fi
}

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
if ! grep -Fq 'LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" source-test-harness-compile-test-smoke' \
    "$ROOT_DIR/scripts/ci_windows_steps.sh"; then
    echo "[build-source-inventory] Windows source harness smoke must use the isolated CI build/bin dirs" >&2
    missing=1
fi
if ! grep -Fq 'LLVM_ENABLED=0 BUILD_DIR="$CI_WINDOWS_BUILD_DIR" BIN_DIR="$CI_WINDOWS_BIN_DIR" self-host-preparation-test-smoke' \
    "$ROOT_DIR/scripts/ci_windows_steps.sh"; then
    echo "[build-source-inventory] Windows self-host preparation smoke must use the isolated CI build/bin dirs" >&2
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
    pgy_scan_ch_E '\b(retun|stncmp|retun_type|infer_spawn_retun)\b' src
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
    pgy_scan_sh_E 'cygpath[[:space:]]+-(m|w)|wslpath[[:space:]]+-(m|w)' tests
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
    pgy_scan_sh_E 'grep[[:space:]][^;&|]*-[A-Za-z]*P' tests \
        | grep -v '^tests/build_source_inventory_smoke.sh:' || true
)"
if [[ -n "$portable_grep_violations" ]]; then
    printf '%s\n' "$portable_grep_violations" >&2
    echo "[build-source-inventory] smoke scripts must not depend on grep -P; use portable grep -E filters instead" >&2
    missing=1
fi

hardcoded_usr_bin_tools="$(
    pgy_scan_sh_E '/usr/bin/(time|bash|python|python3)' tests \
        | grep -v '^tests/build_source_inventory_smoke.sh:' || true
)"
if [[ -n "$hardcoded_usr_bin_tools" ]]; then
    printf '%s\n' "$hardcoded_usr_bin_tools" >&2
    echo "[build-source-inventory] smoke and bench scripts must not hardcode /usr/bin tool paths; Git Bash and macOS layouts differ" >&2
    missing=1
fi

legacy_c_declarator_consumers="$(
    pgy_scan_ch_E 'pergyra_(ast_typed_declarator|func_pointer_declarator_from_decl)\(' src/codegen \
        | grep -v '^src/codegen/transpiler_type_declarator\.[ch]:' || true
)"
if [[ -n "$legacy_c_declarator_consumers" ]]; then
    printf '%s\n' "$legacy_c_declarator_consumers" >&2
    echo "[build-source-inventory] C backend declarator consumers must use ctx-aware _in_ctx APIs" >&2
    missing=1
fi

legacy_c_type_copy_consumers="$(
    pgy_scan_ch_E 'pergyra_ast_type_to_c_copy\(' src/codegen \
        | grep -v '^src/codegen/transpiler_type_render\.[ch]:' || true
)"
if [[ -n "$legacy_c_type_copy_consumers" ]]; then
    printf '%s\n' "$legacy_c_type_copy_consumers" >&2
    echo "[build-source-inventory] C backend AST type consumers must use ctx-aware pergyra_ast_type_to_c_copy_in_ctx" >&2
    missing=1
fi

legacy_type_name_render_consumers="$(
    pgy_scan_ch_E '(^|[^_[:alnum:]])render_type_name\(' src/codegen \
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
    pgy_scan_ch_E 'pergyra_type_to_c_copy\(' src/codegen \
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
    {
        pgy_scan_ch_E 'strcmp\(' src/parser src/semantic src/codegen \
            | grep -E '"(Some|None|Ok|Err)"' || true
        pgy_scan_ch_E '\{[[:space:]]*"(Some|None|Ok|Err)"' \
            src/parser src/semantic src/codegen || true
    } | grep -v '^src/common/match_variant_policy\.[ch]:' \
        | grep -v '^src/codegen/codegen_match_variant_policy\.[ch]:' || true
)"
if [[ -n "$direct_option_result_vocabulary" ]]; then
    printf '%s\n' "$direct_option_result_vocabulary" >&2
    echo "[build-source-inventory] Result/Option constructor vocabulary must use match_variant_policy" >&2
    missing=1
fi
direct_option_result_tag_spelling="$(
    pgy_scan_ch_E 'Pgy(Option(Some|None)|Result(Ok|Err))' src/codegen \
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
for make_term in \
    'PGY_WINDOWS_MSYS_BASH_CANDIDATES' \
    '/usr/bin/bash.exe' \
    'PGY_WINDOWS_BASH_IS_MSYS' \
    'ci-windows requires MSYS2 bash'
do
    if ! grep -Fq "$make_term" "$ROOT_DIR/Makefile"; then
        echo "[build-source-inventory] Windows CI shell source-of-truth missing Makefile term: $make_term" >&2
        missing=1
    fi
done
if ! grep -Fq 'pgy_binary_is_runnable_here()' "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"; then
    echo "[build-source-inventory] shared path helper must classify current-host runnable compiler binaries" >&2
    missing=1
fi
if ! grep -Fq 'pgy_select_optional_exe_binary()' "$ROOT_DIR/tests/pgy_binary_path_helpers.sh" \
    || ! grep -Fq 'pgy_require_runnable_binary_here()' "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"; then
    echo "[build-source-inventory] shared path helper must own optional .exe selection and runnable-binary rejection" >&2
    missing=1
fi
if ! grep -Fq 'pgy_binary_magic_kind()' "$ROOT_DIR/tests/pgy_binary_path_helpers.sh" \
    || ! grep -Fq 'od -An -tx1 -N4' "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"; then
    echo "[build-source-inventory] shared path helper must classify executable format without relying only on file(1)" >&2
    missing=1
fi
for smoke in \
    tests/air_json_schema_smoke.sh \
    tests/fmt_smoke.sh \
    tests/raw_escape_contract_smoke.sh \
    tests/runtime_none_contract_smoke.sh \
    tests/semantic_fixture_isolation_smoke.sh \
    tests/stdlib_surface_smoke.sh
do
    if ! grep -Fq 'pgy_require_runnable_binary_here' "$ROOT_DIR/$smoke"; then
        echo "[build-source-inventory] P0 executable smoke must reject stale cross-platform binaries before launch: $smoke" >&2
        missing=1
    fi
done
if ! grep -Fq 'pgy_binary_is_runnable_here "$candidate"' "$ROOT_DIR/tests/compare_backends.sh" \
    || ! grep -Fq 'compiler binary is not runnable on this host' "$ROOT_DIR/tests/compare_backends.sh"; then
    echo "[build-source-inventory] backend compare must reject stale cross-platform compiler binaries before launch" >&2
    missing=1
fi
if ! grep -Fq 'pgy_powershell_quote' "$ROOT_DIR/tests/compare_backends.sh"; then
    echo "[build-source-inventory] backend compare PowerShell fallback must quote paths explicitly" >&2
    missing=1
fi
if ! grep -Fq 'pgy_powershell_quote()' "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"; then
    echo "[build-source-inventory] PowerShell path quoting must live in the shared binary path helper" >&2
    missing=1
fi
if ! grep -Fq 'pgy_windows_powershell_path_prefix_from_current_path()' "$ROOT_DIR/tests/pgy_binary_path_helpers.sh" \
    || ! grep -Fq 'pgy_windows_powershell_path_prefix_from_current_path' "$ROOT_DIR/tests/compare_backends.sh"; then
    echo "[build-source-inventory] backend compare PowerShell fallback must consume the current Bash launch PATH" >&2
    missing=1
fi
if ! grep -Fq 'pgy_windows_path_is_git_runtime()' "$ROOT_DIR/tests/pgy_binary_path_helpers.sh" \
    || ! grep -Fq '*"\\Git\\mingw64\\bin"|*"\\Git\\usr\\bin")' "$ROOT_DIR/tests/pgy_binary_path_helpers.sh" \
    || ! grep -Fq '"/c/ProgramData/mingw64/mingw64/bin"' "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"; then
    echo "[build-source-inventory] runtime PATH must prefer real MinGW/LLVM runtime dirs over Git-Bash runtime mounts" >&2
    missing=1
fi
if ! grep -Fq 'pgy_binary_is_runnable_here "$ABI_PIPELINE_BIN"' "$ROOT_DIR/tests/compare_backends.sh" \
    || ! grep -Fq 'ABI pipeline test binary is not runnable on this host' "$ROOT_DIR/tests/compare_backends.sh"; then
    echo "[build-source-inventory] backend compare ABI precheck must reject stale cross-platform test binaries before launch" >&2
    missing=1
fi
if ! grep -Fq 'pgy_binary_is_runnable_here "$ABI_BIN"' "$ROOT_DIR/tests/abi_pipeline_same_process_smoke.sh" \
    || ! grep -Fq 'ABI pipeline test binary is not runnable on this host' "$ROOT_DIR/tests/abi_pipeline_same_process_smoke.sh" \
    || ! grep -Fq 'pgy_powershell_quote "$ABI_WIN_BIN"' "$ROOT_DIR/tests/abi_pipeline_same_process_smoke.sh"; then
    echo "[build-source-inventory] ABI same-process smoke must share runnable-binary and PowerShell quoting policy" >&2
    missing=1
fi
if ! grep -Fq 'command -v dirname' "$ROOT_DIR/tests/compare_backends.sh" \
    || ! grep -Fq 'PATH="/usr/bin:/bin:$PATH"' "$ROOT_DIR/tests/compare_backends.sh"; then
    echo "[build-source-inventory] backend compare must self-heal Git Bash POSIX tool PATH" >&2
    missing=1
fi
if ! grep -Fq 'command -v dirname' "$ROOT_DIR/tests/source_utf8_smoke.sh" \
    || ! grep -Fq 'PATH="/usr/bin:/bin:$PATH"' "$ROOT_DIR/tests/source_utf8_smoke.sh"; then
    echo "[build-source-inventory] source UTF-8 smoke must self-heal Git Bash POSIX tool PATH" >&2
    missing=1
fi
if ! grep -Fq 'PGY_BACKEND_COMPARE_SHARD_TOTAL' "$ROOT_DIR/tests/compare_backends.sh" \
    || ! grep -Fq 'case_index % shard_total' "$ROOT_DIR/tests/compare_backends.sh"; then
    echo "[build-source-inventory] backend compare must keep deterministic shard selection" >&2
    missing=1
fi
if ! grep -Fq 'CI_BACKEND_COMPARE_SHARD_TOTAL ?= 20' "$ROOT_DIR/Makefile" \
    || ! grep -Fq 'PGY_BACKEND_COMPARE_SHARD_TOTAL="$CI_BACKEND_COMPARE_SHARD_TOTAL"' "$ROOT_DIR/scripts/ci_linux_steps.sh"; then
    echo "[build-source-inventory] CI backend compare must stay shard-wired" >&2
    missing=1
fi
if ! grep -Fq 'PGY_BACKEND_COMPARE_PRECHECK ?= 1' "$ROOT_DIR/Makefile" \
    || ! grep -Fq 'PGY_BACKEND_COMPARE_PRECHECK_SAME_PROCESS="$(PGY_BACKEND_COMPARE_PRECHECK)"' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] backend compare precheck must remain overrideable and default-on" >&2
    missing=1
fi
if ! grep -Fq 'PGY_BACKEND_COMPARE_START_INDEX' "$ROOT_DIR/tests/compare_backends.sh" \
    || ! grep -Fq 'PGY_BACKEND_COMPARE_MAX_CASES' "$ROOT_DIR/tests/compare_backends.sh" \
    || ! grep -Fq 'PGY_BACKEND_COMPARE_START_INDEX="$(PGY_BACKEND_COMPARE_START_INDEX)"' "$ROOT_DIR/Makefile" \
    || ! grep -Fq 'PGY_BACKEND_COMPARE_MAX_CASES="$(PGY_BACKEND_COMPARE_MAX_CASES)"' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] backend compare must keep deterministic range/limit selection wired" >&2
    missing=1
fi
if ! grep -Fq 'PGY_BACKEND_COMPARE_CASES' "$ROOT_DIR/tests/compare_backends.sh" \
    || ! grep -Fq 'filter_cases_by_name' "$ROOT_DIR/tests/compare_backends.sh" \
    || ! grep -Fq 'PGY_BACKEND_COMPARE_CASES ?=' "$ROOT_DIR/Makefile" \
    || ! grep -Fq 'PGY_BACKEND_COMPARE_CASES="$(PGY_BACKEND_COMPARE_CASES)"' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] backend compare must keep deterministic case-name selection wired" >&2
    missing=1
fi
if ! grep -Fq 'backend-compare-linux:' "$ROOT_DIR/.github/workflows/ci.yml" \
    || ! grep -Fq 'PGY_BACKEND_COMPARE_PRECHECK=0' "$ROOT_DIR/.github/workflows/ci.yml" \
    || ! grep -Fq 'PGY_BACKEND_COMPARE_SHARD_TOTAL=20' "$ROOT_DIR/.github/workflows/ci.yml" \
    || ! grep -Fq 'PGY_BACKEND_COMPARE_SHARD_INDEX="${{ matrix.shard }}"' "$ROOT_DIR/.github/workflows/ci.yml"; then
    echo "[build-source-inventory] GitHub CI must keep full C/LLVM backend-compare shard matrix" >&2
    missing=1
fi

bash4_lint_dirs=(tests)
if [[ -d "$ROOT_DIR/src/self_hosted/parity" ]]; then
    bash4_lint_dirs+=(src/self_hosted/parity)
fi

bash4_only_smoke_terms="$(
    pgy_scan_sh_E '(^|[^A-Za-z0-9_])(mapfile|readarray)([^A-Za-z0-9_]|$)|(^|[[:space:]])(declare|typeset)[[:space:]]+-A([^A-Za-z0-9_]|$)|\$\{[^}]+(,,|\^\^)[^}]*\}' "${bash4_lint_dirs[@]}" \
        | grep -v '^tests/build_source_inventory_smoke.sh:' || true
)"
if [[ -n "$bash4_only_smoke_terms" ]]; then
    printf '%s\n' "$bash4_only_smoke_terms" >&2
    echo "[build-source-inventory] smoke / self_hosted parity scripts must remain compatible with macOS Bash 3.2; avoid Bash 4-only mapfile/readarray, associative arrays, and case-conversion expansions" >&2
    missing=1
fi

multiline_case_continuations="$(
    pgy_scan_sh_E '\|\\$' "${bash4_lint_dirs[@]}" \
        | grep -v '^tests/build_source_inventory_smoke.sh:' || true
)"
if [[ -n "$multiline_case_continuations" ]]; then
    printf '%s\n' "$multiline_case_continuations" >&2
    echo "[build-source-inventory] smoke / self_hosted parity scripts must not use case-pattern line continuations; macOS Bash 3.2 parses these unreliably inside command substitution" >&2
    missing=1
fi

semantic_self_include_leaks="$(
    pgy_scan_ch_E '#include "\.\./semantic/' src/semantic
)"
if [[ -n "$semantic_self_include_leaks" ]]; then
    printf '%s\n' "$semantic_self_include_leaks" >&2
    echo "[build-source-inventory] semantic files must include same-directory headers directly" >&2
    missing=1
fi

rir_program_root_bridge="$(
    pgy_scan_ch_E 'g_rir_program_root|rir_set_program_root|rir_program_root\(\)' src/compiler
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
    pgy_scan_ch_E 'data\.program|data\.(func_decl|class_decl|ability_decl|role_decl|party_decl|roster_decl|world_decl|relation_decl|effect_decl|zone_decl|enum_decl|type_alias)\.name' src/lsp
)"
if [[ -n "$lsp_ast_payload_leaks" ]]; then
    printf '%s\n' "$lsp_ast_payload_leaks" >&2
    echo "[build-source-inventory] LSP must consume parser AST accessors for declaration inventory" >&2
    missing=1
fi

lsp_unclamped_offsets="$(
    pgy_scan_ch_E 'off[[:space:]]*\+=[[:space:]]*\(size_t\)n|if[[:space:]]*\(n[[:space:]]*>[[:space:]]*0\)[[:space:]]*off[[:space:]]*\+=' src/lsp
)"
if [[ -n "$lsp_unclamped_offsets" ]]; then
    printf '%s\n' "$lsp_unclamped_offsets" >&2
    echo "[build-source-inventory] LSP JSON builders must clamp snprintf offsets" >&2
    missing=1
fi

lsp_ignored_offset_status="$(
    pgy_scan_ch_E '^[[:space:]]*lsp_advance_json_offset[[:space:]]*\([^;]*\);' src/lsp
)"
if [[ -n "$lsp_ignored_offset_status" ]]; then
    printf '%s\n' "$lsp_ignored_offset_status" >&2
    echo "[build-source-inventory] LSP JSON builders must handle clamp failure" >&2
    missing=1
fi

json_find_string_uses="$(pgy_scan_ch_E 'json_find_string\(' src/lsp)"
if [[ -n "$json_find_string_uses" ]]; then
    printf '%s\n' "$json_find_string_uses" >&2
    echo "[build-source-inventory] LSP string extraction must use copy/dup ownership lanes" >&2
    missing=1
fi
for lsp_json_term in \
    "json_find_string_copy" \
    "json_find_string_dup"; do
    if [[ -z "$(pgy_scan_ch_E "$lsp_json_term" src/lsp)" ]]; then
        echo "[build-source-inventory] LSP missing JSON string ownership lane: $lsp_json_term" >&2
        missing=1
    fi
done
lsp_atoi_uses="$(pgy_scan_ch_E 'atoi\(' src/lsp)"
if [[ -n "$lsp_atoi_uses" ]]; then
    printf '%s\n' "$lsp_atoi_uses" >&2
    echo "[build-source-inventory] LSP external numeric parsing must use checked strtol-style parsing, not atoi" >&2
    missing=1
fi
unchecked_atoi_uses="$(
    pgy_scan_ch_E 'atoi\(' src
)"
if [[ -n "$unchecked_atoi_uses" ]]; then
    printf '%s\n' "$unchecked_atoi_uses" >&2
    echo "[build-source-inventory] production C numeric parsing must use checked strtol-style parsing, not atoi" >&2
    missing=1
fi
unchecked_numeric_parse_uses="$(
    pgy_scan_ch_E 'strtol\(|strtoul\(|strtoll\(|strtoull\(' \
        src/compiler src/parser src/lsp src/codegen src/semantic \
        | grep -v '^src/common/numeric_parse.c:' || true
)"
if [[ -n "$unchecked_numeric_parse_uses" ]]; then
    printf '%s\n' "$unchecked_numeric_parse_uses" >&2
    echo "[build-source-inventory] checked numeric parsing must route through src/common/numeric_parse" >&2
    missing=1
fi

mutable_static_char_arrays="$(
    pgy_scan_ch_E 'static[[:space:]]+char[[:space:]]+[A-Za-z0-9_]+[[:space:]]*\[[^]]+\]' \
        src/codegen src/semantic src/compiler src/lsp
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
    pgy_scan_ch_E 'static[[:space:]]+char[[:space:]]+\*[[:space:]]*[A-Za-z0-9_]+[[:space:]]*(=|;)' \
        src/codegen src/semantic src/compiler src/lsp
)"
if [[ -n "$mutable_static_char_pointers" ]]; then
    printf '%s\n' "$mutable_static_char_pointers" >&2
    echo "[build-source-inventory] mutable static char pointers are forbidden in compiler helpers" >&2
    missing=1
fi
top_level_mutable_statics="$(
    pgy_scan_ch_E '^static[[:space:]]+[A-Za-z_][A-Za-z0-9_[:space:]]*\*?[[:space:]]*[A-Za-z_][A-Za-z0-9_]*(\[[^]]+\])?[[:space:]]*(=|;)' \
        src/codegen src/semantic src/compiler src/lsp \
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
for match_subject_consumer in \
    "$ROOT_DIR/src/codegen/llvm_mir_match_condition.c" \
    "$ROOT_DIR/src/codegen/transpiler_mir_match_condition_emit.c"; do
    if ! grep -Fq 'pgy_codegen_match_subject_for_branch(' \
        "$match_subject_consumer"; then
        echo "[build-source-inventory] MIR match condition must consume MIR branch match-subject fact owner: $match_subject_consumer" >&2
        missing=1
    fi
    if grep -Fq 'pgy_codegen_match_subject_for_case(func_decl, case_node)' \
        "$match_subject_consumer"; then
        echo "[build-source-inventory] backend MIR match condition regressed to AST case match-subject fallback: $match_subject_consumer" >&2
        missing=1
    fi
    if grep -Fq 'ast_find_match_subject_for_case(ast_func_body(func_decl)' \
        "$match_subject_consumer"; then
        echo "[build-source-inventory] backend MIR match condition must not rescan function body directly: $match_subject_consumer" >&2
        missing=1
    fi
done
if ! grep -Fq 'match-case branch is missing MIR match subject fact' \
    "$ROOT_DIR/src/compiler/mir_fact_terminator_validate.c"; then
    echo "[build-source-inventory] MIR validator must gate match-case subject facts" >&2
    missing=1
fi
if grep -Fq 'pgy_codegen_match_subject_for_case' \
    "$ROOT_DIR/src/codegen/codegen_match_subject_lookup.c" \
    "$ROOT_DIR/src/codegen/codegen_match_subject_lookup.h"; then
    echo "[build-source-inventory] match-subject owner must not retain AST case compatibility fallback" >&2
    missing=1
fi
if grep -Fq 'ast_find_match_subject_for_case(ast_func_body(func_decl)' \
    "$ROOT_DIR/src/codegen/codegen_match_subject_lookup.c"; then
    echo "[build-source-inventory] match-subject owner must consume MIR branch facts only" >&2
    missing=1
fi
if ! grep -Fq '$(CODEGEN_DIR)/codegen_match_subject_lookup.c' \
    "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] match-subject owner missing from Makefile source inventory" >&2
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
    || [[ -n "$(pgy_scan_ch_E 'pgy_cc_extra_target_flag' src/compiler)" ]]; then
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
    pgy_scan_ch_E 'fseek\(|ftell\(|fread\(buf' \
        src/compiler/debugger.c src/compiler/fmt_io.c
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

if ! awk '
    /^AIR_CORE_OBJECTS[[:space:]]*=/ { in_air = 1 }
    /^MIR_CORE_OBJECTS[[:space:]]*=/ { in_air = 0 }
    in_air && /\$\(BUILD_DIR\)\/compiler\/mir_type_helpers[.]o/ { found = 1 }
    END { exit found ? 0 : 1 }
' "$ROOT_DIR/Makefile"; then
    echo "[build-source-inventory] MIR type helper object is not linked by AIR_CORE_OBJECTS" >&2
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

if [[ -n "$(pgy_scan_ch_E 'ASTNode \*subject_type = ast_intent_involves_subject_type\(involves\);' \
    src/codegen)" ]]; then
    echo "[build-source-inventory] intent participant accessors must check involves before reading subject_type" >&2
    missing=1
fi

for intent_zone_slot_term in \
    "resolve_intent_zone_slot_name_for_zone_with_bindings" \
    "intent_zone_binding_type_name_with_bindings("; do
    grep -Fq "$intent_zone_slot_term" \
        "$ROOT_DIR/src/codegen/transpiler_intent_zone_slot.c" || {
        echo "[build-source-inventory] MIR-only intent zone-slot lookup must consume participant metadata: $intent_zone_slot_term" >&2
        missing=1
    }
done

if [[ -n "$(pgy_scan_ch_E 'resolve_intent_zone_slot_name_for_zone_with_metadata|intent_zone_binding_type_name_with_metadata' \
    src/codegen)" ]]; then
    echo "[build-source-inventory] legacy participant-array intent zone-slot metadata API must not reappear" >&2
    missing=1
fi

for metadata_slot_consumer in \
    src/codegen/transpiler_block_intent_helpers.c \
    src/codegen/transpiler_block_intent_rebind_helpers.c \
    src/codegen/transpiler_intent_zone_binding_emit.c \
    src/codegen/transpiler_intent_emit.c; do
    grep -Fq "resolve_intent_zone_slot_name_for_zone_with_bindings" \
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

for builtin_fact in \
    '{ "Contains", "Bool"' \
    '{ "MapSize", "Int"' \
    '{ "QueueEmpty", "Bool"' \
    '{ "QueueSize", "Int"' \
    '{ "SetHas", "Bool"' \
    '{ "SetSize", "Int"' \
    '{ "StringReplace", "String"' \
    '{ "StringSplit", "Array<String>"' \
    '{ "StringTrim", "String"' \
    '{ "Substring", "String"' \
    '{ "ToLower", "String"' \
    '{ "ToUpper", "String"'; do
    if ! grep -Fq "$builtin_fact" \
        "$ROOT_DIR/src/codegen/transpiler_builtin_type_table.c"; then
        echo "[build-source-inventory] fixed-return collection builtin type fact missing: $builtin_fact" >&2
        missing=1
    fi
done

for dynamic_numeric in \
    '{ "Clamp",' \
    '{ "Max",' \
    '{ "Min",'; do
    if grep -Fq "$dynamic_numeric" \
        "$ROOT_DIR/src/codegen/transpiler_builtin_type_table.c"; then
        echo "[build-source-inventory] dynamic numeric builtin must not be fixed-return typed: $dynamic_numeric" >&2
        missing=1
    fi
done

for fixed_return_policy_leak in \
    'TRANS_INFER_CALL_RETURNS_STRING' \
    'TRANS_INFER_CALL_E' \
    'TRANS_INFER_CALL_PI' \
    'TRANS_INFER_CALL_MEASURE' \
    'TRANS_INFER_CALL_QUBIT_STATE'; do
    if grep -Fq "$fixed_return_policy_leak" \
        "$ROOT_DIR/src/codegen/transpiler_expr_type_infer_call_policy.h" \
        "$ROOT_DIR/src/codegen/transpiler_expr_type_infer_call_policy.c"; then
        echo "[build-source-inventory] C call inference policy must not own fixed-return builtin fact: $fixed_return_policy_leak" >&2
        missing=1
    fi
done

if grep -R -F "llvm_stmt_call_returns_collection_size" \
    "$ROOT_DIR/src/codegen" >/dev/null \
    || grep -R -F "llvm_stmt_call_returns_collection_bool" \
        "$ROOT_DIR/src/codegen" >/dev/null \
    || grep -R -F "llvm_stmt_call_returns_domain_bool" \
        "$ROOT_DIR/src/codegen" >/dev/null; then
    echo "[build-source-inventory] LLVM type inference regressed to local fixed-return builtin tables" >&2
    missing=1
fi

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

grep -Fq "transpiler_current_overlay_domain_slot_type_name" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c" || {
    echo "[build-source-inventory] overlay domain-slot type query must be owned by transpiler_projection.c" >&2
    missing=1
}

grep -Fq "transpiler_current_overlay_domain_slot_is_projection" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c" || {
    echo "[build-source-inventory] overlay projection-slot predicate must be owned by transpiler_projection.c" >&2
    missing=1
}

if grep -Fq "ast_domain_slot_is_subject" \
    "$ROOT_DIR/src/codegen/transpiler_expr_domain_query_builtin.c"; then
    echo "[build-source-inventory] domain query builtins regressed to direct domain-slot subject checks" >&2
    missing=1
fi

grep -Fq "transpiler_domain_slot_is_projection_target" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c" || {
    echo "[build-source-inventory] projection-target query must be owned by transpiler_projection.c" >&2
    missing=1
}

grep -Fq "transpiler_domain_slot_view_is_projection_slot" \
    "$ROOT_DIR/src/codegen/transpiler_projection.c" || {
    echo "[build-source-inventory] projection slot-view predicate must be owned by transpiler_projection.c" >&2
    missing=1
}

grep -Fq "transpiler_domain_slot_view_is_projection_slot(" \
    "$ROOT_DIR/src/codegen/transpiler_domain_constructor_emit.c" || {
    echo "[build-source-inventory] domain constructors must consume the projection slot-view predicate" >&2
    missing=1
}

grep -Fq "emit_domain_projection_sync_loop_from_view" \
    "$ROOT_DIR/src/codegen/transpiler_domain_provenance_emit.c" || {
    echo "[build-source-inventory] C projection sync must expose a domain slot-view consumer" >&2
    missing=1
}

grep -Fq "emit_domain_projection_sync_loop_from_view(ctx" \
    "$ROOT_DIR/src/codegen/transpiler_relation_effect_emit.c" || {
    echo "[build-source-inventory] relation/effect projection sync must consume the domain slot view" >&2
    missing=1
}

grep -Fq "emit_domain_projection_sync_loop_from_view(ctx" \
    "$ROOT_DIR/src/codegen/transpiler_zone_decl_emit.c" || {
    echo "[build-source-inventory] zone projection sync must consume the domain slot view" >&2
    missing=1
}

if grep -Fq "ast_compat_slots" \
    "$ROOT_DIR/src/codegen/transpiler_relation_effect_emit.c"; then
    echo "[build-source-inventory] relation/effect projection sync regressed to AST slot array consumption" >&2
    missing=1
fi

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

if grep -Fq "ast_domain_slot_type(" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_projection.c"; then
    echo "[build-source-inventory] overlay projection sync regressed to AST domain-slot type lookup" >&2
    missing=1
fi

grep -Fq "transpiler_hosted_domain_slot_view_from_decl(ctx" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_projection.c" || {
    echo "[build-source-inventory] overlay projection sync must consume domain slot views" >&2
    missing=1
}

if grep -Fq "ast_zone_slots" \
    "$ROOT_DIR/src/codegen/transpiler_overlay_projection.c"; then
    echo "[build-source-inventory] overlay projection sync regressed to AST zone-slot scanning" >&2
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
