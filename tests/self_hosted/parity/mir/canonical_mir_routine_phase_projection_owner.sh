#!/usr/bin/env bash
# Owns one installed-driver MIR canonicalization projection.

pgy_canonical_mir_routine_phase_project() {
    local root_dir="$1" pgy="$2" input="$3" output="$4"
    local input_arg
    input_arg="$(pgy_selfhost_path_relative_to_root "$input")"
    (cd "$root_dir" && "$pgy" --self-driver --canonicalize-mir-json "$input_arg") \
        | tr -d '\r' >"$output"
}
