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
        {
            git ls-files -- "$@"
            git ls-files --others --exclude-standard -- "$@"
        } | grep -E "$ext_pattern" \
            | while IFS= read -r source_file; do
                grep -HnE "$pattern" "$source_file" || true
            done
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
    pgy_scan_sh_E 'grep[[:space:]][^;&|]*-[A-Za-z]*P([[:space:]]|$)' tests \
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

bash4_lint_dirs=(tests)
if [[ -d "$ROOT_DIR/tests/self_hosted/parity" ]]; then
    bash4_lint_dirs+=(tests/self_hosted/parity)
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
        | grep -v '^src/lsp/pgy_lsp_protocol.c:.*completion_items_process_cache' \
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

if [[ "$missing" -ne 0 ]]; then
    exit 1
fi

echo "[build-source-inventory] Makefile source inventory ok"
