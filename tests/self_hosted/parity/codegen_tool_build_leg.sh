#!/usr/bin/env bash
# Codegen parity tool-build and fixture-manifest projection owner.

CODEGEN_TOOL_CACHE_SCHEMA="pgy.selfhost.completeness-cache.v1|codegen-parity-tool-build.v1"
CODEGEN_TOOL_CACHE_PREPARED=0
CODEGEN_TOOL_CACHE_ENABLED=0
CODEGEN_TOOL_SOURCE_SET_FINGERPRINT="no-cache"

codegen_tool_sha256_file() {
    sha256sum "$1" | cut -d' ' -f1
}

prepare_codegen_tool_cache() {
    local fingerprint_input="$ABS_BUILD/codegen_tool_source_set_fingerprint.txt"
    local source_path

    if [[ "$CODEGEN_TOOL_CACHE_PREPARED" -eq 1 ]]; then
        return
    fi
    CODEGEN_TOOL_CACHE_PREPARED=1
    if [[ "${PGY_SELFHOST_CODEGEN_TOOL_CACHE:-1}" == "0" ]] \
        || ! command -v sha256sum >/dev/null 2>&1; then
        return
    fi

    : >"$fingerprint_input"
    while IFS= read -r source_path; do
        printf '%s\t%s\n' "${source_path#"$ROOT_DIR"/}" \
            "$(codegen_tool_sha256_file "$source_path")" \
            >>"$fingerprint_input"
    done < <(find "$ROOT_DIR/src/self_hosted" -type f -name '*.pgy' -print \
        | LC_ALL=C sort)
    if [[ ! -s "$fingerprint_input" ]]; then
        echo "[self-host-parity:codegen] empty codegen tool source-set fingerprint" >&2
        exit 1
    fi
    CODEGEN_TOOL_SOURCE_SET_FINGERPRINT="$(
        codegen_tool_sha256_file "$fingerprint_input"
    )"
    CODEGEN_TOOL_CACHE_ENABLED=1
}

codegen_tool_build_key() {
    local backend="$1"

    printf '%s|stage=codegen|backend=%s|source-set=%s|tool-source=%s|tool-source-hash=%s|compiler-executable=%s\n' \
        "$CODEGEN_TOOL_CACHE_SCHEMA" \
        "$backend" \
        "$CODEGEN_TOOL_SOURCE_SET_FINGERPRINT" \
        "${TOOL_SOURCE#"$ROOT_DIR"/}" \
        "$(codegen_tool_sha256_file "$TOOL_SOURCE")" \
        "$(codegen_tool_sha256_file "$PGY")"
}

compile_tool_backend() {
    local backend="$1"
    local tool_bin="$2"
    local compile_log="$ABS_BUILD/tool_${backend}.compile.log"
    local compile_out="$ABS_BUILD/tool_${backend}.compile.out"
    local compile_err="$ABS_BUILD/tool_${backend}.compile.err"
    local stamp="$ABS_BUILD/tool_${backend}.build.key"
    local build_key=""

    prepare_codegen_tool_cache
    if [[ "$CODEGEN_TOOL_CACHE_ENABLED" -eq 1 ]]; then
        build_key="$(codegen_tool_build_key "$backend")"
        if [[ -f "$tool_bin" && -f "$stamp" ]] \
            && grep -Fxq "$build_key" "$stamp"; then
            echo "[self-host-parity:codegen] reusing fingerprinted codegen tool backend=$backend"
            return 0
        fi
    fi

    echo "[self-host-parity:codegen] compiling codegen tool backend=$backend..."
    if ! run_native_capture "$ROOT_DIR" "$compile_out" "$compile_err" "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$TOOL_SOURCE")" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$tool_bin")"; then
        rm -f "$stamp"
        cat "$compile_out" "$compile_err" > "$compile_log"
        if [[ "$backend" == "llvm" ]] \
            && pgy_selfhost_log_reports_no_llvm "$compile_log"; then
            echo "[self-host-parity:codegen] LLVM backend unavailable; skipping llvm-compiled codegen tool"
            return 2
        fi
        echo "[self-host-parity:codegen] backend=$backend codegen tool failed to build" >&2
        cat "$compile_log" >&2
        exit 1
    fi
    cat "$compile_out" "$compile_err" > "$compile_log"
    if [[ "$CODEGEN_TOOL_CACHE_ENABLED" -eq 1 ]]; then
        printf '%s\n' "$build_key" >"$stamp"
    fi
    return 0
}

read_codegen_fixture_manifest() {
    local line

    # The manifest and C parity leg consume the same compiled codegen owner.
    compile_tool_backend c "$C_TOOL_BIN"
    C_TOOL_COMPILED=1
    FIXTURES=()
    if ! run_native_capture "$ROOT_DIR" "$CODEGEN_FIXTURE_MANIFEST_FILE" \
        "$ABS_BUILD/codegen_fixture_manifest.err" \
        "$C_TOOL_BIN" --fixture-manifest; then
        echo "[self-host-parity:codegen] fixture manifest emission failed" >&2
        cat "$ABS_BUILD/codegen_fixture_manifest.err" >&2
        exit 1
    fi

    while IFS= read -r line; do
        line="${line%$'\r'}"
        [[ -n "$line" ]] || continue
        FIXTURES+=("$line")
    done <"$CODEGEN_FIXTURE_MANIFEST_FILE"

    if [[ "${#FIXTURES[@]}" -ne 73 ]]; then
        echo "[self-host-parity:codegen] fixture manifest count drifted: ${#FIXTURES[@]} != 73" >&2
        exit 1
    fi

    if [[ -n "${PGY_SELFHOST_CODEGEN_FIXTURES:-}" ]]; then
        local selected=()
        local requested
        local requested_fixtures=()
        IFS=', ' read -r -a requested_fixtures \
            <<< "$PGY_SELFHOST_CODEGEN_FIXTURES"
        for requested in "${requested_fixtures[@]}"; do
            [[ -n "$requested" ]] || continue
            if ! printf '%s\n' "${FIXTURES[@]}" | grep -Fxq "$requested"; then
                echo "[self-host-parity:codegen] unknown fixture filter: $requested" >&2
                exit 1
            fi
            selected+=("$requested")
        done
        if [[ "${#selected[@]}" -eq 0 ]]; then
            echo "[self-host-parity:codegen] empty fixture filter" >&2
            exit 1
        fi
        FIXTURES=("${selected[@]}")
    fi
}
