#!/usr/bin/env bash
# Owns the one build per backend of the combined semantic owner probes
# (docs/truth/decisions/2026-11-11_semantic_owner_probes_merge_into_one_build.md).
#
# `pgy_semantic_owner_probe_build ROOT PGY DIR SECONDS` compiles each program
# below for C and LLVM into DIR, all at once, and writes receipts naming the
# compiler, every importable source and every binary. A gate calls
# `pgy_semantic_owner_probe_install` where it used to build its own probe:
# that places a launcher for one probe mode. The receipts in
# PGY_SEMANTIC_OWNER_PROBE_DIR must still match, or the gate fails; a gate
# never builds the probe. Run a gate on its own through its make target, or
# run `make self-host-semantic-owner-probe-build` and export
# PGY_SEMANTIC_OWNER_PROBE_DIR first.
#
# Programs: `probe` holds every merged probe that imports the semantic owners;
# `identity` is the identity cell receiver probe, which imports the direct-MIR
# admission owner (tests/concept_semantics/identity_cell_receiver_execution.py
# takes it through `pgy_semantic_owner_probe_binary`).

PGY_SEMANTIC_OWNER_PROBE_PROGRAMS=(
    "probe|tests/self_hosted/fixtures/semantic_owner_probe.pgy"
    "identity|tests/self_hosted/parity/fixture/identity_cell_receiver_probe.pgy"
)

pgy_semantic_owner_probe_error() {
    echo "[semantic-owner-probe] $*" >&2
    return 1
}

pgy_semantic_owner_probe_inputs() {
    local row
    (cd "$1" && {
        find src/self_hosted tests/self_hosted/fixtures -name '*.pgy' -type f -print0
        for row in "${PGY_SEMANTIC_OWNER_PROBE_PROGRAMS[@]}"; do printf '%s\0' "${row#*|}"; done
    } | LC_ALL=C sort -zu | xargs -0 sha256sum)
}

pgy_semantic_owner_probe_binaries() {
    local row backend
    for row in "${PGY_SEMANTIC_OWNER_PROBE_PROGRAMS[@]}"; do
        for backend in c llvm; do printf '%s-%s.exe\n' "$backend" "${row%%|*}"; done
    done
}

pgy_semantic_owner_probe_build() {
    local root="$1" pgy="$2" dir="$3" seconds="$4"
    local row backend binary failed=0 started index
    local -a pids=() logs=()
    [[ "$seconds" =~ ^[0-9]+$ ]] ||
        pgy_semantic_owner_probe_error "build limit must be whole seconds: $seconds" || return 1
    mkdir -p "$dir" || return 1
    dir="$(cd "$dir" && pwd -P)"
    rm -f "$dir"/*.exe "$dir"/*.sha256 "$dir"/*.compile.log "$dir"/build.receipt
    started="$(date +%s)"
    for row in "${PGY_SEMANTIC_OWNER_PROBE_PROGRAMS[@]}"; do
        for backend in c llvm; do
            binary="$backend-${row%%|*}.exe"
            (cd "$root" && timeout --signal=TERM --kill-after=5s "$seconds" \
                "$pgy" --native-pipeline "${row#*|}" \
                "--backend=$backend" --opt=dev -o "$dir/$binary") \
                >"$dir/$binary.compile.log" 2>&1 &
            pids+=("$!"); logs+=("$dir/$binary.compile.log")
        done
    done
    for index in "${!pids[@]}"; do
        if ! wait "${pids[$index]}"; then
            failed=1
            tail -20 "${logs[$index]}" >&2
        fi
    done
    [[ "$failed" == 0 ]] ||
        pgy_semantic_owner_probe_error "build failed or exceeded ${seconds}s" || return 1
    sha256sum "$pgy" >"$dir/native.sha256" || return 1
    pgy_semantic_owner_probe_inputs "$root" >"$dir/inputs.sha256" || return 1
    (cd "$dir" && pgy_semantic_owner_probe_binaries | xargs sha256sum) >"$dir/probes.sha256" || return 1
    printf 'limit_seconds=%s\nwall_seconds=%s\n' "$seconds" "$(( $(date +%s) - started ))" \
        >"$dir/build.receipt"
    echo "[semantic-owner-probe] built ${#pids[@]} binaries in $(( $(date +%s) - started ))s (limit ${seconds}s): $dir"
}

pgy_semantic_owner_probe_validate() {
    local root="$1" pgy="$2" dir="$3" name
    for name in native.sha256 inputs.sha256 probes.sha256 $(pgy_semantic_owner_probe_binaries); do
        [[ -s "$dir/$name" ]] ||
            pgy_semantic_owner_probe_error "shared build lacks $name: $dir" || return 1
    done
    [[ "$(sha256sum "$pgy" | cut -d' ' -f1)" == "$(cut -d' ' -f1 "$dir/native.sha256")" ]] ||
        pgy_semantic_owner_probe_error "shared build names another compiler than $pgy" || return 1
    cmp -s <(pgy_semantic_owner_probe_inputs "$root") "$dir/inputs.sha256" ||
        pgy_semantic_owner_probe_error "shared build predates the current probe sources" || return 1
    (cd "$dir" && sha256sum --quiet -c probes.sha256) ||
        pgy_semantic_owner_probe_error "shared build binaries changed after their receipt" || return 1
}

# usage: pgy_semantic_owner_probe_binary ROOT PGY BACKEND PROGRAM
# Prints the shared build's binary for PROGRAM once its receipts match.
pgy_semantic_owner_probe_binary() {
    local root="$1" pgy="$2" backend="$3" program="$4"
    local dir="${PGY_SEMANTIC_OWNER_PROBE_DIR:-}"
    [[ -n "$dir" ]] ||
        pgy_semantic_owner_probe_error "PGY_SEMANTIC_OWNER_PROBE_DIR is unset; run the gate through make" || return 1
    dir="$(cd "$dir" && pwd -P)" ||
        pgy_semantic_owner_probe_error "shared build directory is missing: $dir" || return 1
    pgy_semantic_owner_probe_validate "$root" "$pgy" "$dir" || return 1
    [[ -x "$dir/$backend-$program.exe" ]] ||
        pgy_semantic_owner_probe_error "shared build lacks $backend-$program.exe" || return 1
    printf '%s\n' "$dir/$backend-$program.exe"
}

# usage: pgy_semantic_owner_probe_install ROOT PGY BACKEND MODE LAUNCHER
pgy_semantic_owner_probe_install() {
    local root="$1" pgy="$2" backend="$3" mode="$4" launcher="$5" program=probe
    local dir="${PGY_SEMANTIC_OWNER_PROBE_DIR:-}"
    [[ -n "$dir" ]] ||
        pgy_semantic_owner_probe_error "PGY_SEMANTIC_OWNER_PROBE_DIR is unset; run the gate through make" || return 1
    dir="$(cd "$dir" && pwd -P)" ||
        pgy_semantic_owner_probe_error "shared build directory is missing: $dir" || return 1
    pgy_semantic_owner_probe_validate "$root" "$pgy" "$dir" || return 1
    printf '#!/usr/bin/env bash\nexec %q %q "$@"\n' "$dir/$backend-$program.exe" "$mode" >"$launcher" &&
        chmod +x "$launcher"
}
