#!/usr/bin/env bash
# Owns the receipts that let the driver fixed point run as three CI jobs
# (docs/truth/decisions/2026-11-11_native_oracle_emission_runs_in_its_own_job.md).
# The oracle part's receipt names the exact inputs of the native oracle
# driver; the join part admits the oracle artifacts only when every name
# still matches its checkout, compiler pair and C compiler. A missing or
# different row fails closed; nothing is rebuilt.

if ! declare -F pgy_selfhost_driver_receipt_hash_file >/dev/null 2>&1; then
    source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/self_host_driver_fixed_point_receipt_owner.sh"
fi

pgy_selfhost_driver_oracle_part_render() {
    local root_dir="$1" native_pgy="$2" cc="$3" oracle_c="$4" oracle_exe="$5"
    local scratch="$6"
    local revision source_graph runtime_headers cc_fingerprint

    revision="$(git -C "$root_dir" rev-parse HEAD 2>/dev/null)" || {
        pgy_selfhost_driver_receipt_error "oracle part needs the checkout revision"
        return 1
    }
    source_graph="$(pgy_selfhost_driver_source_graph_fingerprint \
        "$root_dir" "$scratch.graph")" || return 1
    runtime_headers="$(pgy_selfhost_driver_runtime_header_fingerprint \
        "$root_dir" "$scratch.headers")" || return 1
    cc_fingerprint="$(pgy_selfhost_driver_c_compiler_fingerprint \
        "$cc" "$scratch.cc")" || return 1
    rm -f "$scratch.graph" "$scratch.headers" "$scratch.cc"
    printf '%s\n' \
        'schema=pgy.selfhost.driver-oracle-part.v1' \
        "source_revision=$revision" \
        "source_graph=$source_graph" \
        "runtime_headers=$runtime_headers" \
        "cc_fingerprint=$cc_fingerprint" \
        "native_pgy=$(pgy_selfhost_driver_receipt_hash_file "$native_pgy")" \
        "driver_oracle_c=$(pgy_selfhost_driver_receipt_hash_file "$oracle_c")" \
        "driver_oracle_exe=$(pgy_selfhost_driver_receipt_hash_file "$oracle_exe")"
}

pgy_selfhost_driver_oracle_part_write() {
    local receipt="${@: -1}"
    pgy_selfhost_driver_oracle_part_render "${@:1:$#-1}" "$receipt.input" \
        >"$receipt.tmp" || { rm -f "$receipt.tmp"; return 1; }
    mv -f "$receipt.tmp" "$receipt"
}

pgy_selfhost_driver_oracle_part_validate() {
    local receipt="${@: -1}"
    local expected="$receipt.expected"
    [[ -s "$receipt" ]] || {
        pgy_selfhost_driver_receipt_error "oracle part receipt is missing: $receipt"
        return 1
    }
    pgy_selfhost_driver_oracle_part_render "${@:1:$#-1}" "$receipt.input" \
        >"$expected" || return 1
    if ! cmp -s "$expected" "$receipt"; then
        pgy_selfhost_driver_receipt_error "oracle part receipt does not match this checkout"
        diff -u "$receipt" "$expected" >&2 || true
        return 1
    fi
    rm -f "$expected"
}

# A part's profile rows travel to the join as plain files next to the
# receipt; the join prepends them so the receipt names every phase once.
pgy_selfhost_driver_part_save_profile_rows() {
    local part="$1" receipt="$2"
    [[ -f "${receipt}.phases.tmp" && -f "${receipt}.stages.tmp" ]] || {
        pgy_selfhost_driver_receipt_error "$part part recorded no profile rows"
        return 1
    }
    cp -f "${receipt}.phases.tmp" "${receipt}.${part}.phases" &&
        cp -f "${receipt}.stages.tmp" "${receipt}.${part}.stages"
}

pgy_selfhost_driver_part_merge_profile_rows() {
    local receipt="$1" part kind
    shift
    for kind in phases stages; do
        : >"${receipt}.${kind}.merged"
        for part in "$@"; do
            [[ -f "${receipt}.${part}.${kind}" ]] || {
                pgy_selfhost_driver_receipt_error "$part part profile rows are missing"
                return 1
            }
            cat "${receipt}.${part}.${kind}" >>"${receipt}.${kind}.merged"
        done
        cat "${receipt}.${kind}.tmp" >>"${receipt}.${kind}.merged" &&
            mv -f "${receipt}.${kind}.merged" "${receipt}.${kind}.tmp" || return 1
    done
}
