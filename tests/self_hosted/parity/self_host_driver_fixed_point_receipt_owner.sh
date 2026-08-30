#!/usr/bin/env bash
# Owns content-addressed evidence for fixed-point driver reuse. The receipt is
# build evidence only; it never owns compiler semantics or source admission.

pgy_selfhost_driver_receipt_error() {
    echo "[self-host-driver-fixed-point-receipt] $*" >&2
    return 1
}

pgy_selfhost_driver_receipt_hash_file() {
    local path="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$path" | cut -d' ' -f1
        return
    fi
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$path" | cut -d' ' -f1
        return
    fi
    pgy_selfhost_driver_receipt_error "no SHA-256 tool is available"
}

pgy_selfhost_driver_source_graph_fingerprint() {
    local root_dir="$1"
    local graph_input="$2"
    local source_rel
    local source_hash
    local path_input="${graph_input}.paths"
    local hash_input="${graph_input}.hashes"
    local -a batch_hash_command

    (cd "$root_dir" && find src/self_hosted -type f -name '*.pgy' -print | LC_ALL=C sort) \
        >"$path_input" || return 1
    [[ -s "$path_input" ]] || {
        pgy_selfhost_driver_receipt_error "self-host source graph is empty"
        return 1
    }
    : >"$graph_input"
    if sort -z </dev/null >/dev/null 2>&1; then
        if command -v sha256sum >/dev/null 2>&1; then
            batch_hash_command=(sha256sum)
        elif command -v shasum >/dev/null 2>&1; then
            batch_hash_command=(shasum -a 256)
        else
            pgy_selfhost_driver_receipt_error "no SHA-256 tool is available"
            return 1
        fi
        if ! (cd "$root_dir" && find src/self_hosted -type f -name '*.pgy' -print0 \
            | LC_ALL=C sort -z | xargs -0 "${batch_hash_command[@]}") >"$graph_input"; then
            pgy_selfhost_driver_receipt_error "batch source hashing failed"
            return 1
        fi
    elif command -v git >/dev/null 2>&1 \
        && (cd "$root_dir" && git rev-parse --is-inside-work-tree >/dev/null 2>&1); then
        if ! (cd "$root_dir" && git hash-object --stdin-paths <"$path_input" >"$hash_input"); then
            pgy_selfhost_driver_receipt_error "Git batch source hashing failed"
            return 1
        fi
        exec 3<"$hash_input"
        while IFS= read -r source_rel; do
            if ! IFS= read -r source_hash <&3; then
                exec 3<&-
                pgy_selfhost_driver_receipt_error "source hash batch ended early"
                return 1
            fi
            printf '%s=%s\n' "$source_rel" "$source_hash" >>"$graph_input" || return 1
        done <"$path_input"
        if IFS= read -r source_hash <&3; then
            exec 3<&-
            pgy_selfhost_driver_receipt_error "source hash batch has extra rows"
            return 1
        fi
        exec 3<&-
    else
        while IFS= read -r source_rel; do
            source_hash="$(pgy_selfhost_driver_receipt_hash_file "$root_dir/$source_rel")" || return 1
            printf '%s=%s\n' "$source_rel" "$source_hash" >>"$graph_input" || return 1
        done <"$path_input"
    fi
    rm -f "$path_input" "$hash_input"
    pgy_selfhost_driver_receipt_hash_file "$graph_input"
}

pgy_selfhost_driver_runtime_header_fingerprint() {
    local root_dir="$1"
    local header_input="$2"
    local header_path
    local header_hash

    : >"$header_input"
    while IFS= read -r header_path; do
        header_hash="$(pgy_selfhost_driver_receipt_hash_file "$header_path")" || return 1
        printf '%s=%s\n' "${header_path#"$root_dir"/}" "$header_hash" \
            >>"$header_input" || return 1
    done < <(find "$root_dir/src/runtime" -type f -name '*.h' -print | LC_ALL=C sort)
    pgy_selfhost_driver_receipt_hash_file "$header_input"
}

pgy_selfhost_driver_render_fixed_point_receipt() {
    local root_dir="$1"
    local codegen_seed="$2"
    local gen2_c="$3"
    local gen3_c="$4"
    local gen2_binary="$5"
    local graph_input="$6"
    local input
    local gen2_c_hash
    local gen3_c_hash
    local source_graph_hash
    local codegen_seed_hash
    local gen2_binary_hash

    for input in "$codegen_seed" "$gen2_c" "$gen3_c" "$gen2_binary"; do
        [[ -f "$input" ]] || {
            pgy_selfhost_driver_receipt_error "missing fixed-point input: $input"
            return 1
        }
    done
    gen2_c_hash="$(pgy_selfhost_driver_receipt_hash_file "$gen2_c")" || return 1
    gen3_c_hash="$(pgy_selfhost_driver_receipt_hash_file "$gen3_c")" || return 1
    [[ "$gen2_c_hash" == "$gen3_c_hash" ]] || {
        pgy_selfhost_driver_receipt_error "gen2/gen3 C is not an exact fixed point"
        return 1
    }
    source_graph_hash="$(pgy_selfhost_driver_source_graph_fingerprint "$root_dir" "$graph_input")" || return 1
    codegen_seed_hash="$(pgy_selfhost_driver_receipt_hash_file "$codegen_seed")" || return 1
    gen2_binary_hash="$(pgy_selfhost_driver_receipt_hash_file "$gen2_binary")" || return 1
    printf '%s\n' \
        'schema=pgy.selfhost.driver-fixed-point-receipt.v1' \
        "source_graph=$source_graph_hash" \
        "codegen_seed=$codegen_seed_hash" \
        "gen2_c=$gen2_c_hash" \
        "gen3_c=$gen3_c_hash" \
        "gen2_binary=$gen2_binary_hash"
}

