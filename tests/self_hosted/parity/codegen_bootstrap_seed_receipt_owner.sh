#!/usr/bin/env bash
# Owns bounded input and exact-output evidence for reusing the codegen seed.

PGY_SELFHOST_CODEGEN_SEED_OWNER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$PGY_SELFHOST_CODEGEN_SEED_OWNER_DIR/emitted_c_runtime_header_owner.sh"
source "$PGY_SELFHOST_CODEGEN_SEED_OWNER_DIR/self_host_driver_fixed_point_receipt_owner.sh"

pgy_selfhost_codegen_seed_receipt_error() {
    echo "[self-host-codegen-seed-receipt] $*" >&2
    return 2
}

pgy_selfhost_codegen_seed_render_artifact_receipt() {
    local source_artifact="$1"
    local binary_artifact="$2"

    [[ -f "$source_artifact" ]] ||
        pgy_selfhost_codegen_seed_receipt_error "missing seed C artifact: $source_artifact" || return 2
    [[ -f "$binary_artifact" ]] ||
        pgy_selfhost_codegen_seed_receipt_error "missing seed binary: $binary_artifact" || return 2
    printf '%s\n' \
        'schema=pgy.selfhost.codegen-seed-artifact.v1' \
        "source_artifact=$(pgy_selfhost_driver_receipt_hash_file "$source_artifact")" \
        "binary_artifact=$(pgy_selfhost_driver_receipt_hash_file "$binary_artifact")"
}

pgy_selfhost_codegen_seed_write_artifact_receipt() {
    local source_artifact="$1"
    local binary_artifact="$2"
    local receipt="$3"
    local receipt_tmp="${receipt}.tmp"

    rm -f "$receipt_tmp"
    pgy_selfhost_codegen_seed_render_artifact_receipt \
        "$source_artifact" "$binary_artifact" >"$receipt_tmp" || return $?
    mv -f "$receipt_tmp" "$receipt"
}

pgy_selfhost_codegen_seed_validate_artifact_receipt() {
    local source_artifact="$1"
    local binary_artifact="$2"
    local receipt="$3"
    local expected="${receipt}.expected.$$"
    local observed_hash expected_hash

    [[ -f "$receipt" ]] || return 1
    if ! pgy_selfhost_codegen_seed_render_artifact_receipt \
        "$source_artifact" "$binary_artifact" >"$expected"; then
        rm -f "$expected"
        return 2
    fi
    observed_hash="$(pgy_selfhost_driver_receipt_hash_file "$receipt")" || return 2
    expected_hash="$(pgy_selfhost_driver_receipt_hash_file "$expected")" || return 2
    rm -f "$expected"
    [[ "$observed_hash" == "$expected_hash" ]] || return 1
}

pgy_selfhost_codegen_seed_prebuild_key() {
    local root_dir="$1"
    local native_pgy="$2"
    local cc_identity="$3"
    local runtime_headers="$4"
    local cc_profile="$5"
    local cc_flags="$6"
    local bootstrap_owner="$7"
    local key_input="$8"
    local graph_input="$9"
    local source_graph_hash owner_dir

    owner_dir="$(cd "$(dirname "$bootstrap_owner")" && pwd)"
    source_graph_hash="$(pgy_selfhost_driver_source_graph_fingerprint \
        "$root_dir" "$graph_input")" || return 2
    printf '%s\n' \
        'schema=pgy.selfhost.codegen-seed-prebuild.v1' \
        "source_graph=$source_graph_hash" \
        "native_pgy=$(pgy_selfhost_driver_receipt_hash_file "$native_pgy")" \
        "runtime_headers=$runtime_headers" \
        "cc_profile=$cc_profile" \
        "cc_flags=$cc_flags" \
        "cc_fingerprint=$cc_identity" \
        "bootstrap_owner=$(pgy_selfhost_driver_receipt_hash_file "$bootstrap_owner")" \
        "compile_owner=$(pgy_selfhost_driver_receipt_hash_file "$owner_dir/codegen_bootstrap_compile_leg.sh")" \
        "parser_owner=$(pgy_selfhost_driver_receipt_hash_file "$owner_dir/parser_tool_build_leg.sh")" \
        "path_owner=$(pgy_selfhost_driver_receipt_hash_file "$root_dir/tests/pgy_binary_path_helpers.sh")" \
        "profile_owner=$(pgy_selfhost_driver_receipt_hash_file "$owner_dir/emitted_c_runtime_header_owner.sh")" \
        "receipt_owner=$(pgy_selfhost_driver_receipt_hash_file "${BASH_SOURCE[0]}")" \
        >"$key_input" || return 2
    pgy_selfhost_driver_receipt_hash_file "$key_input"
}

pgy_selfhost_codegen_seed_current_key() {
    local root_dir="$1"
    local build_dir="$2"
    local native_pgy="$3"
    local cc="$4"
    local bootstrap_owner="$5"
    local runtime_headers cc_identity

    pgy_selfhost_select_emitted_c_compile_profile || return 2
    runtime_headers="$(pgy_selfhost_driver_runtime_header_fingerprint \
        "$root_dir" "$build_dir/codegen-seed.runtime-headers.input")" || return 2
    cc_identity="$(pgy_selfhost_driver_c_compiler_fingerprint \
        "$cc" "$build_dir/codegen-seed.cc-fingerprint.input")" || return 2
    pgy_selfhost_codegen_seed_prebuild_key \
        "$root_dir" "$native_pgy" "$cc_identity" "$runtime_headers" \
        "${PGY_SELFHOST_CC_PROFILE:-release}" \
        "${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[*]}" "$bootstrap_owner" \
        "$build_dir/codegen-seed.prebuild.key.input" \
        "$build_dir/codegen-seed.source-graph.input"
}

pgy_selfhost_codegen_seed_try_reuse() {
    local root_dir="$1" build_dir="$2" native_pgy="$3" cc="$4" bootstrap_owner="$5"
    local key stamp="$build_dir/codegen-seed.prebuild.key"
    local receipt="$build_dir/codegen-seed.output.receipt"

    key="$(pgy_selfhost_codegen_seed_current_key \
        "$root_dir" "$build_dir" "$native_pgy" "$cc" "$bootstrap_owner")" || return 2
    if [[ ! -f "$stamp" ]] || ! grep -Fxq "$key" "$stamp"; then
        return 1
    fi
    if ! pgy_selfhost_codegen_seed_validate_artifact_receipt \
        "$build_dir/gen2.c" "$build_dir/gen2.exe" "$receipt"; then
        echo "[self-host-codegen-seed-receipt] cached output changed; rebuilding" >&2
        return 1
    fi
    pgy_binary_is_runnable_here "$build_dir/gen2.exe" || {
        echo "[self-host-codegen-seed-receipt] cached binary is not runnable; rebuilding" >&2
        return 1
    }
}

pgy_selfhost_codegen_seed_record() {
    local root_dir="$1" build_dir="$2" native_pgy="$3" cc="$4" bootstrap_owner="$5"
    local key stamp="$build_dir/codegen-seed.prebuild.key"

    key="$(pgy_selfhost_codegen_seed_current_key \
        "$root_dir" "$build_dir" "$native_pgy" "$cc" "$bootstrap_owner")" || return 2
    pgy_selfhost_codegen_seed_write_artifact_receipt \
        "$build_dir/gen2.c" "$build_dir/gen2.exe" \
        "$build_dir/codegen-seed.output.receipt" || return 2
    printf '%s\n' "$key" >"${stamp}.tmp" || return 2
    mv -f "${stamp}.tmp" "$stamp"
}
