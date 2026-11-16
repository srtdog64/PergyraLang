#!/usr/bin/env bash
# Owns the receipt of a DRV-2 source artifact: the C the codegen seed emits
# for the installed DRV-2 entry. The artifact may cross from the job that
# emitted it to a job on another platform of the same workflow run, which then
# compiles exactly that C (adopt) or requires its own seed to re-emit the same
# bytes (reemit). The receipt is build evidence only; it never owns compiler
# semantics or source admission.

PGY_SELFHOST_DRIVER_SOURCE_ARTIFACT_OWNER_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$PGY_SELFHOST_DRIVER_SOURCE_ARTIFACT_OWNER_DIR/self_host_driver_fixed_point_receipt_owner.sh"

pgy_selfhost_driver_source_artifact_error() {
    echo "[self-host-driver-source-artifact] $*" >&2
    return 1
}

# The receipt binds the self-host source graph. On MSYS2/Cygwin sha256sum marks
# every line with its binary read mode ('*'); the identity is the digest and
# the path, so the graph drops that marker before it is hashed.
pgy_selfhost_driver_portable_source_graph_fingerprint() {
    local root_dir="$1"
    local graph_input="$2"

    pgy_selfhost_driver_source_graph_fingerprint "$root_dir" "$graph_input" >/dev/null || return 1
    sed -E 's/^([0-9a-f]{64}) [ *]/\1  /' "$graph_input" >"${graph_input}.portable" ||
        pgy_selfhost_driver_source_artifact_error "portable source graph rendering failed" || return 1
    pgy_selfhost_driver_receipt_hash_file "${graph_input}.portable"
}

pgy_selfhost_driver_render_source_artifact_receipt() {
    local root_dir="$1"
    local driver_source="$2"
    local artifact_c="$3"
    local graph_input="$4"
    local source_graph_hash
    local artifact_hash

    source_graph_hash="$(pgy_selfhost_driver_portable_source_graph_fingerprint "$root_dir" "$graph_input")" || return 1
    artifact_hash="$(pgy_selfhost_driver_receipt_hash_file "$artifact_c")" || return 1
    printf '%s\n' \
        'schema=pgy.selfhost.driver-source-artifact.v1' \
        "driver_source=$driver_source" \
        "source_graph=$source_graph_hash" \
        "source_artifact_c=$artifact_hash"
}

pgy_selfhost_driver_write_source_artifact_receipt() {
    local root_dir="$1"
    local driver_source="$2"
    local artifact_c="$3"
    local receipt="$4"

    rm -f "${receipt}.tmp"
    pgy_selfhost_driver_render_source_artifact_receipt "$root_dir" "$driver_source" \
        "$artifact_c" "${receipt}.source-graph.input" >"${receipt}.tmp" || return 1
    mv -f "${receipt}.tmp" "$receipt"
}

pgy_selfhost_driver_validate_source_artifact_receipt() {
    local root_dir="$1"
    local driver_source="$2"
    local artifact_c="$3"
    local receipt="$4"
    local expected="${receipt}.expected.$$"
    local status=0

    if ! pgy_selfhost_driver_render_source_artifact_receipt "$root_dir" "$driver_source" \
        "$artifact_c" "${expected}.source-graph.input" >"$expected"; then
        status=1
    elif [[ "$(pgy_selfhost_driver_receipt_hash_file "$receipt")" != \
            "$(pgy_selfhost_driver_receipt_hash_file "$expected")" ]]; then
        echo "[self-host-driver-source-artifact] admitted receipt:" >&2
        cat "$receipt" >&2
        echo "[self-host-driver-source-artifact] this checkout and artifact:" >&2
        cat "$expected" >&2
        pgy_selfhost_driver_source_artifact_error \
            "the receipt does not match this checkout and artifact" || status=1
    fi
    rm -f "$expected" "${expected}.source-graph.input" "${expected}.source-graph.input.portable"
    return "$status"
}

# usage: pgy_selfhost_driver_source_artifact_admit ROOT ENTRY MODE C RECEIPT
# An empty mode admits no artifact. adopt and reemit admit exactly one
# receipt-bound artifact, never beside a fixed-point driver.
pgy_selfhost_driver_source_artifact_admit() {
    local root_dir="$1"
    local driver_source="$2"
    local mode="$3"
    local artifact_c="$4"
    local receipt="$5"

    case "$mode" in
        "")
            [[ -z "${artifact_c}${receipt}" ]] ||
                pgy_selfhost_driver_source_artifact_error \
                    "a DRV-2 source artifact needs PGY_SELFHOST_DRIVER_SOURCE_ARTIFACT_MODE=adopt|reemit"
            ;;
        adopt|reemit)
            [[ -f "$artifact_c" && -f "$receipt" ]] ||
                pgy_selfhost_driver_source_artifact_error \
                    "mode $mode needs the DRV-2 C artifact and its receipt" || return 1
            [[ -z "${PGY_SELFHOST_FIXED_POINT_DRIVER_C:-}${PGY_SELFHOST_FIXED_POINT_DRIVER_RECEIPT:-}" ]] ||
                pgy_selfhost_driver_source_artifact_error \
                    "a DRV-2 source artifact and a fixed-point driver are two different inputs" || return 1
            pgy_selfhost_driver_validate_source_artifact_receipt \
                "$root_dir" "$driver_source" "$artifact_c" "$receipt"
            ;;
        *)
            pgy_selfhost_driver_source_artifact_error \
                "invalid PGY_SELFHOST_DRIVER_SOURCE_ARTIFACT_MODE=$mode; expected adopt or reemit"
            ;;
    esac
}

# usage: pgy_selfhost_driver_source_artifact_require_reemission EMITTED ADMITTED KEEP
# The emitted C must be the admitted C byte for byte. A different emission is
# kept at KEEP for inspection.
pgy_selfhost_driver_source_artifact_require_reemission() {
    local emitted_c="$1"
    local admitted_c="$2"
    local keep="$3"
    local emitted_hash
    local admitted_hash

    emitted_hash="$(pgy_selfhost_driver_receipt_hash_file "$emitted_c")" || return 1
    admitted_hash="$(pgy_selfhost_driver_receipt_hash_file "$admitted_c")" || return 1
    if [[ "$emitted_hash" != "$admitted_hash" ]]; then
        mv -f "$emitted_c" "$keep"
        pgy_selfhost_driver_source_artifact_error \
            "this host's codegen seed emits another DRV-2 C ($emitted_hash, kept at $keep) than the admitted artifact ($admitted_hash)"
        return 1
    fi
    echo "[self-host-driver-source-artifact] this host's codegen seed re-emits the admitted DRV-2 C byte for byte: $admitted_hash"
}
