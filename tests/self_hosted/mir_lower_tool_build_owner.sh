#!/usr/bin/env bash
# Owns the one build per job of the MIR lowering tool
# (src/self_hosted/mir_lower/main.pgy) that the MIR flow identity gates run.
# It follows the shared-build rule of
# docs/truth/decisions/2026-11-11_semantic_owner_probes_merge_into_one_build.md:
# a make recipe builds the tool once, and a gate reuses it only through
# receipts.
#
# `pgy_mir_lower_tool_build ROOT PGY DIR` compiles the tool with the command
# each gate used to run itself (`pgy main.pgy --backend=c`). It writes
# receipts naming the launcher, the self-host driver the launcher resolves,
# every self-host source and the binary. A gate takes the binary through
# `pgy_mir_lower_tool_binary ROOT PGY`. A missing or different receipt fails
# the gate, and a gate never builds the tool. Run a gate through its make
# target, or run tests/self_hosted/mir_lower_tool_build.sh DIR and export
# PGY_MIR_LOWER_TOOL_DIR=DIR first.

PGY_MIR_LOWER_TOOL_SOURCE="src/self_hosted/mir_lower/main.pgy"

pgy_mir_lower_tool_error() {
    echo "[mir-lower-tool] $*" >&2
    return 1
}

pgy_mir_lower_tool_digest() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$@"
    else
        shasum -a 256 "$@"
    fi
}

# The driver the launcher runs (driver_resolve_self_host_binary): the
# PGY_SELF_DRIVER_BIN override, else pgy-self-driver beside the launcher.
pgy_mir_lower_tool_driver() {
    local pgy="$1"
    if [[ -n "${PGY_SELF_DRIVER_BIN:-}" ]]; then
        pgy_select_optional_exe_binary "$PGY_SELF_DRIVER_BIN"
    else
        pgy_select_optional_exe_binary "$(dirname "$pgy")/pgy-self-driver"
    fi
}

pgy_mir_lower_tool_identity() {
    local pgy="$1" driver
    driver="$(pgy_mir_lower_tool_driver "$pgy")"
    printf 'launcher %s\n' "$(pgy_mir_lower_tool_digest "$pgy" | cut -d' ' -f1)"
    if [[ -f "$driver" ]]; then
        printf 'driver %s\n' "$(pgy_mir_lower_tool_digest "$driver" | cut -d' ' -f1)"
    else
        printf 'driver absent\n'
    fi
}

pgy_mir_lower_tool_inputs() {
    local -a digest=(sha256sum)
    command -v sha256sum >/dev/null 2>&1 || digest=(shasum -a 256)
    (cd "$1" && find src/self_hosted -name '*.pgy' -type f | LC_ALL=C sort |
        xargs "${digest[@]}")
}

pgy_mir_lower_tool_build() {
    local root="$1" pgy="$2" dir="$3" runner_tmp started
    mkdir -p "$dir" || return 1
    dir="$(cd "$dir" && pwd -P)"
    rm -f "$dir/mir_lower.exe" "$dir/identity.receipt" "$dir/inputs.sha256" \
        "$dir/tool.sha256" "$dir/mir_lower.compile.log"
    runner_tmp="$dir/c_runner_tmp"
    mkdir -p "$runner_tmp" || return 1
    started="$(date +%s)"
    if ! (
        case "$(uname -s 2>/dev/null || echo unknown)" in
            MINGW*|MSYS*|CYGWIN*)
                TMPDIR="$(pgy_path_for_windows_tool "$runner_tmp")"
                SHELL="$(command -v bash)"; export SHELL ;;
            *) TMPDIR="$runner_tmp" ;;
        esac
        export TMPDIR
        cd "$root" && "$pgy" \
            "$(pgy_path_for_compiler "$pgy" "$PGY_MIR_LOWER_TOOL_SOURCE")" \
            --backend=c -o "$(pgy_path_for_compiler "$pgy" "$dir/mir_lower.exe")"
    ) >"$dir/mir_lower.compile.log" 2>&1; then
        cat "$dir/mir_lower.compile.log" >&2
        pgy_mir_lower_tool_error "build failed: pgy=$pgy" || return 1
    fi
    pgy_mir_lower_tool_identity "$pgy" >"$dir/identity.receipt" || return 1
    pgy_mir_lower_tool_inputs "$root" >"$dir/inputs.sha256" || return 1
    (cd "$dir" && pgy_mir_lower_tool_digest mir_lower.exe) >"$dir/tool.sha256" || return 1
    echo "[mir-lower-tool] built in $(( $(date +%s) - started ))s: $dir"
}

pgy_mir_lower_tool_validate() {
    local root="$1" pgy="$2" dir="$3" name
    for name in identity.receipt inputs.sha256 tool.sha256 mir_lower.exe; do
        [[ -s "$dir/$name" ]] ||
            pgy_mir_lower_tool_error "shared build lacks $name: $dir" || return 1
    done
    cmp -s <(pgy_mir_lower_tool_identity "$pgy") "$dir/identity.receipt" ||
        pgy_mir_lower_tool_error "shared build names another launcher or driver than $pgy" || return 1
    cmp -s <(pgy_mir_lower_tool_inputs "$root") "$dir/inputs.sha256" ||
        pgy_mir_lower_tool_error "shared build predates the current self-host sources" || return 1
    cmp -s <(cd "$dir" && pgy_mir_lower_tool_digest mir_lower.exe) "$dir/tool.sha256" ||
        pgy_mir_lower_tool_error "shared build binary changed after its receipt" || return 1
}

# usage: pgy_mir_lower_tool_binary ROOT PGY
# Prints the shared build's tool once its receipts match.
pgy_mir_lower_tool_binary() {
    local root="$1" pgy="$2" dir="${PGY_MIR_LOWER_TOOL_DIR:-}"
    [[ -n "$dir" ]] ||
        pgy_mir_lower_tool_error "PGY_MIR_LOWER_TOOL_DIR is unset; run the gate through make" || return 1
    dir="$(cd "$dir" 2>/dev/null && pwd -P)" ||
        pgy_mir_lower_tool_error "shared build directory is missing: ${PGY_MIR_LOWER_TOOL_DIR}" || return 1
    pgy_mir_lower_tool_validate "$root" "$pgy" "$dir" || return 1
    printf '%s\n' "$dir/mir_lower.exe"
}