pgy_selfhost_driver_write_fixed_point_receipt() {
    local root_dir="$1"
    local codegen_seed="$2"
    local gen2_c="$3"
    local gen3_c="$4"
    local gen2_binary="$5"
    local receipt="$6"
    local receipt_tmp="${receipt}.tmp"
    local graph_input="${receipt}.source-graph.input"

    rm -f "$receipt_tmp"
    pgy_selfhost_driver_render_fixed_point_receipt \
        "$root_dir" "$codegen_seed" "$gen2_c" "$gen3_c" "$gen2_binary" \
        "$graph_input" >"$receipt_tmp" || return 1
    mv -f "$receipt_tmp" "$receipt"
}

pgy_selfhost_driver_validate_fixed_point_receipt() {
    local root_dir="$1"
    local codegen_seed="$2"
    local gen2_c="$3"
    local gen3_c="$4"
    local gen2_binary="$5"
    local receipt="$6"
    local expected="${receipt}.expected.$$"
    local graph_input="${receipt}.validate-source-graph.input.$$"
    local observed_hash
    local expected_hash

    [[ -f "$receipt" ]] || {
        pgy_selfhost_driver_receipt_error "missing fixed-point receipt: $receipt"
        return 1
    }
    if ! pgy_selfhost_driver_render_fixed_point_receipt \
        "$root_dir" "$codegen_seed" "$gen2_c" "$gen3_c" "$gen2_binary" \
        "$graph_input" >"$expected"; then
        rm -f "$expected" "$graph_input"
        return 1
    fi
    observed_hash="$(pgy_selfhost_driver_receipt_hash_file "$receipt")" || return 1
    expected_hash="$(pgy_selfhost_driver_receipt_hash_file "$expected")" || return 1
    rm -f "$expected" "$graph_input"
    [[ "$observed_hash" == "$expected_hash" ]] ||
        pgy_selfhost_driver_receipt_error "fixed-point receipt is stale or malformed"
}

pgy_selfhost_driver_installer_prebuild_key() {
    local root_dir="$1"
    local codegen_seed="$2"
    local machine_manifest="$3"
    local runtime_headers="$4"
    local output_key="$5"
    local cc_profile="$6"
    local cc_flags="$7"
    local cc_identity="$8"
    local installer_owner="$9"
    local key_input="${10}"
    local graph_input="${11}"
    local source_graph_hash codegen_seed_hash machine_manifest_hash
    local installer_owner_hash receipt_owner_hash

    source_graph_hash="$(pgy_selfhost_driver_source_graph_fingerprint "$root_dir" "$graph_input")" || return 1
    codegen_seed_hash="$(pgy_selfhost_driver_receipt_hash_file "$codegen_seed")" || return 1
    machine_manifest_hash="$(pgy_selfhost_driver_receipt_hash_file "$machine_manifest")" || return 1
    installer_owner_hash="$(pgy_selfhost_driver_receipt_hash_file "$installer_owner")" || return 1
    receipt_owner_hash="$(pgy_selfhost_driver_receipt_hash_file "${BASH_SOURCE[0]}")" || return 1
    printf '%s\n' \
        'schema=pgy.selfhost.driver-installer-prebuild.v1' \
        "source_graph=$source_graph_hash" \
        "codegen_seed=$codegen_seed_hash" \
        "machine_manifest=$machine_manifest_hash" \
        "runtime_headers=$runtime_headers" \
        "output=$output_key" \
        "cc_profile=$cc_profile" \
        "cc_flags=$cc_flags" \
        "cc=$cc_identity" \
        "installer_owner=$installer_owner_hash" \
        "receipt_owner=$receipt_owner_hash" \
        >"$key_input" || return 1
    pgy_selfhost_driver_receipt_hash_file "$key_input"
}

pgy_selfhost_driver_write_installed_artifact_receipt() {
    local artifact="$1"
    local receipt="$2"
    local artifact_hash
    artifact_hash="$(pgy_selfhost_driver_receipt_hash_file "$artifact")" || return 1
    printf '%s\n' \
        'schema=pgy.selfhost.installed-driver-artifact.v1' \
        "artifact=$artifact_hash" \
        >"${receipt}.tmp" || return 1
    mv -f "${receipt}.tmp" "$receipt"
}

pgy_selfhost_driver_validate_installed_artifact_receipt() {
    local artifact="$1"
    local receipt="$2"
    local expected="${receipt}.expected.$$"
    local artifact_hash

    [[ -f "$artifact" && -f "$receipt" ]] || return 1
    artifact_hash="$(pgy_selfhost_driver_receipt_hash_file "$artifact")" || return 1
    printf '%s\n' \
        'schema=pgy.selfhost.installed-driver-artifact.v1' \
        "artifact=$artifact_hash" \
        >"$expected" || return 1
    local observed_hash
    local expected_hash
    observed_hash="$(pgy_selfhost_driver_receipt_hash_file "$receipt")" || return 1
    expected_hash="$(pgy_selfhost_driver_receipt_hash_file "$expected")" || return 1
    rm -f "$expected"
    [[ "$observed_hash" == "$expected_hash" ]]
}
