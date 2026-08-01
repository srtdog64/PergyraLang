#!/usr/bin/env bash
# Fingerprinted parser-tool build owner shared by parser parity and bootstrap.

PARSER_TOOL_CACHE_SCHEMA="pgy.selfhost.parser-tool-build.v1"

parser_tool_sha256_file() {
    sha256sum "$1" | cut -d' ' -f1
}

parser_tool_source_set_fingerprint() {
    local cache_dir="$1"
    local fingerprint_input="$cache_dir/source_set_fingerprint.txt"
    local source_path

    : >"$fingerprint_input"
    while IFS= read -r source_path; do
        printf '%s\t%s\n' "${source_path#"$ROOT_DIR"/}" \
            "$(parser_tool_sha256_file "$source_path")" \
            >>"$fingerprint_input"
    done < <(find "$ROOT_DIR/src/self_hosted" -type f -name '*.pgy' -print \
        | LC_ALL=C sort)
    if [[ ! -s "$fingerprint_input" ]]; then
        echo "[self-host-parser-build] empty parser source-set fingerprint" >&2
        return 1
    fi
    parser_tool_sha256_file "$fingerprint_input"
}

pgy_selfhost_compile_parser_tool() {
    local label="$1"
    local source="$2"
    local backend="$3"
    local output="$4"
    local compile_log="$5"
    local cache_dir="$ROOT_DIR/.tmp/self_hosted/parser"
    local shared_bin="$cache_dir/main_${backend}.exe"
    local stamp="$cache_dir/main_${backend}.build.key"
    local compiler_exec
    local source_rel
    local source_set
    local build_key

    mkdir -p "$cache_dir" "$(dirname "$output")" "$(dirname "$compile_log")"
    compiler_exec="$(pgy_path_for_bash_tool "$PGY")"
    source_rel="${source#"$ROOT_DIR"/}"
    if command -v sha256sum >/dev/null 2>&1; then
        source_set="$(parser_tool_source_set_fingerprint "$cache_dir")"
        build_key="$PARSER_TOOL_CACHE_SCHEMA|backend=$backend|source-set=$source_set|source=${source#"$ROOT_DIR"/}|source-hash=$(parser_tool_sha256_file "$source")|compiler-executable=$(parser_tool_sha256_file "$PGY")"
        if [[ -f "$shared_bin" && -f "$stamp" ]] \
            && grep -Fxq "$build_key" "$stamp"; then
            if [[ "$output" != "$shared_bin" ]]; then
                cp -f "$shared_bin" "$output"
            fi
            echo "[$label] reusing fingerprinted parser backend=$backend"
            return 0
        fi
    else
        build_key=""
    fi

    if ! (cd "$ROOT_DIR" && "$compiler_exec" \
        "$source_rel" \
        --backend="$backend" \
        -o "$(pgy_path_for_compiler "$PGY" "$shared_bin")" \
        >"$compile_log" 2>&1); then
        rm -f "$stamp"
        return 1
    fi
    if [[ -n "$build_key" ]]; then
        printf '%s\n' "$build_key" >"$stamp"
    fi
    if [[ "$output" != "$shared_bin" ]]; then
        cp -f "$shared_bin" "$output"
    fi
}
